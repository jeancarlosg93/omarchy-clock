import QtQuick
import QtQuick.Controls
ComboBox {
    id: control
    implicitHeight: 40
    font.pixelSize: 13
    leftPadding: 12
    rightPadding: 30
    contentItem: Text { text: control.displayText; font: control.font; color: control.enabled ? theme.colors.foreground : theme.colors.muted; verticalAlignment: Text.AlignVCenter; elide: Text.ElideRight }
    indicator: Icon { name: "chevron"; width: 14; height: 14; x: control.width - width - 10; y: (control.height - height)/2; color: theme.colors.foreground }
    background: Rectangle { radius: 6; color: theme.colors.surface; border.width: control.activeFocus ? 2 : 1; border.color: control.activeFocus ? theme.colors.accent : theme.colors.border }
    delegate: ItemDelegate {
        id: row
        objectName: "choicePopupRow"
        required property int index
        width: control.popup.width - 4
        height: 38
        text: control.textAt(index)
        highlighted: control.highlightedIndex === index
        font: control.font
        contentItem: Text { objectName: "choicePopupText"; text: row.text; font: row.font; color: row.highlighted ? theme.colors.onAccent : theme.colors.foreground; elide: Text.ElideRight; verticalAlignment: Text.AlignVCenter }
        background: Rectangle { objectName: "choicePopupRowBackground"; radius: 4; color: row.highlighted ? theme.colors.accent : row.hovered ? theme.colors.hover : theme.colors.surface }
        Accessible.name: text
    }
    popup: Popup {
        objectName: "choicePopup"
        y: control.height + 4
        width: control.width
        padding: 2
        margins: 12
        implicitHeight: Math.min(282, contentItem.implicitHeight + 4)
        contentItem: ListView {
            clip: true
            implicitHeight: contentHeight
            model: control.popup.visible ? control.delegateModel : null
            currentIndex: control.highlightedIndex
            ScrollIndicator.vertical: ScrollIndicator { }
        }
        background: Rectangle { objectName: "choicePopupBackground"; radius: 6; color: theme.colors.surface; border.color: theme.colors.border; border.width: 1 }
    }
    Accessible.name: displayText
}
