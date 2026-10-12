import QtQuick
Item {
    activeFocusOnTab: true
    readonly property bool consumesSpace: true
    signal activated()
    Accessible.role: Accessible.Button
    Accessible.onPressAction: activated()
    Keys.onReturnPressed: activated()
    Keys.onEnterPressed: activated()
    Keys.onSpacePressed: function(event) { activated(); event.accepted = true }
    // Tab navigation must reveal a focused row as well as make it actionable.
    onActiveFocusChanged: {
        if (!activeFocus) return
        let viewport = parent
        while (viewport) {
            if (viewport.contentY !== undefined && viewport.contentItem && viewport.contentHeight !== undefined) {
                let point = mapToItem(viewport.contentItem, 0, 0)
                let target = viewport.contentY
                if (point.y < target) target = point.y
                else if (point.y + height > target + viewport.height) target = point.y + height - viewport.height
                viewport.contentY = Math.max(0, Math.min(Math.max(0, viewport.contentHeight - viewport.height), target))
                break
            }
            viewport = viewport.parent
        }
    }
}
