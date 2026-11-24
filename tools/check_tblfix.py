#!/usr/bin/env python3
"""Check the table fixup of 1.3 (engine/main/dtutblfix.c, 0x1A344) with the
real tables, and print what it changes.

  python -I tools/check_tblfix.py TABLE.BIN [--tables DIR] [--exe TEST_EXE] [--all]

TABLE.BIN  a level table of the mission (gamedata/unpacked/YPINN.BIN): for
           each line that the fixup changes, the values before and after
           (the keys, words and names that differ). --all prints the whole
           text of each changed line.
--tables   a directory of level tables (*.BIN): which tables the fixup
           applies to (the guard of dtutblfix.c, for each section alone).
           The 39 mission tables must pass with all sections, five older
           ones with the weapons and the spells only, and two with none.
--exe      build/engine/main/test_dtutblfix.exe: the C code of the port is
           applied to the lines of TABLE.BIN too, and its result must be the
           result of tools/tblfix.py.

The tool reads the game files only and writes temporary files in the
system temporary directory. Returns 1 on an error.
"""
import argparse
import glob
import os
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.dont_write_bytecode = True
import tblfix  # noqa: E402
import wiki_rules  # noqa: E402

# The older tables: the sections that pass the guard
OLDER = {"BASE.BIN": "WS", "MINES1.BIN": "WS", "PRISON1.BIN": "WS", "SPIDER.BIN": "WS", "TRAIN1.BIN": "WS",
         "DEMO.BIN": "", "MOIN1.BIN": ""}


def diff(a, b):
    """The words of a and b that differ: (before, after) lists."""
    wa = [a[s:e] for s, e in tblfix.words(a)]
    wb = [b[s:e] for s, e in tblfix.words(b)]
    la = [w.lower() for w in wa]
    lb = [w.lower() for w in wb]
    return [w for w, l in zip(wa, la) if l not in lb], [w for w, l in zip(wb, lb) if l not in la]


def dump(path, show_all):
    tab = wiki_rules.Table(path, fixup=False)
    lines = tblfix.Lines(tab)
    before = {k: [t.replace("\t", " ") for t in v] for k, v in lines.texts.items()}
    lines.texts = {k: list(v) for k, v in before.items()}
    changed = tblfix.apply_lines(lines)
    if changed is None:
        print("%s: the guard fails for each section, the fixup is not applied" % path)
        return None, None
    print("%s: %d lines changed" % (os.path.basename(path), len(changed)))
    for k, i in sorted(changed, key=lambda x: ("MSWA".index(x[0]), x[1])):
        a, b = before[k][i], lines.texts[k][i]
        old, new = diff(a, b)
        print("%s %d %s -> %s" % (tblfix.SECTION[k], i, (tblfix.name_of(a) or "").rstrip(), (tblfix.name_of(b) or "").rstrip()))
        print("    before: %s" % " ".join(old))
        print("    after:  %s" % " ".join(new))
        if show_all:
            print("    text:   %s" % b.strip())
    return before, lines.texts


def check_exe(exe, before, after, problems):
    tmp = tempfile.mkdtemp(prefix="tblfix")
    fin, fout = os.path.join(tmp, "in.txt"), os.path.join(tmp, "out.txt")
    with open(fin, "w", encoding="latin-1") as f:
        for k in "MAWS":
            for i, t in enumerate(before[k]):
                f.write("%s\t%d\t%s\t%s\n" % (k, i, tblfix.name_of(t) or "-", t))
    r = subprocess.run([os.path.abspath(exe), fin, fout])
    if r.returncode != 0:
        problems.append("%s returned %d" % (exe, r.returncode))
        return
    got = {}
    with open(fout, encoding="latin-1") as f:
        n = int(f.readline())
        for row in f:
            k, i, name, text = row.rstrip("\n").split("\t", 3)
            got[(k, int(i))] = (name, text)
    count = 0
    for k in "MAWS":
        for i, t in enumerate(after[k]):
            name, text = got.get((k, i), (None, None))
            if text != t:
                problems.append("%s %d: the port gives\n  %s\ntblfix.py gives\n  %s" % (k, i, text, t))
            elif (name or "") != (tblfix.name_of(t) or ""):
                problems.append("%s %d: the port has the name '%s', the text '%s'" % (k, i, name, tblfix.name_of(t)))
            count += 1
    print("the port (%s): %d writes; %d lines compared with tblfix.py" % (os.path.basename(exe), n, count))


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("table")
    ap.add_argument("--tables")
    ap.add_argument("--exe")
    ap.add_argument("--all", action="store_true")
    args = ap.parse_args(argv)
    problems = []
    if not os.path.exists(args.table):
        print("%s: no file, the check is skipped" % args.table)
        return 0
    before, after = dump(args.table, args.all)
    if before is None:
        problems.append("%s: the fixup does not apply" % args.table)
    elif args.exe:
        check_exe(args.exe, before, after, problems)
    if args.tables:
        ok, no = [], {}
        guard = tblfix.read_source()[1]
        for path in sorted(glob.glob(os.path.join(args.tables, "*.BIN"))):
            try:
                tab = wiki_rules.Table(path, fixup=False)
            except Exception:
                continue
            if not tab["$SPELL"] and not tab["$WEAPON"]:
                continue        # a player table
            base = os.path.basename(path)
            kinds = tblfix.guard_kinds(tblfix.Lines(tab), guard)
            if kinds == "MAWS":
                ok.append(base)
            else:
                no[base] = kinds
        print("%d tables get the fixup: %s" % (len(ok), " ".join(ok)))
        print("%d tables get a part or nothing (other lines at the numbers): %s" %
              (len(no), " ".join("%s(%s)" % (b, k or "-") for b, k in sorted(no.items()))))
        if no != OLDER:
            problems.append("the tables without the whole fixup are not the seven older tables with their sections")
    for p in problems:
        print("ERROR " + p)
    print("%d errors" % len(problems))
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
