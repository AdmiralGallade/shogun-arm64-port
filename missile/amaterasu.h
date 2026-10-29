// amaterasu.h -- what Mission 14's three files share (amaterasu.c, barge.c, raijin.c).
#pragma once

#define AMATERASU 14              // SHOGUN+0x4e6c

void AmaReset(void);              // a new run (barge.c's init calls it)

// Amaterasu: painting (amaterasu.c). A target is painted while the ship stays
// near it; locked, the mirror's beam comes down on it after a delay and hits
// everything round the spot -- the ship too.
// PT_ARMOUR: a part the strike kills (shut to guns until then); PT_PART: a boss's part the
// strike kills when it's in the beam (not painted: no bracket); PT_BOSS: the boss's to act on
// PT_OPEN: a Ryujin's part: painted and struck only on its turn (its script's R14 clear)
enum { PT_BUOY, PT_PICKET, PT_DRONE, PT_ARMOUR, PT_BOSS, PT_PART, PT_OPEN };
void AmaPaintAdd(u32 bg, int kind);   // a target from now
void AmaPaintRemove(u32 bg);
int AmaStruck(u32 bg);            // strikes that landed on it since last asked
void AmaJam(int on);              // Raijin's jam: the engine dark, no grazing, no painting
void AmaFullSun(void);            // the mirror ready now, and a quicker paint (the finish)
int AmaStriking(void);            // a strike is coming down or burning
// Raijin's lightning: from his ship (sx, sy) to a mark on (tx, ty) -- it charges at the
// ship, an enemy-red mark and an aiming line show where, then it cracks along the line
void AmaThunder(s32 sx, s32 sy, s32 tx, s32 ty);
int AmaBolt(s32 sx, s32 sy, s32 tx, s32 ty, int charge);   // ... charging this long: its number, or -1
void AmaBoltFrom(int i, s32 sx, s32 sy);  // bolt i's source has moved: it comes from here
int AmaBoltBusy(int i);
void AmaPaintEnable(u32 bg, int on);   // a target paintable for now, or not
int AmaThunderBusy(void);
// Mission 15: the mirror without Mission 14's story, and the upgrade after France
void AmaSunReset(void);
void AmaSunTick(void);
void AmaUpgrade(int on);
void AmaSunPrespawn(void);        // Mission 16: the strike's pieces made now (over what came before), kept parked
void AmaLoadHud(void);           // the HUD sun's sprite (both missions' Register*)
void AmaHud(void);                // ... drawn beside the score when the mirror is ready (edinburgh.c's HUD hook)

void AmaEnding(void);             // raijin.c: Raijin is down -- the fleet comes up, the last lines
int AmaEndingDone(void);
void BargeBossInit(u32 boss);     // barge.c: the gun barge (worlds.c's mid-boss for world 14)
void RaijinBossInit(u32 boss);    // raijin.c: Lord Raijin's dragon (the barge installs it)
