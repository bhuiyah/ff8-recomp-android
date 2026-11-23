#!/usr/bin/env python3
"""The top of the game's memory, moved out of the way of Android 16's Java heap (GitHub #2).

FF8's program spans 0x00400000-0x0279F000. From Android 16, ART puts the Java heap at 0x02000000:
mapping the game there wiped the first 8 MB of it. So everything the game keeps at or above CUT
(all of it zero at start: .bss) lives DELTA higher, and every place in the program that holds such
an address is changed to match, both in the lift (run_lift.py) and in the program in memory
(src/host/ff8_tail.c). The program has no relocation table; config/tail_reloc.txt is one: the
address of each 32-bit value to change, as LEB128 differences from 0x400000 up, as decimal bytes
(a .txt so the kit takes it, and C can include it). It fits both editions' programs (1.2 US).

Constants that only look like addresses (0x02000000 is a flag) are not in it.
"""
import os, re, struct

CUT, HI, DELTA = 0x01F9DC40, 0x0279F000, 0x69180000   # game_image.h
IAT_LO, IAT_HI = 0x027C10B4, 0x027C10D4   # the 2013 release's own import slots (registry, DirectSound); the
IAT_RM_LO, IAT_RM_HI = 0x00B69000, 0x00B6D000   # Remastered's program has those in .rdata: left alone
HERE = os.path.dirname(os.path.abspath(__file__))


def sites(path=None):
    path = path or os.path.join(HERE, '..', 'config', 'tail_reloc.txt')
    text = re.sub(r'/\*.*?\*/', '', open(path).read(), flags=re.S)
    out, at, d, sh = [], 0x400000, 0, 0
    for b in text.replace(',', ' ').split():
        b = int(b)
        d |= (b & 127) << sh
        sh += 7
        if b < 128:
            at += d
            out.append(at)
            d = sh = 0
    return out


def apply(img, base):
    """The mapped image with every address in the table moved."""
    img = bytearray(img)
    for a in sites():
        v, = struct.unpack_from('<I', img, a - base)
        if IAT_RM_LO <= v < IAT_RM_HI:
            continue
        if not CUT <= v < HI and not IAT_LO <= v < IAT_HI:
            raise SystemExit(f'tail_reloc: 0x{a:08X} holds 0x{v:08X}: not the game program this was made for')
        struct.pack_into('<I', img, a - base, v + DELTA)
    return bytes(img)
