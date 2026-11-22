# Audit: the quests

Scope: can the campaign of 1.3 be finished in the port. The script
statements and conditions, the trigger and trap fields that the shipped
levels and scripts use, the warps of the scripts, the level state across a
save, and the end. Files: `dtuscript.c`, `dtutrig.c`, `dtustate.c`,
`dtuhook.c`, `dtudlg.c`, and the quest parts of `dtuobj.c`, `dtuchar.c`
and `dtufe.c`. The quest graph is in [../re/quests.md](../re/quests.md).

Method: the scripts were counted with `python -I tools/script.py stats
GABE.RLH` and the listing of `dis` (GABE of the 1.3 install, 87 records),
the TIG and TAP files of the 28 levels that have them with a parser of the
formats of docs/formats/tig.md and tap.md (816 triggers, 331 traps). The
functions of DTUGAME.EXE 1.3 were read with `tools/dtudis.py` and in
D:\dev\DTU-Decomp: the statement handlers 0xA9BE6 to 0xAA908, the prison
sequence 0xAA3F3, the teleport 0xAA1FC, the warp check 0x59C3C, the end of
a run 0xABFC4, the kill 0xA6ACB, the talk 0xAC155, the level start loop
0xAD786, the level statement 0x8BA3A and the inventory functions 0xB6D24
and 0xB6F01. Tests: `tools/questtest.py`, 36 tests, all pass (2026-11-22).

## Findings

Most important first. "Before" is the port before this audit.

| Id | Finding | Evidence | Status |
|---|---|---|---|
| Q1 | **Quest blocker: the game could not be ended.** Script 322 sets the hit points of the Flamesword (FLAME object 3, scenery) to 0 with statement BC; the end of a run kills an object with less than 1.0 hit points, and that kill starts the end sequence. The port killed only creatures there. | 0xAC033 (`cmp [ebp+0x6c], 0x10000`, no test of the type), 0xA6AF8 (level 0x20, object 3: 0xA6786). Test `flame_ending`: before, nothing; now "end of the game: CREDITV1.MVE" | Fixed (`kill_after_run`, `obj_kill` of dtuscript.c) |
| Q2 | **Quest blocker: scenery with a script could not be used.** The use action ran a script only for a creature. The Flamesword is used that way (Lolth switches the floor trigger 28 off in her first speech, script 320 at 000AA), and so is the altar of Cyric (IBBALAR object 2, script 294). | 0xAC155: the object of the use ray 0xB30FD with a script id runs it, with no test of the type. Tests `flame_ending`, `flame_sword_lolth_alive`, `ibbalar_altar` | Fixed (`dtu_use_object` of dtuhook.c). The use ray tests scenery with the object test of the query of the executable (`dtu_body_query_test`); the test of the body move found the altar under the sword first |
| Q3 | The chairs, tables and beds of YPINN have script ids of guests (old data of the level editor). 1.3 never runs them: on a level with no saved state the level start clears the script word of every object that is not a creature (0xA674A, called at 0xAD63B) before the level fixes give scenery its scripts. A use, a hit or the break of such a chair runs no script in 1.3, and no guest turns hostile (the scenery damage of 0x29A9C has no attacker and reads no script word). The port did not have the clear, so its chairs talked; `Dtu_fix_furniture_talk` hid that. | YPINN.LVL objects 5, 8, 12 to 15, 19 to 22, 26 (ids 7, 2, 4, 3); 0xA674A `cmp byte [eax+0x2b0618], 2` / `mov [eax+0x2b07c4], 0`; DTU-Decomp `docs/resolved-2026-11-23.md`. Test `ypinn_chair` | Ported as in 1.3 (the clear in `dtu_objects_level_setup` of dtuobj.c). Not a defect of 1.3: `Dtu_fix_furniture_talk` and `-origtalk` are gone (D-074) |
| Q4 | **The warp of a script lost the level state.** Statement DB to another level started the level new: the level that the player left was not saved, and the level of the arrival came from its file, so a level that Halaster (script 296) or Straum (script 31) sends the player back to was whole again. | 0xAA385 to 0xAA3A5: DB writes a warp line into the segment of the player and calls 0x59C3C. Test `script_warp_state` | Fixed (`dtu_trig_script_warp` of dtutrig.c, `warp_go`). The warp of a script also pays the well and asks WEAPON 13. Level 0xFFFF is a command of DTU.WRP (0xAA33A) |
| Q5 | The spell effects of the player ended at each warp: the level start made the player object new. | 0x59C47 copies the player object, 0xBB563 puts 27 fields back (flags, hit points, +0x80 to +0xF8, +0x1B0). Test `warp_keeps_effects`: before, the bits +0x88 were 0 after the warp | Fixed (`warp_go`). The hit points of the frame of the warp are also kept now |
| Q6 | The prison sequence of the Shadow Thieves was not in the port (world W6). | 0xAA3F3 to 0xAA66B. Tests `thief1_prison`, `save_prison` | Fixed (`prison_sequence`, `prison_arrival`). The item loop of 1.3 has a defect: `Dtu_fix_prison_items` (on), option `-origprison` |
| Q7 | The check of WEAPON 13 before a warp was not in the port (world W7), and the talk with it in the hand stopped when script 0x1FD waited for a reply. | 0x59D2D to 0x59EA9, 0xAC206. Test `drowslayer_warp` | Fixed (`dtu_hook_warp_allowed` of dtuscript.c, `After_kind`). Global flag 0x81 of the talk is set from the name of the creature (world W15) |
| Q8 | Statement EB (an area spell) did nothing (world W9). 87 uses. | 0xAA8B3: 0x90CEF(spell, object, object) | Fixed. Not tested in a level |
| Q9 | Statement 9F (the level) did nothing. One use: the favor "Power" of Straum (script 31 at 03098). | 0xAB71A, 0x8BA3A. Test `level_statement` | Fixed (`dtu_char_set_level` of dtuchar.c). The thief points of 0x8BAD6 are not changed |
| Q10 | The sounds of D4 and D5 played at the player, D6 and D7 did nothing (world W14). 14 uses. | 0xA9BE6 (0x2E627), 0xA9CBE (0x2E94B), 0xA9DD2 (0x2E816), 0xA9DEC (0x2EB77) | Fixed (the linked sounds of digi). Not tested by ear |
| Q11 | Statement E5 (walk, world W10): the object is moved to the end of the path. The path of the AI is now made with the option `-scriptwalk`, but the creature did not move on it in the test (KOBOLDS object 8, script 21), so the option is off. In 1.3 a creature that is not hostile walks to the player and not along the path (0x17CFC), so the walk is broken there too. | 0xAA74E, 0xAA79F, 0x17411. 16 uses in 7 scripts, all creatures with behavior 0x81 | Open, small: why `follow_path` does not move the creature |
| Q12 | The light and the 50 sounds before the credits movie (0xA6786) are not in the port. | docs/re/movies.md | Open, cosmetic |
| Q13 | The warp overlap check 0xAD9B2 (a creature in the segment of the arrival is moved) is not in the port. | D:\dev\DTU-Decomp analysis/sweep/range2-gaps.md G10 | Open, cosmetic |
| Q14 | Object flag 0x40 of the level fixes (world W12): six of the seven Ankhs of MULHOR1. In the port each of the seven could be taken. The flag takes an object out of the object queries (0x3F38F), so in 1.3 only object 33 (segment 585, behind the door of the Ruby Key) can be taken; the six decoys are still drawn. No code chooses one at random. The same flag holds the Flamesword of FLAME until the death of Lolth, which runs script 322 itself (0x6A374). | 0xAD0C0, 0x3F380 to 0x3F392, 0x6A579, 0x6A374. Tests `mulhor1_ankh`, `mulhor1_ankh_flagged`, `flame_sword_lolth_alive`, `flame_lolth_death`, `flame_ending` | Fixed (`OF_DTU_NOHIT`, `fix_nohit`, `flame_lolth_dead` of dtuobj.c). Intended: no setting |
| Q15 | The floor triggers of the segment of the player fire again when a script teleports the player (0x59EB9 in the warp check). | 0xAA3A5 | Not ported: the same class as `Dtu_fix_trigger_repeat` |
| Q16 | Condition 98 (the morale, world W15) uses 0 + 500. | No script of GABE 1.3 uses condition 98 | No effect |
| Q17 | The market stock of the mission rewards (`dtu_hook_market_add_stock`). | scripts.md section 8 | Not checked here (the front end) |

## Script statements (GABE 1.3)

| Code | Name | Uses | Port |
|---|---|---|---|
| 99 to 9E | abilities | 31 | yes |
| 9F | level | 1 | yes (Q9) |
| A0 | hit points | 38 | yes |
| B3 to BD | animation state | 75 | yes. BC (die) with the kill at the end of the run (Q1) |
| BE | narration | 268 | yes |
| BF, C0 | give, take | 290, 64 | yes |
| C1 | experience | 57 | yes |
| C2, C3 | anger | 41, 34 | yes |
| C4 | trigger | 51 | yes |
| C5, C6 | flags | 644, 133 | yes |
| C7, C8, CC | goto, end | 982, 699, 1540 | yes |
| CD | say and replies | 1816 | yes |
| D3 | sound | 48 | yes. 0xFFFF is the movie FLAM1.MVE |
| D4, D5 | sound at an object, at a side | 9, 2 | yes (Q10) |
| D6, D7 | stop a sound | 2, 1 | yes (Q10) |
| DA, DC | teleport an object | 44, 109 | yes |
| DB | teleport the player | 69 | yes (Q4, Q6) |
| DD | move an item | 10 | yes |
| DE | summon | 45 | yes |
| DF, E0 | resume label, variable | 128, 104 | yes |
| E5 | walk | 16 | partial (Q11) |
| EA | spell at an object | 130 | yes |
| EB | area spell | 87 | yes (Q8) |
| EC | wait | 206 | yes (game time, the world stands) |
| ED | trigger off, kill | 39 | yes. A kill of scenery breaks it |
| EE | call | 10 | yes |
| F2 to FD | IF, ELSE, ENDIF | 1406, 686, 1406 | yes |

Conditions (terms of IF and of reply lists, lines of the listing that use
them): object flags, alignment 77, class 92, race 58, sex 14, talk counter
106, abilities 186, level 14, hit points 11, mission 14, object number 11,
run mode 23, anger 30, trigger on 23, dead 185, carries 7, player has 130,
resume label 89, variable 120, segment 6, fire count 5, visited 59, global
flag 214, random 324. All are in `sc_term`. Condition 98 has no use.

## Triggers (816 records of 28 TIG files)

| Field | Records | Port |
|---|---|---|
| activation masks FD, FF, F5, F4, FC, F1, F8, F2, F9, FA | 625, 119, 33, 10, 10, 7, 6, 4, 1, 1 | yes |
| flags 1, 2 (occupancy), 4 (texture toggle) | 2, 1, 20 | yes |
| need item, take item, lack item | 63, 29, 7 | yes |
| player masks a, b | 8, 2 | yes (`dtu_hook_player_masks_ok`) |
| message | 580 | yes |
| object (script in mode 3) | 15 | yes |
| textures on and off | 92 | yes |
| skip count | 4 | yes |
| first trap | 275 | yes |
| enable trigger, disable trigger | 6, 277 | yes |
| enable trap, disable trap, sound, `unk30`, conditions 35, 37, 39 | 0 | code is there, no data |
| off at the start | 5 | yes |

## Traps (331 records of 28 TAP files)

| Field | Records | Port |
|---|---|---|
| flag 0x001 open the door | 75 | yes |
| flag 0x008 lock | 100 | yes |
| flags 0x010, 0x020 wall closed, wall open | 11, 105 | yes |
| flag 0x040 repeat | 20 | yes |
| flags 0x100, 0x200 wall toggle | 9, 3 | yes |
| flag 0x8000 recharge | 5 | yes |
| flag 0x002 close the door | 0 | `Dtu_fix_trap_close_door` |
| weapon 7, 8, 11, 15 | 15, 4, 1, 1 | yes (`dtu_hook_trap_weapon`) |
| spell 6, 16, 32 | 1, 1, 7 | yes (`dtu_hook_trap_spawn_object`) |
| creature | 42 | yes (`dtu_hook_trap_gate_robot`; no record has an argument below 4) |
| hit points | 1 | yes |
| textures | 7 | yes |
| interval, delay | 27, 55 | yes (game time, see `Dtu_fix_trap_seconds`) |
| limited charges | 180 | yes |

No trigger or trap field that the files use is missing in the port.

## The soak and the quest order

The soak test (`-soak`) plays random input in one level and cannot follow
a quest. The quest order is tested by `tools/questtest.py` (quests.md
section 13): it sets the flags and items, enters the level, reaches the
goal and reads the flag. No level failed.

The soak after the changes of this audit: the 31 levels with a level file
(`-level NAME -soak 7 -fixedfps 30`, 1000 frames, 600 frames for some of
the levels from MULHOR1 on): 0 crashes, 0 `SOAK FAIL`, exit code 0 in each
run; 26 runs reached the frame limit, 5 ended with the death of the player
(YPINN, HUB1, DROW1, DROW2, MYRKUL).

## Save and load

Tests `save_hub1_key`, `save_flame_statue`, `save_kobolds_chief`,
`save_durnan_talk`, `save_prison`: the global flags, the mission flags,
the mission of Khelben, the items, the walls, the triggers that are off,
the dead creatures, the script state of an object (+0x1B0) and the level
state of a level that the player left before the save all come back.
Not tested: a save while a script waits for a reply, while a creature is
on a path of statement E5, and while a warp or a talk waits for script
0x1FD (these three states are not written to a save).
