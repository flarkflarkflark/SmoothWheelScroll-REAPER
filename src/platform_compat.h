// platform_compat.h -- non-Windows compatibility shim.
//
// Included only when building smooth_wheel_scroll.cpp with !_WIN32 defined
// (see the top of that file). The Windows build is untouched and never sees
// this header.
//
// This plugin's core mechanism -- REAPER's own hookcommand2 callback
// classifying and replaying an action with an animated relative value -- is
// already pure REAPER SDK and platform-independent (see AGENTS.md). What
// this header exists for is the small residue of raw Win32 API the file
// still calls once the Windows-only mechanisms (the WH_GETMESSAGE wheel
// hook, the GDI settings window, the winmm multimedia timer) are compiled
// out on this platform via #ifdef _WIN32 / SWS_NO_SETTINGS_UI:
//
//   - Win32 types (HWND, DWORD, LONG, ...)                 -> SWELL headers
//   - GetTickCount(), Sleep()                               -> SWELL (emulated)
//   - InterlockedExchange / InterlockedCompareExchange      -> shimmed below
//     (these are compiler intrinsics on Windows, not part of the Win32 API
//     SWELL wraps, so SWELL does not provide them)
//   - _snprintf                                              -> shimmed below
//
// See third_party/WDL/VENDORED.md for where the vendored SWELL headers come
// from and why only those 4 files are needed (SWELL_PROVIDED_BY_APP: every
// SWELL function is resolved from the *host* REAPER process at load time,
// not implemented in this plugin).
#ifndef SMOOTHWHEELSCROLL_PLATFORM_COMPAT_H
#define SMOOTHWHEELSCROLL_PLATFORM_COMPAT_H

extern "C" {
#include "swell.h"
}

// LONG is `signed int` on every platform this plugin targets (see
// swell-types.h: only a non-64-bit Apple build would make it `signed long`,
// which is not a target here). The cast below relies on that to pick the
// correctly-sized __sync builtin -- do not change to `long` without
// re-checking swell-types.h's LONG typedef for the platform in question.
static inline LONG InterlockedExchange(volatile LONG *target, LONG value)
{
  return (LONG)__sync_lock_test_and_set((int *)target, (int)value);
}

static inline LONG InterlockedCompareExchange(volatile LONG *dest, LONG exchange, LONG comparand)
{
  return (LONG)__sync_val_compare_and_swap((int *)dest, (int)comparand, (int)exchange);
}

// mingw/MSVC spelling; POSIX libc only has snprintf (same signature).
#define _snprintf snprintf

#endif // SMOOTHWHEELSCROLL_PLATFORM_COMPAT_H
