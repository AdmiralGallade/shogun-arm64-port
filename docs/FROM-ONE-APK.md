# How this was done, starting from one APK

There was no source, no developer to ask, and no documentation. There was a
single 19 MB file. This is the method that got from there to a running game —
what to look at, in what order, and which findings decided what happened next.

[PORTING.md](PORTING.md) is the list of things that broke. This is how the
work was actually approached.

---

## Step 1 — Read the container before writing anything

An APK is a ZIP. The first useful act is to list it and think about what each
entry implies. Shogun has **16 entries**:

| Entry | Size | What it tells you |
|---|---|---|
| `res/raw/data.mp3` | 19,042,949 | STORED, and 95% of the app. Not an MP3. |
| `lib/armeabi/libHAL.Android.so` | 1,978,997 | The whole game. One ABI: 32-bit ARM. |
| `classes.dex` | 56,460 | Tiny. So Java is a shell, not the game. |
| `AndroidManifest.xml` | 3,716 | Entry point, permissions, target SDK |
| icons, `pspbrwse.jbf` × 3 | small | Paint Shop Pro browser files, shipped by accident |

Two conclusions fell out immediately, before any tooling:

- **The game is native.** 56 KB of dex cannot be a bullet-hell shooter. Whatever
  is in `classes.dex` is glue, and the real work is in the `.so`.
- **`data.mp3` is not audio.** It is stored uncompressed and it is 19 MB. The
  first four bytes settle it:

```
$ magic of res/raw/data.mp3
b'PAK!'
```

An asset archive named `.mp3` so it can live in `res/raw/` and be opened with
`openRawResourceFd`, which hands back a real file descriptor and offset. That
naming trick is itself a hint about how the engine expects to read it.

## Step 2 — Ask the one question that decides everything

**Is the native library stripped?**

This is the difference between a weekend and a month, so it is worth asking
before anything else. Parse the ELF section headers and look for `.symtab`:

```
.dynsym     DYNSYM    size=45904    entries=2869
.symtab     SYMTAB    size=140432   entries=8777
.rel.dyn    REL       size=5264     entries=658
.rel.plt    REL       size=768      entries=96
```

`.symtab` is present, with **8,777 entries**. The library shipped unstripped.
That is not skill, it is luck — but it changes the whole problem. It means:

- Every function has a name: `UE_GetCRC16`, `HAL_StartAudioPlaying`,
  `UE_SavePersistentDataPool`, `Java_net_int13_HalActivity_onTick`.
- C++ names are mangled and therefore carry their argument types:
  `_Z13SaveLevelLockP6SHOGUN` is `SaveLevelLock(SHOGUN*)`.
- A crash can be symbolicated. A guest PC becomes `function + offset` instead
  of a bare address, which is the single most valuable debugging tool in the
  whole project.
- You can call an internal function directly to test a hypothesis.

If it had been stripped, everything below would still work, but every bug would
have been found by disassembly rather than by reading a name.

## Step 3 — Establish the actual wall

The obvious first idea is to repackage: rename `lib/armeabi/` to
`lib/armeabi-v7a/`, bump `targetSdk`, re-sign. It is worth doing, because it
is cheap and because it proves what the real constraint is.

```
ro.product.cpu.abilist     arm64-v8a
ro.product.cpu.abilist32   (empty)
```

`adb install` → `INSTALL_FAILED_NO_MATCHING_ABIS`.

That empty second line is the whole problem. The Pixel has no 32-bit ARM
execution state at all — not a missing library, not a policy check, no
instruction in that file can run. No amount of repackaging fixes it. That
attempt is preserved in [`apk-patch/`](../apk-patch/) because the analysis in
it stayed useful, but as a route to running the game it is dead.

Knowing *precisely* why it is dead is what justifies the expensive option.

## Step 4 — Choose among the real options

| Option | Verdict |
|---|---|
| Repackage the APK | Impossible. No ARM32 execution state. |
| Recompile from source | No source exists. |
| Static recompilation (ARM32 → arm64 ahead of time) | Requires solving indirect branches in 834 KB of `.text`. Months, and brittle. |
| Full-system emulation | Works, but you emulate a whole 2012 Android to get one library to run. Slow and enormous. |
| **High-level emulation** | Emulate the CPU for the library only. Every call it makes *out of itself* is serviced by native 64-bit code. |

HLE wins because of the shape of this specific binary: it is one self-contained
library whose entire outside world is 99 imported symbols. The boundary is
small, well-defined, and enumerable. That is what makes the problem finite.

## Step 5 — Measure the thing that can kill it, before building it

HLE only works if the emulated CPU is fast enough and if crossing the boundary
is cheap. The engine issues roughly **645 GL calls per frame**, and every one
is a boundary crossing, so trap cost gets multiplied by 645 before it is spent
against a 16.7 ms budget.

So that was measured first, in isolation, before any runtime existed
([`bench/`](../bench/)):

| Host | Path | ns/trap |
|---|---|---|
| CPython, x86-64 | mode switch | ~12,000 |
| **C, x86-64** | **in place, same mode** | **136** |
| C, x86-64 | mode switch | 274 |

274 ns × 645 = 0.18 ms/frame, about 1% of the budget. Green light. Had it come
back at 5 µs, the answer would have been to stop.

The other half — raw CPU throughput — came from the emulator running the
engine's own CRC routine: ~1,650–2,000 MIPS against the ~600 MIPS the game was
written for.

## Step 6 — Enumerate the boundary exactly

The import surface is not guesswork. It is every undefined symbol in `.dynsym`:

```
dynsym: 2,866 named, 99 UNDEFINED    <- the entire outside world
  of which GL:  41
  other (libc, math, pthread, socket): 58
```

And the relocations that point at them: **658 + 96 = 754**, all of which the
loader must apply. Four types matter: `R_ARM_RELATIVE`, `R_ARM_ABS32`,
`R_ARM_GLOB_DAT`, `R_ARM_JUMP_SLOT`.

The mechanism follows directly. For every PLT slot belonging to an imported
symbol, write the address of a unique 4-byte cell in a **trap page** instead of
real code. Put a hook on that page. Any call the engine makes to the outside
world now lands in the hook, which reads the arguments out of the guest's
registers under AAPCS rules and runs a native implementation.

99 undefined symbols means a finite, checkable to-do list. When all 99 have
shims and the loader reports no unresolved relocations, the boundary is closed.

## Step 7 — Recover the Java contract from the dex

The engine calls *into* Java as well as out. Those callbacks have to exist with
exactly the right names and signatures, or `GetMethodID` returns null and the
engine dereferences it.

`classes.dex` gives the class list:

```
Lnet/int13/HalActivity;
Lnet/int13/HalActivity$AudioThread;
Lnet/int13/GLJNIView;
Lnet/int13/BillingService;      <- Google Play Billing v1, long dead
Lnet/int13/BillingSecurity;
Lnet/int13/C$ResponseCode;
```

and the method names the native side looks up:

```
initAudio  startAudio  pauseAudio  closeAudio  fillAudio
onInit  onTick  onArchiveInit  onAudioFrame  onApplicationPause
onTouchPressed  onTouchMove  onTouchReleased
initCamera  startCamera  updateCamera  stopCamera
canMakePurchase  purchaseItem  restoreItems  finishActivity  geLang  openURL
```

The signatures come from the other side. `GetMethodID(cls, name, sig)` takes a
literal signature string, and those constants are in the library's `.rodata`:

```
()V   ()Z   (II)V   (II)Z   (Ljava/lang/String;)V
```

Which of those goes with which name is answered by simply *logging what the
fake JNIEnv is asked for*. Once the JNI bridge exists, every `GetMethodID` call
prints its name and signature, and the contract writes itself.

One detail from this step was worth the whole exercise. The class name in the
binary is stored **dot-separated**:

```
net.int13.HalActivity
net.int13.GLJNIView
net.int13.ardefender      <- int13 shipped AR titles on this same engine
```

JNI's `FindClass` wants `net/int13/HalActivity`. Passing the string as stored
fails. That is why the bridge normalises dots to slashes, and why the shell
declares a real `net.int13.HalActivity` for `ShogunActivity` to extend — so the
method IDs resolve against an object that genuinely is one.

The camera callbacks are not used by Shogun at all. They are looked up at init
because the engine is shared with int13's AR titles, so they have to resolve
even though nothing ever calls them.

## Step 8 — Prototype in Python, ship in C++

The whole design was built first as a desktop Python harness
([`runtime/`](../runtime/)), against a desktop GL driver, on a laptop.

This was deliberate, and it is the single decision that saved the most time.
Iterating on a relocation bug takes seconds in Python and a build-install-launch
cycle on a phone. The harness reached 39/39 checks passing — including
`UE_GetCRC16("123456789") == 0x29B1`, the canonical CRC-16/CCITT-FALSE test
vector, which proves the emulated CPU is byte-exact over a real memory loop —
before a single line of the Android app was written.

The C++ runtime is a port of that harness, not a fresh design. When something
misbehaves on the phone, the harness is the reference: run the same thing on
the desktop and see whether it differs.

The one thing the harness could *not* answer was trap cost, because ~12 µs
there is almost entirely CPython overhead. Hence Step 5 being a separate C
benchmark.

## Step 9 — Never break the encryption

`data.mp3` is a `PAK!` container with Blowfish-encrypted entries. It is
tempting to reverse the format and unpack it.

Don't. The engine already contains the decryptor, the LZMA decoder and the MP3
codec, and the engine is the thing being run. Hand it a file descriptor and it
decrypts its own assets. During a full boot the harness observed **19,076,021
bytes** pulled through `fread` for a 19,042,949-byte pack — it reads and
decrypts the entire archive internally.

The corollary is the bug this caused: the pack must look like a file the engine
*owns*. Inside the APK it sits at an offset, and the engine seeks absolutely,
so the LZMA decoder was fed bytes from the wrong place. The fix is to extract it
to app storage once and open it at offset 0 — not to understand the format.

## Step 10 — Debug a program you cannot read

Four techniques did essentially all the work.

**Symbolicate every fault.** A guest crash reports `function + offset`, not an
address. `UE_ResolveHostAddress + 0x2c` tells you what the engine was doing;
`0x10041a2c` tells you nothing.

**Log the boundary, not the program.** You cannot step through the game, but
every single thing it does to the outside world passes through code you wrote.
Counting GL calls per entry point revealed that the 3D path used VBOs at all.
Logging every failed `fopen` found two independent save-path bugs in one run,
after an afternoon of guessing at them.

**Ask the engine about itself.** With symbols, you can call its own accessors
as a probe. `UE_isMusicPlaying`, `UE_GetNbPlayingChannels`, `UE_GetMusicVolume`
answered "is this silence because we lost the PCM, or because the engine never
started any music?" — a question that would otherwise have been guesswork.

**Trust the symptom over the theory.** "It flashed for half a second then went
black" is a precise statement: state is correct at first draw and destroyed
afterwards. That points at name aliasing, not at upload. Several fixes here
were found by taking the exact wording of a symptom seriously.

## What made this possible

In rough order of importance:

1. **The library was shipped unstripped.** Pure luck, and worth more than any
   other single factor.
2. **One self-contained native library** with a 99-symbol outside world. A game
   split across several libraries with a large shared surface would be far
   worse.
3. **Fixed-function OpenGL ES 1.x**, which Android still supports natively. The
   GL layer forwards; it does not translate shaders or emulate a pipeline.
4. **The engine mixes audio in software** and hands back finished PCM. There was
   no audio API to emulate, only a buffer to move.
5. **Modern phones are roughly 3× the machine this targeted**, so a
   ~1,700 MIPS emulated CPU clears a ~600 MIPS budget with room to spare.

## What would make it much harder

- A stripped binary — possible, but every bug becomes a disassembly session.
- GLES 2.0+ with custom shaders — you would be translating a shader pipeline,
  not forwarding calls.
- Audio through OpenSL ES or an emulated codec rather than software mixing.
- Anti-tamper or licence checks that actually run. Shogun has none active: the
  developers had already compiled the entitlement system out, and `DRM_Init`,
  `DRM_CheckLicence` and `areAllLevelsUnlocked` are all `mov r0,#1 ; bx lr` in
  the shipped binary.
- A 60 fps budget with 10× the draw calls, where trap cost stops being 1%.

## If you want to do this to a different game

The order matters more than the tools:

1. Unzip it. Look at what is big and what is stored uncompressed.
2. Check `.symtab`. Decide how expensive this is going to be.
3. Confirm the real constraint with an actual install attempt.
4. Count the undefined symbols. That number is your scope.
5. Benchmark the boundary crossing against the frame budget. Be willing to stop.
6. Read the dex for the callback contract; log the JNI requests for signatures.
7. Build it on a desktop first, where iteration costs seconds.
8. Let the engine handle its own assets, encryption and codecs.
9. Instrument the boundary, and believe the symptom.
