#!/usr/bin/env python3
"""Check the item corrections of ASCENT (engine/main/dtuitemfix.c,
docs/audit/items.md).

  python -I tools/check_itemfix.py [--game GAME_DIR] [--tables DIR]

Always: the two tables of dtuitemfix.c are read, and each corrected item
name must have a translation in engine/lang/es_port.catalog (an entry
tbl.fix.* with the same English text, a gender and a plural), so that the
Spanish text shows the corrected name. The names that the table fixup of
1.3 gives (the T_NAME rows of engine/main/dtutblfix.c) need the same entry.

--game GAME_DIR (the installed game of version 1.3, with YPINN.BIN): each
row of the tables is compared with the item lines of YPINN.BIN. The line of
a row must have the name that the row gives for 1.3, and the key or the word
of a rule must have the value that the row gives for 1.3. After the name
corrections no two lines of a table that had the same name by mistake still
have it. Without the file the compare is skipped (the tool returns 0).

--tables DIR (unpacked level tables, *.BIN): the same compare for each
table, as a list only (no error): the tables that have the item lines of
the rows, and the tables (older dialects, the player archives) where rows do
not apply.

The tool reads the game files only. Returns 1 on an error.
"""
import argparse
import glob
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, ".."))
sys.path.insert(0, HERE)
sys.dont_write_bytecode = True

SECTION = {"W": "$WEAPON", "A": "$ARMOR", "M": "$OBJECT"}
ROW = r'\{\s*([WAM]),\s*(\d+),\s*"([^"]*)",\s*'
NAME_ROW = re.compile(ROW + r'"([^"]*)",\s*"([^"]*)"\s*\}')
RULE_ROW = re.compile(ROW + r'([01]),\s*"([^"]*)",\s*"([^"]*)",\s*"([^"]*)",\s*"([^"]*)"\s*\}')


def read_tables(path=None):
    """The rows of dtuitemfix.c: (names, rules) as lists of dicts."""
    path = path or os.path.join(ROOT, "engine", "main", "dtuitemfix.c")
    names, rules = [], []
    with open(path, encoding="latin-1") as f:
        for line in f:
            line = line.strip()
            m = RULE_ROW.match(line)
            if m:
                rules.append(dict(kind=m.group(1), id=int(m.group(2)), name13=m.group(3), flag=int(m.group(4)),
                                  key=m.group(5), v13=m.group(6), v=m.group(7), source=m.group(8)))
                continue
            m = NAME_ROW.match(line)
            if m:
                names.append(dict(kind=m.group(1), id=int(m.group(2)), name13=m.group(3), name=m.group(4),
                                  why=m.group(5)))
    return names, rules


def read_fixup_names(path=None):
    """The names that the table fixup of 1.3 gives (the T_NAME rows of
    dtutblfix.c): they need a translation as the corrected names do."""
    path = path or os.path.join(ROOT, "engine", "main", "dtutblfix.c")
    with open(path, encoding="latin-1") as f:
        return [dict(name=m.group(1)) for m in re.finditer(r'\{\s*T_NAME,\s*[MAWS],\s*\d+,\s*-?\d+,\s*"[^"]*",\s*"([^"]*)"\s*\}', f.read())]


def read_catalog(path):
    entries, cur = [], {}
    with open(path, encoding="utf-8") as f:
        for line in f.read().splitlines() + [""]:
            if not line.strip():
                if cur:
                    entries.append(cur)
                cur = {}
            elif not line.startswith("#") and ":" in line:
                k, v = line.split(":", 1)
                cur[k] = v[1:] if v.startswith(" ") else v
    return entries


def check_catalog(names, problems):
    path = os.path.join(ROOT, "engine", "lang", "es_port.catalog")
    by_en = {}
    for e in read_catalog(path):
        if e.get("id", "").startswith("tbl."):
            by_en[e.get("en")] = e
    for n in names:
        e = by_en.get(n["name"])
        if e is None:
            problems.append("es_port.catalog: no entry tbl.fix.* with en: %s" % n["name"])
        elif not e.get("tr") or e.get("gender") not in ("m", "f") or not e.get("plural"):
            problems.append("es_port.catalog: %s needs tr, gender (m or f) and plural" % e["id"])
    return len(names)


def line_names(rec):
    """name= and weapon_name= of a line (the game shows the last one)."""
    out = []
    for key in ("name", "weapon_name"):
        for v in re.findall(r'(?<![\w#])%s\s*=\s*"([^"]*)"' % key, rec.line, re.I):
            out.append(v.rstrip(" "))
    return out


def check_table(path, names, rules, problems, strict=True):
    import wiki_rules
    tab = wiki_rules.Table(path)
    base = os.path.basename(path)
    skipped = 0

    def line_of(row):
        recs = tab[SECTION[row["kind"]]]
        if row["id"] >= len(recs):
            return None
        rec = recs[row["id"]]
        return rec if set(row["name13"].split("|")) & set(line_names(rec)) else None

    for n in names:
        rec = line_of(n)
        if rec is None:
            skipped += 1
            if strict:
                problems.append("%s: %s %d is not '%s'" % (base, SECTION[n["kind"]], n["id"], n["name13"]))
    for r in rules:
        rec = line_of(r)
        if rec is None:
            skipped += 1
            if strict:
                problems.append("%s: %s %d is not '%s'" % (base, SECTION[r["kind"]], r["id"], r["name13"]))
            continue
        if r["flag"]:
            have = "1" if rec.has(r["key"]) else "0"
        else:
            have = rec.get(r["key"])
            have = "" if have is None else have
        if have != r["v13"] and not strict:
            skipped += 1
        elif have != r["v13"]:
            problems.append("%s: %s %d '%s': %s is '%s', the table of dtuitemfix.c says '%s'" % (
                base, SECTION[r["kind"]], r["id"], r["name13"], r["key"], have, r["v13"]))
    if strict:
        # The shown names: a corrected name must not be the name of another line
        for kind, sec in SECTION.items():
            fixed = {n["id"]: n["name"] for n in names if n["kind"] == kind}
            shown = {}
            for i, rec in enumerate(tab[sec]):
                ln = line_names(rec)
                if ln:
                    shown.setdefault(fixed.get(i, ln[-1]), []).append(i)
            for n in names:
                if n["kind"] == kind and len(shown.get(n["name"], [])) > 1 and "Key" not in n["name"]:
                    problems.append("%s: %s: the corrected name '%s' is the name of the lines %s" % (
                        base, sec, n["name"], shown[n["name"]]))
    return skipped


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--game")
    ap.add_argument("--tables")
    args = ap.parse_args(argv)
    problems = []
    names, rules = read_tables()
    if len(names) < 3 or len(rules) < 3:
        problems.append("dtuitemfix.c: the tables were not read (%d names, %d rules)" % (len(names), len(rules)))
    check_catalog(names, problems)
    print("dtuitemfix.c: %d name rows, %d rules; es_port.catalog checked" % (len(names), len(rules)))
    fixup = read_fixup_names()
    if len(fixup) < 3:
        problems.append("dtutblfix.c: the names were not read (%d)" % len(fixup))
    check_catalog(fixup, problems)
    print("dtutblfix.c: %d names of the table fixup; es_port.catalog checked" % len(fixup))
    if args.game:
        path = os.path.join(args.game, "YPINN.BIN")
        if os.path.exists(path):
            check_table(path, names, rules, problems)
            print("%s: compared" % path)
        else:
            print("%s: no file, the compare with the game tables is skipped" % path)
    if args.tables:
        same, other = [], {}
        for path in sorted(glob.glob(os.path.join(args.tables, "*.BIN"))):
            try:
                skipped = check_table(path, names, rules, problems, strict=False)
            except Exception as e:  # not a table
                print("%s: not read (%s)" % (os.path.basename(path), e))
                continue
            if skipped:
                other.setdefault(skipped, []).append(os.path.basename(path))
            else:
                same.append(os.path.basename(path))
        print("%d tables have the item lines of the rows: %s" % (len(same), " ".join(same)))
        for skipped, tables in sorted(other.items()):
            print("%d rows do not apply (another item at the number, or another 1.3 value) in %d tables: %s" % (
                skipped, len(tables), " ".join(tables)))
    for p in problems:
        print("ERROR " + p)
    print("%d errors" % len(problems))
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
