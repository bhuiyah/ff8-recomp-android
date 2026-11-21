"""Check game/ against what the pipeline expects and record hashes in analysis/inputs.json.

The exe must be the unwrapped 32-bit FF8 1.2 US (Steam 2013) build FFNx knows: version bytes at
0x401004/0x401404 = VERSION_FF8_12_US_NV, entry 0x55ADB7 (tools/unwrap_steamstub.py), AF3DN.P import.
Every data file is hashed so later runs can tell if the inputs changed.

usage: py -I tools/check_inputs.py [--quick]   (--quick skips hashing the data folder)
"""
import hashlib
import json
import os
import struct
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
GAME = os.path.join(ROOT, 'game')
OUT = os.path.join(ROOT, 'analysis', 'inputs.json')

EXPECT_ENTRY_VA = 0x0055ADB7      # mainCRTStartup, after unwrapping SteamStub
EXPECT_VERSION = (0x3885048D, 0x001597C8)   # FFNx common.cpp: VERSION_FF8_12_US_NV
EXPECT_IMAGE_BASE = 0x00400000
# Files and folders under Data/ that the game reads; missing ones mean an incomplete install.
EXPECT_DATA = ['lang-en/battle.fs', 'lang-en/field.fs', 'lang-en/magic.fs', 'lang-en/main.fs',
               'lang-en/menu.fs', 'lang-en/world.fs', 'Sound/audio.dat', 'Sound/audio.fmt',
               'Music/dmusic', 'movies']


def sha1(path):
    h = hashlib.sha1()
    with open(path, 'rb') as f:
        for chunk in iter(lambda: f.read(1 << 20), b''):
            h.update(chunk)
    return h.hexdigest()


def check_exe(path, errors):
    b = open(path, 'rb').read(4096)
    pe = struct.unpack_from('<I', b, 0x3C)[0]
    machine = struct.unpack_from('<H', b, pe + 4)[0]
    entry, = struct.unpack_from('<I', b, pe + 24 + 16)
    base, = struct.unpack_from('<I', b, pe + 24 + 28)
    if machine != 0x14C:
        errors.append(f'ff8_en.exe is not x86-32 (machine 0x{machine:x})')
    if base != EXPECT_IMAGE_BASE or base + entry != EXPECT_ENTRY_VA:
        errors.append(f'ff8_en.exe entry 0x{base + entry:08x} is not the unwrapped 1.2 US build (want 0x{EXPECT_ENTRY_VA:08x})')
    full = open(path, 'rb').read()
    v = struct.unpack_from('<I', full, 0x1004)[0], struct.unpack_from('<I', full, 0x1404)[0]
    if v != EXPECT_VERSION:
        errors.append(f'ff8_en.exe version bytes {v[0]:#x}/{v[1]:#x} are not FF8 1.2 US (Steam 2013)')
    if b'AF3DN.P' not in full:
        errors.append('ff8_en.exe does not import AF3DN.P')


def main():
    quick = '--quick' in sys.argv
    errors = []
    exe = os.path.join(GAME, 'ff8_en.exe')
    data = os.path.join(GAME, 'Data')
    if not os.path.isfile(exe):
        errors.append('game/ff8_en.exe missing (run tools/import_game.py)')
    if not os.path.isdir(data):
        errors.append('game/Data missing (run tools/import_game.py)')
    if errors:
        sys.exit('\n'.join('FAIL ' + e for e in errors))

    check_exe(exe, errors)
    for d in EXPECT_DATA:
        if not os.path.exists(os.path.join(data, d)):
            errors.append(f'game/Data/{d} missing')

    result = {'ff8_en.exe': {'sha1': sha1(exe), 'size': os.path.getsize(exe)}, 'data': {}}
    if not quick:
        n = 0
        for dirpath, _, files in os.walk(data):
            for name in sorted(files):
                p = os.path.join(dirpath, name)
                rel = os.path.relpath(p, data).replace('\\', '/')
                result['data'][rel] = {'sha1': sha1(p), 'size': os.path.getsize(p)}
                n += 1
                if n % 250 == 0:
                    print(f'  hashed {n} data files', flush=True)

    if os.path.exists(OUT):
        old = json.load(open(OUT, encoding='utf-8'))
        if old.get('ff8_en.exe') != result['ff8_en.exe']:
            errors.append('ff8_en.exe differs from the one recorded in analysis/inputs.json')

    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    if not errors:
        if quick and os.path.exists(OUT):
            result['data'] = json.load(open(OUT, encoding='utf-8')).get('data', {})
        json.dump(result, open(OUT, 'w', encoding='utf-8'), indent=1)

    print(f"ff8_en.exe sha1 {result['ff8_en.exe']['sha1']}")
    print(f"data files hashed: {len(result['data'])}")
    if errors:
        sys.exit('\n'.join('FAIL ' + e for e in errors))
    print('PASS')


if __name__ == '__main__':
    main()
