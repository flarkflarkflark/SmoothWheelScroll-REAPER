#!/usr/bin/env bash
# Gate: the Linux/macOS settings panel (Phase 2b) is self-drawn, bound to the right globals, and
# --no-settings-ui still removes all of it.
#
#   1. The dialog has no child controls: the panel is painted by smooth_wheel_scroll.cpp, not by the .rc.
#   2. The four Linux rows use the same globals, ranges and defaults as the Windows table they mirror,
#      and the same hues and end captions.
#   3. Both builds are made. The normal build contains the panel's captions and the settings action;
#      the --no-settings-ui build contains none of them.
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]:-$0}")/.." && pwd)"
RC="$ROOT/src/settings_panel_linux.rc"
HDR="$ROOT/src/settings_panel_linux_resource.h"
CPP="$ROOT/src/smooth_wheel_scroll.cpp"
SO="$ROOT/build/reaper_smoothwheelscroll-x86_64.so"
rc=0
fail() { echo "FAIL: $*" >&2; rc=1; }

# 1. no child controls in the dialog; the header defines only the dialog id
if grep -E -q '^\s*(CHECKBOX|LTEXT|RTEXT|CTEXT|PUSHBUTTON|DEFPUSHBUTTON|CONTROL|EDITTEXT|COMBOBOX|GROUPBOX)\b' "$RC"; then
  fail "the .rc declares child controls; the Linux panel must be self-drawn"
fi
grep -q 'IDD_SWS_SETTINGS_LINUX' "$HDR" || fail "IDD_SWS_SETTINGS_LINUX missing from header"
if grep -q 'IDC_SWS_' "$HDR"; then fail "stale control ids left in the header"; fi

# 2. the Linux rows mirror the Windows table: same global, range, default, hue, captions
grep -q '{"Glide length", &g_windowMs, kWindowMinMs, kWindowMaxMs, kDefaultWindowMs, "ms", "Snappy", "Gentle", RGB(120, 190, 255)}' "$CPP" || fail "Glide length row differs from its Windows row"
grep -q '{"Slow step", &g_startDeltas, kStartMin, kStartMax, kDefaultStart, "d", "Fine", "Coarse", RGB(255, 190, 120)}' "$CPP" || fail "Slow step row differs from its Windows row"
grep -q '{"Ramp-up", &g_budgetDeltas, kBudgetMin, kBudgetMax, kDefaultBudget, "d", "Quick", "Long", RGB(180, 220, 140)}' "$CPP" || fail "Ramp-up row differs from its Windows row"
grep -q '{"Top speed", &g_speedMul, kSpeedMulMin, kSpeedMulMax, kDefaultSpeedMul, "x", "Native", "Double", RGB(200, 170, 255)}' "$CPP" || fail "Top speed row differs from its Windows row"
# the same rows exist on the Windows side, with the same colours
for h in 'RGB(120, 190, 255)' 'RGB(255, 190, 120)' 'RGB(180, 220, 140)' 'RGB(200, 170, 255)'; do
  n=$(grep -c -F "$h" "$CPP"); [[ $n -ge 2 ]] || fail "hue $h is not used on both the Windows and the Linux side"
done
for k in kWindowMinMs kWindowMaxMs kDefaultWindowMs kStartMin kStartMax kDefaultStart \
         kBudgetMin kBudgetMax kDefaultBudget kSpeedMulMin kSpeedMulMax kDefaultSpeedMul; do
  grep -q "static const double $k = " "$CPP" || fail "$k missing from smooth_wheel_scroll.cpp"
done
grep -q 'kLinuxPanelH = kLxPad + kLxHeadH \* 2 + kLxRowH \* kLinuxRows + kLxPad' "$CPP" || fail "panel height no longer derived from the layout"

# 3. builds: the normal one has the panel, the excluded one has none of it
"$ROOT/build-linux.sh" >/dev/null 2>&1 || fail "normal build failed"
[[ -f "$SO" ]] || fail "normal artifact missing"
for s in "Enable smooth scrolling" "Reverse touchpad horizontal zoom (n/a)" "SWS_SCROLL_TUNE" "IsDarkMode"; do
  grep -a -q -F "$s" "$SO" || fail "normal build lacks '$s'"
done
"$ROOT/build-linux.sh" --no-settings-ui >/dev/null 2>&1 || fail "--no-settings-ui build failed"
for s in "Enable smooth scrolling" "Reverse touchpad horizontal zoom" "SWS_SCROLL_TUNE" "Smooth Wheel Scroll settings" "Glide length" "IsDarkMode"; do
  if grep -a -q -F "$s" "$SO"; then fail "--no-settings-ui build still contains '$s'"; fi
done
# Leave build/ holding the normal artifact, as a plain build-linux.sh does.
"$ROOT/build-linux.sh" >/dev/null 2>&1 || fail "normal rebuild failed"

if [[ $rc -eq 0 ]]; then echo "OK: self-drawn settings panel, row bindings and --no-settings-ui exclusion"; fi
exit $rc
