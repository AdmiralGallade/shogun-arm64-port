// starfish.c -- World 5's final boss: Dr. Marrow's starfish.
//
// A hub with five armoured arms, built from badguys (beach/boss/core and
// arm, scripted in beach/enemies.py). This file is the boss itself: where
// the pieces are, which way the starfish turns, and when each piece fires.
// The pieces' scripts do the firing when asked (see enemies.py for the
// command registers).
//
//   enter    it rises into view from the top, arms turning;
//   arms     the hub is armoured shut. The starfish turns so a live arm points
//            at the player and that arm attacks: bullet hail and seeking
//            missiles while all five are on; once one is lost, seeking
//            lasers join the cycle. Each arm has to be blown off;
//   change   with the last arm gone: "Activating submersible mode." The hub
//            sinks;
//   sub      it travels under the water to a random spot, breaks the surface,
//            attacks -- rings, missiles, a laser -- and dives again. It can
//            only be hurt while it is up;
//   death    "Thank you for the data."
//
// It plugs into the engine's BOSS record (SHOGUN+0x4e78) the way the game's
// own bosses do: five callbacks at +0x26c (spawn, update, render, render
// shadow, death) and a health callback at +0x28c that feeds the boss bar.
// The engine calls spawn from the partition's boss event, update every
// frame while +0 (the state) is positive, and death once update leaves the
// state at zero.

typedef unsigned int u32;
typedef int s32;

#define IMG 0x10000000u
#define FN(type, addr) ((type)(IMG + (addr)))

#define SF_GetGameDataPtr    FN(u32 (*)(void), 0x63128)                       // ARM
#define SF_ClearBytes        FN(void (*)(void*, u32), 0x56629)
#define BH_PausePartition    FN(void (*)(u32, u32), 0xbad4c)                  // ARM
#define ShowNextSpeechSection FN(void (*)(u32), 0x3681d)
#define ShakeWorld           FN(void (*)(void), 0x34f79)
#define BH_SpawnBadGuy       FN(u32 (*)(u32, const char*, s32, s32), 0xbc124) // ARM
#define BH_GetBadGuyActorSF  FN(u32 (*)(u32), 0xbac8c)                        // ARM
#define AM_SetActorPosition  FN(void (*)(u32, s32, s32), 0xc174c)             // ARM
#define AM_SetActorOrientation FN(void (*)(u32, s32), 0xc17e0)                // ARM
#define AM_SetActorRegisterValue FN(void (*)(u32, u32, s32), 0xc1814)         // ARM
#define AM_GetActorRegisterValue FN(s32 (*)(u32, u32), 0xc17f8)               // ARM
#define BH_SetBadGuyInvincibility FN(void (*)(u32, s32), 0xbac5c)             // ARM
#define BH_GetBadGuyHealthSF FN(s32 (*)(u32), 0xbac64)                        // ARM
#define BH_SpawnGenericEntity FN(u32 (*)(u32, const char*, s32, s32), 0xbc1d8) // ARM
#define BH_SpawnExplosion    FN(void (*)(u32, const char*, s32, s32), 0xbd80c) // ARM
#define SF_Sin               FN(s32 (*)(s32), 0x49ffd)
#define SF_Cos               FN(s32 (*)(s32), 0x4a0bd)
#define SF_MulFP             FN(s32 (*)(s32, s32), 0x63148)                   // ARM
#define SF_GetFirstBadGuy    FN(u32 (*)(u32), 0xbb05c)                        // ARM
#define SF_GetNextBadGuy     FN(u32 (*)(u32, u32), 0xbb064)                   // ARM
#define SF_RangedRand        FN(s32 (*)(s32, s32), 0x49f9d)
#define SF_GetAngle          FN(s32 (*)(s32, s32, s32, s32), 0x63ef4)         // ARM
#define SF_GetFrameCount     FN(u32 (*)(void), 0x72434)                       // ARM
#define SF_GetGenericActor   FN(u32 (*)(u32), 0xbaba8)                        // ARM
#define SF_GetActorPosition  FN(void (*)(u32, s32*, s32*), 0xc1730)            // ARM
#define StrikePlayer         FN(void (*)(u32, u32, u32), 0x384dd)
#define isDialogHidden       FN(int (*)(u32), 0x28729)

#define BH 0x405
#define R32(p, off)  (*(volatile s32*)((u32)(p) + (off)))

// BOSS record
#define B_STATE     0x000
#define B_MAXHP     0x288      // the boss bar: health callback / this
#define B_SPAWN     0x26c       // then update, render, render shadow, death
#define B_HEALTH    0x28c
#define BOSS_SIZE   0x294
#define SH_PLAYER_X 0x5110      // the ship, 16.16 playfield units
#define SH_PLAYER_Y 0x512c
#define SH_PLAYER   0x5110      // PLAYER
#define SH_HUD      0x8b3a8
#define P_CAPSULES  0x74        // PLAYER: capsules (lives); each a full shield
#define SH_HITLOCK  0x5182      // u16: the ship can't be hit (respawning etc.)
#define SH_HITLOCK2 0x5188
#define SH_GODMODE  0x5180      // u16: the ship takes no hits (EnableGodMode)

// ---- geometry: playfield units, 16.16; angles 2^24 to the turn ----------
#define FX(v)       ((s32)((v) * 65536))
#define TURN        0x1000000
#define DOWN        0x400000                    // straight at the player
#define FIFTH       (TURN / 5)
#define DEG(d)      ((s32)((d) * 46603))
#define ARM_DIST    54                          // hub centre to arm centre
#define HOME_X      200
#define HOME_Y      112
#define ROT_SPEED   DEG(1.4)                    // per frame

// script registers (enemies.py)
#define REG_AIM     13
#define REG_ANIM    18
#define REG_BUSY    19
#define REG_CMD     20
enum { CMD_HAIL = 1, CMD_MISSILES, CMD_LASER, CMD_RING, CMD_STAR, CMD_PINWHEEL,
       CMD_CURTAIN, CMD_BEAM,
       // hard mode's (beach/enemies.py HARD_CMDS)
       CMD_SNIPE, CMD_PINCER, CMD_TWIST, CMD_SNAKE, CMD_GAP };
// hub animations
enum { ANIM_IDLE, ANIM_FIRE, ANIM_CHARGE, ANIM_UNDER, ANIM_BREACH };

enum { P_ENTER = 1, P_ARMS, P_CHANGE, P_SUBMERGED, P_BREACH, P_UP, P_DIVE, P_DEAD,
       P_RUSH };

// The arm rush: the arms it lost come back from under the water, one after
// another, each at a point round the ship. Each takes aim at where the ship
// is as it appears, shows as a dark shape for a moment, then breaks the
// surface and charges straight through that spot. They cannot be shot --
// only dodged (enemies.py: arm_rush, which hurts on contact).
#define RUSH_N       5
#define RUSH_GAP     12         // frames between arms
#define RUSH_RADIUS  165
#define RUSH_TAIL    240        // frames after the last arm before moving on
// The arms hurt the ship by touch, but not through the engine's own body
// collision: that takes a whole capsule per contact and repeats while the
// two overlap, so one slow arm emptied every capsule -- and any badguy has
// it, even with no hitbox (it gets a point at its centre). So the arms are
// generic entities, which never touch the ship, and the boss checks the ship
// against each arm's shape itself and strikes it as a bullet does -- most
// of the shield, a capsule at most. Each arm strikes once; after a strike
// the next arm waits a moment, so two arms crossing the ship together cost
// one strike, but the rest of the volley still hurts.
// A rush arm lives 338 frames (enemies.py arm_rush: 2 + 44 + 8 + 24 + 260);
// it is only read while younger than that -- after the rush phase too, as
// the last arms are still charging when it ends.
#define RUSH_LIFE      330
// The arm as drawn (art_mech.py star_arm, one unit a pixel), from the
// sprite's centre along where it points: the root 48 behind, 22 either side
// of the axis, tapering to 7 at the tip 46 ahead. Plus a margin for the
// ship's own size: the test is against its centre.
#define RUSH_ROOT      48
#define RUSH_TIP       46
#define RUSH_ROOT_W    22
#define RUSH_TIP_W     7
#define RUSH_MARGIN    5
#define RUSH_GRACE     20        // frames after a strike before another arm's

#define NARMS 5

static struct {
  u32 core, arm[NARMS];
  int arm_alive[NARMS];
  s32 x, y;                     // hub centre, 16.16
  s32 rot;                      // arm 0's direction
  int phase, t, cool, seq, attacks_left;
  s32 tx, ty;                   // where the hub is heading under water
  u32 frame;
  int dives;                    // every other dive is an arm rush
  s32 rush_base;
  u32 rush_arm[RUSH_N];         // the arms' actors, where they point, and when
  s32 rush_aim[RUSH_N];         // each was spawned
  u32 rush_born[RUSH_N];
  int rush_struck[RUSH_N];      // that arm has hit the ship already
  int rush_grace;
  int said_first, said_angry, said_rush;
  int pinwheel;                 // frames left of the pinwheel (all arms stream)
  int last_lost;
  int dir;
  s32 settle;                   // turn per frame left over from a pinwheel
  u32 sway;
  s32 p2max;                    // the hub's health when the arms were gone
  int god_set;                  // we raised the ship's god flag for a cutscene
  u32 god_prev;
} S;

// ---- Dr. Marrow's lines -------------------------------------------------------
// The dialog plays its speeches strictly in order (beach/make_beach.py,
// SPEECHES): the hermit's entrance and death (hermit.c), then the
// starfish's intro, first arm lost, angry, submersible, first rush, and its
// death. Several can fall due at once -- two arms blown off in one frame --
// so they queue, and each gets time on screen before the next.
// A queued line waits until the box has closed (and a moment more):
// ShowNextSpeechSection moves on to the next *sentence* while a speech has
// any left, so calling it during a speech of several sentences (Seoul's)
// would skip a sentence and push every later line out of step.
#define SAY_GAP 30
// Each queued line may first step over sentences nobody will hear: a world
// whose story branches (Edinburgh's ShogunGPT) steps over the lines of the
// branch not taken, all in one frame, so none of them is ever drawn. The
// Beach, Seoul and the chase never skip anything.
#define SAY_MAX 32
static int g_say_pending;
static u32 g_shown;               // lines put up so far (SpeechShown)
static unsigned char g_say_skip[SAY_MAX];   // per queued line, oldest first
static u32 g_say_last;
static u32 g_closed_at;           // when the box was last seen to close
static int g_open;
// Hurried (a skipped tutorial, edinburgh.c): the speech on screen moves on
// every few frames -- within itself only. ShowNextSpeechSection on a
// speech's last sentence starts the *next speech in the file*, whoever's it
// is, so there the box is closed instead (HideDialog, which leaves the speech
// where it is, as its own "close" does). Lines still queued are dropped by
// the caller (SpeechDropQueued), who steps over them later. The hurry ends
// by itself once the box is shut and nothing is queued.
#define HURRY_EVERY 4
#define SH_SPEECH   0x8b6e4       // SHOGUN: the speech on screen (SPEECH +0xc sentence, +0x14 count)
#define HideDialog  FN(void (*)(u32), 0x36d55)
static int g_hurry;
static u32 g_hurried_at;

void SpeechHurry(int on) { g_hurry = on; }

// Forget the lines still waiting (not the one on screen); how many there were.
int SpeechDropQueued(void) {
  const int n = g_say_pending;
  for (int i = 0; i < SAY_MAX; i++) g_say_skip[i] = 0;
  g_say_pending = 0;
  return n;
}

static void ShowQueued(void) {
  int n = g_say_skip[0];
  for (int i = 1; i < SAY_MAX; i++) g_say_skip[i - 1] = g_say_skip[i];
  g_say_skip[SAY_MAX - 1] = 0;
  g_say_pending--;
  g_shown++;
  for (; n > 0; n--) ShowNextSpeechSection(SF_GetGameDataPtr());
  ShowNextSpeechSection(SF_GetGameDataPtr());
}

void SpeechPump(void) {
  const u32 now = SF_GetFrameCount();
  const u32 sh = SF_GetGameDataPtr();
  if (!isDialogHidden(sh)) {
    g_open = 1;
    const u32 sp = R32(sh, SH_SPEECH);
    if (g_hurry && sp && now - g_hurried_at >= HURRY_EVERY) {
      g_hurried_at = now;
      if (R32(sp, 0xc) < R32(sp, 0x14) - 1) ShowNextSpeechSection(sh);
      else HideDialog(sh);
    }
    return;
  }
  if (g_hurry && !g_say_pending) g_hurry = 0;           // done: nothing more to hurry
  if (g_open) { g_open = 0; g_closed_at = now; }
  if (!g_say_pending) return;
  // a moment after the last line went up (the box may not show as open
  // until the next frame) and after the box closed
  const u32 gap = g_hurry ? HURRY_EVERY : SAY_GAP;
  if (g_say_last && now - g_say_last < gap) return;
  if (g_closed_at && now - g_closed_at < gap) return;
  ShowQueued();
  g_say_last = now;
}

// The next line, after first stepping over `skip` sentences.
void SaySkipping(int skip) {
  if (g_say_pending < SAY_MAX) {
    g_say_skip[g_say_pending] = (unsigned char)(skip < 0 ? 0 : (skip > 255 ? 255 : skip));
    g_say_pending++;
  }
  SpeechPump();
}

void Say(void) { SaySkipping(0); }

// Lines queued and not yet up (0: the last one asked for is on screen, or done).
int SpeechPending(void) { return g_say_pending; }

// Lines put up since the start (they only ever go up in order: a line asked
// for now is on screen once this reaches SpeechShown() + SpeechPending()).
u32 SpeechShown(void) { return g_shown; }

// Last words cannot wait: whatever is still queued goes by, then this.
void SayNowSkipping(int skip) {
  while (g_say_pending > 0) ShowQueued();
  for (; skip > 0; skip--) ShowNextSpeechSection(SF_GetGameDataPtr());
  ShowNextSpeechSection(SF_GetGameDataPtr());
  g_shown++;
  g_say_last = SF_GetFrameCount();
}

void SayNow(void) { SayNowSkipping(0); }

void SpeechReset(void) {
  g_hurry = 0;
  g_say_pending = 0;
  for (int i = 0; i < SAY_MAX; i++) g_say_skip[i] = 0;
  g_say_last = 0;
  g_closed_at = 0;
  g_open = 0;
}

// A cutscene: Dr. Marrow is talking (a line on screen or one queued). Both
// bosses hold their fire, cannot be hurt, and the ship cannot be hit -- a
// break, the way the other worlds' bosses talk before they fight.
int InCutscene(void) {
  return g_say_pending > 0 || !isDialogHidden(SF_GetGameDataPtr());
}

// How long the dialog has been stuck: frames since anything about it last
// changed (the speech on screen, its sentence, the box opening or closing,
// the queue) while someone is talking; 0 when nobody is. A wait for the talk
// to end gives up only on this -- a long speech is not a stuck one (a flat
// time limit cut the tutorial short and let it run on ahead of the Doc).
int SpeechStalled(void) {
  static u32 last_sig, changed_at;
  const u32 sh = SF_GetGameDataPtr();
  const u32 now = SF_GetFrameCount();
  if (!InCutscene()) { changed_at = now; return 0; }
  const u32 sp = R32(sh, SH_SPEECH);
  const u32 sig = sp ^ ((sp ? (u32)R32(sp, 0xc) : 0) << 20) ^ ((u32)isDialogHidden(sh) << 28) ^ ((u32)g_say_pending << 24);
  if (sig != last_sig) { last_sig = sig; changed_at = now; }
  return (int)(now - changed_at);
}

// The ship's god flag (what the engine's EnableGodMode sets: its bullet and
// ram checks skip the strike). Several things may want it up at once -- a
// cutscene, the capsule shield (missile.c) -- so each holds its own bit,
// the flag is raised while any is held, and put back as it was when the
// last lets go. Held, it is set again every call, in case the game cleared
// it in between.
static int g_god_why;
static int g_god_raised;
static unsigned short g_god_prev;

void GodHold(int who, int on) {
  volatile unsigned short* god =
      (volatile unsigned short*)(SF_GetGameDataPtr() + SH_GODMODE);
  if (on) g_god_why |= who; else g_god_why &= ~who;
  if (g_god_why) {
    if (!g_god_raised) { g_god_prev = *god; g_god_raised = 1; }
    *god = 1;
  } else if (g_god_raised) {
    *god = g_god_prev;
    g_god_raised = 0;
  }
}

#define GOD_CUTSCENE 1            // (missile.c: GOD_SHIELD 2)
void ShipShield(int on) { GodHold(GOD_CUTSCENE, on); }

static u32 Shogun(void) { return SF_GetGameDataPtr(); }
static u32 Actor(u32 bg) { return BH_GetBadGuyActorSF(bg); }

static void SetReg(u32 bg, u32 reg, s32 v) { AM_SetActorRegisterValue(Actor(bg), reg, v); }
static s32 GetReg(u32 bg, u32 reg) { return AM_GetActorRegisterValue(Actor(bg), reg); }

static int Alive(u32 bg) {
  if (!bg) return 0;
  for (u32 b = SF_GetFirstBadGuy(BH); b; b = SF_GetNextBadGuy(BH, b))
    if (b == bg) return BH_GetBadGuyHealthSF(bg) > 0;
  return 0;
}

static s32 Wrap(s32 a) {                       // signed difference, +-half turn
  a &= TURN - 1;
  if (a >= TURN / 2) a -= TURN;
  return a;
}

// Put a piece at (x, y) facing `angle`. A 2^24 turn is the same number as
// the scripts' 256-degree turn in 16.16, so angles pass straight through:
// the sprites are drawn pointing right, which is orientation 0.
static void Place(u32 bg, s32 x, s32 y, s32 angle) {
  const u32 a = Actor(bg);
  AM_SetActorPosition(a, x, y);
  AM_SetActorOrientation(a, angle & (TURN - 1));
}

static s32 ArmAngle(int i) { return (S.rot + FIFTH * i) & (TURN - 1); }

static void PlaceAll(void) {
  Place(S.core, S.x, S.y, S.rot);
  for (int i = 0; i < NARMS; i++) {
    if (!S.arm_alive[i]) continue;
    const s32 a = ArmAngle(i);
    Place(S.arm[i], S.x + SF_MulFP(SF_Cos(a), FX(ARM_DIST)),
          S.y + SF_MulFP(SF_Sin(a), FX(ARM_DIST)), a);
  }
}

static void Order(u32 bg, int cmd, s32 aim) {
  SetReg(bg, REG_AIM, aim & (TURN - 1));       // 256 to the turn, in 16.16
  SetReg(bg, REG_BUSY, FX(1));
  SetReg(bg, REG_CMD, FX(cmd));
}

static int Busy(u32 bg) { return GetReg(bg, REG_BUSY) != 0; }

// Hard mode (docs/HARD_MODE.md): the lap is SHOGUN+0x4e70, 2 on the hell lap.
static int Hard(void) { return R32(Shogun(), 0x4e70) >= 2; }

static void Splash(s32 x, s32 y) {
  BH_SpawnGenericEntity(BH, "*beach/boss/bubbles", x, y);
}

// ---- callbacks ----------------------------------------------------------------

static void SfRender(u32 boss, s32 x, s32 y) { (void)boss; (void)x; (void)y; }

static s32 SfHealth(u32 boss) {
  (void)boss;
  // The bar is the arms while there are arms, then the hub: the engine
  // draws health / BOSS+0x288, and SetBar refills it for the second phase.
  s32 hp = 0;
  if (S.phase == P_DEAD || !S.core) return 0;
  if (S.phase <= P_ARMS) {
    for (int i = 0; i < NARMS; i++)
      if (S.arm_alive[i]) hp += BH_GetBadGuyHealthSF(S.arm[i]);
    return hp;
  }
  return BH_GetBadGuyHealthSF(S.core);
}

static void SfSpawn(u32 boss) {
  S.x = FX(HOME_X);
  S.y = FX(-90);
  S.rot = DOWN;
  S.core = BH_SpawnBadGuy(BH, "*beach/boss/core", S.x, S.y);
  for (int i = 0; i < NARMS; i++) {
    S.arm[i] = BH_SpawnBadGuy(BH, "*beach/boss/arm", S.x, S.y);
    S.arm_alive[i] = S.arm[i] != 0;
  }
  S.phase = P_ENTER;
  S.t = S.cool = S.seq = 0;
  R32(boss, B_STATE) = 1;
  BH_PausePartition(BH, 1);
  PlaceAll();
  R32(boss, B_MAXHP) = SfHealth(boss);         // a full bar: every arm
  // "Finally, a good chance to get data on the entropy engine."
  Say();
}

// The arm that faces the player after the shortest turn, and that turn.
static int NearestArm(s32* turn) {
  int best = -1;
  s32 bestd = TURN;
  for (int i = 0; i < NARMS; i++) {
    if (!S.arm_alive[i]) continue;
    s32 d = Wrap(DOWN - ArmAngle(i));
    s32 ad = d < 0 ? -d : d;
    if (ad < bestd) { bestd = ad; best = i; *turn = d; }
  }
  return best;
}

static int ArmsLeft(void) {
  int n = 0;
  for (int i = 0; i < NARMS; i++) n += S.arm_alive[i];
  return n;
}

// The pinwheel: every arm left fires a stream straight out along itself
// while the starfish spins up and slows -- curved arms of stars sweeping the
// screen. The arms' script (enemies.py, command 6) emits along R13 each
// step; the code turns R13 with the arm.
#define PINWHEEL_TIME 230
static void StartPinwheel(void) {
  S.pinwheel = PINWHEEL_TIME;
  for (int i = 0; i < NARMS; i++)
    if (S.arm_alive[i]) Order(S.arm[i], CMD_PINWHEEL, ArmAngle(i));
  Order(S.core, CMD_BEAM, DOWN);               // and a laser from the hub
}

static void Pinwheel(void) {
  const int t = PINWHEEL_TIME - S.pinwheel;
  // spin up over a second, hold, then slow -- in one direction, then the
  // other every other time
  // ... and slows to the speed it turns at between attacks, so the turn
  // after it carries straight on (UpdateArms, S.settle)
  s32 v = DEG(1.6);
  if (t < 50) v = DEG(1.6) * t >> 6;
  if (S.pinwheel < 32) v = ROT_SPEED + ((DEG(1.6) - ROT_SPEED) * S.pinwheel >> 5);
  const s32 dv = (S.dir & 1) ? -v : v;
  S.rot += dv;
  for (int i = 0; i < NARMS; i++)
    if (S.arm_alive[i]) SetReg(S.arm[i], REG_AIM, ArmAngle(i));
  if (--S.pinwheel == 0) {
    S.settle = (S.dir & 1) ? -ROT_SPEED : ROT_SPEED;
    S.dir++;
    for (int i = 0; i < NARMS; i++)
      if (S.arm_alive[i]) SetReg(S.arm[i], REG_CMD, FX(-1));   // stop
    SetReg(S.core, REG_CMD, FX(-1));
    S.cool = 40;
  }
}

static void UpdateArms(void) {
  // arms lost since last frame
  for (int i = 0; i < NARMS; i++) {
    if (S.arm_alive[i] && !Alive(S.arm[i])) {
      S.arm_alive[i] = 0;
      ShakeWorld();
      S.cool = 50;
    }
  }
  const int left = ArmsLeft();
  if (left < NARMS && !S.said_first) { S.said_first = 1; Say(); }   // intrigued
  if (left <= 2 && !S.said_angry) { S.said_angry = 1; Say(); }      // angry
  if (!left) {
    // "Activating all terrain submersible mode."
    Say();
    S.phase = P_CHANGE;
    S.t = 0;
    if (S.pinwheel) SetReg(S.core, REG_CMD, FX(-1));
    S.pinwheel = 0;
    S.settle = 0;
    return;
  }
  // a gentle sway while it fights -- the pinwheel too, or the starfish
  // would jump back into the sway when it ended
  // (counted from the start of the fight, so it starts from the middle)
  S.sway++;
  S.x = FX(HOME_X) + SF_MulFP(SF_Sin((s32)(S.sway << 15)), FX(34));
  if (S.pinwheel > 0) { Pinwheel(); return; }
  int busy = 0;
  for (int i = 0; i < NARMS; i++)
    if (S.arm_alive[i] && Busy(S.arm[i])) busy = 1;
  if (busy) return;                           // hold still while firing
  if (InCutscene()) return;                   // let him finish talking
  s32 turn = 0;
  const int a = NearestArm(&turn);
  if (a < 0) return;
  if (S.settle) {
    // after a pinwheel: keep turning the way it spun, at the speed it slowed
    // to, until an arm faces the ship -- never back the other way
    if (turn > ROT_SPEED || turn < -ROT_SPEED) { S.rot += S.settle; return; }
    S.settle = 0;
  }
  if (turn > ROT_SPEED || turn < -ROT_SPEED) {
    S.rot += turn > 0 ? ROT_SPEED : -ROT_SPEED;
    return;
  }
  S.rot += turn;
  if (S.cool > 0) { S.cool--; return; }
  // All five on: hail, starbursts and missiles. One lost: the lasers join.
  // Two lost: the pinwheel -- every arm left streams while the starfish
  // spins. Four lost: the curtain -- walls of stars with one gap.
  static const unsigned char kFull[] = { CMD_HAIL, CMD_STAR, CMD_MISSILES };
  static const unsigned char kHurt[] = { CMD_HAIL, CMD_LASER, CMD_STAR, CMD_MISSILES, CMD_LASER, CMD_STAR };
  static const unsigned char kTwo[] = { CMD_HAIL, CMD_LASER, CMD_PINWHEEL, CMD_STAR, CMD_MISSILES, CMD_LASER, CMD_PINWHEEL };
  static const unsigned char kFour[] = { CMD_CURTAIN, CMD_LASER, CMD_PINWHEEL, CMD_HAIL, CMD_CURTAIN, CMD_STAR, CMD_LASER, CMD_MISSILES };
  // hard mode: the original bosses' two-stage attacks woven through each stage
  static const unsigned char kFullH[] = { CMD_HAIL, CMD_PINCER, CMD_STAR, CMD_SNAKE, CMD_MISSILES, CMD_TWIST };
  static const unsigned char kHurtH[] = { CMD_PINCER, CMD_LASER, CMD_TWIST, CMD_MISSILES, CMD_SNIPE, CMD_LASER, CMD_STAR };
  static const unsigned char kTwoH[] = { CMD_HAIL, CMD_LASER, CMD_PINWHEEL, CMD_SNIPE, CMD_PINCER, CMD_LASER, CMD_PINWHEEL,
                                         CMD_TWIST };
  static const unsigned char kFourH[] = { CMD_CURTAIN, CMD_LASER, CMD_PINWHEEL, CMD_PINCER, CMD_CURTAIN, CMD_SNIPE,
                                          CMD_LASER, CMD_GAP };
  const int lost = NARMS - left;
  const int hard = Hard();
  const unsigned char* list;
  int n;
  if (lost >= 4) { list = hard ? kFourH : kFour; n = hard ? sizeof kFourH : sizeof kFour; }
  else if (lost >= 2) { list = hard ? kTwoH : kTwo; n = hard ? sizeof kTwoH : sizeof kTwo; }
  else if (lost >= 1) { list = hard ? kHurtH : kHurt; n = hard ? sizeof kHurtH : sizeof kHurt; }
  else { list = hard ? kFullH : kFull; n = hard ? sizeof kFullH : sizeof kFull; }
  // a new pattern opens with itself
  if (lost != S.last_lost) {
    if (lost == 2 || lost == 4) S.seq = lost == 4 ? 0 : 2;
    S.last_lost = lost;
  }
  if (S.seq >= n) S.seq = 0;                  // no modulo in the blob
  const int cmd = list[S.seq];
  S.seq++;
  S.cool = hard ? 22 : 30;
  if (cmd == CMD_PINWHEEL) { StartPinwheel(); return; }
  Order(S.arm[a], cmd, DOWN);
}

static void PickSpot(void) {
  S.tx = FX(SF_RangedRand(70, 330));
  S.ty = FX(SF_RangedRand(60, 230));
}

// Glide towards the target -- a sixteenth of the way each frame, at most
// `speed` (16.16) on each axis -- like something heavy moving under water.
// 1 when there.
static s32 Step(s32 d, s32 speed) {
  s32 s = d >> 4;
  if (s > speed) s = speed;
  if (s < -speed) s = -speed;
  if (s == 0) s = d;
  return s;
}

static int Travel(s32 speed) {
  const s32 dx = S.tx - S.x, dy = S.ty - S.y;
  if (dx < FX(3) && dx > -FX(3) && dy < FX(3) && dy > -FX(3)) return 1;
  S.x += Step(dx, speed);
  S.y += Step(dy, speed);
  return 0;
}

static void StartRush(void) {
  S.phase = P_RUSH;
  S.t = 0;
  S.rush_base = FX(SF_RangedRand(0, 255));
  if (!S.said_rush) { S.said_rush = 1; Say(); }                     // manic
}

// Is the ship on an arm? Its centre against the arm's shape: along the axis
// between root and tip, across it within the arm's half width there (it
// tapers), each with the ship's margin.
static void RushHits(void) {
  if (S.rush_grace > 0) { S.rush_grace--; return; }
  const u32 sh = Shogun();
  if (*(volatile unsigned short*)(sh + SH_HITLOCK) || R32(sh, SH_HITLOCK2)) return;
  const s32 px = R32(sh, SH_PLAYER_X), py = R32(sh, SH_PLAYER_Y);
  for (int i = 0; i < RUSH_N; i++) {
    const u32 arm = S.rush_arm[i];
    if (!arm || S.rush_struck[i] || S.frame - S.rush_born[i] > RUSH_LIFE) continue;
    // under water it is only a shadow: harmless until it surfaces
    if (AM_GetActorRegisterValue(arm, REG_ANIM) < FX(1)) continue;
    s32 ax, ay;
    SF_GetActorPosition(arm, &ax, &ay);
    const s32 dx = px - ax, dy = py - ay;
    if (dx > FX(80) || dx < -FX(80) || dy > FX(80) || dy < -FX(80)) continue;
    const s32 c = SF_Cos(S.rush_aim[i]), s = SF_Sin(S.rush_aim[i]);
    const s32 along = SF_MulFP(dx, c) + SF_MulFP(dy, s);
    s32 across = SF_MulFP(dy, c) - SF_MulFP(dx, s);
    if (across < 0) across = -across;
    if (along < -FX(RUSH_ROOT + RUSH_MARGIN) || along > FX(RUSH_TIP + RUSH_MARGIN)) continue;
    // the half width where the ship is along it, held to the root's and tip's
    s32 from_root = along + FX(RUSH_ROOT);
    if (from_root < 0) from_root = 0;
    if (from_root > FX(RUSH_ROOT + RUSH_TIP)) from_root = FX(RUSH_ROOT + RUSH_TIP);
    const s32 half_w = FX(RUSH_ROOT_W) -
        SF_MulFP(from_root, FX(RUSH_ROOT_W - RUSH_TIP_W) / (RUSH_ROOT + RUSH_TIP));
    if (across > half_w + FX(RUSH_MARGIN)) continue;
    StrikePlayer(sh + SH_PLAYER, 0, sh + SH_HUD);   // 0: a bullet's strike
    S.rush_struck[i] = 1;
    S.rush_grace = RUSH_GRACE;
    return;
  }
}

static void RushStep(void) {
  SetReg(S.core, REG_ANIM, FX(ANIM_UNDER));
  S.rot += DEG(1.5);
  // the first rush comes with his line: the arms wait until he has said it
  // (the ship can't be hurt while he talks)
  if (InCutscene()) return;
  S.t++;
  for (int i = 0; i < RUSH_N; i++) {
    if (S.t != 1 + i * RUSH_GAP) continue;
    // where the ship is now: this arm's mark
    const u32 sh = Shogun();
    const s32 px = R32(sh, SH_PLAYER_X), py = R32(sh, SH_PLAYER_Y);
    const s32 a = (S.rush_base + FIFTH * i + FIFTH / 2 * (i & 1)) & (TURN - 1);
    s32 ax = px + SF_MulFP(SF_Cos(a), FX(RUSH_RADIUS));
    s32 ay = py + SF_MulFP(SF_Sin(a), FX(RUSH_RADIUS));
    if (ax < FX(20)) ax = FX(20);
    if (ax > FX(380)) ax = FX(380);
    if (ay < FX(10)) ay = FX(10);
    if (ay > FX(390)) ay = FX(390);
    const s32 aim = SF_GetAngle(ax >> 16, ay >> 16, px >> 16, py >> 16);
    const u32 e = BH_SpawnGenericEntity(BH, "*beach/boss/arm_rush", ax, ay);
    const u32 arm = e ? SF_GetGenericActor(e) : 0;
    S.rush_arm[i] = arm;
    S.rush_aim[i] = aim;
    S.rush_born[i] = S.frame;
    S.rush_struck[i] = 0;
    if (!arm) continue;
    AM_SetActorPosition(arm, ax, ay);
    AM_SetActorOrientation(arm, aim & (TURN - 1));
    AM_SetActorRegisterValue(arm, REG_AIM, aim & (TURN - 1));
    Splash(ax, ay);
  }
  if (S.t >= RUSH_N * RUSH_GAP + RUSH_TAIL) {
    PickSpot();
    S.phase = P_SUBMERGED;
    S.t = 0;
  }
}

static void SfUpdate(u32 boss) {
  S.frame++;
  SpeechPump();
  if (S.phase != P_DEAD && !Alive(S.core)) {
    S.phase = P_DEAD;
    R32(boss, B_STATE) = 0;                   // the engine calls death next
    return;
  }
  // The field is not a flag: negative is vulnerable, above 18 invincible,
  // and 0..18 means "while script register n is non-zero" -- 0 would tie it
  // to R0, the bullet speed, and the hub could never be hurt.
  // Cutscenes -- his lines, the entrance, the change -- are a break: no
  // one can be hurt.
  const int scene = InCutscene() || S.phase == P_ENTER || S.phase == P_CHANGE;
  ShipShield(scene);
  BH_SetBadGuyInvincibility(S.core, S.phase == P_UP && !scene ? -1 : 255);
  for (int i = 0; i < NARMS; i++)
    if (S.arm_alive[i]) BH_SetBadGuyInvincibility(S.arm[i], scene ? 255 : -1);

  switch (S.phase) {
  case P_ENTER:
    S.y += FX(1) + (FX(1) >> 1);
    S.rot += DEG(1.2);
    if (S.y >= FX(HOME_Y)) { S.y = FX(HOME_Y); S.phase = P_ARMS; S.cool = 40; }
    break;
  case P_ARMS:
    UpdateArms();
    break;
  case P_CHANGE:
    S.t++;
    S.rot += DEG(3);
    SetReg(S.core, REG_ANIM, FX(S.t < 50 ? ANIM_CHARGE : ANIM_UNDER));
    if (S.t == 50) { Splash(S.x, S.y); ShakeWorld(); }
    if (S.t >= 90) {
      PickSpot(); S.phase = P_SUBMERGED; S.t = 0;
      S.p2max = BH_GetBadGuyHealthSF(S.core);
      R32(boss, B_MAXHP) = S.p2max;          // the bar refills: the hub's
    }
    break;
  case P_SUBMERGED:
    SetReg(S.core, REG_ANIM, FX(ANIM_UNDER));
    S.rot += DEG(0.7);
    if (Travel(FX(2) + (FX(1) >> 2))) {
      S.phase = P_BREACH; S.t = 0;
      Splash(S.x, S.y);
    }
    break;
  case P_BREACH:
    SetReg(S.core, REG_ANIM, FX(ANIM_BREACH));
    if (++S.t >= 14) {
      S.phase = P_UP; S.t = 0;
      SetReg(S.core, REG_ANIM, FX(ANIM_IDLE));
      S.attacks_left = Hard() ? 3 : 2;
      S.cool = 10;
    }
    break;
  case P_UP:
    S.rot += DEG(0.3);
    if (Busy(S.core)) break;
    if (InCutscene()) break;
    if (S.cool > 0) { S.cool--; break; }
    if (S.attacks_left > 0) {
      static const unsigned char kSub[] = { CMD_RING, CMD_MISSILES, CMD_STAR, CMD_HAIL, CMD_LASER, CMD_RING, CMD_STAR, CMD_LASER };
      static const unsigned char kSubH[] = { CMD_SNIPE, CMD_MISSILES, CMD_TWIST, CMD_GAP, CMD_LASER, CMD_PINCER, CMD_STAR,
                                             CMD_SNAKE };
      if (S.seq >= 8) S.seq = 0;              // no modulo in the blob
      Order(S.core, Hard() ? kSubH[S.seq] : kSub[S.seq], DOWN);
      S.seq++;
      S.attacks_left--;
      S.cool = Hard() ? 12 : 16;
    } else {
      S.phase = P_DIVE; S.t = 0;
      Splash(S.x, S.y);
    }
    break;
  case P_DIVE:
    SetReg(S.core, REG_ANIM, FX(ANIM_BREACH));
    if (++S.t >= 10) {
      // every other dive, the arms come instead of the hub -- once he has
      // lost half the hub's health (hard mode too: not from the start)
      const int hurt = BH_GetBadGuyHealthSF(S.core) * 2 <= S.p2max;
      // hard mode: on every dive once it is below 30%
      const int desperate = Hard() && BH_GetBadGuyHealthSF(S.core) * 10 <= S.p2max * 3;
      if (desperate || (hurt && (++S.dives & 1))) StartRush();
      else { PickSpot(); S.phase = P_SUBMERGED; }
    }
    break;
  case P_RUSH:
    RushStep();
    break;
  }
  if (!scene) RushHits();
  PlaceAll();
}

static void SfDeath(u32 boss) {
  (void)boss;
  ShipShield(0);
  BH_PausePartition(BH, 0);
  ShakeWorld();
  BH_SpawnExplosion(BH, "ExplosionXL", S.x, S.y);
  // Lines are a strict sequence: if it died before its first rush, the
  // rush line is still ahead of the last words -- step over it.
  if (!S.said_rush) { S.said_rush = 1; g_say_pending++; }
  // "Fascinating... Thank you for the data! ..." (three sentences)
  SayNow();
  S.core = 0;
}

void StarfishBossInit(u32 boss) {
  SF_ClearBytes((void*)boss, BOSS_SIZE);
  SF_ClearBytes(&S, sizeof S);
  R32(boss, B_SPAWN + 0x00) = (s32)(u32)SfSpawn;
  R32(boss, B_SPAWN + 0x04) = (s32)(u32)SfUpdate;
  R32(boss, B_SPAWN + 0x08) = (s32)(u32)SfRender;
  R32(boss, B_SPAWN + 0x0c) = (s32)(u32)SfRender;
  R32(boss, B_SPAWN + 0x10) = (s32)(u32)SfDeath;
  R32(boss, B_HEALTH) = (s32)(u32)SfHealth;
}
