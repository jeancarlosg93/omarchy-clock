#include "clockmodel.h"
#include <QCoreApplication>
#include <QSettings>
#include <QTemporaryDir>
#include <QDebug>
#include <cstdlib>
#define CHECK(x) do { if (!(x)) { qCritical() << "Failed:" << #x << "line" << __LINE__; return 1; } } while (0)
int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    app.setOrganizationName("OmarchyClockTests"); app.setApplicationName("Tests");
    qint64 now = 1000;
    ClockModel c(false); c.setTestClock([&] { return now; });
    int completions = 0;
    QObject::connect(&c, &ClockModel::timerFinished, [&] { ++completions; });
    c.setDuration(5); c.toggleTimer(); now += 1100;
    CHECK(c.remaining() == 3900); CHECK(c.format(c.remaining(), true) == "00:04");
    c.toggleTimer(); now += 90000; CHECK(c.remaining() == 3900);
    c.toggleTimer(); now += 3901; c.tick(); c.tick();
    CHECK(c.finished()); CHECK(!c.timerRunning()); CHECK(completions == 1);
    c.toggleTimer(); CHECK(c.remaining() == 5000); CHECK(!c.finished());
    c.setDuration(60); CHECK(c.duration() == 5); // A running duration cannot be changed.
    c.resetTimer(); c.setDuration(0); CHECK(c.duration() == 5);
    c.setDuration(359999); CHECK(c.format(c.remaining(), true) == "99:59:59");
    c.setDuration(360000); CHECK(c.duration() == 359999);
    c.setDuration(1); c.toggleTimer(); now += 3600000; c.tick();
    CHECK(c.remaining() == 0); CHECK(completions == 2); // Suspend/late event recovery.
    c.toggleStopwatch(); now += 1234; c.lap(); now += 2567; c.lap();
    CHECK(c.elapsed() == 3801); CHECK(c.laps().size() == 2);
    CHECK(c.laps()[0].toMap()["split"].toLongLong() == 2567);
    CHECK(c.laps()[0].toMap()["total"].toLongLong() == 3801);
    c.toggleStopwatch(); now += 8000; CHECK(c.elapsed() == 3801);
    c.toggleStopwatch(); now += 500; CHECK(c.elapsed() == 4301);
    c.setDuration(2); c.toggleTimer(); now += 2001; c.tick();
    CHECK(c.finished()); CHECK(c.stopwatchRunning()); CHECK(completions == 3);
    c.resetStopwatch(); CHECK(c.elapsed() == 0); CHECK(c.laps().isEmpty());
    CHECK(c.hundredths(1234) == "23");
    QTemporaryDir settingsDir;
    QSettings::setPath(QSettings::NativeFormat, QSettings::UserScope, settingsDir.path());
    { ClockModel saved(true); saved.setDuration(12); saved.toggleTimer(); saved.save(); }
    { ClockModel restored(true); CHECK(restored.duration() == 12); CHECK(restored.timerRunning()); CHECK(restored.remaining() > 11000); restored.resetTimer(); }
    { ClockModel restored(true); CHECK(!restored.timerRunning()); CHECK(restored.remaining() == 12000); }
    qInfo() << "Timing, pause/resume, independent clocks, laps, suspend recovery, limits, and persistence passed.";
    return 0;
}
