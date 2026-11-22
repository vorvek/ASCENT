#!/usr/bin/env python3
"""Check a save and a load of the port with a save that the port makes.

  python -I tools/check_saveload.py --exe build/engine/main/ascent.exe --game E:/UNDAMONTAN/DTU [--datadir DIR]

The test starts YPINN, warps to BONES (a level change with new tables),
saves to slot 3, loads slot 3, then loads the empty slot 4. It checks:

- the program exits with code 0 (a message box of a failed load or of a
  mem library warning waits for a key with -hidden),
- -memreport reports no MEM_ error and no block in use at the exit,
- the character dump after the load is the dump before the save,
- the loaded game plays at difficulty 2,
- the load of the empty slot fails with a log line.

The test writes only in a new temporary directory. Returns 1 on an error.
"""
import argparse
import os
import re
import shutil
import subprocess
import sys
import tempfile

SCRIPT = "20:warp=3.376,80:dump,90:save=3,120:load=3,240:dump,250:load=4,300:dump"


def dump(lines, frame):
    """The lines of the dump at frame, without the time and the frame."""
    out = []
    for ln in lines:
        m = re.match(r"\s*[0-9.]+ f(\d+)\s+(.*)$", ln)
        if m and int(m.group(1)) == frame and not m.group(2).startswith("script:"):
            out.append(m.group(2).rstrip())
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--exe", required=True)
    ap.add_argument("--game", required=True, help="the original game directory")
    ap.add_argument("--datadir", action="append", default=[], help="another data directory")
    ap.add_argument("--keep", action="store_true", help="keep the temporary directory")
    a = ap.parse_args()

    if not os.path.isdir(a.game):
        print("check_saveload: no game directory %s" % a.game)
        return 1
    tmp = tempfile.mkdtemp(prefix="ascent_saveload_")
    errors = []
    try:
        log = os.path.join(tmp, "char.log")
        cmd = [os.path.abspath(a.exe), "-datadir", a.game]
        for d in a.datadir:
            if os.path.isdir(d):
                cmd += ["-datadir", d]
        cmd += ["-nomovies", "-mute", "-hidden", "-nolog", "-userdir", tmp, "-level", "YPINN",
                "-frames", "400", "-fixedfps", "30", "-charlog", log, "-chartest", SCRIPT, "-memreport"]
        try:
            p = subprocess.run(cmd, cwd=tmp, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=180)
            out = p.stdout.decode("latin-1")
            if p.returncode != 0:
                errors.append("exit code %d" % p.returncode)
        except subprocess.TimeoutExpired as e:
            out = (e.stdout or b"").decode("latin-1")
            errors.append("no exit in 180 s")
        for ln in out.splitlines():
            if "MEM_" in ln or "Int3" in ln:
                errors.append("output: " + ln.strip())
        m = re.search(r"(\d+) Kbytes dynamic data in use", out)
        if not m:
            errors.append("no memory report")
        elif m.group(1) != "0":
            errors.append("%s Kbytes in use at the exit" % m.group(1))
        lines = open(log, encoding="latin-1").read().splitlines() if os.path.exists(log) else []
        before, after = dump(lines, 80), dump(lines, 240)
        if not before:
            errors.append("no dump before the save")
        if not any(ln.startswith("dungeon level 4,") for ln in before):
            errors.append("the warp to BONES did not happen")
        strip = lambda d: [re.sub(r", difficulty \d+", "", ln) for ln in d]
        if strip(before) != strip(after):
            errors.append("the dump after the load differs:\n  before: %s\n  after:  %s" %
                          ("\n          ".join(before), "\n          ".join(after)))
        if not any(re.match(r"dungeon level 4, difficulty 2,", ln) for ln in after):
            errors.append("the loaded game does not play at difficulty 2")
        if not any("cannot load slot 4" in ln for ln in lines):
            errors.append("no log line for the load of the empty slot 4")
    finally:
        if a.keep:
            print("check_saveload: files in %s" % tmp)
        else:
            shutil.rmtree(tmp, ignore_errors=True)
    for e in errors:
        print("check_saveload: FAIL " + e)
    if not errors:
        print("check_saveload: ok")
    return 1 if errors else 0


if __name__ == "__main__":
    sys.exit(main())
