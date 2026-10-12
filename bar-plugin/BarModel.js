.pragma library
function fresh(state, now) {
    return state && state.version === 1 && state.enabled === true && Math.abs(now - Number(state.updatedAt || 0)) < 6000
}
function shortKind(kind) {
    return kind === "stopwatch" ? "Stopwatch" : kind === "interval" ? "Intervals" : kind === "tracking" ? "Tracking" : "Timer"
}
function label(state) {
    var clock = state.clock || {}
    if (!clock.id) return ""
    var title = clock.kind === "stopwatch" ? "Stopwatch" : String(clock.title || shortKind(clock.kind))
    var text = title + " " + String(clock.display || "")
    if (!clock.running) text += " · paused"
    var others = Math.max(0, Number(state.runningCount || 0) - (clock.running ? 1 : 0))
    if (others) text += " · +" + others
    return text
}
function tooltip(state) {
    var clocks = state.clocks || []
    var lines = ["Omarchy Clock"]
    for (var i = 0; i < clocks.length; i++) lines.push(String(clocks[i].title || shortKind(clocks[i].kind)) + " · " + String(clocks[i].display || ""))
    if (!clocks.length && state.clock && state.clock.id) lines.push(String(state.clock.title || shortKind(state.clock.kind)) + " · " + state.clock.display + " · paused")
    lines.push("Click: open · Right-click: pause/resume · Middle-click: compact")
    return lines.join("\n")
}
