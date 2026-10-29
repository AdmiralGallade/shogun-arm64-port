// lab.c -- Mission 12 (world 12): the raid on Dr. Marrow's laboratory.
//
// After Mission 11 (abyss.c): the Forge's relays gave up Captain Argus's
// tracker, and it leads to Dr. Marrow's lab on the abyssal plain, where she
// is emptying his mind drive of UNITY's codes for the Shogun. The pilot
// goes down in the Valhalla's prototype, by the Trench's rules (abyss.c runs
// them for world 12 too: the submersible, no H or M, no entropy, the Spin
// Drive, armour by weapon, the dive through the zones):
//
//   the approach  the midnight zone; Volta from the Valhalla, the Doc on
//                 Marrow's uplink (thirty percent, fifty-five, eighty...),
//                 Marrow sure her lab holds long enough;
//   the boneyard  where she dumps her failures; the Lion's Mane (lab_boss.c);
//   the trench    Argus, through his own tracker: if he breaks, the protocol
//                 stands -- whatever it takes;
//   the outworks  the lab's plated ground; its intakes drag the ship up the
//                 screen (Intake);
//   the lab       the Kraken (lab_boss.c), and UNITY's squadron Trident
//                 comes to help: its boats (here: LabSubs*) torpedo the
//                 Kraken, and when it dies they dock and board;
//   the ending    Marrow flees in her escape pod with Argus; the pilot climbs
//                 after it to the surface (the floor fades away under the
//                 ship, the water lightens, the snow streams down); the pod
//                 is flying; the Valhalla rises, its railgun on the pod --
//                 and fires grapples instead. Boarded. (LabEnding)
//
// Never said, only shown: the Doc doesn't want Marrow dead, and Volta wants
// Argus back though he'd rather she shot him.
//
// Same rules as missile.c: no libc, no division, engine reached by address.

#define LAB_LINE_TABLE
#include "lab.h"
#include "andaman_glow.h"                // the Valhalla's lift fans

#define LR_LoadBadGuy       FN(void (*)(u32, const char*), 0xbf324)       // ARM
#define LR_LoadBullet       FN(void (*)(u32, const char*), 0xbeffc)       // ARM
#define LR_LoadGeneric      FN(void (*)(u32, const char*), 0xbf140)       // ARM

static const char* const kBadguys[] = {
  "*lab/scripts/lantern",
  "*lab/scripts/lantern_l",
  "*lab/scripts/lantern_r",
  "*lab/scripts/minisub",
  "*lab/scripts/torpedo",
  "*lab/scripts/jellymine",
  "*lab/scripts/nautilus",
  "*lab/scripts/bathysphere",
  "*lab/scripts/buoy",
  "*lab/scripts/manta",
  "*lab/scripts/manta_l",
  "*lab/scripts/manta_r",
  "*lab/scripts/salvage",
  "*lab/scripts/eel",
  "*lab/scripts/eel_l",
  "*lab/scripts/eel_r",
  "*lab/scripts/polyp",
  "*lab/scripts/squid",
  "*lab/scripts/nine_lantern",      // Specimen Nine's, in the Kraken's third phase
  "*lab/scripts/lantern_x",         // hard mode's elites: the Trench's (abyss_cast.py) ...
  "*lab/scripts/lantern_l_x",
  "*lab/scripts/lantern_r_x",
  "*lab/scripts/minisub_x",
  "*lab/scripts/jellymine_x",
  "*lab/scripts/nautilus_x",
  "*lab/scripts/bathysphere_x",
  "*lab/scripts/buoy_x",
  "*lab/scripts/manta_x",
  "*lab/scripts/manta_l_x",
  "*lab/scripts/manta_r_x",
  "*lab/scripts/eel_x",             // ... and Marrow's own (lab_cast.py, docs/HARD_MODE.md)
  "*lab/scripts/eel_l_x",
  "*lab/scripts/eel_r_x",
  "*lab/scripts/polyp_x",
  "*lab/scripts/squid_x",
  "*lab/boss/bell",
  "*lab/boss/emitter",
  "*lab/boss/mantle",
  "*lab/boss/mantle_shell",         // its stand-in while its eyes are open (lab_cast.py)
  "*lab/boss/club",
  "*lab/boss/siphon",
  "*lab/boss/kgun",
  "*lab/boss/keye_l",               // the Kraken's eyes: laser, coilgun
  "*lab/boss/keye_r",
  "*lab/boss/glantern",             // Specimen Nine's great lure lanterns
  "*lab/boss/eyelet",               // the ring of little eyes round an open eye
};
static const char* const kBullets[] = {
  "*lab/boss/bubble",
  "*lab/boss/ping",
  "*lab/boss/spine",
  "*lab/boss/wave",
  "*lab/boss/spark",
  "*lab/boss/sting",
  "*lab/boss/ink",
  "*lab/boss/hook",
  "*lab/boss/curl",
  "*lab/boss/glare",                // the Kraken's eyes' (lab_cast.py)
  "*lab/boss/iris",
  "*lab/boss/tear",
  "*lab/boss/drift",                // the Trench's, for the snake and the mines
  "*lab/boss/snipe",
  "*lab/boss/charge",
  "*lab/boss/seed",                 // the Trench's mob attacks (abyss_cast.py)
  "*lab/boss/hesitate",
  "*lab/boss/jam",                  // hard mode's jammed ping (the Trench's lanternfish elites)
  "*lab/boss/h_comma",              // hard mode's two-stage bullets (beach/hardkit.py)
  "*lab/boss/h_twist",
  "*lab/boss/h_snipe",
  "*lab/boss/h_lock",
  "*lab/boss/h_drift",
  "*lab/boss/h_hesitate",
  "*lab/boss/h_needle",
  "*lab/boss/h_sig",                 // its signature pair (hardkit.THEMES)
  "*lab/boss/h_sigb",
};
static const char* const kGenerics[] = {
  "*lab/boss/kraken_brain",          // the Kraken's brain at work (glow.h)
  "*lab/boss/bubbles",
  "*lab/boss/spore",
  "*lab/boss/boom",
  "*lab/boss/ray",
  "*lab/boss/snow",
  "*lab/boss/blast",
  "*lab/boss/aura",
  "*lab/boss/warn",
  "*lab/boss/drain",
  "*lab/boss/mane_seg",
  "*lab/boss/mane_tip",
  "*lab/boss/kseg",
  "*lab/boss/trident",
  "*lab/boss/ftorp",
  "*lab/boss/escape_pod",
  "*lab/boss/grapple",
  "*lab/boss/glint",
  "*lab/boss/link",
  "*lab/boss/valhalla",
  "*lab/boss/valhalla_jets",
  "*lab/boss/val_fans_l",
  "*lab/boss/val_fans_r",
  "*lab/boss/valhalla_shadow",
  "*lab/boss/vturret",
  "*lab/boss/vrailgun",
  "*lab/boss/crosshair",
  "*lab/boss/kturret",
  "*lab/boss/kmortar",
  "*lab/boss/kgills",
  "*lab/boss/kfin",
  "*lab/boss/kfin_r",
  "*lab/boss/flame",
  "*lab/boss/keye_vis",             // the Kraken's eyes and pupils
  "*lab/boss/kpupil",
  "*lab/boss/nine",                 // Specimen Nine and her lure lanterns (borrowed from the Sunda Trench)
  "*lab/boss/nine_lure",
};

#define COUNT(a) (sizeof(a) / sizeof((a)[0]))

// Before the world file, which binds every script's sprites (beach.c).
void RegisterLab(void) {
  for (u32 i = 0; i < COUNT(kBullets); i++) LR_LoadBullet(BH, kBullets[i]);
  for (u32 i = 0; i < COUNT(kGenerics); i++) LR_LoadGeneric(BH, kGenerics[i]);
  for (u32 i = 0; i < COUNT(kBadguys); i++) LR_LoadBadGuy(BH, kBadguys[i]);
}

void ForgetDefinitions(const char* const* badguys, u32 nb, const char* const* bullets, u32 nu,
                       const char* const* generics, u32 ng);
void ForgetLab(void) {
  ForgetDefinitions(kBadguys, COUNT(kBadguys), kBullets, COUNT(kBullets), kGenerics, COUNT(kGenerics));
}

// ---- the ship, struck from code -----------------------------------------------------------------
static int g_grace;

static u32 Player_(void) { return Shogun() + SH_PLAYER; }

int LabShipDown(void) {
  const u32 sh = Shogun();
  return *(volatile unsigned short*)(sh + SH_HITLOCK) || R32(sh, SH_HITLOCK2);
}

int LabHitShip(int ram) {
  const u32 sh = Shogun();
  if (*(volatile unsigned short*)(sh + SH_GODFLAG) || LabShipDown() || g_grace > 0) return 0;
  SB_StrikePlayer(sh + SH_PLAYER, ram ? 1 : 0, sh + SH_HUD);
  SB_ShakeWorld();
  g_grace = 60;
  return 1;
}

s32 LabShield(void) { return R32(Player_(), P_ARMOUR_); }

void LabDrainShield(s32 by) {
  const u32 p = Player_();
  s32 a = R32(p, P_ARMOUR_) - by;
  if (a < 0) a = 0;
  R32(p, P_ARMOUR_) = a;
}

// ---- the story, by the clock -----------------------------------------------------------------------
static struct {
  u32 said;
  u32 t;
  int intake;
} L;
enum { S_BRIEF = 1, S_SPIN = 2, S_ELARA = 4, S_WHO = 8, S_ARGUS = 16, S_X55 = 32, S_INTAKE = 64, S_X80 = 128,
       S_LAB = 256 };

static int Once(u32 bit) {
  if (L.said & bit) return 0;
  L.said |= bit;
  return 1;
}

// abyss.c, at each descent
void LabDescent(int k) {
  if (k == 0) {
    AbLine(LB_DIVE_1);              // the Doc: bones down there
    AbLine(LB_BONES);               // Marrow: her rejects
  } else if (k == 1) {
    AbLine(LB_DIVE_2);
  } else if (k == 2) {
    AbLine(LB_DIVE_3);
  } else {
    AbLine(LB_DIVE_4);              // the Doc: the deck ends at a chasm
  }
}

// The first sonar buoy on screen: the Spin Drive's reminder (queued: it
// mustn't step over the brief).
static void Watch(void) {
  if (L.said & S_SPIN) return;
  for (u32 b = SB_GetFirstBadGuy(BH); b; b = SB_GetNextBadGuy(BH, b)) {
    if (SB_GetBadGuyHealth(b) <= 0 || (GetReg(b, 21) >> 16) != MARK_BUOY) continue;
    s32 x, y;
    SB_GetActorPosition(Act(b), &x, &y);
    if (y < FX(10)) continue;
    if (Once(S_SPIN)) AbLine(LB_SPIN);
    return;
  }
}

// The lab's intakes: the water drawn toward it, and the ship up the screen.
static void Intake(s32 pt) {
  if (pt < LAB_INTAKE_0 || pt > LAB_INTAKE_1) {
    if (L.intake) { L.intake = 0; AbStream(0); }
    return;
  }
  L.intake = 1;
  s32 k = FX(1);
  if (pt - LAB_INTAKE_0 < 120) k = (pt - LAB_INTAKE_0) * 546;
  if (LAB_INTAKE_1 - pt < 120) k = (LAB_INTAKE_1 - pt) * 546;
  const s32 gust = FX(1) + (SB_Sin((pt * 0x50000) & (TURN - 1)) >> 1);
  const s32 pull = SB_MulFP(SB_MulFP(FX(0.55), k), gust);
  AbStream(-SB_MulFP(FX(1.2), k));
  if (!LabShipDown()) {
    const u32 p = Player_();
    R32(p, 0x1c) = R32(p, 0x1c) - pull;
  }
}

void LabTick(s32 pt) {
  L.t++;
  if (g_grace > 0) g_grace--;
  if (pt >= LAB_BRIEF_T && Once(S_BRIEF)) {
    AbLine(LB_BRIEF);               // Volta: the tracker, the lab dead ahead
    AbLine(LB_EXTRACT_30);          // the Doc: her uplink, thirty percent
    AbLine(LB_MARROW_HELLO);        // Marrow: her babies will keep you busy
  }
  if (pt >= LAB_ELARA_T && Once(S_ELARA)) {
    AbLine(LB_ELARA);               // the Doc, by a name nobody else uses
    AbLine(LB_NOT_ELARA);
  }
  if (pt >= LAB_WHO_T && Once(S_WHO)) {
    AbLine(LB_WHO);                 // Volta noticed
    AbLine(LB_LATER);
  }
  if (pt >= LAB_ARGUS_T && Once(S_ARGUS)) {
    AbLine(LB_ARGUS_CALL);          // Argus, through his tracker
    AbLine(LB_VOLTA_ARGUS);
    AbLine(LB_ARGUS_ASK);           // ... whatever it takes
    AbLine(LB_VOLTA_NO);
  }
  if (pt >= LAB_X55_T && Once(S_X55)) AbLine(LB_EXTRACT_55);
  if (pt >= LAB_INTAKE_0 && Once(S_INTAKE)) AbLine(LB_INTAKE);
  if (pt >= LAB_X80_T && Once(S_X80)) AbLine(LB_EXTRACT_80);
  if (pt >= LAB_STOP_T && Once(S_LAB)) AbLine(LB_LAB);
  Watch();
  Intake(pt);
}

// ---- UNITY's squadron Trident -------------------------------------------------------------------
#define NFTORP 14
#define SUB_Y  FX(372)
static struct {
  u32 a;
  s32 x, y, tx, ty;
  int alive, held, cool, burst;
} SUB[NSUBS];
static struct { u32 a; s32 x, y, vx, vy, sp, gx, gy; int t; } FT[NFTORP];
static int g_subs_on, g_docking;

static void Torp(int k, s32 tx, s32 ty) {
  int i = 0;
  while (i < NFTORP && FT[i].a) i++;
  if (i == NFTORP) return;
  const s32 x = SUB[k].x, y = SUB[k].y - FX(50);
  FT[i].a = AbSpawn("*lab/boss/ftorp", x, y);
  if (!FT[i].a) return;
  FT[i].x = x;
  FT[i].y = y;
  FT[i].gx = tx + FX(Rnd(-30, 30));
  FT[i].gy = ty + FX(Rnd(-10, 20));
  FT[i].sp = FX(1.5);
  FT[i].t = 0;
  const s32 a = AbAngle(x, y, FT[i].gx, FT[i].gy);
  FT[i].vx = SB_Cos(a);
  FT[i].vy = SB_Sin(a);
  SB_SetActorOrient(FT[i].a, a);
}

static void Torps(void) {
  for (int i = 0; i < NFTORP; i++) {
    if (!FT[i].a) continue;
    FT[i].t++;
    if (FT[i].sp < FX(5)) FT[i].sp += FX(0.12);
    // homing a little on the point it was sent to
    const s32 a = AbAngle(FT[i].x, FT[i].y, FT[i].gx, FT[i].gy);
    FT[i].x += SB_MulFP(SB_Cos(a), FT[i].sp);
    FT[i].y += SB_MulFP(SB_Sin(a), FT[i].sp);
    SB_SetActorOrient(FT[i].a, a);
    AbPut(FT[i].a, FT[i].x, FT[i].y);
    if ((FT[i].t & 3) == 0) SB_SpawnGeneric(BH, "*lab/boss/bubbles", FT[i].x, FT[i].y + FX(8));
    if (AbDist(FT[i].gx - FT[i].x, FT[i].gy - FT[i].y) < 10 || FT[i].t > 240) {
      AbBoom(FT[i].x, FT[i].y, 0);
      LabTorpedoHit(FT[i].x, FT[i].y);
      AbDrop(&FT[i].a);
    }
  }
}

void LabSubsIn(void) {
  for (int k = 0; k < NSUBS; k++) {
    SUB[k].x = FX(100 + 100 * k);
    SUB[k].y = FX(500 + 30 * k);
    SUB[k].tx = SUB[k].x;
    SUB[k].ty = SUB_Y + (k == 1 ? FX(14) : 0);
    SUB[k].a = AbSpawn("*lab/boss/trident", SUB[k].x, SUB[k].y);
    SUB[k].alive = 1;
    SUB[k].held = 0;
    SUB[k].cool = 120 + 50 * k;
    SUB[k].burst = 0;
  }
  g_subs_on = 1;
}

int LabSubAlive(int k) { return g_subs_on && SUB[k].alive; }
void LabSubAt(int k, s32* x, s32* y) { *x = SUB[k].x; *y = SUB[k].y; }
void LabSubHold(int k, s32 x, s32 y) { SUB[k].held = 1; SUB[k].x = x; SUB[k].y = y; }

void LabSubKill(int k) {
  if (!SUB[k].alive) return;
  SUB[k].alive = 0;
  AbBoom(SUB[k].x, SUB[k].y, 1);
  AbBoom(SUB[k].x, SUB[k].y - FX(30), 0);
  AbDrop(&SUB[k].a);
}

void LabSubsVolley(void) {
  for (int k = 0; k < NSUBS; k++)
    if (SUB[k].alive) SUB[k].burst = 3;
}

// The ending: to the lab's ports (lab_timeline.LAB_Y, its middle on screen)
static const short kPortX[NSUBS] = { 116, 200, 284 };
static const short kPortY[NSUBS] = { 18, 52, 18 };       // below the lab's middle
void LabSubsDock(void) { g_docking = 1; }

void LabSubsTick(s32 tx, s32 ty, int fire) {
  if (!g_subs_on) return;
  for (int k = 0; k < NSUBS; k++) {
    if (!SUB[k].alive) continue;
    if (g_docking == 2) {                        // the climb: they fall away below with the lab
      SUB[k].y += FX(2.2);
      AbPut(SUB[k].a, SUB[k].x, SUB[k].y);
      if (SUB[k].y > FX(500)) { SUB[k].alive = 0; AbDrop(&SUB[k].a); }
      continue;
    }
    if (g_docking) {
      SUB[k].tx = FX(kPortX[k]);
      SUB[k].ty = FX(LAB_Y + kPortY[k]);
    } else if (!SUB[k].held) {
      // keep station, rocking a little, each its own way
      SUB[k].tx = FX(100 + 100 * k) + SB_Sin(((s32)L.t * 0x9000 + k * 0x500000) & (TURN - 1)) * 10;
    }
    if (!SUB[k].held) {
      SUB[k].x += Ease(SUB[k].tx - SUB[k].x, FX(g_docking ? 1.4 : 2.0));
      SUB[k].y += Ease(SUB[k].ty - SUB[k].y, FX(g_docking ? 1.4 : 2.0));
    }
    AbPut(SUB[k].a, SUB[k].x, SUB[k].y);
    if (!fire || SUB[k].held || g_docking || SUB[k].y > FX(420)) continue;
    if (SUB[k].burst > 0) {
      if (((s32)L.t & 7) == 0) { Torp(k, tx, ty); SUB[k].burst--; }
      continue;
    }
    if (--SUB[k].cool > 0) continue;
    SUB[k].cool = Rnd(150, 230);
    Torp(k, tx, ty);
  }
  Torps();
}

void LabSubsForget(void) {
  for (int k = 0; k < NSUBS; k++) AbDrop(&SUB[k].a);
  for (int i = 0; i < NFTORP; i++) AbDrop(&FT[i].a);
  SB_ClearBytes(SUB, sizeof SUB);
  SB_ClearBytes(FT, sizeof FT);
  g_subs_on = g_docking = 0;
}

// ---- the ending ------------------------------------------------------------------------------------
enum { E_OFF, E_DOCK, E_LAUNCH, E_RISE, E_BREACH, E_VALHALLA, E_AIM, E_GRAPPLE, E_REEL, E_TALK, E_LEAVE, E_DONE };
extern int g_weapons_jammed;      // missile.c: the trigger held off (Mission 10's boarding; the cutscene here)
#define NLINK   9
#define NGLINT  7
#define RISE_T  320
#define SH_SCROLL 0x434           // the props' scroll, 16.16 rows
#define BOW_REST FX(340)          // her bow half's centre, on station (her own centre 120 below)
static const unsigned char kSurface[3] = { 18, 74, 116 };

// The Valhalla as the Andaman Islands build her (andaman.c VPlace, andaman_lines.h
// VAL_*): her bow and stern halves, her jets astern, her shadow on the sea,
// six turrets, the railgun on her bow -- all from her centre.
#define VAL_HALF 240
static const short kVTurX[6] = { 0, 0, 0, 0, -80, 80 };
static const short kVTurY[6] = { -125, -20, 130, 168, -8, -8 };
#define VAL_RAIL_DY  (-75)
#define VAL_JETS_DY  257
#define UP_          0xc00000
enum { XH_HUNT, XH_LOCKED, XH_GONE };   // the crosshair's animations
enum { RAIL_COLD, RAIL_CHARGED };

static struct {
  int phase, t, ticket;
  u32 pod, bow, cross, grap[2], link[2][NLINK], glint[NGLINT];
  u32 stern, jets, shadow, fans[2], tur[6], rail;
  s32 podx, pody, podv, bowy, gx[2], gy[2], rail_ang;
  int rail_frame, cut;
  u32 send;
  s32 wave_r;
} E;

static void ValSpawn(void) {
  const s32 x = FX(200), y = E.bowy;
  E.shadow = AbSpawn("*lab/boss/valhalla_shadow", x, y);
  E.jets = AbSpawn("*lab/boss/valhalla_jets", x, y);
  E.bow = AbSpawn("*lab/boss/valhalla", x, y);
  E.stern = AbSpawn("*lab/boss/valhalla", x, y);
  AbFrame(E.bow, 0);
  AbFrame(E.stern, 1);
  E.fans[0] = AbSpawn("*lab/boss/val_fans_l", x, y);   // her lift fans running, over the wings
  E.fans[1] = AbSpawn("*lab/boss/val_fans_r", x, y);
  for (int k = 0; k < 6; k++) E.tur[k] = AbSpawn("*lab/boss/vturret", x, y);
  E.rail = AbSpawn("*lab/boss/vrailgun", x, y);
  E.rail_ang = UP_;
}

// Every piece where it belongs; what would be past the screen's wrap (a
// centre beyond +512 shows nowhere; below -512 it would show at the bottom)
// is dropped as she leaves, a piece at a time.
static void ValPlace(void) {
  if (!E.bow && !E.jets) return;
  const s32 x = FX(200), yc = E.bowy + FX(VAL_HALF / 2);
  AbPut(E.shadow, x + FX(80), yc + FX(130));
  AbPut(E.jets, x, yc + FX(VAL_JETS_DY));
  AbPut(E.bow, x, yc - FX(VAL_HALF / 2));
  AbPut(E.stern, x, yc + FX(VAL_HALF / 2));
  AbPut(E.fans[0], x + FX(VAL_FANS_L_X), yc + FX(VAL_FANS_L_Y));
  AbPut(E.fans[1], x + FX(VAL_FANS_R_X), yc + FX(VAL_FANS_R_Y));
  for (int k = 0; k < 6; k++) {
    AbPut(E.tur[k], x + FX(kVTurX[k]), yc + FX(kVTurY[k]));
    if (E.tur[k]) SB_SetActorOrient(E.tur[k], UP_);
  }
  // the railgun: on the pod while she has it in her sights
  if (E.rail && E.pod && E.cross)
    E.rail_ang = AbTurnTo(E.rail_ang, AbAngle(x, yc + FX(VAL_RAIL_DY), E.podx, E.pody), 0x30000);
  else if (E.rail)
    E.rail_ang = AbTurnTo(E.rail_ang, UP_, 0x20000);
  AbPut(E.rail, x, yc + FX(VAL_RAIL_DY));
  if (E.rail) {
    SB_SetActorOrient(E.rail, E.rail_ang);
    AbFrame(E.rail, E.rail_frame);
  }
  if (yc < FX(-VAL_HALF - 40)) {
    AbDrop(&E.bow);
    AbDrop(&E.stern);
    for (int s = 0; s < 2; s++) AbDrop(&E.fans[s]);
    AbDrop(&E.rail);
    AbDrop(&E.shadow);
    for (int k = 0; k < 6; k++) AbDrop(&E.tur[k]);
  }
  if (yc < FX(-VAL_JETS_DY - 70)) AbDrop(&E.jets);
}

static void PodPut(void) { AbPut(E.pod, E.podx, E.pody); }

// Her transmission: rings of violet signal spreading off the pod, one after another.
static void Transmit(void) {
  if (!E.wave_r) return;
  E.wave_r += FX(1.6);
  if (E.wave_r > FX(90)) E.wave_r = FX(10);
  AbSignalRing(E.podx, E.pody, E.wave_r);
}

static void TransmitEnd(void) {
  E.wave_r = 0;
  AbSignalRing(0, 0, 0);
  AbDrop(&E.send);
}

static void Links(int s, s32 ax, s32 ay) {
  for (int k = 0; k < NLINK; k++) {
    if (!E.link[s][k]) continue;
    const s32 u = (k + 1) * 6553;                 // 65536 / 10
    AbPut(E.link[s][k], ax + SB_MulFP(E.gx[s] - ax, u), ay + SB_MulFP(E.gy[s] - ay, u));
  }
}

static s32 AnchorX(int s) { return FX(200) + (s ? FX(34) : FX(-34)); }
static s32 AnchorY(void) { return E.bowy - FX(96); }

static void Glints(void) {
  for (int k = 0; k < NGLINT; k++) {
    if (!E.glint[k]) E.glint[k] = AbSpawn("*lab/boss/glint", FX(Rnd(10, 390)), FX(Rnd(20, 390)));
    if (((E.t + k * 13) & 63) == 0) AbPut(E.glint[k], FX(Rnd(10, 390)), FX(Rnd(20, 390)));
  }
}

void LabEndingStart(void) {
  SB_ClearBytes(&E, sizeof E);
  g_weapons_jammed = 1;
  E.phase = E_DOCK;
  LabSubsDock();
}

int LabEnding(void) {
  if (E.phase == E_OFF) return 0;
  E.t++;
  LabSubsTick(0, 0, 0);
  if (E.pod) PodPut();
  switch (E.phase) {
  case E_DOCK:
    if (E.t == 1) {
      AbLine(LB_BOARD);             // Volta: dock, board, find Argus
      E.ticket = AbLine(LB_MARROW_FLEE);
    }
    if (E.t > 150 && AbLineUp(E.ticket)) { E.phase = E_LAUNCH; E.t = 0; }
    break;
  case E_LAUNCH:
    if (E.t == 1) {
      E.podx = FX(200);
      E.pody = FX(LAB_Y);
      E.podv = FX(0.2);
      E.pod = AbSpawn("*lab/boss/escape_pod", E.podx, E.pody);
      AbBoom(E.podx, E.pody + FX(10), 0);
      SB_ShakeWorld();
      AbLine(LB_LAUNCH);
      AbLine(LB_POD);
      E.ticket = AbLine(LB_CHASE);
    }
    if (E.pody > FX(-80)) {
      if (E.podv < FX(4)) E.podv += FX(0.05);
      E.pody -= E.podv;
      if ((E.t & 3) == 0) SB_SpawnGeneric(BH, "*lab/boss/bubbles", E.podx + FX(Rnd(-6, 6)), E.pody + FX(36));
    } else {
      E.pody = FX(-200);
    }
    if (E.t > 100 && AbLineUp(E.ticket)) { E.phase = E_RISE; E.t = 0; }
    break;
  case E_RISE: {
    // the climb: the floor drops away beneath, the water lightens, the snow streams down
    const s32 k = E.t * 204;                       // 65536 / 320
    AbFloorFade(FX(1) - k);
    AbWaterTo(kSurface[0] + 22, kSurface[1] + 46, kSurface[2] + 50, k);
    AbStream(SB_MulFP(FX(4.5), k));
    R32(Shogun(), SH_SCROLL) = R32(Shogun(), SH_SCROLL) + 0xc00;        // the lab slides away under us
    g_docking = 2;
    if (E.t >= RISE_T) { E.phase = E_BREACH; E.t = 0; }
    break;
  }
  case E_BREACH:
    if (E.t == 1) {
      // the surface: the sea from above, the sun on it
      for (int s = 0; s < NSUBS; s++) AbDrop(&SUB[s].a);
      SB_ClearBytes(SUB, sizeof SUB);
      AbZoneNow(LAB_ZONES - 1);
      AbFloorFade(0);
      AbStream(0);
      SB_ShakeWorld();
      for (int k = 0; k < 8; k++) SB_SpawnGeneric(BH, "*lab/boss/bubbles", FX(Rnd(60, 340)), FX(Rnd(200, 420)));
      E.podx = FX(200);
      E.pody = FX(-60);
      AbLine(LB_SURFACE);
    }
    if (E.t <= 60) AbFloorFade(E.t * 1092);       // 65536 / 60
    AbWaterTo(230, 245, 255, E.t < 40 ? FX(1) - E.t * 1638 : 0);
    Glints();
    E.pody += Ease(FX(150) - E.pody, FX(1.5));   // below the dialog box
    E.podx = FX(200) + SB_Sin((E.t * 0x30000) & (TURN - 1)) * 24;
    if (E.t > 100) { E.phase = E_VALHALLA; E.t = 0; }
    break;
  case E_VALHALLA:
    Glints();
    E.podx = FX(200) + SB_Sin((E.t * 0x30000) & (TURN - 1)) * 24;
    E.pody += Ease(FX(150) - E.pody, FX(1));
    if (E.t == 1) {
      AbFloorFade(-1);
      AbWaterTo(0, 0, 0, 0);
      E.ticket = AbLine(LB_VALHALLA);
    }
    if (!E.bow && E.t > 20 && AbLineUp(E.ticket)) {
      E.bowy = FX(500);                           // her centre at 620: in from below
      ValSpawn();
    }
    if (E.bow) {
      E.bowy += Ease(BOW_REST - E.bowy, FX(2.2));
      ValPlace();
      if (AbAbs(E.bowy - BOW_REST) < FX(2)) {
        AbLine(LB_SOLUTION);        // Volta: a firing solution on the pod
        E.cross = AbSpawn("*lab/boss/crosshair", E.podx, E.pody);
        AbFrame(E.cross, XH_HUNT);
        AbLine(LB_WAIT);            // the Doc: Argus is aboard
        AbLine(LB_ARGUS_SHOOT);     // Argus: take the shot
        AbLine(LB_ORDERS);          // Volta: recover... or confirm
        AbLine(LB_PLEASE);          // the Doc: bring them both home
        E.ticket = AbLine(LB_GRAPPLES);
        E.phase = E_AIM;
        E.t = 0;
      }
    }
    break;
  case E_AIM:
    Glints();
    E.podx = FX(200) + SB_Sin((E.t * 0x30000) & (TURN - 1)) * 24;
    if (E.t == 50) {                              // swung onto it: locked, charged
      AbFrame(E.cross, XH_LOCKED);
      E.rail_frame = RAIL_CHARGED;
    }
    AbPut(E.cross, E.podx, E.pody);
    ValPlace();
    if (AbLineUp(E.ticket)) {
      // the railgun stands down: cold, swinging back fore and aft
      AbDrop(&E.cross);
      E.rail_frame = RAIL_COLD;
      for (int s = 0; s < 2; s++) {
        E.gx[s] = AnchorX(s);
        E.gy[s] = AnchorY();
        E.grap[s] = AbSpawn("*lab/boss/grapple", E.gx[s], E.gy[s]);
        for (int k = 0; k < NLINK; k++) E.link[s][k] = AbSpawn("*lab/boss/link", E.gx[s], E.gy[s]);
      }
      SB_ShakeWorld();
      E.phase = E_GRAPPLE;
      E.t = 0;
    }
    break;
  case E_GRAPPLE: {
    Glints();
    ValPlace();
    E.podx = FX(200) + SB_Sin((E.t * 0x30000) & (TURN - 1)) * 24;
    int there = 0;
    for (int s = 0; s < 2; s++) {
      const s32 wx = E.podx + (s ? FX(10) : FX(-10)), wy = E.pody + FX(14);
      const s32 d = AbDist(wx - E.gx[s], wy - E.gy[s]);
      if (d > 8) {
        const s32 a = AbAngle(E.gx[s], E.gy[s], wx, wy);
        E.gx[s] += SB_Cos(a) * 7;
        E.gy[s] += SB_Sin(a) * 7;
      } else {
        E.gx[s] = wx;
        E.gy[s] = wy;
        AbFrame(E.grap[s], 1);
        there++;
      }
      AbPut(E.grap[s], E.gx[s], E.gy[s]);
      Links(s, AnchorX(s), AnchorY());
    }
    if (there == 2) {
      // the clamps bite: they cut her power -- the rocket dies, the pod drops
      SB_ShakeWorld();
      AbFrame(E.pod, 1);
      E.cut = 1;
      E.send = AbSpawn("*lab/boss/drain", E.podx, E.pody);   // her uplink, pulsing out of the pod
      E.wave_r = FX(10);
      AbLine(LB_SEND);              // Marrow: going nowhere? then sending
      AbLine(LB_SEND_DOC);          // the Doc: she's transmitting, I can't block it
      E.phase = E_REEL;
      E.t = 0;
    }
    break;
  }
  case E_REEL: {
    Glints();
    ValPlace();
    const s32 want = AnchorY() - FX(30);
    E.podx += Ease(FX(200) - E.podx, FX(0.8));
    if (E.t < 30) E.pody += FX(0.9) - E.t * 1966;  // dead weight on the lines, sagging ...
    else E.pody += Ease(want - E.pody, FX(0.9));   // ... then hauled in
    for (int s = 0; s < 2; s++) {
      E.gx[s] = E.podx + (s ? FX(10) : FX(-10));
      E.gy[s] = E.pody + FX(14);
      AbPut(E.grap[s], E.gx[s], E.gy[s]);
      Links(s, AnchorX(s), AnchorY());
    }
    AbPut(E.send, E.podx, E.pody);                // her uplink still pulsing out of it
    Transmit();
    if (E.t == 1) AbLine(LB_CAUGHT);             // Volta: got her
    if (E.t > 60 && AbAbs(E.pody - want) < FX(2)) {
      E.ticket = AbLine(LB_MARROW_CAUGHT);   // Marrow: Doc, tell them who I am (the boarding party has her)
      AbLine(LB_BRIG);              // the Doc: ... gently
      AbLine(LB_ARGUS_SAFE);        // Argus: you were supposed to shoot
      AbLine(LB_VOLTA_SAFE);
      AbLine(LB_HELD);              // the Doc: his encryption held, no codes ...
      AbLine(LB_LOGS);              // Argus: ... but she took his logs instead
      AbLine(LB_HQ);                // the Doc: enough to find HQ
      AbLine(LB_MARROW_LAST);       // Marrow: did it reach him? Wouldn't you like to know
      AbLine(LB_END);               // Volta: home, before he works it out
      E.phase = E_TALK;
      E.t = 0;
    }
    break;
  }
  case E_TALK:
    Glints();
    ValPlace();
    AbPut(E.send, E.podx, E.pody);
    Transmit();
    if (AbLineUp(E.ticket)) TransmitEnd();        // sent, as the boarding party reaches her
    if (E.t > 240 && AbQuiet() && !SpeechPending()) { E.phase = E_LEAVE; E.t = 0; }
    break;
  case E_LEAVE:
    // the Valhalla flies on with her prize, up and away
    Glints();
    E.bowy -= E.t < 160 ? FX(0.4) + E.t * 600 : FX(0.4) + 160 * 600;     // easing up to ~1.9 a frame
    E.pody = AnchorY() - FX(30);
    for (int s = 0; s < 2; s++) {
      E.gx[s] = E.podx + (s ? FX(10) : FX(-10));
      E.gy[s] = E.pody + FX(14);
      AbPut(E.grap[s], E.gx[s], E.gy[s]);
      Links(s, AnchorX(s), AnchorY());
    }
    ValPlace();
    TransmitEnd();
    if (E.pody < FX(-60)) AbDrop(&E.pod);
    if (E.bowy + FX(VAL_HALF / 2) < FX(-VAL_JETS_DY - 80)) {
      ValPlace();
      AbDrop(&E.pod);
      for (int s = 0; s < 2; s++) {
        AbDrop(&E.grap[s]);
        for (int k = 0; k < NLINK; k++) AbDrop(&E.link[s][k]);
      }
      g_weapons_jammed = 0;
      E.phase = E_DONE;
    }
    break;
  case E_DONE:
    return 1;
  }
  return 0;
}

// A new run (lab_boss.c's LabBossInit, from InitBoss)
void LabReset(void) {
  SB_ClearBytes(&L, sizeof L);
  SB_ClearBytes(&E, sizeof E);
  SB_ClearBytes(SUB, sizeof SUB);
  SB_ClearBytes(FT, sizeof FT);
  g_subs_on = g_docking = 0;
  g_grace = 0;
  g_weapons_jammed = 0;
  AbUseLines(kLabSentences, LB_COUNT);
}
