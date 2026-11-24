#!/usr/bin/env bash
# usage: bash tests/texture_scale/run.sh [mod.cpp]   (default: the prepared FFNx tree's)
set -euo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SRC="${1:-$HERE/../../external/FFNx/src/ff8/mod.cpp}"
OUT="$HERE/../../build/texture_scale"; mkdir -p "$OUT"
tr -d '\r' < "$SRC" | awk '/^void ModdedTexture::drawImage\(/{p=1} p{print} p&&/^}/{exit}' \
  | sed 's/const uint32_t \*source/Src source/' > "$OUT/drawimage.inc"
"${CXX:-clang++}" -std=c++20 -I "$OUT" "$HERE/test_draw_image.cpp" -o "$OUT/test.exe"
"$OUT/test.exe"
