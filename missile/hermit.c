// hermit.c -- World 5's mid-boss: the Hermit Fortress.
//
// A mechanical hermit crab with a conch-shell gun turret on its back and
// two great claws. It is three badguys (beach/boss/hermit, hermit_claw_l,
// hermit_claw_r, scripted in beach/enemies.py) plus a shadow, placed every
// frame from here; the scripts fire when told to (the same command
// registers as the starfish's parts).
//
//   enter   it scuttles down into view;
//   fight   it sidles towards the player's side of the screen, its turret
//           firing spreads, its claws taking turns to snap forward and spray
//           a fan of shards;
//   hop     every so often it crouches and leaps at the player: an arc
//           through the air -- the shadow stays on the ground, and nothing
//           hits it while it is up -- then a landing that shakes the screen
//           and throws out a ring of bullets;
//   anger   each claw lost makes it hop sooner; with one gone the other
//           also fires lasers; with both gone it hops twice in a row;
//   death   the shell blows: the final boss can now come (starfish.c).
//
// Hooked in through the BOSS record exactly as starfish.c is.

typedef unsigned int u32;
typedef int s32;

#define IMG 0x10000000u
#define FN(type, addr) ((type)(IMG + (addr)))

#define HB_GetGameDataPtr    FN(u32 (*)(void), 0x63128)                        // ARM
#define HB_ClearBytes        FN(void (*)(void*, u32), 0x56629)
#define HB_PausePartition    FN(void (*)(u32, u32), 0xbad4c)                   // ARM
#define HB_ShakeWorld        FN(void (*)(void), 0x34f79)
#define HB_SpawnBadGuy       FN(u32 (*)(u32, const char*, s32, s32), 0xbc124)  // ARM
#define HB_GetBadGuyActor    FN(u32 (*)(u32), 0xbac8c)                         // ARM
#define HB_SetActorPosition  FN(void (*)(u32, s32, s32), 0xc174c)              // ARM
#define HB_SetActorRegister  FN(void (*)(u32, u32, s32), 0xc1814)              // ARM
#define HB_GetActorRegister  FN(s32 (*)(u32, u32), 0xc17f8)                    // ARM
#define HB_SetInvincibility  FN(void (*)(u32, s32), 0xbac5c)                   // ARM
#define HB_GetBadGuyHealth   FN(s32 (*)(u32), 0xbac64)                         // ARM
#define HB_StrikeBadGuy      FN(s32 (*)(u32, u32, s32), 0xc0700)               // ARM
#define HB_SpawnGeneric      FN(u32 (*)(u32, const char*, s32, s32), 0xbc1d8)  // ARM
#define HB_SpawnExplosion    FN(void (*)(u32, const char*, s32, s32), 0xbd80c) // ARM
#define HB_GetFirstBadGuy    FN(u32 (*)(u32), 0xbb05c)                         // ARM
#define HB_GetNextBadGuy     FN(u32 (*)(u32, u32), 0xbb064)                    // ARM
#define HB_MulFP             FN(s32 (*)(s32, s32), 0x63148)                    // ARM
#define HB_Sin               FN(s32 (*)(s32), 0x49ffd)

#define BH 0x405
#define R32(p, off)  (*(volatile s32*)((u32)(p) + (off)))

#define B_STATE     0x000
#define B_MAXHP     0x288      // the boss bar: health callback / this
#define B_SPAWN     0x26c
#define B_HEALTH    0x28c
#define BOSS_SIZE   0x294
#define SH_PLAYER_X 0x5110              // the ship, 16.16 playfield units
#define SH_LAP      0x4e70              // the lap: 2 is hard mode (docs/HARD_MODE.md)

#define FX(v)       ((s32)((v) * 65536))
#define DOWN        0x400000            // 256-to-the-turn, 16.16: straight down

// script registers (enemies.py) and the body's animations
#define REG_AIM  13
#define REG_ANIM 18
#define REG_BUSY 19
#define REG_CMD  20
enum { CMD_HAIL = 1, CMD_MISSILES, CMD_LASER, CMD_RING,
       // hard mode's (beach/enemies.py HARD_CMDS)
       CMD_SNIPE = 9, CMD_PINCER, CMD_TWIST, CMD_SNAKE, CMD_GAP };
enum { A_WALK, A_FIRE, A_CHARGE, A_CROUCH, A_AIR, A_LAND };

enum { H_ENTER = 1, H_FIGHT, H_CROUCH, H_AIR, H_LAND, H_DEAD };

#define HOME_Y      84
#define CLAW_DX     38                  // claw offset from the body centre
#define CLAW_DY     22
#define HOP_TIME    44                  // frames in the air
#define HOP_HEIGHT  70                  // apex above the ground

void StarfishBossInit(u32 boss);        // starfish.c: the final boss
void Say(void);                         // starfish.c: Dr. Marrow's next line
void SpeechPump(void);
int InCutscene(void);
void ShipShield(int on);
void SpeechReset(void);

static struct {
  u32 body, claw[2], shadow;
  int claw_alive[2];
  s32 x, y;                             // ground position, 16.16
  s32 hx0, hx1, hy0, hy1;               // hop start and end
  s32 lift;                             // height above ground, 16.16
  int phase, t, cool, hop_in, hops_queued, turn, snap[2];
  u32 frame;
} H;

static u32 Act(u32 bg) { return HB_GetBadGuyActor(bg); }
static void SetReg(u32 bg, u32 r, s32 v) { HB_SetActorRegister(Act(bg), r, v); }
static int Busy(u32 bg) { return HB_GetActorRegister(Act(bg), REG_BUSY) != 0; }

static int Alive(u32 bg) {
  if (!bg) return 0;
  for (u32 b = HB_GetFirstBadGuy(BH); b; b = HB_GetNextBadGuy(BH, b))
    if (b == bg) return HB_GetBadGuyHealth(bg) > 0;
  return 0;
}

static void Order(u32 bg, int cmd) {
  SetReg(bg, REG_AIM, DOWN);
  SetReg(bg, REG_BUSY, FX(1));
  SetReg(bg, REG_CMD, FX(cmd));
}

static int ClawsLeft(void) { return H.claw_alive[0] + H.claw_alive[1]; }

// Hard mode (docs/HARD_MODE.md): its turret adds the snake and the
// drifting-gap rings; its claws close pincers on the ship; every landing
// throws a ring that hangs and then snipes; it hops sooner and fires faster.
static int Hard(void) { return R32(HB_GetGameDataPtr(), SH_LAP) >= 2; }
static const unsigned char kHardTurret[4] = { CMD_HAIL, CMD_SNAKE, CMD_MISSILES, CMD_GAP };

static void PlaceAll(void) {
  const s32 y = H.y - H.lift;
  HB_SetActorPosition(Act(H.shadow), H.x, H.y + FX(22));
  HB_SetActorPosition(Act(H.body), H.x, y);
  for (int i = 0; i < 2; i++) {
    if (!H.claw_alive[i]) continue;
    const s32 side = i ? FX(CLAW_DX) : -FX(CLAW_DX);
    // claws sway as it walks, and lunge forward when they snap
    const s32 sway = HB_MulFP(HB_Sin((s32)((H.frame + i * 40) << 17)), FX(3));
    HB_SetActorPosition(Act(H.claw[i]), H.x + side, y + FX(CLAW_DY) + sway + FX(H.snap[i]));
  }
}

// ---- callbacks ------------------------------------------------------------------

static void HbRender(u32 boss, s32 x, s32 y) { (void)boss; (void)x; (void)y; }

static s32 HbHealth(u32 boss) {
  (void)boss;
  if (H.phase == H_DEAD) return 0;
  s32 hp = HB_GetBadGuyHealth(H.body);
  for (int i = 0; i < 2; i++)
    if (H.claw_alive[i]) hp += HB_GetBadGuyHealth(H.claw[i]);
  return hp;
}

static void HbSpawn(u32 boss) {
  HB_ClearBytes(&H, sizeof H);
  H.x = FX(200);
  H.y = FX(-80);
  // the shadow first, so it is drawn under the rest
  H.shadow = HB_SpawnBadGuy(BH, "*beach/boss/hermit_shadow", H.x, H.y);
  H.body = HB_SpawnBadGuy(BH, "*beach/boss/hermit", H.x, H.y);
  H.claw[0] = HB_SpawnBadGuy(BH, "*beach/boss/hermit_claw_l", H.x, H.y);
  H.claw[1] = HB_SpawnBadGuy(BH, "*beach/boss/hermit_claw_r", H.x, H.y);
  H.claw_alive[0] = H.claw[0] != 0;
  H.claw_alive[1] = H.claw[1] != 0;
  H.phase = H_ENTER;
  H.hop_in = 300;
  R32(boss, B_STATE) = 1;
  HB_PausePartition(BH, 1);
  PlaceAll();
  R32(boss, B_MAXHP) = HbHealth(boss);       // a full boss bar
  // Dr. Marrow introduces his pet
  Say();
}

static void StartHop(void) {
  H.phase = H_CROUCH;
  H.t = 0;
  SetReg(H.body, REG_ANIM, FX(A_CROUCH));
}

static void Fight(void) {
  // claws lost since last frame
  for (int i = 0; i < 2; i++)
    if (H.claw_alive[i] && !Alive(H.claw[i])) {
      H.claw_alive[i] = 0;
      HB_ShakeWorld();
      H.hop_in = 40;                         // it lunges straight away
    }
  // sidle towards the ship's side, slowly
  const s32 px = R32(HB_GetGameDataPtr(), SH_PLAYER_X);
  s32 dx = (px - H.x) >> 6;
  if (dx > FX(1)) dx = FX(1);
  if (dx < -FX(1)) dx = -FX(1);
  H.x += dx;
  if (InCutscene()) return;                  // let him finish talking
  if (--H.hop_in <= 0 && !Busy(H.body)) {
    H.hops_queued = ClawsLeft() == 0 ? 2 : 1;
    StartHop();
    return;
  }
  if (H.cool > 0) { H.cool--; return; }
  // take turns: turret, a claw, turret, the other claw
  H.turn++;
  const int hard = Hard();
  if (H.turn & 1) {
    if (!Busy(H.body)) {
      Order(H.body, hard ? kHardTurret[(H.turn >> 1) & 3] : ((H.turn & 2) ? CMD_MISSILES : CMD_HAIL));
      H.cool = hard ? 26 : 34;
    }
  } else {
    const int i = (H.turn & 2) ? 1 : 0;
    const int c = H.claw_alive[i] ? i : 1 - i;
    if (H.claw_alive[c] && !Busy(H.claw[c])) {
      int cmd = (ClawsLeft() == 1 && (H.turn & 4)) ? CMD_LASER : CMD_HAIL;
      if (hard && cmd == CMD_HAIL && (H.turn & 4)) cmd = CMD_PINCER;
      Order(H.claw[c], cmd);
      H.snap[c] = 12;
      H.cool = hard ? 24 : 30;
    }
  }
}

static void HbUpdate(u32 boss) {
  H.frame++;
  SpeechPump();
  if (H.phase != H_DEAD && !Alive(H.body)) {
    H.phase = H_DEAD;
    R32(boss, B_STATE) = 0;                 // the engine calls death next
    return;
  }
  for (int i = 0; i < 2; i++)
    if (H.snap[i] > 0) H.snap[i]--;
  // while Dr. Marrow talks (and while it enters) no one can be hurt
  const int scene = InCutscene() || H.phase == H_ENTER;
  ShipShield(scene);
  const int airborne = H.phase == H_AIR || scene;
  HB_SetInvincibility(H.body, airborne ? 255 : -1);
  for (int i = 0; i < 2; i++)
    if (H.claw_alive[i]) HB_SetInvincibility(H.claw[i], airborne ? 255 : -1);

  switch (H.phase) {
  case H_ENTER:
    H.y += FX(1.25);
    if (H.y >= FX(HOME_Y)) { H.y = FX(HOME_Y); H.phase = H_FIGHT; H.cool = 30; }
    break;
  case H_FIGHT:
    Fight();
    break;
  case H_CROUCH:
    if (++H.t >= 14) {
      // leap at the ship, but stay in the upper part of the screen
      const u32 sh = HB_GetGameDataPtr();
      s32 tx = R32(sh, SH_PLAYER_X);
      if (tx < FX(70)) tx = FX(70);
      if (tx > FX(330)) tx = FX(330);
      H.hx0 = H.x; H.hx1 = tx;
      H.hy0 = H.y;
      H.hy1 = (H.y == FX(HOME_Y)) ? FX(HOME_Y + 46) : FX(HOME_Y);
      H.phase = H_AIR; H.t = 0;
      SetReg(H.body, REG_ANIM, FX(A_AIR));
    }
    break;
  case H_AIR: {
    H.t++;
    // straight-line travel, and a parabola for the height: 4h t (T - t) / T^2
    // 0..1 in 16.16; 1489 = 65536 / HOP_TIME (the blob has no divide)
    const s32 f = H.t >= HOP_TIME ? FX(1) : H.t * 1489;
    H.x = H.hx0 + HB_MulFP(H.hx1 - H.hx0, f);
    H.y = H.hy0 + HB_MulFP(H.hy1 - H.hy0, f);
    H.lift = HB_MulFP(HB_MulFP(FX(4 * HOP_HEIGHT), f), FX(1) - f);
    if (H.t >= HOP_TIME) {
      H.lift = 0;
      H.phase = H_LAND; H.t = 0;
      SetReg(H.body, REG_ANIM, FX(A_LAND));
      HB_ShakeWorld();
      HB_SpawnGeneric(BH, "*beach/scripts/splash", H.x - FX(30), H.y + FX(18));
      HB_SpawnGeneric(BH, "*beach/scripts/splash", H.x + FX(30), H.y + FX(18));
      Order(H.body, Hard() ? CMD_SNIPE : CMD_RING);
    }
    break;
  }
  case H_LAND:
    if (++H.t >= 10) {
      SetReg(H.body, REG_ANIM, FX(A_WALK));
      if (--H.hops_queued > 0) { StartHop(); break; }
      H.phase = H_FIGHT;
      H.cool = 24;
      H.hop_in = ClawsLeft() == 2 ? 300 : (ClawsLeft() == 1 ? 210 : 150);
      if (Hard()) H.hop_in = ClawsLeft() == 2 ? 220 : (ClawsLeft() == 1 ? 150 : 100);
    }
    break;
  }
  PlaceAll();
}

static void Remove(u32 bg) {
  if (!Alive(bg)) return;
  HB_SetInvincibility(bg, -1);
  HB_StrikeBadGuy(BH, bg, 0x7fff);
}

static void HbDeath(u32 boss) {
  ShipShield(0);
  // its claws go with it, and the shadow
  for (int i = 0; i < 2; i++) if (H.claw_alive[i]) Remove(H.claw[i]);
  Remove(H.shadow);
  HB_ShakeWorld();
  HB_SpawnExplosion(BH, "ExplosionXL", H.x, H.y);
  // "My specimen! ...No matter."
  Say();
  // what Ocean's mid-boss does on its way out: set up the final boss and
  // let the level run on to it
  StarfishBossInit(boss);
  HB_PausePartition(BH, 0);
}

void HermitBossInit(u32 boss) {
  HB_ClearBytes((void*)boss, BOSS_SIZE);
  SpeechReset();                        // a new run of the level
  R32(boss, B_SPAWN + 0x00) = (s32)(u32)HbSpawn;
  R32(boss, B_SPAWN + 0x04) = (s32)(u32)HbUpdate;
  R32(boss, B_SPAWN + 0x08) = (s32)(u32)HbRender;
  R32(boss, B_SPAWN + 0x0c) = (s32)(u32)HbRender;
  R32(boss, B_SPAWN + 0x10) = (s32)(u32)HbDeath;
  R32(boss, B_HEALTH) = (s32)(u32)HbHealth;
}
