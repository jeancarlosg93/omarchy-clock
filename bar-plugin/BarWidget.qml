pragma ComponentBehavior: Bound
import QtQuick
import qs.Commons
import qs.Ui
import "." as Local
BarWidget {
    id: root
    moduleName: "jeanc.omarchy-clock"
    readonly property var service: Local.ClockRuntime.service
    visible: !!service && service.visibleClock
    implicitWidth: visible ? (vertical ? barSize : Math.min(Style.space(280), readout.implicitWidth + Style.space(18))) : 0
    implicitHeight: vertical ? Style.bar.iconSlot * 2 : barSize
    Accessible.role: Accessible.Button
    Accessible.name: service ? service.label : "Omarchy Clock"
    Accessible.description: "Click to open. Right-click to pause or resume. Middle-click opens compact display."
    Accessible.onPressAction: if (service) service.launch("--bar-open")
    WidgetButton {
        id: button
        anchors.fill: parent
        bar: root.bar
        labelVisible: false
        hasVisualContent: root.visible
        tooltipText: root.service ? root.service.tooltip : "Omarchy Clock"
        onPressed: function(code) { if (root.service) root.service.launch(code === Qt.RightButton ? "--bar-pause" : code === Qt.MiddleButton ? "--bar-compact" : "--bar-open") }
        Text {
            id: readout
            anchors.fill: parent
            anchors.leftMargin: Style.space(9)
            anchors.rightMargin: Style.space(9)
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
