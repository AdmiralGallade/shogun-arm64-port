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

### 2. A trap cannot be serviced in place across an instruction-set switch

Unicorn resolves ARM-vs-Thumb when it *translates* a block. Writing PC and CPSR
from inside a code hook therefore cannot change mode: execution resumes in the
old one and decodes garbage. The trap has to `uc_emu_stop()` and be restarted by
the caller. Same-mode returns stay on an in-place fast path, which is the common
case and much cheaper.

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

## What is still open

- Gameplay past the early missions is lightly tested.
- No C++ exception has been forced through the emulated unwinder.
- The billing path is stubbed, not reimplemented. The service is gone.
- The trap cost on arm64 was estimated by scaling the measured C figure with a
  CPU-ratio benchmark (292–545 ns), not measured directly with `bench/`. The
  frame timings above supersede it in practice, but the direct number would
  still be worth having.
