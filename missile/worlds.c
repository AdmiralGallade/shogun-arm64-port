// worlds.c -- the worlds after the game's own four: a table of them.
//
//   5  the Beach, Mission 5, Indonesia, Bali   (beach.c, hermit.c, starfish.c)
//   6  Seoul, Mission 6, Korea                  (seoul.c, magpie.c, twins.c)
//   7  the chase, Mission 7, Russia, Siberia    (antarctica.c, chase.c)
//   8  Edinburgh, Mission 8, Scotland            (edinburgh.c, subagent.c,
//                                                  shogungpt.c; chase.c's doom)
//   9  the Iron Reef, Mission 9, India, Andaman  (andaman.c, battery.c,
//                                                  ironreef.c)
//  10  the ambush, Mission 10, Indian Ocean,      (sunda.c, coelacanth.c;
//      Sunda Trench                                andaman.c's machinery)
//  11  the Mariana Trench, Mission 11, Pacific   (abyss.c, abyss_boss.c)
//  12  Dr. Marrow's lab, Mission 12, Pacific     (lab.c, lab_boss.c; abyss.c's rules)
//  13  the run home, Mission 13, the Coral Sea   (homeward.c, fortress.c, inferno.c;
//      to the pack ice                             andaman.c's Valhalla and ships)
//  14  Amaterasu, Mission 14, UNITY HQ to the    (amaterasu.c, barge.c, raijin.c;
//      storm off West Africa                       andaman.c's Valhalla)
//
// The table carries each row's world number, and the level select's entries
// run in the table's order (hook_start_new_game): the tenth starts world 10,
// the eleventh world 11.
//
// The game numbers its worlds 0 (tutorial) to 4 (Storm) in SHOGUN+0x4e6c and
// the level select offers four missions; select entry n plays world n + 1.
// Each extra world needs:
//
//   * level select: the entry count raised (InitSelectLevels' `movs r0,#3`,
//     the highest entry index, is patched to 5) and the entry's three assets
//     -- title sprite, globe overlay, MPEG preview -- loaded. The engine finds
//     them by handle as base + entry, and past entry 3 those handles belong
//     to other things:
//
//          entry   title 0x690+n    globe overlay 0x6a0+n   video 0x69a+n
//            4     0x694 a layer    0x6a4 the earth mesh    0x69e free
//            5     0x695 the globe  0x6a5 the globe entity  0x69f the globe's
//                        layer                                    map texture
//            6     0x696            0x6a6                   0x6a0 entry 0's
//                                                                 globe overlay
//            7     0x697            0x6a7                   0x6a1 entry 1's
//
//     so every draw, texture swap and MPEG call that computes one of them is
//     routed through Title(), Map() or Video(), which swap in handles of ours;
//   * LoadWorld and InitWorld: each switches on the world through a jump
//     table bounded at 4. A shim in front of the bound sends 5-8 to their
//     world files and lets 0-4 through untouched;
//   * InitBoss: worlds 5-8 read as 1 (Ocean) for its switch, and the
//     hooked `bl OceanMidBossInit` installs their own mid-boss instead; each
//     mid-boss installs its world's final boss when it dies.
//
// Everything else that reads the world number either tests for 0 (tutorial)
// or 1 (Ocean's gentler satellites and HUD) and treats 5-8 like any other.

typedef unsigned int u32;
typedef int s32;

#define IMG 0x10000000u
#define FN(type, addr) ((type)(IMG + (addr)))

#define LoadWorldFile       FN(void (*)(u32, const char*), 0x47abd)
#define InitWorldFile       FN(void (*)(u32, const char*), 0x35149)
#define LoadWorld1          FN(void (*)(u32), 0x2b865)
#define LoadBossMusic       FN(void (*)(void), 0x2bbf9)
#define OceanBossInit       FN(void (*)(u32), 0x317ad)
#define UE_LoadSprites5     FN(s32 (*)(u32, const char*, u32, u32), 0x57ea9)
#define UE_LoadTexture      FN(s32 (*)(u32, const char*, u32, u32), 0x5808d)
#define UE_DrawSprite       FN(void (*)(u32, s32, s32, u32), 0x5f351)
#define UE_SetEntityTexture FN(void (*)(u32, u32), 0x53f39)
#define MPEG_Open           FN(s32 (*)(u32, const char*, u32), 0x9d990)     // ARM
#define MPEG_Start          FN(void (*)(u32), 0x9d488)                      // ARM
#define SL_GetHandlePtr     FN(u32 (*)(u32, u32), 0x4d6bd)
#define MPEG_DecodeFrame    FN(s32 (*)(u32), 0xae444)                       // ARM
#define MPEG_GetTexture     FN(u32 (*)(u32), 0x9d4cc)                       // ARM
#define UE_GetGameDataPtr5  FN(u32 (*)(void), 0x63128)                      // ARM
#define UE_SetConstraint    FN(void (*)(u32, s32), 0x72630)                 // ARM
#define OceanMidBossInit    FN(void (*)(u32), 0x2876d)

#define R32(p, off)  (*(volatile s32*)((u32)(p) + (off)))

#define SH_WORLD      0x4e6c
#define SH_PLAYER_RANK (0x5110 + 0x90)   // PLAYER+0x90: the rank, 16.16 (scripts' RANK)
#define SH_LAP        0x4e70      // StartNewGame: 1; Mission 4's hell button: 2 (onUpdate pins rank >= 20 from 2)
#define SH_UI_SCALE   0x8b9dc     // the select menu's sprite scale
#define SH_SELECTED   0x8b978     // the entry the select menu is on

#define FIRST_EXTRA   5           // world number of kWorlds[0]
#define TITLE_BASE    0x690       // + entry: the engine's handles
#define MAP_BASE      0x6a0
#define VIDEO_BASE    0x69a

void RegisterBeach(void);         // beach.c
void ForgetBeach(void);
void ForgetSeoul(void);
void ForgetAntarctica(void);
void ForgetEdinburgh(void);
void RegisterSeoul(void);         // seoul.c
void RegisterAntarctica(void);    // antarctica.c
void RegisterEdinburgh(void);     // edinburgh.c
void HermitBossInit(u32 boss);    // hermit.c: the Beach's mid-boss
void MagpieBossInit(u32 boss);    // magpie.c: Seoul's
void ChaseBossInit(u32 boss);     // chase.c: the chase is the whole of Mission 7
void SubagentBossInit(u32 boss);  // subagent.c: Edinburgh's (it also restarts the homecoming)
void RegisterAndaman(void);       // andaman.c
void ForgetAndaman(void);
void AndamanBossInit(u32 boss);   // battery.c: Mission 9's first checkpoint (and a new run)
void RegisterSunda(void);         // sunda.c
void ForgetSunda(void);
void SundaBossInit(u32 boss);     // coelacanth.c: Mission 10's Specimen Nine (and a new run)
void RegisterAbyss(void);         // abyss.c
void ForgetAbyss(void);
void HunterBossInit(u32 boss);    // abyss_boss.c: Mission 11's Kurozame (and a new run)
void RegisterLab(void);           // lab.c
void ForgetLab(void);
void LabBossInit(u32 boss);       // lab_boss.c: Mission 12's Lion's Mane (and a new run)
void RegisterHomeward(void);      // homeward.c
void ForgetHomeward(void);
void FortBossInit(u32 boss);      // fortress.c: Mission 13's turret fortress (and a new run)
void RegisterAmaterasu(void);     // amaterasu.c
void RegisterParis(void);         // paris.c
void ForgetParis(void);
void TankBossInit(u32 boss);      // tank.c: Mission 15's Kanabo (and a new run; it installs the carrier)
void RegisterAtlantic(void);      // atlantic.c
void ForgetAtlantic(void);
void AtlanticBossInit(u32 boss);  // atlantic.c: the whole of Mission 16 is one boss, as Mission 7's chase
void ForgetAmaterasu(void);
void BargeBossInit(u32 boss);     // barge.c: Mission 14's gun barge (and a new run)

typedef struct {
  u32 number;                     // SHOGUN+0x4e6c
  const char* world;              // the .world file (also its partition)
  const char* title;              // menu assets
  const char* map;
  const char* video;
  u32 h_title, h_map, h_video;    // where we load them
  s32 globe;                      // the globe's turn, whole degrees
  void (*cast)(void);             // registers the world's scripts
  void (*forget)(void);           // ... and takes its own definitions back out
  void (*mid_boss)(u32 boss);
  int ocean_sounds;               // LoadWorld1: Ocean's four boss sounds
  const char* hard;               // the hard mode's world file (its own partition), or 0 (docs/HARD_MODE.md)
} World;

// Hard mode (docs/HARD_MODE.md, the block at the end of this file)
static int HardWanted(void);      // the Cheats tab's Hard Mode switch
static int HardLap(void);         // the run is on lap 2 or later: the engine's hell lap

// The globe's angle table @0xee384 has four entries (Greenland 22, Peru 54,
// Kazakhstan 215, Japan 185) and is not linear in longitude: Bali, 197;
// Seoul, between Bali and Japan, 191; the chase, Siberia (between Kazakhstan
// and Seoul), 202; Edinburgh, between Kazakhstan and Greenland the long way
// round, 335 (estimated from the table, not yet checked on screen: try
// values and look, as for Bali); the Andaman Islands, between Kazakhstan and
// Bali, 206; the Mariana Trench, east of Japan, 181; Dr. Marrow's lab, under
// the Philippine Sea just west of it, 183; the run home, the Coral Sea down to
// the Antarctic (the globe turned to the Tasman Sea, south-east of Bali), 192; Amaterasu, the
// Ross Ice Shelf up the South Atlantic to the Gulf of Guinea (Africa faced, as Paris), 340.
static const World kWorlds[] = {
  { 5, "*beach/beach.world", "*beach/menu/mission5.tga", "*beach/menu/map_level5.tga",
    "*beach/menu/niveau05.mpg", 0x3e50, 0x3e51, 0x69e, 197, RegisterBeach, ForgetBeach,
    HermitBossInit, 1, "*beach/beach_hard.world" },
  { 6, "*seoul/seoul.world", "*seoul/menu/mission6.tga", "*seoul/menu/map_level6.tga",
    "*seoul/menu/niveau06.mpg", 0x3e52, 0x3e53, 0x3e54, 191, RegisterSeoul, ForgetSeoul,
    MagpieBossInit, 0, "*seoul/seoul_hard.world" },
  { 7, "*antarctica/antarctica.world", "*antarctica/menu/mission7.tga", "*antarctica/menu/map_level7.tga",
    "*antarctica/menu/niveau07.mpg", 0x3e55, 0x3e56, 0x3e57, 202, RegisterAntarctica, ForgetAntarctica,
    ChaseBossInit, 0 },
  { 8, "*edinburgh/edinburgh.world", "*edinburgh/menu/mission8.tga", "*edinburgh/menu/map_level8.tga",
    "*edinburgh/menu/niveau08.mpg", 0x3e58, 0x3e59, 0x3e5a, 335, RegisterEdinburgh, ForgetEdinburgh,
    SubagentBossInit, 0, "*edinburgh/edinburgh_hard.world" },
  { 9, "*andaman/andaman.world", "*andaman/menu/mission9.tga", "*andaman/menu/map_level9.tga",
    "*andaman/menu/niveau09.mpg", 0x3e5b, 0x3e5c, 0x3e5d, 206, RegisterAndaman, ForgetAndaman,
    AndamanBossInit, 0, "*andaman/andaman_hard.world" },
  { 10, "*sunda/sunda.world", "*sunda/menu/mission10.tga", "*sunda/menu/map_level10.tga",
    "*sunda/menu/niveau10.mpg", 0x3f83, 0x3f84, 0x3f85, 201, RegisterSunda, ForgetSunda,
    SundaBossInit, 0, "*sunda/sunda_hard.world" },
  { 11, "*abyss/abyss.world", "*abyss/menu/mission11.tga", "*abyss/menu/map_level11.tga",
    "*abyss/menu/niveau11.mpg", 0x3f80, 0x3f81, 0x3f82, 181, RegisterAbyss, ForgetAbyss,
    HunterBossInit, 0, "*abyss/abyss_hard.world" },
  { 12, "*lab/lab.world", "*lab/menu/mission12.tga", "*lab/menu/map_level12.tga",
    "*lab/menu/niveau12.mpg", 0x3f86, 0x3f87, 0x3f88, 183, RegisterLab, ForgetLab,
    LabBossInit, 0, "*lab/lab_hard.world" },
  { 13, "*homeward/homeward.world", "*homeward/menu/mission13.tga", "*homeward/menu/map_level13.tga",
    "*homeward/menu/niveau13.mpg", 0x3f8c, 0x3f8d, 0x3f8e, 192, RegisterHomeward, ForgetHomeward,
    FortBossInit, 0, "*homeward/homeward_hard.world" },
  { 14, "*amaterasu/amaterasu.world", "*amaterasu/menu/mission14.tga", "*amaterasu/menu/map_level14.tga",
    "*amaterasu/menu/niveau14.mpg", 0x3f8f, 0x3f90, 0x3f91, 340, RegisterAmaterasu, ForgetAmaterasu,
    BargeBossInit, 0, "*amaterasu/amaterasu_hard.world" },
  { 15, "*paris/paris.world", "*paris/menu/mission15.tga", "*paris/menu/map_level15.tga",
    "*paris/menu/niveau15.mpg", 0x3f92, 0x3f93, 0x3f94, 337, RegisterParis, ForgetParis,
    TankBossInit, 0, "*paris/paris_hard.world" },
  { 16, "*atlantic/atlantic.world", "*atlantic/menu/mission16.tga", "*atlantic/menu/map_level16.tga",
    "*atlantic/menu/niveau16.mpg", 0x3f95, 0x3f96, 0x3f97, 340, RegisterAtlantic, ForgetAtlantic,
    AtlanticBossInit, 0 },
};
#define NWORLDS ((u32)(sizeof kWorlds / sizeof kWorlds[0]))

// The extra world for a world number, or 0.
static const World* Extra(u32 world) {
  for (u32 i = 0; i < NWORLDS; i++)
    if (kWorlds[i].number == world) return &kWorlds[i];
  return 0;
}

// Hard Mode on: the engine's own hell lap (rank pinned at 20; our worlds load
// their hard world files, our bosses fight their hard fights). Read as a new
// game starts and again as each of our worlds loads -- Next Mission, the
// debriefing's button -- so a switch flipped mid-run counts from the next
// mission on (it used to wait for a new game). Switched off, a lap the switch
// raised drops back; a hell lap from Mission 4's button stays.
static int g_switch_lap;          // the lap is 2 because of the switch

static void SyncHard(u32 shogun) {
  if (HardWanted()) {
    if (R32(shogun, SH_LAP) < 2) {
      R32(shogun, SH_LAP) = 2;
      R32(shogun, SH_PLAYER_RANK) = 20 << 16;   // at the top from the first frame (the floor holds it there)
      g_switch_lap = 1;
    }
  } else if (g_switch_lap) {
    R32(shogun, SH_LAP) = 1;
    g_switch_lap = 0;
  }
}

// The world file to load: the hard one on the hell lap, where the world has one.
static const char* WorldFile(const World* w) { return HardLap() && w->hard ? w->hard : w->world; }

// ... and for a select-menu entry: the game's own four, then the table's rows in order.
static const World* Entry(u32 entry) {
  return entry >= FIRST_EXTRA - 1 && entry < FIRST_EXTRA - 1 + NWORLDS ? &kWorlds[entry - (FIRST_EXTRA - 1)] : 0;
}

// UpdateSelectLevels, both `bl StartNewGame(SHOGUN, entry + 1)`: an entry of
// ours starts its row's world.
#define StartNewGame        FN(void (*)(u32, u32), 0x3941d)
void hook_start_new_game(u32 shogun, u32 world) {
  const World* w = world ? Entry(world - 1) : 0;
  StartNewGame(shogun, w ? w->number : world);
  g_switch_lap = 0;
  SyncHard(shogun);
}

static u32 World_(void) { return R32(UE_GetGameDataPtr5(), SH_WORLD); }

// ---- level select ------------------------------------------------------------

// An engine handle of the form base + entry, for an entry of ours, becomes
// the handle we loaded that asset into.
static u32 Title(u32 h) {
  const World* w = h >= TITLE_BASE ? Entry(h - TITLE_BASE) : 0;
  return w ? w->h_title : h;
}

static u32 Map(u32 h) {
  const World* w = h >= MAP_BASE ? Entry(h - MAP_BASE) : 0;
  return w ? w->h_map : h;
}

static u32 Video(u32 h) {
  const World* w = h >= VIDEO_BASE ? Entry(h - VIDEO_BASE) : 0;
  return w ? w->h_video : h;
}

// ---- the preview clips' memory --------------------------------------------------
// The level select runs in a memory context of its own, and leaving it
// (selectMenu_onExit: UE_PopMemoryContext) forgets every handle made in it
// -- but not the preview clips' decoders, whose memory MPEG_Open takes
// straight from the OS allocator, and nothing ever closed a clip: ~3.8 MB
// was left behind for each, on every visit. The game's own four leaked that
// way; with all eight it was ~30 MB a visit, and the 256 MB guest heap ran
// out after four levels in one session.
//
// So every clip that opens is remembered -- its MPEGDATA, the decoder and
// the file buffer MPEG_Open put there -- and on leaving, those are freed
// (what MPEG_Close frees; not the handle, which the context frees itself:
// freed twice, the handle table came apart a few visits later). Only what is
// still exactly as MPEG_Open left it is touched; anything else is left be.
#define UE_PopMemoryContext FN(void (*)(u32), 0x54825)
#define SL_Mpeg2Close       FN(void (*)(u32), 0x9d818)                      // ARM
#define SL_OSFree           FN(void (*)(u32), 0x5ccf1)
#define NCLIPS 16

static struct { u32 h, clip, dec, buf; } g_clips[NCLIPS];

static s32 OpenClip(u32 h, const char* path, u32 flag) {
  const s32 r = MPEG_Open(h, path, flag);
  const u32 clip = r ? SL_GetHandlePtr(h, 0) : 0;     // MPEGDATA: +0 decoder, +8 file
  if (!clip || !R32(clip, 0)) return r;
  for (u32 i = 0; i < NCLIPS; i++) {
    if (g_clips[i].clip) continue;
    g_clips[i].h = h;
    g_clips[i].clip = clip;
    g_clips[i].dec = R32(clip, 0);
    g_clips[i].buf = R32(clip, 8);
    break;
  }
  return r;
}

// The game's own Mission 1 clip was recorded from an earlier build, where the
// mid-boss's muzzle flash and bolt were yellow-green; the game draws both red.
// beach/ocean_preview.py turns them red, and the Beach's data carries it.
#define OCEAN_CLIP "*beach/menu/niveau01.mpg"

// InitSelectLevels: its four `blx MPEG_Open`, the game's own entries. Entry 0
// plays the corrected clip, or the pack's own if that isn't there.
s32 hook_mpeg_open(u32 h, const char* path, u32 flag) {
  if (h == VIDEO_BASE) {
    const s32 r = OpenClip(h, OCEAN_CLIP, flag);
    if (r) return r;
  }
  return OpenClip(h, path, flag);
}

// selectMenu_onExit: `bl UE_PopMemoryContext`.
void hook_select_exit(u32 arg) {
  for (u32 i = 0; i < NCLIPS; i++) {
    const u32 clip = g_clips[i].clip;
    if (!clip) continue;
    if (SL_GetHandlePtr(g_clips[i].h, 0) == clip) {
      if (R32(clip, 0) == (s32)g_clips[i].dec) {
        SL_Mpeg2Close(g_clips[i].dec);
        R32(clip, 0) = 0;
      }
      if (g_clips[i].buf && R32(clip, 8) == (s32)g_clips[i].buf) {
        SL_OSFree(g_clips[i].buf);
        R32(clip, 8) = 0;
      }
    }
    g_clips[i].clip = 0;
  }
  UE_PopMemoryContext(arg);
}

// InitSelectLevels' fourth `blx MPEG_DecodeFrame`: the last of the engine's
// own select-menu assets is loaded by now, so ours follow.
s32 hook_select_assets(u32 video) {
  const s32 r = MPEG_DecodeFrame(video);
  const u32 sh = UE_GetGameDataPtr5();
  for (u32 i = 0; i < NWORLDS; i++) {
    const World* w = &kWorlds[i];
    UE_LoadSprites5(w->h_title, w->title, 1, R32(sh, SH_UI_SCALE));
    UE_LoadTexture(w->h_map, w->map, 0, 0x10000);
    if (OpenClip(w->h_video, w->video, 1)) {
      MPEG_Start(w->h_video);
      MPEG_DecodeFrame(w->h_video);
    }
  }
  return r;
}

// The preview's MPEG calls, all `blx` with 0x69a + entry: UpdateSelectLevels
// decodes the current entry's clip each frame, and restarts it and puts its
// texture on the screen when the entry changes; InitSelectLevels does the
// same for the entry the menu opens on.
s32 hook_mpeg_decode(u32 video) { return MPEG_DecodeFrame(Video(video)); }
void hook_mpeg_start(u32 video) { MPEG_Start(Video(video)); }
u32 hook_mpeg_texture(u32 video) { return MPEG_GetTexture(Video(video)); }

// UpdateSelectLevels, both `blx UE_SetConstraint`: turn the globe to the
// entry's country, in whole degrees from the four-entry table; ours supply
// their own.
void hook_globe_spin(u32 constraint, s32 degrees) {
  const World* w = Entry(R32(UE_GetGameDataPtr5(), SH_SELECTED));
  UE_SetConstraint(constraint, w ? w->globe : degrees);
}

// RenderSelectLevels: `bl UE_DrawSprite` with the title 0x690 + entry.
void hook_draw_title(u32 sprite, s32 x, s32 y, u32 flags) {
  UE_DrawSprite(Title(sprite), x, y, flags);
}

// RenderSelectLevels: `bl UE_SetEntityTexture` putting map 0x6a0 + entry on
// the globe.
void hook_globe_texture(u32 entity, u32 texture) {
  UE_SetEntityTexture(entity, Map(texture));
}

// ---- loading the world -----------------------------------------------------------

// What the engine's own cases do -- boss music, the world file -- after
// registering the world's cast (its scripts must exist before the world
// file binds their sprites).
// The engine keeps 128 badguy definitions (BH_Init's bag at 0x405 + 0xa, and
// the hash table at + 0xe that finds them), and the game's own 101 are
// registered at start-up. Each added world registers its own as it loads,
// and they were kept for the session: the Beach needs 20, Seoul 16, the
// chase 7 -- so after one world the next could fill the bag, BH_LoadBadGuy
// failed silently, and the first spawn of a missing one crashed in the
// actor creator (the Beach after Seoul or the chase: the hermit's shadow).
// (Making the bag bigger overran the memory laid out after it.)
//
// Their scripts pile up the same way. Every definition's script is compiled
// into one pool (bhdata + 0x14, its write pointer at + 0x10 and the room
// left at + 0xc), and the engine never checks it: the game's own leave
// 14962 halfwords, Seoul takes 8396, the chase 4745, the Beach 3229. Any
// two fit; with all three the last ran on over the partition's
// event-callback table that follows the pool, and the first callback event
// jumped into garbage (the Beach after Seoul and the chase, 20 s in).
//
// So before a world registers its cast, every added world's own
// definitions ('*' paths; never the game's) are taken back out -- badguys,
// bullets, generics, and the actor manager's compiled scripts -- and the
// pool is wound back to its start: the world loading compiles its scripts
// afresh, and only its own are kept.
//
// The bags now hold 256 of each kind (build.py's EXTRA rewrites BH_Init),
// so an added world could compile more script than the engine's pool has
// left. The added worlds' scripts go to a pool of our own instead -- eight
// times what the game leaves free -- and a world that still overflows it
// says so in the engine's log ("ERROR! script pool") rather than writing
// over whatever follows. Compiled scripts are found by address (addScript
// hands AM_RegisterActorDefinition the pointer), so where they live is
// free.
#define WR_GetHash          FN(u32 (*)(const char*, u32), 0x60900)        // ARM
#define WR_LookupHashKey    FN(int (*)(u32, u32, u32*), 0x7860c)          // ARM
#define WR_DeleteHashKey    FN(void (*)(u32, u32), 0x784f8)               // ARM
#define WR_DeleteBagEntry   FN(void (*)(u32, u32), 0x78154)               // ARM
#define WR_GetEntryFromBagIndex FN(u32 (*)(u32, u32), 0x78374)            // ARM
#define WR_GetHandlePtr     FN(u32 (*)(u32, u32), 0x4d6bd)
#define WR_BH               0x405u
#define WR_AM               (WR_BH + 1)   // the actor manager: bag + 2, hash + 3
#define WR_POOL_LEFT        0xc           // halfwords, signed: < 0 overflowed
#define WR_POOL_NEXT        0x10
#define WR_Log              FN(void (*)(const char*, ...), 0x4875d)
#define WR_POOL_HALFWORDS   0x20000u      // 256 KB, in the blob's .bss

// One kind of definition: its bag and hash table (BH_Init): badguys 0xa /
// 0xe, bullets 0xb / 0xf, generics 0xd / 0x11. Each one's compiled script
// is the actor manager's definition under the same name, which its hash
// table finds by bag index.
static void Forget(const char* const* names, u32 n, u32 bag, u32 hash) {
  for (u32 i = 0; i < n; i++) {
    if (names[i][0] != '*') continue;           // the game's own: never
    const u32 h = WR_GetHash(names[i], 1);
    u32 entry = 0;
    if (WR_LookupHashKey(WR_BH + hash, h, &entry) && entry) {
      WR_DeleteHashKey(WR_BH + hash, h);
      WR_DeleteBagEntry(WR_BH + bag, entry);
    }
    u32 index = 0;
    if (WR_LookupHashKey(WR_AM + 3, h, &index)) {
      WR_DeleteBagEntry(WR_AM + 2, WR_GetEntryFromBagIndex(WR_AM + 2, index));
      WR_DeleteHashKey(WR_AM + 3, h);
    }
  }
}

void ForgetDefinitions(const char* const* badguys, u32 nb, const char* const* bullets, u32 nu,
                       const char* const* generics, u32 ng) {
  Forget(badguys, nb, 0xa, 0xe);
  Forget(bullets, nu, 0xb, 0xf);
  Forget(generics, ng, 0xd, 0x11);
}

// Going on from one added world to the next (shim_next_world) loads it
// without StartNewGame, which the level select and the hell button go
// through, so the last world's actors are still about -- running scripts
// Forget is about to take away (Mission 5 -> 6 crashed in AM_UpdateActor a
// few seconds into Seoul). So they go first, as StartNewGame does it: the
// remove callbacks off, then everything cleared.
#define WR_SetRemoveBadGuyCb  FN(void (*)(u32, u32), 0xbaed4)              // ARM
#define WR_SetRemoveGenericCb FN(void (*)(u32, u32), 0xbaef4)              // ARM
#define WR_ClearEntities      FN(void (*)(u32), 0xc0b60)                   // ARM

// The added worlds' script pool. The game's own worlds compile nothing
// after start-up, so the engine's pointer can stay in it when they load.
static unsigned short g_pool[WR_POOL_HALFWORDS];

void c_load_extra(u32 shogun, u32 world) {
  const World* w = Extra(world);
  if (!w) {                                     // (a world number with no row: nothing to load)
    WR_Log("ERROR! no world %d\n", world);
    return;
  }
  SyncHard(shogun);                             // before WorldFile() picks the file
  LoadBossMusic();
  const u32 bh = WR_GetHandlePtr(WR_BH, 0);
  WR_SetRemoveBadGuyCb(WR_BH, 0);
  WR_SetRemoveGenericCb(WR_BH, 0);
  WR_ClearEntities(WR_BH);
  for (u32 i = 0; i < NWORLDS; i++) kWorlds[i].forget();
  R32(bh, WR_POOL_NEXT) = (u32)g_pool;
  R32(bh, WR_POOL_LEFT) = WR_POOL_HALFWORDS;
  w->cast();
  LoadWorldFile(shogun, WorldFile(w));
  if (w->ocean_sounds) LoadWorld1(shogun);
  if (R32(bh, WR_POOL_LEFT) < 0)
    WR_Log("ERROR! script pool: world %d overflowed it by %d halfwords\n", world,
           -R32(bh, WR_POOL_LEFT));
}

void c_init_extra(u32 shogun, u32 world) {
  const World* w = Extra(world);
  if (w) InitWorldFile(shogun, WorldFile(w));
}

// LoadWorld @0x47bd8: `cmp r1, #4 ; bhi out` with r1 = world, r4 = SHOGUN.
// 0-4 return to the jump table that follows; ours load and join the common
// tail at 0x47c12 (which reloads r2 for its log line); anything else takes
// the original `out`.
__attribute__((naked)) void shim_load_world(void) {
  __asm__ volatile(
      "cmp r1, #4\n"
      "bls 3f\n"
      "cmp r1, #16\n"
      "bhi 2f\n"
      "adds r0, r4, #0\n"
      "bl c_load_extra\n"
      "ldr r3, =0x10047c13\n"
      "bx r3\n"
      "2:\n"
      "ldr r3, =0x10047c1b\n"
      "bx r3\n"
      "3:\n"
      "bx lr\n"
      ".ltorg\n");
}

// InitWorld @0x44e8c: `cmp r2, #4 ; bhi out` with r2 = world, r4 = SHOGUN;
// out (0x44ea6) is InitBoss and SetState, which every world goes on to.
__attribute__((naked)) void shim_init_world(void) {
  __asm__ volatile(
      "cmp r2, #4\n"
      "bls 3f\n"
      "cmp r2, #16\n"
      "bhi 2f\n"
      "adds r0, r4, #0\n"
      "adds r1, r2, #0\n"
      "bl c_init_extra\n"
      "2:\n"
      "ldr r3, =0x10044ea7\n"
      "bx r3\n"
      "3:\n"
      "bx lr\n"
      ".ltorg\n");
}

// InitBoss @0x315b8: `subs r3, #0x28 ; ldr r3, [r4, r3]` reads the world for
// its switch of mid-bosses. Ours read as 1: Ocean's case, whose
// `bl OceanMidBossInit` is hooked below.
__attribute__((naked)) void shim_init_boss(void) {
  __asm__ volatile(
      "subs r3, #0x28\n"
      "ldr r3, [r4, r3]\n"
      "cmp r3, #5\n"
      "blo 1f\n"
      "movs r3, #1\n"
      "1:\n"
      "bx lr\n");
}

// GetCurrentLeaderboardLevel's two callers (the results screen and its
// ranking lines). It answers the world number, or 0 for the overall board,
// and RegisterScore indexes four-entry tables with it -- best scores and
// board names -- so a world past 4 would read and write past them (it
// crashed the results screen). Ours go on the overall board instead.
#define GetCurrentLeaderboardLevel FN(u32 (*)(u32), 0x28549)
u32 hook_leaderboard_level(u32 shogun) {
  const u32 level = GetCurrentLeaderboardLevel(shogun);
  return Extra(level) ? 0 : level;
}

// InitBoss, world 1's case (ours read as 1, above): `bl OceanMidBossInit`.
void hook_mid_boss_init(u32 boss) {
  const World* w = Extra(World_());
  if (w)
    w->mid_boss(boss);
  else
    OceanMidBossInit(boss);
}

// OceanMidBossDeath: `bl OceanBossInit` sets up the final boss. Only Ocean's
// mid-boss gets here; ours set up their final bosses themselves.
void hook_final_boss_init(u32 boss) { OceanBossInit(boss); }

// ---- from one mission to the next ------------------------------------------------
// When the results screen closes, onUpdate @0x4801e picks what follows by
// world: `cmp r1, #3 ; bls next ; b ending` -- worlds 1-3 go on to the next
// (world + 1, state 3), and anything past 3 to the debriefing, the game's
// ending. With r1 = world and r4 = SHOGUN:
//   * 1-3 and 5-13 go on to the next world (0x48024);
//   * 4 still ends the game's own story (the `b ending` after the patch);
//   * 14, the last, goes back to the menu as a game over does (0x48304).
__attribute__((naked)) void shim_next_world(void) {
  __asm__ volatile(
      "cmp r1, #3\n"
      "bls 1f\n"
      "cmp r1, #4\n"
      "beq 3f\n"
      "cmp r1, #16\n"
      "bhs 2f\n"
      "1:\n"
      "ldr r3, =0x10048025\n"
      "bx r3\n"
      "2:\n"
      "ldr r3, =0x10048305\n"
      "bx r3\n"
      "3:\n"
      "bx lr\n"
      ".ltorg\n");
}

// The debriefing (after Mission 4's ending plays) offers Back to Menu and
// Continue to Hell Mode, with the hell banner above them; we add Next
// Mission, which goes on to Mission 5. The game's three are laid out from
// the bottom of the screen: menu UIScaled(105) up, hell 205, banner 304.
// Next Mission takes the hell button's place and the hell button and banner
// move up a slot (100). The button fades in with the others, once the
// ending's boards are done (DB_ALPHA reaches 1.0).
#define DB_Load             FN(void (*)(u32), 0x2e6cd)
#define DB_Update           FN(void (*)(u32), 0x39325)
#define DB_Render           FN(void (*)(u32), 0x41599)
#define DB_UIScaled         FN(s32 (*)(s32, u32), 0x2abf5)
#define DB_UIDisplayFactor  FN(u32 (*)(u32), 0x2b1f9)
#define DB_HighQuality      FN(void (*)(void), 0x515c5)
#define DB_LowQuality       FN(void (*)(void), 0x515a9)
#define DB_Filtering        FN(void (*)(void), 0x51571)
#define DB_NoFiltering      FN(void (*)(void), 0x5158d)
#define DB_CreateIcon       FN(void (*)(u32, u32), 0xb5d48)                 // ARM
#define DB_UpdateWidget     FN(void (*)(u32, s32, s32), 0x76a54)            // ARM
#define DB_RenderWidget     FN(void (*)(u32, s32, s32), 0x769fc)            // ARM
#define DB_IconClicked      FN(int (*)(u32), 0xb5d20)                       // ARM
#define DB_PushAlpha        FN(void (*)(s32), 0x4ecc5)
#define DB_PopAlpha         FN(void (*)(void), 0x4ed35)
#define DB_ScreenWidth      FN(s32 (*)(void), 0x4e955)
#define DB_ScreenHeight     FN(s32 (*)(void), 0x4e971)
#define DB_GetWidth         FN(s32 (*)(u32), 0x4d9b9)
#define DB_SetStateFade     FN(void (*)(u32, u32, const char*), 0x2a10d)

#define DB_ALPHA      0x8d538     // the buttons' fade, 0 -> 0x10000
#define DB_LAST_STORY 4           // the world whose end is the debriefing
#define DB_NEXT_Y     205         // UIScaled from the bottom: the hell button's place
#define DB_RAISE      100         // ... and how far the hell button and banner move up
#define SPR_NEXT      0x3f8a      // next_mission.tga (make_next_mission.py)
#define W_NEXT        0x3f8b      // its widget
#define ST_LOAD_WORLD 3           // onUpdate: load SH_WORLD and play it

static int NextOffered(u32 sh) { return R32(sh, SH_WORLD) == DB_LAST_STORY; }

static s32 NextX(void) { return (DB_ScreenWidth() - DB_GetWidth(SPR_NEXT)) / 2; }
static s32 NextY(void) { return DB_ScreenHeight() - DB_UIScaled(DB_NEXT_Y, 1); }

// debriefing_onEnter: `bl LoadDebriefing`. Its button, as it loads the
// game's (high quality, filtered), in the debriefing's memory context.
void hook_load_debriefing(u32 sh) {
  DB_Load(sh);
  DB_HighQuality();
  DB_Filtering();
  UE_LoadSprites5(SPR_NEXT, "*next_mission.tga", 1, DB_UIDisplayFactor(1));
  DB_CreateIcon(W_NEXT, SPR_NEXT);
  DB_NoFiltering();
  DB_LowQuality();
}

// UpdateDebriefing's and RenderDebriefing's `bl UIScaled` for the hell
// button (205) and RenderDebriefing's for the banner (304).
s32 hook_debrief_raise(s32 v, u32 flag) {
  return DB_UIScaled(NextOffered(UE_GetGameDataPtr5()) ? v + DB_RAISE : v, flag);
}

// onUpdate: `bl UpdateDebriefing`. Then the button, once the others are up:
// a press goes on to Mission 5 as the hell button goes to Mission 1 (the
// score carried on, the difficulty as it is).
void hook_update_debriefing(u32 sh) {
  DB_Update(sh);
  if (!NextOffered(sh) || R32(sh, DB_ALPHA) != 0x10000) return;
  DB_UpdateWidget(W_NEXT, NextX(), NextY());
  if (DB_IconClicked(W_NEXT)) {
    R32(sh, SH_WORLD) = FIRST_EXTRA;
    DB_SetStateFade(sh, ST_LOAD_WORLD, "Loading...");
  }
}

// onRender: `bl RenderDebriefing`. The button, faded with the others.
void hook_render_debriefing(u32 sh) {
  DB_Render(sh);
  if (!NextOffered(sh)) return;
  DB_PushAlpha(R32(sh, DB_ALPHA));
  DB_RenderWidget(W_NEXT, NextX(), NextY());
  DB_PopAlpha();
}

// ==== hard mode (docs/HARD_MODE.md) ===========================================
// The Hard Mode switch in the settings' Cheats tab (android/.../hardmode.cpp)
// writes the files directory's hard_mode.bin (4 bytes: 1 on, 0 off); a new game
// reads it. On, the chosen mission starts on lap 2 -- the engine's hell lap, as
// Mission 4's hell button gives -- where onUpdate pins the rank at 20 and our
// worlds and bosses play their hard versions.
#define WR_isFileExist  FN(s32 (*)(const char*), 0x883f0)                      // ARM
#define WR_LoadBinFile  FN(s32 (*)(const char*, void*, u32, u32, u32), 0x88db4) // ARM
#define HARD_FILE     "*hard_mode.bin"
volatile s32 g_hard = -1;         // -1: the switch decides; the desktop harness sets 0 / 1 (levelkit.py; never written here, so volatile)

static int HardWanted(void) {
  if (g_hard >= 0) return g_hard > 0;
  s32 on = 0;
  if (WR_isFileExist(HARD_FILE)) WR_LoadBinFile(HARD_FILE, &on, 4, 0, 0);
  return on > 0;
}

static int HardLap(void) { return R32(UE_GetGameDataPtr5(), SH_LAP) >= 2; }
