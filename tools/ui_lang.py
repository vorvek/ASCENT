#!/usr/bin/env python3
"""Make the pictures of a language: the interface pictures with the English
words painted into them, redrawn with the words of the language.

Output (all derived from the game art, never commit it; the content pack
lang_<code> of tools/content_pack.py installs it), in <hd>/lang/<code>/:

  orig/febgnd_<n>.png  the 8-bit picture n of FEBGND.DAT, a patch: gray and
                       alpha, the gray value is the palette index, alpha 0
                       keeps the pixel of the game file (alpha 255 on every
                       pixel: the whole picture)
  orig/egg_<name>.png  the same for a bitmap of PORTAL.EGG
  ui/febgnd_<n>.png    the upscaled picture n: RGBA, the size of
                       <hd>/ui/febgnd_<n>.png, drawn over it (alpha blend)

The format is in docs/formats/lang.md, the list of the pictures in
docs/re/frontend.md ("The words in the pictures").

  python -I tools/ui_lang.py prep   [--only a,b]   input crops and prompts
  python -I tools/ui_lang.py codex  [--only a,b] [--jobs 6] [--force]
  python -I tools/ui_lang.py build  [--sheet]      the files of the language

Steps:
  prep   For each job a crop with some context: of the upscaled picture
         (<hd>/ui/febgnd_<n>.png) for the pictures that have one, else the
         8-bit art upscaled x4 (the method "detail" of textures_hd.py).
         <hd>/work/ui_lang/<job>/in.png and prompt.txt.
  codex  Runs the Codex CLI (codex exec, image generation) on each job: the
         same image with the words of the language. raw.png is the image
         as generated.
  build  raw.png loses the flat border that the image generator adds for
         another aspect, is resized to the crop and moved by the whole pixel
         offset (at most 6) that matches the crop best outside the text
         boxes. The mask: the text boxes of the job, feathered; only the
         masked part is taken. The upscaled picture gets the result as an overlay; the
         8-bit art gets the result reduced to the source size (box filter)
         and mapped to the nearest color of the palette (only the indexes
         that the source art uses, never 254 or 255), where the reduced mask
         is at least 0.25. The buttons of the play screen: the 8-bit picture
         under a button gets the image at rest of the button sheet of the
         language (the game draws that image there), so that the upscaled
         picture shows there. The sheets of the language (FEBGND 70 + 4 and
         78 + 4 for Spanish) and the bitmaps of PORTAL.EGG with the language
         letter are whole pictures.
  --sheet writes <hd>/work/ui_lang/check/*.png: each job input, result and
         mask side by side, and the 8-bit result.
"""
import argparse
import concurrent.futures
import os
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import textures_hd as th  # noqa: E402
import ui_hd  # noqa: E402

CODEX = os.environ.get("CODEX", os.path.expandvars(r"%LOCALAPPDATA%\Programs\OpenAI\Codex\bin\codex.exe"))
LANG = "es"
LANG_INDEX = 4          # the language number of DTUGAME.EXE: e f g i 1 2 3 (0x145750)
EGG_LETTER = "1"        # the letter of the language in the names of PORTAL.EGG
SHEET_HI, SHEET_LO = 70, 78     # entry 70 + language, 78 + language (0x145718, 0x145734)

# The look of the words, for the prompt
ENGRAVED = "engraved serif capital letters, dark brown ink pressed into the wood, same depth and lighting"
GOLD = "raised golden serif capital letters with the same bevel, highlights and shadow"
CARVED = ("worn golden serif capitals chiselled into the plate: recessed letters with flat, rough faces, the dark "
          "shading of a carving on their upper inner edges, irregular worn edges, no raised bevel and no highlight "
          "on the letter faces, the same color and wear as the original letters")
BUTTON = "the same blocky sans-serif capital letters in pale gold with the same dark outline and bevel"
LABEL = "the same small serif capital letters"

# Jobs. kind "hd": picture n of FEBGND with an upscaled file. box: the
# text box (x, y, w, h) in pixels of the picture, where the words may change.
# kind "bm": 8-bit art only: ("febgnd", n) or ("egg", name, palette entry).
# rest: for the buttons of the play screen, (sheet, x0, y0): the image at
# rest in the sheet of the language, copied into the 8-bit picture under
# the button (box = the button rectangle).
JOBS = []


def hd(name, n, box, en, es, style, rest=None, margin=8, text=None, inset=None):
    if inset is not None:
        text = cells([box], inset)
    JOBS.append(dict(name=name, kind="hd", n=n, box=box, en=en, es=es, style=style, rest=rest, margin=margin,
                     text=text))


def bm(name, src, box, en, es, style, margin=3, out=None, text=None):
    JOBS.append(dict(name=name, kind="bm", src=src, box=box, en=en, es=es, style=style, margin=margin, out=out,
                     text=text))


def cells(pairs, inset):
    """The text boxes of the button images (x, y, w, h), inset by inset pixels."""
    return [(x + inset, y + inset, w - 2 * inset, h - 2 * inset) for x, y, w, h in pairs]


# character creation 1: the title, letters of a mock Greek alphabet (X for
# C, P for R): "XPEATION-XHA" for CREATION-CHA(racter)
hd("p2_title", 2, (262, 44, 140, 32), "XPEATION-XHA", "XPEAXION-ΠJ",
   "the same dark mock-Greek serif capitals painted on the parchment")
# character creation 4: name, portrait, alignment
hd("p5_name", 5, (52, 54, 136, 36), "NAME", "NOMBRE", ENGRAVED)
hd("p5_portrait", 5, (232, 64, 134, 34), "PORTRAIT", "RETRATO", ENGRAVED)
hd("p5_alignment", 5, (414, 86, 158, 34), "ALIGNMENT", "ALINEAMIENTO", ENGRAVED)
ALIGN = [("lg", "LAWFUL GOOD", "LEGAL BUENO", 168, 191), ("ng", "NEUTRAL GOOD", "NEUTRAL BUENO", 205, 227),
         ("ln", "LAWFUL NEUTRAL", "LEGAL NEUTRAL", 242, 263), ("nu", "TRUE NEUTRAL", "NEUTRAL AUTÉNTICO", 278, 300),
         ("cg", "CHAOTIC GOOD", "CAÓTICO BUENO", 315, 336), ("cn", "CHAOTIC NEUTRAL", "CAÓTICO NEUTRAL", 351, 373)]
for _c, _en, _es, _ya, _yb in ALIGN:
    hd("p5_" + _c, 5, (434, _ya, 128, _yb - _ya + 1), _en, _es, LABEL + ", as large and as bold as the original letters, dark on the lit brass plate", margin=6)
    # the selected label (s4_draw), lit
    bm("lb4" + _c, ("egg", "lb4%s_de" % _c, 5), None, _en, _es,
       LABEL + " (dark letters, as large and as bold as the original letters, filling the plate in the same way) on the dark plate lit from behind in orange; keep the plate, its rivets, its frame and its glow exactly where they are", out="lb4%s_d%s" % (_c, EGG_LETTER),
       text=[(12, 4, 142, 30)])
# the market
MARKET = [("buy", "BUY", "COMPRAR", (427, 15, 94, 50)), ("sel", "SELL", "VENDER", (532, 15, 94, 50)),
          ("wpn", "WEAPON", "ARMAS", (452, 81, 168, 63)), ("arm", "ARMOR", "ARMADURAS", (452, 174, 168, 64)),
          ("msc", "MISC.", "VARIOS", (453, 265, 166, 62))]
for _c, _en, _es, _r in MARKET:
    hd("p14_" + _c, 14, (_r[0] + 8, _r[1] + 8, _r[2] - 16, _r[3] - 16), _en, _es, GOLD + " on the wooden button", margin=10)
    bm("lbm" + _c, ("egg", "lbm%sde" % _c, 14), None, _en, _es, GOLD + " on the pressed wooden button",
       out="lbm%sd%s" % (_c, EGG_LETTER), text=[(10, 9, _r[2] - 20, _r[3] - 18)])
# the quantity window of the market (FEBGND 25)
hd("p25_yes", 25, (62, 266, 130, 42), "ACCEPT", "ACEPTAR", CARVED, margin=8)
hd("p25_rej", 25, (225, 267, 128, 42), "REJECT", "RECHAZAR", CARVED, margin=8)
bm("lbpyes", ("egg", "lbpyesde", 14), None, "ACCEPT", "ACEPTAR", CARVED + " (the button is pressed)",
   out="lbpyesd" + EGG_LETTER, text=[(12, 9, 121, 38)])
bm("lbprej", ("egg", "lbprejde", 14), None, "REJECT", "RECHAZAR", CARVED + " (the button is pressed)",
   out="lbprejd" + EGG_LETTER, text=[(12, 9, 118, 37)])

# The button sheets: pairs (pressed, at rest) of the buttons whose word
# changes, (x, y, w, h) of the pair in the sheet. M I R stay (Mapa,
# Inventario, Reposo); S (Stats) becomes C (Caracteristicas).
SHEET_JOBS_HI = [     # name, (pressed, at rest), English, Spanish
    ("s70_main_s", [(203, 2, 42, 38), (247, 2, 42, 38)], "S", "C"),
    ("s70_inv_s", [(203, 42, 39, 33), (247, 42, 39, 33)], "S", "C"),
    ("s70_drop", [(29, 78, 74, 33), (107, 78, 74, 33)], "DROP", "SOLTAR"),
    ("s70_use", [(184, 78, 77, 33), (264, 78, 77, 33)], "USE", "USAR"),
    ("s70_spells", [(344, 78, 76, 32), (424, 78, 76, 32)], "SPELLS", "CONJUROS"),
    ("s70_map_map", [(346, 115, 77, 33), (426, 115, 77, 33)], "MAP", "MAPA"),
    ("s70_map_rest", [(188, 115, 76, 33), (267, 115, 76, 33)], "REST", "REPOSO"),
    ("s70_map_stats", [(521, 41, 77, 33), (521, 78, 77, 33)], "STATS", "CARACT."),
    ("s70_st_map", [(353, 151, 79, 34), (435, 151, 79, 34)], "MAP", "MAPA"),
    ("s70_st_rest", [(193, 151, 77, 34), (273, 151, 77, 34)], "REST", "REPOSO"),
    ("s70_st_stats", [(521, 115, 79, 34), (521, 151, 79, 34)], "STATS", "CARACT."),
]
SHEET_JOBS_LO = [
    ("s78_mn_s", [(93, 39, 19, 16), (114, 39, 19, 16)], "S", "C"),
    ("s78_inv_s", [(443, 39, 16, 15), (462, 39, 16, 15)], "S", "C"),
    ("s78_mp_s", [(173, 2, 39, 17), (214, 2, 39, 17)], "S", "C"),
    ("s78_st_s", [(171, 21, 39, 16), (212, 21, 39, 16)], "S", "C"),
    ("s78_drop", [(480, 39, 39, 19), (521, 39, 39, 19)], "DROP", "SOLTAR"),
    ("s78_use", [(514, 21, 39, 17)], "USE", "USAR"),
]


def bounds(rects):
    x0 = min(r[0] for r in rects)
    y0 = min(r[1] for r in rects)
    return (x0, y0, max(r[0] + r[2] for r in rects) - x0, max(r[1] + r[3] for r in rects) - y0)


for _n, _c, _en, _es in SHEET_JOBS_HI:
    bm(_n, ("febgnd", SHEET_HI), bounds(_c), _en, _es,
       BUTTON + ". The image shows the same button twice (pressed and at rest): change the word on both",
       text=cells(_c, 4))
for _n, _c, _en, _es in SHEET_JOBS_LO:
    bm(_n, ("febgnd", SHEET_LO), bounds(_c), _en, _es,
       BUTTON + ". Tiny low resolution buttons; if the image shows the button twice, change the word on both",
       text=cells(_c, 2))

# The play screen: the buttons of the pictures (the button rectangles of
# dtudisp.c) and the image at rest in the sheet.
S = SHEET_HI + LANG_INDEX
s = SHEET_LO + LANG_INDEX
hd("p63_s", 63, (16, 348, 42, 38), "S", "C", BUTTON, inset=4, rest=(S, 247, 2), margin=4)
hd("p65_s", 65, (541, 312, 39, 33), "S", "C", BUTTON, inset=4, rest=(S, 247, 42), margin=4)
hd("p65_use", 65, (454, 354, 77, 33), "USE", "USAR", BUTTON, inset=4, rest=(S, 264, 78), margin=4)
hd("p65_drop", 65, (548, 354, 74, 33), "DROP", "SOLTAR", BUTTON, inset=4, rest=(S, 107, 78), margin=4)
hd("p65_spells", 65, (18, 324, 76, 32), "SPELLS", "CONJUROS", BUTTON, inset=4, rest=(S, 424, 78), margin=4)
hd("p64_map", 64, (454, 312, 77, 33), "MAP", "MAPA", BUTTON, inset=4, rest=(S, 426, 115), margin=4)
hd("p64_stats", 64, (454, 354, 77, 33), "STATS", "CARACT.", BUTTON, inset=4, rest=(S, 521, 78), margin=4)
hd("p64_rest", 64, (547, 354, 76, 33), "REST", "REPOSO", BUTTON, inset=4, rest=(S, 267, 115), margin=4)
hd("p67_map", 67, (453, 312, 79, 34), "MAP", "MAPA", BUTTON, inset=4, rest=(S, 435, 151), margin=4)
hd("p67_stats", 67, (454, 354, 78, 34), "STATS", "CARACT.", BUTTON, inset=4, rest=(S, 521, 151), margin=4)
hd("p67_rest", 67, (547, 354, 77, 34), "REST", "REPOSO", BUTTON, inset=4, rest=(S, 273, 151), margin=4)
hd("p57_s", 57, (8, 176, 19, 16), "S", "C", BUTTON, inset=0, rest=(s, 114, 39), margin=3)
hd("p58_s", 58, (227, 177, 39, 17), "S", "C", BUTTON, inset=0, rest=(s, 214, 2), margin=3)
hd("p61_s", 61, (227, 178, 39, 16), "S", "C", BUTTON, inset=0, rest=(s, 212, 21), margin=3)
for _p in (59, 60):
    hd("p%d_s" % _p, _p, (274, 156, 16, 15), "S", "C", BUTTON, inset=0, rest=(s, 462, 39), margin=3)
    hd("p%d_use" % _p, _p, (227, 178, 39, 17), "USE", "USAR", BUTTON, inset=0, rest=(s, 514, 21), margin=3)
    hd("p%d_drop" % _p, _p, (273, 178, 39, 19), "DROP", "SOLTAR", BUTTON, inset=0, rest=(s, 521, 39), margin=3)

PROMPT = """The attached image (in.png in the current directory, {w}x{h} pixels) is a crop of the interface art of a 1997 fantasy computer game. It shows the English word(s) "{en}".

Use your image generation tool to make an edited version of this exact image in which "{en}" is replaced by the Spanish "{es}": {style}. Same letter height as the original where the room allows (smaller or narrower letters only if the Spanish word does not fit), centered where the English word was, inside the same plate or button. Everything else (the plate or button, its frame, rivets, wood, stone, metal, the background, the borders, shadows, colors and the framing of the image) must stay as in the input. Keep the same aspect ratio. Spell the Spanish exactly as given, with its accents. Match the lettering of the original exactly: the same font, letter width and stroke weight, the same way the letters are made (carved into the surface or raised from it), the same wear, roughness and irregular edges, the same letter spacing. Do not make the letters cleaner, sharper, bolder or more beveled than the original letters.

Then copy the image file that the image tool wrote, unchanged, to raw.png in the current directory (do not resize, crop, composite or edit it in any other way). Do not change any other file. Reply with the path of raw.png only.
"""


def work_dir(hd_dir, job=None):
    d = os.path.join(hd_dir, "work", "ui_lang")
    return os.path.join(d, job["name"]) if job else d


class Art:
    """The 8-bit art and the palettes."""

    def __init__(self):
        import numpy as np
        self.np = np
        self.fb = ui_hd.Febgnd()
        self.game_pal = self.pal6(open(os.path.join(th.GAME_DIR, "PALETTE.256"), "rb").read()[:768])
        self.egg = None

    def pal6(self, p6):
        np = self.np
        return np.array([((c & 63) << 2) | ((c & 63) >> 4) for c in p6], np.float32).reshape(256, 3)

    def febgnd(self, n):
        """(indexes (h, w) uint8, palette (256, 3) float 0..255)"""
        np = self.np
        w, h, p6, pix = self.fb.picture(n)
        idx = np.frombuffer(pix, np.uint8).reshape(h, w).copy()
        # the in-game pictures and the button sheets are shown with the game palette
        pal = self.game_pal if 57 <= n <= 86 else self.pal6(self.fb.picture(ui_hd.PALETTE_OF.get(n, n))[2])
        return idx, pal

    def egg_bitmap(self, name, pal_n):
        import egg as E
        np = self.np
        if self.egg is None:
            self.egg = E.Egg(os.path.join(th.GAME_DIR, "PORTAL.EGG"))
            self.egg_by = {b.full_name.lower(): b for b in self.egg.bitmaps}
        b = self.egg_by[name.lower()]
        idx = np.frombuffer(bytes(self.egg.bitmap_pixels(b)), np.uint8).reshape(b.height, b.width).copy()
        return idx, self.pal6(self.fb.picture(pal_n)[2])

    def source(self, job):
        """The 8-bit art of a bm job: (indexes, palette, (x, y) of the art in the source)"""
        src = job["src"]
        if src[0] == "febgnd":
            idx, pal = self.febgnd(src[1])
        else:
            idx, pal = self.egg_bitmap(src[1], src[2])
        return idx, pal


def crop_rect(job, w, h):
    """The crop with the margin, clipped to the picture: (x, y, w, h)."""
    x, y, bw, bh = job["box"] if job["box"] else (0, 0, w, h)
    m = job["margin"]
    x0, y0 = max(0, x - m), max(0, y - m)
    x1, y1 = min(w, x + bw + m), min(h, y + bh + m)
    return x0, y0, x1 - x0, y1 - y0


def hd_file(hd_dir, n):
    return os.path.join(hd_dir, "ui", "febgnd_%d.png" % n)


def hd_scale(n):
    return ui_hd.SCALE_LOW if n in ui_hd.LOW else ui_hd.SCALE


def prep(args, jobs):
    import numpy as np
    from PIL import Image
    art = Art()
    ups = None
    for job in jobs:
        d = work_dir(args.hd, job)
        os.makedirs(d, exist_ok=True)
        if job["kind"] == "hd":
            n = job["n"]
            k = hd_scale(n)
            idx, _ = art.febgnd(n)
            cx, cy, cw, ch = crop_rect(job, idx.shape[1], idx.shape[0])
            im = Image.open(hd_file(args.hd, n)).convert("RGB").crop((cx * k, cy * k, (cx + cw) * k, (cy + ch) * k))
        else:
            idx, pal = art.source(job)
            cx, cy, cw, ch = crop_rect(job, idx.shape[1], idx.shape[0])
            rgb = pal[idx[cy:cy + ch, cx:cx + cw]] / 255.0
            if ups is None:
                ups = th.Upscaler(args.hd)
            up = ui_hd.upscale(ups, rgb.astype(np.float32), "detail", th.SCALE)[0]
            im = Image.fromarray(up, "RGB")
        im.save(os.path.join(d, "in.png"))
        with open(os.path.join(d, "prompt.txt"), "w", encoding="utf-8") as f:
            f.write(PROMPT.format(w=im.width, h=im.height, en=job["en"], es=job["es"], style=job["style"]))
        print("%-16s %dx%d" % (job["name"], im.width, im.height))


def run_codex(args, job):
    d = work_dir(args.hd, job)
    raw = os.path.join(d, "raw.png")
    if os.path.isfile(raw) and not args.force:
        return job["name"], "kept"
    if os.path.isfile(raw):
        os.remove(raw)
    with open(os.path.join(d, "prompt.txt"), "rb") as f:
        prompt = f.read()
    with open(os.path.join(d, "codex.log"), "wb") as log:
        subprocess.run([CODEX, "exec", "--skip-git-repo-check", "--ephemeral", "-s", "workspace-write",
                        "-C", d, "--image", "in.png", "-"], input=prompt, stdout=log, stderr=subprocess.STDOUT,
                       cwd=d, timeout=1200)
    return job["name"], "ok" if os.path.isfile(raw) else "NO raw.png (see codex.log)"


def codex(args, jobs):
    with concurrent.futures.ThreadPoolExecutor(args.jobs) as ex:
        for name, res in ex.map(lambda j: run_codex(args, j), jobs):
            print("%-16s %s" % (name, res), flush=True)


def blur(a, sigma):
    import numpy as np
    from PIL import Image, ImageFilter
    im = Image.fromarray(np.clip(a * 255.0, 0, 255).astype(np.uint8), "L")
    return np.asarray(im.filter(ImageFilter.GaussianBlur(sigma)), np.float32) / 255.0


def trim(im):
    """The image generator pads a picture of another aspect (white or a flat
    color): rows and columns at the edges with a flat color go."""
    import numpy as np
    a = np.asarray(im, np.float32)

    def flat(v):
        return v.std(0).max() < 4.0
    t, b, l, r = 0, a.shape[0], 0, a.shape[1]
    while b - t > 8 and flat(a[t, l:r]):
        t += 1
    while b - t > 8 and flat(a[b - 1, l:r]):
        b -= 1
    while r - l > 8 and flat(a[t:b, l]):
        l += 1
    while r - l > 8 and flat(a[t:b, r - 1]):
        r -= 1
    return im.crop((l, t, r, b)) if (t, b, l, r) != (0, a.shape[0], 0, a.shape[1]) else im


def align(a, b, inside, r=6):
    """b moved by the whole pixel offset (at most r) that matches a best outside the text box."""
    import numpy as np
    out = inside == 0
    if not out.any():
        return b
    best = None
    for dy in range(-r, r + 1):
        for dx in range(-r, r + 1):
            s = np.roll(np.roll(b, dy, 0), dx, 1)
            sel = np.ones(a.shape[:2], bool)
            if dy > 0: sel[:dy] = False
            if dy < 0: sel[dy:] = False
            if dx > 0: sel[:, :dx] = False
            if dx < 0: sel[:, dx:] = False
            v = np.abs(a - s).max(2)[out & sel].mean()
            if best is None or v < best[0]:
                best = (v, dx, dy)
    _, dx, dy = best
    s = np.roll(np.roll(b, dy, 0), dx, 1)
    # the rows and columns that came in from the other side keep the input
    if dy > 0: s[:dy] = a[:dy]
    if dy < 0: s[dy:] = a[dy:]
    if dx > 0: s[:, :dx] = a[:, :dx]
    if dx < 0: s[:, dx:] = a[:, dx:]
    return s


def result(args, job, k, cx, cy, w0, h0):
    """(input, result, mask, mean difference outside the text) at the crop
    size; mask 0..1. cx, cy: the crop in the source, w0, h0: the source size."""
    import numpy as np
    from PIL import Image
    d = work_dir(args.hd, job)
    a = np.asarray(Image.open(os.path.join(d, "in.png")).convert("RGB"), np.float32) / 255.0
    h, w = a.shape[:2]
    raw = trim(Image.open(os.path.join(d, "raw.png")).convert("RGB"))
    if abs(raw.width / raw.height - w / h) > 0.03 * w / h:
        print("  %s: the aspect of raw.png (%dx%d after the trim) is not that of in.png (%dx%d)"
              % (job["name"], raw.width, raw.height, w, h))
    b = np.asarray(raw.resize((w, h), Image.LANCZOS), np.float32) / 255.0
    inside = np.zeros(a.shape[:2], np.float32)
    for bx, by, bw, bh in job["text"] or [job["box"] or (0, 0, w0, h0)]:
        bx, by = bx - cx, by - cy
        inside[max(0, by * k):(by + bh) * k, max(0, bx * k):(bx + bw) * k] = 1
    b = align(a, b, inside)
    # the whole text box, feathered (a mask of the changed pixels left
    # parts of the English letters where the new letters cover them)
    m = blur(inside, k * 0.7)
    out_mean = float(np.abs(a - b).max(2)[inside == 0].mean() * 255) if (inside == 0).any() else 0.0
    return a, b, m, out_mean


def quantize(rgb, pal, allowed):
    """rgb (h, w, 3) 0..255 -> indexes, nearest of the allowed indexes"""
    import numpy as np
    cand = np.array(sorted(allowed), np.int32)
    p = pal[cand]
    d = ((rgb[:, :, None, :] - p[None, None]) ** 2).sum(-1)
    return cand[d.argmin(-1)].astype(np.uint8)


def reduce(a, k):
    h, w = a.shape[0] // k, a.shape[1] // k
    return a[:h * k, :w * k].reshape(h, k, w, k, *a.shape[2:]).mean((1, 3))


def write_patch(path, idx, alpha):
    import numpy as np
    from PIL import Image
    os.makedirs(os.path.dirname(path), exist_ok=True)
    Image.fromarray(np.stack([idx, alpha], -1).astype(np.uint8), "LA").save(path, optimize=True)


def build(args, jobs):
    import numpy as np
    from PIL import Image
    art = Art()
    out = os.path.join(args.hd, "lang", LANG)
    chk = os.path.join(work_dir(args.hd), "check")
    if args.sheet:
        os.makedirs(chk, exist_ok=True)
    pics = {}        # n -> [indexes, alpha (changed)]
    overlays = {}    # n -> RGBA float
    eggs = {}
    report = []

    def pic(n):
        if n not in pics:
            idx, pal = art.febgnd(n)
            pics[n] = [idx.copy(), np.zeros(idx.shape, np.uint8), pal, set(np.unique(idx).tolist()) - {254, 255}]
        return pics[n]

    # the sheets of the language: whole pictures, from the English sheets
    for base in (SHEET_HI, SHEET_LO):
        p = pic(base)
        pics[base + LANG_INDEX] = [p[0].copy(), np.full(p[0].shape, 255, np.uint8), p[2], p[3]]

    def eight_bit(job, a, b, m, idx, pal, allowed, cx, cy, k):
        """The 8-bit pixels of the result into idx (in place); returns the changed pixels."""
        comp = a * (1 - m[..., None]) + b * m[..., None]
        small = reduce(comp * 255.0, k)
        ms = reduce(m, k)
        h, w = small.shape[:2]
        q = quantize(small, pal, allowed)
        sel = ms >= 0.25
        region = idx[cy:cy + h, cx:cx + w]
        region[sel] = q[sel]
        return sel

    for job in [j for j in jobs if j["kind"] == "bm"] + [j for j in jobs if j["kind"] == "hd"]:
        if not os.path.isfile(os.path.join(work_dir(args.hd, job), "raw.png")):
            report.append("%-16s no raw.png" % job["name"])
            continue
        if job["kind"] == "bm":
            idx, pal = art.source(job)
            h0, w0 = idx.shape
            cx, cy, cw, ch = crop_rect(job, w0, h0)
            box = job["box"] or (0, 0, w0, h0)
            a, b, m, om = result(args, job, th.SCALE, cx, cy, w0, h0)
            src = job["src"]
            if src[0] == "febgnd":
                p = pics[src[1] + LANG_INDEX]
                sel = eight_bit(job, a, b, m, p[0], p[2], p[3], cx, cy, th.SCALE)
            else:
                allowed = set(np.unique(idx).tolist()) - {254, 255}
                new = idx.copy()
                sel = eight_bit(job, a, b, m, new, pal, allowed, cx, cy, th.SCALE)
                eggs[job["out"]] = new
            report.append("%-16s bm  changed %5d px, outside the box diff %.1f" % (job["name"], sel.sum(), om))
        else:
            n = job["n"]
            k = hd_scale(n)
            p = pic(n)
            h0, w0 = p[0].shape
            cx, cy, cw, ch = crop_rect(job, w0, h0)
            box = job["box"]
            a, b, m, om = result(args, job, k, cx, cy, w0, h0)
            if n not in overlays:
                im = Image.open(hd_file(args.hd, n))
                overlays[n] = np.zeros((im.height, im.width, 4), np.float32)
            ov = overlays[n][cy * k:(cy + ch) * k, cx * k:(cx + cw) * k]
            ov[..., :3] = ov[..., :3] * (1 - m[..., None]) + b * m[..., None]
            ov[..., 3] = np.maximum(ov[..., 3], m)
            if job["rest"]:
                sn, x0, y0 = job["rest"]
                x, y, w, h = box
                p[0][y:y + h, x:x + w] = pics[sn][0][y0:y0 + h, x0:x0 + w]
                p[1][y:y + h, x:x + w] = 255
                nsel = w * h
            else:
                sel = eight_bit(job, a, b, m, p[0], p[2], p[3], cx, cy, k)
                p[1][cy:cy + sel.shape[0], cx:cx + sel.shape[1]][sel] = 255
                nsel = int(sel.sum())
            report.append("%-16s hd  changed %5d px, outside the box diff %.1f" % (job["name"], nsel, om))
        if args.sheet:
            row = np.concatenate([a, b, np.repeat(m[..., None], 3, 2), a * (1 - m[..., None]) + b * m[..., None]], 1)
            Image.fromarray((row * 255 + 0.5).astype(np.uint8), "RGB").save(os.path.join(chk, job["name"] + ".png"))

    for n, (idx, alpha, pal, _) in pics.items():
        if alpha.any():
            write_patch(os.path.join(out, "orig", "febgnd_%d.png" % n), np.where(alpha > 0, idx, 0), alpha)
            if args.sheet:
                Image.fromarray(pal[idx].astype(np.uint8), "RGB").save(os.path.join(chk, "orig_febgnd_%d.png" % n))
    for name, idx in eggs.items():
        write_patch(os.path.join(out, "orig", "egg_%s.png" % name), idx, np.full(idx.shape, 255, np.uint8))
    for n, ov in overlays.items():
        os.makedirs(os.path.join(out, "ui"), exist_ok=True)
        a = ov[..., 3:]
        rgb = np.where(a > 0, ov[..., :3], 0)
        img = np.concatenate([rgb, a], -1)
        Image.fromarray((img * 255 + 0.5).astype(np.uint8), "RGBA").save(
            os.path.join(out, "ui", "febgnd_%d.png" % n), optimize=True)
    print("\n".join(report))


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("step", choices=("prep", "codex", "build", "list"))
    ap.add_argument("--only", help="job names, comma separated")
    ap.add_argument("--hd", default=th.HD_DIR, help="hd directory (default gamedata/hd)")
    ap.add_argument("--jobs", type=int, default=6, help="Codex runs at the same time")
    ap.add_argument("--force", action="store_true", help="run Codex again for a job with a raw.png")
    ap.add_argument("--sheet", action="store_true", help="check images in <hd>/work/ui_lang/check")
    args = ap.parse_args()
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    jobs = JOBS
    if args.only:
        want = set(args.only.split(","))
        jobs = [j for j in JOBS if j["name"] in want]
    if args.step == "list":
        for j in JOBS:
            where = "febgnd %d" % j["n"] if j["kind"] == "hd" else " ".join(str(v) for v in j["src"][:2])
            print("%-16s %-3s %-16s %-26s %-16s %s" % (j["name"], j["kind"], where, j["box"], j["en"], j["es"]))
        return
    if args.step != "codex":
        ui_hd.ensure_venv(args.hd)
    {"prep": prep, "codex": codex, "build": build}[args.step](args, jobs)


if __name__ == "__main__":
    main()
