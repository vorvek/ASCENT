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
    print("%s: %d items, %d market lines, %d spells, %s failed" % (name, items, market, spells, total.group(2)))
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
    if args.dump and first is not None:
        with open(args.dump, "w", encoding="utf-8") as f:
            f.write("".join(line + "\n" for line in first.splitlines() if line.startswith("desc:")))
    for pr in problems:
        print("FAIL", pr)
    print("%d problems" % len(problems))
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
