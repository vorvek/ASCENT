# Audit: priest spells and turn undead

Scope: every cleric spell of 1.3, the spells that items cast with cleric
spell code, the cleric spell slots and the wisdom bonus, the caster level,
and turn undead (`dtuspell.c`, `dtucombat.c`). The wizard spells are a
separate audit.

Method: the cases of 0x8CFFF (jump table 0x8CF1F), the ends of 0x9002C,
turn undead 0xADC46 and the registration 0xAC4C1 were disassembled from
`D:\dev\DTU-Decomp\local\dtu13.elf` (objdump) and compared with the port
line by line. The tables 0x13A8DB (cast times), 0x145DF8 (cleric slots),
0x1403D2 to 0x1404C2 (wisdom bonus), 0x145ED8 (spell levels per cleric
level) and 0xA6487 (turn table) were read from the image and compared with
the port. The texts were read at their addresses (0x12D8AE to 0x12E162,
0x131B64) and match the port. The `$SPELL` lines are those of `YPINN.BIN`.

AD&D 2nd edition column: from the rules of the Player's Handbook. The
scanned PDF has no text layer, so it was not read again for this audit.

Rules of each spell: [docs/re/combat.md](../re/combat.md) B3 and
[docs/re/spells.md](../re/spells.md), section 11.

## The table fixup 0x1A344

1.3 writes over the spell records after each table read (0x1A344; the
writes are listed in D:\dev\DTU-Decomp\analysis\sweep\range1-gaps.md, G1 and
the appendix). Another agent ports the fixup as a layer under
`bm_tbl_get`. This audit takes the values after the fixup as the 1.3 rules.
For the priest spells:

| # | After 0x1A344 | Effect code in the port |
|---|---|---|
| 0 | creature path: 2d8 + 3 per level, cast sound 2129 | case 0 rolls the line dice: OK |
| 1 | range 0, radius 0, flags 0x102 (self) | OK |
| 8, 31 | class cleric; range 720, radius 3, flags 3 (area, no `cast_in_front`): a missile with a light area of radius 3 at the impact, model of spell 17; light 60.0 (8) or 20.0 (31) | `dtu_spell_area`, `light_object`: OK. spells.md section 8 (the lights around the caster) describes the table text, not 1.3 |
| 17 | renamed Produce Fire: cleric, level 4, range 240, radius 24, flags of spell 16 (area, any object type), fire 1d4 + 1 per level | case 17 (the Flaming Sphere case) rolls the line dice: OK |
| 38 | range 0, radius 120, flags 0x102 | OK |
| 40 | class 0x27 | OK |
| 42 | renamed Cure Blindness or Deafness, cleric level 3, flags 0x102 | case 42: OK |
| 51 | renamed Find Traps, cleric level 2, flags 0x102 | case 51: OK (check find_traps) |
| 52 | copy of spell 22, level 2, class cleric: range 720, radius 40, flags 3 (area), `cast_on_top` 133 (not 130 of the line) | case 22/52. Fixed: the counter Spell_info+0x49 (1d4 at each cast, 0x56FF6; one for each hold, 0x8E8C5), so the area holds at most 1d4 targets |
| 53 | copy of spell 39, class 0x25 | case 53: OK |
| 54 | copy of spell 12, class cleric | case 54: OK |
| 55 | copy of spell 13, class cleric: range 720, radius 60, flags 0x803 (area, any object type, no `cast_in_front`) | case 55: OK. Open: `dtu_spell_area` takes only the player and creatures; with +0x37 bit 3 1.3 takes every object in the radius (potions, scenery, spell lights) |

So the 1.3 priest list is 0, 1, 3, 8, 10, 11, 17, 18, 20, 31, 34, 35, 38, 40,
42, 43, 51, 52, 53, 54, 55.

## Spells

L = caster level, T = target level. "min" = 60.0 game seconds (one round).
Status: OK = the port follows 1.3; Fixed = changed by this audit (commit
664a877); Open = not done.

| # | Spell | Case | Port | Status | AD&D 2e difference |
|---|---|---|---|---|---|
| 0 | Abjure | 0x8D297 | `apply_spell` case 0 | OK. The spin of the creature (movement type 3, rotation 0.05) is not in the port (cosmetic). The hit sound volume 1.0 is cut to 0.5 by 0x2E967 in both | 2e: 50 % plus 5 % per level of difference, the creature goes home. 1.3 has the same chance (d20 >= 11 - L + T) but kills it on d100 >= 70 + 2T |
| 1 | Aid | 0x8D3F7 | case 1 | Fixed: the happy sound of the player type at the player (0x8D435). The rest OK | 2e also gives +1 to saves |
| 3 | Bless | 0x8D58E | case 3 | OK | 2e: +1 to saves against fear, on allies in an area |
| 8 | Continual Light (Cleric) | 0x8DC1D, 0x90CEF | case 8 and `dtu_spell_area` | Fixed: a save returns before the damage of a light sensitive creature; a blinded player gets the white flash (0x8DD89). Open: the light +0x1A6 and the vclip 0x8CAD3 on the target | same idea; no damage in 2e |
| 10 | Cure Light Wounds | 0x8DE4B | case 10, `heal` | Fixed: the happy sound on the player. `Dtu_fix_cure_aid` (new) keeps the hit points of Aid above the maximum; 1.3 cuts them (0x8DEE2) | same |
| 11 | Cure Serious Wounds | 0x8DF37 | case 11, `heal` | as 10 | 2e: 2d8 + 1; 1.3: 3 + rand() % 15 (3 to 17, flat) |
| 18 | Flame Walk | 0x8E5D1 | case 18 | OK (bit 10, the same as Resist Fire) | 2e: immunity to normal fire |
| 20 | Free Action | 0x8E6F2 | case 20 | Fixed: with Free Action on the case returns before it ends Hold (0x8E6F8). 1.3 sets the Hold timer to 0 (the end comes in the next frame), the port ends Hold at once | same |
| 31 | Light (Cleric) | 0x8DC1D | case 31 | as 8; 1d6 to a light sensitive creature that fails | 2e: no damage |
| 34 | Magical Stone | 0x8EEFE | case 34 | OK: one die, two against undead (Spell_info+0x3B) | 2e: 3 stones; 1.3: 1d3 (0x56FF6) |
| 35 | Magical Vestment | 0x8EF49 | case 35 | OK | 2e: AC 5, better above level 5; 1.3: AC 6 at levels 1 and 2 (integer division of a negative number) |
| 38 | Prayer | 0x8F078, 0x56D6A | case 38 | OK: Bless for L min on the caster only | 2e: allies +1, enemies -1 to hit, damage and saves |
| 40 | Protection From Fire | 0x8F1B2 | case 40 | Fixed: no effect while bit 9 is on (the port added a second pool) | same pool of 12 per level |
| 42 | Cure Blindness or Deafness (the line says Resist Cold; 0x1A344 renames it, level 3) | 0x8F2FA | case 42 | Fixed: the blindness ends through its timer (0x8F37C), so "Blinded expired." shows in the next frame | 2e: the same level 3 |
| 43 | Resist Fire | 0x8F3CD | case 43 | OK | 2e also +3 to saves |
| 17 | Produce Fire (after 0x1A344) | 0x8E5B1 | case 17 | OK (the line dice of the fixup) | 2e: a fire in the hand, 1d4 + 1 per level |
| 51 | Find Traps (after 0x1A344) | 0x8E4F6 | case 51 | OK: bit 20 for 1800 s | 2e: 3 turns, shows traps; 1.3 Find/Remove Traps 100 |
| 52 | Hold Person (Cleric) | 0x8E85A | case 52 | Fixed: the counter Spell_info+0x49 (1d4). Open: the `cast_on_top` 133 vclip (0x8CCED), and on the player the stand animation, the cleared requests and the input flush (0x8E985) | 2e: 1 to 3 targets |
| 53 | Protection From Evil (Cleric) | 0x8F116 | case 53 | OK | 2e: +2 to AC and saves against evil creatures |
| 54 | Detect Magic (Cleric) | 0x8E019 | case 54 | OK | same |
| 55 | Dispel Magic (Cleric) | 0x8E0B5 | case 55 | OK as the mage spell 13 (the wizard audit owns the case). The line has no `cast_in_front`, so the cleric version fires a missile | 2e: a level check per effect |

Shared rule, also fixed: the end of Bless (0x9002C, 0x9013F) cuts a creature
to its maximum, as it cuts the player.

Spells that items cast with the same cases (0x37A6A): 1, 11, 18, 20, 35, 38,
40, 42, 43, 53 (Spider Pieces, potions, the Ring of Free Action). The port
calls the same `apply_spell` (`spell_on_player`, dtuchar.c). The script
statement EA (0xAB47B) also casts through 0x8CFFF.

## Casting

| Rule | Address | Status |
|---|---|---|
| Cast times of the 56 spells | 0x13A8DB | OK, all 56 values equal `Cast_time_13` |
| Cleric slots per level | 0x145DF8 | OK, 18 rows equal |
| Wisdom bonus, levels 1 to 4, only for a level with a base slot | 0x1403D2 to 0x1404C2, 0x3575B | OK. Same as 2e: 13 +1 first, 14 +1 first, 15 +1 second, 16 +1 second, 17 +1 third, 18 +1 fourth, 19 +1 first and third |
| The cleric spells of each level go to the list | 0xB75A8, 0x145ED8 | OK |
| Caster level: cleric spells use the cleric class | 0x91EC1 | OK |
| Armor does not stop a cleric spell | 0x41A4D | OK |
| No spell failure for a low wisdom | none | as 1.3 (2e has one below 13) |

## Turn undead (0xADC46)

| Rule | Status |
|---|---|
| Half a second between two uses (0xADC54) | OK |
| Cleric level: class 2 if it is the cleric, else class 1; a multi-class cleric turns at the cleric level (0xADC91) | OK |
| The Cloak (armor 45 in the cloak place) turns as level 7 for any class (0xADCB3) | OK |
| One d20 and one count of 2 to 12 (rand() % 11 + 2, flat) for the use (0xADCFF) | OK. 2e: 2d6 |
| Table 0xA6487, 9 classes by 14 levels | OK, every byte compared |
| The order: the NPC list sorted by class (0xADB7D) | OK, in object order (1.3: registration order) |
| A turn also takes one of the count (0xAE226 jumps to 0xAE17F, 0xAE1A0) | Fixed by the earlier agent of this task, checked here. bugs.md T3 said that turns had no limit |
| A T entry writes 0xFF over the d20 (0xAE1B8), and 0xAE1C8 reads it without a sign, so every later undead with a number is turned | Fixed: `Dtu_fix_turn_undead` keeps the d20; -origcombat gives the 1.3 rule |
| The bit 0x10 of the next undead is cleared before the count test (T2) | as before, `Dtu_fix_turn_undead` |
| An undead with no name match: class = the higher class level (0 based) + 1, 8 from level 7, bit 0x10 clear (0xAC4C1) | Fixed: the port used 8. It changes only the order |
| Messages TEX 927 to 931 (0x396C28 to 0x396C38), "You can no longer attempt to turn the %s." (0x131B64) | OK |

The rest of the cleric checks of the task: there is no poison, disease,
raise dead or temple service in 1.3 (ai.md; frontend.md: no temple screen in
Waterdeep). Bless, Aid and Prayer share bit 1: Bless and Prayer do nothing
while it is on, Aid replaces the timer. Resist Fire and Flame Walk share bit
10. The two Protection From Evil spells share bit 23.

## Checks

`-chartest 30:priestcheck` with `-charlog` (dtu_spell_priest_check, a forced
save `Priest_force_save`) and the runner `tools/priestcheck.py`, which runs
it with and without -origcombat on BANE: 23 checks, all pass (Protection
From Fire, Bless and Prayer, Protection From Evil, the cure after Aid in both
settings, Cure Blindness or Deafness, Free Action, Find Traps, the Hold
Person counter, the caster level of a
fighter/cleric, five slot rows, Light with a save and without, the end of
Bless on a creature, cures on undead and living creatures, Hold Person on
undead and humanoids, the turn class of every creature of the level).

## Open

- `cast_on_top` vclips (0x8CCED, a vclip object that follows the target for
  the duration): Hold Person 52 (133 after 0x1A344) and the mage spells 7,
  14, 22, 23, 44.
- The area of 0x90CEF with +0x37 bit 3 (Dispel Magic 55, Produce Fire 17)
  takes every object type; `dtu_spell_area` takes only the player and
  creatures. Needs the fixup layer for the bit.
- Produce Fire is checked only through its case: its dice exist only after
  the fixup layer of the other agent.
- Hold Person on the player: animation 1, the requests 0x13B69F and 0x13B6A3
  cleared, the input flushed (0x40E50), 0x14530D = 0 (0x8E985).
- The light +0x1A6 and the vclip 0x8CAD3 on a target of the light spells.
- The spin of an abjured creature.
- Not tested in a real game: turn undead in sight of undead after the
  changes (the soak test calls it), the happy sound.
