#include "timekeeper.h"
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QSettings>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QTimeZone>
#include <QUuid>
#include <algorithm>
#include <cmath>
#include <time.h>

static qint64 bootNow() {
  timespec ts{};
  clock_gettime(CLOCK_BOOTTIME, &ts);
  return qint64(ts.tv_sec) * 1000 + ts.tv_nsec / 1000000;
}
static qint64 wallNow() { return QDateTime::currentMSecsSinceEpoch(); }
static qint64 num(const QVariantMap &m, const char *key) {
  return m.value(key).toLongLong();
}
static bool running(const QVariantMap &m) {
  return m.value("running").toBool();
}
static QString titleOr(QString value, QString fallback) {
  return value.trimmed().isEmpty() ? fallback : value.trimmed().left(120);
}
static QByteArray json(const QVariant &value) {
  return QJsonDocument::fromVariant(value).toJson(QJsonDocument::Compact);
}
QString Timekeeper::makeId() {
  return QUuid::createUuid().toString(QUuid::WithoutBraces);
}
int Timekeeper::indexOf(const QVariantList &list, QString id) {
  for (int i = 0; i < list.size(); ++i)
    if (list[i].toMap().value("id").toString() == id)
      return i;
  return -1;
}
Timekeeper::Timekeeper(QString path, bool ephemeral, QObject *parent,
                       std::function<qint64()> boot,
                       std::function<qint64()> wall)
    : QObject(parent), m_ephemeral(ephemeral),
      m_boot(boot ? std::move(boot) : bootNow),
      m_wall(wall ? std::move(wall) : wallNow) {
  defaults();
  m_path = path.isEmpty() ? QStandardPaths::writableLocation(
                                QStandardPaths::GenericDataLocation) +
                                "/omarchy-clock/clock.sqlite3"
                          : path;
  if (ephemeral && path.isEmpty())
    m_path = ":memory:";
  if (m_path != ":memory:")
    QDir().mkpath(QFileInfo(m_path).absolutePath());
  m_connection = "clock-" + makeId();
  m_db = QSqlDatabase::addDatabase("QSQLITE", m_connection);
  m_db.setDatabaseName(m_path);
  m_db.setConnectOptions("QSQLITE_BUSY_TIMEOUT=3000");
  if (m_db.open()) {
    QSqlQuery q(m_db);
    m_ready =
        q.exec("CREATE TABLE IF NOT EXISTS app_state (key TEXT PRIMARY KEY, "
               "value TEXT NOT NULL)") &&
        q.exec("CREATE TABLE IF NOT EXISTS sessions (id TEXT PRIMARY KEY, kind "
               "TEXT NOT NULL, title TEXT NOT NULL, project TEXT NOT NULL, "
               "started INTEGER NOT NULL, ended INTEGER NOT NULL, duration "
               "INTEGER NOT NULL, laps TEXT NOT NULL)") &&
        q.exec("CREATE INDEX IF NOT EXISTS session_dates ON "
               "sessions(kind,started,ended)");
    if (m_ready) {
      q.exec("PRAGMA journal_mode=WAL");
      q.exec("PRAGMA synchronous=FULL");
      load();
      loadHistory();
      refreshReports();
    } else
      fail("Could not initialize local storage. Changes cannot be saved.");
  } else
    fail("Could not open local storage. Check the data folder permissions; "
         "changes cannot be saved.");
  m_tick.setInterval(33);
  m_tick.setTimerType(Qt::PreciseTimer);
  connect(&m_tick, &QTimer::timeout, this, &Timekeeper::tick);
  m_tick.start();
  m_checkpoint.setInterval(5000);
  connect(&m_checkpoint, &QTimer::timeout, this, [this] {
    if (hasActive()) {
      save();
      refreshReports();
    }
  });
  m_checkpoint.start();
  QTimer::singleShot(0, this, &Timekeeper::tick);
}
Timekeeper::~Timekeeper() {
  save();
  m_db.close();
  m_db = QSqlDatabase();
  QSqlDatabase::removeDatabase(m_connection);
}
void Timekeeper::defaults() {
  m_settings = {{"sound", false},       {"notifications", true},
                {"precision", 2},       {"hour24", true},
                {"snoozeMinutes", 5},   {"keepInTray", true},
                {"startPage", "Today"}, {"barVisible", true}};
  m_stopwatch = {{"title", "Stopwatch"},   {"elapsed", qint64(0)},
                 {"running", false},       {"lastLap", qint64(0)},
                 {"laps", QVariantList()}, {"startWall", qint64(0)}};
  m_tracking = {{"title", ""},
                {"project", "General"},
                {"elapsed", qint64(0)},
                {"running", false},
                {"startWall", qint64(0)}};
  m_projects = {QVariantMap{{"id", makeId()}, {"title", "General"}}};
  m_zones = {QString::fromUtf8(QTimeZone::systemTimeZoneId()), "UTC"};
  m_zones.removeDuplicates();
  m_timers = {QVariantMap{{"id", makeId()},
                          {"title", "Focus"},
                          {"duration", qint64(1500000)},
                          {"remaining", qint64(1500000)},
                          {"running", false},
                          {"finished", false},
                          {"repeat", false},
                          {"sound", false},
                          {"cycles", 0},
                          {"startWall", qint64(0)}}};
  for (int seconds : {60, 300, 900, 1500, 3600})
    m_presets.append(
        QVariantMap{{"id", makeId()},
                    {"title", QString::number(seconds / 60) + " min"},
                    {"seconds", seconds},
                    {"repeat", false}});
  auto program = [&](QString title, QVariantList phases, int rounds) {
    m_intervals.append(QVariantMap{
        {"id", makeId()},
        {"title", title},
        {"phases", phases},
        {"rounds", rounds},
        {"phase", 0},
        {"round", 1},
        {"running", false},
        {"finished", false},
        {"remaining", num(phases.first().toMap(), "seconds") * 1000},
        {"sound", false},
        {"startWall", qint64(0)}});
  };
  program("Pomodoro",
          {QVariantMap{{"name", "Focus"}, {"seconds", 1500}},
           QVariantMap{{"name", "Short break"}, {"seconds", 300}}},
          4);
  program("Workout",
          {QVariantMap{{"name", "Exercise"}, {"seconds", 40}},
           QVariantMap{{"name", "Rest"}, {"seconds", 20}}},
          8);
}
void Timekeeper::load() {
  QSqlQuery q(m_db);
  q.prepare("SELECT value FROM app_state WHERE key='state'");
  if (!q.exec()) {
    fail("Could not read saved clocks.");
    return;
  }
  if (!q.next()) {
    if (!m_ephemeral) {
      QSettings old;
      auto timer = m_timers[0].toMap();
      int duration = qBound(1, old.value("duration", 1500).toInt(), 359999);
      timer["duration"] = qint64(duration) * 1000;
      timer["remaining"] =
          qBound(qint64(0),
                 old.value("remaining", qint64(duration) * 1000).toLongLong(),
                 qint64(duration) * 1000);
      qint64 deadline = old.value("deadline", 0).toLongLong();
      if (deadline > 0) {
        timer["running"] = true;
        timer["deadline"] = m_boot() + deadline - m_wall();
        timer["startWall"] = deadline - qint64(duration) * 1000;
      }
      timer["finished"] = old.value("finished", false);
      m_timers[0] = timer;
      m_stopwatch["elapsed"] =
          qMax(qint64(0), old.value("elapsed", 0).toLongLong());
      m_stopwatch["laps"] = old.value("laps").toList();
      m_stopwatch["lastLap"] = old.value("lastLap", 0);
    }
    save();
    return;
  }
  auto doc = QJsonDocument::fromJson(q.value(0).toByteArray());
  if (!doc.isObject() || doc.object().value("version").toInt() != 2) {
    m_ready = false;
    fail("Saved clock data could not be read. The original database has been "
         "preserved.");
    return;
  }
  auto state = doc.toVariant().toMap();
  bool valid = state.value("stopwatch").canConvert<QVariantMap>() &&
               state.value("tracking").canConvert<QVariantMap>();
  for (const auto &v : state.value("timers").toList()) {
    auto t = v.toMap();
    valid &= !t.value("id").toString().isEmpty() && num(t, "duration") > 0 &&
             num(t, "duration") <= 359999000;
  }
  for (const auto &v : state.value("intervals").toList()) {
    auto i = v.toMap();
    auto phases = i.value("phases").toList();
    valid &= validPhases(phases, i.value("rounds").toInt()) &&
             i.value("phase").toInt() >= 0 &&
             i.value("phase").toInt() < phases.size();
  }
  if (!valid) {
    m_ready = false;
    fail("Saved clock data is invalid. The database has been preserved; "
         "restore a backup before using it.");
    return;
  }

  m_timers = state.value("timers", m_timers).toList();
  m_presets = state.value("presets", m_presets).toList();
  m_alarms = state.value("alarms").toList();
  m_intervals = state.value("intervals", m_intervals).toList();
  m_projects = state.value("projects", m_projects).toList();
  m_zones = state.value("zones", m_zones).toStringList();
  m_stopwatch = state.value("stopwatch", m_stopwatch).toMap();
  m_tracking = state.value("tracking", m_tracking).toMap();
  auto savedSettings = state.value("settings").toMap();
  for (auto it = savedSettings.begin(); it != savedSettings.end(); ++it)
    m_settings[it.key()] = it.value();
  auto restore = [&](QVariantList &items) {
    for (auto &value : items) {
      auto item = value.toMap();
      if (running(item))
        item["deadline"] = m_boot() + num(item, "wallDeadline") - m_wall();
      value = item;
    }
  };
  restore(m_timers);
  restore(m_intervals);
  auto restoreElapsed = [&](QVariantMap &item) {
    if (running(item)) {
      item["elapsed"] = num(item, "elapsed") +
                        qMax(qint64(0), m_wall() - num(item, "savedWall"));
      item["started"] = m_boot();
    }
  };
  restoreElapsed(m_stopwatch);
  // Active tracking segment has not yet been inserted; preserve its whole start
  // across restart.
  if (running(m_tracking)) {
    qint64 segment = num(m_tracking, "segmentElapsed") +
                     qMax(qint64(0), m_wall() - num(m_tracking, "savedWall"));
    m_tracking["started"] = m_boot() - segment;
  }
}
bool Timekeeper::save() {
  if (!m_ready)
    return false;
  auto prepare = [&](QVariantList items) {
    for (auto &value : items) {
      auto item = value.toMap();
      if (running(item))
        item["wallDeadline"] = m_wall() + num(item, "deadline") - m_boot();
      item.remove("deadline");
      value = item;
    }
    return items;
  };
  auto sw = m_stopwatch;
  sw["elapsed"] = elapsed(sw);
  sw["savedWall"] = m_wall();
  sw.remove("started");
  auto tr = m_tracking;
  tr["savedWall"] = m_wall();
  tr["segmentElapsed"] =
      running(tr) ? qMax(qint64(0), m_boot() - num(tr, "started")) : qint64(0);
  tr.remove("started");
  QVariantMap state{{"version", 2},
                    {"timers", prepare(m_timers)},
                    {"presets", m_presets},
                    {"alarms", m_alarms},
                    {"intervals", prepare(m_intervals)},
                    {"stopwatch", sw},
                    {"tracking", tr},
                    {"projects", m_projects},
                    {"zones", m_zones},
                    {"settings", m_settings}};
  QSqlQuery q(m_db);
  q.prepare("INSERT INTO app_state(key,value) VALUES('state',?) ON "
            "CONFLICT(key) DO UPDATE SET value=excluded.value");
  q.addBindValue(QString::fromUtf8(json(state)));
  if (!q.exec()) {
    if (m_atomicDepth)
      m_atomicFailed = true;
    fail("Could not save changes. Check available disk space and data folder "
         "permissions.");
    return false;
  }
  return true;
}
void Timekeeper::changed() {
  save();
  emit dataChanged();
  ++m_revision;
  emit revisionChanged();
}
void Timekeeper::fail(QString message) {
  m_error = message;
  emit errorChanged();
}
void Timekeeper::clearError() {
  if (!m_ready)
    return;
  m_error.clear();
  emit errorChanged();
}
void Timekeeper::setTestClocks(std::function<qint64()> boot,
                               std::function<qint64()> wall) {
  m_boot = std::move(boot);
  m_wall = std::move(wall);
}
qint64 Timekeeper::remaining(const QVariantMap &item) const {
  return running(item) ? qMax(qint64(0), num(item, "deadline") - m_boot())
                       : qMax(qint64(0), num(item, "remaining"));
}
qint64 Timekeeper::elapsed(const QVariantMap &item) const {
  return num(item, "elapsed") +
         (running(item) ? qMax(qint64(0), m_boot() - num(item, "started")) : 0);
}
QString Timekeeper::format(qint64 ms, bool countdown) const {
  auto two = [](qint64 n) { return QString::number(n).rightJustified(2, '0'); };
  qint64 seconds = qMax(qint64(0), ms + (countdown ? 999 : 0)) / 1000;
  return (seconds >= 3600 ? two(seconds / 3600) + ":" : "") +
         two(seconds / 60 % 60) + ":" + two(seconds % 60);
}
QString Timekeeper::fraction(qint64 ms) const {
  int digits = m_settings.value("precision", 2).toInt();
  return QString::number(qMax(qint64(0), ms) / (digits == 3 ? 1 : 10) %
                         (digits == 3 ? 1000 : 100))
      .rightJustified(digits, '0');
}
QString Timekeeper::humanDuration(qint64 ms) const {
  qint64 s = qMax(qint64(0), ms) / 1000;
  if (s >= 3600)
    return QString::number(s / 3600) + "h " + QString::number(s / 60 % 60) +
           "m";
  if (s >= 60)
    return QString::number(s / 60) + "m " + QString::number(s % 60) + "s";
  return QString::number(s) + "s";
}
QVariantMap Timekeeper::timer(QString id) const {
  int index = indexOf(m_timers, id);
  if (index < 0)
    return {};
  auto item = m_timers[index].toMap();
  item["remaining"] = remaining(item);
  item["display"] = format(remaining(item), true);
  item["progress"] =
      double(remaining(item)) / qMax(qint64(1), num(item, "duration"));
  item["status"] = running(item)                     ? "Running"
                   : item.value("finished").toBool() ? "Finished"
                   : num(item, "startWall") > 0 ||
                           num(item, "remaining") < num(item, "duration")
                       ? "Paused"
                       : "Ready";
  return item;
}
QString Timekeeper::addTimer(QString title, int seconds, bool repeat,
                             bool sound) {
  if (seconds < 1 || seconds > 359999 || m_timers.size() >= 50) {
    fail("Use a duration from 1 second to 99:59:59. Up to 50 timers can be "
         "saved.");
    return {};
  }
  QString id = makeId();
  m_timers.append(QVariantMap{{"id", id},
                              {"title", titleOr(title, "Timer")},
                              {"duration", qint64(seconds) * 1000},
                              {"remaining", qint64(seconds) * 1000},
                              {"running", false},
                              {"repeat", repeat},
                              {"sound", sound},
                              {"finished", false},
                              {"cycles", 0},
                              {"startWall", qint64(0)}});
  changed();
  return id;
}
bool Timekeeper::editTimer(QString id, QString title, int seconds, bool repeat,
                           bool sound) {
  int i = indexOf(m_timers, id);
  if (i < 0)
    return false;
  if (running(m_timers[i].toMap())) {
    fail("Pause the timer before editing it.");
    return false;
  }
  if (seconds < 1 || seconds > 359999) {
    fail("Use a duration from 1 second to 99:59:59.");
    return false;
  }
  auto item = m_timers[i].toMap();
  item["title"] = titleOr(title, "Timer");
  item["duration"] = item["remaining"] = qint64(seconds) * 1000;
  item["repeat"] = repeat;
  item["sound"] = sound;
  item["finished"] = false;
  item["cycles"] = 0;
  item["startWall"] = qint64(0);
  m_timers[i] = item;
  changed();
  return true;
}
void Timekeeper::toggleTimer(QString id) {
  tick();
  int i = indexOf(m_timers, id);
  if (i < 0)
    return;
  auto item = m_timers[i].toMap();
  if (running(item)) {
    item["remaining"] = remaining(item);
    item["running"] = false;
  } else {
    if (num(item, "remaining") <= 0) {
      item["remaining"] = item["duration"];
      item["cycles"] = 0;
    }
    if (num(item, "startWall") == 0 || item.value("finished").toBool())
      item["startWall"] = m_wall();
    item["deadline"] = m_boot() + num(item, "remaining");
    item["running"] = true;
    item["finished"] = false;
  }
  m_timers[i] = item;
  changed();
}
void Timekeeper::resetTimer(QString id) {
  int i = indexOf(m_timers, id);
  if (i < 0)
    return;
  auto m = m_timers[i].toMap();
  m["running"] = false;
  m["finished"] = false;
  m["remaining"] = m["duration"];
  m["cycles"] = 0;
  m["startWall"] = qint64(0);
  m_timers[i] = m;
  changed();
}
void Timekeeper::addTime(QString id, int seconds) {
  int i = indexOf(m_timers, id);
  if (i < 0 || seconds < 1 || seconds > 86400)
    return;
  auto m = m_timers[i].toMap();
  qint64 value = remaining(m) + qint64(seconds) * 1000;
  if (value > 359999000) {
    fail("The timer cannot exceed 99:59:59.");
    return;
  }
  m["remaining"] = value;
  if (running(m))
    m["deadline"] = m_boot() + value;
  m["duration"] = qMax(num(m, "duration"), value);
  m["finished"] = false;
  m_timers[i] = m;
  changed();
}
bool Timekeeper::removeTimer(QString id) {
  int i = indexOf(m_timers, id);
  if (i < 0)
    return false;
  if (running(m_timers[i].toMap())) {
    fail("Pause the timer before deleting it.");
    return false;
  }
  m_timers.removeAt(i);
  changed();
  return true;
}
QString Timekeeper::savePreset(QString title, int seconds, bool repeat) {
  if (seconds < 1 || seconds > 359999 || m_presets.size() >= 50) {
    fail("Use a valid duration; up to 50 presets can be saved.");
    return {};
  }
  QString id = makeId();
  m_presets.append(QVariantMap{{"id", id},
                               {"title", titleOr(title, "Preset")},
                               {"seconds", seconds},
                               {"repeat", repeat}});
  changed();
  emit toast("Preset saved");
  return id;
}
void Timekeeper::removePreset(QString id) {
  int i = indexOf(m_presets, id);
  if (i < 0)
    return;
  m_presets.removeAt(i);
  changed();
}
QVariantMap Timekeeper::stopwatch() const {
  auto item = m_stopwatch;
  qint64 value = elapsed(item);
  item["elapsed"] = value;
  item["display"] = format(value);
  item["fraction"] = fraction(value);
  item["id"] = "stopwatch";
  item["status"] = running(item)                             ? "Running"
                   : value > 0 || num(item, "startWall") > 0 ? "Paused"
                                                             : "Ready";
  QVariantList laps = item.value("laps").toList();
  qint64 best = 0, worst = 0;
  for (const auto &entry : laps) {
    qint64 split = num(entry.toMap(), "split");
    if (best == 0 || split < best)
      best = split;
    worst = qMax(worst, split);
  }
  for (auto &entry : laps) {
    auto lap = entry.toMap();
    lap["best"] = laps.size() > 1 && num(lap, "split") == best;
    lap["worst"] = laps.size() > 1 && num(lap, "split") == worst;
    lap["splitText"] =
        format(num(lap, "split")) + "." + fraction(num(lap, "split"));
    lap["totalText"] =
        format(num(lap, "total")) + "." + fraction(num(lap, "total"));
    entry = lap;
  }
  item["laps"] = laps;
  item["bestLap"] = best;
  item["averageLap"] =
      laps.isEmpty() ? 0 : num(m_stopwatch, "lastLap") / laps.size();
  return item;
}
void Timekeeper::toggleStopwatch() {
  if (running(m_stopwatch)) {
    m_stopwatch["elapsed"] = elapsed(m_stopwatch);
    m_stopwatch["running"] = false;
  } else {
    m_stopwatch["started"] = m_boot();
    if (num(m_stopwatch, "startWall") == 0)
      m_stopwatch["startWall"] = m_wall();
    m_stopwatch["running"] = true;
  }
  changed();
}
void Timekeeper::lap() {
  if (!running(m_stopwatch))
    return;
  auto list = m_stopwatch.value("laps").toList();
  if (list.size() >= 1000) {
    fail("This session has 1,000 laps. Save it and start a new session.");
    return;
  }
  qint64 total = elapsed(m_stopwatch);
  list.prepend(QVariantMap{{"number", list.size() + 1},
                           {"split", total - num(m_stopwatch, "lastLap")},
                           {"total", total}});
  m_stopwatch["lastLap"] = total;
  m_stopwatch["laps"] = list;
  changed();
}
void Timekeeper::resetStopwatch() {
  QString title = m_stopwatch.value("title").toString();
  m_stopwatch = {{"title", title},         {"elapsed", qint64(0)},
                 {"running", false},       {"lastLap", qint64(0)},
                 {"laps", QVariantList()}, {"startWall", qint64(0)}};
  changed();
}
void Timekeeper::renameStopwatch(QString title) {
  m_stopwatch["title"] = titleOr(title, "Stopwatch");
  changed();
}
bool Timekeeper::saveStopwatch(QString title) {
  qint64 value = elapsed(m_stopwatch);
  if (value <= 0) {
    fail("Start the stopwatch before saving a session.");
    return false;
  }
  beginAtomic();
  m_stopwatch["title"] = titleOr(title, m_stopwatch.value("title").toString());
  if (running(m_stopwatch))
    toggleStopwatch();
  if (!addSession("stopwatch",
                  titleOr(title, m_stopwatch.value("title").toString()), "",
                  num(m_stopwatch, "startWall"), m_wall(), value,
                  m_stopwatch.value("laps").toList()))
    m_atomicFailed = true;
  resetStopwatch();
  bool ok = endAtomic();
  if (ok)
    emit toast("Stopwatch session saved to History");
  return ok;
}

qint64 Timekeeper::nextAlarmEpoch(const QVariantMap &alarm, qint64 after) {
  QTimeZone zone(alarm.value("zone").toByteArray());
  if (!zone.isValid())
    return 0;
  QTime time = QTime::fromString(alarm.value("time").toString(), "HH:mm");
  if (!time.isValid())
    return 0;
  int days = alarm.value("days").toInt();
  if (days == 0) {
    QDate date = QDate::fromString(alarm.value("date").toString(), Qt::ISODate);
    QDateTime dt(date, time, zone,
                 QDateTime::TransitionResolution::RelativeToBefore);
    return dt.isValid() && dt.toMSecsSinceEpoch() > after
               ? dt.toMSecsSinceEpoch()
               : 0;
  }
  QDate first = QDateTime::fromMSecsSinceEpoch(after, zone).date();
  for (int offset = 0; offset <= 8; ++offset) {
    QDate date = first.addDays(offset);
    if (!(days & (1 << (date.dayOfWeek() - 1))))
      continue;
    QDateTime dt(date, time, zone,
                 QDateTime::TransitionResolution::RelativeToBefore);
    if (dt.isValid() && dt.toMSecsSinceEpoch() > after)
      return dt.toMSecsSinceEpoch();
  }
  return 0;
}
static QString scheduleText(const QVariantMap &alarm) {
  int days = alarm.value("days").toInt();
  if (!days)
    return alarm.value("date").toString();
  if (days == 127)
    return "Every day";
  if (days == 31)
    return "Weekdays";
  if (days == 96)
    return "Weekends";
  QStringList labels;
  QStringList names = {"Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"};
  for (int i = 0; i < 7; ++i)
    if (days & (1 << i))
      labels << names[i];
  return labels.join(", ");
}
QVariantList Timekeeper::alarms() const {
  QVariantList result;
  for (const auto &value : m_alarms) {
    auto alarm = value.toMap();
    QTimeZone zone(alarm.value("zone").toByteArray());
    qint64 next = num(alarm, "snoozeUntil") > 0 ? num(alarm, "snoozeUntil")
                                                : num(alarm, "next");
    alarm["schedule"] = scheduleText(alarm);
    alarm["nextText"] = next > 0 ? (QDateTime::fromMSecsSinceEpoch(next, zone)
                                        .toString("ddd, MMM d · HH:mm t"))
                                 : "Not scheduled";
    alarm["status"] = alarm.value("ringing").toBool()   ? "Time is up"
                      : num(alarm, "snoozeUntil") > 0   ? "Snoozed"
                      : alarm.value("enabled").toBool() ? "Scheduled"
                                                        : "Off";
    result << alarm;
  }
  return result;
}
QString Timekeeper::addAlarm(QString title, QString time, QString date,
                             int days, QString zone, bool sound) {
  if (m_alarms.size() >= 100) {
    fail("Up to 100 alarms can be saved.");
    return {};
  }
  QVariantMap alarm{{"id", makeId()},   {"title", titleOr(title, "Alarm")},
                    {"time", time},     {"date", date},
                    {"days", days},     {"zone", zone},
                    {"sound", sound},   {"enabled", true},
                    {"ringing", false}, {"snoozeUntil", qint64(0)}};
  if (days < 0 || days > 127 || !QTimeZone(zone.toUtf8()).isValid() ||
      !QTime::fromString(time, "HH:mm").isValid()) {
    fail("Choose a valid time and timezone.");
    return {};
  }
  qint64 next = nextAlarmEpoch(alarm, m_wall());
  if (!next) {
    fail("Choose a future date and time, or select repeat days.");
    return {};
  }
  alarm["next"] = next;
  m_alarms << alarm;
  changed();
  return alarm.value("id").toString();
}
bool Timekeeper::editAlarm(QString id, QString title, QString time,
                           QString date, int days, QString zone, bool sound) {
  int i = indexOf(m_alarms, id);
  if (i < 0)
    return false;
  auto alarm = m_alarms[i].toMap();
  alarm["title"] = titleOr(title, "Alarm");
  alarm["time"] = time;
  alarm["date"] = date;
  alarm["days"] = days;
  alarm["zone"] = zone;
  alarm["sound"] = sound;
  qint64 next = nextAlarmEpoch(alarm, m_wall());
  if (days < 0 || days > 127 || !next) {
    fail("Choose a valid future time, timezone, and repeat schedule.");
    return false;
  }
  alarm["next"] = next;
  alarm["enabled"] = true;
  alarm["ringing"] = false;
  alarm["snoozeUntil"] = qint64(0);
  m_alarms[i] = alarm;
  changed();
  return true;
}
void Timekeeper::enableAlarm(QString id, bool enabled) {
  int i = indexOf(m_alarms, id);
  if (i < 0)
    return;
  auto alarm = m_alarms[i].toMap();
  if (enabled) {
    qint64 next = nextAlarmEpoch(alarm, m_wall());
    if (!next) {
      fail("This one-time alarm has passed. Edit its date before enabling it.");
      return;
    }
    alarm["next"] = next;
  }
  alarm["enabled"] = enabled;
  alarm["ringing"] = false;
  alarm["snoozeUntil"] = qint64(0);
  m_alarms[i] = alarm;
  changed();
}
void Timekeeper::snoozeAlarm(QString id, int minutes) {
  int i = indexOf(m_alarms, id);
  if (i < 0)
    return;
  minutes =
      minutes <= 0 ? m_settings.value("snoozeMinutes", 5).toInt() : minutes;
  if (minutes < 1 || minutes > 120)
    return;
  auto alarm = m_alarms[i].toMap();
  alarm["snoozeUntil"] = m_wall() + qint64(minutes) * 60000;
  alarm["ringing"] = false;
  m_alarms[i] = alarm;
  changed();
  emit toast("Alarm snoozed for " + QString::number(minutes) + " minutes");
}
void Timekeeper::dismissAlarm(QString id) {
  int i = indexOf(m_alarms, id);
  if (i < 0)
    return;
  auto alarm = m_alarms[i].toMap();
  alarm["ringing"] = false;
  alarm["snoozeUntil"] = qint64(0);
  m_alarms[i] = alarm;
  changed();
}
void Timekeeper::removeAlarm(QString id) {
  int i = indexOf(m_alarms, id);
  if (i < 0)
    return;
  m_alarms.removeAt(i);
  changed();
}
bool Timekeeper::validPhases(const QVariantList &phases, int rounds) {
  if (phases.isEmpty() || phases.size() > 20 || rounds < 1 || rounds > 99)
    return false;
  for (const auto &phase : phases) {
    auto p = phase.toMap();
    if (num(p, "seconds") < 1 || num(p, "seconds") > 359999 ||
        p.value("name").toString().trimmed().isEmpty())
      return false;
  }
  return true;
}
QString Timekeeper::addInterval(QString title, QVariantList phases, int rounds,
                                bool sound) {
  if (m_intervals.size() >= 50 || !validPhases(phases, rounds)) {
    fail("Add 1–20 named phases with positive durations and 1–99 rounds. Up to "
         "50 programs can be saved.");
    return {};
  }
  QString id = makeId();
  m_intervals << QVariantMap{
      {"id", id},
      {"title", titleOr(title, "Intervals")},
      {"phases", phases},
      {"rounds", rounds},
      {"phase", 0},
      {"round", 1},
      {"remaining", num(phases[0].toMap(), "seconds") * 1000},
      {"running", false},
      {"finished", false},
      {"sound", sound},
      {"startWall", qint64(0)}};
  changed();
  return id;
}
bool Timekeeper::editInterval(QString id, QString title, QVariantList phases,
                              int rounds, bool sound) {
  int i = indexOf(m_intervals, id);
  if (i < 0)
    return false;
  auto item = m_intervals[i].toMap();
  if (running(item)) {
    fail("Pause the interval program before editing it.");
    return false;
  }
  if (!validPhases(phases, rounds)) {
    fail("Use named phases with positive durations and 1–99 rounds.");
    return false;
  }
  item["title"] = titleOr(title, "Intervals");
  item["phases"] = phases;
  item["rounds"] = rounds;
  item["sound"] = sound;
  item["phase"] = 0;
  item["round"] = 1;
  item["remaining"] = num(phases[0].toMap(), "seconds") * 1000;
  item["finished"] = false;
  item["startWall"] = qint64(0);
  m_intervals[i] = item;
  changed();
  return true;
}
QVariantMap Timekeeper::interval(QString id) const {
  int i = indexOf(m_intervals, id);
  if (i < 0)
    return {};
  auto item = m_intervals[i].toMap();
  auto phases = item.value("phases").toList();
  int phase = item.value("phase").toInt();
  if (phases.isEmpty() || phase < 0 || phase >= phases.size())
    return {};
  auto p = phases[phase].toMap();
  qint64 left = remaining(item);
  qint64 total = 0, elapsedNominal = 0;
  for (int j = 0; j < phases.size(); ++j) {
    qint64 duration = num(phases[j].toMap(), "seconds") * 1000;
    total += duration;
    if (j < phase)
      elapsedNominal += duration;
  }
  elapsedNominal += (item.value("round").toInt() - 1) * total +
                    num(p, "seconds") * 1000 - left;
  total *= item.value("rounds").toInt();
  item["phaseName"] = p.value("name");
  item["remaining"] = left;
  item["display"] = format(left, true);
  item["phaseDuration"] = num(p, "seconds") * 1000;
  item["progress"] = double(left) / qMax(qint64(1), num(p, "seconds") * 1000);
  item["totalDuration"] = total;
  item["totalRemaining"] = item.value("finished").toBool()
                               ? 0
                               : qMax(qint64(0), total - elapsedNominal);
  item["status"] = running(item)                     ? "Running"
                   : item.value("finished").toBool() ? "Finished"
                   : elapsedNominal > 0 || num(item, "startWall") > 0 ? "Paused"
                                                                      : "Ready";
  return item;
}
void Timekeeper::toggleInterval(QString id) {
  tick();
  int i = indexOf(m_intervals, id);
  if (i < 0)
    return;
  auto item = m_intervals[i].toMap();
  if (running(item)) {
    item["remaining"] = remaining(item);
    item["running"] = false;
  } else {
    if (item.value("finished").toBool()) {
      item["phase"] = 0;
      item["round"] = 1;
      item["remaining"] =
          num(item.value("phases").toList()[0].toMap(), "seconds") * 1000;
      item["startWall"] = qint64(0);
    }
    if (num(item, "startWall") == 0)
      item["startWall"] = m_wall();
    item["deadline"] = m_boot() + num(item, "remaining");
    item["running"] = true;
    item["finished"] = false;
  }
  m_intervals[i] = item;
  changed();
}
void Timekeeper::resetInterval(QString id) {
  int i = indexOf(m_intervals, id);
  if (i < 0)
    return;
  auto m = m_intervals[i].toMap();
  m["running"] = false;
  m["finished"] = false;
  m["phase"] = 0;
  m["round"] = 1;
  m["remaining"] = num(m.value("phases").toList()[0].toMap(), "seconds") * 1000;
  m["startWall"] = qint64(0);
  m_intervals[i] = m;
  changed();
}
void Timekeeper::advanceInterval(QVariantMap &item, bool manual) {
  auto phases = item.value("phases").toList();
  int phase = item.value("phase").toInt(), round = item.value("round").toInt(),
      advanced = 0;
  QString previous = phases[phase].toMap().value("name").toString();
  qint64 deadline = manual ? m_boot() : num(item, "deadline");
  while (deadline <= m_boot()) {
    ++advanced;
    if (++phase >= phases.size()) {
      phase = 0;
      ++round;
    }
    if (round > item.value("rounds").toInt()) {
      item["running"] = false;
      item["finished"] = true;
      item["remaining"] = qint64(0);
      qint64 total = 0;
      for (const auto &p : phases)
        total += num(p.toMap(), "seconds") * 1000;
      addSession("interval", item.value("title").toString(), "",
                 num(item, "startWall"), m_wall(),
                 total * item.value("rounds").toInt());
      emitAlert("interval", item.value("id").toString(),
                item.value("title").toString() + " complete",
                "All rounds are finished.", item.value("sound").toBool());
      return;
    }
    deadline += num(phases[phase].toMap(), "seconds") * 1000;
  }
  item["phase"] = phase;
  item["round"] = round;
  item["deadline"] = deadline;
  item["remaining"] = qMax(qint64(0), deadline - m_boot());
  if (advanced > 0)
    emitAlert("interval", item.value("id").toString(),
              item.value("title").toString(),
              previous + " complete · " +
                  phases[phase].toMap().value("name").toString() + " started" +
                  (advanced > 1 ? " (caught up after a delay)" : ""),
              item.value("sound").toBool());
}
void Timekeeper::skipInterval(QString id) {
  int i = indexOf(m_intervals, id);
  if (i < 0)
    return;
  auto item = m_intervals[i].toMap();
  if (!running(item)) {
    fail("Start the interval program before skipping a phase.");
    return;
  }
  beginAtomic();
  advanceInterval(item, true);
  m_intervals[i] = item;
  changed();
  endAtomic();
}
bool Timekeeper::removeInterval(QString id) {
  int i = indexOf(m_intervals, id);
  if (i < 0)
    return false;
  if (running(m_intervals[i].toMap())) {
    fail("Pause the program before deleting it.");
    return false;
  }
  m_intervals.removeAt(i);
  changed();
  return true;
}
QString Timekeeper::addProject(QString title) {
  title = title.trimmed().left(120);
  if (title.isEmpty()) {
    fail("Enter a project name.");
    return {};
  }
  for (const auto &p : m_projects)
    if (p.toMap().value("title").toString().compare(title,
                                                    Qt::CaseInsensitive) == 0)
      return p.toMap().value("id").toString();
  if (m_projects.size() >= 100) {
    fail("Up to 100 projects can be saved.");
    return {};
  }
  QString id = makeId();
  m_projects << QVariantMap{{"id", id}, {"title", title}};
  changed();
  return id;
}
bool Timekeeper::removeProject(QString title) {
  if (m_tracking.value("project").toString() == title &&
      !m_tracking.value("title").toString().isEmpty()) {
    fail("Finish the current activity before removing its project.");
    return false;
  }
  for (int i = 0; i < m_projects.size(); ++i)
    if (m_projects[i].toMap().value("title").toString() == title) {
      m_projects.removeAt(i);
      changed();
      return true;
    }
  return false;
}
QVariantMap Timekeeper::tracking() const {
  auto item = m_tracking;
  item["elapsed"] = elapsed(item);
  item["display"] = format(elapsed(item));
  item["fraction"] = fraction(elapsed(item));
  item["status"] = running(item)                              ? "Running"
                   : item.value("title").toString().isEmpty() ? "Ready"
                                                              : "Paused";
  item["id"] = "tracking";
  return item;
}
bool Timekeeper::startTracking(QString title, QString project) {
  if (running(m_tracking)) {
    fail("Pause or finish the current activity before starting another.");
    return false;
  }
  title = title.trimmed().left(120);
  project = titleOr(project, "General");
  if (title.isEmpty()) {
    fail("Enter an activity name.");
    return false;
  }
  addProject(project);
  m_tracking = {{"title", title},       {"project", project},
                {"elapsed", qint64(0)}, {"running", true},
                {"started", m_boot()},  {"startWall", m_wall()}};
  changed();
  return true;
}
void Timekeeper::finishTrackingSegment() {
  if (!running(m_tracking))
    return;
  qint64 segment = qMax(qint64(0), m_boot() - num(m_tracking, "started"));
  if (segment > 0)
    addSession("tracking", m_tracking.value("title").toString(),
               m_tracking.value("project").toString(),
               num(m_tracking, "startWall"), m_wall(), segment);
  m_tracking["elapsed"] = num(m_tracking, "elapsed") + segment;
  m_tracking["running"] = false;
}
void Timekeeper::toggleTracking() {
  if (m_tracking.value("title").toString().isEmpty())
    return;
  beginAtomic();
  if (running(m_tracking))
    finishTrackingSegment();
  else {
    m_tracking["started"] = m_boot();
    m_tracking["startWall"] = m_wall();
    m_tracking["running"] = true;
  }
  changed();
  endAtomic();
}
void Timekeeper::stopTracking() {
  if (m_tracking.value("title").toString().isEmpty())
    return;
  beginAtomic();
  finishTrackingSegment();
  m_tracking = {{"title", ""},
                {"project", "General"},
                {"elapsed", qint64(0)},
                {"running", false},
                {"startWall", qint64(0)}};
  changed();
  if (endAtomic())
    emit toast("Activity saved to History");
}
bool Timekeeper::addZone(QString zone) {
  if (!QTimeZone(zone.toUtf8()).isValid()) {
    fail("Choose a timezone from the list.");
    return false;
  }
  if (m_zones.contains(zone))
    return true;
  if (m_zones.size() >= 30) {
    fail("Up to 30 world clocks can be saved.");
    return false;
  }
  m_zones << zone;
  changed();
  return true;
}
void Timekeeper::removeZone(QString zone) {
  m_zones.removeAll(zone);
  changed();
}
QVariantList Timekeeper::findZones(QString query) const {
  QVariantList result;
  for (const auto &id : QTimeZone::availableTimeZoneIds()) {
    QString zone = QString::fromUtf8(id);
    if (zone.contains(query.trimmed(), Qt::CaseInsensitive) ||
        zone.replace('_', ' ').contains(query.trimmed(), Qt::CaseInsensitive)) {
      result << QVariantMap{{"id", QString::fromUtf8(id)},
                            {"title", QString::fromUtf8(id).replace('_', ' ')}};
      if (result.size() >= 100)
        break;
    }
  }
  return result;
}
QVariantList Timekeeper::zoneCities() const {
  static const QVariantList cities = [] {
    Q_INIT_RESOURCE(zoneassets);
    QFile file(":/data/timezone-cities.json");
    if (!file.open(QIODevice::ReadOnly))
      return QVariantList();
    QVariantList result;
    for (const auto &value : QJsonDocument::fromJson(file.readAll()).array()) {
      auto city = value.toObject().toVariantMap();
      if (QTimeZone(city["id"].toString().toUtf8()).isValid())
        result.append(city);
    }
    return result;
  }();
  return cities;
}
QVariantMap Timekeeper::nearestCity(double latitude, double longitude) const {
  if (!std::isfinite(latitude) || !std::isfinite(longitude) || latitude < -90 ||
      latitude > 90 || longitude < -180 || longitude > 180)
    return {};
  constexpr double radians = 3.14159265358979323846 / 180.0;
  double best = 2;
  QVariantMap nearest;
  for (const auto &value : zoneCities()) {
    auto city = value.toMap();
    double lat = city["latitude"].toDouble(),
           lon = city["longitude"].toDouble();
    double a = std::pow(std::sin((lat - latitude) * radians / 2), 2) +
               std::cos(latitude * radians) * std::cos(lat * radians) *
                   std::pow(std::sin((lon - longitude) * radians / 2), 2);
    if (a < best) {
      best = a;
      nearest = city;
    }
  }
  return nearest;
}
QVariantMap Timekeeper::zoneLocation(QString zone) const {
  for (const auto &value : zoneCities()) {
    auto city = value.toMap();
    if (city["id"].toString() == zone)
      return city;
  }
  return {};
}
QVariantMap Timekeeper::zoneInfo(QString zone, bool comparison) const {
  QTimeZone tz(zone.toUtf8());
  if (!tz.isValid())
    return {};
  qint64 epoch = comparison && m_comparison ? m_comparison : m_wall();
  QDateTime date = QDateTime::fromMSecsSinceEpoch(epoch, tz),
            local = QDateTime::fromMSecsSinceEpoch(epoch,
                                                   QTimeZone::systemTimeZone());
  int offset = date.offsetFromUtc(), diff = offset - local.offsetFromUtc();
  auto offsetLabel = [](int value) {
    return QString(value >= 0 ? "+" : "−") +
           QString::number(qAbs(value) / 3600).rightJustified(2, '0') + ":" +
           QString::number(qAbs(value) / 60 % 60).rightJustified(2, '0');
  };
  return {{"id", zone},
          {"title", zone.section('/', -1).replace('_', ' ')},
          {"zone", zone},
          {"display",
           date.toString(m_settings.value("hour24").toBool() ? "HH:mm:ss"
                                                             : "h:mm:ss AP")},
          {"date", date.toString("ddd, MMM d")},
          {"offset", "UTC" + offsetLabel(offset)},
          {"difference",
           diff == 0 ? "Same time" : offsetLabel(diff) + " from local"},
          {"abbreviation", date.timeZoneAbbreviation()},
          {"daylight", date.isDaylightTime()},
          {"dayOffset", local.date().daysTo(date.date())},
          {"running", false}};
}
QVariantList Timekeeper::worldClocks() const {
  QVariantList list;
  for (const auto &zone : m_zones)
    list << zoneInfo(zone);
  return list;
}
bool Timekeeper::setComparison(QString date, QString time) {
  QDate d = QDate::fromString(date, Qt::ISODate);
  QTime t = QTime::fromString(time, "HH:mm");
  QDateTime local(d, t, QTimeZone::systemTimeZone(),
                  QDateTime::TransitionResolution::Reject);
  if (!d.isValid() || !t.isValid() || !local.isValid()) {
    fail("Enter a valid local date and time. This time may be missing or "
         "ambiguous at a daylight-saving transition.");
    return false;
  }
  m_comparison = local.toMSecsSinceEpoch();
  ++m_revision;
  emit revisionChanged();
  return true;
}
void Timekeeper::resetComparison() {
  m_comparison = 0;
  ++m_revision;
  emit revisionChanged();
}
void Timekeeper::setSetting(QString key, QVariant value) {
  if (!m_settings.contains(key))
    return;
  if (key == "precision" && value.toInt() != 2 && value.toInt() != 3)
    return;
  if (key == "snoozeMinutes" && (value.toInt() < 1 || value.toInt() > 120))
    return;
  m_settings[key] = value;
  changed();
}
void Timekeeper::pauseAll() {
  beginAtomic();
  for (auto &value : m_timers) {
    auto item = value.toMap();
    if (running(item)) {
      item["remaining"] = remaining(item);
      item["running"] = false;
    }
    value = item;
  }
  for (auto &value : m_intervals) {
    auto item = value.toMap();
    if (running(item)) {
      item["remaining"] = remaining(item);
      item["running"] = false;
    }
    value = item;
  }
  if (running(m_stopwatch)) {
    m_stopwatch["elapsed"] = elapsed(m_stopwatch);
    m_stopwatch["running"] = false;
  }
  finishTrackingSegment();
  changed();
  if (endAtomic())
    emit toast("Active countdowns and elapsed clocks paused. Alarms remain "
               "scheduled.");
}
bool Timekeeper::hasActive() const {
  if (running(m_stopwatch) || running(m_tracking))
    return true;
  for (const auto &v : m_timers)
    if (running(v.toMap()))
      return true;
  for (const auto &v : m_intervals)
    if (running(v.toMap()))
      return true;
  for (const auto &v : m_alarms) {
    auto a = v.toMap();
    if (a.value("enabled").toBool() || num(a, "snoozeUntil") > 0)
      return true;
  }
  return false;
}
QVariantList Timekeeper::activeClocks() const {
  QVariantList list;
  for (const auto &v : m_timers) {
    auto t = v.toMap();
    if (running(t))
      list << QVariantMap{{"kind", "timer"},
                          {"id", t.value("id")},
                          {"title", t.value("title")}};
  }
  if (running(m_stopwatch))
    list << QVariantMap{{"kind", "stopwatch"},
                        {"id", "stopwatch"},
                        {"title", m_stopwatch.value("title")}};
  for (const auto &v : m_intervals) {
    auto t = v.toMap();
    if (running(t))
      list << QVariantMap{{"kind", "interval"},
                          {"id", t.value("id")},
                          {"title", t.value("title")}};
  }
  if (running(m_tracking))
    list << QVariantMap{{"kind", "tracking"},
                        {"id", "tracking"},
                        {"title", m_tracking.value("title")}};
  return list;
}
QVariantMap Timekeeper::clockDetails(QString kind, QString id) const {
  if (kind == "timer")
    return timer(id);
  if (kind == "stopwatch")
    return stopwatch();
  if (kind == "interval")
    return interval(id);
  if (kind == "tracking")
    return tracking();
  if (kind == "world")
    return zoneInfo(
        id.isEmpty() ? QString::fromUtf8(QTimeZone::systemTimeZoneId()) : id,
        false);
  return {};
}
void Timekeeper::emitAlert(QString kind, QString id, QString title,
                           QString body, bool sound) {
  bool audible = sound || m_settings.value("sound").toBool();
  if (m_atomicDepth)
    m_pendingAlerts.append(QVariantMap{{"kind", kind},
                                       {"id", id},
                                       {"title", title},
                                       {"body", body},
                                       {"sound", audible}});
  else
    emit alert(kind, id, title, body, audible);
}
void Timekeeper::beginAtomic() {
  if (m_atomicDepth++ > 0)
    return;
  m_snapshot = {{"timers", m_timers},
                {"intervals", m_intervals},
                {"alarms", m_alarms},
                {"stopwatch", m_stopwatch},
                {"tracking", m_tracking}};
  m_pendingAlerts.clear();
  m_atomicFailed = !m_ready || !m_db.transaction();
  if (m_atomicFailed)
    fail("Could not save this action. Your previous clocks and history have "
         "been kept.");
}
bool Timekeeper::endAtomic() {
  if (m_atomicDepth > 1) {
    --m_atomicDepth;
    return !m_atomicFailed;
  }
  if (!m_atomicFailed && !save())
    m_atomicFailed = true;
  bool ok = !m_atomicFailed && m_db.commit();
  m_atomicDepth = 0;
  if (!ok) {
    m_db.rollback();
    m_timers = m_snapshot["timers"].toList();
    m_intervals = m_snapshot["intervals"].toList();
    m_alarms = m_snapshot["alarms"].toList();
    m_stopwatch = m_snapshot["stopwatch"].toMap();
    m_tracking = m_snapshot["tracking"].toMap();
    fail("Could not save this action. Your previous clocks and history have "
         "been kept. Check disk space and folder permissions.");
  }
  loadHistory();
  refreshReports();
  emit dataChanged();
  ++m_revision;
  emit revisionChanged();
  auto pending = m_pendingAlerts;
  m_pendingAlerts.clear();
  m_snapshot.clear();
  if (ok)
    for (const auto &v : pending) {
      auto a = v.toMap();
      emit alert(a["kind"].toString(), a["id"].toString(),
                 a["title"].toString(), a["body"].toString(),
                 a["sound"].toBool());
    }
  return ok;
}
void Timekeeper::tick() {
  bool structural = false;
  bool due = false;
  for (const auto &v : m_timers) {
    auto i = v.toMap();
    due |= running(i) && num(i, "deadline") <= m_boot();
  }
  for (const auto &v : m_intervals) {
    auto i = v.toMap();
    due |= running(i) && num(i, "deadline") <= m_boot();
  }
  for (const auto &v : m_alarms) {
    auto i = v.toMap();
    due |= (num(i, "snoozeUntil") > 0 && num(i, "snoozeUntil") <= m_wall()) ||
           (i.value("enabled").toBool() && num(i, "next") > 0 &&
            num(i, "next") <= m_wall());
  }
  if (due)
    beginAtomic();
  for (int i = 0; i < m_timers.size(); ++i) {
    auto item = m_timers[i].toMap();
    if (!running(item) || num(item, "deadline") > m_boot())
      continue;
    qint64 duration = qMax(qint64(1), num(item, "duration"));
    qint64 overdue = m_boot() - num(item, "deadline"),
           cycles =
               1 + (item.value("repeat").toBool() ? overdue / duration : 0);
    item["cycles"] = num(item, "cycles") + cycles;
    addSession("timer", item.value("title").toString(), "",
               num(item, "startWall"), m_wall(), duration * cycles);
    if (item.value("repeat").toBool()) {
      item["deadline"] = num(item, "deadline") + cycles * duration;
      item["remaining"] = qMax(qint64(0), num(item, "deadline") - m_boot());
      item["startWall"] = m_wall() - overdue % duration;
    } else {
      item["remaining"] = qint64(0);
      item["running"] = false;
      item["finished"] = true;
    }
    m_timers[i] = item;
    structural = true;
    emitAlert("timer", item.value("id").toString(),
              item.value("title").toString() + " finished",
              cycles > 1 ? QString::number(cycles) +
                               " cycles completed while the app was delayed."
              : item.value("repeat").toBool() ? "The next cycle has started."
                                              : "Your timer has finished.",
              item.value("sound").toBool());
  }
  for (int i = 0; i < m_intervals.size(); ++i) {
    auto item = m_intervals[i].toMap();
    if (running(item) && num(item, "deadline") <= m_boot()) {
      advanceInterval(item);
      m_intervals[i] = item;
      structural = true;
    }
  }
  for (int i = 0; i < m_alarms.size(); ++i) {
    auto a = m_alarms[i].toMap();
    bool snooze =
        num(a, "snoozeUntil") > 0 && num(a, "snoozeUntil") <= m_wall();
    bool due = a.value("enabled").toBool() && num(a, "next") > 0 &&
               num(a, "next") <= m_wall();
    if (!snooze && !due)
      continue;
    a["ringing"] = true;
    if (snooze)
      a["snoozeUntil"] = qint64(0);
    if (due) {
      a["lastFired"] = num(a, "next");
      if (a.value("days").toInt() > 0)
        a["next"] = nextAlarmEpoch(a, m_wall());
      else {
        a["enabled"] = false;
        a["next"] = qint64(0);
      }
    }
    m_alarms[i] = a;
    structural = true;
    addSession("alarm", a.value("title").toString(), "", m_wall(), m_wall(), 0);
    emitAlert("alarm", a.value("id").toString(), a.value("title").toString(),
              snooze ? "Snoozed alarm is due." : "Your scheduled alarm is due.",
              a.value("sound").toBool());
  }
  if (due)
    endAtomic();
  else if (structural) {
    save();
    emit dataChanged();
  }
  ++m_revision;
  emit revisionChanged();
  qint64 minute = m_wall() / 60000;
  if (minute != m_lastMinute) {
    m_lastMinute = minute;
    refreshReports();
    emit dataChanged();
  }
  int interval = running(m_stopwatch)        ? 33
                 : !activeClocks().isEmpty() ? 100
                                             : 1000;
  if (m_tick.interval() != interval)
    m_tick.setInterval(interval);
}

bool Timekeeper::addSession(QString kind, QString title, QString project,
                            qint64 start, qint64 end, qint64 duration,
                            QVariantList laps) {
  if (!m_ready || (m_atomicDepth && m_atomicFailed))
    return false;
  QSqlQuery q(m_db);
  q.prepare(
      "INSERT INTO sessions(id,kind,title,project,started,ended,duration,laps) "
      "VALUES(?,?,?,?,?,?,?,?)");
  QString id = makeId();
  for (const auto &value :
       QVariantList{id, kind, title, project, start, qMax(start, end),
                    qMax(qint64(0), duration), QString::fromUtf8(json(laps))})
    q.addBindValue(value);
  if (!q.exec()) {
    if (m_atomicDepth)
      m_atomicFailed = true;
    fail("Could not save the session. Check available disk space.");
    return false;
  }
  if (!m_atomicDepth) {
    loadHistory();
    refreshReports();
  }
  return true;
}
void Timekeeper::loadHistory() {
  if (!m_ready)
    return;
  QVariantList list;
  QSqlQuery q(m_db);
  if (!q.exec("SELECT id,kind,title,project,started,ended,duration,laps FROM "
              "sessions ORDER BY ended DESC,rowid DESC LIMIT 500")) {
    fail("Could not read session history.");
    return;
  }
  while (q.next()) {
    qint64 start = q.value(4).toLongLong(), end = q.value(5).toLongLong(),
           duration = q.value(6).toLongLong();
    auto laps =
        QJsonDocument::fromJson(q.value(7).toByteArray()).toVariant().toList();
    list << QVariantMap{
        {"id", q.value(0)},
        {"kind", q.value(1)},
        {"title", q.value(2)},
        {"project", q.value(3)},
        {"started", start},
        {"ended", end},
        {"duration", duration},
        {"durationText", format(duration)},
        {"date", QDateTime::fromMSecsSinceEpoch(end).toString("MMM d, HH:mm")},
        {"isoDate",
         QDateTime::fromMSecsSinceEpoch(end).date().toString(Qt::ISODate)},
        {"laps", laps}};
  }
  m_sessions = list;
  emit historyChanged();
}
QVariantMap Timekeeper::session(QString id) const {
  int i = indexOf(m_sessions, id);
  if (i >= 0)
    return m_sessions[i].toMap();
  return {};
}
bool Timekeeper::deleteSession(QString id) {
  if (!m_ready)
    return false;
  QSqlQuery q(m_db);
  q.prepare("DELETE FROM sessions WHERE id=?");
  q.addBindValue(id);
  if (!q.exec()) {
    fail("Could not delete this session.");
    return false;
  }
  loadHistory();
  refreshReports();
  emit toast("Session deleted");
  return true;
}
void Timekeeper::refreshReports() {
  if (!m_ready)
    return;
  QTimeZone zone = QTimeZone::systemTimeZone();
  QDate today = QDateTime::fromMSecsSinceEpoch(m_wall(), zone).date();
  qint64 first =
      QDateTime(today.addDays(-6), QTime(0, 0), zone).toMSecsSinceEpoch();
  qint64 todayStart = QDateTime(today, QTime(0, 0), zone).toMSecsSinceEpoch();
  QMap<QString, qint64> byProject;
  QVector<qint64> daily(7, 0);
  QVector<qint64> boundaries;
  for (int i = 0; i <= 7; ++i)
    boundaries << QDateTime(today.addDays(i - 6), QTime(0, 0), zone)
                      .toMSecsSinceEpoch();
  auto distribute = [&](qint64 start, qint64 end, qint64 duration,
                        QString project) {
    if (duration <= 0)
      return;
    end = qMax(start + 1, end);
    for (int i = 0; i < 7; ++i) {
      qint64 overlap = qMax(qint64(0), qMin(end, boundaries[i + 1]) -
                                           qMax(start, boundaries[i]));
      if (overlap > 0) {
        qint64 value = end - start == duration
                           ? overlap
                           : qint64(double(duration) * double(overlap) /
                                    double(end - start));
        daily[i] += value;
        byProject[project] += value;
      }
    }
  };
  QSqlQuery q(m_db);
  q.prepare("SELECT project,started,ended,duration FROM sessions WHERE "
            "kind='tracking' AND ended>=?");
  q.addBindValue(first);
  if (q.exec())
    while (q.next())
      distribute(q.value(1).toLongLong(), q.value(2).toLongLong(),
                 q.value(3).toLongLong(), q.value(0).toString());
  if (running(m_tracking))
    distribute(num(m_tracking, "startWall"), m_wall(),
               qMax(qint64(0), m_boot() - num(m_tracking, "started")),
               m_tracking.value("project").toString());
  qint64 total = 0, max = 1;
  for (auto value : daily) {
    total += value;
    max = qMax(max, value);
  }
  QVariantList days, projects;
  for (int i = 0; i < 7; ++i)
    days << QVariantMap{{"date", today.addDays(i - 6).toString(Qt::ISODate)},
                        {"label", today.addDays(i - 6).toString("ddd")},
                        {"ms", daily[i]},
                        {"display", humanDuration(daily[i])},
                        {"ratio", double(daily[i]) / max},
                        {"today", i == 6}};
  for (auto it = byProject.begin(); it != byProject.end(); ++it)
    projects << QVariantMap{
        {"title", it.key()},
        {"ms", it.value()},
        {"display", humanDuration(it.value())},
        {"ratio", total > 0 ? double(it.value()) / total : 0}};
  std::sort(projects.begin(), projects.end(),
            [](const QVariant &a, const QVariant &b) {
              return num(a.toMap(), "ms") > num(b.toMap(), "ms");
            });
  int completed = 0;
  QSqlQuery count(m_db);
  count.prepare("SELECT COUNT(*) FROM sessions WHERE ended>=?");
  count.addBindValue(todayStart);
  if (count.exec() && count.next())
    completed = count.value(0).toInt();
  m_reports = {{"todayMs", daily[6]},   {"today", humanDuration(daily[6])},
               {"weekMs", total},       {"week", humanDuration(total)},
               {"days", days},          {"projects", projects},
               {"completed", completed}};
  emit reportsChanged();
}
bool Timekeeper::exportCsv(QUrl path, QString kind, QString sessionId) {
  QString local = path.isLocalFile() ? path.toLocalFile() : path.toString();
  if (local.isEmpty()) {
    fail("Choose a CSV file location.");
    return false;
  }
  QSaveFile file(local);
  if (!file.open(QIODevice::WriteOnly)) {
    fail("Could not open the export file. Choose a writable folder.");
    return false;
  }
  auto quote = [](QString value) {
    value.replace('"', "\"\"");
    return "\"" + value + "\"";
  };
  auto row = [&](QStringList cells) {
    for (auto &cell : cells)
      cell = quote(cell);
    QByteArray bytes = (cells.join(',') + "\r\n").toUtf8();
    return file.write(bytes) == bytes.size();
  };
  bool ok = row({"record", "id", "type", "title", "project", "started_iso",
                 "ended_iso", "duration_ms", "lap_number", "lap_split_ms",
                 "lap_total_ms"});
  QSqlQuery q(m_db);
  QString sql =
      "SELECT id,kind,title,project,started,ended,duration,laps FROM sessions";
  if (!sessionId.isEmpty())
    sql += " WHERE id=?";
  else if (!kind.isEmpty())
    sql += " WHERE kind=?";
  sql += " ORDER BY started ASC";
  q.prepare(sql);
  if (!sessionId.isEmpty())
    q.addBindValue(sessionId);
  else if (!kind.isEmpty())
    q.addBindValue(kind);
  if (!q.exec()) {
    fail("Could not read sessions for export.");
    return false;
  }
  while (ok && q.next()) {
    QString start = QDateTime::fromMSecsSinceEpoch(q.value(4).toLongLong(),
                                                   QTimeZone::utc())
                        .toString(Qt::ISODateWithMs),
            end = QDateTime::fromMSecsSinceEpoch(q.value(5).toLongLong(),
                                                 QTimeZone::utc())
                      .toString(Qt::ISODateWithMs);
    QStringList base = {q.value(0).toString(),
                        q.value(1).toString(),
                        q.value(2).toString(),
                        q.value(3).toString(),
                        start,
                        end,
                        q.value(6).toString()};
    QStringList cells = {"session"};
    cells << base << QStringList{"", "", ""};
    ok = row(cells);
    auto laps =
        QJsonDocument::fromJson(q.value(7).toByteArray()).toVariant().toList();
    std::reverse(laps.begin(), laps.end());
    for (const auto &entry : laps) {
      auto lap = entry.toMap();
      cells = {"lap"};
      cells << base
            << QStringList{lap.value("number").toString(),
                           lap.value("split").toString(),
                           lap.value("total").toString()};
      if (!row(cells)) {
        ok = false;
        break;
      }
    }
  }
  if (!ok || !file.commit()) {
    fail("The export could not be saved. Check available disk space.");
    return false;
  }
  emit toast("CSV exported to " + local);
  return true;
}
void Timekeeper::populateDemo() {
  // Illustration is confined to an in-memory database; production clocks are
  // never seeded with activity.
  qint64 base = m_wall();
  QString timerId = addTimer("Tea", 180, false);
  toggleTimer(timerId);
  QString second = addTimer("Deep work", 2700);
  toggleTimer(second);
  renameStopwatch("Morning run");
  m_stopwatch["elapsed"] = qint64(738460);
  m_stopwatch["lastLap"] = qint64(720100);
  m_stopwatch["laps"] = QVariantList{
      QVariantMap{{"number", 3}, {"split", 240120}, {"total", 720100}},
      QVariantMap{{"number", 2}, {"split", 238450}, {"total", 479980}},
      QVariantMap{{"number", 1}, {"split", 241530}, {"total", 241530}}};
  addProject("Personal");
  addProject("Design");
  for (int i = 0; i < 7; ++i) {
    qint64 end = base - qint64(i) * 86400000 - 3600000;
    addSession("tracking", i % 2 ? "Planning" : "Interface work",
               i % 2 ? "Personal" : "Design", end - (i + 1) * 900000, end,
               (i + 1) * 900000);
  }
  addSession("stopwatch", "Morning run", "", base - 3600000, base - 2800000,
             738460, m_stopwatch.value("laps").toList());
  auto date = QDateTime::fromMSecsSinceEpoch(base).date().addDays(1).toString(
      Qt::ISODate);
  addAlarm("Morning routine", "08:00", date, 31,
           QString::fromUtf8(QTimeZone::systemTimeZoneId()));
  addAlarm("Stretch break", "16:30", date, 127,
           QString::fromUtf8(QTimeZone::systemTimeZoneId()));
  addZone("Europe/London");
  addZone("Asia/Tokyo");
  refreshReports();
  changed();
}

static QVariantMap historyRow(const QSqlQuery &q) {
  qint64 end = q.value(5).toLongLong(), duration = q.value(6).toLongLong();
  auto two = [](qint64 n) { return QString::number(n).rightJustified(2, '0'); };
  qint64 sec = duration / 1000;
  QString display = (sec >= 3600 ? two(sec / 3600) + ":" : "") +
                    two(sec / 60 % 60) + ":" + two(sec % 60);
  return {
      {"id", q.value(0)},
      {"kind", q.value(1)},
      {"title", q.value(2)},
      {"project", q.value(3)},
      {"started", q.value(4)},
      {"ended", end},
      {"duration", duration},
      {"durationText", display},
      {"date", QDateTime::fromMSecsSinceEpoch(end).toString("MMM d, HH:mm")},
      {"isoDate",
       QDateTime::fromMSecsSinceEpoch(end).date().toString(Qt::ISODate)},
      {"laps",
       QJsonDocument::fromJson(q.value(7).toByteArray()).toVariant().toList()}};
}
QVariantList Timekeeper::searchHistory(QString query, QString kind,
                                       int offset) const {
  QVariantList list;
  if (!m_ready)
    return list;
  QSqlQuery q(m_db);
  q.prepare("SELECT id,kind,title,project,started,ended,duration,laps FROM "
            "sessions WHERE (?='' OR kind=?) AND (title LIKE ? OR project LIKE "
            "?) ORDER BY ended DESC,rowid DESC LIMIT 100 OFFSET ?");
  q.addBindValue(kind);
  q.addBindValue(kind);
  q.addBindValue("%" + query + "%");
  q.addBindValue("%" + query + "%");
  q.addBindValue(qMax(0, offset));
  if (q.exec())
    while (q.next())
      list << historyRow(q);
  return list;
}
int Timekeeper::countHistory(QString query, QString kind) const {
  if (!m_ready)
    return 0;
  QSqlQuery q(m_db);
  q.prepare("SELECT COUNT(*) FROM sessions WHERE (?='' OR kind=?) AND (title "
            "LIKE ? OR project LIKE ?)");
  q.addBindValue(kind);
  q.addBindValue(kind);
  q.addBindValue("%" + query + "%");
  q.addBindValue("%" + query + "%");
  return q.exec() && q.next() ? q.value(0).toInt() : 0;
}
