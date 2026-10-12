#!/usr/bin/env python3
"""Manual Wayland regression: reveal the installed clock without touching its data."""
import json
import os
from pathlib import Path
import subprocess
import time

APP = str(Path.home() / '.local/bin/omarchy-clock')

def query(name):
    return json.loads(subprocess.check_output(['hyprctl', '-j', name]))

def window(title='Omarchy Clock'):
    return next((c for c in query('clients') if c['class'] == 'omarchy-clock' and c['title'] == title), None)

def dispatch(expression):
    subprocess.run(['hyprctl', 'dispatch', expression], check=True, capture_output=True)
    time.sleep(.2)

def command(*args):
    subprocess.run([APP, *args], check=True, capture_output=True)

def await_check(check):
    end = time.monotonic() + 3
    while time.monotonic() < end:
        if check():
            return
        time.sleep(.1)
    raise AssertionError('Native launch condition failed; active=' + repr(query('activeworkspace')) + '; window=' + repr(window()) + '; monitors=' + repr(query('monitors')))

def on_screen(c, ws):
    if not c or c['workspace']['id'] != ws['id']:
        return False
    m = next(m for m in query('monitors') if m['name'] == ws['monitor'])
    x, y = c['at']
    width, height = c['size']
    return x >= m['x'] and y >= m['y'] and x + width <= m['x'] + m['width'] / m['scale'] and y + height <= m['y'] + m['height'] / m['scale']

def launch_desktop():
    # A GUI launcher need not inherit the terminal's ~/.local/bin PATH.
    environment = dict(os.environ, PATH='/usr/bin:/bin')
    subprocess.run(['gtk-launch', 'omarchy-clock'], env=environment, check=True, capture_output=True)

command()
await_check(lambda: window() is not None)
time.sleep(.4)
initial = window()
pid, size = initial['pid'], initial['size']
ws = query('activeworkspace')
print('launch check target:', ws['id'], ws['monitorID'])
selector = json.dumps('address:' + initial['address'])

# Reproduce the reported mapped-but-invisible off-screen window.
dispatch(f'hl.dsp.window.move({{window={selector}, x=-1500, y=350, relative=false}})')
assert not on_screen(window(), ws)
launch_desktop()
await_check(lambda: on_screen(window(), ws))
assert window()['pid'] == pid and window()['size'] == size

# Reopening an already-visible clock must preserve deliberate placement.
dispatch(f'hl.dsp.window.move({{window={selector}, x={window()["at"][0] + 25}, y={window()["at"][1] + 25}, relative=false}})')
position = window()['at']
command()
time.sleep(.4)
assert window()['at'] == position

# A launcher invocation reveals the same process on the active workspace.
# Both monitors may already show different active workspaces. Choose an inactive
# destination, then sample the launch workspace after the compositor's move.
active_workspaces = {m['activeWorkspace']['id'] for m in query('monitors')}
workspaces = query('workspaces')
other_id = next((w['id'] for w in workspaces if w['id'] > 0 and w['id'] not in active_workspaces), max(w['id'] for w in workspaces if w['id'] > 0) + 1)
dispatch(f'hl.dsp.window.move({{window={selector}, workspace="{other_id}", follow=false}})')
ws = query('activeworkspace')
assert window()['workspace']['id'] != ws['id']
launch_desktop()
await_check(lambda: on_screen(window(), ws))
assert window()['pid'] == pid and window()['size'] == size

# Foreground toggle hides; background startup does not reveal; toggle reopens.
await_check(lambda: query('activewindow').get('pid') == pid)
command('--toggle')
await_check(lambda: window() is None)
command('--background')
time.sleep(.3)
assert window() is None
command('--toggle')
await_check(lambda: on_screen(window(), ws))

# Compact remains pinned and can also recover from an invalid position.
if window('Omarchy Clock · Compact') is not None:
    command('--compact')
    await_check(lambda: window('Omarchy Clock · Compact') is None)
command('--compact')
await_check(lambda: window('Omarchy Clock · Compact') is not None)
mini = window('Omarchy Clock · Compact')
assert mini['pinned'] and mini['floating']
mini_selector = json.dumps('address:' + mini['address'])
dispatch(f'hl.dsp.window.move({{window={mini_selector}, x=-1500, y=350, relative=false}})')
command('--compact')
await_check(lambda: window('Omarchy Clock · Compact') is None)
command('--compact')
await_check(lambda: on_screen(window('Omarchy Clock · Compact'), ws))
assert window('Omarchy Clock · Compact')['pinned']
# Main Ctrl+W closes to tray; the compact stays mapped and pinned.
command()
await_check(lambda: query('activewindow').get('title') == 'Omarchy Clock')
time.sleep(.25)
subprocess.run(['wtype', '-M', 'ctrl', '-k', 'w', '-m', 'ctrl'], check=True)
await_check(lambda: window() is None)
assert window('Omarchy Clock · Compact')['pinned']
# The same hide path used by Hide to tray affects only the main window.
command()
await_check(lambda: query('activewindow').get('title') == 'Omarchy Clock')
time.sleep(.25)
command('--toggle')
await_check(lambda: window() is None)
assert window('Omarchy Clock · Compact') is not None
command()
await_check(lambda: window() is not None and query('activewindow').get('title') == 'Omarchy Clock')
time.sleep(.25)
mini_selector = json.dumps('address:' + window('Omarchy Clock · Compact')['address'])
dispatch(f'hl.dsp.focus({{window={mini_selector}}})')
await_check(lambda: query('activewindow').get('title') == 'Omarchy Clock · Compact')
subprocess.run(['wtype', '-M', 'ctrl', '-k', 'w', '-m', 'ctrl'], check=True)
await_check(lambda: window('Omarchy Clock · Compact') is None)
assert window() is not None
command()
await_check(lambda: on_screen(window(), ws) and query('activewindow').get('pid') == pid)
print('PASS: desktop launcher with minimal PATH; off-screen recovery; current workspace; position/size preserved; single instance; show/hide; hidden background; pinned compact recovery; independent main/compact Ctrl+W and hiding.')
