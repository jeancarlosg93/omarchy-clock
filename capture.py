#!/usr/bin/env python3
"""Capture native in-memory states. Samples are illustrative; production is untouched."""
from pathlib import Path
import subprocess,os
from PIL import Image, ImageDraw
root=Path(__file__).resolve().parent
out=root/'.impeccable/review'
out.mkdir(parents=True,exist_ok=True)
env=os.environ|{'QT_QPA_PLATFORM':'offscreen','QT_QPA_PLATFORMTHEME':'','QT_QUICK_BACKEND':'software'}
pages=['Today','Timers','Stopwatch','Alarms','Intervals','World clocks','Tracking','History','Settings']
files=[]
for size,label in [('1150x780','desktop'),('380x460','narrow')]:
 for page in pages:
  name=f'{label}-{page.lower().replace(" ","-")}.png'
  subprocess.run([str(root/'build/omarchy-clock'),'--capture',str(out/name),'--demo','--page',page,'--size',size],env=env,check=True,capture_output=True)
  files.append(name)
 for page in ['Timers','Alarms','Intervals']:
  name=f'{label}-{page.lower()}-form.png'
  subprocess.run([str(root/'build/omarchy-clock'),'--capture',str(out/name),'--page',page,'--size',size,'--compose'],env=env,check=True,capture_output=True)
  files.append(name)
for name,args in [('focus-fullscreen.png',['--demo','--focus','stopwatch','--size','2560x1440']),('compact.png',['--demo','--page','Stopwatch','--capture-compact']),('empty-today.png',[])]:
 subprocess.run([str(root/'build/omarchy-clock'),'--capture',str(out/name),*args],env=env,check=True,capture_output=True);files.append(name)
# Contact sheets for the inspection packet; each source capture is opened and validated here.
for label in ['desktop','narrow']:
 names=[n for n in files if n.startswith(label)]
 cw,ch=(575,410) if label=='desktop' else (380,486)
 sheet=Image.new('RGB',(cw*3,ch*4),'#202024');draw=ImageDraw.Draw(sheet)
 for i,n in enumerate(names):
  im=Image.open(out/n);im.load();assert im.size[0]>0 and im.getbbox(),n
  im.thumbnail((cw,ch-26));x=(i%3)*cw;y=(i//3)*ch
  sheet.paste(im,(x,y+26));draw.text((x+8,y+7),n,fill='white')
 sheet.save(out/f'{label}-contact.png')
(out/'capture-list.txt').write_text('\n'.join(files)+'\n')
print(f'Captured {len(files)} native views.')
