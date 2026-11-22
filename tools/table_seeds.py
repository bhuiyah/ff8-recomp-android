"""Function seeds from the game's own function-pointer tables (FF8).

Recursive descent from the entry finds nearly every function, but not one that is only ever
reached through a table in .rdata/.data that nothing else points into: the field script's
opcode table (0xB8DE4C, 394 handlers) had two such (0x522010, 0x523030), and the Remastered's
Lunatic Pandora field stopped on the first ("ICALL to unlifted VA 0x00523030").

A table here is a run of at least 6 aligned dwords that all point into .text, of which at
least 80% are already catalogued functions: a run like that is a table of functions, and its
other entries are functions too. Those are added to analysis/seeds.json (kept with whatever
seeds it has); then rebuild the catalog:

usage: py -I tools/table_seeds.py [game/ff8_en.exe] [config/catalog.json.gz]
       py -I tools/run_lift.py catalog game/ff8_en.exe analysis/seeds.json analysis/catalog.json
"""
import gzip
import json
import os
import struct
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, 'analysis', 'seeds.json')
TEXT_START, TEXT_END = 0x401000, 0xB69000   # ff8_en .text (src/host/game_image.h)
MIN_RUN, MIN_KNOWN = 6, 0.8


def sections(d):
    pe = struct.unpack_from('<I', d, 0x3C)[0]
    n = struct.unpack_from('<H', d, pe + 6)[0]
    opt = struct.unpack_from('<H', d, pe + 20)[0]
    base = struct.unpack_from('<I', d, pe + 24 + 28)[0]
    out = []
    for i in range(n):
        s = pe + 24 + opt + 40 * i
        name = d[s:s + 8].rstrip(b'\0').decode('ascii', 'replace')
        vsize, va, rsize, raw = struct.unpack_from('<IIII', d, s + 8)
        out.append((name, base + va, min(vsize, rsize), raw))
    return base, out


def main():
    exe = sys.argv[1] if len(sys.argv) > 1 else os.path.join(ROOT, 'game', 'ff8_en.exe')
    cat_path = sys.argv[2] if len(sys.argv) > 2 else os.path.join(ROOT, 'config', 'catalog.json.gz')
    d = open(exe, 'rb').read()
    cat = json.load(gzip.open(cat_path, 'rt') if cat_path.endswith('.gz') else open(cat_path))
    known = {f['address'] if isinstance(f['address'], int) else int(f['address'], 16) for f in cat['functions']}
    _, secs = sections(d)
    found = {}
    for name, va, size, raw in secs:
        if name not in ('.rdata', '.data'):
            continue
        k = 0
        while k + 4 <= size:
            run = []
            while k + 4 <= size:
                v = struct.unpack_from('<I', d, raw + k)[0]
                if not TEXT_START <= v < TEXT_END:
                    break
                run.append(v)
                k += 4
            if not run:
                k += 4
                continue
            if len(run) >= MIN_RUN:
                have = sum(1 for v in run if v in known)
                if have >= MIN_KNOWN * len(run):
                    for v in run:
                        if v not in known:
                            found.setdefault(v, va + k - 4 * len(run))
    seeds = json.load(open(OUT)) if os.path.exists(OUT) else []
    have = {int(s['address'], 16) for s in seeds}
    for v, table in sorted(found.items()):
        print(f'0x{v:08X}  (table at 0x{table:08X})')
        if v not in have:
            seeds.append({'address': f'0x{v:08X}', 'name': f'table_fn_{v:08X}', 'source': f'table 0x{table:08X}'})
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    json.dump(seeds, open(OUT, 'w', encoding='utf-8'), indent=1)
    print(f'{len(found)} table functions not catalogued; {len(seeds)} seeds -> {os.path.relpath(OUT, ROOT)}')


if __name__ == '__main__':
    main()
