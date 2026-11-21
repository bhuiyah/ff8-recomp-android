#!/usr/bin/env bash
# Install the release APK (unless --no-install), start the game the way Play does (test API),
# wait, then print the game's log lines and a screenshot of each display to build/shots/.
# Refreshes FF8Recomp's Thor lock (only if FF8Recomp holds it).
# usage: bash tools/play_ff8.sh [seconds=30] [--no-install]
set -uo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
source "$(dirname "${BASH_SOURCE[0]}")/env.sh"
export MSYS_NO_PATHCONV=1
WAIT="${1:-30}"
"$ADB" shell 'grep -q "^FF8Recomp" /data/local/tmp/thor.lock' || { echo "FF8Recomp does not hold the Thor lock"; exit 1; }
"$ADB" shell "echo \"FF8Recomp $(date -u +%Y-%m-%dT%H:%MZ)\" > /data/local/tmp/thor.lock"
if [ "${2:-}" != "--no-install" ]; then
  "$ADB" install -r "$(cygpath -w "$ROOT/android/app/build/outputs/apk/release/app-release.apk")" | tail -1
fi
"$ADB" shell am force-stop com.ff8recomp
"$ADB" shell input keyevent KEYCODE_WAKEUP
"$ADB" logcat -c
"$ADB" shell am start -n com.ff8recomp/.LauncherActivity -f 0x24000000 --ez test_play true > /dev/null
sleep "$WAIT"
mkdir -p "$ROOT/build/shots"
n=$(date +%H%M%S)
"$ADB" exec-out screencap -d 4630946441858561667 -p > "$ROOT/build/shots/top-$n.png" 2>/dev/null
"$ADB" logcat -d -s FF7Recomp:* AndroidRuntime:E | grep -v "timeGetTime" > "$ROOT/build/play.log"
echo "log: build/play.log ($(wc -l < "$ROOT/build/play.log") lines), screenshot: build/shots/top-$n.png"
grep -v "^\S* \S* *[0-9]* *[0-9]* I FF7Recomp:     at \|host pc\|host lr\|native backtrace" "$ROOT/build/play.log" | tail -40
