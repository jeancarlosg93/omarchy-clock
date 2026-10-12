import QtQuick
import QtQuick.Layouts
Item {
    id: focusView
    property var appWindow
    property var live: { let r = engine.revision; return engine.clockDetails(appWindow ? appWindow.focusKind : "world", appWindow ? appWindow.focusId : "") }
    ColumnLayout {
        anchors.fill: parent
        spacing: focusView.height < 340 ? 8 : focusView.height < 440 ? 12 : 20
        Choice {
            Layout.alignment: Qt.AlignHCenter
            Layout.preferredWidth: Math.min(focusView.width, 340)
            model: appWindow ? appWindow.clockOptions : []
            textRole: "title"
            currentIndex: appWindow ? Math.max(0, appWindow.clockOptions.findIndex(function(v) { return v.kind === appWindow.focusKind && v.id === appWindow.focusId })) : 0
            onActivated: { let choice = model[currentIndex]; appWindow.setFocusClock(choice.kind, choice.id) }
            Accessible.name: "Clock to focus"
        }
        Text { text: focusView.live.title || "Local time"; color: theme.colors.foreground; font.pixelSize: 20; Layout.alignment: Qt.AlignHCenter; Layout.maximumWidth: focusView.width; elide: Text.ElideRight }
        TimeFace {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: focusView.height < 340 ? 64 : 90
            maximumSize: 420
            time: focusView.live.display || "00:00"
            fraction: appWindow && appWindow.focusKind === "stopwatch" ? focusView.live.fraction || "" : ""
        }
        Text { text: appWindow && appWindow.focusKind === "interval" ? (focusView.live.phaseName || "") + " · Round " + focusView.live.round + " of " + focusView.live.rounds : appWindow && appWindow.focusKind === "world" ? (focusView.live.date || "") + " · " + (focusView.live.offset || "") : focusView.live.status || "Ready"; color: theme.colors.muted; font.pixelSize: 13; Layout.alignment: Qt.AlignHCenter; Layout.maximumWidth: focusView.width; wrapMode: Text.WordWrap; horizontalAlignment: Text.AlignHCenter }
        Rectangle { visible: appWindow && (appWindow.focusKind === "timer" || appWindow.focusKind === "interval"); Layout.fillWidth: true; height: 3; color: theme.colors.border; Rectangle { height: 3; width: parent.width * Math.max(0, Math.min(1, focusView.live.progress || 0)); color: theme.colors.accent } }
        Flow {
            objectName: "focusActions"
            // Measure the buttons themselves: childrenRect.width measures the
            // already-wrapped layout and can keep this flow stuck in a column.
            property real naturalWidth: {
                let total = 0, count = 0
                for (let i = 0; i < children.length; ++i) {
                    if (children[i].visible) { total += children[i].implicitWidth; ++count }
                }
                return total + Math.max(0, count - 1) * spacing
            }
            Layout.alignment: Qt.AlignHCenter
            Layout.preferredWidth: Math.min(focusView.width, naturalWidth)
            spacing: 8
            AppButton { objectName: "focusStartButton"; visible: appWindow && appWindow.focusKind !== "world"; text: focusView.live.running ? "Pause" : focusView.live.status === "Paused" ? "Resume" : "Start"; primary: true; onClicked: appWindow.toggleFocused() }
            AppButton { visible: appWindow && appWindow.focusKind === "stopwatch"; text: "Lap"; enabled: !!focusView.live.running; onClicked: engine.lap() }
            AppButton { visible: appWindow && appWindow.focusKind === "timer"; text: "+1 min"; onClicked: engine.addTime(appWindow.focusId, 60) }
            AppButton { visible: appWindow && appWindow.focusKind === "interval"; text: "Skip phase"; enabled: !!focusView.live.running; onClicked: engine.skipInterval(appWindow.focusId) }
        }
    }
}
