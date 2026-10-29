// flagship.c -- Mission 15's last push: the Kagutsuchi, the Hojo wing's assault carrier.
//
// Named for the fire god (docs/M15_DESIGN.md section 5). It comes over the
// base's north edge to finish what the wing started:
//
//   the hull     two generics (kagu_top, kagu_bot: a sprite over ~240 tall
//                can't slide in from the edge in one piece), eased down to
//                the top of the screen; never a target;
//   four guns    on its sponsons (badguys pinned to the hull, turned on the
//                ship by the code; their scripts fire);
//   two bays     near its bow, launching Kamikiri;
//   the keep     its pagoda-roofed command keep: armoured (guarded every
//                frame), a paint target only once the guns and bays are gone
//                (PT_BOSS, AmaPaintEnable). The sun, upgraded at the launch
//                (amaterasu.c: it charges, then the burst comes down twice
//                as wide), burns it out: the carrier burns, the wing breaks
//                (ParisRout), and the last lines (ParisFinale) end the level.
//
// The partition is held from its spawn through the finale (the death lets
// the clock run on to the level's end).
//
// Same rules as missile.c: no libc, no division, engine reached by address.

#include "andaman.h"
#include "paris_lines.h"
#include "paris.h"
#include "amaterasu.h"

enum { G_FL, G_FR, G_RL, G_RR, B_L, B_R, KEEP, NPART };
static const char* const kPartName[NPART] = {
  "*paris/boss/kagu_gun", "*paris/boss/kagu_gun", "*paris/boss/kagu_gun", "*paris/boss/kagu_gun",
  "*paris/boss/kagu_bay", "*paris/boss/kagu_bay", "*paris/boss/kagu_keep",
};
// from the hull's centre (art_legion.kagu_hull: 236 x 232, centre (118, 116)), +y down
static const short kPartX[NPART] = { -84, 84, -84, 84, -46, 46, 0 };
static const short kPartY[NPART] = { 34, 34, -50, -50, 65, 65, -12 };

#define HULL_HALF   58             // each half's centre from the hull's
#define START_Y     (-300)
#define STATION_Y   116
#define KEEP_HP     6000

enum { K_IN = 1, K_FIGHT, K_OPEN, K_BURN, K_END, K_DONE };

static struct {
  u32 top, bot, part[NPART];      // (the hull's halves are badguys: generics drew over its guns)
  s32 x, y;
  int phase, t, said_open, striking, struck;
  s32 maxhp;
} K;

static s32 KgHealth(u32 boss) {
  (void)boss;
  if (K.phase >= K_BURN) return 0;
  s32 hp = 0;
  for (int k = 0; k < KEEP; k++)
    if (K.part[k] && Alive(K.part[k])) hp += SB_GetBadGuyHealth(K.part[k]);
  if (K.phase == K_OPEN) return K.maxhp >> 3;     // (the keep: a sliver the sun must take)
  return hp + (K.maxhp >> 3);
}

static void KgRender(u32 boss, s32 x, s32 y) { (void)boss; (void)x; (void)y; }

static void Hull(void) {
  if (K.top && Alive(K.top)) {
    Place(K.top, K.x, K.y - FX(HULL_HALF));
    Anim(K.top, K.phase >= K_BURN);
    Guard(K.top, 1);
  }
  if (K.bot && Alive(K.bot)) {
    Place(K.bot, K.x, K.y + FX(HULL_HALF));
    Anim(K.bot, K.phase >= K_BURN);
    Guard(K.bot, 1);
  }
}

static void KgSpawn(u32 boss) {
  SB_ClearBytes(&K, sizeof K);
  K.x = FX(200);
  K.y = FX(START_Y);
  K.top = SB_SpawnBadGuy(BH, "*paris/boss/kagu_top", K.x, K.y - FX(HULL_HALF));
  K.bot = SB_SpawnBadGuy(BH, "*paris/boss/kagu_bot", K.x, K.y + FX(HULL_HALF));
  for (int k = 0; k < NPART; k++) {
    K.part[k] = SB_SpawnBadGuy(BH, kPartName[k], K.x + FX(kPartX[k]), K.y + FX(kPartY[k]));
    if (K.part[k]) Guard(K.part[k], 1);
  }
  AmaPaintAdd(K.part[KEEP], PT_BOSS);
  AmaPaintEnable(K.part[KEEP], 0);
  K.phase = K_IN;
  R32(boss, B_STATE) = 1;
  SB_PausePartition(BH, 1);
  K.maxhp = 0;
  for (int k = 0; k < KEEP; k++)
    if (K.part[k]) K.maxhp += SB_GetBadGuyHealth(K.part[k]);
  R32(boss, B_MAXHP) = K.maxhp + (K.maxhp >> 3);
  Hull();
  AnLine(PL_DOC_CARRIER);
  AnLine(PL_VOLTA_KEEP);
  AnLine(PL_DOC_PAINT);
}

static u32 Target(void) {
  static const unsigned char kOrder[KEEP] = { G_FL, G_FR, B_L, B_R, G_RL, G_RR };
  for (int i = 0; i < KEEP; i++) {
    const u32 t = K.part[kOrder[i]];
    if (t && Alive(t)) return t;
  }
  return 0;
}

static void KgUpdate(u32 boss) {
  K.t++;
  SpeechPump();
  BossWards();
  if (K.phase == K_DONE) return;
  ShipShield(InCutscene() && K.phase < K_BURN);
  SB_PausePartition(BH, 1);
  // down to its station, a slow sway
  if (K.phase == K_IN || K.phase == K_FIGHT || K.phase == K_OPEN) {
    K.y += Ease(FX(STATION_Y) - K.y, FX(1.1));
    K.x = FX(200) + SB_MulFP(FX(26), SB_Sin(K.t * 0x18000));
    if (K.phase == K_IN && K.y > FX(STATION_Y - 4)) K.phase = K_FIGHT;
  } else if (K.phase == K_END && K.top) {          // burnt out, it drifts off astern ...
    K.y -= FX(0.6);
    if (K.y < FX(-300)) {                           // ... and is gone (past -512 it would wrap to the bottom)
      for (int h = 0; h < 2; h++) {
        const u32 b = h ? K.bot : K.top;
        if (b && Alive(b)) {
          Place(b, FX(200), FX(-460));
          Remove(b);
        }
      }
      K.top = K.bot = 0;
    }
  }
  Hull();
  // (by its outline: the hull narrows to its bow -- art_legion.kagu_hull, every 8 px from its top)
  static const unsigned char kHalf[30] = { 0, 51, 58, 64, 71, 78, 79, 81, 82, 84, 86, 87, 89, 90, 91,
                                           90, 90, 89, 88, 87, 86, 86, 79, 73, 67, 61, 55, 43, 26, 0 };
  if (K.top || K.bot) ParisCoverOutline(K.x, K.y - FX(116), kHalf, 30);
  int left = 0;
  for (int k = 0; k < NPART; k++) {
    const u32 p = K.part[k];
    if (!p) continue;
    if (!Alive(p)) {
      AmaPaintRemove(p);
      AnBlast(K.x + FX(kPartX[k]), K.y + FX(kPartY[k]));
      K.part[k] = 0;
      continue;
    }
    Place(p, K.x + FX(kPartX[k]), K.y + FX(kPartY[k]));
    if (k < B_L) AnTurn(Act(p), AnAngle(K.x + FX(kPartX[k]), K.y + FX(kPartY[k]), PlayerX(), PlayerY()));
    if (k == KEEP) {
      Guard(p, 1);                                  // armoured: the sun only
      Anim(p, K.phase >= K_BURN ? 2 : (K.phase == K_OPEN && AmaStriking()) ? 1 : 0);
      SetReg(p, 14, K.phase == K_OPEN ? FX(1) : 0); // (its script: the spiral, once it's open)
    } else {
      Guard(p, K.phase == K_IN || InCutscene());
      left++;
    }
  }
  if (K.phase == K_FIGHT && left == 0 && K.part[KEEP]) {
    K.phase = K_OPEN;
    AmaPaintEnable(K.part[KEEP], 1);
    AnLine(PL_DOC_OPEN);
  }
  if (K.phase == K_OPEN) {
    if (AmaStriking() && !K.striking) { K.striking = 1; AnLine(PL_DOC_STRIKE); }
    if (!AmaStriking()) K.striking = 0;
    if (K.part[KEEP] && AmaStruck(K.part[KEEP])) {
      // burnt out: the keep goes, the carrier burns, the wing breaks
      K.phase = K_BURN;
      K.t = 0;
      AmaPaintRemove(K.part[KEEP]);
      Remove(K.part[KEEP]);
      K.part[KEEP] = 0;
      SB_ShakeWorld();
      ParisRout();
    }
  }
  if (K.phase == K_BURN) {
    if ((K.t & 5) == 0) AnBlast(K.x + FX(Rnd(-100, 101)), K.y + FX(Rnd(-100, 101)));
    if (K.t > 200) {
      K.phase = K_END;
      K.t = 0;
      ParisFinale();
    }
  }
  if (K.phase == K_END) {
    if ((K.t & 15) == 0 && K.y > FX(-200)) AnBlast(K.x + FX(Rnd(-90, 91)), K.y + FX(Rnd(-90, 91)));
    if (ParisFinaleDone()) {
      K.phase = K_DONE;
      R32(boss, B_STATE) = 0;                       // the engine calls death next
    }
  }
  g_autopilot_target = K.phase == K_FIGHT ? Target() : 0;
}

static void KgDeath(u32 boss) {
  (void)boss;
  ShipShield(0);
  for (int k = 0; k < NPART; k++) {
    if (K.part[k]) {
      AmaPaintRemove(K.part[k]);
      if (Alive(K.part[k])) Remove(K.part[k]);
    }
    K.part[k] = 0;
  }
  for (int h = 0; h < 2; h++) {
    const u32 b = h ? K.bot : K.top;
    if (b && Alive(b)) {                            // away quietly, off the playfield
      Place(b, FX(200), FX(-460));
      Remove(b);
    }
  }
  K.top = K.bot = 0;
  g_autopilot_target = 0;
  SB_PausePartition(BH, 0);                         // on to the level's end
}

// The Kanabo's death (tank.c) sets the carrier up for the next boss event.
void FlagshipBossInstall(u32 boss) {
  SB_ClearBytes((void*)boss, BOSS_SIZE);
  R32(boss, B_SPAWN + 0x00) = (s32)(u32)KgSpawn;
  R32(boss, B_SPAWN + 0x04) = (s32)(u32)KgUpdate;
  R32(boss, B_SPAWN + 0x08) = (s32)(u32)KgRender;
  R32(boss, B_SPAWN + 0x0c) = (s32)(u32)KgRender;
  R32(boss, B_SPAWN + 0x10) = (s32)(u32)KgDeath;
  R32(boss, B_HEALTH) = (s32)(u32)KgHealth;
}
