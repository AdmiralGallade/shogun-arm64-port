// twins.c -- Seoul's final boss: the Shogun's heirs and their titans.
//
// Prince Kaito in the Azure Dragoon and Princess Akane in the Vermilion
// Empress (seoul/art_titans.py), who join into Yongbong, the Dragon-Phoenix
// Emperor. Every piece is a badguy placed each frame from here; their one
// shared script (seoul/cast.py, boss_script) fires whatever attack this code
// asks for.
//
//   enter     both titans drop in, side by side, and the heirs talk (Kaito
//             flirts with the hero; Akane tells him to fight; he tells her
//             to mind her own business, given her own luck with a certain
//             green-goggled scientist);
//   duet      they fight together. Kaito: crescents of kunai, the rail beam,
//             dashes that leave kunai hanging in the air, which then fly.
//             Akane: sweeping blossom fans, missile swarms, walls of
//             petals, blooms. Every fourth turn, the taegeuk: both spin
//             spirals the opposite way at once. A titan beaten down kneels
//             (they have far more health than this takes; the code holds
//             them at a floor) and the other fights on, faster;
//   union     with both down: "Royal Union!" They fly together and, in a
//             flash, become the Emperor: a torso, which cannot be hurt
//             while it has arms, the greatsword arm and the fan-cannon arm.
//             The arms slash, sweep, rain petals and -- the rocket punch --
//             launch their fists at the ship; the torso turns orb spirals,
//             crosses two beams on the ship, and rings orbs round it with a
//             moving gap;
//   overdrive with both arms gone the armour cracks open: everything at
//             once, faster as it weakens, between its other attacks;
//   dying     held at its last floor it stops, explodes in pieces while
//             both heirs have their last words, and only then dies.
//
// The scroll stops for the fight (seoul/timeline.py): it is fought in front
// of Gyeongbokgung's throne hall.

#include "seoul_boss.h"
#include "glow.h"
#include "seoul_glow.h"

enum { T_ENTER = 1, T_TALK, T_DUET, T_MERGE, T_UNION, T_OVERDRIVE, T_DYING, T_DEAD };

#define KAITO 0
#define AKANE 1

// titans: health far above what is taken; beaten at FLOOR + 0
#define TITAN_HP    7600        // what the duet takes from each
#define UNION_HP    16000       // the torso, once its arms are gone
#define HOME_Y      112
#define UNION_Y     104
#define ARM_DX      35          // the arms hang at the shoulders (art_titans.py)
#define ARM_DY      27
// the rocket punch (fist_*.generic): a hit is the ship within this of the
// fist's centre; the fist lives 2 + 16 + 120 steps
#define FIST_R      15
#define FIST_LIFE   260
#define FIST_GRACE  90

static struct {
  u32 boss;                     // the BOSS record
  u32 titan[2], torso, arm[2], fist[2];
  int titan_down[2], arm_alive[2];
  s32 floor_[2], torso_floor;
  s32 x[2], y[2];               // titans' centres, 16.16
  s32 ux, uy;                   // the union's torso
  int phase, t, cool[2], turn[2], seq[2], combo, solo;
  int dash, dash_dir;           // Kaito's dash: frames left
  s32 dash_to;
  u32 fist_born[2];
  int fist_grace, arm_regrow[2];
  int said_down, said_arm, merge_t, od_t, dying_t, od_cmd;
  u32 frame;
  u32 glow[2], uglow;           // the reactors turning (glow.h): the titans', the union's
} T;

static s32 Hp(u32 bg) { return SB_GetBadGuyHealth(bg); }

// The next entry of an attack list of n, wrapping (no modulo in the blob:
// a subtraction loop gets turned into one by the compiler).
static int Next(int* i, int n) {
  const int k = *i;
  if (++*i >= n) *i = 0;
  return k >= n ? 0 : k;
}

static s32 TwHealth(u32 boss) {
  (void)boss;
  s32 hp = 0;
  switch (T.phase) {
  case T_ENTER: case T_TALK: case T_DUET:
    for (int i = 0; i < 2; i++)
      if (!T.titan_down[i]) hp += Hp(T.titan[i]) - T.floor_[i];
    return hp;
  case T_MERGE: case T_UNION:
    for (int i = 0; i < 2; i++)
      if (T.arm_alive[i]) hp += Hp(T.arm[i]);
    return hp;
  case T_OVERDRIVE:
    return Hp(T.torso) - T.torso_floor;
  }
  return 0;
}

static void TwRender(u32 boss, s32 x, s32 y) { (void)boss; (void)x; (void)y; }

static void TwSpawn(u32 boss) {
  SB_ClearBytes(&T, sizeof T);
  T.boss = boss;
  T.x[KAITO] = FX(142);
  T.x[AKANE] = FX(258);
  for (int i = 0; i < 2; i++) {
    T.y[i] = FX(-100 - 30 * i);
    T.titan[i] = SB_SpawnBadGuy(BH, i == KAITO ? "*seoul/boss/dragoon" : "*seoul/boss/empress", T.x[i], T.y[i]);
    T.glow[i] = GlowSpawn(BH, i == KAITO ? "*seoul/boss/dragoon_glow" : "*seoul/boss/empress_glow");
    T.floor_[i] = Hp(T.titan[i]) - TITAN_HP;
    T.cool[i] = 40 + 30 * i;
  }
  T.phase = T_ENTER;
  R32(boss, B_STATE) = 1;
  SB_PausePartition(BH, 1);
  R32(boss, B_MAXHP) = 2 * TITAN_HP;
  g_autopilot_target = T.titan[KAITO];
  // Kaito: "Nobody told me you were this cute." / Akane: "Stop flirting
  // and FIGHT!" / Kaito: "...green-goggled scientist..." / Akane: "Sh-shut up!"
  for (int k = 0; k < 4; k++) Say();
}

// ---- the duet ------------------------------------------------------------------------
static int Other(int i) { return 1 - i; }

static void TitanAttack(int i) {
  const u32 bg = T.titan[i];
  T.turn[i]++;
  const int alone = T.titan_down[Other(i)];
  // the taegeuk: both at once, every fourth of Kaito's turns
  if (i == KAITO && !alone && (T.turn[i] & 3) == 0 && !Busy(T.titan[AKANE])) {
    Order(T.titan[KAITO], C_SPIRAL_K);
    Order(T.titan[AKANE], C_SPIRAL_B);
    T.combo = 190;
    T.cool[KAITO] = T.cool[AKANE] = 40;
    return;
  }
  if (i == KAITO) {
    static const unsigned char kK[] = { C_CRESCENT, C_RAIL, C_TRAIL, C_CRESCENT, C_SPIRAL_K, C_RAIL, C_TRAIL };
    // hard mode (docs/HARD_MODE.md): his pincers and snipe rings among them
    static const unsigned char kKH[] = { C_CRESCENT, C_XPINCER, C_RAIL, C_TRAIL, C_XSNIPE, C_SPIRAL_K, C_RAIL };
    const int hard = HardMode();
    const int k = Next(&T.seq[i], hard ? sizeof kKH : sizeof kK);
    const int cmd = hard ? kKH[k] : kK[k];
    if (cmd == C_TRAIL) {
      // the dash: across to the far side, high, kunai hanging behind him
      T.dash = 1;
      T.dash_dir = T.x[i] < FX(200) ? 1 : -1;
      T.dash_to = FX(T.dash_dir > 0 ? 300 : 100);
    }
    if (cmd == C_SPIRAL_K) T.combo = 150;
    Order(bg, cmd);
  } else {
    static const unsigned char kA[] = { C_BLOOMFAN, C_MISSILES, C_PETALS, C_BLOOM, C_BLOOMFAN, C_SPIRAL_B, C_PETALS };
    // hard mode: her twist rings, the snake and drifting-gap rings among them
    static const unsigned char kAH[] = { C_BLOOMFAN, C_XTWIST, C_MISSILES, C_PETALS, C_XSNAKE, C_BLOOM, C_SPIRAL_B, C_XGAP };
    const int hard = HardMode();
    const int k = Next(&T.seq[i], hard ? sizeof kAH : sizeof kA);
    const int cmd = hard ? kAH[k] : kA[k];
    if (cmd == C_SPIRAL_B) T.combo = 150;
    Order(bg, cmd);
  }
  T.cool[i] = HardMode() ? (alone ? 16 : 36) : (alone ? 22 : 46);
}

static void Duet(void) {
  T.t++;
  // the spirals run until stopped
  if (T.combo > 0 && --T.combo == 0)
    for (int i = 0; i < 2; i++)
      if (!T.titan_down[i]) Stop(T.titan[i]);
  for (int i = 0; i < 2; i++) {
    // beaten down: hold it at its floor, kneeling, out of the fight
    if (!T.titan_down[i] && Hp(T.titan[i]) <= T.floor_[i] + 1) {
      T.titan_down[i] = 1;
      SB_SetBadGuyHealth(T.titan[i], T.floor_[i] + 1);
      Stop(T.titan[i]);
      SB_ShakeWorld();
      SB_SpawnExplosion(BH, "ExplosionL", T.x[i], T.y[i]);
      if (!T.said_down) { T.said_down = 1; Say(); }   // Akane: "He's better than the reports said"
      g_autopilot_target = T.titan[Other(i)];
    }
  }
  if (T.titan_down[KAITO] && T.titan_down[AKANE]) {
    T.phase = T_MERGE;
    T.merge_t = 0;
    Say();                                            // Kaito: "Enough games."
    Say();                                            // Akane: "Royal Union!"
    return;
  }
  // movement: a hover, and Kaito's dash
  for (int i = 0; i < 2; i++) {
    const s32 phase = (s32)((T.t + i * 90) << 15);
    s32 wantx = (i == KAITO ? FX(142) : FX(258)) + SB_MulFP(SB_Sin(phase), FX(18));
    s32 wanty = FX(HOME_Y) + SB_MulFP(SB_Sin(phase << 1), FX(7));
    if (T.titan_down[i]) wanty = FX(HOME_Y - 34);    // knelt back, out of the way
    if (i == KAITO && T.dash) {
      const s32 d = T.dash_to - T.x[i];
      T.x[i] += d > FX(3) ? FX(3) : (d < -FX(3) ? -FX(3) : d);
      T.y[i] += Ease(FX(HOME_Y - 28) - T.y[i], FX(1.5));
      Anim(T.titan[i], A_BOOST);
      if (d < FX(2) && d > -FX(2)) {
        T.dash = 0;
        Stop(T.titan[i]);
      }
      continue;
    }
    // when one is down, the other takes the middle
    if (T.titan_down[Other(i)] && !T.titan_down[i]) wantx = FX(200) + SB_MulFP(SB_Sin(phase), FX(60));
    T.x[i] += Ease(wantx - T.x[i], FX(1.2));
    T.y[i] += Ease(wanty - T.y[i], FX(1));
    if (T.titan_down[i]) Anim(T.titan[i], A_DOWN);
  }
  if (InCutscene()) return;
  for (int i = 0; i < 2; i++) {
    if (T.titan_down[i] || Busy(T.titan[i])) continue;
    if (T.combo > 0) continue;
    if (T.cool[i] > 0) { T.cool[i]--; continue; }
    // take turns: not both at once, except in the taegeuk
    if (!T.titan_down[Other(i)] && Busy(T.titan[Other(i)])) continue;
    TitanAttack(i);
  }
}

// ---- the union ---------------------------------------------------------------------
static void ArmPos(int i, s32* x, s32* y) {
  *x = T.ux + (i == 0 ? -FX(ARM_DX) : FX(ARM_DX));
  *y = T.uy + FX(ARM_DY);
}

static void Merge(void) {
  // the heirs have their say first ("Father built these titans..." /
  // "Royal Union!"): both titans rise from their knees and hold, and only
  // once the last line closes do they fly together
  if (T.merge_t == 0 && InCutscene()) {
    for (int i = 0; i < 2; i++) {
      T.y[i] += Ease(FX(HOME_Y) - T.y[i], FX(1));
      Anim(T.titan[i], A_IDLE);
    }
    return;
  }
  T.merge_t++;
  const int t = T.merge_t;
  // both fly together, up into the middle
  for (int i = 0; i < 2; i++) {
    if (!T.titan[i]) continue;                     // gone into the union
    T.x[i] += Ease(FX(200 + (i ? 20 : -20)) - T.x[i], FX(2.5));
    T.y[i] += Ease(FX(UNION_Y) - T.y[i], FX(2));
    Anim(T.titan[i], A_BOOST);
  }
  if (t == 90 || t == 100 || t == 110) {
    SB_ShakeWorld();
    SB_SpawnExplosion(BH, "ExplosionXL", FX(200), FX(UNION_Y));
  }
  if (t == 110) {
    // the flash: the titans are gone, the Emperor stands there
    for (int i = 0; i < 2; i++) {
      SB_SetBadGuyHealth(T.titan[i], 1);
      Remove(T.titan[i]);
      T.titan[i] = 0;
      GlowDrop(&T.glow[i]);
    }
    T.ux = FX(200);
    T.uy = FX(UNION_Y);
    T.torso = SB_SpawnBadGuy(BH, "*seoul/boss/union", T.ux, T.uy);
    T.uglow = GlowSpawn(BH, "*seoul/boss/union_glow");
    for (int i = 0; i < 2; i++) {
      s32 ax, ay;
      ArmPos(i, &ax, &ay);
      T.arm[i] = SB_SpawnBadGuy(BH, i == 0 ? "*seoul/boss/sword_arm" : "*seoul/boss/fan_arm", ax, ay);
      T.arm_alive[i] = T.arm[i] != 0;
      T.cool[i] = 60 + 40 * i;
      T.turn[i] = 0;
      T.seq[i] = 0;
    }
    T.torso_floor = Hp(T.torso) - UNION_HP;
    R32(T.boss, B_MAXHP) = TwHealth(T.boss);         // the bar refills: the arms
    g_autopilot_target = T.arm[0];
  }
  if (t > 110 && !InCutscene() && t > 200) {
    T.phase = T_UNION;
    T.t = 0;
    T.solo = 0;
    T.od_t = 120;
  }
}

// The rocket punch: the fist flies at where the ship is now. It is a
// generic entity -- it can never ram the ship, which costs a whole capsule
// -- so the hit is checked here and struck as a bullet's.
static void Punch(int i) {
  s32 ax, ay;
  ArmPos(i, &ax, &ay);
  ay += FX(40);
  const s32 aim = SB_GetAngle(ax >> 16, ay >> 16, PlayerX() >> 16, PlayerY() >> 16);
  const u32 e = SB_SpawnGeneric(BH, i == 0 ? "*seoul/boss/fist_sword" : "*seoul/boss/fist_fan", ax, ay);
  const u32 a = e ? SB_GetGenericActor(e) : 0;
  T.fist[i] = a;
  T.fist_born[i] = T.frame;
  if (a) {
    SB_SetActorPosition(a, ax, ay);
    SB_SetActorOrient(a, aim & (TURN - 1));
    SB_SetActorRegister(a, REG_AIM, aim & (TURN - 1));
  }
  Anim(T.arm[i], A_DOWN);                          // the arm's empty wrist
  T.arm_regrow[i] = 170;
}

static void PunchHits(void) {
  if (T.fist_grace > 0) { T.fist_grace--; return; }
  const u32 sh = Shogun();
  if (*(volatile unsigned short*)(sh + SH_HITLOCK) || R32(sh, SH_HITLOCK2)) return;
  for (int i = 0; i < 2; i++) {
    const u32 a = T.fist[i];
    if (!a || T.frame - T.fist_born[i] > FIST_LIFE) continue;
    s32 fx, fy;
    SB_GetActorPosition(a, &fx, &fy);
    const s32 dx = PlayerX() - fx, dy = PlayerY() - fy;
    if (dx > FX(FIST_R) || dx < -FX(FIST_R) || dy > FX(FIST_R) || dy < -FX(FIST_R)) continue;
    SB_StrikePlayer(sh + SH_PLAYER, 0, sh + SH_HUD);
    T.fist_grace = FIST_GRACE;
    return;
  }
}

static void ArmAttack(int i) {
  T.turn[i]++;
  const int alone = !T.arm_alive[1 - i];
  const int hard = HardMode();
  if (i == 0) {
    static const unsigned char kS[] = { C_SLASH, C_CRESCENT, 0, C_SLASH, C_RAIL, 0 };
    static const unsigned char kSH[] = { C_SLASH, C_XPINCER, 0, C_SLASH, C_RAIL, C_XSNIPE, 0 };
    const int k = Next(&T.seq[i], hard ? sizeof kSH : sizeof kS);
    const int cmd = hard ? kSH[k] : kS[k];
    if (!cmd) Punch(i);
    else Order(T.arm[i], cmd);
  } else {
    static const unsigned char kF[] = { C_BLOOMFAN, C_PETALS, 0, C_MISSILES, C_BLOOM, 0 };
    static const unsigned char kFH[] = { C_BLOOMFAN, C_XTWIST, 0, C_MISSILES, C_XGAP, C_BLOOM, 0 };
    const int k = Next(&T.seq[i], hard ? sizeof kFH : sizeof kF);
    const int cmd = hard ? kFH[k] : kF[k];
    if (!cmd) Punch(i);
    else Order(T.arm[i], cmd);
  }
  T.cool[i] = hard ? (alone ? 26 : 54) : (alone ? 34 : 70);
}

static void TorsoAttack(void) {
  // its own four (the Dragon and the Phoenix as one) among the heirs' old ones
  static const unsigned char kT[] = { C_ORBS, C_WINGS, C_CROSS, C_YINYANG, C_SEAL, C_COIL, C_BLOOM, C_SCALES,
                                      C_CROSS, C_ORBS, C_WINGS, C_SEAL, C_COIL, C_YINYANG };
  // hard mode: the snake and twist rings woven into its own
  static const unsigned char kTH[] = { C_ORBS, C_XSNAKE, C_WINGS, C_CROSS, C_XTWIST, C_YINYANG, C_SEAL, C_COIL,
                                       C_XSNIPE, C_SCALES, C_CROSS, C_XGAP, C_ORBS, C_WINGS, C_YINYANG };
  const int hard = HardMode();
  const int k = Next(&T.solo, hard ? sizeof kTH : sizeof kT);
  const int cmd = hard ? kTH[k] : kT[k];
  Order(T.torso, cmd);
  if (cmd == C_ORBS) T.combo = 200;
}

static void UnionMove(void) {
  T.t++;
  const s32 ph = (s32)(T.t << 14);
  T.ux += Ease(FX(200) + SB_MulFP(SB_Sin(ph), FX(30)) - T.ux, FX(0.8));
  T.uy += Ease(FX(UNION_Y) + SB_MulFP(SB_Sin(ph << 1), FX(6)) - T.uy, FX(0.6));
}

static void Union(void) {
  UnionMove();
  for (int i = 0; i < 2; i++) {
    if (T.arm_alive[i] && !Alive(T.arm[i])) {
      T.arm_alive[i] = 0;
      SB_ShakeWorld();
      s32 ax, ay;
      ArmPos(i, &ax, &ay);
      SB_SpawnExplosion(BH, "ExplosionXL", ax, ay);
      g_autopilot_target = T.arm_alive[1 - i] ? T.arm[1 - i] : T.torso;
      if (!T.said_arm) { T.said_arm = 1; Say(); }    // Akane: "Take one arm..."
    }
    if (T.arm_regrow[i] > 0 && --T.arm_regrow[i] == 0 && T.arm_alive[i]) Anim(T.arm[i], A_IDLE);
  }
  PunchHits();
  if (!T.arm_alive[0] && !T.arm_alive[1]) {
    // "Reactor at two hundred percent!"
    Say();
    T.phase = T_OVERDRIVE;
    T.combo = 0;
    Stop(T.torso);
    R32(T.boss, B_MAXHP) = UNION_HP;                 // the bar refills: the torso's
    T.od_t = 60;
    T.cool[0] = 50;
    return;
  }
  if (T.combo > 0 && --T.combo == 0) Stop(T.torso);
  if (InCutscene()) return;
  for (int i = 0; i < 2; i++) {
    if (!T.arm_alive[i] || Busy(T.arm[i]) || T.arm_regrow[i] > 0) continue;   // not while its fist is away
    if (T.cool[i] > 0) { T.cool[i]--; continue; }
    ArmAttack(i);
  }
  // the torso joins in between
  if (!Busy(T.torso) && T.combo == 0) {
    if (--T.od_t <= 0) {
      TorsoAttack();
      T.od_t = (T.arm_alive[0] && T.arm_alive[1]) ? 260 : 170;
    }
  }
}

static void Overdrive(void) {
  UnionMove();
  PunchHits();
  Anim(T.torso, 4);                                  // the cracked armour
  if (Hp(T.torso) <= T.torso_floor + 1) {
    SB_SetBadGuyHealth(T.torso, T.torso_floor + 1);
    Stop(T.torso);
    T.phase = T_DYING;
    T.dying_t = 0;
    SB_ShakeWorld();
    // Kaito: "Heh... you really are something, hero." / Akane: "Not one
    // word to Dr. Marrow."
    SayNow();
    Say();
    return;
  }
  if (T.combo > 0 && --T.combo == 0) Stop(T.torso);
  if (InCutscene() || Busy(T.torso)) return;
  if (T.cool[0] > 0) { T.cool[0]--; return; }
  // weaker, wilder: overdrive bursts between its other attacks, longer and
  // closer together as its health runs out
  const s32 left = Hp(T.torso) - T.torso_floor;
  const int hurt = left * 2 < UNION_HP;
  T.od_cmd++;
  if (T.od_cmd & 1) {
    Order(T.torso, C_OVERDRIVE);
    T.combo = hurt ? 300 : 220;
    T.cool[0] = hurt ? 16 : 30;
  } else {
    TorsoAttack();
    T.cool[0] = hurt ? 12 : 24;
  }
}

static void Dying(u32 boss) {
  T.dying_t++;
  UnionMove();
  if ((T.dying_t & 15) == 0) {
    const s32 dx = FX(SB_RangedRand(-50, 50)), dy = FX(SB_RangedRand(-50, 50));
    SB_SpawnExplosion(BH, (T.dying_t & 32) ? "ExplosionL" : "ExplosionXS", T.ux + dx, T.uy + dy);
  }
  if ((T.dying_t & 63) == 0) SB_ShakeWorld();
  // done when both have spoken
  if (T.dying_t > 240 && !InCutscene()) {
    T.phase = T_DEAD;
    R32(boss, B_STATE) = 0;
  }
}

static void TwUpdate(u32 boss) {
  T.frame++;
  SpeechPump();
  BossWards();
  if (T.phase == T_DEAD) return;
  // the pieces cannot be hurt in cutscenes, while entering or merging, when
  // beaten down, or (the torso) while it has arms
  const int scene = InCutscene() || T.phase == T_ENTER || T.phase == T_TALK || T.phase == T_MERGE ||
                    T.phase == T_DYING;
  ShipShield(scene);
  for (int i = 0; i < 2; i++)
    if (T.titan[i]) Guard(T.titan[i], scene || T.titan_down[i]);
  if (T.torso) Guard(T.torso, scene || T.phase == T_UNION);
  for (int i = 0; i < 2; i++)
    if (T.arm_alive[i]) Guard(T.arm[i], scene);

  switch (T.phase) {
  case T_ENTER:
    for (int i = 0; i < 2; i++) {
      T.y[i] += Ease(FX(HOME_Y) - T.y[i], FX(3));
      Anim(T.titan[i], A_BOOST);
    }
    if (T.y[AKANE] >= FX(HOME_Y - 2)) {
      T.phase = T_TALK;
      for (int i = 0; i < 2; i++) Anim(T.titan[i], A_IDLE);
    }
    break;
  case T_TALK:
    if (!InCutscene()) T.phase = T_DUET;
    break;
  case T_DUET:
    Duet();
    break;
  case T_MERGE:
    Merge();
    break;
  case T_UNION:
    Union();
    break;
  case T_OVERDRIVE:
    Overdrive();
    break;
  case T_DYING:
    Dying(boss);
    break;
  }
  // the reactors turning, over them while they are lit as drawn: the
  // titans idle or boosting, the union idle (not striking, firing,
  // charging, down; not the cracked armour)
  static const signed char kTitan[7] = { 0, GLOW_OFF, GLOW_OFF, GLOW_OFF, GLOW_OFF, 0, 0 };
  static const signed char kUnion[5] = { 0, GLOW_OFF, GLOW_OFF, GLOW_OFF, GLOW_OFF };
  for (int i = 0; i < 2; i++)
    if (T.titan[i]) {
      Place(T.titan[i], T.x[i], T.y[i]);
      GlowPut(T.glow[i], T.x[i], T.y[i], i == KAITO ? DRAGOON_GLOW_X : EMPRESS_GLOW_X,
              i == KAITO ? DRAGOON_GLOW_Y : EMPRESS_GLOW_Y, GlowFor(Act(T.titan[i]), kTitan, 7));
    }
  if (T.torso) {
    Place(T.torso, T.ux, T.uy);
    GlowPut(T.uglow, T.ux, T.uy, UNION_GLOW_X, UNION_GLOW_Y, GlowFor(Act(T.torso), kUnion, 5));
    for (int i = 0; i < 2; i++) {
      if (!T.arm_alive[i]) continue;
      s32 ax, ay;
      ArmPos(i, &ax, &ay);
      Place(T.arm[i], ax, ay);
    }
  }
}

static void TwDeath(u32 boss) {
  (void)boss;
  GlowDrop(&T.uglow);
  for (int i = 0; i < 2; i++) GlowDrop(&T.glow[i]);
  ShipShield(0);
  SB_ShakeWorld();
  for (int i = 0; i < 2; i++) if (T.arm_alive[i]) Remove(T.arm[i]);
  SB_SetBadGuyHealth(T.torso, 1);
  Remove(T.torso);
  SB_SpawnExplosion(BH, "ExplosionXL", T.ux, T.uy);
  SB_SpawnExplosion(BH, "ExplosionXL", T.ux - FX(40), T.uy + FX(20));
  SB_SpawnExplosion(BH, "ExplosionXL", T.ux + FX(40), T.uy - FX(10));
  g_autopilot_target = 0;
  T.torso = 0;
  SB_PausePartition(BH, 0);
}

void TwinsBossInit(u32 boss) {
  SB_ClearBytes((void*)boss, BOSS_SIZE);
  SB_ClearBytes(&T, sizeof T);
  R32(boss, B_SPAWN + 0x00) = (s32)(u32)TwSpawn;
  R32(boss, B_SPAWN + 0x04) = (s32)(u32)TwUpdate;
  R32(boss, B_SPAWN + 0x08) = (s32)(u32)TwRender;
  R32(boss, B_SPAWN + 0x0c) = (s32)(u32)TwRender;
  R32(boss, B_SPAWN + 0x10) = (s32)(u32)TwDeath;
  R32(boss, B_HEALTH) = (s32)(u32)TwHealth;
}
