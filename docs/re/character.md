# The player character and the inventory (DTUGAME.EXE 1.3)

Status: research notes for the character data and the rules that the game uses
during play. Addresses are linear addresses of the LE image with object 1 at
0x10000 (`gamedata/re/dtu_flat.bin`). "P+0x.." is an offset in the player record.
**Code** means read from the code of the executable. **Data** means read from the
files of the install. **Guess** means not verified.

The port code is `engine/main/dtuchar.c` and `dtuchar.h`. The screens are in
`engine/main/dtudisp.c` (see `ingame-ui.md`).

The front end (character generation, market, quests) is not covered here. It
makes the character and then calls the function at 0x8BB91, which gives the first
items.

## 1. Where the data lives

| Place | Content | Status |
|---|---|---|
| 0x1D331C | `Players[]`, one record of 0x1B37 (6967) bytes for each player. The index of the local player is at 0x13B924. The asserts call the record `P` | Code |
| P+0x97 | 80 bytes: the attributes in use. The asserts call them `PA` (`PA.class1`, `PA.wisdom`) | Code, Data |
| 0x187B64 | 124 bytes: physics values of the player type (mass, drag, thrust, eye offset, 0x2C bytes) and then, at 0x187B90, a second copy of the 80 attribute bytes: the values without the effect of items. The item code restores strength, dexterity and constitution from it and copies the five thief skills from it | Code |
| `SG0.SG` | the save game. Header of 141 bytes, the player record at 141, the 80 base attribute bytes at 7108, then a block that starts with `RLPD` | Data |
| level file | the 13181 byte player block. The game does not read it | Data (lvl.md) |
| `SAVE\` | `SGn.LS`, `CSUF.LS` (600 bytes each) and the level state files `SGn_m.LVL`, `.TIG`, `.TAP`. No character file. The `.LS` files have 101 bytes that are not 0, all of them 1, in a pattern of 6 bytes. Guess: one record for each level (visited flags) | Data |

The `$PLAYER` line of a table (`HMC.BIN` and so on) has the keywords `class`,
`race`, `strength`, `money` and so on. The reader at 0x1F495 parses them and does
not use the values: each case copies the field of the attribute structure onto
itself (0x1FC3C and the next ones). **Code.** So the table does not set the
character.

Correction of `docs/formats/misc.md`: the player record of `SG0.SG` starts at file
offset 141, not at 53. Proof: the name at 141 (P+0x00), the item count at 141+0xF3,
the item list at 141+0x103 with 16 byte records, and the copy of the attributes
right after 141+6967.

## 2. The player record

Only the fields that this research needs. **Code** for the offset, the name is from
an assert or a table keyword where one exists.

| Offset | Type | Meaning | Value in `SG0.SG` |
|---|---|---|---|
| 0x00 | char[] | name | "Elmac" |
| 0x17 | int | object number | |
| 0x53 | fix | hit points (the Descent `shields`) | 7.0 |
| 0x77 | int | experience of class 1 | 0 |
| 0x7B | int | experience of class 2 | 0 |
| 0x97 | 80 bytes | the attributes, see below | |
| 0xF3 | int | `P.inventory_count`, at most 127 | 6 |
| 0xF7 | int | count of the mage spell list | 6 |
| 0xFB | int | count of the cleric spell list | 0 |
| 0xFF | int | count of the key item list, at most 22 | 0 |
| 0x103 | 127 x 16 | the item list | |
| 0x8F3 | 127 x 16 | the mage spell list | |
| 0x10E3 | 127 x 16 | the cleric spell list | |
| 0x18D3 | 22 x 16 | the key item list (misc numbers 107 to 124) | |
| 0x1A53 | byte | the thief level + 1 that Mirt last gave skill points for, 0 before the first visit (0xABB53) | 0 |
| 0x1A5C | byte | THAC0 | 20 |
| 0x1A93 to 0x1A9F | 4 int | end times of potion effects (0x1A97 strength, 0x1A9F dexterity; guess for the other two). In a save: the seconds left, cut down by 0xBA857 (`sar 0x10` of end minus now, verified in the bytes; defect D-138). The port rounds to the nearest second (`Dtu_fix_save_seconds`, `-origchar` clears it) | 0 |
| 0x1AA3 | char[16] | the name again | "Elmac" |
| 0x1AB3 | word | the thief skill points to spend (set by 0xABB53, spent by 0xB5BB9) | 60 |
| 0x1AB9 | ref | the bow slot | none |
| 0x1ABB | ref | the crossbow slot | none |
| 0x1ABD | fix | time: no pick up before it (set to clock + 5.0 by a drop) | 0 |
| 0x1AC1 | 10 refs | `P.primary_hot_key[]` | items 1 and 5, spells 0 to 5 |
| 0x1AD7 | ref | the item in the hand ("current item") | item 1 |
| 0x1AD9 | 10 refs | the slots of the paper doll, see section 6 | none |
| 0x1AED | char[13] | model file name | "hmtd_p.pof" |
| 0x1B03 | int | portrait number | 0 |
| 0x1B07 | 12 int | sound numbers of the `$PLAYER` line | |

A reference ("ref", the asserts say `P.boot.n`) has 2 bytes: the list (0 items, 1
mage spells, 2 cleric spells) and the index `n` as a signed byte, -1 for none.
The function 0x35863 returns the entry of a reference.

### The 80 attribute bytes (P+0x97)

| Offset | P+ | Type | Field | `SG0.SG` | Status |
|---|---|---|---|---|---|
| 0x00 | 0x97 | s8 | class 1: 0 cleric, 1 mage, 2 fighter, 3 thief | 3 | Code (0x35A09 to 0x35A6F, the string `CMFTRT`) |
| 0x01 | 0x98 | s8 | class 2, not 0 to 3 for one class | 1 | Code |
| 0x02 | 0x99 | s8 | unknown | -1 | |
| 0x03 | 0x9A | s8 | race: 0 human, 1 dwarf, 2 elf, 3 dark elf, 4 halfling | 0 | Code (name table 0x13AE64) |
| 0x04 | 0x9B | u8 | `female` | 0 | Code (keyword) |
| 0x07 | 0x9E | u8 | spell class bits: 1 cleric, 2 mage | 2 | Code (keyword `spell_class`) |
| 0x08 | 0x9F | u8 | `age` | 21 | Code (keyword) |
| 0x09 | 0xA0 | u8 | effect bits, not decoded | 0 | |
| 0x0A | 0xA1 | s16 | maximum hit points | 7 | Code |
| 0x0C | 0xA3 | s16 | hit points as an integer (the game counts in P+0x53) | 7 | Code |
| 0x0E | 0xA5 | s16 | `damage_dice`: sides of the die of the empty hand | 2 | Code (0x2AF7F) |
| 0x10 | 0xA7 | s16 | `#damage_dice`: number of dice of the empty hand | 1 | Code |
| 0x12 | 0xA9 | s16 | `to_hit_bonus` of the empty hand | 1 | Code |
| 0x14 | 0xAB | s16 | `damage_bonus` of the empty hand | 2 | Code |
| 0x16 | 0xAD | s16 | `armor_class`, result of 0xAFB55 | 6 | Code |
| 0x18 | 0xAF | s16 | `PA.class1_level`, 0 to 18. 0 is the first level: the screen prints the value plus 1 | 0 | Code |
| 0x1A | 0xB1 | s16 | `PA.class2_level` | 0 | Code |
| 0x1C | 0xB3 | s16 | armor class change of the dexterity | -4 | Code |
| 0x1E | 0xB5 | u8 | `infravision` | 0 | Code (keyword) |
| 0x20 | 0xB7 | u16 | `strength` | 18 | Code |
| 0x22 | 0xB9 | u16 | the percent value of strength 18 | 30 | Code |
| 0x24 | 0xBB | u16 | `wisdom` | 11 | Code |
| 0x26 | 0xBD | u16 | `intelligence` | 18 | Code |
| 0x28 | 0xBF | u16 | `charisma` | 14 | Code |
| 0x2A | 0xC1 | u16 | `constitution` | 16 | Code |
| 0x2C | 0xC3 | u16 | `dexterity` | 18 | Code |
| 0x2E | 0xC5 | s16 | `thief_ability` | 1 | Code (keyword) |
| 0x30 | 0xC7 | int | Hide In Shadows, percent | 20 | Code (labels of 0x330C3) |
| 0x34 | 0xCB | int | Move Silently | 20 | Code |
| 0x38 | 0xCF | int | Find/remove traps | 10 | Code |
| 0x3C | 0xD3 | int | Climb Walls | 60 | Code |
| 0x40 | 0xD7 | int | Open Locks | 25 | Code |
| 0x44 | 0xDB | int | gold (`money`) | 102 | Code |
| 0x48 | 0xDF | int | flags. Bit 0 with race 0: half-elf | 1 | Code (0x32CB2) |
| 0x4C | 0xE3 | s16 | alignment 0 to 5: Lawful Good, Neutral Good, Lawful Neutral, True Neutral, Chaotic Good, Chaotic Neutral | 4 | Code (0x32BF7) |
| 0x4E | 0xE5 | s16 | unknown | -1 | |

The character of `SG0.SG` is a half-elf thief and mage of the first level, saved
at the start of the game. All values fit the rules below (armor class 10 - 4,
attack +1 and damage +2 for strength 18, THAC0 20).

Not found in the record: saving throws. No code reads a saving throw table of the
character in the functions that were read. **Guess**: the spell code (`spells.c`)
computes them. Memorized spells are the two spell lists (section 3).

## 3. The item lists

One entry has 16 bytes (**Code** 0xB65BB, **Data** `SG0.SG`):

| Offset | Type | Meaning |
|---|---|---|
| 0 | u8 | type: 5 weapon, 15 armor, 16 misc, 17 spell |
| 1 | u8 | id: the number of the table line |
| 2 | u8 | `icon` of the table line |
| 3 | u8 | low byte of the class bits (`cleric_use` 1, `mage_use` 2, `fighter_use` 4, `thief_use` 0x20, `all_use` 0xFF) |
| 4 | u8 | low byte of the flags of the table entry |
| 5 | u8 | a letter: `_`, `weapon_char` of a weapon, `B` for a bow, `C` for a crossbow, `K` or `N` for a spell |
| 6 | s16 | number of the level where the item was found |
| 8 | s16 | count of the stack or charges. -1 means no limit |
| 10 | u8 | `plot_item` |
| 11 | u8 | spells: a byte of the spell entry (1 in the save game. Guess: the spell level) |
| 12 | fix | `modifier` of the table line |

An empty entry in the save game has the letter `_` and level 1.

Insert (0xB65BB, **Code**). The result code is stored at 0x146874.

1. A spell goes to a spell list (0xB69AD) if the character has the class of the
   spell.
2. Misc numbers 107 to 124 (the `Khelben` keys) go to the key item list: at most
   22 entries (code 4), never the same number twice (code 7).
3. Count 0 is refused (code 1) for all items but misc 102 (`Scroll`).
4. 0xB7303 gives the stack limit and says if the type is stackable. An entry with
   the same type and id takes the new items if the type is stackable. A count of
   -1 makes the entry endless. If the entry is at its limit the insert fails
   (code 6). The limit is `stack_limit=` of the table line (10 if the line has
   none, tbl.md).
5. If no entry takes the items: a new entry at the end. The list holds 127
   entries (code 5 when full). The first shown entry of the inventory screen
   (0x13AD9F) becomes the page of the new entry: `((count - 1) / 10) * 10`.
   A load (0xBB867) and a new character (0x8BC69) set 0x13AD9F to 0, so the
   inventory screen shows the first page (V). The port finds a new entry by the
   count of the list (`inv_check` in `dtudisp.c`); before
   `dtu_display_inventory_reset()` a loaded list of 11 or more entries counted
   as new and the screen opened at its last page, so the potions and the other
   first entries were not on the screen after a load (the user's save of
   Mohair: only `Scroll` on page 2; the list had `Potion of Healing` 3 at
   entry 4).
6. A new weapon is passed to 0x31C41. The function was not read. **Data**: in the
   save game of a new character the weapons stand in the first hot key boxes in
   the order of the list, and the first weapon is the item in the hand. The help
   text 761 says "The order in which you pick weapons up determines what number
   they are assigned to".

There is no weight rule. No code that was read uses `mass` or `weight` of an item
for the player, and the help text 759 says "your bag of holding allows you to
carry a number of heavy items without weighing you down". The limits are the
127 entries and the stack limits. **Code** for the limits, **Guess** that no
other limit exists.

Other functions (**Code**): 0xB6BB3 "has item" (a spell in the spell lists, a
Khelben key in the key list, any other item in the item list), 0xB6D24 "take
count items of a type", 0xB6F01 "remove an entry" with the fix of all references
(0x31C4B), 0xB7084 "use up one item of a reference". 0x31C4B changes only the
references of list 0: a hot key box with a spell (list 1 or 2) keeps its index
of the spell list when an item entry goes away.

## 4. Item data from the tables

The readers fill the tables that `objects.md` lists. The rule values that the
play code reads:

| Value | Where | Used by |
|---|---|---|
| class bits | word 0 of `Misc_info` and `Armor_info` | byte 3 of the list entry. The place where the game refuses an item of another class was not found |
| wear bits | word 4: `helmet` 1, `cloak` 2, `necklace` 4, `armor` 8, `gauntlet` 0x10, `shield` 0x20, `ring` 0x40, `belt` 0x80, `boot` 0x100, `bow` 0x200, `crossbow` 0x400, `lockpick` 0x800 | 0x317EA: the slots that take the item |
| weapon dice | `Weapon_info` 0x0D sides (`damage_dice`), 0x0E number (`#damage_dice`) | 0x2B077 |
| weapon attack type | `Weapon_info` 0x12: 1 melee, 0 missile (`attack_type=PROJECTILE`) | 0x2AAF6, 0x2B0AE |
| weapon bonus | `Weapon_info` 0x36 `to_hit_bonus`, 0x38 `damage_bonus` | 0x2AAE7, 0x2B1E7 |
| weapon fire dice | `Weapon_info` 0x0F and 0x10 (`fire_damage_dice`) | 0x2B338 |
| launcher bonus | `Misc_info` 0x0C `to_hit_bonus` of the bow or crossbow | 0x2AB36 for the attack, 0x2B0EE for the damage too |

The armor class of an item is not read from the table entry. The executable has
its own tables, indexed by the item number (**Code** 0xAFBDA and the next
ones):

- armor numbers 0 to 52 at 0x145C04. The values are minus
  `armor_class_decrement` of the table lines of the disc for all lines that have
  the keyword. Four lines without the keyword have -1 in the table: 33 (Amulet
  of Shield), 50 (Gloves of Dexterity), 51 (Glim-Gauntlet), 52 (Gauntlets of Ogre
  Power).
- misc numbers 0 to 127 at 0x145A04: -1 for 8 (Boots) and 9 (Gauntlets), else 0.
- weapon numbers 0 to 82 at 0x1458B8: -9 for 54 (Sword of W'hin), else 0.

The effects of items are code for single item numbers (0xAFB55, 0x37A6A). The
keywords `set_strength`, `pc_spell`, `pc_spell2` and `duration` of the lines are
not what the code reads there.

The port decodes these values from the record text in `dtu_item_get()`.

## 5. Derived values: 0xAFB55

The function runs after each change of the equipment (**Code**). It sets the
armor class, and it changes strength, dexterity, constitution and the thief
skills for some items.

1. The five thief skills are copied from the base attributes.
2. Each slot adds the table value of its item to a sum. Exceptions:
   - A Cloak of Protection (armor 47 to 49) counts only with no shield (armor 2,
     12, 13), no magic armor (5 to 11, or Bracers 39 to 42) and no heavy armor
     (3, 4, 7 to 11).
   - Rings of Protection (armor 0, 14, 15): with magic armor no ring counts. Of
     two rings of protection the better one counts.
   - ASCENT: `Dtu_fix_bracers_ac` (default, `-origcombat` clears it): the
     Bracers are no magic armor for these two rules (0xAFEA6 ors the bracers
     test into the magic armor flag). `Dtu_fix_ring_magic_armor` (default,
     `-origcombat` clears it): only the highest magic protection bonus counts,
     of the magic of the armor (the armor class of the armor minus that of
     Leather Armor, Chain Mail or Plate Mail), the better Ring of Protection
     the Cloak of Protection and the Amulet of Protection (armor 28 to 32; 1.3
     adds it always). The ring and the cloak count with any armor,
     shield or bracers. A shield adds all of its armor class, as in 1.3.
     Decisions of the user.
3. Items that set a value while worn: Girdle 38 strength 19, Gauntlets 52 and
   Ring 26 strength 18 (percent 0), Ring 21 constitution 18, Gloves 50 dexterity
   plus 4, 2 or 1 (table 0x146240) and Open Locks plus 10 for a thief (37 for
   others), Cloak 43 Hide In Shadows 95, Ring 27 doubles the mage slots of the
   levels 1 to 3, weapon 54 in the hand Find/Remove Traps and Climb Walls 250 (0xB03BC, 0xB03C2). The spell
   effects of worn items: combat.md B6a.
4. Heavy armor takes 250 from Hide In Shadows, Move Silently and Climb Walls.
   Misc 9 (Gauntlets) takes 250 from Find Traps and Open Locks. Lockpicks in
   the lockpick slot add 5, 10, 15 or 20 to Open Locks for a thief (misc 10, 13,
   11, 12).
5. The item in the hand adds its table value.
6. Each thief skill is limited to 0 to 95 (0xB0B7E).
7. Armor class = 10 + sum, then the spell effects (not decoded: bits of the
   player object at 0x88 to 0x8B give fixed values 6 and 3, plus 4, minus 2),
   then the dexterity value of the table at 0x14019A (dexterity 0 to 19: 5, 5, 5,
   4, 3, 2, 1, 0 for 7 to 14, -1, -2, -3, -4, -4), then the limit -10 to 10.
8. 0xB0A92 returns the result. The screens print it.

The table 0x14028E (21 dwords, dexterity 0 to 20): -9, -6, -4, -3, -2, -1, 0
for 6 to 15, 1, 2, 2, 3, 4. Its only reader is the accessor 0x8C047, which
clamps the dexterity to 1 to 19 (0x8C047 to 0x8C064) and which no code calls,
so 1.3 has no dexterity bonus to hit. For 1 to 19 the values are the AD&D 2nd
edition "Missile Attack Adjustment". Checked 2026-11-24 in the bytes of the
executable and the instructions. The port uses it for the attack roll of a
spell of the player (`Dtu_fix_damage_save`, dtuspell.c `Dex_missile`) and of a
weapon missile of the player (`Dtu_fix_dex_missile`).

The attack roll of a weapon of the player (0x2A7F3, checked 2026-11-24 in the
instructions): the weapon to-hit bonus (0x2AAE7); for attack type 0
(PROJECTILE, `Weapon_info+0x12`, 0x2AAF6) the to-hit bonus of the bow (flag
`arrow`) or else of the crossbow (0x2AB0A to 0x2AB91), and nothing more; for a
melee weapon (type 1) or `Weapon_info+0x0A` = 3, the backstab +4 and the
strength bonus 0x2A789 (0x2AB4F to 0x2AB91); a PROJECTILE weapon never
reaches that test. The damage (0x2AF5E): a missile adds the same launcher bonus
(0x2B0AE to 0x2B105); the strength damage bonus (table 0x13A9A2, the 18/xx
part 0x13AA06) goes to attack type 1 or `Weapon_info+0x0A` = 3 (0x2B475 to
0x2B505). The reader stores 3 there for `thrown=1` of the line (0x21020; 1 for
`bounce`, 2 for `stick`). The thrown weapons of the tables (Dart, Throwing
Dagger, Throwing Axe, Throwing Hammer and their + versions) are PROJECTILE with
`thrown=1`: no strength to hit, the strength bonus to damage, and the launcher
bonus of the worn crossbow (they have no `arrow` flag) to hit and to damage.
Arrows and bolts get no strength. AD&D 2nd edition gives a bow no strength
bonus and a thrown weapon the strength bonus to damage (and to hit). The port
gave a thrown weapon no strength damage (it did not read `thrown`): corrected
2026-11-24 (`dtu_item_data.thrown`, `dtu_char_weapon_damage`). The launcher
bonus of a thrown weapon is a defect (`Dtu_fix_thrown_launcher`), and the
missing strength bonus to hit differs from 2e (`Dtu_fix_thrown_str_hit`); see
combat.md, the list of the settings.

## 6. Equipment and the paper doll

The paper doll has 12 places (**Code** 0x30650 to 0x31193, positions from the
tables at 0x1443E0 and 0x144440):

| Place | Reference | Wear bit | Low res | High res | Text |
|---|---|---|---|---|---|
| lockpicks | P+0x1AD9 | 0x800 | (67,92) | (133,184) | 787 |
| cloak | P+0x1ADB | 2 | (193,51) | (383,103) | 665 |
| necklace | P+0x1ADD | 4 | (67,52) | (133,103) | 666 |
| armor | P+0x1ADF | 8 | (191,72) | (383,143) | 667 |
| gauntlet | P+0x1AE1 | 0x10 | (191,92) | (383,183) | 668 |
| shield | P+0x1AE3 | 0x20 | (67,72) | (133,143) | 669 |
| ring 1 | P+0x1AE5 | 0x40 | (73,123) | (141,244) | 670 |
| ring 2 | P+0x1AE7 | 0x40 | (187,123) | (374,246) | 671 |
| belt | P+0x1AE9 | 0x80 | (72,162) | (132,326) | 672 |
| boot | P+0x1AEB | 0x100 | (186,162) | (374,317) | 673 |
| bow | P+0x1AB9 | 0x200 | (164,11) | (328,22) | 785 |
| crossbow | P+0x1ABB | 0x400 | (193,11) | (384,22) | 786 |

The string table has "Put Helmet here" (664) too, and the wear bit 1 exists, but
no place takes it.

An item stays in the list when it is worn. A slot is a reference. The mouse works
like this (**Code**):

- A click on an entry of the list or on a filled slot with an empty cursor: the
  cursor takes the item (flag 0x13ADAF, copy of the entry at 0x1BD46C, reference
  at 0x1BD694). The cursor picture becomes the icon. The slot becomes empty.
  0x317EA puts the wear bits of the item into 0x1BD578. If 0x3533C refuses the
  entry (the class bits, see below), the wear bits become 0 (0x30344 for the
  list, 0x312C3 after the slot): no slot and not the figure take it then. No
  message is printed there.
- A click on a slot with a held item: the slot takes the reference if the wear
  bits have the bit of the slot. Then 0x4D708 runs 0xAFB55.
- A click on a hot key box with a held item puts the item there (log text
  "Selected hotkey item %d"). 0x31752 says which items can have a hot key: all
  weapons and spells, no armor, a misc item without the flag `non_hotkey_item`.
- The cursor over an empty slot prints "Put ... here" in the description bar.
- The button DROP (0x35FD6) drops the held item. The button USE (0x3614B) uses
  the held item if it is of the item list, or with an empty cursor the item in
  the hand if it is a misc item (0x2FAF0).
- Press and drag. 0x2FB10 sets edi to 1 only in the frame where the button goes
  down (0x2FDCD, 0x13AD7E keeps the last state). The list cells (0x3023F), the
  slots (0x30640) and the hot key boxes (0x304AA) act only then: a press. The
  buttons act in each frame while the button is down: 0x3009B calls
  0x9865B(list, 1, x, y), which gives the press handler (+0x46, 0x98E99, the
  press action +0x3E) to each button under the cursor whose flag +0x30 is clear
  (0x98741), and the release handler without action to a pressed button that
  the cursor left (0x987AD). With the button up (0x312FA) a pressed button
  under the cursor gets the release handler with the action (0x98782). So an
  item taken at the press and dragged with the button down acts on the figure of
  the paper doll when the cursor reaches it, and on USE or DROP at the release
  over them. A drag onto a slot or a hot key box does nothing: a click is
  needed there.
- A press in the view (0x2FE1F, the rectangle 0x1C7F64) in each frame while the
  button is down, with a held item: if 0x13ADB0 is 1 (the entry may be dropped:
  an entry of the item list, not `plot_item` (entry byte 10) and not 0xB611C,
  a word of the table entry of the type) and 0xB715A passes (fewer than 700
  objects), 0xB717B drops it (0x2FE5B) and the cursor is empty; else
  0x319FF(*0x3969EC) (not read). The DROP button tests the same 0x13ADB0. Not
  in the port: the port drops by DROP only and does not test 0x13ADB0.

### Spells in the hot keys (Code)

A hot key box is a reference (section 2): list 0 is the item list, list 1 the
mage spell list (P+0x8F3), list 2 the cleric spell list (P+0x10E3), and the
index is the entry of that list (0x35863). The user's save `SG0.SG` (a thief and
mage) has the boxes 3 to 8 at (1,0) to (1,5): Magic Missile, Light, Armor,
Shield, Detect Magic and Sleep of the mage list.

- Draw (0x329CB, each box): the entry of 0x35863; the icon is byte 2 of the
  entry (for a spell: `icon=` of the spell line, set by 0xB69AD), the text is
  0xB758D, which calls 0xB7487(entry, list). 0xB7487 writes into the buffer at
  0x3D4F78: a spell of list 1 or 2: `"%s%d"` (0x1324C2) with "Lvl " (high
  resolution) or "L-" (low resolution, pointer table 0x146880) and the word at
  entry +6 (the spell level). An entry of the item list (types 5, 15, 16, and a
  spell in the item list: a scroll): no text for a count of 0 or 1, "Many"
  (high) or "+++" (low, table 0x146878) for -1, else `"%d"` with the count; misc
  102 (the Scroll) has no text; other types "???". The text area is the
  quantity area of the box (color (32,0,0)). The border (63,63,0) marks the box
  whose reference (both bytes) is the reference of the hand P+0x1AD7.
- The inventory screen shows one of three lists, `BrowseItemType` at 0x13ADB1
  (asserts at 0x35D2C: 0 to `JRG_VIEWING_PRIEST`, 2). Each list has its first
  entry: 0x13AD9F items, 0x13ADA5 mage spells, 0x13ADA9 cleric spells (0x36F2A).
  The button SPELLS (high: record 0x14408C, rectangle (18,324) 76x32, images
  (424,78) and (344,78) of sheet 69; low: record 0x143934, a circle at (22,165)
  with radius 15 (hit test 0x99141), drawn at (6,150) 35x35, images (477,2) and
  (440,2) of sheet 77) has the release action 0x37180: the list becomes the next
  of 0x3713C: from the items to the mage spells for a mage (0x35A2B), else to the
  cleric spells for a cleric (0x35A09); from the mage spells to the cleric
  spells for a cleric, else to the items; from the cleric spells to the items.
  Its release handler 0x98F38 runs the action, then 0x37192 sets 0x13B0BB to 1
  when a spell list is shown and the next list of 0x3713C is the item list
  (else 0). With 0x13B0BB set the button images (+0x56 at rest, +0x5A pressed)
  become PatchBtnUp (0x142EEF) and PatchBtnDn (0x142EEB); with 0x13B0BB clear
  0x9902F puts the images of the sheet back. 0x9881C (high resolution only,
  called at 0x34401 with the flag 0x13A94B) loads `HRISBTN.PCH` (a PCX file,
  96x80, 8 bits) and copies two 76x32 parts: PatchBtnDn from (2,2), PatchBtnUp
  from (2,36), with the pixel values as they are (the new bitmaps are filled
  with color 13 first, 0xD84D0). Both parts show "INV.": the button reads INV.
  while a press leads back to the items. The low resolution button has the
  plain release handler 0x9905A and keeps its images. The list cells draw each spell with its icon and "Lvl N"
  (0x327D7 with 0xB7487 and the list number).
- A click on a list cell with an empty cursor (0x3029E) copies the entry to
  0x1BD46C and the reference (list 0x13ADB1, index) to 0x1BD694: the cursor
  holds the spell. 0x13ADB0 (may drop) is 0 for a spell list, so DROP does
  nothing; USE (0x36189) does nothing with a held spell.
- A click on a hot key box with a held entry (0x3049D to 0x30506): the box takes
  the reference if 0x3533C (usable) and 0x31752 (may stand in a box: all spells
  and weapons) pass and no box holds the reference yet (0x35462). The cursor is
  empty then; the log text is "Selected spell %s as weapon" (0x123CEC), but the
  hand does not change. Else, with an entry in the box, the box acts as its key
  (0x3056E: log "Selected hotkey item %d", 0x43A5A) and the cursor keeps the
  spell. The click does not change 0x13AD15 (the box that was chosen last).
  So when the box was chosen last, its key and a click on it are refused
  (0x43D32, sound 3000) with the new entry in the box, until another box or
  the empty hand is chosen. The key 1 to 0 with a held entry does not have
  this defect: 0x31A55 returns 1 for the box of 0x13AD15 and the caller
  arms the box. The port refuses only while the box holds the hand
  (`Dtu_fix_hot_key_refresh`, `-origchar` clears it).
- 0x3533C for a spell entry: spells 39, 40 and 53 always; else the character is
  a caster (0x359F4: cleric 0x35A09 or mage 0x35A2B), the count is 1 or more (or
  -1), and the class bits of byte 3 fit a class of the character (0x352F7).
- The figure of the paper doll (high: records 0x1440EA (244,24) 55x57 and
  0x144148 (210,81) 122x278; low: 0x1441A6 (120,12) 31x29 and 0x144204 (101,41)
  67x140) has the press action 0x35ADF and no release action. It does nothing
  with an empty cursor (0x35AE6) or in mode 6 of the low resolution (0x35AF3).
  The first wear bit of 0x1BD578 in this order gives the slot: cloak 2,
  necklace 4, armor 8, gauntlet 0x10, shield 0x20, ring 0x40 (ring 1 if the
  index byte of ring 1 is negative, else ring 2, 0x35BF4), belt 0x80, boot
  0x100, bow 0x200, crossbow 0x400, lockpicks 0x800 (0x35B09 to 0x35CE0). The
  slot takes the reference (the item that was in it stays in the list), the
  cursor is empty, 0x4D708 (an empty function) and 0xAFB55 run. The helmet bit
  1 has no case. Else, with a held weapon or spell that 0x3533C
  passes (0x35CE5): if a box holds the reference the cursor keeps it. Else the
  first empty box takes it, or with no empty box the box that was chosen last
  (0x13AD15), box 0 if none; an arrow and a bolt have a second test
  (0x35EDE with 0x35F9D, 0x35F0F with 0x35F64): an arrow without a bow or a
  bolt without a crossbow gives sound 3000 with no message and the cursor is
  empty; then 0x13AD15 = -100 and 0x43A5A(box): the entry goes into the hand.
  Anything else (a misc item, an armor without a slot bit, an entry that
  0x3533C refuses) is used: 0x37A6A with the index byte of the reference
  (0x35E9C), and the cursor is empty. So a potion or the Scroll put on the
  figure is used as with USE. For a held spell that 0x3533C refuses the index
  is one of the spell list, which 0x37A6A reads as an index of the item list.
- A key 1 to 0 (0x430C9 to 0x4310A, the scan codes 2 to 11), a click on a box
  that is not empty with an empty cursor (0x3056E), Cycle Weapon (0x545EB) and
  0x4448D (the first box that is not empty, "Autoarming anything") call
  0x43A5A(box): P+0x1AD7 = the reference of the box, 0x13AD15 = box. Its tests,
  in this order:
  1. 0x7F13D, 0x404318 and 0x13D478 (not traced): return with no sound.
  2. The animation +0x170 of the player object is 6 or 11 (an attack): return.
  3. 0x13B69F is not 0: the attack key was pressed and its attack or cast has
     not started (0x44300 and 0x41A4D keep it while the cast time runs; 0x44481
     and 0x9305D clear it): sound 3000 (0xBB8, `0x2D955(0xBB8, 0x10000)`, volume
     1.0), unless 0x13D2C8 is 2 (not traced), and return.
  4. The box is empty (index byte negative): the empty hand (0x43CBD: P+0x1AD7 =
     (0, -1), 0x13AD15 = -1, the arms model with `_`). No sound.
  5. 0x35863 finds no entry: sound 3000.
  6. A weapon with the flag `arrow` (Weapon_item_info +0x3E bit 4) and no bow
     (P+0x1ABA negative), or with `bolt` (bit 8) and no crossbow (P+0x1ABC):
     "You need a %s to fire %ss" (0x1258C0 with "bow"/"arrow" or
     "crossbow"/"bolt", 0x347A7) and sound 3000.
  7. The box is 0x13AD15, the box that was chosen last: sound 3000.
  With a held entry a key 1 to 0 does not call 0x43A5A directly: 0x31A55 puts
  the entry into the box with the tests of a click (0x3533C, 0x35462) and empties
  the cursor; only if that box is 0x13AD15, 0x13AD15 becomes -1 and 0x43A5A(box)
  arms it. For a spell 0x43A5A also sets P+0x47: for a spell with `uses=-1`
  game time + `duration` + (caster level + 1) * `level_duration`, else
  0x3FFFFFFF (no reader of P+0x47 was found). The model of the arms takes byte 5
  of the entry: `K` (cast with the hands) for a spell of a book (0x43EF8).
- The attack key then casts the spell of the hand: the checks and the cast time
  of 0x41A4D, the cast of 0x56A0D, one slot of the spell level (`combat.md`, B1).

The lockpick items of the tables of the disc (misc 10 to 13) have no wear
keyword. **Guess**: the reader or the insert sets the bit for them, because
0xB02D6 looks for these four numbers in the slot.

## 7. Use: 0x37A6A

The function takes the index of an entry of the item list and has one case for
each item number. Read in detail (**Code**):

- misc 79, Potion of Healing (0x38E27): if the hit points are below the maximum,
  add `rand() % 7 + 4` (4 to 10), message "You feel better.". The item is used up
  in both cases.
- misc 75, Potion of Extra-Healing (0x38C1A): `rand() % 22 + 6` (6 to 27), "You
  feel much better.".
- At the end 0xB7084 takes one item from the entry.

All cases are in combat.md B6 and B6a (potions, the Gem of Curing, the spider
pieces, the scrolls, weapon 13, armor 45, the potion timers 0x396B2, Luck). The
effects of worn items (rings, boots, cloaks, amulet, gauntlets, weapon 54) are
the item bits of 0xAFB55 and 0xB08A8 (combat.md B6a). Wands are weapons with
`pc_spell`: the attack casts the spell (combat.md B1). The game has no
identification of items: no string of the executable and no keyword of the
tables contains "identif" (searched), and the item list has no field for it.

## 8. Drop: 0xB717B

**Code.** No drop with 700 or more objects. The function works on the inventory
screen (mode 3 or 6). It writes "dropping type %d id %d (action %d)", makes the
object with 0x3C93B(player object, type, id, count) and removes the whole entry.
It sets P+0x1ABD to clock + 5.0: the pick up test 0x31A0E refuses items until
then. Then 0xAFB55. The place of the new object was not read.

## 9. Rest: 0x360D5

**Code** (0x3E137 and 0x3E1F6 in `fuelcen.c`). The player can rest in a segment
whose `special` byte is 2 or 6. The level files have 31 such segments in 22
levels, all with the value 2 (lvl.md). Each rest place has an entry in the
station table (0x1C5E28, 40 bytes, the time at +0x18). If the place is not a rest
place or its time is in the future: message "You can not rest here". Else:

- hit points = maximum (P+0xA3 and P+0x53),
- message "You feel refreshed" (a dialog box if a screen is open),
- sound 62 at half volume,
- the time of the place = game time + 300 seconds,
- the fire of the place is removed (0x3DC0F, log text "killing fireball %d"),
- 0x160AB(player, 5) and 0x92FF2 (not read. Guess: the spells come back).

The fire of a rest place (V, code 0x3E037 to 0x3E118 in the station frame
0x3DCAF, called each game frame from 0x43864): this is the rotating silver
ankh. For each station of type 2 or 6 whose time (+0x18) is not in the
future, and whose segment is in the visible list (0x38C060 through 0x389A06),
0x3DC0F looks for a fireball (type 1) with the object flag 0x40 in the
segment. If there is none, the frame makes one with 0x3BD4C, the Descent 1
`object_create_explosion(segnum, station center (+0x1C), 3.0, vclip 91)`, and
sets type 1, bit 2 at 0x1AE and the object flag 0x40. The log text is
"Creating Charged Rest Stop Fireball %d at %d." (" - FAILED" when the segment
has no objects after it, which ends the loop). The vclip ends after its play
time and the next frame makes a new one. The whole station frame is skipped
while the byte 0x1C7FD4 (from 0x9CC99, not read) is set.

A rest (0x3E354) calls 0x3DC0F until it finds no fire: each fire gets
lifeleft 0 and 0x68F45 deletes it ("killing fireball %d"). So the ankh is
gone for the 300 seconds of the rest time.

The level state reader kills every fireball (levelstate.md, section 3), so
the fire is never in a loaded level: the station frame makes it again. The
port does the same in `fuelcen_update_all()` (fuelcen.c); it takes the
segments of the last rendered frame for the visible list and does not test
0x1C7FD4.

## 10. Experience and levels

**Code.** 0x4EA50 (the Descent `add_points_to_score`) calls 0xAF3CF with the
points. So a kill gives `score_value` of the creature line. A trigger can give
experience too (field 0x30 of the trigger, 0x93D8E), and the quests of the front
end do.

0xAF3CF: with two classes the points are divided by 2 (at least 1). The result
is added to P+0x77 and, with two classes, to P+0x7B. Then 0xAF489:

- 0xAF43A raises the level of a class while `level + 1 < 18` and the experience
  is at least `table[class][level + 2]`. The table pointers are at 0x146048.

| Class | Experience for the levels 2, 3, ... |
|---|---|
| cleric | 1500, 3000, 6000, 13000, 27500, 55000, 110000, 225000, 450000, 675000, then +225000 |
| mage | 2500, 5000, 10000, 20000, 40000, 60000, 90000, 135000, 250000, 375000, 750000, then +375000 |
| fighter | 2000, 4000, 8000, 16000, 32000, 64000, 125000, 250000, 500000, 750000, then +250000 |
| thief | 1250, 2500, 5000, 10000, 20000, 30000, 70000, 110000, 160000, 220000, 440000, then +220000 |

- A new level shows "Your character has attained new %s level" (text 844).
- 0xAF694 gives the hit points of each new level: one die of the class (cleric
  d8, mage d4, fighter d10, thief d6) rolled as `rand() % (die - die/2 + 1) +
  die/2`, so at least one half. After level value 9 (cleric, fighter) or 10
  (mage, thief) a fixed 2, 1, 3, 2 in place of the die. Plus the constitution
  value (tables 0x1402E2 and, for a fighter, 0x140332: +1 at 15, +2 at 16, and
  +3, +4, +5 at 17, 18, 19 for a fighter, +2 for others). With two classes the
  sum is divided by 2. The result is added to the maximum and to the hit points.
- 0xAF605 sets THAC0 = `table[class][level + 1]` (bytes, pointers at 0x146590).
  With two classes it takes the higher (worse) of the two values.

| Class | THAC0 for level value + 1 = 0, 1, 2, ... |
|---|---|
| cleric | 20, 20, 20, 18, 18, 18, 16, 16, 16, 14, 14, 14, 12, 12, 12, 10, 10, 10 |
| mage | 20, 20, 20, 19, 19, 19, 18, 18, 18, 17, 17, 17, 16, 16, 16, 15, 15, 15 |
| fighter | 20, 19, 18, 17, 16, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3 |
| thief | 20, 20, 19, 19, 18, 18, 17, 17, 16, 16, 15, 15, 14, 14, 13, 13, 12, 12 |

The index is the level value plus 1, and the level value of the first level is
0. So a fighter of the first level has THAC0 19 and the tables are one level
ahead of the rule books. The port keeps this.

0x37253 gives the screens the experience and the points to the next level:
`table[class][level + 2] - experience`, for two classes the smaller value times
2. The percent value is the rest times 100 divided by the span of the level.

## 11. Combat values

**Code**, 0x2A7F3 (attack roll: attacker, target, weapon number or -1) and
0x2AF5E (damage).

- Roll `rand() % 20 + 1`. A 20 is a critical hit (result 2, "You made a critical
  hit!"). A 1 is a critical fumble (result -1, "You made a critical fumble!"; for
  the player a second d20 above the dexterity calls 0xAE837, which changes the
  item in the hand, guess: the weapon falls; else the player loses the balance).
- Player with a weapon: add `to_hit_bonus` of the weapon. Melee: add the
  strength value of 0x2A789 (table 0x13A956: -5 at 1, -3, -3, -2, -2, -1, -1, 0
  for 8 to 16, +1 at 17 and 18, +3 at 19; a fighter with 18 adds 1, 1, 1 or 2 for
  the percent value). Missile: add `to_hit_bonus` of the bow, or of the crossbow
  for a weapon with flag 4.
- Player with the empty hand: add P+0xA9 and the strength value.
- The target is hit if `THAC0 - armor class <= roll`. THAC0 of the player is
  P+0x1A5C, of a creature `Robot_info` + 0x74. The armor class of the player is
  P+0xAD.
- Damage of the empty hand: P+0xA7 dice with P+0xA5 sides, plus P+0xAB.
- Damage of a weapon: the dice of the weapon, plus `damage_bonus`; melee adds the
  strength value (table 0x13A9A2: +1 at 16 and 17, +2 at 18, +7 at 19; a fighter
  with 18 adds 1, 1, 2, 3 or 4). A backstab (flag 0x13A955) multiplies by
  `(thief level + 7) / 4`.

Not decoded: what the caller does with a critical hit, the spell bits that
change the roll, the attack rate. **Guess**: `fire_wait` of the weapon line is
the time between two attacks (Descent meaning).

## 12. A new character

**Code** 0x8BB91 (front end, the last step): each character gets misc 102
(Scroll) through 0xB655D, then the items of its class or classes from the lists
at 0x13F6CD and the next ones (an item that the character has already is
skipped):

| Class | Items |
|---|---|
| cleric | Mace (weapon 27), Leather Armor (armor 1), Potion of Healing (misc 79) |
| mage | Dagger (weapon 18), Potion of Healing, six spell places |
| fighter | Broad Sword (weapon 20), Leather Armor, Gem of Curing (misc 2), Potion of Healing |
| thief | Short Sword (weapon 19), Leather Armor, Apprentice Lockpicks (misc 13), Potion of Healing |

**Data**: the save game has exactly Scroll, Short Sword, Leather Armor,
Apprentice Lockpicks, Potion of Healing, Dagger for the thief and mage. Nothing
is worn. The Short Sword is in the hand.

**Guess** from the save game: the first level gives the full hit die plus the
constitution bonus, one half of the sum for two classes ((6 + 2 + 4 + 2) / 2 = 7).
The empty hand is 1d2, and its two bonus fields hold the strength values (+1 and
+2 for strength 18).

## 13. The screens

See `ingame-ui.md` for the frames. New facts (**Code**):

- Right column of the screens: name, hit points, "AC: n" (0xB0A92), "n Exp" (the
  experience from 0x37253), "Gold: n" (text 674, P+0xDB).
- Hot key boxes (0x329CB): icon and the text of 0xB7487 (section 6, "Spells in
  the hot keys"). The box of the item in the hand has a border in (63,63,0).
  The keys 1 to 0 arm the item or the spell of a box (help text 760).
- Inventory list: ten entries in two columns, first entry 0x13AD9F.
- Statistics screen (areas created at 0x99B11, rectangles from the tables at
  0x144938 to 0x144B40, low / high resolution):
  - portrait (15,10) 50x45 / (32,22) 100x90
  - alignment (11,63) / (20,131), race (11,72) / (20,147), "Lvl %d %s" for class
    1 (11,81) / (20,163) and class 2 (11,90) / (20,179); low resolution prints
    " %d %s"
  - labels `STR:` `DEX:` `CON:` at x 11 / 20 and `INT:` `WIS:` `CHA:` at x 43 /
    87, y 101, 109, 117 / 200, 216, 232; the values 19 / 27 pixels to the right
  - thief skills: labels at (108, 30 + 11 n) / (218, 62 + 20 n), values "%d%%" at
    x 183 / 338, in the order Hide In Shadows, Move Silently, Open Locks,
    Find/remove traps, Climb Walls
  - spell tables (areas 0x3BA134 to 0x3BA584, "Mage Spells", "Cleric Spells",
    "Level %d"): not decoded
- Map screen: the function 0x34032 ("crystalBall != NULL") draws it. Not read.
  The port shows the frame only.

## 14. The port

`dtuchar.c` holds one character (`Dtu_char`). The item list is `Dtu_inventory[]`
of `dtuobj.c` (the pick up code fills it); the references of the character are
indexes of it. The hit points are the Descent `shields`, as in the executable.

Implemented from **Code** facts: the attribute structure, item record and
reference with the layout of the executable; reading a character from `SG0.SG`
(option `-char FILE`); the items of a new fighter; 0xAFB55 without the spell
effects; equip and unequip with the wear bits; the two healing potions; drop;
rest; experience, levels, hit points of a level, THAC0; the attack and damage
values; the three trigger hooks (0xB6BB3, 0xB6D0C, and the class and race masks
of 0x93D93).

Differences and guesses of the port:

- The list has 149 entries (`DTU_MAX_INVENTORY` of `dtuobj.h`): the 127 items and the
  22 key items of the executable, with these limits each. The key items are in the
  same list; a save writes them into the key item list (P+0x18D3, count P+0xFF). The
  Scroll has count 1, not 0 (a save writes the 0 of the loaded record back).
- Save games: the load keeps the entries of the item list as they are (no stacking)
  with their 16 bytes, the key items, the two spell lists, the spell slots (P+0x23 to
  P+0x42) and the spell in the hand. The save starts from the loaded record and writes
  the fields that the port has; the bytes of an entry come from the loaded entry. The
  slots of a class that the character does not have stay as loaded (the executable
  gives a thief and mage one cleric slot).
- Spells in the hot keys (section 6): a box of `Dtu_char.hot_key[]` holds list 0 and
  an index of `Dtu_inventory[]`, or list 1 or 2 and an index of the spell list of
  `dtuspell.c` (the order of the save game, which the save writes again). The load
  and the save keep them, and a spell in the hand is saved as the reference of the
  hand (list 1 or 2), as the executable keeps it. The bar draws the icon and
  "Lvl N" / "L-N", the border marks the spell in the hand, the key or a click arms
  the spell (`dtu_spell_take`, no message), and the attack key casts it. The
  inventory screen has the button SPELLS and the spell lists; a held spell goes into
  a box by a click on the box or onto the figure of the paper doll. The
  entries of the item list and of the boxes show the text of 0xB7487 (no "1").
- The figure of the paper doll (0x35ADF) is `dtu_char_put_on_doll` with
  `dtu_char_item_usable` (0x3533C for an entry of the item list): a held item is
  worn, armed or used as above, also by a drag (the figure acts while the button
  is down). The buttons follow 0x9865B: while the button is down the button under
  the cursor is pressed, so a drag onto USE or DROP acts at the release. The
  port's `dtu_char_equip` takes the entry out of another slot that holds it (the
  executable writes the reference only, so a ring of ring 1 taken from the list
  and put on the figure stands in both ring slots). A held spell that 0x3533C
  refuses goes back to its list.
- The tests of 0x43A5A are `dtu_char_press_hot_key` (keys, clicks, Cycle Weapon, the
  paper doll); the sound is `digi_play_sample(3000, F1_0)`. The port's attack request
  (`Request` of dtucombat.c) stands for 0x13B69F. An empty box gives the empty hand.
  0x4448D (a used up thrown stack) arms with no tests, as before. A key with a held
  entry follows 0x31A55. The images of the SPELLS button: `HRISBTN.PCH` (high
  resolution), drawn over the button while a spell list is shown whose next list is
  the item list.
- Not in the port: the tests 1 of 0x43A5A and the case 0x13D2C8 = 2, P+0x47 (no
  reader). The port keeps the item of the hand in
  `Dtu_char.current` while a spell is in the hand (`Dtu_spell_current`); an item
  taken in the hand puts the spell away. The first list after a load is the item
  list (guess). Cycle Weapon passes over the boxes with a spell (0x544F2 takes
  weapons).
- The ability scores, alignment, age and gold of the default character are a
  guess. The front end sets them in the original.
- The class bits are tested when an item is worn or taken in the hand (messages
  825 and 828). In the executable these two messages are pickup refusals: 0x28220
  (misc, after 0x31A0E or the add 0xB65BB fails) and 0x284D7 (armor). They are not
  class tests.
  The executable tests the class bits at the pick up (0x3533C clears the wear
  bits, section 6) with no message; a click on a slot of the port still uses the
  test of `dtu_char_equip`.
- The place of a dropped item (15 units in front of the player, on the floor) is
  a guess. The pick up code of `dtuobj.c` does not ask
  `dtu_char_pickup_allowed()` yet.
- Rest: no sound, the fire stays, the two calls that were not read are missing.
  The time of a rest place is kept for each segment.
- Use: the cases of combat.md B6 (see combat.md C2 for the parts that are not
  in the port). The Gem of Curing works once at the start and once more for
  each mission (0x3891B), as in the game; its counter is the byte P+0x1A57.
- Keys of the inventory screen (comma, period, Enter, U, Backspace) are an
  addition. The original has the mouse.
- The map is not in the port. Spells, missile weapons, saving throws, the
  potion timers and the thief skills are (combat.md part C).

Settings (default 1, the option `-origchar` clears them):

- `Dtu_fix_multiclass_thac0`. 0 gives the rule of the executable: a
  character with two classes gets the worse THAC0 of the two.
- `Dtu_fix_level_hit_points`. 0 gives the rule of the executable: the hit
  points in use grow by n / 65536 at a new level (section 15).
- `Dtu_fix_secret_door_names` (dtuthief.c). 0 gives the rule of the
  executable: a door with texture 0 or no overlay is a secret door
  (section 15).

Test options: `-charlog FILE`, `-char FILE.SG`, `-chartest LIST` (see the
comment at `run_command()` in `dtuchar.c`). `-chartest rules` checks every
race and class combination at levels 1, 5 and 10 (docs/audit/character.md).

## 15. Character rules (audit of 2026-11-22)

**Code** for all facts of this section. The audit table is
`docs/audit/character.md`. The recovered C of the decompilation project is
`D:\dev\DTU-Decomp\recovered\character.md`.

### Levels

- 0xAF43A raises the level value while `level + 1 < 18` and the
  experience (unsigned) is at least `table[class][level + 2]`. A level
  value above 17 is set to 17 first. So the highest level is 18 (level
  value 17) for every race and class. No race limit exists.
- 0xAF605 reads THAC0 at index level value + 1. At level value 17 the index
  is 18, one past the 18 bytes of a row: 1.3 reads the first byte of the
  next row (cleric 20, mage 72, fighter 20, thief 20). The port stops at
  index 17.
- 0xAF694 adds, for each new level, the dice (number at 0x146538, die at
  0x146518) + the fixed value (0x146528) + the constitution value of the
  base copy (0x187BBA, at most 19; fighter table for the fighter class).
  The sum of both classes is halved (`sar`). Then 0xAF88B adds it to the
  maximum (0x187B9A, P+0xA1) and to the integer hit points (0x187B9C,
  P+0xA3) as words, and 0xAF8C7 and 0xAF8CD add the same integer to the
  fixed point hit points of the player object and of P+0x53. Those grow by
  n / 65536: a defect.
- 0x8BA3A(attributes, level), used by MakePlayer (level 1) and by script
  statement 9F (0xAB71A, the higher class level + 1 through the arithmetic
  of the statement, plus 1 for two classes): level = min(level, 18) - 1,
  minus 1 more for two classes, at least 0, for both classes; experience =
  `table[class][level + 1]` (0 for a missing class 2); thief points
  P+0x1AB3 = 60 + 30 * level when the class mask of the front end
  (0x3A6465) has the thief; hit points = (0x13FDF3[class] + 0x8C01B(con,
  class is fighter)) * (level + 1), the mean of two classes; the maximum and
  the integer hit points of the record are set, P+0x53 is not; then THAC0.
  The script statement is used once in GABE (a reward: "LEVEL += 1" with a
  chance of 1 in 4 while LEVEL < 18).

### Thief points

- A new thief has 60 points (0x8BACD, `SG0.SG` has 60). Mirt (0xABB53)
  sets the points to `(thief level - paid) * 20 + (paid == 0 ? 20 : 0)`
  before the screen 0xB5B49 is shown, so the 60 are never spent: the first
  visit gives 40.
- `Dtu_fix_mirt_points` (default, -origscript clears it, D-090 verified in
  the bytes 2026-11-23): Mirt adds the award to the points that are left
  and pays the level at once; each talk offers the points that are left. A
  new thief has 100 points after the first talk.
- THAC0 (D-002, D-089, verified in the bytes 2026-11-23): `Dtu_fix_thac0_row`
  takes the last entry of the row at level 18 (1.3 reads the byte past the
  row: 20, a mage 72). `Dtu_fix_new_thac0` gives a new character the THAC0
  of the table at once (1.3 keeps the 20 of 0x8BFF5 until the first level
  check, because 0xAFB55 does not call 0xAF605). -origchar clears both.

### Find/Remove Traps

- 0xB4040: the search of 0xB365F and 0xB396C runs when P+0xCF is not 0 or
  the Find Traps effect is on. The dwarf flag (effective race 1, not a
  thief, no spell) only makes 0xB34E0 and 0xB353A refuse a side with side
  flag & 3.
- 0xB353A: a trigger is harmful when it is on and its experience (+0x30,
  dword) is below 0, or else by the first of: the first trap (+0x3B, that
  trap only), the enabled trigger (+0x3D, recursive), the enabled trap
  (+0x3F).
- A found trigger sets side flag 4 of the searched side (0xB378E), a found
  trap side flag 8 (0xB3ABB); a removal clears them (0xB38EC, 0xB3C7D). The
  automap shows them. The sounds 3001 and 2130 are only in the path of the
  spell. The distances use the quick distance 0xD705C.
- The state bytes are in the records: trigger +0x0F, +0x10; trap +0x03,
  +0x04; wall +0x16 (racial search), +0x17 (Open Locks). The fresh branch of
  level_start (0xAD64A to 0xAD6FC) clears the side flags, the two wall bytes
  of the 300 walls, the trigger fire count and bytes, and the trap bytes. A
  restore reads them from the state files.

### Level textures: 0xA6F89

- Called at 0xAD744 for every level start. 0x1B2B1 compares a name with
  `TmapInfo[i].filename` (0x1A0F44, 26 bytes each, at most 2500) and gives
  the index, or 0.
- Tables: water 0x3CE0E4 (water, water1, water2, wfal01, wfal02, wfal04),
  lava 0x3CE0D4 (lava02, lava, lavaflo, magma), wood 0x3CE07C (wood1 to
  wood9, wood11 to wood16, wood21, wood22, wood91 to wood95), doors
  0x3D4254 (wald1#0 to wald4#0, gdown111#0, dleft11#0, dleft31#0,
  dorsk11#0, down111#0, drght11#0, dup1t11#0, gdown11#0, shelfup1#0,
  stadnup1#0, stadn2p1#0, stadn3p1#0, trapdor1#0, up1pdor1#0, wald1or1#0 to
  wald4or1#0, dleft111#0, dleft311#0, door2511#0, dor1l511#0, dorsk111#0,
  drght111#0, dup1t111#0).
- Each side: side flag &= ~3; water in tmap_num or the raw tmap_num2: |= 1;
  lava, when wall_is_doorway is not 3 and not 7 (child -1 counts as 2, -2
  as 8, no wall as 5): |= 3; wood: |= 2; a door (wall type 2) with a door
  texture: wall flag |= 0x40.
- **Data**: 23 of the 29 door names are in no table ("dleft1#0" exists,
  "dleft11#0" does not). For them 0x1B2B1 gives 0, so a door with texture 0
  or tmap_num2 0 becomes a secret door in 1.3. In the port this adds one
  secret door in THIEF2 (BONES 6, DROW2 4, no others in BANE, FLAME,
  IBBALAR, YPINN, KOBOLDS).
- All 32 water, lava and wood names exist. In the port the side flags & 3
  were 0 before this change.
- **Defect** (decompilation of 0xA6F89, `tools/lvl.py`): the bits are a
  field (1 water, 2 wood, 3 lava) but the code sets them with OR, so a side
  with a water and a wood texture is lava (1 | 2). 19 sides of CRYPTS,
  HUB3, MYRKUL and PRISON have a wood texture with the waterfall `wfal04`;
  HUB3 segment 241 side 3 is the floor of the top floor next to the floor
  trigger 2 of segment 245 (message 4, "The inhabitants of this place must
  enjoy picking off unsuspecting adventurers from up here."), and the lava
  0x3D2BD burns the player there. The lava test also takes a side with no
  wall between two segments (wall_is_doorway 5), which is not drawn: 125
  such sides of 11 levels have a lava texture. DROW3 segments 566 to 570,
  574 and 593 (a layer 2.5 high, y -77.5 to -75) have the lava texture on
  their open bottom sides, and the pit below them (segments 449 to 484) has
  a water floor; 0x3D2BD takes side 3 of the segment below the player, so
  the grates of segment 575 (over 568) and of segment 122 burn the player.
  The port: `Dtu_fix_lava_sides` (dtuthief.c, `-origobj`,
  `-origlavasides`): a side with water and wood is water, and lava needs a
  drawn side (`WID_RENDER_FLAG`). It runs after 0xA6F89 and after the
  restore of a level state, whose side flags the port reads from the file.
  Test: `-level DROW3 -testpos 260,-70,22.5,0 -frames 200 -fixedfps 30
  -objlog F -noattack`: 4 burns of the player in segment 575 with
  `-origlavasides`, none without; HUB3 `-testpos -142.5,37,20,0`: 6 and 0.
