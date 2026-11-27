#!/usr/bin/env python3
"""Balance runs of the caster AI (Dtu_caster_ai, docs/re/ai.md): scripted
fights of a player character of the level that is expected at a place.

    python -I tools/caster_balance.py --exe build-debug/engine/main/ascent.exe
        --datadir DIR [--datadir DIR2] lich|hold [--seeds 6] [--jobs 6]

lich: the fight with the lich of CLAN3 (object 10) for a fighter, a cleric
and a thief/mage, in three cases: nothing (no Dispel Magic, the lich has
his contingency), Dispel Magic (the cleric scroll of CLAN3: a cleric gets
it as a spell of his list, another class reads it from the item list), and
the phylactery destroyed (the test command contingency=0: no contingency).
The character has 3 Potions of Healing, 1 of Extra-Healing and 2 of Freedom
in each case. The test command autofight of -chartest plays the character
(dtuchar.c auto_fight_frame): it walks to the creature, attacks, drinks at
half of its hit points, drinks a Potion of Freedom when held. The
thief/mage casts Mirror Image and the Minor Globe before the fight (the
scrolls of MULHOR1 and CLAN1) and has Lightning Bolt and Magic Missile in
its list. Prints the wins of each case.

hold: a failed save against Hold Person or the Mind Blast: the saves of
the player fail until its first hold (the test command savefail) in front
of the casters of a room, and the damage from the start of that hold to
its end is counted (no potions, no attacks of the player).

--before: without the hold cap, the hold rest and the lower spell levels
of the lich (the test command holdrules=0), for the measure before them.

The characters: setlevel gives the full hit die of each level; the game
rolls from half of the die to the die, so the script takes the difference
to the mean away at the start (the second number of CHARS).

This tool reads only and writes its logs to a temporary folder.
"""
import argparse
import concurrent.futures
import os
import re
import shutil
import subprocess
import sys
import tempfile

FPS = 30

# name, the commands of the character (class, level, items), the hit points
# that setlevel has too many, the autofight mode bits of the class
CHARS = {
    # CLAN3: about 53000 experience points (all creatures of the levels before it)
    "fighter 6": (["3:class=2", "5:setlevel=6", "6:give=15.4", "6:give=15.12", "6:give=5.32", "7:equip=15.4", "7:equip=15.12",
                   "8:current=5.32"], 12, 0),
    "cleric 6": (["3:class=0", "5:setlevel=6", "6:give=15.7", "6:give=15.2", "6:give=5.38", "7:equip=15.7", "7:equip=15.2",
                  "8:current=5.38"], 10, 0),
    "thief/mage 5/5": (["3:class=3", "3:class2=1", "4:dex=17", "5:setlevel=6", "6:give=15.40", "6:give=15.14", "6:give=5.34",
                        "6:give=17.32", "6:give=17.33", "7:equip=15.40", "7:equip=15.14", "8:current=5.34",
                        # the scrolls of Mirror Image (MULHOR1) and of the Minor Globe (CLAN1), read before the fight
                        "13:cast=56", "13:cast=57"], 5, 2),
    # DROW1 and DROW3: about 68000 and 85000
    "fighter 7": (["3:class=2", "5:setlevel=7", "6:give=15.9", "6:give=15.12", "6:give=5.32", "7:equip=15.9", "7:equip=15.12",
                   "8:current=5.32"], 15, 0),
    "cleric 7": (["3:class=0", "5:setlevel=7", "6:give=15.7", "6:give=15.12", "6:give=5.38", "7:equip=15.7", "7:equip=15.12",
                  "8:current=5.38"], 12, 0),
    "thief/mage 6/6": (["3:class=3", "3:class2=1", "4:dex=17", "5:setlevel=7", "6:give=15.40", "6:give=15.14", "6:give=5.34",
                        "7:equip=15.40", "7:equip=15.14", "8:current=5.34"], 6, 2),
}
POTIONS = ["6:give=16.79.3", "6:give=16.75.1", "6:give=16.78.2"]

LICH_LEVEL, LICH = "CLAN3", 10
LICH_CASES = [("nothing", [], 4), ("Dispel Magic scroll", ["6:give=17.55"], 5), ("phylactery destroyed", ["3:contingency=0"], 4)]
LICH_FRAMES = 3600


def run(a, d, level, frames, cmds, extra=()):
    os.makedirs(os.path.join(d, "user"), exist_ok=True)
    obj, char = os.path.join(d, "obj.log"), os.path.join(d, "char.log")
    cmds = sorted(cmds, key=lambda c: int(c.split(":")[0]))
    cmd = [os.path.abspath(a.exe)]
    for dd in a.datadir:
        cmd += ["-datadir", dd]
    cmd += ["-nomovies", "-mute", "-hidden", "-nolog", "-userdir", os.path.join(d, "user"), "-level", level, "-frames", str(frames),
            "-fixedfps", str(FPS), "-objlog", obj, "-charlog", char, "-chartest", ",".join(cmds)] + list(extra)
    try:
        subprocess.run(cmd, capture_output=True, timeout=a.timeout, cwd=d)
    except subprocess.TimeoutExpired:
        pass
    out = []
    for f in (obj, char):
        try:
            with open(f, encoding="latin-1") as h:
                out.append(h.read())
        except OSError:
            out.append("")
    return out


def lich_one(a, work, cname, case, seed):
    cmds, off, mode = CHARS[cname]
    label, extra, cmode = case
    d = os.path.join(work, re.sub(r"\W+", "_", "%s_%s_%d" % (cname, label, seed)))
    script = ["2:seed=%d" % seed, "4:alone=%d" % LICH] + cmds + POTIONS + extra + [
        "9:heal=500", "9:fillslots", "10:damage=%d" % off, "11:goobjsee=%d.30" % LICH, "12:dump", "15:autofight=%d.%d" % (LICH, mode | cmode)] + (
        ["3:holdrules=0"] if a.before else [])
    obj, char = run(a, d, LICH_LEVEL, LICH_FRAMES, script)
    m = re.search(r"autofight: ends at ([\d.]+) s: (the player is dead|obj \d+ is dead)(.*)", char)
    hp = None
    for hp in re.finditer(r"character 'player'.*hit points (\d+) of (\d+), armor class (-?\d+), THAC0 (\d+)", char):
        pass
    res = "timeout"
    if m:
        res = "win" if "obj" in m.group(2) else "death"
    return cname, label, seed, res, float(m.group(1)) if m else None, hp.groups() if hp else None, d


def lich(a, work):
    names = ["fighter 6", "cleric 6", "thief/mage 5/5"]
    jobs = [(c, case, s) for c in names for case in LICH_CASES for s in range(1, a.seeds + 1)]
    res = {}
    stats = {}
    with concurrent.futures.ThreadPoolExecutor(a.jobs) as ex:
        for cname, label, seed, r, t, hp, d in ex.map(lambda j: lich_one(a, work, *j), jobs):
            res.setdefault((cname, label), []).append((seed, r, t))
            if hp:
                stats[cname] = hp
    for c in names:
        if c in stats:
            print("%s: %s of %s hit points at the start, armor class %s, THAC0 %s" % ((c,) + stats[c]))
    print()
    print("| Character | Case | Wins | Deaths | No end in %d s | Mean time of a win |" % (LICH_FRAMES // FPS))
    print("|---|---|---|---|---|---|")
    for c in names:
        for label, _, _ in LICH_CASES:
            rs = res.get((c, label), [])
            wins = [t for _, r, t in rs if r == "win"]
            print("| %s | %s | %d of %d | %d | %d | %s |" % (
                c, label, len(wins), len(rs), sum(1 for _, r, _ in rs if r == "death"), sum(1 for _, r, _ in rs if r == "timeout"),
                "%.0f s" % (sum(wins) / len(wins)) if wins else "-"))


# hold: name, level file, the creatures that stay (the first is the one the
# player faces), the distance, the characters, the effect seconds (None:
# the creatures cast, and the first hold that the player gets is measured)
HOLD_CASES = [
    ("one drow priestess (rffk5, DROW1)", "DROW1", [55], 20, ["fighter 7", "cleric 7", "thief/mage 6/6"]),
    ("a priestess and a drow mage (DROW1, the meeting room)", "DROW1", [55, 56], 20, ["fighter 7", "cleric 7", "thief/mage 6/6"]),
    ("one illithid (DROW3)", "DROW3", [59], 20, ["fighter 7", "cleric 7", "thief/mage 6/6"]),
    ("the illithid and its four trolls (DROW3)", "DROW3", [59, 105, 106, 107, 108], 20, ["fighter 7", "cleric 7", "thief/mage 6/6"]),
]
HOLD_FRAMES = 2700


def hold_one(a, work, case, cname, seed):
    name, level, objs, dist, _ = case
    cmds, off, _ = CHARS[cname]
    d = os.path.join(work, re.sub(r"\W+", "_", "%s_%s_%d" % (name, cname, seed)))
    script = ["2:seed=%d" % seed, "4:alone=%s" % ".".join(str(o) for o in objs)] + cmds + [
        "9:heal=500", "9:fillslots", "10:damage=%d" % off, "11:goobjsee=%d.%d" % (objs[0], dist), "12:dump", "13:savefail=1",
        "14:noise=3", "15:spelltest=28.1.%d.0" % objs[0]] + (["3:holdrules=0"] if a.before else [])
    obj, char = run(a, d, level, HOLD_FRAMES, script)
    hp0 = None
    for hp0 in re.finditer(r"character 'player'.*hit points (\d+) of (\d+), armor class (-?\d+)", char):
        pass
    hp0v = int(hp0.group(1)) if hp0 else 0
    # the first hold of the player, and the damage from its start to its end
    start = end = None
    dmg_before = dmg = 0
    dead = None
    hits = 0
    for line in obj.split("\n"):
        m = re.match(r"\s*([\d.]+)\s+\d+ (.*)", line)
        if not m:
            continue
        t, s = float(m.group(1)), m.group(2)
        if start is None and re.search(r"spell: effect (Hold Person|Mind Blast|Hold\w*) on obj 0 you for ([\d.]+) seconds", s):
            start = t
            end = t + float(re.search(r"for ([\d.]+) seconds", s).group(1))
            continue
        k = re.search(r"damage: the player takes (\d+) from", s)
        if k:
            if start is None:
                dmg_before += int(k.group(1))
            elif t <= end + 0.01:
                dmg += int(k.group(1))
                hits += 1
                if "hp 0" in s and dead is None:
                    dead = t - start
        if start is not None and re.search(r"effect \w+( \w+)? of obj 0 you ends", s) and t < end:
            end = t
    return case[0], cname, seed, hp0v, dmg_before, dmg, hits, (end - start) if start is not None else None, dead, d


def hold(a, work):
    jobs = [(case, c, s) for case in HOLD_CASES for c in case[4] for s in range(1, a.seeds + 1)]
    res = {}
    with concurrent.futures.ThreadPoolExecutor(a.jobs) as ex:
        for name, cname, seed, hp0, before, dmg, hits, dur, dead, d in ex.map(lambda j: hold_one(a, work, *j), jobs):
            res.setdefault((name, cname), []).append((hp0, before, dmg, hits, dur, dead))
    print("| Place | Character | Hit points | Runs with a hold | Hold time | Damage in the hold (mean, most) | Hits | Deaths in the hold |")
    print("|---|---|---|---|---|---|---|---|")
    for case in HOLD_CASES:
        for c in case[4]:
            rs = res.get((case[0], c), [])
            held = [r for r in rs if r[4] is not None]
            if not held:
                print("| %s | %s | %s | 0 of %d | - | - | - | - |" % (case[0], c, rs[0][0] if rs else "?", len(rs)))
                continue
            print("| %s | %s | %d | %d of %d | %.0f s | %.0f, %d | %.1f | %d |" % (
                case[0], c, held[0][0], len(held), len(rs), sum(r[4] for r in held) / len(held),
                sum(r[2] for r in held) / len(held), max(r[2] for r in held), sum(r[3] for r in held) / len(held),
                sum(1 for r in held if r[5] is not None)))


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("what", choices=["lich", "hold"])
    p.add_argument("--exe", required=True)
    p.add_argument("--datadir", action="append", default=[])
    p.add_argument("--seeds", type=int, default=6)
    p.add_argument("--jobs", type=int, default=6)
    p.add_argument("--keep", action="store_true")
    p.add_argument("--before", action="store_true", help="without the hold cap, the hold rest and the lower spell levels of the lich")
    p.add_argument("--timeout", type=int, default=900)
    a = p.parse_args()
    work = tempfile.mkdtemp(prefix="caster_balance_")
    (lich if a.what == "lich" else hold)(a, work)
    if a.keep:
        print("Logs: %s" % work)
    else:
        shutil.rmtree(work, ignore_errors=True)
    return 0


if __name__ == "__main__":
    sys.exit(main())
