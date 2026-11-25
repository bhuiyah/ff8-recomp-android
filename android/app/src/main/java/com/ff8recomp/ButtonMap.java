package com.ff8recomp;

import android.content.SharedPreferences;

/** Settings > Controller: which button of the pad works as which (issue #7). Kept as a permutation,
 *  so no button of the game's can be lost. */
final class ButtonMap {
    static final String[] NAMES = { "A", "B", "X", "Y", "L1", "R1", "L2", "R2", "Select", "Start" };
    private static final int L2 = 6, R2 = 7;
    /** GameActivity.xinputBit's bits; 0: a trigger. */
    private static final int[] BITS = { 0x1000, 0x8000, 0x4000, 0x2000, 0x0100, 0x0200, 0, 0, 0x0020, 0x0010 };
    private static final int MAPPED = 0x2000 | 0x1000 | 0x4000 | 0x8000 | 0x0100 | 0x0200 | 0x0020 | 0x0010;

    static int[] read(SharedPreferences prefs) {
        int[] map = new int[NAMES.length];
        for (int i = 0; i < map.length; i++) map[i] = i;
        String[] s = prefs.getString("button_map", "").split(",");
        if (s.length != map.length) return map;
        int[] m = new int[map.length];
        int seen = 0;
        try {
            for (int i = 0; i < m.length; i++) {
                m[i] = Integer.parseInt(s[i]);
                if (m[i] < 0 || m[i] >= m.length) return map;
                seen |= 1 << m[i];
            }
        } catch (NumberFormatException e) { return map; }
        return seen == (1 << map.length) - 1 ? m : map;
    }

    static void choose(SharedPreferences prefs, int button, int as) {
        int[] m = read(prefs);
        for (int i = 0; i < m.length; i++) if (m[i] == as) m[i] = m[button];
        m[button] = as;
        StringBuilder s = new StringBuilder();
        for (int v : m) s.append(s.length() > 0 ? "," : "").append(v);
        prefs.edit().putString("button_map", s.toString()).apply();
    }

    static void reset(SharedPreferences prefs) { prefs.edit().remove("button_map").apply(); }

    // ---- the save-state buttons, pressed with Select (issue #14): an index into STATE_NAMES each ----
    static final String[] STATE_NAMES = { "A", "B", "X", "Y", "L1", "R1", "L2", "R2", "None" };
    private static final int STATE_NONE = 8;
    /** The bits nativeStateButtons takes. */
    private static final int[] STATE_BITS = { 0x1000, 0x8000, 0x4000, 0x2000, 0x0100, 0x0200, 0x10000, 0x20000, 0 };

    static int stateButton(SharedPreferences prefs, boolean load) {
        int v = prefs.getInt(load ? "state_load_button" : "state_save_button", load ? 5 : 4);
        return v >= 0 && v < STATE_NAMES.length ? v : load ? 5 : 4;
    }

    static int stateBits(SharedPreferences prefs, boolean load) { return STATE_BITS[stateButton(prefs, load)]; }

    /** Choosing the other one's button swaps the two. */
    static void chooseState(SharedPreferences prefs, boolean load, int button) {
        int was = stateButton(prefs, load), other = stateButton(prefs, !load);
        SharedPreferences.Editor e = prefs.edit().putInt(load ? "state_load_button" : "state_save_button", button);
        if (button == other && button != STATE_NONE) e.putInt(load ? "state_save_button" : "state_load_button", was);
        e.apply();
    }

    static void swapState(SharedPreferences prefs) {
        int s = stateButton(prefs, false), l = stateButton(prefs, true);
        prefs.edit().putInt("state_save_button", l).putInt("state_load_button", s).apply();
    }

    /** "Select+L1", or null when there is none. */
    static String stateCombo(SharedPreferences prefs, boolean load) {
        int b = stateButton(prefs, load);
        return b == STATE_NONE ? null : "Select+" + STATE_NAMES[b];
    }

    /** "Select+L1 saves a state, Select+R1 loads it" */
    static String stateHelp(SharedPreferences prefs) {
        String s = stateCombo(prefs, false), l = stateCombo(prefs, true);
        return (s == null ? "no button saves a state" : s + " saves a state") + ", "
                + (l == null ? "no button loads it" : l + " loads it");
    }

    /** The pad's buttons and triggers as the game gets them: { buttons, lt, rt }. */
    static int[] apply(int[] map, int buttons, int lt, int rt) {
        int[] out = { buttons & ~MAPPED, 0, 0 };
        for (int i = 0; i < map.length; i++) {
            int v = i == L2 ? lt : i == R2 ? rt : (buttons & BITS[i]) != 0 ? 255 : 0;
            if (v == 0) continue;
            if (map[i] == L2) out[1] = Math.max(out[1], v);
            else if (map[i] == R2) out[2] = Math.max(out[2], v);
            else if (v > 60) out[0] |= BITS[map[i]];
        }
        return out;
    }
}
