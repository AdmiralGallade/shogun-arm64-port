// barge.c -- Mission 14's mid-boss: Lord Raijin's gun barge, Mark II.
//
// Mission 4's mid-boss (amaterasu/ama_cast.py: its scripts, ported). As the
// game's own did it (StormMidBossSpawn/Update), the code only builds it and
// keeps it together; the hull's script runs the fight:
//
//   the hull      *amaterasu/boss/barge, Mission 4's sprite: down to the top
//                 of the screen, then lower as its turrets go (never a target);
//   ten turrets   eight small ones in four pairs, and two big ones on the
//                 centre line -- badguys pinned here every frame at their
//                 places on the hull (StormMidBossGetTurretPosition's table),
//                 the hull their parent (they read its R1, R2, R10, R12, R13);
//   R10           the hull's register for which small turrets still stand,
//                 kept here every frame (its phases wait on it: 63, 15, 0 --
//                 told by phase, PhaseMask: the sun kills them out of order);
//   armour        the Mk II's big turrets are armoured against guns: painted
//                 and struck by Amaterasu (PT_ARMOUR), they're gone -- and any
//                 small turret in the beam goes with them (PT_PART);
//   the storm     from its second phase, Raijin calls bolts down on the ship
//                 now and then (AmaThunder: a violet warning line, then the
//                 strike) -- the barge's new attack;
//   the end       its small turrets gone, it makes off (its RageQuit): Raijin's
//                 parting words, and his dragon is set up for the next boss
//                 event (raijin.c).
//
// Same rules as missile.c: no libc, no division, engine reached by address.

#include "andaman.h"
#include "amaterasu_lines.h"
#include "amaterasu.h"

#define BG_SetActorParent FN(void (*)(u32, u32), 0xc184c)                // ARM

enum { T_1HD, T_1HG, T_1BD, T_1BG, T_2HD, T_2HG, T_2BG, T_2BD, T_BIGH, T_BIGB, NTUR };
static const char* const kTurName[NTUR] = {
  "*amaterasu/boss/b_tourelle_1_hd", "*amaterasu/boss/b_tourelle_1_hg", "*amaterasu/boss/b_tourelle_1_bd",
  "*amaterasu/boss/b_tourelle_1_bg", "*amaterasu/boss/b_tourelle_2_hd", "*amaterasu/boss/b_tourelle_2_hg",
  "*amaterasu/boss/b_tourelle_2_bg", "*amaterasu/boss/b_tourelle_2_bd", "*amaterasu/boss/b_tourelle_big_h",
  "*amaterasu/boss/b_tourelle_big_b",
};
// from the hull's centre, +y down (docs/BOSS_ATTACKS_M2_M4.md 3.1)
static const signed char kTurX[NTUR] = { 30, -30, 30, -30, 30, -30, -32, 32, 1, 1 };
static const signed char kTurY[NTUR] = { -75, -75, -15, -15, 15, 15, 50, 50, -80, 0 };

#define START_Y      (-170)
#define THUNDER_EVERY 300

enum { F_FIGHT = 1, F_LEAVE, F_DEAD };

static struct {
  u32 hull, tur[NTUR];
  s32 x, y;
  int phase, t, leave_t, thunder, said_bolt, bolt;
  s32 maxhp;
} g_barge;

static s32 BgHealth(u32 boss) {
  (void)boss;
  if (g_barge.phase != F_FIGHT) return 0;
  s32 hp = 0;
  for (int k = 0; k < T_BIGH; k++)
    if (g_barge.tur[k] && Alive(g_barge.tur[k])) hp += SB_GetBadGuyHealth(g_barge.tur[k]);
  return hp;
}

static void BgRender(u32 boss, s32 x, s32 y) { (void)boss; (void)x; (void)y; }

static void BgSpawn(u32 boss) {
  SB_ClearBytes(&g_barge, sizeof g_barge);
  g_barge.x = FX(200);
  g_barge.y = FX(START_Y);
  g_barge.hull = SB_SpawnBadGuy(BH, "*amaterasu/boss/barge", g_barge.x, g_barge.y);
  const u32 ha = Act(g_barge.hull);
  for (int k = 0; k < NTUR; k++) {
    g_barge.tur[k] = SB_SpawnBadGuy(BH, kTurName[k], g_barge.x + FX(kTurX[k]), g_barge.y + FX(kTurY[k]));
    if (g_barge.tur[k]) BG_SetActorParent(Act(g_barge.tur[k]), ha);
  }
  AmaPaintAdd(g_barge.tur[T_BIGH], PT_ARMOUR);   // painted, struck: gone
  AmaPaintAdd(g_barge.tur[T_BIGB], PT_ARMOUR);
  for (int k = 0; k < T_BIGH; k++) AmaPaintAdd(g_barge.tur[k], PT_PART);   // in the beam: gone too
  g_barge.phase = F_FIGHT;
  g_barge.thunder = THUNDER_EVERY;
  g_barge.bolt = -1;
  R32(boss, B_STATE) = 1;
  SB_PausePartition(BH, 1);
  g_barge.maxhp = BgHealth(boss);
  R32(boss, B_MAXHP) = g_barge.maxhp;
  g_autopilot_target = g_barge.tur[T_2BD];
  AnLine(AL_DOC_BARGE);
  AnLine(AL_RAIJIN_BARGE);
}

// What the autopilot should shoot: a small turret of the pair that's fighting.
static u32 Target(void) {
  static const unsigned char kOrder[8] = { T_2BD, T_2BG, T_2HD, T_2HG, T_1HD, T_1HG, T_1BD, T_1BG };
  for (int i = 0; i < 8; i++) {
    const u32 t = g_barge.tur[kOrder[i]];
    if (t && Alive(t)) return t;
  }
  return 0;
}

// The hull's phases wait for R10 to be exactly 63, then 15, then 0 -- Mission
// 4's turrets died in that order. The sun kills whatever is in its beam, out
// of order, so the hull is told the mask its phase is waiting for: the pair
// it fights first (bits 6, 7), then the next (4, 5), then the last four.
static s32 PhaseMask(s32 mask) {
  if (mask & 0xc0) return 0xff;
  if (mask & 0x30) return 63;
  if (mask & 0x0f) return 15;
  return 0;
}

static void BgUpdate(u32 boss) {
  g_barge.t++;
  SpeechPump();
  BossWards();
  if (g_barge.phase == F_DEAD) return;
  if (!Alive(g_barge.hull)) {
    g_barge.phase = F_DEAD;
    R32(boss, B_STATE) = 0;
    return;
  }
  ShipShield(InCutscene());
  SB_GetActorPosition(Act(g_barge.hull), &g_barge.x, &g_barge.y);
  s32 mask = 0;
  for (int k = 0; k < NTUR; k++) {
    const u32 t = g_barge.tur[k];
    if (!t) continue;
    if (!Alive(t)) {
      AmaPaintRemove(t);
      g_barge.tur[k] = 0;
      continue;
    }
    Place(t, g_barge.x + FX(kTurX[k]), g_barge.y + FX(kTurY[k]));
    if (k < T_BIGH) {
      mask |= 1 << k;
      Guard(t, GetReg(t, 14) != 0 || InCutscene());
    } else {
      Guard(t, 1);                    // armoured: only the sun takes it (amaterasu.c Burn)
    }
  }
  SetReg(g_barge.hull, 10, FX(PhaseMask(mask)));
  // the storm: from its second phase, a bolt on the ship now and then
  if (mask != 0xff && g_barge.phase == F_FIGHT && !InCutscene() && --g_barge.thunder <= 0) {
    g_barge.thunder = THUNDER_EVERY - (HardMode() ? 90 : 0);
    if (!AmaThunderBusy()) {
      g_barge.bolt = AmaBolt(g_barge.x, g_barge.y + FX(40), PlayerX(), PlayerY(), 80);
      if (!g_barge.said_bolt) {       // the first: whose it is (not the sun's)
        g_barge.said_bolt = 1;
        AnLine(AL_RAIJIN_BOLT);
        AnLine(AL_DOC_LIGHTNING);
      }
    }
  }
  AmaBoltFrom(g_barge.bolt, g_barge.x, g_barge.y + FX(40));   // (from its hull, as it moves)
  if (mask == 0 && g_barge.phase == F_FIGHT) {
    g_barge.phase = F_LEAVE;          // its RageQuit: it makes off
    g_barge.leave_t = 0;
    for (int k = T_BIGH; k < NTUR; k++)
      if (g_barge.tur[k]) {
        AmaPaintRemove(g_barge.tur[k]);
        Remove(g_barge.tur[k]);
        g_barge.tur[k] = 0;
      }
  }
  if (g_barge.phase == F_LEAVE) {
    if (++g_barge.leave_t > 240 || g_barge.x < FX(-120) || g_barge.y < FX(-150)) {
      SetReg(g_barge.hull, 15, FX(1));
      SB_SetActorPosition(Act(g_barge.hull), FX(200), FX(-400));
      Remove(g_barge.hull);
      g_barge.phase = F_DEAD;
      R32(boss, B_STATE) = 0;
    }
    g_autopilot_target = 0;
    return;
  }
  g_autopilot_target = Target();
}

static void BgDeath(u32 boss) {
  ShipShield(0);
  for (int k = 0; k < NTUR; k++) {
    if (g_barge.tur[k]) {
      AmaPaintRemove(g_barge.tur[k]);
      if (Alive(g_barge.tur[k])) Remove(g_barge.tur[k]);
    }
    g_barge.tur[k] = 0;
  }
  SB_ShakeWorld();
  g_autopilot_target = 0;
  AnLine(AL_RAIJIN_BARGE_DOWN);     // My storm is waiting for you, insect
  RaijinBossInit(boss);             // the next boss event brings up his dragon
  SB_PausePartition(BH, 0);
}

void BargeBossInit(u32 boss) {
  SB_ClearBytes((void*)boss, BOSS_SIZE);
  SpeechReset();                    // a new run of the level
  AmaReset();
  R32(boss, B_SPAWN + 0x00) = (s32)(u32)BgSpawn;
  R32(boss, B_SPAWN + 0x04) = (s32)(u32)BgUpdate;
  R32(boss, B_SPAWN + 0x08) = (s32)(u32)BgRender;
  R32(boss, B_SPAWN + 0x0c) = (s32)(u32)BgRender;
  R32(boss, B_SPAWN + 0x10) = (s32)(u32)BgDeath;
  R32(boss, B_HEALTH) = (s32)(u32)BgHealth;
}
