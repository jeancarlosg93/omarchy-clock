#include "clockmodel.h"
#include <QSettings>
#include <QDateTime>
#include <QFile>
#include <QDir>
#include <QRegularExpression>
#include <QFontDatabase>
#include <QJsonDocument>
#include <QJsonObject>
#include <cmath>
#include <time.h>

static qint64 bootTime() {
    timespec value{};
    clock_gettime(CLOCK_BOOTTIME, &value);
    return qint64(value.tv_sec) * 1000 + value.tv_nsec / 1000000;
}
ClockModel::ClockModel(bool persist, QObject *parent) : QObject(parent), m_persist(persist), m_now(bootTime) {
    m_tick.setInterval(33);
    m_tick.setTimerType(Qt::PreciseTimer);
    connect(&m_tick, &QTimer::timeout, this, &ClockModel::tick);
    if (m_persist) {
        QSettings s;
        m_duration = qBound(1, s.value("duration", 1500).toInt(), 359999);
        m_remaining = qBound(qint64(0), s.value("remaining", qint64(m_duration) * 1000).toLongLong(), qint64(m_duration) * 1000);
        m_elapsed = qMax(qint64(0), s.value("elapsed", 0).toLongLong());
        m_laps = s.value("laps").toList();
        m_lastLap = s.value("lastLap", 0).toLongLong();
        m_finished = s.value("finished", false).toBool();
        const auto wallDeadline = s.value("deadline", 0).toLongLong();
        if (wallDeadline > 0) {
            m_remaining = qMax(qint64(0), wallDeadline - QDateTime::currentMSecsSinceEpoch());
            m_deadline = m_now() + m_remaining;
            m_timerRunning = true;
            QTimer::singleShot(0, this, &ClockModel::tick);
        }
    }
    updateTick();
}
ClockModel::~ClockModel() { save(); }
qint64 ClockModel::remaining() const { return m_timerRunning ? qMax(qint64(0), m_deadline - m_now()) : m_remaining; }
qint64 ClockModel::elapsed() const { return m_elapsed + (m_stopwatchRunning ? m_now() - m_started : 0); }
void ClockModel::updateTick() {
    if (m_timerRunning || m_stopwatchRunning) m_tick.start(); else m_tick.stop();
}
void ClockModel::setDuration(int seconds) {
    if (m_timerRunning || seconds < 1 || seconds > 359999) return;
    m_duration = seconds;
    m_remaining = qint64(seconds) * 1000;
    m_finished = false;
    emit changed(); save();
}
void ClockModel::toggleTimer() {
    // Reconcile completion first, so pausing an expired timer cannot suppress its alert.
    tick();
    if (m_timerRunning) { m_remaining = remaining(); m_timerRunning = false; }
    else {
        if (m_remaining <= 0) m_remaining = qint64(m_duration) * 1000;
        m_deadline = m_now() + m_remaining;
        m_timerRunning = true; m_finished = false;
    }
    updateTick(); emit changed(); save();
}
void ClockModel::resetTimer() {
    m_timerRunning = false; m_finished = false;
    m_remaining = qint64(m_duration) * 1000;
    updateTick(); emit changed(); save();
}
void ClockModel::toggleStopwatch() {
    if (m_stopwatchRunning) { m_elapsed = elapsed(); m_stopwatchRunning = false; }
    else { m_started = m_now(); m_stopwatchRunning = true; }
    updateTick(); emit changed(); save();
}
void ClockModel::resetStopwatch() {
    m_stopwatchRunning = false; m_elapsed = m_lastLap = 0; m_laps.clear();
    updateTick(); emit changed(); save();
}
void ClockModel::lap() {
    if (!m_stopwatchRunning || m_laps.size() >= 1000) return;
    const qint64 total = elapsed();
    m_laps.prepend(QVariantMap{{"number", m_laps.size() + 1}, {"split", total - m_lastLap}, {"total", total}});
    m_lastLap = total;
    emit changed(); save();
}
void ClockModel::tick() {
    if (m_timerRunning && remaining() == 0) {
        m_timerRunning = false; m_remaining = 0; m_finished = true;
        updateTick(); save(); emit timerFinished();
    }
    emit changed();
}
void ClockModel::save() {
    if (!m_persist) return;
    QSettings s;
    s.setValue("duration", m_duration);
    s.setValue("remaining", remaining());
    s.setValue("deadline", m_timerRunning ? QDateTime::currentMSecsSinceEpoch() + remaining() : qint64(0));
    // Stopwatch reopens paused, retaining elapsed time and laps.
    s.setValue("elapsed", elapsed()); s.setValue("laps", m_laps); s.setValue("lastLap", m_lastLap);
    s.setValue("finished", m_finished);
}
QString ClockModel::format(qint64 ms, bool countdown) const {
    qint64 sec = qMax(qint64(0), ms + (countdown ? 999 : 0)) / 1000;
    auto two = [](qint64 n) { return QString::number(n).rightJustified(2, '0'); };
    if (sec >= 3600) return two(sec / 3600) + ":" + two(sec / 60 % 60) + ":" + two(sec % 60);
    return two(sec / 60) + ":" + two(sec % 60);
}
QString ClockModel::hundredths(qint64 ms) const { return QString::number(qMax(qint64(0), ms) / 10 % 100).rightJustified(2, '0'); }

static QByteArray read(const QString &path) { QFile f(path); return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray(); }
static QColor blend(QColor a, QColor b, double t) {
    return QColor::fromRgbF(a.redF()*(1-t)+b.redF()*t, a.greenF()*(1-t)+b.greenF()*t, a.blueF()*(1-t)+b.blueF()*t);
}
static double luminance(QColor c) {
    auto linear = [](double v) { return v <= .04045 ? v / 12.92 : std::pow((v+.055)/1.055, 2.4); };
    return .2126*linear(c.redF()) + .7152*linear(c.greenF()) + .0722*linear(c.blueF());
}
static double contrast(QColor a, QColor b) {
    const double x = luminance(a), y = luminance(b);
    return (qMax(x, y)+.05)/(qMin(x, y)+.05);
}
Theme::Theme(QObject *parent) : QObject(parent) {
    m_poll.setInterval(1500);
    connect(&m_poll, &QTimer::timeout, this, &Theme::reload);
    reload(); m_poll.start();
}
void Theme::reload() {
    const QString home = QDir::homePath();
    const QString state = qEnvironmentVariable("XDG_STATE_HOME", home + "/.local/state");
    QString path = state + "/omarchy/current/theme";
    auto bytes = read(path + "/colors.toml");
    if (bytes.isEmpty()) { path = home + "/.config/omarchy/current/theme"; bytes = read(path + "/colors.toml"); }
    auto name = read(state + "/omarchy/current/theme.name").trimmed();
    auto fontData = read(home + "/.config/omarchy/shell-font.json");
    const auto source = bytes + name + fontData;
    if (!m_colors.isEmpty() && source == m_source) return;
    if (bytes.isEmpty() && !m_colors.isEmpty()) return; // Atomic theme swaps briefly remove the old directory.
    m_source = source;
    QMap<QString, QString> raw;
    QRegularExpression re("^\\s*([\\w-]+)\\s*=\\s*[\"'](#[0-9a-fA-F]{6})[\"']", QRegularExpression::MultilineOption);
    auto match = re.globalMatch(QString::fromUtf8(bytes));
    while (match.hasNext()) { auto m = match.next(); raw[m.captured(1)] = m.captured(2); }
    auto color = [&](QStringList keys, const char *fallback) {
        for (const auto &key : keys) if (raw.contains(key)) return QColor(raw[key]);
        return QColor(fallback);
    };
    const QColor bg = color({"background"}, "#1a1b26"), fg = color({"foreground"}, "#c0caf5");
    const QColor accent = color({"accent", "blue", "color4"}, "#7aa2f7");
    QColor muted = color({"dark_foreground", "muted"}, "#a9b1d6");
    if (contrast(muted, bg) < 4.5) muted = blend(fg, bg, .25);
    const QColor surface = blend(bg, fg, .065);
    if (contrast(muted, surface) < 4.5) muted = fg;
    const QColor ink = contrast(Qt::white, accent) >= contrast(Qt::black, accent) ? QColor(Qt::white) : QColor(Qt::black);
    m_colors = {{"background", bg}, {"foreground", fg}, {"accent", accent}, {"onAccent", ink},
        {"muted", muted}, {"surface", surface}, {"border", blend(bg, fg, .18)},
        {"hover", blend(bg, fg, .12)}, {"success", color({"green", "color2"}, "#9ece6a")}};
    m_name = name.isEmpty() ? "Omarchy" : QString::fromUtf8(name).replace('-', ' ');
    m_font = QFontDatabase::systemFont(QFontDatabase::FixedFont).family();
    const auto font = QJsonDocument::fromJson(fontData).object();
    if (font.value("enabled").toBool() && !font.value("family").toString().isEmpty()) m_font = font.value("family").toString();
    emit changed();
}
