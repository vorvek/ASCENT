# Audit: item names, item data and the AD&D 2nd edition rulebooks

Scope: the item lines of the level tables of 1.3 (`$WEAPON` 89 lines, `$ARMOR`
53 lines, `$OBJECT` 128 lines): the name, the class words, the dice and the
bonuses, compared with what the code does with the item and with the AD&D
2nd edition rulebooks. The mechanics of the effects (potions, wands, rings,
weapon powers) are the subject of another audit and are only named here.

Result: seven name corrections (on by default, `-origitems` gives the names of
1.3) and the setting "Rulebook items" (off by default) with 24 values of 15
items. The two tables are in `engine/main/dtuitemfix.c`.

## Method

- The item lines were read from `YPINN.BIN` of the installed game with
  `tools/wiki_rules.py` (`Table`, `Record`). `tools/check_itemfix.py --tables`
  finds the lines of all rows of `dtuitemfix.c` in the 39 level tables of the
  mission, with the same names and values. Seven older tables (`BASE.BIN`,
  `DEMO.BIN`, `MINES1.BIN`, `MOIN1.BIN`, `PRISON1.BIN`, `SPIDER.BIN`,
  `TRAIN1.BIN`) and the 39 player archives differ; a row does not apply to a
  line with another name.
- The effects were read in the decompilation of DTUGAME.EXE 1.3
  (`local/decompiled/functions/<address>.c`) where an address is named, and
  in the port where the port is named.
- The rulebooks: Player's Handbook (TSR 2159, 1995 printing; PHB) and Dungeon
  Master Guide (TSR 2160, 1995 printing; DMG), read as page images, cited by
  the printed page. Encyclopedia Magica (EM) and Volo's Guide to All Things
  Magical, searched as text, cited by the page of the PDF file.
- 1.3 has no item description text. The description bar of the inventory
  (area 0x3BC524) shows the name. So "description" below is the name.

## How a name reaches the screen

The name of a table line (`name=`, for a weapon the last of `name=` and
`weapon_name=`) is in `item_info.name` (`bmread.c`). The rules of the game
read this name (a name with "otion", a name with '+', 0x1A344), so the
corrections do not change it. The player sees a name at five places, and
each one now asks `dtu_item_shown_name` (`dtuobj.c`):

| Place | Code |
|---|---|
| inventory entry (the description bar, the messages "while wearing %s") | `dtu_inventory_append` copies the shown name into the entry. The save game does not hold the name. |
| item in the world (the target text) | `dtu_object_display_name` |
| "You pick up %s." and "You cannot carry %s." | `dtuobj.c` |
| market | `dtufe_town.c`, `name_en` |

The translation is found by the English name (`dtu_tr_name`, key class 2 of
`dtulang.c`). The corrected names have entries `tbl.fix.*` in
`engine/lang/es_port.catalog` with gender and plural. In a language without
such an entry `dtu_item_shown_name` returns the name of the table, so the
translation of the 1.3 name is shown and no English text appears.
`tools/lang_extract.py merge` keeps the `tbl.fix.*` entries.

## The Ring of Infravision and the half-elf

The wiki said: "Infravision while worn, only for a human or half-elf." The
claim is a true rule of 1.3, not an error of the generator, but the text
hid that the ring is useless for a half-elf.

- 0xB08A8 (`000b08a8.c`): the item bit 0x40 (ring 22, weapon 54) gives the
  Infravision effect only when `Players[Player_num].race == 0`.
- The race byte of a half-elf is 0 (human) with the half-elf flag
  (`docs/re/infravision.md`, 0x5464B). So the ring acts for a human and for a
  half-elf.
- The key N (0x54632) takes the half-elf flag as race 5: a half-elf has
  infravision without the ring. PHB p. 30 agrees: "Half-elven infravision
  enables them to see up to 60 feet in darkness."

No code was changed: the effect bit does no harm. The wiki text now says that
the spell, the potion and the ring are of use to a human only.

## Findings

Layer N: a name correction (`Dtu_fix_item_names`, on by default). Layer R: a
value of "Rulebook items" (`Dtu_rulebook_items`, off by default). Numbers are
line numbers of the table section.

| Id | Item | 1.3 state | Evidence | 2e reference | Correction |
|---|---|---|---|---|---|
| N1 | `$ARMOR` 45 | Named "Cloak of Elvenkind", as line 43. Line 43 gives Hide In Shadows 95%. Line 45 lets any class turn undead and casts Continual Light. | 0xADC46 (`000adc46.c`: the worn cloak with id 0x2D). README.TXT of 1.3, change 28: "Cloak of Lathandar". | The god is Lathander: EM vol. 1 PDF p. 36 (Amulet of Lathander), EM vol. 4 PDF p. 100 (Blade of Lathander), Volo's Guide PDF p. 4 (contents: Blood of Lathander). The DMG has no such cloak. | N: "Cloak of Lathander" |
| N2 | `$ARMOR` 27 | Named "Ring of Warmth". It doubles the mage spell slots of levels 1 to 3. | 0xAFB55 case 0x1B (`000afb55.c`). README.TXT: "Ring of Wizardry". | DMG p. 184, Table 91: Warmth and Wizardry are two rings. | N: "Ring of Wizardry" |
| N3 | `$OBJECT` 83 | Named "Potion of Agility", as line 72. Line 72 sets Dexterity 18. Line 83 makes invisible. | 0x37A6A (`00037a6a.c`: item 0x53 sets the effect bit 0x20000 for 30 to 60 minutes). README.TXT: "Potion of Invisibility". | none | N: "Potion of Invisibility" |
| N4 | `$ARMOR` 16 | Named "Armoring". The line has the word `ring`; the 14 other rings are "Ring of ...". | The table line. | No such ring in DMG Table 91. | N: "Ring of Armoring" |
| N5 | `$OBJECT` 66 | Named "Black Key". Lines 67 and 68 of the same key group are "Black Gem Key". | The table lines 27 to 71: groups of three with one name. | none | N: "Black Gem Key" |
| N6 | `$WEAPON` 41 | Named "Club+1". Every other magic weapon has a space before the bonus. | The table line. | none | N: "Club +1" |
| N7 | `$OBJECT` 127 | Named "thief scroll" with small letters and two spaces at the end. | The table line. | none | N: "Thief Scroll" |
| R1 | `$WEAPON` 28 Warhammer | 3d10, more than any other weapon, for 2 gold. The Throwing Hammer (24) does 1d4+1. The older tables (`BASE.BIN`, `DEMO.BIN`, `MINES1.BIN` and others) have 1d6. | The table line: `#damage_dice=3 damage_dice=10`. | PHB p. 95, Table 44: warhammer 2 gp, 1d4+1. | R: 1d4+1 |
| R2 | `$WEAPON` 40 Warhammer +1 | 1d6+1: less than the plain Warhammer. | The table line. | PHB p. 95; DMG p. 189, Table 109. | R: 1d4+2 |
| R3 | `$WEAPON` 29 Club, 41 Club +1 | 1d4+1 and 1d4+2. | The table lines. | PHB p. 94, Table 44: club 1d6. | R: 1d6 and 1d6+1 |
| R4 | `$WEAPON` 9, 42, 73 Scimitar | A thief can use it. `MOIN1.BIN` (older) has no thief word on lines 9 and 42. | The table lines: `thief_use`. | PHB p. 54: club, dagger, dart, hand crossbow, knife, lasso, short bow, sling, broad sword, long sword, short sword, staff. | R: not for a thief |
| R5 | `$ARMOR` 2, 12, 13 Shield | A thief can use it. The manual of the game says only fighters and priests (the Armor page of `tools/wiki_rules.py`). | The table lines: `thief_use`. | PHB p. 54: leather, studded leather, padded leather or elven chain. | R: not for a thief |
| R6 | `$WEAPON` 70 Mace of Disruption | +2 to hit, 1d6+3 (the values of the Mace +2). | The table line. | EM vol. 2 PDF p. 261 (the DMG text): "This appears to be a mace +1". | R: +1 to hit, 1d6+2 (the values of the Mace +1 of 1.3) |
| R7 | `$WEAPON` 74 Arrows +1, 75 Arrows +2 | The bonus goes to the damage only (`damage_bonus`, no `to_hit_bonus`; Bolt +1 and Bolt +2 have both). Arrows +2 has no word `magic`: it does not hurt a creature that resists weapons. | The table lines. The port adds `to_hit_bonus` of the missile line to the roll (`dtu_char_get_attack`), as for the bolts. | DMG p. 189, Table 109: the bonus is the attack roll adjustment. | R: +1 and +2 to hit; Arrows +2 magic |
| R8 | `$WEAPON` 59 Wand of Lightning Bolts | Every class can use it. The Rod of Lightning (62) and the Wand of Fireballs (58) are for a mage. | The table line: `all_use`. | DMG p. 185, Table 94: Lightning (Wizard). | R: mage only |
| R9 | `$ARMOR` 27 Ring of Wizardry | Every class can wear it. It acts only on mage spell slots. | The table line: `all_use`. | DMG p. 184, Table 91: Wizardry (Wizard). | R: mage only |
| I1 | `$ARMOR` 22 Ring of Infravision | Gives its effect to a human and to a half-elf; a half-elf has infravision by race. | See the section above. | PHB p. 30. | Wiki text only |

## Compared and equal

- Weapon dice equal to PHB Table 44 (p. 94 and 95): Bastard Sword 1d8 (one
  hand), Broad Sword 2d4, Dagger 1d4, Dart 1d3, Hand Axe and Throwing Axe
  1d6, Long Sword 1d8, Mace 1d6+1 (footman's mace), Scimitar 1d8, Short Sword
  1d6, Throwing Hammer 1d4+1, Arrow 1d6 (flight arrow), Bolt 1d4 (light
  quarrel). The prices of the swords, the mace, the bows and the warhammer are
  those of the table.
- Class words equal to the rulebooks: the Broad Sword, the Long Sword, the
  Short Sword, the Club, the Dagger, the Dart and the Short Bow for a thief
  (PHB p. 54); Gauntlets of Ogre Power not for a mage (DMG p. 186, Table 98:
  Priest, Rogue, Warrior); Wand of Fear for mage and cleric, Wand of
  Fireballs and Wand of Paralyzation for a mage, Wand of Magic Missiles and
  Wand of Magic Detection for all (DMG p. 185, Table 94).

## Open

| Id | Finding | Why not changed |
|---|---|---|
| O1 | Values: Bracers of Defense AC2 500 (AC8 500, AC6 1500, AC4 2500), Chain Mail +2 2500 (as +1), Bolt +2 300 (as +1). | The DMG has experience values only (p. 186: 500 for each point of AC; p. 189, Table 106), no price. The market reads `item_info.value`, not the table line, so "Rulebook items" does not reach it. Small: a helper for the value in `dtufe_town.c` and `dtu_item_get`. |
| O2 | Short Sword of Quickness and Short Bow of Speed have no speed power. | Mechanics (the other audit). |
| O3 | Ring of Armoring (the Armor spell number in its line) does nothing. The Lantern does nothing. | Mechanics. |
| O4 | Boots +1 AC, Gauntlets +1 AC, and +1 AC of the Gloves of Dexterity, the Glim-Gauntlet and the Gauntlets of Ogre Power. The rulebooks give these no armor class. | The values are in the code tables 0x145C04 and 0x145A04, not in the item lines. A change moves every armor class of the game. |
| O5 | Names that differ from the DMG but fit their effect: Ring of Coolness (Fire Resistance), Ring of Feather Fall (Feather Falling), Gloves of Dexterity (Gauntlets of Dexterity), Wand of Fireballs (Wand of Fire). | Not defects. Each new name needs a translation. |
| O6 | The destroy rule of the Mace of Disruption, the 6 damage for each level of the wands of lightning and fire. | Mechanics. |
| O7 | With "Rulebook items" on, an item that is already worn or in the hand stays there until the player changes it. | The limits are tested when an item is equipped. |
| O8 | Not in the DMG, kept as they are: Amulet of Protection +1 to +5, Ring of Flying, Ring of Health, Ring of Strength, Ring of Quickness, Ring of Magic Detection, Ring of Infravision, Chameleon Cloak, Elven Cloak, Glim-Gauntlet, Dagger of Impaling, Bolts of Lightning, the potions of Agility, Anti-Magic, Dragon's Blood, Freedom, Luck and Toughening. | No rulebook value to compare. Not every one was searched in the Encyclopedia Magica. |

## Settings

| Setting | Default | Off or on by | Effect |
|---|---|---|---|
| `Dtu_fix_item_names` | on | `-origitems` | The seven names N1 to N7. |
| `Dtu_fix_item_stats` | on | `-origitems` | Defects of the item tables: the Warhammer (R1) does 1d6 in place of 3d10. The evidence is the Warhammer +1 (1d6+1), the price of 2 gold and the 1d6 of the older tables. "Rulebook items" changes it to 1d4+1. |
| `Dtu_rulebook_items` | off | "Rulebook items" of the Improvements menu, `RulebookItems=` of `ascent.cfg`, `-rulebookitems`, `-norulebookitems` (one run), `-origitems`, `-noimprovements` | The values R1 to R9: `bm_tbl_get` and `bm_tbl_has_flag` return the rulebook value of the item line. |

The default of "Rulebook items" is off because it changes the balance: a
cleric loses the 3d10 Warhammer, a thief loses the shield and the scimitar.

## Tests

- `ctest`: `test_dtuitemfix` (the tables and the lookups, no game data) and
  `check_itemfix` (`tools/check_itemfix.py`: the Spanish names of the
  corrected items; with `ASCENT_DATA_DIR`, each row against `YPINN.BIN`).
- Hidden runs on YPINN with `-chartest give=` and `dump`: the default shows
  the seven corrected names and the Warhammer 3d10; `-rulebookitems` gives
  1d4+1; `-origitems` gives the names of 1.3; `-language es` keeps the
  corrected names (the translations are found).
- Not tested: the pictures of the screens (inventory bar, market, pick-up
  message) in English and Spanish, the item of the Improvements menu and
  the write of `RulebookItems` to `ascent.cfg`, the class limits of R4, R5,
  R8 and R9 in play, a creature with a Warhammer.
