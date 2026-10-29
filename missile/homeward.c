// homeward.c -- Mission 13 (world 13): the run home, from the Coral Sea to the pack ice.
//
// Straight on from Mission 12: the Valhalla races south for UNITY's HQ under
// the Antarctic ice, Dr. Marrow in her brig and Captain Argus aboard. The
// pilot flies ahead as scout; the Valhalla follows out of sight.
//
//   the opening  the Valhalla low on the screen, only her bow up, while Volta
//                and Argus talk (she's glad he's safe: take a break; he'll feel
//                better scanned and back aboard his own ship -- he hated feeling
//                powerless); the Doc's orders; then she drops back out of sight;
//   the run      Mission 2's own waves (homeward/home_timeline.py), Mk II craft
//                among them; now and then her shells come over the pilot's head
//                from below the screen, into whatever is ahead;
//   the sea      changes four times as the scroll goes south (ShowZone): the
//                Coral Sea's reefs, the open Tasman, the cold Southern Ocean,
//                the pack ice -- the floor fading to the water's colour and back
//                round each change;
//   the bosses   Captain Ignis's turret fortress (fortress.c) and his magma
//                core (inferno.c, which calls the Valhalla up to finish it);
//   the ending   HQ's escorts come up through the ice with the Bastion,
//                repaired; Argus asks leave to go home to his ship.
//
// This file: the cast (its own, and what it borrows from Missions 2 and 9);
// the story's lines by name (Mission 9's machinery, andaman.c, with this
// world's speeches); the sea's zones; the Valhalla's opening and salvos; the
// ending's ships. Run once a frame from the satellites' update (missile.c).
//
// Same rules as missile.c: no libc, no division, engine reached by address.

#define HOMEWARD_LINE_TABLE
#include "andaman.h"
#include "homeward_lines.h"
#include "homeward.h"

#define HW_LoadBadGuy       FN(void (*)(u32, const char*), 0xbf324)     // ARM
#define HW_LoadBullet       FN(void (*)(u32, const char*), 0xbeffc)     // ARM
#define HW_LoadGeneric      FN(void (*)(u32, const char*), 0xbf140)     // ARM
#define HW_SetEntityOpacity FN(void (*)(u32, s32), 0x4f3cd)
#define HW_GetFrameCount    FN(u32 (*)(void), 0x72434)                  // ARM

// Mission 2's craft and its two bosses are the game's own, registered at
// start-up; these are ours: its craft's Mk II, the bosses ported and
// upgraded (homeward/home_cast.py), and Mission 9's ships and effects.
static const char* const kBadguys[] = {
  "*homeward/scripts/badguy02_mk2",
  "*homeward/scripts/badguy03_mk2",
  "*homeward/scripts/badguy04_mk2",
  "*homeward/scripts/badguy05_mk2",
  "*homeward/scripts/badguy06_mk2",
  "*homeward/scripts/badguy09_mk2",
  "*homeward/boss/fort",
  "*homeward/boss/t_bc",
  "*homeward/boss/t_bcl",
  "*homeward/boss/t_bcr",
  "*homeward/boss/t_bl",
  "*homeward/boss/t_br",
  "*homeward/boss/t_hc",
  "*homeward/boss/t_hcl",
  "*homeward/boss/t_hcr",
  "*homeward/boss/t_hl",
  "*homeward/boss/t_hr",
  "*homeward/boss/core",
  "*homeward/boss/arm",
  "*homeward/boss/pod",
};
static const char* const kBullets[] = {
  "*homeward/boss/ember",
  "*homeward/boss/splitter",
  "*homeward/boss/flak",
  "*homeward/boss/arrow_special",
  "*homeward/boss/disk_special",
  "*homeward/boss/disk_circle",
  "*homeward/boss/disk_tourelle_bc",
  "*homeward/boss/disk_tourelle_hc",
  "*homeward/boss/target",
  "*homeward/boss/virgule",
  "*homeward/boss/wait",
  "*homeward/boss/wide",
};
static const char* const kGenerics[] = {
  "*homeward/boss/mboom",
  "*homeward/boss/deadturret",
  "*homeward/boss/bigdeadturret",
  "*homeward/boss/cboom",
  "*homeward/boss/boomsat",
  "*homeward/boss/armboom",
  "*homeward/boss/geyser",             // the magma geysers the core opens round the ship (inferno.c)
  "*homeward/boss/core_vis",           // the Mk II's look, drawn over its core and pods, and its lance
  "*homeward/boss/pod_vis",
  "*homeward/boss/lance",
  "*homeward/boss/eyeglow",
  // UNITY's ships, the Valhalla, their shells and blasts (Mission 9's)
  "*andaman/boss/crosshair",
  "*andaman/boss/frigate",
  "*andaman/boss/nturret",
  "*andaman/boss/wake",
  "*andaman/boss/tracer",
  "*andaman/boss/bastion",
  "*andaman/boss/bturret",
  "*andaman/boss/ciws",
  "*andaman/boss/fire",
  "*andaman/boss/blast",
  "*andaman/boss/lance",
  "*andaman/boss/valhalla",
  "*andaman/boss/valhalla_jets",
  "*andaman/boss/val_fans_l",
  "*andaman/boss/val_fans_r",
  "*andaman/boss/valhalla_shadow",
  "*andaman/boss/vturret",
  "*andaman/boss/vrailgun",
  "*andaman/boss/vescort",
  "*andaman/boss/vescort_shadow",
  "*andaman/boss/vesc_patch",
  "*andaman/boss/vesc_turret",
};

#define COUNT(a) (sizeof(a) / sizeof((a)[0]))

void RegisterHomeward(void) {
  for (u32 i = 0; i < COUNT(kBullets); i++) HW_LoadBullet(BH, kBullets[i]);
  for (u32 i = 0; i < COUNT(kGenerics); i++) HW_LoadGeneric(BH, kGenerics[i]);
  for (u32 i = 0; i < COUNT(kBadguys); i++) HW_LoadBadGuy(BH, kBadguys[i]);
}

void ForgetDefinitions(const char* const* badguys, u32 nb, const char* const* bullets, u32 nu,
                       const char* const* generics, u32 ng);
void ForgetHomeward(void) {
  ForgetDefinitions(kBadguys, COUNT(kBadguys), kBullets, COUNT(kBullets), kGenerics, COUNT(kGenerics));
}

// ---- the state -------------------------------------------------------------------------------
enum { SAID_OPEN = 1, SAID_DOC = 2, SAID_COLD = 4, SAID_ICE = 8, FELL_BACK = 16, SAID_SALVO = 32 };
enum { E_OFF, E_WAIT, E_SAIL, E_TALK, E_DONE };
enum { SV_OFF, SV_RISE, SV_FIRE };

static struct {
  int on, said, t;
  int salvo;                        // the next of kSalvoT
  int sv, sv_t;                     // the salvo in hand: SV_*, and frames into it
  // the sea: the zone shown, the one coming, the fade between
  int zone, next_zone, fade_t;
  int have_tex;
  u32 tex[H_LOOKS * H_ZONE_FRAMES];
  s32 opacity;
  u32 ticked;                       // the frame HomeTick last ran (playing, not a menu)
  // the ending: HQ's escorts and the Bastion
  int ending, end_t, end_line;
  AnShip fr[2];
  u32 bastion, btur[2], ciws[2];
  s32 bx, by;
} H;

void HomeClearSea(void) {
  for (u32 b = SB_GetFirstBadGuy(BH); b; b = SB_GetNextBadGuy(BH, b))
    if (SB_GetBadGuyHealth(b) > 0) {
      SB_SetActorPosition(Act(b), FX(-400), FX(-400));
      Remove(b);
    }
}

// ---- the sea: four zones, one at a time -------------------------------------------------------
// The floor is one looping animation of H_ZONES x H_ZONE_FRAMES textures
// (homeward/art_home_sea.py); the engine's BACKGROUND record (SHOGUN+0x58)
// plays BG_COUNT of the handles in BG_TEX, and ours gives it one zone's loop
// at a time (abyss.c's way). A change: the floor's six entities fade to the
// water's colour (the screen's clear colour, HomeClearColour) over FADE
// frames, the next zone's loop goes in, and it fades back up.
#define SH_BG_COUNT  0x8c
#define SH_BG_TEX    0x90
#define FLOOR_ENT    0x638
#define FADE         40

static void FloorOpacity(s32 op) {
  if (op == H.opacity) return;
  H.opacity = op;
  for (u32 e = FLOOR_ENT; e < FLOOR_ENT + 6; e++) HW_SetEntityOpacity(e, op);
}

static void ShowZone(int z) {
  const u32 sh = Shogun();
  const int ntex = H_LOOKS * H_ZONE_FRAMES;
  if (!H.have_tex) {
    if (R32(sh, SH_BG_COUNT) != ntex) return;   // (not our world file: leave it)
    for (int k = 0; k < ntex; k++) H.tex[k] = (u32)R32(sh, SH_BG_TEX + 4 * k);
    H.have_tex = 1;
  }
  const int look = kZoneLook[z];
  for (int k = 0; k < H_ZONE_FRAMES; k++) R32(sh, SH_BG_TEX + 4 * k) = (s32)H.tex[look * H_ZONE_FRAMES + k];
  R32(sh, SH_BG_COUNT) = H_ZONE_FRAMES;
  H.zone = z;
}

// The next sea comes in as a band of flat props (homeward/make_homeward.py)
// scrolling down over the old; kZoneRow is the scroll at which that band
// fills the screen, and the floor underneath is swapped then, unseen.
static void Sea(void) {
  if (!H.have_tex) {
    ShowZone(0);
    FloorOpacity(0x10000);
  }
  const s32 row = AnScroll();
  int want = H.zone;
  while (want + 1 < H_ZONES && row >= kZoneRow[want + 1]) want++;
  if (want != H.zone) ShowZone(want);
}

// abyss.c's hook on onRender's `bl UE_ClearScreen(rgb565)` asks here first:
// while Mission 13 plays, the water's colour (the zone's, toward the next
// one's as the floor fades) rather than black.
int HomeClearColour(u32* colour) {
  if (!H.on || R32(Shogun(), SH_WORLD_NO) != HOMEWARD || HW_GetFrameCount() - H.ticked > 4) return 0;
  const unsigned char* a = kZoneWater[kZoneLook[H.zone]];
  s32 r = a[0], g = a[1], b = a[2];
  if (H.fade_t && H.fade_t < FADE) {
    const unsigned char* n = kZoneWater[kZoneLook[H.next_zone]];
    const s32 f = H.fade_t * (0x10000 / FADE);
    r += SB_MulFP(n[0] - r, f);
    g += SB_MulFP(n[1] - g, f);
    b += SB_MulFP(n[2] - b, f);
  }
  *colour = ((u32)(r >> 3) << 11) | ((u32)(g >> 2) << 5) | (u32)(b >> 3);
  return 1;
}

// ---- the Valhalla, out of sight behind the pilot -----------------------------------------------
// A salvo: the Valhalla comes up behind the pilot until her bow is on the
// screen, her turrets fire on whatever is ahead (andaman.c: each picks its
// own), her railgun on the biggest of it, and she drops back out of sight.
#define SV_HOLD   260                // frames on station, at the least
#define SV_MAX    520                // ... and at the most (her railgun found nothing)

// The toughest craft on the screen, ahead of the ship (worth her railgun), or 0.
static u32 Biggest(void) {
  u32 best = 0;
  s32 bh = 300;
  for (u32 b = SB_GetFirstBadGuy(BH); b; b = SB_GetNextBadGuy(BH, b)) {
    const s32 hp = SB_GetBadGuyHealth(b);
    if (hp <= bh) continue;
    s32 x, y;
    SB_GetActorPosition(Act(b), &x, &y);
    if (y < FX(30) || y > FX(260) || x < FX(20) || x > FX(380)) continue;
    bh = hp;
    best = b;
  }
  return best;
}

static void Salvo(s32 pt) {
  switch (H.sv) {
  case SV_OFF:
    if (H.salvo < H_NSALVO && pt >= kSalvoT[H.salvo] && AnValGone()) {
      H.salvo++;
      H.sv = SV_RISE;
      H.sv_t = 0;
      AnValCome();
      if (!(H.said & SAID_SALVO)) {
        H.said |= SAID_SALVO;
        AnLine(HL_VOLTA_SALVO);
      }
    }
    break;
  case SV_RISE:
    if (AnValThere() || ++H.sv_t > 400) {
      H.sv = SV_FIRE;
      H.sv_t = 0;
      const u32 big = Biggest();
      if (big) AnValRailOn(big, 0);
    }
    break;
  default:                            // SV_FIRE
    if (++H.sv_t > SV_MAX || (H.sv_t > SV_HOLD && AnValRailDone())) {
      AnValSink();
      H.sv = SV_OFF;
    }
    break;
  }
}

// ---- the ending: HQ's escorts and the Bastion, waiting up ahead ---------------------------------
// They sail slowly for home ahead of the Valhalla, so she overtakes them:
// they come down the screen from the top into place above her bow, all
// facing the way home (the Bastion's sprite doesn't turn).
#define END_BASTION_Y  150
#define END_FRIGATE_Y  205
#define END_FRIGATE_X  95
#define END_FROM       FX(-260)
// HQ's ships come down the screen to meet the Valhalla, bows first (on their way
// back to her): the Bastion's art turned half round, its guns' places with it

static void EndPlace(void) {
  H.by += Ease(FX(END_BASTION_Y) - H.by, FX(1.0));
  AnPut(H.bastion, H.bx + AnGroundDx(H.by), H.by);
  AnFrame(H.bastion, 0);
  AnTurn(H.bastion, TURN / 2);
  for (int k = 0; k < 2; k++) {
    AnPut(H.btur[k], H.bx + AnGroundDx(H.by) - FX(k ? BASTION_TUR1_X : BASTION_TUR0_X),
          H.by - FX(k ? BASTION_TUR1_Y : BASTION_TUR0_Y));
    AnTurn(H.btur[k], DOWN);
    AnPut(H.ciws[k], H.bx + AnGroundDx(H.by) - FX(k ? BASTION_CIWS1_X : BASTION_CIWS0_X),
          H.by - FX(k ? BASTION_CIWS1_Y : BASTION_CIWS0_Y));
    AnTurn(H.ciws[k], DOWN);
  }
  for (int i = 0; i < 2; i++) {
    AnShip* s = &H.fr[i];
    if (!s->hull) continue;
    s->y += Ease(FX(END_FRIGATE_Y) - s->y, FX(1.0));
    ShipPlace(s);
  }
}

void HomeEnding(void) {
  if (H.ending) return;
  H.ending = E_WAIT;                  // (the Valhalla drops back first: inferno.c)
  H.end_t = 0;
}

static void EndSpawn(void) {
  H.bx = FX(200);
  H.by = END_FROM;
  for (int i = 0; i < 2; i++)
    ShipSpawn(&H.fr[i], SHIP_FRIGATE, FX(i ? 400 - END_FRIGATE_X : END_FRIGATE_X), END_FROM - FX(40), DOWN);
  H.bastion = AnSpawn("*andaman/boss/bastion", H.bx, H.by);
  for (int k = 0; k < 2; k++) {
    H.btur[k] = AnSpawn("*andaman/boss/bturret", H.bx, H.by);
    H.ciws[k] = AnSpawn("*andaman/boss/ciws", H.bx, H.by);
  }
  EndPlace();
}

int HomeEndingDone(void) { return H.ending == E_DONE; }

static void Ending(void) {
  H.end_t++;
  if (H.ending == E_WAIT) {
    // the ships come once the Valhalla is out of sight behind (she flies; they sail)
    if (!AnValGone() && H.end_t < 480) return;
    EndSpawn();
    H.ending = E_SAIL;
    H.end_t = 0;
  }
  EndPlace();
  switch (H.ending) {
  case E_SAIL:
    if (H.end_t == 60) AnLine(HL_DOC_ESCORT);
    if (H.end_t > 200 && AnAbs(FX(END_BASTION_Y) - H.by) < FX(4)) {
      H.ending = E_TALK;
      AnLine(HL_ARGUS_BASTION);
      H.end_line = AnLine(HL_VOLTA_GRANTED);
    }
    break;
  case E_TALK:
    if (AnLineUp(H.end_line) && AnQuiet()) H.ending = E_DONE;
    break;
  }
}

// ---- a new run, and every frame ------------------------------------------------------------------
void AndamanReset(void);

void HomeReset(void) {
  AndamanReset();                              // the shells, blasts and the Valhalla: clean
  AnUseLines(kHomeSentences, HL_COUNT);        // ... with this world's speeches
  SB_ClearBytes(&H, sizeof H);
  H.on = 1;
  H.opacity = -1;
}

void HomewardTick(void) {
  if (R32(Shogun(), SH_WORLD_NO) != HOMEWARD || !H.on) return;
  H.ticked = HW_GetFrameCount();
  H.t++;
  const s32 pt = AnTime();
  Sea();
  // the opening: the Valhalla's bow low on the screen while Volta and Argus talk
  if (!(H.said & SAID_OPEN) && pt >= H_OPEN_VOLTA_T) {
    H.said |= SAID_OPEN;
    AnValCome();
    AnLine(HL_VOLTA_RELIEF);
    AnLine(HL_ARGUS_SCAN);
    AnLine(HL_ARGUS_POWERLESS);
    AnLine(HL_VOLTA_HOME);
  }
  if (!(H.said & SAID_DOC) && pt >= H_OPEN_DOC_T) {
    H.said |= SAID_DOC;
    AnLine(HL_DOC_SCOUT);
    AnLine(HL_VOLTA_BEHIND);
  }
  if (!(H.said & FELL_BACK) && pt >= H_FALL_BACK_T) {
    H.said |= FELL_BACK;
    AnValSink();
  }
  if (!(H.said & SAID_COLD) && pt >= H_COLD_T) { H.said |= SAID_COLD; AnLine(HL_ARGUS_COLD); }
  if (!(H.said & SAID_ICE) && pt >= H_ICE_T) { H.said |= SAID_ICE; AnLine(HL_VOLTA_ICE); }
  if (!R32(Shogun() + 0x4e78, B_STATE)) Salvo(pt);   // (not while a boss is up ...
  else if (H.sv != SV_OFF) {                          // ... and away if one comes)
    AnValSink();
    H.sv = SV_OFF;
  }
  if (H.ending) Ending();
}
