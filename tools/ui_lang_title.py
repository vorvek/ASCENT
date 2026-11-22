#!/usr/bin/env python3
"""Draw the raw.png of the job p10_title of ui_lang.py: the name of the city
on the parchment of the Waterdeep screen (FEBGND 10) in the language.

The English letters are dark red-brown serif capitals painted on the
parchment, with the shading of the parchment over them. This tool finds the
letters in in.png (the crop of the upscaled picture that ui_lang.py prep
writes), fills them with the parchment around them (cv2.inpaint), and draws
the new words in a bold serif font: each pixel of a new letter is the
parchment under it times the color ratio of the old letters to the
parchment, so the shading and the grain of the parchment stay on the
letters. Then run ui_lang.py build. The output is game art: never commit it.

    python -I tools/ui_lang_title.py [--hd DIR] [--text "AGUAS PROFUNDAS"]
        [--font C:/Windows/Fonts/timesbd.ttf] [--height 0.78] [--condense 0.86]

--height is the height of the new capitals as a part of the English ones,
--condense the width of the letters as a part of the font's width. The
words get as tall as both allow inside the width of the English word plus
the margin of the parchment.
"""
import argparse
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import textures_hd as th  # noqa: E402


def letters(a):
    """The mask (0..1) of the dark red letters of the crop (float RGB 0..1)."""
    import numpy as np
    # the parchment and its dark streaks have green / red about 0.7 to 0.8,
    # the letters 0.35 to 0.55
    r, g, b = a[..., 0], a[..., 1], a[..., 2]
    m = (g < 0.6 * r) & (b < 0.4 * r) & (r < 0.45)
    return m.astype(np.uint8)


def main():
    import cv2
    import numpy as np
    from PIL import Image, ImageDraw, ImageFont
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--hd", default=th.HD_DIR)
    ap.add_argument("--job", default="p10_title")
    ap.add_argument("--text", default="AGUAS PROFUNDAS")
    ap.add_argument("--font", default=r"C:\Windows\Fonts\timesbd.ttf")
    ap.add_argument("--height", type=float, default=0.78)
    ap.add_argument("--condense", type=float, default=0.86)
    ap.add_argument("--debug", action="store_true", help="also write mask.png and clean.png")
    args = ap.parse_args()
    d = os.path.join(args.hd, "work", "ui_lang", args.job)
    src = np.asarray(Image.open(os.path.join(d, "in.png")).convert("RGB"), np.float32) / 255.0
    h, w = src.shape[:2]

    # the English letters and the parchment without them
    m = letters(src)
    m = cv2.morphologyEx(m, cv2.MORPH_OPEN, np.ones((3, 3), np.uint8))
    # only the letters: the large parts in the middle rows (the edges of the
    # parchment have red-brown spots too)
    n, lab, st, cen = cv2.connectedComponentsWithStats(m)
    keep = np.zeros(n, bool)
    for i in range(1, n):
        x, y, bw, bh, area = st[i]
        keep[i] = area > 300 and bh > h * 0.25 and h * 0.2 < cen[i][1] < h * 0.8
    m = keep[lab].astype(np.uint8)
    ys, xs = np.nonzero(m)
    top, bot, left, right = ys.min(), ys.max(), xs.min(), xs.max()
    # the soft edge of the letters is part of the hole
    hole = cv2.dilate(m, np.ones((19, 19), np.uint8))
    img8 = (src * 255 + 0.5).astype(np.uint8)
    clean = cv2.inpaint(img8, hole * 255, 15, cv2.INPAINT_TELEA).astype(np.float32) / 255.0
    # the grain of the parchment: the high frequencies of the parchment
    # above or below, moved into the holes (the inpaint is smooth)
    blur = cv2.GaussianBlur(src, (0, 0), 3)
    free = (1 - hole).astype(np.float32)[..., None]
    grain = (src - blur) * free
    shift = bot - top + 1
    up, dn = np.roll(grain, shift, axis=0), np.roll(grain, -shift, axis=0)
    upf = np.roll(free, shift, axis=0)
    donor = up * upf + dn * (1 - upf)
    hm = cv2.GaussianBlur(hole.astype(np.float32), (0, 0), 3)[..., None]
    clean = np.clip(clean + donor * hm, 0, 1)

    # the color of the letters as a part of the parchment
    core = cv2.erode(m, np.ones((5, 5), np.uint8)) > 0
    ratio = np.median(src[core] / np.maximum(clean[core], 0.02), axis=0)

    # the new words
    cap = (bot - top + 1) * args.height
    room = (right - left + 1) + 2 * min(left, w - 1 - right) * 0.35
    size = 200
    font = ImageFont.truetype(args.font, size)
    bx = font.getbbox(args.text)
    capb = font.getbbox("H")
    cap_px = capb[3] - capb[1]
    scale = cap / cap_px
    width = (bx[2] - bx[0]) * scale * args.condense
    if width > room:
        scale *= room / width
    font = ImageFont.truetype(args.font, max(8, int(round(size * scale))))
    bx = font.getbbox(args.text)
    capb = font.getbbox("H")
    tw, th_ = bx[2] - bx[0] + 4, bx[3] + 8
    canvas = Image.new("L", (tw, th_), 0)
    ImageDraw.Draw(canvas).text((-bx[0] + 2, 0), args.text, fill=255, font=font)
    nw = int(round(tw * args.condense))
    canvas = canvas.resize((nw, th_), Image.LANCZOS)
    t = np.asarray(canvas, np.float32) / 255.0
    # soft edges as in the original
    t = cv2.GaussianBlur(t, (0, 0), 1.2)
    cap_top = capb[1]
    cap_h = capb[3] - capb[1]
    mid = (top + bot) / 2.0
    oy = int(round(mid - (cap_top + cap_h / 2.0)))
    ox = int(round((left + right) / 2.0 - nw / 2.0))
    alpha = np.zeros((h, w), np.float32)
    y0, x0 = max(0, oy), max(0, ox)
    y1, x1 = min(h, oy + th_), min(w, ox + nw)
    alpha[y0:y1, x0:x1] = t[y0 - oy:y1 - oy, x0 - ox:x1 - ox]
    ink = clean * ratio[None, None, :]
    out = clean * (1 - alpha[..., None]) + ink * alpha[..., None]
    Image.fromarray((np.clip(out, 0, 1) * 255 + 0.5).astype(np.uint8), "RGB").save(os.path.join(d, "raw.png"))
    if args.debug:
        Image.fromarray(hole * 255).save(os.path.join(d, "mask.png"))
        Image.fromarray((clean * 255 + 0.5).astype(np.uint8), "RGB").save(os.path.join(d, "clean.png"))
    print("letters %d..%d x %d..%d, ratio %s, cap %.0f px, font %d px, width %d of %d" % (
        top, bot, left, right, np.round(ratio, 2), cap_h, font.size, nw, int(room)))


if __name__ == "__main__":
    main()
