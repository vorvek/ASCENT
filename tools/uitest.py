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

    def run(self, name, lines, extra=(), level=None, env=None):
        """Runs the script lines; "shot NAME" writes NAME.ppm. Returns the
        screenshots by name. env: more environment variables (for example
        ASCENT_WINDOW_SIZE, the size of the hidden window)."""
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
        r = subprocess.run(cmd, capture_output=True, text=True, timeout=300, cwd=d,
                           env=dict(os.environ, **env) if env else None)
        self.stdout = r.stdout
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


def mouse_states(t):
    """The lines of the script command mousestate: (relative, immersive)."""
    out = []
    for line in t.stdout.splitlines():
        if line.startswith("Mousestate "):
            f = dict(kv.split("=") for kv in line.split()[1:])
            out.append((int(f["relative"]), int(f["immersive"])))
    return out


def test_immersive_screens(t):
    """The inventory and the statistics screen free the mouse from the
    immersive mode and the mode returns when they close; the map keeps it;
    the immersive key on a screen is the choice of the player (dtudisp.c,
    the screens that free the mouse; a port feature)."""
    st = ["mousestate"]
    t.run("immersive_screens", ["gamekeys", "wait 20", "key v", "wait 5"] + st +
          ["key i", "wait 5"] + st + ["key p", "wait 5"] + st + ["key tab", "wait 5"] + st +
          ["key i", "wait 5"] + st + ["key i", "wait 5"] + st +
          ["key p", "wait 5", "key v", "wait 5"] + st + ["key p", "wait 5"] + st +
          ["key i", "wait 5", "key v", "wait 5", "key v", "wait 5"] + st + ["key i", "wait 5"] + st)
    got = mouse_states(t)
    want = [(1, 1), (0, 0), (0, 0), (1, 1), (0, 0), (1, 1), (1, 1), (1, 1), (0, 0), (0, 0)]
    assert got == want, "mouse states %s, expected %s" % (got, want)


def test_immersive_screen_no_jump(t):
    """The mouse movement made while a screen frees the mouse does not turn
    the view when the immersive mode returns."""
    spot = (200, 100, 240, 120)
    moves = ["rel 200 0", "wait 2"] * 5
    s, _ = t.run("immersive_jump", ["gamekeys", "wait 20", "key v", "wait 10", "shot a", "key i", "wait 5"] +
                 moves + ["key i", "wait 10", "shot b"], extra=["-fixedfps", "30"])
    c, _ = t.run("immersive_turn", ["gamekeys", "wait 20", "key v", "wait 10", "shot a"] +
                 moves + ["wait 10", "shot b"], extra=["-fixedfps", "30"])
    assert differ(region(c["a"], *spot), region(c["b"], *spot)) > 0.2, "rel did not turn the view"
    assert differ(region(s["a"], *spot), region(s["b"], *spot)) < 0.05, "the view turned"


def view_states(t):
    """The lines of the script command viewstate, as dicts."""
    out = []
    for line in t.stdout.splitlines():
        if line.startswith("Viewstate "):
            out.append(dict(kv.split("=") for kv in line.split()[1:]))
    return out


VIEW_SIZES = ["gamekeys", "wait 30", "viewstate", "key =", "wait 10", "viewstate", "key -", "wait 10",
              "viewstate", "key i", "wait 10", "viewstate", "key i", "wait 5"]


def test_view_aspect(t):
    """Each screen size (main, = and F3 full screen, the inventory) shows
    the same shapes: a square thing is square with the pixel shape that the
    window shows, and the angle of view in y is the same (0x41193,
    g3_start_frame 0xD93E8). Also with the low resolution (square pixels
    and -screen43) and the software renderer. -origui: the aspects of the
    executable (2.4 in high resolution, 1.2 in low resolution), so a square
    thing is 1.2 times too wide except on the 4:3 screen of the low
    resolution (Dtu_fix_view_aspect)."""
    for extra, want in (([], 1.0), (["-software"], 1.0), (["-lowres"], 1.0), (["-lowres", "-software"], 1.0),
                        (["-lowres", "-screen43"], 1.0), (["-origui"], 1.2), (["-lowres", "-origui"], 1.2),
                        (["-lowres", "-screen43", "-origui"], 1.0)):
        t.run("view_aspect", VIEW_SIZES, extra=extra)
        v = view_states(t)
        assert len(v) == 4, "%s: %d view states" % (extra, len(v))
        modes = [s["mode"] for s in v]
        assert modes == ["0", "7", "0", "3"], "%s: modes %s" % (extra, modes)
        for s in v:
            assert abs(float(s["shape"]) - want) < 0.01, "%s: shape %s in mode %s" % (extra, s["shape"], s["mode"])
        # the small view of a screen can be taller than wide (s > 1 in
        # g3_start_frame): its angle in x is the fixed one then
        for s in v[:3]:
            assert s["tany"] == v[0]["tany"], "%s: the angle in y changed in mode %s" % (extra, s["mode"])


def test_view_screen_scale(t):
    """The horizontal and the vertical scale of the 3D view agree in the
    window: the window pixels per unit of the tangent of the angle of view
    (the view rectangle of the frame times the window pixels of a frame
    pixel, presentstate, over the tangent of the half angle, viewstate).
    In low and high resolution, with "4:3 screen" off and on
    (Dtu_fix_view_aspect, Dtu_fix_lowres_aspect)."""
    lines = ["gamekeys", "wait 30", "viewstate", "presentstate", "key =", "wait 10", "viewstate", "presentstate",
             "key -", "wait 5"]
    for extra in ([], ["-screen43"], ["-lowres"], ["-lowres", "-screen43"]):
        t.run("view_screen_scale", lines, extra=extra, env={"ASCENT_WINDOW_SIZE": "1280x720"})
        v = view_states(t)
        p = present_states(t)
        assert len(v) == 2 and len(p) == 2, "%s: %d view and %d present states" % (extra, len(v), len(p))
        for s, (ww, wh, fw, fh, rx, ry, rw, rh) in zip(v, p):
            dw, dh = (int(x) for x in s["dest"].split("x"))
            sx = dw * rw / fw / float(s["tanx"])
            sy = dh * rh / fh / float(s["tany"])
            print("    %s mode %s: x %.1f, y %.1f window pixels (ratio %.4f)" % (extra, s["mode"], sx, sy, sx / sy))
            assert abs(sx / sy - 1) < 0.01, "%s mode %s: the scales %.1f and %.1f differ" % (extra, s["mode"], sx, sy)


def pair_equal(img, box, axis, offset):
    """The fraction of the pixel pairs (p, p + 1) along axis (0: x, 1: y)
    with p - box start = offset mod 2 that are equal, in box (x, y, w, h)."""
    iw, ih, px = img
    x0, y0, w, h = box
    same = n = 0
    for y in range(y0, y0 + h - 1):
        for x in range(x0, x0 + w - 1):
            if (x - x0 if axis == 0 else y - y0) % 2 != offset:
                continue
            a = (y * iw + x) * 3
            b = a + 3 if axis == 0 else a + iw * 3
            n += 1
            same += max(abs(px[a + c] - px[b + c]) for c in range(3)) <= 8
    return same / max(1, n)


def test_render_scale_half(t):
    """The internal resolution x1/2 (Dtu_render_scale 0, -renderscale half)
    renders the full screen view of the 640x480 interface at 320x239: with
    the nearest scale filter each view pixel is a block of 2 by 2 frame
    pixels. At x1 the view is 640x239 (each line shown twice): blocks of 1
    by 2. The interface is not scaled. -renderscale 0.5 is the same step."""
    lines = ["gamekeys", "wait 30", "key =", "wait 10", "shot full", "key -", "wait 5"]
    env = {"ASCENT_SCALE_FILTER": "nearest", "ASCENT_SHOT_SCALE": "1"}
    box = (40, 40, 560, 380)        # inside the full screen view (0, 0) 640x478
    got = {}
    for arg in ("half", "0.5", "1"):
        s, d = t.run("render_scale_half", lines, extra=["-renderscale", arg], env=env)
        img = s["full"]
        assert img[:2] == (640, 480), "%s: the shot is %dx%d" % (arg, img[0], img[1])
        got[arg] = [pair_equal(img, box, axis, o) for axis in (0, 1) for o in (0, 1)]
        print("    -renderscale %s: equal pairs x %.3f / %.3f, y %.3f / %.3f" % ((arg,) + tuple(got[arg])))
        if arg != "1":
            from_shot = os.path.join(d, "full.ppm")
            print("    screenshot %s" % from_shot)
    for arg in ("half", "0.5"):
        x0, x1, y0, y1 = got[arg]
        # one phase of each axis is inside a block (all equal), the other
        # is the edge between two blocks
        assert max(x0, x1) > 0.99 and min(x0, x1) < 0.9, "%s: no blocks of 2 in x" % arg
        assert max(y0, y1) > 0.99 and min(y0, y1) < 0.9, "%s: no blocks of 2 in y" % arg
    x0, x1, y0, y1 = got["1"]
    assert max(x0, x1) < 0.9, "x1: blocks in x"
    assert max(y0, y1) > 0.99, "x1: the lines are not doubled"


def present_states(t):
    """The lines of the script command presentstate: (window w, h, frame w,
    h, rect x, y, w, h)."""
    out = []
    for line in t.stdout.splitlines():
        if line.startswith("Presentstate "):
            f = dict(kv.split("=") for kv in line.split()[1:])
            ww, wh = (int(v) for v in f["window"].split("x"))
            fw, fh = (int(v) for v in f["frame"].split("x"))
            xy, size = f["rect"].rsplit(",", 1)
            rx, ry = (float(v) for v in xy.split(","))
            rw, rh = (float(v) for v in size.split("x"))
            out.append((ww, wh, fw, fh, rx, ry, rw, rh))
    return out


def picture_box(img):
    """The bounding box (x, y, w, h) of the pixels that are not black."""
    iw, ih, px = img
    x0, y0, x1, y1 = iw, ih, -1, -1
    for y in range(ih):
        row = px[y * iw * 3:(y + 1) * iw * 3]
        left = len(row) - len(row.lstrip(b"\0"))
        if left == len(row):
            continue
        right = len(row) - len(row.rstrip(b"\0"))
        y0 = min(y0, y)
        y1 = y
        x0 = min(x0, left // 3)
        x1 = max(x1, iw - 1 - right // 3)
    return x0, y0, x1 - x0 + 1, y1 - y0 + 1


PRESENT = ["gamekeys", "wait 30", "presentstate", "shot main", "key esc", "wait 10", "presentstate",
           "shot menu", "key esc", "wait 5"]


def test_present_aspect(t):
    """The 320x200 frame of the low resolution has square pixels (16:10) by
    default. With -screen43 ("4:3 screen", Dtu_fix_lowres_aspect) it fills
    a 4:3 rectangle of the window, as on the 4:3 monitor of DOS, with black
    bars at the sides of a 16:9 or 16:10 window, in both renderers, in the
    game and in a menu. The 640x480 frame of the high resolution is 4:3 in
    both forms. The screenshot of ASCENT_SHOT_SIZE is the window as it shows
    the frame."""
    q = ["-lowres", "-screen43"]
    cases = [(q, "1280x720", 4 / 3), (q, "960x720", 4 / 3), (q, "1280x800", 4 / 3),
             (q + ["-software"], "1280x720", 4 / 3), ([], "1280x720", 4 / 3), (["-screen43"], "1280x720", 4 / 3),
             (["-lowres"], "1280x720", 1.6), (["-lowres", "-squarepixels"], "1280x720", 1.6),
             (["-lowres", "-software"], "1280x720", 1.6), (q + ["ASCENT_NO_GL"], "1280x720", 4 / 3),
             (["-lowres", "ASCENT_NO_GL"], "1280x720", 1.6)]
    for extra, size, want in cases:
        env = {"ASCENT_WINDOW_SIZE": size, "ASCENT_SHOT_SIZE": size}
        nogl = "ASCENT_NO_GL" in extra
        if nogl:
            # the SDL_Renderer form of video.c; its screenshot is the frame
            env["ASCENT_NO_GL"] = "1"
        s, _ = t.run("present_aspect", PRESENT, extra=[a for a in extra if a != "ASCENT_NO_GL"], env=env)
        ww, wh = (int(v) for v in size.split("x"))
        p = present_states(t)
        assert len(p) == 2, "%s %s: %d present states" % (extra, size, len(p))
        for st, shot in zip(p, ("main", "menu")):
            sw, sh, fw, fh, rx, ry, rw, rh = st
            label = "%s %s %s" % (extra, size, shot)
            assert (sw, sh) == (ww, wh), "%s: window %dx%d" % (label, sw, sh)
            assert (fw, fh) == ((640, 480) if "-lowres" not in extra else (320, 200)), "%s: frame %dx%d" % (label, fw, fh)
            assert abs(rw / rh - want) < 0.01, "%s: shape %.4f (rect %.0fx%.0f)" % (label, rw / rh, rw, rh)
            # the largest rectangle of that shape, in the middle
            assert abs(rw - ww) < 1 or abs(rh - wh) < 1, "%s: rect %.0fx%.0f" % (label, rw, rh)
            # (the OpenGL form keeps the rectangle in whole pixels)
            assert abs(rx * 2 + rw - ww) <= 3 and abs(ry * 2 + rh - wh) <= 3, "%s: not centered" % label
            if not nogl:
                bx, by, bw, bh = picture_box(s[shot])
                assert abs(bw / bh - want) < 0.01, "%s: the picture is %dx%d" % (label, bw, bh)


def test_window_mouse_16_9(t):
    """A mouse event of the system in a 16:9 window lands on the right
    pixel of the 320x200 frame shown at 4:3 with -screen43 (the bars are 160
    pixels wide in a 1280x720 window, a frame pixel is 3 by 3.6 window
    pixels), in both renderers, and with square pixels, the default
    (1152x720 at x 64, 3.6 by 3.6). A click
    on the item Graphics of the ESC menu opens the Graphics menu."""
    points = [(16, 20), (160, 100), (300, 180)]
    q = ["-lowres", "-screen43"]
    for extra, x0, kx, nogl in ((q, 160, 3.0, 0), (q + ["-software"], 160, 3.0, 0),
                                (q, 160, 3.0, 1), (["-lowres"], 64, 3.6, 0), (["-lowres"], 64, 3.6, 1)):
        lines = ["gamekeys", "wait 30", "key esc", "wait 10"]
        for fx, fy in points:
            lines += ["winmove %.1f %.1f" % (x0 + (fx + 0.5) * kx, (fy + 0.5) * 3.6), "wait 3", "mousestate"]
        # the item Graphics of the ESC menu (frame 130, 129)
        lines += ["winmove %.1f %.1f" % (x0 + 130.5 * kx, 129.5 * 3.6), "wait 3", "shot menu",
                  "winclick %.1f %.1f" % (x0 + 130.5 * kx, 129.5 * 3.6), "wait 10", "shot gfx",
                  "key esc", "wait 5", "key esc", "wait 5"]
        env = {"ASCENT_WINDOW_SIZE": "1280x720"}
        if nogl:
            env["ASCENT_NO_GL"] = "1"      # the SDL_Renderer form of video.c
        s, _ = t.run("window_mouse", lines, extra=extra, env=env)
        extra = extra + (["ASCENT_NO_GL"] if nogl else [])     # for the messages
        got = []
        for line in t.stdout.splitlines():
            if line.startswith("Mousestate "):
                f = dict(kv.split("=") for kv in line.split()[1:])
                got.append((int(f["x"]), int(f["y"])))
        assert got == points, "%s: the mouse is at %s, not %s" % (extra, got, points)
        # the menus are 320x200 frames: compare the middle of the frame
        a, b = s["menu"], s["gfx"]
        assert a[0] == b[0] and a[1] == b[1]
        assert differ(a[2], b[2]) > 0.1, "%s: the click did not open the Graphics menu" % extra


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


# The scroll of the main menu: FEBGND entry 17, 490 x 480, centered (x 75
# to 565).
def test_fe_main_menu_scroll(t):
    """The main menu draws the picture "scroll2.pcx" (0x631D2: the table
    0x13E328 gives FEBGND entry 17) over the Waterdeep picture."""
    s, d = t.run("fe_scroll", ["wait 60", "shot menu"], level="")
    text = session_log(d)
    assert "Front end: main menu picture scroll2.pcx (FEBGND 17, 490 x 480)" in text, "no scroll in the main menu"
    if t.args.shots:
        os.makedirs(t.args.shots, exist_ok=True)
        save_png(s["menu"], os.path.join(t.args.shots, "scroll_menu.png"))


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


# ASCENT: the next level window (engine/main/dtuxp.c): a right click on the
# experience field of the information areas (227,130 81x6 of the 320x200
# frame, doubled in the 640x480 interface) or on a class line of the
# statistics screen. The character is made a thief and mage with 2112
# experience in each class (thief level 2, mage level 1), as in the report
# of a player.
XP_FIELD = (454 + 40, 260 + 6)
XP_CLASS_LINE = (20 + 30, 163 + 6)
XP_CHAR = "5:class=3,6:class2=1,7:xp=4224"


def test_next_level_window(t):
    """The right click on the experience field and on the class line opens
    the window; a click closes it. The window in both languages and both
    interfaces, to look at (--shots)."""
    log = os.path.join(t.tmp, "xp_window", "char.log")
    # Enter closes the box of the new thief level
    lines = ["gamekeys", "wait 30", "key enter", "wait 5", "key p", "wait 5", "move 600 400", "wait 3", "shot base",
             "rclick %d %d" % XP_FIELD, "wait 5", "shot field", "click 600 400", "wait 5", "shot closed",
             "rclick %d %d" % XP_CLASS_LINE, "wait 5", "shot class", "click 600 400", "wait 5"]
    s, d = t.run("xp_window", lines, extra=["-fixedfps", "30", "-charlog", log, "-chartest", XP_CHAR])
    for name in ("field", "class"):
        assert differ(region(s["base"], *SCROLLTEXT), region(s[name], *SCROLLTEXT)) > 0.3, "no window for the " + name
    assert differ(region(s["base"], *SCROLLTEXT), region(s["closed"], *SCROLLTEXT)) < 0.05, "the window did not close"
    with open(log, encoding="latin-1") as f:
        text = f.read()
    for want in ("Thief, level 2: 2112 of 2500", "Mage, level 1: 2112 of 2500", "one half of each award"):
        assert want in text, "the window has no " + want
    shots = {"high_en": s["field"]}
    for lang in ("en", "es"):
        for res, extra in (("high", []), ("low", ["-lowres"])):
            if (res, lang) == ("high", "en"):
                continue
            # the experience field in the 320x200 frame of -lowres
            x, y = XP_FIELD if res == "high" else (227 + 40, 130 + 3)
            sh, dl = t.run("xp_%s_%s" % (res, lang), ["gamekeys", "wait 30", "key enter", "wait 5", "key p", "wait 5",
                                                    "rclick %d %d" % (x, y), "wait 5", "shot w"],
                           extra=["-fixedfps", "30", "-language", lang, "-chartest", XP_CHAR] + extra)
            shots["%s_%s" % (res, lang)] = sh["w"]
    if t.args.shots:
        os.makedirs(t.args.shots, exist_ok=True)
        for name, img in shots.items():
            save_png(img, os.path.join(t.args.shots, "xp_window_%s.png" % name))


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
