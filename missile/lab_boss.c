// lab_boss.c -- Mission 12's two bosses: Specimen Ten, the Lion's Mane, and
// Specimen Eleven, the Kraken.
//
// The Lion's Mane (the mid-boss), in the boneyard: a great jellyfish, its
// bell drifting across the top of the screen, six long tentacles hanging
// from its rim -- chains of stinging segments laid out by the code every
// frame. Its real weapon is the swing: a tentacle draws back, glowing white,
// the bell lunges at the ship, and the tentacle whips across the screen. A
// swing that lands takes a whole capsule. Get beyond its reach (the bottom
// of the screen), or spin into it: a parried swing recoils, jolts the bell
// and stuns it. Between swings: stings from the tips, rings from the bell,
// a rain of stings, and the pressure wave nothing dodges. Below half health
// it rages: both outer tentacles swing at once, crossing, and it sheds
// polyps.
//
// The Kraken (the final boss), wrapped round Dr. Marrow's laboratory: an
// armoured mantle at the top of the screen, six arms of tapering segments.
//   * its aura, breathed out of the siphon under its beak: a ring of violet
//     light that swells and shrinks; inside it the ship's shield drains (and
//     the Kraken drinks it), and with the shield gone it bleeds capsules.
//     The Entropy Engine is out down here: only parries fill the shield;
//   * slams: an arm rears up, red, a line of warnings marks the lane, and it
//     comes down across the screen -- a capsule if it lands (a spin turns it
//     away and hurts what swung it);
//   * the clubs on two arms fire sucker volleys; the beak spits ink, and it
//     roars (the wave: spin).
//   * now and then the aura flares: it draws in, hot and pink, then blazes
//     out far wider for a second or so, drinking three times as fast.
// Phase one: the mantle is brass and shrugs everything off; the clubs
// (coilgun) and the siphon (laser) are the targets. Phase two, the eyes:
// the shutters over its eyes open, one eye at a time, the pupil following
// the ship -- an eye can be hurt only while open, and it glares (little
// eyes left hanging in the water that dart at a ship that comes near),
// stares (a stream that turns after the ship) and weeps irises (shots
// that open into curling tears); the Kraken starts grabbing the ship -- an
// arm reaches, catches, drags it toward the beak, drinking twice as fast:
// spin (or tear free). Blind one and the other rages. Phase three, the
// deep: blinded, it pulls up out of sight and its arms come down both sides
// of the screen as walls that close in, squeezing now and then; Dr.
// Marrow's Specimen Nine (Mission 10's sub) comes in over the top towing
// three great lure lanterns -- graze rings, jammed pings, lanterns let go --
// and only when all three are shot down do the walls let go. Phase four:
// it comes back armoured, and only now UNITY's squadron Trident comes up
// from below and torpedoes the mantle until its plates crack (then anything
// hurts it); it takes one of the boats. Below 40%: fury -- a wider aura,
// double slams, it breathes in, and the boats fire a full spread on Volta's
// word. Dead, the ending plays (lab.c LabEnding) before the level ends.
//
// Built like the Trench's bosses (abyss_boss.c): parts are badguys with one
// attack dispatcher (lab/lab_cast.py: R20 the command, R19 busy, R13 aim,
// R18 the animation); limbs are generics the code lays out.
//
// Same rules as missile.c: no libc, no division, engine reached by address.

#include "lab.h"
#include "glow.h"
#include "lab_glow.h"

// the attacks (lab_cast.CMDS)
enum { C_PULSE = 1, C_STINGS, C_BROOD, C_ROAR, C_SPARKS, C_INK, C_SPIRAL, C_SUCKERS, C_RAIN, C_TORPS, C_INKSTORM,
       C_LASH, C_HOOKS, C_CURLS, C_FANS, C_SWEEP,
       LC_XSNIPE, LC_XPINCER, LC_XTWIST, LC_XSNAKE, LC_XGAP,     // hard mode's (beach/hardkit.py)
       C_GLARE, C_STARE, C_IRIS, C_LPULSE, C_LJAM, C_LBROOD,     // the eyes', Specimen Nine's lanterns'
       // after Missions 2 and 3's mid-bosses (docs/BOSS_ATTACKS_M2_M4.md), and the eyelets'
       C_NETL, C_NETR, C_HEADFANS, C_SNAKE, C_WIPEL, C_WIPER, C_MINES, C_GAZE, C_GAZEOUT,
       C_LRING, C_LSPIRAL, C_LLANTERNS };                         // Mission 10's lure's (sunda_cast.py)

void LabReset(void);              // lab.c

static s32 Hp(u32 bg) { return Alive(bg) ? SB_GetBadGuyHealth(bg) : 0; }

static s32 AimFrom(u32 bg) {
  s32 x, y;
  SB_GetActorPosition(Act(bg), &x, &y);
  return AbAngle(x, y, PlayerX(), PlayerY());
}

static void Attack(u32 bg, int cmd) {
  if (Alive(bg)) OrderAt(bg, cmd, AimFrom(bg));
}

static void Heal(u32 bg, s32 by, s32 max) {
  if (!Alive(bg)) return;
  s32 h = SB_GetBadGuyHealth(bg) + by;
  if (h > max) h = max;
  SB_SetBadGuyHealth(bg, h);
}

// 0 .. 0x10000, eased in and out
static s32 Smooth(s32 u) {
  if (u <= 0) return 0;
  if (u >= FX(1)) return FX(1);
  return SB_MulFP(SB_MulFP(u, u), FX(3) - 2 * u);
}

// A wave of blasts over a dying boss (abyss_boss.c's).
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
// Limbs: a chain of segments, each turned a little more than the last
// ===================================================================================
#define MAXSEG 12
typedef struct {
  int n, classes, live;
  u32 seg[MAXSEG];
  unsigned char cls[MAXSEG];      // each segment's size, root .. tip
  short shown[MAXSEG];            // the frame it shows (set only when it changes)
  s32 jx[MAXSEG + 1], jy[MAXSEG + 1];
  s32 base, curl, len, amp, ph, kph;
  int state;                      // 0 at rest, 1 lit (to strike, or gripping)
  int big;                        // shown at half its sizes' steps (the Kraken's walls: all thick)
} Limb;

static void LimbSpawn(Limb* L, const char* name, int n, int classes, s32 x, s32 y) {
  L->n = n;
  L->classes = classes;
  int acc = 0, c = 0;
  for (int k = 0; k < n; k++) {
    L->cls[k] = (unsigned char)(c < classes ? c : classes - 1);
    acc += classes;
    if (acc >= n) { acc -= n; c++; }
    L->seg[k] = AbSpawn(name, x, y);
    L->shown[k] = -1;
  }
  L->live = 1;
}

static void LimbPose(Limb* L, s32 rx, s32 ry) {
  L->jx[0] = rx;
  L->jy[0] = ry;
  for (int k = 0; k < L->n; k++) {
    const s32 a = (L->base + L->curl * k + SB_MulFP(L->amp, SB_Sin((L->ph + L->kph * k) & (TURN - 1)))) & (TURN - 1);
    L->jx[k + 1] = L->jx[k] + SB_MulFP(SB_Cos(a), L->len);
    L->jy[k + 1] = L->jy[k] + SB_MulFP(SB_Sin(a), L->len);
  }
}

static void LimbPlace(Limb* L) {
  if (!L->live) return;
  for (int k = 0; k < L->n; k++) {
    AbPut(L->seg[k], L->jx[k + 1], L->jy[k + 1]);
    const int f = (L->big ? L->cls[k] >> 1 : L->cls[k]) * 2 + (L->state ? 1 : 0);   // each size at rest, then lit (8 animations at most)
    if (L->shown[k] != f) { AbFrame(L->seg[k], f); L->shown[k] = (short)f; }
  }
}

static void LimbDrop(Limb* L) {
  for (int k = 0; k < L->n; k++) AbDrop(&L->seg[k]);
  L->live = 0;
}

// Does any joint from `from` to the tip come within r units of (x, y)?
static int LimbNear(const Limb* L, int from, s32 x, s32 y, s32 r) {
  if (!L->live) return 0;
  for (int k = from; k <= L->n; k++)
    if (AbDist(L->jx[k] - x, L->jy[k] - y) < r) return 1;
  return 0;
}

static s32 TipX(const Limb* L) { return L->jx[L->n]; }
static s32 TipY(const Limb* L) { return L->jy[L->n]; }

// Turn the limb's base toward `want` by at most `rate`, its other settings
// eased toward the given ones.
static void LimbToward(Limb* L, s32 want, s32 rate, s32 curl, s32 len, s32 amp) {
  L->base = AbTurnTo(L->base, want & (TURN - 1), rate);
  L->curl += Ease(curl - L->curl, 0x8000);
  L->len += Ease(len - L->len, FX(0.6));
  L->amp += Ease(amp - L->amp, 0x8000);
}

// ===================================================================================
// Specimen Ten: the Lion's Mane
// ===================================================================================
enum { M_IN = 1, M_FIGHT, M_DEAD, M_OVER };
enum { SW_NONE, SW_WIND, SW_SWEEP, SW_BACK };
#define MN_TENT   6
#define MN_SEGS   11
#define MN_HP     7200
#define MN_SEG    FX(15)
#define MN_REST_Y FX(100)
#define WIND_T    46
#define SWEEP_T   24
#define BACK_T    40
#define STUN_T    150
#define SWING_DMG 520             // a parried swing jolts the bell this much
#define WHIP_WIND 24              // the whiplash: each swing after the first winds up this fast ...
#define WHIP_BACK 12              // ... and is barely drawn back before the next
// its other moves (MN.act): the bloom (every tentacle flares, then every tip
// fires), the curtain (it glides across the top, its tips raining stings),
// the jet dive (it plunges at the ship and bursts a ring point-blank)
enum { MA_NONE, MA_BLOOM, MA_CURTAIN, MA_DIVE };
#define BLOOM_T   40              // the bloom's flare, then the tips fire
#define CURTAIN_T 170
#define DIVE_WIND 34              // drawn up, tentacles high, before it plunges
#define DIVE_HIT  64              // spin within this of the bell as it arrives: stunned

static struct {
  u32 boss, bell, emit;
  u32 tip[MN_TENT];               // an emitter at each tentacle's tip (the bloom, the curtain)
  Limb tent[MN_TENT];
  int phase, t, cool, round, stun, rage, said_parry, said_brood, broods;
  // the swing (one tentacle, or two crossing)
  int sw, sw_t, sw_k[2], sw_n, sw_hit;
  int wind, chain;                // this swing's wind-up; swings still to follow (the whiplash)
  s32 sw_from[2], sw_to[2];
  s32 x, y, lx, ly;
  int lunge;
  int act, act_t, act_hit, act_k;
  s32 ax0, ax1, ay;               // the curtain's glide; the dive's mark
} MN;

// where tentacle i hangs from the bell's rim, and the way it points at rest
static s32 MnRootA(int i) { return (DOWN + (2 * i - 5) * 0x0a0000) & (TURN - 1); }
static void MnRoot(int i, s32* x, s32* y) {
  const s32 a = MnRootA(i);
  *x = MN.x + SB_Cos(a) * 46;
  *y = MN.y + SB_Sin(a) * 34 - FX(6);
}

static s32 MnHealth(u32 boss) { (void)boss; return MN.phase == M_FIGHT ? Hp(MN.bell) : 0; }
static void MnRender(u32 boss, s32 x, s32 y) { (void)boss; (void)x; (void)y; }

static void MnRestPose(int i) {
  Limb* L = &MN.tent[i];
  const s32 drift = (2 * i - 5) * -0x14000;                         // curving back toward the middle
  LimbToward(L, MnRootA(i), 0x30000, drift, MN_SEG, 0x90000);
  L->ph = (MN.t * 0x38000 + i * 0x2a0000) & (TURN - 1);
  L->kph = 0x1c0000;
  if (L->state == 1) L->state = 0;
}

static void MnPlaceAll(void) {
  if (Alive(MN.bell)) Place(MN.bell, MN.x, MN.y);
  for (int i = 0; i < MN_TENT; i++) {
    s32 rx, ry;
    MnRoot(i, &rx, &ry);
    LimbPose(&MN.tent[i], rx, ry);
    LimbPlace(&MN.tent[i]);
    if (Alive(MN.tip[i])) Place(MN.tip[i], TipX(&MN.tent[i]), TipY(&MN.tent[i]));
  }
}

static void MnSpawn(u32 boss) {
  MN.boss = boss;
  MN.x = FX(200);
  MN.y = FX(-170);
  MN.bell = SB_SpawnBadGuy(BH, "*lab/boss/bell", MN.x, MN.y);
  MN.emit = SB_SpawnBadGuy(BH, "*lab/boss/emitter", MN.x, MN.y);
  for (int i = 0; i < MN_TENT; i++) {
    Limb* L = &MN.tent[i];
    L->base = MnRootA(i);
    L->len = MN_SEG;
    LimbSpawn(L, "*lab/boss/mane_seg", MN_SEGS, 3, MN.x, MN.y);
    MN.tip[i] = SB_SpawnBadGuy(BH, "*lab/boss/emitter", MN.x, MN.y);
  }
  Guard(MN.bell, 1);
  MnPlaceAll();
  MN.phase = M_IN;
  MN.t = 0;
  MN.cool = 80;
  R32(boss, B_STATE) = 1;
  SB_PausePartition(BH, 1);
  R32(boss, B_MAXHP) = MN_HP;
  g_autopilot_target = MN.bell;
  AbLine(LB_MANE_IN);
  AbLine(LB_MANE_DOC);
}

static void Swing(int k0, int k1) {
  MN.sw = SW_WIND;
  MN.sw_t = 0;
  MN.wind = WIND_T;
  MN.sw_hit = 0;
  MN.sw_n = k1 >= 0 ? 2 : 1;
  MN.sw_k[0] = k0;
  MN.sw_k[1] = k1;
  for (int j = 0; j < MN.sw_n; j++) {
    const int k = MN.sw_k[j];
    // the right-hand tentacles (0..2) sweep right to left, the left-hand ones back
    const int d = k < 3 ? 1 : -1;
    MN.sw_from[j] = (DOWN - d * 0x4a0000) & (TURN - 1);
    MN.sw_to[j] = (DOWN + d * 0x3c0000) & (TURN - 1);
    MN.tent[k].state = 1;
  }
  // the bell lunges at the ship as it winds up
  MN.lunge = 1;
  s32 lx = PlayerX(), ly = PlayerY() - FX(178);
  if (lx < FX(120)) lx = FX(120);
  if (lx > FX(280)) lx = FX(280);
  if (ly < FX(96)) ly = FX(96);
  if (ly > FX(168)) ly = FX(168);
  MN.lx = lx;
  MN.ly = ly;
  AbSound(ABS_CHARGE);
}

// A swing's frame. Contact, in the sweep: a parry (the Spin Drive's window
// open) turns it back and stuns the bell; otherwise a capsule.
static void SwingTick(int talk) {
  if (MN.sw == SW_NONE) return;
  MN.sw_t++;
  for (int j = 0; j < MN.sw_n; j++) {
    const int k = MN.sw_k[j];
    Limb* L = &MN.tent[k];
    const int d = k < 3 ? 1 : -1;
    L->ph = (MN.t * 0x38000 + k * 0x2a0000) & (TURN - 1);
    if (MN.sw == SW_WIND) {
      LimbToward(L, MN.sw_from[j], 0x60000, d * 0x2c000, MN_SEG, 0x30000);
    } else if (MN.sw == SW_SWEEP) {
      const s32 u = Smooth(MN.sw_t * 2730);                              // 65536 / 24
      s32 span = (MN.sw_to[j] - MN.sw_from[j]) & (TURN - 1);
      if (span >= TURN / 2) span -= TURN;
      L->base = (MN.sw_from[j] + SB_MulFP(span, u)) & (TURN - 1);
      L->curl = -d * 0x18000;
      L->len = MN_SEG + SB_MulFP(FX(1.6), u);
      L->amp = 0x10000;
    } else {
      MnRestPose(k);
    }
  }
  if (MN.sw == SW_SWEEP && !talk && !MN.sw_hit && !LabShipDown()) {
    const s32 sx = PlayerX(), sy = PlayerY();
    for (int j = 0; j < MN.sw_n; j++) {
      const Limb* L = &MN.tent[MN.sw_k[j]];
      if (!LimbNear(L, 3, sx, sy, 14)) continue;
      MN.sw_hit = 1;
      if (AbParrying()) {
        // turned: the tentacle recoils, the bell is jolted and stunned
        AbSound(ABS_PARRY);
        AbBoom(sx, sy - FX(8), 0);
        for (int i = 0; i < MN.sw_n; i++) MN.tent[MN.sw_k[i]].state = 0;
        MN.sw = SW_BACK;
        MN.sw_t = 0;
        MN.chain = 0;                    // (a parry ends a whiplash too)
        MN.stun = STUN_T;
        Anim(MN.bell, 2);
        Guard(MN.bell, 0);
        SB_StrikeBadGuy(BH, MN.bell, SWING_DMG);
        if (!MN.said_parry++) AbLine(LB_MANE_PARRY);
      } else if (LabHitShip(1)) {
        AbBoom(sx, sy, 0);
      }
      break;
    }
  }
  if (MN.sw == SW_WIND && MN.sw_t >= MN.wind) { MN.sw = SW_SWEEP; MN.sw_t = 0; }
  else if (MN.sw == SW_SWEEP && MN.sw_t >= SWEEP_T) {
    MN.sw = SW_BACK;
    MN.sw_t = 0;
    for (int j = 0; j < MN.sw_n; j++) MN.tent[MN.sw_k[j]].state = 0;
  } else if (MN.sw == SW_BACK && MN.sw_t >= (MN.chain > 0 ? WHIP_BACK : BACK_T)) {
    for (int j = 0; j < MN.sw_n; j++) MN.tent[MN.sw_k[j]].state = 0;
    if (MN.chain > 0 && !talk) {
      // the whiplash: the far side's tentacle comes straight back across
      MN.chain--;
      Swing(MN.sw_k[0] < 3 ? 5 : 0, -1);
      MN.wind = WHIP_WIND;
      return;
    }
    MN.chain = 0;
    MN.sw = SW_NONE;
    MN.lunge = 0;
  }
}

// ---- the bloom, the curtain, the jet dive --------------------------------------------------------
static void MnAct(int which) {
  MN.act = which;
  MN.act_t = 0;
  MN.act_hit = 0;
  if (which == MA_CURTAIN) {
    // from the side it is on to the other, high over the ship
    MN.ax0 = MN.x;
    MN.ax1 = MN.x < FX(200) ? FX(310) : FX(90);
    MN.ay = FX(84);
  }
  if (which == MA_DIVE || which == MA_BLOOM) AbSound(ABS_CHARGE);
}

static void MnActEnd(void) {
  MN.act = MA_NONE;
  for (int i = 0; i < MN_TENT; i++) MN.tent[i].state = 0;
}

// Where the bell is going while it acts (MnUpdate's drift otherwise).
static void MnActTarget(s32* hx, s32* hy) {
  const int t = MN.act_t;
  if (MN.act == MA_CURTAIN) {
    const s32 u = Smooth(t * 386);                                     // 65536 / 170
    *hx = MN.ax0 + SB_MulFP(MN.ax1 - MN.ax0, u);
    *hy = MN.ay;
  } else if (MN.act == MA_DIVE) {
    if (t < DIVE_WIND) { *hx = MN.x; *hy = FX(70); }                    // drawing itself up
    else if (t < DIVE_WIND + 40) { *hx = MN.lx; *hy = MN.ly; }          // the plunge
  }
}

// The tentacles' poses and the tips' fire while it acts.
static void MnActTick(int talk) {
  const int t = ++MN.act_t;
  if (MN.act == MA_BLOOM) {
    // every tentacle flares out wide and glows, then each tip fires at the ship
    for (int i = 0; i < MN_TENT; i++) {
      Limb* L = &MN.tent[i];
      LimbToward(L, (DOWN + (2 * i - 5) * 0x1c0000) & (TURN - 1), 0x50000, 0, MN_SEG + FX(1.5), 0x18000);
      L->ph = (MN.t * 0x60000 + i * 0x2a0000) & (TURN - 1);
      L->state = t < BLOOM_T ? 1 : 0;
    }
    if (t == BLOOM_T && !talk)
      for (int i = 0; i < MN_TENT; i++) {
        const Limb* L = &MN.tent[i];
        // enraged, every other tip throws a ring of sparks instead of its fan
        const int cmd = MN.rage && (i & 1) ? C_SPARKS : C_STINGS;
        OrderAt(MN.tip[i], cmd, AbAngle(TipX(L), TipY(L), PlayerX(), PlayerY()));
      }
    if (t >= BLOOM_T + 50) MnActEnd();
  } else if (MN.act == MA_CURTAIN) {
    // gliding across, the tentacles trailing behind it, its tips raining stings
    const int d = MN.ax1 > MN.ax0 ? 1 : -1;
    for (int i = 0; i < MN_TENT; i++) {
      Limb* L = &MN.tent[i];
      LimbToward(L, (DOWN - d * 0x160000 + (2 * i - 5) * 0x50000) & (TURN - 1), 0x30000, d * 0xa000, MN_SEG,
                 0x70000);
      L->ph = (MN.t * 0x48000 + i * 0x2a0000) & (TURN - 1);
    }
    // a tip at a time, round the six (act_hit counts the beat, act_k the tip)
    if (!talk && t > 16 && t < CURTAIN_T - 20 && ++MN.act_hit >= (MN.rage ? 9 : 12)) {
      MN.act_hit = 0;
      if (++MN.act_k >= MN_TENT) MN.act_k = 0;
      if (!Busy(MN.tip[MN.act_k])) OrderAt(MN.tip[MN.act_k], C_STINGS, DOWN);
    }
    if (t >= CURTAIN_T) MnActEnd();
  } else if (MN.act == MA_DIVE) {
    // drawn up with its tentacles raised (the tell), then a plunge at where
    // the ship was, a ring burst point-blank at the bottom of it
    for (int i = 0; i < MN_TENT; i++) {
      Limb* L = &MN.tent[i];
      if (t < DIVE_WIND + 40) {
        LimbToward(L, (UP + (2 * i - 5) * 0x140000) & (TURN - 1), 0x60000, (2 * i - 5) * 0x8000, MN_SEG, 0x40000);
        L->state = 1;
      } else {
        MnRestPose(i);
      }
      L->ph = (MN.t * 0x50000 + i * 0x2a0000) & (TURN - 1);
    }
    if (t == DIVE_WIND) {
      s32 lx = PlayerX(), ly = PlayerY() - FX(70);
      if (lx < FX(70)) lx = FX(70);
      if (lx > FX(330)) lx = FX(330);
      if (ly < FX(140)) ly = FX(140);
      if (ly > FX(300)) ly = FX(300);
      MN.lx = lx;
      MN.ly = ly;
      if (!talk) OrderAt(MN.bell, C_PULSE, DOWN);                       // a ring as it launches
    }
    if (t > DIVE_WIND && t < DIVE_WIND + 40 && !MN.act_hit && !talk && AbParrying() &&
        AbDist(PlayerX() - MN.x, PlayerY() - MN.y) < DIVE_HIT) {
      // spun into: it reels, stunned, like a turned swing
      MN.act_hit = 1;
      AbSound(ABS_PARRY);
      AbBoom(MN.x, MN.y + FX(30), 0);
      MN.stun = STUN_T;
      Anim(MN.bell, 2);
      Guard(MN.bell, 0);
      SB_StrikeBadGuy(BH, MN.bell, SWING_DMG);
      if (!MN.said_parry++) AbLine(LB_MANE_PARRY);
      MnActEnd();
      return;
    }
    if (t == DIVE_WIND + 26 && !talk) {
      Place(MN.emit, MN.x, MN.y + FX(30));
      OrderAt(MN.emit, MN.rage ? C_SPARKS : C_PULSE, AbAngle(MN.x, MN.y, PlayerX(), PlayerY()));
    }
    if (t >= DIVE_WIND + 90) MnActEnd();
  }
}

static int Swinging(int k) {
  if (MN.sw == SW_NONE) return 0;
  return MN.sw_k[0] == k || (MN.sw_n == 2 && MN.sw_k[1] == k);
}

static void MnFight(void) {
  const int talk = InCutscene();
  if (!talk && MN.stun <= 0) Guard(MN.bell, 0);
  if (talk) Guard(MN.bell, 1);
  SwingTick(talk);
  if (MN.act != MA_NONE && MN.stun > 0) MnActEnd();
  if (MN.act != MA_NONE) MnActTick(talk);
  else
    for (int i = 0; i < MN_TENT; i++)
      if (!Swinging(i)) MnRestPose(i);
  if (MN.stun > 0) {
    if (--MN.stun == 0) Anim(MN.bell, MN.rage ? 1 : 0);
    return;
  }
  if (talk) return;
  BossWards();
  if (!MN.rage && Hp(MN.bell) * 2 < MN_HP) {
    MN.rage = 1;
    Anim(MN.bell, 1);
    AbLine(LB_MANE_RAGE);
  }
  if (MN.cool > 0) {
    MN.cool--;
    if (HardMode() && (MN.t & 3) == 0 && MN.cool > 0) MN.cool--;    // hard mode: a quarter sooner
    return;
  }
  if (MN.sw != SW_NONE || MN.act != MA_NONE || Busy(MN.bell) || Busy(MN.emit)) return;
  MN.round++;
  const int r = MN.round & 7;
  const int odd = (MN.round >> 3) & 1;           // every other round of eight brings its other moves
  if (r == 1 || r == 5) {
    // the swing: the tentacle on the side the ship isn't, so it sweeps across it
    const int k = PlayerX() < MN.x ? 0 : 5;
    if (MN.rage && r == 5) Swing(0, 5); else Swing(k, -1);
    // ... or, every other round, the whiplash: back across, and (enraged) again
    if (odd && r == 1) MN.chain = MN.rage ? 2 : 1;
    MN.cool = MN.rage ? 50 : 70;
    return;
  }
  if (r == 3) {
    if (odd) {
      MnAct(MA_BLOOM);
      MN.cool = MN.rage ? 50 : 70;
      return;
    }
    OrderAt(MN.bell, C_ROAR, DOWN);
    MN.cool = 150;
    return;
  }
  if (odd && r == 6) {
    MnAct(MA_CURTAIN);
    MN.cool = 40;
    return;
  }
  if (odd && r == 0) {
    MnAct(MA_DIVE);
    MN.cool = MN.rage ? 40 : 60;
    return;
  }
  if (r == 7 && MN.rage) {
    Attack(MN.bell, C_BROOD);
    if (!MN.said_brood++) AbLine(LB_MANE_BROOD);
    MN.cool = 70;
    return;
  }
  if (r == 2 || r == 6) {
    // stings from a tentacle's tip
    const Limb* L = &MN.tent[Rnd(0, MN_TENT)];
    Place(MN.emit, TipX(L), TipY(L));
    // hard mode: every other time, pincers from the tip
    OrderAt(MN.emit, HardMode() && r == 6 ? LC_XPINCER : C_STINGS, AbAngle(TipX(L), TipY(L), PlayerX(), PlayerY()));
    MN.cool = MN.rage ? 40 : 60;
    return;
  }
  if (r == 4 && MN.rage) {
    Place(MN.emit, MN.x, MN.y + FX(30));
    OrderAt(MN.emit, C_RAIN, DOWN);
    MN.cool = 70;
    return;
  }
  if (HardMode()) {
    // hard mode: snipe rings where it would rest; twist rings and drifting-gap
    // rings between its pulses and spirals
    static const unsigned char kBellH[4] = { LC_XTWIST, C_PULSE, LC_XGAP, C_SPIRAL };
    Attack(MN.bell, r == 4 ? LC_XSNIPE : kBellH[(MN.round >> 3) & 3]);
    MN.cool = MN.rage ? 44 : 60;
    return;
  }
  Attack(MN.bell, MN.rage ? C_SPIRAL : C_PULSE);
  MN.cool = MN.rage ? 50 : 70;
}

static void MnUpdate(u32 boss) {
  MN.t++;
  BlastTick();
  SpeechPump();
  // drifting across the top of the screen, pulsing; lunging when it swings
  s32 hx = FX(200) + SB_Sin((MN.t * 0x10000) & (TURN - 1)) * 70;
  s32 hy = MN_REST_Y + SB_Sin((MN.t * 0x24000) & (TURN - 1)) * 8;
  if (MN.lunge && MN.sw != SW_BACK) { hx = MN.lx; hy = MN.ly; }
  if (MN.act != MA_NONE) MnActTarget(&hx, &hy);
  const int fast = MN.lunge || (MN.act == MA_DIVE && MN.act_t >= DIVE_WIND && MN.act_t < DIVE_WIND + 40);
  switch (MN.phase) {
  case M_IN:
    MN.y += Ease(MN_REST_Y - MN.y, FX(1.2));
    for (int i = 0; i < MN_TENT; i++) MnRestPose(i);
    if (AbAbs(MN.y - MN_REST_Y) < FX(1) && AbQuiet() && MN.t > 150) {
      MN.phase = M_FIGHT;
      MN.t = 0;
      MN.cool = 40;
    }
    break;
  case M_FIGHT:
    if (!Alive(MN.bell)) {
      MN.phase = M_DEAD;
      MN.t = 0;
      MN.bell = 0;
      ShipShield(1);
      Blasts(MN.x, MN.y, 50, 40, 16);
      g_autopilot_target = 0;
      R32(boss, B_MAXHP) = 0;
      Remove(MN.emit);
      MN.emit = 0;
      for (int i = 0; i < MN_TENT; i++) { Remove(MN.tip[i]); MN.tip[i] = 0; }
      MN.act = MA_NONE;
      break;
    }
    if (MN.act == MA_DIVE && fast) {
      // the plunge: a quarter of the way a frame, up to ten units
      s32 dx = (hx - MN.x) >> 2, dy = (hy - MN.y) >> 2;
      if (dx > FX(10)) dx = FX(10);
      if (dx < -FX(10)) dx = -FX(10);
      if (dy > FX(10)) dy = FX(10);
      if (dy < -FX(10)) dy = -FX(10);
      MN.x += dx;
      MN.y += dy;
    } else {
      MN.x += Ease(hx - MN.x, FX(fast ? 2.6 : 0.9));
      MN.y += Ease(hy - MN.y, FX(fast ? 2.4 : 0.9));
    }
    MnFight();
    break;
  case M_DEAD:
    // the tentacles go limp and fall away, tips first
    for (int i = 0; i < MN_TENT; i++) {
      Limb* L = &MN.tent[i];
      if (!L->live) continue;
      LimbToward(L, DOWN, 0x20000, 0, MN_SEG, 0);
      if (MN.t == 30 + i * 8) {
        AbBoom(TipX(L), TipY(L), 0);
        LimbDrop(L);
      }
    }
    if (MN.t == 40) AbLineNow(LB_MANE_DOWN);
    if (MN.t > 140 && BW.left <= 0 && AbQuiet()) MN.phase = M_OVER;
    break;
  case M_OVER:
    R32(boss, B_STATE) = 0;
    return;
  }
  if (MN.phase < M_DEAD) MnPlaceAll();
  else
    for (int i = 0; i < MN_TENT; i++) {
      s32 rx, ry;
      MnRoot(i, &rx, &ry);
      LimbPose(&MN.tent[i], rx, ry);
      LimbPlace(&MN.tent[i]);
    }
}

static void MnDeath(u32 boss) {
  for (int i = 0; i < MN_TENT; i++) {
    LimbDrop(&MN.tent[i]);
    if (MN.tip[i]) { Remove(MN.tip[i]); MN.tip[i] = 0; }
  }
  ShipShield(0);
  g_autopilot_target = 0;
  KrakenBossInit(boss);
  SB_PausePartition(BH, 0);
}

static void MnInstall(u32 boss) {
  SB_ClearBytes((void*)boss, BOSS_SIZE);
  SB_ClearBytes(&MN, sizeof MN);
  R32(boss, B_SPAWN + 0x00) = (s32)(u32)MnSpawn;
  R32(boss, B_SPAWN + 0x04) = (s32)(u32)MnUpdate;
  R32(boss, B_SPAWN + 0x08) = (s32)(u32)MnRender;
  R32(boss, B_SPAWN + 0x0c) = (s32)(u32)MnRender;
  R32(boss, B_SPAWN + 0x10) = (s32)(u32)MnDeath;
  R32(boss, B_HEALTH) = (s32)(u32)MnHealth;
}

// ===================================================================================
// Specimen Eleven: the Kraken
// ===================================================================================
enum { K_IN = 1, K_GUARD, K_EYES, K_DEEP, K_RETURN, K_TRIDENT, K_FURY, K_DYING, K_ENDING, K_OVER };
enum { ARM_REST, ARM_LIFT, ARM_SLAM, ARM_LIE, ARM_BACK, ARM_REACH, ARM_LUNGE, ARM_HOLD, ARM_CARRY };
#define KR_ARMS   6
#define KR_SEG    FX(15)
#define KR_REST_Y FX(72)
#define MANTLE_HP 11000
#define CLUB_HP   2800
#define SIPHON_HP 2200
#define NMOTE     32             // the aura's ring
#define NMOTE_IN  16             // ... and an inner one, turning the other way
#define NWARN     12             // two lanes of six
#define CRACKS    6               // UNITY torpedoes to crack the mantle's plates
#define TORP_DMG  150
#define LIE_T     34
#define BACK_T_   44
#define HOLD_MAX  200
#define EYE_HP    2000            // lab_cast.py keye_l, keye_r
#define EYE_DX    56              // the eyes from the mantle's centre (art_lab.MANTLE_EYES)
#define EYE_DY    6
#define LURE_HP   1300            // lab_cast.py glantern
#define NLURE     3
#define NLINK     4
#define NLURECMD  5               // a lure lantern's attacks (Lures)
#define NELET     8               // the eyelets round an open eye
#define ELET_R    46
#define WALL_IN   FX(140)         // the deep's walls close to this far in from each side at most
#define SQUEEZE   FX(24)          // ... and lunge this much further when they squeeze
#define DEEP_Y    FX(-340)        // the Kraken up out of sight
#define NINE_Y    FX(22)          // Specimen Nine's hull, bow down (her lure 118 below it)
enum { ES_SHUT, ES_OPENING, ES_OPEN, ES_CLOSING, ES_BLIND };             // an eye
enum { EV_SHUT, EV_OPENING, EV_OPEN, EV_GLARE, EV_STRUCK, EV_BLIND };     // its picture (lab_cast keye_vis)
enum { LV_GLOW, LV_FIRE, LV_STRUCK, LV_WRECK };                           // a lure lantern's (nine_lure)

// its guns, after the Kazakhstan boss's (lab/art_lab.py MANTLE_TURRET,
// MANTLE_MORTAR, MANTLE_GILLS; from the mantle's centre): two shoulder
// turrets that track the ship and fire hooking bolts, two mortars on its
// back lobbing rings of fire curls; and, once the boats come, two gill
// banks that open on its flanks and fan bolts down the screen. The guns can
// be shot out (anything hurts them); a wreck burns.
#define NGUNS 4
static const short kGunDX[NGUNS] = { 78, -78, 34, -34 };
static const short kGunDY[NGUNS] = { 22, 22, -48, -48 };
#define GILL_DX 46
#define GILL_DY 40
#define FIN_DX  50
#define FIN_DY  (-76)
enum { GUN_IDLE, GUN_FIRE, GUN_WRECK };

// where each arm leaves the head (degrees: 25 50 75 105 130 155 from the right, round the front),
// how many segments it has, and where its tip hangs at rest: the outer two
// spread wide, the club arms (1 and 4) hang down either side of the beak,
// the inner two coil up the flanks -- the middle of the screen stays open
static const s32 kArmA[KR_ARMS] = { 0x11c71c, 0x238e38, 0x355555, 0x4aaaaa, 0x5c71c7, 0x6e38e3 };
static const unsigned char kArmN[KR_ARMS] = { 9, 6, 8, 8, 6, 9 };
static const short kRestX[KR_ARMS] = { 352, 268, 312, 88, 132, 48 };
static const short kRestY[KR_ARMS] = { 196, 238, 150, 150, 238, 196 };
static const s32 kRecip[10] = { 0, 65536, 32768, 21845, 16384, 13107, 10923, 9362, 8192, 7282 };   // 65536 / n

static struct {
  u32 boss, mantle, club[2], siphon;
  u32 armour;                       // the real mantle, parked off screen while its stand-in shows (eyes, deep;
                                    // above -512 or it wraps round onto the bottom of the screen)
  Limb arm[KR_ARMS];
  u32 mote[NMOTE + NMOTE_IN], warn[NWARN], drain;
  int phase, t, cool, round, cracks, cracked, fury, crushed, crush_sub;
  // taking the boat plays out with nobody talking over it: Volta's line comes
  // lost_t frames after the blast, and the Doc's on the cracked plates waits for it
  int lost_t, crack_owed;
  int beep_arm;                     // the arm whose slam the beep is for (Slam)
  int arm_cool, arm_turn;           // the arms' own clock (Arms)
  int act[KR_ARMS], act_t[KR_ARMS];
  s32 lane[KR_ARMS];
  int grab, grab_t;
  int said_aura, said_grab, volley_ticket, volley, bleed;
  int breath;
  int talk_ticket, trident_t;
  s32 x, y, ar, ax, ay;
  u32 gun[NGUNS], gvis[NGUNS], fire[NGUNS];
  s32 gang[NGUNS];
  int gcool[NGUNS], gflash[NGUNS];
  u32 gill[2], gemit[2], fin[2];
  int gill_state, gill_t;
  u32 brain;                        // its brain at work under the glass (glow.h)
  // the aura's flare: 1 drawing in, 2 blazing out; the countdown to the next
  int flare, flare_t, flare_next, mote_hot, said_flare;
  // the eyes: the parts, their pictures and pupils, each one's state and clock
  u32 eye[2], evis[2], pupil[2];
  int es[2], et[2], eflash[2], ehp[2], eturn, ecycle, blind, eshut_t;
  // the deep: how far in the walls are (from each side), its goal, the squeeze;
  // Specimen Nine, her lanterns (parts, pictures, tethers), what each fires next
  s32 wall, wall_goal, sq, wall_u;
  int sq_t, sq_state;
  u32 nine, lure[NLURE], lvis[NLURE], link[NLURE][NLINK];
  s32 nx, ny;
  int lcool[NLURE], lturn[NLURE], lflash[NLURE], lhp[NLURE], lwreck[NLURE], lures_left, deep_t, nine_gone;
  int eshown[2], ecool[2], lshown[NLURE];   // the frame each picture shows (set only when it changes)
  // the eyelets: a ring of little eyes round the open eye -- its radius and
  // turn, and its volley (1 the cascade, 2 the bloom), its clock and its next
  u32 elet[NELET];
  s32 elet_r, elet_a;
  int ering, ering_t, ering_k, ering_next, elet_hot;
  int gill_show;                    // before the boats, the gill banks open only to fire (the net, the wipers)
} KR;

s32 g_lab_dbg[8];                 // the probes read it: phase, aura radius, inside, shield ...

s32 LabKrakenX(void) { return KR.x; }
s32 LabKrakenY(void) { return KR.y + FX(40); }

static s32 KrHealth(u32 boss) {
  (void)boss;
  if (KR.phase == K_GUARD) return Hp(KR.club[0]) + Hp(KR.club[1]) + Hp(KR.siphon);
  if (KR.phase == K_EYES) return Hp(KR.eye[0]) + Hp(KR.eye[1]);
  if (KR.phase == K_DEEP) {
    s32 h = 0;
    for (int k = 0; k < NLURE; k++) h += Hp(KR.lure[k]);
    return h;
  }
  if (KR.phase >= K_RETURN && KR.phase <= K_FURY) return Hp(KR.mantle);
  return 0;
}
static void KrRender(u32 boss, s32 x, s32 y) { (void)boss; (void)x; (void)y; }

static void KrRoot(int k, s32* x, s32* y) {
  *x = KR.x + SB_Cos(kArmA[k]) * 86;
  *y = KR.y + FX(44) + SB_Sin(kArmA[k]) * 38;
}

static void KrRest(int k) {
  Limb* L = &KR.arm[k];
  // toward its resting place, curling a little, waving slowly
  s32 rx, ry;
  KrRoot(k, &rx, &ry);
  const s32 want = AbAngle(rx, ry, KR.x - FX(200) + FX(kRestX[k]), KR.y - KR_REST_Y + FX(kRestY[k]));
  const s32 curl = (k == 2 ? -0x28000 : (k == 3 ? 0x28000 : 0));
  LimbToward(L, want - curl * 4, 0x40000, curl, KR_SEG, 0x100000);
  L->ph = (KR.t * 0x2a000 + k * 0x2b0000) & (TURN - 1);
  L->kph = 0x160000;
  L->state = 0;
}

static void KrSpawn(u32 boss) {
  KR.boss = boss;
  KR.x = FX(200);
  KR.y = FX(-180);
  KR.mantle = SB_SpawnBadGuy(BH, "*lab/boss/mantle", KR.x, KR.y);
  KR.brain = GlowSpawn(BH, "*lab/boss/kraken_brain");          // (before the arms: they reach over it)
  for (int s = 0; s < 2; s++) KR.club[s] = SB_SpawnBadGuy(BH, "*lab/boss/club", KR.x, KR.y);
  KR.siphon = SB_SpawnBadGuy(BH, "*lab/boss/siphon", KR.x, KR.y);
  for (int k = 0; k < KR_ARMS; k++) {
    Limb* L = &KR.arm[k];
    L->base = kArmA[k];
    L->len = KR_SEG;
    LimbSpawn(L, "*lab/boss/kseg", kArmN[k], 4, KR.x, KR.y);
  }
  // its guns, gill banks and fins (generics: spawned after the limbs, over them)
  for (int g = 0; g < NGUNS; g++) {
    KR.gun[g] = SB_SpawnBadGuy(BH, "*lab/boss/kgun", KR.x, KR.y);
    Guard(KR.gun[g], 1);
    KR.gvis[g] = AbSpawn(g < 2 ? "*lab/boss/kturret" : "*lab/boss/kmortar", KR.x, KR.y);
    KR.gang[g] = DOWN;
    KR.gcool[g] = 120 + 45 * g;
  }
  for (int s = 0; s < 2; s++) {
    KR.gill[s] = AbSpawn("*lab/boss/kgills", KR.x, KR.y);
    KR.gemit[s] = SB_SpawnBadGuy(BH, "*lab/boss/emitter", KR.x, KR.y);
    KR.fin[s] = AbSpawn(s ? "*lab/boss/kfin_r" : "*lab/boss/kfin", KR.x, KR.y);
  }
  Guard(KR.mantle, 1);
  Guard(KR.club[0], 1);
  Guard(KR.club[1], 1);
  Guard(KR.siphon, 1);
  KR.phase = K_IN;
  KR.t = 0;
  KR.ar = FX(40);
  KR.flare_next = 420;
  R32(boss, B_STATE) = 1;
  SB_PausePartition(BH, 1);
  R32(boss, B_MAXHP) = CLUB_HP * 2 + SIPHON_HP;
  g_autopilot_target = KR.club[0];
  AbLine(LB_KRAKEN_IN);             // Marrow: my masterpiece will hold you off
  AbLine(LB_SHOGUN_WAIT);           // the Shogun: every code
  AbLine(LB_MARROW_YES);
  AbLine(LB_KRAKEN_DOC);
}

// ---- the deep's walls: the arms come down both sides of the screen ---------------------------
// Arms 0, 2, 1 make the right wall, 5, 3, 4 the left: end to end down the
// screen, from above its top, so each side is one coiling column (shown at
// their two biggest sizes), waving. (The camera shows about 200 units of
// the 400 across, panning half as far as the ship: a wall shows as the ship
// nears it, and once it's in past 100 or so.)
static const signed char kWallSide[KR_ARMS] = { 1, 1, 1, 0, 0, 0 };
static const short kWallTop[KR_ARMS] = { -30, 340, 168, 168, 340, -30 };
static const short kWallBot[KR_ARMS] = { 168, 480, 340, 340, 480, 168 };

static s32 WallL(void) { return KR.wall + KR.sq; }               // the inner edges
static s32 WallR(void) { return FX(400) - KR.wall - KR.sq; }

static void WallJoint(int k, int j, s32* x, s32* y) {
  const s32 top = FX(kWallTop[k]), bot = FX(kWallBot[k]);
  const s32 yy = top + SB_MulFP(bot - top, j * kRecip[KR.arm[k].n]);
  const s32 wave = SB_Sin((KR.t * 0x30000 + (yy >> 16) * 0x40000 + (kWallSide[k] ? 0x800000 : 0)) & (TURN - 1)) * 7;
  *y = yy;
  *x = (kWallSide[k] ? WallR() + FX(9) : WallL() - FX(9)) + wave;
}

static void KrPlace(void) {
  // its brain at work, as the mantle is: armoured, charging, cracked, in fury
  static const signed char kBrain[4] = { 0, 1, 2, 3 };
  if (Alive(KR.mantle)) Place(KR.mantle, KR.x, KR.y);
  if (Alive(KR.armour)) { Place(KR.armour, FX(200), FX(-330)); Guard(KR.armour, 1); }
  GlowPut(KR.brain, KR.x, KR.y, KRAKEN_BRAIN_X, KRAKEN_BRAIN_Y,
          Alive(KR.mantle) ? GlowFor(Act(KR.mantle), kBrain, 4) : GLOW_OFF);
  for (int k = 0; k < KR_ARMS; k++) {
    Limb* L = &KR.arm[k];
    s32 rx, ry;
    KrRoot(k, &rx, &ry);
    LimbPose(L, rx, ry);
    L->big = KR.wall_u > FX(0.5);
    if (KR.wall_u > 0)                           // (blended toward the walls)
      for (int j = 1; j <= L->n; j++) {
        s32 wx, wy;
        WallJoint(k, j, &wx, &wy);
        L->jx[j] += SB_MulFP(wx - L->jx[j], KR.wall_u);
        L->jy[j] += SB_MulFP(wy - L->jy[j], KR.wall_u);
      }
    LimbPlace(L);
  }
  // the eyes on their sockets, the pupils set a little toward the ship
  for (int e = 0; e < 2; e++) {
    const s32 ex = KR.x + (e ? FX(EYE_DX) : FX(-EYE_DX)), ey = KR.y + FX(EYE_DY);
    if (Alive(KR.eye[e])) Place(KR.eye[e], ex, ey);
    AbPut(KR.evis[e], ex, ey);
    if (KR.pupil[e]) {
      const s32 a = AbAngle(ex, ey, PlayerX(), PlayerY());
      const int open = KR.es[e] == ES_OPEN;
      AbPut(KR.pupil[e], open ? ex + SB_Cos(a) * 4 : FX(-200), open ? ey + SB_Sin(a) * 3 : FX(-200));
    }
  }
  // the clubs ride the tips of arms 1 and 4, out past the last segment
  // (generics draw over badguys: under it, a club couldn't be seen); the
  // siphon under the beak
  for (int s = 0; s < 2; s++) {
    if (!Alive(KR.club[s])) continue;
    const Limb* L = &KR.arm[s ? 4 : 1];
    const s32 a = AbAngle(L->jx[L->n - 1], L->jy[L->n - 1], TipX(L), TipY(L));
    Place(KR.club[s], TipX(L) + SB_Cos(a) * 20, TipY(L) + SB_Sin(a) * 20);
  }
  if (Alive(KR.siphon)) Place(KR.siphon, KR.x, KR.y + FX(84));
  for (int g = 0; g < NGUNS; g++) {
    const s32 gx = KR.x + FX(kGunDX[g]), gy = KR.y + FX(kGunDY[g]);
    if (Alive(KR.gun[g])) Place(KR.gun[g], gx, gy);
    AbPut(KR.gvis[g], gx, gy);
    if (g < 2 && KR.gvis[g]) SB_SetActorOrient(KR.gvis[g], KR.gang[g]);
    AbPut(KR.fire[g], gx, gy - FX(8));
  }
  for (int s = 0; s < 2; s++) {
    const s32 sx = s ? FX(1) : FX(-1);
    AbPut(KR.gill[s], KR.x + sx * GILL_DX, KR.y + FX(GILL_DY));
    if (Alive(KR.gemit[s])) Place(KR.gemit[s], KR.x + sx * GILL_DX, KR.y + FX(GILL_DY + 8));
    AbPut(KR.fin[s], KR.x + sx * FIN_DX + SB_Sin((KR.t * 0x30000 + s * 0x800000) & (TURN - 1)) * 2,
          KR.y + FX(FIN_DY));
  }
}

static void KrPlaceParts(void) {
  for (int g = 0; g < NGUNS; g++) {
    AbPut(KR.gvis[g], KR.x + FX(kGunDX[g]), KR.y + FX(kGunDY[g]));
    AbPut(KR.fire[g], KR.x + FX(kGunDX[g]), KR.y + FX(kGunDY[g] - 8));
  }
}

// The guns: the turrets turn after the ship and fire hooks; the mortars lob
// curls; shot out, a gun is a burning wreck.
static void Guns(int talk) {
  const int p = KR.phase;
  if (p == K_DEEP || p == K_RETURN) talk = 1;       // (up out of sight)
  for (int g = 0; g < NGUNS; g++) {
    if (!KR.gvis[g]) continue;
    const s32 gx = KR.x + FX(kGunDX[g]), gy = KR.y + FX(kGunDY[g]);
    if (KR.gun[g] && !Alive(KR.gun[g])) {
      AbBoom(gx, gy, 0);
      SB_ShakeWorld();
      KR.gun[g] = 0;
      PartFrame(KR.gvis[g], GUN_WRECK);
      KR.fire[g] = AbSpawn("*lab/boss/flame", gx, gy - FX(8));
    }
    if (!KR.gun[g]) continue;
    if (Alive(KR.gun[g])) Guard(KR.gun[g], talk);
    if (g < 2) KR.gang[g] = AbTurnTo(KR.gang[g], AbAngle(gx, gy, PlayerX(), PlayerY()), 0x30000);
    if (KR.gflash[g] > 0 && --KR.gflash[g] == 0) PartFrame(KR.gvis[g], GUN_IDLE);
    if (talk || Busy(KR.gun[g])) continue;
    if (--KR.gcool[g] > 0) continue;
    // hard mode: every other volley, the turrets close pincers and the mortars
    // let go drifting-gap rings; all a quarter sooner
    static unsigned char hard_turn[4];
    const int alt = HardMode() && (hard_turn[g]++ & 1);
    if (g < 2) {
      OrderAt(KR.gun[g], alt ? LC_XPINCER : C_HOOKS, KR.gang[g]);
      KR.gcool[g] = p <= K_EYES ? 210 : (p == K_TRIDENT ? 160 : 115);
    } else {
      OrderAt(KR.gun[g], alt ? LC_XGAP : C_CURLS, DOWN);
      KR.gcool[g] = p <= K_EYES ? 280 : (p == K_TRIDENT ? 220 : 170);
    }
    if (HardMode()) KR.gcool[g] -= KR.gcool[g] >> 2;
    PartFrame(KR.gvis[g], GUN_FIRE);
    KR.gflash[g] = 18;
  }
}

// The gill banks: before the boats, open only while they fire the net or
// the wipers (gill_show); once the boats are in, on their own: shut, then
// open, then firing fans.
static void Gills(int talk) {
  if (KR.phase < K_TRIDENT) {
    const int f = KR.gill_show > 0 ? 2 : 0;
    if (KR.gill_show > 0 && !talk) KR.gill_show--;
    if (f != KR.gill_state)
      for (int s = 0; s < 2; s++) PartFrame(KR.gill[s], f);
    KR.gill_state = f;
    return;
  }
  if (talk) return;
  if (KR.gill_show > 0) KR.gill_show--;
  KR.gill_t++;
  if (KR.gill_state == 0 && KR.gill_t > (KR.phase == K_FURY ? 170 : 250)) {
    KR.gill_state = 1;
    KR.gill_t = 0;
    for (int s = 0; s < 2; s++) PartFrame(KR.gill[s], 1);
  } else if (KR.gill_state == 1 && KR.gill_t > 30) {
    KR.gill_state = 2;
    KR.gill_t = 0;
    for (int s = 0; s < 2; s++) {
      PartFrame(KR.gill[s], 2);
      if (Alive(KR.gemit[s]) && !Busy(KR.gemit[s])) OrderAt(KR.gemit[s], C_FANS, DOWN);
    }
    SB_ShakeWorld();
  } else if (KR.gill_state == 2 && KR.gill_t > 70) {
    KR.gill_state = 0;
    KR.gill_t = 0;
    for (int s = 0; s < 2; s++) PartFrame(KR.gill[s], 0);
  }
}

// ---- the aura -------------------------------------------------------------------------------------
// Its flare, now and then while the aura is out: it draws in (the motes
// hot, a charging sound), then blazes out far wider for a second or so,
// drinking three and a half times as fast.
#define FLARE_IN   40
#define FLARE_OUT  80
static void Flare(int on, int talk) {
  if (!on || talk) { KR.flare = 0; return; }       // (someone talking: it dies down)
  if (KR.flare == 0) {
    if (--KR.flare_next > 0) return;
    KR.flare = 1;
    KR.flare_t = 0;
    AbSound(ABS_ABSORB);
    return;
  }
  ++KR.flare_t;
  // drawing in: the Entropy Engine's absorb, over and over, louder than the game's
  if (KR.flare == 1 && (KR.flare_t == 10 || KR.flare_t == 20 || KR.flare_t == 30)) AbSound(ABS_ABSORB);
  if (KR.flare == 1 && KR.flare_t >= FLARE_IN) {
    KR.flare = 2;
    KR.flare_t = 0;
    SB_ShakeWorld();
    if (KR.said_aura && KR.act[5] != ARM_CARRY && !KR.said_flare++) AbLine(LB_FLARE);
  } else if (KR.flare == 2 && KR.flare_t >= FLARE_OUT) {
    KR.flare = 0;
    KR.flare_next = KR.phase == K_FURY ? Rnd(300, 420) : Rnd(380, 520);
    if (HardMode()) KR.flare_next -= KR.flare_next >> 2;
  }
}

static void Aura(int talk) {
  const int p = KR.phase;
  const int on = (p == K_GUARD && Alive(KR.siphon)) || p == K_EYES || p == K_TRIDENT || p == K_FURY;
  // where it breathes from: the siphon, or once that's gone, the beak
  KR.ax = KR.x;
  KR.ay = KR.y + FX(Alive(KR.siphon) ? 86 : 60);
  s32 lo = FX(80), hi = FX(150), w = 0xb000;
  if (p == K_EYES) { lo = FX(95); hi = FX(180); w = 0xc000; }
  if (p == K_TRIDENT) { lo = FX(105); hi = FX(205); w = 0xd000; }
  if (p == K_FURY) { lo = FX(135); hi = FX(250); w = 0x11000; }
  Flare(on, talk);
  s32 want = on ? lo + SB_MulFP(hi - lo, FX(0.5) + (SB_Sin((KR.t * w) & (TURN - 1)) >> 1)) : 0;
  s32 ease = FX(2);
  if (KR.flare == 1) { want = SB_MulFP(lo, FX(0.6)); ease = FX(3); }          // drawing in
  if (KR.flare == 2) {                                                         // blazing out
    want = SB_MulFP(hi, FX(1.7));
    if (want > FX(330)) want = FX(330);
    ease = FX(9);
  }
  KR.ar += Ease(want - KR.ar, ease);
  const int show = KR.ar > FX(12);
  const int hot = KR.flare != 0;
  for (int k = 0; k < NMOTE + NMOTE_IN; k++) {
    if (!show) { AbPut(KR.mote[k], FX(-200), FX(-200)); continue; }
    if (!KR.mote[k]) { KR.mote[k] = AbSpawn("*lab/boss/aura", KR.ax, KR.ay); if (hot) AbFrame(KR.mote[k], 1); }
    if (hot != KR.mote_hot) AbFrame(KR.mote[k], hot);
    s32 a, r;
    if (k < NMOTE) {                                                   // 2^24 / 32 apart, turning
      a = (k * 0x80000 + KR.t * 0x9000) & (TURN - 1);
      r = KR.ar + SB_Sin((KR.t * 0x40000 + k * 0x300000) & (TURN - 1)) * 5;
    } else {                                                           // 2^24 / 16, the other way
      a = ((k - NMOTE) * 0x100000 - KR.t * 0xc000) & (TURN - 1);
      r = SB_MulFP(KR.ar, FX(0.62));
    }
    AbPut(KR.mote[k], KR.ax + SB_MulFP(SB_Cos(a), r), KR.ay + SB_MulFP(SB_Sin(a), r));
  }
  if (show) KR.mote_hot = hot;
  // inside it, the shield drains -- and the Kraken drinks it
  const s32 sx = PlayerX(), sy = PlayerY();
  const int held = KR.grab >= 0;
  const int inside = show && !talk && !LabShipDown() && !AbParrying()
                     && (held || AbDist(sx - KR.ax, sy - KR.ay) < (KR.ar >> 16));
  g_lab_dbg[0] = KR.phase;
  g_lab_dbg[1] = KR.ar >> 16;
  g_lab_dbg[2] = inside;
  g_lab_dbg[3] = LabShield();
  g_lab_dbg[4] = AbDist(sx - KR.ax, sy - KR.ay);
  g_lab_dbg[5] = KR.grab;
  g_lab_dbg[6] = KR.cracks;
  g_lab_dbg[7] = KR.act[5] | KR.lost_t << 8 | KR.arm_cool << 16;
  if (inside) {
    if (!KR.drain) KR.drain = AbSpawn("*lab/boss/drain", sx, sy);
    AbPut(KR.drain, sx, sy);
    // (half as fast again as it was; three and a half times that in a flare)
    s32 rate = (p == K_GUARD ? 0x6c : (p == K_EYES ? 0x84 : (p == K_TRIDENT ? 0x90 : 0xb4))) * (held ? 2 : 1);
    if (KR.flare == 2) rate = (rate * 7) >> 1;
    LabDrainShield(rate);
    if (p == K_GUARD) Heal(Alive(KR.siphon) ? KR.siphon : (Alive(KR.club[0]) ? KR.club[0] : KR.club[1]), 2,
                           Alive(KR.siphon) ? SIPHON_HP : CLUB_HP);
    else if (p == K_EYES) Heal(Alive(KR.eye[KR.eturn]) ? KR.eye[KR.eturn] : KR.eye[KR.eturn ^ 1], 2, EYE_HP);
    else if (KR.cracked) Heal(KR.mantle, 2, MANTLE_HP);
    if (!KR.said_aura++) AbLine(LB_AURA);
    // the shield gone, it bleeds the ship
    if (LabShield() == 0 && ++KR.bleed >= 100) {
      KR.bleed = 0;
      LabHitShip(0);
    }
  } else {
    if (KR.drain) AbPut(KR.drain, FX(-200), FX(-200));
    KR.bleed = 0;
  }
}

// ---- the arms --------------------------------------------------------------------------------------
// A lane's warning: six markers down the screen where the arm will fall
// (the inner arms' lane is set 0, the club arms' set 1).
static int WarnSet(int k) { return (k == 2 || k == 3) ? 0 : 1; }
// The arm comes down as a straight line from where it leaves the head to
// its lane at the bottom of the screen, so the warnings lie along that
// line -- not straight down the lane, which the arm only meets at the
// bottom -- turned along it, and follow it as the head sways.
static void Warn(int set, s32 x, int on) {           // (on: 0 -- put them away)
  (void)x;
  for (int j = 0; j < NWARN / 2; j++) {
    u32* w = &KR.warn[set * (NWARN / 2) + j];
    if (!*w) *w = AbSpawn("*lab/boss/warn", FX(-200), FX(-200));
    if (!on) AbPut(*w, FX(-200), FX(-200));
  }
}
static void WarnArm(int k) {
  s32 rx, ry;
  KrRoot(k, &rx, &ry);
  const s32 tx = KR.lane[k], ty = FX(412);
  const s32 a = AbAngle(rx, ry, tx, ty);
  for (int j = 0; j < NWARN / 2; j++) {
    u32* w = &KR.warn[WarnSet(k) * (NWARN / 2) + j];
    if (!*w) *w = AbSpawn("*lab/boss/warn", FX(-200), FX(-200));
    const s32 f = FX(0.32) + j * 8913;                 // a third of the way down to the end (0.136 apart)
    AbPut(*w, rx + SB_MulFP(tx - rx, f), ry + SB_MulFP(ty - ry, f));
    SB_SetActorOrient(*w, a);
  }
}

// The charge beep warns of the arms and nothing else in this fight: one beep a
// slam (a pair in fury beeps once), and in fury it keeps beeping, quicker,
// all through the wind-up (ArmTick).
static void Slam(int k, s32 x, int beep) {
  if (KR.act[k] != ARM_REST) return;
  KR.act[k] = ARM_LIFT;
  KR.act_t[k] = 0;
  if (x < FX(30)) x = FX(30);
  if (x > FX(370)) x = FX(370);
  KR.lane[k] = x;
  KR.arm[k].state = 1;
  WarnArm(k);
  if (beep) {
    AbSound(ABS_CHARGE);
    KR.beep_arm = k;
  }
}

static void Grab(int k) {
  if (KR.act[k] != ARM_REST || KR.grab >= 0) return;
  KR.act[k] = ARM_REACH;
  KR.act_t[k] = 0;
  KR.arm[k].state = 1;
}

// What an arm that a parry turned away hurts: its club, or the mantle once cracked.
static void Jolt(int k) {
  const u32 c = k == 1 ? KR.club[0] : (k == 4 ? KR.club[1] : 0);
  if (Alive(c)) SB_StrikeBadGuy(BH, c, 380);
  else if (KR.cracked) SB_StrikeBadGuy(BH, KR.mantle, 380);
}

static s32 ToShip(int k) {
  s32 rx, ry;
  KrRoot(k, &rx, &ry);
  return AbAngle(rx, ry, PlayerX(), PlayerY());
}

// The arms' wind-ups ramp up phase by phase: slow and readable at first, quick in fury
static int LiftT(void) {
  const int p = KR.phase;
  return p == K_GUARD ? 78 : (p == K_EYES ? 66 : (p == K_TRIDENT ? 54 : 46));
}
static int ReachT(void) {
  const int p = KR.phase;
  return p <= K_EYES ? 56 : (p == K_TRIDENT ? 44 : 36);
}

static void ArmTick(int k, int talk) {
  Limb* L = &KR.arm[k];
  const int t = ++KR.act_t[k];
  s32 rx, ry;
  KrRoot(k, &rx, &ry);
  switch (KR.act[k]) {
  case ARM_REST:
    KrRest(k);
    break;
  case ARM_LIFT: {
    // rearing back over the head, red
    LimbToward(L, kArmA[k] + (k < 3 ? -0x900000 : 0x900000), 0x60000, k < 3 ? 0x40000 : -0x40000, FX(13), 0x40000);
    L->ph = (KR.t * 0x60000) & (TURN - 1);
    if (KR.phase == K_FURY && k == KR.beep_arm && (t == 12 || t == 24 || t == 34 || t == 42))
      AbSound(ABS_CHARGE);                     // fury: the warning comes thick and fast
    WarnArm(k);                                  // (the head sways: they follow it)
    if (t >= LiftT()) { KR.act[k] = ARM_SLAM; KR.act_t[k] = 0; }
    break;
  }
  case ARM_SLAM:
  case ARM_LIE: {
    // straight down the lane, to the bottom of the screen
    const s32 want = AbAngle(rx, ry, KR.lane[k], FX(412));
    const s32 d = AbDist(KR.lane[k] - rx, FX(412) - ry);
    L->base = KR.act[k] == ARM_SLAM ? AbTurnTo(L->base, want, 0x1a0000) : want;
    L->curl = 0;
    L->amp = 0;
    L->len = SB_MulFP(FX(d), kRecip[L->n]);                               // shared out
    if (KR.act[k] == ARM_SLAM && t == 6) {
      Warn(WarnSet(k), 0, 0);
      LimbPose(L, rx, ry);
      AbBoom(TipX(L), FX(392), 0);
      SB_ShakeWorld();
    }
    if (!talk && !LabShipDown() && t > 3 && LimbNear(L, 2, PlayerX(), PlayerY(), 16)) {
      if (AbParrying()) {
        AbSound(ABS_PARRY);
        AbBoom(PlayerX(), PlayerY() - FX(10), 0);
        Jolt(k);
        KR.act[k] = ARM_BACK;
        KR.act_t[k] = 0;
        L->state = 0;
        break;
      }
      LabHitShip(1);
    }
    if (KR.act[k] == ARM_SLAM && t >= 8) { KR.act[k] = ARM_LIE; KR.act_t[k] = 0; }
    else if (KR.act[k] == ARM_LIE && t >= LIE_T) { KR.act[k] = ARM_BACK; KR.act_t[k] = 0; L->state = 0; }
    break;
  }
  case ARM_BACK:
    KrRest(k);
    L->len += Ease(KR_SEG - L->len, FX(1.5));
    if (t >= BACK_T_) KR.act[k] = ARM_REST;
    break;
  case ARM_REACH:
    // coiling back, violet, aimed at the ship
    LimbToward(L, ToShip(k), 0x50000, k < 3 ? -0x30000 : 0x30000, FX(12), 0x30000);
    L->ph = (KR.t * 0x70000) & (TURN - 1);
    if (t >= ReachT()) { KR.act[k] = ARM_LUNGE; KR.act_t[k] = 0; }
    break;
  case ARM_LUNGE:
    // shooting out at it
    LimbToward(L, ToShip(k), 0x80000, 0, FX(30), 0);
    L->len += FX(1.4);
    LimbPose(L, rx, ry);
    if (!talk && !LabShipDown() && AbDist(TipX(L) - PlayerX(), TipY(L) - PlayerY()) < 20) {
      if (AbParrying()) {
        AbSound(ABS_PARRY);
        Jolt(k);
        KR.act[k] = ARM_BACK;
        KR.act_t[k] = 0;
        L->state = 0;
        break;
      }
      KR.act[k] = ARM_HOLD;
      KR.act_t[k] = 0;
      KR.grab = k;
      SB_ShakeWorld();
      if (!KR.said_grab++) AbLineNow(LB_GRAB);
      break;
    }
    if (t >= 26 || L->len > FX(30)) { KR.act[k] = ARM_BACK; KR.act_t[k] = 0; L->state = 0; }
    break;
  case ARM_HOLD: {
    // reeling the ship in toward the beak, drinking (the aura doubles while it holds)
    LimbToward(L, ToShip(k), 0x80000, 0, FX(9), 0);
    LimbPose(L, rx, ry);
    AbPull(TipX(L), TipY(L), FX(3.0));
    const s32 apart = AbDist(TipX(L) - PlayerX(), TipY(L) - PlayerY());
    int free_ = 0;
    if (AbParrying()) {                           // spun free
      AbSound(ABS_PARRY);
      AbBoom(TipX(L), TipY(L), 0);
      Jolt(k);
      free_ = 1;
    } else if (apart > 56 || LabShipDown()) {     // torn free
      free_ = 1;
    } else if (t >= HOLD_MAX) {                   // crushed
      LabHitShip(1);
      free_ = 1;
    }
    if (free_) {
      KR.grab = -1;
      KR.act[k] = ARM_BACK;
      KR.act_t[k] = 0;
      L->state = 0;
    }
    break;
  }
  case ARM_CARRY: {
    // the arm takes a UNITY boat: out to it, round it, up and crushed
    s32 bx, by;
    LabSubAt(KR.crush_sub, &bx, &by);
    if (t < 60) {
      LimbToward(L, AbAngle(rx, ry, bx, by), 0x60000, 0, SB_MulFP(FX(AbDist(bx - rx, by - ry)), kRecip[L->n]), 0);
      L->state = 1;
    } else {
      LimbToward(L, kArmA[k] + 0x100000, 0x20000, 0x30000, FX(12), 0);
      LimbPose(L, rx, ry);
      LabSubHold(KR.crush_sub, TipX(L), TipY(L) + FX(20));
      if (t == 150) {
        LabSubKill(KR.crush_sub);
        SB_ShakeWorld();
        KR.lost_t = 30;                           // Volta's line a beat after the blast (KrUpdate)
        KR.act[k] = ARM_BACK;
        KR.act_t[k] = 0;
        L->state = 0;
      }
    }
    break;
  }
  }
}

// ---- the fight ------------------------------------------------------------------------------------
static int ArmFree(int k) { return KR.act[k] == ARM_REST; }
// taking Trident Three, up to Volta's line about it
static int Carrying(void) { return KR.act[5] == ARM_CARRY || KR.lost_t > 0; }

// A slam: an inner arm down the ship's lane; `two`, a club arm beside it too,
// so the gap between them is the way out
static void SlamAt(int two) {
  const int k = (KR.round & 8) ? 2 : 3;
  const s32 x = PlayerX();
  const int beeped = ArmFree(k);
  if (beeped) Slam(k, x, 1);
  if (two) {
    const int k2 = k == 2 ? 4 : 1;
    if (ArmFree(k2)) Slam(k2, x + (x < FX(200) ? FX(110) : FX(-110)), !beeped);
  }
}

// The gill banks' lanes (they open to fire): the crossing net, or the wipers
static void GillLanes(int wipers) {
  for (int s = 0; s < 2; s++)
    if (Alive(KR.gemit[s]) && !Busy(KR.gemit[s]))
      OrderAt(KR.gemit[s], wipers ? (s ? C_WIPER : C_WIPEL) : (s ? C_NETR : C_NETL), DOWN);
  KR.gill_show = wipers ? 150 : 100;
}

// What its mantle and gills do, a round at a time, eight rounds to a cycle,
// phase by phase -- after the originals (docs/BOSS_ATTACKS_M2_M4.md): a layer
// that decides where the ship can be (the net's fixed lanes, the wipers, the
// snake onto the bottom centre) paired with one that makes it move (aimed
// fans, mines), a quiet beat after the big ones. The arms keep their own
// time (Arms), over the top of it all.
enum { A_NONE, A_SLAM, A_SLAM2, A_SUCKERS, A_GRAB, A_ROAR, A_INK, A_STORM, A_SWEEP, A_NET, A_FANS, A_NETFANS,
       A_SNAKE, A_WIPERS, A_MINES, A_NETSNAKE, A_BREATH };
static const unsigned char kPlan[4][8] = {
  //  0        1          2           3       4         5          6            7
  { A_SUCKERS, A_NET,     A_ROAR,   A_FANS,   A_SUCKERS, A_NETFANS, A_INK,    A_NONE },   // the clubs and the siphon
  { A_NONE,    A_SNAKE,   A_ROAR,   A_NONE,   A_NET,     A_NONE,    A_NONE,   A_NONE },   // the eyes (they do the shooting)
  { A_WIPERS,  A_MINES,   A_ROAR,   A_SNAKE,  A_SWEEP,   A_NONE,    A_FANS,   A_STORM },  // the boats
  { A_NETSNAKE, A_MINES,  A_ROAR,   A_BREATH, A_WIPERS,  A_SNAKE,   A_STORM,  A_NONE },   // fury
};

// The arms: their own clock, beside the mantle's round and the guns, so a
// slam or a grab can come in the middle of anything else -- sooner stage by
// stage: slams alone at first; from the eyes a grab every third time; with
// the boats slams and grabs by turns; in fury two lanes at once.
static void Arms(void) {
  if (KR.arm_cool > 0) {
    KR.arm_cool--;
    if (HardMode() && (KR.t & 3) == 0 && KR.arm_cool > 0) KR.arm_cool--;   // hard mode: a quarter sooner
    return;
  }
  const int p = KR.phase;
  int grab = 0;
  if (p == K_EYES) {
    if (++KR.arm_turn >= 3) { KR.arm_turn = 0; grab = 1; }
  } else if (p >= K_TRIDENT) {
    KR.arm_turn ^= 1;
    grab = KR.arm_turn;
  }
  if (grab && KR.grab < 0) Grab((KR.round & 8) ? 0 : 5);
  else SlamAt(p == K_FURY);
  KR.arm_cool = p == K_GUARD ? 300 : (p == K_EYES ? 230 : (p == K_TRIDENT ? 170 : 120));
}

static void KrAttack(void) {
  if (KR.cool > 0) {
    KR.cool--;
    if (HardMode() && (KR.round & 1) && KR.cool > 0) KR.cool--;    // hard mode: sooner
    return;
  }
  if (Busy(KR.mantle)) return;
  KR.round++;
  const int r = KR.round & 7;
  const int p = KR.phase;
  const int row = p == K_GUARD ? 0 : (p == K_EYES ? 1 : (p == K_TRIDENT ? 2 : 3));
  int a = kPlan[row][r];
  // hard mode: its own attacks in the quiet slots and in place of the ink
  if (HardMode()) {
    if (a == A_INK) a = 100 + LC_XSNIPE;
    else if (a == A_NONE && r == 7) a = 100 + LC_XSNAKE;
    else if (a == A_SWEEP && (KR.round & 8)) a = 100 + LC_XTWIST;
  }
  const int mood = KR.cracked ? (KR.fury ? 3 : 2) : (p == K_EYES ? 1 : 0);   // (its eyes charging)
  Anim(KR.mantle, mood);
  switch (a) {
  case A_SLAM:
    SlamAt(0);
    KR.cool = p == K_GUARD ? 130 : (p == K_EYES ? 100 : 75);
    return;
  case A_SLAM2:
    SlamAt(1);
    KR.cool = 60;
    return;
  case A_SUCKERS:
    for (int s = 0; s < 2; s++)
      if (Alive(KR.club[s]) && !Busy(KR.club[s])) Attack(KR.club[s], C_SUCKERS);
    KR.cool = 70;
    return;
  case A_GRAB:
    if (KR.grab < 0) Grab((KR.round & 8) ? 0 : 5);
    for (int s = 0; s < 2; s++)
      if (Alive(KR.club[s]) && !Busy(KR.club[s])) Attack(KR.club[s], C_SUCKERS);
    KR.cool = p == K_EYES ? 60 : 45;
    return;
  case A_ROAR:
    Anim(KR.mantle, KR.cracked ? mood : 1);
    OrderAt(KR.mantle, C_ROAR, DOWN);
    KR.cool = 170;
    return;
  case A_INK:
    Attack(KR.mantle, C_INK);
    KR.cool = 70;
    return;
  case A_STORM:
    Attack(KR.mantle, C_INKSTORM);
    KR.cool = 60;
    return;
  case A_SWEEP:
    Attack(KR.mantle, C_SWEEP);
    KR.cool = 60;
    return;
  case A_NET:
    GillLanes(0);
    KR.cool = 120;
    return;
  case A_FANS:
    Attack(KR.mantle, C_HEADFANS);
    KR.cool = 90;
    return;
  case A_NETFANS:                              // the walker's combo: the net, and fans inside it
    GillLanes(0);
    Attack(KR.mantle, C_HEADFANS);
    KR.cool = 160;
    return;
  case A_SNAKE:
    Attack(KR.mantle, C_SNAKE);
    KR.cool = 110;
    return;
  case A_WIPERS:                               // the fortress's: wipers, and fans between them
    GillLanes(1);
    Attack(KR.mantle, C_HEADFANS);
    KR.cool = 150;
    return;
  case A_MINES:
    Attack(KR.mantle, C_MINES);
    KR.cool = 100;
    return;
  case A_NETSNAKE:                             // fury: the net, and the snake through it
    GillLanes(0);
    Attack(KR.mantle, C_SNAKE);
    KR.cool = 150;
    return;
  case A_BREATH:
    if (!KR.breath) {
      KR.breath = 150;                           // it breathes in: everything toward the beak
      KR.cool = 160;
    } else {
      Attack(KR.mantle, C_INKSTORM);
      KR.cool = 60;
    }
    return;
  case A_NONE:
    KR.cool = 40;                                // a quiet beat
    return;
  default:                                       // hard mode's
    Attack(KR.mantle, a - 100);
    KR.cool = 55;
    return;
  }
}

// a UNITY torpedo got there (lab.c)
void LabTorpedoHit(s32 x, s32 y) {
  (void)x; (void)y;
  if (KR.phase != K_TRIDENT && KR.phase != K_FURY) return;
  if (!KR.cracked) {
    if (++KR.cracks >= CRACKS) {
      // the plates give: the eyes bare, the mantle open to everything
      KR.cracked = 1;
      Anim(KR.mantle, 2);
      Guard(KR.mantle, 0);
      Blasts(KR.x, KR.y + FX(30), 70, 30, 8);
      g_autopilot_target = KR.mantle;
      if (Carrying()) KR.crack_owed = 1;          // (said after Volta's line on the boat)
      else AbLine(LB_TRIDENT_DOC);
    }
    return;
  }
  if (Alive(KR.mantle)) SB_StrikeBadGuy(BH, KR.mantle, TORP_DMG);
}

static void Show(u32 a, int* shown, int f) {
  if (*shown != f) { PartFrame(a, f); *shown = f; }
}

// ---- the eyes (phase two) -----------------------------------------------------------------------
// The left eye takes only the laser, the right only the coilgun (each wears
// its weapon's flag). One open at a time, taking turns: shutters parting,
// open (hurt only now; glaring while it fires), shutting. Blind one and the
// other rages: open longer, shut shorter, firing sooner.
#define EYE_OPENING 24
#define EYE_OPEN    260
#define EYE_CLOSING 16

static void EyeAt(int e, s32* x, s32* y) {
  *x = KR.x + (e ? FX(EYE_DX) : FX(-EYE_DX));
  *y = KR.y + FX(EYE_DY);
}

static void EyesIn(void) {
  for (int e = 0; e < 2; e++) {
    s32 ex, ey;
    EyeAt(e, &ex, &ey);
    KR.eye[e] = SB_SpawnBadGuy(BH, e ? "*lab/boss/keye_r" : "*lab/boss/keye_l", ex, ey);   // laser, coilgun
    Guard(KR.eye[e], 1);
    KR.ehp[e] = EYE_HP;
    KR.evis[e] = AbSpawn("*lab/boss/keye_vis", ex, ey);
    KR.pupil[e] = AbSpawn("*lab/boss/kpupil", FX(-200), FX(-200));
    KR.es[e] = ES_SHUT;
    KR.eshown[e] = EV_SHUT;
  }
  KR.eturn = 0;
  KR.eshut_t = 70;
  for (int i = 0; i < NELET; i++) KR.elet[i] = SB_SpawnBadGuy(BH, "*lab/boss/eyelet", FX(200), FX(-330));
  KR.elet_r = 0;
  KR.ering = 0;
}

// The eyelets: as an eye opens, eight little eyes spread out round it and
// turn about it; now and then the eye calls a volley -- the cascade (each in
// turn looks at the ship and fires, round the ring) or the bloom (all at
// once, out, twice). Shut, they draw back into it and are put away.
static void Eyelets(int open_e, int talk) {
  s32 ex = FX(200), ey = FX(-330);
  if (open_e >= 0) EyeAt(open_e, &ex, &ey);
  const int out = open_e >= 0 && KR.es[open_e] == ES_OPEN;
  KR.elet_r += Ease((out ? FX(ELET_R) : 0) - KR.elet_r, FX(2.5));
  if (!talk) KR.elet_a = (KR.elet_a + (KR.blind ? 0x48000 : 0x30000)) & (TURN - 1);
  const int shown = open_e >= 0 && KR.elet_r > FX(6);
  const int rage = KR.blind > 0;
  // the volley
  if (KR.ering && !talk && shown) {
    const int t = KR.ering_t++;
    if (KR.ering == 1) {                                         // the cascade, round the ring
      const int n = rage ? 2 * NELET : NELET;
      if (KR.ering_k < n && t == 5 * KR.ering_k) {                  // one every 5 frames
        const u32 b = KR.elet[KR.ering_k & (NELET - 1)];
        if (Alive(b)) OrderAt(b, C_GAZE, AimFrom(b));
        KR.ering_k++;
      } else if (KR.ering_k >= n && t > 5 * n + 10) {
        KR.ering = 0;
      }
    } else {                                                     // the bloom: all out, twice (thrice raging)
      if (t == 0 || t == 26 || (rage && t == 52))
        for (int i = 0; i < NELET; i++)
          if (Alive(KR.elet[i]))
            OrderAt(KR.elet[i], C_GAZEOUT, (KR.elet_a + i * (TURN / NELET) + (t == 26 ? TURN / 16 : 0)) & (TURN - 1));
      if (t > (rage ? 70 : 44)) KR.ering = 0;
    }
  }
  if (!shown) KR.ering = 0;
  const int hot = KR.ering != 0;
  for (int i = 0; i < NELET; i++) {
    if (!Alive(KR.elet[i])) continue;
    const s32 a = (KR.elet_a + i * (TURN / NELET)) & (TURN - 1);
    if (shown) Place(KR.elet[i], ex + SB_MulFP(SB_Cos(a), KR.elet_r), ey + SB_MulFP(SB_Sin(a), KR.elet_r));
    else Place(KR.elet[i], FX(200), FX(-330));
    if (hot != KR.elet_hot) Anim(KR.elet[i], hot);
  }
  KR.elet_hot = hot;
}

static void Eyes(int talk) {
  // glare, the eyelets' volley, stare, irises -- the eyelets' every other time the cascade, else the bloom
  static const unsigned char kEyeCmd[4] = { C_GLARE, 0, C_STARE, C_IRIS };
  const int rage = KR.blind > 0;
  int open_e = -1;
  for (int e = 0; e < 2; e++)
    if (KR.es[e] == ES_OPENING || KR.es[e] == ES_OPEN || KR.es[e] == ES_CLOSING) open_e = e;
  Eyelets(open_e, talk);
  for (int e = 0; e < 2; e++) {
    if (KR.es[e] == ES_BLIND) continue;
    const u32 bg = KR.eye[e];
    s32 ex, ey;
    EyeAt(e, &ex, &ey);
    if (!Alive(bg)) {                                        // blinded
      AbBoom(ex, ey, 0);
      SB_ShakeWorld();
      Blasts(ex, ey, 14, 10, 5);
      KR.eye[e] = 0;
      KR.es[e] = ES_BLIND;
      Show(KR.evis[e], &KR.eshown[e], EV_BLIND);
      if (!KR.blind++) AbLine(LB_EYE_BLIND);
      KR.eturn = e ^ 1;
      KR.eshut_t = 40;
      g_autopilot_target = KR.eye[e ^ 1];
      continue;
    }
    const s32 h = SB_GetBadGuyHealth(bg);
    if (h < KR.ehp[e] - 20 && KR.es[e] == ES_OPEN) KR.eflash[e] = 6;   // struck: a flash
    KR.ehp[e] = h;
    const int t = talk ? KR.et[e] : ++KR.et[e];
    switch (KR.es[e]) {
    case ES_SHUT:
      Guard(bg, 1);
      Show(KR.evis[e], &KR.eshown[e], EV_SHUT);
      if (!talk && e == KR.eturn && (KR.es[e ^ 1] == ES_SHUT || KR.es[e ^ 1] == ES_BLIND) && --KR.eshut_t <= 0) {
        KR.es[e] = ES_OPENING;
        KR.et[e] = 0;
        g_autopilot_target = bg;
      }
      break;
    case ES_OPENING:
      Guard(bg, 1);
      Show(KR.evis[e], &KR.eshown[e], EV_OPENING);
      if (t >= EYE_OPENING) {
        KR.es[e] = ES_OPEN;
        KR.et[e] = 0;
        KR.ecool[e] = 16;
      }
      break;
    case ES_OPEN: {
      Guard(bg, talk);
      const int busy = Busy(bg) || KR.ering;
      Show(KR.evis[e], &KR.eshown[e], KR.eflash[e] > 0 ? EV_STRUCK : (busy ? EV_GLARE : EV_OPEN));
      if (KR.eflash[e] > 0) KR.eflash[e]--;
      AbFrame(KR.pupil[e], busy);
      if (talk) break;
      if (t >= (rage ? 330 : EYE_OPEN) && (!busy || t >= (rage ? 330 : EYE_OPEN) + 60)) {
        KR.es[e] = ES_CLOSING;
        KR.et[e] = 0;
        break;
      }
      if (busy || --KR.ecool[e] > 0) break;
      if (kEyeCmd[KR.ecycle]) {
        OrderAt(bg, kEyeCmd[KR.ecycle], AimFrom(bg));
      } else {
        KR.ering = (KR.ering_next++ & 1) ? 2 : 1;
        KR.ering_t = 0;
        KR.ering_k = 0;
      }
      if (++KR.ecycle >= 4) KR.ecycle = 0;
      KR.ecool[e] = rage ? 50 : 75;
      if (HardMode()) KR.ecool[e] -= KR.ecool[e] >> 2;
      break;
    }
    case ES_CLOSING:
      Guard(bg, 1);
      Show(KR.evis[e], &KR.eshown[e], EV_OPENING);
      if (t >= EYE_CLOSING) {
        KR.es[e] = ES_SHUT;
        KR.et[e] = 0;
        KR.eturn = KR.es[e ^ 1] == ES_BLIND ? e : e ^ 1;
        KR.eshut_t = rage ? 50 : 80;
      }
      break;
    }
  }
}

// ---- the deep (phase three) ---------------------------------------------------------------------
static void DeepIn(void) {
  KR.deep_t = 0;
  KR.wall = FX(-40);
  KR.wall_goal = FX(100);
  KR.wall_u = 0;
  KR.sq = 0;
  KR.sq_t = 0;
  KR.sq_state = 0;
  KR.lures_left = NLURE;
  KR.nine_gone = 0;
  KR.flare = 0;
  KR.grab = -1;                                  // it lets go of everything
  for (int k = 0; k < KR_ARMS; k++) {
    KR.act[k] = ARM_REST;
    KR.arm[k].state = 0;
  }
  Warn(0, 0, 0);
  Warn(1, 0, 0);
  for (int i = 0; i < NELET; i++) {              // the eyelets, put away (off screen: their pop unseen)
    if (!Alive(KR.elet[i])) continue;
    Place(KR.elet[i], FX(200), FX(-330));
    Remove(KR.elet[i]);
    KR.elet[i] = 0;
  }
}

static void LureAt(int k, s32* x, s32* y) {
  *x = KR.nx + FX(62) * (k - 1) + SB_Sin((KR.t * 0x28000 + k * 0x550000) & (TURN - 1)) * 10;
  *y = KR.ny + FX(k == 1 ? 170 : 150) + SB_Cos((KR.t * 0x30000 + k * 0x400000) & (TURN - 1)) * 6;
}

static void NineIn(void) {
  KR.nx = FX(200);
  KR.ny = FX(-230);
  KR.nine = AbSpawn("*lab/boss/nine", KR.nx, KR.ny);
  for (int k = 0; k < NLURE; k++)
    for (int j = 0; j < NLINK; j++) KR.link[k][j] = AbSpawn("*lab/boss/link", KR.nx, KR.ny);
  for (int k = 0; k < NLURE; k++) {
    KR.lure[k] = SB_SpawnBadGuy(BH, "*lab/boss/glantern", KR.nx, KR.ny);
    Guard(KR.lure[k], 1);
    KR.lhp[k] = LURE_HP;
    KR.lvis[k] = AbSpawn("*lab/boss/nine_lure", KR.nx, KR.ny);
    KR.lshown[k] = LV_GLOW;
    KR.lcool[k] = 70 + 45 * k;
    KR.lturn[k] = k;
    KR.lwreck[k] = 0;
  }
  g_autopilot_target = KR.lure[1];
}

static void NinePlace(void) {
  if (KR.nine) {
    AbPut(KR.nine, KR.nx, KR.ny);
    SB_SetActorOrient(KR.nine, DOWN);
  }
  const s32 axx = KR.nx, ayy = KR.ny + FX(118);              // her lure's root, at her bow
  for (int k = 0; k < NLURE; k++) {
    if (!KR.lvis[k]) continue;
    s32 lx, ly;
    LureAt(k, &lx, &ly);
    if (KR.lwreck[k]) ly += FX(KR.lwreck[k] * 3);             // a wreck sinks away
    if (Alive(KR.lure[k])) Place(KR.lure[k], lx, ly);
    AbPut(KR.lvis[k], lx, ly);
    SB_SetActorOrient(KR.lvis[k], DOWN);
    for (int j = 0; j < NLINK; j++) {
      const s32 f = (j + 1) * kRecip[NLINK + 1];
      AbPut(KR.link[k][j], axx + SB_MulFP(lx - axx, f), ayy + SB_MulFP(ly - ayy, f));
    }
  }
}

static void Lures(int talk) {
  // five attacks by turns, Mission 10's lure's among them (each lantern starts
  // its own place in the round, so the three don't fire the same at once)
  static const unsigned char kLureCmd[NLURECMD] = { C_LPULSE, C_LSPIRAL, C_LJAM, C_LRING, C_LLANTERNS };
  const int ready = KR.ny > NINE_Y - FX(6);
  for (int k = 0; k < NLURE; k++) {
    if (KR.lwreck[k]) {                                       // shot down: sinking, then gone
      if (KR.lwreck[k] < 60 && ++KR.lwreck[k] == 60) AbDrop(&KR.lvis[k]);
      continue;
    }
    const u32 bg = KR.lure[k];
    if (!bg) continue;
    s32 lx, ly;
    LureAt(k, &lx, &ly);
    if (!Alive(bg)) {
      AbBoom(lx, ly, 1);
      SB_ShakeWorld();
      KR.lure[k] = 0;
      KR.lwreck[k] = 1;
      Show(KR.lvis[k], &KR.lshown[k], LV_WRECK);
      for (int j = 0; j < NLINK; j++) AbDrop(&KR.link[k][j]);
      KR.lures_left--;
      KR.wall_goal -= FX(40);                                 // the walls give a little
      if (KR.wall_goal < FX(40)) KR.wall_goal = FX(40);
      g_autopilot_target = Alive(KR.lure[1]) ? KR.lure[1] : (Alive(KR.lure[0]) ? KR.lure[0] : KR.lure[2]);
      continue;
    }
    Guard(bg, talk || !ready);
    const s32 h = SB_GetBadGuyHealth(bg);
    if (h < KR.lhp[k] - 20) KR.lflash[k] = 5;
    KR.lhp[k] = h;
    const int busy = Busy(bg);
    Show(KR.lvis[k], &KR.lshown[k], KR.lflash[k] > 0 ? LV_STRUCK : (busy ? LV_FIRE : LV_GLOW));
    if (KR.lflash[k] > 0) KR.lflash[k]--;
    if (talk || busy || !ready || --KR.lcool[k] > 0) continue;
    OrderAt(bg, kLureCmd[KR.lturn[k]], AimFrom(bg));
    if (++KR.lturn[k] >= NLURECMD) KR.lturn[k] = 0;
    KR.lcool[k] = (KR.lures_left == 1 ? 80 : 120) + Rnd(0, 40);
    if (HardMode()) KR.lcool[k] -= KR.lcool[k] >> 2;
  }
}

// The walls close in by themselves (a lantern shot down pushes them back),
// and every few seconds they glow red and squeeze. Touching them hurts and
// shoves the ship back in.
static void Walls(int talk) {
  if (!talk && !KR.nine_gone) {
    KR.wall_goal += 0x1200;
    if (KR.wall_goal > WALL_IN) KR.wall_goal = WALL_IN;
  }
  KR.wall += Ease(KR.wall_goal - KR.wall, FX(1.2));
  if (!talk) KR.sq_t++;
  switch (KR.sq_state) {
  case 0:                                                     // waiting
    if (KR.sq_t >= (HardMode() ? 240 : 320) && !KR.nine_gone) { KR.sq_state = 1; KR.sq_t = 0; AbSound(ABS_CHARGE); }
    break;
  case 1:                                                     // lit: the warning
    if (KR.sq_t >= 45) { KR.sq_state = 2; KR.sq_t = 0; SB_ShakeWorld(); }
    break;
  case 2:                                                     // the squeeze
    KR.sq += Ease(SQUEEZE - KR.sq, FX(3));
    if (KR.sq_t >= 40) { KR.sq_state = 3; KR.sq_t = 0; }
    break;
  case 3:                                                     // easing off
    KR.sq += Ease(-KR.sq, FX(1.5));
    if (KR.sq_t >= 40) { KR.sq_state = 0; KR.sq_t = 0; KR.sq = 0; }
    break;
  }
  for (int k = 0; k < KR_ARMS; k++) KR.arm[k].state = KR.sq_state == 1 || KR.sq_state == 2;
  if (talk || LabShipDown() || KR.wall_u < FX(0.9) || KR.nine_gone) return;
  const s32 x = PlayerX();
  const int left = x < WallL() + FX(12), right = x > WallR() - FX(12);
  if (left || right) {
    AbPull(left ? WallL() + FX(50) : WallR() - FX(50), PlayerY(), FX(5));
    if (LabHitShip(0)) SB_ShakeWorld();
  }
}

// Phase three, a frame of it: 1 once the walls have let go
static int Deep(void) {
  const int talk = InCutscene();
  const int t = ++KR.deep_t;
  KR.y += Ease(DEEP_Y - KR.y, FX(3));
  KR.x += Ease(FX(200) - KR.x, FX(1));
  if (!KR.nine_gone) KR.wall_u = Smooth(t < 20 ? 0 : SB_MulFP(FX(t - 20), FX(1) / 90));
  Aura(talk);                                    // (out: the ring dies away)
  Walls(talk);
  if (t == 90) {
    NineIn();
    AbLine(LB_NINE);                  // Marrow: remember Specimen Nine?
    AbLine(LB_NINE_VOLTA);
  }
  if (KR.nine) {
    if (!KR.nine_gone) {
      KR.ny += Ease(NINE_Y - KR.ny, FX(1.6));
      KR.nx += Ease(FX(200) + SB_Sin((KR.t * 0x14000) & (TURN - 1)) * 26 - KR.nx, FX(0.8));
    } else {
      KR.ny += Ease(FX(-300) - KR.ny, FX(2.4));               // she runs for it, back up
    }
    Lures(talk);
    NinePlace();
    if (!KR.nine_gone && KR.lures_left <= 0) {
      KR.nine_gone = 1;
      KR.deep_t = 1000;
      KR.wall_goal = FX(-60);
      KR.sq = 0;
      KR.sq_state = 0;
      AbLine(LB_NINE_DOWN);
    }
  }
  if (KR.nine_gone) {
    KR.wall_u = Smooth(FX(1) - SB_MulFP(FX(t - 1000), FX(1) / 140));
    if (t >= 1140 && KR.ny < FX(-260)) {
      AbDrop(&KR.nine);
      for (int k = 0; k < NLURE; k++) {
        AbDrop(&KR.lvis[k]);
        for (int j = 0; j < NLINK; j++) AbDrop(&KR.link[k][j]);
      }
      return 1;
    }
  }
  return 0;
}

static void KrFight(void) {
  const int talk = InCutscene();
  for (int k = 0; k < KR_ARMS; k++) ArmTick(k, talk);
  Aura(talk);
  Guns(talk);
  Gills(talk);
  if (KR.phase == K_EYES) Eyes(talk);
  const int open = !talk;
  if (Alive(KR.mantle)) Guard(KR.mantle, !(open && KR.cracked));
  for (int s = 0; s < 2; s++)
    if (Alive(KR.club[s])) Guard(KR.club[s], !open);
  if (Alive(KR.siphon)) Guard(KR.siphon, !open);
  // (the boats hold their fire while it has one of them)
  if (KR.phase >= K_TRIDENT) LabSubsTick(LabKrakenX(), LabKrakenY(), !talk && !Carrying());
  if (KR.breath > 0) {
    KR.breath--;
    if (!talk) AbDraw(KR.x, KR.y + FX(90), FX(1.15));
    if ((KR.breath & 7) == 0)
      SB_SpawnGeneric(BH, "*lab/boss/bubbles", KR.x + FX(Rnd(-140, 140)), KR.y + FX(Rnd(160, 330)));
  }
  if (talk) return;
  BossWards();
  KrAttack();
  Arms();
}

static void KrUpdate(u32 boss) {
  KR.t++;
  BlastTick();
  SpeechPump();
  const s32 hx = FX(200) + SB_Sin((KR.t * 0x8000) & (TURN - 1)) * 22;
  const s32 hy = KR_REST_Y + SB_Sin((KR.t * 0x14000) & (TURN - 1)) * 6;
  switch (KR.phase) {
  case K_IN:
    KR.y += Ease(KR_REST_Y - KR.y, FX(1.0));
    for (int k = 0; k < KR_ARMS; k++) KrRest(k);
    if (AbAbs(KR.y - KR_REST_Y) < FX(1) && AbQuiet() && KR.t > 160) {
      KR.phase = K_GUARD;
      KR.t = 0;
      KR.cool = 50;
      KR.arm_cool = 160;
    }
    break;
  case K_GUARD:
    KR.x += Ease(hx - KR.x, FX(0.6));
    KR.y += Ease(hy - KR.y, FX(0.6));
    if (!Alive(g_autopilot_target))
      g_autopilot_target = Alive(KR.club[0]) ? KR.club[0] : (Alive(KR.club[1]) ? KR.club[1] : KR.siphon);
    if (!Alive(KR.club[0]) && !Alive(KR.club[1]) && !Alive(KR.siphon)) {
      // stripped: the shutters over its eyes open
      KR.club[0] = KR.club[1] = KR.siphon = 0;
      KR.phase = K_EYES;
      KR.t = 0;
      R32(boss, B_MAXHP) = EYE_HP * 2;
      // the mantle's stand-in takes its place (nothing to hit: the shots must reach
      // the eyes under it); the real one waits off screen
      KR.armour = KR.mantle;
      KR.mantle = SB_SpawnBadGuy(BH, "*lab/boss/mantle_shell", KR.x, KR.y);
      Place(KR.armour, FX(200), FX(-330));
      Guard(KR.armour, 1);
      EyesIn();
      Anim(KR.mantle, 1);
      AbLine(LB_EYES_OPEN);           // Marrow: open your eyes, sweetie
      AbLine(LB_EYES_DOC);
      KR.cool = 90;
      KR.arm_cool = 150;
      break;
    }
    KrFight();
    break;
  case K_EYES:
    KR.x += Ease(hx - KR.x, FX(0.6));
    KR.y += Ease(hy - KR.y, FX(0.6));
    if (KR.blind >= 2) {
      // blind: it pulls back up out of sight, and its arms come down as walls
      KR.phase = K_DEEP;
      KR.t = 0;
      R32(boss, B_MAXHP) = LURE_HP * NLURE;
      g_autopilot_target = 0;
      Anim(KR.mantle, 0);
      DeepIn();
      SB_ShakeWorld();
      AbLine(LB_DEEP);
      break;
    }
    KrFight();
    break;
  case K_DEEP:
    if (Deep()) {
      // the lanterns are out: it comes back down, armoured
      KR.phase = K_RETURN;
      KR.t = 0;
      KR.wall_u = 0;
      R32(boss, B_MAXHP) = MANTLE_HP;
      // the real mantle back in its stand-in's place (that one gone off screen)
      Place(KR.mantle, FX(200), FX(-330));
      Remove(KR.mantle);
      KR.mantle = KR.armour;
      KR.armour = 0;
      Anim(KR.mantle, 0);
      g_autopilot_target = KR.mantle;
      AbLine(LB_KRAKEN_BACK);
    }
    break;
  case K_RETURN:
    KR.y += Ease(KR_REST_Y - KR.y, FX(2.0));
    KR.x += Ease(hx - KR.x, FX(0.6));
    for (int k = 0; k < KR_ARMS; k++) KrRest(k);
    Aura(InCutscene());
    if (AbAbs(KR.y - KR_REST_Y) < FX(2) && KR.t > 140 && !InCutscene()) {
      // and only now Volta's boats come in
      KR.phase = K_TRIDENT;
      KR.t = 0;
      KR.trident_t = 0;
      LabSubsIn();
      AbLine(LB_TRIDENT_IN);
      KR.cool = 90;
      KR.arm_cool = 120;
    }
    break;
  case K_TRIDENT:
  case K_FURY:
    KR.x += Ease(hx - KR.x, FX(0.6));
    KR.y += Ease(hy - KR.y, FX(0.6));
    if (!Alive(KR.mantle)) {
      KR.phase = K_DYING;
      KR.t = 0;
      KR.mantle = 0;
      GlowDrop(&KR.brain);
      ShipShield(1);
      Blasts(KR.x, KR.y + FX(30), 100, 60, 30);
      g_autopilot_target = 0;
      R32(boss, B_MAXHP) = 0;
      Warn(0, 0, 0);
      Warn(1, 0, 0);
      AbLineNow(LB_KRAKEN_DOWN);
      break;
    }
    KR.trident_t++;
    // it takes a boat (Trident Three, the right-hand one) once -- only with
    // nobody talking, so no box hides it; Volta's line once it's gone
    if (!KR.crushed && KR.trident_t > 700 && LabSubAlive(2) && ArmFree(5) && KR.grab != 5 && !InCutscene()
        && AbQuiet()) {
      KR.crushed = 1;
      KR.crush_sub = 2;
      KR.act[5] = ARM_CARRY;
      KR.act_t[5] = 0;
    }
    if (KR.lost_t > 0 && --KR.lost_t == 0) {
      AbLineNow(LB_TRIDENT_LOST);
      if (KR.crack_owed) { KR.crack_owed = 0; AbLine(LB_TRIDENT_DOC); }
    }
    if (!KR.fury && KR.cracked && !Carrying() && Hp(KR.mantle) * 5 < MANTLE_HP * 2) {
      KR.fury = 1;
      KR.phase = K_FURY;
      Anim(KR.mantle, 3);
      AbLine(LB_FURY);
      KR.volley_ticket = AbLine(LB_VOLLEY);       // Volta: all boats, fire
    }
    if (KR.fury && !KR.volley && KR.volley_ticket && AbLineUp(KR.volley_ticket)) {
      KR.volley = 1;
      LabSubsVolley();
    }
    KrFight();
    break;
  case K_DYING:
    // thrashing, then still: the arms fall away, the mantle goes under in fire
    for (int k = 0; k < KR_ARMS; k++) {
      Limb* L = &KR.arm[k];
      if (!L->live) continue;
      LimbToward(L, DOWN, 0x30000, 0, KR_SEG, KR.t < 90 ? 0x200000 : 0);
      L->ph = (KR.t * 0x90000 + k * 0x200000) & (TURN - 1);
      L->state = 0;
      if (KR.t == 90 + k * 10) {
        AbBoom(TipX(L), TipY(L), 0);
        LimbDrop(L);
      }
    }
    for (int m = 0; m < NMOTE + NMOTE_IN; m++) AbDrop(&KR.mote[m]);
    AbDrop(&KR.drain);
    for (int w = 0; w < NWARN; w++) AbDrop(&KR.warn[w]);
    for (int s = 0; s < 2; s++) Remove(KR.club[s]);
    Remove(KR.siphon);
    if (KR.t == 60)
      for (int e = 0; e < 2; e++) {
        s32 ex, ey;
        EyeAt(e, &ex, &ey);
        AbBoom(ex, ey, 0);
        AbDrop(&KR.evis[e]);
        AbDrop(&KR.pupil[e]);
      }
    for (int g = 0; g < NGUNS; g++) {
      if (KR.t == 40 + 15 * g) {
        AbBoom(KR.x + FX(kGunDX[g]), KR.y + FX(kGunDY[g]), 0);
        Remove(KR.gun[g]);
        KR.gun[g] = 0;
        AbDrop(&KR.gvis[g]);
        AbDrop(&KR.fire[g]);
      }
    }
    if (KR.t == 120)
      for (int s = 0; s < 2; s++) {
        AbBoom(KR.x + (s ? FX(GILL_DX) : FX(-GILL_DX)), KR.y + FX(GILL_DY), 0);
        AbDrop(&KR.gill[s]);
        AbDrop(&KR.fin[s]);
        Remove(KR.gemit[s]);
        KR.gemit[s] = 0;
      }
    if (KR.t > 200 && BW.left <= 0 && AbQuiet()) {
      KR.phase = K_ENDING;
      KR.t = 0;
      LabEndingStart();
    }
    break;
  case K_ENDING:
    if (LabEnding()) KR.phase = K_OVER;
    break;
  case K_OVER:
    R32(boss, B_STATE) = 0;
    return;
  }
  if (KR.phase < K_DYING) {
    KrPlace();
  } else if (KR.phase == K_DYING) {
    KrPlaceParts();
    for (int k = 0; k < KR_ARMS; k++) {
      s32 rx, ry;
      KrRoot(k, &rx, &ry);
      LimbPose(&KR.arm[k], rx, ry);
      LimbPlace(&KR.arm[k]);
    }
  }
}

static void KrDeath(u32 boss) {
  (void)boss;
  for (int k = 0; k < KR_ARMS; k++) LimbDrop(&KR.arm[k]);
  ShipShield(0);
  g_autopilot_target = 0;
  SB_PausePartition(BH, 0);                      // on to the level's end
}

void KrakenBossInit(u32 boss) {
  SB_ClearBytes((void*)boss, BOSS_SIZE);
  SB_ClearBytes(&KR, sizeof KR);
  SB_ClearBytes(&BW, sizeof BW);
  KR.grab = -1;
  R32(boss, B_SPAWN + 0x00) = (s32)(u32)KrSpawn;
  R32(boss, B_SPAWN + 0x04) = (s32)(u32)KrUpdate;
  R32(boss, B_SPAWN + 0x08) = (s32)(u32)KrRender;
  R32(boss, B_SPAWN + 0x0c) = (s32)(u32)KrRender;
  R32(boss, B_SPAWN + 0x10) = (s32)(u32)KrDeath;
  R32(boss, B_HEALTH) = (s32)(u32)KrHealth;
}

// abyss.c: which red outline a part that turned a hit wears (abyss.h
// OUTLINE_*), and the frame its animation shows. A dead badguy's handle
// goes to the next spawned, so a handle counts only while its boss is up
// and what holds it still carries the part's mark.
int LabPartOutline(u32 bg, int* frame) {
  if (!bg) return -1;
  const s32 m = GetReg(bg, 21) >> 16;
  const int a = GetReg(bg, REG_ANIM) >> 16;
  if (MN.phase >= M_IN && MN.phase < M_DEAD && bg == MN.bell && m == 0) {
    *frame = a == 1 ? 3 : (a == 2 ? 6 : 0);
    return OUTLINE_BELL;
  }
  if (KR.phase >= K_IN && KR.phase < K_DYING) {
    if (bg == KR.mantle && m == 0) { *frame = a; return OUTLINE_MANTLE; }
    if ((bg == KR.club[0] || bg == KR.club[1]) && m == MARK_SNIPER) { *frame = a; return OUTLINE_CLUB; }
    if (bg == KR.siphon && m == MARK_LASER) { *frame = 0; return OUTLINE_SIPHON; }
  }
  return -1;
}

// InitBoss (worlds.c's row for world 12): a new run of the level, from the Lion's Mane.
void AbyssReset(void);
void LabBossInit(u32 boss) {
  SpeechReset();
  AbyssReset();
  LabReset();
  SB_ClearBytes(&BW, sizeof BW);
  SB_ClearBytes(&KR, sizeof KR);
  KR.grab = -1;
  MnInstall(boss);
}
