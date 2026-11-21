#!/usr/bin/env python3
"""Remove the SteamStub 3.x wrapper from a 32-bit exe, so it can be lifted.

FF8's 2013 Steam release (FF8_EN.exe, app 39150) is wrapped by SteamStub 3.0: the entry point
sits in a '.bind' section whose code checks Steam, then jumps to the game's real entry point. Its
header (just before the stub's entry, XOR-chained) holds that original entry point and flags. The
FF8 build has flag 0x04 (no code encryption), so the game's code is already in the clear and
unwrapping only means pointing the PE entry back at the original one. Encrypted builds (no flag
0x04) are decrypted too: AES-256-CBC over the stolen 16 bytes + the code section, the IV itself
ECB-encrypted with the same key (as Steamless does). That path needs pycryptodome.

The .bind section is left in place (it is never run); nothing else in the file changes.

usage: py -I tools/unwrap_steamstub.py <in.exe> <out.exe>
"""
import struct
import sys

SIG_V30 = 0xC0DEC0DE
SIG_V31 = 0xC0DEC0DF
FLAG_NO_ENCRYPTION = 0x04


def sections(d):
    pe = struct.unpack_from('<I', d, 0x3C)[0]
    n = struct.unpack_from('<H', d, pe + 6)[0]
    osz = struct.unpack_from('<H', d, pe + 20)[0]
    out = []
    for i in range(n):
        o = pe + 24 + osz + 40 * i
        name = d[o:o + 8].rstrip(b'\0').decode('latin1')
        vs, va, rs, ro = struct.unpack_from('<IIII', d, o + 8)
        out.append((name, va, vs, ro, rs))
    return pe, out


def rva_to_off(secs, rva):
    for _, va, vs, ro, rs in secs:
        if va <= rva < va + max(vs, rs):
            return rva - va + ro
    raise ValueError(f'rva 0x{rva:x} is in no section')


def xor_chain(buf):
    """SteamStub 3.x header: dword[0] is the key; each later dword ^= the previous raw dword."""
    key = struct.unpack_from('<I', buf, 0)[0]
    out = bytearray(buf)
    for i in range(4, len(buf), 4):
        v = struct.unpack_from('<I', buf, i)[0]
        struct.pack_into('<I', out, i, v ^ key)
        key = v
    return bytes(out)


def unwrap(d):
    pe, secs = sections(d)
    if struct.unpack_from('<H', d, pe + 24)[0] != 0x10B:
        raise SystemExit('not a 32-bit PE')
    ep = struct.unpack_from('<I', d, pe + 40)[0]
    name = next((s[0] for s in secs if s[1] <= ep < s[1] + max(s[2], s[4])), None)
    if name != '.bind':
        raise SystemExit(f'entry point is in {name!r}, not .bind: not SteamStub-wrapped')
    ep_off = rva_to_off(secs, ep)
    for hsize in (0xD0, 0xF0):          # x86 3.0 header, x86 3.1 header
        h = xor_chain(d[ep_off - hsize:ep_off])
        sig = struct.unpack_from('<I', h, 4)[0]
        if sig in (SIG_V30, SIG_V31):
            break
    else:
        raise SystemExit('no SteamStub 3.x header found before the entry point')
    # 3.0 x86 layout: key, sig, u64 image base, entry, bind offset, ?, original entry, ?,
    # payload size, drmp offset, drmp size, app id, flags, bind vsize, ?, code va, code size,
    # aes key[32], aes iv[16], stolen[16]
    (image_base, stub_ep, _bind_off, _u0, oep, _u1, _payload, _drmp_off, _drmp_size, app_id,
     flags, _bind_vsize, _u2, code_va, code_size) = struct.unpack_from('<QIIIIIIIIIIIIII', h, 8)
    aes_key = h[0x44:0x64]
    aes_iv = h[0x64:0x74]
    stolen = h[0x74:0x84]
    if stub_ep != ep:
        raise SystemExit(f'header entry 0x{stub_ep:x} does not match the PE entry 0x{ep:x}')
    print(f'SteamStub {"3.0" if sig == SIG_V30 else "3.1"}: app {app_id}, flags 0x{flags:x}, '
          f'original entry 0x{oep:x}, image base 0x{image_base:x}')

    out = bytearray(d)
    struct.pack_into('<I', out, pe + 40, oep)
    if not flags & FLAG_NO_ENCRYPTION:
        from Crypto.Cipher import AES   # pycryptodome
        iv = AES.new(aes_key, AES.MODE_ECB).decrypt(aes_iv)
        code_off = rva_to_off(secs, code_va)
        enc = stolen + d[code_off:code_off + code_size]
        enc = enc[:len(enc) - len(enc) % 16]
        plain = AES.new(aes_key, AES.MODE_CBC, iv).decrypt(enc)
        out[code_off:code_off + len(plain)] = plain
        print(f'decrypted {len(plain)} bytes of code at rva 0x{code_va:x}')
    else:
        print('code is not encrypted: only the entry point changes')
    return bytes(out)


def main():
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    d = open(sys.argv[1], 'rb').read()
    open(sys.argv[2], 'wb').write(unwrap(d))
    print(f'wrote {sys.argv[2]}')


if __name__ == '__main__':
    main()
