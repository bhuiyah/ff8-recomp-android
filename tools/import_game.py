"""Import the user's FF8 (Steam 2013 release, app 39150) into game/.

Copies FF8_EN.exe, removes its SteamStub wrapper (tools/unwrap_steamstub.py) and writes
game/ff8_en.exe; links game/Data to the install's Data folder (a directory junction on Windows,
so the data is not duplicated). Nothing is modified in the install itself.

usage: py -I tools/import_game.py "<path to the FINAL FANTASY VIII install>"
"""
import json
import os
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
GAME = os.path.join(ROOT, 'game')
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from unwrap_steamstub import unwrap  # noqa: E402


def main(install):
    install = os.path.abspath(install)
    exe = os.path.join(install, 'FF8_EN.exe')
    data = os.path.join(install, 'Data')
    for p in (exe, data):
        if not os.path.exists(p):
            sys.exit(f'missing: {p}\nIs this the FINAL FANTASY VIII (Steam 2013) install folder?')

    os.makedirs(GAME, exist_ok=True)
    out = os.path.join(GAME, 'ff8_en.exe')
    open(out, 'wb').write(unwrap(open(exe, 'rb').read()))
    print('unwrapped FF8_EN.exe -> game/ff8_en.exe')

    link = os.path.join(GAME, 'Data')
    if os.path.lexists(link):
        if os.path.realpath(link) != os.path.realpath(data):
            sys.exit(f'game/Data already exists and points elsewhere: {os.path.realpath(link)}')
        print('game/Data already linked')
    elif os.name == 'nt':
        subprocess.run(['cmd', '/c', 'mklink', '/J', link, data], check=True, capture_output=True)
        print(f'linked game/Data -> {data}')
    else:
        os.symlink(data, link)
        print(f'linked game/Data -> {data}')

    with open(os.path.join(GAME, 'source.json'), 'w', encoding='utf-8') as f:
        json.dump({'install': install, 'edition': 'steam-2013', 'steam_appid': 39150}, f, indent=2)


if __name__ == '__main__':
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    main(sys.argv[1])
