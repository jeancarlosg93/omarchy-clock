import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
ScrollView {
    id: page
    objectName: "intervalsPage"
    property var appWindow
    property bool composing: false
    property string editingId: ""
    property var live: { let r = engine.revision; return engine.interval(appWindow ? appWindow.intervalId : "") }
    clip: true
    contentWidth: availableWidth
    ListModel { id: phases }
    function compose(id) {
        editingId = id || ""
        let item = id ? engine.interval(id) : null
        nameField.text = item ? item.title : ""
        rounds.value = item ? item.rounds : 4
        soundBox.checked = item ? item.sound : false
        phases.clear()
        let initial = item ? item.phases : [{name: "Work", seconds: 1500}, {name: "Rest", seconds: 300}]
        for (let p of initial) phases.append({name: p.name, seconds: p.seconds})
        composing = true
        nameField.forceActiveFocus()
    }
    function saveForm() {
        let items = []
        for (let i = 0; i < phases.count; ++i) { let p = phases.get(i); items.push({name: p.name, seconds: p.seconds}) }
        let id = editingId
        if (id) { if (!engine.editInterval(id, nameField.text, items, rounds.value, soundBox.checked)) return }
        else { id = engine.addInterval(nameField.text, items, rounds.value, soundBox.checked); if (!id) return }
        appWindow.intervalId = id
        appWindow.setFocusClock("interval", id)
        composing = false
    }
    ColumnLayout {
        width: page.availableWidth
        spacing: 22
        PageTitle { Layout.fillWidth: true; title: "Intervals"; subtitle: "A sequence of phases, repeated at your pace."; AppButton { objectName: "addIntervalButton"; text: "New program"; primary: true; iconName: "plus"; onClicked: page.compose("") } }
        ColumnLayout {
            visible: page.composing
            Layout.fillWidth: true
            spacing: 12
            Section { title: page.editingId ? "Edit program" : "New program"; Layout.fillWidth: true }
            Field { id: nameField; objectName: "intervalNameField"; Layout.fillWidth: true; placeholderText: "Program name"; maximumLength: 120; Accessible.name: "Interval program name" }
            RowLayout { Text { text: "Rounds"; color: theme.colors.muted; font.pixelSize: 13 } Number { id: rounds; from: 1; to: 99; value: 4; Accessible.name: "Rounds" } CheckBox { id: soundBox; text: "Play sound" } }
            Repeater {
                model: phases
                ColumnLayout {
                    required property int index
                    required property string name
                    required property int seconds
                    Layout.fillWidth: true
                    spacing: 8
                    RowLayout { Layout.fillWidth: true; Text { text: (index + 1) + "."; color: theme.colors.muted; font.pixelSize: 13 } Field { text: name; Layout.fillWidth: true; maximumLength: 120; onEditingFinished: phases.setProperty(index, "name", text); Accessible.name: "Phase " + (index + 1) + " name" } AppButton { iconName: "trash"; tip: "Remove phase"; enabled: phases.count > 1; onClicked: phases.remove(index) } }
                    Duration { seconds: parent.seconds; onSecondsChanged: { if (seconds !== parent.seconds) phases.setProperty(parent.index, "seconds", seconds) } }
                }
            }
            AppButton { text: "Add phase"; iconName: "plus"; enabled: phases.count < 20; onClicked: phases.append({name: "Phase " + (phases.count + 1), seconds: 60}) }
            RowLayout { spacing: 8; AppButton { objectName: "saveIntervalButton"; text: page.editingId ? "Save changes" : "Create program"; primary: true; onClicked: page.saveForm() } AppButton { text: "Cancel"; onClicked: page.composing = false } }
            Rectangle { Layout.fillWidth: true; height: 1; color: theme.colors.border }
        }
        ColumnLayout {
            visible: !!page.live.id
            Layout.fillWidth: true
            spacing: 10
            Text { text: page.live.title || ""; color: theme.colors.foreground; font.pixelSize: 18; Layout.alignment: Qt.AlignHCenter; Layout.maximumWidth: page.availableWidth; elide: Text.ElideRight }
            Text { text: (page.live.phaseName || "") + " · Round " + (page.live.round || 1) + " of " + (page.live.rounds || 1); color: theme.colors.muted; font.pixelSize: 13; Layout.alignment: Qt.AlignHCenter }
            TimeFace { Layout.fillWidth: true; Layout.preferredHeight: page.height > 650 ? 190 : 140; time: page.live.display || "00:00" }
            Rectangle { Layout.fillWidth: true; height: 3; color: theme.colors.border; Rectangle { height: 3; width: parent.width * Math.max(0, Math.min(1, page.live.progress || 0)); color: theme.colors.accent } }
            Flow {
                Layout.fillWidth: true
                spacing: 8
                AppButton { objectName: "intervalStartButton"; text: page.live.running ? "Pause" : page.live.status === "Paused" ? "Resume" : page.live.finished ? "Start again" : "Start program"; primary: true; onClicked: engine.toggleInterval(page.live.id) }
                AppButton { text: "Skip phase"; enabled: !!page.live.running; onClicked: engine.skipInterval(page.live.id) }
                AppButton { text: "Reset"; onClicked: engine.resetInterval(page.live.id) }
                AppButton { objectName: "editIntervalButton"; text: "Edit"; enabled: !page.live.running; onClicked: page.compose(page.live.id) }
                AppButton { iconName: "focus"; tip: "Focus display"; onClicked: appWindow.enterFocus("interval", page.live.id) }
                AppButton { iconName: "trash"; tip: "Delete program"; enabled: !page.live.running; onClicked: { let id = page.live.id; appWindow.confirm("Delete program?", "This removes the saved interval program. Recorded sessions stay in History.", function() { engine.removeInterval(id); appWindow.ensureSelections() }) } }
            }
            Text { text: (page.live.status || "Ready") + " · " + engine.humanDuration(page.live.totalRemaining || 0) + " remaining in program"; color: theme.colors.muted; font.pixelSize: 12; wrapMode: Text.WordWrap; Layout.fillWidth: true }
            Repeater {
                model: page.live.phases || []
                RowLayout { required property var modelData; required property int index; Layout.fillWidth: true; Text { text: (index + 1) + ". " + modelData.name; color: theme.colors.foreground; font.pixelSize: 13; Layout.fillWidth: true; elide: Text.ElideRight } Text { text: engine.humanDuration(modelData.seconds * 1000); color: theme.colors.muted; font.pixelSize: 13 } }
            }
        }
        Section { title: "Your programs"; Layout.fillWidth: true }
        Text { visible: engine.intervals.length === 0; text: "Create a sequence for focus, exercise, or a routine."; color: theme.colors.muted; font.pixelSize: 13; wrapMode: Text.WordWrap; Layout.fillWidth: true }
        Repeater {
            model: engine.intervals
            ClockRow { required property var modelData; Layout.fillWidth: true; kind: "interval"; clockId: modelData.id; selected: appWindow && appWindow.intervalId === clockId; onChosen: { appWindow.intervalId = clockId; appWindow.setFocusClock("interval", clockId) } }
        }
        Item { Layout.preferredHeight: 12 }
    }
}
