package com.ff8recomp;

import java.io.IOException;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;

/**
 * Removes the SteamStub 3.x wrapper from FF8_EN.exe (the 2013 Steam release, app 39150), the way
 * tools/unwrap_steamstub.py does: the PE entry point goes back to the game's own, read from the
 * stub's XOR-chained header just before the stub's entry. FF8's build has flag 0x04 (its code is
 * not encrypted), so nothing else changes; an encrypted build is refused.
 */
final class SteamStub {
    private SteamStub() {}

    private static final int SIG_V30 = 0xC0DEC0DE, SIG_V31 = 0xC0DEC0DF, FLAG_NO_ENCRYPTION = 0x04;

    /** True when d is a 32-bit PE whose entry point is in a ".bind" section. */
    static boolean wrapped(byte[] d) {
        try { return bindEntry(le(d)) >= 0; } catch (RuntimeException e) { return false; }
    }

    /** d unwrapped (a new array). */
    static byte[] unwrap(byte[] d) throws IOException {
        ByteBuffer b = le(d);
        int epOff;
        try { epOff = bindEntry(b); } catch (RuntimeException e) { throw new IOException("not a Windows program"); }
        if (epOff < 0) throw new IOException("not a SteamStub-wrapped program");
        int pe = b.getInt(0x3C);
        int ep = b.getInt(pe + 40);
        for (int hsize : new int[] { 0xD0, 0xF0 }) {        // x86 3.0 header, x86 3.1 header
            if (epOff - hsize < 0) continue;
            int[] h = xorChain(b, epOff - hsize, hsize);
            if (h[1] != SIG_V30 && h[1] != SIG_V31) continue;
            // dwords: 0 key, 1 signature, 2-3 image base, 4 stub entry, 5 bind offset, 6 ?,
            // 7 original entry, 8 ?, 9 payload, 10-11 DRMP offset and size, 12 app id, 13 flags
            int stubEp = h[4], oep = h[7], flags = h[13];
            if (stubEp != ep) throw new IOException("SteamStub header does not match the program");
            if ((flags & FLAG_NO_ENCRYPTION) == 0) throw new IOException("this program's code is encrypted (not the FF8 build)");
            byte[] out = d.clone();
            le(out).putInt(pe + 40, oep);
            return out;
        }
        throw new IOException("no SteamStub header found");
    }

    private static ByteBuffer le(byte[] d) { return ByteBuffer.wrap(d).order(ByteOrder.LITTLE_ENDIAN); }

    /** File offset of the entry point when it is in ".bind", -1 when elsewhere. */
    private static int bindEntry(ByteBuffer b) {
        if (b.getShort(0) != 0x5A4D) throw new IllegalArgumentException();
        int pe = b.getInt(0x3C);
        if (b.getInt(pe) != 0x4550 || (b.getShort(pe + 24) & 0xFFFF) != 0x10B) throw new IllegalArgumentException();
        int n = b.getShort(pe + 6) & 0xFFFF, optSize = b.getShort(pe + 20) & 0xFFFF;
        int ep = b.getInt(pe + 40);
        for (int i = 0; i < n; i++) {
            int o = pe + 24 + optSize + 40 * i;
            StringBuilder name = new StringBuilder();
            for (int k = 0; k < 8 && b.get(o + k) != 0; k++) name.append((char) b.get(o + k));
            int vs = b.getInt(o + 8), va = b.getInt(o + 12), rs = b.getInt(o + 16), ro = b.getInt(o + 20);
            if (ep >= va && ep < va + Math.max(vs, rs)) return name.toString().equals(".bind") ? ep - va + ro : -1;
        }
        return -1;
    }

    /** The header's dwords: dword 0 is the key; each later dword is XORed with the previous raw one. */
    private static int[] xorChain(ByteBuffer b, int off, int size) {
        int[] out = new int[size / 4];
        int key = b.getInt(off);
        out[0] = key;
        for (int i = 1; i < out.length; i++) {
            int v = b.getInt(off + 4 * i);
            out[i] = v ^ key;
            key = v;
        }
        return out;
    }
}
