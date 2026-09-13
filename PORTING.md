# Porting to Linux / macOS

Status: Linux v1 builds and is under live testing. macOS not started (no
Windows-hook-equivalent code exists yet on either platform -- see below).

## Why this was tractable at all

The plugin's core mechanism -- REAPER's own `hookcommand2` callback classifying
a wheel-driven action and replaying it with an animated relative value -- is
pure REAPER SDK, not Win32. That path needed no rewrite, only its Windows-only
surroundings removed.

## What "Windows-only surroundings" means, concretely

1. **The animation clock.** Windows used its own `SetTimer`/`timeSetEvent`
   (winmm) plus a private message-only window to hop back onto the UI thread.
   Replaced on Linux/macOS by REAPER's own `plugin_register("timer", ...)`,
   which already exists in this file (it used to only heal a dropped message
   hook) and needs no window or OS timer of its own.
2. **The GDI settings window.** Not ported. Non-Windows builds always define
   `SWS_NO_SETTINGS_UI`; the plugin runs on compiled-in defaults there.
3. **The `WH_GETMESSAGE` wheel hook.** This is the one with no drop-in
   replacement, and it did more than "extra surfaces":
   - It drove two surfaces with no REAPER action at all (TCP panel body,
     MIDI piano keys) -- not ported.
   - It set a 250ms latch (`g_wheelTick`) that `OnAction`'s admission gate
     reads for every action in the static `kActions` table (all of arrange
     scroll/zoom and MIDI editor scroll/zoom -- none of them are flagged
     `relativeAction`, so on Windows *all* of them require a fresh latch).
     Its purpose: tell a real wheel notch apart from the same action fired
     by a keyboard shortcut or a script, so only the wheel gets smoothed.

## The v1 decision on the latch (documented, not an oversight)

There is no cross-platform, permission-free equivalent of `WH_GETMESSAGE`:
- macOS: `CGEventTapCreate` (Quartz) -- needs one-time Input Monitoring
  permission from the user.
- Linux: no OS-level equivalent; the closest is the X11 XRecord extension
  (works against REAPER even under Wayland, since REAPER-Linux is itself an
  X11 client via XWayland -- but not on a Wayland-only setup without
  XWayland).

Three options were on the table:
- **A**: drop the gate permanently, no revisit planned.
- **B**: build the OS-level tap now (XRecord / CGEventTap), matching Windows
  behavior exactly but reintroducing the macOS permission prompt this v1
  was meant to avoid, and adding real per-OS input-tap code before any of
  it had been tried.
- **C** (chosen): drop the gate for v1, ship, measure on real usage whether
  it's actually noticeable, decide on B later with data instead of guesses.

Effect of C: on Linux/macOS, `OnAction` admits every matched action
regardless of what triggered it. A keyboard shortcut or script bound to
e.g. "View: Scroll horizontally" is glide-smoothed the same as a wheel
notch would be. See the `#else` branch of the admission gate in `OnAction`
(`src/smooth_wheel_scroll.cpp`) for the exact code, and the live-test
checklist below for how to tell whether this matters in practice.

## SWELL vendoring

`third_party/reaper-sdk-git/WDL/` -- see `VENDORED.md` there for the pinned
commit, source URL, and license. Only 4 files (headers +
`swell-modstub-generic.cpp`), built with `-DSWELL_PROVIDED_BY_APP`: every
SWELL call resolves against REAPER's own already-loaded SWELL implementation
at runtime (via the exported `SWELL_dllMain`, which REAPER calls
automatically on load) -- this plugin does not implement or link a SWELL of
its own.

Lives next to `sdk/` (not under a generic `third_party/WDL/`) because
`reaper_plugin.h` itself hardcodes `#include "../WDL/swell/swell.h"` on
non-Windows -- that's the layout the upstream `reaper-sdk` README documents
too ("merge or symlink in WDL" as a sibling of `sdk/`).

## Building

```sh
./build-linux.sh              # release
./build-linux.sh --debug-log  # writes $TMPDIR/SmoothWheelScroll.log (or /tmp)
```

Produces `build/reaper_smoothwheelscroll-x86_64.so`. No settings-UI flag
exists for this build (always headless) -- there is nothing to toggle yet.

## Live-test checklist (Linux)

1. REAPER loads without error; the extension is registered (check the
   Extensions list, or `--debug-log` output).
2. Arrange view: wheel scroll (vertical + horizontal) and zoom are smoothed.
3. MIDI editor: scroll and zoom are smoothed; the piano-key area does
   **not** smooth (not ported -- expected, matches the "not yet ported"
   list above, not a bug).
4. The native "View: ... one page (...)" actions are untouched by the
   plugin (one notch = one page, no multiplication) -- this was the real
   bug fixed in 1.3.9 on Windows; confirms the classification table ported
   correctly.
5. Trigger one of the `kActions`-table actions (e.g. "View: Scroll
   horizontally") from the Actions list or a keyboard shortcut, not the
   wheel. Expected under option C: it gets glide-smoothed too. Note how
   noticeable/objectionable this actually is -- that's the data option B's
   decision should be based on.

## Known issue: sustained replay activity on experimental native-Wayland SWELL

**Status: experimental native Wayland is currently unsupported pending
further diagnosis -- this is not a permanent rejection, and no defect in
native SWELL itself has been proven (see Conclusion below). Stock
(X11/XWayland) REAPER has passed build, load, and idle-stability checks;
complete manual wheel-behavior parity testing on stock is still pending.**

### How it was found

Live-testing against two isolated portable REAPER 7.78 installs
(`~/reaper_native_wayland_test/{stock,native}/REAPER`, not part of this
repo): `stock` runs REAPER's normal `libSwell.so` through XWayland
(`GDK_BACKEND=x11`), `native` runs an experimental Wayland-native
`libSwell.so` (`GDK_BACKEND=wayland`, WDL pinned at
`cb8e42442fe6f1afcef2d73fce4bc461cfff0fb9`, see that rig's own README/REPORT.md).

The plugin loaded cleanly on both. On `native` only, after a variable delay
(anywhere from ~3s to ~30s after load), the arrange/MIDI view started
zooming and scrolling on its own, continuously, for anywhere from ~10
seconds to over a minute, with no input from the test session. `stock` did
not show this in any run, but those runs were passive observation (build,
load, idle stability), not a deliberate manual wheel-scroll test -- see
"What was and wasn't verified on stock" below.

### A/B test (uncommitted diagnostic build, since reverted -- see below)

Three build variants of `src/smooth_wheel_scroll.cpp` (`--debug-log` always
on), each run on `native` alone, with `date`-stamped launch/kill commands to
remove timing ambiguity:

1. **Plain build** (the committed v1 source, unmodified): `hookcommand2`
   classifies, `Kick()` animates, `SendRelative()` actually calls
   `KBD_OnMainActionEx`/`onAction`. This is what showed the flood above.
2. **Hook-only variant**: classify and log via `Log("MATCH ...")`, then
   `return false` immediately -- never call `Kick()`, never animate, never
   replay. Isolates whether *anything* keeps re-invoking these actions with
   no help from us at all.
3. **No-replay variant**: `Kick()`/`Tick()`/the glide run exactly as normal,
   but `SendRelative()` logs `REPLAY-NOOP` instead of calling
   `KBD_OnMainActionEx`/`onAction`. Isolates whether the actual REAPER call
   is the necessary trigger, independent of whether our own animation loop
   runs.

Both variants also logged, for the first 40 events of each kind then a
once-per-second aggregate: command, val/val2/relmode, hwnd, thread id
(`pthread_self()`), `g_replaying`, both axes' `glide.Active()`, and explicit
`REPLAY-BEGIN`/`REPLAY-END` markers bracketing the actual REAPER call.

Exact sequence (times are real wall-clock, checked with `date` before/after
each step, against `~/reaper_native_wayland_test/bin/reaper-native-wayland`):

| Variant | Launched | Killed | Observed window | HOOK/MATCH activity |
|---|---|---|---|---|
| Hook-only | 20:55:04 | 20:56:24 | ~65s (checked at +15s, +45s, +65s) | **none at all** -- log has only the two load lines |
| No-replay | 20:56:45 | 20:58:52 | ~48s+ (checked at +20s, +40s) before being stopped | **none at all** -- log has only the two load lines |
| Plain (earlier runs) | -- | -- | 10s to ~2min | sustained, tens of events/sec, spanning multiple unrelated commands (990 arrange zoom-h, 988 arrange scroll-h, 40431/40430 MIDI editor zoom/scroll) |

Reproduction commands (run from `~/reaper_native_wayland_test`, requires no
`reaper` process already running -- both launcher scripts refuse to start
otherwise):

```sh
cp <plugin>.so native/REAPER/UserPlugins/reaper_smoothwheelscroll_TESTPORT-x86_64.so
rm -f /tmp/SmoothWheelScroll.log   # or $TMPDIR/SmoothWheelScroll.log
date; ./bin/reaper-native-wayland >stdout.log 2>stderr.log & echo launched pid=$!
# wait, inspect /tmp/SmoothWheelScroll.log
date; kill -TERM <pid>; sleep 5; pgrep -x reaper || echo stopped
rm -f native/REAPER/UserPlugins/reaper_smoothwheelscroll_TESTPORT-x86_64.so
```

### Conclusion

Per the interpretation this test was designed around: **the flood disappears
when replay is disabled** (hook-only and no-replay both stayed completely
silent; only the variant that actually calls the REAPER action floods).

Be precise about what that does and does not establish. **Replay was shown to
be a necessary trigger in these tests -- it was not shown to be the proven
root cause.** "Necessary" means: with replay removed, the flood did not occur
in ~65s (hook-only) and ~48s+ (no-replay) of observation. It does not mean
the mechanism connecting "replay happens" to "the view floods" is understood
-- it is not. In particular, **no defect in native SWELL itself has been
proven**: the evidence is consistent with a native-SWELL bug, but equally
consistent with some other interaction this plugin's calling pattern
triggers (see the unexplained-command-switching note below), and no direct
inspection of native SWELL's own code or state was done.

What the two negative-result variants DO establish cleanly: this plugin's
own classification and animation logic, on their own, produce zero side
effects on this platform (hook-only: classifies and logs with zero side
effects; no-replay: animates a full glide end-to-end with zero side
effects). Whatever is happening only starts once `SendRelative()` actually
calls `KBD_OnMainActionEx`/`onAction`.

**What remains genuinely unexplained**, and should not be glossed over: in
the plain-build logs, every `RAWHOOK` reentry captured during a `REPLAY-
BEGIN`/`REPLAY-END` window was correctly flagged `g_replaying=1` on the same
thread id -- the reentrancy guard caught it synchronously every time, not
delayed past the flag being cleared, which was the original hypothesis for
*how* a feedback loop could slip past `g_replaying`. Yet the flood continued
regardless, and -- oddly for a simple self-reentry loop -- wandered between
different, unrelated commands (arrange zoom, arrange scroll, MIDI editor
zoom, MIDI editor scroll) rather than looping on the one action being
replayed. A guess, not a finding: replaying a view action at the pace this
plugin's precision design calls for (tens of small increments per second,
see the OUTPUT-DENSITY section in the source) changes the visible view fast
enough that this experimental Wayland input layer misreads the resulting
rapid redraws/pointer-relative changes as fresh scroll input of its own --
but proving that would need injecting a single controlled real scroll event
directly into a native-Wayland client, which `xdotool` (X11/XWayland only)
cannot do, and no Wayland-native equivalent (`ydotool`, `wtype`, ...) was
available in this environment to try instead.

### What was and wasn't verified on stock (X11/XWayland)

Stock has passed: clean compile, clean load (no stderr errors, correct
`settings loaded`/`loaded main=` log), and idle stability (REAPER's own
`timer` callback running continuously for ~25s with no crash and no
spurious activity).

Stock has NOT yet had: a deliberate, manual wheel-scroll/zoom test with real
input (no `xdotool`-equivalent was available for injecting a controlled
scroll event into a native-Wayland client, and manual testing of stock
itself with real input was not performed in this session either). So "stock
never showed the flood" is an absence-of-evidence observation from passive
runs, not a completed behavioral test. Manual wheel-behavior parity testing
on stock (does scroll/zoom actually feel smooth, do all the classified
actions animate as designed) is still pending.

### Recommendation

Treat experimental native Wayland as **unsupported pending further
diagnosis** -- not permanently rejected. Nothing here indicates a defect in
this plugin's own code (see Conclusion), and no defect in native SWELL
itself has been proven either; the honest state is "unexplained interaction,
not yet safe to use." Stock (X11/XWayland) REAPER, REAPER's own supported
Linux configuration today, has passed build/load/idle-stability checks and
should be the basis for continued Linux work, with manual wheel-behavior
testing on it still to be done before calling the Linux port verified.

### On the diagnostic instrumentation itself

The three-variant A/B harness above was built directly into
`src/smooth_wheel_scroll.cpp` and `build-linux.sh` (`SWS_DIAG_HOOKONLY`,
`SWS_DIAG_NOREPLAY`, plus the `RAWHOOK`/`REPLAY-BEGIN`/`REPLAY-END`/
`REENTRY-DURING-REPLAY` tracing and rate-limited counters) to get an answer
quickly. It has been **reverted** -- `src/smooth_wheel_scroll.cpp` and
`build-linux.sh` are back to exactly the committed `8a65d9e` state -- because
ad-hoc instrumentation wired into the production translation unit is not
something to carry forward once its one question is answered. If this needs
revisiting (e.g. to test the rapid-redraw hypothesis above, or a future
native-Wayland SWELL release), rebuild it as a **separate, focused test
harness** rather than re-embedding branches like these in
`smooth_wheel_scroll.cpp` itself.

## macOS

Not started. Once Linux is verified, the plan is: same source (the
`#ifdef _WIN32` / `#else` split already covers both non-Windows platforms
identically -- SWELL's own `__APPLE__` detection picks the Cocoa backend),
a `build-macos.sh` using clang + the same vendored SWELL headers, built and
tested on real hardware (arm64) since none of this was buildable or
runnable here.
