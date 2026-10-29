// seoul_boss.h -- what Seoul's two bosses (magpie.c, twins.c) share: the
// engine calls a boss built from badguys needs, and small helpers over them.
// The BOSS record, the parts' command registers and the cutscene rules are
// the Beach's (starfish.c, docs/NEW_LEVEL.md section 8); the dialog queue
// and the cutscene shield are starfish.c's own functions.

#pragma once

typedef unsigned int u32;
typedef int s32;

#define IMG 0x10000000u
#define FN(type, addr) ((type)(IMG + (addr)))

#define SB_GetGameDataPtr    FN(u32 (*)(void), 0x63128)                        // ARM
#define SB_ClearBytes        FN(void (*)(void*, u32), 0x56629)
#define SB_PausePartition    FN(void (*)(u32, u32), 0xbad4c)                   // ARM
#define SB_ShakeWorld        FN(void (*)(void), 0x34f79)
#define SB_SpawnBadGuy       FN(u32 (*)(u32, const char*, s32, s32), 0xbc124)  // ARM
#define SB_GetBadGuyActor    FN(u32 (*)(u32), 0xbac8c)                         // ARM
#define SB_SetActorPosition  FN(void (*)(u32, s32, s32), 0xc174c)              // ARM
#define SB_SetActorOrient    FN(void (*)(u32, s32), 0xc17e0)                   // ARM
#define SB_SetActorRegister  FN(void (*)(u32, u32, s32), 0xc1814)              // ARM
#define SB_GetActorRegister  FN(s32 (*)(u32, u32), 0xc17f8)                    // ARM
#define SB_GetActorPosition  FN(void (*)(u32, s32*, s32*), 0xc1730)            // ARM
#define SB_SetInvincibility  FN(void (*)(u32, s32), 0xbac5c)                   // ARM
#define SB_GetBadGuyHealth   FN(s32 (*)(u32), 0xbac64)                         // ARM
#define SB_SetBadGuyHealth   FN(void (*)(u32, s32), 0xbac70)                   // ARM
#define SB_StrikeBadGuy      FN(s32 (*)(u32, u32, s32), 0xc0700)               // ARM
#define SB_SpawnGeneric      FN(u32 (*)(u32, const char*, s32, s32), 0xbc1d8)  // ARM
#define SB_SpawnBonus        FN(u32 (*)(u32, const char*, s32, s32), 0xbc054)  // ARM: a pickup
#define SB_GetGenericActor   FN(u32 (*)(u32), 0xbaba8)                         // ARM
#define SB_SpawnExplosion    FN(void (*)(u32, const char*, s32, s32), 0xbd80c) // ARM
#define SB_GetFirstBadGuy    FN(u32 (*)(u32), 0xbb05c)                         // ARM
#define SB_GetNextBadGuy     FN(u32 (*)(u32, u32), 0xbb064)                    // ARM
#define SB_MulFP             FN(s32 (*)(s32, s32), 0x63148)                    // ARM
#define SB_Sin               FN(s32 (*)(s32), 0x49ffd)
#define SB_Cos               FN(s32 (*)(s32), 0x4a0bd)
#define SB_RangedRand        FN(s32 (*)(s32, s32), 0x49f9d)
#define SB_GetAngle          FN(s32 (*)(s32, s32, s32, s32), 0x63ef4)          // ARM
#define SB_StrikePlayer      FN(void (*)(u32, u32, u32), 0x384dd)

#define BH 0x405
#define R32(p, off)  (*(volatile s32*)((u32)(p) + (off)))

// BOSS record
#define B_STATE     0x000
#define B_SPAWN     0x26c       // then update, render, render shadow, death
#define B_MAXHP     0x288       // the boss bar: health callback / this
#define B_HEALTH    0x28c
#define BOSS_SIZE   0x294
// SHOGUN
#define SH_PLAYER   0x5110      // PLAYER: +0 x, +0x1c y (16.16 playfield units)
#define SH_PLAYER_X 0x5110
#define SH_PLAYER_Y 0x512c
#define SH_HUD      0x8b3a8
#define SH_HITLOCK  0x5182      // u16: the ship can't be hit (respawning etc.)
#define SH_HITLOCK2 0x5188

#define FX(v)       ((s32)((v) * 65536))
#define TURN        0x1000000
#define DOWN        0x400000    // 256-to-the-turn in 16.16: straight down

// the parts' script registers (seoul/cast.py)
#define REG_AIM  13
#define REG_ANIM 18
#define REG_BUSY 19
#define REG_CMD  20
enum { C_VULCAN = 1, C_MISSILES, C_SPREAD, C_FLARES, C_CARPET, C_CRESCENT, C_RAIL, C_TRAIL,
       C_SPIRAL_K, C_SPIRAL_B, C_BLOOMFAN, C_PETALS, C_BLOOM, C_ORBS, C_CROSS, C_SEAL,
       C_OVERDRIVE, C_SLASH,
       // the union's own (seoul/cast.py)
       C_YINYANG, C_WINGS, C_COIL, C_SCALES,
       // hard mode's (beach/hardkit.py boss_attacks, seoul/cast.py CMDS): docs/HARD_MODE.md
       C_XSNIPE, C_XPINCER, C_XTWIST, C_XSNAKE, C_XGAP };
enum { A_IDLE, A_STRIKE, A_FIRE, A_CHARGE, A_DOWN, A_BOOST };

// starfish.c: Dr. Marrow's dialog queue, used by every added boss
void Say(void);
void SayNow(void);
void SaySkipping(int skip);         // step over `skip` sentences first
void SayNowSkipping(int skip);
void SpeechPump(void);
void SpeechHurry(int on);
int SpeechPending(void);            // lines queued, not yet shown
u32 SpeechShown(void);              // lines put up so far
int SpeechDropQueued(void);         // the lines still waiting, forgotten: how many
int SpeechStalled(void);            // frames the dialog has gone without changing           // the box moves on every few frames (a skipped tutorial)
void SpeechReset(void);
int InCutscene(void);
void ShipShield(int on);
void GodHold(int who, int on);      // the god flag, shared (ShipShield is one holder)
// missile.c: the capsule shield
int ShieldUp(void);
// courier.c: a shield ward from the top every so often during the fight
void BossWards(void);

// What the desktop autopilot should shoot at (runtime/levelkit.py).
extern u32 g_autopilot_target;

static inline u32 Shogun(void) { return SB_GetGameDataPtr(); }
// Hard mode (docs/HARD_MODE.md): the run is on the engine's hell lap -- the
// level select's HARD toggle, or Mission 4's hell button. The lap is SHOGUN+0x4e70.
static inline int HardMode(void) { return R32(Shogun(), 0x4e70) >= 2; }
static inline u32 Act(u32 bg) { return SB_GetBadGuyActor(bg); }
static inline void SetReg(u32 bg, u32 r, s32 v) { SB_SetActorRegister(Act(bg), r, v); }
static inline s32 GetReg(u32 bg, u32 r) { return SB_GetActorRegister(Act(bg), r); }
static inline int Busy(u32 bg) { return GetReg(bg, REG_BUSY) != 0; }
static inline void Anim(u32 bg, int a) { SetReg(bg, REG_ANIM, FX(a)); }

static inline int Alive(u32 bg) {
  if (!bg) return 0;
  for (u32 b = SB_GetFirstBadGuy(BH); b; b = SB_GetNextBadGuy(BH, b))
    if (b == bg) return SB_GetBadGuyHealth(bg) > 0;
  return 0;
}

// An attack, aimed "down": every aimed attack turns on the ship itself.
static inline void Order(u32 bg, int cmd) {
  SetReg(bg, REG_AIM, DOWN);
  SetReg(bg, REG_BUSY, FX(1));
  SetReg(bg, REG_CMD, FX(cmd));
}

// End an attack that runs until stopped.
static inline void Stop(u32 bg) { SetReg(bg, REG_CMD, FX(-1)); }

static inline void Place(u32 bg, s32 x, s32 y) { SB_SetActorPosition(Act(bg), x, y); }

// Invincibility is not a flag: < 0 vulnerable, > 18 invincible.
static inline void Guard(u32 bg, int on) { SB_SetInvincibility(bg, on ? 255 : -1); }

static inline void Remove(u32 bg) {
  if (!Alive(bg)) return;
  SB_SetInvincibility(bg, -1);
  SB_StrikeBadGuy(BH, bg, 0x7fff);
}

static inline s32 PlayerX(void) { return R32(Shogun(), SH_PLAYER_X); }
static inline s32 PlayerY(void) { return R32(Shogun(), SH_PLAYER_Y); }

// A boss's moving part: a generic the code places every frame (its script
// holds until R20 is set), R18 the frame it shows. Spawned later, drawn over.
static inline u32 SpawnPart(const char* name, s32 x, s32 y) {
  const u32 e = SB_SpawnGeneric(BH, name, x, y);
  const u32 a = e ? SB_GetGenericActor(e) : 0;
  if (a) {
    SB_SetActorPosition(a, x, y);
    SB_SetActorRegister(a, REG_CMD, 0);
  }
  return a;
}
static inline void PlacePart(u32 a, s32 x, s32 y) { if (a) SB_SetActorPosition(a, x, y); }
static inline void PartFrame(u32 a, int f) { if (a) SB_SetActorRegister(a, REG_ANIM, FX(f)); }
static inline void DropPart(u32* a) {
  if (*a) SB_SetActorRegister(*a, REG_CMD, FX(1));
  *a = 0;
}

// Ease a value towards a target: a sixteenth of the way, at most `speed`.
static inline s32 Ease(s32 d, s32 speed) {
  s32 s = d >> 4;
  if (s > speed) s = speed;
  if (s < -speed) s = -speed;
  if (s == 0) s = d;
  return s;
}
