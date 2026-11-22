# usage: py tools/make_icon.py
# Writes the launcher icon (adaptive icon: background, foreground, monochrome) as vector drawables,
# and the same picture as docs/images/icon.svg for the README.
#
# The picture (drawn here as vectors, nothing taken from the game): a gunblade, a revolver's grip
# and cylinder with a long single-edged blade, raised across a deep crimson night with a faint cut
# of light behind it, and a white feather drifting down past it. Red keeps it apart from FF7Recomp's cyan and FF9Recomp's gold. The sky,
# glow and sparks are the background layer, the gunblade the foreground (launchers move the two
# apart a little). 108-unit adaptive-icon canvas; what matters sits inside the 66-unit circle.
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


# ---- a gunblade, drawn along x from the grip's end (0) to the blade's tip (82), y down;
# then turned and placed. A revolver's grip, trigger and cylinder, and a long single-edged blade.
BLADE = 'M27,-3.6 L82,-3.6 L91,-0.4 L88,1.1 L81,3.6 L27,3.6 Z'
SPINE = 'M27,-3.6 L82,-3.6 L87,-1.8 L27,-1.8 Z'                 # the back of the blade, darker
FULLER = 'M31,-0.4 L76,-0.4'                                      # the groove along it
EDGE = 'M27,2.8 L80.6,2.8 L87.4,0.8'                              # the bright cutting edge
RECEIVER = 'M17,-6.4 L29,-6.4 Q30.4,-6.4 30.4,-5 L30.4,5 Q30.4,6.4 29,6.4 L17,6.4 Q15.6,6.4 15.6,5 L15.6,-5 Q15.6,-6.4 17,-6.4 Z'
CYLINDER = circle(22.6, -0.6, 4.6)
CHAMBERS = ' '.join(circle(22.6 + 2.6 * math.cos(a), -0.6 + 2.6 * math.sin(a), 0.85)
                    for a in (k * math.pi / 3 for k in range(6)))
GRIP = 'M15.8,3.6 L21.4,5.4 L16.4,18.4 Q15.6,20.6 13.4,20.2 L10.2,19.4 Q8.2,18.8 9,16.6 Z'
GRIP_WRAP = ' '.join(f'M{f2(14.8 - 1.1 * k)},{f2(6.4 + 3 * k)} L{f2(20.2 - 1.1 * k)},{f2(8.2 + 3 * k)}' for k in range(4))
GUARD = 'M20.4,6.4 Q20.2,12.2 25.4,12 Q28.6,11.8 28.4,6.4'           # the trigger guard
TRIGGER = 'M23.4,6.4 Q23,8.8 24.6,9.8'
RING = circle(12.2, 22.6, 1.9)                                       # the pommel's ring

STEEL = ('linear', 0, -3.6, 0, 3.6, [(0, '#FF8A93A6'), (0.35, '#FFD4D9E3'), (0.62, '#FFF2F5FA'), (1, '#FFB9C0CE')])
GUNMETAL = ('linear', 0, -6.4, 0, 6.4, [(0, '#FF59606F'), (0.5, '#FF2C313C'), (1, '#FF15181F')])
GUNBLADE_ITEMS = [
    G([(BLADE, ('solid', '#80000000')), (RECEIVER, ('solid', '#80000000')), (GRIP, ('solid', '#80000000'))], tx=1.2, ty=1.8),  # shadow
    (BLADE, STEEL),
    (SPINE, ('solid', '#5C3A4254')),
    (FULLER, ('stroke', '#8C5C6577', 0.9)),
    (EDGE, ('stroke', '#FFFFFFFF', 0.7)),
    (GRIP, ('linear', 0, 3, 0, 18, [(0, '#FF3A2A2C'), (1, '#FF140C0E')])),
    (GRIP_WRAP, ('stroke', '#66E8D7C9', 0.55)),
    (RING, ('stroke', '#FFC9CED8', 0.9)),
    (GUARD, ('stroke', '#FFAEB5C2', 1.1)),
    (TRIGGER, ('stroke', '#FFAEB5C2', 0.9)),
    (RECEIVER, GUNMETAL),
    (CYLINDER, ('radial', 21.4, -2.2, 5.6, [(0, '#FFE6EAF1'), (0.55, '#FF9AA2B2'), (1, '#FF4A5160')])),
    (CHAMBERS, ('solid', '#FF20242C')),
    (circle(22.6, -0.6, 1.1), ('solid', '#FF5A6272')),
]
GUNBLADE = dict(px=50, py=4, rot=-42, s=0.74, tx=6, ty=49)

# ---- a white feather drifting down past the blade (the first icon's), drawn along x from its quill (0) to its
# tip (L), then turned and placed
L = 46


def vane_edge(side, n=18):
    """One edge of the vane, from the end of the bare quill (x 9) to the tip: widest a little
    before the middle, tapering to a point; side +1/-1."""
    pts = []
    for k in range(n + 1):
        t = k / n
        x = 9 + t * (L - 9)
        w = 7.2 * math.sin(math.pi * t ** 0.85) ** 0.75 * (1 - 0.2 * t)
        w += 0.45 * math.sin(t * 23 + side) * math.sin(math.pi * t)        # soft ragged edge
        pts.append((x, side * max(0.0, w)))
    return pts


top, bot = vane_edge(-1), vane_edge(1)
VANE = 'M9,0 ' + ' '.join(f'L{f2(x)},{f2(y)}' for x, y in top) + ' ' + \
       ' '.join(f'L{f2(x)},{f2(y)}' for x, y in reversed(bot)) + ' Z'
RACHIS = f'M0,0.4 L{L - 1},0'
BARBS = ' '.join(f'M{f2(x)},0 L{f2(x + 4)},{f2(s * 4.6 * math.sin(math.pi * (x + 4 - 9) / (L - 9)) ** 0.75)}'
                 for x in range(13, L - 9, 4) for s in (-1, 1))
NOTCH = 'M24,-0.3 L28.5,-6.4 L29.6,-6.1 L25.6,-0.2 Z'                       # a parted gap in the vane
FEATHER_ITEMS = [
    G([(VANE, ('solid', '#66140208'))], tx=0.9, ty=1.4),                         # soft shadow on the sky
    (VANE, ('linear', 0, -8, 0, 8, [(0, '#FFFFFFFF'), (0.5, '#FFF4ECF2'), (1, '#FFD8CCDC')])),
    (NOTCH, ('solid', '#FFE9C3CC')),
    (BARBS, ('stroke', '#59B8A6BC', 0.5)),
    (RACHIS, ('stroke', '#FFFFFFFF', 1.1)),
]
FEATHER = dict(px=0, py=0, rot=-38, s=0.9, tx=25, ty=58)    # in the open sky above the blade

SPARKS = ' '.join(
    f'M{f2(x)},{f2(y - r)} L{f2(x + r * 0.25)},{f2(y - r * 0.25)} L{f2(x + r)},{f2(y)} L{f2(x + r * 0.25)},{f2(y + r * 0.25)} '
    f'L{f2(x)},{f2(y + r)} L{f2(x - r * 0.25)},{f2(y + r * 0.25)} L{f2(x - r)},{f2(y)} L{f2(x - r * 0.25)},{f2(y - r * 0.25)} Z'
    for x, y, r in ((30, 32, 1.6), (80, 30, 2.2), (78, 80, 1.2)))

SKY = ('M0,0 h108 v108 h-108 Z', ('linear', 0, 0, 108, 108,
       [(0, '#FF12030A'), (0.5, '#FF4F0A1A'), (1, '#FF9C1530')]))
GLOW = (circle(54, 54, 34), ('radial', 54, 54, 34, [(0, '#73FF4D66'), (0.6, '#21FF2A4A'), (1, '#00FF2A4A')]))
SLASH = ('M30,74 Q54,58 80,40', ('stroke', '#59FFD1DA', 0.8))      # a thin cut of light behind the blade

BACKGROUND = [SKY, GLOW, SLASH, (SPARKS, ('solid', '#D9FFE4EA'))]
FOREGROUND = [G(FEATHER_ITEMS, **FEATHER), G(GUNBLADE_ITEMS, **GUNBLADE)]
MONO = [G([(VANE, ('solid', '#FFFFFFFF'))], **FEATHER),
        G([(BLADE, ('solid', '#FFFFFFFF')), (RECEIVER, ('solid', '#FFFFFFFF')), (GRIP, ('solid', '#FFFFFFFF')),
           (GUARD, ('stroke', '#FFFFFFFF', 1.1))], **GUNBLADE)]


# ---- Android vector drawables
HEAD = '''<?xml version="1.0" encoding="utf-8"?>
<!-- FF8Recomp launcher icon ({part}): a gunblade and a white feather on a deep crimson night (drawn as
     vectors, not taken from the game). Written by tools/make_icon.py;
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


for name, part, items in (('background', 'background: sky, glow and sparks', BACKGROUND),
                          ('foreground', 'foreground: the gunblade and the feather', FOREGROUND),
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
