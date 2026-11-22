# Scripts, conversations and mission flags

This document covers the script system of DTUGAME.EXE 1.3 (`dialogue.c`,
`dlgmenu.c`, the mission code of `mission.c` and `frontend.c`): the script
files, the interpreter, the conversation window, the global flags, the 19
mission flags and the mission switch of Khelben.

Addresses are linear addresses of `gamedata/re/dtu_flat.bin` (object 1 at
0x10000). `P` is the record of the local player, `Players[]` at 0x1D331C,
0x1B37 bytes each (see [character.md](character.md)). TEX[n] is line n of
DTUE.TEX (`Text_string[]` at 0x395DAC).

Marks:

- **V** verified: read in the disassembly, or checked against the data files.
- **G** guess: a reading that the code or the data suggests but does not prove.

The tool [`tools/script.py`](../../tools/script.py) disassembles the scripts
with the grammar of this document. It parses every record of all seven GABx
files to the last byte with no unknown code and with matching IF levels
(section 12). That is the data check of the grammar.

[ai.md](ai.md), section "Scripts and NPCs", has the loader, the NPC list
and the level rule of the script ids. This document replaces its statement
and condition tables: the table of ai.md had wrong operand lists (C5, C6, C3,
D4, D5, D7, DD, DE, DF, EB) and missed the text blocks, the mission flag
copies and the window code.

## 1. Files

**V** `GABx.RLH` and `GABx.DAT`, x = E, F, G, I, S, N, M by the language
(0xA6467, the letter is the fourth character). The layout and the XOR key are
in [../formats/jrg.md](../formats/jrg.md). A record is one script. The record
id is the script id (low 9 bits).

Script ids (GABE): 1 to 8 (YPINN, level 0), `level * 10 + n` for the levels
of DTU.MSN (20 to 26 KOBOLDS, ... 320 to 323 FLAME), and the four global
scripts 508 to 511 (0x1FC to 0x1FF).

| Id | Started by | Content |
|---|---|---|
| 0x1FC | 0x3846A | raises one ability score by class (**V** data) |
| 0x1FD | seven callers, one is the talk with item WEAPON 13 in the hand (section 6) | **G** not read |
| 0x1FE | 0x37DEE (the use of an item) | the "Orb of Insight": random questions and answers (**V** data) |
| 0x1FF | 0x429DA, a cheat sequence of 8 keys, message "Ready or not, here we go!" | a level select menu: TELEPORT_PLAYER to the start of each level (**V** data) |

## 2. The interpreter state

**V** Globals of the interpreter:

| Address | Size | Meaning |
|---|---|---|
| 0x145526 | ptr | read pointer |
| 0x14552A | u8 | slot of the current script (start 0x3CE41C, end 0x3D03AC, by slot) |
| 0x145525 | u8 | IF depth |
| 0x145532 | u16 | current label |
| 0x145537 | ptr | current object |
| 0x145536 | u8 | run mode 0 to 4 (section 5) |
| 0x145535 | u8 | value that flag 0 of the object gets after the run |
| 0x145534 | u8 | 1 while the conversation window is open |
| 0x145531 | s8 | number of replies minus 1 |
| 0x3CE46C | 800 * 10 | replies: u16 label, then the text (798 bytes) |
| 0x3D4588 | 0x31E | the text after the token replacement |
| 0x3D03FC | 0x31E * 20 | the text alternatives of one text operand |

## 3. Byte code

**V** A script is a byte stream. A byte of 0x7A or above is a code. Numbers
and texts are strings that end with a NUL byte.

A record is a list of blocks. A block starts with `CB` and a label:

- `CB <hex>`: a label number.
- `CB E7 <hex>`: the label of a segment, used by run mode 2 (0xA78CF).

Labels 0x3E9 to 0x3EC (17, 19, 15 and 7 scripts in GABE) hold only a text
operand: the lines that the status code shows (section 5). Every other block
holds statements. The interpreter never reads a `CB`: a block ends with a
statement that ends the run (`CC`, `C8`) or jumps (`C7`).

### 3.1 Operands

| Name | Bytes | Reader |
|---|---|---|
| HEX | a string of hex digits, read with `sscanf("%hx")`; the pointer moves past the NUL | 0xA76B9 |
| NUM | `D2 HEX HEX`: random. `D2 a 0` is 1 to a, `D2 a b` is a to b (`rand() % (b - a + 1) + a`, 16 bits) | 0xA7DF0 |
| | `E0`: the 4-bit variable of the current object. `E0 15 NUM`: of object NUM. Without 15 the next byte is not read | |
| | else HEX | |
| TEXT | one or more strings, up to the first byte of 0x7A or above. One string is chosen at random (`rand() % count`). More than 20 strings make the reader loop forever. No string gives "" | 0xA77BE |
| OBJ | `15 NUM`: an object number (index of Objects). Optional where marked | |
| ARITH | an operator byte, then NUM: `7A` add, `7B` subtract, `7C` multiply, `7D` divide (by 1 if NUM is 0 or 1), `7E` set. Another byte keeps the old value | 0xA8DB2 |
| CMP | a compare byte, then `EF` (the maximum hit points of the player, word P+0xA1) or NUM: `7F` <, `80` <=, `81` ==, `82` !=, `83` >, `84` >=. The compare is unsigned 16-bit | 0xA7BEE |
| CLASS | `A4` WEAPON (type 5), `A5` ARMOR (15), `A6` MISC (16), `A7` SPELL (17), `A8` GOLD (table 0xA641C) | |
| FACING | `A9` east, `AA` west, `AB` north, `AC` south (0 to 3 for 0x5A5DE) | |
| SIDE | `AD` to `B2`: side 0 to 5 of a segment | |
| WHO | one raw byte: an index of the critter table 0xA504A (ai.md), or `CA` "any" | |
| RANGE | `CA` "any distance", or NUM (whole units) | 0xA7DE0 |

Text tokens (0xA76ED, 0xA7582): byte 0x09 is the name of the player (P+0).
0x0A: a word for class 1 (TEX 642 "Cleric" ... or TEX 692 "cleric", 910
"wizard", 911 "warrior", 912 "rogue", chosen at random), 0x0B: the plural
(TEX 906 to 909 or 913 to 916), 0x0C: the race (TEX 649, 650, 651, 957,
653, 917; half-elf when the race is human and bit 0 of P+0xDF is set), 0x0D:
the plural race (TEX 954 to 959), 0x0E to 0x11: by sex (byte P+0x9B): "boy"
/ "girl", "man" / "woman", "sir" / "madam", "gentleman" / "lady" (TEX 918 to
925). The first letter of a token is made upper case at the start of a
sentence (after `.`, `!` or `?` and spaces) and always for the name.

### 3.2 Statements

**V** Dispatch 0xAAE3E, loop 0xAB5AB: read a code, map `F5`..`F8` to `F4` and
`FA`..`FD` to `F9` (0xA8CA6), then the jump table at 0xAAC6E (97 entries for
0x99 to 0xF9). A code below 0x99 or a code with no handler prints `RLH: *
Don't know statement code %d (%X) *` and ends the run. The loop ends when
the pointer reaches the end of the record. The "Count" column is the number
in GABE (tools/script.py stats).

| Code | Operands | Handler | Meaning | Count |
|---|---|---|---|---|
| 99 | ARITH | 0xAB4D5 | strength, word P+0xB7. For a fighter: from below 18 to 18 the percent value P+0xB9 = `rand() % 100`; at 18 with a percent value above 0 it goes up by 10 (above 99: 0) | 5 |
| 9A | ARITH | 0xAB5F1 | dexterity, P+0xC3 | 5 |
| 9B | ARITH | 0xAB61F | constitution, P+0xC1, then the maximum hit points P+0xA1 grow by 0x8C01B(con, fighter) | 5 |
| 9C | ARITH | 0xAB683 | intelligence, P+0xBD | 5 |
| 9D | ARITH | 0xAB6B4 | wisdom, P+0xBB | 6 |
| 9E | ARITH | 0xAB6E7 | charisma, P+0xBF | 5 |
| 9F | ARITH | 0xAB71A | the level (the higher of P+0xAF and P+0xB1, plus 1), set with 0x8BA3A | 1 |
| A0 | ARITH | 0xAB7AA | hit points (P+0x53, whole part). Less: damage through 0x29A9C with a pain sound (a big one if the loss is above 1/20 of the maximum). More: capped at the maximum, heal sound | 38 |
| B3..BD | none | 0xAB2C4 | animation state of the object = code - 0xB3 (0xA9E2B): control type AI, clears the silence bit 0x40 of +0x89, state and goal = n, frame = first frame of n. State 9 (die) only sets the hit points to 0. States 6 and 7 play the attack sound, 8 the flinch sound | 75 |
| BE | TEXT | 0xAAE51 | narration (0xA79FD). Opens the window in mode 0. In the window as narration, else as a HUD message | 268 |
| BF | CLASS NUM [`E4` NUM \| `E3` NUM] | 0xA8F77 | give (log "give %s %d to PC from seg %d"). GOLD: gold P+0xDB += NUM. `E4 FFFF`: a new item for the player (0xB6390). `E4 seg`: the player picks up the item object of that class and id that lies in the segment (0x29455). No E4: only if the object carries it (+0x7C class, +0x7D id, +0x7E count >= 1): with `E3 seg` the item is dropped into the segment, else the player gets a new item; the count +0x7E goes down by 1 | 290 |
| C0 | CLASS NUM [`E4` NUM \| `E3` NUM] | 0xA9191 | take. GOLD: subtract if the player has enough. `E4 seg`: delete the item object in the segment (0x68F45) and the object carries one. Else only if the player has it (0xB6BB3): `E3 FFFF` nothing more, `E3 FFFE` a random robot or dressing object with no item carries it, `E3 seg` a new item object in the segment, no E3: the object carries it. Then the item leaves the inventory (0xB6D0C) | 64 |
| C1 | op NUM | 0xAAE93 (0xA8E77) | experience. For add and subtract the value is divided by the number of classes (2 for a valid class 2), at least 1. Each class gets it, then the level check 0xAF489 | 57 |
| C2 | ARITH | 0xAAE9D | anger of the object (ai_local +0x00, 0x153EB0 + 47 * objnum), capped at 5. At 5 the object turns hostile (0xA9418) unless flag 3 is set. At 0 flag 2 is cleared and the target +0x1A2 = -1 | 41 |
| C3 | WHO RANGE op NUM | 0xAAF8C (0xA94CE) | the same for each entry of the NPC list in range of the current object with the robot id of WHO. Add caps at 5, subtract stops at 0, set caps at 5. Bit 9 also blocks the change to hostile | 34 |
| C4 | NUM SIDE | 0xAAF96 (0xA9755) | the trigger of the side (0xA829E): if it is off (+0x0E is 0) switch it on (0xA8324), else fire it: check_trigger 0x939AD(seg, side, 2, object) | 51 |
| C5 | k [OBJ], k = 0..7 | 0xAAFAA | set flag k of the object (bit 20 + k of +0x1B0). Flag 2 set: hostile (0xA9418) unless flag 3. Flag 2 cleared: anger 0. Flag 3 set: 0x160AB(object, 5) | 644 with C6 |
| C5 | b NUM, b > 7 (always `F1`) | 0xAB124 | set global flag NUM. Ignored for NUM <= 0x20. NUM 0x21 to 0x33 also set mission flag NUM - 0x20 (section 7) | |
| C6 | as C5 | 0xAAFAA | clear | 133 |
| C7 | NUM | 0xAB242 | goto label (0xA7872: search from the start of the record for `CB` bytes, depth 0) | 982 |
| C8, CC | none | 0xAB256 | end of the run. 0xA7A57 goes to the exit label (0x3EA if flag 2, else 0x3EB if flag 3, else 0x3E9 if flag 1, else 0x3EC) and shows one of its lines as a HUD message half of the time | 699, 1540 |
| CD | TEXT, reply list | 0xAB469 (0xA8CBC) | say (section 4) | 1816 |
| D3 | NUM NUM [`D8`] | 0xAB260 (0xA97B3) | sound, volume 1 to 10. 0x2710 to 0x271B are the sounds of the player record (P+0x1B07 on). 0xFFFF starts the end sequence with the movie FLAM1.MVE. `D8` not decoded | 48 |
| D4 | NUM NUM NUM NUM [`D9`] | 0xAB274 (0xA9BE6) | sound, volume, object, a fourth value; at the object (0x2E627). `D9`: looped (**G**) | 9 |
| D5 | NUM NUM NUM SIDE NUM [`D9`] | 0xAB288 (0xA9CBE) | sound, volume, segment, side, a value; at the side (0x2E94B) | 2 |
| D6 | NUM | 0xAB29C | stop the sound of an object (0x2E816) | 2 |
| D7 | NUM NUM SIDE | 0xAB2B0 (0xA9DEC) | stop a sound of a side (0x2EB77) | 1 |
| DA | NUM FACING | 0xAB2DD (0xA9F70) | move the current object to the center of a segment (0x13899) with a facing. 0xFFFF removes it (0xA9F58: control, movement and render type 0, +0x1AE bit 4, flags bits 0xC0) | 44 |
| DB | NUM NUM FACING | 0xAB2F1 (0xAA1FC) | move the player to level NUM (index of DTU.MSN), segment, facing. It writes the warp of the player's segment and calls the warp code 0x59C3C, then puts the old warp back. Another level, or segment 0xFFFF, first ends the run (flag 0, talk counter, window, death) and the run stops. Segment 0xFFFF: 0xAA3F3, the prison sequence (**G** from ai.md: the player loses the items and goes to level 7) | 69 |
| DC | NUM NUM FACING | 0xAB32E (0xA9FDF) | the same as DA for object NUM | 109 |
| DD | CLASS NUM NUM NUM | 0xAB367 (0xAA04B) | the item of class and id that lies in segment 1 goes to segment 2; 0xFFFF deletes it | 10 |
| DE | WHO NUM FACING | 0xAB382 (0xAA14D) | summon a critter of WHO in the segment (gate_in_robot 0x1374D) and set the facing. At most 100 NPCs ("RLH: can't have more than %d NPCs in a level"). WHO 0xBE and above calls 0xA66B4 (a stub) | 45 |
| DF | `15` NUM ARITH \| ARITH \| nothing | 0xAB3C0 (0xAA670) | the resume label (bits 10 to 19 of +0x1B0) of object NUM or of the current object. With no operator: the current label | 128 |
| E0 | [OBJ] ARITH | 0xAB3CA | the 4-bit variable (bits 24 to 27 of +0x1B0) | 104 |
| E5 | HEX ... `E6` | 0xAB435 (0xAA79F) | walk a path through the segments (plain HEX, not NUM). Sets 0x145535 = 1 (flag 0 stays set), target -1, AI mode 2, ai_local +5 bits 0x30, speed. At the end of the path the AI runs the script in mode 2 (0x174FC) | 16 |
| EA | NUM NUM | 0xAB47B (0xAA86C) | spell NUM (at most 50) from the current object at object NUM (0x8CFFF) | 130 |
| EB | NUM NUM | 0xAB48F (0xAA8B3) | spell NUM from the current object (0x90CEF). The segment NUM is checked against the highest segment but not passed (a defect of the executable) | 87 |
| EC | NUM | 0xAB4A3 (0xAA8E4) | wait NUM ticks of the 1/100 s clock 0xEDC4C, with reduced frames 0x4391D | 206 |
| ED | `C4` NUM SIDE \| NUM | 0xAB4B7 (0xAA908) | switch the trigger of the side off (0xA83BD), or kill object NUM (0xA6ACB) | 39 |
| EE | NUM | 0xAB4C1 (0xAA989) | run the script of object NUM in mode 4, then go on. The pointer, slot, depth, label, object and 0x145535 are saved and restored; the mode is not (it stays 4) | 10 |
| F2 | condition | 0xAB43F (0xA8C3A) | IF (section 3.3) | 1406 |
| F4..F8 | none | 0xAB449 | ELSE of depth 1 to 5, reached at the end of a true branch: skip to the ENDIF of the current depth and run it | 686 |
| F9..FD | none | 0xAB45E | ENDIF: depth - 1 | 1406 |

### 3.3 Conditions

**V** `F2 term [E1 term | E2 term]... F3` (0xA8BDB). E1 is AND, E2 is OR,
left to right with no precedence. `F3` adds 1 to the depth. Another byte ends
the condition without the depth change. If the value is 0, 0xA8C49 scans the
raw bytes for the ELSE of the depth (`F4 + depth - 1`, the pointer stops
after it) or the ENDIF (`F9 + depth - 1`, the pointer stops on it, so it runs
and lowers the depth). The scan does not parse operands.

A term (0xA8516) can start with `E9` (NOT: the value is 1 if the result is 0).

| First byte | Operands | Value |
|---|---|---|
| 00..07 | [OBJ] | flag k of the object |
| 85..8A | | alignment (word P+0xE3) = code - 0x85: LG, NG, LN, TN, CG, CN |
| 8B..8E | | class 1 or class 2 (P+0x97, P+0x98) = code - 0x8B: cleric, mage, fighter, thief |
| 8F..94 | | race (P+0x9A; 5 for a half-elf) = code - 0x8F: human, dwarf, elf, drow, halfling, half-elf |
| 95, 96 | | sex (P+0x9B) = code - 0x95: male, female |
| 97 | [OBJ] CMP | talk counter (bits 28 to 31) |
| 98 | CMP | the word at 0x153ED8 + 47 * objnum (ai_local +0x28, morale) plus 500 |
| 99..9E | CMP | the abilities of statements 99 to 9E |
| 9F | CMP | class 1 level (P+0xAF) + 1 |
| A0 | CMP | hit points |
| A1 | CMP | the mission of Khelben (0x13E588, 0x8445A) + 1 |
| A2 | CMP | the object number of the current object |
| A3 | CMP | the run mode |
| C2 | CMP | anger of the current object (signed byte) |
| C3 | WHO RANGE CMP | 1 if any NPC in range with that robot id has anger CMP (signed) |
| C4 | NUM SIDE | the trigger of the side is on (+0x0E) |
| C9 | NUM | object NUM is dead or gone (0xA66C9: a robot with less than 1.0 hit points, bits 7 of the flags, or state 7 at +0x142 or +0x143; another object with a different signature) |
| D0 | CLASS NUM | the current object carries the item (GOLD: always 1) |
| D1 | CLASS NUM | the player has the item (0xB6BB3). GOLD: NUM <= gold |
| DF | [OBJ] CMP | resume label |
| E0 | [OBJ] CMP | 4-bit variable |
| E7 | CMP | segment of the current object |
| E8 | NUM SIDE CMP | `fire_count` of the trigger (+0x2E). With no trigger the CMP is not read (a defect; no script hits it, **G**) |
| F0 | NUM | `Automap_visited[NUM]` is not 0 (0x185FEC, written by the automap code at 0x189EE) |
| F1 | NUM | global flag NUM. For NUM 1 to 19 the mission flag NUM is first copied into the global flag (section 7) |
| other | NUM CMP | a number, for example `D2 2 0 81 1` (one chance in two) |

## 4. The conversation window

**V** `CD` (0xA8CBC):

1. Open the window if it is not open (0xA74AA).
2. Show the text as speech (0xA7A22(1) → 0xB1F5F).
3. Read the reply list: `CF NUM TEXT` adds a reply (0xA7B56, at most 10, the
   others are read and dropped), `F2` IF, `F4..F8` ELSE, `F9..FD` ENDIF (the
   same rules as the statements), `CE` ends the list. Any other byte prints
   `RLH: * Don't know say code %d (%X) *` and ends the run.
4. At `CE` (0xA7ADC): wait for the choice (0xB2FA2). A choice goes to the
   label of the reply. Escape ends the run (0xA7A57, the exit label). A list
   with no reply waits for Enter and goes on after the `CE`.

`CD 00 CE` (an empty text and no reply) is used after narration to wait for
the player.

Open (0xA74AA): 0x458BA(1) ramps the byte 0x14C4CE (the dark view, see below)
from 1 to 15. Then
0xB1D77(10) (at most 10 replies, `MaxDialogResponses` 0x1465A8; 8 when the value is not
above 0), 0xB13C4 (the layout), 0x92FF2. Close
(0xA74D0): 0x458BA(0) ramps it back, 0xB1A8C, and the temporary object of
run_script_id is deleted.

### The dark view behind the window

**V** (code read in 0x458BA, 0x45A60, 0xEA503, 0xEC4D7, 0x105A81, 0x105C60):

- No palette function is called by the window code (0xA74AA, 0xA74D0, 0xB13C4,
  0xB14D8, 0xB19A2, 0xB1A8C, 0xB1D77). The palette functions 0xD9DB4
  (gr_palette_step_up), 0xD9EA4, 0xD9EE0, 0xD9F70, 0xDA03C and 0xDA1E8 have no
  caller in the window or script code. The change of the view is a rule of the
  texture mappers.
- 0x458BA(1): D = 0, then while D < 15: D + 1, and reduced frames (0x4391D)
  until the game time (0x13CD30) has moved 0x800 (1/32 second). So D is 1 at
  once and 15 after 14 x 0x800; the call returns 0x800 later. 0x458BA(0): D =
  15, then while D > 0: D - 1 and the same wait. D is the byte 0x14C4CE. The
  window code goes on (and shows the window) only after the ramp.
- 0x45A60 calls 0x458BA(1) with the byte 0x14C4CF at 1 (another use, not the
  window). The window leaves 0x14C4CF at 0.
- The light functions of the mappers, 0xEC4D7 (linear) and its copy 0x105A81
  (perspective, the globals copied by 0x105C60: 0x14C4CE to 0x1512BF, 0x14C4CF
  to 0x1512C0), get the texel color c in AL and the fade row r in AH. When
  0x14C4D8 is 0 they return `fade[r][c]`. Else, for c not 0xFF and D not 0, when
  `(c & 15) <= D` (signed compare), before the effect colors and infravision:
  - r = r - 5, at least 0;
  - 0x14C4CF = 0: the color is `(fade[r][c] & 15) | 8`;
  - 0x14C4CF = 1: v = `fade[r][c] & 15`, the color is `v | 0xF0` when v is 1
    to 13, else v.
- 0x14C4D8 is set for each polygon by draw_tmap (0xEA503 to 0xEA525): 1 when
  the bitmap has flag 0x20 (the merged walls of the texture merge cache, see
  [effectcolors.md](effectcolors.md)), when 0x14C4CD (the tint of an object)
  is not 0, or when 0x14C4CF is not 0. The window sets only D. No other
  write of 0x14C4D8 or 0x14C4CF was found (all.asm, also writes of a word or a
  dword over these bytes), and no code but the texture merge cache (0x7AB89)
  sets flag 0x20 on a bitmap. By this reading only the lit merged walls (a
  wall with an overlay texture) and the objects with a tint would change.
- **Seen in the DOS game** (a screenshot of the user, version 1.3, a
  conversation with a skeleton in a stone room): the whole 3D view changes,
  the walls, the floor, the ceiling, the skeleton and the first person sword,
  to a desaturated lavender grey of middle brightness with a dithered look;
  the interface around keeps its colors; the change takes a moment. So
  something makes the rule apply to every lit polygon while the window is
  open. **G**: what it is was not found (open question 6).
- `(x & 15) | 8` is a color 8 to 15 of palette row 0: PALETTE.256 gives
  (28,22,27), (24,18,24), (19,15,20), (15,12,16), (11,9,13), (7,6,9), (4,3,5),
  (1,1,2) (6-bit values), the dark half of the grey ramp, lavender. The lit
  step s of the texel (five rows brighter) gives color `s | 8`: steps 0 to 7
  become 8 to 15, steps 8 to 15 stay. With D at 15 every texel takes one of
  these eight colors with its brightness steps folded, which looks dithered.
  This matches the screenshot.
- The ramp tests the unlit texel step `c & 15` (0 is the brightest of a
  ramp), so the bright texels change first. The ramp takes 15 x 0x800 of game
  time (about half a second) with reduced frames, and the game waits for it.

The port (`engine/texmap/tmapfx.c`, `engine/main/dtudlg.c`), as the DOS game
looks: `dtu_dlg_view_dim` gives D from the game time (the steps above); the
port does not wait for the end of the ramp. render.c gives it to the software
mappers (a copy of the fade table with the rule, used for every lit polygon
of draw_tmap: walls, floors, ceilings and the models, also the first person
arms and weapon) and to the GPU renderer (`u_dim` for each lit textured
polygon). The 8-bit form of the GPU renderer does the rule on the palette
index. The RGB form takes the nearest palette color of the texel for the
test, the light five rows brighter, the step of its nearest palette color for
the new color, and keeps the detail of a filtered or upscaled texel at the
scale of the new color. Flat polygons (no texture) keep their colors, as the
rule is in the texture mapper. A level start and a loaded game end the dark
view at once.

The port had an extra half light of the window area (fade table row 11). It
made the view behind the text darker than in the DOS game, so it was
removed: the lavender grey view alone is the original look.

Test (YPINN, Durnan, `-level YPINN -fixedfps 30 -testpos -60,-8.7,9,270
-testmove wait:10,use:1,wait:400 -dlgauto esc,esc,esc,esc -dlgdelay 10`,
frames 8, 45 and 100): during the conversation the whole view, the walls, the
floor, Durnan and the first person sword, is lavender grey with folded steps
in the software renderer and in both forms of the GPU renderer; the sword has
no orange or purple swirls; less than half a second after the window closes
the view is the same as before the conversation.

Layout (0xB13C4, tables at 0x1465F4, 0x146624, 0x146654, values for low and
high resolution). A screen mode other than 0, 1 and 7 is changed to mode 0
first (0x407EB(0)).

| Mode | x | y | width | text height | reply height |
|---|---|---|---|---|---|
| 0 (main screen), low / high | 70 / 141 | 12 / 26 | 237 / 473 | 62 / 123 | 62 / 124 |
| 7 (full screen) | 10 / 20 | 10 / 20 | 300 / 600 | 90 / 220 | 90 / 220 |
| 1 | 10 / 20 | 10 / 20 | 300 / 600 | 62 / 124 | 62 / 124 |

The window has no picture and no portrait: the text is drawn over the 3D
view. The text area is at (x, y), the reply area under it. The replies have a
column of numbers `"%d)"` and a column of text.

Positions (0xB13C4, 0xB1B9E, 0xB2D0D, 0xB2D90, verified in the code). The margin
m is 10 (low) or 16 (high) (0x1466AC). An area of height h holds n = (h - 2m) /
ft_h lines of the font; its lines start at m + (h - 2m - n x ft_h) / 2 from the
top of the area. Each part is a sub-canvas of the screen, which clips the text:

- Text: x, y + top of the text area, width, n x ft_h. The speech and narration
  buffers are drawn at the top of it (with the scroll offset), lines ft_h apart.
- Number column: x, y + text height + top of the reply area, width numw, where
  numw is the largest width of `"%d) "` (with the space) for 1 to
  `MaxDialogResponses` (0xB170B).
- Reply text: x + numw + 10 (x + width - (width - numw - 10)), the same y, width
  - numw - 10; the replies are wrapped to that width (0xB20F0 in 0xB1D99).
- A reply is drawn at row r of both columns with `gr_string(0, r)`; the next reply
  starts at r + the height of the reply + ft_h (one empty line, 0xB2E69).
- "Press Enter...": centered in a canvas of the whole reply area width at the
  same y, at (canvas height - ft_h) / 2 (0xB2E91).
- The magenta frame (0xB2EDD): lines at x - 3 and x + width + 3 and at the top and
  the bottom row of the active area (y and y + text height, or y + text height
  and y + text height + reply height).

High resolution, mode 0, the game font (ft_h 8 in the port): text lines from y 43, 11 lines;
reply rows from y 167, 11 rows; the reply text at x 141 + numw + 10. Colors (0xB19A2 to 0xB19F1,
the closest palette colors): speech green (0, 63, 0), narration yellow (63,
63, 0), the selected reply orange (63, 32, 0), the other replies grey (32,
32, 32), a frame of magenta (63, 0, 63) lines around the active area, from
x - 3 to x + width + 3. The font is the game font of 0x13B844. With no reply
the reply area shows "Press Enter..." in orange. The arrows FEBGND 186 and
187 (high, "Up" and "Dn", 64x64) or 188 and 189 (low) mark text that can
scroll.

Input (states 0xB28F0 with the focus on the replies, 0xB2BED with the
focus on the text, 0xB2B6F with no replies). The focus starts on the replies
(0x1466C4 = 1, the magenta frame is around the reply area) with no reply
selected (0x3D4990 = -1). Up and Down (also keypad 8 and 2) move the
selection (the first press selects reply 1; the selection stops at the first
and the last reply), or scroll the text when the focus is on the text. Enter
(also keypad Enter) returns the selection. Each frame with the focus on the
replies ends with 0xB29A1 or 0xB2ABC: a selection of -1 becomes reply 1, and
a selection outside the replies shown in the area (first 0x3D498C, count
0x3D4984) moves the area to it, adding the later replies and then the
earlier ones (0xB29A1, the selection went up) or the earlier and then the
later ones (0xB2ABC), each reply with its lines and one empty line while
they fit. So -1 is left only for Enter in the first frame: the window closes
with -1 (0xB23C6 clears the active flag that 0xB2FA2 tests) and 0xA7ADC ends
the run as for Escape. Space and Tab move the focus
between the replies and the text. Escape closes the window and returns -2.
The key ` is the screenshot key (0x80DA1). With no replies (0xB2B6F),
Enter or keypad Enter go on (not with the focus on the text), Up and Down do
nothing, Space and Tab move the focus to the text
(0xB2BB5) as with replies, and Escape also goes on (0xA7ADC ignores a
negative choice when there is no reply). With the focus on the text
(0xB2BED) Enter does nothing. The mouse pointer is kept inside the window
(0xB249C). There are no number keys.

While the window waits, 0xB2FA2 calls the reduced frame 0x4391D in a loop:
it draws the view and the interface and advances the game time, the player
animation (0x4450C) and the spell effects of the player (0x9002C). **G**: it
does not move the creatures (object_move_all is not among its calls).

Speech audio: **V** the say and narration statements play no sound. The
dialogue streams of `str_dlg.c` (WERDZ.DAT) are the speech of Khelben in the
front end; no statement of the dialogue code calls them.

## 5. Running a script

`run_script(object, mode)` is 0xABBE5. **V**

| Mode | Caller | When |
|---|---|---|
| 0 | 0xAC155 (talk), 0xAC055 (run_script_id) | the player uses the object |
| 1 | no caller | the status line |
| 2 | 0x174FC (AI path end, function 0x17411) | the object reached the end of its path and flag 0 is set |
| 3 | 0x93EB3 (trigger) | a trigger with `object_num` above 1 fired |
| 4 | 0xAA9E3 (statement EE) | another script calls this object |

Steps:

1. Mode 3 while the window is open: return.
2. No script id, or no object: "* RLH: object %d is NULL or has no script *".
3. Modes 0 and 2, id below 0x1FC: a dead object does not run ("is dead").
4. The script must be in table1 and loaded.
5. Ids other than 0x1FC to 0x1FF, 0x37, 0x3F: if `id / 10` is not the level
   byte P+0x71, the id is cleared and the object turns hostile.
6. Current object and mode. A live robot with activity below 2 wakes up
   (0xA6505(object, 1)).
7. Start label (0xAAA32, below). Return if there is none.
8. The player stops (0x417ED, the control values at 0x30F460 cleared).
   Animation state of the object = 2. A live robot with an id below 0x14A
   turns to the player in steps of 0xCCC with a frame drawn after each step.
   Then the state is 1 (stand).
9. Silenced (+0x89 bit 0x40): HUD "%s tries to speak, but nothing
   happens." and return.
10. Mode 0, a thief, level 0 (YPINN), and the display name of the object is
    TEX[905] "Mirt": the thief skill points (0xABB53, `thiefpts.c`): if the
    thief level + 1 is above the byte P+0x1A53, the points (word P+0x1AB3) =
    (level + 1 - P+0x1A53) * 20, plus 20 if P+0x1A53 is 0 (set, not added);
    the screen 0xB5B49 opens (combat.md, B7); if it returns 1, P+0x1A53 =
    level + 1. Then the script runs. The thief level is the level of the
    second class if it is the thief (P+0xB1), else of the first (P+0xAF).
    In the port: `thief_points` of `dtuscript.c` and `dtuthiefpts.c`.
11. Flag 0 = 1, 0x145535 = 0, run the interpreter.
12. If the interpreter returns 0 (DB to another level), stop. Else flag 0 =
    0x145535; in mode 0 the talk counter + 1 (max 15); close the window; if
    the object has less than 1.0 hit points, kill it (0xA6ACB).

Start label (0xAAA32):

- Status label: 0x3EA if flag 2 (hostile), else 0x3EB if flag 3, else 0x3E9
  if flag 1 (wary), else 0x3E8.
- 0x3EA, or 0x3E9 when `rand() % 20 + 1` is above the charisma: go to the
  label; if it exists, show one of its lines half of the time, and for 0x3EA
  play the attack sound of the robot. Else in mode 0 a robot shows TEX[926]
  "No response...". No run.
- Modes 0, 3 and 4: the resume label if it is not 0, else the first label
  below 0x3E8 in the record.
- Mode 1: go to the status label and show one of its lines. No run.
- Mode 2: the block `CB E7 <segment of the object>` (0xA78CF), label 0.

The HUD lines of the status code use 0x347A2 with the color
`signature % 8` of the object (**G**: a color per speaker).

`run_script_id(object, id)` is 0xAC055. With object 0 it creates a temporary
object of type 0xD at the player (0x68AC8), with no flags and +0x1AE bit 2,
gives it the id and runs it in mode 0. The window close deletes it.

## 6. Starting a conversation

**V** The use action (0x95050, key "Activate", A by default) calls 0xAC155
(0x951D1):

1. Nothing while the player is in the attack states 6, 7, 11 or 12.
2. 0xB30FD(player, 7, 30): the object in front of the player within 30.0.
3. A robot with no script id: HUD TEX[926] "No response...".
4. A dead object: nothing.
5. With item WEAPON 13 in the hand: global flag 0x81 (if the robot has bit 8
   of `Robot_info + 0xD9`), global flag 0x8F, run script 0x1FD, stop if
   global flag 0xA0 is then set.
6. `run_script(object, 0)`.

## 7. Global flags and mission flags

**V** Global flags: 256 bits in the player record at P+0x1A33 (0x1D4D4F),
bit `n - 1` for flag n (0xADAFE read, 0xADB3B write; n is a byte, so flag 0
is bit 255). They are saved with the player record. They are cleared once at
the program start (0xAD8E6, after "RLH: Init dialogue system"), not for a new
character (**V**: MakePlayer 0x8BEBC does not clear the record).

**V** Mission flags: 19 dwords at 0x13CDD4. The address order is not the
number order. With `Mslot(k)` the address of mission flag k:

| k | 1..9 | 10..17 | 18 | 19 |
|---|---|---|---|---|
| address | 0x13CDD4 + 4 * (k - 1) | 0x13CE00 + 4 * (k - 10) | 0x13CDF8 | 0x13CDFC |

Three places use this same numbering:

- Script statement `C5/C6 F1 (0x20 + k)` sets or clears mission flag k (and
  global flag 0x20 + k), jump table 0xAADF2.
- Script condition `F1 k`, k = 1 to 19: copies mission flag k into global
  flag k, then reads global flag k (jump table 0xA84CA). Statements cannot
  write global flags 1 to 0x20.
- DTU.WRP command k (0x5B3AC, jump table 0x5B360) sets mission flag k to 1:

| k | Condition (0x5B6FA: no live robot with this script id in the level) | DTU.WRP lines (level, segment) |
|---|---|---|
| 1 | script 0x17 (KOBOLDS) | 2,6 |
| 2 | script 0x20 (BONES) | 3,241 |
| 3 | script 0x46 (THIEF2) | 7,328 and 7,34 |
| 4 | script 0xBE (CLAN3) | 19,94 and 19,131 |
| 5 | script 0xC9 (DROW1) | 20,422 20,396 20,17 |
| 6 | script 0xD4 (DROW2) | 21,24 21,684 |
| 7 | script 0xDE (DROW3) | 22,311 |
| 8 | script 0xFC (BEHOLDER) | 25,160 25,773 |
| 9 | script 0xFE (BEHOLDER) | 25,472 |
| 10 to 17 | none | 10: 4,319; 11: 14,226; 12: 17,2; 13: 20,248; 14: 22,285; 17: 32,2 and 32,519; 15 and 16: no line |
| 18 | no dressing object (type 18) named "Cloning Machine" in the level | no line |
| 19 | no dressing object named "Spectator Stone" | 29,0 |

The mission flags are cleared by 0x5ADC9, called from MakePlayer 0x8BEBC
(a new character).

Save file ([../formats/misc.md](../formats/misc.md), SG0.SG): offset 53 the
count 19 (the loader 0x5B095 stops with "saved data is out of date" if it is
not 19), offset 57 the 19 dwords in address order (0x5AE7B writes,
0x5B0A9 reads). The global flags are in the player record at file offset
141 + 0x1A33 = 6852.

## 8. Khelben and the missions

**V** At each visit of the Blackstaff tower, 0x83DC8 calls 0x83E36 before
the text: while the switch 0x83EF5 returns 1, the blurb = 0, the rewards of
the new mission are given (0x844ED) and the mission (0x13E588) = the new
mission. So one visit can advance several missions. The text is FETXT entry
`first + blurb` of the table 0x142840 ([frontend.md](frontend.md)).

Switch 0x83EF5 (jump table 0x83E69, 35 entries). `has(n)` is "the player has
MISC n" (0xB6BB3), `take(n)` takes it (0xB6D0C), `Fk` is mission flag k. The
result is 1 when the mission changed.

| Mission | Next |
|---|---|
| 0 | F1: 1 |
| 1 | F2: 2, and 3 if has(0x12) |
| 2 | has(0x12): 4 |
| 3, 4 | F10: 5. Then has(0x7F): 6, and 7 if F3 |
| 5 | has(0x7F): 6, and 7 if F3 |
| 6 | F3: 8 |
| 7, 8 | F11: 9. Then has(0x10): 10 and take(0x10) |
| 9 | has(0x10): 10 and take(0x10) |
| 10 | has(0x13): 11 |
| 11 | has(0x0F): 12 and take(0x0F) |
| 12 | F12: 13. Then F4: 14 |
| 13 | F4: 14 |
| 14 | F13: 15. Then F5: 16 |
| 15 | F5: 16 |
| 16 | has(0x17): 17 |
| 17 | F6: 18 |
| 18 | F14: 19. Then F7: 21, and 20 if has(0x18) |
| 19 | F7: 21, and 20 if has(0x18) |
| 20, 21 | F15: 22. Then F8: 23. Then F9: 24 |
| 22 | F8: 23. Then F9: 24 |
| 23 | F9: 24 |
| 24 | has(0x11): 25 and take(0x11) |
| 25, 26 | 27 |
| 27 | 28 |
| 28 | 29 |
| 29 | F19: 30, and 31 if has all of 0x12, 0x13, 0x14, 0x16, 0x17, 0x18, 0x1A |
| 30 | has all of these seven: 32 |
| 31, 32 | F17: 33 |
| 33, 34 | no change |

Rewards 0x844ED (jump table 0x84469) for the new mission: gold (P+0xDB),
a MISC item (0xB6B86 or 0xB6390), experience (0xAF3CF, split between the
classes) and a stock file of the market (0x879A7(n): the file of the table
0x13F6A0[n] is added to `geldor.dat`).

| Mission | Gold | MISC item | Experience | Market |
|---|---|---|---|---|
| 1 | 200 | 0x26 | 200 | 0 |
| 2 | 100 | | 200 | |
| 3 | 200 | 0x6B | 200 | 1 |
| 4 | | 0x6B | | 1 |
| 5 | 200 | | 200 | |
| 6 | 200 | | | |
| 7 | 400 | 0x6C | 400 | 2 |
| 8 | 200 | 0x6C | 400 | 2 |
| 10 | 1500 | 0x6D | 1000 | 3 |
| 11 | | 0x6E | | 4 |
| 12 | 1500 | 0x6F | 600 | |
| 13 | | | 250 | |
| 14 | | 0x70 | 1000 | 5 |
| 15 | | | 500 | |
| 16 | | 0x71 | 1000 | 6 |
| 17 | 1200 | 0x72 and 0x73 | | |
| 18, 19 | | | 1200 | |
| 20, 21 | 2000 | 0x74 | 1200 | 7 |
| 22 | | | 1400 | |
| 24 | 1200 | 0x75 | 1400 | |
| 25, 26 | 2000 | 0x76 (takes 0x11 first) | 1400 | |
| 27 | | 0x77 | 1400 | |
| 29 | 2000 | 0x78 | 1600 | 8 |
| 30 | | | 1600 | |
| 31, 32 | | 0x79 | 1400 | |

Missions 9, 23, 28 and 33 have no reward. **G**: the MISC items 0x6B to 0x79
are the keys that Durnan speaks of ("he'll give you the proper key to move
into the next section of the dungeon"), so each mission opens the next part
of the hubs (HUB1 to HUB4) through the doors that need these keys. HUB1 is
traced below; the doors of HUB2 to HUB4 were not traced.

### The new opening of HUB1 (missions 3 and 4)

**V** The text of missions 3 and 4 (FETXT 331 and 341: "If you return to the
tomb, I believe that you will find a new opening has appeared") comes with
the reward MISC 0x6B (107, "Violet Gem Key"). No mission flag, global flag or
level start rule opens the passage. The key is the condition:

- The has item function 0xB6BB3 looks for MISC 0x6B to 0x7C in the key item
  list (0xB6CC3: the id byte at P+0x18D4 + 16 * i, count P+0xFF), every
  other item in the item list.
- HUB1.TIG trigger 10: the floor (side 3) of segment 61, the segment in
  front of the Skull Key door of the Cassalanter tomb (trigger 8, door
  segment 58 side 4, MISC 0x26). Mask 0xFF, needs MISC 107, does not take
  it, trap 3. The fire function reads the item at trigger +0x19
  (0x93E0E). The floor fires with activation type 8 when the player enters
  the segment.
- HUB1.TAP trap 3: segment 40 side 4, flags 0x20, one charge: the walls of
  both sides of the opening (wall 36, segment 40 side 4, and wall 37,
  segment 162 side 5, both type 5 "closed" in the level file) become type 4
  "open". Behind it the corridor runs south (segments 162, 118, 115) to the
  iron gate, wall 10 (segment 115 side 4, a locked door). HUB1.TIG trigger 9
  opens that gate with MISC 107 and takes the key (trap 2). Message 10 of
  HUB1.SAY: "Beyond the gate is a dark passage leading up."

So a player who gets the key walks through segment 61 on the way to the
tomb and the wall opens. The change stays in the level state of HUB1
([levelstate.md](levelstate.md)): the wall types in `SGn_1.LVL` and trap 3
with 0 charges in `SGn_1.TAP`. Data check, the two saves of a played 1.3
game (both in BONES, the player carries MISC 0x12):

| Save | Mission | Key 107 | `SGn_1.LVL` walls 36, 37 | `SGn_1.TAP` trap 3 | `SGn_1.TIG` trigger 10 fire_count |
|---|---|---|---|---|---|
| `SG0.SG` "TERCERA MISION" | 4 | yes | 4, 4 (open) | charges 0, flags 0x620 | 5 |
| `SG1.SG` "En mision" | 2 | no | 5, 5 (closed) | charges 1, flags 0x20 | 3 |

At mission 2 the passage is closed in the original too; the next visit to
Khelben gives mission 4 (has MISC 0x12) and the key, and the next walk
through segment 61 opens it.

The port: the same rule with no special code (`dtu_char_has` reads the key
list, `dtu_trig_frame` fires the floor trigger, `trap_set_wall_type` sets
both walls). Checked with copies of the two saves (`-fescript`,
`-statetest 40,4,-1,...`, `-triglog`):

- `SG0.SG`: the warp from BONES to HUB1 restores `SG1_1.LVL` with wall 36
  type 4; the view from segment 42 shows the corridor and the iron gate.
- `SG1.SG` loaded in Waterdeep: Khelben gives mission 4 and MISC 107; in
  HUB1 the walls are type 5, the move into segment 61 fires trigger 10 and
  trap 3, and the walls are type 4. A save in the port then writes
  `SG2_1.LVL` with walls 36 and 37 of type 4 and `SG2_1.TAP` with trap 3 at
  0 charges and flags 0x620 (as the original), and the load of that save
  starts HUB1 with wall 36 of type 4.

## 9. Level start

See [ai.md](ai.md), "Scripts and NPCs": the loader 0xAD5BF, the NPC list, the
level rule of the ids and the peaceful test (a script with a `CD` byte).

## 10. The port

`engine/main/dtuscript.c` is the interpreter, the mission flags and the
mission switch; `engine/main/dtudlg.c` is the window. The port runs a script
until it must wait (a reply list, `EC`) and continues it in a later frame;
the executable waits in a loop of reduced frames. While a script waits the
game does not read the controls and does not move the objects (two calls in
GameLoop of game.c), as in the reduced frame. Calls from other files: the use
action (`dtu_use_object` of dtuhook.c) calls `dtu_script_talk`, the trigger
hook calls `dtu_script_run(obj, 3)`, DTU.WRP commands 1 to 19 call
`dtu_mission_check`, the Blackstaff screen calls `dtu_mission_advance`, the
save and load of dtufe.c call `dtu_mission_write_save` and
`dtu_mission_read_save`, a new character calls `dtu_mission_new_character`.
dtuobj.c gives the anger byte (`dtu_script_anger`) and 0xA9418
(`dtu_script_make_hostile`).

Settings (globals in dtuscript.c):

- `Dtu_fix_script_new_char` (1): a new character clears the global flags.
  The executable clears them only at the program start.
- `Dtu_fix_script_abilities` (1): statements 99, 9A and 9B also change the
  values without items, so the next change of the equipment keeps the
  change. The executable changes only the values with items.
- `Dtu_script_walk_teleport` (1, a port decision): `E5` moves the object to
  the last segment of the path at the end of the run and then runs mode 2,
  because the port has no access to the AI paths.

Not in the port: `EB` (no area spell function), the prison sequence of DB, the sounds of D4 to D7 at an object or
side (they play at the player), the step turn of the speaker, the shading
ramp, the scroll arrows of the window, the market stock of the
rewards (weak hook `dtu_hook_market_add_stock`), global flag 0x81 of the WEAPON 13 talk,
and condition 98 (the morale of dtuobj.c; the port uses 0 + 500).

Port additions to the window (not in the executable): the keys 1 to 9 and
the keypad digits choose the reply with that number at once, and the mouse
works as in the menus (the cursor of the play screen shows over the window,
also in the immersive mode, which comes back after the conversation; the
reply under the cursor is selected when the cursor moves, a click chooses
it, the wheel scrolls). See the top of `engine/main/dtudlg.c`.

Test options: `-scriptlog` (trace), `-dlgauto LIST` and `-dlgdelay N`
(automatic replies), `-scriptflags LIST` (global flags and `mK` mission
flags at the first level start and for a new character).

Checks done: Durnan (script 1) and Sabrae (script 3) in YPINN with replies,
labels and the talk counter; Sabrae sets global flags 74 and 153; Georges
(script 70, THIEF2) gives 5000 gold with `-scriptflags 110`; in BAT the
DTU.WRP command 10 at segment 319 sets mission flag 10; with mission flag 1
a new character sees mission 1 at Khelben with the Skull Key, 200 gold and
200 experience. **G**: the MISC names of the rewards come from the item
table of YPINN: 0x26 "Skull Key", 0x6B to 0x79 "Violet Gem Key", 0x12 to
0x1A "Spider Piece", 0x10 "Scarab Beetle", 0x11 "Crystal Rose", 0x0F
"Mykrul's Wand", 0x7F "thief scroll".

## 11. Open questions

1. The window draws: the scroll of long texts (which replies 0x3D498C and
   0x3D4984 show), the mouse.
2. The meaning of `D8`, `D9` and the fourth value of D4 and D5.
3. The prison sequence 0xAA3F3. (The end sequence of sound 0xFFFF is in
   movies.md: the movie FLAM1.MVE.)
4. Script 0x1FD and its seven callers.
5. The MISC items of the rewards by name, and the doors that they open in
   HUB2 to HUB4 (HUB1 and MISC 107 are in section 8).
6. What makes the light functions (0xEC4D7, 0x105A81) apply the dark view of the
   conversation window to every lit polygon (0x14C4D8 is 1 only for flag 0x20,
   a tint or 0x14C4CF by the code read), as the DOS game shows.

## 12. Tool check

`python -I tools/script.py check E:/UNDAMONTAN/DTU`:

    GABE.RLH: OK, 87 records, 369929 bytes parsed to the end, no unknown code
    GABF.RLH: OK, 86 records, 340610 bytes parsed to the end, no unknown code
    (the same for G, I, M, N, S)

The check also requires that every ELSE and ENDIF code has the depth of the
open IF, that no block starts or ends inside an IF, and that the IF depth
stays at most 5.
