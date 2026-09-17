#!/usr/bin/env bash
# Build SmoothWheelScroll (REAPER extension) for Linux x86_64.
#
# v1 scope (see PORTING.md): action-based glide only via REAPER's own
# hookcommand2 -- no WH_GETMESSAGE-equivalent wheel hook. Settings window: as
# of the settings-ui-port branch, Phase 2a (an empty, themed, open/closable
# panel) is in; the fader/knob controls and the motion chart are not yet.
# Compiles the settings UI in by default now, matching build.sh's own
# --no-settings-ui opt-OUT convention on Windows (previously always excluded
# here via a hardcoded -DSWS_NO_SETTINGS_UI with no way to turn it back on).
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]:-$0}")" && pwd)"
SDK="$ROOT/third_party/reaper-sdk-git/sdk"
SWELL="$ROOT/third_party/reaper-sdk-git/WDL/swell"
SRC="$ROOT/src/smooth_wheel_scroll.cpp"
RC="$ROOT/src/settings_panel_linux.rc"
MODSTUB="$SWELL/swell-modstub-generic.cpp"
OUT="$ROOT/build"
SO="$OUT/reaper_smoothwheelscroll-x86_64.so"

mkdir -p "$OUT"

DEFS=()
for arg in "$@"; do
  case "$arg" in
    # Compile-time switch for the diagnostic logging (writes to $TMPDIR or /tmp).
    --debug-log) DEFS+=(-DSWS_DEBUG_LOG) ;;
    # Opt out of the settings window, matching build.sh's Windows flag of the same name.
    --no-settings-ui) DEFS+=(-DSWS_NO_SETTINGS_UI) ;;
    *) echo "unknown build option: $arg" >&2; exit 2 ;;
  esac
done

# Settings window: generate the SWELL dialog resource from settings_panel_linux.rc (skip
# entirely for a --no-settings-ui build, same as the Windows tuning-ui resource would be).
# swell_resgen.pl reads and writes relative to its own working directory, so run it from $OUT
# (matching the SWS CMake build's own "copy .rc there, resgen there" pattern) and add $OUT to
# the include path so smooth_wheel_scroll.cpp's #include of the generated .rc_mac_dlg finds it.
if [[ " ${DEFS[*]} " != *" -DSWS_NO_SETTINGS_UI "* ]]; then
  cp "$RC" "$ROOT/src/settings_panel_linux_resource.h" "$OUT/"
  echo "== generating settings panel dialog resource =="
  perl "$SWELL/swell_resgen.pl" "$OUT/settings_panel_linux.rc"
fi

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
  -I"$SDK" -I"$SWELL" -I"$OUT" \
  "$SRC" "$MODSTUB" \
  -o "$SO"

echo "== built =="
ls -l "$SO"
echo "== exported entry points =="
nm -D "$SO" 2>/dev/null | grep -E "ReaperPluginEntry|SWELL_dllMain" || { echo "MISSING EXPORT"; exit 1; }
