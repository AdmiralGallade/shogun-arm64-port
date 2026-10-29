"""make_upgrades.py -- the art for the Doc's two upgrades (Mission 8): the
plasma sphere (P) and the shield.

    python missile/make_upgrades.py          # writes the .tga files next to this file

Writes, next to this file (build.py's ICONS carries them into both
runtimes' files directory, where missile.c's '*' paths load them):

    weapon_p_up.tga      P, a weapon letter like M and E: the second ring,
    weapon_p_down.tga    above the ship (edges down) or below it (flipped)
    weapon_sh_up.tga     the shield: the Valkyrie's crest, same two cells
    weapon_sh_down.tga
    shield_bubble.tga    the bubble round the ship while the shield is up:
                         4 frames of shimmer, then 1 faint (running out)
    plasma_sphere.tga    the sphere: 4 frames of churning plasma, drawn 28
                         units in radius; missile.c loads it at each power
                         level's size

The buttons are made from the E button (weapon_e_up.tga, itself made from
the game's own sheet by make_icon.py): the E is painted out and the new
glyph drawn with make_icon's weight, alpha ramp and bevel. The bubble is a
small copy of the Valkyrie's hexagon shield (antarctica/art_fleet.py) --
it is her generator. The two sheets keep to 64 px cells and few frames:
they load in every world, into an atlas the worlds already fill.
"""
import math
import os
import struct
import sys

import numpy as np
from PIL import Image, ImageDraw, ImageFilter

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import make_icon as MI          # noqa: E402

FW, FH = MI.FW, MI.FH
SS = MI.SS
GREEN = (0, 255, 0, 255)


# ---- the buttons -----------------------------------------------------------------------
def read_cell(name):
    a = np.array(Image.open(os.path.join(HERE, name)).convert("RGBA")).astype(float)
    return a[:FH, :FW].copy()


def blank_e(cell):
    """The E cell with its E painted out, row by row from either side (as
    make_icon.blank_cell paints out the S). The glyph is grey-white and its
    bevel grey: low saturation, brighter than the blue cell."""
    rgb = cell[..., :3]
    sat = rgb.max(-1) - rgb.min(-1)
    m = (sat < 34) & (rgb.max(-1) > 96) & (cell[..., 3] > 40)
    box = np.zeros_like(m)
    box[22:78, 33:71] = True
    m &= box
    g = m.copy()
    for dy in (-1, 0, 1, 2, 3):
        for dx in (-1, 0, 1, 2, 3):
            g |= np.roll(np.roll(m, dy, 0), dx, 1)
    g &= np.roll(np.roll(box, 2, 0), 2, 1) | box
    out = cell.copy()
    for y in range(FH):
        xs = np.where(g[y])[0]
        if not len(xs):
            continue
        l, r = max(xs.min() - 1, 0), min(xs.max() + 1, FW - 1)
        for c in range(4):
            out[y, xs, c] = np.interp(xs, [l, r], [cell[y, l, c], cell[y, r, c]])
    ys, xs = np.where(m)
    return out, (xs.min(), ys.min(), xs.max(), ys.max())


def draw_glyph(cell, glyph_box, ops, width_ratio):
    """make_icon.draw, for a glyph of several polygons: `ops(L, R, T, B, b)`
    gives [(points, 255 to fill | 0 to cut)]."""
    x0, y0, x1, y1 = glyph_box
    gh = 41
    gw = int(round(gh * width_ratio))
    bar = 8
    cx, top = (x0 + x1) / 2, (y0 + y1) / 2 - gh / 2
    out = cell.copy()

    def mask(dx, dy):
        im = Image.new("L", (FW * SS, FH * SS), 0)
        L, R = (cx - gw / 2 + dx) * SS, (cx + gw / 2 + dx) * SS
        T, B = (top + dy) * SS, (top + gh + dy) * SS
        d = ImageDraw.Draw(im)
        for pts, fill in ops(L, R, T, B, bar * SS):
            d.polygon(pts, fill=fill)
        return np.array(im.resize((FW, FH), Image.LANCZOS)).astype(float) / 255

    def over(rgb, a):
        da = out[..., 3] / 255.0
        na = a + da * (1 - a)
        for c in range(3):
            num = rgb * a + out[..., c] * da * (1 - a)
            out[..., c] = np.where(na > 1e-6, num / np.maximum(na, 1e-6), out[..., c])
        out[..., 3] = na * 255

    ys = np.arange(FH)[:, None] * np.ones((1, FW))
    ramp = np.clip((163 + (ys - top) * 1.25) / 255, 0, 1)
    for dx, dy, col, k in [(2, 2, 128, 0.55), (1, 1, 118, 0.95)]:
        over(col, mask(dx, dy) * ramp * k)
    over(255, mask(0, 0) * ramp)
    return out


def letter_p(L, R, T, B, b):
    """A P in the game's blocky face: the stem, and a bowl to mid-height."""
    mid = T + (B - T) * 0.56
    r = (R - L) * 0.30                      # the bowl's rounded corners
    outer = [(L, T), (R - r, T), (R, T + r), (R, mid - r), (R - r, mid), (L + b, mid), (L + b, B), (L, B)]
    hole = [(L + b, T + b), (R - b - r * 0.4, T + b), (R - b, T + b + r * 0.4), (R - b, mid - b - r * 0.4),
            (R - b - r * 0.4, mid - b), (L + b, mid - b)]
    return [(outer, 255), (hole, 0)]


def crest(L, R, T, B, b):
    """The shield: a heater shield, outlined, with a bolt down it (the
    Valkyrie's)."""
    cx = (L + R) / 2

    def heater(l, r, t, bot):
        w = r - l
        pts = [(l, t), (r, t), (r, t + (bot - t) * 0.42)]
        for k in range(1, 8):
            a = k / 8
            pts.append((r - w / 2 * (1 - math.cos(a * math.pi / 2)), t + (bot - t) * (0.42 + 0.58 * math.sin(a * math.pi / 2))))
        pts.append((cx, bot))
        for k in range(7, 0, -1):
            a = k / 8
            pts.append((l + w / 2 * (1 - math.cos(a * math.pi / 2)), t + (bot - t) * (0.42 + 0.58 * math.sin(a * math.pi / 2))))
        pts.append((l, t + (bot - t) * 0.42))
        return pts

    k = b * 0.7
    my = T + (B - T) * 0.46
    bolt = [(cx + b * 0.9, T + b * 1.2), (cx - b * 1.05, my + b * 0.25), (cx + b * 0.05, my + b * 0.25),
            (cx - b * 0.8, B - b * 1.6), (cx + b * 1.1, my - b * 0.45), (cx + b * 0.05, my - b * 0.45)]
    return [(heater(L, R, T, B), 255), (heater(L + k, R - k, T + k, B - k * 1.5), 0), (bolt, 255)]


def write_cell(frame, path):
    MI.write_tga(frame, path)


def buttons():
    cell, box = blank_e(read_cell("weapon_e_up.tga"))
    x0, y0, x1, y1 = box
    flipped = cell[::-1].copy()
    fbox = (x0, FH - 1 - y1, x1, FH - 1 - y0)
    for name, glyph, ratio in (("p", letter_p, 0.66), ("sh", crest, 0.84)):
        write_cell(draw_glyph(cell, box, glyph, ratio), os.path.join(HERE, "weapon_%s_up.tga" % name))
        write_cell(draw_glyph(flipped, fbox, glyph, ratio), os.path.join(HERE, "weapon_%s_down.tga" % name))


# ---- the sheets -----------------------------------------------------------------------
CELL = 64


def sheet(frames):
    """Frames side by side, each followed by a column of pure green: how
    UE_LoadSprites finds where one ends (beach/art_mech.sheet)."""
    out = Image.new("RGBA", ((CELL + 1) * len(frames), CELL), (0, 0, 0, 0))
    d = ImageDraw.Draw(out)
    for i, f in enumerate(frames):
        out.paste(f, (i * (CELL + 1), 0))
        d.line([(i * (CELL + 1) + CELL, 0), (i * (CELL + 1) + CELL, CELL - 1)], fill=GREEN)
    return out


def write_tga(img, path):
    img = img.convert("RGBA")
    w, h = img.size
    hdr = struct.pack("<BBBHHBHHHHBB", 0, 0, 2, 0, 0, 0, 0, 0, w, h, 32, 8)
    a = np.asarray(img)[::-1][..., [2, 1, 0, 3]]
    open(path, "wb").write(hdr + a.tobytes())


def bubble():
    """The Valkyrie's hexagon shield, small: 26 units in radius round the
    ship, a wave of light running round it (0-3), then faint (4)."""
    out = []
    S = 4
    N = CELL * S
    c = N / 2
    R = 26 * S
    hs = 4.2 * S
    for f in range(5):
        faint = f == 4
        img = Image.new("RGBA", (N, N), (0, 0, 0, 0))
        d = ImageDraw.Draw(img)
        for row in range(-10, 11):
            for col in range(-10, 11):
                x = c + col * hs * 1.5
                y = c + row * hs * math.sqrt(3) + (col % 2) * hs * math.sqrt(3) / 2
                rr = math.hypot(x - c, y - c) / R
                if rr > 0.96:
                    continue
                ang = math.atan2(y - c, x - c)
                wave = 0.5 + 0.5 * math.cos(ang - (f % 4) * math.pi / 2)
                a = (30 + 150 * rr ** 3) * (0.5 + 0.5 * wave) * (0.35 if faint else 1.0)
                pts = [(x + hs * 0.88 * math.cos(k * math.pi / 3), y + hs * 0.88 * math.sin(k * math.pi / 3))
                       for k in range(6)]
                d.line(pts + [pts[0]], fill=(120, 220, 255, int(min(255, a))), width=S)
        rim = Image.new("RGBA", img.size, (0, 0, 0, 0))
        dr = ImageDraw.Draw(rim)
        dr.ellipse([c - R, c - R, c + R, c + R], outline=(150, 235, 255, 70 if faint else 200), width=int(2.5 * S))
        rim = rim.filter(ImageFilter.GaussianBlur(S * 1.2))
        img.alpha_composite(rim)
        # the glint where the light is
        if not faint:
            g = Image.new("RGBA", img.size, (0, 0, 0, 0))
            a = (f % 4) * math.pi / 2
            gx, gy = c + math.cos(a) * R * 0.92, c + math.sin(a) * R * 0.92
            ImageDraw.Draw(g).ellipse([gx - 5 * S, gy - 5 * S, gx + 5 * S, gy + 5 * S], fill=(230, 252, 255, 200))
            img.alpha_composite(g.filter(ImageFilter.GaussianBlur(S * 2)))
        out.append(img.resize((CELL, CELL), Image.LANCZOS))
    return out


def plasma():
    """A ball of violet plasma, 28 units in radius: a hot rim, filaments
    churning round inside it (they turn a quarter of their period a frame,
    so four frames loop), a thin glow outside. Mostly clear in the middle:
    the ship is inside it."""
    out = []
    S = 4
    N = CELL * S
    c = N / 2
    R = 28 * S
    rng = np.random.default_rng(8)
    fil = [(rng.uniform(0, 2 * math.pi), rng.uniform(0.45, 0.95), rng.uniform(0.8, 1.6), rng.integers(2, 4))
           for _ in range(7)]
    yy, xx = np.mgrid[0:N, 0:N]
    rr = np.hypot(xx - c + 0.5, yy - c + 0.5) / R
    for f in range(4):
        # the body: clear centre, a deepening violet towards a white-pink rim
        body = np.zeros((N, N, 4), np.float32)
        body[..., 0], body[..., 1], body[..., 2] = 190, 70, 255
        a = np.where(rr <= 1.0, 18 + 120 * np.clip(rr, 0, 1) ** 4, 0)
        a += np.clip(1 - np.abs(rr - 1.0) / 0.10, 0, 1) * 200            # the rim
        a += np.where(rr > 1.0, np.clip(1 - (rr - 1.0) / 0.12, 0, 1) * 70, 0)   # the glow
        body[..., 3] = np.clip(a, 0, 255)
        hot = np.clip(1 - np.abs(rr - 0.99) / 0.05, 0, 1)
        body[..., 0] += (255 - body[..., 0]) * hot
        body[..., 1] += (220 - body[..., 1]) * hot
        body[..., 2] += (255 - body[..., 2]) * hot
        img = Image.fromarray(np.clip(body, 0, 255).astype(np.uint8), "RGBA")
        # filaments: arcs of light round the inside, turning
        fl = Image.new("RGBA", img.size, (0, 0, 0, 0))
        d = ImageDraw.Draw(fl)
        for (a0, rad, span, w) in fil:
            a1 = a0 + f * math.pi / 2 / 2
            pts = []
            for k in range(14):
                t = a1 + span * k / 13
                wob = 1 + 0.08 * math.sin(3 * t + f * math.pi / 2)
                pts.append((c + math.cos(t) * R * rad * wob, c + math.sin(t) * R * rad * wob))
            d.line(pts, fill=(255, 205, 255, 255), width=int(w * S * 0.8))
        fl = fl.filter(ImageFilter.GaussianBlur(S * 0.8))
        img.alpha_composite(fl)
        # sparks crawling on the rim
        d = ImageDraw.Draw(img)
        for k in range(6):
            t = 2 * math.pi * k / 6 + f * math.pi / 12
            x, y = c + math.cos(t) * R, c + math.sin(t) * R
            d.ellipse([x - 1.6 * S, y - 1.6 * S, x + 1.6 * S, y + 1.6 * S], fill=(255, 255, 255, 255))
        out.append(img.resize((CELL, CELL), Image.LANCZOS))
    return out


def main():
    buttons()
    write_tga(sheet(bubble()), os.path.join(HERE, "shield_bubble.tga"))
    write_tga(sheet(plasma()), os.path.join(HERE, "plasma_sphere.tga"))
    print("wrote the P and shield buttons, shield_bubble.tga, plasma_sphere.tga")


if __name__ == "__main__":
    main()
