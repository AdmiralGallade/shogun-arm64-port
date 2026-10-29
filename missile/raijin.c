// raijin.c -- Mission 14's final boss: Lord Raijin's storm dragon, Mark II.
//
// Mission 4's final boss (amaterasu/ama_cast.py: its scripts, ported, its
// parts tougher). As the game's own did it (StormBossSpawn/Update), the code
// builds it and keeps its alive mask; the body's script runs the cycles:
//
//   the body      *amaterasu/boss/dragon (30000), down to the middle of the
//                 screen; wings, head, tail and four cannons spawned on it
//                 (StormBossSpawn's table), the body their parent -- they fly
//                 with it (ParentSpeed, ParentAngle) and wait on its R6 / R0;
//   R19           the body's register for which parts still stand (1, 2 the
//                 wings, 4 the head, 8 the tail, 16-128 the cannons), kept
//                 here every frame: its cycles wait on it;
//   wrecks        every main part shot off leaves its wreck -- the head's and
//                 the tail's (teteg, queueg) as StormBossBadguyDeath did, the
//                 wings' (aileg_l, aileg_r: wrecked frames) -- pinned to the
//                 body to the end: something of the dragon is always there, and
//                 the lightning comes from it (a bolt from a part that's gone
//                 comes from its wreck, never from the air);
//
// The Mk II, and the mission's story:
//
//   the jam       as over Japan, Raijin jams the Entropy Engine: no grazing,
//                 and Amaterasu can't see the pilot (AmaJam). The jammer is
//                 the head: shot off, the engine is clear;
//   the sun       on a part's turn, that part is painted (PT_OPEN: a strike
//                 breaks it off); between turns, the body (PT_BOSS: a fifth
//                 of it, and a part broken off) -- so parts go one by one, and
//                 the pilot has to chase the wings out to the sides;
//   the thunder   bolts on the ship now and then, more often as he weakens,
//                 each from one part for its whole flight: its jaws, or its
//                 tail's tip on the tail's turn (twice as often); and once the
//                 head is gone, the lightning cage (Cage): four marks round
//                 the ship, then a fifth on it -- out through a corner;
//   the turns     head first (and longer), then the wings, then the tail;
//                 each part open only on its turn, to guns and sun alike;
//   Mark II       each of Mission 4's attacks changed where players beat it,
//                 and new ones (ama_cast.py): aimed corner fans and the head's
//                 roar (rings with a drifting gap), bending spirals and the
//                 wings' talons (curling in on the ship), aimed seeds, rings
//                 that turn at the ship, a third rosette cross;
//   his words     all queued at the start, in order; the jam takes hold as he
//                 says it; his last words before he blows;
//   the finish    at FINISH of his health he can't be hurt by guns any more;
//                 the mirror goes to full sun (AmaFullSun: ready, a quick
//                 paint) and the pilot paints him: the strike ends him. Then
//                 the fleet comes up (AmaEnding) and the level ends once the
//                 last line is up.
//
// Same rules as missile.c: no libc, no division, engine reached by address.

#include "andaman.h"
#include "amaterasu_lines.h"
#include "amaterasu.h"

#define RJ_SetActorParent FN(void (*)(u32, u32), 0xc184c)                // ARM

enum { P_WING_L, P_WING_R, P_HEAD, P_TAIL, P_CAN_L, P_CAN_R, P_CAN_CL, P_CAN_CR, NPART };
static const char* const kPartName[NPART] = {
  "*amaterasu/boss/aile_l", "*amaterasu/boss/aile_r", "*amaterasu/boss/tete", "*amaterasu/boss/queue",
  "*amaterasu/boss/canon_l", "*amaterasu/boss/canon_r", "*amaterasu/boss/canon_cl", "*amaterasu/boss/canon_cr",
};
static const signed char kPartX[NPART] = { -63, 63, 0, 0, 0, 0, 0, 0 };
static const signed char kPartY[NPART] = { -7, -7, 83, -71, -22, -22, 7, 7 };

#define START_Y       (-128)
#define ART_DY        11            // the body's look sits this far below it (Mission 4's Emit bossg 11 0)
#define BODY_HP       30000
#define FINISH        3600          // the body's health where the finish begins (12 %)
#define SUN_BITE      6000          // a strike on the body: a fifth of him ...
#define THUNDER_SLOW  280
#define THUNDER_FAST  170
#define CAGE_EVERY    3             // every third bolt, once the head is gone, is the cage
#define CAGE_R        46            // units from the ship: the cage's four marks ...
#define CAGE_CHARGE   96            // ... charging this long; the fifth, on the ship,
#define CAGE_LAST     150           // this long (out through a corner before it lands)
#define NB            5             // bolts the dragon keeps track of

enum { R_FIGHT = 1, R_FINISH, R_SUN, R_END, R_DONE };
enum { SRC_JAWS, SRC_TAIL, SRC_CHEST, SRC_WING_L, SRC_WING_R };

static struct {
  u32 body, part[NPART];
  u32 wreck_head, wreck_tail, wreck_wing[2];
  u32 art;                          // the body's look (bossg), pinned to it
  s32 x, y;
  int phase, t, thunder, bolts_fired, said_thunder, said_jam, end_t, quiet;
  int jam_line, sun_line;           // tickets (AnLine) of his jam's words and of his last
  int bolt[NB];                     // amaterasu.c's bolt numbers in flight ...
  unsigned char src[NB];            // ... and which part of him each comes from
} R;

// The desktop probes read the fight through this (runtime/probes/ama_dragon.py):
// the body, then the eight parts.
u32* volatile g_ryujin;

static s32 RjHealth(u32 boss) {
  (void)boss;
  if (R.phase != R_FIGHT && R.phase != R_FINISH) return 0;
  return Alive(R.body) ? SB_GetBadGuyHealth(R.body) : 0;
}

static void RjRender(u32 boss, s32 x, s32 y) { (void)boss; (void)x; (void)y; }

static int Up(int k) { return R.part[k] && Alive(R.part[k]); }
static int Open(int k) { return Up(k) && GetReg(R.part[k], 14) == 0; }

static void RjSpawn(u32 boss) {
  SB_ClearBytes(&R, sizeof R);
  for (int i = 0; i < NB; i++) R.bolt[i] = -1;
  R.x = FX(200);
  R.y = FX(START_Y);
  R.body = SB_SpawnBadGuy(BH, "*amaterasu/boss/dragon", R.x, R.y);
  g_ryujin = &R.body;
  const u32 ba = Act(R.body);
  for (int k = 0; k < NPART; k++) {
    R.part[k] = SB_SpawnBadGuy(BH, kPartName[k], R.x + FX(kPartX[k]), R.y + FX(kPartY[k]));
    if (R.part[k]) RJ_SetActorParent(Act(R.part[k]), ba);
    // painted on its turn (the cannons once they've dropped out, below)
    if (k < P_CAN_L) AmaPaintAdd(R.part[k], PT_OPEN);
  }
  AmaPaintAdd(R.body, PT_BOSS);     // painted between the parts' turns
  {
    const u32 e = SB_SpawnGeneric(BH, "*amaterasu/boss/bossg", R.x, R.y + FX(ART_DY));
    R.art = e ? SB_GetGenericActor(e) : 0;
    if (R.art) RJ_SetActorParent(R.art, ba);
  }
  R.phase = R_FIGHT;
  R.thunder = THUNDER_SLOW;
  R32(boss, B_STATE) = 1;
  R32(boss, B_MAXHP) = BODY_HP;
  SB_PausePartition(BH, 1);
  g_autopilot_target = R.part[P_HEAD];
  // his words, in order, all queued now (lines waiting on a quiet moment, or
  // forgotten at the finish, went unseen): the jam takes hold as he says it
  AnLine(AL_RAIJIN_IN);
  AnLine(AL_DOC_RAIJIN);
  R.jam_line = AnLine(AL_RAIJIN_JAM);
  AnLine(AL_DOC_JAM);
}

static u32 Target(void) {
  static const unsigned char kOrder[NPART] = { P_HEAD, P_WING_L, P_WING_R, P_TAIL, P_CAN_L, P_CAN_R, P_CAN_CL,
                                               P_CAN_CR };   // (the open one is picked)
  for (int i = 0; i < NPART; i++)
    if (Open(kOrder[i])) return R.part[kOrder[i]];
  return R.body;
}

static void Wrecks(void) {
  if (R.wreck_head) SB_SetActorPosition(R.wreck_head, R.x, R.y + FX(kPartY[P_HEAD]));
  if (R.wreck_tail) SB_SetActorPosition(R.wreck_tail, R.x, R.y + FX(kPartY[P_TAIL]));
  for (int k = 0; k < 2; k++)
    if (R.wreck_wing[k]) SB_SetActorPosition(R.wreck_wing[k], R.x + FX(kPartX[k]), R.y + FX(kPartY[k]));
}

// ... and gone with him, at the last
static void WrecksAway(void) {
  u32* w[4] = { &R.wreck_head, &R.wreck_tail, &R.wreck_wing[0], &R.wreck_wing[1] };
  for (int k = 0; k < 4; k++) {
    if (*w[k]) SB_SetActorPosition(*w[k], FX(-400), FX(-400));
    *w[k] = 0;
  }
}

static u32 WreckAt(const char* name, s32 dy) {
  const u32 e = SB_SpawnGeneric(BH, name, R.x, R.y + dy);
  return e ? SB_GetGenericActor(e) : 0;
}

static void Break(int k) {
  s32 x, y;
  SB_GetActorPosition(Act(R.part[k]), &x, &y);
  AmaPaintRemove(R.part[k]);
  Remove(R.part[k]);
  AnBlast(x, y);
}

// A strike on the body breaks a part off besides: the wings, then the tail
// (one each strike), a cannon only once it has dropped out -- the head last.
static void BreakOne(void) {
  static const unsigned char kOrder[NPART] = { P_WING_L, P_WING_R, P_TAIL, P_CAN_CL, P_CAN_CR, P_CAN_L, P_CAN_R,
                                               P_HEAD };
  for (int i = 0; i < NPART; i++) {
    const int k = kOrder[i];
    if (!Up(k) || (k >= P_CAN_L && GetReg(R.part[k], 14) != 0)) continue;
    Break(k);
    return;
  }
}

// ---- the lightning ---------------------------------------------------------------------------------
// Each bolt keeps the part it came from for its whole flight (it used to ask
// "where from?" every frame: a bolt aimed from the tail cracked from the jaws
// once the tail's turn ended under it).
static int TailTurn(void) {
  return R.body && Alive(R.body) && GetReg(R.body, 6) == FX(2) && Up(P_TAIL);
}

// Where a bolt comes from: the part, or (shot off) its wreck, pinned where it
// stood -- the body's always there to see, so nothing cracks out of thin air.
static void SrcPos(int src, s32* x, s32* y) {
  *x = R.x;
  *y = R.y + FX(50);                                  // its chest
  if (src == SRC_TAIL) {
    *y = R.y - FX(71 + 40);                           // the lightning rod's tip
  } else if (src == SRC_JAWS && Up(P_HEAD)) {
    SB_GetActorPosition(Act(R.part[P_HEAD]), x, y);
    *y += FX(30);
  } else if (src == SRC_JAWS) {
    *y = R.y + FX(kPartY[P_HEAD] + 12);               // the wreck's torn neck
  } else if (src == SRC_WING_L) {
    *x = R.x - FX(110);
    *y = R.y - FX(20);
  } else if (src == SRC_WING_R) {
    *x = R.x + FX(110);
    *y = R.y - FX(20);
  }
}

static void Fire(int src, s32 tx, s32 ty, int charge) {
  for (int i = 0; i < NB; i++) {
    if (AmaBoltBusy(R.bolt[i])) continue;
    s32 sx, sy;
    SrcPos(src, &sx, &sy);
    R.bolt[i] = AmaBolt(sx, sy, tx, ty, charge);
    R.src[i] = (unsigned char)src;
    return;
  }
}

static void BoltsFollow(void) {
  for (int i = 0; i < NB; i++) {
    if (!AmaBoltBusy(R.bolt[i])) continue;
    s32 sx, sy;
    SrcPos(R.src[i], &sx, &sy);
    AmaBoltFrom(R.bolt[i], sx, sy);
  }
}

// The lightning cage: four marks round the ship -- above, below, either side
// -- charging together, then a fifth on the ship itself, later. The way out
// is through a corner, between two marks, before the fifth lands. From
// whatever it has left: its jaws, its wing tips, its tail, its chest.
static void Cage(void) {
  const s32 px = PlayerX(), py = PlayerY();
  static const signed char kDx[4] = { 0, 0, -1, 1 }, kDy[4] = { -1, 1, 0, 0 };
  static const unsigned char kSrc[4] = { SRC_TAIL, SRC_JAWS, SRC_WING_L, SRC_WING_R };
  for (int k = 0; k < 4; k++) Fire(kSrc[k], px + FX(CAGE_R) * kDx[k], py + FX(CAGE_R) * kDy[k], CAGE_CHARGE);
  Fire(SRC_CHEST, px, py, CAGE_LAST);
}

static void Thunder(int cut, s32 hp) {
  BoltsFollow();
  if (cut || --R.thunder > 0) return;
  R.thunder = (hp < BODY_HP / 2 ? THUNDER_FAST : THUNDER_SLOW) - (HardMode() ? 60 : 0);
  if (TailTurn()) R.thunder >>= 1;                  // the lightning rod's turn: twice as often
  if (AmaThunderBusy()) return;
  R.bolts_fired++;
  // once the jammer's gone, every third is the cage (not on the tail's turn: its own)
  if (!Up(P_HEAD) && !TailTurn() && R.bolts_fired >= CAGE_EVERY) {
    R.bolts_fired = 0;
    R.thunder += 120;
    Cage();
  } else {
    Fire(TailTurn() ? SRC_TAIL : SRC_JAWS, PlayerX(), PlayerY(), 80);
  }
  if (!R.said_thunder) {
    R.said_thunder = 1;
    AnLine(AL_RAIJIN_THUNDER);
  }
}

// ---- every frame ------------------------------------------------------------------------------------
static void RjUpdate(u32 boss) {
  R.t++;
  SpeechPump();
  BossWards();
  if (R.phase == R_DONE) return;
  const int cut = InCutscene();
  ShipShield(cut);
  if (R.body && Alive(R.body)) SB_GetActorPosition(Act(R.body), &R.x, &R.y);
  // ---- the parts, the mask, the wrecks ----
  s32 mask = 0;
  int any_open = 0;
  for (int k = 0; k < NPART; k++) {
    const u32 p = R.part[k];
    if (!p) continue;
    if (!Alive(p)) {
      R.part[k] = 0;
      if (k == P_HEAD) {
        R.wreck_head = WreckAt("*amaterasu/boss/teteg", FX(83));
        if (R.said_jam == 1) {
          AmaJam(0);                  // the jammer's gone
          AnLine(AL_DOC_UNJAM);
        }
        R.said_jam = 2;
      }
      if (k == P_TAIL) R.wreck_tail = WreckAt("*amaterasu/boss/queueg", FX(-71));
      if (k <= P_WING_R) R.wreck_wing[k] = WreckAt(k ? "*amaterasu/boss/aileg_r" : "*amaterasu/boss/aileg_l", 0);
      continue;
    }
    mask |= 1 << k;
    const int open = GetReg(p, 14) == 0;
    any_open |= open;
    Guard(p, !open || cut || R.phase != R_FIGHT);
    // pinned to the body (their scripts followed by copying its speed and
    // drifted off it); the cannons drop out on their own, so not them
    if (k < P_CAN_L && R.body && Alive(R.body)) Place(p, R.x + FX(kPartX[k]), R.y + FX(kPartY[k]));
    // a cannon is painted once it has dropped out (hidden under the body, every
    // strike on it took all four, and its phase never came)
    if (k >= P_CAN_L && open) AmaPaintAdd(p, PT_OPEN);
  }
  if (R.art && R.body && Alive(R.body)) SB_SetActorPosition(R.art, R.x, R.y + FX(ART_DY));
  Wrecks();
  if (R.body && Alive(R.body)) SetReg(R.body, 19, FX(mask));
  switch (R.phase) {
  case R_FIGHT: {
    const s32 hp = Alive(R.body) ? SB_GetBadGuyHealth(R.body) : 0;
    Guard(R.body, GetReg(R.body, 14) != 0 || cut);
    // the body is painted only between the parts' turns: on a turn, that part
    // (and not before the jammer's gone: the sun can't see him while he jams)
    AmaPaintEnable(R.body, !any_open && R.said_jam == 2);
    // the jam, as he says it (the head still on)
    if (!R.said_jam && R.jam_line && AnLineUp(R.jam_line)) {
      R.said_jam = Up(P_HEAD) ? 1 : 2;
      if (R.said_jam == 1) AmaJam(1);
    }
    // the sun on the body, through any armour: a fifth of him, and a part off
    // (one part a strike, however the strikes were counted)
    if (AmaStruck(R.body)) {
      BreakOne();
      const s32 now = SB_GetBadGuyHealth(R.body);
      SB_SetBadGuyHealth(R.body, now - SUN_BITE > FINISH ? now - SUN_BITE : FINISH);
      SB_ShakeWorld();
    }
    Thunder(cut, hp);
    if (Alive(R.body) && hp <= FINISH) {
      // his last stand: whatever is left of him breaks off (nothing goes on
      // firing from the sides of a dragon that's down to its body)
      R.phase = R_FINISH;
      for (int k = 0; k < NPART; k++)
        if (Up(k)) Break(k);
      SB_SetBadGuyHealth(R.body, FINISH);
      AmaJam(0);
      AmaFullSun();
      AmaPaintEnable(R.body, 1);
      AnLine(AL_DOC_FINISH);
    }
    g_autopilot_target = Target();
    break;
  }
  case R_FINISH:
    // the guns can't end him now: only the sun
    Guard(R.body, 1);
    SB_SetBadGuyHealth(R.body, FINISH);
    BoltsFollow();
    if (AmaStruck(R.body)) {
      R.phase = R_SUN;
      R.end_t = 0;
      AmaPaintRemove(R.body);
      R.sun_line = AnLine(AL_RAIJIN_SUN);     // (queued: said in turn, not over others)
    }
    g_autopilot_target = 0;
    break;
  case R_SUN:
    // he burns: a run of blasts over him, until his last words are up; then he's gone
    R.end_t++;
    SB_PausePartition(BH, 1);       // (see R_END)
    if (R.body) {
      if ((R.end_t & 7) == 0) AnBlast(R.x + FX(Rnd(-60, 61)), R.y + FX(Rnd(-60, 61)));
      if (R.end_t >= 90 && (AnLineUp(R.sun_line) || R.end_t > 1500)) {
        Guard(R.body, 0);
        Remove(R.body);
        if (R.art) SB_SetActorPosition(R.art, FX(-400), FX(-400));   // (its look goes with it,
        WrecksAway();                                                  //  and what's left of him)
        Blast("ExplosionXL", R.x, R.y);
        SB_ShakeWorld();
        R.body = 0;
        R.quiet = 0;
      }
      break;
    }
    // ... and the ending once his words are done
    R.quiet = cut ? 0 : R.quiet + 1;
    if (R.quiet > 40) {
      R.phase = R_END;
      AmaEnding();
    }
    break;
  case R_END:
    // (the body's PausePartition line: its death let the level's clock run
    // on, and the level ended 260 frames later, under the ending's lines --
    // held here until they're done; RjDeath lets it go)
    SB_PausePartition(BH, 1);
    if (AmaEndingDone()) {
      R.phase = R_DONE;
      R32(boss, B_STATE) = 0;       // the engine calls death next
    }
    break;
  }
}

static void RjDeath(u32 boss) {
  (void)boss;
  ShipShield(0);
  AmaJam(0);
  for (int k = 0; k < NPART; k++) {
    if (R.part[k] && Alive(R.part[k])) Remove(R.part[k]);
    R.part[k] = 0;
  }
  if (R.art) SB_SetActorRegister(R.art, REG_CMD, FX(1));
  R.art = 0;
  WrecksAway();
  g_autopilot_target = 0;
  SB_PausePartition(BH, 0);         // on to the level's end
}

void RaijinBossInit(u32 boss) {
  SB_ClearBytes((void*)boss, BOSS_SIZE);
  R32(boss, B_SPAWN + 0x00) = (s32)(u32)RjSpawn;
  R32(boss, B_SPAWN + 0x04) = (s32)(u32)RjUpdate;
  R32(boss, B_SPAWN + 0x08) = (s32)(u32)RjRender;
  R32(boss, B_SPAWN + 0x0c) = (s32)(u32)RjRender;
  R32(boss, B_SPAWN + 0x10) = (s32)(u32)RjDeath;
  R32(boss, B_HEALTH) = (s32)(u32)RjHealth;
}
