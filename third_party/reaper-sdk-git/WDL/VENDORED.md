# Vendored: WDL/SWELL (headers + modstub only)

Lives at `third_party/reaper-sdk-git/WDL/` -- a sibling of `reaper-sdk-git/sdk/`
-- because that is the layout `reaper_plugin.h` itself expects and hardcodes
(`#include "../WDL/swell/swell.h"` on non-Windows; see
`third_party/reaper-sdk-git/README`, which documents the same "WDL next to
sdk/" layout for the upstream Cockos reaper-sdk repo this was vendored from).
Not a WDL-wide convention, just where this one header expects to find it.

Source: https://github.com/justinfrankel/WDL
Commit: 0ae28d7b6a6d839f922c1a199d353b392926dd5d
Commit date: 2026-09-13 08:29:31 -0400
Vendored: 2026-09-13
License: zlib-style (Cockos, Inc.), full text in each vendored file's header comment.

## What's here and why

4 files, copied verbatim from `WDL/swell/` at the commit above:

- `swell/swell.h`
- `swell/swell-types.h`
- `swell/swell-functions.h`
- `swell/swell-modstub-generic.cpp`

Nothing else from WDL is needed for those 4. The plugin is built with
`SWELL_PROVIDED_BY_APP` defined: `swell-modstub-generic.cpp` does not implement
SWELL itself, it resolves every SWELL function pointer from the *host* REAPER
process at load time (REAPER on Linux/macOS already links its own SWELL
implementation). This is the same pattern used by SWS and other cross-platform
REAPER extensions, and by the sibling project `LOKAAL/openreasmoothplayhead-linux`
in this environment (`third_party/WDL/swell/`), which this vendoring mirrors.

We do NOT compile a full SWELL implementation into this plugin, and we do NOT
vendor the rest of WDL (no jnetlib, no LICE, no jsfx machinery, etc.) -- none
of it is used.

## Added for the settings-ui-port (2026-09-17): dialog resource generation

2 more files, added when the Linux settings panel needed an actual window with
child controls -- SWELL has no `CreateWindowEx`/`RegisterClass` at all (see
PORTING.md's settings-ui-port scoping report); the only way to create one is a
dialog resource, generated from a Win32 `.rc` file the same way REAPER and SWS
themselves do it:

- `swell/swell_resgen.pl` -- the WDL-provided generator: reads a `.rc` file, writes
  a `.rc_mac_dlg` (and `.rc_mac_menu`) that `SWELL_DEFINE_DIALOG_RESOURCE_BEGIN2`
  and friends (in `swell-dlggen.h`) turn into a real dialog template at compile
  time. Perl, not the PHP twin SWS's CMake uses (`swell_resgen.php`, functionally
  identical) -- Perl is what's on this machine, PHP is not.
- `swell/swell-dlggen.h` -- the macros the generated `.rc_mac_dlg` expands
  through (`BEGIN`/`END`/`LTEXT`/`PUSHBUTTON`/etc., `SWELL_DialogRegHelper`).
  Header-only; the `SWELL_curmodule_dialogresource_head` variable it reads is
  already declared `extern` in `swell-types.h` and defined in
  `swell-modstub-generic.cpp` -- both already vendored above, nothing extra
  needed there.

Pinned separately since they were added later and are unrelated to the
original 4:

Source: https://github.com/justinfrankel/WDL
Commit: 8f4d783de745126ac8c201455dc30818c8613324
Commit date: 2026-09-14 17:29:45 -0400
Vendored: 2026-09-17

## Updating

To refresh at a newer WDL commit:

```sh
git clone https://github.com/justinfrankel/WDL.git /tmp/wdl-update
cd /tmp/wdl-update && git log -1 --format="%H|%ci"   # record the new commit/date below
cp WDL/swell/{swell.h,swell-types.h,swell-functions.h,swell-modstub-generic.cpp} \
   /path/to/this/repo/third_party/WDL/swell/
```

Then update the commit/date at the top of this file. Do not add other WDL files
without a concrete reason (recorded here).
