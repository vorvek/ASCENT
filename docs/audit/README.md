# Audit of 2026-11-22

Five audits compared the port with DTUGAME.EXE 1.3, each in one area. Each
document has a table of findings with the evidence and the status (fixed with
its commit, or open with an estimate of size).

| Area | Document | Fixed | Most important open items |
|---|---|---|---|
| Front end and town | [frontend.md](frontend.md) | credits background, Blackstaff torches, doubled market prices, load menu layout, Waterdeep music in the main menu | Khelben's hidden face animation, credits speed, quit fade |
| In-game interface | [ingame-ui.md](ingame-ui.md) | keys under a conversation, F3 and minus/equal screen size, Alt+F2/F3, message box scroll bar, leftover Descent 1 keys, F2 options items, conversation focus keys, immersive mouse, screenshot key | the sensitivity slider has no effect, Descent 1 HUD messages, Descent 1 cheat codes |
| Combat, spells and items | [combat.md](combat.md) | invisibility, player protections against creature attacks, 18/xx strength, Flame Tongue and Mace of Disruption, arrow and bolt bonuses, hit messages, level-up box, Chill Touch on undead, sounds, creature setup by name (0xAC7B2) | the lich touch spell (C16), three original defects without a correction |
| Character rules | [character.md](character.md) | secret doors and the water, lava and wood side bits (0xA6F89), thief search state in the level records, the trap and trigger tests of the search, set level (0x8BA3A), level hit points, `-chartest rules` | the marker object of Find Traps, the alert call of the rest |
| World, scripts and levels | [world.md](world.md) | FLAME quest blocker (level fixes 0xACCDB), trigger rules after a restore, Lolth's conversation rules, YPINN soak "hangs", -testtour fall damage | the Shadow Thieves prison sequence, the WEAPON 13 warp check, script walk statement, the 1 gold fee at the well |
| Rendering, audio and platform | [presentation.md](presentation.md) | static walls in 8-bit for one frame, the menu shadow on the GPU renderer, slow frame statistics | ambient sounds of DTU.WRP (WHAMMO), memory leaks at exit, spreading the level start texture loads |
| Conversation input, HUD areas, Blackstaff timing (decompilation notes) | [decomp-gaps.md](decomp-gaps.md) | reply paging and first selection, Enter and Up/Down with no reply, Blackstaff pixel step and fade | Khelben's hidden face animation, cursor centering in the conversation |

Test aids added by the audits: `tools/uitest.py` (interface tests, needs the
game files), `-chartest str=` and `attackedby=`, `-testobjpos`, the soak test
answers choice boxes and the "Leave the Yawning Portal?" box.
