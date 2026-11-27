#!/usr/bin/env python3
"""The gold and the gems that the creatures of the levels leave, by mission
group, with the rule of 1.3 and with the rule of the setting "New gear"
(Dtu_new_gear, engine/main/dtugear.c).

    python -I tools/gear_income.py [DIR] [--classes]

DIR holds the unpacked .LVL and .BIN files (default gamedata/unpacked of
the main checkout). The count is the creatures that the level files place
(the objects of type 2); creatures that a script or a maker brings in and
the items that a creature carries (contains) are not in it. The values are
the table values; the market pays 75 percent of them at Charisma 12.

--classes lists the creature types of each gem class (undead, intelligent,
none) and the types that leave no gold (the named people).

This tool reads only. The numbers of the rule come from dtugear.h and
dtugear.c (tools/gear_tables.py); the gem classes follow dtu_gear_gem_class.
"""
import collections
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
sys.dont_write_bytecode = True
import lvl  # noqa: E402
import objcheck  # noqa: E402
import gear_tables  # noqa: E402  (the numbers of engine/main/dtugear.c and dtugear.h)

G = gear_tables.read()
D = G["d"]

DEFAULT_DIR = r"D:/dev/ASCENT/gamedata/unpacked"

# The levels of DTU.MSN by the mission groups of Khelben (docs/re/quests.md,
# section 1). The hub of each part is in the group that starts there.
GROUPS = [
    ("0-2", ["YPINN", "HUB1", "KOBOLDS", "BONES"]),
    ("3-6", ["BAT", "THIEF0", "THIEF1", "THIEF2"]),
    ("7-11", ["HUB2", "BANE", "PRISON", "MINE", "CRYPTS", "MULHOR1", "MULHOR2", "HUB3", "MYRKUL"]),
    ("12-19", ["CLAN1", "CLAN2", "CLAN3", "DROW1", "DROW2", "DROW3", "DROW4"]),
    ("20-23", ["BEHOLDER", "BEHOLDR2"]),
    ("24-31", ["HUB4", "DROW5", "MADLANDS", "IBBALAR", "IBBALAR2", "PLAYGRND", "FLAME"]),
]

# The named people: no gold (dtugear.c Gear_no_gold, by the name= of the line)
NAMED = set(n.lower() for n in G["no_gold"])

# The person models of Improved AI (dtuobj_ai.c dtu_model_is_person)
PERSON = ("hum_", "haf", "dwf_", "drw", "dlf_", "elf_", "orc_", "gob_", "kob", "ogr", "ognew", "armogr",
          "troll", "armtroll", "lic_", "min_", "ban_", "ghu_", "gha_", "mum_", "gar_",
          "durnan", "daland", "thashasa", "hal.", "ibba")

# The gems: the new lines of dtugear.c, the Ruby and the Diamond of 1.3
GEM_VALUE = {"Ruby": 500, "Diamond": 1000}
for _l in G["lines"]:
    if _l.startswith("$OBJECT"):
        GEM_VALUE[objcheck.value(_l, "name")] = int(objcheck.value(_l, "value"))


def flags_of(line):
    return set(w for w in re.sub(r'"[^"]*"', " ", line).split() if "=" not in w and w.isupper())


def num(line, key, default=0):
    v = objcheck.value(line, key)
    try:
        return int(float(v)) if v not in (None, "") else default
    except ValueError:
        return default


def gem_class(line):
    """'undead', 'intelligent' or None (dtugear.c gem_class)."""
    f = flags_of(line)
    if "UNDEAD" in f:
        return "undead"
    model = (objcheck.value(line, "model") or "").split()
    model = model[0].lower() if model else ""
    if "HUMANOID" in f or num(line, "door_opening") == 1 or model.startswith(PERSON):
        return "intelligent"
    # spell casters: a spell level, a spell as the weapon, the eye rays
    wt = num(line, "weapon_type", 0)
    if num(line, "spell_level") > 0 or wt < 0 or 81 <= wt <= 88:
        return "intelligent"
    return None


def gem_expect(xp):
    """The expected table value of the gem of one gem roll (dtugear.c)."""
    if xp < D["DTU_GEAR_GEM_XP_PEARL"]:
        return GEM_VALUE["Bloodstone"]
    if xp < D["DTU_GEAR_GEM_XP_RUBY"]:
        p = D["DTU_GEAR_PEARL_IN_4"] / 4.0
        return p * GEM_VALUE["Pearl"] + (1 - p) * GEM_VALUE["Ruby"]
    if xp < D["DTU_GEAR_GEM_XP_DIAMOND"]:
        p = D["DTU_GEAR_RUBY_IN_3"] / 3.0
        return p * GEM_VALUE["Ruby"] + (1 - p) * GEM_VALUE["Diamond"]
    p = 1.0 / D["DTU_GEAR_SAPPHIRE_ONE_IN"]
    return (1 - p) * GEM_VALUE["Diamond"] + p * GEM_VALUE["Star Sapphire"]


# The approved table had gold moola/2 + 1d(moola) and gems in 1 of 4; the
# dice of dtugear.h are the ones that reach the target
GEM_CHANCE = 1.0 / D["DTU_GEAR_GEM_ONE_IN"]
GOLD_CHANCE = D["DTU_GEAR_GOLD_IN_10"] / 10.0


def old_gold(moola, f):
    # 0x6A639: HUMANOID, 4 in 10, 1 to moola/4
    q = moola >> 2
    return 0.4 * (q + 1) / 2.0 if "HUMANOID" in f and q > 0 else 0.0


def new_gold(moola, name):
    # moola + 1d(GOLD_DICE x moola) in GOLD_IN_10 of 10 deaths
    if moola <= 0 or name.lower() in NAMED:
        return 0.0
    return GOLD_CHANCE * (moola + (D["DTU_GEAR_GOLD_DICE"] * moola + 1) / 2.0)


def main(argv):
    d = DEFAULT_DIR
    show = "--classes" in argv
    args = [a for a in argv if not a.startswith("--")]
    if args:
        d = args[0]
    classes = collections.defaultdict(set)
    rows = []
    for gname, levels in GROUPS:
        og = ng = gem = 0.0
        kills = 0
        for n in levels:
            bin_, lv_ = os.path.join(d, n + ".BIN"), os.path.join(d, n + ".LVL")
            if not (os.path.exists(bin_) and os.path.exists(lv_)):
                continue
            t = objcheck.table_lines(bin_)
            mons = t["$MONSTER"]
            with open(lv_, "rb") as f:
                lv = lvl.Level(f.read(), n)
            for o in lv.objects:
                if o["type"] != 2 or o["id"] >= len(mons):
                    continue
                line = mons[o["id"]]
                name = objcheck.value(line, "name") or "?"
                moola, xp, f = num(line, "moola"), num(line, "score_value"), flags_of(line)
                if name.lower() in NAMED:
                    classes["named"].add(name)
                    continue
                kills += 1
                og += old_gold(moola, f)
                ng += new_gold(moola, name)
                c = gem_class(line)
                classes[c or "none"].add("%s (%d xp)" % (name, xp))
                if c:
                    gem += GEM_CHANCE * gem_expect(xp)
        rows.append((gname, kills, og, ng, gem))
    # sold: the gold and 75 percent of the gem values (the market at
    # Charisma 12); an item of the market costs 1.35 times its value
    print("%-6s %6s %9s %9s %9s %9s %9s" % ("group", "kills", "gold 1.3", "gold new", "gems new", "total", "as gold"))
    tot = [0, 0, 0, 0]
    for g, k, og, ng, gem in rows:
        print("%-6s %6d %9.0f %9.0f %9.0f %9.0f %9.0f" % (g, k, og, ng, gem, ng + gem, ng + 0.75 * gem))
        tot = [tot[0] + k, tot[1] + og, tot[2] + ng, tot[3] + gem]
    print("%-6s %6d %9.0f %9.0f %9.0f %9.0f %9.0f" % ("all", tot[0], tot[1], tot[2], tot[3], tot[2] + tot[3],
                                                     tot[2] + 0.75 * tot[3]))
    if show:
        for c in ("undead", "intelligent", "none", "named"):
            print("\n%s: %s" % (c, ", ".join(sorted(classes[c], key=str.lower))))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
