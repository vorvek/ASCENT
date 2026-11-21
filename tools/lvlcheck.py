#!/usr/bin/env python3
"""lvlcheck.py: compare the level loader of the engine with tools/lvl.py.

For each level file the script runs

    ascent -datadir DIR -checklevel NAME

and compares the summary with the one that lvl.py makes from the same file
(lvl.py --summary). The lines of the engine that start with "#" are not a
part of the summary. Standard library only.

Usage:
    python -I tools/lvlcheck.py ASCENT_EXE FILE_OR_DIR [FILE_OR_DIR ...]

A directory stands for all its .LVL and .LV2 files. If a directory has
NAME.LVL and NAME.LV2, the engine loads NAME.LV2 for the name (the level of
the 1.3 patch has priority), so only the .LV2 file is compared.

The exit status is 0 if all levels match.
"""

import io
import os
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import lvl  # noqa: E402


def level_files(arg):
    if os.path.isdir(arg):
        names = sorted(os.listdir(arg))
        upper = {n.upper() for n in names}
        out = []
        for n in names:
            base, ext = os.path.splitext(n.upper())
            if ext == ".LV2":
                out.append(os.path.join(arg, n))
            elif ext == ".LVL" and base + ".LV2" not in upper:
                out.append(os.path.join(arg, n))
        return out
    return [arg]


def python_summary(path):
    with open(path, "rb") as f:
        data = f.read()
    lv = lvl.Level(data, os.path.basename(path))
    out = io.StringIO()
    lvl.print_engine_summary(lv, path, out)
    return out.getvalue().splitlines(), lv


def engine_summary(exe, path):
    name = os.path.splitext(os.path.basename(path))[0]
    r = subprocess.run(
        [exe, "-datadir", os.path.dirname(os.path.abspath(path)),
         "-checklevel", name],
        stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=120)
    text = r.stdout.decode("latin-1").replace("\r\n", "\n")
    lines = text.splitlines()
    # "Int3:" is a message of the error library of the engine (a debug
    # break of the original code), not a line of the summary.
    note = lambda ln: ln.startswith("#") or ln.startswith("Int3:")
    return ([ln for ln in lines if not note(ln)],
            [ln for ln in lines if note(ln)], r.returncode)


def main(argv):
    if len(argv) < 3:
        print(__doc__)
        return 2
    exe = os.path.abspath(argv[1])
    files = []
    for a in argv[2:]:
        files.extend(level_files(a))
    bad = 0
    for path in files:
        try:
            want, lv = python_summary(path)
        except (OSError, lvl.FormatError, ValueError) as e:
            print("%-14s lvl.py error: %s" % (os.path.basename(path), e))
            bad += 1
            continue
        got, notes, status = engine_summary(exe, path)
        ok = (status == 0 and got == want)
        d = dict(ln.split(" ", 1) for ln in want if " " in ln)
        validate = ""
        for n in notes:
            if n.startswith("# validate_segment:"):
                w = n.split()
                validate += "  validate: %s types, %s normals changed" \
                    % (w[2], w[7])
            if n.startswith("# fix_object_segs:"):
                w = n.split()
                validate += "  objects: %s to another segment, %s to " \
                    "the segment center" % (w[2], w[8])
        print("%-14s %s  game %s  segs %4s  verts %4s  objs %3s  walls %3s%s"
              % (os.path.basename(path), "MATCH   " if ok else "MISMATCH",
                 d.get("game_version"), d.get("segments"), d.get("vertices"),
                 d.get("objects"), d.get("walls"), validate))
        if not ok:
            bad += 1
            print("  exit status of the engine: %d" % status)
            for i in range(max(len(want), len(got))):
                a = want[i] if i < len(want) else "(no line)"
                b = got[i] if i < len(got) else "(no line)"
                if a != b:
                    print("  lvl.py: %s" % a)
                    print("  engine: %s" % b)
    print("%d of %d level files match" % (len(files) - bad, len(files)))
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
