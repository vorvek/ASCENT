"""music_video.py: make one video of all the game music, for a Content ID check.

Each track gets a title card (number, file name, DISCO.JRG entry numbers)
and 2 seconds of silence before it, so that a claim at a time stamp maps to
one track. The tool also writes chapters.txt: one line per track with its
start time, in the form that a YouTube description uses for chapters.

The order: the DISCO.JRG entries (the music that the game plays by number),
each file one time, then the .TUN files that are not in DISCO.JRG.

Usage:
    python -I tools/music_video.py OUTDIR [DATADIR ...]

The default data directories are the install and the extracted disc.
Needs ffmpeg in the PATH or at C:/FFmpeg/ffmpeg.exe. The output is derived
from the game files: never commit it.
"""
import glob
import os
import shutil
import struct
import subprocess
import sys

GAP = 2.0           # silence before each track, in seconds
TAIL = 1.0          # silence after each track
W, H = 1280, 720

DEFAULT_DIRS = ['E:/UNDAMONTAN/DTU', 'D:/dev/ASCENT/gamedata/cd/DTU']


def find_ffmpeg():
    p = shutil.which('ffmpeg')
    if p:
        return p
    for c in ('C:/FFmpeg/ffmpeg.exe', 'C:/FFmpeg/bin/ffmpeg.exe'):
        if os.path.exists(c):
            return c
    raise SystemExit('ffmpeg not found')


def read_disco(dirs):
    for d in dirs:
        p = os.path.join(d, 'DISCO.JRG')
        if os.path.exists(p):
            b = open(p, 'rb').read()
            n = struct.unpack_from('<I', b, 0)[0]
            return [b[4 + i * 20:4 + i * 20 + 16].split(b'\0')[0].decode('latin-1').upper()
                    for i in range(n)]
    return []


def find_tracks(dirs):
    files = {}
    for d in dirs:
        for f in glob.glob(os.path.join(d, '*.TUN')) + glob.glob(os.path.join(d, '*.tun')):
            name = os.path.basename(f).upper()
            files.setdefault(name, f)
    return files


def tun_seconds(path):
    sig, values, channels, rate, packed = struct.unpack('<IIHHH', open(path, 'rb').read(14))
    return values / channels / rate, channels


def stamp(sec):
    sec = int(sec)
    h, m, s = sec // 3600, sec // 60 % 60, sec % 60
    return '%d:%02d:%02d' % (h, m, s) if h else '%d:%02d' % (m, s)


def main(argv):
    if len(argv) < 2:
        print(__doc__)
        return 1
    out = os.path.abspath(argv[1])
    dirs = argv[2:] or DEFAULT_DIRS
    ffmpeg = find_ffmpeg()
    os.makedirs(out, exist_ok=True)

    disco = read_disco(dirs)
    files = find_tracks(dirs)
    order = []
    for name in disco:
        if name and name in files and name not in order:
            order.append(name)
    order += sorted(n for n in files if n not in order)

    # The font: a copy in the output directory, so that the filter needs no
    # escaped drive letter.
    for font in ('C:/Windows/Fonts/arialbd.ttf', 'C:/Windows/Fonts/arial.ttf'):
        if os.path.exists(font):
            shutil.copyfile(font, os.path.join(out, 'font.ttf'))
            break
    else:
        raise SystemExit('no font found for the title cards')

    chapters = []
    t = 0.0
    seg_list = open(os.path.join(out, 'segments.txt'), 'w')
    for i, name in enumerate(order):
        path = files[name]
        secs, channels = tun_seconds(path)
        entries = [str(k) for k, n in enumerate(disco) if n == name]
        where = ('DISCO.JRG entry ' + ', '.join(entries)) if entries else 'not in DISCO.JRG'
        title = '%02d  %s' % (i + 1, name)
        with open(os.path.join(out, 'card%02d.txt' % (i + 1)), 'w') as f:
            f.write('%s\n%s\n%s' % (title, where, stamp(secs)))
        chapters.append('%s %s (%s)' % (stamp(t), title, where))
        seg = 'seg%02d.mp4' % (i + 1)
        dur = GAP + secs + TAIL
        seg_path = os.path.join(out, seg)
        if not os.path.exists(seg_path):
            vf = ("drawtext=fontfile=font.ttf:textfile=card%02d.txt:fontcolor=white:"
                  "fontsize=44:line_spacing=18:x=(w-text_w)/2:y=(h-text_h)/2" % (i + 1))
            cmd = [ffmpeg, '-y', '-loglevel', 'error',
                   '-f', 'lavfi', '-i', 'color=c=black:s=%dx%d:r=1:d=%.3f' % (W, H, dur),
                   '-f', 'acm', '-i', path,
                   '-filter_complex',
                   '[1:a]adelay=%d:all=1,apad[a]' % int(GAP * 1000),
                   '-map', '0:v', '-map', '[a]', '-vf', vf,
                   '-t', '%.3f' % dur,
                   '-c:v', 'libx264', '-tune', 'stillimage', '-pix_fmt', 'yuv420p',
                   '-c:a', 'aac', '-b:a', '192k', '-ar', '44100', '-ac', '2',
                   seg]
            r = subprocess.run(cmd, cwd=out)
            if r.returncode != 0:
                raise SystemExit('ffmpeg failed on %s' % name)
        print('%-44s %7.1f s' % (title, secs))
        seg_list.write("file '%s'\n" % seg)
        t += dur
    seg_list.close()

    video = os.path.join(out, 'dtu_music.mp4')
    r = subprocess.run([ffmpeg, '-y', '-loglevel', 'error', '-f', 'concat', '-safe', '0',
                        '-i', 'segments.txt', '-c', 'copy', video], cwd=out)
    if r.returncode != 0:
        raise SystemExit('ffmpeg concat failed')
    with open(os.path.join(out, 'chapters.txt'), 'w') as f:
        f.write('\n'.join(chapters) + '\n')
    print('%d tracks, %s, video %s, chapters %s'
          % (len(order), stamp(t), video, os.path.join(out, 'chapters.txt')))
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
