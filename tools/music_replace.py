#!/usr/bin/env python3
"""Make a replacement for one music track of Descent to Undermountain.

The port (engine/main/dtumusic.c) plays <hd>/music/<TRACK>.ogg in place of
<TRACK>.TUN when the setting "Streaming safe music" (Dtu_safe_music) is on
and the file exists. Use it for a track that a video platform claims, so
that a recording of the game can be streamed.

This tool converts any audio file that ffmpeg reads into that file:
  1. decodes the input to 44100 Hz stereo and removes the silence at the
     start and at the end (below -60 dB), so that the loop has no gap,
  2. measures the loudness (RMS, ffmpeg volumedetect) of the input and of
     the original TUN (ffmpeg -f acm),
  3. applies the difference as a gain, with a limiter if the peaks would
     clip, and encodes Ogg Vorbis, 44100 Hz stereo, quality 6,
  4. measures the result and writes <TRACK>.txt with the numbers, and
     README.txt in the music directory.

  python -I tools/music_replace.py TRACK INPUT [--hd DIR] [--quality Q]

TRACK is the name of the TUN file without the extension, for example
STARTPUB. The output directory is gamedata/hd/music, which git ignores.
Never commit the output or the input: they are music files.
"""
import argparse
import os
import re
import shutil
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, ".."))
HD_DIR = os.path.join(ROOT, "gamedata", "hd")
GAME_DIRS = ["E:/UNDAMONTAN/DTU", os.path.join(ROOT, "gamedata", "cd", "DTU")]
FFMPEG_DIRS = ["C:/FFmpeg", "C:/FFmpeg/bin"]

README = """Replacement music for ASCENT
============================

The port plays NAME.ogg of this directory in place of the original track
NAME.TUN when the setting "Streaming safe music" is on (Improvements menu,
key ascent.cfg SafeMusic=1) and the file exists. NAME is the file name of
the TUN file, for example STARTPUB.ogg for STARTPUB.TUN. The case of the
name does not matter on Windows.

The file must be Ogg Vorbis. Any sample rate and one or two channels work.
The track loops from its last sample to its first sample, as the original.
The queue, the loop and the fade are the same as for the original track.

Make a file with tools/music_replace.py TRACK INPUT. The tool removes the
silence at the start and at the end and matches the loudness (RMS) to the
original track. NAME.txt has the numbers of the conversion; the port does
not read it.

With the setting off (the default) the port plays the original music.
"""


def find_tool(name):
    for d in FFMPEG_DIRS:
        p = os.path.join(d, name + ".exe")
        if os.path.isfile(p):
            return p
    p = shutil.which(name)
    if not p:
        sys.exit("music_replace: %s not found" % name)
    return p


FFMPEG = find_tool("ffmpeg")


def run(args):
    r = subprocess.run(args, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    if r.returncode != 0:
        sys.stderr.write(r.stderr.decode("utf-8", "replace")[-2000:])
        sys.exit("music_replace: ffmpeg failed")
    return r.stderr.decode("utf-8", "replace")


def loudness(path, fmt=None):
    """(mean RMS dB, peak dB, seconds) of an audio file."""
    args = [FFMPEG, "-hide_banner", "-nostdin"]
    if fmt:
        args += ["-f", fmt]
    args += ["-i", path, "-af", "volumedetect", "-f", "null", "-"]
    err = run(args)
    # ffmpeg can print the report of a second, empty filter instance: the
    # last report is the one of the decoded stream.
    mean = float(re.findall(r"mean_volume: (-?[\d.]+) dB", err)[-1])
    peak = float(re.findall(r"max_volume: (-?[\d.]+) dB", err)[-1])
    n = int(re.findall(r"n_samples: (\d+)", err)[-1])
    rate, ch = 44100, 2
    m = re.findall(r"Audio: pcm_s16le, (\d+) Hz, (mono|stereo)", err)
    if m:
        rate = int(m[-1][0])
        ch = 1 if m[-1][1] == "mono" else 2
    return mean, peak, n / ch / rate


def find_tun(track):
    for d in GAME_DIRS:
        for name in (track + ".TUN", track + ".tun"):
            p = os.path.join(d, name)
            if os.path.isfile(p):
                return p
    return None


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("track", help="TUN name without extension, e.g. STARTPUB")
    ap.add_argument("input", help="any audio file that ffmpeg reads")
    ap.add_argument("--hd", default=HD_DIR, help="the hd directory (default gamedata/hd)")
    ap.add_argument("--quality", type=float, default=6.0, help="Vorbis quality (default 6)")
    ap.add_argument("--threshold", default="-60dB", help="silence threshold (default -60dB)")
    a = ap.parse_args()

    track = os.path.splitext(os.path.basename(a.track))[0].upper()
    tun = find_tun(track)
    if not tun:
        sys.exit("music_replace: %s.TUN is not in %s" % (track, ", ".join(GAME_DIRS)))
    if not os.path.isfile(a.input):
        sys.exit("music_replace: no file %s" % a.input)

    out_dir = os.path.join(a.hd, "music")
    os.makedirs(out_dir, exist_ok=True)
    out = os.path.join(out_dir, track + ".ogg")

    orig_mean, orig_peak, orig_len = loudness(tun, "acm")
    in_mean, in_peak, in_len = loudness(a.input)

    tmp = tempfile.mkdtemp(prefix="music_replace_")
    try:
        # 1. Decode, 44100 Hz stereo, without the silence at both ends.
        trim = ("silenceremove=start_periods=1:start_threshold={t}:start_silence=0,"
                "areverse,"
                "silenceremove=start_periods=1:start_threshold={t}:start_silence=0,"
                "areverse").format(t=a.threshold)
        wav = os.path.join(tmp, "trim.wav")
        run([FFMPEG, "-hide_banner", "-nostdin", "-y", "-i", a.input, "-vn",
             "-af", trim, "-ar", "44100", "-ac", "2", "-c:a", "pcm_f32le", wav])
        trim_mean, trim_peak, trim_len = loudness(wav)

        # 2. The gain that gives the RMS of the original. A limiter keeps the
        # peaks under -0.5 dB if the gain would make them clip.
        gain = orig_mean - trim_mean
        af = "volume=%.2fdB" % gain
        limited = trim_peak + gain > -0.5
        if limited:
            af += ",alimiter=limit=0.944:level=disabled"
        run([FFMPEG, "-hide_banner", "-nostdin", "-y", "-i", wav, "-af", af,
             "-ar", "44100", "-ac", "2", "-c:a", "libvorbis", "-q:a", str(a.quality),
             "-map_metadata", "-1", out])
    finally:
        shutil.rmtree(tmp, ignore_errors=True)

    out_mean, out_peak, out_len = loudness(out)

    lines = [
        "track      %s.TUN -> %s.ogg" % (track, track),
        "input      %s" % os.path.basename(a.input),
        "original   RMS %6.1f dB  peak %5.1f dB  %7.2f s" % (orig_mean, orig_peak, orig_len),
        "input      RMS %6.1f dB  peak %5.1f dB  %7.2f s" % (in_mean, in_peak, in_len),
        "trimmed    RMS %6.1f dB  peak %5.1f dB  %7.2f s" % (trim_mean, trim_peak, trim_len),
        "gain       %+.2f dB%s" % (gain, ", limiter" if limited else ""),
        "output     RMS %6.1f dB  peak %5.1f dB  %7.2f s" % (out_mean, out_peak, out_len),
        "difference %+.1f dB (output RMS - original RMS)" % (out_mean - orig_mean),
        "encoding   Ogg Vorbis, 44100 Hz, stereo, quality %g" % a.quality,
    ]
    with open(os.path.join(out_dir, track + ".txt"), "w", newline="\n") as f:
        f.write("\n".join(lines) + "\n")
    with open(os.path.join(out_dir, "README.txt"), "w", newline="\n") as f:
        f.write(README)
    print("wrote " + out)
    print("\n".join(lines))


if __name__ == "__main__":
    main()
