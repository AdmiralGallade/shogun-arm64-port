// beach.c -- the Beach world's cast, registered with the bullet-hell engine.
//
// The game's worlds are data: a .world config (music, the animated base
// texture, the 3D models and every sprite), an environment file that places
// those models along the scroll, a partition that schedules the enemies, and
// a text script per enemy. All of that for the Beach lives in the files
// directory under beach/ (built by beach/make_beach.py) and is reached with
// the engine's own '*' prefix, which reads from there instead of the pack.
// The Beach is world 5; worlds.c routes the game to it.
//
// Same rules as missile.c: no libc, no division, engine reached by address.

typedef unsigned int u32;
typedef int s32;

#define IMG 0x10000000u
#define FN(type, addr) ((type)(IMG + (addr)))

#define BH_LoadBadGuy       FN(void (*)(u32, const char*), 0xbf324)     // ARM
#define BH_LoadBullet       FN(void (*)(u32, const char*), 0xbeffc)     // ARM
#define BH_LoadGenericEntity FN(void (*)(u32, const char*), 0xbf140)    // ARM

#define BH 0x405

// Everything the Beach's scripts Emit or the partition or boss spawns.
// Loading is idempotent: the engine looks each name up first and skips what
// it has.
static const char* const kBadguys[] = {
  "*beach/scripts/gull",
  "*beach/scripts/skipper",
  "*beach/scripts/bouncer",
  "*beach/scripts/clacker",
  "*beach/scripts/medusa",
  "*beach/scripts/coco",
  "*beach/scripts/palmboat",
  "*beach/scripts/whale",
  "*beach/scripts/dolphin",
  "*beach/scripts/gull_glide",      // the easier company
  "*beach/scripts/skipper_cruise",
  "*beach/scripts/medusa_drift",
  "*beach/scripts/entropy_spinner",  // grazing drills (the tutorial's)
  "*beach/scripts/entropy_drip",
  "*beach/scripts/gull_x",          // hard mode's elites (enemies.py, docs/HARD_MODE.md)
  "*beach/scripts/skipper_x",
  "*beach/scripts/bouncer_x",
  "*beach/scripts/clacker_x",
  "*beach/scripts/medusa_x",
  "*beach/scripts/coco_x",
  "*beach/boss/core",
  "*beach/boss/arm",
  "*beach/boss/hermit",
  "*beach/boss/hermit_claw_l",
  "*beach/boss/hermit_claw_r",
  "*beach/boss/hermit_shadow",
};
static const char* const kBullets[] = {
  "*beach/boss/starlet",            // the starfish's own bullet
  "*beach/boss/h_comma",            // hard mode's two-stage bullets (beach/hardkit.py)
  "*beach/boss/h_twist",
  "*beach/boss/h_snipe",
  "*beach/boss/h_lock",
  "*beach/boss/h_drift",
  "*beach/boss/h_hesitate",
  "*beach/boss/h_needle",
  "*beach/boss/h_sig",                 // its signature pair (hardkit.THEMES)
  "*beach/boss/h_sigb",
};
static const char* const kGenerics[] = {
  "*beach/scripts/spout",
  "*beach/scripts/splash",
  "*beach/boss/bubbles",
  "*beach/boss/arm_rush",           // generic: it must never touch the ship
};

#define COUNT(a) (sizeof(a) / sizeof((a)[0]))

// Registered before the world file is read, because LoadWorldFile ends by
// binding every enemy's sprite names to the sprites the world just loaded.
void RegisterBeach(void) {
  for (u32 i = 0; i < COUNT(kBullets); i++) BH_LoadBullet(BH, kBullets[i]);
  for (u32 i = 0; i < COUNT(kGenerics); i++)
    BH_LoadGenericEntity(BH, kGenerics[i]);
  for (u32 i = 0; i < COUNT(kBadguys); i++) BH_LoadBadGuy(BH, kBadguys[i]);
}

// worlds.c: before any added world loads, this one's own definitions go.
void ForgetDefinitions(const char* const* badguys, u32 nb, const char* const* bullets, u32 nu,
                       const char* const* generics, u32 ng);
void ForgetBeach(void) {
  ForgetDefinitions(kBadguys, COUNT(kBadguys), kBullets, COUNT(kBullets), kGenerics, COUNT(kGenerics));
}
