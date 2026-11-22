# Audit: character rules

Scope: races, classes, two classes, ability scores, hit points, THAC0,
saving throws, experience and levels, thief skills and their use in the
world, racial abilities, rest, death, character creation and the
character record of the save games (`dtuchar.c`, `dtuthief.c`,
`dtuthiefpts.c`, `dtufe_char.c`). Spells, items, the attack interval,
creatures and quests belong to other audits.

Method: the functions of DTUGAME.EXE 1.3 were read again in
`gamedata/re/all.asm` and in the C of the decompilation
(`D:\dev\DTU-Decomp\local\decompiled\functions`). The tables were dumped
from `gamedata/re/dtu_flat.bin` (object 1 at 0x10000) and compared with the
tables of the port. The rules are in `docs/re/character.md`, section 15.

Tests: `-chartest rules` (new) builds each of the 36 race and class
combinations of the character generation with fixed statistics and checks
it at levels 1, 5 and 10 against the tables of the executable: hit points
(level 1 exact, then the range of the dice), class levels, THAC0, the five
saving throws, the five thief skills and the thief points. Result: 36
combinations, 1482 checks, 0 failures, also with `-origchar`. Other runs:
`-chartest setlevel=5`, the racial search on BONES (`race=1`,
`gosecret=0..2`, `search`), the texture bits on eight levels, and `ctest`
(16 of 16).

Status words: Equal (the port does what 1.3 does), Fixed (this audit
changed the port; the commit is on the branch
`worktree-agent-a07ed1ab227979f3c`), Setting (a defect of 1.3 with a
`Dtu_fix_` setting; `-origchar` gives the 1.3 rule), Open, Not ported.

## Rules

| Rule | Original | Port | Status | 2e (Player's Handbook) |
|---|---|---|---|---|
| Races and allowed classes: human one class; half-elf 9 masks; elf and drow 7; dwarf 5; halfling 4; no cleric/thief, no three classes | 0x88B25, tables 0x13FECE, 0x13FEE6 | `combo_allowed` (dtufe_char.c) | Equal | 2e allows three classes for some races and cleric/thief for half-elves; no level limits in DTU (see below) |
| Ability scores: 3d6 with ones rolled again (6 to 18), 7 bonus points, racial modifiers and limits, class minimums 9 | 0x89A77, 0xB0CDB, 0x89B95, 0x13FD8F, 0x8B71C | dtufe_char.c, frontend.md section 5 | Equal | 2e: 3d6 in order or methods I to VI; the manual of DTU says 5 points |
| First level hit points: full hit die + constitution, the mean of two classes | 0x8BA3A (0x8BB17) | `make_player` | Equal | 2e rolls the first die |
| Hit points of a new level: one die as `rand() % (die - die/2 + 1) + die/2`, a die up to level value 9 (cleric, fighter) or 10 (mage, thief), then 2, 1, 3, 2; the constitution bonus at every level; the sum halved for two classes | 0xAF694, tables 0x146518, 0x146528, 0x146538, 0x1402E2, 0x140332 | `level_hit_points`, `check_levels` | Equal | 2e: 1 to die; the constitution bonus stops after level 9 (10 for wizards and rogues); the die stops one level earlier in DTU terms (DTU gives a die at level 10 for a cleric and fighter) |
| The hit points in use at a new level: 1.3 adds the integer to the fixed point hit points (P+0x53 and the object), so they grow by n / 65536; the maximum grows by n | 0xAF8C7, 0xAF8CD | `check_levels` | Setting `Dtu_fix_level_hit_points` (new) | 2e: the new hit points are added |
| Experience tables, maximum level 18 (level value 17); a level value above 18 becomes 17 | 0xAF43A, table 0x146048 | `check_level`, `Xp_table` | Equal | 2e has racial level limits for demihumans (dwarf fighter 15, elf mage 15 and so on): DTU has none, the port follows DTU |
| Two classes: experience divided by 2 (at least 1) for each class | 0xAF3CF | `dtu_char_award_xp` | Equal | 2e: the same |
| Experience for a kill (`score_value`), a trigger (+0x30), script statement C1, the quests of Khelben | 0x4EA65, 0x93D8E, 0xAAE93, 0x8452B to 0x8472E | dtuobj.c, dtutrig.c, dtuscript.c, dtufe_town.c | Equal (spot check of the calls) | 2e: story awards are at the DM's choice |
| Level-up box for each class, TEX 844 | 0xAF560 | `check_levels` | Equal | |
| THAC0 by class and level value + 1; two classes: 1.3 takes the worse value | 0xAF605, table 0x146590 | `update_thac0` | Setting `Dtu_fix_multiclass_thac0` (existing) | 2e: the better value |
| THAC0 at level 18 (level value 17): index 18 is past the 18 bytes of each row; 1.3 reads the next row (20, 72, 20, 20) | 0xAF605 | `update_thac0` clamps to 17 | Not ported (no setting: 2 250 000 experience or more) | |
| Saving throws: level = higher class level, at most 12; row (level - 1) / 3, 5, 2, 4; two classes: the better number; no racial, constitution or wisdom modifiers | 0xAE596, tables 0x1456D2, 0x1456E2 | `dtu_spell_save` (dtuspell.c) | Equal | 2e: dwarves and halflings get +1 per 3.5 constitution against rods, wands, spells and poison; DTU has none |
| Set level (script statement 9F, the new character): levels, the start experience of the level, hit points (die + con) * level, THAC0, 60 + 30 * level value thief points | 0x8BA3A, 0xAB71A | `dtu_char_set_level` (new), dtuscript.c 0x9F | Fixed (was "not in the port") | |
| Thief points of a new thief: 60 | 0x8BACD | `make_player` | Fixed (was 0; Mirt sets the points again before they can be spent, so only the save record changes) | 2e: 60 discretionary points at level 1 |
| Thief points at a new level: Mirt sets 20 per thief level since the last visit, 20 more at the first visit; at most half of them in one skill per visit; skills at most 95 | 0xABB53, 0xB5B49, 0xB5BB9, 0xB4ECF | `thief_points` (dtuscript.c), dtuthiefpts.c | Equal (the open item of combat.md C6 is done) | 2e: 30 points per level, at most 15 in one skill |
| Thief skills of a new character: base 5, 10, 10, 5, 60 (Hide, Move, Locks, Traps, Climb) + race + dexterity; a dwarf who is not a thief has Find Traps 50 | 0x8BDE5, tables 0x146078 to 0x146228 | `make_player` | Equal | 2e: the same base, race and dexterity tables; DTU gives -50 below dexterity 9 |
| Pick Pockets, Detect Noise, Read Languages | none (no string, no code) | none | Equal (not in 1.3) | 2e thief skills |
| Open Locks: key O, ray 20, 0.5 s, one try per lock and thief level | 0xB31BD | `dtu_thief_open_locks` | Fixed: the failed level is now byte 0x17 of the wall record, so a level state and a save keep it (was an array that a new level cleared) | |
| Find/Remove Traps: harmful trap and trigger tests | 0xB34E0, 0xB353A | `trap_harmful`, `trigger_harmful_depth` | Fixed: a trigger with negative experience counts; only the first trap, not its chain; then the enabled trigger, then the enabled trap | |
| Find Traps: a dwarf who is no thief skips sides with side flag & 3 | 0xB40D4, 0xB3504, 0xB356B | `dwarf_skips` | Fixed (the flag was ignored) | 2e: dwarves find traps in stonework |
| Find Traps: the search needs the skill or the spell; the dwarf flag is not a third way | 0xB40AF | `search_frame` | Fixed | |
| Find Traps: quick distance 40.0; side flag 4 (trigger) and 8 (trap) for the automap when found, cleared when removed | 0xB370D, 0xB378E, 0xB38EC, 0xB3ABB, 0xB3C7D | `search_triggers`, `search_traps` | Fixed (the flags were never set) | |
| Find Traps: sound 3001 (and 2130 for a magical trap) only with the spell | 0xB37B6, 0xB3AE7 | `found_sound` | Fixed (the port played it before every box) | |
| Find Traps: the trigger and trap states (+0x0F/+0x10, +0x03/+0x04) in the records | 0xAD6A8, 0xAD6D4 | `Trig_find`, `Trap_find` | Fixed: in the records, saved with the TIG and TAP state files | |
| Find Traps: a magical trap halves the skill and doubles it again, so an odd skill is 1 lower until the next 0xAFB55 | 0xB3A4A, 0xB3CF9 | `search_traps` | Not ported (quirk) | |
| Find Traps: the marker object of the spell for 5 s | 0xB3605 | none | Open, small | |
| Racial search for secret walls: one d6 per wall against human 3, dwarf 6, elf 5, drow 5, halfling 3, half-elf 5; the wall latch byte 0x16; a dwarf skips side flag & 3; closed walls with a trap, doors with flag 0x40 | 0xB3ECE, 0xB3D1E, 0xB3E57, 0xB3D4E, table 0x1466E8 | `racial_search` | Equal to recovered/secret_search.md and the patch; the latch is now byte 0x16 of the wall record (was an array) | 2e: elves 2 in 6 when searching, others 1 in 6 |
| Secret doors: a door with one of 29 door textures gets wall flag 0x40 | 0xA6F89 (0xA747E), called at 0xAD744 | `dtu_thief_level_textures` (new) | Fixed (nothing set the flag: the racial search, "You found a secret door!" at a door and the automap rule never ran). BONES 6, DROW2 4 secret doors | |
| Secret door names that no table has: 23 of the 29 names (`dleft11#0` and so on) are in no table of 1.3; 0x1B2B1 gives texture 0, and a door with texture 0 or no overlay becomes secret | 0x1B2B1 | `Dtu_fix_secret_door_names` | Setting (new). THIEF2 gets 1 more secret door with `-origchar` | |
| Water, lava and wood bits of the side flags (& 3): water 1, lava 3 (not on an illusory side), wood 2, from 6, 4 and 22 texture names | 0xA6F89 (0xA7322 to 0xA7492) | `dtu_thief_level_textures` | Fixed: the port had 0 on every side (gamemine.c clears the byte; the level file has no such byte). This changes the footsteps (dtufoot.c), the trigger sound (dtutrig.c), the climb (water) and the dwarf search | |
| Move Silently: thief, skill, or racial for elf, drow, halfling; metal armor | 0xB4196 | `dtu_thief_move_silently` | Equal | 2e: elves and halflings surprise others |
| Hide In Shadows: one roll at each stop | 0xB333F, 0x9DFDE | `hide_frame` | Equal | |
| Backstab: silent, melee, humanoid, no watcher; +4 to hit, damage x (level + 8) / 4 | 0xB337E, 0x2AB66 | `dtu_thief_backstab_check` | Equal (combat.md) | 2e: x2 to x5 by level |
| Climb Walls: ladders, the skill, slips | 0x951DE, 0x9EBEB | dtuthief.c | Equal (combat.md B7) | |
| Infravision: every race but human (the half-elf flag counts), the spell, the potion, ring 22 for humans | 0x5464B, 0xB0058, 0x8EBFC | dtudisp.c, dtuchar.c, dtuspell.c | Equal (infravision.md) | 2e: 60 feet for demihumans |
| Racial resistances: elf 90% and half-elf 30% against Sleep; Scare saves only for elf, half-elf, cleric | 0x8F7B8, 0x8F4C4, 0x92ADC | dtuspell.c | Equal | 2e: the same for sleep and charm |
| Player mass by race | 0xAE444, table 0x1453EE | none | Equal: the table is all 0 in 1.3, so every race has the minimum mass 4.0 | |
| Rest: segment special 2 or 6, 300 s, full hit points, spells, sound 62 | 0x360D5, 0x3E137 | `dtu_char_rest` | Equal; the alert call 0x160AB is open (combat.md C19) | 2e: 1 hit point per day |
| Death: the death menu, no loss of experience | 0x423F5 | dtufe.c | Equal (frontend.md 8) | |
| Character record of the save game: P+0x1AB3 thief points, P+0x1A53 last paid level, the wall, trigger and trap bytes in the level states | 0xBB867, 0x5A756 | dtuchar.c, dtustate.c | Equal | |

## Not checked here

- The front end screens (dtufe_char.c) beyond the rules in the table.
- The spell slots by level and wisdom (spell audit).
- A DOS run of the secret door and texture rules: the port result was
  compared with the code only.

## Changes to the test options

- `-chartest rules`: the race and class test above. The log has one line
  for each combination and level and the line "rules: N combinations, N
  checks, N failures"; the session log has the summary.
- `-chartest setlevel=N`: 0x8BA3A with level N.
- The session log has "Level textures (0xA6F89)" with the counts of each
  level and the names that the tables do not have.
