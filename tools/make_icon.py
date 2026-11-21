# usage: py tools/make_icon.py
# Writes the launcher icon (adaptive icon: background, foreground, monochrome) as vector drawables,
# and the same picture as docs/images/icon.svg for the README.
#
# The picture (drawn here as vectors, nothing taken from the game): a gunblade planted in the
# ground on the left under a deep blue night sky, a large pale moon and a few stars on the right.
# The sky, stars and moon are the background layer, the blade and the ground the foreground
# (launchers move the two apart a little). 108-unit adaptive-icon canvas; what matters sits inside the 66-unit circle.
import math
import os

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..')
RES = os.path.join(ROOT, 'android', 'app', 'src', 'main', 'res')


# ---- a tiny scene model: groups (Android's transform: scale and rotate about a pivot, then
# translate) holding paths. Fills: ('solid', '#AARRGGBB'), ('stroke', '#AARRGGBB', width),
# ('linear', x1, y1, x2, y2, stops), ('radial', cx, cy, r, stops), in the path's own units.
class G:
    def __init__(self, items, px=0, py=0, rot=0, s=1, tx=0, ty=0):
        self.items, self.px, self.py, self.rot, self.s, self.tx, self.ty = items, px, py, rot, s, tx, ty


def circle(cx, cy, r):
    return f'M{cx - r:g},{cy:g} a{r:g},{r:g} 0 1,0 {2 * r:g},0 a{r:g},{r:g} 0 1,0 {-2 * r:g},0 Z'


def f2(v):
    return f'{v:.2f}'.rstrip('0').rstrip('.')


# ---- the gunblade, drawn tip up (tip y=12, grip end y=86); the planted group turns it over
STEEL = [(0, '#FF9AA3BD'), (1, '#FF3E4663')]
BLADE = 'M46.5,63 L46.5,22 L54,12 L61.5,22 L61.5,63 Z'
FULLER = 'M52.6,24 L55.4,24 L55.4,60 L52.6,60 Z'
CYL = 'M43,63 h22 a2,2 0 0,1 2,2 v6 a2,2 0 0,1 -2,2 h-22 a2,2 0 0,1 -2,-2 v-6 a2,2 0 0,1 2,-2 Z'
CHAMBERS = circle(48, 68, 1.6) + ' ' + circle(54, 68, 1.6) + ' ' + circle(60, 68, 1.6)
GRIP = 'M51,73 h7 l-1.5,13 h-6.5 Z'
GUARD_PATH = 'M50.5,73.5 q-6,1 -5.5,7'
SWORD = [
    (BLADE, ('linear', 46, 0, 62, 0, [(0, '#FF5D6787'), (0.45, '#FFD4DAE8'), (1, '#FF8C96B4')])),
    ('M58,17.4 L61.5,22 L61.5,63 L58,63 Z', ('linear', 57, 0, 62, 0, [(0, '#FFC4CBDD'), (1, '#FFF2F4FA')])),   # edge
    (FULLER, ('solid', '#663A4566')),
    (CYL, ('linear', 0, 63, 0, 73, STEEL)),
    (CHAMBERS, ('solid', '#FF151A2C')),
    (GRIP, ('linear', 51, 0, 58, 0, [(0, '#FF2A1E16'), (1, '#FF4A3828')])),
    ('M51.6,76 L57.6,77.5 M51.4,79 L57.2,80.5 M51.2,82 L56.8,83.5', ('stroke', '#FF6A5038', 0.9)),  # wrap
    (GUARD_PATH, ('stroke', '#FF7A84A2', 1.4)),
]
# planted: turned over and scaled 0.86 about (54,58), its middle moved to (42,54)
PLANTED = dict(px=54, py=58, rot=180, s=0.86, tx=-12, ty=-4)
GROUND = 'M0,84 Q30,77 54,80 T108,78 V108 H0 Z'

# ---- the moon (MX,MY, radius R) and a few stars
MX, MY, R = 70, 36, 15
MOON = G([
    (circle(0, 0, R * 2.4), ('radial', 0, 0, R * 2.4, [(0, '#55B8C8FF'), (0.45, '#226A7CD8'), (1, '#002A3070')])),   # halo
    (circle(0, 0, R), ('radial', -R * 0.3, -R * 0.3, R * 1.4, [(0, '#FFF6F4FF'), (0.6, '#FFD8DCF2'), (1, '#FFA8B0D8')])),
    (circle(-R * 0.35, R * 0.1, R * 0.22), ('solid', '#26707AA8')),            # maria
    (circle(R * 0.3, -R * 0.35, R * 0.16), ('solid', '#22707AA8')),
    (circle(R * 0.2, R * 0.45, R * 0.12), ('solid', '#1F707AA8')),
], tx=MX, ty=MY)


def star(x, y, r):
    return (f'M{f2(x)},{f2(y - r)} L{f2(x + r * 0.25)},{f2(y - r * 0.25)} L{f2(x + r)},{f2(y)} L{f2(x + r * 0.25)},{f2(y + r * 0.25)} '
            f'L{f2(x)},{f2(y + r)} L{f2(x - r * 0.25)},{f2(y + r * 0.25)} L{f2(x - r)},{f2(y)} L{f2(x - r * 0.25)},{f2(y - r * 0.25)} Z')


STARS = ' '.join(star(x, y, r) for x, y, r in ((30, 26, 2.2), (42, 40, 1.4), (90, 60, 1.6), (24, 50, 1.2), (86, 20, 1.3)))

SKY = ('M0,0 h108 v108 h-108 Z', ('linear', 0, 0, 0, 108,
       [(0, '#FF05081C'), (0.5, '#FF13204E'), (0.8, '#FF2C3A7A'), (0.95, '#FF5A4E8C')]))

BACKGROUND = [SKY, (STARS, ('solid', '#E6EEF2FF')), MOON]
FOREGROUND = [G(SWORD, **PLANTED), (GROUND, ('solid', '#FF04060E'))]
MONO = [G([(f'{BLADE}', ('solid', '#FFFFFFFF')), (f'{CYL} {GRIP}', ('solid', '#FFFFFFFF'))], **PLANTED),
        G([(circle(0, 0, R), ('solid', '#FFFFFFFF'))], tx=MX, ty=MY),
        (GROUND, ('solid', '#FFFFFFFF'))]


# ---- Android vector drawables
HEAD = '''<?xml version="1.0" encoding="utf-8"?>
<!-- FF8Recomp launcher icon ({part}): a gunblade planted under a night sky with a large moon
     (drawn as vectors, not taken from the game). Written by tools/make_icon.py;
     edit that, not this. -->
<vector xmlns:android="http://schemas.android.com/apk/res/android"
    xmlns:aapt="http://schemas.android.com/aapt"
    android:width="108dp" android:height="108dp"
    android:viewportWidth="108" android:viewportHeight="108">
'''


def a_stops(items):
    return ''.join(f'\n                <item android:offset="{o:g}" android:color="{c}"/>' for o, c in items)


def a_path(d, fill, ind):
    if fill[0] in ('solid', 'solid_eo'):
        eo = ' android:fillType="evenOdd"' if fill[0] == 'solid_eo' else ''
        return f'{ind}<path android:fillColor="{fill[1]}"{eo} android:pathData="{d}"/>\n'
    if fill[0] == 'stroke':
        return (f'{ind}<path android:strokeColor="{fill[1]}" android:strokeWidth="{fill[2]}" android:strokeLineCap="round" '
                f'android:strokeLineJoin="round" android:pathData="{d}"/>\n')
    if fill[0] == 'linear':
        _, x1, y1, x2, y2, st = fill
        g = (f'<gradient android:type="linear" android:startX="{f2(x1)}" android:startY="{f2(y1)}" '
             f'android:endX="{f2(x2)}" android:endY="{f2(y2)}">')
    else:
        _, cx, cy, rr, st = fill
        g = f'<gradient android:type="radial" android:centerX="{f2(cx)}" android:centerY="{f2(cy)}" android:gradientRadius="{f2(rr)}">'
    return f'''{ind}<path android:pathData="{d}">
{ind}    <aapt:attr name="android:fillColor">
{ind}        {g}{a_stops(st)}
{ind}        </gradient>
{ind}    </aapt:attr>
{ind}</path>
'''


def a_items(items, ind='    '):
    out = ''
    for it in items:
        if isinstance(it, G):
            attrs = ''
            for name, v, dflt in (('pivotX', it.px, 0), ('pivotY', it.py, 0), ('rotation', it.rot, 0), ('scaleX', it.s, 1),
                                  ('scaleY', it.s, 1), ('translateX', it.tx, 0), ('translateY', it.ty, 0)):
                if v != dflt:
                    attrs += f' android:{name}="{v:g}"'
            out += f'{ind}<group{attrs}>\n' + a_items(it.items, ind + '    ') + f'{ind}</group>\n'
        else:
            out += a_path(it[0], it[1], ind)
    return out


def write(path, text):
    with open(path, 'w', encoding='utf-8', newline='\n') as f:
        f.write(text)


for name, part, items in (('background', 'background: sky, stars and moon', BACKGROUND),
                          ('foreground', 'foreground: gunblade and ground', FOREGROUND),
                          ('monochrome', 'themed-icon silhouette', MONO)):
    write(os.path.join(RES, 'drawable', f'ic_launcher_{name}.xml'), HEAD.format(part=part) + a_items(items) + '</vector>\n')


# ---- docs/images/icon.svg: both layers in a rounded square
defs = []


def s_color(c):
    return '#' + c[3:], int(c[1:3], 16) / 255


def s_stops(items):
    out = ''
    for o, c in items:
        rgb, a = s_color(c)
        out += f'<stop offset="{o:g}" stop-color="{rgb}" stop-opacity="{a:.3g}"/>'
    return out


def s_path(d, fill):
    if fill[0] in ('solid', 'solid_eo'):
        rgb, a = s_color(fill[1])
        return f'<path fill="{rgb}" fill-opacity="{a:.3g}" d="{d}"/>'
    if fill[0] == 'stroke':
        rgb, a = s_color(fill[1])
        return (f'<path fill="none" stroke="{rgb}" stroke-opacity="{a:.3g}" stroke-width="{fill[2]}" '
                f'stroke-linecap="round" stroke-linejoin="round" d="{d}"/>')
    n = len(defs)
    if fill[0] == 'linear':
        _, x1, y1, x2, y2, st = fill
        defs.append(f'<linearGradient id="g{n}" gradientUnits="userSpaceOnUse" x1="{f2(x1)}" y1="{f2(y1)}" '
                    f'x2="{f2(x2)}" y2="{f2(y2)}">{s_stops(st)}</linearGradient>')
    else:
        _, cx, cy, rr, st = fill
        defs.append(f'<radialGradient id="g{n}" gradientUnits="userSpaceOnUse" cx="{f2(cx)}" cy="{f2(cy)}" '
                    f'r="{f2(rr)}">{s_stops(st)}</radialGradient>')
    return f'<path fill="url(#g{n})" d="{d}"/>'


def s_items(items):
    out = []
    for it in items:
        if isinstance(it, G):
            t = f'translate({it.tx + it.px:g} {it.ty + it.py:g}) rotate({it.rot:g}) scale({it.s:g}) translate({-it.px:g} {-it.py:g})'
            out.append(f'<g transform="{t}">' + ''.join(s_items(it.items)) + '</g>')
        else:
            out.append(s_path(it[0], it[1]))
    return out


body = s_items(BACKGROUND + FOREGROUND)
svg = ('<svg xmlns="http://www.w3.org/2000/svg" viewBox="18 18 72 72" width="216" height="216">\n'
       '<clipPath id="c"><rect x="18" y="18" width="72" height="72" rx="16"/></clipPath>\n'
       f'<defs>{"".join(defs)}</defs>\n<g clip-path="url(#c)">\n' + '\n'.join(body) + '\n</g>\n</svg>\n')
write(os.path.join(ROOT, 'docs', 'images', 'icon.svg'), svg)
print('icon written')
