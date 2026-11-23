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
change the move speed.

The miss sound (engine/main/dtumiss.c, docs/re/combat.md "Miss sound"):
the sound is the flash_sound of the weapon, a game sound. Each swing of a
long sword at the air logs one miss sound "(air)", sound 2400 (cmbt01a),
which starts on the first frame of the hit test (about 0.34 of the swing
time).
In BONES the player swings at a zombie (goobjh, reach, heal each frame):
a swing with a hit logs no miss sound of the player, a swing with a missed
roll logs one "(roll)", and each melee miss of a creature on the player
logs one miss sound of that creature (or "skipped" while the last one
plays). An "air" sound that the contact of the swing stopped does not
count. A bow shot logs none, and with -origsound or -nomisssound nothing
plays and the swings go on.

A load in the same level during a swing (the save has an earlier game
time): the next attack after the load swings at once and a new item in the
hand changes the arms (dtu_combat_reset_view). The exit status is 0 if all cases pass.
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
MISS = re.compile(r"combat: miss sound (?:\d+ of the player|skipped) \((air|roll)\)")
MISS_AT = re.compile(r"combat: miss sound \d+ of the player \(air\), handle -?\d+, at (-?[\d.]+)")
MISS_STOP = re.compile(r"combat: miss sound of the player stopped \(contact\)")
MISS_NUM = re.compile(r"combat: miss sound (\d+) of the player \(air\)")
SWING_START = re.compile(r"combat: swing starts at (-?[\d.]+), swing time ([\d.]+)")
MISS_CREATURE = re.compile(r"combat: miss sound (?:\d+ of creature obj|skipped \(creature obj) (\d+)")
CREATURE_MISSES = re.compile(r"attack: (\S+) misses the player")
ZOMBIE, ZOMBIE_SEG = 11, 153        # BONES: obj 11 Zombie in segment 153


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


def run(args, d, cmds, testmove, frames, extra, level="YPINN", script=None):
    os.makedirs(os.path.join(d, "user"), exist_ok=True)
    if script is None:
        script = ",".join("%d:%s" % (5 + i, c) for i, c in enumerate(["class=2"] + cmds))
    cmd = [os.path.abspath(args.exe)]
    for dd in args.datadir:
        cmd += ["-datadir", dd]
    cmd += ["-nomovies", "-mute", "-hidden", "-nolog", "-userdir", os.path.join(d, "user"), "-level", level,
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


def swings(text):
    """The lines of each swing of the player: from "swing starts" to "swing ends"."""
    out, cur = [], None
    for ln in text:
        if "combat: swing starts" in ln:
            cur = []
        elif cur is not None:
            cur.append(ln)
            if "combat: swing ends" in ln:
                out.append(cur)
                cur = None
    return out


def miss_cases(args, tmp, extra):
    """The cases of the miss sound. Returns the failures."""
    hd = os.path.join(tmp, "hd")
    os.makedirs(hd, exist_ok=True)
    bad = []
    presses = "wait:30" + ",attack:1,wait:3" * 31
    frames = 30 + 31 * 4 + 40
    sword = ["give=%s" % LONG_SWORD, "current=%s" % LONG_SWORD]

    def obj_log(name, cmds, hddir, more=(), level="YPINN", script=None, testmove=presses, n=frames):
        d = os.path.join(tmp, name)
        log = os.path.join(d, "obj.log")
        run(args, d, cmds, testmove, n, extra + ["-objlog", log, "-hddir", hddir] + list(more), level, script)
        return lines(log)

    # swings at the air
    air_text = obj_log("miss_air", sword, hd)
    sw = swings(air_text)
    air = [sum(1 for ln in s if MISS.search(ln)) for s in sw]
    if len(sw) < 3 or air != [1] * len(sw):
        bad.append("miss sound, swing at the air: %d swings, miss sounds %s, expected one each" % (len(sw), air))
    nums = sorted(set(m.group(1) for m in (MISS_NUM.search(ln) for ln in air_text) if m))
    if nums != ["2400"]:
        bad.append("miss sound, swing at the air: sounds %s, expected the flash_sound 2400 of the long sword" % nums)
    print("%-36s %d swings, %d miss sounds" % ("miss sound: swing at the air", len(sw), sum(air)))
    # the sound starts on the first frame of the hit test (relative frame 6
    # of 16: 5.5 / 16 of the swing time), not when those frames are over
    start = None
    delays = []
    for ln in air_text:
        m = SWING_START.search(ln)
        if m:
            start = (float(m.group(1)), float(m.group(2)))
            continue
        m = MISS_AT.search(ln)
        if m and start:
            delays.append((float(m.group(1)) - start[0]) / start[1])
            start = None
    late = [d for d in delays if d > 0.45 or d < 0.25]
    if not delays or late:
        bad.append("miss sound, swing at the air: delays %s of the swing time, expected 0.25 to 0.45" %
                   ["%.2f" % d for d in delays])
    print("%-36s %s of the swing time" % ("miss sound: start", " ".join("%.2f" % d for d in delays)))

    # a zombie: hits, missed rolls, and the misses of the creatures
    # (strength 3: -3 to hit, so that some rolls miss)
    end = 40 + 31 * 10 + 20
    script = ["5:class=2", "6:give=%s" % LONG_SWORD, "7:current=%s" % LONG_SWORD, "8:xp=500000", "9:str=3",
              "10:goseg=%d" % ZOMBIE_SEG]
    script += ["%d:heal=100" % f for f in range(40, end)]
    script += ["%d:goobjh=%d.6,%d:reach=%d" % (f, ZOMBIE, f, ZOMBIE) for f in range(40, end, 3)]
    tm = "wait:40" + ",attack:1,wait:9" * 31
    text = obj_log("miss_zombie", [], hd, level="BONES", script=",".join(script), testmove=tm, n=end + 20)
    hits = rolls = airs = 0
    for s in swings(text):
        hit = any("combat: player hits" in ln for ln in s)
        missed = any("combat: player misses" in ln for ln in s)
        touch = any("the weapon hits" in ln or "combat: contact obj" in ln for ln in s)
        # an "air" sound that the contact of the swing stopped does not count
        found = []
        for ln in s:
            m = MISS.search(ln)
            if m:
                found.append(m.group(1))
            elif MISS_STOP.search(ln) and found and found[-1] == "air":
                found.pop()
        if hit:
            hits += 1
            want = []
        elif missed:
            rolls += 1
            want = ["roll"]
        elif touch:
            want = []
        else:
            airs += 1
            want = ["air"]
        if found != want:
            bad.append("miss sound, zombie: a swing (%s) logs %s, expected %s" %
                       ("hit" if hit else "missed roll" if missed else "contact" if touch else "air", found, want))
    if not hits or not rolls:
        bad.append("miss sound, zombie: %d hits and %d missed rolls, the run needs one of each" % (hits, rolls))
    cm = sum(1 for ln in text if CREATURE_MISSES.search(ln))
    cs = sum(1 for ln in text if MISS_CREATURE.search(ln))
    if not cm or cm != cs:
        bad.append("miss sound, zombie: %d misses of the creatures, %d miss sounds" % (cm, cs))
    print("%-36s %d hits, %d missed rolls, %d at the air; creatures: %d misses, %d sounds" %
          ("miss sound: zombie", hits, rolls, airs, cm, cs))

    # a bow shot, -origsound, -nomisssound
    bow_cmds = ["give=%s" % SHORT_BOW, "equip=%s" % SHORT_BOW, "give=%s.99" % ARROW, "current=%s" % ARROW]
    for name, cmds, hddir, more in (("bow shot", bow_cmds, hd, []),
                                    ("-origsound", sword, hd, ["-origsound"]),
                                    ("-nomisssound", sword, hd, ["-nomisssound"])):
        text = obj_log("miss_" + name.strip("-").replace(" ", "_"), cmds, hddir, more)
        n = sum(1 for ln in text if "combat: miss sound" in ln)
        acts = sum(1 for ln in text if "combat: swing starts" in ln or "the player fires" in ln)
        if n or acts < 2:
            bad.append("miss sound, %s: %d miss sounds (expected 0), %d attacks" % (name, n, acts))
        print("%-36s %d attacks, %d miss sounds" % ("miss sound: " + name, acts, n))
    return bad


LOG_LINE = re.compile(r"\s*(-?[\d.]+)\s+(\d+) (.*)$")
MODEL = re.compile(r"combat: player model .*, letter (\S)")


def load_case(args, tmp, extra):
    """A load in the same level during a swing: the save has an earlier game
    time. After the load the arms start again: the next attack swings at
    once and a new item in the hand changes the arms. (Before the fix the
    swing of before the load did not end until the game time came back to
    it: no swing and no change of the weapon for seconds.) Returns the
    failures."""
    d = os.path.join(tmp, "load")
    log = os.path.join(d, "obj.log")
    script = ",".join(["5:class=2", "6:give=%s" % LONG_SWORD, "7:current=%s" % LONG_SWORD, "8:give=%s" % DAGGER,
                       "40:save=0", "250:load=0", "275:current=%s" % DAGGER])
    tm = "wait:30" + ",attack:1,wait:9" * 40
    run(args, d, [], tm, 340, extra + ["-objlog", log], script=script)
    starts, letters, before = [], [], None
    for ln in lines(log):
        m = LOG_LINE.match(ln)
        if not m:
            continue
        frame, text = int(m.group(2)), m.group(3)
        if "combat: swing starts" in text and frame < 250:
            before = frame
        elif "combat: swing starts" in text and 250 <= frame <= 300:
            starts.append(frame)
        mm = MODEL.search(text)
        if mm and frame < 250:
            letters.append(("before", mm.group(1)))
        elif mm and 275 <= frame <= 300:
            letters.append(("after", mm.group(1)))
    bad = []
    if before is None or before < 220:
        bad.append("load: no swing in progress at the load (last start frame %s)" % before)
    if not starts:
        bad.append("load: no swing starts in the first second after the load")
    first = [l for w, l in letters if w == "before"]
    after = [l for w, l in letters if w == "after"]
    if not first or not after or after[-1] == first[-1]:
        bad.append("load: the arms do not change to the dagger after the load (models %s)" % letters)
    print("%-36s swings after the load at frames %s, arms %s" % ("load in a swing", starts, letters))
    return bad


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--exe", required=True)
    p.add_argument("--datadir", action="append", default=[])
    p.add_argument("--origcombat", action="store_true", help="the rules of 1.3")
    p.add_argument("--keep", action="store_true")
    p.add_argument("--timeout", type=int, default=300)
    p.add_argument("--miss-only", action="store_true", help="only the cases of the miss sound")
    args = p.parse_args()
    extra = ["-origcombat"] if args.origcombat else []
    tmp = tempfile.mkdtemp(prefix="check_attack_rate_")
    bad = []
    if not args.origcombat:
        bad += miss_cases(args, tmp, extra)
    if not args.miss_only:
        bad += load_case(args, tmp, extra)
    for i, case in enumerate([] if args.miss_only else ORIG_CASES if args.origcombat else CASES):
        b, mean, sw = attack_case(args, tmp, i, case, extra)
        bad += b
        print("%-36s %s %s%s" % (case[0], "%.3f s" % mean if mean else "-",
                                 "swing %.3f s" % sw if sw else "", "  FAIL" if b else ""))
    base = None
    for i, case in enumerate([] if args.miss_only else MOVES):
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
