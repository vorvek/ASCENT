#!/usr/bin/env python3
"""Checks the texts of the description window (engine/main/dtudesc.c,
docs/description_window.md).

Runs the game with the test command -chartest desc, which writes the text
of each item number of the three item tables and of each spell, and checks
that no text is empty or broken (a pattern that was not filled, no line
after the name, an empty line) and that each text fits the text box of the
scroll. One run for each of the four forms: the high and the low
resolution interface, English and Spanish. The inventory texts are measured
with the game font of the run, the market texts with the small font of the
front end in the high resolution box.

    python tools/check_desc.py --exe build/engine/main/ascent.exe \\
        --datadir E:/UNDAMONTAN/DTU --datadir D:/dev/ASCENT/gamedata/cd/DTU \\
        [--dump FILE] [--only es-low]

--dump writes all texts of the first run to FILE (one "desc:" result line
and one line with the text, "|" between the lines of the window).

A fifth run (--only game) compares numbers of the texts with the game: the
run wears each armor, drinks each potion, fires each wand and casts
each of the 56 spells, and the armor class, the hit points, the effects, their times and
the spells of the game logs must be the ones of the texts.

Two more runs (--only conditions, also part of --only game) compare the
condition lines of the spell texts with the game: the caster in the area,
Shield against Magic Missile, the level limits, undead only and no saving
throw (check_conditions). Each text run also needs "rule check: 0
problems" (dtu_spell_rule_check of dtuspell.c).
"""
import argparse
import os
import re
import subprocess
import sys
import tempfile

RUNS = [("en-high", []), ("en-low", ["-lowres"]), ("es-high", ["-language", "es"]),
        ("es-low", ["-language", "es", "-lowres"])]


def run(args, name, extra, tmp):
    d = os.path.join(tmp, name)
    os.makedirs(os.path.join(d, "user"), exist_ok=True)
    cmd = [os.path.abspath(args.exe)]
    for dd in args.datadir:
        cmd += ["-datadir", dd]
    cmd += ["-nomovies", "-mute", "-userdir", os.path.join(d, "user"), "-level", "YPINN", "-frames", "20",
            "-chartest", "5:desc"] + extra
    r = subprocess.run(cmd, capture_output=True, text=True, errors="replace", timeout=300, cwd=d)
    return r.stdout


def check(out, name):
    """Returns the list of problems of one run."""
    bad = []
    results = re.findall(r"^desc: (item|market|spell) ([0-9.]+) (ok|FAIL)(.*)$", out, re.M)
    total = re.search(r"^desc: (\d+) texts, (\d+) failed", out, re.M)
    if not total:
        return ["%s: no summary line (did the run reach frame 5?)" % name]
    for kind, num, res, rest in results:
        if res != "ok":
            bad.append("%s: %s %s%s" % (name, kind, num, rest))
    items = sum(1 for r in results if r[0] == "item")
    spells = sum(1 for r in results if r[0] == "spell")
    market = sum(1 for r in results if r[0] == "market")
    if items < 100 or spells < 50:
        bad.append("%s: only %d items and %d spells" % (name, items, spells))
    if market != items:
        bad.append("%s: %d market texts for %d items" % (name, market, items))
    if "not checked" in out:
        bad.append("%s: the market texts were not checked" % name)
    # dtu_spell_rule_check: the kinds and the conditions of the spell lines
    rc = re.search(r"^desc: rule check: (\d+) problems", out, re.M)
    if not rc or rc.group(1) != "0":
        bad.append("%s: the rule check of the spell table: %s problems (see the -objlog file)" %
                   (name, rc.group(1) if rc else "no"))
    print("%s: %d items, %d market lines, %d spells, %s failed" % (name, items, market, spells, total.group(2)))
    return bad


# ---------------------------------------------------------------------------
# The numbers against the game. One run does what the texts describe (it
# wears each armor, drinks each potion, fires each wand, casts spells) and
# the results of the game logs (-charlog, -objlog) are compared with the
# numbers of the texts of the same run. The texts and the logs come from
# different code: the logs are written where the game applies the value.

SLOTS = {"Lockpicks": 0, "Cloak": 1, "Neck": 2, "Body": 3, "Hands": 4, "Shield arm": 5, "Ring finger": 6, "Waist": 8,
         "Feet": 9, "Bow": 10, "Crossbow": 11}
ARMORS = range(0, 53)
POTIONS = [75, 79, 88, 72, 74, 76, 73, 77, 78, 80, 81, 82, 83, 84, 85, 86, 87]
HEAL_POTIONS = (75, 79, 88)
WANDS = range(55, 63)
# damage, then healing, then effects with a duration (Shield stops Magic Missile)
DAMAGE_SPELLS = [4, 16, 17, 32, 33, 37]
HEAL_SPELLS = [10, 11]
TIME_SPELLS = [3, 21, 29, 45, 20, 24]
# every other spell: cast on the target creature (a spell cast on the caster
# acts on the player), and its effects are checked against the text
ALL_SPELLS = range(56)
OTHER_SPELLS = [sp for sp in ALL_SPELLS if sp not in DAMAGE_SPELLS + HEAL_SPELLS + TIME_SPELLS]
SPELL_TRIALS = 20
# words of a text that name an effect of the game logs (the effect names of
# Fx_name, dtuspell.c), besides the names of "Gives"/"Causes" and the name
# of the spell itself
EFFECT_WORDS = {"Blindness": ("blind",), "Deafness": ("deafen",), "Hold Person": ("hold", "stun"),
                "Sleep": ("sleep",), "Fear": ("fear",), "Bless": ("bless",)}
SPELL_LEVEL = 5
GAME_RUNS = [("game-rulebook", ["-rulebookitems"]), ("game-orig", ["-origitems"])]
TARGET = 17  # object 17 of YPINN: Aleena, 200 hit points, no immunity
UNITS = {"second": 1, "minute": 60, "hour": 3600}


def parse_texts(out):
    """{("item", "5.20"): [lines], ("spell", "16"): [lines]} of a run."""
    texts = {}
    lines = out.splitlines()
    for i, line in enumerate(lines[:-1]):
        m = re.match(r"desc: (item|spell) ([0-9.]+) ", line)
        if m and lines[i + 1].startswith("desc:   "):
            texts[(m.group(1), m.group(2))] = lines[i + 1][8:].split("|")
    return texts


def time_range(text):
    """(low, high) in seconds of a time of a text, or None. The text shows
    whole minutes or hours, so the high end gets the rest of the unit."""
    m = re.search(r"(\d+) to (\d+) (second|minute|hour)s?", text)
    if m:
        u = UNITS[m.group(3)]
        return int(m.group(1)) * u, int(m.group(2)) * u + u - 1
    m = re.search(r"(\d+) (second|minute|hour)s?", text)
    if m:
        u = UNITS[m.group(2)]
        return int(m.group(1)) * u, int(m.group(1)) * u + u - 1
    return None


def find(lines, pattern):
    for line in lines:
        m = re.search(pattern, line)
        if m:
            return m
    return None


class Script:
    def __init__(self):
        self.cmds = []
        self.frame = 10
        self.marks = {}

    def add(self, cmd, mark=None):
        self.cmds.append("%d:%s" % (self.frame, cmd))
        if mark is not None:
            self.marks.setdefault(mark, []).append(self.frame)
        self.frame += 1


def by_frame(path, pattern):
    """{frame: [line]} of a log; pattern has the group of the frame number."""
    d = {}
    with open(path, errors="replace") as f:
        for line in f:
            m = re.match(pattern, line)
            if m:
                d.setdefault(int(m.group(1)), []).append(line.rstrip("\n"))
    return d


def check_game(args, tmp, run_name, extra):
    d = os.path.join(tmp, run_name)
    os.makedirs(os.path.join(d, "user"), exist_ok=True)
    s = Script()
    # a fighter of level 8 with 86 hit points, dexterity 14 (armor class 10)
    s.add("class=2")
    s.add("xp=200000")
    s.add("heal=500")
    s.add("desc=%d" % SPELL_LEVEL, "desc")
    s.frame += 2
    s.add("dump", "base")
    for a in ARMORS:
        s.add("give=15.%d" % a)
        s.add("equip=15.%d" % a)
        s.add("dump", ("armor", a))
        for slot in (1, 2, 3, 4, 5, 6, 8, 9):
            s.add("unequip=%d" % slot)
        s.add("drop=15.%d" % a)
    # spelltest puts the target back after each trial. A spell on one
    # target does no damage to its caster: the damage spells go to a creature.
    # The hit points of the player stay, so each cure gets its own damage.
    for sp in DAMAGE_SPELLS + TIME_SPELLS:
        s.add("spelltest=%d.%d.%d.%d" % (sp, SPELL_LEVEL, TARGET if sp in DAMAGE_SPELLS else -1,
                                         1 if sp in TIME_SPELLS else 30), ("spell", sp))
    for sp in HEAL_SPELLS:
        for trial in range(20):
            s.add("heal=500")
            s.add("damage=40")
            s.add("spelltest=%d.%d.-1.1" % (sp, SPELL_LEVEL), ("spell", sp))
    for p in POTIONS:
        for trial in range(30 if p in HEAL_POTIONS else 4):
            s.add("heal=500")
            s.add("damage=40")
            s.add("give=16.%d" % p)
            s.add("use=16.%d" % p, ("potion", p))
        s.add("dump", ("potion_dump", p))
    s.add("class=1")  # a mage for the wands
    for w in WANDS:
        s.add("give=5.%d.2" % w)
        s.add("current=5.%d" % w)
        s.frame += 3
        s.add("fire", ("wand", w))
        s.frame += 8
    # last: the spell tests can leave state behind (the potion tests come first)
    s.add("class=2")
    s.add("spelltest=13.%d.-1.0" % SPELL_LEVEL)  # Dispel Magic stays: the effects of the potions end
    for sp in OTHER_SPELLS:
        s.add("heal=500")
        s.add("spelltest=%d.%d.%d.%d" % (sp, SPELL_LEVEL, TARGET, SPELL_TRIALS), ("spell", sp))
    char_log = os.path.join(d, "char.log")
    obj_log = os.path.join(d, "obj.log")
    cmd = [os.path.abspath(args.exe)]
    for dd in args.datadir:
        cmd += ["-datadir", dd]
    # -fixedfps: one step of the game for each frame, so the frame numbers of
    # the logs are the frames of the script (with the fixed step of 1/60
    # second a frame can hold several steps)
    cmd += ["-nomovies", "-mute", "-fixedfps", "30", "-userdir", os.path.join(d, "user"), "-level", "YPINN",
            "-frames", str(s.frame + 20),
            "-chartest", ",".join(s.cmds), "-charlog", char_log, "-objlog", obj_log] + extra
    r = subprocess.run(cmd, capture_output=True, text=True, errors="replace", timeout=600, cwd=d)
    texts = parse_texts(r.stdout)
    if not texts:
        return ["game: no texts (did the run reach frame %d?)" % s.marks["desc"][0]]
    cl = by_frame(char_log, r"\s*[0-9.]+ f(\d+) ")
    ol = by_frame(obj_log, r"\s*[0-9.]+\s+(\d+) ")
    bad = []
    n = 0

    def dump_value(mark, pattern):
        m = find(cl.get(s.marks[mark][0], []), pattern)
        return m

    def text_of(kind, num, pattern):
        return find(texts.get((kind, num), []), pattern)

    base = dump_value("base", r"armor class (-?\d+),")
    if not base or int(base.group(1)) != 10:
        return ["game: the test character has no armor class 10"]

    # each armor: the armor class of the character with only this item worn
    for a in ARMORS:
        lines = texts.get(("item", "15.%d" % a))
        if not lines:
            bad.append("game: no text for armor %d" % a)
            continue
        got = dump_value(("armor", a), r"armor class (-?\d+),")
        m1, m2 = find(lines, r"^Armor class: (-?\d+)$"), find(lines, r"^Armor class: (\d+) better$")
        want = int(m1.group(1)) if m1 else 10 - int(m2.group(1)) if m2 else 10
        # the dump gives the armor class against melee: the first value of the
        # Shield line (Dtu_fix_shield_ac)
        cap = (find(lines, r"^Protects like worn armor of armor class (-?\d+)\.$") or
               find(lines, r"^Armor class (-?\d+) against melee weapons and -?\d+ against missiles\.$"))
        if cap:
            want = min(want, int(cap.group(1)))
        # one point of armor class for dexterity 15 (the character has 14)
        if find(lines, r"^Raises your dexterity by 1\.$"):
            want -= 1
        usable = find(lines, r"^Classes: (All classes|.*Fighter)")
        if not usable:
            want = 10
        n += 1
        if not got or int(got.group(1)) != want:
            bad.append("game: armor %d '%s': the text gives armor class %d, the game %s" %
                       (a, lines[0], want, got.group(1) if got else "nothing"))
        slot = find(lines, r"^Slot: (.+)$")
        if not slot or slot.group(1) not in SLOTS:
            bad.append("game: armor %d '%s' has no slot line" % (a, lines[0]))

    # spells at caster level SPELL_LEVEL, cast on the player
    def results(sp):
        res = []
        for f in s.marks[("spell", sp)]:
            for line in ol.get(f, []):
                m = re.search(r"spelltest: result: hit points (-?\d+) to (-?\d+)", line)
                if m:
                    res.append((int(m.group(1)) - int(m.group(2)), ol[f]))
        return res

    for sp in DAMAGE_SPELLS:
        lines = texts.get(("spell", str(sp)), [])
        lo = hi = 0
        for line in lines:
            m = re.match(r"Deals (?:(\d+)d(\d+))?([+-]?\d+)? ?(?:fire |acid |electric )?damage\.", line)
            if m and (m.group(1) or m.group(3)):
                cnt, sides, add = int(m.group(1) or 0), int(m.group(2) or 0), int(m.group(3) or 0)
                lo += cnt + add
                hi += cnt * sides + add
        mm = find(lines, r"^Missiles: (?:\d+ to )?(\d+)$")
        mhi = int(mm.group(1)) if mm else 1
        res = [x[0] for x in results(sp)]
        n += 1
        if not hi or len(res) < 10:
            bad.append("game: spell %d '%s': no damage line or %d results" % (sp, lines[0] if lines else "?", len(res)))
        # a made save halves; each missile rolls
        elif min(res) < lo // 2 or max(res) > hi * mhi or max(res) < lo:
            bad.append("game: spell %d '%s': the text gives %d to %d damage (%d missiles), the game %d to %d" %
                       (sp, lines[0], lo, hi, mhi, min(res), max(res)))
    for sp in HEAL_SPELLS:
        lines = texts.get(("spell", str(sp)), [])
        m = find(lines, r"^Heals (\d+) to (\d+) hit points\.")
        res = [-x[0] for x in results(sp)]
        n += 1
        if not m or len(res) < 10:
            bad.append("game: spell %d: no heal line or %d results" % (sp, len(res)))
        elif min(res) < int(m.group(1)) or max(res) > int(m.group(2)):
            bad.append("game: spell %d '%s': the text heals %s to %s, the game %d to %d" %
                       (sp, lines[0], m.group(1), m.group(2), min(res), max(res)))
    for sp in TIME_SPELLS:
        lines = texts.get(("spell", str(sp)), [])
        m = find(lines, r"^Duration: (.+)$")
        t = time_range(m.group(1)) if m else None
        res = results(sp)
        got = find(res[0][1], r"new effect .+, ([0-9.]+) seconds") if res else None
        n += 1
        if not t or not got:
            bad.append("game: spell %d: no duration line or no new effect in the game" % sp)
        elif not t[0] <= float(got.group(1)) <= t[1]:
            bad.append("game: spell %d '%s': the text gives '%s', the game %s seconds" % (sp, lines[0], m.group(1), got.group(1)))

    # every spell: each effect that the game gives must be named by the text,
    # each effect that the text gives must happen, and its time must be in
    # the duration of the text (a creature can have less: wisdom)
    for sp in ALL_SPELLS:
        lines = texts.get(("spell", str(sp)), [])
        if len(lines) < 3:
            bad.append("game: spell %d has no text" % sp)
            continue
        n += 1
        name = lines[0]
        low = " ".join(lines[1:]).lower()
        named, gives = set(), set()
        for line in lines:
            m = re.match(r"(Gives|Causes) ([A-Z][^:]*?)(?:\.| on creatures)", line)
            if m:
                named |= set(m.group(2).split(", "))
                if m.group(1) == "Gives":
                    gives |= set(m.group(2).split(", "))
        got = {}
        on_player = False
        for f in s.marks[("spell", sp)]:
            for line in ol.get(f, []):
                m = re.search(r"spelltest: '.+' \(\d+\) level \d+ on obj (\d+) ", line)
                if m:
                    on_player = m.group(1) == "0"
                m = re.search(r"spelltest:   new effect (.+?), (?:no end|no timer|([0-9.]+) seconds)", line)
                if m:
                    got.setdefault(m.group(1), []).append(float(m.group(2)) if m.group(2) else None)

        def allowed(fx):
            key = lambda x: re.sub(r"[^a-z]", "", x.lower())
            return (fx in named or key(name).startswith(key(fx)[:5]) or
                    any(w in low for w in EFFECT_WORDS.get(fx, ())))
        for fx in sorted(got):
            if not allowed(fx):
                bad.append("game: spell %d '%s': the game gives %s, the text does not name it" % (sp, name, fx))
        for fx in sorted(gives):
            if fx not in got:
                bad.append("game: spell %d '%s': the text gives %s, the game did not" % (sp, name, fx))
        m = find(lines, r"^Duration: (.+)$")
        t = time_range(m.group(1)) if m else None
        cap = find(lines, r"^Blinds each creature within .+ for up to (.+), and deafens")
        for fx, secs in sorted(got.items()):
            if cap and fx == "Blindness":
                tt = time_range(cap.group(1))
            elif cap and fx == "Deafness":
                continue  # no timer
            else:
                tt = t
            if not tt:
                continue
            for x in secs:
                if x is None or x > tt[1] or (on_player and x < tt[0]):
                    bad.append("game: spell %d '%s': %s for %s seconds, the text gives '%s'" %
                               (sp, name, fx, x, m.group(1) if m else "?"))
                    break

    # each potion
    for p in POTIONS:
        lines = texts.get(("item", "16.%d" % p), [])
        name = lines[0] if lines else "?"
        frames = s.marks[("potion", p)]
        use_c = [l for f in frames for l in cl.get(f, [])]
        use_o = [l for f in frames for l in ol.get(f, [])]
        dump = cl.get(s.marks[("potion_dump", p)][0], [])
        n += 1
        m = find(lines, r"^Heals (\d+) to (\d+) hit points\.") or find(lines, r"^Adds (\d+) to (\d+) hit points above")
        if m:
            got = [int(x) for l in use_c for x in re.findall(r"use: .*?[,:] (\d+) hit points", l)]
            lo, hi = int(m.group(1)), int(m.group(2))
            if len(got) < 25 or min(got) < lo or max(got) > hi or min(got) > lo + (hi - lo) // 3 or max(got) < hi - (hi - lo) // 3:
                bad.append("game: potion %d '%s': the text gives %d to %d hit points, the game %s to %s in %d uses" %
                           (p, name, lo, hi, min(got) if got else "?", max(got) if got else "?", len(got)))
        m = find(lines, r"^Gives (.+) for (.+)\.$") or find(lines, r"hit points above the maximum and (Bless) for (.+)\.$")
        if m:
            t = time_range(m.group(2))
            got = [float(x) for l in use_o for x in re.findall(r"effect %s set on obj 0 you for ([0-9.]+) seconds" % re.escape(m.group(1)), l)]
            if not t or not got or min(got) < t[0] or max(got) > t[1]:
                bad.append("game: potion %d '%s': the text gives %s for '%s', the game %s" % (p, name, m.group(1), m.group(2), got))
        m = find(lines, r"^Casts (.+) on you\.$")
        if m:
            for spell in m.group(1).split(", "):
                if not find(use_o, r"spell: '%s' \(\d+\) level \d+ on obj 0 you" % re.escape(spell)):
                    bad.append("game: potion %d '%s': the game did not cast %s" % (p, name, spell))
        for word, key in (("Dexterity", "dex"), ("Strength", "str")):
            m = find(lines, r"^%s becomes at least (\d+) for" % word)
            got = find(dump, r" %s (\d+)" % key)
            if m and (not got or got.group(1) != m.group(1)):
                bad.append("game: potion %d '%s': the text gives %s %s, the game %s" %
                           (p, name, key, m.group(1), got.group(1) if got else "?"))
        m = find(lines, r"^Intelligence becomes (\d+) to (\d+) for")
        if m:
            got = find(dump, r" int (\d+)")
            if not got or not int(m.group(1)) <= int(got.group(1)) <= int(m.group(2)):
                bad.append("game: potion %d '%s': intelligence %s" % (p, name, got.group(1) if got else "?"))
        if find(lines, r"^Ends all spell and potion effects on you"):
            got = find(dump, r"effects 0x([0-9a-f]+),")
            if not got or int(got.group(1), 16) != 0:
                bad.append("game: potion %d '%s': effects %s are left" % (p, name, got.group(1) if got else "?"))
        if find(lines, r"one attack roll is a 20"):
            got = find(dump, r"potions \d \d \d \d (\d)")
            if not got or got.group(1) != "1":
                bad.append("game: potion %d '%s': no luck in the game" % (p, name))
        if len(lines) < 3:
            bad.append("game: potion %d has no text" % p)

    # each wand: the spell that the game casts
    for w in WANDS:
        lines = texts.get(("item", "5.%d" % w), [])
        m = find(lines, r"^Each use casts (.+)\.$")
        got = find(ol.get(s.marks[("wand", w)][0], []), r"the player casts '(.+)' \(")
        n += 1
        if not m or not got or m.group(1) != got.group(1):
            bad.append("game: wand %d '%s': the text casts %s, the game %s" %
                       (w, lines[0] if lines else "?", m.group(1) if m else "nothing", got.group(1) if got else "nothing"))
    print("%s: %d items and spells against the game, %d differ" % (run_name, n, len(bad)))
    return bad


# ---------------------------------------------------------------------------
# The condition lines of the spell texts against the game. The lines come
# from the conditions of the rule table (DTU_SR_ of dtuspell.h), and the
# cases of the effect function test the same conditions (rule_reaches,
# rule_save). These runs check that a line and the game agree:
# - "Also hits you when you are in the area.": each area spell that lands
#   is cast at the player; with the line the player must be hit, without it
#   (and without "yours too") never.
# - "Stops Magic Missile.": a creature casts Magic Missile at the player
#   with Shield (no damage) and without it (damage).
# - "Causes X on creatures of level N or less." and "Holds undead of level
#   N or less.": the spells on the creatures of BANE: no target above level
#   N and no living target of Hold Undead gets the effect, and a target at
#   or below N does.
# - "Saving throw: None": the target rolls no save.

COND_TRIALS = 12
COND_SELF_TRIALS = 4
COND_SPELLS = [23, 44, 46]
BANE_TARGETS = [6, 8, 13, 16, 18, 42]
BANE_UNDEAD = {6, 8, 13}  # Banegard, ske_axe, Zombie (MONSTER_F_UNDEAD)


def run_script(args, d, level, s, extra):
    os.makedirs(os.path.join(d, "user"), exist_ok=True)
    obj_log = os.path.join(d, "obj.log")
    cmd = [os.path.abspath(args.exe)]
    for dd in args.datadir:
        cmd += ["-datadir", dd]
    cmd += ["-nomovies", "-mute", "-hidden", "-fixedfps", "30", "-userdir", os.path.join(d, "user"), "-level", level,
            "-frames", str(s.frame + 20), "-chartest", ",".join(s.cmds), "-objlog", obj_log] + extra
    r = subprocess.run(cmd, capture_output=True, text=True, errors="replace", timeout=600, cwd=d)
    return parse_texts(r.stdout), by_frame(obj_log, r"\s*[0-9.]+\s+(\d+) ")


def trials_of(ol, frames):
    """[(obj, level, damage, {effects}, [lines])] of the spelltest trials in the frames."""
    res = []
    for f in frames:
        cur = None
        for line in ol.get(f, []):
            m = re.search(r"spelltest: '.+' \(\d+\) level \d+ on obj (\d+) .*\(level (\d+)\), trial", line)
            if m:
                cur = [int(m.group(1)), int(m.group(2)), 0, set(), []]
                res.append(cur)
                continue
            if cur is None:
                continue
            cur[4].append(line)
            m = re.search(r"spelltest: result: hit points (-?\d+) to (-?\d+)", line)
            if m:
                cur[2] = int(m.group(1)) - int(m.group(2))
            m = re.search(r"spelltest:   new effect (.+?),", line)
            if m:
                cur[3].add(m.group(1))
    return res


def saves_of(trial):
    return [l for l in trial[4] if re.search(r"save: obj %d " % trial[0], l)]


def check_conditions(args, tmp, run_name, extra):
    bad = []
    n = 0
    # the player as the target of each spell, and Magic Missile against Shield
    s = Script()
    s.add("class=1")
    s.add("xp=200000")
    s.add("desc=%d" % SPELL_LEVEL, "desc")
    s.frame += 2
    # one trial for each heal: the trials do not give the hit points back
    for sp in ALL_SPELLS:
        for i in range(COND_SELF_TRIALS):
            s.add("heal=500")
            s.add("spelltest=%d.%d.-1.1" % (sp, SPELL_LEVEL), ("self", sp))
    s.add("heal=500")
    s.add("spelltest=45.%d.-1.0" % SPELL_LEVEL, "shield")
    for i in range(6):
        s.add("castby=33.%d" % TARGET, "mm_shield")
    s.add("spelltest=13.%d.-1.0" % SPELL_LEVEL)
    for i in range(6):
        s.add("heal=500")
        s.add("castby=33.%d" % TARGET, "mm_free")
    texts, ol = run_script(args, os.path.join(tmp, run_name + "-self"), "YPINN", s, extra)
    if not texts:
        return ["conditions: no texts"]
    for sp in ALL_SPELLS:
        lines = texts.get(("spell", str(sp)), [])
        tr = [t for t in trials_of(ol, s.marks[("self", sp)]) if t[0] == 0]
        if not lines or not tr:
            bad.append("conditions: spell %d: no text or no trial on the player" % sp)
            continue
        hit = any(t[2] > 0 or t[3] for t in tr)
        if any(re.search(r"^Area: .* where it lands$", l) for l in lines):
            n += 1
            if "Also hits you when you are in the area." in lines and not hit:
                bad.append("conditions: spell %d '%s': the text hits the caster in the area, the game does not" % (sp, lines[0]))
            elif ("Also hits you when you are in the area." not in lines and not any("yours too" in l for l in lines)
                  and hit):
                bad.append("conditions: spell %d '%s': the game hits the caster in the area, the text does not say it" %
                           (sp, lines[0]))
        if "Saving throw: None" in lines:
            n += 1
            if any(saves_of(t) for t in tr):
                bad.append("conditions: spell %d '%s': no saving throw in the text, the player rolls one" % (sp, lines[0]))
    shield = texts.get(("spell", "45"), [])
    if "Stops Magic Missile." in shield:
        n += 1
        hits = lambda mark: [l for f in s.marks[mark] for l in ol.get(f, [])
                             if re.search(r"spell: 'Magic Missile' level \d+ on obj 0 you: damage", l)]
        if hits("mm_shield") or not hits("mm_free"):
            bad.append("conditions: Shield: %d Magic Missile hits with Shield, %d without" %
                       (len(hits("mm_shield")), len(hits("mm_free"))))
    # the level limits and the undead of BANE
    s = Script()
    s.add("class=1")
    s.add("xp=200000")
    s.add("desc=%d" % SPELL_LEVEL, "desc")
    s.frame += 2
    for sp in COND_SPELLS:
        for t in BANE_TARGETS:
            s.add("spelltest=%d.%d.%d.%d" % (sp, SPELL_LEVEL, t, COND_TRIALS), ("cond", sp, t))
    texts, ol = run_script(args, os.path.join(tmp, run_name + "-bane"), "BANE", s, extra)
    if not texts:
        return bad + ["conditions: no texts of BANE"]
    for sp in COND_SPELLS:
        lines = texts.get(("spell", str(sp)), [])
        m1 = find(lines, r"^Causes (.+) on creatures of level (\d+) or less\.( The target may drop its weapon\.)?$")
        m2 = find(lines, r"^Holds undead of level (\d+) or less\.$")
        if not (m1 or m2):
            bad.append("conditions: spell %d '%s': no level limit line" % (sp, lines[0] if lines else "?"))
            continue
        fx, cap = (m1.group(1), int(m1.group(2))) if m1 else ("Hold Person", int(m2.group(1)))
        reached = 0
        for t in BANE_TARGETS:
            tr = [x for x in trials_of(ol, s.marks[("cond", sp, t)]) if x[0] == t]
            if not tr:
                bad.append("conditions: spell %d on obj %d: no trial" % (sp, t))
                continue
            n += 1
            level, got = tr[0][1], any(fx in x[3] for x in tr)
            out = level > cap or (m2 and t not in BANE_UNDEAD)
            if out and got:
                bad.append("conditions: spell %d '%s': obj %d (level %d%s) gets %s, the text limits it to level %d%s" %
                           (sp, lines[0], t, level, ", undead" if t in BANE_UNDEAD else "", fx, cap,
                            " and undead" if m2 else ""))
            reached += got
            if "Saving throw: None" in lines and any(saves_of(x) for x in tr):
                bad.append("conditions: spell %d '%s': no saving throw in the text, obj %d rolls one" % (sp, lines[0], t))
        if not reached:
            bad.append("conditions: spell %d '%s': no target of BANE gets %s" % (sp, lines[0], fx))
    print("%s: %d condition lines against the game, %d differ" % (run_name, n, len(bad)))
    return bad


def main():
    p = argparse.ArgumentParser()
    p.add_argument("--exe", required=True)
    p.add_argument("--datadir", action="append", default=[])
    p.add_argument("--dump")
    p.add_argument("--only")
    args = p.parse_args()
    tmp = tempfile.mkdtemp(prefix="check_desc_")
    problems = []
    first = None
    for name, extra in RUNS:
        if args.only and args.only != name:
            continue
        out = run(args, name, extra, tmp)
        if first is None:
            first = out
        problems += check(out, name)
    # the item corrections on and off (Dtu_rulebook_items, dtuitemfix.h)
    for name, extra in GAME_RUNS:
        if not args.only or args.only == "game":
            problems += [name + " " + pr for pr in check_game(args, tmp, name, extra)]
    if not args.only or args.only in ("game", "conditions"):
        problems += ["game-conditions " + pr for pr in check_conditions(args, tmp, "game-conditions", [])]
    if args.dump and first is not None:
        with open(args.dump, "w", encoding="utf-8") as f:
            f.write("".join(line + "\n" for line in first.splitlines() if line.startswith("desc:")))
    for pr in problems:
        print("FAIL", pr)
    print("%d problems" % len(problems))
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
