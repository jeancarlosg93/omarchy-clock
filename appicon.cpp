#include "appicon.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QPainter>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStandardPaths>
#include <QSvgRenderer>

QByteArray clockIconSvg(const QColor &foreground) {
  QFile source(":/omarchy-clock.svg");
  if (!source.open(QIODevice::ReadOnly)) return {};
  return source.readAll().replace("currentColor", foreground.name().toUtf8());
}

QIcon clockIcon(const QColor &foreground) {
  QSvgRenderer renderer(clockIconSvg(foreground));
  QIcon icon;
  for (int size : {16, 22, 24, 32, 48, 64, 128, 256}) {
    QPixmap image(size, size);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    renderer.render(&painter);
    painter.end();
    icon.addPixmap(image);
  }
  return icon;
}

static bool saveChanged(const QString &path, const QByteArray &bytes) {
  QFile existing(path);
  if (existing.open(QIODevice::ReadOnly) && existing.readAll() == bytes) return true;
  if (!QDir().mkpath(QFileInfo(path).absolutePath())) return false;
  QSaveFile file(path);
  return file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size() && file.commit();
}

bool updateLauncherIcon(const QColor &foreground) {
  const QString data = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
  const QString desktop = data + "/applications/omarchy-clock.desktop";
  // Source builds and capture fixtures must not install desktop integration.
  if (!QFile::exists(desktop)) return true;
  const QString name = "omarchy-clock-" + foreground.name().mid(1);
  if (!saveChanged(data + "/icons/hicolor/scalable/apps/" + name + ".svg",
                   clockIconSvg(foreground))) return false;
  const QString startup = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation)
                          + "/autostart/omarchy-clock.desktop";
  for (const auto &path : {desktop, startup}) {
    QFile entry(path);
    if (!entry.exists()) continue;
    if (!entry.open(QIODevice::ReadOnly)) return false;
    QString content = QString::fromUtf8(entry.readAll());
    const auto line = QRegularExpression("^Icon=[^\\r\\n]*", QRegularExpression::MultilineOption).match(content);
    if (!line.hasMatch()) return false;
    content.replace(line.capturedStart(), line.capturedLength(), "Icon=" + name);
    if (!saveChanged(path, content.toUtf8())) return false;
  }
  return true;
}
