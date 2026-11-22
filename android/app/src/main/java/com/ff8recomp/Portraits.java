package com.ff8recomp;

import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.util.Log;

import java.io.BufferedInputStream;
import java.io.DataInputStream;
import java.io.EOFException;
import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.RandomAccessFile;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.charset.StandardCharsets;
import java.util.List;

/**
 * The party's menu portraits for the bottom screen (CompanionView), cut from the player's own
 * game files when the panels open: nothing from the game ships with the app.
 *
 * FF8 keeps them on two face sheets, 4 x 2 faces of 64 x 96 each (face.sp2 has the low-res
 * layout, half that):
 *   face_b00: Squall, Zell, Irvine, Quistis / Laguna, Kiros, Ward
 *   face_b01: Rinoa, Selphie, Seifer, Edea / Griever, Mog, Boko, Angelo
 * so savemap character ids 0-3 and 8-10 are on b00, 4-7 on b01.
 *   2013 release   Data/lang-en/menu.fs (.fl names, .fi {size, offset, compression}):
 *                  menu/hires/Face_b0N.TEX, LZSS; a 0xF0-byte TEX v2 header, then 256 x 256
 *                  16-bit pixels (5-5-5, red in the low bits).
 *   Remastered     main.zzz: textures/hires/face_b0N.tex.png, its HD sheet of the same layout.
 *                  main.zzz: u32 count, then {u32 name length, name, u64 offset, u32 size}.
 */
final class Portraits {
    private static final String TAG = "FF8Portraits";
    static final int FACE_W = 64, FACE_H = 96;   // in the 256 x 256 sheet
    private static final int CHARS = 11;         // Squall .. Ward
    private static final int OUT_H = 192;        // kept at 2x: the bottom screen draws them ~170 px tall

    private Portraits() {}

    /** The faces by character id (null where one could not be read). Call off the UI thread. */
    static Bitmap[] load(File gameDir) {
        Bitmap[] out = new Bitmap[CHARS];
        try {
            Bitmap[] sheets = new File(gameDir, GameInstall.REMASTER_MARKER).isFile() ? remastered(gameDir) : classic(gameDir);
            if (sheets == null) return out;
            for (int id = 0; id < CHARS; id++) {
                Bitmap s = sheets[id >= 4 && id < 8 ? 1 : 0];
                if (s == null) continue;
                int col = id < 8 ? id % 4 : id - 8, row = id < 8 ? 0 : 1;
                float k = s.getWidth() / 256f;
                int x = Math.round(col * FACE_W * k), y = Math.round(row * FACE_H * k);
                int w = Math.round(FACE_W * k), h = Math.round(FACE_H * k);
                if (x + w > s.getWidth() || y + h > s.getHeight()) continue;
                Bitmap face = Bitmap.createBitmap(s, x, y, w, h);
                out[id] = h == OUT_H ? face : Bitmap.createScaledBitmap(face, OUT_H * FACE_W / FACE_H, OUT_H, true);
            }
        } catch (IOException | RuntimeException | OutOfMemoryError e) {
            Log.w(TAG, "portraits: " + e);
        }
        return out;
    }

    // ---- the 2013 release: menu.fs ---------------------------------------------------------

    private static Bitmap[] classic(File gameDir) throws IOException {
        File base = new File(gameDir, "Data/lang-en");
        File fl = new File(base, "menu.fl"), fi = new File(base, "menu.fi"), fs = new File(base, "menu.fs");
        if (!fl.isFile() || !fi.isFile() || !fs.isFile()) return null;
        List<String> names = java.nio.file.Files.readAllLines(fl.toPath(), StandardCharsets.ISO_8859_1);
        ByteBuffer idx = ByteBuffer.wrap(java.nio.file.Files.readAllBytes(fi.toPath())).order(ByteOrder.LITTLE_ENDIAN);
        Bitmap[] sheets = new Bitmap[2];
        try (RandomAccessFile f = new RandomAccessFile(fs, "r")) {
            for (int i = 0; i < names.size() && 12 * i + 12 <= idx.capacity(); i++) {
                String n = names.get(i).trim().toLowerCase(java.util.Locale.ROOT);
                int sheet = n.endsWith("\\hires\\face_b00.tex") ? 0 : n.endsWith("\\hires\\face_b01.tex") ? 1 : -1;
                if (sheet < 0) continue;
                int size = idx.getInt(12 * i), off = idx.getInt(12 * i + 4), comp = idx.getInt(12 * i + 8);
                if (size <= 0 || size > 1 << 22) continue;
                byte[] data;
                if (comp == 0) data = read(f, off, size);
                else if (comp == 1) {
                    int clen = ByteBuffer.wrap(read(f, off, 4)).order(ByteOrder.LITTLE_ENDIAN).getInt();
                    if (clen <= 0 || clen > 1 << 22) continue;
                    data = lzss(read(f, off + 4L, clen), size);
                } else continue;
                sheets[sheet] = tex16(ByteBuffer.wrap(data).order(ByteOrder.LITTLE_ENDIAN));
            }
        }
        return sheets;
    }

    /** A TEX v2 sheet of 16-bit pixels (5-5-5, red low), as FF8's hires faces are. */
    private static Bitmap tex16(ByteBuffer t) {
        if (t.capacity() < 0xF0) return null;
        int w = t.getInt(0x3C), h = t.getInt(0x40), bpp = t.getInt(0x38);
        int px = t.getInt(0) >= 2 ? 0xF0 : 0xEC;
        if (bpp != 16 || w <= 0 || h <= 0 || w > 2048 || h > 2048 || t.capacity() < px + w * h * 2) return null;
        int[] argb = new int[w * h];
        for (int i = 0; i < w * h; i++) {
            int v = t.getShort(px + 2 * i) & 0xFFFF;
            int r = (v & 31) * 255 / 31, g = ((v >> 5) & 31) * 255 / 31, b = ((v >> 10) & 31) * 255 / 31;
            argb[i] = 0xFF000000 | (r << 16) | (g << 8) | b;
        }
        return Bitmap.createBitmap(argb, w, h, Bitmap.Config.ARGB_8888);
    }

    /** FF8's LZSS (the PS1 one: a 4 KB window starting at 0xFEE, flag bit 1 = a literal). */
    private static byte[] lzss(byte[] src, int outLen) {
        byte[] out = new byte[outLen];
        byte[] win = new byte[4096];
        int wp = 0xFEE, i = 0, o = 0;
        while (i < src.length && o < outLen) {
            int flags = src[i++] & 0xFF;
            for (int b = 0; b < 8 && i < src.length && o < outLen; b++) {
                if ((flags & (1 << b)) != 0) {
                    byte c = src[i++];
                    out[o++] = c; win[wp] = c; wp = (wp + 1) & 0xFFF;
                } else {
                    if (i + 1 >= src.length) return out;
                    int lo = src[i] & 0xFF, hi = src[i + 1] & 0xFF;
                    i += 2;
                    int off = lo | ((hi & 0xF0) << 4), len = (hi & 0x0F) + 3;
                    for (int k = 0; k < len && o < outLen; k++) {
                        byte c = win[(off + k) & 0xFFF];
                        out[o++] = c; win[wp] = c; wp = (wp + 1) & 0xFFF;
                    }
                }
            }
        }
        return out;
    }

    // ---- the Remastered: main.zzz -----------------------------------------------------------

    private static Bitmap[] remastered(File gameDir) throws IOException {
        File zzz = new File(gameDir, "main.zzz");
        if (!zzz.isFile()) return null;
        long[][] at = new long[2][];
        try (DataInputStream in = new DataInputStream(new BufferedInputStream(new FileInputStream(zzz), 1 << 16))) {
            long count = u32(in);
            byte[] name = new byte[512];
            for (long i = 0; i < count && (at[0] == null || at[1] == null); i++) {
                int len = (int) u32(in);
                if (len <= 0 || len > name.length) return null;
                in.readFully(name, 0, len);
                long pos = u32(in) | (u32(in) << 32), size = u32(in);
                String n = new String(name, 0, len, StandardCharsets.ISO_8859_1).toLowerCase(java.util.Locale.ROOT);
                if (n.equals("textures\\hires\\face_b00.tex.png")) at[0] = new long[] { pos, size };
                else if (n.equals("textures\\hires\\face_b01.tex.png")) at[1] = new long[] { pos, size };
            }
        }
        Bitmap[] sheets = new Bitmap[2];
        try (RandomAccessFile f = new RandomAccessFile(zzz, "r")) {
            for (int s = 0; s < 2; s++) {
                if (at[s] == null || at[s][1] <= 0 || at[s][1] > 32 << 20) continue;
                byte[] png = read(f, at[s][0], (int) at[s][1]);
                sheets[s] = BitmapFactory.decodeByteArray(png, 0, png.length);
            }
        }
        return sheets;
    }

    private static long u32(InputStream in) throws IOException {
        int a = in.read(), b = in.read(), c = in.read(), d = in.read();
        if ((a | b | c | d) < 0) throw new EOFException();
        return (a | (b << 8) | (c << 16) | ((long) d << 24)) & 0xFFFFFFFFL;
    }

    private static byte[] read(RandomAccessFile f, long off, int n) throws IOException {
        byte[] b = new byte[n];
        f.seek(off);
        f.readFully(b);
        return b;
    }
}
