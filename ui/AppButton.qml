import QtQuick
import QtQuick.Controls
Button {
    id: control
    property bool primary: false
    property bool selected: false
    property string tip: ""
    property string iconName: ""
    property var c: theme.colors
    implicitHeight: 40
    implicitWidth: iconName && !text ? 40 : Math.max(64, label.implicitWidth + (iconName ? 48 : 28))
    font.pixelSize: 13
    opacity: enabled ? 1 : 0.4
    focusPolicy: Qt.StrongFocus
    contentItem: Item {
        implicitWidth: label.implicitWidth + (control.iconName ? 20 : 0)
        Icon { visible: control.iconName.length > 0; name: control.iconName; color: control.primary ? control.c.onAccent : control.c.foreground; anchors.verticalCenter: parent.verticalCenter; x: control.text ? 0 : (parent.width - width)/2 }
        Text { id: label; text: control.text; font: control.font; color: control.primary ? control.c.onAccent : control.c.foreground; anchors.centerIn: parent; anchors.horizontalCenterOffset: control.iconName && control.text ? 10 : 0; elide: Text.ElideRight }
    }
    background: Rectangle {
        radius: 6
        color: control.primary ? control.c.accent : control.selected || control.down ? control.c.hover : control.hovered ? control.c.surface : "transparent"
        border.width: control.activeFocus || control.selected ? 2 : 1
        border.color: control.activeFocus || control.selected ? control.c.accent : control.c.border
    }
    ToolTip.visible: hovered && tip.length > 0
    ToolTip.delay: 650
    ToolTip.text: tip
    Accessible.name: text || tip
}
