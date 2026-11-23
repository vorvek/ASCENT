# Audit: wizard spells

Scope: the wizard (mage) spells of 1.3 and the rules a wizard spell needs: the
cast, the slots and the rest, the spell lists, armor, multi-class casting,
scrolls and wands. The priest spells and Turn Undead, and the choice of spells
by creatures, are in other audits.

## The table fixup comes first

1.3 does not play the `$SPELL` lines as written. After each table read,
0x1A344 writes fixed values into the spell records (D:\dev\DTU-Decomp,
`analysis/sweep/range1-gaps.md`, gap G1 and its appendix). It renames spell 17
"Produce Fire" and makes it a level 4 cleric spell, renames 51 "Find Traps"
(cleric, level 2), changes class masks, flags (AREA and cast on the caster),
range, radius and dice of about 27 spells. The port reads the table text
only. Another change ports the fixup as an override layer under `bm_tbl_get`.

This audit takes the values after the fixup as the 1.3 rule. A port result
that differs only because the layer is missing is listed as "fixup" and is
not corrected in `dtuspell.c`. Until the layer is in, the port plays those
spells by the table text.

Method: the cases of the spell effect function 0x8CFFF (jump table 0x8CF1F),
the roll 0x91BC3, the damage function 0x911AA, the missile setup 0x56FF6, the
cast 0x41A4D, the slots 0x3575B and the `$SPELL` reader 0x242F6, in the Ghidra
export of D:\dev\DTU-Decomp. Data from `local/dtu13.elf`: the mage slot table
0x145CD8 and the cast times 0x13A8DB match the port byte for byte. The earlier
spell audit is `docs/re/spells.md` sections 1 to 10; this audit adds section
11.

Test aid: `-chartest FRAME:spelltest=SPELL.LEVEL.OBJ.TRIALS` (new) casts a
spell at a caster level on a creature (OBJ -1: the player) and writes the hit
points, the effects and their times to `-objlog`. Each spell was run in BANE
(`-char SG0.SG -noattack -fixedfps 30`) on the Beetle (obj 18, 40 hp), the
Giant Rat (obj 16), the large skeleton (obj 14) or the player, with and
without `-origcombat`. The runs used the table text (no fixup layer).

Status: "same" (the port follows 1.3), "fixup" (1.3 after the fixup differs
from the table text; the override layer gives it), "fixed" (a 1.3 defect,
commit on the branch `worktree-agent-ad33fe223619b16d5`), "kept" (a
difference from the manual or AD&D 2nd edition that is not a defect).

## Spells

L is the caster level (0x91EC1). Flags after the fixup: the word +0x36, bit 0
AREA, bit 8 cast on the caster.

| # | Spell | 1.3 case | 1.3 after the fixup | Port | Status | AD&D 2e |
|---|---|---|---|---|---|---|
| 2 | Armor | 0x8D4E7 | self, AC 6, pool 8 + L of weapon damage | case 2 | same | same |
| 4 | Burning Hands | 0x8D614 | AREA, 6 missiles, fire 1d3 + 2L (the fixup writes the table values again), save half | case 4 | same | at most +20 |
| 5 | Chill Touch | 0x8D62F | the fixup sets the cast on the caster: L + 3 minutes | case 5, `spell_self` | same by default. `Dtu_fix_chill_touch` describes 1.3 wrongly: `-origcombat` gives a missile that 1.3 never had (fixup G1 item 5) | |
| 6 | Color Spray | 0x8D6D4 | 1d6 missiles, sleep, blind or stun by level; the fixup changes only the sound | case 6 | same | |
| 7 | Confusion | 0x8DB82 | flags 2 (a missile), 1d4 + L missiles, save, L + 2 minutes | case 7 | same | |
| 9 | Continual Light (Mage) | 0x8DC1D | flags 3: a missile, AREA of radius 3 where it lands, light 60.0, class mage | case 9 | fixup (the table text casts it around the caster) | |
| 12 | Detect Magic | 0x8E019 | class mage, 2L minutes | case 12 | same | |
| 13 | Dispel Magic | 0x8E0B5 | flags 0x803: a missile, AREA of radius 60, class mage | case 13 | fixup (the table text casts it on the caster) | same |
| 14 | Fear | 0x8E2D8 | flags 0x103: AREA of radius 90 around the caster; save, L minutes | case 14 | fixup (the table text fires one missile) | 2e: a cone |
| 15 | Feather Fall | 0x8E464 | flags 0x103: AREA of radius 20 around the caster, L minutes | case 15 | fixup | |
| 16 | Fireball | 0x8E55C | flags 0x803: AREA of radius 40; fire d6, the case writes min(L, 10) dice: Ld6 at most 10d6, save half | case 16 | fixup (the table text: 0 sides + 6 per level, 6L; test L5 mean 24.0) | same as 2e |
| 19 | Fly | 0x8E665 | 10 (1d6 + L) minutes | case 19 | same | same |
| 21 | Haste | 0x8E79C | flags 0x103: AREA of radius 80 around the caster; L + 3 minutes | case 21 | fixup (`Dtu_fix_haste_attack` earlier) | 2e: an area |
| 22 | Hold Person | 0x8E85A | flags 3: AREA of radius 40 where it lands, class mage; humanoid, save, 2L minutes | case 22 | fixup | |
| 23 | Hold Undead | 0x8EA21 | flags 3: AREA of radius 80; undead of level L or lower | case 23 | fixup | |
| 24 | Improved Invisibility | 0x8EB53 | range and radius 3, L + 4 minutes | case 24 | same | |
| 25 | Infravision | 0x8EBE8 | (L + 2) hours in 32 bits: wraps from L 8, the effect ends at once | case 25, `long_duration` | fixed, 24fa78a (`Dtu_fix_spell_hours`): at most 4 h 33 min. The port also read L 3 to 7 as no end | 2 h + 1 h/level |
| 26 | Invisibility | 0x8ECB4 | range and radius 3, until an attack | case 26 | same | |
| 27 | Jump | 0x8ED20 | 1d3 + L minutes | case 27 | same | |
| 28 | Knock | 0x8FFE5 | flags 2: a missile; the wall hit 0x26C2F opens the door (unlock 1) | `spell_self`, dtuobj.c wall hit | fixup (the table text has `self_cast_ok`: nothing happens). The layer must clear the cast on the caster | same |
| 29 | Levitate | 0x8EDB2 | 10L minutes, no cast while levitating | case 29 | same | |
| 30 | Light (Mage) | 0x8DC1D | flags 3: a missile, AREA of radius 3 where it lands, light 20.0; blind 10L minutes | case 30 | fixup | 2e: blinds when cast at the eyes |
| 32 | Lightning Bolt | 0x8EE76 | flags 0x802, radius 5; electric d6, min(L, 10) dice: Ld6 at most 10d6, save half; missile life 2 s | case 32 | fixup (table text: 6L; test L5 mean 27.0) | same as 2e |
| 33 | Magic Missile | 0x8EED5 | dice 4 sides, 1 die, +1 per level, rolled at level 1: 1d4 + 1; save half (0x911AA); (L >> 1) + 1 missiles, at most 5 | case 33 | fixup (table text: 1d4) | no save since 2026-11-24 (`Dtu_fix_damage_save`, 2e and the manual; `-origcombat`: the save for half of 1.3). Kept: 2e gives one more missile at 3, 5, 7, 9 |
| 36 | Melf's Acid Arrows | 0x8EFE0 | acid d4, the case writes 2 (L/3) + 2 dice: one hit, save half | case 36 | fixed earlier (`Dtu_fix_melf_acid`, the 2e rule). The 1.3 rule of spells.md section 10 ("4L") is the table text; corrected in section 11 | 2e: same total dice, over rounds |
| 37 | Melf's Minute Meteors | 0x8F021 | flags 0x802, radius 6; fire 1d4 per meteor, min(L, 5) meteors, save half | case 37 | fixup (table text: 1d4 normal damage) | since 2026-11-24 (`Dtu_fix_damage_save`, 2e): no save, each meteor needs an attack roll with the THAC0 of the caster, +2 to hit, no penalty for range, against the armor class against missiles; `-origcombat`: the save for half of 1.3. Kept: 2e one meteor per level |
| 39 | Protection From Evil | 0x8F116 | class 0x26, range and radius 3, self; 2L minutes | case 39 | same; class 0x26 is in the fixup layer | |
| 40 | Protection From Fire | 0x8F1B2 | class 0x27 (has the mage bit) | priest audit | fixup: whether a mage can learn it was not checked | |
| 41 | Rary's Mnemonic Enhancer | 0x8F21D | one cast, slots until Dispel Magic | case 41 | fixed earlier (`Dtu_fix_rary_manual`); not run by `spelltest` (choice box) | |
| 44 | Scare | 0x8F467 | flags 3: AREA of radius 30 where it lands; level 6 or lower | case 44 | fixup | kept: the manual says level 6 or more is safe |
| 45 | Shield | 0x8F6AE | 5L minutes, AC 3, stops Magic Missile | case 45 | fixed (`Dtu_fix_shield_ac`, -origcombat gives AC 3): AC 4 against melee, 2 against missiles (the player, a creature with Shield, the Amulet of Shield, the Ring of Armoring 3 and 1), stops Magic Missile | decision of the user, the 2e spell text: AC 4 against melee weapons, 2 against missiles, immune to Magic Missile |
| 46 | Sleep | 0x8F766 | flags 3: AREA of radius 60 where it lands; level 3 or lower, pool 4 to 16 | case 46 | fixup | 2e: 2d4 HD in an area |
| 47 | Slow | 0x8FAD8 | flags 3: AREA of radius 80 where it lands; save, L + 3 minutes | case 47 | fixup | 2e: an area |
| 48 | Spider Climb | 0x8FBEE | L + 3 minutes | case 48 | same | |
| 49 | Stone Skin | 0x8FC7A | 1d4 + L/2 hits | case 49 | same (`Dtu_fix_stoneskin_word` earlier) | same |
| 50 | Strength | 0x8FCFA | L hours, L * 225 << 20 in 32 bits: wraps from L 10 | case 50, `long_duration` | fixed, 24fa78a (`Dtu_fix_spell_hours`): at most 4 h 33 min. The port also read L 5 to 9 as no end | 1 h/level |

Not wizard spells after the fixup: 17 (Produce Fire, cleric level 4, fire 1d4
+ L in an AREA of radius 24) and 51 (Find Traps, cleric level 2). Both are for
the priest audit.

## Rules shared by the wizard spells

| Rule | 1.3 | Port | Status |
|---|---|---|---|
| Slots | 0x3575B: the mage row of 0x145CD8 by mage level, no intelligence bonus | `dtu_spell_compute_slots` | same (table checked byte by byte) |
| Rest | 0x3551B fills all slots (call 0x3608B) | `dtu_spell_restore_slots` | same |
| Fill at the game start | game() 0x42284 calls 0x3551B only after Waterdeep: flag 0x141C38 set by 0x8C206, cleared by the load 0xBBA8A and by game() 0x4228B; level byte P+0x71 <= 1 | `dtu_spell_fill_at_start`, `dtu_spell_frame` | fixed: the port filled all slots at each level change and after each load (a free rest). Test `tools/check_saveload.py`. spells.md section 13 |
| New character | 0x8BEBC calls 0x3551B (0x8BFFE) | `make_player` (dtufe_char.c) | fixed: the port did not fill; the level start did |
| Level change, load | slots left stay; the load reads P+0x2C and P+0x3C; a warp ends the cast (0x16413) | `dtu_spell_level_start` | fixed (same change) |
| Cast time | words of 0x13A8DB in seconds (the fixup writes them); a scroll or a wand casts at once | `Cast_time_13` | same (table checked) |
| Level limit | 0x3598C, 0x359C0: "Spell is of too high a level.", then "Out of spell points ..." | `cast_allowed` | same |
| Armor | 0x41A4D: a mage spell in armor without `MAGIC_OK` (item flags 0x19FD34 bit 1) is refused, sound 3000 | `cast_allowed` | same |
| Climbing, Levitate | player +0x1AE bit 0x80: refused | `cast_allowed` | same |
| Interruption | a hit during the cast time ends the cast, the slot is used | `dtu_spell_interrupt` | same |
| Multi-class | 0x91EC1: a mage class casts at the mage level; a scroll or a wand at the level of the class | `dtu_spell_player_level` | same |
| Learning | 0xB65BB, 0xB69AD: a spell with the mage class bit goes to the list of a mage at once, no intelligence roll | `dtu_spell_insert` | same; the class bits after the fixup come with the layer. Kept: 2e has a chance to learn |
| Scrolls and wands | any class casts them, at the level above | as 1.3 | kept |

Spells with no placement in a level and not given to a new mage (`lvl.py`
over the 31 levels; frontend.md 0x8BB91): 19, 21, 22, 25, 26, 41, 44, 47, 50.
Scripts and shops that may give them were not traced.

## Not tested

All rules marked "fixup": the runs used the table text. The missile flight
and homing (the test aid calls 0x8CFFF directly).
