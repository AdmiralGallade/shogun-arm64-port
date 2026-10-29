// andaman.h -- what Mission 9's three files share (andaman.c, battery.c,
// ironreef.c): the engine calls of seoul_boss.h, the level's generated
// header (andaman/make_andaman.py: lines, riders, layout), and small helpers
// for pieces placed by the code, shells in flight and UNITY's ships.

#pragma once

#include "seoul_boss.h"
#include "andaman_lines.h"

#define ANDAMAN       9           // SHOGUN+0x4e6c
#define SUNDA         10          // Mission 10 borrows this machinery (sunda.c)
#define HOMEWARD      13          // ... and Mission 13 (homeward.c): the Valhalla, shells, blasts, ships
#define AMATERASU     14          // ... and Mission 14 (amaterasu.c): the Valhalla
#define PARIS         15          // ... and Mission 15 (paris.c): the Valhalla, shells, blasts
#define ATLANTIC      16          // ... and Mission 16 (atlantic.c): the Valhalla, shells, blasts
#define SH_WORLD_NO   0x4e6c
#define UP            0xc00000    // 256-to-the-turn in 16.16: up the screen
#define LEFT          0x800000
#define MISSILE_MARK  88          // R21 of the garrison's missiles (anda_cast.py)

// ---- andaman.c ---------------------------------------------------------------------------
int AnLine(int k);                // say speech k (by name), stepping over any not reached: its ticket
int AnLineUp(int ticket);         // that line is on screen (or has been)
void AnDropQueued(void);          // lines still waiting are forgotten (not the one on screen)
void AnLineNow(int k);            // ... now, whatever is queued going by first
void AnUseLines(const unsigned char* sentences, int n);   // another world's speeches (Mission 10)
int AnQuiet(void);                // nobody has spoken for a while (or the dialog is stuck)
void AnFlyby(void);               // the Valhalla passes over
void AnFlybyAt(s32 tx, s32 ty);   // ... her railgun on (tx, ty)
// the Valhalla at a checkpoint (andaman.c): up onto her station, orders, away
void AnValCome(void);
int AnValThere(void);
void AnValStation(s32 y, int charging);   // move her station; charging: every gun loosed
int AnValGone(void);
void AnValSink(void);             // she drops back down the screen and out of sight (Mission 13)
void AnValLeave(void);
int AnValBusy(int k);             // turret k still has its orders
void AnValOrder(int k, s32 x, s32 y, int size, int tag, int every);
void AnValRailOn(u32 bg, int line);   // the railgun on a badguy, fired once `line` is up
extern u32 g_an_spare;            // a badguy the shells leave alone (her railgun's, Mission 13: 0 for none)
void AnValRailAt(s32 x, s32 y, int line);
int AnValRailDone(void);
void AnValPrespawn(void);         // her pieces made now, out of sight: she draws over what came before (Mission 10)
void AnValComeSide(s32 x0, s32 x1, s32 y);   // in from the side to x1, at height y (Mission 10)
int AnValRailState(void);
void AnValMoveTo(s32 x, s32 y);   // on station, ease to (x, y) (Mission 10)
void AnValPos(s32* x, s32* y);         // RAIL_*: where her railgun is in its shot
void AnValRailMove(s32 x, s32 y); // ... its target point moved (it follows)
void AnValEscortPos(int s, s32* x, s32* y);   // her escort s (0 left, 1 right) (Mission 16)
int AnValEscortUp(int s);
void AnValEscortLose(int s);      // ... shot down
void AnValVanish(void);           // all of her gone at once (Mission 16: her wreck takes over)
int AnValTakeGuns(u32* a, s32* dx, s32* dy, s32* ang);   // ... her turrets and railgun handed to it first
enum { RAIL_WAIT, RAIL_TURN, RAIL_CHARGING, RAIL_FIRING, RAIL_DONE };   // (andaman.c's VR_*)
s32 AnTime(void);                 // the partition's clock (slows in bullet time)
s32 AnScroll(void);               // the engine's props scroll (16.16 rows)
s32 AnRowY(s32 row);
void AnScrollBy(s32 d);
s32 AnGroundDx(s32 y);
void AnBlast(s32 x, s32 y);             // a fireball drawn over the big sprites
void Blast(const char* size, s32 x, s32 y);   // the engine's explosion (its sound) and a blast            // keep a thing at playfield y on the sea as the camera pans              // playfield y (16.16) of a props row, as things stand

// Shells in flight: a tracer from (x0, y0) to (x1, y1) at `speed` units a
// frame; where it lands an explosion (SHOT_SMALL .. SHOT_HUGE), and if `bg`
// is still alive it takes `dmg`. `tag` counts the landings (AnLanded).
enum { SHOT_SMALL, SHOT_MEDIUM, SHOT_BIG, SHOT_HUGE };
enum { TAG_NONE, TAG_DOME, TAG_GEN, TAG_FORT, TAG_IKA, TAG_MISSILE, NTAGS };
int AnShoot(s32 x0, s32 y0, s32 x1, s32 y1, int speed, int size, int tag, u32 bg, int dmg);
int AnLanded(int tag);            // landings of `tag` since last asked
void AnShotsClear(void);

// UNITY's ships: a hull, its wake, its turrets, placed from its heading
enum { SHIP_CRUISER, SHIP_FRIGATE };
typedef struct {
  u32 hull, wake, tur[2];
  int kind, ntur;
  s32 x, y, head, speed;          // 16.16; head 2^24 to the turn
  s32 tur_ang[2];
  int tur_cool[2], tur_flash[2];
} AnShip;
void ShipSpawn(AnShip* s, int kind, s32 x, s32 y, s32 head);
void ShipPlace(AnShip* s);
void ShipSail(AnShip* s);         // on along its heading
int ShipFire(AnShip* s, int k, s32 tx, s32 ty, int every, int size, int tag, u32 bg, int dmg);
void ShipTurret(AnShip* s, int k, s32* x, s32* y);
void ShipDrop(AnShip* s);

// missiles: the nearest of the garrison's within `range` of (x, y)
u32 AnNearestMissile(s32 x, s32 y, s32 range, s32* ox, s32* oy);

// ---- battery.c / ironreef.c ------------------------------------------------------------
void BatteryRide(int k, s32 emp_y, s32 gen_y);   // its island on screen (k = 0, 1)
void BatteryRideOut(int k, s32 emp_y, s32 gen_y);  // ... and gone below it
void BatteryForget(void);
void BatteryBossInit(u32 boss, int k);
void ReefRide(s32 fort_y, s32 bastion_y, s32 escort_y);
void ReefForget(void);
void ReefBossInit(u32 boss);

// ---- helpers -----------------------------------------------------------------------------
static inline u32 AnSpawn(const char* name, s32 x, s32 y) {
  const u32 e = SB_SpawnGeneric(BH, name, x, y);
  const u32 a = e ? SB_GetGenericActor(e) : 0;
  if (a) {
    SB_SetActorPosition(a, x, y);
    SB_SetActorRegister(a, REG_CMD, 0);
  }
  return a;
}
static inline void AnDrop(u32* a) {
  if (*a) SB_SetActorRegister(*a, REG_CMD, FX(1));
  *a = 0;
}
static inline void AnFrame(u32 a, int n) { if (a) SB_SetActorRegister(a, REG_ANIM, FX(n)); }
static inline void AnPut(u32 a, s32 x, s32 y) { if (a) SB_SetActorPosition(a, x, y); }
static inline void AnTurn(u32 a, s32 ang) { if (a) SB_SetActorOrient(a, ang & (TURN - 1)); }
static inline s32 AnAbs(s32 v) { return v < 0 ? -v : v; }
static inline s32 AnAngle(s32 x0, s32 y0, s32 x1, s32 y1) {
  return SB_GetAngle(x0 >> 16, y0 >> 16, x1 >> 16, y1 >> 16) & (TURN - 1);
}
// About the distance in whole units (the larger leg and half the smaller).
static inline s32 AnDist(s32 dx, s32 dy) {
  dx = AnAbs(dx) >> 16;
  dy = AnAbs(dy) >> 16;
  return dx > dy ? dx + (dy >> 1) : dy + (dx >> 1);
}
// Turn `cur` towards `want` by at most `rate` (2^24 to the turn).
static inline s32 AnTurnTo(s32 cur, s32 want, s32 rate) {
  s32 d = (want - cur) & (TURN - 1);
  if (d >= TURN / 2) d -= TURN;
  if (d > rate) d = rate;
  if (d < -rate) d = -rate;
  return (cur + d) & (TURN - 1);
}
static inline s32 AnOff(s32 a, s32 b) {         // how far apart two headings are
  s32 d = (a - b) & (TURN - 1);
  if (d >= TURN / 2) d -= TURN;
  return AnAbs(d);
}
// (dx, dy) whole units, turned by `a`: 16.16.
static inline void AnRot(s32 dx, s32 dy, s32 a, s32* ox, s32* oy) {
  const s32 c = SB_Cos(a), s = SB_Sin(a);
  *ox = dx * c - dy * s;
  *oy = dx * s + dy * c;
}
// A gun told an attack, along its own aim.
static inline void OrderAt(u32 bg, int cmd, s32 aim) {
  SetReg(bg, REG_AIM, aim & (TURN - 1));
  SetReg(bg, REG_BUSY, FX(1));
  SetReg(bg, REG_CMD, FX(cmd));
}
static inline void Boom(const char* size, s32 x, s32 y) { SB_SpawnExplosion(BH, size, x, y); }
static inline s32 Rnd(s32 a, s32 b) { return SB_RangedRand(a, b); }
// d / k for small k (1..16), without the division the blob cannot link:
// a multiply by the reciprocal (d up to a few thousand)
static inline s32 DivSmall(s32 d, int k) {
  static const unsigned short kRecip[17] = { 0, 65535, 32768, 21846, 16384, 13108, 10923, 9363, 8192, 7282,
                                             6554, 5958, 5462, 5042, 4682, 4370, 4096 };
  if (d <= 0) return 0;
  return (s32)(((u32)d * kRecip[k < 1 ? 1 : (k > 16 ? 16 : k)]) >> 16);
}

// a flak shell flies `per_step` units a script step from 30 out: the steps
// to the ship
static inline s32 FlakSteps(s32 x, s32 y, int per_step) {
  const s32 n = DivSmall(AnDist(PlayerX() - x, PlayerY() - y) - 30, per_step) + 1;
  return n > 60 ? 60 : n;
}

// the attacks (anda_cast.py CMDS)
enum { A_FLAK = 1, A_QUAD, A_BOX, A_SWEEP, A_PEPPER, A_RING, A_ARC, A_COIL, A_SCATTER, A_SALVO, A_POP,
       A_NEEDLES, A_GUNS, A_THUNDER, A_WALL, A_SPIRAL, A_RAIN, A_FAN,
       // (2026-10-11) the twin guns' shears, the Thunder cannon's wheel of
       // spokes, the citadel's star
       A_SCISSOR, A_WHEEL, A_STAR,
       // (2026-10-11, the originals' curves) Tokyo's boss's vortex of
       // turning rings, its rosace and its wings' spokes; the desert city's
       // curls and hooks
       A_VORTEX, A_ROSACE, A_CURL, A_HOOK, A_SPOKES,
       // hard mode's (beach/hardkit.py boss_attacks, docs/HARD_MODE.md)
       A_XSNIPE, A_XPINCER, A_XTWIST, A_XSNAKE, A_XGAP };
