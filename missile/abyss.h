// abyss.h -- what Mission 11's two files share (abyss.c, abyss_boss.c): the
// engine calls of seoul_boss.h, the level's generated header
// (abyss/make_abyss.py: lines and moments), and the world's own services --
// lines by name, booms, the pull of a current.

#pragma once

#include "seoul_boss.h"
#include "abyss_lines.h"
#include "lab_lines.h"

#define ABYSS         11          // SHOGUN+0x4e6c (Mission 10 is the Sunda Trench: sunda.c)
#define LAB           12          // Mission 12, Dr. Marrow's lab (lab.c): the same rules down there
#define SH_WORLD_NO   0x4e6c
#define UP            0xc00000    // 256-to-the-turn in 16.16: up the screen

// R21 marks (abyss/abyss_cast.py)
#define MARK_LASER    61          // armour only the laser burns
#define MARK_SNIPER   62          // armour only the coilgun's slug pierces
#define MARK_BUOY     70
#define MARK_TORP     90          // a torpedo; 91 once the Spin Drive has turned it round
#define MARK_TORP_BACK 91

// abyss.c's red outlines that Mission 12's bosses wear (lab_boss.c LabPartOutline)
enum { OUTLINE_BELL = 6, OUTLINE_MANTLE, OUTLINE_CLUB, OUTLINE_SIPHON };

// where a hit comes from (missile.c asks AbyssGate)
enum { SRC_OTHER, SRC_LASER, SRC_SNIPER, SRC_PARRY };

// ---- abyss.c ------------------------------------------------------------------------------
int AbyssOn(void);                // world 11, playing
int AbLine(int k);                // say speech k (by name), stepping over any not reached: its ticket
int AbLineUp(int ticket);         // that line is on screen (or has been)
void AbLineNow(int k);
int AbQuiet(void);                // nobody has spoken for a while
s32 AbTime(void);                 // the partition's clock
void AbBoom(s32 x, s32 y, int big);     // an underwater blast (and the engine's explosion, for its sound)
void AbPull(s32 x, s32 y, s32 rate);    // drag the ship toward (x, y), `rate` units a frame (16.16)
void AbDraw(s32 x, s32 y, s32 rate);    // a breath drawing it in: AbPull, harder up close and in gulps
int AbParrying(void);             // the Spin Drive's window is open
int AbParries(void);              // successful parries so far this run (any shot turned)
void AbSound(int which);          // ABS_*
enum { ABS_PARRY, ABS_CHARGE, ABS_ABSORB };   // the parry, the charge beep, the Entropy Engine's absorb
// ... and what Mission 12 needs of it (lab.c)
void AbUseLines(const unsigned char* sentences, int n);
int AbLineSaid(int k);            // speech k has been said (or stepped over)
void AbZoneNow(int z);            // show floor zone z
int AbZone(void);
void AbFloorFade(s32 k);          // the floor's brightness by hand, 0..0x10000 (-1: the dive's)
void AbWaterTo(int r, int g, int b, s32 k);   // the water toward this colour, k of the way
void AbSignalRing(s32 x, s32 y, s32 r);   // Marrow's transmission round (x, y), radius r (0: none)
void AbStream(s32 v);             // the snow streaming down past the ship (rising), units a frame

// ---- abyss_boss.c --------------------------------------------------------------------------
void HunterBossInit(u32 boss);    // the Kurozame, and a new run of the level
void LeviathanBossInit(u32 boss);
void AbyssBossForget(void);

static inline s32 AbAbs(s32 v) { return v < 0 ? -v : v; }
static inline s32 AbDist(s32 dx, s32 dy) {     // whole units, about
  dx = AbAbs(dx) >> 16;
  dy = AbAbs(dy) >> 16;
  return dx > dy ? dx + (dy >> 1) : dy + (dx >> 1);
}
static inline s32 AbAngle(s32 x0, s32 y0, s32 x1, s32 y1) {
  return SB_GetAngle(x0 >> 16, y0 >> 16, x1 >> 16, y1 >> 16) & (TURN - 1);
}
static inline s32 AbTurnTo(s32 cur, s32 want, s32 rate) {
  s32 d = (want - cur) & (TURN - 1);
  if (d >= TURN / 2) d -= TURN;
  if (d > rate) d = rate;
  if (d < -rate) d = -rate;
  return (cur + d) & (TURN - 1);
}
static inline u32 AbSpawn(const char* name, s32 x, s32 y) {
  const u32 e = SB_SpawnGeneric(BH, name, x, y);
  const u32 a = e ? SB_GetGenericActor(e) : 0;
  if (a) {
    SB_SetActorPosition(a, x, y);
    SB_SetActorRegister(a, REG_CMD, 0);
  }
  return a;
}
static inline void AbDrop(u32* a) {
  if (*a) SB_SetActorRegister(*a, REG_CMD, FX(1));
  *a = 0;
}
static inline void AbPut(u32 a, s32 x, s32 y) { if (a) SB_SetActorPosition(a, x, y); }
static inline void AbFrame(u32 a, int n) { if (a) SB_SetActorRegister(a, REG_ANIM, FX(n)); }
static inline void Boom(const char* size, s32 x, s32 y) { SB_SpawnExplosion(BH, size, x, y); }
static inline s32 Rnd(s32 a, s32 b) { return SB_RangedRand(a, b); }
static inline void OrderAt(u32 bg, int cmd, s32 aim) {
  SetReg(bg, REG_AIM, aim & (TURN - 1));
  SetReg(bg, REG_BUSY, FX(1));
  SetReg(bg, REG_CMD, FX(cmd));
}
