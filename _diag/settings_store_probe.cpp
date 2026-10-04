// Probe: the persisted settings store (LoadSettings / SaveSettings) against an in-memory ExtState.
//
// Compiled against the REAL src/smooth_wheel_scroll.cpp (included below), so the migration and the
// defaults are the shipped code, not a copy. The REAPER ExtState calls are replaced by a map.
// Exits non-zero on the first failed check.
// The standard headers come first: SWELL defines min/max as macros, which break them if they follow.
#include <cmath>
#include <cstdio>
#include <map>
#include <string>

#include "../src/smooth_wheel_scroll.cpp"

static std::map<std::string, std::string> g_store;
static int g_fails = 0;

static const char *FakeGet(const char *section, const char *key)
{
  auto it = g_store.find(std::string(section) + "/" + key);
  return it == g_store.end() ? nullptr : it->second.c_str();
}
static void FakeSet(const char *section, const char *key, const char *val, bool)
{
  g_store[std::string(section) + "/" + key] = val ? val : "";
}

static void Put(const char *key, const char *val) { g_store[std::string(kStateSection) + "/" + key] = val; }
static std::string Get(const char *key) { return g_store[std::string(kStateSection) + "/" + key]; }

static void Check(bool ok, const char *what)
{
  std::printf("%s: %s\n", ok ? "ok  " : "FAIL", what);
  if (!ok) ++g_fails;
}
static bool Near(double a, double b) { return std::fabs(a - b) < 1e-6; }

// A new session: REAPER starts the plugin with g_defaultsRev at its compiled-in 0, then LoadSettings runs.
static void NewSession() { g_defaultsRev = 0; LoadSettings(); }

// Load, then save, the way a restarted session does: a fresh load, then a save on each change.
static void Cycle() { NewSession(); SaveSettings(); }

int main()
{
  GetExtState = FakeGet;
  SetExtState = FakeSet;

  // 1. No defrev, old defaults: migrated once, and SaveSettings writes defrev=2.
  g_store.clear();
  Put("window", "150.000000");
  Put("startd", "1.000000");
  Put("budget", "600.000000");
  Put("speedmul", "1.000000");
  NewSession();
  Check(Near(g_windowMs, kDefaultWindowMs), "1 old window 150 migrated to default");
  Check(Near(g_startDeltas, kDefaultStart), "1 old start 1.0 migrated to default");
  Check(Near(g_budgetDeltas, kDefaultBudget), "1 old budget 600 migrated to default");
  Check(Near(g_speedMul, kDefaultSpeedMul), "1 old speedmul 1.0 migrated to default");
  SaveSettings();
  Check(Get("defrev") == "2", "1 SaveSettings writes defrev=2 after a migration");
  Cycle();
  Check(Near(g_speedMul, kDefaultSpeedMul) && Near(g_windowMs, kDefaultWindowMs),
        "1 a second load does not migrate again");

  // 2. defrev=2 with speedmul exactly 1.0 (a value the user tuned): stays 1.0 through repeated cycles.
  g_store.clear();
  Put("defrev", "2");
  Put("speedmul", "1.000000");
  NewSession();
  Check(Near(g_speedMul, 1.0), "2 speedmul 1.0 stays 1.0 on load");
  SaveSettings();
  Check(Get("defrev") == "2", "2 SaveSettings keeps defrev=2");
  Cycle();
  Cycle();
  Check(Near(g_speedMul, 1.0), "2 speedmul 1.0 survives two more load/save cycles");
  Check(Get("defrev") == "2", "2 defrev stays 2 after repeated cycles");
  Check(Get("speedmul") == "1.000000", "2 stored speedmul unchanged after repeated cycles");

  // 3. defrev=2 with explicitly tuned values that equal the OLD defaults: nothing is migrated.
  g_store.clear();
  Put("defrev", "2");
  Put("window", "150.000000");
  Put("startd", "1.000000");
  Put("budget", "600.000000");
  Put("speedmul", "1.000000");
  Put("glide", "0");
  Put("tprevzoom", "0");
  NewSession();
  Check(Near(g_windowMs, 150.0) && Near(g_startDeltas, 1.0) && Near(g_budgetDeltas, 600.0) &&
            Near(g_speedMul, 1.0),
        "3 no value is re-migrated when defrev=2");
  Check(!g_glideOn && !g_touchpadReverse, "3 switches read from the store");
  SaveSettings();
  Cycle();
  Check(Near(g_windowMs, 150.0) && Near(g_budgetDeltas, 600.0), "3 values still unchanged after save and reload");

  // 4a. Empty store: compiled-in defaults.
  g_store.clear();
  NewSession();
  Check(Near(g_windowMs, kDefaultWindowMs) && Near(g_startDeltas, kDefaultStart) &&
            Near(g_budgetDeltas, kDefaultBudget) && Near(g_speedMul, kDefaultSpeedMul),
        "4a empty store gives the compiled-in defaults");
  Check(g_glideOn && g_touchpadReverse, "4a empty store gives switches on");

  // 4b. Partial store: the keys present are used, the others fall back to the defaults.
  g_store.clear();
  Put("window", "250.000000");
  NewSession();
  Check(Near(g_windowMs, 250.0), "4b present key is read");
  Check(Near(g_startDeltas, kDefaultStart) && Near(g_budgetDeltas, kDefaultBudget) &&
            Near(g_speedMul, kDefaultSpeedMul),
        "4b missing keys fall back to the compiled-in defaults");

  std::printf("%s (%d failed)\n", g_fails ? "FAILED" : "OK", g_fails);
  return g_fails ? 1 : 0;
}
