#!/usr/bin/env python3
"""Manual native shell test using an in-memory app; restores the real app afterward."""
import json
import os
from pathlib import Path
import subprocess
import socket
import time

project = Path(__file__).resolve().parent
app = str(Path.home() / '.local/bin/omarchy-clock')

def call(*args):
    return subprocess.check_output(args, text=True).strip()

def status():
    return json.loads(call('omarchy-shell', 'omarchy-clock-bar', 'status'))

def wait_for(check, seconds=4):
    end = time.monotonic() + seconds
    while time.monotonic() < end:
        if check(): return
        time.sleep(.1)
    raise AssertionError('Native bar check timed out')

def clients():
    return [c for c in json.loads(call('hyprctl', '-j', 'clients')) if c['class']=='omarchy-clock']

subprocess.Popen([app], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
wait_for(lambda: any(c['title']=='Omarchy Clock' for c in clients()))
previous_pid = next(c['pid'] for c in clients() if c['title']=='Omarchy Clock')
subprocess.run([app, '--save-and-quit'], check=True)
wait_for(lambda: not Path(f'/proc/{previous_pid}').exists())
log = open(project/'bar-test.log', 'w')
fixture = subprocess.Popen([app, '--test-bar', '--page', 'Stopwatch', '--focus', 'stopwatch'], stdout=log, stderr=log)
try:
    wait_for(lambda: status().get('visible') is True)
    before = status()
    assert before['fresh'] and before['state']['pid'] == fixture.pid
    assert before['state']['runningCount'] >= 2
    # Stream commands must wait for a newline and acknowledge actual handling.
    with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as peer:
        peer.settimeout(.15)
        peer.connect(str(Path(os.environ['XDG_RUNTIME_DIR'])/'omarchy-clock.sock'))
        peer.sendall(b'back')
        try:
            premature = peer.recv(32)
            raise AssertionError('Partial command was handled: ' + repr(premature))
        except TimeoutError:
            pass
        peer.sendall(b'ground\n')
        peer.settimeout(2)
        assert peer.recv(32) == b'ok\n'
    advertised = before['state']['clock']['id']
    initial_window = next(c for c in clients() if c['title']=='Omarchy Clock')
    initial_monitor = next(m for m in json.loads(call('hyprctl','-j','monitors')) if m['id']==initial_window['monitor'])
    initial_geometry = f'{initial_monitor["x"]+max(0,initial_monitor["width"]//2-800)},{initial_monitor["y"]} 1600x34'
    subprocess.run(['grim','-g',initial_geometry,str(project/'.impeccable/review/bar-timer.png')],check=True)
    call('omarchy-shell', 'omarchy-clock-bar', 'pause')
    wait_for(lambda: status()['state']['runningCount'] < before['state']['runningCount'])
    assert advertised not in [c['id'] for c in status()['state']['clocks']]
    for _ in range(10):
        if status()['state']['runningCount'] == 0: break
        current = status()['state']['runningCount']
        call('omarchy-shell', 'omarchy-clock-bar', 'pause')
        wait_for(lambda: status()['state']['runningCount'] < current)
    assert status()['state']['runningCount'] == 0
    assert 'paused' in status()['label']
    call('omarchy-shell', 'omarchy-clock-bar', 'pause')
    wait_for(lambda: status()['state']['runningCount'] == 1)
    call('omarchy-shell', 'omarchy-clock-bar', 'pause')
    wait_for(lambda: status()['state']['runningCount'] == 0)
    subprocess.run([app, '--pause-focused'], check=True)
    wait_for(lambda: status()['state']['clock'].get('kind') == 'stopwatch' and status()['state']['runningCount'] == 1)
    call('omarchy-shell', 'omarchy-clock-bar', 'open')
    wait_for(lambda: any(c['pid'] == fixture.pid for c in clients()))
    call('omarchy-shell', 'omarchy-clock-bar', 'compact')
    wait_for(lambda: any('Compact' in c['title'] and c['pinned'] for c in clients()))
    subprocess.run([app, '--compact'], check=True)
    wait_for(lambda: not any('Compact' in c['title'] for c in clients()))
    # Native evidence: the central bar strip on the application's monitor.
    main = next(c for c in clients() if c['title']=='Omarchy Clock')
    monitor = next(m for m in json.loads(call('hyprctl','-j','monitors')) if m['id']==main['monitor'])
    out = project/'.impeccable/review/bar-live.png'
    geometry = f'{monitor["x"]+max(0,monitor["width"]//2-800)},{monitor["y"]} 1600x34'
    subprocess.run(['grim','-g',geometry,str(out)],check=True)
    print(json.dumps({'initialLabel':before['label'],'runningClocks':before['state']['runningCount'],'streamCommandAcknowledgement':'passed','pauseAdvertised':'passed','resumePaused':'passed','open':'passed','compactPinned':'passed','finalSampleLabel':status()['label'],'nativeCapture':str(out)},indent=2))
finally:
    subprocess.run([app, '--save-and-quit'], check=False)
    fixture.wait(timeout=5)
    log.close()
    # Restore the user's persisted application, not the sample backend.
    with open(project/'installed-app.log','a') as real_log:
        restored = subprocess.Popen([app], stdout=real_log, stderr=real_log, start_new_session=True)
    wait_for(lambda: any(c['pid']==restored.pid for c in clients()))
