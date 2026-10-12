import QtQuick
import QtQuick.Layouts
RowLayout {
    id: root
    property int seconds: 1500
    spacing: 8
    function update() { seconds = hours.value * 3600 + minutes.value * 60 + secs.value }
    onSecondsChanged: { hours.value = Math.floor(seconds / 3600); minutes.value = Math.floor(seconds / 60 % 60); secs.value = seconds % 60 }
    Component.onCompleted: { hours.value = Math.floor(seconds / 3600); minutes.value = Math.floor(seconds / 60 % 60); secs.value = seconds % 60 }
    ColumnLayout {
        spacing: 4
        Number { id: hours; objectName: "durationHours"; from: 0; to: 99; onValueModified: root.update(); Accessible.name: "Hours" }
        Text { text: "Hours"; color: theme.colors.muted; font.pixelSize: 11; Layout.alignment: Qt.AlignHCenter }
    }
    ColumnLayout {
        spacing: 4
        Number { id: minutes; objectName: "durationMinutes"; from: 0; to: 59; onValueModified: root.update(); Accessible.name: "Minutes" }
        Text { text: "Minutes"; color: theme.colors.muted; font.pixelSize: 11; Layout.alignment: Qt.AlignHCenter }
    }
    ColumnLayout {
        spacing: 4
        Number { id: secs; objectName: "durationSeconds"; from: 0; to: 59; onValueModified: root.update(); Accessible.name: "Seconds" }
        Text { text: "Seconds"; color: theme.colors.muted; font.pixelSize: 11; Layout.alignment: Qt.AlignHCenter }
    }
}
