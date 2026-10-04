#!/usr/bin/env bash
# Gate: the Linux/macOS panel detects the theme optionally through REAPER 7.81's IsDarkMode, falls back
# to the config flag, and repaints only when its palette signature changes.
#
#   1. Static: IsDarkMode is looked up through GetFunc and never declared through the SDK header;
#      the lookup is compiled out of the Windows build and of --no-settings-ui.
#   2. Runtime probe (_diag/theme_watch_probe.cpp, includes the real source): the API is preferred when
#      present, the config flag is the fallback, a colour change with the same dark flag changes the
#      signature, a dark-flag change changes it, an unchanged signature requests no refresh.
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]:-$0}")/.." && pwd)"
CPP="$ROOT/src/smooth_wheel_scroll.cpp"
SDK="$ROOT/third_party/reaper-sdk-git/sdk"
SWELL="$ROOT/third_party/reaper-sdk-git/WDL/swell"
rc=0
fail() { echo "FAIL: $*" >&2; rc=1; }

# 1. static
grep -q 'rec->GetFunc("IsDarkMode")' "$CPP" || fail "IsDarkMode is not looked up through GetFunc"
if grep -q 'REAPERAPI_WANT_IsDarkMode' "$CPP"; then fail "IsDarkMode must not be requested through the SDK header"; fi
grep -q 'typedef bool (\*IsDarkModeFn)();' "$CPP" || fail "IsDarkMode function-pointer type missing"
grep -B2 'GetFunc("IsDarkMode")' "$CPP" | grep -q '#if !defined(_WIN32) && !defined(SWS_NO_SETTINGS_UI)' || fail "IsDarkMode lookup is not guarded for the Linux settings build"

# 2. runtime probe
g++ -std=c++17 -O2 -DSWELL_PROVIDED_BY_APP -D_FILE_OFFSET_BITS=64 -w \
  -I"$SDK" -I"$SWELL" -I"$ROOT/build" \
  "$ROOT/_diag/theme_watch_probe.cpp" "$SWELL/swell-modstub-generic.cpp" -o "$ROOT/build/_theme_watch_probe.exe" \
  || fail "probe did not compile"
if [[ -x "$ROOT/build/_theme_watch_probe.exe" ]]; then
  "$ROOT/build/_theme_watch_probe.exe" || fail "probe checks failed"
  rm -f "$ROOT/build/_theme_watch_probe.exe"
fi
exit $rc
