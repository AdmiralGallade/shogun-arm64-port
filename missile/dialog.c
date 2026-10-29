// dialog.c -- the dialog box, tapped through like a light novel, in every
// mission (the game's four, worlds 5-12, the tutorial: they all talk through
// the engine's one dialog box).
//
// The engine types a sentence out at half a letter a frame, then holds it for
// its pause (bossspeech.cfg's sentenceNpause, often 200 frames) before doing
// its action -- slow. Now a tap on the box shows the whole sentence at once,
// and a tap on a whole sentence ends its pause: what the sentence would have
// done when the pause ran out happens now ("next": the next sentence;
// "close": the box shuts; "wait": a tutorial step is done waiting). Untapped,
// it all plays as before.
//
// Nothing else is told. The code that waits for the talk to end
// (starfish.c's queue and InCutscene, the bosses' entrances, the tutorial's
// isDialogAwaitingInput) simply gets there sooner -- as it does when a line is
// short. A sentence held by code (pause 0xffff: it waits until the code moves
// it on) is shown whole but stays.
//
// Two `bl`s are rerouted here (build.py PATCHES):
//   UpdateHUD's `bl UpdateWeaponMenu` (hook_update_weapon_menu) -- the touch.
//     It runs before the dialog every frame. Lifting a finger opens the weapon
//     wheel (UpdateBulletTimeWeaponMenu: held last frame, not now), so for a
//     tap on the box that "held last frame" is cleared first and the wheel
//     stays shut. Lifting the finger that steers is how a player stops to
//     read, so the wheel is often up already: a tap on the box still counts
//     -- and closes the wheel, as a tap off its buttons always has -- unless
//     it lands on one of the wheel's buttons (they ring the ship, and reach
//     into the box when it flies high), which is the wheel's -- or on the
//     Pause button that slides in at the top right with the wheel;
//   UpdateDialog's `bl UpdateSpeech` (hook_update_speech) -- the tap, applied
//     to the sentence on screen. UpdateDialog only calls it while the box is
//     fully in.
// The touch is read once a frame, by whichever runs first: UpdateHUD is
// skipped in one of inGameUpdate's modes (SHOGUN+0x8b6e0 == 2), the dialog
// never is.

typedef unsigned int u32;
typedef int s32;
typedef unsigned short u16;
typedef unsigned char u8;

#define IMG 0x10000000u
#define FN(type, addr) ((type)(IMG + (addr)))

#define DG_UpdateSpeech      FN(int (*)(u32), 0x28665)
#define DG_UpdateWeaponMenu  FN(void (*)(u32, u32, s32, s32), 0x3abe9)
#define DG_isDialogHidden    FN(int (*)(u32), 0x28729)
#define DG_GetGameDataPtr    FN(u32 (*)(void), 0x63128)                    // ARM
#define DG_GetFrameCount     FN(u32 (*)(void), 0x72434)                    // ARM
#define DG_GetScreenHeight   FN(s32 (*)(void), 0x4e971)
#define DG_GetScreenWidth    FN(s32 (*)(void), 0x4e955)
#define DG_UIScaled          FN(s32 (*)(s32, u32), 0x2abf5)
#define DG_Clamp             FN(s32 (*)(s32, s32, s32), 0x63254)          // ARM
#define DG_TestWeaponSelection FN(s32 (*)(u32, u32, s32, s32), 0x337b1)
#define DG_isMouseOverWidget FN(s32 (*)(u32, s32, s32), 0x76aac)            // ARM
#define DG_GetHandlePtr      FN(u32 (*)(u32, u32), 0x4d6bd)
#define DG_GetMouseX         FN(s32 (*)(u32), 0x62d58)                     // ARM
#define DG_GetMouseY         FN(s32 (*)(u32), 0x62d80)                     // ARM
#define DG_GetMouseState     FN(s32 (*)(u32), 0x5257d)

#define R32(p, off)  (*(volatile s32*)((u32)(p) + (off)))
#define R16(p, off)  (*(volatile u16*)((u32)(p) + (off)))

// SHOGUN
#define SH_SPEECH     0x8b6e4     // the speech in the box
#define SH_WHEEL_OPEN 0x8b490     // the weapon wheel is up
#define SH_WHEEL_HELD 0x8b4a4     // the wheel's "a finger was down last frame"
#define SH_WEAPONMENU 0x8b46c     // WEAPONMENU
#define WM_X          0x2c        // WEAPONMENU: where the ship was when it opened (world units)
#define WM_Y          0x30
#define WHEEL_ENTRIES 5           // the game's own buttons: S H L, EMP, overload
#define W_PAUSE       0x75e       // the in-game Pause button (UpdatePauseMenu); off screen unless the wheel is up

// SPEECH (UpdateSpeech, RenderSpeech)
#define SP_SHOWN      0x04        // letters typed so far, 16.16 (the sentence is whole past its length)
#define SP_SPEED      0x08        // letters a frame, 16.16 (half a letter)
#define SP_SENTENCE   0x0c        // the sentence on screen
#define SP_HELD       0x10        // frames of its pause gone by
#define SP_COUNT      0x14        // how many sentences
#define SP_PAUSE      0x18        // u16 per sentence: its pause, frames (0xffff: until code moves it on)
#define SP_LENGTH     0x28        // u16 per sentence: its letters
#define PAUSE_HELD    0xffff

// The box slides in along the top of the screen, down to a little over a
// third of the way (edinburgh.c's skip button sits just under it, at 92/256
// of the height). Screen units.
#define BOX_BOTTOM(h) (((h) * 87) >> 8)
// A finger that wanders further than this is steering the ship, not tapping.
#define TAP_SLOP      10

static struct {
  u32 polled;                     // the frame the touch was last read in
  int down;                       // a finger was down
  int on_box;                     // ... and came down on the box, while it was up
  int moved;                      // ... and has wandered off since
  s32 x0, y0;
  int tap;                        // a tap on the box this frame, not yet used
} T;

// On the Pause button (UE_isMouseOverWidget reads the widget unchecked: only
// if it was made).
static int OnPause(void) { return DG_GetHandlePtr(W_PAUSE, 0) && DG_isMouseOverWidget(W_PAUSE, 0, 0); }

static int BoxUp(u32 sh) { return R32(sh, SH_SPEECH) != 0 && !DG_isDialogHidden(sh); }

// missile.c: the last of the wheel's own buttons, then the ones it added
// (M, E, P, the shield)
s32 hook_test_last(u32 menu, u32 entry, s32 x, s32 y);

// The touch is on one of the open wheel's buttons. Its middle as
// ChooseBulletTimeWeaponMenu works it out: the ship, less the scroll
// (UpdateWeaponMenu's x, y), kept clear of the screen's edges.
static int OnWheelButton(u32 sh, s32 scroll_x, s32 scroll_y) {
  const u32 menu = sh + SH_WEAPONMENU;
  const s32 cx = DG_Clamp(R32(menu, WM_X) - scroll_x, DG_UIScaled(100, 1),
                          DG_GetScreenWidth() - DG_UIScaled(100, 1));
  const s32 cy = DG_Clamp(R32(menu, WM_Y) - scroll_y, DG_UIScaled(150, 1),
                          DG_GetScreenHeight() - DG_UIScaled(100, 1));
  for (u32 e = 0; e < WHEEL_ENTRIES - 1; e++)
    if (DG_TestWeaponSelection(menu, e, cx, cy)) return 1;
  return hook_test_last(menu, WHEEL_ENTRIES - 1, cx, cy) != 0;
}

static s32 Dist(s32 a, s32 b) { return a > b ? a - b : b - a; }

// Read the touch, once a frame: T.tap on the frame a tap on the box ends.
// The wheel only matters where UpdateHUD reads the touch (wheel: it passed
// the scroll); without it the wheel is not being updated either.
static void Poll(u32 sh, int wheel, s32 scroll_x, s32 scroll_y) {
  const u32 now = DG_GetFrameCount();
  if (now == T.polled) return;
  T.polled = now;
  T.tap = 0;
  const int down = (DG_GetMouseState(0) & 1) != 0;
  const s32 mx = (s32)(short)DG_GetMouseX(0), my = (s32)(short)DG_GetMouseY(0);
  if (down && !T.down) {
    T.on_box = BoxUp(sh) && my < BOX_BOTTOM(DG_GetScreenHeight()) &&
               !OnPause() &&
               !(wheel && R32(sh, SH_WHEEL_OPEN) && OnWheelButton(sh, scroll_x, scroll_y));
    T.moved = 0;
    T.x0 = mx;
    T.y0 = my;
  } else if (T.on_box) {
    if (Dist(mx, T.x0) > TAP_SLOP || Dist(my, T.y0) > TAP_SLOP) T.moved = 1;
    if (!down) {
      T.tap = !T.moved && BoxUp(sh);
      T.on_box = 0;
    }
  }
  T.down = down;
}

// UpdateHUD: `bl UpdateWeaponMenu`.
void hook_update_weapon_menu(u32 sh, u32 mode, s32 x, s32 y) {
  Poll(sh, 1, x, y);
  if (T.tap) R32(sh, SH_WHEEL_HELD) = 0;      // this lifted finger opens no wheel
  DG_UpdateWeaponMenu(sh, mode, x, y);
}

// UpdateDialog: `bl UpdateSpeech`.
int hook_update_speech(u32 sp) {
  Poll(DG_GetGameDataPtr(), 0, 0, 0);
  if (T.tap) {
    T.tap = 0;
    const s32 i = R32(sp, SP_SENTENCE);
    // Each tap puts the sentence one frame short of where it was going, and
    // UpdateSpeech takes that last step itself: whoever watches the speech
    // sees every state it would have passed through, only sooner. The
    // tutorial moves on only in the one frame between a last sentence's
    // pause running out and its action (isCurrentSpeechComplete); skip that
    // frame and it waits for ever.
    if (i >= 0 && i < R32(sp, SP_COUNT)) {
      const s32 len = R16(sp, SP_LENGTH + 2 * i);
      const u32 pause = R16(sp, SP_PAUSE + 2 * i);
      if ((R32(sp, SP_SHOWN) >> 16) <= len) {
        // typed out: one more letter's step and it is whole
        const s32 last = ((len + 1) << 16) - R32(sp, SP_SPEED);
        if (R32(sp, SP_SHOWN) < last) R32(sp, SP_SHOWN) = last;
      } else if (pause != PAUSE_HELD && pause > 0 && (u32)R32(sp, SP_HELD) < pause - 1) {
        R32(sp, SP_HELD) = (s32)(pause - 1);    // its pause runs out this frame: its action next
      }
    }
  }
  return DG_UpdateSpeech(sp);
}
