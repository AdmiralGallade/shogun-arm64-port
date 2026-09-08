# What can be changed, and how

Running the original binary rather than rewriting it turns out to be less
limiting than it sounds. The engine was built to be data-driven and it exports
most of its own machinery, so a lot can be changed from the host side with no
patching at all.

Ordered by how invasive they are.

## 1. Call the engine's own API (no patching)

`libHAL.Android.so` exports **90 `BH_*` functions** — the bullet-hell
subsystem — plus several hundred `UE_*` engine functions, all in `.dynsym`.
The runtime can call any of them by name:

```cpp
uint32_t out = 0;
std::string err;
rt.CallSym("BH_SetMinRank", {kBhHandle, 20u << 16}, &out, &err);
```

A sample of what is reachable:

| Function | Effect |
|---|---|
| `BH_SetMinRank` / `BH_GetMaxRank` | difficulty floor and ceiling (see below) |
| `BH_EnableSuicideBullets` | enemies fire a spread when they die |
| `BH_SetBadGuyInvincibility` | make an enemy unkillable |
| `BH_SetBadGuyHealth` | retune a specific enemy |
| `BH_CancelBullets` | clear the screen |
| `BH_SpawnBadGuy` / `BH_SpawnBonus` | put things into the level |
| `BH_SetPlayfieldSize` | change the playable area |
| `BH_GetGrazeCount` / `BH_GetBulletCount` | read live game state |
| `UE_GetConfigValue` / `UE_SetConfigValue` | the engine's own config system |

The handle for the bullet-hell module is `0x405`. Handles are ids in a
`UE_AllocHandle` namespace, so new objects can claim unused ones.

**Rule:** these must be called from the tick, never from inside a guest
callback. `Runtime::CallAddr` now refuses re-entrant calls rather than
deadlocking, and will tell you so in the error string.

## 2. Watch or replace any guest function

Two primitives make the guest's internals reachable:

- `Runtime::AddWatch(addr, fn)` — run host code at a guest address with the
  registers intact and execution continuing. Used to learn the address of the
  game state, which the engine never hands out.
- `Runtime::AddTrapRegion(base, size, fn, mapped)` — map an address the guest
  can *call*. The engine sees a function pointer; calling it lands in host code
  that returns to `LR` like any leaf function. This is the same mechanism the
  99 imports use, and it is how a settings switch gets a callback written in
  C++.

`Runtime::SymAddr("_Z16InitSettingsMenuP6SHOGUN")` resolves any symbol,
mangled or not — the library shipped unstripped, so all 8,777 are available.

## 3. Add to the game's own menus (no patching)

The settings menu is data. Lines live in a fixed `[tab][line]` array inside the
game state, and **the per-tab line count is a `uint16` in memory**, not a
compiled-in bound — so a line can be appended at runtime and the menu's own
loops will pick it up.

Measured layout:

| Thing | Value |
|---|---|
| Tab array | `SHOGUN + 0x8ba30`, stride `0x568` |
| Lines within a tab | `tab + 0x1c`, stride `0x60`, capacity 14 |
| Line count | `uint16` at `tab + 8` |
| Tab 0 | Controls — 6 lines used, 8 free |
| Tab 1 | Options — 10 lines used, 4 free |
| Widget type at `line + 0x18` | 0 = label, 1 = separator, 2 = switch, 4 = slider |
| Switch handle at `line + 0x1c` | game uses `0x6cb`–`0x6d7` |

Build the line with the engine's own constructor so it is a real one —
registered by name, rendered and hit-tested like the rest:

```
InitSwitchSettingsLine(line, "HardMode", x, y, w, h,
                       switchHandle, "Hard Mode", callback, user, state)
InitTextSettingsLine(...)      // label
InitSlideBarSettingsLine(...)  // slider, takes min/max/step
```

then bump the count. `hardmode.cpp` does exactly this and re-checks every
assumption first, because a wrong offset writes over live game state.

## 4. Edit text in place

Every UI string is a NUL-terminated constant in `.rodata`, editable with
`tools/patch_changelog.py`. A replacement may be shorter than the original but
never longer; the tool length-checks each slot before writing anything.

## 5. Edit the game's data files

The asset pack holds `.badguy`, `.bullet`, `.bonus`, `.generic` and
`.partition` files, read through the engine's config-script parser with named
fields — `Health`, `Radius`, `Attraction`, `AttractionRankRatio`,
`BulletCancel`, `Collision`, `Visible`. Enemy stats are values, not compiled
constants. Changing them means intercepting the loader or repacking, neither of
which is done yet.

Bullet patterns are **scripted**, with a VM whose variables include `RANK`,
`ROUNDRANK`, `TRUNCRANK` and `DIFFICULTY` — which is how difficulty actually
reaches the patterns.

## 6. Levels

A level is four separate things, and the engine exports enough to reach all of
them.

| Piece | Format | Loaded by |
|---|---|---|
| World file | text config | `LoadWorldFile(SHOGUN*, char*)` -> `UE_LoadConfigFile` |
| Partition | binary | `InitWorldFile` -> `BH_LoadPartition` -> `UE_LoadBinFile` |
| Entity defs | text config | `BH_LoadBadGuy` and friends, by name |
| Registration | mission index | `StartNewGame(SHOGUN*, n)` writes `SHOGUN+0x4e70` |

### The partition format

`BH_SavePartition(handle, name)` gives it away entirely -- it is a single call
to `UE_SaveBinFile(name, &count, 2 + count*12)`. So a partition is a `uint16`
event count followed by that many 12-byte events, and it can be read straight
out of the BH context at `+0x10168` without going near a file.

Dumped from `worlds/ocean/scripts/ocean.world` (133 events, 1596 bytes):

```c
struct EVENT {            // 12 bytes
  uint16 time;            // 109 .. 4997, monotonically non-decreasing
  uint16 where;           // spawn position for ordinary events
  uint32 name_hash;       // UE_GetHashFromString of the entity name
  uint32 flags;           // 0, 0x10, 0x18, 0x20, 0x30, 0x54, 0x70, 0xfff0
};
```

Confirmed against the real data: the time column is sorted, so a partition is a
timeline; 30 distinct hashes appear across 133 events, so a level reuses a small
cast; and eight events carry a `name_hash` of 0 with a `where` field outside the
playfield, which lines up with `BH_RegisterPartitionCustomEventCallback` -- they
are almost certainly scripted events rather than spawns.

### What is still unknown

- **Hash to name.** `UE_GetHashFromString` is exported, so candidate names can
  be hashed and matched, but the names themselves live in the pack. Hooking the
  `"BH: Loading %s.badguy..."` path would capture a level's cast as it loads.
- Whether `where` is purely an x coordinate; it is for ordinary spawns, but the
  custom events use it for something else.
- Whether a partition can reference an entity the pack does not contain.

### Why this is tractable

Every piece needed is a public export:

| Export | Use |
|---|---|
| `BH_GetPartitionEvent` / `BH_SetPartitionEvent` | read and write single events through the engine, no memory poking |
| `BH_SavePartition` / `BH_LoadPartition` | round-trip a whole timeline |
| `UE_GetHashFromString` | resolve entity names to the hashes events use |
| `UE_CreateArchive`, `UE_AddBufferToArchive`, `UE_PushCurrentArchive` | build a small archive of new files and have it searched ahead of the 19 MB pack, instead of repacking it |
| `UE_SaveConfigFile`, `UE_WriteConfigScript` | write the text formats |

`BH_SavePartition` and `UE_WriteConfigScript` existing at all says int13 had an
in-engine editor, which is why the formats are so approachable.

## 7. Patch the binary

Still available and cheap for small things (the original APK patcher does two
4-byte edits to stub the dead billing calls), but nothing in the port needs it
any more. Prefer the API.

---

## Worked example: difficulty

There is no "hard mode" in this game. There is **rank** — dynamic difficulty,
16.16 fixed point, stored at `PLAYER + 0x90`:

- `PlayerAddRank(PLAYER*, delta)` is called from `StrikeBadguy` (you killed
  something), `ControlPlayer` (you are playing well), and `StrikePlayer` (you
  got hit, rank falls).
- Each change is clamped to `[BH_GetMinRank(), BH_GetMaxRank()]`.
- Measured live: **min 1.0, max 20.0**, typical in-game value around 5.0.

The engine ratchets the floor itself, in `onUpdate`:

```c
if (shogun->mission > 1) BH_SetMinRank(BH, 20.0);   // = the ceiling
BH_EnableSuicideBullets(BH, mission == 2);
```

So the later missions pin rank to its maximum permanently. "Hard mode from the
start" is that same floor applied from mission 1 — which is all
`SetHardMode()` does, re-applied every 30 ticks because `onUpdate` re-asserts
its own value on the later missions.

Turning it off hands the floor back to `1.0` and lets the engine resume
managing difficulty normally.
