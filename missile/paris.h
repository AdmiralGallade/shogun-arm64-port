// paris.h -- what Mission 15's two files share (paris.c, flagship.c).
#pragma once

#define PARIS 15                  // SHOGUN+0x4e6c

void ParisReset(void);            // a new run (flagship.c's init calls it)
int ParisLine(int k);             // say a line (andaman.c's AnLine with this world's speeches)
void ParisRout(void);             // the carrier is down: the legion breaks (every craft blown)
void ParisFinale(void);           // ... the last lines; done when ParisFinaleDone()
int ParisFinaleDone(void);
void FlagshipBossInstall(u32 boss);  // flagship.c: the Kagutsuchi, for the next boss event
void TankBossInit(u32 boss);      // tank.c: the Kanabo (worlds.c's mid-boss for world 15; and a new run)
void ParisValWipe(int frames);    // the Valhalla's guns clear the screen while she's here
int ParisBreachSite(void);        // the Kanabo's target: the frontmost generator standing (or a turret)
void ParisSitePos(int k, s32* x, s32* y);
void ParisSiteHit(int k, int dmg);   // a breaching shot lands on site k
void ParisScrollAhead(int on);    // the stopped scroll eased forward (the Kanabo), or back
void ParisCover(s32 x0, s32 y0, s32 x1, s32 y1);
void ParisCoverOutline(s32 cx, s32 cy0, const unsigned char* half, int n);   // ... by its outline   // a boss hull is over this box: what's under it hidden
void ParisTankDone(void);         // the Kanabo is gone: the defence's event lines may come
