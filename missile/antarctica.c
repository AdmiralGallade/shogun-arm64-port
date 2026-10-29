// antarctica.c -- the chase's cast (Mission 7, world 7), registered with the
// bullet-hell engine.
//
// Built like the Beach and Seoul (beach.c, seoul.c): data in the files
// directory under antarctica/ (antarctica/make_antarctica.py), reached with
// the engine's '*' paths; worlds.c routes the game to it. Its cast is the
// Shogun's dreadnought and its guns and interceptors, UNITY's railgun ship
// (antarctica/chase_cast.py), and ships borrowed from the original game's
// worlds as the Shogun's escorts, two of them carrying capsules. chase.c runs
// the level.
//
// Same rules as missile.c: no libc, no division, engine reached by address.

typedef unsigned int u32;

#define IMG 0x10000000u
#define FN(type, addr) ((type)(IMG + (addr)))

#define AN_LoadBadGuy       FN(void (*)(u32, const char*), 0xbf324)     // ARM
#define AN_LoadBullet       FN(void (*)(u32, const char*), 0xbeffc)     // ARM
#define AN_LoadGeneric      FN(void (*)(u32, const char*), 0xbf140)     // ARM

#define BH 0x405

static const char* const kBadguys[] = {
  "*antarctica/scripts/karasu",
  "*antarctica/boss/turret",
  "*antarctica/boss/bow_gun",
  "*antarctica/boss/flak",
  "*antarctica/boss/wrath",
  // Tokyo's ships, the Shogun's escorts, and the missile they and his guns fire
  "worlds/storm/scripts/badguy01",
  "worlds/storm/scripts/badguy02",
  "worlds/storm/scripts/badguy03",
  "worlds/storm/scripts/badguy08",
  "worlds/storm/scripts/badguy09",
  "worlds/ocean/scripts/badguy_missile",
  // more of the original game's ships, for variety
  "worlds/volcano/scripts/badguy02",
  "worlds/desertcity/scripts/badguy05",
  "worlds/desertcity/scripts/badguy06",
  "worlds/desertcity/scripts/badguy07",
  "worlds/desertcity/scripts/badguy_missile",
  "worlds/ocean/scripts/badguy01c",
  // the capsule carriers: Ocean's and Volcano's ships on gentler scripts,
  // dropping the original's capsule through the original's explosions
  "*antarctica/scripts/carrier_a",
  "*antarctica/scripts/carrier_b",
  "*antarctica/scripts/karasu_x",      // hard mode's elite (chase_cast.py, docs/HARD_MODE.md)
};
static const char* const kBullets[] = {
  "*antarctica/boss/ember",
  "*antarctica/boss/spark",
  "*antarctica/boss/h_comma",          // hard mode's two-stage bullets (beach/hardkit.py)
  "*antarctica/boss/h_twist",
  "*antarctica/boss/h_snipe",
  "*antarctica/boss/h_lock",
  "*antarctica/boss/h_drift",
  "*antarctica/boss/h_hesitate",
  "*antarctica/boss/h_needle",
  "*antarctica/boss/h_sig",                 // its signature pair (hardkit.THEMES)
  "*antarctica/boss/h_sigb",
};
static const char* const kGenerics[] = {
  // the dreadnought, piece by piece
  "*antarctica/boss/underglow",
  "*antarctica/boss/aft",
  "*antarctica/boss/hull",
  "*antarctica/boss/streamer",
  "*antarctica/boss/nacelle",
  "*antarctica/boss/oni",
  "*antarctica/boss/hangar",
  "*antarctica/boss/hatch",
  "*antarctica/boss/megacannon",
  "*antarctica/boss/mega_beam",
  "*antarctica/boss/reticle",
  "*antarctica/boss/turret_vis",
  "*antarctica/boss/turret_big_vis",
  "*antarctica/boss/cannon_vis",
  "*antarctica/boss/blaze",
  // the Valkyrie, her shield, charge, beam, aiming line and wing guns
  "*antarctica/boss/valkyrie",
  "*antarctica/boss/railgun",
  "*antarctica/boss/shield",
  "*antarctica/boss/orb",
  "*antarctica/boss/beam",
  "*antarctica/boss/sight",
  "*antarctica/boss/wing_vis",
  "*antarctica/boss/bolt",
  "*antarctica/boss/smoke",           // her crash: the smoke, and Volta's pod
  "*antarctica/boss/pod",
  "*antarctica/scripts/shield_ward",
  // the borrowed ships' explosions (the carriers' emit the capsule)
  "worlds/ocean/scripts/badguy02explosion",
  "worlds/volcano/scripts/badguy06explosion",
  "worlds/desertcity/scripts/badguy05explosion",
  "worlds/desertcity/scripts/badguy06explosion",
};

#define COUNT(a) (sizeof(a) / sizeof((a)[0]))

// Before the world file, which binds every script's sprites (beach.c).
void RegisterAntarctica(void) {
  for (u32 i = 0; i < COUNT(kBullets); i++) AN_LoadBullet(BH, kBullets[i]);
  for (u32 i = 0; i < COUNT(kGenerics); i++) AN_LoadGeneric(BH, kGenerics[i]);
  for (u32 i = 0; i < COUNT(kBadguys); i++) AN_LoadBadGuy(BH, kBadguys[i]);
}

// worlds.c: before any added world loads, this one's own definitions go.
void ForgetDefinitions(const char* const* badguys, u32 nb, const char* const* bullets, u32 nu,
                       const char* const* generics, u32 ng);
void ForgetAntarctica(void) {
  ForgetDefinitions(kBadguys, COUNT(kBadguys), kBullets, COUNT(kBullets), kGenerics, COUNT(kGenerics));
}
