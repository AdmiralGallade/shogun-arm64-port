// abyss_boss.c -- Mission 11's two bosses: the Kurozame and the Leviathan.
//
// The Kurozame (the mid-boss): Warden Grimwald's hunter-killer submarine, a
// black hull in brass-bound armour that only the coilgun's slug pierces (or
// its own parried shots). It slides across the top of the screen firing
// fans of torpedoes, clusters of bubbles, sonar rings with a gap round the
// ship -- and, every third round, a sonar roar with none: it charges (its
// bow lights up, the Doc warns the first time) and the wave goes out, and
// only the Spin Drive gets the pilot through it. Its death sets up the
// Leviathan (worlds.c's way: each boss installs the next).
//
// The Leviathan (the final boss): Grimwald's mechanical anglerfish, waiting
// at the Forge. Its head is armour all over (shots spark off it). Phase one:
// its lure -- a glowing bulb on a jointed stalk swaying in front of its
// jaws, glazed so only the laser burns it -- flashes rings and aimed
// triplets; two torpedo pods on its flanks (coilgun only) launch torpedoes;
// the head drops curtains of spines and spins spirals, and roars. Phase two,
// the lure burnt out: the jaw opens on the furnace in its throat (anything
// hurts it), and between attacks it breathes in, dragging the ship toward
// its mouth, and roars. Below 40% it is faster about all of it.
//
// Both are built from badguys (abyss/abyss_cast.py: one attack dispatcher,
// R20 the command, R19 busy, R13 the aim), placed every frame; the boss
// record's callbacks are set here (docs/NEW_LEVEL.md section 8).
//
// Same rules as missile.c: no libc, no division, engine reached by address.

#include "abyss.h"
#include "glow.h"
#include "abyss_glow.h"

// the attacks (abyss_cast.CMDS)
enum { A_TORPS = 1, A_BUBBLES, A_PING, A_ROAR, A_SPREAD, A_TEETH, A_SPIRAL, A_SPIT, A_FLASH, A_TRIPLE, A_PODTORP,
       A_STREAM,
       // the revamp (2026-10-12), after the original Missions 2-4 bosses
       // (docs/BOSS_ATTACKS_M2_M4.md, docs/BOSS_REVAMP_M8_M11.md)
       A_WIPE, A_SNAKE, A_CHARGES, A_GAP, A_COMMA, A_CURTAIN, A_OSC, A_TWIST, A_INHALE,
       // hard mode's (beach/hardkit.py boss_attacks, docs/HARD_MODE.md)
       A_XSNIPE, A_XPINCER, A_XTWIST, A_XSNAKE, A_XGAP };

// How both bosses are timed now (the originals' rules): the fight is a
// sequence of WINDOWS. A window pairs one layer that lays lanes (wipers, a
// snake, curtains, a spiral) with one that makes the ship move (aimed
// streams, mines that stop and snipe, commas); a beat of quiet follows each;
// a roar is a window of its own, nothing else firing, so the spin it asks
// for can be read. Windows count only while nobody talks.

static s32 Hp(u32 bg) { return Alive(bg) ? SB_GetBadGuyHealth(bg) : 0; }

static s32 AimAt(u32 bg) {
  s32 x, y;
  SB_GetActorPosition(Act(bg), &x, &y);
  return AbAngle(x, y, PlayerX(), PlayerY());
}

static void Attack(u32 bg, int cmd) {
  if (!Alive(bg)) return;
  OrderAt(bg, cmd, AimAt(bg));
}

// A wave of blasts over a dying boss: n of them, one every few frames.
static struct { s32 x, y, w, h; int left, cool; } BW;

static void Blasts(s32 x, s32 y, s32 w, s32 h, int n) {
  BW.x = x; BW.y = y; BW.w = w; BW.h = h;
  BW.left = n;
  BW.cool = 0;
}

static void BlastTick(void) {
  if (BW.left <= 0 || --BW.cool > 0) return;
  BW.cool = Rnd(4, 9);
  BW.left--;
  AbBoom(BW.x + FX(Rnd(-BW.w, BW.w)), BW.y + FX(Rnd(-BW.h, BW.h)), BW.left == 0);
}

// ===================================================================================
// The Kurozame
// ===================================================================================
enum { K_INTRO = 1, K_FIGHT, K_DEAD, K_OVER };
// its windows (KWindow); the dive and the surfacing move it between the top
// of the screen and the bottom
enum { KW_SNAKE = 1, KW_MINES, KW_TORPS, KW_ROAR, KW_DIVE, KW_SURFACE, KW_XPIN, KW_XTWIST, KW_XGAP };
#define K_REST_Y   FX(108)
#define K_LOW_Y    FX(392)        // surfaced below: its nose up the screen, the ship above it. (A badguy
                                  // whose centre is under the bottom edge can't fire: 430 was silent.)
#define K_DEEP_Y   FX(570)        // out of sight under the bottom edge
#define K_GONE_Y   FX(-175)       // out of sight over the top
#define K_CHARGE_T 90
#define K_HP       12000         // long enough for its windows to play out (abyss_cast.py "hunter")
#define K_BEAT     60             // the quiet after a window (40 once badly hurt)
// its moving parts, from the hull's centre facing down (abyss/art_deep.py HUNTER_*)
#define SCREW_Y    (-95)
#define ARRAY_Y    80
static const signed char kGunY[2] = { -60, 38 };
#define GUN_TURN   0x28000        // a turret's turn a frame (2^24 to the turn): ~3.5 degrees
#define YAW_MAX    0x50000        // ~7 degrees either way, swinging its nose into its slide

// The plans. At the top: snake and wipers, depth charges and streams, torpedoes
// and drifting gap rings, the roar. Past 55% it starts diving: it leaves over
// the top and surfaces below the ship (Volcano's fortress passing the ship),
// fights from there nose-up, and goes back.
static const unsigned char kPlanTop[] = { KW_SNAKE, KW_ROAR, KW_MINES, KW_TORPS };
static const unsigned char kPlanDive[] = { KW_DIVE, KW_MINES, KW_SNAKE, KW_ROAR, KW_SURFACE, KW_TORPS, KW_SNAKE,
                                           KW_MINES, KW_ROAR };
// Hard mode (docs/HARD_MODE.md): windows of its own woven in -- its turrets
// close pincers while its bow lets go snipe rings; twist rings under the
// turrets' streams; drifting-gap rings through the wipers -- the beats shorter.
static const unsigned char kPlanTopH[] = { KW_SNAKE, KW_XPIN, KW_ROAR, KW_MINES, KW_XTWIST, KW_TORPS, KW_XGAP };
static const unsigned char kPlanDiveH[] = { KW_DIVE, KW_MINES, KW_XPIN, KW_SNAKE, KW_ROAR, KW_SURFACE, KW_XTWIST,
                                            KW_TORPS, KW_SNAKE, KW_XGAP, KW_MINES, KW_ROAR };

static struct {
  u32 boss, body;
  u32 gun[2], gvis[2], screw, ping;
  int phase, t, round, roars, anim_t;
  int gflash[2], bubble;
  int win, wt, beat, step, diving, dove, roared, low, gnext;
  s32 x, y, px, yaw, gang[2], hx, vy;
} KZ;

static s32 KHealth(u32 boss) { (void)boss; return KZ.phase >= K_FIGHT && KZ.phase < K_DEAD ? Hp(KZ.body) : 0; }
static void KRender(u32 boss, s32 x, s32 y) { (void)boss; (void)x; (void)y; }

// A point on the hull (dx, dy from its centre, facing down), as it yaws.
static void KAt(s32 dx, s32 dy, s32* x, s32* y) {
  const s32 a = (KZ.yaw - DOWN) & (TURN - 1);
  const s32 c = SB_Cos(a), sn = SB_Sin(a);
  *x = KZ.x + dx * c - dy * sn;
  *y = KZ.y + dx * sn + dy * c;
}

static void KParts(void) {
  s32 x, y;
  if (Alive(KZ.body)) SB_SetActorOrient(Act(KZ.body), KZ.yaw);
  KAt(0, SCREW_Y, &x, &y);
  PlacePart(KZ.screw, x, y);
  if (++KZ.bubble >= 10) {                        // its wake: bubbles off the screw
    KZ.bubble = 0;
    SB_SpawnGeneric(BH, "*abyss/boss/bubbles", x + FX(Rnd(-8, 8)), y - FX(6));
  }
  KAt(0, ARRAY_Y, &x, &y);
  PlacePart(KZ.ping, x, y);
  for (int k = 0; k < 2; k++) {
    KAt(0, kGunY[k], &x, &y);
    if (Alive(KZ.gun[k])) Place(KZ.gun[k], x, y);
    PlacePart(KZ.gvis[k], x, y);
    if (KZ.gvis[k]) SB_SetActorOrient(KZ.gvis[k], KZ.gang[k]);
  }
}

static void KSpawn(u32 boss) {
  KZ.boss = boss;
  KZ.x = KZ.px = FX(200);
  KZ.y = FX(-140);
  KZ.yaw = DOWN;
  KZ.body = SB_SpawnBadGuy(BH, "*abyss/boss/hunter", KZ.x, KZ.y);
  for (int k = 0; k < 2; k++) {
    KZ.gun[k] = SB_SpawnBadGuy(BH, "*abyss/boss/hunter_gun", KZ.x, KZ.y);
    Guard(KZ.gun[k], 1);
    KZ.gang[k] = DOWN;
  }
  // spawned after the hull: drawn over it
  KZ.screw = SpawnPart("*abyss/boss/hunter_screw", KZ.x, KZ.y);
  KZ.ping = SpawnPart("*abyss/boss/hunter_ping", KZ.x, KZ.y);
  for (int k = 0; k < 2; k++) KZ.gvis[k] = SpawnPart("*abyss/boss/hunter_turret", KZ.x, KZ.y);
  Place(KZ.body, KZ.x, KZ.y);
  Guard(KZ.body, 1);
  KParts();
  KZ.phase = K_INTRO;
  KZ.t = 0;
  KZ.beat = 50;
  R32(boss, B_STATE) = 1;
  SB_PausePartition(BH, 1);
  R32(boss, B_MAXHP) = Hp(KZ.body);
  g_autopilot_target = KZ.body;
  AbLine(L_HUNTER_IN);
  AbLine(L_HUNTER_DOC);
}

// The deck turrets: each turns after the ship at its own pace; they fire only
// when a window tells them to. Shot out (coilgun), a turret is a wreck.
static int KGunUp(int k) { return KZ.gun[k] && Alive(KZ.gun[k]); }

static void KGuns(int guard) {
  for (int k = 0; k < 2; k++) {
    if (!KZ.gvis[k]) continue;
    if (KZ.gun[k] && !Alive(KZ.gun[k])) {
      s32 x, y;
      KAt(0, kGunY[k], &x, &y);
      AbBoom(x, y, 0);
      KZ.gun[k] = 0;
      PartFrame(KZ.gvis[k], 2);
    }
    if (!KZ.gun[k]) continue;
    Guard(KZ.gun[k], guard);
    s32 x, y;
    KAt(0, kGunY[k], &x, &y);
    KZ.gang[k] = AbTurnTo(KZ.gang[k], AbAngle(x, y, PlayerX(), PlayerY()), GUN_TURN);
    if (Busy(KZ.gun[k])) { PartFrame(KZ.gvis[k], 1); KZ.gflash[k] = 12; }
    else if (KZ.gflash[k] > 0 && --KZ.gflash[k] == 0) PartFrame(KZ.gvis[k], 0);
  }
}

// a turret, the next one that is still up (they take turns)
static void KGunOrder(int cmd, s32 aim, int use_track) {
  for (int n = 0; n < 2; n++) {
    const int k = (KZ.gnext + n) & 1;
    if (!KGunUp(k) || Busy(KZ.gun[k])) continue;
    if (KZ.low && k == 0) continue;              // below, its stern turret is under the bottom edge
    OrderAt(KZ.gun[k], cmd, use_track ? KZ.gang[k] : aim);
    KZ.gnext = k ^ 1;
    return;
  }
}

static int KQuietParts(void) {
  if (Busy(KZ.body)) return 0;
  for (int k = 0; k < 2; k++) if (KGunUp(k) && Busy(KZ.gun[k])) return 0;
  return 1;
}

static s32 KFwd(void) { return KZ.low ? UP : DOWN; }

// One window: the moment `t` of it. Returns 1 when it is over.
static int KWindow(int w, int t) {
  const s32 fwd = KFwd();
  const int hurt = Hp(KZ.body) * 2 < K_HP;
  switch (w) {
  case KW_SNAKE:
    // the snake from the bow, and the wipers sweeping their arcs in turn
    if (t == 0) OrderAt(KZ.body, A_SNAKE, fwd);
    if (t == 40 || t == 200) KGunOrder(A_WIPE, fwd, 0);
    if (t == 120 && !KGunUp(0) && !KGunUp(1)) OrderAt(KZ.body, A_PING, AimAt(KZ.body));   // no turrets: rings instead
    return t > 300 && KQuietParts();
  case KW_MINES:
    // depth charges either side; the turrets' aimed streams make the ship move
    // through what the charges leave hanging
    if (t == 0) { OrderAt(KZ.body, A_CHARGES, fwd); Anim(KZ.body, 1); KZ.anim_t = 40; }
    if (t == 70 || t == 150 || (hurt && t == 230)) KGunOrder(A_STREAM, 0, 1);
    return t > 290 && KQuietParts();
  case KW_TORPS:
    // a fan of torpedoes, then sonar rings whose gap drifts round
    if (t == 0) { Attack(KZ.body, A_TORPS); Anim(KZ.body, 1); KZ.anim_t = 40; }
    if (t == 100) Attack(KZ.body, A_GAP);
    return t > 140 && KQuietParts();
  case KW_ROAR:
    // the charge (its bow lights up), the roar, and nothing else
    if (t == 0) {
      KZ.roared = 0;
      Anim(KZ.body, 2);
      PartFrame(KZ.ping, 1);
      AbSound(ABS_CHARGE);
      if (!KZ.roars++) AbLineNow(L_HUNTER_ROAR);
    }
    if (!KZ.roared) {
      if (t < K_CHARGE_T || Busy(KZ.body)) return 0;
      OrderAt(KZ.body, A_ROAR, DOWN);
      Anim(KZ.body, Hp(KZ.body) * 10 < K_HP * 3 ? 3 : 0);
      PartFrame(KZ.ping, 0);
      KZ.roared = 1;
      return 0;
    }
    return !Busy(KZ.body);
  case KW_XPIN:
    // hard mode: the turrets close pincers, the bow lets go snipe rings
    if (t == 0) KGunOrder(A_XPINCER, 0, 1);
    if (t == 40) OrderAt(KZ.body, A_XSNIPE, fwd);
    if (t == 110) KGunOrder(A_XPINCER, 0, 1);
    return t > 170 && KQuietParts();
  case KW_XTWIST:
    // hard mode: twist rings from the bow, the turrets' streams through them
    if (t == 0) OrderAt(KZ.body, A_XTWIST, fwd);
    if (t == 60 || (hurt && t == 130)) KGunOrder(A_STREAM, 0, 1);
    return t > 150 && KQuietParts();
  case KW_XGAP:
    // hard mode: drifting-gap rings, the wipers sweeping across them
    if (t == 0) OrderAt(KZ.body, A_XGAP, AimAt(KZ.body));
    if (t == 30) KGunOrder(A_WIPE, fwd, 0);
    return t > 120 && KQuietParts();
  }
  return 1;
}

// Pick the next window from the plan (the dive's once past 55%).
static void KNext(void) {
  const int diving = Hp(KZ.body) * 100 < K_HP * 55 && KZ.roars;    // the roar (and the Doc's lesson) first
  if (diving && !KZ.diving) { KZ.diving = 1; KZ.step = 0; }
  const int hard = HardMode();
  const unsigned char* plan = KZ.diving ? (hard ? kPlanDiveH : kPlanDive) : (hard ? kPlanTopH : kPlanTop);
  const int n = KZ.diving ? (hard ? (int)sizeof kPlanDiveH : (int)sizeof kPlanDive)
                          : (hard ? (int)sizeof kPlanTopH : (int)sizeof kPlanTop);
  if (KZ.step >= n) KZ.step = 0;
  KZ.win = plan[KZ.step];
  if (++KZ.step >= n) KZ.step = 0;
  // never dive while below, nor surface while up top
  if ((KZ.win == KW_DIVE && KZ.low) || (KZ.win == KW_SURFACE && !KZ.low)) KZ.win = KW_SNAKE;
  KZ.wt = 0;
  KZ.vy = 0;
}

// The dive: up and over the top, bubbles boiling at the bottom where it will
// come up (away from the ship), then up out of the deep, nose first. The
// surfacing is the same backwards. Returns 1 when it is in place.
static int KMove(int w) {
  if (w == KW_DIVE) {
    if (!KZ.low && KZ.y > K_GONE_Y) {
      KZ.vy -= FX(0.06);
      if (KZ.vy < -FX(3.0)) KZ.vy = -FX(3.0);
      KZ.y += KZ.vy;
      if (KZ.y <= K_GONE_Y) {                    // gone: turn it round under the bottom edge
        KZ.low = 1;
        KZ.hx = PlayerX() < FX(200) ? FX(Rnd(250, 290)) : FX(Rnd(110, 150));
        KZ.x = KZ.px = KZ.hx;
        KZ.y = K_DEEP_Y;
        KZ.yaw = UP;
        KZ.wt = 0;
        if (!KZ.dove++) AbLine(L_HUNTER_DIVE);
      }
      return 0;
    }
    // bubbles boil up where it will surface, a second before it does
    if ((KZ.wt & 3) == 0 && KZ.wt < 90)
      SB_SpawnGeneric(BH, "*abyss/boss/bubbles", KZ.hx + FX(Rnd(-30, 30)), FX(Rnd(370, 420)));
    if (KZ.wt < 60) return 0;
    KZ.y += Ease(K_LOW_Y - KZ.y, FX(1.6));
    return AbAbs(KZ.y - K_LOW_Y) < FX(1);
  }
  // surfacing: down out of sight, round, and in again from the top
  if (KZ.low) {
    KZ.vy += FX(0.06);
    if (KZ.vy > FX(3.0)) KZ.vy = FX(3.0);
    KZ.y += KZ.vy;
    if (KZ.y >= K_DEEP_Y) {
      KZ.low = 0;
      KZ.x = KZ.px = FX(200);
      KZ.y = K_GONE_Y;
      KZ.yaw = DOWN;
    }
    return 0;
  }
  KZ.y += Ease(K_REST_Y - KZ.y, FX(1.6));
  return AbAbs(KZ.y - K_REST_Y) < FX(1);
}

static void KFight(void) {
  const int talk = InCutscene();
  const int moving = KZ.win == KW_DIVE || KZ.win == KW_SURFACE;
  KGuns(talk || moving);
  Guard(KZ.body, talk || moving);
  if (!talk) BossWards();
  const int battered = Hp(KZ.body) * 10 < K_HP * 3;
  if (KZ.anim_t > 0 && --KZ.anim_t == 0) Anim(KZ.body, battered ? 3 : 0);
  if (moving) {                                   // the dive goes on whoever talks
    KZ.wt++;
    if (KMove(KZ.win)) { KZ.win = 0; KZ.beat = 40; }
    return;
  }
  if (talk) return;
  if (KZ.win) {
    if (KWindow(KZ.win, KZ.wt++)) {
      KZ.win = 0;
      KZ.beat = battered ? 40 : K_BEAT;
      if (HardMode()) KZ.beat -= KZ.beat >> 2;                       // hard mode: a quarter less quiet
    }
    return;
  }
  if (KZ.beat > 0) { KZ.beat--; return; }
  KNext();
}

static void KUpdate(u32 boss) {
  KZ.t++;
  BlastTick();
  SpeechPump();
  switch (KZ.phase) {
  case K_INTRO:
    KZ.y += Ease(K_REST_Y - KZ.y, FX(1.2));
    if (AbAbs(KZ.y - K_REST_Y) < FX(1) && AbQuiet() && KZ.t > 120) {
      KZ.phase = K_FIGHT;
      KZ.t = 0;
    }
    break;
  case K_FIGHT:
    if (!Alive(KZ.body)) {
      KZ.phase = K_DEAD;
      KZ.t = 0;
      ShipShield(1);
      Blasts(KZ.x, KZ.y, 26, 80, 14);
      for (int k = 0; k < 2; k++) { Remove(KZ.gun[k]); KZ.gun[k] = 0; DropPart(&KZ.gvis[k]); }
      DropPart(&KZ.screw);
      DropPart(&KZ.ping);
      g_autopilot_target = 0;
      R32(boss, B_MAXHP) = 0;
      break;
    }
    // never quite still: across the top slowly, or swaying a little where it surfaced
    if (KZ.win != KW_DIVE && KZ.win != KW_SURFACE) {
      const s32 wx = KZ.low ? KZ.hx + SB_Sin((KZ.t * 0x14000) & (TURN - 1)) * 26
                            : FX(200) + SB_Sin((KZ.t * 0x14000) & (TURN - 1)) * 88;
      const s32 wy = (KZ.low ? K_LOW_Y : K_REST_Y) + SB_Sin((KZ.t * 0x23000) & (TURN - 1)) * 8;
      KZ.x += Ease(wx - KZ.x, FX(1.4));
      KZ.y += Ease(wy - KZ.y, FX(0.8));
    }
    KFight();
    break;
  case K_DEAD:
    if (KZ.t == 40) AbLineNow(L_HUNTER_DOWN);
    if (KZ.t > 120 && AbQuiet()) KZ.phase = K_OVER;
    break;
  case K_OVER:
    R32(boss, B_STATE) = 0;                     // the engine calls death next
    return;
  }
  // the nose swings into its slide, easing back as it slows
  const s32 base = KZ.low ? UP : DOWN;
  s32 want = base + (KZ.low ? 6 : -6) * (KZ.x - KZ.px);
  if (want > base + YAW_MAX) want = base + YAW_MAX;
  if (want < base - YAW_MAX) want = base - YAW_MAX;
  KZ.yaw = AbTurnTo(KZ.yaw, want & (TURN - 1), 0x8000);
  KZ.px = KZ.x;
  if (Alive(KZ.body)) Place(KZ.body, KZ.x, KZ.y);
  if (KZ.phase < K_DEAD) KParts();
}

static void KDeath(u32 boss) {
  for (int k = 0; k < 2; k++) DropPart(&KZ.gvis[k]);
  DropPart(&KZ.screw);
  DropPart(&KZ.ping);
  ShipShield(0);
  g_autopilot_target = 0;
  LeviathanBossInit(boss);
  SB_PausePartition(BH, 0);
}

void HunterInstall(u32 boss) {
  SB_ClearBytes((void*)boss, BOSS_SIZE);
  SB_ClearBytes(&KZ, sizeof KZ);
  R32(boss, B_SPAWN + 0x00) = (s32)(u32)KSpawn;
  R32(boss, B_SPAWN + 0x04) = (s32)(u32)KUpdate;
  R32(boss, B_SPAWN + 0x08) = (s32)(u32)KRender;
  R32(boss, B_SPAWN + 0x0c) = (s32)(u32)KRender;
  R32(boss, B_SPAWN + 0x10) = (s32)(u32)KDeath;
  R32(boss, B_HEALTH) = (s32)(u32)KHealth;
}

// ===================================================================================
// The Leviathan
// ===================================================================================
enum { V_IN = 1, V_HUNT, V_OPENING, V_MAW, V_DYING, V_ENDING, V_OVER };
#define V_REST_Y  FX(92)
#define NLINKS    7
#define BROW_Y    (-52)           // where the stalk rises, from the head's centre (art_deep._leviathan)
#define MOUTH_Y   36              // the furnace, from the head's centre
#define POD_X     102
#define POD_Y     16
#define CORE_HP   12000         // (abyss_cast.py "lev_core": long enough for the maw's windows)

static struct {
  u32 boss, head, lure, pod[2], core;
  u32 eye[2], fire;                    // its lights burning (glow.h): eyes, the furnace in its throat
  u32 link[NLINKS];
  int phase, t, jaw, pod_cool[2], roars, lure_t;
  int win, wt, beat, step;             // the window running, its moment, the quiet after, the plan's place
  s32 x, y, lx, ly;
  int ended;
} LV;

static s32 VHealth(u32 boss) {
  (void)boss;
  if (LV.phase == V_HUNT) return Hp(LV.lure) + Hp(LV.pod[0]) + Hp(LV.pod[1]);
  if (LV.phase == V_MAW || LV.phase == V_OPENING) return Hp(LV.core);
  return 0;
}
static void VRender(u32 boss, s32 x, s32 y) { (void)boss; (void)x; (void)y; }

// The lure swings in front of the jaws; the stalk is a chain of links on a
// curve from the brow, up over the head and down to it.
static void PlaceLure(void) {
  const s32 t = LV.lure_t;
  // slow and narrow enough for a laser to stay on it: faster and wider, the beam
  // spent its time on the armoured head beside it and the lure seemed immune
  LV.lx = LV.x + SB_Sin((t * 0xd000) & (TURN - 1)) * 40;
  LV.ly = LV.y + FX(96) + SB_Sin((t * 0x2b000 + 0x400000) & (TURN - 1)) * 16;
  const s32 bx = LV.x, by = LV.y + FX(BROW_Y);
  const s32 cx = LV.x + ((LV.lx - LV.x) >> 1), cy = LV.y + FX(BROW_Y - 40);
  for (int k = 0; k < NLINKS; k++) {
    // a quadratic Bezier, brow -> control -> lure, at u = (k + 1) / (NLINKS + 1)
    const s32 u = (k + 1) * 8192;                         // 65536 / 8
    const s32 v = FX(1) - u;
    const s32 a = SB_MulFP(v, v), b = 2 * SB_MulFP(u, v), c = SB_MulFP(u, u);
    const s32 x = SB_MulFP(a, bx) + SB_MulFP(b, cx) + SB_MulFP(c, LV.lx);
    const s32 y = SB_MulFP(a, by) + SB_MulFP(b, cy) + SB_MulFP(c, LV.ly);
    if (LV.link[k]) AbPut(LV.link[k], x, y);
  }
  if (Alive(LV.lure)) Place(LV.lure, LV.lx, LV.ly);
}

// Its eyes and the furnace in its throat burning, over the head, whatever
// its jaw (R18 0 shut .. 3 wide); the furnace only while the jaw is open and
// the core -- the furnace itself, a target -- is not out in front of it.
static void PlaceGlow(void) {
  static const signed char kEyes[4] = { 0, 1, 2, 3 };
  static const signed char kFire[4] = { GLOW_OFF, 0, 1, 2 };
  const u32 a = Alive(LV.head) ? Act(LV.head) : 0;
  GlowPut(LV.eye[0], LV.x, LV.y, LEV_EYE_L_X, LEV_EYE_L_Y, GlowFor(a, kEyes, 4));
  GlowPut(LV.eye[1], LV.x, LV.y, LEV_EYE_R_X, LEV_EYE_R_Y, GlowFor(a, kEyes, 4));
  GlowPut(LV.fire, LV.x, LV.y, LEV_FIRE_X, LEV_FIRE_Y, Alive(LV.core) ? GLOW_OFF : GlowFor(a, kFire, 4));
}

static void PlaceAll(void) {
  if (Alive(LV.head)) Place(LV.head, LV.x, LV.y);
  PlaceGlow();
  for (int s = 0; s < 2; s++)
    if (Alive(LV.pod[s])) Place(LV.pod[s], LV.x + FX(s ? POD_X : -POD_X), LV.y + FX(POD_Y));
  if (Alive(LV.core)) Place(LV.core, LV.x, LV.y + FX(MOUTH_Y));
  PlaceLure();
}

static void VSpawn(u32 boss) {
  LV.boss = boss;
  LV.x = FX(200);
  LV.y = FX(-150);
  LV.head = SB_SpawnBadGuy(BH, "*abyss/boss/lev_head", LV.x, LV.y);
  // (before the lure's links: the stalk passes in front of the jaws)
  LV.eye[0] = GlowSpawn(BH, "*abyss/boss/lev_eye_l");
  LV.eye[1] = GlowSpawn(BH, "*abyss/boss/lev_eye_r");
  LV.fire = GlowSpawn(BH, "*abyss/boss/lev_fire");
  for (int s = 0; s < 2; s++) LV.pod[s] = SB_SpawnBadGuy(BH, "*abyss/boss/lev_pod", LV.x, LV.y);
  LV.lure = SB_SpawnBadGuy(BH, "*abyss/boss/lev_lure", LV.x, LV.y);
  for (int k = 0; k < NLINKS; k++) LV.link[k] = AbSpawn("*abyss/boss/lure_link", LV.x, LV.y);
  Guard(LV.head, 1);
  Guard(LV.lure, 1);
  Guard(LV.pod[0], 1);
  Guard(LV.pod[1], 1);
  PlaceAll();
  LV.phase = V_IN;
  LV.t = 0;
  R32(boss, B_STATE) = 1;
  SB_PausePartition(BH, 1);
  R32(boss, B_MAXHP) = VHealth(boss) ? VHealth(boss) : 1;
  g_autopilot_target = LV.lure;
  AbLine(L_LEV_IN);
  AbLine(L_LEV_DOC);
}

static void Roar(void) {
  OrderAt(LV.head, A_ROAR, DOWN);
  if (!LV.roars++) AbLineNow(L_LEV_ROAR);
}

// The windows. Its lure and pods alive (the hunt): curtains off the jaw;
// the lure's commas; rings whose gap drifts; a spiral with aimed triplets;
// the roar alone. The pods' torpedoes are the constant layer under all of it
// but the roar. The jaw open (the maw): the oscillating spiral out of the
// furnace; curtains; the breath (the pull, and rings that go out and bend
// back into the maw) and the roar after it; twist rings; spit.
enum { LW_CURTAIN = 1, LW_COMMA, LW_GAP, LW_SPIRAL, LW_ROAR, LW_OSC, LW_BREATH, LW_TWIST, LW_SPIT,
       LW_XPIN, LW_XSNIPE, LW_XSNAKE, LW_XGAP };
static const unsigned char kPlanHunt[] = { LW_CURTAIN, LW_COMMA, LW_GAP, LW_ROAR, LW_SPIRAL, LW_COMMA, LW_CURTAIN,
                                           LW_GAP, LW_ROAR };
static const unsigned char kPlanMaw[] = { LW_OSC, LW_CURTAIN, LW_BREATH, LW_ROAR, LW_TWIST, LW_SPIT, LW_OSC,
                                          LW_BREATH, LW_ROAR, LW_CURTAIN };
// Hard mode (docs/HARD_MODE.md): the lure's pincers under the jaw's curtains,
// snipe rings over the spiral; out of the maw the snake under the teeth and
// drifting-gap rings through the curtains -- the beats a third shorter, the
// pods quicker.
static const unsigned char kPlanHuntH[] = { LW_CURTAIN, LW_XPIN, LW_COMMA, LW_GAP, LW_XSNIPE, LW_ROAR, LW_SPIRAL,
                                            LW_XPIN, LW_CURTAIN, LW_GAP, LW_ROAR };
static const unsigned char kPlanMawH[] = { LW_OSC, LW_XSNAKE, LW_CURTAIN, LW_BREATH, LW_ROAR, LW_TWIST, LW_XGAP,
                                           LW_SPIT, LW_OSC, LW_XSNIPE, LW_BREATH, LW_ROAR, LW_CURTAIN };
#define BREATH_T   150

static int VFury(void) { return LV.phase == V_MAW && Hp(LV.core) * 5 < CORE_HP * 2; }

static int VQuiet(void) {
  if (Alive(LV.head) && Busy(LV.head)) return 0;
  if (Alive(LV.lure) && Busy(LV.lure)) return 0;
  if (Alive(LV.core) && Busy(LV.core)) return 0;
  return 1;
}

// the attacker of the moment: the lure while it lives, else the furnace
static u32 VEye(void) { return Alive(LV.lure) ? LV.lure : (Alive(LV.core) ? LV.core : LV.head); }

static int VWindow(int w, int t) {
  const int fury = VFury();
  switch (w) {
  case LW_CURTAIN:
    if (t == 0) Attack(LV.head, A_CURTAIN);
    return t > 160 && VQuiet();
  case LW_COMMA:
    if (t == 0) Attack(VEye(), A_COMMA);
    return t > 60 && VQuiet();
  case LW_GAP:
    if (t == 0) Attack(VEye(), A_GAP);
    return t > 60 && VQuiet();
  case LW_SPIRAL:
    if (t == 0) Attack(LV.head, A_SPIRAL);
    if (t == 50 || t == 110) Attack(VEye(), A_TRIPLE);
    return t > 120 && VQuiet();
  case LW_ROAR:
    if (t == 0) Roar();
    return t > 10 && VQuiet();
  case LW_OSC:
    if (t == 0) Attack(LV.core, A_OSC);
    return t > 60 && VQuiet();
  case LW_BREATH:
    // breathing in: everything drawn toward the maw, and rings out of it
    // that bend back in; the roar comes next (its own window)
    if (t == 0) AbSound(ABS_CHARGE);
    if (t == 10) Attack(LV.core, A_INHALE);
    // toward the maw -- but to a point well below it: pulled up against the
    // furnace (to 30 below it, as it was), the ship's shots started past it and
    // all struck the armoured head: the Leviathan seemed to take no damage
    AbDraw(LV.x, LV.y + FX(MOUTH_Y + 120), fury ? FX(1.3) : FX(1.0));
    if ((t & 7) == 0)
      SB_SpawnGeneric(BH, "*abyss/boss/bubbles", LV.x + FX(Rnd(-120, 120)), LV.y + FX(Rnd(140, 300)));
    return t >= BREATH_T && VQuiet();
  case LW_TWIST:
    if (t == 0) Attack(LV.core, A_TWIST);
    if (fury && t == 60) Attack(LV.head, A_TEETH);
    return t > 70 && VQuiet();
  case LW_SPIT:
    if (t == 0) Attack(LV.head, A_SPIT);
    if (t == 40) Attack(LV.core, A_GAP);
    return t > 60 && VQuiet();
  case LW_XPIN:
    if (t == 0) Attack(VEye(), A_XPINCER);
    if (t == 30) Attack(LV.head, A_CURTAIN);
    return t > 170 && VQuiet();
  case LW_XSNIPE:
    if (t == 0) Attack(LV.head, A_SPIRAL);
    if (t == 40) Attack(VEye(), A_XSNIPE);
    return t > 120 && VQuiet();
  case LW_XSNAKE:
    if (t == 0) Attack(LV.core, A_XSNAKE);
    if (t == 50) Attack(LV.head, A_TEETH);
    return t > 90 && VQuiet();
  case LW_XGAP:
    if (t == 0) Attack(LV.core, A_XGAP);
    if (t == 20) Attack(LV.head, A_CURTAIN);
    return t > 170 && VQuiet();
  }
  return 1;
}

static void VPods(int fury) {
  // the constant layer: the pods take turns at a pair of torpedoes
  if (LV.win == LW_ROAR || LV.win == LW_BREATH) return;
  for (int s = 0; s < 2; s++) {
    if (!Alive(LV.pod[s])) continue;
    if (LV.pod_cool[s] > 0) { LV.pod_cool[s]--; continue; }
    if (Busy(LV.pod[s])) continue;
    Attack(LV.pod[s], A_PODTORP);
    Anim(LV.pod[s], 1);
    LV.pod_cool[s] = fury ? 220 : 300;            // each; the two half a cycle apart
    if (HardMode()) LV.pod_cool[s] -= LV.pod_cool[s] >> 2;
  }
}

static void VFight(void) {
  const int talk = InCutscene();
  if (LV.phase == V_HUNT) Guard(LV.lure, talk);
  else Guard(LV.core, talk);
  Guard(LV.pod[0], talk);
  Guard(LV.pod[1], talk);
  if (talk) return;
  BossWards();
  const int fury = VFury();
  VPods(fury);
  if (LV.win) {
    if (VWindow(LV.win, LV.wt++)) {
      LV.win = 0;
      LV.beat = fury ? 30 : 50;
      if (HardMode()) LV.beat = fury ? 20 : 34;
    }
    return;
  }
  if (LV.beat > 0) { LV.beat--; return; }
  const int hard = HardMode();
  const unsigned char* plan = LV.phase == V_HUNT ? (hard ? kPlanHuntH : kPlanHunt) : (hard ? kPlanMawH : kPlanMaw);
  const int n = LV.phase == V_HUNT ? (hard ? (int)sizeof kPlanHuntH : (int)sizeof kPlanHunt)
                                   : (hard ? (int)sizeof kPlanMawH : (int)sizeof kPlanMaw);
  if (LV.step >= n) LV.step = 0;
  LV.win = plan[LV.step];
  if (++LV.step >= n) LV.step = 0;
  LV.wt = 0;
}

static void VUpdate(u32 boss) {
  LV.t++;
  LV.lure_t++;
  BlastTick();
  SpeechPump();
  // sway, heavy and slow
  const s32 rest_x = FX(200) + SB_Sin((LV.t * 0x9000) & (TURN - 1)) * 46;
  const s32 rest_y = V_REST_Y + SB_Sin((LV.t * 0x15000) & (TURN - 1)) * 7;
  switch (LV.phase) {
  case V_IN:
    LV.y += Ease(V_REST_Y - LV.y, FX(1.0));
    if (AbAbs(LV.y - V_REST_Y) < FX(1) && AbQuiet() && LV.t > 140) {
      LV.phase = V_HUNT;
      LV.t = 0;
      LV.beat = 50;
      LV.step = 0;
      LV.pod_cool[0] = 120;
      LV.pod_cool[1] = 270;
    }
    break;
  case V_HUNT:
    LV.x += Ease(rest_x - LV.x, FX(0.8));
    LV.y += Ease(rest_y - LV.y, FX(0.8));
    if (!Alive(LV.lure)) {                        // blinded: the jaw opens
      LV.lure = 0;
      for (int k = 0; k < NLINKS; k++) AbDrop(&LV.link[k]);
      Blasts(LV.lx, LV.ly, 10, 10, 5);
      LV.phase = V_OPENING;
      LV.t = 0;
      LV.win = 0;
      Stop(LV.head);
      AbLine(L_LEV_OPEN);
      break;
    }
    if (!Alive(g_autopilot_target)) g_autopilot_target = Alive(LV.lure) ? LV.lure : 0;
    VFight();
    break;
  case V_OPENING:
    LV.x += Ease(rest_x - LV.x, FX(0.8));
    if (LV.t == 20 || LV.t == 40 || LV.t == 60) Anim(LV.head, LV.t == 20 ? 1 : (LV.t == 40 ? 2 : 3));
    if (LV.t == 60) {
      LV.core = SB_SpawnBadGuy(BH, "*abyss/boss/lev_core", LV.x, LV.y + FX(MOUTH_Y));
      Guard(LV.core, 1);
      R32(boss, B_MAXHP) = Hp(LV.core);
      g_autopilot_target = LV.core;
    }
    if (LV.t > 100) {
      LV.phase = V_MAW;
      LV.t = 0;
      LV.win = 0;
      LV.step = 0;
      LV.beat = 40;
    }
    break;
  case V_MAW:
    LV.x += Ease(rest_x - LV.x, FX(0.8));
    LV.y += Ease(rest_y - LV.y, FX(0.8));
    if (!Alive(LV.core)) {
      LV.core = 0;
      LV.phase = V_DYING;
      LV.t = 0;
      ShipShield(1);
      Stop(LV.head);
      Blasts(LV.x, LV.y, 90, 70, 26);
      g_autopilot_target = 0;
      R32(boss, B_MAXHP) = 0;
      AbLineNow(L_LEV_LOSE);
      break;
    }
    VFight();
    break;
  case V_DYING:
    if (LV.t == 100) {
      for (int s = 0; s < 2; s++) Remove(LV.pod[s]);
      Remove(LV.head);
      for (int s = 0; s < 2; s++) GlowDrop(&LV.eye[s]);
      GlowDrop(&LV.fire);
      AbBoom(LV.x, LV.y, 1);
    }
    if (LV.t > 160 && BW.left <= 0 && AbQuiet()) {
      LV.phase = V_ENDING;
      LV.t = 0;
      AbLine(L_END_TRACE);          // the Doc: the relays give up Argus's signal: Marrow's lab
      AbLine(L_END_MARROW);         // Marrow, listening in
      AbLine(L_END_VOLTA);          // Volta: we're coming
    }
    break;
  case V_ENDING:
    if (LV.t > 200 && AbQuiet() && !SpeechPending()) LV.phase = V_OVER;
    break;
  case V_OVER:
    R32(boss, B_STATE) = 0;
    return;
  }
  if (LV.phase < V_DYING) PlaceAll();
}

static void VDeath(u32 boss) {
  (void)boss;
  ShipShield(0);
  g_autopilot_target = 0;
  SB_PausePartition(BH, 0);                      // on to the level's end
}

void LeviathanBossInit(u32 boss) {
  SB_ClearBytes((void*)boss, BOSS_SIZE);
  SB_ClearBytes(&LV, sizeof LV);
  SB_ClearBytes(&BW, sizeof BW);
  R32(boss, B_SPAWN + 0x00) = (s32)(u32)VSpawn;
  R32(boss, B_SPAWN + 0x04) = (s32)(u32)VUpdate;
  R32(boss, B_SPAWN + 0x08) = (s32)(u32)VRender;
  R32(boss, B_SPAWN + 0x0c) = (s32)(u32)VRender;
  R32(boss, B_SPAWN + 0x10) = (s32)(u32)VDeath;
  R32(boss, B_HEALTH) = (s32)(u32)VHealth;
}

// InitBoss (worlds.c's row for world 11): a new run of the level, from the Kurozame.
void AbyssReset(void);
// abyss.c: which red outline (art_deep.OUTLINED: 2 the Kurozame, 3 the
// Leviathan's head, 4 its lure, 5 a pod) a part that turned a hit wears,
// and the frame its animation register shows.
// A dead badguy's handle goes to the next one spawned, so a part's handle
// counts only while its boss is up, and only if what holds it now still
// carries the part's mark (the hunter and the pods 62, the lure 61).
int AbyssPartOutline(u32 bg, int* frame) {
  if (!bg) return -1;
  const s32 m = GetReg(bg, 21) >> 16;
  int k = -1;
  if (KZ.phase >= K_INTRO && KZ.phase < K_DEAD) {
    if (bg == KZ.body && m == MARK_SNIPER) k = 2;
    else if (bg == KZ.gun[0] || bg == KZ.gun[1]) return -1;     // under their own pictures: no outline
  }
  if (k < 0 && LV.phase >= V_IN && LV.phase < V_DYING) {
    if (bg == LV.head && m == 0) k = 3;
    else if (bg == LV.lure && m == MARK_LASER) k = 4;
    else if ((bg == LV.pod[0] || bg == LV.pod[1]) && m == MARK_SNIPER) k = 5;
  }
  if (k < 0) return -1;
  *frame = k == 4 ? 0 : GetReg(bg, REG_ANIM) >> 16;
  return k;
}

void HunterBossInit(u32 boss) {
  SpeechReset();
  AbyssReset();
  SB_ClearBytes(&BW, sizeof BW);
  HunterInstall(boss);
}
