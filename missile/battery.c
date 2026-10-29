// battery.c -- Mission 9's two checkpoints: a heavy anti-aircraft gun
// guarding a shield generator, and the UNITY cruiser that comes for it.
//
// Each stands on an island (a prop): the emplacement, the generator and its
// shield dome, and what is seen of the guns are generics that come down the
// screen with it (andaman.c's riders: BatteryRide), stop with it over the
// island, and ride on out with it afterwards (BatteryRideOut). The boss
// event (two of them in the partition; worlds.c's row gives the first, each
// one's death sets up the next) brings the fight:
//
//   Typhoon   (checkpoint one) a quad flak turret on a hardened emplacement,
//             two light flak guns at its corners. Flak -- shells that burst
//             into rings at the ship's range (the code tells each how far,
//             R12) -- streams down its aim, curtains of flak across the
//             ship's path, a swept double stream; the corner guns pepper
//             the ship and throw slow rings to graze. Its aim is a semi-aim:
//             the turret turns after the ship at its own pace, and fires
//             where it points;
//   Raijin    (checkpoint two) a rail cannon fed by two capacitor pylons:
//             while either stands it cannot be hurt. It charges -- an aiming
//             line coming round after the ship, locking -- and fires its
//             lance, a beam that sweeps slowly after the ship and burns
//             through the shield (code strikes the ship); between lances its
//             coils vent spirals and fans of sparks. The pylons discharge at
//             the ship and ring sparks round themselves. With the pylons
//             down it is exposed, and fires faster;
//   the visit the gun silenced, a UNITY cruiser (Lancer, then Halberd) and two
//             frigates sail up from the bottom of the screen, turn their
//             guns on the generator and shell its dome until it fails, then
//             the generator until it blows; a few interceptors come in from
//             above to harass; the ships turn about and sail away, and the
//             flight goes on.

#include "andaman.h"

enum { K_FLAK, K_RAIL };

// ---- the islands' pieces (they outlive the fights) -----------------------------------------
typedef struct {
  int up, done, gone;
  u32 emp, gen, dome, vis_main, vis_side[2];
  s32 ey, gy;                     // the emplacement's and the generator's centres (x = 200)
  s32 aim_main, aim_side[2];
  int f_emp, f_gen, f_dome, f_main, f_side[2];
} Island;
static Island I[2];
static int g_fight = -1;          // the checkpoint whose fight is on

#define CX FX(200)
#define GX(y) (CX + AnGroundDx(y))   // x = 200 on the island at y, as the camera pans

static void MainAt(int k, s32* x, s32* y) {
  *y = I[k].ey + FX(k == K_RAIL ? RAIL_MAIN_Y : FLAK_MAIN_Y);
  *x = GX(I[k].ey) + FX(k == K_RAIL ? RAIL_MAIN_X : FLAK_MAIN_X);
}

static void SideAt(int k, int s, s32* x, s32* y) {
  if (k == K_RAIL) {
    *x = GX(I[k].ey) + FX(s ? RAIL_PYLON1_X : RAIL_PYLON0_X);
    *y = I[k].ey + FX(s ? RAIL_PYLON1_Y : RAIL_PYLON0_Y);
  } else {
    *x = GX(I[k].ey) + FX(s ? FLAK_SIDE1_X : FLAK_SIDE0_X);
    *y = I[k].ey + FX(s ? FLAK_SIDE1_Y : FLAK_SIDE0_Y);
  }
}

static void PlaceIsland(int k) {
  Island* p = &I[k];
  AnPut(p->emp, GX(p->ey), p->ey);
  AnFrame(p->emp, p->f_emp);
  AnPut(p->gen, GX(p->gy), p->gy);
  AnFrame(p->gen, p->f_gen);
  AnPut(p->dome, GX(p->gy), p->gy);
  AnFrame(p->dome, p->f_dome);
  s32 x, y;
  MainAt(k, &x, &y);
  AnPut(p->vis_main, x, y);
  AnTurn(p->vis_main, p->aim_main);
  AnFrame(p->vis_main, p->f_main);
  for (int s = 0; s < 2; s++) {
    SideAt(k, s, &x, &y);
    AnPut(p->vis_side[s], x, y);
    if (k == K_FLAK) AnTurn(p->vis_side[s], p->aim_side[s]);
    AnFrame(p->vis_side[s], p->f_side[s]);
  }
}

static void IslandUp(int k, s32 ey, s32 gy) {
  Island* p = &I[k];
  if (p->up) return;
  SB_ClearBytes(p, sizeof *p);
  p->up = 1;
  p->ey = ey;
  p->gy = gy;
  p->f_emp = k == K_RAIL ? 2 : 0;
  p->aim_main = DOWN;
  p->aim_side[0] = p->aim_side[1] = DOWN;
  // spawned bottom to top
  p->emp = AnSpawn("*andaman/boss/emplacement", CX, ey);
  p->gen = AnSpawn("*andaman/boss/generator", CX, gy);
  p->dome = AnSpawn("*andaman/boss/gen_dome", CX, gy);
  s32 x, y;
  MainAt(k, &x, &y);
  p->vis_main = AnSpawn(k == K_RAIL ? "*andaman/boss/rail_vis" : "*andaman/boss/flak_vis", x, y);
  for (int s = 0; s < 2; s++) {
    SideAt(k, s, &x, &y);
    p->vis_side[s] = AnSpawn(k == K_RAIL ? "*andaman/boss/pylon_vis" : "*andaman/boss/flakgun_vis", x, y);
  }
  PlaceIsland(k);
}

// Every frame its island is on screen (andaman.c): coming in, at rest under
// the fight, and -- once the fight is done -- going.
void BatteryRide(int k, s32 ey, s32 gy) {
  Island* p = &I[k];
  if (p->gone) return;
  IslandUp(k, ey, gy);
  p->ey = ey;
  p->gy = gy;
  if (!p->done) {
    // the guns watch the ship come, idly
    s32 x, y;
    MainAt(k, &x, &y);
    if (ey > FX(0) && g_fight != k)
      p->aim_main = AnTurnTo(p->aim_main, AnAngle(x, y, PlayerX(), PlayerY()), TURN / 700);
  }
  PlaceIsland(k);
}

// Its island has gone below the screen: so do its pieces.
void BatteryRideOut(int k, s32 ey, s32 gy) {
  Island* p = &I[k];
  (void)ey;
  (void)gy;
  if (!p->up || p->gone) return;
  AnDrop(&p->emp);
  AnDrop(&p->gen);
  AnDrop(&p->dome);
  AnDrop(&p->vis_main);
  for (int s = 0; s < 2; s++) AnDrop(&p->vis_side[s]);
  p->gone = 1;
}

void BatteryForget(void) {
  SB_ClearBytes(I, sizeof I);
  g_fight = -1;
}

// ---- the fight ------------------------------------------------------------------------------
enum { P_INTRO = 1, P_FIGHT, P_BLOWN, P_COME, P_SWARM, P_SHELL, P_RAIL, P_LEAVE, P_OVER };

#define NSEG 12                   // lengths of lance, 40 units each
#define LANCE_HALF 12
#define LANCE_GRACE 30
#define LANCE_CHARGE 110
#define LANCE_LOCK 22             // the last frames of the charge: it stops turning
#define LANCE_FIRE 84
#define LANCE_EVERY 290
#define LANCE_EVERY_EXPOSED 210
enum { LN_OFF, LN_CHARGE, LN_FIRE, LN_FADE };

#define DOME_HITS 18              // shells the dome takes before it fails ...
#define DOME_FAIL 12              // ... flickering from this many
#define GEN_HITS 6                // ... and the generator after it

static struct {
  u32 boss;
  int k, phase, t;
  u32 main, side[2];
  s32 hp_main, hp_side[2], max_main;
  int flash_main, flash_side[2];
  int cool, turn, side_cool[2], side_turn[2];
  int exposed;
  // the lance
  int lance, lance_t, lance_cool, lance_grace;
  s32 lance_ang;
  u32 seg[NSEG];
  // the Valhalla's visit
  int line, down_line;            // tickets: Volta's call that brings her
  u32 tough;                      // the gunship: the railgun's, not the pilot's
  int hits, gen_hits, flare;
} B;

static s32 Hp(u32 bg) { return Alive(bg) ? SB_GetBadGuyHealth(bg) : 0; }

static s32 BtHealth(u32 boss) {
  (void)boss;
  if (B.phase != P_FIGHT && B.phase != P_INTRO) return 0;
  return Hp(B.main) + Hp(B.side[0]) + Hp(B.side[1]);
}

static void BtRender(u32 boss, s32 x, s32 y) { (void)boss; (void)x; (void)y; }

static int Hurt(void) { return Hp(B.main) * 2 < B.max_main; }

static void BtSpawn(u32 boss) {
  const int k = B.k;
  B.boss = boss;
  g_fight = k;
  if (!I[k].up) IslandUp(k, FX(EMP_Y), FX(GEN_Y));     // (forced early: levelkit --boss)
  s32 x, y;
  MainAt(k, &x, &y);
  B.main = SB_SpawnBadGuy(BH, k == K_RAIL ? "*andaman/boss/rail_main" : "*andaman/boss/flak_main", x, y);
  Place(B.main, x, y);
  for (int s = 0; s < 2; s++) {
    SideAt(k, s, &x, &y);
    B.side[s] = SB_SpawnBadGuy(BH, k == K_RAIL ? "*andaman/boss/rail_pylon" : "*andaman/boss/flak_side", x, y);
    Place(B.side[s], x, y);
    B.hp_side[s] = Hp(B.side[s]);
    B.side_cool[s] = 70 + 40 * s;
  }
  B.hp_main = B.max_main = Hp(B.main);
  B.cool = 50;
  B.lance_cool = 150;
  B.lance_ang = I[k].aim_main;
  B.phase = P_INTRO;
  R32(boss, B_STATE) = 1;
  SB_PausePartition(BH, 1);
  R32(boss, B_MAXHP) = BtHealth(boss);
  g_autopilot_target = k == K_RAIL ? B.side[0] : B.main;
  if (k == K_FLAK) {
    AnLine(L_CP1_VORN);           // Vorn: "Remember me, maggot?"
    AnLine(L_CP1_DOC);            // the Doc: "the admiral whose fleet you sank"
  } else {
    AnLine(L_CP2_VORN);           // "pulled it from your friend's wreck"
    AnLine(L_CP2_VOLTA);          // Volta: "That's MY railgun! The pylons..."
  }
}

// ---- the lance (the rail battery) ------------------------------------------------------------
static void Muzzle(s32* x, s32* y) {
  s32 px, py;
  MainAt(K_RAIL, &px, &py);
  *x = px + SB_Cos(B.lance_ang) * RAIL_BARREL;
  *y = py + SB_Sin(B.lance_ang) * RAIL_BARREL;
}

// The beam (or the aiming line, frame 0), laid from the muzzle along the
// cannon's heading past the edge of the screen. anim < 0 takes it down.
static void Lance(int anim) {
  s32 mx, my;
  Muzzle(&mx, &my);
  const s32 ux = SB_Cos(B.lance_ang), uy = SB_Sin(B.lance_ang);
  for (int k = 0; k < NSEG; k++) {
    const s32 x = mx + ux * (20 + 40 * k), y = my + uy * (20 + 40 * k);
    const int want = anim >= 0 && y < FX(440) && y > FX(-40) && x > FX(-40) && x < FX(440);
    if (want && !B.seg[k]) B.seg[k] = AnSpawn("*andaman/boss/lance", x, y);
    if (!want && B.seg[k]) AnDrop(&B.seg[k]);
    if (B.seg[k]) {
      AnPut(B.seg[k], x, y);
      AnTurn(B.seg[k], B.lance_ang);
      AnFrame(B.seg[k], anim);
    }
  }
}

static void LanceHits(void) {
  if (B.lance_grace > 0) { B.lance_grace--; return; }
  const u32 sh = Shogun();
  if (*(volatile unsigned short*)(sh + SH_HITLOCK) || R32(sh, SH_HITLOCK2)) return;
  s32 mx, my;
  Muzzle(&mx, &my);
  const s32 ux = SB_Cos(B.lance_ang), uy = SB_Sin(B.lance_ang);
  const s32 rx = PlayerX() - mx, ry = PlayerY() - my;
  const s32 along = SB_MulFP(rx, ux) + SB_MulFP(ry, uy);
  const s32 perp = SB_MulFP(rx, uy) - SB_MulFP(ry, ux);
  if (along < 0 || perp > FX(LANCE_HALF) || perp < -FX(LANCE_HALF)) return;
  SB_StrikePlayer(sh + SH_PLAYER, 0, sh + SH_HUD);
  B.lance_grace = LANCE_GRACE;
}

static void LanceStep(void) {
  s32 px, py;
  MainAt(K_RAIL, &px, &py);
  const s32 want = AnAngle(px, py, PlayerX(), PlayerY());
  B.lance_t++;
  switch (B.lance) {
  case LN_CHARGE:
    if (B.lance_t < LANCE_CHARGE - LANCE_LOCK) B.lance_ang = AnTurnTo(B.lance_ang, want, TURN / 330);
    I[K_RAIL].f_main = B.lance_t < 37 ? 1 : (B.lance_t < 74 ? 2 : 3);
    Lance(0);
    if (B.lance_t >= LANCE_CHARGE) {
      B.lance = LN_FIRE;
      B.lance_t = 0;
      SB_ShakeWorld();
      Muzzle(&px, &py);
      Boom("ExplosionL", px, py);
    }
    break;
  case LN_FIRE:
    // it sweeps after the ship, slower than the ship can fly
    B.lance_ang = AnTurnTo(B.lance_ang, want, B.exposed ? TURN / 900 : TURN / 1200);
    I[K_RAIL].f_main = 4;
    Lance(B.lance_t < 5 ? 1 : 5);
    LanceHits();
    if ((B.lance_t & 31) == 0) SB_ShakeWorld();
    if (B.lance_t >= LANCE_FIRE) {
      B.lance = LN_FADE;
      B.lance_t = 0;
    }
    break;
  case LN_FADE:
    Lance(4);
    I[K_RAIL].f_main = 3;
    if (B.lance_t >= 8) {
      Lance(-1);
      B.lance = LN_OFF;
      B.lance_cool = B.exposed ? LANCE_EVERY_EXPOSED : LANCE_EVERY;
      B.cool = 60;
    }
    break;
  }
}

static void LanceStop(void) {
  Lance(-1);
  B.lance = LN_OFF;
}

// ---- the guns -------------------------------------------------------------------------------
static void Flashes(void) {
  const s32 hm = Hp(B.main);
  if (hm < B.hp_main) B.flash_main = 5;
  B.hp_main = hm;
  if (B.flash_main > 0) B.flash_main--;
  for (int s = 0; s < 2; s++) {
    const s32 h = Hp(B.side[s]);
    if (h < B.hp_side[s]) B.flash_side[s] = 5;
    B.hp_side[s] = h;
    if (B.flash_side[s] > 0) B.flash_side[s]--;
  }
}

// What is seen of the guns: frames from what they are doing.
static void Looks(void) {
  Island* p = &I[B.k];
  if (B.k == K_FLAK) {
    if (Alive(B.main)) p->f_main = B.flash_main ? 3 : (Busy(B.main) ? 2 : 0);
    for (int s = 0; s < 2; s++)
      if (Alive(B.side[s])) p->f_side[s] = Busy(B.side[s]) ? 1 : 0;
  } else {
    if (Alive(B.main) && B.lance == LN_OFF) p->f_main = B.flash_main ? 5 : (Busy(B.main) ? 1 : 0);
    if (Alive(B.main) && B.lance != LN_OFF && B.flash_main) p->f_main = 5;
    for (int s = 0; s < 2; s++)
      if (Alive(B.side[s])) p->f_side[s] = B.flash_side[s] ? 2 : (Busy(B.side[s]) ? 1 : 0);
  }
}

// A side gun or pylon gone: wrecked where it stood.
static void SidesLost(void) {
  for (int s = 0; s < 2; s++) {
    if (!B.side[s] || Alive(B.side[s])) continue;
    s32 x, y;
    SideAt(B.k, s, &x, &y);
    Boom("ExplosionL", x, y);
    I[B.k].f_side[s] = B.k == K_RAIL ? 3 : 2;
    B.side[s] = 0;
    SB_ShakeWorld();
  }
}

static void FlakGuns(int scene) {
  Island* p = &I[K_FLAK];
  s32 mx, my;
  MainAt(K_FLAK, &mx, &my);
  const int hurt = Hurt();
  p->aim_main = AnTurnTo(p->aim_main, AnAngle(mx, my, PlayerX(), PlayerY()), hurt ? TURN / 260 : TURN / 360);
  for (int s = 0; s < 2; s++) {
    if (!Alive(B.side[s])) continue;
    s32 x, y;
    SideAt(K_FLAK, s, &x, &y);
    p->aim_side[s] = AnTurnTo(p->aim_side[s], AnAngle(x, y, PlayerX(), PlayerY()), TURN / 200);
    if (scene || Busy(B.side[s])) continue;
    if (--B.side_cool[s] > 0) continue;
    static const unsigned char kSide[4] = { A_PEPPER, A_RING, A_PEPPER, A_SPOKES };
    // hard mode (docs/HARD_MODE.md): pincers and snipe rings from the corners
    static const unsigned char kSideH[4] = { A_PEPPER, A_XPINCER, A_RING, A_XSNIPE };
    OrderAt(B.side[s], (HardMode() ? kSideH : kSide)[B.side_turn[s]++ & 3], p->aim_side[s]);
    B.side_cool[s] = HardMode() ? (hurt ? 50 : 70) : (hurt ? 68 : 95);
  }
  if (scene || Busy(B.main)) return;
  if (--B.cool > 0) return;
  static const unsigned char kSeq[] = { A_FLAK, A_QUAD, A_HOOK, A_BOX, A_FLAK, A_SWEEP, A_VORTEX, A_BOX };
  // hard mode: twist rings, drifting-gap rings, the snake and snipe rings
  // woven into the main gun's, and sooner
  static const unsigned char kSeqH[] = { A_FLAK, A_XTWIST, A_QUAD, A_HOOK, A_XGAP, A_BOX, A_FLAK, A_XSNAKE, A_SWEEP,
                                         A_VORTEX, A_XSNIPE, A_BOX };
  const int hard = HardMode();
  const unsigned char* seq = hard ? kSeqH : kSeq;
  const int n = hard ? (int)sizeof kSeqH : (int)sizeof kSeq;
  if (B.turn >= n) B.turn = 0;
  const int cmd = seq[B.turn];
  if (++B.turn >= n) B.turn = 0;
  SetReg(B.main, 12, FX(FlakSteps(mx, my, cmd == A_BOX ? 7 : 6)));
  OrderAt(B.main, cmd, p->aim_main);
  B.cool = hard ? (hurt ? 28 : 40) : (hurt ? 38 : 54);
}

static void RailGuns(int scene) {
  Island* p = &I[K_RAIL];
  s32 mx, my;
  MainAt(K_RAIL, &mx, &my);
  // the pylons: discharges at the ship, rings of sparks round them
  for (int s = 0; s < 2; s++) {
    if (!Alive(B.side[s])) continue;
    if (scene || Busy(B.side[s])) continue;
    if (--B.side_cool[s] > 0) continue;
    s32 x, y;
    SideAt(K_RAIL, s, &x, &y);
    OrderAt(B.side[s], (B.side_turn[s]++ & 1) ? A_RING : A_ARC, AnAngle(x, y, PlayerX(), PlayerY()));
    B.side_cool[s] = HardMode() ? 60 : 80;
  }
  // exposed once both pylons are down
  if (!B.exposed && !Alive(B.side[0]) && !Alive(B.side[1])) {
    B.exposed = 1;
    SB_ShakeWorld();
    g_autopilot_target = B.main;
    AnLine(L_CP2_EXPOSED);        // the Doc: "The cannon is exposed!"
  } else if (!B.exposed && !Alive(g_autopilot_target)) {
    g_autopilot_target = Alive(B.side[0]) ? B.side[0] : B.side[1];
  }
  if (B.lance != LN_OFF) {
    LanceStep();
    p->aim_main = B.lance_ang;
    return;
  }
  p->aim_main = AnTurnTo(p->aim_main, AnAngle(mx, my, PlayerX(), PlayerY()), TURN / 400);
  B.lance_ang = p->aim_main;
  if (scene) return;
  if (B.lance_cool > 0) B.lance_cool--;
  if (Busy(B.main)) return;
  if (B.lance_cool == 0) {
    B.lance = LN_CHARGE;
    B.lance_t = 0;
    return;
  }
  if (--B.cool > 0) return;
  static const unsigned char kRail[4] = { A_COIL, A_SCATTER, A_CURL, A_SCATTER };
  // hard mode: pincers, twist rings, snipe rings and gap rings between
  static const unsigned char kRailH[8] = { A_COIL, A_XPINCER, A_SCATTER, A_XTWIST, A_CURL, A_XSNIPE, A_SCATTER, A_XGAP };
  if (HardMode()) {
    OrderAt(B.main, kRailH[B.turn++ & 7], p->aim_main);
    B.cool = B.exposed ? 45 : 64;
    return;
  }
  OrderAt(B.main, kRail[B.turn++ & 3], p->aim_main);
  B.cool = B.exposed ? 60 : 85;
}

// The gun is silenced: it blows, its emplacement is wrecked, the corner
// guns go with it -- and the call goes out to the cruiser.
static void Blown(void) {
  const int k = B.k;
  s32 x, y;
  MainAt(k, &x, &y);
  Boom("ExplosionXL", x, y);
  Boom("ExplosionL", x - FX(30), y + FX(10));
  Boom("ExplosionL", x + FX(28), y - FX(12));
  SB_ShakeWorld();
  for (int s = 0; s < 2; s++) Remove(B.side[s]);
  SidesLost();
  // forget them: a dead badguy's handle is handed to the next one spawned
  B.main = B.side[0] = B.side[1] = 0;
  if (k == K_RAIL) LanceStop();
  I[k].f_emp = k == K_RAIL ? 3 : 1;
  I[k].f_main = k == K_RAIL ? 6 : 4;
  R32(B.boss, B_MAXHP) = 0;
  g_autopilot_target = 0;
  B.phase = P_BLOWN;
  B.t = 0;
  B.down_line = AnLine(k == K_RAIL ? L_CP2_DOWN : L_CP1_DOWN);
  // Volta: "meet the Valhalla" (one) / "Valhalla, moving in" (two)
  B.line = k == K_FLAK ? AnLine(L_CP1_VOLTA) : B.down_line;
}

// ---- the Valhalla's visit ------------------------------------------------------------------------
// Volta brings her up from below the screen, her bow and railgun showing.
// At checkpoint one -- her first sight of the Iron Reef's garrison -- it
// throws its interceptors at her (her turrets tear them apart), her guns
// break the dome and the generator under it, and a gunship that comes in
// late meets her railgun. At checkpoint two her guns crack the dome and the
// railgun ends the generator. Then she flies on past, up the screen and away.
#define VAL_WAIT    900           // Volta's line stuck: she comes anyway
#define SWARM_T     520           // the mass's longest
#define CHARGE_Y    FX(320)       // her centre while she charges into it: her bow at the screen's top third
static const int kShellGuns[4] = { 0, 1, 4, 5 };   // her forward guns lay the shells

// The garrison throws everything it has at her: flights of interceptors
// from the top and the sides, gunboats, sky mines -- a mass of them.
static void Swarm(int t) {
  static const short kTop[6] = { 110, 148, 186, 224, 262, 300 };
  if (t == 0 || t == 70 || t == 140)
    for (int i = 0; i < 6; i++) SB_SpawnBadGuy(BH, "*andaman/scripts/tobiuo", FX(kTop[i]), FX(-30 - 14 * (i & 1)));
  if (t == 35 || t == 47 || t == 59 || t == 105 || t == 117) {
    const s32 y = FX(50 + (t & 31));
    SB_SpawnBadGuy(BH, "*andaman/scripts/tobiuo_l", FX(-24), y);
    SB_SpawnBadGuy(BH, "*andaman/scripts/tobiuo_r", FX(424), y + FX(10));
  }
  if (t == 20) {
    SB_SpawnBadGuy(BH, "*andaman/scripts/gunboat", FX(140), FX(-30));
    SB_SpawnBadGuy(BH, "*andaman/scripts/gunboat", FX(260), FX(-30));
  }
  if (t == 90)
    for (int i = 0; i < 3; i++) SB_SpawnBadGuy(BH, "*andaman/scripts/skymine", FX(130 + 70 * i), FX(-24));
}

static int SkyClear(void) {
  for (u32 b = SB_GetFirstBadGuy(BH); b; b = SB_GetNextBadGuy(BH, b)) {
    if (SB_GetBadGuyHealth(b) <= 0 || b == B.main || b == B.side[0] || b == B.side[1]) continue;
    s32 x, y;
    SB_GetActorPosition(Act(b), &x, &y);
    if (y > FX(-60) && y < FX(420)) return 0;
  }
  return 1;
}

static void GenBlows(Island* p) {
  const s32 x = GX(p->gy);
  Blast("ExplosionXL", x, p->gy);
  Blast("ExplosionXL", x - FX(18), p->gy + FX(10));
  Blast("ExplosionXL", x + FX(16), p->gy - FX(14));
  for (int k = 0; k < 4; k++) Blast("ExplosionL", x + FX(Rnd(-40, 40)), p->gy + FX(Rnd(-30, 30)));
  SB_ShakeWorld();
  SB_ShakeWorld();
  p->f_gen = 2;
}

// Shell the dome until it fails, then (checkpoint one) the generator until
// it blows: 1 once the generator's turn has come (two) or it is gone (one).
static int Shell(void) {
  Island* p = &I[B.k];
  const int dome_up = p->dome != 0;
  const int n = AnLanded(TAG_DOME);
  if (n) {
    B.hits += n;
    B.flare = 6;
  }
  if (dome_up) {
    if (B.flare > 0) B.flare--;
    p->f_dome = B.hits >= DOME_FAIL ? 2 : (B.flare ? 1 : 0);
    if (B.hits >= DOME_HITS) {
      // the dome fails: it bursts in a ring of fire and is gone
      for (int k = 0; k < 8; k++) {
        const s32 a = k * (TURN / 8);
        Blast(k & 1 ? "ExplosionL" : "ExplosionXS", GX(p->gy) + SB_Cos(a) * 56, p->gy + SB_Sin(a) * 48);
      }
      SB_ShakeWorld();
      AnDrop(&p->dome);
      p->f_gen = 1;                       // overloading
      if (B.k == K_RAIL) return 1;        // the railgun's, now
    }
  }
  B.gen_hits += AnLanded(TAG_GEN);
  if (!dome_up && B.gen_hits >= GEN_HITS && p->f_gen != 2) {
    GenBlows(p);
    return 1;
  }
  // her forward guns: at the dome's near rim while it stands, then the
  // generator; each told a point, and another once it has fired
  for (int i = 0; i < 4; i++) {
    const int k = kShellGuns[i];
    if (AnValBusy(k)) continue;
    if (dome_up) AnValOrder(k, GX(p->gy) + FX(Rnd(-44, 44)), p->gy + FX(44 + Rnd(0, 10)), SHOT_BIG, TAG_DOME, 22);
    else AnValOrder(k, GX(p->gy) + FX(Rnd(-12, 12)), p->gy + FX(Rnd(-6, 16)), SHOT_HUGE, TAG_GEN, 26);
  }
  return 0;
}

static void BtUpdate(u32 boss) {
  SpeechPump();
  B.t++;
  const int scene = InCutscene();
  const int fighting = B.phase == P_INTRO || B.phase == P_FIGHT;
  ShipShield(fighting && scene);
  if (g_autopilot_target && !Alive(g_autopilot_target)) g_autopilot_target = 0;
  if (fighting) {
    Flashes();
    SidesLost();
    if (Alive(B.main)) Guard(B.main, scene || B.phase == P_INTRO || (B.k == K_RAIL && !B.exposed));
    for (int s = 0; s < 2; s++) if (Alive(B.side[s])) Guard(B.side[s], scene || B.phase == P_INTRO);
    BossWards();
  }
  switch (B.phase) {
  case P_INTRO:
    if (B.t > 40 && AnQuiet()) {
      B.phase = P_FIGHT;
      B.t = 0;
    }
    break;
  case P_FIGHT:
    if (B.k == K_FLAK) FlakGuns(scene);
    else RailGuns(scene);
    Looks();
    if (!Alive(B.main)) Blown();
    break;
  case P_BLOWN:
    // she comes up as Volta calls her in
    if (B.t > 40 && (AnLineUp(B.line) || B.t > VAL_WAIT)) {
      AnValCome();
      B.phase = P_COME;
      B.t = 0;
    }
    break;
  case P_COME:
    if (AnValThere()) {
      B.phase = B.k == K_FLAK ? P_SWARM : P_SHELL;
      B.t = 0;
      if (B.k == K_FLAK) {
        AnLine(L_FLY_VALHALLA);                 // the Doc: one of the strongest we have
        AnValStation(CHARGE_Y, 1);              // and she charges into the mass of them
      }
    }
    break;
  case P_SWARM:
    if (B.t < 160) Swarm(B.t);
    if ((B.t > 200 && SkyClear()) || B.t > SWARM_T) {
      AnValStation(FX(470), 0);               // back onto her station, for the generator
      B.phase = P_SHELL;
      B.t = 0;
    }
    break;
  case P_SHELL:
    if (Shell()) {
      if (B.k == K_FLAK) {
        // the generator's gone -- and a gunship comes in late, for the railgun
        // in from the top right at an angle (spawned off the screen's edge
        // -- above the playfield it would be culled at once), to a stop beside
        // the wrecked emplacement: her railgun swings round onto it
        B.tough = SB_SpawnBadGuy(BH, "*andaman/scripts/raider_in", FX(372), FX(40));
        AnValRailOn(B.tough, AnLine(L_FLY_RAIL));   // Volta: generator's down -- that one's mine -- FIRE
      } else {
        I[B.k].f_gen = 1;
        AnValRailAt(GX(I[B.k].gy), I[B.k].gy, 0);   // the railgun, on the bare generator
      }
      B.phase = P_RAIL;
      B.t = 0;
    }
    break;
  case P_RAIL:
    if (Alive(B.tough)) Guard(B.tough, 1);    // (the railgun's strike lifts it)
    if (AnValRailDone()) {
      if (B.k == K_RAIL) {
        GenBlows(&I[B.k]);
        AnLine(L_CP2_GEN);
        AnLine(L_CP2_LEAVE);
      } else {
        AnLine(L_CP1_LEAVE);
      }
      AnValLeave();                         // the shot fired, she flies on past
      B.phase = P_LEAVE;
      B.t = 0;
    }
    break;
  case P_LEAVE:
    if ((AnValGone() || B.t > 900) && AnQuiet()) B.phase = P_OVER;
    break;
  case P_OVER:
    R32(boss, B_STATE) = 0;                 // the engine calls death next
    return;
  }
  // the gun's emitters with the island (it is at rest: the partition holds)
  s32 x, y;
  MainAt(B.k, &x, &y);
  if (Alive(B.main)) Place(B.main, x, y);
  for (int s = 0; s < 2; s++) {
    if (!Alive(B.side[s])) continue;
    SideAt(B.k, s, &x, &y);
    Place(B.side[s], x, y);
  }
  PlaceIsland(B.k);
}

void ReefBossInit(u32 boss);

static void BtDeath(u32 boss) {
  ShipShield(0);
  LanceStop();
  AnValLeave();                             // (forced on: she goes too)
  I[B.k].done = 1;                          // its island rides on out (andaman.c)
  g_fight = -1;
  g_autopilot_target = 0;
  // set up the next fight, and let the level run on to it
  if (B.k == K_FLAK) BatteryBossInit(boss, K_RAIL);
  else ReefBossInit(boss);
  SB_PausePartition(BH, 0);
}

void BatteryBossInit(u32 boss, int k) {
  SB_ClearBytes((void*)boss, BOSS_SIZE);
  SB_ClearBytes(&B, sizeof B);
  B.k = k;
  R32(boss, B_SPAWN + 0x00) = (s32)(u32)BtSpawn;
  R32(boss, B_SPAWN + 0x04) = (s32)(u32)BtUpdate;
  R32(boss, B_SPAWN + 0x08) = (s32)(u32)BtRender;
  R32(boss, B_SPAWN + 0x0c) = (s32)(u32)BtRender;
  R32(boss, B_SPAWN + 0x10) = (s32)(u32)BtDeath;
  R32(boss, B_HEALTH) = (s32)(u32)BtHealth;
}

// InitBoss (worlds.c's row for world 9): a new run of the level, from the
// first checkpoint.
void AndamanReset(void);
void AndamanBossInit(u32 boss) {
  SpeechReset();
  AndamanReset();
  BatteryBossInit(boss, K_FLAK);
}
