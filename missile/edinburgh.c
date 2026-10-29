// edinburgh.c -- Mission 8 (world 8): UNITY's regional HQ, then Edinburgh.
//
// The cast, registered with the bullet-hell engine; the dialog, by name;
// and the homecoming, run once a frame from the satellites' update
// (missile.c) before the level proper begins:
//
//   welcome  the ship lands at UNITY's regional HQ in a Highland glen (the
//            real headquarters is under the Antarctic ice);
//            the partition is held at its first frames, with the scroll
//            stopped, for as long as this takes (edinburgh/edin_timeline.py);
//   lore     the Doc: Volta is safe, the Shogun is hurt, and will repair his
//            dreadnought; two new toys;
//   shield   the Valkyrie's shield (missile.c). Practice drones pour fire on
//            the ship; the Doc asks for the shield, waits for it to be raised
//            and run out (or a while), then sends them away;
//   plasma   the plasma sphere (missile.c, P). Launchers fire missiles and
//            kelpies come to ram; the Doc waits for the sphere (or a while);
//   outro    more on the bench; Dr. Marrow is his daughter, and he regrets
//            it; the mission: something in Edinburgh talks to the Shogun
//            every day. The partition goes, and the flight begins;
//   flight   two lines on the way: the Forth Bridge, then the city.
//
// A SKIP TUTORIAL button sits on the HUD through the homecoming, the first
// time the level is played too: tapped, the drills go, the dialog hurries through
// what is left of its lines, and the flight begins (Skip).
//
// Then subagent.c (the mid-boss) and shogungpt.c (the final boss).
//
// The dialog plays strictly in order, so every line is asked for by name
// (edinburgh_lines.h, made by edinburgh/make_edinburgh.py from SPEECHES):
// EdLine steps over the sentences of any speech before it that a run never
// reached -- ShogunGPT's fight has two endings -- in the same frame, so none
// of them is ever drawn (starfish.c SaySkipping).
//
// Same rules as missile.c: no libc, no division, engine reached by address.

#include "seoul_boss.h"
#define EDINBURGH_LINE_TABLE
#include "edinburgh_lines.h"
#include "amaterasu.h"

#define ED_LoadBadGuy       FN(void (*)(u32, const char*), 0xbf324)     // ARM
#define ED_LoadBullet       FN(void (*)(u32, const char*), 0xbeffc)     // ARM
#define ED_LoadGeneric      FN(void (*)(u32, const char*), 0xbf140)     // ARM

#define SH_WORLD   0x4e6c
#define EDINBURGH  8

// the skip button: the HUD's own calls (RenderShieldArea's) and touch
#define ED_LoadSprites      FN(s32 (*)(u32, const char*, u32, u32), 0x57ea9)
#define ED_DrawSprite       FN(void (*)(u32, s32, s32, u32), 0x5f351)
#define ED_GetScreenWidth   FN(s32 (*)(void), 0x4e955)
#define ED_GetScreenHeight  FN(s32 (*)(void), 0x4e971)
#define ED_GetWidth         FN(s32 (*)(u32), 0x4d9b9)
#define ED_GetHeight        FN(s32 (*)(u32), 0x4d9f9)
#define ED_GetMouseX        FN(s32 (*)(u32), 0x62d58)                   // ARM
#define ED_GetMouseY        FN(s32 (*)(u32), 0x62d80)                   // ARM
#define ED_GetMouseState    FN(s32 (*)(u32), 0x5257d)
#define ED_RenderShieldArea FN(void (*)(u32, u32), 0x35f49)
#define ED_isFileExist      FN(s32 (*)(const char*), 0x883f0)           // ARM
#define ED_LoadBinFile      FN(s32 (*)(const char*, void*, u32, u32, u32), 0x88db4)   // ARM
#define ED_SaveBinFile      FN(s32 (*)(const char*, const void*, u32), 0x87d70)       // ARM
#define SH_UI_SCALE   0x8b9dc     // the menus' sprite scale (worlds.c)
// the level's own clock (frames): it slows in bullet time, as the scroll and
// the spawns do -- a frame count of our own ran ahead of them
#define ED_GetPartitionTime FN(s32 (*)(u32), 0xbae08)           // ARM
#define SPR_SKIP      0x3e80      // clear of missile.c's 0x3e00..0x3e7b
#define TAUGHT_FILE   "*edinburgh_taught.bin"   // P and the shield: the homecoming got to the flight
#define P_CAPSULES    (0x5110 + 0x74)            // SHOGUN: the ship's capsules
#define CAPSULE       "common/items/powerup"     // the game's blue capsule (courier.c drops it too)
#define CAPSULE_AGAIN 300                        // frames between spares while none is left
#define SKIP_MARGIN   4           // screen units from the right edge
#define SKIP_PAD      6           // a forgiving touch target
#define SKIP_CLOSE    30          // frames after a skip to close the wheel the tap opens
// Lifting a finger opens the bullet-time weapon wheel (when that setting is
// on, SHOGUN+0x8b470): the tap on the button would too, so it is closed again
#define ED_DeactivateBTMenu FN(void (*)(u32), 0x37aa5)
#define SH_BT_ON      0x8b470
#define SH_WEAPONMENU 0x8b46c

static const char* const kBadguys[] = {
  "*edinburgh/scripts/token",
  "*edinburgh/scripts/token_drift",
  "*edinburgh/scripts/piper",
  "*edinburgh/scripts/thistle",
  "*edinburgh/scripts/kelpie",
  "*edinburgh/scripts/caber",
  "*edinburgh/scripts/unicorn",
  "*edinburgh/scripts/saltire",
  "*edinburgh/scripts/knot",
  "*edinburgh/scripts/token_x",           // hard mode's elites (edin_cast.py, docs/HARD_MODE.md)
  "*edinburgh/scripts/token_drift_x",
  "*edinburgh/scripts/piper_x",
  "*edinburgh/scripts/thistle_x",
  "*edinburgh/scripts/kelpie_x",
  "*edinburgh/scripts/caber_x",
  "*edinburgh/scripts/unicorn_x",
  "*edinburgh/scripts/saltire_x",
  "*edinburgh/scripts/knot_x",
  "*edinburgh/scripts/drill",
  "*edinburgh/scripts/launcher",
  "*edinburgh/boss/scheduler",
  "*edinburgh/boss/gpt",
  "*edinburgh/boss/head",
  "*edinburgh/boss/gpt_turret",
  // the Kurogane, if it comes (chase.c DoomStart): its guns ...
  "*antarctica/boss/turret",
  "*antarctica/boss/bow_gun",
  "*antarctica/boss/flak",
  "*antarctica/boss/wrath",
  // ... Tokyo's ships, its escorts (06, the shield carrier, flies here
  // anyway), and Ocean's missile, which the cabers fire too
  "worlds/storm/scripts/badguy01",
  "worlds/storm/scripts/badguy02",
  "worlds/storm/scripts/badguy03",
  "worlds/storm/scripts/badguy06",
  "worlds/storm/scripts/badguy08",
  "worlds/storm/scripts/badguy09",
  "worlds/ocean/scripts/badguy_missile",
  // (the chase's other escorts and its capsule carriers are Tokyo's ships
  // in the doom: chase.c DoomShip)
};
static const char* const kBullets[] = {
  "*edinburgh/boss/glyph",
  "*edinburgh/boss/petal",
  "*edinburgh/boss/glyph_curve",          // (edin_cast.py: shots that swirl ...
  "*edinburgh/boss/petal_curve",
  "*edinburgh/boss/glyph_accel",          // ... and shots that hang, then lunge)
  "*edinburgh/boss/petal_accel",
  "*edinburgh/boss/glyph_twist",          // the bosses' revamp after Missions 2-4's (edin_cast.py)
  "*edinburgh/boss/petal_twist",
  "*edinburgh/boss/glyph_lock",
  "*edinburgh/boss/petal_lock",
  "*edinburgh/boss/glyph_comma",
  "*edinburgh/boss/petal_comma",
  "*edinburgh/boss/glyph_snipe",
  "*edinburgh/boss/glyph_seed",
  "*edinburgh/boss/petal_hesitate",
  "*antarctica/boss/ember",
  "*antarctica/boss/spark",
  "*edinburgh/boss/h_comma",              // hard mode's two-stage bullets (beach/hardkit.py)
  "*edinburgh/boss/h_twist",
  "*edinburgh/boss/h_snipe",
  "*edinburgh/boss/h_lock",
  "*edinburgh/boss/h_drift",
  "*edinburgh/boss/h_hesitate",
  "*edinburgh/boss/h_needle",
  "*edinburgh/boss/h_sig",                 // its signature pair (hardkit.THEMES)
  "*edinburgh/boss/h_sigb",
  "*antarctica/boss/h_comma",             // the chase guns' hard attacks (chase.c, docs/HARD_MODE.md)
  "*antarctica/boss/h_twist",
  "*antarctica/boss/h_snipe",
  "*antarctica/boss/h_lock",
  "*antarctica/boss/h_drift",
  "*antarctica/boss/h_hesitate",
  "*antarctica/boss/h_needle",
  "*antarctica/boss/h_sig",                 // its signature pair (hardkit.THEMES)
  "*antarctica/boss/h_sigb",
};
static const char* const kGenerics[] = {
  "*edinburgh/boss/uplink",
  // the bosses' moving parts (art_bosses.py): Scheduler's hands, pendulum and
  // bells; ShogunGPT's face and what it types
  "*edinburgh/boss/sched_hands",
  "*edinburgh/boss/sched_pendulum",
  "*edinburgh/boss/sched_bells",
  "*edinburgh/boss/gpt_screen",
  "*edinburgh/boss/gpt_turret_vis",
  "*edinburgh/boss/gpt_text0",
  "*edinburgh/boss/gpt_text1",
  "*edinburgh/boss/gpt_text2",
  // what is seen of the Kurogane
  "*antarctica/boss/underglow",
  "*antarctica/boss/aft",
  "*antarctica/boss/hull",
  "*antarctica/boss/streamer",
  "*antarctica/boss/nacelle",
  "*antarctica/boss/oni",
  "*antarctica/boss/hangar",
  "*antarctica/boss/hatch",
  "*antarctica/boss/megacannon",
  "*antarctica/boss/mega_beam",
  "*antarctica/boss/reticle",
  "*antarctica/boss/turret_vis",
  "*antarctica/boss/turret_big_vis",
  "*antarctica/boss/cannon_vis",
};

#define COUNT(a) (sizeof(a) / sizeof((a)[0]))

// Before the world file, which binds every script's sprites (beach.c).
void RegisterEdinburgh(void) {
  for (u32 i = 0; i < COUNT(kBullets); i++) ED_LoadBullet(BH, kBullets[i]);
  for (u32 i = 0; i < COUNT(kGenerics); i++) ED_LoadGeneric(BH, kGenerics[i]);
  for (u32 i = 0; i < COUNT(kBadguys); i++) ED_LoadBadGuy(BH, kBadguys[i]);
  ED_LoadSprites(SPR_SKIP, "*edinburgh/menu/skip.tga", 1, R32(Shogun(), SH_UI_SCALE));
}

// worlds.c: before any added world loads, this one's own definitions go.
void ForgetDefinitions(const char* const* badguys, u32 nb, const char* const* bullets, u32 nu,
                       const char* const* generics, u32 ng);
void ForgetEdinburgh(void) {
  ForgetDefinitions(kBadguys, COUNT(kBadguys), kBullets, COUNT(kBullets), kGenerics, COUNT(kGenerics));
}

// ---- the dialog, by name ------------------------------------------------------------
static int g_line_next;           // the first speech not yet said or stepped over
// where each line queued so far began (g_line_next before it): a skipped
// tutorial forgets its waiting lines and rewinds to the first, so the next
// line steps over them all (Skip)
#define HIST 48
static unsigned char g_hist[HIST];
static int g_hist_n;

static int SkipTo(int k) {
  int skip = 0;
  if (g_hist_n < HIST) g_hist[g_hist_n++] = (unsigned char)g_line_next;
  for (int i = g_line_next; i < k; i++) skip += kLineSentences[i];
  g_line_next = k + 1;
  return skip;
}

// Queue speech k, after any not yet reached. One already passed is never said.
void EdLine(int k) {
  if (k < g_line_next || k >= L_COUNT) return;
  SaySkipping(SkipTo(k));
}

// The same, now: whatever is queued goes by first (last words).
void EdLineNow(int k) {
  if (k < g_line_next || k >= L_COUNT) return;
  SayNowSkipping(SkipTo(k));
}

// ---- the homecoming -----------------------------------------------------------------------
int PlasmaUp(void);               // missile.c: the ship holds P

enum { T_OFF, T_START, T_TALK, T_SHIELD, T_PLASMA_TALK, T_PLASMA, T_OUTRO, T_FLIGHT };

#define SHIELD_MIN    420         // frames of drill at least, once the shield was used
#define SHIELD_MAX    1000        // ... and at most (the drills leave after ~950)
#define PLASMA_MIN    720
#define PLASMA_MAX    1150
#define KELPIE_EVERY  150         // a pair of rammers this often, while they come
#define KELPIE_UNTIL  1050
#define NDRONES       3

static struct {
  int phase, t, quiet;
  int skipping, was_down, skip_close;
  int capsule_cool;
  int shield_used, plasma_used;
  u32 drone[NDRONES];
  int kelpie_cool;
  int said_forth, said_city;      // the flight's two lines
  int scroll_state, glide;        // the scroll brought home (ScrollHome)
  s32 scroll_last, scroll_step;
} E;

// The layout (edin_timeline.py) puts every landmark where the scroll will be
// if the level starts as the others do: at START + 2.45 rows. But Mission 8
// opens with its world stopped (speed 0), and the engine's fly-in then
// leaves the scroll at 71.63 -- 9.5 rows short. Everything stood that much
// higher up the screen than laid out: HQ at the top edge through the
// homecoming, Calton Hill and the Castle off the top of the screen in both
// boss fights. So once the fly-in has settled, the scroll glides forward to
// where the layout expects it, before the flight starts.
#define SH_SCROLL      0x434
#define HOME_SCROLL    FX(81.12)          // edin_timeline.py START + 2.45
#define HOME_GLIDE_SH  6                  // over 64 frames (a shift: no division in the blob)
enum { SC_SETTLE, SC_GLIDE, SC_DONE };

static void ScrollHome(void) {
  volatile s32* sc = &R32(Shogun(), SH_SCROLL);
  if (E.scroll_state == SC_SETTLE) {
    // the fly-in is over when the scroll has stopped moving
    if (*sc == 0 || *sc != E.scroll_last) { E.scroll_last = *sc; return; }
    E.scroll_step = (HOME_SCROLL - *sc) >> HOME_GLIDE_SH;
    E.glide = 1 << HOME_GLIDE_SH;
    E.scroll_state = SC_GLIDE;
  }
  if (E.scroll_state == SC_GLIDE) {
    *sc += E.scroll_step;
    if (--E.glide <= 0) {
      *sc = HOME_SCROLL;
      E.scroll_state = SC_DONE;
    }
  }
}

// Whether a homecoming has reached the flight, kept across sessions (the
// files directory keeps it; the APK's world folders are replaced, not this).
static s32 g_taught = -1;

static int Taught(void) {
  if (g_taught < 0) {
    g_taught = 0;
    if (ED_isFileExist(TAUGHT_FILE)) ED_LoadBinFile(TAUGHT_FILE, &g_taught, 4, 0, 0);
  }
  return g_taught > 0;
}

static void Teach(void) {
  if (Taught()) return;
  g_taught = 1;
  ED_SaveBinFile(TAUGHT_FILE, &g_taught, 4);
}

// missile.c: P and the shield are on the wheel once the homecoming has taught
// them -- and in Mission 8 itself, where it does, and in Mission 9.
int EdUpgradesReady(void) {
  const s32 world = R32(Shogun(), SH_WORLD);
  // Mission 9's barrage can't be survived without the sphere: there they
  // are on the wheel whatever the save says
  return Taught() || (world == EDINBURGH && E.phase != T_OFF) || world == 9 || world == 10;
}

void EdinburghReset(void) {
  SB_ClearBytes(&E, sizeof E);
  E.phase = T_START;
  g_line_next = 0;
  g_hist_n = 0;
}

// ---- the skip button -----------------------------------------------------------------
static int SkipShown(void) {
  return R32(Shogun(), SH_WORLD) == EDINBURGH && !E.skipping && E.phase >= T_START &&
         E.phase < T_FLIGHT;
}

static void SkipRect(s32* x, s32* y, s32* w, s32* h) {
  *w = ED_GetWidth(SPR_SKIP);
  *h = ED_GetHeight(SPR_SKIP);
  *x = ED_GetScreenWidth() - *w - SKIP_MARGIN;
  *y = (ED_GetScreenHeight() * 92) >> 8;           // a third of the way down, under the dialog box
}

// RenderHUD: `bl RenderShieldArea` (both of them). The HUD, then the button.
void AtlanticHud(void);           // atlantic.c

void hook_render_shield_area(u32 hud, u32 player) {
  ED_RenderShieldArea(hud, player);
  AmaHud();                                        // Missions 14/15: the mirror's sun by the score
  AtlanticHud();                                   // Mission 16: Volta's gauge under the shield bar
  if (!SkipShown()) return;
  s32 x, y, w, h;
  SkipRect(&x, &y, &w, &h);
  ED_DrawSprite(SPR_SKIP, x, y, 0);
}

static void Skip(void);

// A touch that lands on the button (on the press, not while held).
static void SkipTouch(void) {
  const int down = (ED_GetMouseState(0) & 1) != 0;
  const int pressed = down && !E.was_down;
  E.was_down = down;
  if (E.skip_close > 0) {
    // the tap that skipped: once the finger is up, the wheel it opened goes
    const u32 sh = Shogun();
    if (!down && R32(sh, SH_BT_ON)) ED_DeactivateBTMenu(sh + SH_WEAPONMENU);
    E.skip_close--;
  }
  if (!pressed || !SkipShown()) return;
  s32 x, y, w, h;
  SkipRect(&x, &y, &w, &h);
  const s32 mx = (s32)(short)ED_GetMouseX(0), my = (s32)(short)ED_GetMouseY(0);
  if (mx < x - SKIP_PAD || mx > x + w + SKIP_PAD || my < y - SKIP_PAD || my > y + h + SKIP_PAD) return;
  Skip();
}

// Quiet: nobody talking for a moment -- or the step has waited long enough
// (STUCK_WAIT, frames without the dialog changing at all -- no sentence
// lasts that long): it is never allowed to hold the level up for ever.
#define STUCK_WAIT 900
static int Quiet(void) { return E.quiet > 40 || SpeechStalled() > STUCK_WAIT; }

static void Drones(const char* name) {
  static const short kX[NDRONES] = { 110, 200, 290 };
  for (int i = 0; i < NDRONES; i++) E.drone[i] = SB_SpawnBadGuy(BH, name, FX(kX[i]), FX(-30 - 20 * i));
}

// Off you go: a drill leaves the way it came when R20 is set (edin_cast.py).
static void Dismiss(void) {
  for (int i = 0; i < NDRONES; i++) {
    if (Alive(E.drone[i])) SetReg(E.drone[i], REG_CMD, FX(1));
    E.drone[i] = 0;
  }
}

static void Homecoming(void) {
  E.t++;
  if (E.scroll_state != SC_DONE) ScrollHome();
  switch (E.phase) {
  case T_START:
    // hold the partition (and so the scroll, still stopped) until we're done
    SB_PausePartition(BH, 1);
    EdLine(L_TUT_WELCOME);
    EdLine(L_TUT_LORE);
    EdLine(L_TUT_TOOLS);
    EdLine(L_TUT_SHIELD);
    EdLine(L_TUT_BEAMS);
    E.phase = T_TALK;
    E.t = 0;
    break;
  case T_TALK:
    if (E.t > 60 && Quiet()) {
      // capsules for the lesson: they drift down and come to the ship
      SB_SpawnBonus(BH, CAPSULE, FX(140), FX(50));
      SB_SpawnBonus(BH, CAPSULE, FX(200), FX(30));
      SB_SpawnBonus(BH, CAPSULE, FX(260), FX(50));
      E.capsule_cool = CAPSULE_AGAIN;
      Drones("*edinburgh/scripts/drill");
      E.phase = T_SHIELD;
      E.t = 0;
    }
    break;
  case T_SHIELD:
    if (ShieldUp()) E.shield_used = 1;
    // none left and the shield not raised yet: another capsule now and then
    if (!E.shield_used && R32(Shogun(), P_CAPSULES) < 1 && --E.capsule_cool <= 0) {
      SB_SpawnBonus(BH, CAPSULE, PlayerX(), FX(40));
      E.capsule_cool = CAPSULE_AGAIN;
    }
    if (E.t > SHIELD_MAX || (E.shield_used && !ShieldUp() && E.t > SHIELD_MIN)) {
      Dismiss();
      EdLine(L_TUT_SHIELD_DONE);
      EdLine(L_TUT_PLASMA);
      EdLine(L_TUT_PLASMA_UP);
      E.phase = T_PLASMA_TALK;
      E.t = 0;
    }
    break;
  case T_PLASMA_TALK:
    if (E.t > 60 && Quiet()) {
      Drones("*edinburgh/scripts/launcher");
      E.phase = T_PLASMA;
      E.t = 0;
    }
    break;
  case T_PLASMA:
    if (PlasmaUp()) E.plasma_used = 1;
    if (E.t <= KELPIE_UNTIL && --E.kelpie_cool <= 0) {      // (no modulo in the blob)
      E.kelpie_cool = KELPIE_EVERY;
      const s32 x = PlayerX() < FX(200) ? FX(280) : FX(120);
      SB_SpawnBadGuy(BH, "*edinburgh/scripts/kelpie", x, FX(-30));
      SB_SpawnBadGuy(BH, "*edinburgh/scripts/kelpie", FX(400) - x, FX(-60));
    }
    if (E.t > PLASMA_MAX || (E.plasma_used && E.t > PLASMA_MIN)) {
      Dismiss();
      EdLine(L_TUT_PLASMA_DONE);
      EdLine(L_TUT_EXCITED);
      EdLine(L_TUT_DAUGHTER);
      EdLine(L_TUT_REGRET);
      EdLine(L_TUT_MISSION);
      E.phase = T_OUTRO;
      E.t = 0;
    }
    break;
  case T_OUTRO:
    if ((E.t > 60 || E.skipping) && Quiet() && E.scroll_state == SC_DONE) {
      ShipShield(0);
      SpeechHurry(0);
      Teach();                          // P and the shield, from now on, everywhere
      SB_PausePartition(BH, 0);         // the flight: the partition and the scroll go
      E.phase = T_FLIGHT;
    }
    break;
  }
  if (E.phase != T_FLIGHT) ShipShield(InCutscene());
}

// Skipped: the drills go, the lines left hurry by, and the outro starts the
// flight once the box has closed.
static void Skip(void) {
  E.skipping = 1;
  E.skip_close = SKIP_CLOSE;
  Dismiss();
  // the lines still waiting are forgotten -- the next line said steps over
  // them -- and the one on screen hurries to its end
  const int n = SpeechDropQueued();
  if (n > 0 && n <= g_hist_n) {
    g_line_next = g_hist[g_hist_n - n];
    g_hist_n -= n;
  }
  SpeechHurry(1);
  E.phase = T_OUTRO;
  E.t = 0;
}

// A boss has come (subagent.c, shogungpt.c): in play the homecoming is long
// over; a boss forced early (runtime/levelkit.py --boss) ends it here.
void EdinburghBossCame(void) {
  SpeechHurry(0);
  if (E.phase == T_FLIGHT || E.phase == T_OFF) return;
  Dismiss();
  E.phase = T_FLIGHT;
  E.said_forth = E.said_city = 1;   // and no flight lines
}

// Once a frame while playing, from missile.c (every world: only ours acts).
void EdinburghTick(void) {
  if (R32(Shogun(), SH_WORLD) != EDINBURGH || E.phase == T_OFF) return;
  SpeechPump();
  E.quiet = InCutscene() ? 0 : E.quiet + 1;
  SkipTouch();
  if (E.phase != T_FLIGHT) {
    Homecoming();
    return;
  }
  // the flight: its lines by the partition's own clock, so they come over
  // the bridge and the city however long bullet time held things up
  const s32 pt = ED_GetPartitionTime(BH);
  if (!E.said_forth && pt >= FLY_FORTH_T) { E.said_forth = 1; EdLine(L_FLY_FORTH); }
  if (!E.said_city && pt >= FLY_CITY_T) { E.said_city = 1; EdLine(L_FLY_CITY); }
}
