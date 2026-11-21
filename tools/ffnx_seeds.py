"""Function seeds for the catalog from FFNx's patch manifests (FF8).

FFNx names FF8's functions by walking code from the entry point, so recursive descent from
the entry reaches nearly all of them. But FFNx also replaces some functions that start in the
middle of what the catalog saw as one function (a body the compiler shares, or code reached
only by falling through): such a replace_function site has to be a function entry, or the
hook cannot be installed (the run reports it as "mid-function"). Every replace_function site
in config/ffnx_manifests/*.json (tools/refresh_manifests.sh) becomes a seed.

Writes analysis/seeds.json: [{"address": "0x...", "name": "...", "source": "ffnx-manifest"}].
usage: py -I tools/ffnx_seeds.py
"""
import glob
import json
import os

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, 'analysis', 'seeds.json')
TEXT_START, TEXT_END = 0x401000, 0xB69000   # ff8_en .text (src/host/game_image.h)


def main():
    sites = set()
    for path in sorted(glob.glob(os.path.join(ROOT, 'config', 'ffnx_manifests', '*.json'))):
        for h in json.load(open(path, encoding='utf-8')).get('hooks', []):
            if h.get('kind') == 'replace_function':
                a = int(h['site'], 16)
                if TEXT_START <= a < TEXT_END:
                    sites.add(a)
    seeds = [{'address': f'0x{a:08X}', 'name': f'ffnx_hook_{a:08X}', 'source': 'ffnx-manifest'}
             for a in sorted(sites)]
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    json.dump(seeds, open(OUT, 'w', encoding='utf-8'), indent=1)
    print(f'{len(seeds)} seeds -> {os.path.relpath(OUT, ROOT)}')


if __name__ == '__main__':
    main()
