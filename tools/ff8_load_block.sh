#!/usr/bin/env bash
# Restart FF8 on the Thor and load a save from the title's Continue menu, Slot 1. The saves are
# game/save/user_0/slotN_saveNN.ff8 on the device (FFNx's userdata path); test saves for this:
# game/test_saves/ (git-ignored), pushed with: adb push game/test_saves/. <files>/game/save/user_0/
# usage: bash tools/ff8_load_block.sh ["<pad presses on the block list>"]
#   the block list opens on the newest block; Right/Left (8/4) page by 3, Up/Down (1/2) move.
#   "" loads the block it opens on. Needs FF8Recomp's Thor lock.
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
source "$ROOT/tools/env.sh"; export MSYS_NO_PATHCONV=1
"$ADB" shell 'grep -q "^FF8Recomp" /data/local/tmp/thor.lock' || { echo "FF8Recomp does not hold the Thor lock"; exit 1; }
"$ADB" shell "echo \"FF8Recomp $(date -u +%Y-%m-%dT%H:%MZ)\" > /data/local/tmp/thor.lock"
"$ADB" shell "am force-stop com.ff8recomp; input keyevent KEYCODE_WAKEUP; logcat -c; am start -n com.ff8recomp/.LauncherActivity -f 0x24000000 --ez test_play true --es test_start off" > /dev/null
sleep 45                                                                    # boot to the intro
bash "$ROOT/tools/pad.sh" "4096 w5 4096 w3 4096 w3" 0.3 load1 > /dev/null   # intro, Continue, Slot 1
[ -n "${1:-}" ] && bash "$ROOT/tools/pad.sh" "$1" 0.4 load2 > /dev/null
bash "$ROOT/tools/pad.sh" "4096 w7" 0.3 load3                               # load; build/shots/load3.png
