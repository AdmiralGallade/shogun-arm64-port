// paris.c -- Mission 15 (world 15): The Third Mirror. Paris, then Base Lutece.
//
// The French base can't be evacuated until it has launched Amaterasu-3, and
// the Shogun knows it (docs/M15_DESIGN.md):
//
//   the flyover   west to east over Paris (paris/art_paris_city.py's map),
//                 the Hojo assault wing streaming over the city for the base;
//                 the general's call, the Doc's "supply raid", the Valhalla
//                 passing over the Seine;
//   the stop      the code brings the scroll to rest over Base Lutece itself
//                 (Scroll: it watches the real scroll), and the partition
//                 waits at each of the story's gates (Gates);
//   the defence   four Durandal main turrets and two Egide shield generators
//                 round the pad (Sites): the turrets shoot the nearest of the
//                 legion; a generator's dome halves the harm to its side.
//                 The legion comes for them: Tsuchigumo bombers pick one and
//                 dive (a red mark on it, a dashed red line), Ishiyumi gunships
//                 lob mortar shells at them, Tate bearers make what's near them
//                 untouchable (Legion). With both generators down a hit on the
//                 pad sets the countdown back;
//   the launch    at zero the rocket (Mission 14's) lights and climbs; the sun
//                 is upgraded (amaterasu.c AmaUpgrade);
//   the evacuation six Arche lifters loading on the aprons lift one by one,
//                 the legion heavier and going for them too; the Bastion's
//                 broadside from Le Havre, the Valhalla's pass;
//   the carrier   flagship.c's Kagutsuchi, then the rout and the last lines.
//
// The countdown and the evacuation are on the screen (Hud): "T-" and the
// seconds, then six lifter icons. Every structure has its pips under it.
//
// Run once a frame from the satellites' update (missile.c). Same rules as
// missile.c: no libc, no division, engine reached by address.

#define PARIS_LINE_TABLE
#include "andaman.h"
#include "paris_lines.h"
#include "paris.h"
#include "amaterasu.h"

#define PA_LoadBadGuy       FN(void (*)(u32, const char*), 0xbf324)     // ARM
#define PA_LoadGeneric      FN(void (*)(u32, const char*), 0xbf140)     // ARM
#define PA_GetFrameCount    FN(u32 (*)(void), 0x72434)                  // ARM
#define PA_EnableEntity     FN(void (*)(u32), 0x4f0a1)                  // the entity drawn again
#define PA_DisableEntity    FN(void (*)(u32), 0x4f089)                  // ... not drawn
#define PA_CurrentSprite    FN(u32 (*)(u32), 0xbb3b8)                   // ARM: a generic's sprite this frame

extern u32 g_late_lo, g_late_hi;    // abyss.c: sprites drawn after the generics' pass

static const char* const kBadguys[] = {
  "*paris/scripts/kamikiri_l",
  "*paris/scripts/kamikiri_r",
  "*paris/scripts/kamikiri_drill",
  "*paris/scripts/tsuchigumo",
  "*paris/scripts/tsuchigumo_run",
  "*paris/scripts/ishiyumi",
  "*paris/scripts/ishiyumi_run",
  "*paris/scripts/tate",
  "*paris/scripts/oni",
  "*paris/boss/kagu_gun",
  "*paris/boss/kagu_bay",
  "*paris/boss/kagu_keep",
  "*paris/boss/kagu_top",
  "*paris/boss/kagu_bot",
  "*paris/boss/kanabo_hull",
  "*paris/boss/kanabo_cannon",
  "*paris/boss/kanabo_gun",
  "*paris/boss/kanabo_pod",
};
static const char* const kGenerics[] = {
  "*paris/boss/arche",
  "*paris/boss/durandal",
  "*paris/boss/durandal_gun",
  "*paris/boss/egide",
  "*paris/boss/egide_dome",
  "*paris/boss/gantry",
  "*paris/boss/hud_a",
  "*paris/boss/hud_b",
  "*paris/boss/mark",
  "*paris/boss/pips",
  "*paris/boss/shell",
  "*paris/boss/tate_dome",
  "*paris/boss/tate_ward",
  "*paris/boss/breach",
  // Mission 14's: the rocket, its smoke, the mirror's light and reticle, HQ's fighters
  "*amaterasu/boss/rocket",
  "*amaterasu/boss/plume",
  "*amaterasu/boss/beam",
  "*amaterasu/boss/reticle",
  "*amaterasu/boss/closing",
  "*amaterasu/boss/einherjar",
  "*amaterasu/boss/fenrir",
  // Mission 9's: the Valhalla, shells, blasts
  "*andaman/boss/bturret",
  "*andaman/boss/ciws",
  "*andaman/boss/blast",
  "*andaman/boss/tracer",
  "*andaman/boss/lance",
  "*andaman/boss/crosshair",
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

void RegisterParis(void) {
  for (u32 i = 0; i < COUNT(kGenerics); i++) PA_LoadGeneric(BH, kGenerics[i]);
  for (u32 i = 0; i < COUNT(kBadguys); i++) PA_LoadBadGuy(BH, kBadguys[i]);
  AmaLoadHud();
}

void ForgetDefinitions(const char* const* badguys, u32 nb, const char* const* bullets, u32 nu,
                       const char* const* generics, u32 ng);
void ForgetParis(void) { ForgetDefinitions(kBadguys, COUNT(kBadguys), 0, 0, kGenerics, COUNT(kGenerics)); }

// ---- tuning -------------------------------------------------------------------------------------
#define COUNTDOWN      6000          // frames on the clock when the defence starts (100 s)
#define PAD_SETBACK    300           // a hit on the open pad puts this back on it
#define HP_TURRET      1000
#define HP_GEN         1300
#define HP_LIFTER      600
#define BOMB_DMG       260           // a bomber arriving
#define SHELL_DMG      110           // a mortar shell landing
#define TUR_EVERY      34            // a turret's reload
#define TUR_DMG        32
#define TUR_RANGE      250
#define NBOMB          10
#define NDASH          4             // the dashes of a bomber's aiming line
#define NSIEGE         4
#define SIEGE_EVERY    190
#define NSHELL         8
#define SHELL_T        72            // a shell's flight
#define NTATE          3
#define TATE_R         62            // what a bearer covers: inside its ring (tate_dome, 132 across)
#define NWARD          10            // the ward shown on each craft it covers
#define NWIPE          64            // the Valhalla's targets in one pass
#define WIPE_EVERY     4             // ... a new one every this many frames: her kills spread out, so
                                     // their explosions don't pile up (stacked, they were 7-10 screens of
                                     // fill a frame and the phone's GPU fell behind: 2026-10-27)
#define VAL_GUNS_Y     FX(476)       // her centre this high or above: her guns are on the screen
#define EVAC_EVERY     600           // a lifter lifts every so often
#define FILL_EVERY     230           // the code's own waves while a gate is held
#define STOP_ROWS      FX(3)         // the code takes the scroll this far from the stop
#define AHEAD_ROWS     FX(1.7)       // ... forward of the stop for the Kanabo's fight
#define AHEAD_STEP     FX(0.009)     // ... eased there and back at this a frame (~3 s)
#define IGNITE         100           // the rocket burns on the pad this long, then climbs
#define ROCKET_DY      (-44)
#define END_QUIET      120

#define MK_SWARM   90
#define MK_BOMBER  91
#define MK_RUN     92
#define MK_SIEGE   93
#define MK_TATE    94
#define MK_ONI     95
#define MK_KAGU    96

enum { PH_FLY, PH_DEFEND, PH_LAUNCH, PH_EVAC, PH_BOSS, PH_END };
enum { MK_RING, MK_DOT, MK_DASH };                // the mark's animations

typedef struct {
  int kind, dead, flash, cool, lift_t, gone, up;   // up: its pieces spawned
  s32 hp, max;
  u32 a, gun, pips, dome;
  s32 x, y;                                        // where it is this frame (16.16 playfield)
  s32 ly, lv;                                      // a lifter in the air: its own y and speed
} Site;
typedef struct { u32 bg, mark, dash[NDASH]; int site, t; s32 x, y, v; } Bomber;
typedef struct { u32 bg; int cool, anim_t; } Siege;
typedef struct { u32 a, mark; int site, t; s32 x0, y0; } Shell;
typedef struct { u32 bg, dome; } Tate;
typedef struct { u32 a; s32 x, y, vx; int t; } Puff;

#define NPUFF 16

static struct {
  int on, phase, said;
  u32 ticked;
  int floor_on;                                    // the floor layer drawn (Floor)
  int taken, stopped, ahead;                       // the code has the scroll; it's at rest; eased forward
  s32 cover[4];                                    // a boss hull's box this frame (x0, y0, x1, y1) ...
  int cover_t;                                     // ... while this counts
  int cover_shape, cover_n;                        // ... or its outline (ParisCoverOutline)
  const unsigned char* cover_half;
  int hold, hold_t, gate;                          // the partition held at a gate (0: none)
  int promise_line, count, count_on, pad_hits, half_said;
  Site site[P_NSITE];
  Bomber bomb[NBOMB];
  Siege siege[NSIEGE];
  Shell shell[NSHELL];
  Tate tate[NTATE];
  int shell_next;
  // the launch
  u32 rocket, gantry;
  int lift_t;
  s32 rx, ry, rv;
  Puff puff[NPUFF];
  int puff_next;
  // the evacuation
  int evac_t, lift_next, away, lost;
  // the code's waves while held
  int fill_t, fill_n;
  // the HUD
  u32 hud_t, hud_d[2], hud_icon[P_NLIFTER];
  int hud_dv[2];
  // the Bastion's broadside, the Valhalla's pass
  int broadside_t, broadside_n;
  // the end
  int finale, end_line, end_quiet;
  int seen_tate, seen_bomb;
  u32 ward[NWARD];
  int wipe_t, wipe_n, wave_done, tank_done;
  u32 wipe_hit[NWIPE];
} P;

enum { SAID_HELLO = 1, SAID_RAID = 2, SAID_SEINE = 4, SAID_TOWER = 8, SAID_CANAL = 16, SAID_PAD = 32,
       SAID_GENDOWN = 64, SAID_TURDOWN = 128, SAID_PADHIT = 256, SAID_EVAC = 512, SAID_AWAY = 1024,
       SAID_LOST = 2048, SAID_FLYBY = 4096, SAID_LAST = 8192, SAID_BROAD = 16384 };

int ParisLine(int k) { return AnLine(k); }

static int InWorld(void) { return R32(Shogun(), SH_WORLD_NO) == PARIS && P.on; }

// ---- where the ground is ------------------------------------------------------------------------
static s32 Depth(s32 r) {
  s32 i = (r - FX(P_DEPTH_LO)) >> 13;              // eighths of a row
  if (i < 0) i = 0;
  if (i > P_DEPTH_N - 1) i = P_DEPTH_N - 1;
  return kDepth[i];
}

// Site k on the screen this frame: its props row through the camera, its x by
// the depth there, and the pan's difference (the props pan less than sprites).
static void SiteAt(int k, s32* x, s32* y) {
  *y = AnRowY(kSiteRow[k]);
  const s32 r = kSiteRow[k] - AnScroll();
  *x = FX(200) + SB_MulFP(kSiteXk[k], Depth(r)) + AnGroundDx(*y);
}

// the camera's centre across (the HUD keeps to it)
static s32 CamX(s32 dx) { return FX(200) + SB_MulFP(PlayerX() - FX(200), FX(0.3933)) + FX(dx); }

// ---- the floor: off once the map covers it --------------------------------------------------------
// The animated floor shows only in the opening, before the props reach the
// painted map's first row (art_paris_city.suburb_tile). After that the map
// hides it completely -- but the engine went on drawing it, a whole screen of
// fill under the map every frame: ~40% of the GPU's work (runtime/probes/
// paris_gpu.py), and the phone's GPU, clocked down by its governor, had no
// room left when the dialog box or the Valhalla came up (2026-10-27).
#define FLOOR_ENT      0x638             // the floor's six entities (LoadBackground)
#define MAP_COVERS     FX(75.6)          // the scroll from which the map fills the screen (MAP_ROW0 78 - 2.68 + margin)

static void Floor(void) {
  const int want = AnScroll() < MAP_COVERS;
  if (want == P.floor_on) return;
  P.floor_on = want;
  for (u32 e = FLOOR_ENT; e < FLOOR_ENT + 6; e++) {
    if (want) PA_EnableEntity(e);
    else PA_DisableEntity(e);
  }
}

// ---- the scroll: brought to rest over the base -------------------------------------------------
#define SH_SPEED       0x5c
#define SH_SPEED_TO    0x60
#define SH_SPEED_STEP  0x64

static void Scroll(void) {
  const u32 sh = Shogun();
  const s32 left = P_STOP_SCROLL - AnScroll();
  if (!P.taken && left > STOP_ROWS) return;
  P.taken = 1;
  s32 v = R32(sh, SH_SPEED);
  // decelerating so as to arrive: the speed a row's 64ths a frame; to cover `left` in ~100 frames
  s32 want = left > 0 ? SB_MulFP(left, FX(0.64)) : 0;
  if (want > FX(P_CRUISE) >> 6) want = FX(P_CRUISE) >> 6;
  if (left < FX(0.02)) want = 0;
  const s32 d = want - v;
  v += d > FX(0.02) ? FX(0.02) : (d < -FX(0.02) ? -FX(0.02) : d);
  if (v < 0) v = 0;
  R32(sh, SH_SPEED) = v;
  R32(sh, SH_SPEED_TO) = v;
  R32(sh, SH_SPEED_STEP) = 0;
  if (v == 0 && left < FX(0.05)) P.stopped = 1;
  // stopped, the scroll eased forward for the Kanabo's fight (the ground ahead
  // of the walls on the screen, the base lower down), and back after
  // (and back whatever happened to the tank: past its kill, or the launch, the view is the base's)
  if (P.tank_done || P.phase >= PH_LAUNCH) P.ahead = 0;
  if (P.stopped) {
    const s32 want_at = P_STOP_SCROLL + (P.ahead ? AHEAD_ROWS : 0);
    const s32 d = want_at - AnScroll();
    if (d > FX(0.002) || d < -FX(0.002)) AnScrollBy(d > AHEAD_STEP ? AHEAD_STEP : (d < -AHEAD_STEP ? -AHEAD_STEP : d));
  }
}

void ParisScrollAhead(int on) { P.ahead = on; }

// ---- the gates: the partition held until the story gets there --------------------------------------
static void Hold(int on) {
  if (on == P.hold) return;
  P.hold = on;
  P.hold_t = 0;
  SB_PausePartition(BH, on);
}

// ---- the structures ------------------------------------------------------------------------------
// Each line's generator (sites 1 and 2: the front line's, the inner's) covers its
// line; the inner one covers the back too (the pad, the lifters).
static int GenOf(int k) { return kSiteLine[k] == 1 ? 1 : 2; }

static int Covered(int k) {
  const int g = GenOf(k);
  return !P.site[g].dead && P.site[k].kind != SITE_GEN && P.site[k].kind != SITE_PAD;
}

static void Blast2(s32 x, s32 y) {
  AnBlast(x, y);
  Boom("ExplosionL", x, y);
}

static void Damage(int k, int dmg) {
  Site* s = &P.site[k];
  if (s->dead) return;
  if (s->kind == SITE_PAD) {                       // the pad: the countdown slips, if it's open
    if (!P.site[2].dead) {                         // the inner dome keeps it
      AnFrame(P.site[2].dome, 1);
      P.site[2].flash = 20;
      return;
    }
    if (P.phase == PH_DEFEND) {
      P.count += PAD_SETBACK;
      if (P.count > COUNTDOWN) P.count = COUNTDOWN;
      if (!(P.said & SAID_PADHIT) && (P.said & SAID_PAD) && P.tank_done) { P.said |= SAID_PADHIT; AnLine(PL_GEN_PADHIT); }
    }
    return;
  }
  if (Covered(k)) {
    dmg >>= 1;
    const int g = GenOf(k);
    AnFrame(P.site[g].dome, 1);
    P.site[g].flash = 20;
  }
  s->hp -= dmg;
  s->flash = 8;
  if (s->hp > 0) return;
  s->hp = 0;
  s->dead = 1;
  Blast2(s->x, s->y);
  SB_ShakeWorld();
  if (s->kind == SITE_TURRET) {
    AnDrop(&s->gun);
    // (each event's line only once the story's lines before it are queued: asked
    // for first, it would step over them -- docs/NEW_LEVEL.md 9)
    if (!(P.said & SAID_TURDOWN) && (P.said & SAID_PAD) && P.tank_done) { P.said |= SAID_TURDOWN; AnLine(PL_GEN_TURRETDOWN); }
  } else if (s->kind == SITE_GEN) {
    AnDrop(&s->dome);
    if (!(P.said & SAID_GENDOWN) && (P.said & SAID_PAD) && P.tank_done) { P.said |= SAID_GENDOWN; AnLine(PL_GEN_GENDOWN); }
  } else if (s->kind == SITE_LIFTER) {
    AnDrop(&s->a);
    AnDrop(&s->pips);
    P.lost++;
    // (only once the evacuation's lines before it are asked for: first, it stepped over them)
    if (!(P.said & SAID_LOST) && (P.said & SAID_FLYBY)) { P.said |= SAID_LOST; AnLine(PL_GEN_LOST); }
  }
}

static int Mark(u32 b);

// The nearest of the legion to (x, y) within `range` (not the carrier's hull: no gun hurts it).
static u32 Nearest(s32 x, s32 y, s32 range, s32* ox, s32* oy) {
  u32 best = 0;
  s32 bd = range;
  for (u32 b = SB_GetFirstBadGuy(BH); b; b = SB_GetNextBadGuy(BH, b)) {
    if (SB_GetBadGuyHealth(b) <= 0 || Mark(b) == 98) continue;
    s32 bx, by;
    SB_GetActorPosition(Act(b), &bx, &by);
    if (by < FX(0) || by > FX(420) || bx < FX(-10) || bx > FX(410)) continue;
    const s32 d = AnDist(bx - x, by - y);
    if (d >= bd) continue;
    bd = d;
    best = b;
    *ox = bx;
    *oy = by;
  }
  return best;
}

static const short kPipK[4] = { 0, 6 * 65536 / HP_GEN, 6 * 65536 / HP_TURRET, 6 * 65536 / HP_LIFTER };

static void SiteSpawn(int k) {
  Site* s = &P.site[k];
  s->kind = kSiteKind[k];
  switch (s->kind) {
  case SITE_PAD:
    P.gantry = AnSpawn("*paris/boss/gantry", s->x + FX(26), s->y + FX(ROCKET_DY));
    {
      // the rocket is drawn after the generics (abyss.c LateDraws): made with the base,
      // it rose under the defences' platforms. Its sheet's four frames, from its first.
      const u32 e = SB_SpawnGeneric(BH, "*amaterasu/boss/rocket", s->x, s->y + FX(ROCKET_DY));
      P.rocket = e ? SB_GetGenericActor(e) : 0;
      if (P.rocket) {
        SB_SetActorPosition(P.rocket, s->x, s->y + FX(ROCKET_DY));
        SB_SetActorRegister(P.rocket, REG_CMD, 0);
        AnFrame(P.rocket, 0);
        g_late_lo = PA_CurrentSprite(e);
        g_late_hi = g_late_lo + 4;
      }
    }
    break;
  case SITE_GEN:
    s->max = s->hp = HP_GEN;
    s->a = AnSpawn("*paris/boss/egide", s->x, s->y);
    s->dome = AnSpawn("*paris/boss/egide_dome", s->x, s->y - FX(10));
    break;
  case SITE_TURRET:
    s->max = s->hp = HP_TURRET;
    s->a = AnSpawn("*paris/boss/durandal", s->x, s->y);
    s->gun = AnSpawn("*paris/boss/durandal_gun", s->x, s->y);
    s->cool = Rnd(0, TUR_EVERY);
    break;
  default:
    s->max = s->hp = HP_LIFTER;
    s->a = AnSpawn("*paris/boss/arche", s->x, s->y);
    break;
  }
  if (s->kind != SITE_PAD) s->pips = AnSpawn("*paris/boss/pips", s->x, s->y);
}

// ---- under a boss ----------------------------------------------------------------------------------
// The engine draws every generic over every badguy, and the structures are
// generics, the bosses' hulls badguys: a turret's base showed through the
// Kanabo and the Kagutsuchi (2026-10-26). So whatever a hull is over is hidden
// (its "hidden" animation, a clear frame): the boss is above the base, it hides it.
enum { HID_TURRET = 3, HID_GUN = 2, HID_GEN = 3, HID_DOME = 2, HID_GANTRY = 1, HID_LIFTER = 3, HID_PIPS = 7 };

void ParisCover(s32 x0, s32 y0, s32 x1, s32 y1) {
  P.cover[0] = x0;
  P.cover[1] = y0;
  P.cover[2] = x1;
  P.cover[3] = y1;
  P.cover_t = 3;
  P.cover_shape = 0;
}

// A hull by its outline: half-widths every 8 px down from its top (cy0) about cx.
// Only what the hull itself is over hides: a box hid the outer turrets beside the
// Kagutsuchi's narrowing bow, and they went on firing unseen (2026-10-27).
void ParisCoverOutline(s32 cx, s32 cy0, const unsigned char* half, int n) {
  P.cover[0] = cx;
  P.cover[1] = cy0;
  P.cover_half = half;
  P.cover_n = n;
  P.cover_t = 3;
  P.cover_shape = 1;
}

static int Under(s32 x, s32 y, int hw, int hh) {
  if (P.cover_t <= 0) return 0;
  if (P.cover_shape) {                             // the piece's centre inside the outline
    const s32 dy = (y - P.cover[1]) >> 16;
    if (dy < 0 || dy >= P.cover_n * 8) return 0;
    const s32 dx = (x > P.cover[0] ? x - P.cover[0] : P.cover[0] - x) >> 16;
    return dx < P.cover_half[dy >> 3];
  }
  return x + FX(hw) > P.cover[0] && x - FX(hw) < P.cover[2] && y + FX(hh) > P.cover[1] && y - FX(hh) < P.cover[3];
}

static void Hide(u32 a, s32 x, s32 y, int hw, int hh, int anim) {
  if (a && Under(x, y, hw, hh)) AnFrame(a, anim);
}

static void HideSite(const Site* s) {
  if (s->kind == SITE_PAD) {
    Hide(P.gantry, s->x + FX(26), s->y + FX(ROCKET_DY + 6), 15, 58, HID_GANTRY);
    return;
  }
  const int hid = s->kind == SITE_TURRET ? HID_TURRET : s->kind == SITE_GEN ? HID_GEN : HID_LIFTER;
  Hide(s->a, s->x, s->y, 25, 26, hid);
  Hide(s->gun, s->x, s->y - FX(4), 30, 30, HID_GUN);
  Hide(s->dome, s->x, s->y - FX(10), 125, 35, HID_DOME);
  Hide(s->pips, s->x, s->y + FX(28), 17, 3, HID_PIPS);
}

static void Sites(void) {
  if (P.cover_t > 0) P.cover_t--;
  for (int k = 0; k < P_NSITE; k++) {
    Site* s = &P.site[k];
    if (s->gone) continue;
    s32 x, y;
    SiteAt(k, &x, &y);
    if (s->kind == SITE_LIFTER && s->lift_t) {      // in the air: its own way up
      x = s->x;
      y = s->ly;
    }
    s->x = x;
    s->y = y;
    if (!s->up && y > FX(-90)) {
      s->up = 1;
      SiteSpawn(k);
    }
    if (s->flash > 0) s->flash--;
    switch (s->kind) {
    case SITE_PAD:
      if (P.gantry) {
        AnPut(P.gantry, x + FX(26), y + FX(ROCKET_DY + 6));
        AnFrame(P.gantry, 0);
      }
      break;
    case SITE_GEN:
      AnPut(s->a, x, y);
      AnFrame(s->a, s->dead ? 2 : s->hp * 2 < s->max ? 1 : 0);
      if (s->dome) {
        AnPut(s->dome, x, y - FX(10));
        if (!s->flash) AnFrame(s->dome, 0);
      }
      break;
    case SITE_TURRET:
      AnPut(s->a, x, y);
      AnFrame(s->a, s->dead ? 2 : s->hp * 2 < s->max ? 1 : 0);
      if (s->gun) {
        AnPut(s->gun, x, y - FX(4));
        // its guns on the nearest of the legion
        if (P.phase >= PH_DEFEND && --s->cool <= 0) {
          s32 ex, ey;
          const u32 e = Nearest(x, y, TUR_RANGE, &ex, &ey);
          if (e) {
            AnTurn(s->gun, AnAngle(x, y, ex, ey));
            AnShoot(x + SB_MulFP(SB_Cos(AnAngle(x, y, ex, ey)), FX(26)),
                    y + SB_MulFP(SB_Sin(AnAngle(x, y, ex, ey)), FX(26)), ex, ey, 9, SHOT_SMALL, TAG_NONE, e, TUR_DMG);
            AnFrame(s->gun, 1);
            s->cool = TUR_EVERY;
          } else {
            s->cool = 10;
          }
        }
        if (s->cool < TUR_EVERY - 6) AnFrame(s->gun, 0);
      }
      break;
    default:                                       // a lifter
      if (!s->a) break;
      AnPut(s->a, x, y);
      if (s->lift_t) {
        AnFrame(s->a, 2);
        s->lift_t++;
        if (s->lv < FX(2.4)) s->lv += FX(0.02);
        s->ly -= s->lv;
        if (s->ly < FX(-140)) {
          AnDrop(&s->a);
          AnDrop(&s->pips);
          s->gone = 1;
          P.away++;
          if (!(P.said & SAID_AWAY)) { P.said |= SAID_AWAY; AnLine(PL_GEN_AWAY); }
        }
      } else {
        AnFrame(s->a, P.phase >= PH_EVAC ? 1 : 0);
      }
      break;
    }
    if (s->pips) {
      AnPut(s->pips, x, y + FX(s->kind == SITE_LIFTER ? 26 : 28));
      s32 n = (s->hp * kPipK[s->kind] + 65535) >> 16;
      if (n > 6) n = 6;
      AnFrame(s->pips, s->dead ? 0 : n);
      if (s->dead && s->kind != SITE_LIFTER) AnDrop(&s->pips);
    }
    HideSite(s);
  }
}

// A structure for the legion to go for: the front line while any of it
// stands, then the inner line, then what's behind -- the pad (in the
// defence), the lifters on the ground (once they're loading; now and then
// they're gone for over the lines, too).
static int Open(int k) {
  const Site* s = &P.site[k];
  if (s->dead || s->gone || !s->up) return 0;
  if (s->kind == SITE_LIFTER) return P.phase >= PH_EVAC && !s->lift_t;
  if (s->kind == SITE_PAD) return P.phase == PH_DEFEND;
  return 1;
}

static int PickSite(void) {
  int pick[P_NSITE], n = 0;
  if (P.phase >= PH_EVAC && Rnd(0, 4) == 0)
    for (int k = 0; k < P_NSITE; k++)
      if (kSiteKind[k] == SITE_LIFTER && Open(k)) pick[n++] = k;
  for (int line = 1; line <= 3 && n == 0; line++)
    for (int k = 0; k < P_NSITE; k++)
      if (kSiteLine[k] == line && Open(k)) pick[n++] = k;
  if (n == 0) return -1;
  int i = Rnd(0, n);
  if (i >= n) i = n - 1;
  return pick[i];
}

static int Targetable(int k) {
  if (k < 0) return 0;
  const Site* s = &P.site[k];
  return s->up && !s->dead && !s->gone && !(s->kind == SITE_LIFTER && s->lift_t);
}

// ---- the legion ----------------------------------------------------------------------------------
static int Mark(u32 b) { return (int)(GetReg(b, 21) >> 16); }

static void BombDrop(Bomber* m) {
  AnDrop(&m->mark);
  for (int i = 0; i < NDASH; i++) AnDrop(&m->dash[i]);
  m->bg = 0;
}

static Bomber* BombFind(u32 b) {
  for (int i = 0; i < NBOMB; i++)
    if (P.bomb[i].bg == b) return &P.bomb[i];
  return 0;
}

static void Bombers(void) {
  // the new ones: taken over
  for (u32 b = SB_GetFirstBadGuy(BH); b; b = SB_GetNextBadGuy(BH, b)) {
    if (SB_GetBadGuyHealth(b) <= 0 || Mark(b) != MK_BOMBER || BombFind(b)) continue;
    if (P.phase < PH_DEFEND || P.phase >= PH_BOSS + 1) continue;
    for (int i = 0; i < NBOMB; i++) {
      Bomber* m = &P.bomb[i];
      if (m->bg) continue;
      SB_ClearBytes(m, sizeof *m);
      m->bg = b;
      m->site = PickSite();
      SB_GetActorPosition(Act(b), &m->x, &m->y);
      m->v = FX(1.2);
      break;
    }
  }
  for (int i = 0; i < NBOMB; i++) {
    Bomber* m = &P.bomb[i];
    if (!m->bg) continue;
    if (!Alive(m->bg) || Mark(m->bg) != MK_BOMBER) {
      BombDrop(m);
      continue;
    }
    if (!Targetable(m->site)) m->site = PickSite();
    if (m->site < 0) {                            // nothing left to go for: it just hovers
      Place(m->bg, m->x, m->y);
      continue;
    }
    m->t++;
    const Site* s = &P.site[m->site];
    // in to a point over its target, then the dive
    const int diving = m->t > 90;
    const s32 tx = s->x, ty = diving ? s->y : (s->y - FX(150) < FX(40) ? FX(40) : s->y - FX(150));
    const s32 a = AnAngle(m->x, m->y, tx, ty);
    if (diving) {
      if (m->v < FX(3.4)) m->v += FX(0.035);
      if (!P.seen_bomb && !(P.said & SAID_PAD)) { P.seen_bomb = 1; AnLine(PL_GEN_BOMBER); }
    } else if (m->v > FX(1.4)) {
      m->v -= FX(0.02);
    }
    const s32 d = AnDist(tx - m->x, ty - m->y);
    if (!diving && d < 6) {
      m->v = FX(0.2);
    } else {
      m->x += SB_MulFP(SB_Cos(a), m->v);
      m->y += SB_MulFP(SB_Sin(a), m->v);
    }
    Place(m->bg, m->x, m->y);
    Anim(m->bg, diving ? 1 : 0);
    // the aim: a red mark on the structure, a dashed line to it
    if (!m->mark) m->mark = AnSpawn("*paris/boss/mark", tx, s->y);
    AnPut(m->mark, s->x, s->y);
    AnFrame(m->mark, diving ? MK_DOT : MK_RING);
    if (diving) {
      for (int j = 0; j < NDASH; j++) {
        if (!m->dash[j]) m->dash[j] = AnSpawn("*paris/boss/mark", m->x, m->y);
        const s32 f = (j + 1) * 13107;            // a fifth of the way each (NDASH + 1 = 5)
        AnPut(m->dash[j], m->x + SB_MulFP(s->x - m->x, f), m->y + SB_MulFP(s->y - m->y, f));
        AnTurn(m->dash[j], a);
        AnFrame(m->dash[j], MK_DASH);
      }
    }
    if (diving && d < 12) {                       // it's home: the bomb load goes off on it
      const int k = m->site;
      const s32 bx = m->x, by = m->y;
      const u32 bg = m->bg;
      BombDrop(m);
      SB_SetInvincibility(bg, -1);
      SB_StrikeBadGuy(BH, bg, 0x7fff);
      Blast2(bx, by);
      Damage(k, BOMB_DMG);
    }
  }
}

// ---- the siege gunships' shells -------------------------------------------------------------------
static void Sieges(void) {
  for (u32 b = SB_GetFirstBadGuy(BH); b; b = SB_GetNextBadGuy(BH, b)) {
    if (SB_GetBadGuyHealth(b) <= 0 || Mark(b) != MK_SIEGE) continue;
    int have = 0;
    for (int i = 0; i < NSIEGE; i++) have |= P.siege[i].bg == b;
    if (have) continue;
    for (int i = 0; i < NSIEGE; i++) {
      if (P.siege[i].bg && Alive(P.siege[i].bg) && Mark(P.siege[i].bg) == MK_SIEGE) continue;
      P.siege[i].bg = b;
      P.siege[i].cool = 120 + Rnd(0, 60);
      P.siege[i].anim_t = 0;
      break;
    }
  }
  for (int i = 0; i < NSIEGE; i++) {
    Siege* g = &P.siege[i];
    if (!g->bg) continue;
    if (!Alive(g->bg) || Mark(g->bg) != MK_SIEGE) {
      g->bg = 0;
      continue;
    }
    if (g->anim_t > 0 && --g->anim_t == 0) Anim(g->bg, 0);
    if (P.phase < PH_DEFEND || --g->cool > 0) continue;
    g->cool = SIEGE_EVERY + Rnd(0, 60);
    const int k = PickSite();
    if (k < 0) continue;
    for (int j = 0; j < NSHELL; j++) {
      Shell* sh = &P.shell[j];
      if (sh->a) continue;
      SB_GetActorPosition(Act(g->bg), &sh->x0, &sh->y0);
      sh->y0 += FX(12);
      sh->site = k;
      sh->t = 0;
      sh->a = AnSpawn("*paris/boss/shell", sh->x0, sh->y0);
      sh->mark = AnSpawn("*paris/boss/mark", P.site[k].x, P.site[k].y);
      Anim(g->bg, 1);
      g->anim_t = 18;
      break;
    }
  }
  // the shells in flight: up in an arc and down on the mark
  for (int j = 0; j < NSHELL; j++) {
    Shell* sh = &P.shell[j];
    if (!sh->a) continue;
    sh->t++;
    const Site* s = &P.site[sh->site];
    const s32 f = sh->t * 910;                    // t / SHELL_T (72), 16.16: no division in the blob
    const s32 arc = SB_MulFP(FX(70), SB_Sin(SB_MulFP(f, FX(128))));   // half a turn over the flight
    AnPut(sh->a, sh->x0 + SB_MulFP(s->x - sh->x0, f), sh->y0 + SB_MulFP(s->y - sh->y0, f) - arc);
    AnPut(sh->mark, s->x, s->y);
    AnFrame(sh->mark, sh->t * 2 < SHELL_T ? MK_RING : MK_DOT);
    if (sh->t >= SHELL_T || !Targetable(sh->site)) {
      if (Targetable(sh->site)) {
        Blast2(s->x, s->y);
        Damage(sh->site, SHELL_DMG);
      }
      AnDrop(&sh->a);
      AnDrop(&sh->mark);
    }
  }
}

// ---- the shield bearers ---------------------------------------------------------------------------
static void Tates(void) {
  int nward = 0;
  for (u32 b = SB_GetFirstBadGuy(BH); b; b = SB_GetNextBadGuy(BH, b)) {
    if (SB_GetBadGuyHealth(b) <= 0 || Mark(b) != MK_TATE) continue;
    int have = 0;
    for (int i = 0; i < NTATE; i++) have |= P.tate[i].bg == b;
    if (have) continue;
    for (int i = 0; i < NTATE; i++) {
      if (P.tate[i].bg) continue;
      P.tate[i].bg = b;
      if (!P.seen_tate) { P.seen_tate = 1; AnLine(PL_DOC_TATE); }    // (the first is in the flyover)
      break;
    }
  }
  for (int i = 0; i < NTATE; i++) {
    Tate* t = &P.tate[i];
    if (!t->bg) continue;
    if (!Alive(t->bg) || Mark(t->bg) != MK_TATE) {
      t->bg = 0;
      AnDrop(&t->dome);
      continue;
    }
    s32 tx, ty;
    SB_GetActorPosition(Act(t->bg), &tx, &ty);
    if (!t->dome) t->dome = AnSpawn("*paris/boss/tate_dome", tx, ty);
    AnPut(t->dome, tx, ty);
    // everything of the legion inside its ring: untouchable (a short countdown,
    // kept up), and wearing a gold ward that says so -- unmarked, two gunships
    // that shrugged off every shot read as a bug (2026-10-26)
    for (u32 b = SB_GetFirstBadGuy(BH); b; b = SB_GetNextBadGuy(BH, b)) {
      if (b == t->bg || SB_GetBadGuyHealth(b) <= 0) continue;
      const int mk = Mark(b);
      if (mk < MK_SWARM || mk > MK_ONI || mk == MK_TATE) continue;
      s32 x, y;
      SB_GetActorPosition(Act(b), &x, &y);
      if (AnDist(x - tx, y - ty) >= TATE_R) continue;
      SB_SetInvincibility(b, 24);
      if (nward < NWARD) {
        if (!P.ward[nward]) P.ward[nward] = AnSpawn("*paris/boss/tate_ward", x, y);
        AnPut(P.ward[nward], x, y);
        nward++;
      }
    }
  }
  for (int i = nward; i < NWARD; i++) AnDrop(&P.ward[i]);
}

static u32 FindMark(int mk, s32* x, s32* y) {
  for (u32 b = SB_GetFirstBadGuy(BH); b; b = SB_GetNextBadGuy(BH, b))
    if (SB_GetBadGuyHealth(b) > 0 && Mark(b) == mk) {
      SB_GetActorPosition(Act(b), x, y);
      if (*y > FX(0)) return b;
    }
  return 0;
}

// ---- the code's own waves while a gate holds the partition --------------------------------------
static void Fillers(void) {
  if (!P.hold || P.gate == 1 || ++P.fill_t < FILL_EVERY) return;
  P.fill_t = 0;
  P.fill_n++;
  SB_SpawnBadGuy(BH, "*paris/scripts/tsuchigumo", FX(Rnd(60, 341)), FX(-30));
  if (P.phase >= PH_EVAC || (P.fill_n & 1)) SB_SpawnBadGuy(BH, "*paris/scripts/tsuchigumo", FX(Rnd(60, 341)), FX(-30));
  if ((P.fill_n & 3) == 2) SB_SpawnBadGuy(BH, "*paris/scripts/ishiyumi", FX(Rnd(100, 301)), FX(-30));
  if ((P.fill_n & 3) == 0) SB_SpawnBadGuy(BH, "*paris/scripts/kamikiri_drill", FX(Rnd(100, 301)), FX(-30));
}

// ---- the launch ------------------------------------------------------------------------------------
static void PuffAt(s32 x, s32 y, s32 vx) {
  Puff* p = &P.puff[P.puff_next];
  if (++P.puff_next >= NPUFF) P.puff_next = 0;
  AnDrop(&p->a);
  p->a = AnSpawn("*amaterasu/boss/plume", x, y);
  p->x = x;
  p->y = y;
  p->vx = vx;
  p->t = 0;
  AnFrame(p->a, 0);
}

static void Puffs(void) {
  for (int k = 0; k < NPUFF; k++) {
    Puff* p = &P.puff[k];
    if (!p->a) continue;
    p->t++;
    p->x += p->vx;
    p->y -= FX(0.15);
    AnPut(p->a, p->x, p->y);
    if (p->t == 46) AnFrame(p->a, 1);
    if (p->t > 120) AnDrop(&p->a);
  }
}

static void Rocket(void) {
  const Site* pad = &P.site[0];
  if (!P.rocket) return;
  if (!P.lift_t) {
    AnPut(P.rocket, pad->x, pad->y + FX(ROCKET_DY));
    return;
  }
  P.lift_t++;
  if (P.lift_t < IGNITE) {
    if ((P.lift_t & 7) == 0)
      PuffAt(P.rx + FX(Rnd(-34, 35)), pad->y + FX(Rnd(-6, 10)), (P.lift_t & 8) ? FX(0.5) : FX(-0.5));
    AnPut(P.rocket, P.rx + FX((P.lift_t & 2) ? 1 : 0) - FX(0.5), P.ry);
    return;
  }
  if (P.lift_t == IGNITE) AnLine(PL_VOLTA_LIFT);
  if (P.rv < FX(3.2)) P.rv += FX(0.018);
  P.ry -= P.rv;
  AnPut(P.rocket, P.rx, P.ry);
  if ((P.lift_t & 3) == 0) PuffAt(P.rx + FX(Rnd(-4, 5)), P.ry + FX(66), FX(Rnd(-20, 21)) >> 6);
  if (P.ry < FX(-180)) {
    AnDrop(&P.rocket);
    g_late_lo = g_late_hi = 0;
    // in orbit: the ring is whole, and the sun changes
    AmaUpgrade(1);
    AnLine(PL_DOC_WHOLE);
    AnLine(PL_DOC_UPGRADE);
    P.phase = PH_EVAC;
    P.evac_t = 0;
    P.lift_next = P_LIFTER0;
    if (!(P.said & SAID_EVAC)) { P.said |= SAID_EVAC; AnLine(PL_GEN_EVAC); }
  }
}

static void Ignite(void) {
  if (P.lift_t || !P.rocket) return;
  P.lift_t = 1;
  P.rx = P.site[0].x;
  P.ry = P.site[0].y + FX(ROCKET_DY);
  AnFrame(P.rocket, 1);
  AnLine(PL_GEN_LAUNCH);
}

// ---- the evacuation ---------------------------------------------------------------------------------
static void Evac(void) {
  if (P.phase != PH_EVAC) return;
  P.evac_t++;
  // the Bastion's broadside, from off Le Havre: shells up from the bottom of the screen
  if (P.evac_t == 900) { P.broadside_t = 1; AnLine(PL_ARGUS_BROADSIDE); }
  if (P.broadside_t && ++P.broadside_t > 120 && P.broadside_n < 14 && (P.broadside_t & 7) == 0) {
    s32 ex, ey;
    const u32 e = Nearest(FX(Rnd(60, 341)), FX(Rnd(40, 240)), 400, &ex, &ey);
    if (e) AnShoot(FX(Rnd(60, 341)), FX(470), ex, ey, 8, SHOT_BIG, TAG_NONE, e, 500);
    P.broadside_n++;
  }
  // the Valhalla's pass, her railgun on the worst of them, every other gun clearing the screen
  if (P.evac_t == 1700) {
    s32 x, y;
    if (FindMark(MK_ONI, &x, &y)) AnFlybyAt(x, y);
    else AnFlyby();
    ParisValWipe(520);
    AnLine(PL_VOLTA_FLYBY);
    P.said |= SAID_FLYBY;
  }
  // the lifters in turn
  if (P.evac_t > 300 && P.evac_t >= (EVAC_EVERY * (P.lift_next - P_LIFTER0 + 1))) {
    while (P.lift_next < P_NSITE && (P.site[P.lift_next].dead || P.site[P.lift_next].gone || !P.site[P.lift_next].a))
      P.lift_next++;
    if (P.lift_next < P_NSITE) {
      Site* s = &P.site[P.lift_next];
      s->lift_t = 1;
      s->ly = s->y;
      s->lv = 0;
      P.lift_next++;
    }
  }
  if (P.away + P.lost >= P_NLIFTER && !(P.said & SAID_LAST)) {
    P.said |= SAID_LAST;
    AnLine(PL_GEN_LAST);
  }
}

// ---- the HUD: the countdown, then the lifters --------------------------------------------------------
#define HUD_Y  34

static void Digit(int i, int d, s32 x) {
  const int grp = d >= 8;
  if (P.hud_d[i] && P.hud_dv[i] != grp) AnDrop(&P.hud_d[i]);
  if (!P.hud_d[i]) {
    P.hud_d[i] = AnSpawn(grp ? "*paris/boss/hud_b" : "*paris/boss/hud_a", x, FX(HUD_Y));
    P.hud_dv[i] = grp;
  }
  AnPut(P.hud_d[i], x, FX(HUD_Y));
  AnFrame(P.hud_d[i], grp ? d - 8 : d);
}

static void Hud(void) {
  const int counting = P.phase == PH_DEFEND && P.count_on;
  if (counting) {
    const int s = (P.count * 1093) >> 16;                 // seconds (/ 60)
    const int tens = (s * 6554) >> 16;
    const int ones = s - tens * 10;
    if (!P.hud_t) P.hud_t = AnSpawn("*paris/boss/hud_b", CamX(54), FX(HUD_Y));
    AnPut(P.hud_t, CamX(54), FX(HUD_Y));
    AnFrame(P.hud_t, 2);                                   // "T-"
    Digit(0, tens, CamX(76));
    Digit(1, ones, CamX(96));
  } else {
    AnDrop(&P.hud_t);
    AnDrop(&P.hud_d[0]);
    AnDrop(&P.hud_d[1]);
  }
  const int icons = P.phase == PH_EVAC || P.phase == PH_BOSS;
  for (int i = 0; i < P_NLIFTER; i++) {
    if (!icons) {
      AnDrop(&P.hud_icon[i]);
      continue;
    }
    const s32 x = CamX(26 + 22 * i);
    if (!P.hud_icon[i]) P.hud_icon[i] = AnSpawn("*paris/boss/hud_b", x, FX(HUD_Y));
    AnPut(P.hud_icon[i], x, FX(HUD_Y));
    const Site* s = &P.site[P_LIFTER0 + i];
    AnFrame(P.hud_icon[i], s->dead ? 5 : (s->gone || s->lift_t) ? 4 : 3);
  }
}

// ---- the story by the clock and by the gates ----------------------------------------------------------
// The flyover's lines go by where the map is (the real scroll), each when its
// sight is on the screen, spaced out (2026-10-26: back to back, they crowded).
static void Story(s32 pt) {
  const s32 sc = AnScroll();
  if (!(P.said & SAID_HELLO) && pt >= P_GENERAL_T) { P.said |= SAID_HELLO; AnLine(PL_GEN_HELLO); }
  if (!(P.said & SAID_TOWER) && sc >= P_ROW_TOWER) { P.said |= SAID_TOWER; AnLine(PL_VOLTA_TOWER); }
  if (!(P.said & SAID_RAID) && sc >= P_ROW_RAID) {
    P.said |= SAID_RAID;
    AnLine(PL_DOC_RAID);
    AnLine(PL_GEN_DOUBT);
  }
  // the great wave (paris_timeline.py): a screenful of the wing, and the Valhalla through it
  // (her line first: she calls it as the wave comes in, then comes through it)
  if (!(P.said & SAID_SEINE) && pt >= P_WAVE_T - 120) { P.said |= SAID_SEINE; AnLine(PL_VOLTA_WAVE); }
  if (!P.wave_done && pt >= P_WAVE_T + 200 && P.phase == PH_FLY) {
    P.wave_done = 1;
    AnFlyby();
    ParisValWipe(620);
  }
}

// ---- the Valhalla: a fighting force --------------------------------------------------------------
// While she passes (or holds station), every gun she has works the screen:
// each craft of the legion ahead of her (up the screen from her bow) gets a
// heavy shell, a few new ones a frame, till nothing of the wing is left in
// sight -- the shield bearers' wards stripped first. (Not the bosses' hulls or
// parts: their fights are theirs.)
void ParisValWipe(int frames) {
  P.wipe_t = frames;
  P.wipe_n = 0;
}

static void ValWipe(void) {
  if (P.wipe_t <= 0) return;
  P.wipe_t--;
  if (AnValGone()) return;
  s32 vx, vy;
  AnValPos(&vx, &vy);
  // her guns open only once she is on the screen: her centre from below
  // (her station, 470, shows her bow and the railgun; passing, she comes up
  // from 640) -- they opened as she came in, and the wing blew up before
  // anyone could see who shot it (2026-10-26)
  if (vy > VAL_GUNS_Y) return;
  if (P.wipe_t & (WIPE_EVERY - 1)) return;
  int shots = 0;
  for (int pass = 0; pass < 2 && shots < 1; pass++)          // the bearers first, then the rest
    for (u32 b = SB_GetFirstBadGuy(BH); b && shots < 1; b = SB_GetNextBadGuy(BH, b)) {
      if (SB_GetBadGuyHealth(b) <= 0) continue;
      const int mk = Mark(b);
      if (mk < MK_SWARM || mk > MK_ONI || (pass == 0) != (mk == MK_TATE)) continue;
      s32 x, y;
      SB_GetActorPosition(Act(b), &x, &y);
      if (y < FX(-10) || y > FX(400) || x < FX(-20) || x > FX(420) || y > vy - FX(40)) continue;
      int done = 0;
      for (int i = 0; i < P.wipe_n; i++) done |= P.wipe_hit[i] == b;
      if (done || P.wipe_n >= NWIPE) continue;
      P.wipe_hit[P.wipe_n++] = b;
      SB_SetInvincibility(b, -1);
      // from one of her turrets that's on the screen (andaman.c's own guns'
      // places), shells you can watch go
      static const short kTx[6] = { VAL_TUR0_X, VAL_TUR1_X, VAL_TUR2_X, VAL_TUR3_X, VAL_TUR4_X, VAL_TUR5_X };
      static const short kTy[6] = { VAL_TUR0_Y, VAL_TUR1_Y, VAL_TUR2_Y, VAL_TUR3_Y, VAL_TUR4_Y, VAL_TUR5_Y };
      int g = -1;
      for (int i = 0; i < 6; i++) {
        const int k = (P.wipe_n + i) & 7;
        if (k < 6 && vy + FX(kTy[k]) < FX(430)) { g = k; break; }
      }
      if (g < 0) { P.wipe_n--; break; }
      // (the small impact: the craft's own explosion is the show -- a big shell's blast on top of it
      // doubled the fill for nothing)
      AnShoot(vx + FX(kTx[g]), vy + FX(kTy[g]), x, y, 9, SHOT_SMALL, TAG_NONE, b, 30000);
      shots++;
    }
}

// ---- what the Kanabo (tank.c) asks of the base ------------------------------------------------------
// The site its breaching cannon goes for: the front line's generator while it
// stands, then the inner one's, then a turret; -1 if nothing's left.
int ParisBreachSite(void) {
  if (P.site[1].up && !P.site[1].dead) return 1;
  if (P.site[2].up && !P.site[2].dead) return 2;
  for (int k = 0; k < P_NSITE; k++)
    if (kSiteKind[k] == SITE_TURRET && P.site[k].up && !P.site[k].dead) return k;
  return -1;
}

void ParisSitePos(int k, s32* x, s32* y) {
  *x = P.site[k].x;
  *y = P.site[k].y;
}

void ParisSiteHit(int k, int dmg) {
  if (k < 0 || k >= P_NSITE) return;
  Blast2(P.site[k].x, P.site[k].y);
  SB_ShakeWorld();
  P.site[k].flash = 0;
  P.site[k].hp -= dmg - (dmg >> 2);         // (the dome's halving doesn't hold a breach: most of it goes through)
  Damage(k, dmg >> 2);
}

void ParisTankDone(void) { P.tank_done = 1; }

static void Gates(s32 pt) {
  switch (P.phase) {
  case PH_FLY:
    if (P.stopped) {                               // over the base: the general's words, the defence
      P.phase = PH_DEFEND;
      P.promise_line = AnLine(PL_GEN_PROMISE);
      P.count = COUNTDOWN;
    }
    if (pt >= P_ARRIVE_T) { P.gate = 1; Hold(1); }
    break;
  case PH_DEFEND:
    if (!P.count_on && (AnLineUp(P.promise_line) || ++P.hold_t > 1200)) P.count_on = 1;
    if (P.gate == 1 && P.count_on) { P.gate = 0; Hold(0); }
    if (pt >= P_ARRIVE_T && !P.count_on && !P.hold) { P.gate = 1; Hold(1); }
    if (P.count_on) {
      if (P.count > 0) P.count--;
      if (P.count == COUNTDOWN - 600 && !(P.said & SAID_PAD)) {
        P.said |= SAID_PAD;
        AnLine(PL_GEN_PAD);
        AnLine(PL_DOC_PAD);
        AnLine(PL_DOC_LEAVE);
        AnLine(PL_GEN_STAY);
      }
      if (!P.half_said && P.count < 30 * 60) { P.half_said = 1; AnLine(PL_GEN_HALF); }
      if (P.count == 0) {
        P.phase = PH_LAUNCH;
        Ignite();
      }
    }
    if (pt >= P_LAUNCH_T) { P.gate = 2; Hold(1); }
    break;
  case PH_LAUNCH:
    if (pt >= P_LAUNCH_T) { P.gate = 2; Hold(1); }
    break;
  case PH_EVAC:
    if (P.gate == 2) { P.gate = 0; Hold(0); }
    if (pt >= P_EVAC_END_T && !(P.said & SAID_LAST)) { P.gate = 3; Hold(1); }
    if (P.gate == 3 && (P.said & SAID_LAST)) {
      P.gate = 0;
      Hold(0);
      P.phase = PH_BOSS;
    }
    if (pt >= P_EVAC_END_T && (P.said & SAID_LAST)) P.phase = PH_BOSS;
    break;
  default:
    break;
  }
}

// ---- the end: the legion breaks, the last lines -----------------------------------------------------
void ParisRout(void) {
  for (u32 b = SB_GetFirstBadGuy(BH); b; b = SB_GetNextBadGuy(BH, b)) {
    if (SB_GetBadGuyHealth(b) <= 0) continue;
    const int mk = Mark(b);
    if (mk < MK_SWARM || mk >= MK_KAGU) continue;   // (not the carrier's: flagship.c)
    s32 x, y;
    SB_GetActorPosition(Act(b), &x, &y);
    AnBlast(x, y);
    SB_SetInvincibility(b, -1);
    SB_StrikeBadGuy(BH, b, 0x7fff);
  }
  for (int i = 0; i < NBOMB; i++)
    if (P.bomb[i].bg) BombDrop(&P.bomb[i]);
  AnLine(PL_GEN_BROKEN);
}

void ParisFinale(void) {
  if (P.finale) return;
  P.finale = 1;
  P.phase = PH_END;
  AnLine(PL_GEN_FAREWELL);
  AnLine(PL_ARGUS_CONVOY);
  P.end_line = AnLine(PL_DOC_EASY);
}

int ParisFinaleDone(void) { return P.finale == 2; }

static void Finale(void) {
  if (P.finale != 1) return;
  P.end_quiet = InCutscene() ? 0 : P.end_quiet + 1;
  if (AnLineUp(P.end_line) && P.end_quiet > END_QUIET) P.finale = 2;
}

// ---- a new run, and every frame -----------------------------------------------------------------------
void AndamanReset(void);

void ParisReset(void) {
  AndamanReset();                                // the shells, blasts and the Valhalla: clean
  AnUseLines(kParisSentences, PL_COUNT);         // ... with this world's speeches
  AmaSunReset();                                 // the mirror: in orbit, not yet upgraded
  SB_ClearBytes(&P, sizeof P);
  P.on = 1;
  P.floor_on = 1;
  g_late_lo = g_late_hi = 0;
  for (u32 e = FLOOR_ENT; e < FLOOR_ENT + 6; e++) PA_EnableEntity(e);
  for (int k = 0; k < P_NSITE; k++) P.site[k].kind = kSiteKind[k];
}

void ParisTick(void) {
  if (!InWorld()) return;
  P.ticked = PA_GetFrameCount();
  const s32 pt = AnTime();
  if (P.hold) P.hold_t++;
  Floor();
  Story(pt);
  Scroll();
  Gates(pt);
  Sites();
  Rocket();
  Puffs();
  if (P.phase >= PH_DEFEND && P.phase <= PH_BOSS) {
    Bombers();
    Sieges();
  }
  Tates();
  ValWipe();
  Fillers();
  Evac();
  Hud();
  Finale();
  AmaSunTick();
}
