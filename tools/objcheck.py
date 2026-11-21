#!/usr/bin/env python3
"""Check the object records of the level files against the level tables.

Proves from data the claims of docs/re/objects.md:

  - obj->id of a creature, weapon, armor, misc, spell or dressing object is
    the index of the $MONSTER, $WEAPON, $ARMOR, $OBJECT, $SPELL or $DRESSING
    line of the level table (counted from 0, in table order),
  - the model of a creature or dressing object in the file is the model of
    that table line,
  - the render type 8 value of the file is a slot number of the object
    bitmap table that grows with the table line (one slot for each
    picture= of a $OBJECT or $ARMOR line),
  - the size of an item in the file is size= of its table line,
  - which values of the 124 byte block are not zero, for each object type,
    and which items have a value at 0xAC (the loader moves it to 0x1D5).

usage: python -I tools/objcheck.py DIR [-v]

DIR holds the unpacked .LVL and .BIN files (gamedata/unpacked).
This tool reads only.
"""
import collections
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import egg  # noqa: E402
import lvl  # noqa: E402

SECTIONS = {2: "$MONSTER", 5: "$WEAPON", 15: "$ARMOR", 16: "$OBJECT",
            17: "$SPELL", 18: "$DRESSING"}
TYPE_NAMES = {2: "creature", 4: "player", 5: "weapon", 15: "armor",
              16: "misc", 17: "spell", 18: "dressing"}


def table_lines(path):
    """Return {section word: [line text, ...]} in table order."""
    with open(path, "rb") as f:
        raw = f.read()
    if len(raw) < 14:
        return None
    _, text = egg.decode_bin(raw)
    out = collections.defaultdict(list)
    for line in text.decode("latin-1").split("\n"):
        line = line.split(";")[0].strip()
        if line.startswith("@"):
            line = line[1:]
        m = re.match(r"(\$[A-Z_]+)\b", line)
        if m:
            out[m.group(1)].append(line)
    return out


def value(line, key):
    m = re.search(r"(?<![\w#])%s\s*=\s*(\"[^\"]*\"|\S+)" % re.escape(key),
                  line, re.I)
    return m.group(1).strip('"') if m else None


class Count:
    def __init__(self):
        self.ok = 0
        self.n = 0
        self.bad = []

    def add(self, ok, note=None):
        self.n += 1
        if ok:
            self.ok += 1
        elif note and len(self.bad) < 8:
            self.bad.append(note)

    def __str__(self):
        return "%d / %d" % (self.ok, self.n)


def main(argv):
    if len(argv) < 2:
        print(__doc__)
        return 2
    d = argv[1]
    verbose = "-v" in argv
    checks = collections.OrderedDict(
        (k, Count()) for k in (
            "id below the number of table lines",
            "creature: file model name is model= of $MONSTER line id",
            "dressing: file model name is pof= of $DRESSING line id",
            "dressing: render type of the file fits the table line",
            "render 8: one value for each (type, id) in a level",
            "render 8: value grows with id inside a type",
            "misc, armor: value step is the count of picture= lines",
            "item: file size is size= of the table line (default 5)",
            "dressing: file shields is the file size",
        ))
    ext_nonzero = collections.defaultdict(collections.Counter)
    ext_values = collections.defaultdict(lambda: collections.defaultdict(
        collections.Counter))
    n_type = collections.Counter()
    size_by_type = collections.defaultdict(collections.Counter)
    shields_by_type = collections.defaultdict(collections.Counter)
    counted = collections.defaultdict(list)
    files = 0
    for fn in sorted(os.listdir(d)):
        if not fn.upper().endswith(".LVL"):
            continue
        binp = os.path.join(d, fn[:-4] + ".BIN")
        if not os.path.exists(binp):
            continue
        tab = table_lines(binp)
        if tab is None:
            continue
        with open(os.path.join(d, fn), "rb") as f:
            lv = lvl.Level(f.read(), fn)
        lv.parse()
        files += 1
        r8 = collections.defaultdict(set)
        for i, ob in enumerate(lv.objects):
            t, oid = ob["type"], ob["id"]
            n_type[t] += 1
            for off, v in ob["ext"].items():
                if v:
                    ext_nonzero[t][off] += 1
                    ext_values[t][off][v] += 1
            size_by_type[t][ob["size"]] += 1
            shields_by_type[t][ob["shields"]] += 1
            sec = SECTIONS.get(t)
            if sec is None:
                continue
            lines = tab[sec]
            where = "%s object %d type %d id %d" % (fn, i, t, oid)
            checks["id below the number of table lines"].add(
                oid < len(lines), where)
            if oid >= len(lines):
                continue
            line = lines[oid]
            po = ob.get("polyobj")
            if po is not None:
                m = po["model_num"]
                name = (lv.pof_names[m].lower()
                        if 0 <= m < len(lv.pof_names) else "?")
                if t == 2:
                    want = (value(line, "model") or "").lower()
                    checks["creature: file model name is model= of "
                           "$MONSTER line id"].add(
                        name == want, "%s: %s, table %s" % (where, name, want))
                elif t == 18:
                    want = (value(line, "pof") or "").lower()
                    checks["dressing: file model name is pof= of "
                           "$DRESSING line id"].add(
                        name == want, "%s: %s, table %s" % (where, name, want))
            if t == 18:
                kind = ("pof" if value(line, "pof") else
                        "vclip" if value(line, "vclip") else
                        "bitmap" if value(line, "bitmap") else "?")
                rt = ob["render_type"]
                checks["dressing: render type of the file fits the "
                       "table line"].add(
                    (kind, rt) in (("pof", 1), ("vclip", 7), ("bitmap", 8)),
                    "%s: render %d, table %s" % (where, rt, kind))
                checks["dressing: file shields is the file size"].add(
                    abs(ob["shields"] - ob["size"]) <= 1,
                    "%s: size %.3f, shields %.3f"
                    % (where, ob["size"] / 65536.0, ob["shields"] / 65536.0))
            if t in (5, 15, 16, 17):
                want = float(value(line, "size") or 5)
                checks["item: file size is size= of the table line "
                       "(default 5)"].add(
                    abs(ob["size"] / 65536.0 - want) < 0.001,
                    "%s: %.3f, table %s" % (where, ob["size"] / 65536.0, want))
                count = ob["ext"].get(0xAC, 0)
                if count:
                    name = (value(line, "weapon_name") or value(line, "name")
                            or "?")
                    counted[(t, oid, name)].append(count)
            if "render8" in ob:
                r8[(t, oid)].add(ob["render8"])
        for key, vals in sorted(r8.items()):
            checks["render 8: one value for each (type, id) in a level"].add(
                len(vals) == 1, "%s %s: %s" % (fn, key, sorted(vals)))
        for t in (5, 15, 16, 17):
            ids = sorted(k[1] for k, v in r8.items()
                         if k[0] == t and len(v) == 1)
            for a, b in zip(ids, ids[1:]):
                va = min(r8[(t, a)])
                vb = min(r8[(t, b)])
                checks["render 8: value grows with id inside a type"].add(
                    vb > va, "%s type %d ids %d %d: %d %d"
                    % (fn, t, a, b, va, vb))
                if t in (15, 16):
                    lines = tab[SECTIONS[t]]
                    step = sum(1 for ln in lines[a + 1:b + 1]
                               if value(ln, "picture"))
                    checks["misc, armor: value step is the count of "
                           "picture= lines"].add(
                        vb - va == step, "%s type %d ids %d %d: step %d, "
                        "pictures %d" % (fn, t, a, b, vb - va, step))
    print("%d level files with a table" % files)
    print()
    print("| Check | Passed |")
    print("|---|---|")
    for k, c in checks.items():
        print("| %s | %s |" % (k, c))
    if verbose:
        for k, c in checks.items():
            for b in c.bad:
                print("  FAIL %s: %s" % (k, b))
    print()
    print("Values of the 124 byte block that are not zero "
          "(memory offset: records, distinct values, lowest, highest):")
    for t in sorted(n_type):
        print("type %d (%s), %d records" % (t, TYPE_NAMES.get(t, "?"),
                                            n_type[t]))
        for off in sorted(ext_nonzero[t]):
            vals = ext_values[t][off]
            print("  0x%03X: %d records, %d values, 0x%X .. 0x%X"
                  % (off, ext_nonzero[t][off], len(vals),
                     min(vals) & 0xFFFFFFFF, max(vals) & 0xFFFFFFFF))
    print()
    print("Items with a value at 0xAC (type, id, name: values):")
    for (t, oid, name), vals in sorted(counted.items()):
        print("  %d, %d, %s: %s" % (t, oid, name,
                                    " ".join(str(v) for v in sorted(vals))))
    if verbose:
        print()
        for t in sorted(n_type):
            top = ", ".join("%.3f x%d" % (s / 65536.0, n) for s, n in
                            size_by_type[t].most_common(6))
            print("type %d size: %d values: %s" % (t, len(size_by_type[t]),
                                                    top))
            top = ", ".join("%.3f x%d" % (s / 65536.0, n) for s, n in
                            shields_by_type[t].most_common(6))
            print("type %d shields: %d values: %s"
                  % (t, len(shields_by_type[t]), top))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
