# Shogun ARM32 guest runtime — milestones M0–M5

A working ARM32 high-level-emulation runtime for `libHAL.Android.so`, built as
the desktop harness the port plan calls for. It loads the 32-bit library into
an emulated address space, runs its code, and services all 99 of its imports
from the host.

    pip install unicorn capstone keystone-engine
    python run_tests.py

Currently **39/39 checks pass**, and the engine boots: it loads its encrypted
asset pack, runs its game loop, and issues real OpenGL draw calls.

    python boot.py            # bring the engine up and run frames, with tracing
    python render.py 600                       # render, capture PNGs
    python render.py 1800 --window             # watch it live in a window
    python render.py 600 --size 1080x2400      # letterbox to a Pixel panel
    python render.py 600 --wav music.wav       # capture the audio too
    python play.py                             # drive the menu with taps

## Files
| File | Role |
|---|---|
| `guest.py` | Address space, ELF loader, relocations, trap dispatch, guest heap, calling convention, crash symbolication |
| `shims.py` | Host implementations of all 97 trapped imports (58 libc + 41 GLES) |
| `run_tests.py` | Verification suite + performance benchmark |
| `jni.py` | Fake 233-slot JNIEnv in guest memory + mock JVM modelling HalActivity |
| `boot.py` | Brings the engine up the way HalActivity does and runs the game loop |
| `gl.py` | Forwards the guest's GLES 1.x calls to a real GL driver |
| `render.py` | Boots the engine and renders frames to PNG |
| `play.py` | Drives the menu with synthetic taps |
| `halelf.py` | Standalone ELF/symbol/disassembly helper |
| `assets/data.mp3` | The encrypted `PAK!` asset pack, extracted from the APK |
| `lib/libHAL.Android.so` | The unmodified 2012 library under test |

## Guest address map
    0x10000000  image        1232 KB, mapped from PT_LOAD
    0x40000000  heap         256 MB, first-fit allocator
    0x70000000  stack top    8 MB, grows down
    0x7E000000  data symbols __sF, __stack_chk_guard, errno, scratch
    0x7F000000  trap page    one 4-byte slot per imported function
    0x7FFF0000  return magic emu_start stops here

## What is verified
* All 754 relocations applied; no unresolved symbols.
* Both instruction sets: ARM and Thumb entry points return correct values.
* `UE_GetCRC16("123456789")` = **0x29B1**, the canonical CRC-16/CCITT-FALSE
  check vector — the emulated CPU is byte-exact over a real memory loop.
* `__udivsi3`, `__absvsi2`, `UE_Clamp` match plain arithmetic.
* Soft-float ABI: `UE_ATanHP(0.5)` calls out to the host `atan` shim across an
  ARM/Thumb switch and returns libm's value to the last bit.
* All 97 imports have shims.

## The engine boots
Driven in the order a device would: `JNI_OnLoad` -> `onInit` -> `onArchiveInit`
(with a real file descriptor onto the pack) -> `onTick`.

* **900 consecutive frames** with no fault.
* **116,958 `glDrawArrays` calls** — about 166 per frame once the scene is up,
  which is what a bullet-hell shooter should look like.
* **19,076,021 bytes** pulled through `fread` for a 19,042,949-byte pack: the
  whole archive is read and decrypted internally.
* 20 textures created, 1,235 texture uploads.
* Touch press/move/release accepted.
* Zero unimplemented JNI slots, zero unimplemented imports.

The engine calls back into Java exactly as expected — `initAudio(II)Z` during
`onInit`, reached through `GetMethodID` + `CallBooleanMethodV` + `ExceptionCheck`.

## It renders
`gl.py` forwards the engine's OpenGL ES 1.x calls to a desktop GL driver.
600 frames, 67,002 draw calls, 20 textures, **33.6 fps** with a CPython host
in the loop. The title screen, the v1.2 changelog dialog, the main menu and
the story intro all draw correctly, and synthetic taps navigate them.

Three translations are needed at that boundary, and all three are load-bearing:

1. **Soft-float arguments.** A `GLfloat` arrives as raw IEEE-754 bits in an
   integer register, so every float parameter is reinterpreted, not read.
2. **Fixed point.** The engine uses the GLES `x` entry points (`glOrthox`,
   `glLoadMatrixx`, `glMultMatrixx`) with 16.16 values. Desktop GL has no such
   calls, so they are converted and forwarded to the float equivalents.
3. **Client arrays live in guest memory.** `glVertexPointer` is called *once*
   for the whole run, with the engine rewriting that buffer between draws, so
   vertex and texture-coordinate data must be copied out at every draw.

## Scaling
The game is authored for a 480x800 portrait panel. Rather than lie to it about
the resolution, it renders into an offscreen buffer at its native size which is
then scaled up with aspect ratio preserved — 2.25x into 1080x2400, with 300px
bars top and bottom. Compositing happens in a second offscreen buffer, not the
window: a window manager clamps a window to the physical display, so asking for
a 2400px-tall phone panel on a shorter monitor silently returns a smaller
buffer and crops the capture.

## Audio
Working. The engine mixes in software and hands back 16-bit mono PCM at
22050 Hz in 512-sample buffers; `frames/shogun-menu-music.wav` is 8 seconds of
the menu theme, decoded by the emulated MP3 codec (peak 14358, RMS 3522).

Two bugs had to be fixed, both mine:

* **`initAudio(II)Z` is `(bufferSize, sampleRateHz)`** — in that order. I had it
  backwards, so the mixer got a buffer of the wrong length and walked off the
  end of its stream table, arriving at a null codec pointer. The crash looked
  like an engine defect and was not.
* **`GetPrimitiveArrayCritical` must reuse its staging buffer.** The audio
  thread asks for the same array every frame; bump-allocating per call exhausts
  the JNI arena within seconds of playback.

## Two findings worth keeping
**1. VFP must be enabled even though the ABI is soft-float.** ARM cores come out
of reset with the FPU off and Unicorn is faithful about it. libgcc's own
double-precision helpers still contain VFP instructions, so those paths fault as
`UC_ERR_INSN_INVALID` until cp10/cp11 access is granted and `FPEXC.EN` is set.

**2. A trap cannot be serviced in place across an instruction-set switch.**
Unicorn fixes ARM-vs-Thumb when it *translates* a block, so writing PC and CPSR
from inside a code hook cannot change mode — execution resumes in the old one
and decodes garbage. The trap must `emu_stop()` and be resumed by the caller.
`guest.py` keeps a same-mode fast path and falls back to stop/restart only when
the mode actually changes.

## Measured performance
| Metric | Value |
|---|---|
| Sustained emulation | **~2000 MIPS** (single core, CPython host) |
| Original hardware budget | ~600 MIPS (600 MHz ARM11 class) |
| Headroom | **~3.4x** |
| Trap round trip | ~12 us — but see below |

The ~12 us trap cost is **almost entirely CPython**, not Unicorn: bypassing the
`emu_stop`/`emu_start` round trip via the same-mode fast path saves only 9%, so
the remaining ~11 us is the hook callback and the Python shim body. A C++ host
pays neither. What this harness establishes is that the *CPU core* is fast
enough; the trap path has to be re-measured in C++ before trusting it.

## A third finding, from the game loop
Failure conventions are not uniform and it matters. `gethostbyname` returns a
*pointer*, so it must report `NULL` — stubbing it to `-1` like the socket calls
made the engine treat `0xffffffff` as a valid `struct hostent*` and dereference
it. That crash only appeared 169 frames in, inside `UE_ResolveHostAddress`.

## Not done yet
* No C++ exception has been forced through the emulated unwinder.
* Audio and video are not synchronised to a clock — the harness pumps both
  from one loop. Real playback needs the audio thread driving at its own rate.
* Gameplay past the story intro is unexplored; only the menus and the intro
  have been driven.
* **No Android project yet: this is still the desktop harness.** Everything
  here is platform-independent Python; the on-device runtime is a C++ rewrite
  of the same design.
