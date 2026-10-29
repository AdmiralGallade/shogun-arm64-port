// sunda.h -- what Mission 10's two files share (sunda.c, coelacanth.c).
#pragma once

void SundaReset(void);              // a new run (coelacanth.c's init calls it)
void SundaFleetPos(s32* x, s32* y); // the Bastion's centre, on screen
void SundaBridge(s32* x, s32* y);   // ... her bridge, where Argus stands
void SundaFleetTo(s32 y);           // the Bastion eased to this y (her frigate with her)
int SundaFleetSettled(void);        // ... and is there
void SundaFleetGuns(int on);       // the fleet fires on what comes up
void SundaFleetShellAt(s32 x, s32 y, int on);   // the Bastion's main guns on a point (the boarding)
void SundaSinkPike(s32 x0, s32 y0); // a spread from (x0, y0) sinks the Pike
int SundaPikeDown(void);            // ... it has struck
void SundaClearSea(u32 spare);      // every badguy still out removed (but `spare`)
void CoelBossInit(u32 boss);        // coelacanth.c: Specimen Nine (worlds.c's mid-boss for world 10)
