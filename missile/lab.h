// lab.h -- what Mission 12's two files share (lab.c, lab_boss.c): the
// Trench's services (abyss.h: lines by name, booms, the Spin Drive), and
// the level's own -- striking the ship from code, UNITY's boats, the ending.

#pragma once

#include "abyss.h"

#define SH_GODFLAG   0x5180       // u16: the ship takes no hits (cheat, cutscene, capsule shield, parry)
#define P_ARMOUR_    0x8c         // PLAYER: the shield, 0xaaaa full

// ---- lab.c ---------------------------------------------------------------------------------
// A blow from code (a tentacle, an arm): a bullet's hit, or a ram's whole
// capsule. Not while the ship can't be hit, nor within the grace of the
// last; a Spin Drive parry is the caller's to check first. 1 if it landed.
int LabHitShip(int ram);
s32 LabShield(void);              // the ship's shield, 0 .. 0xaaaa
void LabDrainShield(s32 by);      // the aura drinks it (never below 0)
int LabShipDown(void);            // respawning: leave it be

// UNITY's attack boats, squadron Trident (the Kraken's second phase, and the ending)
#define NSUBS 3
void LabSubsIn(void);             // they come up from below, into line
void LabSubsTick(s32 tx, s32 ty, int fire);   // keep station; torpedoes at (tx, ty) if fire
void LabSubsVolley(void);         // every boat, a full spread, now
int LabSubAlive(int k);
void LabSubAt(int k, s32* x, s32* y);
void LabSubHold(int k, s32 x, s32 y);         // an arm has it: it goes where it is dragged
void LabSubKill(int k);
void LabSubsDock(void);           // the ending: to the lab's ports
void LabSubsForget(void);
void LabTorpedoHit(s32 x, s32 y); // lab_boss.c: a UNITY torpedo got there

// The ending: the boarding, the pod, the climb to the surface, the Valhalla.
void LabEndingStart(void);
int LabEnding(void);              // a frame of it: 1 once it has played out

// ---- lab_boss.c ----------------------------------------------------------------------------
void LabBossInit(u32 boss);       // the Lion's Mane, and a new run of the level
void KrakenBossInit(u32 boss);
s32 LabKrakenX(void);             // where the Kraken's mantle is (for the boats' aim)
s32 LabKrakenY(void);
