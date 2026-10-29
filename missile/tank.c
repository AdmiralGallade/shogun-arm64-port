// tank.c -- Mission 15's mid-boss: the Kanabo, a flying siege tank, and the Valhalla that ends it.
//
// Mid-defence (paris_timeline.MID_T) the Hojo wing sends a breaching tank
// over the base's wall (docs/M15_DESIGN.md): a heavy hover-tank on two
// sponsons of lift fans (paris/art_legion.py kanabo_*), built of badguys
// pinned to its hull every frame:
//
//   the hull     never a target (guarded, its hitboxes off the playfield);
//   the cannon   its main turret: the twin breaching cannon. Every so often it
//                turns on the frontmost shield generator standing, a red mark
//                on it, the charge building in its muzzles -- then a slug of
//                plasma that burns through the dome (ParisSiteHit: most of a
//                generator in one shot). Between, fans of heavy arrows at the
//                ship. Hurtable, and tough;
//   two guns     rotary guns on its sponsons: aimed bursts at the ship;
//   two pods     missile pods astern: rings of slow bombs.
//
// It can't be brought down in time by the pilot alone, and isn't meant to
// be: after its second breach (or once its guns and pods are gone, or a
// time limit) Volta brings the Valhalla up on station at the bottom of the
// screen, every gun clearing the sky (ParisValWipe), and her railgun on the
// tank -- when it fires, the Kanabo is gone. Its death sets the Kagutsuchi up
// for the next boss event (flagship.c) and lets the clock run.
//
// Same rules as missile.c: no libc, no division, engine reached by address.

#include "andaman.h"
#include "paris_lines.h"
#include "paris.h"
#include "amaterasu.h"

enum { K_CANNON, K_GUN_L, K_GUN_R, K_POD_L, K_POD_R, NPART };
static const char* const kName[NPART] = { "*paris/boss/kanabo_cannon", "*paris/boss/kanabo_gun",
                                          "*paris/boss/kanabo_gun", "*paris/boss/kanabo_pod",
                                          "*paris/boss/kanabo_pod" };
// from the hull's centre (art_legion.kanabo_hull: 176 x 184, centre (88, 92)), +y down
static const short kX[NPART] = { 0, -64, 64, -40, 40 };
static const short kY[NPART] = { -8, 64, 64, -66, -66 };

#define START_Y      (-210)
#define STATION_Y    92
#define BREACH_EVERY 420           // frames between breaching shots
#define BREACH_AIM   110           // ... the turn and the charge before one
#define BREACH_V     FX(5.5)       // the slug's speed
#define BREACH_DMG   1000          // a generator has 1300: two of these
#define CALL_AFTER   2         // breaches before Volta is called
#define CALL_LIMIT   2400          // ... or this many frames
#define DEATH_T      150

enum { T_IN = 1, T_FIGHT, T_CALL, T_KILL, T_DONE };
enum { CN_IDLE, CN_CHARGE, CN_FIRE };          // the cannon's animations

static struct {
  u32 hull, part[NPART];
  s32 x, y;
  int phase, t, fight_t, breach_t, breaches, kill_t;
  int site;                                    // the breach's target, or -1
  u32 mark, slug;
  s32 sx, sy, sdx, sdy;
  int slug_t, slug_left;
  int rail_line;
  s32 maxhp;
} T;

static s32 TkHealth(u32 boss) {
  (void)boss;
  if (T.phase >= T_KILL) return 0;
  s32 hp = 0;
  for (int k = 0; k < NPART; k++)
    if (T.part[k] && Alive(T.part[k])) hp += SB_GetBadGuyHealth(T.part[k]);
  return hp;
}

static void TkRender(u32 boss, s32 x, s32 y) { (void)boss; (void)x; (void)y; }

static void TkSpawn(u32 boss) {
  SB_ClearBytes(&T, sizeof T);
  T.x = FX(200);
  T.y = FX(START_Y);
  T.hull = SB_SpawnBadGuy(BH, "*paris/boss/kanabo_hull", T.x, T.y);
  for (int k = 0; k < NPART; k++) {
    T.part[k] = SB_SpawnBadGuy(BH, kName[k], T.x + FX(kX[k]), T.y + FX(kY[k]));
    if (T.part[k]) Guard(T.part[k], 1);
  }
  T.phase = T_IN;
  T.site = -1;
  R32(boss, B_STATE) = 1;
  SB_PausePartition(BH, 1);
  T.maxhp = TkHealth(boss);
  R32(boss, B_MAXHP) = T.maxhp;
  AnLine(PL_DOC_TANK);
  ParisScrollAhead(1);                            // the ground ahead of the walls, for its fight
}

static void Pin(void) {
  if (T.hull && Alive(T.hull)) {
    Place(T.hull, T.x, T.y);
    Guard(T.hull, 1);
    Anim(T.hull, T.phase == T_KILL ? 1 : (T.phase == T_IN ? 2 : 0));
  }
  for (int k = 0; k < NPART; k++) {
    const u32 p = T.part[k];
    if (!p) continue;
    if (!Alive(p)) {
      AnBlast(T.x + FX(kX[k]), T.y + FX(kY[k]));
      T.part[k] = 0;
      continue;
    }
    const s32 px = T.x + FX(kX[k]), py = T.y + FX(kY[k]);
    Place(p, px, py);
    Guard(p, T.phase == T_IN || T.phase >= T_KILL || InCutscene());
    if (k == K_GUN_L || k == K_GUN_R) AnTurn(Act(p), AnAngle(px, py, PlayerX(), PlayerY()));
  }
}

// The cannon: on the ship between breaches, on its target through one.
static void Cannon(void) {
  const u32 c = T.part[K_CANNON];
  if (!c) return;
  const s32 cx = T.x + FX(kX[K_CANNON]), cy = T.y + FX(kY[K_CANNON]);
  s32 tx = PlayerX(), ty = PlayerY();
  if (T.site >= 0) ParisSitePos(T.site, &tx, &ty);
  AnTurn(Act(c), AnAngle(cx, cy, tx, ty));
  if (T.phase != T_FIGHT && T.phase != T_CALL) return;
  T.breach_t++;
  if (T.site < 0 && T.breach_t >= BREACH_EVERY - BREACH_AIM && !InCutscene()) {
    T.site = ParisBreachSite();
    if (T.site >= 0) {
      ParisSitePos(T.site, &tx, &ty);
      T.mark = AnSpawn("*paris/boss/mark", tx, ty);
      SetReg(c, 14, FX(1));                     // (its script holds its fans)
    } else {
      T.breach_t = 0;
    }
  }
  if (T.site >= 0) {
    AnPut(T.mark, tx, ty);
    AnFrame(T.mark, T.breach_t > BREACH_EVERY - 30 ? 1 : 0);
    Anim(c, CN_CHARGE);
    if (T.breach_t >= BREACH_EVERY) {           // fire: the slug, along the turret's line
      const s32 a = AnAngle(cx, cy, tx, ty);
      T.sdx = SB_MulFP(SB_Cos(a), BREACH_V);
      T.sdy = SB_MulFP(SB_Sin(a), BREACH_V);
      T.sx = cx + SB_MulFP(SB_Cos(a), FX(56));
      T.sy = cy + SB_MulFP(SB_Sin(a), FX(56));
      T.slug_left = DivSmall(AnDist(tx - T.sx, ty - T.sy), 5) + 1;   // (5.5 a frame: a frame or so early)
      T.slug = AnSpawn("*paris/boss/breach", T.sx, T.sy);
      AnTurn(T.slug, a);
      T.slug_t = T.site + 1;                    // (the site it's for)
      T.site = -1;
      T.breach_t = 0;
      T.breaches++;
      Anim(c, CN_FIRE);
      SetReg(c, 14, 0);
      SB_ShakeWorld();
      Boom("ExplosionL", T.sx, T.sy);
    }
  } else if (T.breach_t > 20) {
    Anim(c, CN_IDLE);
  }
}

static void Slug(void) {
  if (!T.slug) return;
  T.sx += T.sdx;
  T.sy += T.sdy;
  AnPut(T.slug, T.sx, T.sy);
  if (--T.slug_left <= 0) {
    ParisSiteHit(T.slug_t - 1, BREACH_DMG);
    AnDrop(&T.slug);
    AnDrop(&T.mark);
  }
}

static u32 Target(void) {
  static const unsigned char kOrder[NPART] = { K_GUN_L, K_GUN_R, K_POD_L, K_POD_R, K_CANNON };
  for (int i = 0; i < NPART; i++) {
    const u32 t = T.part[kOrder[i]];
    if (t && Alive(t)) return t;
  }
  return 0;
}

static void TkUpdate(u32 boss) {
  T.t++;
  SpeechPump();
  BossWards();
  if (T.phase == T_DONE) return;
  SB_PausePartition(BH, 1);
  ShipShield(InCutscene() && T.phase != T_KILL);
  // over the wall to its station, then the slow sway of a hovering tank
  if (T.phase == T_IN) {
    T.y += Ease(FX(STATION_Y) - T.y, FX(1.4));
    if (T.y > FX(STATION_Y - 3)) { T.phase = T_FIGHT; T.fight_t = 0; }
  } else if (T.phase == T_FIGHT || T.phase == T_CALL) {
    T.fight_t++;
    T.x = FX(200) + SB_MulFP(FX(34), SB_Sin(T.fight_t * 0x10000));
    T.y = FX(STATION_Y) + SB_MulFP(FX(6), SB_Sin(T.fight_t * 0x2c000));
  } else if (T.phase == T_KILL) {
    T.y += FX(0.25);                              // (sinking, burning)
  }
  if (T.hull) ParisCover(T.x - FX(80), T.y - FX(88), T.x + FX(80), T.y + FX(88));
  Pin();
  Cannon();
  Slug();
  int small = 0;
  for (int k = K_GUN_L; k < NPART; k++) small += T.part[k] != 0;
  // Volta: called after its second breach, or its guns and pods all gone, or the limit
  if (T.phase == T_FIGHT && (T.breaches >= CALL_AFTER || small == 0 || T.fight_t > CALL_LIMIT)) {
    T.phase = T_CALL;
    AnValCome();
    T.rail_line = AnLine(PL_VOLTA_TANK);
    AnValRailOn(T.hull, T.rail_line);
    ParisValWipe(900);
  }
  if (T.phase == T_CALL) {
    AnValRailMove(T.x, T.y);
    // her railgun: the Kanabo is gone (and if the call ever stuck, it goes anyway)
    if (AnValRailState() >= RAIL_FIRING || ++T.kill_t > 1800) {
      T.phase = T_KILL;
      T.kill_t = 0;
      for (int k = 0; k < NPART; k++)
        if (T.part[k] && Alive(T.part[k])) {
          SB_SetInvincibility(T.part[k], -1);
          SB_StrikeBadGuy(BH, T.part[k], 0x7fff);
        }
      AnDrop(&T.mark);
      AnDrop(&T.slug);
      SB_ShakeWorld();
      AnLine(PL_VOLTA_TANK_DOWN);
      AnLine(PL_GEN_THANKS);
      ParisScrollAhead(0);                        // back to the base's own view as it burns
    }
  }
  if (T.phase == T_KILL) {
    T.kill_t++;
    if ((T.kill_t & 3) == 0) AnBlast(T.x + FX(Rnd(-80, 81)), T.y + FX(Rnd(-80, 81)));
    if (T.kill_t == DEATH_T) {
      if (T.hull && Alive(T.hull)) {
        Blast("ExplosionXL", T.x, T.y);
        SB_SetInvincibility(T.hull, -1);
        SB_StrikeBadGuy(BH, T.hull, 0x7fff);
      }
      T.hull = 0;
      AnValLeave();
      T.phase = T_DONE;
      R32(boss, B_STATE) = 0;                     // the engine calls death next
    }
  }
  g_autopilot_target = T.phase == T_FIGHT || T.phase == T_CALL ? Target() : 0;
}

static void TkDeath(u32 boss) {
  ShipShield(0);
  for (int k = 0; k < NPART; k++) {
    if (T.part[k] && Alive(T.part[k])) Remove(T.part[k]);
    T.part[k] = 0;
  }
  if (T.hull && Alive(T.hull)) {
    Place(T.hull, FX(200), FX(-460));
    Remove(T.hull);
  }
  T.hull = 0;
  AnDrop(&T.mark);
  AnDrop(&T.slug);
  g_autopilot_target = 0;
  ParisTankDone();
  ParisScrollAhead(0);                            // back to the base's own view
  FlagshipBossInstall(boss);                      // the next boss event brings up the carrier
  SB_PausePartition(BH, 0);
}

void TankBossInit(u32 boss) {
  SB_ClearBytes((void*)boss, BOSS_SIZE);
  SpeechReset();                                  // a new run of the level
  ParisReset();
  R32(boss, B_SPAWN + 0x00) = (s32)(u32)TkSpawn;
  R32(boss, B_SPAWN + 0x04) = (s32)(u32)TkUpdate;
  R32(boss, B_SPAWN + 0x08) = (s32)(u32)TkRender;
  R32(boss, B_SPAWN + 0x0c) = (s32)(u32)TkRender;
  R32(boss, B_SPAWN + 0x10) = (s32)(u32)TkDeath;
  R32(boss, B_HEALTH) = (s32)(u32)TkHealth;
}
