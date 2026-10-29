// chase.c -- Mission 7: the Shogun's pursuit across Siberia, to its Arctic coast.
//
// The whole level is this "boss". The partition raises it a moment after the
// start (antarctica/make_antarctica.py) and stays paused until it is over;
// everything after that is decided here:
//
//   hail     the Doc calls the ship home from Seoul -- then something huge on
//            the scanner behind it;
//   rise     the Kurogane, the Shogun's dreadnought, climbs into the bottom
//            of the screen; the Shogun, the Doc: an emergency, divert, lead
//            him away from UNITY's headquarters;
//   round    the dreadnought chases from below, untouchable -- its hull and
//            everything seen of it are generic entities, its guns badguys
//            that cannot be hurt -- firing up at the ship, while wave after
//            wave of the Shogun's escorts come at it from ahead (a pursuit:
//            nothing launches from the dreadnought). Shooting them down
//            (their health lost, counted here) and the time survived fill
//            the green bar: a chance to break away. About half-way, the
//            Tenshu cannon (below);
//   break    full: the ship pulls away, the Kurogane falls behind, off the
//            bottom of the screen ...
//   catch    ... and catches up again, afterburners blazing. Twice: over
//            Siberia, then over the mountains; after the second the Doc
//            calls HQ;
//   call     HQ launches the Valkyrie, Volta's railgun ship;
//   rail     she comes in shield up, Volta introduces herself, and they roam
//            the top of the screen; her railgun turret swings round and locks
//            on an engine nacelle (Volta tells the Shogun he's going down),
//            charges behind its aiming line -- holding while the ship is in
//            it -- and holds its beam on the nacelle until it blows, then
//            turns back to the front to reload. The bar is the four engines;
//            each one lost, the Kurogane staggers and drops back; with two
//            gone the Shogun turns all guns loose. His turrets shell her
//            shield; her wing guns pick off his escorts;
//   down     the fourth: the Tenshu cannon turns on the Valkyrie -- a
//            cutscene: her shield gives, her wings burn, she lurches away out
//            of control and crashes ("Mayday!");
//   last     the last stand: everything he has, cannon and all, against the
//            clock (the bar);
//   escape   his guns blow, the hull sinks off the bottom in flames. Last
//            words -- Volta ejected safely -- and the level ends.
//
// The Tenshu cannon: the keep's hatch opens, the cannon rises and runs out
// its barrel while his other guns fire on; it locks on, fires a huge beam
// that sweeps after the ship, then sinks back into the keep under the
// hardest pattern in the level. See Mega().
//
// The dreadnought is in pieces, the way the game's own big bosses are
// (antarctica/art_fleet.py): the hull (intact, damaged, wrecked), the oni
// mask on the bow that wakes and roars as the bow gun fires, the hangar
// (its doors kept shut), two deck turrets and two heavy ones, four flak cannons on the gunwales and
// four engine nacelles; the guns' turrets turn to their targets and flash
// when they fire. Generics draw over badguys, so each gun is an invisible
// emitter under the piece that is seen of it.
//
// The ship is kept above the bow (it would only fly under the hull, and
// nothing there can be fought), and the scroll is paced from here so the
// route -- taiga, the ice mountains, the Arctic coast and sea -- comes up
// as the story reaches it (ROUTE_*, as make_antarctica.py lays it out).

#include "seoul_boss.h"

#define SH_WORLD      0x4e6c
#define SH_SPEED      0x5c      // the scroll: current speed (16.16, 64ths of a unit a frame)
#define SH_SPEED_TO   0x60      // ... the speed event's target
#define SH_SPEED_STEP 0x64      // ... and its step a frame

// the route (props units along the scroll, make_antarctica.py ROUTE)
#define ROUTE_START   FX(78.67)
#define ROUTE_MOUNT   FX(300)
#define ROUTE_ANT     FX(470)
#define ROUTE_END     FX(790)

// the dreadnought: its centre, and its parts relative to it, in playfield
// units (antarctica/art_fleet.py layout(); make_antarctica.py checks these
// tables against the art)
#define HULL_Y        352
#define HULL_BOW      80        // the bow's tip, above the centre
#define CLAMP_GAP     6         // the ship stays this far above it
#define BOW_DY        (-53)
#define HANGAR_DY     (-25)
#define ONI_DY        (-50)
#define KEEP_DY       32        // the keep, where the Tenshu cannon rises
#define AFT_DY        99        // the aft section, under the hull: it runs on off the screen
#define STREAMER_DX   50        // the war streamers, tied to the horn tips (and mirrored) ...
#define STREAMER_DY   (-75)
#define STREAMER_LEN  64        // ... streaming back this far
#define STAGGER       12        // how far it drops back for each engine lost
static const signed char kNacX[4] = { -127, -77, 77, 127 };
static const signed char kNacY[4] = { 22, 19, 19, 22 };
static const signed char kTurX[4] = { -34, 34, -52, 52 };
static const signed char kTurY[4] = { -12, -12, 31, 31 };
static const signed char kCanX[4] = { -57, 57, -63, 63 };
static const signed char kCanY[4] = { 2, 2, 47, 47 };
static const unsigned char kEngineOrder[4] = { 1, 2, 0, 3 };   // inner ones first

// the Valkyrie
#define VAL_Y         72
#define VAL_PIVOT_DY  4         // her railgun turret's pivot, below her centre
#define BARREL        58        // ... to its muzzle
#define FRONT         0xc00000  // the turret at rest: up the screen, the way she flies
// her animations (chase_cast.py valkyrie.generic: the engine allows 0-7 only)
#define VA_CHARGED    4
#define VA_FIRING     5
#define VA_DOWN       6
static const signed char kWingX[2] = { -40, 40 };   // her wing guns, out on the wingtips
#define WING_DY       (-10)
#define WING_RANGE    170       // they reach this far
#define WING_EVERY    18        // frames between each gun's shots
#define WING_DAMAGE   12        // a light hit: a few to bring down a small escort
#define WING_MUZZLE   13
#define BOLT_SPEED    9         // units a frame
#define NBOLT         6
#define SHIELD_R      66        // her shield's radius
#define SEG           40        // one length of beam or sight
#define NSEG          8
#define LINE_HALF     16        // the ship is "in the line" this close to it
#define AIM_TIME      175       // frames of charging before the shot
#define BEAM_OPEN     6         // the beam widening ...
#define BEAM_HOLD     84        // ... held on the nacelle until it blows ...
#define BEAM_END      98        // ... and dying away
#define COOL_TIME     300       // between shots

// the bar in each round: points, sixteenths -- about 0.7 of the escorts'
// health lost, and the time survived (alone it fills a round in ~120 s and
// ~145 s; fighting well, in well under a minute each)
#define ROUND1_MAX    10000
#define ROUND2_MAX    12000
#define TIME_RATE     22        // (40% faster than at first, as the damage below)
#define BREAK_TIME    420       // frames pulling away before he catches up
#define LAST_TIME     2400      // the last stand
#define CALL_WAIT     600       // frames between the Doc's call and the Valkyrie (10 s)
#define FALL_HOLD     180       // frames the Valkyrie lurches about in view before she goes
#define EJECT_T       75        // ... when Volta ejects

// the Tenshu cannon
#define MEGA_BARREL   60        // its pivot to the muzzle, run out
#define NMEGA         12        // lengths of beam: past the edge of the screen
#define MEGA_HALF     15        // the beam hurts this close to its line
#define MEGA_GRACE    30
#define MEGA_LOCK     70        // frames turning and locking on
#define MEGA_FIRE     170       // the beam
#define MEGA_SWEEP    (TURN / 720)    // how fast it follows the ship, a frame
#define MEGA_SINK     300       // sinking back, under the parting pattern
#define MEGA_EVERY    900       // frames before it may rise again
#define MEGA_LIFT     40        // the Kurogane rises this much to bring it to bear
enum { MG_OFF, MG_RISE, MG_LOCK, MG_FIRE, MG_SINK };
#define DOOM_MEGA_EVERY 420     // Mission 8's doom: the cannon comes round sooner

#define REG_SNAKE_X   14        // XSnake's point to drift for (Fire(); the script copies it to R10, R11)
#define REG_SNAKE_Y   15
// the guns' commands (antarctica/chase_cast.py CMDS)
enum { G_FAN = 1, G_STREAM, G_FLAK, G_MISSILES, G_LANCE, G_SWEEP, G_CURTAIN, G_BLOOM, G_BARRAGE, G_PEPPER,
       G_WRATH,
       // simple: a slow two-armed spiral; scattered slow shells; two aimed
       // streams with a safe gap between them
       G_SPIRAL, G_RAIN, G_TWIN,
       // fast: quick aimed shots (keep moving); a fast laser raked across the
       // screen (cross it); rows with one gap that slides across (follow it)
       G_SNIPE, G_RAKE, G_GATE,
       // slow patterns, to fill the sky between the rest: rings each turned
       // half a step; two spirals crossing into a lattice; staggered rows up
       // the screen; a stream swinging side to side
       G_PULSE, G_WEAVE, G_COMB, G_SNAKE,
       // a five-pointed star of shells that grows as it comes, three of
       // them turned a tenth each
       G_STAR,
       // hard mode's (beach/hardkit.py boss_attacks, docs/HARD_MODE.md): rings that
       // brake and snipe; pincers curling onto the ship; rings that turn at once;
       // the snake onto the bottom centre; rings with a drifting gap
       G_XSNIPE, G_XPINCER, G_XTWIST, G_XSNAKE, G_XGAP };
#define REG_STEPS 12            // a barrage's shells live this many steps

enum { P_HAIL = 1, P_RISE, P_ROUND, P_BREAK, P_CATCH, P_CALL, P_ARRIVE, P_RAIL, P_DOWN, P_LAST, P_ESCAPE,
       P_GONE, P_DOOM };
enum { R_TURN, R_AIM, R_FIRE, R_COOL };

#define NTRACK 24
#define MAX_UP 5                // a new wave waits while more escorts than this fly
#define MAX_UP_LATE 3           // ... once the Valkyrie is up
#define CALM 45                 // frames with every gun quiet before a lull wave: its shots clear
#define FILL_EVERY 90           // frames between light attacks (LightAttack())
#define FILL_EVERY_LATE 70      // ... once the Valkyrie is up (60 with two engines gone)
#define WAVE_HOLD 180           // ... and the guns hold this long after it comes (fury: 120)

static struct {
  u32 boss;
  int phase, rail, t, sub_t;
  u32 frame;
  // the dreadnought: what is seen of it ...
  u32 glow, aft, hull, streamer[2], nac[4], blaze[4], oni, hangar;
  u32 tur_vis[4], can_vis[4];
  // ... and its guns, invisible emitters
  u32 tur[4], can[4], bow, wrath;
  // the Tenshu cannon
  u32 hatch, mcannon, reticle, mbeam[NMEGA];
  int mega, mega_t, mega_val, mega_cool, mega_grace, said_mega, mega_done;
  s32 mega_ang;
  s32 hx, hy, base_y;
  int engines, lost[4], stun;
  int gun_cool, gun_turn, bow_turn, tur_turn, can_turn;
  int oni_t;
  int at_val[4];                // frames a turret keeps its aim on the Valkyrie
  int barrage_cool, barrage_turn;
  // escorts
  int wave_cool, wave_turn;
  int fill_cool, fill_turn;     // the light attacks (LightAttack())
  unsigned char cmd_tur[4], cmd_can[4], cmd_bow;   // what each gun was last told
  int lull, calm, hold;          // a wave waiting for the guns to finish; frames all quiet; guns held
  u32 track[NTRACK];
  s32 track_hp[NTRACK];
  s32 progress;                 // sixteenths of a point
  int round;
  s32 round_max, last_left;
  // the Valkyrie
  u32 val, rg, shield, orb, impact, sight[NSEG], beam[NSEG];
  u32 wing_vis[2];               // her wing guns, and their bolts in flight
  s32 wing_ang[2];
  int wing_cool[2], wing_flash[2];
  u32 bolt[NBOLT], bolt_to[NBOLT];
  s32 bolt_x[NBOLT], bolt_y[NBOLT], bolt_dx[NBOLT], bolt_dy[NBOLT];
  int bolt_left[NBOLT];
  s32 vx, vy, gun_ang;          // rg: her railgun turret; gun_ang its heading
  u32 roam;
  int target, aim;
  u32 shield_from, shield_until;
  u32 wing_fire[2];
  int falling, fall_t, fall_side, crashed;
  s32 val_rot;                  // her tilt and roll as she goes down (her sprite rotates)
  u32 pod;                      // Volta's ejection pod, then under its chute
  s32 pod_x, pod_y, pod_dx, pod_dy;
  int pod_t;
  // the scroll
  s32 pos;                      // where the props are, 16.16 units
  s32 speed_want;
  int quiet;                    // frames since anyone last spoke
  int lance_grace;
  int jammed;                   // Mission 8's secret: the access codes (DoomJam)
} D;

int ShieldUp(void);             // missile.c: the capsule shield is up

static u32 GenericActor(u32 e) { return e ? SB_GetGenericActor(e) : 0; }

static u32 Spawn(const char* name, s32 x, s32 y) {
  const u32 a = GenericActor(SB_SpawnGeneric(BH, name, x, y));
  if (a) {
    SB_SetActorPosition(a, x, y);
    SB_SetActorRegister(a, REG_CMD, 0);
  }
  return a;
}

static void Drop(u32* a) {                     // a held generic: gone
  if (*a) SB_SetActorRegister(*a, REG_CMD, FX(1));
  *a = 0;
}

static void GenAnim(u32 a, int n) { if (a) SB_SetActorRegister(a, REG_ANIM, FX(n)); }
static void Put(u32 a, s32 x, s32 y) { if (a) SB_SetActorPosition(a, x, y); }

static s32 AngleTo(s32 x0, s32 y0, s32 x1, s32 y1) {
  return SB_GetAngle(x0 >> 16, y0 >> 16, x1 >> 16, y1 >> 16) & (TURN - 1);
}

static s32 Abs(s32 v) { return v < 0 ? -v : v; }

// About the distance, in whole units (the larger leg plus half the smaller:
// within an eighth -- there is no square root here).
static s32 Dist(s32 dx, s32 dy) {
  dx = Abs(dx) >> 16;
  dy = Abs(dy) >> 16;
  return dx > dy ? dx + (dy >> 1) : dy + (dx >> 1);
}

// Nobody has spoken for a while: the box can look closed for a frame or two
// between one sentence and the next, so a story beat waits for this.
static int Quiet(void) { return D.quiet > 40; }

// A line on screen holds the fight (his guns, and the ship can't be hit)
// only until the Valkyrie is on station: from then on the lines come thick
// and fast -- every shot, every engine -- and the fight goes on under them.
static int Paused(void) { return InCutscene() && D.phase <= P_ARRIVE; }

// ---- the scroll ------------------------------------------------------------------------
// Written directly every frame, eased: the partition is paused, so no speed
// event fights it.
static void Scroll(void) {
  const u32 sh = Shogun();
  s32 cur = R32(sh, SH_SPEED);
  s32 want = D.speed_want;
  // never run off the end of the route (the props would start over)
  if (D.pos > ROUTE_END - FX(40) && want > FX(1.2)) want = FX(1.2);
  if (D.pos > ROUTE_END - FX(14)) want = FX(0.3);
  const s32 d = want - cur;
  cur += d > FX(0.02) ? FX(0.02) : (d < -FX(0.02) ? -FX(0.02) : d);
  R32(sh, SH_SPEED) = cur;
  R32(sh, SH_SPEED_TO) = cur;
  R32(sh, SH_SPEED_STEP) = 0;
  D.pos += cur >> 6;
}

// ---- escorts: tracked, so the damage done to them counts -------------------------------
static void Track(u32 bg) {
  if (!bg) return;
  for (int i = 0; i < NTRACK; i++)
    if (!D.track[i]) { D.track[i] = bg; D.track_hp[i] = SB_GetBadGuyHealth(bg); return; }
}

static int Listed(u32 bg) {
  for (u32 b = SB_GetFirstBadGuy(BH); b; b = SB_GetNextBadGuy(BH, b))
    if (b == bg) return 1;
  return 0;
}

static void CountDamage(void) {
  for (int i = 0; i < NTRACK; i++) {
    const u32 bg = D.track[i];
    if (!bg) continue;
    if (!Listed(bg)) { D.track[i] = 0; continue; }
    s32 hp = SB_GetBadGuyHealth(bg);
    if (hp < 0) hp = 0;
    if (hp < D.track_hp[i] && D.phase == P_ROUND) D.progress += ((D.track_hp[i] - hp) * 45) >> 2;
    D.track_hp[i] = hp;
    if (hp == 0) D.track[i] = 0;
  }
}

// Mission 8's doom flies Tokyo's ships only: Edinburgh's world file lists
// only their sprites, not the other worlds' ships or the capsule carriers.
// (That was for a "~64 sprites a world" limit that turned out not to exist
// -- docs/ENGINE_LIMITS.md; list the others in edinburgh.world to fly them.)
static const char* DoomShip(const char* name) {
  static const char* const kTokyo[] = { "worlds/storm/scripts/badguy01", "worlds/storm/scripts/badguy02",
                                        "worlds/storm/scripts/badguy03", "worlds/storm/scripts/badguy08",
                                        "worlds/storm/scripts/badguy09" };
  static int turn;
  if (name[0] == 'w' && name[7] == 's' && name[8] == 't') return name;      // "worlds/storm/..."
  if (++turn >= 5) turn = 0;
  return kTokyo[turn];
}

static u32 Escort(const char* name, s32 x, s32 y) {
  if (D.phase == P_DOOM) name = DoomShip(name);
  const u32 bg = SB_SpawnBadGuy(BH, name, FX(x), FX(y));
  Track(bg);
  if (!g_autopilot_target || !Alive(g_autopilot_target)) g_autopilot_target = bg;
  return bg;
}

// The escorts, from the original game's worlds: Tokyo's (storm) ...
#define S01 "worlds/storm/scripts/badguy01"
#define S02 "worlds/storm/scripts/badguy02"
#define S03 "worlds/storm/scripts/badguy03"
#define S08 "worlds/storm/scripts/badguy08"
#define S09 "worlds/storm/scripts/badguy09"
// ... and a few from Volcano, the desert city and Ocean, for variety
#define V02 "worlds/volcano/scripts/badguy02"       // fans of disks, ahead and behind
#define D05 "worlds/desertcity/scripts/badguy05"    // a pair of missiles, then away
#define D06 "worlds/desertcity/scripts/badguy06"    // aimed lasers, then away
#define D07 "worlds/desertcity/scripts/badguy07"    // stops, sprays at the ship, dives
#define O1C "worlds/ocean/scripts/badguy01c"        // big lasers, in and out fast
// The capsule carriers (chase_cast.py): shot down, each drops the original
// game's blue capsule, as Ocean's and Volcano's carriers did.
#define CARRIER_A "*antarctica/scripts/carrier_a"
#define CARRIER_B "*antarctica/scripts/carrier_b"
// Hard mode's elite (chase_cast.py): a crimson crow that dives in from ahead,
// closes pincers on the ship, lets go a ring that turns at once, dives past.
#define KARASU_X "*antarctica/scripts/karasu_x"

static void Line5(const char* name, int x0, int dx, int y0, int dy, int n) {
  for (int k = 0; k < n; k++) Escort(name, x0 + dx * k, y0 + dy * k);
}

// Escorts still flying (tracked, not yet shot down).
static int EscortsUp(void) {
  int n = 0;
  for (int i = 0; i < NTRACK; i++) if (D.track[i] && D.track_hp[i] > 0) n++;
  return n;
}

// A wave comes when the last has mostly gone: never more than about
// MAX_UP escorts on screen together. Most waves come in a lull: when one is
// due, the dreadnought starts no new attack (Guns(), D.lull); once all its
// guns have finished and their shots have had CALM frames to clear, the
// wave comes, and the guns hold a while longer to let the ship meet it. The
// rest (5 of the 14) still come in the thick of the fire.
#define NWAVES 14
static const unsigned char kInLull[NWAVES] = { 1, 0, 1, 0, 1, 1, 0, 1, 1, 1, 0, 1, 1, 0 };

static void Waves(void) {
  if (D.wave_cool > 0) { D.wave_cool--; return; }
  // once the Valkyrie is up, fewer escorts: at most MAX_UP_LATE, and the
  // waves further apart (below) -- the dreadnought does more of the fighting
  // ... and fewer still for every engine she takes (3, 2, 1)
  const int hard = HardMode();
  const int max_up = (D.phase >= P_ARRIVE ? (D.engines >= 3 ? MAX_UP_LATE : D.engines) : MAX_UP) + hard;
  if (EscortsUp() > max_up) { D.lull = 0; D.wave_cool = 20; return; }
  // waves in a lull only before the Valkyrie: from then on his guns don't
  // pause for them
  if (kInLull[D.wave_turn] && D.phase < P_ARRIVE) {
    if (D.calm < CALM) { D.lull = 1; return; }
    D.lull = 0;
    D.hold = D.engines <= 2 ? 120 : WAVE_HOLD;
  }
  const s32 px = PlayerX() >> 16;
  s32 near = px < 130 ? 130 : (px > 270 ? 270 : px);
  switch (D.wave_turn) {
  case 0:
    Line5(S03, 110, 45, -20, -12, 5);
    break;
  case 1:
    Line5(S08, 130, 70, -24, 0, 3);
    Escort(S01, near, -60);
    break;
  case 2:                                       // a capsule carrier, with an escort of lancers
    Escort(CARRIER_A, 200, -40);
    Escort(O1C, 120, -20);
    Escort(O1C, 280, -20);
    break;
  case 3:
    Escort(D06, near - 60, -30);
    Escort(D06, near + 60, -30);
    break;
  case 4:
    Line5(V02, 130, 70, -30, -10, 3);
    break;
  case 5:
    Escort(S02, 200, -40);
    Line5(S03, 140, 30, -70, -10, 4);
    break;
  case 6:
    Escort(D07, 140, -30);
    Escort(D07, 260, -50);
    break;
  case 7:
    Line5(D05, 120, 80, -24, -12, 3);
    break;
  case 8:
    Line5(S03, 280, -40, -20, -12, 5);
    Escort(S01, near, -80);
    break;
  case 9:                                       // the other carrier, flanked
    Escort(CARRIER_B, 200, -50);
    Line5(S09, 110, 180, -60, 0, 2);
    break;
  case 10:
    Line5(O1C, 140, 60, -20, -10, 3);
    break;
  case 11:
    Escort(S02, 140, -40);
    Escort(S02, 260, -70);
    break;
  case 12:
    Line5(S08, 110, 60, -24, -14, 4);
    Escort(D06, near, -60);
    break;
  case 13:
    Line5(S01, 120, 80, -30, -20, 3);
    Escort(V02, 200, -80);
    break;
  }
  // hard mode: crimson crows with every wave -- a pair either side of the ship
  // with every other one, one straight ahead of it with the rest
  if (hard) {
    if (D.wave_turn & 1) { Escort(KARASU_X, near - 90, -30); Escort(KARASU_X, near + 90, -50); }
    else Escort(KARASU_X, near, -40);
  }
  if (++D.wave_turn >= NWAVES) D.wave_turn = 0;
  D.wave_cool = D.phase == P_ROUND || D.phase == P_DOOM ? 230
              : (D.phase >= P_ARRIVE ? 420 + 120 * (4 - D.engines) : 330);
  if (hard) D.wave_cool = D.wave_cool * 3 / 4;
}

// ---- the dreadnought ----------------------------------------------------------------------
static void Build(void) {
  const s32 x = D.hx, y = D.hy;
  // spawned bottom to top: later generics draw over earlier ones
  D.glow = Spawn("*antarctica/boss/underglow", x, y);
  D.aft = Spawn("*antarctica/boss/aft", x, y + FX(AFT_DY));
  D.hull = Spawn("*antarctica/boss/hull", x, y);
  for (int s = 0; s < 2; s++)
    D.streamer[s] = Spawn("*antarctica/boss/streamer", x + FX(s ? STREAMER_DX : -STREAMER_DX),
                          y + FX(STREAMER_DY + STREAMER_LEN / 2));
  for (int i = 0; i < 4; i++) D.nac[i] = Spawn("*antarctica/boss/nacelle", x + FX(kNacX[i]), y + FX(kNacY[i]));
  D.hangar = Spawn("*antarctica/boss/hangar", x, y + FX(HANGAR_DY));
  D.oni = Spawn("*antarctica/boss/oni", x, y + FX(ONI_DY));
  for (int i = 0; i < 4; i++) {
    const s32 cx = x + FX(kCanX[i]), cy = y + FX(kCanY[i]);
    D.can[i] = SB_SpawnBadGuy(BH, "*antarctica/boss/flak", cx, cy);
    D.can_vis[i] = Spawn("*antarctica/boss/cannon_vis", cx, cy);
  }
  for (int i = 0; i < 4; i++) {
    const s32 tx = x + FX(kTurX[i]), ty = y + FX(kTurY[i]);
    D.tur[i] = SB_SpawnBadGuy(BH, "*antarctica/boss/turret", tx, ty);
    // the rear pair are the heavy guns
    D.tur_vis[i] = Spawn(i < 2 ? "*antarctica/boss/turret_vis" : "*antarctica/boss/turret_big_vis", tx, ty);
  }
  D.bow = SB_SpawnBadGuy(BH, "*antarctica/boss/bow_gun", x, y + FX(BOW_DY));
  D.wrath = SB_SpawnBadGuy(BH, "*antarctica/boss/wrath", x, y + FX(KEEP_DY));
  D.engines = 4;
}

// The oni on the bow follows the bow gun: dormant; waking as it charges (the
// eyes kindle, the jaw drops); roaring while it fires; cracked once the
// dreadnought is badly hurt.
static int OniFrame(void) {
  const int st = D.bow ? GetReg(D.bow, REG_ANIM) >> 16 : 0;
  if (st == 1) {
    D.oni_t++;
    return D.oni_t < 16 ? 1 : (D.oni_t < 32 ? 2 : 3);
  }
  D.oni_t = 0;
  if (st == 2) return 6;                       // the roaring loop
  return D.engines <= 2 ? 5 : 0;
}

static void PlaceDreadnought(void) {
  const s32 x = D.hx, y = D.hy;
  const int hurt = D.engines >= 3 ? 0 : (D.engines >= 1 ? 1 : 2);
  Put(D.aft, x, y + FX(AFT_DY));
  GenAnim(D.aft, hurt);
  Put(D.hull, x, y);
  GenAnim(D.hull, hurt);
  for (int s = 0; s < 2; s++)
    Put(D.streamer[s], x + FX(s ? STREAMER_DX : -STREAMER_DX), y + FX(STREAMER_DY + STREAMER_LEN / 2));
  // the red glow round the hull while the Tenshu cannon is up: smouldering
  // as it rises and sinks, pulsing as it locks on, blazing as it fires
  Put(D.glow, x, y);
  GenAnim(D.glow, D.mega == MG_RISE || D.mega == MG_SINK ? 1 : (D.mega == MG_LOCK ? 3 : (D.mega == MG_FIRE ? 2 : 0)));
  for (int i = 0; i < 4; i++) {
    const s32 nx = x + FX(kNacX[i]), ny = y + FX(kNacY[i]);
    Put(D.nac[i], nx, ny);
    Put(D.blaze[i], nx, ny - FX(8));
  }
  Put(D.hangar, x, y + FX(HANGAR_DY));
  Put(D.oni, x, y + FX(ONI_DY));
  if (D.oni) GenAnim(D.oni, OniFrame());
  const s32 px = PlayerX(), py = PlayerY();
  for (int i = 0; i < 4; i++) {
    const s32 tx = x + FX(kTurX[i]), ty = y + FX(kTurY[i]);
    if (D.tur[i]) Place(D.tur[i], tx, ty);
    if (D.tur_vis[i]) {
      Put(D.tur_vis[i], tx, ty);
      // turned on the ship, or on the Valkyrie while it shells her
      const int on_val = D.at_val[i] > 0 && D.val;
      if (D.at_val[i] > 0) D.at_val[i]--;
      SB_SetActorOrient(D.tur_vis[i], on_val ? AngleTo(tx, ty, D.vx, D.vy) : AngleTo(tx, ty, px, py));
      if (D.tur[i]) GenAnim(D.tur_vis[i], GetReg(D.tur[i], REG_ANIM) >> 16);
    }
    const s32 cx = x + FX(kCanX[i]), cy = y + FX(kCanY[i]);
    if (D.can[i]) Place(D.can[i], cx, cy);
    if (D.can_vis[i]) {
      Put(D.can_vis[i], cx, cy);
      SB_SetActorOrient(D.can_vis[i], AngleTo(cx, cy, px, py));
      if (D.can[i]) GenAnim(D.can_vis[i], GetReg(D.can[i], REG_ANIM) >> 16 ? 1 : 0);
    }
  }
  if (D.bow) Place(D.bow, x, y + FX(BOW_DY));
  if (D.wrath) Place(D.wrath, x, y + FX(KEEP_DY));
}

// The hull follows the ship across, lazily, and bobs.
static void Chase(void) {
  s32 want = PlayerX();
  if (want < FX(150)) want = FX(150);
  if (want > FX(250)) want = FX(250);
  D.hx += Ease(want - D.hx, FX(0.45));
  const s32 bob = SB_MulFP(SB_Sin((s32)(D.frame << 15)), FX(2.5));
  const s32 lift = D.mega && !D.mega_val ? FX(MEGA_LIFT) : 0;   // presenting the Tenshu cannon
  D.hy += Ease(D.base_y - lift + bob - D.hy, FX(1.2));
}

// Keep the ship above the bow.
static void Clamp(void) {
  const s32 limit = D.hy - FX(HULL_BOW + CLAMP_GAP);
  if (PlayerY() > limit) R32(Shogun(), SH_PLAYER_Y) = limit;
}

static int GunsBusy(void) {
  for (int i = 0; i < 4; i++) if (D.tur[i] && Busy(D.tur[i])) return 1;
  return D.bow && Busy(D.bow);
}

// Every gun quiet, the flak cannons and the cannon's parting pattern too.
static int AllQuiet(void) {
  if (D.mega || GunsBusy()) return 0;
  for (int i = 0; i < 4; i++) if (D.can[i] && Busy(D.can[i])) return 0;
  return !(D.wrath && Busy(D.wrath));
}

// ---- what may fire together -------------------------------------------------------------
// His attacks are heavy (the turrets' and the bow gun's main attacks) or
// light (the slow patterns, the gunwale cannons' pepper, flak and rain). How
// many of each may run at once depends on the fight (Caps()); the guns
// remember what they were last told (cmd_*) so the running ones can be
// counted, and any over the limit -- when it tightens, as the Tenshu
// cannon rises -- are cut short (Stop(): every pause in an attack checks).
static int Light(int cmd) {
  return cmd == G_PULSE || cmd == G_WEAVE || cmd == G_COMB || cmd == G_SNAKE || cmd == G_SPIRAL ||
         cmd == G_RAIN || cmd == G_PEPPER || cmd == G_FLAK;
}

static void Fire(u32 bg, unsigned char* slot, int cmd) {
  // hard mode's snake: its bullets drift for the ship (R14, R15 -- the chase's
  // XSnake, chase_cast.py gun_script; hardkit's aims at the bottom middle,
  // where the Kurogane is)
  if (cmd == G_XSNAKE) {
    SetReg(bg, REG_SNAKE_X, PlayerX());
    SetReg(bg, REG_SNAKE_Y, PlayerY());
  }
  Order(bg, cmd);
  *slot = (unsigned char)cmd;
}

// While the Tenshu cannon is up: in the first round nothing else fires; in
// the second, one light attack; from the second catch-up on, one of each;
// with his engines gone, two of each. Otherwise: one heavy and one light
// until two engines are gone, then two heavy; with all four gone, two of
// each (and no more escorts).
static void Caps(int* heavy, int* light) {
  if (D.mega && !D.mega_val) {
    if (D.phase == P_DOOM) { *heavy = 2; *light = 2; }     // Mission 8: no mercy
    else if (D.phase < P_CALL && D.round <= 1) { *heavy = 0; *light = 0; }
    else if (D.phase < P_CALL) { *heavy = 0; *light = 1; }
    else if (D.engines > 0) { *heavy = 1; *light = 1; }
    else { *heavy = 2; *light = 2; }
    return;
  }
  if (D.engines == 0 || D.phase == P_DOOM) { *heavy = 3; *light = 2; }   // everything he has left
  else if (D.engines <= 2) { *heavy = 2; *light = 1; }
  else { *heavy = 1; *light = 1; }
  // hard mode (docs/HARD_MODE.md): a second layer from the start, a third later
  if (HardMode()) { (*heavy)++; if (D.engines <= 2) (*light)++; }
}

// How often he starts an attack: faster as the fight goes on, and faster
// still with every engine the Valkyrie takes.
static int HeavyEvery(void) {
  static const unsigned char kByEngines[5] = { 18, 22, 28, 36, 44 };   // 0..4 engines left
  const int hard = HardMode();                  // hard mode: a quarter sooner
  if (D.phase >= P_ARRIVE) return hard ? kByEngines[D.engines] * 3 / 4 : kByEngines[D.engines];
  const int half = D.phase == P_ROUND && D.progress > (D.round_max << 3);
  const int t = D.round >= 2 ? (half ? 48 : 60) : (half ? 56 : 72);
  return hard ? t * 3 / 4 : t;
}

static int LightEvery(void) {
  static const unsigned char kByEngines[5] = { 32, 40, 48, 60, FILL_EVERY_LATE };
  const int t = D.phase >= P_ARRIVE ? kByEngines[D.engines] : FILL_EVERY;
  return HardMode() ? t * 3 / 4 : t;
}

// Count what is running; stop whatever is over the limits.
static void Running(int* heavy, int* light, int cap_heavy, int cap_light) {
  *heavy = 0;
  *light = 0;
  for (int i = 0; i < 4; i++) {
    if (D.tur[i] && Busy(D.tur[i]) && D.cmd_tur[i] != G_BARRAGE) {
      int* n = Light(D.cmd_tur[i]) ? light : heavy;
      if (*n >= (Light(D.cmd_tur[i]) ? cap_light : cap_heavy)) Stop(D.tur[i]);
      else (*n)++;
    }
    if (D.can[i] && Busy(D.can[i])) {
      if (*light >= cap_light) Stop(D.can[i]);
      else (*light)++;
    }
  }
  if (D.bow && Busy(D.bow)) {
    if (*heavy >= cap_heavy) Stop(D.bow);
    else (*heavy)++;
  }
}

// A light attack: the gunwale cannons' own fire, or one of the slow
// patterns from a free turret, so the sky never goes quiet -- also while a
// wave is being met (D.hold). Not while a wave waits for the guns to finish
// (D.lull), which needs the sky clear.
static void LightAttack(void) {
  if (D.fill_cool > 0) { D.fill_cool--; return; }
  if (D.lull) return;
  if (D.fill_turn & 1) {
    // the cannons: peppering fans, slow rings to graze, rain
    static const unsigned char kCan[4] = { G_PEPPER, G_FLAK, G_PEPPER, G_RAIN };
    int i = D.can_turn & 3;
    for (int k = 0; k < 4 && (!D.can[i] || Busy(D.can[i])); k++) i = (i + 1) & 3;
    if (D.can[i] && !Busy(D.can[i])) Fire(D.can[i], &D.cmd_can[i], kCan[(D.can_turn >> 2) & 3]);
    D.can_turn++;
  } else {
    static const unsigned char kFill[] = { G_PULSE, G_COMB, G_WEAVE, G_SNAKE, G_PULSE, G_SPIRAL, G_COMB, G_SNAKE };
    // the rear (heavy) turrets first, then the deck ones
    static const unsigned char kOrder[] = { 2, 3, 0, 1 };
    for (int k = 0; k < 4; k++) {
      const int i = kOrder[(D.fill_turn / 2 + k) & 3];
      if (!D.tur[i] || Busy(D.tur[i])) continue;
      Fire(D.tur[i], &D.cmd_tur[i], kFill[(D.fill_turn / 2) & 7]);
      break;
    }
  }
  D.fill_turn++;
  D.fill_cool = LightEvery();
}

static void Guns(void) {
  if (D.stun > 0) { D.stun--; return; }
  if (Paused()) return;
  int cap_heavy, cap_light, heavy, light;
  Caps(&cap_heavy, &cap_light);
  Running(&heavy, &light, cap_heavy, cap_light);
  const int fury = D.engines <= 2;              // "All guns!"
  if (light < cap_light) LightAttack();
  // a wave is coming in a lull (Waves()): no new heavy attack until it has
  // come, and then a breather while the ship meets it (light ones go on)
  if (D.hold > 0) { D.hold--; return; }
  if (D.lull) return;
  if (D.gun_cool > 0) { D.gun_cool--; return; }
  if (heavy >= cap_heavy) return;
  // Before the Valkyrie, the simpler set; once she is up (and to the end) the
  // fast ones join in, and he fires more often (below).
  const int late = D.phase >= P_ARRIVE;
  int i = D.tur_turn & 3;
  for (int k = 0; k < 4 && (!D.tur[i] || Busy(D.tur[i])); k++) i = (i + 1) & 3;
  const int tur_free = D.tur[i] && !Busy(D.tur[i]);
  const int bow_free = D.bow && !Busy(D.bow);
  if (!tur_free && !bow_free) return;          // every gun busy: try again next frame
  // every fourth attack is the bow gun's (or whenever the turrets are all busy)
  if (bow_free && (++D.gun_turn >= 4 || !tur_free)) {
    D.gun_turn = 0;
    static const unsigned char kBow[] = { G_LANCE, G_STAR, G_CURTAIN, G_SWEEP, G_BLOOM };
    static const unsigned char kBowLate[] = { G_LANCE, G_RAKE, G_STAR, G_CURTAIN, G_BLOOM, G_RAKE, G_SWEEP };
    // hard mode: twist rings and drifting-gap rings from the bow
    static const unsigned char kBowH[] = { G_LANCE, G_XTWIST, G_STAR, G_XGAP, G_CURTAIN, G_BLOOM };
    static const unsigned char kBowLateH[] = { G_LANCE, G_XTWIST, G_RAKE, G_XGAP, G_STAR, G_CURTAIN, G_XTWIST, G_SWEEP };
    const int hard = HardMode();
    const unsigned char* list = hard ? (late ? kBowLateH : kBowH) : (late ? kBowLate : kBow);
    const int n = hard ? (late ? (int)sizeof kBowLateH : (int)sizeof kBowH)
                       : (late ? (int)sizeof kBowLate : (int)sizeof kBow);
    if (D.bow_turn >= n) D.bow_turn = 0;
    Fire(D.bow, &D.cmd_bow, list[D.bow_turn]);
    if (++D.bow_turn >= n) D.bow_turn = 0;
  } else {
    static const unsigned char kTur[] = { G_FAN, G_STREAM, G_TWIN, G_MISSILES, G_STAR, G_TWIN, G_STREAM, G_MISSILES };
    static const unsigned char kTurLate[] = { G_SNIPE, G_FAN, G_GATE, G_STAR, G_SNIPE, G_MISSILES, G_STREAM, G_GATE };
    // hard mode: pincers, snipe rings and the snake among the turrets' (8 each)
    static const unsigned char kTurH[] = { G_FAN, G_XPINCER, G_TWIN, G_MISSILES, G_XSNIPE, G_STAR, G_XSNAKE, G_STREAM };
    static const unsigned char kTurLateH[] = { G_SNIPE, G_XPINCER, G_GATE, G_XSNIPE, G_STAR, G_XSNAKE, G_MISSILES, G_XPINCER };
    const unsigned char* list = HardMode() ? (late ? kTurLateH : kTurH) : (late ? kTurLate : kTur);
    // each turret in turn, each turn of all four the next attack
    Fire(D.tur[i], &D.cmd_tur[i], list[D.tur_turn >> 2]);
    D.tur_turn++;
    if (D.tur_turn >= 4 * 8) D.tur_turn = 0;
  }
  D.gun_cool = HeavyEvery();
  (void)fury;
}

// ---- the Valkyrie ----------------------------------------------------------------------
// She flies with the ship, facing up, roaming the top of the screen. Her
// railgun is a turret on her spine: it rests facing front; when she has
// reloaded it swings round and locks on the next engine still running,
// charges and fires, then turns back to the front while she reloads.
static s32 TargetX(void) { return D.hx + FX(kNacX[D.target]); }
static s32 TargetTop(void) { return D.hy + FX(kNacY[D.target]) - FX(30); }
static s32 PivotY(void) { return D.vy + FX(VAL_PIVOT_DY); }

// The muzzle: out along the barrel from the turret's pivot.
static void MuzzleAt(s32* x, s32* y) {
  *x = D.vx + SB_MulFP(SB_Cos(D.gun_ang), FX(BARREL));
  *y = PivotY() + SB_MulFP(SB_Sin(D.gun_ang), FX(BARREL));
}

// Turn the railgun towards `want` (2^24 to the turn), at most `rate` a
// frame; 1 once it is there.
static int TurnGun(s32 want, s32 rate) {
  s32 d = (want - D.gun_ang) & (TURN - 1);
  if (d >= TURN / 2) d -= TURN;
  if (d > rate) d = rate;
  if (d < -rate) d = -rate;
  D.gun_ang = (D.gun_ang + d) & (TURN - 1);
  return d < (TURN >> 9) && d > -(TURN >> 9);
}

static s32 AimAtTarget(void) { return AngleTo(D.vx, PivotY(), TargetX(), TargetTop()); }

// Her line of fire, muzzle to the target's top: its direction (16.16 unit
// vector), its angle and its length in units.
static void FireLine(s32* mx, s32* my, s32* ux, s32* uy, s32* ang, s32* len) {
  MuzzleAt(mx, my);
  *ang = D.gun_ang;
  *ux = SB_Cos(*ang);
  *uy = SB_Sin(*ang);
  *len = Dist(TargetX() - *mx, TargetTop() - *my);
}

static int InLine(void) {
  s32 mx, my, ux, uy, ang, len;
  FireLine(&mx, &my, &ux, &uy, &ang, &len);
  const s32 rx = PlayerX() - mx, ry = PlayerY() - my;
  const s32 along = SB_MulFP(rx, ux) + SB_MulFP(ry, uy);
  const s32 perp = SB_MulFP(rx, uy) - SB_MulFP(ry, ux);
  return along > -FX(8) && along < FX(len + 10) && perp < FX(LINE_HALF) && perp > -FX(LINE_HALF);
}

// A line of generics from her muzzle to the target's top -- her aiming line
// or her beam, rotating sprites laid along it -- kept in place as both move.
// `anim` < 0 takes it down.
static void Line(u32* seg, const char* name, int anim) {
  s32 mx, my, ux, uy, ang, len;
  FireLine(&mx, &my, &ux, &uy, &ang, &len);
  for (int k = 0; k < NSEG; k++) {
    const s32 d = FX(SEG / 2 + SEG * k);
    const int want = anim >= 0 && SEG * k < len;
    const s32 x = mx + SB_MulFP(ux, d), y = my + SB_MulFP(uy, d);
    if (want && !seg[k]) seg[k] = Spawn(name, x, y);
    if (!want && seg[k]) Drop(&seg[k]);
    if (seg[k]) {
      Put(seg[k], x, y);
      SB_SetActorOrient(seg[k], ang);
      GenAnim(seg[k], anim);
    }
  }
}

// Where she roams: a slow figure of eight about the top of the screen,
// never far off the ship (the camera follows it), held steadier while she
// fires.
static void Roam(int steady) {
  if (!steady) D.roam++;
  s32 cx = PlayerX();
  if (cx < FX(150)) cx = FX(150);
  if (cx > FX(250)) cx = FX(250);
  const s32 x = cx + SB_MulFP(SB_Sin((s32)(D.roam << 14)), FX(52));
  const s32 y = FX(VAL_Y) + SB_MulFP(SB_Sin((s32)(D.roam << 15)), FX(20));
  D.vx += Ease(x - D.vx, FX(steady ? 0.3 : 1.3));
  D.vy += Ease(y - D.vy, FX(steady ? 0.3 : 1.0));
}

static void Explode(s32 x, s32 y, int big) {
  SB_SpawnExplosion(BH, big ? "ExplosionXL" : "ExplosionL", x, y);
}

static void HitEngine(void) {
  const int e = D.target;
  const s32 nx = D.hx + FX(kNacX[e]), ny = D.hy + FX(kNacY[e]);
  Explode(nx, ny, 1);
  Explode(nx - FX(14), ny + FX(12), 0);
  Explode(nx + FX(12), ny - FX(16), 0);
  SB_ShakeWorld();
  GenAnim(D.nac[e], 2);
  D.blaze[e] = Spawn("*antarctica/boss/blaze", nx, ny - FX(8));
  D.lost[e] = 1;
  D.engines--;
  D.base_y += FX(STAGGER);
  D.stun = 100;
  for (int i = 0; i < 4; i++) {
    if (D.tur[i]) Stop(D.tur[i]);
    if (D.can[i]) Stop(D.can[i]);
  }
  switch (D.engines) {
  case 3: Say(); Say(); break;                // the Shogun: "My engine...!"; the Doc: "One down"
  case 2: Say(); break;                       // the Shogun: "All guns!"
  case 1: Say(); break;                       // the Doc: "One more engine"
  }
}

// The next engine still running, in the order she takes them.
static void NextTarget(void) {
  for (int k = 0; k < 4; k++) {
    const int e = kEngineOrder[k];
    if (!D.lost[e]) { D.target = e; return; }
  }
}

static void Rail(void) {
  s32 mx, my;
  Roam(D.rail == R_FIRE);
  switch (D.rail) {
  case R_TURN:
    // reloaded: swing round onto the engine
    GenAnim(D.val, 0);
    GenAnim(D.rg, 0);
    if (TurnGun(AimAtTarget(), TURN / 220) && ++D.sub_t > 12) {
      D.sub_t = 0;
      D.rail = R_AIM;
      D.aim = 0;
      Say();                                    // Volta: "You're going down, Shogun!"
      MuzzleAt(&mx, &my);
      D.orb = Spawn("*antarctica/boss/orb", mx, my);
    }
    break;
  case R_AIM: {
    TurnGun(AimAtTarget(), TURN / 160);         // locked: it tracks the engine
    const int held = InLine() || InCutscene();
    if (!held) D.aim++;
    // seven frames of charge, a step every 25 frames; the barrel heats
    int stage = 1 + ((D.aim * 41) >> 10);
    if (stage > 7) stage = 7;
    GenAnim(D.val, stage >= 7 ? VA_CHARGED : (stage + 1) >> 1);
    GenAnim(D.rg, stage < 3 ? 1 : (stage < 6 ? 2 : 3));
    MuzzleAt(&mx, &my);
    Put(D.orb, mx, my);
    GenAnim(D.orb, stage >= 7 ? 6 : stage - 1);
    Line(D.sight, "*antarctica/boss/sight", held);
    if (D.aim >= AIM_TIME) {
      D.rail = R_FIRE;
      D.sub_t = 0;
      Line(D.sight, "*antarctica/boss/sight", -1);
      GenAnim(D.val, VA_FIRING);
      GenAnim(D.rg, 4);
      SB_ShakeWorld();
      SB_SpawnExplosion(BH, "ExplosionL", mx, my);
      D.impact = Spawn("*antarctica/boss/orb", TargetX(), TargetTop());
    }
    break;
  }
  case R_FIRE: {
    TurnGun(AimAtTarget(), TURN / 160);
    D.sub_t++;
    // the beam opens, holds pulsing on the nacelle -- which sparks and
    // shudders under it -- until it blows, then dies away
    const int a = D.sub_t < 3 ? 0 : (D.sub_t < BEAM_OPEN ? 1 : (D.sub_t < BEAM_HOLD ? 5 : 4));
    Line(D.beam, "*antarctica/boss/beam", a);
    MuzzleAt(&mx, &my);
    Put(D.orb, mx, my);
    Put(D.impact, TargetX(), TargetTop());
    GenAnim(D.impact, D.sub_t < BEAM_HOLD ? 6 : 2);
    if (D.sub_t < BEAM_HOLD) {
      GenAnim(D.nac[D.target], 1);
      if ((D.sub_t & 15) == 8)
        SB_SpawnExplosion(BH, "ExplosionXS", TargetX() + FX(SB_RangedRand(-12, 12)),
                          TargetTop() + FX(SB_RangedRand(0, 30)));
    }
    if (D.sub_t == BEAM_HOLD) {
      HitEngine();
      GenAnim(D.val, 0);
      GenAnim(D.rg, 3);                         // still glowing hot
      Drop(&D.orb);
      Drop(&D.impact);
    }
    if (D.sub_t >= BEAM_END) {
      Line(D.beam, "*antarctica/boss/beam", -1);
      if (D.engines == 0) {
        D.phase = P_DOWN;
        D.t = 0;
        R32(D.boss, B_MAXHP) = 0;
        Say();                                  // the Shogun: "your little bird falls with me!"
        return;
      }
      D.rail = R_COOL;
      D.sub_t = 0;
    }
    break;
  }
  case R_COOL:
    // back to the front, cooling, while she reloads
    TurnGun(FRONT, TURN / 260);
    GenAnim(D.rg, D.sub_t < 60 ? 2 : (D.sub_t < 120 ? 1 : 0));
    if (++D.sub_t >= COOL_TIME && Quiet()) {
      D.sub_t = 0;
      NextTarget();
      D.rail = R_TURN;
    }
    break;
  }
}

// The Kurogane shells her: a turret turns on her and fires shells timed to
// end on her shield (chase_cast.py "spark"), which flares as they strike.
static void Barrage(void) {
  if (!D.val || D.stun > 0 || Paused() || D.lull || D.hold > 0) return;
  if (D.barrage_cool > 0) { D.barrage_cool--; return; }
  int i = D.barrage_turn & 3;
  for (int k = 0; k < 4 && (!D.tur[i] || Busy(D.tur[i])); k++) i = (i + 1) & 3;
  if (!D.tur[i] || Busy(D.tur[i])) return;
  D.barrage_turn++;
  const s32 tx = D.hx + FX(kTurX[i]), ty = D.hy + FX(kTurY[i]);
  // the shells fly 3 units a frame, 6 a script step, from 14 out
  s32 travel = Dist(D.vx - tx, D.vy - ty) - SHIELD_R - 14;
  int steps = 0;
  while (travel > 0) { travel -= 6; steps++; }
  SetReg(D.tur[i], REG_AIM, AngleTo(tx, ty, D.vx, D.vy));
  SetReg(D.tur[i], REG_STEPS, FX(steps));
  SetReg(D.tur[i], REG_BUSY, FX(1));
  SetReg(D.tur[i], REG_CMD, FX(G_BARRAGE));
  D.cmd_tur[i] = G_BARRAGE;
  D.at_val[i] = 2 * steps + 70;
  D.shield_from = D.frame + 2 * steps + 4;
  D.shield_until = D.shield_from + 66;
  D.barrage_cool = D.engines <= 2 ? 170 : 260;
}

// The nearest tracked escort on the playfield within `range` of (x, y).
static u32 NearestEscort(s32 x, s32 y, s32 range, s32* ox, s32* oy) {
  u32 best = 0;
  s32 bd = range;
  for (int k = 0; k < NTRACK; k++) {
    const u32 bg = D.track[k];
    if (!bg || D.track_hp[k] <= 0) continue;
    s32 ex, ey;
    SB_GetActorPosition(Act(bg), &ex, &ey);
    if (ey < FX(0) || ey > FX(400)) continue;
    const s32 d = Dist(ex - x, ey - y);
    if (d < bd) { bd = d; best = bg; *ox = ex; *oy = ey; }
  }
  return best;
}

// Her wing guns: twin cannons out on her wingtips. Idle, they sweep the sky
// ahead, each on its own side; with an escort in reach they swing onto the
// nearest and fire bolts -- seen in flight -- that land a light hit.
static void WingGuns(void) {
  // not until she is on the screen: a bolt started off the playfield can be
  // culled by the engine while the code still holds it, and stick
  const int on = D.vy > FX(20);
  for (int s = 0; s < 2; s++) {
    if (!D.wing_vis[s]) continue;
    const s32 gx = D.vx + FX(kWingX[s]), gy = D.vy + FX(WING_DY);
    s32 tx = 0, ty = 0;
    const u32 t = NearestEscort(gx, gy, WING_RANGE, &tx, &ty);
    s32 want;
    if (t) want = AngleTo(gx, gy, tx, ty);
    else {
      // up the screen, swept out to her side and back
      const s32 sweep = SB_MulFP(SB_Sin((s32)((D.frame + s * 97) << 16)), TURN / 9);
      want = (FRONT + (s ? TURN / 14 : -(TURN / 14)) + sweep) & (TURN - 1);
    }
    s32 d = (want - D.wing_ang[s]) & (TURN - 1);
    if (d >= TURN / 2) d -= TURN;
    const s32 rate = TURN / 80;
    D.wing_ang[s] = (D.wing_ang[s] + (d > rate ? rate : (d < -rate ? -rate : d))) & (TURN - 1);
    if (D.wing_cool[s] > 0) D.wing_cool[s]--;
    if (D.wing_flash[s] > 0) D.wing_flash[s]--;
    Put(D.wing_vis[s], gx, gy);
    SB_SetActorOrient(D.wing_vis[s], D.wing_ang[s]);
    GenAnim(D.wing_vis[s], D.wing_flash[s] > 0);
    if (!on || !t || D.wing_cool[s] > 0 || d > TURN / 40 || d < -(TURN / 40)) continue;
    if (ty < FX(0) || ty > FX(400) || tx < FX(0) || tx > FX(400)) continue;
    // fire: a bolt from the muzzle to the escort
    int k = 0;
    while (k < NBOLT && D.bolt[k]) k++;
    if (k == NBOLT) continue;
    const s32 ux = SB_Cos(D.wing_ang[s]), uy = SB_Sin(D.wing_ang[s]);
    const s32 mx = gx + SB_MulFP(ux, FX(WING_MUZZLE)), my = gy + SB_MulFP(uy, FX(WING_MUZZLE));
    const s32 ang = AngleTo(mx, my, tx, ty);
    D.bolt[k] = Spawn("*antarctica/boss/bolt", mx, my);
    if (!D.bolt[k]) continue;
    SB_SetActorOrient(D.bolt[k], ang);
    D.bolt_to[k] = t;
    D.bolt_x[k] = mx;
    D.bolt_y[k] = my;
    D.bolt_dx[k] = SB_Cos(ang) * BOLT_SPEED;
    D.bolt_dy[k] = SB_Sin(ang) * BOLT_SPEED;
    s32 left = Dist(tx - mx, ty - my), n = 0;
    while (left > 0) { left -= BOLT_SPEED; n++; }
    D.bolt_left[k] = n;
    D.wing_cool[s] = WING_EVERY;
    D.wing_flash[s] = 6;
  }
}

// The bolts in flight; each lands on its escort, if it is still there.
static void Bolts(void) {
  for (int k = 0; k < NBOLT; k++) {
    if (!D.bolt[k]) continue;
    D.bolt_x[k] += D.bolt_dx[k];
    D.bolt_y[k] += D.bolt_dy[k];
    Put(D.bolt[k], D.bolt_x[k], D.bolt_y[k]);
    if (--D.bolt_left[k] > 0) continue;
    if (Alive(D.bolt_to[k])) {
      SB_StrikeBadGuy(BH, D.bolt_to[k], WING_DAMAGE);
      SB_SpawnExplosion(BH, "ExplosionXXS", D.bolt_x[k], D.bolt_y[k]);
    }
    Drop(&D.bolt[k]);
  }
}

static void PlaceValkyrie(void) {
  Put(D.val, D.vx, D.vy);
  if (!D.falling) SB_SetActorOrient(D.val, 0);  // (her sprite rotates: upright)
  Put(D.rg, D.vx, PivotY());
  if (D.rg) SB_SetActorOrient(D.rg, D.gun_ang);
  Put(D.shield, D.vx, D.vy);
  GenAnim(D.shield, D.frame >= D.shield_from && D.frame < D.shield_until);
}

static void ShootDown(void);

// ---- the Tenshu cannon ------------------------------------------------------------------
// The Kurogane's great beam. The keep's hatch opens, the cannon rises out of
// it and runs its barrel out (his other guns fire on, but no new escorts
// come); it turns on
// its target, a lock-on closing round it and a thin red warning line; a
// moment after the lock, the beam -- huge, sweeping after its target, a
// hit as hard as a bullet's every half second the ship spends in it. Then,
// as it sinks back into the keep, the parting pattern ("Wrath"), the hardest
// thing in the level to dodge. Aimed at the Valkyrie, it is the cutscene
// that brings her down (no pattern then: it sinks straight back).
static s32 MegaPivotY(void) { return D.hy + FX(KEEP_DY); }

static void MegaTarget(s32* x, s32* y) {
  if (D.mega_val && D.val) { *x = D.vx; *y = D.vy; }
  else { *x = PlayerX(); *y = PlayerY(); }
}

static int TurnMega(s32 want, s32 rate) {
  s32 d = (want - D.mega_ang) & (TURN - 1);
  if (d >= TURN / 2) d -= TURN;
  if (d > rate) d = rate;
  if (d < -rate) d = -rate;
  D.mega_ang = (D.mega_ang + d) & (TURN - 1);
  return d < (TURN >> 9) && d > -(TURN >> 9);
}

static void MegaStart(int at_val) {
  if (D.mega) return;
  D.mega = MG_RISE;
  D.mega_t = 0;
  D.mega_val = at_val;
  D.mega_ang = FRONT;
  D.hatch = Spawn("*antarctica/boss/hatch", D.hx, MegaPivotY());
  {
    // the lock-on comes onto its target as soon as the cannon starts to rise
    s32 x, y;
    MegaTarget(&x, &y);
    D.reticle = Spawn("*antarctica/boss/reticle", x, y);
    GenAnim(D.reticle, 3);
  }
  // the other guns keep firing while it is up -- except when it turns on
  // the Valkyrie, a cutscene
  if (at_val) {
    for (int i = 0; i < 4; i++) {
      if (D.tur[i]) Stop(D.tur[i]);
      if (D.can[i]) Stop(D.can[i]);
    }
    if (D.bow) Stop(D.bow);
  }
  if (!at_val && !D.said_mega) {
    D.said_mega = 1;
    Say();                                      // the Doc: "a beam cannon!"
    Say();                                      // the Shogun: "Behold the Tenshu cannon"
  }
}

// The beam, laid from the muzzle along the cannon's heading; to the target
// (the Valkyrie) or past the edge of the screen. `anim` < 0 takes it down.
static void MegaBeam(int anim) {
  const s32 ux = SB_Cos(D.mega_ang), uy = SB_Sin(D.mega_ang);
  const s32 mx = D.hx + SB_MulFP(ux, FX(MEGA_BARREL)), my = MegaPivotY() + SB_MulFP(uy, FX(MEGA_BARREL));
  int len = 460;
  if (D.mega_val && D.val) len = Dist(D.vx - mx, D.vy - my);
  for (int k = 0; k < NMEGA; k++) {
    const s32 d = FX(SEG / 2 + SEG * k);
    const int want = anim >= 0 && SEG * k < len;
    const s32 x = mx + SB_MulFP(ux, d), y = my + SB_MulFP(uy, d);
    if (want && !D.mbeam[k]) D.mbeam[k] = Spawn("*antarctica/boss/mega_beam", x, y);
    if (!want && D.mbeam[k]) Drop(&D.mbeam[k]);
    if (D.mbeam[k]) {
      Put(D.mbeam[k], x, y);
      SB_SetActorOrient(D.mbeam[k], D.mega_ang);
      GenAnim(D.mbeam[k], anim);
    }
  }
}

// The ship in the beam: a bullet's hit, then a grace.
static void MegaHits(void) {
  if (D.mega_grace > 0) { D.mega_grace--; return; }
  if (D.mega_val) return;
  const u32 sh = Shogun();
  if (*(volatile unsigned short*)(sh + SH_HITLOCK) || R32(sh, SH_HITLOCK2)) return;
  const s32 ux = SB_Cos(D.mega_ang), uy = SB_Sin(D.mega_ang);
  const s32 rx = PlayerX() - D.hx, ry = PlayerY() - MegaPivotY();
  const s32 along = SB_MulFP(rx, ux) + SB_MulFP(ry, uy);
  const s32 perp = SB_MulFP(rx, uy) - SB_MulFP(ry, ux);
  if (along < FX(MEGA_BARREL) || perp > FX(MEGA_HALF) || perp < -FX(MEGA_HALF)) return;
  SB_StrikePlayer(sh + SH_PLAYER, 0, sh + SH_HUD);
  D.mega_grace = MEGA_GRACE;
}

static void Mega(void) {
  if (!D.mega) return;
  if ((D.mega == MG_RISE || D.mega == MG_LOCK) && (D.mega_val ? InCutscene() : Paused()) && D.mega_t > 20) {
    Put(D.hatch, D.hx, MegaPivotY());
    if (D.mcannon) Put(D.mcannon, D.hx, MegaPivotY());
    if (D.reticle) {
      s32 x, y;
      MegaTarget(&x, &y);
      Put(D.reticle, x, y);
    }
    return;
  }
  D.mega_t++;
  s32 tx, ty;
  MegaTarget(&tx, &ty);
  const s32 aim = AngleTo(D.hx, MegaPivotY(), tx, ty);
  Put(D.hatch, D.hx, MegaPivotY());
  switch (D.mega) {
  case MG_RISE: {
    // the hatch opens; the cannon rises out of it; the barrel runs out
    const int t = D.mega_t;
    GenAnim(D.hatch, t < 10 ? 1 : (t < 20 ? 2 : 3));
    Put(D.reticle, tx, ty);
    GenAnim(D.reticle, 3);
    if (t == 24) {
      // the cannon's sprite carries the open hatch: the hatch's own goes, so
      // it can never be drawn over the cannon
      D.mcannon = Spawn("*antarctica/boss/megacannon", D.hx, MegaPivotY());
      Drop(&D.hatch);
    }
    GenAnim(D.mcannon, t < 36 ? 0 : (t < 48 ? 1 : (t < 60 ? 2 : (t < 70 ? 3 : (t < 80 ? 4 : 5)))));
    if (t == 60) SB_ShakeWorld();
    if (t >= 90) {
      D.mega = MG_LOCK;
      D.mega_t = 0;
      if (!D.reticle) D.reticle = Spawn("*antarctica/boss/reticle", tx, ty);
      if (D.mega_val) Say();                    // Volta: "his keep is aiming at ME?!"
    }
    break;
  }
  case MG_LOCK:
    // it turns on its target; a short lock, the warning line, then fire
    if (D.mega_val && InCutscene() && D.mega_t > 30) D.mega_t = 30;   // her line first
    TurnMega(aim, D.mega_t < MEGA_LOCK - 24 ? TURN / 90 : TURN / 240);
    Put(D.reticle, tx, ty);
    GenAnim(D.reticle, D.mega_t < MEGA_LOCK - 24 ? 3 : 2);
    GenAnim(D.mcannon, D.mega_t < MEGA_LOCK - 24 ? 5 : 6);
    MegaBeam(D.mega_t < MEGA_LOCK - 24 ? -1 : 0);
    if (D.mega_t >= MEGA_LOCK) {
      D.mega = MG_FIRE;
      D.mega_t = 0;
      Drop(&D.reticle);
      SB_ShakeWorld();
    }
    break;
  case MG_FIRE:
    // the beam sweeps after its target, slower than the ship can fly
    TurnMega(aim, D.mega_val ? TURN / 200 : MEGA_SWEEP);
    GenAnim(D.mcannon, 7);
    MegaBeam(D.mega_t < 6 ? 1 : 5);
    MegaHits();
    if ((D.mega_t & 31) == 0) SB_ShakeWorld();
    if (D.mega_val && D.val) {
      // on her shield, until it gives
      D.shield_from = D.frame;
      D.shield_until = D.frame + 4;
      if ((D.mega_t & 7) == 0)
        SB_SpawnExplosion(BH, "ExplosionXS", D.vx + FX(SB_RangedRand(-30, 30)), D.vy + FX(SB_RangedRand(10, 40)));
      if (D.mega_t == 50) {
        Drop(&D.shield);
        Explode(D.vx, D.vy + FX(20), 1);
      }
      if (D.mega_t == 80) ShootDown();
    }
    if (D.mega_t >= (D.mega_val ? 100 : MEGA_FIRE)) {
      D.mega = MG_SINK;
      D.mega_t = 0;
      MegaBeam(4);
      if (!D.mega_val && D.wrath) Order(D.wrath, G_WRATH);
    }
    break;
  case MG_SINK:
    // it sinks back into the keep -- under the parting pattern, unless it
    // has just brought the Valkyrie down
    if (D.mega_t == 10) MegaBeam(-1);
    if (D.wrath) Place(D.wrath, D.hx, MegaPivotY());
    {
      const int dur = D.mega_val ? 100 : MEGA_SINK;
      const int left = dur - D.mega_t;
      GenAnim(D.mcannon, left > 80 ? 5 : (left > 64 ? 4 : (left > 48 ? 3 : (left > 32 ? 2 : (left > 16 ? 1 : 0)))));
      if (left == 12) {
        Drop(&D.mcannon);
        D.hatch = Spawn("*antarctica/boss/hatch", D.hx, MegaPivotY());
        GenAnim(D.hatch, 3);
      }
      if (left < 12) GenAnim(D.hatch, left > 8 ? 2 : (left > 4 ? 1 : 0));
      if (left <= 0) {
        Drop(&D.hatch);
        D.mega = MG_OFF;
        D.mega_val = 0;
        D.mega_cool = D.phase == P_DOOM ? DOOM_MEGA_EVERY : MEGA_EVERY;
        D.gun_cool = 60;
      }
    }
    break;
  }
  if (D.mcannon) {
    Put(D.mcannon, D.hx, MegaPivotY());
    SB_SetActorOrient(D.mcannon, D.mega_ang);
  }
}

// ---- beams go through the shield ------------------------------------------------------
// The capsule shield (missile.c) raises the ship's god flag, so the engine
// lets every bullet by -- the bow gun's lance too, which is a line of laser
// bullets. A beam is a beam: while the shield is up, the lance is checked
// here, along its line, and strikes as a bullet would. (The Tenshu cannon,
// MegaHits, always strikes from code.)
#define LANCE_HALF  8
#define LANCE_GRACE 30
static void LanceHits(void) {
  if (D.lance_grace > 0) { D.lance_grace--; return; }
  if (!D.bow || !ShieldUp()) return;
  if ((GetReg(D.bow, 21) >> 16) != G_LANCE || (GetReg(D.bow, REG_ANIM) >> 16) != 2) return;
  const u32 sh = Shogun();
  if (*(volatile unsigned short*)(sh + SH_HITLOCK) || R32(sh, SH_HITLOCK2)) return;
  // R11 holds the lance's heading, 256 to the turn: as 16.16, 2^24 to the turn
  const s32 ang = GetReg(D.bow, 11) & (TURN - 1);
  const s32 ux = SB_Cos(ang), uy = SB_Sin(ang);
  const s32 rx = PlayerX() - D.hx, ry = PlayerY() - (D.hy + FX(BOW_DY));
  const s32 along = SB_MulFP(rx, ux) + SB_MulFP(ry, uy);
  const s32 perp = SB_MulFP(rx, uy) - SB_MulFP(ry, ux);
  if (along < FX(8) || perp > FX(LANCE_HALF) || perp < -FX(LANCE_HALF)) return;
  SB_StrikePlayer(sh + SH_PLAYER, 0, sh + SH_HUD);
  D.lance_grace = LANCE_GRACE;
}

// ---- the Valkyrie brought down -----------------------------------------------------------
// A point on her, (x, y) from her centre in her own frame, turned with her.
static void OnHer(s32 x, s32 y, s32* ox, s32* oy) {
  const s32 c = SB_Cos(D.val_rot), sn = SB_Sin(D.val_rot);
  *ox = D.vx + SB_MulFP(FX(x), c) - SB_MulFP(FX(y), sn);
  *oy = D.vy + SB_MulFP(FX(x), sn) + SB_MulFP(FX(y), c);
}

static void ShootDown(void) {
  GenAnim(D.val, VA_DOWN);
  GenAnim(D.rg, 0);
  for (int s = 0; s < 2; s++) {
    D.wing_fire[s] = Spawn("*antarctica/boss/blaze", D.vx + FX(s ? 26 : -26), D.vy + FX(4));
    Drop(&D.wing_vis[s]);
  }
  Drop(&D.shield);
  Explode(D.vx - FX(20), D.vy, 1);
  Explode(D.vx + FX(24), D.vy + FX(8), 1);
  Explode(D.vx, D.vy - FX(20), 0);
  SB_ShakeWorld();
  D.falling = 1;
  D.fall_t = 0;
  D.val_rot = 0;
  D.fall_side = D.vx < FX(200) ? -1 : 1;
  Say();                                        // Volta: "Mayday, mayday!"
}

static void Puff(s32 x, s32 y) {
  const u32 a = GenericActor(SB_SpawnGeneric(BH, "*antarctica/boss/smoke", x, y));
  if (a) SB_SetActorPosition(a, x, y);
}

// Volta's pod: blown out of the cockpit, its rocket carrying it up and
// away from her ship; then its chute opens and it drifts down the screen.
static void Pod(void) {
  if (!D.pod) return;
  D.pod_t++;
  if (D.pod_t == 40) {
    GenAnim(D.pod, 1);                          // the chute
    D.pod_dy = FX(0.5);
    D.pod_dx = -D.fall_side * FX(0.25);
  }
  if (D.pod_t < 40) D.pod_dy += FX(0.09);       // the rocket burning out
  const s32 sway = D.pod_t >= 40 ? SB_MulFP(SB_Sin((s32)(D.pod_t << 18)), FX(0.35)) : 0;
  D.pod_x += D.pod_dx + sway;
  D.pod_y += D.pod_dy;
  Put(D.pod, D.pod_x, D.pod_y);
  if (D.pod_y > FX(470) || D.pod_x < FX(-60) || D.pod_x > FX(460)) Drop(&D.pod);
}

// Out of control: she tilts and wobbles, then rolls faster and faster as she
// loses height, both wings burning, a trail of black smoke behind her and
// pieces breaking off; Volta ejects; she goes off the side of the screen,
// and down.
static void Fall(void) {
  D.fall_t++;
  const s32 lurch = SB_MulFP(SB_Sin((s32)(D.fall_t << 19)), FX(2.2));
  const int late = D.fall_t > FALL_HOLD ? D.fall_t - FALL_HOLD : 0;
  D.vx += D.fall_side * (FX(0.12) + (late << 10)) + lurch;
  D.vy += FX(0.2) + SB_MulFP(SB_Cos((s32)(D.fall_t << 18)), FX(1.2));
  // the tilt: a heavy wobble towards her side at first, then a roll that
  // speeds up as she goes
  if (!late) D.val_rot = D.fall_side * (TURN / 36 + SB_MulFP(SB_Sin((s32)(D.fall_t << 18)), TURN / 14));
  else {
    s32 spin = (s32)late * (TURN / 4000);
    if (spin > TURN / 30) spin = TURN / 30;
    D.val_rot += D.fall_side * spin;
  }
  D.val_rot &= TURN - 1;
  if (D.val) SB_SetActorOrient(D.val, D.val_rot);
  D.gun_ang = (D.gun_ang + (TURN >> 7)) & (TURN - 1);      // the turret spinning loose
  s32 x, y;
  for (int s = 0; s < 2; s++) {
    OnHer(s ? 26 : -26, 4, &x, &y);
    Put(D.wing_fire[s], x, y);
  }
  // smoke: from each wing in turn, and her engines
  if ((D.fall_t % 3) == 0) {
    OnHer((D.fall_t & 1) ? 30 : -30, 10, &x, &y);
    Puff(x, y);
  }
  if ((D.fall_t % 5) == 0) {
    OnHer(SB_RangedRand(-16, 16), 50, &x, &y);
    Puff(x, y);
  }
  // pieces breaking off
  if ((D.fall_t & 7) == 0) {
    OnHer(SB_RangedRand(-40, 40), SB_RangedRand(-30, 40), &x, &y);
    SB_SpawnExplosion(BH, (D.fall_t & 31) == 0 ? "ExplosionL" : "ExplosionXS", x, y);
  }
  if (D.fall_t == EJECT_T) {
    // the canopy blows and Volta ejects
    OnHer(0, -34, &x, &y);
    SB_SpawnExplosion(BH, "ExplosionXS", x, y);
    D.pod = Spawn("*antarctica/boss/pod", x, y);
    GenAnim(D.pod, 0);
    D.pod_x = x;
    D.pod_y = y;
    D.pod_dx = -D.fall_side * FX(1.4);
    D.pod_dy = -FX(3.4);
    D.pod_t = 0;
  }
  if (D.vx < FX(-80) || D.vx > FX(480)) {
    // the crash, just off the edge: a fireball, and its smoke
    const s32 ex = D.vx < 0 ? FX(-10) : FX(410);
    Explode(ex, D.vy, 1);
    Explode(D.vx < 0 ? FX(10) : FX(390), D.vy + FX(30), 1);
    Explode(ex, D.vy - FX(30), 1);
    for (int k = 0; k < 4; k++) Puff(D.vx < 0 ? FX(SB_RangedRand(0, 24)) : FX(SB_RangedRand(376, 400)),
                                     D.vy + FX(SB_RangedRand(-30, 30)));
    SB_ShakeWorld();
    Drop(&D.val);
    Drop(&D.rg);
    for (int s = 0; s < 2; s++) Drop(&D.wing_fire[s]);
    D.falling = 0;
    D.crashed = 1;
    Say();                                      // the Doc: "Commander! ... Valkyrie is down"
  }
}

// ---- Mission 8: the Kurogane answers ShogunGPT's call ---------------------------------
// If the pilot cannot bring ShogunGPT down before its distress call goes
// through (missile/shogungpt.c), the Shogun comes -- the dreadnought, half
// repaired, rises from the bottom of the screen and turns everything it has
// on the ship: every gun at once, the escorts and the Tenshu cannon again
// and again. It never ends; nothing can hurt it. The same
// pieces as the chase (antarctica/ assets, registered by edinburgh.c), run
// by the same code, from ShogunGPT's update.
void DoomStart(void) {
  SB_ClearBytes(&D, sizeof D);
  D.phase = P_DOOM;
  D.hx = FX(200);
  D.base_y = FX(HULL_Y);
  D.hy = FX(HULL_Y + 170);
  D.said_mega = 1;                // its lines are the chase's: not here
  D.mega_cool = 360;
  D.gun_cool = 120;
  D.wave_cool = 200;
  D.engines = 4;
  Build();
  SB_ShakeWorld();
}

// Mission 8's secret: survive the doom long enough and the Doc, with part of
// ShogunGPT's data, has codes that jam the Kurogane -- once (shogungpt.c
// G_JAM). Every gun stops, the Tenshu cannon goes back into its keep, and the
// dreadnought sinks out of the bottom of the screen, sparking.
void DoomJam(void) {
  if (D.jammed) return;
  D.jammed = 1;
  for (int i = 0; i < 4; i++) {
    if (Alive(D.tur[i])) Stop(D.tur[i]);
    if (Alive(D.can[i])) Stop(D.can[i]);
  }
  if (Alive(D.bow)) Stop(D.bow);
  if (Alive(D.wrath)) Stop(D.wrath);
  if (D.mega && D.mega != MG_SINK) {
    D.mega = MG_SINK;
    D.mega_t = 0;
  }
  SB_ShakeWorld();
}

// Out of sight below (shogungpt.c ends the level then).
int DoomGone(void) { return D.jammed && D.hy > FX(HULL_Y + 260); }

void DoomUpdate(void) {
  D.frame++;
  D.t++;
  D.quiet = InCutscene() ? 0 : D.quiet + 1;
  if (g_autopilot_target && !Alive(g_autopilot_target)) g_autopilot_target = 0;
  if (D.t == 90) SB_ShakeWorld();
  Chase();
  if (D.jammed) {
    Mega();                                 // the cannon, if it was up, goes home
    if (!D.mega) D.base_y += FX(0.5);       // then the hull sinks away
    if ((D.t & 7) == 0)
      SB_SpawnExplosion(BH, "ExplosionXS", D.hx + FX(SB_RangedRand(-70, 70)), D.hy + FX(SB_RangedRand(-30, 50)));
    if ((D.t & 63) == 0) SB_ShakeWorld();
    PlaceDreadnought();
    return;
  }
  const int risen = D.hy - D.base_y < FX(6);
  if (risen) {
    if (D.mega_cool > 0) D.mega_cool--;
    if (!D.mega) {
      Guns();
      Waves();
    }
    if (!D.mega && D.mega_cool == 0 && Quiet()) MegaStart(0);
    Mega();
    LanceHits();
  }
  PlaceDreadnought();
  if (risen) Clamp();
}

// ---- the boss callbacks -----------------------------------------------------------------
static s32 ChHealth(u32 boss) {
  (void)boss;
  if (D.phase == P_ROUND) {
    const s32 p = D.progress >> 4;
    return p > D.round_max ? D.round_max : p;
  }
  if (D.phase == P_ARRIVE || D.phase == P_RAIL) return D.engines * 1000;
  if (D.phase == P_LAST) return D.last_left;
  return 0;
}

static void ChRender(u32 boss, s32 x, s32 y) { (void)boss; (void)x; (void)y; }

static void ChSpawn(u32 boss) {
  SB_ClearBytes(&D, sizeof D);
  D.boss = boss;
  D.phase = P_HAIL;
  D.hx = FX(200);
  D.base_y = FX(HULL_Y);
  D.hy = FX(HULL_Y + 170);
  D.pos = ROUTE_START;
  D.speed_want = FX(2.2);
  D.wave_cool = 60;
  D.barrage_cool = 200;
  R32(boss, B_STATE) = 1;
  SB_PausePartition(BH, 1);
  g_autopilot_target = 0;
  // the Doc: "Seoul is free... come home"; "...something huge behind you"
  Say();
  Say();
}

static void StartRound(u32 boss, int round) {
  D.phase = P_ROUND;
  D.round = round;
  D.t = 0;
  D.progress = 0;
  D.round_max = round == 1 ? ROUND1_MAX : ROUND2_MAX;
  D.mega_done = 0;
  R32(boss, B_MAXHP) = D.round_max;             // the bar: the chance to break away
}

static void BlowGuns(void) {
  // the guns blow one by one -- the turrets left as wrecks, the cannons torn
  // away -- every 32 frames of the escape
  if (D.t >= 300 || (D.t & 31)) return;
  const int i = D.t >> 5;
  if (i < 4 && D.tur[i]) {
    SB_SetBadGuyHealth(D.tur[i], 1);
    Remove(D.tur[i]);
    D.tur[i] = 0;
    GenAnim(D.tur_vis[i], 3);
  } else if (i >= 4 && i < 8 && D.can[i - 4]) {
    const int c = i - 4;
    SB_SetBadGuyHealth(D.can[c], 1);
    Remove(D.can[c]);
    D.can[c] = 0;
    Drop(&D.can_vis[c]);
  } else if (i == 8 && D.bow) {
    SB_SetBadGuyHealth(D.bow, 1);
    Remove(D.bow);
    D.bow = 0;
  } else if (i == 9 && D.wrath) {
    SB_SetBadGuyHealth(D.wrath, 1);
    Remove(D.wrath);
    D.wrath = 0;
  }
}

// Breaking away, the ship races off and leaves the escorts behind: they
// drop down the screen and are gone off the bottom. (Left hovering at the
// top they sat under the Doc's dialog box ("Now! Over the ridge"), hidden,
// and all that showed as the ship's shots struck them was the engine's white
// hit flash -- a white shape flickering at the top, then gone.)
#define LEFT_BEHIND   FX(5)     // a frame, down the screen
#define BEHIND_Y      FX(560)   // ... past the bottom: gone
#define ABOVE_Y       FX(-24)   // above the top: not yet seen
static int OwnGun(u32 bg) {
  for (int i = 0; i < 4; i++) if (bg == D.tur[i] || bg == D.can[i]) return 1;
  return bg == D.bow || bg == D.wrath;
}

static void LeaveBehind(void) {
  // every ship but his own guns -- escorts other ships launched too, which
  // were never tracked
  u32 next;
  for (u32 bg = SB_GetFirstBadGuy(BH); bg; bg = next) {
    next = SB_GetNextBadGuy(BH, bg);
    if (OwnGun(bg) || SB_GetBadGuyHealth(bg) <= 0) continue;
    s32 x, y;
    SB_GetActorPosition(Act(bg), &x, &y);
    // not yet on the screen: never seen (pushed down it would come in at
    // the top and cross the whole screen)
    if (y < ABOVE_Y) { Remove(bg); continue; }
    y += LEFT_BEHIND;
    if (y > BEHIND_Y) Remove(bg);
    else Place(bg, x, y);
  }
}

static void Escape(u32 boss) {
  R32(boss, B_MAXHP) = 0;
  D.speed_want = FX(4.5);
  D.base_y += FX(0.45);
  D.hy += Ease(D.base_y - D.hy, FX(1.0));
  BlowGuns();
  if ((D.t & 15) == 0 && D.hy < FX(470))
    Explode(D.hx + FX(SB_RangedRand(-100, 100)), D.hy + FX(SB_RangedRand(-60, 30)), (D.t & 63) == 0);
  if (D.hy > FX(600) && Quiet()) {
    D.phase = P_GONE;
    D.t = 0;
  }
}

// The pursuit's escorts, launches, guns -- and its cannon.
static void Fight(void) {
  Chase();
  if (D.mega_cool > 0) D.mega_cool--;
  // his guns fire on while the Tenshu cannon is up (as Caps() allows); no
  // new escorts come then, nor once his engines are gone
  Guns();
  if (!D.mega && D.engines > 0) Waves();
  Mega();
  LanceHits();
  BossWards();
}

static void ChUpdate(u32 boss) {
  D.frame++;
  D.t++;
  SpeechPump();
  Scroll();
  CountDamage();
  D.quiet = InCutscene() ? 0 : D.quiet + 1;
  D.calm = AllQuiet() ? D.calm + 1 : 0;
  ShipShield(Paused() || D.phase == P_HAIL || D.phase == P_RISE || D.phase == P_DOWN);
  if (g_autopilot_target && !Alive(g_autopilot_target)) g_autopilot_target = 0;
  switch (D.phase) {
  case P_HAIL:
    if (D.t > 60 && Quiet()) {
      D.phase = P_RISE;
      D.t = 0;
      Build();
      SB_ShakeWorld();
      D.speed_want = FX(3.0);
    }
    break;
  case P_RISE:
    Chase();
    if (D.t == 90) SB_ShakeWorld();
    if (D.hy - D.base_y < FX(4)) {
      // the Shogun; the Doc: an emergency; divert; thin out his escorts
      for (int k = 0; k < 4; k++) Say();
      StartRound(boss, 1);
    }
    break;
  case P_ROUND: {
    Fight();
    if (!InCutscene()) D.progress += TIME_RATE;
    // keep each region for its round
    const s32 end = D.round == 1 ? ROUTE_MOUNT : ROUTE_ANT;
    D.speed_want = D.pos > end - FX(30) ? FX(1.3) : FX(2.4);
    // the cannon once a round, at about half the bar (and again later on)
    if (!D.mega && Quiet() && D.mega_cool == 0 &&
        (D.progress > (D.round_max << 3) || (D.mega_done && D.progress > D.round_max * 13))) {
      MegaStart(0);
      D.mega_done++;
    }
    // (the first round never ends before the cannon has risen: its lines
    // come before the breakaway's)
    if (D.progress >= (D.round_max << 4) && !D.mega && Quiet() && (D.round != 1 || D.mega_done)) {
      // the chance: he falls behind
      D.phase = P_BREAK;
      D.t = 0;
      R32(boss, B_MAXHP) = 0;
      for (int i = 0; i < 4; i++) {
        if (D.tur[i]) Stop(D.tur[i]);
        if (D.can[i]) Stop(D.can[i]);
      }
      Say();                                    // the Doc: "you're pulling away!" / "over the ridge!"
    }
    break;
  }
  case P_BREAK:
    // pulling away: the scroll races, the Kurogane drops back off the bottom
    Chase();
    D.speed_want = FX(5.0);
    D.base_y += Ease(FX(HULL_Y + 160) - D.base_y, FX(0.8));
    LeaveBehind();
    if (D.t > BREAK_TIME && Quiet()) {
      D.phase = P_CATCH;
      D.t = 0;
      for (int i = 0; i < 4; i++) GenAnim(D.nac[i], 3);
      SB_ShakeWorld();
    }
    break;
  case P_CATCH:
    // ... and it catches up, afterburners blazing
    Chase();
    D.speed_want = FX(3.2);
    D.base_y += Ease(FX(HULL_Y) - D.base_y, FX(2.6));
    if ((D.t & 31) == 0) SB_ShakeWorld();
    if (D.base_y - FX(HULL_Y) < FX(2) && D.hy - D.base_y < FX(6)) {
      for (int i = 0; i < 4; i++) GenAnim(D.nac[i], 0);
      if (D.round == 1) {
        Say();                                  // the Shogun: "outrun the Kurogane?"
        StartRound(boss, 2);
      } else {
        Say();                                  // the Shogun: "I will always catch you"
        Say();                                  // the Doc: "launch the Valkyrie. Now!"
        D.phase = P_CALL;
        D.t = 0;
      }
    }
    break;
  case P_CALL:
    Fight();
    D.speed_want = FX(2.6);
    if (D.t > CALL_WAIT && Quiet() && !D.mega) {
      D.phase = P_ARRIVE;
      D.t = 0;
      D.lull = 0;                               // no waves while she arrives
      D.hold = 0;
      D.vx = FX(200);
      D.vy = FX(-140);
      D.shield = Spawn("*antarctica/boss/shield", D.vx, D.vy);
      D.val = Spawn("*antarctica/boss/valkyrie", D.vx, D.vy);
      D.rg = Spawn("*antarctica/boss/railgun", D.vx, D.vy + FX(VAL_PIVOT_DY));
      D.gun_ang = FRONT;
      for (int s = 0; s < 2; s++) {
        D.wing_vis[s] = Spawn("*antarctica/boss/wing_vis", D.vx + FX(kWingX[s]), D.vy + FX(WING_DY));
        D.wing_ang[s] = FRONT;
        D.wing_cool[s] = 30 + 11 * s;
      }
      R32(boss, B_MAXHP) = 4000;
    }
    break;
  case P_ARRIVE:
    Chase();
    Guns();
    BossWards();
    WingGuns();
    D.vy += Ease(FX(VAL_Y) - D.vy, FX(2.2));
    if (D.vy > FX(VAL_Y - 2)) {
      D.phase = P_RAIL;
      D.rail = R_TURN;
      D.sub_t = 0;
      D.target = kEngineOrder[0];
      // Volta: "Valkyrie on station!"; "leave his engines to me"; the Doc
      for (int k = 0; k < 3; k++) Say();
      D.speed_want = FX(3.0);
    }
    break;
  case P_RAIL:
    Chase();
    Guns();
    Barrage();
    Waves();
    BossWards();
    WingGuns();
    Rail();
    if (D.phase == P_RAIL && D.speed_want < FX(2.6)) D.speed_want = FX(3.0);
    break;
  case P_DOWN:
    // the cutscene: the cannon turns on the Valkyrie and brings her down
    Chase();
    D.speed_want = FX(2.4);
    if (D.t == 60) MegaStart(1);
    if (D.val && !D.falling) {
      Roam(1);
      TurnGun(FRONT, TURN / 260);
        WingGuns();
    }
    Mega();
    if (D.falling) Fall();
    if (D.crashed && !D.mega && Quiet()) {
      D.phase = P_LAST;
      D.t = 0;
      D.last_left = LAST_TIME;
      R32(boss, B_MAXHP) = LAST_TIME;          // the bar: how long to hold on
      D.mega_cool = 600;
      D.gun_cool = 30;
      // nothing left over may hold his guns: a wave that was waiting for a
      // lull when the engines went (no more waves now to clear it)
      D.lull = 0;
      D.hold = 0;
      D.stun = 0;
      D.fill_cool = 0;
      Say();                                    // the Shogun: "If I cannot have the sky..."
    }
    break;
  case P_LAST:
    // the last stand: everything he has, against the clock
    Fight();
    D.speed_want = FX(3.0);
    if (D.last_left > 0) D.last_left--;
    if (!D.mega && Quiet() && D.mega_cool == 0 && D.last_left > 300) MegaStart(0);
    if (D.last_left == 0 && !D.mega) {
      D.phase = P_ESCAPE;
      D.t = 0;
      // the Shogun: "This is not over"; Volta: "Ejected!"; the Doc: "You did it!"
      Say();
      Say();
      Say();
    }
    break;
  case P_ESCAPE:
    Escape(boss);
    break;
  case P_GONE:
    if (D.t > 90) {
      R32(boss, B_STATE) = 0;                  // the engine calls death next
      return;
    }
    break;
  }
  if (D.hull && D.phase >= P_RISE) {
    PlaceDreadnought();
    if (D.phase != P_ESCAPE && D.phase != P_GONE && D.phase != P_BREAK) Clamp();
  }
  if (D.val) PlaceValkyrie();
  Bolts();
  Pod();
}

static void ChDeath(u32 boss) {
  (void)boss;
  ShipShield(0);
  Drop(&D.glow);
  Drop(&D.aft);
  for (int s = 0; s < 2; s++) Drop(&D.streamer[s]);
  Drop(&D.hull);
  Drop(&D.oni);
  Drop(&D.hangar);
  Drop(&D.hatch);
  Drop(&D.mcannon);
  Drop(&D.reticle);
  Drop(&D.val);
  Drop(&D.rg);
  Drop(&D.shield);
  Drop(&D.orb);
  Drop(&D.impact);
  for (int i = 0; i < 4; i++) {
    Drop(&D.nac[i]);
    Drop(&D.blaze[i]);
    Drop(&D.tur_vis[i]);
    Drop(&D.can_vis[i]);
  }
  for (int s = 0; s < 2; s++) {
    Drop(&D.wing_vis[s]);
    Drop(&D.wing_fire[s]);
  }
  for (int k = 0; k < NBOLT; k++) Drop(&D.bolt[k]);
  Drop(&D.pod);
  for (int k = 0; k < NSEG; k++) {
    Drop(&D.sight[k]);
    Drop(&D.beam[k]);
  }
  for (int k = 0; k < NMEGA; k++) Drop(&D.mbeam[k]);
  g_autopilot_target = 0;
  SB_PausePartition(BH, 0);
}

void ChaseBossInit(u32 boss) {
  SB_ClearBytes((void*)boss, BOSS_SIZE);
  SB_ClearBytes(&D, sizeof D);
  SpeechReset();                        // a new run of the level
  R32(boss, B_SPAWN + 0x00) = (s32)(u32)ChSpawn;
  R32(boss, B_SPAWN + 0x04) = (s32)(u32)ChUpdate;
  R32(boss, B_SPAWN + 0x08) = (s32)(u32)ChRender;
  R32(boss, B_SPAWN + 0x0c) = (s32)(u32)ChRender;
  R32(boss, B_SPAWN + 0x10) = (s32)(u32)ChDeath;
  R32(boss, B_HEALTH) = (s32)(u32)ChHealth;
}
