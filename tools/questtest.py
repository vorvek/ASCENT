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
# MYRKUL, Daland: the first talk ("You did not have time"), then "Sure", "And...", "How terrible!", "I think I can do that." (label 13 gives misc 74)
DALAND_DLG = "1,1,2,1,2," + ",".join(["1"] * 20)
# MYRKUL: the load of the level stops twice at the Int3 of fix_object_segs
# (object.c): an object of the level file has no segment. Not a case of the
# Daland tests; it happens in main too.
MYRKUL_INT3 = r"Int3: file [^\n]*object\.c, line \d+"
# CLAN3: the lich (object 10) alone, a fight with him (tools/check_caster_ai.py)
LICH_FIGHT = "2:seed=1,4:alone=10,5:setlevel=8,8:heal=500,10:goobjsee=10.20,15:spelltest=28.1.10.0"
LICH_FULL = (r"caster: obj 10 Lich3 notices the player .*contingency: Stone Skin, Shield, Minor Globe of Invulnerability, "
             r"Mirror Image")
# The steps of the end sequence 0xA6786 (engine/main/dtuobj_scenery.c), in order
END_SEQ = (r"end sequence: start \(0xA6786\), sword in seg 487, light obj \d+, spin 0x147\s+"
           r"[\s\S]*end sequence: 147 steps, light 441\.0, spin 0x269a\s+end sequence: sound -1 on the sword\s+"
           r"end sequence: 50 explosions, 50 sounds\s+end of the game: CREDITV1")


def T(name, level, script, expect, reject=(), frames=400, dlg=ONES, extra=(), invuln=True, also=(), known=None):
    """known: a regex of an error line of the level that is not the case of
    the test (it is reported elsewhere); such lines are left out of the
    error test."""
    return dict(name=name, level=level, script=script, expect=list(expect) + list(also), reject=list(reject),
                frames=frames, dlg=dlg, extra=list(extra) + (["-invulnerability"] if invuln else []), known=known)


# The quest order of the campaign. Each test names the script, trigger or
# DTU.WRP line that it checks in its comment.
TESTS = [
    # Mission 1, KOBOLDS: DTU.WRP command 1 at segment 6 sets mission flag 1
    # when no creature with script 23 (the chief) lives.
    T("kobolds_flag1_alive", "KOBOLDS", "30:goseg=6,60:quest",
      [r"quest: mflags \[ \]"], frames=90),
    T("kobolds_flag1", "KOBOLDS", "20:killscript=23,40:goseg=6,60:quest",
      [r"mission check 1: done", r"quest: mflags \[ 1 \]"], frames=90),
    # KOBOLDS: statement E5 (walk) of script 21 (object 8, flags 4 to 7):
    # the kobold walks to segment 424 (it passes other segments on the
    # way), and the blocks CB E7 424, 43 and 125 teleport it and walk it on
    # to segment 119 (Dtu_fix_script_walk). -noattack keeps the player
    # alive for the 43 seconds.
    T("kobolds_walk", "KOBOLDS",
      "10:goobj=8.20,12:objflag=8.4,12:objflag=8.5,12:objflag=8.6,12:objflag=8.7,15:talk=8,"
      "90:obj=8,150:obj=8,300:obj=8,1290:obj=8",
      [r"f90 +obj 8: type 2 id 1 seg (?!337 )(?!424 )\d+ .* anim 2",
       r"f150 +obj 8: type 2 id 1 seg (?!337 )(?!424 )\d+ .* anim 2",
       r"walk to seg 424\s[\s\S]*walk: obj 8 is at the end of its path in seg 424\s+script: run script 21 of obj 8 .* in mode 2 [^\n]*\s+script: teleport NPC to seg 29 "
       r"[\s\S]*end of its path in seg 43\s[\s\S]*end of its path in seg 125\s[\s\S]*end of its path in seg 119\s",
       r"f1290 +obj 8: type 2 id 1 seg 119 .* anim 1"],
      frames=1300, extra=["-noattack"]),
    # The same walk with a save and a load on the way (1.3 keeps the path in
    # Point_segs): the kobold walks on to segment 424 and the script goes
    # on to segment 119.
    T("kobolds_walk_saveload", "KOBOLDS",
      "10:goobj=8.20,12:objflag=8.4,12:objflag=8.5,12:objflag=8.6,12:objflag=8.7,15:talk=8,"
      "90:obj=8,95:save=3,100:load=3,140:obj=8,1290:obj=8",
      [r"f140 +obj 8: type 2 id 1 seg (?!341 )(?!424 )\d+ ",
       r"walk: obj 8 is at the end of its path in seg 424\s[\s\S]*end of its path in seg 43\s[\s\S]*end of its path in seg 119\s",
       r"f1290 +obj 8: type 2 id 1 seg 119 .* anim 1"],
      frames=1300, extra=["-noattack"]),
    # The other scripts with statement E5 (Dtu_fix_script_walk). THIEF0
    # script 53 (object 48, label 800): WALK 181 178 165 keeps the last
    # segment (D-076).
    T("thief0_walk53", "THIEF0", "10:goobj=48.20,12:resume=48.800,15:talk=48,890:obj=48",
      [r"run script 53 of obj 48 .* at label 320", r"walk to seg 165\s", r"walk: obj 48 is at the end of its path in seg 165\s",
       r"f890 +obj 48: type 2 id 0 seg 165 "], frames=900, extra=["-noattack"]),
    # THIEF0 script 54 (Devlin, object 41) with 12 of the 13 goblins dead:
    # "The goblins are already dead." (label 300) calls Mishad (script 53,
    # label 900), who waits rand(500..1000) and, with seed 1, kills the last
    # goblin and sets his resume label to 1. Devlin says "Good work Mishad."
    # and, with Dtu_fix_script_devlin, goes on to label 800 (flag 4 of
    # object 10). 1.3 (-origscript) ends the run there.
    T("thief0_devlin_goblins", "THIEF0", "5:seed=1," + ",".join("6:kill=%d" % n for n in
      (31, 30, 28, 29, 38, 36, 34, 37, 39, 33, 32, 35)) + ",10:goobj=41.20,15:talk=41",
      [r"script: wait \d+\s", r"Good work Mishad\.[\s\S]*goto 3FF\s+script: set flag 5 of obj 10\s+"
       r"script: goto 320\s+script: set flag 4 of obj 10\s"], frames=500,
      dlg="enter,5,enter,enter,2," + ",".join(["enter"] * 10), extra=["-noattack"]),
    T("thief0_devlin_goblins_orig", "THIEF0", "5:seed=1," + ",".join("6:kill=%d" % n for n in
      (31, 30, 28, 29, 38, 36, 34, 37, 39, 33, 32, 35)) + ",10:goobj=41.20,15:talk=41",
      [r"Good work Mishad\.[\s\S]*I mean, you did a good job as well\.\"\s+dlg: choice 0\s+script: end of script 54 of obj 41"],
      reject=[r"set flag 4 of obj 10"], frames=500,
      dlg="enter,5,enter,enter,2," + ",".join(["enter"] * 10), extra=["-noattack", "-origscript"]),
    # THIEF1 script 62 (object 25, label 10): WALK 161.
    T("thief1_walk62", "THIEF1", "10:goobj=25.20,12:resume=25.10,15:talk=25,890:obj=25",
      [r"walk to seg 161\s", r"walk: obj 25 is at the end of its path in seg 161\s", r"f890 +obj 25: type 2 id \d+ seg 161 "],
      frames=900, extra=["-noattack"]),
    # THIEF1 script 60 (object 95, label 930): out of its area the guard
    # walks back to segment 639 and the script runs there in mode 2.
    T("thief1_walk60", "THIEF1", "8:objseg=95.630,10:goobj=95.20,12:resume=95.930,15:talk=95",
      [r"walk to seg 639\s", r"walk: obj 95 is at the end of its path in seg 639\s", r"run script 60 of obj 95 .* in mode 2 "],
      frames=300, extra=["-noattack"]),
    # THIEF2 script 71 (object 1, label 900): WALK 635 625. A door on the
    # path (segment 635 side 5) opens when the goblin walks into it; the
    # door opener (0x14AF2) does not send a walker to a door.
    T("thief2_walk71", "THIEF2", "10:goobj=1.20,12:resume=1.900,15:talk=1,300:obj=1",
      [r"walk to seg 625\s", r"obj 1 gbloser: opens the door seg 635 side 5", r"walk: obj 1 is at the end of its path in seg 625\s",
       r"f300 +obj 1: type 2 id 17 seg 625 "], frames=310, extra=["-noattack", "-objlog", "thief2_walk_obj.log"]),
    # PRISON script 103 (object 21, a prisoner with no door bit): with MISC
    # 59 the prisoner walks out of its cell and opens the cell door. The
    # path search stops at a depth of 30 segments (0x1686D), short of
    # segment 428.
    T("prison_walk103", "PRISON", "5:give=16.59,10:goobj=21.20,15:talk=21,300:obj=21",
      [r"walk to seg 428\s", r"obj 21 hffg1: opens the door seg 607 side 5", r"f300 +obj 21: type 2 id 18 seg (?!607 )\d+ .* anim 2"],
      frames=310, extra=["-noattack", "-objlog", "prison_walk_obj.log"]),
    # PRISON object 34 (a goblin with the door bit) in its cell (seg 589).
    # The door is locked on the wall of the corridor only (wall 50, seg 347
    # side 4; trigger 45 opens it with MISC 59). Dtu_fix_door_pair: the
    # goblin stays in the cell. -origdoorpair: it opens the door from the
    # unlocked wall of the cell (wall 51), as 1.3. With Improved AI the
    # goblin does not notice the player through the closed door, so the
    # cases that need it to do so run with -noimprovedai.
    T("prison_cell_locked", "PRISON", "5:goseg=344,900:obj=34",
      [r"f900 +obj 34: type 2 id 25 seg 589 "], [r"obj 34 \S+: opens the door"],
      frames=910, extra=["-noattack", "-objlog", "prison_cell_obj.log"]),
    T("prison_cell_locked_orig", "PRISON", "5:goseg=344,900:obj=34",
      [r"obj 34 \S+: opens the door seg 589 side 5"],
      frames=910, extra=["-noattack", "-origdoorpair", "-noimprovedai", "-objlog", "prison_cell_orig_obj.log"]),
    # The same goblin attacks the player in the corridor (seg 344) for 10
    # seconds through the closed door. Dtu_fix_melee_wall: the door stops
    # each swing and the player takes no damage. -origmeleewall: the sphere
    # test of 0x6CFF8 starts behind the door and the swing hits, as 1.3.
    # (-invulnerability of the tests does not protect from the creatures.)
    T("prison_cell_melee", "PRISON", "5:goseg=344,300:obj=34",
      [r"obj 34 \S+: a wall or a closed door stops the weapon", r"f300 +obj 34: type 2 id 25 seg 589 "],
      [r"damage: the player takes", r"obj 34 \S+ swings at the player", r"obj 34 \S+: opens the door"],
      frames=310, extra=["-noimprovedai", "-objlog", "prison_melee_obj.log"]),
    # Improved AI: the goblin does not notice the player behind its door
    T("prison_cell_unaware", "PRISON", "5:goseg=344,300:obj=34",
      [r"f300 +obj 34: type 2 id 25 seg 589 "],
      [r"obj 34 \S+: sees the player", r"obj 34 \S+ swings at the player", r"obj 34 \S+: opens the door"],
      frames=310, extra=["-objlog", "prison_unaware_obj.log"]),
    T("prison_cell_melee_orig", "PRISON", "5:goseg=344,300:obj=34",
      [r"attack: obj 34 \S+ swings at the player"], [r"obj 34 \S+: opens the door"],
      frames=310, extra=["-origmeleewall", "-noimprovedai", "-objlog", "prison_melee_orig_obj.log"]),
    # The NPCs of YPINN stay at their places after a load. The door opener
    # (0x14AF2) can leave objects 28 and 30 in mode 2 with the goal segment
    # 0, which is also their station; that is no walk of a script.
    T("inn_npcs_stay_load", "YPINN",
      "10:aipath=28.0,10:aipath=30.0,20:save=3,30:load=3,600:obj=28,600:obj=30",
      [r"f600 +obj 28: type 2 id 6 seg 55 ", r"f600 +obj 30: type 2 id 5 seg 143 "], [r"walk: obj"],
      frames=610, extra=["-noattack"]),
    # Script 22 (objects 20, 21, 22, label 400): statement EE runs the
    # script of the other two, so the three walk at the same time. The run
    # of object 21 sets flag 1 (wary) of object 22, so the run of object 22
    # makes the charisma roll of start_label (0xAAA32): charisma 20 passes it.
    # Label 600 of the script sends object 22 to segment 385 or 388 at
    # random (rand(1..2)).
    T("kobolds_walk3", "KOBOLDS",
      "5:cha=20,10:goobj=20.20,12:resume=20.400,15:talk=20,30:obj=22,200:obj=20,200:obj=21,200:obj=22",
      [r"f30 +obj 22: type 2 id \d+ seg 383 .* anim 2", r"f200 +obj 20: type 2 id \d+ seg 386 ",
       r"f200 +obj 21: type 2 id \d+ seg 389 ", r"f200 +obj 22: type 2 id \d+ seg 38[58] ",
       r"end of its path in seg 386", r"end of its path in seg 389", r"walk: obj 22 is at the end of its path in seg 38[58]"],
      frames=210, extra=["-noattack"]),
    # D-125: statement EE (0xAA989) does not give the outer run its mode
    # back; Dtu_fix_script_run_mode does, so the talk counts (1.3: 0).
    T("script_ee_mode", "KOBOLDS", "5:cha=20,10:goobj=20.20,12:resume=20.400,15:talk=20,200:obj=20",
      [r"f200 +obj 20: type 2 id \d+ seg \d+ .* talks 1 "], frames=210, extra=["-noattack"]),
    T("script_ee_mode_orig", "KOBOLDS", "5:cha=20,10:goobj=20.20,12:resume=20.400,15:talk=20,200:obj=20",
      [r"f200 +obj 20: type 2 id \d+ seg \d+ .* talks 0 "], frames=210, extra=["-noattack", "-origscript"]),
    # The rule of 1.3 (-origscriptwalk): the kobold does not move.
    T("kobolds_walk_orig", "KOBOLDS",
      "10:goobj=8.20,12:objflag=8.4,12:objflag=8.5,12:objflag=8.6,12:objflag=8.7,15:talk=8,300:obj=8",
      [r"walk to seg 424", r"f300 +obj 8: type 2 id 1 seg 337 "], [r"end of its path"],
      frames=310, extra=["-noattack", "-origscriptwalk"]),

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
    # DROW2, the encounter with Tamaramagil (engine/main/dtudragon.c,
    # docs/re/dragon.md). 1.3 has no object for her; the port makes object
    # 64. The player stands in front of her (her position is the axis of
    # her collision cylinder, radius 29.4). "dragon=N" is the test command of
    # dtudragon.h: 1 claws, 2 bite, 3 breath, 4 her state.
    # The talk: the use action runs script 210. With the replies 2 (label
    # 6) and 2 (label 10) she stays at peace, and her third speech of label
    # 6 names the key in the cells.
    T("dragon_talk", "DROW2", "10:goseg=470,11:goobj=64.42,12:face=64,20:activate,280:dragon=4",
      [r"run script 210 of obj 64 \(Dragon\) in mode 0", r"speech \"In the corridor from whence you came, there is a lever that will release "
       r"my children\. The drow keep it behind a locked door, and its key lies in the cells behind me",
       r"reply 2 -> label A\b", r"speech \"Excellent! I pledge you the friendship of dragons",
       r" 280 dragon: test 4: obj 64 Dragon hp 90 ac -5 thac0 7 xp 15000 .* mode rest hostile 0 peaceful 1"],
      frames=300, dlg="1,2,1,1,2,1,1,1", extra=["-objlog", "dragon_talk_obj.log"]),
    # (Global flag 240 in the quest lines: the Flame Tongue of the new gear
    # is placed in DROW2, dtugear.h.)
    # The key lies in cell 407. The door of the alcove of the lever (wall
    # 133, seg 481 side 0) is locked and no thief skill opens it (keys 4);
    # with the key (MISC 43) the player opens it from the end of the
    # corridor (seg 481).
    T("dragon_key_alcove", "DROW2", "5:goside=481.0,12:activate,30:items=407,31:wall=481.0,40:give=16.43,60:goseg=481,100:wall=481.0",
      [r"item type 16 id 43 count 1 \"Jailer's Key\": obj \d+ in segment 407, picture \d+,", r"items in seg 407: 16\.43",
       r"use: door wall 133 clip \d+ flags 0x18 keys 4 state 0: locked", r"f31 +wall 481\.0: type 2 flags 0x18 state 0",
       r"dragon: the key of the alcove \(MISC 43\) opens the door of seg 481 side 0",
       r"message \"You use the jailer's key to unlock the door\.\"", r"f100 +wall 481\.0: type 2 flags 0x12 state"],
      frames=120, extra=["-noattack", "-triglog", "-objlog", "dragon_key_alcove_obj.log"]),
    # The lever (trigger 20): the hatchlings stay (1.3: script 213 sends
    # them away and sets global flag 107), the door of the hatchery (wall
    # 138, seg 398 side 4) opens and has no automatic close (flags 0x2), she
    # laughs and is hostile. After a save and a load the door is open, the
    # flags 241 and 242 of the port are set, and she is hostile.
    T("dragon_lever", "DROW2", "5:wall=398.4,6:goside=482.2,8:activate,40:wall=398.4,41:dragon=4,42:quest,43:obj=48,"
      "60:save=3,90:load=3,200:wall=398.4,201:quest,202:dragon=4,203:obj=48",
      [r"f5 +wall 398\.4: type 2 flags 0x18 state 0",
       r"dragon: the lever \(trigger 20, fired 1\): the hatchery \(seg 398 side 4\) is open, 4 hatchlings, the dragon attacks",
       r"message \"A heavy door grinds open far down the hall, and a deep laugh rolls through the corridor: \"Little fool!",
       r"dragon: obj 64 is hostile: the lever", r"obj 48 hatchlng: walks a path of \d+ segments to seg 430 for its script \(walk\)",
       r"f40 +wall 398\.4: type 2 flags 0x2 state", r"f42 +quest: level 21 .* gflags \[ (\d+ )*241 242 243 244 \]", r"f43 +obj 48: type 2 id 2 seg \d+ hp 50 ",
       r"f200 +wall 398\.4: type 2 flags 0x2 state", r"f201 +quest: level 21 .* gflags \[ (\d+ )*241 242 243 244 \]",
       r" 202 dragon: test 4: obj 64 Dragon hp 90 .* hostile 1 peaceful 0", r"f203 +obj 48: type 2 id 2 seg \d+ hp 50 "],
      [r"gflags \[[^\]]* 107 ", r"flapping of wings"], frames=220, extra=["-noattack", "-objlog", "dragon_lever_obj.log"]),
    # While she lives two hatchlings fight at one time: objects 48 and 49
    # leave the hatchery, 61 comes when 48 is dead, and all come when she
    # is dead.
    T("dragon_hatchlings", "DROW2", "6:goside=482.2,8:activate,60:obj=61,100:kill=48,160:obj=61,200:kill=64,260:obj=63",
      [r" (\d+) dragon: obj 48 hatchlng leaves the hatchery \(0 in the fight, the dragon lives\)",
       r"dragon: obj 49 hatchlng leaves the hatchery \(1 in the fight, the dragon lives\)",
       r"f60 +obj 61: type 2 id 2 seg 792 [\s\S]* 10\d dragon: obj 61 hatchlng leaves the hatchery \(1 in the fight, the dragon lives\)",
       r" 20\d dragon: obj 63 hatchlng leaves the hatchery \(2 in the fight, the dragon is dead\)",
       r"obj 63 hatchlng: walks a path of \d+ segments to seg 391 for its script \(walk\)"],
      frames=270, extra=["-noattack", "-objlog", "dragon_hatchlings_obj.log"]),
    # The hatchery before the lever (door wall 138, seg 398 side 4): the use
    # action finds it locked, no thief skill opens it (keys 4), and a
    # Fireball at the door from her room hurts no hatchling behind it. She
    # stays at peace.
    T("dragon_hatchery_shut", "DROW2", "5:class=3,6:skill=2.200,10:goside=398.4,12:activate,30:picklock,100:goseg=397,101:spellat=16.48,"
      "200:obj=48,201:obj=61,202:dragon=4,203:wall=398.4",
      [r"use: door wall 138 clip \d+ flags 0x18 keys 4 state 0: locked", r"thief: message 932 'Strong magic prevents you from picking this lock\.'",
       r"area of 'Fireball' at ", r"f200 +obj 48: type 2 id 2 seg 791 hp 50 ", r"f201 +obj 61: type 2 id 2 seg 792 hp 50 ",
       r" 202 dragon: test 4: obj 64 Dragon hp 90 .* hostile 0 peaceful 1", r"f203 +wall 398\.4: type 2 flags 0x18 state 0"],
      [r"leaves the hatchery", r"a hatchling is hurt"], frames=220, extra=["-noattack", "-triglog", "-objlog", "dragon_hatchery_shut_obj.log"]),
    # She fights already (no lever), then the lever: the same event.
    T("dragon_lever_in_fight", "DROW2", "10:dragon=5,30:goside=482.2,32:activate,100:quest",
      [r"dragon: obj 64 is hostile: test", r"dragon: the lever \(trigger 20, fired 1\): the hatchery \(seg 398 side 4\) is open, 4 hatchlings, the dragon attacks",
       r"dragon: obj 48 hatchlng leaves the hatchery \(0 in the fight, the dragon lives\)",
       r"dragon: obj 49 hatchlng leaves the hatchery \(1 in the fight, the dragon lives\)",
       r"f100 +quest: level 21 .* gflags \[ (\d+ )*241 242 243 244 \]"],
      [r"obj 61 hatchlng leaves", r"obj 63 hatchlng leaves"], frames=120, extra=["-noattack", "-objlog", "dragon_lever_in_fight_obj.log"]),
    # A hatchling that waits is not helpless. The player comes into the
    # hatchery: the two that wait join the fight (the flags 245 and 246).
    # After a save and a load the four flags are set and no hatchling leaves
    # a second time.
    T("dragon_hatchling_near", "DROW2", "6:goside=482.2,8:activate,60:quest,100:goseg=792,130:quest,140:save=3,170:load=3,260:quest",
      [r"f60 +quest: level 21 .* gflags \[ (\d+ )*241 242 243 244 \]",
       r"dragon: obj 61 hatchlng leaves the hatchery \(2 in the fight, the dragon lives, the player is near\)",
       r"dragon: obj 63 hatchlng leaves the hatchery \(3 in the fight, the dragon lives, the player is near\)",
       r"f130 +quest: level 21 .* gflags \[ (\d+ )*241 242 243 244 245 246 \]", r"f260 +quest: level 21 .* gflags \[ (\d+ )*241 242 243 244 245 246 \]"],
      [r" (1[7-9]\d|2\d\d) dragon: obj \d+ hatchlng leaves the hatchery"], frames=270, extra=["-noattack", "-objlog", "dragon_hatchling_near_obj.log"]),
    # A hatchling that waits and is hurt (an area spell into the hatchery)
    # joins the fight.
    T("dragon_hatchling_hurt", "DROW2", "6:goside=482.2,8:activate,60:obj=61,61:dragon=7,100:quest",
      [r"f60 +obj 61: type 2 id 2 seg 792 hp 50 ", r"dragon: obj 61 hatchlng leaves the hatchery \(2 in the fight, the dragon lives, it is hurt\)",
       r"f100 +quest: level 21 .* gflags \[ (\d+ )*241 242 243 244 245 \]"],
      [r"obj 63 hatchlng leaves"], frames=120, extra=["-noattack", "-objlog", "dragon_hatchling_hurt_obj.log"]),
    # A hatchling that is hurt behind the closed door before the lever: the
    # hatchery opens and she attacks.
    T("dragon_hatchery_breach", "DROW2", "30:dragon=7,80:wall=398.4,81:dragon=4,82:quest",
      [r"dragon: a hatchling is hurt behind the door of the hatchery: the hatchery is open, the dragon attacks",
       r"message \"Tamaramagil's roar shakes the hall: \"MY CHILDREN!", r"f80 +wall 398\.4: type 2 flags 0x2 state",
       r" 81 dragon: test 4: obj 64 Dragon .* hostile 1 peaceful 0", r"f82 +quest: level 21 .* gflags \[ (\d+ )*241 242 245 246 \]"],
      frames=100, extra=["-noattack", "-objlog", "dragon_hatchery_breach_obj.log"]),
    # Her attacks, with the fixed seed. Claws: two attack rolls, 1d10
    # each. Bite: one roll, 3d10. The reach is measured from the weapon box
    # of the hit frame (the claw) and from the tip of her snout (the bite).
    T("dragon_claws", "DROW2", "5:seed=1,6:goseg=470,7:goobj=64.36,8:face=64,10:dragon=1",
      [r"dragon: obj 64: idle -> claws, hp 90", r"dragon: claw 1 of 2 \(1d10\) at [\d.]+, reach 38\.25: (damage|no damage) \d+",
       r"dragon: claw 2 of 2 \(1d10\) at [\d.]+, reach 38\.25: (damage|no damage) \d+"],
      [r"out of reach"], frames=80, extra=["-objlog", "dragon_claws_obj.log"]),
    T("dragon_bite", "DROW2", "5:seed=1,6:goseg=470,7:goobj=64.40,8:face=64,10:dragon=2",
      [r"dragon: obj 64: idle -> bite, hp 90", r"dragon: bite 1 of 1 \(3d10\) at [\d.]+, reach 24\.50: (damage|no damage) \d+"],
      [r"out of reach"], frames=80, extra=["-objlog", "dragon_bite_obj.log"]),
    # The bite does not reach a player at her side; she turns first.
    T("dragon_bite_far", "DROW2", "5:seed=1,6:goseg=470,7:goobj=64.75,8:face=64,10:dragon=2",
      [r"dragon: bite: the player is out of reach \([\d.]+, reach 24\.50\)"], frames=80, extra=["-noattack", "-objlog", "dragon_bite_far_obj.log"]),
    # The breath: a wind-up of 2 seconds (the aim is fixed after 1 second),
    # then 12d10+6 of fire and a save against breath (category 3) for half.
    # Seed 1: the save fails. Seed 2: the save is made. Resist Fire (effect
    # bit 10, as the Ring of Coolness gives) halves what is left.
    T("dragon_breath_failed", "DROW2", "5:seed=1,6:goseg=470,7:goobj=64.55,8:face=64,10:dragon=3",
      [r"dragon: obj 64: idle -> breath wind-up", r"dragon: the aim of the breath is fixed at", r"save: obj 0 you, category 3: d20 \d+ against \d+: failed",
       r"dragon: breath on the player: 12d10\+6 = \d+, save against breath failed: \d+, after the fire protections: \d+"],
      frames=125, extra=["-objlog", "dragon_breath_failed_obj.log"]),
    T("dragon_breath_made", "DROW2", "5:seed=12,6:goseg=470,7:goobj=64.55,8:face=64,10:dragon=3",
      [r"save: obj 0 you, category 3: d20 \d+ against \d+: saved",
       r"dragon: breath on the player: 12d10\+6 = \d+, save against breath made: \d+, after the fire protections: \d+"],
      frames=125, extra=["-objlog", "dragon_breath_made_obj.log"]),
    T("dragon_breath_resist", "DROW2", "5:seed=1,6:goseg=470,7:goobj=64.55,8:face=64,9:fx=10.300,10:dragon=3",
      [r"dragon: breath on the player: 12d10\+6 = \d+, save against breath failed: \d+, after the fire protections: \d+"],
      frames=125, extra=["-objlog", "dragon_breath_resist_obj.log"]),
    # The player leaves the cone after the aim is fixed: no damage.
    T("dragon_breath_dodge", "DROW2", "5:seed=1,6:goseg=470,7:goobj=64.55,8:face=64,10:dragon=3,85:goseg=466",
      [r"dragon: the aim of the breath is fixed at", r"dragon: the breath misses the player"], [r"breath on the player"],
      frames=140, extra=["-objlog", "dragon_breath_dodge_obj.log"]),
    # The magic resistance: a d100 roll against 35 for each spell on her
    # (spelltest: 20 casts of Magic Missile at level 9).
    T("dragon_magic_resistance", "DROW2", "5:seed=1,10:goseg=470,11:goobj=64.50,12:face=64,20:spelltest=33.9.64.20",
      [r"dragon: magic resistance, spell 33 'Magic Missile' of obj 0: d100 \d+ against 35: resisted",
       r"dragon: magic resistance, spell 33 'Magic Missile' of obj 0: d100 \d+ against 35: not resisted"],
      frames=60, extra=["-noattack", "-objlog", "dragon_magic_resistance_obj.log"]),
    # Her death: 15000 experience, and the Dragon Key (MISC 42) of the door
    # of her hoard drops on the floor in front of her.
    T("dragon_death_key", "DROW2", "10:goseg=470,11:goobj=64.50,12:face=64,20:kill=64,150:obj=64",
      [r"xp: obj 64 Dragon gives 15000 experience", r"drop: obj 64 Dragon leaves type 16 id 42 count 1 \(obj \d+\) at 0\.00 -105\.00 -47\.50",
       r"f150 +obj 64: type 2 id 1 seg \d+ hp 0 ", r"message: You killed Tamaramagil: 15000 Exp\."], frames=160, extra=["-noattack", "-objlog", "dragon_death_key_obj.log"]),
    # A thief picks the lock of the door of the hoard (wall 136, seg 449
    # side 4) while she lives: she sees it, says her line and is hostile.
    T("dragon_thief", "DROW2", "5:class=3,6:skill=2.200,10:goside=449.4,11:wall=449.4,30:picklock,60:wall=449.4,61:dragon=4",
      [r"f11 +wall 449\.4: type 2 flags 0x18 state 0", r"thief: pick the lock of wall 136 \(seg 449 side 4\)",
       r"dragon: she sees the player open the door of her hoard", r"message \"Tamaramagil's roar shakes the hall: \"THIEF!",
       r"dragon: obj 64 is hostile: the door of the hoard", r" 61 dragon: test 4: obj 64 Dragon .* hostile 1 peaceful 0"],
      frames=80, extra=["-noattack", "-objlog", "dragon_thief_obj.log"]),
    # The treasure is placed one time: the items of the hoard are the same
    # after a save and a load (a level state has them).
    T("dragon_treasure_once", "DROW2", "30:items=784,31:items=409,32:items=407,40:save=3,60:load=3,150:items=784,151:items=409,152:items=407",
      [r"dragon: the key and the treasure are placed \(a new level\)",
       r"f30 +items in seg 784: (.*5\.68.*)\n[\s\S]*f150 +items in seg 784: \1\n", r"f31 +items in seg 409: (.*15\.17.*)\n[\s\S]*f151 +items in seg 409: \1\n",
       r"f32 +items in seg 407: (16\.43 )\n[\s\S]*f152 +items in seg 407: \1\n"],
      [r"placed \(a level state without them\)"], frames=170, extra=["-noattack", "-objlog", "dragon_treasure_once_obj.log"]),
    # With the setting off (-origtamaramagil, or the line of the
    # Improvements menu) the level is as 1.3 ships it: no dragon, no key, no
    # lock on the alcove, the hatchlings in her room, and the lever runs
    # script 213 (they leave, global flag 107).
    T("dragon_off", "DROW2", "30:dragon=4,31:wall=481.0,32:items=407,33:obj=48,40:goside=482.2,42:activate,100:quest",
      [r"dragon: test 4: no dragon", r"f31 +wall 481\.0: type 2 flags 0x10 state 0", r"f32 +items in seg 407: \n",
       r"f33 +obj 48: type 2 id 2 seg 430 ", r"flapping of wings", r"f100 +quest: level 21 .* gflags \[ 107 (\d+ )*\]"],
      [r"Jailer", r"dragon: obj 64"], frames=120, extra=["-noattack", "-origtamaramagil", "-objlog", "dragon_off_obj.log"]),
    # DROW3: command 14 at 285, command 7 (script 222) at 311.
    T("drow3_flag14", "DROW3", "40:goseg=285,60:quest", [r"quest: mflags \[ 14 \]"], frames=90),
    T("drow3_flag7", "DROW3", "20:killscript=222,40:goseg=311,60:quest",
      [r"quest: mflags \[ 7 \]"], frames=90),
    # DROW3: the lone mind flayer (object 128) carries the Scroll of the
    # Illithids and drops one at its death (Dtu_fix_illithid_scroll);
    # message 16 of trigger 27 (segment 711) says that he hides it.
    # -origillithidscroll: the scroll lies on the floor (object 129) with
    # the text number 18 in its count byte; Dtu_fix_plot_count gives one
    # copy, -origplotcount the 18 copies of 1.3.
    T("drow3_illithid_scroll", "DROW3", "10:goseg=711,60:kill=128,100:goobj=129,101:face=129,160:quest",
      [r"TRIG: say 16: .*he hides the scroll and attacks", r"drop: obj 128 flayer leaves type 16 id 126 count 1",
       r"pickup: obj 129 type 16 id 126 '[^']*' count 1,", r"quest: items \[ .*16\.126"],
      [r"drops the scroll"], frames=170,
      extra=["-noattack", "-triglog", "-testmove", "wait:102,forward:40", "-objlog", "drow3_scroll_obj.log"]),
    T("drow3_illithid_scroll_floor", "DROW3", "10:goseg=711,60:kill=128,100:goobj=129,101:face=129,160:quest",
      [r"TRIG: say 16: .*he drops the scroll and attacks", r"count 18 of the level file -> 1 \(Dtu_fix_plot_count\)",
       r"pickup: obj 129 type 16 id 126 '[^']*' count 1,"],
      [r"drop: obj 128 flayer leaves type 16"], frames=170,
      extra=["-noattack", "-triglog", "-origillithidscroll", "-testmove", "wait:102,forward:40", "-objlog", "drow3_scroll_floor_obj.log"]),
    T("drow3_illithid_scroll_orig", "DROW3", "10:goseg=711,60:kill=128,100:goobj=129,101:face=129,160:quest",
      [r"pickup: obj 129 type 16 id 126 '[^']*' count 18,"], [r"Dtu_fix_plot_count"], frames=170,
      extra=["-noattack", "-origillithidscroll", "-origplotcount", "-testmove", "wait:102,forward:40", "-objlog", "drow3_scroll_orig_obj.log"]),
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
    # FLAME: the level fixes give the altar (object 2) and the Flamesword
    # (object 3, script 322) object flag 0x40: the use ray passes them while
    # Lolth (object 36) lives. Her death (0x6A579, 0x6A374) clears the flag
    # of the sword, stops the creatures and runs script 322, which ends the
    # game with no use action. The use action after that also runs it.
    T("flame_sword_lolth_alive", "FLAME", "20:goseg=486,40:aim=3,45:activate,120:obj=3,121:obj=2",
      [r"obj 3: type 18 .* flags 0x40 ", r"obj 2: type 18 .* flags 0x40 "],
      [r"run script 322 of obj 3", r"CREDITV1"], frames=140),
    # The end sequence 0xA6786 before the credits: a light that grows by
    # 3.0 in each of 147 steps while the sword spins faster, then 50
    # explosions with 50 sounds (one step in each reference tick, 9.2 s).
    T("flame_lolth_death", "FLAME", "10:kill=36,15:obj=3",
      [r"obj 3: type 18 .* flags 0x40 ", r"run script 322 of obj 3 .* in mode 0", r"YOU  HAVE  DESTROYED",
       END_SEQ, r"end of the game: CREDITV1"], frames=600),
    # FLAME: the level fix 0xAD4FB gives object 35 (rffk9, the image of
    # Lolth that script 320 moves and that speaks) flag 0x40: the AI frame
    # (0x13C8B) and the move (0x9F174) skip it, and the use ray passes it.
    T("flame_lolth_image", "FLAME", "5:obj=35,10:goobj=35.12,300:obj=35,310:aim=35,312:activate,380:obj=35",
      [r"f5\s+obj 35: type 2 id 8 .* flags 0x40 .* pos -0\.2,46\.6,140\.1",
       r"f380\s+obj 35: type 2 id 8 .* var 0 .* pos -0\.2,46\.6,140\.1"],
      [r"run script 320 of obj 35"], frames=400),
    T("flame_ending", "FLAME", "10:kill=36,20:goseg=486,40:aim=3,45:activate",
      [r"run script 322 of obj 3", r"YOU  HAVE  DESTROYED", END_SEQ, r"end of the game: CREDITV1"], frames=600),

    # IBBALAR: the use action on the altar of Cyric (object 2, scenery,
    # script 294). The altar stands under the center of segment 424 and is
    # solid (Dtu_improved_physics), so the player stands 10.0 away from it.
    T("ibbalar_altar", "IBBALAR", "20:goseg=424,21:goobjh=2.10,60:aim=2,62:activate", [r"run script 294 of obj 2"],
      frames=120),

    # YPINN: the chairs with old script ids do not talk. The level start
    # clears the script word of all scenery on a fresh level (0xA674A at
    # 0xAD63B, D-074), as 1.3 does.
    T("ypinn_chair", "YPINN", "10:obj=5,10:obj=26,20:goobj=5,25:activate",
      [r"obj 5: type 18 .* script 0 ", r"obj 26: type 18 .* script 0 "], [r"run script 7 of obj 5"], frames=80),
    # YPINN: Mirt (object 2, 0xABB53) and a thief of level 1. D-090
    # (Dtu_fix_mirt_points): a thief of level 1 keeps the 60 points of the new
    # character at the first talk (the Player's Handbook value) and gets nothing
    # at the second. 1.3 (-origscript) sets the points to 40 at each talk,
    # since the screen is not shown in a hidden run and the level stays unpaid.
    T("ypinn_mirt_points", "YPINN", "5:class=3,10:goobj=2,15:talk=2,40:talk=2",
      [r"thief points: level 1, paid 0, 60 points \(added\)[\s\S]*thief points: level 1, paid 1, 60 points \(added\)"],
      frames=70),
    T("ypinn_mirt_points_orig", "YPINN", "5:class=3,10:goobj=2,15:talk=2,40:talk=2",
      [r"thief points: level 1, paid 0, 40 points\s[\s\S]*thief points: level 1, paid 0, 40 points\s"],
      [r"\(added\)"], frames=70, extra=["-origscript"]),

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
    # THIEF1: after the restore of its state, 0x5A855(0) switches trigger 34
    # off (0x5A94E, the `on` byte 34 * 0x59 + 0x0E; D-070). 1.3
    # (-origtrig) only: Dtu_fix_keep_triggers leaves it on.
    T("thief1_restore_rule", "THIEF1", "20:warp=7.612,60:warp=6.1,100:quest",
      [r"level rule of the restore \(0x5A855\), level 6", r"quest: level 6 "], frames=120,
      extra=["-triglog", "-statelog", "-origtrig"]),
    T("thief1_restore_keep", "THIEF1", "20:warp=7.612,60:warp=6.1,100:quest",
      [r"Restoring TEMP6\.LVL", r"quest: level 6 "], [r"level rule of the (load|restore) \(0x5A855\), level 6"], frames=120,
      extra=["-triglog", "-statelog"]),
    # THIEF1 trigger 34: the ladder of seg 281 side 4 heals 6 points (trap
    # 12, stat_delta 6). 1.3 switches the trigger off at each load (0x5A855
    # at 0x49C38); Dtu_fix_keep_triggers leaves it on.
    T("thief1_heal281", "THIEF1", "10:damage=8,20:goside=281.4,80:quest",
      [r"trap 12 runs: seg 281 side 4", r"quest: level 6 seg 281 "], frames=90,
      extra=["-triglog", "-testmove", "wait:22,forward:40"]),
    T("thief1_heal281_orig", "THIEF1", "10:damage=8,20:goside=281.4,80:quest",
      [r"level rule of the load \(0x5A855\), level 6", r"trigger 34 \(on=0 type=8", r"quest: level 6 seg 281 "],
      [r"trap 12 runs"], frames=90, extra=["-triglog", "-origtrig", "-testmove", "wait:22,forward:40"]),
    # KOBOLDS trigger 1: the message of seg 167 ("This room is strangely
    # quiet"). 1.3 switches it off at each load; Dtu_fix_keep_triggers
    # leaves it on.
    T("kobolds_msg167", "KOBOLDS", "20:goseg=167",
      [r"say 2: This room is strangely quiet"], [r"level rule of the load"], frames=40, extra=["-triglog"]),
    # THIEF1 trigger 20, the sign of seg 464 side 2 (mask 0xFD). 1.3 fires
    # it on each frame of a push into the wall (type 8); Dtu_fix_touch_use
    # leaves it to the use action.
    T("thief1_sign_push", "THIEF1", "20:goside=464.2",
      [r"quest: level|f\d+ "], [r"say 15: A sign"], frames=140,
      extra=["-triglog", "-testmove", "wait:22,forward:100"]),
    T("thief1_sign_push_orig", "THIEF1", "20:goside=464.2",
      [r"(say 15: A sign[^\n]*\n[\s\S]*){20}"], frames=140,
      extra=["-triglog", "-origtrig", "-testmove", "wait:22,forward:100"]),
    T("thief1_sign_use", "THIEF1", "20:goside=464.2,30:activate",
      [r"trigger 20 \(on=1 type=1 [^\n]*\n[^\n]*say 15: A sign"], frames=40, extra=["-triglog"]),
    # THIEF1 trigger 11, the lever of seg 90 side 2: only the use action
    # runs trap 7.
    T("thief1_lever_use", "THIEF1", "20:goside=90.2,22:move=7,30:activate",
      [r"trigger 11 \(on=1 type=1 [^\n]*\n[\s\S]*trap 7 runs"], [r"trigger 11 \(on=1 type=8"], frames=60,
      extra=["-triglog", "-testmove", "wait:40,forward:20"]),
    # BAT trigger 24, the Banegem at the symbol of Cyric: its place is the
    # center of side 3 of seg 85 (the wall) and its side number is 4 (the
    # ceiling). Dtu_fix_trigger_side: the use action on the wall fires it.
    T("bat_banegem_use", "BAT", "5:give=16.103,20:goside=85.3,30:activate,50:quest",
      [r"trigger 24 has the place of side 3 and the side number 4", r"take: 1 of entry \d+ 'Banegem'",
       r"f50 +quest: [^\n]*gflags \[[^\]]* 73 "], frames=70, extra=["-triglog"]),
    T("bat_banegem_use_orig", "BAT", "5:give=16.103,20:goside=85.3,30:activate,50:quest",
      [r"use: seg=85 side=3", r"f50 +quest: "], [r"take: 1 of entry \d+ 'Banegem'"], frames=70,
      extra=["-triglog", "-origtrig"]),
    # BANE, the symbol of Cyric of seg 585 side 4: Dtu_fix_bane_cyric shows
    # message 15 of BANE.SAY (no trigger has it) for the use action with
    # the Banegem. The gem stays.
    T("bane_cyric_gem", "BANE", "5:give=16.103,20:goside=585.4,30:activate,50:quest",
      [r"say 15: You remove the gem", r"f50 +quest: [^\n]*\n[^\n]*\n[^\n]*items \[[^\]]*16\.103"], frames=70,
      extra=["-triglog"]),
    T("bane_cyric_no_gem", "BANE", "20:goside=585.4,30:activate,50:quest",
      [r"use: seg=585 side=4"], [r"say 15:"], frames=70, extra=["-triglog"]),
    T("bane_cyric_gem_orig", "BANE", "5:give=16.103,20:goside=585.4,30:activate,50:quest",
      [r"use: seg=585 side=4"], [r"say 15:"], frames=70, extra=["-triglog", "-origtrig"]),
    # BAT trigger 10, the skull of seg 39 (no disable_trigger, a warp
    # command line in the segment). Dtu_fix_say_repeat: no message again
    # within 30 seconds after the player left the segment.
    T("bat_say_cooldown", "BAT",
      "20:goseg=39,40:goseg=42,60:goseg=39,80:goseg=42,100:goseg=39,1300:goseg=42,1320:goseg=39,1340:goseg=42,2400:goseg=39",
      [r"say 9: The skull[\s\S]*say 10: Two ghouls[\s\S]*no message: shown 0\.7 s ago[\s\S]*no message: shown 0\.7 s ago"
       r"[\s\S]*no message: shown 0\.7 s ago[\s\S]*say 9: The skull"],
      [r"(say 9: The skull[\s\S]*){3}", r"(say 10: Two ghouls[\s\S]*){2}"], frames=2420, extra=["-triglog", "-noattack"]),
    # CLAN3 triggers 1 and 2 (seg 9 and 10) gate three creatures with traps
    # of one charge. Dtu_fix_say_repeat: no message when the traps are used
    # up, also after a save and a load.
    T("clan3_say_spent", "CLAN3", "20:goseg=9,40:goseg=30,1200:goseg=10,1220:goseg=30,1240:save=3,1250:load=3,1280:goseg=9",
      [r"say 2: Ah, more dead things[\s\S]*no message: the traps are used up[\s\S]*no message: the traps are used up"],
      [r"(say 2: Ah, more dead things[\s\S]*){2}"], frames=1300, extra=["-triglog", "-noattack"]),
    T("kobolds_msg167_orig", "KOBOLDS", "20:goseg=167",
      [r"level rule of the load \(0x5A855\), level 2", r"trigger 1 \(on=0 type=8"], [r"say 2:"], frames=40,
      extra=["-triglog", "-origtrig"]),
    # THIEF2: the symbol of Mask (trigger 46, seg 266 side 0) opens the wall
    # of the cache (trap 18, seg 255 side 0). 1.3 (-origtrig) makes trigger
    # 46 a copy of trigger 43 at the load (0x494EB calls 0x5A855(1)): trap
    # 17 and the door of seg 607, the wall stays closed
    # (Dtu_fix_thief2_cache).
    T("thief2_mask_cache", "THIEF2", "20:goside=266.0,22:activate,40:wall=255.0",
      [r"trap 18 runs: seg 255 side 0", r"wall 255\.0: type 4 "], [r"level rule of thief2", r"level rule of the load"],
      frames=50, extra=["-triglog"]),
    T("thief2_mask_cache_orig", "THIEF2", "20:goside=266.0,22:activate,40:wall=255.0",
      [r"level rule of the load \(0x5A855\), level 7", r"trap 17 runs", r"level rule of thief2: trigger 46 opens the door of seg 607",
       r"wall 255\.0: type 5 "], [r"trap 18 runs"], frames=50, extra=["-triglog", "-origtrig"]),
    # The same after a restore of the state of THIEF2: the symbol still
    # opens the wall.
    T("thief2_mask_cache_restore", "THIEF2", "20:warp=6.1,60:warp=7.266,100:goside=266.0,102:activate,120:wall=255.0",
      [r"Restoring TEMP7\.LVL", r"trap 18 runs: seg 255 side 0", r"wall 255\.0: type 4 "], [r"level rule of thief2"],
      frames=130, extra=["-triglog", "-statelog"]),
    # The spell effects of the player (the bits +0x88 of the player object)
    # stay after a warp (0xBB563).
    T("warp_keeps_effects", "HUB1", "10:damage=5,12:fx=3.600,14:cast=20,20:quest,30:warp=3.376,60:quest",
      [r"quest: level 1 .* hp 7 fx 0x1008 [\s\S]*quest: level 3 .* hp 7 fx 0x1008 "], frames=70, invuln=False),

    # A warp (0x5A278, 0xAD9B2) sends a creature that overlaps the arrival
    # to its hide segment: KOBOLDS object 57 (seg 48, hide segment 0 of the
    # level file). Dtu_fix_warp_overlap tests the point of the arrival.
    T("warp_displace", "KOBOLDS", "5:obj=57,10:warp=2.48,11:obj=57",
      [r"f5\s+obj 57: type 2 id 1 seg 48 ", r"obj 57 kob_swrd overlaps the arrival in seg 48 and goes to seg 0",
       r"f11\s+obj 57: type 2 id 1 seg 0 "], frames=14, extra=["-objlog", "warp_displace_obj.log"]),
    # A cast that runs at a save: 1.3 writes the cast timer 0x153EBC into
    # the AI block of the save (0x16413) and then ends the running cast
    # (0x164B3); Dtu_fix_save_cast keeps it. The load gives the cast of the
    # save back (0x164C7).
    T("save_cast_runs_on", "KOBOLDS", "5:class=1,6:give=17.12.1,10:armspell=12,40:fire,42:save=3,80:fire",
      [r"the cast of 'Detect Magic' starts", r"'Detect Magic' \(12\) level 1 on obj 0"], [r"the save ends the running cast"],
      frames=90, extra=["-testslots", "9", "-objlog", "save_cast_obj.log"]),
    T("load_cast_goes_on", "KOBOLDS", "5:class=1,6:give=17.12.1,10:armspell=12,40:fire,42:save=3,44:load=3,120:fire",
      [r"the cast of 'Detect Magic' from the save goes on, the cast at 2\.300", r"'Detect Magic' \(12\) level 1 on obj 0"],
      frames=130, extra=["-testslots", "9", "-objlog", "load_cast_obj.log"]),
    # The talk of WEAPON 13 before a warp (0x59D2D): script 0x1FD runs.
    T("drowslayer_warp", "DROW1", "10:give=5.13,15:current=5.13,30:warp=20.1,31:quest,60:warp=21.1,100:quest",
      [r"warp with WEAPON 13 in the hand: script 0x1FD"], frames=120),

    # Save and load in the middle of a quest (the test commands save and
    # load are the keys Alt+F2 and Alt+F3 with no menu). The state after
    # the save is changed before the load, so the load must bring the saved
    # state back.
    # HUB1: the wall that the Violet Gem Key opened, flags and the mission.
    T("save_hub1_key", "HUB1",
      "10:give=16.107,12:gflag=67,13:mflag=3,14:mission=5,20:goseg=61,40:wall=40.4,41:quest,50:save=3,"
      "70:gflag=68,71:mflag=4,72:goseg=1,73:mission=9,80:quest,90:load=3,150:wall=40.4,151:quest",
      [r"quest: level 1 seg 1 mission 9 .* gflags \[ 67 68 \][\s\S]*"
       r"wall 40\.4: type 4[\s\S]*quest: level 1 seg 61 mission 5 .* gflags \[ 67 \]\n.*quest: mflags \[ 3 \]"],
      frames=160, extra=["-statelog"], also=[r"Restoring SG4_1\.LVL"]),
    # FLAME: three pieces are in the statue at the save. After the load the
    # other four are placed and script 323 opens the wall. The sign of the
    # script has a random branch: seed=1 fixes the random numbers before it.
    T("save_flame_statue", "FLAME",
      "10:give=16.18,10:give=16.19,10:give=16.20,10:give=16.22,10:give=16.23,10:give=16.24,10:give=16.26,"
      "20:goside=229.4,22:activate,40:goside=245.4,42:activate,60:goside=275.4,62:activate,70:quest,75:save=2,"
      "80:give=16.18,81:goseg=2,85:load=2,140:quest,"
      "160:goside=287.4,162:activate,180:goside=266.4,182:activate,200:goside=258.4,202:activate,"
      "220:goside=284.4,222:activate,240:wall=278.0,250:gflag=128,260:goside=279.5,261:seed=1,262:activate,640:wall=278.0",
      [r"quest: items \[ 16\.102 5\.20 15\.1 16\.2 16\.79 16\.20 16\.22 16\.23 16\.26 \][\s\S]*"
       r"quest: items \[ 16\.102 5\.20 15\.1 16\.2 16\.79 16\.20 16\.22 16\.23 16\.26 \][\s\S]*"
       r"wall 278\.0: type 5[\s\S]*wall 278\.0: type 4"],
      frames=660, extra=["-triglog", "-statelog"], also=[r"Restoring SG3_32\.LVL"]),
    # KOBOLDS: the chief (script 23) is dead at the save; after the load
    # command 1 of DTU.WRP still finds no creature with script 23.
    T("save_kobolds_chief", "KOBOLDS",
      "20:killscript=23,30:obj=19,40:save=1,50:goseg=6,60:quest,70:load=1,120:quest,130:goseg=6,150:quest",
      [r"quest: mflags \[ 1 \][\s\S]*quest: mflags \[ \][\s\S]*quest: mflags \[ 1 \]"],
      frames=160, extra=["-statelog"], also=[r"Restoring SG2_2\.LVL"]),
    # YPINN: the script state of Durnan (object 1: talk counter, resume
    # label, flags, variable) is in the level state.
    T("save_durnan_talk", "YPINN",
      "20:talk=1,200:obj=1,210:save=4,220:talk=1,400:obj=1,410:load=4,470:obj=1",
      [r"obj 1: .* talks 1 [\s\S]*obj 1: .* talks 2 [\s\S]*obj 1: .* talks 1 "],
      frames=480, dlg=",".join(["esc"] * 40), extra=["-statelog"], also=[r"Restoring SG5_0\.LVL"]),
    # THIEF1 and THIEF2: the prison sequence, a save in THIEF2, a load, and
    # the way back: the chest of THIEF1 (object 99) or the floor still has
    # what the thieves took (the level state of THIEF1 is in the slot). The
    # sequence puts each item by a coin (0xAA448): seed=2 puts the Scroll on
    # the floor of segment 297.
    T("save_prison", "THIEF1",
      "10:gold=500,19:seed=2,20:prison,60:quest,65:save=5,70:gold=77,75:give=16.2,80:load=5,140:quest,150:warp=6.297,190:quest,191:items=297",
      [r"prison: type 16 id 102 lies in seg 297[\s\S]*quest: level 7 seg 612 .* gold 0 hp 1 [\s\S]*"
       r"quest: level 7 seg 612 .* gold 0 hp 1 .*\n.*entries 0[\s\S]*quest: level 6 seg 297 [\s\S]*items in seg 297: .*16\.102"],
      frames=200, invuln=False, extra=["-statelog"], also=[r"Restoring SG6_7\.LVL"]),

    # MULHOR1 and BONES: the Ankh (MISC 14) of the skeleton priest Straum
    # (script 31, object 55). The Ankh is object 33 of MULHOR1 (segment
    # 585); the player takes it by walking into it. Straum takes it at
    # label 700 and Thoth grants a favor (reply 5, label 750: 8000 gold),
    # then 600 experience at label 910. Flag 1 (the promise of label 600)
    # makes Straum wary: a talk starts only when a roll of 1 to 20 is not
    # above the charisma, so the test talks six times.
    T("mulhor1_ankh", "MULHOR1", "20:goobj=33,21:face=33,22:obj=33,100:quest",
      [r"obj 33: type 16 id 14 .* flags 0x0 ", r"quest: items \[ .*16\.14 \]"], frames=110,
      extra=["-testmove", "wait:25,forward:60"]),
    # The level fixes (0xAD0C0) give the other six Ankhs (1, 2, 34, 60, 73,
    # 91) object flag 0x40: the object query (0x3F38F) skips them, so the
    # player walks through them and does not take them. They stay drawn.
    T("mulhor1_ankh_flagged", "MULHOR1",
      "20:goobj=91,21:face=91,22:obj=91,23:obj=1,24:obj=2,25:obj=34,26:obj=60,27:obj=73,100:quest,101:obj=91",
      [r"obj %d: type 16 id 14 .* flags 0x40 " % n for n in (91, 1, 2, 34, 60, 73)]
      + [r"quest: items \[ [^\]]*\]\n[\s\S]*obj 91: type 16 id 14 .* flags 0x40 "],
      [r"quest: items \[ .*16\.14 \]", r"pickup: obj 91"], frames=110,
      extra=["-testmove", "wait:25,forward:60"]),
    T("bones_straum_ankh", "BONES",
      "10:give=16.14,12:kill=106,14:objflag=55.1,15:resume=55.700,20:talkscript=31,40:talkscript=31,60:talkscript=31,"
      "80:talkscript=31,100:talkscript=31,120:talkscript=31,1780:quest,1781:obj=55",
      [r"run script 31 of obj 55 .* at label 2BC", r"take class 16 id 14", r"gold 100 -> 8100", r"experience: \+600 ",
       r"quest: level 3 .* gold 8100 ", r"death: obj 55, attacker none, xp -1", r"obj 55: .* hp 0 flags 0x4 .* anim 9"],
      frames=1800, dlg="1,1,1,5," + ONES),
    # Label 910 ends with statement BC (ANIM 9): the hit points of Straum
    # become 0, and the end of the run kills him (0xAC033: 0xA6ACB sets the
    # hit points to 1.0 and does 1.0 damage). He stays dead after a save
    # and a load, and the talk does not start again.
    T("bones_straum_dead_saveload", "BONES",
      "10:give=16.14,12:kill=106,14:objflag=55.1,15:resume=55.700,20:talkscript=31,40:talkscript=31,60:talkscript=31,"
      "80:talkscript=31,100:talkscript=31,120:talkscript=31,1700:save=3,1710:load=3,1750:talkscript=31,1780:quest,1781:obj=55",
      [r"death: obj 55, attacker none, xp -1", r"quest: level 3 .* gold 8100 ",
       r"f1781 +obj 55: .* hp 0 flags 0x4 .* anim 9"],
      [r"run script 31 of obj 55 .* in mode 0 at label (?!2BC)"],
      frames=1800, dlg="1,1,1,5," + ONES),

    # Area spells hit the caster in the area: 0x90CEF has no test of the
    # caster in its object loop (0x90D54 to 0x90DDA, from object 0), and
    # 0x8CFFF has none either. A level 9 Fireball of the player on the
    # ceiling of BONES segment 48 (15 units away) burns the player, one on
    # a zombie 80 units away does not (radius 40). The cast starts at the
    # start point, away from the rats, whose hits would stop it.
    T("fireball_caster_in_area", "BONES",
      "5:class=1,6:setlevel=9,7:give=17.16.1,8:armspell=16,9:heal=100,22:fire,140:goside=48.1,142:fire",
      [r"the area spell 16 of obj \d+ hits a wall", r"'Fireball' level 9 on obj 0 you: damage \d+"], frames=190,
      extra=["-testslots", "9", "-objlog", "fireball_near_obj.log"]),
    T("fireball_caster_out_of_area", "BONES",
      "5:class=1,6:setlevel=9,7:give=17.16.1,8:armspell=16,9:heal=100,20:goobj=11.80,22:aim=11,25:fire,118:aim=11,120:fire",
      [r"area of 'Fireball'", r"'Fireball' level 9 on obj 11 Zombie: damage"], [r"'Fireball' level 9 on obj 0 you"],
      frames=160, extra=["-testslots", "9", "-objlog", "fireball_far_obj.log"]),
    # Lightning Bolt goes on through each body on its line until its life ends
    # (Dtu_fix_bolt_pass, D-195; AD&D 2e). BONES: three zombies 20, 40 and 60
    # in front of the player (the test command objfront) take one level 9
    # bolt of the player, each once with its own save. 1.3 (-origcombat): the
    # first zombie ends it (0x272DD).
    T("bolt_line_three", "BONES",
      "2:seed=1,5:class=1,6:setlevel=9,7:give=17.32.1,8:armspell=32,9:heal=100,20:fire,"
      "198:objfront=4.20,198:objfront=11.40,198:objfront=23.60,200:fire",
      [r"save: obj 4 Zombie, category 4[\s\S]*'Lightning Bolt' level 9 on obj 4 Zombie: damage",
       r"save: obj 11 Zombie, category 4[\s\S]*'Lightning Bolt' level 9 on obj 11 Zombie: damage",
       r"save: obj 23 Zombie, category 4[\s\S]*'Lightning Bolt' level 9 on obj 23 Zombie: damage",
       r"crosses obj 4 Zombie \(leg 0\)", r"crosses obj 11 Zombie \(leg 0\)", r"crosses obj 23 Zombie \(leg 0\)"],
      [r"on obj (4|11|23) Zombie: damage[\s\S]*on obj \1 Zombie: damage"],
      frames=260, extra=["-testslots", "9", "-noattack", "-objlog", "bolt_line_obj.log"]),
    T("bolt_line_three_orig", "BONES",
      "2:seed=1,5:class=1,6:setlevel=9,7:give=17.32.1,8:armspell=32,9:heal=100,20:fire,"
      "198:objfront=4.20,198:objfront=11.40,198:objfront=23.60,200:fire",
      [r"'Lightning Bolt' level 9 on obj 4 Zombie: damage"],
      [r"on obj (11|23) Zombie: damage", r"Lightning Bolt obj \d+ crosses"],
      frames=260, extra=["-origcombat", "-testslots", "9", "-noattack", "-objlog", "bolt_line_orig_obj.log"]),
    # The same bolt flies on: it comes back from the wall and crosses the
    # player, who makes the save (half); after the wall behind the player it
    # crosses the player again: a new save, which fails, gives the other half
    # (AD&D 2e: full damage when one save fails).
    T("bolt_cross_again", "BONES",
      "2:seed=1,5:class=1,6:setlevel=9,7:give=17.32.1,8:armspell=32,9:heal=100,20:fire,"
      "198:objfront=4.20,198:objfront=11.40,198:objfront=23.60,200:fire",
      [r"crosses obj 0 player \(leg 1\): damage roll (\d+), save made[\s\S]*'Lightning Bolt' crosses obj 0 you again: "
       r"the save fails, the other \d+ of the roll \1"],
      frames=500, invuln=False, extra=["-testslots", "9", "-noattack", "-objlog", "bolt_again_obj.log"]),
    # The bolt of the player into the ceiling of BONES segment 48 (15 units
    # away) comes back and hits the player, with the save and the damage; the
    # same with the Wand of Lightning Bolts (weapon 59).
    T("bolt_near_wall_self", "BONES",
      "2:seed=1,5:class=1,6:setlevel=9,7:give=17.32.1,8:armspell=32,9:heal=100,20:goside=48.1,22:fire,130:fire",
      [r"Lightning Bolt obj \d+ bounces", r"'Lightning Bolt' \(32\) level 9 on obj 0 you", r"save: obj 0 you, category 4",
       r"damage: the player takes \d+ from player", r"crosses obj 0 player \(leg [1-9]\)"],
      frames=200, invuln=False, extra=["-testslots", "9", "-noattack", "-objlog", "bolt_wall_obj.log"]),
    T("bolt_wand_near_wall_self", "BONES",
      "2:seed=1,5:class=1,6:setlevel=9,7:give=5.59.5,8:current=5.59,9:heal=100,20:goside=48.1,22:fire",
      [r"casts 'Lightning Bolt' \(32\) at level 9, source 2", r"Lightning Bolt obj \d+ bounces",
       r"save: obj 0 you, category 4", r"damage: the player takes \d+ from player", r"crosses obj 0 player \(leg [1-9]\)"],
      frames=120, invuln=False, extra=["-noattack", "-objlog", "bolt_wand_obj.log"]),
    # A Lightning Bolt of the zombie obj 11 (40 in front of the player) at the
    # player passes its ally obj 4 (20 in front) with no harm (Dtu_caster_ai),
    # hits the player, comes back from the wall behind the player and hits
    # its caster. With a Minor Globe the player is not harmed and the bolt
    # goes on.
    T("bolt_creature_ally", "BONES", "2:seed=1,9:heal=100,20:objfront=4.20,20:objfront=11.40,22:spellby=32.11",
      [r"crosses obj 0 player \(leg 0\)", r"damage: the player takes \d+ from Zombie",
       r"Lightning Bolt obj \d+ bounces", r"'Lightning Bolt' level 2 on obj 11 Zombie: damage"],
      [r"'Lightning Bolt' level 2 on obj 4 Zombie: damage"],
      frames=200, invuln=False, extra=["-noattack", "-objlog", "bolt_ally_obj.log"]),
    T("bolt_creature_globe", "BONES", "2:seed=1,9:heal=100,10:cast=57,20:objfront=4.20,20:objfront=11.40,22:spellby=32.11",
      [r"Minor Globe of Invulnerability of obj 0 you stops 'Lightning Bolt'", r"'Lightning Bolt' level 2 on obj 11 Zombie: damage"],
      [r"damage: the player takes \d+ from Zombie"],
      frames=200, invuln=False, extra=["-testslots", "9", "-noattack", "-objlog", "bolt_globe_obj.log"]),
    # The Mirror Shield (armor 53) sends the bolt of the zombie back: it is a
    # bolt of the player now and crosses the ally obj 4 and the caster.
    T("bolt_creature_mirror", "BONES",
      "2:seed=1,9:heal=100,10:give=15.53.1,11:equip=15.53,20:objfront=4.20,20:objfront=11.40,21:face=11,22:spellby=32.11",
      [r"Mirror Shield sends obj \d+ \(weapon \d+ spell 32\) back to obj 11", r"crosses obj 4 Zombie \(leg 1\)",
       r"'Lightning Bolt' level 2 on obj 11 Zombie: damage"],
      frames=60, invuln=False, extra=["-noattack", "-objlog", "bolt_mirror_obj.log"]),

    # Statement EB (script 291 of IBBALAR, label 23): the area spells go off
    # at the segments that the statement names (D-075, Dtu_fix_script_area_seg).
    T("script_area_segment", "IBBALAR", "10:resume=82.23,15:talkscript=291",
      [r"area spell 4 at the center of segment 231", r"area of 'Burning Hands'"], frames=120,
      extra=["-objlog", "eb_obj.log"]),

    # Statement 9F (script 31 of BONES): the level.
    T("level_statement", "BONES", "20:setlevel=5,30:dump", [r"script: setlevel 5", r"level 5/5"], frames=40),

    # ASCENT: the kill line with the experience (Dtu_kill_xp_message,
    # engine/main/dtuxp.h). The Ghoul (obj 34 of BAT) gives score_value 175;
    # a character with two classes gets one half in each class (the session
    # log); the message log shows the total only.
    T("kill_xp_line", "BAT", "20:kill=34",
      [r"message: You killed Ghoul: 175 Exp\.\n", r"experience: \+(175|87) for each class"], frames=40),
    T("kill_xp_line_orig", "BAT", "20:kill=34", [r"message: You killed Ghoul\.\n"],
      [r"message: You killed Ghoul:"], frames=40, extra=["-nokillxpmessage"]),
    # A death by another cause that the player sees gives no experience and
    # says so: a creature kills a creature next to the player, and the kill
    # statement of a script (0xA6ACB, killscript=0: every creature with no
    # script) kills the creature in front of the player.
    T("kill_by_creature_no_xp", "BAT", "20:goobj=43.12,25:killby=43.44",
      [r"death: obj 43, attacker obj 44 gskelpus, xp -1, seen by the player 1",
       r"message: .+ killed .+: no experience\."], [r"experience: \+"], frames=40),
    T("kill_by_script_no_xp", "BAT", "20:goobj=43.12,25:killscript=0",
      [r"death: obj 43, attacker none, xp -1, seen by the player 1", r"message: .+ died: no experience\."],
      [r"experience: \+"], frames=40),

    # ASCENT: Maximum HP on level up (Dtu_max_hp_level_up, dtuxp.h): with
    # the setting each level that rolls its hit die gets the full die; off,
    # the roll of 0xAF694 (one die, at least one half). check_hit_dice
    # tests each line.
    T("max_hp_level_up_on", "BAT", "20:xp=60000", [r"level: class \d level \d+: roll"], frames=40,
      extra=["-maxhplevelup"], also=[]),
    T("max_hp_level_up_off", "BAT", "20:xp=60000", [r"level: class \d level \d+: roll"], frames=40,
      extra=["-nomaxhplevelup"]),

    # ASCENT: the verbose combat log (Dtu_verbose_combat, dtuxp.h): the
    # attacks of a skeleton on the player and the swings of the player at
    # it. check_rolls tests the numbers of each line.
    T("verbose_combat_on", "BAT",
      "15:goobj=43,17:heal=99," + ",".join("%d:attackedby=43" % f for f in range(20, 40, 2)) + ","
      + ",".join("%d:swingat=43" % f for f in range(40, 70, 2)),
      [r"message: .+ -> .+: Roll d20 \d+"], frames=80, extra=["-verbosecombat"]),
    T("verbose_combat_off", "BAT",
      "15:goobj=43,17:heal=99," + ",".join("%d:attackedby=43" % f for f in range(20, 40, 2)) + ","
      + ",".join("%d:swingat=43" % f for f in range(40, 70, 2)),
      [r"attack roll: d20"], [r"message: .*Roll d20", r"message: .*Damage"], frames=80, extra=["-noverbosecombat"]),

    # ASCENT: the new gear (Dtu_new_gear, dtugear.h). MYRKUL: trigger 31 at
    # segment 460 takes the Potion of Dragon's Blood (misc 74) and
    # consecrates the altar of Kelemvor; Daland (object 3, script 160) then
    # gives the Potion of Extra-Healing (misc 75), 600 experience, and with
    # the setting the Mace of Disruption (weapon 70). -nonewgear: no mace.
    # The level fix 0xAD129 holds Daland with no end, and a held creature
    # cannot talk (0xABBE5 tests +0x89 bit 0x40): the talk shows the hint of
    # Dispel Magic, a Dispel Magic of the player (area=13.3, the spell of the
    # player at Daland) ends the Hold and leaves him peaceful
    # (Dtu_fix_dispel_peace). The first talk after it: "You did not have
    # time"; the second gives the quest and misc 74 (talks counts from 0).
    T("myrkul_daland_mace", "MYRKUL", "8:goobj=3.20,10:face=3,12:talk=3,14:area=13.3,20:talk=3,150:talk=3,300:goseg=460,"
      "330:goobj=3.20,332:face=3,334:talk=3,680:dump",
      [r"obj 3: held with no end, the hint of Dispel Magic", r"effect Hold Person of obj 3 daland ends",
       r"run script 160 of obj 3", r"give class 16 id 75", r"You receive 600 xp",
       r"entry \d+: type 5 id 70 'Mace of Disruption'", r"new gear: script 160 gives weapon 70"],
      [r"obj 3 daland: woken, hostile 1", r"obj 3: status 3EA"],
      frames=700, dlg=DALAND_DLG, extra=["-noattack", "-objlog", "myrkul_daland_obj.log"], known=MYRKUL_INT3),
    T("myrkul_daland_no_mace", "MYRKUL", "8:goobj=3.20,10:face=3,12:talk=3,14:area=13.3,20:talk=3,150:talk=3,300:goseg=460,"
      "330:goobj=3.20,332:face=3,334:talk=3,680:dump",
      [r"give class 16 id 75"], [r"type 5 id 70 "], frames=700, dlg=DALAND_DLG, extra=["-noattack", "-nonewgear"], known=MYRKUL_INT3),

    # ASCENT: the phylactery of the lich of CLAN3 (Dtu_lich_phylactery,
    # engine/main/dtulich.c, docs/re/lich.md). The altar of Velsharoon is
    # object 61 in the cell of segment 557, the Dispel Magic scroll (spell
    # 55) object 62. LICH_FIGHT makes Morik Stormhand (object 10) the only
    # creature and starts a fight with him as tools/check_caster_ai.py
    # does. "phylactery=N" is the test command of dtulich.h: 0 breaks it,
    # 1 logs its state.
    T("lich_phylactery_whole", "CLAN3", LICH_FIGHT + ",40:phylactery=1",
      [r"lich: the phylactery is obj 61 in segment 557 at 289\.50 6\.00 305\.00",
       r"lich: the scroll of Dispel Magic \(spell 55\) is obj 62 in segment 557", LICH_FULL,
       r"lich: test: phylactery in the level, placed 1, broken 0, on 1, lich alive"],
      frames=60, extra=["-objlog", "lich_whole_obj.log"]),
    # Broken before the fight: no contingency
    T("lich_phylactery_before", "CLAN3", "3:phylactery=0," + LICH_FIGHT,
      [r"lich: obj 0 breaks the phylactery obj 61; the lich has no protections now",
       r"lich: message \"The altar of Velsharoon cracks apart\. A thin scream rises",
       r"caster: obj 10 Lich3 notices the player .*contingency: none"], [LICH_FULL],
      frames=60, extra=["-objlog", "lich_before_obj.log"]),
    # Broken during the fight: Stone Skin, Shield, the images and the globe
    # end at once, and the effect word of the lich is empty
    T("lich_phylactery_fight", "CLAN3", LICH_FIGHT + ",60:fxobj=10,70:phylactery=0,80:fxobj=10",
      [LICH_FULL, r"f60 +fx: obj 10 bits 0x22000000", r"lich: obj 0 breaks the phylactery obj 61; the lich loses his protections",
       r"effect Stoneskin of obj 10 Lich3 taken away", r"Mirror Image of obj 10 Lich3 ends",
       r"Minor Globe of Invulnerability of obj 10 Lich3 ends", r"lich: message \"The altar of Velsharoon cracks apart\. With a howl",
       r"f80 +fx: obj 10 bits 0x00000000"],
      frames=90, extra=["-objlog", "lich_fight_obj.log"]),
    # One blow of the attack key breaks it (the scenery rule of dtucombat.c)
    T("lich_phylactery_blow", "CLAN3", "80:phylactery=1",
      [r"combat: the weapon hits scenery obj 61 an altar of Velsharoon", r"lich: obj 0 breaks the phylactery obj 61",
       r"lich: test: phylactery not in the level, placed 1, broken 1"],
      frames=90, extra=["-objlog", "lich_blow_obj.log", "-testpos", "299,10,305,270,20", "-testmove", "wait:30,attack:40,wait:20"]),
    # A save and a load: the flags 247 (broken) and 248 (placed) stay, the
    # level state has no altar, no new one is made, and the lich starts his
    # fight with no contingency. A whole altar stays one altar.
    T("lich_phylactery_save_load", "CLAN3", "3:phylactery=0,30:save=3,60:load=3,100:phylactery=1,101:quest,"
      "102:alone=10,103:setlevel=8,105:heal=500,110:goobjsee=10.20,115:spelltest=28.1.10.0",
      [r"lich: test: phylactery not in the level, placed 1, broken 1, on 1, lich alive",
       r"f101 +quest: level 19 .* gflags \[ (\d+ )*247 248 \]", r"caster: obj 10 Lich3 notices the player .*contingency: none"],
      frames=140, extra=["-objlog", "lich_save_obj.log"]),
    T("lich_phylactery_load_whole", "CLAN3", "30:save=3,60:load=3,100:phylactery=1",
      [r"lich: test: phylactery in the level, placed 1, broken 0", r"lich: test: phylactery obj 61 seg 557"],
      [r"(?s)lich: the phylactery is obj.*lich: the phylactery is obj"], frames=110, extra=["-objlog", "lich_load_obj.log"]),
    # The setting off (-nophylactery): no altar, the full contingency
    T("lich_phylactery_off", "CLAN3", LICH_FIGHT + ",40:phylactery=1",
      [LICH_FULL, r"lich: test: phylactery not in the level, placed 0, broken 0, on 0"], [r"lich: the phylactery is obj"],
      frames=60, extra=["-objlog", "lich_off_obj.log", "-nophylactery"]),
    # A fighter (the default character) and a thief read the Dispel Magic
    # scroll from a hot key box: the lich loses his protections. The paper
    # doll does not take a spell item of another class (0x3533C), the hot
    # key box does (dtuchar.c, dtu_char_set_hot_key).
    T("lich_dispel_scroll_fighter", "CLAN3", LICH_FIGHT + ",40:give=17.55.1,42:sethot=3.17.55,44:presshot=3,50:fire,80:fxobj=10",
      [LICH_FULL, r"the player casts 'Dispel Magic' \(55\)", r"'Dispel Magic' \(55\) level \d+ on obj 10 Lich3", r"f80 +fx: obj 10 bits 0x00000000"],
      frames=90, extra=["-objlog", "lich_scroll_fighter_obj.log"]),
    T("lich_dispel_scroll_thief", "CLAN3", "1:class=3," + LICH_FIGHT + ",40:give=17.55.1,42:sethot=3.17.55,44:presshot=3,50:fire,80:fxobj=10",
      [LICH_FULL, r"the player casts 'Dispel Magic' \(55\)", r"'Dispel Magic' \(55\) level \d+ on obj 10 Lich3", r"f80 +fx: obj 10 bits 0x00000000"],
      frames=90, extra=["-objlog", "lich_scroll_thief_obj.log"]),
    # The texts: Barander (CLAN1 object 141, script 170) tells of the
    # phylactery in his answer to "How can he be stopped?" (label 12; the
    # replies 4, 1, 1 after his greeting); CLAN3 message 1 (trigger 0, seg
    # 26) points to the east crypts; after the break the lich answers "Die,
    # lich!" (reply 3, label 4) with the line of the port.
    T("lich_barander_hint", "CLAN1", "20:talkscript=170",
      [r"reply 1 -> label C\b", r"the text of the port for gab\.170\.3845",
       r"dlg: speech \"Not by steel alone\. A lich binds his life to a phylactery"],
      frames=900, dlg="1,4,1,1,1"),
    T("lich_arrival_message", "CLAN3", "20:goseg=27,40:goseg=26",
      [r"lich: the text of the port for message 1 of CLAN3"], frames=60, extra=["-objlog", "lich_message_obj.log"]),
    T("lich_talk_after_break", "CLAN3", "6:phylactery=0,20:talkscript=190",
      [r"reply 3 -> label 4\b", r"dlg: speech \"So you found my altar and broke it"],
      frames=300, dlg="3,1,1"),
    # The Rod of Dispel Magic of the new gear (weapon 67), fired at Daland
    # with the attack key: its missile frees him and he stays peaceful.
    T("myrkul_daland_rod", "MYRKUL", "5:class=1,6:give=5.67.6,7:current=5.67,8:goobj=3.20,10:aim=3,80:talk=3,82:talk=3",
      [r"casts 'Dispel Magic' \(13\) at level \d+, source 2", r"effect Hold Person of obj 3 daland ends",
       r"Hail, wanderer of these fell halls"],
      [r"obj 3 daland: woken, hostile 1", r"obj 3: status 3EA"],
      frames=200, extra=["-noattack", "-objlog", "myrkul_daland_rod_obj.log", "-testmove", "wait:30,attack:1,wait:100"],
      known=MYRKUL_INT3),
    # -origcombat (Dtu_fix_dispel_peace 0): the Dispel Magic makes him
    # hostile, and he does not talk
    T("myrkul_daland_orig", "MYRKUL", "8:goobj=3.20,10:face=3,12:talk=3,14:area=13.3,20:talk=3",
      [r"obj 3: status 3EA"], [r"held with no end, the hint", r"run script 160"],
      frames=60, dlg=DALAND_DLG, extra=["-noattack", "-origcombat"], known=MYRKUL_INT3),
]

DIE = {0: 8, 1: 4, 2: 10, 3: 6}       # 0x13FDF3: cleric, mage, fighter, thief


def check_hit_dice(name, text):
    """The hit die lines of the level gains (dtuchar.c, level_hit_points)."""
    problems = []
    rows = re.findall(r"level: class (\d) level (\d+): roll (\d+) \(die (\d+)\)", text)
    if not rows:
        return ["no hit die roll"]
    for cls, lvl, n, die in rows:
        n, die = int(n), int(die)
        if die != DIE[int(cls)]:
            problems.append("class %s die %d" % (cls, die))
        if name.endswith("_on") and n != die:
            problems.append("class %s level %s: %d, not the full die %d" % (cls, lvl, n, die))
        if name.endswith("_off") and not (die >> 1) <= n <= die:
            problems.append("class %s level %s: %d out of %d..%d" % (cls, lvl, n, die >> 1, die))
    # the sum of the hit points of the levels, halved for two classes
    return problems


def check_rolls(name, text):
    """Each verbose line: the d20 from 1 to 20, the total the d20 plus the
    listed modifiers, the damage total the dice plus the listed bonuses."""
    problems = []
    lines = re.findall(r"message: (.*?Roll d20 .*)", text)
    if not lines:
        return ["no verbose line"]
    for ln in lines:
        # "attacker -> target: " before the numbers (dtu_xp_line_say; the
        # player is the name of the character, or "You" with no character)
        w = re.match(r"(.+?) -> (.+?): (Roll d20 .*)", ln)
        if not w or w.group(1) == w.group(2):
            problems.append("no attacker and target: " + ln)
            continue
        ln = w.group(3)
        m = re.match(r"Roll d20 (\d+)(?::|(.*?) = (-?\d+) vs (-?\d+):)", ln)
        if not m:
            problems.append("cannot read: " + ln)
            continue
        d20 = int(m.group(1))
        if not 1 <= d20 <= 20:
            problems.append("d20 %d: %s" % (d20, ln))
        if m.group(3) is not None:
            mods = sum(int(v) for v in re.findall(r"([+-]\d+)", m.group(2) or ""))
            if d20 + mods != int(m.group(3)):
                problems.append("total: " + ln)
        d = re.search(r"Damage (.*) = (-?\d+)\.", ln)
        if d:
            parts = re.findall(r"(?:^| )(?:\d+d\d+ (\d+)|([+-]\d+)|(\d+))(?= |$)", d.group(1).replace(" + ", " "))
            total = 0
            for dice, mod, plain in parts:
                total += int(dice or mod or plain)
            if "x" not in d.group(1) and "/" not in d.group(1) and total != int(d.group(2)):
                problems.append("damage: " + ln)
    return problems


def check_breath(name, text):
    """The breath of Tamaramagil (dtudragon.c): 12d10+6 is 18 to 126, a made
    save halves it (rounded down), Resist Fire halves what is left."""
    m = re.search(r"dragon: breath on the player: 12d10\+6 = (\d+), save against breath (made|failed): (\d+), "
                  r"after the fire protections: (\d+)", text)
    if not m:
        return ["no breath line"]
    roll, saved, dmg, after = int(m.group(1)), m.group(2) == "made", int(m.group(3)), int(m.group(4))
    problems = []
    if not 18 <= roll <= 126:
        problems.append("roll %d" % roll)
    if dmg != (roll // 2 if saved else roll):
        problems.append("the save: %d of %d" % (dmg, roll))
    if saved != (name == "dragon_breath_made"):
        problems.append("the save is %s" % m.group(2))
    if after != (dmg // 2 if name == "dragon_breath_resist" else dmg):
        problems.append("the fire protections: %d of %d" % (after, dmg))
    if not re.search(r"damage: the player takes %d from Dragon" % after, text):
        problems.append("no damage line of %d" % after)
    return problems


CHECKS = {"max_hp_level_up_on": check_hit_dice, "max_hp_level_up_off": check_hit_dice,
          "verbose_combat_on": check_rolls, "dragon_breath_failed": check_breath, "dragon_breath_made": check_breath,
          "dragon_breath_resist": check_breath}


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
    # -objlog NAME (a name in the log directory): the object and spell log
    if "-objlog" in extra:
        objlog = os.path.join(logdir, extra[list(extra).index("-objlog") + 1])
        if os.path.exists(objlog):
            with open(objlog, "r", encoding="latin-1") as f:
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
        if t["name"] in CHECKS:
            problems += CHECKS[t["name"]](t["name"], text)
        if re.search(r"SOAK FAIL|Int3|Assertion|Error:", re.sub(t["known"], "", text) if t["known"] else text):
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
