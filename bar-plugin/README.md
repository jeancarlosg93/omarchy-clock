# Omarchy Clock bar widget

This user-owned plugin displays the native app's running timer, stopwatch, interval or tracking activity. It follows the Omarchy bar font and colors. It is installed beside Chime; Chime's own clocks remain independent.

Left-click opens the displayed clock in the app's Focus view. Right-click pauses/resumes that exact clock. Middle-click opens its compact display. A `+N` suffix indicates additional active clocks; hover lists all of them. When all clocks are paused, the last displayed paused clock remains available for resume. Ready or finished clocks disappear. Settings → Background operation → Show active clocks in the Omarchy bar can hide this widget.

The app atomically publishes a private runtime JSON file once per second: `$XDG_RUNTIME_DIR/omarchy-clock-status.json`. The shared service watches it, ignores malformed/unsupported data, and hides stale data after six seconds. Normal app exit removes the bridge file. Clocks and notifications stay owned by the native app; the widget does not run a duplicate timer or schedule its own alerts.

The service exposes a local Quickshell IPC target for verification:

```
omarchy-shell omarchy-clock-bar status
omarchy-shell omarchy-clock-bar open
omarchy-shell omarchy-clock-bar pause
omarchy-shell omarchy-clock-bar compact
```

The installer backs up the bar config and replaced plugin files. It adds one layout entry on the first integration and preserves later manual removal. Remove `jeanc.omarchy-clock` from the bar layout to remove it completely from the bar; the app's setting is the simpler visibility toggle.
