<div align="center">

# Shogun · 64-bit

**A 2012 bullet-hell shooter, running on hardware that cannot execute a single one of its instructions.**

[![Platform](https://img.shields.io/badge/platform-Android%2026%2B%20arm64-3DDC84?logo=android&logoColor=white)](#quick-start)
[![Status](https://img.shields.io/badge/status-playable-success)](#status)
[![Game binary](https://img.shields.io/badge/game%20binary-unmodified-blue)](#the-approach)
[![License](https://img.shields.io/badge/license-MIT-lightgrey)](LICENSE)

<img src="docs/screenshot.png" width="300" alt="Shogun running on a 64-bit Pixel">

</div>

> [!IMPORTANT]
> **This repository contains the port, not the game.** No engine binary, asset
> pack, artwork or music is distributed here. You need your own copy of the
> APK — see [Supply the game](#2-supply-the-game).

---

## The problem

*Shogun: Rise Of The Renegade* shipped as **one ARMv7 native library**,
`libHAL.Android.so`. The Java side is a shell; all of the game is in that
binary. There is no source, and int13 is gone.

Modern phones have dropped 32-bit ARM entirely:

```console
$ adb shell getprop ro.product.cpu.abilist32
                                    # empty

$ adb install Shogun-1.2.4.apk
  INSTALL_FAILED_NO_MATCHING_ABIS
```

That empty line is the whole problem. It is not a missing library or a policy
check — **the CPU cannot execute any instruction in that file.** No amount of
repackaging fixes it.

## The approach

So the 2012 library is not recompiled. It is **run**, inside an emulated ARM32
address space, with every call it makes *out of itself* caught at the boundary
and serviced by 64-bit native code.

```
        Android (arm64)                     guest (ARM32, emulated)
  ┌───────────────────────────┐      ┌──────────────────────────────┐
  │ ShogunActivity            │      │                              │
  │   GLSurfaceView (GLES1)   │      │      libHAL.Android.so       │
  │   AudioTrack thread       │      │      (unmodified, 2012)      │
  │                           │      │                              │
  │ libshogun.so              │◄────►│  imports resolved to a trap  │
  │   Runtime   (ELF + CPU)   │ trap │  page; every call out lands  │
  │   shims     (99 imports)  │      │  in a 64-bit shim            │
  │   gl        (GLES1 fwd)   │      │                              │
  │   jni_bridge (fake JNIEnv)│      └──────────────────────────────┘
  └───────────┬───────────────┘
              │  real glDrawArrays, glTexImage2D, …
              ▼
       PowerVR GLES 1.x driver
```

Four pieces do the work:

| Piece | What it does |
|---|---|
| **Loader** | Maps the `PT_LOAD` segments and applies all **754 relocations**. Every PLT slot for an imported symbol is pointed at a *trap page* rather than at real code. |
| **Shims** | **99 host implementations** — libc, math, pthreads, file I/O, GLES. Arguments are read straight out of the guest's registers and stack under AAPCS soft-float rules. |
| **GL bridge** | Forwards fixed-function GLES 1.x to the device driver untouched. No shader translation, no ANGLE. Converts the 16.16 fixed-point entry points the engine prefers. |
| **JNI bridge** | A **233-slot `JNIEnv`** built inside guest memory. The engine calls into it exactly as it would have on a 2012 device; each slot traps out to the real one. |

## Status

Running on a Pixel 10 Pro, Android 17 — `arm64-v8a` only, PowerVR GPU.

| | |
|---|---|
| Boot, asset pack, save data | ✅ |
| 2D rendering — menus, HUD, bullets | ✅ |
| 3D rendering — hangar, level transitions | ✅ |
| Audio — MP3 music and SFX at 22050 Hz | ✅ |
| Touch input, pause/resume, saved progress | ✅ |
| **Frame cost** | **5.8–12.9 ms** against a 16.7 ms budget |
| Audio callback | ~3 ms per 512-sample buffer |
| Emulated throughput | ~1650–2000 MIPS, against the ~600 MIPS it was written for |

The **online leaderboard works again** — not by reviving int13's dead Hub
Server, but by doing the networking in the port and feeding the engine's own
`onReceiveScore` callback. Runs on a free Cloudflare Worker; see
[server/](server/). Off by default until you point it at an endpoint.

There is also a **Cheats tab** in the game's own settings menu — Hard Mode,
Full Capsules and a shield-strength slider — built with the engine's own widget
constructors rather than drawn over the top of it. See
[MODDING.md](docs/MODDING.md).

## The game

| | |
|---|---|
| **Title** | Shogun: Rise Of The Renegade |
| **Developer** | [int13](https://www.linkedin.com/company/int13), France |
| **Released** | 23 February 2012 on Android; iOS earlier that year |
| **Genre** | Vertical-scrolling bullet hell / danmaku, four levels |
| **Price then** | First level free, $2.49 for the full game |

A deliberate throwback to 1990s Japanese arcade shooters, the kind where most
of the screen fills with projectiles. Its signature mechanic: **lift your finger
and time slows to a crawl** while a weapon-select ring opens around the ship —
spread, laser, or homing.

int13 were mobile **augmented-reality** specialists, shipping across iOS,
Android, bada, WP8 and the Nintendo 3DS. The teardown corroborates that from
the inside: the binary still carries `net.int13.ardefender` and a set of camera
callbacks that must resolve at startup even though Shogun never once uses them.

## Quick start

### 1. Prerequisites

| Requirement | Notes |
|---|---|
| Android SDK | build-tools 36 and platform 36 — set `ANDROID_HOME` |
| JDK 17+ | set `JAVA_HOME`; Android Studio ships one at `jbr/` |
| Android NDK | set `ANDROID_NDK` |
| CMake 3.22+, Python 3.9+ | |
| `pip install Pillow` | for launcher-icon generation |

### 2. Supply the game

Put **your own copy** of the Shogun APK in the repository root as
`Shogun-1.2.4.apk`, then:

```bash
python tools/extract_assets.py Shogun-1.2.4.apk
```

That unpacks the engine binary and the encrypted asset pack into the places the
build expects, rebuilds the launcher icon as an adaptive icon, and rewrites the
in-game "What's New" panel to credit the port. Nothing it produces is tracked
by git.

### 3. Build Unicorn for arm64-v8a

```bash
git clone https://github.com/unicorn-engine/unicorn
cd unicorn && mkdir build && cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=$ANDROID_NDK/build/cmake/android.toolchain.cmake \
         -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-26 \
         -DCMAKE_BUILD_TYPE=Release -DUNICORN_ARCH=arm
cmake --build . -j
cp libunicorn.so ../../android/app/src/main/jniLibs/arm64-v8a/
```

### 4. Build the runtime

```bash
cd android/app/src/main/cpp && mkdir -p build && cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=$ANDROID_NDK/build/cmake/android.toolchain.cmake \
         -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-26 \
         -DCMAKE_BUILD_TYPE=Release \
         -DUNICORN_INC=/path/to/unicorn/include \
         -DUNICORN_LIB=/path/to/unicorn/build/libunicorn.so
cmake --build . -j
```

### 5. Package and install

```bash
./android/build-apk.sh
adb install -r android/shogun-arm64.apk
```

`build-apk.sh` runs aapt2, javac, d8, zipalign and apksigner directly — no
Gradle. On first run it generates a debug keystore beside itself; that key is
gitignored, and you need the same one to sign any later update.

## Documentation

| Document | What it covers |
|---|---|
| **[FROM-ONE-APK.md](docs/FROM-ONE-APK.md)** | How this was arrived at, starting from nothing but the APK — what to look at, in what order, and which findings decided what happened next |
| **[PORTING.md](docs/PORTING.md)** | The guest address map, and the nine findings that cost the most to learn |
| **[MODDING.md](docs/MODDING.md)** | What can be changed and how — the engine's 90-function gameplay API, adding to the game's own menus, and the level format |
| **[HARNESS.md](docs/HARNESS.md)** | The desktop Python harness the whole design was proven on first |
| **[server/README.md](server/README.md)** | Why the original leaderboard is unreachable, and the free replacement that stands in for it |

## Repository layout

```
android/app/src/main/cpp/    the runtime: loader + CPU, shims, GL, JNI, cheats
android/app/src/main/java/   the Android shell: Activity, GL surface, audio
android/build-apk.sh         Gradle-free packaging pipeline
runtime/                     the desktop harness the design was proven on
apk-patch/                   repack and re-sign the original 32-bit APK
bench/                       trap-cost benchmark: the number that decides 60 fps
tools/                       asset extraction, icon generation, text patching
device-test.py               everything about a device adb alone can establish
```

## Legal

The porting code is MIT licensed. The game is not mine and is not included —
see [LICENSE](LICENSE) and [CREDITS.md](CREDITS.md).

This exists to keep a purchased copy of a delisted 2012 game playable on
hardware that dropped the instruction set it was built for. It ships no game
code or assets — the one screenshot above is of the port running, included as
evidence that it does — defeats no DRM (the shipped build has none active), and
is of no use to anyone without their own copy of the game.

<div align="center">

---

Original game © 2012 **int13**. All rights reserved.<br>
64-bit port by **AdmiralGallade**.

</div>
