#!/usr/bin/env python3
"""Check that the player can target and hit each live creature of a level.

    python -I tools/check_reach.py --exe build/engine/main/ascent.exe \
        --datadir E:/UNDAMONTAN/DTU --datadir D:/dev/ASCENT/gamedata/cd/DTU \
        [--level BONES] [--jobs N] [--keep] [--origcombat] [--weapon N]

The first run lists the creatures of the level (-chartest near). Then one
run for each live creature: the player gets a weapon (--weapon, default the
Short Sword, weapon 19) in the hand and goes to the center of the segment of
the creature (goseg), lands, and then, one frame for each distance,
goobjh=N.D puts the player at the horizontal distance D from the creature on
its own floor, and the test command reach=N.M (engine/main/dtucombat.c)
turns the body of the player to the creature (the heading only, as in play)
and sets the look pitch: M 0 looks at the center of the creature from the
eye (the pitch pass), M 1 looks level (the level pass), M 2 looks fully up
(the negative pass). The log line gives whether a wall is between them, the
first object that the swing touches, the object that a missile of the
player meets, the target hint, whether the weapon can hurt the creature
(the rule of the hint), the first contact of the swing with the rule of 1.3
(orig) and the height of the top of the body above the eye.

A creature that a wall hides at each distance, or that another creature
in front of it hides from each swing, is skipped. Each other
creature must be the first contact of the swing at one of the distances of
the pitch pass, and the target hint must follow the rule on each frame that
the swing touches it (and be off when the swing touches nothing), in each
pass. The swing follows the look pitch (Dtu_fix_swing_pitch): in 1.3 it
ignores the pitch, so a low creature near the player is hit only at some
distances with a level look. In the level pass each frame on which the
swing of 1.3 touches the creature must still touch it. In the negative pass
a creature whose body is all below the eye must not be hit: a swing at the
ceiling does not hit a rat at the feet (1.3 hits it when the level swing
reaches it). With --origcombat the level and negative passes are not
checked. On each frame that the swing reaches the creature, a missile on
the same line must not miss (it reaches the creature, or another creature
in front of it): the body test of a missile is the ray against the capped
cylinder of the body (0x3EE70, 0x4F7D5, dtu_body_query_test in fvi.c). In
1.3 a ray that starts above a body and within its radius misses it
(0x4F4B6); the correction Dtu_fix_body_ray lets it enter through the top,
so a steep shot down at a low or wide creature hits. With --origcombat the
missile part fails for such creatures.

--commit runs only the test of the committed melee attack of a creature
(Dtu_fix_attack_commit): a Zombie of BONES starts an attack on the player in
reach, and the test command stepback=N.D moves the player back at once by
8.0 (out of reach, within twice the reach: the roll is made; with
-origcombat the zombie does not swing at the player) and by 40.0 (farther
than twice the reach: no roll). The player has the Invulnerability effect
(fx=18): the zombies (level below 4) still swing, but their hits do no
damage, so the player lives until the attack of the test. The run sets
seed=1.

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
                   r"hurt (\d) up (\S+) seg (-?\d+) pitch (-?\d+) mode (\d) orig (-?\d+) top (\S+) eyedist (\S+)")
DISTS = (3, 4, 5, 6, 7, 8, 10, 14)
START = 40       # the first reach frame: the player lands after goseg
COMMIT_OBJ = (11, 153)  # --commit: a Zombie of BONES and its segment
LOW = 4.0        # the negative pass: the top of the body at least this far below the eye
BIG = (0x12, 0x13, 0x22, 0x23, 0x2B, 0x2C, 0x2D, 0x45)    # the sphere times 8 (0x6D2A0): no negative pass


def run(args, user, script, frames, extra=(), level=None):
    os.makedirs(user, exist_ok=True)
    log = os.path.join(user, "char.log")
    if os.path.exists(log):
        os.remove(log)
    cmd = [args.exe]
    for d in args.datadir:
        cmd += ["-datadir", d]
    cmd += ["-nomovies", "-mute", "-hidden", "-nolog", "-userdir", user, "-level", level or args.level,
            "-frames", str(frames), "-fixedfps", "30", "-invulnerability", "-charlog", log,
            "-chartest", script] + list(extra)
    subprocess.run(cmd, capture_output=True, timeout=args.timeout, cwd=user)
    try:
        with open(log, encoding="latin-1") as f:
            return f.read()
    except OSError:
        return ""


def committed_attack(args, work, origcombat, step):
    """The log lines of the attack of the zombie COMMIT_OBJ of BONES after the
    player steps back by step when the attack starts (stepback=N.D)."""
    n, seg = COMMIT_OBJ
    user = os.path.join(work, "commit%d%s" % (step, "o" if origcombat else ""))
    # -invulnerability does not protect the player of DTU: two zombies of
    # the segment can kill the player (12 hit points) before the attack of
    # the test, and then the test does not start. Which rolls hit depends on
    # the random numbers. The Invulnerability effect (fx=18) keeps off the
    # zombies after their swing (damage_player_w); seed=N fixes the rolls.
    script = ("2:seed=1,3:fx=18.600,5:give=5.20,6:current=5.20,10:goseg=%d,40:goobjh=%d.5,41:stepback=%d.%d"
              % (seg, n, n, step))
    olog = os.path.join(user, "obj.log")
    run(args, user, script, 200, (["-origcombat"] if origcombat else []) + ["-objlog", olog], level="BONES")
    try:
        with open(olog, encoding="latin-1") as f:
            lines = f.read().splitlines()
    except OSError:
        return None
    start = [i for i, l in enumerate(lines) if "test: the player steps back" in l]
    if not start:
        return None
    out = []
    for l in lines[start[0] + 1:]:
        if "attack: obj %d " % n in l and "starts a melee attack" in l:
            break
        if "attack: obj %d " % n in l:
            out.append(l.strip())
    return out


def check_commit(args, work):
    """Dtu_fix_attack_commit: a step back during the attack of a creature.
    One step (8.0, out of the reach of 13.9 and within twice the reach): with
    the correction the roll is made (the committed swing), with -origcombat
    the creature does not swing at the player. 40.0 is farther than twice
    the reach: no roll. Returns the failures."""
    fails = []
    near = committed_attack(args, work, False, 8)
    near_orig = committed_attack(args, work, True, 8)
    far = committed_attack(args, work, False, 40)
    if near is None or near_orig is None or far is None:
        return ["FAIL commit: the zombie %d of BONES did not start an attack on the player" % COMMIT_OBJ[0]]
    if not any("swings at the player" in l and "committed" in l for l in near):
        fails.append("FAIL commit: a step back of 8 avoids the attack with the correction: %s" % " | ".join(near))
    if any("swings at the player" in l for l in near_orig):
        fails.append("FAIL commit: with -origcombat the zombie still swings at the player: %s" % " | ".join(near_orig))
    if any("swings at the player" in l for l in far):
        fails.append("FAIL commit: a step back of 40 is hit: %s" % " | ".join(far))
    swung = lambda ls: any("swings at the player" in l for l in ls)
    print("check_reach commit: a step of 8: %s; with -origcombat: %s; a step of 40: %s" % (
        "roll" if swung(near) else "no roll", "roll" if swung(near_orig) else "no roll", "roll" if swung(far) else "no roll"))
    return fails


def check_one(args, work, extra, n, name, seg, size):
    """(failure text or None, hidden, missile reached)"""
    script = ["5:give=5.%d" % args.weapon, "6:current=5.%d" % args.weapon, "10:goseg=%d" % seg]
    dists = sorted(set(DISTS + (int(size * 0.6 + 2),)))
    frame = START
    for d in dists:
        script += ["%d:goobjh=%d.%d" % (frame, n, d)] + ["%d:reach=%d.%d" % (frame, n, mode) for mode in (1, 2, 0)]
        frame += 1
    text = run(args, os.path.join(work, "obj%d" % n), ",".join(script), frame + 1, extra)
    allm = [m for m in REACH.finditer(text) if int(m.group(1)) == n]
    ms = [m for m in allm if m.group(13) == "0"]
    if not ms:
        return "FAIL %s obj %d %s: no reach line" % (args.level, n, name), False, False
    # the hint agrees with the swing in each pass
    bad = [m for m in allm if (int(m.group(6)) == n and m.group(8) != m.group(9)) or (int(m.group(6)) < 0 and m.group(8) != "0")]
    if bad:
        return ("FAIL %s obj %d %s: the hint does not agree with the swing: %s" % (args.level, n, name, " | ".join(
            "d %s mode %s pitch %s c %s h %s/%s" % (m.group(4), m.group(13), m.group(12), m.group(6), m.group(8), m.group(9))
            for m in bad)), False, False)
    if not args.origcombat:
        lost = [m for m in allm if m.group(13) == "1" and m.group(5) == "0" and int(m.group(14)) == n and int(m.group(6)) != n]
        if lost:
            return ("FAIL %s obj %d %s: the level swing of 1.3 hits it, the port does not: %s" % (args.level, n, name, " | ".join(
                "d %s c %s" % (m.group(4), m.group(6)) for m in lost)), False, False)
        up = [m for m in allm if args.weapon not in BIG and m.group(13) == "2" and float(m.group(15)) < -LOW and int(m.group(6)) == n]
        if up:
            return ("FAIL %s obj %d %s: a swing at the ceiling hits a low body: %s" % (args.level, n, name, " | ".join(
                "d %s top %s" % (m.group(4), m.group(15)) for m in up)), False, False)
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
    p.add_argument("--weapon", type=int, default=19, help="the weapon in the hand (19 Short Sword, 20 Broad Sword, 27 Mace)")
    p.add_argument("--origcombat", action="store_true", help="run with -origcombat (the rules of 1.3)")
    p.add_argument("--timeout", type=int, default=300)
    p.add_argument("--commit", action="store_true", help="only the test of the committed attack of a creature (BONES)")
    args = p.parse_args()
    args.exe = os.path.abspath(args.exe)
    extra = ["-origcombat"] if args.origcombat else []

    work = tempfile.mkdtemp(prefix="check_reach_")
    try:
        if args.commit:
            fails = check_commit(args, work)
            for f in fails:
                print(f)
            return 1 if fails else 0
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
        print("check_reach %s weapon %d: %d creatures, %d tested, %d hidden, %d failures (a missile reached %d)" % (
            args.level, args.weapon, len(creatures), tested, hidden, fails, missiles))
        return 1 if fails else 0
    finally:
        if args.keep:
            print("logs in", work)
        else:
            shutil.rmtree(work, ignore_errors=True)


if __name__ == "__main__":
    sys.exit(main())
