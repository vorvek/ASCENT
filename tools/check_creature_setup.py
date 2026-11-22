#!/usr/bin/env python3
"""Compare the creature setup by name of the port with a reference library.

The port logs each call of the creature setup by name (0xAC7B2) and each
add_npc (0xAC431) of the level start with the option -setuplog. This tool
runs the port on the levels of DTU.MSN, reads the session logs and runs the
reference C routine `dtu_creature_setup_by_name` of the DTU 1.3
decompilation (a DLL that was compared with the DOS code) on the same input
values, in the same object order, with one shared record for each type of a
level. Each value that the routine writes into the type (weapon, attack
type, dice, score, hit points, touch spell, regeneration, strength, door
bit) must be the same after each call. With --flag67 the quest flag 67 is
set, and the tool also checks the hostility after add_npc of the 15 names of
the Shadow Thieves: 1.3 with -origguildpeace, peaceful with
Dtu_fix_guild_peace.

  python -I tools/check_creature_setup.py --exe EXE --datadir DIR [--datadir DIR]
      [--hddir DIR] --library DLL --work DIR [--levels A,B] [--flag67] [--orig]
      [--software]

With --abilities (no --library needed) the tool checks the creature
abilities of docs/audit/monsters.md instead, with -chartest:
  - the touch spell of the lich (0x2A637): attackedby=10 in CLAN3 casts
    spell 22 (Hold Person) on the player;
  - the revive (0x1440C, Dtu_fix_revive): kill=1 in MINE, the troll gets up
    with all hit points; with -origrevive it stays dead;
  - the group alert (0x160AB): goobj=13 in CLAN3, the player runs into the
    ghast, which wakes the creatures that see its segment.

The levels run hidden and muted, each with its own -userdir in --work.
"""
import argparse
import ctypes
import glob
import os
import re
import struct
import subprocess
import sys

GUILD = {"sfmtd2", "sfmtf2", "shmfb1", "shmfb5", "shmfc3", "shmfg1", "shmfg5", "shmtd2",
         "shmtd4", "shmtd6", "shmtf2", "shmtf4", "shmtf6", "shdleadf", "shdleadg"}
STATE = re.compile(r"weapon (-?\d+) attack (-?\d+) dice (-?\d+)d(-?\d+) score (-?\d+) hp (-?\d+) "
                   r"touch (-?\d+) regen (-?\d+) str (-?\d+) door (\d+) levels (\d+)/(\d+)")
SETUP = re.compile(r"creature setup \(0xAC7B2\), level (\d+): obj (\d+) (.+?) type (\d+) script (\d+)"
                   r"(?: \(type only\))?:(.*); before (.*); after (.*)$")
NPC = re.compile(r"add_npc \(0xAC431\): obj (\d+) (.+?) script (\d+) (hostile|peaceful) awareness (\d+) target (-?\d+)")

# dtu_robot_info (dtu_layout.h of the decompilation): offset, struct format
FIELDS = (("weapon", 0x6A, "<h"), ("attack", 0x62, "<b"), ("count", 0x6E, "<h"), ("sides", 0x6C, "<h"),
          ("score", 0x0C, "<i"), ("hp", 0x78, "<h"), ("touch", 0x7C, "<h"), ("regen", 0x7E, "<h"),
          ("str", 0xDE, "<i"))


def state(text):
    m = STATE.search(text)
    assert m, text
    v = [int(x) for x in m.groups()]
    return dict(zip(("weapon", "attack", "count", "sides", "score", "hp", "touch", "regen", "str",
                     "door", "mage", "cleric"), v))


def robot_record(s):
    r = bytearray(242)
    for name, off, fmt in FIELDS:
        struct.pack_into(fmt, r, off, s[name])
    r[0x63] = s["door"]
    r[0xE2], r[0xE3] = s["mage"], s["cleric"]
    return r


def robot_values(raw):
    s = {name: struct.unpack_from(fmt, raw, off)[0] for name, off, fmt in FIELDS}
    s["door"] = raw[0x63] & 1
    return s


def run_level(args, level, work):
    user = os.path.join(work, level)
    os.makedirs(user, exist_ok=True)
    for old in glob.glob(os.path.join(user, "logs", "*.log")):
        os.remove(old)
    cmd = [os.path.abspath(args.exe)]
    for d in args.datadir:
        cmd += ["-datadir", d]
    if args.hddir:
        cmd += ["-hddir", args.hddir]
    cmd += ["-nomovies", "-mute", "-hidden", "-userdir", user, "-level", level, "-frames", "20", "-setuplog"]
    if args.flag67:
        cmd += ["-scriptflags", "67"]
    if args.orig:
        cmd += ["-origguildpeace"]
    if args.software:
        cmd += ["-software"]
    r = subprocess.run(cmd, capture_output=True, text=True, timeout=300, cwd=user)
    logs = glob.glob(os.path.join(user, "logs", "*.log"))
    assert logs, "%s: no session log (exit %d)" % (level, r.returncode)
    with open(logs[0], encoding="latin1") as f:
        return r.returncode, f.read().splitlines()


def run_test(args, name, level, frames, script, extra=()):
    """One run with -chartest; returns the lines of the object log."""
    user = os.path.join(args.work, name)
    os.makedirs(user, exist_ok=True)
    objlog = os.path.join(user, "obj.txt")
    if os.path.exists(objlog):
        os.remove(objlog)
    cmd = [os.path.abspath(args.exe)]
    for d in args.datadir:
        cmd += ["-datadir", d]
    if args.hddir:
        cmd += ["-hddir", args.hddir]
    # the player gets its hit points back each 10 frames, so that it lives
    heal = ",".join("%d:heal=100" % f for f in range(10, frames, 10))
    cmd += ["-nomovies", "-mute", "-hidden", "-userdir", user, "-level", level, "-frames", str(frames),
            "-objlog", objlog, "-chartest", heal + "," + script] + list(extra)
    if args.software:
        cmd += ["-software"]
    r = subprocess.run(cmd, capture_output=True, text=True, timeout=600, cwd=user)
    assert r.returncode == 0, "%s: exit %d" % (name, r.returncode)
    with open(objlog, encoding="latin1") as f:
        return f.read().splitlines()


def check_abilities(args):
    lines = run_test(args, "touch", "CLAN3", 60, "5:attackedby=10,7:attackedby=10")
    touch = [l for l in lines if "attack: the touch of Lich3 casts spell 22 on the player" in l]
    held = [l for l in lines if "effect Hold Person on obj 0" in l]
    assert touch and held, "touch spell: %d casts, %d holds" % (len(touch), len(held))
    print("touch spell: %d casts of spell 22 by the lich, %d hold" % (len(touch), len(held)))

    lines = run_test(args, "revive", "MINE", 4000, "5:kill=1")
    up = [l for l in lines if "obj 1 Troll: gets up again with 54 hit points" in l]
    assert up, "revive: the troll did not get up"
    lines = run_test(args, "revive-orig", "MINE", 4000, "5:kill=1", ["-origrevive"])
    assert not [l for l in lines if "gets up again" in l], "revive: the troll got up with -origrevive"
    print("revive: the troll gets up (%s); with -origrevive it stays dead" % up[0].split()[0])

    lines = run_test(args, "alert", "CLAN3", 30, "3:goobj=13")
    woken = [l for l in lines if "alert: level 3 from obj 13 Ghast wakes" in l]
    assert woken, "alert: nothing woken"
    print("alert: the bump of the ghast wakes %d creatures" % len(woken))


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--exe", required=True)
    p.add_argument("--datadir", action="append", required=True)
    p.add_argument("--hddir")
    p.add_argument("--library")
    p.add_argument("--work", required=True)
    p.add_argument("--levels")
    p.add_argument("--flag67", action="store_true")
    p.add_argument("--orig", action="store_true")
    p.add_argument("--software", action="store_true")
    p.add_argument("--abilities", action="store_true")
    args = p.parse_args()

    if args.abilities:
        check_abilities(args)
        return 0
    assert args.library, "--library is needed for the creature setup check"

    lib = ctypes.CDLL(os.path.abspath(args.library))
    setup = lib.dtu_creature_setup_by_name
    setup.argtypes = [ctypes.c_char_p] + [ctypes.c_void_p] * 4 + [ctypes.c_int32]
    setup.restype = None

    if args.levels:
        levels = args.levels.split(",")
    else:
        msn = None
        for d in args.datadir:
            if os.path.exists(os.path.join(d, "DTU.MSN")):
                msn = os.path.join(d, "DTU.MSN")
                break
        with open(msn, encoding="latin1") as f:
            levels = [m.upper() for m in re.findall(r"^\s*([A-Za-z0-9]+)\.lvl\b", f.read(), re.M | re.I)]

    calls = fallbacks = guild_checked = 0
    missing = []
    branches = {}
    for level in levels:
        code, lines = run_level(args, level, args.work)
        types = {}
        level_calls = 0
        for line in lines:
            m = SETUP.search(line)
            if m:
                index, obj, name, kind, script = (int(m.group(1)), int(m.group(2)), m.group(3),
                                                  int(m.group(4)), int(m.group(5)))
                what, before, after = m.group(6), state(m.group(7)), state(m.group(8))
                if kind not in types:
                    types[kind] = ctypes.create_string_buffer(bytes(robot_record(before)), 242)
                else:
                    # the shared record: the port starts where the last call ended
                    want = robot_values(types[kind].raw)
                    got = {k: before[k] for k in want}
                    assert want == got, "%s obj %d %s: before %s, reference %s" % (level, obj, name, got, want)
                robot = types[kind]
                record = bytearray(479)
                record[4:6] = bytes((2, kind & 0xFF))
                struct.pack_into("<I", record, 0x1B0, script)
                actor = ctypes.create_string_buffer(bytes(record), 479)
                local = ctypes.create_string_buffer(47)
                player = bytearray(6967)
                player[0x71] = index
                if args.flag67:
                    player[0x1A3B] |= 4
                pl = ctypes.create_string_buffer(bytes(player), 6967)
                setup(name.encode("latin1"), robot, actor, local, pl, 0)
                want = robot_values(robot.raw)
                got = {k: after[k] for k in want}
                assert want == got, "%s obj %d %s: after %s, reference %s" % (level, obj, name, got, want)
                guild = bool(args.flag67 and name.lower() in GUILD)
                assert ("flag67 peaceful" in what) == guild, "%s obj %d %s: flag 67 %s" % (level, obj, name, what)
                if guild:
                    assert local.raw[0] == 0 and not actor.raw[0x1B2] & 0x40
                if "-> 33" in what:
                    fallbacks += 1
                for b in ("spectatr", "hatchlng", "gargoyle", "flayer", "lich"):
                    if " " + b in what:
                        branches[b] = branches.get(b, 0) + 1
                calls += 1
                level_calls += 1
            m = NPC.search(line)
            if m and args.flag67:
                name = m.group(2)
                if name.lower() in GUILD:
                    guild_checked += 1
                    hostile = m.group(4) == "hostile"
                    if not args.orig:
                        assert not hostile, "%s: %s" % (level, line)
        if code != 0 and level_calls == 0:
            print("%s: exit %d, not in the data" % (level, code))
            missing.append(level)
            continue
        print("%s: exit %d, %d creature setups matched" % (level, code, level_calls))
        assert code == 0, level
    print("%d creature setups matched the reference in %d levels; %d weapon fallbacks; branches %s"
          % (calls, len(levels) - len(missing), fallbacks, branches))
    if not args.levels:
        # The counts of the decompilation (recovered/creatures.md): 965
        # placements in 31 levels; with the level fixes 0xACCDB of a fresh load
        # 22 fallbacks in object order (recovered/level_fixes.md), 12 flayer,
        # 1 flaynpc and 1 lich branches
        assert calls == 965 and fallbacks == 22, (calls, fallbacks)
        assert branches.get("flayer") == 13 and branches.get("lich") == 1, branches
    if args.flag67:
        print("%d add_npc lines of the 15 guild names checked (%s)" % (guild_checked, "1.3" if args.orig else "Dtu_fix_guild_peace"))


if __name__ == "__main__":
    sys.exit(main())
