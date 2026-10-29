// lore.c -- the story so far, on the black screen a mission loads behind.
//
// Every way into a mission -- the level select's START, Next Mission on the
// results screen, the debriefing's Next Mission -- fades to black through
// SetStateWithCrossfade(sh, 3, ...), and the engine writes "Loading..." in the
// middle (RenderCrossfade). For the missions with a recap
// (docs/LOADING_RECAPS.md) that screen carries the recap instead, and the
// mission waits on it:
//
//   fade to black   the crossfade (state 15) towards state 3, the recap fading
//                   in with the black;
//   state 3         onUpdate loads the world in one go (LoadWorld) and sets
//                   state 4; the recap stays up, "Loading..." under it;
//   state 4         would build the world (InitWorld) and start the mission.
//                   It is held: onUpdate and onRender read the state as 0xff
//                   (no case: onRender clears the screen and draws only the
//                   crossfade), the fade stays black, and "Tap to continue"
//                   pulses under the recap;
//   the tap         (on the finger's lift, so the weapon wheel's lift does not
//                   come after it) lets state 4 run and the fade lift as ever.
//
// The fade's own fields (InitCrossfade, UpdateCrossfade): SHOGUN+0x8d1c0
// alpha (16.16), +0x8d1c4 its step a frame, +0x8d1cc the state it fades to,
// +0x8d1d0 its text.

typedef unsigned int u32;
typedef int s32;

#define IMG 0x10000000u
#define FN(type, addr) ((type)(IMG + (addr)))
#define R32(p, off)  (*(volatile s32*)((u32)(p) + (off)))

#define LO_GetState         FN(s32 (*)(u32), 0x76f0c)                        // ARM
#define LO_UpdateCrossfade  FN(void (*)(u32), 0x29165)
#define LO_RenderCrossfade  FN(void (*)(u32), 0x357b1)
#define LO_GameData         FN(u32 (*)(void), 0x63128)                       // ARM
#define LO_PushAlpha        FN(void (*)(s32), 0x4ecc5)
#define LO_PopAlpha         FN(void (*)(void), 0x4ed35)
#define LO_ScreenWidth      FN(s32 (*)(void), 0x4e955)
#define LO_ScreenHeight     FN(s32 (*)(void), 0x4e971)
#define LO_SetFont          FN(void (*)(u32), 0x51e45)
#define LO_GetTextWidth     FN(s32 (*)(const char*), 0x52099)
#define LO_GetTextHeight    FN(s32 (*)(void), 0x521c9)
#define LO_DrawText         FN(void (*)(const char*, s32, s32, u32, u32), 0x5edd1)
// (text, x, y, width, line gap, font, letters): the text wrapped at spaces
// into a column, left-aligned; answers the y under its last line
#define LO_DrawColumn       FN(s32 (*)(const char*, s32, s32, s32, s32, u32, u32), 0x411d9)
#define LO_SetColor         FN(void (*)(u32), 0x4eb79)
#define LO_GetMouseState    FN(s32 (*)(u32), 0x5257d)
#define LO_MulFP            FN(s32 (*)(s32, s32), 0x63148)                   // ARM

#define SH_WORLD      0x4e6c
#define SH_FONT_BIG   0x14        // LoadFonts: the large font's handle
#define SH_FONT       0x18        // ... and the one RenderCrossfade writes with
#define CF_ALPHA      0x8d1c0
#define CF_TARGET     0x8d1cc
#define CF_TEXT       0x8d1d0
#define ST_FADE       15          // the crossfade
#define ST_LOAD       3           // onUpdate: load the world
#define ST_INIT       4           // onUpdate: build it and play
#define ST_NONE       0xff        // read while held: no case in onUpdate or onRender
#define FX_ONE        0x10000
#define MIN_HOLD      30          // frames on the recap before a tap counts

volatile s32 g_lore = -1;         // -1 shown; the desktop harness sets 0 to skip it (levelkit.py)

typedef struct {
  u32 world;
  const char* place;
  const char* text[4];            // paragraphs
  const char* mission;
} Recap;

static const Recap kRecaps[] = {
  {1, "Greenland",
   {"October 2140. World War IV is over, and the peace lasts only a moment.",
    "A robot army has swept out of nowhere and crushed what was left of the old armies. "
    "Intel names the man behind it: the renegade General Leo Ming, who now calls himself the Shogun.",
    "The resistance, UNITY, has built one prototype fighter. Its Entropy Engine drinks the energy "
    "of enemy fire and turns it into power. You are its pilot."},
   "Your mission: Find the Shogun's headquarters. The trail begins at sea, off Greenland."},
  {2, "Peru",
   {"The Shogun's admiral threw his fleet at you off Greenland, and it went down.",
    "But the robot army is still out there, and so are its commanders. The Doc, your engineer at "
    "base, has traced the next one to a volcano in Peru."},
   "Your mission: Break through the volcano's defences."},
  {3, "Kazakhstan",
   {"Peru is behind you. A hot-headed commander fell there, and another will take his place.",
    "The Shogun's generals keep boasting that they will end you. The Doc's next lead is a walled "
    "desert city in Kazakhstan, where a war machine has taken over."},
   "Your mission: Silence it."},
  {4, "Japan",
   {"The war machine in the Kazakh desert is scrap. Every general you beat only shows how large "
    "the Shogun's army is.",
    "In Japan waits a storm commander who mocks you for hunting his master."},
   "Your mission: Stop the storm."},
  {5, "Bali",
   {"The Shogun's most trusted generals are gone. From his bunker he shakes a fist at the sky, but "
    "his robot army still stands, and UNITY keeps a secret weapon ready for the day he shows his face.",
    "A new lead from Indonesia: a scientist in the Shogun's service is using Bali's coast as a "
    "testing ground."},
   "Your mission: Find out what she is testing."},
  {6, "Seoul",
   {"In Bali you met Dr. Marrow, a cheerful scientist in the Shogun's service who breeds living "
    "war machines.",
    "She tested them against your Entropy Engine, was delighted rather than beaten, and promised "
    "you a gift. She says her next creation is already growing.",
    "Now word comes from Korea: the Shogun's children, Prince Kaito and Princess Akane, have taken "
    "Seoul's palaces for their father's court."},
   "Your mission: Free Seoul."},
  {7, "Siberia",
   {"Seoul is free. The Shogun's heirs lost their giant machines and went into hiding, and Akane "
    "warned that her father would come for you himself.",
    "The Doc calls you home, with an upgrade waiting. UNITY's headquarters lies hidden under the "
    "Antarctic ice, and no enemy must ever follow you there."},
   "Your mission: Fly home."},
  {8, "Edinburgh",
   {"The Shogun came for you himself, aboard the Kurogane, a dreadnought no gun could scratch. To "
    "keep him from headquarters, you led him north across Siberia.",
    "Commander Volta's railgun ship, the Valkyrie, shot out his engines one by one, and was shot "
    "down for it. She lived. The Kurogane limped home.",
    "At UNITY's hidden base in the Scottish Highlands the Doc has new weapons waiting, and a lead: "
    "something in Edinburgh talks to the Shogun every day."},
   "Your mission: Find it."},
  {9, "The Andaman Islands",
   {"In Scotland the Doc fitted you a shield and a plasma sphere, and admitted a secret: the "
    "Shogun's scientist, Dr. Marrow, is his daughter.",
    "You captured the data of ShogunGPT, the AI that ran the Shogun's life. The jammed Kurogane "
    "fled with a promise: the third time, he would be ready.",
    "The data points to the Iron Reef, a fortress in the Andaman Islands that feeds his supply "
    "lines across the Indian Ocean. Volta is back with a flying battleship, the Valhalla, and "
    "Captain Argus's Bastion will shell the Reef."},
   "Your mission: Silence its guns."},
  {10, "The Indian Ocean",
   {"The Iron Reef is rubble, and the Shogun's network across the Indian Ocean has gone dark. He "
    "swears to take back every island.",
    "Now you fly escort for Captain Argus's Bastion and her frigates, bound for home. Argus carries "
    "something priceless in his mind drive: the secret codes to UNITY's headquarters.",
    "The sea is calm. Somewhere beneath it, a scientist's promised gift is growing."},
   "Your mission: Bring the fleet home."},
  {11, "The Mariana Trench",
   {"The gift was a living submarine. Dr. Marrow ambushed the fleet, sank the frigates, and "
    "carried off Captain Argus, with UNITY's codes in his head. She dived, and she is gone.",
    "Volta has lent you the Valhalla's prototype submarine. It has no Entropy Engine for Marrow to "
    "turn against you, and a new trick, the Spin Drive.",
    "Argus carries a faint tracker. To find it you must raid the Forge, the Shogun's relay station "
    "on the Mariana Trench."},
   "Your mission: Dive."},
  {12, "Dr. Marrow's Laboratory",
   {"The Forge is broken, and its relays gave up Argus's signal. It leads to Dr. Marrow's secret "
    "laboratory under the Philippine Sea.",
    "She is draining his mind drive for the Shogun, a little faster every minute. If she gets the "
    "codes, UNITY's headquarters is lost."},
   "Your mission: Raid the lab before she finishes."},
  {13, "The Coral Sea",
   {"You broke Marrow's Kraken and her laboratory. She fled in a rocket pod with Argus aboard. "
    "HQ ordered it shot down. The Doc asked for grapples, and Volta held her fire.",
    "Marrow is in the Valhalla's brig, and the Doc, her father, asked that she be treated gently. "
    "Argus's codes held. But she sent the Shogun his logs: patrol routes, supply runs, flight times.",
    "Pieced together, they may be enough to find UNITY's headquarters."},
   "Your mission: Race for the Antarctic before he works it out."},
  {14, "Antarctica",
   {"The Valhalla ran for home with Marrow in her brig. Captain Ignis, the volcano's commander, "
    "rebuilt, hunted you across the sea, and the Valhalla's railgun finished him.",
    "HQ's escorts met you at the ice, and with them the Bastion, repaired. Argus is back aboard his "
    "own ship.",
    "Now the Shogun is massing on UNITY's French base. And the Doc says it is time for something "
    "he has waited twenty years to do."},
   "Your mission: Come home."},
  {15, "France",
   {"Amaterasu: UNITY's secret weapon, hidden at headquarters for twenty years. Three mirrors in "
    "orbit focus sunlight into one beam, and your Entropy Engine paints its target.",
    "Two mirrors are up. Off the African coast you painted the Shogun's storm commander, Lord "
    "Raijin, and the beam burned him from the sky. His logs showed every gun and shield around "
    "the French base, mapped.",
    "The third mirror still waits on its pad at Base Lutece, outside Paris. The Doc reads the "
    "Shogun's massing ships as a raid on the base's stores."},
   "Your mission: Hold the base."},
  {16, "The Atlantic",
   {"Base Lutece held until the launch. The third mirror is in orbit, Amaterasu is whole, and "
    "its beam now strikes twice as wide. The Shogun's assault is broken.",
    "General Arnaud's people sail from Le Havre for headquarters, under the guns of the Valhalla "
    "and the Bastion. Everyone is cheering. For once, the Doc calls it an easy trip home."},
   "Your mission: Escort the convoy."},
  {17, "Antarctica",
   {"It was a trap. The Shogun never went to France. Akane delayed the convoy, and he waited on "
    "its road home: the Kurogane rebuilt against railguns, a jamming field that blinded Amaterasu.",
    "Volta turned the Valhalla to hold him while the convoy escaped. Her last shot crippled his "
    "Tenshu cannon, then his broadside broke her ship. Argus's Bastion pulled her from the wreck, "
    "alive, barely, and drove the Kurogane off.",
    "The Doc has broken the jam. But his signal pointed straight at UNITY's headquarters, and the "
    "Shogun is coming with everything he has."},
   "Your mission: Hold headquarters."},
};
#define NRECAPS (sizeof(kRecaps) / sizeof(kRecaps[0]))

static struct {
  const Recap* on;                // the recap up, or 0
  u32 frames;                     // since it came up
  int was_down;
  int go;                         // tapped: let the mission start
} L;

static const Recap* Find(u32 world) {
  for (u32 i = 0; i < NRECAPS; i++)
    if (kRecaps[i].world == world) return &kRecaps[i];
  return 0;
}

// The load is done and state 4 waits for the tap.
static int Holding(s32 state) { return L.on && !L.go && state == ST_INIT; }
// ... and the black stays from the load on (the frame of the load may still
// read state 3).
static int HoldFade(s32 state) { return L.on && !L.go && (state == ST_LOAD || state == ST_INIT); }

static void Track(u32 sh, s32 state) {
  if (state == ST_FADE && R32(sh, CF_TARGET) == ST_LOAD) {
    if (!L.on && g_lore != 0) {
      L.on = Find((u32)R32(sh, SH_WORLD));
      L.frames = 0;
      L.was_down = 0;
      L.go = 0;
    }
  } else if (state != ST_LOAD && state != ST_INIT) {
    L.on = 0;                     // the mission is under way (or the fade went elsewhere)
  }
}

// onUpdate's `blx UE_GetState` (r0 = 1) choosing its case.
s32 hook_update_state(u32 m) {
  const s32 state = LO_GetState(m);
  const u32 sh = LO_GameData();
  Track(sh, state);
  if (!Holding(state)) return state;
  L.frames++;
  const int down = (LO_GetMouseState(0) & 1) != 0;
  if (L.was_down && !down && L.frames > MIN_HOLD) L.go = 1;
  L.was_down = down;
  return L.go ? state : ST_NONE;
}

// onRender's `blx UE_GetState`: while held, nothing but the crossfade.
s32 hook_render_state(u32 m) {
  const s32 state = LO_GetState(m);
  return Holding(state) ? ST_NONE : state;
}

// onUpdate's `bl UpdateCrossfade`: the black stays while the recap is held.
void hook_update_crossfade(u32 sh) {
  if (HoldFade(LO_GetState(1))) return;
  LO_UpdateCrossfade(sh);
}

// The pulse of "Tap to continue": a triangle wave, 40% to full, every 64 frames.
static s32 Pulse(u32 t) {
  const s32 a = (s32)(t & 63) < 32 ? (s32)(t & 63) : 63 - (s32)(t & 63);
  return 0x6666 + a * 0x4f4;      // 0.4 + up to 0.6
}

static s32 Tenth(s32 v) { return (v * 6554) >> 16; }   // (no division in the blob)

static void Centered(const char* s, s32 y) {
  LO_DrawText(s, (LO_ScreenWidth() - LO_GetTextWidth(s)) >> 1, y, 0, 0);
}

static void DrawRecap(u32 sh, const Recap* r, s32 alpha) {
  const s32 w = LO_ScreenWidth(), h = LO_ScreenHeight();
  const s32 margin = Tenth(w);
  const u32 font = (u32)R32(sh, SH_FONT), big = (u32)R32(sh, SH_FONT_BIG);
  // the engine's own black (its "Loading..." held back for the one call)
  volatile char* text = (volatile char*)(sh + CF_TEXT);
  const char first = text[0];
  text[0] = 0;
  LO_SetColor(0xffff);            // a world's load leaves GL's colour behind the engine's
  LO_RenderCrossfade(sh);         // ... and UE_SetColor(0) skips a colour it thinks is set
  text[0] = first;
  LO_PushAlpha(alpha);

  s32 y = Tenth(h);
  LO_SetFont(font);
  static char head[16] = "MISSION ";
  {
    u32 n = r->world, i = 8;
    if (n >= 10) { head[i++] = '1'; n -= 10; }     // (missions 1 to 19)
    head[i++] = (char)('0' + n);
    head[i] = 0;
  }
  Centered(head, y);
  y += LO_GetTextHeight() + 4;
  LO_SetFont(big);
  Centered(r->place, y);
  y += LO_GetTextHeight() * 2;

  LO_SetFont(font);
  const s32 gap = LO_GetTextHeight() >> 2;
  for (u32 i = 0; i < 4 && r->text[i]; i++)
    y = LO_DrawColumn(r->text[i], margin, y, w - 2 * margin, gap, font, 0xffff) + gap * 3;
  y += gap * 2;
  LO_DrawColumn(r->mission, margin, y, w - 2 * margin, gap, font, 0xffff);
  LO_PopAlpha();

  // under it, "Loading..." until the world is loaded, then the pulse
  LO_SetFont(font);
  const s32 by = h - Tenth(h);
  LO_PushAlpha(Holding(LO_GetState(1)) ? LO_MulFP(alpha, Pulse(L.frames)) : alpha);
  Centered(Holding(LO_GetState(1)) ? "Tap to continue" : "Loading...", by);
  LO_PopAlpha();
}

// onRender's `bl RenderCrossfade` (when the state is past 15, and over every
// screen): the recap in place of the engine's "Loading...".
void hook_render_crossfade(u32 sh) {
  const s32 alpha = R32(sh, CF_ALPHA);
  if (!L.on || alpha == 0) {
    LO_RenderCrossfade(sh);
    return;
  }
  DrawRecap(sh, L.on, alpha);
}
