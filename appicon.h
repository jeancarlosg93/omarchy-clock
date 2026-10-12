#pragma once
#include <QColor>
#include <QIcon>
#include <QString>

QByteArray clockIconSvg(const QColor &foreground);
QIcon clockIcon(const QColor &foreground);
bool updateLauncherIcon(const QColor &foreground);
