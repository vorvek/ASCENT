#!/usr/bin/env python3
"""Make upscaled copies of the interface pictures of FEBGND.DAT.

Output: <hd>/ui/febgnd_<n>.png, RGB 8-bit (RGBA for a picture of KEY), SCALE times the width and the height
of picture n (6 times for the pictures of the 320x200 screen), and
<hd>/ui/index.txt with one "n w h scale method" line per file. The default <hd>
is gamedata/hd, which git ignores. Never commit the output: it is derived from
the game data.

  python -I tools/ui_hd.py [--only 63,86] [--force] [--sheet]

The tool uses the virtual environment and the models of tools/textures_hd.py
(run that tool with --setup first).

The game shows the upscaled picture where the interface draws the picture
(engine/main/dtuhdui.c, the compose step of engine/platform/gpu.c). The
pictures and their uses are in docs/hd_textures.md.

Steps for each picture:
  1. the colors of the palette of the entry (the palette that the game shows
     the picture with: the in-game pictures have the colors of PALETTE.256,
     and a front end picture that is drawn over a screen has the palette of
     that screen; see docs/hd_textures.md),
  2. padding by mirroring (PAD source pixels),
  3. upscale x4 with the method: "detail" (RealESRGAN x4plus plus 0.6 times
     the fine detail of 4x-UltraSharp, the method of the wall textures) for
     the stone, wood and metal art, x4plus for the rendered scenes,
     4x-UltraSharp for the Waterdeep screen (x4plus wipes out the grain of
     its grass),
  4. crop the padding, resize to the output scale (bicubic with antialias:
     3x is a reduction of the 4x result, 6x an enlargement),
  5. the mean color is moved back to the mean of the original (a constant
     offset per channel).
--sheet writes 1:1 crops of the original (nearest) and of the result side by
side into <hd>/work/uicompare for a check by eye.
"""
import argparse
import os
import subprocess
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import jrg  # noqa: E402
import textures_hd as th  # noqa: E402

GAME_DIR = th.GAME_DIR
PAD = 16
SCALE = 3            # pictures of the 640x480 screen: 1920x1440 at 1:1
SCALE_LOW = 6        # pictures of the 320x200 screen: 1920x1200 at 1:1

# n -> (method, use). The method "detail" is for the stone, wood and metal
# art, x4plus for the rendered scenes and the logos. The Waterdeep screen (10)
# takes 4x-UltraSharp: x4plus made its grass a flat green (the grass of the
# original is a fine grain of green, brown and red pixels).
PICTURES = {
    # the play screen (640x480, Dtu_hires) and the screens that open in play
    63: ("detail", "play screen, main mode"),
    64: ("detail", "play screen, map"),
    65: ("detail", "play screen, inventory (and hot keys)"),
    67: ("detail", "play screen, statistics and spells"),
    86: ("detail", "menu box stone (newmenu, 640x480)"),
    # the same at 320x200
    57: ("detail", "play screen 320x200, main mode"),
    58: ("detail", "play screen 320x200, map"),
    59: ("detail", "play screen 320x200, inventory"),
    60: ("detail", "play screen 320x200, hot keys"),
    61: ("detail", "play screen 320x200, statistics"),
    85: ("detail", "menu box stone (newmenu, 320x200)"),
    # the front end
    193: ("x4plus", "title"),
    18: ("x4plus", "screen 10 (Interplay logo)"),
    19: ("x4plus", "screen 11 (title of the demo)"),
    0: ("x4plus", "screen 1, Blackstaff (main menu)"),
    1: ("detail", "screens 4 (stone), menu box of screens 1 and 4"),
    10: ("ultrasharp", "screen 0, Waterdeep"),
    11: ("detail", "menu box of Waterdeep"),
    2: ("detail", "character creation 1"),
    3: ("detail", "character creation 2"),
    4: ("detail", "character creation 3"),
    5: ("detail", "character creation 4"),
    6: ("detail", "menu box of character creation 1"),
    7: ("detail", "menu box of character creation 2"),
    8: ("detail", "menu box of character creation 3"),
    9: ("detail", "menu box of character creation 4"),
    14: ("detail", "screen 3, market"),
    15: ("detail", "menu box of the market"),
    25: ("detail", "market quantity window"),
    26: ("x4plus", "screen 2"),
    12: ("x4plus", "screen 5, loading picture of the front end"),
    155: ("x4plus", "loading picture"),
    156: ("x4plus", "loading picture"),
    157: ("x4plus", "loading picture"),
    158: ("x4plus", "loading picture"),
    159: ("x4plus", "loading picture"),
    160: ("x4plus", "loading picture"),
    161: ("x4plus", "death picture"),
    # the Blackstaff screen: the talking head of Khelben (7 mouth pictures
    # of 82x117 side by side, color 255 not drawn) and the pictures that
    # the speech shows (~pN of the text, entry 27 + N)
    195: ("face", "Khelben, the talking head (7 frames)"),
}
for _n in range(27, 57):
    PICTURES[_n] = ("x4plus", "Blackstaff screen, picture of the speech")
LOW = {57, 58, 59, 60, 61, 85}
# A picture that the game shows with the palette of another entry.
PALETTE_OF = {25: 14, 195: 0}
PALETTE_OF.update({_n: 0 for _n in range(27, 57)})
# A picture that the game draws with a transparent index (fe_blit_part):
# the file is RGBA, alpha 0 where the picture has that index.
KEY = {195: 255}
# A sheet of frames: each cell of this width is upscaled alone, so that no
# frame takes colors from the next one.
CELL_W = {195: 82}


def ensure_venv(hd):
    """Run this script again with the Python of the virtual environment."""
    try:
        import numpy  # noqa: F401
        import torch  # noqa: F401
        import PIL  # noqa: F401
        return
    except ImportError:
        pass
    vpy = th.venv_python(hd)
    if not os.path.isfile(vpy):
        sys.exit("no virtual environment in %s: run tools/textures_hd.py --setup first" % hd)
    if os.path.abspath(sys.executable).lower() == os.path.abspath(vpy).lower():
        sys.exit("the virtual environment lacks numpy, torch or Pillow")
    sys.exit(subprocess.call([vpy, "-I", os.path.abspath(__file__)] + sys.argv[1:]))


class Febgnd:
    def __init__(self):
        self.index = jrg.read_file(os.path.join(GAME_DIR, "FEBGND.JRG"))
        self.data = jrg.read_file(os.path.join(GAME_DIR, "FEBGND.DAT"))
        self.entries = jrg.parse_febgnd(self.index)

    def picture(self, n):
        """(w, h, palette 768 bytes of 6-bit values, pixels w*h)"""
        off, w, h, size = self.entries[n]
        pix = jrg.lzss_decode(self.data, off + 768, off + size)[0]
        return w, h, self.data[off:off + 768], bytes(pix[:w * h])

    def rgb(self, n):
        """float32 (h, w, 3) in 0..1 with the palette that the game uses"""
        import numpy as np
        w, h, _, pix = self.picture(n)
        pal6 = self.picture(PALETTE_OF.get(n, n))[2]
        pal = np.array([((c & 63) << 2) | ((c & 63) >> 4) for c in pal6], np.float32).reshape(256, 3) / 255.0
        return pal[np.frombuffer(pix, np.uint8).reshape(h, w)]


def resize(a, h, w, reduce):
    """(H, W, C) float32 -> (h, w, C), bicubic (with antialias for a reduction)"""
    import numpy as np
    import torch
    t = torch.from_numpy(np.ascontiguousarray(a.transpose(2, 0, 1)))[None].cuda()
    t = torch.nn.functional.interpolate(t, size=(h, w), mode="bicubic",
                                        align_corners=False, antialias=reduce)
    return t[0].permute(1, 2, 0).cpu().numpy()


def upscale(ups, rgb, method, scale, opaque=None):
    """rgb (h, w, 3) float32 -> (h*scale, w*scale, 3) uint8, mean diff before and after.
    With opaque (h, w) bool: the other pixels get the colors of their opaque
    neighbors before the upscale, the result is RGBA with alpha 0 there (the
    mask upscaled the same way and cut at 50 percent)."""
    import numpy as np
    h, w = rgb.shape[:2]
    if opaque is not None and not opaque.any():
        return np.zeros((h * scale, w * scale, 4), np.uint8), 0.0, 0.0
    if opaque is not None:
        rgb = th.fill_transparent(rgb, opaque, False)
    pad = np.pad(rgb, ((PAD, PAD), (PAD, PAD), (0, 0)), mode="symmetric")
    up = ups.run(pad[None].astype(np.float32), method)[0]
    c = PAD * th.SCALE
    up = up[c:c + h * th.SCALE, c:c + w * th.SCALE]
    if scale != th.SCALE:
        up = np.clip(resize(up, h * scale, w * scale, scale < th.SCALE), 0, 1)
    up = up * 255.0
    sel = np.ones((h * scale, w * scale), bool)
    orig = rgb.reshape(-1, 3)
    if opaque is not None:
        m = np.pad(opaque.astype(np.float32), PAD, mode="symmetric")[..., None]
        m = resize(m, (h + 2 * PAD) * scale, (w + 2 * PAD) * scale, False)[..., 0]
        sel = m[PAD * scale:(PAD + h) * scale, PAD * scale:(PAD + w) * scale] >= 0.5
        orig = rgb[opaque]
    mo = orig.mean(0) * 255.0
    before = float(np.abs(up[sel].mean(0) - mo).max())
    up = np.clip(np.rint(up + (mo - up[sel].mean(0))), 0, 255).astype(np.uint8)
    after = float(np.abs(up[sel].astype(np.float64).mean(0) - mo).max())
    if opaque is not None:
        up = np.concatenate([up, np.where(sel, 255, 0).astype(np.uint8)[..., None]], -1)
    return up, before, after


def upscale_picture(ups, fb, n, scale):
    """Picture n with its method, key and cells: (RGB or RGBA uint8, diff before, after)."""
    import numpy as np
    rgb = fb.rgb(n)
    method = PICTURES[n][0]
    opaque = None
    if n in KEY:
        opaque = np.frombuffer(fb.picture(n)[3], np.uint8).reshape(rgb.shape[:2]) != KEY[n]
    cw = CELL_W.get(n, rgb.shape[1])
    parts, diffs = [], []
    for x in range(0, rgb.shape[1], cw):
        part, b, a = upscale(ups, rgb[:, x:x + cw], method, scale,
                             None if opaque is None else opaque[:, x:x + cw])
        parts.append(part)
        diffs.append((b, a))
    return np.concatenate(parts, 1), max(d[0] for d in diffs), max(d[1] for d in diffs)


def block_diff(rgb, up, scale, b=8):
    """The largest difference (0..255) of the mean color of a block of b x b source
    pixels between the original and the result."""
    import numpy as np
    h, w = rgb.shape[:2]
    h2, w2 = h // b * b, w // b * b
    o = (rgb[:h2, :w2] * 255.0).reshape(h2 // b, b, w2 // b, b, 3).mean((1, 3))
    u = up[:h2 * scale, :w2 * scale].astype(np.float64).reshape(h2 // b, b * scale, w2 // b, b * scale, 3).mean((1, 3))
    return float(np.abs(o - u).max()), float(np.abs(o - u).mean())


def sheet(fb, n, up, scale, outdir):
    """1:1 crops: the original repeated (nearest) and the result."""
    import numpy as np
    from PIL import Image
    rgb = (fb.rgb(n) * 255.0 + 0.5).astype(np.uint8)
    h, w = rgb.shape[:2]
    cw, ch = min(w, 160), min(h, 120)
    x0, y0 = (w - cw) // 2, (h - ch) // 2
    near = rgb.repeat(scale, 0).repeat(scale, 1)[y0 * scale:(y0 + ch) * scale, x0 * scale:(x0 + cw) * scale]
    hd = up[y0 * scale:(y0 + ch) * scale, x0 * scale:(x0 + cw) * scale, :3]
    out = np.concatenate([near, np.zeros((near.shape[0], 8, 3), np.uint8), hd], 1)
    Image.fromarray(out, "RGB").save(os.path.join(outdir, "febgnd_%d.png" % n))


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--only", help="picture numbers, comma separated")
    ap.add_argument("--out", default=th.HD_DIR, help="hd directory (default gamedata/hd)")
    ap.add_argument("--force", action="store_true", help="redo existing files")
    ap.add_argument("--sheet", action="store_true", help="1:1 crops in <hd>/work/uicompare")
    args = ap.parse_args()
    ensure_venv(args.out)
    from PIL import Image

    fb = Febgnd()
    outdir = os.path.join(args.out, "ui")
    os.makedirs(outdir, exist_ok=True)
    sheetdir = os.path.join(args.out, "work", "uicompare")
    if args.sheet:
        os.makedirs(sheetdir, exist_ok=True)
    todo = sorted(PICTURES)
    if args.only:
        todo = [int(x) for x in args.only.split(",")]
    ups = th.Upscaler(args.out)
    t_all = time.time()
    for n in todo:
        method = PICTURES[n][0]
        scale = SCALE_LOW if n in LOW else SCALE
        path = os.path.join(outdir, "febgnd_%d.png" % n)
        if os.path.isfile(path) and not args.force and not args.sheet:
            continue
        t0 = time.time()
        rgb = fb.rgb(n)
        up, before, after = upscale_picture(ups, fb, n, scale)
        tmp = path + ".tmp"
        Image.fromarray(up, "RGBA" if up.shape[2] == 4 else "RGB").save(tmp, format="PNG", compress_level=6)
        os.replace(tmp, path)
        bmax, bmean = block_diff(rgb, up[..., :3], scale)
        print("febgnd %3d %dx%d -> %dx%d %-6s %5.1fs  mean diff %.2f -> %.2f, 8x8 block diff max %.1f mean %.2f, %.1f MB"
              % (n, rgb.shape[1], rgb.shape[0], up.shape[1], up.shape[0], method, time.time() - t0,
                 before, after, bmax, bmean, os.path.getsize(path) / 1048576.0), flush=True)
        if args.sheet:
            sheet(fb, n, up, scale, sheetdir)
    with open(os.path.join(outdir, "index.txt"), "w") as f:
        for n in sorted(PICTURES):
            w, h = fb.entries[n][1], fb.entries[n][2]
            if os.path.isfile(os.path.join(outdir, "febgnd_%d.png" % n)):
                f.write("%d %d %d %d %s\n" % (n, w, h, SCALE_LOW if n in LOW else SCALE, PICTURES[n][0]))
    print("done in %.1f s" % (time.time() - t_all))


if __name__ == "__main__":
    main()
