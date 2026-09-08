#!/usr/bin/env python3
"""
extract_assets.py -- take the two files the port needs out of your own copy of
the game, and put them where the build expects them.

    python tools/extract_assets.py Shogun-1.2.4.apk

Nothing it writes is tracked by git: this repository ships the port, not the
game. You need a copy of the APK you legally own.

Produces:
    android/app/src/main/assets/libHAL.Android.so   engine, changelog patched
    android/app/src/main/assets/data.mp3            encrypted asset pack
    runtime/lib/libHAL.Android.so                   pristine, for the harness
    runtime/ext/lib/armeabi/libHAL.Android.so       pristine, harness alt path
    runtime/assets/data.mp3                         pack, for the harness
"""
import argparse
import os
import subprocess
import sys
import zipfile

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)

LIB_IN_APK = "lib/armeabi/libHAL.Android.so"
PAK_IN_APK = "res/raw/data.mp3"


def out(*parts):
    p = os.path.join(ROOT, *parts)
    os.makedirs(os.path.dirname(p), exist_ok=True)
    return p


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("apk", nargs="?", default=os.path.join(ROOT, "Shogun-1.2.4.apk"),
                    help="your copy of the Shogun APK")
    ap.add_argument("--no-changelog", action="store_true",
                    help="skip the in-game credit patch")
    args = ap.parse_args()

    if not os.path.exists(args.apk):
        sys.exit("not found: %s\nSupply your own copy of the game -- see the README."
                 % args.apk)

    with zipfile.ZipFile(args.apk) as z:
        names = z.namelist()
        for want in (LIB_IN_APK, PAK_IN_APK):
            if want not in names:
                sys.exit("%s has no %s -- is this Shogun v1.2.4?" % (args.apk, want))
        lib = z.read(LIB_IN_APK)
        pak = z.read(PAK_IN_APK)

    print("engine     %9d bytes" % len(lib))
    print("asset pack %9d bytes" % len(pak))

    # The harness reads the library from either of two paths; write both so it
    # works whichever way you invoke it.
    pristine = [out("runtime", "lib", "libHAL.Android.so"),
                out("runtime", "ext", "lib", "armeabi", "libHAL.Android.so")]
    for p in pristine:
        open(p, "wb").write(lib)
        print("  ->", os.path.relpath(p, ROOT))

    for p in (out("runtime", "assets", "data.mp3"),
              out("android", "app", "src", "main", "assets", "data.mp3")):
        open(p, "wb").write(pak)
        print("  ->", os.path.relpath(p, ROOT))

    # The APK's own copy of the engine carries the port credit; the harness copy
    # stays byte-identical to the original so it can be compared against it.
    apk_lib = out("android", "app", "src", "main", "assets", "libHAL.Android.so")
    if args.no_changelog:
        open(apk_lib, "wb").write(lib)
        print("  ->", os.path.relpath(apk_lib, ROOT), "(unpatched)")
    else:
        src = pristine[0]
        r = subprocess.run([sys.executable, os.path.join(HERE, "patch_changelog.py"),
                            src, "-o", apk_lib])
        if r.returncode != 0:
            sys.exit("changelog patch failed; re-run with --no-changelog to skip it")
        print("  ->", os.path.relpath(apk_lib, ROOT), "(changelog patched)")

    print("\nready. Next: build unicorn, then android/build-apk.sh -- see the README.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
