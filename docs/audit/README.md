# Audit of 2026-11-22

Five audits compared the port with DTUGAME.EXE 1.3, each in one area. Each
document has a table of findings with the evidence and the status (fixed with
its commit, or open with an estimate of size).

| Area | Document | Fixed | Most important open items |
|---|---|---|---|
| Front end and town | [frontend.md](frontend.md) | credits background, Blackstaff torches, doubled market prices, load menu layout, Waterdeep music in the main menu | Khelben's hidden face animation, credits speed, quit fade |
| In-game interface | [ingame-ui.md](ingame-ui.md) | keys under a conversation, F3 and minus/equal screen size, Alt+F2/F3, message box scroll bar, leftover Descent 1 keys, F2 options items, conversation focus keys, immersive mouse, screenshot key | the sensitivity slider has no effect, Descent 1 HUD messages, Descent 1 cheat codes |
| Combat, spells and items | [combat.md](combat.md) | invisibility, player protections against creature attacks, 18/xx strength, Flame Tongue and Mace of Disruption, arrow and bolt bonuses, hit messages, level-up box, Chill Touch on undead, sounds, creature setup by name (0xAC7B2) | thief skill points at a new level, the lich touch spell (C16), three original defects without a correction |
| Wizard spells | [spells-wizard.md](spells-wizard.md) | Strength and Infravision times, the `spelltest` test aid | most differences come from the 1.3 table fixup 0x1A344 (Fireball, Lightning Bolt, Magic Missile, Knock, area spells): the override layer, not `dtuspell.c` |
| World, scripts and levels | [world.md](world.md) | FLAME quest blocker (level fixes 0xACCDB), trigger rules after a restore, Lolth's conversation rules, YPINN soak "hangs", -testtour fall damage | the Shadow Thieves prison sequence, the WEAPON 13 warp check, script walk statement, the 1 gold fee at the well |
| Item effects | [gear.md](gear.md) | Ring of Armoring, Climb Walls of the Sword of W'hin, Potion of Luck flag 77, potion texts, Cursed Scroll, Spider Piece turn level, weapon 13 use and drop, plot item drop, saved potion timers, partial stacks | drop placement 0x3C93B, the Luck timer in a save, Find Traps marker |
| Rendering, audio and platform | [presentation.md](presentation.md) | static walls in 8-bit for one frame, the menu shadow on the GPU renderer, slow frame statistics, ambient sounds of DTU.WRP (WHAMMO), memory leaks at exit, upload time limit, item drop sound, warp sparkles | the preload batch of the first frame (64 ms), effect colors of models, end of game steps, joystick and gamepad twice, pan law of the driver |
| Item names, item data and the AD&D 2nd edition rulebooks | [items.md](items.md) | seven item names (Cloak of Lathander, Ring of Wizardry, Potion of Invisibility), the setting "Rulebook items" (off by default) | item values of the market, the speed weapons, the armor class of boots and gauntlets |
| Priest spells and turn undead | [spells-priest.md](spells-priest.md) | Protection From Fire stacking, light damage after a save, Hold Person counter, cure after Aid (`Dtu_fix_cure_aid`), turn undead count and d20 | `cast_on_top` vclips, Hold Person on the player, area of any object type for Dispel Magic |
| Conversation input, HUD areas, Blackstaff timing (decompilation notes) | [decomp-gaps.md](decomp-gaps.md) | reply paging and first selection, Enter and Up/Down with no reply, Blackstaff pixel step and fade | Khelben's hidden face animation, cursor centering in the conversation |

Test aids added by the audits: `tools/uitest.py` (interface tests, needs the
game files), `-chartest str=` and `attackedby=`, `-testobjpos`, the soak test
answers choice boxes and the "Leave the Yawning Portal?" box, `tools/check_items.py` (item effects, needs the game files), `-chartest priestcheck`
and `tools/priestcheck.py`.
