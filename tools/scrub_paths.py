#!/usr/bin/env python3
"""scrub_paths.py -- keep the builder's home directory out of shipped native libraries.

Run on already-stripped .so files (build-apk.sh does both). Stripping removes the
debug info, but __FILE__ strings in assert messages survive it, e.g.
"C:/Users/<name>/.../unicorn-2.1.3/qemu/tcg/tcg.c". Each private prefix (up to
the source tree's own folder) is overwritten in place with '/' characters of the
same length, so no string moves and every pointer still lands where it did.

    python tools/scrub_paths.py lib1.so [lib2.so ...]

Exits non-zero if any home-directory path is still present afterwards.
"""
import re
import sys

# A home directory on Windows, macOS or Linux, through to the last path
# component before a source tree we can name.
PREFIX = re.compile(rb"(?:[A-Za-z]:[/\\]Users|/Users|/home)[/\\][^\x00]*?[/\\]"
                    rb"(?=unicorn-[0-9]|android[/\\]app[/\\]src)")
LEAK = re.compile(rb"(?:[A-Za-z]:[/\\]Users|/Users|/home)[/\\][^\x00/\\]+")


def main():
    bad = 0
    for path in sys.argv[1:]:
        blob = bytearray(open(path, "rb").read())
        n = 0
        for m in list(PREFIX.finditer(blob)):
            blob[m.start():m.end()] = b"/" * (m.end() - m.start())
            n += 1
        open(path, "wb").write(bytes(blob))
        left = len(LEAK.findall(blob))
        print("   scrubbed %d path(s) in %s%s" % (n, path, "" if not left else
                                                  ", %d LEFT" % left))
        bad += left
    if bad:
        print("home-directory paths remain; not packaging", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
