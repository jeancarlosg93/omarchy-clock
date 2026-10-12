import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
ScrollView {
    id: page
    objectName: "historyPage"
    property var appWindow
    property var results: []
    property int total: 0
    property int offset: 0
    property string kind: ""
    property var selectedSession: null
    clip: true
    contentWidth: availableWidth
    function refresh() { results = engine.searchHistory(search.text, kind, offset); total = engine.countHistory(search.text, kind); if (offset >= total && offset > 0) { offset = Math.max(0, Math.floor((total - 1) / 100) * 100); results = engine.searchHistory(search.text, kind, offset) } }
    Component.onCompleted: refresh()
    Connections { target: engine; function onHistoryChanged() { page.refresh() } }
    Timer { id: searchDelay; interval: 200; onTriggered: { page.offset = 0; page.refresh() } }
    ColumnLayout {
        width: page.availableWidth
        spacing: 22
        PageTitle { Layout.fillWidth: true; title: "History"; subtitle: "Your recorded time, kept on this computer."; AppButton { text: "Export all"; onClicked: appWindow.exportHistory(page.kind, "") } }
        RowLayout {
            Layout.fillWidth: true
            Field { id: search; objectName: "historySearchField"; Layout.fillWidth: true; placeholderText: "Search name or project"; onTextChanged: searchDelay.restart(); Accessible.name: "Search history" }
            Choice { objectName: "historyKindChoice"; Layout.preferredWidth: page.width < 500 ? 110 : 150; model: ["All types", "Timers", "Stopwatch", "Alarms", "Intervals", "Tracking"]; onActivated: { page.kind = ["", "timer", "stopwatch", "alarm", "interval", "tracking"][currentIndex]; page.offset = 0; page.refresh() } }
        }
        ColumnLayout {
            visible: !!page.selectedSession
            Layout.fillWidth: true
            spacing: 12
            Section { title: page.selectedSession ? page.selectedSession.title : "Session"; Layout.fillWidth: true }
            Text { text: page.selectedSession ? page.selectedSession.kind + " · " + page.selectedSession.date + (page.selectedSession.project ? " · " + page.selectedSession.project : "") : ""; color: theme.colors.muted; font.pixelSize: 13; Layout.fillWidth: true; wrapMode: Text.WordWrap }
            TimeFace { visible: page.selectedSession && page.selectedSession.kind !== "alarm"; Layout.fillWidth: true; Layout.preferredHeight: 120; time: page.selectedSession ? engine.format(page.selectedSession.duration) : "00:00"; fraction: page.selectedSession && page.selectedSession.kind === "stopwatch" ? engine.fraction(page.selectedSession.duration) : "" }
            Flow {
                Layout.fillWidth: true
                spacing: 8
                AppButton { objectName: "exportSessionButton"; text: "Export session"; onClicked: appWindow.exportHistory("", page.selectedSession.id) }
                AppButton { objectName: "deleteSessionButton"; text: "Delete"; onClicked: { let id = page.selectedSession.id; appWindow.confirm("Delete recorded session?", "This permanently removes this record and its lap data from your local history.", function() { if (engine.deleteSession(id)) page.selectedSession = null }) } }
                AppButton { text: "Close details"; onClicked: page.selectedSession = null }
            }
            Repeater {
                model: page.selectedSession ? page.selectedSession.laps : []
                RowLayout { required property var modelData; Layout.fillWidth: true; Text { text: "Lap " + modelData.number; color: theme.colors.muted; font.pixelSize: 12; Layout.fillWidth: true } Text { text: engine.format(modelData.split) + "." + engine.fraction(modelData.split); color: theme.colors.foreground; font.family: theme.fontFamily; font.pixelSize: 12 } Text { text: engine.format(modelData.total) + "." + engine.fraction(modelData.total); color: theme.colors.muted; font.family: theme.fontFamily; font.pixelSize: 12; Layout.leftMargin: 12 } }
            }
            Rectangle { Layout.fillWidth: true; height: 1; color: theme.colors.border }
        }
        Text { visible: page.results.length === 0; text: search.text || page.kind ? "No matching records. Try another name or type." : "No history yet. Save a stopwatch session, complete a timer, or track an activity."; color: theme.colors.muted; font.pixelSize: 14; wrapMode: Text.WordWrap; Layout.fillWidth: true }
        Repeater {
            model: page.results
            KeyboardRow {
                id: historyRow
                objectName: "historyRow-" + modelData.id
                Accessible.name: modelData.title + ", " + modelData.kind + ", " + modelData.date + ", " + modelData.durationText
                onActivated: page.selectedSession = modelData
                required property var modelData
                Layout.fillWidth: true
                implicitHeight: 74
                Rectangle { anchors.fill: parent; radius: 6; border.width: historyRow.activeFocus ? 2 : 0; border.color: theme.colors.accent; color: historyMouse.containsMouse || (page.selectedSession && page.selectedSession.id === modelData.id) ? theme.colors.surface : "transparent" }
                MouseArea { id: historyMouse; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: { historyRow.forceActiveFocus(); page.selectedSession = modelData } }
                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 10
                    spacing: 12
                    Icon { name: modelData.kind }
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 4
                        Text { text: modelData.title; color: theme.colors.foreground; font.pixelSize: 14; Layout.fillWidth: true; elide: Text.ElideRight }
                        Text { text: modelData.date + " · " + (modelData.project || modelData.kind); color: theme.colors.muted; font.pixelSize: 11; Layout.fillWidth: true; elide: Text.ElideRight }
                    }
                    Text { text: modelData.kind === "alarm" ? "Alarm" : modelData.durationText; color: theme.colors.muted; font.family: theme.fontFamily; font.pixelSize: 16 }
                }
            }
        }
        RowLayout {
            Layout.fillWidth: true
            visible: page.total > 0
            AppButton { text: "Previous"; enabled: page.offset > 0; onClicked: { page.offset = Math.max(0, page.offset - 100); page.refresh() } }
            Text { text: (page.offset + 1) + "–" + Math.min(page.total, page.offset + page.results.length) + " of " + page.total; color: theme.colors.muted; font.pixelSize: 11; Layout.fillWidth: true; horizontalAlignment: Text.AlignHCenter }
            AppButton { text: "Next"; enabled: page.offset + page.results.length < page.total; onClicked: { page.offset += 100; page.refresh() } }
        }
        Text { text: "CSV exports include exact millisecond durations and lap splits, with UTC timestamps. Export all follows the selected type and includes all matching records, beyond this page."; color: theme.colors.muted; font.pixelSize: 12; wrapMode: Text.WordWrap; Layout.fillWidth: true }
        Item { Layout.preferredHeight: 12 }
    }
}
