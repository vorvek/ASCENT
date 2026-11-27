#!/usr/bin/env python3
"""Draw the icons of the spells of the port and write engine/main/dtuspellicons.h.

The spells 56 to 58 of the port (Mirror Image, Minor Globe of
Invulnerability, Spiritual Hammer; engine/main/dtuspell.h) have the icons
200 to 202, the Pearls of Power of the class gear (engine/main/dtugear.h)
the icon 203. The icon sheets of the game (FEBGND 23, 16x16, and 24, 32x32)
have 200 cells and no free one, so the game adds a row to the loaded sheet
and copies these icons into it (engine/main/dtudisp.c).

The icons are new art: this script draws each one from shapes (PIL), four
times the size of the 32x32 icon, reduces it and maps each pixel to the
nearest color of the game palette; the pixels around the art are
transparent (index 255), as in the icons of the game. Nothing is taken from the pictures of the
game. The palette (PALETTE.256 of the game, 256 colors of 6 bits) is read
only to choose the color numbers; index 255 (transparent) and 254 are never
used. The output holds color numbers only.

  python -I tools/spell_icons.py [--palette FILE] [--out FILE] [--preview DIR]

Defaults: gamedata/cd/DTU/PALETTE.256 (of this checkout or of the main
checkout of a worktree), engine/main/dtuspellicons.h. --preview DIR writes
icon_<name>_32.png, icon_<name>_16.png (each 8 times enlarged) and
icons_port.png (all, both sizes) with the colors of the palette.

Needs Pillow.
"""
import argparse
import math
import os
import subprocess
import sys

from PIL import Image, ImageChops, ImageDraw, ImageFilter

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, ".."))
S = 128            # the drawing size, 4 times the large icon
NAMES = ("mirror_image", "minor_globe", "spiritual_hammer", "pearl_of_power")


def gamedata_dir():
    own = os.path.join(ROOT, "gamedata")
    if os.path.isdir(own):
        return own
    try:
        common = subprocess.run(["git", "rev-parse", "--path-format=absolute", "--git-common-dir"], cwd=ROOT,
                                capture_output=True, text=True, timeout=30).stdout.strip()
    except (OSError, subprocess.SubprocessError):
        return own
    return os.path.join(os.path.dirname(common), "gamedata") if common else own


def layer():
    return Image.new("RGBA", (S, S), (0, 0, 0, 0))


def glow(img, color, radius, strength=1.0):
    """A soft halo of the alpha of img in one color."""
    a = img.split()[3].filter(ImageFilter.GaussianBlur(radius))
    a = a.point(lambda v: min(255, int(v * strength)))
    g = Image.new("RGBA", img.size, color + (0,))
    g.putalpha(a)
    return g


def shade(img, light=(-0.6, -0.8), amount=70):
    """Light from the upper left: brighten and darken by the slope of the alpha."""
    a = img.split()[3].filter(ImageFilter.GaussianBlur(3))
    w, h = img.size
    ap = a.load()
    px = img.load()
    for y in range(h):
        for x in range(w):
            r, g, b, al = px[x, y]
            if al == 0:
                continue
            dx = ap[min(w - 1, x + 2), y] - ap[max(0, x - 2), y]
            dy = ap[x, min(h - 1, y + 2)] - ap[x, max(0, y - 2)]
            k = (dx * light[0] + dy * light[1]) / 255.0 * amount
            px[x, y] = (max(0, min(255, int(r + k))), max(0, min(255, int(g + k))), max(0, min(255, int(b + k))), al)
    return img


def figure(color, dark, cx, top, scale):
    """The bust of a robed figure: a hood, a face in shadow, shoulders."""
    im = layer()
    d = ImageDraw.Draw(im)
    r = int(17 * scale)
    # shoulders and robe
    d.polygon([(cx - 27 * scale, S + 4), (cx - 23 * scale, top + 48 * scale), (cx - 9 * scale, top + 33 * scale),
               (cx + 9 * scale, top + 33 * scale), (cx + 23 * scale, top + 48 * scale), (cx + 27 * scale, S + 4)], fill=color + (255,))
    # hood
    d.ellipse([cx - r, top, cx + r, top + 2 * r + 4 * scale], fill=color + (255,))
    d.polygon([(cx - r, top + r), (cx, top - 5 * scale), (cx + r, top + r)], fill=color + (255,))
    # the face in the hood
    d.ellipse([cx - r * 0.62, top + r * 0.55, cx + r * 0.62, top + r * 2.0], fill=dark + (255,))
    # the fold of the robe
    d.line([(cx, top + 40 * scale), (cx - 3 * scale, S)], fill=dark + (255,), width=max(2, int(3 * scale)))
    return shade(im)


def icon_mirror_image():
    base = layer()
    blue, blue_dark = (60, 110, 215), (14, 30, 90)
    for cx, top, scale in ((24, 34, 0.86), (104, 34, 0.86)):
        f = figure(blue, blue_dark, cx, top, scale)
        base.alpha_composite(glow(f, (70, 130, 255), 4, 1.2))
        base.alpha_composite(f)
    f = figure((206, 158, 86), (58, 34, 14), 64, 14, 1.0)
    base.alpha_composite(glow(f, (0, 0, 0), 5, 3.0))          # a dark edge between the caster and the images
    base.alpha_composite(f)
    # the eyes of the caster
    d = ImageDraw.Draw(base)
    d.ellipse([56, 38, 61, 42], fill=(255, 236, 170, 255))
    d.ellipse([67, 38, 72, 42], fill=(255, 236, 170, 255))
    return base


def icon_minor_globe():
    base = layer()
    cx = cy = S // 2
    R = 54
    px = base.load()
    for y in range(S):
        for x in range(S):
            dist = math.hypot(x - cx, y - cy) / R
            if dist > 1.12:
                continue
            if dist > 1.0:                       # the halo
                k = (1.12 - dist) / 0.12
                px[x, y] = (40, 70, 170, int(255 * k))
                continue
            rim = dist ** 5                      # the shell: bright at the edge
            hx, hy = (x - (cx - 22)) / 26.0, (y - (cy - 26)) / 18.0
            spec = max(0.0, 1.0 - (hx * hx + hy * hy))     # the highlight
            r = 18 + 80 * rim + 190 * spec ** 2
            g = 22 + 120 * rim + 200 * spec ** 2
            b = 70 + 180 * rim + 185 * spec ** 2
            px[x, y] = (min(255, int(r)), min(255, int(g)), min(255, int(b)), 255)
    # the one inside
    f = figure((206, 160, 80), (70, 40, 14), 64, 44, 0.62)
    mask = Image.new("L", (S, S), 0)
    ImageDraw.Draw(mask).ellipse([cx - R + 6, cy - R + 6, cx + R - 6, cy + R - 6], fill=255)
    f.putalpha(ImageChops.multiply(f.split()[3], mask))
    base.alpha_composite(f)
    # two spells that break on the shell
    d = ImageDraw.Draw(base)
    for (x0, y0, x1, y1) in ((4, 96, 20, 86), (124, 30, 108, 40)):
        d.line([(x0, y0), (x1, y1)], fill=(255, 150, 40, 255), width=5)
        d.ellipse([x1 - 6, y1 - 6, x1 + 6, y1 + 6], fill=(255, 228, 130, 255))
    return base


def icon_spiritual_hammer():
    base = layer()
    h = layer()
    d = ImageDraw.Draw(h)
    # the handle, from the lower left to the upper right
    d.line([(26, 106), (74, 54)], fill=(170, 200, 255, 255), width=10)
    d.ellipse([18, 98, 34, 114], fill=(210, 228, 255, 255))       # the pommel
    # the head: a block across the handle
    ang = math.atan2(54 - 106, 74 - 26)
    ux, uy = math.cos(ang), math.sin(ang)          # along the handle
    vx, vy = -uy, ux                               # across it
    hx, hy = 80, 48
    L, W = 15, 29
    pts = [(hx - ux * L + vx * W, hy - uy * L + vy * W), (hx + ux * L + vx * W, hy + uy * L + vy * W),
           (hx + ux * L - vx * W, hy + uy * L - vy * W), (hx - ux * L - vx * W, hy - uy * L - vy * W)]
    d.polygon(pts, fill=(228, 238, 255, 255))
    # the band of the head
    d.line([(hx - vx * W, hy - vy * W), (hx + vx * W, hy + vy * W)], fill=(120, 160, 240, 255), width=5)
    h = shade(h, amount=110)
    base.alpha_composite(glow(h, (50, 110, 255), 12, 1.8))
    base.alpha_composite(glow(h, (150, 200, 255), 4, 1.5))
    base.alpha_composite(h)
    # the flight
    d = ImageDraw.Draw(base)
    for k, (x0, y0) in enumerate(((6, 66), (14, 44), (52, 122))):
        d.line([(x0, y0), (x0 + 18, y0 - 19)], fill=(70, 120, 235, 255), width=3)
    return base


def icon_pearl_of_power():
    """The Pearls of Power of the class gear (engine/main/dtugear.h): a
    white pearl in a violet glow with a rune of four rays, on a small gold
    cup, so that it does not look like the gem Pearl (icon 129)."""
    base = layer()
    cx, cy, R = S // 2, S // 2 - 8, 34
    # the cup
    d = ImageDraw.Draw(base)
    d.polygon([(cx - 30, cy + 28), (cx + 30, cy + 28), (cx + 16, cy + 50), (cx - 16, cy + 50)], fill=(200, 150, 40, 255))
    d.rectangle([cx - 22, cy + 50, cx + 22, cy + 58], fill=(150, 100, 20, 255))
    p = layer()
    px = p.load()
    for y in range(S):
        for x in range(S):
            dist = math.hypot(x - cx, y - cy) / R
            if dist > 1.0:
                continue
            hx, hy = (x - (cx - 12)) / 16.0, (y - (cy - 14)) / 12.0
            spec = max(0.0, 1.0 - (hx * hx + hy * hy))
            shade_k = 1.0 - 0.45 * dist ** 2
            r = 200 * shade_k + 55 * spec
            g = 196 * shade_k + 59 * spec
            b = 225 * shade_k + 30 * spec
            px[x, y] = (min(255, int(r)), min(255, int(g)), min(255, int(b)), 255)
    base.alpha_composite(glow(p, (170, 70, 255), 12, 2.2))
    base.alpha_composite(p)
    # the rune: four short rays of light from the pearl
    d = ImageDraw.Draw(base)
    for (x0, y0, x1, y1) in ((cx, cy - R - 4, cx, cy - R - 22), (cx - R - 4, cy, cx - R - 20, cy),
                             (cx + R + 4, cy, cx + R + 20, cy)):
        d.line([(x0, y0), (x1, y1)], fill=(220, 160, 255, 255), width=6)
    d.ellipse([cx - 5, cy - 5, cx + 5, cy + 5], fill=(150, 60, 230, 255))
    return base


def nearest_table(pal):
    cache = {}

    def find(rgb):
        key = (rgb[0] >> 2, rgb[1] >> 2, rgb[2] >> 2)
        v = cache.get(key)
        if v is None:
            best, bd = 0, 1 << 30
            for i in range(254):                 # never 254 or 255
                pr, pg, pb = pal[i]
                dr, dg, db = pr - rgb[0], pg - rgb[1], pb - rgb[2]
                dd = 3 * dr * dr + 4 * dg * dg + 2 * db * db
                if dd < bd:
                    best, bd = i, dd
            cache[key] = v = best
        return v
    return find


TRANSPARENT = 255      # the transparent color of the icon sheets


def quantize(img, size, find):
    """Reduce the drawing; a pixel that the art covers less than about a
    third is transparent, the others get the color of the art over black."""
    small = img.convert("RGBa").resize((size, size), Image.LANCZOS).convert("RGBA")
    over = Image.new("RGBA", (size, size), (0, 0, 0, 255))
    over.alpha_composite(small)
    out = []
    for y in range(size):
        for x in range(size):
            out.append(TRANSPARENT if small.getpixel((x, y))[3] < 88 else find(over.getpixel((x, y))[:3]))
    return out


def c_array(name, size, icons):
    out = ["static const unsigned char %s[%d][%d] = {" % (name, len(icons), size * size)]
    for n, pix in zip(NAMES, icons):
        out.append("\t{\t// %s" % n)
        for y in range(size):
            out.append("\t\t" + ",".join("%d" % v for v in pix[y * size:(y + 1) * size]) + ",")
        out.append("\t},")
    out.append("};")
    return "\n".join(out)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--palette", default=os.path.join(gamedata_dir(), "cd", "DTU", "PALETTE.256"))
    ap.add_argument("--out", default=os.path.join(ROOT, "engine", "main", "dtuspellicons.h"))
    ap.add_argument("--preview")
    a = ap.parse_args()

    raw = open(a.palette, "rb").read()[:768]
    pal = [(min(255, raw[3 * i] * 4), min(255, raw[3 * i + 1] * 4), min(255, raw[3 * i + 2] * 4)) for i in range(256)]
    find = nearest_table(pal)
    art = [icon_mirror_image(), icon_minor_globe(), icon_spiritual_hammer(), icon_pearl_of_power()]
    hi = [quantize(im, 32, find) for im in art]
    lo = [quantize(im, 16, find) for im in art]

    text = "\n".join([
        "/*",
        " * ASCENT: new file, written by tools/spell_icons.py. Do not edit it.",
        " * The icons 200 to 202 of the spells of the port (Mirror Image, Minor",
        " * Globe of Invulnerability, Spiritual Hammer; dtuspell.h) and 203 of the",
        " * Pearls of Power of the class gear (dtugear.h) as color",
        " * numbers of the game palette, 32x32 for the sheet FEBGND 24 and 16x16",
        " * for the sheet FEBGND 23. New art, drawn by the script from shapes.",
        " */",
        "#define DTU_PORT_ICON_FIRST\t200",
        "#define DTU_PORT_ICON_COUNT\t%d" % len(art),
        "",
        c_array("Dtu_port_icons_hi", 32, hi),
        "",
        c_array("Dtu_port_icons_lo", 16, lo),
        "",
    ])
    with open(a.out, "w", newline="\n") as f:
        f.write(text)
    print("wrote %s" % a.out)

    if a.preview:
        os.makedirs(a.preview, exist_ok=True)
        sheet = Image.new("RGB", (len(art) * 264 + 8, 264 + 136 + 16), (40, 40, 40))
        for k, name in enumerate(NAMES):
            for size, pix, y0 in ((32, hi[k], 8), (16, lo[k], 8 + 264)):
                im = Image.new("RGB", (size, size))
                im.putdata([(96, 74, 30) if v == TRANSPARENT else pal[v] for v in pix])      # transparent: the color of a panel
                big = im.resize((size * 8, size * 8), Image.NEAREST)
                big.save(os.path.join(a.preview, "icon_%s_%d.png" % (name, size)))
                sheet.paste(big, (8 + k * 264, y0))
        sheet.save(os.path.join(a.preview, "icons_port.png"))
        print("wrote the previews to %s" % a.preview)
    return 0


if __name__ == "__main__":
    sys.exit(main())
