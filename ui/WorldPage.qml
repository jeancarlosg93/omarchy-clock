import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
ScrollView {
    id: page
    objectName: "worldPage"
    property var appWindow
    property bool adding: false
    function compose(unused) { adding = true }
    property var selectedInfo: { let r = engine.revision; return engine.zoneInfo(picker.zone) }
    clip: true
    contentWidth: availableWidth
    ColumnLayout {
        width: page.availableWidth
        spacing: 24
        PageTitle { Layout.fillWidth: true; title: "World clocks"; subtitle: "See the same moment in different places."; AppButton { text: "Add clock"; primary: true; iconName: "plus"; onClicked: page.adding = !page.adding } }
        ColumnLayout {
            visible: page.adding
            Layout.fillWidth: true
            spacing: 10
            ZonePicker { id: picker; objectName: "worldZonePicker"; Layout.fillWidth: true }
        }
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 12
            WorldMap { Layout.fillWidth: true; Layout.preferredHeight: Math.max(200, Math.min(360, width / 2)); selectedZone: picker.zone; onZoneSelected: function(zone) { picker.chooseZone(zone) } }
            RowLayout {
                Layout.fillWidth: true
                ColumnLayout {
                    Layout.fillWidth: true
                    Text { text: page.selectedInfo.title || "Choose a city"; color: theme.colors.foreground; font.pixelSize: 18; font.weight: Font.Medium; Layout.fillWidth: true; elide: Text.ElideRight }
                    Text { text: (page.selectedInfo.zone || "") + " · " + (page.selectedInfo.offset || ""); color: theme.colors.muted; font.pixelSize: 12; Layout.fillWidth: true; wrapMode: Text.WrapAnywhere }
                }
                AppButton { objectName: "addZoneButton"; text: engine.favoriteZones.indexOf(picker.zone) >= 0 ? "Added" : "Add clock"; primary: true; enabled: engine.favoriteZones.indexOf(picker.zone) < 0; onClicked: { if (engine.addZone(picker.zone)) page.adding = false } }
            }
            Text { text: "Click near a city to select its timezone. Zoom in, then drag to pan. Search offers exact city or timezone selection."; color: theme.colors.muted; font.pixelSize: 12; Layout.fillWidth: true; wrapMode: Text.WordWrap }
            AppButton { visible: page.adding; text: "Cancel search"; onClicked: page.adding = false }
        }
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 12
            Section { title: "Compare a local date and time"; Layout.fillWidth: true }
            Flow {
                Layout.fillWidth: true
                spacing: 8
                Field { id: compareDate; objectName: "compareDateField"; width: 144; text: Qt.formatDate(new Date(), "yyyy-MM-dd"); maximumLength: 10; Accessible.name: "Comparison date" }
                Field { id: compareTime; objectName: "compareTimeField"; width: 92; text: Qt.formatTime(new Date(), "HH:mm"); maximumLength: 5; Accessible.name: "Comparison time" }
                AppButton { objectName: "compareButton"; text: "Compare"; onClicked: engine.setComparison(compareDate.text, compareTime.text) }
                AppButton { text: "Now"; selected: engine.comparisonEpoch === 0; onClicked: { engine.resetComparison(); compareDate.text = Qt.formatDate(new Date(), "yyyy-MM-dd"); compareTime.text = Qt.formatTime(new Date(), "HH:mm") } }
            }
            Text { text: engine.comparisonEpoch ? "Showing your selected moment in each timezone." : "Live clocks · offsets and daylight-saving changes update automatically."; color: theme.colors.muted; font.pixelSize: 12; Layout.fillWidth: true; wrapMode: Text.WordWrap }
        }
        Text { visible: engine.favoriteZones.length === 0; text: "Add a city or timezone to compare it with your local time."; color: theme.colors.muted; font.pixelSize: 14; Layout.fillWidth: true; wrapMode: Text.WordWrap }
        Repeater {
            model: engine.favoriteZones
            ColumnLayout {
                required property string modelData
                property var info: { let r = engine.revision; return engine.zoneInfo(modelData) }
                Layout.fillWidth: true
                spacing: 10
                RowLayout {
                    Layout.fillWidth: true
                    ColumnLayout {
                        Layout.fillWidth: true
                        Text { text: info.title; color: theme.colors.foreground; font.pixelSize: 19; font.weight: Font.Medium; Layout.fillWidth: true; elide: Text.ElideRight }
                        Text { text: info.zone; color: theme.colors.muted; font.pixelSize: 12; Layout.fillWidth: true; elide: Text.ElideRight }
                    }
                    AppButton { iconName: "focus"; tip: "Focus live clock"; onClicked: appWindow.enterFocus("world", modelData) }
                    AppButton { iconName: "trash"; tip: "Remove world clock"; onClicked: engine.removeZone(modelData) }
                }
                Text { text: info.display; color: theme.colors.foreground; font.family: theme.fontFamily; font.pixelSize: page.width < 500 ? 38 : 50; Layout.fillWidth: true; elide: Text.ElideRight }
                Text { text: info.date + " · " + info.offset + " · " + info.difference + (info.daylight ? " · daylight saving" : "") + (info.dayOffset > 0 ? " · next day" : info.dayOffset < 0 ? " · previous day" : ""); color: theme.colors.muted; font.pixelSize: 12; Layout.fillWidth: true; wrapMode: Text.WordWrap }
                Rectangle { Layout.fillWidth: true; height: 1; color: theme.colors.border }
            }
        }
        Item { Layout.preferredHeight: 12 }
    }
}
