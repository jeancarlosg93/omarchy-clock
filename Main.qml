import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import "ui" as UI

ApplicationWindow {
    id: root
    objectName: "clockWindow"
    title: "Omarchy Clock"
    width: 1150
    height: 780
    minimumWidth: 380
    minimumHeight: 460
    visible: true
    color: theme.colors.background
    font.pixelSize: 14
    property string page: "Today"
    property string timerId: ""
    property string intervalId: ""
    property string focusKind: "timer"
    property string focusId: ""
    onFocusKindChanged: shell.publishBarState()
    onFocusIdChanged: shell.publishBarState()
    property bool focusMode: false
    property bool allowQuit: false
    property bool full: visibility === Window.FullScreen
    property bool wide: width >= 960
    property int margin: width < 560 ? 18 : 26
    property bool editing: activeFocusItem && (activeFocusItem instanceof TextInput || activeFocusItem instanceof SpinBox || activeFocusItem instanceof ComboBox)
    property var confirmationCallback: null
    property string confirmationText: ""
    property string notificationText: ""
    property var pages: [{title:"Today", icon:"today", file:"TodayPage.qml"}, {title:"Timers", icon:"timer", file:"TimersPage.qml"}, {title:"Stopwatch", icon:"stopwatch", file:"StopwatchPage.qml"}, {title:"Alarms", icon:"alarm", file:"AlarmsPage.qml"}, {title:"Intervals", icon:"interval", file:"IntervalsPage.qml"}, {title:"World clocks", icon:"world", file:"WorldPage.qml"}, {title:"Tracking", icon:"tracking", file:"TrackingPage.qml"}, {title:"History", icon:"history", file:"HistoryPage.qml"}, {title:"Settings", icon:"settings", file:"SettingsPage.qml"}]
    property var clockOptions: {
        let options = []
        for (let t of engine.timers) options.push({kind:"timer",id:t.id,title:"Timer · " + t.title})
        options.push({kind:"stopwatch",id:"stopwatch",title:"Stopwatch"})
        for (let p of engine.intervals) options.push({kind:"interval",id:p.id,title:"Intervals · " + p.title})
        if (engine.trackHasActivity) options.push({kind:"tracking",id:"tracking",title:"Tracking"})
        for (let z of engine.favoriteZones) options.push({kind:"world",id:z,title:z.replace(/_/g," ")})
        return options
    }
    palette.window: theme.colors.background
    palette.base: theme.colors.surface
    palette.alternateBase: theme.colors.hover
    palette.windowText: theme.colors.foreground
    palette.text: theme.colors.foreground
    palette.button: theme.colors.surface
    palette.buttonText: theme.colors.foreground
    palette.highlight: theme.colors.accent
    palette.highlightedText: theme.colors.onAccent
    palette.mid: theme.colors.border
    palette.dark: theme.colors.border
    palette.placeholderText: theme.colors.muted

    function ensureSelections() {
        if (!engine.timer(timerId).id) timerId = engine.timers.length ? engine.timers[0].id : ""
        if (!engine.interval(intervalId).id) intervalId = engine.intervals.length ? engine.intervals[0].id : ""
        if ((focusKind === "timer" && !engine.timer(focusId).id) || (focusKind === "interval" && !engine.interval(focusId).id)) setFocusClock("timer", timerId)
        if (!focusId && focusKind === "timer") setFocusClock("timer", timerId)
    }
    function setFocusClock(kind, id) { focusKind = kind; focusId = id || "" }
    function enterFocus(kind, id) { setFocusClock(kind, id); focusMode = true }
    function openClock(kind, id) {
        if (kind === "timer") { timerId = id; go("Timers") }
        else if (kind === "stopwatch") go("Stopwatch")
        else if (kind === "interval") { intervalId = id; go("Intervals") }
        else if (kind === "tracking") go("Tracking")
        else go("World clocks")
        setFocusClock(kind, id)
    }
    function go(title, compose) {
        page = title
        focusMode = false
        if (page === "Timers") setFocusClock("timer", timerId)
        else if (page === "Stopwatch") setFocusClock("stopwatch", "stopwatch")
        else if (page === "Intervals") setFocusClock("interval", intervalId)
        else if (page === "Tracking") setFocusClock("tracking", "tracking")
        loadPage()
        if (compose && pageLoader.item && pageLoader.item.compose) pageLoader.item.compose("")
    }
    function loadPage() {
        let entry = pages.filter(function(p) { return p.title === page })[0] || pages[0]
        pageLoader.setSource("ui/" + entry.file, {appWindow:root})
    }
    function toggleFull() { if (full) showNormal(); else showFullScreen() }
    function clockShortcutAllowed(item) { return !item || (!(item instanceof TextInput || item instanceof SpinBox || item instanceof ComboBox || item instanceof AbstractButton) && item.pressed === undefined && typeof item.clicked !== "function" && typeof item.activated !== "function" && !item.consumesSpace) }
    function toggleFocused() {
        if (focusKind === "timer") engine.toggleTimer(focusId)
        else if (focusKind === "stopwatch") engine.toggleStopwatch()
        else if (focusKind === "interval") engine.toggleInterval(focusId)
        else if (focusKind === "tracking") engine.toggleTracking()
    }
    function toggleCompact() { compact.visible = !compact.visible; if (compact.visible) shell.presentWindow(compact) }
    function showMain() { shell.command("show") }
    function confirm(title, text, callback) { confirmation.title = title; confirmationText = text; confirmationCallback = callback; confirmation.open() }
    function requestQuit() {
        if (engine.hasActive) confirm("Quit Omarchy Clock?", "Running clocks and alarms will be saved, but alerts cannot arrive while the app is closed. Hide the window to keep them running.", function() { root.allowQuit = true; shell.quit() })
        else { allowQuit = true; shell.quit() }
    }
    function exportHistory(kind, sessionId) { exportDialog.kind = kind; exportDialog.sessionId = sessionId; exportDialog.open() }
    Component.onCompleted: { ensureSelections(); page = engine.settings.startPage || "Today"; loadPage() }
    onClosing: function(close) {
        if (allowQuit) return
        close.accepted = false
        if (engine.settings.keepInTray && shell.trayAvailable) { hide(); engine.save() }
        else requestQuit()
    }
    Connections {
        target: engine
        function onDataChanged() { root.ensureSelections() }
        function onToast(message) { root.notificationText = message; toastTimer.restart() }
        function onAlert(kind, id, title, body, sound) { root.notificationText = title + " · " + body; toastTimer.restart(); root.alert(0) }
    }
    Timer { id: toastTimer; interval: 6000; onTriggered: root.notificationText = "" }
    Shortcut { sequence: "F11"; onActivated: root.toggleFull() }
    Shortcut { sequence: "Escape"; enabled: !confirmation.visible; onActivated: { if (root.full) root.showNormal(); else if (root.focusMode) root.focusMode = false } }
    Shortcut { sequence: "Space"; enabled: shell.focusedWindow === root && root.clockShortcutAllowed(root.activeFocusItem) && !confirmation.visible && !exportDialog.visible; onActivated: root.toggleFocused() }
    Shortcut { sequence: "L"; enabled: !root.editing && !confirmation.visible && root.focusKind === "stopwatch"; onActivated: engine.lap() }
    Shortcut { sequence: "Ctrl+Shift+M"; onActivated: root.toggleCompact() }
    Shortcut { sequence: "Ctrl+W"; enabled: shell.focusedWindow === root && !confirmation.visible && !exportDialog.visible; onActivated: root.close() }
    Instantiator { model: 9; Shortcut { required property int index; sequence: "Ctrl+" + (index + 1); enabled: !confirmation.visible; onActivated: root.go(root.pages[index].title) } }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: root.margin
        spacing: 18
        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: 42
            Layout.maximumHeight: 42
            spacing: 8
            UI.Icon { visible: !root.wide && root.width >= 560; name: "timer"; color: theme.colors.foreground; width: 24; height: 24 }
            Text { visible: root.wide; text: "Omarchy Clock"; color: theme.colors.foreground; font.pixelSize: 18; font.weight: Font.DemiBold }
            UI.Choice { objectName: "toolChoice"; visible: !root.wide && !root.focusMode; Layout.preferredWidth: root.width < 560 ? 148 : 170; model: root.pages.map(function(p) { return p.title }); currentIndex: Math.max(0, model.indexOf(root.page)); onActivated: root.go(currentText); Accessible.name: "Timekeeping tool" }
            Item { Layout.fillWidth: true; Layout.fillHeight: true; MouseArea { anchors.fill: parent; onPressed: root.startSystemMove(); onDoubleClicked: root.toggleFull() } }
            UI.AppButton { visible: root.width >= 560; text: root.focusMode ? "Back" : root.width > 720 ? "Focus" : ""; iconName: root.focusMode ? "close" : "focus"; tip: root.focusMode ? "Back to tools" : "Focus on the selected clock"; onClicked: root.focusMode = !root.focusMode }
            UI.AppButton { iconName: "compact"; tip: "Compact floating display · Ctrl+Shift+M"; onClicked: root.toggleCompact() }
            UI.AppButton { objectName: "fullscreenButton"; iconName: "focus"; text: root.width > 720 ? root.full ? "Exit full" : "Full screen" : ""; tip: "F11 · Toggle fullscreen"; onClicked: root.toggleFull() }
            UI.AppButton { iconName: "settings"; tip: "App menu"; onClicked: appMenu.popup() }
            Menu { id: appMenu; MenuItem { text: root.focusMode ? "Back to tools" : "Focus selected clock"; onTriggered: root.focusMode = !root.focusMode } MenuItem { text: "Hide to tray"; enabled: shell.trayAvailable; onTriggered: root.hide() } MenuItem { text: "Pause active clocks"; onTriggered: engine.pauseAll() } MenuItem { text: "Settings"; onTriggered: root.go("Settings") } MenuSeparator {} MenuItem { text: "Quit app"; onTriggered: root.requestQuit() } }
        }
        Rectangle { Layout.fillWidth: true; height: 1; color: theme.colors.border }
        RowLayout {
            visible: engine.error.length > 0
            Layout.fillWidth: true
            Text { text: engine.error; color: theme.colors.foreground; font.pixelSize: 13; Layout.fillWidth: true; wrapMode: Text.WordWrap }
            UI.AppButton { text: "Dismiss"; onClicked: engine.clearError() }
        }
        RowLayout {
            visible: root.notificationText.length > 0
            Layout.fillWidth: true
            Text { text: root.notificationText; color: theme.colors.muted; font.pixelSize: 12; Layout.fillWidth: true; wrapMode: Text.WordWrap }
            UI.AppButton { iconName: "close"; tip: "Dismiss message"; onClicked: root.notificationText = "" }
        }
        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 28
            ColumnLayout {
                visible: root.wide && !root.focusMode
                Layout.preferredWidth: 176
                Layout.maximumWidth: 176
                Layout.fillHeight: true
                spacing: 6
                Repeater {
                    model: root.pages
                    UI.AppButton { required property var modelData; Layout.fillWidth: true; text: modelData.title; iconName: modelData.icon; selected: root.page === modelData.title; onClicked: root.go(modelData.title) }
                }
                Item { Layout.fillHeight: true }
                Text { text: theme.name; font.pixelSize: 11; color: theme.colors.muted; Layout.fillWidth: true; elide: Text.ElideRight }
                Text { text: engine.activeClocks.length + " clocks running"; font.pixelSize: 11; color: theme.colors.muted }
                UI.AppButton { text: "Hide to tray"; iconName: "compact"; enabled: shell.trayAvailable; Layout.fillWidth: true; onClicked: root.hide() }
            }
            Loader { id: pageLoader; objectName: "pageLoader"; visible: !root.focusMode; Layout.fillWidth: true; Layout.minimumWidth: 0; Layout.fillHeight: true }
            UI.FocusView { visible: root.focusMode; appWindow: root; Layout.fillWidth: true; Layout.fillHeight: true }
        }
    }
    UI.ResizeZones { window: root }
    Dialog {
        id: confirmation
        objectName: "confirmationDialog"
        anchors.centerIn: parent
        width: Math.min(root.width - 36, 440)
        modal: true
        title: "Confirm"
        background: Rectangle { color: theme.colors.surface; radius: 8; border.color: theme.colors.border }
        contentItem: Label { text: root.confirmationText; color: theme.colors.foreground; wrapMode: Text.WordWrap }
        footer: DialogButtonBox {
            background: Rectangle { color: theme.colors.surface }
            UI.AppButton { objectName: "cancelConfirmationButton"; text: "Cancel"; DialogButtonBox.buttonRole: DialogButtonBox.RejectRole }
            UI.AppButton { objectName: "acceptConfirmationButton"; text: "Continue"; primary: true; DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole }
        }
        onAccepted: { let callback = root.confirmationCallback; root.confirmationCallback = null; if (callback) callback() }
        onRejected: root.confirmationCallback = null
    }
    FileDialog {
        id: exportDialog
        property string kind: ""
        property string sessionId: ""
        title: "Export timekeeping history"
        fileMode: FileDialog.SaveFile
        nameFilters: ["CSV files (*.csv)"]
        defaultSuffix: "csv"
        onAccepted: engine.exportCsv(selectedFile, kind, sessionId)
    }
    Window {
        id: compact
        objectName: "compactWindow"
        title: "Omarchy Clock · Compact"
        width: 360
        height: 240
        minimumWidth: 160
        minimumHeight: 110
        visible: false
        transientParent: null
        color: theme.colors.background
        flags: Qt.Tool | Qt.WindowStaysOnTopHint
        property bool tiny: width < 280 || height < 190
        property var live: { let r = engine.revision; return engine.clockDetails(root.focusKind, root.focusId) }
        Shortcut { sequence: "Ctrl+W"; enabled: shell.focusedWindow === compact; onActivated: compact.hide() }
        Shortcut { sequence: "Space"; enabled: compact.visible && shell.focusedWindow === compact && root.clockShortcutAllowed(compact.activeFocusItem); onActivated: root.toggleFocused() }
        ColumnLayout {
            anchors.fill: parent
            anchors.margins: compact.tiny ? 8 : 16
            spacing: compact.tiny ? 4 : 8
            RowLayout {
                Layout.fillWidth: true
                spacing: 6
                Text { text: compact.live.title || "Local time"; color: theme.colors.foreground; font.pixelSize: 12; Layout.fillWidth: true; elide: Text.ElideRight }
                Item { Layout.preferredWidth: compact.tiny ? 12 : 20; Layout.preferredHeight: compact.tiny ? 26 : 30; MouseArea { anchors.fill: parent; onPressed: compact.startSystemMove() } }
                UI.AppButton { objectName: "compactCloseButton"; iconName: "close"; tip: "Close compact display · Ctrl+W"; implicitWidth: compact.tiny ? 26 : 30; implicitHeight: compact.tiny ? 26 : 30; onClicked: compact.hide() }
            }
            UI.TimeFace { objectName: "compactTimeFace"; Layout.fillWidth: true; Layout.fillHeight: true; implicitHeight: 0; maximumSize: 76; time: compact.live.display || "00:00"; fraction: root.focusKind === "stopwatch" ? compact.live.fraction || "" : "" }
            RowLayout {
                Layout.alignment: Qt.AlignHCenter
                spacing: compact.tiny ? 6 : 8
                UI.AppButton { objectName: "compactPlaybackButton"; visible: root.focusKind !== "world"; text: compact.tiny ? "" : compact.live.running ? "Pause" : compact.live.status === "Paused" ? "Resume" : "Start"; iconName: compact.tiny ? compact.live.running ? "pause" : "play" : ""; tip: compact.live.running ? "Pause selected clock · Space" : "Start or resume selected clock · Space"; implicitWidth: compact.tiny ? 28 : Math.max(64, contentItem.implicitWidth + 28); implicitHeight: compact.tiny ? 28 : 40; primary: true; onClicked: root.toggleFocused() }
                UI.AppButton { objectName: "compactLapButton"; visible: root.focusKind === "stopwatch"; text: "Lap"; implicitWidth: compact.tiny ? 40 : 64; implicitHeight: compact.tiny ? 28 : 40; enabled: !!compact.live.running; onClicked: engine.lap() }
                UI.AppButton { objectName: "compactOpenButton"; text: compact.tiny ? "" : "Open"; iconName: compact.tiny ? "focus" : ""; tip: "Open main window"; implicitWidth: compact.tiny ? 28 : 64; implicitHeight: compact.tiny ? 28 : 40; onClicked: { root.showMain(); root.openClock(root.focusKind, root.focusId) } }
            }
        }
        UI.ResizeZones { window: compact }
    }
}
