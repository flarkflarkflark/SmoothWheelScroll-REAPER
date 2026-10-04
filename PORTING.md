# Porting to Linux / macOS

Status: Linux v1 (action-based glide, no settings UI) builds and is under
live testing, rebased onto upstream 1.7.2. Settings window Phase 2a (empty,
themed, open/closable panel) is live-tested on the `settings-ui-port` branch
-- see below. macOS not started (no Windows-hook-equivalent code exists yet
on either platform -- see below).

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
2. **The GDI settings window.** Partially ported (`settings-ui-port` branch,
   Phase 2a): an empty, themed, open/closable panel exists via a SWELL dialog
   resource -- see "Settings window: Phase 2a" below. The fader/knob controls
   and the motion chart are not ported yet; `--no-settings-ui` still builds
   the fully headless variant (now opt-out, matching `build.sh`'s own flag of
   the same name, rather than the old hardcoded always-on exclusion).
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
./build-linux.sh                    # release, settings window included
./build-linux.sh --debug-log        # writes $TMPDIR/SmoothWheelScroll.log (or /tmp)
./build-linux.sh --no-settings-ui   # headless, matching build.sh's own flag
```

Produces `build/reaper_smoothwheelscroll-x86_64.so`. Building the settings
window in requires Perl (`swell_resgen.pl`, vendored -- see VENDORED.md);
`--no-settings-ui` skips that step entirely, same headless variant as before
Phase 2a.

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

## Stock XWayland: manual smoke test -- PASS

Full manual pass of the live-test checklist above, done by hand (not
automated), against:

- **Exact commit:** `7e5c5dbbed56a22d460aa9bf885f314245e516b0` (`7e5c5db`),
  clean working tree (`git status` empty at test time).
- **Artifact:** `build/reaper_smoothwheelscroll-x86_64.so`, built with
  `./build-linux.sh --debug-log`.
  SHA-256: `97bd8d323922363f773d33e5d9651c9136bfd105805f43895d2cb905667f9873`
- **Host:** REAPER 7.78, stock `libSwell.so`, running through XWayland
  (`GDK_BACKEND=x11`) under a Plasma Wayland session -- the
  `~/reaper_native_wayland_test/stock/REAPER` portable install (see the
  native-Wayland section above for what this rig is).

Results, all PASS:

| # | Item | Result |
|---|---|---|
| 1 | Arrange scroll/zoom, vertical + horizontal | PASS |
| 2 | Repeated fast/slow notched-wheel movement, direction changes | PASS |
| 3 | MIDI editor scroll/zoom | PASS |
| 4 | "One page" actions stay native (one notch = one page, tested via Alt+Shift+Mousewheel on "View: Scroll view vertically one page (MIDI CC relative/mousewheel)") | PASS |
| 5 | Parameter wheels (faders/knobs) unaffected | PASS |
| 6 | Keyboard/script(Run)-triggered actions -- do they unexpectedly glide? | PASS as *documented option-C behavior*: running "View: Zoom horizontally (MIDI CC relative/mousewheel)" from the Action List glides exactly like a wheel notch, since Linux has no wheel-vs-other-trigger admission gate (see "The v1 decision on the latch" above). Confirmed expected, not a new defect. |
| 7 | No continued movement or log activity after input stops | PASS -- checked directly against `/tmp/SmoothWheelScroll.log`: last glide-related line tapered to a sub-unit value and stopped; over a minute of subsequent silence (one unrelated "View: Toggle mixer visible" pass-through line aside, correctly logged as un-touched) |
| 8 | TCP panel / MIDI piano keys / mixer surfaces | PASS as *expected v1 limitation*: all three respond instantly/natively, not smoothed -- these surfaces were driven by the Windows-only `WH_GETMESSAGE` hook (see "What 'Windows-only surroundings' means" above), which has no Linux port. Confirmed as expected, not treated as a bug to fix immediately. |

**Explicit v1 limitations reconfirmed by this pass** (not defects, both already
documented above): (a) option-C admission -- keyboard- or Run-triggered
invocations of the classified actions glide the same as a wheel notch would,
because Linux has no equivalent of the Windows wheel-latch gate; (b) TCP
panel, mixer, and MIDI piano-key surfaces are not ported and behave exactly
as native REAPER, unsmoothed.

**Native Wayland**: still unsupported pending further diagnosis (see "Known
issue" above) -- unaffected by this pass, which was run on stock/XWayland
only.

**Cleanup confirmed** after this test: REAPER process stopped and verified
gone (`pgrep -x reaper` empty), the test artifact removed from
`stock/REAPER/UserPlugins`, and all three environments checked clean
afterward -- `stock/REAPER/UserPlugins`, `native/REAPER/UserPlugins`
(untouched throughout), and the regular `~/.config/REAPER/UserPlugins`
(untouched throughout).

## Rebased onto upstream 1.7.0 (model/panel split)

The fork's `main` was 12 commits behind `bobo198504/SmoothWheelScroll-REAPER`
at the time of the v1 port and the smoke test above (both anchored to
1.3.9). Rebasing onto 1.7.0 pulled in a source split (the single file's
model now lives behind `src/model.h`, `src/device.h`, `src/routing.h`,
`src/anim161_core.h`, `src/anim3_core.h`) and a settings-panel rework. None
of the new headers touch Windows or REAPER -- pure math/data, same as
`anim_core.h` before it -- so the split needed no platform guards of its
own.

Two more Windows-only surfaces existed in 1.7.0 that didn't exist at v1
port time, and got the same treatment as the TCP panel body / MIDI piano
keys above (scoped out, not ported):

- **Mixer/MCP wheel forwarding** (`ApplyMcpWheel`): sends a synthetic
  `WM_MOUSEWHEEL` to the mixer window, reached only through a `Route` the
  message hook itself builds -- so unreachable on Linux/macOS regardless of
  whether it's guarded. Stubbed to a no-op rather than left to fail to
  compile on `WHEEL_DELTA`, which SWELL does not define.
- **Touchpad device classification** (`LastWheelDevice` and its callers
  `LastWheelWasTouchpad` / `LastWheelPassedThrough` / `TouchpadZoomReverse`):
  depends on state (`g_lastDevTick`/`g_lastDevKind`) the message hook alone
  sets. Stubbed to the same "no recent wheel" fallback the Windows code
  already used for "no record", so every existing caller needed no change.

`kFastTimerMs` was also retuned upstream (4ms -> 1ms) between 1.3.9 and
1.7.0; that constant stays Windows-only (the multimedia timer it configures
has no Linux/macOS equivalent -- see "The animation clock" above), so the
retune carries through unchanged in spirit.

Checked with `unifdef -D_WIN32` against `upstream/main`: preprocessed for
Windows, `src/smooth_wheel_scroll.cpp` differs from upstream only in
comments, one harmless declaration reorder, and `__declspec(dllexport)` ->
the SDK's own `REAPER_PLUGIN_DLL_EXPORT` macro (identical expansion on
Windows) -- no Windows behavior change. `build-linux.sh` builds clean.

**Re-run against 1.7.0**: PASS, full checklist. Done via MCP first, then
the two rows the MCP tooling couldn't reach (MIDI editor scroll/zoom,
vertical arrange zoom) were confirmed by hand with real hardware wheel
input directly in REAPER. See the dedicated section below for the detail
and for what was organic vs. synthesized vs. manual.

## 1.7.0 rebase: live-test results (via MCP, 2026-09-17)

Driven through REAPER's own `flark-reaper-mcp` bridge (`mcp_bridge.lua`,
already running against the user's normal REAPER instance) rather than by
hand. Artifact: `build/reaper_smoothwheelscroll-x86_64.so`, built with
`./build-linux.sh --debug-log`.
SHA-256: `98185d3bc2d2740c7b8f7ba066c04f37a2571674a378fa0932050a132c45d213`

**Setup**: installed to `UserPlugins`, REAPER's real running instance
(stock `GDK_BACKEND=x11` through XWayland, confirmed from its own
environment -- not the experimental native-Wayland rig from the section
above) was quit (project was not dirty -- confirmed via
`dsl_is_project_dirty` first, nothing lost) and relaunched with the same
captured environment. Bridge was back and answering read-only requests in
0.5s. Plugin loaded clean: `loaded main=0x...`, correct MMPROBE mouse-
modifier readout, no errors in REAPER's stdout/stderr.

**1. Arrange scroll/zoom**: PASS, and organically -- not synthesized. ~26s
after load, a real wheel gesture (cmd=990, "View: Zoom horizontally") came
through the log with a proper relative-encoded value (val=113,
relmode=1), producing a clean `HOOK -> MATCH -> kick -> smooth` sequence
and a burst of `replay` calls at decaying intervals (11ms widening to
35ms) -- exactly the expected glide taper. Log went silent afterward (70+s
checked); no continued activity. This is the single strongest piece of
evidence in this pass: a real user wheel notch exercised the full
OnAction -> Kick -> Tick -> ReplayAction path on the rebased build with no
part of that chain flagged by the compiler-verified `#ifdef _WIN32` split.

**Direct-invocation sanity checks** (`dsl_run_action` with `unsafe: true`,
Main section): commands 988 (scroll-h) and 989 (scroll-v) both reached
`hookcommand2` and classified correctly. Their `val=0, relmode=0` (this
tool has no way to pass a real relative-wheel encoding) correctly
short-circuits the decode (`raw = val & 0x7f == 0`) with no crash --
which incidentally exercises `TouchpadZoomReverse` /
`LastWheelPassedThrough` (both stubbed for this rebase, item 5 in the
original checklist) on every single one of these calls, always resolving
cleanly to "not a touchpad / nothing passed through". Across the whole
session (organic event + every direct invocation) that stub path ran
several times with zero incidents.

**2. Arrange zoom**: PASS. Horizontal confirmed organically (990, above).
Vertical (command 1000) could not be triggered through `dsl_run_action`
(see the MCP-dispatch gap noted below) but was confirmed PASS afterward
by hand, with real hardware wheel input directly in REAPER.

**3. MIDI editor scroll/zoom**: PASS. The MIDI editor itself opened
correctly via MCP (via a bound user script, since no direct "open MIDI
editor" tool call was available) -- confirmed by a real, distinct editor
`hwnd` in the log and correct classification of the script's own internal
actions (1227, "View: Zoom to project loop selection") as pass-through.
`dsl_run_action` with `section: 32060` for the actual scroll/zoom commands
(40430-40433) never produced a `HOOK` line at all, before or after
closing an unrelated stray dialog (see below) -- and command 1000 (Main
section, "View: Zoom vertically") showed the exact same symptom, while
every other Main-section ID tried did reach the hook. Since this affects a
native Main-section command untouched by the rebase (same `routing.h`
entry, same registration code, unmodified since before this port
existed), this was assessed as a gap in how the MCP tool dispatches those
specific command IDs, not a plugin defect. **Confirmed correct** by manual
follow-up: both MIDI-editor scroll/zoom and vertical arrange-zoom were
verified directly in REAPER with real hardware wheel input (not via MCP),
PASS.

**4. Mixer/MCP wheel scroll**: unreachable by construction, as designed --
`DRIVE_MCP_WHEEL` is only ever set inside the Windows-only message hook
(see the rebase section above), so there is no runtime path on Linux that
reaches `ApplyMcpWheel` regardless of input. Confirmed by code inspection
and the clean compile, not by a live trigger (there is nothing to
trigger). No crash risk since the stub is never called.

**5. Touchpad-style input**: see the direct-invocation checks above --
`LastWheelDevice`'s stub ran on every `OnAction` call in this session
(organic and direct), always returning the safe "no recent wheel"
default. No dedicated touchpad hardware/synthetic-input path was
available to test the reverse-zoom feature's actual UX, but the fallback
it depends on is proven inert and crash-free.

**6. Debug log, full session**: no warnings, no unexpected branches, no
duplicate/storm HOOK activity, nothing resembling the native-Wayland flood
in the section above. The only anomalies were the two tool-dispatch gaps
noted in item 3, and one unrelated operator error (below).

**Operator error, not a plugin issue**: action 40009 was guessed to be
"open in built-in MIDI editor" and instead opened a Media Item Properties
dialog, which then sat open. `xdotool`-driven synthetic clicks/keys
(mousemove+click, explicit button down/up, `--window`-targeted click,
Alt+F4) all reached the window (confirmed active/focused) but produced no
effect -- REAPER's SWELL/XWayland input handling did not act on any of
them. A real keypress (the user pressing Escape) closed it immediately.
Worth knowing for future MCP-driven test sessions: this bridge can drive
REAPER's own action list reliably, but cannot currently dismiss native
modal dialogs -- avoid actions that open one, and if one appears, ask a
human to close it rather than spending time on synthetic input.

**Cleanup**: a disposable second project tab (`SmoothWheelTest`, one MIDI
track/item, never saved to disk) was created for MIDI-editor testing and
is still open, harmless, closable anytime. The user's original project
tab was confirmed untouched throughout (checked track/item counts on both
tabs; not dirty before the test, not modified by anything done here).

**Manual follow-up (2026-09-17, real hardware wheel, direct in REAPER, not
via MCP)**: MIDI-editor scroll/zoom and vertical arrange-zoom -- the two
rows the MCP tooling could not reach -- PASS. That closes out every row of
this checklist against 1.7.0: the rebase is verified, not just compiled.
Native mixer/TCP-panel fallback behavior (item 8 of the original
1.3.9-era smoke test below) was not re-touched here since nothing in the
rebase changed that surface's already-documented "not ported, behaves
natively" status.

## Settings window: Phase 2a (branch `settings-ui-port`, 2026-09-17)

Dialog architecture only -- no fader/knob controls, no motion chart. Goal was
an empty, correctly-sized, correctly-themed panel opening and closing on
Linux via both entry points, and that's what this phase delivers.

**Why a dialog resource at all**: SWELL has no `CreateWindowEx`/`RegisterClass`
-- confirmed against real WDL source, not assumed. The only way to get a
window with child controls is a dialog resource, generated from a Win32 `.rc`
file by `swell_resgen.pl` into a `.rc_mac_dlg` that's `#include`d and expands
through macros in `swell-dlggen.h` (both vendored -- see VENDORED.md) into a
real dialog template at compile time. This is the same mechanism REAPER
itself and SWS use.

**Decision: Windows untouched, Linux gets its own path.** Rather than move
Windows onto the same dialog-resource mechanism (one code path, but touching
a working, extensively-tuned feature purely for consistency), the existing
`#ifdef _WIN32` half of the settings code -- everything from `CreatePanelChildren`
through the accelerator table -- was left completely alone, now nested one
level deeper inside a `#ifdef _WIN32`/`#else` split of its own. Verified with
`unifdef -D_WIN32` the same way the earlier rebase was: preprocessed for
Windows, `src/smooth_wheel_scroll.cpp` on `settings-ui-port` is byte-for-byte
identical to `linux-port` -- zero Windows behavior change.

**Decision: no scrolling.** The eventual full layout (`kDesignClientW/H` =
360x527) comfortably fits any normal screen at 100% scale, and SWELL has no
`GetScrollInfo`/`SetScrollInfo`/`ShowScrollBar` equivalent at all (checked
against real WDL source). Building a fixed-size, non-scrolling panel avoids
that gap entirely rather than working around it.

**What's in the new `#else` branch**: a minimal `Theme` (background + text
only -- the Windows version's 10-colour theme with card/line/grid brushes
stays Windows-only until the fader/knob/chart phase needs it on Linux too),
`ReadAppDarkFlag`/`ThemeColorRgb`/`ResolveTheme` (already-portable REAPER SDK
calls, just the one `GetPrivateProfileIntA` -> `GetPrivateProfileInt` rename),
`ApplyTitleBar`/`ApplyScrollbarTheme` as intentional no-ops (Linux window
decorations are the desktop's own, outside the app's control -- same pattern
as `StopTimer`/`StartTimer`'s Linux bodies elsewhere in this file), a minimal
dialog proc (`WM_CTLCOLORDLG`/`WM_CTLCOLORSTATIC` for theming,
`WM_CLOSE`/`WM_DESTROY` for lifecycle), `ShowConfigWindow`/`ToggleConfigWindow`,
a pass-through `PanelKeyHandler` (no child controls yet to steal keys from),
and `OnMenuHook`, ported for real (see "Extensions menu fix" below -- the
first draft of this phase shipped a no-op here on a wrong assumption about
what `AddExtensionsMainMenu()` alone does).

**Bug found and fixed during testing, not just claimed to work**: the .rc's
360x527 are old-style dialog units, and SWELL's non-macOS backend applies a
fixed 1.9x dialog-unit-to-pixel factor with no compile-time override (the
`SWELL_DEF_DLGSCALE2` macro in `swell-dlggen.h` is an unconditional `#define`
outside `SWELL_TARGET_OSX`, and the alternate scale-aware macro path
references `SWELL_DLG_WS_DEFAULT_SCALING`, which is itself only defined
`#ifdef SWELL_TARGET_OSX` -- i.e. genuinely unusable on Linux, not a mistake
on this project's part). On this machine that compounded with the desktop's
own font/DPI scale into a panel measured at ~2.85x the requested size (1026x1501
instead of 360x527). Fixed by measuring the border SWELL actually added
(window rect minus client rect) immediately after `CreateDialogParam` and
resizing to that plus the real target client size -- the same "measure, do
not guess" approach `FitWindowToContent` already takes on Windows, aimed at
the border instead of the content. Verified via screenshot: exact 360x527
after the fix, both on first open and after 5 rapid open/close cycles.

**Live-tested via MCP** (`flark-reaper-mcp`, real REAPER instance, not just
compiled): built with `--debug-log`, installed, REAPER restarted clean, no
crash on load. Both entry points wired and the action confirmed working --
`_SWS_SCROLL_TUNE` resolved via named-command lookup and run directly (the
Extensions-menu entry is the same unconditional, unmodified
`AddExtensionsMainMenu()`/`custom_action` registration already covered by the
scoping report as portable; not separately click-tested through the actual
menu UI, since this session's earlier attempts at synthetic mouse/keyboard
input into a REAPER-owned window were unreliable under this XWayland
setup -- worth one manual check).

Results: panel opens at the exact designed size, dark REAPER theme renders
correctly (dark background, light text -- confirmed by screenshot; light
theme not separately re-tested this session), closes cleanly via the same
toggle action, and survived 5 rapid open/close cycles plus the underlying
REAPER instance staying alive and responsive throughout. One unrelated
`terminate called without an active exception` was observed in a *prior*
REAPER session's stderr, immediately following a DrivenByMoss/JVM
initialization sequence and with no SmoothWheelScroll log activity anywhere
near it in time -- noted for completeness, not attributed to this plugin.

**Not yet done** (later phases, tracked here rather than silently skipped):
fader/knob custom controls (need SWELL's dialog child-control mechanism plus
the `GWL_WNDPROC`-based subclassing pattern SWS itself uses, since SWELL has
no `CreateWindowEx` for a custom control class either), `DrawMonitorBlock`
(needs `TextOut` -> `DrawText`+RECT, `GetTextExtentPoint32A` -> `DT_CALCRECT`,
`FrameRect` -> 4x`FillRect`, and `PS_DOT` -> manually-drawn dash segments,
per the scoping report), and docking support.

### Extensions menu fix (same day)

Reported after the phase above shipped: the action worked, but no
"SmoothScroll..." entry appeared in REAPER's Extensions menu at all.

**Root cause**: wrong assumption in the first draft. `AddExtensionsMainMenu()`
only reserves this extension a slot in the menu system; actually *populating*
that slot is entirely `OnMenuHook`'s job -- REAPER calls it back (via the
`hookcustommenu` registration) with the real `HMENU` each time a
customizable menu is about to be shown, and it's expected to `InsertMenuItem`
into it right there. The Phase 2a `OnMenuHook` was a no-op, so nothing ever
got inserted -- confirmed by adding temporary trace logging (removed again
once confirmed) rather than guessing: `hookcustommenu` itself fired
correctly for other menus at load (`"Main toolbar"`, `"Empty TCP area
toolbar"`, `flag=0`), and `AddExtensionsMainMenu` resolved to a real,
non-null function pointer -- both processes worked; only the body that was
supposed to use them didn't exist yet.

**Fix**: ported the real `OnMenuHook` from the Windows side. Needed only the
same category of change as everything else in this phase --
`InsertMenuItemA` -> `InsertMenuItem`, `MENUITEMINFOA` -> `MENUITEMINFO`
(SWELL has no ANSI/wide split) -- plus `MIIM_STRING`, which SWELL doesn't
define at all; defined locally as `0` since SWELL's `InsertMenuItem` treats
`dwTypeData` as the label unconditionally, no flag bit needed. `StrHasI`
(case-insensitive substring match, used to recognise the "Main extensions"
menu id) turned out to already be portable in `routing.h` -- the scoping
report's "missing" line for it was wrong, an artifact of grepping the
settings-UI block in isolation rather than the whole file.

**Verified live, not just compiled**: rebuilt, reinstalled, REAPER
restarted clean. Debug log showed `hookcustommenu` firing for "Main
extensions" with `flag=1` each time the user opened it by hand (menu clicks
couldn't be driven synthetically here either, same as the dialog earlier in
this phase), and the user confirmed "SmoothScroll..." now appears and opens
the panel. Temporary trace logging removed afterward; `unifdef -D_WIN32`
re-confirmed the Windows path is still byte-for-byte identical to
`linux-port`.

## Rebased onto upstream 1.7.1 / 1.7.2 (macros, mixer guard)

Two upstream feature releases since the 1.7.0 rebase, pulled in together
(merge-base `4c374f6`, theirs `f6a7411` = v1.7.2):

- **1.7.1 -- `Custom:` macros**: a wheel gesture on a REAPER custom macro
  (an action whose name starts with `Custom:`) now plays as one animated
  gesture instead of a burst of raw wheel events replayed per child action.
  The implementation is `src/macro.h`, which is pure REAPER-free string/data
  code like `routing.h` and `device.h` before it -- no platform guards
  needed, it compiles as-is on Linux. The macro chain runs through the same
  `hookcommand2` + replay path the port already had, so the feature is live
  on Linux unchanged. Upstream's `DumpActionNames` debug helper came along
  with it; it lives under `SWS_DEBUG_LOG`, which the port already has.
- **1.7.2 -- mixer faders/knobs keep their own wheel**: upstream added a
  guard so that wheel input over mixer *faders/knobs* (as opposed to the
  mixer background) is no longer captured for smoothing and is left to
  REAPER natively. That guard lives in the Win32 message-hook code
  (`GetMsgProc`), which does not compile on Linux -- and the whole MCP
  surface it guards was never ported in the first place (see the 1.7.0
  rebase section: mixer wheel forwarding is stubbed and unreachable). So
  the change is present in the merged source but functionally Windows-only;
  nothing on the Linux side changes behavior.

Deliberately skipped, same policy as earlier passes: `src/wheel_log.h`,
`test/check_wheel_log.sh`, `_diag/wheel_log_probe.cpp`, and the build.sh
`--wheel-log` option (upstream DEV-only raw-wheel logging, not part of the
shipped plugin), plus documentation-only commits and the `versions/2.0`
snapshot.

One port-only adjustment was needed: `test/check_macro.sh`'s compile line
gained `-D_snprintf=snprintf` because the probes include `src/macro.h`
standalone (the main build already gets the same shim from
`platform_compat.h`). `src/macro.h` and the probes themselves stay
byte-identical to upstream.

Verified: `unifdef -D_WIN32 src/smooth_wheel_scroll.cpp` against upstream
v1.7.2 differs only in the same named categories as before (skipped
wheel-log code, port comments, `REAPER_PLUGIN_DLL_EXPORT`, declaration
order) -- no Windows behavior change. Both `./build-linux.sh` and
`./build-linux.sh --no-settings-ui` build clean, and all eight test gates
pass, including the new `test/check_macro.sh`. Our build.sh linker line
(`-luser32 -lgdi32 -lole32 -lwinmm -lcomctl32`) is unchanged; upstream's
`--wheel-log` build option was not adopted.

## macOS

Not started. Once Linux is verified, the plan is: same source (the
`#ifdef _WIN32` / `#else` split already covers both non-Windows platforms
identically -- SWELL's own `__APPLE__` detection picks the Cocoa backend),
a `build-macos.sh` using clang + the same vendored SWELL headers, built and
tested on real hardware (arm64) since none of this was buildable or
runnable here.
