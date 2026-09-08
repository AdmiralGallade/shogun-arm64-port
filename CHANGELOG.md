# Changelog

All notable changes to the port. The game itself is unmodified 2012 code apart
from the in-game "What's New" panel, which is rewritten to carry the credit.

## [1.2.4-arm64] — 2026-09-08

The first build that runs the game end to end on a 64-bit-only Android device.

### Added
- ARM32 guest runtime in C++: ELF loader, relocations, trap-based import
  interception, per-thread guest stacks.
- 99 host shims covering libc, math, pthreads, file I/O and sockets.
- Fake 233-slot `JNIEnv` and `JavaVM` in guest memory.
- GLES 1.x forwarding to the device driver, including the 16.16 fixed-point
  entry points and per-draw client-array upload.
- Vertex and element buffer objects, with a CPU shadow of their contents.
- Android shell: `GLSurfaceView`, letterboxed viewport, touch mapping, an
  `AudioTrack` thread fed by the engine's software mixer.
- Gradle-free packaging pipeline (`build-apk.sh`).
- Launcher icon rebuilt from the original 96x96 bitmap as an adaptive icon, so
  a modern launcher masks the artwork deliberately instead of shrinking a
  legacy bitmap into a white circle. Without it the app showed the default
  Android robot, because the manifest declared no icon at all.
- Desktop Python harness the whole design was proven on, 39/39 checks passing.

### Added (gameplay)
- **A "Cheats" tab in the game's own settings menu**, beside Controls and
  Options, holding Hard Mode, Full Capsules and a Shield strength slider. The
  tab count is a plain field at `SHOGUN+0x8ba34`, so a third tab can be
  declared; the memory it needs (`0x8c500..0x8ca68`) was checked against every
  function in the binary and nothing else addresses it. Tab 3 onward belongs to
  the info box and What's New, so exactly one spare tab exists.
- **Full Capsules** sets the player's capsule count (`PLAYER+0x74`, which
  `InitPlayerGame` sets to 3) to its maximum at the start of a game.
- **Shield strength**, 0.2x to 5x with the knob centred at no change. Rather
  than find and patch every damage site, the shield value (`PLAYER+0x8c`) is
  watched and the share of each hit the multiplier says should not have landed
  is given back.
- **A "Hard Mode" switch in the game's own Options tab.** The game has no hard
  mode as such: it has *rank*, a dynamic difficulty value that rises as you
  play well, and the engine pins it to its ceiling from mission 3 onward. The
  switch applies that same floor from the first mission.
- The line is a real one, built with the engine's own
  `InitSwitchSettingsLine` and appended to the settings array at runtime -- the
  per-tab line count is a `uint16` in memory rather than a compiled-in bound.
  No binary patching is involved.
- `Runtime::AddWatch` (observe a guest function without disturbing it) and
  `Runtime::SymAddr`, which is how the host learns the address of the game
  state the engine never hands out.

### Fixed
- **Using the shield slider froze the game.** A slide-bar's last three
  arguments are `(onMove, onRelease, user)`, not `(callback, user, spare)` --
  the symbol table names them, `slideBar_masterVolume_onMove` and
  `slideBar_sensitivityX_onRelease`. Passing the game state as the second one
  had the engine call it as a function and branch into heap data. The same
  mistake put the knob off-scale: `UpdateSettingsLine` mirrors a value as
  `(max + min) - v`, so the range is the two constants every slider is built
  with, `0x10000..0x20000` -- and the initial value handed over was below the
  minimum. Confirmed by a real drag reporting `0x1cccd` and the maximum landing
  exactly on `0x20000`.
- **Toggling a host-provided settings switch froze the game.** The switch
  callback runs inside guest execution, on a thread already holding the runtime
  lock; calling back into the guest from there deadlocked. Guest calls are now
  deferred to the tick, and `Runtime::CallAddr` refuses a re-entrant call with
  an error instead of hanging.
- **Saves were never written.** The engine forms a save path by pasting the
  storage directory it is handed onto a file name, with no separator of its
  own, so `.../files` + `settings.sav` became `.../filessettings.sav`. The path
  shim then re-prefixed the already-absolute result, giving
  `<root>/<root>/<name>`. Every write failed silently; progress, settings and
  the tutorial-completed flag were lost on every close.
- **The engine was never told the app was pausing.** `onApplicationPause` is
  its only shutdown hook and nothing else writes the save file. It is now
  called on the GL thread, before that thread stops.
- **Textures were lost on minimise.** `GLSurfaceView` destroys the EGL context
  in `onPause`; the engine is never notified and goes on drawing with dead
  texture and buffer names. The context is now preserved, and if the driver
  refuses, every texture, buffer and enable is rebuilt from a shadow copy.
- **Audio did not come back after a minimise.** The audio thread was torn down
  in `onPause` and the engine has no resume hook to rebuild it.
- **The activity could be recreated**, restarting the engine and discarding the
  session. It now declares the configuration changes it handles itself.
- 3D geometry drew from the wrong vertex data: a VBO-backed array addresses its
  data by offset, and offset 0 is the normal case, so treating 0 as "no
  pointer" skipped every mesh array.
- Buffer and texture names could alias, letting two meshes share one buffer.
- Audio stopped after about a second: the per-frame path took a new global
  reference and a new staging buffer every frame until the JNI arena ran dry.
- The asset pack was read at the wrong offset, because it sits inside the APK
  but the engine seeks absolutely. It is extracted to app storage first.
- `glClearColor` was mapped to the fixed-point entry point, so float bits were
  read as 16.16 and clamped to white.
- `initAudio(II)Z` is `(bufferSize, sampleRateHz)`, in that order.
- `gethostbyname` returns a pointer, so it must fail with `NULL`, not `-1`.

### Known limitations
- Gameplay past the early missions is lightly tested.
- Google Play Billing v1 is stubbed out. The service has not existed for over a
  decade and nothing in this build was gated behind it.
- No leaderboard or social features: the servers are gone.
