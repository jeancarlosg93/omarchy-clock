import QtQuick
import QtQuick.Controls
Switch {
    id: control
    font.pixelSize: 14
    indicator: Rectangle {
        implicitWidth: 46
        implicitHeight: 24
        x: control.leftPadding
        y: (control.height - height) / 2
        radius: 12
        color: control.checked ? theme.colors.accent : theme.colors.border
        Rectangle {
            x: control.checked ? parent.width - width - 3 : 3
            y: 3
            width: 18
            height: 18
            radius: 9
            color: control.checked ? theme.colors.onAccent : theme.colors.foreground
        }
        border.width: control.activeFocus ? 2 : 0
        border.color: theme.colors.foreground
    }
    contentItem: Text {
        text: control.text
        font: control.font
        color: theme.colors.foreground
        verticalAlignment: Text.AlignVCenter
        leftPadding: control.indicator.width + control.spacing
        wrapMode: Text.WordWrap
    }
}
