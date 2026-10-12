import QtQuick
Item {
    id: face
    property string time: "00:00"
    property string fraction: ""
    property int maximumSize: 240
    property int requestedSize: Math.max(32, Math.min(maximumSize, height * 0.8))
    property real fittedSize: Math.max(16, Math.min(requestedSize, requestedSize * width / Math.max(1, metrics.advanceWidth + (fraction.length ? fractionMetrics.advanceWidth : 0))))
    implicitHeight: 160
    TextMetrics { id: metrics; text: face.time; font.family: theme.fontFamily; font.pixelSize: face.requestedSize }
    TextMetrics { id: fractionMetrics; text: "." + face.fraction; font.family: theme.fontFamily; font.pixelSize: Math.round(face.requestedSize * 0.4) }
    Row {
        anchors.centerIn: parent
        Text { id: mainTime; objectName: "timeDisplay"; text: face.time; color: theme.colors.foreground; font.family: theme.fontFamily; font.pixelSize: Math.floor(face.fittedSize); font.weight: Font.Normal; Accessible.name: face.time + (face.fraction ? "." + face.fraction : "") }
        Text { objectName: "fractionDisplay"; visible: face.fraction.length > 0; anchors.baseline: mainTime.baseline; text: "." + face.fraction; color: theme.colors.muted; font.family: theme.fontFamily; font.pixelSize: Math.max(10, Math.round(face.fittedSize * 0.4)); Accessible.ignored: true }
    }
}
