import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
ScrollView {
    id: page
    objectName: "trackingPage"
    property var appWindow
    property var live: engine.tracking
    property bool addingProject: false
    clip: true
    contentWidth: availableWidth
    function start() { engine.startTracking(activity.text, projectChoice.currentText || "General") }
    ColumnLayout {
        width: page.availableWidth
        spacing: 24
        PageTitle { Layout.fillWidth: true; title: "Time tracking"; subtitle: "Record the work, then see where your time went."; AppButton { iconName: "focus"; tip: "Focus display"; enabled: page.live.title.length > 0; onClicked: appWindow.enterFocus("tracking", "tracking") } }
        ColumnLayout {
            visible: page.live.title.length > 0
            Layout.fillWidth: true
            spacing: 10
            Text { text: page.live.title; color: theme.colors.foreground; font.pixelSize: 20; font.weight: Font.Medium; Layout.fillWidth: true; elide: Text.ElideRight }
            Text { text: page.live.project + " · " + page.live.status; color: theme.colors.muted; font.pixelSize: 13 }
            TimeFace { Layout.fillWidth: true; Layout.preferredHeight: 140; time: page.live.display }
            RowLayout { AppButton { objectName: "trackingPauseButton"; text: page.live.running ? "Pause" : "Resume"; primary: true; onClicked: engine.toggleTracking() } AppButton { objectName: "trackingFinishButton"; text: "Finish activity"; onClicked: engine.stopTracking() } }
            Text { text: "Each work segment is saved when you pause or finish. Breaks stay out of your totals."; color: theme.colors.muted; font.pixelSize: 12; Layout.fillWidth: true; wrapMode: Text.WordWrap }
        }
        ColumnLayout {
            visible: !page.live.running
            Layout.fillWidth: true
            spacing: 12
            Section { title: page.live.title.length ? "Start another activity" : "Start an activity"; Layout.fillWidth: true }
            Field { id: activity; objectName: "activityNameField"; Layout.fillWidth: true; placeholderText: "Activity, e.g. Writing or Client work"; maximumLength: 120; onAccepted: page.start(); Accessible.name: "Activity name" }
            RowLayout {
                Layout.fillWidth: true
                Choice { id: projectChoice; objectName: "projectChoice"; Layout.fillWidth: true; model: engine.projects; textRole: "title"; valueRole: "id"; Accessible.name: "Project" }
                AppButton { iconName: "plus"; tip: "Create project"; onClicked: page.addingProject = !page.addingProject }
            }
            RowLayout {
                visible: page.addingProject
                Layout.fillWidth: true
                Field { id: projectName; Layout.fillWidth: true; placeholderText: "New project name"; maximumLength: 120; Accessible.name: "New project name" }
                AppButton { text: "Add"; enabled: projectName.text.trim().length > 0; onClicked: { engine.addProject(projectName.text); projectChoice.currentIndex = engine.projects.findIndex(function(p) { return p.title.toLowerCase() === projectName.text.trim().toLowerCase() }); page.addingProject = false; projectName.text = "" } }
            }
            AppButton { objectName: "trackingStartButton"; text: "Start tracking"; primary: true; enabled: activity.text.trim().length > 0; onClicked: page.start() }
        }
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 16
            Section { title: "Last 7 days"; Layout.fillWidth: true }
            Text { text: engine.reports.today + " today · " + engine.reports.week + " in the last 7 days"; color: theme.colors.foreground; font.pixelSize: 15; Layout.fillWidth: true; wrapMode: Text.WordWrap }
            Text { visible: !engine.reports.weekMs; text: "Your report starts with your first tracked activity. No sample time is added to your records."; color: theme.colors.muted; font.pixelSize: 13; Layout.fillWidth: true; wrapMode: Text.WordWrap }
            RowLayout {
                visible: engine.reports.weekMs > 0
                Layout.fillWidth: true
                Layout.preferredHeight: 155
                spacing: 10
                Repeater {
                    model: engine.reports.days
                    ColumnLayout {
                        required property var modelData
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        spacing: 8
                        Item {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            Rectangle { anchors.bottom: parent.bottom; anchors.horizontalCenter: parent.horizontalCenter; width: Math.max(12, parent.width * 0.7); height: Math.max(2, parent.height * modelData.ratio); radius: 2; color: modelData.today ? theme.colors.accent : theme.colors.muted }
                            MouseArea { id: dayMouse; anchors.fill: parent; hoverEnabled: true }
                            ToolTip.visible: dayMouse.containsMouse
                            ToolTip.text: modelData.date + " · " + modelData.display
                            Accessible.name: modelData.date + " tracked " + modelData.display
                        }
                        Text { text: modelData.label; color: theme.colors.muted; font.pixelSize: 11; Layout.alignment: Qt.AlignHCenter }
                    }
                }
            }
        }
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 12
            Section { title: "Projects · last 7 days"; Layout.fillWidth: true }
            Text { visible: engine.reports.projects.length === 0; text: "Tracked projects and their totals appear here."; color: theme.colors.muted; font.pixelSize: 13; wrapMode: Text.WordWrap; Layout.fillWidth: true }
            Repeater {
                model: engine.reports.projects
                ColumnLayout {
                    required property var modelData
                    Layout.fillWidth: true
                    spacing: 8
                    RowLayout { Layout.fillWidth: true; Text { text: modelData.title; color: theme.colors.foreground; font.pixelSize: 14; Layout.fillWidth: true; elide: Text.ElideRight } Text { text: modelData.display; color: theme.colors.muted; font.pixelSize: 13 } }
                    Rectangle { height: 3; Layout.fillWidth: true; color: theme.colors.border; Rectangle { height: 3; width: parent.width * modelData.ratio; color: theme.colors.accent } }
                }
            }
        }
        Flow { Layout.fillWidth: true; spacing: 8; AppButton { text: "View history"; onClicked: appWindow.go("History") } AppButton { text: "Export tracking CSV"; onClicked: appWindow.exportHistory("tracking", "") } }
        Item { Layout.preferredHeight: 12 }
    }
}
