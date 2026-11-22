package com.ff8recomp;

import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.util.ArrayList;
import java.util.List;
import java.util.zip.DataFormatException;
import java.util.zip.Inflater;

/**
 * FINAL FANTASY VIII - REMASTERED keeps the game's original program (the same 1.2 US code as the
 * 2013 release's FF8_EN.exe, without its Steam wrapper) in FFVIII_EN.exe.dat: Dotemu's serialized
 * image, not encrypted, each section zlib-compressed. This rebuilds it as a PE (as
 * tools/remaster_extract.py does, byte for byte), which the build lifts like FF8_EN.exe.
 *
 * Layout (all u32 little-endian): IMAGE_FILE_HEADER (0x14), IMAGE_OPTIONAL_HEADER32 (0xE0),
 * nSections x { section header (0x28), uncompSize, compSize, zlib stream, name (u32 len + bytes) },
 * then the import tables and IAT slot list (not needed: the sections hold the real ones).
 */
final class RemasterExe {
    private RemasterExe() {}

    private static final class Section { byte[] hdr; byte[] data; }

    static byte[] decode(byte[] dat) throws IOException {
        ByteBuffer b = ByteBuffer.wrap(dat).order(ByteOrder.LITTLE_ENDIAN);
        try {
            byte[] fh = take(b, 0x14), oh = take(b, 0xE0);
            ByteBuffer fhb = ByteBuffer.wrap(fh).order(ByteOrder.LITTLE_ENDIAN), ohb = ByteBuffer.wrap(oh).order(ByteOrder.LITTLE_ENDIAN);
            if ((fhb.getShort(0) & 0xFFFF) != 0x14C || (ohb.getShort(0) & 0xFFFF) != 0x10B) throw new IOException("not the Remastered's program file");
            int n = b.getInt();
            if (n <= 0 || n > 32) throw new IOException("unexpected section count " + n);
            List<Section> secs = new ArrayList<>();
            for (int i = 0; i < n; i++) {
                Section s = new Section();
                s.hdr = take(b, 0x28);
                int uncomp = b.getInt(), comp = b.getInt();
                s.data = inflate(take(b, comp), uncomp);
                take(b, b.getInt());                                  // the section's name
                secs.add(s);
            }
            return buildPe(fh, oh, secs);
        } catch (java.nio.BufferUnderflowException | IllegalArgumentException e) {
            throw new IOException("FFVIII_EN.exe.dat is damaged (it ends early)");
        }
    }

    private static byte[] take(ByteBuffer b, int n) {
        if (n < 0 || n > b.remaining()) throw new IllegalArgumentException();
        byte[] out = new byte[n];
        b.get(out);
        return out;
    }

    private static byte[] inflate(byte[] z, int size) throws IOException {
        Inflater inf = new Inflater();
        inf.setInput(z);
        byte[] out = new byte[size];
        try {
            int got = 0;
            while (got < size && !inf.finished()) {
                int k = inf.inflate(out, got, size - got);
                if (k == 0 && (inf.needsInput() || inf.needsDictionary())) break;
                got += k;
            }
            if (got != size) throw new IOException("FFVIII_EN.exe.dat: a section did not unpack to its size");
        } catch (DataFormatException e) {
            throw new IOException("FFVIII_EN.exe.dat is damaged: " + e.getMessage());
        } finally {
            inf.end();
        }
        return out;
    }

    private static int align(int v, int a) { return (v + a - 1) / a * a; }

    private static byte[] buildPe(byte[] fhIn, byte[] ohIn, List<Section> secs) throws IOException {
        int nsec = secs.size();
        byte[] fh = fhIn.clone(), oh = ohIn.clone();
        ByteBuffer fhb = ByteBuffer.wrap(fh).order(ByteOrder.LITTLE_ENDIAN), ohb = ByteBuffer.wrap(oh).order(ByteOrder.LITTLE_ENDIAN);
        fhb.putShort(2, (short) nsec);
        int fileAlign = ohb.getInt(36);
        if (fileAlign == 0) fileAlign = 0x200;
        byte[] dos = new byte[0x40];
        dos[0] = 'M'; dos[1] = 'Z';
        ByteBuffer.wrap(dos).order(ByteOrder.LITTLE_ENDIAN).putInt(0x3C, 0x40);
        int hdrEnd = 0x40 + 4 + 0x14 + 0xE0 + 0x28 * nsec;
        int pos = align(hdrEnd, fileAlign);
        ohb.putInt(60, pos);                                           // SizeOfHeaders
        ByteArrayOutputStream shdrs = new ByteArrayOutputStream(), body = new ByteArrayOutputStream();
        for (Section s : secs) {
            byte[] h = s.hdr.clone();
            int size = align(s.data.length, fileAlign);
            ByteBuffer hb = ByteBuffer.wrap(h).order(ByteOrder.LITTLE_ENDIAN);
            hb.putInt(16, size);                                       // SizeOfRawData
            hb.putInt(20, pos);                                        // PointerToRawData
            shdrs.write(h);
            body.write(s.data);
            body.write(new byte[size - s.data.length]);
            pos += size;
        }
        ByteArrayOutputStream out = new ByteArrayOutputStream();
        out.write(dos);
        out.write(new byte[] { 'P', 'E', 0, 0 });
        out.write(fh);
        out.write(oh);
        out.write(shdrs.toByteArray());
        out.write(new byte[align(hdrEnd, fileAlign) - out.size()]);
        out.write(body.toByteArray());
        return out.toByteArray();
    }
}
