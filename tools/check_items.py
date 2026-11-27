#!/usr/bin/env python3
"""Checks of the item effects (docs/audit/gear.md, docs/re/objects.md).

Each check starts the game hidden and muted at YPINN with -chartest
commands and reads the -charlog and -objlog lines. The checks need the game
files, so they are not registered with ctest.

    python tools/check_items.py --exe build/engine/main/ascent.exe \
        --datadir E:/UNDAMONTAN/DTU --datadir D:/dev/ASCENT/gamedata/cd/DTU \
        [--only NAME]

The default character is a human fighter: strength 17, dexterity 14
(armor class adjustment 0), 12 hit points.
"""
import argparse
import os
import re
import subprocess
import sys
import tempfile

LEVEL = "YPINN"

# The effect bits of the object (dtuspell.h DTU_FX_)
FX = {
    "armor": 0, "bless": 1, "detect_magic": 6, "feather_fall": 7, "resist_fire": 10,
    "fly": 11, "free_action": 12, "haste": 13, "infravision": 15, "imp_invis": 16,
    "invisible": 17, "invulnerable": 18, "jump": 19, "levitate": 21, "shield": 25,
}


class Dump:
    """The values of one "dump" of the -charlog."""

    def __init__(self, lines):
        m = re.search(r"hit points (\d+) of (\d+), armor class (-?\d+)", lines[0])
        self.hp, self.max_hp, self.ac = int(m.group(1)), int(m.group(2)), int(m.group(3))
        m = re.search(r"str (\d+)/(\d+) dex (\d+) con (\d+) int (\d+) wis (\d+) cha (\d+)", lines[1])
        (self.str, self.pct, self.dex, self.con, self.int, self.wis, self.cha) = [int(x) for x in m.groups()]
        m = re.search(r"effects 0x([0-9a-f]+), worn 0x([0-9a-f]+), skills (-?\d+) (-?\d+) (-?\d+) (-?\d+) (-?\d+), "
                      r"potions (\d) (\d) (\d) (\d) (\d), flag 77 (\d)", lines[2])
        self.fx = int(m.group(1), 16)
        self.worn = int(m.group(2), 16)
        (self.hide, self.move, self.locks, self.traps, self.climb) = [int(x) for x in m.groups()[2:7]]
        self.potions = [int(x) for x in m.groups()[7:12]]
        self.flag77 = int(m.group(13))
        self.entries = {}
        for line in lines[3:]:
            m = re.search(r"entry (\d+): type (\d+) id (\d+) '.*' count (-?\d+)", line)
            if m:
                self.entries[(int(m.group(2)), int(m.group(3)))] = int(m.group(4))

    def has(self, name):
        return bool(self.fx & (1 << FX[name]))

    def count(self, type_, id_):
        return self.entries.get((type_, id_), 0)


class Runner:
    def __init__(self, args):
        self.args = args
        self.tmp = tempfile.mkdtemp(prefix="itemtest_")

    def run(self, name, commands, extra=(), level=None, more_frames=0):
        """commands: a list of -chartest commands, one each second frame
        from frame 5; "N:cmd" runs at frame N. level: the level (default
        YPINN). more_frames: frames after the last command. Returns the
        dumps (in order) and the -objlog text."""
        d = os.path.join(self.tmp, name)
        os.makedirs(os.path.join(d, "user"), exist_ok=True)
        script = os.path.join(d, "script.txt")
        plain = [c for c in commands if not re.match(r"\d+:", c)]
        timed = [c for c in commands if re.match(r"\d+:", c)]
        last = max([5 + 2 * len(plain)] + [int(c.split(":")[0]) for c in timed])
        frames = last + 10 + more_frames
        with open(script, "w") as f:
            f.write("wait %d\nquit\n" % frames)
        chartest = ",".join(["%d:%s" % (5 + 2 * i, c) for i, c in enumerate(plain)] + timed)
        charlog = os.path.join(d, "char.log")
        objlog = os.path.join(d, "obj.log")
        cmd = [os.path.abspath(self.args.exe)]
        for dd in self.args.datadir:
            cmd += ["-datadir", dd]
        cmd += ["-nomovies", "-mute", "-userdir", os.path.join(d, "user"), "-level", level or LEVEL,
                "-fescript", script, "-fixedfps", "30", "-chartest", chartest,
                "-charlog", charlog, "-objlog", objlog] + list(extra)
        r = subprocess.run(cmd, capture_output=True, text=True, timeout=300, cwd=d)
        if not os.path.exists(charlog):
            raise AssertionError("%s: no -charlog (exit %d)\n%s" % (name, r.returncode, r.stdout[-2000:]))
        with open(charlog, errors="replace") as f:
            lines = f.read().split("\n")
        dumps = []
        for i, line in enumerate(lines):
            if re.search(r"f\d+\s+character '", line):
                block = [line]
                for nxt in lines[i + 1:]:
                    if re.search(r"f\d+\s+  ", nxt) and ("str " in nxt or "effects " in nxt or "entry " in nxt
                                                         or "dungeon level " in nxt):
                        block.append(nxt)
                    else:
                        break
                dumps.append(Dump(block))
        obj = ""
        if os.path.exists(objlog):
            with open(objlog, errors="replace") as f:
                obj = f.read()
        return dumps, obj, "\n".join(lines)


def check(cond, text):
    if not cond:
        raise AssertionError(text)


# ---------------------------------------------------------------------
# Potions (0x37A6A, combat.md B6)

def test_potions(t):
    """Each potion (misc 72 to 88) of 0x37A6A."""
    cmds = []
    order = [72, 74, 80, 81, 76, 77, 78, 82, 83, 84, 85, 86, 87]
    for p in order + [79, 75, 88, 73]:
        cmds.append("give=16.%d.1" % p)
    cmds.append("dump")
    for p in order:
        cmds += ["use=16.%d" % p, "dump"]
    cmds += ["damage=8", "use=16.79", "dump", "damage=8", "use=16.75", "dump"]
    cmds += ["heal=20", "use=16.88", "dump", "use=16.73", "dump"]
    d, _, log = t.run("potions", cmds)
    base = d[0]
    check(base.dex == 14 and base.str == 17, "base values %d %d" % (base.dex, base.str))
    names = {}
    for i, p in enumerate(order):
        names[p] = d[1 + i]
        check(names[p].count(16, p) == 0, "potion %d was not used up" % p)
    check(names[72].dex == 18, "Agility: dexterity %d" % names[72].dex)
    check(names[74].str == 18, "Dragon's Blood: strength %d" % names[74].str)
    check(names[80].str == 19 and names[80].pct == 0, "Hill Giant Strength: %d/%d" % (names[80].str, names[80].pct))
    check(5 <= names[81].int <= 7, "Intelligence potion: %d" % names[81].int)
    check(names[76].has("resist_fire"), "Fire Resistance")
    check(names[77].has("fly"), "Flying")
    check(names[78].has("free_action"), "Freedom")
    check(names[82].has("infravision"), "Infravision")
    check(names[83].has("invisible"), "the second Potion of Agility: Invisibility")
    check(names[84].has("invulnerable") and names[84].ac == names[83].ac - 2,
          "Invulnerability: AC %d after %d" % (names[84].ac, names[83].ac))
    check(names[85].has("jump"), "Jumping")
    check(names[86].flag77 == 1 and names[86].potions[4] == 1, "Luck: flag 77 and the timer")
    check(names[87].has("haste"), "Speed")
    k = 1 + len(order)
    heal, extra, tough, anti = d[k], d[k + 1], d[k + 2], d[k + 3]
    check(heal.hp >= 4 + 4 and heal.count(16, 79) == base.count(16, 79) - 1, "Healing: %d hit points" % heal.hp)
    check(extra.count(16, 75) == 0 and extra.hp >= heal.hp - 8 + 6, "Extra-Healing: %d hit points" % extra.hp)
    check(tough.hp > tough.max_hp and tough.has("bless"), "Toughening: %d of %d" % (tough.hp, tough.max_hp))
    check(anti.fx == 0 and anti.potions == [0, 0, 0, 0, 0] and anti.flag77 == 0,
          "Anti-Magic: effects 0x%x, potions %s, flag 77 %d" % (anti.fx, anti.potions, anti.flag77))
    check(anti.str == 17 and anti.dex == 14, "Anti-Magic: strength %d dexterity %d" % (anti.str, anti.dex))
    return "17 potions"


def test_potion_class(t):
    """Dtu_fix_potion_class: a mage cannot drink the two fighter potions
    (80, 84; table fixup 0x1A344). With -origcombat any class drinks them
    (0x37A6A has no class test). test_potions covers the fighter."""
    cmds = ["class=1", "give=16.80.1", "give=16.84.1", "use=16.80", "use=16.84", "dump"]
    d, _, _ = t.run("potion_class", cmds)
    m = d[0]
    check(m.count(16, 80) == 1 and m.count(16, 84) == 1, "a mage drank a fighter potion")
    check(m.str == 17 and not m.has("invulnerable"), "mage: strength %d, effects 0x%x" % (m.str, m.fx))
    d, _, _ = t.run("potion_class_orig", cmds, ["-origcombat"])
    m = d[0]
    check(m.count(16, 80) == 0 and m.count(16, 84) == 0, "-origcombat: a mage did not drink the fighter potions")
    check(m.str == 19 and m.has("invulnerable"), "-origcombat mage: strength %d, effects 0x%x" % (m.str, m.fx))
    return "a mage refuses potions 80 and 84, -origcombat drinks them"


# ---------------------------------------------------------------------
# Wands (weapons with pc_spell, 0x56A0D)

WANDS = {55: 33, 56: 7, 57: 14, 58: 16, 59: 32, 60: 12, 61: 22, 62: 32}


def test_wands(t):
    """Each wand casts its pc_spell and uses one charge."""
    cmds = ["class=1"]
    for w in WANDS:
        cmds += ["give=5.%d.5" % w, "current=5.%d" % w, "fire", "dump"]
    d, obj, _ = t.run("wands", cmds)
    for i, (w, spell) in enumerate(WANDS.items()):
        check(d[i].count(5, w) == 4, "wand %d: %d charges after one cast" % (w, d[i].count(5, w)))
        check(re.search(r"casts '[^']*' \(%d\) at level \d+, source 2" % spell, obj),
              "wand %d did not cast spell %d" % (w, spell))
    return "%d wands" % len(WANDS)


# ---------------------------------------------------------------------
# Rings (0xAFB55, 0xB08A8)

RINGS = {17: "resist_fire", 18: "feather_fall", 19: "fly", 20: "free_action", 22: "infravision",
         23: "jump", 24: "detect_magic", 25: "haste"}


def test_rings(t):
    """Each ring effect while it is worn, and its end when it is taken off."""
    cmds = []
    for r in RINGS:
        cmds += ["give=15.%d.1" % r, "equip=15.%d" % r, "dump", "unequip=6", "dump"]
    cmds += ["give=15.21.1", "equip=15.21", "dump", "unequip=6"]
    cmds += ["give=15.26.1", "equip=15.26", "dump", "unequip=6"]
    cmds += ["give=15.14.1", "equip=15.14", "dump", "give=15.15.1", "equip=15.15", "dump", "unequip=6", "unequip=7"]
    cmds += ["class=1", "cast=2", "dump", "give=15.16.1", "equip=15.16", "dump", "class=2", "str=17.0"]
    d, _, _ = t.run("rings", cmds)
    for i, (r, fx) in enumerate(RINGS.items()):
        on, off = d[2 * i], d[2 * i + 1]
        check(on.has(fx), "ring %d: no %s (0x%x)" % (r, fx, on.fx))
        check(not off.has(fx), "ring %d: %s stays after the ring is off" % (r, fx))
    k = 2 * len(RINGS)
    check(d[k].con == 18, "Ring of Health: constitution %d" % d[k].con)
    check(d[k + 1].str == 18 and d[k + 1].pct == 0, "Ring of Strength: %d/%d" % (d[k + 1].str, d[k + 1].pct))
    check(d[k + 2].ac == 9, "Ring of Protection +1: AC %d" % d[k + 2].ac)
    check(d[k + 3].ac == 7, "Rings of Protection +1 and +3: the better one counts, AC %d" % d[k + 3].ac)
    check(d[k + 4].ac == 6, "Armor: AC %d" % d[k + 4].ac)
    check(d[k + 5].ac == 5, "Ring of Armoring on a mage with Armor: AC %d (0xAFAD1)" % d[k + 5].ac)
    check(d[k + 6].ac == 6, "Ring of Armoring on a fighter with Armor: AC %d" % d[k + 6].ac)
    return "%d ring effects, Health, Strength, Protection, Armoring" % len(RINGS)


def test_armoring_bracers(t):
    """A mage with the Ring of Armoring and Armor, then Shield: each better
    Bracers of Defense never gives a worse armor class (Dtu_fix_armoring_cap).
    -origcombat: 1.3, AC6 gives 6 with Armor and AC8 gives 5."""
    cmds = ["class=1", "class2=-1", "give=15.16.1", "equip=15.16", "cast=2"]
    for b in (39, 40, 41, 42):
        cmds += ["give=15.%d.1" % b, "equip=15.%d" % b, "dump", "unequip=3"]
    cmds += ["cast=45"]
    for b in (39, 40, 41, 42):
        cmds += ["equip=15.%d" % b, "dump", "unequip=3"]
    d, _, _ = t.run("armoring_bracers", cmds)
    o, _, _ = t.run("armoring_bracers_orig", cmds, ["-origcombat"])
    acs, orig = [x.ac for x in d], [x.ac for x in o]
    check(acs == [5, 5, 4, 2, 3, 3, 3, 2], "AC %s" % acs)
    check(orig == [5, 6, 4, 2, 2, 2, 2, 2], "-origcombat: AC %s" % orig)
    return "AC %s, 1.3 %s" % (acs, orig)


# ---------------------------------------------------------------------
# Armor class of a full set (0xAFB55)

def test_armor_set(t):
    """Two full sets: the rules of the shield, the magic armor, the cloak
    and the rings of protection."""
    a = ["give=15.11.1", "give=15.13.1", "give=15.15.1", "give=15.32.1", "give=15.49.1", "give=16.9.1", "give=16.8.1",
         "equip=15.11", "equip=15.13", "equip=15.15", "equip=15.32", "equip=15.49", "equip=16.9", "equip=16.8", "dump"]
    b = ["unequip=3", "unequip=5", "give=15.1.1", "give=15.0.1", "equip=15.1", "equip=15.0", "dump"]
    d, _, _ = t.run("armor_set", a + b)
    # Plate Mail +3 -10, Shield +2 -3, Amulet +5 -5, Gauntlets -1; the ring and
    # the cloak do not count with magic armor and a shield; the misc Boots do
    # not count in the boot place: 1.3 -9. Dtu_fix_ring_magic_armor: only
    # the Amulet +5 of the magic protection bonuses counts: Plate Mail -7,
    # Shield +2 -3, Amulet -5, Gauntlets -1.
    check(d[0].ac == -6, "plate set: AC %d, expected -6" % d[0].ac)
    check(d[0].locks == 0 and d[0].traps == 0, "Gauntlets: thief skills %d %d" % (d[0].locks, d[0].traps))
    # Leather Armor -2, Cloak +3 -3, Rings +3 and +2: -3, Amulet -5, Gauntlets
    # -1: 1.3 -4. Only the highest of the cloak, the ring and the amulet counts
    # (Dtu_fix_ring_magic_armor): Leather Armor -2, Amulet -5, Gauntlets -1.
    check(d[1].ac == 2, "leather set: AC %d, expected 2" % d[1].ac)
    return "AC -6 and 2"


# ---------------------------------------------------------------------
# Bracers, magic armor and the protection items; Armor and Shield before
# the armor (Dtu_fix_bracers_ac, Dtu_fix_ring_magic_armor)

PROTECTION = [
    "give=15.41.1", "equip=15.41", "give=15.0.1", "equip=15.0", "dump",     # Bracers AC4, Ring of Protection +2
    "give=15.47.1", "equip=15.47", "dump",                                    # and a Cloak of Protection +1
    "unequip=6", "dump",                                                      # Bracers AC4 and the cloak +1
    "unequip=3", "equip=15.0", "dump",                                        # the ring +2 and the cloak +1
    "give=15.5.1", "equip=15.5", "dump",                                      # and Leather Armor +1
    "unequip=6", "unequip=1", "give=15.49.1", "equip=15.49", "dump",          # Leather Armor +1, Cloak +3
    "give=15.2.1", "equip=15.2", "dump", "unequip=5",                         # and a Shield
    "unequip=3", "give=15.4.1", "equip=15.4", "dump", "unequip=3",            # Plate Mail and the cloak +3
    "unequip=1", "give=15.1.1", "equip=15.1", "give=15.47.1", "equip=15.47", "give=15.2.1", "equip=15.2", "dump",
                                                                              # Leather Armor, Shield, Cloak +1
    "unequip=1", "unequip=5", "unequip=3", "equip=15.5", "give=15.13.1", "equip=15.13", "equip=15.0", "dump",
                                                                              # Leather Armor +1, Shield +2, Ring +2
    "unequip=5", "give=15.30.1", "equip=15.30", "dump",                       # Leather Armor +1, Ring +2, Amulet +3
    "unequip=3", "dump",                                                      # Ring +2 and Amulet +3
    "unequip=6", "equip=15.5", "unequip=2", "give=15.28.1", "equip=15.28", "dump",   # Leather Armor +1, Amulet +1
    "unequip=2",
    "unequip=6", "unequip=5", "unequip=3", "equip=15.49",                     # back to the cloak +3 alone
    "unequip=1", "cast=2", "dump",                                            # Armor, no armor
    "equip=15.1", "dump", "unequip=3",                         # and Leather Armor (AC 8)
    "equip=15.41", "dump", "unequip=3",                                       # and Bracers AC4
    "give=15.39.1", "equip=15.39", "dump", "unequip=3",                       # and Bracers AC8
    "cast=45", "equip=15.39", "dump", "unequip=3",                            # Shield and Bracers AC8
    "equip=15.4", "dump",                                                     # Shield and Plate Mail (AC 3)
]


def test_protection(t):
    """Only the highest magic protection bonus counts (the magic of the
    armor, a ring, a cloak and an amulet of protection), with any armor, shield or
    bracers; a shield adds all of its armor class; the better armor class of the Armor and Shield spells and the
    worn armor. -origcombat: the rules of 1.3."""
    d, _, _ = t.run("protection", PROTECTION)
    o, _, _ = t.run("protection_orig", PROTECTION, ["-origcombat"])
    acs, orig = [x.ac for x in d], [x.ac for x in o]
    check(acs == [2, 2, 3, 8, 6, 5, 4, 0, 6, 3, 5, 7, 7, 6, 6, 4, 6, 4, 3], "AC %s" % acs)
    check(orig == [4, 4, 4, 7, 7, 7, 6, 3, 7, 4, 4, 5, 6, 6, 6, 4, 6, 3, 3], "-origcombat: AC %s" % orig)
    return "AC %s, 1.3 %s" % (acs, orig)


# ---------------------------------------------------------------------
# A weapon power: the Sword of W'hin (0xB038F)

def test_whin(t):
    """Weapon 54 in the hand: armor class 9 better, eight effects, Find/Remove
    Traps and Climb Walls 95."""
    d, _, _ = t.run("whin", ["dump", "give=5.54.1", "current=5.54", "dump", "current=5.20", "dump"])
    base, on, off = d
    check(on.ac == base.ac - 9, "AC %d to %d" % (base.ac, on.ac))
    for fx in ("resist_fire", "feather_fall", "free_action", "jump", "detect_magic", "haste", "imp_invis", "infravision"):
        check(on.has(fx), "W'hin: no %s (0x%x)" % (fx, on.fx))
    check(on.traps == 95 and on.climb == 95, "W'hin: traps %d climb %d" % (on.traps, on.climb))
    check(off.fx == 0 and off.ac == base.ac, "W'hin put away: effects 0x%x AC %d" % (off.fx, off.ac))
    return "AC -9, 8 effects, skills 95"


# ---------------------------------------------------------------------
# Misc items

def test_misc(t):
    """The Cursed Scroll stays (0x3817F), the Scroll stays, the Gem of Curing
    is not used up."""
    d, _, _ = t.run("misc", ["give=16.106.1", "use=16.106", "dump", "damage=4", "use=16.2", "dump",
                             "give=16.103.1", "drop=16.103", "give=16.4.1", "drop=16.4", "dump"])
    check(d[0].count(16, 106) == 1, "the Cursed Scroll was used up")
    check(d[1].count(16, 2) == 1 and d[1].hp > d[0].hp - 4, "Gem of Curing")
    check(d[2].count(16, 103) == 1, "the Banegem (plot_item) was dropped (0xB611C)")
    check(d[2].count(16, 4) == 0, "the Ruby was not dropped")
    return "Cursed Scroll, Gem of Curing, plot item drop"


def save_lines(t, name):
    """The save lines of the session log of a run (user/logs)."""
    d = os.path.join(t.tmp, name, "user", "logs")
    out = []
    for f in sorted(os.listdir(d)) if os.path.isdir(d) else []:
        with open(os.path.join(d, f), encoding="utf-8", errors="replace") as fh:
            out += [l for l in fh.read().split("\n") if "Save of the player:" in l]
    return out


def test_saves(t):
    """The bonuses of the saves of the player, each with a session log
    line: the best protection item (Dtu_fix_prot_saves; a Ring of
    Protection +3 and a Cloak +1: +3), the Constitution of a dwarf or a
    halfling against Rod/Staff/Wand and Spell, Wisdom against the spells
    that attack the mind (Dtu_fix_ability_saves). A human fighter of level
    1 needs 17 against a spell. -origcombat: no bonus."""
    cmds = ["give=15.15.1", "equip=15.15", "give=15.47.1", "equip=15.47", "castby=7.17",
            "unequip=6", "unequip=1",
            "race=1", "con=18", "wis=17", "castby=47.17",       # dwarf, Con 18, Slow: +5 (CON), no WIS
            "race=4", "con=11", "castby=30.17",                  # halfling, Con 11, Light: +3 (CON)
            "race=0", "castby=22.17",                             # human, Wis 17, Hold Person: +3 (WIS)
            "wis=4", "castby=14.17"]                              # Wis 4, Fear: -2 (WIS)
    t.run("saves", cmds)
    t.run("saves_orig", cmds, ["-origcombat"])
    new, orig = save_lines(t, "saves"), save_lines(t, "saves_orig")
    want = ["Confusion (Aleena), Spell: d20 %d, needs 14 (table 17, +3 (protection))",
            "Slow (Aleena), Spell: d20 %d, needs 12 (table 17, +5 (CON))",
            "Light (Mage) (Aleena), Spell: d20 %d, needs 14 (table 17, +3 (CON))",
            "Hold Person (Aleena), Spell: d20 %d, needs 14 (table 17, +3 (WIS))",
            "Fear (Aleena), Spell: d20 %d, needs 19 (table 17, -2 (WIS))"]
    check(len(new) == len(want) and len(orig) == len(want), "save lines: %d and %d" % (len(new), len(orig)))
    for l, w in zip(new, want):
        check(re.search(re.escape(w).replace("%d", r"\d+"), l), "line: %s, expected %s" % (l, w))
    for l in orig:
        check(re.search(r"needs 17 \(table 17\):", l), "-origcombat line: %s" % l)
    return "%d lines, e.g. %s" % (len(new), new[1].split("] ", 1)[-1])


# ---------------------------------------------------------------------
# The new gear (Dtu_new_gear, engine/main/dtugear.h)

def test_gear_worn(t):
    """Each new armor (53 to 59) worn, then taken off: the armor class, the
    constitution of the Belt of Dwarvenkind and the effects Free Action,
    Resist Fire, Infravision and Regeneration, logged as worn item bits.
    -nonewgear: the lines do not exist, the give does nothing."""
    cmds = ["dump"] + ["give=15.%d.1" % i for i in range(53, 60)] + ["dump"]
    cmds += ["equip=15.59", "equip=15.53", "dump", "equip=15.54", "dump", "equip=15.56", "equip=15.57", "dump",
             "equip=15.58", "equip=15.55", "dump",
             "unequip=3", "unequip=5", "unequip=9", "unequip=1", "unequip=8", "unequip=6", "unequip=4", "dump"]
    d, _, log = t.run("gear_worn", cmds)
    base, plate, boots, belt, ring, off = d[0], d[2], d[3], d[4], d[5], d[6]
    for i in range(53, 60):
        check(d[1].count(15, i) == 1, "armor %d was not given" % i)
    check(plate.ac == 10 - 9 - 2, "Plate Mail of Fire Resistance and Mirror Shield: armor class %d" % plate.ac)
    check(plate.has("resist_fire"), "Plate Mail of Fire Resistance: no Resist Fire")
    check(boots.has("free_action"), "Boots of Free Action: no Free Action")
    check(belt.con == 18 and belt.has("infravision"), "Belt of Dwarvenkind: con %d, effects 0x%x" % (belt.con, belt.fx))
    check(ring.fx & (1 << 24), "Ring of Regeneration: no Regeneration effect")
    check(off.fx == 0 and off.worn == 0 and off.con == base.con and off.ac == base.ac,
          "after the items came off: effects 0x%x worn 0x%x con %d ac %d" % (off.fx, off.worn, off.con, off.ac))
    check(re.search(r"worn items: effect bits 0x8062", log), "no log line of the worn item bits")
    d, _, _ = t.run("gear_worn_off", ["give=15.53.1", "give=16.128.1", "dump"], ["-nonewgear"])
    check(d[0].count(15, 53) == 0 and d[0].count(16, 128) == 0, "-nonewgear: the new items exist")
    return "AC %d, effects 0x%x, con %d" % (ring.ac, ring.fx, ring.con)


def test_gear_mirror(t):
    """The Mirror Shield at BEHOLDER: a Magic Missile of Xabash (object
    106, a beholder: its eye rays are its spell missiles) that hits the
    shield from the front flies back and hurts Xabash at its caster level;
    from behind it hurts the player."""
    cmds = ["heal=200", "give=15.53.1", "equip=15.53", "goobj=106.40", "face=106", "spellby=33.106"] + ["dump"] * 15 + \
           ["face=2", "spellby=33.106"] + ["dump"] * 15
    _, obj, _ = t.run("gear_mirror", cmds, level="BEHOLDER")
    check(re.search(r"Mirror Shield sends obj \d+ \(weapon \d+ spell 33\) back to obj 106 xabash", obj), "no reflection")
    check(re.search(r"'Magic Missile' \(33\) level 12 on obj 106 xabash", obj), "the reflected missile did not hit Xabash at level 12")
    check(re.search(r"damage: obj 106 xabash takes \d+ from player", obj), "Xabash took no damage")
    check(len(re.findall(r"Mirror Shield sends", obj)) == 1, "the missile from behind was sent back")
    check(re.search(r"damage: the player takes \d+ from xabash", obj), "the missile from behind did not hit the player")
    return "a missile from the front goes back, one from behind hits"


def test_gear_archer(t):
    """The Gloves of the Archer at BAT: arrows from a short bow at a
    skeleton (object 43) get +1 to the roll and +2 damage."""
    def shots(name, gloves):
        cmds = ["give=16.89.1", "equip=16.89", "give=5.7.99", "current=5.7", "goobj=43.15", "face=43", "heal=99"]
        if gloves:
            cmds += ["give=15.55.1", "equip=15.55"]
        cmds += ["aim=43"]
        # the attack key (-testmove) after frame 40, each 20 frames
        _, _, log = t.run(name, cmds, level="BAT", more_frames=260,
                          extra=["-noattack", "-testmove", "wait:40" + ",attack:1,wait:20" * 12])
        rolls = [int(b) - int(a) for a, b in re.findall(r"attack roll: d20 (\d+), roll (-?\d+) against", log)]
        return rolls, log
    a, _ = shots("gear_archer_no", False)
    b, log = shots("gear_archer", True)
    check(a and b, "no attack rolls (%d, %d)" % (len(a), len(b)))
    check(set(b) == {a[0] + 1}, "roll bonus %s without, %s with the gloves" % (sorted(set(a)), sorted(set(b))))
    check(re.search(r"damage roll: the Gloves of the Archer add 2", log), "no damage bonus line")
    return "roll bonus %d -> %d, damage +2" % (a[0], b[0])


def test_gear_regen(t):
    """The Ring of Regeneration: 1 hit point each 60 seconds; none after it
    is taken off."""
    cmds = ["damage=6", "give=15.58.1", "equip=15.58", "dump", "1830:dump", "1850:unequip=6", "3700:dump"]
    d, _, log = t.run("gear_regen", cmds)
    check(len(d) == 3, "%d dumps" % len(d))
    check(d[1].hp == d[0].hp + 1, "after 60 s: %d hit points, %d before" % (d[1].hp, d[0].hp))
    check(d[2].hp == d[1].hp, "the ring was off: %d hit points, %d before" % (d[2].hp, d[1].hp))
    check(re.search(r"Ring of Regeneration: 1 hit point", log), "no log line")
    return "%d -> %d hit points in 60 s" % (d[0].hp, d[1].hp)


GEAR_WANDS = {63: 46, 64: 47, 65: 23, 66: 37, 67: 13}


def test_gear_wands(t):
    """The wands 63 to 67 cast Sleep, Slow, Hold Undead, Melf's Minute
    Meteors and Dispel Magic and use one charge each."""
    cmds = ["class=1"]
    for w in GEAR_WANDS:
        cmds += ["give=5.%d.6" % w, "current=5.%d" % w, "fire", "dump"]
    d, obj, _ = t.run("gear_wands", cmds)
    for i, (w, spell) in enumerate(GEAR_WANDS.items()):
        check(d[i].count(5, w) == 5, "wand %d: %d charges after one cast" % (w, d[i].count(5, w)))
        check(re.search(r"casts '.*?' \(%d\) at level \d+, source 2" % spell, obj), "wand %d did not cast spell %d" % (w, spell))
    return "%d wands" % len(GEAR_WANDS)


def test_gear_drops(t):
    """With the seed 1 the Banegard of BAT (object 31: moola 100, undead,
    270 experience) leaves gold and a gem (a Pearl or a Ruby). -nonewgear:
    the rule of 1.3 (only a HUMANOID type leaves gold)."""
    d, obj, _ = t.run("gear_drops", ["seed=%d" % GEAR_SEED, "kill=31", "dump"], level="BAT", more_frames=400)
    m = re.search(r"obj 31 Banegard leaves (\d+) gold \(obj \d+\) .*\(new gear\)", obj)
    check(m and 100 < int(m.group(1)) <= 300, "no gold of the new rule: %s" % (m.group(0) if m else None))
    check(re.search(r"obj 31 Banegard leaves misc (129|4) ", obj), "no Pearl or Ruby")
    _, obj2, _ = t.run("gear_drops_off", ["seed=%d" % GEAR_SEED, "kill=31", "dump"], ["-nonewgear"], level="BAT", more_frames=400)
    check(not re.search(r"obj 31 Banegard leaves", obj2), "-nonewgear: the Banegard left something")
    return "%s gold and %s" % (m.group(1), re.search(r"leaves misc \d+ '[^']*'", obj).group(0))


GEAR_SEED = 1


TESTS = [test_potions, test_potion_class, test_wands, test_rings, test_armoring_bracers, test_armor_set, test_protection, test_saves, test_whin, test_misc,
         test_gear_worn, test_gear_mirror, test_gear_archer, test_gear_regen, test_gear_wands, test_gear_drops]


def main():
    p = argparse.ArgumentParser()
    p.add_argument("--exe", required=True)
    p.add_argument("--datadir", action="append", default=[])
    p.add_argument("--only")
    args = p.parse_args()
    t = Runner(args)
    failed = 0
    for test in TESTS:
        name = test.__name__[5:]
        if args.only and args.only != name:
            continue
        try:
            print("%-12s ok   %s" % (name, test(t)))
        except AssertionError as e:
            failed += 1
            print("%-12s FAIL %s" % (name, e))
    print("logs in %s" % t.tmp)
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
