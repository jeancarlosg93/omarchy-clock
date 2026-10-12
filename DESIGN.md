---
name: Omarchy Clock
description: A native timekeeping suite in the active Omarchy desktop theme.
colors:
  accent: "#5573e2"
  on-accent: "#000000"
  background: "#080a0e"
  foreground: "#ffffff"
  muted: "#bfbfbf"
  surface: "#181a1e"
  border: "#343639"
  hover: "#26272b"
typography:
  display:
    fontFamily: "monospace"
    fontSize: "240px"
    fontWeight: 400
  focus-display:
    fontFamily: "monospace"
    fontSize: "420px"
    fontWeight: 400
  fraction:
    fontFamily: "monospace"
    fontSize: "40%"
  headline:
    fontFamily: "sans-serif"
    fontSize: "24px"
    fontWeight: 600
  body:
    fontFamily: "sans-serif"
    fontSize: "14px"
  field:
    fontFamily: "sans-serif"
    fontSize: "14px"
  control:
    fontFamily: "sans-serif"
    fontSize: "13px"
  status:
    fontFamily: "sans-serif"
    fontSize: "13px"
  lap:
    fontFamily: "monospace"
    fontSize: "13px"
  label:
    fontFamily: "sans-serif"
    fontSize: "11px"
rounded:
  control: "6px"
  dialog: "8px"
  switch: "12px"
spacing:
  tight: "4px"
  navigation: "6px"
  control-gap: "8px"
  row-gap: "12px"
  compact-margin: "16px"
  narrow-margin: "18px"
  focus-gap: "20px"
  form-section: "22px"
  section: "24px"
  margin: "26px"
  rail-gap: "28px"
components:
  button-primary:
    backgroundColor: "{colors.accent}"
    textColor: "{colors.on-accent}"
    typography: "{typography.control}"
    rounded: "{rounded.control}"
    height: "40px"
  button-secondary:
    backgroundColor: "transparent"
    textColor: "{colors.foreground}"
    typography: "{typography.control}"
    rounded: "{rounded.control}"
    height: "40px"
  button-secondary-hover:
    backgroundColor: "{colors.surface}"
    textColor: "{colors.foreground}"
    rounded: "{rounded.control}"
    height: "40px"
  button-selected:
    backgroundColor: "{colors.hover}"
    textColor: "{colors.foreground}"
    rounded: "{rounded.control}"
    height: "40px"
  text-field:
    backgroundColor: "{colors.surface}"
    textColor: "{colors.foreground}"
    typography: "{typography.field}"
    rounded: "{rounded.control}"
    padding: "0 12px"
    height: "40px"
  duration-field:
    backgroundColor: "{colors.surface}"
    textColor: "{colors.foreground}"
    typography: "{typography.field}"
    rounded: "{rounded.control}"
    height: "40px"
    width: "96px"
  choice:
    backgroundColor: "{colors.surface}"
    textColor: "{colors.foreground}"
    typography: "{typography.control}"
    rounded: "{rounded.control}"
    padding: "0 30px 0 12px"
    height: "40px"
  switch-on:
    backgroundColor: "{colors.accent}"
    textColor: "{colors.on-accent}"
    rounded: "{rounded.switch}"
    height: "24px"
    width: "46px"
  clock-row:
    backgroundColor: "transparent"
    textColor: "{colors.foreground}"
    rounded: "{rounded.control}"
    padding: "12px"
    height: "82px"
  time-display:
    textColor: "{colors.foreground}"
    typography: "{typography.display}"
  progress-track:
    backgroundColor: "{colors.border}"
    height: "3px"
  confirmation-dialog:
    backgroundColor: "{colors.surface}"
    textColor: "{colors.foreground}"
    rounded: "{rounded.dialog}"
---

# Design System: Omarchy Clock

## Overview

**Creative North Star: "The Omarchy Timepiece"**

The established Omarchy desktop supplies the visual authority: its active palette, installed measurement font, flat surfaces, thin dividers, and restrained corners. Large regular-weight numerals make time readable at a distance; supporting controls stay quiet and direct.

This document records the implemented Linux desktop Qt Quick system in `Main.qml`, `ui/*.qml`, `Theme::reload()` in `clockmodel.cpp`, and the native shell widget in `bar-plugin/`. The suite expands the incumbent timepiece into nine tools while retaining its visual world. Frontmatter colors preserve the observed blackmesa snapshot; runtime theme roles and derivation rules below remain authoritative when the desktop theme changes. Generic font families represent native font resolution, and display sizes are requested caps rather than fixed rendered sizes.

**Key Characteristics:**

- Active desktop palette and theme-resolved measurement typography.
- Flat tonal surfaces with thin borders.
- Large centered time and compact supporting controls.
- Native Wayland movement, resizing, and fullscreen.
- Desktop navigation rail, narrow tool selector, and scrollable working pages.
- Explicit popup color pairs, offline geographic selection, and shell-native clock readouts.

## Colors

One active accent sits against the desktop background; foreground, muted text, and derived neutral layers carry the rest of the interface.

### Primary

- **Desktop accent:** primary actions, countdown progress, current-day report bars, text selection, selected controls, and focus outlines.
- **Accent ink:** black or white, whichever has the higher computed contrast against the accent; also the checked switch thumb.

### Neutral

- **Desktop background / foreground:** window canvas and time numerals; normal control text.
- **Muted foreground:** status, subtitles, field labels, theme name, fractions, secondary lap values, and historical report bars.
- **Surface:** fields, selectors, dialogs, hovered secondary controls, and selected or hovered rows.
- **Border:** resting control outlines, dividers, progress tracks, and unchecked switch tracks.
- **Hover:** selected or pressed secondary controls.

### Runtime mapping

The reader polls every (1500ms). It first reads `$XDG_STATE_HOME/omarchy/current/theme/colors.toml`, defaulting the state root to `~/.local/state`, then tries `~/.config/omarchy/current/theme/colors.toml`. Only quoted six-digit hex assignments are parsed. An empty theme during a later atomic swap retains the last palette.

| UI role | First available source key | Fallback or derivation |
| --- | --- | --- |
| Background | `background` | `#1a1b26` |
| Foreground | `foreground` | `#c0caf5` |
| Accent | `accent`, `blue`, `color4` | `#7aa2f7` |
| Muted | `dark_foreground`, `muted` | `#a9b1d6` |
| Surface | — | 93.5% background + 6.5% foreground |
| Border | — | 82% background + 18% foreground |
| Hover | — | 88% background + 12% foreground |
| Accent ink | — | Higher-contrast black or white |

Blends interpolate sRGB channels. If muted/background contrast is below (4.5:1), muted becomes 75% foreground + 25% background; if its contrast against surface remains below (4.5:1), it becomes foreground. The reader also exposes `success` from `green`, `color2`, or `#9ece6a`; no current visual component uses it. Theme name comes from the state directory's `theme.name`, with hyphens displayed as spaces, or defaults to “Omarchy”.

**The Desktop Authority Rule.** Resolve app colors through the theme reader and bar colors through the shell widget; theme changes belong to the desktop, not this app.

The frontmatter is an inherited snapshot, not a new palette extraction. In particular, the theme's secondary color is not a source key for `muted`; the reader uses the mapping above. This documentation refresh does not reconcile any unrelated historical snapshot drift.

## Typography

**Measurement Font:** the family returned by Qt's system fixed font. An enabled `~/.config/omarchy/shell-font.json` with a nonempty `family` overrides it; its `weight` field is not read. `TimeFace` explicitly uses regular weight. Local/world clocks, clock-row values, history durations, and lap measurements use the resolved family too.

**Interface Font:** Qt's application font. The main window supplies the body size; buttons, selectors, text fields, spin boxes, headings, labels, and supporting status text inherit or explicitly size this font. Frontmatter sans-serif is a portable description of the observed native interface font, not an instruction to override Qt. Qt font metrics determine line height; there is no custom tracking.

- **TimeFace:** centered measurements with the `display` cap by default, the `focus-display` cap in Focus, and (76px) maximum in compact. Requested size is `max(32, min(maximumSize, height × 0.8))`; combined text metrics fit seconds and fractions to width, with a (16px) lower fitted bound. Dedicated views allocate different heights, so they often render below the cap.
- **Fractions:** muted, immediately after seconds, sharing their baseline. Size is 40% of fitted numerals, with a (10px) minimum. Hundredths and optional milliseconds use the same treatment. Accessibility announces the combined value once.
- **Headlines:** `headline`, semibold. Sections use (15px) semibold; the wide app title uses (18px) semibold. Supporting titles range from (18px) to (20px).
- **Body / fields / switches:** `body` and `field`. Navigation, buttons, selectors, subtitles, and status use the smaller `control` / `status` roles.
- **Supporting values:** lap splits/totals use `lap`; lap descriptions use (12px) application text. Labels and metadata use `label`; explanatory copy commonly uses (12px) or (13px).

**The Measurement Rule.** Use the resolved monospace family for measured time and regular weight for the large time face; keep supporting interface text in the native application font.

## Layout

Qt Quick layouts use logical pixels and follow compositor scale. The main window opens at (1150×780), with a minimum of (380×460). Its outer margin uses `margin`, switching to `narrow-margin` below width (560px). Main vertical spacing is (18px). A (42px) header and one-pixel divider frame the flexible working canvas.

At width (960px) and above, a (176px) navigation rail lists Today, Timers, Stopwatch, Alarms, Intervals, World clocks, Tracking, History, and Settings. Rail items use `navigation` gaps; `rail-gap` separates navigation from the page. Below that threshold, a header selector replaces the rail: (170px) wide normally and (148px) below width (560px). Focus/Back, Compact, Fullscreen, and the ellipsis app menu remain in the narrow header. Header gaps reduce from (8px) to (6px) below width (560px), retaining all four actions at the minimum window size. Settings belongs only to tool navigation; Hide to tray, Pause active clocks, and Quit belong only to the app menu. Ctrl+1…9 switches tools.

Working pages and creation forms use clipped vertical ScrollViews whose content width follows available width. Actions use Flow where necessary; labels wrap or elide according to their role. Common page group spacing is (20px) to (26px). The desktop rail is navigation rather than a decorative container, and page sections are separated by whitespace and thin dividers.

Focus replaces the page/rail composition with a centered clock selector (up to 340px), title, flexible time face, status/progress, and centered actions. Actions form one horizontal row when their intrinsic widths fit, wrap within the available width when narrow, and return to one row when widened; preserve the primary playback action first. Focus/Back and Compact remain shared header controls, with no repeated navigation actions below the time; Compact is also available through Ctrl+Shift+M. Clock pages do not repeat the shared Focus action. World and local clock selection choose a target, then the header enters Focus. Local time is available in the Focus selector. Focus vertical spacing uses `control-gap` below height (340px), `row-gap` below (440px), and `focus-gap` otherwise. Its time face yields vertical space to controls, with minimum height (64px) below (340px) and (90px) otherwise. Fullscreen uses the same native window and header; F11 toggles it, and Escape returns to normal size or leaves Focus. Focus and fullscreen are independent states.

The compact tool window opens at (360×240), with minimum (160×110), `compact-margin` and `control-gap`. It contains an elided clock title, draggable area, small close button, fitted time, and Start/Pause/Resume, optional Lap, and Open actions. Below width (280px) or height (190px), its slim mode uses (8px) margins, (4px) vertical gaps, a (26px) header, and (28px) action height. Playback and Open become named icon controls; Lap remains a short label. The time face flexes into the remaining space while preserving inline fractions.

The compact window explicitly has no transient parent: hiding the main window or closing it to the tray leaves compact visible. It carries the always-on-top hint, with the installed Omarchy rule floating and pinning it across workspaces. Compact's close control and focused Ctrl+W hide only compact; main-window Ctrl+W follows main close-to-tray/quit behavior. Explicit app Quit exits both windows. Keep these independent window lifecycles when extending the compact display.

The empty header space calls `startSystemMove()`; double-click toggles fullscreen. Main and compact windows use four (12×12px) native corner resize zones, disabled in fullscreen. Compositor snapping, decoration, and scaling remain native responsibilities. Launcher and Open actions reveal the app on the current Hyprland workspace; they preserve a valid window placement and size, recentering an off-screen or moved window. The show/hide shortcut hides a focused main window and otherwise brings it forward. Verified evidence spans the supplied 27-capture desktop/narrow/form/focus/compact/empty matrix and actual (2560×1440) Wayland fullscreen capture.

## Elevation & Depth

The app draws no shadows. Tonal fills, borders, and modal surfaces establish separation. No gradients, translucent panels, or animated elevation are used. Authored control states update immediately; AppButton tooltips appear after (650ms). Qt supplies menu, tooltip, scrollbar, and file-dialog internals through the window's live palette. Shared ComboBox popup surfaces and rows bind their color pairs explicitly.

## Shapes

Shared buttons, fields, selectors, and selectable rows use `rounded.control`; confirmation surfaces use `rounded.dialog`. Resting control borders are (1px); focused fields/buttons and selected buttons use (2px) accent borders. Keyboard rows use the same focus outline. Switch tracks are pill-shaped with an (18px) round thumb inset (3px). Progress tracks are flat (3px) strips; section dividers are (1px). Containers stay flat and open rather than decorative cards.

## Components

### Actions and navigation

`AppButton` is a strong-focus Qt Button with centered native interface text and the shared height. Icon-only buttons are (40px) wide; labeled width is `max(64, label width + 28)` or `max(64, label width + 48)` with an icon, unless a local layout overrides it. Primary buttons retain accent fill across hover/press. Secondary controls are transparent at rest, surface on hover, and hover-tone when pressed or selected. Selected/focused outlines use accent; disabled opacity is (0.4). Tooltips name icon-only controls, and accessible names use the label or tooltip. Selected navigation reuses this button treatment. Compact's close control reduces to (30×30px), or (26×26px) in slim mode; slim playback/Open are (28×28px), and Lap is (40×28px).

Space on a focused button, switch, checkbox, selector, editor, or keyboard row belongs to that control. Background/display Space starts or pauses the selected clock in the focused main or compact window. Editing, confirmations, and exports suppress the relevant global clock actions. L records a stopwatch lap outside editing. Preserve these native keyboard expectations when adding controls.

### Inputs and choices

`Field` is a (40px) native TextField with surface fill, symmetric horizontal padding, live placeholder/selection colors, mouse selection, and an accent focus border. Names and descriptions use deliberate commit policies; the stopwatch name holds a dirty draft through running and paused ticks, commits on editing completion, and Save & new consumes the current draft.

`Number` is an editable (96px) SpinBox with (26px) minus/plus indicators, centered numeric editing space, surface fill, and an accent focus border. Duration groups label hours/minutes/seconds, use (0–99)/(0–59)/(0–59) ranges, and share this component.

`Choice` is a (40px) ComboBox with padding applied once: (12px) left, (30px) right. A single (14px) chevron sits (10px) from the right edge. Text elides inside the remaining area. Tool, history-type, precision, project, and clock selectors reuse this component.

Its popup sits (4px) below the control, uses a (2px) inset, the shared control corner, and a (1px) border over opaque surface fill. The clipped list is capped at (282px), with a scroll indicator and (38px) rows. Ordinary rows pair foreground text with surface fill; hovered rows use hover tone; highlighted rows pair contrast-aware accent ink with opaque accent fill. Popup-row corners are (4px). Row labels inherit control type and elide within the popup width. These explicit pairs apply to every shared choice in both light and dark themes; native palette defaults alone do not establish popup legibility.

`AppSwitch` has a (46×24px) track: accent with contrast-aware ink thumb when checked, border tone with foreground thumb otherwise. Focus adds a (2px) foreground border. Label text wraps and uses body type. Alarm switches expose explicit enable names; the window palette supplies conventional Qt checkboxes.

### Selectable clock and history rows

`KeyboardRow` supplies Tab focus, Enter/Space activation, Button accessibility role and press action, and automatic scroll reveal. `ClockRow` and History provide descriptive accessible names and a (2px) accent focus outline. Clock rows are (82px) high with `row-gap` inset/gaps; history rows are (74px) high with (10px) inset. Selected or hovered rows use surface fill. Selecting a row opens/selects its clock or record; the distinct inline Start/Pause button remains a separate action. Do not make row selection toggle playback implicitly.

### Time, progress, laps, and reports

`TimeFace` uses foreground numerals and muted inline fractions. Countdown and interval progress fill accent over border-tone tracks, clamped to (0–1). Lap rows show newest first, mark best/slowest splits when meaningful, and retain separate measured split/total columns. History exposes saved session detail, laps, export, and delete through keyboard reachable controls.

Today and ordinary Focus/compact world clocks always use live date/time. World comparison affects the explicitly labeled World-page comparison workflow; it does not freeze the local clock elsewhere. Empty states describe actual available actions rather than insert sample records.

Tracking reports use accent for the current day and muted for earlier days, with flat bars and project progress strips. Daily values currently appear through hover tooltips and accessible names; a keyboard tooltip or visible per-day value remains a nonblocking review observation, not a claim of completed keyboard inspection support.

### Geographic timezone selection

`WorldMap` is an offline, clipped geographic canvas using the shared control corner, surface ground, and border or accent focus outline. An equirectangular projection draws bundled vector land with hover-tone fill and muted contours; thin border-tone graticules and muted city dots give location context. Hovered and selected cities use accent markers with a foreground outline; the selected marker is larger. The viewport height follows half its width within (200–360px), preserving the scrollable page at narrow sizes. Inside it, the geographic image fits at a fixed (2:1) aspect ratio using one pixels-per-degree scale for both axes, centered with surface-tone padding where needed. Land, graticules, and markers clip to the geographic extent; padding has no city hit target.

Clicking chooses the nearest listed IANA representative city, then shows its city name, timezone identifier, offset, and a separate Add clock action. This is a geographic selection aid, not timezone-boundary or address lookup. Search remains available for exact timezone selection and supplies explicit no-results recovery. The data and geometry ship locally; no tiles or geocoding requests are needed.

Zoom ranges from (1–4×). Opaque background-tone backing keeps Zoom out, Zoom in, and Reset controls distinct over the map. The helper directs users to zoom in, then drag to pan. When the rendered geography exceeds the viewport, an open hand becomes a closed hand during a held press. The map retains the pointer grab through release, including outside the canvas, so the enclosing ScrollView cannot steal the gesture. Movement beyond the (6px) combined-axis threshold suppresses city selection on release. Pan offsets use geographic units, survive resize where the rendered bounds allow, and clamp to those bounds; Reset restores fit and zero pan. Ctrl+wheel and plus/minus keys zoom, arrow keys select nearby cities, and Enter/Space confirms the selected city. The map owns Space while focused. Hover labels use opaque background fill, a thin border, and foreground text. Selection and comparison retain separate live/comparison labeling.

### Application clock mark

The shared identity mark is Pictogrammers Material Design Icons clock-outline, explicitly chosen to match Omarchy’s native Nerd Font menu icons. The bundled SVG path is reused unchanged with transparent ground and live foreground fill. Its header slot is (20×20px); native window and tray icons render the same mark at their requested sizes. The bar uses its corresponding Nerd Font glyph (U+F0150) with the host font and foreground. Functional navigation icons retain their existing authored stroke treatment.

Launcher SVGs use a foreground-specific filename and atomically updated desktop Icon field, giving theme changes a fresh image-cache identity. The user-owned theme-set hook updates this mark even when the app is closed. Icon source and Apache 2.0 attribution live in `data/clock-icon-README.md`; application code remains MIT.

### Shell clock readout

The separate `jeanc.omarchy-clock` widget sits beside the existing Chime widget and inherits the shell's `WidgetButton` foreground, font family, font size, tooltip, and interaction surface. It does not import the app's large display type or fix the shell to the palette snapshot. Horizontal readouts place the clock glyph before the name and measured time, add “paused” when relevant, and show a count of other running clocks. Vertical bars stack the glyph above the wrapping time in three icon slots. Independent TextMetrics determine horizontal width, with rounded pixel padding and a (300px) scaled maximum. Text elides in the bounded slot.

The app supplies the advertised clock and remains its timing owner. A focused running clock takes precedence, followed by the previously advertised running clock and another active clock; when all clocks pause, an eligible paused clock remains resumable. Clicking opens that clock in Focus, right-clicking pauses/resumes it, and middle-clicking opens its compact display. The tooltip describes these actions and active clocks. Settings exposes the shared switch for bar visibility. Missing, disabled, unsupported, or stale app state hides the readout; data older than (6s) is not presented as live.

### Dialogs and background behavior

Confirmation is a centered modal surface with width `min(window.width - 36, 440)`, wrapped text, and shared Cancel/Continue buttons carrying RejectRole/AcceptRole. Delete/reset/quit actions supply contextual title and explanation. CSV export uses a native SaveFile dialog. Closing hides to the tray when configured and available; otherwise an active-clock quit confirmation explains that closed-app alerts cannot arrive. The tray menu supplies Open, Pause active clocks, and Quit; tool and Settings selection stay in the main navigation. Qt owns dialog internals; do not substitute web or mobile patterns.

The sidecar's self-contained HTML/CSS components are illustrative design-panel translations of these native primitives. They do not supply app runtime behavior, desktop font discovery, dynamic theme updates, native menu/dialog semantics, or compositor operations. No shipping raster assets are introduced.

## Do's and Don'ts

### Do:

- **Do** inherit the active Omarchy palette and resolved measurement font.
- **Do** preserve minimum-size controls, scrollable forms, and native fullscreen/resize behavior.
- **Do** use tonal fills and thin accent focus borders for interaction states.
- **Do** preserve keyboard row activation, scroll reveal, and focused controls' ownership of Space.
- **Do** fit the combined time/fraction group and keep fractions on the seconds baseline.
- **Do** pair ordinary popup foreground/surface and highlighted popup accent ink/accent explicitly in both theme modes.
- **Do** describe representative-city map selection honestly and retain exact timezone search.
- **Do** preserve equal geographic axis scale, non-selectable map padding, and held-pointer grab through release; clamp geographic pan to the rendered bounds on resize.
- **Do** inherit shell typography for the bar and target the advertised app-owned clock.

### Don't:

- **Don't** hardcode the blackmesa snapshot as the app's permanent theme.
- **Don't** replace native window operations with simulated dragging or scaling.
- **Don't** introduce raster decoration, shadows, or a separate display font into this established surface.
- **Don't** apply selector padding twice or sacrifice narrow tool names to secondary header actions.
- **Don't** overwrite an active edit draft with live clock refreshes or show comparison time as the live local clock.
