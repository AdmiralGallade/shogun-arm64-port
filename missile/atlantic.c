// atlantic.c -- Mission 16 (world 16): The Intercept. The convoy's way home from France.
//
// The Shogun never went to France: he sent the legion to force the
// evacuation and waited on the convoy's road (docs/M16_DESIGN.md). The whole
// level is this "boss", as Mission 7's chase is: the partition raises it a
// moment after the start and stays paused until it is over.
//
//   calm      grey dawn, the convoy (five ships, two of France's Arche
//             lifters) on course up the left of the screen, the Valhalla at
//             the bottom right, stragglers from the sides;
//   akane     the Black Magpie (Mission 6's, *seoul/boss/magpie) comes in from
//             the north: Akane is there to make the convoy late for her
//             father's trap. She fights as at Seoul; when she's hurt enough (or
//             has stalled long enough) Volta puts her railgun on the Magpie --
//             and a moment before it fires Akane lights her afterburners and is
//             gone, the shot burning empty sea;
//   storm     rain, the storm wall, the jam's violet haze (amaterasu.c AmaJam).
//             The Kurogane comes out of the wall ahead, bow down; the convoy
//             turns about at once (the ships wheel round), and the scroll eases
//             nearly to rest (the sea's own clock keeps it moving);
//   first     Volta's full charge into the hull: moderate damage, far less
//             than anyone expected (the hull rebuilt against railguns). The
//             Shogun laughs. She stays to hold him;
//   cycle 1   the Tenshu cannon rises on the convoy; Volta's shot overloads the
//             dreadnought -- its guns stall, the jam mast goes dark, the haze
//             thins, the Tenshu's aim is thrown and it is left stuck out. In
//             that window the pilot flies in over the bow, paints it and calls
//             down the sun; the Shogun pulls the cannon under its armour before
//             the strike lands;
//   approach  the Valhalla and the Kurogane close until they are nose to nose
//             in the middle of the screen;
//   cycles    four more, face to face: the hunters (Kamikiri files from the
//             sides, crows out of the hangar, gunships from the corners,
//             bombers on the convoy) fill Volta's gauge; the dreadnought shells
//             the Valhalla and her escorts at point blank (fires on her hull),
//             the Tenshu goes for the convoy and her escorts; each window
//             shorter; a ship of the convoy gets clear with each;
//   fall      the Tenshu at full charge on the convoy; Volta's expert shot
//             cripples it (it can't fire again); the broadside breaks the
//             Valhalla's back -- her two halves tear apart, tilt, and fall
//             burning into the sea;
//   alone     the Kurogane turns on the wreck; the pilot keeps its hunters off;
//   charge    Argus brings the Bastion in: the breaches are the targets
//             (btarget badguys under each), the Bastion's shells and the pilot's
//             guns; at last the Kurogane withdraws into the storm;
//   after     the boats at the wreck, the last lines; the partition runs on to
//             the end.
//
// The dreadnought is Mission 7's, in pieces (antarctica/art_fleet.py), turned
// to face down (atlantic/art_kurogane16.py); its guns are Mission 7's own
// badguys, borrowed (only the attacks that aim at the ship are ordered). The
// pilot may fly over it: it is all generics and untouchable guns.
//
// Same rules as missile.c: no libc, no division, engine reached by address.

#define ATLANTIC_LINE_TABLE
#include "andaman.h"
#include "atlantic_lines.h"
#include "amaterasu.h"
#include "glow.h"
#include "seoul_glow.h"

#define ATLANTIC 16                // SHOGUN+0x4e6c

#define AL_LoadBadGuy       FN(void (*)(u32, const char*), 0xbf324)     // ARM
#define AL_LoadBullet       FN(void (*)(u32, const char*), 0xbeffc)     // ARM
#define AL_LoadGeneric      FN(void (*)(u32, const char*), 0xbf140)     // ARM
#define AL_LoadSprites      FN(s32 (*)(u32, const char*, u32, u32), 0x57ea9)
#define AL_DrawSprite       FN(void (*)(u32, s32, s32, u32), 0x5f351)
#define AL_GetScreenWidth   FN(s32 (*)(void), 0x4e955)
#define AL_GetScreenHeight  FN(s32 (*)(void), 0x4e971)
#define AL_GetWidth         FN(s32 (*)(u32), 0x4d9b9)
#define AL_GetFrameCount    FN(u32 (*)(void), 0x72434)                  // ARM
#define SH_UI_SCALE         0x8b9dc     // the menus' sprite scale (worlds.c)
#define SPR_GAUGE           0x3e82      // Volta's gauge, three frames (clear of the sun's 0x3e81)

static const char* const kBadguys[] = {
  "*atlantic/scripts/kami_side",
  "*atlantic/scripts/bomber",
  "*atlantic/scripts/crow16",
  "*atlantic/scripts/gunship16",
  "*atlantic/boss/core",
  "*atlantic/boss/btarget",
  "*atlantic/boss/atk16",
  "*atlantic/boss/mine16",
  "*atlantic/boss/wall16",
  "*atlantic/boss/harpoon16",
  // Mission 6's Black Magpie (seoul/cast.py), Akane's
  "*seoul/boss/magpie",
  // Mission 7's guns (antarctica/chase_cast.py), and what their scripts can emit
  "*antarctica/boss/turret",
  "*antarctica/boss/bow_gun",
  "*antarctica/boss/flak",
  "*antarctica/boss/wrath",
  "worlds/ocean/scripts/badguy_missile",
};
static const char* const kBullets[] = {
  "*atlantic/boss/hes16",
  "*antarctica/boss/ember",
  "*antarctica/boss/spark",
  "*antarctica/boss/h_comma",
  "*antarctica/boss/h_twist",
  "*antarctica/boss/h_snipe",
  "*antarctica/boss/h_lock",
  "*antarctica/boss/h_drift",
  "*antarctica/boss/h_hesitate",
  "*antarctica/boss/h_needle",
  "*antarctica/boss/h_sig",
  "*antarctica/boss/h_sigb",
  // the Magpie's
  "*seoul/boss/blossom",
  "*seoul/boss/kunai",
  "*seoul/boss/orb",
  "*seoul/boss/h_comma",
  "*seoul/boss/h_drift",
  "*seoul/boss/h_sig",
  "*seoul/boss/h_sigb",
  "*seoul/boss/h_snipe",
  "*seoul/boss/h_twist",
};
static const char* const kGenerics[] = {
  "*atlantic/boss/hull16",
  "*atlantic/boss/aft16",
  "*atlantic/boss/streamer16",
  "*atlantic/boss/underglow16",
  "*atlantic/boss/nacelle16",
  "*atlantic/boss/oni16",
  "*atlantic/boss/hangar16",
  "*atlantic/boss/hatch16",
  "*atlantic/boss/jam_mast",
  "*atlantic/boss/breach16",
  "*atlantic/boss/breach16_big",
  "*atlantic/boss/overload",
  "*atlantic/boss/tenshu_wreck",
  "*atlantic/boss/transport",
  "*atlantic/boss/arche16",
  "*atlantic/boss/scar16",
  "*atlantic/boss/portflash",
  "*atlantic/boss/searchlight",
  "*atlantic/boss/chain16",
  "*atlantic/boss/target16",
  "*atlantic/boss/cagebar",
  "*atlantic/boss/cagepost",
  "*atlantic/boss/rent16",
  "*atlantic/boss/valdown16",
  "*atlantic/boss/rain",
  "*atlantic/boss/haze",
  "*atlantic/boss/wall",
  "*atlantic/boss/flash",
  "*seoul/boss/magpie_glow",
  // Mission 7's: the turrets seen, the Tenshu cannon, its beam and lock-on, a blaze
  "*antarctica/boss/turret_vis",
  "*antarctica/boss/turret_big_vis",
  "*antarctica/boss/cannon_vis",
  "*antarctica/boss/megacannon",
  "*antarctica/boss/mega_beam",
  "*antarctica/boss/reticle",
  "*antarctica/boss/blaze",
  // Mission 15's: the bombers' mark
  "*paris/boss/mark",
  // Mission 14's: the sun's beam and reticle; the Bastion and the Home Fleet's destroyers
  "*amaterasu/boss/beam",
  "*amaterasu/boss/reticle",
  "*amaterasu/boss/closing",
  "*amaterasu/boss/bolt",          // (Raijin's enemy lightning: the lightning cage)
  "*amaterasu/boss/bastion14",
  "*andaman/boss/bturret",
  "*andaman/boss/ciws",
  "*amaterasu/boss/destroyer",
  // Mission 9's: the Valhalla, shells, blasts
  "*andaman/boss/blast",
  "*andaman/boss/tracer",
  "*andaman/boss/lance",
  "*andaman/boss/crosshair",
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

void RegisterAtlantic(void) {
  for (u32 i = 0; i < COUNT(kBullets); i++) AL_LoadBullet(BH, kBullets[i]);
  for (u32 i = 0; i < COUNT(kGenerics); i++) AL_LoadGeneric(BH, kGenerics[i]);
  for (u32 i = 0; i < COUNT(kBadguys); i++) AL_LoadBadGuy(BH, kBadguys[i]);
  AmaLoadHud();
  AL_LoadSprites(SPR_GAUGE, "*atlantic/sprites/pip.tga", 3, R32(Shogun(), SH_UI_SCALE));
}

void ForgetDefinitions(const char* const* badguys, u32 nb, const char* const* bullets, u32 nu,
                       const char* const* generics, u32 ng);
void ForgetAtlantic(void) {
  ForgetDefinitions(kBadguys, COUNT(kBadguys), kBullets, COUNT(kBullets), kGenerics, COUNT(kGenerics));
}

extern int g_graze_jammed;        // amaterasu.c: the jam blinds the sun's link only, not the engine
void AndamanReset(void);          // andaman.c: the shells, blasts and the Valhalla, clean
extern int g_an_slow, g_an_roam;  // andaman.c: her guns' rate and roaming (this mission's)
static int Hooked(void);          // (his harpoons in the Valhalla: her gauge slowed)

// ---- tuning -------------------------------------------------------------------------------------
#define HULL_Y       96             // the hull's centre, holding the top of the screen
#define FACE_Y       104            // ... nose to nose with the Valhalla, held back (its bow at ~184)
#define START_Y      (-230)         // ... where it waits in the storm wall
#define AWAY_Y       (-330)         // ... and where it withdraws to
#define VAL_X        310            // the Valhalla on station at the bottom right ...
#define VAL_Y        468
#define VAL_FACE_X   200            // ... and nose to nose, in the middle, come forward (her bow at ~200:
#define VAL_FACE_Y   440            //     more of her on the screen, her damage with it)
#define CALM_T       3600           // the calm, from the boss's start (a minute)
#define HP_MAX       10000
#define RAIL_DMG     500            // a railgun hit: moderate (the Valkyrie's went straight through in M7)
#define SUN_DMG16    650            // a sun strike
#define CRIPPLE_DMG  300
#define HP_WITHDRAW  1500           // the charge drives him off here ...
#define CHARGE_MIN   4800           // ... once it has lasted this long (80 s)
#define BAR_FULL     1500           // Volta's gauge: health taken off the hunters ...
#define BAR_EVERY    3              // ... and a point every this many frames
#define NPIP         12
#define NCYCLE       5
#define BREACH_HP    700            // a breach target in the charge (atlantic_cast.py btarget: the same)
#define NBREACH      12
#define NHUNT        16
#define NSHIP        7              // five ships on the sea, two Arche lifters over them (Mission 15's)
#define NSEA         5
#define ESC_HP       900            // the Valhalla's escorts
#define SHIP_HP      900
#define BOMB_DMG     320
#define BEAM_SHIP    4              // the Tenshu's beam on a ship, a frame
#define SHELL_ESC    70             // a shell on an escort
#define NRAIN        16
#define NHAZE        4
#define NWALL        3
#define NWRECK       5
#define NVDMG        24             // hits on the Valhalla's hull (art_damage.valhalla_points)
#define NKDMG        16             // ... on the Kurogane's (the Valhalla's guns)
#define NEDMG        3              // ... on each escort
#define ALONE_T      3000           // 50 s
#define AK_MIN       2400           // Akane: at least this long before Volta can call her shot ...
#define AK_MAX       6600           // ... and called at the latest here (110 s)
#define AK_HOME_Y    110
#define AK_FLEE_AT   80             // frames into the railgun's charge when she bolts
static const short kWindow[NCYCLE] = { 760, 700, 640, 580, 520 };   // the overload: shorter every time
// his gunports, down each flank (from the hull's centre; the last of each flank nearest the bow)
static const signed char kPortX[12] = { -56, -56, -56, -56, -56, -56, 56, 56, 56, 56, 56, 56 };
static const signed char kPortY[12] = { -40, -22, -4, 14, 32, 50, -40, -22, -4, 14, 32, 50 };
// where Volta's shots land, from the hull's centre (the belt, both flanks, the deck)
static const signed char kMarkX[8] = { -60, 58, -40, 44, -70, 66, -20, 24 };
static const signed char kMarkY[8] = { 10, -6, 30, 22, -20, -26, 44, -40 };

// Mission 7's gun commands (antarctica/chase_cast.py CMDS) -- only those aimed at the ship
enum { G_FAN = 1, G_STREAM, G_FLAK, G_MISSILES, G_LANCE, G_SWEEP, G_CURTAIN, G_BLOOM, G_BARRAGE, G_PEPPER,
       G_WRATH, G_SPIRAL, G_RAIN, G_TWIN, G_SNIPE, G_RAKE, G_GATE, G_PULSE, G_WEAVE, G_COMB, G_SNAKE, G_STAR };

// the Tenshu cannon (chase.c's, from the top)
enum { MG_OFF, MG_RISE, MG_LOCK, MG_FIRE, MG_SINK, MG_HELD };   // held: its aim thrown by the overload, stuck out
#define NMEGA        12
#define SEG          40
#define MEGA_HALF    15
#define MEGA_GRACE   30
#define MEGA_LOCK    80
#define MEGA_FIRE    170
#define MEGA_SINK    120
#define MEGA_FAST    36             // pulled down before the sun
#define MEGA_AT      (BAR_FULL * 6 / 10)   // it rises as Volta's gauge passes this
#define MEGA_SWEEP   (TURN / 900)

enum { PH_CALM = 1, PH_AKANE, PH_STORM, PH_FIRST, PH_CYCLE, PH_APPROACH, PH_FALL, PH_ALONE, PH_CHARGE,
       PH_WITHDRAW, PH_AFTER, PH_DONE };
enum { AK_IN = 1, AK_HOVER, AK_STRAFE, AK_FLEE, AK_GONE };
enum { WRECK_SHIP = NSHIP, ESC_TGT = 20 };   // a bomber's target: the wreck; the Tenshu's: her escorts (ESC_TGT + s)
enum { W_FILE, W_CROWS, W_BOMB, W_GUNSHIP };

typedef struct { u32 a, mark; s32 x, y, hp, head; int lost, away, sink; } Ship;
typedef struct { u32 bg; s32 hp; int bomber, tgt, mark; s32 x, y; } Hunter;

static struct {
  int on, phase, t, cycle, said;
  u32 frame, boss;
  // the dreadnought, what is seen of it ...
  u32 glow, aft, hull, streamer[2], nac[4], oni, hangar, mast, over;
  int k_on;                       // out of the storm wall and in the fight (made, parked, from the start)
  u32 tur_vis[4], can_vis[4];
  // ... its guns, invisible emitters, and the sun's targets (bow, keep)
  u32 tur[4], can[4], bow, wrath, core[2];
  int gun_cool, fill_cool, tur_turn, fill_turn, bow_turn, gun_turn, stun;
  s32 hx, hy, want_y;
  s32 hp;
  int face;                       // nose to nose with the Valhalla
  // the Tenshu cannon
  u32 hatch, mcannon, mret, mbeam[NMEGA], wreck;
  int mega, mega_t, mega_grace, mega_tgt, mega_fast, crippled, mega_up;
  s32 mega_ang;
  // breaches: where shots landed (offsets from the hull's centre), their fires, the charge's targets
  u32 breach[NBREACH], btgt[NBREACH];
  s32 bx[NBREACH], by[NBREACH];
  s32 bhp[NBREACH];
  int nbreach;
  // the overload
  int over_t, over_len, struck;
  // Volta: her gauge, her shot
  s32 bar;
  u32 ticked;                   // the frame the fight last ran (the HUD draws only while it does)
  int firing, mark, rail_t, rail_line, cripple_shot, bar_t;
  // Akane
  u32 mag, mglow;
  s32 mx, my, mtx, mvy, ak_max;
  int ak, ak_t, ak_cool, ak_turn, ak_side, ak_rail, ak_rail_t, ak_said;
  // the hunters
  Hunter hunt[NHUNT];
  int wave_cool, wave_k;
  // the convoy
  Ship ship[NSHIP];
  int turned, turn_t, lost_said, away_n;
  // the Valhalla: her escorts, the shells on her, the fires they leave
  s32 esc_hp[2];
  int esc_said, esc_lost;

  int shell_cool, kshell_cool;
  // shells that will leave their mark: the point they were aimed at, frames till they land
  struct { int k, t; } vpend[8], kpend[8];
  int vshot_n, kshot_n;
  u32 vdmg[NVDMG], kdmg[NKDMG], edmg[2][NEDMG];
  unsigned char vdmg_on[NVDMG];                     // (her holes are made at the start, parked: shown once hit)
  int nvdmg, nkdmg, nedmg[2];
  // the weather
  u32 rain[NRAIN], haze[NHAZE], wall[NWALL];
  s32 rain_off, wall_y, haze_x;
  int rain_on, haze_on, lit_t;
  // the fall: her two halves; the wreck
  int fall_t, fall_go, crash_line;
  u32 strip[VAL_NSTRIP];
  u32 wgun[7];                                      // her turrets and railgun, carried by the wreck
  s32 wgx[7], wgy[7], wga[7];
  int nwgun;
  s32 dcx, dcy, dca, dcv, dcs;                     // her wreck going down: centre, turn, sink, spin
  u32 wfire[NWRECK];
  s32 wx, wy;
  // the charge
  u32 bastion, dest[2], btur[3], ciws[4];
  s32 bsx, bsy, btur_ang[3], ciws_ang[4];
  int btur_flash[3], ciws_cool[4], btur_k;
  int bcool, charge_t, blive, charging;
  // his new attacks (Specials): emitters on his hull, what they need to be seen
  u32 atk_port[12], atk_bow, atk_mid, pfx[12];
  int pfx_t[12], pfx_f[12];
  int sp, sp_t, sp_cool, sp_turn, oni_force, vent_warn, mines_laid, hit_grace;
  s32 ram_dy;
  u32 light, cross[5];
  s32 light_ang, cross_x[5], cross_y[5];
  int light_lock, cross_eta[5], crossed;
  s32 cage_x[4], cage_y[4];
  int cage_open;
  u32 cpost[4], cbar[4][3];          // the cage drawn: its corners, three lengths of each side
  s32 wall_gx, wall_gy;
  u32 harp[2], chain[2][8];
  s32 harp_x[2], harp_y[2];
  int harp_state[2], harp_t[2];
  s32 ppx, ppy, pvx, pvy;         // the ship last frame, and its speed (lead fire)
  int end_line, queued, cue;       // cue: the line a beat waits for (on screen), its ticket
  int cue_t;
} D;

enum { SAID_HIDE = 1, SAID_WINDOW = 2, SAID_TENSHU = 4, SAID_CANCEL = 8, SAID_SHORTER = 16, SAID_VALHIT = 32,
       SAID_HURT = 64, SAID_STRIKE = 128 };

static int InWorld(void) { return R32(Shogun(), SH_WORLD_NO) == ATLANTIC && D.on; }

// For the desktop probes (runtime/probes/atlantic_run.py): phase, cycle, hull, gauge, overload, Tenshu.
volatile int g_at_dbg[6];

// ---- small helpers ---------------------------------------------------------------------------------
static u32 Gen(const char* name, s32 x, s32 y) { return AnSpawn(name, x, y); }
// A random index 0 .. n-1 (the engine's range can give its upper end too: an escort 2 wrote past a table)
static int Pick(int n) {
  const int v = Rnd(0, n);
  return v < 0 ? 0 : (v >= n ? n - 1 : v);
}
static s32 KeepY(void) { return D.hy + FX(K_KEEP_DY); }
static s32 BowY(void) { return D.hy + FX(K_HULL_BOW); }

static void Hurt(s32 dmg) {
  D.hp -= dmg;
  const s32 floor = D.phase >= PH_CHARGE ? HP_WITHDRAW - 200 : 1;   // (the charge: he breaks off, never dies)
  if (D.hp < floor) D.hp = floor;
}

// A breach where a shot landed (dx, dy from the hull's centre, whole units): its fire stays.
static void AddBreach(int dx, int dy, int big) {
  if (D.nbreach >= NBREACH) return;
  const int k = D.nbreach++;
  D.bx[k] = FX(dx);
  D.by[k] = FX(dy);
  D.breach[k] = Gen(big ? "*atlantic/boss/breach16_big" : "*atlantic/boss/breach16", D.hx + D.bx[k], D.hy + D.by[k]);
  AnBlast(D.hx + D.bx[k], D.hy + D.by[k]);
}

// ---- the weather -----------------------------------------------------------------------------------
static void Weather(void) {
  // the storm wall: down to the top of the screen as the front comes in, lit now and then
  if (D.phase >= PH_STORM && D.phase <= PH_AFTER) {
    if (!D.wall[0]) {
      D.wall_y = FX(-70);
      for (int k = 0; k < NWALL; k++) D.wall[k] = Gen("*atlantic/boss/wall", FX(60 + 140 * k), D.wall_y);
    }
    D.wall_y += Ease(FX(D.phase >= PH_AFTER ? -70 : (D.phase >= PH_FIRST ? -34 : 6)) - D.wall_y, FX(0.4));
    if (D.lit_t > 0) D.lit_t--;
    else if (Rnd(0, 400) == 0) { D.lit_t = 10; SB_ShakeWorld(); }
    for (int k = 0; k < NWALL; k++) {
      AnPut(D.wall[k], FX(60 + 140 * k), D.wall_y + FX(k == 1 ? 6 : 0));
      AnFrame(D.wall[k], D.lit_t > 0 && ((D.lit_t >> 1) & 1));
    }
  }
  // the jam's haze: a band drifting over the dreadnought's middle; thinned in the overload
  if (D.haze_on && !D.haze[0])
    for (int k = 0; k < NHAZE; k++) D.haze[k] = Gen("*atlantic/boss/haze", FX(-400), FX(-400));
  if (D.haze[0]) {
    D.haze_x += FX(0.25);
    if (D.haze_x >= FX(200)) D.haze_x -= FX(200);
    const int f = !D.haze_on ? 2 : (D.over_t > 0 ? (D.over_t < 40 || D.over_len - D.over_t < 40 ? 1 : 2) : 0);
    for (int k = 0; k < NHAZE; k++) {
      const s32 x = D.haze_x + FX(200 * (k & 1)) - FX(100);
      const s32 y = FX(150 + 46 * (k >> 1)) + SB_MulFP(FX(6), SB_Sin((s32)((D.frame + k * 60) << 16)));
      AnPut(D.haze[k], x, y);
      AnFrame(D.haze[k], f);
    }
  }
  // rain over everything (faint), stepped down the screen
  if (D.rain_on && !D.rain[0])
    for (int k = 0; k < NRAIN; k++) D.rain[k] = Gen("*atlantic/boss/rain", FX(-400), FX(-400));
  if (D.rain[0]) {
    D.rain_off += FX(5);
    if (D.rain_off >= FX(128)) D.rain_off -= FX(128);
    for (int k = 0; k < NRAIN; k++) {
      const int c = k & 3, r = k >> 2;
      AnPut(D.rain[k], FX(128 * c) - FX(10), FX(128 * r) - FX(64) + D.rain_off);
      AnFrame(D.rain[k], D.rain_on >= 2);
    }
  }
}

// ---- the convoy -----------------------------------------------------------------------------------
// up the left, clear of the Valhalla's wing; the last two are France's Arche
// lifters, flying low over the ships (paris/art_legion.arche)
static const short kShipX[NSHIP] = { 70, 112, 58, 100, 62, 152, 140 };
static const short kShipY[NSHIP] = { 236, 268, 296, 330, 366, 250, 318 };
// the order they get clear in, one more each window (the lifters are quicker)
static const unsigned char kAway[NSHIP] = { 5, 0, 6, 1, 2, 3, 4 };

static void ConvoySpawn(void) {
  for (int k = 0; k < NSHIP; k++) {
    Ship* s = &D.ship[k];
    s->x = FX(kShipX[k]);
    s->y = FX(kShipY[k]);
    s->hp = SHIP_HP;
    s->head = UP;
    s->a = Gen(k >= NSEA ? "*atlantic/boss/arche16" : "*atlantic/boss/transport", s->x, s->y);
  }
}

static int ShipLive(int k) { return k >= 0 && k < NSHIP && !D.ship[k].lost && !D.ship[k].away; }

static void ShipHit(int k, s32 dmg) {
  if (!ShipLive(k)) return;
  Ship* s = &D.ship[k];
  s->hp -= dmg;
  if (s->hp > 0) return;
  s->lost = 1;
  AnDrop(&s->mark);
  AnBlast(s->x, s->y);
  Boom("ExplosionL", s->x, s->y);
}

static int Away(int k) {
  for (int i = 0; i < D.away_n && i < NSHIP; i++) if (kAway[i] == k) return 1;
  return 0;
}

// On course up the screen (holding their place: the scroll carries them); once
// turned about, they wheel round to the left and come down the screen, and get
// clear one at a time.
static void Convoy(void) {
  if (D.turned) D.turn_t++;
  for (int k = 0; k < NSHIP; k++) {
    Ship* s = &D.ship[k];
    if (!s->a) continue;
    const s32 bob = SB_MulFP(FX(1.5), SB_Sin((s32)((D.frame * 3 + k * 50) << 15)));
    if (s->lost) {
      s->sink++;
      s->y += FX(0.2);
      if (s->sink > 160) { AnDrop(&s->a); continue; }
    } else if (D.turned) {
      // wheeling round through west to south, each a little after the last
      if (D.turn_t > k * 12 && s->head != DOWN) {
        s->head -= TURN / 300;
        if (s->head <= DOWN) s->head = DOWN;
      }
      const s32 v = k >= NSEA ? FX(0.26) : FX(0.2);
      s->x += SB_MulFP(SB_Cos(s->head), v);
      s->y += SB_MulFP(SB_Sin(s->head), v) + v;             // (the sea carrying them)
      if (s->x < FX(30)) s->x = FX(30);
      if (s->head == DOWN && !Away(k)) s->y -= v + v - FX(0.04);   // (turned: holding on, creeping down)
      if (s->y > FX(480)) { s->away = 1; AnDrop(&s->a); AnDrop(&s->mark); continue; }
    }
    AnPut(s->a, s->x, s->y + bob);
    AnTurn(s->a, s->head);
    if (k < NSEA) AnFrame(s->a, s->lost || s->hp < SHIP_HP / 3);
  }
}

static int PickShip(void) {
  int best = -1, n = 0;
  for (int k = 0; k < NSHIP; k++)
    if (ShipLive(k) && D.ship[k].y < FX(430)) { n++; if (Rnd(0, n) == 0) best = k; }
  return best;
}

static void TargetPos(int tgt, s32* x, s32* y) {
  if (tgt == WRECK_SHIP) { *x = D.wx; *y = D.wy; return; }
  if (tgt >= ESC_TGT && AnValEscortUp(tgt - ESC_TGT)) { AnValEscortPos(tgt - ESC_TGT, x, y); return; }
  if (ShipLive(tgt)) { *x = D.ship[tgt].x; *y = D.ship[tgt].y; return; }
  *x = PlayerX();
  *y = PlayerY();
}

// ---- the hunters -----------------------------------------------------------------------------------
static int HuntersUp(void) {
  int n = 0;
  for (int k = 0; k < NHUNT; k++) if (D.hunt[k].bg) n++;
  return n;
}

static Hunter* Track(u32 bg) {
  if (!bg) return 0;
  for (int k = 0; k < NHUNT; k++)
    if (!D.hunt[k].bg) {
      Hunter* h = &D.hunt[k];
      SB_ClearBytes(h, sizeof *h);
      h->bg = bg;
      h->hp = SB_GetBadGuyHealth(bg);
      h->mark = -1;                                // (its R21 mark, read once its script has run)
      return h;
    }
  return 0;
}

static void FileFrom(int left, s32 y, int n) {
  for (int i = 0; i < n && HuntersUp() < NHUNT; i++) {
    const s32 x = left ? FX(-20 - 22 * i) : FX(420 + 22 * i);
    const u32 bg = SB_SpawnBadGuy(BH, "*atlantic/scripts/kami_side", x, y - FX(10 * i));
    if (!bg) return;
    SetReg(bg, 4, FX(left ? 10 : 118));        // heading, a little down the screen
    SetReg(bg, 6, left ? FX(0.9) : -FX(0.9));  // its curl: down and back the way it came
    Track(bg);
  }
}

// Crows out of the hangar under the bow, fanned down the screen.
static void Crows(int n) {
  if (!D.k_on) return;
  for (int i = 0; i < n && HuntersUp() < NHUNT; i++) {
    const u32 bg = SB_SpawnBadGuy(BH, "*atlantic/scripts/crow16", D.hx, D.hy + FX(K_HANGAR_DY));
    if (!bg) return;
    static const signed char kFan[4] = { -24, 24, -8, 8 };     // (no division: a table)
    SetReg(bg, 4, FX(kFan[i & 3]));
    Track(bg);
  }
}

// Gunships in from the top corners, a pair.
static void Gunships(void) {
  for (int s = 0; s < 2 && HuntersUp() < NHUNT; s++) {
    const u32 bg = SB_SpawnBadGuy(BH, "*atlantic/scripts/gunship16", s ? FX(430) : FX(-30), FX(Rnd(60, 120)));
    if (!bg) return;
    SetReg(bg, 4, FX(s ? 100 : 28));           // in: down and across
    SetReg(bg, 5, FX(s ? 0 : 128));            // out: back the way it came
    Track(bg);
  }
}

static void Bomber(int left, int tgt) {
  if (tgt != WRECK_SHIP && !ShipLive(tgt)) return;
  const s32 x = left ? FX(-30) : FX(430), y = FX(Rnd(140, 220));
  Hunter* h = Track(SB_SpawnBadGuy(BH, "*atlantic/scripts/bomber", x, y));
  if (!h) return;
  h->bomber = 1;
  h->tgt = tgt;
  h->x = x;
  h->y = y;
  s32 tx, ty;
  TargetPos(tgt, &tx, &ty);
  if (tgt != WRECK_SHIP) {
    AnDrop(&D.ship[tgt].mark);
    D.ship[tgt].mark = Gen("*paris/boss/mark", tx, ty);
  }
}

// Each hunter's health lost (shot) fills Volta's gauge; bombers flown by the code.
static void Hunters(void) {
  for (int k = 0; k < NHUNT; k++) {
    Hunter* h = &D.hunt[k];
    if (!h->bg) continue;
    // gone -- or gone and its handle taken by something new (a craft that flew off the screen frees
    // its handle at once: the bow's paint target once took one, and was "hunted" by the autopilot)
    const int mark = Alive(h->bg) ? (int)(GetReg(h->bg, 21) >> 16) : -2;
    if (h->mark < 0 && (mark == 90 || mark == 91)) h->mark = mark;
    if (mark == -2 || (h->mark >= 0 && mark != h->mark)) {
      if (h->bomber && h->tgt != WRECK_SHIP && ShipLive(h->tgt)) AnDrop(&D.ship[h->tgt].mark);
      h->bg = 0;
      continue;
    }
    const s32 hp = SB_GetBadGuyHealth(h->bg);
    if (hp < h->hp) {
      if (!D.over_t) D.bar += Hooked() ? (h->hp - (hp > 0 ? hp : 0)) >> 1 : h->hp - (hp > 0 ? hp : 0);   // (not in the window; halved while hooked)
      h->hp = hp;
    }
    if (!h->bomber) continue;
    s32 tx, ty;
    TargetPos(h->tgt, &tx, &ty);
    if (h->tgt != WRECK_SHIP && !ShipLive(h->tgt)) {
      h->tgt = PickShip();
      if (h->tgt < 0) { Remove(h->bg); h->bg = 0; continue; }
      TargetPos(h->tgt, &tx, &ty);
    }
    const s32 a = AnAngle(h->x, h->y, tx, ty);
    h->x += SB_MulFP(SB_Cos(a), FX(1.05));
    h->y += SB_MulFP(SB_Sin(a), FX(1.05));
    Place(h->bg, h->x, h->y);
    Anim(h->bg, AnDist(tx - h->x, ty - h->y) < 60);
    if (h->tgt != WRECK_SHIP) AnPut(D.ship[h->tgt].mark, tx, ty);
    if (AnDist(tx - h->x, ty - h->y) < 14) {      // arrived: the bomb load on it
      if (h->tgt != WRECK_SHIP) { ShipHit(h->tgt, BOMB_DMG); AnDrop(&D.ship[h->tgt].mark); }
      AnBlast(tx, ty);
      Remove(h->bg);
      h->bg = 0;
    }
  }
}

// The waves: from the sides, the corners, the hangar -- more as the fight goes on.
// The quiet stretches (the storm coming in, the first shot, the approach, the fall): a simple
// file now and then so the screen is never empty.
static void Simple(void) {
  if (D.wave_cool > 0) { D.wave_cool--; return; }
  if (HuntersUp() >= 4 || InCutscene()) return;
  FileFrom(D.wave_k & 1, FX(Rnd(150, 250)), 4);
  if (++D.wave_k >= 8) D.wave_k = 0;
  D.wave_cool = 240;
}

static void Waves(void) {
  if (D.wave_cool > 0) { D.wave_cool--; return; }
  int cap = 8 + (D.cycle > 2 ? 2 : 0);
  if (D.phase == PH_CALM) cap = 8;
  if (D.phase == PH_AKANE) cap = 4;
  if (D.phase == PH_ALONE) cap = 12;
  if (D.phase == PH_CHARGE) cap = 6;
  if (HuntersUp() >= cap) return;
  static const unsigned char kKind[8] = { W_FILE, W_CROWS, W_FILE, W_BOMB, W_GUNSHIP, W_FILE, W_CROWS, W_BOMB };
  int kind = kKind[D.wave_k];
  if (++D.wave_k >= 8) D.wave_k = 0;
  const int left = D.wave_k & 1;
  if (kind == W_CROWS && (!D.k_on || D.over_t > 0)) kind = W_FILE;
  if (kind == W_BOMB && D.phase < PH_STORM) kind = W_GUNSHIP;
  if (D.phase == PH_AKANE) kind = W_FILE;
  switch (kind) {
  case W_FILE:
    FileFrom(left, FX(Rnd(150, 260)), D.phase == PH_AKANE ? 3 : 5 + (D.cycle >= 3));
    if (D.phase == PH_CALM && (D.wave_k & 2)) FileFrom(left ^ 1, FX(Rnd(150, 260)), 4);   // (both sides at once)
    break;
  case W_CROWS:
    Crows(D.cycle >= 3 || D.phase == PH_ALONE ? 4 : 3);
    break;
  case W_GUNSHIP:
    Gunships();
    break;
  case W_BOMB: {
    const int n = D.phase == PH_ALONE ? 3 : (D.cycle >= 3 ? 2 : 1);
    for (int i = 0; i < n; i++) {
      const int tgt = D.phase == PH_ALONE ? WRECK_SHIP : PickShip();
      if (tgt >= 0) Bomber(left ^ i, tgt);
    }
    break;
  }
  }
  int cool = 140 - 12 * D.cycle;
  if (D.phase == PH_CALM) cool = 110;
  if (D.phase == PH_AKANE) cool = 360;
  if (D.phase == PH_ALONE) cool = 100;
  D.wave_cool = cool;
}

// ---- Akane: the delay -------------------------------------------------------------------------------
// The Magpie as at Seoul (magpie.c): it hovers at the top working through its
// guns, banks across strafing, and makes afterburner runs when hurt. Its
// body is the badguy; its engines' glow laid over it.
static int AkThirds(void) {
  const s32 hp = SB_GetBadGuyHealth(D.mag);
  if (hp * 3 > D.ak_max * 2) return 2;
  if (hp * 3 > D.ak_max) return 1;
  return 0;
}

static void AkStrafeStart(void) {
  D.ak = AK_STRAFE;
  D.ak_side = -D.ak_side;
  D.mtx = FX(D.ak_side > 0 ? 304 : 96);
  Order(D.mag, C_CARPET);
}

static void AkHover(void) {
  const s32 cx = D.mtx ? D.mtx : FX(200);
  const s32 wantx = cx + SB_MulFP(SB_Sin((s32)(D.ak_t << 15)), FX(26));
  const s32 wanty = FX(AK_HOME_Y) + SB_MulFP(SB_Sin((s32)(D.ak_t << 16)), FX(8));
  D.mx += Ease(wantx - D.mx, FX(1.5));
  D.my += Ease(wanty - D.my, FX(1));
  if (InCutscene() || Busy(D.mag)) return;
  if (D.ak_cool > 0) { D.ak_cool--; return; }
  const int th = AkThirds();
  if (th == 0) { AkStrafeStart(); return; }
  static const unsigned char kHealthy[] = { C_VULCAN, C_SPREAD, C_MISSILES, 0 };
  static const unsigned char kHurt[] = { C_VULCAN, C_FLARES, C_SPREAD, C_MISSILES, 0, C_VULCAN, C_FLARES, 0 };
  const unsigned char* list = th == 2 ? kHealthy : kHurt;
  const int n = th == 2 ? (int)sizeof kHealthy : (int)sizeof kHurt;
  const int seq = D.ak_turn;
  if (++D.ak_turn >= n) D.ak_turn = 0;
  const int cmd = list[seq >= n ? 0 : seq];
  if (!cmd) { AkStrafeStart(); return; }
  Order(D.mag, cmd);
  D.ak_cool = th == 2 ? 34 : 24;
}

static void AkStrafe(void) {
  const s32 d = D.mtx - D.mx;
  const s32 speed = AkThirds() == 0 ? FX(2.6) : FX(2.0);
  D.mx += d > speed ? speed : (d < -speed ? -speed : d);
  D.my += Ease(FX(AK_HOME_Y + 14) - D.my, FX(1));
  Anim(D.mag, A_CHARGE);                           // afterburner
  if (d > FX(2) || d < -FX(2)) return;
  Stop(D.mag);
  D.ak = AK_HOVER;
  D.ak_cool = AkThirds() == 0 ? 10 : 30;
}

static void Akane(void) {
  if (!D.mag) return;
  D.ak_t++;
  const s32 hp = SB_GetBadGuyHealth(D.mag);
  // she is not to die here: under a quarter she can't be hurt (the railgun is coming for her)
  Guard(D.mag, InCutscene() || D.ak == AK_IN || D.ak == AK_FLEE || hp * 4 < D.ak_max);
  switch (D.ak) {
  case AK_IN:
    Anim(D.mag, A_CHARGE);
    D.my += FX(2.2);
    if (D.my >= FX(AK_HOME_Y)) {
      D.my = FX(AK_HOME_Y);
      D.ak = AK_HOVER;
      D.ak_cool = 40;
      D.mtx = FX(200);
      Anim(D.mag, A_IDLE);
    }
    break;
  case AK_HOVER:
    AkHover();
    break;
  case AK_STRAFE:
    AkStrafe();
    break;
  case AK_FLEE:
    // afterburners: straight up and away, faster and faster
    Anim(D.mag, A_CHARGE);
    if (D.mvy < FX(9)) D.mvy += FX(0.35);
    D.my -= D.mvy;
    D.mx += Ease(FX(260) - D.mx, FX(2));
    if (D.my < FX(-180)) {
      Place(D.mag, FX(200), FX(-460));
      Remove(D.mag);
      D.mag = 0;
      g_an_spare = 0;
      GlowDrop(&D.mglow);
      D.ak = AK_GONE;
      return;
    }
    break;
  }
  if (D.ak_t == 2000) AnLine(AL_DOC_STALL);
  // Volta's shot: once she's hurt enough or has stalled long enough
  if (!D.ak_rail && D.ak != AK_IN && D.ak_t > AK_MIN && (hp * 5 < D.ak_max * 2 || D.ak_t > AK_MAX)) {
    D.ak_rail = 1;
    D.rail_line = AnLine(AL_VOLTA_RAIL_AKANE);
    AnValRailAt(D.mx, D.my, D.rail_line);
  }
  if (D.ak_rail && D.ak != AK_FLEE) {
    AnValRailMove(D.mx, D.my);
    if (AnValRailState() == RAIL_CHARGING && ++D.ak_rail_t >= AK_FLEE_AT) {
      // a moment before it fires, she's gone
      D.ak = AK_FLEE;
      D.mvy = FX(0.5);
      Stop(D.mag);
      AnLineNow(AL_AKANE_ESCAPE);
    }
  }
  g_an_spare = D.mag;                              // (the Valhalla's guns leave her be: she is to get away)
  Place(D.mag, D.mx, D.my);
  static const signed char kMap[7] = { 0, 0, 0, GLOW_OFF, 0, GLOW_OFF, GLOW_OFF };
  GlowPut(D.mglow, D.mx, D.my, MAGPIE_GLOW_X, MAGPIE_GLOW_Y, GlowFor(Act(D.mag), kMap, 7));
  g_autopilot_target = D.ak == AK_FLEE ? 0 : D.mag;
}

static void AkaneStart(void) {
  D.mx = FX(200);
  D.my = FX(-90);
  D.mag = SB_SpawnBadGuy(BH, "*seoul/boss/magpie", D.mx, D.my);
  D.mglow = GlowSpawn(BH, "*seoul/boss/magpie_glow");
  D.ak = AK_IN;
  D.ak_side = 1;
  D.ak_t = 0;
  if (D.mag) {
    Place(D.mag, D.mx, D.my);
    D.ak_max = SB_GetBadGuyHealth(D.mag);
    R32(D.boss, B_MAXHP) = D.ak_max;
  }
}

// ---- the dreadnought ------------------------------------------------------------------------------
// What is seen of it, made at the very start (parked out of sight until the storm), so that
// nothing made later can take a slot under it -- and the sun's pieces made right after it
// (amaterasu.c AmaSunPrespawn), so the beam always draws over it.
static void BuildSeen(void) {
  const s32 x = D.hx, y = D.hy;
  // spawned bottom to top: later generics draw over earlier ones
  D.glow = Gen("*atlantic/boss/underglow16", x, y);
  D.aft = Gen("*atlantic/boss/aft16", x, y + FX(K_AFT_DY));
  D.hull = Gen("*atlantic/boss/hull16", x, y);
  for (int s = 0; s < 2; s++)
    D.streamer[s] = Gen("*atlantic/boss/streamer16", x + FX(s ? K_STREAMER_DX : -K_STREAMER_DX), y + FX(K_STREAMER_DY));
  for (int i = 0; i < 4; i++) D.nac[i] = Gen("*atlantic/boss/nacelle16", x + FX(kNacX[i]), y + FX(kNacY[i]));
  D.hangar = Gen("*atlantic/boss/hangar16", x, y + FX(K_HANGAR_DY));
  D.oni = Gen("*atlantic/boss/oni16", x, y + FX(K_ONI_DY));
  for (int i = 0; i < 4; i++) D.can_vis[i] = Gen("*antarctica/boss/cannon_vis", x + FX(kCanX[i]), y + FX(kCanY[i]));
  for (int i = 0; i < 4; i++) {
    const s32 tx = x + FX(kTurX[i]), ty = y + FX(kTurY[i]);
    D.tur_vis[i] = Gen(i < 2 ? "*antarctica/boss/turret_vis" : "*antarctica/boss/turret_big_vis", tx, ty);
  }
  D.mast = Gen("*atlantic/boss/jam_mast", x, KeepY() - FX(22));
  D.over = Gen("*atlantic/boss/overload", FX(-400), FX(-400));
  // his new attacks' pieces seen on him: the gunports' flashes, the searchlight
  for (int k = 0; k < 12; k++) { D.pfx[k] = Gen("*atlantic/boss/portflash", FX(200), FX(200)); AnPut(D.pfx[k], FX(-300), FX(-300)); }
  D.light = Gen("*atlantic/boss/searchlight", FX(200), FX(200));
  AnPut(D.light, FX(-300), FX(-300));
  AmaSunPrespawn();
}

// Its guns (badguys: they draw under every generic whatever their order), and the sun's targets.
static void BuildGuns(void) {
  const s32 x = D.hx, y = D.hy;
  for (int i = 0; i < 4; i++) {
    D.can[i] = SB_SpawnBadGuy(BH, "*antarctica/boss/flak", x + FX(kCanX[i]), y + FX(kCanY[i]));
    D.tur[i] = SB_SpawnBadGuy(BH, "*antarctica/boss/turret", x + FX(kTurX[i]), y + FX(kTurY[i]));
  }
  D.bow = SB_SpawnBadGuy(BH, "*antarctica/boss/bow_gun", x, y + FX(K_BOW_DY));
  // the emitters of his new attacks (atk_cast.py): down his flanks' gunports, at the bow, at his centre
  for (int k = 0; k < 12; k++) D.atk_port[k] = SB_SpawnBadGuy(BH, "*atlantic/boss/atk16", x, y);
  D.atk_bow = SB_SpawnBadGuy(BH, "*atlantic/boss/atk16", x, y);
  D.atk_mid = SB_SpawnBadGuy(BH, "*atlantic/boss/atk16", x, y);
  D.wrath = SB_SpawnBadGuy(BH, "*antarctica/boss/wrath", x, KeepY());
  // the sun's target: the bow (the keep is too near the top of the screen: the beam onto it was
  // two tiles long, hidden under the storm wall and the dialog box)
  D.core[0] = SB_SpawnBadGuy(BH, "*atlantic/boss/core", x, BowY() - FX(24));
}

static void Place16(void) {
  const s32 x = D.hx, y = D.hy;
  const int stage = D.hp > HP_MAX * 2 / 3 ? 0 : (D.hp > HP_MAX / 3 ? 1 : 2);   // (and the Valhalla's hits on top)
  const int hurt = stage;
  AnPut(D.glow, x, y);
  AnFrame(D.glow, D.over_t > 0 ? 1 : (D.mega == MG_FIRE ? 2 : (D.mega ? 3 : 0)));
  AnPut(D.aft, x, y + FX(K_AFT_DY));
  AnFrame(D.aft, hurt);
  AnPut(D.hull, x, y);
  AnFrame(D.hull, stage);
  for (int s = 0; s < 2; s++) AnPut(D.streamer[s], x + FX(s ? K_STREAMER_DX : -K_STREAMER_DX), y + FX(K_STREAMER_DY));
  for (int i = 0; i < 4; i++) AnPut(D.nac[i], x + FX(kNacX[i]), y + FX(kNacY[i]));
  AnPut(D.hangar, x, y + FX(K_HANGAR_DY));
  AnPut(D.oni, x, y + FX(K_ONI_DY));
  AnFrame(D.oni, D.bow && Busy(D.bow) ? 6 : (hurt ? 5 : 0));
  const s32 px = PlayerX(), py = PlayerY();
  for (int i = 0; i < 4; i++) {
    const s32 tx = x + FX(kTurX[i]), ty = y + FX(kTurY[i]);
    if (D.tur[i]) Place(D.tur[i], tx, ty);
    AnPut(D.tur_vis[i], tx, ty);
    AnTurn(D.tur_vis[i], AnAngle(tx, ty, px, py));
    if (D.tur[i]) AnFrame(D.tur_vis[i], D.over_t > 0 ? 0 : GetReg(D.tur[i], REG_ANIM) >> 16);
    const s32 cx = x + FX(kCanX[i]), cy = y + FX(kCanY[i]);
    if (D.can[i]) Place(D.can[i], cx, cy);
    AnPut(D.can_vis[i], cx, cy);
    AnTurn(D.can_vis[i], AnAngle(cx, cy, px, py));
    if (D.can[i]) AnFrame(D.can_vis[i], GetReg(D.can[i], REG_ANIM) >> 16 ? 1 : 0);
  }
  if (D.bow) Place(D.bow, x, y + FX(K_BOW_DY));
  if (D.wrath) Place(D.wrath, x, KeepY());
  if (D.core[0]) Place(D.core[0], x, BowY() - FX(24));
  // "Invincible = 255" counts down: kept untouchable every frame (the bow's paint target was
  // shot down by the Valhalla's own guns, and the sun lost it)
  for (int i = 0; i < 4; i++) {
    if (D.tur[i]) Guard(D.tur[i], 1);
    if (D.can[i]) Guard(D.can[i], 1);
  }
  if (D.bow) Guard(D.bow, 1);
  if (D.wrath) Guard(D.wrath, 1);
  if (D.core[0]) Guard(D.core[0], 1);
  // ... and kept whole (whatever gets through: the pilot's guns, the Valhalla's)
  for (int i = 0; i < 4; i++) {
    if (D.tur[i]) SB_SetBadGuyHealth(D.tur[i], 30000);
    if (D.can[i]) SB_SetBadGuyHealth(D.can[i], 30000);
  }
  if (D.bow) SB_SetBadGuyHealth(D.bow, 30000);
  if (D.wrath) SB_SetBadGuyHealth(D.wrath, 30000);
  if (D.core[0]) SB_SetBadGuyHealth(D.core[0], 30000);
  AnPut(D.mast, x, KeepY() - FX(22));
  AnFrame(D.mast, D.over_t > 0 ? (D.over_len - D.over_t < 50 ? 2 : 1) : 0);
  if (D.over_t > 0 && ((D.frame & 1) || D.vent_warn > 0)) AnPut(D.over, x, y);
  else AnPut(D.over, FX(-400), FX(-400));
  for (int k = 0; k < D.nbreach; k++) {
    AnPut(D.breach[k], x + D.bx[k], y + D.by[k]);
    if (D.btgt[k]) Place(D.btgt[k], x + D.bx[k], y + D.by[k]);
  }
  for (int k = 0; k < 12; k++) {
    const s32  px_ = x + FX(kPortX[k]), py_ = y + FX(kPortY[k]);
    if (D.atk_port[k]) { Place(D.atk_port[k], px_, py_); Guard(D.atk_port[k], 1); SB_SetBadGuyHealth(D.atk_port[k], 30000); }
    if (D.pfx_t[k] > 0) { D.pfx_t[k]--; AnPut(D.pfx[k], px_, py_); AnFrame(D.pfx[k], D.pfx_f[k]); }
    else AnPut(D.pfx[k], FX(-300), FX(-300));
  }
  if (D.atk_bow) { Place(D.atk_bow, x, BowY() - FX(6)); Guard(D.atk_bow, 1); SB_SetBadGuyHealth(D.atk_bow, 30000); }
  if (D.atk_mid) { Place(D.atk_mid, x, y + FX(20)); Guard(D.atk_mid, 1); SB_SetBadGuyHealth(D.atk_mid, 30000); }
  if (D.oni_force) AnFrame(D.oni, D.oni_force);
  if (D.wreck) {
    AnPut(D.wreck, x, KeepY() + FX(6));
    AnTurn(D.wreck, DOWN);
  }
  for (int k = 0; k < NKDMG; k++) if (D.kdmg[k]) AnPut(D.kdmg[k], x + FX(kKDmgX[k]), y + FX(kKDmgY[k]));
}

// The hull sways a little across (hardly at all nose to nose), and bobs; it eases to where it is wanted.
static void Hold16(void) {
  const s32 sway = SB_MulFP(FX(D.face ? 6 : 26), SB_Sin((s32)(D.frame << 14)));
  D.hx += Ease(FX(D.face ? VAL_FACE_X : 200) + sway - D.hx, FX(0.3));
  const s32 bob = SB_MulFP(FX(3), SB_Sin((s32)(D.frame << 15)));
  D.hy += Ease(D.want_y + D.ram_dy + bob - D.hy, FX(D.phase == PH_WITHDRAW ? 0.9 : (D.phase == PH_APPROACH ? 0.35 : 1.2)));
}

// ---- his guns ----------------------------------------------------------------------------------------
static void StopGuns(void) {
  for (int i = 0; i < 4; i++) {
    if (D.tur[i]) Stop(D.tur[i]);
    if (D.can[i]) Stop(D.can[i]);
  }
  if (D.bow) Stop(D.bow);
}

static int Light(int cmd) {
  return cmd == G_PULSE || cmd == G_WEAVE || cmd == G_SPIRAL || cmd == G_PEPPER || cmd == G_FLAK;
}

static void Guns(void) {
  if (D.stun > 0) { D.stun--; return; }
  if (D.over_t > 0 || InCutscene()) return;
  // one heavy attack at a time until the Valhalla falls (the Tenshu, the hunters and the
  // shelling are the rest of the pressure); two once he turns on the wreck
  const int cap_heavy = D.phase >= PH_CHARGE ? 2 : 1;
  const int cap_light = D.cycle >= 3 || D.phase >= PH_ALONE ? 2 : 1;
  int heavy = 0, light = 0;
  for (int i = 0; i < 4; i++) {
    if (D.tur[i] && Busy(D.tur[i])) {
      if (Light(GetReg(D.tur[i], 21) >> 16)) light++;
      else heavy++;
    }
    if (D.can[i] && Busy(D.can[i])) light++;
  }
  if (D.bow && Busy(D.bow)) heavy++;
  // the light ones: the gunwale cannons, and slow patterns from a free turret
  if (D.fill_cool > 0) D.fill_cool--;
  else if (light < cap_light) {
    if (D.fill_turn & 1) {
      static const unsigned char kCan[4] = { G_PEPPER, G_FLAK, G_PEPPER, G_FLAK };   // (not Rain: it throws upwards)
      const int i = (D.fill_turn >> 1) & 3;
      if (D.can[i] && !Busy(D.can[i])) Order(D.can[i], kCan[(D.fill_turn >> 3) & 3]);
    } else {
      static const unsigned char kFill[] = { G_PULSE, G_WEAVE, G_SPIRAL, G_PULSE };   // (not Snake: it swings up)
      for (int k = 0; k < 4; k++) {
        const int i = (D.fill_turn / 2 + k) & 3;
        if (!D.tur[i] || Busy(D.tur[i])) continue;
        Order(D.tur[i], kFill[(D.fill_turn >> 1) & 3]);
        break;
      }
    }
    D.fill_turn++;
    D.fill_cool = D.phase >= PH_ALONE ? 100 : 120 - 8 * D.cycle;
  }
  if (D.gun_cool > 0) { D.gun_cool--; return; }
  if (heavy >= cap_heavy || D.sp) return;
  int i = D.tur_turn & 3;
  for (int k = 0; k < 4 && (!D.tur[i] || Busy(D.tur[i])); k++) i = (i + 1) & 3;
  const int tur_free = D.tur[i] && !Busy(D.tur[i]);
  const int bow_free = D.bow && !Busy(D.bow);
  if (!tur_free && !bow_free) return;
  if (bow_free && (++D.gun_turn >= 3 || !tur_free)) {
    D.gun_turn = 0;
    static const unsigned char kBow[] = { G_LANCE, G_STAR, G_CURTAIN, G_BLOOM, G_LANCE, G_CURTAIN };
    Order(D.bow, kBow[D.bow_turn]);
    if (++D.bow_turn >= 6) D.bow_turn = 0;
  } else {
    static const unsigned char kTur[] = { G_FAN, G_STREAM, G_TWIN, G_SNIPE, G_STAR, G_FAN, G_MISSILES, G_STREAM };
    Order(D.tur[i], kTur[(D.tur_turn >> 2) & 7]);
    D.tur_turn++;
  }
  D.gun_cool = D.phase >= PH_ALONE ? 80 : 96 - 8 * D.cycle;
}

// ---- his new attacks (Specials) ------------------------------------------------------------------------------
// Each asks a different skill of the pilot and warns before it lands (docs/M16_DESIGN.md 6.2,
// the user's list of 2026-10-27, all built to be tried and pruned): brought in by the cycle, one
// at a time, his guns' heavy attacks held while one runs.
//   broadside ladder  his gunports light in order down a flank and fire rods across the screen,
//                     the flanks out of step (an X), the gap moving from volley to volley
//   oni roar          the mask wakes, then three crescents, fast to slow, gaps in different places
//   Kanabo ram        (nose to nose) the bow lunges, a ring off it with its gap away from the ship
//   hesitation volley a spread that stops dead, hangs, re-aims at the ship and dashes
//   lead fire         shots aimed where the ship will be, not where it is
//   searchlight       a cone tracks the ship; held on it two seconds, a cross of mortar shells
//   lightning cage    four marks round the ship, lightning along three sides: find the open one
//   static mines      off the jam mast, pulsing, bursting into rings (and laid before a window)
//   shield wall       a line of his craft abreast, one gap, a stream through it
//   harpoons          (nose to nose) grapples into the Valhalla: chains across the screen, her
//                     gauge slowed while she's hooked; shoot the heads
//   backlash          (in Overload) the window's end warned, then rings off the whole hull
enum { SP_NONE, SP_BROAD, SP_ROAR, SP_RAM, SP_HES, SP_LEAD, SP_SEARCH, SP_CAGE, SP_MINES, SP_WALL, SP_HARPOON, SP_N };
enum { A_BROAD = 1, A_ROAR, A_RINGGAP, A_HESITATE, A_LEAD, A_VENT, A_STREAM };   // (atk_cast.py CMDS)
static const unsigned char kSpFrom[SP_N] = { 0, 2, 2, 2, 3, 4, 4, 4, 3, 3, 3 };   // the cycle each comes in
static const unsigned char kSpOrder[SP_N - 1] = { SP_BROAD, SP_ROAR, SP_HES, SP_RAM, SP_MINES, SP_WALL, SP_CAGE,
                                                  SP_HARPOON, SP_SEARCH, SP_LEAD };

// The ship struck by something the code checks (a chain, the cage, a mortar): once, then a grace.
static void HitShip(void) {
  const u32 sh = Shogun();
  if (D.hit_grace > 0 || *(volatile unsigned short*)(sh + SH_HITLOCK) || R32(sh, SH_HITLOCK2) || InCutscene()) return;
  SB_StrikePlayer(sh + SH_PLAYER, 0, sh + SH_HUD);
  D.hit_grace = 40;
}

// The ship within r of the segment (x0, y0)-(x1, y1).
static int NearSegment(s32 x0, s32 y0, s32 x1, s32 y1, int r) {
  const s32 a = AnAngle(x0, y0, x1, y1);
  const s32 ux = SB_Cos(a), uy = SB_Sin(a);
  const s32 rx = PlayerX() - x0, ry = PlayerY() - y0;
  const s32 along = SB_MulFP(rx, ux) + SB_MulFP(ry, uy);
  const s32 perp = SB_MulFP(rx, uy) - SB_MulFP(ry, ux);
  return along > 0 && along < FX(AnDist(x1 - x0, y1 - y0)) && perp < FX(r) && perp > -FX(r);
}

static int SpOk(int sp) {
  if (D.phase == PH_APPROACH || D.phase == PH_FALL || D.phase >= PH_WITHDRAW) return 0;
  const int c = D.phase >= PH_ALONE ? 9 : D.cycle;
  if (c < kSpFrom[sp]) return 0;
  if ((sp == SP_RAM || sp == SP_HARPOON) && (!D.face || AnValGone() || D.phase >= PH_ALONE)) return 0;
  return 1;
}

static void SpEnd(void) {
  D.sp = SP_NONE;
  D.oni_force = 0;
  D.ram_dy = 0;
  AnPut(D.light, FX(-300), FX(-300));
  D.sp_cool = D.phase >= PH_ALONE ? 200 : 300 - 25 * D.cycle;
}

static void Mines(void) {
  for (int k = 0; k < 6; k++) {
    const u32 bg = SB_SpawnBadGuy(BH, "*atlantic/boss/mine16", D.hx, KeepY());
    if (bg) SetReg(bg, 4, FX(30 + 14 * k));       // fanned down the screen
  }
}

static void Broadside(void) {
  // six volleys, 40 frames apart, alternate flanks; in each the ports light front to back
  // (a flash, then the shot), one of them dark: the gap
  static const unsigned char kGap[6] = { 1, 3, 2, 4, 3, 1 };
  for (int v = 0; v < 6; v++) {
    const int t = D.sp_t - v * 40;
    if (t < 0 || t > 40) continue;
    const int right = v & 1;
    for (int j = 0; j < 6; j++) {
      if (j == kGap[v]) continue;
      const int port = (right ? 6 : 0) + 5 - j;   // (front = toward the bow, down the screen)
      if (t == 3 * j) { D.pfx_t[port] = 20; D.pfx_f[port] = 1; }
      if (t == 20 + 3 * j) {
        D.pfx_t[port] = 6;
        D.pfx_f[port] = 2;
        OrderAt(D.atk_port[port], A_BROAD, right ? DOWN + TURN / 8 : DOWN - TURN / 8);
      }
    }
  }
  if (D.sp_t > 6 * 40 + 40) SpEnd();
}

static void Roar(void) {
  const int t = D.sp_t;
  if (t < 40) D.oni_force = t < 14 ? 1 : (t < 28 ? 2 : 3);    // waking: the eyes kindle, the jaw drops
  if (t == 40) { D.oni_force = 6; OrderAt(D.atk_bow, A_ROAR, DOWN); SB_ShakeWorld(); }
  if (t >= 100) SpEnd();
}

static void Ram(void) {
  const int t = D.sp_t;
  if (t < 30) D.ram_dy += Ease(FX(20) - D.ram_dy, FX(1.5));
  if (t == 30) {
    SB_ShakeWorld();
    SB_ShakeWorld();
    Boom("ExplosionL", D.hx, BowY());
    OrderAt(D.atk_bow, A_RINGGAP, AnAngle(D.hx, BowY(), PlayerX(), PlayerY()) + TURN / 2);
  }
  if (t > 30) D.ram_dy += Ease(-D.ram_dy, FX(0.8));
  if (t >= 100) SpEnd();
}

static void Hesitation(void) {
  if (D.sp_t == 1) OrderAt(D.atk_mid, A_HESITATE, DOWN);
  if (D.sp_t == 100) OrderAt(D.atk_bow, A_HESITATE, DOWN);
  if (D.sp_t >= 220) SpEnd();
}

static void Lead(void) {
  if (D.sp_t == 1 || D.sp_t == 30 || D.sp_t == 60 || D.sp_t == 90) {
    const int port = (D.sp_t & 2) ? 11 : 5;          // the foremost port of one flank, then the other
    const s32 ex = D.hx + FX(kPortX[port]), ey = D.hy + FX(kPortY[port]);
    const int frames = DivSmall(AnDist(PlayerX() - ex, PlayerY() - ey), 4) + 1;   // (the shell flies 4 a frame)
    const s32 tx = PlayerX() + D.pvx * frames, ty = PlayerY() + D.pvy * frames;
    D.pfx_t[port] = 8;
    D.pfx_f[port] = 2;
    OrderAt(D.atk_port[port], A_LEAD, AnAngle(ex, ey, tx, ty));
  }
  if (D.sp_t >= 140) SpEnd();
}

#define LIGHT_HALF 115               // the cone's sprite: its lamp at one end, 230 long (art_damage.searchlight)
static void Searchlight(void) {
  const s32 lx = D.hx, ly = BowY() - FX(10);       // (the lamp on his bow: from the keep, half the cone was off the top)
  if (D.sp_t == 1) { D.light_ang = DOWN; D.light_lock = 0; D.crossed = 0; }
  if (!D.crossed) {
    const s32 want = AnAngle(lx, ly, PlayerX(), PlayerY());
    D.light_ang = AnTurnTo(D.light_ang, want, TURN / 520);
    const int in = AnOff(D.light_ang, want) < TURN / 48 && AnDist(PlayerX() - lx, PlayerY() - ly) < 230;
    if (in) D.light_lock++;
    else if (D.light_lock > 0) D.light_lock -= 2;
    if (D.light_lock < 0) D.light_lock = 0;
    AnPut(D.light, lx + SB_Cos(D.light_ang) * LIGHT_HALF, ly + SB_Sin(D.light_ang) * LIGHT_HALF);
    AnTurn(D.light, D.light_ang);
    AnFrame(D.light, D.light_lock > 60);
    if (D.light_lock >= 120) {
      // the cross: five shells onto where it held the ship, their marks first
      static const signed char kCx[5] = { 0, -36, 36, 0, 0 }, kCy[5] = { 0, 0, 0, -36, 36 };
      D.crossed = 1;
      AnPut(D.light, FX(-300), FX(-300));
      for (int k = 0; k < 5; k++) {
        D.cross_x[k] = PlayerX() + FX(kCx[k]);
        D.cross_y[k] = PlayerY() + FX(kCy[k]);
        AnPut(D.cross[k], D.cross_x[k], D.cross_y[k]);
        AnFrame(D.cross[k], 0);
        D.cross_eta[k] = 70 + 6 * k;
        AnShoot(lx, ly, D.cross_x[k], D.cross_y[k], 5, SHOT_BIG, TAG_NONE, 0, 0);
      }
    } else if (D.sp_t > 300) {
      SpEnd();                                     // never caught the ship in five seconds: it gives up
    }
    return;
  }
  int left = 0;
  for (int k = 0; k < 5; k++) {
    if (D.cross_eta[k] <= 0) continue;
    left++;
    AnFrame(D.cross[k], D.cross_eta[k] < 20);      // (bright as it lands)
    if (--D.cross_eta[k] > 0) continue;
    Boom("ExplosionL", D.cross_x[k], D.cross_y[k]);
    AnBlast(D.cross_x[k], D.cross_y[k]);
    if (AnDist(PlayerX() - D.cross_x[k], PlayerY() - D.cross_y[k]) < 26) HitShip();
    AnPut(D.cross[k], FX(-300), FX(-300));
  }
  if (!left) SpEnd();
}

static void Cage(void) {
  const int t = D.sp_t;
  if (t == 1) {
    static const signed char kX[4] = { -52, 52, 52, -52 }, kY[4] = { -52, -52, 52, 52 };
    for (int k = 0; k < 4; k++) { D.cage_x[k] = PlayerX() + FX(kX[k]); D.cage_y[k] = PlayerY() + FX(kY[k]); }
    D.cage_open = Pick(4);
    for (int k = 0; k < 4; k++) {
      if (k == D.cage_open) continue;
      const int n = k + 1 > 3 ? 0 : k + 1;
      AmaBolt(D.cage_x[k], D.cage_y[k], D.cage_x[n], D.cage_y[n], 70);
    }
  }
  if (t >= 70 && t <= 96)
    for (int k = 0; k < 4; k++) {
      if (k == D.cage_open) continue;
      const int n = k + 1 > 3 ? 0 : k + 1;
      if (NearSegment(D.cage_x[k], D.cage_y[k], D.cage_x[n], D.cage_y[n], 9)) HitShip();
    }
  const int show = t < 98;
  const int bright = t > 50 && ((t >> (t > 62 ? 1 : 2)) & 1);   // (flickering faster as it comes)
  for (int k = 0; k < 4; k++) {
    AnPut(D.cpost[k], show ? D.cage_x[k] : FX(-300), show ? D.cage_y[k] : FX(-300));
    AnFrame(D.cpost[k], bright);
    const int n = k + 1 > 3 ? 0 : k + 1;
    const s32 a = AnAngle(D.cage_x[k], D.cage_y[k], D.cage_x[n], D.cage_y[n]);
    static const s32 kAlong[3] = { FX(1) / 6, FX(3) / 6, FX(5) / 6 };   // (no division at run time)
    for (int j = 0; j < 3; j++) {
      const int on = show && k != D.cage_open;
      const s32 bx = D.cage_x[k] + SB_MulFP(D.cage_x[n] - D.cage_x[k], kAlong[j]);
      const s32 by = D.cage_y[k] + SB_MulFP(D.cage_y[n] - D.cage_y[k], kAlong[j]);
      AnPut(D.cbar[k][j], on ? bx : FX(-300), on ? by : FX(-300));
      AnTurn(D.cbar[k][j], a);
      AnFrame(D.cbar[k][j], bright);
    }
  }
  if (t >= 120) SpEnd();
}

static void Wall(void) {
  const int t = D.sp_t;
  if (t == 1) {
    s32 ys = PlayerY() - FX(70);
    if (ys < FX(200)) ys = FX(200);
    if (ys > FX(330)) ys = FX(330);
    const int gap = Pick(6);
    const s32 y0 = BowY() + FX(16);
    for (int k = 0; k < 6; k++) {
      const s32 x = FX(70 + 52 * k);
      if (k == gap) { D.wall_gx = x; D.wall_gy = ys; continue; }
      const u32 bg = SB_SpawnBadGuy(BH, "*atlantic/boss/wall16", x, y0);
      if (!bg) continue;
      SetReg(bg, 6, FX(DivSmall(AnDist(0, ys - y0), 3)));   // the steps down to its place (1.4 a frame)
      SetReg(bg, 5, FX(k < 3 ? 128 : 0));                     // and away, the near side
      Track(bg);
    }
  }
  if (t == 160 || t == 240 || t == 320)
    OrderAt(D.atk_mid, A_STREAM, AnAngle(D.hx, D.hy + FX(20), D.wall_gx, D.wall_gy));
  if (t >= 440) SpEnd();
}

static void HarpoonDrop(int h) {
  AnBlast(D.harp_x[h], D.harp_y[h]);
  if (D.harp[h] && Alive(D.harp[h])) { Place(D.harp[h], FX(200), FX(-460)); Remove(D.harp[h]); }
  D.harp[h] = 0;
  D.harp_state[h] = 0;
  for (int k = 0; k < 8; k++) AnPut(D.chain[h][k], FX(-300), FX(-300));
}

static void Harpoons(void) {
  s32 vx, vy;
  AnValPos(&vx, &vy);
  if (D.sp_t == 1)
    for (int h = 0; h < 2; h++) {
      D.harp_x[h] = D.hx + FX(h ? 34 : -34);
      D.harp_y[h] = BowY() - FX(6);
      D.harp[h] = SB_SpawnBadGuy(BH, "*atlantic/boss/harpoon16", D.harp_x[h], D.harp_y[h]);
      D.harp_state[h] = D.harp[h] ? 1 : 0;
      D.harp_t[h] = 0;
      SB_ShakeWorld();
    }
  int live = 0;
  for (int h = 0; h < 2; h++) {
    if (!D.harp_state[h]) continue;
    D.harp_t[h]++;
    // cut (shot) -- not in its first frames: a badguy just made isn't in the engine's list yet, and
    // every harpoon was "cut" the moment it was fired
    if ((D.harp_t[h] > 10 && !Alive(D.harp[h])) || AnValGone() || D.fall_go) { D.harp[h] = 0; HarpoonDrop(h); continue; }
    live++;
    const s32 lx = D.hx + FX(h ? 34 : -34), ly = BowY() - FX(6);
    const s32 tx = vx + FX(h ? 60 : -60), ty = vy - FX(150);
    if (D.harp_state[h] == 1) {                    // flying at her
      const s32 a = AnAngle(D.harp_x[h], D.harp_y[h], tx, ty);
      D.harp_x[h] += SB_MulFP(SB_Cos(a), FX(3));
      D.harp_y[h] += SB_MulFP(SB_Sin(a), FX(3));
      if (AnDist(tx - D.harp_x[h], ty - D.harp_y[h]) < 5) { D.harp_state[h] = 2; AnBlast(tx, ty); SB_ShakeWorld(); }
    } else {                                       // hooked into her
      D.harp_x[h] = tx;
      D.harp_y[h] = ty;
      if (D.harp_t[h] > 900) { HarpoonDrop(h); continue; }   // (wrenched free at last)
    }
    const s32 a = AnAngle(lx, ly, D.harp_x[h], D.harp_y[h]);
    Place(D.harp[h], D.harp_x[h], D.harp_y[h]);
    AnTurn(Act(D.harp[h]), a);
    for (int k = 0; k < 8; k++) {                  // the chain, link by link from the launcher
      AnPut(D.chain[h][k], lx + SB_MulFP(D.harp_x[h] - lx, FX(k) >> 3), ly + SB_MulFP(D.harp_y[h] - ly, FX(k) >> 3));
      AnTurn(D.chain[h][k], a);
    }
    if (NearSegment(lx, ly, D.harp_x[h], D.harp_y[h], 6)) HitShip();
  }
  if (!live && D.sp_t > 2) SpEnd();
}

static int Hooked(void) { return D.harp_state[0] == 2 || D.harp_state[1] == 2; }

// The desktop probes: set to an attack's number to have it now, whatever the cycle (then cleared).
volatile int g_at_force_sp;

static void Specials(void) {
  if (D.hit_grace > 0) D.hit_grace--;
  if (!D.k_on) return;
  if (!D.sp && g_at_force_sp > 0 && g_at_force_sp < SP_N) {
    D.sp = g_at_force_sp;
    D.sp_t = 0;
    g_at_force_sp = 0;
    return;
  }
  if (!D.sp) {
    if (D.over_t || InCutscene() || D.mega) return;
    if (D.sp_cool > 0) { D.sp_cool--; return; }
    for (int i = 0; i < SP_N - 1; i++) {
      const int sp = kSpOrder[D.sp_turn];
      if (++D.sp_turn >= SP_N - 1) D.sp_turn = 0;
      if (SpOk(sp)) { D.sp = sp; D.sp_t = 0; break; }
    }
    return;
  }
  D.sp_t++;
  switch (D.sp) {
  case SP_BROAD: Broadside(); break;
  case SP_ROAR: Roar(); break;
  case SP_RAM: Ram(); break;
  case SP_HES: Hesitation(); break;
  case SP_LEAD: Lead(); break;
  case SP_SEARCH: Searchlight(); break;
  case SP_CAGE: Cage(); break;
  case SP_MINES: Mines(); SpEnd(); break;
  case SP_WALL: Wall(); break;
  case SP_HARPOON: Harpoons(); break;
  }
}

// ---- the ships' battle damage ----------------------------------------------------------------------
// Hits stay where they landed as torn plating (art_damage.py), riding with what they hit: on
// the Valhalla only on her own plating (never over her escorts, who fly over her wings), on
// an escort its own (gone with it), on the Kurogane where her guns hit it.
// Her holes, all made at the start (after her, so they draw over her; parked out of sight), and
// her wreck's strips before her (under her guns and holes): a generic made later can take a freed
// slot under one made earlier, and when she went down her damage and turrets vanished under it.
static void ValDamageMake(void) {
  static const unsigned char kRent[NVDMG] = { 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1 };
  for (int k = 0; k < NVDMG; k++) {
    D.vdmg[k] = Gen(kRent[k] ? "*atlantic/boss/rent16" : "*atlantic/boss/scar16", FX(200), FX(200));   // (every third a gash)
    if (!kRent[k]) AnFrame(D.vdmg[k], k & 3);
    AnPut(D.vdmg[k], FX(-300), FX(-300));
  }
}

static void StripsMake(void) {
  for (int k = 0; k < VAL_NSTRIP; k++) {
    D.strip[k] = Gen("*atlantic/boss/valdown16", FX(200), FX(200));
    AnFrame(D.strip[k], k * 2);
    AnPut(D.strip[k], FX(-300), FX(-300));
  }
}

// Her plating torn at point k (art_damage.valhalla_points), where a shell landed.
static void ValHitAt(int k) {
  if (k < 0 || k >= NVDMG || D.vdmg_on[k] || D.fall_go) return;
  s32 vx, vy;
  AnValPos(&vx, &vy);
  D.vdmg_on[k] = 1;
  D.nvdmg++;
  AnBlast(vx + FX(kValDmgX[k]), vy + FX(kValDmgY[k]));
}

// His, at point k (art_damage.kurogane_points), where her guns landed.
static void KHitAt(int k) {
  if (k < 0 || k >= NKDMG || D.kdmg[k]) return;
  const s32 x = D.hx + FX(kKDmgX[k]), y = D.hy + FX(kKDmgY[k]);
  D.kdmg[k] = Gen((k & 3) == 3 ? "*atlantic/boss/rent16" : "*atlantic/boss/scar16", x, y);
  if ((k & 3) != 3) AnFrame(D.kdmg[k], k & 3);
  D.nkdmg++;
  AnBlast(x, y);
}

// A point not yet torn (a few tries; else any).
static int FreePoint(const u32* have, int n) {
  int k = Pick(n);
  for (int i = 0; i < 6 && have[k]; i++) k = Pick(n);
  return k;
}

// A shell that will leave its mark at point k when it lands (after `t` frames).
static void Track2(int which, int k, int t) {
  for (int i = 0; i < 8; i++) {
    if (which ? D.kpend[i].t : D.vpend[i].t) continue;
    if (which) { D.kpend[i].k = k; D.kpend[i].t = t; }
    else { D.vpend[i].k = k; D.vpend[i].t = t; }
    return;
  }
}

static void Landings(void) {
  for (int i = 0; i < 8; i++) {
    if (D.vpend[i].t > 0 && --D.vpend[i].t == 0) ValHitAt(D.vpend[i].k);
    if (D.kpend[i].t > 0 && --D.kpend[i].t == 0) KHitAt(D.kpend[i].k);
  }
}

static void EscHit(int s) {
  if (D.nedmg[s] >= NEDMG || !AnValEscortUp(s)) return;
  s32 ex, ey;
  AnValEscortPos(s, &ex, &ey);
  const int k = D.nedmg[s]++;
  D.edmg[s][k] = Gen("*atlantic/boss/scar16", ex + FX(kEscDmgX[k]), ey + FX(kEscDmgY[k]));
  AnFrame(D.edmg[s][k], (k + s) & 3);
}

static void EscLose(int s) {
  if (!AnValEscortUp(s)) return;
  AnValEscortLose(s);
  for (int k = 0; k < NEDMG; k++) AnDrop(&D.edmg[s][k]);
  D.nedmg[s] = NEDMG;
  if (!D.esc_lost++) AnLine(AL_VOLTA_ESCORT);
}

static void EscDamage(int s, s32 dmg) {
  const s32 before = D.esc_hp[s];
  D.esc_hp[s] -= dmg;
  // a scar at each third of its strength gone
  if ((before > ESC_HP * 2 / 3 && D.esc_hp[s] <= ESC_HP * 2 / 3) || (before > ESC_HP / 3 && D.esc_hp[s] <= ESC_HP / 3))
    EscHit(s);
  if (D.esc_hp[s] <= 0) EscLose(s);
}

static void Damage(void) {
  // the Valhalla's and her escorts' damage rides with them
  if (!AnValGone()) {
    s32 vx, vy;
    AnValPos(&vx, &vy);
    for (int k = 0; k < NVDMG; k++) if (D.vdmg_on[k]) AnPut(D.vdmg[k], vx + FX(kValDmgX[k]), vy + FX(kValDmgY[k]));
    for (int s = 0; s < 2; s++) {
      if (!AnValEscortUp(s)) continue;
      s32 ex, ey;
      AnValEscortPos(s, &ex, &ey);
      for (int k = 0; k < D.nedmg[s] && k < NEDMG; k++) AnPut(D.edmg[s][k], ex + FX(kEscDmgX[k]), ey + FX(kEscDmgY[k]));
    }
  }
}

// His shells: tracers from his gunwales onto her bow (and, from the third cycle, her escorts).
static void Shelling(void) {
  if (!D.k_on || D.over_t > 0 || AnValGone() || D.fall_go) return;
  if (D.phase != PH_CYCLE && D.phase != PH_APPROACH && D.phase != PH_FALL) return;
  Landings();
  if (D.shell_cool > 0) { D.shell_cool--; return; }
  const int i = Pick(4);
  const s32 x0 = D.hx + FX(kCanX[i]), y0 = D.hy + FX(kCanY[i]);
  s32 vx, vy;
  AnValPos(&vx, &vy);
  const int s = Pick(2);
  if (D.cycle >= 3 && Rnd(0, 4) == 0 && AnValEscortUp(s)) {
    s32 ex, ey;
    AnValEscortPos(s, &ex, &ey);
    if (AnShoot(x0, y0, ex + FX(Rnd(-12, 13)), ey + FX(Rnd(-12, 13)), 7, SHOT_MEDIUM, TAG_NONE, 0, 0))
      EscDamage(s, SHELL_ESC);
  } else {
    // most shells only burst on her; every few leaves its mark where it lands -- spread over the fight
    const int mark = ++D.vshot_n >= (D.face ? 6 : 3);
    int k = Pick(NVDMG);
    for (int i = 0; mark && i < 6 && D.vdmg_on[k]; i++) k = Pick(NVDMG);
    const s32 tx = vx + FX(kValDmgX[k]), ty = vy + FX(kValDmgY[k]);
    if (AnShoot(x0, y0, tx, ty, 7, SHOT_MEDIUM, TAG_NONE, 0, 0) && mark) {
      D.vshot_n = 0;
      Track2(0, k, DivSmall(AnDist(tx - x0, ty - y0), 7) + 1);
    }
  }
  D.shell_cool = D.face ? 40 - 3 * D.cycle : 90;
}

// Her guns back at him, nose to nose: where they land, his plating torn open.
static void ValGuns(void) {
  if (!D.face || !D.k_on || AnValGone() || D.fall_go || D.phase >= PH_ALONE) return;
  if (D.kshell_cool > 0) { D.kshell_cool--; return; }
  s32 vx, vy;
  AnValPos(&vx, &vy);
  const int mark = ++D.kshot_n >= 5;
  const int k = mark ? FreePoint(D.kdmg, NKDMG) : Pick(NKDMG);
  const s32 x0 = vx + FX(Rnd(-60, 61)), y0 = vy - FX(200);
  const s32 tx = D.hx + FX(kKDmgX[k]), ty = D.hy + FX(kKDmgY[k]);
  if (AnShoot(x0, y0, tx, ty, 8, SHOT_MEDIUM, TAG_NONE, 0, 0)) {
    D.kshell_cool = 138;                           // (a third the rate, as her other guns)
    if (mark) {
      D.kshot_n = 0;
      Track2(1, k, DivSmall(AnDist(tx - x0, ty - y0), 8) + 1);
    }
  }
}

// ---- the Tenshu cannon (chase.c's, from the top, aimed at the convoy and her escorts) -------------------
static void MegaTarget(s32* x, s32* y) {
  if (D.phase == PH_FALL) { TargetPos(D.mega_tgt, x, y); return; }
  if ((D.mega_tgt >= 0 && ShipLive(D.mega_tgt)) || (D.mega_tgt >= ESC_TGT && AnValEscortUp(D.mega_tgt - ESC_TGT)))
    TargetPos(D.mega_tgt, x, y);
  else { *x = PlayerX(); *y = PlayerY(); }
}

// The Tenshu's mark: a ship of the convoy, or (the fourth cycle on) one of the Valhalla's escorts.
static int PickTenshu(void) {
  if (D.cycle >= 4) {
    const int s = D.cycle & 1;
    if (AnValEscortUp(s)) return ESC_TGT + s;
    if (AnValEscortUp(s ^ 1)) return ESC_TGT + (s ^ 1);
  }
  return PickShip();
}

static void MegaStart(int tgt) {
  if (D.mega || D.crippled) return;
  D.mega = MG_RISE;
  D.mega_t = 0;
  D.mega_tgt = tgt;
  D.mega_fast = 0;
  D.mega_up = 1;
  D.mega_ang = DOWN;
  D.hatch = Gen("*atlantic/boss/hatch16", D.hx, KeepY());
  s32 x, y;
  MegaTarget(&x, &y);
  D.mret = Gen("*antarctica/boss/reticle", x, y);
  AnFrame(D.mret, 3);
  if (D.phase == PH_CYCLE && !(D.said & SAID_TENSHU)) { D.said |= SAID_TENSHU; AnLine(AL_DOC_TENSHU); }
}

static void MegaBeam(int anim) {
  const s32 ux = SB_Cos(D.mega_ang), uy = SB_Sin(D.mega_ang);
  const s32 mx = D.hx + SB_MulFP(ux, FX(K_MEGA_BARREL)), my = KeepY() + SB_MulFP(uy, FX(K_MEGA_BARREL));
  for (int k = 0; k < NMEGA; k++) {
    const s32 d = FX(SEG / 2 + SEG * k);
    const s32 x = mx + SB_MulFP(ux, d), y = my + SB_MulFP(uy, d);
    const int want = anim >= 0 && y < FX(470);
    if (want && !D.mbeam[k]) D.mbeam[k] = Gen("*antarctica/boss/mega_beam", x, y);
    if (!want && D.mbeam[k]) AnDrop(&D.mbeam[k]);
    if (D.mbeam[k]) {
      AnPut(D.mbeam[k], x, y);
      AnTurn(D.mbeam[k], D.mega_ang);
      AnFrame(D.mbeam[k], anim);
    }
  }
}

// What's in the beam: the ship (a bullet's hit, then a grace), a convoy ship (burning).
static int InBeam(s32 x, s32 y, s32 half) {
  const s32 ux = SB_Cos(D.mega_ang), uy = SB_Sin(D.mega_ang);
  const s32 rx = x - D.hx, ry = y - KeepY();
  const s32 along = SB_MulFP(rx, ux) + SB_MulFP(ry, uy);
  const s32 perp = SB_MulFP(rx, uy) - SB_MulFP(ry, ux);
  return along > FX(K_MEGA_BARREL) && perp < half && perp > -half;
}

static void MegaHits(void) {
  for (int k = 0; k < NSHIP; k++)
    if (ShipLive(k) && InBeam(D.ship[k].x, D.ship[k].y, FX(MEGA_HALF + 6))) ShipHit(k, BEAM_SHIP);
  // her escorts too: held in the beam long enough, one is gone
  for (int s = 0; s < 2; s++) {
    s32 ex, ey;
    if (!AnValEscortUp(s)) continue;
    AnValEscortPos(s, &ex, &ey);
    if (!InBeam(ex, ey, FX(MEGA_HALF + 20))) continue;
    if ((D.frame & 7) == 0) AnBlast(ex + FX(Rnd(-20, 21)), ey + FX(Rnd(-20, 21)));
    EscDamage(s, BEAM_SHIP * 2);
  }
  if (D.mega_grace > 0) { D.mega_grace--; return; }
  const u32 sh = Shogun();
  if (*(volatile unsigned short*)(sh + SH_HITLOCK) || R32(sh, SH_HITLOCK2)) return;
  if (!InBeam(PlayerX(), PlayerY(), FX(MEGA_HALF))) return;
  SB_StrikePlayer(sh + SH_PLAYER, 0, sh + SH_HUD);
  D.mega_grace = MEGA_GRACE;
}

// Pulled down under the armour: fast (the sun coming).
static void MegaHide(void) {
  if (!D.mega || D.mega == MG_SINK) return;
  D.mega = MG_SINK;
  D.mega_t = 0;
  D.mega_fast = 1;
  MegaBeam(-1);
  AnDrop(&D.mret);
}

static void Mega(void) {
  if (!D.mega) return;
  D.mega_t++;
  s32 tx, ty;
  MegaTarget(&tx, &ty);
  const s32 aim = AnAngle(D.hx, KeepY(), tx, ty);
  AnPut(D.hatch, D.hx, KeepY());
  switch (D.mega) {
  case MG_RISE: {
    const int t = D.mega_t;
    AnFrame(D.hatch, t < 10 ? 1 : (t < 20 ? 2 : 3));
    AnPut(D.mret, tx, ty);
    if (t == 24) {
      D.mcannon = Gen("*antarctica/boss/megacannon", D.hx, KeepY());
      AnDrop(&D.hatch);
    }
    AnFrame(D.mcannon, t < 36 ? 0 : (t < 48 ? 1 : (t < 60 ? 2 : (t < 70 ? 3 : (t < 80 ? 4 : 5)))));
    if (t == 60) SB_ShakeWorld();
    if (t >= 90) { D.mega = MG_LOCK; D.mega_t = 0; }
    break;
  }
  case MG_LOCK: {
    D.mega_ang = AnTurnTo(D.mega_ang, aim, D.mega_t < MEGA_LOCK - 24 ? TURN / 90 : TURN / 240);
    AnPut(D.mret, tx, ty);
    AnFrame(D.mret, D.mega_t < MEGA_LOCK - 24 ? 3 : 2);
    AnFrame(D.mcannon, D.mega_t < MEGA_LOCK - 24 ? 5 : 6);
    MegaBeam(D.mega_t < MEGA_LOCK - 24 ? -1 : 0);
    // (held on its charge in the first cycle until Volta's shot comes -- the lesson -- and in the fall)
    const int hold = D.phase == PH_FALL || (D.phase == PH_CYCLE && D.cycle == 1 && D.over_t == 0);
    if (D.mega_t >= MEGA_LOCK && !hold) {
      D.mega = MG_FIRE;
      D.mega_t = 0;
      AnDrop(&D.mret);
      SB_ShakeWorld();
    }
    if (hold && D.mega_t > MEGA_LOCK) D.mega_t = MEGA_LOCK;
    break;
  }
  case MG_FIRE:
    D.mega_ang = AnTurnTo(D.mega_ang, aim, MEGA_SWEEP);
    AnFrame(D.mcannon, 7);
    MegaBeam(D.mega_t < 6 ? 1 : 5);
    MegaHits();
    if ((D.mega_t & 31) == 0) SB_ShakeWorld();
    if (D.mega_t >= MEGA_FIRE) {
      D.mega = MG_SINK;
      D.mega_t = 0;
      MegaBeam(4);
    }
    break;
  case MG_HELD:
    // stuck out by the overload, its aim thrown: it sweeps blind, the beam down
    D.mega_ang = AnTurnTo(D.mega_ang, DOWN + SB_MulFP(TURN / 10, SB_Sin((s32)(D.frame << 16))), TURN / 400);
    AnFrame(D.mcannon, 5);
    if (D.over_t == 0) { D.mega = MG_SINK; D.mega_t = 0; }   // the window closed without the sun: it goes in
    break;
  case MG_SINK: {
    if (D.mega_t == 10 || D.mega_fast) MegaBeam(-1);
    const int dur = D.mega_fast ? MEGA_FAST : MEGA_SINK;
    const int left = dur - D.mega_t;
    const int step = D.mega_fast ? 6 : 16;
    AnFrame(D.mcannon, left > 5 * step ? 5 : (left > 4 * step ? 4 : (left > 3 * step ? 3 : (left > 2 * step ? 2 : (left > step ? 1 : 0)))));
    if (left == 12) {
      AnDrop(&D.mcannon);
      D.hatch = Gen("*atlantic/boss/hatch16", D.hx, KeepY());
      AnFrame(D.hatch, 3);
    }
    if (left < 12) AnFrame(D.hatch, left > 8 ? 2 : (left > 4 ? 1 : 0));
    if (left <= 0) {
      AnDrop(&D.hatch);
      D.mega = MG_OFF;
      if (D.wrath && !D.mega_fast && D.over_t == 0) Order(D.wrath, G_WRATH);
    }
    break;
  }
  }
  if (D.mcannon) {
    AnPut(D.mcannon, D.hx, KeepY());
    AnTurn(D.mcannon, D.mega_ang);
  }
}

// ---- Volta's gauge and her shot -------------------------------------------------------------------------
// On the HUD under the shield bar (edinburgh.c's hook): a row of cells, lit
// as the gauge fills, flashing when the shot is ready.
#define GAUGE_X      150            // the first cell, in 256ths of the screen ...
#define GAUGE_STEP   7              // ... each one on
#define GAUGE_Y      17

void AtlanticHud(void) {
  if (!D.on || R32(Shogun(), SH_WORLD_NO) != ATLANTIC || AL_GetFrameCount() - D.ticked > 4) return;
  if (D.phase < PH_CYCLE || D.phase > PH_FALL) return;
  const int lit = (int)SB_MulFP(D.bar, (NPIP << 16) / BAR_FULL);   // (a multiply by the reciprocal)
  const int ready = D.bar >= BAR_FULL || D.firing == 1;
  const s32 sw = AL_GetScreenWidth(), sh = AL_GetScreenHeight();
  const s32 y = (sh * GAUGE_Y) >> 8;
  for (int k = 0; k < NPIP; k++) {
    const u32 f = ready ? ((AL_GetFrameCount() >> 3) & 1 ? 2 : 1) : (k < lit ? 1 : 0);
    const s32 x = ((sw * (GAUGE_X + GAUGE_STEP * k)) >> 8) - (AL_GetWidth(SPR_GAUGE) >> 1);
    AL_DrawSprite(SPR_GAUGE + f, x, y, 0);
  }
}

static void MarkPos(s32* x, s32* y) {
  if (D.cripple_shot) { *x = D.hx; *y = KeepY(); return; }
  *x = D.hx + FX(kMarkX[D.mark & 7]);
  *y = D.hy + FX(kMarkY[D.mark & 7]);
}

static void Fire(int line) {
  D.firing = 1;
  D.rail_t = 0;
  D.rail_line = AnLine(line);
  s32 x, y;
  MarkPos(&x, &y);
  AnValRailAt(x, y, D.rail_line);
}

static void OverloadStart(int len);
static void Cue(int line);

// Her shot in flight: the target followed; when it lands, the hit.
static void Rail(void) {
  if (!D.firing) return;
  D.rail_t++;
  s32 x, y;
  MarkPos(&x, &y);
  AnValRailMove(x, y);
  const int st = AnValRailState();
  if (D.firing == 1 && (st >= RAIL_FIRING || D.rail_t > 900)) {
    D.firing = 2;
    SB_ShakeWorld();
    if (D.cripple_shot) {
      // the expert shot: the Tenshu cannon broken where it stands, its charge gone
      D.crippled = 1;
      D.mega = MG_OFF;
      MegaBeam(-1);
      AnDrop(&D.mret);
      AnDrop(&D.mcannon);
      AnDrop(&D.hatch);
      D.wreck = Gen("*atlantic/boss/tenshu_wreck", D.hx, KeepY());
      AnBlast(D.hx, KeepY());
      Blast("ExplosionXL", D.hx, KeepY());
      Hurt(CRIPPLE_DMG);
      return;
    }
    Hurt(RAIL_DMG);
    AddBreach(kMarkX[D.mark & 7], kMarkY[D.mark & 7], 0);
    D.mark++;
    OverloadStart(D.phase == PH_FIRST ? 150 : kWindow[D.cycle - 1 < 0 ? 0 : (D.cycle - 1 > 4 ? 4 : D.cycle - 1)]);
  }
  if (D.firing == 2 && (st == RAIL_DONE || D.rail_t > 1100)) {
    D.firing = 0;
    D.bar = 0;
  }
}

// ---- the overload ------------------------------------------------------------------------------------------
static void FileFrom(int left, s32 y, int n);
static void Gunships(void);

static void OverloadStart(int len) {
  // his escorts rush in from both sides to cover the weakness (not the first shot's: no window yet)
  if (D.phase != PH_FIRST) {
    FileFrom(1, BowY() + FX(30), 4);
    FileFrom(0, BowY() + FX(60), 4);
    Gunships();
  }
  D.over_t = 1;
  D.over_len = len;
  D.struck = 0;
  StopGuns();
  AmaJam(0);
  if (D.phase != PH_FIRST) AmaPaintEnable(D.core[0], 1);
  // the Tenshu up when it hits: its aim thrown, stuck out (until he sees the sun coming)
  if (D.mega == MG_LOCK || D.mega == MG_FIRE || (D.mega == MG_RISE && D.mcannon)) {
    D.mega = MG_HELD;
    D.mega_t = 0;
    MegaBeam(-1);
    AnDrop(&D.mret);
  }
}

static void Overload(void) {
  if (!D.over_t) return;
  D.over_t++;
  if (D.phase != PH_FIRST && !(D.said & SAID_WINDOW)) { D.said |= SAID_WINDOW; AnLineNow(AL_DOC_WINDOW); }
  // the sun coming down: the cannon goes under (if it was already down, he keeps it so)
  if (AmaStriking()) {
    if (D.mega && D.mega != MG_SINK) {
      MegaHide();
      if (!(D.said & SAID_HIDE)) { D.said |= SAID_HIDE; AnLine(AL_DOC_HIDE); }
    }
  }
  if (AmaStruck(D.core[0]) > 0 && !D.struck) {
    D.struck = 1;
    Hurt(SUN_DMG16);
    AddBreach(Rnd(-20, 21), K_HULL_BOW - 30 - Rnd(0, 20), 1);
    if (D.over_len - D.over_t > 90) D.over_len = D.over_t + 90;   // (the window has done its work)
  }
  if (D.cycle >= 2 && D.over_len - D.over_t == 120) D.vent_warn = 120;   // the backlash coming: two seconds
  if (D.vent_warn > 0) {
    D.vent_warn--;
    if ((D.vent_warn & 3) == 0) AnBlast(D.hx + FX(Rnd(-90, 91)), D.hy + FX(Rnd(-50, 51)));
  }
  if (D.over_t >= D.over_len) {
    D.over_t = 0;
    D.mines_laid = 0;
    AmaJam(1);
    g_graze_jammed = 0;
    AmaPaintEnable(D.core[0], 0);
    // the systems come back with a surge: the backlash, rings off the whole hull (Mission 7's
    // wrath ring before the second cycle)
    if (D.cycle >= 2 && D.atk_mid) { OrderAt(D.atk_mid, A_VENT, 0); SB_ShakeWorld(); }
    else if (D.wrath && D.phase != PH_FIRST) Order(D.wrath, G_WRATH);
    D.stun = 40;
    if (D.phase == PH_CYCLE) {
      D.away_n++;                                   // a ship of the convoy gets clear
      D.mega_up = 0;                                // the Tenshu may rise again
      if (D.cycle == 1) {
        // after the lesson: nose to nose
        D.phase = PH_APPROACH;
        D.t = 0;
        Cue(AL_VOLTA_CLOSE);
        return;
      }
      D.cycle++;
      if (D.cycle == 3 && !(D.said & SAID_SHORTER)) { D.said |= SAID_SHORTER; AnLine(AL_DOC_SHORTER); }
      if (D.cycle == 4 && !(D.said & SAID_HURT)) { D.said |= SAID_HURT; AnLine(AL_DOC_VAL_HURT); }
    }
  }
}

// ---- the fall: the Valhalla goes down ---------------------------------------------------------------
// Her own pieces go (andaman.c AnValVanish) and her wreck takes over, whole
// (art_damage.valdown16: her wings torn, holes all along her, her lift fans and
// jets on fire) in four strips held together as one: she rolls over a little and
// sinks, slowly gathering speed, burning, her engines pouring fire.
static void StripAt(int k, s32* x, s32* y) {
  s32 ox, oy;
  AnRot(0, -VAL_STRIP_H * VAL_NSTRIP / 2 + VAL_STRIP_H / 2 + VAL_STRIP_H * k, D.dca, &ox, &oy);
  *x = D.dcx + ox;
  *y = D.dcy + oy;
}

static void GoingDown(void) {
  // spinning out of control, slowly at first, faster and faster, as she drifts and sinks: her
  // torn wings and burning engines come round into view
  if (D.dcs < TURN / 500) D.dcs += TURN / 400000;
  D.dca -= D.dcs;
  D.dcv += FX(0.0006);                             // (lurching up off the broadside, then falling)
  D.dcy += D.dcv;
  D.dcx += SB_MulFP(FX(0.25), SB_Sin(D.dca));
  int up = 0;
  for (int k = 0; k < VAL_NSTRIP; k++) {
    if (!D.strip[k]) continue;
    if (D.dcy > FX(780)) { AnDrop(&D.strip[k]); continue; }   // (all of her past the bottom)
    up++;
    s32 x, y;
    StripAt(k, &x, &y);
    if (y > FX(500)) {                             // (a piece round past the bottom waits out of sight:
      AnPut(D.strip[k], FX(-300), FX(-300));       //  past 512 it would show at the top)
      continue;
    }
    AnPut(D.strip[k], x, y);
    AnTurn(D.strip[k], UP + D.dca);
    AnFrame(D.strip[k], k * 2 + ((D.fall_t >> 2) & 1));
  }
  // her turrets and railgun, dead, and her holes, carried round with her
  for (int g = 0; g < D.nwgun; g++) {
    s32 ox, oy;
    AnRot(D.wgx[g] >> 16, D.wgy[g] >> 16, D.dca, &ox, &oy);
    const s32 gx = D.dcx + ox, gy = D.dcy + oy;
    const int up_ = up && gy < FX(500);
    AnPut(D.wgun[g], up_ ? gx : FX(-300), up_ ? gy : FX(-300));
    AnTurn(D.wgun[g], D.wga[g] + D.dca);
    AnFrame(D.wgun[g], 0);
  }
  for (int k = 0; k < NVDMG; k++) {
    if (!D.vdmg_on[k]) continue;
    s32 ox, oy;
    AnRot(kValDmgX[k], kValDmgY[k], D.dca, &ox, &oy);
    const s32 hx = D.dcx + ox, hy = D.dcy + oy;
    const int up_ = up && hy < FX(500);
    AnPut(D.vdmg[k], up_ ? hx : FX(-300), up_ ? hy : FX(-300));
  }
  // her engines pouring fire; blasts along her
  if ((D.fall_t & 3) == 0) {
    const int e = Pick((int)(sizeof kEngX / sizeof kEngX[0]));
    s32 ox, oy;
    AnRot(kEngX[e], kEngY[e] + VAL_STRIP_H * VAL_NSTRIP / 4, D.dca, &ox, &oy);
    if (D.dcy + oy < FX(500)) AnBlast(D.dcx + ox, D.dcy + oy);
  }
  if ((D.fall_t & 15) == 0) {
    s32 ox, oy;
    AnRot(Rnd(-150, 151), Rnd(-220, 60), D.dca, &ox, &oy);
    if (D.dcy + oy < FX(480)) Boom("ExplosionL", D.dcx + ox, D.dcy + oy);
  }
  if (!up) {
    D.dcv = 0;
    for (int g = 0; g < D.nwgun; g++) AnDrop(&D.wgun[g]);
    D.nwgun = 0;
    for (int k = 0; k < NVDMG; k++) { AnDrop(&D.vdmg[k]); D.vdmg_on[k] = 0; }
  }
}

static void FallStart(void) {
  s32 vx, vy;
  AnValPos(&vx, &vy);
  D.wx = vx;
  D.wy = FX(405);
  for (int i = 0; i < 8; i++) D.vpend[i].t = 0;
  for (int s = 0; s < 2; s++) for (int k = 0; k < NEDMG; k++) AnDrop(&D.edmg[s][k]);
  D.nwgun = AnValTakeGuns(D.wgun, D.wgx, D.wgy, D.wga);   // (her guns stay with her, dead where they point)
  D.dcx = vx;
  D.dcy = vy;
  D.dca = 0;
  D.dcv = -FX(0.3);
  D.dcs = 0;
  // (the strips were made at the start, under her: placed before her pieces go, so nothing blinks out)
  for (int k = 0; k < VAL_NSTRIP; k++) {
    s32 x, y;
    StripAt(k, &x, &y);
    AnPut(D.strip[k], x, y);
    AnTurn(D.strip[k], UP);
  }
  AnValVanish();
  SB_ShakeWorld();
  SB_ShakeWorld();
  Blast("ExplosionXL", vx, vy - FX(100));
}

static void Wreck(void) {
  if (!D.wfire[0] && D.phase >= PH_ALONE) {
    for (int k = 0; k < NWRECK; k++)
      D.wfire[k] = Gen("*atlantic/boss/breach16_big", D.wx + FX(-60 + 30 * k), D.wy + FX(Rnd(-10, 14)));
  }
  if (D.wfire[0] && (D.frame & 31) == 0) AnBlast(D.wx + FX(Rnd(-60, 61)), D.wy + FX(Rnd(-12, 13)));
}

// ---- the charge -----------------------------------------------------------------------------------------------
// The Bastion, her refit's guns and the destroyers: made at the start (parked out of sight) so
// her turrets are made right after her and always draw over her (a generic made later can take a
// freed slot under one made before: docs/NEW_LEVEL.md 13).
static void BastionMake(void) {
  D.bastion = Gen("*amaterasu/boss/bastion14", FX(200), FX(200));
  for (int k = 0; k < 3; k++) { D.btur[k] = Gen("*andaman/boss/bturret", FX(200), FX(200)); D.btur_ang[k] = UP; }
  for (int k = 0; k < 4; k++) { D.ciws[k] = Gen("*andaman/boss/ciws", FX(200), FX(200)); D.ciws_ang[k] = UP; }
  for (int s = 0; s < 2; s++) D.dest[s] = Gen("*amaterasu/boss/destroyer", FX(200), FX(200));
  AnPut(D.bastion, FX(-300), FX(-300));
  for (int k = 0; k < 3; k++) AnPut(D.btur[k], FX(-300), FX(-300));
  for (int k = 0; k < 4; k++) AnPut(D.ciws[k], FX(-300), FX(-300));
  for (int s = 0; s < 2; s++) AnPut(D.dest[s], FX(-300), FX(-300));
}

static void ChargeStart(void) {
  D.bsx = FX(120);
  D.bsy = FX(560);
  D.charging = 1;
  for (int s = 0; s < 2; s++) AnTurn(D.dest[s], UP);
  // every breach the railgun and the sun made: open to guns now
  for (int k = 0; k < D.nbreach; k++) {
    D.btgt[k] = SB_SpawnBadGuy(BH, "*atlantic/boss/btarget", D.hx + D.bx[k], D.hy + D.by[k]);
    D.bhp[k] = BREACH_HP;
  }
}

static void Charge(void) {
  if (!D.charging) return;
  D.charge_t++;
  D.bsy += Ease(FX(330) - D.bsy, FX(1.0));
  AnPut(D.bastion, D.bsx, D.bsy);
  // her main turrets on the breach she's shelling (the aft one on the nearest hunter), her
  // point-defence on the hunters, each flashing as it fires
  static const signed char kBTur[3][2] = { { BASTION_TUR0_X, BASTION_TUR0_Y }, { BASTION_TUR1_X, BASTION_TUR1_Y },
                                           { BASTION_TUR2_X, BASTION_TUR2_Y } };
  static const signed char kCiws[4][2] = { { -36, -24 }, { 36, -24 }, { -34, 48 }, { 34, 48 } };
  for (int k = 0; k < 3; k++) {
    const s32 tx0 = D.bsx + FX(kBTur[k][0]), ty0 = D.bsy + FX(kBTur[k][1]);
    s32 want = k == 2 ? DOWN : UP;
    const int b = D.btur_k;
    if (k < 2 && b >= 0 && b < D.nbreach && D.btgt[b]) want = AnAngle(tx0, ty0, D.hx + D.bx[b], D.hy + D.by[b]);
    D.btur_ang[k] = AnTurnTo(D.btur_ang[k], want, TURN / 160);
    if (D.btur_flash[k] > 0) D.btur_flash[k]--;
    AnPut(D.btur[k], tx0, ty0);
    AnTurn(D.btur[k], D.btur_ang[k]);
    AnFrame(D.btur[k], D.btur_flash[k] > 0);
  }
  for (int k = 0; k < 4; k++) {
    const s32 cx = D.bsx + FX(kCiws[k][0]), cy = D.bsy + FX(kCiws[k][1]);
    s32 want = UP;
    s32 best = 200;
    for (int j = 0; j < NHUNT; j++) {
      if (!D.hunt[j].bg) continue;
      s32 hx, hy;
      SB_GetActorPosition(Act(D.hunt[j].bg), &hx, &hy);
      const s32 d = AnDist(hx - cx, hy - cy);
      if (d < best) { best = d; want = AnAngle(cx, cy, hx, hy); }
    }
    D.ciws_ang[k] = AnTurnTo(D.ciws_ang[k], want, TURN / 60);
    if (D.ciws_cool[k] > 0) D.ciws_cool[k]--;
    else if (best < 200) D.ciws_cool[k] = 30 + 9 * k;
    AnPut(D.ciws[k], cx, cy);
    AnTurn(D.ciws[k], D.ciws_ang[k]);
    AnFrame(D.ciws[k], D.ciws_cool[k] > 24 + 9 * k);
  }
  for (int s = 0; s < 2; s++) AnPut(D.dest[s], D.bsx + FX(s ? 110 : -80), D.bsy + FX(40));
  // the breaches: what is taken off them comes off the hull; the Bastion's shells on them
  int live = 0;
  for (int k = 0; k < D.nbreach; k++) {
    const u32 b = D.btgt[k];
    if (!b) continue;
    if (D.charge_t < 10) { live++; continue; }      // (just made: not in the engine's list yet)
    if (!Alive(b)) {
      Hurt(D.bhp[k]);
      AnBlast(D.hx + D.bx[k], D.hy + D.by[k]);
      D.btgt[k] = 0;
      continue;
    }
    live++;
    const s32 hp = SB_GetBadGuyHealth(b);
    if (hp < D.bhp[k]) { Hurt(D.bhp[k] - hp); D.bhp[k] = hp; }
  }
  D.blive = live;
  if (D.bcool > 0) D.bcool--;
  else if (D.bsy < FX(420) && live) {
    int k = Pick(D.nbreach);
    for (int i = 0; i < D.nbreach && !D.btgt[k]; i++) k = k + 1 >= D.nbreach ? 0 : k + 1;
    if (D.btgt[k]) {
      const s32 tx = D.hx + D.bx[k], ty = D.hy + D.by[k];
      D.btur_k = k;
      const int g = (D.bcool & 1);                         // (her two forward turrets in turn)
      const s32 mx = D.bsx + FX(g ? BASTION_TUR1_X : BASTION_TUR0_X), my = D.bsy + FX(g ? BASTION_TUR1_Y : BASTION_TUR0_Y);
      if (AnOff(D.btur_ang[g], AnAngle(mx, my, tx, ty)) < TURN / 30 &&
          AnShoot(mx + SB_MulFP(SB_Cos(D.btur_ang[g]), FX(20)), my + SB_MulFP(SB_Sin(D.btur_ang[g]), FX(20)), tx, ty, 8,
                  SHOT_BIG, TAG_NONE, D.btgt[k], 30)) {
        D.bcool = 50 + (D.charge_t & 1);
        D.btur_flash[g] = 8;
      }
    }
  }
  g_autopilot_target = 0;
  for (int k = 0; k < D.nbreach; k++) if (D.btgt[k]) { g_autopilot_target = D.btgt[k]; break; }
  if (D.phase == PH_CHARGE && D.charge_t > CHARGE_MIN && (D.hp <= HP_WITHDRAW || !live || D.charge_t > 7200)) {
    D.phase = PH_WITHDRAW;
    D.t = 0;
    StopGuns();
    Cue(AL_SHOGUN_RAGE);
  }
}

// ---- the story ---------------------------------------------------------------------------------------------------
// Few lines, and each one the cue for what happens: a beat waits for its line to be on the
// screen (Cue/Cued, a time limit in case the talk sticks), so the order and the deed come together.
static void Cue(int line) {
  D.cue = AnLine(line);
  D.cue_t = 0;
}

static int Cued(void) {
  if (!D.cue) return 1;
  if (AnLineUp(D.cue) || ++D.cue_t > 600) {
    D.cue = 0;
    return 1;
  }
  return 0;
}

static void Story(void) {
  D.t++;
  switch (D.phase) {
  case PH_CALM:
    if (D.t == 60) AnLine(AL_DOC_HOME);
    if (D.t == 900) AnLine(AL_VOLTA_CHEER);
    if (D.t >= CALM_T && AnQuiet()) {
      D.phase = PH_AKANE;
      D.t = 0;
      AkaneStart();
      AnLine(AL_AKANE_HELLO);
    }
    break;
  case PH_AKANE:
    if (D.ak == AK_GONE && !D.queued && AnValRailState() == RAIL_DONE) { D.queued = 1; D.t = 0; }
    if (D.queued && D.t > 240 && AnQuiet()) {
      D.queued = 0;
      D.phase = PH_STORM;
      D.t = 0;
      D.rain_on = 1;
      R32(D.boss, B_MAXHP) = HP_MAX;
    }
    break;
  case PH_STORM:
    // the jam (on its line), then the Kurogane out of the wall; the convoy turns on Argus's order
    if (D.t == 240) Cue(AL_DOC_JAM);
    if (D.t > 240 && !D.haze_on && Cued()) {
      D.haze_on = 1;
      D.rain_on = 2;
      AmaJam(1);
      g_graze_jammed = 0;
      D.t = 241;
      D.queued = 1;
    }
    if (D.queued == 1 && D.t == 420) {
      D.queued = 2;
      D.hx = FX(200);
      D.hy = FX(START_Y);
      D.want_y = FX(HULL_Y);
      D.k_on = 1;
      BuildGuns();
      AmaPaintAdd(D.core[0], PT_BOSS);
      AmaPaintEnable(D.core[0], 0);
      Cue(AL_ARGUS_TURN);
    }
    if (D.queued == 2 && Cued()) {
      D.queued = 3;
      D.turned = 1;                               // "hard about!": they wheel round now
      AnLine(AL_SHOGUN_HELLO);
    }
    if (D.queued == 3 && D.hy > FX(HULL_Y - 6) && AnQuiet()) {
      D.queued = 0;
      D.phase = PH_FIRST;
      D.t = 0;
      Fire(AL_VOLTA_SHOT);                        // (her railgun waits for her line)
    }
    break;
  case PH_FIRST:
    if (D.firing == 0 && D.t > 60 && !D.queued) {
      D.queued = 1;
      AnLine(AL_SHOGUN_LEARN);
      D.end_line = AnLine(AL_VOLTA_HOLD);
    }
    if (D.queued && AnLineUp(D.end_line) && AnQuiet() && D.over_t == 0) {
      D.phase = PH_CYCLE;
      D.cycle = 1;
      D.t = 0;
      D.queued = 0;
      D.bar = 0;
      D.away_n = 1;
    }
    break;
  case PH_CYCLE:
    // the gauge: the hunters' health and the time; full, Volta fires
    if (++D.bar_t >= (Hooked() ? 2 * BAR_EVERY : BAR_EVERY)) {   // (hooked by his harpoons: half as fast)
      D.bar_t = 0;
      if (D.over_t == 0) D.bar++;
    }
    // from the third cycle, as her gauge nears full, he lays mines across the way to his bow
    if (D.cycle >= 3 && !D.mines_laid && D.bar >= BAR_FULL * 9 / 10) { D.mines_laid = 1; Mines(); }
    if (!D.firing && D.over_t == 0 && D.mega != MG_RISE &&
        (D.bar >= BAR_FULL || (D.cycle == 1 && D.mega == MG_LOCK && D.mega_t >= MEGA_LOCK))) {
      // (the lesson: in the first cycle she fires the moment the Tenshu has its lock)
      D.bar = BAR_FULL;
      Fire(AL_COUNT);                             // (no line: the gauge says it)
    }
    // as her gauge passes MEGA_AT, the Tenshu comes out for the convoy (and her escorts)
    if (!D.mega && !D.mega_up && D.over_t == 0 && !D.firing && D.bar >= MEGA_AT) MegaStart(PickTenshu());
    if (D.cycle > NCYCLE && D.over_t == 0 && !D.firing) {
      D.phase = PH_FALL;
      D.t = 0;
      D.bar = 0;
    }
    break;
  case PH_APPROACH:
    // on Volta's order, closing until nose to nose; then the fight goes on there
    if (D.cue && Cued()) {
      D.want_y = FX(FACE_Y);
      AnValMoveTo(FX(VAL_FACE_X), FX(VAL_FACE_Y));
      D.t = 0;
    }
    if (!D.cue && D.t > 400 && D.hy > FX(FACE_Y - 6)) {
      D.face = 1;
      D.phase = PH_CYCLE;
      D.cycle = 2;
      D.t = 0;
      D.bar = 0;
    }
    break;
  case PH_FALL:
    // the Tenshu at full charge on the convoy; Volta's expert shot; the broadside; she goes down
    if (D.t == 1 && D.mega) MegaHide();
    if (D.t >= 80 && !D.mega && !D.cripple_shot) MegaStart(PickShip());   // (once it's down from before)
    if (D.mega == MG_LOCK && !D.cripple_shot) {
      D.cripple_shot = 1;
      Fire(AL_VOLTA_TENSHU);
    }
    if (D.crippled && !D.fall_t) D.fall_t = 1;
    if (D.fall_t) {
      D.fall_t++;
      s32 vx, vy;
      if (!D.fall_go) {
        // the broadside: every gun he has into her
        AnValPos(&vx, &vy);
        if ((D.fall_t & 1) == 0) {
          const int k = Pick(NVDMG);
          AnShoot(D.hx + FX(Rnd(-120, 121)), D.hy + FX(Rnd(-20, 50)), vx + FX(kValDmgX[k]), vy + FX(kValDmgY[k]),
                  9, SHOT_BIG, TAG_NONE, 0, 0);
        }
        if ((D.fall_t & 3) == 0) AnBlast(vx + FX(Rnd(-120, 121)), vy - FX(Rnd(0, 200)));
        if ((D.fall_t & 15) == 0) ValHitAt(Pick(NVDMG));
      }
      // her last words up, and she goes
      if (D.fall_t == 30) D.crash_line = AnLine(AL_VOLTA_CRASH);
      if (!D.fall_go && D.fall_t > 60 && (AnLineUp(D.crash_line) || D.fall_t > 400)) {
        D.fall_go = D.fall_t;
        FallStart();
        D.firing = 0;
      }
      if (D.fall_go) GoingDown();
      int up = 0;
      for (int k = 0; k < VAL_NSTRIP; k++) up += D.strip[k] != 0;
      if (D.fall_go && !up && !D.queued) {
        D.queued = 1;
        D.end_line = AnLine(AL_DOC_VOLTA);
      }
      if (D.queued && AnLineUp(D.end_line) && AnQuiet()) {
        D.queued = 0;
        D.phase = PH_ALONE;
        D.t = 0;
        D.away_n = NSHIP;
      }
    }
    break;
  case PH_ALONE:
    if (D.t == ALONE_T) Cue(AL_ARGUS_CHARGE);
    if (D.t > ALONE_T && Cued()) {
      D.phase = PH_CHARGE;
      D.t = 0;
      ChargeStart();
    }
    break;
  case PH_WITHDRAW:
    if (D.cue && Cued()) D.want_y = FX(AWAY_Y);   // on his last word, back into the storm
    if (!D.cue && D.hy < FX(-200) && AnQuiet()) {
      D.phase = PH_AFTER;
      D.t = 0;
      AnLine(AL_ARGUS_BOATS);
      AnLine(AL_ARGUS_BLAME);
      D.end_line = AnLine(AL_DOC_GUILT);
      D.queued = 1;
    }
    break;
  case PH_AFTER:
    if (D.queued && AnLineUp(D.end_line) && AnQuiet() && D.t > 200) D.phase = PH_DONE;
    break;
  }
}

// ---- the scroll: eased nearly to rest once the trap is shut ---------------------------------------------------
#define SH_SPEED       0x5c
#define SH_SPEED_TO    0x60
#define SH_SPEED_STEP  0x64

static void Scroll(void) {
  if (D.phase < PH_STORM) return;
  const u32 sh = Shogun();
  const s32 want = D.phase >= PH_AFTER ? FX(0.6) : FX(0.08);
  s32 v = R32(sh, SH_SPEED);
  const s32 d = want - v;
  v += d > FX(0.004) ? FX(0.004) : (d < -FX(0.004) ? -FX(0.004) : d);
  R32(sh, SH_SPEED) = v;
  R32(sh, SH_SPEED_TO) = v;
  R32(sh, SH_SPEED_STEP) = 0;
}

// ---- the boss ------------------------------------------------------------------------------------------------------
static s32 AtHealth(u32 boss) {
  (void)boss;
  if (D.phase == PH_AKANE) return D.mag ? SB_GetBadGuyHealth(D.mag) : 0;
  return D.phase >= PH_FIRST && D.phase <= PH_CHARGE ? D.hp : 0;
}

static void AtRender(u32 boss, s32 x, s32 y) { (void)boss; (void)x; (void)y; }

static void AtSpawn(u32 boss) {
  D.boss = boss;
  R32(boss, B_STATE) = 1;
  R32(boss, B_MAXHP) = HP_MAX;
  SB_PausePartition(BH, 1);
  D.phase = PH_CALM;
  D.t = 0;
  D.hp = HP_MAX;
  D.mega_tgt = -1;
  D.esc_hp[0] = D.esc_hp[1] = ESC_HP;
  // the Kurogane's pieces first (parked in the storm until it comes), the sun's right after it,
  // then the convoy, then the Valhalla flying over them
  D.hx = FX(-300);
  D.hy = FX(-300);
  BuildSeen();
  Place16();
  ConvoySpawn();
  StripsMake();                                    // (her wreck: under her guns and holes)
  AnValComeSide(FX(VAL_X), FX(VAL_X), FX(VAL_Y));   // (already on station when the mission opens)
  ValDamageMake();                                 // (her holes: over her)
  // the searchlight's cross and the lightning cage, made last of all: over everything, the Valhalla too
  for (int k = 0; k < 5; k++) { D.cross[k] = Gen("*atlantic/boss/target16", FX(200), FX(200)); AnPut(D.cross[k], FX(-300), FX(-300)); }
  for (int k = 0; k < 4; k++) {
    D.cpost[k] = Gen("*atlantic/boss/cagepost", FX(200), FX(200));
    AnPut(D.cpost[k], FX(-300), FX(-300));
    for (int j = 0; j < 3; j++) { D.cbar[k][j] = Gen("*atlantic/boss/cagebar", FX(200), FX(200)); AnPut(D.cbar[k][j], FX(-300), FX(-300)); }
  }
  for (int h = 0; h < 2; h++)                      // (the harpoons' chains: over her too)
    for (int k = 0; k < 8; k++) { D.chain[h][k] = Gen("*atlantic/boss/chain16", FX(200), FX(200)); AnPut(D.chain[h][k], FX(-300), FX(-300)); }
  BastionMake();
}

static void AtUpdate(u32 boss) {
  D.frame++;
  D.pvx = PlayerX() - D.ppx;
  D.pvy = PlayerY() - D.ppy;
  D.ppx = PlayerX();
  D.ppy = PlayerY();
  D.ticked = AL_GetFrameCount();
  BossWards();
  if (D.phase == PH_DONE) {
    R32(boss, B_STATE) = 0;                         // the engine calls death next
    return;
  }
  SB_PausePartition(BH, 1);
  ShipShield(InCutscene() && D.phase < PH_CYCLE);
  Story();
  Scroll();
  Weather();
  Convoy();
  Akane();
  if (D.phase < PH_WITHDRAW) {
    Hunters();
    if (D.phase != PH_STORM && D.phase != PH_FIRST && D.phase != PH_FALL && D.phase != PH_APPROACH &&
        D.phase < PH_CHARGE)
      Waves();
    else if (D.phase < PH_CHARGE)
      Simple();
    if (D.phase == PH_CHARGE) Waves();
  }
  if (D.k_on) {
    Hold16();
    Place16();
    if ((D.phase >= PH_CYCLE && D.phase < PH_WITHDRAW) && D.phase != PH_APPROACH) Guns();
    Specials();
    Mega();
    Rail();
    Overload();
    Shelling();
    ValGuns();
    Charge();
    Wreck();
  }
  Damage();
  if (D.phase < PH_CHARGE && D.phase != PH_AKANE) {
    // the desktop autopilot: the nearest hunter
    g_autopilot_target = 0;
    for (int k = 0; k < NHUNT; k++) if (D.hunt[k].bg) { g_autopilot_target = D.hunt[k].bg; break; }
  }
  AmaSunTick();
  g_at_dbg[0] = D.phase;
  g_at_dbg[1] = D.cycle;
  g_at_dbg[2] = D.phase == PH_AKANE && D.mag ? SB_GetBadGuyHealth(D.mag) : D.hp;
  g_at_dbg[3] = D.phase == PH_CHARGE ? D.blive * 1000 + D.nbreach : D.bar;
  g_at_dbg[4] = D.over_t;
  g_at_dbg[5] = D.mega + (D.crippled << 4) + (D.esc_lost << 8) + (D.sp << 12);
}

static void AtDeath(u32 boss) {
  (void)boss;
  ShipShield(0);
  for (int i = 0; i < 4; i++) {
    if (D.tur[i]) Remove(D.tur[i]);
    if (D.can[i]) Remove(D.can[i]);
    AnDrop(&D.tur_vis[i]);
    AnDrop(&D.can_vis[i]);
    AnDrop(&D.nac[i]);
  }
  if (D.bow) Remove(D.bow);
  if (D.wrath) Remove(D.wrath);
  for (int k = 0; k < 12; k++) if (D.atk_port[k]) Remove(D.atk_port[k]);
  if (D.atk_bow) Remove(D.atk_bow);
  if (D.atk_mid) Remove(D.atk_mid);
  for (int h = 0; h < 2; h++) if (D.harp[h] && Alive(D.harp[h])) Remove(D.harp[h]);
  for (int k = 0; k < 2; k++)
    if (D.core[k]) { AmaPaintRemove(D.core[k]); Remove(D.core[k]); }
  for (int k = 0; k < NBREACH; k++) {
    if (D.btgt[k]) Remove(D.btgt[k]);
    AnDrop(&D.breach[k]);
  }
  AnDrop(&D.glow);
  AnDrop(&D.aft);
  AnDrop(&D.hull);
  for (int s = 0; s < 2; s++) AnDrop(&D.streamer[s]);
  AnDrop(&D.oni);
  AnDrop(&D.hangar);
  AnDrop(&D.mast);
  AnDrop(&D.over);
  AnDrop(&D.wreck);
  AnDrop(&D.hatch);
  AnDrop(&D.mcannon);
  AnDrop(&D.mret);
  for (int k = 0; k < NMEGA; k++) AnDrop(&D.mbeam[k]);
  GlowDrop(&D.mglow);
  g_autopilot_target = 0;
  AmaJam(0);
  g_graze_jammed = 0;
  SB_PausePartition(BH, 0);
}

void AtlanticBossInit(u32 boss) {
  SB_ClearBytes((void*)boss, BOSS_SIZE);
  SpeechReset();                                  // a new run of the level
  AndamanReset();                                 // the shells, blasts and the Valhalla: clean
  AnUseLines(kAtlanticSentences, AL_COUNT);       // ... with this world's speeches
  g_an_slow = 3;                                  // her guns a third the rate (their noise, nose to nose) ...
  g_an_roam = 1;                                  // ... each turret on a new target after every shot
  AmaSunReset();                                  // the mirror: in orbit ...
  AmaUpgrade(1);                                  // ... with all three mirrors (France's launch)
  SB_ClearBytes(&D, sizeof D);
  D.on = 1;
  R32(boss, B_SPAWN + 0x00) = (s32)(u32)AtSpawn;
  R32(boss, B_SPAWN + 0x04) = (s32)(u32)AtUpdate;
  R32(boss, B_SPAWN + 0x08) = (s32)(u32)AtRender;
  R32(boss, B_SPAWN + 0x0c) = (s32)(u32)AtRender;
  R32(boss, B_SPAWN + 0x10) = (s32)(u32)AtDeath;
  R32(boss, B_HEALTH) = (s32)(u32)AtHealth;
}

int AtlanticOn(void) { return InWorld(); }
