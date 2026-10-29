// limits.c -- engine tables made bigger than the game built them
// (docs/ENGINE_LIMITS.md).
//
// The environment's placements -- every prop the world file's .env puts
// on the ground, 9 words each (model, x, y, rotation, scale, extents) --
// lived in a fixed table of 512 inside ENVIRONMENT (SHOGUN + 0x114, the
// table at + 0x53c, i.e. SHOGUN + 0x650), clamped by loadEnvironment.
// They now live here, 2048 of them. Three functions touch the table, each
// through one base: loadEnvironment's three ways of filling it (a BGV3
// file is copied; BGV1/2 are converted, then BGV1's entries fixed up) and
// its reading of the last one (the loop's length), AddEntityDyn, which
// spawns them as the scroll reaches them, and the Ocean boss's
// isItemInOceanBossPath. Each base is made here by a `bl`
// over the two instructions that made it (build.py PATCHES); everything
// after keeps its own field offsets. The clamp itself, `lsls r3,r7,#2`
// (0x200), is build.py's EXTRA (`#4`, 0x800).

#define PLACEMENTS     2048
#define ENV_TABLE      0x53c      // the table's offset in ENVIRONMENT

unsigned int g_placements[PLACEMENTS * 9];

// loadEnvironment, BGV3 @0x47362: `lsls r3,r0,#3 ; adds r5,r6,r2`, then
// `adds r0,r4,r3` makes the copy's destination from SHOGUN (r4). Here r3
// becomes table - SHOGUN, so r0 comes out as the table.
__attribute__((naked)) void shim_env_copy(void) {
  __asm__ volatile(
      "adds r5, r6, r2\n"
      "ldr r3, =g_placements\n"
      "subs r3, r3, r4\n"
      "bx lr\n"
      ".ltorg\n");
}

// loadEnvironment, BGV1/2 @0x477d6: `lsls r1,r3,#3 ; adds r3,r4,r1`, the
// first entry to convert into.
__attribute__((naked)) void shim_env_convert(void) {
  __asm__ volatile(
      "ldr r3, =g_placements\n"
      "bx lr\n"
      ".ltorg\n");
}

// loadEnvironment, BGV1's fix-up @0x4782e: `adds r5,r4,r6 ; mov sl,r3`
// with r6 = 0x654, the first entry's second word.
__attribute__((naked)) void shim_env_fixup(void) {
  __asm__ volatile(
      "mov sl, r3\n"
      "ldr r5, =g_placements + 4\n"
      "bx lr\n"
      ".ltorg\n");
}

// loadEnvironment, the loop's length @0x4774c: `adds r6,r2,r4 ; adds
// r3,r6,r1` with r2 = count * 36 and r1 = 0x62c, so [r3 + 8] is the last
// placement's y (the environment repeats a whole number of background
// lengths past it). Without this the length came out 0 and every prop
// spawned at once (Ocean's floes, missing).
__attribute__((naked)) void shim_env_length(void) {
  __asm__ volatile(
      "ldr r3, =g_placements - 36\n"
      "adds r3, r3, r2\n"
      "bx lr\n"
      ".ltorg\n");
}

// AddEntityDyn @0x35294: `adds r5,r3,r4 ; mov sl,r5`, r3 = slot * 36 and
// r4 = ENVIRONMENT; sl + 0x53c + field reads the entry.
__attribute__((naked)) void shim_dyn_entry(void) {
  __asm__ volatile(
      "ldr r5, =g_placements - 0x53c\n"
      "adds r5, r5, r3\n"
      "mov sl, r5\n"
      "bx lr\n"
      ".ltorg\n");
}

// isItemInOceanBossPath @0x28e2c: `lsls r2,r3,#3 ; adds r6,r6,r0`, r6 =
// slot * 36, r0 = ENVIRONMENT; r2 (0x550) is a field offset.
__attribute__((naked)) void shim_ocean_path(void) {
  __asm__ volatile(
      "ldr r2, =g_placements - 0x53c\n"
      "adds r6, r6, r2\n"
      "lsls r2, r3, #3\n"
      "bx lr\n"
      ".ltorg\n");
}

// ... and @0x28e46: `lsls r6,r7,#2 ; adds r4,r6,r0`, the same entry again.
__attribute__((naked)) void shim_ocean_path2(void) {
  __asm__ volatile(
      "lsls r6, r7, #2\n"
      "ldr r4, =g_placements - 0x53c\n"
      "adds r4, r4, r6\n"
      "bx lr\n"
      ".ltorg\n");
}
