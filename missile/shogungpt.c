// shogungpt.c -- Mission 8's final boss: ShogunGPT, against the clock.
//
// Dr. Marrow's AI, the Shogun's favourite chatbot: it knows his whole
// routine. The Doc wants it captured, not destroyed -- its data, before the
// Shogun repairs his dreadnought -- and ShogunGPT opens a support ticket
// with the Shogun the moment it sees the ship. The bar under it (the
// uplink, a generic) fills as its distress call goes out; the fight has
// TIMER frames of it (cutscenes don't count).
//
//   enter     it descends over the Castle, helmet first; "How can I help
//             you today?"; the Doc: capture it; the ticket is opened;
//   heads     four attention heads orbit it and stare at the ship -- streams,
//             fans of thistledown, slow rings. It cannot be hurt while any
//             of them is left (the bar: the heads);
//   reason    heads gone, "Switching to reasoning mode": it can be hurt
//             (the bar refills: its own). Tokens streaming out in spirals,
//             the context window (a ring with one gap, round on the ship),
//             beam search (a swept laser), hallucination (glyphs anywhere),
//             missiles. Half-way through the call, and near its end, a line;
//   captured  brought down to its floor before the call gets through: it
//             shuts down sparking, the Doc downloads everything, and the
//             level ends -- the ship escapes;
//   too late  the call goes through. "The Shogun has joined the chat." It
//             rises out of reach, and the Kurogane -- the chase's
//             dreadnought, half repaired -- climbs into the bottom of the
//             screen: "You will not get away this time." It turns every gun
//             it has on the ship, again and again, until the ship is
//             destroyed (chase.c DoomStart, DoomUpdate). There is no
//             escape -- unless the ship lasts DOOM_SURVIVE frames of it. Then
//             the Doc has part of ShogunGPT's data after all: access codes
//             that jam the Kurogane, once (he will patch them). Its guns go
//             silent and it sinks away; the Shogun vows that the third time
//             he will be prepared; the ship escapes (G_JAM, the secret).
//
// The core's health is far more than the fight takes; the code lowers it to
// a floor and treats that as captured (as twins.c does its titans).
//
// Its parts (edinburgh/art_bosses.py): two shoulder turrets, badguys of their
// own that track the ship and fire bursts when told (and can be shot off);
// its face, a generic on the screen in its helmet, which shows how it is
// doing; and now and then, over the face, a reply it types out -- one of
// twelve, at random (gpt_text0..2, four to a sheet, three frames each). The
// heads turn their lens on the ship.

#include "seoul_boss.h"
#include "edinburgh_lines.h"

void EdLine(int k);
void EdinburghBossCame(void);   // edinburgh.c
void EdLineNow(int k);
void DoomStart(void);             // chase.c: the Kurogane comes ...
void DoomUpdate(void);            // ... and never stops
void DoomJam(void);               // ... unless the access codes jam it
int DoomGone(void);

// the dispatcher's commands (edinburgh/edin_cast.py CMDS)
enum { E_CLOCK = 1, E_CALENDAR, E_REMIND, E_TICKTOCK, E_ALARM, E_STREAM, E_FAN, E_HALO, E_TOKENS, E_SEARCH,
       E_DREAM, E_CONTEXT, E_MISSILES, E_SNOOZE, E_ATTEND, E_TURN, E_ROSETTE,
       E_XSNIPE, E_XPINCER, E_XTWIST, E_XSNAKE, E_XGAP };       // hard mode's (beach/hardkit.py)
// Its windows (the original bosses' timing: docs/BOSS_REVAMP_M8_M11.md): one
// layer that lays lanes and one that makes the ship move, a beat of quiet
// after. The heads: a stare and a fan from opposite heads; attention (commas
// folding onto the ship) over the core's hanging rings; reminders that come
// back from two heads; rings that turn a quarter at once. Reasoning: tokens
// with a turret's bursts; the context window sliding; beam search; the
// hallucination; snooze; missiles; and, hurt, parallel processing (the
// rosette). The turrets fire when a window says, no longer on a clock of
// their own.
enum { HW_STARE = 1, HW_ATTEND, HW_LOCK, HW_TURN,
       RW_TOKENS, RW_CONTEXT, RW_SEARCH, RW_DREAM, RW_SNOOZE, RW_MISSILES, RW_ROSE,
       HW_XPINCER, HW_XTWIST, RW_XSNIPE, RW_XGAP, RW_XSNAKE, RW_XTWIST };
static const unsigned char kPlanHeads[] = { HW_STARE, HW_ATTEND, HW_LOCK, HW_TURN };
static const unsigned char kPlanThink[] = { RW_TOKENS, RW_CONTEXT, RW_SEARCH, RW_DREAM, RW_SNOOZE, RW_MISSILES };
static const unsigned char kPlanRush[] = { RW_TOKENS, RW_ROSE, RW_CONTEXT, RW_SEARCH, RW_DREAM, RW_ROSE, RW_SNOOZE,
                                           RW_MISSILES };
// Hard mode (docs/HARD_MODE.md): windows of its own woven in -- heads closing
// pincers and snipe rings under the face's fire, the core's twist rings with
// reminders over them; reasoning, snipe rings, drifting-gap rings and the
// snake, each with the face and a turret -- and the beats a third shorter.
static const unsigned char kPlanHeadsH[] = { HW_STARE, HW_XPINCER, HW_ATTEND, HW_LOCK, HW_XTWIST, HW_TURN };
static const unsigned char kPlanThinkH[] = { RW_TOKENS, RW_XSNIPE, RW_CONTEXT, RW_SEARCH, RW_XGAP, RW_DREAM,
                                             RW_SNOOZE, RW_XSNAKE, RW_MISSILES };
static const unsigned char kPlanRushH[] = { RW_TOKENS, RW_ROSE, RW_XTWIST, RW_CONTEXT, RW_XSNAKE, RW_SEARCH, RW_ROSE,
                                            RW_XGAP, RW_DREAM, RW_XSNIPE, RW_MISSILES };
// the core's animations (running, hurt), the heads' and turrets' frames, and
// the face's (edinburgh/art_bosses.py FACES)
enum { GA_RUN, GA_HURT };
enum { HA_IDLE, HA_FIRE, HA_HURT };
// the face's moods (edin_cast.py gpt_screen: each its own animation, never
// more than eight -- a ninth fails an engine assertion that clears the touch)
enum { F_IDLE, F_THINK, F_FIRE, F_ERROR, F_HURT, F_SMUG, F_TYPING, F_STATIC };
enum { G_ENTER = 1, G_HEADS, G_REASON, G_CAPTURED, G_FAIL, G_DOOM, G_JAM };

#define TIMER      6000         // frames of fight before the call goes through (100 s)
#define TIMER_HALF 3000
#define TIMER_LATE 5100
#define SEGMENTS   12           // the uplink bar
#define SEG_K      1223         // 7 / TIMER in 2^-20: the bar's eight steps (no division)
#define BAR_STEPS  7
#define CORE_HP    13000        // what the reasoning takes
#define NHEADS     4
#define HEAD_HP    2000         // (edinburgh/edin_cast.py: the head's whole health)
#define HOME_Y     96
#define ORBIT_R    72
#define UPLINK_DY  84           // the bar, under the monolith
#define SCREEN_DY  (-4)         // the face, from the core's centre
#define TURRET_DX  62           // the shoulder turrets
#define TURRET_DY  (-6)
#define NTURRETS   2
#define TURRET_HP  1500         // (edinburgh/edin_cast.py)
#define NSAYINGS   12           // replies: four to a text sheet, three frames each
#define SAY_TYPE   10           // frames a third of a reply takes to type
#define SAY_HOLD   150          // ... and it stays this long
// The endings wait for the talk to finish -- unless the dialog has gone this
// long without changing at all: a capture right on top of a line (the plasma
// and shield made it quick) could leave it out of step, and the level waited
// for it for ever.
#define TALK_WAIT  900
#define DOOM_SURVIVE 3600       // frames of the doom (talk not counted) for the secret

static struct {
  u32 boss;
  u32 core, head[NHEADS], uplink;
  u32 turret[NTURRETS], screen, text;
  u32 tvis[NTURRETS];             // what is seen of each turret, over the body
  int turret_cool, turret_turn;
  int say_t, say_cool, said_last, face_t;
  int doom_t, jam_step;           // the secret: frames survived; how far the jam has got
  s32 x, y, floor_;
  s32 orbit;
  int phase, t, cool, turn, head_turn, head_cool, halo_cool;
  int elapsed;                  // frames of the call so far
  int said_half, said_late, said_heads;
  int quiet;
  int win, wt, beat, step, rushed;
} G;

static s32 Hp(u32 bg) { return SB_GetBadGuyHealth(bg); }

static u32 GenericAt(const char* name, s32 x, s32 y) {
  const u32 e = SB_SpawnGeneric(BH, name, x, y);
  const u32 a = e ? SB_GetGenericActor(e) : 0;
  if (a) {
    SB_SetActorPosition(a, x, y);
    SB_SetActorRegister(a, REG_CMD, 0);
  }
  return a;
}

static int HeadsLeft(void) {
  int n = 0;
  for (int i = 0; i < NHEADS; i++) if (Alive(G.head[i])) n++;
  return n;
}

static s32 GpHealth(u32 boss) {
  (void)boss;
  if (G.phase == G_HEADS) {
    s32 sum = 0;
    for (int i = 0; i < NHEADS; i++)
      if (Alive(G.head[i])) sum += Hp(G.head[i]);
    return sum;
  }
  if (G.phase == G_REASON) {
    const s32 left = Hp(G.core) - G.floor_;
    return left < 0 ? 0 : left;
  }
  return 0;
}

static void GpRender(u32 boss, s32 x, s32 y) { (void)boss; (void)x; (void)y; }

static void GpSpawn(u32 boss) {
  EdinburghBossCame();
  SB_ClearBytes(&G, sizeof G);
  G.boss = boss;
  G.x = FX(200);
  G.y = FX(-120);
  G.core = SB_SpawnBadGuy(BH, "*edinburgh/boss/gpt", G.x, G.y);
  G.floor_ = Hp(G.core) - CORE_HP;
  for (int i = 0; i < NHEADS; i++) G.head[i] = SB_SpawnBadGuy(BH, "*edinburgh/boss/head", G.x, G.y);
  for (int i = 0; i < NTURRETS; i++)
    G.turret[i] = SB_SpawnBadGuy(BH, "*edinburgh/boss/gpt_turret", G.x + FX(i ? TURRET_DX : -TURRET_DX), G.y);
  G.uplink = GenericAt("*edinburgh/boss/uplink", G.x, G.y + FX(UPLINK_DY));
  G.screen = SpawnPart("*edinburgh/boss/gpt_screen", G.x, G.y + FX(SCREEN_DY));
  for (int i = 0; i < NTURRETS; i++)
    G.tvis[i] = SpawnPart("*edinburgh/boss/gpt_turret_vis", G.x + FX(i ? TURRET_DX : -TURRET_DX), G.y);
  G.turret_cool = 120;
  G.say_cool = 300;
  G.said_last = -1;
  G.phase = G_ENTER;
  G.head_cool = 60;
  G.halo_cool = 200;
  R32(boss, B_STATE) = 1;
  SB_PausePartition(BH, 1);
  R32(boss, B_MAXHP) = NHEADS * HEAD_HP;
  g_autopilot_target = G.head[0];
  EdLine(L_GPT_HELLO);              // "How can I help you today?"
  EdLine(L_GPT_DOC);                // the Doc: "Capture it!"
  EdLine(L_GPT_TICKET);             // "Estimated response time: one hundred seconds."
}

// ---- movement --------------------------------------------------------------------------
static void Float(int steady) {
  if (!steady) G.t++;
  const s32 wantx = FX(200) + SB_MulFP(SB_Sin((s32)(G.t << 14)), FX(40));
  const s32 wanty = FX(HOME_Y) + SB_MulFP(SB_Sin((s32)(G.t << 15)), FX(8));
  G.x += Ease(wantx - G.x, FX(1.0));
  G.y += Ease(wanty - G.y, FX(0.8));
}

static void PlaceAll(void) {
  Place(G.core, G.x, G.y);
  G.orbit = (G.orbit + (TURN >> 9)) & (TURN - 1);          // a slow turn
  for (int i = 0; i < NHEADS; i++) {
    if (!Alive(G.head[i])) continue;
    const s32 a = (G.orbit + i * (TURN / NHEADS)) & (TURN - 1);
    const s32 hx = G.x + SB_MulFP(SB_Cos(a), FX(ORBIT_R)), hy = G.y + SB_MulFP(SB_Sin(a), FX(ORBIT_R));
    Place(G.head[i], hx, hy);       // (its script turns the lens on the ship: edin_cast.py boss_script)
  }
  for (int i = 0; i < NTURRETS; i++) {
    const s32 tx = G.x + FX(i ? TURRET_DX : -TURRET_DX), ty = G.y + FX(TURRET_DY);
    if (!Alive(G.turret[i])) { DropPart(&G.tvis[i]); continue; }
    Place(G.turret[i], tx, ty);
    // the visible turret takes the badguy's own aim (its script's Target
    // Angle), copied as stored (actor+0x1a): SB_SetActorOrient adds a
    // quarter turn, a script doesn't, and the art faces down for the script
    PlacePart(G.tvis[i], tx, ty);
    if (G.tvis[i]) *(volatile short*)(G.tvis[i] + 0x1a) = *(volatile short*)(Act(G.turret[i]) + 0x1a);
  }
  if (G.uplink) SB_SetActorPosition(G.uplink, G.x, G.y + FX(UPLINK_DY));
  PlacePart(G.screen, G.x, G.y + FX(SCREEN_DY));
  PlacePart(G.text, G.x, G.y + FX(SCREEN_DY));
}

// ---- the turrets ---------------------------------------------------------------------------
static int CoreHurt(void) { return G.phase == G_REASON && (Hp(G.core) - G.floor_) * 2 < CORE_HP; }

static void Turrets(int scene) {
  for (int i = 0; i < NTURRETS; i++) {
    const u32 t = G.turret[i];
    if (!Alive(t)) continue;
    Guard(t, scene || G.phase >= G_FAIL || G.phase == G_ENTER);
    SetReg(t, 15, CoreHurt() ? FX(1) : 0);               // a spread, once it is hurt
    const int f = Busy(t) ? HA_FIRE : (Hp(t) * 2 < TURRET_HP ? HA_HURT : HA_IDLE);
    Anim(t, f);
    PartFrame(G.tvis[i], f);
  }
}

// a burst from a shoulder turret, the two taking turns (a window's cue)
static void TurretBurst(void) {
  for (int k = 0; k < NTURRETS; k++) {
    const u32 t = G.turret[(G.turret_turn + k) & 1];
    if (!Alive(t) || Busy(t)) continue;
    Order(t, 1);
    G.turret_turn = ((G.turret_turn + k) & 1) ^ 1;
    return;
  }
}

// ---- its face, and what it says ------------------------------------------------------------
static void Face(int scene) {
  G.face_t++;
  int f = F_IDLE;
  if (G.phase == G_CAPTURED) f = F_ERROR;              // (it flickers to static by itself)
  else if (G.phase == G_FAIL || G.phase == G_DOOM || G.phase == G_JAM) f = F_SMUG;
  else if (scene) f = F_TYPING;
  else if (Busy(G.core)) f = F_FIRE;
  else if (G.phase == G_REASON) f = CoreHurt() && (G.face_t & 64) ? F_HURT : F_THINK;
  PartFrame(G.screen, f);
}

static void SayEnd(void) { DropPart(&G.text); }

// Now and then, while it fights, a reply types itself out over its face.
static void Chatter(int scene) {
  if (G.text) {
    G.say_t++;
    PartFrame(G.text, (G.said_last & 3) * 2 + (G.say_t >= 2 * SAY_TYPE));   // typing, then all of it
    if (G.say_t > 2 * SAY_TYPE + SAY_HOLD || scene) SayEnd();
    return;
  }
  if (scene || (G.phase != G_HEADS && G.phase != G_REASON)) return;
  if (--G.say_cool > 0) return;
  int k = SB_RangedRand(0, NSAYINGS);
  if (k >= NSAYINGS) k = NSAYINGS - 1;
  if (k == G.said_last) k = k + 1 >= NSAYINGS ? 0 : k + 1;
  static const char* const kSheets[3] = { "*edinburgh/boss/gpt_text0", "*edinburgh/boss/gpt_text1",
                                          "*edinburgh/boss/gpt_text2" };
  G.text = SpawnPart(kSheets[k >> 2], G.x, G.y + FX(SCREEN_DY));
  G.said_last = k;
  G.say_t = 0;
  PartFrame(G.text, (k & 3) * 2);
  G.say_cool = SB_RangedRand(360, 600);
}

static void Uplink(void) {
  int seg = (G.elapsed * SEG_K) >> 20;
  if (seg > BAR_STEPS) seg = BAR_STEPS;
  if (G.uplink) SB_SetActorRegister(G.uplink, REG_ANIM, FX(seg));
}

// ---- the fight ----------------------------------------------------------------------------
// the next attention head still up, from head_turn on (they take turns)
static u32 NextHead(void) {
  for (int k = 0; k < NHEADS; k++) {
    const int i = (G.head_turn + k) & (NHEADS - 1);
    if (Alive(G.head[i]) && !Busy(G.head[i])) { G.head_turn = i + 1; return G.head[i]; }
  }
  return 0;
}

static int AllQuiet(void) {
  if (Busy(G.core)) return 0;
  for (int i = 0; i < NHEADS; i++) if (Alive(G.head[i]) && Busy(G.head[i])) return 0;
  return 1;
}

static void Cue(u32 bg, int cmd) { if (bg && Alive(bg)) Order(bg, cmd); }

// The face on its screen fires a layer of its own (edin_cast.py gpt_screen:
// R16 the order, R15 busy): the core runs one attack at a time.
enum { FX_PREDICT = 1, FX_SPOOL, FX_HOVER, FX_SWEEP, FX_RETRY };
static void FaceFire(int what) { if (G.screen) SB_SetActorRegister(G.screen, 16, FX(what)); }
static int FaceBusy(void) {
  return G.screen && (SB_GetActorRegister(G.screen, 15) != 0 || SB_GetActorRegister(G.screen, 16) != 0);
}

// One window at its moment t; 1 when it is over. Three layers in most: what
// lays the lanes, what makes the ship move, and the face's (or a turret's)
// over them, each coming in a moment after the last.
static int Window(int w, int t) {
  const int rush = CoreHurt();
  int done = 1;
  switch (w) {
  case HW_STARE:
    if (t == 0) Cue(NextHead(), E_STREAM);
    if (t == 20) FaceFire(FX_SPOOL);
    if (t == 50) { G.head_turn++; Cue(NextHead(), E_FAN); }       // the head across the ring
    if (t == 110) TurretBurst();
    done = t > 120;
    break;
  case HW_ATTEND:
    if (t == 0) Cue(G.core, E_HALO);
    if (t == 30) Cue(NextHead(), E_ATTEND);
    if (t == 90) TurretBurst();
    done = t > 100;
    break;
  case HW_LOCK:
    if (t == 0) FaceFire(FX_SWEEP);
    if (t == 20) Cue(NextHead(), E_REMIND);
    if (t == 90) { G.head_turn++; Cue(NextHead(), E_REMIND); }
    done = t > 100;
    break;
  case HW_TURN:
    if (t == 0) Cue(G.core, E_TURN);
    if (t == 50) FaceFire(FX_HOVER);
    if (t == 100) TurretBurst();
    if (t == 120) Cue(NextHead(), E_STREAM);
    done = t > 130;
    break;
  case RW_TOKENS:
    if (t == 0) Cue(G.core, E_TOKENS);
    if (t == 40 || t == 170) FaceFire(FX_PREDICT);
    if (t == 80 || t == 210) TurretBurst();
    done = t > 220;
    break;
  case RW_CONTEXT:
    if (t == 0) Cue(G.core, E_CONTEXT);
    if (t == 40) FaceFire(FX_HOVER);
    if (t == 70 || (rush && t == 130)) TurretBurst();
    done = t > 140;
    break;
  case RW_SEARCH:
    if (t == 0) Cue(G.core, E_SEARCH);
    if (t == 10) FaceFire(FX_SPOOL);
    if (t == 60) TurretBurst();
    done = t > 70;
    break;
  case RW_DREAM:
    if (t == 0) Cue(G.core, E_DREAM);
    if (t == 60) FaceFire(FX_PREDICT);
    if (t == 120) TurretBurst();
    done = t > 130;
    break;
  case RW_SNOOZE:
    if (t == 0) FaceFire(FX_SWEEP);
    if (t == 20) Cue(G.core, E_SNOOZE);
    if (rush && t == 90) TurretBurst();
    done = t > 100;
    break;
  case RW_MISSILES:
    if (t == 0) Cue(G.core, E_MISSILES);
    if (t == 10) FaceFire(FX_RETRY);
    if (t == 50) TurretBurst();
    done = t > 60;
    break;
  case HW_XPINCER:
    if (t == 0) FaceFire(FX_HOVER);
    if (t == 20) Cue(NextHead(), E_XPINCER);
    if (t == 70) { G.head_turn++; Cue(NextHead(), E_XSNIPE); }
    if (t == 100) TurretBurst();
    done = t > 110;
    break;
  case HW_XTWIST:
    if (t == 0) Cue(G.core, E_XTWIST);
    if (t == 40) Cue(NextHead(), E_REMIND);
    if (t == 90) TurretBurst();
    done = t > 100;
    break;
  case RW_XSNIPE:
    if (t == 0) Cue(G.core, E_XSNIPE);
    if (t == 30) FaceFire(FX_SWEEP);
    if (t == 70) TurretBurst();
    done = t > 80;
    break;
  case RW_XGAP:
    if (t == 0) Cue(G.core, E_XGAP);
    if (t == 40) FaceFire(FX_PREDICT);
    if (t == 80 || (rush && t == 140)) TurretBurst();
    done = t > 150;
    break;
  case RW_XSNAKE:
    if (t == 0) Cue(G.core, E_XSNAKE);
    if (t == 30) FaceFire(FX_SPOOL);
    if (t == 60) TurretBurst();
    done = t > 70;
    break;
  case RW_XTWIST:
    if (t == 0) Cue(G.core, E_XTWIST);
    if (t == 40) FaceFire(FX_HOVER);
    if (t == 90) TurretBurst();
    done = t > 100;
    break;
  case RW_ROSE:
    if (t == 0) Cue(G.core, E_ROSETTE);
    if (t == 50) FaceFire(FX_PREDICT);
    if (t == 110) TurretBurst();
    done = t > 120;
    break;
  }
  return done && AllQuiet() && !FaceBusy();
}

static void Fight(void) {
  if (G.win) {
    if (Window(G.win, G.wt++)) {
      G.win = 0;
      // the beat: shorter as the heads fall, and once the core is hurt
      G.beat = G.phase == G_HEADS ? 24 + 6 * HeadsLeft() : (CoreHurt() ? 28 : 40);
      if (HardMode()) G.beat = (G.beat * 11) >> 4;
    }
    return;
  }
  if (G.beat > 0) { G.beat--; return; }
  if (G.phase == G_REASON && CoreHurt() && !G.rushed) { G.rushed = 1; G.step = 0; }   // the rush, from its top
  const unsigned char* plan = G.phase == G_HEADS ? kPlanHeads : (CoreHurt() ? kPlanRush : kPlanThink);
  int n = G.phase == G_HEADS ? (int)sizeof kPlanHeads : (CoreHurt() ? (int)sizeof kPlanRush : (int)sizeof kPlanThink);
  if (HardMode()) {
    plan = G.phase == G_HEADS ? kPlanHeadsH : (CoreHurt() ? kPlanRushH : kPlanThinkH);
    n = G.phase == G_HEADS ? (int)sizeof kPlanHeadsH : (CoreHurt() ? (int)sizeof kPlanRushH : (int)sizeof kPlanThinkH);
  }
  if (G.step >= n) G.step = 0;
  G.win = plan[G.step];
  if (++G.step >= n) G.step = 0;
  G.wt = 0;
}

static void StopAll(void) {
  if (Alive(G.core)) Stop(G.core);
  for (int i = 0; i < NHEADS; i++) if (Alive(G.head[i])) Stop(G.head[i]);
  for (int i = 0; i < NTURRETS; i++) if (Alive(G.turret[i])) Stop(G.turret[i]);
  SayEnd();
}

static void Captured(void) {
  G.phase = G_CAPTURED;
  G.t = 0;
  SB_SetBadGuyHealth(G.core, G.floor_ + 1);
  StopAll();
  for (int i = 0; i < NTURRETS; i++) Remove(G.turret[i]);
  R32(G.boss, B_MAXHP) = 0;
  SB_ShakeWorld();
  EdLineNow(L_WIN_GPT);             // "Error 418. I'm a teapot..." (the lines of the other ending go by)
  EdLine(L_WIN_DOC);                // the Doc: "Download complete!"
}

static void TooLate(void) {
  G.phase = G_FAIL;
  G.t = 0;
  StopAll();
  R32(G.boss, B_MAXHP) = 0;
  EdLine(L_FAIL_GPT);               // "Ticket resolved! The Shogun has joined the chat."
}

static void GpUpdate(u32 boss) {
  SpeechPump();
  G.quiet = InCutscene() ? 0 : G.quiet + 1;
  if (g_autopilot_target && !Alive(g_autopilot_target)) g_autopilot_target = 0;
  const int fighting = G.phase == G_HEADS || G.phase == G_REASON;
  const int scene = InCutscene() || !fighting;
  // no hits while anyone talks, nor while it enters or is taken; in the
  // doom, only while someone talks
  ShipShield(G.phase == G_DOOM ? InCutscene() : scene);
  // the core can be hurt only while reasoning; the heads while they fight
  Guard(G.core, scene || G.phase != G_REASON);
  for (int i = 0; i < NHEADS; i++) if (Alive(G.head[i])) Guard(G.head[i], scene);
  Turrets(scene);
  Face(scene);
  Chatter(InCutscene());
  if (fighting) {
    BossWards();
    if (!InCutscene()) G.elapsed++;
    Uplink();
    if (G.elapsed >= TIMER_HALF && !G.said_half) { G.said_half = 1; EdLine(L_GPT_HALF); }
    if (G.elapsed >= TIMER_LATE && !G.said_late) { G.said_late = 1; EdLine(L_GPT_LATE); }
    if (G.elapsed >= TIMER) { TooLate(); return; }
  }
  switch (G.phase) {
  case G_ENTER:
    G.y += FX(1.4);
    if (G.y >= FX(HOME_Y)) {
      G.y = FX(HOME_Y);
      if (G.quiet > 30) {
        G.phase = G_HEADS;
        G.t = 0;
        G.beat = 30;
      }
    }
    break;
  case G_HEADS:
    Float(0);
    if (!InCutscene()) Fight();
    if (!HeadsLeft()) {
      G.phase = G_REASON;
      G.turn = 0;
      G.win = 0;
      G.step = 0;
      G.beat = 60;
      R32(boss, B_MAXHP) = CORE_HP;                   // the bar refills: its own
      g_autopilot_target = G.core;
      if (!G.said_heads) { G.said_heads = 1; EdLine(L_GPT_HEADS); }
    } else if (!Alive(g_autopilot_target)) {
      for (int i = 0; i < NHEADS; i++) if (Alive(G.head[i])) { g_autopilot_target = G.head[i]; break; }
    }
    break;
  case G_REASON:
    Float(0);
    if (!InCutscene()) Fight();
    if (Hp(G.core) <= G.floor_ + 1) Captured();
    break;
  case G_CAPTURED:
    // it shuts down, sparking, sinking a little, while the Doc downloads it
    G.t++;
    G.y += Ease(FX(HOME_Y + 16) - G.y, FX(0.3));
    // a few crackles as it goes down, thinning out, then quiet: an explosion
    // every 16 frames for the whole of the ending's talk was one long roar
    if (G.t == 8 || G.t == 34 || G.t == 70 || G.t == 112 || G.t == 160)
      SB_SpawnExplosion(BH, "ExplosionXS", G.x + FX(SB_RangedRand(-36, 36)), G.y + FX(SB_RangedRand(-40, 40)));
    if (G.t > 240 && (G.quiet > 40 || SpeechStalled() > TALK_WAIT)) {
      G.phase = 0;
      R32(boss, B_STATE) = 0;                         // the engine calls death next
      return;
    }
    break;
  case G_FAIL:
    // it rises out of reach, and the Kurogane comes
    G.t++;
    if (G.t > 50) G.y -= FX(1.2);
    if (G.t > 60 && (G.quiet > 40 || SpeechStalled() > TALK_WAIT)) {
      if (G.uplink) { SB_SetActorRegister(G.uplink, REG_CMD, FX(1)); G.uplink = 0; }
      DoomStart();
      EdLine(L_FAIL_SHOGUN);        // "You will not get away this time."
      EdLine(L_FAIL_DOC);           // the Doc: "Pilot, run!"
      G.phase = G_DOOM;
      G.t = 0;
    }
    break;
  case G_DOOM:
    // ShogunGPT out of sight above; the dreadnought fires everything it has
    // until the ship is gone -- or until it has lasted long enough
    if (G.y > FX(-300)) G.y -= FX(1.2);
    DoomUpdate();
    if (!InCutscene() && ++G.doom_t >= DOOM_SURVIVE) {
      G.phase = G_JAM;
      G.t = 0;
      G.jam_step = 0;
      EdLine(L_SECRET_DOC);         // "We can jam the Kurogane! ... only once."
    }
    break;
  case G_JAM:
    // the Doc sends the codes; the Kurogane goes dead in the air and sinks
    // away; the Shogun's vow; the Doc, and home
    G.t++;
    DoomUpdate();
    if (G.jam_step == 0 && G.t > 60 && (G.quiet > 40 || SpeechStalled() > TALK_WAIT)) {
      DoomJam();
      EdLine(L_SECRET_SHOGUN);      // "The third time, I will be prepared."
      G.jam_step = 1;
      G.t = 0;
    } else if (G.jam_step == 1 && DoomGone() && G.t > 120 && (G.quiet > 40 || SpeechStalled() > TALK_WAIT)) {
      EdLine(L_SECRET_DOC_GO);
      G.jam_step = 2;
      G.t = 0;
    } else if (G.jam_step == 2 && G.t > 60 && (G.quiet > 40 || SpeechStalled() > TALK_WAIT)) {
      G.phase = 0;
      R32(boss, B_STATE) = 0;       // the engine calls death next: the level ends
      return;
    }
    break;
  }
  Anim(G.core, G.phase == G_CAPTURED || CoreHurt() ? GA_HURT : GA_RUN);
  for (int i = 0; i < NHEADS; i++)
    if (Alive(G.head[i])) Anim(G.head[i], Busy(G.head[i]) ? HA_FIRE : (Hp(G.head[i]) * 2 < HEAD_HP ? HA_HURT : HA_IDLE));
  PlaceAll();
}

static void GpDeath(u32 boss) {
  (void)boss;
  ShipShield(0);
  SayEnd();
  PartFrame(G.screen, F_STATIC);    // dark now, but for the static
  if (G.uplink) SB_SetActorRegister(G.uplink, REG_CMD, FX(1));
  G.uplink = 0;
  g_autopilot_target = 0;
  // captured, not destroyed: it stays, dark, while the ship gets away
  SB_PausePartition(BH, 0);
}

void GptBossInit(u32 boss) {
  SB_ClearBytes((void*)boss, BOSS_SIZE);
  SB_ClearBytes(&G, sizeof G);
  R32(boss, B_SPAWN + 0x00) = (s32)(u32)GpSpawn;
  R32(boss, B_SPAWN + 0x04) = (s32)(u32)GpUpdate;
  R32(boss, B_SPAWN + 0x08) = (s32)(u32)GpRender;
  R32(boss, B_SPAWN + 0x0c) = (s32)(u32)GpRender;
  R32(boss, B_SPAWN + 0x10) = (s32)(u32)GpDeath;
  R32(boss, B_HEALTH) = (s32)(u32)GpHealth;
}
