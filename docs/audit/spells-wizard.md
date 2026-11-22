# Audit: wizard spells

Scope: the 37 `mage_use` lines of the `$SPELL` tables (the same in all 46
tables) and the rules a wizard spell needs: the cast, the slots and the rest,
the spell lists, armor, multi-class casting, scrolls and wands. The priest
spells and Turn Undead, and the choice of spells by creatures, are in other
audits.

Method: the cases of the spell effect function 0x8CFFF (jump table 0x8CF1F,
addresses below) and its helpers were read in the Ghidra export of
D:\dev\DTU-Decomp (`local/decompiled/functions/0008cfff.c`), the damage roll
0x91BC3, the damage function 0x911AA, the missile setup 0x56FF6, the cast
0x41A4D, the slot function 0x3575B and the `$SPELL` reader 0x242F6. The data
tables were read from `local/dtu13.elf`: the mage slot table 0x145CD8 (18 rows
of 9) and the cast times 0x13A8DB (56 words) match the port. The earlier spell
audit is `docs/re/spells.md` (sections 1 to 10); this audit adds section 11.

Test aid: `-chartest FRAME:spelltest=SPELL.LEVEL.OBJ.TRIALS` (new) casts a
spell at a caster level on a creature (OBJ -1: the player) and writes the hit
points, the effects and their times to `-objlog`. Each spell was run in BANE
(`-char SG0.SG -noattack -fixedfps 30`) on the Beetle (obj 18, 40 hp), the
Giant Rat (obj 16), the large skeleton (obj 14) or the player, with and
without `-origcombat`.

Status: "same" (the port follows 1.3 and the rule is intended), "fixed"
(commit on the branch `worktree-agent-ad33fe223619b16d5`), "kept" (a
difference from the manual or AD&D 2nd edition that is not a defect), "open".

## Spells

L is the caster level (0x91EC1). Mean damage: the mean over 20 test casts.

| # | Spell | 1.3 case | Port | 1.3 rule | Status | AD&D 2e |
|---|---|---|---|---|---|---|
| 2 | Armor | 0x8D4E7 | `apply_spell` case 2 | self, AC 6, pool 8 + L of weapon damage | same | 2e: 8 + L points, AC 6 |
| 4 | Burning Hands | 0x8D614 | case 4 | AREA, 6 missiles, fire 1d3 + 2L, save half | same | 2e: one cone, at most +20 |
| 5 | Chill Touch | 0x8D633 | case 5, `spell_self` | no `self_cast_ok`: the missile does nothing | fixed earlier (`Dtu_fix_chill_touch`) | |
| 6 | Color Spray | 0x8D6D4 | case 6 | 1d6 missiles, sleep, blind or stun by level | same (rat: sleep 420 s; beetle: blind 180 s) | |
| 7 | Confusion | 0x8DB82 | case 7 | 1d4 + L missiles, save, L + 2 minutes | same | |
| 9 | Continual Light (Mage) | 0x8DC1D | case 9 | cast_in_front: light and blindness around the caster | same | |
| 12 | Detect Magic | 0x8E019 | case 12 | 2L minutes, blue tint | same (`Dtu_fix_magic_tint` earlier) | |
| 13 | Dispel Magic | 0x8E0B5 | case 13 | cast_in_front: all effects of the caster end | kept (spells.md section 8) | 2e: an area at range |
| 14 | Fear | 0x8E2D8 | case 14 | save, L minutes, weapon drop | same | |
| 15 | Feather Fall | 0x8E464 | case 15 | L minutes, no end text | same | |
| 16 | Fireball | 0x8E55C | case 16, `d6_per_level` | min(L, 10) dice of 0 sides + 6L: 6L, no limit | fixed, 24fa78a (`Dtu_fix_spell_dice`): Ld6, at most 10d6, save half. L5 mean 13.1 (1.3: 24.0), L12 28.1 (1.3: 39.0) | 2e rule |
| 17 | Flaming Sphere | 0x8E5B1 | case 17, `sphere_hit` | no damage dice: nothing | fixed, 24fa78a (`Dtu_fix_flaming_sphere`): 2d4 fire for L rounds, a save each round negates. L4: rounds at 0.2, 60.2, 120.2, 180.2 s | 2e rule (the sphere is not steered) |
| 19 | Fly | 0x8E665 | case 19 | 10 (1d6 + L) minutes | same | 2e: 1d6 + L turns, the same |
| 21 | Haste | 0x8E79C | case 21 | L + 3 minutes, ends Slow | same (`Dtu_fix_haste_attack` earlier) | |
| 22 | Hold Person | 0x8E85A | case 22 | humanoid, save, 2L minutes | same | |
| 23 | Hold Undead | 0x8EA21 | case 23 | undead of level L or lower, 1d4 + L minutes | same (L10 on the skeleton: 840 s) | |
| 24 | Improved Invisibility | 0x8EB53 | case 24 | L + 4 minutes | same | |
| 25 | Infravision | 0x8EBE8 | case 25, `long_duration` | (L + 2) hours in 32 bits: wraps from L 8 (ends at once); the port read L 3 to 7 as no end | fixed, 24fa78a (`Dtu_fix_spell_hours`): at most 4 h 33 min | 2e: 2 h + 1 h/level |
| 26 | Invisibility | 0x8ECB4 | case 26 | until an attack | same | |
| 27 | Jump | 0x8ED20 | case 27 | 1d3 + L minutes | same | |
| 28 | Knock | 0x8FFE5 (common end) | `spell_self`, dtuobj.c wall hit | `self_cast_ok`: cast on the caster, nothing | fixed, 24fa78a (`Dtu_fix_knock`): a missile; at seg 488 side 4 of BANE "Knock hits seg 488 side 4" | 2e: opens a locked door |
| 29 | Levitate | 0x8EDB2 | case 29 | 10L minutes, no cast while levitating | same | |
| 30 | Light (Mage) | 0x8DC1D | case 30 | cast_in_front, blind 10L minutes | same | 2e: blinds only when cast at the eyes |
| 32 | Lightning Bolt | 0x8EE76 | case 32, `d6_per_level` | as Fireball, electricity; missile life 2 s | fixed, 24fa78a (`Dtu_fix_spell_dice`). L5 mean 15.3 (1.3: 27.0) | 2e rule |
| 33 | Magic Missile | 0x8EED5 | case 33, `missile_count` | 1d4, save half, L/2 + 1 missiles (at most 5); `damage_bonus=1` never read | fixed, 24fa78a (`Dtu_fix_magic_missile`): 1d4 + 1, no save, (L + 1)/2 missiles. L1 3.5 (1.3: 1.85), L9 18.2 (1.3: 9.9) | manual and 2e rule |
| 36 | Melf's Acid Arrows | 0x8EFE0 | case 36 | 4L acid, save half | fixed earlier (`Dtu_fix_melf_acid`) | |
| 37 | Melf's Minute Meteors | 0x8F021 | case 37 | min(L, 5) missiles, 1d4, save half | kept: the manual says one per level | 2e: one per level, 1d4 fire, +2 attack roll |
| 39 | Protection From Evil | 0x8F116 | case 39 | 2L minutes | same | |
| 41 | Rary's Mnemonic Enhancer | 0x8F21D | case 41 | one cast, slots until Dispel Magic | fixed earlier (`Dtu_fix_rary_manual`); not run by `spelltest` (choice box) | |
| 44 | Scare | 0x8F467 | case 44 | level 6 or lower, 1d4 + L minutes | kept: the manual says level 6 or more is safe | 2e: below 6 HD |
| 45 | Shield | 0x8F6AE | case 45 | 5L minutes, AC 3, stops Magic Missile | same | |
| 46 | Sleep | 0x8F766 | case 46 | level 3 or lower, pool 4 to 16 | same (spells.md section 8) | 2e: 2d4 HD |
| 47 | Slow | 0x8FAD8 | case 47 | save, L + 3 minutes | same | |
| 48 | Spider Climb | 0x8FBEE | case 48 | L + 3 minutes | same | |
| 49 | Stone Skin | 0x8FC7A | case 49 | 1d4 + L/2 hits | same (`Dtu_fix_stoneskin_word` earlier) | 2e rule |
| 50 | Strength | 0x8FCFA | case 50, `long_duration` | L hours in 32 bits: wraps from L 10; the port read L 5 to 9 as no end | fixed, 24fa78a (`Dtu_fix_spell_hours`): at most 4 h 33 min | 2e: 1 h/level |
| 51 | "Burning Hands" (cold) | 0x8E4F6 | case 51 | the case is Find Traps (30 minutes); the cold dice of the line are never rolled | open, no effect for players: only the weapon Ice Shards (`pc_spell=51`) casts it, and no level, creature or new character has it | |

## Rules shared by the wizard spells

| Rule | 1.3 | Port | Status |
|---|---|---|---|
| Slots | 0x3575B: the mage table 0x145CD8 by mage level, no intelligence bonus | `dtu_spell_compute_slots` | same (table checked byte by byte) |
| Rest | 0x3551B fills all slots | `dtu_spell_restore_slots` | same |
| Cast time | words of 0x13A8DB in seconds; a scroll or a wand casts at once | `Cast_time_13` | same (table checked) |
| Level limit | 0x3598C, 0x359C0: "Spell is of too high a level." when the spell level - 1 is above the class level, then "Out of spell points ..." | `cast_allowed` | same |
| Armor | 0x41A4D: a mage spell in armor without `MAGIC_OK` (item flag 0x19FD34 bit 1) is refused, sound 3000 | `cast_allowed` | same |
| Climbing, Levitate | player +0x1AE bit 0x80: refused | `cast_allowed` | same |
| Interruption | a hit during the cast time ends the cast, the slot is used | `dtu_spell_interrupt` | same |
| Multi-class | 0x91EC1: a mage class casts at the mage level; a scroll or a wand at the level of the class | `dtu_spell_player_level` | same |
| Learning | 0xB65BB, 0xB69AD: a mage spell goes to the list of a mage at once, no intelligence roll; a second copy stays a scroll | `dtu_spell_insert` | kept (2e has a chance to learn) |
| Scrolls and wands | any class casts them, at the level above | as 1.3 | kept |

Spells with no placement in a level and not given to a new mage (`lvl.py` over
the 31 levels; frontend.md 0x8BB91): 19, 21, 22, 25, 26, 41, 44, 47, 50, 51.
Scripts and shops that may give them were not traced.

## Not tested

The missile flight and homing of the new rules (the test aid calls 0x8CFFF
directly), the door that Knock opens (only the wall hit was logged), the
Flaming Sphere rounds across a save and load (they end, as the acid does).
