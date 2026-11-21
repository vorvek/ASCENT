#!/usr/bin/env python3
"""Make upscaled copies of the movies of Descent to Undermountain.

For each MVE file this tool:
  1. writes the decoded pictures as PNG files (ffmpeg; its decoder gives the
     same pixels as engine/main/mvedec.c, see docs/formats/mve.md),
  2. upscales them with Real-ESRGAN (realesrgan-ncnn-vulkan, on the GPU),
  3. encodes them as an MPEG-1 video stream NAME.m1v, one picture for each
     picture of the MVE file, with no sound.

The port (engine/main/dtumovie.c) shows the pictures of NAME.m1v in place of
the MVE pictures. The sound and the timing still come from the MVE file, so
the sound is the original sound. The output directory is gamedata/hd, which
git ignores. Never commit the output: it is derived from the game data.

  python -I tools/movies_hd.py [--setup] [MOVIE ...]

--setup downloads the Windows release of realesrgan-ncnn-vulkan (Real-ESRGAN
v0.2.5.0, BSD-3-Clause, with its models) into gamedata/hd/realesrgan.
Without MOVIE arguments all five movies are made.

The port finds the files with -hddir gamedata/hd, or in a "hd" directory of
a data directory or of the current directory.
"""
import argparse
import hashlib
import os
import shutil
import struct
import subprocess
import sys
import time
import urllib.request
import zipfile

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, ".."))
HD_DIR = os.path.join(ROOT, "gamedata", "hd")
GAME_DIRS = [os.path.join(ROOT, "gamedata", "cd", "DTU"), "E:/UNDAMONTAN/DTU"]
MOVIES = ["LOGO22S.MVE", "INTRO2.MVE", "TLOGOS.MVE", "FLAM1.MVE", "CREDITV1.MVE"]

ESRGAN_URL = ("https://github.com/xinntao/Real-ESRGAN/releases/download/v0.2.5.0/"
              "realesrgan-ncnn-vulkan-20220424-windows.zip")
ESRGAN_SHA256 = "abc02804e17982a3be33675e4d471e91ea374e65b70167abc09e31acb412802d"


def find_tool(name, extra):
    for p in extra:
        if os.path.isfile(p):
            return p
    p = shutil.which(name)
    if p:
        return p
    sys.exit("cannot find %s" % name)


def mve_picture_count(path):
    """The number of opcodes 0x11 (decoded pictures) of an MVE file."""
    with open(path, "rb") as f:
        d = f.read()
    if d[:20] != b"Interplay MVE File\x1a\x00":
        sys.exit("%s is not an MVE file" % path)
    p, n = 26, 0
    while p + 4 <= len(d):
        clen, ctype = struct.unpack_from("<HH", d, p)
        p += 4
        end = p + clen
        q = p
        while q + 4 <= end:
            olen, op = struct.unpack_from("<HB", d, q)
            if op == 0x11:
                n += 1
            q += 4 + olen
        p = end
        if ctype == 5:
            break
    return n


def setup(hd_dir):
    dl = os.path.join(hd_dir, "download")
    os.makedirs(dl, exist_ok=True)
    zpath = os.path.join(dl, os.path.basename(ESRGAN_URL))
    if not os.path.isfile(zpath):
        print("download", ESRGAN_URL)
        urllib.request.urlretrieve(ESRGAN_URL, zpath)
    h = hashlib.sha256(open(zpath, "rb").read()).hexdigest()
    if h != ESRGAN_SHA256:
        sys.exit("%s: wrong SHA-256 %s" % (zpath, h))
    dest = os.path.join(hd_dir, "realesrgan")
    os.makedirs(dest, exist_ok=True)
    with zipfile.ZipFile(zpath) as z:
        z.extractall(dest)
    print("Real-ESRGAN is in", dest)


def run(cmd):
    r = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    if r.returncode != 0:
        print(r.stdout[-3000:])
        sys.exit("failed: %s" % " ".join(cmd))
    return r.stdout


def make(movie, args, ffmpeg, esrgan):
    src = None
    for d in args.game_dir:
        p = os.path.join(d, movie)
        if os.path.isfile(p):
            src = p
            break
    if not src:
        print("%s: not found" % movie)
        return
    base = os.path.splitext(movie)[0]
    work = os.path.join(args.out, "work", base)
    fin, fup = os.path.join(work, "in"), os.path.join(work, "up")
    for d in (fin, fup):
        shutil.rmtree(d, ignore_errors=True)
        os.makedirs(d)
    out = os.path.join(args.out, base + ".m1v")
    count = mve_picture_count(src)

    t0 = time.time()
    run([ffmpeg, "-v", "error", "-y", "-i", src, "-map", "0:v", "-vsync", "0",
         "-start_number", "0", os.path.join(fin, "%05d.png")])
    n_in = len(os.listdir(fin))
    if n_in != count:
        sys.exit("%s: ffmpeg wrote %d pictures, the file has %d" % (movie, n_in, count))
    t1 = time.time()
    run([esrgan, "-i", fin, "-o", fup, "-n", args.model, "-s", str(args.scale), "-f", "png"])
    n_up = len(os.listdir(fup))
    if n_up != count:
        sys.exit("%s: Real-ESRGAN wrote %d pictures of %d" % (movie, n_up, count))
    t2 = time.time()
    # One MPEG-1 picture for each MVE picture. The rate in the stream (30) is
    # not used: the player takes the timing from the MVE file.
    run([ffmpeg, "-v", "error", "-y", "-framerate", "30", "-start_number", "0",
         "-i", os.path.join(fup, "%05d.png"), "-c:v", "mpeg1video", "-q:v", str(args.quality),
         "-g", "15", "-bf", "0", "-pix_fmt", "yuv420p", "-f", "mpeg1video", out])
    t3 = time.time()
    if not args.keep:
        shutil.rmtree(work, ignore_errors=True)
    print("%s: %d pictures, decode %.1f s, upscale %.1f s (%.3f s per picture), encode %.1f s; "
          "%s %.1f MB -> %s %.1f MB" % (
              movie, count, t1 - t0, t2 - t1, (t2 - t1) / max(count, 1), t3 - t2,
              movie, os.path.getsize(src) / 1e6, os.path.basename(out), os.path.getsize(out) / 1e6))


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("movies", nargs="*", help="MVE names (default: all five)")
    ap.add_argument("--setup", action="store_true", help="download and unpack realesrgan-ncnn-vulkan")
    ap.add_argument("--out", default=HD_DIR, help="output directory (default gamedata/hd)")
    ap.add_argument("--game-dir", action="append", default=None, help="directory of the MVE files")
    ap.add_argument("--model", default="realesrgan-x4plus",
                    help="Real-ESRGAN model (realesrgan-x4plus, realesr-animevideov3, ...)")
    ap.add_argument("--scale", type=int, default=4)
    ap.add_argument("--quality", type=int, default=2, help="MPEG-1 quantizer, 2 (best) to 31")
    ap.add_argument("--keep", action="store_true", help="keep the PNG pictures")
    args = ap.parse_args()
    args.game_dir = args.game_dir or GAME_DIRS
    os.makedirs(args.out, exist_ok=True)

    if args.setup:
        setup(args.out)
    if not args.movies and args.setup:
        return
    ffmpeg = find_tool("ffmpeg", ["C:/FFmpeg/ffmpeg.exe", "C:/FFmpeg/bin/ffmpeg.exe"])
    esrgan = find_tool("realesrgan-ncnn-vulkan",
                       [os.path.join(args.out, "realesrgan", "realesrgan-ncnn-vulkan.exe"),
                        os.path.join(args.out, "realesrgan", "realesrgan-ncnn-vulkan")])
    for m in (args.movies or MOVIES):
        make(m.upper() if m.upper().endswith(".MVE") else m.upper() + ".MVE", args, ffmpeg, esrgan)


if __name__ == "__main__":
    main()
