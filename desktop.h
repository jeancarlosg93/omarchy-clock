#pragma once
#include "clockmodel.h"
#include "timekeeper.h"
#include <QObject>
#include <QPointer>
#include <QProcess>
#include <QQuickWindow>
#include <QSystemTrayIcon>
class Desktop : public QObject {
  Q_OBJECT
  Q_PROPERTY(QObject *focusedWindow READ focusedWindow NOTIFY focusChanged)
  Q_PROPERTY(bool trayAvailable READ trayAvailable NOTIFY changed)
  Q_PROPERTY(bool autoStart READ autoStart NOTIFY changed)
  Q_PROPERTY(QString showShortcut READ showShortcut CONSTANT)
  Q_PROPERTY(QString pauseShortcut READ pauseShortcut CONSTANT)
public:
  Desktop(Timekeeper *engine, Theme *theme, bool ephemeral = false,
          QObject *parent = nullptr);
  QObject *focusedWindow() const;
  bool trayAvailable() const;
  bool autoStart() const;
  QString showShortcut() const;
  QString pauseShortcut() const;
  void setWindow(QQuickWindow *window);
  Q_INVOKABLE void command(QString command);
  Q_INVOKABLE void presentWindow(QQuickWindow *window);
  Q_INVOKABLE void publishBarState();
  QVariantMap barStatus();
  Q_INVOKABLE void setAutoStart(bool enabled);
  Q_INVOKABLE void testNotification();
  Q_INVOKABLE void testSound();
  Q_INVOKABLE void openDataFolder();
  Q_INVOKABLE void quit();
signals:
  void changed();
  void focusChanged();
  void message(QString text);
  void notificationDelivered(uint id);
private slots:
  void notificationAction(uint id, QString action);

private:
  Timekeeper *m_engine;
  Theme *m_theme;
  bool m_ephemeral;
  QPointer<QQuickWindow> m_window;
  QSystemTrayIcon m_tray;
  QPointer<QProcess> m_sound;
  QMap<uint, QPair<QString, QString>> m_notifications;
  QString m_barKind, m_barId;
  void focusBarClock();
  void notify(QString kind, QString id, QString title, QString body,
              bool sound);
  void applyPalette();
};
