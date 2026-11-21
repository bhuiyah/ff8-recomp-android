<p align="center"><img src="docs/images/icon.svg" width="144" alt="FF8Recomp's icon: a white feather drifting across a large moon on a dusky orange sky"></p>

# FF8Recomp

FINAL FANTASY VIII (PC, the 2013 Steam release) statically recompiled to native ARM64 Android,
rendered by [FFNx](https://github.com/julianxhokaxhiu/FFNx) on Vulkan (or OpenGL ES). Built for
the AYN Thor, and meant to run on other Android handhelds and phones. A sibling of
[FF7Recomp](https://github.com/bhuiyah/ff7-recomp-android) and FF9Recomp.

> **This repository and the APK contain no game code or data.** You need your own copy of FINAL
> FANTASY VIII from Steam. The app recompiles **your** `FF8_EN.exe` on your device, once; nothing
> derived from the game is ever published here.
> Not affiliated with or endorsed by Square Enix. FINAL FANTASY is a trademark of Square Enix.

**Status: 0.1, the first public build.** The game plays from the title screen through fields, the
world map, menus, battles and movies, with its music. Some of FF7Recomp's launcher features are
not there for FF8 yet (see [Known limitations](#known-limitations)).

## Made with AI

**This project was built with AI, start to finish.** As with FF7Recomp, I directed an AI coding
agent (Anthropic's Claude, in Claude Code) to build it: the recompilation pipeline, the Android
port of FFNx for FF8, the DirectMusic player, the launcher and these docs were written by the AI.
My part was deciding what to build, testing every step on the Thor, and saying what worked and
what did not. Commits are credited to Claude for that reason.

- **It has not been reviewed line by line by an experienced human developer.** It works on my
  Thor, but expect rough edges and bugs.
- **The hard parts were built by people.** FFNx, pcrecomp and nfsu2-recompiled, bgfx and the
  other projects listed under [Credits](#credits-and-licenses) are human work this project stands
  on. Please support them.
- **I am making it public to improve it,** and so people can play it. Bug reports, testing on
  other devices, reviews and fixes are all welcome.

If AI-made software is not something you want to run or contribute to, that is a fair choice,
and I would rather you know before you install it.

## What you get

- The game running natively (no emulation, no Wine/FEX), at its original frame rate or FFNx's
  30 FPS battles / 60 FPS (experimental) modes.
- Its music: Android has no DirectMusic, so FF8Recomp carries a small DirectMusic player that
  plays the game's own segments with its own instrument set (`FF8.dls`), through FFNx's audio
  engine (crossfades, battle music, resume after battles).
- Movies (FFmpeg), 16:9 widescreen, controller support, keyboard support, on-screen touch
  controls for devices without a controller.
- Save states (Select+L1 saves, Select+R1 loads), on top of the game's own saves.
- A launcher: import or download your game files, build the game for your device, settings,
  save states, problem reports.

## Playing

1. Download the APK from [Releases](../../releases/latest) and install it (allow installing from
   your browser or file manager when Android asks).
2. Open FF8Recomp → **Game files**:
   - **Download from Steam…**: sign in once by scanning a QR code with the Steam app on your phone
     (you never type your Steam password into FF8Recomp); the launcher downloads the game files
     your account owns (about 1.6 GB) straight from Steam and checks each one.
   - or **Find my FF8 folder…**: pick the game's install folder if you already have it on the
     device (`steamapps/common/FINAL FANTASY VIII`, the one with `FF8_EN.exe` and `Data`).
3. The launcher builds the game for your device: it translates your `FF8_EN.exe` to C, compiles
   it with the clang it carries and links it with the engine (FFNx, the renderer, the runtime).
   This happens once and carries on in the background (a notification shows the progress).
4. Press **Play**.

FINAL FANTASY VIII as Steam installs it (the 2013 release, English, unpatched) has everything
FF8Recomp uses. Its SteamStub DRM wrapper is removed on the device when the files are imported.
Patched game files are not supported.

**Your PC saves:** copy `slot1_save01.ff8`… from `Documents\Square Enix\FINAL FANTASY VIII
Steam\user_<id>` into `Android/data/com.ff8recomp/files/game/save/user_0/`; they appear under
**Continue**.

## Building the APK

The APK is built on **Windows** (Git Bash) and needs no game files.

### 1. Install the tools

| Tool | Notes |
|---|---|
| [Git for Windows](https://git-scm.com/download/win) | provides Git Bash |
| [Python 3](https://www.python.org/downloads/) | |
| [Android Studio](https://developer.android.com/studio) or the command-line SDK | SDK Manager → SDK Tools: **NDK (Side by side) 28.2.13676358**, **CMake 3.30.5**, Android SDK Platform-Tools |
| JDK 17 | Android Studio's bundled JBR works, or [Microsoft OpenJDK 17](https://learn.microsoft.com/java/openjdk/download) |
| [LLVM for Windows](https://github.com/llvm/llvm-project/releases) | clang for build-time host tools |
| [Visual Studio 2022 Build Tools](https://visualstudio.microsoft.com/downloads/) | "Desktop development with C++" (bgfx's shader compiler, LLVM's tablegen) |

`tools/env.sh` finds these in their standard install locations. If yours are elsewhere, set
`ANDROID_SDK_ROOT`, `JAVA_HOME`, `LLVM_DIR` or `MSBUILD`.

### 2. Build

In Git Bash:

```bash
git clone <this repository> FF8Recomp
cd FF8Recomp
bash tools/build_all.sh
```

`build_all.sh` runs every step in order: the pinned toolkits (pcrecomp, FFNx, bgfx) and their
patches, the libraries, FFmpeg, shaders, the on-device build kit (clang and lld built for Android,
about 40 minutes the first time; Python for Android; the lifter; the engine's prebuilt archives)
and the APK. If a step fails, fix the cause and run it again; finished steps are skipped.

`config/catalog.json.gz` (the function boundaries of the game's exe: addresses only) and
`config/ffnx_manifests/` (FFNx's patch sites) come from the game's exe and are made on a PC that
has it (`tools/import_game.py`, `tools/run_lift.py catalog`, `tools/ff8_manifest.sh`).
`docs/NOTES.md` is the engineering log; `docs/ROADMAP.md` the plan.

## Reporting problems

Please open an issue with your device, Android version and what happened. **Report a problem** in
the launcher collects the logs (`ff7.log`, `build.log`, `game/FFNx.log` under
`Android/data/com.ff8recomp/files/`).

## Known limitations

- Only the 2013 Steam release of FINAL FANTASY VIII (`FF8_EN.exe`, English) is supported. The
  Remastered edition is planned (it runs the same game code inside its own engine) but not done.
- Not yet for FF8: mods and mod catalogs (Junction VIII), Steam achievements and cloud saves,
  importing PlayStation saves, the second-screen panels on dual-screen devices, game boosters.
- The music player covers what FF8's segments use (sequence, tempo and band tracks, DLS level 1
  instruments); DirectMusic's reverb and chorus are not reproduced.
- Movies decode on the CPU.

## Credits and licenses

FF8Recomp is GPL-3.0 (it links FFNx). It builds on:
[pcrecomp](https://github.com/sp00nznet/pcrecomp) (MIT),
[nfsu2-recompiled](https://github.com/codepdbh/nfsu2-recompiled) (MIT; Android host),
[FFNx](https://github.com/julianxhokaxhiu/FFNx) (GPL-3.0),
[bgfx/bx/bimg](https://github.com/bkaradzic/bgfx) (BSD-2),
[SoLoud](https://github.com/jarikomppa/soloud) (zlib) and [vgmstream](https://github.com/vgmstream/vgmstream) (ISC),
[rcheevos](https://github.com/RetroAchievements/rcheevos) (MIT),
the [LZMA SDK](https://www.7-zip.org/sdk.html) (public domain),
[FFmpeg](https://ffmpeg.org/) (LGPL).
The APK also carries, for the on-device build: [LLVM/clang and lld](https://llvm.org/) (Apache-2.0
with LLVM exceptions), [Python for Android](https://www.python.org/downloads/android/) (PSF),
[Capstone](https://www.capstone-engine.org/) (BSD-3) and [pefile](https://github.com/erocarrera/pefile) (MIT).
For Steam downloads: [JavaSteam](https://github.com/Longi94/JavaSteam) (MIT) with its libraries
(protobuf, BSD-3; OkHttp, Ktor, Kotlin and kotlinx.coroutines, Apache-2.0; Commons Lang,
Apache-2.0; [Bouncy Castle](https://www.bouncycastle.org/), MIT;
[XZ for Java](https://tukaani.org/xz/java.html), public domain;
[zstd-jni](https://github.com/luben/zstd-jni), BSD-2) and
[ZXing](https://github.com/zxing/zxing) (Apache-2.0) for the QR code.
