"""make_icon.py -- regenerate the weapon buttons for M and E.

Takes the game's own common/hud/weapons_buttons_thin.tga (pull it out of the
pack with UE_LoadFile in the desktop harness; it is not shipped here), erases
the S from the S cell, and draws a new letter with the same weight, alpha ramp
and 2px bevel as the game's H. The S cell has its edges along the bottom,
facing the ship, which suits a cell above the ship; flipped, it suits one below.

    python missile/make_icon.py <weapons_buttons_thin.tga>

writes, next to this file:
    weapon_m.tga       M, bottom slot (flipped)
    weapon_e_up.tga    E, outer slot above S
    weapon_e_down.tga  E, outer slot below M (flipped), used near the top edge
"""
import os
import struct
import sys

import numpy as np
from PIL import Image, ImageDraw

HERE = os.path.dirname(os.path.abspath(__file__))
FW, FH = 102, 88          # one frame; the sheet adds a green column and row
SS = 8                    # supersampling for the glyph polygons


def glyph_mask(fr, box):
    x0, y0, x1, y1 = box
    m = np.zeros(fr.shape[:2], bool)
    m[y0:y1, x0:x1] = fr[y0:y1, x0:x1, 3] > 100
    g = m.copy()
    for dy in (-1, 0, 1):
        for dx in (-1, 0, 1):
            g |= np.roll(np.roll(m, dy, 0), dx, 1)
    return g


def bbox(m):
    ys, xs = np.where(m)
    return xs.min(), ys.min(), xs.max(), ys.max()


def blank_cell(sheet):
    """The S cell with the S painted out, row by row, from its neighbours."""
    S = sheet[:FH, 0:FW].copy()
    mS = glyph_mask(S, (39, 25, 70, 74))
    for y in range(FH):
        xs = np.where(mS[y])[0]
        if not len(xs):
            continue
        l, r = max(xs.min() - 1, 0), min(xs.max() + 1, FW - 1)
        for c in range(4):
            S[y, xs, c] = np.interp(xs, [l, r], [S[y, l, c], S[y, r, c]])
    return S, bbox(mS)


def letter_m(L, R, T, B, b):
    mid, vy = (L + R) / 2, T + (B - T) * 0.62
    return [(L, T), (L + b, T), (mid, vy - b * 1.15), (R - b, T), (R, T),
            (R, B), (R - b, B), (R - b, T + b * 1.9), (mid, vy + b * 0.2),
            (L + b, T + b * 1.9), (L + b, B), (L, B)]


def letter_e(L, R, T, B, b):
    mid = (T + B) / 2
    arm = R - (R - L) * 0.12          # the middle arm is a little shorter
    return [(L, T), (R, T), (R, T + b), (L + b, T + b),
            (L + b, mid - b / 2), (arm, mid - b / 2), (arm, mid + b / 2),
            (L + b, mid + b / 2), (L + b, B - b), (R, B - b), (R, B), (L, B)]


def draw(cell, glyph_box, shape, width_ratio):
    """Composite a letter over `cell`, in the style of the game's H."""
    x0, y0, x1, y1 = glyph_box
    gh = 41                             # H is 44 tall, less its bevel
    gw = int(round(gh * width_ratio))
    bar = 8
    cx, top = (x0 + x1) / 2, (y0 + y1) / 2 - gh / 2
    out = cell.copy()

    def mask(dx, dy):
        im = Image.new("L", (FW * SS, FH * SS), 0)
        L, R = (cx - gw / 2 + dx) * SS, (cx + gw / 2 + dx) * SS
        T, B = (top + dy) * SS, (top + gh + dy) * SS
        ImageDraw.Draw(im).polygon(shape(L, R, T, B, bar * SS), fill=255)
        return np.array(im.resize((FW, FH), Image.LANCZOS)).astype(float) / 255

    def over(rgb, a):
        da = out[..., 3] / 255.0
        na = a + da * (1 - a)
        for c in range(3):
            num = rgb * a + out[..., c] * da * (1 - a)
            out[..., c] = np.where(na > 1e-6, num / np.maximum(na, 1e-6),
                                   out[..., c])
        out[..., 3] = na * 255

    ys = np.arange(FH)[:, None] * np.ones((1, FW))
    ramp = np.clip((163 + (ys - top) * 1.25) / 255, 0, 1)
    for dx, dy, col, k in [(2, 2, 128, 0.55), (1, 1, 118, 0.95)]:
        over(col, mask(dx, dy) * ramp * k)
    over(255, mask(0, 0) * ramp)
    return out


def write_tga(frame, path):
    """One frame, then a green separator column and row, as the game's are."""
    fr = np.clip(frame, 0, 255).astype(np.uint8)
    sheet = np.zeros((FH + 1, FW + 1, 4), np.uint8)
    sheet[...] = [0, 255, 0, 255]
    sheet[:FH, :FW] = fr
    hdr = struct.pack("<BBBHHBHHHHBB", 0, 0, 2, 0, 0, 0, 0, 0,
                      FW + 1, FH + 1, 32, 0x20)
    with open(path, "wb") as f:
        f.write(hdr + sheet[..., [2, 1, 0, 3]].tobytes())


def main():
    src = sys.argv[1] if len(sys.argv) > 1 else "weapons_buttons_thin.tga"
    sheet = np.array(Image.open(src).convert("RGBA")).astype(float)
    cell, box = blank_cell(sheet)
    x0, y0, x1, y1 = box
    flipped = cell[::-1].copy()
    fbox = (x0, FH - 1 - y1, x1, FH - 1 - y0)
    write_tga(draw(flipped, fbox, letter_m, 0.80),
              os.path.join(HERE, "weapon_m.tga"))
    write_tga(draw(cell, box, letter_e, 0.62),
              os.path.join(HERE, "weapon_e_up.tga"))
    write_tga(draw(flipped, fbox, letter_e, 0.62),
              os.path.join(HERE, "weapon_e_down.tga"))


if __name__ == "__main__":
    main()
