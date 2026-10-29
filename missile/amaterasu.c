// amaterasu.c -- Mission 14 (world 14): Amaterasu. UNITY HQ, then the South Atlantic.
//
// Home at last (docs/M14_DESIGN.md): the pilot's first look at UNITY HQ on
// the Ross Ice Shelf -- a base to look at, a buried warship really (it rises
// in a later mission; amaterasu/art_hq.py lays its footprint) -- the launch of
// Amaterasu's first focusing mirror from HQ's silo, and north to meet the
// Shogun's probe:
//
//   the homecoming  over HQ, the scroll slow: the Valhalla comes in over the
//                   stern and settles towards her hangar (out of sight), lines;
//   the launch      the scroll stops over the silo; the rocket stands on it,
//                   lights, and climbs up the screen on its smoke while the
//                   Shogun's old recon drones come in (the partition's);
//   the lesson      Amaterasu is up: the Doc's target drone over the shelf's
//                   edge, painted;
//   the run         Mission 4's waves (amaterasu/ama_timeline.py), the probe's
//                   listening buoys riding the sea (armoured:
//                   painted, not shot); HQ's Home Fleet past the bow;
//   the sea         the shelf's ice, the cold sea from HQ's bow, the
//                   Atlantic, the storm (bands of props, Mission 13's way);
//   the bosses      the gun barge (barge.c) and Lord Raijin's dragon
//                   (raijin.c), whose jam and finish go through here;
//   the ending      the Bastion refitted and two of the Home Fleet's destroyers
//                   come up from behind, Argus speaks, they sail on up the
//                   screen for France; then the Valhalla comes up, alone (on
//                   the screen together their sprites flickered over hers).
//
// Painting (the mission's mechanic): a paintable target carries a bracket;
// while the ship stays within PAINT_R of it, a ring round it fills (and
// drains slowly when the ship is away). Full, it locks: the mirror takes the
// spot -- following the target a moment, then fixed -- a hair-line comes down
// to it and a ring closes on it, then the beam: everything in its radius is
// struck, the ship too (a beam goes through the shield). One strike, then the
// mirror recharges (the small sun on the HUD beside the score, lit when ready).
//
// Run once a frame from the satellites' update (missile.c). Same rules as
// missile.c: no libc, no division, engine reached by address.

#define AMATERASU_LINE_TABLE
#include "andaman.h"
#include "amaterasu_lines.h"
#include "amaterasu.h"

#define AT_LoadBadGuy       FN(void (*)(u32, const char*), 0xbf324)     // ARM
#define AT_LoadBullet       FN(void (*)(u32, const char*), 0xbeffc)     // ARM
#define AT_LoadGeneric      FN(void (*)(u32, const char*), 0xbf140)     // ARM
#define AT_SetEntityOpacity FN(void (*)(u32, s32), 0x4f3cd)
#define AT_GetFrameCount    FN(u32 (*)(void), 0x72434)                  // ARM
#define AT_LoadSprites      FN(s32 (*)(u32, const char*, u32, u32), 0x57ea9)
#define AT_DrawSprite       FN(void (*)(u32, s32, s32, u32), 0x5f351)
#define AT_GetScreenWidth   FN(s32 (*)(void), 0x4e955)
#define AT_GetScreenHeight  FN(s32 (*)(void), 0x4e971)
#define AT_GetWidth         FN(s32 (*)(u32), 0x4d9b9)
#define AT_GetHeight        FN(s32 (*)(u32), 0x4d9f9)
#define SH_UI_SCALE         0x8b9dc     // the menus' sprite scale (worlds.c)

// Mission 4's craft, its bosses' and Ocean's drones are the game's own,
// registered at start-up; these are ours (amaterasu/ama_cast.py), and
// Mission 9's Valhalla and the Bastion's guns.
static const char* const kBadguys[] = {
  "*amaterasu/scripts/badguy01_mk2",
  "*amaterasu/scripts/badguy02_mk2",
  "*amaterasu/scripts/badguy03_mk2",
  "*amaterasu/scripts/badguy06_mk2",
  "*amaterasu/scripts/badguy08_mk2",
  "*amaterasu/scripts/badguy09_mk2",
  "*amaterasu/boss/barge",
  "*amaterasu/boss/b_tourelle_1_bd",
  "*amaterasu/boss/b_tourelle_1_bg",
  "*amaterasu/boss/b_tourelle_1_hd",
  "*amaterasu/boss/b_tourelle_1_hg",
  "*amaterasu/boss/b_tourelle_2_bd",
  "*amaterasu/boss/b_tourelle_2_bg",
  "*amaterasu/boss/b_tourelle_2_hd",
  "*amaterasu/boss/b_tourelle_2_hg",
  "*amaterasu/boss/b_tourelle_big_b",
  "*amaterasu/boss/b_tourelle_big_h",
  "*amaterasu/boss/dragon",
  "*amaterasu/boss/aile_l",
  "*amaterasu/boss/aile_r",
  "*amaterasu/boss/tete",
  "*amaterasu/boss/queue",
  "*amaterasu/boss/canon_l",
  "*amaterasu/boss/canon_r",
  "*amaterasu/boss/canon_cl",
  "*amaterasu/boss/canon_cr",
  "*amaterasu/boss/buoy",
  "*amaterasu/boss/target_drone",
};
static const char* const kBullets[] = {
  "*amaterasu/boss/b_laser_high",
  "*amaterasu/boss/b_laser_low2b",
  "*amaterasu/boss/ball_vnr",
  "*amaterasu/boss/bullet_heaven",
  "*amaterasu/boss/bullet_hell",
  "*amaterasu/boss/gomid",
  "*amaterasu/boss/laser_high2",
  "*amaterasu/boss/bullet_hell2",
  "*amaterasu/boss/bend_laser",
  "*amaterasu/boss/talon",
};
static const char* const kGenerics[] = {
  "*amaterasu/boss/aileg_l",
  "*amaterasu/boss/aileg_r",
  "*amaterasu/boss/bossg",
  "*amaterasu/boss/explode",
  "*amaterasu/boss/explosiontrail",
  "*amaterasu/boss/lastexplosion",
  "*amaterasu/boss/queueg",
  "*amaterasu/boss/superdisk2",
  "*amaterasu/boss/superdisk3",
  "*amaterasu/boss/teteg",
  "*amaterasu/boss/tortilla",
  "*amaterasu/boss/tortillon",
  "*amaterasu/boss/destroyer",
  "*amaterasu/boss/heimdall",
  "*amaterasu/boss/einherjar",
  "*amaterasu/boss/fenrir",
  "*amaterasu/boss/sleipnir",
  "*amaterasu/boss/bastion14",
  "*amaterasu/boss/rocket",
  "*amaterasu/boss/plume",
  "*amaterasu/boss/beam",
  "*amaterasu/boss/reticle",
  "*amaterasu/boss/closing",
  "*amaterasu/boss/bolt",
  // the Valhalla, the Bastion's guns, the blasts (Mission 9's)
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

void RegisterAmaterasu(void) {
  for (u32 i = 0; i < COUNT(kBullets); i++) AT_LoadBullet(BH, kBullets[i]);
  for (u32 i = 0; i < COUNT(kGenerics); i++) AT_LoadGeneric(BH, kGenerics[i]);
  for (u32 i = 0; i < COUNT(kBadguys); i++) AT_LoadBadGuy(BH, kBadguys[i]);
  AmaLoadHud();
}

void ForgetDefinitions(const char* const* badguys, u32 nb, const char* const* bullets, u32 nu,
                       const char* const* generics, u32 ng);
void ForgetAmaterasu(void) {
  ForgetDefinitions(kBadguys, COUNT(kBadguys), kBullets, COUNT(kBullets), kGenerics, COUNT(kGenerics));
}

// ---- tuning -----------------------------------------------------------------------------------
#define PAINT_R      70             // units: painting range
#define FULL         90             // frames of painting to lock (a boss: FULL_BOSS)
#define FULL_BOSS    150
#define FOLLOW       36             // the spot follows a flying target this long after the lock
#define WARN         72             // ... the warning line and the closing ring
#define BURN         40             // ... the beam
#define BEAM_R       52             // units: what the beam strikes round its spot
#define SUN_DMG      30000          // ... a craft in it: gone (the sun is the strongest thing in the game)
#define RECHARGE     480            // frames until the mirror can strike again
#define SEG_H        46             // a beam tile's height on the screen
#define NSEG         11
#define NPT          16             // paint targets at once (a boss and its parts)
#define NRIDE        8              // things riding the sea
#define NPUFF        18             // the launch's smoke
// the mirror's sun: a small icon on the HUD beside the score, drawn in screen
// space (edinburgh.c's RenderShieldArea hook) so it isn't part of the map
#define SPR_SUN_HUD  0x3e81         // clear of missile.c's 0x3e00..0x3e7b and Edinburgh's 0x3e80
#define SUN_HUD_X    48             // its centre, in 256ths of the screen: right of "SCORE"
#define SUN_HUD_Y    12

// beam frames (ama_cast.py: held("beam")); reticle animations (held("reticle"))
enum { BM_WARN, BM_WARN_R, BM_SUN, BM_BOLT, BM_SCORCH, BM_FLARE };
enum { BO_BOLT, BO_SPARK, BO_MARK, BO_AIM };       // the bolt's animations (held("bolt"))
#define BOLT_CHARGE  80             // the lightning charges at Raijin's ship this long ...
#define BOLT_BURN    26             // ... and cracks along its line this long
#define BOLT_R       30             // units round its mark that it hits
#define NBOLT        5              // bolts at once (the lightning cage: four round the ship, one on it)
enum { RT_BRACKET, RT_Q1, RT_Q2, RT_Q3, RT_Q4, RT_LOCKED, RT_NONE, RT_SUN };

enum { SAID_HOME = 1, SAID_BRIEF = 2, SAID_LAUNCH = 4, SAID_RECON = 8, SAID_LIFT = 16, SAID_CLEAR = 32,
       SAID_ORBIT = 64, SAID_PROBE = 128, SAID_EDIN = 8192, SAID_BUOY = 256, SAID_PICKET = 512, SAID_STORM = 1024,
       VAL_DOCKED = 2048, SAID_PAINT = 4096 };
enum { E_OFF, E_COME, E_TALK, E_SAIL, E_VAL, E_TALK2, E_DONE };
enum { ST_IDLE, ST_FOLLOW, ST_WARN, ST_BURN };

typedef struct { u32 bg; int kind; s32 fill; u32 ret; int struck, off; } Target;
typedef struct {
  int state, t, thunder, tgt;       // tgt: the Target it was locked on, or -1
  s32 x, y;
  u32 seg[NSEG], ring, flare;
  u32 flare2[4];                    // the doubled burst's (after France)
} Strike;
typedef struct {
  int state, t, charge;
  s32 sx, sy, tx, ty;
  u32 seg[NSEG], spark, mark;
} Bolt;
typedef struct { u32 bg; s32 row, x; } Rider;
typedef struct { u32 a; s32 x, y, vx; int t; } Puff;

static struct {
  int on, said;
  u32 ticked;
  // the sea
  int zone, have_tex, sea_f;
  u32 tex[A_LOOKS * A_ZONE_FRAMES];
  s32 opacity;
  // Amaterasu
  Target pt[NPT];
  Strike sun;
  Bolt bolt[NBOLT];
  int mirror;                       // frames until ready (0: ready)
  int jammed, full_sun;
  int upgraded;                     // all three mirrors up (Mission 15 on): a lock charges, then doubles
  // the launch
  u32 rocket;
  int lift_t;                       // frames since ignition (0: not yet)
  s32 rx, ry, rv;                   // the rocket once it's off the pad
  Puff puff[NPUFF];
  int puff_next;
  // the lesson's drone
  u32 drone;
  int drone_t, drone_done, paint_line;   // paint_line: its line's ticket + 1 (0: not asked)
  // the sea's riders: the buoys
  Rider ride[NRIDE];
  int buoy_next;
  int buoy_line, buoy_wait;         // (the first's line's ticket + 1; frames waited)
  // HQ's fleet past the bow
  u32 heimdall, dd[2];
  // HQ's air cover (Flights)
  struct Flight { u32 a[3]; int t, type, cool[3]; s32 x0, y0; int vx, vy; } flight[2];
  int flights, flight_next;
  // the story's holds: the partition waits for the briefing, and for the lesson
  int hold, hold_t, open_done, lesson_done, brief_line;
  // the ending
  int ending, end_t, end_line, end_quiet;
  s32 end_v;
  u32 bastion, btur[3], ciws[4], edd[2];
  s32 by;
} G;

volatile int g_autopaint;           // the desktop harness: targets paint themselves (runtime/probes)
int g_graze_jammed;                 // abyss.c's c_graze reads 0 while it is set

static s32 Abs(s32 v) { return v < 0 ? -v : v; }

// ---- the sea: four looks, one at a time (Mission 13's) ----------------------------------------
#define SH_BG_COUNT  0x8c
#define SH_BG_TEX    0x90

static void ShowZone(int z) {
  const u32 sh = Shogun();
  const int ntex = A_LOOKS * A_ZONE_FRAMES;
  if (!G.have_tex) {
    if (R32(sh, SH_BG_COUNT) != ntex) return;
    for (int k = 0; k < ntex; k++) G.tex[k] = (u32)R32(sh, SH_BG_TEX + 4 * k);
    G.have_tex = 1;
  }
  const int look = kZoneLook[z];
  for (int k = 0; k < A_ZONE_FRAMES; k++) R32(sh, SH_BG_TEX + 4 * k) = (s32)G.tex[look * A_ZONE_FRAMES + k];
  R32(sh, SH_BG_COUNT) = A_ZONE_FRAMES;
  G.zone = z;
}

// The sea's bands are props, laid over the floor so they keep to the props
// (the floor runs on its own clock): each zone's a run of models on one
// texture, its look's first frame. Stepping their texture through the look's
// frames -- the floor's own, loaded already -- gives them the floor's water.
#define SH_MODELS    0x450              // the props' models: 16 bytes each, +4 the texture
#define SEA_SHIFT    3                  // a frame of water every 8

static void SeaMoves(void) {
  const int f = (int)((AT_GetFrameCount() >> SEA_SHIFT) & (A_ZONE_FRAMES - 1));
  if (f == G.sea_f) return;
  G.sea_f = f;
  const u32 sh = Shogun();
  for (int k = 0; k < A_BAND_SETS; k++) {
    const s32 tex = (s32)G.tex[kBandSet[k][2] * A_ZONE_FRAMES + f];
    for (int m = kBandSet[k][0]; m <= kBandSet[k][1]; m++) R32(sh, SH_MODELS + 16 * m + 4) = tex;
  }
}

static void Sea(void) {
  if (!G.have_tex) ShowZone(0);
  const s32 row = AnScroll();
  int want = G.zone;
  while (want + 1 < A_ZONES && row >= kZoneRow[want + 1]) want++;
  if (want != G.zone) ShowZone(want);
  if (G.have_tex) SeaMoves();
}

// abyss.c's hook on onRender's `bl UE_ClearScreen(rgb565)`: the look's colour, not black.
int AmaClearColour(u32* colour) {
  if (!G.on || R32(Shogun(), SH_WORLD_NO) != AMATERASU || AT_GetFrameCount() - G.ticked > 4) return 0;
  const unsigned char* a = kZoneWater[kZoneLook[G.zone]];
  *colour = ((u32)(a[0] >> 3) << 11) | ((u32)(a[1] >> 2) << 5) | (u32)(a[2] >> 3);
  return 1;
}

// ---- Amaterasu ----------------------------------------------------------------------------------
static int Find(u32 bg) {
  for (int k = 0; k < NPT; k++)
    if (G.pt[k].bg == bg) return k;
  return -1;
}

void AmaPaintAdd(u32 bg, int kind) {
  if (!bg || Find(bg) >= 0) return;
  for (int k = 0; k < NPT; k++) {
    Target* p = &G.pt[k];
    if (p->bg) continue;
    SB_ClearBytes(p, sizeof *p);
    p->bg = bg;
    p->kind = kind;
    if (kind == PT_PART) return;   // (not painted: no bracket)
    s32 x, y;
    SB_GetActorPosition(Act(bg), &x, &y);
    p->ret = AnSpawn("*amaterasu/boss/reticle", x, y);
    AnFrame(p->ret, RT_BRACKET);
    return;
  }
}

static void Forget(int k) {
  Target* p = &G.pt[k];
  AnDrop(&p->ret);
  if (G.sun.tgt == k) G.sun.tgt = -1;
  p->bg = 0;
}

void AmaPaintRemove(u32 bg) {
  const int k = Find(bg);
  if (k >= 0) Forget(k);
}

// A target paintable or not for now (the Ryujin's body: only while none of
// its parts is open); off, its bracket hidden and its paint gone.
void AmaPaintEnable(u32 bg, int on) {
  const int k = Find(bg);
  if (k >= 0) G.pt[k].off = !on;
}

int AmaStruck(u32 bg) {
  const int k = Find(bg);
  if (k < 0) return 0;
  const int n = G.pt[k].struck;
  G.pt[k].struck = 0;
  return n;
}

void AmaJam(int on) {
  G.jammed = on;
  g_graze_jammed = on;
}

void AmaFullSun(void) {
  G.full_sun = 1;
  G.mirror = 0;
}

int AmaStriking(void) { return G.sun.state != ST_IDLE; }

// ---- after France (docs/M14_DESIGN.md 3.1, Mission 15) -------------------------------------------
// With the third mirror up a lock no longer fires at once: the column charges
// on the spot (longer, the light building), then the burst comes down twice
// the radius, all at once.
#define WARN_UP      150            // the charge
#define BEAM_R_UP    (BEAM_R * 2)

void AmaUpgrade(int on) { G.upgraded = on; }

// ---- the HUD's sun ---------------------------------------------------------------------------
void AmaLoadHud(void) {
  AT_LoadSprites(SPR_SUN_HUD, "*amaterasu/sprites/sun_hud.tga", 1, R32(Shogun(), SH_UI_SCALE));
}

// RenderHUD (edinburgh.c's hook): lit when the mirror is ready (and nothing while jammed),
// in Missions 14 and 15 while the mirror is running
void AmaHud(void) {
  const u32 w = R32(Shogun(), SH_WORLD_NO);
  if (!G.on || (w != AMATERASU && w != PARIS && w != ATLANTIC) || AT_GetFrameCount() - G.ticked > 4) return;
  if (G.mirror != 0 || G.jammed || G.sun.state != ST_IDLE || !(G.said & SAID_ORBIT)) return;
  const s32 x = ((AT_GetScreenWidth() * SUN_HUD_X) >> 8) - (AT_GetWidth(SPR_SUN_HUD) >> 1);
  const s32 y = ((AT_GetScreenHeight() * SUN_HUD_Y) >> 8) - (AT_GetHeight(SPR_SUN_HUD) >> 1);
  AT_DrawSprite(SPR_SUN_HUD, x, y, 0);
}

static int BeamR(void) { return G.upgraded ? BEAM_R_UP : BEAM_R; }

// ---- the strike's pieces, made up front (Mission 16) ------------------------------------------
// A generic made later can take a freed slot and draw *under* one made earlier
// (docs/NEW_LEVEL.md 13): over Mission 16's dreadnought the beam vanished under
// its hull. So there they are made once, right after the hull, and kept parked
// out of sight between strikes (AmaSunPrespawn); elsewhere they come and go as before.
#define POOL_BEAM  (NSEG + 5)       // the column, the flare and the doubled burst's four
#define POOL_PARK  FX(-300)
static u32 g_pool_beam[POOL_BEAM], g_pool_ring;
static unsigned char g_pool_used[POOL_BEAM], g_pool_ring_used;
static int g_pooled;

static void PoolClear(void) {
  g_pooled = 0;
  for (int k = 0; k < POOL_BEAM; k++) { g_pool_beam[k] = 0; g_pool_used[k] = 0; }
  g_pool_ring = 0;
  g_pool_ring_used = 0;
}

void AmaSunPrespawn(void) {
  PoolClear();
  for (int k = 0; k < POOL_BEAM; k++) {
    g_pool_beam[k] = AnSpawn("*amaterasu/boss/beam", FX(200), FX(200));
    AnPut(g_pool_beam[k], POOL_PARK, POOL_PARK);
  }
  g_pool_ring = AnSpawn("*amaterasu/boss/closing", FX(200), FX(200));
  AnPut(g_pool_ring, POOL_PARK, POOL_PARK);
  g_pooled = 1;
}

// A beam tile (ring: the closing ring) from the pool, or made as before.
static u32 SunNew(int ring, s32 x, s32 y) {
  if (!g_pooled) return AnSpawn(ring ? "*amaterasu/boss/closing" : "*amaterasu/boss/beam", x, y);
  if (ring) {
    if (g_pool_ring_used) return 0;
    g_pool_ring_used = 1;
    AnPut(g_pool_ring, x, y);
    return g_pool_ring;
  }
  for (int k = 0; k < POOL_BEAM; k++)
    if (!g_pool_used[k]) {
      g_pool_used[k] = 1;
      AnPut(g_pool_beam[k], x, y);
      return g_pool_beam[k];
    }
  return 0;
}

static void SunDrop(u32* a) {
  if (!*a) return;
  if (g_pooled) {
    if (*a == g_pool_ring) g_pool_ring_used = 0;
    for (int k = 0; k < POOL_BEAM; k++) if (g_pool_beam[k] == *a) g_pool_used[k] = 0;
    AnPut(*a, POOL_PARK, POOL_PARK);
    *a = 0;
    return;
  }
  AnDrop(a);
}

// Mission 15 runs the mirror without Mission 14's story: a clean start, the
// mirror in orbit from the first frame.
void AmaSunReset(void) {
  PoolClear();
  SB_ClearBytes(&G, sizeof G);
  G.on = 1;
  G.opacity = -1;
  G.sun.tgt = -1;
  G.said = SAID_ORBIT;
  g_graze_jammed = 0;
}

static void StrikeDrop(Strike* s) {
  for (int k = 0; k < 4; k++) SunDrop(&s->flare2[k]);
  for (int k = 0; k < NSEG; k++) SunDrop(&s->seg[k]);
  SunDrop(&s->ring);
  SunDrop(&s->flare);
  s->state = ST_IDLE;
}

// The column of tiles from the top of the screen down to the spot, showing `frame`.
static void Column(Strike* s, int frame) {
  const s32 top = FX(-20);
  for (int k = 0; k < NSEG; k++) {
    const s32 y = top + FX(SEG_H) * k;
    if (y > s->y) {
      SunDrop(&s->seg[k]);
      continue;
    }
    if (!s->seg[k]) s->seg[k] = SunNew(0, s->x, y);
    AnPut(s->seg[k], s->x, y + FX(SEG_H / 2));
    AnFrame(s->seg[k], frame);
  }
}

static void Begin(Strike* s, s32 x, s32 y, int thunder, int tgt) {
  StrikeDrop(s);
  s->state = thunder ? ST_WARN : ST_FOLLOW;
  s->t = 0;
  s->thunder = thunder;
  s->tgt = tgt;
  s->x = x;
  s->y = y;
}

// ---- Raijin's lightning ---------------------------------------------------------------------------
// Not the sun's way at all: it comes from his ship, not from straight above,
// in violet and red, crooked. It charges at the ship (a crackle), an
// enemy-red mark sits on its target and a dashed red line aims at it; then
// it cracks from the ship to the mark along that line, and hits what's there.
static int ShipNear(s32 x, s32 y, int r);
static void HurtShip(void);

static void BoltDrop(Bolt* b) {
  for (int k = 0; k < NSEG; k++) AnDrop(&b->seg[k]);
  AnDrop(&b->spark);
  AnDrop(&b->mark);
  b->state = ST_IDLE;
}

// A bolt from (sx, sy) to a mark on (tx, ty), charging `charge` frames: its
// number (for AmaBoltFrom), or -1 if every one is busy.
int AmaBolt(s32 sx, s32 sy, s32 tx, s32 ty, int charge) {
  for (int i = 0; i < NBOLT; i++) {
    Bolt* b = &G.bolt[i];
    if (b->state != ST_IDLE) continue;
    BoltDrop(b);
    b->state = ST_WARN;
    b->t = 0;
    b->charge = charge;
    b->sx = sx;
    b->sy = sy;
    b->tx = tx;
    b->ty = ty;
    return i;
  }
  return -1;
}

void AmaThunder(s32 sx, s32 sy, s32 tx, s32 ty) { AmaBolt(sx, sy, tx, ty, BOLT_CHARGE); }

// Bolt i's source has moved (its ship): it comes from here now.
void AmaBoltFrom(int i, s32 sx, s32 sy) {
  if (i < 0 || i >= NBOLT || G.bolt[i].state == ST_IDLE) return;
  G.bolt[i].sx = sx;
  G.bolt[i].sy = sy;
}

int AmaBoltBusy(int i) { return i >= 0 && i < NBOLT && G.bolt[i].state != ST_IDLE; }

int AmaThunderBusy(void) {
  for (int i = 0; i < NBOLT; i++)
    if (G.bolt[i].state != ST_IDLE) return 1;
  return 0;
}

// Lengths of `anim` laid from the ship to the mark, turned along the line.
static void BoltLine(Bolt* b, int anim) {
  const s32 a = AnAngle(b->sx, b->sy, b->tx, b->ty);
  const s32 d = AnDist(b->tx - b->sx, b->ty - b->sy);
  const s32 c = SB_Cos(a), s = SB_Sin(a);
  for (int k = 0; k < NSEG; k++) {
    const s32 along = SEG_H * k + SEG_H / 2;
    if (along - SEG_H / 2 > d) {
      AnDrop(&b->seg[k]);
      continue;
    }
    const s32 x = b->sx + c * along, y = b->sy + s * along;
    if (!b->seg[k]) b->seg[k] = AnSpawn("*amaterasu/boss/bolt", x, y);
    AnPut(b->seg[k], x, y);
    AnTurn(b->seg[k], a);
    AnFrame(b->seg[k], anim);
  }
}

static void BoltTick(Bolt* b) {
  if (b->state == ST_IDLE) return;
  b->t++;
  if (!b->spark) b->spark = AnSpawn("*amaterasu/boss/bolt", b->sx, b->sy);
  AnPut(b->spark, b->sx, b->sy);
  AnFrame(b->spark, BO_SPARK);
  if (!b->mark) b->mark = AnSpawn("*amaterasu/boss/bolt", b->tx, b->ty);
  AnPut(b->mark, b->tx, b->ty);
  AnFrame(b->mark, BO_MARK);
  if (b->state == ST_WARN) {
    BoltLine(b, BO_AIM);
    if (b->t >= b->charge) {
      b->state = ST_BURN;
      b->t = 0;
      if (ShipNear(b->tx, b->ty, BOLT_R)) HurtShip();
      AnBlast(b->tx, b->ty);
      SB_ShakeWorld();
    }
    return;
  }
  BoltLine(b, BO_BOLT);
  if (b->t >= BOLT_BURN) BoltDrop(b);
}

static int ShipNear(s32 x, s32 y, int r) { return AnDist(PlayerX() - x, PlayerY() - y) < r; }

static void HurtShip(void) {
  const u32 sh = Shogun();
  if (*(volatile unsigned short*)(sh + SH_HITLOCK) || R32(sh, SH_HITLOCK2) || InCutscene()) return;
  SB_StrikePlayer(sh + SH_PLAYER, 0, sh + SH_HUD);
}

// The beam's first frame: everything round the spot is struck.
static void Burn(Strike* s) {
  const int R = s->thunder ? BEAM_R : BeamR();
  if (ShipNear(s->x, s->y, R)) HurtShip();
  SB_ShakeWorld();
  if (s->thunder) return;
  for (u32 b = SB_GetFirstBadGuy(BH); b; b = SB_GetNextBadGuy(BH, b)) {
    if (SB_GetBadGuyHealth(b) <= 0) continue;
    s32 x, y;
    SB_GetActorPosition(Act(b), &x, &y);
    if (AnDist(x - s->x, y - s->y) >= R + 16) continue;
    const int k = Find(b);
    if (k < 0) {
      SB_StrikeBadGuy(BH, b, SUN_DMG);      // (a boss's own shut hull shrugs it off)
      continue;
    }
    Target* p = &G.pt[k];
    if (p->kind == PT_BOSS) {
      p->struck++;                  // the boss's to act on (raijin.c)
    } else if (p->kind == PT_OPEN && GetReg(b, 14) != 0 && s->tgt != k) {
      continue;                     // not its turn (and not the one painted): shut, even to the sun
    } else {                        // riders, the drone, armoured and boss parts: destroyed outright
      p->struck++;
      Forget(k);
      Remove(b);
    }
  }
}

static void StrikeTick(Strike* s) {
  if (s->state == ST_IDLE) return;
  s->t++;
  switch (s->state) {
  case ST_FOLLOW:                   // (the sun's only) the spot on the target a moment
    if (s->tgt >= 0 && G.pt[s->tgt].bg && Alive(G.pt[s->tgt].bg))
      SB_GetActorPosition(Act(G.pt[s->tgt].bg), &s->x, &s->y);
    if (s->t >= FOLLOW) {
      s->state = ST_WARN;
      s->t = 0;
    }
    // fall through to draw the line
  case ST_WARN:
    // a target riding the sea keeps its spot on it (the spot is on the sea, too)
    if (!s->thunder && s->tgt >= 0 && G.pt[s->tgt].bg && G.pt[s->tgt].kind <= PT_PICKET && Alive(G.pt[s->tgt].bg))
      SB_GetActorPosition(Act(G.pt[s->tgt].bg), &s->x, &s->y);
    {
      // (upgraded: the charge -- the line flickers to the beam's own light, faster as it builds)
      const int t = s->state == ST_WARN ? s->t : 0;
      int f = s->thunder ? BM_WARN_R : BM_WARN;
      if (!s->thunder && G.upgraded && t > 30 && ((t * t) & 0x400)) f = BM_SUN;
      Column(s, f);
    }
    if (!s->ring) s->ring = SunNew(1, s->x, s->y);
    AnPut(s->ring, s->x, s->y);
    {
      const int t = s->state == ST_WARN ? s->t : 0;
      const int w = (G.upgraded && !s->thunder) ? WARN_UP : WARN;
      AnFrame(s->ring, t * 4 < w ? 0 : t * 2 < w ? 1 : t * 4 < w * 3 ? 2 : 3);
    }
    if (s->state == ST_WARN && s->t >= ((G.upgraded && !s->thunder) ? WARN_UP : WARN)) {
      s->state = ST_BURN;
      s->t = 0;
      SunDrop(&s->ring);
      s->flare = SunNew(0, s->x, s->y);
      if (G.upgraded && !s->thunder) {        // twice the size: four more flares round it, and blasts
        static const signed char kDx[4] = { -40, 40, 0, 0 }, kDy[4] = { 0, 0, -40, 40 };
        for (int k = 0; k < 4; k++) s->flare2[k] = SunNew(0, s->x + FX(kDx[k]), s->y + FX(kDy[k]));
        for (int k = 0; k < 8; k++) AnBlast(s->x + FX(Rnd(-80, 81)), s->y + FX(Rnd(-80, 81)));
      }
      Burn(s);
    }
    break;
  default:                          // ST_BURN
    Column(s, s->thunder ? BM_BOLT : BM_SUN);
    AnPut(s->flare, s->x, s->y);
    AnFrame(s->flare, s->t < BURN - 8 ? BM_FLARE : BM_SCORCH);
    for (int k = 0; k < 4; k++) {
      static const signed char kDx[4] = { -40, 40, 0, 0 }, kDy[4] = { 0, 0, -40, 40 };
      AnPut(s->flare2[k], s->x + FX(kDx[k]), s->y + FX(kDy[k]));
      AnFrame(s->flare2[k], s->t < BURN - 8 ? BM_FLARE : BM_SCORCH);
    }
    if (s->t >= BURN) {
      if (!s->thunder) G.mirror = G.full_sun ? 60 : RECHARGE;
      StrikeDrop(s);
    }
    break;
  }
}

static void Paint(void) {
  const int can = G.mirror == 0 && !G.jammed && G.sun.state == ST_IDLE;
  if (G.mirror > 0) G.mirror--;
  for (int k = 0; k < NPT; k++) {
    Target* p = &G.pt[k];
    if (!p->bg) continue;
    if (!Alive(p->bg)) {
      Forget(k);
      continue;
    }
    if (p->kind == PT_PART) continue;     // (struck only if it's in the beam)
    s32 x, y;
    SB_GetActorPosition(Act(p->bg), &x, &y);
    AnPut(p->ret, x, y);
    // a Ryujin's part only on its turn (its script's R14 clear); a target turned off
    if ((p->kind == PT_OPEN && GetReg(p->bg, 14) != 0) || p->off) {
      if (!(G.sun.state != ST_IDLE && G.sun.tgt == k)) {
        p->fill = 0;
        AnFrame(p->ret, RT_NONE);
        continue;
      }
    }
    const int full = p->kind == PT_BOSS && !G.full_sun ? FULL_BOSS : FULL;
    const int near = can && y > FX(-10) && (g_autopaint || ShipNear(x, y, PAINT_R + (p->kind == PT_BOSS ? 30 : 0)));
    if (near) p->fill++;
    else if (p->fill > 0 && (G.mirror & 1)) p->fill--;
    else if (p->fill > 0 && G.sun.state == ST_IDLE) p->fill -= (AT_GetFrameCount() & 1);
    if (G.jammed) p->fill = 0;
    int f = RT_BRACKET;
    if (G.sun.state != ST_IDLE && G.sun.tgt == k) f = RT_LOCKED;
    else if (p->fill >= full) f = RT_LOCKED;
    else if (p->fill * 4 >= full * 3) f = RT_Q4;
    else if (p->fill * 2 >= full) f = RT_Q3;
    else if (p->fill * 4 >= full) f = RT_Q2;
    else if (p->fill > 0) f = RT_Q1;
    AnFrame(p->ret, f);
    if (can && p->fill >= full) {
      p->fill = 0;
      Begin(&G.sun, x, y, 0, k);
      if (p->kind == PT_DRONE) AnLine(AL_DOC_GETOUT);
    }
  }
  StrikeTick(&G.sun);
  for (int i = 0; i < NBOLT; i++) BoltTick(&G.bolt[i]);
}

// ---- the launch ---------------------------------------------------------------------------------
#define LIFT_T      (A_RECON_T + 300)   // the rocket lights ...
#define IGNITE      90                  // ... and lifts off this much later
#define ROCKET_DY   (-40)               // its centre above the silo's middle

static void PuffAt(s32 x, s32 y, s32 vx) {
  Puff* p = &G.puff[G.puff_next];
  if (++G.puff_next >= NPUFF) G.puff_next = 0;
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
    Puff* p = &G.puff[k];
    if (!p->a) continue;
    p->t++;
    p->x += p->vx;
    p->y -= FX(0.15);
    AnPut(p->a, p->x + AnGroundDx(p->y), p->y);
    if (p->t == 46) AnFrame(p->a, 1);
    if (p->t > 120) AnDrop(&p->a);
  }
}

static void Launch(s32 pt) {
  const s32 sy = AnRowY(A_SILO_ROW);
  if (!G.rocket && !G.lift_t && sy > FX(-60) && pt < LIFT_T) {
    G.rocket = AnSpawn("*amaterasu/boss/rocket", FX(200), sy + FX(ROCKET_DY));
    AnFrame(G.rocket, 0);
  }
  if (!G.rocket) return;
  if (!G.lift_t) {
    AnPut(G.rocket, FX(200) + AnGroundDx(sy), sy + FX(ROCKET_DY));
    if (pt >= LIFT_T) {
      G.lift_t = 1;
      G.rx = FX(200);
      G.ry = sy + FX(ROCKET_DY);
      AnFrame(G.rocket, 1);
    }
    return;
  }
  G.lift_t++;
  if (G.lift_t < IGNITE) {          // lit on the pad: smoke rolling out of the trenches
    if ((G.lift_t & 7) == 0)
      PuffAt(G.rx + FX(Rnd(-30, 31)), sy + FX(Rnd(-6, 10)), (G.lift_t & 8) ? FX(0.5) : FX(-0.5));
    AnPut(G.rocket, G.rx + AnGroundDx(sy) + FX((G.lift_t & 2) ? 1 : 0) - FX(0.5), G.ry);
    return;
  }
  if (G.lift_t == IGNITE) AnLine(AL_VOLTA_LIFTOFF);
  if (G.rv < FX(3.2)) G.rv += FX(0.018);
  G.ry -= G.rv;
  AnPut(G.rocket, G.rx + AnGroundDx(G.ry), G.ry);
  if ((G.lift_t & 3) == 0) PuffAt(G.rx + FX(Rnd(-4, 5)), G.ry + FX(66), FX(Rnd(-20, 21)) >> 6);
  if (G.ry < FX(-180)) AnDrop(&G.rocket);
}

// ---- the lesson: the target drone over the shelf's edge -----------------------------------------
static void Lesson(s32 pt) {
  // the Doc's words first, then the drone (it waits for them: a line takes a while)
  if (!G.paint_line && pt >= A_LESSON_T) G.paint_line = AnLine(AL_DOC_PAINT) + 1;
  if (!G.drone && !G.drone_done && G.paint_line && (AnLineUp(G.paint_line - 1) || G.hold_t > 900)) {
    G.drone = SB_SpawnBadGuy(BH, "*amaterasu/boss/target_drone", FX(200), FX(-40));
    G.drone_t = 0;
    if (G.drone) AmaPaintAdd(G.drone, PT_DRONE);
  }
  if (!G.drone) return;
  G.drone_t++;
  if (!Alive(G.drone)) {
    G.drone = 0;
    G.drone_done = 1;
    AnLine(AL_DOC_GOOD);
    return;
  }
  // in to a slow hover, swaying; away up the screen if the lesson runs out
  s32 x, y;
  SB_GetActorPosition(Act(G.drone), &x, &y);
  const s32 want_y = pt < A_SHIFT_A + 240 ? FX(130) : FX(-120);
  y += Ease(want_y - y, FX(1.2));
  x = FX(200) + SB_MulFP(FX(60), SB_Sin((s32)(G.drone_t * 0x8000)));
  Place(G.drone, x, y);
  if (y < FX(-100) && pt > A_SHIFT_A + 240) {
    AmaPaintRemove(G.drone);
    SB_SetActorPosition(Act(G.drone), FX(200), FX(-400));
    Remove(G.drone);
    G.drone = 0;
    G.drone_done = 1;
  }
}

// ---- the sea's riders: the probe's buoys ----------------------------------------
static void Ride(const char* name, int kind, s32 x) {
  for (int k = 0; k < NRIDE; k++) {
    Rider* r = &G.ride[k];
    if (r->bg) continue;
    r->row = AnScroll() + FX(13.6);
    r->x = x;
    r->bg = SB_SpawnBadGuy(BH, name, x, FX(-60));
    if (r->bg) AmaPaintAdd(r->bg, kind);
    return;
  }
}

// The first buoy comes in as the Doc's line about it goes up, not when its
// time comes: lines run behind (~500 frames each untapped), and a rider could
// scroll past before it was spoken of. (A limit, in case the talk sticks.)
#define RIDER_WAIT 900

static void Riders(s32 pt) {
  while (G.buoy_next < A_NBUOY && pt >= kBuoyT[G.buoy_next]) {
    if (!(G.said & SAID_BUOY)) {
      if (!G.buoy_line) G.buoy_line = AnLine(AL_DOC_BUOYS) + 1;
      if (!AnLineUp(G.buoy_line - 1) && ++G.buoy_wait < RIDER_WAIT) break;
      G.said |= SAID_BUOY;
    }
    const int k = G.buoy_next++;
    Ride("*amaterasu/boss/buoy", PT_BUOY, FX((k & 1) ? 270 : 130) + FX(Rnd(-30, 31)));
  }
  for (int k = 0; k < NRIDE; k++) {
    Rider* r = &G.ride[k];
    if (!r->bg) continue;
    if (!Alive(r->bg)) {
      r->bg = 0;
      continue;
    }
    const s32 y = AnRowY(r->row);
    Place(r->bg, r->x + AnGroundDx(y), y);
    if (y > FX(470)) {                // gone by: away quietly
      AmaPaintRemove(r->bg);
      SB_SetActorPosition(Act(r->bg), FX(200), FX(-400));
      Remove(r->bg);
      r->bg = 0;
    }
  }
}

// ---- HQ's Home Fleet: on the open water past the bow, and the fighters ------------------------
static void FleetAt(u32* a, const char* name, s32 row, s32 x, int turn) {
  const s32 y = AnRowY(row);
  if (y < FX(-220) || y > FX(620)) {
    AnDrop(a);
    return;
  }
  if (!*a) *a = AnSpawn(name, x, y);
  AnPut(*a, x + AnGroundDx(y), y);
  if (turn) AnTurn(*a, UP);
}

static void Fleet(void) {
  if (G.ending) return;
  FleetAt(&G.heimdall, "*amaterasu/boss/heimdall", A_BOW_ROW + FX(4.5), FX(110), 0);
  FleetAt(&G.dd[0], "*amaterasu/boss/destroyer", A_BOW_ROW + FX(6.5), FX(300), 1);
  FleetAt(&G.dd[1], "*amaterasu/boss/destroyer", A_BOW_ROW + FX(9.0), FX(150), 1);
}

// HQ's air cover, until the pilot is well clear of the base (its bow past the
// screen): flights crossing the screen now and then, each craft picking the
// nearest of the Shogun's craft and shooting it (andaman.c's shells: a
// tracer, a hit where it lands).
//   Einherjar  interceptors, three, fast, low left to high right: guns
//   Fenrir     strike fighters, two, a fast pass nearly straight down the
//              screen (a fighter's run, not a drift): missiles, heavy
//   Sleipnir   a four-rotor gunship, alone, slow, straight up the screen: a
//              raking chin gun
enum { FT_EINHERJAR, FT_FENRIR, FT_SLEIPNIR, NFT };
static const char* const kFtName[NFT] = { "*amaterasu/boss/einherjar", "*amaterasu/boss/fenrir",
                                          "*amaterasu/boss/sleipnir" };
static const unsigned char kFtCount[NFT] = { 3, 2, 1 };
// each craft's place in the flight (mirrored for a flight going left)
static const signed char kFtDx[3] = { 0, -16, 16 }, kFtDy[3] = { 0, 22, 22 };
static const unsigned char kFtEvery[NFT] = { 16, 34, 6 };       // frames between shots, each craft
static const unsigned char kFtShell[NFT] = { SHOT_SMALL, SHOT_MEDIUM, SHOT_SMALL };
static const short kFtDmg[NFT] = { 60, 220, 30 };
#define FLIGHT_MAX    1200         // frames a flight may last at the most; it ends once off the screen
#define FLIGHT_GAP    170          // frames between one flight's start and the next
#define FLIGHT_RANGE  230          // units: what a craft will shoot at

// The nearest of the Shogun's craft within range of (x, y): not a paint
// target (the drone, the buoys are the mirror's), on the screen.
static u32 Nearest(s32 x, s32 y, s32* ox, s32* oy) {
  u32 best = 0;
  s32 bd = FLIGHT_RANGE;
  for (u32 b = SB_GetFirstBadGuy(BH); b; b = SB_GetNextBadGuy(BH, b)) {
    if (SB_GetBadGuyHealth(b) <= 0 || Find(b) >= 0) continue;
    s32 bx, by;
    SB_GetActorPosition(Act(b), &bx, &by);
    if (by < FX(-10) || by > FX(420) || bx < FX(-10) || bx > FX(410)) continue;
    const s32 d = AnDist(bx - x, by - y);
    if (d >= bd) continue;
    bd = d;
    best = b;
    *ox = bx;
    *oy = by;
  }
  return best;
}

// Each flight its own path (they all flew one line): where it comes in, which
// way, how steep -- velocities in sixteenths of a unit a frame.
static void FlightPath(struct Flight* f) {
  const int flip = Rnd(0, 2);                       // from the left or the right
  switch (f->type) {
  case FT_EINHERJAR:                                // up across the screen from a low corner, fast
    f->x0 = flip ? FX(440) : FX(-40);
    f->y0 = FX(Rnd(300, 430));
    f->vx = (flip ? -1 : 1) * Rnd(30, 42);
    f->vy = -Rnd(14, 32);
    break;
  case FT_FENRIR:                                   // a straight run, down from the top or up from behind
    f->x0 = FX(Rnd(70, 331));
    f->vx = Rnd(-7, 8);
    if (Rnd(0, 3)) {
      f->y0 = FX(-50);
      f->vy = Rnd(54, 66);
    } else {
      f->y0 = FX(500);
      f->vy = -Rnd(54, 66);
    }
    break;
  default:                                          // the gunship: up from behind, or across, slowly
    if (Rnd(0, 2)) {
      f->x0 = FX(Rnd(80, 321));
      f->y0 = FX(480);
      f->vx = Rnd(-3, 4);
      f->vy = -Rnd(10, 15);
    } else {
      f->x0 = flip ? FX(450) : FX(-50);
      f->y0 = FX(Rnd(120, 300));
      f->vx = (flip ? -1 : 1) * Rnd(11, 16);
      f->vy = Rnd(-4, 5);
    }
    break;
  }
}

static void Flights(s32 pt) {
  // until the base is well behind (HQ's bow past the bottom of the screen)
  const int over_base = AnScroll() < A_BOW_ROW + FX(3);
  if (over_base && pt >= A_HOME_T + 150 && pt >= G.flight_next) {
    for (int i = 0; i < 2; i++) {
      if (G.flight[i].t) continue;
      const int type = G.flights;
      if (++G.flights >= NFT) G.flights = 0;     // (the three in turn: no modulo in the blob)
      G.flight[i].t = 1;
      G.flight[i].type = type;
      FlightPath(&G.flight[i]);
      for (int k = 0; k < kFtCount[type]; k++)
        G.flight[i].a[k] = AnSpawn(kFtName[type], G.flight[i].x0, G.flight[i].y0);
      G.flight_next = pt + FLIGHT_GAP;
      break;
    }
  }
  for (int i = 0; i < 2; i++) {
    struct Flight* f = &G.flight[i];
    if (!f->t) continue;
    const int t = f->t++, ty = f->type;
    int gone = t > 30;               // (every craft of it off the screen: the flight's over)
    for (int k = 0; k < kFtCount[ty]; k++) {
      const s32 lag = k ? 12 : 0;
      const s32 x = f->x0 + FX(kFtDx[k] * (f->vx < 0 ? -1 : 1)) + (FX(f->vx) >> 4) * (t - lag);
      const s32 y = f->y0 + FX(kFtDy[k] * (f->vy < 0 ? 1 : -1)) + (FX(f->vy) >> 4) * (t - lag);
      AnPut(f->a[k], x, y);
      AnTurn(f->a[k], AnAngle(0, 0, FX(f->vx), FX(f->vy)));   // nose along its path
      if (x > FX(-80) && x < FX(480) && y > FX(-80) && y < FX(540)) gone = 0;
      // its guns on the nearest of the Shogun's craft
      f->cool[k]--;
      if (f->cool[k] <= 0 && x > FX(0) && x < FX(400) && y > FX(10) && y < FX(440)) {
        s32 ex, ey;
        const u32 e = Nearest(x, y, &ex, &ey);
        if (e) {
          AnShoot(x, y, ex, ey, ty == FT_FENRIR ? 7 : 10, kFtShell[ty], TAG_NONE, e, kFtDmg[ty]);
          f->cool[k] = kFtEvery[ty] + (k << 2);
        }
      }
    }
    // (a fixed length cut the slow Sleipnir off halfway up the screen: it vanished)
    if (gone || t > FLIGHT_MAX) {
      for (int k = 0; k < 3; k++) AnDrop(&f->a[k]);
      f->t = 0;
    }
  }
}

// ---- the ending: the fleet comes up from behind -------------------------------------------------
#define END_BASTION_X 112          // off to the left, clear of the ship
#define END_BASTION_Y 230
#define END_DD_X      300          // the destroyers to the right, one ahead of the other
#define END_DD_Y      190
#define END_QUIET     120          // frames of silence before the level may end

void AmaEnding(void) {
  if (G.ending) return;
  G.ending = E_COME;
  G.end_t = 0;
  G.by = FX(620);
  // (the Valhalla after them, alone: she and her escorts fill the screen)
  G.bastion = AnSpawn("*amaterasu/boss/bastion14", FX(200), G.by);
  for (int k = 0; k < 3; k++) G.btur[k] = AnSpawn("*andaman/boss/bturret", FX(200), G.by);
  for (int k = 0; k < 4; k++) G.ciws[k] = AnSpawn("*andaman/boss/ciws", FX(200), G.by);
  for (int k = 0; k < 2; k++) G.edd[k] = AnSpawn("*amaterasu/boss/destroyer", FX(200), G.by);
  AnLine(AL_DOC_DATA);
}

int AmaEndingDone(void) { return G.ending == E_DONE; }

static void EndPlace(void) {
  static const signed char kTur[3][2] = { { BASTION_TUR0_X, BASTION_TUR0_Y }, { BASTION_TUR1_X, BASTION_TUR1_Y },
                                          { BASTION_TUR2_X, BASTION_TUR2_Y } };
  static const signed char kCiws[4][2] = { { -36, -24 }, { 36, -24 }, { -34, 48 }, { 34, 48 } };
  if (G.ending == E_COME || G.ending == E_TALK) G.by += Ease(FX(END_BASTION_Y) - G.by, FX(1.0));
  const s32 bx = FX(END_BASTION_X) + AnGroundDx(G.by);
  AnPut(G.bastion, bx, G.by);
  AnFrame(G.bastion, 0);
  for (int k = 0; k < 3; k++) {
    AnPut(G.btur[k], bx + FX(kTur[k][0]), G.by + FX(kTur[k][1]));
    AnTurn(G.btur[k], k == 2 ? DOWN : UP);
  }
  for (int k = 0; k < 4; k++) {
    AnPut(G.ciws[k], bx + FX(kCiws[k][0]), G.by + FX(kCiws[k][1]));
    AnTurn(G.ciws[k], UP);
  }
  for (int k = 0; k < 2; k++) {
    const s32 y = G.by + FX(END_DD_Y - END_BASTION_Y) + FX(k ? 130 : 0);
    AnPut(G.edd[k], FX(END_DD_X + (k ? 40 : 0)) + AnGroundDx(y), y);
    AnTurn(G.edd[k], UP);
  }
}

static void Ending(void) {
  G.end_t++;
  if (G.ending != E_VAL && G.ending != E_TALK2 && G.ending != E_DONE) EndPlace();
  switch (G.ending) {
  case E_COME:
    if (G.end_t > 200 && Abs(FX(END_BASTION_Y) - G.by) < FX(4)) {
      G.ending = E_TALK;
      G.end_line = AnLine(AL_ARGUS_FRANCE);
      G.end_quiet = 0;
    }
    break;
  case E_TALK:
    G.end_quiet = InCutscene() ? 0 : G.end_quiet + 1;
    if (AnLineUp(G.end_line) && G.end_quiet > 40) {
      G.ending = E_SAIL;               // on for France: up the screen and away
      G.end_t = 0;
    }
    break;
  case E_SAIL:
    G.end_v += FX(0.03);
    if (G.end_v > FX(2.4)) G.end_v = FX(2.4);
    G.by -= G.end_v;
    if (G.by < FX(-340)) {
      AnDrop(&G.bastion);
      for (int k = 0; k < 3; k++) AnDrop(&G.btur[k]);
      for (int k = 0; k < 4; k++) AnDrop(&G.ciws[k]);
      for (int k = 0; k < 2; k++) AnDrop(&G.edd[k]);
      // the screen's clear: the Valhalla comes up alone (her sprites and the
      // Bastion's flickered over each other on the screen together)
      G.ending = E_VAL;
      G.end_t = 0;
      AnValCome();
    }
    break;
  case E_VAL:
    if (AnValThere() || G.end_t > 420) {
      G.ending = E_TALK2;
      G.end_line = AnLine(AL_VOLTA_ARRIVE);
      G.end_quiet = 0;
    }
    break;
  case E_TALK2:
    // done only once every line has been said: nothing queued and the box
    // closed for END_QUIET frames running (a line's ticket alone read as
    // done while the last one was still waiting: the level ended under it)
    G.end_quiet = InCutscene() ? 0 : G.end_quiet + 1;
    if (AnLineUp(G.end_line) && G.end_quiet > END_QUIET) G.ending = E_DONE;
    break;
  }
}

// ---- the story's holds ---------------------------------------------------------------------------
// Untapped, a line takes ~500 frames: the launch waits (the partition held,
// the scroll stopped over the silo) until the briefing is done, and the run
// north until the lesson's drone is down -- each with a limit, in case.
static void Hold(int on) {
  if (on == G.hold) return;
  G.hold = on;
  G.hold_t = 0;
  SB_PausePartition(BH, on);
}

static void Holds(s32 pt) {
  if (!G.open_done && pt >= A_STOPPED_T) {
    Hold(1);
    if ((AnLineUp(G.brief_line) && AnQuiet()) || ++G.hold_t > 3600) {
      G.open_done = 1;
      Hold(0);
    }
  }
  if (G.open_done && !G.lesson_done && pt >= A_LESSON_T + 30) {
    Hold(1);
    if (G.drone_done || ++G.hold_t > 2100) {
      G.lesson_done = 1;
      Hold(0);
    }
  }
}

// ---- a new run, and every frame ------------------------------------------------------------------
void AndamanReset(void);

void AmaReset(void) {
  PoolClear();
  AndamanReset();                              // the shells, blasts and the Valhalla: clean
  AnUseLines(kAmaSentences, AL_COUNT);         // ... with this world's speeches
  SB_ClearBytes(&G, sizeof G);
  G.on = 1;
  G.opacity = -1;
  G.sun.tgt = -1;
  g_graze_jammed = 0;
}

// Mission 15 (paris.c): the mirror alone -- painting, strikes -- every frame.
// the desktop probes: the mirror's state (runtime/probes)
volatile int g_ama_dbg[8];

void AmaSunTick(void) {
  if (!G.on) return;
  G.ticked = AT_GetFrameCount();
  Paint();
  g_ama_dbg[0] = G.mirror;
  g_ama_dbg[1] = G.jammed;
  g_ama_dbg[2] = G.sun.state;
  for (int k = 0; k < 4 && k < NPT; k++) g_ama_dbg[3 + k] = G.pt[k].bg ? G.pt[k].fill + (G.pt[k].off << 12) + (G.pt[k].kind << 16) : -1;
}

void AmaterasuTick(void) {
  if (R32(Shogun(), SH_WORLD_NO) != AMATERASU || !G.on) return;
  G.ticked = AT_GetFrameCount();
  const s32 pt = AnTime();
  Sea();
  // the homecoming: the Valhalla comes in over the stern, the lines
  if (!(G.said & SAID_HOME) && pt >= A_HOME_T) {
    G.said |= SAID_HOME;
    AnValCome();
    AnLine(AL_VOLTA_HOME);
    AnLine(AL_VOLTA_DOORS);
    AnLine(AL_DOC_LATER);
    AnLine(AL_ARGUS_REFIT);
  }
  if (!(G.said & VAL_DOCKED) && pt >= A_VAL_DOCK_T) {
    G.said |= VAL_DOCKED;
    AnValSink();                    // down to her berth, out of sight
  }
  if (!(G.said & SAID_BRIEF) && pt >= A_DOC_BRIEF_T) {
    G.said |= SAID_BRIEF;
    AnLine(AL_DOC_BRIEF);
    G.brief_line = AnLine(AL_DOC_AMATERASU);
  }
  // the launch
  if (!(G.said & SAID_LAUNCH) && pt >= A_LAUNCH_T) { G.said |= SAID_LAUNCH; AnLine(AL_DOC_LAUNCH); }
  if (!(G.said & SAID_RECON) && pt >= A_RECON_T) {
    G.said |= SAID_RECON;
    AnLine(AL_ARGUS_RECON);
    AnLine(AL_DOC_NONE);
  }
  if (!(G.said & SAID_CLEAR) && pt >= A_ORBIT_T - 200) { G.said |= SAID_CLEAR; AnLine(AL_DOC_CLEAR); }
  if (!(G.said & SAID_ORBIT) && pt >= A_ORBIT_T) { G.said |= SAID_ORBIT; AnLine(AL_DOC_ORBIT); }
  Holds(pt);
  Launch(pt);
  Puffs();
  Lesson(pt);
  // north
  if (!(G.said & SAID_PROBE) && pt >= A_RUN_T) {
    G.said |= SAID_PROBE;
    AnLine(AL_DOC_PROBE);
    AnLine(AL_VOLTA_BEHIND);
  }
  if (!(G.said & SAID_EDIN) && pt >= A_RUN_T + 700) { G.said |= SAID_EDIN; AnLine(AL_DOC_EDINBURGH); }
  if (!(G.said & SAID_STORM) && pt >= A_STORM_T) { G.said |= SAID_STORM; AnLine(AL_VOLTA_STORM); }
  Riders(pt);
  Fleet();
  Flights(pt);
  Paint();
  if (G.ending) Ending();
}
