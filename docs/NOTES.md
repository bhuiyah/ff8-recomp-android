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
