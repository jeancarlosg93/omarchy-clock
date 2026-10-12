import QtQuick
import QtQuick.Controls
TextField {
    id: control
    implicitHeight: 40
    font.pixelSize: 14
    color: theme.colors.foreground
    placeholderTextColor: theme.colors.muted
    selectionColor: theme.colors.accent
    selectedTextColor: theme.colors.onAccent
    selectByMouse: true
    leftPadding: 12
    rightPadding: 12
    background: Rectangle { radius: 6; color: theme.colors.surface; border.width: control.activeFocus ? 2 : 1; border.color: control.activeFocus ? theme.colors.accent : theme.colors.border }
}
