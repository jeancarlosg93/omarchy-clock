import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
ScrollView {
    id: page
    objectName: "todayPage"
    property var appWindow
    property var local: { let r = engine.revision; return engine.clockDetails("world", "") }
    property var nextAlarms: engine.alarms.filter(function(a) { return a.enabled || a.snoozeUntil > 0 || a.ringing }).sort(function(a,b) { return (a.snoozeUntil || a.next) - (b.snoozeUntil || b.next) })
    clip: true
    contentWidth: availableWidth
    ColumnLayout {
        width: page.availableWidth
        spacing: 26
        PageTitle { Layout.fillWidth: true; title: "Today"; subtitle: page.local.date + " · " + page.local.abbreviation }
        RowLayout {
            Layout.fillWidth: true
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 8
                Text { text: page.local.display; color: theme.colors.foreground; font.family: theme.fontFamily; font.pixelSize: page.width < 500 ? 38 : 54; Layout.fillWidth: true; elide: Text.ElideRight }
                Text { text: engine.reports.today + " tracked today · " + engine.reports.completed + " recorded events"; color: theme.colors.muted; font.pixelSize: 13; wrapMode: Text.WordWrap; Layout.fillWidth: true }
            }
            AppButton { iconName: "world"; selected: appWindow.focusKind === "world" && appWindow.focusId === ""; tip: "Select local clock, then use Focus in the header"; onClicked: appWindow.setFocusClock("world", "") }
        }
        Flow {
            Layout.fillWidth: true
            spacing: 8
            AppButton { text: "New timer"; iconName: "timer"; onClicked: appWindow.go("Timers", true) }
            AppButton { text: "Stopwatch"; iconName: "stopwatch"; onClicked: appWindow.go("Stopwatch") }
            AppButton { text: "Track time"; iconName: "tracking"; onClicked: appWindow.go("Tracking") }
        }
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 8
            Section { title: "Running now"; Layout.fillWidth: true; AppButton { visible: engine.activeClocks.length > 0; text: "Pause all"; onClicked: engine.pauseAll() } }
            Text { visible: engine.activeClocks.length === 0; text: "Your day has room. Start a timer, record a session, or track an activity."; color: theme.colors.muted; font.pixelSize: 13; wrapMode: Text.WordWrap; Layout.fillWidth: true }
            Repeater {
                model: engine.activeClocks
                ClockRow { required property var modelData; Layout.fillWidth: true; kind: modelData.kind; clockId: modelData.id; onChosen: appWindow.openClock(kind, clockId) }
            }
        }
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 12
            Section { title: "Upcoming alarms"; Layout.fillWidth: true }
            Text { visible: page.nextAlarms.length === 0; text: "No alarms scheduled. Set a one-time reminder or a recurring routine."; color: theme.colors.muted; font.pixelSize: 13; wrapMode: Text.WordWrap; Layout.fillWidth: true }
            Repeater {
                model: page.nextAlarms.slice(0, 4)
                RowLayout {
                    required property var modelData
                    Layout.fillWidth: true
                    Layout.preferredHeight: 56
                    spacing: 12
                    Icon { name: "alarm" }
                    ColumnLayout {
                        Layout.fillWidth: true
                        Text { text: modelData.title; color: theme.colors.foreground; font.pixelSize: 14; Layout.fillWidth: true; elide: Text.ElideRight }
                        Text { text: modelData.ringing ? "Time is up" : modelData.nextText; color: theme.colors.muted; font.pixelSize: 12; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                    }
                    AppButton { text: modelData.ringing ? "Snooze" : "View"; onClicked: { if (modelData.ringing) engine.snoozeAlarm(modelData.id); else appWindow.go("Alarms") } }
                }
            }
            AppButton { text: "Manage alarms"; onClicked: appWindow.go("Alarms") }
        }
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 12
            Section { title: "Recently recorded"; Layout.fillWidth: true }
            Text { visible: engine.sessions.length === 0; text: "Completed timers, alarms, saved stopwatches, and tracked activities appear here."; color: theme.colors.muted; font.pixelSize: 13; wrapMode: Text.WordWrap; Layout.fillWidth: true }
            Repeater {
                model: engine.sessions.slice(0, 3)
                RowLayout {
                    required property var modelData
                    Layout.fillWidth: true
                    Layout.preferredHeight: 54
                    ColumnLayout { Layout.fillWidth: true; Text { text: modelData.title; color: theme.colors.foreground; font.pixelSize: 14; Layout.fillWidth: true; elide: Text.ElideRight } Text { text: modelData.kind + " · " + modelData.date; color: theme.colors.muted; font.pixelSize: 11 } }
                    Text { text: modelData.kind === "alarm" ? "Alarm" : modelData.durationText; font.family: theme.fontFamily; color: theme.colors.muted; font.pixelSize: 14 }
                }
            }
            AppButton { text: "Open history"; onClicked: appWindow.go("History") }
        }
        Item { Layout.preferredHeight: 12 }
    }
}
