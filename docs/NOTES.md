# FF8Recomp notes

A dated log of findings: cause, change, how it was tested. FF7Recomp's own log stays in its
repository.

## 2026-11-21 Start

- Forked FF7Recomp at `54897c5` (v1.1.37) with fresh history. Renamed the app to
  `com.ff8recomp` (package, JNI names, test intents, label, update repository, keystore path).
  Source file names (`ff7_*.c`, `libff7.so`) are unchanged for now. Not built yet.
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
