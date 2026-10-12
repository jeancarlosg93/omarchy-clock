#!/usr/bin/env python3
"""Capture map and open dropdowns in isolated light/dark themes; no production writes."""
import os
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parent
out = root / '.impeccable/review'
files = []
for mode,bg,fg,accent,muted in [('light','#eff1f5','#4c4f69','#1e66f5','#5f646a'),('dark','#080a0e','#ffffff','#5573e2','#bfbfbf')]:
    with tempfile.TemporaryDirectory(prefix='clock-review-theme-') as folder:
        palette=Path(folder)/'omarchy/current/theme'
        palette.mkdir(parents=True)
        (palette/'colors.toml').write_text(f'background = "{bg}"\nforeground = "{fg}"\naccent = "{accent}"\nmuted = "{muted}"\n')
        env=os.environ|{'XDG_STATE_HOME':folder,'QT_QPA_PLATFORM':'offscreen','QT_QPA_PLATFORMTHEME':'','QT_QUICK_BACKEND':'software'}
        states=[('map',['--page','World clocks','--size','1150x780','--map-zone','America/Santo_Domingo']),('world-popup',['--page','World clocks','--size','1150x780','--compose','--map-zone','America/Santo_Domingo','--open-choice','zoneChoice']),('narrow-map',['--page','World clocks','--size','380x460','--map-zone','America/Santo_Domingo']),('settings-popup',['--page','Settings','--size','1150x780','--open-choice','precisionChoice'])]
        for state,args in states:
            name=f'extension-{mode}-{state}.png'
            result=subprocess.run([str(root/'build/omarchy-clock'),'--capture',str(out/name),'--demo',*args],env=env,capture_output=True,text=True,check=True)
            assert not result.stderr.strip(),result.stderr
            files.append(name)
(out/'extension-capture-list.txt').write_text('\n'.join(files+['bar-live.png','bar-timer.png','extension-native-world-popup.png'])+'\n')
print(f'Captured {len(files)} light/dark map and open-popup states.')
