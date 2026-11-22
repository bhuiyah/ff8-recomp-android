#!/usr/bin/env bash
# Press pad buttons on the running game (test API) and take a screenshot.
# usage: bash tools/pad.sh "<bits>[:ms] <bits>[:ms] ..." [gap seconds] [shot name]
#   bits: XInput (up 1, down 2, left 4, right 8, start 16, select 32, L1 256, R1 512,
#         X 16384, Y 32768); "w5" waits 5 s. FF8 (FFNx, Steam layout): 4096 confirm, 8192 menu,
#         32768 cancel
source "$(dirname "${BASH_SOURCE[0]}")/env.sh"; export MSYS_NO_PATHCONV=1
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
GAP="${2:-0.8}"
for p in $1; do
  case "$p" in w*) sleep "${p#w}"; continue;; esac
  b="${p%%:*}"; ms=150; [ "$p" != "$b" ] && ms="${p#*:}"
  "$ADB" shell am broadcast -a com.ff8recomp.test.PAD -p com.ff8recomp -f 0x20 --ei buttons "$b" --ei ms "$ms" > /dev/null
  sleep "$GAP"
done
"$ADB" exec-out screencap -d 4630946441858561667 -p > "$ROOT/build/shots/${3:-pad}.png"
"$ADB" logcat -d -s FF7Recomp:* | grep -E "FAULT|FATAL" | tail -2
