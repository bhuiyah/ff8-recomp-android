"""Guest memory out of an FF8Recomp save state (src/host/ff8_savestate.c), to look up game data
on the PC: the header, then a zlib stream of records {u32 id, u32 size, data}; memory records
(id R_MEM) carry {u32 guest address} then the bytes.

usage: py -I tools/savestate_mem.py <slotNN.ff7s> read <hex addr> <len>
       py -I tools/savestate_mem.py <slotNN.ff7s> find16 <value> [lo hi]   (u16 little-endian)
       py -I tools/savestate_mem.py <slotNN.ff7s> find32 <value> [lo hi]
"""
import struct
import sys
import zlib

R_MEM = None   # learned from the stream: the record id whose payload starts with a guest address


def load(path):
    d = open(path, 'rb').read()
    hsize = struct.unpack_from('<I', d, 12)[0]
    raw = zlib.decompress(d[hsize:])
    regions, o = [], 0
    while o + 8 <= len(raw):
        rid, n = struct.unpack_from('<II', raw, o)
        body = raw[o + 8:o + 8 + n]
        o += 8 + n
        if n > 4096 + 4:
            lo = struct.unpack_from('<I', body, 0)[0]
            if 0x400000 <= lo < 0x80000000:
                regions.append((lo, body[4:], rid))
    return regions


def read(regions, addr, n):
    for lo, b, _ in regions:
        if lo <= addr and addr + n <= lo + len(b):
            return b[addr - lo:addr - lo + n]
    return None


def main():
    if len(sys.argv) < 3:
        sys.exit(__doc__)
    regions = load(sys.argv[1])
    cmd = sys.argv[2]
    if cmd == 'read':
        a, n = int(sys.argv[3], 16), int(sys.argv[4], 0)
        b = read(regions, a, n)
        for i in range(0, len(b or b''), 16):
            print(f'{a + i:08X} ' + b[i:i + 16].hex(' '))
    elif cmd in ('find16', 'find32'):
        v = int(sys.argv[3], 0)
        lo = int(sys.argv[4], 16) if len(sys.argv) > 4 else 0
        hi = int(sys.argv[5], 16) if len(sys.argv) > 5 else 0xFFFFFFFF
        pat = struct.pack('<H' if cmd == 'find16' else '<I', v)
        for base, b, _ in regions:
            i = b.find(pat)
            while i >= 0:
                if lo <= base + i < hi:
                    print(f'{base + i:08X}')
                i = b.find(pat, i + 1)
    else:
        sys.exit(__doc__)


if __name__ == '__main__':
    main()
