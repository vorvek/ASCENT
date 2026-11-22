#!/usr/bin/env python3
"""Redraw the words of a plate label in the raw.png of a job of ui_lang.py.

For a label whose Codex image has the right letters but a wrong word (the
alignment TRUE NEUTRAL, made as "NEUTRAL AUTENTICO" before the glossary
fixed "Neutral absoluto"). The dark letters on the lit plate are found by
their contrast to the plate around them, filled with the plate
(cv2.inpaint), and the new words are drawn in a bold serif font: each pixel
of a new letter is the plate under it times the color ratio of the old
letters to the plate. The first run keeps the Codex image as raw_codex.png
and always reads that file. Then run ui_lang.py build. The output is game
art: never commit it.

    python tools/ui_lang_plate.py --hd DIR --job p5_nu --text "NEUTRAL ABSOLUTO" \\
        [--band 0.3,0.7] [--x 0.06,0.935] [--extra X0,Y0,X1,Y1] [--font F]

The jobs of the Spanish alignment TRUE NEUTRAL:

    --job p5_nu --x 0.06,0.935
    --job lb4nu --band 0.36,0.6 --x 0.1,0.83 --extra 1245,220,1305,280

--band and --x limit the search for the letters (parts of the height and the
width of raw.png: the rivets and the frame stay out), --extra fills one more
rectangle of raw.png (an accent above the band).
"""
import argparse
import os
import shutil
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import textures_hd as th  # noqa: E402


def main():
    import cv2
    import numpy as np
    from PIL import Image, ImageDraw, ImageFont
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--hd", default=th.HD_DIR)
    ap.add_argument("--job", required=True)
    ap.add_argument("--text", required=True)
    ap.add_argument("--font", default=r"C:\Windows\Fonts\GOUDOSB.TTF")
    ap.add_argument("--band", default="0.3,0.7")
    ap.add_argument("--x", default="0.04,0.96")
    ap.add_argument("--extra", default=None)
    ap.add_argument("--thr", type=float, default=0.62, help="a letter pixel is darker than this part of the plate")
    args = ap.parse_args()
    d = os.path.join(args.hd, "work", "ui_lang", args.job)
    codex = os.path.join(d, "raw_codex.png")
    if not os.path.isfile(codex):
        shutil.copyfile(os.path.join(d, "raw.png"), codex)
    src = np.asarray(Image.open(codex).convert("RGB"), np.float32) / 255.0
    h, w = src.shape[:2]

    # the letters: darker than the plate around them
    lum = src.mean(axis=2)
    bg = cv2.GaussianBlur(cv2.dilate(lum, np.ones((31, 31), np.uint8)), (0, 0), 15)
    m = ((lum < args.thr * bg) & (bg > 0.08)).astype(np.uint8)
    y0, y1 = map(float, args.band.split(","))
    x0, x1 = map(float, args.x.split(","))
    lim = np.zeros_like(m)
    lim[int(h * y0):int(h * y1), int(w * x0):int(w * x1)] = 1
    m &= lim
    m = cv2.morphologyEx(m, cv2.MORPH_OPEN, np.ones((3, 3), np.uint8))
    n, lab, st, _ = cv2.connectedComponentsWithStats(m)
    keep = np.zeros(n, bool)
    for i in range(1, n):
        keep[i] = st[i][4] > 150 and st[i][3] > h * 0.05
    m = keep[lab].astype(np.uint8)
    xs = np.nonzero(m)[1]
    rows = m.sum(axis=1)
    core_rows = np.nonzero(rows > rows.max() * 0.35)[0]
    top, bot, left, right = core_rows.min(), core_rows.max(), xs.min(), xs.max()

    # the plate without the letters
    hole_l = cv2.dilate(m, np.ones((15, 15), np.uint8))
    hole = hole_l.copy()
    if args.extra:
        ex0, ey0, ex1, ey1 = map(int, args.extra.split(","))
        hole[ey0:ey1, ex0:ex1] = 1
    img8 = (src * 255 + 0.5).astype(np.uint8)
    clean = cv2.inpaint(img8, hole * 255, 12, cv2.INPAINT_TELEA).astype(np.float32) / 255.0
    # the grain of the plate above or below, moved into the holes of the letters
    blur = cv2.GaussianBlur(src, (0, 0), 3)
    free = (1 - hole).astype(np.float32)[..., None]
    grain = (src - blur) * free
    shift = bot - top + 1
    up, dn = np.roll(grain, shift, axis=0), np.roll(grain, -shift, axis=0)
    upf = np.roll(free, shift, axis=0)
    donor = up * upf + dn * (1 - upf)
    hm = cv2.GaussianBlur(hole_l.astype(np.float32), (0, 0), 3)[..., None]
    clean = np.clip(clean + donor * hm * 0.8, 0, 1)
    core = cv2.erode(m, np.ones((5, 5), np.uint8)) > 0
    ratio = np.median(src[core] / np.maximum(clean[core], 0.02), axis=0)

    # the new words: the cap height of the old ones, at most their width
    size = 300
    font = ImageFont.truetype(args.font, size)
    capb = font.getbbox("H")
    scale = (bot - top + 1) / (capb[3] - capb[1])
    bx = font.getbbox(args.text)
    width = (bx[2] - bx[0]) * scale
    if width > right - left + 1:
        scale *= (right - left + 1) / width
    font = ImageFont.truetype(args.font, max(8, int(round(size * scale))))
    bx = font.getbbox(args.text)
    capb = font.getbbox("H")
    tw, tht = bx[2] - bx[0] + 4, bx[3] + 8
    canvas = Image.new("L", (tw, tht), 0)
    ImageDraw.Draw(canvas).text((-bx[0] + 2, 0), args.text, fill=255, font=font)
    t = cv2.GaussianBlur(np.asarray(canvas, np.float32) / 255.0, (0, 0), 1.5)
    oy = int(round((top + bot) / 2.0 - (capb[1] + (capb[3] - capb[1]) / 2.0)))
    ox = int(round((left + right) / 2.0 - tw / 2.0))
    alpha = np.zeros((h, w), np.float32)
    ya, xa, yb, xb = max(0, oy), max(0, ox), min(h, oy + tht), min(w, ox + tw)
    alpha[ya:yb, xa:xb] = t[ya - oy:yb - oy, xa - ox:xb - ox]
    out = clean * (1 - alpha[..., None]) + clean * ratio[None, None, :] * alpha[..., None]
    Image.fromarray((np.clip(out, 0, 1) * 255 + 0.5).astype(np.uint8), "RGB").save(os.path.join(d, "raw.png"))
    print("letters rows %d..%d, columns %d..%d, ratio %s, font %d px, width %d" % (
        top, bot, left, right, np.round(ratio, 2), font.size, tw))


if __name__ == "__main__":
    main()
