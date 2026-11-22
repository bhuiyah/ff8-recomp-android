#!/usr/bin/env python3
"""Decode the FF8 Remastered FFVIII_<LANG>.exe.dat into the image FFVIII_<LANG>.dll builds.

The .dat is not encrypted. It is Dotemu's serialized "ImageFile" (sources\\emulation\\ImageFile.cpp
in FFVIII_EFIGS.dll; loader at 0x10051B50, called from vprocess::loadPEImage at 0x1004AC30).
All integers little-endian u32. Layout, read strictly in order to EOF:

    IMAGE_FILE_HEADER        0x14 bytes  (no MZ / PE signature)
    IMAGE_OPTIONAL_HEADER32  0xE0 bytes
    u32 nSections
    nSections x {
        IMAGE_SECTION_HEADER 0x28 bytes
        u32 uncompSize                  (= VirtualSize rounded up to 0x1000; BSS included)
        u32 compSize
        compSize bytes zlib stream      (zlib 1.2.8 inflate; must yield exactly uncompSize)
        u32 nameLen, nameLen bytes      (section name string, no NUL)
    }
    u32 nImportDlls
    nImportDlls x {
        IMAGE_IMPORT_DESCRIPTOR 0x14 bytes
        u32 len, bytes dllName
        u32 n,  n x { u32 iltValue, u32 len, bytes name }   (ILT entry -> name)
        u32 n2, n2 x { u32 iatVA,   u32 len, bytes name }   (IAT slot VA -> name)
    }
    u32 nSlots, nSlots x u32            (every IAT slot VA)

The DLL maps each section into its emulated address space ("vmem") at ImageBase + VirtualAddress
with the inflated buffer, plus a zeroed 0x400000-byte region "exe.dat.GF" at 0x0E100000.

Usage:  py -I tools/remaster_extract.py [FFVIII_EN.exe.dat] [-o build/remaster]
Writes  <out>/FFVIII_EN.exe     a PE32 (MZ stub + headers; each section's raw data = the full
                                inflated buffer, so BSS bytes are kept)
        <out>/FFVIII_EN.image   flat image, ImageBase-relative, SizeOfImage bytes
        <out>/FFVIII_EN.json    header fields, section map, imports, IAT slot list
Outputs are derived from game files: keep them under build/ (git-ignored).
"""
import argparse, hashlib, json, os, struct, sys, zlib

DEFAULT_DAT = r"C:\Program Files (x86)\Steam\steamapps\common\FINAL FANTASY VIII Remastered\FFVIII_EN.exe.dat"


class Reader:
    def __init__(self, data):
        self.d, self.o = data, 0

    def u32(self):
        v = struct.unpack_from("<I", self.d, self.o)[0]
        self.o += 4
        return v

    def take(self, n):
        if self.o + n > len(self.d):
            raise ValueError("truncated at 0x%x (+0x%x)" % (self.o, n))
        b = self.d[self.o:self.o + n]
        self.o += n
        return b

    def lstr(self):
        return self.take(self.u32()).decode("latin-1")


def parse(data):
    r = Reader(data)
    fh, oh = r.take(0x14), r.take(0xE0)
    if struct.unpack_from("<H", fh)[0] != 0x14C or struct.unpack_from("<H", oh)[0] != 0x10B:
        raise ValueError("not an i386 PE32 ImageFile")
    sections = []
    for _ in range(r.u32()):
        hdr = r.take(0x28)
        uncomp, comp = r.u32(), r.u32()
        raw = zlib.decompress(r.take(comp))
        if len(raw) != uncomp:
            raise ValueError("inflate size mismatch")
        sections.append({"hdr": hdr, "data": raw, "name": r.lstr(), "comp": comp})
    imports = []
    for _ in range(r.u32()):
        desc = r.take(0x14)
        dll = r.lstr()
        ilt = [(r.u32(), r.lstr()) for _ in range(r.u32())]
        iat = [(r.u32(), r.lstr()) for _ in range(r.u32())]
        imports.append({"desc": struct.unpack("<5I", desc), "dll": dll, "ilt": ilt, "iat": iat})
    slots = [r.u32() for _ in range(r.u32())]
    if r.o != len(data):
        raise ValueError("trailing data at 0x%x" % r.o)
    return fh, oh, sections, imports, slots


def align(v, a):
    return (v + a - 1) // a * a


def build_pe(fh, oh, sections):
    nsec = len(sections)
    fh = bytearray(fh)
    struct.pack_into("<H", fh, 2, nsec)
    file_align = struct.unpack_from("<I", oh, 36)[0] or 0x200
    dos = bytearray(0x40)
    dos[0:2] = b"MZ"
    struct.pack_into("<I", dos, 0x3C, 0x40)
    hdr_end = 0x40 + 4 + 0x14 + 0xE0 + 0x28 * nsec
    pos = align(hdr_end, file_align)
    oh = bytearray(oh)
    struct.pack_into("<I", oh, 60, pos)  # SizeOfHeaders
    shdrs, body = bytearray(), bytearray()
    for s in sections:
        h = bytearray(s["hdr"])
        size = align(len(s["data"]), file_align)
        struct.pack_into("<II", h, 16, size, pos)
        shdrs += h
        body += s["data"] + b"\0" * (size - len(s["data"]))
        pos += size
    head = bytes(dos) + b"PE\0\0" + bytes(fh) + bytes(oh) + bytes(shdrs)
    return head + b"\0" * (align(hdr_end, file_align) - len(head)) + bytes(body)


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("dat", nargs="?", default=DEFAULT_DAT)
    ap.add_argument("-o", "--out", default=os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                                        "..", "build", "remaster"))
    a = ap.parse_args()
    data = open(a.dat, "rb").read()
    fh, oh, sections, imports, slots = parse(data)
    base, entry, size_img = (struct.unpack_from("<I", oh, o)[0] for o in (28, 16, 56))
    stem = os.path.basename(a.dat)
    stem = stem[:-len(".exe.dat")] if stem.lower().endswith(".exe.dat") else stem
    os.makedirs(a.out, exist_ok=True)

    image = bytearray(size_img)
    hdr_len = 0x14 + 0xE0
    image[0:hdr_len] = fh + oh  # header page holds the raw COFF+optional header (no MZ)
    secmap = []
    for s in sections:
        name, vs, va, rs, ro = struct.unpack_from("<8sIIII", s["hdr"])
        chars = struct.unpack_from("<I", s["hdr"], 36)[0]
        image[va:va + len(s["data"])] = s["data"]
        secmap.append({"name": s["name"], "va": "0x%08X" % (base + va), "rva": "0x%X" % va,
                       "virtual_size": "0x%X" % vs, "raw_size_in_header": "0x%X" % rs,
                       "inflated_size": "0x%X" % len(s["data"]), "zlib_size": "0x%X" % s["comp"],
                       "characteristics": "0x%08X" % chars,
                       "sha1": hashlib.sha1(s["data"]).hexdigest()})
    meta = {
        "source": os.path.basename(a.dat), "source_sha1": hashlib.sha1(data).hexdigest(),
        "image_base": "0x%08X" % base, "entry": "0x%08X" % (base + entry),
        "size_of_image": "0x%X" % size_img,
        "timestamp": struct.unpack_from("<I", fh, 4)[0],
        "data_dirs": {i: ["0x%X" % v for v in struct.unpack_from("<II", oh, 96 + 8 * i)]
                      for i in range(16) if struct.unpack_from("<I", oh, 96 + 8 * i)[0]},
        "sections": secmap,
        "extra_regions": [{"name": "exe.dat.GF", "va": "0x0E100000", "size": "0x400000",
                           "note": "zero-filled; mapped by FFVIII_EFIGS.dll 0x1004A18C"}],
        "imports": [{"dll": im["dll"], "descriptor": ["0x%X" % v for v in im["desc"]],
                     "ilt": [["0x%X" % v, n] for v, n in im["ilt"]],
                     "iat": [["0x%08X" % v, n] for v, n in im["iat"]]} for im in imports],
        "iat_slots": ["0x%08X" % v for v in slots],
    }
    out = os.path.join(a.out, stem)
    open(out + ".exe", "wb").write(build_pe(fh, oh, sections))
    open(out + ".image", "wb").write(image)
    with open(out + ".json", "w") as f:
        json.dump(meta, f, indent=1)
    for s in secmap:
        print("%-7s %s vsize %-9s inflated %-9s sha1 %s" % (s["name"], s["va"], s["virtual_size"],
                                                           s["inflated_size"], s["sha1"]))
    print("%d import DLLs, %d IAT slots; wrote %s.{exe,image,json}" %
          (len(imports), len(slots), os.path.normpath(out)))


if __name__ == "__main__":
    sys.exit(main())
