# Audit: the world

Scope: the levels and the warps between them, the level state across visits
and saves, triggers (TIG), traps (TAP), the GAB scripts, doors, locks and
secret doors, the movement of the player, and the creatures (AI, senses,
morale, spells and missiles of creatures). Files: `dtuscript.c`, `dtutrig.c`,
`dtustate.c`, `physics.c`, `fvi.c`, `ai.c`, `dtuobj.c` (creatures),
`gameseq.c`, `wall.c`, `fuelcen.c`.

Method: the functions of DTUGAME.EXE 1.3 were read in
`gamedata/re/dtu_flat.bin` with `tools/dtudis.py` and compared with the
port: the level start 0xAD5BF and its level fixes 0xACCDB, the level rules
of the restore 0x5A855, the end of the trigger check 0x939AD, the end of
`wall_open_door` 0x7D77E, the warp 0x59C3C, `run_script_id` 0xAC055 and the
prison sequence 0xAA3F3. The objects, script words and triggers of the
levels were read from the level files (`tools/lvl.py`, LV2 where the 1.3
patch has one, else the LVL of the EGG archives) and the TIG files. The
scripts were read with `tools/script.py dis GABE.RLH`.

Tests (all hidden and muted, scratch user folders, `-datadir E:/UNDAMONTAN/DTU
-datadir gamedata/cd/DTU -nomovies`):

- Soak: the 33 levels of DTU.MSN that exist (PLAYGRND and MADLANDS are not in
  the data; IBBALAR1 and SPIDER have files but no line in DTU.MSN), seeds 1,
  2 and 3, 3000 frames each (99 runs), before the level fixes; then seed 7,
  1500 frames each, after them (33 runs).
- `-testtour` on the 33 levels, 3500 frames, `-fixedfps 30 -triglog`.
- `-statetest` warps (FLAME to HUB4 and back), `-statetest 478,4` (the door of
  Lolth), `-testpos` and `-testmove use` (the statue of FLAME).
- A load of a copy of the save slot 1 of the user (`SG0.SG`, BONES, with the
  level files of `SAVE\`) through `-fescript`.

Results of the soak: 99 runs before the fixes, 0 crashes, 0 `SOAK FAIL`, 80
runs reached 3000 frames, 19 ended with the death of the player (a spell of a
creature above the hit points). After the fixes: 33 runs, 0 crashes, 0
`SOAK FAIL`, 28 reached 1500 frames, 5 deaths. The only warnings were the
known `Int3` of object.c 2264 (objects outside the mine in BEHOLDR2 and
MYRKUL, bugs.md P6). The KOBOLDS exit with code 1 did not come back in 4
KOBOLDS runs and the tour.

## Findings

Most important first.

| Id | Finding | Evidence | Status |
|---|---|---|---|
| W1 | **Quest blocker: the last level could not be finished.** The level fixes 0xACCDB were not in the port. In FLAME they give object 4 (MISC 25, the "Assembled Spider") script 0x143 (323) and object 3 (the Flamesword) script 0x142 (322); the level file has no script for either. Script 323 is the statue puzzle: trigger 27 (seg 279 side 5) runs it, it checks the eight spider pieces and opens the wall of seg 278. Script 322 (trigger 28, seg 486) ends the game after the death of Lolth. Without them the trigger found no script and nothing happened. The same function gives scripts to THIEF2 object 1 (71), DROW2 object 42 (213: the bats of the hall), BEHOLDER object 109 (255: the guard of the thieves' master) and DROW5 object 13 (273: the fountain), makes NPCs of BONES, THIEF0, THIEF2, MINE, CRYPTS, MYRKUL and IBBALAR peaceful, removes WEAPON 13 from BANE (object 53) and changes table entries (Lolth and Halaster in FLAME, CLAN3, DROW5). | 0xAD716 calls 0xACCDB; FLAME cases 0xAD3BB to 0xAD5BA (`obj 3 +0x1B0 = 0x142`, `obj 4 +0x1B0 = 0x143`). `tools/lvl.py FLAME.LV2`: objects 3 and 4 have script word 0. Test `-level FLAME -testpos 0,-7.92,452.5,0 -testmove wait:20,use:5,wait:100 -scriptlog`: before, "trigger: obj 4 has no script"; after, "run script 323 of obj 4 (Assembled Spider) in mode 3" and the list of the pieces that are not in place. Soak seed 7 logs all 22 object changes of the 16 levels. Table in objects.md, "Level fixes (0xACCDB)". | Fixed, f8a0af9. Not ported: object flag 0x40, the run time texture of BANE, the times of KOBOLDS object 19 (see W12). This also closes the 0xACCDB part of combat C4 |
| W2 | The trigger rules of a level restore (0x5A855(0)) were not in the port. After the restore of a saved state: KOBOLDS trigger 1 off, THIEF2 triggers 43 and 46 on and trigger 46 copied from 43 (trap 17 in place of 18), DROW2 trigger 20 runs the script of object 42, FLAME triggers 26, 32 and 33 (the one-time triggers of Lolth) on again. | 0x5A901 to 0x5AA5D. Test `-level FLAME -statetest -1,0,-1,1,62 -triglog`: "Restoring TEMP32.LVL." then "level rule of the restore (0x5A855), level 32". levelstate.md section 5. | Fixed, 51398b8. The THIEF1 rule writes a byte of the trigger position (a defect of the original with no seen effect) and is not ported |
| W3 | The Lolth rules of FLAME were not in the port: after the triggers 33, 32 and 26 the executable runs `run_script(object 36, 0)` while the script variable of Lolth is below 1, 2 and 3 (0x93ADE); when the door of seg 478 side 4 opens and the variable is below 3, the door stays open and Lolth talks 5 seconds later (0x7DBF5). | Test `-level FLAME -statetest 478,4,-1,-1,-1 -triglog -scriptlog`: "level rule of flame: the door of seg 478 stays open", wall flags 0x0, then "run script 320 of obj 36 (avatar) in mode 0" and the first line of Lolth. triggers.md sections 2 and 6. | Fixed, 51398b8 |
| W4 | The YPINN soak hangs at frames 256 and 302 (seeds 1 and 3) were the question "Leave the Yawning Portal?" (DTU.WRP command 0): the soak walked into the exit segment, and the message box waited for a key in the hidden window. Not a hang of the game. | gdb on the hung process: `nm_messagebox` from `dtufe_game_frame` (dtufe.c 2884), subtitle "Leave the\nYawning Portal?". After the fix seeds 1 and 3 run 1000 frames (seed 1: death at frame 953, seed 3: 0 problems). | Fixed, c9818ea (the soak answers No) |
| W5 | `-testtour` killed the player in most levels ("Player dies ..., killed by nothing"), so the tour ended early: a segment center is in the air, the player starts to fall there, and the next jump of the tour to a lower segment counted the whole height as a fall. | Session logs of the tour: BANE, BEHOLDER, DROW2, YPINN died at 9 to 36 seconds; 19 of 33 levels finished. | Fixed, 28f6471; the tour results are below |
| W6 | The prison sequence 0xAA3F3 (statement DB with segment 0xFFFF) is not in the port. Script 60 (THIEF1, the Shadow Thieves): when the player does not evade, the executable takes the items that are not worn (to the script object, to object 99 or to 8 segments of the prison), moves the player to THIEF2 segment 612 and object 165 to segment 631. The port ends the conversation and the player stays free. Not a blocker (the player keeps the items), but a quest path of the original is missing. | 0xAA3F3 to 0xAA600; `dtuscript.c` logs "the prison sequence 0xAA3F3 is not in the port". | Fixed: `prison_sequence` and `prison_arrival` of dtuscript.c, with `Dtu_fix_prison_items` for the item loop of 1.3 (quests Q6). "Moves object 165" was a wrong reading: 0xAA5BA summons critter row 0xA5 in segment 631 |
| W7 | The warp check 0x59D2D is not in the port: with WEAPON 13 in the hand, a warp to another level makes the weapon talk (script events 0x8B, 0x8C, per level 0x93 to 0x98, 0x8E), runs script 0x1FD, and global flag 0xA0 cancels the warp. | 0x59D2D to 0x59EA9. `dtu_hook_warp_allowed` is the weak function of dtutrig.c that returns 1. | Fixed: `dtu_hook_warp_allowed` of dtuscript.c (quests Q7) |
| W8 | The passage gold of the well is not in the port: a warp from the inn needs 1 gold (and asks with less than 2), a warp to the inn needs 1 gold, and the warp takes 1 gold. | 0x59F19 to 0x5A026, text at 0x13CE3A ("Passage in and out of\nUndermountain costs one gold..."). The texts are in the executable, not in DTUE.TEX: this needs the text work of the localization agent. | Fixed: `well_pay_passage` in dtutrig.c, after D:\dev\DTU-Decomp recovered/world-warp.md; the three texts are in the Spanish catalog |
| W9 | Statement EB (a spell at a segment) does nothing. Scripts 273 (DROW5) and 291 (IBBALAR, 87 casts after "What?!!!") use it. | scripts.md section 10. | Fixed: `dtu_spell_area` around the object of the script (quests Q8) |
| W10 | Statement E5 (walk) moves the object to the end of the path at the end of the run (`Dtu_script_walk_teleport`, a port decision) because the script code has no access to the AI paths. | scripts.md section 10. | Open: the path of the AI is made with `-scriptwalk`, but the creature stood still in the test, so the default still moves the object (quests Q11) |
| W11 | The Lolth hit of FLAME (0x6D184): object 36 hits the player once in each swing, with no roll and no reach test, text "Lolth struck you with a mighty blow!". | 0x6D184 to 0x6D1FE (`cmp eax, 0x24`, 0x29A9C, 0x347A2). | Open, small (combat code) |
| W12 | Parts of the level fixes that are not ported: object flag 0x40 (MULHOR1 misc objects 1, 2, 34, 60, 73, 91; FLAME 2, 3, 35), the `tmap_override` of the two spinning pieces of BANE (a table made at run time), the shields of DROW5 object 83 (the executable reads past the end of the monster table: a defect), +0x7C of the CLAN3 type, the times of KOBOLDS object 19, the control type 0 of MYRKUL object 3 (the Hold effect keeps it in place). | objects.md, "Level fixes (0xACCDB)". | Open, small (flag 0x40 needs its meaning first) |
| W13 | The creature setup by name 0xAC7B2 (`creature_fixes`) is not in the port (lich, mephits, flayers, gargoyles, hatchling, spectator, the `s...` names, 200 hit points for the NPCs of YPINN). | Same as combat C4. | Fixed, 5f68994 (combat C4). The level fixes now run after the object setup and before the creature setup; the KOBOLDS weapon, the CLAN3 touch spell, the YPINN guests, the DROW5 hatchling shields (`Dtu_fix_drow5_shields`), the BANE texture (`Dtu_fix_bane_texture`) and the timers were added; objects.md, "Level fixes" |
| W14 | The sounds of the statements D4 to D7 play at the player, not at the object or side, and the stop statements D6 and D7 do nothing. | `dtuscript.c` 0xD4 to 0xD7. | Fixed: the linked sounds of digi (quests Q10) |
| W15 | Global flag 0x81 of the WEAPON 13 talk and condition 98 (the morale; the port uses 0 + 500) are not ported. | scripts.md section 10. | Global flag 0x81 is set now (the name flag of the creature). Condition 98 has no use in GABE 1.3 (quests Q16) |
| W16 | The thief hold of a wall at the end of a fall (0x9EBEB, "PCONS: you fell!") and the push of a new object above the floor (0x6A490) are not in the port. | physics.md section 2. | Open, small to medium |
| W17 | AI: the evasion of weapons, the alert function with the awareness events, and the revive of a regenerating creature are not in the port; the port has guessed rules in their place (a creature that the player hits becomes hostile; a hostile creature that sees the player keeps awareness 1). | ai.md, "Not in the port". | Open, medium |
| W18 | Objects outside the mine at the level start of BEHOLDR2 (7 objects) and MYRKUL (2): `Int3` in object.c 2264, `fix_object_segs` moves them. | bugs.md P6; each soak and tour of the two levels. | Open, known, no effect seen |
| W19 | The use of the Spider Pieces other than 0x13 is not in the port ("use: Spider Piece 0x%x is not in the port", dtuchar.c). The pieces are placed in FLAME by the triggers 10 to 16 (need item, take item), so the statue is not blocked by this. | dtuchar.c 1494. | Fixed: 0x14 and 0x17 step their frame loops one game frame at a time (dtucombat.c); 0x18 runs script 0x1FC (f396b59). Checked 2026-11-22: the pieces 20 (IBBALAR object 22, seg 193), 23 (DROW1 object 91, DROW4 object 5) and 24 (DROW3 object 63) are level objects, the item table has them, the port picks them up, and with the seven pieces 18, 19, 20, 22, 23, 24 and 26 on the triggers 10 to 16 script 323 opens the wall of segment 278 (chartest gosecret and activate, goside=279.5) |
| W20 | **Test start only: `-level KOBOLDS` ran HUB1.** The player object of KOBOLDS.LVL (object 0) is in segment 12, and DTU.WRP has the line `2,12 1,102 0` (Kobold to Hub1) for that segment. The direct start put the player at the player object, so the warp check 0x59C3C fired in the first frame and loaded HUB1; the log had no line for the cause. The game never starts there: a new character starts in YPINN at its player object (0x5895E), every other level is entered by a warp, which puts the player at the center of the destination segment (0x5A268), or by a save game. A real arrival from HUB1 (`1,91 2,1 2`) goes to KOBOLDS segment 1 and stays. | `tools/lvl.py KOBOLDS.LVL --objects` (unpacked from KOBOLDS.EGG): `0 player seg=12`. Before: `-level KOBOLDS -frames 120` logs kobolds.lvl, then hub1.lvl. A walk in HUB1 into segment 91 (`-level HUB1 -testpos -244.97,-307.34,226,0`, fescript `gamekeys`, `hold up 200`) loads KOBOLDS and stays 10 s. `-level X -frames 300` on the 28 levels of DTU.MSN with data: only KOBOLDS changed level. After the fix the start segment differs from the player object in KOBOLDS (1, was 12), HUB3 (7, was 3), HUB4 (50, was 51), CLAN3 (131, was 94), IBBALAR (790, was 723) and FLAME (2, was 3); DROW4 has no line into it and keeps the player object; no level changes level. | Fixed: the direct start of a level other than level 0 puts the player where the first DTU.WRP line into the level (file order, from another level) puts it (`dtu_trig_place_at_arrival`, log line `Level start: segment ...`). `-startobject` keeps the player object. The session log now names each level change (`Level change: DTU.WRP line ...`, the script teleport, the exit of the inn) |

Not found: no crash, hang or `SOAK FAIL` in the world code in 132 soak runs
and the tour; no quest blocker other than W1. The levels with no line in
DTU.MSN (IBBALAR1, SPIDER) were tested with `-level`.

## The tour

`-testtour` after W5, 3500 frames at 30 per second: the tour visited every
segment of 31 of the 33 levels (98 to 800 segments). DROW3 (killed by a
flayer) and DROW5 (killed by a creature of type rmmk5) ended before the last
segment: the creatures attack the player at the centers of their rooms. In
CLAN3 the player fell from a segment center into a warp segment and the tour
went on in CLAN1 (747 segments). Other deaths came after the end of the tour
(BAT, BONES, CLAN2, MINE, MULHOR2). No crash, no stuck tour, no `Error`.

## Notes for the other work

- The text paths: no change. The new code prints only log lines.
- Combat C4: the 0xACCDB half is done here (W1); `creature_fixes` 0xAC7B2 is
  fixed in 5f68994 (W13).
- The soak answers the inn exit question with No (`Dtu_soak` in dtufe.c, the
  front end file); nothing else in dtufe.c changed.
