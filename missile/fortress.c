// fortress.c -- Mission 13's mid-boss: Captain Ignis's turret fortress, Mark II.
//
// Mission 2's mid-boss, rebuilt (homeward/home_cast.py: its scripts, ported
// and upgraded). As the game's own did it (VolcanoMidBossSpawn/Update), the
// code only builds it and keeps it together; the hull's script runs the
// fight:
//
//   the hull     *homeward/boss/fort, Mission 2's sprite: comes down onto the
//                screen and brakes; invulnerable until its last stand (its
//                own InvincibleRegister);
//   ten turrets  five on its bottom edge, facing the ship, and five on its
//                top edge, facing up -- each a badguy pinned here every frame
//                at its place on the hull, and the hull its parent (so the
//                turrets read the hull's command registers, ParentR15 ...:
//                AM_SetActorParent, as the game's own spawn does);
//   R1           the hull's register for which turrets still stand, kept here
//                every frame: bits 0-2 the top row's centre, middle pair and
//                outer pair; 3-5 the bottom row's (the hull's phases wait on
//                it: the bottom row gone, it slides down past the ship and its
//                top row wakes; the top row gone, its last stand);
//   wrecks       a turret shot off leaves its wreck, smoking (Mission 2's
//                deadturret generics, pinned like the turrets);
//   the end      shot down, or slipped away off the bottom after its last
//                stand: Ignis's parting words, and the magma core is set up
//                for the level's next boss event (inferno.c).
//
// The Mk II's new attacks (ember bombs, broadside curtains as it passes, the
// double helix) are all the hull's script's.
//
// Same rules as missile.c: no libc, no division, engine reached by address.

#include "andaman.h"
#include "homeward_lines.h"
#include "homeward.h"

#define FT_SetActorParent FN(void (*)(u32, u32), 0xc184c)                // ARM

enum { T_HC, T_HCL, T_HCR, T_HL, T_HR, T_BC, T_BCL, T_BCR, T_BL, T_BR, NTUR };
static const char* const kTurName[NTUR] = {
  "*homeward/boss/t_hc", "*homeward/boss/t_hcl", "*homeward/boss/t_hcr", "*homeward/boss/t_hl", "*homeward/boss/t_hr",
  "*homeward/boss/t_bc", "*homeward/boss/t_bcl", "*homeward/boss/t_bcr", "*homeward/boss/t_bl", "*homeward/boss/t_br",
};
// VolcanoMidBossGetTurretPosition: from the hull's centre, +y down
static const signed char kTurX[NTUR] = { 0, -33, 33, -57, 57, 0, -33, 33, -57, 57 };
static const signed char kTurY[NTUR] = { -40, -27, -27, -27, -27, 40, 27, 27, 27, 27 };
// the bit of R1 each one keeps up (a pair keeps its bit while either stands)
static const unsigned char kTurBit[NTUR] = { 1, 2, 2, 4, 4, 8, 16, 16, 32, 32 };
static const unsigned char kTurBig[NTUR] = { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0 };

#define START_Y      (-155)
#define GONE_Y       470            // past here, after its last stand: slipped away

enum { F_UP = 1, F_FIGHT, F_DEAD };

struct {
  u32 hull, tur[NTUR];
  u32 wreck[NTUR];                  // a wreck's actor (a generic)
  s32 x, y;
  int phase, t, said_pass;
  s32 maxhp, hull_hp;
} g_fort;

static s32 FtHealth(u32 boss) {
  (void)boss;
  if (g_fort.phase != F_FIGHT) return 0;
  s32 hp = Alive(g_fort.hull) ? SB_GetBadGuyHealth(g_fort.hull) : 0;
  for (int k = 0; k < NTUR; k++)
    if (g_fort.tur[k] && Alive(g_fort.tur[k])) hp += SB_GetBadGuyHealth(g_fort.tur[k]);
  return hp;
}

static void FtRender(u32 boss, s32 x, s32 y) { (void)boss; (void)x; (void)y; }

static u32 Wreck(int k) {
  const u32 e = SB_SpawnGeneric(BH, kTurBig[k] ? "*homeward/boss/bigdeadturret" : "*homeward/boss/deadturret",
                                g_fort.x + FX(kTurX[k]), g_fort.y + FX(kTurY[k]));
  const u32 a = e ? SB_GetGenericActor(e) : 0;
  if (a) FT_SetActorParent(a, Act(g_fort.hull));
  return a;
}

static void FtSpawn(u32 boss) {
  SB_ClearBytes(&g_fort, sizeof g_fort);
  g_fort.x = FX(200);
  g_fort.y = FX(START_Y);
  g_fort.hull = SB_SpawnBadGuy(BH, "*homeward/boss/fort", g_fort.x, g_fort.y);
  const u32 ha = Act(g_fort.hull);
  for (int k = 0; k < NTUR; k++) {
    g_fort.tur[k] = SB_SpawnBadGuy(BH, kTurName[k], g_fort.x + FX(kTurX[k]), g_fort.y + FX(kTurY[k]));
    if (g_fort.tur[k]) FT_SetActorParent(Act(g_fort.tur[k]), ha);
  }
  g_fort.phase = F_FIGHT;
  R32(boss, B_STATE) = 1;
  SB_PausePartition(BH, 1);
  g_fort.hull_hp = SB_GetBadGuyHealth(g_fort.hull);
  g_fort.maxhp = FtHealth(boss);
  R32(boss, B_MAXHP) = g_fort.maxhp;
  g_autopilot_target = g_fort.hull;
  // Ignis: the hero boy in his doll ship! ... my fortress, Mark Two
  AnLine(HL_IGNIS_IN);
  AnLine(HL_IGNIS_FORT);
  AnLine(HL_DOC_IGNIS);
}

// What the autopilot should shoot: a turret of the row that is fighting, else the hull.
static u32 Target(void) {
  const int top = g_fort.y > FX(200);
  for (int k = 0; k < NTUR; k++) {
    const int bottom_row = k >= T_BC;
    if (bottom_row == top) continue;
    if (g_fort.tur[k] && Alive(g_fort.tur[k])) return g_fort.tur[k];
  }
  return g_fort.hull;
}

static void FtUpdate(u32 boss) {
  g_fort.t++;
  SpeechPump();
  BossWards();
  if (g_fort.phase != F_FIGHT) return;
  if (!Alive(g_fort.hull)) {
    g_fort.phase = F_DEAD;
    R32(boss, B_STATE) = 0;                       // the engine calls death next
    return;
  }
  ShipShield(InCutscene());
  SB_GetActorPosition(Act(g_fort.hull), &g_fort.x, &g_fort.y);
  // its last stand over (R5, its invincibility, cleared for it), it makes
  // off down the screen: let it go. (Its pass goes as low as y 400 or so.)
  if (GetReg(g_fort.hull, 5) == 0 && g_fort.y > FX(GONE_Y)) {
    SB_SetActorPosition(Act(g_fort.hull), FX(200), FX(-400));
    Remove(g_fort.hull);
    g_fort.phase = F_DEAD;
    R32(boss, B_STATE) = 0;
    return;
  }
  // Shut while its script says so -- the hull until its last stand (R5), the
  // top row until it wakes (R6, R10): what their InvincibleRegister lines
  // meant, held from here (invincibility is a countdown here, not a flag).
  {
    const int shut = GetReg(g_fort.hull, 5) != 0;
    Guard(g_fort.hull, shut);
    // ... and whole: sliding down through the ship's place, it rams it (the
    // ship pays a capsule; the hull, as Mission 2's, pays nothing)
    if (shut) SB_SetBadGuyHealth(g_fort.hull, g_fort.hull_hp);
  }
  // the turrets pinned on the hull; the ones shot off, wrecks; R1 kept
  s32 mask = 0;
  for (int k = 0; k < NTUR; k++) {
    const s32 x = g_fort.x + FX(kTurX[k]), y = g_fort.y + FX(kTurY[k]);
    if (g_fort.tur[k]) {
      if (Alive(g_fort.tur[k])) {
        if (k < T_BC) Guard(g_fort.tur[k], GetReg(g_fort.tur[k], k == T_HC ? 6 : 10) != 0);
        Place(g_fort.tur[k], x, y);
        mask |= kTurBit[k];
      } else {
        g_fort.tur[k] = 0;                              // (its handle is reused: let it go)
        g_fort.wreck[k] = Wreck(k);
      }
    }
    if (g_fort.wreck[k]) SB_SetActorPosition(g_fort.wreck[k], x, y);
  }
  SetReg(g_fort.hull, 1, FX(mask));
  // Ignis calls the pass as it starts down past the ship
  if (!g_fort.said_pass && mask < 8 && g_fort.y > FX(60)) {
    g_fort.said_pass = 1;
    AnLine(HL_IGNIS_PASS);
  }
  g_autopilot_target = Target();
}

static void FtDeath(u32 boss) {
  ShipShield(0);
  for (int k = 0; k < NTUR; k++) {
    if (g_fort.tur[k] && Alive(g_fort.tur[k])) Remove(g_fort.tur[k]);
    g_fort.tur[k] = 0;
    if (g_fort.wreck[k]) SB_SetActorPosition(g_fort.wreck[k], FX(-400), FX(-400));
    g_fort.wreck[k] = 0;
  }
  SB_ShakeWorld();
  g_autopilot_target = 0;
  AnLine(HL_IGNIS_FORT_DOWN);       // ... Run home, boy. I'll be waiting where the sea freezes!
  InfernoBossInit(boss);            // the next boss event brings up his core
  SB_PausePartition(BH, 0);
}

void FortBossInit(u32 boss) {
  SB_ClearBytes((void*)boss, BOSS_SIZE);
  SpeechReset();                    // a new run of the level
  HomeReset();
  R32(boss, B_SPAWN + 0x00) = (s32)(u32)FtSpawn;
  R32(boss, B_SPAWN + 0x04) = (s32)(u32)FtUpdate;
  R32(boss, B_SPAWN + 0x08) = (s32)(u32)FtRender;
  R32(boss, B_SPAWN + 0x0c) = (s32)(u32)FtRender;
  R32(boss, B_SPAWN + 0x10) = (s32)(u32)FtDeath;
  R32(boss, B_HEALTH) = (s32)(u32)FtHealth;
}
