# Audit: combat, spells, items and character rules

Scope: attack rolls, damage, critical hits and fumbles, weapons and
launchers, creature attacks on the player, saving throws, effects, potions
and other items, experience and levels, thief skills, rest and death
(`dtucombat.c`, `dtuspell.c`, `dtuchar.c`, `dtuobj.c`, `dtuthief.c`).

Method: the functions of DTUGAME.EXE 1.3 were read again in
`gamedata/re/dtu_flat.bin` with `tools/dtudis.py` and compared with the port:
the to-hit roll 0x2A7F3, the damage roll 0x2AF5E, the damage function
0x29A9C (creature and player branches), the strength bonus 0x2A789, the
visibility test 0x165C4 and 0x16636, the level functions 0xAF3CF to 0xAF8A0,
the rest 0x360D5, 0x3E137 and 0x3E1F6, the backstab test 0xB337E, the
creature setup 0xAC7B2. Texts were checked against `DTUE.TEX` (line n is
TEX n; `Text_string` is at 0x395DAC). The spell audit of `docs/re/spells.md`
was not repeated.

Tests: hidden and muted runs with `-fixedfps 30` on BANE (the skeleton obj
10 at `-testpos 25,-1,233,0`), the new `-chartest` commands
`str=STRENGTH.PERCENT` and `attackedby=OBJNUM`, the `-charlog` and `-objlog`
lines, and `-soak 3` on BANE, BONES and DROW2 (2500 frames each, 0
problems). `ctest`: 16 of 16 pass.

## Findings

Most important first. Status: the commit of the fix on the branch
`worktree-agent-a02b5e0b596a899b4`, or open with an estimate.

| Id | Finding | Evidence | Status |
|---|---|---|---|
| C1 | An invisible player was seen and attacked by every creature. Invisibility, Improved Invisibility, the Potion of Invisibility and the worn items with these bits did nothing against creatures. | 0x165C4 tests the effect bits 16 and 17 (`test byte [player+0x8A], 3`) and the hide flag, and a creature with the effect Blindness (`+0x88` bit 4) sees the player. The wake test of `do_ai_frame` (0x13D1A) calls 0x165C4 first. The port tested only Hide In Shadows and only in the sight test. Test: BANE, `1:fx=17.600`: before, 27 "sees the player" and 32 attack animations in 600 frames; after, 0 and 0 (without the effect: 11 and 41). | Fixed, 67485f6 |
| C2 | Creature attacks on the player ignored the protections of the player: Stone Skin, Invulnerability and Protection From Evil had no effect against melee and missiles. The effects of the attacker (Bless, Slow, Strength, Levitate, Chill Touch) and the -4 for a hidden player were missing; a creature fumble did nothing; a sleeping player was not hit at once. | 0x2A24A (player branch of 0x29A9C): Invulnerability (`+0x8A` bit 4) keeps off a creature of level below 4 (0xAE3BB) or a weapon without `magic`, text "You are protected by Invulnerability." (TEX 832, 833, 30); Stone Skin (`+0x8B` bit 0x20) takes the attack, TEX 832 to 834; 0x2A49B Chill Touch 1d4. 0x2A7F3: 0x2A839 sleep, 0x2A9EB creature fumble (d20 above str: flinch), 0x2AA5B Levitate, 0x2ACBA Strength, 0x2ACE8 hidden -4, 0x2AD27 Bless, 0x2AD36 Protection From Evil (extraplanar: roll 0; else -2), 0x2AE22 Slow, 0x2AE44 Stone Skin. Test: 200 attacks of the skeleton with `attackedby=10`: none 102 hits and 58 misses; Stone Skin (`cast=49`) 4 attacks taken by the skin; Invulnerability 200 kept off; Protection From Evil 101 hits, 99 misses. | Fixed, b778be8 |
| C3 | The strength bonus of a fighter with 18/xx used the wrong column: 18/00 gave +0 to hit and +1 damage in place of +2 and +4, and every other percent band was one column too high. The Ring with strength 18/00 (armor 26) and the Potion of Dragon's Blood were affected. | 0x2A7B9 and 0x2B4BB: column 4 for percent 0 (18/00), 3 for 91 to 99, 2 for 76 to 90, 1 for 51 to 75, 0 for 1 to 50; tables 0x13A9F2 (0,1,1,1,2) and 0x13AA06 (1,1,2,3,4). Test: broad sword, `str=18.0`: +3 to hit, 2d4+6 (was +1, 2d4+3); `str=18.30`: +1, 2d4+3. | Fixed, a4861c9 |
| C4 | The creature setup by name 0xAC7B2 (called from 0xAD716) and 0xACCDB (0xAD7A1) is not in the port. The lich types get no undead, weapon_resistant, no_sleep, magic_resistant, electricity_immune and humanoid flags, no 10d1 damage and no touch spell; mephits, flayers, gargoyles and the hatchling keep the weapon and dice of the table (the code gives weapon type 0x21 and 4 dice); the hatchling is not fire immune; the spectator keeps its attack type; the names `sfmtd2` to `shdleadg` and the boss values of level 0 (200 hit points, str 19, score 10000) and the single creatures of 0xACCDB are missing. | ai.md, "The game changes entries after load"; 0xAC80E to 0xACCD3 (strncmp of the robot name, stores to the Robot_info entry). No reference to 0xAC7B2 or 0xACCDB in the port. | Fixed, 5f68994: `creature_setup_by_name` and `level_start_npcs` (dtuobj.c) in the order of level_start 0xAD5BF, checked against the C reference of the decompilation for the 965 placements of the 31 levels (`tools/check_creature_setup.py`, 22 weapon fallbacks with the level fixes). The 1.3 rule that `add_npc` makes the Shadow Thieves hostile again with quest flag 67 is corrected (`Dtu_fix_guild_peace`, bugs.md D16). See ai.md, "Creature setup by name" |
| C5 | Flame Tongue, Mace of Disruption and the fire dice of weapons had no effect for the player. A weapon with fire dice in the hand of the player did no fire damage, and fire damage did not respect fire_immune, fire_resistant, Resist Fire and Protection From Fire. | 0x2AB9F and 0x2B1F2: weapon 71 +2 to hit and fire against a regenerating target, +4 against undead; weapon 70 against undead: double damage when d100 <= 5 * (level - 4), else the creature is destroyed (hit points as damage, effects cleared, thrown along the up and forward vectors with strength * 4.0); 0x2B327 fire dice with the resistances (only for a weapon with fire dice), 0x2B5D4 fire ends regeneration. Test: weapon 70 on the skeleton: "the Mace of Disruption destroys obj 10"; weapon 71: d20 5 gives roll 11 (+4 undead), fire 4. | Fixed, b778be8. The table fixup (0x1B04A) later gave weapon 71 the dice 0d0, and the port sent a weapon with 0 sides to the empty hand: the Flame Tongue lost its +1, its fire and its bonus. Fixed again with the description window review: `weapon_attack` has no test of the dice (as 0x2AAE7), and `dtu_item_get` has the defaults of 0x207E7 (1d6, fire_wait 0.25), so the arrows 10, 12, 74 and 75 (no `damage_dice` key) roll 1d6. |
| C6 | A thief gets no skill points at a new level. The screen of `thiefpts.c` (0xB4419 to 0xB5B49, main 0xB5BB9) where the points are spent is not in the port, and the number of points for each level was not read. | combat.md B7 and B9. Test: `tools/uitest.py`, `test_thief_points`. | Fixed (`dtuthiefpts.c`; the points and the level paid are P+0x1AB3 and P+0x1A53 of the saved record) |
| C7 | Critical hits and fumbles of the player: no sound 3001; a fumble that keeps the weapon showed no text and the attack went on. | 0x2A8CA and 0x2A907 play 0xBB9 at 1.0. 0x2A949: `rand() % 3` selects "You lost your balance!", "Your weapon was entangled!" or "The wind was knocked out of you!", the arms go to state 10 (`+0x176`, `+0x170`), the attack request 0x13B69F and 0x13B6A3 is cleared. Test: BANE fight, frame 91: "fumble, d20 12 not above dexterity 14: Your weapon was entangled!". | Fixed, b778be8 |
| C8 | Missiles of the player (arrows, bolts, thrown weapons in flight) did the bare dice of the weapon table: no launcher bonus to damage, no strength, no Potion of Luck. The bow or crossbow was chosen by the flag `bolt`; the executable takes the bow for the flag `arrow` and the crossbow for any other missile. The Potion of Luck never gave the highest damage roll. | 0x2AB0A and 0x2B0C1 (`test [weapon+0x3E], 4`, the flag `arrow`); 0x2B107 the Potion of Luck sets the damage to number * sides; the player damage of 0x2AF5E is used for every hit of the player. | Fixed, b778be8 |
| C9 | The Strength spell gave the player +1 to hit on top of its higher strength. The executable gives the +1 only to a creature (and +1 damage to a creature, 0x2B053, which the port did not have). | 0x2ACBA is in the creature branch of 0x2A7F3 (0x2AC4D to 0x2ACCC). | Fixed, b778be8 |
| C10 | The armor class of a creature with several effects: the port took the lowest of the caps of Vestment (always 5), Shield and Armor. | 0x2ADAF: Vestment caps at max(5 - (L - 5) / 3, 1) with the creature level, else Shield 3, else Armor 6 (one of them); then Slow +4, at most 10. | Fixed, b778be8 |
| C11 | Hit texts. The port showed "You hit Skeleton for 3." and "Skeleton hits you for 3."; a Stone Skin of a creature showed nothing. | 0x2A05F and 0x2A54E: TEX 831 "%s hit %s for %d damage." with TEX 70 "You"; 0x29E02 and 0x2AF1D: "%s %s" with TEX 830 "protected by StoneSkin Spell.". Only the English source strings in `dtuobj.c` changed (note for the localization work). | Fixed, bbd873c (texts), b778be8 (Stone Skin text) |
| C12 | A new level showed a HUD line "Your character has attained new Fighter level". | 0xAF560 to 0xAF5AE: for each class bit (cleric, mage, fighter, thief order) a message box 0x655FE with the button TEX 34 "Ok" and TEX 844 "Your character has attained\nnew %s levels!" with TEX 642, 643, 644, 647. Test: `-char SG0.SG`, `xp=3000`: two boxes, Mage then Thief. Only the English defaults in `dtuchar.c` changed. | Fixed, 2bd1b92 |
| C13 | Chill Touch of the player on an undead creature cast the spell Fear (a guess): a second save, the duration of the spell and the weapon drop. | 0x29E8F to 0x29F20: the Fear bit with the timer `+0xAC` = 1d4 + caster level (0x91EC1 of spell 5) minutes, then the morale function 0x10ADF. Test: two failed saves on the skeleton: Fear for 180 and 240 s, morale rolls. | Fixed, 5b71802 |
| C14 | No hurt sound of the player type when the player took damage. | 0x2B50C to 0x2B588: `hard_sound` (0x1D4E27) or `hit_sound` (0x1D4E2B) of the `$PLAYER` line, volume 0x5555, rate 0.25 s (0x25C50). The key order was read in the parser 0x1F952 to 0x1FB2D. | Fixed, e2a9251 |
| C15 | Rest had no sound. | 0x3E309: sound 62 at volume 0x8000. Test: `gorest`, `rest`: hit points 7 to 12, the fire removed, a second rest refused. | Fixed, 2785691 |
| C16 | The lich touch spell: after a hit on the player, a creature with `Robot_info+0x7C` >= 0 casts that spell on the player (0x2A637 to 0x2A67B). Only C4 sets the field (spell 22, Hold Person, for the lich). | 0x2A64A `cmp word [Robot_info+0x7C], 0`, 0x2A67B call 0x8CFFF. | Fixed, 9f709bf: `damage_player_w` casts the spell after each attack that did damage. Test: `attackedby=10` in CLAN3, "Hold Person cast on You by Lich for 4 minutes." (monsters.md) |
| C17 | Defects of the executable that the port does not copy and that have no setting yet: (a) the strength bonus of a creature is 0x2A789(str, 100), so a creature with strength 18 gets +1 more when the player is a fighter (0x2AC9B, 0x2A7B0); (b) the damage of a creature missile adds the launcher bonus of the player and can use up the Potion of Luck of the player (0x2B0B7, 0x2B107 have no test of the attacker); (c) Invulnerability calls 0xAE3BB with no attacker (0x2A27B reads `[0 + 5]`). | Code addresses in the column to the left. | (b) setting `Dtu_fix_creature_missile` (gear audit, docs/audit/gear.md). (a) is not an item rule. (c) not ported: it reads address 5 of DOS memory, a value the port cannot know |
| C18 | A thrown melee weapon (attack type 1) in flight can backstab in the executable (0xB337E is called by 0x29A9C for every hit of the player); the port sets the backstab flag only for the swing. Chill Touch of a creature on another creature (0x29E3D) is not in the port. | 0x29D5B, 0xB33C7, 0x29E3D. | Open, small |
| C19 | The rest calls the alert function 0x160AB(player, 5) and 0x4D95D(player) (not read). | 0x3E2C6, 0x3E31D. | Alert fixed, df15f2e and 35f9714 (`dtu_creature_alert`, monsters.md); 0x4D95D open |
| C20 | A hidden or invisible player in the AI: the executable keeps an active creature after the player with a believed position that moves at random every 2.0 s (`Ai_cloak_info`, `compute_vis_and_vec` 0x12BE8); the port gives a hidden player no visibility at all. | ai.md, `compute_vis_and_vec`. | Open, medium (AI) |
| C21 | Not in the port, known before this audit: the scroll text screen 0x9DC42 (now in `dtuscroll.c`), the Spider Pieces 0x14, 0x17 and 0x18, the decode of the Gem of Curing counter, the marker object of Find Traps, the racial search for secret walls (0xB3ECE). | combat.md C2. | Open, medium each. The racial search is now in the port, with the secret doors of 0xA6F89 (character.md) |
| C22 | Weapon 13 has +2 to hit and double damage against creatures with `Robot_info+0xD9` bit 8 (0x2ABE1, 0x2B301). Weapon 13 is the Wand of Lightning Bolts in the tables of 1.3, which casts and never reaches these lines. | 0x2ABE1, 0x2B301; `-chartest give=5.13` in BANE, HUB1, DROW2. | Not ported (dead code with the shipped data) |

## Checked and found equal

- Experience: the split for two classes, the level tables 0x146048, the
  limit of level 18, the hit dice by level (0x146528, 0x146538: a die up to
  level value 9 for cleric and fighter, 10 for mage and thief, then 2, 1, 3,
  2), the constitution tables, the half for two classes, THAC0 0x146590.
- The attack roll of the empty hand and of a weapon, the critical hit as a
  second damage roll, the backstab multiplier `(level + 8) >> 2` and the 3 to
  6 of weapon 45, the weapon_resistant test, the saving throw tables
  0xAE596.
- The potions 0x48 to 0x58 (spot check of the dice and the durations).
- The backstab test 0xB337E.

## Changes to the test options

- `-chartest str=STRENGTH.PERCENT`: sets the base strength and prints the
  attack values.
- `-chartest attackedby=OBJNUM`: a melee attack of a creature on the player
  through the full roll.
- A choice box in a run with `-soak` or `-frames` and no `-autochoice`
  takes the first choice (the level box would stop an unattended run).
