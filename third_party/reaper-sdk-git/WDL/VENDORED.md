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

Only 4 files, copied verbatim from `WDL/swell/` at the commit above:

- `swell/swell.h`
- `swell/swell-types.h`
- `swell/swell-functions.h`
- `swell/swell-modstub-generic.cpp`

Nothing else from WDL is needed. The plugin is built with `SWELL_PROVIDED_BY_APP`
defined: `swell-modstub-generic.cpp` does not implement SWELL itself, it resolves
every SWELL function pointer from the *host* REAPER process at load time (REAPER
on Linux/macOS already links its own SWELL implementation). This is the same
pattern used by SWS and other cross-platform REAPER extensions, and by the
sibling project `LOKAAL/openreasmoothplayhead-linux` in this environment
(`third_party/WDL/swell/`), which this vendoring mirrors.

We do NOT compile a full SWELL implementation into this plugin, and we do NOT
vendor the rest of WDL (no jnetlib, no LICE, no jsfx machinery, etc.) -- none
of it is used.

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
