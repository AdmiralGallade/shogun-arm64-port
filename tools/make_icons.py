#!/usr/bin/env python3
"""
make_icons.py -- build launcher icons from the ones inside your own APK.

The original ships 36x36, 48x48 and 96x96 bitmaps. A modern launcher treats a
legacy bitmap that small as untrustworthy: it shrinks it and drops it inside a
white circle, which looks like a broken app rather than a 2012 one. An adaptive
icon is rendered at the launcher's own size and masked deliberately, so the
artwork fills the shape it is given.

So: lift the 96x96, cut off its baked-in rounded border (the launcher draws its
own shape now), upscale it into the 66% safe zone of a 432x432 foreground, and
pair it with a background colour sampled from the art itself.

    python tools/make_icons.py <Shogun.apk> <res-dir>

Writes only generated files; none of them are tracked by git, because the
artwork is int13's.
"""
import argparse
import io
import os
import sys
import zipfile

try:
    from PIL import Image
except ImportError:
    sys.exit("Pillow is required for icon generation: pip install Pillow\n"
             "(or run extract_assets.py --no-icons to skip it)")

# res/drawable-<d>/icon.png in the APK -> the density bucket we emit for
SOURCES = [
    ("res/drawable-hdpi/icon.png", "hdpi"),
    ("res/drawable-mdpi/icon.png", "mdpi"),
    ("res/drawable/icon.png",      "ldpi"),
]

# Adaptive icons are 108dp square with only the middle 72dp guaranteed visible.
# At 4x that is a 432px canvas with a 288px safe zone.
CANVAS = 432
SAFE = 288
# The art fills exactly the safe zone. Every launcher mask -- circle, squircle,
# rounded square -- is inscribed in that square, so it lands entirely on artwork
# and the background colour never actually shows. Drawing the art any larger
# just zooms in and starts cutting the ship's wings off.
ART = SAFE
# The source art sits inside a drawn frame with a few pixels of rounded border.
# The launcher applies its own mask, so that border is now just noise.
BORDER_FRACTION = 0.10


def load_best(apk: zipfile.ZipFile) -> Image.Image:
    for name, _ in SOURCES:
        if name in apk.namelist():
            return Image.open(io.BytesIO(apk.read(name))).convert("RGBA")
    raise SystemExit("no res/drawable*/icon.png in that APK")


def background_colour(img: Image.Image) -> str:
    """Darkest common colour in the art, which is its backdrop rather than the
    ship. Sampling the mean instead would wash out to grey."""
    small = img.convert("RGB").resize((32, 32), Image.LANCZOS)
    counts = {}
    for n, px in small.getcolors(32 * 32) or []:
        # quantise so near-identical shades count together
        key = (px[0] // 16, px[1] // 16, px[2] // 16)
        counts[key] = counts.get(key, 0) + n
    # among the most common shades, prefer a dark one: this art is a ship on a
    # near-black hex field, and the field is what should become the background
    common = sorted(counts.items(), key=lambda kv: -kv[1])[:6]
    key = min(common, key=lambda kv: sum(kv[0]))[0]
    r, g, b = (c * 16 + 8 for c in key)
    return "#%02X%02X%02X" % (r, g, b)


def build_foreground(img: Image.Image) -> Image.Image:
    w, h = img.size
    cut = int(min(w, h) * BORDER_FRACTION)
    art = img.crop((cut, cut, w - cut, h - cut))
    art = art.resize((ART, ART), Image.LANCZOS)
    fg = Image.new("RGBA", (CANVAS, CANVAS), (0, 0, 0, 0))
    off = (CANVAS - ART) // 2
    fg.paste(art, (off, off), art)
    return fg


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("apk")
    ap.add_argument("res", help="the app's res/ directory")
    args = ap.parse_args()

    with zipfile.ZipFile(args.apk) as z:
        src = load_best(z)
        legacy = {d: z.read(n) for n, d in SOURCES if n in z.namelist()}

    print("source icon: %dx%d" % src.size)

    def out(*parts):
        p = os.path.join(args.res, *parts)
        os.makedirs(os.path.dirname(p), exist_ok=True)
        return p

    # 1. adaptive foreground, one density-independent copy
    fg = build_foreground(src)
    p = out("drawable-nodpi", "ic_launcher_fg.png")
    fg.save(p)
    print("  ->", os.path.relpath(p, args.res), "%dx%d" % fg.size)

    # 2. the legacy bitmaps, for anything that ignores the adaptive icon
    for dens, blob in legacy.items():
        sub = "mipmap-%s" % dens if dens != "ldpi" else "mipmap-ldpi"
        p = out(sub, "ic_launcher.png")
        open(p, "wb").write(blob)
        print("  ->", os.path.relpath(p, args.res))

    # 3. the background colour, sampled from the art
    bg = background_colour(src)
    p = out("values", "ic_launcher_bg.xml")
    open(p, "w", encoding="utf-8", newline="\n").write(
        '<?xml version="1.0" encoding="utf-8"?>\n'
        "<!-- Sampled from the original icon by tools/make_icons.py. -->\n"
        "<resources>\n"
        '    <color name="ic_launcher_bg">%s</color>\n'
        "</resources>\n" % bg)
    print("  ->", os.path.relpath(p, args.res), "background", bg)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
