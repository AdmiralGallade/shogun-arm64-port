// hardmode.cpp -- a Cheats tab in the game's own settings menu.
//
// The game has no "hard mode". What it has is *rank*: a 16.16 fixed-point
// dynamic-difficulty value at PLAYER+0x90 that rises when you kill things and
// falls when you are hit, clamped to [BH_GetMinRank(), BH_GetMaxRank()] --
// measured live as 1.0 and 20.0. The engine ratchets the floor itself:
//
//     if (shogun->mission > 1) BH_SetMinRank(kBhHandle, 20.0);   // the ceiling
//
// So "start on hard" is that same call from mission 1. Every lever used here is
// a public export, so none of this patches the binary.
//
// The settings menu is likewise data, not code:
//
//   * tabs live at SHOGUN+0x8ba30, stride 0x568, and the tab *count* is a
//     plain field at SHOGUN+0x8ba34 -- so a third tab can be added,
//   * each tab holds up to 14 SETTINGSLINEs at tab+0x1c, stride 0x60, and its
//     line count is a uint16 at tab+8,
//   * lines are built by the engine's own constructors, which register them by
//     name and give switches a callback pointer.
//
// The memory a third tab needs (0x8c500..0x8ca68) was checked against every
// function in the binary: nothing else addresses it. Tab 3 onward belongs to
// the info box and the What's New panel, so exactly one spare tab exists --
// which is also why the lines live here rather than appended to Options, where
// the panel is a fixed height and an 11th row lands under the OK button.
#include <android/log.h>

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "shogun_runtime.h"

#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  "shogun", __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN,  "shogun", __VA_ARGS__)

namespace shogun {
namespace {

// ---- what the teardown established --------------------------------------
constexpr uint32_t kBhHandle    = 0x405;    // UE handle for the bullet-hell module
constexpr uint32_t kTabsOff     = 0x8ba30;  // SHOGUN -> SETTINGSTAB[]
constexpr uint32_t kTabCountOff = 0x8ba34;  // SHOGUN -> number of tabs
constexpr uint32_t kTabStride   = 0x568;
constexpr uint32_t kTabCountAt  = 0x08;     // uint16: lines used in this tab
constexpr uint32_t kTabLabelAt  = 0x0c;     // inline label buffer
constexpr uint32_t kLinesOff    = 0x1c;     // first SETTINGSLINE within a tab
constexpr uint32_t kLineStride  = 0x60;
constexpr uint32_t kLineCap     = (kTabStride - kLinesOff) / kLineStride;  // 14

// SETTINGSLINE fields (from InitTextSettingsLine / InitSwitchSettingsLine)
constexpr uint32_t kLineWidget = 0x18;     // 0 text, 1 separator, 2 switch, 4 slider
constexpr uint32_t kLineHandle = 0x1c;     // widget handle, via UE_AllocHandle

// Row geometry, copied from the tabs the game builds itself.
constexpr uint32_t kFirstRowY = 129;
constexpr uint32_t kRowStep   = 25;
constexpr uint32_t kRowX      = 24;
constexpr uint32_t kRowW      = 191;
constexpr uint32_t kRowH      = 25;

constexpr uint32_t kOurTab = 2;            // the one spare tab
// Settings widgets own sequential UE handles; the game's run 0x6cb..0x6d7.
constexpr uint32_t kSwHardMode = 0x6e0;
constexpr uint32_t kSwCapsules = 0x6e1;
constexpr uint32_t kSlShield   = 0x6e2;

constexpr uint32_t kRankHard = 20u << 16;  // == the measured ceiling
constexpr uint32_t kRankBase = 1u << 16;

// PLAYER fields, read out of UpdateShield / UpdateLives / InitPlayerGame.
// InitPlayerGame sets shield to 0xaaaa and capsules to 3 on every new game.
constexpr uint32_t kPlayerCapsules = 0x74;
constexpr uint32_t kPlayerShield   = 0x8c;
constexpr int32_t  kShieldFull     = 0xaaaa;
constexpr uint32_t kMaxCapsules    = 9;

// Every one of the game's own sliders passes these two, and UpdateSettingsLine
// mirrors a value as (p12 + p11) - v -- which only balances if they are the
// ends of the range. So the slider runs 0x10000..0x20000, and the midpoint is
// 0x18000. Handing it 0x8000 put the knob below its own minimum.
constexpr uint32_t kSliderMin = 0x10000;
constexpr uint32_t kSliderMax = 0x20000;
constexpr uint32_t kSliderMid = (kSliderMin + kSliderMax) / 2;

// Trap page for the guest-callable callbacks we hand the engine.
constexpr uint32_t kCbBase     = 0x7E800000u;
constexpr uint32_t kCbHardMode  = kCbBase + 0x00;
constexpr uint32_t kCbCapsules  = kCbBase + 0x04;
constexpr uint32_t kCbShieldMv  = kCbBase + 0x08;   // onMove
constexpr uint32_t kCbShieldRel = kCbBase + 0x0c;   // onRelease

const char* kInitSettingsMenu = "_Z16InitSettingsMenuP6SHOGUN";
const char* kInitSettingTabs  = "_Z15InitSettingTabsP6SHOGUN";
const char* kInitSwitchLine =
    "_Z22InitSwitchSettingsLineP12SETTINGSLINEPciiiijS1_PvS2_j";
const char* kInitSliderLine =
    "_Z24InitSlideBarSettingsLineP12SETTINGSLINEPciiiijS1_PvS2_S2_iii";
const char* kInitPlayerGame = "_Z14InitPlayerGameP6PLAYER";
const char* kInitTextLine =
    "_Z20InitTextSettingsLineP12SETTINGSLINEPciiiiS1_jS1_";

struct State {
  Runtime* rt = nullptr;
  uint32_t shogun = 0;          // learned from the InitSettingsMenu watch
  bool     dumped = false;
  bool     tab_made = false;
  bool     giveup = false;
  bool     hard = false;
  bool     want_restore = false;
  uint32_t rank = kRankHard;
  // shield: damage is scaled by 1/mult, so mult>1 is tougher
  double   shield_mult = 1.0;
  bool     caps = false;
  uint32_t player = 0;        // learned from the InitPlayerGame watch
  bool     player_fresh = false;
  int32_t  shield_prev = -1;
  uint32_t shield_line = 0;    // so its label can show the current value
  std::string save_path;
};
State g;

uint16_t RdU16(uint32_t a) { uint16_t v = 0; g.rt->Read(a, &v, 2); return v; }
uint32_t RdU32(uint32_t a) { uint32_t v = 0; g.rt->Read(a, &v, 4); return v; }
void WrU16(uint32_t a, uint16_t v) { g.rt->Write(a, &v, 2); }
void WrU32(uint32_t a, uint32_t v) { g.rt->Write(a, &v, 4); }

bool CallQuiet(const char* sym, const std::vector<uint32_t>& a, uint32_t* out) {
  std::string err;
  return g.rt->CallSym(sym, a, out, &err);
}

uint32_t TabAt(uint32_t i) { return g.shogun + kTabsOff + i * kTabStride; }

uint32_t GuestStr(const char* text) {
  const uint32_t n = static_cast<uint32_t>(std::strlen(text)) + 1;
  const uint32_t p = g.rt->Malloc(n);
  if (p) g.rt->Write(p, text, n);
  return p;
}

// ---- diagnostics ---------------------------------------------------------
void DumpOnce() {
  if (g.dumped || !g.shogun) return;
  g.dumped = true;
  uint32_t mn = 0, mx = 0, cur = 0;
  CallQuiet("BH_GetMinRank", {kBhHandle}, &mn);
  CallQuiet("BH_GetMaxRank", {kBhHandle}, &mx);
  CallQuiet("BH_GetRank",    {kBhHandle}, &cur);
  LOGI("rank: min=%.2f max=%.2f current=%.2f", mn / 65536.0, mx / 65536.0,
       cur / 65536.0);
  const uint32_t tabs = RdU32(g.shogun + kTabCountOff);
  LOGI("settings: %u tabs", tabs);
  for (uint32_t t = 0; t < tabs && t < 4; t++) {
    const uint32_t tab = TabAt(t);
    LOGI("  tab %u '%s': %u lines", t,
         g.rt->CStr(tab + kTabLabelAt, 15).c_str(), RdU16(tab + kTabCountAt));
  }
}

// ---- building the tab ----------------------------------------------------
// Claim the one spare tab. Everything is checked first: being wrong here means
// writing over live game state.
bool EnsureTab() {
  if (g.tab_made) return true;
  if (g.giveup || !g.shogun) return false;
  const uint32_t tabs = RdU32(g.shogun + kTabCountOff);
  if (tabs == 0 || tabs > 8) return false;          // menu not built yet
  if (tabs > kOurTab) { g.tab_made = true; return true; }   // already ours
  if (tabs != kOurTab) {
    LOGW("cheats: expected %u tabs, found %u -- not adding one", kOurTab, tabs);
    g.giveup = true;
    return false;
  }
  const uint32_t tab = TabAt(kOurTab);
  // Inherit the header from a tab the engine built itself. The first 0x1c
  // bytes carry per-tab display geometry -- the settings box among it -- and
  // zeroing them collapsed the panel to a single row, so the lines below the
  // first drew on bare background and OK, which is pinned to the box's
  // bottom-right corner, ended up on top of the first row and unclickable.
  // Tab 1 is copied rather than tab 0 because tab 0's +4 holds the global tab
  // count. Only the lines area is cleared.
  std::vector<uint8_t> header(kLinesOff, 0);
  g.rt->Read(TabAt(1), header.data(), header.size());
  g.rt->Write(tab, header.data(), header.size());
  const std::vector<uint8_t> zero(kTabStride - kLinesOff, 0);
  g.rt->Write(tab + kLinesOff, zero.data(), zero.size());
  g.rt->Write(tab + kTabLabelAt, "Cheats", 7);
  WrU16(tab + kTabCountAt, 0);
  WrU32(g.shogun + kTabCountOff, kOurTab + 1);
  // Let the engine measure the new label and lay the tab strip out itself.
  uint32_t out = 0;
  if (!CallQuiet(kInitSettingTabs, {g.shogun}, &out)) {
    LOGW("cheats: InitSettingTabs failed, rolling back");
    WrU32(g.shogun + kTabCountOff, kOurTab);
    g.giveup = true;
    return false;
  }
  g.tab_made = true;
  LOGI("cheats: added tab %u 'Cheats' (now %u tabs)", kOurTab, kOurTab + 1);
  return true;
}

// Add a switch as the next line of our tab.
bool AddSwitch(const char* name, const char* label, uint32_t handle,
               uint32_t callback, bool state) {
  const uint32_t tab = TabAt(kOurTab);
  const uint16_t n = RdU16(tab + kTabCountAt);
  if (n >= kLineCap) return false;
  const uint32_t line = tab + kLinesOff + n * kLineStride;
  const uint32_t y = kFirstRowY + n * kRowStep;

  const uint32_t nm = GuestStr(name), lb = GuestStr(label);
  if (!nm || !lb) return false;
  // InitSwitchSettingsLine(line, name, x, y, w, h, handle,
  //                        label, callback, user, initialState)
  uint32_t out = 0;
  std::string err;
  if (!g.rt->CallSym(kInitSwitchLine,
                     {line, nm, kRowX, y, kRowW, kRowH, handle, lb, callback,
                      g.shogun, state ? 1u : 0u}, &out, &err)) {
    LOGW("cheats: '%s' failed: %s", label, err.c_str());
    return false;
  }
  if (RdU32(line + kLineWidget) != 2) {
    LOGW("cheats: '%s' did not initialise (widget=%u)", label,
         RdU32(line + kLineWidget));
    return false;
  }
  // Only now make it visible to the menu's loops.
  WrU16(tab + kTabCountAt, static_cast<uint16_t>(n + 1));
  LOGI("cheats: '%s' as line %u @0x%08x y=%u", label, n, line, y);
  return true;
}

// The engine draws no number beside a slider -- its own sliders are all
// unlabelled -- so the value goes in the line's own label. That is the inline
// buffer at line+0x20, a plain 32-byte string, so this is a memory write and
// needs no call into the guest.
void UpdateShieldLabel() {
  if (!g.shield_line) return;
  char buf[32];
  std::snprintf(buf, sizeof buf, "Shield %d%%",
                static_cast<int>(g.shield_mult * 100.0 + 0.5));
  g.rt->Write(g.shield_line + 0x20, buf, std::strlen(buf) + 1);
}

// Inverse of SetShieldFromSlider, so a saved multiplier restores the knob.
uint32_t ShieldToSlider(double mult) {
  const double frac = mult < 1.0 ? (mult - 0.2) / 0.8 * 0.5
                                 : 0.5 + (mult - 1.0) / 4.0 * 0.5;
  const double f = frac < 0.0 ? 0.0 : (frac > 1.0 ? 1.0 : frac);
  return kSliderMin +
         static_cast<uint32_t>(f * (kSliderMax - kSliderMin));
}

// Add a slider as the next line of our tab. The two magic parameters are the
// ones every one of the game's own sliders uses, so they are geometry, not a
// value range; the chosen value arrives in the callback like a switch's state.
bool AddSlider(const char* name, const char* label, uint32_t handle,
               uint32_t on_move, uint32_t on_release, uint32_t value) {
  const uint32_t tab = TabAt(kOurTab);
  const uint16_t n = RdU16(tab + kTabCountAt);
  if (n >= kLineCap) return false;
  const uint32_t line = tab + kLinesOff + n * kLineStride;
  const uint32_t y = kFirstRowY + n * kRowStep;

  const uint32_t nm = GuestStr(name), lb = GuestStr(label);
  if (!nm || !lb) return false;
  uint32_t out = 0;
  std::string err;
  // (line, name, x, y, w, h, handle, label, onMove, onRelease, user, min,
  //  max, value). The last three pointers are two callbacks and a user
  // pointer, NOT callback+user+spare: passing the game state as the second one
  // had the engine call it as a function, which hung the whole game.
  if (!g.rt->CallSym(kInitSliderLine,
                     {line, nm, kRowX, y, kRowW, kRowH, handle, lb, on_move,
                      on_release, g.shogun, kSliderMin, kSliderMax, value},
                     &out, &err)) {
    LOGW("cheats: '%s' failed: %s", label, err.c_str());
    return false;
  }
  if (RdU32(line + kLineWidget) != 4) {
    LOGW("cheats: '%s' did not initialise (widget=%u)", label,
         RdU32(line + kLineWidget));
    return false;
  }
  WrU16(tab + kTabCountAt, static_cast<uint16_t>(n + 1));
  g.shield_line = line;
  UpdateShieldLabel();
  LOGI("cheats: slider '%s' as line %u @0x%08x y=%u", label, n, line, y);
  return true;
}

// An empty row at the end. The OK button is pinned to the settings box's
// bottom-right corner, so whatever control sits on the last row gets covered
// by it -- that is what put OK on top of the Hard Mode switch in Options, and
// on the Shield slider here. A blank final row gives OK somewhere to land.
bool AddSpacer() {
  const uint32_t tab = TabAt(kOurTab);
  const uint16_t n = RdU16(tab + kTabCountAt);
  if (n >= kLineCap) return false;
  const uint32_t line = tab + kLinesOff + n * kLineStride;
  const uint32_t nm = GuestStr("CheatsPad"), empty = GuestStr("");
  if (!nm || !empty) return false;
  // InitTextSettingsLine(line, name, x, y, w, h, label, value, valueText)
  uint32_t out = 0;
  std::string err;
  if (!g.rt->CallSym(kInitTextLine,
                     {line, nm, kRowX, kFirstRowY + n * kRowStep, kRowW, kRowH,
                      empty, 0, empty}, &out, &err)) {
    LOGW("cheats: spacer failed: %s", err.c_str());
    return false;
  }
  WrU16(tab + kTabCountAt, static_cast<uint16_t>(n + 1));
  LOGI("cheats: spacer as line %u (keeps OK off the last control)", n);
  return true;
}

// The menu is rebuilt whenever it is constructed afresh, which resets the
// counts and drops our lines. Re-adding when they are missing is cheaper and
// far more robust than hooking every rebuild.
void BuildLines() {
  if (!EnsureTab()) return;
  if (RdU16(TabAt(kOurTab) + kTabCountAt) != 0) return;   // already populated
  AddSwitch("HardMode", "Hard Mode", kSwHardMode, kCbHardMode, g.hard);
  AddSwitch("MaxCapsules", "Full Capsules", kSwCapsules, kCbCapsules, g.caps);
  AddSlider("ShieldStrength", "Shield", kSlShield, kCbShieldMv, kCbShieldRel,
            ShieldToSlider(g.shield_mult));
  AddSpacer();
}

// ---- preference ----------------------------------------------------------
void LoadPref() {
  FILE* f = std::fopen(g.save_path.c_str(), "rb");
  if (!f) return;
  int hard = 0, caps = 0;
  double mult = 1.0;
  if (std::fscanf(f, "%d %d %lf", &hard, &caps, &mult) == 3) {
    g.hard = hard != 0;
    g.caps = caps != 0;
    if (mult >= 0.2 && mult <= 5.0) g.shield_mult = mult;
  }
  std::fclose(f);
  LOGI("cheats: loaded hard=%d caps=%d shield=%.2fx", g.hard, g.caps,
       g.shield_mult);
}

void SavePref() {
  FILE* f = std::fopen(g.save_path.c_str(), "wb");
  if (!f) return;
  std::fprintf(f, "%d %d %.3f\n", g.hard ? 1 : 0, g.caps ? 1 : 0,
               g.shield_mult);
  std::fclose(f);
}

}  // namespace

void SetFullCapsules(bool on) {
  g.caps = on;
  SavePref();
  LOGI("cheats: full capsules %s", on ? "ON" : "off");
}

// Centre is neutral: the knob in the middle changes nothing, left makes the
// shield weaker and right makes it stronger, symmetrically.
void SetShieldFromSlider(uint32_t raw) {
  double frac = (static_cast<double>(raw) - kSliderMin) /
                static_cast<double>(kSliderMax - kSliderMin);
  if (frac < 0.0) frac = 0.0;
  if (frac > 1.0) frac = 1.0;
  g.shield_mult = frac < 0.5 ? 0.2 + (frac / 0.5) * 0.8    // 0.2x .. 1x
                             : 1.0 + ((frac - 0.5) / 0.5) * 4.0;  // 1x .. 5x
  SavePref();
  UpdateShieldLabel();
  LOGI("cheats: shield slider raw=0x%x -> %.2fx (%+.0f%%)", raw, g.shield_mult,
       (g.shield_mult - 1.0) * 100.0);
}

void SetHardMode(bool on) {
  g.hard = on;
  SavePref();
  LOGI("cheats: hard mode %s", on ? "ON" : "off");
  // This runs from the switch's own callback -- inside guest execution, on a
  // thread already holding the runtime lock. Calling the guest from here
  // deadlocked and froze the game. Record the intent; the tick does the work.
  if (!on) g.want_restore = true;
}

void InstallHardMode(Runtime& rt, const std::string& files_dir) {
  g.rt = &rt;
  g.save_path = files_dir + "/cheats.cfg";
  LoadPref();

  // Guest-callable addresses for the switches. Same mechanism as the 99
  // imports: the engine sees a function pointer, calling it lands in host code
  // that returns to LR like any leaf call.
  rt.AddTrapRegion(kCbBase, 0x1000, [](Runtime& r, uint32_t addr) {
    const uint32_t state = r.Arg(0);
    switch (addr) {
      case kCbHardMode: SetHardMode(state != 0); break;
      case kCbCapsules: SetFullCapsules(state != 0); break;
      case kCbShieldMv:  SetShieldFromSlider(state); break;
      case kCbShieldRel: break;   // must be a real function, but does nothing
      default: LOGW("cheats: callback at unmapped 0x%08x", addr); break;
    }
    r.Ret(0);
  }, false);

  const uint32_t at = rt.SymAddr(kInitSettingsMenu);
  if (!at) { LOGW("cheats: %s not found", kInitSettingsMenu); return; }
  // The game state is never handed to us, but it is r0 of every function that
  // takes a SHOGUN*. The watch only records -- calling into the guest from
  // here would deadlock on the lock this thread already holds.
  rt.AddWatch(at, [](Runtime& r) {
    if (!g.shogun) {
      g.shogun = r.Arg(0);
      LOGI("cheats: SHOGUN=0x%08x", g.shogun);
    }
  });
  LOGI("cheats: watching %s @0x%08x", kInitSettingsMenu, at);

  // Every new game runs InitPlayerGame(PLAYER*), which is both where the
  // PLAYER pointer becomes knowable and the moment "at the start" means.
  const uint32_t pg = rt.SymAddr(kInitPlayerGame);
  if (pg) {
    rt.AddWatch(pg, [](Runtime& r) {
      g.player = r.Arg(0);
      g.player_fresh = true;      // acted on from the tick, not here
      g.shield_prev = -1;
    });
    LOGI("cheats: watching %s @0x%08x", kInitPlayerGame, pg);
  }
}

// Called from the tick, between guest calls -- never from inside one.
void HardModeTick(uint64_t ticks) {
  if (!g.rt || !g.shogun) return;
  if (ticks == 240) DumpOnce();
  if ((ticks % 60) == 0) BuildLines();

  if (g.want_restore) {
    g.want_restore = false;
    uint32_t out = 0;
    CallQuiet("BH_SetMinRank", {kBhHandle, kRankBase}, &out);
    LOGI("cheats: rank floor handed back to the engine");
  }
  // onUpdate re-asserts a floor of 20.0 on the later missions, so setting this
  // once would not survive. Re-applying is two instructions in the guest.
  if (g.hard && (ticks % 30) == 0) {
    uint32_t out = 0;
    CallQuiet("BH_SetMinRank", {kBhHandle, g.rank}, &out);
  }

  if (!g.player) return;

  // "At the start": InitPlayerGame has just set capsules to 3 and the shield
  // to full. Top the capsules up once, here rather than in the watch, because
  // the watch runs before the function that would overwrite it.
  if (g.player_fresh) {
    g.player_fresh = false;
    if (g.caps) {
      WrU32(g.player + kPlayerCapsules, kMaxCapsules);
      LOGI("cheats: capsules set to %u", kMaxCapsules);
    }
  }

  // Shield strength. The engine subtracts damage from PLAYER+0x8c; rather than
  // find and patch every damage site, watch the value and give back the part
  // the multiplier says should not have been taken. mult>1 is tougher.
  if (g.shield_mult != 1.0) {
    const int32_t cur = static_cast<int32_t>(RdU32(g.player + kPlayerShield));
    if (g.shield_prev >= 0 && cur < g.shield_prev && cur >= 0) {
      const int32_t dmg = g.shield_prev - cur;
      int32_t scaled = static_cast<int32_t>(dmg / g.shield_mult);
      if (scaled < 1) scaled = 1;              // never make the player immortal
      int32_t adjusted = g.shield_prev - scaled;
      if (adjusted > kShieldFull) adjusted = kShieldFull;
      if (adjusted < 0) adjusted = 0;
      WrU32(g.player + kPlayerShield, static_cast<uint32_t>(adjusted));
      g.shield_prev = adjusted;
      return;
    }
    g.shield_prev = cur;
  }
}

bool HardModeEnabled() { return g.hard; }

}  // namespace shogun
