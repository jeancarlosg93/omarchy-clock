pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import qs.Commons
import qs.Ui
import "." as Local
BarWidget {
    id: root
    moduleName: "jeanc.omarchy-clock"
    readonly property var service: Local.ClockRuntime.service
    visible: !!service && service.visibleClock
    implicitWidth: visible ? (vertical ? barSize : Math.min(Style.space(300), Math.ceil(labelMetrics.advanceWidth) + Style.space(9) * 2 + Style.space(5) + Style.bar.iconCanvas + Style.space(1))) : 0
    implicitHeight: vertical ? Style.bar.iconSlot * 3 : barSize
    Accessible.role: Accessible.Button
    Accessible.name: service ? service.label : "Omarchy Clock"
    Accessible.description: "Click to open. Right-click to pause or resume. Middle-click opens compact display."
    Accessible.onPressAction: if (service) service.launch("--bar-open")
    TextMetrics {
        id: labelMetrics
        text: root.service ? root.service.label : ""
        font.family: button.fontFamily
        font.pixelSize: button.fontSize
    }
    WidgetButton {
        id: button
        anchors.fill: parent
        bar: root.bar
        labelVisible: false
        hasVisualContent: root.visible
        tooltipText: root.service ? root.service.tooltip : "Omarchy Clock"
        onPressed: function(code) { if (root.service) root.service.launch(code === Qt.RightButton ? "--bar-pause" : code === Qt.MiddleButton ? "--bar-compact" : "--bar-open") }
        GridLayout {
            anchors.fill: parent
            anchors.leftMargin: root.vertical ? Style.space(2) : Style.space(9)
            anchors.rightMargin: root.vertical ? Style.space(2) : Style.space(9)
            columns: root.vertical ? 1 : 2
            columnSpacing: Style.space(5)
            rowSpacing: Style.space(2)
            OpticalGlyph {
                Layout.alignment: Qt.AlignCenter
                Layout.preferredWidth: Style.bar.iconCanvas
                Layout.preferredHeight: Style.bar.iconCanvas
                text: "\udb80\udd50"
                color: button.foreground
                fontFamily: button.fontFamily
                fontSize: Style.bar.iconFont
                Accessible.ignored: true
            }
            Text {
                id: readout
                Layout.fillWidth: true
                Layout.fillHeight: true
                textFormat: Text.PlainText
                text: !root.service ? "" : root.vertical ? (root.service.state.clock ? root.service.state.clock.display : "") : root.service.label
                color: button.foreground
                font.family: button.fontFamily
                font.pixelSize: button.fontSize
                verticalAlignment: Text.AlignVCenter
                horizontalAlignment: Text.AlignHCenter
                elide: Text.ElideRight
                wrapMode: root.vertical ? Text.WrapAnywhere : Text.NoWrap
            }
        }
    }
}
