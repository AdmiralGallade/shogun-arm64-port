# Porting Shogun to 64-bit Android

Notes from making a 2012 ARMv7 game run on hardware that has no 32-bit ARM
execution state. Written down because most of it was learned the expensive way.

## The constraint

*Shogun: Rise Of The Renegade* ships as one native library, `libHAL.Android.so`,
built for ARMv7-A. The Java side is a thin shell: an `Activity`, a
`GLSurfaceView`, an `AudioTrack`. All the game is in the binary. There is no
source, int13 is gone, and the binary is 1.9 MB of ARM32 code with 3,816
symbols.

On a Pixel 10:

```
ro.product.cpu.abilist     arm64-v8a
ro.product.cpu.abilist32   (empty)
```

`adb install` of the original APK gives `INSTALL_FAILED_NO_MATCHING_ABIS`, and
that is the end of the repackaging route. The CPU will not execute a single
instruction in that file. Renaming `lib/armeabi/` to `lib/armeabi-v7a/`, raising
`targetSdk`, re-signing — all of it is beside the point.

So: emulate the CPU, and nothing else.

## The shape of the solution

High-level emulation. The guest is the 2012 library and nothing more. Every call
it makes *out of itself* — libc, OpenGL ES, JNI — is caught at the boundary and
serviced by native arm64 code on the host side. Graphics go to the device's real
GLES 1.x driver.

That boundary is where all the value is. Emulating the CPU is a solved problem
(Unicorn, which is QEMU's TCG). Getting 99 imports, a JNI interface and a
fixed-function GL pipeline to behave exactly as a 2012 device did is not.

### Guest address map

```
0x10000000  image        1232 KB, mapped from PT_LOAD
0x40000000  heap         256 MB, first-fit allocator
0x70000000  stack top    8 MB, grows down, sliced per calling thread
0x7C000000  JNI data     the fake JNIEnv function tables
0x7D000000  JNI trap     one slot per JNIEnv entry
0x7E000000  data symbols __sF, __stack_chk_guard, errno, scratch
0x7F000000  import trap  one 4-byte slot per imported function
0x7FFF0000  return magic emu_start stops here
```

### The trap mechanism

The loader applies all 754 relocations. For `R_ARM_JUMP_SLOT` and
`R_ARM_GLOB_DAT` against an imported symbol, the PLT entry is pointed at a
unique address in the trap page rather than at real code. A hook on that page
dispatches to the host shim, which reads arguments straight out of the guest
registers and stack under AAPCS rules, does the work, writes the return value
back, and resumes.

## Findings worth keeping

### 1. VFP must be enabled even though the ABI is soft-float

ARM cores come out of reset with the FPU off, and Unicorn is faithful about it.
The ABI is soft-float, so you would expect never to need the FPU — but libgcc's
own double-precision helpers contain VFP instructions. Those paths fault with
`UC_ERR_INSN_INVALID` until cp10/cp11 access is granted and `FPEXC.EN` is set.

```cpp
uint32_t c1 = 0, fpexc = 0x40000000u;
uc_reg_read(uc_, UC_ARM_REG_C1_C0_2, &c1);
c1 |= (0xFu << 20);                      // full access to cp10 and cp11
uc_reg_write(uc_, UC_ARM_REG_C1_C0_2, &c1);
uc_reg_write(uc_, UC_ARM_REG_FPEXC, &fpexc);
```

### 2. A trap cannot change instruction set from inside the hook, so do not try

Unicorn resolves ARM-vs-Thumb when it *translates* a block, so writing PC and
CPSR from a code hook cannot redirect anything: the translated block carries on
in the old mode and decodes garbage. The first design accepted that and had the
trap `uc_emu_stop()`, to be restarted by the caller, keeping an in-place fast
path for same-mode returns.

Two things were wrong with that, and only counting found either. The fast path
ran **0% of the time**: the library is Thumb-2, so a caller's `LR` has bit 0
set, while a trap page entered at an even address runs in ARM state — the two
can never agree. And the fast path would not have worked if it had been
reached, for the translation reason above.

The fix is to stop fighting the translator. Put a real ARM `BX LR` at every
trap slot: the hook services the import and returns, the stub executes, and
interworking sends control back in whatever state the caller came from, because
bit 0 of `LR` decides. No stop, no restart, no PC writes, and no mode logic at
all.

Worth recording what this did *not* buy. It took the restart rate from 100% to
1% and frame time did not measurably move — the restarts were never the
bottleneck, despite being an obvious suspect. It is kept because it is the right
shape, not because it paid.

### 3. Failure conventions are not uniform, and it matters

`gethostbyname` returns a `struct hostent*`, so a stub must fail with `NULL`.
Stubbing it to `-1` like the socket calls made the engine treat `0xffffffff` as
a valid pointer and dereference it. The crash landed 169 frames later inside
`UE_ResolveHostAddress` and looked nothing like its cause.

### 4. A VBO addresses its data by offset, and offset 0 is normal

The client-array upload skipped any array whose pointer was 0, on the reasoning
that a null pointer means "not set". For a VBO-backed array that field is a byte
*offset*, and 0 is the ordinary case. Every mesh array was skipped, GL kept
pointing at the 2D sprite buffer, and the 3D scene drew as torn geometry. The
test is not "is the pointer null" but "is this a client array with a null
pointer":

```cpp
if (!a.vbo && !a.ptr) continue;
```

### 5. Names handed to the guest must be allocated monotonically

`glGenTextures` returning `map.size() + 1` collides with names inserted on
demand elsewhere, so a later `Gen` can reissue a live name and two meshes end up
sharing one buffer. Geometry appears for one frame and is then overwritten.

### 6. Anything called per frame must not take a JNI global reference

Wrapping the activity fresh on every tick overflowed the global reference table
— 50,688 references to one object — and killed the process. The same mistake in
the audio path allocated a new `GetPrimitiveArrayCritical` staging buffer every
frame until the JNI arena was exhausted, which is why sound worked for about a
second and then stopped. Recurring objects go through a small fixed set of
cached slots.

### 7. The engine pastes paths together without a separator

The storage directory handed to `onInit` is concatenated directly with a file
name. `.../files` + `settings.sav` becomes `.../filessettings.sav`. On a 2012
device the string arrived with a trailing slash, so the engine never needed one
of its own. Without it every save silently fails to open — the game runs
perfectly and forgets everything.

Compounding it, the path shim stripped the leading `/` from what it was given
and re-prefixed the app's private root, turning an already-absolute path into
`<root>/<root>/<name>`. Two independent bugs producing one symptom.

The lesson is narrower than "check your paths": a shim that *silently* returns a
failure code the caller ignores is invisible. Logging every failed `fopen` found
both bugs in one run, after an afternoon of guessing.

### 8. `onApplicationPause` is the only shutdown hook there is

Nothing else in the engine writes the save file. If the app closes without that
call reaching the guest, the session is gone. It has to run on the thread that
owns the emulated CPU, and before `GLSurfaceView` stops that thread.

### 9. The EGL context is not yours to keep

`GLSurfaceView` destroys it in `onPause`. `setPreserveEGLContextOnPause(true)`
asks for it back, but that is a request the driver may refuse — and the engine
is never told either way. It keeps drawing with texture and buffer names that
now refer to nothing, and the game returns from a minimise untextured.

The fallback is to keep a CPU-side copy of every texture upload, every buffer,
and every enable, and replay them into fresh names when the surface is
recreated. The guest's names never change; only the real GL names behind them
do. Per-material state (blend, texture environment, matrices) is deliberately
not replayed, because the engine flushes its own render state every frame.

### 10. A frame pacer has to land on the vsync grid, not on a wall clock

Sleeping to hold 60 fps and then letting `eglSwapBuffers` present is only
coherent when the frame period is a whole number of vsyncs. On a 90 Hz panel
16.667 ms is one and a half of them, so each frame was shown either 11.1 ms or
22.2 ms — 3:2 judder at a perfectly correct 60 fps *average*. It reads to a
player as the game being slower, not as a stutter, and no frame-rate number
anywhere in the log looks wrong.

Pace to a whole multiple of the real refresh interval, and ask for a refresh
rate that divides into the content. `Surface.setFrameRate` is only a vote and
was granted on one launch and refused on the next; `preferredDisplayModeId`
names the mode and sticks.

### 11. Serialised guest entry needs a *fair* lock

One `uc_engine` is not thread-safe, so the render thread and the audio thread
have to take turns. A plain `std::mutex` is not FIFO, and these two threads are
not equals — the audio thread runs at `URGENT_AUDIO` and loops tightly, so it
kept re-acquiring the lock ahead of a render thread that was already waiting.
Frames that wanted 16 ms of emulation spent up to **98 ms queueing**.

A ticket lock bounds it: each side waits at most one turn. Relatedly, do not
let the audio thread generate several buffers in a row — four chunks back to
back held the emulator for ~22 ms and locked rendering out every fourth frame.

### 12. The scheduler will not guess that an emulator is urgent

This one cost the most to find and is the most transferable. On a big.LITTLE
phone the emulator threads were migrating across all three clusters — including
the little cores — and landing on cores sitting at 553 MHz, against a prime
core that can run at 2850. Time-weighted, the big cores spent 63% of their time
at the 500 MHz floor and **never once exceeded 1426 MHz**.

Nothing was broken. The render thread uses only ~36% CPU, because it blocks on
vsync and on the lock above, so the governor correctly sees a small task. That
is a self-sustaining loop: the low clock makes frames slow, the thread still
blocks, so it is never boosted.

Two things fix it. Pin the threads to the fastest cluster, discovered by
reading `cpuinfo_max_freq` rather than hardcoding core numbers. Then use
`PerformanceHintManager` to state the deadline and report each frame against
it. Both are needed; pinning alone leaves the cluster downclocked.

Two ways that hint can silently do nothing, both of which happened here:
`GLSurfaceView` builds a new GL thread whenever the surface returns, so a
session created once names a thread that no longer exists; and the target has
to be *maintained*, because a session created while the panel was still at
90 Hz carried a 22 ms deadline, against which a 16 ms frame looks comfortable.

The lesson generalises past this project: any emulator or interpreter that
blocks on a frame boundary looks small to a scheduler, and will be clocked as
if it were.

## Performance

The desktop harness answered the only question that mattered up front — whether
the CPU core is fast enough:

| Metric | Value |
|---|---|
| Sustained emulation | ~1650–2000 MIPS (single core, CPython host) |
| Original hardware budget | ~600 MIPS (600 MHz ARM11 class) |
| Trap round trip, CPython | ~12 µs — almost entirely Python, not Unicorn |
| Trap round trip, C on x86-64 | 274 ns (mode switch), 136 ns (in place) |

On the device, measured in the running game:

| Metric | Value |
|---|---|
| Frame cost | 5.8–12.9 ms against a 16.7 ms budget |
| Audio callback | ~3 ms per 512-sample buffer |
| Audio callbacks per 120 ticks | 84–88, correct for 22050 Hz |

The engine issues roughly 645 GL calls per frame and every one is a trap, so
trap cost is multiplied by 645 before it is spent. That is why it was measured
before any of the rest was built.

### Measuring a frame, and why averages are useless here

Those figures are a 120-frame mean, which is the wrong instrument for
smoothness and hid every problem in findings 10 to 12 — a mean of a correct
60 fps is exactly what 3:2 judder looks like. The runtime therefore also
reports, per window: the *worst* frame, a count of frames over 33 ms, and any
gap past half a second; and for each individual bad frame, a breakdown into
guest game logic, guest audio, GL and lock queueing. Guest and host time are
kept apart, because a shim runs inside the emulator's own run window and would
otherwise be charged to the guest.

That breakdown is what isolated the contention and the clocking. Seven
plausible explanations died against it first: audio cost, trap restarts, asset
unpacking, the logging itself, Mali depth-buffer tile loads, refresh-rate
flapping, and the belief that the device was simply short on emulation
throughput. The last was the most expensive to hold and the most wrong — the
same work that took 22–30 ms a frame at 500 MHz takes 16.3 ms at full clock,
and audio that appeared to cost 6–12 ms a frame actually costs 1.2.

Two numbers worth not rediscovering. `glClear` is by far the most expensive GL
entry point, about 2.7 ms a frame, and none of it is work: it is the first call
of a frame, so EGL parks the thread there waiting for a free back buffer and it
absorbs the vsync wait that `eglSwapBuffers` appears not to take. Excluding it,
the whole GL layer costs roughly 0.4 ms a frame. And a tick timed from outside
the lock charges one thread for the other thread's work, which is why the split
exists at all.

A 90 Hz Pixel 7 on Mali now holds 58–60 fps with 4–8 frames per 120 over
33 ms. Before findings 10 to 12 the same screen ran at 38–43 fps with worst
frames of 135–182 ms.

## What is still open

- Gameplay past the early missions is lightly tested.
- No C++ exception has been forced through the emulated unwinder.
- The billing path is stubbed, not reimplemented. The service is gone.
- The trap cost on arm64 was estimated by scaling the measured C figure with a
  CPU-ratio benchmark (292–545 ns), not measured directly with `bench/`. The
  frame timings above supersede it in practice, but the direct number would
  still be worth having.
- One tick costing about 65 ms recurs roughly every ten seconds. It is almost
  entirely emulation rather than contention, so it is real game work — likely
  a wave or event. Mild beside what preceded it, but unexplained.
- Nothing has been tried to make the audio path cheaper at source. The engine
  decodes MP3 with libmad in emulated code, and `MPEG_DecodeFrame`,
  `mad_frame_decode` and `mad_synth_frame` are all exported, so a host decoder
  could stand in. At 1.2 ms a frame it is not worth doing — which is only true
  because of finding 12.
