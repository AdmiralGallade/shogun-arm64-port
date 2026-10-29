// ironreef.c -- Mission 9's final boss: the Iron Reef, and Admiral Vorn's
// battleship, the Ikazuchi.
//
// The fortress stands on Barren Island's volcano, the hub of the Shogun's
// electronic supply lines. It and its shield dome come down the screen with
// the island (andaman.c's riders: ReefRide), and with them, already there,
// already fighting, the naval battleship Bastion -- Captain Argus, a cyborg
// and the best ship-handler UNITY has -- and her two escorts,
// shelling the dome -- to no effect. Then:
//
//   shield    the scroll stops; the Doc: without its generators the shield's
//             capacitors are draining. The shells keep landing, the dome
//             flickers, fails, and bursts;
//   fallback  Argus: fire support. The Bastion falls back to the bottom of the
//             screen, her escorts out to the flanks; the sea gate opens;
//   emerge    the Ikazuchi sails out of the harbour, bow first, and turns
//             broadside. Vorn -- the Ocean boss, back from Greenland -- swears
//             revenge;
//   phase 1   the whole ship: two launchers on the bow and its cells. The
//             launchers are semi-aimed: they turn after the ship at their
//             own pace and fire curving missiles where they point; the cells
//             pop needles that climb and turn down on the ship. The citadel
//             (an emitter nothing can hurt) keeps up slow spirals to graze;
//             the fortress's two guns fire too, until shot out;
//   phase 2   the bow torn away (it drifts off, burning; the ship's shape
//             changes, the rest of her steadies): the stern's launchers, and
//             the Thousand Needles -- waves of needles from the stern's
//             battery, far too many to dodge. The Doc: the plasma sphere.
//             The Bastion's CIWS shoot down what they can (some);
//   phase 3   the stern torn away: the citadel alone. Its twin guns rise out
//             of their hatches, and the Thunder cannon out of the keep's
//             roof; the citadel's own cells join in with needles;
//   finale    every gun silent: the Bastion's main battery sinks her, and
//             then levels the fortress. Last words, and the level ends.
//
// All the while the Bastion lands her heavy shells on the fortress (it
// shows the damage), and a capsule comes down every so often (BossWards).
// The bar is each phase's targets; it refills for the next.

#include "andaman.h"

#define CX         FX(200)
#define FCX        (CX + AnGroundDx(Q.fy))   // the fortress's centre line, as the camera pans
#define SHIP_Y     178            // the Ikazuchi's station, broadside
#define FORT_LIFT  42             // the fight opens: the world scrolls back, the fortress this far up
#define LIFT_STEP  FX(0.0125)     // ... at this many rows a frame
#define FORT_SHOW_Y 165           // the bombardment: the fortress eased down to here, below the dialog box
#define CLAMP_GAP  8              // the ship is kept this far below her hull
#define BASTION_Y  420            // the Bastion's station for fire support
#define CIWS_RANGE 330
#define CIWS_EVERY 6
#define REG_MARK   15             // a missile's mark (the scripts leave it): seen, spared, hers, engaged
#define BT_EVERY   96             // the Bastion's main guns on the Ikazuchi ...
#define BT_DMG     60
#define BT_FOCUS_EVERY 64         // ... and on her launchers, once Argus gives the order: faster,
#define BT_FOCUS_DMG   120        // ... and twice as hard
#define FOCUS_AT   300            // the order: this far into phase one
#define ESC_STATION_Y 352         // the escorts, back for the barrage: either side of the pilot
#define ESC_RANGE  270
#define ESC_EVERY0 60             // their anti-air: a burst a second at first ...
#define ESC_EVERY1 10             // ... tightening to this, a notch every second of the fight
#define ESC_X0     110            // their stations, either side of the pilot (on screen
#define ESC_X1     290            // wherever the camera has panned)

// ---- what rides in: the fortress, the Bastion, her escorts ----------------------------------
static struct {
  int up_fort, up_bastion, up_escort;
  u32 fort, gate, core, dome, gun_vis[2];
  u32 bastion, btur[2], ciws[2];
  AnShip esc[2];
  s32 fy, by, bx, ey;
  int f_fort, f_gate, f_core, f_dome, f_gun[2];
  s32 btur_ang[2], ciws_ang[2], gun_aim[2];
  int btur_cool[2], btur_flash[2], ciws_cool[2], ciws_flash[2];
  int flare;
  s32 tx[2], ty[2], etx[2], ety[2];   // where each gun is laying its next shell
  int aimed[2], eaimed[2];
  u32 atk[2];                       // the Ikazuchi's gun each main gun is on (0: the fortress)
  int atk_dbl[2];                   // ... a launcher, under Argus's order: double damage
} Q;

static void FortPlace(void) {
  AnPut(Q.fort, FCX, Q.fy);
  AnFrame(Q.fort, Q.f_fort);
  AnPut(Q.gate, FCX + FX(FORT_GATE_X), Q.fy + FX(FORT_GATE_Y));
  AnFrame(Q.gate, Q.f_gate);
  AnPut(Q.core, FCX + FX(FORT_CORE_X), Q.fy + FX(FORT_CORE_Y));
  AnFrame(Q.core, Q.f_core);
  AnPut(Q.dome, FCX, Q.fy + FX(4));
  AnFrame(Q.dome, Q.f_dome);
  for (int i = 0; i < 2; i++) {
    AnPut(Q.gun_vis[i], FCX + FX(i ? FORT_GUN1_X : FORT_GUN0_X), Q.fy + FX(i ? FORT_GUN1_Y : FORT_GUN0_Y));
    AnTurn(Q.gun_vis[i], Q.gun_aim[i]);
    AnFrame(Q.gun_vis[i], Q.f_gun[i]);
  }
}

static void BtTurretAt(int k, s32* x, s32* y) {
  *x = Q.bx + AnGroundDx(Q.by) + FX(k ? BASTION_TUR1_X : BASTION_TUR0_X);
  *y = Q.by + FX(k ? BASTION_TUR1_Y : BASTION_TUR0_Y);
}

static void CiwsAt(int k, s32* x, s32* y) {
  *x = Q.bx + AnGroundDx(Q.by) + FX(k ? BASTION_CIWS1_X : BASTION_CIWS0_X);
  *y = Q.by + FX(k ? BASTION_CIWS1_Y : BASTION_CIWS0_Y);
}

static void BastionPlace(void) {
  AnPut(Q.bastion, Q.bx + AnGroundDx(Q.by), Q.by);
  s32 x, y;
  for (int k = 0; k < 2; k++) {
    BtTurretAt(k, &x, &y);
    AnPut(Q.btur[k], x, y);
    AnTurn(Q.btur[k], Q.btur_ang[k]);
    if (Q.btur_flash[k] > 0) Q.btur_flash[k]--;
    AnFrame(Q.btur[k], Q.btur_flash[k] > 0);
    if (Q.btur_cool[k] > 0) Q.btur_cool[k]--;
    CiwsAt(k, &x, &y);
    AnPut(Q.ciws[k], x, y);
    AnTurn(Q.ciws[k], Q.ciws_ang[k]);
    if (Q.ciws_flash[k] > 0) Q.ciws_flash[k]--;
    AnFrame(Q.ciws[k], Q.ciws_flash[k] > 0);
    if (Q.ciws_cool[k] > 0) Q.ciws_cool[k]--;
  }
}

// One of the Bastion's main turrets onto (tx, ty); a heavy shell once it bears.
static int BastionFire(int k, s32 tx, s32 ty, int every, int size, int tag, u32 bg, int dmg) {
  s32 x, y;
  BtTurretAt(k, &x, &y);
  const s32 want = AnAngle(x, y, tx, ty);
  Q.btur_ang[k] = AnTurnTo(Q.btur_ang[k], want, TURN / 220);
  if (Q.btur_cool[k] > 0 || AnOff(Q.btur_ang[k], want) > TURN / 80) return 0;
  const s32 mx = x + SB_Cos(Q.btur_ang[k]) * 22, my = y + SB_Sin(Q.btur_ang[k]) * 22;
  if (!AnShoot(mx, my, tx, ty, 8, size, tag, bg, dmg)) return 0;
  Boom("ExplosionXXS", mx, my);
  Q.btur_cool[k] = every;
  Q.btur_flash[k] = 8;
  return 1;
}

// A point on the near half of the dome.
static void DomeRim(s32* x, s32* y) {
  const s32 a = (s32)Rnd(12, 116) << 16;
  *x = FCX + SB_Cos(a) * 150;
  *y = Q.fy + FX(4) + SB_Sin(a) * 80;
}

// While the shield stands: the Bastion and the escorts shell it. Each gun
// lays on a point of the dome and fires when it bears, then picks another.
static void ShellDome(void) {
  if (!Q.dome || Q.by > FX(470)) return;
  for (int k = 0; k < 2; k++) {
    if (!Q.aimed[k]) { DomeRim(&Q.tx[k], &Q.ty[k]); Q.aimed[k] = 1; }
    if (BastionFire(k, Q.tx[k], Q.ty[k], 64 + 10 * k, SHOT_BIG, TAG_DOME, 0, 0)) Q.aimed[k] = 0;
  }
  for (int i = 0; i < 2; i++) {
    if (!Q.esc[i].hull || Q.esc[i].y > FX(430)) continue;
    if (!Q.eaimed[i]) { DomeRim(&Q.etx[i], &Q.ety[i]); Q.eaimed[i] = 1; }
    if (ShipFire(&Q.esc[i], 0, Q.etx[i], Q.ety[i], 44 + 9 * i, SHOT_MEDIUM, TAG_DOME, 0, 0)) Q.eaimed[i] = 0;
  }
  if (AnLanded(TAG_DOME)) Q.flare = 8;
  if (Q.flare > 0) Q.flare--;
}

static int g_fight;               // the boss is up: it places everything itself

void ReefRide(s32 fy, s32 by, s32 ey) {
  if (g_fight) return;
  if (!Q.up_fort) {
    Q.up_fort = 1;
    Q.fort = AnSpawn("*andaman/boss/fortress", CX, fy);
    Q.gate = AnSpawn("*andaman/boss/fort_gate", CX, fy);
    Q.core = AnSpawn("*andaman/boss/fort_core", CX, fy);
    for (int i = 0; i < 2; i++) {
      Q.gun_vis[i] = AnSpawn("*andaman/boss/fortgun_vis", CX, fy);
      Q.gun_aim[i] = DOWN;
    }
    Q.dome = AnSpawn("*andaman/boss/fort_dome", CX, fy);
  }
  if (!Q.up_bastion) {
    Q.up_bastion = 1;
    Q.bx = CX;
    Q.bastion = AnSpawn("*andaman/boss/bastion", CX, by);
    for (int k = 0; k < 2; k++) {
      Q.btur[k] = AnSpawn("*andaman/boss/bturret", CX, by);
      Q.btur_ang[k] = UP;
      Q.btur_cool[k] = 40 + 30 * k;
      Q.ciws[k] = AnSpawn("*andaman/boss/ciws", CX, by);
      Q.ciws_ang[k] = UP;
    }
  }
  if (!Q.up_escort && ey > FX(-60)) {
    Q.up_escort = 1;
    ShipSpawn(&Q.esc[0], SHIP_FRIGATE, FX(112), ey, UP);
    ShipSpawn(&Q.esc[1], SHIP_FRIGATE, FX(288), ey, UP);
  }
  Q.fy = fy;
  Q.by = by;
  Q.ey = ey;
  for (int i = 0; i < 2; i++) {
    if (!Q.esc[i].hull) continue;
    Q.esc[i].y = ey;
    ShipPlace(&Q.esc[i]);
  }
  if (by < FX(440)) ShellDome();
  FortPlace();
  BastionPlace();
}

void ReefForget(void) {
  SB_ClearBytes(&Q, sizeof Q);
  g_fight = 0;
}

// ---- the fight's state -----------------------------------------------------------------------
enum { F_INTRO = 1, F_COLLAPSE, F_FALLBACK, F_EMERGE, F_P1, F_BREAK1, F_P2, F_BREAK2, F_P3, F_FINALE, F_SINK,
       F_FORT, F_END };
enum { SEC_BOW, SEC_MID, SEC_STERN };
// the Ikazuchi's guns: launchers 0-3, the bow's cells, the stern's battery,
// the citadel's guns 0-1, the Thunder cannon
enum { G_L0, G_L1, G_L2, G_L3, G_VLS, G_BARRAGE, G_GUN0, G_GUN1, G_MAIN, NGUNS };
static const signed short kGunX[NGUNS] = { IKA_LAUNCHER0_X, IKA_LAUNCHER1_X, IKA_LAUNCHER2_X, IKA_LAUNCHER3_X,
                                           IKA_VLS_X, IKA_BARRAGE_X, IKA_GUN0_X, IKA_GUN1_X, IKA_MAIN_X };
static const signed short kGunY[NGUNS] = { IKA_LAUNCHER0_Y, IKA_LAUNCHER1_Y, IKA_LAUNCHER2_Y, IKA_LAUNCHER3_Y,
                                           IKA_VLS_Y, IKA_BARRAGE_Y, IKA_GUN0_Y, IKA_GUN1_Y, IKA_MAIN_Y };
static const unsigned char kGunSec[NGUNS] = { SEC_BOW, SEC_BOW, SEC_STERN, SEC_STERN, SEC_BOW, SEC_STERN,
                                              SEC_MID, SEC_MID, SEC_MID };
// the frames of what is seen of each (anda_cast.py): idle, firing, struck, wrecked
static const unsigned char kWreck[NGUNS] = { 3, 3, 3, 3, 3, 3, 3, 3, 5 };
static const unsigned char kStruck[NGUNS] = { 2, 2, 2, 2, 0, 0, 2, 2, 4 };
static const unsigned char kFire[NGUNS] = { 1, 1, 1, 1, 2, 2, 1, 1, 2 };
static const unsigned char kIdle[NGUNS] = { 0, 0, 0, 0, 0, 0, 0, 0, 1 };

static struct {
  u32 boss;
  int phase, t;
  // the ship, and her sections (each placed by its own transform once torn away)
  s32 sx, sy, head, cx_want, drift_t;
  u32 sec[3];
  int sec_f[3], loose[3], sec_t[3];
  s32 sec_x[3], sec_y[3], sec_rot[3], sec_vx[3], sec_vy[3];
  // her guns: what is seen, the emitters, their aims
  u32 vis[NGUNS], gun[NGUNS], core;
  s32 aim[NGUNS], hp[NGUNS];
  int f[NGUNS], flash[NGUNS], cool[NGUNS], dead[NGUNS], rise[NGUNS];
  int core_cool, core_turn, main_turn, vls_turn, l2_turn;
  // the fortress's guns
  u32 fgun[2];
  s32 fgun_hp[2];
  int fgun_cool[2], fgun_turn[2], fgun_flash[2];
  int fort_hits, said_barrage, barrage_cool;
  int salvo, landed, flyby_at;
  s32 maxhp, by_want;               // where the Bastion keeps station in the fight
  s32 lift_to;                      // where the fortress is going, up the screen (0: there)
  int lift_t;
  int mark_n, bt_turn;              // the missiles' count, a third hers; her guns' turn
  int focus_line;                   // Argus's order (every gun on the launchers): its ticket
  int esc_on, esc_t, esc_every;     // the escorts back on the pilot's flanks; their AA's ramp
  int fire_end;                     // the last shell into the fortress: the explosions run on to it
} R;

// ---- where everything is --------------------------------------------------------------------
static s32 SecOff(int i) { return i == SEC_BOW ? -IKA_SEC : (i == SEC_STERN ? IKA_SEC : 0); }

static void SecTransform(int i, s32* x, s32* y, s32* rot) {
  if (R.loose[i]) {
    *x = R.sec_x[i];
    *y = R.sec_y[i];
    *rot = R.sec_rot[i];
    return;
  }
  s32 ox, oy;
  AnRot(SecOff(i), 0, R.head, &ox, &oy);
  *x = R.sx + ox;
  *y = R.sy + oy;
  *rot = R.head;
}

// A piece at (dx, dy) on the ship (from her centre), carried by section i.
static void OnShip(int i, s32 dx, s32 dy, s32* x, s32* y) {
  s32 sx, sy, rot, ox, oy;
  SecTransform(i, &sx, &sy, &rot);
  AnRot(dx - SecOff(i), dy, rot, &ox, &oy);
  *x = sx + ox + AnGroundDx(sy);              // (on the sea)
  *y = sy + oy;
}

static void GunAt(int g, s32* x, s32* y) { OnShip(kGunSec[g], kGunX[g], kGunY[g], x, y); }

static void PlaceShip(void) {
  for (int i = 0; i < 3; i++) {
    if (!R.sec[i]) continue;
    s32 x, y, rot;
    SecTransform(i, &x, &y, &rot);
    AnPut(R.sec[i], x + AnGroundDx(y), y);
    AnTurn(R.sec[i], rot);
    AnFrame(R.sec[i], R.sec_f[i]);
  }
  for (int g = 0; g < NGUNS; g++) {
    s32 x, y;
    GunAt(g, &x, &y);
    if (R.vis[g]) {
      AnPut(R.vis[g], x, y);
      s32 sx, sy, rot;
      SecTransform(kGunSec[g], &sx, &sy, &rot);
      // the cells and the battery lie flat on the deck; the rest turn
      AnTurn(R.vis[g], g == G_VLS || g == G_BARRAGE ? rot : R.aim[g]);
      AnFrame(R.vis[g], R.f[g]);
    }
    if (R.gun[g] && Alive(R.gun[g])) Place(R.gun[g], x, y);
  }
  if (R.core && Alive(R.core)) {
    s32 x, y;
    OnShip(SEC_MID, IKA_MAIN_X, IKA_MAIN_Y, &x, &y);
    Place(R.core, x, y);
  }
}

// ---- spawning ---------------------------------------------------------------------------------
static const char* const kVis[NGUNS] = {
  "*andaman/boss/launcher_vis", "*andaman/boss/launcher_vis", "*andaman/boss/launcher_vis",
  "*andaman/boss/launcher_vis", "*andaman/boss/vls_vis", "*andaman/boss/barrage_vis", "*andaman/boss/gun_vis",
  "*andaman/boss/gun_vis", "*andaman/boss/main_vis" };
static const char* const kGun[NGUNS] = {
  "*andaman/boss/ika_launcher", "*andaman/boss/ika_launcher", "*andaman/boss/ika_launcher",
  "*andaman/boss/ika_launcher", "*andaman/boss/ika_vls", "*andaman/boss/ika_barrage", "*andaman/boss/ika_gun",
  "*andaman/boss/ika_gun", "*andaman/boss/ika_main" };

static void ShipUp(void) {
  R.sx = CX;
  R.sy = Q.fy + FX(FORT_GATE_Y - 156);
  R.head = 0xc00000;                     // bow down, out through the gate
  R.cx_want = CX;
  static const char* const kSec[3] = { "*andaman/boss/ika_bow", "*andaman/boss/ika_mid", "*andaman/boss/ika_stern" };
  for (int i = 0; i < 3; i++) R.sec[i] = AnSpawn(kSec[i], R.sx, R.sy);
  for (int g = 0; g <= G_BARRAGE; g++) {
    R.vis[g] = AnSpawn(kVis[g], R.sx, R.sy);
    R.aim[g] = DOWN;
  }
  PlaceShip();
}

static void Arm(int g) {
  s32 x, y;
  GunAt(g, &x, &y);
  R.gun[g] = SB_SpawnBadGuy(BH, kGun[g], x, y);
  Place(R.gun[g], x, y);
  R.hp[g] = SB_GetBadGuyHealth(R.gun[g]);
  R.cool[g] = 60 + 23 * g;
}

static s32 Hp(u32 bg) { return Alive(bg) ? SB_GetBadGuyHealth(bg) : 0; }

static s32 PhaseHp(void) {
  s32 sum = 0;
  int lo = 0, hi = -1;
  if (R.phase == F_P1) { lo = G_L0; hi = G_VLS; }
  if (R.phase == F_P2) { lo = G_L2; hi = G_BARRAGE; }
  if (R.phase == F_P3) { lo = G_GUN0; hi = G_MAIN; }
  for (int g = lo; g <= hi; g++) {
    if (R.phase == F_P1 && (g == G_L2 || g == G_L3)) continue;
    if (R.phase == F_P2 && g == G_VLS) continue;
    sum += Hp(R.gun[g]);
  }
  return sum;
}

static s32 RfHealth(u32 boss) {
  (void)boss;
  return PhaseHp();
}

static void RfRender(u32 boss, s32 x, s32 y) { (void)boss; (void)x; (void)y; }

static void StartPhase(int phase) {
  R.phase = phase;
  R.t = 0;
  R.maxhp = PhaseHp();
  R32(R.boss, B_MAXHP) = R.maxhp;
}

static void RfSpawn(u32 boss) {
  R.boss = boss;
  if (!Q.up_fort) ReefRide(FX(FORT_Y), FX(348), FX(330));    // (forced early: levelkit --boss final)
  g_fight = 1;
  R.phase = F_INTRO;
  R.t = 0;
  Q.aimed[0] = Q.aimed[1] = 0;
  R32(boss, B_STATE) = 1;
  SB_PausePartition(BH, 1);
  g_autopilot_target = 0;
  AnLine(L_FORT_DOC);               // the Doc: its capacitors are draining
}

// ---- the guns --------------------------------------------------------------------------------
static int Live(int g) { return R.gun[g] && Alive(R.gun[g]); }

// Every frame, each gun: its semi-aim, its look, its flashes; and one lost
// is wrecked where it stands.
static void Guns(int scene) {
  const s32 px = PlayerX(), py = PlayerY();
  for (int g = 0; g < NGUNS; g++) {
    if (!R.vis[g]) continue;
    s32 x, y;
    GunAt(g, &x, &y);
    if (R.gun[g] && !R.dead[g] && !Alive(R.gun[g])) {
      R.dead[g] = 1;
      R.gun[g] = 0;                 // (its handle goes to the next badguy spawned)
      Blast("ExplosionXL", x, y);
      Blast("ExplosionL", x + FX(Rnd(-14, 14)), y + FX(Rnd(-12, 12)));
      SB_ShakeWorld();
    }
    if (R.dead[g]) { R.f[g] = kWreck[g]; continue; }
    if (R.rise[g] > 0) {
      R.rise[g]--;
      R.f[g] = g == G_MAIN ? 0 : 4;
      continue;
    }
    // the semi-aim: turned after the ship at the gun's own pace -- it lags
    // the ship's dodges, and fires where it points
    const s32 rate = g <= G_L3 ? TURN / 520 : (g == G_MAIN ? TURN / 600 : TURN / 300);
    R.aim[g] = AnTurnTo(R.aim[g], AnAngle(x, y, px, py), rate);
    if (!R.gun[g]) { R.f[g] = kIdle[g]; continue; }
    const s32 h = Hp(R.gun[g]);
    if (h < R.hp[g]) R.flash[g] = 5;
    R.hp[g] = h;
    if (R.flash[g] > 0) R.flash[g]--;
    R.f[g] = R.flash[g] && kStruck[g] ? kStruck[g] : (Busy(R.gun[g]) ? kFire[g] : kIdle[g]);
    if (g == G_VLS || g == G_BARRAGE) R.f[g] = Busy(R.gun[g]) ? 2 : 0;
    Guard(R.gun[g], scene);
  }
}

static int Fire(int g, int cmd, int every) {
  if (!Live(g) || Busy(R.gun[g]) || R.rise[g] > 0) return 0;
  if (--R.cool[g] > 0) return 0;
  if (cmd == A_THUNDER) {
    s32 x, y;
    GunAt(g, &x, &y);
    SetReg(R.gun[g], 12, FX(FlakSteps(x, y, 4)));
  }
  OrderAt(R.gun[g], cmd, R.aim[g]);
  R.cool[g] = HardMode() ? every - (every >> 2) : every;      // hard mode: a quarter sooner
  return 1;
}

// Hard mode (docs/HARD_MODE.md): each gun takes its own list, the hard attacks
// woven in, in turn (a counter a gun; no division in the blob).
static unsigned char g_hturn[NGUNS];
static int FireList(int g, const unsigned char* list, int n, int every) {
  if (g_hturn[g] >= n) g_hturn[g] = 0;
  if (!Fire(g, list[g_hturn[g]], every)) return 0;
  if (++g_hturn[g] >= n) g_hturn[g] = 0;
  return 1;
}
static const unsigned char kL0H[] = { A_SALVO, A_XPINCER, A_SALVO, A_XTWIST };
static const unsigned char kL1H[] = { A_SALVO, A_XSNIPE, A_SALVO, A_XGAP };
static const unsigned char kVlsH[] = { A_POP, A_CURL, A_XSNAKE };
static const unsigned char kL2H[] = { A_SALVO, A_HOOK, A_XPINCER, A_SALVO, A_XGAP };
static const unsigned char kL3H[] = { A_SALVO, A_XSNIPE, A_SALVO, A_XTWIST };
static const unsigned char kG0H[] = { A_GUNS, A_SCISSOR, A_XPINCER, A_GUNS };
static const unsigned char kG1H[] = { A_GUNS, A_XSNIPE, A_GUNS, A_SPOKES };
static const unsigned char kMainH[] = { A_THUNDER, A_XTWIST, A_ROSACE, A_WALL, A_XGAP, A_WHEEL };

// The citadel: something to graze, always; in the last phase, needles.
static void Core(int scene) {
  if (!R.core || scene || Busy(R.core)) return;
  if (--R.core_cool > 0) return;
  if (R.phase == F_P3 && (R.core_turn & 1)) {
    OrderAt(R.core, A_NEEDLES, DOWN);
    R.core_cool = 560;
  } else if (R.phase != F_P1 && (R.core_turn & 3) == 2) {
    OrderAt(R.core, A_STAR, DOWN);
    R.core_cool = 230;
  } else if ((R.core_turn & 3) == 1) {
    OrderAt(R.core, A_VORTEX, DOWN);
    R.core_cool = 230;
  } else {
    OrderAt(R.core, (R.core_turn & 2) ? A_RING : A_SPIRAL, DOWN);
    R.core_cool = R.phase == F_P1 ? 230 : 190;
  }
  if (HardMode()) R.core_cool -= R.core_cool >> 2;         // hard mode: a quarter sooner
  R.core_turn++;
}

// The fortress's own guns, at the ship, until they are shot out.
static void FortGuns(int scene) {
  for (int i = 0; i < 2; i++) {
    const u32 g = R.fgun[i];
    s32 x = FCX + FX(i ? FORT_GUN1_X : FORT_GUN0_X), y = Q.fy + FX(i ? FORT_GUN1_Y : FORT_GUN0_Y);
    if (!g) continue;
    if (!Alive(g)) {
      Blast("ExplosionXL", x, y);
      SB_ShakeWorld();
      R.fgun[i] = 0;
      Q.f_gun[i] = 3;
      continue;
    }
    Place(g, x, y);
    Guard(g, scene);
    Q.gun_aim[i] = AnTurnTo(Q.gun_aim[i], AnAngle(x, y, PlayerX(), PlayerY()), TURN / 300);
    const s32 h = Hp(g);
    if (h < R.fgun_hp[i]) R.fgun_flash[i] = 5;
    R.fgun_hp[i] = h;
    if (R.fgun_flash[i] > 0) R.fgun_flash[i]--;
    Q.f_gun[i] = R.fgun_flash[i] ? 2 : (Busy(g) ? 1 : 0);
    if (scene || Busy(g) || --R.fgun_cool[i] > 0) continue;
    OrderAt(g, (R.fgun_turn[i]++ & 1) ? A_RAIN : A_PEPPER, Q.gun_aim[i]);
    R.fgun_cool[i] = 170 + 40 * i;
  }
}

// Every missile the garrison puts up is counted as it is first seen: one in
// three is the Bastion's to stop (REG_MARK 2), the rest are the pilot's (1).
static void MarkMissiles(void) {
  for (u32 b = SB_GetFirstBadGuy(BH); b; b = SB_GetNextBadGuy(BH, b)) {
    if (SB_GetBadGuyHealth(b) <= 0 || (GetReg(b, 21) >> 16) != MISSILE_MARK || GetReg(b, REG_MARK)) continue;
    SetReg(b, REG_MARK, FX(R.mark_n == 0 ? 2 : 1));
    R.mark_n = R.mark_n == 2 ? 0 : R.mark_n + 1;
  }
}

// The nearest missile marked hers and not yet engaged, in reach of (x, y).
static u32 HerMissile(s32 x, s32 y, s32* ox, s32* oy) {
  u32 best = 0;
  s32 bd = CIWS_RANGE;
  for (u32 b = SB_GetFirstBadGuy(BH); b; b = SB_GetNextBadGuy(BH, b)) {
    if (SB_GetBadGuyHealth(b) <= 0 || (GetReg(b, 21) >> 16) != MISSILE_MARK || GetReg(b, REG_MARK) != FX(2)) continue;
    s32 mx, my;
    SB_GetActorPosition(Act(b), &mx, &my);
    if (my < FX(-10) || my > FX(400)) continue;
    const s32 d = AnDist(mx - x, my - y);
    if (d < bd) { bd = d; best = b; *ox = mx; *oy = my; }
  }
  return best;
}

// A gun of the Ikazuchi's still fighting (the phase's own first), for the
// Bastion's main battery; 0 if none.
static u32 IkaGun(s32* x, s32* y) {
  for (int i = 0; i < NGUNS; i++) {
    const int g = (R.bt_turn + i) < NGUNS ? R.bt_turn + i : R.bt_turn + i - NGUNS;
    if (!Live(g) || R.rise[g] > 0) continue;
    GunAt(g, x, y);
    return R.gun[g];
  }
  return 0;
}

// Argus's order is out (and up on the screen): every gun on the launchers.
static int Focus(void) { return R.focus_line && AnLineUp(R.focus_line); }

// A missile launcher of the Ikazuchi's still firing, turn about; 0 if none.
static u32 Launcher(s32* x, s32* y) {
  for (int i = 0; i < 4; i++) {
    const int g = G_L0 + ((R.bt_turn + i) & 3);
    if (!Live(g) || R.rise[g] > 0) continue;
    GunAt(g, x, y);
    return R.gun[g];
  }
  return 0;
}

// The Bastion in the fight: her main battery on the Ikazuchi's guns, turn
// and turn about with the fortress -- or, once Argus orders it, every gun on
// her launchers, twice as hard; her CIWS on her third of the missiles.
static void Support(int scene) {
  if (!scene) {
    const int focus = Focus();
    for (int k = 0; k < 2; k++) {
      if (!Q.aimed[k]) {
        s32 x, y;
        u32 g = focus ? Launcher(&x, &y) : 0;
        Q.atk_dbl[k] = g != 0;
        if (!g) g = (k == 0 || (R.bt_turn & 1)) ? IkaGun(&x, &y) : 0;
        Q.atk[k] = g;
        if (g) {
          Q.tx[k] = x + FX(Rnd(-8, 8));
          Q.ty[k] = y + FX(Rnd(-6, 6));
        } else {
          Q.tx[k] = FCX + FX(Rnd(-110, 110));
          Q.ty[k] = Q.fy + FX(Rnd(-50, 40));
        }
        Q.aimed[k] = 1;
      }
      const int on_ship = Q.atk[k] != 0 && Alive(Q.atk[k]);
      const int dbl = on_ship && Q.atk_dbl[k];
      if (BastionFire(k, Q.tx[k], Q.ty[k], dbl ? BT_FOCUS_EVERY + 12 * k : BT_EVERY + 30 * k,
                      on_ship ? SHOT_BIG : SHOT_HUGE, on_ship ? TAG_NONE : TAG_FORT, on_ship ? Q.atk[k] : 0,
                      dbl ? BT_FOCUS_DMG : BT_DMG)) {
        Q.aimed[k] = 0;
        R.bt_turn = R.bt_turn + 1 < NGUNS ? R.bt_turn + 1 : 0;
      }
    }
  }
  R.fort_hits += AnLanded(TAG_FORT);
  if (R.fort_hits >= 6 && Q.f_fort == 0) Q.f_fort = 1;
  // CIWS: her third of the missiles, each engaged once, every one stopped
  MarkMissiles();
  for (int k = 0; k < 2; k++) {
    s32 cx, cy, mx = 0, my = 0;
    CiwsAt(k, &cx, &cy);
    const u32 m = HerMissile(cx, cy, &mx, &my);
    if (!m) { Q.ciws_ang[k] = AnTurnTo(Q.ciws_ang[k], UP, TURN / 120); continue; }
    const s32 want = AnAngle(cx, cy, mx, my);
    Q.ciws_ang[k] = AnTurnTo(Q.ciws_ang[k], want, TURN / 16);
    if (Q.ciws_cool[k] > 0 || AnOff(Q.ciws_ang[k], want) > TURN / 24) continue;
    if (!AnShoot(cx, cy, mx, my, 12, SHOT_MEDIUM, TAG_MISSILE, m, 200)) continue;
    SetReg(m, REG_MARK, FX(3));             // engaged
    // a stream, not a single round: tracers either side of the one that hits
    AnShoot(cx, cy, mx + FX(Rnd(-14, 14)), my + FX(Rnd(-22, -6)), 12, SHOT_SMALL, TAG_NONE, 0, 0);
    AnShoot(cx, cy, mx + FX(Rnd(-14, 14)), my + FX(Rnd(6, 22)), 13, SHOT_SMALL, TAG_NONE, 0, 0);
    Boom("ExplosionXXS", cx + SB_Cos(Q.ciws_ang[k]) * 10, cy + SB_Sin(Q.ciws_ang[k]) * 10);
    Q.ciws_cool[k] = CIWS_EVERY;
    Q.ciws_flash[k] = 6;
  }
}

// As the barrage begins, Argus sends his escorts back in: up from the
// bottom of the screen to either side of the pilot, every gun on the
// missiles -- slowly at first, faster and faster.
static void EscortsIn(void) {
  R.esc_on = 1;
  R.esc_t = 0;
  R.esc_every = ESC_EVERY0;
  for (int i = 0; i < 2; i++) {
    AnShip* s = &Q.esc[i];
    if (!s->hull) {
      // made at the screen's edge (far off it they are culled), then moved below it
      ShipSpawn(s, SHIP_FRIGATE, FX(i ? ESC_X1 : ESC_X0), FX(400), UP);
      s->y = FX(470 + 30 * i);
    }
    s->head = UP;
  }
}

static void Escorts(void) {
  if (!R.esc_on) return;
  // the ramp runs only while the barrage is on (not through the talk between)
  const int barrage = (R.phase == F_P2 || R.phase == F_P3) && !InCutscene();
  if (barrage && (++R.esc_t & 63) == 0 && R.esc_every > ESC_EVERY1) R.esc_every -= 2;
  for (int i = 0; i < 2; i++) {
    AnShip* s = &Q.esc[i];
    if (!s->hull) continue;
    // onto station, easing in, heading up
    s->head = AnTurnTo(s->head, UP, TURN / 200);
    s->x += Ease(FX(i ? ESC_X1 : ESC_X0) - s->x, FX(1.2));
    s->y += Ease(FX(ESC_STATION_Y) - s->y, FX(1.6));
    // the nearest missile in reach that nobody is shooting at yet
    s32 tx, ty;
    ShipTurret(s, 0, &tx, &ty);
    u32 best = 0;
    s32 bd = ESC_RANGE, mx = 0, my = 0;
    for (u32 b = SB_GetFirstBadGuy(BH); b; b = SB_GetNextBadGuy(BH, b)) {
      if (SB_GetBadGuyHealth(b) <= 0 || (GetReg(b, 21) >> 16) != MISSILE_MARK || GetReg(b, REG_MARK) == FX(3)) continue;
      s32 x, y;
      SB_GetActorPosition(Act(b), &x, &y);
      if (y < FX(-10) || y > FX(400)) continue;
      const s32 d = AnDist(x - tx, y - ty);
      if (d < bd) { bd = d; best = b; mx = x; my = y; }
    }
    if (!best) { s->tur_ang[0] = AnTurnTo(s->tur_ang[0], UP, TURN / 120); continue; }
    const s32 want = AnAngle(tx, ty, mx, my);
    s->tur_ang[0] = AnTurnTo(s->tur_ang[0], want, TURN / 24);
    if (s->tur_cool[0] > 0 || AnOff(s->tur_ang[0], want) > TURN / 24) continue;
    if (!AnShoot(tx, ty, mx, my, 12, SHOT_MEDIUM, TAG_MISSILE, best, 200)) continue;
    SetReg(best, REG_MARK, FX(3));          // engaged
    AnShoot(tx, ty, mx + FX(Rnd(-14, 14)), my + FX(Rnd(-20, -6)), 12, SHOT_SMALL, TAG_NONE, 0, 0);
    AnShoot(tx, ty, mx + FX(Rnd(-14, 14)), my + FX(Rnd(6, 20)), 13, SHOT_SMALL, TAG_NONE, 0, 0);
    s->tur_cool[0] = R.esc_every;
    s->tur_flash[0] = 5;
  }
}

static void Clamp(void) {
  const s32 limit = R.sy + FX(IKA_HALF_BEAM + CLAMP_GAP);
  if (PlayerY() < limit) R32(Shogun(), SH_PLAYER_Y) = limit;
}

// The ship steaming slowly back and forth on her station.
static void Steam(void) {
  R.drift_t++;
  const s32 want = R.cx_want + SB_MulFP(SB_Sin((s32)(R.drift_t << 14)), FX(26));
  R.sx += Ease(want - R.sx, FX(0.4));
  R.sy += Ease(FX(SHIP_Y) + SB_MulFP(SB_Sin((s32)(R.drift_t << 15)), FX(3)) - R.sy, FX(0.3));
}

// A section torn away: it drifts off, listing, burning, and goes down.
static void TearAway(int i, s32 vx) {
  s32 x, y, rot;
  SecTransform(i, &x, &y, &rot);
  R.loose[i] = 1;
  R.sec_x[i] = x;
  R.sec_y[i] = y;
  R.sec_rot[i] = rot;
  R.sec_vx[i] = vx;
  R.sec_vy[i] = FX(0.15);
  R.sec_t[i] = 0;
  R.sec_f[i] = 2;
  Blast("ExplosionXL", (x + R.sx) >> 1, y);
  SB_ShakeWorld();
  SB_ShakeWorld();
}

static void Loose(void) {
  for (int i = 0; i < 3; i++) {
    if (!R.loose[i] || !R.sec[i]) continue;
    R.sec_t[i]++;
    R.sec_x[i] += R.sec_vx[i];
    R.sec_y[i] += R.sec_vy[i];
    R.sec_vy[i] += FX(0.004);
    R.sec_rot[i] = (R.sec_rot[i] + (R.sec_vx[i] < 0 ? -(TURN / 2400) : TURN / 2400)) & (TURN - 1);
    if ((R.sec_t[i] & 7) == 0)
      Blast(R.sec_t[i] & 8 ? "ExplosionL" : "ExplosionXS", R.sec_x[i] + FX(Rnd(-40, 40)), R.sec_y[i] + FX(Rnd(-26, 26)));
    if (R.sec_t[i] > 170) {
      Blast("ExplosionXL", R.sec_x[i], R.sec_y[i]);
      AnDrop(&R.sec[i]);
      for (int g = 0; g < NGUNS; g++)
        if (kGunSec[g] == i) AnDrop(&R.vis[g]);
    }
  }
}

static void StopGuns(void) {
  for (int g = 0; g < NGUNS; g++) if (Live(g)) Stop(R.gun[g]);
  if (R.core && Alive(R.core)) Stop(R.core);
}

// ---- the story ------------------------------------------------------------------------------
// The fight opens: the world eases back a little (the scroll runs down
// the screen as it grows), the fortress up the screen with its island --
// room in the middle for what comes out of it.
static int Lift(void) {
  if (!R.lift_to) R.lift_to = Q.fy - FX(FORT_LIFT);
  if (Q.fy <= R.lift_to || ++R.lift_t > 300) return 1;
  AnScrollBy(-LIFT_STEP);
  Q.fy = AnRowY(ROW_FORT);
  return 0;
}

// The Ikazuchi gone, the world eases on again: the fortress comes down the
// screen out from under the dialog box (the closing talk runs through the
// whole bombardment), so its burning is seen.
static void Lower(void) {
  if (Q.fy >= FX(FORT_SHOW_Y)) return;
  AnScrollBy(LIFT_STEP * 2);
  Q.fy = AnRowY(ROW_FORT);
}

static void Fallback(void) {
  // the Bastion to the bottom of the screen; her escorts out to the flanks
  Q.by += Ease(FX(BASTION_Y) - Q.by, FX(0.9));
  for (int i = 0; i < 2; i++) {
    AnShip* s = &Q.esc[i];
    if (!s->hull) continue;
    s->head = AnTurnTo(s->head, i ? 0x200000 : 0x600000, TURN / 300);
    if (s->speed < FX(1.8)) s->speed += FX(0.02);
    ShipSail(s);
    if (s->x < FX(-120) || s->x > FX(520) || s->y > FX(560)) ShipDrop(s);
  }
  // the sea gate opens
  if (R.t > 60) Q.f_gate = R.t < 75 ? 1 : (R.t < 90 ? 2 : 3);
}

static void Emerge(void) {
  // out of the harbour bow first, turning broadside as she clears it
  const s32 dy = FX(SHIP_Y) - R.sy;
  R.sy += dy > FX(0.9) ? FX(0.9) : dy;
  if (R.sy > FX(SHIP_Y - 78)) R.head = AnTurnTo(R.head, 0, TURN / 340);
  PlaceShip();
  if (R.t > 140 && R.head == 0 && dy < FX(1)) {
    // she is on station: the gate shuts behind her, and her guns open up
    Q.f_gate = 0;
    Arm(G_L0);
    Arm(G_L1);
    Arm(G_VLS);
    {
      s32 x, y;
      OnShip(SEC_MID, IKA_MAIN_X, IKA_MAIN_Y, &x, &y);
      R.core = SB_SpawnBadGuy(BH, "*andaman/boss/ika_core", x, y);
      R.core_cool = 200;
    }
    for (int i = 0; i < 2; i++) {
      R.fgun[i] = SB_SpawnBadGuy(BH, "*andaman/boss/fort_gun", FCX + FX(i ? FORT_GUN1_X : FORT_GUN0_X),
                                 Q.fy + FX(i ? FORT_GUN1_Y : FORT_GUN0_Y));
      R.fgun_hp[i] = Hp(R.fgun[i]);
      R.fgun_cool[i] = 140 + 70 * i;
    }
    g_autopilot_target = R.gun[G_L0];
    R.by_want = FX(BASTION_Y);
    StartPhase(F_P1);
  }
}

static int PhaseClear(int a, int b, int c) { return !Live(a) && !Live(b) && !Live(c); }

static void Target(int a, int b, int c) {
  if (g_autopilot_target && Alive(g_autopilot_target)) return;
  g_autopilot_target = Live(a) ? R.gun[a] : (Live(b) ? R.gun[b] : (Live(c) ? R.gun[c] : 0));
}

// The finale: the Bastion's main battery on the Ikazuchi, then the fortress.
static void Salvo(s32 tx0, s32 ty0, int spread_x, int spread_y, int tag) {
  for (int k = 0; k < 2; k++) {
    if (R.salvo <= 0) return;
    if (!Q.aimed[k]) {
      Q.tx[k] = tx0 + FX(Rnd(-spread_x, spread_x));
      Q.ty[k] = ty0 + FX(Rnd(-spread_y, spread_y));
      Q.aimed[k] = 1;
    }
    if (BastionFire(k, Q.tx[k], Q.ty[k], 22, SHOT_HUGE, tag, 0, 0)) {
      Q.aimed[k] = 0;
      R.salvo--;
    }
  }
}

static void RfUpdate(u32 boss) {
  SpeechPump();
  R.t++;
  const int scene = InCutscene();
  const int fighting = R.phase == F_P1 || R.phase == F_P2 || R.phase == F_P3;
  ShipShield(scene || (!fighting && R.phase < F_FINALE));
  if (g_autopilot_target && !Alive(g_autopilot_target)) g_autopilot_target = 0;
  if (fighting) BossWards();
  switch (R.phase) {
  case F_INTRO:
    // the shells keep landing; with no generators behind it, the dome fails
    ShellDome();
    if (R.t > 120) Q.f_dome = 1;
    if (R.t > 200 && AnQuiet()) {
      R.phase = F_COLLAPSE;
      R.t = 0;
    }
    break;
  case F_COLLAPSE:
    if ((R.t & 3) == 0) {
      s32 x, y;
      DomeRim(&x, &y);
      Blast(R.t & 4 ? "ExplosionL" : "ExplosionXS", x, y - FX(Rnd(0, 120)));
    }
    if (R.t == 30) Q.f_core = 1;
    if (R.t == 60) {
      SB_ShakeWorld();
      SB_ShakeWorld();
      AnDrop(&Q.dome);
      AnDrop(&Q.core);
      Blast("ExplosionXL", FCX + FX(FORT_CORE_X), Q.fy + FX(FORT_CORE_Y));
      AnLine(L_FORT_DOWN);          // Argus: "And there it goes. Fire-support range."
      R.phase = F_FALLBACK;
      R.t = 0;
    }
    break;
  case F_FALLBACK:
    Fallback();
    if (Lift() && R.t > 110 && Q.by > FX(BASTION_Y - 3) && AnQuiet()) {
      ShipUp();
      AnLine(L_VORN_OUT);           // Vorn: "Enough! ... the Ikazuchi!"
      AnLine(L_VORN_DOC);           // the Doc: knock out its weapons
      R.phase = F_EMERGE;
      R.t = 0;
    }
    break;
  case F_EMERGE:
    Fallback();
    Emerge();
    break;
  case F_P1:
    Steam();
    Guns(scene);
    if (!scene && HardMode()) {
      FireList(G_L0, kL0H, sizeof kL0H, 150);
      FireList(G_L1, kL1H, sizeof kL1H, 150);
      FireList(G_VLS, kVlsH, sizeof kVlsH, 300);
    } else if (!scene) {
      Fire(G_L0, A_SALVO, 150);
      Fire(G_L1, A_SALVO, 150);
      if (Fire(G_VLS, R.vls_turn ? A_CURL : A_POP, 300)) R.vls_turn ^= 1;
    }
    Core(scene);
    FortGuns(scene);
    Target(G_L0, G_L1, G_VLS);
    // they are all on the pilot: Argus turns every gun on their launchers
    if (!R.focus_line && R.t > FOCUS_AT) R.focus_line = AnLine(L_ARGUS_FOCUS);
    if (PhaseClear(G_L0, G_L1, G_VLS)) {
      StopGuns();
      TearAway(SEC_BOW, -FX(0.45));
      R.sec_f[SEC_MID] = 1;
      R.cx_want = CX - FX(IKA_SEC >> 1);          // what is left of her, centred
      R32(boss, B_MAXHP) = 0;
      AnLine(L_BOW_GONE);           // Vorn: "My bow! Thousand Needles, fire!"
      AnLine(L_BARRAGE_WARN);       // the Doc: the plasma sphere, now!
      AnLine(L_ARGUS_PULLBACK);     // Argus: thin them out -- back off a little -- escorts, on the pilot
      EscortsIn();
      R.by_want = FX(BASTION_Y + 20);
      R.phase = F_BREAK1;
      R.t = 0;
    }
    break;
  case F_BREAK1:
    Steam();
    Guns(1);
    FortGuns(1);
    if (R.t > 90 && AnQuiet()) {
      Arm(G_L2);
      Arm(G_L3);
      Arm(G_BARRAGE);
      R.cool[G_BARRAGE] = 20;       // the first barrage at once: the warning was for it
      R.core_cool = 300;
      g_autopilot_target = R.gun[G_BARRAGE];
      StartPhase(F_P2);
    }
    break;
  case F_P2:
    Steam();
    Guns(scene);
    if (!scene && HardMode()) {
      FireList(G_L2, kL2H, sizeof kL2H, 120);
      FireList(G_L3, kL3H, sizeof kL3H, 120);
      Fire(G_BARRAGE, A_NEEDLES, 440);
    } else if (!scene) {
      if (Fire(G_L2, R.l2_turn ? A_HOOK : A_SALVO, 120)) R.l2_turn ^= 1;
      Fire(G_L3, A_SALVO, 120);
      Fire(G_BARRAGE, A_NEEDLES, 440);
    }
    Core(scene);
    FortGuns(scene);
    Target(G_BARRAGE, G_L2, G_L3);
    if (PhaseClear(G_L2, G_L3, G_BARRAGE)) {
      StopGuns();
      TearAway(SEC_STERN, FX(0.45));
      R.sec_f[SEC_MID] = 2;
      R.cx_want = CX;
      R32(boss, B_MAXHP) = 0;
      AnLine(L_STERN_GONE);         // Vorn: "she still has her thunder!"
      AnLine(L_P3_DOC);             // the Doc: a cannon!
      AnLine(L_ARGUS_RETURN);       // Argus: back into range
      R.by_want = FX(BASTION_Y);
      R.phase = F_BREAK2;
      R.t = 0;
    }
    break;
  case F_BREAK2:
    Steam();
    Guns(1);
    FortGuns(1);
    if (R.t == 40) {
      // the citadel opens: its guns rise from their hatches, the Thunder
      // cannon out of the keep's roof
      for (int g = G_GUN0; g <= G_MAIN; g++) {
        s32 x, y;
        GunAt(g, &x, &y);
        R.vis[g] = AnSpawn(kVis[g], x, y);
        R.aim[g] = DOWN;
        R.rise[g] = 50;
      }
      SB_ShakeWorld();
    }
    if (R.t > 100 && AnQuiet()) {
      for (int g = G_GUN0; g <= G_MAIN; g++) Arm(g);
      R.core_cool = 240;
      g_autopilot_target = R.gun[G_MAIN];
      StartPhase(F_P3);
    }
    break;
  case F_P3:
    Steam();
    Guns(scene);
    if (!scene && HardMode()) {
      FireList(G_GUN0, kG0H, sizeof kG0H, 96);
      FireList(G_GUN1, kG1H, sizeof kG1H, 96);
      FireList(G_MAIN, kMainH, sizeof kMainH, 150);
    } else if (!scene) {
      // the twin guns: streams, and now and then the shears; the cannon
      // goes thunder, wall, wheel
      // (main_turn runs 0..3 round: the guest code has no division)
      Fire(G_GUN0, R.main_turn == 1 ? A_SCISSOR : A_GUNS, 96);
      Fire(G_GUN1, R.main_turn == 3 ? A_SPOKES : A_GUNS, 96);
      static const unsigned char kMain[4] = { A_THUNDER, A_ROSACE, A_WALL, A_WHEEL };
      if (Fire(G_MAIN, kMain[R.main_turn], 150) && ++R.main_turn >= 4) R.main_turn = 0;
    }
    Core(scene);
    FortGuns(scene);
    Target(G_MAIN, G_GUN0, G_GUN1);
    if (PhaseClear(G_GUN0, G_GUN1, G_MAIN)) {
      StopGuns();
      R32(boss, B_MAXHP) = 0;
      g_autopilot_target = 0;
      AnLine(L_FINAL_ARGUS);        // Argus: "Main battery... FIRE!"
      R.phase = F_FINALE;
      R.t = 0;
      R.salvo = 8;
      Q.aimed[0] = Q.aimed[1] = 0;
    }
    break;
  case F_FINALE:
    // the Bastion's broadside into her
    Steam();
    Guns(1);
    FortGuns(1);
    if (R.t > 90 && (AnQuiet() || R.t > 400)) Salvo(R.sx, R.sy, 44, 18, TAG_IKA);
    R.landed += AnLanded(TAG_IKA);
    if ((R.landed & 1) && (R.t & 3) == 0)
      Blast("ExplosionL", R.sx + FX(Rnd(-50, 50)), R.sy + FX(Rnd(-24, 24)));
    if (R.landed >= 6) {
      R.sec_f[SEC_MID] = 3;
      for (int g = 0; g < NGUNS; g++) if (R.vis[g]) { R.dead[g] = 1; }
      if (R.core && Alive(R.core)) Remove(R.core);
      R.core = 0;
      AnLine(L_VORN_LOSE);          // Vorn: "Not again! Shogun, forgive me..."
      R.phase = F_SINK;
      R.t = 0;
    }
    break;
  case F_SINK:
    // she lists and goes down, burning
    Guns(1);
    FortGuns(1);
    R.sy += FX(0.18);
    R.head = (R.head + TURN / 3600) & (TURN - 1);
    if ((R.t & 5) == 0) Blast(R.t & 8 ? "ExplosionL" : "ExplosionXS", R.sx + FX(Rnd(-56, 56)), R.sy + FX(Rnd(-30, 30)));
    if ((R.t & 31) == 0) SB_ShakeWorld();
    if (R.t == 200) {
      Blast("ExplosionXL", R.sx, R.sy);
      Blast("ExplosionXL", R.sx - FX(30), R.sy + FX(10));
      Blast("ExplosionXL", R.sx + FX(30), R.sy - FX(8));
      for (int i = 0; i < 3; i++) AnDrop(&R.sec[i]);
      for (int g = 0; g < NGUNS; g++) AnDrop(&R.vis[g]);
      AnLine(L_FORT_FIRE);          // Argus: "Now the fortress."
      R.phase = F_FORT;
      R.t = 0;
      R.salvo = 44;                 // a sustained bombardment, as long as the fortress burns
      R.landed = 0;
      Q.aimed[0] = Q.aimed[1] = 0;
    }
    break;
  case F_FORT:
    // and the Iron Reef
    Lower();
    if (R.t > 60) Salvo(FCX, Q.fy, 120, 50, TAG_FORT);
    if (R.salvo > 0) R.fire_end = R.t;      // (the explosions run on till the shelling stops)
    R.landed += AnLanded(TAG_FORT);
    if (R.landed >= 4 && Q.f_fort < 2) {
      Q.f_fort = 2;
      for (int i = 0; i < 2; i++) {
        if (R.fgun[i] && Alive(R.fgun[i])) Remove(R.fgun[i]);
        R.fgun[i] = 0;
        Q.f_gun[i] = 3;
      }
      Q.f_gate = 0;
      AnLine(L_FORT_GONE);          // the Doc: the Iron Reef is gone!
      AnLine(L_SHOGUN_END);         // the Shogun
      AnLine(L_DOC_END);            // the Doc: come home
      AnLine(L_ARGUS_END);          // Argus: told you I'd take care of the rest
    }
    // the talk done, Volta comes for the pilot: the Valhalla over the ruins,
    // a parting shot from her railgun
    if (Q.f_fort == 2 && !R.flyby_at && R.t > 200 && R.salvo <= 0 && AnQuiet()) {
      AnLine(L_VOLTA_END);          // Volta: "Valhalla to pilot: form up on me."
      AnFlybyAt(CX, Q.fy);
      R.flyby_at = R.t;
    }
    if (Q.f_fort == 2 && R.t < R.fire_end + 60 && (R.t & 3) == 0)
      Blast((R.t & 12) == 0 ? "ExplosionXL" : "ExplosionL", FCX + FX(Rnd(-150, 150)), Q.fy + FX(Rnd(-70, 70)));
    if (Q.f_fort == 2 && R.t < R.fire_end + 60 && (R.t & 31) == 0) SB_ShakeWorld();
    if (Q.f_fort == 2 && R.flyby_at && R.t > R.flyby_at + 560 && AnQuiet()) {
      R.phase = F_END;
      R.t = 0;
    }
    break;
  case F_END:
    if (R.t > 60) {
      R32(boss, B_STATE) = 0;     // the engine calls death next: the level runs to its end
      return;
    }
    break;
  }
  Loose();
  if (R.phase >= F_EMERGE && R.phase <= F_FINALE) PlaceShip();
  if (fighting) {
    Support(scene);
    Clamp();
  }
  if (R.phase >= F_BREAK1 && R.phase <= F_FINALE) Escorts();
  for (int i = 0; i < 2; i++) if (Q.esc[i].hull) ShipPlace(&Q.esc[i]);
  if (R.phase >= F_P1) Q.by += Ease(R.by_want - Q.by, FX(0.35));     // Argus keeps his station
  FortPlace();
  BastionPlace();
}

static void RfDeath(u32 boss) {
  (void)boss;
  ShipShield(0);
  g_autopilot_target = 0;
  SB_PausePartition(BH, 0);
}

void ReefBossInit(u32 boss) {
  SB_ClearBytes((void*)boss, BOSS_SIZE);
  SB_ClearBytes(&R, sizeof R);
  R32(boss, B_SPAWN + 0x00) = (s32)(u32)RfSpawn;
  R32(boss, B_SPAWN + 0x04) = (s32)(u32)RfUpdate;
  R32(boss, B_SPAWN + 0x08) = (s32)(u32)RfRender;
  R32(boss, B_SPAWN + 0x0c) = (s32)(u32)RfRender;
  R32(boss, B_SPAWN + 0x10) = (s32)(u32)RfDeath;
  R32(boss, B_HEALTH) = (s32)(u32)RfHealth;
}
