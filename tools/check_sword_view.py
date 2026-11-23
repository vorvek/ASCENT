#!/usr/bin/env python3
"""Check of Dtu_fix_sword_view (engine/main/dtucombat.c).

In 1.3 the arms model of a long sword (letter g, hmcg_p.pof) is drawn with
the middle of the blade 0.07 left of the eye, in the middle of the view: the
center of its body box is 0.85 further right than in the other models
(0x729C1). With the correction the arms move right until the middle of the
weapon is as far right as that of the broad sword (hmch_p.pof). The game
logs the measure (-objlog):

  combat: sword view of model M: weapon middle X, broad sword Y, shift S, applied A

    python -I tools/check_sword_view.py --exe build/engine/main/ascent.exe \
        --datadir E:/UNDAMONTAN/DTU --datadir D:/dev/ASCENT/gamedata/cd/DTU
"""
import argparse
import os
import re
import shutil
import subprocess
import sys
import tempfile

LINE = re.compile(r"combat: sword view of model (\d+): weapon middle (-?[\d.]+), broad sword (-?[\d.]+), "
                  r"shift (-?[\d.]+), applied (\d)")
LONG_SWORD, BROAD_SWORD = "5.6", "5.20"

# (name, weapon, extra options, the check of the last line)
CASES = [
    ("long sword", LONG_SWORD, [],
     lambda x, y, s, a: x < 0.1 and abs(y - 0.692) < 0.01 and abs(s - (y - x)) < 0.01 and s > 0.5 and a == 1),
    ("long sword, -origcombat", LONG_SWORD, ["-origcombat"],
     lambda x, y, s, a: x < 0.1 and s > 0.5 and a == 0),
    ("broad sword", BROAD_SWORD, [],
     lambda x, y, s, a: abs(x - y) < 0.001 and s == 0 and a == 1),
]


def run(args, d, weapon, extra):
    user = os.path.join(d, "user")
    os.makedirs(user, exist_ok=True)
    log = os.path.join(d, "obj.log")
    cmd = [os.path.abspath(args.exe)]
    for dd in args.datadir:
        cmd += ["-datadir", dd]
    cmd += ["-nomovies", "-mute", "-hidden", "-nolog", "-userdir", user, "-level", "YPINN",
            "-frames", "40", "-fixedfps", "30", "-chartest",
            "5:class=2,10:give=%s,12:current=%s" % (weapon, weapon), "-objlog", log] + extra
    subprocess.run(cmd, capture_output=True, timeout=args.timeout, cwd=d)
    try:
        with open(log, encoding="latin-1") as f:
            return [m.groups() for m in map(LINE.search, f) if m]
    except OSError:
        return []


def main(argv=None):
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--exe", required=True)
    p.add_argument("--datadir", action="append", default=[])
    p.add_argument("--keep", action="store_true")
    p.add_argument("--timeout", type=int, default=120)
    args = p.parse_args(argv)
    tmp = tempfile.mkdtemp(prefix="ascent_swordview_")
    fails = 0
    try:
        for i, (name, weapon, extra, ok) in enumerate(CASES):
            lines = run(args, os.path.join(tmp, str(i)), weapon, extra)
            if not lines:
                print("FAIL %s: no sword view line" % name)
                fails += 1
                continue
            m, x, y, s, a = lines[-1]
            good = ok(float(x), float(y), float(s), int(a))
            print("%s %s: model %s, weapon middle %s, broad sword %s, shift %s, applied %s"
                  % ("ok  " if good else "FAIL", name, m, x, y, s, a))
            fails += not good
    finally:
        if args.keep:
            print("kept", tmp)
        else:
            shutil.rmtree(tmp, ignore_errors=True)
    print("check_sword_view: %s" % ("%d failed" % fails if fails else "all passed"))
    return 1 if fails else 0


if __name__ == "__main__":
    sys.exit(main())
