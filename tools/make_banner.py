# usage: py tools/make_banner.py
# Writes docs/images/banner.svg and banner.png (1280x640, GitHub's social-preview size): the app
# icon (docs/images/icon.svg, from tools/make_icon.py) on the left, the title and a few lines on
# the right, as FF7Recomp's and FF9Recomp's banners are laid out. The PNG is rendered with
# headless Microsoft Edge (Windows: the Bahnschrift font).
import os
import re
import subprocess

ROOT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))
IMG = os.path.join(ROOT, 'docs', 'images')
EDGE = r'C:\Program Files (x86)\Microsoft\Edge\Application\msedge.exe'

ACCENT, ACCENT_DEEP, WHITE = '#FF7A8C', '#B3203E', '#F4F0F2'   # FF8: the icon's crimson, lifted to read on the dark
FONT = "Bahnschrift, 'Segoe UI', Arial, sans-serif"

# the icon, nested: its gradient and clip ids get a prefix so they cannot clash
icon = open(os.path.join(IMG, 'icon.svg'), encoding='utf-8').read()
inner = icon[icon.index('>', icon.index('<svg')) + 1:icon.rindex('</svg>')]
inner = re.sub(r'id="([^"]+)"', r'id="i-\1"', inner)
inner = re.sub(r'url\(#([^)]+)\)', r'url(#i-\1)', inner)
IX, IY, IS = 96, 166, 308   # icon box

svg = f'''<svg xmlns="http://www.w3.org/2000/svg" width="1280" height="640" viewBox="0 0 1280 640">
<defs>
 <linearGradient id="bg" x1="0" y1="0" x2="0" y2="640" gradientUnits="userSpaceOnUse">
  <stop offset="0" stop-color="#0B080C"/><stop offset="0.6" stop-color="#1A0B13"/><stop offset="1" stop-color="#3E0C1C"/>
 </linearGradient>
 <radialGradient id="ember" cx="250" cy="640" r="560" gradientUnits="userSpaceOnUse">
  <stop offset="0" stop-color="#FF3C5A" stop-opacity="0.2"/><stop offset="1" stop-color="#FF3C5A" stop-opacity="0"/>
 </radialGradient>
 <filter id="glow" x="-30%" y="-30%" width="160%" height="160%"><feGaussianBlur stdDeviation="18"/></filter>
 <filter id="tglow" x="-20%" y="-40%" width="140%" height="180%"><feGaussianBlur stdDeviation="9"/></filter>
</defs>
<rect width="1280" height="640" fill="url(#bg)"/>
<rect width="1280" height="640" fill="url(#ember)"/>
<rect x="{IX}" y="{IY}" width="{IS}" height="{IS}" rx="{IS * 0.22:g}" fill="{ACCENT_DEEP}" opacity="0.6" filter="url(#glow)"/>
<svg x="{IX}" y="{IY}" width="{IS}" height="{IS}" viewBox="18 18 72 72">{inner}</svg>
<rect x="{IX}" y="{IY}" width="{IS}" height="{IS}" rx="{IS * 0.22:g}" fill="none" stroke="{ACCENT}" stroke-opacity="0.35" stroke-width="2"/>
<text x="470" y="262" font-family="{FONT}" font-weight="700" font-size="98" fill="{WHITE}" font-stretch="semi-condensed">FINAL FANTASY</text>
<text x="1078" y="262" font-family="{FONT}" font-weight="700" font-size="98" fill="{ACCENT}" font-stretch="semi-condensed" opacity="0.8" filter="url(#tglow)">VIII</text>
<text x="1078" y="262" font-family="{FONT}" font-weight="700" font-size="98" fill="{ACCENT}" font-stretch="semi-condensed">VIII</text>
<text x="473" y="334" font-family="{FONT}" font-weight="600" font-size="36" letter-spacing="12" fill="{ACCENT}">NATIVE ANDROID RECOMP</text>
<text x="472" y="398" font-family="{FONT}" font-size="27" fill="{WHITE}" opacity="0.85">Either Steam edition, recompiled to native ARM64. No emulator.</text>
<text x="472" y="444" font-family="{FONT}" font-size="24" fill="{ACCENT}">AYN Thor dual screen · Junction VIII mods · bring your own game</text>
</svg>
'''
svg_path = os.path.join(IMG, 'banner.svg')
with open(svg_path, 'w', encoding='utf-8', newline='\n') as f:
    f.write(svg)

# render: an HTML page holding the SVG inline, screenshotted at exactly 1280x640
page = os.path.join(IMG, '_banner_render.html')
with open(page, 'w', encoding='utf-8') as f:
    f.write('<!doctype html><html><body style="margin:0;overflow:hidden">' + svg + '</body></html>')
png = os.path.join(IMG, 'banner.png')
if os.path.exists(png):
    os.remove(png)
subprocess.run([EDGE, '--headless=new', '--disable-gpu', '--hide-scrollbars', f'--screenshot={png}',
                '--window-size=1280,640', 'file:///' + page.replace(os.sep, '/')], check=True,
               stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
# msedge.exe can hand the job to an Edge already running and return at once: wait for the picture
import time
for _ in range(60):
    if os.path.exists(png) and os.path.getsize(png) > 0:
        time.sleep(0.5)
        break
    time.sleep(0.5)
os.remove(page)
if not os.path.exists(png):
    raise SystemExit('Edge wrote no banner.png')
print('banner written:', svg_path, png)
