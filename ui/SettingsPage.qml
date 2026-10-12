import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
ScrollView {
    id: page
    objectName: "settingsPage"
    property var appWindow
    clip: true
    contentWidth: availableWidth
    ColumnLayout {
        width: page.availableWidth
        spacing: 24
        PageTitle { Layout.fillWidth: true; title: "Settings"; subtitle: "Make the clock fit your day." }
        Section { title: "Alerts"; Layout.fillWidth: true }
        AppSwitch { objectName: "notificationSwitch"; text: "Desktop notifications"; checked: engine.settings.notifications; onToggled: engine.setSetting("notifications", checked) }
        AppSwitch { text: "Play sound by default"; checked: engine.settings.sound; onToggled: engine.setSetting("sound", checked) }
        Text { text: "Silent notifications are the default. A timer, alarm, or interval can opt into sound individually."; color: theme.colors.muted; font.pixelSize: 12; wrapMode: Text.WordWrap; Layout.fillWidth: true }
        Flow { Layout.fillWidth: true; spacing: 8; AppButton { text: "Test notification"; onClicked: shell.testNotification() } AppButton { text: "Test sound"; onClicked: shell.testSound() } }
        RowLayout { Text { text: "Snooze minutes"; color: theme.colors.foreground; font.pixelSize: 14 } Number { from: 1; to: 120; value: engine.settings.snoozeMinutes; onValueModified: engine.setSetting("snoozeMinutes", value) } }
        Section { title: "Display"; Layout.fillWidth: true }
        RowLayout {
            Layout.fillWidth: true
            Text { text: "Stopwatch precision"; color: theme.colors.foreground; font.pixelSize: 14; Layout.fillWidth: true }
            Choice { objectName: "precisionChoice"; Layout.preferredWidth: page.width < 500 ? 125 : 180; model: ["Hundredths", "Milliseconds"]; currentIndex: engine.settings.precision === 3 ? 1 : 0; onActivated: engine.setSetting("precision", currentIndex === 1 ? 3 : 2) }
        }
        AppSwitch { text: "24-hour world clocks"; checked: engine.settings.hour24; onToggled: engine.setSetting("hour24", checked) }
        RowLayout {
            Layout.fillWidth: true
            Text { text: "Open to"; color: theme.colors.foreground; font.pixelSize: 14; Layout.fillWidth: true }
            Choice { Layout.preferredWidth: 170; model: appWindow ? appWindow.pages.map(function(p) { return p.title }) : []; currentIndex: Math.max(0, model.indexOf(engine.settings.startPage)); onActivated: engine.setSetting("startPage", currentText) }
        }
        Text { text: "Theme: " + theme.name + " · updates with Omarchy. Time uses " + theme.fontFamily + "."; color: theme.colors.muted; font.pixelSize: 12; Layout.fillWidth: true; wrapMode: Text.WordWrap }
        Section { title: "Background operation"; Layout.fillWidth: true }
        AppSwitch { objectName: "barVisibleSwitch"; text: "Show active clocks in the Omarchy bar"; checked: engine.settings.barVisible !== false; onToggled: engine.setSetting("barVisible", checked) }
        Text { text: "The bar shows the focused running clock, or another active clock. Click to open it, right-click to pause or resume, and middle-click for the compact display."; color: theme.colors.muted; font.pixelSize: 12; Layout.fillWidth: true; wrapMode: Text.WordWrap }
        AppSwitch { text: "Keep running when the window closes"; checked: engine.settings.keepInTray; onToggled: engine.setSetting("keepInTray", checked) }
        AppSwitch { text: "Start in the tray when I sign in"; checked: shell.autoStart; onToggled: shell.setAutoStart(checked) }
        Text { text: shell.trayAvailable ? "The tray menu opens each tool, controls the focused clock, and lets you quit. Scheduled alarms remain active while the window is hidden." : "No system tray is currently available. Open the app from the launcher to restore the window; closing it will ask before quitting."; color: theme.colors.muted; font.pixelSize: 12; Layout.fillWidth: true; wrapMode: Text.WordWrap }
        Text { text: "Computer sleep delays alerts until wake. Quitting the app stops alert delivery; saved clocks catch up when you reopen it."; color: theme.colors.muted; font.pixelSize: 12; Layout.fillWidth: true; wrapMode: Text.WordWrap }
        Section { title: "Keyboard"; Layout.fillWidth: true }
        Text { text: "Space · start/pause the selected clock\nL · stopwatch lap\nF11 · fullscreen\nEscape · leave fullscreen or focus\nCtrl+1…9 · switch tools\nCtrl+Shift+M · compact display\n" + shell.showShortcut + " · show/hide app\n" + shell.pauseShortcut + " · start/pause focused clock"; color: theme.colors.muted; font.pixelSize: 13; lineHeight: 1.5; Layout.fillWidth: true; wrapMode: Text.WordWrap }
        Section { title: "Timer presets"; Layout.fillWidth: true }
        Repeater {
            model: engine.presets
            RowLayout { required property var modelData; Layout.fillWidth: true; Text { text: modelData.title + " · " + engine.humanDuration(modelData.seconds * 1000); color: theme.colors.foreground; font.pixelSize: 13; Layout.fillWidth: true; elide: Text.ElideRight } AppButton { text: "Remove"; onClicked: engine.removePreset(modelData.id) } }
        }
        Section { title: "Projects"; Layout.fillWidth: true }
        Repeater {
            model: engine.projects
            RowLayout { required property var modelData; Layout.fillWidth: true; Text { text: modelData.title; color: theme.colors.foreground; font.pixelSize: 13; Layout.fillWidth: true; elide: Text.ElideRight } AppButton { text: "Remove"; onClicked: { let title = modelData.title; appWindow.confirm("Remove project?", "Existing history keeps its project name. This removes it from your project choices.", function() { engine.removeProject(title) }) } } }
        }
        Section { title: "Local data"; Layout.fillWidth: true }
        Text { text: "Your clocks and history are stored in a local SQLite database. There is no account, cloud sync, or network connection."; color: theme.colors.muted; font.pixelSize: 12; Layout.fillWidth: true; wrapMode: Text.WordWrap }
        Text { text: engine.storagePath; color: theme.colors.muted; font.family: theme.fontFamily; font.pixelSize: 11; Layout.fillWidth: true; wrapMode: Text.WrapAnywhere }
        Flow { Layout.fillWidth: true; spacing: 8; AppButton { text: "Open data folder"; onClicked: shell.openDataFolder() } AppButton { text: "Export all history"; onClicked: appWindow.exportHistory("", "") } AppButton { text: "Quit app"; onClicked: appWindow.requestQuit() } }
        Text { text: "Omarchy Clock 2.1.5 · Qt Quick"; color: theme.colors.muted; font.pixelSize: 12 }
        Item { Layout.preferredHeight: 12 }
    }
}
