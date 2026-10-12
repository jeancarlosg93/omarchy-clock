import QtQuick
import QtQuick.Controls
SpinBox {
    id: control
    editable: true
    implicitHeight: 40
    implicitWidth: 96
    font.pixelSize: 14
    up.indicator: Rectangle {
        x: control.width - width; width: 26; height: control.height
        color: control.up.pressed ? theme.colors.hover : "transparent"
        Text { anchors.centerIn: parent; text: "+"; color: control.up.enabled ? theme.colors.foreground : theme.colors.muted; font.pixelSize: 20 }
    }
    down.indicator: Rectangle {
        width: 26; height: control.height
        color: control.down.pressed ? theme.colors.hover : "transparent"
        Text { anchors.centerIn: parent; text: "−"; color: control.down.enabled ? theme.colors.foreground : theme.colors.muted; font.pixelSize: 20 }
    }
    background: Rectangle { radius: 6; color: theme.colors.surface; border.width: control.activeFocus ? 2 : 1; border.color: control.activeFocus ? theme.colors.accent : theme.colors.border }
}
