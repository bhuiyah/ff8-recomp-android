# FF8Recomp roadmap

FF8Recomp runs Final Fantasy VIII on Android as native ARM64 code, recompiled on the player's
device from their own copy. It is a fork of FF7Recomp (v1.1.37) and supports **both** Steam
editions:

| | 2013 release | Remastered (2019) |
|---|---|---|
| Steam app | 39150 | 1026680 |
| Game code | `FF8_EN.exe` (32-bit x86, SteamStub DRM wrapper) | the same program, stored encrypted in `FFVIII_EN.exe.dat` / `FFVIII_JP.exe.dat` |
| Layer around it | FFNx (ported, as in FF7Recomp) | Square's own `FFVIII_EFIGS.dll` / `FFVIII_JP.dll` (32-bit x86, OpenGL, DirectSound, DirectInput 8, XInput, no DRM wrapper), **lifted too** |
| Assets | `Data\` (`.fs/.fl/.fi`, `audio.dat`, `dmusic`, movies) | `main.zzz`, `other.zzz` |
| Mods | Junction VIII catalogs (IRO, `mod.xml`) | none planned at first |

## What we know (2026-11-21, from the user's Steam copies)

- `FF8_EN.exe` (buildid 296320): timestamp 955151253 (April 2000), SizeOfCode `0x768000`. Its
  entry point is in a `.bind` section (SteamStub), so `.text` must be unwrapped before lifting.
  SHA-1 `03230c11328f8a1f9635435096e1659d9bfd002d`.
- `FFVIII_EN.exe.dat` (buildid 8131267) starts with a bare COFF header with the **same**
  timestamp and SizeOfCode. The rest is encrypted or compressed. So both editions run the same
  original game code; they differ in the layer around it.
- `FFVIII.exe` is a small SteamStub-wrapped stub. `FFVIII_LAUNCHER.exe` and the SharpDX DLLs are
  the .NET settings launcher, not the game.

Not checked yet: whether the decrypted embedded program is byte-identical to the unwrapped 2013
`.text` (Dotemu may have patched either), and which OpenGL version and entry points the Remastered
DLL uses.

## How "edition" runs through the app

Edition is first-class from the start, not bolted on later:

- **Game files:** the importer recognises either folder; Download from Steam offers either app id
  the account owns. A player may have both installed side by side.
- **Build:** `GameBuild` lifts the edition's code (2013: the unwrapped exe; Remastered: the
  decrypted embedded exe plus the EFIGS DLL) into its own `libff8_<edition>.so`.
- **Per edition:** saves, save states, Steam Cloud folder, achievement list, settings that only
  one edition has (FFNx options vs. the Remastered's own options and boosters).
- **Launcher:** picks the edition on Game files; the info bar shows which one is active.

## Milestones

2013 first, because FFNx and FF7Recomp's pipeline already fit it. The Remastered follows as its
own track.

| # | Milestone | Exit test |
|---|---|---|
| M0 | Inputs: unwrap SteamStub from `FF8_EN.exe` (our own unwrapper, so it can run on the device); `check_inputs.py` and the function catalog for FF8; FFNx's FF8 half compiled for ARM64 instead of stubbed | Lifter difftest passes on FF8's code |
| M1 | 2013 boots on the Thor (headless, then in the APK) | Title screen drawn by FFNx |
| M2 | 2013 playable | New Game → Balamb Garden → Fire Cavern battle, no divergence from PC exe + FFNx |
| M3 | Launcher and on-device build, edition-aware; the APK carries no game code | Fresh install → Download from Steam → build → Play |
| M4 | Mods: Junction VIII catalogs, IRO overlay | A texture mod and a music mod work in game |
| M5 | Polish: frame rates, widescreen, save states, release signing, updater, Report a problem | Each setting on the Thor; a release installs over the last |
| M6 | Steam: download, Cloud saves, achievements (per edition) | One achievement unlocks; a save crosses to the PC and back |
| M7 | Second screen: Junction, GF, Magic, Cards, Map tabs; deck; layouts | Every tab live in field, world map and battle |
| M8 | Everything else in the handoffs | Parity checklist |
| R0 | Remastered inputs: find how `FFVIII_EFIGS.dll` decrypts and loads `FFVIII_EN.exe.dat`; reproduce it in a tool; compare with the 2013 `.text` | Decrypted image matches the DLL's in-memory copy on the PC |
| R1 | Lift `FFVIII_EFIGS.dll` and the embedded program into one guest; Win32 HLE for its imports; survey its OpenGL use | Headless boot reaches the DLL's main loop |
| R2 | Renderer: its OpenGL calls onto GLES 3.2 / Vulkan; `.zzz` archive reader; DirectSound and XInput onto ours | Remastered title screen on the Thor |
| R3 | Remastered playable | Same route as M2, compared against the PC Remastered |
| R4 | Remastered in the app: its saves, its achievements, its boosters on the deck and second screen | Parity with the 2013 edition's app features |

Ground truth is always the original PC program running on Windows (with FFNx for 2013), never an
emulator.
