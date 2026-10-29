// magpie.c -- Seoul's mid-boss: the Black Magpie.
//
// A stealth fighter -- Princess Akane's own, flown at the player from her
// palace -- and nothing more than a plane: one body (seoul/boss/magpie,
// scripted in seoul/cast.py), placed every frame from here, the script
// firing when told to.
//
//   enter    it streaks in from the top, afterburners lit, and settles;
//   hover    it holds the top of the screen, swaying, and works through
//            its guns: twin vulcan streams, fans of disks, missiles out of
//            its bays;
//   strafe   every few attacks it banks across the screen, carpet-bombing
//            the lane below it with falling shards, and stops on the far
//            side;
//   hurt     below two thirds it adds flares (slow rings, good to graze);
//            below a third it stops hovering and makes afterburner runs,
//            strafing from side to side with barely a pause;
//   death    the final boss can now come (twins.c).
//
// The scroll stops for it (seoul/timeline.py): it fights over Namsan, N
// Seoul Tower behind it.

#include "seoul_boss.h"
#include "glow.h"
#include "seoul_glow.h"

void TwinsBossInit(u32 boss);           // twins.c: the final boss

u32 g_autopilot_target;

enum { M_ENTER = 1, M_HOVER, M_STRAFE, M_DEAD };

#define HOME_Y     92
#define LEFT_X     96
#define RIGHT_X    304

static struct {
  u32 body;
  s32 x, y, tx;
  int phase, t, cool, turn, side;
  s32 maxhp;
  u32 frame;
  u32 glow;                     // its engines burning (glow.h)
} M;

// Its engines burning, over it: cruising, firing, bays open, hurt -- not
// on afterburner (its own frame: the long flames).
static void PlaceGlow(void) {
  static const signed char kMap[7] = { 0, 0, 0, GLOW_OFF, 0, GLOW_OFF, GLOW_OFF };
  GlowPut(M.glow, M.x, M.y, MAGPIE_GLOW_X, MAGPIE_GLOW_Y, M.body ? GlowFor(Act(M.body), kMap, 7) : GLOW_OFF);
}

static s32 MpHealth(u32 boss) {
  (void)boss;
  if (M.phase == M_DEAD || !M.body) return 0;
  return SB_GetBadGuyHealth(M.body);
}

static void MpRender(u32 boss, s32 x, s32 y) { (void)boss; (void)x; (void)y; }

static void MpSpawn(u32 boss) {
  SB_ClearBytes(&M, sizeof M);
  M.x = FX(200);
  M.y = FX(-90);
  M.body = SB_SpawnBadGuy(BH, "*seoul/boss/magpie", M.x, M.y);
  M.glow = GlowSpawn(BH, "*seoul/boss/magpie_glow");
  M.phase = M_ENTER;
  M.side = 1;
  R32(boss, B_STATE) = 1;
  SB_PausePartition(BH, 1);
  Place(M.body, M.x, M.y);
  M.maxhp = SB_GetBadGuyHealth(M.body);
  R32(boss, B_MAXHP) = M.maxhp;
  g_autopilot_target = M.body;
  // Akane: "So you're the hero boy who keeps wrecking Father's toys?"
  Say();
}

// What fraction of its health is left, in thirds: 2 healthy, 1 hurt, 0 desperate.
static int Thirds(void) {
  const s32 hp = SB_GetBadGuyHealth(M.body);
  if (hp * 3 > M.maxhp * 2) return 2;
  if (hp * 3 > M.maxhp) return 1;
  return 0;
}

static void StartStrafe(void) {
  M.phase = M_STRAFE;
  M.t = 0;
  M.side = -M.side;
  M.tx = FX(M.side > 0 ? RIGHT_X : LEFT_X);
  Order(M.body, C_CARPET);
}

static void Hover(void) {
  M.t++;
  // a lazy figure of eight about its station
  const s32 cx = M.tx ? M.tx : FX(200);
  const s32 wantx = cx + SB_MulFP(SB_Sin((s32)(M.t << 15)), FX(26));
  const s32 wanty = FX(HOME_Y) + SB_MulFP(SB_Sin((s32)(M.t << 16)), FX(8));
  M.x += Ease(wantx - M.x, FX(1.5));
  M.y += Ease(wanty - M.y, FX(1));
  if (InCutscene()) return;
  if (Busy(M.body)) return;
  if (M.cool > 0) { M.cool--; return; }
  const int th = Thirds();
  if (th == 0) { StartStrafe(); return; }          // desperate: run after run
  const int seq = M.turn;
  // healthy: vulcan, spread, missiles, strafe; hurt: flares join
  static const unsigned char kHealthy[] = { C_VULCAN, C_SPREAD, C_MISSILES, 0 };
  static const unsigned char kHurt[] = { C_VULCAN, C_FLARES, C_SPREAD, C_MISSILES, 0, C_VULCAN, C_FLARES, 0 };
  // hard mode (docs/HARD_MODE.md): pincers, the snake, twist rings, snipe
  // rings and drifting-gap rings woven between its guns
  static const unsigned char kHealthyH[] = { C_VULCAN, C_XPINCER, C_SPREAD, C_XSNAKE, C_MISSILES, 0 };
  static const unsigned char kHurtH[] = { C_VULCAN, C_XTWIST, C_XSNIPE, C_SPREAD, C_XGAP, C_MISSILES, 0, C_XPINCER };
  const int hard = HardMode();
  const unsigned char* list = th == 2 ? (hard ? kHealthyH : kHealthy) : (hard ? kHurtH : kHurt);
  const int n = th == 2 ? (hard ? (int)sizeof kHealthyH : (int)sizeof kHealthy)
                        : (hard ? (int)sizeof kHurtH : (int)sizeof kHurt);
  if (++M.turn >= n) M.turn = 0;                   // no modulo in the blob
  const int cmd = list[seq >= n ? 0 : seq];
  if (!cmd) { StartStrafe(); return; }
  Order(M.body, cmd);
  M.cool = th == 2 ? (hard ? 24 : 34) : (hard ? 18 : 24);
}

static void Strafe(void) {
  M.t++;
  // bank across at speed, a little lower, then pull up on the far side
  const s32 d = M.tx - M.x;
  const s32 speed = Thirds() == 0 ? FX(2.6) : FX(2.0);
  M.x += d > speed ? speed : (d < -speed ? -speed : d);
  M.y += Ease(FX(HOME_Y + 14) - M.y, FX(1));
  Anim(M.body, A_CHARGE);                          // afterburner
  if (d > FX(2) || d < -FX(2)) return;
  Stop(M.body);
  if (Thirds() == 0) {
    // desperate: a vulcan burst, then straight back the other way
    M.phase = M_HOVER;
    M.cool = 10;
    if (!Busy(M.body)) Order(M.body, HardMode() ? C_XTWIST : C_VULCAN);
    M.turn = 0;
    return;
  }
  M.phase = M_HOVER;
  M.t = 0;
  M.cool = 30;
}

static void MpUpdate(u32 boss) {
  M.frame++;
  SpeechPump();
  BossWards();
  if (M.phase != M_DEAD && !Alive(M.body)) {
    M.phase = M_DEAD;
    R32(boss, B_STATE) = 0;                       // the engine calls death next
    return;
  }
  const int scene = InCutscene() || M.phase == M_ENTER;
  ShipShield(scene);
  Guard(M.body, scene);
  switch (M.phase) {
  case M_ENTER:
    Anim(M.body, A_CHARGE);
    M.y += FX(2.2);
    if (M.y >= FX(HOME_Y)) {
      M.y = FX(HOME_Y);
      M.phase = M_HOVER;
      M.cool = 40;
      M.tx = FX(200);
      Anim(M.body, A_IDLE);
    }
    break;
  case M_HOVER:
    Hover();
    break;
  case M_STRAFE:
    Strafe();
    break;
  }
  Place(M.body, M.x, M.y);
  PlaceGlow();
}

static void MpDeath(u32 boss) {
  GlowDrop(&M.glow);
  ShipShield(0);
  SB_ShakeWorld();
  SB_SpawnExplosion(BH, "ExplosionXL", M.x, M.y);
  SB_SpawnExplosion(BH, "ExplosionL", M.x - FX(30), M.y + FX(10));
  SB_SpawnExplosion(BH, "ExplosionL", M.x + FX(30), M.y - FX(6));
  g_autopilot_target = 0;
  // Akane: "Tch... lucky shot. Kaito, he's coming to the palace!"
  Say();
  // set up the final boss and let the level run on to it
  TwinsBossInit(boss);
  SB_PausePartition(BH, 0);
}

void MagpieBossInit(u32 boss) {
  SB_ClearBytes((void*)boss, BOSS_SIZE);
  SpeechReset();                        // a new run of the level
  R32(boss, B_SPAWN + 0x00) = (s32)(u32)MpSpawn;
  R32(boss, B_SPAWN + 0x04) = (s32)(u32)MpUpdate;
  R32(boss, B_SPAWN + 0x08) = (s32)(u32)MpRender;
  R32(boss, B_SPAWN + 0x0c) = (s32)(u32)MpRender;
  R32(boss, B_SPAWN + 0x10) = (s32)(u32)MpDeath;
  R32(boss, B_HEALTH) = (s32)(u32)MpHealth;
}
