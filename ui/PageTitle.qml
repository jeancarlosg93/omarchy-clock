import QtQuick
import QtQuick.Layouts
RowLayout {
    property string title: ""
    property string subtitle: ""
    spacing: 12
    ColumnLayout {
        Layout.fillWidth: true
        Layout.minimumWidth: 0
        spacing: 6
        Text { text: parent.parent.title; color: theme.colors.foreground; font.pixelSize: 24; font.weight: Font.DemiBold; Layout.fillWidth: true; elide: Text.ElideRight }
        Text { text: parent.parent.subtitle; color: theme.colors.muted; font.pixelSize: 13; Layout.fillWidth: true; wrapMode: Text.WordWrap; visible: text.length > 0 }
    }
}
