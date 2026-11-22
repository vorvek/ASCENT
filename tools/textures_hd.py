#!/usr/bin/env python3
"""Make an upscaled (4 times the size) copy of every bitmap of the EGG archives.

Output: <hd>/textures/<egg>/<name>.png, RGBA 8-bit, 4 times the width and the
height of the bitmap. <egg> is the archive base name in lower case, <name> is
the bitmap name of the EGG in lower case with '#' replaced by '_'. Alpha is
255 for a bitmap without the transparent flag; with it, the mask of color 255
upscaled with smooth edges (0 outside, 255 inside, a ramp of about one pixel
between). <hd>/textures/<egg>/index.txt has one "name w h" line per file
(the EGG name with '#', the original size). The default <hd> is gamedata/hd,
which git ignores. Never commit the output: it is derived from the game data.

  python -I tools/textures_hd.py [--setup] [--only portal,bane] [--method NAME]
  python -I tools/textures_hd.py --compare      (comparison sheets of the methods)
  python -I tools/textures_hd.py --check portal (quality checks of a finished set)
  python -I tools/textures_hd.py --out DIR --only kobolds --names flame8,well1
                                (only these bitmaps again, into a copy of a set)

--setup makes a virtual environment in <hd>/venv (PyTorch with CUDA, spandrel,
Pillow, numpy) and downloads the models into <hd>/models. The tool runs itself
again with the Python of that environment. The method and the checks are in
docs/hd_textures.md.

Steps for each bitmap (interface art, flag 0x40, is skipped):
  1. palette colors (PALETTE.256); transparent pixels get the color of their
     opaque neighbors, filled outward ring by ring, so the upscale makes no
     dark halo,
  2. padding: tiling textures (wall textures of the tables, and every 64x64
     bitmap) are padded by wrapping with TILE_PAD pixels (a 3x3 tiling of a
     64x64 texture), the others by mirroring with PAD pixels,
  3. upscale x4 (the method, on the GPU), crop the padding,
  4. alpha: the opaque mask (same padding) is blurred, upscaled bicubic and
     cut at 50 percent with a ramp of one output pixel (smooth_alpha),
  5. the mean color of the opaque pixels is moved back to the mean of the
     original (a constant offset per channel),
  6. animations (frames base#N with a static part, anim_groups): the pixels
     that change little or not at all take the result of one reference
     frame plus the color change, so that the static parts do not shimmer
     (anim_merge).
Equal bitmaps (same pixels, size, flags and tiling rule) are upscaled once;
the other files are hard links (or copies) of the first one.
"""
import argparse
import glob
import hashlib
import os
import shutil
import struct
import subprocess
import sys
import time
import urllib.request

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, ".."))
HD_DIR = os.path.join(ROOT, "gamedata", "hd")
GAME_DIR = "E:/UNDAMONTAN/DTU"
UNPACKED = os.path.join(ROOT, "gamedata", "unpacked")
MODELS_DIR = None    # --models; else <out>/models, then gamedata/hd/models
sys.path.insert(0, HERE)
import egg  # noqa: E402

VERSION = 4          # bump when the processing changes; written to .method, so
                     # that a run makes the files of an archive again
                     # 2: wall textures use the "detail" method
                     # 3: tiling textures padded by TILE_PAD (a 3x3 tiling)
                     # 4: soft alpha edges (ALPHA_VERSION), animations with
                     #    a common static part (ANIM_VERSION)
CACHE_VERSION = 2    # part of the cache key of every bitmap; the key of a
                     # tiling bitmap also has TILE_PAD, so a change of the
                     # tiling padding does not redo the other bitmaps
# The key of a bitmap with transparency has ALPHA_VERSION, the key of a
# frame of an animation has ANIM_VERSION, so a change of one of them redoes
# only those bitmaps.
ALPHA_VERSION = 3    # 3: the mask is blurred, upscaled and cut with a
                     # 1 pixel ramp (smooth_alpha)
ANIM_VERSION = 1     # 1: one reference frame for the pixels that change
                     # little or not at all (anim_merge)
# smooth_alpha: the sigma of the gaussian blur of the mask, source pixels
ALPHA_SIGMA = 1.0
# anim_merge: a pixel whose color differs from the reference frame by more
# than ANIM_LIGHT (of 255, largest channel) is a changing pixel. The result
# of the reference is used fully from ANIM_RAMP0 + ANIM_RAMP source pixels
# away from the changing pixels, not at all up to ANIM_RAMP0. An animation
# needs ANIM_MIN_STATIC of its opaque pixels equal in all frames (a torch in
# a niche: 64 to 98 percent; lava and waterfalls change everywhere and keep
# the result of each frame).
ANIM_LIGHT = 40
# the color difference to the reference is blurred with this sigma (source
# pixels): the light of a flame is drawn as a dither that changes from frame
# to frame, which would show as a grain at 4x
ANIM_LIGHT_SIGMA = 1.0
ANIM_RAMP0 = 0.5
ANIM_RAMP = 2.0
ANIM_MIN_STATIC = 0.3
SCALE = 4
PAD = 16             # source pixels of padding on each side
TILE_PAD = 64        # the same for a tiling bitmap, by wrapping: a 64x64
                     # texture is upscaled as the center of a 3x3 tiling, so
                     # the receptive field of the model never reaches the end
                     # of the padding and the edges of the result match

# The effect colors (docs/re/effectcolors.md): the game draws these texels
# with a color from the screen, not with their palette color (a bright red,
# 63 0 0 for most of them). The upscale fills them with the colors around
# them, as the transparent pixels, so that the red does not bleed into the
# texels next to them. The game marks them from the original pixels.
EFFECT_COLORS = (0x4E, 0x4F, 0x6D, 0x6E, 0x6F, 0xBE, 0xBF, 0xCF)


def has_effect_colors(m):
    return any(c in m.pix for c in EFFECT_COLORS)

# name -> (file, url, sha256, source, license)
MODELS = {
    "ultrasharp": ("4x-UltraSharp.pth",
                   "https://huggingface.co/Kim2091/UltraSharp/resolve/main/4x-UltraSharp.pth",
                   "a5812231fc936b42af08a5edba784195495d303d5b3248c24489ef0c4021fe01",
                   "Kim2091, ESRGAN", "CC BY-NC-SA 4.0"),
    "siax": ("4x_NMKD-Siax_200k.pth",
             "https://huggingface.co/uwg/upscaler/resolve/main/ESRGAN/4x_NMKD-Siax_200k.pth",
             "560424d9f68625713fc47e9e7289a98aabe1d744e1cd6a9ae5a35e9957fd127e",
             "Nmkd, ESRGAN", "not checked (evaluation only)"),
    "superscale-sp": ("4x_NMKD-Superscale-SP_178000_G.pth",
                      "https://huggingface.co/uwg/upscaler/resolve/main/ESRGAN/4x_NMKD-Superscale-SP_178000_G.pth",
                      "1d1b0078fe71446e0469d8d4df59e96baa80d83cda600d68237d655830821bcc",
                      "Nmkd, ESRGAN", "not checked (evaluation only)"),
    "remacri": ("4x_foolhardy_Remacri.pth",
                "https://huggingface.co/uwg/upscaler/resolve/main/ESRGAN/4x_foolhardy_Remacri.pth",
                "e1a73bd89c2da1ae494774746398689048b5a892bd9653e146713f9df8bca86a",
                "FoolhardyVEVO, ESRGAN", "not checked (evaluation only)"),
    "x4plus": ("RealESRGAN_x4plus.pth",
               "https://github.com/xinntao/Real-ESRGAN/releases/download/v0.1.0/RealESRGAN_x4plus.pth",
               "4fa0d38905f75ac06eb49a7951b426670021be3018265fd191d2125df9d682f1",
               "xinntao, Real-ESRGAN", "BSD-3-Clause"),
    "x4plus-anime": ("RealESRGAN_x4plus_anime_6B.pth",
                     "https://github.com/xinntao/Real-ESRGAN/releases/download/v0.2.2.4/RealESRGAN_x4plus_anime_6B.pth",
                     "f872d837d3c90ed2e05227bed711af5671a6fd1c9f7d7e91c911a61f155e99da",
                     "xinntao, Real-ESRGAN", "BSD-3-Clause"),
    "animevideov3": ("realesr-animevideov3.pth",
                     "https://github.com/xinntao/Real-ESRGAN/releases/download/v0.2.5.0/realesr-animevideov3.pth",
                     "b8a8376811077954d82ca3fcf476f1ac3da3e8a68a4f4d71363008000a18b75d",
                     "xinntao, Real-ESRGAN (SRVGG compact)", "BSD-3-Clause"),
}
PLAIN_METHODS = ["nearest", "scale4x", "bicubic"]
DEFAULT_METHOD = "combo"

# Bitmaps for --compare: (archive, name)
COMPARE_SET = [
    ("portal", "grey4"), ("portal", "wood2"), ("portal", "bflor09"),
    ("portal", "eroc10"), ("portal", "eroc50"), ("portal", "lava#0"),
    ("portal", "hum_mt04"), ("portal", "dlf_ff01"), ("portal", "min_b10"),
    ("hmc", "pa_a01"), ("portal", "flamswrd"), ("portal", "hands"),
    ("portal", "firbal1#0"), ("portal", "gencmail"), ("portal", "genscrl2"),
    ("portal", "potion#0"), ("portal", "exp19#0"), ("portal", "chain1#0"),
    ("portal", "pebble#0"), ("portal", "spray2#0"), ("portal", "cursor"),
    ("portal", "swtch08a"),
]


# ---------------------------------------------------------------- archives

def archive_paths():
    """(base name lower case, path) of every archive, E: first for the three of the install."""
    out = {}
    for p in sorted(glob.glob(os.path.join(UNPACKED, "*.EGG"))):
        out[os.path.splitext(os.path.basename(p))[0].lower()] = p
    for n in ("PORTAL", "HMC", "HMT"):
        p = os.path.join(GAME_DIR, n + ".EGG")
        if os.path.isfile(p):
            out[n.lower()] = p
    return out


class Bmp:
    __slots__ = ("name", "base", "frame", "w", "h", "flags", "pix", "error")


def read_archive(path):
    """Bitmaps of an EGG file. Handles the old layout (20-byte sound headers)."""
    with open(path, "rb") as f:
        d = f.read()
    nb, ns, nm = struct.unpack_from("<3I", d, 0)
    base = None
    for shdr in (24, 20):
        b = 12 + 17 * nb + shdr * ns + egg.MODEL_HDR_SIZE * nm
        if nm == 0 or egg.cstr(d[b - egg.MODEL_HDR_SIZE * nm + 0x3D:][:13]).endswith(".pof"):
            base = b
            break
    if base is None:
        raise egg.EggError("%s: cannot tell the layout" % path)
    out = []
    for i in range(nb):
        name, dflags, w, h, flags, avg, off = egg.BITMAP_HDR.unpack_from(d, 12 + 17 * i)
        m = Bmp()
        m.base = egg.cstr(name)
        m.frame = dflags & 0x3F if dflags & egg.DBM_FLAG_ABM else None
        m.name = m.base if m.frame is None else "%s#%d" % (m.base, m.frame)
        m.w = w + (256 if dflags & egg.DBM_FLAG_LARGE else 0)
        m.h = h
        m.flags = flags
        m.error = None
        m.pix = None
        fo = base + off
        try:
            if flags & egg.BM_RLE:
                m.pix = egg.rle_decode(d, fo, m.w, m.h, m.name)
            else:
                m.pix = d[fo:fo + m.w * m.h]
                if len(m.pix) != m.w * m.h:
                    raise egg.EggError("short data")
        except egg.EggError as e:
            m.error = str(e)
        out.append(m)
    return out


def wall_names():
    """Base names of the bitmaps that the tables use as wall textures."""
    names = set()
    bins = glob.glob(os.path.join(UNPACKED, "*.BIN")) + glob.glob(os.path.join(GAME_DIR, "*.BIN"))
    for p in bins:
        with open(p, "rb") as f:
            raw = f.read()
        if len(raw) <= 13:
            continue
        try:
            text = egg.decode_bin(raw)
        except Exception:
            continue
        if isinstance(text, tuple):
            text = text[-1]
        if isinstance(text, bytes):
            text = text.decode("latin-1")
        section, remap_off = None, False
        for line in text.splitlines():
            s = line.split(";", 1)[0].strip()
            if not s:
                continue
            if s.startswith("!BITMAP_REMAP_OFF"):
                remap_off = True
            elif s.startswith("!BITMAP_REMAP_ON"):
                remap_off = False
            if s.startswith("$"):
                section = s.split()[0]
            words = s.split()
            if section == "$TEXTURES" and not remap_off:
                take = True
            elif section == "$WCLIP":
                take = True
            elif section == "$ECLIP":
                take = "obj_eclip=1" not in s
            else:
                take = False
            if take:
                for wd in words:
                    lw = wd.lower()
                    if lw.endswith(".bbm") or lw.endswith(".abm"):
                        names.add(lw[:-4])
    return names


def load_palette():
    return egg.load_palette(os.path.join(GAME_DIR, "PALETTE.256"))


def out_name(name):
    return name.lower().replace("#", "_")


# ---------------------------------------------------------------- environment

def venv_python(hd):
    return os.path.join(hd, "venv", "Scripts" if os.name == "nt" else "bin",
                        "python.exe" if os.name == "nt" else "python")


def setup(hd):
    vpy = venv_python(hd)
    if not os.path.isfile(vpy):
        uv = shutil.which("uv")
        if uv:
            subprocess.check_call([uv, "venv", "--python", "3.11", os.path.join(hd, "venv")])
        else:
            subprocess.check_call([sys.executable, "-m", "venv", os.path.join(hd, "venv")])
    uv = shutil.which("uv")
    pip = [uv, "pip", "install", "--python", vpy] if uv else [vpy, "-m", "pip", "install"]
    subprocess.check_call(pip + ["torch", "--index-url", "https://download.pytorch.org/whl/cu126"])
    subprocess.check_call(pip + ["spandrel", "pillow", "numpy"])
    md = os.path.join(hd, "models")
    os.makedirs(md, exist_ok=True)
    for key, (fn, url, sha, _, _) in MODELS.items():
        p = os.path.join(md, fn)
        if not os.path.isfile(p):
            print("download", url)
            urllib.request.urlretrieve(url, p + ".part")
            os.replace(p + ".part", p)
        h = hashlib.sha256(open(p, "rb").read()).hexdigest()
        if h != sha:
            sys.exit("%s: wrong SHA-256 %s" % (p, h))
    print("environment and models are in", hd)


def ensure_venv(hd):
    """Run this script again with the Python of the virtual environment."""
    try:
        import numpy  # noqa: F401
        import torch  # noqa: F401
        import PIL  # noqa: F401
        return
    except ImportError:
        pass
    vpy = venv_python(hd)
    if not os.path.isfile(vpy):
        vpy = venv_python(HD_DIR)       # an --out directory with no venv of its own
    if not os.path.isfile(vpy):
        sys.exit("no virtual environment in %s: run with --setup first" % hd)
    if os.path.abspath(sys.executable).lower() == os.path.abspath(vpy).lower():
        sys.exit("the virtual environment lacks numpy, torch or Pillow: run with --setup")
    r = subprocess.call([vpy, "-I", os.path.abspath(__file__)] + sys.argv[1:])
    sys.exit(r)


# ---------------------------------------------------------------- image work
# Everything below runs in the virtual environment (numpy, torch, Pillow).

COMBO = ("detail", "x4plus")   # (wall textures, everything else)

# The "detail" method: x4plus as the base, plus DETAIL_STRENGTH times the
# fine detail of UltraSharp (UltraSharp minus a gaussian blur of itself with
# DETAIL_SIGMA output pixels). A plain mix of the two models gives double
# edges, because the models put edges in slightly different places; the
# high frequencies alone add the grain and the cracks of UltraSharp to the
# shapes of x4plus. The blur runs on the padded image, so tiling textures
# stay seamless.
DETAIL_STRENGTH = 0.6
DETAIL_SIGMA = 2.0
# The "face" method: the same with less of the UltraSharp detail. On the
# dithered faces of the interface 0.6 brings back the dither as a grain,
# x4plus alone loses the wrinkles and the beard (docs/hd_textures.md).
FACE_DETAIL_STRENGTH = 0.15


def sub_method(method, wall):
    """The method for one bitmap. combo: UltraSharp for wall textures, x4plus for the rest."""
    if method == "combo":
        return COMBO[0] if wall else COMBO[1]
    return method


class Upscaler:
    def __init__(self, hd):
        import torch
        self.torch = torch
        self.hd = hd
        self.models = {}
        torch.backends.cudnn.benchmark = False
        torch.backends.cudnn.deterministic = True

    def model(self, method):
        if method not in self.models:
            import spandrel
            p = os.path.join(MODELS_DIR or os.path.join(self.hd, "models"), MODELS[method][0])
            if not os.path.isfile(p):
                p = os.path.join(HD_DIR, "models", MODELS[method][0])
            if not os.path.isfile(p):
                sys.exit("%s: missing, run with --setup" % p)
            desc = spandrel.ModelLoader().load_from_file(p)
            assert desc.scale == SCALE
            self.models[method] = desc.model.cuda().eval().half()
        return self.models[method]

    def run(self, batch, method):
        """batch: numpy float32 (N, H, W, 3) in 0..1 -> (N, 4H, 4W, 3)."""
        import numpy as np
        torch = self.torch
        if method == "nearest":
            return batch.repeat(SCALE, axis=1).repeat(SCALE, axis=2)
        if method == "scale4x":
            return np.stack([scale4x(b) for b in batch])
        x = torch.from_numpy(np.ascontiguousarray(batch.transpose(0, 3, 1, 2))).cuda()
        with torch.no_grad():
            if method in ("detail", "face"):
                strength = DETAIL_STRENGTH if method == "detail" else FACE_DETAIL_STRENGTH
                base = self.model("x4plus")(x.half()).float()
                sharp = self.model("ultrasharp")(x.half()).float()
                r = int(DETAIL_SIGMA * 3 + 0.5)
                k = torch.exp(-0.5 * (torch.arange(-r, r + 1, device=x.device,
                                                   dtype=torch.float32) / DETAIL_SIGMA) ** 2)
                k = k / k.sum()
                c = sharp.shape[1]
                kh = k.view(1, 1, 1, -1).repeat(c, 1, 1, 1)
                kv = k.view(1, 1, -1, 1).repeat(c, 1, 1, 1)
                blur = torch.nn.functional.pad(sharp, (r, r, 0, 0), mode="reflect")
                blur = torch.nn.functional.conv2d(blur, kh, groups=c)
                blur = torch.nn.functional.pad(blur, (0, 0, r, r), mode="reflect")
                blur = torch.nn.functional.conv2d(blur, kv, groups=c)
                y = base + strength * (sharp - blur)
            elif method == "bicubic":
                y = torch.nn.functional.interpolate(x, scale_factor=SCALE, mode="bicubic",
                                                    align_corners=False)
            elif method in MODELS:
                y = self.model(method)(x.half()).float()
            else:
                sys.exit("unknown method %s" % method)
        return y.clamp(0, 1).permute(0, 2, 3, 1).cpu().numpy()


def scale2x(a):
    """Scale2x (EPX) on an (H, W) integer image, borders by edge replication."""
    import numpy as np
    p = np.pad(a, 1, mode="edge")
    B, D, E, F, H = p[:-2, 1:-1], p[1:-1, :-2], p[1:-1, 1:-1], p[1:-1, 2:], p[2:, 1:-1]
    c = (B != H) & (D != F)
    e0 = np.where(c & (D == B), D, E)
    e1 = np.where(c & (B == F), F, E)
    e2 = np.where(c & (D == H), D, E)
    e3 = np.where(c & (H == F), F, E)
    h, w = a.shape
    o = np.empty((2 * h, 2 * w), a.dtype)
    o[0::2, 0::2], o[0::2, 1::2], o[1::2, 0::2], o[1::2, 1::2] = e0, e1, e2, e3
    return o


def scale4x(img):
    import numpy as np
    q = np.rint(img * 255).astype(np.int32)
    packed = (q[..., 0] << 16) | (q[..., 1] << 8) | q[..., 2]
    s = scale2x(scale2x(packed))
    return np.stack([(s >> 16) & 255, (s >> 8) & 255, s & 255], -1).astype(np.float32) / 255


def fill_transparent(rgb, opaque, wrap):
    """Give transparent pixels the mean color of their filled neighbors, ring by ring."""
    import numpy as np
    rgb = rgb.copy()
    known = opaque.copy()
    if known.all() or not known.any():
        return rgb
    while not known.all():
        if wrap:
            kp = np.pad(known, 1, mode="wrap")
            cp = np.pad(rgb * known[..., None], ((1, 1), (1, 1), (0, 0)), mode="wrap")
        else:
            kp = np.pad(known, 1, mode="constant")
            cp = np.pad(rgb * known[..., None], ((1, 1), (1, 1), (0, 0)), mode="constant")
        h, w = known.shape
        cnt = np.zeros((h, w), np.float32)
        acc = np.zeros((h, w, 3), np.float32)
        for dy in range(3):
            for dx in range(3):
                if dy == 1 and dx == 1:
                    continue
                cnt += kp[dy:dy + h, dx:dx + w]
                acc += cp[dy:dy + h, dx:dx + w]
        new = (~known) & (cnt > 0)
        rgb[new] = acc[new] / cnt[new][:, None]
        known = known | new
    return rgb


def pad_of(tiling):
    """Source pixels of padding on each side."""
    return TILE_PAD if tiling else PAD


def pad_img(a, tiling):
    import numpy as np
    p = pad_of(tiling)
    pw = ((p, p), (p, p)) + ((0, 0),) * (a.ndim - 2)
    return np.pad(a, pw, mode="wrap" if tiling else "symmetric")


def gauss_blur(a, sigma):
    """Separable gaussian blur of a 2D float image (edges by reflection)."""
    import numpy as np
    r = max(1, int(sigma * 3 + 0.5))
    k = np.exp(-0.5 * (np.arange(-r, r + 1) / sigma) ** 2)
    k = (k / k.sum()).astype(np.float32)
    p = np.pad(a, r, mode="reflect")
    h, w = a.shape
    t = sum(k[i] * p[:, i:i + w] for i in range(2 * r + 1))
    return sum(k[i] * t[i:i + h, :] for i in range(2 * r + 1))


def smooth_alpha(opaque, tiling):
    """Alpha (float 0..1) at SCALE times the size of the opaque mask, with smooth edges.

    The mask (padded as the colors) is blurred with ALPHA_SIGMA, upscaled
    bicubic, and cut at 0.5 with a ramp of one output pixel: the signed
    distance to the 0.5 line is estimated as (f - 0.5) / |grad f|. A plain
    upscale of the 1-bit mask keeps the stair steps of the source pixels;
    the blur makes them a smooth line. A source pixel that the blur would
    lose (a line or a dot of 1 pixel, opaque or transparent) is kept: a pixel
    with such a pixel of the same kind beside it (a line) keeps its square, so
    that a line stays a line (a disk at each pixel made a dotted line of the
    transparent top row of arch4); a single pixel (a dot, the pixels of a
    dither) keeps a disk of one source pixel across at its center."""
    import numpy as np
    import torch
    S = SCALE
    h, w = opaque.shape
    p = pad_of(tiling)
    b = gauss_blur(pad_img(opaque.astype(np.float32), tiling), ALPHA_SIGMA)
    f = torch.nn.functional.interpolate(torch.from_numpy(np.ascontiguousarray(b))[None, None],
                                        scale_factor=S, mode="bicubic", align_corners=False)[0, 0].numpy()
    gy, gx = np.gradient(f)
    c = p * S
    f = f[c:c + h * S, c:c + w * S]
    g = np.hypot(gy, gx)[c:c + h * S, c:c + w * S]
    d = np.clip((f - 0.5) / np.maximum(g, 1e-3), -S, S)
    bs = b[p:p + h, p:p + w]
    t = (np.arange(S) + 0.5) / S - 0.5
    disk = np.tile(0.5 * S - S * np.hypot(t[:, None], t[None, :]), (h, w))
    for weak, sign in ((opaque & (bs < 0.5), 1.0), (~opaque & (bs >= 0.5), -1.0)):
        if not weak.any():
            continue
        wp = np.pad(weak, 1, mode="wrap" if tiling else "constant")
        line = weak & (wp[:-2, 1:-1] | wp[2:, 1:-1] | wp[1:-1, :-2] | wp[1:-1, 2:])
        keep = np.where(line.repeat(S, 0).repeat(S, 1), 1.0, disk)
        sel = weak.repeat(S, 0).repeat(S, 1)
        if sign > 0:
            d = np.where(sel, np.maximum(d, keep), d)
        else:
            d = np.where(sel, np.minimum(d, -keep), d)
    return np.clip(0.5 + d, 0.0, 1.0)


class Job:
    __slots__ = ("key", "w", "h", "tiling", "rgb", "opaque", "has_alpha", "result", "meth", "idx")


def make_job(m, pal, tiling, method):
    import numpy as np
    idx = np.frombuffer(m.pix, np.uint8).reshape(m.h, m.w)
    rgb = pal[idx]
    if m.flags & egg.BM_TRANSPARENT:
        opaque = idx != 255
    else:
        opaque = np.ones(idx.shape, bool)
    effect = np.isin(idx, EFFECT_COLORS) & opaque
    j = Job()
    j.w, j.h, j.tiling = m.w, m.h, tiling
    j.has_alpha = not opaque.all()
    j.opaque = opaque
    j.rgb = fill_transparent(rgb, opaque & ~effect, tiling) if j.has_alpha or effect.any() else rgb
    h = hashlib.sha1()
    h.update(("%s|%d|%d|%d|%d|%d|" % (method, CACHE_VERSION, m.w, m.h, tiling,
                                      j.has_alpha)).encode())
    if tiling:
        h.update(("wrap%d|" % TILE_PAD).encode())
    h.update(opaque.tobytes())
    h.update(np.ascontiguousarray(rgb).tobytes())
    if j.has_alpha:
        h.update(("alpha%d|" % ALPHA_VERSION).encode())
    if effect.any():
        # a new key for the bitmaps with effect colors only: the others
        # keep their files and cache entries
        h.update(b"effect|")
        h.update(effect.tobytes())
    j.key = h.hexdigest()
    j.idx = idx
    j.result = None
    j.meth = method
    return j


def finish(j, up_rgb, method):
    """Crop, alpha, mean color. Returns (RGBA uint8, color diff before, after)."""
    import numpy as np
    W, H = j.w * SCALE, j.h * SCALE
    c = pad_of(j.tiling) * SCALE
    rgb = up_rgb[c:c + H, c:c + W] * 255.0
    if j.has_alpha:
        if method == "nearest":
            soft = j.opaque.repeat(SCALE, 0).repeat(SCALE, 1).astype(np.float32)
        else:
            soft = smooth_alpha(j.opaque, j.tiling)
            if not (soft >= 0.5).any():
                soft = j.opaque.repeat(SCALE, 0).repeat(SCALE, 1).astype(np.float32)
    else:
        soft = np.ones((H, W), np.float32)
    a = soft >= 0.5
    orig = j.rgb[j.opaque] * 255.0
    mo = orig.mean(0) if len(orig) else np.zeros(3)
    sel = rgb[a] if a.any() else rgb.reshape(-1, 3)
    before = np.abs(sel.mean(0) - mo).max() if len(orig) else 0.0
    rgb = rgb + (mo - sel.mean(0)) if len(orig) else rgb
    rgb = np.clip(np.rint(rgb), 0, 255).astype(np.uint8)
    after = np.abs(rgb[a].astype(np.float64).mean(0) - mo).max() if len(orig) and a.any() else 0.0
    out = np.empty((H, W, 4), np.uint8)
    out[..., :3] = rgb
    out[..., 3] = np.rint(soft * 255).astype(np.uint8)
    return out, float(before), float(after)


def upscale_jobs(jobs, ups, progress=None):
    """Upscale the color of jobs in batches of equal padded size."""
    import numpy as np
    groups = {}
    for j in jobs:
        groups.setdefault((j.meth, j.h, j.w, j.tiling), []).append(j)
    done = 0
    for (meth, h, w, tiling), lst in sorted(groups.items()):
        ph, pw = h + 2 * pad_of(tiling), w + 2 * pad_of(tiling)
        n = max(1, min(64, 400000 // (ph * pw)))
        for i in range(0, len(lst), n):
            part = lst[i:i + n]
            batch = np.stack([pad_img(j.rgb.astype(np.float32), j.tiling) for j in part])
            up = ups.run(batch, meth)
            for j, u in zip(part, up):
                j.result = finish(j, u, meth)
            done += len(part)
            if progress:
                progress(done)


# ---------------------------------------------------------------- animations

def anim_groups(bitmaps):
    """base name -> frames (in frame order) of the animations that anim_merge changes:
    two or more frames of one size that are not all equal, with at least
    ANIM_MIN_STATIC of the opaque pixels equal in all frames."""
    import numpy as np
    groups = {}
    for m in bitmaps:
        if m.frame is not None and not m.error and not m.flags & egg.BM_UNKNOWN_40:
            groups.setdefault(m.base, []).append(m)
    out = {}
    for base, fl in groups.items():
        fl.sort(key=lambda m: m.frame)
        if len(fl) < 2 or len({(m.w, m.h) for m in fl}) != 1:
            continue
        idx = np.stack([np.frombuffer(m.pix, np.uint8) for m in fl])
        static = (idx == idx[0]).all(0)
        opaque = np.stack([(i != 255) | (not m.flags & egg.BM_TRANSPARENT)
                           for i, m in zip(idx, fl)]).any(0)
        if not static.all() and opaque.any() and (static & opaque).sum() >= ANIM_MIN_STATIC * opaque.sum():
            out[base] = fl
    return out


def anim_key(raw_keys, i):
    """The cache key of frame i of an animation: its own key and the keys of all frames."""
    h = hashlib.sha1(("anim%d|%g|%g|%g|%g|%d|" % (ANIM_VERSION, ANIM_LIGHT, ANIM_LIGHT_SIGMA, ANIM_RAMP0,
                                                   ANIM_RAMP, i)).encode())
    for k in raw_keys:
        h.update(k.encode())
    return h.hexdigest()


def static_weight(static, tiling):
    """Weight (float, SCALE times the size) of the common static result: 0 up to
    ANIM_RAMP0 source pixels from a pixel that changes, 1 from ANIM_RAMP0 +
    ANIM_RAMP on, linear between. The distance is an octagonal estimate
    (alternate 4- and 8-neighbor steps at the output size), wrapped for a
    tiling bitmap."""
    import numpy as np
    S = SCALE
    dyn = (~static).repeat(S, 0).repeat(S, 1)
    steps = int(np.ceil((ANIM_RAMP0 + ANIM_RAMP) * S)) + 1
    dist = np.full(dyn.shape, float(steps), np.float32)
    dist[dyn] = 0.0
    cur = dyn.copy()
    for k in range(1, steps + 1):
        if tiling:
            p = np.pad(cur, 1, mode="wrap")
        else:
            p = np.pad(cur, 1, mode="edge")
        n = p[1:-1, :-2] | p[1:-1, 2:] | p[:-2, 1:-1] | p[2:, 1:-1]
        if k % 2 == 0:
            n |= p[:-2, :-2] | p[:-2, 2:] | p[2:, :-2] | p[2:, 2:]
        new = n & ~cur
        dist[new] = k
        cur |= n
    # the distance from the edge of the changing pixels, in source pixels
    d = np.maximum(dist - 0.5, 0) / S
    return np.clip((d - ANIM_RAMP0) / ANIM_RAMP, 0.0, 1.0)


def anim_merge(jobs, results):
    """Make the frames of one animation agree where the source pixels agree.

    Each frame is upscaled alone, so the parts that do not move (the niche
    and the holder of a torch, the wall behind) come out a little different
    in each frame and shimmer when the animation plays. In a torch the light
    of the flame also changes the colors of the niche by a few shades from
    frame to frame; the model turns these small changes into changes of the
    shapes. So one frame is the reference (the frame closest to the median
    of the frames). Where a frame differs from the reference by up to
    ANIM_LIGHT, its result is the result of the reference plus the bicubic
    upscale of the color difference: the same detail in every frame, the
    colors of the frame. Where it differs more (the flame), the frame keeps
    its own result; between the two the weight ramps over ANIM_RAMP source
    pixels (static_weight), so no seam shows. A pixel that no frame changes
    gets the result of the reference in every frame. Equal frames stay
    equal. jobs: the jobs of the frames (rgb, opaque); results: RGBA uint8
    per frame. Returns the new list."""
    import numpy as np
    import torch
    S = SCALE
    rgb = np.stack([j.rgb for j in jobs]) * 255.0
    op = np.stack([j.opaque for j in jobs])
    med = np.median(rgb, axis=0)
    r = int(np.argmin([np.abs(c - med)[o].sum() for c, o in zip(rgb, op)]))
    tiling = jobs[0].tiling
    p = pad_of(tiling)
    h, w = jobs[0].h, jobs[0].w
    ref = results[r].astype(np.float32)
    out = []
    for k, j in enumerate(jobs):
        if j.key == jobs[r].key:
            out.append(results[r])
            continue
        d = rgb[k] - rgb[r]
        big = ((np.abs(d).max(-1) > ANIM_LIGHT) & (op[k] | op[r])) | (op[k] != op[r])
        d[big] = 0.0
        d[~(op[k] | op[r])] = 0.0
        wt = static_weight(~big, tiling)[..., None] if big.any() else np.ones((h * S, w * S, 1), np.float32)
        dp = pad_img(d.astype(np.float32), tiling)
        dp = np.stack([gauss_blur(dp[..., c], ANIM_LIGHT_SIGMA) for c in range(3)], -1)
        dt = torch.from_numpy(np.ascontiguousarray(dp.transpose(2, 0, 1)))[None]
        du = torch.nn.functional.interpolate(dt, scale_factor=S, mode="bicubic",
                                             align_corners=False)[0].numpy().transpose(1, 2, 0)
        du = du[p * S:p * S + h * S, p * S:p * S + w * S]
        light = ref.copy()
        light[..., :3] += du
        own = results[k].astype(np.float32)
        out.append(np.clip(np.rint(wt * light + (1 - wt) * own), 0, 255).astype(np.uint8))
    return out


def save_png(path, rgba):
    from PIL import Image
    tmp = path + ".tmp"
    Image.fromarray(rgba, "RGBA").save(tmp, format="PNG", compress_level=6)
    os.replace(tmp, path)


# ---------------------------------------------------------------- checks

def rolled(m, dy, dx):
    import numpy as np
    r = Bmp()
    for k in Bmp.__slots__:
        setattr(r, k, getattr(m, k))
    r.pix = np.roll(np.frombuffer(m.pix, np.uint8).reshape(m.h, m.w), (dy, dx), (0, 1)).tobytes()
    return r


def seam_check(seams, pal, walls, outdir, args):
    """Upscale each wall texture again, rolled by half its size, so that its wrap seam
    lies in the middle of the picture, and roll the result back. With seamless
    wrapping, the two results agree at the seam as well as they agree elsewhere.
    The control does the same with mirror padding (no wrapping)."""
    import numpy as np
    meth = open(os.path.join(outdir, ".method")).read().split()[0]
    ups = Upscaler(args.out)
    S = SCALE

    def band(img):
        h, w = img.shape[:2]
        b = np.zeros((h, w), bool)
        b[:, [0, 1, w - 2, w - 1]] = True
        b[[0, 1, h - 2, h - 1], :] = True
        return b

    def run(tiling, lst):
        jobs = [make_job(rolled(m, m.h // 2, m.w // 2), pal, tiling,
                         sub_method(meth, m.base.lower() in walls)) for m, _ in lst]
        upscale_jobs(jobs, ups)
        out = []
        for (m, up), j in zip(lst, jobs):
            r = np.roll(j.result[0], (-m.h // 2 * S, -m.w // 2 * S), (0, 1)).astype(np.float32)
            d = np.abs(up[..., :3].astype(np.float32) - r[..., :3]).mean(-1)
            bd = band(d)
            seam = d[bd].mean()
            inner = d[~bd].mean()
            # step across the seam in the stored file and in the rolled result
            st = lambda a: (np.abs(a[:, 0, :3].astype(np.float32) - a[:, -1, :3]).mean() +
                            np.abs(a[0, :, :3].astype(np.float32) - a[-1, :, :3]).mean()) / 2
            out.append((seam, inner, st(up), st(r), m.name))
        return out

    res = run(True, seams)
    sd = np.array([x[0] for x in res])
    idf = np.array([x[1] for x in res])
    ratio = np.array([x[2] / max(x[3], 0.5) for x in res])
    w = max(res, key=lambda x: x[0])
    print("  seams (%d wall textures, rolled by half and upscaled again): mean difference at the seam "
          "%.2f, inside %.2f (of 255); seam step file / rolled: median %.2f, 5%% %.2f, 95%% %.2f; "
          "worst seam %s %.2f" % (len(res), sd.mean(), idf.mean(), np.median(ratio),
                                  np.percentile(ratio, 5), np.percentile(ratio, 95), w[4], w[0]))
    ctl = seams[:200]
    # control: files made with mirror padding, compared with the rolled mirror results
    cjobs = [make_job(m, pal, False, sub_method(meth, m.base.lower() in walls)) for m, _ in ctl]
    upscale_jobs(cjobs, ups)
    cres = run(False, [(m, j.result[0]) for (m, _), j in zip(ctl, cjobs)])
    csd = np.array([x[0] for x in cres])
    cid = np.array([x[1] for x in cres])
    cratio = np.array([x[2] / max(x[3], 0.5) for x in cres])
    print("  seam control (%d, mirror padding): difference at the seam %.2f, inside %.2f; "
          "seam step median %.2f, 95%% %.2f" % (len(cres), csd.mean(), cid.mean(),
                                              np.median(cratio), np.percentile(cratio, 95)))


def frame_diff(a, b):
    import numpy as np
    pa = a[..., :3].astype(np.float32) * (a[..., 3:] / 255.0)
    pb = b[..., :3].astype(np.float32) * (b[..., 3:] / 255.0)
    return float(np.abs(pa - pb).mean())


def orig_rgba(m, pal):
    import numpy as np
    idx = np.frombuffer(m.pix, np.uint8).reshape(m.h, m.w)
    out = np.empty((m.h, m.w, 4), np.uint8)
    out[..., :3] = np.rint(pal[idx] * 255).astype(np.uint8)
    t = (idx == 255) if m.flags & egg.BM_TRANSPARENT else np.zeros(idx.shape, bool)
    out[..., 3] = np.where(t, 0, 255)
    return out


def halo_score(o4, up):
    """Mean luminance of opaque pixels next to transparency, output minus original."""
    import numpy as np

    def edge_lum(img):
        op = img[..., 3] >= 128     # a soft edge counts from half
        p = np.pad(op, 1, mode="constant", constant_values=True)
        nb = p[:-2, 1:-1] & p[2:, 1:-1] & p[1:-1, :-2] & p[1:-1, 2:]
        e = op & ~nb
        if not e.any():
            return None
        return (img[..., :3][e].astype(np.float32) @ np.array([.299, .587, .114])).mean()
    a, b = edge_lum(o4), edge_lum(up)
    return None if a is None or b is None else float(b - a)


# ---------------------------------------------------------------- main work

def process_archive(eggname, path, args, ups, pal, walls, cache_dir, stats):
    import numpy as np
    from PIL import Image
    t0 = time.time()
    outdir = os.path.join(args.out, "textures", eggname)
    os.makedirs(outdir, exist_ok=True)
    src_mtime = os.path.getmtime(path)
    mfile = os.path.join(outdir, ".method")
    old_method = open(mfile).read().strip() if os.path.isfile(mfile) else None
    stale = old_method not in (None, "%s %d" % (args.method, VERSION))
    bitmaps = read_archive(path)
    index, todo, skipped_iface, bad, kept = [], [], 0, [], 0
    for m in bitmaps:
        if m.flags & egg.BM_UNKNOWN_40:
            skipped_iface += 1
            continue
        if m.error:
            bad.append(m.name)
            continue
        dst = os.path.join(outdir, out_name(m.name) + ".png")
        index.append((m, dst))
        if args.names is not None:
            if m.base.lower() in args.names:
                todo.append((m, dst))
            else:
                kept += 1
            continue
        if (not args.force and not stale and os.path.isfile(dst) and os.path.getmtime(dst) > src_mtime
                and not (args.redo_effects and has_effect_colors(m))):
            kept += 1
            continue
        todo.append((m, dst))
    # build jobs, reuse the cache of equal bitmaps
    groups = anim_groups(bitmaps)

    def job_of(m):
        wall = m.base.lower() in walls
        tiling = (m.w == 64 and m.h == 64) or wall
        return make_job(m, pal, tiling, sub_method(args.method, wall))

    jobs, pending, linked = {}, [], 0
    anims = {}          # base -> (jobs of all frames, final keys, {frame number: [dst]})
    for m, dst in todo:
        if m.base in groups:
            if m.base not in anims:
                gj = [job_of(f) for f in groups[m.base]]
                keys = [anim_key([g.key for g in gj], i) for i in range(len(gj))]
                anims[m.base] = (gj, keys, {})
            gj, keys, want = anims[m.base]
            i = [f.frame for f in groups[m.base]].index(m.frame)
            cp = os.path.join(cache_dir, keys[i] + ".png")
            if os.path.isfile(cp):
                link(cp, dst)
                linked += 1
                continue
            want.setdefault(i, []).append(dst)
            continue
        j = job_of(m)
        cp = os.path.join(cache_dir, j.key + ".png")
        if os.path.isfile(cp):
            link(cp, dst)
            linked += 1
            continue
        if j.key not in jobs:
            jobs[j.key] = (j, [])
        jobs[j.key][1].append(dst)
    anims = {b: a for b, a in anims.items() if a[2]}
    total = len(jobs) + sum(len({g.key for g in a[0]}) for a in anims.values())
    last = [time.time()]

    def progress(done):
        if time.time() - last[0] > 10 or done == total:
            last[0] = time.time()
            el = time.time() - t0
            print("  %s: %d/%d upscaled, %.0f s, %.2f per s" % (eggname, done, total, el, done / max(el, 1e-6)),
                  flush=True)

    # in chunks so an interrupted run keeps its work
    lst = list(jobs.values())
    report = stats.setdefault("color", [])
    for i in range(0, len(lst), 256):
        chunk = lst[i:i + 256]
        upscale_jobs([j for j, _ in chunk], ups,
                     lambda d, base=i: progress(base + d))
        for j, dsts in chunk:
            rgba, before, after = j.result
            cp = os.path.join(cache_dir, j.key + ".png")
            save_png(cp, rgba)
            for d in dsts:
                link(cp, d)
            report.append((eggname, os.path.basename(dsts[0]), before, after))
            j.result = j.rgb = None
    # animations: all frames of a group, then the common static part
    done = len(lst)
    for base, (gj, keys, want) in sorted(anims.items()):
        uniq = {}
        for g in gj:
            uniq.setdefault(g.key, g)
        upscale_jobs(list(uniq.values()), ups)
        done += len(uniq)
        progress(done)
        res = anim_merge(gj, [uniq[g.key].result[0] for g in gj])
        for i, dsts in want.items():
            cp = os.path.join(cache_dir, keys[i] + ".png")
            save_png(cp, res[i])
            for d in dsts:
                link(cp, d)
            r = uniq[gj[i].key].result
            report.append((eggname, os.path.basename(dsts[0]), r[1], r[2]))
    with open(os.path.join(outdir, "index.txt"), "w", newline="\n") as f:
        for m, dst in index:
            f.write("%s %d %d\n" % (m.name, m.w, m.h))
    if args.names is None:      # a run of some names leaves the other files as they are
        with open(mfile, "w") as f:
            f.write("%s %d\n" % (args.method, VERSION))
    el = time.time() - t0
    size = sum(os.path.getsize(d) for _, d in index if os.path.isfile(d))
    print("%s: %d files (%d kept, %d from the cache, %d upscaled), %d interface skipped, "
          "%d bad %s, %.1f s, %.1f MB" % (eggname, len(index), kept, linked, total, skipped_iface,
                                          len(bad), bad, el, size / 1e6), flush=True)
    stats.setdefault("archives", []).append((eggname, len(index), total, el, size))


def link(src, dst):
    if os.path.exists(dst):
        os.remove(dst)
    try:
        os.link(src, dst)
    except OSError:
        shutil.copyfile(src, dst)


def run_all(args):
    import numpy as np
    pal = np.frombuffer(load_palette(), np.uint8).reshape(256, 3).astype(np.float32) / 255.0
    archives = archive_paths()
    names = list(archives)
    # PORTAL and HMC first, then the level sets, then the player sets
    order = ["portal", "hmc"] + sorted(n for n in names if len(n) > 3 and n not in ("portal",)) + \
        sorted(n for n in names if len(n) <= 3 and n != "hmc")
    if args.only:
        want = [w.strip().lower() for w in args.only.split(",") if w.strip()]
        for w in want:
            if w not in archives:
                sys.exit("no archive %s" % w)
        order = [n for n in order if n in want]
    walls = wall_names()
    cache_dir = os.path.join(args.out, "textures", ".cache")
    os.makedirs(cache_dir, exist_ok=True)
    ups = Upscaler(args.out)
    stats = {}
    t0 = time.time()
    print("method %s, %d archives, %d wall texture names" % (args.method, len(order), len(walls)), flush=True)
    for n in order:
        process_archive(n, archives[n], args, ups, pal, walls, cache_dir, stats)
    c = stats.get("color", [])
    if c:
        worst = max(c, key=lambda r: r[3])
        print("color: largest mean difference %.2f before the offset, %.2f after (%s/%s)" % (
            max(r[2] for r in c), worst[3], worst[0], worst[1]))
    print("total %.1f s" % (time.time() - t0))


# ---------------------------------------------------------------- checks of a set

def check_set(args):
    """Seam, halo, color and animation checks of finished archives."""
    import numpy as np
    from PIL import Image
    pal = np.frombuffer(load_palette(), np.uint8).reshape(256, 3).astype(np.float32) / 255.0
    archives = archive_paths()
    walls = wall_names()
    for name in [w.strip().lower() for w in args.check.split(",")]:
        outdir = os.path.join(args.out, "textures", name)
        bm = [m for m in read_archive(archives[name]) if not m.error and not m.flags & egg.BM_UNKNOWN_40]
        seams, seams_o, colors, halos, sizes_ok = [], [], [], [], 0
        alpha_ok, alpha_soft = 0, 0
        frames = {}
        ups = {}
        for m in bm:
            p = os.path.join(outdir, out_name(m.name) + ".png")
            im = Image.open(p)
            up = np.asarray(im)
            o = orig_rgba(m, pal)
            ok = im.mode == "RGBA" and up.shape == (m.h * 4, m.w * 4, 4)
            sizes_ok += ok
            a = up[..., 3]
            binary = bool(np.isin(a, (0, 255)).all())
            alpha_ok += binary or bool(m.flags & egg.BM_TRANSPARENT)
            alpha_soft += not binary
            tiling = (m.w == 64 and m.h == 64) or m.base.lower() in walls
            op = o[..., 3] > 0
            if tiling and op.all() and m.base.lower() in walls and m.w == m.h == 64:
                seams.append((m, up))
            if op.any():
                mo = o[..., :3][op].astype(np.float64).mean(0)
                mu = up[..., :3][a >= 128].astype(np.float64).mean(0) if (a >= 128).any() else mo
                colors.append((float(np.abs(mu - mo).max()), m.name))
            if not op.all() and op.any():
                h = halo_score(o.repeat(4, 0).repeat(4, 1), up)
                if h is not None:
                    halos.append((h, m.name))
            if m.frame is not None:
                frames.setdefault(m.base, []).append((m.frame, m.name))
            ups[m.name] = (o, up)
        print("%s: %d files, size and mode right: %d, alpha 0/255 or a transparent bitmap: %d, soft alpha: %d"
              % (name, len(bm), sizes_ok, alpha_ok, alpha_soft))
        if seams:
            seam_check(seams, pal, walls, outdir, args)
        if colors:
            w = max(colors)
            print("  color: largest mean channel difference %.2f of 255 (%s), median %.2f" % (
                w[0], w[1], np.median([c for c, _ in colors])))
        if halos:
            hv = np.array([h for h, _ in halos])
            w = min(halos)
            print("  halo (edge luminance output - original, %d sprites): median %.2f, min %.2f (%s), max %.2f (%s), "
                  "|d| > 5: %d" % (len(hv), np.median(hv), w[0], w[1], max(halos)[0], max(halos)[1],
                                   int((np.abs(hv) > 5).sum())))
        rat, same_bad, npairs, worst_pairs = [], 0, 0, []
        for base, fl in frames.items():
            fl.sort()
            for (_, a), (_, b) in zip(fl, fl[1:]):
                oa, ua = ups[a]
                ob, ub = ups[b]
                if oa.shape != ob.shape:
                    continue
                npairs += 1
                do = frame_diff(oa.repeat(4, 0).repeat(4, 1), ob.repeat(4, 0).repeat(4, 1))
                du = frame_diff(ua, ub)
                if do == 0:
                    same_bad += du != 0
                else:
                    rat.append(du / do)
                    worst_pairs.append((du / do, do, du, a, b))
        if rat:
            rat = np.array(rat)
            print("  animation: %d frame pairs, diff output / diff original median %.2f, 5%% %.2f, 95%% %.2f, "
                  "max %.2f; equal frames that differ after: %d" % (
                      npairs, np.median(rat), np.percentile(rat, 5), np.percentile(rat, 95), rat.max(), same_bad))
            worst_pairs.sort(reverse=True)
            print("  largest ratios: " + ", ".join("%s/%s %.2f (%.2f -> %.2f)" % (a, b, r, do, du)
                                                     for r, do, du, a, b in worst_pairs[:4]))
        # the animations of anim_merge: the output of the pixels that no
        # frame changes, frame to frame (0 in the middle of a static area)
        st = []
        for base, fl in anim_groups(bm).items():
            idx = np.stack([np.frombuffer(m.pix, np.uint8).reshape(m.h, m.w) for m in fl])
            static = ((idx == idx[0]).all(0)).repeat(4, 0).repeat(4, 1)
            outs = [ups[m.name][1].astype(np.float32) for m in fl]
            for ua, ub in zip(outs, outs[1:]):
                sel = static & (ua[..., 3] == 255) & (ub[..., 3] == 255)
                if sel.any():
                    st.append((float(np.abs(ua[..., :3] - ub[..., :3]).max(-1)[sel].mean()), base))
        if st:
            print("  animations with a static part: %d frame pairs, mean difference of the static pixels "
                  "median %.2f, max %.2f (%s)" % (len(st), np.median([v for v, _ in st]), max(st)[0], max(st)[1]))


# ---------------------------------------------------------------- comparison sheets

def compare(args):
    import numpy as np
    from PIL import Image, ImageDraw
    pal = np.frombuffer(load_palette(), np.uint8).reshape(256, 3).astype(np.float32) / 255.0
    archives = archive_paths()
    walls = wall_names()
    sample = []
    cache = {}
    for eg, name in COMPARE_SET:
        if eg not in cache:
            cache[eg] = {m.name.lower(): m for m in read_archive(archives[eg])}
        m = cache[eg].get(name.lower())
        if m is None:
            print("compare: %s/%s not found" % (eg, name))
            continue
        sample.append(m)
    outdir = os.path.join(args.out, "work", "texcompare")
    os.makedirs(outdir, exist_ok=True)
    methods = args.compare_methods.split(",")
    cell = 256
    cols = 5
    rows = (len(sample) + cols - 1) // cols
    results = {}
    for meth in methods:
        ups = Upscaler(args.out)
        jobs = []
        for m in sample:
            wall = m.base.lower() in walls
            tiling = (m.w == 64 and m.h == 64) or wall
            jobs.append(make_job(m, pal, tiling, sub_method(meth, wall)))
        t = time.time()
        upscale_jobs(jobs, ups)
        print("%s: %.2f s for %d bitmaps" % (meth, time.time() - t, len(jobs)))
        sheet = Image.new("RGB", (cols * cell, rows * (cell + 14)), (255, 0, 255))
        dr = ImageDraw.Draw(sheet)
        results[meth] = []
        for k, (m, j) in enumerate(zip(sample, jobs)):
            rgba = j.result[0]
            results[meth].append(rgba)
            im = Image.fromarray(rgba, "RGBA")
            bg = Image.new("RGBA", im.size, (255, 0, 255, 255))
            im = Image.alpha_composite(bg, im).convert("RGB")
            s = min(cell / im.width, cell / im.height)
            im = im.resize((max(1, int(im.width * s)), max(1, int(im.height * s))), Image.LANCZOS)
            x, y = (k % cols) * cell, (k // cols) * (cell + 14)
            sheet.paste(im, (x, y + 14))
            dr.rectangle((x, y, x + cell, y + 13), fill=(0, 0, 0))
            dr.text((x + 2, y + 1), "%s %dx%d %s" % (m.name, m.w, m.h, meth), fill=(255, 255, 255))
        sheet.save(os.path.join(outdir, "sheet_%s.png" % meth))
        del ups
    # detail sheet: the center 24x24 source pixels of ten bitmaps, output shown at 2x,
    # one row per method
    pick = list(range(len(sample)))[:10]
    crop = 24
    cw = crop * SCALE * 2
    det = Image.new("RGB", (len(pick) * (cw + 4) + 110, len(methods) * (cw + 4)), (40, 40, 40))
    dr = ImageDraw.Draw(det)
    for r, meth in enumerate(methods):
        dr.text((4, r * (cw + 4) + cw // 2), meth, fill=(255, 255, 255))
        for c, k in enumerate(pick):
            m = sample[k]
            rgba = results[meth][k]
            cy = max(0, (m.h - crop) // 2) * SCALE
            cx = max(0, (m.w - crop) // 2) * SCALE
            im = Image.fromarray(rgba[cy:cy + crop * SCALE, cx:cx + crop * SCALE], "RGBA")
            bg = Image.new("RGBA", im.size, (255, 0, 255, 255))
            im = Image.alpha_composite(bg, im).convert("RGB").resize(
                (im.width * 2, im.height * 2), Image.NEAREST)
            det.paste(im, (110 + c * (cw + 4), r * (cw + 4)))
    det.save(os.path.join(outdir, "detail.png"))
    print("sheets in", outdir)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--setup", action="store_true", help="make the virtual environment, download the models")
    ap.add_argument("--only", help="archives, comma separated (portal,hmc,bane,...)")
    ap.add_argument("--method", default=DEFAULT_METHOD,
                    help="upscale method: combo (default), detail, %s" % ", ".join(list(MODELS) + PLAIN_METHODS))
    ap.add_argument("--out", default=HD_DIR, help="hd directory (default gamedata/hd)")
    ap.add_argument("--force", action="store_true", help="redo existing files")
    ap.add_argument("--redo-effects", action="store_true",
                    help="redo the files of the bitmaps with effect colors (a set made before the fill of these colors)")
    ap.add_argument("--names", help="make only the bitmaps of these base names (comma separated, "
                    "frames included) again, in the archives of --only or all; the other files and "
                    "the .method file stay")
    ap.add_argument("--unpacked", help="directory of the unpacked archives (default gamedata/unpacked)")
    ap.add_argument("--models", help="directory of the models (default <out>/models)")
    ap.add_argument("--compare", action="store_true", help="make comparison sheets in <hd>/work/texcompare")
    ap.add_argument("--compare-methods", default="nearest,scale4x,bicubic," + ",".join(MODELS) + ",combo")
    ap.add_argument("--check", help="check finished archives (comma separated)")
    args = ap.parse_args()
    global UNPACKED, MODELS_DIR
    if args.unpacked:
        UNPACKED = args.unpacked
    if args.models:
        MODELS_DIR = args.models
    args.names = ({n.strip().lower() for n in args.names.split(",") if n.strip()}
                  if args.names else None)
    os.makedirs(args.out, exist_ok=True)
    if args.method not in ("combo", "detail") and args.method not in MODELS and args.method not in PLAIN_METHODS:
        sys.exit("unknown method %s" % args.method)
    if args.setup:
        setup(args.out)
        if not (args.only or args.compare or args.check):
            return
    ensure_venv(args.out)
    if args.compare:
        compare(args)
    elif args.check:
        check_set(args)
    else:
        run_all(args)


if __name__ == "__main__":
    main()
