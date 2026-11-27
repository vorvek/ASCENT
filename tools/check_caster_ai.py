#!/usr/bin/env python3
"""Check the Caster AI setting in the game and measure each caster type.

    python -I tools/check_caster_ai.py --exe build/engine/main/ascent.exe \\
        --datadir E:/UNDAMONTAN/DTU --datadir D:/dev/ASCENT/gamedata/cd/DTU
        [--only NAME,NAME] [--jobs N] [--table] [--keep]

The rules are in docs/re/ai.md, "Caster AI" (engine/main/dtuspell.c,
dtucaster.h, dtuobj_ai.c). Each case is one hidden run of 60 seconds at 30
frames a second with -objlog and the seed 1: the player (a fighter of level
8, healed each second, who does not fight) stands in front of one caster,
and the test command alone=OBJ removes the other creatures. A Magic Missile
of the player at second 0.5 makes a caster hostile whose script keeps it
peaceful. Each case runs with the setting on ("after") and with
-origcasterai ("before", the casters of 1.3 with their Dtu_fix_caster_*
corrections).

The measures of a run, from the log:
  buffs    the spells of the contingency at the first notice of the player
  casts    the spells that the caster casts
  attacks  the casts that go out, the touches and the shots of its weapon
  gap      the mean time between two attacks in seconds
  damage   the hit points that the player loses to the caster
  dpr      the damage for each round of 6 seconds, from the first notice

The checks (the exit status is 1 when one fails):
  - after: a type with protections in its list gets them at its first
    notice, in the frame of the notice;
  - after: the caster attacks 4 times or more in the minute (the dragon
    hatchling is not in the list and is only measured);
  - door: CLAN3, the lich obj 10 in segment 443 and the player behind the
    closed door of its room (segment 251), nobody else: no notice, no
    protection and no cast in 30 seconds. Then the player stands in the
    room: the lich notices the player;
  - allies: DROW1, the player in front of the priestess obj 59 with the drow
    around her: no spell of a drow acts on a drow;
  - player: Mirror Image and Minor Globe of Invulnerability on the player
    against the mage obj 13 of DROW1: an image takes an attack, the globe
    stops a spell of level 1 to 3;
  - off: with -origcasterai no line of the loop ("caster:") is in the log.

--table prints the measures as a Markdown table (docs/re/ai.md). --jobs N
runs N games at a time (a run takes the 60 seconds of its game time).
"""
import argparse
import os
import re
import shutil
import subprocess
import sys
import tempfile
from concurrent.futures import ThreadPoolExecutor

FPS = 30
FRAMES = 1800

# name, level, object, distance in front of the object, the display name of
# the type in the log (the damage lines), has protections, checked
CASES = [
    ("lich", "CLAN3", 10, 30, "Lich3", True, True),
    ("drow mage 5 (rmmk5)", "DROW1", 13, 14, "rmmk5", True, True),
    ("drow mage 7 (rmmk7)", "SPIDER", 5, 14, "rmmk7", True, True),
    ("drow mage 9 (rmmk9)", "FLAME", 19, 14, "rmmk9", True, True),
    ("drow priestess 5 (rffk5)", "DROW1", 55, 14, "rffk5", True, True),
    ("drow priestess 8 (rffk8b)", "DROW1", 59, 14, "rffk8b", True, True),
    ("drow priestess 9 (rffk9)", "FLAME", 35, 14, "rffk9", True, True),
    ("illithid (flayer)", "DROW3", 59, 20, "flayer", False, True),
    ("human cleric 4 (hmft4)", "CLAN1", 105, 20, "hmft4", True, True),
    ("human cleric 7 (hmft7)", "IBBALAR", 69, 20, "hmft7", True, True),
    ("kobold shaman (kob_spel)", "KOBOLDS", 19, 14, "kob_spel", True, True),
    ("Ibbalar", "IBBALAR", 90, 20, "ibbalar", True, True),
    ("Halaster", "FLAME", 17, 20, "halaster", True, True),
    ("Thashasa", "IBBALAR", 20, 14, "thashasa", True, True),
    ("Xabash", "BEHOLDER", 106, 30, "xabash", False, True),
    ("beholder", "BEHOLDER", 111, 30, "Beholder", False, True),
    ("avatar of Lolth", "FLAME", 36, 30, "avatar", True, True),
    ("priest spirit (prstsprt)", "MULHOR2", 2, 14, "prstsprt", True, True),
    ("Mage Spirit", "MULHOR2", 3, 14, "Mage Spirit", True, True),
    ("Aleena Paladinstar", "YPINN", 17, 14, "Aleena", True, True),
    ("drow priestess of the inn (drowfem)", "YPINN", 27, 14, "drowfem", True, True),
    ("priestess of the inn (priestes)", "YPINN", 32, 14, "priestes", True, True),
    ("dragon hatchling (hatchlng), not in the list", "DROW2", 48, 30, "hatchlng", False, False),
]


def script(cmds, frames, heal=True):
    have = {int(c.split(":")[0]) for c in cmds}
    out = list(cmds)
    if heal:
        out += ["%d:heal=500" % f for f in range(30, frames, 30) if f not in have]
    out.sort(key=lambda c: int(c.split(":")[0]))
    return ",".join(out)


def run(a, d, level, frames, cmds, extra):
    os.makedirs(os.path.join(d, "user"), exist_ok=True)
    log = os.path.join(d, "obj.log")
    cmd = [os.path.abspath(a.exe)]
    for dd in a.datadir:
        cmd += ["-datadir", dd]
    cmd += ["-nomovies", "-mute", "-hidden", "-nolog", "-userdir", os.path.join(d, "user"), "-level", level,
            "-frames", str(frames), "-fixedfps", str(FPS), "-objlog", log, "-chartest", cmds] + extra
    subprocess.run(cmd, capture_output=True, timeout=a.timeout, cwd=d)
    try:
        with open(log, encoding="latin-1") as f:
            return f.read()
    except OSError:
        return ""


LINE = re.compile(r"^\s*(\d+\.\d+)\s+(\d+) (.*)$")


def measure(text, obj, frames):
    """The measures of the caster obj in a log."""
    m = {"buffs": [], "casts": [], "attacks": [], "damage": 0, "notice": None, "notice_frame": None, "buff_frames": set(),
         "caster_lines": 0, "interrupted": 0}
    tag = "obj %d " % obj
    last_spell_caster = None
    for line in text.splitlines():
        lm = LINE.match(line)
        if not lm:
            continue
        t, frame, s = float(lm.group(1)), int(lm.group(2)), lm.group(3)
        if s.startswith("caster:"):
            m["caster_lines"] += 1
        if s.startswith("caster: " + tag):
            k = re.search(r"notices the player.*contingency: (.*)$", s)
            if k:
                m["notice"], m["notice_frame"] = t, frame
                m["buffs"] = [] if k.group(1) == "none" else k.group(1).split(", ")
            k = re.search(r" casts '([^']+)'", s)
            if k:
                m["casts"].append((t, k.group(1)))
                m["attacks"].append(t)
            if "is interrupted" in s:
                m["interrupted"] += 1
        elif s.startswith("cspell: " + tag):
            k = re.search(r" casts '([^']+)'", s)
            if k:
                m["casts"].append((t, k.group(1)))
                m["attacks"].append(t)
                if m["notice"] is None:
                    m["notice"] = t
        elif s.startswith("attack: " + tag) and ("touches the player" in s or "fires weapon" in s or "casts spell" in s
                                               or "swings at the player" in s):
            m["attacks"].append(t)
            if m["notice"] is None:
                m["notice"] = t
        elif s.startswith(tag) and "sees the player" in s and m["notice"] is None:
            m["notice"] = t
        k = re.match(r"spell: '[^']+' level \d+ on obj 0 you: damage (\d+), done (\d+)", s)
        if k:
            m["damage"] += int(k.group(2))
        k = re.match(r"damage: the player takes (\d+) from ", s)
        if k:
            m["damage"] += int(k.group(1))
    end = frames / FPS
    start = m["notice"] if m["notice"] is not None else 0.5
    rounds = max(1.0, (end - start) / 6.0)
    m["dpr"] = m["damage"] / rounds
    at = sorted(m["attacks"])
    m["gap"] = (at[-1] - at[0]) / (len(at) - 1) if len(at) > 1 else None
    return m


def case_cmds(obj, dist, extra=()):
    return script(["2:seed=1", "4:alone=%d" % obj, "5:setlevel=8", "8:heal=500", "10:goobj=%d.%d" % (obj, dist), "12:face=%d" % obj,
                   "15:spelltest=33.1.%d.0" % obj] + list(extra), FRAMES)


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--exe", required=True)
    p.add_argument("--datadir", action="append", default=[])
    p.add_argument("--only")
    p.add_argument("--jobs", type=int, default=4)
    p.add_argument("--table", action="store_true")
    p.add_argument("--keep", action="store_true")
    p.add_argument("--timeout", type=int, default=600)
    a = p.parse_args()
    only = set(a.only.split(",")) if a.only else None
    work = tempfile.mkdtemp(prefix="caster_ai_")
    fails = 0
    rows = []

    def check(name, ok, what):
        nonlocal fails
        print("%s %s: %s" % ("PASS" if ok else "FAIL", name, what))
        if not ok:
            fails += 1

    # the runs of the types, --jobs at a time
    jobs = []
    for name, level, obj, dist, disp, buffs, checked in CASES:
        key = disp.replace(" ", "_")
        if only and key not in only and "types" not in only:
            continue
        for mode, extra in (("before", ["-origcasterai"]), ("after", [])):
            d = os.path.join(work, "%s_%s" % (key, mode))
            os.makedirs(d)
            jobs.append((key, mode, d, level, obj, dist, extra))
    with ThreadPoolExecutor(max_workers=max(1, a.jobs)) as pool:
        texts = list(pool.map(lambda j: run(a, j[2], j[3], FRAMES, case_cmds(j[4], j[5]), j[6]), jobs))
    results = {(j[0], j[1]): t for j, t in zip(jobs, texts)}

    for k, (name, level, obj, dist, disp, buffs, checked) in enumerate(CASES):
        key = disp.replace(" ", "_")
        if only and key not in only and "types" not in only:
            continue
        res = {}
        for mode in ("before", "after"):
            text = results[(key, mode)]
            res[mode] = measure(text, obj, FRAMES)
            if not text:
                check(name, False, "%s: no log" % mode)
        b, f = res["before"], res["after"]
        rows.append((name, b, f))

        def gap(m):
            return "%.1f" % m["gap"] if m["gap"] is not None else "-"

        print("%s (%s obj %d)" % (name, level, obj))
        print("  before: %d casts (%s), %d attacks, gap %s s, damage %d, %.1f for each round" % (
            len(b["casts"]), ", ".join(c for _, c in b["casts"]) or "none", len(b["attacks"]), gap(b), b["damage"], b["dpr"]))
        print("  after:  buffs %s; %d casts (%s), %d attacks, gap %s s, damage %d, %.1f for each round" % (
            ", ".join(f["buffs"]) or "none", len(f["casts"]), ", ".join(c for _, c in f["casts"]) or "none", len(f["attacks"]),
            gap(f), f["damage"], f["dpr"]))
        if checked:
            check(name, f["notice_frame"] is not None, "the loop notices the player (frame %s)" % f["notice_frame"])
            if buffs:
                check(name, len(f["buffs"]) > 0, "the contingency at the first notice: %s" % (", ".join(f["buffs"]) or "none"))
            check(name, len(f["attacks"]) >= 4, "%d attacks in the minute" % len(f["attacks"]))
            check(name, b["caster_lines"] == 0, "no line of the loop with -origcasterai")

    if not only or "door" in only:
        # CLAN3: the lich obj 10 in segment 443, the closed door between 443 and 251
        d = os.path.join(work, "door")
        os.makedirs(d)
        text = run(a, d, "CLAN3", 900, script(["2:seed=1", "4:alone=10", "5:setlevel=8", "10:goseg=251"], 900), [])
        m = measure(text, 10, 900)
        seen = re.search(r"obj 10 Lich3: sees the player", text) is not None
        check("door", bool(text) and m["notice_frame"] is None and not m["casts"] and not seen and "of obj 10 Lich3" not in text.split("goseg")[-1],
              "the lich behind its closed door: notice %s, casts %d, sees the player %s" % (m["notice_frame"], len(m["casts"]), seen))
        d = os.path.join(work, "door_open")
        os.makedirs(d)
        text = run(a, d, "CLAN3", 900, script(["2:seed=1", "4:alone=10", "5:setlevel=8", "10:goobj=10.30", "12:face=10"], 900), [])
        m = measure(text, 10, 900)
        check("door", m["notice_frame"] is not None and len(m["casts"]) > 0,
              "the player in the room of the lich: notice at frame %s, %d casts" % (m["notice_frame"], len(m["casts"])))

    if not only or "allies" in only:
        d = os.path.join(work, "allies")
        os.makedirs(d)
        text = run(a, d, "DROW1", FRAMES, script(["2:seed=1", "5:setlevel=8", "8:heal=500", "10:goobj=59.14", "12:face=59",
                                                  "15:spelltest=33.1.59.0"], FRAMES), [])
        bad = re.findall(r"spell: '([^']+)' level \d+ on obj (\d+) (r\w+): damage", text)
        held = [h for h in re.findall(r"spell: effect (Hold Person|Slow|Fear) on obj (\d+) (\w+)", text) if h[1] != "0"]
        spared = len(re.findall(r"does not act on its ally", text))
        casts = len(re.findall(r"caster: obj \d+ \w+ casts", text))
        check("allies", bool(text) and casts > 0 and not held, "%d casts of the drow, %d effects on a drow (%s), %d times an ally was spared" % (
            casts, len(held), ", ".join("%s on %s" % (h[0], h[2]) for h in held) or "none", spared))

    if not only or "player" in only:
        d = os.path.join(work, "player")
        os.makedirs(d)
        text = run(a, d, "DROW1", 900, script(["2:seed=1", "4:alone=13", "5:class=2", "6:setlevel=8", "8:heal=500", "9:cast=56", "9:cast=57",
                                               "10:goobj=13.14", "12:face=13", "15:spelltest=33.1.13.0"], 900), [])
        took = len(re.findall(r"Mirror Image of obj 0 you: an image takes the attack", text))
        stopped = re.findall(r"the Minor Globe of Invulnerability of obj 0 you stops '([^']+)'", text)
        check("player", took + len(stopped) > 0 and "Mirror Image on obj 0 you" in text and "Minor Globe of Invulnerability on obj 0 you" in text,
              "the player with Mirror Image and a Minor Globe: %d images took an attack, the globe stopped %d spells (%s)" % (
                  took, len(stopped), ", ".join(sorted(set(stopped))) or "none"))
        own = len(re.findall(r"Mirror Image of obj 13 rmmk5: an image takes the attack", text))
        d = os.path.join(work, "player_attacks")
        os.makedirs(d)
        text = run(a, d, "DROW1", 600, script(["2:seed=1", "4:alone=13", "5:setlevel=8", "8:heal=500", "10:goobj=13.14", "12:face=13",
                                               "15:spelltest=33.1.13.0"] + ["%d:spelltest=33.8.13.0" % f for f in range(60, 600, 30)], 600), [])
        own = len(re.findall(r"Mirror Image of obj 13 rmmk5: an image takes the attack", text))
        check("player", own > 0, "the images of the drow mage took %d Magic Missiles of the player" % own)

    if a.table:
        print()
        print("| Type | Before: casts | attacks | damage per round | After: contingency | casts | attacks | seconds between attacks | damage per round |")
        print("|---|---|---|---|---|---|---|---|---|")
        for name, b, f in rows:
            print("| %s | %d | %d | %.1f | %s | %d | %d | %s | %.1f |" % (
                name, len(b["casts"]), len(b["attacks"]), b["dpr"], ", ".join(f["buffs"]) or "none", len(f["casts"]), len(f["attacks"]),
                "%.1f" % f["gap"] if f["gap"] is not None else "-", f["dpr"]))
    print("%s: %d failures. Logs: %s" % ("check_caster_ai", fails, work if a.keep or fails else "(removed)"))
    if not a.keep and not fails:
        shutil.rmtree(work, ignore_errors=True)
    return 1 if fails else 0


if __name__ == "__main__":
    sys.exit(main())
