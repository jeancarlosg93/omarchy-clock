#include "clockmodel.h"
#include "desktop.h"
#include "timekeeper.h"
#include <QApplication>
#include <QDebug>
#include <QFileInfo>
#include <QImage>
#include <QLocalServer>
#include <QLocalSocket>
#include <QLockFile>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QSettings>
#include <QStandardPaths>
#include <cstdio>
#include <sys/stat.h>

int main(int argc, char **argv) {
  umask(0077);
  QApplication app(argc, argv);
  app.setApplicationName("Omarchy Clock");
  app.setOrganizationName("OmarchyClock");
  app.setApplicationVersion("2.1.4");
  app.setDesktopFileName("omarchy-clock");
  app.setWindowIcon(QIcon(":/omarchy-clock.svg"));
  app.setQuitOnLastWindowClosed(false);
  QQuickStyle::setStyle("Basic");
  qInstallMessageHandler(
      [](QtMsgType type, const QMessageLogContext &, const QString &message) {
        if (type != QtDebugMsg)
          fprintf(stderr, "%s\n", qPrintable(message));
      });
  auto args = app.arguments();
  bool capture = args.contains("--capture"),
       testAlert = args.contains("--test-notification"),
       testBar = args.contains("--test-bar"),
       desktopTest = capture || testAlert, ephemeral = desktopTest || testBar;
  QString command = args.contains("--toggle")          ? "toggle"
                    : args.contains("--bar-open")      ? "bar-open"
                    : args.contains("--bar-pause")     ? "bar-pause"
                    : args.contains("--bar-compact")   ? "bar-compact"
                    : args.contains("--pause-focused") ? "pause"
                    : args.contains("--compact")       ? "compact"
                    : args.contains("--save-and-quit") ? "save-and-quit"
                    : args.contains("--background")    ? "background"
                                                       : "show";
  QString runtime =
              QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation),
          socketPath = runtime + "/omarchy-clock.sock";
  QLockFile lock(runtime + "/omarchy-clock.lock");
  QLocalServer server;
  if (!desktopTest) {
    QLocalSocket peer;
    peer.connectToServer(socketPath);
    if (peer.waitForConnected(300)) {
      peer.write(command.toUtf8() + "\n");
      peer.waitForBytesWritten(1000);
      if (peer.waitForReadyRead(2500) && peer.readLine().trimmed() == "ok")
        return 0;
      qWarning() << "The running clock did not acknowledge the command.";
      return 2;
    }
    if (command == "save-and-quit")
      return 0;
    if (!lock.tryLock(0)) {
      qCritical() << "Omarchy Clock is already starting. Try opening it again "
                     "in a moment.";
      return 2;
    }
    QLocalServer::removeServer(socketPath);
    server.setSocketOptions(QLocalServer::UserAccessOption);
    if (!server.listen(socketPath)) {
      qCritical() << "Could not create the local app control socket.";
      return 2;
    }
  }
  Timekeeper keeper(QString(), ephemeral);
  Theme theme;
  Desktop desktop(&keeper, &theme, desktopTest);
  QQmlApplicationEngine engine;
  engine.rootContext()->setContextProperty("engine", &keeper);
  engine.rootContext()->setContextProperty("theme", &theme);
  engine.rootContext()->setContextProperty("shell", &desktop);
  if ((capture && args.contains("--demo")) || testBar)
    keeper.populateDemo();
  QObject::connect(
      &engine, &QQmlApplicationEngine::objectCreationFailed, &app,
      [] { QCoreApplication::exit(1); }, Qt::QueuedConnection);
  engine.load(QUrl("qrc:/Main.qml"));
  if (engine.rootObjects().isEmpty())
    return 1;
  auto window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
  desktop.setWindow(window);
  QSettings settings;
  QSize normalSize(
      qBound(380, settings.value("suiteWidth", 1150).toInt(), 2400),
      qBound(460, settings.value("suiteHeight", 780).toInt(), 1800));
  if (!ephemeral) {
    window->resize(normalSize);
    window->setProperty("timerId",
                        settings.value("timerId", window->property("timerId")));
    window->setProperty(
        "intervalId",
        settings.value("intervalId", window->property("intervalId")));
    window->setProperty("focusKind", settings.value("focusKind", "timer"));
    window->setProperty("focusId",
                        settings.value("focusId", window->property("timerId")));
    QMetaObject::invokeMethod(window, "ensureSelections");
  }
  auto argument = [&](QString flag, QString fallback = QString()) {
    int i = args.indexOf(flag);
    return i >= 0 && i + 1 < args.size() ? args[i + 1] : fallback;
  };
  if (args.contains("--size")) {
    auto size = argument("--size").split('x');
    if (size.size() == 2)
      window->resize(size[0].toInt(), size[1].toInt());
  }
  QString page =
      argument("--page", args.contains("--stopwatch") ? "Stopwatch" : "");
  if (!page.isEmpty())
    QMetaObject::invokeMethod(window, "go", Q_ARG(QVariant, QVariant(page)),
                              Q_ARG(QVariant, QVariant(false)));
  if (args.contains("--focus")) {
    QString kind = argument("--focus");
    QString id = kind == "timer" ? window->property("timerId").toString()
                 : kind == "interval"
                     ? window->property("intervalId").toString()
                     : kind;
    QMetaObject::invokeMethod(window, "enterFocus",
                              Q_ARG(QVariant, QVariant(kind)),
                              Q_ARG(QVariant, QVariant(id)));
  }
  if (args.contains("--fullscreen"))
    window->showFullScreen();
  if (args.contains("--background"))
    window->hide();
  QObject::connect(window, &QWindow::widthChanged, &app, [&] {
    if (window->visibility() == QWindow::Windowed)
      normalSize.setWidth(window->width());
  });
  QObject::connect(window, &QWindow::heightChanged, &app, [&] {
    if (window->visibility() == QWindow::Windowed)
      normalSize.setHeight(window->height());
  });
  QObject::connect(&server, &QLocalServer::newConnection, &app, [&] {
    while (server.hasPendingConnections()) {
      auto peer = server.nextPendingConnection();
      auto receive = [&, peer] {
        if (!peer->canReadLine())
          return;
        QString request = QString::fromUtf8(peer->readLine()).trimmed();
        desktop.command(request);
        peer->write("ok\n");
        peer->flush();
        peer->disconnectFromServer();
      };
      QObject::connect(peer, &QLocalSocket::readyRead, &app, receive);
      QObject::connect(peer, &QLocalSocket::disconnected, peer,
                       &QObject::deleteLater);
      // A fast local client may have sent bytes before readyRead was connected.
      receive();
    }
  });
  QObject::connect(&desktop, &Desktop::message, &app, [&](QString message) {
    window->setProperty("notificationText", message);
    qWarning().noquote() << message;
  });
  if (capture && args.contains("--capture-compact"))
    QMetaObject::invokeMethod(window, "toggleCompact");
  if (capture && args.contains("--compose")) {
    auto loader = window->findChild<QObject *>("pageLoader");
    auto item = qvariant_cast<QObject *>(loader->property("item"));
    if (item)
      QMetaObject::invokeMethod(item, "compose", Q_ARG(QVariant, QVariant("")));
  }
  if (capture && args.contains("--map-zone")) {
    auto picker = window->findChild<QObject *>("worldZonePicker");
    if (picker)
      QMetaObject::invokeMethod(
          picker, "chooseZone",
          Q_ARG(QVariant, QVariant(argument("--map-zone"))));
  }
  if (capture && args.contains("--map-zoom")) {
    auto map = window->findChild<QObject *>("worldMap");
    if (map) {
      QMetaObject::invokeMethod(
          map, "setZoom",
          Q_ARG(QVariant, QVariant(argument("--map-zoom").toDouble())));
      auto pan = argument("--map-pan", "0,0").split(',');
      if (pan.size() == 2)
        QMetaObject::invokeMethod(map, "panTo",
                                  Q_ARG(QVariant, QVariant(pan[0].toDouble())),
                                  Q_ARG(QVariant, QVariant(pan[1].toDouble())));
    }
  }
  if (capture && args.contains("--open-choice")) {
    QTimer::singleShot(100, &app, [&] {
      auto choice = window->findChild<QObject *>(argument("--open-choice"));
      auto popup = choice ? qvariant_cast<QObject *>(choice->property("popup"))
                          : nullptr;
      if (popup)
        QMetaObject::invokeMethod(popup, "open");
    });
  }
  if (capture) {
    QTimer::singleShot(400, &app, [&] {
      auto target = args.contains("--capture-compact")
                        ? window->findChild<QQuickWindow *>("compactWindow")
                        : window;
      bool ok = target && target->grabWindow().save(argument("--capture"));
      app.exit(ok ? 0 : 2);
    });
  }
  if (testAlert) {
    QObject::connect(&desktop, &Desktop::notificationDelivered, &app,
                     [&](uint id) {
                       qInfo() << "Silent notification delivered:" << id;
                       app.exit(0);
                     });
    QTimer::singleShot(200, &desktop, &Desktop::testNotification);
    QTimer::singleShot(5000, &app, [&] { app.exit(3); });
  }
  if (!desktopTest)
    QTimer::singleShot(0, &app, [&] { desktop.command(command); });
  QObject::connect(&app, &QCoreApplication::aboutToQuit, &app, [&] {
    if (ephemeral)
      return;
    keeper.save();
    settings.setValue("suiteWidth", normalSize.width());
    settings.setValue("suiteHeight", normalSize.height());
    for (const QString &key :
         QStringList{"timerId", "intervalId", "focusKind", "focusId"})
      settings.setValue(key, window->property(key.toUtf8().constData()));
  });
  return app.exec();
}
