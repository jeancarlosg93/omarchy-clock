#pragma once
#include <QObject>
#include <QTimer>
#include <QVariantList>
#include <QVariantMap>
#include <QColor>
#include <functional>

class ClockModel : public QObject {
    Q_OBJECT
    Q_PROPERTY(qint64 remaining READ remaining NOTIFY changed)
    Q_PROPERTY(int duration READ duration NOTIFY changed)
    Q_PROPERTY(bool timerRunning READ timerRunning NOTIFY changed)
    Q_PROPERTY(bool finished READ finished NOTIFY changed)
    Q_PROPERTY(qint64 elapsed READ elapsed NOTIFY changed)
    Q_PROPERTY(bool stopwatchRunning READ stopwatchRunning NOTIFY changed)
    Q_PROPERTY(QVariantList laps READ laps NOTIFY changed)
public:
    explicit ClockModel(bool persist = true, QObject *parent = nullptr);
    ~ClockModel();
    qint64 remaining() const;
    int duration() const { return m_duration; }
    bool timerRunning() const { return m_timerRunning; }
    bool finished() const { return m_finished; }
    qint64 elapsed() const;
    bool stopwatchRunning() const { return m_stopwatchRunning; }
    QVariantList laps() const { return m_laps; }
    Q_INVOKABLE void setDuration(int seconds);
    Q_INVOKABLE void toggleTimer();
    Q_INVOKABLE void resetTimer();
    Q_INVOKABLE void toggleStopwatch();
    Q_INVOKABLE void resetStopwatch();
    Q_INVOKABLE void lap();
    Q_INVOKABLE QString format(qint64 ms, bool countdown = false) const;
    Q_INVOKABLE QString hundredths(qint64 ms) const;
    void tick();
    void save();
    void setTestClock(std::function<qint64()> clock) { m_now = std::move(clock); }
signals:
    void changed();
    void timerFinished();
private:
    bool m_persist;
    int m_duration = 1500;
    qint64 m_remaining = 1500000, m_deadline = 0;
    qint64 m_elapsed = 0, m_started = 0, m_lastLap = 0;
    bool m_timerRunning = false, m_stopwatchRunning = false, m_finished = false;
    QVariantList m_laps;
    QTimer m_tick;
    std::function<qint64()> m_now;
    void updateTick();
};

class Theme : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantMap colors READ colors NOTIFY changed)
    Q_PROPERTY(QString name READ name NOTIFY changed)
    Q_PROPERTY(QString fontFamily READ fontFamily NOTIFY changed)
public:
    explicit Theme(QObject *parent = nullptr);
    QVariantMap colors() const { return m_colors; }
    QString name() const { return m_name; }
    QString fontFamily() const { return m_font; }
    void reload();
signals:
    void changed();
private:
    QVariantMap m_colors;
    QString m_name, m_font;
    QByteArray m_source;
    QTimer m_poll;
};
