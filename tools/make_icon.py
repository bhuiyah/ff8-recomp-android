# usage: py tools/make_icon.py
# Writes the launcher icon (adaptive icon: background, foreground, monochrome) as vector drawables,
# and the same picture as docs/images/icon.svg for the README.
#
# The picture (drawn here as vectors, nothing taken from the game): a single white feather
# drifting down across a large pale moon, on a dusky orange sky. The sky, stars and moon are
# the background layer, the feather the foreground (launchers move the two apart a little). 108-unit adaptive-icon canvas; what matters sits inside the 66-unit circle.
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


# ---- the moon (MX,MY, radius R), low and large behind the feather
MX, MY, R = 58, 50, 21
MOON = G([
    (circle(0, 0, R * 1.9), ('radial', 0, 0, R * 1.9, [(0, '#66FFE6C8'), (0.5, '#26FFB070'), (1, '#00FF8040')])),   # halo
    (circle(0, 0, R), ('radial', -R * 0.3, -R * 0.35, R * 1.45, [(0, '#FFFFF1CC'), (0.55, '#FFF6C987'), (1, '#FFDC9450')])),
    (circle(-R * 0.32, R * 0.12, R * 0.2), ('solid', '#1F9A6A48')),          # maria
    (circle(R * 0.3, -R * 0.32, R * 0.14), ('solid', '#1C9A6A48')),
    (circle(R * 0.22, R * 0.42, R * 0.11), ('solid', '#1A9A6A48')),
], tx=MX, ty=MY)


# ---- a white feather drifting down across the moon, drawn along x from its quill (0) to its
# tip (L), then turned and placed
L = 46


def vane_edge(side, n=14):
    """One edge of the vane: widest a third of the way along, closing to the tip; side +1/-1."""
    pts = []
    for k in range(n + 1):
        t = k / n
        x = 6 + t * (L - 6)
        w = 7.5 * math.sin(math.pi * min(1.0, t * 1.35) ** 0.8) * (1 - 0.25 * t)
        w += 0.6 * math.sin(t * 23 + side)                                   # soft ragged edge
        pts.append((x, side * max(0.0, w)))
    return pts


top, bot = vane_edge(-1), vane_edge(1)
VANE = 'M6,0 ' + ' '.join(f'L{f2(x)},{f2(y)}' for x, y in top) + ' ' + \
       ' '.join(f'L{f2(x)},{f2(y)}' for x, y in reversed(bot)) + ' Z'
RACHIS = f'M0,0.4 L{L - 2},0'
BARBS = ' '.join(f'M{f2(x)},0 L{f2(x + 4.5)},{f2(s * 5.2 * (1 - x / L) + s * 0.6)}'
                 for x in range(10, L - 6, 4) for s in (-1, 1))
NOTCH = 'M24,-0.3 L28.5,-6.4 L29.6,-6.1 L25.6,-0.2 Z'                       # a parted gap in the vane
FEATHER_ITEMS = [
    G([(VANE, ('solid', '#663A1206'))], tx=0.9, ty=1.4),                       # soft shadow on the moon
    (VANE, ('linear', 0, -8, 0, 8, [(0, '#FFFFFFFF'), (0.5, '#FFF4ECF2'), (1, '#FFD8CCDC')])),
    (NOTCH, ('solid', '#FFE3A06A')),
    (BARBS, ('stroke', '#59B8A6BC', 0.5)),
    (RACHIS, ('stroke', '#FFFFFFFF', 1.1)),
]
FEATHER = dict(px=0, py=0, rot=-50, s=1.3, tx=36, ty=80)

STARS = ' '.join(
    f'M{f2(x)},{f2(y - r)} L{f2(x + r * 0.25)},{f2(y - r * 0.25)} L{f2(x + r)},{f2(y)} L{f2(x + r * 0.25)},{f2(y + r * 0.25)} '
    f'L{f2(x)},{f2(y + r)} L{f2(x - r * 0.25)},{f2(y + r * 0.25)} L{f2(x - r)},{f2(y)} L{f2(x - r * 0.25)},{f2(y - r * 0.25)} Z'
    for x, y, r in ((28, 26, 1.8), (86, 24, 1.3), (82, 82, 1.1)))

SKY = ('M0,0 h108 v108 h-108 Z', ('linear', 0, 0, 0, 108,
       [(0, '#FF2A0B14'), (0.4, '#FF7A2412'), (0.75, '#FFD3621A'), (1, '#FFF6A24A')]))

BACKGROUND = [SKY, (STARS, ('solid', '#D9FFF0DC')), MOON]
FOREGROUND = [G(FEATHER_ITEMS, **FEATHER)]
MONO = [G([(circle(0, 0, R), ('solid', '#FFFFFFFF'))], tx=MX, ty=MY),
        G([(VANE, ("solid", "#FFFFFFFF"))], **FEATHER)]


# ---- Android vector drawables
HEAD = '''<?xml version="1.0" encoding="utf-8"?>
<!-- FF8Recomp launcher icon ({part}): a white feather drifting across a large moon on a
     dusky orange sky (drawn as vectors, not taken from the game). Written by tools/make_icon.py;
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
                          ('foreground', 'foreground: the feather', FOREGROUND),
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
