// homeward.h -- what Mission 13's three files share (homeward.c, fortress.c, inferno.c).
#pragma once

void HomeReset(void);               // a new run (fortress.c's init calls it)
void HomeEnding(void);              // inferno.c: Ignis is down -- the escorts, the Bastion, home
int HomeEndingDone(void);           // ... and the last line has gone up: the level may end
void HomeClearSea(void);            // every badguy still out removed (a scene is played over it)
void FortBossInit(u32 boss);        // fortress.c: Captain Ignis's fortress (worlds.c's mid-boss for world 13)
void InfernoBossInit(u32 boss);     // inferno.c: his magma core (the fortress installs it)
