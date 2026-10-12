#!/usr/bin/env python3
"""Install Omarchy Clock for the current user with reversible desktop integration."""
from pathlib import Path
import datetime, json, os, shlex, shutil, socket, subprocess, time

project = Path(__file__).resolve().parent
home = Path.home()
binary = project / 'build/omarchy-clock'
target = home / '.local/bin/omarchy-clock'
config = home / '.config/hypr/hyprland.lua'
module = home / '.config/hypr/omarchy-clock.lua'
startup = home / '.config/autostart/omarchy-clock.desktop'
integration = home / '.local/share/omarchy-clock/integration.json'
bar_config = home / '.config/omarchy/shell.json'
plugin_id = 'jeanc.omarchy-clock'
plugin_dir = home / '.config/omarchy/plugins' / plugin_id
plugin_sources = sorted((project / 'bar-plugin').iterdir(), key=lambda p: p.name == 'manifest.json')
stamp = datetime.datetime.now().strftime('%Y%m%d-%H%M%S')
backup_root = home / '.local/share/omarchy-clock/backups' / stamp
if not binary.is_file():
    raise SystemExit('Build first: cmake --build build')
subprocess.run(['desktop-file-validate', str(project / 'omarchy-clock.desktop')], check=True)
if not config.exists():
    raise SystemExit('Expected the Omarchy Lua configuration at ~/.config/hypr/hyprland.lua.')

def run(*args):
    return subprocess.run(args, check=True, capture_output=True, text=True).stdout

def backup(path):
    if path.exists():
        saved = backup_root / path.relative_to(home)
        saved.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(path, saved)
        return saved
    return None

# Choose unused keys; never replace a user's binding.
old_integration = json.loads(integration.read_text()) if integration.exists() else {}
binds = json.loads(run('hyprctl', '-j', 'binds'))
def choose(candidates, description):
    for key in candidates:
        collisions = [b for b in binds if b.get('modmask') == 76 and b.get('key', '').lower() == key.lower()]
        if not collisions or all(b.get('description') == description for b in collisions):
            return key
    raise SystemExit(f'No free shortcut found for {description}; no configuration was changed.')
show_key = choose([old_integration.get('showKey', 'C'), 'F10', 'F9'], 'Omarchy Clock: show or hide')
pause_key = choose([old_integration.get('pauseKey', 'space'), 'P', 'F8'], 'Omarchy Clock: start or pause')

# Gracefully save and stop previous instances before replacing their executable.
clients = [c for c in json.loads(run('hyprctl', '-j', 'clients')) if c.get('class') == 'omarchy-clock']
control_socket = Path(os.environ.get("XDG_RUNTIME_DIR", f"/run/user/{os.getuid()}")) / "omarchy-clock.sock"
if control_socket.exists():
    with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as peer:
        peer.settimeout(3)
        try:
            peer.connect(str(control_socket))
            peer.sendall(b"save-and-quit\n")
        except ConnectionRefusedError:
            pass
    time.sleep(.3)
for c in clients:
    # The original v1 app has no control socket. Its normal close path saves QSettings.
    if Path(f'/proc/{c["pid"]}').exists():
        run('hyprctl', 'dispatch', 'hl.dsp.window.close({window = ' + json.dumps('address:' + c['address']) + '})')
    for _ in range(50):
        if not Path(f'/proc/{c["pid"]}').exists():
            break
        time.sleep(.1)
    else:
        raise SystemExit('The previous clock is still open. Its active-clock confirmation has been preserved; installation stopped before replacing it.')

paths = [config, module, startup, integration, target,
         home / '.local/share/applications/omarchy-clock.desktop',
         home / '.local/share/icons/hicolor/scalable/apps/omarchy-clock.svg']
paths += [bar_config] + [plugin_dir / p.name for p in plugin_sources if p.is_file()]
backups = {p: backup(p) for p in paths}
backup(home / '.config/OmarchyClock/Omarchy Clock.conf')
# SQLite online backup also works if a headless background instance was just stopped.
database = home / '.local/share/omarchy-clock/clock.sqlite3'
if database.exists():
    import sqlite3
    saved = backup_root / '.local/share/omarchy-clock/clock.sqlite3'
    saved.parent.mkdir(parents=True, exist_ok=True)
    with sqlite3.connect(database) as src, sqlite3.connect(saved) as dest:
        src.backup(dest)

try:
    for source, destination in [
        (binary, target),
        (project / 'omarchy-clock.desktop', paths[5]),
        (project / 'omarchy-clock.svg', paths[6]),
    ]:
        destination.parent.mkdir(parents=True, exist_ok=True)
        temporary = destination.with_name(destination.name + '.installing')
        shutil.copy2(source, temporary)
        os.replace(temporary, destination)
    # Launchers may inherit a different PATH from terminals. Use the installed
    # executable explicitly rather than relying on ~/.local/bin being present.
    desktop = paths[5]
    desktop_binary = str(target).replace('\\', '\\\\').replace('"', '\\"').replace('$', '\\$').replace('`', '\\`').replace('%', '%%')
    desktop.write_text(desktop.read_text().replace('Exec=omarchy-clock\n', f'Exec="{desktop_binary}"\n'))
    subprocess.run(['desktop-file-validate', str(desktop)], check=True)
    plugin_dir.mkdir(parents=True, exist_ok=True)
    for source in plugin_sources:
        if source.is_file():
            temporary = plugin_dir / (source.name + '.installing')
            shutil.copy2(source, temporary)
            os.replace(temporary, plugin_dir / source.name)
    bar_settings = json.loads(bar_config.read_text()) if bar_config.exists() else {}
    layout = bar_settings.setdefault('bar', {}).setdefault('layout', {})
    present = any((entry if isinstance(entry, str) else entry.get('id')) == plugin_id
                  for section in ('left', 'center', 'right') for entry in layout.get(section, []))
    # Add it once on first integration; later manual removal remains respected.
    if not present and not old_integration.get('barInstalled'):
        center = layout.setdefault('center', [])
        neighbor = next((i for i, entry in enumerate(center)
                         if (entry if isinstance(entry, str) else entry.get('id')) == 'io.github.nousd.chime'), len(center) - 1)
        center.insert(neighbor + 1, {'id': plugin_id})
        bar_config.parent.mkdir(parents=True, exist_ok=True)
        temporary = bar_config.with_name('shell.json.installing')
        temporary.write_text(json.dumps(bar_settings, indent=2) + '\n')
        os.replace(temporary, bar_config)
    contents = config.read_text()
    old_rule = '-- Omarchy Clock: freely resizable timer and stopwatch\no.window("^omarchy-clock$", { float = true, center = true, tag = "-default-opacity", opacity = "1 1" })\n'
    contents = contents.replace(old_rule, '')
    if 'require("hypr.omarchy-clock")' not in contents:
        contents = contents.rstrip() + '\n\n-- Omarchy Clock: user-owned window rules and shortcuts\nrequire("hypr.omarchy-clock")\n'
    command = shlex.quote(str(target))
    # Omarchy's o.bind accepts the standard modifier syntax used by existing bindings.
    module.write_text(
        '-- Installed by Omarchy Clock. Kept outside packaged Omarchy defaults.\n'
        'o.window("^omarchy-clock$", { float = true, center = true, tag = "-default-opacity", opacity = "1 1" })\n'
        'o.window({ class = "^omarchy-clock$", title = "^Omarchy Clock · Compact$" }, { float = true, pin = true, size = {360, 240} })\n'
        f'o.bind("SUPER + CTRL + ALT + {show_key}", "Omarchy Clock: show or hide", {json.dumps(command + " --toggle")})\n'
        f'o.bind("SUPER + CTRL + ALT + {pause_key}", "Omarchy Clock: start or pause", {json.dumps(command + " --pause-focused")})\n'
        # The desktop-file existence is the user's Settings toggle. Duplicate XDG startup is harmless.
        f'o.exec_on_start({json.dumps("test -f " + shlex.quote(str(startup)) + " && " + command + " --background")})\n'
    )
    config.write_text(contents)
    startup.parent.mkdir(parents=True, exist_ok=True)
    # Enable login startup on initial suite installation; preserve a later user opt-out.
    if startup.exists() or not old_integration:
        startup.write_text('[Desktop Entry]\nType=Application\nName=Omarchy Clock\n'
                           f'Exec="{target}" --background\nIcon=omarchy-clock\nTerminal=false\nX-GNOME-Autostart-enabled=true\n')
    integration.parent.mkdir(parents=True, exist_ok=True)
    integration.write_text(json.dumps({'showKey': show_key, 'pauseKey': pause_key,
        'showShortcut': 'Super+Ctrl+Alt+' + show_key.upper(),
        'pauseShortcut': 'Super+Ctrl+Alt+' + pause_key.capitalize(),
        'version': 2, 'barInstalled': True, 'barPlugin': plugin_id,
        'backup': str(backup_root)}, indent=2) + '\n')
    run('hyprctl', 'reload')
    errors = run('hyprctl', 'configerrors').strip()
    if errors not in ('', 'ok'):
        raise RuntimeError('Hyprland configuration errors: ' + errors)
    run('update-desktop-database', str(home / '.local/share/applications'))
    if shutil.which('gtk-update-icon-cache'):
        subprocess.run(['gtk-update-icon-cache', '-f', '-t', str(home / '.local/share/icons/hicolor')], check=False, capture_output=True)
except Exception:
    for path, saved in backups.items():
        if saved:
            shutil.copy2(saved, path)
        elif path.exists():
            path.unlink()
    subprocess.run(['hyprctl', 'reload'], check=False, capture_output=True)
    raise
print('Installed Omarchy Clock 2.1.2:', target)
print('Backups:', backup_root)
print('Show/hide:', 'Super+Ctrl+Alt+' + show_key.upper())
print('Start/pause:', 'Super+Ctrl+Alt+' + pause_key.capitalize())
print('Login startup:', 'enabled' if startup.exists() else 'disabled')
subprocess.run(['omarchy-shell', '-q', 'shell', 'rescanPlugins'], check=False, capture_output=True)
print('Bar widget:', plugin_id)
# Open the real, migrated application for the user to discover in the morning.
with open(project / 'installed-app.log', 'a') as log:
    launched = subprocess.Popen([str(target)], stdout=log, stderr=log, start_new_session=True)
for _ in range(40):
    current = json.loads(run('hyprctl', '-j', 'clients'))
    if any(c.get('class') == 'omarchy-clock' and c.get('title') == 'Omarchy Clock' for c in current):
        print('Native window: opened')
        break
    if launched.poll() is not None:
        raise SystemExit('Installed, but the app exited during startup. Check installed-app.log.')
    time.sleep(.1)
else:
    raise SystemExit('Installed, but no native window appeared. Check installed-app.log.')
