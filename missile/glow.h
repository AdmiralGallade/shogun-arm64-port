// glow.h -- light overlays: what makes a boss's engines, cores and lamps
// flicker. Each is a small generic (beach/art_mech.py overlay) cut out of
// the boss's own sprite where its lights are, a few frames of them burning;
// laid every frame over the body at a fixed offset from its centre, it
// covers the still lights with burning ones. Only to be seen: nothing hits
// it, nothing is changed underneath.
//
// A body shows its lights as drawn in some of its animations only (it
// strikes, fires, charges, burns in others): an overlay's table gives, for
// each of the body's animations (its R18), the overlay's own animation, or
// GLOW_OFF -- then it is parked out of sight (up and to the left, clear of
// the wrap at -512: a generic put far off the playfield is culled).
//
// The generic's script is the worlds' usual held one: R20 set = vanish; R18
// picks the animation.
#pragma once

#define GLOW_OFF (-1)

#define GLOW_SpawnGeneric   ((u32 (*)(u32, const char*, s32, s32))(0x10000000u + 0xbc1d8))
#define GLOW_GenericActor   ((u32 (*)(u32))(0x10000000u + 0xbaba8))
#define GLOW_SetPosition    ((void (*)(u32, s32, s32))(0x10000000u + 0xc174c))
#define GLOW_SetRegister    ((void (*)(u32, u32, s32))(0x10000000u + 0xc1814))
#define GLOW_GetRegister    ((s32 (*)(u32, u32))(0x10000000u + 0xc17f8))
#define GLOW_PARK           (-300 * 65536)

// A new overlay (its actor, 0 if none), out of sight until it is put. (Made
// on the playfield -- one made far off it is culled at once -- and parked.)
static inline u32 GlowSpawn(u32 bh, const char* name) {
  const u32 e = GLOW_SpawnGeneric(bh, name, 200 * 65536, 200 * 65536);
  const u32 a = e ? GLOW_GenericActor(e) : 0;
  if (a) {
    GLOW_SetPosition(a, GLOW_PARK, GLOW_PARK);
    GLOW_SetRegister(a, 20, 0);
  }
  return a;
}

// Over the body's centre (x, y) at (dx, dy) units, showing animation `anim`
// (GLOW_OFF: out of sight).
static inline void GlowPut(u32 g, s32 x, s32 y, int dx, int dy, int anim) {
  if (!g) return;
  if (anim < 0) {
    GLOW_SetPosition(g, GLOW_PARK, GLOW_PARK);
    return;
  }
  GLOW_SetPosition(g, x + dx * 65536, y + dy * 65536);
  GLOW_SetRegister(g, 18, anim * 65536);
}

// The overlay's animation for the body's current one (`host` an actor),
// through `map` (n entries; past it, GLOW_OFF).
static inline int GlowFor(u32 host, const signed char* map, int n) {
  if (!host) return GLOW_OFF;
  const int k = GLOW_GetRegister(host, 18) >> 16;
  return k >= 0 && k < n ? map[k] : GLOW_OFF;
}

static inline void GlowDrop(u32* g) {
  if (*g) GLOW_SetRegister(*g, 20, 1 << 16);
  *g = 0;
}
