#!/usr/bin/env python3
"""Run every check that needs the game files, one line for each check.

    python -I tools/run_game_checks.py [--exe EXE] [--game DIR] [--gamedata DIR]
        [--library DLL] [--jobs N] [--only NAME,NAME] [--list] [--keep]

The checks are the tools of tools/README.md that read the game files:
check_items, check_desc, questtest, priestcheck, check_saveload,
check_itemfix, check_tblfix, check_creature_setup (the setup by name, with
quest flag 67, and --abilities), check_reach (BONES, MINE, and CLAN3 with the Broad Sword), check_attack_rate,
check_sword_view, check_improved_ai, check_caster_ai and uitest. Run them
after each merge.

Defaults (the layout of the maintainer machine):
  --exe       build/engine/main/ascent.exe of this checkout. check_tblfix
              uses test_dtutblfix.exe of the same directory (build it with
              `ninja engine/main/test_dtutblfix.exe`).
  --game      E:/UNDAMONTAN/DTU, the installed game of version 1.3.
  --gamedata  gamedata/ of this checkout, or of the main checkout when this
              is a git worktree. The checks use gamedata/cd/DTU (the disc),
              gamedata/hd and gamedata/unpacked.
  --library   D:/dev/DTU-Decomp/local/reference-build/libdtu13_test_dll.dll,
              the reference library of the decompilation for
              check_creature_setup. Without the file that check is skipped.

Each check writes its output to a log file in a new temporary directory
(printed at the end). --jobs runs checks in parallel (each game run has its
own user directory). The exit status is 1 when a check fails.
"""
import argparse
import os
import shutil
import subprocess
import sys
import tempfile
import time
from concurrent.futures import ThreadPoolExecutor

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, ".."))


def main_checkout_gamedata():
    """gamedata/ of this checkout, else of the main checkout of a worktree."""
    own = os.path.join(ROOT, "gamedata")
    if os.path.isdir(own):
        return own
    try:
        common = subprocess.run(["git", "rev-parse", "--path-format=absolute", "--git-common-dir"], cwd=ROOT,
                                capture_output=True, text=True, timeout=30).stdout.strip()
    except (OSError, subprocess.SubprocessError):
        return own
    if common:
        other = os.path.join(os.path.dirname(common), "gamedata")
        if os.path.isdir(other):
            return other
    return own


def checks(a, work):
    """(name, command, what is missing or None) for each check."""
    py = [sys.executable, "-I"]
    exe = os.path.abspath(a.exe)
    cd = os.path.join(a.gamedata, "cd", "DTU")
    hd = os.path.join(a.gamedata, "hd")
    unpacked = os.path.join(a.gamedata, "unpacked")
    dd = ["--datadir", a.game, "--datadir", cd]
    tblexe = os.path.join(os.path.dirname(exe), "test_dtutblfix.exe")

    def tool(name):
        return os.path.join(HERE, name)

    def need(*paths):
        for p in paths:
            if not os.path.exists(p):
                return p
        return None

    game = need(exe, a.game, cd)
    out = [
        ("check_items", py + [tool("check_items.py"), "--exe", exe] + dd, game),
        ("check_desc", py + [tool("check_desc.py"), "--exe", exe] + dd, game),
        ("questtest", py + [tool("questtest.py"), "--exe", exe] + dd, game),
        ("priestcheck", py + [tool("priestcheck.py"), "--exe", exe] + dd, game),
        ("check_saveload", py + [tool("check_saveload.py"), "--exe", exe, "--game", a.game, "--datadir", cd], game),
        ("check_itemfix", py + [tool("check_itemfix.py"), "--game", a.game, "--tables", unpacked],
         need(a.game, unpacked)),
        ("check_tblfix", py + [tool("check_tblfix.py"), os.path.join(unpacked, "YPINN.BIN"), "--tables", unpacked,
                               "--exe", tblexe], need(unpacked, tblexe)),
        ("creature_setup", py + [tool("check_creature_setup.py"), "--exe", exe] + dd +
         ["--library", a.library, "--work", os.path.join(work, "creature_setup")], game or need(a.library)),
        ("creature_flag67", py + [tool("check_creature_setup.py"), "--exe", exe] + dd +
         ["--library", a.library, "--work", os.path.join(work, "creature_flag67"), "--flag67"],
         game or need(a.library)),
        ("creature_abilities", py + [tool("check_creature_setup.py"), "--exe", exe] + dd +
         ["--work", os.path.join(work, "creature_abilities"), "--abilities"], game),
        ("check_reach", py + [tool("check_reach.py"), "--exe", exe, "--level", "BONES", "--jobs", "4"] + dd, game),
        ("check_attack_rate", py + [tool("check_attack_rate.py"), "--exe", exe] + dd, game),
        # the place of the arms of a long sword (Dtu_fix_sword_view)
        ("check_sword_view", py + [tool("check_sword_view.py"), "--exe", exe] + dd, game),
        # rats, beetles and giant bats: the swing and a steep missile
        ("check_reach_mine", py + [tool("check_reach.py"), "--exe", exe, "--level", "MINE", "--jobs", "4"] + dd, game),
        # giant rats with the Broad Sword: the swing at the look pitch and the
        # negative pass (Dtu_fix_swing_pitch)
        # a step back during the attack of a creature (Dtu_fix_attack_commit)
        ("check_reach_commit", py + [tool("check_reach.py"), "--exe", exe, "--commit"] + dd, game),
        ("check_reach_clan3", py + [tool("check_reach.py"), "--exe", exe, "--level", "CLAN3", "--weapon", "20", "--jobs", "4"] + dd,
         game),
        # the senses, doors, attacks and groups of the Improved AI setting
        ("check_improved_ai", py + [tool("check_improved_ai.py"), "--exe", exe] + dd, game),
        # the casters of the Caster AI setting: each type before and after,
        # a closed door, the allies, Mirror Image and the Minor Globe
        ("check_caster_ai", py + [tool("check_caster_ai.py"), "--exe", exe, "--jobs", "4"] + dd, game),
        ("uitest", py + [tool("uitest.py"), "--exe", exe] + dd + ["--hddir", hd], game or need(hd)),
    ]
    return out


def summary(text):
    """The last line of the output that is not empty."""
    lines = [ln.strip() for ln in text.splitlines() if ln.strip()]
    return lines[-1] if lines else ""


def run(item, work, timeout):
    name, cmd, missing = item
    if missing:
        return name, "skip", 0.0, "missing " + missing
    d = os.path.join(work, name)
    os.makedirs(d, exist_ok=True)
    log = os.path.join(work, name + ".log")
    t0 = time.time()
    try:
        r = subprocess.run(cmd, cwd=d, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True,
                           errors="replace", timeout=timeout)
        text, code = r.stdout, r.returncode
    except subprocess.TimeoutExpired as e:
        text, code = (e.stdout or "") if isinstance(e.stdout, str) else "", None
        text += "\ntimeout after %d s" % timeout
    dt = time.time() - t0
    with open(log, "w", encoding="utf-8") as f:
        f.write(" ".join(cmd) + "\n\n" + text)
    fails = [ln.strip() for ln in text.splitlines() if "FAIL" in ln]
    if code == 0:
        return name, "ok", dt, summary(text)
    return name, "FAIL", dt, (fails[0] if fails else summary(text))


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--exe", default=os.path.join(ROOT, "build", "engine", "main", "ascent.exe"))
    p.add_argument("--game", default="E:/UNDAMONTAN/DTU")
    p.add_argument("--gamedata", default=None)
    p.add_argument("--library", default="D:/dev/DTU-Decomp/local/reference-build/libdtu13_test_dll.dll")
    p.add_argument("--jobs", type=int, default=1)
    p.add_argument("--only", help="names separated by commas")
    p.add_argument("--timeout", type=int, default=3600, help="seconds for each check")
    p.add_argument("--list", action="store_true")
    p.add_argument("--keep", action="store_true", help="keep the logs of the checks that pass")
    a = p.parse_args()
    a.gamedata = os.path.normpath(a.gamedata or main_checkout_gamedata())

    work = tempfile.mkdtemp(prefix="gamechecks_")
    items = checks(a, work)
    if a.list:
        for name, cmd, _ in items:
            print("%-19s %s" % (name, " ".join(cmd[2:])))
        shutil.rmtree(work, ignore_errors=True)
        return 0
    if a.only:
        want = a.only.split(",")
        items = [i for i in items if i[0] in want]

    if os.path.exists(a.exe):
        print("exe %s (%s)" % (a.exe, time.strftime("%Y-%m-%d %H:%M", time.localtime(os.path.getmtime(a.exe)))))
    t0 = time.time()
    results = []
    with ThreadPoolExecutor(max_workers=max(1, a.jobs)) as pool:
        for res in pool.map(lambda i: run(i, work, a.timeout), items):
            name, status, dt, text = res
            print("%-19s %-4s %5.0f s  %s" % (name, status, dt, text), flush=True)
            results.append(res)
    n_ok = sum(1 for r in results if r[1] == "ok")
    n_fail = sum(1 for r in results if r[1] == "FAIL")
    n_skip = sum(1 for r in results if r[1] == "skip")
    print("total: %d ok, %d failed, %d skipped, %.0f s" % (n_ok, n_fail, n_skip, time.time() - t0))
    if n_fail or a.keep:
        print("logs in %s" % work)
    else:
        shutil.rmtree(work, ignore_errors=True)
    return 1 if n_fail else 0


if __name__ == "__main__":
    sys.exit(main())
