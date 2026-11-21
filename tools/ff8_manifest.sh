#!/usr/bin/env bash
# Record FFNx's FF8 code-patch manifest on the Thor and re-lift with it (FFNx installs its
# patches when the game window is created, so ~35 s of running is enough).
#   1. env.txt: FF7_WATCHDOG (the watchdog thread writes the manifest) + FF7_FFNX_MANIFEST
#   2. run the installed APK, pull the manifest to config/ffnx_manifests/ffnx_manifest_ff8_l1.json
#   3. re-lift generated/recomp with it (only changed files are rewritten), retn table
# Then rebuild the APK. Needs FF8Recomp's Thor lock (tools/play_ff8.sh checks it).
# usage: bash tools/ff8_manifest.sh
set -uo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
source "$ROOT/tools/env.sh"
export MSYS_NO_PATHCONV=1
F=/sdcard/Android/data/com.ff8recomp/files
OUT="$ROOT/config/ffnx_manifests/ffnx_manifest_ff8_l1.json"
"$ADB" shell "printf 'FF7_WATCHDOG=10\nFF7_FFNX_MANIFEST=$F/ffnx_manifest.json\n' > $F/env.txt; rm -f $F/ffnx_manifest.json"
bash "$ROOT/tools/play_ff8.sh" 35 --no-install > /dev/null 2>&1 || { echo "play failed (Thor lock?)"; exit 1; }
"$ADB" logcat -d -s FF7Recomp:* | grep "ffnx-patch" | tail -1
"$ADB" shell am force-stop com.ff8recomp
"$ADB" shell rm -f "$F/env.txt"
mkdir -p "$(dirname "$OUT")"
"$ADB" pull "$F/ffnx_manifest.json" "$(cygpath -w "$OUT")" > /dev/null || { echo "no manifest written"; exit 1; }
echo "manifest: $(grep -c '"offset"' "$OUT") code writes"
(cd "$ROOT" && py tools/run_lift.py lift game/ff8_en.exe analysis/catalog.json generated/recomp --manifests "$(cygpath -m "$OUT")" \
   | grep -E 'FFNx manifests|\[!\]|lift_failures')
(cd "$ROOT" && py tools/gen_retn_table.py | tail -1)
