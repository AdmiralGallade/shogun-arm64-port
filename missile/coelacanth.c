// coelacanth.c -- Mission 10's boss: Dr. Marrow's Specimen Nine, and what
// she came for.
//
// A living submarine grown round a hunter-killer's hull, bow down the screen
// at the fleet. The pilot can drive her off, but not stop her:
//
//   rise      she surfaces ahead of the fleet: a shape under the water,
//             breaking the surface, up; Dr. Marrow taunts;
//   phase 1   her guns: two bow tubes (torpedoes at the Bastion -- shoot
//             them down -- and seeker torpedoes that hunt the ship), two
//             dorsal bio-turrets (streams and fans of her starlets), the
//             angler's lure (rings to graze, spirals, and lanterns: bio-
//             lights that drift after the ship until shot down), and two
//             drone bays in her flanks that slide open to launch a drone --
//             and can only be hurt while open. Her hull can't be hurt while
//             any of them stands;
//   dive      every gun gone, she dives: a shape sweeping under the fleet,
//             loosing torpedoes, and comes up battered, her hull open;
//   phase 2   the hull itself: starbursts, curtains, her drones let go,
//             spirals, torpedoes; and her jaws, gaping now and then on the
//             inverted entropy core in her throat -- Dr. Marrow's own work,
//             the trick she later turns on the ships -- turn about: a fan of
//             seekers, or the Entropy Breath: the core charging draws the
//             ship in towards her jaws while an aiming line hunts it, then a
//             beam that sweeps after it and goes through the shield. A hit
//             down her throat counts twice. At 70 % a frenzy: lanterns rising from
//             the deep all round, everything faster. Past half her strength,
//             she has had enough: "I didn't come all this way for you" -- a
//             crash dive, a spread into the Pike on her way down, and she is
//             gone from the sonar;
//   boarding  she comes up alongside the Bastion (the fleet eased up the
//             screen), her arm reaching for the bridge. Argus orders his
//             mind drive purged; she inverts the ships' entropy engines and
//             every weapon jams (the pilot's trigger held off: missile.c); the claw
//             closes on him and she dives with him. The Shogun gloats;
//   too late  she slips away under the water; the Valhalla sweeps in from
//             the side, only her railgun's end of her on the screen, the
//             gun swinging on as Volta calls it -- charging as the shape
//             fades -- and the slug hits the sea just as she is gone. HQ's orders: get him back, or
//             make sure of him;
//   the dive  weapons back, the pilot sets down in the Valhalla's bow, and
//             her prototype submersible goes over the side and down. The
//             level ends (Mission 11 is down there).
//
// Lines are spoken by name (andaman.c's AnLine, this world's speeches) and
// the set pieces wait for them.

#include "andaman.h"
#include "sunda_lines.h"
#include "sunda.h"
#include "glow.h"
#include "sunda_glow.h"


#define CX           FX(200)
#define SUB_Y        120            // her station, bow down: her centre this far down
#define TUBE_EVERY   200
#define TURRET_EVERY 120
#define LURE_EVERY   180
#define HULL_EVERY   120
#define ESCAPE_AT    45             // she leaves at this percent of her hull's strength
#define DIVE_T       420
#define NSEG         5              // lengths of her boarding arm
#define SEG_LEN      38
#define NFX          10

int g_arm_hits;                     // hits on the boarding arm (runtime/probes/ending.py)
u32 g_sunda_spare;                  // sunda.c: the fleet holds its fire on this (her hull, while it can't be hurt)
extern int g_weapons_jammed;        // missile.c: the trigger held off ...
extern int g_hide_player;           // ... and the ship not drawn

enum { P_TUBE0, P_TUBE1, P_TUR0, P_TUR1, P_LURE, P_BAY0, P_BAY1, NPARTS };
static const signed short kPartX[NPARTS] = { COEL_TUBE0_X, COEL_TUBE1_X, COEL_TURRET0_X, COEL_TURRET1_X, COEL_LURE_X,
                                             COEL_BAY0_X, COEL_BAY1_X };
static const signed short kPartY[NPARTS] = { COEL_TUBE0_Y, COEL_TUBE1_Y, COEL_TURRET0_Y, COEL_TURRET1_Y, COEL_LURE_Y,
                                             COEL_BAY0_Y, COEL_BAY1_Y };
static const char* const kPartVis[NPARTS] = { "*sunda/boss/tube_vis", "*sunda/boss/tube_vis", "*sunda/boss/turret_vis",
                                              "*sunda/boss/turret_vis", "*sunda/boss/lure_vis", "*sunda/boss/bay_vis",
                                              "*sunda/boss/bay_vis" };
static const char* const kPartGun[NPARTS] = { "*sunda/boss/tube", "*sunda/boss/tube", "*sunda/boss/turret",
                                              "*sunda/boss/turret", "*sunda/boss/lure", "*sunda/boss/bay",
                                              "*sunda/boss/bay" };
// the attacks (sunda_cast.py CMDS)
enum { K_TORP = 1, K_STREAM, K_FAN, K_RING, K_SPIRAL, K_CURTAIN, K_REMORAS, K_BURST, K_SPRAY, K_SEEKERS,
       K_LANTERNS, K_BAY, K_GULP,
       K_XSNIPE, K_XPINCER, K_XTWIST, K_XSNAKE, K_XGAP };   // hard mode's (beach/hardkit.py)
#define BAY_SHUT     230            // a bay shut this long, then open ...
#define BAY_OPEN     130            // ... this long (only now can it be hurt)
#define MAW_SHUT     260            // her jaws shut, then gaping ...
#define MAW_OPEN     150
#define MAW_HP       30000          // (her jaws' emitter, topped back up: the damage goes to the hull)
#define FRENZY_AT    70             // at this percent of her hull: lanterns from the deep, everything faster
#define VAL_X0       FX(490)        // the Valhalla in from the right ...
#define VAL_X1       FX(318)        // ... only so far: her railgun on the screen
#define VAL_Y        FX(372)
#define SD_FIRST     420            // a surprise dive this far into a phase ...
#define SD_EVERY     600            // ... and then every so often (P2: less; the frenzy: less again)
#define SD_DOWN      45
#define SD_PROWL     150
#define SD_MARK      55
#define DOCK_X       FX(120)        // the pilot's ship taken aboard the Valhalla here
#define DOCK_Y       FX(250)
static const unsigned char kHullCycle[6] = { K_BURST, K_CURTAIN, K_REMORAS, K_SPIRAL, K_TORP, K_SPRAY };
// hard mode (docs/HARD_MODE.md): the snake and drifting-gap rings from her hull too
static const unsigned char kHullCycleH[8] = { K_BURST, K_XSNAKE, K_CURTAIN, K_REMORAS, K_XGAP, K_SPIRAL, K_TORP,
                                              K_SPRAY };

enum { C_RISE = 1, C_P1, C_DIVE, C_P2, C_ESCAPE, C_GONE, C_SURFACE, C_REACH, C_JAM, C_TAKE, C_SINK, C_VALHALLA,
       C_ORDERS, C_LAUNCH, C_END };

struct {
  u32 boss;
  int phase, t;
  s32 hx, hy, head;                 // her centre on the sea (no pan), her heading
  u32 hull_vis, hull;
  u32 glow;                         // the lamps round Dr. Marrow's cockpit, chasing round (glow.h)
  int hull_f;
  s32 hull_max;
  int hull_cool, hull_turn;
  u32 vis[NPARTS], gun[NPARTS];
  s32 aim[NPARTS], hp[NPARTS];
  int f[NPARTS], flash[NPARTS], cool[NPARTS], dead[NPARTS], turn[NPARTS];
  s32 maxhp;
  int sway;                         // phase-local clock for her movement
  // the boarding arm
  u32 seg[NSEG], claw;
  s32 reach;                        // how far the arm is out, 16.16 units
  int claw_f;
  // the jam, the pilot's ride down
  u32 jam, proto;
  s32 px, py;
  int jammed;
  int line, line2;                  // tickets of the lines a set piece waits on
  s32 rail_x, rail_y;
  int rail_seen;
  struct { u32 a; int t, life; } fx[NFX];
  // the bays' and the jaws' cycles
  int bay_t[2];
  u32 maw, maw_vis;
  int maw_t, maw_open, maw_flash;
  s32 maw_hp;
  int said;                         // the fight's one-off lines, a bit each
  int frenzy, lantern_t;
  int pike_line;                    // Argus's "Torpedoes on the Pike!": the spread waits for it
  int rail_t0;                      // the Valhalla's railgun began its charge
  // surprise dives, in either phase: down, prowling under the water to
  // beneath the ship, the water boiling where she will come up, the breach
  int sd, sd_t, sd_next;
  s32 sd_x, sd_y;                   // where she breaches
  u32 breach;                       // the emitter thrown up with the water
  int breach_t;
  int under;                        // her guns and hull out of reach, unseen
  // the Entropy Breath
  int br, br_t, br_grace, maw_turn;
  s32 br_ang;
  u32 br_seg[10];
  // the arm as a target: shot at, pushed back, never broken
  u32 arm[4];                       // along it: three lengths and the claw
  s32 base_x, base_y;
  int arm_hits, arm_spark;
  s32 tip_x, tip_y;
} g_coel;                           // (named for the probes: runtime/probes/ending.py)
#define C g_coel

// ---- effects: splashes and boils played once ----------------------------------------------
static void Effect(const char* name, s32 x, s32 y, int life) {
  for (int k = 0; k < NFX; k++) {
    if (C.fx[k].a) continue;
    C.fx[k].a = AnSpawn(name, x, y);
    C.fx[k].t = 0;
    C.fx[k].life = life;
    return;
  }
}
static void Splash(s32 x, s32 y) { Effect("*sunda/boss/splash", x, y, 24); }
static void Boil(s32 x, s32 y) { Effect("*sunda/boss/boil", x, y, 48); }

static void Effects(void) {
  for (int k = 0; k < NFX; k++)
    if (C.fx[k].a && ++C.fx[k].t >= C.fx[k].life) AnDrop(&C.fx[k].a);
}

// ---- where everything is ---------------------------------------------------------------------
static void OnHull(s32 dx, s32 dy, s32* x, s32* y) {
  s32 ox, oy;
  AnRot(dx, dy, C.head, &ox, &oy);
  *x = C.hx + ox;
  *y = C.hy + oy;
  *x += AnGroundDx(*y);                         // (on the sea)
}

static void PartAt(int p, s32* x, s32* y) { OnHull(kPartX[p], kPartY[p], x, y); }

static void CPlace(void) {
  if (C.hull_vis) {
    AnPut(C.hull_vis, C.hx + AnGroundDx(C.hy), C.hy);
    AnTurn(C.hull_vis, C.head);
    AnFrame(C.hull_vis, C.hull_f);
  }
  // her cockpit's lamps chasing round, turned with her, while she is up and
  // dry (surfaced, battered); not awash, nor under
  if (C.glow) {
    s32 ox, oy;
    AnRot(COEL_GLOW_X, COEL_GLOW_Y, C.head, &ox, &oy);
    GlowPut(C.glow, C.hx + AnGroundDx(C.hy) + ox, C.hy + oy, 0, 0, C.hull_vis && C.hull_f < 2 ? C.hull_f : GLOW_OFF);
    AnTurn(C.glow, C.head);
  }
  const s32 away = FX(-400);
  if (C.hull && Alive(C.hull)) PlacePart(Act(C.hull), C.under ? away : C.hx + AnGroundDx(C.hy), C.hy);
  for (int p = 0; p < NPARTS; p++) {
    s32 x, y;
    PartAt(p, &x, &y);
    if (C.under) x = away;
    if (C.vis[p]) {
      AnPut(C.vis[p], x, y);
      AnTurn(C.vis[p], (p == P_TUR0 || p == P_TUR1) ? C.aim[p] : C.head);
      if (C.flash[p] > 0) C.flash[p]--;
      AnFrame(C.vis[p], C.dead[p] ? 3 : (C.flash[p] > 0 ? 2 : C.f[p]));
      if (p < P_BAY0 && C.f[p] == 1 && C.cool[p] < TURRET_EVERY - 30) C.f[p] = 0;
    }
    if (C.gun[p] && Alive(C.gun[p])) PlacePart(Act(C.gun[p]), x, y);
  }
  if (C.maw_vis || C.maw) {
    s32 x, y;
    OnHull(COEL_MAW_X, COEL_MAW_Y, &x, &y);
    if (C.under) x = away;
    if (C.maw_vis) {
      AnPut(C.maw_vis, x, y);
      AnTurn(C.maw_vis, C.head);
      if (C.maw_flash > 0) C.maw_flash--;
      AnFrame(C.maw_vis, !C.maw_open ? 0 : (C.maw_flash > 0 ? 3 : (C.maw_open == 2 ? 2 : 1)));
    }
    if (C.maw && Alive(C.maw)) PlacePart(Act(C.maw), x, y);
  }
}

static s32 Hp(u32 bg) { return Alive(bg) ? SB_GetBadGuyHealth(bg) : 0; }

static s32 PhaseHp(void) {
  if (C.phase == C_P2) return Hp(C.hull);
  s32 sum = 0;
  for (int p = 0; p < NPARTS; p++) sum += Hp(C.gun[p]);
  return sum;
}

static s32 CbHealth(u32 boss) {
  (void)boss;
  return C.phase == C_P1 || C.phase == C_P2 ? PhaseHp() : 0;
}

static void CbRender(u32 boss, s32 x, s32 y) { (void)boss; (void)x; (void)y; }

static void StartPhase(int phase) {
  C.phase = phase;
  C.t = 0;
  C.maxhp = phase == C_P2 ? C.hull_max : PhaseHp();
  R32(C.boss, B_MAXHP) = C.maxhp;
}

static void Go(int phase) {
  C.phase = phase;
  C.t = 0;
}

// ---- her guns ---------------------------------------------------------------------------------
static int Live(int p) { return C.gun[p] && Alive(C.gun[p]); }

static void Guns(int hold) {
  for (int p = 0; p < NPARTS; p++) {
    if (C.gun[p] && !Alive(C.gun[p])) {
      s32 x, y;
      PartAt(p, &x, &y);
      Blast("ExplosionL", x, y);
      C.gun[p] = 0;                             // (a dead badguy's handle is reused)
      C.dead[p] = 1;
    }
    if (!Live(p)) continue;
    if (p >= P_BAY0) {
      // a bay: shut (armoured), then open to launch a drone -- and only
      // then can it be hurt
      const int b = p - P_BAY0;
      Guard(C.gun[p], hold || !C.f[p]);
      const s32 hp = SB_GetBadGuyHealth(C.gun[p]);
      if (hp < C.hp[p]) C.flash[p] = 4;
      C.hp[p] = hp;
      if (hold) continue;
      if (--C.bay_t[b] > 0) continue;
      C.f[p] = !C.f[p];
      C.bay_t[b] = C.f[p] ? BAY_OPEN : BAY_SHUT;
      if (C.f[p]) {
        OrderAt(C.gun[p], K_BAY, (C.head + (b ? TURN / 4 : -TURN / 4)) & (TURN - 1));
        if (!(C.said & 1)) { C.said |= 1; AnLine(SL_BOSS_BAYS); }   // the Doc: hit the bays while open
      }
      continue;
    }
    Guard(C.gun[p], hold);
    const s32 hp = SB_GetBadGuyHealth(C.gun[p]);
    if (hp < C.hp[p]) C.flash[p] = 4;
    C.hp[p] = hp;
    s32 x, y;
    PartAt(p, &x, &y);
    // the turrets turn after the ship at their own pace; the rest look ahead
    if (p == P_TUR0 || p == P_TUR1) C.aim[p] = AnTurnTo(C.aim[p], AnAngle(x, y, PlayerX(), PlayerY()), TURN / 220);
    else C.aim[p] = C.head;
    if (C.cool[p] > 0) C.cool[p]--;
    if (hold || C.cool[p] || Busy(C.gun[p])) continue;
    int cmd, every;
    if (p <= P_TUBE1) {
      // torpedoes at the fleet and seekers at the ship, turn about
      cmd = (C.turn[p] & 1) ? K_SEEKERS : K_TORP;
      every = TUBE_EVERY;
      if (cmd == K_SEEKERS) C.aim[p] = AnAngle(x, y, PlayerX(), PlayerY());
    } else if (p == P_LURE) {
      const int k = C.turn[p] - (C.turn[p] >= 3 ? 3 * ((C.turn[p] * 21846) >> 16) : 0);   // (turn mod 3)
      cmd = k == 0 ? K_RING : (k == 1 ? K_LANTERNS : K_SPIRAL);
      every = LURE_EVERY;
      if (cmd == K_LANTERNS) C.aim[p] = AnAngle(x, y, PlayerX(), PlayerY());
    }
    else { cmd = (C.turn[p] & 1) ? K_FAN : K_STREAM; every = TURRET_EVERY; }
    if (HardMode()) {
      // hard mode: the turrets close pincers and snipe between their own; the
      // lure's rings turn at once, and drift a gap round; every gun sooner
      static const unsigned char kTurH[4] = { K_STREAM, K_XPINCER, K_FAN, K_XSNIPE };
      if (p == P_TUR0 || p == P_TUR1) cmd = kTurH[C.turn[p] & 3];
      else if (p == P_LURE && cmd == K_RING) cmd = K_XTWIST;
      else if (p == P_LURE && cmd == K_SPIRAL && (C.turn[p] & 1)) cmd = K_XGAP;
      every -= every >> 2;
    }
    OrderAt(C.gun[p], cmd, C.aim[p]);
    C.turn[p]++;
    C.cool[p] = every;
    C.f[p] = 1;
  }
}

static void BreathStop(void);       // (the Entropy Breath, below)

static void StopGuns(void) {
  for (int p = 0; p < NPARTS; p++) if (Live(p)) Stop(C.gun[p]);
  if (C.hull && Alive(C.hull)) Stop(C.hull);
  if (C.maw && Alive(C.maw)) { Stop(C.maw); Guard(C.maw, 1); }
  C.maw_open = 0;
  BreathStop();
}

static int AllDown(void) {
  for (int p = 0; p < NPARTS; p++) if (Live(p)) return 0;
  return 1;
}

// The Entropy Breath: the core in her throat inverted -- instead of feeding
// on the ship's grazes it draws the ship in -- charging behind an aiming line
// that turns after the ship, then a beam out of her jaws that sweeps after it
// and goes through the shield (StrikePlayer, as Mission 9's lance). Its
// lengths are the lance's art (andaman.c's), spawned with her jaws so they
// draw over her hull, kept unseen until wanted.
#define BREATH_CHARGE 110
#define BREATH_LOCK   20            // the aim holds still this long before it fires
#define BREATH_FIRE   120
#define BREATH_SEGS   10
#define BREATH_HALF   9             // the beam's half-width, units
#define BREATH_GRACE  40
#define BREATH_PULL   FX(1.1)       // the draw on the ship, a frame, as it starts to charge (AbDraw-like)
#define LN_HIDDEN     6             // (andaman's lance: an empty animation)
enum { BR_OFF, BR_CHARGE, BR_FIRE, BR_FADE };

static void BreathMouth(s32* x, s32* y) {
  OnHull(COEL_MAW_X + 14, COEL_MAW_Y, x, y);
}

static void BreathLine(int anim) {
  s32 mx, my;
  BreathMouth(&mx, &my);
  const s32 ux = SB_Cos(C.br_ang), uy = SB_Sin(C.br_ang);
  for (int k = 0; k < BREATH_SEGS; k++) {
    if (!C.br_seg[k]) continue;
    s32 x = mx + ux * (20 + 40 * k), y = my + uy * (20 + 40 * k);
    const int want = anim >= 0 && !C.under && y < FX(500) && y > FX(-40) && x > FX(-40) && x < FX(440);
    if (!want) { x = mx; y = my; }
    AnPut(C.br_seg[k], x, y);
    AnTurn(C.br_seg[k], C.br_ang);
    AnFrame(C.br_seg[k], want ? anim : LN_HIDDEN);
  }
}

static void BreathHits(void) {
  if (C.br_grace > 0) { C.br_grace--; return; }
  const u32 sh = Shogun();
  if (*(volatile unsigned short*)(sh + SH_HITLOCK) || R32(sh, SH_HITLOCK2)) return;
  s32 mx, my;
  BreathMouth(&mx, &my);
  const s32 ux = SB_Cos(C.br_ang), uy = SB_Sin(C.br_ang);
  const s32 rx = PlayerX() - mx, ry = PlayerY() - my;
  const s32 along = SB_MulFP(rx, ux) + SB_MulFP(ry, uy);
  const s32 perp = SB_MulFP(rx, uy) - SB_MulFP(ry, ux);
  if (along < 0 || perp > FX(BREATH_HALF) || perp < -FX(BREATH_HALF)) return;
  SB_StrikePlayer(sh + SH_PLAYER, 0, sh + SH_HUD);
  C.br_grace = BREATH_GRACE;
}

static void BreathStop(void) {
  C.br = BR_OFF;
  BreathLine(-1);
}

// One frame of the Breath; 1 while it is still going.
static int Breath(void) {
  s32 mx, my;
  BreathMouth(&mx, &my);
  const s32 want = AnAngle(mx, my, PlayerX(), PlayerY());
  C.br_t++;
  switch (C.br) {
  case BR_CHARGE: {
    if (C.br_t < BREATH_CHARGE - BREATH_LOCK) C.br_ang = AnTurnTo(C.br_ang, want, TURN / 300);
    // the inverted core draws the ship in towards her jaws: harder as it
    // wakes, harder up close, and in gulps (the finger steers relative to
    // where it went down, so a still finger gives all of it away)
    const s32 dx = mx - PlayerX(), dy = my - PlayerY();
    const s32 d = AnDist(dx, dy);
    if (d > 40 && d < 360 && !*(volatile unsigned short*)(Shogun() + SH_HITLOCK)) {
      const s32 a = AnAngle(PlayerX(), PlayerY(), mx, my);
      s32 r = BREATH_PULL + SB_MulFP(BREATH_PULL, C.br_t * (FX(1) / (2 * BREATH_CHARGE)));
      if (d < 300) r += SB_MulFP(r, d <= 60 ? FX(1) : (300 - d) * (FX(1) / 240));
      if ((C.br_t & 31) < 8) r += r >> 1;
      if (d < 60) r = SB_MulFP(r, (d - 40) * (FX(1) / 20));     // (eases off at her teeth)
      R32(Shogun(), SH_PLAYER_X) += SB_MulFP(SB_Cos(a), r);
      R32(Shogun(), SH_PLAYER_Y) += SB_MulFP(SB_Sin(a), r);
    }
    if ((C.br_t & 7) == 0) Boom("ExplosionXXS", mx + FX(Rnd(-6, 6)), my + FX(Rnd(-6, 6)));   // (the core crackling)
    BreathLine(0);
    if (C.br_t >= BREATH_CHARGE) {
      C.br = BR_FIRE;
      C.br_t = 0;
      SB_ShakeWorld();
      Boom("ExplosionL", mx, my);
    }
    break;
  }
  case BR_FIRE:
    // the beam sweeps after the ship, slowly: outrun it
    C.br_ang = AnTurnTo(C.br_ang, want, TURN / 760);
    BreathLine(C.br_t < 4 ? 1 : 5);
    BreathHits();
    if ((C.br_t & 31) == 0) SB_ShakeWorld();
    if (C.br_t >= BREATH_FIRE) {
      C.br = BR_FADE;
      C.br_t = 0;
    }
    break;
  case BR_FADE:
    BreathLine(4);
    if (C.br_t >= 8) {
      BreathStop();
      return 0;
    }
    break;
  default:
    return 0;
  }
  return 1;
}

// Her jaws: shut, then gaping on the inverted core -- turn about, a fan of
// seekers and a cone of starlets, or the Entropy Breath -- and what is struck
// down her throat goes to her hull twice over (the jaws' emitter topped back
// up).
static void Maw(int hold) {
  if (!C.maw || !Alive(C.maw)) return;
  const s32 hp = SB_GetBadGuyHealth(C.maw);
  if (hp < C.maw_hp) {
    if (C.maw_open && C.hull && Alive(C.hull)) SB_StrikeBadGuy(BH, C.hull, (C.maw_hp - hp) * 2);
    C.maw_flash = 4;
    SB_SetBadGuyHealth(C.maw, MAW_HP);
  }
  C.maw_hp = MAW_HP;
  Guard(C.maw, hold || !C.maw_open);
  if (hold) return;
  if (C.br) {                       // the Breath: the jaws held open till it is spent
    if (!Breath()) { C.maw_open = 0; C.maw_t = C.frenzy ? MAW_SHUT - 80 : MAW_SHUT; }
    else C.maw_open = 2;
    return;
  }
  if (--C.maw_t > 0) {
    if (C.maw_open == 2 && !Busy(C.maw)) C.maw_open = 1;
    return;
  }
  if (!C.maw_open) {
    C.maw_open = 2;
    C.maw_t = MAW_OPEN;
    // the first time, and every other after: the Entropy Breath
    if (!(C.maw_turn & 1)) {
      s32 mx, my;
      BreathMouth(&mx, &my);
      C.br = BR_CHARGE;
      C.br_t = 0;
      C.br_ang = AnAngle(mx, my, PlayerX(), PlayerY());
      if (!(C.said & 2)) {
        C.said |= 2;
        AnLine(SL_BOSS_MAW);        // the Doc: an inverted entropy core in her throat!
        AnLine(SL_BREATH_IN);       // Dr. Marrow: say hello to my Entropy Breath!
      }
    } else OrderAt(C.maw, K_GULP, AnAngle(C.hx, C.hy, PlayerX(), PlayerY()));
    C.maw_turn++;
  } else {
    C.maw_open = 0;
    C.maw_t = C.frenzy ? MAW_SHUT - 80 : MAW_SHUT;
  }
}

// The frenzy: lanterns rising out of the deep all round the ship.
static void Lanterns(int hold) {
  if (hold || !C.frenzy || --C.lantern_t > 0) return;
  C.lantern_t = 70;
  const u32 l = SB_SpawnBadGuy(BH, "*sunda/scripts/lantern", FX(Rnd(40, 360)), FX(410));
  if (l) SB_SetActorOrient(Act(l), UP);
}

// her hull, in the second phase
static void HullGuns(int hold) {
  if (!C.hull || !Alive(C.hull)) return;
  Guard(C.hull, hold);
  Maw(hold);
  Lanterns(hold);
  if (!C.frenzy && Hp(C.hull) * 100 < C.hull_max * FRENZY_AT) {
    C.frenzy = 1;
    C.lantern_t = 40;
    AnLine(SL_FRENZY);              // "You're scratching her paint! Lanterns, all of them!"
    // ... and all her torpedoes: a spread into the Pike, the last escort --
    // loosed as Argus calls it (PikeSpread), so she is hit and goes down
    // on his words, not a line or two before them
    C.pike_line = AnLine(SL_PIKE_HIT);
    if (!C.pike_line) SundaSinkPike(C.hx + AnGroundDx(C.hy), C.hy + FX(60));
  }
  if (C.hull_cool > 0) C.hull_cool--;
  if (hold || C.hull_cool || Busy(C.hull) || C.br) return;   // (nothing else while she breathes)
  const int hard = HardMode();
  const int k = C.hull_turn > (hard ? 7 : 5) ? 0 : C.hull_turn;
  C.hull_turn = k == (hard ? 7 : 5) ? 0 : k + 1;
  OrderAt(C.hull, hard ? kHullCycleH[k] : kHullCycle[k], C.head);
  // quicker as she weakens
  C.hull_cool = C.frenzy ? HULL_EVERY - 40 : HULL_EVERY;
  if (hard) C.hull_cool -= C.hull_cool >> 2;
}

// The spread into the Pike, once Argus's line about it is up (whatever
// phase the fight has reached by then).
static void PikeSpread(void) {
  if (!C.pike_line || !AnLineUp(C.pike_line)) return;
  C.pike_line = 0;
  SundaSinkPike(C.hx + AnGroundDx(C.hy), C.hy + FX(60));
}

// Her station: ahead of the fleet, easing across after the ship, bobbing.
static void Sway(s32 speed) {
  s32 want = PlayerX() - AnGroundDx(C.hy);
  if (want < FX(130)) want = FX(130);
  if (want > FX(270)) want = FX(270);
  C.hx += Ease(want - C.hx, speed);
  C.sway += 2;
  if (C.sway >= 360 * 4) C.sway = 0;
  const s32 bob = SB_Sin((s32)(((u32)C.sway * 46603u) & (TURN - 1))) * 8;   // (~one bob per 720 frames)
  C.hy += Ease(FX(SUB_Y) + bob - C.hy, FX(0.4));
}

// As she comes up, every time: a spread of torpedoes off her hull, down the
// screen at the ships -- the pilot can shoot them down.
static void SurfaceSpread(void) {
  for (int k = -1; k <= 1; k++) {
    s32 x, y;
    OnHull(30 * k, 22 * k, &x, &y);
    const u32 t = SB_SpawnBadGuy(BH, "*sunda/scripts/torpedo_dn", x, y);
    if (t) SB_SetActorOrient(Act(t), DOWN);
  }
}

// ---- surprise dives ---------------------------------------------------------------------------
// In either phase, every so often: she goes under (her guns too: out of
// reach, unseen), prowls beneath the water to a point ahead of the ship, the
// water boils there -- the warning -- and she breaches, throwing up a
// starburst with the spray; then her guns are back, and she eases back up
// to her station as the fight goes on. A dive she is in the middle of: 1.
static int SurpriseDive(void) {
  if (!C.sd) {
    if (--C.sd_next > 0) return 0;
    C.sd = 1;
    C.sd_t = 0;
    C.hull_f = 2;
    Boil(C.hx + AnGroundDx(C.hy), C.hy);
    StopGuns();
  }
  C.sd_t++;
  if (C.sd == 1) {                                   // going down
    if (C.sd_t == 25) { C.hull_f = 3; C.under = 1; }
    if (C.sd_t >= SD_DOWN) {
      C.sd = 2;
      C.sd_t = 0;
    }
  } else if (C.sd == 2) {                            // prowling, to beneath the ship
    s32 tx = PlayerX() - AnGroundDx(PlayerY()), ty = PlayerY() - FX(200);
    if (tx < FX(70)) tx = FX(70);
    if (tx > FX(330)) tx = FX(330);
    if (ty < FX(100)) ty = FX(100);
    C.hx += Ease(tx - C.hx, FX(2.4));
    C.hy += Ease(ty - C.hy, FX(2.4));
    C.head = AnTurnTo(C.head, AnAngle(C.hx, C.hy, tx, ty + FX(1)), TURN / 120);
    if (C.sd_t >= SD_PROWL) {
      C.sd = 3;
      C.sd_t = 0;
      C.sd_x = C.hx;
      C.sd_y = C.hy;
    }
  } else if (C.sd == 3) {                            // the water boils: she is coming up
    C.head = AnTurnTo(C.head, DOWN, TURN / 60);
    if ((C.sd_t & 15) == 1) Boil(C.sd_x + AnGroundDx(C.sd_y) + FX(Rnd(-30, 30)), C.sd_y + FX(Rnd(-60, 60)));
    if (C.sd_t >= SD_MARK) {
      // the breach: up through the spray, a starburst thrown up with it
      C.sd = 0;
      C.under = 0;
      C.head = DOWN;
      C.hull_f = C.phase == C_P2 ? 1 : 0;
      Splash(C.hx + AnGroundDx(C.hy), C.hy);
      Splash(C.hx + AnGroundDx(C.hy), C.hy + FX(70));
      Splash(C.hx + AnGroundDx(C.hy), C.hy - FX(70));
      SB_ShakeWorld();
      SurfaceSpread();
      if (C.breach && Alive(C.breach)) { PlacePart(Act(C.breach), FX(-400), FX(-400)); Remove(C.breach); }
      C.breach = SB_SpawnBadGuy(BH, "*sunda/boss/breach", C.hx + AnGroundDx(C.hy), C.hy + FX(40));
      if (C.breach) OrderAt(C.breach, K_BURST, AnAngle(C.hx, C.hy, PlayerX(), PlayerY()));
      C.breach_t = 120;
      C.sd_next = C.phase == C_P2 ? (C.frenzy ? SD_EVERY - 220 : SD_EVERY - 120) : SD_EVERY;
      for (int p = 0; p < NPARTS; p++) C.cool[p] = 30 + 10 * p;   // (her guns open up again at once)
      C.hull_cool = 30;
      return 0;
    }
  }
  return 1;
}

// the breach's emitter, once its burst is out: gone quietly, off the screen
static void BreachTidy(void) {
  if (!C.breach || --C.breach_t > 0) return;
  if (Alive(C.breach)) {
    PlacePart(Act(C.breach), FX(-400), FX(-400));
    Remove(C.breach);
  }
  C.breach = 0;
}

// ---- the boarding arm -----------------------------------------------------------------------
static void Arm(int show) {
  s32 sx, sy, bx, by;
  OnHull(COEL_CLAW_X, COEL_CLAW_Y, &sx, &sy);
  SundaBridge(&bx, &by);
  const s32 ang = AnAngle(sx, sy, bx, by);
  const s32 ux = SB_Cos(ang), uy = SB_Sin(ang);
  for (int k = 0; k < NSEG; k++) {
    if (!C.seg[k]) continue;
    const s32 at = FX(SEG_LEN * k + SEG_LEN / 2);
    const int seen = show && at < C.reach;
    AnPut(C.seg[k], seen ? sx + SB_MulFP(ux, at) : sx, seen ? sy + SB_MulFP(uy, at) : sy);
    AnTurn(C.seg[k], ang);
    AnFrame(C.seg[k], seen ? 0 : 1);
  }
  C.base_x = sx;
  C.base_y = sy;
  C.tip_x = sx + SB_MulFP(ux, C.reach);
  C.tip_y = sy + SB_MulFP(uy, C.reach);
  if (C.claw) {
    AnPut(C.claw, C.tip_x, C.tip_y);
    AnTurn(C.claw, ang);
    AnFrame(C.claw, C.claw_f);
  }
}

static s32 ArmLength(void) {
  s32 sx, sy, bx, by;
  OnHull(COEL_CLAW_X, COEL_CLAW_Y, &sx, &sy);
  SundaBridge(&bx, &by);
  return FX(AnDist(bx - sx, by - sy) - 6);
}

// The claw as a target: what strikes it shows (sparks off it) and pushes it
// back a little -- but it is topped back up, and on it comes.
#define ARM_HP 30000
static void ArmTarget(void) {
  int hit = 0;
  for (int k = 0; k < 4; k++) {
    const u32 a = C.arm[k];
    if (!a || !Alive(a)) continue;
    // k = 3 the claw; the rest spaced along the arm, so a shot crossing it anywhere lands
    const s32 x = C.base_x + SB_MulFP(C.tip_x - C.base_x, FX(k + 1) >> 2);
    const s32 y = C.base_y + SB_MulFP(C.tip_y - C.base_y, FX(k + 1) >> 2);
    PlacePart(Act(a), x, y);
    if (SB_GetBadGuyHealth(a) >= ARM_HP) continue;
    C.arm_hits++;
    g_arm_hits++;
    if (!hit) {
      hit = 1;
      C.reach -= FX(0.25);                      // (struck, it staggers -- once a frame: it creeps on under fire)
      if (C.reach < 0) C.reach = 0;
    }
    if (++C.arm_spark >= 3) {
      C.arm_spark = 0;
      Boom("ExplosionXXS", x + FX(Rnd(-8, 8)), y + FX(Rnd(-8, 8)));
    }
    SB_SetBadGuyHealth(a, ARM_HP);
  }
}

static void ArmTargetGone(void) {
  for (int k = 0; k < 4; k++) {
    if (C.arm[k] && Alive(C.arm[k])) {
      PlacePart(Act(C.arm[k]), FX(-400), FX(-400));
      Remove(C.arm[k]);
    }
    C.arm[k] = 0;
  }
  if (g_autopilot_target) g_autopilot_target = 0;
}

// ---- the ship, taken in hand ------------------------------------------------------------------
static void Jam(int on) {
  if (on && !C.jammed) {
    g_weapons_jammed = 1;
    C.jam = AnSpawn("*sunda/boss/jam", PlayerX(), PlayerY());
    C.jammed = 1;
  } else if (!on && C.jammed) {
    g_weapons_jammed = 0;
    AnDrop(&C.jam);
    C.jammed = 0;
  }
  if (C.jam) AnPut(C.jam, PlayerX(), PlayerY());
}

// The pilot flies down into the Valhalla's bow.
static void Steer(s32 x, s32 y, s32 speed) {
  const s32 px = PlayerX(), py = PlayerY();
  R32(Shogun(), SH_PLAYER_X) = px + Ease(x - px, speed);
  R32(Shogun(), SH_PLAYER_Y) = py + Ease(y - py, speed);
}

// ---- the engine's callbacks -------------------------------------------------------------------
static void CbSpawn(u32 boss) {
  C.boss = boss;
  C.hx = CX;
  C.hy = FX(SUB_Y);
  C.head = DOWN;
  C.hull_vis = AnSpawn("*sunda/boss/coel", C.hx, C.hy);
  C.glow = GlowSpawn(BH, "*sunda/boss/coel_glow");
  C.hull_f = 3;
  for (int p = 0; p < NPARTS; p++) C.aim[p] = C.head;
  R32(boss, B_STATE) = 1;
  // the WARNING and the boss music came first: she surfaces as it clears
  // (sunda_timeline.py); the partition runs on a moment, then is held in C_RISE
  g_autopilot_target = 0;
  Go(C_RISE);
  AnLine(SL_BOSS_UP);               // Dr. Marrow: Specimen Nine, surface!
  CPlace();
}

static void Arms(void) {
  for (int p = 0; p < NPARTS; p++) {
    s32 x, y;
    PartAt(p, &x, &y);
    C.vis[p] = AnSpawn(kPartVis[p], x, y);
    C.gun[p] = SB_SpawnBadGuy(BH, kPartGun[p], x, y);
    C.hp[p] = SB_GetBadGuyHealth(C.gun[p]);
    C.cool[p] = 50 + 37 * p;
    Guard(C.gun[p], 1);            // (not before the fight is on)
  }
  C.bay_t[0] = 160;
  C.bay_t[1] = 160 + BAY_SHUT / 2;
}

// Her hull as a target -- only once her guns are gone: its hitbox runs the
// length of her and would shield them.
static void HullUp(void) {
  C.hull = SB_SpawnBadGuy(BH, "*sunda/boss/hull", C.hx + AnGroundDx(C.hy), C.hy);
  C.hull_max = SB_GetBadGuyHealth(C.hull);
  s32 x, y;
  OnHull(COEL_MAW_X, COEL_MAW_Y, &x, &y);
  C.maw_vis = AnSpawn("*sunda/boss/maw_vis", x, y);
  for (int k = 0; k < BREATH_SEGS; k++) {     // the Breath's lengths, drawn over her, unseen till wanted
    C.br_seg[k] = AnSpawn("*andaman/boss/lance", x, y);
    AnFrame(C.br_seg[k], LN_HIDDEN);
  }
  C.maw = SB_SpawnBadGuy(BH, "*sunda/boss/maw", x, y);
  C.maw_hp = MAW_HP;
  C.maw_t = 150;
  C.maw_open = 0;
}

static void CbUpdate(u32 boss) {
  C.t++;
  const int scene = InCutscene();
  switch (C.phase) {
  case C_RISE:
    // a shape under the water, breaking the surface, up (the WARNING over)
    if (C.t == 165) SB_PausePartition(BH, 1);
    if (C.t == 70) { C.hull_f = 2; Splash(C.hx, C.hy); Splash(C.hx, C.hy + FX(80)); SB_ShakeWorld(); }
    if (C.t == 120) {
      C.hull_f = 0;
      SurfaceSpread();
      Arms();
      AnLine(SL_BOSS_DOC);          // the Doc: knock out her torpedo tubes
    }
    if (C.t > 170) {                // (her taunts don't hold up the fight)
      g_autopilot_target = C.gun[P_TUBE0];
      StartPhase(C_P1);
      C.sd_next = SD_FIRST;
    }
    break;
  case C_P1:
    BossWards();
    BreachTidy();
    if (SurpriseDive()) { Guns(1); break; }
    Sway(FX(0.3));
    Guns(0);
    if (!Live(P_TUBE0) && !Live(P_TUBE1)) g_autopilot_target = Live(P_LURE) ? C.gun[P_LURE] : C.gun[Live(P_TUR0) ? P_TUR0 : P_TUR1];
    else g_autopilot_target = C.gun[Live(P_TUBE0) ? P_TUBE0 : P_TUBE1];
    if (AllDown()) {
      StopGuns();
      R32(boss, B_MAXHP) = 0;
      g_autopilot_target = 0;
      AnLine(SL_BOSS_DIVE);         // "You broke all her toys! Dive, dive!"
      Go(C_DIVE);
    }
    break;
  case C_DIVE: {
    // under, sweeping beneath the fleet, torpedoes loosed at it
    Guns(1);
    if (C.t == 20) {
      C.hull_f = 2;
      Boil(C.hx + AnGroundDx(C.hy), C.hy);
      for (int p = 0; p < NPARTS; p++) AnDrop(&C.vis[p]);   // (her wrecked guns go under with her)
    }
    if (C.t == 50) C.hull_f = 3;
    if (C.t > 50 && C.t < DIVE_T - 60) {
      const s32 a = (s32)(((u32)(C.t - 50) * 25000u) & (TURN - 1));
      C.hx += Ease(CX + SB_Cos(a) * 90 - C.hx, FX(1.4));
      C.hy += Ease(FX(220) + SB_Sin(a) * 80 - C.hy, FX(1.4));
      C.head = AnTurnTo(C.head, (a + TURN / 4) & (TURN - 1), TURN / 160);
      if ((C.t & 63) == 0) {
        const u32 tp = SB_SpawnBadGuy(BH, "*sunda/scripts/torpedo", C.hx + AnGroundDx(C.hy), C.hy);
        if (tp) SB_SetActorOrient(Act(tp), C.head);
      }
    }
    if (C.t >= DIVE_T - 60) {
      C.hx += Ease(CX - C.hx, FX(1.4));
      C.hy += Ease(FX(SUB_Y) - C.hy, FX(1.4));
      C.head = AnTurnTo(C.head, DOWN, TURN / 120);
    }
    if (C.t == DIVE_T - 20) { C.hull_f = 2; Splash(C.hx, C.hy); }
    if (C.t == DIVE_T) {
      C.hull_f = 1;                 // battered, her armour cracked
      SurfaceSpread();
      C.head = DOWN;
      AnLine(SL_BOSS_P2);           // the Doc: hit the hull with everything!
    }
    if (C.t > DIVE_T + 40) {
      HullUp();
      C.sd_next = SD_FIRST;
      g_sunda_spare = 0;
      C.hull_cool = 60;
      g_autopilot_target = C.hull;
      StartPhase(C_P2);
    }
    break;
  }
  case C_P2:
    BossWards();
    BreachTidy();
    if (SurpriseDive()) { HullGuns(1); break; }
    Sway(FX(0.45));
    HullGuns(0);
    if (Hp(C.hull) * 100 < C.hull_max * ESCAPE_AT) {
      // she has had enough of the pilot: what she came for is elsewhere
      StopGuns();
      Guard(C.hull, 1);
      g_sunda_spare = C.hull;
      R32(boss, B_MAXHP) = 0;
      g_autopilot_target = 0;
      C.line = AnLine(SL_BOSS_ESCAPE);   // "I didn't come all this way for you, silly."
      C.sd = 0;
      C.under = 0;
      Go(C_ESCAPE);
    }
    break;
  case C_ESCAPE:
    if (C.t > 60 && (AnLineUp(C.line) || C.t > 600) && C.hull_f < 2 && C.t > 140) {
      C.hull_f = 2;
      Boil(C.hx + AnGroundDx(C.hy), C.hy);
      SB_ShakeWorld();
      AnDrop(&C.maw_vis);
      for (int k = 0; k < 10; k++) AnDrop(&C.br_seg[k]);
      C.line2 = C.t;
    }
    if (C.hull_f == 2 && C.t > C.line2 + 30) C.hull_f = 3;
    if (C.hull_f == 3 && !(C.said & 4) && C.t > C.line2 + 40) {
      C.said |= 4;
      AnLine(SL_DOC_LOST);          // the Doc: she's crash diving! We've lost her
    }
    if (C.hull_f == 3 && (C.said & 4)) {
      C.hy += FX(2.2);
      if (C.hy > FX(470)) {
        // her torpedoes, drones and lanterns gone with her -- and her own
        // hull's and jaws' targets, for good: left in place they followed
        // her back up and took the shots meant for her arm, and the
        // Valhalla's turrets kept firing at them
        SundaClearSea(0);
        C.hull = 0;
        C.maw = 0;
        g_sunda_spare = 0;
        AnDrop(&C.hull_vis);
        GlowDrop(&C.glow);
        SundaFleetTo(FX(300));      // the Bastion, dead in the water, drifts up into view
        SundaFleetGuns(0);
        C.line2 = 0;
        Go(C_GONE);
      }
    }
    break;
  case C_GONE:
    if (C.t == 40) C.line = AnLine(SL_ARGUS_WARN);   // Argus: contact, right under us... she's boarding!
    // (and only once the Bastion is on her station: placed beside her while
    // she was still drifting up, the sub was left lower than her bridge --
    // whenever Argus's line came up quickly, as it often did in hard mode)
    if (C.t > 60 && (AnLineUp(C.line) || C.t > 700) && (SundaFleetSettled() || C.t > 900)) {
      s32 bx, by;
      SundaFleetPos(&bx, &by);
      C.hx = bx - AnGroundDx(by) + FX(80);
      C.hy = by - FX(10);
      C.head = UP;
      C.hull_vis = AnSpawn("*sunda/boss/coel", C.hx, C.hy);
      C.glow = GlowSpawn(BH, "*sunda/boss/coel_glow");
      C.hull_f = 3;
      Go(C_SURFACE);
    }
    break;
  case C_SURFACE:
    if (C.t == 50) {
      C.hull_f = 2;
      Splash(C.hx, C.hy - FX(60));
      Splash(C.hx, C.hy + FX(50));
      SB_ShakeWorld();
      SundaFleetShellAt(C.hx + AnGroundDx(C.hy), C.hy, 1);   // Argus: every gun on her
    }
    if (C.t == 90) SurfaceSpread();
    if (C.t == 90) {
      C.hull_f = 1;
      for (int k = 0; k < NSEG; k++) {
        C.seg[k] = AnSpawn("*sunda/boss/tendril", C.hx, C.hy);
        AnFrame(C.seg[k], 1);
      }
      C.claw = AnSpawn("*sunda/boss/claw", C.hx, C.hy);
      C.claw_f = 0;
      C.reach = 0;
      C.line = AnLine(SL_MARROW_GRAB);   // "Every code to UNITY's HQ, in one handsome head!"
      for (int k = 0; k < 4; k++) C.arm[k] = SB_SpawnBadGuy(BH, "*sunda/boss/arm", C.hx + AnGroundDx(C.hy), C.hy);
      C.arm_hits = 0;
      g_autopilot_target = C.arm[3];
      C.line2 = 0;
      Go(C_REACH);
    }
    break;
  case C_REACH:
    // the arm reaches for the bridge, slowly; the pilot can shoot it -- the
    // hits stagger it -- but it never breaks. Dr. Marrow laughs at it, and
    // Argus gives his order
    ArmTarget();
    if (C.reach < ArmLength()) C.reach += FX(0.42);
    if (C.reach > ArmLength()) C.reach = ArmLength();
    if (!C.line2 && C.t > 200 && (C.arm_hits > 40 || C.reach >= ArmLength() || C.t > 700)) {
      C.line2 = AnLine(SL_ARGUS_PURGE);  // Argus: purge my mind drive...
    }
    if ((C.line2 && C.reach >= ArmLength() && AnLineUp(C.line2)) || C.t > 1400) {
      C.line = AnLine(SL_MARROW_JAM);    // "Inverting an unprotected entropy engine..."
      AnLine(SL_DOC_JAM);                // the Doc: our weapons are jammed!
      Go(C_JAM);
    }
    break;
  case C_JAM:
    if (!C.jammed) ArmTarget();          // (shot at to the last)
    if (C.reach < ArmLength()) C.reach += FX(0.8);
    if (C.reach > ArmLength()) C.reach = ArmLength();
    if (AnLineUp(C.line) || C.t > 600) {
      Jam(1);                            // every weapon jammed, the Bastion's too
      SundaFleetShellAt(0, 0, 0);
      ArmTargetGone();
    }
    if (C.jammed && C.reach >= ArmLength() && C.claw_f == 0 && C.t > 60) C.claw_f = 1;   // shut on him
    if (C.claw_f == 1 && C.t > 100) C.claw_f = 2;
    if (C.claw_f == 2 && C.t > 200 && AnQuiet()) {
      C.line = AnLine(SL_MARROW_BYE);    // "Hope you come visit sometime for another gift!"
      Go(C_TAKE);
    }
    break;
  case C_TAKE:
    // the arm drawn in with Argus in its claw; then down she goes
    if (C.reach > 0) C.reach -= FX(0.9);
    if (C.reach <= 0) {
      C.reach = 0;
      for (int k = 0; k < NSEG; k++) AnDrop(&C.seg[k]);
      AnDrop(&C.claw);
    }
    if (!C.claw && (AnLineUp(C.line) || C.t > 600)) {
      C.hull_f = 2;                      // awash, slipping away
      Boil(C.hx + AnGroundDx(C.hy), C.hy);
      C.line = AnLine(SL_SHOGUN_TAUNT);  // the Shogun: your captain will tell me everything
      C.line2 = 0;
      Go(C_SINK);
    }
    break;
  case C_SINK:
    // awash, she turns away and makes off up the screen; the Shogun gloats.
    // As his last words go up, the Valhalla sweeps in from the side
    C.head = AnTurnTo(C.head, (UP - TURN / 10) & (TURN - 1), TURN / 400);
    C.hx += Ease(FX(140) - C.hx, FX(0.45));
    C.hy += Ease(FX(175) - C.hy, FX(0.45));
    // his last word said, she sweeps in from the side as Volta calls it
    if ((C.t > 120 && AnLineUp(C.line) && AnQuiet()) || C.t > 1400) {
      AnValComeSide(VAL_X0, VAL_X1, VAL_Y);
      C.line2 = AnLine(SL_VOLTA_IN);     // Volta: flank speed! Railgun, full charge! Let him GO!
      C.rail_t0 = 0;
      Go(C_VALHALLA);
    }
    break;
  case C_VALHALLA: {
    // the gun swings on and charges as she fades under; the slug lands as
    // she is gone
    C.hx += Ease(FX(140) - C.hx, FX(0.3));
    C.hy += Ease(FX(175) - C.hy, FX(0.3));
    // on station, the gun swings onto her and charges (two seconds: the
    // slug lands on "Let him GO!")
    if (!(C.said & 8) && AnValThere()) {
      C.said |= 8;
      AnValRailAt(C.hx + AnGroundDx(C.hy), C.hy, C.line2);
    }
    // her turrets open up on the sub as she comes, shells walking round
    // the shape as it goes under -- until the railgun takes its shot
    if (AnValRailState() < RAIL_FIRING && C.hull_vis) {
      for (int k = 0; k < 6; k++)
        if (!AnValBusy(k))
          AnValOrder(k, C.hx + AnGroundDx(C.hy) + FX(Rnd(-36, 36)), C.hy + FX(Rnd(-50, 50)), SHOT_MEDIUM, TAG_NONE,
                     16 + 3 * k);
    }
    if (!(C.said & 8)) break;
    AnValRailMove(C.hx + AnGroundDx(C.hy), C.hy);
    const int rs = AnValRailState();
    if (rs == RAIL_CHARGING && !C.rail_t0) C.rail_t0 = C.t;
    if (C.rail_t0 && C.t == C.rail_t0 + 40) { C.hull_f = 3; Boil(C.hx + AnGroundDx(C.hy), C.hy); }
    if (rs >= RAIL_FIRING && C.hull_vis) {
      C.rail_x = C.hx + AnGroundDx(C.hy);
      C.rail_y = C.hy;
      AnDrop(&C.hull_vis);               // gone deep, a heartbeat before the slug
      GlowDrop(&C.glow);
      Splash(C.rail_x, C.rail_y);        // the slug into empty sea
      Splash(C.rail_x + FX(20), C.rail_y + FX(16));
      Boil(C.rail_x, C.rail_y);
    }
    if (rs == RAIL_DONE && !C.rail_seen) {
      C.rail_seen = 1;
      AnLine(SL_VOLTA_LATE);             // "too late. She's gone deep."
      AnLine(SL_HQ_ORDERS);              // the Doc: recover Argus, or confirm his destruction
      C.line = AnLine(SL_VOLTA_SUB);     // Volta: a prototype submersible. She's yours.
      Go(C_ORDERS);
    }
    break;
  }
  case C_ORDERS:
    if (C.t > 60 && (AnLineUp(C.line) || C.t > 2400) && AnQuiet()) {
      Jam(0);
      g_weapons_jammed = 1;              // (no firing while she sets down)
      AnLine(SL_DOC_DIVE);               // "Your weapons are back online. Bring him home."
      C.line2 = 0;
      Go(C_LAUNCH);
    }
    break;
  case C_LAUNCH: {
    // the pilot holds still; the Valhalla moves over the ship and takes her
    // aboard, then flies on -- and where the ship was, the submersible
    Steer(DOCK_X, DOCK_Y, FX(1.6));
    if (C.t == 40) AnValMoveTo(DOCK_X, DOCK_Y);
    s32 vx, vy;
    AnValPos(&vx, &vy);
    if (!C.line2 && C.t > 60 && AnAbs(vx - DOCK_X) < FX(4) && AnAbs(vy - DOCK_Y) < FX(4)) {
      C.line2 = C.t;                     // over her: docking
      g_hide_player = 1;
      SB_ShakeWorld();
    }
    if (C.line2 && C.t == C.line2 + 70) AnValLeave();   // and on, up the screen
    // her stern clear of the spot: the submersible is there
    if (C.line2 && !C.proto && !(C.said & 32) && C.t > C.line2 + 70 && vy + FX(VAL_HALF_LEN) < DOCK_Y - FX(30)) {
      C.said |= 32;                      // (spawned once)
      C.px = DOCK_X;
      C.py = DOCK_Y;
      C.proto = AnSpawn("*sunda/boss/proto", C.px, C.py);
      C.sd_t = C.t;
    }
    if (C.proto) {
      // a moment on the surface, then down, slowly ahead
      const int k = C.t - C.sd_t;
      if (k > 60) C.py -= FX(0.45);
      AnPut(C.proto, C.px + AnGroundDx(C.py), C.py);
      AnFrame(C.proto, k < 60 ? 0 : (k < 110 ? 1 : (k < 170 ? 2 : 3)));
      if (k == 60 || k == 110 || k == 170) Boil(C.px + AnGroundDx(C.py), C.py + FX(10));
      if (k > 240) {
        AnDrop(&C.proto);
        Go(C_END);
      }
    }
    break;
  }
  case C_END:
    Steer(DOCK_X, DOCK_Y, FX(2));
    if (C.t > 60) {
      R32(boss, B_STATE) = 0;            // the engine calls death next: the level runs to its end
      return;
    }
    break;
  }
  if (C.phase >= C_GONE) {
    // the boarding and after: nothing hurts the ship; the arm
    ShipShield(1);
    Arm(C.phase >= C_REACH && C.phase <= C_TAKE);
    if (C.jammed) Jam(1);
  } else {
    (void)scene;
    ShipShield(C.phase == C_RISE || C.phase == C_ESCAPE);
  }
  PikeSpread();
  Effects();
  CPlace();
}

static void CbDeath(u32 boss) {
  (void)boss;
  Jam(0);
  ShipShield(0);
  g_autopilot_target = 0;
  g_sunda_spare = 0;
  SB_PausePartition(BH, 0);
}

// Installs Specimen Nine; the partition's boss event spawns her. (levelkit
// forces her with this too.)
void CoelBossInit(u32 boss) {
  SB_ClearBytes((void*)boss, BOSS_SIZE);
  SB_ClearBytes(&C, sizeof C);
  g_sunda_spare = 0;
  g_weapons_jammed = g_hide_player = 0;   // (a run left in the middle of the ending)
  C.sd_next = SD_FIRST;
  R32(boss, B_SPAWN + 0x00) = (s32)(u32)CbSpawn;
  R32(boss, B_SPAWN + 0x04) = (s32)(u32)CbUpdate;
  R32(boss, B_SPAWN + 0x08) = (s32)(u32)CbRender;
  R32(boss, B_SPAWN + 0x0c) = (s32)(u32)CbRender;
  R32(boss, B_SPAWN + 0x10) = (s32)(u32)CbDeath;
  R32(boss, B_HEALTH) = (s32)(u32)CbHealth;
}

// worlds.c: world 10's "mid-boss" init, at the start of every run: the
// world's state afresh, and Specimen Nine installed.
void SundaBossInit(u32 boss) {
  SundaReset();
  CoelBossInit(boss);
}
