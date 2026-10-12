#pragma once
#include <QObject>
#include <QSqlDatabase>
#include <QTimer>
#include <QUrl>
#include <QVariantList>
#include <QVariantMap>
#include <functional>

class Timekeeper : public QObject {
  Q_OBJECT
  Q_PROPERTY(QVariantList timers READ timers NOTIFY dataChanged)
  Q_PROPERTY(QVariantList presets READ presets NOTIFY dataChanged)
  Q_PROPERTY(QVariantList alarms READ alarms NOTIFY dataChanged)
  Q_PROPERTY(QVariantList intervals READ intervals NOTIFY dataChanged)
  Q_PROPERTY(QVariantList sessions READ sessions NOTIFY historyChanged)
  Q_PROPERTY(QVariantList projects READ projects NOTIFY dataChanged)
  Q_PROPERTY(QStringList favoriteZones READ favoriteZones NOTIFY dataChanged)
  Q_PROPERTY(QVariantList worldClocks READ worldClocks NOTIFY revisionChanged)
  Q_PROPERTY(QVariantList zoneCities READ zoneCities CONSTANT)
  Q_PROPERTY(QVariantList activeClocks READ activeClocks NOTIFY dataChanged)
  Q_PROPERTY(QVariantMap stopwatch READ stopwatch NOTIFY revisionChanged)
  Q_PROPERTY(QVariantList stopwatchLaps READ stopwatchLaps NOTIFY dataChanged)
  Q_PROPERTY(QVariantMap tracking READ tracking NOTIFY revisionChanged)
  Q_PROPERTY(QVariantMap reports READ reports NOTIFY reportsChanged)
  Q_PROPERTY(QVariantMap settings READ settings NOTIFY dataChanged)
  Q_PROPERTY(int revision READ revision NOTIFY revisionChanged)
  Q_PROPERTY(QString error READ error NOTIFY errorChanged)
  Q_PROPERTY(QString storagePath READ storagePath CONSTANT)
  Q_PROPERTY(qint64 comparisonEpoch READ comparisonEpoch NOTIFY revisionChanged)
  Q_PROPERTY(bool hasActive READ hasActive NOTIFY dataChanged)
  Q_PROPERTY(bool trackHasActivity READ trackHasActivity NOTIFY dataChanged)
public:
  explicit Timekeeper(QString databasePath = QString(), bool ephemeral = false,
                      QObject *parent = nullptr,
                      std::function<qint64()> boot = {},
                      std::function<qint64()> wall = {});
  ~Timekeeper();
  QVariantList timers() const { return m_timers; }
  QVariantList presets() const { return m_presets; }
  QVariantList alarms() const;
  QVariantList intervals() const { return m_intervals; }
  QVariantList sessions() const { return m_sessions; }
  QVariantList projects() const { return m_projects; }
  QStringList favoriteZones() const { return m_zones; }
  QVariantList worldClocks() const;
  QVariantList zoneCities() const;
  QVariantList activeClocks() const;
  QVariantMap stopwatch() const;
  QVariantList stopwatchLaps() const {
    return stopwatch().value("laps").toList();
  }
  QVariantMap tracking() const;
  QVariantMap reports() const { return m_reports; }
  QVariantMap settings() const { return m_settings; }
  int revision() const { return m_revision; }
  QString error() const { return m_error; }
  QString storagePath() const { return m_path; }
  qint64 comparisonEpoch() const { return m_comparison; }
  bool hasActive() const;
  bool trackHasActivity() const {
    return !m_tracking.value("title").toString().isEmpty();
  }
  Q_INVOKABLE QVariantMap timer(QString id) const;
  Q_INVOKABLE QVariantMap interval(QString id) const;
  Q_INVOKABLE QVariantMap clockDetails(QString kind,
                                       QString id = QString()) const;
  Q_INVOKABLE QString addTimer(QString title, int seconds, bool repeat = false,
                               bool sound = false);
  Q_INVOKABLE bool editTimer(QString id, QString title, int seconds,
                             bool repeat, bool sound);
  Q_INVOKABLE void toggleTimer(QString id);
  Q_INVOKABLE void resetTimer(QString id);
  Q_INVOKABLE void addTime(QString id, int seconds);
  Q_INVOKABLE bool removeTimer(QString id);
  Q_INVOKABLE QString savePreset(QString title, int seconds,
                                 bool repeat = false);
  Q_INVOKABLE void removePreset(QString id);
  Q_INVOKABLE void toggleStopwatch();
  Q_INVOKABLE void lap();
  Q_INVOKABLE void resetStopwatch();
  Q_INVOKABLE void renameStopwatch(QString title);
  Q_INVOKABLE bool saveStopwatch(QString title);
  Q_INVOKABLE QString addAlarm(QString title, QString time, QString date,
                               int days, QString zone, bool sound = false);
  Q_INVOKABLE bool editAlarm(QString id, QString title, QString time,
                             QString date, int days, QString zone, bool sound);
  Q_INVOKABLE void enableAlarm(QString id, bool enabled);
  Q_INVOKABLE void snoozeAlarm(QString id, int minutes = 0);
  Q_INVOKABLE void dismissAlarm(QString id);
  Q_INVOKABLE void removeAlarm(QString id);
  Q_INVOKABLE QString addInterval(QString title, QVariantList phases,
                                  int rounds, bool sound = false);
  Q_INVOKABLE bool editInterval(QString id, QString title, QVariantList phases,
                                int rounds, bool sound);
  Q_INVOKABLE void toggleInterval(QString id);
  Q_INVOKABLE void resetInterval(QString id);
  Q_INVOKABLE void skipInterval(QString id);
  Q_INVOKABLE bool removeInterval(QString id);
  Q_INVOKABLE QString addProject(QString title);
  Q_INVOKABLE bool removeProject(QString title);
  Q_INVOKABLE bool startTracking(QString title, QString project);
  Q_INVOKABLE void toggleTracking();
  Q_INVOKABLE void stopTracking();
  Q_INVOKABLE bool addZone(QString zone);
  Q_INVOKABLE void removeZone(QString zone);
  Q_INVOKABLE QVariantList findZones(QString query) const;
  Q_INVOKABLE QVariantMap nearestCity(double latitude, double longitude) const;
  Q_INVOKABLE QVariantMap zoneLocation(QString zone) const;
  Q_INVOKABLE QVariantMap zoneInfo(QString zone, bool comparison = true) const;
  Q_INVOKABLE bool setComparison(QString date, QString time);
  Q_INVOKABLE void resetComparison();
  Q_INVOKABLE void setSetting(QString key, QVariant value);
  Q_INVOKABLE void pauseAll();
  Q_INVOKABLE QString format(qint64 ms, bool countdown = false) const;
  Q_INVOKABLE QString fraction(qint64 ms) const;
  Q_INVOKABLE QString humanDuration(qint64 ms) const;
  Q_INVOKABLE bool exportCsv(QUrl path, QString kind = QString(),
                             QString sessionId = QString());
  Q_INVOKABLE bool deleteSession(QString id);
  Q_INVOKABLE QVariantMap session(QString id) const;
  Q_INVOKABLE QVariantList searchHistory(QString query, QString kind,
                                         int offset = 0) const;
  Q_INVOKABLE int countHistory(QString query, QString kind) const;
  Q_INVOKABLE void clearError();
  void tick();
  Q_INVOKABLE bool save();
  void populateDemo();
  void setTestClocks(std::function<qint64()> boot,
                     std::function<qint64()> wall);
  static qint64 nextAlarmEpoch(const QVariantMap &alarm, qint64 after);
signals:
  void dataChanged();
  void revisionChanged();
  void historyChanged();
  void reportsChanged();
  void errorChanged();
  void alert(QString kind, QString id, QString title, QString body, bool sound);
  void toast(QString message);

private:
  QSqlDatabase m_db;
  QString m_connection, m_path, m_error;
  bool m_ephemeral, m_ready = false;
  int m_atomicDepth = 0;
  bool m_atomicFailed = false;
  QVariantMap m_snapshot;
  QVariantList m_pendingAlerts;
  void beginAtomic();
  bool endAtomic();
  QVariantList m_timers, m_presets, m_alarms, m_intervals, m_projects,
      m_sessions;
  QStringList m_zones;
  QVariantMap m_stopwatch, m_tracking, m_settings, m_reports;
  QTimer m_tick, m_checkpoint;
  int m_revision = 0;
  qint64 m_comparison = 0, m_lastMinute = -1;
  std::function<qint64()> m_boot, m_wall;
  void defaults();
  void load();
  void loadHistory();
  void refreshReports();
  void changed();
  void fail(QString message);
  void emitAlert(QString kind, QString id, QString title, QString body,
                 bool sound);
  bool addSession(QString kind, QString title, QString project, qint64 start,
                  qint64 end, qint64 duration, QVariantList laps = {});
  void finishTrackingSegment();
  void advanceInterval(QVariantMap &item, bool manual = false);
  qint64 remaining(const QVariantMap &item) const;
  qint64 elapsed(const QVariantMap &item) const;
  static int indexOf(const QVariantList &list, QString id);
  static QString makeId();
  static bool validPhases(const QVariantList &phases, int rounds);
};
