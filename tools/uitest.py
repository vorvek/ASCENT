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
        cmd += ["-nomovies", "-mute", "-userdir", os.path.join(d, "user")]
        if level != "":     # "": the front end (no direct start)
            cmd += ["-level", level or LEVEL]
        cmd += ["-fescript", script] + list(extra)
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


def type_keys(word):
    out = []
    for c in word:
        out += ["key " + c, "wait 2"]
    return out


def session_log(d):
    for root, _, files in os.walk(d):
        for f in files:
            if f.startswith("ascent-") and f.endswith(".log"):
                with open(os.path.join(root, f), errors="replace") as fh:
                    return fh.read()
    return ""


def test_cheat_codes(t):
    """The cheat codes of 1.3 act (0x4324E, 0x428B9 to 0x42BED): zazb,
    glassy, and eightisgreat with its message 0x125783 (its two I keys open
    and close the inventory). The cheat enable code of Descent 1 does
    nothing."""
    lines = ["gamekeys", "wait 20", "move 300 200"] + type_keys("zazb") + type_keys("glassy")
    lines += ["wait 5", "shot before"] + type_keys("eightisgreat") + ["wait 5", "shot sword"]
    lines += type_keys("gabbagabbahey") + ["wait 5", "shot d1"]
    s, d = t.run("cheats", lines)
    text = session_log(d)
    for c in ("zazb", "glassy", "eightisgreat"):
        assert "cheat " + c in text, "the code %s did not act" % c
    assert differ(region(s["before"], *MSGBOX), region(s["sword"], *MSGBOX)) > 0.001, "no message"
    assert differ(region(s["sword"], *MSGBOX), region(s["d1"], *MSGBOX)) < 0.001, "a Descent 1 code acted"


# The front end (docs/audit/screens.md). A corner of the picture, away from
# the menu box and the credits column (x 112 to 528).
FE_CORNER = (0, 0, 100, 100)


def test_fe_credits_key_only(t):
    """The credits (0x2C61B) end on a key (0xDA3E4), not on a mouse click,
    and the main menu comes back."""
    s, _ = t.run("fe_credits", ["wait 60", "shot menu", "key down", "wait 2", "key down", "wait 2", "key down",
                                "wait 2", "key enter", "wait 90", "shot credits", "click 320 240", "wait 30",
                                "shot clicked", "key esc", "wait 60", "shot after"], level="")
    assert differ(region(s["menu"], *FE_CORNER), region(s["credits"], *FE_CORNER)) > 0.2, "the credits did not start"
    assert differ(region(s["credits"], *FE_CORNER), region(s["clicked"], *FE_CORNER)) < 0.02, "a click ended the credits"
    assert differ(region(s["menu"], *FE_CORNER), region(s["after"], *FE_CORNER)) < 0.05, "a key did not end the credits"


# The description bar of the high resolution inventory screen (area 0x3BC524)
DESCBAR = (10, 376, 416, 14)


def test_inv_message_in_desc_bar(t):
    """In the inventory screen a new message also goes to the description
    bar (0x34A83 to 0x34AD2): the V key adds "Immersive mode is on"."""
    s, _ = t.run("inv_msg", ["gamekeys", "wait 20", "key i", "wait 5", "move 600 460", "wait 3", "shot base",
                             "key v", "wait 5", "shot msg"])
    assert differ(region(s["base"], *DESCBAR), region(s["msg"], *DESCBAR)) > 0.01, "the bar shows no message"


# The mage count column of the spell tables of the statistics screen
# (0x144CF8, 48 wide, rows 305 to 385)
MAGECOUNT = (142, 305, 48, 80)


def test_stats_spell_table(t):
    """The statistics screen has the spell tables (0x331AC): a fighter has
    "n/a" in the mage column, a mage of --char the slots."""
    if not t.args.char:
        print("skip test_stats_spell_table: no --char")
        return
    lines = ["gamekeys", "wait 20", "key p", "wait 5", "move 600 460", "wait 3", "shot stats"]
    f, _ = t.run("stats_fighter", lines)
    m, _ = t.run("stats_mage", lines, extra=["-char", t.args.char])
    assert differ(region(f["stats"], *MAGECOUNT), region(m["stats"], *MAGECOUNT)) > 0.01, "the mage column is the same"


def test_fe_death_picture_key(t):
    """The death picture (0x42517 to 0x4252E) ignores the mouse and goes on
    at the first key, with no shortest time; then the Death Menu, and after
    Abort Game the main menu has no Save Game (0x42423: no character, the
    first list of Waterdeep)."""
    _, d = t.run("fe_death", ["gamekeys", "wait 200", "click 320 240", "wait 20", "click 320 240", "wait 20",
                              "key space", "wait 30", "key down", "wait 3", "key enter", "wait 10",
                              "key enter", "wait 60", "shot menu"],
                 extra=["-fixedfps", "30", "-chartest", "5:damage=999"])
    text = session_log(d)
    assert "Front end: death picture left" in text, "the death picture did not end on a key"
    left = text.index("Front end: death picture left")
    assert "Front end: main menu, 5 items\n" in text[left:], "the main menu after the death:\n" + \
        "\n".join(l for l in text.splitlines() if "Front end" in l)


# The description window (engine/main/dtudesc.c, docs/description_window.md)
# uses the picture and the text box of the scroll text screen. The cells
# of the inventory list, high resolution: entry 1 (Broad Sword) at 51,9,
# entry 2 (Leather Armor) at 9,67, entry 4 (Potion of Healing) at 9,125.
DESC_CELLS = {"weapon": (67, 25), "armor": (25, 83), "potion": (25, 141)}
DESC_EMPTY = (67, 141)      # the cell of entry 5


def save_png(img, path):
    """Writes a screenshot as PNG (to look at it), if Pillow is there."""
    try:
        from PIL import Image
    except ImportError:
        return
    w, h, px = img
    Image.frombytes("RGB", (w, h), px).save(path)


def test_desc_window(t):
    """A right click on an entry of the inventory opens the description
    window over the play screen; a click closes it. / opens it for the
    entry of the keys. The window of a spell (descspell) and of a line of
    the market (-testmarket, a right click on the first row)."""
    log = os.path.join(t.tmp, "desc_items", "char.log")
    lines = ["gamekeys", "wait 20", "key i", "wait 5", "move 600 400", "wait 3", "shot base"]
    for name, (x, y) in DESC_CELLS.items():
        lines += ["rclick %d %d" % (x, y), "wait 5", "shot " + name, "click 600 400", "wait 5", "shot closed_" + name]
    lines += ["key /", "wait 5", "shot slash", "key x", "wait 5", "shot closed_slash"]
    # an empty cell (the character has five entries): no window
    lines += ["rclick %d %d" % DESC_EMPTY, "wait 5", "shot empty"]
    # a right click with the window open closes it and opens no new one
    x, y = DESC_CELLS["weapon"]
    lines += ["rclick %d %d" % (x, y), "wait 5", "rclick %d %d" % (x, y), "wait 5", "shot twice"]
    # with an item on the cursor: no window; a click on no place puts the item back
    lines += ["click %d %d" % (x, y), "wait 5", "rclick %d %d" % DESC_CELLS["armor"], "wait 5", "move 600 400", "wait 3",
              "shot held", "click 600 400", "wait 5"]
    s, d = t.run("desc_items", lines, extra=["-charlog", log])
    # the longest texts in the small box of the low resolution, in Spanish
    # and in English, to look at (--shots)
    longest = {}
    for lang in ("es", "en"):
        for what, cmd in (("item", "descitem=5.54"), ("spell", "descspell=5"), ("spell2", "descspell=41")):
            sh, dl = t.run("desc_low_%s_%s" % (lang, what), ["gamekeys", "wait 60", "shot w"],
                           extra=["-fixedfps", "30", "-lowres", "-language", lang, "-chartest", "30:" + cmd])
            longest["low_%s_%s" % (lang, what)] = sh["w"]
    sh, dl = t.run("desc_high_es_item", ["gamekeys", "wait 60", "shot w"],
                   extra=["-fixedfps", "30", "-language", "es", "-chartest", "30:descitem=5.54"])
    longest["high_es_item"] = sh["w"]
    sp, d2 = t.run("desc_spell", ["gamekeys", "wait 60", "shot spell"],
                   extra=["-fixedfps", "30", "-chartest", "30:descspell=16"])
    mk, d3 = t.run("desc_market", ["wait 60", "move 600 300", "wait 3", "shot market_base", "rclick 120 53", "wait 5",
                                   "shot market", "click 600 300", "wait 5", "shot market_closed"],
                   extra=["-testmarket"], level="")
    if t.args.shots:
        os.makedirs(t.args.shots, exist_ok=True)
        for name in list(DESC_CELLS) + ["slash"]:
            save_png(s[name], os.path.join(t.args.shots, "desc_%s.png" % name))
        save_png(sp["spell"], os.path.join(t.args.shots, "desc_spell.png"))
        save_png(mk["market"], os.path.join(t.args.shots, "desc_market.png"))
        for name, img in longest.items():
            save_png(img, os.path.join(t.args.shots, "desc_%s.png" % name))
    for name in DESC_CELLS:
        assert differ(region(s["base"], *SCROLLTEXT), region(s[name], *SCROLLTEXT)) > 0.3, "no window for the " + name
        assert differ(region(s["base"], *SCROLLTEXT), region(s["closed_" + name], *SCROLLTEXT)) < 0.05, \
            "the window of the %s did not close" % name
    assert differ(region(s["base"], *SCROLLTEXT), region(s["slash"], *SCROLLTEXT)) > 0.3, "/ did not open the window"
    for name, what in (("closed_slash", "a key did not close the window"), ("empty", "a window for an empty cell"),
                       ("twice", "a second right click left a window open"), ("held", "a window with an item on the cursor")):
        assert differ(region(s["base"], *SCROLLTEXT), region(s[name], *SCROLLTEXT)) < 0.05, what
    with open(log) as f:
        text = f.read()
    for word in ("Broad Sword", "Leather Armor", "Heals 4 to 10 hit points."):
        assert word in text, "the window has no " + word
    assert differ(region(s["base"], *SCROLLTEXT), region(sp["spell"], *SCROLLTEXT)) > 0.3, "no window for the spell"
    # the front end screenshot is not scaled: compare the whole frame
    assert differ(mk["market_base"][2], mk["market"][2]) > 0.2, "no window in the market"
    assert differ(mk["market_base"][2], mk["market_closed"][2]) < 0.02, "the window of the market did not close"


TESTS = [v for k, v in sorted(globals().items()) if k.startswith("test_")]


def main():
    p = argparse.ArgumentParser()
    p.add_argument("--exe", required=True)
    p.add_argument("--datadir", action="append", default=[])
    p.add_argument("--hddir")
    p.add_argument("--only")
    p.add_argument("--char")
    p.add_argument("--shots", help="write the screenshots of some tests as PNG files to this directory")
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
