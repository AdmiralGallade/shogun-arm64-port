> **Historical note.** This is the *first* attempt: repackage the original
> 32-bit APK so a device would accept it. It does not work on a 64-bit-only
> phone -- no repackaging can, because the hardware cannot execute ARM32 at all.
> It is kept because the analysis in it is still correct and still useful, and
> because it is the fastest way to run the game on an emulator or an older
> device. The port that actually runs on modern hardware is the rest of this
> repository.

# Shogun v1.2.4 — patched rebuild

## Files
| File | What it is |
|---|---|
| `build.py` output | The rebuilt APK, written to `out/`. Neither it nor the original is in this repository — supply your own copy of the game. |
| `out/shogun-resign-key.pem` | RSA-2048 key + self-signed cert, generated on first run. **Keep it and never commit it**: you need the *same* key to sign any future rebuild, or upgrades will be rejected. It is gitignored. |
| `build.py` | Repack + patch + sign pipeline. Pure Python, no JDK/Android SDK needed. |
| `verify.py` | Independent verifier — re-derives the v1 and v2 signatures from the finished file. |
| `halelf.py` | Small ELF/symbol/disassembly helper used by both. |

## What changed vs the original
1. `lib/armeabi/` → `lib/armeabi-v7a/`. The binary's `.ARM.attributes` declares
   ARMv7-A, so the original folder name was simply wrong; modern ABI lists no
   longer contain bare `armeabi`.
2. Two 4-byte native patches (8 bytes total, nothing else in the library moved):
   - `HAL_CanMakePurchases` @0x5e23c → `movs r0,#0 ; bx lr`
   - `HAL_PurchaseItem`     @0x5e158 → `movs r0,#0 ; bx lr`
   Both previously called into Java to reach the retired Google Play Billing v1
   `IMarketBillingService`. They now report "no store available", which is the
   path the game already took on any device without Market installed.
3. Re-signed with a fresh key: JAR v1 **and** APK Signature Scheme v2.
4. `res/raw/data.mp3` kept STORED and 4-byte aligned so `openRawResourceFd`
   still returns a mappable descriptor. It is byte-identical to the original.

`AndroidManifest.xml`, `classes.dex` and all assets are otherwise untouched.

## Note on the "billing patch"
The developers had already compiled the entitlement system out of this build.
Verified in the shipped binary:

    DRM_CheckLicence        -> mov r0,#1 ; bx lr      (always licensed)
    DRM_RequestLicence      -> mov r0,#1 ; bx lr
    DRM_Init                -> mov r0,#1 ; bx lr
    areAllLevelsUnlocked    -> movs r0,#1 ; bx lr     (always true)
    UnlockAll               -> bx lr                  (nothing to do)
    UnlockLevel             -> bx lr                  (nothing to do)
    LockLevel               -> bx lr                  (nothing can be locked)
    SetLevelTimeout         -> bx lr
    UE_ForcePurchaseResponse-> bx lr

So no content was ever gated in this APK. The patches above are hardening —
they stop the game touching a billing service that no longer exists — not an
unlock.

## Installing
`targetSdk` was deliberately left at 7. Raising it to 24+ would opt the app
into runtime permissions, and it would then fail to write its save data
because it never asks for them.

Emulator (recommended — API 30 x86_64 image includes ARM translation):

    adb install Shogun-1.2.4-patched-armeabi-v7a-signed.apk

Android 15/16, which refuses targetSdk < 24:

    adb install --bypass-low-target-sdk-block Shogun-1.2.4-patched-armeabi-v7a-signed.apk

This will NOT run natively on a Pixel 10 — that hardware has no 32-bit ARM
execution state. See the teardown report for the routes that would.

## Rebuilding
    python build.py path/to/Shogun-1.2.4.apk && python verify.py

`$SHOGUN_APK` and `$SHOGUN_OUT` override the input and output locations.

Edit `PATCHES` in `build.py` to change which functions get stubbed.
