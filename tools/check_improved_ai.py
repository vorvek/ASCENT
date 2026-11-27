#!/usr/bin/env python3
"""Check the rules of the Improved AI setting in the game.

    python -I tools/check_improved_ai.py --exe build/engine/main/ascent.exe \\
        --datadir E:/UNDAMONTAN/DTU --datadir D:/dev/ASCENT/gamedata/cd/DTU [--keep]

Each case is one hidden run at 30 frames a second with -objlog. The rules
are in docs/re/ai.md, "Improved AI: senses, alarms, doors, attacks and
groups" (engine/main/dtuobj_aware.c, dtuobj_ai.c). Each case runs with the
setting on and, where the rule of 1.3 shows in the log, with -noimprovedai.

- ambush: DROW2, the hall of the four orcs in the closets behind the closed
  walls 0, 2, 4 and 6. The player walks along the hall. On: the orcs obj 8
  and 11 (their walls stay closed) never see the player. Off: they see the
  player in the first second, through the walls.
- door: DROW1, the orc obj 6 in segment 192 behind the closed door of wall
  20, the player in segment 187. On: the orc does not notice the player.
  Off: it does.
- blow: DROW1, the skeleton obj 76 behind that door, the player swings a
  long sword 4 times. On: no hit. Off: the player hits it.
- attack: DROW1, the player 6.0 in front of the drow fighter obj 1, healed.
  On: its first attack starts less than 1.0 s after it sees the player and
  it attacks 4 times or more in 12 s. Off: the first attack is later.
- join: DROW1, the player in front of the wizard obj 56. On: the priestess
  obj 55 joins the fight. Off: she does not.
- guard: THIEF0, the trigger of segment 207 and the goblin obj 31 next to
  the guard obj 22. On: the guard joins the fight. Off: it does not.

The exit status is 0 if all cases pass.
"""
import argparse
import os
import re
import shutil
import subprocess
import sys
import tempfile

FPS = 30


def heal(frames, cmds):
    """The commands plus heal=200 each 6 frames."""
    have = {int(c.split(":")[0]) for c in cmds}
    out = list(cmds) + ["%d:heal=200" % f for f in range(6, frames, 6) if f not in have]
    out.sort(key=lambda c: int(c.split(":")[0]))
    return ",".join(out)


def run(a, d, level, frames, extra):
    os.makedirs(os.path.join(d, "user"), exist_ok=True)
    log = os.path.join(d, "obj.log")
    cmd = [os.path.abspath(a.exe)]
    for dd in a.datadir:
        cmd += ["-datadir", dd]
    cmd += ["-nomovies", "-mute", "-hidden", "-nolog", "-userdir", os.path.join(d, "user"), "-level", level,
            "-frames", str(frames), "-fixedfps", str(FPS), "-objlog", log] + extra
    subprocess.run(cmd, capture_output=True, timeout=a.timeout, cwd=d)
    try:
        with open(log, encoding="latin-1") as f:
            return f.read()
    except OSError:
        return ""


def first(text, pattern):
    m = re.search(r"^\s*([\d.]+)\s+\d+ " + pattern, text, re.M)
    return float(m.group(1)) if m else None


SWINGS = "wait:60,attack:1,wait:45,attack:1,wait:45,attack:1,wait:45,attack:1"

# (name, level, frames, arguments, test of the log with the setting on, test with it off or None)
CASES = [
    ("ambush", "DROW2", 450, ["-testpos", "0,1,150,0", "-testmove", "forward:400", "-noattack"],
     lambda t: first(t, r"obj (8|11) gorcasci: sees the player") is None and first(t, r"obj (9|10) gorcasci: sees the player") is not None,
     lambda t: (first(t, r"obj (8|11) gorcasci: sees the player") or 99) < 1.0),
    ("door", "DROW1", 300, ["-chartest", "5:objseg=6.192,7:goseg=187", "-noattack"],
     lambda t: "level start" in t and first(t, r"obj 6 gorcaaxe: sees the player") is None,
     lambda t: first(t, r"obj 6 gorcaaxe: sees the player") is not None),
    ("blow", "DROW1", 300, ["-chartest", heal(300, ["3:class=2", "4:give=5.6", "5:current=5.6", "7:goseg=187", "10:objseg=76.187",
                                                  "40:objseg=76.192", "45:face=76", "100:face=76", "150:face=76", "200:face=76"]),
                            "-testmove", SWINGS],
     lambda t: "miss sound" in t and "player hits obj 76" not in t,
     lambda t: "player hits obj 76" in t or "player misses obj 76" in t),
    ("attack", "DROW1", 380, ["-chartest", heal(380, ["10:goobj=1.6"])],
     lambda t: (first(t, r"attack: obj 1 rmfg5 starts") or 99) - (first(t, r"obj 1 rmfg5: sees the player") or 0) < 1.0 and
     len(re.findall(r"attack: obj 1 rmfg5 starts", t)) >= 4,
     lambda t: (first(t, r"attack: obj 1 rmfg5 starts") or 99) - (first(t, r"obj 1 rmfg5: sees the player") or 0) > 1.0),
    ("join", "DROW1", 150, ["-chartest", heal(150, ["10:goobj=56.10"])],
     lambda t: "join: obj 55 rffk5 joins the fight" in t,
     lambda t: "level start" in t and "join:" not in t),
    ("guard", "THIEF0", 300, ["-chartest", heal(300, ["10:goobj=22.10", "40:trigger=207.3", "60:objseg=31.210"])],
     lambda t: "join: obj 22 gbloser joins the fight of obj 31" in t and "attack: obj 22 gbloser starts" in t,
     lambda t: "obj 31 gbloser: sees the player" in t and "attack: obj 22 gbloser starts" not in t),
]


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--exe", required=True)
    p.add_argument("--datadir", action="append", required=True)
    p.add_argument("--timeout", type=int, default=600)
    p.add_argument("--keep", action="store_true")
    a = p.parse_args()
    tmp = tempfile.mkdtemp(prefix="improved_ai_")
    bad = 0
    for name, level, frames, extra, on, off in CASES:
        for label, test, more in (("on", on, []), ("off", off, ["-noimprovedai"])):
            if test is None:
                continue
            text = run(a, os.path.join(tmp, name + "_" + label), level, frames, extra + more)
            ok = bool(text) and bool(test(text))
            print("%-4s %s, setting %s" % ("ok" if ok else "FAIL", name, label), flush=True)
            bad += not ok
    print("%d cases, %d failed" % (2 * len(CASES), bad))
    if bad or a.keep:
        print("logs in %s" % tmp)
    else:
        shutil.rmtree(tmp, ignore_errors=True)
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
