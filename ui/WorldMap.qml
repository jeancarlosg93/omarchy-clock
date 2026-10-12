import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "WorldLand.js" as Land

Rectangle {
    id: map
    objectName: "worldMap"
    property string selectedZone: ""
    property var selectedCity: engine.zoneLocation(selectedZone)
    property var hoverCity: ({})
    property real zoom: 1
    property real longitudePan: 0
    property real latitudePan: 0
    readonly property real degreeScale: Math.max(0.001, Math.min(width / 360, height / 180)) * zoom
    readonly property real renderedWidth: 360 * degreeScale
    readonly property real renderedHeight: 180 * degreeScale
    readonly property real panX: longitudePan * degreeScale
    readonly property real panY: latitudePan * degreeScale
    readonly property real maxPanX: Math.max(0, (renderedWidth - width) / 2)
    readonly property real maxPanY: Math.max(0, (renderedHeight - height) / 2)
    readonly property bool canPan: maxPanX > 0.1 || maxPanY > 0.1
    signal zoneSelected(string zone)
    radius: 6
    color: theme.colors.surface
    border.color: activeFocus ? theme.colors.accent : theme.colors.border
    border.width: activeFocus ? 2 : 1
    clip: true
    activeFocusOnTab: true
    Accessible.role: Accessible.Button
    Accessible.name: "World timezone map. " + (selectedCity.title || "No city selected")
    Accessible.description: "Click near a city to select its timezone. Arrow keys select nearby cities; plus and minus zoom. Search below offers exact selection."
    Accessible.onPressAction: if (selectedCity.id) zoneSelected(selectedCity.id)
    property real originX: (width - renderedWidth) / 2 + panX
    property real originY: (height - renderedHeight) / 2 + panY
    function xAt(lon) { return originX + (lon + 180) * degreeScale }
    function yAt(lat) { return originY + (90 - lat) * degreeScale }
    function cityAt(x, y) {
        if (x < originX || x > originX + renderedWidth || y < originY || y > originY + renderedHeight) return ({})
        return engine.nearestCity(Math.max(-90, Math.min(90, 90 - (y - originY) / degreeScale)), Math.max(-180, Math.min(180, (x - originX) / degreeScale - 180)))
    }
    function clampPan() { longitudePan = Math.max(-maxPanX / degreeScale, Math.min(maxPanX / degreeScale, longitudePan)); latitudePan = Math.max(-maxPanY / degreeScale, Math.min(maxPanY / degreeScale, latitudePan)) }
    function panTo(x, y) { longitudePan = x / degreeScale; latitudePan = y / degreeScale; clampPan() }
    function setZoom(value) { zoom = Math.max(1, Math.min(4, value)); clampPan() }
    function resetView() { longitudePan = 0; latitudePan = 0; setZoom(1) }
    function selectNear(lat, lon) { let city = engine.nearestCity(Math.max(-90, Math.min(90, lat)), ((lon + 540) % 360) - 180); if (city.id) zoneSelected(city.id) }
    Keys.onPressed: function(event) {
        let city = selectedCity.id ? selectedCity : engine.nearestCity(0, 0)
        let step = 10 / zoom
        if (event.key === Qt.Key_Left) selectNear(city.latitude, city.longitude - step)
        else if (event.key === Qt.Key_Right) selectNear(city.latitude, city.longitude + step)
        else if (event.key === Qt.Key_Up) selectNear(city.latitude + step, city.longitude)
        else if (event.key === Qt.Key_Down) selectNear(city.latitude - step, city.longitude)
        else if (event.key === Qt.Key_Plus || event.key === Qt.Key_Equal) setZoom(zoom * 1.5)
        else if (event.key === Qt.Key_Minus) setZoom(zoom / 1.5)
        else if (event.key === Qt.Key_Return || event.key === Qt.Key_Space) { if (selectedCity.id) zoneSelected(selectedCity.id) }
        else return
        event.accepted = true
    }
    property bool consumesSpace: true
    onWidthChanged: { Qt.callLater(map.clampPan); canvas.requestPaint() }
    onHeightChanged: { Qt.callLater(map.clampPan); canvas.requestPaint() }
    onZoomChanged: canvas.requestPaint()
    onPanXChanged: canvas.requestPaint()
    onPanYChanged: canvas.requestPaint()
    onSelectedCityChanged: canvas.requestPaint()
    onHoverCityChanged: canvas.requestPaint()
    Connections { target: theme; function onChanged() { canvas.requestPaint() } }
    Canvas {
        id: canvas
        anchors.fill: parent
        onPaint: {
            let ctx = getContext("2d")
            ctx.reset()
            ctx.fillStyle = theme.colors.surface
            ctx.fillRect(0, 0, width, height)
            ctx.save()
            ctx.beginPath()
            ctx.rect(map.originX, map.originY, map.renderedWidth, map.renderedHeight)
            ctx.clip()
            ctx.strokeStyle = theme.colors.border
            ctx.lineWidth = 0.6
            ctx.beginPath()
            for (let lon = -180; lon <= 180; lon += 30) { let x = map.xAt(lon); ctx.moveTo(x, 0); ctx.lineTo(x, height) }
            for (let lat = -60; lat <= 60; lat += 30) { let y = map.yAt(lat); ctx.moveTo(0, y); ctx.lineTo(width, y) }
            ctx.stroke()
            ctx.fillStyle = theme.colors.hover
            ctx.strokeStyle = theme.colors.muted
            ctx.lineWidth = 0.65
            for (let ring of Land.polygons) {
                ctx.beginPath()
                for (let i = 0; i < ring.length; i++) { let x = map.xAt(ring[i][0]), y = map.yAt(ring[i][1]); if (i === 0) ctx.moveTo(x, y); else ctx.lineTo(x, y) }
                ctx.closePath(); ctx.fill(); ctx.stroke()
            }
            ctx.fillStyle = theme.colors.muted
            for (let city of engine.zoneCities) {
                let x = map.xAt(city.longitude), y = map.yAt(city.latitude)
                if (x < 0 || y < 0 || x > width || y > height) continue
                ctx.beginPath(); ctx.arc(x, y, zoom > 2 ? 2 : 1.4, 0, Math.PI * 2); ctx.fill()
            }
            let cities = [map.hoverCity, map.selectedCity]
            for (let city of cities) {
                if (!city.id) continue
                let x = map.xAt(city.longitude), y = map.yAt(city.latitude)
                ctx.fillStyle = theme.colors.accent
                ctx.strokeStyle = theme.colors.foreground
                ctx.lineWidth = 1.5
                ctx.beginPath(); ctx.arc(x, y, city.id === map.selectedZone ? 5 : 3.5, 0, Math.PI * 2); ctx.fill(); ctx.stroke()
            }
            ctx.restore()
        }
    }
    MouseArea {
        id: mouse
        objectName: "worldMapMouse"
        anchors.fill: parent
        hoverEnabled: true
        preventStealing: true
        cursorShape: map.canPan ? (pressed ? Qt.ClosedHandCursor : Qt.OpenHandCursor) : Qt.PointingHandCursor
        property real startX: 0
        property real startY: 0
        property real initialPanX: 0
        property real initialPanY: 0
        property bool dragged: false
        onPressed: function(event) { map.forceActiveFocus(); startX = event.x; startY = event.y; initialPanX = map.panX; initialPanY = map.panY; dragged = false }
        onPositionChanged: function(event) {
            if (pressed && (dragged || Math.abs(event.x - startX) + Math.abs(event.y - startY) > 6)) { dragged = true; map.hoverCity = ({}); if (map.canPan) map.panTo(initialPanX + event.x - startX, initialPanY + event.y - startY) }
            else map.hoverCity = map.cityAt(event.x, event.y)
        }
        onClicked: function(event) { if (!dragged) { let city = map.cityAt(event.x, event.y); if (city.id) map.zoneSelected(city.id) } }
        onExited: map.hoverCity = ({})
        onCanceled: { dragged = false; map.hoverCity = ({}) }
        onWheel: function(event) { if (event.modifiers & Qt.ControlModifier) { map.setZoom(map.zoom * (event.angleDelta.y > 0 ? 1.3 : 1 / 1.3)); event.accepted = true } else event.accepted = false }
    }
    Rectangle {
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: 10
        width: zoomActions.implicitWidth + 4
        height: 40
        radius: 6
        color: theme.colors.background
        Row {
            id: zoomActions
            anchors.centerIn: parent
            spacing: 4
            AppButton { objectName: "worldZoomOut"; text: "−"; tip: "Zoom out"; enabled: map.zoom > 1; width: 36; height: 36; onClicked: map.setZoom(map.zoom / 1.5) }
            AppButton { objectName: "worldZoomIn"; text: "+"; tip: "Zoom in"; enabled: map.zoom < 4; width: 36; height: 36; onClicked: map.setZoom(map.zoom * 1.5) }
            AppButton { objectName: "worldMapReset"; text: "Reset"; height: 36; enabled: map.zoom !== 1; onClicked: map.resetView() }
        }
    }
    Rectangle {
        visible: map.hoverCity.id !== undefined && !mouse.pressed
        anchors.left: parent.left
        anchors.bottom: parent.bottom
        anchors.margins: 10
        width: Math.min(parent.width - 20, hoverLabel.implicitWidth + 20)
        height: hoverLabel.implicitHeight + 14
        color: theme.colors.background
        border.color: theme.colors.border
        radius: 4
        Text { id: hoverLabel; anchors.fill: parent; anchors.margins: 7; text: map.hoverCity.title || ""; color: theme.colors.foreground; font.pixelSize: 13; elide: Text.ElideRight; verticalAlignment: Text.AlignVCenter }
    }
}
