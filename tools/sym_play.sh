#!/usr/bin/env bash
# Symbolize the libff7.so frames of the last crash in build/play.log (tools/play_ff8.sh).
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
L=$(ls -t $(find "$ROOT/android/app/build/intermediates/cxx" -name libff7.so -path "*arm64*") | head -1)
AL="$LOCALAPPDATA/Android/Sdk/ndk/28.2.13676358/toolchains/llvm/prebuilt/windows-x86_64/bin/llvm-addr2line.exe"
[ -x "$AL" ] || AL=/c/Users/$USERNAME/AppData/Local/Android/Sdk/ndk/28.2.13676358/toolchains/llvm/prebuilt/windows-x86_64/bin/llvm-addr2line.exe
addrs=$(grep -o "at libff7.so+0x[0-9a-f]*" "$ROOT/build/play.log" | sed 's/at libff7.so+//' | head -${1:-16})
"$AL" -f -C -e "$L" $addrs | paste - - | sed -E 's#\t.*(/src/|/external/|/generated/)#\t\1#; s#\(anonymous namespace\)::##' | awk -F'\t' '{print $1"  @ "$2}' | grep -v "^std::\|^void guest::set_result\|^on_fault"
