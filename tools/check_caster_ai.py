#!/usr/bin/env python3
"""Check the Caster AI setting in the game and measure each caster type.

    python -I tools/check_caster_ai.py --exe build/engine/main/ascent.exe \\
        --datadir E:/UNDAMONTAN/DTU --datadir D:/dev/ASCENT/gamedata/cd/DTU
        [--only NAME,NAME] [--jobs N] [--table] [--keep]

The rules are in docs/re/ai.md, "Caster AI" (engine/main/dtuspell.c,
dtucaster.h, dtuobj_ai.c). Each case is one hidden run of 60 seconds at 30
frames a second with -objlog and the seed 1: the player (a fighter of level
8, healed each second, who does not fight) stands 20.0 from one caster with
a free line to it (the test command goobjsee), and the test command
alone=OBJ removes the other creatures. A Knock of the player on the
caster at second 0.5 makes it hostile (a creature that is the target of a
spell of the player wakes and is hostile; the script of most casters keeps
them peaceful before). Each case runs with the setting on ("after") and with
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
  - door: FLAME, the drow mage obj 19 (hostile from the start) in segment
    191 and the player in segment 318, behind the closed door of its room
    (wall 16), nobody else: no notice, no protection and no cast in 30
    seconds. Then the player stands in the room: the mage notices the
    player and casts;
  - allies: DROW1, the player in front of the priestess obj 59 with the drow
    around her: no spell of a drow acts on a drow. And the player 10.0 from
    the priestess obj 55 while the mage obj 56 next to her casts Hold Person
    and Slow on the player (the test command castby): the area spares the
    priestess; with -origcasterai Slow takes her too;
  - player: Mirror Image and Minor Globe of Invulnerability on the player
    against the mage obj 13 of DROW1: an image takes an attack, the globe
    stops a spell of level 1 to 3;
  - bolt: FLAME, the player 20.0 from the drow mage obj 19 with a wall just
    behind: the mage casts, but not Lightning Bolt, which would go on
    through the player and come back to it (Dtu_fix_bolt_pass);
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
    ("illithid (flayer)", "DROW3", 59, 20, "flayer", False, True),
    ("human cleric 4 (hmft4)", "CLAN1", 105, 20, "hmft4", True, True),
    ("human cleric 7 (hmft7)", "IBBALAR", 69, 20, "hmft7", True, True),
    ("kobold shaman (kob_spel)", "KOBOLDS", 19, 14, "kob_spel", True, True),
    ("Ibbalar", "IBBALAR", 90, 20, "ibbalar", True, True),
    # Halaster is not an AI object before his script starts him, and Thashasa
    # loses the player at once in this setup: measured, not checked
    ("Halaster", "FLAME", 17, 20, "halaster", True, False),
    ("Thashasa", "IBBALAR", 20, 14, "thashasa", True, False),
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


def run(a, d, level, frames, cmds, extra, char=False):
    os.makedirs(os.path.join(d, "user"), exist_ok=True)
    log = os.path.join(d, "obj.log")
    clog = os.path.join(d, "char.log")
    cmd = [os.path.abspath(a.exe)]
    for dd in a.datadir:
        cmd += ["-datadir", dd]
    cmd += ["-nomovies", "-mute", "-hidden", "-nolog", "-userdir", os.path.join(d, "user"), "-level", level,
            "-frames", str(frames), "-fixedfps", str(FPS), "-objlog", log, "-charlog", clog, "-chartest", cmds] + extra
    subprocess.run(cmd, capture_output=True, timeout=a.timeout, cwd=d)
    out = []
    for f in (log, clog):
        try:
            with open(f, encoding="latin-1") as h:
                out.append(h.read())
        except OSError:
            out.append("")
    return tuple(out) if char else out[0]


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
        # (a spell that deals damage has this line too)
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
    return script(["2:seed=1", "4:alone=%d" % obj, "5:setlevel=8", "8:heal=500", "10:goobjsee=%d.20" % obj,
                   "15:spelltest=28.1.%d.0" % obj] + list(extra), FRAMES)


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
        # FLAME: the drow mage obj 19 in segment 191, the closed door wall 16 between segment 190 and 318
        d = os.path.join(work, "door")
        os.makedirs(d)
        text = run(a, d, "FLAME", 900, script(["2:seed=1", "4:alone=19", "5:setlevel=8", "10:goseg=318"], 900), [])
        m = measure(text, 19, 900)
        seen = re.search(r"obj 19 rmmk9: sees the player", text) is not None
        effects = len(re.findall(r"spell: '[^']+' \(\d+\) level \d+ on obj 19 ", text))
        check("door", bool(text) and m["notice_frame"] is None and not m["casts"] and not seen and effects == 0,
              "the drow mage behind its closed door: notice %s, casts %d, spells on it %d, sees the player %s" % (
                  m["notice_frame"], len(m["casts"]), effects, seen))
        d = os.path.join(work, "door_open")
        os.makedirs(d)
        text = run(a, d, "FLAME", 900, script(["2:seed=1", "4:alone=19", "5:setlevel=8", "10:goobjsee=19.20"], 900), [])
        m = measure(text, 19, 900)
        check("door", m["notice_frame"] is not None and len(m["casts"]) > 0,
              "the player in the room of the drow mage: notice at frame %s, buffs %s, %d casts" % (
                  m["notice_frame"], ", ".join(m["buffs"]) or "none", len(m["casts"])))

    if not only or "allies" in only:
        d = os.path.join(work, "allies")
        os.makedirs(d)
        text = run(a, d, "DROW1", FRAMES, script(["2:seed=1", "5:setlevel=8", "8:heal=500", "10:goobjsee=59.20",
                                                  "15:spelltest=28.1.59.0"], FRAMES), [])
        bad = re.findall(r"spell: '([^']+)' level \d+ on obj (\d+) (r\w+): damage", text)
        held = [h for h in re.findall(r"spell: effect (Hold Person|Slow|Fear) on obj (\d+) (\w+)", text) if h[1] != "0"]
        spared = len(re.findall(r"does not act on its ally", text))
        casts = len(re.findall(r"caster: obj \d+ \w+ casts", text))
        check("allies", bool(text) and casts > 0 and not held, "%d casts of the drow, %d effects on a drow (%s), %d times an ally was spared" % (
            casts, len(held), ", ".join("%s on %s" % (h[0], h[2]) for h in held) or "none", spared))
        cmds = script(["2:seed=1", "5:setlevel=8", "10:goobjsee=55.10", "20:castby=22.56", "22:castby=22.56", "26:castby=47.56"], 60, heal=False)
        res = {}
        for mode, extra in (("on", []), ("off", ["-origcasterai"])):
            d = os.path.join(work, "allies_area_" + mode)
            os.makedirs(d)
            text = run(a, d, "DROW1", 60, cmds, ["-noattack"] + extra)
            res[mode] = (len(re.findall(r"does not act on its ally obj 55 rffk5", text)),
                         len(re.findall(r"spell: '(?:Hold Person|Slow)' \(\d+\) level \d+ on obj 55 rffk5", text)),
                         len(re.findall(r"spell: effect (?:Hold Person|Slow) on obj 55 ", text)))
        check("allies", res["on"][0] == 3 and res["on"][1] == 0 and res["off"][0] == 0 and res["off"][1] == 3,
              "Hold Person and Slow of the mage next to the priestess: spared %d times, on her %d times; with -origcasterai on her %d times (%d effects)" % (
                  res["on"][0], res["on"][1], res["off"][1], res["off"][2]))

    if not only or "player" in only:
        d = os.path.join(work, "player")
        os.makedirs(d)
        # the player with a Minor Globe: the mage of level 5 has only spells of level 1 to 3, so it fights in melee
        text = run(a, d, "DROW1", 1200, script(["2:seed=1", "4:alone=13", "5:setlevel=8", "8:heal=500", "9:cast=56", "9:cast=57",
                                                "10:goobjsee=13.20", "15:spelltest=28.1.13.0"], 1200), [])
        took = len(re.findall(r"Mirror Image of obj 0 you: an image takes the attack", text))
        melee = "the Minor Globe of the player stops its spells: it fights in melee" in text
        check("player", took > 0 and melee and "Mirror Image on obj 0 you" in text and "Minor Globe of Invulnerability on obj 0 you" in text,
              "the player with Mirror Image and a Minor Globe against the drow mage: the mage fights in melee (%s), %d images took a touch" % (
                  melee, took))
        # the globe against spells that are cast: the test command castby
        d = os.path.join(work, "player_globe")
        os.makedirs(d)
        text = run(a, d, "DROW1", 200, script(["2:seed=1", "4:alone=13", "5:setlevel=8", "8:heal=500", "9:cast=57", "20:castby=33.13",
                                               "30:castby=22.13", "40:castby=16.13", "50:castby=59.13", "60:castby=13.13", "70:castby=33.13"],
                                              200, heal=False), ["-noattack"])
        stopped = re.findall(r"the Minor Globe of Invulnerability of obj 0 you stops '([^']+)'", text)
        flame = re.search(r"spell: 'Flame Strike' level \d+ on obj 0 you: damage (\d+)", text)
        missile = re.findall(r"spell: 'Magic Missile' level \d+ on obj 0 you: damage (\d+)", text)
        check("player", sorted(set(stopped)) == ["Fireball", "Hold Person", "Magic Missile"] and flame is not None and len(missile) > 0,
              "the globe of the player stops %s; Flame Strike (level 5) deals %s; after Dispel Magic a Magic Missile deals %s" % (
                  ", ".join(sorted(set(stopped))) or "nothing", flame.group(1) if flame else "nothing", missile[0] if missile else "nothing"))
        d = os.path.join(work, "player_attacks")
        os.makedirs(d)
        text = run(a, d, "DROW1", 600, script(["2:seed=1", "4:alone=13", "5:setlevel=8", "8:heal=500", "10:goobjsee=13.20",
                                               "15:spelltest=28.1.13.0"] + ["%d:spelltest=33.8.13.0" % f for f in range(60, 600, 30)], 600), [])
        own = len(re.findall(r"Mirror Image of obj 13 rmmk5: an image takes the attack", text))
        check("player", own > 0, "the images of the drow mage took %d Magic Missiles of the player" % own)

    if not only or "bolt" in only:
        # Dtu_fix_bolt_pass: the Lightning Bolt goes on through the player and
        # comes back from a wall. FLAME: the player 20.0 from the drow mage obj
        # 19 has a wall about 7.0 behind: the mage does not cast Lightning Bolt
        # (its bolt would come back to it). -origcombat: the bolt ends at the
        # player, so the test does not apply.
        res = {}
        cmds = script(["2:seed=1", "4:alone=19", "5:setlevel=8", "8:heal=500", "10:goobjsee=19.20", "15:spelltest=28.1.19.0"], 900)
        for mode, extra in (("on", []), ("off", ["-origcombat"])):
            d = os.path.join(work, "bolt_" + mode)
            os.makedirs(d)
            text = run(a, d, "FLAME", 900, cmds, extra)
            res[mode] = (len(re.findall(r"obj 19 rmmk9: its Lightning Bolt would come back from the wall", text)),
                         len(re.findall(r"caster: obj 19 rmmk9 casts 'Lightning Bolt'", text)),
                         len(re.findall(r"caster: obj 19 rmmk9 casts", text)))
        check("bolt", res["on"][0] > 0 and res["on"][1] == 0 and res["on"][2] > 0 and res["off"][0] == 0,
              "the drow mage with a wall behind the player: %d times no Lightning Bolt, %d bolts of %d casts; "
              "with -origcombat %d such tests" % (res["on"][0], res["on"][1], res["on"][2], res["off"][0]))
    if not only or "hammer" in only:
        # Spiritual Hammer of the player (a cleric of level 12: +2): the cast,
        # the throws with their rolls, a save and a load in the middle (the
        # hammer is gone, the sword is in the hand), Dispel Magic, a rest,
        # another item in the hand, and the end of the time (level 1: 1 minute)
        d = os.path.join(work, "hammer_player")
        os.makedirs(d)
        cmds = ["2:seed=1", "3:class=0", "4:alone=59", "5:setlevel=12", "8:heal=500", "20:cast=58", "25:descspell=58"]
        for f in (40, 100, 160, 220, 280, 340):
            cmds += ["%d:goobjsee=59.8" % (f - 3), "%d:aim=59" % (f - 2), "%d:attack" % f]
        cmds += ["400:dump", "410:save=3", "440:load=3", "480:dump", "500:spelltest=58.1.0.0", "520:cast=13",
                 "580:spelltest=58.1.0.0", "590:current=16.79", "620:current=5.20", "630:spelltest=58.1.0.0", "2500:dump"]
        text, char = run(a, d, "DROW1", 2520, script(cmds, 2520, heal=False), ["-noattack"], char=True)
        thrown = len(re.findall(r"the player fires weapon \d+ 'Spiritual Hammer'", text))
        rolls = re.findall(r"attack roll: d20 (\d+), roll (\d+) against", char)
        dmg = re.findall(r"damage roll: 1d4 \+ 3: (\d+)", char)
        bonus_ok = bool(rolls) and all(int(r) - int(d20) >= 2 for d20, r in rolls)
        # 1d4 + 1 of the line + 2 of the level, and + 1 of the strength 17 of the test character
        check("hammer", "Spiritual Hammer on obj 0 you: level 12, bonus +2" in text and thrown >= 6 and len(rolls) >= 3 and len(dmg) >= 2 and
              bonus_ok and all(5 <= int(x) <= 8 for x in dmg),
              "the cleric of level 12 casts it (+2): %d throws, %d attack rolls, each with the bonus (%s), %d hits with 1d4 + 3 and the "
              "strength (%s)" % (thrown, len(rolls), bonus_ok, len(dmg), ", ".join(dmg)))
        hands = re.findall(r"f(\d+)\s+entry \d+: type 5 id 20 'Broad Sword' count 1 \(in the hand\)", char)
        models = re.findall(r"\s(\d+) combat: player model hmc(\w)_p\.pof", text)
        after_load = [l for f, l in models if 440 <= int(f) < 500]
        check("hammer", len(hands) >= 2 and "left" in char and after_load[-1:] == ["h"],
              "the Broad Sword stays the item of the hand in the inventory (%d dumps); the description has the time left (%s); "
              "after the save and the load the arm model is the one of the sword (%s)" % (
                  len(hands), "are left" in char, ", ".join(after_load) or "none"))
        ends = [int(f) for f in re.findall(r"\s(\d+) spell: Spiritual Hammer of obj 0 you ends", text)]
        check("hammer", any(520 <= f <= 525 for f in ends) and any(590 <= f <= 595 for f in ends) and any(2400 <= f <= 2460 for f in ends),
              "it ends at Dispel Magic, with another item in the hand, and after 1 minute at level 1 (frames %s)" % ends)
        # a rest (the inn has a rest place)
        d = os.path.join(work, "hammer_rest")
        os.makedirs(d)
        text, char = run(a, d, "YPINN", 120, script(["3:class=0", "5:setlevel=12", "7:mission=1", "10:warp=1.136", "40:gorest", "50:cast=58", "70:rest", "100:dump"], 120,
                                                    heal=False), ["-noattack"], char=True)
        check("hammer", "Spiritual Hammer on obj 0 you" in text and "Spiritual Hammer of obj 0 you ends" in text,
              "a rest ends it (%s)" % ("rest: not here" in char and "no rest place" or "rested"))
        # the priestess: one cast, then the throws between her casts
        d = os.path.join(work, "hammer_priestess")
        os.makedirs(d)
        text = run(a, d, "DROW1", FRAMES, case_cmds(59, 14), [])
        casts = len(re.findall(r"caster: obj 59 rffk8b casts 'Spiritual Hammer'", text))
        throws = len(re.findall(r"caster: obj 59 rffk8b throws its Spiritual Hammer", text))
        others = len(re.findall(r"caster: obj 59 rffk8b casts '(?!Spiritual Hammer)", text))
        hits = re.findall(r"damage: the player takes (\d+) from rffk8b, weapon Spiritual Hammer", text)
        check("hammer", casts == 1 and throws >= 4 and others >= 3 and len(hits) > 0 and all(3 <= int(h) <= 12 for h in hits),
              "the priestess of level 8 casts it %d time, throws it %d times (%d hits: %s), and casts %d other spells in 60 s" % (
                  casts, throws, len(hits), ", ".join(hits), others))

    if not only or "holdhit" in only:
        # Dtu_fix_hold_hit: a held player (plate mail +3 and a shield +2) is
        # hit by each attack of a drow fighter, and the hold stays; without
        # the hold some attacks miss
        res = {}
        for mode in ("held", "free"):
            d = os.path.join(work, "holdhit_player_" + mode)
            os.makedirs(d)
            cmds = ["2:seed=1", "3:class=2", "4:alone=1", "5:setlevel=8", "6:give=15.11", "6:give=15.13", "7:equip=15.11", "7:equip=15.13",
                    "8:heal=500", "10:goobjsee=1.12"] + (["20:fx=14.60"] if mode == "held" else [])
            for k in range(20):
                cmds += ["%d:attackedby=1" % (30 + 4 * k), "%d:heal=500" % (32 + 4 * k)]
            cmds += ["120:fxstate"]
            text, char = run(a, d, "DROW1", 130, script(cmds, 130, heal=False), ["-noattack"], char=True)
            res[mode] = (len(re.findall(r"attack: obj 0 \w+ is held: hit with no roll", text)),
                         len(re.findall(r"misses the player", text)),
                         len(re.findall(r"damage: the player takes \d+ from rmfg5", text)),
                         bool(re.search(r"fx: bits 0x[0-9a-f]{4}[4-7cdef][0-9a-f]{3}", char)))
        h, f = res["held"], res["free"]
        check("holdhit", h[0] == 20 and h[1] == 0 and h[2] == 20 and h[3] and f[1] > 0 and f[0] == 0,
              "20 attacks of a drow fighter on the held player: %d hits with no roll, %d misses, %d times damage, the hold stays (%s); "
              "not held: %d misses, %d times damage" % (h[0], h[1], h[2], h[3], f[1], f[2]))
        # a held creature: each swing and each thrown dagger of the player hits
        res = {}
        for mode in ("held", "free"):
            d = os.path.join(work, "holdhit_creature_" + mode)
            os.makedirs(d)
            cmds = ["2:seed=1", "3:class=2", "3:contingency=0", "4:alone=10", "5:setlevel=1", "6:give=5.22.30", "7:current=5.20",
                    "10:goobjsee=10.14"] + (["20:fxset=10.14"] if mode == "held" else [])
            for k in range(12):
                cmds += ["%d:swingat=10" % (30 + 3 * k)]
            cmds += ["70:current=5.22"]
            for k in range(6):
                f = 100 + 60 * k
                cmds += ["%d:goobjsee=10.14" % (f - 3), "%d:aim=10" % (f - 2), "%d:attack" % f]
            text, char = run(a, d, "CLAN3", 470, script(cmds, 470, heal=False), ["-noattack"], char=True)
            res[mode] = (len(re.findall(r"combat: obj 10 Lich3 is held: hit with no roll", text)),
                         len(re.findall(r"attack: obj 10 Lich3 is held: hit with no roll", text)),
                         len(re.findall(r"attack roll: d20 \d+, roll -?\d+ against THAC0 \d+ and armor class -?\d+: (?:0|-1)", char)),
                         len(re.findall(r"the player fires weapon 22", text)))
        h, f = res["held"], res["free"]
        check("holdhit", h[0] == 12 and h[1] >= 4 and h[2] == 0 and f[2] > 0 and f[0] == 0 and f[1] == 0,
              "a fighter of level 1 against the held lich: %d of 12 swings and %d of %d thrown daggers hit with no roll, %d misses; "
              "not held: %d misses" % (h[0], h[1], h[3], h[2], f[2]))

    if not only or "lich" in only:
        # the lower spell levels of the lich of CLAN3 (Caster_limits), and the
        # one test point of the contingency with its end function
        d = os.path.join(work, "lich_limits")
        os.makedirs(d)
        text = run(a, d, "CLAN3", FRAMES, case_cmds(10, 30, ["900:contingencyend=10", "905:fxobj=10"]), [])
        levels = {}
        for name, lv in re.findall(r"caster: obj 10 Lich3 casts '([^']+)' \(\d+\) at level (\d+)", text):
            levels.setdefault(name, set()).add(int(lv))
        dmg_ok = all(levels.get(n, {5}) == {5} for n in ("Lightning Bolt", "Fireball", "Magic Missile"))
        other_ok = all(v == {12} for n, v in levels.items() if n not in ("Lightning Bolt", "Fireball", "Magic Missile"))
        skin = re.search(r"'Stone Skin' \(49\) level (\d+) on obj 10 Lich3", text)
        check("lich", bool(levels) and dmg_ok and other_ok and skin is not None and skin.group(1) == "12",
              "the lich casts %s; his Stone Skin has level %s" % (
                  ", ".join("%s at level %s" % (n, "/".join(str(x) for x in sorted(v))) for n, v in sorted(levels.items())) or "nothing",
                  skin.group(1) if skin else "?"))
        ended = "caster: the protections of obj 10 Lich3 end" in text
        gone = len(re.findall(r"\s90\d spell: (?:Mirror Image|Minor Globe of Invulnerability) of obj 10 Lich3 ends|"
                              r"\s90\d spell: effect (?:Stone Skin|Shield) of obj 10 Lich3 ends", text))
        check("lich", ended and gone >= 3, "contingencyend at frame 900: the protections end (%s), %d of them logged" % (ended, gone))
        d = os.path.join(work, "lich_no_contingency")
        os.makedirs(d)
        text = run(a, d, "CLAN3", 600, script(["2:seed=1", "3:contingency=0", "4:alone=10", "5:setlevel=8", "8:heal=500", "10:goobjsee=10.20",
                                               "15:spelltest=28.1.10.0"], 600), [])
        off = "caster: obj 10 Lich3 has no contingency (switched off)" in text
        m = re.search(r"caster: obj 10 Lich3 notices the player.*contingency: (.*)", text)
        casts = len(re.findall(r"caster: obj 10 Lich3 casts", text))
        check("lich", off and m is not None and m.group(1).strip() == "none" and casts > 0,
              "contingency=0 (the test point caster_contingency): switched off (%s), the fight starts with: %s, %d casts" % (
                  off, m.group(1).strip() if m else "?", casts))

    if not only or "holdrules" in only:
        # the hold of a creature on the player: 10 seconds at most, and no
        # Hold Person or Mind Blast before 12 seconds after its end. The
        # illithid of DROW3 against a player whose saves fail until the first hold.
        d = os.path.join(work, "holdrules")
        os.makedirs(d)
        cmds = ["2:seed=1", "4:alone=59", "5:setlevel=8", "8:heal=500", "10:goobjsee=59.20", "13:savefail=1", "15:spelltest=28.1.59.0"]
        text = run(a, d, "DROW3", 2700, script(cmds, 2700), [])
        holds, ends, casts = [], [], []
        for line in text.split("\n"):
            m = re.match(r"\s*([\d.]+)\s+\d+ (.*)", line)
            if not m:
                continue
            t, s = float(m.group(1)), m.group(2)
            k = re.search(r"spell: effect Hold Person on obj 0 you for ([\d.]+) seconds", s)
            if k:
                holds.append((t, float(k.group(1))))
            elif re.search(r"spell: effect Hold Person of obj 0 you (ends|taken away)", s):
                ends.append(t)
            elif re.search(r"caster: obj 59 flayer casts '(Hold Person|Mind Blast)'", s):
                casts.append(t)
        early = [c for c in casts if any(0 <= c - e < 12.0 for e in ends)]
        longest = max([h[1] for h in holds], default=0)
        check("holdrules", len(holds) >= 2 and longest <= 10.0 and not early,
              "the illithid in 90 s: %d holds of the player, the longest %.0f s (10 at most), %d casts of Hold Person or the Mind Blast, "
              "%d of them before 12 s after the end of a hold" % (len(holds), longest, len(casts), len(early)))

    if not only or "stoneskin" in only:
        # Dtu_fix_stoneskin_hit: a miss of a creature does not use a Stone
        # Skin of the player, a hit and a natural 20 use one
        d = os.path.join(work, "stoneskin")
        os.makedirs(d)
        cmds = ["2:seed=1", "3:class=2", "4:alone=1", "5:setlevel=8", "6:give=15.11", "6:give=15.13", "7:equip=15.11", "7:equip=15.13",
                "8:heal=500", "10:goobjsee=1.12"]
        for k in range(60):
            cmds += ["%d:spelltest=49.20.0.0" % (28 + 4 * k), "%d:attackedby=1" % (30 + 4 * k)]
        text = run(a, d, "DROW1", 280, script(cmds, 280, heal=False), ["-noattack"])
        misses = len(re.findall(r"attack: rmfg5 misses the player", text))
        skins = re.findall(r"the stone skin of the player takes the hit of rmfg5 \(roll \d+, result (\d)\), (\d+) left", text)
        old = len(re.findall(r"the stone skin of the player takes the attack of rmfg5", text))
        dmg = len(re.findall(r"damage: the player takes \d+ from rmfg5", text))
        nat20 = sum(1 for r, _ in skins if r == "2")
        lefts = sorted(set(int(n) for _, n in skins))
        check("stoneskin", misses > 0 and len(skins) > 0 and nat20 > 0 and old == 0 and dmg == 0 and misses + len(skins) == 60,
              "60 attacks of a drow fighter on the player with a Stone Skin: %d misses that use none, %d hits that use one "
              "(%d of them a natural 20), %d times damage, %d uses before the roll; hits left after a use: %s" % (
                  misses, len(skins), nat20, dmg, old, lefts))

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
