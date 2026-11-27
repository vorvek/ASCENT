# The quest graph of the campaign

What the player must do to finish Descent to Undermountain 1.3, and where
each condition is tested: the missions of Khelben, the mission flags, the
quest items, the levels with their entries and exits, the scripted
sequences, the last fight and the end.

Sources: the scripts of `GABE.RLH` and `GABE.DAT` of the 1.3 install
(`python -I tools/script.py dis GABE.RLH`; "script 31 (02651)" is the byte
offset 0x02651 of the listing of script 31), `DTU.WRP` (line numbers of the
file), the `.TIG` and `.TAP` files, the level files (`tools/lvl.py`, the
`.LV2` of the 1.3 patch where it exists), and DTUGAME.EXE 1.3 (linear
addresses, `tools/dtudis.py` and D:\dev\DTU-Decomp). The byte code is in
[scripts.md](scripts.md), the triggers in [triggers.md](triggers.md), the
level state in [levelstate.md](levelstate.md).

The CD version has other scripts (`GABE.DAT` of the CD: 83 records, script
320 has 4437 bytes; 1.3: 87 records, 4492 bytes). This document is about 1.3.

The tables of sections 8 to 12 are made from the files by a program, not
by hand. Marks: **V** read in the code or the data, **G** a guess.

Tests: `tools/questtest.py` (section 13).

## 1. The frame of the campaign

**V** The player starts in the Yawning Portal (YPINN, level 0). Segment 1
has the DTU.WRP command 0: the way to Waterdeep (the front end), where
Khelben gives the missions (the Blackstaff tower), and the market sells the
stock that grows with the missions. The well of the inn (segments 118 and
226) leads to HUB1; a passage through the well costs 1 gold each way
(0x59F0B, `well_pay_passage`).

**V** The mission of Khelben is one number (0x13E588, 0 to 34). At each
visit the switch 0x83EF5 moves it on while its condition holds, and the
rewards 0x844ED of each new mission are given (the tables are in
[scripts.md](scripts.md), section 8). A condition is a mission flag (19
flags at 0x13CDD4) or a MISC item of the player. So the campaign is this
chain:

| Mission | Condition to leave it | What the player does | Where |
|---|---|---|---|
| 0 | flag 1 | kills the creature with script 23 (the kobold chief) and steps on the command segment | KOBOLDS segment 6. Script 23 also sets global flag 33 (flag 1) itself (00C02) |
| 1 | flag 2 (to 3 with MISC 0x12) | kills the creature with script 32 (the Ghoulmaster, object 106) | BONES segment 241. The first Spider Piece (MISC 0x12) is BONES object 6 |
| 2 | MISC 0x12 | brings the Spider Piece of BONES | |
| 3, 4 | flag 10, then MISC 0x7F, then flag 3 | walks into BAT segment 319; takes the thief scroll (MISC 0x7F, carried by THIEF2 object 93); kills the creature with script 70, or script 70 sets global flag 35 (01F32) | BAT, THIEF0, THIEF1, THIEF2 (segments 328 and 34). The Violet Gem Key (MISC 107, the reward of missions 3 and 4) opens the wall of HUB1 segment 40 |
| 5, 6 | MISC 0x7F, flag 3 | as above | |
| 7, 8 | flag 11, then MISC 0x10 (taken) | walks into MULHOR2 segment 226; brings the Scarab Beetle (MULHOR2 object 1) | HUB2, MULHOR1, MULHOR2 |
| 9 | MISC 0x10 | | |
| 10 | MISC 0x13 | brings the Spider Piece of MINE (object 36) | PRISON, MINE |
| 11 | MISC 0x0F (taken) | brings the wand of Myrkul (MYRKUL object 49) | HUB3, MYRKUL |
| 12, 13 | flag 12, then flag 4 | walks into CLAN1 segment 2; kills the creature with script 190 | CLAN1, CLAN2, CLAN3 (segments 94 and 131) |
| 14, 15 | flag 13, then flag 5 | walks into DROW1 segment 248; kills the creature with script 201 | DROW1 (segments 422, 396, 17) |
| 16 | MISC 0x17 | brings the Spider Piece of DROW1 (object 91) or DROW4 (object 5) | |
| 17 | flag 6 | kills the creature with script 212 | DROW2 (segments 24, 684) |
| 18, 19 | flag 14, then flag 7 (to 20 with MISC 0x18) | walks into DROW3 segment 285; kills the creature with script 222; the Spider Piece of DROW3 is object 63 | DROW3 (segment 311) |
| 20, 21, 22 | flag 15, flag 8, flag 9 | flag 15: script 54 of THIEF0 sets global flag 47 (no DTU.WRP line); kills the creatures with the scripts 252 and 254 | BEHOLDER (segments 160, 773 and 472) |
| 23 | flag 9 | | |
| 24 | MISC 0x11 (taken) | brings the Crystal Rose (DROW5 object 7) | HUB4, DROW5 |
| 25 to 28 | none | one visit moves through them | |
| 29, 30 | flag 19, and the seven pieces 0x12, 0x13, 0x14, 0x16, 0x17, 0x18, 0x1A | the scenery "Spectator Stone" of IBBALAR is gone when the player is in segment 0; the pieces of IBBALAR (object 22), DROW2 (object 41) and DROW5 (object 181) | IBBALAR |
| 31, 32 | flag 17 | walks into FLAME segment 2 or 519 | FLAME |
| 33, 34 | the end | | |

**V** No mission number is a condition of a level entry. The ways into the
later parts are the key items of the rewards (MISC 0x6B to 0x79, the list at
P+0x18D4) and the items of section 9: a trigger needs the item and runs the
trap that opens the wall or takes the lock off. A player can do a level
before its mission; the flag is then set already.

**V** Mission flag 15 has no DTU.WRP line. Script 54 (THIEF0) sets global
flag 47 at 00743 (section 10), which statement C5 copies to mission flag
15. Flags 16 and 18 are set by nothing in the data (flag 18 would need the
scenery "Cloning Machine").

## 2. The quest items

MISC ids, names as the item tables of the levels give them:

| MISC | Name | Use |
|---|---|---|
| 0x0E (14) | Ankh | Straum of BONES (script 31), the altar of Cyric of IBBALAR (script 294). Section 6 |
| 0x0F (15) | Mykrul's Wand | mission 11 (taken by Khelben); the scripts 160 to 162 of MYRKUL test it |
| 0x10 (16) | Scarab Beetle | mission 7 to 9 (taken); script 140 of MULHOR2 |
| 0x11 (17) | Crystal Rose | mission 24 (taken) |
| 0x12 to 0x1A (18 to 26) | Spider Pieces | 0x12 BONES, 0x13 MINE, 0x14 IBBALAR, 0x16 DROW2, 0x17 DROW1 and DROW4, 0x18 DROW3, 0x1A DROW5; 0x15 lies in FLAME (object 5). The triggers 10 to 16 of FLAME take the seven others. 0x19 (25) is the "Assembled Spider", FLAME object 4 |
| 0x26 (38) | Skull Key | reward of mission 1; the Cassalanter tomb of HUB1 (trigger 8) |
| 0x6B to 0x79 | the keys of Khelben | rewards of the missions (scripts.md section 8); needed by the triggers of the hubs (section 9) |
| 0x7F (127) | thief scroll | mission 3 to 6. Carried by THIEF2 object 93; script 70 also gives it |

The places of the items are in section 11.

## 3. Scripted sequences that the port had to get right

- **V** A warp of a script (statement DB, 0xAA1FC) writes a warp line into
  the segment of the player and calls the warp check 0x59C3C: the level
  state is saved and restored, the well takes its gold, and WEAPON 13 can
  stop the warp. 69 statements, 44 of them in the cheat script 511; the
  others are script 31 (BONES to the inn), 60 (the prison), 292, 294, 295
  and 296 (IBBALAR, 22 statements) and 321 (FLAME to BEHOLDER).
- **V** The prison of the Shadow Thieves (script 60 of THIEF1 at 02D99,
  0xAA3F3): when the player does not evade the thieves, the gold is gone,
  the hit points are 1, and each item that is not worn goes to the object
  of the script, to the chest object 99 or to one of the segments 297, 53,
  65, 52, 331, 330, 329, 328 of THIEF1. The player wakes in THIEF2 segment
  612; a guard (critter row 0xA5, script 55) comes into segment 631. The
  way back to the items is THIEF2 segment 329 to THIEF1. 1.3 reads the
  wrong list entry after it removes an item (`Dtu_fix_prison_items`).
- **V** Drowslayer (WEAPON 13): script 0x1FD (509) runs when the player
  talks with it in the hand (0xAC206), uses it, attacks, drops it, and
  before each warp to another level (0x59D2D). Global flag 0xA0 after the
  script drops the warp or the talk.
- **V** Scenery with a script is used with the use action (0xAC155 has no
  test of the type): the altar of Cyric (IBBALAR object 2, script 294) and
  the Flamesword (FLAME object 3, script 322, only after the death of
  Lolth: section 4).

## 4. The last level

**V** FLAME (level 32), all from script 320 (Lolth, object 36), 321
(Halaster, object 17), 322 (the Flamesword, object 3) and 323 (the statue,
object 4); the scripts of the objects 3 and 4 come from the level fixes
0xACCDB.

1. The statue: the triggers 10 to 16 (segments 229, 245, 275, 287, 266,
   258, 284, side 4) need the pieces 18, 19, 24, 26, 22, 23, 20, take them,
   open a wall and switch themselves off. Trigger 27 (segment 279 side 5)
   runs script 323: with all seven triggers off it opens the wall of
   segment 278 (trigger 17) and removes the eight piece objects; else the
   player is pulled away along one of four ways and loses 5 to 30 hit
   points when a dexterity roll fails.
2. Behind the wall is MISC 25, the Assembled Spider (object 4). The
   triggers 19, 20, 28 to 31 need it.
3. Lolth: the triggers 33, 32 and 26 run script 320 with its variable 0, 1
   and 2: two speeches, a trap of creatures by the class of the player
   (triggers 22 to 25), then global flag 17 (ignored: statement C5 does
   not write the flags 1 to 0x20), sound 0xFFFF (the movie FLAM1.MVE,
   0xA9A5B) and the fight. The first speech switches trigger 28 off
   (script 320 at 000AA, only in 1.3). The level rules 0x93ADE and 0x7DBF5
   run the script when the trigger could not.
4. The end: the death of Lolth (object 36 of level 0x20) calls 0x6A374
   in `remove_level_object` (0x6A579 to 0x6A59B). It clears object flag
   0x40 and +0x1AE bit 2 of the Flamesword (the level fixes set them, so
   the use ray passes the sword and the altar while Lolth lives), gives
   each creature control type 0 and runs script 322 of the sword
   (0xAC055(object 3, 0)). No use action is needed. After that the use
   action on the sword runs the script too. Script
   322: "YOU HAVE DESTROYED LOLTH'S AVATAR", the player goes to segment
   482, and the animation statement BC sets the hit points of the sword to
   0. The end of the run kills an object with less than 1.0 hit points
   (0xAC033, no test of the type), and 0xA6ACB for object 3 of level 0x20
   is the end sequence 0xA6786: a light, 50 sounds, the movie CREDITV1.MVE
   and the front end. Trigger 28 (the floor of segment 486, needs MISC 25)
   runs the same script in mode 3 while it is on.
5. Mission flag 17 comes from the DTU.WRP command 17 (segments 2 and 519),
   so Khelben shows mission 33 after the end.

The sword hangs 8.5 above the head of a player on segment 486; the object
test of the query (0x3EE70) misses it with a level ray. After the death of
Lolth the player can look up at the sword to use it (`tools/questtest.py`,
test `flame_ending`). Tests `flame_sword_lolth_alive` (the use ray passes
the sword, flag 0x40 on objects 2 and 3) and `flame_lolth_death` (the death
alone runs script 322 and the end). The port: `flame_lolth_dead` (dtuobj_combat.c).

## 5. Defects of 1.3 in the quests

| Defect | Evidence | The port |
|---|---|---|
| The prison sequence takes entry i of the item list, reads the entry that moved to its place and skips it | 0xAA47B (0xB6D24, 0xB6F01 moves the entries), 0xAA4AE reads `[ecx]` after it, 0xAA58C `inc esi` | `Dtu_fix_prison_items` (on): each item that is not worn goes to the thieves. `-origprison` |
| A creature that is not hostile does not walk along the path of statement E5: the goal NONE gives the animation STAND with no speed, the path follower turns it to the player, the end of the path near the player gives a path to the player, and far away only the index moves | 0x10148, 0x17CFC, 0x17505 end, 0x17411 | `Dtu_fix_script_walk` (on): the creature walks the path and its script runs in mode 2 at the end. `-origscriptwalk` |
| Statement EB reads a segment and does not use it | 0xAA8C7 | Kept: the spell is around the object |
| Statement C5 cannot set global flag 17 (script 320) | 0xAB124, numbers up to 0x20 are ignored | Kept: mission flag 17 comes from DTU.WRP |
| Six of the seven Ankhs of MULHOR1 get object flag 0x40 from the level fixes: the object queries skip them, so only object 33 can be taken | 0xAD0C0, 0x3F38F | Intended (decoys), ported: `OF_DTU_NOHIT`. Tests `mulhor1_ankh`, `mulhor1_ankh_flagged` |
| THIEF0 script 54 (Devlin), label 300, branch `resume obj 48 == 1`: after "Good work Mishad." the run ends, so flag 5 and flag 4 of object 10 stay clear and label 800 (gold, global flag 66) is never reached; the other two branches of the same result go to label 800 | script 54 at 0x142D to 0x1477, 0x13AD, 0x164E (tools/script.py) | `Dtu_fix_script_devlin` (on): the branch goes on to label 800. `-origscript`. Tests `thief0_devlin_goblins`, `thief0_devlin_goblins_orig` |
| Daland of MYRKUL (object 3, script 160): the level fix 0xAD129 holds him with no end (control type 0, peaceful), and a held creature cannot talk ("%s tries to speak, but nothing happens.", 0xABBE5). A Dispel Magic frees him in 1.3: 0x12AD9 makes only a creature of control type 1 hostile. The port keeps him an AI creature, so the spell made him hostile and his quest could not be completed | 0xAD129, 0xABBE5 (+0x89 bit 0x40), 0x8CFFF (0xA6505, 0x12AD9 type 5); script 160; MYRKUL.SAY 18 (trigger 24, segment 17) | `Dtu_fix_dispel_peace` (on, defects D-196): a Dispel Magic of the player is not an attack on a creature that is not hostile; the talk to a creature held with no end adds "Some magic seems to hold %s completely paralyzed. Perhaps it can be dispelled." `-origcombat`. Tests `myrkul_daland_mace`, `myrkul_daland_rod`, `myrkul_daland_orig` |

### Daland and the altar of Kelemvor (MYRKUL)

The story of the data: Daland, a priest of Kelemvor, read a cursed scroll in the temple of Myrkul and "became transfixed to this spot. I cannot move." (script 160, label 5). His hope: "If the altar were consecrated with a flask of Dragon's Blood, this place would become holy to Kelemvor and he could free me." (label 11). He gives the player the Potion of Dragon's Blood (misc 74, label 13). Trigger 31 (segment 460 side 3, needs misc 74, message 25 "You remove the potion of Dragon's Blood and pour it upon the altar...") consecrates the altar; then he gives the Potion of Extra-Healing (misc 75), 600 experience, and turns the sign of segment 448 to Kelemvor (trigger 32, message 26). Trigger 24 (segment 17, message 18) announces him: "You have found some hidden crypts. There is a lone person standing here, still as a statue... or a corpse!" The curse is the Hold of the level fix with no end; in 1.3 it also silences him, so the player must dispel it first. MYRKUL has no Dispel Magic: the mage scroll of the spell lies in MULHOR2 (object 31, segment 331), the cleric scroll in CLAN3 (object 30, segment 372, after MYRKUL); a party with no mage has no way to free him. The level has a Hold Person scroll near his cell (object 74, segment 274) and a Stone Skin scroll (object 73, segment 369). With the new gear the Mace of Disruption comes with the potion (docs/re/objects.md, "New gear").

## 6. The Ankh of Straum (BONES, script 31)

**V** Straum is object 55 of BONES (a skeleton, script 31). Label 1 (00003)
goes to label 800, the "too late" branch, when the mission + 1 is above 11
or the talk counter is 15. He asks for his Ankh; the reply "Until we meet
again..." (label 600, 025D3) sets flag 1. After the death of the
Ghoulmaster (object 106, script 32) and without the Ankh he says "From
beyond the grave, his power haunts me still!".

**V** No script gives MISC 14 to the player (script 31 at 0377B drops the
Ankh that Straum carries). The Ankh is in MULHOR1 (level 13): seven MISC 14
objects, 1, 2, 33, 34, 60, 73 and 91 (section 11). The level fixes set
object flag 0x40 on the six objects 91, 73, 1, 2, 34 and 60 (0xAD0C0); object
33 in segment 585 is the one that they leave.

**V** Flag 0x40 takes an object out of the object queries: the object loop
of `fvi_sub` skips it (0x3F38F). A pick up is a hit of the player move, so
the player walks through a flagged Ankh and does not take it. The use ray
and shots pass it too. The Ankh is still drawn: no draw code reads the
flag. So the player sees seven Ankhs and can take only object 33. Nothing
chooses one at random: the level fixes are a fixed list of slots, no
script names MISC 14 in a give statement (the "give from seg" statement
0xA8F77 has no flag test, but no script uses it for MISC 14), and no
trigger or trap of MULHOR1 moves or removes an object. Each level start
sets the flags again, also after a restore.

**V** The places (`tools/lvl.py`, `tools/sidefiles.py`): objects 1 (segment
462), 2 (467), 60 (697), 73 (246) and 91 (239) each lie in a segment with
one open side, a niche at the end of a passage; 1 and 2 (x -70 and 70) and
73 and 91 (x -124 and 125) are mirror pairs. Object 34 (segment 499) is
behind the door of the Bronze Key (trigger 30, MISC 51), object 33 (segment
585) behind the door of the Ruby Key (triggers 32 and 33, MISC 57), and 60
is behind a closed wall (segment 146 side 3, trap 24). Trigger 46 (segment
223) opens a door for a player with MISC 14 and does not take it; trigger
45 says "This door opens only to those who hold the holy Ankh, the key of
life! Find the pyramid of life!" **G**: the six decoys are intended: the
fix keeps the one Ankh behind the Ruby Key door. The port does the same.

So the player meets Straum in mission 2,
and can bring the Ankh from mission 7 on (HUB2 leads to MULHOR1); mission
11 is the last one (label 800 after it).

**V** Label 700 (0264B): with the Ankh, `TAKE MISC 14` (Straum carries it).
If the player is True Neutral or a cleric or flag 1 is set, and flag 6 of
object 1 is clear, and the anger is below 3, Thoth grants one favor: 710 a
magical item, 720 armor, 730 spells, 740 a weapon, 750 8000 gold, 760 power
(one chance in four: statement 9F, one level more; else constitution and
charisma + 1), 770 the Yawning Portal (DB level 0 segment 33), 780 later,
790 anything, 795 nothing. Label 910 gives 600 experience. At label 900
Straum, with the Ankh, rises (TELEPORT_SELF to segments 440, 441 and 442,
each in four facings) and goes to label 910: the player gets the hit
points, four spells and the experience; "his formal shell crumbles to the
floor", then statement BC (ANIM 9) sets his hit points to 0. The end of the
run (0xAC033) calls 0xA6ACB(Straum, 0, 1.0): the hit points become 1.0
(0xA6B6A) and 0x29A9C does 1.0 damage with no attacker, so he dies (no
experience) and drops the Ankh. 1.3 does not leave him standing.

**V** Flag 1 is the "wary" flag of the status code (0xAAA32): while it is
set, a talk starts only when `rand() % 20 + 1` is not above the charisma;
else Straum says a line of label 0x3E9. A player with low charisma must
talk several times.

The port: the seven objects are placed, and the level fixes give the six
the flag `OF_DTU_NOHIT` (`fix_nohit`, dtuobj_setup.c). Test `mulhor1_ankh`: the
player takes object 33. Test `mulhor1_ankh_flagged`: the six have flag 0x40,
the player walks into object 91 and does not take it (the build of the port
without the flag takes it). Test `bones_straum_ankh`: label 700 takes the
Ankh, reply 5 gives 8000 gold, 600 experience, and Straum dies at the end
of the run (anim 9). Test `bones_straum_dead_saveload`: he is still dead
after a save and a load, and a talk does not start. The port had left him
standing at 0 hit points: its kill did damage only to a creature with 1.0
hit points or more and did not set them to 1.0 first (fixed, `obj_kill` of
dtuscript.c).

## 7. What the port does not have

See [../audit/quests.md](../audit/quests.md).

## 8. The levels: warps and mission commands

From DTU.MSN and DTU.WRP. A warp inside a level and the music and sound
commands are left out. MADLANDS and PLAYGRND have no level file; CLAN2,
BEHOLDR2 and IBBALAR2 have no TIG file. IBBALAR2 is reached only by a
script teleport (script 511).

| Level | File | In the data | Warps to other levels (segment: level segment, DTU.WRP line) | Mission commands (command at segment, line) |
|---|---|---|---|---|
| 0 | YPINN | yes | 118: HUB1 136 (47); 226: HUB1 204 (48) | 0 at 1 (49) |
| 1 | HUB1 | yes | 91: KOBOLDS 1 (61); 107: BAT 2 (62); 170: BONES 307 (63); 121: YPINN 33 (64); 205: YPINN 225 (65); 193: HUB2 188 (66) |  |
| 2 | KOBOLDS | yes | 12: HUB1 102 (78) | 1 at 6 (95) |
| 3 | BONES | yes | 381: HUB1 48 (102) | 2 at 241 (125) |
| 4 | BAT | yes | 1: HUB1 110 (132); 322: THIEF0 2 (133) | 10 at 319 (153) |
| 5 | THIEF0 | yes | 1: BAT 321 (160); 37: THIEF1 320 (161); 152: THIEF1 79 (162) |  |
| 6 | THIEF1 | yes | 190: THIEF0 151 (183); 257: THIEF0 32 (184); 556: THIEF2 328 (185) |  |
| 7 | THIEF2 | yes | 329: THIEF1 557 (224); 22: HUB1 110 (225) | 3 at 34 (250); 3 at 328 (249) |
| 8 | HUB2 | yes | 288: HUB1 2 (257); 178: BANE 1 (258); 272: PRISON 428 (259); 84: MULHOR1 1 (260); 204: HUB3 7 (261) |  |
| 9 | BANE | yes | 121: HUB2 231 (275) |  |
| 10 | PRISON | yes | 776: HUB2 238 (305); 467: MINE 0 (306) |  |
| 11 | MINE | yes | 108: PRISON 468 (338); 705: CRYPTS 384 (339) |  |
| 12 | CRYPTS | yes | 385: MINE 694 (372); 556: HUB2 199 (373) |  |
| 13 | MULHOR1 | yes | 721: MULHOR2 75 (402); 0: HUB2 252 (403) |  |
| 14 | MULHOR2 | yes | 270: HUB2 252 (432) | 11 at 226 (452) |
| 15 | HUB3 | yes | 1: HUB2 245 (459); 42: MYRKUL 113 (460); 78: CLAN1 2 (461); 148: DROW1 422 (462); 87: HUB4 50 (463) |  |
| 16 | MYRKUL | yes | 112: HUB3 106 (481) |  |
| 17 | CLAN1 | yes | 0: HUB3 29 (514); 534: CLAN3 131 (515); 523: CLAN3 94 (516) | 12 at 2 (534) |
| 18 | CLAN2 | yes | none |  |
| 19 | CLAN3 | yes | 98: CLAN1 547 (548); 589: CLAN1 20 (549) | 4 at 94 (567); 4 at 131 (568) |
| 20 | DROW1 | yes | 427: HUB3 115 (575); 366: DROW2 1 (576); 313: CLAN1 744 (577) | 5 at 17 (594); 5 at 396 (593); 5 at 422 (592); 13 at 248 (595) |
| 21 | DROW2 | yes | 0: DROW1 25 (602); 483: DROW3 311 (603) | 6 at 24 (639); 6 at 684 (640) |
| 22 | DROW3 | yes | 316: DROW2 481 (647) | 7 at 311 (668); 14 at 285 (669) |
| 23 | DROW4 | yes | none |  |
| 24 | HUB4 | yes | 52: HUB3 177 (683); 24: BEHOLDER 5 (684); 73: DROW5 1 (685); 70: IBBALAR 790 (686); 62: FLAME 2 (687) |  |
| 25 | BEHOLDER | yes | 161: HUB4 94 (705); 774: THIEF2 604 (706) | 8 at 160 (758); 8 at 773 (760); 9 at 472 (759) |
| 26 | BEHOLDR2 | yes | none |  |
| 27 | DROW5 | yes | 0: HUB4 6 (774) |  |
| 28 | MADLANDS | no | none |  |
| 29 | IBBALAR | yes | 774: HUB4 67 (806) | 19 at 0 (831) |
| 30 | IBBALAR2 | yes | none |  |
| 31 | PLAYGRND | no | none |  |
| 32 | FLAME | yes | 520: HUB4 40 (852); 1: HUB4 40 (853) | 17 at 2 (871); 17 at 519 (872) |

## 9. The triggers that need or lack an item

From the TIG and TAP files. "Takes": the item leaves the inventory. Side 3
is the floor (the player enters the segment); another side is used with
the use action or touched.

| Level | Trigger | Segment.side | On | Needs | Takes | Lacks | Effect |
|---|---|---|---|---|---|---|---|
| YPINN | 11 | 225.4 | 1 | MISC 71 |  |  | trap 4 (225.4: lock) |
| HUB1 | 7 | 204.4 | 1 | MISC 71 | yes |  | trap 0 (204.4: opens the door, lock) |
| HUB1 | 8 | 58.4 | 1 | MISC 38 | yes |  | trap 1 (58.4: opens the door, lock) |
| HUB1 | 9 | 115.4 | 1 | MISC 107 | yes |  | trap 2 (115.4: opens the door, lock) |
| HUB1 | 10 | 61.3 | 1 | MISC 107 |  |  | trap 3 (40.4: wall open) |
| HUB1 | 11 | 100.4 | 1 | MISC 108 | yes |  | trap 4 (100.4: opens the door, lock) |
| BONES | 8 | 21.4 | 1 | MISC 36 | yes |  | trap 2 (21.4: lock) |
| BONES | 9 | 187.4 | 1 | MISC 33 | yes |  | trap 1 (187.4: lock) |
| BONES | 10 | 182.3 | 1 |  |  | MISC 33 | message 4 |
| BONES | 11 | 18.3 | 1 |  | yes | MISC 36 | message 2 |
| BAT | 24 | 85.4 | 1 | MISC 103 | yes |  | trap 5 (85.4: spell 16); script of object 9; message 18 |
| THIEF1 | 1 | 3.3 | 1 | MISC 66 |  |  | trap 0 (6.4: opens the door, lock); message 3 |
| HUB2 | 7 | 273.5 | 1 | MISC 109 |  |  | trap 0 (273.5: lock) |
| HUB2 | 8 | 248.4 | 1 | MISC 110 |  |  | trap 1 (248.4: lock) |
| PRISON | 44 | 565.4 | 1 | MISC 59 |  |  | trap 11 (565.4: opens the door, lock) |
| PRISON | 45 | 347.4 | 1 | MISC 59 |  |  | trap 12 (347.4: opens the door, lock) |
| PRISON | 46 | 392.4 | 1 | MISC 59 |  |  | trap 13 (392.4: opens the door, lock) |
| PRISON | 47 | 389.4 | 1 | MISC 59 |  |  | trap 14 (389.4: opens the door, lock) |
| PRISON | 48 | 558.4 | 1 | MISC 59 |  |  | trap 15 (558.4: opens the door, lock) |
| PRISON | 49 | 559.4 | 1 | MISC 59 |  |  | trap 16 (559.4: opens the door, lock) |
| PRISON | 50 | 271.4 | 1 | MISC 59 |  |  | trap 17 (271.4: opens the door, lock) |
| PRISON | 51 | 561.4 | 1 | MISC 59 |  |  | trap 18 (561.4: opens the door, lock) |
| PRISON | 52 | 564.4 | 1 | MISC 59 |  |  | trap 19 (564.4: opens the door, lock) |
| MINE | 17 | 205.3 | 1 |  |  | MISC 27 | message 16 |
| MINE | 18 | 169.3 | 1 | MISC 27 |  |  | trap 5 (205.4: opens the door, lock) |
| MULHOR1 | 29 | 562.3 | 1 |  |  | MISC 51 | message 18 |
| MULHOR1 | 30 | 551.3 | 1 | MISC 51 |  |  | trap 18 (523.5: lock) |
| MULHOR1 | 31 | 556.3 | 1 |  |  | MISC 57 | message 19 |
| MULHOR1 | 32 | 558.3 | 1 | MISC 57 |  |  | trap 19 (556.4: opens the door, lock) |
| MULHOR1 | 33 | 565.3 | 1 | MISC 57 |  |  | trap 19 (556.4: opens the door, lock) |
| MULHOR1 | 45 | 599.3 | 1 |  |  | MISC 14 | message 17 |
| MULHOR1 | 46 | 223.3 | 1 | MISC 14 |  |  | trap 29 (637.4: opens the door, lock) |
| MULHOR2 | 28 | 347.3 | 1 | MISC 27 | yes |  | trap 11 (345.4: opens the door, lock) |
| HUB3 | 10 | 74.4 | 1 | MISC 112 |  |  | trap 1 (74.4: lock) |
| HUB3 | 11 | 29.4 | 1 | MISC 111 |  |  | trap 2 (29.4: lock) |
| HUB3 | 12 | 177.0 | 1 | MISC 116 |  |  | trap 0 (177.0: lock) |
| MYRKUL | 16 | 50.3 | 1 | MISC 38 | yes |  | trap 6 (50.5: opens the door, lock) |
| MYRKUL | 23 | 201.3 | 1 | MISC 32 | yes |  | trap 8 (201.4: opens the door, lock) |
| MYRKUL | 31 | 460.3 | 1 | MISC 74 | yes |  | message 25 |
| CLAN3 | 9 | 284.0 | 1 | WEAPON 73 | yes |  | trap 7 (404.4: lock); message 7 |
| CLAN3 | 10 | 268.4 | 1 | WEAPON 72 | yes |  | trap 8 (263.5: lock); message 8 |
| CLAN3 | 12 | 169.3 | 1 |  |  | MISC 27 | message 10 |
| CLAN3 | 13 | 164.3 | 1 | MISC 27 | yes |  | trap 9 (170.4: lock); message 11 |
| DROW1 | 15 | 25.3 | 1 | MISC 114 |  |  | trap 6 (25.4: lock) |
| DROW1 | 17 | 490.3 | 1 | MISC 47 |  |  | trap 7 (490.4: lock) |
| DROW1 | 18 | 707.3 | 1 | MISC 47 |  |  | trap 8 (707.4: lock) |
| DROW1 | 22 | 396.4 | 1 | MISC 71 | yes |  | trap 5 (396.4: lock) |
| DROW2 | 27 | 448.3 | 1 | MISC 42 | yes |  | trap 13 (449.4: lock); message 22 |
| DROW2 | 29 | 481.3 | 1 | MISC 115 |  |  | trap 15 (481.2: opens the door, lock) |
| DROW4 | 5 | 399.3 | 1 | MISC 71 | yes |  | trap 0 (399.4: lock) |
| DROW4 | 9 | 380.3 | 1 | MISC 71 | yes |  | trap 1 (380.4: lock) |
| HUB4 | 4 | 40.0 | 1 | MISC 121 |  |  | trap 2 (40.0: opens the door, lock) |
| HUB4 | 5 | 71.4 | 1 | MISC 117 |  |  | trap 0 (71.4: opens the door, lock) |
| HUB4 | 6 | 68.4 | 1 | MISC 118 |  |  | trap 1 (68.4: opens the door, lock) |
| BEHOLDER | 48 | 786.4 | 1 | MISC 27 | yes |  | trap 17 (786.4: opens the door, lock) |
| DROW5 | 9 | 369.3 | 1 | MISC 57 | yes |  | trap 2 (426.4: opens the door, lock) |
| DROW5 | 17 | 146.3 | 1 | MISC 48 | yes |  | trap 3 (147.4: lock) |
| FLAME | 10 | 229.4 | 1 | MISC 18 | yes |  | trap 0 (229.4: wall open); message 10 |
| FLAME | 11 | 245.4 | 1 | MISC 19 | yes |  | trap 1 (245.4: wall open); message 11 |
| FLAME | 12 | 275.4 | 1 | MISC 24 | yes |  | trap 2 (275.4: wall open); message 12 |
| FLAME | 13 | 287.4 | 1 | MISC 26 | yes |  | trap 3 (287.4: wall open); message 13 |
| FLAME | 14 | 266.4 | 1 | MISC 22 | yes |  | trap 4 (266.4: wall open); message 14 |
| FLAME | 15 | 258.4 | 1 | MISC 23 | yes |  | trap 5 (258.4: wall open); message 15 |
| FLAME | 16 | 284.4 | 1 | MISC 20 | yes |  | trap 6 (284.4: wall open); message 16 |
| FLAME | 19 | 626.3 | 1 | MISC 25 |  |  | message 18 |
| FLAME | 20 | 280.3 | 1 | MISC 25 |  |  | trap 9 (231.3: creature 8); trap 10 (244.3: creature 4); trap 11 (621.3: creature 3); trap 12 (239.3: creature 0); message 19 |
| FLAME | 28 | 486.3 | 1 | MISC 25 |  |  | script of object 3 |
| FLAME | 29 | 416.4 | 1 | MISC 25 |  |  | trap 13 (416.4: opens the door, lock); message 22 |
| FLAME | 30 | 291.4 | 1 | MISC 25 |  |  | trap 26 (291.4: opens the door, lock) |
| FLAME | 31 | 292.4 | 1 | MISC 25 |  |  | trap 27 (292.4: opens the door, lock) |

The Violet Gem Keys (Data, the tables and the saves of 2026-11-26): misc 107 to
124 all have the name `Violet Gem Key`, and each one opens its own lock (the
table above: 109 and 110 in HUB2, 111 and 112 in HUB3, 114 in DROW1, 115 in
DROW2, 116 to 118 and 121 later). Khelben gives them with his missions (the
log of the port: misc 113 with mission 16, 114 and 115 with mission 17), and no
trigger takes them. The key list holds each number once (0xB65BB, code 7). So a
player in the middle of the game has several keys with the same name, each
needed for its door: the user's save after mission 17 has the seven keys 109 to
115, one of each. The level-file key misc 71 (also named `Violet Gem Key`) goes
to the item list, and HUB1 trigger 7, DROW1 trigger 22 and DROW4 triggers 5 and 9
take it.

The turn-in of a plot item (YPINN script 6 label 17, Sangalor: `TAKE MISC 126`;
script 1 label 16, Mirt: `TAKE MISC 104`) takes one copy and sets flag5 of the
script, and the next talk goes past the reward. So more copies give no second
reward (the extra copies of 1.3 stay in the bag, docs/re/objects.md, the byte at
0x1D5). Script 6 label 16 ("Actually, I have something right here.") ends the
talk with no reward, no take and no flag; only label 17 (from label 9) gives it.
Label 16 is never reached (checked 2026-11-27): its reply is in label 12,
which only label 9 reaches when object flag 4 is clear, and label 999 (the
first label, where each talk starts: the script sets no resume label) sets
flag 4 at the first talk. So label 9 always takes the document and goes to
label 17 (a run with `resume=30.9` after a first talk: `goto 11`, 2000 gold,
take misc 126). No setting: the branch has no effect for a player.

## 10. The scripts: flags, items and teleports

Scripts with none of these are left out. The number in brackets is the
offset of the first use in the listing of the script.

| Script | Level | Global flags set | Global flags cleared | Global flags read | Mission read | MISC given | MISC taken | MISC tested | Teleports the player to |
|---|---|---|---|---|---|---|---|---|---|
| 1 | YPINN | 111 (037E9), 119 (03894) | 119 (03B22) | 1 (01225), 33 (01225), 15 (01235), 2 (0123F), 3 (01249), 35 (01249), 11 (01259), 43 (01259), 66 (018BC), 67 (018C2), 111 (018C9), 107 (022D3), 103 (02945), 18 (02A03), and 4 more |  | 75 (02613) | 104 (0260E) | 104 (02459) |  |
| 2 | YPINN | 113 (028CB), 72 (02A3A) |  | 112 (00AD4), 113 (00AD4), 2 (00AF1), 34 (00AF1), 3 (00B01), 35 (00B01), 11 (00B11), 43 (00B11), 97 (00EA6), 12 (0265E), 44 (0265E), 13 (0266E), 45 (0266E), 15 (0267E), and 3 more |  |  | 106 (028D0), 103 (02A30) | 103 (00AE5), 106 (026E2) |  |
| 3 | YPINN | 74 (00443), 153 (00448), 111 (0110E) |  | 13 (00437), 45 (00437), 99 (00978), 100 (009E6), 97 (00A03), 7 (00B6F), 39 (00B6F), 9 (00B7F), 41 (00B7F), 101 (00B8F), 110 (00FE4), 111 (00FEA) |  | 2 (01104), 75 (01622), 12 (01AA3), 83 (01AAD) |  |  |  |
| 4 | YPINN |  |  | 10 (00F4E), 42 (00F4E), 3 (00F5E), 35 (00F5E), 11 (00F6E), 43 (00F6E), 12 (00F7E), 44 (00F7E), 15 (00F8E), 47 (00F8E), 107 (01574) | >= 8 (02016) |  |  |  |  |
| 5 | YPINN |  |  | 107 (00D85), 10 (0174F), 42 (0174F), 3 (0175F), 35 (0175F), 11 (0176F), 43 (0176F), 12 (0177F), 44 (0177F), 15 (0178E), 47 (0178E) |  | 79 (013FD) | 3 (00695) | 3 (00584) |  |
| 6 | YPINN | 106 (005A9) |  | 97 (002F5), 106 (002FB), 2 (007A0), 3 (007AA), 35 (007AA), 11 (007BA), 7 (007C5), 39 (007C5), 9 (01A55), 1 (0314E) |  | 78 (019A4), 80 (019AF), 86 (019BA) | 126 (0199F) | 126 (00D13) |  |
| 7 | YPINN | 75 (0000B), 109 (011B4) |  | 109 (006AE), 10 (007D4), 42 (007D4), 14 (007E4), 46 (007E4), 107 (00941) | < 8 (00116), >= 8 (005D5) |  |  |  |  |
| 8 | YPINN |  |  | 70 (000A6), 11 (000A6), 13 (00817), 45 (00817), 15 (0093A), 47 (0093A) | < 8 (0071A) |  |  |  |  |
| 20 | KOBOLDS | 97 (00AFF) |  |  |  |  |  |  |  |
| 21 | KOBOLDS |  |  | 97 (00C3B) |  |  |  | 89 (0082B), 90 (0082B), 91 (0082B) |  |
| 23 | KOBOLDS | 33 (00C02) |  |  |  |  | 79 (00A72) | 79 (009C9) |  |
| 24 | KOBOLDS | 71 (00C7A) |  |  | > 1 (00BD4), > rand(6..14) (00F55) |  |  |  |  |
| 31 | BONES | 70 (00020), 69 (03149) |  | 97 (00291) | > 11 (00003), >= 8 (01C03) | 72 (02900), 76 (0290B), 80 (02916), 84 (02921), 87 (029B1), 75 (029BC), 78 (029C7), 88 (029D2), 2 (029F4), 79 (02A1E), 12 (02A5E), 86 (02A7E), 85 (02AD2), 77 (02B15), and 6 more | 14 (02657) | 14 (000DE), 18 (0098E), 2 (029ED), 12 (02A57), 96 (02EBF), 100 (02FAA) | YPINN 33 (03137) |
| 40 | BAT | 73 (0000B) |  |  |  |  |  |  |  |
| 50 | THIEF0 | 65 (00B90) |  | 65 (00005), 68 (00005), 67 (00C9B) |  |  |  |  |  |
| 51 | THIEF0 | 65 (009D3) | 65 (00993) | 65 (00078), 68 (00080) |  | 3 (00F34) |  |  |  |
| 52 | THIEF0 |  |  | 65 (00005), 68 (00005), 67 (00017) |  |  |  |  |  |
| 53 | THIEF0 |  |  |  |  |  | 4 (000C7) |  |  |
| 54 | THIEF0 | 47 (00743), 66 (02885) | 69 (01D5A) | 67 (0013D), 69 (01D54) | >= 4 (01F45), >= 2 (01F9E) |  | 18 (01E1E) | 18 (01BBD) |  |
| 55 | THIEF0 |  |  | 67 (00003) |  |  |  |  |  |
| 60 | THIEF1 | 65 (01299), 67 (025B3) | 69 (0182C), 66 (018FF), 67 (0299C) | 65 (000B5), 66 (00119), 67 (001D9), 113 (0059B), 107 (006B5), 3 (0072A), 33 (00C2F), 69 (01811), 77 (02D10) |  | 102 (026FC) |  |  | THIEF2 65535 (02D99) |
| 61 | THIEF1 |  |  | 67 (00042), 35 (000C7) |  |  |  |  |  |
| 62 | THIEF1 |  |  | 67 (00015) |  |  |  |  |  |
| 63 | THIEF1 |  |  | 67 (00003) |  |  |  |  |  |
| 70 | THIEF2 | 67 (001B0), 35 (01F32) | 66 (02C24), 67 (02C29) | 110 (00057), 67 (001FF), 68 (00289), 65 (00744), 97 (00F6C) |  | 3 (001BC), 4 (001C0), 2 (001C4), 99 (001CB), 127 (01F2D) |  |  |  |
| 71 | THIEF2 | 68 (0086F) |  | 67 (0009B), 65 (001D2) |  |  |  |  |  |
| 90 | BANE |  |  | 73 (0000E), 72 (0017B) |  | 103 (00714) |  | 103 (00227) |  |
| 103 | PRISON |  |  |  |  |  |  | 59 (00007) |  |
| 110 | MINE | 97 (008F3) |  |  |  |  |  |  |  |
| 111 | MINE | 97 (006CF) |  |  |  |  |  |  |  |
| 120 | CRYPTS | 97 (00878) |  |  |  |  | 79 (00576), 75 (0058E) | 79 (0078F), 75 (007B6) |  |
| 121 | CRYPTS | 97 (00086) |  | 97 (0055F) |  |  |  |  |  |
| 140 | MULHOR2 |  |  |  |  |  |  | 16 (0000B) |  |
| 160 | MYRKUL |  |  |  |  | 75 (001A8), 74 (00FC2) |  | 15 (00073) |  |
| 161 | MYRKUL |  |  |  |  |  |  | 15 (00113) |  |
| 162 | MYRKUL |  |  |  |  |  |  | 15 (0009D) |  |
| 170 | CLAN1 |  |  | 4 (0013B) |  |  |  |  |  |
| 200 | DROW1 | 155 (0042F) |  |  |  | 71 (00F89) |  |  |  |
| 202 | DROW1 | 154 (00B6F) |  |  |  | 47 (00B5F) |  |  |  |
| 210 | DROW2 | 107 (0000B) |  | 107 (0018C) |  |  |  |  |  |
| 211 | DROW2 | 156 (00011), 100 (00016) |  | 74 (00176) |  |  |  |  |  |
| 213 | DROW2 | 107 (0008A) |  | 107 (0000B) |  |  |  |  |  |
| 221 | DROW3 |  |  | 106 (00086) |  |  |  |  |  |
| 250 | BEHOLDER | 110 (0000E) |  | 3 (0009F), 35 (0009F) |  |  |  |  |  |
| 251 | BEHOLDER |  |  | 67 (000BF) |  |  |  |  |  |
| 254 | BEHOLDER | 110 (0000E), 112 (0109E) |  | 67 (00014), 112 (0001A), 113 (00020), 110 (00ABE) |  | 106 (01094) |  |  |  |
| 255 | BEHOLDER |  |  | 67 (00498), 35 (0069A) |  | 27 (00356) |  |  |  |
| 270 | DROW5 | 158 (0011F) |  |  |  |  |  |  |  |
| 271 | DROW5 | 159 (002D1) | 159 (005C8) |  |  |  |  |  |  |
| 272 | DROW5 | 157 (0017D) |  |  |  |  |  |  |  |
| 273 | DROW5 | 69 (00A76) |  |  |  |  |  |  |  |
| 290 | IBBALAR | 116 (0000B), 115 (00018), 117 (01318) |  |  |  |  |  |  |  |
| 291 | IBBALAR | 116 (0000B), 115 (00018), 114 (0007D) |  | 98 (00210), 118 (002C9) |  |  |  | 105 (00216), 20 (00268) |  |
| 292 | IBBALAR | 116 (0000B), 115 (00018), 118 (00A74), 98 (00CD6) |  | 98 (00035), 116 (000D7), 117 (000D7), 114 (00123) |  | 75 (00B55), 76 (00B60) | 105 (00ABB) | 105 (00025) | IBBALAR 666 (0045A) |
| 293 | IBBALAR | 116 (00009), 115 (00016) |  |  |  |  |  |  |  |
| 294 | IBBALAR | 116 (00009), 115 (00016) |  | 98 (00478), 33 (0053D), 35 (0053D), 69 (0053D) |  | 4 (0075D) | 4 (00753), 103 (00898), 105 (0094F) | 103 (00206), 105 (002DB), 14 (0053D) | IBBALAR 198 (00B51) |
| 295 | IBBALAR | 116 (00009), 115 (00016) |  | 98 (00DA3) |  | 75 (00E81), 76 (00F6A), 86 (01080) |  | 20 (00D39), 105 (00DA3), 75 (00E75) | IBBALAR 127 (00EB6), IBBALAR 493 (00F81), IBBALAR 596 (00FFB) |
| 296 | IBBALAR | 116 (00013), 115 (00020) |  | 71 (003BF), 76 (004AA), 75 (005D5) |  |  |  | 101 (006DC) | BONES 376 (00815), MINE 477 (0082C), DROW1 523 (00843), DROW2 208 (0085B), DROW3 482 (00872), DROW5 147 (0088A), FLAME 2 (008B7), IBBALAR 140 (008CD), IBBALAR 74 (008E4), IBBALAR 347 (009C2), FLAME 417 (00A4B), HUB4 50 (00AC5) |
| 320 | FLAME | 17 (0042B) |  | 128 (00ABF) |  |  |  |  |  |
| 321 | FLAME | 76 (00017), 128 (00E50) |  | 116 (00FC8) |  |  |  | 21 (001B5) | BEHOLDER 99 (01A21) |
| 323 | FLAME |  |  | 69 (0003D), 100 (0003D), 106 (0003D), 128 (0003D) |  |  |  |  |  |
| 508 | global |  |  | 77 (00023), 69 (00023) |  |  |  |  |  |
| 509 | global | 160 (0017F), 137 (012C5), 136 (01789), 135 (01790) | 160 (00003), 129 (01940), 130 (01945), 131 (0194A), 132 (0194F), 133 (01954), and 14 more | 141 (00008), 142 (00013), 143 (0001E), 144 (0002A), 145 (00036), 146 (00042), 130 (00058), 131 (00058), 132 (00058), 133 (00058), 139 (002BB), 152 (00371), 147 (00554), 148 (00667), and 17 more |  |  |  |  |  |
| 510 | global |  |  | 19 (0014F), 17 (00379), 1 (005D4), 33 (005D9), 3 (006FA), 35 (006FF), 71 (0085A), 107 (0094C), 113 (009F8) | == 30 (000F7), == 31 (00229), == 32 (0036C), == 33 (0036C), == 34 (004CD) |  |  | 20 (000FE), 21 (0028B), 25 (003FA), 103 (00A71), 106 (00A98), 2 (00AE9), 75 (00AE9), 79 (00AE9) |  |
| 511 | global |  |  |  |  |  |  |  | YPINN 33 (001F6), HUB1 136 (00202), KOBOLDS 1 (0020E), BONES 307 (00219), BAT 2 (00226), THIEF0 2 (00231), THIEF1 320 (0023C), THIEF2 328 (00249), HUB2 188 (00256), BANE 1 (00262), PRISON 428 (0026D), MINE 0 (0027A), and 32 more |

## 11. Where the quest items are

| MISC | Level objects (level file, the 1.3 LV2 where it exists) |
|---|---|
| 14 (0xE) | MULHOR1 object 1 (segment 462); MULHOR1 object 2 (segment 467); MULHOR1 object 33 (segment 585); MULHOR1 object 34 (segment 499); MULHOR1 object 60 (segment 697); MULHOR1 object 73 (segment 246); MULHOR1 object 91 (segment 239) |
| 15 (0xF) | MYRKUL object 49 (segment 224) |
| 16 (0x10) | MULHOR2 object 1 (segment 476) |
| 17 (0x11) | DROW5 object 7 (segment 435) |
| 18 (0x12) | BONES object 6 (segment 376); FLAME object 6 (segment 658) |
| 19 (0x13) | MINE object 36 (segment 477); FLAME object 12 (segment 652) |
| 20 (0x14) | IBBALAR object 22 (segment 193); FLAME object 9 (segment 655) |
| 21 (0x15) | FLAME object 5 (segment 659) |
| 22 (0x16) | DROW2 object 41 (segment 208); FLAME object 8 (segment 656) |
| 23 (0x17) | DROW1 object 91 (segment 523); DROW4 object 5 (segment 147); FLAME object 10 (segment 654) |
| 24 (0x18) | DROW3 object 63 (segment 482); FLAME object 7 (segment 657) |
| 25 (0x19) | FLAME object 4 (segment 642) |
| 26 (0x1A) | DROW5 object 181 (segment 712); FLAME object 11 (segment 653) |
| 38 (0x26) | MYRKUL object 45 (segment 441) |
| 127 (0x7F) | THIEF2 object 93 carries it (type 2, segment 383) |
| 101 (0x65) | IBBALAR object 12 (segment 203) |
| 102 (0x66) | KOBOLDS object 64 (segment 155); KOBOLDS object 65 (segment 378); KOBOLDS object 66 (segment 419); THIEF0 object 68 (segment 199); THIEF1 object 107 (segment 638); PRISON object 82 (segment 712); PRISON object 83 (segment 713); MULHOR1 object 93 (segment 101); MULHOR1 object 94 (segment 134); MULHOR1 object 95 (segment 325); HUB3 object 1 (segment 107); MYRKUL object 106 (segment 305); CLAN1 object 143 (segment 6); CLAN1 object 144 (segment 49); DROW1 object 94 (segment 73); DROW2 object 62 (segment 229); DROW3 object 140 (segment 71); DROW3 object 141 (segment 507); HUB4 object 1 (segment 67); IBBALAR object 119 (segment 381); IBBALAR object 120 (segment 586); IBBALAR object 121 (segment 282); IBBALAR object 122 (segment 297); IBBALAR object 123 (segment 273) |
| 103 (0x67) | none: given by a script or by Khelben |
| 104 (0x68) | DROW5 object 176 (segment 301) |
| 105 (0x69) | IBBALAR object 21 (segment 497) |
| 106 (0x6A) | BEHOLDER object 110 (segment 798) |

## 12. The global flags of the scripts

Flags 1 to 19 are read as copies of the mission flags; flags 33 to 51 set
the mission flags 1 to 19 (scripts.md section 7). The flags 129 to 160
belong to Drowslayer (script 509).

| Flag | Set or cleared by script | Read by script |
|---|---|---|
| 1 (copy of mission flag 1) |  | 1, 6, 510 |
| 2 (copy of mission flag 2) |  | 1, 2, 6 |
| 3 (copy of mission flag 3) |  | 1, 2, 4, 5, 6, 60, 250, 510 |
| 4 (copy of mission flag 4) |  | 170 |
| 7 (copy of mission flag 7) |  | 3, 6 |
| 9 (copy of mission flag 9) |  | 1, 3, 6 |
| 10 (copy of mission flag 10) |  | 4, 5, 7 |
| 11 (copy of mission flag 11) |  | 1, 2, 4, 5, 6, 8 |
| 12 (copy of mission flag 12) |  | 2, 4, 5 |
| 13 (copy of mission flag 13) |  | 2, 3, 8 |
| 14 (copy of mission flag 14) |  | 1, 7 |
| 15 (copy of mission flag 15) |  | 1, 2, 4, 5, 8 |
| 17 (copy of mission flag 17) | 320 | 510 |
| 18 (copy of mission flag 18) |  | 1 |
| 19 (copy of mission flag 19) |  | 1, 2, 510 |
| 33 (mission flag 1) | 23 | 1, 60, 294, 510 |
| 34 (mission flag 2) |  | 2 |
| 35 (mission flag 3) | 70 | 1, 2, 4, 5, 6, 61, 250, 255, 294, 510 |
| 39 (mission flag 7) |  | 3, 6 |
| 41 (mission flag 9) |  | 3 |
| 42 (mission flag 10) |  | 4, 5, 7 |
| 43 (mission flag 11) |  | 1, 2, 4, 5 |
| 44 (mission flag 12) |  | 2, 4, 5 |
| 45 (mission flag 13) |  | 2, 3, 8 |
| 46 (mission flag 14) |  | 7 |
| 47 (mission flag 15) | 54 | 2, 4, 5, 8 |
| 51 (mission flag 19) |  | 2 |
| 65 | 50, 51, 60 | 50, 51, 52, 60, 70, 71 |
| 66 | 54, 60, 70 | 1, 60 |
| 67 | 60, 70 | 1, 50, 52, 54, 55, 60, 61, 62, 63, 70, 71, 251, 254, 255 |
| 68 | 71 | 50, 51, 52, 70 |
| 69 | 31, 54, 60, 273 | 54, 60, 294, 323, 508 |
| 70 | 31 | 8 |
| 71 | 24 | 296, 510 |
| 72 | 2 | 90 |
| 73 | 40 | 90 |
| 74 | 3 | 211 |
| 75 | 7 | 296 |
| 76 | 321 | 296 |
| 77 |  | 60, 508 |
| 97 | 20, 110, 111, 120, 121 | 2, 3, 6, 21, 31, 70, 121 |
| 98 | 292 | 291, 292, 294, 295 |
| 99 |  | 3 |
| 100 | 211 | 3, 323 |
| 101 |  | 3 |
| 103 |  | 1 |
| 106 | 6 | 6, 221, 323 |
| 107 | 210, 213 | 1, 4, 5, 7, 60, 210, 213, 510 |
| 109 | 7 | 7 |
| 110 | 250, 254 | 3, 70, 254 |
| 111 | 1, 3 | 1, 3 |
| 112 | 254 | 2, 254 |
| 113 | 2 | 2, 60, 254, 510 |
| 114 | 291 | 292 |
| 115 | 290, 291, 292, 293, 294, 295, 296 |  |
| 116 | 290, 291, 292, 293, 294, 295, 296 | 292, 321 |
| 117 | 290 | 292 |
| 118 | 292 | 291 |
| 119 | 1 | 1 |
| 128 | 321 | 320, 323 |
| 129 | 509 | 509 |
| 130 | 509 | 509 |
| 131 | 509 | 509 |
| 132 | 509 | 509 |
| 133 | 509 | 509 |
| 134 |  | 509 |
| 135 | 509 | 509 |
| 136 | 509 | 509 |
| 137 | 509 | 509 |
| 138 |  | 509 |
| 139 | 509 | 509 |
| 140 | 509 | 509 |
| 141 | 509 | 509 |
| 142 | 509 | 509 |
| 143 | 509 | 509 |
| 144 | 509 | 509 |
| 145 | 509 | 509 |
| 146 | 509 | 509 |
| 147 | 509 | 509 |
| 148 | 509 | 509 |
| 149 | 509 | 509 |
| 150 | 509 | 509 |
| 151 | 509 | 509 |
| 152 | 509 | 509 |
| 153 | 3 | 509 |
| 154 | 202 | 509 |
| 155 | 200 | 509 |
| 156 | 211 | 509 |
| 157 | 272 | 509 |
| 158 | 270 | 509 |
| 159 | 271 | 509 |
| 160 | 509 |  |

## 13. Tests

`python tools/questtest.py --exe build/engine/main/ascent.exe --datadir
E:/UNDAMONTAN/DTU --datadir D:/dev/ASCENT/gamedata/cd/DTU` runs 36 tests
(2026-11-22: all pass). Each test starts a level, sets the state with the
test commands of `-chartest` (dtuchar.c: `give`, `gflag`, `mflag`,
`mission`, `kill`, `killscript`, `goseg`, `goside`, `goobj`, `aim`, `face`,
`activate`, `talk`, `talkscript`, `objflag`, `resume`, `warp`, `prison`,
`setlevel`, `save`, `load`, `quest`, `obj`, `wall`, `items`) and checks the
log lines.

- The mission flags 1 to 14 and 17 through their DTU.WRP commands, in
  the order of the campaign: the creature with the script dies, the player
  enters the segment, the flag is set (flag 1 also with the creature alive:
  no flag; flag 19 is not set while the Spectator Stone of IBBALAR stands).
- HUB1: the Violet Gem Key opens the wall of segment 40.
- FLAME: the seven pieces and script 323; the Flamesword with Lolth alive
  (no end) and dead (the end, CREDITV1.MVE).
- IBBALAR: the use action on the altar runs script 294.
- YPINN: a chair does not talk (0xA674A clears the old script ids of the scenery).
- THIEF1: the prison sequence.
- A script warp keeps the level state; the spell effects stay after a
  warp; Drowslayer runs script 0x1FD before a warp; statement 9F.
- The Ankh of MULHOR1 and Straum of BONES.
- Save and load in the middle of a quest (the keys Alt+F2 and Alt+F3 with
  no menu, the state is changed between the save and the load): the HUB1
  wall with flags and the mission; three pieces in the statue of FLAME,
  then the other four and the wall; the dead kobold chief and command 1;
  the talk counter of Durnan; the prison (THIEF2, no gold, 1 hit point, no
  items) and the items that lie in THIEF1 after the way back.

Not tested: a whole play of the campaign by hand, the conversations of the
quest scripts reply by reply (only script 31 label 700 and script 322), the
market stock of the rewards, the levels HUB2 to HUB4 with the keys of
Khelben, and the saves of the DOS game in the middle of these quests.
