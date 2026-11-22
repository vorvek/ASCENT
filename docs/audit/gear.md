# Audit: the effects of the items

Scope: the use, equip and drop rules of every item of the tables of 1.3:
potions, scrolls, wands, rings, amulets, cloaks, boots, gauntlets, armor
and shields, magic weapons, charges, plot items, stacks, gold and drops.
Item names, the "Rulebook items" setting, weapon speed and the spells
belong to other audits.

Method: the decompiled functions of DTUGAME.EXE 1.3 (D:\dev\DTU-Decomp) and
the disassembly of `tools/dtudis.py`: the use 0x37A6A, the derived values
0xAFB55 with 0xB08A8 and 0xAFAD1, the potion timers 0x396B2, the drop
0xB717B with 0xB611C, the stack insert 0xB65BB, the use up 0xB7084 with
0xB6FE3, the save 0xBA87E and the load 0xBA911, the damage 0x2AF5E. The
armor class tables 0x145C04 (armor), 0x145A04 (misc), 0x1458B8 (weapons),
the dexterity tables 0x146240 and 0x14019A were dumped and compare equal to
the port. The item ids are the lines of `YPINN.BIN`; all 46 level tables of
the shipped levels have the same item lines (checked with a script).

Rules in [objects.md](../re/objects.md), "Item effects", and in
combat.md B6 and B6a. Labels in `D:\dev\DTU-Decomp\analysis\items-labels.tsv`.

Test: `python tools/check_items.py --exe build/engine/main/ascent.exe
--datadir E:/UNDAMONTAN/DTU --datadir D:/dev/ASCENT/gamedata/cd/DTU`: 17
potions, 8 wands, 8 ring effects and the rings of Health, Strength,
Protection and Armoring, the armor class of two full sets (-9 and -4), the
Sword of W'hin, the Cursed Scroll, the Gem of Curing and the drop of a plot
item. All pass. The `-chartest` commands `fire` and the `dump` line
`effects` were added for it.

## Findings

| Id | Finding | Evidence | Status |
|---|---|---|---|
| G1 | The Ring of Armoring did nothing. | 0xB0014 and 0xB01B6 (the first case of the ring jump tables 0xAFAF5 and 0xAFB25) set the byte [esp+0x2C]; 0xAFAD1 reads it after the caps of Armor (0xB05FC), Magical Vestment (0xB064F) and Shield (0xB067C): a mage (0x35A2B) gets 1 better armor class. combat.md said that nothing reads the flag. | Fixed, a95cbb3 |
| G2 | The Sword of W'hin did not set Climb Walls. | 0xB03BC and 0xB03C2 write 250 to P+0xCF and P+0xD3. | Fixed, a95cbb3 |
| G3 | The Potion of Luck did not set global flag 77, which script 508 of a Spider Piece reads. Two of its three vclips were missing. | 0x393A3 to 0x393D6 (vclips 1, 3, 38, 0xADB3B(0x4D, 1)), cleared at 0x39913 and 0xAE58E. | Fixed, a95cbb3 |
| G4 | Potion texts: the port showed the effect texts ("Infravision for 40 minutes.") and short end texts. | 0x38A6C, 0x38BE9, 0x38D75 (the "for %d minutes" texts), 0x391A7, 0x3923E, 0x39335, 0x3944C, 0x39013 (TEX 968, 969, 970, 972, 977), 0x39711 and the next (the "Potion of ... expired." texts). The Hill Giant Strength vclip of spell 50 (0x38C09). | Fixed, a95cbb3. The English strings of the port changed: the Spanish catalog needs `tools/lang_extract.py merge` |
| G5 | The Cursed Scroll was used up; 1.3 keeps it. Its damage is rand() % (hit points as a fix), less than half a point. | 0x3817F jumps to 0x391C4, past the use up 0x39694. 0x38159 divides by [ebp+0x6C] (shields, fix). | Fixed, a95cbb3: kept; damage as whole points with `Dtu_fix_cursed_scroll` (`-origcombat`) |
| G6 | The Spider Piece of turn undead (misc 18) turned at level 2. | 0x38196 to 0x381FD: class 1 becomes a cleric of level (class1_level + 1) * 2 for the call of 0xADC46. | Fixed, a95cbb3 |
| G7 | Weapon 13 (the talking sword) on USE: the port returned when one of the effects 0xC004124 was on, and set no flags. | 0x37B69 to 0x37BC7: flag 0x86 = the effect test, flag 0x91 = 1, script 0x1FD, then flag 0xA0 and alignment 0 decide the three spells. | Fixed, a95cbb3 |
| G8 | combat.md C17 (b): a creature missile gets the launcher bonus and the Potion of Luck of the player. | 0x2B0AE, 0x2B107 have no attacker test. | Setting `Dtu_fix_creature_missile` (`-origcombat`), a95cbb3. The port keeps the corrected rule by default |
| G9 | Plot items could be dropped. | 0xB611C tests `plot_item` of the table (Weapon_info +0x52, Armor_info +0x24, Misc_info +0x26, Spell_info +0x26). | Fixed, adc32e8 |
| G10 | The drop of weapon 13 has a refusal roll. | 0xB7197 to 0xB7230: flag 0x8A; d18 above intelligence or d7 above the class 1 level: flag 0x90, script 0x1FD, no drop. | Fixed, adc32e8 |
| G11 | The four potion timers were not saved, and a load kept the timers of the game before. | 0xBA87E (save: seconds left), 0xBA911 (load: now + seconds). | Fixed, adc32e8. The Luck timer P+0x1A58 is a raw time in the save of 1.3; the port does not save it (open, small) |
| G12 | The intelligence stayed at the potion value after the Poshun uv Hil Gient Inteljuns ended. | 0x396B2 sets word 0x187BB6. | Fixed, adc32e8 |
| G13 | A stack that does not fit was refused. 1.3 adds it and cuts the count to `stack_limit`; the rest is lost. | 0xB65BB, 0xB67D3 to 0xB68C1. | Fixed, adc32e8: the inventory takes what fits and the rest stays on the floor (`Dtu_fix_partial_stack`, `-origobj`: the rule of 1.3) |
| G14 | 0xB6FE3 cuts each stack to its limit before a use. It also makes a count below 1 into 1, so an endless stack goes at its first use. | 0xB6FE3, called by 0xB7084. | Fixed, adc32e8: the cut to the limit. The endless stack stays (a defect, no shipped item has one) |
| G15 | The drop places the item by a guess (ahead of the player). | 0xB717B calls 0x3C93B: it sets the contains fields of the player object and calls object_create_egg 0x3C4BD, so the item starts at the player. | Open, small. The pick up delay of 5 s (0xB72BA) is in the port |
| G16 | Find Traps marker object of a found trap. | 0xB3605. | Open, cosmetic (thief area) |

Checked and found equal: the armor class rules of all places (boot, belt,
necklace, shield, gauntlet, armor, cloak, rings, lockpicks, hand), the
cloak and ring exceptions, the effect bits of the worn items and their end,
Girdle, Gauntlets of Ogre Power, Ring of Strength, Ring of Health, Gloves of
Dexterity, the class limits (0x3533C), the wand charges, the bows, gold.
The game has no identification, no cursed items other than the Cursed
Scroll, and no weight or encumbrance rule: no string of the executable
names them, and `weight` is a table keyword for the mass of the physics.

## Each item id

### misc (type 16)

| Id | Item | Original | Port | Status |
|---|---|---|---|---|
| 0 | Lantern | none | - | same: no use, light only |
| 1 | gold | 0x2805B | pick_up_item | same: "You pick up %d gold.", the amount is the hit points |
| 2 | Gem of Curing | 0x3891B | use_misc | same: one use per mission, cures blindness or Cure Serious Wounds, not used up |
| 3 | Diamond | none | - | same: no use (market value) |
| 4 | Ruby | none | - | same: no use (market value) |
| 5 | Spider Amulet | none | - | same: necklace, plot item, no effect in 0xAFB55; drop refused, fixed in adc32e8 |
| 6 | Cloak | 0xAFB55 | dtu_char_update | same: worn, armor class 0 (0x145A04) |
| 7 | Belt | 0xAFB55 | dtu_char_update | same: worn, armor class 0 (0x145A04) |
| 8 | Boots | 0xAFB55 | dtu_char_update | same: table -1, but the boot place reads only armor (0xAFBC8): no armor class |
| 9 | Gauntlets | 0xAFB55, 0xB0B5F | dtu_char_update | same: -1 armor class, Find Traps and Open Locks -250 |
| 10 | Lockpicks | 0xB02BE | dtu_char_update | same: Open Locks +5, +15, +20, +10 for a thief |
| 11 | Journeyman Lockpicks | 0xB02BE | dtu_char_update | same: Open Locks +5, +15, +20, +10 for a thief |
| 12 | Master Lockpicks | 0xB02BE | dtu_char_update | same: Open Locks +5, +15, +20, +10 for a thief |
| 13 | Apprentice Lockpicks | 0xB02BE | dtu_char_update | same: Open Locks +5, +15, +20, +10 for a thief |
| 14 | Ankh | none | - | same: no use (market value) |
| 15 | Mykrul's Wand | none | - | same: plot item, no use; drop refused (0xB611C), fixed in adc32e8 |
| 16 | Scarab Beetle | none | - | same: plot item, no use; drop refused (0xB611C), fixed in adc32e8 |
| 17 | Crystal Rose | none | - | same: plot item, no use; drop refused (0xB611C), fixed in adc32e8 |
| 18 | Spider Piece | 0x38196 | use_misc | fixed in a95cbb3: turn undead at (level + 1) * 2 + 1, was level 2 |
| 19 | Spider Piece | 0x37A6A | use_misc | same: the whole map |
| 20 | Spider Piece | 0x37A6A | use_misc | same: dtu_spider_tour_start (dtucombat.c) |
| 21 | Spider Piece | none | - | same: no use case |
| 22 | Spider Piece | 0x37A6A | use_misc | same: area Fear and seven spells |
| 23 | Spider Piece | 0x37A6A | use_misc | same: dtu_spider_tour_start (dtucombat.c) |
| 24 | Spider Piece | 0x37A6A | use_misc | same: script 0x1FC; flag 77 of the Potion of Luck now set, fixed in a95cbb3 |
| 25 | Assembled Spider | 0x37A6A | use_misc | same: four effects with no end |
| 26 | Spider Piece | 0x37A6A | use_misc | same: all hit points and nine spells |
| 27 | Gold Key | none | - | same: key item (key list, doors: world audit) |
| 28 | Gold Key | none | - | same: key item (key list, doors: world audit) |
| 29 | Gold Key | none | - | same: key item (key list, doors: world audit) |
| 30 | Magic Gold Key | none | - | same: key item (key list, doors: world audit) |
| 31 | Magic Gold Key | none | - | same: key item (key list, doors: world audit) |
| 32 | Magic Gold Key | none | - | same: key item (key list, doors: world audit) |
| 33 | Cross Key | none | - | same: key item (key list, doors: world audit) |
| 34 | Cross Key | none | - | same: key item (key list, doors: world audit) |
| 35 | Cross Key | none | - | same: key item (key list, doors: world audit) |
| 36 | Skull Key | none | - | same: key item (key list, doors: world audit) |
| 37 | Skull Key | none | - | same: key item (key list, doors: world audit) |
| 38 | Skull Key | none | - | same: key item (key list, doors: world audit) |
| 39 | Fire Key | none | - | same: key item (key list, doors: world audit) |
| 40 | Fire Key | none | - | same: key item (key list, doors: world audit) |
| 41 | Fire Key | none | - | same: key item (key list, doors: world audit) |
| 42 | Dragon Key | none | - | same: key item (key list, doors: world audit) |
| 43 | Dragon Key | none | - | same: key item (key list, doors: world audit) |
| 44 | Dragon Key | none | - | same: key item (key list, doors: world audit) |
| 45 | Golden Key | none | - | same: key item (key list, doors: world audit) |
| 46 | Golden Key | none | - | same: key item (key list, doors: world audit) |
| 47 | Golden Key | none | - | same: key item (key list, doors: world audit) |
| 48 | Silver Key | none | - | same: key item (key list, doors: world audit) |
| 49 | Silver Key | none | - | same: key item (key list, doors: world audit) |
| 50 | Silver Key | none | - | same: key item (key list, doors: world audit) |
| 51 | Bronze Key | none | - | same: key item (key list, doors: world audit) |
| 52 | Bronze Key | none | - | same: key item (key list, doors: world audit) |
| 53 | Bronze Key | none | - | same: key item (key list, doors: world audit) |
| 54 | Stone Key | none | - | same: key item (key list, doors: world audit) |
| 55 | Stone Key | none | - | same: key item (key list, doors: world audit) |
| 56 | Stone Key | none | - | same: key item (key list, doors: world audit) |
| 57 | Red Gem Key | none | - | same: key item (key list, doors: world audit) |
| 58 | Red Gem Key | none | - | same: key item (key list, doors: world audit) |
| 59 | Red Gem Key | none | - | same: key item (key list, doors: world audit) |
| 60 | Purple Gem Key | none | - | same: key item (key list, doors: world audit) |
| 61 | Purple Gem Key | none | - | same: key item (key list, doors: world audit) |
| 62 | Purple Gem Key | none | - | same: key item (key list, doors: world audit) |
| 63 | Green Gem Key | none | - | same: key item (key list, doors: world audit) |
| 64 | Green Gem Key | none | - | same: key item (key list, doors: world audit) |
| 65 | Green Gem Key | none | - | same: key item (key list, doors: world audit) |
| 66 | Black Key | none | - | same: key item (key list, doors: world audit) |
| 67 | Black Gem Key | none | - | same: key item (key list, doors: world audit) |
| 68 | Black Gem Key | none | - | same: key item (key list, doors: world audit) |
| 69 | Violet Gem Key | none | - | same: key item (key list, doors: world audit) |
| 70 | Violet Gem Key | none | - | same: key item (key list, doors: world audit) |
| 71 | Violet Gem Key | none | - | same: key item (key list, doors: world audit) |
| 72 | Potion of Agility | 0x37A6A | use_misc | fixed in a95cbb3: text "You have increased agility for 10 minutes.", end text "Potion of Agility expired." |
| 73 | Potion of Anti-Magic | 0x37A6A | use_misc | same: all effects and potion timers end (0x9002C, 0x396B2) |
| 74 | Potion of Dragon's Blood | 0x37A6A | use_misc | fixed in a95cbb3: text "You have increased strength for %d minutes.", end text |
| 75 | Potion of Extra-Healing | 0x37A6A | use_misc | same: 6 to 27 hit points |
| 76 | Potion of Fire Resistance | 0x37A6A | use_misc | fixed in a95cbb3: text "You have Fire Resistance for 10 minutes." in place of the effect text |
| 77 | Potion of Flying | 0x37A6A | use_misc | same: spell 19 (Fly) |
| 78 | Potion of Freedom | 0x37A6A | use_misc | fixed in a95cbb3: no effect text, only "You are immune to paralysis for awhile." |
| 79 | Potion of Healing | 0x37A6A | use_misc | same: 4 to 10 hit points |
| 80 | Potion of Hill Giant Strength | 0x37A6A | use_misc | fixed in a95cbb3: vclip of spell 50, end text "Potion of Hill Giant Strength expired."; saved timer adc32e8 |
| 81 | Poshun uv Hil Gient Inteljuns | 0x37A6A | use_misc | fixed in a95cbb3 (TEX 977, end text) and adc32e8: the intelligence returns at the end (0x396B2), saved timer |
| 82 | Potion of Infravision | 0x37A6A | use_misc | fixed in a95cbb3: TEX 968 "You have infravision for some time." |
| 83 | Potion of Agility | 0x37A6A | use_misc | fixed in a95cbb3: TEX 969 "You are invisible for now." |
| 84 | Potion of Invulnerability | 0x37A6A | use_misc | fixed in a95cbb3: TEX 970 "You presently feel invincible." |
| 85 | Potion of Jumping | 0x37A6A | use_misc | same: spell 27 (Jump) |
| 86 | Potion of Luck | 0x37A6A | use_misc | fixed in a95cbb3: global flag 77 set at use and cleared at the end (0x393D6, 0x39913, 0xAE58E), vclips 1, 3, 38 |
| 87 | Potion of Speed | 0x37A6A | use_misc | fixed in a95cbb3: TEX 972 "You feel faster." |
| 88 | Potion of Toughening | 0x37A6A | use_misc | same: system shock table 0x2FACC, Bless with extra hit points |
| 89 | Short Bow | 0x2AB36, 0x2B0EE | launcher_bonus | same: to_hit_bonus to the roll and the damage of arrows or bolts |
| 90 | Long Bow | 0x2AB36, 0x2B0EE | launcher_bonus | same: to_hit_bonus to the roll and the damage of arrows or bolts |
| 91 | Crossbow | 0x2AB36, 0x2B0EE | launcher_bonus | same: to_hit_bonus to the roll and the damage of arrows or bolts |
| 92 | Short Bow +1 | 0x2AB36, 0x2B0EE | launcher_bonus | same: to_hit_bonus to the roll and the damage of arrows or bolts |
| 93 | Short Bow +2 | 0x2AB36, 0x2B0EE | launcher_bonus | same: to_hit_bonus to the roll and the damage of arrows or bolts |
| 94 | Long Bow +1 | 0x2AB36, 0x2B0EE | launcher_bonus | same: to_hit_bonus to the roll and the damage of arrows or bolts |
| 95 | Long Bow +2 | 0x2AB36, 0x2B0EE | launcher_bonus | same: to_hit_bonus to the roll and the damage of arrows or bolts |
| 96 | Long Bow +3 | 0x2AB36, 0x2B0EE | launcher_bonus | same: to_hit_bonus to the roll and the damage of arrows or bolts |
| 97 | Crossbow +1 | 0x2AB36, 0x2B0EE | launcher_bonus | same: to_hit_bonus to the roll and the damage of arrows or bolts |
| 98 | Crossbow +2 | 0x2AB36, 0x2B0EE | launcher_bonus | same: to_hit_bonus to the roll and the damage of arrows or bolts |
| 99 | Crossbow +3 | 0x2AB36, 0x2B0EE | launcher_bonus | same: to_hit_bonus to the roll and the damage of arrows or bolts |
| 100 | Short Bow of Speed | 0x2AB36 | launcher_bonus | same: +2; 0xAFB55 does not read the bow place, pc_spell1 is not read. Speed items: other agent |
| 101 | Orb of Insight | 0x37A6A | use_misc | same: script event 0x1FE |
| 102 | Scroll | 0x9DC42 | use_misc, dtu_scroll_show | same: scroll text of the count, not used up |
| 103 | Banegem | none | - | same: plot item, no use; drop refused (0xB611C), fixed in adc32e8 |
| 104 | Ruldegost Document | none | - | same: plot item, no use; drop refused (0xB611C), fixed in adc32e8 |
| 105 | Kossuth Ruby | none | - | same: plot item, no use; drop refused (0xB611C), fixed in adc32e8 |
| 106 | Cursed Scroll | 0x37A6A (0x3817F) | use_misc | fixed in a95cbb3: not used up; damage below half a point in 1.3, `Dtu_fix_cursed_scroll` |
| 107 | Violet Gem Key | none | - | same: key item (key list, doors: world audit) |
| 108 | Violet Gem Key | none | - | same: key item (key list, doors: world audit) |
| 109 | Violet Gem Key | none | - | same: key item (key list, doors: world audit) |
| 110 | Violet Gem Key | none | - | same: key item (key list, doors: world audit) |
| 111 | Violet Gem Key | none | - | same: key item (key list, doors: world audit) |
| 112 | Violet Gem Key | none | - | same: key item (key list, doors: world audit) |
| 113 | Violet Gem Key | none | - | same: key item (key list, doors: world audit) |
| 114 | Violet Gem Key | none | - | same: key item (key list, doors: world audit) |
| 115 | Violet Gem Key | none | - | same: key item (key list, doors: world audit) |
| 116 | Violet Gem Key | none | - | same: key item (key list, doors: world audit) |
| 117 | Violet Gem Key | none | - | same: key item (key list, doors: world audit) |
| 118 | Violet Gem Key | none | - | same: key item (key list, doors: world audit) |
| 119 | Violet Gem Key | none | - | same: key item (key list, doors: world audit) |
| 120 | Violet Gem Key | none | - | same: key item (key list, doors: world audit) |
| 121 | Violet Gem Key | none | - | same: key item (key list, doors: world audit) |
| 122 | Violet Gem Key | none | - | same: key item (key list, doors: world audit) |
| 123 | Violet Gem Key | none | - | same: key item (key list, doors: world audit) |
| 124 | Violet Gem Key | none | - | same: key item (key list, doors: world audit) |
| 125 | Flamesword | none | - | same: plot item, no use; drop refused (0xB611C), fixed in adc32e8 |
| 126 | Scroll of the Illithids | none | - | same: plot item, no use; drop refused (0xB611C), fixed in adc32e8 |
| 127 | thief scroll | 0x9DC42 | use_misc | same: scroll text 1 |

### armor (type 15)

| Id | Item | Original | Port | Status |
|---|---|---|---|---|
| 0 | Ring of Protection +2 | 0xAF9AB | dtu_char_update | same: the better ring counts, none with magic armor (checked) |
| 1 | Leather Armor | 0xAFB55 | dtu_char_update | same: table 0x145C04; magic 5 to 11, heavy 3, 4, 7 to 11 (checked) |
| 2 | Shield | 0xAFB55 | dtu_char_update | same: table 0x145C04; magic 5 to 11, heavy 3, 4, 7 to 11 (checked) |
| 3 | Chain Mail | 0xAFB55 | dtu_char_update | same: table 0x145C04; magic 5 to 11, heavy 3, 4, 7 to 11 (checked) |
| 4 | Plate Mail | 0xAFB55 | dtu_char_update | same: table 0x145C04; magic 5 to 11, heavy 3, 4, 7 to 11 (checked) |
| 5 | Leather Armor +1 | 0xAFB55 | dtu_char_update | same: table 0x145C04; magic 5 to 11, heavy 3, 4, 7 to 11 (checked) |
| 6 | Leather Armor +2 | 0xAFB55 | dtu_char_update | same: table 0x145C04; magic 5 to 11, heavy 3, 4, 7 to 11 (checked) |
| 7 | Chain Mail +1 | 0xAFB55 | dtu_char_update | same: table 0x145C04; magic 5 to 11, heavy 3, 4, 7 to 11 (checked) |
| 8 | Chain Mail +2 | 0xAFB55 | dtu_char_update | same: table 0x145C04; magic 5 to 11, heavy 3, 4, 7 to 11 (checked) |
| 9 | Plate Mail +1 | 0xAFB55 | dtu_char_update | same: table 0x145C04; magic 5 to 11, heavy 3, 4, 7 to 11 (checked) |
| 10 | Plate Mail +2 | 0xAFB55 | dtu_char_update | same: table 0x145C04; magic 5 to 11, heavy 3, 4, 7 to 11 (checked) |
| 11 | Plate Mail +3 | 0xAFB55 | dtu_char_update | same: table 0x145C04; magic 5 to 11, heavy 3, 4, 7 to 11 (checked) |
| 12 | Shield +1 | 0xAFB55 | dtu_char_update | same: table 0x145C04; magic 5 to 11, heavy 3, 4, 7 to 11 (checked) |
| 13 | Shield +2 | 0xAFB55 | dtu_char_update | same: table 0x145C04; magic 5 to 11, heavy 3, 4, 7 to 11 (checked) |
| 14 | Ring of Protection +1 | 0xAF9AB | dtu_char_update | same: the better ring counts, none with magic armor (checked) |
| 15 | Ring of Protection +3 | 0xAF9AB | dtu_char_update | same: the better ring counts, none with magic armor (checked) |
| 16 | Armoring | 0xB0014, 0xAFAD1 | armoring_ac | fixed in a95cbb3: a mage gets 1 better after each cap of Armor, Vestment, Shield (checked) |
| 17 | Ring of Coolness | 0xAFB55, 0xB08A8 | worn_item_bits, worn_effects | same: effect with no end while worn, ends when taken off (checked) |
| 18 | Ring of Feather Fall | 0xAFB55, 0xB08A8 | worn_item_bits, worn_effects | same: effect with no end while worn, ends when taken off (checked) |
| 19 | Ring of Flying | 0xAFB55, 0xB08A8 | worn_item_bits, worn_effects | same: effect with no end while worn, ends when taken off (checked) |
| 20 | Ring of Free Action | 0xAFB55, 0xB08A8 | worn_item_bits, worn_effects | same: effect with no end while worn, ends when taken off (checked) |
| 21 | Ring of Health | 0xAFB55 | dtu_char_update | same: constitution 18 (checked) |
| 22 | Ring of Infravision | 0xAFB55, 0xB08A8 | worn_item_bits, worn_effects | same: effect with no end while worn, ends when taken off (checked) |
| 23 | Ring of Jumping | 0xAFB55, 0xB08A8 | worn_item_bits, worn_effects | same: effect with no end while worn, ends when taken off (checked) |
| 24 | Ring of Magic Detection | 0xAFB55, 0xB08A8 | worn_item_bits, worn_effects | same: effect with no end while worn, ends when taken off (checked) |
| 25 | Ring of Quickness | 0xAFB55, 0xB08A8 | worn_item_bits, worn_effects | same: effect with no end while worn, ends when taken off (checked) |
| 26 | Ring of Strength | 0xAFB55 | dtu_char_update | same: strength 18/00 (checked) |
| 27 | Ring of Warmth | 0xB00A5, 0xB075F | worn_effects | same with `Dtu_fix_wizardry_ring` (slot doubling) |
| 28 | Amulet of Protection +1 | 0xAFB55 | dtu_char_update | same: table value (checked with +5) |
| 29 | Amulet of Protection +2 | 0xAFB55 | dtu_char_update | same: table value (checked with +5) |
| 30 | Amulet of Protection +3 | 0xAFB55 | dtu_char_update | same: table value (checked with +5) |
| 31 | Amulet of Protection +4 | 0xAFB55 | dtu_char_update | same: table value (checked with +5) |
| 32 | Amulet of Protection +5 | 0xAFB55 | dtu_char_update | same: table value (checked with +5) |
| 33 | Amulet of Shield | 0xAFB55, 0xB08A8 | worn_item_bits, worn_effects | same: Shield effect, no armor class |
| 34 | Winged Boots | 0xAFB55, 0xB08A8 | worn_item_bits, worn_effects | same: Fly, Feather Fall, Levitate, Haste |
| 35 | Boots of Feather Falling | 0xAFB55, 0xB08A8 | worn_item_bits, worn_effects | same: Fly, Feather Fall, Levitate, Haste |
| 36 | Boots of Levitation | 0xAFB55, 0xB08A8 | worn_item_bits, worn_effects | same: Fly, Feather Fall, Levitate, Haste |
| 37 | Boots of Speed | 0xAFB55, 0xB08A8 | worn_item_bits, worn_effects | same: Fly, Feather Fall, Levitate, Haste |
| 38 | Girdle of Hill Giant Strength | 0xAFB55 | dtu_char_update | same: strength 19 |
| 39 | Bracers of Defense AC8 | 0xAFB55 | dtu_char_update | same: magic armor for the rings and cloaks |
| 40 | Bracers of Defense AC6 | 0xAFB55 | dtu_char_update | same: magic armor for the rings and cloaks |
| 41 | Bracers of Defense AC4 | 0xAFB55 | dtu_char_update | same: magic armor for the rings and cloaks |
| 42 | Bracers of Defense AC2 | 0xAFB55 | dtu_char_update | same: magic armor for the rings and cloaks |
| 43 | Cloak of Elvenkind | 0xAFB55 | dtu_char_update | same: Hide In Shadows 95 |
| 44 | Chameleon Cloak | 0xAFB55, 0xB08A8 | worn_item_bits, worn_effects | same: Improved Invisibility |
| 45 | Cloak of Elvenkind | 0x37A6A, 0xADCB3 | dtu_char_use, dtu_combat_turn_undead | same: USE casts Continual Light; key T turns as a cleric of level 7 |
| 46 | Elven Cloak | 0xAFB55, 0xB08A8 | worn_item_bits, worn_effects | same: Invisibility |
| 47 | Cloak of Protection +1 | 0xAF930 | dtu_char_update | same: only with no shield, magic or heavy armor (checked) |
| 48 | Cloak of Protection +2 | 0xAF930 | dtu_char_update | same: only with no shield, magic or heavy armor (checked) |
| 49 | Cloak of Protection +3 | 0xAF930 | dtu_char_update | same: only with no shield, magic or heavy armor (checked) |
| 50 | Gloves of Dexterity | 0xAFB55 | dtu_char_update | same: dexterity table 0x146240, Open Locks |
| 51 | Glim-Gauntlet | 0xAFB55, 0xB08A8 | worn_item_bits, worn_effects | same: Detect Magic |
| 52 | Gauntlets of Ogre Power | 0xAFB55 | dtu_char_update | same: strength 18/00 |

### weapon (type 5)

| Id | Item | Original | Port | Status |
|---|---|---|---|---|
| 0 | Empty hand | 0x2A7F3, 0x2AF5E | weapon_attack, dtu_char_weapon_damage | same: the dice of the character |
| 1 | Flare | none | - | not an item of the player: spell or creature missile template; creature missiles: `Dtu_fix_creature_missile`, fixed in a95cbb3 |
| 2 | Spell Cast | none | - | not an item of the player: spell or creature missile template; creature missiles: `Dtu_fix_creature_missile`, fixed in a95cbb3 |
| 3 | Magic Wand | none | - | not an item of the player: spell or creature missile template; creature missiles: `Dtu_fix_creature_missile`, fixed in a95cbb3 |
| 4 | Magic Scroll | none | - | not an item of the player: spell or creature missile template; creature missiles: `Dtu_fix_creature_missile`, fixed in a95cbb3 |
| 5 | Magic Bow | none | - | not an item of the player: spell or creature missile template; creature missiles: `Dtu_fix_creature_missile`, fixed in a95cbb3 |
| 6 | Long Sword | 0x2A7F3, 0x2AF5E | weapon_attack, dtu_char_weapon_damage | same: dice and bonus of the table (0x2B077, 0x2AAE7, 0x2B1E7) |
| 7 | Arrow | 0x2A7F3, 0x2AF5E | weapon_attack, dtu_char_weapon_damage | same: launcher bonus by the flag arrow (audit C8) |
| 8 | Flame Arrow | none | - | not an item of the player: spell or creature missile template; creature missiles: `Dtu_fix_creature_missile`, fixed in a95cbb3 |
| 9 | Scimitar | 0x2A7F3, 0x2AF5E | weapon_attack, dtu_char_weapon_damage | same: dice and bonus of the table (0x2B077, 0x2AAE7, 0x2B1E7) |
| 10 | DO NOT USE | 0x2A7F3, 0x2AF5E | weapon_attack, dtu_char_weapon_damage | same: launcher bonus by the flag arrow (audit C8) |
| 11 | Throwing Axe | 0x2A7F3, 0x2AF5E | weapon_attack, dtu_char_weapon_damage | same; the backstab of a thrown weapon is audit C18 (open) |
| 12 | Magic Arrow | 0x2A7F3, 0x2AF5E | weapon_attack, dtu_char_weapon_damage | same: launcher bonus by the flag arrow (audit C8) |
| 13 | Wand of Lightning Bolts | 0x37B58, 0xB7197, 0x59D2D | dtu_char_use, dtu_char_drop | fixed in a95cbb3 (use: flags 0x86, 0x91, 0xA0) and adc32e8 (drop roll); the warp check is world W7 (open) |
| 14 | Black Lightning | none | - | not an item of the player: spell or creature missile template; creature missiles: `Dtu_fix_creature_missile`, fixed in a95cbb3 |
| 15 | Bomb | none | - | not an item of the player: spell or creature missile template; creature missiles: `Dtu_fix_creature_missile`, fixed in a95cbb3 |
| 16 | Lava Flames | none | - | not an item of the player: spell or creature missile template; creature missiles: `Dtu_fix_creature_missile`, fixed in a95cbb3 |
| 17 | Mini Flames | none | - | not an item of the player: spell or creature missile template; creature missiles: `Dtu_fix_creature_missile`, fixed in a95cbb3 |
| 18 | Dagger | 0x2A7F3, 0x2AF5E | weapon_attack, dtu_char_weapon_damage | same: dice and bonus of the table (0x2B077, 0x2AAE7, 0x2B1E7) |
| 19 | Short Sword | 0x2A7F3, 0x2AF5E | weapon_attack, dtu_char_weapon_damage | same: dice and bonus of the table (0x2B077, 0x2AAE7, 0x2B1E7) |
| 20 | Broad Sword | 0x2A7F3, 0x2AF5E | weapon_attack, dtu_char_weapon_damage | same: dice and bonus of the table (0x2B077, 0x2AAE7, 0x2B1E7) |
| 21 | Dart | 0x2A7F3, 0x2AF5E | weapon_attack, dtu_char_weapon_damage | same; the backstab of a thrown weapon is audit C18 (open) |
| 22 | Throwing Dagger | 0x2A7F3, 0x2AF5E | weapon_attack, dtu_char_weapon_damage | same; the backstab of a thrown weapon is audit C18 (open) |
| 23 | Bastard Sword | 0x2A7F3, 0x2AF5E | weapon_attack, dtu_char_weapon_damage | same: dice and bonus of the table (0x2B077, 0x2AAE7, 0x2B1E7) |
| 24 | Throwing Hammer | 0x2A7F3, 0x2AF5E | weapon_attack, dtu_char_weapon_damage | same; the backstab of a thrown weapon is audit C18 (open) |
| 25 | SLow Bomb | none | - | not an item of the player: spell or creature missile template; creature missiles: `Dtu_fix_creature_missile`, fixed in a95cbb3 |
| 26 | fast Bomb | none | - | not an item of the player: spell or creature missile template; creature missiles: `Dtu_fix_creature_missile`, fixed in a95cbb3 |
| 27 | Mace | 0x2A7F3, 0x2AF5E | weapon_attack, dtu_char_weapon_damage | same: dice and bonus of the table (0x2B077, 0x2AAE7, 0x2B1E7) |
| 28 | Warhammer | 0x2A7F3, 0x2AF5E | weapon_attack, dtu_char_weapon_damage | same: dice and bonus of the table (0x2B077, 0x2AAE7, 0x2B1E7) |
| 29 | Club | 0x2A7F3, 0x2AF5E | weapon_attack, dtu_char_weapon_damage | same: dice and bonus of the table (0x2B077, 0x2AAE7, 0x2B1E7) |
| 30 | Hand Axe | 0x2A7F3, 0x2AF5E | weapon_attack, dtu_char_weapon_damage | same: dice and bonus of the table (0x2B077, 0x2AAE7, 0x2B1E7) |
| 31 | Bolt | 0x2A7F3, 0x2AF5E | weapon_attack, dtu_char_weapon_damage | same: launcher bonus by the flag arrow (audit C8) |
| 32 | Long Sword +1 | 0x2A7F3, 0x2AF5E | weapon_attack, dtu_char_weapon_damage | same: dice and bonus of the table (0x2B077, 0x2AAE7, 0x2B1E7) |
| 33 | Long Sword +2 | 0x2A7F3, 0x2AF5E | weapon_attack, dtu_char_weapon_damage | same: dice and bonus of the table (0x2B077, 0x2AAE7, 0x2B1E7) |
| 34 | Short Sword +1 | 0x2A7F3, 0x2AF5E | weapon_attack, dtu_char_weapon_damage | same: dice and bonus of the table (0x2B077, 0x2AAE7, 0x2B1E7) |
| 35 | Short Sword +2 | 0x2A7F3, 0x2AF5E | weapon_attack, dtu_char_weapon_damage | same: dice and bonus of the table (0x2B077, 0x2AAE7, 0x2B1E7) |
| 36 | Broad Sword +1 | 0x2A7F3, 0x2AF5E | weapon_attack, dtu_char_weapon_damage | same: dice and bonus of the table (0x2B077, 0x2AAE7, 0x2B1E7) |
| 37 | Bastard Sword +1 | 0x2A7F3, 0x2AF5E | weapon_attack, dtu_char_weapon_damage | same: dice and bonus of the table (0x2B077, 0x2AAE7, 0x2B1E7) |
| 38 | Mace +1 | 0x2A7F3, 0x2AF5E | weapon_attack, dtu_char_weapon_damage | same: dice and bonus of the table (0x2B077, 0x2AAE7, 0x2B1E7) |
| 39 | Mace +2 | 0x2A7F3, 0x2AF5E | weapon_attack, dtu_char_weapon_damage | same: dice and bonus of the table (0x2B077, 0x2AAE7, 0x2B1E7) |
| 40 | Warhammer +1 | 0x2A7F3, 0x2AF5E | weapon_attack, dtu_char_weapon_damage | same: dice and bonus of the table (0x2B077, 0x2AAE7, 0x2B1E7) |
| 41 | Club+1 | 0x2A7F3, 0x2AF5E | weapon_attack, dtu_char_weapon_damage | same: dice and bonus of the table (0x2B077, 0x2AAE7, 0x2B1E7) |
| 42 | Scimitar +1 | 0x2A7F3, 0x2AF5E | weapon_attack, dtu_char_weapon_damage | same: dice and bonus of the table (0x2B077, 0x2AAE7, 0x2B1E7) |
| 43 | Dagger +1 | 0x2A7F3, 0x2AF5E | weapon_attack, dtu_char_weapon_damage | same: dice and bonus of the table (0x2B077, 0x2AAE7, 0x2B1E7) |
| 44 | Dagger +2 | 0x2A7F3, 0x2AF5E | weapon_attack, dtu_char_weapon_damage | same: dice and bonus of the table (0x2B077, 0x2AAE7, 0x2B1E7) |
| 45 | Dagger of Impaling | 0x2B15B | dtu_char_weapon_damage | same: backstab +3 to 6 on a roll of 19 or 20 |
| 46 | Hand Axe +1 | 0x2A7F3, 0x2AF5E | weapon_attack, dtu_char_weapon_damage | same: dice and bonus of the table (0x2B077, 0x2AAE7, 0x2B1E7) |
| 47 | Bolt +1 | 0x2A7F3, 0x2AF5E | weapon_attack, dtu_char_weapon_damage | same: launcher bonus by the flag arrow (audit C8) |
| 48 | Bolt +2 | 0x2A7F3, 0x2AF5E | weapon_attack, dtu_char_weapon_damage | same: launcher bonus by the flag arrow (audit C8) |
| 49 | Bolts of Lightning | 0x2A7F3, 0x2AF5E | weapon_attack, dtu_char_weapon_damage | same: bolt, 5d6 |
| 50 | Dart +1 | 0x2A7F3, 0x2AF5E | weapon_attack, dtu_char_weapon_damage | same; the backstab of a thrown weapon is audit C18 (open) |
| 51 | Dart +2 | 0x2A7F3, 0x2AF5E | weapon_attack, dtu_char_weapon_damage | same; the backstab of a thrown weapon is audit C18 (open) |
| 52 | Throwing Axe +1 | 0x2A7F3, 0x2AF5E | weapon_attack, dtu_char_weapon_damage | same; the backstab of a thrown weapon is audit C18 (open) |
| 53 | Throwing Hammer +1 | 0x2A7F3, 0x2AF5E | weapon_attack, dtu_char_weapon_damage | same; the backstab of a thrown weapon is audit C18 (open) |
| 54 | Sword of W'hin | 0xB038F | dtu_char_update, worn_item_bits | fixed in a95cbb3: Climb Walls 250 was missing; AC -9 and eight effects (checked) |
| 55 | Wand of Magic Missiles | 0x56A0D | dtu_spell_fire, hand_spell | same: casts pc_spell, one charge (checked) |
| 56 | Wand of Confusion | 0x56A0D | dtu_spell_fire, hand_spell | same: casts pc_spell, one charge (checked) |
| 57 | Wand of Fear | 0x56A0D | dtu_spell_fire, hand_spell | same: casts pc_spell, one charge (checked) |
| 58 | Wand of Fireballs | 0x56A0D | dtu_spell_fire, hand_spell | same: casts pc_spell, one charge (checked) |
| 59 | Wand of Lightning Bolts | 0x56A0D | dtu_spell_fire, hand_spell | same: casts pc_spell, one charge (checked) |
| 60 | Wand of Magic Detection | 0x56A0D | dtu_spell_fire, hand_spell | same: casts pc_spell, one charge (checked) |
| 61 | Wand of Paralyzation | 0x56A0D | dtu_spell_fire, hand_spell | same: casts pc_spell, one charge (checked) |
| 62 | Rod of Lightning | 0x56A0D | dtu_spell_fire, hand_spell | same: casts pc_spell, one charge (checked) |
| 63 | Wand of Genericness | none | - | same: no spell (enemy_spell=-1); not in the levels |
| 64 | Wand of Genericness | none | - | same: no spell (enemy_spell=-1); not in the levels |
| 65 | Wand of Genericness | none | - | same: no spell (enemy_spell=-1); not in the levels |
| 66 | Rod of Genericness | none | - | same: no spell (enemy_spell=-1); not in the levels |
| 67 | Rod of Genericness | none | - | same: no spell (enemy_spell=-1); not in the levels |
| 68 | Rod of Genericness | none | - | same: no spell (enemy_spell=-1); not in the levels |
| 69 | Short Sword of Quickness | 0x56A0D | hand_spell | pc_spell 21 (Haste): speed items, other agent |
| 70 | Mace of Disruption | 0x2B1F2 | dtu_weapon_extra_damage | same: undead destroyed or double damage (audit C5) |
| 71 | Flame Tongue | 0x2AB9F, 0x2B1F2 | dtu_weapon_extra_damage | same: +2/+4 and fire (audit C5) |
| 72 | Dwarven Sword | 0x2A7F3, 0x2AF5E | weapon_attack, dtu_char_weapon_damage | same: dice and bonus of the table (0x2B077, 0x2AAE7, 0x2B1E7) |
| 73 | Scimitar | 0x2A7F3, 0x2AF5E | weapon_attack, dtu_char_weapon_damage | same: dice and bonus of the table (0x2B077, 0x2AAE7, 0x2B1E7) |
| 74 | Arrows +1 | 0x2A7F3, 0x2AF5E | weapon_attack, dtu_char_weapon_damage | same: launcher bonus by the flag arrow (audit C8) |
| 75 | Arrows +2 | 0x2A7F3, 0x2AF5E | weapon_attack, dtu_char_weapon_damage | same: launcher bonus by the flag arrow (audit C8) |
| 76 | Temp | none | - | not an item of the player: spell or creature missile template; creature missiles: `Dtu_fix_creature_missile`, fixed in a95cbb3 |
| 77 | Wand | none | - | not an item of the player: spell or creature missile template; creature missiles: `Dtu_fix_creature_missile`, fixed in a95cbb3 |
| 78 | Acid Spittle | none | - | not an item of the player: spell or creature missile template; creature missiles: `Dtu_fix_creature_missile`, fixed in a95cbb3 |
| 79 | Ice Shards | none | - | not an item of the player: spell or creature missile template; creature missiles: `Dtu_fix_creature_missile`, fixed in a95cbb3 |
| 80 | Myconid Spores | none | - | not an item of the player: spell or creature missile template; creature missiles: `Dtu_fix_creature_missile`, fixed in a95cbb3 |
| 81 | Beholder Burn | none | - | not an item of the player: spell or creature missile template; creature missiles: `Dtu_fix_creature_missile`, fixed in a95cbb3 |
| 82 | Spectator Burn | none | - | not an item of the player: spell or creature missile template; creature missiles: `Dtu_fix_creature_missile`, fixed in a95cbb3 |
| 83 | Spectator Burn | none | - | not an item of the player: spell or creature missile template; creature missiles: `Dtu_fix_creature_missile`, fixed in a95cbb3 |
| 84 | Spectator Burn | none | - | not an item of the player: spell or creature missile template; creature missiles: `Dtu_fix_creature_missile`, fixed in a95cbb3 |
| 85 | Spectator Burn | none | - | not an item of the player: spell or creature missile template; creature missiles: `Dtu_fix_creature_missile`, fixed in a95cbb3 |
| 86 | Spectator Burn | none | - | not an item of the player: spell or creature missile template; creature missiles: `Dtu_fix_creature_missile`, fixed in a95cbb3 |
| 87 | Spectator Burn | none | - | not an item of the player: spell or creature missile template; creature missiles: `Dtu_fix_creature_missile`, fixed in a95cbb3 |
| 88 | Spectator Burn | none | - | not an item of the player: spell or creature missile template; creature missiles: `Dtu_fix_creature_missile`, fixed in a95cbb3 |
