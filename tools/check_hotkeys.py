#!/usr/bin/env python3
"""Check the key of the hot key box that was chosen last (Dtu_fix_hot_key_refresh).

  python -I tools/check_hotkeys.py --exe build/engine/main/ascent.exe --game E:/UNDAMONTAN/DTU [--datadir DIR]

The run starts YPINN with the default fighter. Box 0 holds the Broad Sword.
The key of box 0 takes it (box 0 is now the box that was chosen last). The
Mace goes into box 0 as a click puts it there (sethot, 0x2FB10 does not
reset 0x13AD15). It checks:

- the key of box 0 then takes the Mace (1.3, and the port with -origchar,
  refuse it with sound 3000 until another box is chosen),
- a second key of box 0 is refused (the box holds the hand),
- the same for a spell: the character becomes a mage, Detect Magic goes
  into box 0, the key takes the spell, a second key is refused,
- after the hand is emptied (current=-1, 0x43A5A(-3)) the key takes the
  box again,
- with -origchar the key after the new Mace is refused.

The test writes only in a new temporary directory. Returns 1 on an error.
"""
import argparse
import os
import re
import shutil
import subprocess
import sys
import tempfile

SCRIPT = ("8:presshot=0,10:give=5.27,12:sethot=0.5.27,14:presshot=0,16:hotkeys,18:presshot=0,"
          "20:class=1,21:give=17.12.1,22:sethotspell=0.12,24:presshot=0,26:hotkeys,28:presshot=0,"
          "30:current=-1,32:presshot=0,34:hotkeys")
REFUSED = "hot key 0 refused: the box that was chosen last"


def frame_lines(lines, frame):
    out = []
    for ln in lines:
        m = re.match(r"\s*[0-9.]+ f(\d+)\s+(.*)$", ln)
        if m and int(m.group(1)) == frame:
            out.append(m.group(2).rstrip())
    return out


def run(base, tmp, extra, errors, name):
    log = os.path.join(tmp, name + ".log")
    cmd = base + ["-level", "YPINN", "-frames", "45", "-charlog", log, "-chartest", SCRIPT] + extra
    try:
        p = subprocess.run(cmd, cwd=tmp, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=180)
        if p.returncode != 0:
            errors.append("%s: exit code %d" % (name, p.returncode))
    except subprocess.TimeoutExpired:
        errors.append("%s: timeout" % name)
    return open(log, encoding="latin-1").read().splitlines() if os.path.exists(log) else []


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--exe", required=True)
    ap.add_argument("--game", required=True, help="the original game directory")
    ap.add_argument("--datadir", action="append", default=[], help="another data directory")
    a = ap.parse_args()

    if not os.path.isdir(a.game):
        print("check_hotkeys: no game directory %s" % a.game)
        return 1
    tmp = tempfile.mkdtemp(prefix="ascent_hotkeys_")
    errors = []
    try:
        base = [os.path.abspath(a.exe), "-datadir", a.game]
        for d in a.datadir:
            if os.path.isdir(d):
                base += ["-datadir", d]
        base += ["-nomovies", "-mute", "-hidden", "-nolog", "-userdir", tmp, "-fixedfps", "30"]

        lines = run(base, tmp, [], errors, "fix")
        checks = (
            (14, lambda f: any("current item:" in s and "'Mace'" in s for s in f), "the key did not take the new Mace"),
            (16, lambda f: any(re.match(r"hot key 0: entry \d+ type 5 id 27 \(in the hand\)", s) for s in f),
             "box 0 does not hold the Mace in the hand"),
            (18, lambda f: REFUSED in "\n".join(f), "the second key of the Mace was not refused"),
            (24, lambda f: REFUSED not in "\n".join(f), "the key of the new spell was refused"),
            (26, lambda f: any(re.match(r"hot key 0: spell 12 .*\(in the hand\)", s) for s in f),
             "box 0 does not hold Detect Magic in the hand"),
            (28, lambda f: REFUSED in "\n".join(f), "the second key of the spell was not refused"),
            (32, lambda f: REFUSED not in "\n".join(f), "the key after the empty hand was refused"),
            (34, lambda f: any(re.match(r"hot key 0: spell 12 .*\(in the hand\)", s) for s in f),
             "the key after the empty hand did not take the spell"),
        )
        for frame, ok, what in checks:
            f = frame_lines(lines, frame)
            if not ok(f):
                errors.append("%s (frame %d: %s)" % (what, frame, f))

        lines = run(base, tmp, ["-origchar"], errors, "orig")
        if REFUSED not in "\n".join(frame_lines(lines, 14)):
            errors.append("-origchar: the key of the new Mace was not refused (1.3)")
    finally:
        shutil.rmtree(tmp, ignore_errors=True)
    for e in errors:
        print("check_hotkeys: FAIL " + e)
    if not errors:
        print("check_hotkeys: ok")
    return 1 if errors else 0


if __name__ == "__main__":
    sys.exit(main())
