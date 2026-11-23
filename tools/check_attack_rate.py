#!/usr/bin/env python3
"""Measure the attack rate and the move speed of the player in the game.

    python -I tools/check_attack_rate.py --exe build/engine/main/ascent.exe \\
        --datadir E:/UNDAMONTAN/DTU --datadir D:/dev/ASCENT/gamedata/cd/DTU \\
        [--origcombat] [--keep]

Each case is one run on YPINN at 30 frames a second. -chartest gives the
items and the effects (fx=13 Haste, fx=27 Slow), and -testmove presses the
attack key every few frames. The object log gives the times: "attack
request accepted" for a melee weapon, "the player fires" for an arrow,
and "swing ends ... after its start" for the length of a swing. The mean
interval must be the expected one, plus at most one press period (the
presses come in steps). The move cases walk forward for 30 frames
(-movelog) and compare the distance with the empty hand.

The rules are in engine/main/dtuatk.c (docs/re/combat.md, section A8).
Dtu_fix_speed_items: the Short Sword of Quickness and the Short Bow of
Speed attack twice as fast, do not add to Haste, cancel Slow and do not
change the move speed. The exit status is 0 if all cases pass.
"""
import argparse
import os
import re
import shutil
import subprocess
import sys
import tempfile

FPS = 30
FRAME = 1.0 / FPS
LONG_SWORD, DAGGER, QUICKNESS, ARROW = "5.6", "5.18", "5.69", "5.7"
SHORT_BOW, BOW_OF_SPEED = "16.89", "16.100"
RING_OF_QUICKNESS = "15.25"
HASTE, SLOW = "fx=13.600", "fx=27.600"

ACCEPT = re.compile(r"combat: attack request accepted at ([0-9.]+)")
FIRE = re.compile(r"^\s*([0-9.]+)\s+\d+ combat: the player fires weapon")
SWING = re.compile(r"combat: swing ends at [0-9.]+, ([0-9.]+) after its start")
MOVE = re.compile(r"^MOVE (\d+) t \S+ pos (\S+) \S+ (\S+) ")


def melee(name, weapon, fx, expect, swing):
    return (name, ["give=%s" % weapon, "current=%s" % weapon] + fx, "melee", expect, swing)


def bow(name, launcher, fx, expect):
    return (name, ["give=%s" % launcher, "equip=%s" % launcher, "give=%s.99" % ARROW, "current=%s" % ARROW] + fx,
            "shot", expect, None)


# (name, chartest commands, kind, interval, swing) with the corrections
CASES = [
    melee("long sword", LONG_SWORD, [], 1.0, 1.0),
    melee("dagger", DAGGER, [], 1.0, 1.0),
    melee("Short Sword of Quickness", QUICKNESS, [], 0.5, 0.5),
    melee("Short Sword of Quickness, Haste", QUICKNESS, [HASTE], 0.5, 0.5),
    melee("Short Sword of Quickness, Slow", QUICKNESS, [SLOW], 1.0, 1.0),
    melee("long sword, Haste", LONG_SWORD, [HASTE], 0.5, 0.5),
    melee("dagger, Haste", DAGGER, [HASTE], 0.5, 0.5),
    melee("long sword, Slow", LONG_SWORD, [SLOW], 2.0, 2.0),
    bow("short bow", SHORT_BOW, [], 2.0),
    bow("Short Bow of Speed", BOW_OF_SPEED, [], 1.0),
    bow("Short Bow of Speed, Haste", BOW_OF_SPEED, [HASTE], 1.0),
    bow("short bow, Haste", SHORT_BOW, [HASTE], 1.0),
    bow("Short Bow of Speed, Slow", BOW_OF_SPEED, [SLOW], 2.0),
]

# the rules of 1.3 (-origcombat): no speed items, Haste makes melee slower
ORIG_CASES = [
    melee("long sword", LONG_SWORD, [], 1.0, 1.0),
    melee("dagger", DAGGER, [], 1.0, 1.0),
    melee("Short Sword of Quickness", QUICKNESS, [], 1.0, 1.0),
    melee("long sword, Haste", LONG_SWORD, [HASTE], 1.5, 1.0),
    melee("Short Sword of Quickness, Haste", QUICKNESS, [HASTE], 1.5, 1.0),
    # D-098: a Slow of 1 s doubles the swing time until the item changes
    melee("long sword, Slow that ends", LONG_SWORD, ["fx=27.1"], 2.0, 2.0),
]

# (name, chartest commands, distance relative to the empty hand)
MOVES = [
    ("empty hand", [], 1.0),
    ("Short Sword of Quickness in the hand", ["give=%s" % QUICKNESS, "current=%s" % QUICKNESS], 1.0),
    ("Short Bow of Speed in the bow slot", ["give=%s" % BOW_OF_SPEED, "equip=%s" % BOW_OF_SPEED], 1.0),
    ("Ring of Quickness", ["give=%s" % RING_OF_QUICKNESS, "equip=%s" % RING_OF_QUICKNESS], 2.0),
]


def run(args, d, cmds, testmove, frames, extra):
    os.makedirs(os.path.join(d, "user"), exist_ok=True)
    script = ",".join("%d:%s" % (5 + i, c) for i, c in enumerate(["class=2"] + cmds))
    cmd = [os.path.abspath(args.exe)]
    for dd in args.datadir:
        cmd += ["-datadir", dd]
    cmd += ["-nomovies", "-mute", "-hidden", "-nolog", "-userdir", os.path.join(d, "user"), "-level", "YPINN",
            "-frames", str(frames), "-fixedfps", str(FPS), "-invulnerability", "-chartest", script,
            "-testmove", testmove] + extra
    subprocess.run(cmd, capture_output=True, timeout=args.timeout, cwd=d)


def lines(path):
    try:
        with open(path, encoding="latin-1") as f:
            return f.read().splitlines()
    except OSError:
        return []


def attack_case(args, tmp, i, case, extra):
    name, cmds, kind, expect, swing = case
    # a press each `period` frames; 31 presses fit in the 64 steps of -testmove
    gap = 1 if kind == "melee" and expect <= 1.0 else 2 if expect <= 2.0 else 5
    period = (gap + 1) * FRAME
    tm = "wait:30" + ",attack:1,wait:%d" % gap * 31
    d = os.path.join(tmp, "a%02d" % i)
    log = os.path.join(d, "obj.log")
    run(args, d, cmds, tm, 30 + 31 * (gap + 1) + 40, extra + ["-objlog", log])
    text = lines(log)
    pat = ACCEPT if kind == "melee" else FIRE
    times = [float(m.group(1)) for m in (pat.search(ln) for ln in text) if m]
    swings = [float(m.group(1)) for m in (SWING.search(ln) for ln in text) if m]
    bad = []
    if len(times) < 2:
        return ["%s: %d attacks, no interval" % (name, len(times))], None, None
    gaps = [b - a for a, b in zip(times, times[1:])]
    mean = sum(gaps) / len(gaps)
    if not (expect - FRAME - 0.005 <= mean <= expect + period + FRAME + 0.005):
        bad.append("%s: %.3f s between attacks, expected %.2f s" % (name, mean, expect))
    sw = sum(swings) / len(swings) if swings else None
    if kind == "melee" and (sw is None or abs(sw - swing) > FRAME + 0.005):
        bad.append("%s: swing %s s, expected %.2f s" % (name, "%.3f" % sw if sw else "none", swing))
    return bad, mean, sw


def move_case(args, tmp, i, case, extra):
    name, cmds, _ = case
    d = os.path.join(tmp, "m%02d" % i)
    log = os.path.join(d, "move.log")
    run(args, d, cmds, "wait:30,forward:30", 90, extra + ["-movelog", log])
    pos = {}
    for ln in lines(log):
        m = MOVE.match(ln)
        if m:
            pos[int(m.group(1))] = (float(m.group(2)), float(m.group(3)))
    if 30 not in pos or 60 not in pos:
        return None
    (x0, z0), (x1, z1) = pos[30], pos[60]
    return ((x1 - x0) ** 2 + (z1 - z0) ** 2) ** 0.5


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--exe", required=True)
    p.add_argument("--datadir", action="append", default=[])
    p.add_argument("--origcombat", action="store_true", help="the rules of 1.3")
    p.add_argument("--keep", action="store_true")
    p.add_argument("--timeout", type=int, default=300)
    args = p.parse_args()
    extra = ["-origcombat"] if args.origcombat else []
    tmp = tempfile.mkdtemp(prefix="check_attack_rate_")
    bad = []
    for i, case in enumerate(ORIG_CASES if args.origcombat else CASES):
        b, mean, sw = attack_case(args, tmp, i, case, extra)
        bad += b
        print("%-36s %s %s%s" % (case[0], "%.3f s" % mean if mean else "-",
                                 "swing %.3f s" % sw if sw else "", "  FAIL" if b else ""))
    base = None
    for i, case in enumerate(MOVES):
        dist = move_case(args, tmp, i, case, extra)
        if dist is None:
            bad.append("%s: no move log" % case[0])
            continue
        if base is None:
            base = dist
        ratio = dist / base if base else 0
        fail = abs(ratio - case[2]) > 0.1 * case[2]
        if fail:
            bad.append("%s: move %.3f of the empty hand, expected %.1f" % (case[0], ratio, case[2]))
        print("%-36s move %.1f in 1 s, %.2f of the empty hand%s" % (case[0], dist, ratio, "  FAIL" if fail else ""))
    for b in bad:
        print("FAIL: " + b)
    if args.keep:
        print("runs in " + tmp)
    else:
        shutil.rmtree(tmp, ignore_errors=True)
    print("check_attack_rate: %s" % ("%d failures" % len(bad) if bad else "ok"))
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
