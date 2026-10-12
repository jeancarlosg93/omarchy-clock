import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
ScrollView {
    id: page
    objectName: "alarmsPage"
    property var appWindow
    property bool composing: false
    property string editingId: ""
    property int daysMask: 0
    clip: true
    contentWidth: availableWidth
    function compose(id) {
        editingId = id || ""
        let a = id ? engine.alarms.filter(function(v) { return v.id === id })[0] : null
        nameField.text = a ? a.title : ""
        timeField.text = a ? a.time : "08:00"
        let tomorrow = new Date(); tomorrow.setDate(tomorrow.getDate() + 1)
        dateField.text = a ? a.date : Qt.formatDate(tomorrow, "yyyy-MM-dd")
        daysMask = a ? a.days : 0
        schedule.currentIndex = daysMask === 0 ? 0 : daysMask === 127 ? 1 : daysMask === 31 ? 2 : daysMask === 96 ? 3 : 4
        zone.zone = a ? a.zone : engine.favoriteZones[0] || "UTC"
        soundBox.checked = a ? a.sound : false
        composing = true
        nameField.forceActiveFocus()
    }
    function saveForm() {
        let ok = editingId ? engine.editAlarm(editingId, nameField.text, timeField.text, dateField.text, daysMask, zone.zone, soundBox.checked) : engine.addAlarm(nameField.text, timeField.text, dateField.text, daysMask, zone.zone, soundBox.checked).length > 0
        if (ok) composing = false
    }
    ColumnLayout {
        width: page.availableWidth
        spacing: 22
        PageTitle { Layout.fillWidth: true; title: "Alarms"; subtitle: "One-time reminders and recurring routines."; AppButton { objectName: "addAlarmButton"; text: "New alarm"; iconName: "plus"; primary: true; onClicked: page.compose("") } }
        ColumnLayout {
            visible: page.composing
            Layout.fillWidth: true
            spacing: 12
            Section { Layout.fillWidth: true; title: page.editingId ? "Edit alarm" : "New alarm" }
            Field { id: nameField; objectName: "alarmNameField"; Layout.fillWidth: true; placeholderText: "Name, e.g. Morning routine"; maximumLength: 120; Accessible.name: "Alarm name" }
            RowLayout {
                Layout.fillWidth: true
                ColumnLayout { Layout.fillWidth: true; Text { text: "Time · HH:mm"; font.pixelSize: 12; color: theme.colors.muted } Field { id: timeField; objectName: "alarmTimeField"; Layout.fillWidth: true; maximumLength: 5; validator: RegularExpressionValidator { regularExpression: /[0-2][0-9]:[0-5][0-9]/ } Accessible.name: "Alarm time" } }
                ColumnLayout { Layout.fillWidth: true; Text { text: "Repeat"; font.pixelSize: 12; color: theme.colors.muted } Choice { id: schedule; Layout.fillWidth: true; model: ["Once", "Every day", "Weekdays", "Weekends", "Custom days"]; onActivated: page.daysMask = currentIndex === 0 ? 0 : currentIndex === 1 ? 127 : currentIndex === 2 ? 31 : currentIndex === 3 ? 96 : page.daysMask || 127 } }
            }
            Field { id: dateField; objectName: "alarmDateField"; visible: schedule.currentIndex === 0; Layout.fillWidth: true; placeholderText: "Date · YYYY-MM-DD"; maximumLength: 10; Accessible.name: "Alarm date, year month day" }
            Flow {
                visible: schedule.currentIndex === 4
                Layout.fillWidth: true
                spacing: 4
                Repeater {
                    model: ["Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"]
                    CheckBox { required property string modelData; required property int index; text: modelData; checked: !!(page.daysMask & (1 << index)); onToggled: page.daysMask = checked ? page.daysMask | (1 << index) : page.daysMask & ~(1 << index) }
                }
            }
            ZonePicker { id: zone; Layout.fillWidth: true }
            CheckBox { id: soundBox; text: "Play sound for this alarm" }
            Text { text: "Silent by default. During a daylight-saving gap, an alarm advances to the next valid time; a repeated hour rings once."; color: theme.colors.muted; font.pixelSize: 12; wrapMode: Text.WordWrap; Layout.fillWidth: true }
            RowLayout { spacing: 8; AppButton { objectName: "saveAlarmButton"; text: page.editingId ? "Save changes" : "Schedule alarm"; primary: true; enabled: timeField.acceptableInput && (schedule.currentIndex === 0 || page.daysMask > 0); onClicked: page.saveForm() } AppButton { text: "Cancel"; onClicked: page.composing = false } }
            Rectangle { Layout.fillWidth: true; height: 1; color: theme.colors.border }
        }
        Text { visible: engine.alarms.length === 0; text: "No alarms yet. Schedule a reminder once, or choose the days it should repeat."; color: theme.colors.muted; font.pixelSize: 14; wrapMode: Text.WordWrap; Layout.fillWidth: true }
        Repeater {
            model: engine.alarms
            ColumnLayout {
                required property var modelData
                Layout.fillWidth: true
                spacing: 12
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 4
                        Text { text: modelData.time; color: theme.colors.foreground; font.family: theme.fontFamily; font.pixelSize: 36 }
                        Text { text: modelData.title; color: theme.colors.foreground; font.pixelSize: 16; font.weight: Font.Medium; Layout.fillWidth: true; elide: Text.ElideRight }
                        Text { text: modelData.schedule + " · " + modelData.zone; color: theme.colors.muted; font.pixelSize: 12; Layout.fillWidth: true; wrapMode: Text.WordWrap }
                        Text { text: modelData.status + (modelData.enabled || modelData.snoozeUntil > 0 ? " · " + modelData.nextText : ""); color: theme.colors.muted; font.pixelSize: 12; Layout.fillWidth: true; wrapMode: Text.WordWrap }
                    }
                    AppSwitch { checked: modelData.enabled; Accessible.name: "Enable " + modelData.title; onToggled: { engine.enableAlarm(modelData.id, checked); let a = engine.alarms.filter(function(v) { return v.id === modelData.id })[0]; checked = a ? a.enabled : false } }
                }
                Flow {
                    Layout.fillWidth: true
                    spacing: 8
                    AppButton { visible: modelData.ringing; text: "Snooze " + engine.settings.snoozeMinutes + "m"; primary: true; onClicked: engine.snoozeAlarm(modelData.id) }
                    AppButton { visible: modelData.ringing || modelData.snoozeUntil > 0; text: "Dismiss"; onClicked: engine.dismissAlarm(modelData.id) }
                    AppButton { text: "Edit"; onClicked: page.compose(modelData.id) }
                    AppButton { text: "Delete"; onClicked: { let id = modelData.id; appWindow.confirm("Delete alarm?", "This removes the saved alarm and its future schedule.", function() { engine.removeAlarm(id) }) } }
                }
                Rectangle { Layout.fillWidth: true; height: 1; color: theme.colors.border }
            }
        }
        Text { text: "Alarms continue while the window is hidden in the tray. Sleep delays delivery until the computer wakes."; color: theme.colors.muted; font.pixelSize: 12; wrapMode: Text.WordWrap; Layout.fillWidth: true }
        Item { Layout.preferredHeight: 12 }
    }
}
