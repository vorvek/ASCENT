# Audit of 2026-11-22

Five audits compared the port with DTUGAME.EXE 1.3, each in one area. Each
document has a table of findings with the evidence and the status (fixed with
its commit, or open with an estimate of size).

| Area | Document | Fixed | Most important open items |
|---|---|---|---|
| Front end and town | [frontend.md](frontend.md) | credits background, Blackstaff torches, doubled market prices, load menu layout, Waterdeep music in the main menu, quit fade, credits key | none: the hidden face animation is not reachable in 1.3, the credits speed is a decision of the port |
| In-game interface | [ingame-ui.md](ingame-ui.md) | keys under a conversation, F3 and minus/equal screen size, Alt+F2/F3, message box scroll bar, leftover Descent 1 keys, F2 options items, conversation focus keys, immersive mouse, screenshot key, sensitivity slider, Descent 1 HUD messages, cheat codes of 1.3 | message colors (U18), the V key gate (U16) |
| Combat, spells and items | [combat.md](combat.md) | invisibility, player protections against creature attacks, 18/xx strength, Flame Tongue and Mace of Disruption, arrow and bolt bonuses, hit messages, level-up box, Chill Touch on undead, sounds, creature setup by name (0xAC7B2) | thief skill points at a new level, the lich touch spell (C16), three original defects without a correction |
| World, scripts and levels | [world.md](world.md) | FLAME quest blocker (level fixes 0xACCDB), trigger rules after a restore, Lolth's conversation rules, YPINN soak "hangs", -testtour fall damage | the Shadow Thieves prison sequence, the WEAPON 13 warp check, script walk statement, the 1 gold fee at the well |
| Rendering, audio and platform | [presentation.md](presentation.md) | static walls in 8-bit for one frame, the menu shadow on the GPU renderer, slow frame statistics | ambient sounds of DTU.WRP (WHAMMO), memory leaks at exit, spreading the level start texture loads |
| Conversation input, HUD areas, Blackstaff timing (decompilation notes) | [decomp-gaps.md](decomp-gaps.md) | reply paging and first selection, Enter and Up/Down with no reply, Blackstaff pixel step and fade, cursor centering in the conversation | the two fixed statistics area tests (H11) |
| Each screen, second pass | [screens.md](screens.md) | main menu after Abort Game, death picture keys and fade, load error text, difficulty 2, statistics spell tables, inventory usability marks, messages in the description bar | the world runs during the 3 s of the death, message word wrap, ending light and sounds |

Test aids added by the audits: `tools/uitest.py` (interface tests, needs the
game files), `-chartest str=` and `attackedby=`, `-testobjpos`, the soak test
answers choice boxes and the "Leave the Yawning Portal?" box.
