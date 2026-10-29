// abyss.c -- Mission 11 (world 11): the Mariana Trench, and the Forge.
//
// After Mission 10 (sunda.c): Dr. Marrow's Specimen Nine has carried
// Captain Argus off and gone deep, and the pilot has gone down after her in
// the Valhalla's prototype submersible. Her trail is lost. Argus carries a
// UNITY tracker too faint to find down here -- unless they tap the Forge,
// the Shogun's relay station on the Mariana Trench, which Warden Grimwald
// keeps: every signal under the sea passes through it. Raided, its relays
// give up his signal, and the way to Marrow's lab (the next mission).
// Volta's prototype fights differently:
//
//   the ship   drawn as the prototype (sub.tga: nine banking frames, its
//              thrusters' wash under the stern) in place of RenderShip;
//   weapons    no homing (H) and no missiles (M): the prototype has no
//              racks for them. The radial menu shows H's cell drowned and takes
//              neither; the switch key skips both; a ship that arrives
//              holding one is put on S;
//   entropy    Volta pulled it (Marrow inverted the fleet's): ControlPlayer's
//              graze count reads 0 (shim_graze), and the shield's own
//              trickle back while the guns are quiet is undone every frame
//              (Entropy) -- it only rises with a parry, or a new life;
//   Spin Drive the parry. Trace a quick little circle and for a moment
//              every enemy shot near the ship is turned round -- it flies
//              back as a gold pearl, the pilot's own shot, at the nearest
//              enemy -- torpedoes too; nothing touches the ship meanwhile.
//              Each pearl tops the shield up a little (the Entropy Engine's
//              job, done another way). Sonar waves -- rings with no gap,
//              that nothing dodges -- are made for it;
//   armour     the nautilus's ceramic shell takes only the laser, the
//              bathysphere's brass only the coilgun's slug (marks in R21,
//              abyss/abyss_cast.py). Anything else sparks off, and the
//              weapon that would get through is shown over it. Parried
//              shots go through any armour;
//   currents   across the vents a current drags the ship sideways.
//
// And the deep's look, besides the floor and the props (abyss/): shafts of
// light from the surface, marine snow drifting down, bubbles rising.
//
// The bosses -- the Kurozame, the Leviathan -- are abyss_boss.c's.
// Run once a frame from the satellites' update (missile.c), and from the
// hooks: RenderPlayer's `bl RenderShip`, UpdateLaser's `bl StrikeBadguy`.
//
// Mission 12 (world 12, lab.c: the raid on Dr. Marrow's lab) is fought by
// the same rules in the same submersible, so all of this runs for it too:
// what differs between the two -- the floor's zones and their water, the
// lines, the generics' names, the story -- is a Deep record of each.
//
// Same rules as missile.c: no libc, no division, engine reached by address.

#define ABYSS_LINE_TABLE
#include "abyss.h"

#define AB_LoadBadGuy       FN(void (*)(u32, const char*), 0xbf324)       // ARM
#define AB_LoadBullet       FN(void (*)(u32, const char*), 0xbeffc)       // ARM
#define AB_LoadGeneric      FN(void (*)(u32, const char*), 0xbf140)       // ARM
#define AB_GetPartitionTime FN(s32 (*)(u32), 0xbae08)                     // ARM
#define AB_LoadSprites      FN(s32 (*)(u32, const char*, u32, u32), 0x57ea9)
#define AB_LoadSound        FN(s32 (*)(u32, const char*), 0x5bf35)
#define AB_PlaySound        FN(s32 (*)(u32, u32, u32, u32, u32), 0x5e485)
#define AB_SetChannelVolume FN(void (*)(u32, u32), 0x4e601)
#define AB_DrawSprite       FN(void (*)(u32, s32, s32, u32), 0x5f351)
#define AB_GetWidth         FN(s32 (*)(u32), 0x4d9b9)
#define AB_GetHeight        FN(s32 (*)(u32), 0x4d9f9)
#define AB_DrawRotatedSprite FN(void (*)(u32, s32, s32, s32, s32), 0x5f9a1)
#define AB_GetAlpha         FN(s32 (*)(void), 0x4eca9)
#define AB_InsertBagEntry   FN(u32* (*)(u32, u32*), 0x7828c)              // ARM
#define AB_GetFirstBagEntry FN(u32* (*)(u32), 0x77e7c)                    // ARM
#define AB_GetNextBagEntry  FN(u32* (*)(u32, u32*), 0x77f10)              // ARM
#define AB_StrikeBadguy     FN(void (*)(u32, s32), 0x3a3d1)
#define AB_RenderShip       FN(void (*)(u32, s32, s32), 0x2ea89)
#define AB_GetBadGuyPosition FN(void (*)(u32, s32*, s32*), 0xbbbb4)       // ARM
#define AB_SetEntityOpacity FN(void (*)(u32, s32), 0x4f3cd)
#define AB_ClearScreen      FN(void (*)(u32), 0x53455)
#define AB_GetFrameCount    FN(u32 (*)(void), 0x72434)                    // ARM
#define AB_GetGrazeCount    FN(s32 (*)(u32), 0xbaf7c)                     // ARM
#define AB_IsInvincible     FN(s32 (*)(u32), 0xbb40c)                     // ARM

static const char* const kBadguys[] = {
  "*abyss/scripts/lantern",
  "*abyss/scripts/lantern_l",
  "*abyss/scripts/lantern_r",
  "*abyss/scripts/minisub",
  "*abyss/scripts/torpedo",
  "*abyss/scripts/jellymine",
  "*abyss/scripts/nautilus",
  "*abyss/scripts/bathysphere",
  "*abyss/scripts/buoy",
  "*abyss/scripts/manta",
  "*abyss/scripts/manta_l",
  "*abyss/scripts/manta_r",
  "*abyss/scripts/salvage",
  "*abyss/scripts/squid",           // the small fry for the sparse stretches (abyss_cast.py)
  "*abyss/scripts/hatchet_l",
  "*abyss/scripts/hatchet_r",
  "*abyss/scripts/puffer",
  "*abyss/scripts/angel",
  "*abyss/scripts/lantern_x",       // hard mode's elites (abyss_cast.py, docs/HARD_MODE.md)
  "*abyss/scripts/lantern_l_x",
  "*abyss/scripts/lantern_r_x",
  "*abyss/scripts/minisub_x",
  "*abyss/scripts/jellymine_x",
  "*abyss/scripts/nautilus_x",
  "*abyss/scripts/bathysphere_x",
  "*abyss/scripts/buoy_x",
  "*abyss/scripts/manta_x",
  "*abyss/scripts/manta_l_x",
  "*abyss/scripts/manta_r_x",
  "*abyss/scripts/squid_x",
  "*abyss/scripts/hatchet_l_x",
  "*abyss/scripts/hatchet_r_x",
  "*abyss/scripts/puffer_x",
  "*abyss/scripts/angel_x",
  "*abyss/boss/hunter",
  "*abyss/boss/hunter_gun",
  "*abyss/boss/lev_head",
  "*abyss/boss/lev_lure",
  "*abyss/boss/lev_pod",
  "*abyss/boss/lev_core",
};
static const char* const kBullets[] = {
  "*abyss/boss/bubble",
  "*abyss/boss/ping",
  "*abyss/boss/spine",
  "*abyss/boss/wave",
  "*abyss/boss/seed",               // the mob attacks after Missions 3/4's bosses (abyss_cast.py)
  "*abyss/boss/hesitate",
  "*abyss/boss/drift",              // the bosses' revamp after Missions 2-4's (abyss_cast.py)
  "*abyss/boss/snipe",
  "*abyss/boss/charge",
  "*abyss/boss/comma",
  "*abyss/boss/twist",
  "*abyss/boss/trail",
  "*abyss/boss/needle",
  "*abyss/boss/jam",                // hard mode's jammed ping (the lanternfish elites)
  "*abyss/boss/h_comma",            // hard mode's two-stage bullets (beach/hardkit.py)
  "*abyss/boss/h_twist",
  "*abyss/boss/h_snipe",
  "*abyss/boss/h_lock",
  "*abyss/boss/h_drift",
  "*abyss/boss/h_hesitate",
  "*abyss/boss/h_needle",
  "*abyss/boss/h_sig",                 // its signature pair (hardkit.THEMES)
  "*abyss/boss/h_sigb",
};
static const char* const kGenerics[] = {
  "*abyss/boss/bubbles",
  "*abyss/boss/boom",
  "*abyss/boss/ray",
  "*abyss/boss/snow",
  "*abyss/boss/lure_link",
  "*abyss/boss/blast",
  "*abyss/boss/hunter_screw",
  "*abyss/boss/hunter_turret",
  "*abyss/boss/hunter_ping",
  "*abyss/boss/spore",
  "*abyss/boss/lev_eye_l",           // the Leviathan's lights burning (glow.h)
  "*abyss/boss/lev_eye_r",
  "*abyss/boss/lev_fire",
};

#define COUNT(a) (sizeof(a) / sizeof((a)[0]))

// Before the world file, which binds every script's sprites (beach.c).
void RegisterAbyss(void) {
  for (u32 i = 0; i < COUNT(kBullets); i++) AB_LoadBullet(BH, kBullets[i]);
  for (u32 i = 0; i < COUNT(kGenerics); i++) AB_LoadGeneric(BH, kGenerics[i]);
  for (u32 i = 0; i < COUNT(kBadguys); i++) AB_LoadBadGuy(BH, kBadguys[i]);
}

void ForgetDefinitions(const char* const* badguys, u32 nb, const char* const* bullets, u32 nu,
                       const char* const* generics, u32 ng);
void ForgetAbyss(void) {
  ForgetDefinitions(kBadguys, COUNT(kBadguys), kBullets, COUNT(kBullets), kGenerics, COUNT(kGenerics));
}

// ---- what the code draws itself (abyss/art_deep.py; abyss_cast.CODE_SPRITES) -------------------
// Handles clear of missile.c's 0x3e00..0x3e7b, Edinburgh's 0x3e80 and the
// level select's (worlds.c: up to 0x3f82).
#define SPR_SUB      0x3fa0       // four animation steps of nine frames, banked left .. right,
#define SUB_STEP     0x10         // ... 0x10 apart: 0x3fa0, 0x3fb0, 0x3fc0, 0x3fd0
#define SUB_PHASES   4
#define SPR_WASH     0x3f10       // six
#define SPR_PARRY    0x3f18       // six
#define SPR_PEARL    0x3f20       // six
#define SPR_IMMUNE   0x3f28       // "IMMUNE"
#define SPR_ARMOUR   0x3f29       // the weapon that gets through: laser, coilgun (two)
#define SPR_SIGNAL   0x3f89       // Mission 12: Marrow's transmission, a violet wisp (four)
// red outlines of the armoured, by sheet (art_deep.OUTLINED; Mission 12's
// bosses' from lab/art_lab.py, loaded only there)
enum { OL_NAUTILUS, OL_BATHY, OL_HUNTER, OL_LEVIATHAN, OL_LURE, OL_POD, OL_BELL = OUTLINE_BELL, OL_MANTLE, OL_CLUB,
       OL_SIPHON, NOUTLINES };
#define OL_FIRST_LAB OL_BELL
static const unsigned short kOutline[NOUTLINES] = { 0x3f50, 0x3f54, 0x3f58, 0x3f5c, 0x3f60, 0x3f64,
                                                    0x3f68, 0x3f70, 0x3f78, 0x3f7c };
static const unsigned char kOutlineFrames[NOUTLINES] = { 4, 4, 4, 4, 4, 2, 7, 4, 2, 3 };
static const char* const kOutlineFile[NOUTLINES] = {
  "*abyss/sprites/nautilus_red.tga", "*abyss/sprites/bathysphere_red.tga", "*abyss/sprites/hunter_red.tga",
  "*abyss/sprites/leviathan_red.tga", "*abyss/sprites/lure_red.tga", "*abyss/sprites/pod_red.tga",
  "*lab/sprites/bell_red.tga", "*lab/sprites/mantle_red.tga", "*lab/sprites/club_red.tga",
  "*lab/sprites/siphon_red.tga",
};
#define SPR_LOCKED   0x3f2c       // the drowned weapon cell
#define SPR_HITBOX   0x4ac        // the game's own
#define SPR_IMPACT   0x509        // Spread's impacts (missile.c)
#define SND_PARRY    0x3f40
#define SND_CHARGE   0x3f42
#define SND_ABSORB   0x820        // the game's own: the Entropy Engine drinking a graze (GrazeFXCallback)

static void Load(void) {
  static const char* const kSub[SUB_PHASES] = {
    "*abyss/sprites/sub0.tga", "*abyss/sprites/sub1.tga", "*abyss/sprites/sub2.tga", "*abyss/sprites/sub3.tga",
  };
  for (int k = 0; k < SUB_PHASES; k++) AB_LoadSprites(SPR_SUB + SUB_STEP * k, kSub[k], 9, 0x8000);
  const int n = R32(Shogun(), SH_WORLD_NO) == LAB ? NOUTLINES : OL_FIRST_LAB;
  for (int k = 0; k < n; k++) AB_LoadSprites(kOutline[k], kOutlineFile[k], kOutlineFrames[k], 0x10000);
  AB_LoadSprites(SPR_IMMUNE, "*abyss/sprites/immune.tga", 1, 0x10000);
  AB_LoadSprites(SPR_ARMOUR, "*abyss/sprites/armour.tga", 2, 0x10000);
  if (R32(Shogun(), SH_WORLD_NO) == LAB) AB_LoadSprites(SPR_SIGNAL, "*lab/sprites/aura.tga", 4, 0x10000);
  AB_LoadSprites(SPR_WASH, "*abyss/sprites/sub_wash.tga", 6, 0x8000);
  AB_LoadSprites(SPR_PARRY, "*abyss/sprites/parry.tga", 6, 0x12000);
  AB_LoadSprites(SPR_PEARL, "*abyss/sprites/pearl.tga", 6, 0x10000);
  AB_LoadSprites(SPR_LOCKED, "*abyss/sprites/locked.tga", 1, 0x8000);
  AB_LoadSound(SND_PARRY, "common/sounds/BULLET_CANCEL.wav");
  AB_LoadSound(SND_CHARGE, "common/sounds/MIDBOSS1LOAD.wav");
}

void AbSound(int which) {
  if (which == ABS_ABSORB) {             // a little under full volume (255)
    const s32 ch = AB_PlaySound(SND_ABSORB, 0, 0, 0, 0);
    if (ch) AB_SetChannelVolume(ch, 150);
    return;
  }
  AB_PlaySound(which == ABS_PARRY ? SND_PARRY : SND_CHARGE, 0, 0, 0, 0);
}

// ---- the PLAYER's fields this file touches -------------------------------------------------------
#define P_X          0x00         // 16.16
#define P_Y          0x1c
#define P_TOUCHX     0x5c         // s16: where the finger is (UpdatePlayerInput)
#define P_TOUCHY     0x5e
#define P_LIFTED     0x60         // non-zero while no finger is down
#define P_BANK       0x64         // MovePlayer's banking (RenderShip picks the frame from it)
#define P_ARMOUR     0x8c         // the ship's shield, 0xaaaa full
#define P_TYPE       0xbc         // the weapon: 1 S, 2 H, 3 L; 4 M, 5 E
#define P_ACTION     0xdc         // WEAPON+0x2c: a pending action (1-3 set S/H/L)
#define ARMOUR_FULL  0xaaaa
#define SH_DRAW_X    0x5114       // the ship as RenderShip draws it (whole units)
#define SH_DRAW_Y    0x5130
#define SH_SHOWHIT   0x8d170      // the hitbox option

static u32 Player(void) { return Shogun() + SH_PLAYER; }
#define P_CAPS 0x74
static s32 g_armour_prev, g_caps_prev;  // the shield and capsules last frame (Entropy)
static int g_armour_known;

// ---- the level's state ---------------------------------------------------------------------------
static struct {
  int on;
  int quiet;
  int said;
  int graze_off;                  // we hold the no-graze flag
  u32 t;                          // frames this run
  int bubble_cool;
  int buoys_seen, naut_seen, bathy_seen;
} A;
enum { SAID_BRIEF = 1, SAID_SPIN = 2, SAID_BUOY = 4, SAID_PARRY = 8, SAID_NAUT = 16, SAID_BATHY = 32,
       SAID_CURRENT = 64, SAID_FORGE = 128 };
static u32 g_ticked;              // the frame count when AbyssTick last ran (playing, not a menu)

// The two worlds fought down here, and what differs between them.
static int Deep(void) {
  const s32 w = R32(Shogun(), SH_WORLD_NO);
  return w == ABYSS || w == LAB;
}
static int IsLab(void) { return R32(Shogun(), SH_WORLD_NO) == LAB; }

#define MAX_ZONES 6
typedef struct {
  int zones, frames;                       // the floor: zones x frames textures, one zone's loop at a time
  s32 opacity[MAX_ZONES];                  // how bright each zone's floor shows
  unsigned char water[MAX_ZONES][3];       // the water round it (the clear colour)
  unsigned char rays[MAX_ZONES];           // shafts of light from the surface
  int glow_from;                           // from this zone on, a quarter of the snow glows
  int ndesc;                               // how many descents
  unsigned short descents[4];              // the partition's clock at each descent
  const char *snow, *ray, *bubbles, *boom; // the generics the deep's look is made of
} DeepWorld;

static const DeepWorld kTrench = {
  ZONES, ZONE_FRAMES,
  { 0x10000, 0xe400, 0xc000, 0xd800 },
  // sunlit blue, twilight, midnight, the trench's ember dark
  { { 14, 58, 92 }, { 6, 30, 56 }, { 2, 8, 22 }, { 12, 6, 10 } },
  { 3, 1, 0, 0 }, 2,
  3, { DESCENT_1_T, DESCENT_2_T, DESCENT_3_T },
  "*abyss/boss/snow", "*abyss/boss/ray", "*abyss/boss/bubbles", "*abyss/boss/boom",
};
static const DeepWorld kLab = {
  LAB_ZONES, LAB_ZONE_FRAMES,
  { 0xc000, 0xd000, 0xd800, 0xe800, 0x10000, 0x10000 },
  // midnight, the boneyard's green-black, the trench, the outworks' violet dark, the chasm, the surface
  { { 2, 8, 22 }, { 6, 18, 16 }, { 12, 6, 10 }, { 10, 4, 18 }, { 2, 5, 12 }, { 18, 74, 116 } },
  { 0, 0, 0, 0, 0, 3 }, 0,
  4, { LAB_DESCENT_1, LAB_DESCENT_2, LAB_DESCENT_3, LAB_DESCENT_4 },
  "*lab/boss/snow", "*lab/boss/ray", "*lab/boss/bubbles", "*lab/boss/boom",
};
static const DeepWorld* W_ = &kTrench;

int AbyssOn(void) { return Deep() && A.on; }
int AbyssNoSeekers(void) { return Deep(); }   // missile.c: H and M are out
s32 AbTime(void) { return AB_GetPartitionTime(BH); }

// ---- the dialog, by name (andaman.c's way) ---------------------------------------------------------
static int g_line_next;
static const unsigned char* g_sentences = kLineSentences;
static int g_nlines = L_COUNT;

// lab.c: its own speeches (each world's bossspeech.cfg, in order)
void AbUseLines(const unsigned char* sentences, int n) {
  g_sentences = sentences;
  g_nlines = n;
  g_line_next = 0;
}

static int SkipTo(int k) {
  int skip = 0;
  for (int i = g_line_next; i < k; i++) skip += g_sentences[i];
  g_line_next = k + 1;
  return skip;
}

int AbLine(int k) {
  if (k < g_line_next || k >= g_nlines) return 0;
  SaySkipping(SkipTo(k));
  return (int)(SpeechShown() + SpeechPending());
}

int AbLineUp(int ticket) { return (int)SpeechShown() >= ticket; }

void AbLineNow(int k) {
  if (k < g_line_next || k >= g_nlines) return;
  SayNowSkipping(SkipTo(k));
}

int AbLineSaid(int k) { return k < g_line_next; }

#define STUCK_WAIT 900
int AbQuiet(void) { return A.quiet > 40 || SpeechStalled() > STUCK_WAIT; }

// ---- booms ----------------------------------------------------------------------------------------
void AbBoom(s32 x, s32 y, int big) {
  Boom(big ? "ExplosionXL" : "ExplosionL", x, y);
  SB_SpawnGeneric(BH, W_->boom, x, y);
  if (big) SB_ShakeWorld();
}

// ---- the dive: down through the zones -------------------------------------------------------------
// The floor is one looping animation of ZONES x ZONE_FRAMES textures
// (abyss/art_seabed.py: the sunlit reef, the twilight silt, the midnight
// ooze, the trench floor). The engine's BACKGROUND record (SHOGUN+0x58)
// plays BG_COUNT of the handles in BG_TEX; ours keeps the whole list and
// gives it one zone's loop at a time. A descent: the floor falls away into
// the dark (its six entities fade out), the snow streams up past the ship,
// and at the bottom of the fall the next zone's floor is swapped in and
// comes up out of the dark -- dimmer than the one before, the water darker
// round it (the screen's clear colour, hook_clear_screen).
#define SH_BG_COUNT  0x8c         // BACKGROUND+0x34: animation length
#define SH_BG_TEX    0x90         // BACKGROUND+0x38: the texture of each animation step
#define FLOOR_ENT    0x638        // the floor's six entities (LoadBackground)
#define NTEX         32           // LoadBackground keeps 32 (it writes the next field at +0x110)
static u32 g_tex[NTEX];
static int g_zone, g_have_tex, g_desc = -1, g_desc_t, g_desc_k;
static s32 g_floor_op = -1;
// lab.c's ending, climbing back to the surface: the floor faded by hand
// (-1: not), the water's colour given (and how far toward it), the snow
// streaming down past the ship
static s32 g_fade = -1, g_water_k;
static unsigned char g_water_to[3];
static s32 g_stream;

static void FloorOpacity(s32 op) {
  if (op == g_floor_op) return;
  g_floor_op = op;
  for (u32 e = FLOOR_ENT; e < FLOOR_ENT + 6; e++) AB_SetEntityOpacity(e, op);
}

static void ShowZone(int z) {
  const u32 sh = Shogun();
  const int ntex = W_->zones * W_->frames;
  if (!g_have_tex) {
    if (R32(sh, SH_BG_COUNT) != ntex || ntex > NTEX) return;   // (not our world file: leave it)
    for (int k = 0; k < ntex; k++) g_tex[k] = (u32)R32(sh, SH_BG_TEX + 4 * k);
    g_have_tex = 1;
  }
  for (int k = 0; k < W_->frames; k++) R32(sh, SH_BG_TEX + 4 * k) = (s32)g_tex[z * W_->frames + k];
  R32(sh, SH_BG_COUNT) = W_->frames;
  g_zone = z;
}

// lab.c: a zone shown now (the surface), the floor faded (0..0x10000, -1 to
// let the dive decide again), the water drawn toward a colour (k of the
// way), the snow streaming down (rising) at v units a frame.
void AbZoneNow(int z) { ShowZone(z); }
int AbZone(void) { return g_zone; }
void AbFloorFade(s32 k) { g_fade = k; }
void AbWaterTo(int r, int g, int b, s32 k) {
  g_water_to[0] = (unsigned char)r;
  g_water_to[1] = (unsigned char)g;
  g_water_to[2] = (unsigned char)b;
  g_water_k = k;
}
void AbStream(s32 v) { g_stream = v; }

// 0..0x10000: how far into the fall (0 at the ledge, 1 at the bottom, 0 again on the new floor)
static s32 Fall(void) {
  if (g_desc < 0) return 0;
  if (g_desc_t < DESCENT_SWAP) return g_desc_t * 504;                 // 65536 / 130
  return FX(1) - (g_desc_t - DESCENT_SWAP) * 386;                      // 65536 / 170
}

void LabDescent(int k);           // lab.c: its lines for descent k

static void Depth(s32 pt) {
  if (g_desc < 0 && g_desc_k < W_->ndesc && pt >= W_->descents[g_desc_k]) {
    g_desc = g_desc_k++;
    g_desc_t = 0;
    if (IsLab())
      LabDescent(g_desc);
    else
      AbLine(g_desc == 0 ? L_DIVE_1 : (g_desc == 1 ? L_DIVE_2 : L_DIVE_3));
  }
  if (g_desc >= 0) {
    if (++g_desc_t == DESCENT_SWAP) ShowZone(g_desc + 1);
    if (g_desc_t >= DESCENT_LEN) g_desc = -1;
  }
  const s32 f = Fall();
  s32 op = SB_MulFP(W_->opacity[g_zone], FX(1) - f);
  if (g_fade >= 0) op = SB_MulFP(op, g_fade);
  FloorOpacity(op);
}

// The props camera's `blx glClipPlanef(GL_CLIP_PLANE0, plane)`: the engine
// cuts every model off at the ground (0, 0, 1, 0.01: z >= -0.01). Mission
// 12's chasm walls go down below the ground (lab/art_labprops.py cliff), so
// there the plane is let down out of the way. The plane's floats are copied as
// bits (no float sums in here); its stub in the PLT is ARM code.
typedef void (*ClipPlaneFn)(u32 plane, const u32* eq);
#define GL_CLIP_PLANEF ((ClipPlaneFn)0x100214a8)
void hook_clip_plane(u32 plane, const u32* eq) {
  if (!IsLab()) { GL_CLIP_PLANEF(plane, eq); return; }
  static u32 deep[4];
  deep[0] = eq[0];
  deep[1] = eq[1];
  deep[2] = eq[2];
  deep[3] = 0x461c4000;                  // 10000.0f (the plane is in the camera's units, not rows)
  GL_CLIP_PLANEF(plane, deep);
}

// ... and its `blx glOrthox(l, r, b, t, near, far)` (16.16), where it sets
// up the props' camera: the far plane at 128 cuts the chasm's walls off a
// little way below the ground, so there it goes back to 2048; and the
// floor, drawn first, writes the depth of the ground, which hides anything
// under it -- so from the chasm's descent on the props go without the depth
// test (the chasm model lays its faces deepest first for that).
typedef void (*OrthoxFn)(s32 l, s32 r, s32 b, s32 t, s32 n, s32 f);
#define GL_ORTHOX ((OrthoxFn)0x100214b4)
typedef void (*GlEnumFn)(u32 cap);
#define GL_DISABLE ((GlEnumFn)0x1002146c)
#define GL_DEPTH_TEST 0x0b71
#define LAB_CHASM_DESCENT 4                     // lab_timeline.DESCENTS: the fourth, into the chasm
void hook_props_ortho(s32 l, s32 r, s32 b, s32 t, s32 n, s32 f) {
  if (!IsLab()) { GL_ORTHOX(l, r, b, t, n, f); return; }
  GL_ORTHOX(l, r, b, t, n, FX(2048));
  if (g_desc_k >= LAB_CHASM_DESCENT) GL_DISABLE(GL_DEPTH_TEST);
}

// Every world: T.292, the generic entities' renderer, draws each one with
// `bl UE_DrawRotatedSprite(sprite, x, y, scale, turn)` -- a sine, a cosine and
// a turned quad in soft float, ~1.5 KB of code a draw -- though nearly all of
// them stand unturned at full size (a Kraken's arms, its aura, the floors'
// motes: ~100 a frame); BH_RenderBullets draws every bullet the same way,
// and most aren't turned either. Those are drawn plain instead, at the same
// centre, at full alpha only (the plain draw ignores the alpha stack,
// UE_PushAlpha) -- the same pixels, checked frame by frame against the
// engine's own drawing (Missions 5, 6 and 12; the graze sparkles by the
// ship differ run to run anyway).
// Drawn late (Mission 15's rocket): the engine draws generics in the order of
// their slots, so one made early is under everything made since -- the rocket,
// standing from the base's arrival, rose under the defences' platforms. The
// sprites in [g_late_lo, g_late_hi) are kept out of the generics' pass and drawn
// after it (LateDraws, from the player's render: missile.c).
u32 g_late_lo, g_late_hi;
#define NLATE 4
static struct { u32 spr; s32 x, y, scale, turn; } g_late[NLATE];
static int g_nlate;

static void DrawGeneric(u32 spr, s32 x, s32 y, s32 scale, s32 turn);

void LateDraws(void) {
  for (int i = 0; i < g_nlate; i++) DrawGeneric(g_late[i].spr, g_late[i].x, g_late[i].y, g_late[i].scale, g_late[i].turn);
  g_nlate = 0;
}

void hook_generic_draw(u32 spr, s32 x, s32 y, s32 scale, s32 turn) {
  if (spr >= g_late_lo && spr < g_late_hi && g_nlate < NLATE) {
    g_late[g_nlate].spr = spr;
    g_late[g_nlate].x = x;
    g_late[g_nlate].y = y;
    g_late[g_nlate].scale = scale;
    g_late[g_nlate].turn = turn;
    g_nlate++;
    return;
  }
  DrawGeneric(spr, x, y, scale, turn);
}

static void DrawGeneric(u32 spr, s32 x, s32 y, s32 scale, s32 turn) {
  if (turn == 0 && scale == 0x10000 && AB_GetAlpha() >= 0x10000) {
    AB_DrawSprite(spr, x - (AB_GetWidth(spr) >> 1), y - (AB_GetHeight(spr) >> 1), 0);
    return;
  }
  AB_DrawRotatedSprite(spr, x, y, scale, turn);
}

// onRender's `bl UE_ClearScreen(rgb565)`: playing down here, the water's colour, not black.
int HomeClearColour(u32* colour);  // homeward.c: Mission 13's water, round its sea's changes
int AmaClearColour(u32* colour);   // amaterasu.c: Mission 14's shelf and seas
void hook_clear_screen(u32 colour) {
  if (colour == 0) HomeClearColour(&colour);
  if (colour == 0) AmaClearColour(&colour);
  if (colour == 0 && Deep() && AB_GetFrameCount() - g_ticked < 4) {
    const unsigned char* w = W_->water[g_zone];
    s32 r = w[0], g = w[1], b = w[2];
    // falling: the water darkens toward the next zone's as the floor goes
    if (g_desc >= 0 && g_desc_t < DESCENT_SWAP && g_zone + 1 < W_->zones) {
      const unsigned char* n = W_->water[g_zone + 1];
      const s32 f = Fall();
      r += SB_MulFP(n[0] - r, f);
      g += SB_MulFP(n[1] - g, f);
      b += SB_MulFP(n[2] - b, f);
    }
    if (g_water_k > 0) {
      r += SB_MulFP(g_water_to[0] - r, g_water_k);
      g += SB_MulFP(g_water_to[1] - g, g_water_k);
      b += SB_MulFP(g_water_to[2] - b, g_water_k);
    }
    if (r > 255) r = 255;
    if (g > 255) g = 255;
    if (b > 255) b = 255;
    colour = ((u32)(r >> 3) << 11) | ((u32)(g >> 2) << 5) | (u32)(b >> 3);
  }
  AB_ClearScreen(colour);
}

// ---- the look of the deep: shafts of light, marine snow, bubbles -----------------------------------
#define NRAYS 3
#define NSNOW 26
static struct { u32 a; } RAY[NRAYS];
static struct { u32 a; s32 x, y, vy, ph; } SNOW[NSNOW];

static void Ambience(void) {
  const s32 t = (s32)A.t;
  const int rays = W_->rays[g_zone];               // the light gives out as we go down
  const s32 fall = Fall();
  for (int k = 0; k < NRAYS; k++) {
    if (k >= rays) { AbDrop(&RAY[k].a); continue; }
    if (!RAY[k].a) RAY[k].a = AbSpawn(W_->ray, FX(80 + 120 * k), FX(150));
    // swaying slowly, each its own way
    const s32 sway = SB_Sin((t * 9000 + k * 0x550000) & (TURN - 1)) * 34;
    AbPut(RAY[k].a, FX(70 + 130 * k) + sway, FX(140));
  }
  for (int k = 0; k < NSNOW; k++) {
    if (!SNOW[k].a) {
      SNOW[k].x = FX(Rnd(0, 400));
      SNOW[k].y = FX(Rnd(-20, 420));
      SNOW[k].vy = Rnd(10, 26) * 1024;          // 0.15 .. 0.4 units a frame
      SNOW[k].ph = Rnd(0, 255) << 16;
      SNOW[k].a = AbSpawn(W_->snow, SNOW[k].x, SNOW[k].y);
      AbFrame(SNOW[k].a, k & 1);
    }
    // deep down a quarter of the motes are living light
    if (!(A.t & 63))
      AbFrame(SNOW[k].a, (g_zone >= W_->glow_from && !W_->rays[g_zone] && !(k & 3)) ? 2 + ((k >> 2) & 1) : (k & 1));
    // sinking: everything in the water streams up past the ship (rising: down)
    SNOW[k].y += SNOW[k].vy - SB_MulFP(fall, FX(4.5)) + g_stream;
    if (SNOW[k].y < FX(-20)) {
      SNOW[k].y = FX(415);
      SNOW[k].x = FX(Rnd(0, 400));
    }
    SNOW[k].ph = (SNOW[k].ph + 0x30000) & (TURN - 1);
    if (SNOW[k].y > FX(420)) {
      SNOW[k].y = FX(-12);
      SNOW[k].x = FX(Rnd(0, 400));
    }
    AbPut(SNOW[k].a, SNOW[k].x + SB_Sin(SNOW[k].ph) * 3, SNOW[k].y);
  }
  if (--A.bubble_cool <= 0) {
    A.bubble_cool = (fall > FX(0.3) || g_stream > FX(1)) ? Rnd(10, 18) : Rnd(45, 90);
    SB_SpawnGeneric(BH, W_->bubbles, FX(Rnd(10, 390)), FX(430));
  }
}

// The current drags the ship (and the snow) sideways across the vents.
void AbPull(s32 x, s32 y, s32 rate) {
  const u32 p = Player();
  if (*(volatile unsigned short*)(Shogun() + SH_HITLOCK)) return;      // (respawning)
  const s32 dx = x - R32(p, P_X), dy = y - R32(p, P_Y);
  const s32 d = AbDist(dx, dy);
  if (d < 4) return;
  const s32 a = AbAngle(R32(p, P_X), R32(p, P_Y), x, y);
  R32(p, P_X) = R32(p, P_X) + SB_MulFP(SB_Cos(a), rate);
  R32(p, P_Y) = R32(p, P_Y) + SB_MulFP(SB_Sin(a), rate);
}

// A breath drawing the ship in: AbPull, but it bites harder the closer the
// ship is (up to twice `rate` within 60 units, `rate` at 300 and beyond) and
// comes in gulps -- every 64 frames a 16-frame heave at half again, bubbles
// torn off the hull. The finger steers relative to where it
// went down, so a still finger gives the whole of it away: under a unit a
// frame was a drift you barely noticed; this has to be swum against.
void AbDraw(s32 x, s32 y, s32 rate) {
  const u32 p = Player();
  if (*(volatile unsigned short*)(Shogun() + SH_HITLOCK)) return;      // (respawning)
  const s32 d = AbDist(x - R32(p, P_X), y - R32(p, P_Y));
  s32 near = d <= 60 ? FX(1) : d >= 300 ? 0 : (300 - d) * (FX(1) / 240);
  s32 r = rate + SB_MulFP(rate, near);
  const int gulp = (A.t & 63) < 16;
  if (gulp) r += r >> 1;
  if (d < 14) r = SB_MulFP(r, d * (FX(1) / 14));                       // (settle, no jitter)
  AbPull(x, y, r);
  if (gulp && (A.t & 3) == 0)
    SB_SpawnGeneric(BH, W_->bubbles, R32(p, P_X) + FX(Rnd(-10, 10)), R32(p, P_Y) + FX(Rnd(-6, 6)));
}

static void Current(s32 pt) {
  if (pt < CURRENT_0_T || pt > CURRENT_1_T) return;
  // eases in and out, and gusts
  s32 k = FX(1);
  if (pt - CURRENT_0_T < 120) k = (pt - CURRENT_0_T) * 546;
  if (CURRENT_1_T - pt < 120) k = (CURRENT_1_T - pt) * 546;
  const s32 gust = FX(1) + (SB_Sin((pt * 0x40000) & (TURN - 1)) >> 1);
  const s32 push = SB_MulFP(SB_MulFP(FX(0.75), k), gust);
  const u32 p = Player();
  if (!*(volatile unsigned short*)(Shogun() + SH_HITLOCK)) R32(p, P_X) = R32(p, P_X) + push;
  for (int i = 0; i < NSNOW; i++) SNOW[i].x += push * 2;
}

// ---- weapons: no seekers down here, and no entropy ------------------------------------------------
static void Weapons(void) {
  const u32 p = Player();
  const s32 t = R32(p, P_TYPE);
  if ((t == 2 || t == 4) && R32(p, P_ACTION) == 0) R32(p, P_ACTION) = 1;      // to S
}

// ControlPlayer: `blx BH_GetGrazeCount` -- what it adds to the shield (and,
// full, turns into capsules). Nothing, down here.
extern int g_graze_jammed;           // amaterasu.c: Raijin's jam (Mission 14)

s32 c_graze(u32 bh) {
  const s32 n = AB_GetGrazeCount(bh);
  return AbyssOn() || g_graze_jammed ? 0 : n;
}

__attribute__((naked)) void shim_graze(void) {
  __asm__ volatile(
      "push {lr}\n"
      "bl c_graze\n"
      "pop {pc}\n");
}

// The shield's trickle (ControlPlayer: +0x10 a frame while not firing):
// undone. A new life (the capsules go down, or the ship is respawning)
// fills it as ever.

static void Entropy(void) {
  const u32 p = Player();
  const s32 a = R32(p, P_ARMOUR), c = R32(p, P_CAPS);
  const int reborn = *(volatile unsigned short*)(Shogun() + SH_HITLOCK) || c < g_caps_prev;
  if (g_armour_known && !reborn && a > g_armour_prev) R32(p, P_ARMOUR) = g_armour_prev;
  g_armour_known = 1;
  g_armour_prev = R32(p, P_ARMOUR);
  g_caps_prev = c;
}

// ---- the Spin Drive ---------------------------------------------------------------------------------
// The gesture: a quick small circle. The ship follows the finger, so its own
// track is watched (in playfield units, the same on every phone): over the
// last HIST frames with the finger down, the heading of its motion must
// turn through SPIN_TURN one way, round a loop no wider than SPIN_BOX and
// no smaller than SPIN_MIN, covering SPIN_PATH at least. Dodging zig-zags
// turn back and forth and cancel out; a circle keeps turning.
#define HIST         28
#define SPIN_TURN    0xd00000     // ~293 degrees (2^24 to the turn)
#define SPIN_BOX     FX(84)
#define SPIN_MIN     FX(7)
#define SPIN_PATH    FX(34)
#define SEG_MIN      FX(1.25)     // shorter moves join the next
#define SPIN_STEP_MAX 0x500000    // ~112 degrees: more in one move is a reversal, not a circle
#define SPIN_BACK    0x180000     // ~34 degrees the other way is a wobble; more, a change of mind
#define PARRY_TIME   18           // frames the window stays open
#define PARRY_COOL   26           // ... and after it, before the next
#define PARRY_R      58           // how far out shots are caught (units)
#define PARRY_REFILL 0x500        // shield per pearl (0xaaaa full: ~34)
#define GOD_PARRY    4            // our bit in GodHold (starfish.c: 1 cutscenes, missile.c: 2 the shield)

static struct { s32 x[HIST], y[HIST]; int n, head; } H;
static int g_parry, g_parry_cool, g_parry_t = 99, g_parries, g_turned;
static s32 g_armour_full;         // the shield as the level starts: full (parries refill to it, no further)

int AbParrying(void) { return g_parry > 0; }
int AbParries(void) { return g_parries; }

static void Track(void) {
  const u32 p = Player();
  if (R32(p, P_LIFTED)) { H.n = 0; return; }
  H.head = H.head + 1 == HIST ? 0 : H.head + 1;
  H.x[H.head] = R32(p, P_X);
  H.y[H.head] = R32(p, P_Y);
  if (H.n < HIST) H.n++;
}

static int Spun(void) {
  if (H.n < 8) return 0;
  s32 turn = 0, path = 0, last = -1;
  s32 x0 = 0x7fffffff, x1 = -0x7fffffff, y0 = 0x7fffffff, y1 = -0x7fffffff;
  int i = H.head;
  s32 px = H.x[i], py = H.y[i];
  for (int k = 1; k < H.n; k++) {
    i = i == 0 ? HIST - 1 : i - 1;
    const s32 qx = H.x[i], qy = H.y[i];
    if (qx < x0) x0 = qx;
    if (qx > x1) x1 = qx;
    if (qy < y0) y0 = qy;
    if (qy > y1) y1 = qy;
    const s32 dx = px - qx, dy = py - qy;
    if (AbAbs(dx) + AbAbs(dy) < SEG_MIN) continue;        // wait for a longer move
    path += AbAbs(dx) > AbAbs(dy) ? AbAbs(dx) + (AbAbs(dy) >> 1) : AbAbs(dy) + (AbAbs(dx) >> 1);
    const s32 a = SB_GetAngle(0, 0, dx >> 12, dy >> 12) & (TURN - 1);
    if (last >= 0) {
      s32 d = (last - a) & (TURN - 1);
      if (d >= TURN / 2) d -= TURN;
      // a circle turns smoothly, one way: a sharp reversal (dodging back
      // and forth) or a turn against the way it has been going ends it
      if (AbAbs(d) > SPIN_STEP_MAX) break;
      if (turn && ((d ^ turn) < 0) && AbAbs(d) > SPIN_BACK) break;
      turn += d;
    }
    last = a;
    px = qx;
    py = qy;
    if (x1 - x0 > SPIN_BOX || y1 - y0 > SPIN_BOX) return 0;
    if (AbAbs(turn) >= SPIN_TURN && path >= SPIN_PATH && (x1 - x0 >= SPIN_MIN || y1 - y0 >= SPIN_MIN))
      return 1;
  }
  return 0;
}

// The pearls: parried shots, flying as the pilot's own. Each is an entry in
// the engine's shot bag (missile.c's), so its hits, impacts and drawing are
// the engine's; its model gives the damage, and missile.c's c_strike asks
// AbyssGate with SRC_PARRY, which every armour lets by.
typedef struct {            // missile.c's DEF and MODEL, as the engine has them
  s32 sprite, mval, vx, vy, ox, oy, impact, ix, iy;
} DEF;
typedef struct { s32 damage, radius, mask, count; DEF* defs; } MODEL;
#define NEVER 0x7fffffff
#define PEARL(f) { SPR_PEARL + (f), NEVER, 0, 0, 0, 0, SPR_IMPACT, -9, -55 }
static DEF kPearlDefs[6] = { PEARL(0), PEARL(1), PEARL(2), PEARL(3), PEARL(4), PEARL(5) };
static MODEL kPearl = { 85, 7, 0xf, 6, kPearlDefs };
#define BAG_SHOTS    0x5c5
#define BAG_BULLETS  (BH + 0x14)  // the enemies' bullets: {?, actor, ?, cancel countdown}
#define NPEARL 96
#define PEARL_SPEED  FX(6.5)
static struct { u32* e; s32 x, y, vx, vy; int seen; } PL[NPEARL];

int AbyssShotSource(u32 model) { return model == (u32)&kPearl ? SRC_PARRY : SRC_OTHER; }

// The nearest enemy to shoot back at, or straight up.
static s32 BackAt(s32 x, s32 y, u32 skip) {
  u32 best = 0;
  s32 bd = 360, bx = 0, by = 0;
  for (u32 b = SB_GetFirstBadGuy(BH); b; b = SB_GetNextBadGuy(BH, b)) {
    if (b == skip || SB_GetBadGuyHealth(b) <= 0) continue;
    const s32 m = GetReg(b, 21) >> 16;
    if (m == MARK_TORP || m == MARK_TORP_BACK) continue;
    s32 ex, ey;
    AB_GetBadGuyPosition(b, &ex, &ey);
    if (ey < FX(-30) || ey > FX(430)) continue;
    const s32 d = AbDist(ex - x, ey - y);
    if (d < bd) { bd = d; best = b; bx = ex; by = ey; }
  }
  return best ? AbAngle(x, y, bx, by) : UP;
}

static void Pearl(s32 x, s32 y) {
  int k = 0;
  while (k < NPEARL && PL[k].e) k++;
  if (k == NPEARL) return;
  u32* e = AB_InsertBagEntry(BAG_SHOTS, 0);
  if (!e) return;
  const s32 a = BackAt(x, y, 0);
  e[0] = (u32)&kPearl;
  e[1] = 0;
  e[2] = (u32)(x >> 16);
  e[3] = (u32)(y >> 16);
  PL[k].e = e;
  PL[k].x = x;
  PL[k].y = y;
  PL[k].vx = SB_MulFP(SB_Cos(a), PEARL_SPEED);
  PL[k].vy = SB_MulFP(SB_Sin(a), PEARL_SPEED);
  PL[k].seen = 1;
  const u32 p = Player();
  const s32 full = g_armour_full ? g_armour_full : ARMOUR_FULL;
  s32 s = R32(p, P_ARMOUR) + PARRY_REFILL;
  if (s > full) s = full;
  if (s > R32(p, P_ARMOUR)) R32(p, P_ARMOUR) = s;
  g_armour_prev = R32(p, P_ARMOUR);
  g_turned++;
}

static int g_anim6, g_anim6_t;           // a six-frame loop, a frame every four (no modulo: no division)
static int g_sub_ph, g_sub_t;            // the sub's animation step, one every five frames

static void Pearls(void) {
  if (++g_anim6_t == 4) { g_anim6_t = 0; if (++g_anim6 == 6) g_anim6 = 0; }
  if (++g_sub_t == 5) { g_sub_t = 0; if (++g_sub_ph == SUB_PHASES) g_sub_ph = 0; }
  for (int k = 0; k < NPEARL; k++) PL[k].seen = 0;
  for (u32* e = AB_GetFirstBagEntry(BAG_SHOTS); e; e = AB_GetNextBagEntry(BAG_SHOTS, e)) {
    if (e[0] != (u32)&kPearl) continue;
    for (int k = 0; k < NPEARL; k++)
      if (PL[k].e == e) { PL[k].seen = 1; break; }
  }
  for (int k = 0; k < NPEARL; k++) {
    if (!PL[k].e) continue;
    if (!PL[k].seen || PL[k].e[0] != (u32)&kPearl) { PL[k].e = 0; continue; }
    PL[k].x += PL[k].vx;
    PL[k].y += PL[k].vy;
    u32* e = PL[k].e;
    e[1] = (u32)g_anim6;
    e[2] = (u32)(PL[k].x >> 16);
    e[3] = (u32)(PL[k].y >> 16);
  }
}

// Shots caught in the window: the enemies' bullets turned to pearls, their
// torpedoes turned round.
static void Turn(void) {
  const u32 p = Player();
  const s32 sx = R32(p, P_X), sy = R32(p, P_Y);
  for (u32* e = AB_GetFirstBagEntry(BAG_BULLETS); e; e = AB_GetNextBagEntry(BAG_BULLETS, e)) {
    if (e[3]) continue;                          // already being cancelled
    s32 bx, by;
    SB_GetActorPosition(e[1], &bx, &by);
    if (AbDist(bx - sx, by - sy) > PARRY_R) continue;
    e[3] = 1;                                    // gone next frame (BH_CancelBullets' countdown)
    Pearl(bx, by);
  }
  for (u32 b = SB_GetFirstBadGuy(BH); b; b = SB_GetNextBadGuy(BH, b)) {
    if (SB_GetBadGuyHealth(b) <= 0 || (GetReg(b, 21) >> 16) != MARK_TORP) continue;
    s32 bx, by;
    AB_GetBadGuyPosition(b, &bx, &by);
    if (AbDist(bx - sx, by - sy) > PARRY_R + 6) continue;
    SetReg(b, REG_AIM, BackAt(bx, by, b));
    SetReg(b, REG_CMD, FX(1));
    SetReg(b, 21, FX(MARK_TORP_BACK));           // (it says so itself next step; not twice)
    g_turned++;
  }
}

// Torpedoes turned round strike what they meet.
#define TORP_DMG 320
static void Torpedoes(void) {
  for (u32 t = SB_GetFirstBadGuy(BH); t; t = SB_GetNextBadGuy(BH, t)) {
    if (SB_GetBadGuyHealth(t) <= 0 || (GetReg(t, 21) >> 16) != MARK_TORP_BACK) continue;
    s32 tx, ty;
    AB_GetBadGuyPosition(t, &tx, &ty);
    for (u32 b = SB_GetFirstBadGuy(BH); b; b = SB_GetNextBadGuy(BH, b)) {
      if (b == t || SB_GetBadGuyHealth(b) <= 0) continue;
      const s32 m = GetReg(b, 21) >> 16;
      if (m == MARK_TORP || m == MARK_TORP_BACK) continue;
      s32 bx, by;
      AB_GetBadGuyPosition(b, &bx, &by);
      if (AbDist(bx - tx, by - ty) > 22) continue;
      SB_StrikeBadGuy(BH, b, TORP_DMG);
      AbBoom(tx, ty, 0);
      Remove(t);
      break;
    }
  }
}

static void SpinDrive(void) {
  Track();
  if (g_parry_t < 99) g_parry_t++;
  if (g_parry > 0) {
    Turn();
    if (--g_parry == 0) GodHold(GOD_PARRY, 0);
  } else if (g_parry_cool > 0) {
    g_parry_cool--;
  } else if (Spun() && !*(volatile unsigned short*)(Shogun() + SH_HITLOCK)) {
    g_parry = PARRY_TIME;
    g_parry_cool = PARRY_COOL;
    g_parry_t = 0;
    g_turned = 0;
    H.n = 0;
    GodHold(GOD_PARRY, 1);
    AbSound(ABS_PARRY);
    Turn();
  }
  if (g_parry == 0 && g_turned) {               // a window that caught something
    g_parries++;
    g_turned = 0;
  }
  Pearls();
  Torpedoes();
}

// ---- armour ----------------------------------------------------------------------------------------
// missile.c asks before a shot or the plasma strikes, and the laser hook
// below: 0 means the armour turned it. Whatever shrugs a hit off -- armour
// the weapon can't get through, or a part that can't be hurt at all (the
// Leviathan's head, anything shielded while someone talks) -- shows it: a
// red outline round it and IMMUNE over it, for a moment. No sound.
#define NBLOCK 8
#define BLOCK_SHOW 36
static struct { u32 bg; int t; } BLK[NBLOCK];

int AbyssPartOutline(u32 bg, int* frame);    // abyss_boss.c: a boss part's sheet (OL_*) and frame, or -1

static void Blocked(u32 bg) {
  int free_ = -1;
  for (int k = 0; k < NBLOCK; k++) {
    if (BLK[k].bg == bg && BLK[k].t > 0) { BLK[k].t = BLOCK_SHOW; return; }
    if (BLK[k].t <= 0 && free_ < 0) free_ = k;
  }
  if (free_ < 0) return;
  BLK[free_].bg = bg;
  BLK[free_].t = BLOCK_SHOW;
}

int AbyssGate(u32 bg, int src) {
  if (!AbyssOn()) return 1;
  if (AB_IsInvincible(bg)) {                              // the engine turns it; we show it --
    if (!InCutscene()) Blocked(bg);                      // not while everyone is held for a line
    return 1;
  }
  if (src == SRC_PARRY) return 1;
  const s32 m = GetReg(bg, 21) >> 16;
  if (m == MARK_LASER && src != SRC_LASER) { Blocked(bg); return 0; }
  if (m == MARK_SNIPER && src != SRC_SNIPER) { Blocked(bg); return 0; }
  return 1;
}

// Which red outline, and which of its frames, for a badguy that turned a hit.
int LabPartOutline(u32 bg, int* frame);      // lab_boss.c: the same, for Mission 12's (OUTLINE_*)
static int Outline(u32 bg, int* frame) {
  const int k = IsLab() ? LabPartOutline(bg, frame) : AbyssPartOutline(bg, frame);
  if (k >= 0) return k;
  *frame = 0;
  const s32 m = GetReg(bg, 21) >> 16;
  return m == MARK_LASER ? OL_NAUTILUS : (m == MARK_SNIPER ? OL_BATHY : -1);
}

// UpdateLaser: `bl StrikeBadguy`.
void hook_laser_strike(u32 bg, s32 dmg) {
  if (AbyssGate(bg, SRC_LASER)) AB_StrikeBadguy(bg, dmg);
}

// Every armoured enemy carries a flag: the weapon that gets through it (the
// laser's beam, the coilgun's crosshair), over its head, bobbing a little.
// Big ones (a boss part with an outline sheet) carry it above their top
// edge; it stays on screen.
// Mission 12's ending: a ring of signal spreading off Marrow's pod as she
// transmits -- drawn here, over everything (generics can land under the
// Valhalla). lab.c sets it each frame; r 0 is none.
static s32 g_ring_x, g_ring_y, g_ring_r;
void AbSignalRing(s32 x, s32 y, s32 r) { g_ring_x = x; g_ring_y = y; g_ring_r = r; }

static void SignalRing(s32 dx, s32 dy) {
  if (g_ring_r <= 0 || !IsLab()) return;
  for (int k = 0; k < 12; k++) {
    const s32 a = (k * 0x155555 + (s32)A.t * 0x8000) & (TURN - 1);      // 2^24 / 12 apart, turning
    const u32 spr = SPR_SIGNAL + ((k + (A.t >> 3)) & 3);
    const s32 x = (g_ring_x + SB_MulFP(SB_Cos(a), g_ring_r)) >> 16, y = (g_ring_y + SB_MulFP(SB_Sin(a), g_ring_r)) >> 16;
    AB_DrawSprite(spr, x + dx - (AB_GetWidth(spr) >> 1), y + dy - (AB_GetHeight(spr) >> 1), 0);
  }
}

static void Flags(s32 dx, s32 dy) {
  for (u32 b = SB_GetFirstBadGuy(BH); b; b = SB_GetNextBadGuy(BH, b)) {
    if (SB_GetBadGuyHealth(b) <= 0) continue;
    const s32 m = GetReg(b, 21) >> 16;
    if (m != MARK_LASER && m != MARK_SNIPER) continue;
    s32 bx, by;
    AB_GetBadGuyPosition(b, &bx, &by);
    if (by < FX(-30) || by > FX(430)) continue;
    int frame = 0;
    s32 up = 24;
    const int ol = Outline(b, &frame);
    if (ol >= 0) {
      if (frame < 0) frame = 0;
      if (frame >= kOutlineFrames[ol]) frame = kOutlineFrames[ol] - 1;
      up = (AB_GetHeight(kOutline[ol] + frame) >> 1) + 8;
    }
    const u32 icon = SPR_ARMOUR + (m == MARK_SNIPER);
    const s32 w = AB_GetWidth(icon), h = AB_GetHeight(icon);
    s32 y = (by >> 16) - up + (SB_Sin(((s32)A.t * 0x50000) & (TURN - 1)) >> 15);
    if (y < 14) y = 14;
    AB_DrawSprite(icon, (bx >> 16) + dx - (w >> 1), y + dy - (h >> 1), 0);
  }
}

// ---- the ship, as a submersible -----------------------------------------------------------------
// RenderPlayer: `bl RenderShip(SHOGUN, x, y)` -- (x, y) the camera's offset.
// The engine draws the ship at SHOGUN+0x5114/0x5130 (whole units), frame
// (bank + 0x12000) * 4 >> 16 of nine, its flames under it; ours draws the
// submersible there, its wash under its stern, and over it the Spin Drive's
// ring and the armour hints.
void hook_render_ship(u32 sh, s32 cx, s32 cy) {
  if (!Deep()) { AB_RenderShip(sh, cx, cy); return; }
  const u32 p = sh + SH_PLAYER;
  s32 f = ((R32(p, P_BANK) + 0x12000) * 4) >> 16;
  if (f < 0) f = 0;
  if (f > 8) f = 8;
  const s32 sx = R32(sh, SH_DRAW_X), sy = R32(sh, SH_DRAW_Y);
  const u32 sub = SPR_SUB + SUB_STEP * g_sub_ph;
  const s32 w = AB_GetWidth(sub), h = AB_GetHeight(sub);
  const u32 wf = SPR_WASH + g_anim6;
  const s32 ww = AB_GetWidth(wf);
  AB_DrawSprite(wf, sx - (ww >> 1) - cx, sy + (h >> 1) - 6 - cy, 0);
  AB_DrawSprite(sub + f, sx - (w >> 1) - cx, sy - (h >> 1) - cy, 0);
  if (R32(sh, SH_SHOWHIT)) {
    const s32 hw = AB_GetWidth(SPR_HITBOX), hh = AB_GetHeight(SPR_HITBOX);
    AB_DrawSprite(SPR_HITBOX, sx - (hw >> 1) - cx, sy - (hh >> 1) - cy, 0);
  }
  if (g_parry_t < 18) {
    const u32 rf = SPR_PARRY + ((g_parry_t * 11) >> 5);    // a third, for 0..17
    const s32 rw = AB_GetWidth(rf), rh = AB_GetHeight(rf);
    AB_DrawSprite(rf, sx - (rw >> 1) - cx, sy - (rh >> 1) - cy, 0);
  }
  // what turned a hit: a red outline round it, IMMUNE over it. The ship's
  // draw position is its playfield position plus an offset; so is
  // everything else's, and badguys are drawn centred on theirs.
  const s32 ox = sx - (R32(p, P_X) >> 16), oy = sy - (R32(p, P_Y) >> 16);
  Flags(ox - cx, oy - cy);
  SignalRing(ox - cx, oy - cy);
  for (int k = 0; k < NBLOCK; k++) {
    if (BLK[k].t <= 0 || !Alive(BLK[k].bg)) continue;
    if ((BLK[k].t & 2) && BLK[k].t < 12) continue;          // blinking out
    s32 bx, by;
    AB_GetBadGuyPosition(BLK[k].bg, &bx, &by);
    bx = (bx >> 16) + ox - cx;
    by = (by >> 16) + oy - cy;
    int frame = 0;
    const int ol = Outline(BLK[k].bg, &frame);
    s32 top = by - 14;
    if (ol >= 0) {
      if (frame < 0) frame = 0;
      if (frame >= kOutlineFrames[ol]) frame = kOutlineFrames[ol] - 1;
      const u32 spr = kOutline[ol] + frame;
      const s32 w = AB_GetWidth(spr), h = AB_GetHeight(spr);
      AB_DrawSprite(spr, bx - (w >> 1), by - (h >> 1), 0);
      top = by - (h >> 1) + 4;
      if (top < 26) top = by;                               // a big one high up: over its middle
    }
    AB_DrawSprite(SPR_IMMUNE, bx - (AB_GetWidth(SPR_IMMUNE) >> 1), top - AB_GetHeight(SPR_IMMUNE), 0);
  }
}

// missile.c's radial menu: the cell H takes is drowned down here.
u32 AbyssMenuSprite(u32 slot, u32 sprite) {
  return (AbyssNoSeekers() && slot == 1) ? SPR_LOCKED : sprite;
}

// ---- the dive's lines ---------------------------------------------------------------------------
static void Watch(void) {
  for (u32 b = SB_GetFirstBadGuy(BH); b; b = SB_GetNextBadGuy(BH, b)) {
    if (SB_GetBadGuyHealth(b) <= 0) continue;
    s32 x, y;
    AB_GetBadGuyPosition(b, &x, &y);
    if (y < FX(10)) continue;
    const s32 m = GetReg(b, 21) >> 16;
    // these go up the moment it shows, over whatever is waiting: they're
    // only any use while the thing is on screen
    if (m == MARK_BUOY && !(A.said & SAID_BUOY)) { A.said |= SAID_BUOY; AbLineNow(L_BUOY); }
    if (m == MARK_LASER && !(A.said & SAID_NAUT) && !g_autopilot_target) { A.said |= SAID_NAUT; AbLineNow(L_NAUTILUS); }
    if (m == MARK_SNIPER && !(A.said & SAID_BATHY) && !g_autopilot_target) { A.said |= SAID_BATHY; AbLineNow(L_BATHY); }
  }
}

static void Dive(s32 pt) {
  if (!(A.said & SAID_BRIEF) && pt >= BRIEF_T) {
    A.said |= SAID_BRIEF;
    AbLine(L_BRIEF);                // Volta: lost Specimen Nine
    AbLine(L_TRACE);                // the Doc: Argus's tracker, the Forge's relays
    AbLine(L_REFIT);                // Volta: no missiles, no entropy engine
  }
  if (!(A.said & SAID_PARRY) && g_parries > 0) { A.said |= SAID_PARRY; AbLine(L_PARRY_GOOD); }
  if (!(A.said & SAID_CURRENT) && pt >= CURRENT_0_T) { A.said |= SAID_CURRENT; AbLine(L_CURRENT); }
  if (!(A.said & SAID_FORGE) && pt >= FORGE_T) { A.said |= SAID_FORGE; AbLine(L_FORGE); }
  Watch();
}

// ---- a run --------------------------------------------------------------------------------------------
// InitBoss (worlds.c's rows for worlds 11 and 12): a new run of the level.
void LabTick(s32 pt);              // lab.c
void AbyssReset(void) {
  SB_ClearBytes(&A, sizeof A);
  SB_ClearBytes(RAY, sizeof RAY);                // (the world went: nothing to drop)
  SB_ClearBytes(SNOW, sizeof SNOW);
  SB_ClearBytes(PL, sizeof PL);
  SB_ClearBytes(BLK, sizeof BLK);
  SB_ClearBytes(&H, sizeof H);
  g_parry = g_parry_cool = g_parries = g_turned = 0;
  g_parry_t = 99;
  g_line_next = 0;
  g_have_tex = 0;
  g_desc = -1;
  g_desc_t = g_desc_k = 0;
  g_floor_op = -1;
  g_armour_known = 0;
  W_ = IsLab() ? &kLab : &kTrench;
  g_sentences = kLineSentences;                  // (lab.c gives its own)
  g_nlines = L_COUNT;
  g_fade = -1;
  g_ring_r = 0;
  g_water_k = 0;
  g_stream = 0;
  A.graze_off = 1;
  ShowZone(0);
  FloorOpacity(W_->opacity[0]);
  GodHold(GOD_PARRY, 0);
  Load();
  A.on = 1;
  A.bubble_cool = 60;
}

// Once a frame while playing, from missile.c (every world: only ours acts).
void AbyssTick(void) {
  if (!Deep() || !A.on) {
    if (A.graze_off) {                           // another world: its floor whole
      A.graze_off = 0;
      g_floor_op = -1;
      FloorOpacity(0x10000);
    }
    return;
  }
  A.t++;
  if (A.t == 2) g_armour_full = R32(Player(), P_ARMOUR);
  SpeechPump();
  A.quiet = InCutscene() ? 0 : A.quiet + 1;
  const s32 pt = AbTime();
  g_ticked = AB_GetFrameCount();
  Entropy();
  Weapons();
  Depth(pt);
  if (IsLab()) {
    LabTick(pt);                                 // lab.c: Mission 12's story
  } else {
    Dive(pt);
    Current(pt);
  }
  SpinDrive();
  Ambience();
  for (int k = 0; k < NBLOCK; k++)
    if (BLK[k].t > 0) BLK[k].t--;
}
