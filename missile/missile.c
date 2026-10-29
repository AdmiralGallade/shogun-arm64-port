// missile.c -- the M (missile) and E (extinguishing coilgun) weapons, as
// guest code.
//
// The game has three weapons: S (spread, type 1), H (homing, 2), L (laser, 3).
// This adds two, both built on the engine's own shot system -- the same one
// Spread uses -- so damage, hit detection, impacts and drawing are all the
// engine's, unchanged.
//   M (type 4): missiles that launch slowly, accelerate, and turn gently toward
//       the nearest enemy.
//   E (type 5): the extinguishing coilgun. One slug at a time, very fast,
//       straight ahead -- the player aims it -- and it hits very hard. It is
//       the red bolt the first level's mid-boss fires, turned round.
//
// And two upgrades the Doc fits at UNITY's regional HQ in Scotland (Mission 8):
//   P  the plasma sphere: a short-range weapon, a ball of plasma held round
//      the ship that burns whatever comes close -- missiles, rammers. To the
//      engine it *is* E (type 5); g_plasma says which. See "P" below.
//   the shield: the Valkyrie's generator. Bought on the weapon wheel for one
//      capsule; for ten seconds no bullet touches the ship. Alone it is no
//      help against missiles or rammers (that is P's job): with P held as
//      well, nothing touches the ship -- but beams (the Kurogane's Tenshu
//      cannon and its bow lance, which strike from code) get through even
//      then. Grazing recharges the ship far slower while it is up. See "the
//      shield" below.
//
// Both come only once Mission 8's homecoming has taught them (edinburgh.c
// EdUpgradesReady): until then the wheel neither shows nor takes them.
//
// It is compiled for ARMv5TE Thumb, the instruction set the game itself uses,
// and linked at a fixed guest address (see missile.ld). Both runtimes -- the
// desktop harness and the arm64 app -- map the same blob there and redirect a
// handful of `bl` instructions into it (see build.py for the list). Nothing
// here runs on the host: it is ordinary game code that happens to be new.
//
// Rules for this file, because nothing links it but itself:
//   * no libc, no division or modulo, no 64-bit maths, no switch tables --
//     each would pull in a runtime helper the blob does not have;
//   * the engine is reached only through the absolute addresses below. The
//     image base is fixed at 0x10000000 in both runtimes, so they are stable.
//     Thumb functions carry bit 0; ARM ones do not, and are reached with blx.

typedef unsigned int u32;
typedef int s32;
typedef unsigned short u16;

#define IMG 0x10000000u
#define FN(type, addr) ((type)(IMG + (addr)))

// ---- engine entry points (Thumb = odd) -------------------------------------
#define UE_GetGameDataPtr   FN(u32 (*)(void), 0x63128)             // ARM
#define UE_GetHandlePtr     FN(u32* (*)(u32, u32), 0x4d6bd)
#define FAdd                FN(u32 (*)(u32, u32), 0x22a80)    // ARM; floats as bits
#define UE_PlaySound        FN(s32 (*)(u32, u32, u32, u32, u32), 0x5e485)
#define UE_LoadSprites      FN(s32 (*)(u32, const char*, u32, u32), 0x57ea9)
#define UE_LoadSound        FN(s32 (*)(u32, const char*), 0x5bf35)
#define UE_PopBlendMode     FN(void (*)(void), 0x4ee69)
#define UE_Sin              FN(s32 (*)(s32), 0x49ffd)
#define UE_Cos              FN(s32 (*)(s32), 0x4a0bd)
#define UE_GetAngle         FN(s32 (*)(s32, s32, s32, s32), 0x63ef4)   // ARM
#define UE_GetAngleDelta    FN(s32 (*)(s32, s32), 0x63f10)             // ARM
#define UE_MulFP            FN(s32 (*)(s32, s32), 0x63148)             // ARM
#define UE_DeleteBagEntry   FN(void (*)(u32, u32*), 0x78154)           // ARM
#define UE_GetFirstBagEntry FN(u32* (*)(u32), 0x77e7c)                 // ARM
#define UE_GetNextBagEntry  FN(u32* (*)(u32, u32*), 0x77f10)           // ARM
#define BH_GetFirstBadGuy   FN(u32 (*)(u32), 0xbb05c)                  // ARM
#define BH_GetNextBadGuy    FN(u32 (*)(u32, u32), 0xbb064)             // ARM
#define BH_GetBadGuyPosition FN(void (*)(u32, s32*, s32*), 0xbbbb4)    // ARM
#define BH_isBadGuyInvincible FN(s32 (*)(u32), 0xbb40c)                // ARM
#define BH_GetBadGuyHealth  FN(s32 (*)(u32), 0xbac64)                  // ARM
#define MS_GetBadGuyActor   FN(u32 (*)(u32), 0xbac8c)                  // ARM
#define MS_SetActorPosition FN(void (*)(u32, s32, s32), 0xc174c)       // ARM

#define UpdateSpread        FN(void (*)(u32), 0x429bd)
#define UpdateSatellites    FN(void (*)(u32), 0x2c829)
#define RenderSatellites    FN(void (*)(u32, s32, s32, s32), 0x2ef0d)
#define RenderPlayer        FN(void (*)(u32, s32, s32, s32), 0x3bc6d)
#define RenderPlayerShadow  FN(void (*)(u32, s32, s32), 0x37325)
#define PlayerOutro         FN(void (*)(u32, u32), 0x37efd)
#define PlayerPreOutro      FN(void (*)(u32, u32, u32), 0x37925)
#define LoadSatellites      FN(void (*)(u32), 0x2d1d5)
#define UpdateShotWeapon    FN(s32 (*)(u32, u32, void*, void*), 0x2db79)
#define WeaponBulletCancel  FN(void (*)(u32), 0x383d5)
#define StrikeBadguy        FN(void (*)(u32, s32), 0x3a3d1)
#define AddShotImpact       FN(void (*)(u32, u32, s32, s32), 0x2d781)
#define GetEntryCoord       FN(void (*)(s32, s32*, s32*), 0x2b14d)
#define isNarrowScreen      FN(s32 (*)(void), 0x2b0dd)
#define TestWeaponSelection FN(s32 (*)(u32, u32, s32, s32), 0x337b1)
#define SelectBTWeaponMenu  FN(void (*)(u32, u32, u32), 0x37ae5)
#define DecideBTWeaponMenu  FN(void (*)(u32, u32, u32), 0x28bbd)
#define DeactivateBTWeaponMenu FN(void (*)(u32), 0x37aa5)
#define SetActionWeaponSet  FN(void (*)(u32, u32), 0x28b85)
#define EnableWeapon        FN(void (*)(u32), 0x28b45)
#define RenderBTWeaponMenuSprite FN(void (*)(u32, u32, s32, s32, s32), 0x3800d)

// ---- what the teardown established -----------------------------------------
#define BH        0x405       // bullet-hell module handle
#define BAG_SHOTS 0x5c5       // live player shots: {model, def, x, y}

// PLAYER (r0 of every Update*/Render* weapon function)
#define P_X       0x00        // 16.16
#define P_Y       0x1c        // 16.16
#define P_WEAPON  0xb0        // the embedded WEAPON
#define P_TYPE    0xbc        // == WEAPON+0x0c: 1 S, 2 H, 3 L; 4 M, 5 E
#define P_LEVEL   0xc0        // == WEAPON+0x10: 1..4, 5 = overload
#define P_CAPSULES 0x74       // capsules: each a full shield's worth
// WEAPON
#define W_TYPE    0x0c
#define W_LEVEL   0x10
#define W_CHARGE  0x18        // cleared by WeaponSet unless overloaded
#define W_ACTION  0x2c        // pending action: 1-3 set weapon, 4 power-up,
                              // 5 bullet cancel; 6 set M, 7 set E (ours)
// WEAPONMENU (the radial "bullet time" menu, SHOGUN+0x8b46c)
#define WM_DECIDED  0x3c      // sprite pulsed after a choice
#define WM_DECISION 0x44
#define WM_TUTOLOCK 0x50      // u16: non-zero while a tutorial restricts it
#define WM_CANCEL   0x54
#define SH_TUTOLOCK 0x8b4bc   // the same u16, from SHOGUN
#define SH_MENU_Y   0x8b49c   // menu centre, fixed while it is open
#define SH_MENU_SCALE 0x8b474 // 16.16: pixels per unit of GetEntryCoord

#define TYPE_M      4
#define ACTION_M    6         // 4 is taken: it means "power up"
#define ENTRY_M     5         // radial slot 0..4 are S H L + EMP; 5 is new
#define SELECT_M    (ENTRY_M + 1)   // TestWeaponSelection returns slot+1
#define TYPE_E      5
#define ACTION_E    7
#define ENTRY_E     6         // the hexagon is full; E is in the outer ring
#define SELECT_E    (ENTRY_E + 1)
#define ACTION_P    8         // P is E to the engine, with g_plasma set
#define ENTRY_P     7         // P and the shield: the second ring, either
#define SELECT_P    (ENTRY_P + 1)   // side of E (see hook_entry_coord)
#define ENTRY_SH    8
#define SELECT_SH   (ENTRY_SH + 1)

// Sprite handles. The engine's handle table holds 0x4000 and the game never
// goes above 0x2113; these sit well clear.
#define SPR_BUTTON  0x3e00    // the M button, drawn in the hexagon's free
                              // bottom slot (its frame is flipped to suit)
#define SPR_MISSILE 0x3e10    // 16 headings, 22.5 degrees apart
#define SPR_SUPER   0x3e20    // 6 animation frames, always nose-up
#define SPR_E_UP    0x3e01    // E button, edges down: above S
#define SPR_E_DOWN  0x3e02    // E button, edges up: below M, near the top
#define SPR_SLUG    0x3e30    // the mid-boss's red bolt, flipped to fly up,
                              // loaded once per size: 0x3e30 .. 0x3e36
#define SPR_P_UP    0x3e03    // P and shield buttons, edges down (above the
#define SPR_P_DOWN  0x3e04    // ship) and edges up (below it, near the top)
#define SPR_SH_UP   0x3e05
#define SPR_SH_DOWN 0x3e06
#define SPR_BUBBLE  0x3e60    // the shield's bubble: 4 shimmering, 1 faint
#define SPR_PLASMA  0x3e68    // the sphere, 4 frames, at each level's size:
                              // 0x3e68 .. 0x3e7b
#define SPR_IMPACT  0x509     // Spread's own impacts, already loaded
#define SPR_IMPACT_BIG 0x510

#define SND_LAUNCH  0x814     // Homing's launch
#define SND_SELECT  0x80c     // "weapon changed", as WeaponSet plays it
// The mid-boss's own shot, for the bolt it fires: one sharp report decaying
// to nothing over a second. (SPREAD_2 was tried first and is a loop segment of
// Spread's rapid fire -- evenly loud start to end -- so every volley played
// a second of rattle.) The game loads this only for the first world, under a
// handle each world reuses, so it gets a handle of its own.
#define SND_COIL    0x3e40
#define SND_COIL_FILE "common/sounds/MIDBOSS1_SHOOT.wav"
#define SPRITE_TAG  0x49525053  // "SPRI": UE_GetHandlePtr's type check

#define R32(p, off)  (*(volatile s32*)((u32)(p) + (off)))
#define R16(p, off)  (*(volatile u16*)((u32)(p) + (off)))

// ---- abyss.c: Mission 11's rules (no seekers down there, armour, the Spin Drive) -------
int AbyssNoSeekers(void);                   // H and M are out
int AbyssGate(u32 badguy, int src);         // 0: armour turned the hit
int AbyssShotSource(u32 model);             // a parried shot's model, or not
u32 AbyssMenuSprite(u32 slot, u32 sprite);  // the drowned cell
void AbyssTick(void);
#define SRC_OTHER  0                         // (abyss.h)
#define SRC_SNIPER 2

// ---- angles -----------------------------------------------------------------
// The engine's: 2^24 to the turn, 0 = +x, increasing clockwise on screen.
#define DEG(d)   ((s32)((d) * 46603))             // 2^24 / 360, rounded
#define UP       0xc00000

// ---- the shot system's own records ------------------------------------------
typedef struct {            // one way a model can fire, 0x24 bytes
  s32 sprite;               // drawn centred; 0 draws a debug circle
  s32 mval;                 // fires when (frame & model.mask) == mval
  s32 vx, vy;               // added every frame by UpdateShots
  s32 ox, oy;               // spawn offset from the ship
  s32 impact, ix, iy;       // impact animation and its offset
} DEF;

typedef struct {            // WEAPONSHOTMODEL, 0x14 bytes
  s32 damage, radius, mask, count;
  DEF* defs;
} MODEL;

// A missile in flight. Its shot entry points at `m`, so the engine reads its
// damage, radius and current heading frame from here; everything after `m` is
// ours. Being a copy per missile is what lets each one steer independently
// while UpdateShots still does the hit test.
typedef struct {
  MODEL m;
  s32 x, y;                 // 16.16
  s32 speed, angle, age;
  s32 kind, anim;
  s32 state;                // LAUNCH, AIM, COMMITTED, STRAIGHT (see Fly)
  u32 target;               // the enemy it locked on to
  s32 aim_age;              // frames spent in the current AIM
  s32 fixes;                // course corrections still allowed
  u32* entry;               // the shot that points at us; 0 = free
  s32 seen;
} MISSILE;

// Missile steering states and limits; Steer1 explains them.
enum { MS_LAUNCH, MS_AIM, MS_COMMITTED, MS_STRAIGHT };
#define MAX_AIM       30
#define LOCK_CONE     DEG(60)   // half-width of the lock-on cone
#define DRIFT_PX      48
#define MISSILE_FIXES 1

// Flight profiles. Speeds and acceleration are 16.16 pixels per frame.
typedef struct {
  s32 v0, accel, vmax;
  s32 turn;                 // max heading change per frame; 0 = no homing
  s32 delay;                // frames of straight flight before it seeks
  s32 damage, radius;
  s32 super;                // big, nose-up, animated
} KIND;

enum { K_SMALL, K_FAST, K_SUPER, K_SAT, K_COUNT };
static const KIND kKinds[K_COUNT] = {
  // One volley per 16 frames (see the spawn tables), so each missile carries
  // roughly twice the damage it would at Spread's rate: fewer, heavier hits.
  // Speeds were halved after play-testing: v0, accel and vmax are all half
  // their first values.
  //  v0        accel    vmax       turn     delay dmg rad super
  { 0x18000,  0x3999,  0x58000,   DEG(3),    6,   40,  7, 0 },  // L1, L2
  { 2 << 16,  0x4ccc,  7 << 16,   DEG(4.5),  4,   56,  8, 0 },  // L3, L4
  { 1 << 16,  0x6666,  8 << 16,   0,         0,  120, 14, 1 },  // overload
  { 0x18000,  0x3999,  6 << 16,   DEG(3.5),  6,   24,  6, 0 },  // satellites
};

// Heading frames: frame f of the sheet points 180 + 22.5*f degrees clockwise
// from straight up (measured from the sprite sheet: frame 8 is nose-up). Each
// is a DEF the engine can draw, with no velocity -- missiles are moved here.
// mval is unreachable so Shoot never spawns from these.
#define NEVER 0x7fffffff
#define HEAD(f) { SPR_MISSILE + (f), NEVER, 0, 0, 0, 0, SPR_IMPACT, -9, -55 }
#define SUPR(f) { SPR_SUPER + (f),   NEVER, 0, 0, 0, 0, SPR_IMPACT, -9, -55 }
static DEF kHeadings[16] = {
  HEAD(0), HEAD(1), HEAD(2),  HEAD(3),  HEAD(4),  HEAD(5),  HEAD(6),  HEAD(7),
  HEAD(8), HEAD(9), HEAD(10), HEAD(11), HEAD(12), HEAD(13), HEAD(14), HEAD(15),
};
static DEF kSuperFrames[6] = {
  SUPR(0), SUPR(1), SUPR(2), SUPR(3), SUPR(4), SUPR(5),
};

// Spawn defs. Shoot() turns each matching one into a shot entry; the entry is
// adopted into a MISSILE before UpdateShots ever moves it, so a spawn def's
// velocity is never used -- vx carries the launch angle offset in degrees and
// vy the flight kind instead.
#define SPAWN(mval, ox, oy, deg, kind) \
  { SPR_MISSILE + 8, (mval), (deg), (kind), (ox), (oy), SPR_IMPACT, -9, -55 }

// Main ship, one model per power level. The mask gives a 16-frame cycle and
// each level fires one volley in it.
// Upgrades alternate: L2 adds a missile, L3 hits harder and steers better,
// L4 adds a third, overload swaps in super missiles.
static DEF kL1[] = {
  SPAWN(0,   0, -14,   0, K_SMALL),
};
static DEF kL2[] = {
  SPAWN(0,  -8, -10, -12, K_SMALL), SPAWN(0,   8, -10,  12, K_SMALL),
};
static DEF kL3[] = {
  SPAWN(0,  -8, -10, -12, K_FAST),  SPAWN(0,   8, -10,  12, K_FAST),
};
static DEF kL4[] = {
  SPAWN(0, -10,  -8, -20, K_FAST),  SPAWN(0,   0, -14,   0, K_FAST),
  SPAWN(0,  10,  -8,  20, K_FAST),
};
static DEF kOverload[] = {
  SPAWN(0, -12, -12,   0, K_SUPER), SPAWN(0,  12, -12,   0, K_SUPER),
  SPAWN(8, -14,  -6, -35, K_FAST),  SPAWN(8,  14,  -6,  35, K_FAST),
};
// Satellites: one seeker each per cycle, offset from the ship's volley.
static DEF kSat[] = {
  SPAWN(8,   0, -10,   0, K_SAT),
};
static DEF kSatOverload[] = {
  SPAWN(8,   0, -10,   0, K_SAT),
};

#define COUNT(a) (sizeof(a) / sizeof((a)[0]))
#define MDL(defs) { 0, 0, 0xf, COUNT(defs), defs }
static MODEL kModels[5] = {
  MDL(kL1), MDL(kL2), MDL(kL3), MDL(kL4), MDL(kOverload),
};
// UpdateShotWeapon takes WEAPONSHOTMODEL** -- pointers, indexed by level - 1.
static MODEL* kModelPtrs[5] = {
  &kModels[0], &kModels[1], &kModels[2], &kModels[3], &kModels[4],
};
static MODEL kSatModel = MDL(kSat);
static MODEL kSatOverloadModel = MDL(kSatOverload);
// UpdateShotWeapon indexes this as [satellite][level - 2]. GetSatelliteCount
// gives 0, 2, 4, 4, 6 for levels 1..5, so six rows; level 1 reads none.
#define SATS { &kSatModel, &kSatModel, &kSatModel, &kSatOverloadModel }
static MODEL* kSatModels[6][4] = { SATS, SATS, SATS, SATS, SATS, SATS };

// ---- E: the extinguishing coilgun --------------------------------------------
// A slug straight ahead of the ship, at 34 pixels a frame: across the screen
// in about half a second, and still short of the smallest enemy's hit
// diameter, so it cannot step over one. It passes through what it hits,
// striking each enemy on its line once, and only one volley is ever in flight.
// Upgrades make the slug bigger and harder-hitting, and from L2 add two
// smaller slugs alongside it that hit for 3/8 as much (def 0 is always the
// main slug; c_strike scales the others).
//
// Every size is the same bolt loaded at its own scale (hook_load).
enum { SLUG_100, SLUG_125, SLUG_150, SLUG_180,     // main
       SIDE_50, SIDE_65, SIDE_80, SLUG_SIZES };    // side
static const u32 kSlugScale[SLUG_SIZES] = {
  0x10000, 0x14000, 0x18000, 0x1cccc, 0x8000, 0xa666, 0xcccc,
};
#define SLUG(size, ox, oy) \
  { SPR_SLUG + (size), 0, 0, -34, (ox), (oy), SPR_IMPACT_BIG, -9, -55 }
static DEF kSlug1[] = { SLUG(SLUG_100, 0, -40) };
static DEF kSlug2[] = {
  SLUG(SLUG_100, 0, -40), SLUG(SIDE_50, -16, -24), SLUG(SIDE_50, 16, -24),
};
static DEF kSlug3[] = {
  SLUG(SLUG_125, 0, -48), SLUG(SIDE_50, -18, -24), SLUG(SIDE_50, 18, -24),
};
static DEF kSlug4[] = {             // L4 widens to five
  SLUG(SLUG_150, 0, -58), SLUG(SIDE_65, -22, -28), SLUG(SIDE_65, 22, -28),
  SLUG(SIDE_50, -40, -16), SLUG(SIDE_50, 40, -16),
};
static DEF kSlugOL[] = {
  SLUG(SLUG_180, 0, -68), SLUG(SIDE_80, -26, -32), SLUG(SIDE_80, 26, -32),
};
static MODEL kCoil[5] = {
  // damage radius mask count defs   -- mask 0: fires whenever allowed.
  // Never exactly 100: StrikeBadguy lets 100 through invincibility.
  { 240,  8, 0, 1, kSlug1 },
  { 330,  9, 0, 3, kSlug2 },
  { 435, 10, 0, 3, kSlug3 },
  { 555, 11, 0, 5, kSlug4 },
  { 720, 14, 0, 3, kSlugOL },           // overload
};
static MODEL* kCoilPtrs[5] = {
  &kCoil[0], &kCoil[1], &kCoil[2], &kCoil[3], &kCoil[4],
};
// Satellites hold fire: a model with nothing to shoot.
static MODEL kNoShot = { 0, 0, 0, 0, kSlug1 };
#define NOSHOT { &kNoShot, &kNoShot, &kNoShot, &kNoShot }
static MODEL* kCoilSats[6][4] = {
  NOSHOT, NOSHOT, NOSHOT, NOSHOT, NOSHOT, NOSHOT,
};
#define COIL_COOLDOWN 45        // frames from one slug to the next: 0.75 s
#define COIL_COOLDOWN_OL 30     // overload: 0.5 s
static int g_coil_alive, g_coil_cool;

static int IsCoil(u32 m) {
  return m >= (u32)&kCoil[0] && m < (u32)&kCoil[5];
}

// Pass-through. UpdateShots would strike an enemy on every frame the slug
// overlaps it, and then delete the slug; instead each enemy is struck once per
// slug, and only shots that are not slugs are deleted. One volley is in flight
// at a time, so one list serves, keyed by enemy and slug (the def index); it
// is cleared whenever a volley is fired.
#define MAX_PIERCED 48
static u32 g_pierced[MAX_PIERCED];
static u32 g_pierced_by[MAX_PIERCED];
static int g_npierced;

static int AlreadyPierced(u32 badguy, u32 slug) {
  for (int i = 0; i < g_npierced; i++)
    if (g_pierced[i] == badguy && g_pierced_by[i] == slug) return 1;
  if (g_npierced < MAX_PIERCED) {
    g_pierced[g_npierced] = badguy;
    g_pierced_by[g_npierced] = slug;
    g_npierced++;
  }
  return 0;
}

static int IsSpawnModel(u32 m) {
  if (m == (u32)&kSatModel || m == (u32)&kSatOverloadModel) return 1;
  for (int i = 0; i < 5; i++)
    if (m == (u32)&kModels[i]) return 1;
  return 0;
}

// ---- the shield ---------------------------------------------------------------
// The Valkyrie's generator, salvaged from her wreck. Picked on the weapon
// wheel, it burns one capsule and holds the ship's god flag up (through
// starfish.c's GodHold, which a cutscene shares) for SHIELD_TIME frames;
// another capsule adds as long again. While it is up the engine skips every
// bullet's and every ram's strike -- but beams that strike from code (the
// Tenshu cannon, chase.c MegaHits, and the Kurogane's bow lance, LanceHits)
// call StrikePlayer themselves, so the shield never stops them.
//
// Nor does it stop what comes to ram -- missiles, kamikazes, anything that
// flies into the ship: the plasma sphere is for those. While the shield is
// up (and nobody is talking) the code strikes the ship itself for whatever
// touches it, as the engine would, and the rammer takes the knock too, so a
// missile still bursts on the hull (ShieldRams) -- unless P is held too: the
// two together are the only true invulnerability there is. And the entropy engine, which
// refills the ship's own shield as bullets graze it, works at an eighth of
// its rate while the bubble is up (ShieldEntropy).
#define SHIELD_TIME 600         // ten seconds a purchase ...
#define SHIELD_COST 1           // ... of one capsule
#define SHIELD_FADE 120         // the bubble flickers for the last two
#define GOD_SHIELD  2           // our bit in GodHold (starfish.c)
#define RAM_REACH   16          // a badguy this close (units, each way) has rammed
#define RAM_DMG     400         // ... and takes this (a missile bursts)
#define ENTROPY_SHIFT 3         // the graze refill, divided by 2^this
#define P_ARMOUR    0x8c        // PLAYER: the ship's own shield, 0xaaaa full
#define MS_StrikePlayer     FN(void (*)(u32, u32, u32), 0x384dd)
#define SH_PLAYER   0x5110      // the PLAYER inside SHOGUN
#define SH_HUD      0x8b3a8
#define SH_HITLOCK  0x5182      // u16: the ship can't be hit (respawning etc.)
#define SH_HITLOCK2 0x5188
void GodHold(int who, int on);  // starfish.c
int InCutscene(void);           // starfish.c: someone is talking
static int g_shield_left;
static s32 g_armour_prev, g_armour_gain;   // the ship's shield last frame; refill owed

int ShieldUp(void) { return g_shield_left > 0; }

static int ShieldBuy(u32 player) {
  if (R32(player, P_CAPSULES) < SHIELD_COST) return 0;
  R32(player, P_CAPSULES) = R32(player, P_CAPSULES) - SHIELD_COST;
  g_shield_left += SHIELD_TIME;
  GodHold(GOD_SHIELD, 1);
  UE_PlaySound(SND_SELECT, 0, 0, 0, 0);
  return 1;
}

static void ShieldRams(u32 player);   // below, beside the targets it reads
int PlasmaUp(void);                   // below: P is held

// Grazing refills the ship's shield; with the bubble up, only an eighth of
// each gain is kept (the rest owed in g_armour_gain, so slow trickles count).
static void ShieldEntropy(u32 player) {
  const s32 now = R32(player, P_ARMOUR);
  if (g_shield_left > 0 && now > g_armour_prev) {
    g_armour_gain += now - g_armour_prev;
    const s32 keep = g_armour_gain >> ENTROPY_SHIFT;
    g_armour_gain -= keep << ENTROPY_SHIFT;
    R32(player, P_ARMOUR) = g_armour_prev + keep;
  } else if (g_shield_left <= 0) {
    g_armour_gain = 0;
  }
  g_armour_prev = R32(player, P_ARMOUR);
}

static void ShieldTick(u32 player) {
  if (g_shield_left > 0) g_shield_left--;
  GodHold(GOD_SHIELD, g_shield_left > 0);
  ShieldEntropy(player);
  if (g_shield_left > 0 && !InCutscene() && !PlasmaUp()) ShieldRams(player);
}

// ---- P: the plasma sphere --------------------------------------------------------
// A ball of plasma round the ship, for what gets close: a missile swarm, a
// ship coming to ram. It reaches only a little way, so it is no weapon for
// hunting; it burns whatever is inside it every PLASMA_EVERY frames. The
// power levels grow it, and teach it more:
//   L1  ember     the sphere
//   L2  corona    wider and hotter
//   L3  deflector it holds small fry (missiles, rammers) at its edge, where
//                 they burn, instead of letting them through to the ship
//   L4  arc       lightning jumps to the nearest enemy within ARC_RANGE
//   OL  nova      widest and hottest; the lightning forks to three
// The engine has five weapon types and tables sized for them, so P is not a
// sixth: it is E's type with g_plasma set (selecting anything else clears
// it), and E's satellites, which hold their fire, serve it too.
typedef struct { short r, dmg, push, arc_every, arcs, arc_dmg; } PLASMA;
static const PLASMA kPlasma[5] = {
  //  r  dmg  push every arcs arc_dmg   -- dmg never 100 (see c_strike)
  { 30,  22,  0,    0,   0,    0 },
  { 38,  28,  0,    0,   0,    0 },
  { 44,  32,  1,    0,   0,    0 },
  { 48,  36,  1,   40,   1,  150 },
  { 56,  44,  1,   24,   3,  180 },
};
// The sphere's art is drawn 28 units in radius; each level loads it to size.
static const u32 kSphereScale[5] = { 0x11249, 0x15b6e, 0x19249, 0x1b6db, 0x20000 };
#define PLASMA_EVERY 4          // frames between burns
#define PUSH_MAX_HP  600        // the deflector holds what has no more than this
#define BOSS_HP      2500       // more health than this is a boss's: it burns at
#define BOSS_SHIFT   3          // ... an eighth of the rate (with the shield up, a
                                // ship parked on a boss took it apart in seconds)
#define ARC_RANGE    130
#define ARC_STEP     14         // sparks along a bolt
static int g_plasma;            // E's slot holds P
static int g_plasma_t, g_arc_cool;

int PlasmaUp(void) { return g_plasma; }     // edinburgh.c: the Doc's lesson

// What the sphere does to `bg`: a boss's parts burn slowly.
static s32 PlasmaDamage(u32 bg, s32 dmg) {
  if (BH_GetBadGuyHealth(bg) <= BOSS_HP) return dmg;
  const s32 d = dmg >> BOSS_SHIFT;
  return d < 1 ? 1 : d;
}

static int PlasmaLevel(u32 player) {
  const s32 l = R32(player, P_LEVEL);
  return l < 1 ? 1 : (l > 5 ? 5 : l);
}

// ---- auras: what is drawn round the ship ---------------------------------------
// The bubble and the sphere are drawn the way the missiles are: as entries
// in the engine's own shot bag, which it draws. One of each is asked for when
// wanted (UpdateShotWeapon, firing a spawn model of ours), adopted in Steer,
// and kept on the ship every frame; one no longer wanted is sent off the top
// of the screen, where the engine deletes it. They strike nothing (c_strike
// and c_impact pass them over): the sphere's burning is done in PlasmaTick.
enum { AURA_BUBBLE, AURA_SPHERE, AURAS };
#define AURA_DEF(spr) { (spr), NEVER, 0, 0, 0, 0, SPR_IMPACT, -9, -55 }
static DEF kBubbleFrames[5] = {
  AURA_DEF(SPR_BUBBLE), AURA_DEF(SPR_BUBBLE + 1), AURA_DEF(SPR_BUBBLE + 2),
  AURA_DEF(SPR_BUBBLE + 3), AURA_DEF(SPR_BUBBLE + 4),
};
#define SPH(l) { AURA_DEF(SPR_PLASMA + 4 * (l)), AURA_DEF(SPR_PLASMA + 4 * (l) + 1), \
                 AURA_DEF(SPR_PLASMA + 4 * (l) + 2), AURA_DEF(SPR_PLASMA + 4 * (l) + 3) }
static DEF kSphereFrames[5][4] = { SPH(0), SPH(1), SPH(2), SPH(3), SPH(4) };
static MODEL kBubble = { 0, 0, 0xf, 5, kBubbleFrames };
static MODEL kSphere[5] = {
  { 0, 0, 0xf, 4, kSphereFrames[0] }, { 0, 0, 0xf, 4, kSphereFrames[1] },
  { 0, 0, 0xf, 4, kSphereFrames[2] }, { 0, 0, 0xf, 4, kSphereFrames[3] },
  { 0, 0, 0xf, 4, kSphereFrames[4] },
};
// spawn models: one def each, fired on any call (mask 0), from the ship
static DEF kBubbleSpawnDef[1] = { { SPR_BUBBLE, 0, 0, 0, 0, 0, SPR_IMPACT, -9, -55 } };
static DEF kSphereSpawnDef[1] = { { SPR_PLASMA, 0, 0, 0, 0, 0, SPR_IMPACT, -9, -55 } };
static MODEL kBubbleSpawn = { 0, 0, 0, 1, kBubbleSpawnDef };
static MODEL kSphereSpawn = { 0, 0, 0, 1, kSphereSpawnDef };
#define FIVE(m) { &(m), &(m), &(m), &(m), &(m) }
static MODEL* kBubbleSpawnPtrs[5] = FIVE(kBubbleSpawn);
static MODEL* kSphereSpawnPtrs[5] = FIVE(kSphereSpawn);

static struct { u32* entry; int seen; } g_aura[AURAS];

// Which aura a shot model belongs to (its own or its spawn model), or -1.
static int AuraKind(u32 m) {
  if (m == (u32)&kBubble || m == (u32)&kBubbleSpawn) return AURA_BUBBLE;
  if (m == (u32)&kSphereSpawn) return AURA_SPHERE;
  if (m >= (u32)&kSphere[0] && m < (u32)&kSphere[5]) return AURA_SPHERE;
  return -1;
}

#define RETIRED(e) ((s32)(e)[3] < -500)

// In Steer's walk of the shot bag: the first live entry of each aura is
// the one kept; any other is a spare and goes.
static void SeeAura(u32* e, int k) {
  if (RETIRED(e)) return;
  if (g_aura[k].seen) { e[3] = (u32)-1000; return; }
  g_aura[k].entry = e;
  g_aura[k].seen = 1;
  if (k == AURA_BUBBLE) e[0] = (u32)&kBubble;
  else if (e[0] == (u32)&kSphereSpawn) e[0] = (u32)&kSphere[0];
}

// Lend E's type for the call, the way E itself fires (the satellites hold
// fire: kCoilSats).
static void AuraSpawn(u32 player, int k) {
  const s32 t = R32(player, P_TYPE);
  R32(player, P_TYPE) = TYPE_E;
  UpdateShotWeapon(player, TYPE_E, k == AURA_BUBBLE ? kBubbleSpawnPtrs : kSphereSpawnPtrs, kCoilSats);
  R32(player, P_TYPE) = t;
}

static void Aura(u32 player, int k, int want, MODEL* model, int frame) {
  u32* e = g_aura[k].entry;
  if (!want) {
    if (e) e[3] = (u32)-1000;
    g_aura[k].entry = 0;
    return;
  }
  if (!e) { AuraSpawn(player, k); return; }
  e[0] = (u32)model;
  e[1] = (u32)frame;
  e[2] = (u32)(R32(player, P_X) >> 16);
  e[3] = (u32)(R32(player, P_Y) >> 16);
}

// ---- missiles in flight ------------------------------------------------------
// The shot bag holds 0x200; a full screen of missiles is well under this.
#define MAX_MISSILES 160
static MISSILE g_pool[MAX_MISSILES];

static int IsOurs(u32 m) {
  return m >= (u32)&g_pool[0] && m < (u32)&g_pool[MAX_MISSILES];
}

// Targets, gathered once per update rather than once per missile.
#define MAX_TARGETS 48
static s32 g_tx[MAX_TARGETS], g_ty[MAX_TARGETS];
static u32 g_tbg[MAX_TARGETS];
static int g_targets;

static void GatherTargets(void) {
  g_targets = 0;
  for (u32 bg = BH_GetFirstBadGuy(BH); bg && g_targets < MAX_TARGETS;
       bg = BH_GetNextBadGuy(BH, bg)) {
    if (BH_isBadGuyInvincible(bg) || BH_GetBadGuyHealth(bg) <= 0) continue;
    s32 x, y;
    BH_GetBadGuyPosition(bg, &x, &y);
    x >>= 16; y >>= 16;
    if (y < -16) continue;                // not on screen yet
    g_tx[g_targets] = x;
    g_ty[g_targets] = y;
    g_tbg[g_targets] = bg;
    g_targets++;
  }
}

// The god flag lets rams by too: strike for them here, one a frame (the
// strike's own recovery time keeps it from repeating).
static void ShieldRams(u32 player) {
  const u32 sh = UE_GetGameDataPtr();
  if (*(volatile unsigned short*)(sh + SH_HITLOCK) || R32(sh, SH_HITLOCK2)) return;
  const s32 sx = R32(player, P_X) >> 16, sy = R32(player, P_Y) >> 16;
  for (int i = 0; i < g_targets; i++) {
    const s32 dx = g_tx[i] - sx, dy = g_ty[i] - sy;
    if (dx > RAM_REACH || dx < -RAM_REACH || dy > RAM_REACH || dy < -RAM_REACH) continue;
    if (BH_GetBadGuyHealth(g_tbg[i]) <= 0) continue;
    MS_StrikePlayer(sh + SH_PLAYER, 0, sh + SH_HUD);
    StrikeBadguy(g_tbg[i], RAM_DMG);
    return;
  }
}

// Where the locked target is this frame, or -1 once it has died (or turned
// invincible, which is the same thing to a missile).
static int FindTarget(u32 bg) {
  for (int i = 0; i < g_targets; i++)
    if (g_tbg[i] == bg) return i;
  return -1;
}

// Nearest enemy inside the missile's forward cone. A missile only locks on to
// what it can turn to face in its AIM time; anything wider and it would
// commit to a heading that misses.
static int PickTarget(s32 x, s32 y, s32 angle) {
  int best = -1;
  s32 bestd = 0x7fffffff;
  for (int i = 0; i < g_targets; i++) {
    s32 dx = g_tx[i] - x, dy = g_ty[i] - y;
    if (dx > 600 || dx < -600 || dy > 900 || dy < -900) continue;  // d^2 fits
    s32 off = UE_GetAngleDelta(angle, UE_GetAngle(x, y, g_tx[i], g_ty[i]));
    if (off > LOCK_CONE || off < -LOCK_CONE) continue;
    s32 d = dx * dx + dy * dy;
    if (d < bestd) { bestd = d; best = i; }
  }
  return best;
}

static int HeadingFrame(s32 angle) {
  // frame 8 is nose-up (UP); one frame per 2^20, rounded to nearest
  return (((angle - UP + 0x80000) >> 20) + 8) & 15;
}

static void Adopt(u32* e) {
  MODEL* src = (MODEL*)e[0];
  DEF* d = &src->defs[e[1]];
  const s32 kind = d->vy;
  const KIND* k = &kKinds[kind];
  MISSILE* m = 0;
  for (int i = 0; i < MAX_MISSILES; i++)
    if (!g_pool[i].entry) { m = &g_pool[i]; break; }
  if (!m) {                                // pool full: let it fly off-screen
    e[3] = -1000;
    return;
  }
  m->m.damage = k->damage;
  m->m.radius = k->radius;
  m->m.mask = 0xf;
  m->m.count = k->super ? 6 : 16;
  m->m.defs = k->super ? kSuperFrames : kHeadings;
  m->x = (s32)e[2] << 16;
  m->y = (s32)e[3] << 16;
  m->speed = k->v0;
  m->angle = (UP + DEG(1) * d->vx) & 0xffffff;
  m->age = 0;
  m->kind = kind;
  m->anim = 0;
  m->state = k->turn ? MS_LAUNCH : MS_STRAIGHT;
  m->target = 0;
  m->aim_age = 0;
  m->fixes = MISSILE_FIXES;
  m->entry = e;
  m->seen = 1;
  e[0] = (u32)&m->m;
}

// How a missile steers -- deliberately not like H, whose charges chase:
//   LAUNCH     straight off the rail for k->delay frames, then locks on to the
//              nearest enemy within LOCK_CONE of its heading and goes to AIM
//              (none: STRAIGHT);
//   AIM        turns toward its target at k->turn per frame until it points at
//              it (or MAX_AIM frames pass), then COMMITTED;
//   COMMITTED  flies straight. If the target slips more than DRIFT_PX off that
//              line while still ahead, it may re-aim -- MISSILE_FIXES times,
//              once -- and after that it is committed for good;
//   STRAIGHT   no more steering, ever. Where a missile ends up once its
//              target dies, whatever state it was in.

static void Steer1(MISSILE* m, const KIND* k) {
  const s32 x = m->x >> 16, y = m->y >> 16;
  if (m->state == MS_LAUNCH) {
    if (m->age < k->delay) return;
    const int t = PickTarget(x, y, m->angle);
    if (t < 0) { m->state = MS_STRAIGHT; return; }
    m->target = g_tbg[t];
    m->state = MS_AIM;
    m->aim_age = 0;
  }
  if (m->state == MS_STRAIGHT) return;
  const int t = FindTarget(m->target);
  if (t < 0) { m->state = MS_STRAIGHT; return; }   // target died
  const s32 dx = g_tx[t] - x, dy = g_ty[t] - y;
  if (m->state == MS_COMMITTED) {
    // How far the target sits off the line of flight, and whether it is
    // still in front: the cross and dot products with the heading.
    const s32 c = UE_Cos(m->angle), s = UE_Sin(m->angle);
    const s32 ahead = (dx * c + dy * s) >> 16;
    s32 off = (dx * s - dy * c) >> 16;
    if (off < 0) off = -off;
    if (ahead <= 0 || off <= DRIFT_PX) return;
    if (m->fixes <= 0) { m->state = MS_STRAIGHT; return; }
    m->fixes--;
    m->state = MS_AIM;
    m->aim_age = 0;
  }
  // MS_AIM
  const s32 want = UE_GetAngle(x, y, g_tx[t], g_ty[t]);
  s32 d = UE_GetAngleDelta(m->angle, want);
  if (d <= k->turn && d >= -k->turn) {
    m->angle = want & 0xffffff;          // on target: lock the heading
    m->state = MS_COMMITTED;
    return;
  }
  if (d > k->turn) d = k->turn;
  if (d < -k->turn) d = -k->turn;
  m->angle = (m->angle + d) & 0xffffff;
  if (++m->aim_age >= MAX_AIM) m->state = MS_COMMITTED;
}

static void Fly(MISSILE* m) {
  const KIND* k = &kKinds[m->kind];
  m->age++;
  m->speed += k->accel;
  if (m->speed > k->vmax) m->speed = k->vmax;
  if (k->turn) Steer1(m, k);
  m->x += UE_MulFP(UE_Cos(m->angle), m->speed);
  m->y += UE_MulFP(UE_Sin(m->angle), m->speed);
  u32* e = m->entry;
  e[2] = (u32)(m->x >> 16);
  e[3] = (u32)(m->y >> 16);
  if (k->super) {
    // a counter, not age % 6: see the file header (and the compiler turns a
    // subtract loop back into a modulo)
    if (!(m->age & 1) && ++m->anim == 6) m->anim = 0;
    e[1] = (u32)m->anim;
  } else {
    e[1] = (u32)HeadingFrame(m->angle);
  }
}

// Called once per game update, after the ship has fired.
static void Steer(void) {
  for (int i = 0; i < MAX_MISSILES; i++) g_pool[i].seen = 0;
  for (int k = 0; k < AURAS; k++) g_aura[k].seen = 0;
  g_coil_alive = 0;
  GatherTargets();
  for (u32* e = UE_GetFirstBagEntry(BAG_SHOTS); e;
       e = UE_GetNextBagEntry(BAG_SHOTS, e)) {
    const u32 m = e[0];
    if (IsSpawnModel(m)) {
      Adopt(e);
      if (IsOurs(e[0])) Fly((MISSILE*)e[0]);
    } else if (IsOurs(m)) {
      MISSILE* ms = (MISSILE*)m;
      if (ms->entry != e) continue;        // stale: slot reused elsewhere
      ms->seen = 1;
      Fly(ms);
    } else if (IsCoil(m)) {
      g_coil_alive++;                      // the engine flies these itself
    } else if (AuraKind(m) >= 0) {
      SeeAura(e, AuraKind(m));
    }
  }
  for (int k = 0; k < AURAS; k++)
    if (!g_aura[k].seen) g_aura[k].entry = 0;
  // A missile whose entry is gone has hit something or left the screen.
  for (int i = 0; i < MAX_MISSILES; i++)
    if (!g_pool[i].seen) g_pool[i].entry = 0;
}

// ---- P in action ---------------------------------------------------------------
// After Steer, which has gathered the targets (positions in whole units).
static s32 Abs(s32 v) { return v < 0 ? -v : v; }

static DEF kArcDef[1] = { { SPR_IMPACT_BIG, NEVER, 0, 0, 0, 0, SPR_IMPACT_BIG, -9, -55 } };
static MODEL kArc = { 0, 0, 0, 1, kArcDef };

// A bolt from the sphere's rim to (tx, ty): sparks every ARC_STEP along it,
// zig-zagging, drawn as the engine's own shot impacts.
static void Bolt(s32 sx, s32 sy, s32 r, s32 tx, s32 ty, s32 d) {
  const s32 a = UE_GetAngle(sx, sy, tx, ty);
  const s32 ux = UE_Cos(a), uy = UE_Sin(a);
  int k = 0;
  for (s32 t = r; t < d; t += ARC_STEP, k++) {
    const s32 side = (k & 1) ? 3 : -3;
    const s32 x = sx + (UE_MulFP(ux, t << 16) >> 16) - ((side * uy) >> 16);
    const s32 y = sy + (UE_MulFP(uy, t << 16) >> 16) + ((side * ux) >> 16);
    AddShotImpact((u32)&kArc, 0, x, y);
  }
  AddShotImpact((u32)&kArc, 0, tx, ty);
}

static void PlasmaTick(u32 player) {
  const PLASMA* k = &kPlasma[PlasmaLevel(player) - 1];
  const s32 sx = R32(player, P_X) >> 16, sy = R32(player, P_Y) >> 16;
  const int burn = ++g_plasma_t >= PLASMA_EVERY;
  if (burn) g_plasma_t = 0;
  const s32 reach = k->r + 6, r2 = k->r * k->r;
  for (int i = 0; i < g_targets; i++) {
    const s32 dx = g_tx[i] - sx, dy = g_ty[i] - sy;
    if (dx > reach || dx < -reach || dy > reach || dy < -reach) continue;
    const s32 d2 = dx * dx + dy * dy;
    if (d2 > reach * reach) continue;
    const u32 bg = g_tbg[i];
    if (burn && AbyssGate(bg, SRC_OTHER)) StrikeBadguy(bg, PlasmaDamage(bg, k->dmg));
    // the deflector: small fry inside are put back on the rim
    if (k->push && d2 < r2) {
      const s32 hp = BH_GetBadGuyHealth(bg);
      const u32 actor = hp > 0 && hp <= PUSH_MAX_HP ? MS_GetBadGuyActor(bg) : 0;
      if (actor) {
        const s32 a = UE_GetAngle(sx, sy, g_tx[i], g_ty[i]);
        MS_SetActorPosition(actor, (sx << 16) + UE_MulFP(UE_Cos(a), (k->r + 2) << 16),
                            (sy << 16) + UE_MulFP(UE_Sin(a), (k->r + 2) << 16));
      }
    }
  }
  if (!k->arcs) return;
  if (g_arc_cool > 0) { g_arc_cool--; return; }
  // lightning: the nearest targets outside the sphere, within reach
  int took[3] = { -1, -1, -1 };
  int n = 0;
  for (int j = 0; j < k->arcs; j++) {
    int best = -1;
    s32 bd = ARC_RANGE + 1;
    for (int i = 0; i < g_targets; i++) {
      if (i == took[0] || i == took[1] || i == took[2]) continue;
      const s32 dx = Abs(g_tx[i] - sx), dy = Abs(g_ty[i] - sy);
      if (dx > ARC_RANGE || dy > ARC_RANGE) continue;
      const s32 d = dx > dy ? dx + (dy >> 1) : dy + (dx >> 1);   // within an eighth
      if (d <= k->r || d >= bd || BH_GetBadGuyHealth(g_tbg[i]) <= 0) continue;
      bd = d;
      best = i;
    }
    if (best < 0) break;
    took[n++] = best;
    if (AbyssGate(g_tbg[best], SRC_OTHER)) StrikeBadguy(g_tbg[best], PlasmaDamage(g_tbg[best], k->arc_dmg));
    Bolt(sx, sy, k->r, g_tx[best], g_ty[best], bd);
  }
  if (n) {
    UE_PlaySound(SND_COIL, 0, 0, 0, 0);
    g_arc_cool = k->arc_every;
  }
}

// What is drawn round the ship this frame.
static u32 g_tick;

static void Auras(u32 player) {
  g_tick++;
  int bubble = (g_tick >> 2) & 3;
  if (g_shield_left < SHIELD_FADE && (g_tick & 8)) bubble = 4;      // running out
  Aura(player, AURA_BUBBLE, g_shield_left > 0, &kBubble, bubble);
  const int p = R32(player, P_TYPE) == TYPE_E && g_plasma;
  Aura(player, AURA_SPHERE, p, &kSphere[PlasmaLevel(player) - 1], (g_tick >> 2) & 3);
}

// ---- hooks: each replaces one `bl` in the game ------------------------------

// The red bolt is drawn heading down. Swapping its top and bottom texture
// coordinates turns it round. At any scale but 0.5, 1 and 2 the engine also
// nudges each coordinate 0.001 inward when it draws -- which, swapped, would
// be outward, into the neighbouring sprite -- so those sizes are stored 0.002
// further in to land where they should. Checked every frame rather than once,
// because a texture reload (the app losing its GL context) would put the
// original coordinates back.
#define F_PLUS_2EPS  0x3b03126fu   // 0.002f
#define F_MINUS_2EPS 0xbb03126fu   // -0.002f
static void FlipUpright(u32 sprite) {
  u32* hw = UE_GetHandlePtr(sprite, SPRITE_TAG);
  if (!hw) return;
  // v at +0x10 and +0x18, positive floats: integer order is float order
  const s32 v0 = (s32)hw[4], v1 = (s32)hw[6];
  if (v0 >= v1) return;                    // already upright
  const u32 scale = hw[2];
  if (scale == 0x8000 || scale == 0x10000 || scale == 0x20000) {
    hw[4] = (u32)v1;
    hw[6] = (u32)v0;
  } else {
    hw[4] = FAdd((u32)v1, F_MINUS_2EPS);
    hw[6] = FAdd((u32)v0, F_PLUS_2EPS);
  }
}

static void FlipSlugs(void) {
  for (int i = 0; i < SLUG_SIZES; i++) FlipUpright(SPR_SLUG + i);
}

// UpdateWeapon: `bl UpdateSpread`. Spread first, exactly as before; then ours.
void hook_update_spread(u32 player) {
  UpdateSpread(player);
  const s32 type = R32(player, P_TYPE);
  if (type == TYPE_M) {
    const s32 shots = UpdateShotWeapon(player, TYPE_M, kModelPtrs, kSatModels);
    if (shots > 0) UE_PlaySound(SND_LAUNCH, 0, 0, 0, 0);
  }
  if (g_coil_cool > 0) g_coil_cool--;
  if (type != TYPE_E) g_plasma = 0;        // picked something else
  if (type == TYPE_E && !g_plasma) {
    FlipSlugs();
    if (!g_coil_alive && !g_coil_cool &&
        UpdateShotWeapon(player, TYPE_E, kCoilPtrs, kCoilSats) > 0) {
      g_npierced = 0;
      UE_PlaySound(SND_COIL, 0, 0, 0, 0);
      g_coil_cool = R32(player, P_LEVEL) == 5 ? COIL_COOLDOWN_OL : COIL_COOLDOWN;
    }
  }
  Steer();
  ShieldTick(player);
  if (type == TYPE_E && g_plasma) PlasmaTick(player);
  Auras(player);
}

// The satellites' formation table has a row per weapon and nothing past L.
// M and E fly Spread's formation; the type is lent and restored.
static int IsNewType(s32 t) { return t == TYPE_M || t == TYPE_E; }

void CourierTick(void);           // courier.c: Seoul's shield couriers
void EdinburghTick(void);         // edinburgh.c: Mission 8's homecoming
void AndamanTick(void);           // andaman.c: Mission 9's flight, riders and shells
void SundaTick(void);             // sunda.c: Mission 10's fleet and ambush
void HomewardTick(void);          // homeward.c: Mission 13's sea, story and Valhalla
void AmaterasuTick(void);         // amaterasu.c: Mission 14's HQ, launch, painting
void ParisTick(void);             // paris.c: Mission 15's flyover, base, launch, evacuation

// Mission 10's jam (coelacanth.c): while it is set the ship's weapons can't
// fire -- WEAPON+4, the trigger UpdateWeapon has just read from the input,
// held off before anything that fires looks at it. (The engine's own
// DisableWeapon only sets SHOGUN+0x51c0, which the weapon menus read: the
// ship fired on.)
int g_weapons_jammed;
// ... and once the pilot is aboard the Valhalla, the ship isn't drawn
// (onRender's and RenderNight's `bl RenderPlayer`, RenderShadows' shadow).
int g_hide_player;

void hook_render_player(u32 shogun, s32 x, s32 y, s32 z) {
  if (!g_hide_player) RenderPlayer(shogun, x, y, z);
}

void hook_render_player_shadow(u32 shogun, s32 x, s32 y) {
  if (!g_hide_player) RenderPlayerShadow(shogun, x, y);
}

// UpdatePlayer's `bl UpdatePlayerStatePreOutro` / `UpdatePlayerStateOutro`:
// the level's end warp (the ship drawn back, the warp's sound, its sparks,
// away up the screen). For a ship that has gone under the sea (unseen) no
// sparks and no sound -- but it must still leave the screen: the level only
// ends once it has (skipping the outro altogether left Mission 10 standing
// on its last picture for good).
void hook_player_preoutro(u32 player, u32 a, u32 b) { PlayerPreOutro(player, a, b); }

void hook_player_outro(u32 player, u32 state) {
  if (!g_hide_player) {
    PlayerOutro(player, state);
    return;
  }
  R32(player, P_Y) -= 0x60000;                // (6 units a frame, up and away, unseen)
}

// Also the once-a-frame tick of the worlds' own goings-on.
void hook_update_satellites(u32 player) {
  if (g_weapons_jammed) R32(player, P_WEAPON + 4) = 0;
  const s32 t = R32(player, P_TYPE);
  if (IsNewType(t)) R32(player, P_TYPE) = 1;
  UpdateSatellites(player);
  R32(player, P_TYPE) = t;
  CourierTick();
  EdinburghTick();
  AndamanTick();
  SundaTick();
  AbyssTick();
  HomewardTick();
  AmaterasuTick();
  ParisTick();
}

void LateDraws(void);              // abyss.c: generics kept back from their pass (Mission 15's rocket)

void hook_render_satellites(u32 player, s32 x, s32 y, s32 z) {
  const s32 t = R32(player, P_TYPE);
  if (IsNewType(t)) R32(player, P_TYPE) = 1;
  RenderSatellites(player, x, y, z);
  R32(player, P_TYPE) = t;
  LateDraws();                      // (after every generic: drawn over them)
}

// UpdateWeapon's action dispatch, past "1-3 set weapon" and "4 power up".
// 5 is bullet cancel, as before; 6 and 7 are ours. The caller clears the
// action.
void c_action(u32 player, s32 action) {
  if (action == 5) {
    WeaponBulletCancel(player);
  } else if (action == ACTION_M || action == ACTION_E || action == ACTION_P) {
    const u32 w = player + P_WEAPON;
    R32(w, W_TYPE) = action == ACTION_M ? TYPE_M : TYPE_E;
    g_plasma = action == ACTION_P;
    if (R32(w, W_LEVEL) != 5) R32(w, W_CHARGE) = 0;
    UE_PlaySound(SND_SELECT, 0, 0, 0, 0);
  }
}

// UpdateShots, on a hit: `bl StrikeBadguy`, then `bl AddShotImpact`, then a
// branch to delete the shot. A slug strikes each enemy once and survives; the
// delete branch now skips the delete, which c_impact does for everything else.
static int g_repeat_hit;      // set by c_strike for the c_impact that follows

void c_strike(u32 badguy, s32 damage, u32* entry) {
  g_repeat_hit = 0;
  if (AuraKind(entry[0]) >= 0) return;     // drawn only: PlasmaTick burns
  if (!AbyssGate(badguy, IsCoil(entry[0]) ? SRC_SNIPER : AbyssShotSource(entry[0]))) return;
  if (IsCoil(entry[0])) {
    g_repeat_hit = AlreadyPierced(badguy, entry[1]);
    if (entry[1] != 0) damage = (damage * 3) >> 3;   // a side slug
    if (damage == 100) damage = 101;   // 100 would ignore invincibility
  }
  if (!g_repeat_hit) StrikeBadguy(badguy, damage);
}

void c_impact(u32 model, u32 def, s32 x, s32 y, u32* entry) {
  if (AuraKind(model) >= 0) return;        // stays on the ship
  if (IsCoil(model)) {
    // one impact per enemy, on the frame it was first struck; no delete
    if (!g_repeat_hit) AddShotImpact(model, def, x, y);
    return;
  }
  AddShotImpact(model, def, x, y);
  UE_DeleteBagEntry(BAG_SHOTS, entry);
}

// SetActionWeaponSwitch (the hardware switch key): S H L M E P, then S again.
void hook_switch(u32 weapon) {
  if (R32(weapon, W_ACTION) != 0) return;
  const s32 t = R32(weapon, W_TYPE);
  s32 a = t == 3 ? ACTION_M : (t == TYPE_M ? ACTION_E
                              : (t == TYPE_E && !g_plasma ? ACTION_P : (t >= TYPE_E ? 1 : t + 1)));
  if (AbyssNoSeekers()) {                  // Mission 11: S, L, E, P -- no H, no M
    if (a == 2) a = 3;
    if (a == ACTION_M) a = ACTION_E;
  }
  R32(weapon, W_ACTION) = a;
}

// ---- the radial menu --------------------------------------------------------

// The hexagon's six cells are S, H, L, EMP, + and M, so E goes in the outer
// ring, one cell beyond S. Near the top of the screen that would be cut off,
// so there it goes one cell beyond M instead, with its frame flipped to face
// the ship from below. The menu centre is fixed while the menu is open, so
// drawing and hit-testing always agree on which.
#define RING2_NORMAL 0x26536    // 2 x S's 1.198
#define RING2_NARROW 0x207ae    // 2 x S's 1.015

static s32 Ring2(void) { return isNarrowScreen() ? RING2_NARROW : RING2_NORMAL; }

static int CoilBelow(void) {
  const u32 sh = UE_GetGameDataPtr();
  const s32 rise = UE_MulFP(Ring2(), R32(sh, SH_MENU_SCALE)) >> 16;
  return R32(sh, SH_MENU_Y) - rise < 48;      // 48: half a button, and a bit
}

static u32 CoilButton(void) { return CoilBelow() ? SPR_E_DOWN : SPR_E_UP; }
static u32 PlasmaButton(void) { return CoilBelow() ? SPR_P_DOWN : SPR_P_UP; }
static u32 ShieldButton(void) { return CoilBelow() ? SPR_SH_DOWN : SPR_SH_UP; }

// GetEntryCoord: unit-hexagon positions, scaled by the menu. The table has
// five; M takes the empty bottom slot, mirroring S at the top, and E the
// outer ring as above. P and the shield take the second ring's cells either
// side of E, nestled between it, S and H or L (sqrt 3 cells out, half a cell
// to the side): above the ship, or, with E, below it near the top.
#define SIDE_NORMAL 0x10998     // 0.866 x S's 1.198
#define SIDE_NARROW 0xe105      // 0.866 x S's 1.015
#define RISE_NORMAL 0x1cc08     // 1.5 x 1.198
#define RISE_NARROW 0x185c3     // 1.5 x 1.015
void hook_entry_coord(s32 entry, s32* x, s32* y) {
  if (entry == ENTRY_P || entry == ENTRY_SH) {
    const int narrow = isNarrowScreen();
    const s32 side = narrow ? SIDE_NARROW : SIDE_NORMAL;
    const s32 rise = narrow ? RISE_NARROW : RISE_NORMAL;
    *x = entry == ENTRY_P ? -side : side;
    *y = CoilBelow() ? rise : -rise;
    return;
  }
  if (entry == ENTRY_M) {
    *x = 0;
    *y = isNarrowScreen() ? 0x103d7 : 0x1329b;   // 1.015 / 1.198
  } else if (entry == ENTRY_E) {
    *x = 0;
    *y = CoilBelow() ? Ring2() : -Ring2();
  } else {
    GetEntryCoord(entry, x, y);
  }
}

int EdUpgradesReady(void);         // edinburgh.c: P and the shield, once taught

// ChooseBulletTimeWeaponMenu tests slots 0..4 in turn; extend it to 5-8.
s32 hook_test_last(u32 menu, u32 entry, s32 x, s32 y) {
  s32 r = TestWeaponSelection(menu, entry, x, y);
  if (!r && !AbyssNoSeekers()) r = TestWeaponSelection(menu, ENTRY_M, x, y);
  if (!r) r = TestWeaponSelection(menu, ENTRY_E, x, y);
  if (!r && EdUpgradesReady()) r = TestWeaponSelection(menu, ENTRY_P, x, y);
  if (!r && EdUpgradesReady()) r = TestWeaponSelection(menu, ENTRY_SH, x, y);
  return r;
}

static void CloseMenu(u32 menu) {
  if (R16(menu, WM_TUTOLOCK) == 0)
    DeactivateBTWeaponMenu(menu);
  else
    EnableWeapon(UE_GetGameDataPtr());
}

void hook_select(u32 menu, u32 player, u32 sel) {
  // P and the shield decide as E does (a value the engine is known to
  // take), then do their own thing
  if (sel == SELECT_SH) {
    R32(menu, WM_CANCEL) = 0;
    DecideBTWeaponMenu(menu, player, SELECT_E);
    R32(menu, WM_DECIDED) = ShieldButton();
    ShieldBuy(player);                  // no capsule: nothing happens
    CloseMenu(menu);
    return;
  }
  if (sel == SELECT_P) {
    R32(menu, WM_CANCEL) = 0;
    if (R32(player, P_TYPE) == TYPE_E && g_plasma && R16(menu, WM_TUTOLOCK) == 0) {
      R32(menu, WM_DECISION) = 7;       // already holding it: just close
      DeactivateBTWeaponMenu(menu);
      return;
    }
    DecideBTWeaponMenu(menu, player, SELECT_E);
    R32(menu, WM_DECIDED) = PlasmaButton();
    SetActionWeaponSet(player + P_WEAPON, ACTION_P);
    CloseMenu(menu);
    return;
  }
  if (AbyssNoSeekers() && (sel == 2 || sel == SELECT_M)) {   // H, M: drowned (Mission 11)
    R32(menu, WM_DECISION) = 7;         // just close
    DeactivateBTWeaponMenu(menu);
    return;
  }
  if (sel != SELECT_M && sel != SELECT_E) {
    SelectBTWeaponMenu(menu, player, sel);
    return;
  }
  const s32 type = sel == SELECT_M ? TYPE_M : TYPE_E;
  R32(menu, WM_CANCEL) = 0;
  if (R32(player, P_TYPE) == type && !(type == TYPE_E && g_plasma) &&
      R16(menu, WM_TUTOLOCK) == 0) {
    R32(menu, WM_DECISION) = 7;         // already holding it: just close
    DeactivateBTWeaponMenu(menu);
    return;
  }
  DecideBTWeaponMenu(menu, player, sel);
  // pulse our button, not the 0x761 + sel frame Decide picked
  R32(menu, WM_DECIDED) = sel == SELECT_M ? SPR_BUTTON : CoilButton();
  SetActionWeaponSet(player + P_WEAPON, sel == SELECT_M ? ACTION_M : ACTION_E);
  CloseMenu(menu);
}

// RenderBulletTimeWeaponMenu's last call, `bl UE_PopBlendMode`: draw M and E
// with the others, while their blend mode is still pushed. Tutorials restrict
// the menu to the weapon being taught, so ours stay out of them.
void c_render_menu(u32 shogun, s32 x, s32 y) {
  if (R16(shogun, SH_TUTOLOCK) == 0) {
    if (!AbyssNoSeekers()) RenderBTWeaponMenuSprite(shogun, ENTRY_M, x, y, 0x10000);
    RenderBTWeaponMenuSprite(shogun, ENTRY_E, x, y, 0x10000);
    if (EdUpgradesReady()) {
      RenderBTWeaponMenuSprite(shogun, ENTRY_P, x, y, 0x10000);
      RenderBTWeaponMenuSprite(shogun, ENTRY_SH, x, y, 0x10000);
    }
  }
  UE_PopBlendMode();
}

// The button sprite for a slot (the shim below calls this).
u32 c_sprite_id(u32 slot) {
  if (slot == ENTRY_M) return SPR_BUTTON;
  if (slot == ENTRY_E) return CoilButton();
  if (slot == ENTRY_P) return PlasmaButton();
  if (slot == ENTRY_SH) return ShieldButton();
  return AbyssMenuSprite(slot, 0x762 + slot);
}

// LoadWeapon, after LoadSatellites: load what M draws. A leading '*' makes
// the engine read from the files directory instead of its pack, which is how
// the button -- art the game never had -- gets in.
void hook_load(u32 weapon) {
  LoadSatellites(weapon);
  g_weapons_jammed = g_hide_player = 0;      // (Mission 10's ending, left behind)
  UE_LoadSprites(SPR_BUTTON, "*weapon_m.tga", 1, 0x8000);
  UE_LoadSprites(SPR_MISSILE, "worlds/ocean/badguys/lvl1-badguy_missile.tga",
                 16, 0x10000);
  UE_LoadSprites(SPR_SUPER,
                 "worlds/ocean/badguys/lvl1-badguy_super_missile.tga", 6,
                 0x10000);
  UE_LoadSprites(SPR_E_UP, "*weapon_e_up.tga", 1, 0x8000);
  UE_LoadSprites(SPR_E_DOWN, "*weapon_e_down.tga", 1, 0x8000);
  for (int i = 0; i < SLUG_SIZES; i++)
    UE_LoadSprites(SPR_SLUG + i, "worlds/ocean/midboss/lboss1bullets.tga", 1,
                   kSlugScale[i]);
  FlipSlugs();
  UE_LoadSound(SND_COIL, SND_COIL_FILE);
  g_coil_alive = g_coil_cool = 0;
  for (int i = 0; i < MAX_MISSILES; i++) g_pool[i].entry = 0;
  // the shield and P (missile/make_upgrades.py)
  UE_LoadSprites(SPR_P_UP, "*weapon_p_up.tga", 1, 0x8000);
  UE_LoadSprites(SPR_P_DOWN, "*weapon_p_down.tga", 1, 0x8000);
  UE_LoadSprites(SPR_SH_UP, "*weapon_sh_up.tga", 1, 0x8000);
  UE_LoadSprites(SPR_SH_DOWN, "*weapon_sh_down.tga", 1, 0x8000);
  UE_LoadSprites(SPR_BUBBLE, "*shield_bubble.tga", 5, 0x10000);
  for (int l = 0; l < 5; l++)
    UE_LoadSprites(SPR_PLASMA + 4 * l, "*plasma_sphere.tga", 4, kSphereScale[l]);
  for (int k = 0; k < AURAS; k++) g_aura[k].entry = 0;
  g_shield_left = 0;
  GodHold(GOD_SHIELD, 0);
  g_plasma_t = g_arc_cool = 0;
}

// ---- register shims for the three patch sites that are not calls ----------
// Each replaces instructions mid-function, so the values it needs are in
// whatever registers the original code had them in.

// UpdateWeapon @0x42b68: `cmp r3, #5 ; beq BulletCancel`. r3 = action,
// r4 = PLAYER; execution resumes where the action is cleared.
__attribute__((naked)) void shim_action(void) {
  __asm__ volatile(
      "push {lr}\n"
      "adds r0, r4, #0\n"          // ARMv5 Thumb has no low-register mov
      "adds r1, r3, #0\n"
      "bl c_action\n"
      "pop {pc}\n");
}

// RenderBulletTimeWeaponMenu @0x38240: r4 = SHOGUN, r6/r7 = menu centre.
__attribute__((naked)) void shim_render_menu(void) {
  __asm__ volatile(
      "push {lr}\n"
      "adds r0, r4, #0\n"
      "adds r1, r6, #0\n"
      "adds r2, r7, #0\n"
      "bl c_render_menu\n"
      "pop {pc}\n");
}

// RenderBulletTimeWeaponMenuSprite @0x3802e: `ldr r2, =0x762 ; adds r6, r5, r2`
// picks the sprite as 0x762 + slot (r5). Slots 5 and 6 would land on the
// overload frame and past the sheet, so c_sprite_id answers instead. Sets r6,
// leaves r2 as the original did, and preserves everything else live.
__attribute__((naked)) void shim_sprite_id(void) {
  __asm__ volatile(
      "push {r0, r1, r3, lr}\n"
      "adds r0, r5, #0\n"
      "bl c_sprite_id\n"
      "adds r6, r0, #0\n"
      "ldr r2, =0x762\n"
      "pop {r0, r1, r3, pc}\n"
      ".ltorg\n");
}

// UpdateShots @0x3c2ec: `bl StrikeBadguy` with r0 = enemy, r1 = damage and
// r4 = the shot entry.
__attribute__((naked)) void shim_strike(void) {
  __asm__ volatile(
      "push {lr}\n"
      "adds r2, r4, #0\n"
      "bl c_strike\n"
      "pop {pc}\n");
}

// UpdateShots @0x3c2f8: `bl AddShotImpact` with its four arguments in r0-r3
// and the shot entry in r4, passed on as a fifth.
__attribute__((naked)) void shim_impact(void) {
  __asm__ volatile(
      "push {r4, lr}\n"
      "sub sp, #8\n"
      "str r4, [sp]\n"
      "bl c_impact\n"
      "add sp, #8\n"
      "pop {r4, pc}\n");
}
