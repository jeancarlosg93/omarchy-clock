import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
ScrollView {
    id: page
    objectName: "stopwatchPage"
    property var appWindow
    property var live: engine.stopwatch
    property bool nameDirty: false
    Connections { target: engine; function onDataChanged() { if (!sessionName.activeFocus && !page.nameDirty) sessionName.text = engine.stopwatch.title || "Stopwatch" } }
    clip: true
    contentWidth: availableWidth
    ColumnLayout {
        width: page.availableWidth
        spacing: 20
        PageTitle { Layout.fillWidth: true; title: "Stopwatch"; subtitle: "Measure a session. Keep the laps and the story." }
        Field { id: sessionName; objectName: "stopwatchNameField"; Layout.fillWidth: true; Component.onCompleted: text = engine.stopwatch.title || "Stopwatch"; maximumLength: 120; placeholderText: "Session name"; onTextEdited: page.nameDirty = true; onEditingFinished: { engine.renameStopwatch(text); page.nameDirty = false; text = engine.stopwatch.title || "Stopwatch" } Accessible.name: "Stopwatch session name" }
        TimeFace { Layout.fillWidth: true; Layout.preferredHeight: page.height > 650 ? 230 : 160; time: page.live.display; fraction: page.live.fraction }
        Flow {
            Layout.fillWidth: true
            spacing: 8
            AppButton { objectName: "stopwatchStartButton"; text: page.live.running ? "Pause" : page.live.elapsed > 0 ? "Resume" : "Start stopwatch"; primary: true; onClicked: engine.toggleStopwatch(); tip: "Space · Start or pause" }
            AppButton { objectName: "lapButton"; text: "Lap"; enabled: !!page.live.running; onClicked: engine.lap(); tip: "L · Record a lap" }
            AppButton { objectName: "saveStopwatchButton"; text: "Save & new"; enabled: page.live.elapsed > 0; onClicked: { if (engine.saveStopwatch(sessionName.text)) { page.nameDirty = false; sessionName.text = engine.stopwatch.title || "Stopwatch" } } }
            AppButton { text: "Reset"; enabled: page.live.elapsed > 0; onClicked: appWindow.confirm("Reset stopwatch?", "Unsaved elapsed time and laps will be cleared. Save the session to keep them.", function() { engine.resetStopwatch() }) }
        }
        Text { text: page.live.status + (engine.settings.precision === 3 ? " · milliseconds" : " · hundredths"); color: theme.colors.muted; font.pixelSize: 12 }
        Section { title: "Laps"; Layout.fillWidth: true }
        Text { visible: engine.stopwatchLaps.length === 0; text: "Press Lap while the stopwatch runs. Fastest and slowest splits are marked once you have two laps."; color: theme.colors.muted; font.pixelSize: 13; wrapMode: Text.WordWrap; Layout.fillWidth: true }
        RowLayout {
            Layout.fillWidth: true
            visible: engine.stopwatchLaps.length > 0
            Text { text: "Lap"; color: theme.colors.muted; font.pixelSize: 11; Layout.fillWidth: true }
            Text { text: "Split"; color: theme.colors.muted; font.pixelSize: 11; Layout.preferredWidth: 110; horizontalAlignment: Text.AlignRight }
            Text { text: "Total"; color: theme.colors.muted; font.pixelSize: 11; Layout.preferredWidth: page.width < 500 ? 96 : 120; horizontalAlignment: Text.AlignRight }
        }
        Repeater {
            model: engine.stopwatchLaps
            RowLayout {
                required property var modelData
                Layout.fillWidth: true
                Layout.preferredHeight: 34
                spacing: 12
                Text { text: "Lap " + modelData.number + (modelData.best ? " · best" : modelData.worst ? " · slowest" : ""); color: theme.colors.foreground; font.pixelSize: 12; Layout.fillWidth: true; elide: Text.ElideRight }
                Text { text: modelData.splitText; font.family: theme.fontFamily; color: theme.colors.foreground; font.pixelSize: 13; Layout.preferredWidth: 110; horizontalAlignment: Text.AlignRight }
                Text { text: modelData.totalText; font.family: theme.fontFamily; color: theme.colors.muted; font.pixelSize: 13; Layout.preferredWidth: page.width < 500 ? 96 : 120; horizontalAlignment: Text.AlignRight }
            }
        }
        Text { visible: engine.stopwatchLaps.length > 1; text: "Best " + engine.format(page.live.bestLap) + "." + engine.fraction(page.live.bestLap) + " · Average " + engine.format(page.live.averageLap) + "." + engine.fraction(page.live.averageLap); color: theme.colors.muted; font.pixelSize: 12; wrapMode: Text.WordWrap; Layout.fillWidth: true }
        AppButton { text: "View saved sessions"; onClicked: appWindow.go("History") }
        Item { Layout.preferredHeight: 12 }
    }
}
