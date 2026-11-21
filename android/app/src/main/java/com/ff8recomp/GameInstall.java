package com.ff8recomp;

import android.content.ContentResolver;
import android.content.Context;
import android.content.res.AssetManager;
import android.database.Cursor;
import android.net.Uri;
import android.provider.DocumentsContract;
import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.List;
import java.util.Locale;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

/**
 * The game's files in app storage (files/game): the 2013 Steam release's FF8_EN.exe, without its
 * SteamStub wrapper, as ff8_en.exe (SteamStub.java), and its Data folder, imported once from a
 * folder the player picks or downloaded from Steam; FFNx's run-time files from the APK;
 * FFNx.toml written from the launcher's settings.
 */
final class GameInstall {
    interface Progress { void update(String what); }

    final Context ctx;
    final File files, game;

    GameInstall(Context ctx) {
        this.ctx = ctx;
        files = Storage.root(ctx);   // internal storage or the SD card (Settings > Storage)
        game = new File(files, "game");
    }

    boolean hasGame() {
        return new File(game, "ff8_en.exe").isFile() && new File(game, "Data/lang-en").isDirectory() && !steamDownloading().exists();
    }

    /** Present while a download from Steam is unfinished (SteamFiles): the game is not complete. */
    File steamDownloading() { return new File(game, ".steam-download"); }

    /** Files under data/ the game opens at start-up and cannot run without: one missing (a copy
     *  cut short, another release's layout) stopped it on a black screen (GitHub #18, #22). */
    static final String[] REQUIRED = {
        // FF8 (Steam 2013): the per-language archives and the sound banks
        "lang-en/battle.fs", "lang-en/field.fs", "lang-en/magic.fs", "lang-en/main.fs", "lang-en/menu.fs",
        "lang-en/world.fs", "Sound/audio.fmt", "Sound/audio.dat",
    };

    /** The REQUIRED files missing or empty in the imported data folder (names matched ignoring
     *  case, as the game does), or an empty list. */
    List<String> missingFiles() {
        List<String> out = new ArrayList<>();
        for (String rel : REQUIRED) {
            File f = new File(game, "Data");
            for (String part : rel.split("/")) {
                File next = new File(f, part);
                if (!next.exists()) {
                    String[] names = f.list();
                    if (names != null) for (String n : names) if (n.equalsIgnoreCase(part)) { next = new File(f, n); break; }
                }
                f = next;
            }
            if (!f.isFile() || f.length() == 0) out.add("Data/" + rel);
        }
        return out;
    }

    /** What to tell the player when files are missing, or null. */
    String missingMessage() {
        List<String> m = missingFiles();
        if (m.isEmpty()) return null;
        return "Some of the game's files are missing from the imported copy, so it can't start: "
                + String.join(", ", m) + ".\n\nThe copy may have been cut short, or the folder picked holds an incomplete "
                + "install. Check that the game is fully installed (in Steam or GameNative: verify the game files), then "
                + "import it again: Game files → Import again from another folder.";
    }

    // ---------------------------------------------------------------- import from a folder
    /** A document in the picked tree. */
    private static final class Doc { String id, name; boolean dir; long size = -1; }

    /** An import continued after a pause or a stop (JobService): files already copied whole
     *  (same size) are kept. A new import copies everything, so it also repairs damaged files. */
    boolean skipCopied;

    private List<Doc> children(Uri tree, String parentId) {
        List<Doc> out = new ArrayList<>();
        Uri u = DocumentsContract.buildChildDocumentsUriUsingTree(tree, parentId);
        String[] cols = { DocumentsContract.Document.COLUMN_DOCUMENT_ID, DocumentsContract.Document.COLUMN_DISPLAY_NAME,
                          DocumentsContract.Document.COLUMN_MIME_TYPE, DocumentsContract.Document.COLUMN_SIZE };
        try (Cursor c = ctx.getContentResolver().query(u, cols, null, null, null)) {
            while (c != null && c.moveToNext()) {
                Doc d = new Doc();
                d.id = c.getString(0);
                d.name = c.getString(1);
                d.dir = DocumentsContract.Document.MIME_TYPE_DIR.equals(c.getString(2));
                if (!c.isNull(3)) d.size = c.getLong(3);
                out.add(d);
            }
        }
        return out;
    }

    private Doc child(Uri tree, String parentId, String name) {
        for (Doc d : children(tree, parentId)) if (d.name.equalsIgnoreCase(name)) return d;
        return null;
    }

    private Doc path(Uri tree, String rootId, String... names) {
        String id = rootId;
        Doc d = null;
        for (String n : names) {
            d = child(tree, id, n);
            if (d == null) return null;
            id = d.id;
        }
        return d;
    }

    /** What to pick, said when the exe found is not the one the game is built from. */
    static final String WHICH_FOLDER = "FF8Recomp needs FINAL FANTASY VIII as sold on Steam (the 2013 release, English), "
            + "unpatched: pick the game's main folder, \"FINAL FANTASY VIII\" in Steam's steamapps/common (the one that "
            + "holds FF8_EN.exe and Data). The Remastered edition (\"FINAL FANTASY VIII - REMASTERED\") is not supported yet";

    /** The Remastered's folder holds this instead of FF8_EN.exe. */
    static final String FROM_REMASTERED = "this is FINAL FANTASY VIII - REMASTERED. FF8Recomp runs the 2013 release for "
            + "now (\"FINAL FANTASY VIII\" on Steam, the one with FF8_EN.exe); the Remastered will follow";

    /**
     * The exe as Steam or the player's copy has it, checked and unwrapped into game/ff8_en.exe.
     * Returns null, or what is wrong (seen: where it came from, for the message).
     */
    private String installExe(File copied, String seen) throws IOException {
        byte[] d = readAll(new java.io.FileInputStream(copied));
        String sha = sha1(d);
        byte[] plain;
        if (GameBuild.WRAPPED_SHA1.equals(sha)) plain = SteamStub.unwrap(d);
        else if (GameBuild.EXE_SHA1.equals(sha)) plain = d;               // unwrapped already (tools/import_game.py)
        else return seen + " (SHA-1 " + sha.substring(0, 12) + "…) is not the FF8_EN.exe the game is built from. "
                + WHICH_FOLDER + " (If your copy is from Steam and unpatched, please report this number.)";
        if (!GameBuild.EXE_SHA1.equals(sha1(plain))) return "FF8_EN.exe did not unwrap as expected. Please report this";
        File out = new File(game, "ff8_en.exe"), tmp = new File(game, "ff8_en.exe.new");
        try (OutputStream o = new FileOutputStream(tmp)) { o.write(plain); }
        if (out.exists() && !out.delete()) throw new IOException("cannot replace " + out);
        if (!tmp.renameTo(out)) throw new IOException("cannot write " + out);
        return null;
    }

    private static String sha1(byte[] d) {
        try {
            StringBuilder s = new StringBuilder();
            for (byte b : java.security.MessageDigest.getInstance("SHA-1").digest(d)) s.append(String.format(Locale.ROOT, "%02x", b));
            return s.toString();
        } catch (java.security.NoSuchAlgorithmException e) { throw new IllegalStateException(e); }
    }

    /**
     * Import from the folder the player picked: Steam's "FINAL FANTASY VIII" folder, or any folder
     * holding FF8_EN.exe and Data (with lang-en). The exe is checked before anything is copied.
     * Returns null on success, else what is wrong.
     */
    String importGame(Uri tree, Progress p) throws IOException {
        String root = DocumentsContract.getTreeDocumentId(tree);
        List<Doc> exes = new ArrayList<>();
        List<String> exeWhere = new ArrayList<>();
        Doc atRoot = path(tree, root, "FF8_EN.exe");
        if (atRoot != null) { exes.add(atRoot); exeWhere.add("FF8_EN.exe"); }
        List<Doc> datas = new ArrayList<>();
        Doc dataRoot = path(tree, root, "Data");
        if (dataRoot != null && path(tree, dataRoot.id, "lang-en") != null) datas.add(dataRoot);
        p.update("Looking for the game files");
        scan(tree, root, "", 0, exes, exeWhere, datas);
        if (exes.isEmpty()) {
            if (path(tree, root, "FFVIII_EFIGS.dll") != null || path(tree, root, "FFVIII.exe") != null) return FROM_REMASTERED;
            return "no FF8_EN.exe was found in this folder. " + WHICH_FOLDER;
        }
        if (datas.isEmpty()) return "the game's Data folder (with lang-en) was not found in this folder. " + WHICH_FOLDER;
        game.mkdirs();
        p.update("Checking FF8_EN.exe");
        File tmp = new File(game, "FF8_EN.exe.copy");
        String err = null;
        java.util.Set<String> tried = new java.util.HashSet<>();
        for (int i = 0; i < exes.size(); i++) {
            if (!tried.add(exes.get(i).id)) continue;
            copyDoc(tree, exes.get(i).id, tmp);
            err = installExe(tmp, exeWhere.get(i));
            if (err == null) break;
        }
        tmp.delete();
        if (err != null) return err;
        copyTree(tree, datas.get(0).id, new File(game, "Data"), p, "Data");
        List<String> missing = missingFiles();
        if (!missing.isEmpty())
            return "the game's Data folder here is incomplete, these files are missing: " + String.join(", ", missing)
                    + ". Check that the game is fully installed (in Steam or GameNative: verify the game files), then import again";
        return null;
    }

    /**
     * Downloads the game from Steam (SteamFiles) into the game folder, as importGame would copy it.
     * Returns null on success, else what is wrong. exeChanged[0]: ff8_en.exe is new (build again).
     */
    String downloadFromSteam(SteamLink link, Progress p, java.util.concurrent.atomic.AtomicBoolean cancel, boolean[] exeChanged) throws Exception {
        game.mkdirs();
        File exe = new File(game, "ff8_en.exe");
        String before = exe.isFile() ? GameBuild.sha1(exe) : null;
        File mark = steamDownloading();
        if (!mark.isFile()) new FileOutputStream(mark).close();
        String err = new SteamFiles(link).downloadGame(game, p::update, cancel);
        if (err != null) return err;
        err = installExe(new File(game, SteamFiles.WRAPPED_EXE), "Steam's FF8_EN.exe");
        if (err != null) return err;
        exeChanged[0] = !GameBuild.sha1(exe).equals(before);
        List<String> missing = missingFiles();
        if (!missing.isEmpty()) return "these files did not come with the download: " + String.join(", ", missing) + ". Please report this";
        mark.delete();
        return null;
    }

    /** Folders the search skips: the game's own data (searched as a whole), and big unrelated ones. */
    private static final java.util.Set<String> SKIP = new java.util.HashSet<>(java.util.Arrays.asList(
            "data", "movies", "music", "sound", "shaders", "mods", "textures", "plugins", "licenses",
            "redist", "__support", "_commonredist", "lang-en", "lang-fr", "lang-de", "lang-es", "lang-it", "lang-jp"));

    /**
     * Search the picked folder, 5 levels deep, for FF8_EN.exe files and the game's Data folder (a
     * "Data" folder holding "lang-en"), appended after the known places.
     */
    private void scan(Uri tree, String id, String rel, int depth, List<Doc> exes, List<String> exeWhere, List<Doc> datas) {
        if (depth > 5) return;
        for (Doc d : children(tree, id)) {
            String lower = d.name.toLowerCase(Locale.ROOT);
            String here = rel.isEmpty() ? d.name : rel + "/" + d.name;
            if (!d.dir) {
                if (lower.equals("ff8_en.exe")) { exes.add(d); exeWhere.add(here); }
                continue;
            }
            if (lower.equals("data")) {
                for (Doc k : children(tree, d.id)) {
                    String kl = k.name.toLowerCase(Locale.ROOT);
                    if (k.dir && kl.equals("lang-en")) { if (!datas.contains(d)) datas.add(d); break; }
                }
                continue;
            }
            if (SKIP.contains(lower)) continue;
            scan(tree, d.id, here, depth + 1, exes, exeWhere, datas);
        }
    }

    private void copyTree(Uri tree, String id, File dst, Progress p, String rel) throws IOException {
        dst.mkdirs();
        for (Doc d : children(tree, id)) {
            String lower = d.name.toLowerCase(Locale.ROOT);
            // English only: each language of the 2013 release is its own exe (FF8_EN.exe is English)
            if (rel.equals("Data") && d.dir && lower.startsWith("lang-") && !lower.equals("lang-en")) continue;
            File out = new File(dst, d.name);
            if (d.dir) copyTree(tree, d.id, out, p, rel + "/" + d.name);
            else {
                JobService.checkPause();       // Pause (#53): what is copied stays
                // copied whole before a pause or a stop (copy() writes .part, then renames)
                if (skipCopied && d.size > 0 && out.isFile() && out.length() == d.size) continue;
                p.update("Copying " + rel + "/" + d.name);
                copyDoc(tree, d.id, out);
            }
        }
    }

    private void copyDoc(Uri tree, String id, File dst) throws IOException {
        Uri u = DocumentsContract.buildDocumentUriUsingTree(tree, id);
        try (InputStream in = ctx.getContentResolver().openInputStream(u)) {
            if (in == null) throw new IOException("cannot read " + id);
            copy(in, dst);
        }
    }

    /** Copy a picked file (a mod archive) into dir, keeping its display name. */
    File importFile(Uri uri, File dir) throws IOException {
        String name = "mod.iro";
        ContentResolver cr = ctx.getContentResolver();
        try (Cursor c = cr.query(uri, new String[] { android.provider.OpenableColumns.DISPLAY_NAME }, null, null, null)) {
            if (c != null && c.moveToFirst() && c.getString(0) != null) name = c.getString(0);
        }
        File out = new File(dir, new File(name).getName());
        try (InputStream in = cr.openInputStream(uri)) {
            if (in == null) throw new IOException("cannot read " + uri);
            copy(in, out);
        }
        return out;
    }

    /** Copy a picked folder (a mod folder) into dir. */
    File importFolder(Uri tree, File dir, Progress p) throws IOException {
        String root = DocumentsContract.getTreeDocumentId(tree);
        String name = "mod";
        try (Cursor c = ctx.getContentResolver().query(DocumentsContract.buildDocumentUriUsingTree(tree, root),
                new String[] { DocumentsContract.Document.COLUMN_DISPLAY_NAME }, null, null, null)) {
            if (c != null && c.moveToFirst() && c.getString(0) != null) name = c.getString(0);
        }
        File out = new File(dir, name);
        copyTree(tree, root, out, p, name);
        return out;
    }

    private static void copy(InputStream in, File dst) throws IOException {
        File tmp = new File(dst.getPath() + ".part");
        dst.getParentFile().mkdirs();
        try (OutputStream out = new FileOutputStream(tmp)) {
            byte[] buf = new byte[1 << 20];
            int n;
            while ((n = in.read(buf)) > 0) out.write(buf, 0, n);
        }
        if (dst.exists() && !dst.delete()) throw new IOException("cannot replace " + dst);
        if (!tmp.renameTo(dst)) throw new IOException("cannot write " + dst);
    }

    // ------------------------------------------------------------ FFNx run-time files
    /** FFNx's shaders and data tables from the APK (assets/ffnx), once per app version. */
    void installRuntime() throws IOException {
        long version;
        try {
            version = ctx.getPackageManager().getPackageInfo(ctx.getPackageName(), 0).lastUpdateTime;
        } catch (Exception e) {
            version = 0;
        }
        File marker = new File(game, ".ffnx-runtime");
        String want = Long.toString(version);
        if (marker.isFile() && want.equals(new String(readAll(new java.io.FileInputStream(marker)), StandardCharsets.UTF_8))) return;
        game.mkdirs();
        copyAssets(ctx.getAssets(), "ffnx", game);
        // 7th Heaven's variable aliases for mods' run-time conditions, next to the profile.
        File mods = new File(files, "mods");
        mods.mkdirs();
        try (InputStream in = ctx.getAssets().open("modsdata/7thHeaven.var")) { copy(in, new File(mods, "7thHeaven.var")); }
        try (OutputStream o = new FileOutputStream(marker)) { o.write(want.getBytes(StandardCharsets.UTF_8)); }
    }

    private static void copyAssets(AssetManager am, String path, File dst) throws IOException {
        String[] kids = am.list(path);
        if (kids == null || kids.length == 0) {                 // a file
            if (path.endsWith("FFNx.toml")) {                    // the template; the real one is written by writeFfnxToml
                try (InputStream in = am.open(path)) { copy(in, new File(dst.getParentFile(), "FFNx.template.toml")); }
                return;
            }
            try (InputStream in = am.open(path)) { copy(in, dst); }
            return;
        }
        dst.mkdirs();
        for (String k : kids) copyAssets(am, path + "/" + k, new File(dst, k));
    }

    private static byte[] readAll(InputStream in) throws IOException {
        try (InputStream i = in) {
            ByteArrayOutputStream b = new ByteArrayOutputStream();
            byte[] buf = new byte[65536];
            int n;
            while ((n = i.read(buf)) > 0) b.write(buf, 0, n);
            return b.toByteArray();
        }
    }

    /**
     * FFNx.toml from FFNx's own defaults (the template in the APK) with the Android settings,
     * the launcher's choices, then each enabled mod's FFNxConfig (later mods first, so the
     * highest-priority mod's values win, as files do).
     */
    /** FFNx's Steam achievement code on (signed in to Steam: SteamLink.java). */
    boolean steamAchievements;
    boolean openGl;             // Settings > Display > Renderer: OpenGL ES instead of Vulkan
    boolean crtFilter;          // Settings > Display > CRT filter (FFNx crt_filter, an FF7Recomp edit)
    boolean analogControls;     // Settings > Controller > Analog stick (FFNx's analogue controls and auto run)
    int resolutionScale;        // Settings > Display > Resolution: 1-4 times the original, 0 automatic
    /** Settings > Sound and Game options (FfnxOptions): changed FFNx.toml keys, over the mods' values. */
    java.util.Map<String, String> ffnxOptions = new java.util.HashMap<>();

    /** The game's languages: FFNx's game_language, the launcher's name, the Data/lang-* folder.
     *  FF8's 2013 exes are one per language; FF8_EN.exe is English. */
    static final String[][] LANGUAGES = { { "1", "English", "en" } };

    /** FFNx game_language (1 English). */
    int gameLanguage = 1;

    /** The Data/lang-xx code of a game_language (en when unknown). */
    static String languageCode(int language) {
        for (String[] l : LANGUAGES) if (Integer.parseInt(l[0]) == language) return l[2];
        return "en";
    }

    /** Whether the imported game files hold that language's data folder (Data/lang-xx). */
    boolean hasLanguage(int language) {
        for (String[] l : LANGUAGES)
            if (Integer.parseInt(l[0]) == language) return new File(game, "Data/lang-" + l[2]).isDirectory();
        return false;
    }

    void writeFfnxToml(int fpsLimiter, boolean turbo, int aspectRatio, boolean showFps, List<ModLibrary.Mod> mods, int scaleCap) throws IOException {
        File template = new File(game, "FFNx.template.toml");
        String toml = new String(readAll(new java.io.FileInputStream(template)), StandardCharsets.UTF_8);
        toml = set(toml, "renderer_backend", openGl ? "1" : "5");   // 1: OpenGL ES on Android (an FF7Recomp edit), 5: Vulkan
        toml = set(toml, "fullscreen", "true");
        // 2x speed: FFNx's speedhack at 2x from the start (L3 then L2/R2 toggles it). With 60 or 120
        // FPS chosen the game keeps its 60 FPS timing, which at 2x is shown at 120 FPS.
        if (turbo && fpsLimiter > 3) fpsLimiter = 3;
        toml = set(toml, "ff7_fps_limiter", Integer.toString(fpsLimiter));
        // FF8 reads its own key (FFNx cfg.cpp): 0 original, 1 default, 2 30 FPS battles, 3 60 FPS;
        // FFNx clamps it at 3 (no 120 FPS for FF8)
        toml = set(toml, "ff8_fps_limiter", Integer.toString(Math.min(fpsLimiter, 3)));
        toml = set(toml, "speedhack_min", turbo ? "2.0" : "1.0");
        toml = set(toml, "show_fps", showFps ? "true" : "false");
        toml = set(toml, "crt_filter", crtFilter ? "true" : "false");
        // FFNx's version and renderer lines: debug text drawn every frame, not worth the work
        toml = set(toml, "show_version", "false");
        toml = set(toml, "show_renderer_backend", "false");
        toml = set(toml, "enable_steam_achievements", steamAchievements ? "true" : "false");
        toml = set(toml, "trace_achievement", steamAchievements ? "true" : "false");
        // the re-release's English exe in French, German or Spanish (FFNx's multilanguage patches)
        toml = set(toml, "game_language", Integer.toString(hasLanguage(gameLanguage) ? gameLanguage : 1));
        // 16:9 (FFNx aspect_ratio 2) also uncrops: the Thor's screen is 16:9 top to bottom.
        toml = set(toml, "aspect_ratio", Integer.toString(aspectRatio));
        toml = set(toml, "enable_uncrop", aspectRatio != 0 ? "true" : "false");
        // FFNx's analogue controls: the left stick walks when tilted a little and runs when tilted
        // all the way; the right stick and L2/R2 move the camera in battles (GitHub #25). Off: as
        // the template or a mod sets them.
        if (analogControls) {
            toml = set(toml, "enable_analogue_controls", "true");
            toml = set(toml, "enable_auto_run", "true");
        }
        for (int i = mods.size() - 1; i >= 0; i--) {
            ModLibrary.Mod m = mods.get(i);
            if (!m.enabled || m.error != null) continue;
            for (String[] kv : m.ffnx)
                if (m.optionsMatch(kv.length > 2 ? kv[2] : null)) toml = set(toml, kv[0], tomlValue(kv[1]));
        }
        for (java.util.Map.Entry<String, String> o : ffnxOptions.entrySet()) toml = set(toml, o.getKey(), o.getValue());
        // Settings > Display > Resolution: a chosen multiple of the original resolution, whatever
        // the mods or Performance would pick (0 = automatic, below)
        if (resolutionScale > 0) {
            toml = set(toml, "internal_resolution_scale", Integer.toString(resolutionScale));
        } else
        // Performance: the internal resolution at most scaleCap times 640x480 (mods ask for 4x,
        // which on a 1080p screen renders 2.4 times the pixels shown; 0 = as the mods set it)
        if (scaleCap > 0) {
            java.util.regex.Matcher sm = java.util.regex.Pattern.compile("(?m)^internal_resolution_scale\\s*=\\s*(\\d+)").matcher(toml);
            int cur = sm.find() ? Integer.parseInt(sm.group(1)) : 0;
            if (cur == 0 || cur > scaleCap) toml = set(toml, "internal_resolution_scale", Integer.toString(scaleCap));
        }
        File out = new File(game, "FFNx.toml");
        try (OutputStream o = new FileOutputStream(out)) { o.write(toml.getBytes(StandardCharsets.UTF_8)); }
    }

    /** files/companion.txt for src/ffnx_port/companion_render.cpp: the bottom screen's battle windows. */
    void writeCompanion(String battleWindows) throws IOException {
        String s = "battlewindows=" + battleWindows + "\n";
        try (OutputStream o = new FileOutputStream(new File(files, "companion.txt"))) { o.write(s.getBytes(StandardCharsets.UTF_8)); }
    }

    /** files/boosters.txt for src/ffnx_port/boosters.cpp: the game boosters a game starts with. */
    void writeBoosters(int speed, boolean noEncounters, boolean battlePlus, boolean frameSkip, boolean saveAnywhere, int gil,
                       int exp, int ap, boolean autofire, int rumble) throws IOException {
        String s = "speed=" + speed + "\nencounters=" + (noEncounters ? "off" : "on") + "\nbattle=" + (battlePlus ? "on" : "off")
                + "\nframeskip=" + (frameSkip ? "on" : "off") + "\n"
                + "saveanywhere=" + (saveAnywhere ? "on" : "off") + "\n"   // src/host/ff7_hext.cpp, at boot
                + "gil=" + gil + "\n"                                       // gil from battles times this
                + "exp=" + exp + "\n" + "ap=" + ap + "\n"               // EXP and AP too (src/ffnx_port/boosters.cpp)
                + "autofire=" + (autofire ? "on" : "off") + "\n"          // battles: confirm held repeats (#33)
                + "rumble=" + rumble + "\n";                              // 0 off, 1 light, 2 strong (rumble.cpp)
        try (OutputStream o = new FileOutputStream(new File(files, "boosters.txt"))) { o.write(s.getBytes(StandardCharsets.UTF_8)); }
    }

    /** files/quickboot.txt for src/ffnx_port/quickboot.cpp: "newgame", "continue", or none. */
    void writeQuickboot(String mode) throws IOException {
        File f = new File(files, "quickboot.txt");
        if (mode == null || mode.isEmpty()) { if (f.exists() && !f.delete()) throw new IOException("cannot remove " + f); return; }
        try (OutputStream o = new FileOutputStream(f)) { o.write(mode.getBytes(StandardCharsets.UTF_8)); }
    }

    private static final Pattern NUMBER = Pattern.compile("-?\\d+(\\.\\d+)?");

    private static String tomlValue(String v) {
        String t = v.trim();
        if (NUMBER.matcher(t).matches() || t.equalsIgnoreCase("true") || t.equalsIgnoreCase("false")) return t.toLowerCase(Locale.ROOT);
        if (t.startsWith("[") || t.startsWith("\"")) return t;
        if (t.contains(",")) {                                   // a list from <key><item/>...</key>
            StringBuilder b = new StringBuilder("[");
            for (String s : t.split(",")) b.append(b.length() > 1 ? ", " : "").append(tomlValue(s));
            return b.append("]").toString();
        }
        return "\"" + t.replace("\\", "\\\\").replace("\"", "\\\"") + "\"";
    }

    /** Replace `key = ...` (uncommented, at line start) or append it. */
    private static String set(String toml, String key, String value) {
        Pattern p = Pattern.compile("(?m)^" + Pattern.quote(key) + "\\s*=.*$");
        Matcher m = p.matcher(toml);
        if (m.find()) return m.replaceFirst(Matcher.quoteReplacement(key + " = " + value));
        return toml + "\n" + key + " = " + value + "\n";
    }
}
