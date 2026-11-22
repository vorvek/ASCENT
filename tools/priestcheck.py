#!/usr/bin/env python3
"""Checks of the priest spells and of turn undead (docs/audit/spells-priest.md).

Starts the game hidden and muted at a level, runs the -chartest command
priestcheck (dtu_spell_priest_check in dtuspell.c) and reads the PASS and
FAIL lines of the -charlog file. The checks need the game files, so they
are not registered with ctest.

    python tools/priestcheck.py --exe build/engine/main/ascent.exe \
        --datadir E:/UNDAMONTAN/DTU --datadir D:/dev/ASCENT/gamedata/cd/DTU

Exit code 0 when every check passes. BANE has undead, humanoids and the
creatures that the creature checks need.
"""
import argparse
import os
import subprocess
import sys
import tempfile


def run(args, level, orig):
    d = tempfile.mkdtemp(prefix="priestcheck_")
    os.makedirs(os.path.join(d, "user"))
    script = os.path.join(d, "script.txt")
    log = os.path.join(d, "char.log")
    with open(script, "w") as f:
        f.write("wait 60\nquit\n")
    cmd = [os.path.abspath(args.exe)]
    for dd in args.datadir:
        cmd += ["-datadir", dd]
    cmd += ["-nomovies", "-mute", "-hidden", "-userdir", os.path.join(d, "user"), "-level", level,
            "-fixedfps", "30", "-fescript", script, "-chartest", "30:priestcheck", "-charlog", log]
    if orig:
        cmd.append("-origcombat")
    subprocess.run(cmd, capture_output=True, text=True, timeout=300, cwd=d)
    if not os.path.exists(log):
        return None
    with open(log) as f:
        return [line.split("priestcheck: ", 1)[1].strip() for line in f if "priestcheck: " in line]


def main():
    p = argparse.ArgumentParser()
    p.add_argument("--exe", required=True)
    p.add_argument("--datadir", action="append", default=[])
    p.add_argument("--level", default="BANE")
    args = p.parse_args()
    failed = 0
    for orig in (False, True):
        lines = run(args, args.level, orig)
        name = "-origcombat" if orig else "default"
        if not lines:
            print("FAIL %s: no priestcheck lines (did the level start?)" % name)
            failed += 1
            continue
        passes = [l for l in lines if l.startswith("PASS")]
        fails = [l for l in lines if l.startswith("FAIL")]
        for l in fails:
            print("%s: %s" % (name, l))
        print("%s: %d passed, %d failed" % (name, len(passes), len(fails)))
        failed += len(fails) + (0 if passes else 1)
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
