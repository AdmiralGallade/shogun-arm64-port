// andaman.c -- Mission 9 (world 9): the Andaman Islands, and the Iron Reef.
//
// After Edinburgh, ShogunGPT's data leads UNITY to the Shogun's hidden
// garrison fortress in the Andaman Sea, the hub of his electronic supply
// lines across the Indian Ocean. The pilot leads a hit-and-run on its heavy
// anti-aircraft guns; UNITY's ships do the heavy lifting:
//
//   flight     the open sea, the archipelago: the garrison's gunboats,
//              missile hydrofoils, flak barges, sky mines, interceptors;
//   battery 1  over the first island, the Typhoon flak battery guards a
//              shield generator (battery.c). Silence it, and the cruiser
//              Lancer and two frigates sail in, shell the generator to
//              pieces, and leave;
//   strait     more of the garrison, its first raiders;
//   battery 2  the Raijin rail battery, fed by two pylons; the cruiser
//              Halberd takes the second generator;
//   approach   the volcanic islands, the garrison throwing everything in;
//   the Reef   the fortress on Barren Island's volcano, its shield still up
//              on its reserves -- with Volta's battleship Bastion and two
//              escorts already shelling it (ironreef.c). The shield fails;
//              the Bastion falls back to give fire support, and Admiral
//              Vorn (the Ocean boss, back) sails out in his battleship, the
//              Ikazuchi, three phases as her hull breaks up; then the
//              Bastion sinks her and levels the fortress.
//
// This file: the cast, registered with the engine; the dialog by name (the
// speeches play strictly in order: a line asked for steps over any before
// it that a run never reached, as Edinburgh's do); the flight's lines; the
// riders -- the batteries, their generators and the fortress are sprites
// standing on islands that are props, so they come down the screen with
// them, from tables made by andaman/anda_timeline.py; shells in flight; and
// UNITY's ships. Run once a frame from the satellites' update (missile.c).
//
// Same rules as missile.c: no libc, no division, engine reached by address.

#define ANDAMAN_LINE_TABLE
#define ANDAMAN_RIDES
#include "andaman.h"
#include "andaman_glow.h"

#define AD_LoadBadGuy       FN(void (*)(u32, const char*), 0xbf324)     // ARM
#define SF_GetFrameCount    FN(u32 (*)(void), 0x72434)                    // ARM
#define AD_LoadBullet       FN(void (*)(u32, const char*), 0xbeffc)     // ARM
#define AD_LoadGeneric      FN(void (*)(u32, const char*), 0xbf140)     // ARM
#define AD_GetPartitionTime FN(s32 (*)(u32), 0xbae08)                   // ARM

static const char* const kBadguys[] = {
  "*andaman/scripts/missile",
  "*andaman/scripts/needle",
  "*andaman/scripts/tobiuo",
  "*andaman/scripts/tobiuo_l",
  "*andaman/scripts/tobiuo_r",
  "*andaman/scripts/gunboat",
  "*andaman/scripts/supply",
  "*andaman/scripts/hydrofoil",
  "*andaman/scripts/barge",
  "*andaman/scripts/skymine",
  "*andaman/scripts/raider",
  "*andaman/scripts/raider_in",
  "*andaman/scripts/tobiuo_x",          // hard mode's elites (anda_cast.py, docs/HARD_MODE.md)
  "*andaman/scripts/tobiuo_l_x",
  "*andaman/scripts/tobiuo_r_x",
  "*andaman/scripts/gunboat_x",
  "*andaman/scripts/hydrofoil_x",
  "*andaman/scripts/barge_x",
  "*andaman/scripts/skymine_x",
  "*andaman/scripts/raider_x",
  "*andaman/boss/flak_main",
  "*andaman/boss/flak_side",
  "*andaman/boss/rail_main",
  "*andaman/boss/rail_pylon",
  "*andaman/boss/ika_launcher",
  "*andaman/boss/ika_vls",
  "*andaman/boss/ika_barrage",
  "*andaman/boss/ika_gun",
  "*andaman/boss/ika_main",
  "*andaman/boss/ika_core",
  "*andaman/boss/fort_gun",
};
static const char* const kBullets[] = {
  "*andaman/boss/shell",
  "*andaman/boss/spark",
  "*andaman/boss/kink",                 // (the curving shells: anda_cast.py)
  "*andaman/boss/curl",
  "*andaman/boss/hook",
  "*andaman/boss/dart",                 // (after Missions 3/4's bosses: anda_cast.py)
  "*andaman/boss/lock",
  "*andaman/boss/h_comma",              // hard mode's two-stage bullets (beach/hardkit.py)
  "*andaman/boss/h_twist",
  "*andaman/boss/h_snipe",
  "*andaman/boss/h_lock",
  "*andaman/boss/h_drift",
  "*andaman/boss/h_hesitate",
  "*andaman/boss/h_needle",
  "*andaman/boss/h_sig",                 // its signature pair (hardkit.THEMES)
  "*andaman/boss/h_sigb",
};
static const char* const kGenerics[] = {
  "*andaman/boss/flak",
  "*andaman/boss/flak_big",
  "*andaman/boss/puff",
  // the batteries and generators
  "*andaman/boss/emplacement",
  "*andaman/boss/flak_vis",
  "*andaman/boss/flakgun_vis",
  "*andaman/boss/rail_vis",
  "*andaman/boss/pylon_vis",
  "*andaman/boss/lance",
  "*andaman/boss/generator",
  "*andaman/boss/gen_dome",
  // UNITY's ships, their guns, their wakes, their shells
  "*andaman/boss/crosshair",
  "*andaman/boss/frigate",
  "*andaman/boss/nturret",
  "*andaman/boss/wake",
  "*andaman/boss/tracer",
  "*andaman/boss/bastion",
  "*andaman/boss/bturret",
  "*andaman/boss/ciws",
  // the Ikazuchi
  "*andaman/boss/ika_bow",
  "*andaman/boss/ika_mid",
  "*andaman/boss/ika_stern",
  "*andaman/boss/launcher_vis",
  "*andaman/boss/vls_vis",
  "*andaman/boss/barrage_vis",
  "*andaman/boss/gun_vis",
  "*andaman/boss/main_vis",
  // the Iron Reef
  "*andaman/boss/fortress",
  "*andaman/boss/fort_gate",
  "*andaman/boss/fort_core",
  "*andaman/boss/fort_dome",
  "*andaman/boss/fortgun_vis",
  "*andaman/boss/fire",
  "*andaman/boss/blast",
  // the Valhalla, her guns, her escorts
  "*andaman/boss/valhalla",
  "*andaman/boss/valhalla_jets",
  "*andaman/boss/val_fans_l",
  "*andaman/boss/val_fans_r",
  "*andaman/boss/valhalla_shadow",
  "*andaman/boss/vturret",
  "*andaman/boss/vrailgun",
  "*andaman/boss/vescort",
  "*andaman/boss/vescort_shadow",
  "*andaman/boss/vesc_patch",
  "*andaman/boss/vesc_turret",
};

#define COUNT(a) (sizeof(a) / sizeof((a)[0]))

// Before the world file, which binds every script's sprites (beach.c).
void RegisterAndaman(void) {
  for (u32 i = 0; i < COUNT(kBullets); i++) AD_LoadBullet(BH, kBullets[i]);
  for (u32 i = 0; i < COUNT(kGenerics); i++) AD_LoadGeneric(BH, kGenerics[i]);
  for (u32 i = 0; i < COUNT(kBadguys); i++) AD_LoadBadGuy(BH, kBadguys[i]);
}

// worlds.c: before any added world loads, this one's own definitions go.
void ForgetDefinitions(const char* const* badguys, u32 nb, const char* const* bullets, u32 nu,
                       const char* const* generics, u32 ng);
void ForgetAndaman(void) {
  ForgetDefinitions(kBadguys, COUNT(kBadguys), kBullets, COUNT(kBullets), kGenerics, COUNT(kGenerics));
}

s32 AnTime(void) { return AD_GetPartitionTime(BH); }

// ---- the dialog, by name ------------------------------------------------------------
// Mission 10 (sunda.c) lends itself this machinery with its own speeches:
// the table of sentence counts is whichever world's is running.
static int g_line_next;           // the first speech not yet said or stepped over
static const unsigned char* g_lines = kLineSentences;
static int g_nlines = L_COUNT;
// The lines asked for lately, oldest first (AnDropQueued steps back over them).
#define LINE_HIST 16
static int g_line_hist[LINE_HIST], g_line_hist_n;


void AnUseLines(const unsigned char* sentences, int n) {
  g_lines = sentences;
  g_nlines = n;
  g_line_next = 0;
  g_line_hist_n = 0;
}

static int SkipTo(int k) {
  int skip = 0;
  for (int i = g_line_next; i < k; i++) skip += g_lines[i];
  if (g_line_hist_n == LINE_HIST) {
    for (int i = 1; i < LINE_HIST; i++) g_line_hist[i - 1] = g_line_hist[i];
    g_line_hist_n--;
  }
  g_line_hist[g_line_hist_n++] = g_line_next;      // (where this one's skipping began)
  g_line_next = k + 1;
  return skip;
}

int SpeechDropQueued(void);

// Chatter still waiting is forgotten (not the line on screen): the next line
// said steps over it (edinburgh.c's Skip).
void AnDropQueued(void) {
  const int n = SpeechDropQueued();
  if (n > 0 && n <= g_line_hist_n) {
    g_line_hist_n -= n;
    g_line_next = g_line_hist[g_line_hist_n];
  }
}

// A ticket: the count of lines put up at which this one is (0: not said).
// A line with no sentences was dropped from the script (make_andaman.py
// DROPPED): nothing is said, and a cue waiting on it goes on at once.
int AnLine(int k) {
  if (k < g_line_next || k >= g_nlines || !g_lines[k]) return 0;
  SaySkipping(SkipTo(k));
  return (int)(SpeechShown() + SpeechPending());
}

int AnLineUp(int ticket) { return (int)SpeechShown() >= ticket; }

void AnLineNow(int k) {
  if (k < g_line_next || k >= g_nlines || !g_lines[k]) return;
  SayNowSkipping(SkipTo(k));
}

// ---- the level's state ----------------------------------------------------------------
#define STUCK_WAIT 900
static struct {
  int on;
  int quiet;
  int said;                       // the flight's lines said, a bit each
} A;
enum { SAID_BRIEF = 1, SAID_ISLES = 2, SAID_CP1 = 4, SAID_STRAIT = 8, SAID_CP2 = 16, SAID_VOLCANO = 32,
       SAID_FORT = 64, SAID_ARGUS = 128 };

int AnQuiet(void) { return A.quiet > 40 || SpeechStalled() > STUCK_WAIT; }

// ---- blasts: fireballs drawn over the big sprites ------------------------------------
// The engine's explosions draw under generics -- over the fortress, a
// generator, a ship's hull they are heard and not seen. A blast is a generic
// fireball played once on top (with the engine's explosion for the sound).
#define NBLAST 20
#define BLAST_LIFE 24             // its 6 frames, 4 ticks each
static struct { u32 a; int t; } BL[NBLAST];

void AnBlast(s32 x, s32 y) {
  int k = 0;
  while (k < NBLAST && BL[k].a) k++;
  if (k == NBLAST) return;
  BL[k].a = AnSpawn("*andaman/boss/blast", x, y);
  BL[k].t = 0;
}

// An explosion that must be seen: the engine's (its sound) and a blast.
void Blast(const char* size, s32 x, s32 y) {
  Boom(size, x, y);
  AnBlast(x, y);
}

static void Blasts(void) {
  for (int k = 0; k < NBLAST; k++)
    if (BL[k].a && ++BL[k].t >= BLAST_LIFE) AnDrop(&BL[k].a);
}

// ---- shells in flight -----------------------------------------------------------------------
#define NSHOT 48
static struct {
  u32 a, bg;
  s32 x, y, dx, dy;
  short left, dmg;
  unsigned char size, tag;
} S[NSHOT];
static int g_landed[NTAGS];

int AnShoot(s32 x0, s32 y0, s32 x1, s32 y1, int speed, int size, int tag, u32 bg, int dmg) {
  int k = 0;
  while (k < NSHOT && S[k].a) k++;
  if (k == NSHOT) return 0;
  // not from off the playfield: the engine can cull a generic spawned there
  if (x0 < FX(-20) || x0 > FX(420) || y0 < FX(-20) || y0 > FX(480)) return 0;
  const s32 ang = AnAngle(x0, y0, x1, y1);
  S[k].a = AnSpawn("*andaman/boss/tracer", x0, y0);
  if (!S[k].a) return 0;
  AnTurn(S[k].a, ang);
  S[k].x = x0;
  S[k].y = y0;
  S[k].dx = SB_Cos(ang) * speed;
  S[k].dy = SB_Sin(ang) * speed;
  S[k].left = (short)(DivSmall(AnDist(x1 - x0, y1 - y0), speed) + 1);
  S[k].size = (unsigned char)size;
  S[k].tag = (unsigned char)tag;
  S[k].bg = bg;
  S[k].dmg = (short)dmg;
  return 1;
}

int AnLanded(int tag) {
  const int n = g_landed[tag];
  g_landed[tag] = 0;
  return n;
}

void AnShotsClear(void) {
  for (int k = 0; k < NSHOT; k++) AnDrop(&S[k].a);
  for (int t = 0; t < NTAGS; t++) g_landed[t] = 0;
}

u32 g_an_spare;

static void Shots(void) {
  static const char* const kSize[4] = { "ExplosionXXS", "ExplosionXS", "ExplosionL", "ExplosionXL" };
  for (int k = 0; k < NSHOT; k++) {
    if (!S[k].a) continue;
    S[k].x += S[k].dx;
    S[k].y += S[k].dy;
    AnPut(S[k].a, S[k].x, S[k].y);
    if (--S[k].left > 0) continue;
    if (S[k].size >= SHOT_BIG) Blast(kSize[S[k].size & 3], S[k].x, S[k].y);   // a heavy shell: seen
    else Boom(kSize[S[k].size & 3], S[k].x, S[k].y);
    if (S[k].size == SHOT_HUGE) SB_ShakeWorld();
    if (S[k].bg && Alive(S[k].bg) && S[k].bg != g_an_spare) {
      SB_SetInvincibility(S[k].bg, -1);
      SB_StrikeBadGuy(BH, S[k].bg, S[k].dmg);
    }
    g_landed[S[k].tag]++;
    AnDrop(&S[k].a);
  }
}

// ---- the garrison's missiles ----------------------------------------------------------------
u32 AnNearestMissile(s32 x, s32 y, s32 range, s32* ox, s32* oy) {
  u32 best = 0;
  s32 bd = range;
  for (u32 b = SB_GetFirstBadGuy(BH); b; b = SB_GetNextBadGuy(BH, b)) {
    if (SB_GetBadGuyHealth(b) <= 0) continue;
    const u32 act = Act(b);
    if ((SB_GetActorRegister(act, 21) >> 16) != MISSILE_MARK) continue;
    s32 mx, my;
    SB_GetActorPosition(act, &mx, &my);
    if (my < FX(0) || my > FX(400)) continue;
    const s32 d = AnDist(mx - x, my - y);
    if (d < bd) { bd = d; best = b; *ox = mx; *oy = my; }
  }
  return best;
}

// ---- UNITY's ships -------------------------------------------------------------------------
// Their art faces up the screen (andaman/art_navy.py), its layout too: a
// piece at (dx, dy) on a ship heading `head` is at that offset turned by
// head - UP.
static void TurretAt(const AnShip* s, int k, s32* dx, s32* dy) {
  if (s->kind == SHIP_CRUISER) {
    *dx = k ? CRUISER_TUR1_X : CRUISER_TUR0_X;
    *dy = k ? CRUISER_TUR1_Y : CRUISER_TUR0_Y;
  } else {
    *dx = FRIGATE_TUR0_X;
    *dy = FRIGATE_TUR0_Y;
  }
}

void ShipTurret(AnShip* s, int k, s32* x, s32* y) {
  s32 dx, dy, ox, oy;
  TurretAt(s, k, &dx, &dy);
  AnRot(dx, dy, s->head - UP, &ox, &oy);
  *x = s->x + ox;
  *y = s->y + oy;
  *x += AnGroundDx(*y);
}

void ShipSpawn(AnShip* s, int kind, s32 x, s32 y, s32 head) {
  SB_ClearBytes(s, sizeof *s);
  s->kind = kind;
  s->x = x;
  s->y = y;
  s->head = head;
  s->ntur = kind == SHIP_CRUISER ? 2 : 1;
  // spawned bottom to top: the wake under the hull, the turrets over it
  s->wake = AnSpawn("*andaman/boss/wake", x, y);
  s->hull = AnSpawn("*andaman/boss/frigate", x, y);   // (the cruisers are gone: the Valhalla does their work)
  for (int k = 0; k < s->ntur; k++) {
    s->tur[k] = AnSpawn("*andaman/boss/nturret", x, y);
    s->tur_ang[k] = head;
    s->tur_cool[k] = 20 + 17 * k;
  }
  ShipPlace(s);
}

void ShipPlace(AnShip* s) {
  const s32 gx = s->x + AnGroundDx(s->y);       // (on the sea)
  AnPut(s->hull, gx, s->y);
  AnTurn(s->hull, s->head);
  s32 ox, oy;
  const int stern = s->kind == SHIP_CRUISER ? CRUISER_STERN : FRIGATE_STERN;
  AnRot(0, stern + 70, s->head - UP, &ox, &oy);
  AnPut(s->wake, gx + ox, s->y + oy);
  AnTurn(s->wake, s->head);
  for (int k = 0; k < s->ntur; k++) {
    s32 tx, ty;
    ShipTurret(s, k, &tx, &ty);
    AnPut(s->tur[k], tx, ty);
    AnTurn(s->tur[k], s->tur_ang[k]);
    if (s->tur_flash[k] > 0) s->tur_flash[k]--;
    AnFrame(s->tur[k], s->tur_flash[k] > 0);
    if (s->tur_cool[k] > 0) s->tur_cool[k]--;
  }
}

void ShipSail(AnShip* s) {
  s->x += SB_MulFP(SB_Cos(s->head), s->speed);
  s->y += SB_MulFP(SB_Sin(s->head), s->speed);
}

// Turn gun k on (tx, ty); once it bears and has reloaded, a shell: 1 fired.
int ShipFire(AnShip* s, int k, s32 tx, s32 ty, int every, int size, int tag, u32 bg, int dmg) {
  if (k >= s->ntur) return 0;
  s32 x, y;
  ShipTurret(s, k, &x, &y);
  const s32 want = AnAngle(x, y, tx, ty);
  s->tur_ang[k] = AnTurnTo(s->tur_ang[k], want, TURN / 150);
  if (s->tur_cool[k] > 0 || AnOff(s->tur_ang[k], want) > TURN / 60) return 0;
  const s32 mx = x + SB_Cos(s->tur_ang[k]) * 12, my = y + SB_Sin(s->tur_ang[k]) * 12;
  if (!AnShoot(mx, my, tx, ty, 7, size, tag, bg, dmg)) return 0;
  s->tur_cool[k] = every;
  s->tur_flash[k] = 6;
  return 1;
}

void ShipDrop(AnShip* s) {
  AnDrop(&s->hull);
  AnDrop(&s->wake);
  for (int k = 0; k < 2; k++) AnDrop(&s->tur[k]);
}

// ---- the riders ------------------------------------------------------------------------------
// What stands on an island (the batteries, the generators, the fortress)
// and the ships holding station off it are put on screen by the engine's
// own scroll (SHOGUN+0x434), through the props camera's projection (kProj,
// measured: andaman/anda_timeline.py) -- so they stay on their islands
// whatever the partition's clock does (it stops while the ship is down; the
// scroll doesn't).
#define SH_SCROLL 0x434

s32 AnScroll(void) { return R32(Shogun(), SH_SCROLL); }

// How far a thing on the sea at playfield y has to be moved, this frame, to
// keep to the props under it as the camera pans with the ship (kGround).
s32 AnGroundDx(s32 y) {
  s32 i = (y - FX(GROUND_LO)) >> 19;          // 8 units an entry
  if (i < 0) i = 0;
  if (i > GROUND_N - 1) i = GROUND_N - 1;
  return SB_MulFP(PlayerX() - FX(200), kGround[i]);
}

// The garrison's boats (anda_cast.py: R14 = 1) kept on the water: each moved
// by the change in its ground offset since last frame (R16 holds it).
static void Boats(void) {
  for (u32 b = SB_GetFirstBadGuy(BH); b; b = SB_GetNextBadGuy(BH, b)) {
    if (SB_GetBadGuyHealth(b) <= 0 || GetReg(b, 14) != FX(1)) continue;
    s32 x, y;
    SB_GetActorPosition(Act(b), &x, &y);
    const s32 dx = AnGroundDx(y);
    SB_SetActorPosition(Act(b), x + dx - GetReg(b, 16), y);
    SetReg(b, 16, dx);
  }
}

// The world on by `d` rows (16.16), the props with it, the partition held
// or not: the engine carries on from what is written here.
void AnScrollBy(s32 d) { R32(Shogun(), SH_SCROLL) += d; }

// Playfield y (16.16) of props row `row` (16.16) as things stand.
s32 AnRowY(s32 row) {
  s32 rr = row - AnScroll() - FX(PROJ_LO);
  if (rr < 0) rr = 0;
  const s32 i = rr >> 12;                     // PROJ_STEP (16) entries a row
  if (i >= PROJ_N - 1) return (s32)kProj[PROJ_N - 1] * 16384;
  const s32 f = rr & 0xfff;
  return (s32)kProj[i] * 16384 + ((s32)kProj[i + 1] - kProj[i]) * f * 4;   // (x4 units to 16.16)
}

#define SHOWN_FROM FX(-240)       // a rider is spawned above the screen from here ...
#define SHOWN_TO   FX(600)        // ... and dropped below it here

static void Riders(void) {
  for (int k = 0; k < 2; k++) {
    const s32 ey = AnRowY(k ? ROW_EMP2 : ROW_EMP1), gy = AnRowY(k ? ROW_GEN2 : ROW_GEN1);
    if (gy > SHOWN_FROM && ey < SHOWN_TO) BatteryRide(k, ey, gy);
    else if (ey >= SHOWN_TO) BatteryRideOut(k, ey, gy);
  }
  const s32 by = AnRowY(ROW_BASTION);
  if (by > SHOWN_FROM - FX(160)) ReefRide(AnRowY(ROW_FORT), by, AnRowY(ROW_ESCORT));
}

// ---- the Valhalla ----------------------------------------------------------------------------
// Volta's flying battleship -- one of the strongest ships UNITY has -- high
// over the sea, her two escorts on her flanks. Her shadow falls far off on
// the sea; her engines' jets are a piece of their own, astern. Her six heavy
// turrets each pick their own target (the nearest of the garrison's in
// reach) and engage it, unless told where to lay their shells; her railgun,
// heavier than the Valkyrie's ever was, swings onto the toughest thing on
// screen (or what she is sent to), charges behind its aiming line, and fires
// once: a beam that ends it.
//
// At the checkpoints (battery.c) she comes up from below the screen and
// holds station low, only her bow on the screen: AnValCome, AnValThere, AnValOrder (a turret's
// next shell), AnValRailOn/At (paced by Volta's call: a line's ticket),
// AnValRailDone, AnValLeave (away up the screen), AnValGone. At the finale
// (ironreef.c) she just flies over: AnFlybyAt.
#define FLY_SPEED   FX(1.7)
#define VS_STATION  FX(470)         // her centre at station: only her bow and railgun up on the screen
#define VS_LEAVE_V  FX(2.2)         // her speed away, at the last
#define VS_RAIL_MAX 720             // the call stuck: she fires anyway
#define VT_RANGE    230
#define VT_EVERY    24
#define VT_HOLD     14              // her turrets' reload, at station ...
#define VT_CHARGE   7               // ... and charging into a mass of them
#define VE_EVERY    36
#define VR_CHARGE   120             // her railgun's charge: two seconds behind its aiming line
#define VR_FIRE     34
#define VR_SEGS     12
#define VR_HIDDEN   6               // the lance's empty animation: a length held ready
#define XH_HUNT     0               // the crosshair: hunting ...
#define XH_LOCKED   1               // ... locked ...
#define XH_HIDDEN   2               // ... unseen
#define ESC_TUR_DX    0             // an escort's turret, from its centre (art_navy.ESC_TUR) ...
#define ESC_TUR_DY    (-26)
#define ESC_PATCH_DX  0             // ... and the patch over its painted gun (art_navy.ESC_PATCH)
#define ESC_PATCH_DY  (-36)
#define VAL_ESC_X   108             // her escorts, either side ...
#define VAL_ESC_Y   (-150)          // ... of her bow
enum { VR_WAIT, VR_TURN, VR_CHARGING, VR_FIRING, VR_DONE };
enum { VS_PASS, VS_RISE, VS_STAY, VS_LEAVE, VS_SIDE, VS_SINK };
static const signed short kVTurX[6] = { VAL_TUR0_X, VAL_TUR1_X, VAL_TUR2_X, VAL_TUR3_X, VAL_TUR4_X, VAL_TUR5_X };
static const signed short kVTurY[6] = { VAL_TUR0_Y, VAL_TUR1_Y, VAL_TUR2_Y, VAL_TUR3_Y, VAL_TUR4_Y, VAL_TUR5_Y };

static struct {
  u32 shadow, jets, ship, stern, fans[2], tur[6], rail, esc[2], esc_shadow[2], seg[VR_SEGS], xhair;
  u32 esc_patch[2], esc_tur[2];     // the escorts' turrets: a patch over the painted gun, a turret that turns
  s32 x, y, tur_ang[6], rail_ang, esc_ang[2];
  int on, tur_cool[6], tur_flash[6], esc_cool[2];
  int rail_state, rail_t;
  u32 rail_bg;                      // her railgun's target, if a badguy ...
  s32 rail_tx, rail_ty;             // ... else this point (0: straight on)
  int rail_line;                    // Volta's call to fire: its ticket (0: none)
  int phase;                        // VS_*
  s32 speed;                        // up the screen, a frame
  // a turret's orders: its next shell, here
  int ord[6], ord_size[6], ord_tag[6], ord_every[6];
  s32 ord_x[6], ord_y[6];
  u32 tgt[6];                       // what each turret is on: none two alike
  s32 station;                      // where she holds (her centre's y) ...
  int charging;                     // ... and whether she is charging in, every gun loosed
  s32 side_x;                       // VS_SIDE: in from the side to this x (Mission 10)
  u32 prev[6];                      // each turret's last target (g_an_roam: it looks for another)
  int esc_flash[2];                 // an escort's turret firing
} V;

// Mission 16 (atlantic.c): her guns at a third the rate (the noise of them nose to nose), each
// turret on a new target after every shot so they keep turning. Reset by AndamanReset.
int g_an_slow = 1;                  // her reloads, and her escorts', times this
int g_an_roam;

// The nearest of the garrison within `range` of (x, y), on the playfield
// (and the toughest, if `toughest`) -- not the railgun's own.
#define NOT_HERS 98                 // an R21 mark her guns and her escorts' leave alone

static u32 Nearest(s32 x, s32 y, s32 range, int toughest, s32* ox, s32* oy) {
  u32 best = 0;
  s32 bd = range, bhp = 0;
  for (u32 b = SB_GetFirstBadGuy(BH); b; b = SB_GetNextBadGuy(BH, b)) {
    const s32 hp = SB_GetBadGuyHealth(b);
    if (hp <= 0 || (b == V.rail_bg && V.phase != VS_PASS)) continue;
    if ((SB_GetActorRegister(Act(b), 21) >> 16) == NOT_HERS) continue;   // (Mission 16: his harpoons, the pilot's to cut)
    s32 mx, my;
    SB_GetActorPosition(Act(b), &mx, &my);
    if (my < FX(0) || my > FX(400) || mx < FX(0) || mx > FX(400)) continue;
    const s32 d = AnDist(mx - x, my - y);
    if (d > range) continue;
    if (toughest ? hp > bhp : d < bd) { bd = d; bhp = hp; best = b; *ox = mx; *oy = my; }
  }
  return best;
}

// The nearest of the garrison in reach of turret k that no other turret is
// on: spread over a mass of them, not all on one.
static u32 NearestFree(int k, s32 x, s32 y, s32* ox, s32* oy) {
  u32 best = 0;
  s32 bd = g_an_roam ? VT_RANGE * 2 : VT_RANGE;
  for (u32 b = SB_GetFirstBadGuy(BH); b; b = SB_GetNextBadGuy(BH, b)) {
    if (SB_GetBadGuyHealth(b) <= 0 || (b == V.rail_bg && V.phase != VS_PASS)) continue;
    if ((SB_GetActorRegister(Act(b), 21) >> 16) == NOT_HERS) continue;   // (Mission 16: his harpoons, the pilot's to cut)
    int taken = 0;
    for (int j = 0; j < 6; j++) if (j != k && V.tgt[j] == b) taken = 1;
    if (taken) continue;
    s32 mx, my;
    SB_GetActorPosition(Act(b), &mx, &my);
    if (my < FX(0) || my > FX(400) || mx < FX(0) || mx > FX(400)) continue;
    s32 d = AnDist(mx - x, my - y);
    if (g_an_roam) {
      if (d > VT_RANGE) continue;
      d += Rnd(0, 140);                         // (roaming: any in reach, not always the nearest ...)
      if (b == V.prev[k]) d += VT_RANGE;        // (... and another than the last)
    }
    if (d < bd) { bd = d; best = b; *ox = mx; *oy = my; }
  }
  return best;
}

static void VPlace(void) {
  // in from the side, what would be past the edge (a position wraps past
  // 512) waits out of sight above the screen until it is in
  const s32 off = FX(-400);
  AnPut(V.shadow, V.x + FX(80) < FX(500) ? V.x + FX(80) : off, V.y + FX(130));
  AnPut(V.jets, V.x, V.y + FX(VAL_JETS_Y));
  AnPut(V.ship, V.x, V.y - FX(VAL_HALF_LEN / 2));   // her bow half ...
  AnPut(V.stern, V.x, V.y + FX(VAL_HALF_LEN / 2)); // ... and her stern half
  // her lift fans running, laid over the wings
  const s32 fl = V.x + FX(VAL_FANS_L_X), fr = V.x + FX(VAL_FANS_R_X);
  AnPut(V.fans[0], fl < FX(500) ? fl : off, V.y + FX(VAL_FANS_L_Y));
  AnPut(V.fans[1], fr < FX(500) ? fr : off, V.y + FX(VAL_FANS_R_Y));
  for (int k = 0; k < 6; k++) {
    const s32 tx = V.x + FX(kVTurX[k]);
    AnPut(V.tur[k], tx < FX(500) ? tx : off, V.y + FX(kVTurY[k]));
    AnTurn(V.tur[k], V.tur_ang[k]);
    AnFrame(V.tur[k], V.tur_flash[k] > 0);
  }
  AnPut(V.rail, V.x + FX(VAL_RAIL_X), V.y + FX(VAL_RAIL_Y));
  AnTurn(V.rail, V.rail_ang);
  for (int s = 0; s < 2; s++) {
    const s32 ex = V.x + FX(s ? VAL_ESC_X : -VAL_ESC_X), ey = V.y + FX(VAL_ESC_Y);
    const int in = ex + FX(50) < FX(500);
    AnPut(V.esc_shadow[s], in ? ex + FX(50) : off, ey + FX(80));
    AnPut(V.esc[s], in ? ex : off, ey);
    AnPut(V.esc_patch[s], in ? ex + FX(ESC_PATCH_DX) : off, ey + FX(ESC_PATCH_DY));
    AnPut(V.esc_tur[s], in ? ex + FX(ESC_TUR_DX) : off, ey + FX(ESC_TUR_DY));
    AnTurn(V.esc_tur[s], V.esc_ang[s]);
    AnFrame(V.esc_tur[s], V.esc_flash[s] > 0);
  }
}

s32 g_fly_from = FX(640);         // where she comes in from, below the screen (not past +512: it wraps)

// Her pieces, bottom to top: shadows, jets, the hull, her guns, her beam's
// lengths (drawn over her hull: a generic made later can be drawn under
// her), her reticle, her escorts -- spawned at the screen's edge (a generic
// spawned far off the playfield is culled at once).
#define VAL_PIECES 34               // the handles at the head of V: shadow .. xhair, esc_patch, esc_tur
_Static_assert(__builtin_offsetof(__typeof__(V), x) == VAL_PIECES * 4, "V's pieces come first");
static int g_val_pre;               // her pieces made early, out of sight (AnValPrespawn)

static void VSpawn(void) {
  const s32 x = FX(200), y = FX(400);
  V.shadow = AnSpawn("*andaman/boss/valhalla_shadow", x, y);
  for (int s = 0; s < 2; s++) V.esc_shadow[s] = AnSpawn("*andaman/boss/vescort_shadow", x, y);
  V.jets = AnSpawn("*andaman/boss/valhalla_jets", x, y);
  V.ship = AnSpawn("*andaman/boss/valhalla", x, y);
  V.stern = AnSpawn("*andaman/boss/valhalla", x, y);
  V.fans[0] = AnSpawn("*andaman/boss/val_fans_l", x, y);
  V.fans[1] = AnSpawn("*andaman/boss/val_fans_r", x, y);
  for (int k = 0; k < 6; k++) V.tur[k] = AnSpawn("*andaman/boss/vturret", x, y);
  V.rail = AnSpawn("*andaman/boss/vrailgun", x, y);
  for (int k = 0; k < VR_SEGS; k++) V.seg[k] = AnSpawn("*andaman/boss/lance", x, y);
  V.xhair = AnSpawn("*andaman/boss/crosshair", x, y);       // her reticle, on what she aims at
  for (int s = 0; s < 2; s++) V.esc[s] = AnSpawn("*andaman/boss/vescort", x, y);
  // their guns, over them: a patch of deck over the painted twin gun, a turret on it that turns
  for (int s = 0; s < 2; s++) {
    V.esc_patch[s] = AnSpawn("*andaman/boss/vesc_patch", x, y);
    V.esc_tur[s] = AnSpawn("*andaman/boss/vesc_turret", x, y);
  }
  AnFrame(V.stern, 1);
  for (int k = 0; k < VR_SEGS; k++) AnFrame(V.seg[k], VR_HIDDEN);
  AnFrame(V.xhair, XH_HIDDEN);
}

// Mission 10: a generic draws over those made before it, and she must fly
// over the fleet and its fires -- so her pieces are made right after them,
// at the start, and kept out of sight (up and to the left, clear of the
// wrap at -512) until she is called; AnFlybyAt then takes them as they are.
void AnValPrespawn(void) {
  if (V.on || g_val_pre) return;
  SB_ClearBytes(&V, sizeof V);
  VSpawn();
  V.x = FX(-300);
  V.y = FX(-300);
  VPlace();
  g_val_pre = 1;
}

void AnFlybyAt(s32 tx, s32 ty) {
  if (V.on) return;
  u32 keep[VAL_PIECES];
  const int pre = g_val_pre;
  for (int k = 0; k < VAL_PIECES; k++) keep[k] = ((u32*)&V)[k];
  SB_ClearBytes(&V, sizeof V);
  if (pre) for (int k = 0; k < VAL_PIECES; k++) ((u32*)&V)[k] = keep[k];
  g_val_pre = 0;
  V.on = 1;
  V.x = FX(200);
  V.y = g_fly_from;
  V.rail_tx = tx;
  V.rail_ty = ty;
  V.rail_ang = UP;
  if (!pre) VSpawn();               // (moved to where she really starts before it is drawn)
  for (int k = 0; k < 6; k++) {
    V.tur_ang[k] = kVTurY[k] > 100 ? DOWN : UP;           // the aft pair look astern
    V.tur_cool[k] = 10 + 7 * k;
  }
  for (int s = 0; s < 2; s++) {
    V.esc_cool[s] = 20 + 13 * s;
    V.esc_ang[s] = UP;
  }
  VPlace();
}

void AnFlyby(void) { AnFlybyAt(0, 0); }

// Mission 10: in from the side, at height y, from x0 to x1 -- only so far
// that her railgun is on the screen -- and there she holds.
void AnValComeSide(s32 x0, s32 x1, s32 y) {
  AnFlybyAt(0, 0);
  V.x = x0;
  V.y = y;
  V.side_x = x1;
  V.station = y;
  V.phase = VS_SIDE;
  VPlace();
}

int AnValRailState(void) { return V.rail_state; }

// Mission 10: on station, she moves to (x, y) (easing) -- over the pilot,
// to take the ship aboard.
void AnValMoveTo(s32 x, s32 y) {
  V.side_x = x;
  V.station = y;
}

void AnValPos(s32* x, s32* y) {
  *x = V.x;
  *y = V.y;
}

// Mission 16: her escorts, either side of her bow -- where, whether still up, and lost
void AnValEscortPos(int s, s32* x, s32* y) {
  *x = V.x + FX(s ? VAL_ESC_X : -VAL_ESC_X);
  *y = V.y + FX(VAL_ESC_Y);
}

int AnValEscortUp(int s) { return V.on && V.esc[s & 1] != 0; }

void AnValEscortLose(int s) {
  s &= 1;
  if (!V.esc[s]) return;
  s32 x, y;
  AnValEscortPos(s, &x, &y);
  AnBlast(x, y);
  Boom("ExplosionL", x, y);
  AnDrop(&V.esc[s]);
  AnDrop(&V.esc_shadow[s]);
  AnDrop(&V.esc_patch[s]);
  AnDrop(&V.esc_tur[s]);
}

// Mission 16: her turrets and her railgun handed over as she goes down (her wreck carries them,
// atlantic.c): their handles, their offsets from her centre (16.16) and headings. She forgets them,
// so AnValVanish leaves them be.
int AnValTakeGuns(u32* a, s32* dx, s32* dy, s32* ang) {
  int n = 0;
  for (int k = 0; k < 6; k++) {
    if (!V.tur[k]) continue;
    a[n] = V.tur[k];
    dx[n] = FX(kVTurX[k]);
    dy[n] = FX(kVTurY[k]);
    ang[n] = V.tur_ang[k];
    V.tur[k] = 0;
    n++;
  }
  if (V.rail) {
    a[n] = V.rail;
    dx[n] = FX(VAL_RAIL_X);
    dy[n] = FX(VAL_RAIL_Y);
    ang[n] = V.rail_ang;
    V.rail = 0;
    n++;
  }
  return n;
}

// Mission 16: every piece of her gone at once (her wreck takes over, atlantic.c)
void AnValVanish(void) {
  for (int k = 0; k < VAL_PIECES; k++) AnDrop(&((u32*)&V)[k]);
  V.on = 0;
}

void AnValRailMove(s32 x, s32 y) {
  V.rail_tx = x;
  V.rail_ty = y;
}

// ---- her orders, at a checkpoint ----
void AnValCome(void) {
  AnFlybyAt(0, 0);
  V.phase = VS_RISE;
  V.station = VS_STATION;
}

// Her station moved (she eases there), and whether she charges, every gun loosed.
void AnValStation(s32 y, int charging) {
  V.station = y;
  V.charging = charging;
}

int AnValThere(void) { return V.on && V.phase == VS_STAY; }
int AnValGone(void) { return !V.on; }

// Mission 13: she drops back, down the screen and out of sight (following on behind the pilot).
void AnValSink(void) {
  if (!V.on || V.phase == VS_SINK) return;
  V.phase = VS_SINK;
  V.speed = 0;
  for (int k = 0; k < 6; k++) V.ord[k] = 0;
}

void AnValLeave(void) {
  if (V.phase == VS_PASS || V.phase == VS_LEAVE) return;
  V.phase = VS_LEAVE;
  V.speed = 0;
  for (int k = 0; k < 6; k++) V.ord[k] = 0;
}

int AnValBusy(int k) { return V.ord[k]; }

void AnValOrder(int k, s32 x, s32 y, int size, int tag, int every) {
  V.ord[k] = 1;
  V.ord_x[k] = x;
  V.ord_y[k] = y;
  V.ord_size[k] = size;
  V.ord_tag[k] = tag;
  V.ord_every[k] = every;
}

static void RailGo(int line) {
  V.rail_state = VR_TURN;
  V.rail_t = 0;
  V.rail_line = line;
}

void AnValRailOn(u32 bg, int line) {
  V.rail_bg = bg;
  V.rail_tx = V.rail_ty = 0;
  RailGo(line);
}

void AnValRailAt(s32 x, s32 y, int line) {
  V.rail_bg = 0;
  V.rail_tx = x;
  V.rail_ty = y;
  RailGo(line);
}

int AnValRailDone(void) { return V.rail_state == VR_DONE; }

// Her beam, along the railgun (anim -1: held ready, unseen, at the muzzle).
static void VRailLine(int anim) {
  const s32 px = V.x + FX(VAL_RAIL_X), py = V.y + FX(VAL_RAIL_Y);
  const s32 ux = SB_Cos(V.rail_ang), uy = SB_Sin(V.rail_ang);
  const s32 mx = px + ux * VAL_RAIL_BARREL, my = py + uy * VAL_RAIL_BARREL;
  for (int k = 0; k < VR_SEGS; k++) {
    if (!V.seg[k]) continue;
    s32 x = mx + ux * (20 + 40 * k), y = my + uy * (20 + 40 * k);
    const int want = anim >= 0 && y < FX(500) && y > FX(-40) && x > FX(-40) && x < FX(440);
    if (!want) { x = px; y = py; }
    AnPut(V.seg[k], x, y);
    AnTurn(V.seg[k], V.rail_ang);
    AnFrame(V.seg[k], want ? anim : VR_HIDDEN);
  }
}

static void VRail(void) {
  const s32 px = V.x + FX(VAL_RAIL_X), py = V.y + FX(VAL_RAIL_Y);
  s32 tx = 0, ty = 0;
  switch (V.rail_state) {
  case VR_WAIT:
    // passing over: once she is well up the screen, pick her target (at a
    // checkpoint she is told)
    if (py > FX(330) || V.phase != VS_PASS) break;
    if (V.rail_tx) { tx = V.rail_tx; ty = V.rail_ty; V.rail_bg = 0; }
    else V.rail_bg = Nearest(px, py, 400, 1, &tx, &ty);
    V.rail_tx = V.rail_bg ? 0 : (tx ? tx : px);
    V.rail_ty = V.rail_bg ? 0 : (tx ? ty : FX(-200));
    V.rail_state = VR_TURN;
    break;
  case VR_TURN:
  case VR_CHARGING:
  case VR_FIRING:
    if (V.rail_bg && Alive(V.rail_bg)) {
      SB_GetActorPosition(Act(V.rail_bg), &tx, &ty);
      V.rail_tx = tx;                         // (kept: once it dies, the beam holds there)
      V.rail_ty = ty;
    } else if (V.rail_bg && !V.rail_tx) { tx = px; ty = py - FX(200); }
    else { tx = V.rail_tx; ty = V.rail_ty; }
    V.rail_ang = AnTurnTo(V.rail_ang, AnAngle(px, py, tx, ty), V.rail_state == VR_FIRING ? TURN / 600 : TURN / 160);
    V.rail_t++;
    if (V.rail_state == VR_TURN) {
      // on it, the target on screen, Volta's call up
      const int ready = V.rail_t > VS_RAIL_MAX || (ty > FX(20) && (!V.rail_line || AnLineUp(V.rail_line)));
      if (ready && (AnOff(V.rail_ang, AnAngle(px, py, tx, ty)) < TURN / 128 || V.rail_t > 80)) {
        V.rail_state = VR_CHARGING;
        V.rail_t = 0;
      }
    } else if (V.rail_state == VR_CHARGING) {
      AnFrame(V.rail, 1);
      VRailLine(0);
      if (V.rail_t >= VR_CHARGE) {
        V.rail_state = VR_FIRING;
        V.rail_t = 0;
        SB_ShakeWorld();
        SB_ShakeWorld();
        if (V.rail_bg && Alive(V.rail_bg)) {
          SB_SetInvincibility(V.rail_bg, -1);
          SB_StrikeBadGuy(BH, V.rail_bg, 6000);
        }
        Boom("ExplosionXL", tx, ty);
      }
    } else {
      AnFrame(V.rail, 2);
      VRailLine(V.rail_t < 4 ? 1 : (V.rail_t < VR_FIRE - 6 ? 5 : 4));
      if (!V.rail_bg && (V.rail_t & 7) == 0 && ty > FX(0)) Boom("ExplosionXL", tx + FX(Rnd(-30, 30)), ty + FX(Rnd(-20, 20)));
      if (V.rail_t >= VR_FIRE) {
        VRailLine(-1);
        AnFrame(V.rail, 0);
        V.rail_state = VR_DONE;
      }
    }
    break;
  default:
    V.rail_ang = AnTurnTo(V.rail_ang, UP, TURN / 300);
    break;
  }
}

// Each heavy turret on its orders, else its own target; the escorts' guns
// on theirs.
static void VGuns(void) {
  for (int k = 0; k < 6; k++) {
    const s32 x = V.x + FX(kVTurX[k]), y = V.y + FX(kVTurY[k]);
    if (V.tur_flash[k] > 0) V.tur_flash[k]--;
    if (V.tur_cool[k] > 0) V.tur_cool[k]--;
    const int live = y > FX(-20) && y < FX(480);
    s32 tx, ty;
    u32 t = 0;
    if (V.ord[k]) { tx = V.ord_x[k]; ty = V.ord_y[k]; }
    else if (live) {
      // her own target while it lives and is in reach; else the nearest free
      t = V.tgt[k];
      if (t && Alive(t)) {
        SB_GetActorPosition(Act(t), &tx, &ty);
        if (ty < FX(0) || ty > FX(400) || AnDist(tx - x, ty - y) > VT_RANGE) t = 0;
      } else t = 0;
      if (!t) t = NearestFree(k, x, y, &tx, &ty);
    }
    V.tgt[k] = t;
    if (!V.ord[k] && !t) { V.tur_ang[k] = AnTurnTo(V.tur_ang[k], kVTurY[k] > 100 ? DOWN : UP, TURN / 300); continue; }
    const s32 want = AnAngle(x, y, tx, ty);
    V.tur_ang[k] = AnTurnTo(V.tur_ang[k], want, TURN / 100);
    if (!live || V.tur_cool[k] || AnOff(V.tur_ang[k], want) > TURN / 64) continue;
    const s32 mx = x + SB_Cos(V.tur_ang[k]) * 24, my = y + SB_Sin(V.tur_ang[k]) * 24;
    const int fired = V.ord[k] ? AnShoot(mx, my, tx, ty, 9, V.ord_size[k], V.ord_tag[k], 0, 0)
                               : AnShoot(mx, my, tx, ty, 9, SHOT_MEDIUM, TAG_NONE, t, 140);
    if (!fired) continue;
    V.tur_cool[k] = V.ord[k] ? V.ord_every[k] : (V.phase == VS_PASS ? VT_EVERY : (V.charging ? VT_CHARGE : VT_HOLD));
    V.tur_cool[k] *= g_an_slow;
    if (g_an_roam) { V.prev[k] = t; V.tgt[k] = 0; }   // a new target for the next shot
    V.tur_flash[k] = 6;
    V.ord[k] = 0;
  }
  for (int s = 0; s < 2; s++) {
    if (!V.esc[s]) continue;                    // (lost: Mission 16)
    const s32 ex = V.x + FX(s ? VAL_ESC_X : -VAL_ESC_X), ey = V.y + FX(VAL_ESC_Y) - FX(40);
    if (V.esc_flash[s] > 0) V.esc_flash[s]--;
    s32 tx, ty;
    const u32 t = (ey > FX(-20) && ey < FX(410)) ? Nearest(ex, ey, VT_RANGE, 0, &tx, &ty) : 0;
    // its turret turns onto what it shoots at (else back to straight ahead), and fires once it bears
    const s32 want = t ? AnAngle(ex, ey, tx, ty) : UP;
    V.esc_ang[s] = AnTurnTo(V.esc_ang[s], want, TURN / 90);
    if (V.esc_cool[s] > 0) { V.esc_cool[s]--; continue; }
    if (t && AnOff(V.esc_ang[s], want) < TURN / 40 && AnShoot(ex, ey, tx, ty, 10, SHOT_SMALL, TAG_NONE, t, 60)) {
      V.esc_cool[s] = VE_EVERY * g_an_slow;
      V.esc_flash[s] = 6;
    }
  }
}

static void Flyby(void) {
  if (!V.on) return;
  switch (V.phase) {
  case VS_PASS:
    V.y -= FLY_SPEED;
    break;
  case VS_RISE: {
    // up from below, slowing onto her station
    s32 v = (V.y - VS_STATION) >> 6;
    if (v > FX(1.8)) v = FX(1.8);
    if (v < FX(0.5)) v = FX(0.5);
    V.y -= v;
    if (V.y <= VS_STATION) {
      V.y = VS_STATION;
      V.phase = VS_STAY;
    }
    break;
  }
  case VS_SIDE: {
    // in from the side at speed, braking onto her station
    const s32 d = V.side_x - V.x;
    s32 v = AnAbs(d) >> 5;
    if (v > FX(4)) v = FX(4);
    if (v < FX(0.6)) v = FX(0.6);
    if (AnAbs(d) <= v) {
      V.x = V.side_x;
      V.phase = VS_STAY;
    } else V.x += d > 0 ? v : -v;
    break;
  }
  case VS_STAY:
    // on her station, or easing to it (forward into them, at a charge)
    V.y += Ease(V.station - V.y, V.charging ? FX(2.2) : FX(1.2));
    if (V.side_x) V.x += Ease(V.side_x - V.x, FX(1.2));   // (Mission 10: across, too)
    break;
  case VS_SINK:                     // Mission 13: back down the screen, gathering speed
    if (V.speed < FX(1.4)) V.speed += FX(0.015);
    V.y += V.speed;
    break;
  default:                          // VS_LEAVE: away up the screen, gathering speed
    if (V.speed < VS_LEAVE_V) V.speed += FX(0.02);
    V.y -= V.speed;
    break;
  }
  VGuns();
  VPlace();
  VRail();
  if (V.phase == VS_SINK) {
    // going down past the bottom: each piece dropped once its centre is past
    // it, before its position wraps round to the top (Mission 13)
    const s32 b = FX(520);
    if (V.y + FX(130) > b) AnDrop(&V.shadow);
    if (V.y + FX(VAL_JETS_Y) > b) AnDrop(&V.jets);
    if (V.y + FX(VAL_HALF_LEN / 2) > b) AnDrop(&V.stern);
    if (V.y + FX(VAL_FANS_L_Y) > b) AnDrop(&V.fans[0]);
    if (V.y + FX(VAL_FANS_R_Y) > b) AnDrop(&V.fans[1]);
    for (int k = 0; k < 6; k++)
      if (V.y + FX(kVTurY[k]) > b) AnDrop(&V.tur[k]);
    if (V.y + FX(VAL_RAIL_Y) > b) {
      AnDrop(&V.rail);
      AnDrop(&V.xhair);
      for (int k = 0; k < VR_SEGS; k++) AnDrop(&V.seg[k]);
    }
    if (V.y + FX(VAL_ESC_Y) + FX(80) > b)
      for (int s = 0; s < 2; s++) AnDrop(&V.esc_shadow[s]);
    if (V.y + FX(VAL_ESC_Y) > b)
      for (int s = 0; s < 2; s++) { AnDrop(&V.esc[s]); AnDrop(&V.esc_patch[s]); AnDrop(&V.esc_tur[s]); }
    if (V.y - FX(VAL_HALF_LEN / 2) > b) {
      AnDrop(&V.ship);
      V.on = 0;
    }
    return;
  }
  if (V.rail_state != VR_CHARGING && V.rail_state != VR_FIRING) VRailLine(-1);   // (held ready, with her)
  {
    // her reticle: on the target while she swings onto it and charges
    s32 tx = V.rail_tx, ty = V.rail_ty;
    if (V.rail_bg && Alive(V.rail_bg)) SB_GetActorPosition(Act(V.rail_bg), &tx, &ty);
    const int on = (V.rail_state == VR_TURN || V.rail_state == VR_CHARGING) && ty > FX(-10);
    AnPut(V.xhair, on ? tx : V.x, on ? ty : V.y);
    AnFrame(V.xhair, !on ? XH_HIDDEN : (V.rail_state == VR_CHARGING ? XH_LOCKED : XH_HUNT));
  }
  // her engines' roar, while she moves
  if (V.phase != VS_STAY && (SF_GetFrameCount() & 63) == 0 && V.y < FX(400) && V.y > FX(-100)) SB_ShakeWorld();
  // off the top, a piece at a time: each goes once it is out of sight, well
  // before its position wraps round (past -512 it would show again at the
  // bottom for a moment)
  if (V.y + FX(VAL_ESC_Y) < FX(-160))
    for (int s = 0; s < 2; s++) {
      AnDrop(&V.esc[s]);
      AnDrop(&V.esc_shadow[s]);
      AnDrop(&V.esc_patch[s]);
      AnDrop(&V.esc_tur[s]);
    }
  if (V.y < FX(-VAL_HALF_LEN - 40)) {
    AnDrop(&V.ship);
    AnDrop(&V.stern);
    for (int s = 0; s < 2; s++) AnDrop(&V.fans[s]);
    AnDrop(&V.rail);
    AnDrop(&V.xhair);
    for (int k = 0; k < 6; k++) AnDrop(&V.tur[k]);
    for (int k = 0; k < VR_SEGS; k++) AnDrop(&V.seg[k]);
  }
  if (V.y < FX(-VAL_JETS_Y - 70)) AnDrop(&V.jets);
  if (V.y < FX(-380)) {
    AnDrop(&V.shadow);
    V.on = 0;
  }
}

// ---- a new run, and every frame ----------------------------------------------------------------
void AndamanReset(void) {
  g_an_slow = 1;
  g_an_roam = 0;
  SB_ClearBytes(&A, sizeof A);
  g_an_spare = 0;
  SB_ClearBytes(&V, sizeof V);
  for (int k = 0; k < NSHOT; k++) S[k].a = 0;          // (the world went: nothing to drop)
  for (int k = 0; k < NBLAST; k++) BL[k].a = 0;
  for (int t = 0; t < NTAGS; t++) g_landed[t] = 0;
  BatteryForget();
  ReefForget();
  AnUseLines(kLineSentences, L_COUNT);
  g_val_pre = 0;
  A.on = 1;
}

static void Flight(s32 pt) {
  if (!(A.said & SAID_BRIEF) && pt >= FLY_BRIEF_T) {
    A.said |= SAID_BRIEF;
    AnLine(L_FLY_BRIEF);            // the Doc: ShogunGPT's data led us here ...
    AnLine(L_FLY_PLAN);             // ... two generators, heavy AA guns: hit and run
  }
  if (!(A.said & SAID_ARGUS) && pt >= FLY_ARGUS_T) {
    A.said |= SAID_ARGUS;
    AnLine(L_FLY_ARGUS);            // Captain Argus, the Bastion: shelling the Iron Reef
  }
  if (!(A.said & SAID_CP1) && AnRowY(ROW_GEN1) > FX(-20)) { A.said |= SAID_CP1; AnLine(L_CP1_SEE); }
  if (!(A.said & SAID_STRAIT) && pt >= FLY_STRAIT_T) { A.said |= SAID_STRAIT; AnLine(L_FLY_STRAIT); }
  if (!(A.said & SAID_CP2) && AnRowY(ROW_GEN2) > FX(-20)) { A.said |= SAID_CP2; AnLine(L_CP2_SEE); }
  if (!(A.said & SAID_VOLCANO) && pt >= FLY_VOLCANO_T) { A.said |= SAID_VOLCANO; AnLine(L_FLY_VOLCANO); }
  if (!(A.said & SAID_FORT) && AnRowY(ROW_FORT) > FX(-30)) { A.said |= SAID_FORT; AnLine(L_FLY_FORT); }
}

// Once a frame while playing, from missile.c (every world: only ours acts --
// and Mission 10, which borrows the shells, the boats, the blasts and the
// Valhalla, and runs its own story: sunda.c).
void AndamanTick(void) {
  const s32 world = R32(Shogun(), SH_WORLD_NO);
  if ((world != ANDAMAN && world != SUNDA && world != HOMEWARD && world != AMATERASU && world != PARIS &&
       world != ATLANTIC) || !A.on)
    return;
  SpeechPump();
  A.quiet = InCutscene() ? 0 : A.quiet + 1;
  if (world == ANDAMAN) {
    const s32 pt = AnTime();
    Flight(pt);
    Riders();
  }
  Boats();
  Shots();
  Blasts();
  Flyby();
}
