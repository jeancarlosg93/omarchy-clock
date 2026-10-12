import QtQuick
import QtQuick.Layouts
ColumnLayout {
    id: picker
    property string zone: engine.favoriteZones.length ? engine.favoriteZones[0] : "UTC"
    property var matches: engine.findZones(search.text || zone)
    function chooseZone(id) { zone = id; search.text = ""; Qt.callLater(function() { for (let i = 0; i < matches.length; i++) if (matches[i].id === zone) { choice.currentIndex = i; break } }) }
    spacing: 8
    Field {
        id: search
        objectName: "zoneSearchField"
        Layout.fillWidth: true
        placeholderText: "Search city or timezone"
        onAccepted: { if (picker.matches.length) picker.chooseZone(picker.matches[0].id) }
        Accessible.name: "Search timezone"
    }
    Choice {
        id: choice
        objectName: "zoneChoice"
        Layout.fillWidth: true
        model: picker.matches
        textRole: "title"
        valueRole: "id"
        onActivated: picker.chooseZone(currentValue)
        Accessible.name: "Timezone"
    }
    Text { text: "Timezone: " + picker.zone; color: theme.colors.muted; font.pixelSize: 11; Layout.fillWidth: true; wrapMode: Text.WordWrap }
    Text { visible: picker.matches.length === 0; text: "No matching city or timezone. Try a city name or an IANA timezone such as America/Santo_Domingo."; color: theme.colors.muted; font.pixelSize: 12; Layout.fillWidth: true; wrapMode: Text.WordWrap }
}
