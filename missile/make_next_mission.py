"""make_next_mission.py -- the ending's NEXT MISSION button (worlds.c).

    python missile/make_next_mission.py      # writes next_mission.tga next to this file

After Mission 4 the game plays its ending and offers two buttons, "Back to
Menu" (common/hud/menu_menu.tga) and "Continue to Hell Mode"
(common/debriefing/button_hell_mode.tga). This is a third in their style,
drawn from scratch to their measurements: 440 x 62, a chamfered frame (a
faint outer line, a gap, a solid cyan rim, a translucent teal fill), and
the label in pale cyan bold italic with a dark teal outline, its capitals
~25 px tall. build.py's ICONS carries it into both runtimes' files
directory, where worlds.c loads it as "*next_mission.tga".
"""
import os
import sys

import numpy as np
from PIL import Image, ImageDraw

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
sys.path.insert(0, os.path.join(ROOT, "beach"))
import art_menu                  # noqa: E402  (beach/: the bold italic font)
from formats import write_tga    # noqa: E402

W, H = 440, 62
S = 4                            # supersampling
CYAN = (0, 255, 255)
FILL = (0, 181, 181, 153)
TEXT_TOP, TEXT_BOTTOM = (189, 255, 255), (140, 250, 250)
OUTLINE = (0, 119, 119, 255)
LABEL = "Next Mission"


def octagon(inset, cut):
    """The frame's outline, `inset` px in from the edge, corners cut `cut` px."""
    a, b, c, d = inset, inset, W - 1 - inset, H - 1 - inset
    k = cut
    return [(x * S, y * S) for x, y in ((a + k, b), (c - k, b), (c, b + k), (c, d - k), (c - k, d), (a + k, d),
                                        (a, d - k), (a, b + k))]


def frame():
    img = Image.new("RGBA", (W * S, H * S), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    d.polygon(octagon(0.5, 11), outline=CYAN + (128,), width=int(2.5 * S))
    d.polygon(octagon(4.5, 9), fill=FILL, outline=CYAN + (255,), width=2 * S)
    return img


def label():
    f = art_menu._font(38 * S)
    box = f.getbbox(LABEL)
    tw, th = box[2] - box[0], box[3] - box[1]
    x = (W * S - tw) // 2 - box[0]
    y = (H * S - th) // 2 - box[1] - S
    # the outline: the label drawn fat in dark teal, a little low (its shadow)
    out = Image.new("RGBA", (W * S, H * S), (0, 0, 0, 0))
    ImageDraw.Draw(out).text((x, y + S), LABEL, font=f, fill=OUTLINE, stroke_width=int(3.0 * S), stroke_fill=OUTLINE)
    # the face: a mask (thickened to the game's weight), filled with a top-to-bottom ramp
    mask = Image.new("L", (W * S, H * S), 0)
    ImageDraw.Draw(mask).text((x, y), LABEL, font=f, fill=255, stroke_width=int(0.9 * S), stroke_fill=255)
    t = np.linspace(0, 1, H * S)[:, None, None]
    ramp = (np.array(TEXT_TOP) * (1 - t) + np.array(TEXT_BOTTOM) * t).repeat(W * S, axis=1)
    face = Image.fromarray(np.dstack([ramp.astype(np.uint8), np.asarray(mask)]), "RGBA")
    out.alpha_composite(face)
    return out


def button():
    img = frame()
    img.alpha_composite(label())
    return img.resize((W, H), Image.LANCZOS)


def main():
    path = os.path.join(HERE, "next_mission.tga")
    write_tga(path, button())
    print("wrote", path)


if __name__ == "__main__":
    main()
