# The phylactery of the lich of CLAN3

Code: `engine/main/dtulich.c`, `dtulich.h`. Setting: the line "The phylactery
of the lich" of the group "Restored and new content" of the Improvements
menu (`Dtu_lich_phylactery`, `LichPhylactery` in `ascent.cfg`), on by
default. The options `-origobj` and `-nophylactery` turn it off for one run.
Tests: the cases `lich_*` of `tools/questtest.py`.

Section 1 has the facts of 1.3, checked in the files. The rest is new in the
port: 1.3 has no phylactery.

## 1. What 1.3 has (checked)

Level 19 of `DTU.MSN` is CLAN3, "Crypts of Clangeddin" (594 segments). The
facts below come from `tools/lvl.py`, `tools/sidefiles.py` (TIG, TAP, SAY),
`tools/script.py` (GABE) and `DTU.WRP`.

- **The lich.** Object 10, type `Lich3` (`lic_gk.pof`, 88 hit points, script
  190, "Morik Stormhand"), at (278.4, -32.3, 135.2) in segment 443. The level
  fix 0xAD157 gives the type flags 0x7042 and the touch spell 22 (Hold
  Person, objects.md). Mission 13 of Khelben ends when no creature with
  script 190 lives (mission flag 4, `DTU.WRP` command 4 at the segments 94
  and 131).
- **His room** is on the lower level: 13 segments behind the two grills of
  the walls 120 to 123 (segment 249 side 4, segment 252 side 4). Trigger 15
  (segment 104 side 4, a lever behind the illusory wall of the walls 124 and
  125) runs the traps 10 and 11, which open the grills (message 13, "You hear
  the sound of metal grating across stone."). Two Banegards (objects 11 and
  12) stand in the alcoves behind the grills. The closed wall 90 (segment 443
  side 0) has no trap.
- **The way of the player.** CLAN1 has two warps to CLAN3: segment 523 to
  segment 94 (the start of the level file) and segment 534 to segment 131.
  Segment 534 is behind the secret wall of segment 506 side 4 (trap 9,
  trigger 23: the plate of segment 505 side 4, or the search). In CLAN3:
  1. Upper level: the levers of the segments 105 and 106 (behind illusory
     walls, triggers 5 and 6) open the closed walls to the two wings.
  2. The west wing (x below 0, 16 crypt cells) is behind the locked door of
     wall 106 (segment 404 side 4). Trigger 9 takes the scimitar (weapon 73,
     from CLAN1) at the shield of segment 284 and unlocks it (trap 7). The
     key MISC 27 (object 7) lies in cell 393, the Dispel Magic scroll (object
     30, spell 55) in cell 372 at (-120, 12.5, 305).
  3. The east wing (x above 145, 16 crypt cells) is behind the locked door of
     wall 104 (segment 263 side 5). Trigger 10 takes the bastard sword (weapon
     72, object 60 in segment 292 near segment 131) at the shield of segment
     268 and unlocks it (trap 8, message 8). Nothing in the east wing is
     needed for the quest.
  4. The lever of segment 1 side 4 (trigger 3) unlocks the door of room 107
     (behind the illusory walls 112 and 113); the lever of room 107 (trigger
     4) drops the floor of segment 1 (trap 4): the way to the lower level.
  5. Lower level: the corridor of the rats, the door of segment 170 side 4
     (locked; trigger 13 at segment 164 takes MISC 27 and unlocks it), message
     12 at segment 210 ("So you have found me at last..."), the corridor of
     segment 238, the lever of segment 104, the grills, the lich.

  None of the locks has the magic key bits (keys 0x01): a thief picks each
  of them, and Knock opens them.
- **The texts.** Script 190 (the lich): "In life, I was called Morik
  Stormhand", "I cannot die, child" (label 4, the answer to "Die, lich!").
  Script 170 (Barander, CLAN1 object 141 in segment 733, a priest of
  Lathander who hunts undead): "An old priest of Velsharoon, now a lich"
  (label 7, "Who's Morik?"), and label 12 (the answer to "How can he be
  stopped?") speaks only of his ritual. Khelben (the voiced text of missions
  12 and 13) speaks of "some powerful master of the undead".
- **CLAN2** has no line in `DTU.WRP` and no script teleport leads to it: the
  player never sees it.
- **Models.** `altar_a.pof` is the only altar model of the game; it is in
  `CLAN.EGG` and in eight other archives (13.0 wide, 7.1 high, 6.8 deep, 7
  textures `alt_01.bbm` to `alt_07.bbm`), and line 13 of the CLAN3 table uses
  it (object 1 of CLAN3). The CLAN3 table has 33 `$DRESSING` lines (0 to 32).
  The other candidates of `CLAN.EGG`: `chest5.pof` (7.7 by 4.7 by 6.2),
  `kettle3.pof`, `restob2.pof`.

## 2. The design (port)

The caster AI (ai.md, "Caster AI") gives the lich a contingency at the first
notice of the player: Stone Skin, Shield, Minor Globe of Invulnerability and
Mirror Image. A fighter or a thief has no spell to take it away. The port
adds a phylactery that the player can find and break before the fight.

- **The object.** A new `$DRESSING` line 33 of the CLAN3 table, "an altar of
  Velsharoon": `altar_a.pof` with the textures of line 13, `magic`,
  `explode_sound=2218`, `break_vclip=62` (the values of line 13). It is read
  after the other lines of the table when the table of CLAN3 has 33 such
  lines, with the setting off too, so that a level state with the object
  keeps its model. Morik was a priest of Velsharoon (script 170), so his
  phylactery is an altar of his god.
- **The place.** The empty cell of segment 557 of the east wing (x 285 to
  305, z 295 to 315, floor 2.5; its door on the side of x 305). The altar
  stands at (289.5, 6.0, 305.0) at the back wall, its front to the door; a
  Dispel Magic scroll (spell 55, the version of object 30) lies at (299,
  12.5, 298). A thief picks the lock of the wing (wall 104); the other
  classes bring the bastard sword to the shield of segment 268. The cell is
  before the floor drop to the lower level and is not seen from the main
  path.
- **Once in a game.** The objects are made at the setup of CLAN3
  (`dtu_lich_level_setup`, after the Flame Tongue of dtugear.h): not with a
  saved level state before its restore, not when the global flag 248 (placed)
  or 247 (broken) is set, and not when the lich is dead (mission flag 4, or
  no live creature with script 190). The flags are in the player record and
  are saved with the game. A level state from before the setting gets the
  objects at its restore.
- **The break.** Any rule that breaks scenery breaks it: one blow of a
  weapon (dtucombat.c), Fireball, Lightning Bolt (a save), Dispel Magic (a
  magic piece of scenery, save 5), the damage of the missile spells.
  `dtu_break_dressing` calls `dtu_lich_dressing_broken`, which sets flag 247
  and writes the message:
  - the lich is not in a fight: "The altar of Velsharoon cracks apart. A thin
    scream rises from the crypts far below."
  - the lich has a protection of the contingency: his Stone Skin, Shield,
    Mirror Image and globe end at once (`dtu_spell_end_contingency`,
    dtuspell.c; his other effects stay), and the message is "The altar of
    Velsharoon cracks apart. With a howl of rage the wards of Morik
    Stormhand fail!"
  The lich does not come for the player: the grills keep him in his room,
  as in 1.3.
- **The fight.** `caster_fight_start` (dtuspell.c) casts no `CS_BUFF` spell of
  the list when `dtu_lich_no_contingency(obj)` is 1 (the lich of CLAN3, the
  setting on, flag 247). The caster log says "contingency: none". A later
  fight starts with none too.
- **The texts** (with the Spanish of `engine/lang/es_port.catalog`):
  - Barander, label 12 (gab.170.3845), before his text of 1.3: "Not by steel
    alone. A lich binds his life to a phylactery, and Morik was a priest of
    Velsharoon: his will be an altar of his dark god, hidden in some crypt
    near his lair. While it stands, his wards rise at his call. Break it
    before you face him."
  - CLAN3 message 1 (trigger 0, segment 26, the arrival), while the altar
    stands, after the text of 1.3: "One set of tracks, made by bare bone,
    leads again and again to the east crypts."
  - The lich, label 4 (gab.190.1513), after the break: "So you found my altar
    and broke it, you meddling worm. No matter. I shall raise another from
    your bones."
  A text of the port replaces a string of a record only when the English
  text of the record at that offset starts as expected.
- **The Dispel Magic scroll.** A spell item of another class does not go on
  the paper doll (0x3533C, `dtu_char_item_usable`), but a hot key box takes
  it (`dtu_char_set_hot_key` has no class test) and the attack key casts it.
  So a fighter and a thief read both versions (13 and 55) from a hot key box
  (tests `lich_dispel_scroll_fighter`, `lich_dispel_scroll_thief`); a
  cleric or a mage who picks up the scroll of the own class gets it in the
  spell list. The port places 55, as object 30.
- **The test command** `phylactery=N` of `-chartest`: 0 breaks the altar (as
  a blow of the player), 1 logs its state, 2 puts the player at the door of
  the cell.

## 3. Saved games

The user's save of 2026-11-26 (`SG1.SG`, mission 17) has mission flag 4: the
lich is dead, and CLAN3 gets no altar (checked with a copy of the save and a
warp to CLAN3). Mirror Image and the globe are not in a save (spells.md);
Stone Skin and Shield are, and a broken altar keeps flag 247, so a load in a
fight gives no new contingency.
