#!/usr/bin/env python3
"""Check a save and a load of the port with a save that the port makes.

  python -I tools/check_saveload.py --exe build/engine/main/ascent.exe --game E:/UNDAMONTAN/DTU [--datadir DIR]

The first run starts YPINN, makes the character a mage with Detect Magic,
warps to HUB1, rests, casts the spell, warps back to YPINN and saves to
slot 0, warps to BONES (a level change with new tables), saves to slot 3,
loads slot 3, then loads the empty slot 4. It checks:

- the program exits with code 0 (a message box of a failed load or of a
  mem library warning waits for a key with -hidden),
- -memreport reports no MEM_ error and no block in use at the exit,
- the character dump after the load is the dump before the save,
- the hot key boxes after the load are the boxes before the save, with
  the spell that armspell put into a box,
- the loaded game plays at difficulty 2,
- the load of the empty slot fails with a log line,
- the spell slots (docs/re/spells.md): a level change does not fill them,
  the rest does, the cast uses the slot, the slot stays used after two
  level changes and after the save and the load, and the save has 0 at
  the mage slots left of level 1 (P+0x2C).

The second run starts at the main menu, loads slot 0 (YPINN), goes out of
the inn to Waterdeep and through the Yawning Portal into the game. It
checks that the load keeps the used slot and that the game start after
Waterdeep fills it (game() 0x42284).

The third run starts KOBOLDS. It checks that a save rounds the time left
of a potion to the nearest second (Dtu_fix_save_seconds, D-138) and that
a creature keeps its spell points after a save and a load
(Dtu_fix_restore_spells, D-134).

The test writes only in a new temporary directory. Returns 1 on an error.
"""
import argparse
import os
import re
import shutil
import subprocess
import sys
import tempfile

SCRIPT = ("5:class=1,6:give=17.12.1,7:mission=1,10:warp=1.136,30:slots,32:gorest,34:rest,36:slots,"
          "38:armspell=12,40:fire,80:fire,90:slots,92:current=-1,100:warp=0.225,110:slots,115:save=0,"
          "130:warp=3.376,160:slots,180:dump,182:hotkeys,190:save=3,220:load=3,340:dump,342:hotkeys,345:slots,350:load=4,400:dump")
# current=-1 empties the hand: the dump does not show a spell in the hand.
# The second run: the front end loads slot 0, chartest frame 30 puts the
# player on the exit of the inn (DTU.WRP 0,1), Enter answers the question,
# Y is the Yawning Portal of Waterdeep.
SCRIPT2 = "20:slots,30:goseg=1,100:slots,300:slots"
FESCRIPT2 = "wait 60\nkey enter\nwait 10\nkey enter\nwait 10\ngamekeys\nwait 120\nkey enter\nwait 300\nkey y\nwait 400\nquit\n"
SCRIPT3 = "5:give=16.72,10:use=16.72,12:cpoints=19.1,22:save=3,60:cpoints=19.2,80:load=3,180:cpoints=19"
# The fourth run (the new gear, engine/main/dtugear.h): the new items, a
# gem and a wand worn and carried, a save to slot 3 and a load. The fifth
# run loads that save with -nonewgear: the new lines do not exist, the load
# leaves the items out (dtu_inventory_append), their effects end at the
# first frame (gear_lost_effects, dtuchar.c) and the game goes on.
SCRIPT4 = ("5:give=15.53.1,6:give=15.58.1,7:give=15.59.1,8:give=16.129.3,9:give=5.63.6,10:equip=15.58,"
           "11:equip=15.59,12:equip=15.53,20:dump,30:save=3,40:load=3,90:dump")
SCRIPT5 = "20:load=3,60:dump"
NEW_ITEMS = ("type 15 id 53 ", "type 15 id 58 ", "type 15 id 59 ", "type 16 id 129 ")
SG_PLAYER = 141         # the player record in a save of the port (DTU_SG_PLAYER_OFFSET)


def dump(lines, frame):
    """The lines of the dump at frame, without the time and the frame."""
    out = []
    for ln in lines:
        m = re.match(r"\s*[0-9.]+ f(\d+)\s+(.*)$", ln)
        if m and int(m.group(1)) == frame and not m.group(2).startswith("script:"):
            out.append(m.group(2).rstrip())
    return out


def slots(lines, frame):
    """The mage slots left and the maximum of level 1 at frame, or None."""
    for ln in dump(lines, frame):
        m = re.match(r"spell slots: mage left (\d+) \d+ \d+ max (\d+)", ln)
        if m:
            return int(m.group(1)), int(m.group(2))
    return None


def run(cmd, tmp, errors, name):
    try:
        p = subprocess.run(cmd, cwd=tmp, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=180)
        out = p.stdout.decode("latin-1")
        if p.returncode != 0:
            errors.append("%s: exit code %d" % (name, p.returncode))
    except subprocess.TimeoutExpired as e:
        out = (e.stdout or b"").decode("latin-1")
        errors.append("%s: no exit in 180 s" % name)
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
        base = [os.path.abspath(a.exe), "-datadir", a.game]
        for d in a.datadir:
            if os.path.isdir(d):
                base += ["-datadir", d]
        base += ["-nomovies", "-mute", "-hidden", "-nolog", "-userdir", tmp, "-fixedfps", "30"]
        cmd = base + ["-level", "YPINN", "-frames", "450", "-charlog", log, "-chartest", SCRIPT, "-memreport"]
        out = run(cmd, tmp, errors, "first run")
        for ln in out.splitlines():
            if "MEM_" in ln or "Int3" in ln:
                errors.append("output: " + ln.strip())
        m = re.search(r"(\d+) Kbytes dynamic data in use", out)
        if not m:
            errors.append("no memory report")
        elif m.group(1) != "0":
            errors.append("%s Kbytes in use at the exit" % m.group(1))
        lines = open(log, encoding="latin-1").read().splitlines() if os.path.exists(log) else []
        before, after = dump(lines, 180), dump(lines, 340)
        if not before:
            errors.append("no dump before the save")
        if not any(ln.startswith("dungeon level 4,") for ln in before):
            errors.append("the warp to BONES did not happen")
        strip = lambda d: [re.sub(r", difficulty \d+", "", ln) for ln in d]
        if strip(before) != strip(after):
            errors.append("the dump after the load differs:\n  before: %s\n  after:  %s" %
                          ("\n          ".join(before), "\n          ".join(after)))
        # The hot key boxes: the box of Detect Magic (armspell=12) and the
        # other boxes are the same after the save and the load.
        hk_before, hk_after = dump(lines, 182), dump(lines, 342)
        if not any(re.match(r"hot key \d+: spell 12 ", ln) for ln in hk_before):
            errors.append("no hot key box with spell 12 before the save: %s" % hk_before)
        if hk_before != hk_after:
            errors.append("the hot key boxes after the load differ:\n  before: %s\n  after:  %s" %
                          ("\n          ".join(hk_before), "\n          ".join(hk_after)))
        if not any(re.match(r"dungeon level 4, difficulty 2,", ln) for ln in after):
            errors.append("the loaded game does not play at difficulty 2")
        if not any("cannot load slot 4" in ln for ln in lines):
            errors.append("no log line for the load of the empty slot 4")

        # The spell slots: (frame, mage slots left of level 1, maximum, what)
        for frame, left, mx, what in ((30, 0, 1, "the level change to HUB1 filled the slot"),
                                      (36, 1, 1, "the rest did not fill the slot"),
                                      (90, 0, 1, "the cast did not use the slot"),
                                      (110, 0, 1, "the level change to YPINN filled the slot"),
                                      (160, 0, 1, "the level change to BONES filled the slot"),
                                      (345, 0, 1, "the load filled the slot")):
            got = slots(lines, frame)
            if got != (left, mx):
                errors.append("%s (frame %d: left and maximum %s, expected %s)" % (what, frame, got, (left, mx)))
        sg = os.path.join(tmp, "SG3.SG")
        if not os.path.exists(sg):
            errors.append("no save in slot 3")
        else:
            b = open(sg, "rb").read()
            if len(b) <= SG_PLAYER + 0x2C or b[SG_PLAYER + 0x23] != 1 or b[SG_PLAYER + 0x2C] != 0:
                errors.append("the save of slot 3 does not have the mage maximum 1 and the slots left 0")

        # The second run: the load, the exit of the inn, Waterdeep, the portal
        log2 = os.path.join(tmp, "char2.log")
        fe = os.path.join(tmp, "fe.txt")
        with open(fe, "w") as f:
            f.write(FESCRIPT2)
        cmd = base + ["-fescript", fe, "-charlog", log2, "-chartest", SCRIPT2]
        run(cmd, tmp, errors, "second run")
        lines = open(log2, encoding="latin-1").read().splitlines() if os.path.exists(log2) else []
        starts = [ln for ln in lines if "level start:" in ln]
        if len(starts) != 2:
            errors.append("second run: %d level starts, expected 2 (the load and the game start after Waterdeep)"
                          % len(starts))
        got = slots(lines, 20)
        if got != (0, 1):
            errors.append("the load of slot 0 filled the slot (left and maximum %s, expected (0, 1))" % (got,))
        got = slots(lines, 300)
        if got != (1, 1):
            errors.append("the game start after Waterdeep did not fill the slot (left and maximum %s, expected (1, 1))"
                          % (got,))

        # The third run (D-134, D-138): KOBOLDS. The Potion of Agility (600 s)
        # at frame 10 and a save 0.4 s later: the save has 600 seconds left
        # (1.3 cuts to 599). The spellcaster object 19 (3 mage points) keeps
        # 1 point after the save and the load (1.3 gives it 3 again).
        log3 = os.path.join(tmp, "char3.log")
        for f in ("SG3.SG",):
            if os.path.exists(os.path.join(tmp, f)):
                os.remove(os.path.join(tmp, f))
        cmd = base + ["-level", "KOBOLDS", "-frames", "200", "-charlog", log3, "-chartest", SCRIPT3]
        run(cmd, tmp, errors, "third run")
        lines = open(log3, encoding="latin-1").read().splitlines() if os.path.exists(log3) else []
        sg = os.path.join(tmp, "SG3.SG")
        if not os.path.exists(sg):
            errors.append("third run: no save in slot 3")
        else:
            b = open(sg, "rb").read()
            off = SG_PLAYER + 0x1A93 + 12
            left = int.from_bytes(b[off:off + 4], "little", signed=True) if len(b) >= off + 4 else None
            if left != 600:
                errors.append("third run: the save has %s seconds of the Potion of Agility, expected 600" % left)
        pts = [ln for ln in lines if re.search(r"\bf180\s+cpoints: obj 19 ", ln)]
        if not pts or "mage 1 cleric 0" not in pts[0]:
            errors.append("third run: the spellcaster after the load: %s, expected mage 1 cleric 0" %
                          (pts[0].strip() if pts else "no line"))
        # The fourth and the fifth run (the new gear)
        for f in ("SG3.SG",):
            if os.path.exists(os.path.join(tmp, f)):
                os.remove(os.path.join(tmp, f))
        log4, log5 = os.path.join(tmp, "char4.log"), os.path.join(tmp, "char5.log")
        run(base + ["-level", "KOBOLDS", "-frames", "120", "-charlog", log4, "-chartest", SCRIPT4, "-newgear"],
            tmp, errors, "fourth run")
        lines = open(log4, encoding="latin-1").read().splitlines() if os.path.exists(log4) else []
        before, after = dump(lines, 20), dump(lines, 90)
        if not all(any(n in ln for ln in before) for n in NEW_ITEMS):
            errors.append("fourth run: the new items are not in the dump: %s" % before)
        if strip(before) != strip(after):
            errors.append("fourth run: the dump after the load differs:\n  before: %s\n  after:  %s" %
                          ("\n          ".join(before), "\n          ".join(after)))
        run(base + ["-level", "KOBOLDS", "-frames", "80", "-charlog", log5, "-chartest", SCRIPT5, "-nonewgear"],
            tmp, errors, "fifth run")
        lines = open(log5, encoding="latin-1").read().splitlines() if os.path.exists(log5) else []
        after = dump(lines, 60)
        if not after or any(n in ln for ln in after for n in NEW_ITEMS) or not any("effects 0x00000000," in ln for ln in after):
            errors.append("fifth run (-nonewgear): the new items after the load: %s" % after)
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
