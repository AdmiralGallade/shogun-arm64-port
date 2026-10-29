// courier.c -- the pickups in Seoul, the chase and Edinburgh: shoot a courier down and
// it drops a capsule; during the boss fights, and all through the chase, a
// capsule comes down every so often.
//
// The capsule is the original game's own (common/items/powerup, the blue
// one that wanders the screen and is drawn to the ship; caught, it is a
// capsule more, up to 7, and points): the engine loads it, spawns it
// (BH_SpawnBonus) and catches it (its bonus callback, PowerUpHandler)
// itself. In the original game it comes out of a few enemies' explosions.
// (These were shield wards, hexagons that refilled the shield, once.)
//
// The courier is an ordinary badguy (seoul/cast.py, courier.badguy) that
// marks itself with R21 = 77. Nothing in the engine drops pickups for a new
// enemy, so this watches the couriers every frame: one whose health has run
// out while it is on the playfield was shot down (one that flew off the far
// side just leaves), and in its place a capsule appears. Called once a frame
// while playing, from the satellites' update (missile.c), in Seoul and in
// the chase (world 7, chase.c) and Edinburgh (world 8), which have no
// couriers.
//
// The bosses get capsules too: while either of Seoul's or Edinburgh's bosses
// fights, or the chase runs, BossWards (called from their updates, magpie.c,
// twins.c, subagent.c, shogungpt.c and chase.c) lets one come down from the top of the screen every
// BOSS_WARD_EVERY frames, somewhere across it.
//
// Same rules as missile.c: no libc, no division, engine reached by address.

#include "seoul_boss.h"

#define SH_WORLD     0x4e6c
#define SEOUL        6
#define CHASE        7           // Mission 7 (chase.c): capsules, no couriers
#define EDINBURGH    8           // Mission 8: capsules in its boss fights
#define ANDAMAN      9           // Mission 9: supply boats (couriers), and its fights' capsules
#define SUNDA        10          // Mission 10: its fight's capsules
#define ABYSS        11          // Mission 11: salvage pods (couriers), and its fights' capsules
#define LAB          12          // Mission 12: the same, at Dr. Marrow's lab
#define HOMEWARD     13          // Mission 13: its fights' capsules
#define AMATERASU    14          // Mission 14: its fights' capsules
#define PARIS        15          // Mission 15: its fights' capsules
#define ATLANTIC     16          // Mission 16: its fight's capsules

#define COURIER_MARK 77          // R21 of a courier
#define REG_MARK     21

#define NCOURIERS    4
#define BOSS_WARD_EVERY 720      // frames between a boss fight's wards (12 s)
#define BOSS_WARD_FIRST 420      // ... and before the first

static struct {
  u32 bg[NCOURIERS];             // couriers being watched
  s32 x[NCOURIERS], y[NCOURIERS];
  u32 frame;
  int boss_ward;                 // frames to the next boss-fight ward
  u32 boss_seen;                 // C.frame when BossWards last ran
} C;

static int Mark(u32 actor) { return SB_GetActorRegister(actor, REG_MARK) >> 16; }

static int OnField(s32 x, s32 y) {
  return x > FX(-16) && x < FX(416) && y > FX(-16) && y < FX(416);
}

static void DropWard(s32 x, s32 y) {
  SB_SpawnBonus(BH, "common/items/powerup", x, y);
}

// Couriers: take on new ones; one gone from the list, or out of health, on
// the playfield, was shot down.
static void WatchCouriers(void) {
  for (int i = 0; i < NCOURIERS; i++) {
    const u32 bg = C.bg[i];
    if (!bg) continue;
    int listed = 0;
    for (u32 b = SB_GetFirstBadGuy(BH); b; b = SB_GetNextBadGuy(BH, b))
      if (b == bg) { listed = 1; break; }
    const int dead = !listed || SB_GetBadGuyHealth(bg) <= 0;
    if (listed && !dead) SB_GetActorPosition(Act(bg), &C.x[i], &C.y[i]);
    if (!dead) continue;
    if (OnField(C.x[i], C.y[i])) DropWard(C.x[i], C.y[i]);
    C.bg[i] = 0;
  }
  for (u32 b = SB_GetFirstBadGuy(BH); b; b = SB_GetNextBadGuy(BH, b)) {
    if (Mark(Act(b)) != COURIER_MARK || SB_GetBadGuyHealth(b) <= 0) continue;
    int known = 0, free_ = -1;
    for (int i = 0; i < NCOURIERS; i++) {
      if (C.bg[i] == b) known = 1;
      if (!C.bg[i] && free_ < 0) free_ = i;
    }
    if (known || free_ < 0) continue;
    C.bg[free_] = b;
    SB_GetActorPosition(Act(b), &C.x[free_], &C.y[free_]);
  }
}

// Once a frame from a Seoul boss's update. A fight that starts afresh (no
// call for a while) waits BOSS_WARD_FIRST before its first capsule.
void BossWards(void) {
  if (C.frame - C.boss_seen > 2) C.boss_ward = BOSS_WARD_FIRST;
  C.boss_seen = C.frame;
  if (--C.boss_ward > 0) return;
  C.boss_ward = BOSS_WARD_EVERY;
  DropWard(FX(SB_RangedRand(110, 290)), FX(40));
}

void CourierTick(void) {
  const u32 sh = Shogun();
  const s32 world = R32(sh, SH_WORLD);
  if (world != SEOUL && world != CHASE && world != EDINBURGH && world != ANDAMAN && world != SUNDA && world != ABYSS
      && world != LAB && world != HOMEWARD && world != AMATERASU && world != PARIS && world != ATLANTIC) {
    if (C.frame) SB_ClearBytes(&C, sizeof C);
    return;
  }
  C.frame++;
  WatchCouriers();
}
