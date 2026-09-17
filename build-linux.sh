#!/usr/bin/env bash
# Build SmoothWheelScroll (REAPER extension) for Linux x86_64.
#
# v1 scope (see PORTING.md): action-based glide only via REAPER's own
# hookcommand2 -- no WH_GETMESSAGE-equivalent wheel hook, no settings window.
# Always compiles with -DSWS_NO_SETTINGS_UI: there is no Linux/macOS port of
# the GDI settings window yet, so building it in would be dead code.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]:-$0}")" && pwd)"
SDK="$ROOT/third_party/reaper-sdk-git/sdk"
SWELL="$ROOT/third_party/reaper-sdk-git/WDL/swell"
SRC="$ROOT/src/smooth_wheel_scroll.cpp"
MODSTUB="$SWELL/swell-modstub-generic.cpp"
OUT="$ROOT/build"
SO="$OUT/reaper_smoothwheelscroll-x86_64.so"

mkdir -p "$OUT"

DEFS=(-DSWS_NO_SETTINGS_UI)
for arg in "$@"; do
  case "$arg" in
    # Compile-time switch for the diagnostic logging (writes to $TMPDIR or /tmp).
    --debug-log) DEFS+=(-DSWS_DEBUG_LOG) ;;
    *) echo "unknown build option: $arg" >&2; exit 2 ;;
  esac
done

echo "== g++ =="
g++ --version | head -1
echo "== compiling =="
# -DSWELL_PROVIDED_BY_APP (not SWELL_LOAD_SWELL_DYLIB): swell-modstub-generic.cpp
# only declares the SWELL function pointers and exports SWELL_dllMain(); REAPER
# itself calls that at load time with its own SWELL resolver, so every SWELL
# call in this plugin resolves against REAPER's already-loaded libSwell.so --
# nothing here implements or links a separate SWELL. See third_party/WDL/VENDORED.md.
g++ -std=c++17 -O2 -shared -fPIC -fvisibility=hidden \
  -DSWELL_PROVIDED_BY_APP -D_FILE_OFFSET_BITS=64 \
  -Wall -Wno-unused-function -Wno-multichar \
  "${DEFS[@]}" \
  -I"$SDK" -I"$SWELL" \
  "$SRC" "$MODSTUB" \
  -o "$SO"

echo "== built =="
ls -l "$SO"
echo "== exported entry points =="
nm -D "$SO" 2>/dev/null | grep -E "ReaperPluginEntry|SWELL_dllMain" || { echo "MISSING EXPORT"; exit 1; }
