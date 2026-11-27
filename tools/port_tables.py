#!/usr/bin/env python3
"""The tables of the port that the wiki pages need, read from the C files so
that the pages follow a change of a table (tools/wiki_rules.py):

  - the spell lines of the port and their cast times (engine/main/dtuspell.c,
    dtu_spell_port_line, port_cast_time),
  - the icons of the port (engine/main/dtuspellicons.h, written by
    tools/spell_icons.py),
  - the spell lists of the creature casters (engine/main/dtucaster.h) and the
    numbers of the Caster AI (dtuspell.c),
  - the numbers of Improved AI (engine/main/dtuobj_aware.c),
  - the dragon Tamaramagil (engine/main/dtudragon.c).

  import port_tables
  s = port_tables.spells()     # see each function

    python -I tools/port_tables.py        prints what it read

None of these tables is in DTUGAME.EXE 1.3.
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


def _value(expr):
    """A define of the code as a number: 12, -5, 0x8CCD, i2f(10), F1_0,
    (2*F1_0), (F1_0*6/10), (15*F1_0), F1_0/2, fl2f(-102.5). A length of the
    model (DR_S(...)) is None."""
    e = expr.strip()
    if "DR_S(" in e:
        return None
    e = re.sub(r"\bi2f\(([^()]*)\)", r"(\1)", e)
    e = re.sub(r"\bfl2f\(([^()]*)\)", r"(\1)", e)
    e = e.replace("F1_0", "1.0")
    if not re.fullmatch(r"[\d\s.+\-*/()xXa-fA-F]+", e):
        return None
    try:
        v = eval(e, {"__builtins__": {}})
    except Exception:
        return None
    return int(v) if float(v).is_integer() else float(v)


def defines(text, prefix):
    """The #define NAME VALUE lines whose name starts with prefix, as numbers
    (a value that is no number is left out)."""
    out = {}
    for m in re.finditer(r"^#define\s+(%s\w*)\s+(.+?)\s*(?://.*)?$" % re.escape(prefix), text, re.M):
        v = _value(m.group(2))
        if v is not None:
            out[m.group(1)] = v
    return out


def spells():
    """The spell lines of the port: {number: (line text, cast time in seconds)}."""
    c = _read("dtuspell.c")
    i = c.index("const char *dtu_spell_port_line(int k)")
    block = c[i:c.index("};", i)]
    out = {}
    # each line is one or more C strings, the first starts with "$SPELL n"
    for m in re.finditer(r'"(\$SPELL (\d+) (?:[^"\\]|\\.)*)"((?:\s*"(?:[^"\\]|\\.)*")*)', block):
        text = m.group(1) + "".join(re.findall(r'"((?:[^"\\]|\\.)*)"', m.group(3)))
        out[int(m.group(2))] = [text.replace('\\"', '"'), None]
    h = _read("dtuspell.h")
    names = {k: int(v) for k, v in re.findall(r"#define\s+(DTU_SPELL_\w+)\s+(\d+)", h)}
    j = c.index("static int port_cast_time(int spell)")
    for name, sec in re.findall(r"case\s+(DTU_SPELL_\w+):\s*return\s+(\d+);", c[j:c.index("return -1;", j)]):
        out[names[name]][1] = int(sec)
    for n, (line, ct) in out.items():
        if ct is None:
            raise SystemExit("port_tables: no cast time for the spell line %d" % n)
    if not out:
        raise SystemExit("port_tables: no spell lines in dtuspell.c")
    return {n: tuple(v) for n, v in out.items()}


def port_scrolls():
    """The scrolls that Caster AI puts in place of a scroll of 1.3
    (Port_scrolls of dtuobj_setup.c): {new spell: [(level line of DTU.MSN,
    object, spell of 1.3)]}."""
    h = _read("dtuspell.h")
    names = {k: int(v) for k, v in re.findall(r"#define\s+(DTU_SPELL_\w+)\s+(\d+)", h)}
    s = _read("dtuobj_setup.c")
    i = s.index("Port_scrolls[]")
    out = {}
    for lvl, obj, old, new in re.findall(r"\{\s*(\d+),\s*(\d+),\s*(\d+),\s*(DTU_SPELL_\w+|\d+)\s*\}",
                                         s[i:s.index("};", i)]):
        n = names[new] if new in names else int(new)
        out.setdefault(n, []).append((int(lvl), int(obj), int(old)))
    if not out:
        raise SystemExit("dtuobj_setup.c: no rows in Port_scrolls")
    return out


def icons():
    """The icons of the port: {icon number: 1024 color numbers (32x32)}."""
    h = _read("dtuspellicons.h")
    first = int(re.search(r"#define\s+DTU_PORT_ICON_FIRST\s+(\d+)", h).group(1))
    count = int(re.search(r"#define\s+DTU_PORT_ICON_COUNT\s+(\d+)", h).group(1))
    i = h.index("Dtu_port_icons_hi[")
    body = h[h.index("{", i) + 1:h.index("};", i)]
    out = {}
    for k, part in enumerate(re.findall(r"\{([^{}]*)\}", body)):
        pix = [int(x) for x in re.findall(r"\b\d+\b", re.sub(r"//[^\n]*", "", part))]
        if len(pix) != 1024:
            raise SystemExit("dtuspellicons.h: icon %d has %d pixels" % (first + k, len(pix)))
        out[first + k] = bytes(pix)
    if len(out) != count:
        raise SystemExit("dtuspellicons.h: %d icons, DTU_PORT_ICON_COUNT %d" % (len(out), count))
    return out


def casters():
    """The creature casters of the Caster AI (dtucaster.h):
    lists:  {list name (CL_...): [(spell, min level, weight, flags)]}
    types:  [(type name, list name, caster level, flags)]
    limits: [(type name, spell, level)]
    flags:  the CS_ and CF_ defines
    d:      the CASTER_ and CAI_ numbers of dtuspell.c (seconds, units)"""
    h = _read("dtucaster.h")
    order = re.search(r"enum\s*\{([^}]*)\}", h).group(1)
    names = [x.strip() for x in order.split(",") if x.strip() and x.strip() != "CL_COUNT"]
    flags = {k: int(v, 0) for k, v in re.findall(r"#define\s+(C[SF]_\w+)\s+(0x[0-9A-Fa-f]+)", h)}
    i = h.index("Caster_lists[CL_COUNT][CASTER_LIST_MAX]")
    body = re.sub(r"//[^\n]*", "", h[h.index("{", i) + 1:h.index("};", i)])
    lists, depth, cur, start = {}, 0, [], 0
    # the outer entries: one { ... } with inner { spell, level, weight, flags }
    for ch_i, ch in enumerate(body):
        if ch == "{":
            depth += 1
            if depth == 1:
                start = ch_i
        elif ch == "}":
            depth -= 1
            if depth == 0:
                cur.append(body[start + 1:ch_i])
    for name, part in zip(names, cur):
        rows = []
        for e in re.findall(r"\{([^{}]*)\}", part):
            v = [x.strip() for x in e.split(",")]
            if v[0] == "255":
                break
            f = 0
            for w in v[3].split("|") if len(v) > 3 else ():
                w = w.strip()
                f |= flags.get(w, 0) if not w.isdigit() else int(w)
            rows.append((int(v[0]), int(v[1]), int(v[2]), f))
        lists[name] = rows
    if len(lists) != len(names):
        raise SystemExit("dtucaster.h: %d lists, %d names" % (len(lists), len(names)))
    types = []
    j = h.index("Caster_types[]")
    for t, lst, lvl, f in re.findall(r'\{\s*"([^"]+)",\s*(CL_\w+),\s*(\d+),\s*([^}]*)\}', h[j:h.index("};", j)]):
        fl = 0
        for w in f.split("|"):
            w = w.strip()
            fl |= int(w) if w.isdigit() else flags.get(w, 0)
        types.append((t, lst, int(lvl), fl))
    k = h.index("Caster_limits[]")
    limits = [(t, int(s), int(lv)) for t, s, lv in
              re.findall(r'\{\s*"([^"]+)",\s*(\d+),\s*(\d+)\s*\}', h[k:h.index("};", k)])]
    s = _read("dtuspell.c")
    d = defines(s, "CASTER_")
    d.update(defines(s, "CAI_"))
    d.update(defines(_read("dtuspell.h"), "DTU_CASTER_KEEP_"))
    for k in ("CASTER_HOLD_MAX", "CASTER_HOLD_REST", "CAI_ROUND", "CAI_FIGHT_END", "CAI_POOL_LEVEL",
              "CAI_HOLD_WAIT", "CAI_POWER_WAIT", "CAI_INTERRUPT", "CAI_THROW", "CAI_CAST_MAX",
              "DTU_CASTER_KEEP_NEAR", "DTU_CASTER_KEEP_FAR"):
        if k not in d:
            raise SystemExit("port_tables: no %s in dtuspell.c or dtuspell.h" % k)
    return {"lists": lists, "types": types, "limits": limits, "flags": flags, "d": d}


def improved_ai():
    """The numbers of Improved AI (dtuobj_aware.c): DTU_HEAR_DIST,
    DTU_ALARM_DIST, DTU_COMBAT_TIME, DTU_JOIN_DIST, DTU_JOIN_PERIOD."""
    d = defines(_read("dtuobj_aware.c"), "DTU_")
    for k in ("DTU_HEAR_DIST", "DTU_ALARM_DIST", "DTU_COMBAT_TIME", "DTU_JOIN_DIST", "DTU_JOIN_PERIOD"):
        if k not in d:
            raise SystemExit("dtuobj_aware.c: no %s" % k)
    return d


def dragon():
    """The dragon of DROW2 (dtudragon.c):
    d:        the DR_ numbers (lengths of the model are left out)
    treasure: [(class 5, 15 or 16, number, count)] of Dr_treasure (no free places)"""
    c = _read("dtudragon.c")
    d = defines(c, "DR_")
    cls = {"OBJ_WEAPON": 5, "OBJ_ARMOR": 15, "OBJ_MISC": 16}
    i = c.index("Dr_treasure[]")
    body = re.sub(r"//[^\n]*", "", c[c.index("{", i) + 1:c.index("};", i)])
    tr = []
    for e in re.findall(r"\{([^{}]*)\}", body):
        v = [x.strip() for x in e.split(",")]
        if v[0] == "-1":
            continue
        num = d.get(v[1]) if v[1] in d else int(v[1])
        tr.append((cls[v[0]], num, int(v[2])))
    for k in ("DR_HIT_POINTS", "DR_ARMOR_CLASS", "DR_THAC0", "DR_EXPERIENCE", "DR_MAGIC_RESIST",
              "DR_BREATH_DICE", "DR_BREATH_PAUSE", "DR_HATCH_AT_ONCE"):
        if k not in d:
            raise SystemExit("dtudragon.c: no %s" % k)
    return {"d": d, "treasure": tr}


if __name__ == "__main__":
    import pprint
    pprint.pprint({"spells": spells(), "scrolls": port_scrolls(), "icons": sorted(icons()), "casters": casters(),
                   "improved_ai": improved_ai(), "dragon": dragon()}, width=140)
    sys.exit(0)
