// subagent.c -- Mission 8's mid-boss: Scheduler, a subagent of ShogunGPT.
//
// The Shogun's calendar, given a body: a flying clock (edinburgh/boss/
// scheduler, scripted in edinburgh/edin_cast.py), placed every frame from here,
// its script firing when told to. Over it, its moving parts (generics,
// edinburgh/art_bosses.py): the pendulum swinging below, the alarm bells on
// top (their hammer rattles once it is hurt), and the hands, which tick
// round -- and spin, in the alarm. The scroll stops for it over Calton Hill,
// the National Monument's columns and the Nelson Monument below it
// (edinburgh/edin_timeline.py).
//
//   enter    it drops in from the top and settles, ticking;
//   hover    it sways about the top of the screen and keeps its schedule:
//            the clock's hands (two streams sweeping round), reminders (fans
//            at the ship), the week (walls with a day free), tick-tock (rings
//            turning on). Every fourth appointment it glides to a new spot;
//   hurt     below half, the alarm joins: missiles and a ring;
//   death    "Escalating to the orchestrator..." -- ShogunGPT comes next
//            (shogungpt.c).
//
// Gentle, like the level: slow glyphs, all of it to be grazed.

#include "seoul_boss.h"
#include "edinburgh_lines.h"

void GptBossInit(u32 boss);       // shogungpt.c: the final boss
void EdinburghReset(void);        // edinburgh.c: a new run of the level
void EdLine(int k);
void EdinburghBossCame(void);   // edinburgh.c

// the dispatcher's commands (edinburgh/edin_cast.py CMDS)
enum { E_CLOCK = 1, E_CALENDAR, E_REMIND, E_TICKTOCK, E_ALARM, E_STREAM, E_FAN, E_HALO, E_TOKENS, E_SEARCH,
       E_DREAM, E_CONTEXT, E_MISSILES, E_SNOOZE, E_ATTEND, E_TURN, E_ROSETTE,
       E_XSNIPE, E_XPINCER, E_XTWIST, E_XSNAKE, E_XGAP };       // hard mode's (beach/hardkit.py)
// Its windows (the original bosses' timing: docs/BOSS_REVAMP_M8_M11.md): the
// hands sweep their streams -- the lanes -- while the clock sends reminders
// that come back for the ship; the week's walls alone; tick-tock, rings that
// turn on the tock; snooze, a ring that hangs and then wakes; a glide to a new
// spot. Hurt, the alarm joins. A beat of quiet after each.
enum { SW_HANDS = 1, SW_WEEK, SW_TICK, SW_SNOOZE, SW_GLIDE, SW_ALARM,
       SW_XSNIPE, SW_XPINCER, SW_XTWIST, SW_XSNAKE, SW_XGAP };
static const unsigned char kPlanCalm[] = { SW_HANDS, SW_WEEK, SW_GLIDE, SW_TICK, SW_SNOOZE, SW_GLIDE };
static const unsigned char kPlanHurt[] = { SW_HANDS, SW_ALARM, SW_WEEK, SW_GLIDE, SW_TICK, SW_ALARM, SW_SNOOZE,
                                           SW_GLIDE };
// Hard mode (docs/HARD_MODE.md): windows of its own between them -- the hands'
// lanes with snipe rings or pincers over them, the week's walls with twist
// rings, the snake under the reminders, drifting-gap rings alone -- and the
// beat a third shorter.
static const unsigned char kPlanCalmH[] = { SW_HANDS, SW_XSNIPE, SW_WEEK, SW_GLIDE, SW_XPINCER, SW_TICK, SW_XGAP,
                                            SW_GLIDE };
static const unsigned char kPlanHurtH[] = { SW_XPINCER, SW_ALARM, SW_XTWIST, SW_GLIDE, SW_XSNAKE, SW_XSNIPE, SW_ALARM,
                                            SW_XGAP, SW_GLIDE };
enum { SA_TICK, SA_FIRE, SA_ALARM };
enum { S_ENTER = 1, S_HOVER, S_GLIDE, S_DEAD };

#define HOME_Y 96
// the parts, from the clock's centre (edinburgh/art_bosses.py)
#define HANDS_DY      2
#define PENDULUM_DY   69
#define BELLS_DY      (-58)

static struct {
  u32 body;
  u32 hands, pendulum, bells;     // the moving parts
  s32 x, y, tx;
  int phase, t, cool, turn, glide_in;
  s32 maxhp;
  int win, wt, step, alarmed;
} SC;   // (not S: the starfish has one, and probes find these by name)

static s32 SgHealth(u32 boss) {
  (void)boss;
  if (SC.phase == S_DEAD || !SC.body) return 0;
  return SB_GetBadGuyHealth(SC.body);
}

static void SgRender(u32 boss, s32 x, s32 y) { (void)boss; (void)x; (void)y; }

static void SgSpawn(u32 boss) {
  EdinburghBossCame();
  SB_ClearBytes(&SC, sizeof SC);
  SC.x = FX(200);
  SC.y = FX(-80);
  SC.body = SB_SpawnBadGuy(BH, "*edinburgh/boss/scheduler", SC.x, SC.y);
  SC.pendulum = SpawnPart("*edinburgh/boss/sched_pendulum", SC.x, SC.y + FX(PENDULUM_DY));
  SC.bells = SpawnPart("*edinburgh/boss/sched_bells", SC.x, SC.y + FX(BELLS_DY));
  SC.hands = SpawnPart("*edinburgh/boss/sched_hands", SC.x, SC.y + FX(HANDS_DY));
  SC.phase = S_ENTER;
  SC.glide_in = 4;
  R32(boss, B_STATE) = 1;
  SB_PausePartition(BH, 1);
  Place(SC.body, SC.x, SC.y);
  SC.maxhp = SB_GetBadGuyHealth(SC.body);
  R32(boss, B_MAXHP) = SC.maxhp;
  g_autopilot_target = SC.body;
  EdLine(L_MID_HELLO);              // "You don't seem to have an appointment."
  EdLine(L_MID_DOC);                // the Doc: "A chatbot with guns?"
}

static int Hurt(void) { return SB_GetBadGuyHealth(SC.body) * 2 < SC.maxhp; }

// The parts follow the clock; the hands go round by themselves (and spin, in
// the alarm: edin_cast.py).
static void PlaceParts(void) {
  const int alarm = Hurt();
  PartFrame(SC.hands, alarm);
  PartFrame(SC.bells, alarm && !InCutscene());
  PlacePart(SC.pendulum, SC.x, SC.y + FX(PENDULUM_DY));
  PlacePart(SC.bells, SC.x, SC.y + FX(BELLS_DY));
  PlacePart(SC.hands, SC.x, SC.y + FX(HANDS_DY));
}

static void DropParts(void) {
  DropPart(&SC.hands);
  DropPart(&SC.pendulum);
  DropPart(&SC.bells);
}

// the hands' sweep (edin_cast.py sched_hands: R16 the order, R15 busy)
static void HandsSweep(int quick) { if (SC.hands) SB_SetActorRegister(SC.hands, 16, FX(quick ? 2 : 1)); }
static int HandsBusy(void) {
  return SC.hands && (SB_GetActorRegister(SC.hands, 15) != 0 || SB_GetActorRegister(SC.hands, 16) != 0);
}

// One window at its moment t; 1 when it is over.
static int Window(int w, int t) {
  switch (w) {
  case SW_HANDS:
    if (t == 0) HandsSweep(Hurt());
    if (t == 40) Order(SC.body, E_REMIND);
    return t > 60 && !Busy(SC.body) && !HandsBusy();
  case SW_WEEK:
    if (t == 0) Order(SC.body, E_CALENDAR);
    return t > 10 && !Busy(SC.body);
  case SW_TICK:
    if (t == 0) Order(SC.body, E_TICKTOCK);
    return t > 10 && !Busy(SC.body);
  case SW_SNOOZE:
    if (t == 0) Order(SC.body, E_SNOOZE);
    return t > 10 && !Busy(SC.body);
  case SW_ALARM:
    if (t == 0) Order(SC.body, E_ALARM);
    if (t == 30) HandsSweep(1);
    return t > 40 && !Busy(SC.body) && !HandsBusy();
  case SW_XSNIPE:                   // the hands' lanes, snipe rings over them
    if (t == 0) HandsSweep(Hurt());
    if (t == 20) Order(SC.body, E_XSNIPE);
    return t > 40 && !Busy(SC.body) && !HandsBusy();
  case SW_XPINCER:                  // the hands' lanes, pincers closing through them
    if (t == 0) HandsSweep(1);
    if (t == 16) Order(SC.body, E_XPINCER);
    return t > 40 && !Busy(SC.body) && !HandsBusy();
  case SW_XTWIST:                   // the week's walls, then rings that turn at once
    if (t == 0) Order(SC.body, E_CALENDAR);
    if (t > 10 && !Busy(SC.body) && t < 200) { Order(SC.body, E_XTWIST); SC.wt = 200; return 0; }
    return t > 200 && !Busy(SC.body);
  case SW_XSNAKE:                   // the snake, then reminders that come back
    if (t == 0) Order(SC.body, E_XSNAKE);
    if (t > 10 && !Busy(SC.body) && t < 300) { Order(SC.body, E_REMIND); SC.wt = 300; return 0; }
    return t > 300 && !Busy(SC.body);
  case SW_XGAP:
    if (t == 0) Order(SC.body, E_XGAP);
    return t > 10 && !Busy(SC.body);
  case SW_GLIDE:
    // a new spot, away from where it is
    if (t == 0) SC.tx = SC.x < FX(200) ? FX(SB_RangedRand(230, 290)) : FX(SB_RangedRand(110, 170));
    {
      const s32 d = SC.tx - SC.x;
      const s32 v = FX(1.6);
      SC.x += d > v ? v : (d < -v ? -v : d);
      SC.y += Ease(FX(HOME_Y) - SC.y, FX(0.8));
      return d < FX(2) && d > -FX(2);
    }
  }
  return 1;
}

static void Hover(void) {
  SC.t++;
  if (SC.win != SW_GLIDE) {
    const s32 cx = SC.tx ? SC.tx : FX(200);
    const s32 wantx = cx + SB_MulFP(SB_Sin((s32)(SC.t << 15)), FX(22));
    const s32 wanty = FX(HOME_Y) + SB_MulFP(SB_Sin((s32)(SC.t << 16)), FX(10));
    SC.x += Ease(wantx - SC.x, FX(1.2));
    SC.y += Ease(wanty - SC.y, FX(0.8));
  }
  if (InCutscene()) return;
  if (SC.win) {
    if (Window(SC.win, SC.wt++)) {
      SC.win = 0;
      SC.cool = Hurt() ? 30 : 45;          // the beat
      if (HardMode()) SC.cool = Hurt() ? 20 : 30;
    }
    return;
  }
  if (SC.cool > 0) { SC.cool--; return; }
  const int hurt = Hurt();
  if (hurt && !SC.alarmed) { SC.alarmed = 1; SC.step = 0; }   // the hurt plan, from its top
  const int hard = HardMode();
  const unsigned char* plan = hard ? (hurt ? kPlanHurtH : kPlanCalmH) : (hurt ? kPlanHurt : kPlanCalm);
  const int n = hard ? (hurt ? (int)sizeof kPlanHurtH : (int)sizeof kPlanCalmH)
                     : (hurt ? (int)sizeof kPlanHurt : (int)sizeof kPlanCalm);
  if (SC.step >= n) SC.step = 0;
  SC.win = plan[SC.step];
  if (++SC.step >= n) SC.step = 0;                   // no modulo in the blob
  SC.wt = 0;
}

static void SgUpdate(u32 boss) {
  SpeechPump();
  BossWards();
  if (SC.phase != S_DEAD && !Alive(SC.body)) {
    DropParts();
    SC.phase = S_DEAD;
    R32(boss, B_STATE) = 0;                       // the engine calls death next
    return;
  }
  const int scene = InCutscene() || SC.phase == S_ENTER;
  ShipShield(scene);
  Guard(SC.body, scene);
  switch (SC.phase) {
  case S_ENTER:
    SC.y += FX(1.8);
    if (SC.y >= FX(HOME_Y)) {
      SC.y = FX(HOME_Y);
      SC.phase = S_HOVER;
      SC.cool = 50;
      SC.step = 0;
    }
    break;
  case S_HOVER:
    Hover();
    break;
  }
  Anim(SC.body, Hurt() ? SA_ALARM : (Busy(SC.body) ? SA_FIRE : SA_TICK));
  Place(SC.body, SC.x, SC.y);
  PlaceParts();
}

static void SgDeath(u32 boss) {
  DropParts();
  ShipShield(0);
  SB_ShakeWorld();
  SB_SpawnExplosion(BH, "ExplosionXL", SC.x, SC.y);
  SB_SpawnExplosion(BH, "ExplosionL", SC.x - FX(28), SC.y + FX(14));
  SB_SpawnExplosion(BH, "ExplosionL", SC.x + FX(26), SC.y - FX(10));
  g_autopilot_target = 0;
  EdLine(L_MID_DOWN);               // "Escalating to the orchestrator..."
  // set up the final boss and let the level run on to it
  GptBossInit(boss);
  SB_PausePartition(BH, 0);
}

void SubagentBossInit(u32 boss) {
  SB_ClearBytes((void*)boss, BOSS_SIZE);
  SpeechReset();                    // a new run of the level ...
  EdinburghReset();                 // ... from the homecoming
  R32(boss, B_SPAWN + 0x00) = (s32)(u32)SgSpawn;
  R32(boss, B_SPAWN + 0x04) = (s32)(u32)SgUpdate;
  R32(boss, B_SPAWN + 0x08) = (s32)(u32)SgRender;
  R32(boss, B_SPAWN + 0x0c) = (s32)(u32)SgRender;
  R32(boss, B_SPAWN + 0x10) = (s32)(u32)SgDeath;
  R32(boss, B_HEALTH) = (s32)(u32)SgHealth;
}
