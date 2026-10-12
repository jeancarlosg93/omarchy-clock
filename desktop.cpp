#include "desktop.h"
#include "appicon.h"
#include <QAction>
#include <QApplication>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMenu>
#include <QSaveFile>
#include <QStandardPaths>

static QString autostartPath() {
  return QStandardPaths::writableLocation(
             QStandardPaths::GenericConfigLocation) +
         "/autostart/omarchy-clock.desktop";
}
static QVariantMap integration() {
  QFile f(
      QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) +
      "/omarchy-clock/integration.json");
  if (!f.open(QIODevice::ReadOnly))
    return {};
  return QJsonDocument::fromJson(f.readAll()).toVariant().toMap();
}
Desktop::Desktop(Timekeeper *engine, Theme *theme, bool ephemeral,
                 QObject *parent)
    : QObject(parent), m_engine(engine), m_theme(theme),
      m_ephemeral(ephemeral) {
  connect(engine, &Timekeeper::alert, this, &Desktop::notify);
  connect(qApp, &QGuiApplication::focusWindowChanged, this,
          &Desktop::focusChanged);
  connect(theme, &Theme::changed, this, &Desktop::applyPalette);
  applyPalette();
  if (ephemeral)
    return;
  auto barTick = new QTimer(this);
  barTick->setInterval(1000);
  connect(barTick, &QTimer::timeout, this, &Desktop::publishBarState);
  connect(engine, &Timekeeper::dataChanged, this, &Desktop::publishBarState);
  barTick->start();
  connect(qApp, &QCoreApplication::aboutToQuit, this, [] {
    QFile::remove(
        QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation) +
        "/omarchy-clock-status.json");
  });
  m_tray.setToolTip("Omarchy Clock · timekeeping continues in the tray");
  auto menu = new QMenu();
  menu->setParent(nullptr);
  auto open = menu->addAction("Open Omarchy Clock");
  connect(open, &QAction::triggered, this, [this] { command("show"); });
  auto pause = menu->addAction("Pause active clocks");
  connect(pause, &QAction::triggered, engine, &Timekeeper::pauseAll);
  connect(menu, &QMenu::aboutToShow, this, [this, pause] {
    pause->setEnabled(!m_engine->activeClocks().isEmpty());
  });
  menu->addSeparator();
  auto exit = menu->addAction("Quit app…");
  connect(exit, &QAction::triggered, this, [this] { command("quit"); });
  m_tray.setContextMenu(menu);
  connect(this, &QObject::destroyed, menu, &QObject::deleteLater);
  connect(&m_tray, &QSystemTrayIcon::activated, this,
          [this](QSystemTrayIcon::ActivationReason reason) {
            if (reason == QSystemTrayIcon::Trigger ||
                reason == QSystemTrayIcon::DoubleClick)
              command("toggle");
          });
  m_tray.show();
  QDBusConnection::sessionBus().connect(
      "org.freedesktop.Notifications", "/org/freedesktop/Notifications",
      "org.freedesktop.Notifications", "ActionInvoked", this,
      SLOT(notificationAction(uint, QString)));
  auto poll = new QTimer(this);
  poll->setInterval(3000);
  connect(poll, &QTimer::timeout, this, [this] { emit changed(); });
  poll->start();
}
void Desktop::setWindow(QQuickWindow *window) {
  m_window = window;
  publishBarState();
}
QVariantMap Desktop::barStatus() {
  QVariantList clocks;
  for (const auto &entry : m_engine->activeClocks()) {
    auto summary = entry.toMap();
    auto details = m_engine->clockDetails(summary["kind"].toString(),
                                          summary["id"].toString());
    details["kind"] = summary["kind"];
    clocks.append(details);
  }
  QString kind = m_window ? m_window->property("focusKind").toString() : "",
          id = m_window ? m_window->property("focusId").toString() : "";
  auto focused = m_engine->clockDetails(kind, id);
  auto previous = m_engine->clockDetails(m_barKind, m_barId);
  QVariantMap clock;
  if (focused["running"].toBool()) {
    clock = focused;
    clock["kind"] = kind;
  } else if (previous["running"].toBool()) {
    clock = previous;
    clock["kind"] = m_barKind;
  } else if (!clocks.isEmpty())
    clock = clocks.first().toMap();
  else if (previous["status"].toString() == "Paused") {
    clock = previous;
    clock["kind"] = m_barKind;
  } else if (focused["status"].toString() == "Paused") {
    clock = focused;
    clock["kind"] = kind;
  }
  m_barKind = clock.value("kind").toString();
  m_barId = clock.value("id").toString();
  // Keep the bridge small: laps and full program definitions remain in the app.
  auto concise = [](QVariantMap input) {
    QVariantMap out;
    for (const auto &key :
         {"id", "kind", "title", "display", "status", "running"})
      out[key] = input.value(key);
    return out;
  };
  QVariantList summaries;
  for (const auto &item : clocks)
    summaries.append(concise(item.toMap()));
  return {{"version", 1},
          {"pid", QCoreApplication::applicationPid()},
          {"updatedAt", QDateTime::currentMSecsSinceEpoch()},
          {"enabled", m_engine->settings().value("barVisible", true)},
          {"clock", clock.isEmpty() ? QVariantMap() : concise(clock)},
          {"clocks", summaries},
          {"runningCount", clocks.size()}};
}
void Desktop::publishBarState() {
  if (m_ephemeral)
    return;
  QSaveFile file(
      QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation) +
      "/omarchy-clock-status.json");
  if (file.open(QIODevice::WriteOnly)) {
    file.setPermissions(QFile::ReadOwner | QFile::WriteOwner);
    auto payload =
        QJsonDocument::fromVariant(barStatus()).toJson(QJsonDocument::Compact);
    if (file.write(payload) == payload.size())
      file.commit();
  }
}
void Desktop::focusBarClock() {
  auto clock = barStatus()["clock"].toMap();
  if (m_window && !clock.isEmpty())
    QMetaObject::invokeMethod(m_window, "enterFocus",
                              Q_ARG(QVariant, clock["kind"]),
                              Q_ARG(QVariant, clock["id"]));
}
QObject *Desktop::focusedWindow() const {
  return QGuiApplication::focusWindow();
}
bool Desktop::trayAvailable() const {
  return !m_ephemeral && QSystemTrayIcon::isSystemTrayAvailable();
}
bool Desktop::autoStart() const { return QFile::exists(autostartPath()); }
QString Desktop::showShortcut() const {
  return integration().value("showShortcut", "Super+Ctrl+Alt+T").toString();
}
QString Desktop::pauseShortcut() const {
  return integration()
      .value("pauseShortcut", "Super+Ctrl+Alt+Space")
      .toString();
}
void Desktop::command(QString command) {
  if (!m_window)
    return;
  if (command == "show") {
    presentWindow(m_window);
  } else if (command == "bar-open") {
    focusBarClock();
    presentWindow(m_window);
  } else if (command == "bar-compact") {
    focusBarClock();
    this->command("compact");
  } else if (command == "bar-pause") {
    auto clock = barStatus()["clock"].toMap();
    QString kind = clock["kind"].toString(), id = clock["id"].toString();
    if (kind == "timer")
      m_engine->toggleTimer(id);
    else if (kind == "stopwatch")
      m_engine->toggleStopwatch();
    else if (kind == "interval")
      m_engine->toggleInterval(id);
    else if (kind == "tracking")
      m_engine->toggleTracking();
  } else if (command == "toggle") {
    if (m_window->isVisible() && m_window->isActive())
      m_window->hide();
    else
      this->command("show");
  } else if (command == "pause")
    QMetaObject::invokeMethod(m_window, "toggleFocused");
  else if (command == "compact") {
    QMetaObject::invokeMethod(m_window, "toggleCompact");
  } else if (command == "quit") {
    this->command("show");
    QMetaObject::invokeMethod(m_window, "requestQuit");
  } else if (command == "save-and-quit") {
    m_engine->save();
    m_window->setProperty("allowQuit", true);
    quit();
  }
  publishBarState();
}
void Desktop::presentWindow(QQuickWindow *window) {
  window->show();
  if (m_ephemeral ||
      qEnvironmentVariableIsEmpty("HYPRLAND_INSTANCE_SIGNATURE") ||
      QGuiApplication::platformName() != "wayland") {
    window->raise();
    window->requestActivate();
    return;
  }
  // Qt activation can focus the old workspace before the compositor samples
  // the workspace from which this window was requested. Let Hyprland focus it.
  // Wayland clients cannot reposition themselves or switch workspaces. Ask the
  // compositor to reveal only our window, retaining its size and valid
  // position.
  QPointer<QQuickWindow> target(window);
  QTimer::singleShot(100, this, [this, target] {
    if (!target || !target->isVisible())
      return;
    const QString title =
        target == m_window ? "Omarchy Clock" : "Omarchy Clock · Compact";
    const QString script = QString::fromUtf8(R"lua(
local ws = hl.get_active_special_workspace() or hl.get_active_workspace()
if not ws or not ws.monitor then return end
for _, w in ipairs(hl.get_windows()) do
  if w.pid == %1 and w.class == "omarchy-clock" and w.title == "%2" and w.mapped then
    local moved = not w.monitor or w.monitor.id ~= ws.monitor.id
      or (not w.pinned and (not w.workspace or w.workspace.id ~= ws.id))
    local m = ws.monitor
    local outside = w.at.x < m.x or w.at.y < m.y
      or w.at.x + w.size.x > m.x + m.width / m.scale
      or w.at.y + w.size.y > m.y + m.height / m.scale
    if moved then
      hl.dispatch(hl.dsp.window.move({window = w, workspace = ws, follow = false}))
    end
    if (moved or outside) and w.floating and w.fullscreen == 0 then
      hl.dispatch(hl.dsp.window.center({window = w}))
    end
    hl.dispatch(hl.dsp.focus({window = w}))
    break
  end
end
)lua")
                               .arg(QCoreApplication::applicationPid())
                               .arg(title);
    auto process = new QProcess(this);
    connect(process, &QProcess::finished, this, [process](int code) {
      auto output = process->readAllStandardOutput().trimmed();
      if (code != 0 || !output.startsWith("ok"))
        qWarning().noquote() << "Could not reveal Omarchy Clock:" << output
                             << process->readAllStandardError();
    });
    connect(process, &QProcess::finished, process, &QObject::deleteLater);
    connect(process, &QProcess::errorOccurred, process, &QObject::deleteLater);
    process->start("hyprctl", {"eval", script});
    QTimer::singleShot(2000, process, [process] {
      if (process->state() != QProcess::NotRunning)
        process->kill();
    });
  });
}
void Desktop::setAutoStart(bool enabled) {
  if (m_ephemeral)
    return;
  QString path = autostartPath();
  if (enabled) {
    QDir().mkpath(QFileInfo(path).absolutePath());
    QSaveFile f(path);
    if (!f.open(QIODevice::WriteOnly)) {
      emit message("Could not enable login startup. Check the autostart folder "
                   "permissions.");
      return;
    }
    QString binary = QCoreApplication::applicationFilePath();
    binary.replace('\\', "\\\\")
        .replace('"', "\\\"")
        .replace('$', "\\$")
        .replace('`', "\\`");
    QByteArray data =
        ("[Desktop Entry]\nType=Application\nName=Omarchy Clock\nExec=\"" +
         binary +
         "\" "
         "--background\nIcon=omarchy-clock\nTerminal=false\nX-GNOME-Autostart-"
         "enabled=true\n")
            .toUtf8();
    if (f.write(data) != data.size() || !f.commit()) {
      emit message("Could not save login startup settings.");
      return;
    }
  } else if (QFile::exists(path) && !QFile::remove(path)) {
    emit message("Could not disable login startup.");
    return;
  }
  emit changed();
}
void Desktop::testSound() {
  if (m_ephemeral)
    return;
  if (m_sound && m_sound->state() != QProcess::NotRunning)
    return;
  QString executable = QStandardPaths::findExecutable("pw-play");
  if (executable.isEmpty())
    executable = QStandardPaths::findExecutable("paplay");
  QString sound =
      "/usr/share/sounds/freedesktop/stereo/alarm-clock-elapsed.oga";
  if (executable.isEmpty() || !QFile::exists(sound)) {
    emit message("No sound player or alarm sound is installed.");
    return;
  }
  auto process = new QProcess(this);
  m_sound = process;
  connect(process, &QProcess::errorOccurred, this, [this, process] {
    emit message("Could not play the alert sound.");
    process->deleteLater();
  });
  connect(process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
          process, &QObject::deleteLater);
  process->start(executable, {sound});
}
void Desktop::testNotification() {
  notify("test", "", "Omarchy Clock", "Silent notifications are working.",
         false);
}
void Desktop::notify(QString kind, QString id, QString title, QString body,
                     bool sound) {
  if (m_ephemeral)
    return;
  if (sound)
    testSound();
  if (!m_engine->settings().value("notifications").toBool() && kind != "test")
    return;
  auto msg = QDBusMessage::createMethodCall(
      "org.freedesktop.Notifications", "/org/freedesktop/Notifications",
      "org.freedesktop.Notifications", "Notify");
  QStringList actions;
  if (kind == "alarm")
    actions = {"snooze",
               "Snooze " +
                   QString::number(
                       m_engine->settings().value("snoozeMinutes", 5).toInt()) +
                   "m",
               "dismiss", "Dismiss"};
  QVariantMap hints{{"suppress-sound", true},
                    {"desktop-entry", "omarchy-clock"},
                    {"urgency", uchar(1)}};
  msg.setArguments({QString("Omarchy Clock"), uint(0), QString("omarchy-clock"),
                    title, body, actions, hints, int(0)});
  auto watcher = new QDBusPendingCallWatcher(
      QDBusConnection::sessionBus().asyncCall(msg), this);
  connect(watcher, &QDBusPendingCallWatcher::finished, this,
          [this, watcher, kind, id] {
            QDBusPendingReply<uint> reply = *watcher;
            if (reply.isError())
              emit message("Desktop notification could not be delivered. The "
                           "clock's completed state is saved.");
            else {
              m_notifications[reply.value()] = {kind, id};
              emit notificationDelivered(reply.value());
              if (m_notifications.size() > 500)
                m_notifications.erase(m_notifications.begin());
            }
            watcher->deleteLater();
          });
}
void Desktop::notificationAction(uint id, QString action) {
  if (!m_notifications.contains(id))
    return;
  auto clock = m_notifications.value(id);
  if (clock.first == "alarm") {
    if (action == "snooze")
      m_engine->snoozeAlarm(clock.second);
    else if (action == "dismiss")
      m_engine->dismissAlarm(clock.second);
  }
  command("show");
  if (m_window && clock.first == "alarm")
    QMetaObject::invokeMethod(m_window, "go",
                              Q_ARG(QVariant, QVariant("Alarms")),
                              Q_ARG(QVariant, QVariant(false)));
}
void Desktop::openDataFolder() {
  if (!m_ephemeral)
    QDesktopServices::openUrl(
        QUrl::fromLocalFile(QFileInfo(m_engine->storagePath()).absolutePath()));
}
void Desktop::quit() {
  m_engine->save();
  qApp->quit();
}
QString Desktop::iconSource() const {
  return "data:image/svg+xml;base64," + QString::fromLatin1(
      clockIconSvg(m_theme->colors()["foreground"].value<QColor>()).toBase64());
}
void Desktop::applyPalette() {
  QPalette palette;
  auto c = m_theme->colors();
  palette.setColor(QPalette::Window, c["background"].value<QColor>());
  palette.setColor(QPalette::Base, c["surface"].value<QColor>());
  palette.setColor(QPalette::AlternateBase, c["hover"].value<QColor>());
  palette.setColor(QPalette::WindowText, c["foreground"].value<QColor>());
  palette.setColor(QPalette::Text, c["foreground"].value<QColor>());
  palette.setColor(QPalette::Button, c["surface"].value<QColor>());
  palette.setColor(QPalette::ButtonText, c["foreground"].value<QColor>());
  palette.setColor(QPalette::Highlight, c["accent"].value<QColor>());
  palette.setColor(QPalette::HighlightedText, c["onAccent"].value<QColor>());
  palette.setColor(QPalette::Mid, c["border"].value<QColor>());
  QApplication::setPalette(palette);
  const auto foreground = c["foreground"].value<QColor>();
  const auto icon = clockIcon(foreground);
  QApplication::setWindowIcon(icon);
  m_tray.setIcon(icon);
  if (!m_ephemeral && !updateLauncherIcon(foreground))
    qWarning("Could not update the Omarchy Clock launcher icon.");
  emit changed();
}
