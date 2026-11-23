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

    def run(self, name, commands, extra=()):
        """commands: a list of -chartest commands, one each second frame
        from frame 5. Returns the dumps (in order) and the -objlog text."""
        d = os.path.join(self.tmp, name)
        os.makedirs(os.path.join(d, "user"), exist_ok=True)
        script = os.path.join(d, "script.txt")
        frames = 5 + 2 * len(commands) + 10
        with open(script, "w") as f:
            f.write("wait %d\nquit\n" % frames)
        chartest = ",".join("%d:%s" % (5 + 2 * i, c) for i, c in enumerate(commands))
        charlog = os.path.join(d, "char.log")
        objlog = os.path.join(d, "obj.log")
        cmd = [os.path.abspath(self.args.exe)]
        for dd in self.args.datadir:
            cmd += ["-datadir", dd]
        cmd += ["-nomovies", "-mute", "-userdir", os.path.join(d, "user"), "-level", LEVEL,
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
    # not count in the boot place.
    check(d[0].ac == -9, "plate set: AC %d, expected -9" % d[0].ac)
    check(d[0].locks == 0 and d[0].traps == 0, "Gauntlets: thief skills %d %d" % (d[0].locks, d[0].traps))
    # Leather Armor -2, Cloak +3 -3, Rings +3 and +2: -3, Amulet -5, Gauntlets
    # -1. Only the highest of the cloak and the ring counts
    # (Dtu_fix_ring_magic_armor; 1.3 adds them: -4).
    check(d[1].ac == -1, "leather set: AC %d, expected -1" % d[1].ac)
    return "AC -9 and -1"


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
    armor, a ring and a cloak of protection), with any armor, shield or
    bracers; a shield adds all of its armor class; the better armor class of the Armor and Shield spells and the
    worn armor. -origcombat: the rules of 1.3."""
    d, _, _ = t.run("protection", PROTECTION)
    o, _, _ = t.run("protection_orig", PROTECTION, ["-origcombat"])
    acs, orig = [x.ac for x in d], [x.ac for x in o]
    check(acs == [2, 2, 3, 8, 6, 5, 4, 0, 6, 3, 6, 6, 4, 6, 4, 3], "AC %s" % acs)
    check(orig == [4, 4, 4, 7, 7, 7, 6, 3, 7, 4, 6, 6, 4, 6, 3, 3], "-origcombat: AC %s" % orig)
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


TESTS = [test_potions, test_potion_class, test_wands, test_rings, test_armor_set, test_protection, test_whin, test_misc]


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
