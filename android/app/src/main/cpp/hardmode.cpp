// hardmode.cpp -- difficulty control, and the settings line that drives it.
//
// The game has no "hard mode". What it has is *rank*: a 16.16 fixed-point
// dynamic-difficulty value at PLAYER+0x90 that rises when you kill things and
// falls when you are hit, clamped to [BH_GetMinRank(), BH_GetMaxRank()]. The
// engine ratchets the floor itself once you are past the early missions:
//
//     if (shogun->mission > 1) BH_SetMinRank(kBhHandle, 20.0);
//
// So "start on hard" is just raising that floor from the first mission. Every
// lever needed is a public export, so none of this patches the binary.
//
// The settings menu is likewise data, not code. Lines live in a fixed
// [tab][line] array inside the SHOGUN struct, and the per-tab line count is a
// uint16 in memory rather than a compiled-in bound -- so a line can be
// appended at runtime by calling the engine's own constructor and bumping the
// count. The layout constants below were read out of UpdateSettingsMenu and
// InitSwitchSettingsLine; nothing here is guesswork, but all of it is checked
// before use, because being wrong means writing over the game's own state.
#include <android/log.h>

#include <cstdio>
#include <cstring>
#include <string>

#include "shogun_runtime.h"

#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  "shogun", __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN,  "shogun", __VA_ARGS__)

namespace shogun {
namespace {

// ---- what the teardown established --------------------------------------
constexpr uint32_t kBhHandle   = 0x405;    // UE handle for the bullet-hell module
constexpr uint32_t kTabsOff    = 0x8ba30;  // SHOGUN -> SETTINGSTAB[]
constexpr uint32_t kTabStride  = 0x568;
constexpr uint32_t kTabCountAt = 0x08;     // uint16: lines used in this tab
constexpr uint32_t kLinesOff   = 0x1c;     // first SETTINGSLINE within a tab
constexpr uint32_t kLineStride = 0x60;
constexpr uint32_t kLineCap    = (kTabStride - kLinesOff) / kLineStride;  // 14

// SETTINGSLINE fields used here (from InitTextSettingsLine / InitSwitchSettingsLine)
constexpr uint32_t kLineX      = 0x04;
constexpr uint32_t kLineY      = 0x08;
constexpr uint32_t kLineP5     = 0x0c;
constexpr uint32_t kLineP4     = 0x10;
constexpr uint32_t kLineWidget = 0x18;     // 0 = text, 2 = switch
constexpr uint32_t kLineHandle = 0x1c;     // switch handle, via UE_AllocHandle

// The engine's own hard floor, in 16.16. Measured on device, maxRank is also
// 20.0 -- so this floor pins rank to the ceiling, which is what "hard" means
// here: the dynamic difficulty stops being dynamic and stays at the top.
constexpr uint32_t kRankHard = 20u << 16;

// Measured on device: tab 0 is Controls, tab 1 is the gameplay/audio tab that
// already holds "Display Ship Hitbox", "Use Dark Ship" and friends, with 4 of
// its 14 slots free. Lines there sit at x=24, w=191, h=25, stepping y by 25.
constexpr uint32_t kOurTab    = 1;
constexpr uint32_t kLineStep  = 25;
// Settings switches own sequential UE handles; the game's run 0x6cb..0x6d7.
constexpr uint32_t kOurSwitch = 0x6d8;
constexpr uint32_t kOurWidget = 2;        // switch

// Trap page for the one guest-callable function we hand the engine.
constexpr uint32_t kCbBase = 0x7E800000u;

const char* kInitSettingsMenu = "_Z16InitSettingsMenuP6SHOGUN";

struct State {
  Runtime* rt = nullptr;
  uint32_t shogun = 0;          // learned from the InitSettingsMenu watch
  bool     dumped = false;
  bool     enabled = false;
  uint32_t rank = kRankHard;
  std::string save_path;
  uint32_t name_str = 0, label_str = 0;   // guest copies, allocated once
  int      appended = 0;                  // times the line has been added
  bool     giveup = false;
  bool     want_restore = false;          // hand the floor back, from the tick
};
State g;

uint16_t RdU16(uint32_t a) { uint16_t v = 0; g.rt->Read(a, &v, 2); return v; }
uint32_t RdU32(uint32_t a) { uint32_t v = 0; g.rt->Read(a, &v, 4); return v; }

bool CallQuiet(const char* sym, const std::vector<uint32_t>& a, uint32_t* out) {
  std::string err;
  return g.rt->CallSym(sym, a, out, &err);
}

// The engine is the authority on its own numbers; ask it rather than assume.
void DumpOnce() {
  if (g.dumped) return;
  g.dumped = true;

  uint32_t mn = 0, mx = 0, cur = 0;
  CallQuiet("BH_GetMinRank", {kBhHandle}, &mn);
  CallQuiet("BH_GetMaxRank", {kBhHandle}, &mx);
  CallQuiet("BH_GetRank",    {kBhHandle}, &cur);
  LOGI("rank: min=%.2f max=%.2f current=%.2f  (engine's hard floor is %.1f)",
       mn / 65536.0, mx / 65536.0, cur / 65536.0, kRankHard / 65536.0);

  if (!g.shogun) { LOGW("settings: SHOGUN pointer not seen yet"); return; }
  LOGI("settings: SHOGUN=0x%08x tabs at +0x%x stride 0x%x, %u lines/tab max",
       g.shogun, kTabsOff, kTabStride, kLineCap);
  for (uint32_t t = 0; t < 6; t++) {
    const uint32_t tab = g.shogun + kTabsOff + t * kTabStride;
    const uint16_t n = RdU16(tab + kTabCountAt);
    if (n == 0 || n > kLineCap) {          // not a tab, or not one we understand
      LOGI("  tab %u: count=%u  (skipped)", t, n);
      continue;
    }
    LOGI("  tab %u: %u lines, %u free", t, n, kLineCap - n);
    for (uint16_t i = 0; i < n; i++) {
      const uint32_t ln = tab + kLinesOff + i * kLineStride;
      LOGI("     line %u @0x%08x widget=%u x=%d y=%d p4=%d p5=%d handle=0x%x '%s'",
           i, ln, RdU32(ln + kLineWidget),
           static_cast<int32_t>(RdU32(ln + kLineX)),
           static_cast<int32_t>(RdU32(ln + kLineY)),
           static_cast<int32_t>(RdU32(ln + kLineP4)),
           static_cast<int32_t>(RdU32(ln + kLineP5)),
           RdU32(ln + kLineHandle),
           g.rt->CStr(ln + 0x20, 31).c_str());
    }
  }
}

void LoadPref() {
  if (g.save_path.empty()) return;
  FILE* f = std::fopen(g.save_path.c_str(), "rb");
  if (!f) return;
  int on = 0, rank = 0;
  if (std::fscanf(f, "%d %d", &on, &rank) == 2) {
    g.enabled = on != 0;
    if (rank > 0) g.rank = static_cast<uint32_t>(rank);
  }
  std::fclose(f);
  LOGI("hard mode: loaded pref enabled=%d rank=%.2f", g.enabled, g.rank / 65536.0);
}

uint32_t GuestStr(const char* text) {
  const uint32_t n = static_cast<uint32_t>(std::strlen(text)) + 1;
  const uint32_t p = g.rt->Malloc(n);
  if (p) g.rt->Write(p, text, n);
  return p;
}

// Add our switch to the end of the gameplay tab, using the engine's own
// constructor so the line is a real one: registered by name, rendered and
// hit-tested exactly like the game's own. Every assumption is re-checked here
// because a wrong offset would scribble on live game state.
void AppendLine() {
  if (g.giveup || !g.shogun) return;
  const uint32_t tab = g.shogun + kTabsOff + kOurTab * kTabStride;
  const uint16_t n = RdU16(tab + kTabCountAt);
  if (n == 0 || n > kLineCap) return;            // menu not built yet
  if (n >= kLineCap) {
    LOGW("hard mode: tab %u is full (%u lines), not adding", kOurTab, n);
    g.giveup = true;
    return;
  }
  const uint32_t prev = tab + kLinesOff + (n - 1) * kLineStride;
  // If our line is already the last one, the menu has not been rebuilt.
  if (RdU32(prev + kLineHandle) == kOurSwitch) return;

  const int32_t x  = static_cast<int32_t>(RdU32(prev + kLineX));
  const int32_t y  = static_cast<int32_t>(RdU32(prev + kLineY)) + kLineStep;
  const int32_t p4 = static_cast<int32_t>(RdU32(prev + kLineP4));
  const int32_t p5 = static_cast<int32_t>(RdU32(prev + kLineP5));
  if (x <= 0 || x > 400 || p4 <= 0 || p4 > 2000 || p5 <= 0 || p5 > 200) {
    LOGW("hard mode: tab %u line %u looks wrong (x=%d w=%d h=%d), not adding",
         kOurTab, n - 1, x, p4, p5);
    g.giveup = true;
    return;
  }

  if (!g.name_str)  g.name_str  = GuestStr("HardMode");
  if (!g.label_str) g.label_str = GuestStr("Hard Mode");
  if (!g.name_str || !g.label_str) { g.giveup = true; return; }

  const uint32_t line = tab + kLinesOff + n * kLineStride;
  // InitSwitchSettingsLine(line, name, x, y, w, h, switchHandle,
  //                        label, callback, user, initialState)
  const std::vector<uint32_t> a = {
      line, g.name_str, static_cast<uint32_t>(x), static_cast<uint32_t>(y),
      static_cast<uint32_t>(p4), static_cast<uint32_t>(p5), kOurSwitch,
      g.label_str, kCbBase, g.shogun, g.enabled ? 1u : 0u};
  uint32_t out = 0;
  std::string err;
  if (!g.rt->CallSym("_Z22InitSwitchSettingsLineP12SETTINGSLINEPciiiijS1_PvS2_j",
                     a, &out, &err)) {
    LOGW("hard mode: InitSwitchSettingsLine failed: %s", err.c_str());
    g.giveup = true;
    return;
  }
  if (RdU32(line + kLineWidget) != kOurWidget) {
    LOGW("hard mode: line did not initialise (widget=%u), not publishing",
         RdU32(line + kLineWidget));
    g.giveup = true;
    return;
  }
  // Only now make it visible to the menu's loops.
  const uint16_t nn = static_cast<uint16_t>(n + 1);
  g.rt->Write(tab + kTabCountAt, &nn, 2);
  g.appended++;
  LOGI("hard mode: added 'Hard Mode' as tab %u line %u @0x%08x (y=%d), pass %d",
       kOurTab, n, line, y, g.appended);
}

}  // namespace

// Defined below; the switch callback is installed before it in the file.
void SetHardMode(bool on);

void InstallHardMode(Runtime& rt, const std::string& files_dir) {
  g.rt = &rt;
  g.save_path = files_dir + "/hardmode.cfg";
  LoadPref();

  // A guest-callable address for the switch to invoke. This is the same
  // mechanism the 99 imports use: the engine sees a function pointer, calling
  // it lands in a host hook, and the hook returns to LR like any leaf call.
  rt.AddTrapRegion(kCbBase, 0x1000, [](Runtime& r, uint32_t) {
    const uint32_t a0 = r.Arg(0), a1 = r.Arg(1), a2 = r.Arg(2);
    LOGI("hard mode: switch callback(%u, %u, 0x%08x)", a0, a1, a2);
    SetHardMode(a0 != 0);
    r.Ret(0);
  }, false);

  const uint32_t at = rt.SymAddr(kInitSettingsMenu);
  if (!at) { LOGW("hard mode: %s not found", kInitSettingsMenu); return; }
  // The game state is never handed to us, but it is r0 of every function that
  // takes a SHOGUN*. Watching one of them is enough, and this one runs once
  // the menus exist. The watch only records -- calling into the guest from
  // here would deadlock on the runtime lock this thread already holds.
  rt.AddWatch(at, [](Runtime& r) {
    if (!g.shogun) {
      g.shogun = r.Arg(0);
      LOGI("hard mode: SHOGUN=0x%08x (from InitSettingsMenu)", g.shogun);
    }
  });
  LOGI("hard mode: watching %s @0x%08x", kInitSettingsMenu, at);
}

// Called from the tick, between guest calls -- never from inside one.
void HardModeTick(uint64_t ticks) {
  if (!g.rt) return;
  if (ticks == 240) DumpOnce();      // once the menus have been built

  // The settings menu is rebuilt whenever it is constructed afresh, which
  // resets the line count and drops our entry. Re-adding when it is missing is
  // cheaper and far more robust than trying to hook every rebuild.
  if ((ticks % 60) == 0) AppendLine();

  // onUpdate re-asserts a floor of 20.0 on the later missions, so setting this
  // once would not survive. Re-applying is two instructions in the guest.
  if (g.want_restore) {
    g.want_restore = false;
    uint32_t out = 0;
    CallQuiet("BH_SetMinRank", {kBhHandle, 1u << 16}, &out);
    LOGI("hard mode: rank floor handed back to the engine");
  }
  if (g.enabled && (ticks % 30) == 0) {
    uint32_t out = 0;
    CallQuiet("BH_SetMinRank", {kBhHandle, g.rank}, &out);
  }
}

bool HardModeEnabled() { return g.enabled; }

void SetHardMode(bool on) {
  g.enabled = on;
  if (!g.save_path.empty()) {
    FILE* f = std::fopen(g.save_path.c_str(), "wb");
    if (f) { std::fprintf(f, "%d %u\n", on ? 1 : 0, g.rank); std::fclose(f); }
  }
  LOGI("hard mode: %s", on ? "ON" : "off");
  // Turning it off has to hand the rank floor back to the engine -- but this
  // runs from the switch's own callback, i.e. from *inside* guest execution on
  // a thread that already holds the runtime lock. Calling the guest from here
  // deadlocked and froze the game. Record the intent; the tick does the work.
  if (!on) g.want_restore = true;
}

}  // namespace shogun
