#!/usr/bin/env python3
"""
patch_changelog.py -- rewrite the in-game "What's New" panel.

The changelog lives as plain NUL-terminated strings in the library's .rodata,
not inside the encrypted asset pack, so it can be edited in place. Each slot is
padded to a 4-byte boundary; a replacement may be shorter than the original but
never longer, so every line below is length-checked before anything is written.

    python tools/patch_changelog.py <libHAL.Android.so> [-o out.so]

Operates on a copy of the library you supply yourself; nothing here ships it.
"""
import argparse
import shutil
import sys

# Original text -> replacement. The engine renders these as consecutive lines
# of the "What's New" panel.
LINES = [
    ("Version 1.2",                        "v1.2 arm64"),
    ("New Control Options:",               "64-bit port by"),
    ("  - Alternative static weapon menu",  "  AdmiralGallade"),
    ("  - Left/right handed",              ""),
    ("New Gameplay Options:",              "What changed:"),
    ("  - Adjustable bottom deadzone",     "  - Runs on 64-bit Android"),
    ("  - Togglable hitbox diplay",        "  - ARM32 is emulated"),
    ("New Audio Settings",                 "  - Leaderboard on"),
    ("Score is now recorded on game over", "  - Cheats menu added"),
    ("IAP restore bug fixed",              "Source + build guide:"),
    ("Various minor bug fixes",            "  see Cheats > GitHub"),
    ("Stay tuned, new playing modes",      "Original game (c) 2012"),
    ("and goodies are coming! ;)",         "int13, rights reserved"),
]


def slot_capacity(blob: bytes, at: int) -> int:
    """Bytes available before the next string begins (excluding the NUL)."""
    end = blob.index(b"\x00", at)
    # consume the alignment padding that follows the terminator
    pad = end
    while pad + 1 < len(blob) and blob[pad + 1] == 0:
        pad += 1
    return pad - at


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("lib", help="path to libHAL.Android.so")
    ap.add_argument("-o", "--out", help="output path (default: patch in place)")
    ap.add_argument("--dry-run", action="store_true")
    args = ap.parse_args()

    blob = bytearray(open(args.lib, "rb").read())
    changed = failed = 0

    for old, new in LINES:
        at = blob.find(old.encode())
        if at < 0:
            print(f"  MISS  {old!r} not found"); failed += 1; continue
        cap = slot_capacity(bytes(blob), at)
        data = new.encode()
        if len(data) > cap:
            print(f"  TOO LONG  {new!r} needs {len(data)} > {cap} available")
            failed += 1
            continue
        blob[at:at + cap + 1] = data + b"\x00" * (cap + 1 - len(data))
        print(f"  0x{at:06x}  {cap:>2}B  {old!r}\n           -> {new!r}")
        changed += 1

    if failed:
        print(f"\n{failed} line(s) could not be patched", file=sys.stderr)
        return 1
    if args.dry_run:
        print(f"\ndry run: {changed} line(s) would change")
        return 0

    out = args.out or args.lib
    if out != args.lib:
        shutil.copyfile(args.lib, out)
    open(out, "wb").write(bytes(blob))
    print(f"\npatched {changed} line(s) -> {out}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
