# FF8Recomp notes

A dated log of findings: cause, change, how it was tested. FF7Recomp's own log stays in its
repository.

## 2026-11-21 Start

- Forked FF7Recomp at `54897c5` (v1.1.37) with fresh history. Renamed the app to
  `com.ff8recomp` (package, JNI names, test intents, label, update repository, keystore path).
  Source file names (`ff7_*.c`, `libff7.so`) kept FF7Recomp's names until 2026-11-22 (now `ff8_*`, `libff8.so`). Not built yet.
- The user owns both Steam editions and wants both supported. Remastered route: lift its own DLL
  (not Remastered assets on FFNx). See `ROADMAP.md`.
- Both editions carry the same original 2000 game program: `FF8_EN.exe` and the header of
  `FFVIII_EN.exe.dat` share timestamp 955151253 and SizeOfCode `0x768000`. Found by reading the
  PE/COFF headers; the encrypted body of the `.dat` is not decoded yet.
- `FF8_EN.exe` is SteamStub-wrapped (entry point in `.bind`). `FFVIII_EFIGS.dll` is not; it
  imports OPENGL32, DSOUND, DINPUT8, XINPUT9_1_0, steam_api, zlibwapi and libvgmstream.

## 2026-11-21 M0: inputs, catalog, FFNx's FF8 half
- `FF8_EN.exe` (buildid 296320) is SteamStub 3.0, flags 0x26: 0x04 = no code encryption, so
  `tools/unwrap_steamstub.py` only restores the entry point 0x55ADB7 (MSVC mainCRTStartup;
  FFNx's `ff8_externals.start` for VERSION_FF8_12_US_NV). Version bytes 0x3885048D / 0x1597C8.
- Imports: like FF7's plus binkw32 (9), EAX.DLL (ordinal), ADVAPI32 registry (5), and 8
  `dotemu*` file/registry functions from AF3DN.P.
- Image is 40 MB (0x400000-0x282B000; .data 28 MB of mostly .bss), so game memory now
  starts at 0x03000000 (`src/host/game_image.h`).
- Catalog: 57,654 functions in 257 s, seeds = entry only (FFNx walks every FF8 address from
  the entry, so recursive descent reaches them).
- FFNx's FF8 half had only had its header converted: all four rewrite passes in
  `tools/ffnx_edit_android.py` and the `calls` pass skipped `ff8*` files. Turned on, they
  rewrote 167 casted game calls, 17 patch values, 6 function-pointer variables, 24 casts.
- `-fms-extensions -Wno-microsoft` let 126 pointer-to-uint32 truncations compile silently
  in FF8 code. FF8 files now build with `-Werror=pointer-to-int-cast`; each site goes
  through `ffnx_va()` (checked: guest pointers pass, native functions become thunks, host
  pointers stop the run by name).
- Left out for now (they relocate game tables into FFNx memory): kernel_ability/command/
  item/magic, battle/monsters, menu/volume, vibration, remaster. Vanilla stand-ins in
  `src/ffnx_port/ff8_later_android.cpp`.
- Probe: the 25 FF8 core files compile with 0 errors. Not linked or run yet.

## 2026-11-21 M1: FF8 on the Thor (release APK, FFNx, Vulkan)
- Headless: CRT, registry and DirectDraw setup pass; it stops where FF8 starts its own
  Direct3D renderer (FFNx replaces that). Needed: pcrecomp 0022 (SSE register file in
  win32hle), 0023 (lift `smsw`: FF8's CRT tests CR0.EM for an FPU), 0024 (ordinal imports
  by "DLL#N": EAX.DLL#5 was bound to 0), `src/hle/ff8_win32.c` (FF8's extra imports).
- APK: FFNx attaches (FF8 1.2 US NV, Steam 2013), creates the window and renderer, the game
  draws the Squaresoft logo, the title screen, and plays the opening movie (FFmpeg).
  Confirm is XInput B (4096) on the test API; the logo waits for a button.
- Bugs found on the way, each now stopped by name if it comes back:
  - `_O_BINARY` was 0 in our io.h; FFNx passes it to the *game's* CRT `_open`, which then
    opened archives in text mode and stopped at the first Ctrl-Z (menu.fi, offset 217).
    io.h keeps MSVC's values; host-side opens translate them.
  - win32hle's GetFileType said FILE_TYPE_CHAR for files (pcrecomp 0025).
  - FFNx data the game reads: AI pair tables, font widths (guest block), movie camera
    data (guest heap), fake DirectDraw objects (guest COM objects, fake_dd_guest.cpp).
  - Metadata::init read past a missing `user_<id>`; user data is now `<game>/save`.
  - `(void *)fn` replacements (55) and the variadic `ff8_open` needed typed thunks.
- Open: a fault in the game's CRT (0x55E0F9, pointer 1) when the first field loads after
  the opening movie. Music: the game's DirectMusic fails (FFNx external music not set up).

## 2026-11-21 M2 (in progress): field, save states, first battles
- Field: the infirmary, naming, Quistis's FMV and the classroom play. The CRT fault was
  FFNx's mid-function `texture_reload_fix` hooks (0x46545F, 0x4657DD): `replace_function`
  at a non-entry site now writes a jmp and is recorded as a code write; manifest
  `config/ffnx_manifests/ffnx_manifest_ff8_l1.json` (293 hooks, 109 code writes).
- Save states (Select+L1 / Select+R1) and quickboot `resume N`: a state loads only in the
  field at the depth it was saved at, so resume goes New Game → skip the movie → load.
  States belong to the build that made them.
- Battles: tested from a 2000-PC save set (17 saves across discs 1-3; `game/test_saves/`,
  git-ignored, source savegame.pro). The 2000 saves are the same format as the 2013 ones:
  rename `Save/SlotN/saveNN` to `slotN_saveNN.ff8`, push to `game/save/user_0/`, then
  Continue → Slot 1 (`tools/ff8_load_block.sh`). Random battles on the world map run to
  victory and the EXP screen (Esthar plains, Lv 65 party; a Grendel fight on disc 2).
- Battle bugs, all FFNx code using 32-bit habits (tools/ffnx_edit_ff8.py):
  - guest pointer cells read as 8-byte `T *`: `ff8_battle_get_monster_name` (it replaces
    sub_495100, so every battle opened with "host pointer given to the guest"), the battle
    text voice hooks, card names, draw-point messages, field question windows
    (dword_1D76608), the world-map Selphie model patch (dword_24FEE48).
  - game addresses cast to native function types and called from the host (the host
    jumped to 0x571900 / 0x502270): magic effect loading (`load_magic_data_sub_571900`),
    the anim-set binder (`ff8_set_com_file_data` typedef), the esc menu icon draw.
    rewrite_game_fn_casts only sees direct `((T(*)(..))addr)(..)` calls; `auto x = (cast)`
    and typedef'd forms are edited by hand.
- Note: voice.cpp still truncates `&execute_opcode_table[..]` with `(uint32_t)` in its
  voice-acting setup (warnings, only with voice mods); the shared files are not built with
  -Werror=pointer-to-int-cast yet.

## 2026-11-21 Toward a public 0.1: music, FF8 game files, launcher
- Music. The 2013 release plays DirectMusic segments (Data/Music/dmusic/*.sgt) with one DLS
  collection (FF8.dls, DLS1, mostly 8-bit 22 kHz samples). Android has none of it, so FFNx's
  external-music path is on for the 2013 release too: FFNx already replaces every FF8 music call
  (play, crossfade, battle swap, resume, Eyes On Me, the FH concert segments) and only asks for a
  file by name. src/ffnx_port/dmusic resolves the name ("battle" -> 005s-battle.sgt; "s" over
  "xg" variants; Data/Music/stream/*.wav go to vgmstream) and plays segments: sequence track
  (evtl 20-byte items, curl 28-byte DX6 curves), tempo (tetr), band (bins: patch, pchannel,
  flags), segh DX6 = repeats, length, play start, loop start, loop end. DLS ptbl offsets count
  from after the wvpl form type. Units: wsmp gain 1/655360 dB, EG times timecents*65536, sustain
  0.1 % * 65536, EG1 decay = time for the full 96 dB. All 92 segments render on the PC
  (tools/sgt_render.cpp); on the Thor intro, world map and battle music play and switch.
- An audit of FF7-only native paths found FFNx's override/call-site tables sized to FF7's .text
  (0x7B6000): FF8 hooks above it were refused (the "six call sites that could not be hooked").
  Now GAME_TEXT_HI; two former mid-function jmps (0xB664A0, 0xB66560) are plain entry overrides.
- Game files: FF8_EN.exe (wrapped, SHA-1 03230c11...) is unwrapped on import in Java
  (SteamStub.java, flags 0x26: only the entry point changes) to ff8_en.exe (d0f2ae94...).
  Steam download: app 39150, Windows English depots, FF8_EN.exe + Data. The kit carries
  config/catalog.json.gz (addresses only) and the FF8 manifest.
- Launcher: FF8 text; achievements, cloud saves, boosters, mods, save import and the second
  screen are hidden until they are made for FF8. New icon (tools/make_icon.py).
- M3 on the Thor: the public APK (103 MB, no game code; kit 1d33859641f71923) built the game from
  the imported ff8_en.exe in 875 s (634 MB of C, 4 compiles at once at ~80 C) and played it:
  world map, battles to the EXP screen, music. An APK update with the same kit keeps the build.
  (Building the kit needs config/clang-device.profdata.gz, git-ignored, copied from FF7Recomp:
  without it build_device_clang.sh rebuilds clang for ~40 minutes.)

## 2026-11-21 The Remastered edition, through FFNx's Remastered mode
- FFVIII_EN.exe.dat is Dotemu's serialized image, zlib per section, not encrypted
  (tools/remaster_extract.py; RemasterExe.java on the device, same bytes: SHA-1 cebb705a...).
  It is the same 1.2 US NV program as the 2013 FF8_EN.exe: .rdata/.data identical, .text 261
  bytes apart (58 call operands through Dotemu's AF3DN.P wrappers in 2013, one 30-byte patch
  at 0x5699B7). FFVIII_EFIGS.dll is Dotemu's own static recompile of it: not needed.
- FFNx runs the Remastered as on the PC (FFNx-FF8_2000 + the Remastered's data): the original
  program, FFVIII.exe beside it as the marker, main.zzz / other.zzz read by ff8/zzz_archive.cpp
  through its file hooks. remaster.cpp is built now (archives opened at FFNx's host basedir; a
  model pointer read as a guest cell). The 2013 exe cannot stand in: its startup path code is
  where the two differ (it spun in 0x401200's drive search); the Remastered's program is lifted.
- Its code patches differ (232 more code writes, the file hooks' replace_function sites):
  config/ffnx_manifests/ffnx_manifest_ff8_remaster.json, recorded in Remastered mode
  (tools/ff8_manifest.sh remaster). The lift takes every manifest, so one kit serves both.
- Steam redirection (dotemuCreateFileA) is skipped in Remastered mode (the 2000 program opens
  files itself; FFNx's hooks map them into the archives).
- Import: a folder with main.zzz (or FFVIII.exe / FFVIII_EFIGS.dll) imports as the Remastered:
  FFVIII_EN.exe.dat rebuilt into ff8_en.exe, the two archives copied, the marker written. Steam:
  the 2013 release when the account has it, else the Remastered (app 1026680).

## 2026-11-21 Device tests of the 2013 edition, test points, mods
- Tested on the Thor: in-game save (slot1_save27.ff8 + metadata.xml), OpenGL ES renderer,
  60 FPS (ff8_fps_limiter 3), Report a problem, folder import through Android's picker.
- The test API's pad in FF8 (FFNx's Steam layout): 0x1000 confirm, 0x2000 menu, 0x8000 cancel.
- Test points (Hyne's location table names the saves): save 01 B-Garden Hall (Triple Triad,
  Balamb plains battles), 03 Laguna dream 1 at the Galbadia Hotel (Julia's Eyes On Me), 09
  Deling Gateway (disc 1 -> 2 in ~15 min), 10 MD Level (FH concert ~1 h on), 11 Garden deck
  (disc 2 -> 3), 28/30 disc 3 with the Ragnarok, Slot2-02 Lunar Base (FMVs, vocal Eyes On Me).
- Mods: Junction VIII's catalog is 7th Heaven's format with iroj:// links and .iroj archives
  (same IRO container): the launcher's catalog/download code takes both; the overlay (src/mods)
  is unchanged. Default subscription: Tsunamods Catalog (Junction-VIII-Catalogs canary).

## 2026-11-21 Remastered boot: the drive search, and hooks above FF7's .text
- The 2000 program's game paths (0x401200) ask GetDriveTypeA for its DataDrive ("CD:"), and if
  that is not the drive type it wants, try c:, d:, ... until one is. FFNx's Remastered mode
  patches the wanted type to DRIVE_NO_ROOT_DIR (+0x1F0), which "CD:" is on a PC, so the search
  never runs. The CRT shim answered DRIVE_FIXED for everything: an endless loop (the watchdog's
  0x401200 + sprintf). src/hle/ff8_win32.c answers as Windows does (c: fixed, the rest
  no root). The 2013 exe asks AF3DN.P (dotemuGetDriveTypeA) instead and is unaffected.
- src/host/portable_guest.h and src/interp/x86_interp.c still bounded entry overrides and
  call-site hooks by FF7's .text end (0x7B6000): FFNx hooks above it were installed but never
  ran. FF8's .text runs to 0xB69000 (its CRT at the end): 8 hooks in the 2013 manifest
  (0x8DDD87.., 0xB664A0, 0xB66560, 0x84F958) and 11 in the Remastered's, among them
  _filelength (0xB67C00), so every file read from main.zzz got size -1. Both now use
  game_image.h (the kit carries it for the generated code; a new lift version).
- files/ffnx_debug.toml: key = value lines over the launcher's FFNx.toml (trace_files etc.);
  files/env.txt FF7_FFNX_MANIFEST=<path> now writes each hook's "installed".
- FFNx calls the game's CRT through gfn<int(int, long, int)> (_lseek, _lseek_lk). Android's
  long is 64 bits: the offset went over as two stack words and whence (SEEK_END) landed in
  the high one, so every "seek to the end" was a seek to 0 and every file size 0. guest_abi.h
  now passes a long / unsigned long as one word and takes it back from eax alone, as
  Windows' 32-bit long is. (The Remastered's save list: LZS decompressing an empty buffer.)
- The Remastered's archive files went to the game as host descriptors (59, ...). The world
  map's file layer (0x51BF50) takes any descriptor as an index into its own 20-entry table at
  0xB8D480 and freed the "file" of "file_system.cpp" that follows it. On Windows FFNx's own
  CRT numbers them from 3, like the game's: zzz_archive now hands the game small numbers
  (19 down to 10, away from the game's own from 3), the host descriptor kept inside.

## 2026-11-21 The Remastered plays; mods on it; the field opcode table
- The Remastered on the Thor: HD title, block list with the new portraits, fields with the
  HD models, the world map, a battle (Ruby Dragon), its own OGG music from other.zzz.
- Mods (Junction VIII's catalog): SkipIntro (5 .hext files) applies after FFNx now, as FFNx's
  hextPatcher does on the PC: applied before it, FFNx followed a patched call (0x52F327) while
  finding its addresses and crashed. "Force high res models" has one ModFolder with
  ActiveWhen="1", which Junction VIII evaluates as false (IsConfigActive wants an operator),
  so the mod's root serves: Wrap.Run maps the root when no folder is *active*, and the
  overlay had asked whether any was *declared*. 7th Heaven's FF7 catalogs (left by early
  builds) are dropped from the subscriptions.
- STOP: ICALL to unlifted VA 0x00523030 (the Lunatic Pandora field): the field script's
  opcode table (0xB8DE4C, 394 handlers) had two handlers the catalog never made functions:
  data-scan false starts a few bytes before them (0x523012, 0x522000) decoded across them.
  tools/table_seeds.py seeds every uncatalogued entry of a .rdata/.data run of code pointers
  that is 80% catalogued (7: the two field opcodes, 5 in other tables); pcrecomp patch 0026
  keeps seeds over a straddling decode. Not tools/ffnx_seeds.py: it makes FFNx's
  mid-function sites entries (0x46545F, 0x4657DD, inside 0x4653B0/0x465720, which FFNx
  patches into a tail jump and which run interpreted); split there, the intro crashed in
  FFNx's replacement. Catalog: py -I tools/table_seeds.py game/ff8_en.exe <a catalog built
  with no seeds>; py -I tools/run_lift.py catalog game/ff8_en.exe analysis/seeds.json
  analysis/catalog.json (gzip to config/). 57,645 functions.
- The bottom screen in FF8 battles: battle slots (0x1D27B10, 0xD0 each: 3 party, 5 enemies),
  current/max HP and ATB as u32 at +0x10..+0x1C, element defense u16[8] at +0x44, status at
  +0x08/+0x80, level +0xBC; the enemy name is the first dword of the info at +0x00 (FFNx
  voice.cpp). The play clock is the savemap's game_time (the header's is the last save's).
- Bottom screen portraits: Portraits.java cuts the faces from the player's files: the 2013
  release's Data/lang-en/menu.fs (menu/hires/Face_b00/b01.TEX, LZSS, 256 x 256 16-bit), the
  Remastered's main.zzz (textures/hires/face_b00/b01.tex.png, its HD sheets). Both sheets are
  4 x 2 faces of 64 x 96 (b00: Squall, Zell, Irvine, Quistis / Laguna, Kiros, Ward; b01:
  Rinoa, Selphie, Seifer, Edea). The bottom screen is drawn in the launcher's orange.
- Frame rate: the game counts frames and FFNx's FF8 limiter scales nothing. ff8_fps_limiter 2
  ("30 FPS battles", the default until 0.1.2) runs battles at twice their speed and 3 (60 FPS) at
  four times. The default is 1 (battles 15, the rest 30); smooth motion comes from frame generation
  (src/ffnx_port/framegen.cpp).
- The 2013 edition after all of the above: SkipIntro, Force high res models (d000/d009/d024/
  d027 from the mod), a battle, the bottom screen. A folder import of the 2013 release deletes
  the Remastered's copies (main.zzz, other.zzz, FFVIII_EN.exe.dat; 2.8 GB).
- Known: in the Remastered, Down does not move the cursor in the Continue block list (it does
  on the title menu, and in the 2013 edition); the game sees it (engine mapped 0x4000).
- Steam on the Thor: QR sign-in, ownership (app 39150), Repair from Steam (every file checked),
  and a full download (347 files, 1.66 GB in 309 s; FF8_EN.exe 03230c11..., the wrapped one).
  The test API's REPORT/SCREEN/STEAM_DOWNLOAD/STEAM_CLOUD actions were never in the manifest.
- Place names: menu/areames.dc1 (u16 count, u16 offsets, FF8 text) by the savemap header's
  location_id, read from menu.fs (or the Remastered's main.zzz). Text code 0x0E nn is a region
  name: 0x20-0x27 Galbadia, Esthar, Balamb, Dollet, Timber, Trabia, Centra, Horizon (Deling),
  0x2D Deling City and 0x3E Garden as the save list shows them; the rest are left out.
- Not a bug: Down in the Continue block list wraps inside its page of three (Left/Right turn
  pages); three presses come back to the same block.
- Remastered: HD portraits from main.zzz, the opening movie (New Game), a GF summon in battle.
- Steam, both editions on the account: the launcher asks which to play (a first download),
  Game files can switch to the other edition from Steam, Repair keeps the installed one. The
  account check found both (39150, 1026680). SteamLink.open() threw for an account without the
  2013 release (ownedApp); it takes the Remastered now. 2013 -> Remastered from Steam: 3 files,
  2.8 GB (depot 1152240 AccessDenied, not one of them).
- Languages: the Remastered's main.zzz has data/lang-{en,fr,de,es,it,jp}; FFNx's Remastered
  mode runs the English program in any of them (game_language). The launcher offers English,
  French, German, Spanish, Italian with the Remastered (not Japanese: its own program and
  fonts); the 2013 release stays English (one exe per language).
- Place-name prefixes: text code 0x0E nn is entry nn - 0x20 of main's namedic.bin (32 names:
  Galbadia ... Fishermans Horizon, East Academy, Desert Prison, Trabia Garden, Lunar Base,
  Shumi Village, Deling City, Balamb Garden, ..., Ultimecia Castle, Garden, Deling), read from
  the player's files, in the game's language.
- Steam account, read only (as asked): the 2013 cloud has no FF8 files for this account; the
  stats read at sign-in works (schema 28973 bytes, 0 stats). Cloud saves and achievements stay
  off in 0.1.
- The Remastered from Steam, tested end to end: a 503 from a busy content server stopped it
  (a chunk had 6 tries over ~10 s; now 10 with pauses up to 30 s, ~3 min), and after 2.8 GB
  RemasterExe ran out of the launcher's heap (growing streams and copies, several times the
  37 MB program): it writes one array of the exact size now, and the app has a large heap.
- French on the Remastered (game_language 2): the game's menus and text, and the bottom
  screen's place names and spell names (Brasier, Foudre ...: the kernel's), no fault.

## 2026-11-22 Steam achievements; 60 FPS
- FFNx's FF8 achievement code (SteamAchievementsFF8: 45 achievements, the 2013 release's
  counters nmy_kill / mag_draw / mag_stck / won_card) installs its hooks only with
  enable_steam_achievements on, and both manifests were recorded with it off.
- Fixed in 0.2.0 (the sixth copy of the "breaks apart" effect, tools/ffnx_edit_ff8.py; the overwrite
  was reproduced at the same place by forcing that effect): battles sometimes stopped on "ICALL to unlifted VA 0x008D0017 from func 0x00508420"
  (0x508420 walks the battle task list and calls each task's [esi+8]). The STOP report now dumps
  guest memory at the registers: the list head [ebp] points at 0x02643E30, and what is there is
  not a task but model-like data (FD15FFE2, FD2AFFEF: pairs of small negative shorts), so
  something wrote over the list or the head points into the wrong buffer. Measured with
  scripted battles (world map, block 27, 30 FPS battles): achievements on with FFNx's
  achievement hooks 5 of 10, with every hook group skipped (temporary switch) 2 of 21; with
  "Force high res models" off still 1 of 6. Not the in-game Steam sync (it is gone, below,
  and the stops stayed). Achievements off: 0 of 3 so far, too few to call.
- The game's process makes no Steam connection now: unlocks wait in pending.txt (the companion
  shows them at once) and the launcher sends them when the game has closed, and at Play.
- The Remastered manifest is recorded again with achievements on (files/ffnx_debug.toml
  enable_steam_achievements = true, tools/ff8_manifest.sh remaster): 353 hooks, 26 of them the
  achievement hooks (Triple Triad, GF unlock, SeeD rank, Gil, level, kills, junk shop, Chocobo
  World, magazines, Obel Lake...), 339 code writes, 0 lift failures. The two programs share the
  hooked code, and the lift takes every manifest, so it serves the 2013 release too.
- manage_time_engine (FFNx finds it at WinMain+0x23) is not hooked on the Remastered: Dotemu's
  patch at 0x5699B7 puts NOPs over that call, so FFNx resolves junk (0x90E72A62, installed 0).
  The 2013 manifest keeps the hook at 0x569971. Expected, not a fault.
- Each edition is its own Steam app with its own list: 39150 (UNLOCK_GF_QUEZACOTL, ...) and
  1026680 (NEW_ACHIEVEMENT_1_x). The sync now uses the edition installed (SteamLink
  installedApp), not the first the account owns, and the game only uses a cached state of that
  app. Stats: steam_android.cpp keeps them (state.txt "stat", pending.txt "stat\t<name>\t<value>"),
  SteamLink sends the larger of the game's and Steam's value with the unlocks; the in-game sync
  only connects for unlocks (stats go with the next unlock or Play).
- files/steam/dry-run: the sync logs what it would send and sends nothing (testing with saves
  that are not the player's: the 100% test set would unlock achievements nobody earned).
- 60 FPS (ff8_fps_limiter 3, FFNx's frame limiter at 60 in fields, the world map, battles and
  swirls; no interpolation): measured on the Thor with SurfaceFlinger, 59.6-59.7 fps in the
  field and in battle. Walking covers the same ground in the same time as at 30 (the field
  moves by elapsed time), and a GF summon plays.
- Files named after FF7 (kept from the FF7Recomp fork) are renamed: src/{host,hle,android}/ff7_*
  -> ff8_*, cmake/ff7core.cmake -> ff8core.cmake, the run log ff7.log -> ff8.log (and
  ff8.prev.log), the game library libff7.so -> libff8.so (GameBuild renames a 0.1.0 build once,
  so no rebuild), libff7launcher.so -> libff8launcher.so, the ff7_core / ff7_rcheevos archives.
  Function and macro names (ff7_*, FF7_*) are unchanged; FFNx's own ff7/ code is FFNx's.

## 2026-11-24 Mod textures of different sizes on one page (#8; 0.2.2)
- octom6 (Remastered, 0.2.1): Lunar Cry Plus closed at the start of a battle, Horizon Pack Plus in Dollet.
  Both faults at the same address, the load in ModdedTexture::drawImage's unequal-scale branch:
  scaleRatio = targetScale / sourceScale was cut to a whole number, so a 7x texture on an 8x page
  read rows past its image (FFNx's own code; fixed in tools/ffnx_edit_ff8.py).
- Test: tests/texture_scale/run.sh (every pair of scales 1..16; 86 pairs fail with the old code).
  On the Thor: solid PNGs game/mods/Textures/world/esk/chara_one/model0-0 (20x64) at 7x and
  model0-1 (44x64) at 8x, _00.png, crash 0.2.1 on loading a world map save with the reporter's
  backtrace; 0.2.2 loads it. The sizes come from FFNx's "must be scaled" warnings with 1x1 PNGs.

## 2026-11-24 A mod's field backgrounds (#13; 0.2.3)
- AxlRose's WIP (field/mapdata/<field>/<field>.dds, BC7): its menu textures loaded, none of its 303
  backgrounds. FFNx looks for that layout only when dirExists("<mods/Textures>/field/mapdata/<field>"),
  else it falls back to Tonberry's names; the overlay resolves files, so the folder was missing.
  Both editions. ff7_mods_has_dir (src/mods) answers for folders; asked only there (ffnx_edit_ff8.py).
- Not in ffnx_stat: FFNx then finds a mod's hext/ folder and lists it with std::filesystem, which
  throws (the game stopped at boot).
- Test: a folder mod with the mod's mod.xml and a magenta BC7 bvtr_3.dds (2752x1024, block 02 Train),
  Remastered: 0.2.2 logs "Directory does not exist, fallback" and draws the game's own, 0.2.3 draws it.
