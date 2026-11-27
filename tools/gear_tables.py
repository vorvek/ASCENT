#!/usr/bin/env python3
"""The tables of the new gear (the setting "New gear", engine/main/dtugear.c
and dtugear.h) for the tools: tools/gear_income.py and the wiki page
(tools/wiki_rules.py). The values come from the C files, so the tools follow
a change of a table.

  import gear_tables
  g = gear_tables.read()      # a dict, see read()

    python -I tools/gear_tables.py        prints what it read

Keep each row of the tables of dtugear.c on one line.
"""
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, ".."))
MAIN = os.path.join(ROOT, "engine", "main")


def _read(name):
    with open(os.path.join(MAIN, name), encoding="latin-1") as f:
        return f.read()


def _block(text, name):
    """The text between the "{" after name and its "};"."""
    i = text.index(name)
    j = text.index("{", i)
    k = text.index("};", j)
    return text[j + 1:k]


def defines(text):
    out = {}
    for m in re.finditer(r"^#define\s+(DTU_GEAR_\w+)\s+(0x[0-9A-Fa-f]+|-?\d+)", text, re.M):
        out[m.group(1)] = int(m.group(2), 0)
    return out


def read():
    """A dict:
    d:        the DTU_GEAR_ defines of dtugear.h (numbers)
    lines:    the new table lines, as text ("$ARMOR name=... ")
    armor_ac: the armor class of armor 53 and up (the table Gear_armor_ac)
    wands:    [(id, name of 1.3, new name, spell)]
    values:   [(id, name of 1.3, value)]
    market:   [(stage, quantity, class, number, count)]
    removed:  [(stage, class, number)]
    dragon:   [(class, number, count)]
    no_gold:  [names]
    daland:   (script, misc number of the potion, weapon number)
    flame:    (level line of DTU.MSN, weapon number, (x, y, z))
    """
    c, h = _read("dtugear.c"), _read("dtugear.h")
    g = {"d": defines(h)}
    g["lines"] = [s.replace('\\"', '"') for s in re.findall(r'^\s*"(\$(?:ARMOR|OBJECT) (?:[^"\\]|\\.)*)",?\s*$',
                                                            _block(c, "Gear_lines[]"), re.M)]
    g["armor_ac"] = [int(v) for v in re.findall(r"-?\d+", _block(c, "Gear_armor_ac[DTU_GEAR_N_ARMOR]"))]
    g["wands"] = [(int(a), b, n, int(s)) for a, b, n, s in
                  re.findall(r'\{\s*(\d+),\s*"([^"]*)",\s*"([^"]*)",\s*(\d+)\s*\}', _block(c, "Gear_wands[DTU_GEAR_N_WANDS]"))]
    m = re.search(r'Gear_frost\s*=\s*\{\s*(\w+),\s*"([^"]*)",\s*(\w+),\s*"([^"]*)",\s*"([^"]*)",\s*(\d+)\s*\}', c)
    g["frost"] = (g["d"].get(m.group(1)) or int(m.group(1)), m.group(2),
                  g["d"].get(m.group(3)) or int(m.group(3)), m.group(4), m.group(5), int(m.group(6)))
    g["values"] = [(int(a), b, int(v)) for a, b, v in
                   re.findall(r'\{\s*(\d+),\s*"([^"]*)",\s*(\d+)\s*\}', _block(c, "Gear_values[]"))]

    def num(tok):
        tok = tok.strip()
        return g["d"][tok] if tok in g["d"] else int(tok)
    g["market"] = [tuple(num(t) for t in m.group(1).split(","))
                   for m in re.finditer(r"\{([^{}]*)\}", _block(c, "Gear_market[][5]"))]
    g["removed"] = [tuple(num(t) for t in m.group(1).split(","))
                    for m in re.finditer(r"\{([^{}]*)\}", _block(c, "Gear_market_removed[][3]"))]
    g["dragon"] = [tuple(num(t) for t in m.group(1).split(","))
                   for m in re.finditer(r"\{([^{}]*)\}", _block(c, "Gear_dragon[][3]"))]
    g["no_gold"] = re.findall(r'"([^"]*)"', _block(c, "Gear_no_gold[]"))
    m = re.search(r"#define\s+DALAND_SCRIPT\s+(\d+)\s+#define\s+DALAND_POTION\s+(\d+)\s+#define\s+MACE\s+(\d+)", c)
    g["daland"] = tuple(int(x) for x in m.groups())
    s = _read("dtuobj_setup.c")
    lvl = int(re.search(r"#define\s+FT_LEVEL\s+(\d+)", s).group(1))
    wpn = int(re.search(r"#define\s+FT_WEAPON\s+(\d+)", s).group(1))
    m = re.search(r"Ft_pos\s*=\s*\{\s*([^}]*)\}", s)
    pos = []
    for part in m.group(1).split(","):
        # "-172 * F1_0 - F1_0 / 2" and the like: F1_0 is 1.0
        pos.append(float(eval(part.replace("F1_0", "1.0"), {"__builtins__": {}})))
    g["flame"] = (lvl, wpn, tuple(pos))
    for k in ("lines", "armor_ac", "wands", "values", "market", "dragon", "no_gold"):
        if not g[k]:
            raise SystemExit("gear_tables: no rows for %s in dtugear.c" % k)
    return g


if __name__ == "__main__":
    import pprint
    pprint.pprint(read(), width=140)
    sys.exit(0)
