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
