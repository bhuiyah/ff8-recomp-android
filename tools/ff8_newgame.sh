#!/usr/bin/env bash
# From a fresh start (tools/play_ff8.sh): press confirm until the title menu, pick NEW GAME.
# usage: bash tools/ff8_newgame.sh
source "$(dirname "${BASH_SOURCE[0]}")/env.sh"; export MSYS_NO_PATHCONV=1
LOG=/sdcard/Android/data/com.ff8recomp/files/game/FFNx.log
P(){ "$ADB" shell am broadcast -a com.ff8recomp.test.PAD -p com.ff8recomp -f 0x20 --ei buttons $1 --ei ms ${2:-150} > /dev/null; sleep 0.7; }
for i in $(seq 1 60); do
  "$ADB" shell "grep -q MODE_MAIN_MENU $LOG" && break
  P 4096; sleep 2
done
sleep 6; P 1; sleep 1; P 4096   # up to NEW GAME (the menu fades in first), confirm (XInput B)
echo "new game started"
