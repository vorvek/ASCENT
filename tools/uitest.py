#!/usr/bin/env python3
"""Tests of the in-game interface (docs/audit/ingame-ui.md).

Each test starts the game hidden and muted at a level, drives it with an
-fescript script and checks the screenshots of the script. The tests need
the game files, so they are not registered with ctest.

    python tools/uitest.py --exe build/engine/main/ascent.exe \
        --datadir E:/UNDAMONTAN/DTU --datadir D:/dev/ASCENT/gamedata/cd/DTU \
        --hddir D:/dev/ASCENT/gamedata/hd [--only NAME]

The screenshot of the port is the 640x480 interface scaled 3 times.
"""
import argparse
import os
import subprocess
import sys
import tempfile

LEVEL = "YPINN"
SCALE = 3


def read_ppm(path):
    with open(path, "rb") as f:
        data = f.read()
    parts = []
    pos = 0
    while len(parts) < 4:
        while data[pos:pos + 1].isspace():
            pos += 1
        if data[pos:pos + 1] == b"#":
            while data[pos:pos + 1] != b"\n":
                pos += 1
            continue
        start = pos
        while not data[pos:pos + 1].isspace():
            pos += 1
        parts.append(data[start:pos])
    pos += 1
    w, h = int(parts[1]), int(parts[2])
    return w, h, data[pos:pos + w * h * 3]


def region(img, x, y, w, h):
    """The pixels of a rectangle of the 640x480 interface."""
    iw, ih, px = img
    out = []
    for yy in range(y * SCALE, (y + h) * SCALE):
        row = yy * iw * 3
        out.append(px[row + x * SCALE * 3:row + (x + w) * SCALE * 3])
    return b"".join(out)


def differ(a, b):
    """The fraction of the bytes that differ by more than 8."""
    n = sum(1 for i in range(0, len(a)) if abs(a[i] - b[i]) > 8)
    return n / max(1, len(a))


class Runner:
    def __init__(self, args):
        self.args = args
        self.tmp = tempfile.mkdtemp(prefix="uitest_")

    def run(self, name, lines, extra=()):
        """Runs the script lines; "shot NAME" writes NAME.ppm. Returns the
        screenshots by name."""
        d = os.path.join(self.tmp, name)
        os.makedirs(os.path.join(d, "user"), exist_ok=True)
        script = os.path.join(d, "script.txt")
        shots = []
        with open(script, "w") as f:
            for line in lines:
                if line.startswith("shot "):
                    shot = line[5:].strip()
                    shots.append(shot)
                    line = "shot " + os.path.join(d, shot + ".ppm").replace("\\", "/")
                f.write(line + "\n")
            f.write("wait 2\nquit\n")
        cmd = [os.path.abspath(self.args.exe)]
        for dd in self.args.datadir:
            cmd += ["-datadir", dd]
        if self.args.hddir:
            cmd += ["-hddir", self.args.hddir]
        cmd += ["-nomovies", "-mute", "-userdir", os.path.join(d, "user"),
                "-level", LEVEL, "-fescript", script] + list(extra)
        r = subprocess.run(cmd, capture_output=True, text=True, timeout=300)
        out = {}
        for s in shots:
            p = os.path.join(d, s + ".ppm")
            if not os.path.exists(p):
                raise AssertionError("%s: no screenshot %s (exit %d)\n%s" % (name, s, r.returncode, r.stdout[-2000:]))
            out[s] = read_ppm(p)
        return out, r.stdout


# The portrait area of the main screen (ingame-ui.md, "Layout variables")
# and the list column of the inventory screen differ there.
LEFT = (10, 34, 100, 90)


def test_f3_screen(t):
    """F3 on a screen (inventory) returns to the main screen (0x40974)."""
    s, _ = t.run("f3_screen", ["gamekeys", "wait 20", "shot main", "key i", "wait 5", "shot inv",
                              "key f3", "wait 5", "shot after"])
    assert differ(region(s["main"], *LEFT), region(s["inv"], *LEFT)) > 0.2, "the inventory did not open"
    assert differ(region(s["main"], *LEFT), region(s["after"], *LEFT)) < 0.02, "F3 did not return to the main screen"


def test_equal_minus(t):
    """"=" makes the main screen full screen, "-" returns (0x40974, 0x40C3A)."""
    s, _ = t.run("equal_minus", ["gamekeys", "wait 20", "shot main", "key =", "wait 5", "shot full",
                                "key -", "wait 5", "shot back"])
    assert differ(region(s["main"], *LEFT), region(s["full"], *LEFT)) > 0.2, "= did not change the screen"
    assert differ(region(s["main"], *LEFT), region(s["back"], *LEFT)) < 0.02, "- did not return to the main screen"


# The text of the message box of the main screen, high resolution
MSGBOX = (131, 306, 470, 77)
MSG_V = ["gamekeys", "wait 20"] + ["key v", "wait 3"] * 10 + ["move 300 200", "wait 3", "shot base"]


def test_msg_arrows(t):
    """The arrows of the message box scroll bar (0x2FF58): the up arrow
    shows older lines (Dtu_fix_msg_scroll), the down arrow newer ones."""
    s, _ = t.run("msg_arrows", MSG_V + ["click 618 314", "wait 3", "move 300 200", "wait 3", "shot up",
                                        "click 617 375", "wait 3", "move 300 200", "wait 3", "shot down"])
    assert differ(region(s["base"], *MSGBOX), region(s["up"], *MSGBOX)) > 0.002, "the up arrow did not scroll"
    assert differ(region(s["base"], *MSGBOX), region(s["down"], *MSGBOX)) < 0.001, "the down arrow did not scroll back"


def test_msg_arrows_orig(t):
    """-origui: the up arrow of the original shows newer lines, so at the
    newest line it does nothing, and the down arrow shows older lines."""
    s, _ = t.run("msg_arrows_orig", MSG_V + ["click 618 314", "wait 3", "move 300 200", "wait 3", "shot up",
                                             "click 617 375", "wait 3", "move 300 200", "wait 3", "shot down"],
                 extra=["-origui"])
    assert differ(region(s["base"], *MSGBOX), region(s["up"], *MSGBOX)) < 0.001, "the up arrow scrolled"
    assert differ(region(s["base"], *MSGBOX), region(s["down"], *MSGBOX)) > 0.002, "the down arrow did not scroll"


def test_msg_bar_and_hold(t):
    """A click at the top of the bar shows the oldest lines; a held down
    arrow comes back to the newest line; the wheel scrolls."""
    s, _ = t.run("msg_bar", MSG_V + ["click 615 326", "wait 3", "move 300 200", "wait 3", "shot top",
                                     "press 617 375", "wait 60", "release", "move 300 200", "wait 3", "shot held",
                                     "move 300 340", "wheel 2", "wait 3", "shot wheel"])
    assert differ(region(s["base"], *MSGBOX), region(s["top"], *MSGBOX)) > 0.002, "the bar did not scroll"
    assert differ(region(s["base"], *MSGBOX), region(s["held"], *MSGBOX)) < 0.001, "the held arrow did not scroll back"
    assert differ(region(s["base"], *MSGBOX), region(s["wheel"], *MSGBOX)) > 0.002, "the wheel did not scroll"


# The inventory list of the high resolution inventory screen
INVLIST = (5, 5, 80, 290)


def test_inv_sbar(t):
    """The arrows of the inventory list scroll bar (0x3010C) move the list
    by one row."""
    if not t.args.char:
        print("skip test_inv_sbar: no --char")
        return
    s, _ = t.run("inv_sbar", ["gamekeys", "wait 20", "key i", "wait 5", "move 300 200", "wait 3", "shot base",
                              "click 98 285", "wait 3", "move 300 200", "wait 3", "shot down",
                              "click 99 16", "wait 3", "move 300 200", "wait 3", "shot up"],
                 extra=["-char", t.args.char])
    assert differ(region(s["base"], *INVLIST), region(s["down"], *INVLIST)) > 0.05, "the down arrow did not scroll"
    assert differ(region(s["base"], *INVLIST), region(s["up"], *INVLIST)) < 0.001, "the up arrow did not scroll back"


TESTS = [v for k, v in sorted(globals().items()) if k.startswith("test_")]


def main():
    p = argparse.ArgumentParser()
    p.add_argument("--exe", required=True)
    p.add_argument("--datadir", action="append", default=[])
    p.add_argument("--hddir")
    p.add_argument("--only")
    p.add_argument("--char")
    args = p.parse_args()
    t = Runner(args)
    failed = 0
    for test in TESTS:
        if args.only and args.only not in test.__name__:
            continue
        try:
            test(t)
            print("PASS", test.__name__)
        except AssertionError as e:
            failed += 1
            print("FAIL", test.__name__, e)
    print("%d failed" % failed)
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
