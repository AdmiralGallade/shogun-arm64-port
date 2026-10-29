// sunda.c -- Mission 10 (world 10): the Indian Ocean, over the Sunda Trench.
//
// The Shogun strikes back. The Bastion -- Captain Argus -- and her two
// frigates sail home from the Iron Reef with the pilot on their wing:
//
//   the way home  the open ocean; stragglers of the garrison;
//   the ambush    torpedoes out of nowhere hit the Bastion (the clock held
//                 for the moment of it); a vast shape passes under the
//                 fleet: Dr. Marrow's living submarine, Specimen Nine;
//   the fight     her surfacing drones and the garrison's last craft come at
//                 the fleet; her torpedoes run at the Bastion and the pilot
//                 must shoot them down (the ones that get through hit her);
//                 the fleet's guns join in; a spread sinks the frigate Lance;
//   the boss      Specimen Nine surfaces (coelacanth.c), and is driven off
//                 -- then takes Captain Argus anyway, and the pilot follows
//                 her down in the Valhalla's prototype submersible.
//
// This file: the cast (its own, and what it borrows from Mission 9 and the
// Beach); the fleet, sailing at the scroll's speed so its ships hold their
// places low on the screen, their guns on whatever comes up; the torpedoes
// striking home; the drones' invincibility under the water; the story's
// lines, by name (Mission 9's machinery, andaman.c, with this world's
// speeches). Run once a frame from the satellites' update (missile.c).
//
// Same rules as missile.c: no libc, no division, engine reached by address.

#define SUNDA_LINE_TABLE
#include "andaman.h"
#include "sunda_lines.h"
#include "sunda.h"

#define SU_LoadBadGuy       FN(void (*)(u32, const char*), 0xbf324)     // ARM
#define SU_LoadBullet       FN(void (*)(u32, const char*), 0xbeffc)     // ARM
#define SU_LoadGeneric      FN(void (*)(u32, const char*), 0xbf140)     // ARM
#define SU_PausePartition   FN(void (*)(u32, u32), 0xbad4c)             // ARM

static const char* const kBadguys[] = {
  "*sunda/scripts/torpedo",
  "*sunda/scripts/torpedo_dn",
  "*sunda/scripts/remora",
  "*sunda/scripts/remora_hi",
  "*sunda/scripts/remora_lo",
  "*sunda/scripts/medusa",
  "*sunda/scripts/seeker",
  "*sunda/scripts/lantern",
  "*sunda/scripts/remora_x",          // hard mode's elites (sunda_cast.py, docs/HARD_MODE.md)
  "*sunda/scripts/remora_hi_x",
  "*sunda/scripts/remora_lo_x",
  "*sunda/scripts/medusa_x",
  "*sunda/boss/tube",
  "*sunda/boss/turret",
  "*sunda/boss/lure",
  "*sunda/boss/hull",
  "*sunda/boss/bay",
  "*sunda/boss/maw",
  "*sunda/boss/breach",
  "*sunda/boss/arm",
  // the garrison's last craft (Mission 9's)
  "*andaman/scripts/missile",
  "*andaman/scripts/tobiuo",
  "*andaman/scripts/tobiuo_l",
  "*andaman/scripts/tobiuo_r",
  "*andaman/scripts/gunboat",
  "*andaman/scripts/hydrofoil",
  "*andaman/scripts/skymine",
  "*andaman/scripts/supply",
  "*andaman/scripts/tobiuo_x",        // hard mode: Mission 9's elites (anda_cast.py)
  "*andaman/scripts/tobiuo_l_x",
  "*andaman/scripts/tobiuo_r_x",
  "*andaman/scripts/gunboat_x",
  "*andaman/scripts/hydrofoil_x",
  "*andaman/scripts/skymine_x",
};
static const char* const kBullets[] = {
  "*beach/boss/starlet",            // Dr. Marrow's own
  "*andaman/boss/shell",
  "*andaman/boss/kink",            // Mission 9's craft fire these too
  "*andaman/boss/curl",
  "*andaman/boss/hook",
  "*andaman/boss/dart",
  "*andaman/boss/lock",
  "*andaman/boss/h_comma",            // hard mode's two-stage bullets (beach/hardkit.py): Mission 9's elites' ...
  "*andaman/boss/h_twist",
  "*andaman/boss/h_snipe",
  "*andaman/boss/h_lock",
  "*andaman/boss/h_drift",
  "*andaman/boss/h_hesitate",
  "*andaman/boss/h_needle",
  "*andaman/boss/h_sig",                 // its signature pair (hardkit.THEMES)
  "*andaman/boss/h_sigb",
  "*sunda/boss/h_comma",              // ... and hers
  "*sunda/boss/h_twist",
  "*sunda/boss/h_snipe",
  "*sunda/boss/h_lock",
  "*sunda/boss/h_drift",
  "*sunda/boss/h_hesitate",
  "*sunda/boss/h_needle",
  "*sunda/boss/h_sig",                 // its signature pair (hardkit.THEMES)
  "*sunda/boss/h_sigb",
};
static const char* const kGenerics[] = {
  "*sunda/boss/coel",
  "*sunda/boss/coel_glow",            // her cockpit's lamps chasing round (glow.h)
  "*sunda/boss/bay_vis",
  "*sunda/boss/maw_vis",
  "*sunda/boss/tube_vis",
  "*sunda/boss/turret_vis",
  "*sunda/boss/lure_vis",
  "*sunda/boss/torp_vis",
  "*sunda/boss/tendril",
  "*sunda/boss/claw",
  "*sunda/boss/proto",
  "*sunda/boss/splash",
  "*sunda/boss/boil",
  "*sunda/boss/jam",
  // UNITY's ships, the Valhalla, their shells, fires and blasts (Mission 9's)
  "*andaman/boss/crosshair",
  "*andaman/boss/frigate",
  "*andaman/boss/nturret",
  "*andaman/boss/wake",
  "*andaman/boss/tracer",
  "*andaman/boss/bastion",
  "*andaman/boss/bturret",
  "*andaman/boss/ciws",
  "*andaman/boss/fire",
  "*andaman/boss/blast",
  "*andaman/boss/lance",
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

void RegisterSunda(void) {
  for (u32 i = 0; i < COUNT(kBullets); i++) SU_LoadBullet(BH, kBullets[i]);
  for (u32 i = 0; i < COUNT(kGenerics); i++) SU_LoadGeneric(BH, kGenerics[i]);
  for (u32 i = 0; i < COUNT(kBadguys); i++) SU_LoadBadGuy(BH, kBadguys[i]);
}

void ForgetDefinitions(const char* const* badguys, u32 nb, const char* const* bullets, u32 nu,
                       const char* const* generics, u32 ng);
void ForgetSunda(void) {
  ForgetDefinitions(kBadguys, COUNT(kBadguys), kBullets, COUNT(kBullets), kGenerics, COUNT(kGenerics));
}

// ---- the fleet ---------------------------------------------------------------------------
// The Bastion low in the middle, her frigates either side of her bow, all
// sailing up the screen at the scroll's speed: they hold their places, and
// only the camera's pan moves them (AnGroundDx: they are on the sea).
#define FLEET_BX     200
#define FLEET_BY     430
#define FRIGATE_X0   112
#define FRIGATE_X1   288
#define FRIGATE_Y    352
#define BT_EVERY     70             // the Bastion's main guns, at what comes up
#define BT_DMG       60
#define BT_RANGE     300
#define FR_EVERY     54
#define FR_DMG       40
#define CIWS_EVERY   46             // her CIWS on a torpedo: now and then, not every one
#define CIWS_RANGE   150
#define NFIRES       4
// At the start the fleet is up ahead: the ships come down the screen to
// their stations as the pilot catches up with them
#define INTRO_DY     640
#define INTRO_SPEED  FX(2)
#define SH_HITLOCK2  0x5188         // u16: the engine's spawn shield, its frames left (the bubble)

static const signed short kFireX[NFIRES] = { -18, 22, -10, 14 };
static const signed short kFireY[NFIRES] = { -96, -60, -20, 30 };

static struct {
  int on, said, t;
  int intro;                        // the fleet still coming down to its stations
  // the Bastion
  u32 bastion, btur[2], ciws[2], fire[NFIRES];
  s32 bx, by, by_want;
  s32 btur_ang[2], ciws_ang[2];
  int btur_cool[2], btur_flash[2], ciws_cool[2], ciws_flash[2];
  int hits, fires, scarred;
  // her frigates: 0 the Lance (to port), 1 the Pike
  AnShip fr[2];
  s32 fr_want_x[2], fr_want_y[2];
  struct {                          // each one's end: a spread, burning, going down
    int state, t;
    u32 torp[2];
    s32 tx[2], ty[2], tdx[2], tdy[2];
    s32 x0, y0;                     // where the spread comes from
  } sink[2];
  int guns;                         // the fleet fires on what comes up
  // the ambush: the first spread, unseen until it strikes, and the shape
  // passing under
  int amb, amb_t, amb_line;
  u32 torp[3], shape;
  s32 tx[3], ty[3], tdx[3], tdy[3];
  s32 sx, sy;
  int seen_remora;
  int shell_on;                     // the Bastion's main guns on a point (the boarding) ...
  s32 shell_x, shell_y;             // ... this one
} F;

enum { SAID_BRIEF = 1, SAID_ARGUS = 2, SAID_LATE = 4, SAID_REMORA = 8 };
enum { AMB_OFF, AMB_RUN, AMB_SHAPE, AMB_DONE };
enum { FS_NONE, FS_RUN, FS_SINK, FS_GONE };

void SundaFleetPos(s32* x, s32* y) {
  *x = F.bx + AnGroundDx(F.by);
  *y = F.by;
}

void SundaBridge(s32* x, s32* y) {      // the Bastion's bridge, where Argus stands
  *x = F.bx + AnGroundDx(F.by);
  *y = F.by - FX(30);
}

void SundaFleetTo(s32 y) { F.by_want = y; }

int SundaFleetSettled(void) { return AnAbs(F.by_want - F.by) < FX(1); }

void SundaFleetGuns(int on) { F.guns = on; }

// The Bastion's main guns laid on (x, y) -- Specimen Nine alongside, boarding
// -- shells bursting on her hull (she shrugs them off); on = 0 to stop.
void SundaFleetShellAt(s32 x, s32 y, int on) {
  F.shell_on = on;
  F.shell_x = x;
  F.shell_y = y;
}

static void BtTurretAt(int k, s32* x, s32* y) {
  *x = F.bx + AnGroundDx(F.by) + FX(k ? BASTION_TUR1_X : BASTION_TUR0_X);
  *y = F.by + FX(k ? BASTION_TUR1_Y : BASTION_TUR0_Y);
}

static void CiwsAt(int k, s32* x, s32* y) {
  *x = F.bx + AnGroundDx(F.by) + FX(k ? BASTION_CIWS1_X : BASTION_CIWS0_X);
  *y = F.by + FX(k ? BASTION_CIWS1_Y : BASTION_CIWS0_Y);
}

static void FleetUp(void) {
  F.bx = FX(FLEET_BX);
  F.by_want = FX(FLEET_BY);
  F.by = FX(FLEET_BY - INTRO_DY);
  F.intro = 1;
  // bottom to top: the frigates' wakes and hulls, the Bastion, her guns
  for (int i = 0; i < 2; i++) {
    F.fr_want_x[i] = FX(i ? FRIGATE_X1 : FRIGATE_X0);
    F.fr_want_y[i] = FX(FRIGATE_Y);
    ShipSpawn(&F.fr[i], SHIP_FRIGATE, F.fr_want_x[i], F.fr_want_y[i] - FX(INTRO_DY), UP);
  }
  F.bastion = AnSpawn("*andaman/boss/bastion", F.bx, F.by);
  for (int k = 0; k < 2; k++) {
    F.btur[k] = AnSpawn("*andaman/boss/bturret", F.bx, F.by);
    F.btur_ang[k] = UP;
    F.btur_cool[k] = 60 + 25 * k;
    F.ciws[k] = AnSpawn("*andaman/boss/ciws", F.bx, F.by);
    F.ciws_ang[k] = UP;
  }
  // her fires, made now and kept out of sight until she burns; then the
  // Valhalla's pieces -- so that she draws over the fleet and its fires
  // (a generic draws over those made before it)
  for (int i = 0; i < NFIRES; i++) {
    F.fire[i] = AnSpawn("*andaman/boss/fire", F.bx, F.by);
    AnPut(F.fire[i], FX(-400), FX(-400));
  }
  AnValPrespawn();
}

static void FleetPlace(void) {
  if (F.intro && AnAbs(F.by_want - F.by) < FX(1)) F.intro = 0;
  F.by += Ease(F.by_want - F.by, F.intro ? INTRO_SPEED : FX(0.6));
  AnPut(F.bastion, F.bx + AnGroundDx(F.by), F.by);
  AnFrame(F.bastion, F.scarred);
  s32 x, y;
  for (int k = 0; k < 2; k++) {
    BtTurretAt(k, &x, &y);
    AnPut(F.btur[k], x, y);
    AnTurn(F.btur[k], F.btur_ang[k]);
    if (F.btur_flash[k] > 0) F.btur_flash[k]--;
    AnFrame(F.btur[k], F.btur_flash[k] > 0);
    if (F.btur_cool[k] > 0) F.btur_cool[k]--;
    CiwsAt(k, &x, &y);
    AnPut(F.ciws[k], x, y);
    AnTurn(F.ciws[k], F.ciws_ang[k]);
    if (F.ciws_flash[k] > 0) F.ciws_flash[k]--;
    AnFrame(F.ciws[k], F.ciws_flash[k] > 0);
    if (F.ciws_cool[k] > 0) F.ciws_cool[k]--;
  }
  for (int i = 0; i < F.fires; i++)
    AnPut(F.fire[i], F.bx + AnGroundDx(F.by) + FX(kFireX[i]), F.by + FX(kFireY[i]));
  for (int i = 0; i < 2; i++) {
    AnShip* s = &F.fr[i];
    if (!s->hull) continue;
    if (F.sink[i].state < FS_SINK) {
      s->x += Ease(F.fr_want_x[i] - s->x, FX(0.4));
      s->y += Ease(F.fr_want_y[i] - s->y, F.intro ? INTRO_SPEED : FX(0.4));
    }
    ShipPlace(s);
  }
}

// A hit on the Bastion: a blast where it struck, and she burns a little more.
static void BastionHit(s32 x, s32 y) {
  Blast("ExplosionL", x, y);
  SB_ShakeWorld();
  F.hits++;
  F.scarred = 1;
  if (F.fires < NFIRES && (F.hits == 1 || F.hits == 3 || F.hits == 6 || F.hits == 10)) F.fires++;   // (one more lit)
}

// The fleet's guns: the Bastion's main turrets and the frigates' on the
// nearest thing up and in reach, her CIWS now and then on a torpedo.
static u32 NearestUp(s32 x, s32 y, s32 range, int torpedoes, s32* ox, s32* oy) {
  u32 best = 0;
  s32 bd = range;
  for (u32 b = SB_GetFirstBadGuy(BH); b; b = SB_GetNextBadGuy(BH, b)) {
    if (SB_GetBadGuyHealth(b) <= 0) continue;
    const s32 mark = GetReg(b, 21) >> 16;
    if ((mark == TORPEDO_MARK) != torpedoes) continue;
    if (mark == REMORA_MARK && GetReg(b, 15)) continue;           // under the water
    s32 mx, my;
    SB_GetActorPosition(Act(b), &mx, &my);
    if (my < FX(10) || my > FX(400) || mx < FX(0) || mx > FX(400)) continue;
    const s32 d = AnDist(mx - x, my - y);
    if (d < bd) { bd = d; best = b; *ox = mx; *oy = my; }
  }
  return best;
}

extern u32 g_sunda_spare;         // coelacanth.c: what the fleet must not fire on (her hull while she dives)

static void FleetGuns(void) {
  if (F.shell_on) {
    for (int k = 0; k < 2; k++) {
      s32 x, y;
      BtTurretAt(k, &x, &y);
      const s32 tx = F.shell_x + FX(Rnd(-20, 20)), ty = F.shell_y + FX(Rnd(-40, 40));
      const s32 want = AnAngle(x, y, tx, ty);
      F.btur_ang[k] = AnTurnTo(F.btur_ang[k], want, TURN / 160);
      if (F.btur_cool[k] || AnOff(F.btur_ang[k], want) > TURN / 40) continue;
      const s32 mx = x + SB_Cos(F.btur_ang[k]) * 22, my = y + SB_Sin(F.btur_ang[k]) * 22;
      if (!AnShoot(mx, my, tx, ty, 8, SHOT_BIG, TAG_NONE, 0, 0)) continue;
      Boom("ExplosionXXS", mx, my);
      F.btur_cool[k] = 34 + 9 * k;
      F.btur_flash[k] = 8;
    }
    return;
  }
  if (!F.guns) return;
  for (int k = 0; k < 2; k++) {
    s32 x, y, tx, ty;
    BtTurretAt(k, &x, &y);
    const u32 t = NearestUp(x, y, BT_RANGE, 0, &tx, &ty);
    if (!t || t == g_sunda_spare) { F.btur_ang[k] = AnTurnTo(F.btur_ang[k], UP, TURN / 300); continue; }
    const s32 want = AnAngle(x, y, tx, ty);
    F.btur_ang[k] = AnTurnTo(F.btur_ang[k], want, TURN / 200);
    if (F.btur_cool[k] || AnOff(F.btur_ang[k], want) > TURN / 64) continue;
    const s32 mx = x + SB_Cos(F.btur_ang[k]) * 22, my = y + SB_Sin(F.btur_ang[k]) * 22;
    if (!AnShoot(mx, my, tx, ty, 8, SHOT_MEDIUM, TAG_NONE, t, BT_DMG)) continue;
    F.btur_cool[k] = BT_EVERY + 13 * k;
    F.btur_flash[k] = 8;
  }
  for (int k = 0; k < 2; k++) {
    s32 x, y, tx, ty;
    CiwsAt(k, &x, &y);
    if (F.ciws_cool[k]) continue;
    const u32 t = NearestUp(x, y, CIWS_RANGE, 1, &tx, &ty);
    if (!t) continue;
    F.ciws_ang[k] = AnAngle(x, y, tx, ty);
    if (AnShoot(x, y, tx, ty, 11, SHOT_SMALL, TAG_NONE, t, 60)) {
      F.ciws_cool[k] = CIWS_EVERY + 9 * k;
      F.ciws_flash[k] = 6;
    }
  }
  for (int i = 0; i < 2; i++) {
    AnShip* s = &F.fr[i];
    if (!s->hull || F.sink[i].state >= FS_SINK) continue;
    s32 x, y, tx, ty;
    ShipTurret(s, 0, &x, &y);
    const u32 t = NearestUp(x, y, BT_RANGE, 0, &tx, &ty);
    if (t && t != g_sunda_spare) ShipFire(s, 0, tx, ty, FR_EVERY + 7 * i, SHOT_SMALL, TAG_NONE, t, FR_DMG);
  }
}

// ---- torpedoes striking home, and the drones under the water ------------------------------
static void Torpedoes(void) {
  for (u32 b = SB_GetFirstBadGuy(BH); b; b = SB_GetNextBadGuy(BH, b)) {
    if (SB_GetBadGuyHealth(b) <= 0) continue;
    const s32 mark = GetReg(b, 21) >> 16;
    if (mark == REMORA_MARK) {
      // under the water it can't be hurt: invincible while its R15 is set
      if (!GetReg(b, 12)) {
        SB_SetInvincibility(b, 15);
        SetReg(b, 12, FX(1));
      }
      if (!F.seen_remora && !GetReg(b, 15)) F.seen_remora = 1;
      continue;
    }
    if (mark != TORPEDO_MARK || GetReg(b, REG_CMD)) continue;
    s32 x, y;
    SB_GetActorPosition(Act(b), &x, &y);
    const s32 bx = F.bx + AnGroundDx(F.by);
    int hit = 0;
    if (AnAbs(x - bx) < FX(44) && y > F.by - FX(146) && y < F.by + FX(150)) {
      BastionHit(x, y);
      hit = 1;
    }
    for (int i = 0; i < 2 && !hit; i++) {
      const AnShip* s = &F.fr[i];
      if (!s->hull || F.sink[i].state >= FS_SINK) continue;
      const s32 fx = s->x + AnGroundDx(s->y);
      if (AnAbs(x - fx) < FX(16) && AnAbs(y - s->y) < FX(56)) {
        Blast("ExplosionL", x, y);
        hit = 1;
      }
    }
    if (hit) SetReg(b, REG_CMD, FX(1));       // (the script sees R20 and vanishes)
  }
}

// ---- the ambush --------------------------------------------------------------------------------
// Three torpedoes out of the east, unseen until they strike; the clock held
// for it, the ship safe. Then the shape of her passes under the fleet,
// bottom right to top left, while Dr. Marrow says hello.
static void Ambush(void) {
  switch (F.amb) {
  case AMB_RUN:
    if (F.amb_t == 1) {
      ShipShield(1);
      SU_PausePartition(BH, 1);
      for (int k = 0; k < 3; k++) {
        F.tx[k] = FX(430 + 26 * k);
        F.ty[k] = FX(250 + 40 * k);
        const s32 ex = F.bx + FX(30), ey = F.by - FX(120 - 40 * k);
        const s32 a = AnAngle(F.tx[k], F.ty[k], ex, ey);
        F.tdx[k] = SB_Cos(a) * 2;
        F.tdy[k] = SB_Sin(a) * 2;
        F.torp[k] = AnSpawn("*sunda/boss/torp_vis", FX(400), F.ty[k]);
        AnTurn(F.torp[k], a);
      }
    }
    for (int k = 0; k < 3; k++) {
      if (!F.torp[k]) continue;
      F.tx[k] += F.tdx[k];
      F.ty[k] += F.tdy[k];
      AnPut(F.torp[k], F.tx[k] + AnGroundDx(F.ty[k]), F.ty[k]);
      if (F.tx[k] < F.bx + FX(40)) {
        BastionHit(F.tx[k] + AnGroundDx(F.ty[k]) - FX(6), F.ty[k]);
        Blast("ExplosionXL", F.tx[k] - FX(20), F.ty[k] + FX(6));
        AnDrop(&F.torp[k]);
        if (k == 0) F.amb_line = AnLine(SL_AMB_HIT);     // Argus: torpedoes! we're hit
      }
    }
    if (!F.torp[0] && !F.torp[1] && !F.torp[2] && F.amb_t > 200) {
      F.amb = AMB_SHAPE;
      F.amb_t = 0;
      F.sx = FX(330);
      F.sy = FX(470);
      F.shape = AnSpawn("*sunda/boss/coel", FX(330), FX(400));
      AnFrame(F.shape, 3);                                  // a dark shape under the water
      AnTurn(F.shape, (UP - TURN / 14) & (TURN - 1));
      AnLine(SL_AMB_SONAR);                                 // the Doc: something huge on sonar
    }
    break;
  case AMB_SHAPE:
    F.sx -= FX(0.42);
    F.sy -= FX(1.25);
    AnPut(F.shape, F.sx + AnGroundDx(F.sy), F.sy);
    if (F.amb_t == 90) { AnLine(SL_MARROW_IN); AnLine(SL_MARROW_NAME); AnLine(SL_DOC_MARROW); }
    if (F.amb_t == 120) {
      ShipShield(0);
      SU_PausePartition(BH, 0);                             // and her drones come up
    }
    if (F.sy < FX(-150)) {
      AnDrop(&F.shape);
      F.amb = AMB_DONE;
    }
    break;
  default:
    break;
  }
  F.amb_t++;
}

// ---- the frigates' ends ---------------------------------------------------------------------
// A spread of two from (x0, y0): she is hit twice, burning; she heels over,
// drops astern down the screen and is gone. The Lance in the thick of the
// fight (from under the fleet, the port side); the Pike when Specimen Nine
// crash dives (coelacanth.c: SundaSinkPike, from where she went down).
static void Sink(int i) {
  AnShip* s = &F.fr[i];
  if (!s->hull) return;
  __typeof__(F.sink[0])* k_ = &F.sink[i];
  if (k_->state == FS_RUN) {
    if (k_->t == 1) {
      for (int k = 0; k < 2; k++) {
        k_->tx[k] = k_->x0 + FX(18 * k);
        k_->ty[k] = k_->y0 + FX(40 * k);
        const s32 a = AnAngle(k_->tx[k], k_->ty[k], s->x, s->y + FX(30 * k - 20));
        k_->tdx[k] = SB_Cos(a) * 2;
        k_->tdy[k] = SB_Sin(a) * 2;
        k_->torp[k] = AnSpawn("*sunda/boss/torp_vis", k_->x0 < FX(0) ? FX(0) : k_->x0, k_->ty[k]);
        AnTurn(k_->torp[k], a);
      }
    }
    for (int k = 0; k < 2; k++) {
      if (!k_->torp[k]) continue;
      k_->tx[k] += k_->tdx[k];
      k_->ty[k] += k_->tdy[k];
      AnPut(k_->torp[k], k_->tx[k] + AnGroundDx(k_->ty[k]), k_->ty[k]);
      // struck when it reaches her (or has run long enough to: never stuck)
      if (AnDist(k_->tx[k] - s->x, k_->ty[k] - s->y) < 30 || k_->t > 400) {
        Blast("ExplosionXL", s->x + AnGroundDx(s->y), k_->ty[k]);
        SB_ShakeWorld();
        AnDrop(&k_->torp[k]);
        if (k == 1) {
          k_->state = FS_SINK;
          k_->t = 0;
          AnDrop(&k_->torp[0]);
          if (i == 0) {
            AnLine(SL_FRIGATE_HIT);     // Argus: the Lance is breaking up
            AnLine(SL_MARROW_FRIGATE);  // Dr. Marrow: I'm keeping score!
          }                             // (the Pike's line called her spread: coelacanth.c PikeSpread)
        }
      }
    }
  } else if (k_->state == FS_SINK) {
    s->y += FX(0.32);
    s->head = (s->head + (i ? -TURN / 2400 : TURN / 2400)) & (TURN - 1);
    if ((k_->t & 15) == 0)
      Blast((k_->t & 16) ? "ExplosionL" : "ExplosionXS", s->x + AnGroundDx(s->y) + FX(Rnd(-10, 10)),
            s->y + FX(Rnd(-40, 40)));
    if (s->y > FX(470)) {
      ShipDrop(s);
      k_->state = FS_GONE;
    }
  }
  k_->t++;
}

static void StartSink(int i, s32 x0, s32 y0) {
  if (F.sink[i].state != FS_NONE) return;
  F.sink[i].state = FS_RUN;
  F.sink[i].t = 0;
  F.sink[i].x0 = x0;
  F.sink[i].y0 = y0;
}

void SundaSinkPike(s32 x0, s32 y0) { StartSink(1, x0, y0); }
int SundaPikeDown(void) { return F.sink[1].state >= FS_SINK; }

// Clear the sea: every torpedo, drone and seeker still out (a scene is
// about to be played over it).
void SundaClearSea(u32 spare) {
  for (u32 b = SB_GetFirstBadGuy(BH); b; b = SB_GetNextBadGuy(BH, b))
    if (b != spare && SB_GetBadGuyHealth(b) > 0) {
      SB_SetActorPosition(Act(b), FX(-400), FX(-400));      // (gone where its blast is not seen)
      Remove(b);
    }
}

// ---- a new run, and every frame ------------------------------------------------------------------
void AndamanReset(void);

void SundaReset(void) {
  AndamanReset();                              // the shells, blasts and the Valhalla: clean
  AnUseLines(kSundaSentences, SL_COUNT);       // ... with this world's speeches
  SB_ClearBytes(&F, sizeof F);
  F.on = 1;
  F.guns = 1;
}

void SundaTick(void) {
  if (R32(Shogun(), SH_WORLD_NO) != SUNDA || !F.on) return;
  if (!F.bastion) FleetUp();
  const s32 pt = AnTime();
  // no spawn shield at the start: its bubble sat on the Bastion's bow (and
  // nothing comes at the ship in the cruise)
  if (pt < 240) *(volatile unsigned short*)(Shogun() + SH_HITLOCK2) = 0;
  if (!(F.said & SAID_BRIEF) && pt >= S_FLY_BRIEF_T) { F.said |= SAID_BRIEF; AnLine(SL_FLY_BRIEF); }
  if (!(F.said & SAID_ARGUS) && pt >= S_FLY_ARGUS_T) { F.said |= SAID_ARGUS; AnLine(SL_FLY_ARGUS); }
  if (F.amb == AMB_OFF && pt >= S_AMBUSH_T) { F.amb = AMB_RUN; F.amb_t = 0; }
  if (F.amb != AMB_OFF) Ambush();
  if (pt >= S_FRIGATE_T) StartSink(0, FX(-40), FX(300));
  for (int i = 0; i < 2; i++) Sink(i);
  if (!(F.said & SAID_LATE) && pt >= S_LATE_T) { F.said |= SAID_LATE; AnLine(SL_ARGUS_LATE); }
  Torpedoes();
  FleetGuns();
  FleetPlace();
}
