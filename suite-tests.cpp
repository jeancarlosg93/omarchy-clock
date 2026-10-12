#include "timekeeper.h"
#include <QCoreApplication>
#include <QDateTime>
#include <QDebug>
#include <QFile>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QTimeZone>
#include <cstdio>
#define CHECK(x)                                                               \
  do {                                                                         \
    if (!(x)) {                                                                \
      qCritical() << "Failed:" << #x << "line" << __LINE__;                    \
      return 1;                                                                \
    }                                                                          \
  } while (0)
int main(int argc, char **argv) {
  QCoreApplication app(argc, argv);
  qInstallMessageHandler(
      [](QtMsgType, const QMessageLogContext &, const QString &m) {
        fprintf(stderr, "%s\n", qPrintable(m));
      });
  app.setOrganizationName("OmarchyClockSuiteTests");
  app.setApplicationName("SuiteTests");
  qint64 boot = 100000,
         wall = QDateTime::fromString("2026-10-10T14:00:00Z", Qt::ISODate)
                    .toMSecsSinceEpoch();
  auto b = [&] { return boot; };
  auto w = [&] { return wall; };
  auto advance = [&](qint64 ms) {
    boot += ms;
    wall += ms;
  };
  QTemporaryDir dir;
  Timekeeper e(":memory:", true, nullptr, b, w);
  CHECK(e.zoneCities().size() > 350);
  CHECK(e.nearestCity(18.466667, -69.9)["id"].toString() ==
        "America/Santo_Domingo");
  CHECK(e.nearestCity(35.654444, 139.744722)["id"].toString() == "Asia/Tokyo");
  CHECK(e.nearestCity(-18.133333, 178.416667)["id"].toString() ==
        "Pacific/Fiji");
  CHECK(e.nearestCity(91, 0).isEmpty());
  CHECK(e.zoneLocation("UTC").isEmpty());
  CHECK(e.zoneLocation("America/Santo_Domingo")["title"].toString() ==
        "Santo Domingo");
  int alerts = 0;
  bool silent = true;
  QObject::connect(&e, &Timekeeper::alert,
                   [&](QString, QString, QString, QString, bool sound) {
                     silent &= !sound;
                     ++alerts;
                   });
  CHECK(!e.addTimer("Bad", 0).size());
  e.clearError();
  QString t = e.addTimer("Tea", 5);
  CHECK(!t.isEmpty());
  e.toggleTimer(t);
  advance(1100);
  CHECK(e.timer(t)["remaining"].toLongLong() == 3900);
  CHECK(e.timer(t)["display"].toString() == "00:04");
  e.toggleTimer(t);
  advance(90000);
  CHECK(e.timer(t)["remaining"].toLongLong() == 3900);
  e.toggleTimer(t);
  advance(3901);
  e.tick();
  e.tick();
  CHECK(alerts == 1);
  CHECK(e.timer(t)["finished"].toBool());
  e.addTime(t, 60);
  CHECK(e.timer(t)["remaining"].toLongLong() == 60000);
  CHECK(!e.timer(t)["finished"].toBool());
  e.toggleTimer(t);
  CHECK(!e.removeTimer(t));
  e.clearError();
  CHECK(!e.editTimer(t, "Changed", 12, false, false));
  e.clearError();
  e.resetTimer(t);
  CHECK(e.removeTimer(t));
  QString repeat = e.addTimer("Repeat", 5, true);
  e.toggleTimer(repeat);
  advance(26000);
  e.tick();
  CHECK(e.timer(repeat)["cycles"].toInt() == 5);
  CHECK(e.timer(repeat)["remaining"].toLongLong() == 4000);
  CHECK(alerts == 2);
  e.resetTimer(repeat);
  e.renameStopwatch("Run, \"quoted\"");
  e.toggleStopwatch();
  advance(1234);
  e.lap();
  advance(2000);
  e.lap();
  CHECK(e.stopwatch()["elapsed"].toLongLong() == 3234);
  auto laps = e.stopwatch()["laps"].toList();
  CHECK(laps.size() == 2);
  CHECK(laps[0].toMap()["split"].toLongLong() == 2000);
  CHECK(laps[1].toMap()["best"].toBool());
  e.setSetting("precision", 3);
  CHECK(e.fraction(1234) == "234");
  e.toggleStopwatch();
  advance(1000);
  CHECK(e.stopwatch()["elapsed"].toLongLong() == 3234);
  CHECK(e.saveStopwatch("Run, \"quoted\""));
  CHECK(e.stopwatch()["elapsed"].toLongLong() == 0);
  QVariantList phases{QVariantMap{{"name", "Work"}, {"seconds", 2}},
                      QVariantMap{{"name", "Rest"}, {"seconds", 3}}};
  QString in = e.addInterval("Cycles", phases, 2);
  CHECK(!in.isEmpty());
  e.toggleInterval(in);
  advance(2000);
  e.tick();
  CHECK(e.interval(in)["phaseName"].toString() == "Rest");
  CHECK(e.interval(in)["remaining"].toLongLong() == 3000);
  e.toggleInterval(in);
  advance(10000);
  CHECK(e.interval(in)["remaining"].toLongLong() == 3000);
  e.toggleInterval(in);
  e.skipInterval(in);
  CHECK(e.interval(in)["round"].toInt() == 2);
  advance(10000);
  e.tick();
  CHECK(e.interval(in)["finished"].toBool());
  CHECK(!e.interval(in)["running"].toBool());
  QString bad =
      e.addInterval("Bad", {QVariantMap{{"name", "x"}, {"seconds", 0}}}, 1);
  CHECK(bad.isEmpty());
  e.clearError();
  QTimeZone utc("UTC");
  QDateTime now = QDateTime::fromMSecsSinceEpoch(wall, utc);
  QVariantMap schedule{{"zone", "UTC"}, {"time", "15:00"}, {"days", 127}};
  CHECK(Timekeeper::nextAlarmEpoch(schedule, wall) > wall);
  CHECK(e.addAlarm("Bad", "25:00", "", 0, "UTC").isEmpty());
  e.clearError();
  QDateTime next = now.addSecs(120);
  QString alarm = e.addAlarm("Wake", next.toString("HH:mm"),
                             next.date().toString(Qt::ISODate), 0, "UTC");
  CHECK(!alarm.isEmpty());
  qint64 deadline =
      Timekeeper::nextAlarmEpoch({{"zone", "UTC"},
                                  {"time", next.toString("HH:mm")},
                                  {"date", next.date().toString(Qt::ISODate)},
                                  {"days", 0}},
                                 wall);
  advance(deadline - wall);
  int before = alerts;
  e.tick();
  e.tick();
  CHECK(alerts == before + 1);
  CHECK(e.alarms().last().toMap()["ringing"].toBool());
  e.snoozeAlarm(alarm, 1);
  advance(60000);
  e.tick();
  CHECK(alerts == before + 2);
  e.dismissAlarm(alarm);
  CHECK(!e.alarms().last().toMap()["ringing"].toBool());
  QVariantMap dst{
      {"zone", "America/Toronto"}, {"time", "02:30"}, {"days", 127}};
  qint64 spring =
      QDateTime(QDate(2026, 3, 8), QTime(0, 0), QTimeZone("America/Toronto"))
          .toMSecsSinceEpoch();
  QDateTime shifted = QDateTime::fromMSecsSinceEpoch(
      Timekeeper::nextAlarmEpoch(dst, spring), QTimeZone("America/Toronto"));
  CHECK(shifted.date() == QDate(2026, 3, 8));
  CHECK(shifted.time() == QTime(3, 30));
  CHECK(e.startTracking("Design", "Project A"));
  advance(60000);
  e.toggleTracking();
  CHECK(e.tracking()["elapsed"].toLongLong() == 60000);
  advance(9000);
  e.toggleTracking();
  advance(30000);
  e.stopTracking();
  CHECK(e.reports()["todayMs"].toLongLong() == 90000);
  CHECK(e.tracking()["title"].toString().isEmpty());
  CHECK(e.addZone("Asia/Tokyo"));
  CHECK(!e.addZone("Invalid/Zone"));
  e.clearError();
  CHECK(e.zoneInfo("Asia/Tokyo")["offset"].toString() == "UTC+09:00");
  CHECK(e.setComparison("2026-10-10", "12:00"));
  CHECK(e.comparisonEpoch() > 0);
  // World-page comparison never contaminates ordinary live time details.
  CHECK(e.setComparison("2028-12-15", "12:00"));
  auto live = e.clockDetails("world", "");
  QDateTime wallLocal = QDateTime::fromMSecsSinceEpoch(wall);
  CHECK(live["display"].toString() == wallLocal.toString("HH:mm:ss"));
  CHECK(live["date"].toString() == wallLocal.toString("ddd, MMM d"));
  QString compared = e.zoneInfo("Asia/Tokyo")["display"].toString();
  advance(2000);
  CHECK(e.clockDetails("world", "")["display"].toString() !=
        live["display"].toString());
  CHECK(e.clockDetails("world", "Asia/Tokyo")["display"].toString() ==
        QDateTime::fromMSecsSinceEpoch(wall, QTimeZone("Asia/Tokyo"))
            .toString("HH:mm:ss"));
  CHECK(e.zoneInfo("Asia/Tokyo")["display"].toString() == compared);

  e.resetComparison();
  CHECK(e.comparisonEpoch() == 0);
  QString csv = dir.path() + "/history.csv";
  CHECK(e.exportCsv(QUrl::fromLocalFile(csv)));
  QFile f(csv);
  CHECK(f.open(QIODevice::ReadOnly));
  QByteArray output = f.readAll();
  CHECK(output.contains("Run, \"\"quoted\"\""));
  CHECK(output.contains("\"lap\""));
  CHECK(output.contains("\r\n"));
  auto last = e.sessions().first().toMap();
  CHECK(e.deleteSession(last["id"].toString()));
  QString persistent = dir.path() + "/clocks.sqlite3", timerId;
  {
    Timekeeper saved(persistent, true, nullptr, b, w);
    timerId = saved.addTimer("Persistent", 5);
    saved.toggleTimer(timerId);
    saved.toggleStopwatch();
    advance(1000);
    CHECK(saved.save());
  }
  {
    advance(6000);
    Timekeeper restored(persistent, true, nullptr, b, w);
    int n = 0;
    QObject::connect(&restored, &Timekeeper::alert,
                     [&](QString, QString, QString, QString, bool) { ++n; });
    restored.tick();
    CHECK(n == 1);
    CHECK(restored.timer(timerId)["finished"].toBool());
    CHECK(restored.stopwatch()["elapsed"].toLongLong() == 7000);
    restored.pauseAll();
  }
  {
    Timekeeper restored(persistent, true, nullptr, b, w);
    int n = 0;
    QObject::connect(&restored, &Timekeeper::alert,
                     [&](QString, QString, QString, QString, bool) { ++n; });
    restored.tick();
    CHECK(n == 0);
  }
  CHECK(silent);
  // Fall-back overlap fires on the first 01:30, then schedules the following
  // day.
  QTimeZone toronto("America/Toronto");
  QVariantMap fall{
      {"zone", "America/Toronto"}, {"time", "01:30"}, {"days", 127}};
  auto fallStart =
      QDateTime(QDate(2026, 11, 1), QTime(0, 0), toronto).toMSecsSinceEpoch();
  qint64 firstFall = Timekeeper::nextAlarmEpoch(fall, fallStart);
  CHECK(QDateTime::fromMSecsSinceEpoch(firstFall, toronto).offsetFromUtc() ==
        -14400);
  CHECK(QDateTime::fromMSecsSinceEpoch(
            Timekeeper::nextAlarmEpoch(fall, firstFall), toronto)
            .date() == QDate(2026, 11, 2));
  // A wall-clock adjustment cannot change an in-process monotonic countdown.
  QString monotonic = e.addTimer("Monotonic", 60);
  e.toggleTimer(monotonic);
  wall += 3600000;
  CHECK(e.timer(monotonic)["remaining"].toLongLong() == 60000);
  boot += 1000;
  CHECK(e.timer(monotonic)["remaining"].toLongLong() == 59000);
  e.resetTimer(monotonic);
  // Atomic session/state persistence: injected write failure neither loses nor
  // duplicates work.
  QString atomicFile = dir.path() + "/atomic.sqlite3";
  Timekeeper atomic(atomicFile, true, nullptr, b, w);
  QSqlDatabase sql;
  for (const QString &name : QSqlDatabase::connectionNames())
    if (QSqlDatabase::database(name).databaseName() == atomicFile)
      sql = QSqlDatabase::database(name);
  CHECK(sql.isOpen());
  QSqlQuery query(sql);
  CHECK(query.exec("CREATE TRIGGER fail_history BEFORE INSERT ON sessions "
                   "BEGIN SELECT RAISE(ABORT,'injected failure'); END"));
  QString at = atomic.addTimer("Atomic", 1);
  atomic.toggleTimer(at);
  advance(1001);
  int deliveries = 0;
  QObject::connect(
      &atomic, &Timekeeper::alert,
      [&](QString, QString, QString, QString, bool) { ++deliveries; });
  atomic.tick();
  CHECK(atomic.timer(at)["running"].toBool());
  CHECK(atomic.sessions().isEmpty());
  CHECK(deliveries == 0);
  atomic.toggleStopwatch();
  advance(1234);
  CHECK(!atomic.saveStopwatch("Preserved"));
  CHECK(atomic.stopwatch()["elapsed"].toLongLong() == 1234);
  CHECK(atomic.stopwatch()["running"].toBool());
  CHECK(atomic.startTracking("Preserved work", "General"));
  advance(2000);
  atomic.stopTracking();
  CHECK(atomic.tracking()["running"].toBool());
  CHECK(atomic.tracking()["elapsed"].toLongLong() == 2000);
  CHECK(query.exec("DROP TRIGGER fail_history"));
  atomic.tick();
  CHECK(!atomic.timer(at)["running"].toBool());
  CHECK(deliveries == 1);
  atomic.tick();
  CHECK(deliveries == 1);
  CHECK(atomic.saveStopwatch("Recovered"));
  atomic.stopTracking();
  CHECK(atomic.countHistory("", "") == 3);
  CHECK(query.exec("CREATE TRIGGER fail_state BEFORE UPDATE ON app_state BEGIN "
                   "SELECT RAISE(ABORT,'state failure'); END"));
  atomic.toggleStopwatch();
  advance(500);
  int records = atomic.countHistory("", "");
  CHECK(!atomic.saveStopwatch("Must rollback"));
  CHECK(atomic.countHistory("", "") == records);
  CHECK(atomic.stopwatch()["elapsed"].toLongLong() == 500);
  CHECK(query.exec("DROP TRIGGER fail_state"));
  CHECK(atomic.saveStopwatch("After rollback"));
  // SQL paging and export include records beyond the in-memory dashboard
  // window.
  for (int i = 0; i < 505; ++i) {
    atomic.toggleStopwatch();
    advance(1);
    CHECK(atomic.saveStopwatch("Paged " + QString::number(i)));
  }
  CHECK(atomic.sessions().size() == 500);
  CHECK(atomic.countHistory("Paged", "stopwatch") == 505);
  CHECK(atomic.searchHistory("Paged", "stopwatch", 500).size() == 5);
  CHECK(atomic.exportCsv(QUrl::fromLocalFile(dir.path() + "/all.csv")));
  QFile all(dir.path() + "/all.csv");
  CHECK(all.open(QIODevice::ReadOnly));
  CHECK(all.readAll().contains("Paged 0"));
  // Malformed stored data is retained and never overwritten by the destructor.
  QString invalidFile = dir.path() + "/invalid.sqlite3";
  {
    Timekeeper valid(invalidFile, true, nullptr, b, w);
    CHECK(valid.save());
  }
  auto invalid = QSqlDatabase::addDatabase("QSQLITE", "invalid-test");
  invalid.setDatabaseName(invalidFile);
  CHECK(invalid.open());
  QSqlQuery corrupt(invalid);
  CHECK(corrupt.exec("UPDATE app_state SET value='broken'"));
  {
    Timekeeper invalidKeeper(invalidFile, true, nullptr, b, w);
    CHECK(!invalidKeeper.error().isEmpty());
    CHECK(!invalidKeeper.save());
  }
  CHECK(corrupt.exec("SELECT value FROM app_state"));
  CHECK(corrupt.next());
  CHECK(corrupt.value(0).toString() == "broken");
  Timekeeper broken(dir.path(), true, nullptr, b, w);
  CHECK(!broken.error().isEmpty());
  CHECK(!broken.save());
  qInfo() << "Suite: concurrent timers, repeats, laps, CSV, interval catch-up, "
             "alarm snooze/DST, tracking summaries, world clocks, restart "
             "recovery, and storage failure passed.";
  return 0;
}
