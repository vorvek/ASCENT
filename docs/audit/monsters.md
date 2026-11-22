# Audit: monsters

Date: 2026-11-22. Scope: the creature types of the 31 levels of DTU.MSN, their
values, attacks, special abilities, AI, death and corpse. Player spells,
items, the attack interval of the player and the character rules are other
audits.

Method: each level ran in the port with `-setuplog -objlog` (20 frames). The
values below are the values of each type after the creature setup by name
(0xAC7B2) and the level fixes (0xACCDB). `tools/check_creature_setup.py`
compares these writes with the C reference of the decompilation for all 965
placements, so the values are the values of 1.3. Armor class and THAC0 come
from the NPC overlay (0xA504A, 191 rows, applied by 0xA7E97); "table" means
that the type has no row and keeps the monster table value. Hit points are
the `max_hit_points` of the type (the object can have fewer: the level file
gives each object its own shields). XP is `score_value`, given at the death
(0x4EA50) unless the regeneration bit is set (the test before the call in 0x29A9C).

## Rules of 1.3 that apply to all creatures

- Melee: the weapon test of the model (0x6CFF8), the to-hit roll 0x2A7F3
  against the armor class of the player, the damage dice of the weapon
  (`Weapon_info`) or, with a negative weapon type, the dice of the type
  (+0x6C, +0x6E, 0x2AF5E). Attack interval: the animation of the ATTACK
  states (frames from frame 8 to the end of the state), not a timer.
- Ranged (attack type 0, no spell level): `ai_fire` 0x11B06 makes a
  missile of `weapon_type`; a negative weapon type is a spell missile
  (0x10040). A type with a mage or cleric level casts (0x92D99, 0x9231F,
  0x91FDC) and touches a target in reach (0x27D82).
- No melee status effects: no poison, paralysis, level drain, disease,
  petrification, breath or gaze in the code (ai.md, "Creature melee status
  effects"). The only on-hit effect is the touch spell of +0x7C (the lich,
  spell 22 Hold Person) and the `enemy_spell` of a weapon, which is -1 in all
  shipped tables.
- Immunities: `weapon_resistant` (only a `magic` weapon hits, 0x29D9D),
  `magic_resistant` (the saving throw need goes down by min(level, 12) / 3,
  0xAE77F), `fire_immune` and `fire_resistant` (0x91DAF, 0x2B3B3),
  `electricity_immune` (0x91D90), `undead` (Turn Undead, Mace of Disruption,
  Flame Tongue +4, Chill Touch Fear, the morale function), `no_sleep` and
  `no_scare` (0x92F5D), `extraplanar` (Protection From Evil, no corpse).
- Regeneration: one hit point each `regeneration` seconds while the status
  bit 0x01000000 is set (0x1440C); fire or acid damage clears the bit
  (0x91E3D, 0x2B5D4).
- Death: experience, the drop of `contains` (0x3C4BD), the corpse at the end
  of the death animation (0x6A3CC), gold of a `humanoid` in 4 of 10 cases
  (0x6A639), the alert of level 4 from the body (0x6A45A).
- Senses: the wake test (0x13D1A): the brain bit, a ray, a chance by `iq`
  when the player is behind, Move Silently and deafness, Hide In Shadows and
  invisibility (0x165C4). The alert function 0x160AB and the awareness
  events (0x16055 to 0x16366) spread noise.
- Summons: `gate_in_robot` (0x1374D) for a creature with a gate spell; a
  limit of 2 * difficulty + 3.

## Creature types

| Type | HP | AC | THAC0 | Weapon | Attack | Dice | XP | Str | Regen | Touch | Mage/cleric | Levels |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| Aleena | 200 | 0 | 10 | 44 | ranged/spell | 4d4 | 10000 | 19 |  |  | 4/0 | YPINN |
| armOgre | 33 | 0 | 17 | 39 | melee | weapon | 420 | 18 |  |  |  | BEHOLDER, BEHOLDR2, CRYPTS, MINE |
| armTroll | 54 | 0 | 13 | 39 | melee | weapon | 975 | 19 | 1 |  |  | BEHOLDER, BEHOLDR2 |
| avatar | 256 | -10 | 5 | 33 | melee | 5d10 | 32000 | 19 |  |  | 4/4 | FLAME |
| Banegard | 36 | 2 | 11 | 19 | melee | weapon | 270 | 16 |  |  |  | BANE, BAT, CLAN1 (+8) |
| Beetle | 40 | 3 | 15 | 29 | melee | weapon | 175 | 14 |  |  |  | BANE, CRYPTS, MINE, MULHOR2 |
| Beholder | 49 | 4 | 11 | 81 | ranged/spell | weapon | 4000 | 3 |  |  | 32/0 | BEHOLDER, IBBALAR |
| blackbat | 8 | 7 | 17 | 0 | melee | weapon | 15 | 3 |  |  |  | BANE, BAT, MYRKUL, THIEF0 |
| daland | 40 | 0 | 10 | 27 | melee | weapon | 65 | 16 |  |  |  | MYRKUL |
| dmfw1 | 10 | 4 | 20 | 30 | melee | weapon | 15 | 17 |  |  |  | CRYPTS, MINE, PRISON |
| drowfem | 200 | -3 | 7 | 39 | ranged/spell | weapon | 10000 | 19 |  |  | 0/4 | YPINN |
| durnan | 200 | -10 | 5 | 14 | melee | weapon | 10000 | 19 |  |  |  | YPINN |
| flayer | 68 | table | table | -22 | ranged/spell | 2d4 | 3000 | 14 |  |  |  | DROW3, DROW5 |
| flaynpc | 200 | -5 | 5 | -22 | ranged/spell | 2d4 | 10000 | 19 |  |  |  | YPINN |
| fMephit | 24 | 5 | 17 | -4 | ranged/spell | weapon | 420 | 12 |  |  |  | IBBALAR, IBBALAR2, MINE, MYRKUL |
| fmtnpc | 200 | -5 | 5 | 14 | melee | weapon | 10000 | 19 |  |  |  | YPINN |
| Gargoyle | 36 | 5 | 15 | 33 | melee | 4d4 | 420 | 16 |  |  |  | IBBALAR, MYRKUL |
| gbloser | 4 | 10 | 20 | 0 | melee | weapon | 7 | 8 |  |  |  | THIEF0, THIEF2 |
| gbscsswd | 7 | 10 | 20 | 27 | melee | weapon | 15 | 8 |  |  |  | MINE, PRISON |
| gbscthxe | 7 | 10 | 20 | 11 | ranged/spell | weapon | 35 | 8 |  |  |  | CRYPTS, MINE |
| Ghast | 32 | 4 | 17 | 6 | melee | weapon | 420 | 14 |  |  |  | BANE, CLAN2, CLAN3, DROW3 |
| Ghoul | 16 | 6 | 19 | 19 | melee | weapon | 175 | 14 |  |  |  | BANE, BAT, BONES (+8) |
| GiantBat | 8 | 7 | 17 | 18 | melee | weapon | 65 | 3 |  |  |  | BEHOLDR2, MINE |
| GiantRat | 4 | 6 | 19 | 18 | melee | weapon | 15 | 6 |  |  |  | BANE, BEHOLDER, BEHOLDR2 (+14) |
| gobascim | 7 | 6 | 20 | 27 | melee | weapon | 15 | 8 |  |  |  | MINE, PRISON |
| gorc_axe | 16 | 10 | 19 | 9 | melee | weapon | 35 | 15 |  |  |  | PRISON |
| gorcaaxe | 16 | 6 | 19 | 9 | melee | weapon | 35 | 15 |  |  |  | DROW1, DROW2, DROW3, DROW5 |
| gorcasci | 16 | 6 | 19 | 9 | melee | weapon | 35 | 15 |  |  |  | CRYPTS, DROW1, DROW2 (+5) |
| gorcatax | 16 | 6 | 19 | 11 | ranged/spell | weapon | 65 | 15 |  |  |  | CRYPTS, MINE, PRISON |
| gskelpus | 8 | 2 | 16 | 6 | melee | weapon | 35 | 16 |  |  |  | BAT |
| gskelsci | 72 | 0 | 10 | 42 | melee | weapon | 650 | 16 |  |  |  | BANE, BAT, CRYPTS, PRISON |
| gSpider | 27 | 4 | 15 | 9 | melee | weapon | 270 | 9 |  |  |  | DROW1, DROW3, DROW4 (+2) |
| halaster | 256 | -10 | 1 | 33 | ranged/spell | 10d10 | 10000 | 13 | 1 |  | 4/0 | FLAME |
| hatchlng | 50 | 0 | 7 | -16 | ranged/spell | weapon | 7000 | 10 |  |  | 4/0 | DROW2, DROW5 |
| hffg1 | 10 | 5 | 20 | 6 | melee | weapon | 15 | 16 |  |  |  | MINE, PRISON |
| hmfg1 | 10 | 6 | 20 | 6 | melee | weapon | 15 | 16 |  |  |  | CRYPTS, PRISON |
| hmft4 | 40 | 2 | 17 | 27 | melee | weapon | 120 | 16 |  |  | 0/2 | CLAN1, CLAN2, CLAN3, MYRKUL |
| hmft7 | 70 | -1 | 14 | 27 | melee | weapon | 420 | 16 |  |  | 0/4 | IBBALAR |
| ibbalar | 100 | -6 | 16 | 33 | ranged/spell | 4d4 | 5000 | 14 |  |  | 4/0 | IBBALAR, IBBALAR2 |
| Jeryth | 24 | 6 | 16 | 27 | melee | weapon | 7 | 14 |  |  |  | BEHOLDER |
| kob_bow | 4 | 10 | 20 | 7 | ranged/spell | weapon | 15 | 8 |  |  |  | KOBOLDS |
| kob_spel | 4 | 10 | 19 | 18 | ranged/spell | weapon | 15 | 8 |  |  | 1/0 | KOBOLDS |
| kob_swrd | 4 | 10 | 20 | 18 | melee | weapon | 7 | 8 |  |  |  | KOBOLDS, MINE, PRISON |
| kobaswrd | 4 | 7 | 20 | 18 | melee | weapon | 15 | 8 |  |  |  | KOBOLDS |
| Lich3 | 88 | 0 | 9 | 33 | ranged/spell | 10d1 | 8000 | 12 |  | 22 | 4/0 | CLAN3 |
| mirt | 200 | -7 | 5 | 14 | melee | weapon | 10000 | 19 | 1 |  |  | YPINN |
| Mummy | 51 | 3 | 13 | 37 | melee | weapon | 975 | 16 |  |  |  | MULHOR1, MULHOR2 |
| Ogre2 | 33 | 5 | 17 | 41 | melee | weapon | 270 | 18 |  |  |  | BEHOLDER, CRYPTS, MINE |
| Orc_axe | 8 | 10 | 19 | 9 | melee | weapon | 15 | 12 |  |  |  | PRISON |
| Orc_taxe | 8 | 10 | 19 | 11 | ranged/spell | weapon | 35 | 12 |  |  |  | MINE |
| Orcascim | 8 | 6 | 19 | 9 | melee | weapon | 15 | 12 |  |  |  | CRYPTS, PRISON |
| Orcataxe | 8 | 6 | 19 | 11 | ranged/spell | weapon | 35 | 12 |  |  |  | MINE, PRISON |
| priestes | 200 | -5 | 5 | 44 | ranged/spell | 4d4 | 10000 | 19 |  |  | 0/4 | YPINN |
| prstsprt | 48 | 0 | 11 | 13 | ranged/spell | weapon | 420 | 15 |  |  | 0/3 | MULHOR2 |
| Rat | 4 | 7 | 20 | 18 | melee | weapon | 7 | 4 |  |  |  | BEHOLDR2, BONES, CLAN1 (+2) |
| rffk5 | 40 | 1 | 17 | 33 | ranged/spell | 3d4 | 270 | 16 |  |  | 0/3 | DROW1, DROW2, DROW3, DROW5 |
| rffk8b | 50 | -1 | 15 | 33 | ranged/spell | 4d4 | 975 | 16 |  |  | 0/4 | DROW1, DROW2, DROW3 (+2) |
| rffk9 | 60 | -3 | 15 | 33 | ranged/spell | 4d4 | 1400 | 16 |  |  | 0/4 | FLAME |
| rmfb3 | 30 | 2 | 17 | 7 | ranged/spell | weapon | 120 | 16 |  |  |  | BEHOLDER |
| rmfb5 | 50 | 0 | 15 | 7 | ranged/spell | weapon | 270 | 16 |  |  |  | DROW1, DROW2, DROW3, DROW5 |
| rmfb9 | 90 | -4 | 11 | 7 | ranged/spell | weapon | 1400 | 16 |  |  |  | FLAME |
| rmfg3 | 30 | 2 | 17 | 6 | melee | weapon | 65 | 16 |  |  |  | BEHOLDER |
| rmfg5 | 50 | 0 | 15 | 6 | melee | weapon | 175 | 16 |  |  |  | DROW1, DROW2, DROW3 (+2) |
| rmfg9 | 90 | -4 | 11 | 6 | melee | weapon | 975 | 16 |  |  |  | FLAME |
| rmmk5 | 20 | 2 | 18 | 33 | ranged/spell | 3d4 | 270 | 9 |  |  | 3/0 | DROW1, DROW2, DROW3 (+2) |
| rmmk9 | 36 | 0 | 16 | 33 | ranged/spell | 4d4 | 1400 | 9 |  |  | 4/0 | FLAME |
| rmmk9b | 36 | 0 | 16 | 33 | ranged/spell | 4d4 | 1400 | 12 |  |  | 4/0 | FLAME |
| Scarab | 40 | 3 | 13 | 9 | melee | weapon | 175 | 16 |  |  |  | MULHOR1 |
| sfmtd2 | 24 | 5 | 20 | 19 | melee | weapon | 35 | 13 |  |  |  | BEHOLDER, BEHOLDR2, THIEF1, THIEF2 |
| sfmtf2 | 20 | 5 | 20 | 22 | ranged/spell | weapon | 65 | 13 |  |  |  | BEHOLDER, BEHOLDR2, THIEF1 |
| shdleadf | 80 | 1 | 16 | 22 | ranged/spell | weapon | 975 | 15 |  |  |  | BEHOLDER, THIEF2 |
| shdleadg | 80 | 1 | 16 | 6 | melee | weapon | 650 | 15 |  |  |  | BEHOLDER, THIEF2 |
| shmfb5 | 50 | 3 | 18 | 7 | ranged/spell | weapon | 270 | 16 |  |  |  | BEHOLDER |
| shmfc3 | 30 | 4 | 19 | 7 | ranged/spell | weapon | 120 | 16 |  |  |  | BEHOLDER, BEHOLDR2, THIEF1, THIEF2 |
| shmfg5 | 50 | 3 | 18 | 6 | melee | weapon | 175 | 16 |  |  |  | BEHOLDER, BEHOLDR2, THIEF2 |
| shmtd2 | 12 | 5 | 20 | 19 | melee | weapon | 35 | 13 |  |  |  | THIEF0, THIEF1, THIEF2 |
| shmtd6 | 36 | 3 | 18 | 19 | melee | weapon | 270 | 13 |  |  |  | BEHOLDER, BEHOLDR2, THIEF1, THIEF2 |
| shmtf2 | 12 | 5 | 20 | 22 | ranged/spell | weapon | 65 | 13 |  |  |  | THIEF0, THIEF1, THIEF2 |
| shmtf4 | 24 | 4 | 19 | 22 | ranged/spell | weapon | 175 | 13 |  |  |  | THIEF2 |
| shmtf6 | 36 | 3 | 18 | 22 | ranged/spell | weapon | 420 | 13 |  |  |  | BEHOLDER, BEHOLDR2 |
| ske_axe | 8 | 7 | 19 | 27 | melee | weapon | 35 | 13 |  |  |  | BANE, BONES, CLAN1 (+9) |
| ske_bow | 8 | 7 | 19 | 7 | ranged/spell | weapon | 65 | 13 |  |  |  | BANE, CLAN1, CRYPTS (+4) |
| ske_sci | 8 | 7 | 19 | 27 | melee | weapon | 35 | 13 |  |  |  | BANE |
| ske_swd | 8 | 7 | 19 | 27 | melee | weapon | 35 | 13 |  |  |  | BONES, CLAN1, CLAN2 (+3) |
| Spectatr | 36 | 4 | 11 | 0 | ranged/spell | 2d3 | 2000 | 3 |  |  |  | BEHOLDER, BEHOLDR2, IBBALAR, IBBALAR2 |
| sSpider | 9 | 8 | 19 | 18 | melee | weapon | 120 | 9 |  |  |  | DROW1, DROW3, DROW4, DROW5 |
| thashasa | 40 | 2 | 7 | 13 | ranged/spell | weapon | 975 | 14 |  |  | 4/4 | IBBALAR |
| Troll | 54 | 4 | 13 | 33 | melee | weapon | 650 | 19 | 1 |  |  | BEHOLDER, BEHOLDR2, DROW2 (+2) |
| xabash | 98 | -3 | 5 | 81 | ranged/spell | weapon | 5000 | 3 |  |  | 4/1 | BEHOLDER, BEHOLDR2 |
| Zombie | 16 | 8 | 19 | 6 | melee | weapon | 65 | 14 |  |  |  | BANE, BONES, CLAN1 (+9) |

Weapon: the weapon type (a negative value is a spell missile or the dice of
the type). Attack: attack type 1 is melee, 0 ranged or a caster. Dice: the
dice of the type (+0x6E d +0x6C) where the setup sets them, else the dice of
the weapon. Mage/cleric: the caster levels of the type. Levels: the levels
of DTU.MSN that place the type.

Special cases of the setup by name (ai.md, "Creature setup by name"): the
lich (flags 0x7042: undead, weapon_resistant, no_sleep, magic_resistant,
electricity_immune, humanoid; 10d1; touch spell 22), the CLAN3 type 10 (the
same), the spectator (attack type 0, weapon 0, 2d3), the hatchling
(fire_immune, fire_resistant, 7000 XP), gargoyle, mephits and flayers
(weapon 0x21, 4 dice; the flayer 2d4 and no regeneration), the boss values
of YPINN (200 hit points, str 19, 10000 XP).

## Abilities: original and port

| Creature | Ability | Original | Port | Status |
|---|---|---|---|---|
| all | values of the type after the setup by name and the level fixes | 0xAC7B2, 0xACCDB | `creature_setup_by_name`, `level_start_npcs` (dtuobj.c) | Same (965 placements, check_creature_setup.py) |
| all | NPC overlay: THAC0, armor class, caster levels | 0xA7E97, table 0xA504A | `dtu_objects_level_setup`, dtunpc.h | Same |
| all | melee weapon test, to-hit roll, damage dice | 0x6CFF8, 0x2A7F3, 0x2AF5E | `weapon_test`, `damage_player_w` | Same (combat.md C2) |
| all | ranged attack, spell missile | 0x11B06, 0x10040 | `ai_fire`, `creature_cast` | Same |
| casters | creature spells | 0x92D99, 0x9231F, 0x91FDC | dtuspell.c | Same (combat.md B10) |
| Lich, CLAN3 type 10 | touch spell 22 (Hold Person) after each hit with damage | 0x2A637 to 0x2A67B | `damage_player_w` | Fixed, 9f709bf (audit C16) |
| Lich, CLAN3 type 10 | magic resistance from the setup flags 0x7042 | 0xACA16, 0xAE77F | `monster_flags_from_table`, `dtu_spell_save` | Fixed, fdb1c24: the port read `magic_resistant` from the table line only, so the setup flag had no effect |
| Hatchling, 0x5CBA types | fire_resistant from the setup flags | 0xAC7B2, 0xAD2A1, 0x91DAF | `monster_flags_from_table` | Fixed, fdb1c24 (same cause) |
| Troll, armTroll, Halaster, Mirt | regeneration of hit points | 0x1440C | `dtu_do_ai_frame` | Same |
| Troll, armTroll, Halaster, Mirt | revive of a dead creature | 0x1440C to 0x14655 | `corpse_regen_frame` | Fixed, 8b11534 (`Dtu_fix_revive`, below) |
| all regenerating | fire or acid ends the regeneration | 0x91E3D, 0x2B5D4 | `dtu_spell_damage_roll`, the fire dice of `damage_creature_w` | Same |
| all regenerating | no experience for a kill with the regeneration bit | 0x29A9C, the test before 0x4EA50 | `creature_dies` | Same (with Dtu_fix_revive the troll gets up, so the kill gives no experience until fire or acid took the bit) |
| all | wake test, sight | 0x13D1A, 0x165C4 | `wake_test`, `player_is_visible` | Same |
| all | group alert, levels 3 to 5 | 0x160AB | `dtu_creature_alert` | Fixed, df15f2e |
| all | awareness events of the alerts | 0x16055, 0x16304, 0x162A1, 0x16366 | `awareness_event`, `awareness_events_frame` | Fixed, 35f9714 |
| all | alerts of level 1 and 2: wake, fire, doors, missile on a wall or scenery | 0x13EFF, 0x11EB7, 0x7DBEB, 0x7DE62, 0x26F73, 0x2653C | `dtu_do_ai_frame`, `ai_fire`, `wall_open_door`, `wall_close_door`, `missile_hit_wall` | Fixed, 35f9714 |
| all | alert of a rest place | 0x3E2C1 | `dtu_char_rest` | Fixed, df15f2e |
| all | alerts of level 2 when the player runs into scenery or a wall fast, and of the burst 0x93294 | 0x26142, 0x26194, 0x2668E, 0x93477 | none | Open, small |
| all | alert of the player bump in the move code | 0x9E70D | the collision of the port (0x2707A) | Same in effect: the port has one collision path |
| all | hidden or invisible player: chase toward a believed position that moves at random (16.0 per 2.0 s), set by the events of type 2 to 4 | 0x12BE8, 0x15FBC | none: a hidden player is not seen at all | Open, medium (audit C20, sweep G4) |
| all | Chill Touch of a creature on another creature | 0x29E3D | none | Open, small (C18; creatures fight each other only in scripts) |
| all | agitation counter | 0x167174 | none | Not ported (no reader in the AI found) |
| all | pathing through doors, the door bit | 0x1686D, 0x14AF2, 0x25CAA | `create_path`, door opener | Same (ai.md, "Paths") |
| all | morale, flee | 0x10ADF | `morale_check` | Same |
| all | death, drops, gold, corpse on the floor | 0x2A091, 0x3C4BD, 0x6A639, 0x6A3CC | `creature_dies`, `creature_is_corpse` | Same |
| extraplanar | no corpse drawn | 0x6A5A0 | `creature_is_corpse` | Same |
| Spectator, Beholder, Xabash | gaze or eye attacks | none: spell missiles of the weapon table and the mage levels | `ai_fire`, dtuspell.c | Same (no gaze code in 1.3) |
| Spiders | web | none: no web code in 1.3 | none | Same |
| Mummy, Ghoul, Ghast | disease, paralysis | none: no such code in 1.3 | none | Same |

## Dtu_fix_revive

1.3 has the code that lets a dead regenerating creature get up
(0x1440C to 0x14655: one hit point each `regeneration` seconds; with all hit
points and the player farther away than the sum of both body radii: state
REST, goal SRCH, the animation played backward, hostile, awareness 4, the
morale check, the alert of level 4). The damage function clears all effects
of a creature that dies (0x2A1D0, also 0x916D9 for spell damage), the
regeneration bit too, so the code never runs, and the kill with the bit set
gives no experience. The fidelity rule calls this a defect: the port keeps
the bit (unless fire or acid took it) and the troll gets up, as in AD&D 2nd
Edition. The options `-origobj` and `-origrevive` give the 1.3 rule. Test:
`check_creature_setup.py --abilities` (MINE, troll obj 1: up after 56.6 s
with 54 hit points; with -origrevive it stays dead).

## Tests

- `python -I tools/check_creature_setup.py --exe EXE --datadir ... --work
  DIR --abilities`: the touch spell, the revive (and -origrevive) and the
  group alert.
- `-chartest kill=OBJNUM`: the player kills a creature (new test command).
- `-chartest attackedby=10` in CLAN3: the lich casts Hold Person
  ("Hold Person cast on You by Lich for 4 minutes.").
