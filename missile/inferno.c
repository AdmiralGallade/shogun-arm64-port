// inferno.c -- Mission 13's final boss: Captain Ignis's magma core, rebuilt -- and the Valhalla.
//
// Mission 2's final boss (homeward/home_cast.py: its scripts, ported and
// upgraded), built and kept together here as the game's own code did it
// (VolcanoBossSpawn/Update), its phases told to the core through R18:
//
//   1 the core    alone: curtain carriers, the ember fan, the needle flower,
//                 forward and back between them (R18 = 0);
//   2 the arms    at 60 % the core shields itself (R18 = 1) and its two arms
//                 come out of their stations to their firing spots: comma
//                 pincers and mines from them, watering hoses and sniper
//                 disks from the core. Both arms must go;
//   3 the ring    the core moves to the centre (R18 = 2) and its four pods
//                 ease in from the corners into a ring round it, awake and
//                 firing (Mission 2's had them asleep until the core fell);
//                 a little later the core is open again and fires its
//                 oscillating spiral (R18 = 3);
//   4 meltdown    at 35 % (R18 = 4): the pods circle the core, open to the
//                 pilot's guns, faster as they're hurt; the core's gap rings
//                 and two-armed spiral at once;
//   5 the end     at 12 % (or if meltdown runs long) the core stops (R18 = 5)
//                 and the Valhalla comes up on station behind the pilot: her
//                 turrets on the pods, her railgun on the core -- it goes
//                 through. Then HQ's escorts and the Bastion (homeward.c), and
//                 the level runs on to its end.
//
// Same rules as missile.c: no libc, no division, engine reached by address.

#include "andaman.h"
#include "homeward_lines.h"
#include "homeward.h"


#define IN_SetActorParent FN(void (*)(u32, u32), 0xc184c)                // ARM

#define ARMS_AT      60             // percent of the core's health: the arms come out
#define MELT_AT      35             // ... meltdown
#define END_AT       12             // ... the Valhalla
#define MELT_MAX     2700           // meltdown's longest (45 s): then the Valhalla anyway
#define RING_WAIT    320            // frames from the ring's forming to the core's opening (Mission 2's)
#define POD_RING     108            // the pods' ring: each corner this far from the centre, across and down
#define ORBIT_R      118            // ... meltdown's orbit round the core
// the flame lance (Mark II): an aiming line from the core's mouth that hunts
// the ship, then a beam that sweeps after it and goes through the shield
// the magma geysers (Mark II): vents opened on the sea round the ship, a pool
// bubbling for a second, then a jet of fire and a stream of fireballs up the
// screen (homeward/home_cast.py's geyser generic does the rest itself)
#define GEYSER_N     3              // vents a volley ...
#define GEYSER_GAP   60             // ... this far apart across the ship's column
#define GEYSER_STEP  16             // ... opened this many frames apart
#define GEYSER_BELOW 34             // ... the pools this far below the ship
#define GEYSER_EVERY 360            // the core's first phase: a volley this often
#define MELT_GEYSER  330            // meltdown's
#define LANCE_SEGS   14             // 40-unit lengths, end to end
#define LANCE_AIM    72             // frames of charging and aiming line (meltdown's twins) ...
#define RING_AIM     96             // ... the ring's single lance, a fairer warning
#define LANCE_FIRE   84             // ... and of beam
#define LANCE_FULL   (20 + 40 * LANCE_SEGS)
#define LANCE_REACH  16             // the aiming line reaches out this many units a frame ...
#define BEAM_REACH   55             // ... the beam, when it fires
#define BEAM_FADE    10             // the beam's last frames: thinning back to a line
enum { EG_FLARE = 5, EG_HIDDEN = 6 };   // the eye glow's animations (home_cast.py): 0-4 growing
#define LANCE_EVERY  330            // the ring's: one lance this often
#define TWIN_EVERY   300            // meltdown's: two, closing like scissors
#define LANCE_HALF   FX(8)          // how close to the beam's line hurts
#define LANCE_GRACE  45
enum { L_OFF, L_AIM, L_FIRE };
// the heat chain (Mark II, the arms phase): while both arms are out at their
// firing spots, a line links them, then a chain of fire burns between them as
// they dive down the screen together and climb back -- round its ends is clear
#define CHAIN_EVERY  420            // frames between dives
#define CHAIN_WARN   60             // the linking line, before it burns
#define CHAIN_DOWN   80             // ... the dive (and the climb back)
#define CHAIN_HOLD   30             // ... held at the bottom of it
#define CHAIN_DEPTH  190            // how far down they dive
#define ARM_SPOT_Y   100            // the arms' firing spots (VolcanoBossSpawn's): (120, 100), (280, 100)
enum { C_OFF, C_WARN, C_DOWN, C_HOLD, C_UP };
enum { LA_AIM, LA_BURN, LA_HIDDEN };   // the lance's animations (home_cast.py)

enum { P_CORE = 1, P_ARMS, P_RING, P_MELT, P_VAL, P_KILL, P_END, P_DONE };

static const signed short kPodFromX[4] = { -70, 470, -70, 470 };
static const signed short kPodFromY[4] = { -70, -70, 470, 470 };
static const signed char kPodSX[4] = { -1, 1, -1, 1 };
static const signed char kPodSY[4] = { -1, -1, 1, 1 };

struct {
  u32 core, arm[2], pod[4];
  s32 maxhp, x, y;
  s32 pod_x[4], pod_y[4];
  s32 orbit;                        // meltdown: the pods' angle round the core (2^24 to the turn)
  int phase, t, line;
  int vstep;                        // the Valhalla's finish, step by step, each on its line
  int pod_shot[4];                  // the Valhalla's shell for this pod: frames since it went
  int pod_wait[4];                  // ... frames since it was asked for (a turret off the screen never fires)
  int core_shut, pods_shut;         // shielded (invincibility is a countdown, not a flag: set every frame)
  // the Mark II's look (art_ignis.py), drawn over its invisible badguys, and its lances
  u32 core_vis, pod_vis[4], seg[2][LANCE_SEGS], glow[2];
  int ln_state[2], ln_t[2], ln_twin, ln_next, grace;
  int ln_eye[2];                    // which eye each lance fires from: -1 left, 1 right
  int ln_side;                      // the ring's single lance: the eye it used last
  int ch_state, ch_t, ch_next;      // the heat chain
  int gy_next, gy_k, gy_t;          // the geysers: the next volley, its next vent, frames to it
  s32 gy_x[GEYSER_N], gy_y;
  s32 ln_ang[2], ln_dir[2];
} g_inferno;

static int Pct(void) {
  if (!g_inferno.core || !g_inferno.maxhp) return 0;
  const s32 hp = SB_GetBadGuyHealth(g_inferno.core);
  return hp * 100 > g_inferno.maxhp * MELT_AT ? (hp * 100 > g_inferno.maxhp * ARMS_AT ? 100 : 50)
                                       : (hp * 100 > g_inferno.maxhp * END_AT ? 20 : 0);
}

static s32 InHealth(u32 boss) {
  (void)boss;
  if (g_inferno.phase == P_ARMS) {
    s32 hp = 0;
    for (int k = 0; k < 2; k++)
      if (g_inferno.arm[k] && Alive(g_inferno.arm[k])) hp += SB_GetBadGuyHealth(g_inferno.arm[k]);
    return hp;
  }
  if (g_inferno.phase >= P_KILL || !g_inferno.core || !Alive(g_inferno.core)) return 0;
  return SB_GetBadGuyHealth(g_inferno.core);
}

static void InRender(u32 boss, s32 x, s32 y) { (void)boss; (void)x; (void)y; }

static void Phase(u32 boss, int p) {
  g_inferno.phase = p;
  g_inferno.t = 0;
  if (p == P_ARMS) {
    s32 hp = 0;
    for (int k = 0; k < 2; k++)
      if (g_inferno.arm[k]) hp += SB_GetBadGuyHealth(g_inferno.arm[k]);
    R32(boss, B_MAXHP) = hp;
  } else if (p == P_RING) {
    R32(boss, B_MAXHP) = g_inferno.maxhp;
  }
}

static void InSpawn(u32 boss) {
  SB_ClearBytes(&g_inferno, sizeof g_inferno);
  g_inferno.core = SB_SpawnBadGuy(BH, "*homeward/boss/core", FX(200), FX(-60));
  const u32 ca = Act(g_inferno.core);
  for (int k = 0; k < 2; k++) {
    g_inferno.arm[k] = SB_SpawnBadGuy(BH, "*homeward/boss/arm", FX(k ? 350 : 50), FX(-60));
    const u32 a = Act(g_inferno.arm[k]);
    IN_SetActorParent(a, ca);
    // its station, and where it fires from (VolcanoBossSpawn's registers)
    SB_SetActorRegister(a, 10, FX(k ? 312 : 88));
    SB_SetActorRegister(a, 11, FX(50));
    SB_SetActorRegister(a, 12, FX(k ? 280 : 120));
    SB_SetActorRegister(a, 13, FX(100));
  }
  for (int k = 0; k < 4; k++) {
    g_inferno.pod_x[k] = FX(kPodFromX[k]);
    g_inferno.pod_y[k] = FX(kPodFromY[k]);
    g_inferno.pod[k] = SB_SpawnBadGuy(BH, "*homeward/boss/pod", g_inferno.pod_x[k], g_inferno.pod_y[k]);
    IN_SetActorParent(Act(g_inferno.pod[k]), ca);
  }
  g_inferno.pods_shut = 1;
  // its look, made after its badguys and bottom to top: the body, the pods, the lances over them
  g_inferno.core_vis = SpawnPart("*homeward/boss/core_vis", FX(200), FX(-60));
  for (int k = 0; k < 4; k++) g_inferno.pod_vis[k] = SpawnPart("*homeward/boss/pod_vis", FX(-400), FX(-400));
  for (int i = 0; i < 2; i++) {
    g_inferno.glow[i] = SpawnPart("*homeward/boss/eyeglow", FX(-400), FX(-400));
    PartFrame(g_inferno.glow[i], EG_HIDDEN);
  }
  for (int i = 0; i < 2; i++)
    for (int k = 0; k < LANCE_SEGS; k++) {
      g_inferno.seg[i][k] = SpawnPart("*homeward/boss/lance", FX(-400), FX(-400));
      PartFrame(g_inferno.seg[i][k], LA_HIDDEN);
    }
  g_inferno.ln_next = 120;
  g_inferno.gy_next = 240;
  g_inferno.gy_k = GEYSER_N;
  g_inferno.maxhp = SB_GetBadGuyHealth(g_inferno.core);
  R32(boss, B_STATE) = 1;
  R32(boss, B_MAXHP) = g_inferno.maxhp;
  SB_PausePartition(BH, 1);
  Phase(boss, P_CORE);
  g_autopilot_target = g_inferno.core;
  AnLine(HL_IGNIS_CORE);            // Told you I'd be waiting!
}

static void Pods(void) {
  for (int k = 0; k < 4; k++) {
    if (!g_inferno.pod[k]) continue;
    if (!Alive(g_inferno.pod[k])) { g_inferno.pod[k] = 0; continue; }
    s32 wx = g_inferno.pod_x[k], wy = g_inferno.pod_y[k];
    if (g_inferno.phase == P_RING || g_inferno.phase == P_MELT) {
      if (g_inferno.phase == P_RING) {
        wx = FX(200 + kPodSX[k] * POD_RING);
        wy = FX(200 + kPodSY[k] * POD_RING);
      } else {
        // round the core, a quarter turn apart, faster as they're hurt
        s32 ox, oy;
        AnRot(ORBIT_R, 0, g_inferno.orbit + k * (TURN / 4), &ox, &oy);
        wx = g_inferno.x + ox;
        wy = g_inferno.y + oy;
      }
      g_inferno.pod_x[k] += Ease(wx - g_inferno.pod_x[k], FX(2.5));
      g_inferno.pod_y[k] += Ease(wy - g_inferno.pod_y[k], FX(2.5));
      // how far the ship is: near, far, too far (the too-far homers)
      const s32 d = AnDist(PlayerX() - g_inferno.pod_x[k], PlayerY() - g_inferno.pod_y[k]);
      const int far = d <= 150 ? 0 : (d <= 200 ? 1 : 2);
      SetReg(g_inferno.pod[k], 16, FX(g_inferno.phase == P_RING && far > 1 ? 1 : far));
    }
    Place(g_inferno.pod[k], g_inferno.pod_x[k], g_inferno.pod_y[k]);
  }
}

// ---- the look -----------------------------------------------------------------------------------
static int CoreState(void) {
  const int p = g_inferno.phase;
  if (p == P_ARMS) return CORE_SHIELD;
  if (p == P_MELT || p == P_VAL || p == P_KILL) return (g_inferno.t >> 2) & 1 ? CORE_MELT_B : CORE_MELT_A;
  if (g_inferno.ln_state[0] == L_FIRE || g_inferno.ln_state[1] == L_FIRE) return CORE_FIRE;
  if (g_inferno.ln_state[0] == L_AIM) return CORE_CHARGE;
  const s32 a = GetReg(g_inferno.core, 14) >> 16;       // its scripted animation: Mission 2's frames
  if (a >= 2 && a <= 7) return CORE_CHARGE;            // (curtains)
  if (a >= 8 && a <= 13) return CORE_FIRE;             // (the flower, the spiral)
  return CORE_IDLE;
}

static void Look(void) {
  if (g_inferno.core_vis) {
    if (g_inferno.core) {
      PlacePart(g_inferno.core_vis, g_inferno.x, g_inferno.y - FX(IGNIS_BODY_DY));
      PartFrame(g_inferno.core_vis, CoreState());
    } else {
      PlacePart(g_inferno.core_vis, FX(-400), FX(-400));
      DropPart(&g_inferno.core_vis);
    }
  }
  for (int k = 0; k < 4; k++) {
    if (!g_inferno.pod_vis[k]) continue;
    if (!g_inferno.pod[k]) {
      PlacePart(g_inferno.pod_vis[k], FX(-400), FX(-400));
      DropPart(&g_inferno.pod_vis[k]);
      continue;
    }
    // Mission 2's 26 steps (its R15) to our five frames: in, out, far, further, too far
    const s32 a = GetReg(g_inferno.pod[k], 15) >> 16;
    PlacePart(g_inferno.pod_vis[k], g_inferno.pod_x[k], g_inferno.pod_y[k]);
    PartFrame(g_inferno.pod_vis[k], a <= 3 ? 0 : (a <= 6 ? 1 : (a <= 12 ? 2 : (a <= 19 ? 3 : 4))));
  }
}

// ---- the flame lance ---------------------------------------------------------------------------
static s32 ToShip(s32 mx, s32 my) { return AnAngle(mx, my, PlayerX(), PlayerY()); }

// Beam i's lengths laid from (mx, my) along `ang` for `len` units (a 40-unit
// length centred every 40 from 20 out), showing `anim`; the rest hidden.
static void BeamDraw(int i, s32 mx, s32 my, s32 ang, s32 len, int anim) {
  const s32 ux = SB_Cos(ang), uy = SB_Sin(ang);
  for (int k = 0; k < LANCE_SEGS; k++) {
    const u32 a = g_inferno.seg[i][k];
    if (!a) continue;
    s32 x = mx + ux * (20 + 40 * k), y = my + uy * (20 + 40 * k);
    const int in = anim != LA_HIDDEN && 20 + 40 * k <= len + 10 && y < FX(470) && y > FX(-40) && x > FX(-40)
                   && x < FX(440);
    if (!in) { x = FX(-400); y = FX(-400); }
    PlacePart(a, x, y);
    AnTurn(a, ang);
    PartFrame(a, in ? anim : LA_HIDDEN);
  }
}

// Where lance i starts: its eye.
static void Eye(int i, s32* x, s32* y) {
  *x = g_inferno.x + g_inferno.ln_eye[i] * FX(IGNIS_EYE_X);
  *y = g_inferno.y + FX(IGNIS_EYE_DY);
}

// How far lance i reaches, and whether it burns: the aiming line reaching out
// as the eye charges; the beam shooting out when it fires, thinning back to a
// line at its end.
static s32 LanceLen(int i, int* burning) {
  const int t = g_inferno.ln_t[i];
  *burning = 0;
  if (g_inferno.ln_state[i] == L_AIM) {
    const s32 len = 30 + LANCE_REACH * t;
    return len > LANCE_FULL ? LANCE_FULL : len;
  }
  if (g_inferno.ln_state[i] == L_FIRE) {
    *burning = t < LANCE_FIRE - BEAM_FADE;
    const s32 len = 40 + BEAM_REACH * t;
    return len > LANCE_FULL ? LANCE_FULL : len;
  }
  return 0;
}

static void LanceDraw(int i) {
  s32 mx, my;
  Eye(i, &mx, &my);
  int burning;
  const s32 len = LanceLen(i, &burning);
  const int st = g_inferno.ln_state[i];
  BeamDraw(i, mx, my, g_inferno.ln_ang[i], len, st == L_OFF ? LA_HIDDEN : (burning ? LA_BURN : LA_AIM));
  // the glow at its eye: growing as it charges, flaring as it fires
  const u32 g = g_inferno.glow[i];
  if (!g) return;
  int f = EG_HIDDEN;
  if (st == L_AIM) {
    const int aim = g_inferno.ln_twin ? LANCE_AIM : RING_AIM;
    f = 0;
    while (f < 4 && g_inferno.ln_t[i] * 5 >= aim * (f + 1)) f++;
  } else if (st == L_FIRE) {
    f = burning ? EG_FLARE : 2;
  }
  PlacePart(g, f == EG_HIDDEN ? FX(-400) : mx, f == EG_HIDDEN ? FX(-400) : my);
  PartFrame(g, f);
}

// The ship on the beam's line (within LANCE_HALF of it, along its length): a
// hit -- struck from here, so it goes through the shield (the Tenshu cannon's way).
static void BeamHits(s32 mx, s32 my, s32 ang, s32 len) {
  if (g_inferno.grace > 0 || InCutscene()) return;
  const u32 sh = Shogun();
  if (*(volatile unsigned short*)(sh + SH_HITLOCK) || R32(sh, SH_HITLOCK2)) return;
  const s32 dx = PlayerX() - mx, dy = PlayerY() - my;
  const s32 c = SB_Cos(ang), s = SB_Sin(ang);
  const s32 along = SB_MulFP(dx, c) + SB_MulFP(dy, s);
  s32 perp = SB_MulFP(dy, c) - SB_MulFP(dx, s);
  if (perp < 0) perp = -perp;
  if (along < 0 || along > FX(len) || perp > LANCE_HALF) return;
  SB_StrikePlayer(sh + SH_PLAYER, 0, sh + SH_HUD);
  g_inferno.grace = LANCE_GRACE;
}

static void LanceHits(int i) {
  s32 mx, my;
  Eye(i, &mx, &my);
  int burning;
  const s32 len = LanceLen(i, &burning);
  if (burning) BeamHits(mx, my, g_inferno.ln_ang[i], len);
}

// The heat chain, between the arms (beam 1: the lance's second, idle while the arms fight).
static void Chain(void) {
  const u32 a0 = g_inferno.arm[0], a1 = g_inferno.arm[1];
  const int both = a0 && a1 && Alive(a0) && Alive(a1);
  s32 x0 = 0, y0 = 0, x1 = 0, y1 = 0;
  if (both) {
    SB_GetActorPosition(Act(a0), &x0, &y0);
    SB_GetActorPosition(Act(a1), &x1, &y1);
  }
  // both out at their spots, firing (the core's R16), and no one talking
  const int ready = both && GetReg(g_inferno.core, 16) != 0 && AnAbs(y0 - FX(ARM_SPOT_Y)) < FX(4)
                    && AnAbs(y1 - FX(ARM_SPOT_Y)) < FX(4);
  if (g_inferno.phase != P_ARMS || !both || (g_inferno.ch_state == C_OFF && (!ready || InCutscene()))
      || (g_inferno.ch_state != C_OFF && GetReg(g_inferno.core, 16) == 0)) {
    if (g_inferno.ch_state != C_OFF && both) {
      // let go mid-dive: back to their spots, so their scripts carry on from there
      Place(a0, FX(120), FX(ARM_SPOT_Y));
      Place(a1, FX(280), FX(ARM_SPOT_Y));
    }
    g_inferno.ch_state = C_OFF;
    if (g_inferno.ch_next > 0) g_inferno.ch_next--;
    if (g_inferno.ln_state[1] == L_OFF) BeamDraw(1, 0, 0, 0, 0, LA_HIDDEN);
    return;
  }
  g_inferno.ch_t++;
  s32 dive = 0;
  switch (g_inferno.ch_state) {
  case C_OFF:
    if (--g_inferno.ch_next > 0) { BeamDraw(1, 0, 0, 0, 0, LA_HIDDEN); return; }
    g_inferno.ch_state = C_WARN;
    g_inferno.ch_t = 0;
    break;
  case C_WARN:
    if (g_inferno.ch_t >= CHAIN_WARN) { g_inferno.ch_state = C_DOWN; g_inferno.ch_t = 0; SB_ShakeWorld(); }
    break;
  case C_DOWN:
    dive = g_inferno.ch_t * (FX(CHAIN_DEPTH) / CHAIN_DOWN);
    if (g_inferno.ch_t >= CHAIN_DOWN) { g_inferno.ch_state = C_HOLD; g_inferno.ch_t = 0; }
    break;
  case C_HOLD:
    dive = FX(CHAIN_DEPTH);
    if (g_inferno.ch_t >= CHAIN_HOLD) { g_inferno.ch_state = C_UP; g_inferno.ch_t = 0; }
    break;
  default:                                     // C_UP
    dive = FX(CHAIN_DEPTH) - g_inferno.ch_t * (FX(CHAIN_DEPTH) / CHAIN_DOWN);
    if (g_inferno.ch_t >= CHAIN_DOWN) {
      dive = 0;
      g_inferno.ch_state = C_OFF;
      g_inferno.ch_next = CHAIN_EVERY;
    }
    break;
  }
  if (dive < 0) dive = 0;
  // the arms held on their spots, dived down by `dive`
  Place(a0, FX(120), FX(ARM_SPOT_Y) + dive);
  Place(a1, FX(280), FX(ARM_SPOT_Y) + dive);
  // the chain from arm to arm (inside their bodies' edges)
  const s32 cy = FX(ARM_SPOT_Y) + dive, cx0 = FX(120 + 24), len = 160 - 48;
  const int burning = g_inferno.ch_state != C_WARN && g_inferno.ch_state != C_OFF;
  BeamDraw(1, cx0, cy, 0, len, g_inferno.ch_state == C_OFF ? LA_HIDDEN : (burning ? LA_BURN : LA_AIM));
  if (burning) BeamHits(cx0, cy, 0, len);
}

static void LanceStart(int twin) {
  g_inferno.ln_twin = twin;
  // the twins from both eyes; the single lance from one, the other next time
  g_inferno.ln_side = g_inferno.ln_side > 0 ? -1 : 1;
  g_inferno.ln_eye[0] = twin ? -1 : g_inferno.ln_side;
  g_inferno.ln_eye[1] = 1;
  for (int i = 0; i < 1 + twin; i++) {
    s32 mx, my;
    Eye(i, &mx, &my);
    const s32 at = ToShip(mx, my);
    g_inferno.ln_state[i] = L_AIM;
    g_inferno.ln_t[i] = 0;
    // a twin pair opens either side of the ship and closes on it like scissors
    g_inferno.ln_dir[i] = twin ? (i ? -1 : 1) : 0;
    g_inferno.ln_ang[i] = (at - g_inferno.ln_dir[i] * (TURN / 11)) & (TURN - 1);
  }
}

static void Lances(void) {
  if (g_inferno.grace > 0) g_inferno.grace--;
  const int p = g_inferno.phase;
  const int may = (p == P_RING && g_inferno.t > RING_WAIT) || p == P_MELT;
  if (!may || !g_inferno.core) {
    for (int i = 0; i < 2; i++) {
      g_inferno.ln_state[i] = L_OFF;
      if (i == 0 || p != P_ARMS) LanceDraw(i);    // (beam 1 is the arms' chain)
      else PartFrame(g_inferno.glow[i], EG_HIDDEN);
    }
    return;
  }
  if (g_inferno.ln_state[0] == L_OFF && g_inferno.ln_state[1] == L_OFF) {
    if (--g_inferno.ln_next <= 0 && !InCutscene()) {
      LanceStart(p == P_MELT);
      g_inferno.ln_next = p == P_MELT ? TWIN_EVERY : LANCE_EVERY;
    }
  }
  for (int i = 0; i < 2; i++) {
    if (g_inferno.ln_state[i] == L_OFF) {
      LanceDraw(i);
      continue;
    }
    s32 mx, my;
    Eye(i, &mx, &my);
    const s32 at = ToShip(mx, my);
    g_inferno.ln_t[i]++;
    if (g_inferno.ln_state[i] == L_AIM) {
      // the single lance hunts the ship while it aims; the twins hold their opening
      if (!g_inferno.ln_twin) g_inferno.ln_ang[i] = AnTurnTo(g_inferno.ln_ang[i], at, TURN / 220);
      if (g_inferno.ln_t[i] >= (g_inferno.ln_twin ? LANCE_AIM : RING_AIM)) {
        g_inferno.ln_state[i] = L_FIRE;
        g_inferno.ln_t[i] = 0;
        SB_ShakeWorld();
      }
    } else {
      if (!g_inferno.ln_twin) g_inferno.ln_ang[i] = AnTurnTo(g_inferno.ln_ang[i], at, TURN / 900);
      else g_inferno.ln_ang[i] = (g_inferno.ln_ang[i] + g_inferno.ln_dir[i] * (TURN / 760)) & (TURN - 1);
      LanceHits(i);
      if (g_inferno.ln_t[i] >= LANCE_FIRE) g_inferno.ln_state[i] = L_OFF;
    }
    LanceDraw(i);
  }
}

// ---- the magma geysers ---------------------------------------------------------------------------
// A volley: GEYSER_N vents across the ship's column, just below it, the far
// ones first and the one under the ship last, each a moment after the last --
// the ship's way out is sideways, before the middle one goes. Kept on the
// playfield's 240-wide window round the ship, never the edges' (the phone
// shows no more), and clear of the bottom edge.
static void Geysers(void) {
  const int p = g_inferno.phase;
  if ((p != P_CORE && p != P_MELT) || !g_inferno.core) {
    g_inferno.gy_k = GEYSER_N;
    return;
  }
  if (g_inferno.gy_k < GEYSER_N) {
    if (--g_inferno.gy_t > 0) return;
    const s32 x = g_inferno.gy_x[g_inferno.gy_k++];
    SB_SpawnGeneric(BH, "*homeward/boss/geyser", x, g_inferno.gy_y - FX(GEYSER_FOOT));
    g_inferno.gy_t = GEYSER_STEP;
    return;
  }
  if (--g_inferno.gy_next > 0 || InCutscene()) return;
  g_inferno.gy_next = p == P_MELT ? MELT_GEYSER : GEYSER_EVERY;
  const s32 px = PlayerX();
  s32 y = PlayerY() + FX(GEYSER_BELOW);
  if (y > FX(366)) y = FX(366);
  if (y < FX(230)) y = FX(230);
  g_inferno.gy_y = y;
  // the outer two first (either order), the ship's own column last
  const int flip = SB_RangedRand(0, 2);
  g_inferno.gy_x[0] = px + FX(flip ? GEYSER_GAP : -GEYSER_GAP);
  g_inferno.gy_x[1] = px - FX(flip ? GEYSER_GAP : -GEYSER_GAP);
  g_inferno.gy_x[2] = px;
  for (int k = 0; k < GEYSER_N; k++) {
    if (g_inferno.gy_x[k] < FX(40)) g_inferno.gy_x[k] = FX(40);
    if (g_inferno.gy_x[k] > FX(360)) g_inferno.gy_x[k] = FX(360);
  }
  g_inferno.gy_k = 0;
  g_inferno.gy_t = 1;
}

static int PodsLeft(void) {
  int n = 0;
  for (int k = 0; k < 4; k++) n += g_inferno.pod[k] != 0;
  return n;
}

static u32 AnyArm(void) {
  for (int k = 0; k < 2; k++)
    if (g_inferno.arm[k] && Alive(g_inferno.arm[k])) return g_inferno.arm[k];
  return 0;
}

static void InUpdate(u32 boss) {
  g_inferno.t++;
  SpeechPump();
  if (g_inferno.phase < P_VAL) BossWards();
  if (g_inferno.phase == P_DONE) return;
  if (g_inferno.core && Alive(g_inferno.core)) SB_GetActorPosition(Act(g_inferno.core), &g_inferno.x, &g_inferno.y);
  if (g_inferno.phase < P_VAL) ShipShield(InCutscene());
  for (int k = 0; k < 2; k++) {
    if (g_inferno.arm[k] && !Alive(g_inferno.arm[k])) g_inferno.arm[k] = 0;
    // shut until it is out at its firing spot (its R14: Mission 2's InvincibleRegister)
    if (g_inferno.arm[k]) Guard(g_inferno.arm[k], GetReg(g_inferno.arm[k], 14) != 0);
  }
  // (through the finish too: the ship's own fire killed it before her railgun
  // could, and Ignis's last words swept the finish's lines away unseen. The
  // railgun lifts the guard as it strikes: andaman.c)
  if (g_inferno.core && g_inferno.phase <= P_KILL) Guard(g_inferno.core, g_inferno.core_shut);
  for (int k = 0; k < 4; k++)
    if (g_inferno.pod[k] && Alive(g_inferno.pod[k])) Guard(g_inferno.pod[k], g_inferno.pods_shut);
  switch (g_inferno.phase) {
  case P_CORE:
    if (Pct() <= 50) {
      SetReg(g_inferno.core, 18, FX(1));
      g_inferno.core_shut = 1;
      g_inferno.ch_next = 240;
      Phase(boss, P_ARMS);
      AnLine(HL_IGNIS_ARMS);
      AnLine(HL_DOC_ARMS);
    }
    break;
  case P_ARMS:
    g_autopilot_target = AnyArm();
    if (!AnyArm()) {
      SetReg(g_inferno.core, 18, FX(2));
      for (int k = 0; k < 4; k++) if (g_inferno.pod[k]) SetReg(g_inferno.pod[k], 18, FX(1));
      Phase(boss, P_RING);
      AnLine(HL_IGNIS_RING);
    }
    break;
  case P_RING:
    g_autopilot_target = g_inferno.core;
    if (g_inferno.t == RING_WAIT) {
      SetReg(g_inferno.core, 18, FX(3));
      g_inferno.core_shut = 0;
      g_inferno.ln_next = 50;                    // (its first lance, soon after it opens)
    }
    if (g_inferno.t > RING_WAIT && Pct() <= 20) {
      SetReg(g_inferno.core, 18, FX(4));
      g_inferno.pods_shut = 0;
      if (g_inferno.ln_next > 70) g_inferno.ln_next = 70;   // (the twins, soon)
      Phase(boss, P_MELT);
      AnLine(HL_IGNIS_MELT);
    }
    break;
  case P_MELT: {
    // the orbit speeds up as the pods go
    const int left = PodsLeft();
    g_inferno.orbit = (g_inferno.orbit + (left >= 3 ? TURN / 420 : (left == 2 ? TURN / 300 : TURN / 200))) & (TURN - 1);
    if (Pct() == 0 || g_inferno.t > MELT_MAX) {
      SetReg(g_inferno.core, 18, FX(5));
      g_inferno.core_shut = g_inferno.pods_shut = 1;
      Phase(boss, P_VAL);
      g_autopilot_target = 0;
      g_inferno.vstep = 0;
      // (the fight's chatter still waiting goes: behind it, the finish's lines came late
      // and the railgun waited on them)
      AnDropQueued();
      g_inferno.line = AnLine(HL_VOLTA_ARRIVE);   // "Not today. Valhalla, on station!" -- then she comes
      g_an_spare = g_inferno.core;               // (her turrets' shells finished it before her railgun could)
    }
    break;
  }
  case P_VAL:
    // Each step on its line: Volta's call, and up she comes; on station, Ignis
    // sees her, and her turrets take the pods, a shell each; then "Allow me",
    // and her railgun turns on the core, charges and fires.
    ShipShield(1);
    if (g_inferno.vstep == 0) {
      if (!AnLineUp(g_inferno.line) && g_inferno.t < 900) break;
      AnValCome();
      g_inferno.vstep = 1;
      break;
    }
    if (g_inferno.vstep == 1) {
      if (!AnValThere()) break;
      g_inferno.line = AnLine(HL_IGNIS_SEE);      // "That's... a flying battleship."
      g_inferno.vstep = 2;
      break;
    }
    if (g_inferno.vstep == 2 && !AnLineUp(g_inferno.line) && g_inferno.t < 1500) break;
    g_inferno.vstep = 3;
    {
      for (int k = 0; k < 4; k++) {
        if (!g_inferno.pod[k]) continue;
        // (her turret k's shell; or, should it not have fired in a while -- off the
        // screen where she stands, or the pod's old place out of its reach: the
        // finish hung on it -- a shell from the others' fire takes it all the same)
        const int stuck = ++g_inferno.pod_wait[k] > 150 + 30 * k;
        if (g_inferno.pod_shot[k] == 0 && !AnValBusy(k) && !stuck) {
          AnValOrder(k, g_inferno.pod_x[k], g_inferno.pod_y[k], SHOT_BIG, TAG_NONE, 20);
          g_inferno.pod_shot[k] = 1;
        } else if (stuck || (g_inferno.pod_shot[k] && !AnValBusy(k) && ++g_inferno.pod_shot[k] > 26)) {
          AnBlast(g_inferno.pod_x[k], g_inferno.pod_y[k]);
          Remove(g_inferno.pod[k]);
          g_inferno.pod[k] = 0;
        }
      }
    }
    if (!PodsLeft() && g_inferno.core) {
      g_inferno.line = AnLine(HL_VOLTA_FIRE);     // "Nice softening up, kid. Allow me." -- then the shot
      AnValRailOn(g_inferno.core, g_inferno.line);
      Phase(boss, P_KILL);
    }
    break;
  case P_KILL:
    ShipShield(1);
    if (g_inferno.core && !Alive(g_inferno.core)) {
      g_inferno.core = 0;
      g_an_spare = 0;
      SB_ShakeWorld();
      AnBlast(g_inferno.x, g_inferno.y);
      AnLineNow(HL_IGNIS_DOWN);     // Not again! Not to a doll ship!
      HomeClearSea();
    }
    if (!g_inferno.core && AnValRailDone()) {
      Phase(boss, P_END);
      AnValSink();                  // she drops back; the escorts and the Bastion come after (homeward.c)
      HomeEnding();
    }
    break;
  case P_END:
    ShipShield(1);
    if (HomeEndingDone()) {
      g_inferno.phase = P_DONE;
      R32(boss, B_STATE) = 0;       // the engine calls death next
    }
    break;
  }
  Pods();
  Lances();
  Chain();
  Geysers();
  Look();
}

static void InDeath(u32 boss) {
  (void)boss;
  DropPart(&g_inferno.core_vis);
  for (int k = 0; k < 4; k++) DropPart(&g_inferno.pod_vis[k]);
  for (int i = 0; i < 2; i++) {
    DropPart(&g_inferno.glow[i]);
    for (int k = 0; k < LANCE_SEGS; k++) DropPart(&g_inferno.seg[i][k]);
  }
  ShipShield(0);
  g_autopilot_target = 0;
  AnValLeave();
  SB_PausePartition(BH, 0);         // on to the level's end
}

void InfernoBossInit(u32 boss) {
  SB_ClearBytes((void*)boss, BOSS_SIZE);
  R32(boss, B_SPAWN + 0x00) = (s32)(u32)InSpawn;
  R32(boss, B_SPAWN + 0x04) = (s32)(u32)InUpdate;
  R32(boss, B_SPAWN + 0x08) = (s32)(u32)InRender;
  R32(boss, B_SPAWN + 0x0c) = (s32)(u32)InRender;
  R32(boss, B_SPAWN + 0x10) = (s32)(u32)InDeath;
  R32(boss, B_HEALTH) = (s32)(u32)InHealth;
}
