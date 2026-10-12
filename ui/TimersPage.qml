import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
ScrollView {
    id: page
    objectName: "timersPage"
    property var appWindow
    property bool composing: false
    property string editingId: ""
    property var live: { let r = engine.revision; return engine.timer(appWindow ? appWindow.timerId : "") }
    clip: true
    contentWidth: availableWidth
    function compose(id) {
        editingId = id || ""
        let value = id ? engine.timer(id) : ({title: "", duration: 1500000, repeat: false, sound: false})
        nameField.text = id ? value.title : ""
        duration.seconds = Math.floor(value.duration / 1000)
        repeatBox.checked = !!value.repeat
        soundBox.checked = !!value.sound
        presetBox.checked = false
        composing = true
        nameField.forceActiveFocus()
    }
    function saveForm() {
        let id = editingId
        if (id) { if (!engine.editTimer(id, nameField.text, duration.seconds, repeatBox.checked, soundBox.checked)) return }
        else { id = engine.addTimer(nameField.text, duration.seconds, repeatBox.checked, soundBox.checked); if (!id) return }
        if (presetBox.checked) engine.savePreset(nameField.text || "Timer", duration.seconds, repeatBox.checked)
        appWindow.timerId = id
        appWindow.setFocusClock("timer", id)
        composing = false
    }
    ColumnLayout {
        width: page.availableWidth
        spacing: 22
        PageTitle {
            Layout.fillWidth: true
            title: "Timers"
            subtitle: "Independent countdowns for whatever comes next."
            AppButton { objectName: "addTimerButton"; text: "New timer"; primary: true; iconName: "plus"; onClicked: page.compose("") }
        }
        ColumnLayout {
            visible: page.composing
            Layout.fillWidth: true
            spacing: 12
            Section { Layout.fillWidth: true; title: page.editingId ? "Edit timer" : "New timer" }
            Field { id: nameField; objectName: "timerNameField"; Layout.fillWidth: true; placeholderText: "Name, e.g. Tea or Deep work"; maximumLength: 120; onAccepted: page.saveForm(); Accessible.name: "Timer name" }
            Duration { id: duration; objectName: "timerDuration" }
            Flow {
                Layout.fillWidth: true
                spacing: 16
                CheckBox { id: repeatBox; objectName: "timerRepeatBox"; text: "Repeat" }
                CheckBox { id: soundBox; text: "Play sound" }
                CheckBox { id: presetBox; text: "Save as preset" }
            }
            RowLayout { spacing: 8; AppButton { objectName: "saveTimerButton"; text: page.editingId ? "Save changes" : "Create timer"; primary: true; enabled: duration.seconds > 0; onClicked: page.saveForm() } AppButton { text: "Cancel"; onClicked: page.composing = false } }
            Rectangle { Layout.fillWidth: true; height: 1; color: theme.colors.border }
        }
        ColumnLayout {
            Layout.fillWidth: true
            visible: !!page.live.id
            spacing: 10
            Text { text: page.live.title || ""; color: theme.colors.foreground; font.pixelSize: 18; font.weight: Font.Medium; Layout.alignment: Qt.AlignHCenter; Layout.maximumWidth: page.availableWidth; elide: Text.ElideRight }
            TimeFace { Layout.fillWidth: true; Layout.preferredHeight: page.height > 650 ? 190 : 140; time: page.live.display || "00:00" }
            Text { text: (page.live.status || "Ready") + (page.live.repeat ? " · Repeating" : "") + (page.live.cycles > 0 ? " · " + page.live.cycles + " completed" : ""); color: theme.colors.muted; font.pixelSize: 12; Layout.alignment: Qt.AlignHCenter }
            Rectangle { Layout.fillWidth: true; height: 3; color: theme.colors.border; Rectangle { height: 3; width: parent.width * Math.max(0, Math.min(1, page.live.progress || 0)); color: theme.colors.accent } }
            Flow {
                Layout.fillWidth: true
                spacing: 8
                AppButton { objectName: "timerStartButton"; text: page.live.running ? "Pause" : page.live.status === "Paused" ? "Resume" : page.live.finished ? "Start again" : "Start timer"; primary: true; onClicked: engine.toggleTimer(page.live.id); tip: "Space · Start or pause" }
                AppButton { text: "+1 min"; onClicked: engine.addTime(page.live.id, 60) }
                AppButton { text: "+5 min"; onClicked: engine.addTime(page.live.id, 300) }
                AppButton { text: "Reset"; onClicked: engine.resetTimer(page.live.id) }
                AppButton { objectName: "editTimerButton"; text: "Edit"; enabled: !page.live.running; onClicked: page.compose(page.live.id) }
                AppButton { iconName: "focus"; tip: "Focus display"; onClicked: appWindow.enterFocus("timer", page.live.id) }
                AppButton { iconName: "trash"; tip: "Delete timer"; enabled: !page.live.running; onClicked: { let id = page.live.id; appWindow.confirm("Delete timer?", "This removes the saved timer. Recorded sessions stay in History.", function() { engine.removeTimer(id); appWindow.ensureSelections() }) } }
            }
        }
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 10
            Section { title: "Your timers"; Layout.fillWidth: true }
            Text { visible: engine.timers.length === 0; text: "Create a named timer, or choose a preset below."; color: theme.colors.muted; font.pixelSize: 13; wrapMode: Text.WordWrap; Layout.fillWidth: true }
            Repeater {
                model: engine.timers
                ClockRow {
                    required property var modelData
                    Layout.fillWidth: true
                    kind: "timer"
                    clockId: modelData.id
                    selected: appWindow && appWindow.timerId === clockId
                    onChosen: { appWindow.timerId = clockId; appWindow.setFocusClock("timer", clockId) }
                }
            }
        }
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 12
            Section { title: "New from a preset"; Layout.fillWidth: true }
            Flow {
                Layout.fillWidth: true
                spacing: 8
                Repeater {
                    model: engine.presets
                    AppButton {
                        required property var modelData
                        text: modelData.title
                        tip: engine.humanDuration(modelData.seconds * 1000) + (modelData.repeat ? " · repeating" : "")
                        onClicked: { let id = engine.addTimer(modelData.title, modelData.seconds, modelData.repeat); if (id) { appWindow.timerId = id; appWindow.setFocusClock("timer", id) } }
                    }
                }
            }
            Text { text: "Presets create a new timer. Save your own in the timer form."; color: theme.colors.muted; font.pixelSize: 12; wrapMode: Text.WordWrap; Layout.fillWidth: true }
        }
        Item { Layout.preferredHeight: 12 }
    }
}
