# Audit: each screen of 1.3 against the port (2026-11-22)

This is a second pass over the screens of DTUGAME.EXE 1.3 after the audits in
`frontend.md`, `ingame-ui.md` and `decomp-gaps.md`. The items that those
files checked are not repeated. Evidence: the decompilation (`D:\dev\DTU-Decomp`,
C per function), `gamedata/re/all.asm` and the strings of `dtu_flat.bin`.
TEX n is line n + 1 of `DTUE.TEX` (TEX = 0x395DAC + 4n in the image).

Status: **same**, **fixed** (this pass, with the commit), **open**, **port**
(a decision of the port), **no 1.3** (the element does not exist in 1.3).

UI tests: `tools/uitest.py` (needs the game files). New in this pass:
`test_fe_credits_key_only`, `test_fe_death_picture_key`,
`test_inv_message_in_desc_bar`, `test_stats_spell_table`. `test_inv_sbar` needs
a save with more than ten items (SG0.SG of the install has six).

## Front end

| Screen | Element | Original | Status |
|---|---|---|---|
| Main menu | Save Game and Return to Game: shown while 0x141BBB (a character) or while the full list of Waterdeep is in use (0x8C4B6). Abort Game, the end of the game (0x42649, 0x4264F) and the death (0x42423) clear both; the inn exit sets the full list (0x42642). The port used the Khelben mission as the test, so both items stayed after Abort Game. | 0x57F28, 0x8C426, 0x8C431 | fixed: `Fe_full_list` |
| Main menu | Quit: the palette fades out in 32 steps (0x7F54E(0x20)) | 0x581E9 | fixed (FE-12) |
| Main menu | Items, conditions, last choice, Quit question | 0x57F28, 0x58075 | same (frontend.md) |
| Credits | Only a key ends the credits | 0x2C68B, 0xDA3E4 | fixed (FE-9) |
| Credits | Speed: one pixel when 0x100 has passed, no wait: at most 256 pixels a second, slower on a slow machine; a hold of 10 s (0xA0000) at the end | 0x2C776, 0x2C7E8 | port: 60 pixels a second (FE-8) |
| Character, screen 1 | Page shields: the back shield on the first page says TEX 715 "No more pages", the forward shield on the last page TEX 714 "End Of book" (the port had them swapped) | 0x88423 | fixed |
| Character, screen 1 | Figure help when not chosen: "Click here to select a ..." and a second line TEX 610 | 0x88423, format 0x13F7DA | fixed (the second line). The word order "Human Male" of 1.3 is "Male Human" in the port: the port keeps one translatable format |
| Character, screens 1 and 2 | "?" help: "Quickly generate a" (TEX 632), a line break, race, sex and classes, "character" (TEX 633) only with no class | 0x88423, 0x881ED, 0x8865B | port: one translatable line |
| Character, screen 3 | TEX 719 subject: the class words are joined with a space. The code means "/" but never sets its flag ([ebp-4]) | 0x8B68A | port: "/" (a quirk of 1.3 with no effect on play) |
| Character | An aborted New Game also resets Khelben (0x82BC4) and the first list (0x8C426) | 0x87E60, 0x87E65 | open: the list is reset (fixed with the main menu item); the Khelben reset of a loaded game on abort is a defect of 1.3, not ported |
| Character | Abort question, refusals TEX 712, 713, 716, 717, class help 613 to 619, 634, 635, name box, alignment rule | 0x87EFC, 0x881ED, 0x8C0BB | same |
| Load | A file with a valid header whose character cannot be read: TEX 804 "Error reading character data", and 0xBB763 (no character, Khelben reset, first list). Other failures TEX 802 | 0xBBB44 to 0xBBB53 | fixed |
| Save | A failed open shows no text; a failed write TEX 800 | 0xBA99F | corrected: `Dtu_fix_save_open_text` (TEX 800 for both; verified in the bytes: 0xBAA3E goes to the exit with no text, 0xBABF3 shows the text of a failed write); `-origfe` gives no text |
| Load and save | Spacer, width 190, TEX 67, 796, 797, 798, 801 | 0xBA647, 0xBA486 | same |
| Options | Hires Dungeon Graphics and "Hear Khelben In English"; two blank lines after Reverse Stereo | 0x58C18 | port (FE-10, FE-11) |
| Death | The world stops at once when the hit points go below 1 (0x13AD0D, 0x13D3F7); a wait of 3 s or a key; fade out (32 steps) | 0x33C5E, 0x423F5 to 0x42460 | fixed: the world stops at the death, the streams stop, a key ends the wait (dtuobj.c dtu_player_death_wait, verified in the bytes: 0x33C96, 0x4240B to 0x42452); `test_death_stops_world` |
| Death picture | FEBGND 161, music 7 after the picture, the keys that are down are taken, then any key; the mouse does nothing | 0x42478 to 0x4252E | fixed (key wait) |
| Death picture | Fade out in 32 steps before the Death Menu | 0x4253E, 0x4254F | fixed |
| Death Menu | Load Game, Abort Game, "Death Menu\n" | 0x42593 | same |
| Ending | The light and 50 explosion sounds before CREDITV1 (0xA6786) | 0xA6A2F | fixed (docs/re/movies.md section 5) |
| Ending | After the movie the main menu (Function_mode 6, 0xB89C5); no character | 0x42649 | fixed with the main menu item |

## Waterdeep and the town

1.3 has no temple, no inn rooms and no rumour screen: the feature lists
0x141D48 and 0x141F10 have Blackstaff, the Yawning Portal, the market and
the City Gates only. The Yawning Portal is the level YPINN.

| Screen | Element | Original | Status |
|---|---|---|---|
| Yawning Portal | The difficulty: the difficulty menu of 1.3 has no items and sets Player_default_difficulty and Difficulty_level to 2. The port stayed at 1 (creature morale, missile speed, notice range, path lengths, the warp gate). | 0x5851A, called at 0x58A18 | fixed |
| Save and load | The difficulty is offset 7362 of the save (0x13CDCA); the port now writes Difficulty_level and reads it back | 0xBA99F, 0xBB780 | fixed |
| Waterdeep | Hot spots, keys, help TEX 566 to 569, buttons, the two lists | 0x141D48, 0x141F10, 0x8C22A | same |
| Waterdeep | "`" screenshot feature, F1 and "\" feature with an empty handler | 0x80D93, 0x82B2E | open (debug only) |
| Blackstaff | Mission texts, blurb order, the mission switch, timing | 0x142840, 0x83DC8, 0x83EF5, 0x830D4 | same |
| Blackstaff | Khelben's hidden face animation (FESEQ 29, 30) | 0x83A93 | no 1.3: it runs only when the phase pointer 0x13E922 is 0, and the screen entry 0x82B31 always sets it to 0x8313C before the runner; all six writes are not 0. Not reachable, not ported (FE-6, T8, T9) |
| Well | Passage gold | 0x59F0B | same (world audit W8) |

## The market

The market is the only shop of 1.3. It has no identify service, and its
catalog shows no class limits: the row draw 0x85E06 prints the icon, the
quantity, the name and the price, with no class test. The class mask of an
item is shown in the inventory (see below).

| Element | Original | Status |
|---|---|---|
| Price: value, uses, stackable, charisma tables, global flag 107 doubles | 0x86C5A, 0x86F5A, 0x13F2DD, 0x13F32D | same (frontend.md FE-3) |
| Buy: room (stack limit or 127 entries), TEX 700, 702, 704, most = min(quantity, gold / price, room) | 0x84D41, 0xB7303 | same |
| Sell: sell price, TEX 705, TEX 703 for an equipped line | 0x84CA6, 0x8622B | same |
| Accept: buy takes gold per item, sell gives gold, then the stock gets the quantity of the line (Dtu_fix_market_sell_count) | 0x85458 | same |
| Stock: geldor.dat, first stock 0x13EB6C, TheStore.n, Khelben rewards | 0x872C1, 0x86846, 0x87C05, 0x879A7 | same |
| Catalog: 9 rows, circular lists, scroll buttons, "many" | 0x85AF0, 0x84B6D | same |
| Quantity pop-up: FEBGND 25, keys, Enter Qty | 0x84EEA | same |

## In the game

| Screen | Element | Original | Status |
|---|---|---|---|
| Statistics | The spell tables: "Mage Spells", "Cleric Spells" (TEX 765, 766), "Level n" (TEX 767) for levels 1 to 4, and the slots left: "n/a" (24,24,24) without the class, "none" (32,32,32) with none left, else the number; labels (24,24,24) without the class or with no slot of the level. Rectangles from the tables 0x144CA8 to 0x144D40. The port's own spell list in that place is removed. | 0x331AC to 0x334A6, 0x9A312 | fixed |
| Statistics | Thief skills: color (63,48,32); (24,24,24) for a character that is no thief when the value is 0 | 0x32172, 0x9A0B0 | fixed |
| Statistics | Class lines "Lvl %d %s": (63,48,32) | 0x99F25 | fixed |
| Statistics | Abilities, alignment, race, portrait | 0x32BE0 to 0x330C3 | same (colors of the alignment and race lines not checked) |
| Inventory | Icon 199 over an entry that the character cannot use (0x3533C: class bits, count, spells 39, 40, 53, the Scroll) | 0x32899, 0x9C6C4 | fixed |
| Inventory | Gray icon for an entry in use: armor and wearable misc items in a slot (0x3223E), weapons, misc items and spells in a hot key (0x323B0) | 0x328AB, 0x323F3, 0x9C537 | fixed |
| Inventory | Icon 142 for a spell in the item list; icon 141 for the Scroll with a count of 0 | 0x32824 to 0x32858 | fixed |
| Inventory | Icon 143 for the Scroll when byte +10 of its record is not 0 | 0x3285F | fixed: byte +10 is the plot_item of the table line (0xB65BB); the port reads it from the table (dtudisp.c scroll_plot_item). Verified in the bytes: count 0 gives 141 (0x32858), then plot_item not 0 gives 143 (0x32865) |
| Inventory | In modes 3 and 6 a new message also goes to the description bar (100 characters) | 0x34A83 to 0x34AD2 | fixed |
| Inventory | Paper doll, USE, DROP, scroll bar | 0x35ADF, 0x3010C | same (ingame-ui.md U21) |
| Spell lists | "Lvl %d" / "L-%d", the marks as in the item list | 0xB7487, 0x32899 | same, fixed (marks) |
| Message box | Word wrap: words split on space, tab, CR, LF (strtok); two spaces after ".", "!", "?"; a line ends at 100 characters or the box width | 0x347A7, 0x347DA | open: the port breaks lines at "\n" and uses one space |
| Message box | Color of a line (0x34998, the color byte) | 0x34998 | open (ingame-ui.md U18) |
| Level-up box | Flag 0x146294 (always 1), class texts 642, 643, 644, 647, no hit point roll shown | 0xAF489, 0xAF694 | same |
| Information column | Name, hit points "%d / %d", AC, "%d Exp", gold | 0x32657 to 0x326C2 | same |
| Map screen | Keys, colors, texts | 0x1887E | same (docs/re/automap.md for the known gaps) |
| Conversation | The cursor goes to the middle of the window at the first poll of a choice | 0xB2433 | fixed (decomp-gaps D14). The poller 0xB249C keeps the cursor in the window; the port does not hold the desktop cursor |
| Options (F2) | Joystick/Mouse Sensitivity scales the look and turn axes and the look keys by value / 8 | 0x52BCE to 0x53A8C | fixed (ingame-ui.md U13) |

## Not covered

The click rules of character screen 3 (0x8A578) and every feature rectangle
of the four character screens; the colors of the alignment and race lines of
the statistics screen. The fixes were tested with `tools/uitest.py` and
screenshots in high resolution only.
