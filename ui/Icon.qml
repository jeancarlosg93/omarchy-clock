import QtQuick
Image {
    property string name: "timer"
    property color color: theme.colors.muted
    width: 20
    height: 20
    source: "data:image/svg+xml;utf8," + encodeURIComponent('<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" stroke="' + color + '" stroke-width="1.7" stroke-linecap="round" stroke-linejoin="round">' + paths[name] + '</svg>')
    property var paths: ({
        today: '<path d="M3 10l9-7 9 7v10H3zM9 20v-7h6v7"/>',
        timer: '<circle cx="12" cy="14" r="8"/><path d="M9 2h6M12 2v4M18 7l2-2M12 9v5l3 2"/>',
        stopwatch: '<circle cx="12" cy="14" r="8"/><path d="M9 2h6M12 2v4M18 7l2-2M12 9v5"/>',
        alarm: '<circle cx="12" cy="13" r="8"/><path d="M3 5l3-3M18 2l3 3M6 20l-2 2M18 20l2 2M12 8v5h4"/>',
        interval: '<path d="M4 7h14l-3-3M20 17H6l3 3M20 7l-2 2M4 17l2-2"/>',
        world: '<circle cx="12" cy="12" r="9"/><ellipse cx="12" cy="12" rx="4" ry="9"/><path d="M3 12h18M5 7h14M5 17h14"/>',
        tracking: '<rect x="3" y="7" width="18" height="14" rx="2"/><path d="M8 7V3h8v4M3 12h18M10 12v3h4v-3"/>',
        history: '<path d="M3 5v5h5M3 10a9 9 0 1 1 2 8M12 7v5l3 2"/>',
        settings: '<path d="M4 5h16M4 12h16M4 19h16"/><circle cx="8" cy="5" r="2" fill="' + color + '"/><circle cx="16" cy="12" r="2" fill="' + color + '"/><circle cx="10" cy="19" r="2" fill="' + color + '"/>',
        focus: '<path d="M3 8V3h5M16 3h5v5M21 16v5h-5M8 21H3v-5"/>',
        compact: '<rect x="3" y="4" width="18" height="16" rx="2"/><rect x="12" y="12" width="9" height="8" rx="1"/>',
        close: '<path d="M5 5l14 14M19 5L5 19"/>',
        chevron: '<path d="M6 9l6 6 6-6"/>',
        plus: '<path d="M12 4v16M4 12h16"/>',
        play: '<path d="M8 4l12 8-12 8z"/>',
        pause: '<path d="M8 4v16M16 4v16"/>',
        trash: '<path d="M3 6h18M9 6V3h6v3M6 6l1 15h10l1-15M10 10v7M14 10v7"/>'
    })
}
