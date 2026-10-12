#include "clockmodel.h"
#include "desktop.h"
#include "timekeeper.h"
#include <QApplication>
#include <QDir>
#include <QFile>
#include <QFontMetricsF>
#include <QJSValue>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQmlProperty>
#include <QQuickItem>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QTemporaryDir>
#include <QtTest/QSignalSpy>
#include <QtTest/QTest>
#include <cmath>
#include <cstdio>
#define CHECK(x)                                                               \
  do {                                                                         \
    if (!(x)) {                                                                \
      fprintf(stderr, "Failed: %s line %d\n", #x, __LINE__);                   \
      return 1;                                                                \
    }                                                                          \
  } while (0)
int main(int argc, char **argv) {
  QApplication app(argc, argv);
  app.setQuitOnLastWindowClosed(false);
  QQuickStyle::setStyle("Basic");
  qInstallMessageHandler(
      [](QtMsgType, const QMessageLogContext &, const QString &m) {
        fprintf(stderr, "%s\n", qPrintable(m));
      });
  qint64 boot = 100000, wall = 1791640800000;
  auto advance = [&](qint64 ms) {
    boot += ms;
    wall += ms;
  };
  Timekeeper keeper(
      ":memory:", true, nullptr, [&] { return boot; }, [&] { return wall; });
  Theme theme;
  Desktop desktop(&keeper, &theme, true);
  QQmlApplicationEngine qml;
  qml.rootContext()->setContextProperty("engine", &keeper);
  qml.rootContext()->setContextProperty("theme", &theme);
  qml.rootContext()->setContextProperty("shell", &desktop);
  int warnings = 0;
  QObject::connect(
      &qml, &QQmlEngine::warnings,
      [&](const QList<QQmlError> &list) { warnings += list.size(); });
  qml.load(QUrl("qrc:/Main.qml"));
  CHECK(!qml.rootObjects().isEmpty());
  auto window = qobject_cast<QQuickWindow *>(qml.rootObjects().first());
  CHECK(window);
  desktop.setWindow(window);
  QTest::qWait(60);
  auto go = [&](QString page) {
    bool ok =
        QMetaObject::invokeMethod(window, "go", Q_ARG(QVariant, QVariant(page)),
                                  Q_ARG(QVariant, QVariant(false)));
    QTest::qWait(30);
    return ok;
  };
  for (const QString &page :
       QStringList{"Today", "Timers", "Stopwatch", "Alarms", "Intervals",
                   "World clocks", "Tracking", "History", "Settings"}) {
    CHECK(go(page));
    auto loader = window->findChild<QObject *>("pageLoader");
    CHECK(loader);
    CHECK(loader->property("status").toInt() == 1);
    auto item = qvariant_cast<QObject *>(loader->property("item"));
    CHECK(item);
    CHECK(item->property("width").toDouble() > 600);
  }
  std::function<QQuickItem *(QQuickItem *, QString)> findVisual =
      [&](QQuickItem *item, QString name) -> QQuickItem * {
    if (item->objectName() == name)
      return item;
    for (auto child : item->childItems())
      if (auto found = findVisual(child, name))
        return found;
    return nullptr;
  };
  auto control = [&](const char *name) -> QObject * {
    auto found = window->findChild<QObject *>(name);
    return found ? found
                 : findVisual(window->contentItem(), QString::fromUtf8(name));
  };
  auto click = [&](const char *name) {
    auto button = control(name);
    bool ok = button && button->property("enabled").toBool() &&
              QMetaObject::invokeMethod(button, "clicked");
    QTest::qWait(10);
    return ok;
  };
  CHECK(go("Timers"));
  CHECK(click("addTimerButton"));
  CHECK(control("timerNameField"));
  control("timerNameField")->setProperty("text", "UI timer");
  control("timerDuration")->setProperty("seconds", 3723);
  int timers = keeper.timers().size();
  CHECK(click("saveTimerButton"));
  CHECK(keeper.timers().size() == timers + 1);
  QString timerId = window->property("timerId").toString();
  CHECK(keeper.timer(timerId)["duration"].toLongLong() == 3723000);
  CHECK(click("timerStartButton"));
  CHECK(keeper.timer(timerId)["running"].toBool());
  CHECK(go("Stopwatch"));
  CHECK(click("stopwatchStartButton"));
  advance(1234);
  keeper.tick();
  CHECK(click("lapButton"));
  CHECK(keeper.stopwatchLaps().size() == 1);
  CHECK(keeper.timer(timerId)["running"].toBool());
  auto stopwatchPage = control("stopwatchPage");
  CHECK(stopwatchPage);
  auto face = stopwatchPage->findChild<QQuickItem *>("timeDisplay");
  auto fraction = stopwatchPage->findChild<QQuickItem *>("fractionDisplay");
  CHECK(face && fraction);
  CHECK(fraction->x() > face->x());
  CHECK(fraction->y() < face->y() + face->height());
  CHECK(click("saveStopwatchButton"));
  CHECK(keeper.countHistory("", "stopwatch") == 1);
  CHECK(keeper.stopwatch()["elapsed"].toLongLong() == 0);
  CHECK(go("Alarms"));
  CHECK(click("addAlarmButton"));
  control("alarmNameField")->setProperty("text", "UI alarm");
  control("alarmTimeField")->setProperty("text", "08:30");
  control("alarmDateField")->setProperty("text", "2099-01-01");
  CHECK(click("saveAlarmButton"));
  CHECK(keeper.alarms().size() == 1);
  CHECK(go("Intervals"));
  CHECK(click("addIntervalButton"));
  control("intervalNameField")->setProperty("text", "UI program");
  int programs = keeper.intervals().size();
  CHECK(click("saveIntervalButton"));
  CHECK(keeper.intervals().size() == programs + 1);
  CHECK(click("intervalStartButton"));
  CHECK(keeper.interval(window->property("intervalId").toString())["running"]
            .toBool());
  CHECK(go("Tracking"));
  control("activityNameField")->setProperty("text", "UI activity");
  CHECK(click("trackingStartButton"));
  advance(60000);
  keeper.tick();
  CHECK(click("trackingPauseButton"));
  CHECK(keeper.reports()["todayMs"].toLongLong() == 60000);
  CHECK(click("trackingPauseButton"));
  advance(15000);
  CHECK(click("trackingFinishButton"));
  CHECK(keeper.reports()["todayMs"].toLongLong() == 75000);
  CHECK(go("World clocks"));
  auto mapItem = qobject_cast<QQuickItem *>(control("worldMap"));
  CHECK(mapItem);
  // The map's projection and a real pointer click select a city, then add it.
  auto mapCoordinate = [&](const char *method, double value) {
    QVariant result;
    QMetaObject::invokeMethod(mapItem, method, Q_RETURN_ARG(QVariant, result),
                              Q_ARG(QVariant, QVariant(value)));
    return result.toDouble();
  };
  double cityX = mapCoordinate("xAt", -69.9);
  double cityY = mapCoordinate("yAt", 18.466667);
  QPointF point = mapItem->mapToScene(QPointF(cityX, cityY));
  QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, point.toPoint());
  QTest::qWait(30);
  auto worldPage = control("worldPage");
  CHECK(worldPage);
  CHECK(mapItem->property("selectedZone").toString() ==
        "America/Santo_Domingo");
  CHECK(click("addZoneButton"));
  CHECK(keeper.favoriteZones().contains("America/Santo_Domingo"));
  CHECK(click("worldZoomIn"));
  CHECK(mapItem->property("zoom").toDouble() == 1.5);
  CHECK(click("worldZoomIn"));
  CHECK(click("worldZoomIn"));
  auto pageScroll =
      qvariant_cast<QObject *>(worldPage->property("contentItem"));
  CHECK(pageScroll && pageScroll->property("contentY").isValid());
  double initialScroll = pageScroll->property("contentY").toDouble();
  QString initialCity = mapItem->property("selectedZone").toString();
  QPoint dragStart =
      mapItem->mapToScene(QPointF(mapItem->width() / 2, mapItem->height() / 2))
          .toPoint();
  QTest::mousePress(window, Qt::LeftButton, Qt::NoModifier, dragStart);
  for (int step = 1; step <= 8; ++step) {
    QTest::mouseMove(window, dragStart + QPoint(step * 15, -step * 5), 10);
    QTest::qWait(10);
  }
  auto grabber = window->mouseGrabberItem();
  fprintf(stderr, "Map drag probe: pan=(%.1f,%.1f), grab=%s, pageScroll=%.1f\n",
          mapItem->property("panX").toDouble(),
          mapItem->property("panY").toDouble(),
          grabber ? qPrintable(grabber->metaObject()->className()) : "none",
          pageScroll->property("contentY").toDouble());
  CHECK(grabber && grabber->property("pressed").toBool());
  CHECK(grabber->property("cursorShape").toInt() == Qt::ClosedHandCursor);
  CHECK(qAbs(mapItem->property("panX").toDouble() - 120) < 1);
  CHECK(qAbs(mapItem->property("panY").toDouble() + 40) < 1);
  CHECK(qAbs(pageScroll->property("contentY").toDouble() - initialScroll) < 1);
  QTest::mouseRelease(window, Qt::LeftButton, Qt::NoModifier,
                      dragStart + QPoint(120, -40));
  QTest::qWait(30);
  CHECK(mapItem->property("selectedZone").toString() == initialCity);
  CHECK(grabber->property("cursorShape").toInt() == Qt::OpenHandCursor);
  // Leaving the map while holding the button must not transfer the grab.
  QTest::mousePress(window, Qt::LeftButton, Qt::NoModifier, dragStart);
  QPoint outsideMap =
      mapItem->mapToScene(QPointF(mapItem->width() + 12, mapItem->height() / 2))
          .toPoint();
  QTest::mouseMove(window, outsideMap, 20);
  QTest::qWait(20);
  CHECK(window->mouseGrabberItem() == grabber);
  CHECK(grabber->property("pressed").toBool());
  CHECK(grabber->property("cursorShape").toInt() == Qt::ClosedHandCursor);
  CHECK(mapItem->property("panX").toDouble() > 200);
  QTest::mouseRelease(window, Qt::LeftButton, Qt::NoModifier, outsideMap);
  QTest::qWait(30);
  CHECK(mapItem->property("selectedZone").toString() == initialCity);
  CHECK(qAbs(pageScroll->property("contentY").toDouble() - initialScroll) < 1);
  QMetaObject::invokeMethod(mapItem, "panTo", Q_ARG(QVariant, QVariant(120)),
                            Q_ARG(QVariant, QVariant(-40)));
  double longitudePan = mapItem->property("longitudePan").toDouble();
  double latitudePan = mapItem->property("latitudePan").toDouble();
  for (QSize size :
       {QSize(1800, 900), QSize(600, 780), QSize(380, 460), QSize(1150, 780)}) {
    window->resize(size);
    QTest::qWait(40);
    double across = mapCoordinate("xAt", 30) - mapCoordinate("xAt", 0);
    double up = mapCoordinate("yAt", 0) - mapCoordinate("yAt", 30);
    CHECK(qAbs(across - up) < .001);
    CHECK(qAbs(mapItem->property("longitudePan").toDouble() - longitudePan) <
          .001);
    CHECK(qAbs(mapItem->property("latitudePan").toDouble() - latitudePan) <
          .001);
  }
  CHECK(click("worldMapReset"));
  CHECK(mapItem->property("zoom").toDouble() == 1);
  CHECK(mapItem->property("panX").toDouble() == 0);
  CHECK(mapItem->property("panY").toDouble() == 0);
  // Padding is not a geographic location and cannot change the chosen city.
  double padding = mapItem->property("originX").toDouble();
  CHECK(padding > 0);
  QPoint paddingPoint =
      mapItem->mapToScene(QPointF(padding / 2, mapItem->height() / 2))
          .toPoint();
  QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, paddingPoint);
  CHECK(mapItem->property("selectedZone").toString() == initialCity);
  CHECK(qAbs((mapCoordinate("xAt", 180) - mapCoordinate("xAt", -180)) /
                 (mapCoordinate("yAt", -90) - mapCoordinate("yAt", 90)) -
             2) < .001);
  control("compareDateField")->setProperty("text", "2026-10-11");
  control("compareTimeField")->setProperty("text", "12:00");
  CHECK(click("compareButton"));
  CHECK(keeper.comparisonEpoch() > 0);
  keeper.resetComparison();
  CHECK(go("History"));
  control("historySearchField")->setProperty("text", "UI activity");
  QTest::qWait(240);
  auto history = control("historyPage");
  CHECK(history);
  CHECK(history->property("total").toInt() == 2);
  CHECK(go("Stopwatch"));
  auto input = qobject_cast<QQuickItem *>(control("stopwatchNameField"));
  CHECK(input);
  input->forceActiveFocus();
  QTest::keyClick(window, Qt::Key_A);
  QTest::keyClick(window, Qt::Key_Space);
  CHECK(!keeper.stopwatch()["running"].toBool());
  input->setFocus(false);
  window->requestActivate();
  window->contentItem()->forceActiveFocus();
  QTest::qWait(20);
  CHECK(!window->property("editing").toBool());
  QTest::keyClick(window, Qt::Key_Space);
  QTest::qWait(20);
  CHECK(keeper.stopwatch()["running"].toBool());
  advance(500);
  QTest::keyClick(window, Qt::Key_L);
  QTest::qWait(10);
  CHECK(keeper.stopwatchLaps().size() == 1);
  QTest::keyClick(window, Qt::Key_F11);
  QTest::qWait(10);
  CHECK(window->visibility() == QWindow::FullScreen);
  QTest::keyClick(window, Qt::Key_Escape);
  QTest::qWait(10);
  CHECK(window->visibility() == QWindow::Windowed);
  CHECK(QMetaObject::invokeMethod(window, "enterFocus",
                                  Q_ARG(QVariant, QVariant("stopwatch")),
                                  Q_ARG(QVariant, QVariant("stopwatch"))));
  CHECK(window->property("focusMode").toBool());
  QTest::keyClick(window, Qt::Key_Escape);
  CHECK(!window->property("focusMode").toBool());
  CHECK(QMetaObject::invokeMethod(window, "toggleCompact"));
  auto compact = qobject_cast<QQuickWindow *>(control("compactWindow"));
  CHECK(compact && compact->isVisible());
  CHECK(compact->transientParent() == nullptr);
  CHECK(compact->flags().testFlag(Qt::WindowStaysOnTopHint));
  compact->resize(160, 110);
  QTest::qWait(30);
  CHECK(compact->size() == QSize(160, 110));
  CHECK(compact->property("tiny").toBool());
  for (const auto &name : {"compactCloseButton", "compactPlaybackButton",
                            "compactLapButton", "compactOpenButton",
                            "compactTimeFace"}) {
    auto item = compact->findChild<QQuickItem *>(name);
    CHECK(item && item->isVisible());
    const auto position = item->mapToScene(QPointF());
    CHECK(position.x() >= 0 && position.y() >= 0);
    CHECK(position.x() + item->width() <= compact->width() + 1);
    CHECK(position.y() + item->height() <= compact->height() + 1);
  }
  // Hiding the main window must leave the independent compact display mapped.
  window->hide();
  QTest::qWait(30);
  CHECK(compact->isVisible());
  compact->requestActivate();
  QTest::qWait(20);
  QTest::keyClick(compact, Qt::Key_W, Qt::ControlModifier);
  QTest::qWait(20);
  CHECK(!compact->isVisible() && !window->isVisible());
  compact->resize(360, 240);
  window->show();
  window->requestActivate();
  window->resize(380, 460);
  QTest::qWait(30);
  for (const QString &page :
       QStringList{"Today", "Timers", "Stopwatch", "Alarms", "Intervals",
                   "World clocks", "Tracking", "History", "Settings"}) {
    CHECK(go(page));
    auto loader = control("pageLoader");
    CHECK(loader->property("status").toInt() == 1);
    auto item = qvariant_cast<QObject *>(loader->property("item"));
    fprintf(stderr, "Narrow %s: %.0f px\n", qPrintable(page),
            item->property("width").toDouble());
    CHECK(item->property("width").toDouble() <= 344);
  }
  CHECK(go("Timers"));
  CHECK(click("addTimerButton"));
  CHECK(go("Alarms"));
  CHECK(click("addAlarmButton"));
  CHECK(go("Intervals"));
  CHECK(click("addIntervalButton"));

  // F1: selectors retain their full ordinary labels at the minimum window
  // width.
  auto fits = [&](QObject *combo) {
    if (!combo)
      return false;
    auto text = qvariant_cast<QObject *>(combo->property("contentItem"));
    if (!text)
      return false;
    return text->property("width").toDouble() + 1 >=
           QFontMetricsF(qvariant_cast<QFont>(text->property("font")))
               .horizontalAdvance(combo->property("displayText").toString());
  };
  for (QString name :
       QStringList{"Today", "Timers", "Stopwatch", "Alarms", "Intervals",
                   "World clocks", "Tracking", "History", "Settings"}) {
    CHECK(go(name));
    CHECK(fits(control("toolChoice")));
  }
  CHECK(go("History"));
  CHECK(fits(control("historyKindChoice")));
  CHECK(go("Settings"));
  CHECK(fits(control("precisionChoice")));
  keeper.setSetting("precision", 3);
  QTest::qWait(10);
  CHECK(fits(control("precisionChoice")));
  // F2: real Space presses belong to native focused controls.
  CHECK(go("Stopwatch"));
  if (!keeper.stopwatch()["running"].toBool())
    keeper.toggleStopwatch();
  advance(300);
  keeper.tick();
  auto lapButton = qobject_cast<QQuickItem *>(control("lapButton"));
  CHECK(lapButton);
  window->requestActivate();
  QTest::qWait(20);
  lapButton->forceActiveFocus();
  QTest::qWait(10);
  int lapCount = keeper.stopwatchLaps().size();
  QSignalSpy clicks(lapButton, SIGNAL(clicked()));
  QSignalSpy presses(lapButton, SIGNAL(pressed()));
  QVariant permitted;
  QMetaObject::invokeMethod(window, "clockShortcutAllowed",
                            Q_RETURN_ARG(QVariant, permitted),
                            Q_ARG(QVariant, QVariant::fromValue(lapButton)));
  QTest::keyClick(window, Qt::Key_Space);
  QTest::qWait(160);
  CHECK(clicks.count() == 1 && presses.count() == 1 && !permitted.toBool());
  CHECK(keeper.stopwatchLaps().size() == lapCount + 1);
  CHECK(keeper.stopwatch()["running"].toBool());
  CHECK(go("Settings"));
  auto notification = qobject_cast<QQuickItem *>(control("notificationSwitch"));
  CHECK(notification);
  notification->forceActiveFocus();
  bool enabled = keeper.settings()["notifications"].toBool();
  QTest::keyClick(window, Qt::Key_Space);
  QTest::qWait(10);
  CHECK(keeper.settings()["notifications"].toBool() != enabled);
  CHECK(keeper.stopwatch()["running"].toBool());
  CHECK(go("Timers"));
  CHECK(click("addTimerButton"));
  auto repeatBox = qobject_cast<QQuickItem *>(control("timerRepeatBox"));
  CHECK(repeatBox);
  repeatBox->forceActiveFocus();
  bool repeated = repeatBox->property("checked").toBool();
  QTest::keyClick(window, Qt::Key_Space);
  QTest::qWait(10);
  CHECK(repeatBox->property("checked").toBool() != repeated);
  CHECK(keeper.stopwatch()["running"].toBool());
  // F3: keyboard rows select another clock and open saved details without
  // toggling clocks.
  QString secondTimer = keeper.addTimer("Second keyboard timer", 30);
  CHECK(go("Timers"));
  auto row = qobject_cast<QQuickItem *>(
      control(qPrintable("clockRow-timer-" + secondTimer)));
  CHECK(row);
  CHECK(row->property("activeFocusOnTab").toBool());
  CHECK(QQmlProperty(row, "Accessible.name", qmlContext(row))
            .read()
            .toString()
            .contains("Second keyboard timer"));
  auto first = qobject_cast<QQuickItem *>(control("addTimerButton"));
  CHECK(first);
  first->forceActiveFocus();
  for (int n = 0; n < 80 && !row->hasActiveFocus(); ++n) {
    QTest::keyClick(window, Qt::Key_Tab);
    QTest::qWait(1);
  }
  CHECK(row->hasActiveFocus());
  CHECK(row->mapToScene(QPointF(0, row->height())).y() <= window->height());
  QTest::keyClick(window, Qt::Key_Space);
  QTest::qWait(10);
  CHECK(window->property("timerId").toString() == secondTimer);
  CHECK(!keeper.timer(secondTimer)["running"].toBool());
  CHECK(click("editTimerButton"));
  CHECK(control("timerNameField")->property("text").toString() ==
        "Second keyboard timer");
  QString secondProgram =
      keeper.addInterval("Second keyboard program",
                         {QVariantMap{{"name", "One"}, {"seconds", 30}}}, 1);
  CHECK(go("Intervals"));
  row = qobject_cast<QQuickItem *>(
      control(qPrintable("clockRow-interval-" + secondProgram)));
  CHECK(row);
  row->forceActiveFocus();
  QTest::keyClick(window, Qt::Key_Return);
  QTest::qWait(10);
  CHECK(window->property("intervalId").toString() == secondProgram);
  CHECK(click("editIntervalButton"));
  CHECK(control("intervalNameField")->property("text").toString() ==
        "Second keyboard program");
  keeper.toggleTimer(secondTimer);
  CHECK(go("Today"));
  row = qobject_cast<QQuickItem *>(
      control(qPrintable("clockRow-timer-" + secondTimer)));
  CHECK(row);
  row->forceActiveFocus();
  QTest::keyClick(window, Qt::Key_Return);
  QTest::qWait(10);
  CHECK(window->property("page").toString() == "Timers");
  CHECK(keeper.timer(secondTimer)["running"].toBool());
  keeper.resetTimer(secondTimer);
  CHECK(go("History"));
  auto record = keeper.searchHistory("", "stopwatch").first().toMap();
  row = qobject_cast<QQuickItem *>(
      control(qPrintable("historyRow-" + record["id"].toString())));
  CHECK(row);
  row->forceActiveFocus();
  QTest::keyClick(window, Qt::Key_Space);
  QTest::qWait(10);
  auto asMap = [](QVariant value) {
    return value.metaType() == QMetaType::fromType<QJSValue>()
               ? value.value<QJSValue>().toVariant().toMap()
               : value.toMap();
  };
  auto selected = asMap(control("historyPage")->property("selectedSession"));
  CHECK(selected["id"].toString() == record["id"].toString());
  CHECK(control("exportSessionButton") && control("deleteSessionButton"));
  CHECK(keeper.stopwatch()["running"].toBool());
  // F5: typed drafts survive live ticks, paused ticks, navigation blur, and
  // save.
  CHECK(go("Stopwatch"));
  input = qobject_cast<QQuickItem *>(control("stopwatchNameField"));
  CHECK(input);
  input->forceActiveFocus();
  QTest::keyClick(window, Qt::Key_A, Qt::ControlModifier);
  QString typed = "morning run notes";
  for (QChar ch : typed) {
    QTest::keyClick(window,
                    ch == QLatin1Char(' ') ? Qt::Key_Space
                                           : Qt::Key(ch.toUpper().unicode()),
                    ch.isUpper() ? Qt::ShiftModifier : Qt::NoModifier);
    advance(50);
    keeper.tick();
    QTest::qWait(2);
  }
  CHECK(input->property("text").toString() == typed);
  keeper.toggleStopwatch();
  advance(500);
  keeper.tick();
  CHECK(input->property("text").toString() == typed);
  input->setFocus(false);
  QTest::qWait(10);
  CHECK(keeper.stopwatch()["title"].toString() == typed);
  CHECK(click("saveStopwatchButton"));
  CHECK(keeper.countHistory(typed, "stopwatch") == 1);
  CHECK(input->property("text").toString() == typed);
  // F2 compact: focused Lap uses Space while the blank display retains playback
  // shortcut.
  keeper.toggleStopwatch();
  advance(500);
  keeper.tick();
  CHECK(QMetaObject::invokeMethod(window, "toggleCompact"));
  compact = qobject_cast<QQuickWindow *>(control("compactWindow"));
  CHECK(compact && compact->isVisible());
  compact->requestActivate();
  QTest::qWait(20);
  auto compactLap = compact->findChild<QQuickItem *>("compactLapButton");
  CHECK(compactLap);
  compactLap->forceActiveFocus();
  QTest::qWait(20);
  lapCount = keeper.stopwatchLaps().size();
  QTest::keyClick(compact, Qt::Key_Space);
  QTest::qWait(160);
  CHECK(compactLap->hasActiveFocus());
  CHECK(keeper.stopwatchLaps().size() == lapCount + 1);
  CHECK(keeper.stopwatch()["running"].toBool());
  compactLap->setFocus(false);
  compact->contentItem()->forceActiveFocus();
  QTest::qWait(10);
  QTest::keyClick(compact, Qt::Key_Space);
  QTest::qWait(160);
  CHECK(!keeper.stopwatch()["running"].toBool());
  compact->hide();
  window->requestActivate();
  QTest::qWait(10);
  // F4: Today and focus details remain live while World comparison persists.
  CHECK(keeper.setComparison("2028-12-15", "12:00"));
  CHECK(go("Today"));
  auto today = control("todayPage");
  CHECK(today);
  QString now = keeper.clockDetails("world", "")["display"].toString();
  CHECK(asMap(today->property("local"))["display"].toString() == now);
  advance(2000);
  keeper.tick();
  CHECK(asMap(today->property("local"))["display"].toString() != now);
  keeper.resetComparison();
  keeper.setSetting("keepInTray", false);
  window->close();
  CHECK(control("confirmationDialog")->property("visible").toBool());
  CHECK(click("cancelConfirmationButton"));
  CHECK(window->isVisible());
  QTemporaryDir state;
  QString dir = state.path() + "/omarchy/current/theme";
  QDir().mkpath(dir);
  qputenv("XDG_STATE_HOME", state.path().toUtf8());
  QFile colors(dir + "/colors.toml");
  CHECK(colors.open(QIODevice::WriteOnly));
  colors.write("background = \"#ffffff\"\nforeground = \"#111111\"\naccent = "
               "\"#5573e2\"\nmuted = \"#cccccc\"\n");
  colors.close();
  theme.reload();
  CHECK(theme.colors()["background"].value<QColor>() == QColor("#ffffff"));
  CHECK(theme.colors()["muted"].value<QColor>() != QColor("#cccccc"));
  auto contrast = [](QColor a, QColor b) {
    auto luminance = [](QColor c) {
      auto linear = [](double v) {
        return v <= .04045 ? v / 12.92 : std::pow((v + .055) / 1.055, 2.4);
      };
      return .2126 * linear(c.redF()) + .7152 * linear(c.greenF()) +
             .0722 * linear(c.blueF());
    };
    double x = luminance(a), y = luminance(b);
    return (qMax(x, y) + .05) / (qMin(x, y) + .05);
  };
  auto verifyPopup = [&]() {
    window->resize(1150, 780);
    if (!go("Settings"))
      return false;
    auto choice = control("precisionChoice");
    auto popup =
        choice ? qvariant_cast<QObject *>(choice->property("popup")) : nullptr;
    if (!popup)
      return false;
    auto choiceItem = qobject_cast<QQuickItem *>(choice);
    QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier,
                      choiceItem
                          ->mapToScene(QPointF(choiceItem->width() / 2,
                                               choiceItem->height() / 2))
                          .toPoint());
    QTest::qWait(50);
    auto background = popup->findChild<QObject *>("choicePopupBackground");
    auto list = qvariant_cast<QObject *>(popup->property("contentItem"));
    auto current = list
                       ? qvariant_cast<QObject *>(list->property("currentItem"))
                       : nullptr;
    auto rowText =
        current ? current->findChild<QObject *>("choicePopupText") : nullptr;
    auto rowBackground =
        current ? current->findChild<QObject *>("choicePopupRowBackground")
                : nullptr;
    bool ok = background && rowText && rowBackground &&
              background->property("color").value<QColor>() ==
                  theme.colors()["surface"].value<QColor>() &&
              contrast(rowText->property("color").value<QColor>(),
                       rowBackground->property("color").value<QColor>()) >= 4.5;
    if (!ok)
      fprintf(
          stderr,
          "Popup probes bg=%p list=%p current=%p text=%p rowbg=%p visible=%d\n",
          (void *)background, (void *)list, (void *)current, (void *)rowText,
          (void *)rowBackground, popup->property("visible").toBool());
    QTest::keyClick(window, Qt::Key_Down);
    QTest::keyClick(window, Qt::Key_Return);
    QTest::qWait(20);
    return ok && !popup->property("visible").toBool();
  };
  CHECK(verifyPopup());
  CHECK(colors.open(QIODevice::WriteOnly | QIODevice::Truncate));
  colors.write("background = \"#080a0e\"\nforeground = \"#ffffff\"\naccent = "
               "\"#5573e2\"\n");
  colors.close();
  theme.reload();
  CHECK(verifyPopup());
  // The bar controls the clock it advertises, even with another focus choice.
  keeper.pauseAll();
  QString barTimer = keeper.addTimer("Bar test", 300);
  keeper.toggleTimer(barTimer);
  window->setProperty("focusKind", "world");
  window->setProperty("focusId", "UTC");
  CHECK(desktop.barStatus()["clock"].toMap()["id"].toString() == barTimer);
  desktop.command("bar-pause");
  CHECK(!keeper.timer(barTimer)["running"].toBool());
  CHECK(desktop.barStatus()["clock"].toMap()["status"].toString() == "Paused");
  desktop.command("bar-pause");
  CHECK(keeper.timer(barTimer)["running"].toBool());
  desktop.command("bar-open");
  CHECK(window->property("focusId").toString() == barTimer);
  keeper.toggleStopwatch();
  window->setProperty("focusKind", "stopwatch");
  window->setProperty("focusId", "stopwatch");
  CHECK(desktop.barStatus()["clock"].toMap()["kind"].toString() == "stopwatch");
  CHECK(desktop.barStatus()["runningCount"].toInt() == 2);
  keeper.setSetting("barVisible", false);
  CHECK(!desktop.barStatus()["enabled"].toBool());
  keeper.pauseAll();
  QFile::remove(colors.fileName());
  theme.reload();
  CHECK(theme.colors()["background"].value<QColor>() == QColor("#080a0e"));
  CHECK(warnings == 0);
  fprintf(stderr, "All native suite pages loaded without QML warnings.\n");
  return 0;
}
