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

    def run(self, name, lines, extra=(), level=None):
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
                "-level", level or LEVEL, "-fescript", script] + list(extra)
        r = subprocess.run(cmd, capture_output=True, text=True, timeout=300, cwd=d)
        out = {}
        for s in shots:
            p = os.path.join(d, s + ".ppm")
            if not os.path.exists(p):
                raise AssertionError("%s: no screenshot %s (exit %d)\n%s" % (name, s, r.returncode, r.stdout[-2000:]))
            out[s] = read_ppm(p)
        return out, d


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


# The middle of the view, where the menus open
MIDDLE = (200, 120, 160, 80)


def test_alt_f2_f3(t):
    """Alt+F2 opens the Save Game slot menu, Alt+F3 the restore slot menu
    (0x432B8, 0x432C2), and Alt+F3 does not change the screen as F3 does."""
    s, _ = t.run("alt_f2_f3", ["gamekeys", "wait 20", "move 300 200", "wait 3", "shot base",
                               "hold lalt 4", "key f2", "wait 5", "shot save", "key esc", "wait 5", "shot back",
                               "hold lalt 4", "key f3", "wait 5", "shot load", "key esc", "wait 5"])
    assert differ(region(s["base"], *MIDDLE), region(s["save"], *MIDDLE)) > 0.2, "Alt+F2 opened no menu"
    assert differ(region(s["base"], *LEFT), region(s["load"], *LEFT)) < 0.02, "Alt+F3 changed the screen"
    assert differ(region(s["back"], *MIDDLE), region(s["load"], *MIDDLE)) > 0.2, "Alt+F3 opened no menu"


# Durnan in YPINN: the conversation opens (docs/re/scripts.md)
DURNAN = ["-fixedfps", "30", "-testpos", "-60,-8.7,9,270", "-testmove", "wait:10,use:1"]
TEXTFRAME = (100, 15, 440, 30)


def test_dlg_blocks_interface(t):
    """While the conversation window is open the interface keys do nothing
    (0x4391D calls no display input and no key table)."""
    s, _ = t.run("dlg_keys", ["gamekeys", "wait 60", "shot open", "key i", "wait 5", "shot i",
                              "key p", "wait 5", "shot p", "key esc", "wait 30"], extra=DURNAN)
    assert differ(region(s["open"], *LEFT), region(s["i"], *LEFT)) < 0.02, "I opened the inventory"
    assert differ(region(s["open"], *LEFT), region(s["p"], *LEFT)) < 0.02, "P opened the statistics"


def test_dlg_tab_focus(t):
    """Tab moves the focus of the conversation window to the text, also with
    no reply (0xB2BB5), and the window stays open."""
    s, _ = t.run("dlg_tab", ["gamekeys", "wait 60", "shot open", "key tab", "wait 5", "shot tab",
                             "key esc", "wait 30"], extra=DURNAN)
    assert differ(region(s["open"], *TEXTFRAME), region(s["tab"], *TEXTFRAME)) > 0.003, "Tab did not move the focus"
    assert differ(region(s["open"], *LEFT), region(s["tab"], *LEFT)) < 0.02, "Tab opened the map"


def test_shift_fkeys_and_grave(t):
    """Shift+F1 to Shift+F8 add no stereo messages of Descent 1 (not in the
    switch 0x42F9C), Shift+F3 does not change the screen, and ` writes a
    screenshot (0x42FEC)."""
    s, d = t.run("shift_f", ["gamekeys", "wait 20", "move 300 200", "wait 3", "shot base",
                             "hold lshift 4", "key f1", "wait 3", "hold lshift 4", "key f3", "wait 3",
                             "hold lshift 4", "key f8", "wait 3", "key `", "wait 5", "shot after"])
    assert differ(region(s["base"], *MSGBOX), region(s["after"], *MSGBOX)) < 0.001, "a message was added"
    assert differ(region(s["base"], *LEFT), region(s["after"], *LEFT)) < 0.02, "the screen changed"
    assert any(f.lower().startswith("screen") for f in os.listdir(d)), "` wrote no screenshot"


def test_map_minus_equal(t):
    """While the map has the controls (a click in the map, 0x305C9) minus
    and equal change its segment limit (0x18944) and do not change the
    screen as they do in the key switch."""
    s, _ = t.run("map_keys", ["gamekeys", "wait 20", "key tab", "wait 5", "click 200 200", "wait 5",
                              "move 420 440", "wait 3", "shot map",
                              "key -", "wait 2", "key -", "wait 2", "key -", "wait 2", "key -", "wait 5", "shot less",
                              "key =", "wait 2", "key =", "wait 2", "key =", "wait 2", "key =", "wait 5", "shot more"])
    info = (450, 180, 170, 100)
    mapr = (60, 40, 300, 300)
    assert differ(region(s["map"], *info), region(s["less"], *info)) < 0.02, "minus changed the screen"
    assert differ(region(s["map"], *info), region(s["more"], *info)) < 0.02, "equal changed the screen"
    assert differ(region(s["map"], *mapr), region(s["less"], *mapr)) > 0.001, "minus did not change the map"


def test_immersive_off_centers_mouse(t):
    """Immersive mode off puts the mouse in the middle of the view
    (0x54470 to 0x544D7)."""
    s, _ = t.run("immersive", ["gamekeys", "wait 20", "move 40 260", "wait 3", "shot before",
                               "key v", "wait 5", "key v", "wait 5", "shot after"])
    spot = (25, 245, 30, 30)
    assert differ(region(s["before"], *spot), region(s["after"], *spot)) > 0.05, "the cursor stayed in place"


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


# The figure of the paper doll and the armor slot, high resolution
DOLL = (270, 200)
ARMOR_SLOT = (383, 143, 32, 32)


def drag(x, y, to_x, to_y):
    return ["press %d %d" % (x, y), "wait 3", "move %d %d" % ((x + to_x) // 2, (y + to_y) // 2), "wait 2",
            "move %d %d" % (to_x, to_y), "wait 3", "release", "wait 3"]


def test_doll_drop(t):
    """An item taken at the press and dragged onto the figure of the paper
    doll is used, worn or armed (the press action 0x35ADF acts while the
    button is down, 0x9865B), and an item dragged onto USE is used at the
    release. The default fighter: entry 2 Leather Armor, entry 4 Potion of
    Healing, entry 5 Potion of Extra-Healing (both given)."""
    log = os.path.join(t.tmp, "doll_drop", "char.log")
    lines = ["gamekeys", "wait 20", "key i", "wait 5", "move 600 400", "wait 3", "shot base"]
    lines += drag(25, 141, *DOLL) + drag(25, 83, *DOLL) + ["move 600 400", "wait 3", "shot worn"]
    lines += drag(67, 141, 490, 370)
    s, _ = t.run("doll_drop", lines, extra=["-chartest", "5:give=16.79.3,5:give=16.75.2,5:damage=6", "-charlog", log])
    with open(log) as f:
        text = f.read()
    assert "paper doll: entry 4 'Potion of Healing' is used" in text, "the potion on the figure was not used"
    assert "equip: entry 2 'Leather Armor' in slot 3" in text, "the armor on the figure was not worn"
    assert "use: entry 5 type 16 id 75" in text, "the potion on USE was not used"
    assert differ(region(s["base"], *ARMOR_SLOT), region(s["worn"], *ARMOR_SLOT)) > 0.1, "the armor slot is empty"


# The text box of the scroll text screen, high resolution (85 + 85, 40 + 80)
SCROLLTEXT = (170, 120, 300, 200)


def test_scroll_text(t):
    """Using the Scroll opens the scroll text screen over the play screen
    (0x9DC42); while it is open a key goes to the screen and closes it
    (0x9D165), so I does not open the inventory."""
    s, _ = t.run("scroll_text", ["gamekeys", "wait 20", "move 600 400", "wait 3", "shot base", "wait 40",
                                 "shot open", "key i", "wait 10", "shot closed"],
                 extra=["-fixedfps", "30", "-chartest", "30:use=16.102"])
    assert differ(region(s["base"], *SCROLLTEXT), region(s["open"], *SCROLLTEXT)) > 0.3, "the scroll did not open"
    assert differ(region(s["base"], *SCROLLTEXT), region(s["closed"], *SCROLLTEXT)) < 0.05, "the scroll did not close"
    assert differ(region(s["base"], *LEFT), region(s["closed"], *LEFT)) < 0.02, "I opened the inventory"


# The thief skill point screen, high resolution: the rows, the value of the
# first row and the message line (85 + 100, 40 + 130; 40 + 399 - 100 + 5)
THIEFROWS = (185, 170, 270, 100)
THIEFVALUE = (188, 173, 50, 14)
THIEFMSG = (185, 344, 270, 14)


def test_thief_points(t):
    """Talking to Mirt as a thief of level 1 gives 40 points (0xABB53); Yes
    opens the screen (0xB5B49), = adds a point to the selected skill
    (0xB44B4), and Escape with points left keeps the screen and shows
    "You must spend all points" (0xB5F56)."""
    s, _ = t.run("thief_points", ["gamekeys", "wait 15", "move 600 400", "wait 3", "shot base", "wait 25",
                                  "shot open", "key =", "wait 3", "shot plus", "key esc", "wait 3", "shot esc"],
                 extra=["-fixedfps", "30", "-autochoice", "0", "-chartest", "5:class=3,20:goobj=2,25:activate"])
    assert differ(region(s["base"], *THIEFROWS), region(s["open"], *THIEFROWS)) > 0.3, "the screen did not open"
    assert differ(region(s["open"], *THIEFVALUE), region(s["plus"], *THIEFVALUE)) > 0.02, "= added no point"
    assert differ(region(s["plus"], *THIEFROWS), region(s["esc"], *THIEFROWS)) < 0.01, "Escape closed the screen"
    assert differ(region(s["plus"], *THIEFMSG), region(s["esc"], *THIEFMSG)) > 0.02, "no message"


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
