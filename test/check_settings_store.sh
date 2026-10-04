#!/usr/bin/env bash
# Gate: the persisted settings store (LoadSettings / SaveSettings) keeps its one-time defaults
# migration and its defrev marker, and a tuned value survives repeated load/save cycles.
#
# Compiles _diag/settings_store_probe.cpp, which includes the REAL src/smooth_wheel_scroll.cpp and
# runs the shipped LoadSettings/SaveSettings against an in-memory ExtState. The panel is compiled out
# (-DSWS_NO_SETTINGS_UI): the store code is shared and does not depend on the panel.
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]:-$0}")/.." && pwd)"
SDK="$ROOT/third_party/reaper-sdk-git/sdk"
SWELL="$ROOT/third_party/reaper-sdk-git/WDL/swell"
MODSTUB="$SWELL/swell-modstub-generic.cpp"
OUT="$ROOT/build/_settings_store_probe.exe"

g++ -std=c++17 -O2 -DSWELL_PROVIDED_BY_APP -D_FILE_OFFSET_BITS=64 -DSWS_NO_SETTINGS_UI \
  -Wno-unused-function -Wno-multichar -I"$SDK" -I"$SWELL" -I"$ROOT/build" \
  "$ROOT/_diag/settings_store_probe.cpp" "$MODSTUB" -o "$OUT"
rc=0
"$OUT" || rc=$?
rm -f "$OUT"
exit $rc
