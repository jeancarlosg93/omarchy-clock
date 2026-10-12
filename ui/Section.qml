import QtQuick
import QtQuick.Layouts
ColumnLayout {
    property string title: ""
    spacing: 12
    Text { text: parent.title; color: theme.colors.foreground; font.pixelSize: 15; font.weight: Font.DemiBold; Layout.fillWidth: true }
    Rectangle { Layout.fillWidth: true; height: 1; color: theme.colors.border }
}
