import QtQuick
import QtQuick.Layouts
KeyboardRow {
    id: row
    objectName: "clockRow-" + kind + "-" + clockId
    Accessible.name: (live.title || "Clock") + ", " + kind + ", " + (live.status || "") + ", " + (live.display || "")
    onActivated: row.chosen()
    property string kind: "timer"
    property string clockId: ""
    property bool selected: false
    property bool controls: true
    property var live: { let r = engine.revision; return engine.clockDetails(kind, clockId) }
    signal chosen()
    implicitHeight: 82
    Rectangle { anchors.fill: parent; color: row.selected ? theme.colors.surface : mouse.containsMouse ? theme.colors.surface : "transparent"; radius: 6; border.width: row.activeFocus ? 2 : 0; border.color: theme.colors.accent }
    MouseArea { id: mouse; anchors.fill: parent; hoverEnabled: true; onClicked: { row.forceActiveFocus(); row.chosen() } cursorShape: Qt.PointingHandCursor }
    RowLayout {
        anchors.fill: parent
        anchors.margins: 12
        spacing: 12
        Icon { name: row.kind; Layout.alignment: Qt.AlignVCenter }
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 4
            Text { text: row.live.title || "Clock"; color: theme.colors.foreground; font.pixelSize: 14; font.weight: Font.Medium; Layout.fillWidth: true; elide: Text.ElideRight }
            Text { text: row.kind === "interval" ? (row.live.phaseName || "") + " · " + row.live.status : row.live.status || ""; color: theme.colors.muted; font.pixelSize: 11; Layout.fillWidth: true; elide: Text.ElideRight }
        }
        Text { text: row.live.display || "00:00"; font.family: theme.fontFamily; font.pixelSize: row.width < 500 ? 19 : 24; color: theme.colors.foreground }
        AppButton {
            visible: row.controls
            text: row.live.running ? "Pause" : "Start"
            implicitWidth: 66
            onClicked: {
                if (row.kind === "timer") engine.toggleTimer(row.clockId)
                else if (row.kind === "stopwatch") engine.toggleStopwatch()
                else if (row.kind === "interval") engine.toggleInterval(row.clockId)
                else if (row.kind === "tracking") engine.toggleTracking()
            }
        }
    }
}
