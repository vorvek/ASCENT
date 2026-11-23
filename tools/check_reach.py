#!/usr/bin/env python3
"""Check that the player can target and hit each live creature of a level.

    python -I tools/check_reach.py --exe build/engine/main/ascent.exe \
        --datadir E:/UNDAMONTAN/DTU --datadir D:/dev/ASCENT/gamedata/cd/DTU \
        [--level BONES] [--jobs N] [--keep] [--origcombat]

The first run lists the creatures of the level (-chartest near). Then one
run for each live creature: the player gets a Short Sword (weapon 19) in the
hand and goes to the center of the segment of the creature (goseg), lands,
and then, one frame for each distance, goobjh=N.D puts the player at the
horizontal distance D from the creature on its own floor, and the test
command reach=N (engine/main/dtucombat.c) makes the player look at the
center of the creature. The log line gives whether a wall is between them,
the first object that the swing touches, the object that a missile of the
player meets, the target hint and whether the sword can hurt the creature
(the rule of the hint).

A creature that a wall hides at each distance, or that another creature
in front of it hides from each swing, is skipped. Each other
creature must be the first contact of the swing at one of the distances,
and the target hint must follow the rule on each frame that the swing
touches it. On each frame that the swing reaches the creature, a missile on
the same line must not miss (it reaches the creature, or another creature
in front of it): the body test of a missile is the ray against the capped
cylinder of the body (0x3EE70, 0x4F7D5, dtu_body_query_test in fvi.c). In
1.3 a ray that starts above a body and within its radius misses it
(0x4F4B6); the correction Dtu_fix_body_ray lets it enter through the top,
so a steep shot down at a low or wide creature hits. With --origcombat the
missile part fails for such creatures.

The rats have pass_thru: without the correction Dtu_fix_rat_hit
(--origcombat, the rules of 1.3) no attack reaches them, so the check fails
for them. The exit status is 0 if all creatures pass.
"""
import argparse
import os
import re
import shutil
import subprocess
import sys
import tempfile
from concurrent.futures import ThreadPoolExecutor

NEAR = re.compile(r"near (\d+): (\S+) type 2 id (\d+) seg (-?\d+) dist \S+ dot \S+ hp (-?\d+) flags (0x[0-9a-f]+) size (\S+)")
REACH = re.compile(r"reach (\d+): (\S+) id (\d+) dist (\S+) wall (\d) contact (-?\d+) missile (-?\d+) hint (\d) "
                   r"hurt (\d) up (\S+) seg (-?\d+)")
DISTS = (3, 4, 5, 6, 7, 8, 10, 14)
START = 40       # the first reach frame: the player lands after goseg


def run(args, user, script, frames, extra=()):
    os.makedirs(user, exist_ok=True)
    log = os.path.join(user, "char.log")
    if os.path.exists(log):
        os.remove(log)
    cmd = [args.exe]
    for d in args.datadir:
        cmd += ["-datadir", d]
    cmd += ["-nomovies", "-mute", "-hidden", "-nolog", "-userdir", user, "-level", args.level,
            "-frames", str(frames), "-fixedfps", "30", "-invulnerability", "-charlog", log,
            "-chartest", script] + list(extra)
    subprocess.run(cmd, capture_output=True, timeout=args.timeout, cwd=user)
    try:
        with open(log, encoding="latin-1") as f:
            return f.read()
    except OSError:
        return ""


def check_one(args, work, extra, n, name, seg, size):
    """(failure text or None, hidden, missile reached)"""
    script = ["5:give=5.19", "6:current=5.19", "10:goseg=%d" % seg]
    dists = sorted(set(DISTS + (int(size * 0.6 + 2),)))
    frame = START
    for d in dists:
        script += ["%d:goobjh=%d.%d" % (frame, n, d), "%d:reach=%d" % (frame, n)]
        frame += 1
    text = run(args, os.path.join(work, "obj%d" % n), ",".join(script), frame + 1, extra)
    ms = [m for m in REACH.finditer(text) if int(m.group(1)) == n]
    if not ms:
        return "FAIL %s obj %d %s: no reach line" % (args.level, n, name), False, False
    seen = [m for m in ms if m.group(5) == "0"]
    if not seen:
        return None, True, False
    melee = any(int(m.group(6)) == n for m in seen)
    missile = any(int(m.group(7)) == n for m in seen)
    hint = all(m.group(8) == m.group(9) for m in seen if int(m.group(6)) == n)
    # a missile on the same line must reach each creature that the swing
    # reaches at that distance (another creature in front of it may take the
    # missile: not a failure)
    missed = [m for m in seen if int(m.group(6)) == n and int(m.group(7)) < 0]
    if missed:
        return ("FAIL %s obj %d %s: the swing reaches it, the missile misses: %s" % (
            args.level, n, name, " | ".join("d %s up %s m %s" % (m.group(4), m.group(10), m.group(7)) for m in missed)),
            False, missile)
    if melee and hint:
        return None, False, missile
    # another creature in front of it takes each swing: not a failure
    if any(int(m.group(6)) >= 0 for m in seen):
        return None, True, missile
    return ("FAIL %s obj %d %s: swing %s, hint %s; %s" % (
        args.level, n, name, "ok" if melee else "no", "ok" if hint else "wrong",
        " | ".join("d %s up %s c %s m %s h %s/%s" % (m.group(4), m.group(10), m.group(6), m.group(7), m.group(8),
                                                     m.group(9)) for m in ms)), False, missile)


def main():
    p = argparse.ArgumentParser()
    p.add_argument("--exe", required=True)
    p.add_argument("--datadir", action="append", default=[])
    p.add_argument("--level", default="BONES")
    p.add_argument("--jobs", type=int, default=max(1, min(8, (os.cpu_count() or 2) // 2)))
    p.add_argument("--keep", action="store_true")
    p.add_argument("--origcombat", action="store_true", help="run with -origcombat (the rules of 1.3)")
    p.add_argument("--timeout", type=int, default=300)
    args = p.parse_args()
    args.exe = os.path.abspath(args.exe)
    extra = ["-origcombat"] if args.origcombat else []

    work = tempfile.mkdtemp(prefix="check_reach_")
    try:
        text = run(args, os.path.join(work, "list"), "10:near=30000", 12, extra)
        creatures = {}
        for m in NEAR.finditer(text):
            n, name, seg, hp, flags = int(m.group(1)), m.group(2), int(m.group(4)), int(m.group(5)), int(m.group(6), 16)
            if hp >= 1 and seg >= 0 and not flags & 0x06:
                creatures[n] = (name, seg, float(m.group(7)))
        if not creatures:
            print("check_reach: no creature in %s" % args.level)
            return 1

        with ThreadPoolExecutor(max_workers=args.jobs) as ex:
            results = list(ex.map(lambda item: check_one(args, work, extra, item[0], *item[1]), sorted(creatures.items())))
        fails = sum(1 for r in results if r[0])
        hidden = sum(1 for r in results if r[1])
        missiles = sum(1 for r in results if r[2])
        for r in results:
            if r[0]:
                print(r[0])
        tested = len(creatures) - hidden
        print("check_reach %s: %d creatures, %d tested, %d hidden, %d failures (a missile reached %d)" % (
            args.level, len(creatures), tested, hidden, fails, missiles))
        return 1 if fails else 0
    finally:
        if args.keep:
            print("logs in", work)
        else:
            shutil.rmtree(work, ignore_errors=True)


if __name__ == "__main__":
    sys.exit(main())
