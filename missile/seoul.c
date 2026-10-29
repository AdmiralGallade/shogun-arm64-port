// seoul.c -- the Seoul world's cast, registered with the bullet-hell engine.
//
// Seoul (Mission 6, world 6) is built like the Beach (beach.c): data in the
// files directory under seoul/ (seoul/make_seoul.py), reached with the
// engine's '*' paths; worlds.c routes the game to it. Its cast is the
// Shogun's imperial guard (seoul/cast.py), Tokyo's own ships borrowed from
// the pack as they are, and the two bosses' parts (magpie.c, twins.c).
//
// Same rules as missile.c: no libc, no division, engine reached by address.

typedef unsigned int u32;

#define IMG 0x10000000u
#define FN(type, addr) ((type)(IMG + (addr)))

#define SE_LoadBadGuy       FN(void (*)(u32, const char*), 0xbf324)     // ARM
#define SE_LoadBullet       FN(void (*)(u32, const char*), 0xbeffc)     // ARM
#define SE_LoadGeneric      FN(void (*)(u32, const char*), 0xbf140)     // ARM

#define BH 0x405

static const char* const kBadguys[] = {
  "*seoul/scripts/guard",
  "*seoul/scripts/guard_dive",
  "*seoul/scripts/phoenix",
  "*seoul/scripts/haetae",
  "*seoul/scripts/palanquin",
  "*seoul/scripts/turtle",
  "*seoul/scripts/lantern",          // the grazing drill
  "*seoul/scripts/fan",
  "*seoul/scripts/courier",          // drops a shield ward (courier.c)
  "*seoul/scripts/guard_x",          // hard mode's elites (cast.py, docs/HARD_MODE.md)
  "*seoul/scripts/phoenix_x",
  "*seoul/scripts/haetae_x",
  "*seoul/scripts/fan_x",
  "*seoul/scripts/palanquin_x",
  "*seoul/scripts/turtle_x",
  "*seoul/scripts/shield_ship",      // Tokyo's shield carrier, made a passer-by
  "*seoul/boss/magpie",
  "*seoul/boss/dragoon",
  "*seoul/boss/empress",
  "*seoul/boss/union",
  "*seoul/boss/sword_arm",
  "*seoul/boss/fan_arm",
  // Tokyo's ships, and the missile that they and the bosses fire
  "worlds/storm/scripts/badguy01",
  "worlds/storm/scripts/badguy02",
  "worlds/storm/scripts/badguy03",
  "worlds/storm/scripts/badguy08",
  "worlds/storm/scripts/badguy09",
  "worlds/ocean/scripts/badguy_missile",
};
static const char* const kBullets[] = {
  "*seoul/boss/blossom",
  "*seoul/boss/kunai",
  "*seoul/boss/orb",
  "*seoul/boss/h_comma",            // hard mode's two-stage bullets (beach/hardkit.py)
  "*seoul/boss/h_twist",
  "*seoul/boss/h_snipe",
  "*seoul/boss/h_lock",
  "*seoul/boss/h_drift",
  "*seoul/boss/h_hesitate",
  "*seoul/boss/h_needle",
  "*seoul/boss/h_sig",                 // its signature pair (hardkit.THEMES)
  "*seoul/boss/h_sigb",
  "*seoul/boss/twist",               // the mob attacks after Missions 3/4's bosses (cast.py)
  "*seoul/boss/brake",
  "*seoul/boss/lock",
  "*seoul/boss/seeder",
};
static const char* const kGenerics[] = {
  "*seoul/boss/fist_sword",          // the rocket punch (twins.c)
  "*seoul/boss/fist_fan",
  "*seoul/scripts/shield_ward",      // what a courier drops
  "*seoul/boss/magpie_glow",         // the bosses' engines and reactors burning (glow.h)
  "*seoul/boss/dragoon_glow",
  "*seoul/boss/empress_glow",
  "*seoul/boss/union_glow",
};

#define COUNT(a) (sizeof(a) / sizeof((a)[0]))

// Before the world file, which binds every script's sprites (beach.c).
void RegisterSeoul(void) {
  for (u32 i = 0; i < COUNT(kBullets); i++) SE_LoadBullet(BH, kBullets[i]);
  for (u32 i = 0; i < COUNT(kGenerics); i++) SE_LoadGeneric(BH, kGenerics[i]);
  for (u32 i = 0; i < COUNT(kBadguys); i++) SE_LoadBadGuy(BH, kBadguys[i]);
}

// worlds.c: before any added world loads, this one's own definitions go.
void ForgetDefinitions(const char* const* badguys, u32 nb, const char* const* bullets, u32 nu,
                       const char* const* generics, u32 ng);
void ForgetSeoul(void) {
  ForgetDefinitions(kBadguys, COUNT(kBadguys), kBullets, COUNT(kBullets), kGenerics, COUNT(kGenerics));
}
