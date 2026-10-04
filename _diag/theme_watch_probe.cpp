// Probe: the Linux/macOS panel's theme detection and change watch.
//
// Compiled against the REAPER-facing code in the real src/smooth_wheel_scroll.cpp (included below).
// REAPER's own functions (get_config_var, GetThemeColor, ColorFromNative) are replaced by fakes, so
// each palette and dark-flag case is set exactly. Exits non-zero on the first failed check.
// The standard headers come first: SWELL defines min/max as macros, which break them if they follow.
#include <cstdio>
#include <cstring>
#include <map>
#include <string>

#include "../src/smooth_wheel_scroll.cpp"

static int g_fails = 0;
static void Check(bool ok, const char *what)
{
  std::printf("%s: %s\n", ok ? "ok  " : "FAIL", what);
  if (!ok) ++g_fails;
}

// --- fakes for REAPER's functions -------------------------------------------------------------
static int g_cfgDark = 0;          // what the config flag says (win32_darkmode)
static bool g_haveCfg = true;
static std::map<std::string, int> g_colors; // theme colour keys -> native colour value
static int FakeGetThemeColor(const char *key, int)
{
  auto it = g_colors.find(key);
  return it == g_colors.end() ? -1 : it->second;
}
static void *FakeGetConfigVar(const char *name, int *sz)
{
  static int v;
  if (!g_haveCfg || std::strcmp(name, "win32_darkmode") != 0)
    return nullptr;
  v = g_cfgDark;
  *sz = (int)sizeof(v);
  return &v;
}
static int FakeGetSysColor(int idx) { return idx == COLOR_3DFACE ? (int)RGB(179, 179, 179) : 0; }
static bool g_apiDark = false;
static bool FakeIsDarkMode() { return g_apiDark; }

int main()
{
  GetThemeColor = FakeGetThemeColor;
  GetSysColor = FakeGetSysColor;
  ColorFromNative = nullptr; // raw channel read; the colours below are chosen so this is unambiguous
  get_config_var = FakeGetConfigVar;
  get_ini_file = nullptr;

  // 1. Optional API: present -> used; absent -> config fallback.
  g_isDarkMode = nullptr;
  g_cfgDark = 1;
  bool d = false;
  Check(DetectDarkMode(&d) && d, "1a no API: config flag (dark) is the fallback");
  g_isDarkMode = FakeIsDarkMode;
  g_apiDark = false;
  d = true;
  Check(DetectDarkMode(&d) && d == false, "1b API present: IsDarkMode() wins over a contradicting config flag");
  g_isDarkMode = nullptr;
  g_haveCfg = false;
  d = true;
  Check(DetectDarkMode(&d) == false, "1c no API and no config: fallback reports unknown, no crash");
  g_haveCfg = true;

  // 2. Palette: a colour change inside the same mode changes the signature (dark stays dark).
  g_colors.clear();
  g_colors["col_main_bg"] = (int)RGB(40, 40, 40);
  g_colors["col_main_text"] = (int)RGB(220, 220, 220);
  g_colors["col_main_3dsh"] = (int)RGB(30, 30, 30);
  g_isDarkMode = nullptr;
  const Palette a = ReadPalette();
  const uint32_t sigA = ThemeSigOf(a);
  g_colors["col_main_bg"] = (int)RGB(60, 60, 60); // still dark, different background
  const Palette b = ReadPalette();
  Check(a.dark && b.dark, "2a both palettes are dark");
  Check(ThemeSigOf(b) != sigA, "2b colour change inside the same mode changes the signature");

  // 3. A change of mode (background brightness) changes the signature and the mode.
  g_colors["col_main_bg"] = (int)RGB(200, 200, 200);
  const Palette light = ReadPalette();
  Check(!light.dark && ThemeSigOf(light) != sigA, "3 light background changes the mode and the signature");
  const uint32_t sigDark = sigA;

  // 4. Watch step: an unchanged signature never requests a refresh; a change does, exactly once.
  uint32_t last = sigA;
  int refreshes = 0;
  for (int i = 0; i < 100; ++i)
    if (ThemeWatchStep(&last, sigA))
      ++refreshes;
  Check(refreshes == 0, "4a unchanged signature over 100 checks requests no refresh");
  if (ThemeWatchStep(&last, ThemeSigOf(light)))
    ++refreshes;
  for (int i = 0; i < 50; ++i)
    if (ThemeWatchStep(&last, ThemeSigOf(light)))
      ++refreshes;
  Check(refreshes == 1, "4b one change requests one refresh, then none while it stays");

  // 5. A palette read with no theme colours at all falls back to the Windows panel's values.
  g_colors.clear();
  g_cfgDark = 1;
  const Palette none = ReadPalette();
  Check(none.dark && none.bg == RGB(48, 48, 48) && none.text == RGB(235, 235, 235),
        "5 no theme colours: dark fallback values");

  // 6. Brightness decides the mode, not a contradicting flag (7.81 read 0 in a dark theme).
  g_colors.clear();
  g_colors["col_main_bg"] = (int)RGB(0, 0, 0);
  g_colors["col_main_text"] = -1;
  g_isDarkMode = FakeIsDarkMode;
  g_apiDark = false;
  g_cfgDark = 0;
  const Palette dk = ReadPalette();
  Check(dk.dark && dk.text == RGB(235, 235, 235), "6a dark background with flag 0: dark mode and light text");
  g_colors["col_main_bg"] = (int)RGB(200, 200, 200);
  g_apiDark = true;
  const Palette lt = ReadPalette();
  Check(!lt.dark && lt.text == RGB(0, 0, 0), "6b light background with flag 1: light mode and dark text");
  g_isDarkMode = nullptr;

  // 7. Light background: the panel takes COLOR_3DFACE (the REAPER dialog face); dark keeps col_main_bg.
  g_colors.clear();
  g_colors["col_main_bg"] = (int)RGB(200, 200, 200);
  g_isDarkMode = nullptr;
  const Palette lightFace = ReadPalette();
  Check(!lightFace.dark && lightFace.bg == RGB(179, 179, 179), "7a light: panel background is COLOR_3DFACE");
  g_colors["col_main_bg"] = (int)RGB(0, 0, 0);
  const Palette darkBg = ReadPalette();
  Check(darkBg.dark && darkBg.bg == RGB(0, 0, 0), "7b dark: panel background is col_main_bg");

  // 8. col_main_bg missing: the panel uses col_main_bg2 and decides the mode from it.
  g_colors.clear();
  g_colors["col_main_bg2"] = (int)RGB(51, 51, 51);
  g_isDarkMode = nullptr;
  g_cfgDark = 0;
  const Palette bg2 = ReadPalette();
  Check(bg2.dark && bg2.bg == RGB(51, 51, 51), "8 col_main_bg missing: col_main_bg2 is used and decides the mode");

  // 9. Text: a theme text colour too close to the background is replaced by a mode-based one.
  g_colors.clear();
  g_colors["col_main_bg2"] = (int)RGB(51, 51, 51);
  g_colors["col_main_text"] = (int)RGB(44, 44, 44);
  g_isDarkMode = nullptr;
  const Palette lowC = ReadPalette();
  Check(lowC.dark && lowC.text == RGB(235, 235, 235), "9a text too close to a dark background: light text used");
  g_colors["col_main_text"] = (int)RGB(230, 230, 230);
  const Palette okC = ReadPalette();
  Check(okC.text == RGB(230, 230, 230), "9b text with enough contrast: the theme text is used");

  std::printf("%s (%d failed)\n", g_fails ? "FAILED" : "OK", g_fails);
  return g_fails ? 1 : 0;
}
