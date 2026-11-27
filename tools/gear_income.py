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

This tool reads only. Keep the rule here the same as dtugear.c.
"""
import collections
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
sys.dont_write_bytecode = True
import lvl  # noqa: E402
import objcheck  # noqa: E402

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
NAMED = {"durnan", "mirt", "aleena", "drowfem", "flaynpc", "fmtnpc", "priestes", "daland", "thashasa",
         "halaster", "ibbalar", "jeryth", "avatar"}

# The person models of Improved AI (dtuobj_ai.c dtu_model_is_person)
PERSON = ("hum_", "haf", "dwf_", "drw", "dlf_", "elf_", "orc_", "gob_", "kob", "ogr", "ognew", "armogr",
          "troll", "armtroll", "lic_", "min_", "ban_", "ghu_", "gha_", "mum_", "gar_",
          "durnan", "daland", "thashasa", "hal.", "ibba")

GEM_VALUE = {"Bloodstone": 50, "Pearl": 100, "Ruby": 500, "Diamond": 1000, "Star Sapphire": 5000}


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
    if xp < 100:
        return GEM_VALUE["Bloodstone"]
    if xp < 500:
        return 0.75 * GEM_VALUE["Pearl"] + 0.25 * GEM_VALUE["Ruby"]
    if xp < 2000:
        return 2.0 / 3 * GEM_VALUE["Ruby"] + 1.0 / 3 * GEM_VALUE["Diamond"]
    return 0.9 * GEM_VALUE["Diamond"] + 0.1 * GEM_VALUE["Star Sapphire"]


GEM_CHANCE = 0.25
GOLD_CHANCE = 0.6


def old_gold(moola, f):
    # 0x6A639: HUMANOID, 4 in 10, 1 to moola/4
    q = moola >> 2
    return 0.4 * (q + 1) / 2.0 if "HUMANOID" in f and q > 0 else 0.0


def new_gold(moola, name):
    # moola/2 + 1d(moola), 6 in 10
    if moola <= 0 or name.lower() in NAMED:
        return 0.0
    return GOLD_CHANCE * (moola // 2 + (moola + 1) / 2.0)


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
    print("%-6s %6s %10s %10s %10s %10s" % ("group", "kills", "gold 1.3", "gold new", "gems new", "new total"))
    tot = [0, 0, 0, 0]
    for g, k, og, ng, gem in rows:
        print("%-6s %6d %10.0f %10.0f %10.0f %10.0f" % (g, k, og, ng, gem, ng + gem))
        tot = [tot[0] + k, tot[1] + og, tot[2] + ng, tot[3] + gem]
    print("%-6s %6d %10.0f %10.0f %10.0f %10.0f" % ("all", tot[0], tot[1], tot[2], tot[3], tot[2] + tot[3]))
    if show:
        for c in ("undead", "intelligent", "none", "named"):
            print("\n%s: %s" % (c, ", ".join(sorted(classes[c], key=str.lower))))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
