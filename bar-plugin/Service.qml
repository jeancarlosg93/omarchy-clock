pragma ComponentBehavior: Bound
import QtQuick
import Quickshell
import Quickshell.Io
import "BarModel.js" as Model
import "." as Local
Item {
    id: root
    property var shell: null
    property var state: ({})
    property double nowMs: Date.now()
    readonly property bool fresh: Model.fresh(state, nowMs)
    readonly property bool visibleClock: fresh && !!state.clock && !!state.clock.id
    readonly property string label: visibleClock ? Model.label(state) : ""
    readonly property string tooltip: Model.tooltip(state)
    readonly property string executable: (Quickshell.env("HOME") || "") + "/.local/bin/omarchy-clock"
    function launch(action) { Quickshell.execDetached([executable, action]) }
    Component.onCompleted: Local.ClockRuntime.service = root
    Component.onDestruction: if (Local.ClockRuntime.service === root) Local.ClockRuntime.service = null
    SystemClock { precision: SystemClock.Seconds; onDateChanged: root.nowMs = date.getTime() }
    FileView {
        path: (Quickshell.env("XDG_RUNTIME_DIR") || "/tmp") + "/omarchy-clock-status.json"
        watchChanges: true
        printErrors: false
        onFileChanged: reload()
        onLoaded: {
            try { let parsed = JSON.parse(text()); root.state = parsed.version === 1 ? parsed : ({}) } catch (error) { root.state = ({}) }
        }
        onLoadFailed: root.state = ({})
    }
    IpcHandler {
        target: "omarchy-clock-bar"
        function status(): string { return JSON.stringify({fresh:root.fresh, visible:root.visibleClock, label:root.label, state:root.state}) }
        function open(): void { root.launch("--bar-open") }
        function pause(): void { root.launch("--bar-pause") }
        function compact(): void { root.launch("--bar-compact") }
    }
}
