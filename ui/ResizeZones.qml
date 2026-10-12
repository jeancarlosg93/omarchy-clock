import QtQuick
Item {
    property var window
    anchors.fill: parent
    Repeater {
        model: [Qt.TopEdge | Qt.LeftEdge, Qt.TopEdge | Qt.RightEdge, Qt.BottomEdge | Qt.LeftEdge, Qt.BottomEdge | Qt.RightEdge]
        MouseArea {
            required property int modelData
            width: 12
            height: 12
            x: modelData & Qt.LeftEdge ? 0 : parent.width - width
            y: modelData & Qt.TopEdge ? 0 : parent.height - height
            enabled: parent.window && parent.window.visibility !== Window.FullScreen
            cursorShape: (modelData === (Qt.TopEdge | Qt.LeftEdge) || modelData === (Qt.BottomEdge | Qt.RightEdge)) ? Qt.SizeFDiagCursor : Qt.SizeBDiagCursor
            onPressed: parent.window.startSystemResize(modelData)
        }
    }
}
