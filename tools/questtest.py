#!/usr/bin/env python3
"""Tests of the quests of the campaign (docs/re/quests.md).

Each test starts the game hidden and muted at a level, sets the quest state
with the test commands of -chartest (flags, items, a move to a segment, the
use action, the death of an object), lets the scripts and triggers run and
checks the lines of the script log, the trigger log and the character log.
A test of two parts saves the game after the first part and loads it in a
second run: the flags, the items and the level state must be the same.
The tests need the game files, so they are not registered with ctest.

    python tools/questtest.py --exe build/engine/main/ascent.exe \
        --datadir E:/UNDAMONTAN/DTU --datadir D:/dev/ASCENT/gamedata/cd/DTU \
        [--only NAME] [--keep] [--list]

The commands of a script are "frame:command=value" (engine/main/dtuchar.c,
run_command). --keep leaves the logs of each test in the directory that is
printed. The exit status is 0 if all tests pass.
"""
import argparse
import os
import re
import shutil
import subprocess
import sys
import tempfile

ONES = ",".join(["1"] * 60)        # -dlgauto: the first reply, 60 times


def T(name, level, script, expect, reject=(), frames=400, dlg=ONES, extra=(), invuln=True):
    return dict(name=name, level=level, script=script, expect=list(expect), reject=list(reject),
                frames=frames, dlg=dlg, extra=list(extra) + (["-invulnerability"] if invuln else []))


# The quest order of the campaign. Each test names the script, trigger or
# DTU.WRP line that it checks in its comment.
TESTS = [
    # Mission 1, KOBOLDS: DTU.WRP command 1 at segment 6 sets mission flag 1
    # when no creature with script 23 (the chief) lives.
    T("kobolds_flag1_alive", "KOBOLDS", "30:goseg=6,60:quest",
      [r"quest: mflags \[ \]"], frames=90),
    T("kobolds_flag1", "KOBOLDS", "20:killscript=23,40:goseg=6,60:quest",
      [r"mission check 1: done", r"quest: mflags \[ 1 \]"], frames=90),
    # Mission 2, BONES: command 2 at segment 241, script 32.
    T("bones_flag2", "BONES", "20:killscript=32,40:goseg=241,60:quest",
      [r"quest: mflags \[ 2 \]"], frames=90),
    # BAT: command 10 at segment 319 has no condition.
    T("bat_flag10", "BAT", "40:goseg=319,60:quest", [r"quest: mflags \[ 10 \]"], frames=90),
    # THIEF2: command 3 at segments 328 and 34, script 70 (the guild master).
    T("thief2_flag3", "THIEF2", "20:killscript=70,40:goseg=328,60:quest",
      [r"quest: mflags \[ 3 \]"], frames=90),
    # MULHOR2: command 11 at segment 226.
    T("mulhor2_flag11", "MULHOR2", "40:goseg=226,60:quest", [r"quest: mflags \[ 11 \]"], frames=90),
    # CLAN1: command 12 at segment 2. CLAN3: command 4, script 190.
    T("clan1_flag12", "CLAN1", "40:goseg=2,60:quest", [r"quest: mflags \[ 12 \]"], frames=90),
    T("clan3_flag4", "CLAN3", "20:killscript=190,40:goseg=94,60:quest",
      [r"quest: mflags \[ 4 \]"], frames=90),
    # DROW1: command 13 at segment 248, command 5 (script 201) at 422.
    T("drow1_flag13", "DROW1", "40:goseg=248,60:quest", [r"quest: mflags \[ 13 \]"], frames=90),
    T("drow1_flag5", "DROW1", "20:killscript=201,40:goseg=422,60:quest",
      [r"quest: mflags \[ 5 \]"], frames=90),
    # DROW2: command 6 (script 212) at segment 24.
    T("drow2_flag6", "DROW2", "20:killscript=212,40:goseg=24,60:quest",
      [r"quest: mflags \[ 6 \]"], frames=90),
    # DROW3: command 14 at 285, command 7 (script 222) at 311.
    T("drow3_flag14", "DROW3", "40:goseg=285,60:quest", [r"quest: mflags \[ 14 \]"], frames=90),
    T("drow3_flag7", "DROW3", "20:killscript=222,40:goseg=311,60:quest",
      [r"quest: mflags \[ 7 \]"], frames=90),
    # BEHOLDER: command 8 (script 252) at 160, command 9 (script 254) at 472.
    T("beholder_flag8", "BEHOLDER", "20:killscript=252,40:goseg=160,60:quest",
      [r"quest: mflags \[ 8 \]"], frames=90),
    T("beholder_flag9", "BEHOLDER", "20:killscript=254,40:goseg=472,60:quest",
      [r"quest: mflags \[ 9 \]"], frames=90),
    # IBBALAR: command 19 at segment 0: no scenery named "Spectator Stone".
    T("ibbalar_flag19_stone", "IBBALAR", "40:goseg=0,60:quest", [r"quest: mflags \[ \]"], frames=90),
    # FLAME: command 17 at segments 2 and 519.
    T("flame_flag17", "FLAME", "40:goseg=519,60:quest", [r"quest: mflags \[ 17 \]"], frames=90),

    # HUB1: floor trigger 10 (segment 61) needs MISC 107 and opens the wall
    # of segment 40 side 4 (trap 3).
    T("hub1_gem_key", "HUB1", "20:wall=40.4,30:give=16.107,40:goseg=61,60:wall=40.4",
      [r"wall 40\.4: type 5", r"wall 40\.4: type 4"], frames=90, extra=["-triglog"]),

    # FLAME: the seven triggers of the statue take the pieces, script 323
    # (trigger 27, object 4) opens the wall of segment 278.
    T("flame_statue", "FLAME",
      "10:give=16.18,10:give=16.19,10:give=16.20,10:give=16.22,10:give=16.23,10:give=16.24,10:give=16.26,"
      "20:goside=229.4,22:activate,40:goside=245.4,42:activate,60:goside=275.4,62:activate,"
      "80:goside=287.4,82:activate,100:goside=266.4,102:activate,120:goside=258.4,122:activate,"
      "140:goside=284.4,142:activate,160:wall=278.0,170:gflag=128,180:goside=279.5,182:activate,560:wall=278.0,565:quest",
      [r"run script 323 of obj 4", r"wall 278\.0: type 5", r"wall 278\.0: type 4"],
      frames=600, extra=["-triglog"]),
    # FLAME: the use action on the Flamesword (object 3, script 322 of the
    # level fixes) after the death of Lolth (object 36) ends the game.
    T("flame_sword_lolth_alive", "FLAME", "20:goseg=486,40:aim=3,45:activate,120:obj=3",
      [r"run script 322 of obj 3", r"obj 3: type 18 .* flags 0x0 "], [r"CREDITV1"], frames=140),
    T("flame_ending", "FLAME", "10:kill=36,20:goseg=486,40:aim=3,45:activate",
      [r"run script 322 of obj 3", r"YOU  HAVE  DESTROYED", r"end of the game: CREDITV1"], frames=260),

    # IBBALAR: the use action on the altar of Cyric (object 2, scenery,
    # script 294).
    T("ibbalar_altar", "IBBALAR", "20:goseg=424,22:aim=2,25:activate", [r"run script 294 of obj 2"], frames=120),

    # YPINN: the chairs with old script ids do not talk (Dtu_fix_furniture_talk).
    T("ypinn_chair", "YPINN", "20:goobj=5,25:activate", [], [r"run script 7 of obj 5"], frames=80),
    T("ypinn_chair_orig", "YPINN", "20:goobj=5,25:activate", [r"run script 7 of obj 5"], frames=80,
      extra=["-origtalk"]),

    # THIEF1: the prison sequence of script 60 (statement DB with segment
    # 0xFFFF): the items that are not worn stay in THIEF1, the player is in
    # THIEF2 segment 612 with no gold and 1 hit point.
    T("thief1_prison", "THIEF1",
      "10:give=5.1,10:give=16.2,10:gold=500,20:prison,60:quest",
      [r"the prison sequence 0xAA3F3", r"prison: \d+ items taken", r"prison: guard critter 0xA5 \(robot \d+\) is obj \d+",
       r"quest: level 7 seg 612 mission \d+ gold 0 hp 1 "], frames=90, invuln=False),

    # A warp of a script (statement DB) saves the state of the level that
    # the player leaves: script 511 is not used, the test command warp is
    # the same call.
    T("script_warp_state", "HUB1", "20:give=16.107,30:goseg=61,50:wall=40.4,60:warp=3.376,100:quest,110:warp=1.61,150:wall=40.4,155:quest",
      [r"wall 40\.4: type 4[\s\S]*quest: level 3 [\s\S]*wall 40\.4: type 4[\s\S]*quest: level 1 "], frames=170,
      extra=["-triglog", "-statelog"]),
    # The spell effects of the player (the bits +0x88 of the player object)
    # stay after a warp (0xBB563).
    T("warp_keeps_effects", "HUB1", "10:damage=5,12:fx=3.600,14:cast=20,20:quest,30:warp=3.376,60:quest",
      [r"quest: level 1 .* hp 7 fx 0x1008 [\s\S]*quest: level 3 .* hp 7 fx 0x1008 "], frames=70, invuln=False),

    # The talk of WEAPON 13 before a warp (0x59D2D): script 0x1FD runs.
    T("drowslayer_warp", "DROW1", "10:give=5.13,15:current=5.13,30:warp=20.1,31:quest,60:warp=21.1,100:quest",
      [r"warp with WEAPON 13 in the hand: script 0x1FD"], frames=120),

    # Statement 9F (script 31 of BONES): the level.
    T("level_statement", "BONES", "20:setlevel=5,30:dump", [r"statement 9F sets level 5", r"level 5/"], frames=40),
]


def run_game(args, level, script, frames, dlg, extra, user, logdir, tag):
    charlog = os.path.join(logdir, tag + "_char.log")
    cmd = [args.exe]
    for dd in args.datadir:
        cmd += ["-datadir", dd]
    cmd += ["-nomovies", "-mute", "-hidden", "-nolog", "-userdir", user, "-level", level,
            "-frames", str(frames), "-fixedfps", "30", "-scriptlog",
            "-charlog", charlog, "-chartest", script, "-dlgauto", dlg, "-dlgdelay", "2"] + list(extra)
    try:
        r = subprocess.run(cmd, capture_output=True, timeout=args.timeout, cwd=logdir)
        out = r.stdout.decode("latin-1", "replace") + r.stderr.decode("latin-1", "replace")
        code = r.returncode
    except subprocess.TimeoutExpired as e:
        out = (e.stdout or b"").decode("latin-1", "replace") + "\nTIMEOUT\n"
        code = -999
    text = out
    if os.path.exists(charlog):
        with open(charlog, "r", encoding="latin-1") as f:
            text += "\n" + f.read()
    with open(os.path.join(logdir, tag + "_out.log"), "w", encoding="latin-1") as f:
        f.write(" ".join(cmd) + "\n" + text)
    return code, text


def main():
    p = argparse.ArgumentParser()
    p.add_argument("--exe", required=True)
    p.add_argument("--datadir", action="append", default=[])
    p.add_argument("--only", action="append", default=[])
    p.add_argument("--keep", action="store_true")
    p.add_argument("--list", action="store_true")
    p.add_argument("--timeout", type=int, default=240)
    args = p.parse_args()
    args.exe = os.path.abspath(args.exe)
    if args.list:
        for t in TESTS:
            print(t["name"], t["level"])
        return 0
    logdir = tempfile.mkdtemp(prefix="questtest_")
    failed = []
    for t in TESTS:
        if args.only and not any(o in t["name"] for o in args.only):
            continue
        user = os.path.join(logdir, t["name"] + "_user")
        os.makedirs(user, exist_ok=True)
        code, text = run_game(args, t["level"], t["script"], t["frames"], t["dlg"], t["extra"], user, logdir, t["name"])
        problems = []
        if code not in (0,):
            problems.append("exit code %d" % code)
        for e in t["expect"]:
            if not re.search(e, text):
                problems.append("missing: " + e)
        for e in t["reject"]:
            if re.search(e, text):
                problems.append("not expected: " + e)
        if re.search(r"SOAK FAIL|Int3|Assertion|Error:", text):
            problems.append("error line in the output")
        print("%-28s %-9s %s" % (t["name"], t["level"], "ok" if not problems else "FAIL: " + "; ".join(problems)))
        if problems:
            failed.append(t["name"])
    print("%d failed%s" % (len(failed), (": " + " ".join(failed)) if failed else ""))
    if args.keep or failed:
        print("logs: " + logdir)
    else:
        shutil.rmtree(logdir, ignore_errors=True)
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
