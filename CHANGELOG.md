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
- Desktop Python harness the whole design was proven on, 39/39 checks passing.

### Fixed
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
