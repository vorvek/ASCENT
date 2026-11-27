# Audit: all spells, with their visuals

Date: 2026-11-24. Scope: the 56 spell lines of 1.3 (wizard and priest, every level),
the scrolls (a spell line as an item) and the wands and the rod that cast a spell, in
the port and in 1.3, against AD&D 2nd edition, with the visual of each spell. Fireball
(16) and Burning Hands (4) are fixed at the same time by another agent: they are in
the table, but no change is proposed here that touches their cases.

This audit adds to `spells-wizard.md` and `spells-priest.md` (the rules of the cases,
the slots, Turn Undead). It does not repeat their checks. New here: the table after the
fixup as the game reads it, the visuals, the description texts, the counters of
Haste, Slow and Hold Undead, the Lightning Bolt wall rule, the wands.

## Method and evidence

Each fact is marked V (verified, the evidence is named) or G (a guess or not traced).

- Table: the `$SPELL`, `$VCLIP` and `$WEAPON` lines of `YPINN.BIN` (`E:\UNDAMONTAN\DTU`)
  after the table fixup of the port (`tools/wiki_rules.py` with `tools/tblfix.py`,
  which reads the writes of `engine/main/dtutblfix.c`). V: the 39 mission tables of
  `gamedata/unpacked` (BANE to YPINN) have the same 56 spell lines (one hash) and the
  same 51 vclips that the spells name. The older tables BASE, DEMO, MOIN1, MINES1,
  PRISON1, SPIDER and TRAIN1 lack most of these vclips (81 to 180); whether a player
  can reach them was not checked (G).
- 1.3: the pseudocode of `D:\dev\DTU-Decomp\local\decompiled\functions`, and the bytes
  of `local\dtu13.elf` (objdump) where marked "bytes". The rules of the cases are those
  of `DTU-Decomp\docs\spells.md` section 5 and of the two earlier audits.
- Port: `engine/main/dtuspell.c` (D = this file below), `dtuobj_missile.c`,
  `dtudesc.c`, `dtutblfix.c`, at commit 08c5737.
- The texts the player sees: `tools/check_desc.py --only en-high --dump` with
  `build-rel/engine/main/ascent.exe` (built 2026-11-24 20:16), caster level 1,
  56 spells, 0 failed, rule check 0 problems. The texts are in the appendix.
- A game run (YPINN, `-chartest` fire of the Wand of Fireballs, of Confusion and of
  Magic Missiles, `-objlog`) for the missile sizes.
- AD&D 2e: `https://vodabois.fi/2eSpells/` is a script page; its data are
  `assets/WizardSpells.json` and `assets/PriestSpells.json` (fetched 2026-11-24). The
  2e column summarizes the fields range, duration, area, save and the description.
  Priest Dispel Magic is not on the site; the wizard spell is used.

Units: one round of 2e is one game minute (60.0 game seconds) in both games. A
distance is in game units; the description window shows units / 2 as feet. The
missile range is the speed times the missile life, not the `range=` word: 0x56FF6
writes the life 30.0 (Lightning Bolt 2.0, Melf's Acid Arrows (L/3 + 1) minutes) and
uses `range=` only in a debug text (V, pseudocode of 0x56FF6 after LAB_00057474;
port D:2935 `spell_missile_life`). So every missile spell has a range of 3000 units
(100.0 x 30.0), shown as 1500 ft.

L = caster level, T = target level. "Save" is the save against spells
(`dtu_spell_save(target, DTU_SAVE_SPELL)`, 1.3 0xAE596).

## Findings shared by many spells

These are the main results. The spell blocks below refer to them by number.

### Visuals

**S1. The cast visual of a spell cast on the caster is missing (port bug).** V:
0x8CAD3 (`spell_play_cast_effects`) creates the `cast_vclip` of the line, or the
`weapon_vclip` when `cast_vclip` is -1, as a vclip object fixed to the caster, and
a light object with `player_light` when that is not 0. 0x56A0D calls it after each
cast on the caster (pseudocode line 171), 0x91FDC after each creature cast on
itself, 0x37A6A for each item that casts a spell, and 0x8CFFF in the Light case. No
spell line has `cast_vclip` (table dump), so in 1.3 the visual is always the
`weapon_vclip`. The port plays only `cast_vclip`: D:2971 (`cast_now`), D:3660
(`creature_cast`), D:3765 (`dtu_spell_cast_vclip`, used by `dtuchar.c:1938` for
potions and items and by `dtucombat.c:2566, 2590`). So no spell cast on the caster,
no potion and no item spell shows a visual, and none gives the cast light. The
art exists for each of them (vclips 82 to 126, list below). The lifetime of the
light object was not read (G).

**S2. Spell missiles drawn as a vclip are 1/16 unit wide (port bug).** V: 0x56FF6
writes `weapon_vclip_size` of the spell (default 1.0 from the reader; Fireball 3.0,
Produce Fire 3.2) into `blob_size` of the missile template. The port takes
`blob_size` of the template weapon (`dtuobj_missile.c:119`): weapon 2 (hands) has
the reader default 0x1000 (`bmread.c:2133`), weapons 3 (wand) and 4 (scroll) have
`blob_size=0.0`, and `missile_look` raises it to 1/16. Game run: the Fireball and
the Confusion of a wand were created with "render 7 size 0.06". In 1.3 the sizes are
3.0 and 1.0. This hits every spell missile drawn as a vclip: 6, 7, 8, 9, 13, 16, 17,
22, 23, 30, 31, 34, 37, 44, 46, 47, 52, 55. A missile that is a model (0, 4, 5,
32, 33, 36) has the size of the model and is right.

**S3. Spell missiles give no light (port bug).** V: 0x56FF6 writes `player_light`
of the spell (8.0 for most lines, 20.0 for Light, 60.0 for Continual Light, 0 for
Dispel Magic) into `lightcast` of the template, and 0x577FE adds `lightcast` of
each weapon object to the light of the mine. The port lights a weapon object with
`Weapon_info[template].light` (`lighting.c:297`): 0 for the hands, 1.0 for the wand
and the scroll.

**S4. The trail of a missile is missing (port bug).** V: 0x5656C creates a
`vclip_trail` vclip behind each weapon object in each frame when the value is above
0, and 0x56FF6 copies it from the spell. The port parses no `vclip_trail` and draws
no trail (no use in `engine/main` outside the parser). Spells with a trail:
Abjure 99 (abjtr.abm), Fireball 135 (smoke1.abm), Produce Fire 42 (diss3.abm),
Melf's Acid Arrows 129 (asmok.abm), Melf's Minute Meteors 135 (smoke1.abm).

**S5. A spell missile that is a model shows the wrong wall hit (port bug).** V:
0x26C2F, for a missile with a spell, plays the `wall_hit_vclip` and the
`wall_hit_sound` of the spell line (pseudocode lines 98 to 103; only for a model,
`+0xC == 1`, the same test as the port). The port plays those of the template
weapon (`dtuobj_missile.c:313-314`), which has none. So the wall hit of Magic
Missile (101 mmring.abm) and of Melf's Acid Arrows (116 acdxpld.abm) is not shown.

**S6. The vclip that follows a held, feared or confused target is missing (port
bug, known open item of `spells-priest.md`).** V: the common end of 0x8CFFF calls
0x8CCED when `cast_on_top` is 0 or more (pseudocode line 1721); 0x8CCED creates that
vclip 8 units above the target, fixed to it, until GameTime plus the duration. Lines:
Confusion 81 (confus.abm), Fear 130 (fear.abm), Hold Person 22 and 52 133 (holdp.abm),
Hold Undead 131 (holdun.abm), Scare 24 (level1.abm). The port has no such call.

**S7. Color Spray always shows the same missile (port bug).** V: 0x56FF6 sets the
missile vclip to 175 + rand() % 6 (ospray, rspray, gspray, pspray, yspray and
level4.abm) for each cast (pseudocode, branch of spell 6). The port takes the
`weapon_vclip` 180 (level4.abm) of the line every time (`missile_look`).

**S8. The light spells give the target no light (known open item).** V: case
0x8DC1D adds `player_light` to the light value of the target (+0x1A6) and calls
0x8CAD3 (pseudocode line 480). The port blinds and makes the light object where the
missile lands (D:2584), but adds no light to the target and shows no cast effect.

**S9. The spread of several missiles (G).** 0x56EB0 fires one missile, or each of
the count with 0x564F7 (`laser_player_fire_spread`), a fan of side offsets; the
offsets were not read in the bytes. The port turns each missile by a random vector
of 1/8 (D:3005). Spells: Burning Hands (6), Color Spray (1d6), Confusion (1d4 + L),
Magical Stone (1d3), Minute Meteors (up to 5). Magic Missile homes in both.

Art of the spells (`$VCLIP`, all present in the mission tables): 14 firbal1, 16
Pebble, 19 spray-hi, 24 level1, 39 missile1, 42 diss3, 81 confus, 82 jump, 83 detmag,
84 knock, 85 aid, 86 bless, 87 clwound, 89 prayer, 90 mmmet, 91 cswound, 92 flmwlk,
93 invis, 94 mgvest, 95 proevil, 96 protfire, 97 rescold, 98 resfire, 99 abjtr, 101
mmring, 103 light, 108 brnhnds, 109 mmexpld, 111 fbxpld, 116 acdxpld, 117 spidcl, 118
fethfal, 119 infrav, 120 levit, 121 stone, 122 armor, 123 conivis, 124 fly, 126 streng,
129 asmok, 130 fear, 131 holdun, 132 sleep, 133 holdp, 135 smoke1, 175 to 179 the five
sprays, 180 level4. Models: abjur, burn, chill2, ltning, miss2, acid3, ice.

### Rules

**R1. Lightning Bolt bounces and does not stop at a wall (port bug).** V (bytes):
0x571C5 compares the spell with 0x20 and sets the life 2.0 and the bounce byte 1 of
the template; 0x26DC3 to 0x26DD2: for spell 0x20 the wall hit writes -1 to +0x144 and
jumps to 0x26FAF, after the call that removes the missile. So the bolt goes on for
its 2.0 seconds and bounces (the 2e bolt bounces too). In 0x272DD a bouncing weapon
turns back after a creature hit (LAB_00027763), so it can hit again (G: the physics
flag that the bounce byte sets was not traced). The port removes the bolt at the
first wall (`dtuobj_missile.c:320`) and at the first body (`:423`), so it hits one
target. Later (2026-11-25, docs/re/spells.md section 14): the bounce is ported; the
bytes show that 0x272DD turns back only a missile with flag 0x80 and removes the bolt
(a model) at the first body. Later (2026-11-27, D-195): `Dtu_fix_bolt_pass` lets the
bolt go on through the bodies, by the 2e rule.

**R2. The counters of Haste, Slow and Hold Undead are missing (port bug).** V:
0x56A0D stores the caster level in the counter `S+0x49` of Haste before the cast on
the caster (pseudocode line 148, record `+0x4B`); 0x56FF6 sets it to L for Slow and
to rand() % 3 + 1 for Hold Undead (pseudocode, branches 0x2F and 0x17). The cases
need it above 0 and take one for each target: Haste 0x8E79C (line 841), Hold Undead
0x8EA21 (line 932), Slow 0x8FAD8 (line 1527). So Haste and Slow act on at most L
creatures and Hold Undead on at most 1d3. The port has the counters of Hold Person
and Sleep only (D:2872-2887); its Haste, Slow and Hold Undead act on each creature in
the area (radius 80). This is the 2e rule too (1 creature per level, 1d3 undead).

**R3. Haste and Fear hit every creature in the area (1.3, kept).** V: 0x90CEF has no
test of friend or foe (docs/spells.md section 6). Haste (radius 80 around the
caster) can haste hostile creatures; with R2 at most L of them, in object order
after the player. 2e lets the caster choose. Under the fidelity rule this is a
candidate 1.3 defect (see the fix list, low priority).

**R4. Confusion on the player (G, not traced).** Case 0x8DB82 sets the player
`script_flags` to 0x3FFFFFFF and `type_flags |= 0x10` (pseudocode lines 442-444). The
port sets only the bit, and no code outside `dtuspell.c` reads
`DTU_FX_CONFUSION`, so a confused creature or player acts as before. What 1.3 does
with bit 5 in the creature AI was not traced.

## The spells

Each block: the port (file:line), 1.3 (address), 2e, the visual, the verdict. "Text"
is in the appendix. Class and level are after the fixup.

### 0 Abjure (cleric 4)
- Port: missile `abjur.pof`, 1 missile, no area. Extraplanar creatures only: d20 >= 11
  - L + T, then d100 >= 70 + 2T kills, else the creature leaves (D:2217). No save.
  A creature caster deals 2d8 + 3L (as level 1) instead. Immune: all that are not
  extraplanar. V (code).
- 1.3: 0x8D297, the same rules (`spells-priest.md`). Range 120 in the line is not used.
- 2e: range 10 yds, one extraplanar creature, 50 % (11 or better on d20) adjusted by
  level, the creature goes home; the 70 % + 2 %/HD rule is for creatures with no
  Constitution.
- Visual: model abjur.pof, trail 99 missing (S4). The spin of the creature (movement
  type 3) is missing (`spells-priest.md`, cosmetic).
- Verdict: correct; missing visual (S4).

### 1 Aid (cleric 2)
- Port: self, 1d8 hit points above the maximum and Bless for (L + 1) minutes (D:2059,
  1746, 1796). V.
- 1.3: 0x8D3F7, same. 2e: touch, Bless and 1d8 temporary hit points, 1 round + 1/level.
- Visual: 1.3 vclip 85 (aid.abm) on the caster and a light 8.0 (S1). Port: none.
- Verdict: correct rules; missing visual (S1).

### 2 Armor (mage 1)
- Port: self, bit 0, pool 8 + L of weapon damage, AC 6 at most (D:2079, 1797). V.
- 1.3: 0x8D4E7, same. 2e: touch, AC 6, ends after 8 + 1/level points of damage. Same.
- Visual: 1.3 vclip 122 (armor.abm) and light (S1). Port: none.
- Verdict: correct; missing visual (S1).

### 3 Bless (cleric 1)
- Port: self, 360 s, returns when bit 1 is set (D:2094, 1747). V.
- 1.3: 0x8D58E, self (flags 0x542). 2e: 60 yds, 50 ft cube of allies, 6 rounds, +1 to
  hit and morale.
- Visual: 1.3 vclip 86 (bless.abm) (S1). Port: none.
- Verdict: correct for 1.3 (2e area kept as 1.3: the line is a self cast); missing
  visual (S1).

### 4 Burning Hands (mage 1), owned by the other agent
- Port: AREA radius 10 where each of 6 missiles lands, model burn.pof, fire 1d3 + 2L,
  save half, the caster spared (D:2098, 2888, rule D:1852). Text: "Range: 1500 ft,
  Area: 5 ft where it lands, Missiles: 6, Deals 1d3+2 fire damage."
- 1.3: 0x8D614, 6 missiles, fire 1d3 + 2L, save half; a missile life of 30 s.
- 2e: range 0, a 5 ft arc of 120 degrees in front, 1d3 + 2/level, at most 1d3 + 20,
  save half.
- Visual: model burn.pof (the line also names vclip 108 brnhnds.abm; the last word of
  the line wins, here the model, V: tblfix keeps both, the port prefers the model).
- Verdict: left to the other agent. Note for it: 2e caps the bonus at +20.

### 5 Chill Touch (mage 1)
- Port: self, bit 3 for (L + 3) minutes (D:2105, 1748); the melee hit adds 1d4 with a
  save, undead get Fear (dtucombat.c:550). V.
- 1.3: 0x8D62F, the fixup makes it a self cast. 2e: the caster's touch, 3 rounds +
  1/level, save negates 1d4 and 1 Strength; undead flee 1d4 + L rounds.
- Visual: line has `weapon_pof=chill2.pof` and no `weapon_vclip`: 0x8CAD3 shows
  nothing (`weapon_vclip` -1) (V, table). Port: none. Same.
- Verdict: correct. Kept: no Strength loss (1.3 has none).

### 6 Color Spray (mage 1)
- Port: 1d6 missiles, AREA radius 40 where each lands, the caster spared. T <= L:
  sleep 2 to 8 minutes (the player is blinded too); T <= L + 2: blind 1d4 minutes;
  else stun (Hold) 1 minute cut by wisdom. Save only for T > min(L, 5). Immune:
  blind creatures, already blind targets (D:2109, 1733, 1924). V.
- 1.3: 0x8D6D4 (`spells-wizard.md`: same). 2e: a 5 x 20 x 20 ft wedge, 1d6
  creatures by distance, the same three bands, save for those above the caster level
  or of 6 HD or more.
- Visual: 1.3 missile vclip 175 + rand() % 6, monster hit vclip 19 (spray-hi.abm).
  Port: always 180 (S7), 1/16 size (S2).
- Verdict: correct rules; port bug in the visual (S2, S7).

### 7 Confusion (mage 4)
- Port: 1d4 + L missiles, one target each, save negates, (L + 2) minutes, no other
  effect (D:2132, 1749, 2890). V.
- 1.3: 0x8DB82, the same, and on the player `script_flags` and `type_flags` (R4).
- 2e: 120 yds, 1d4 + 1/level creatures in a 60 ft cube, save at -2, 2 + 1/level
  rounds, a d10 table of actions each round.
- Visual: 1.3 missile vclip 81 (confus.abm), hit vclip 81, `cast_on_top` 81 on the
  target (S6). Port: 1/16 size (S2), no vclip on the target (S6).
- Verdict: port bug (S2, S6); R4 open (the effect of the bit was not traced).

### 8 Continual Light (Cleric) (cleric 3), 9 Continual Light (Mage) (mage 2)
- Port: missile, AREA radius 3 where it lands, a light object of 60.0 with no end;
  each target in the area (the caster spared) saves or is blinded with no end; 1d6
  to a light sensitive creature that fails (D:2136, 2584, 1750). Immune: blind
  creatures. V.
- 1.3: 0x8DC1D and 0x90CEF, the same, plus the target light and 0x8CAD3 (S8).
- 2e: 60 or 120 yds, a 60 ft globe of light, permanent; cast at the eyes it blinds
  (save: the light is behind the target); no damage.
- Visual: missile vclip 39 (missile1.abm) at 1/16 (S2), no missile light (S3), no
  light on the target (S8). The light object where it lands is right.
- Verdict: correct rules; port bug in the visual (S2, S3, S8).

### 10 Cure Light Wounds (cleric 1), 11 Cure Serious Wounds (cleric 4)
- Port: self; 1d8, or 3 + rand() % 15 (3 to 17); not undead or extraplanar (D:2161,
  1798-1799). `Dtu_fix_cure_aid` keeps the Aid points. V.
- 1.3: 0x8DE4B, 0x8DF37. 2e: touch, 1d8; 2d8 + 1.
- Visual: 1.3 vclip 87 (clwound.abm) or 91 (cswound.abm) on the caster (S1). Port:
  none.
- Verdict: correct (the flat 3 to 17 is 1.3, kept); missing visual (S1).

### 12 Detect Magic (mage 1), 54 Detect Magic (cleric 1)
- Port: self, bit 6 for 2L minutes (12) or 10 minutes (54) (D:2171, 1753-1754). The
  renderer tints magic objects (`dtu_magic_look`). V.
- 1.3: 0x8E019. 2e: 10 x 60 ft path, 2 rounds/level (wizard); 1 turn (priest).
- Visual: 1.3 vclip 83 (detmag.abm) on the caster (S1). Port: none.
- Verdict: correct; missing visual (S1).

### 13 Dispel Magic (mage 3), 55 Dispel Magic (cleric 3)
- Port: missile, AREA radius 60 where it lands, any object type (`hit_objects`), each
  target loses all effects, the player also the potion timers; no save; the caster
  too when in the area (D:2177, 2604). Objects: `object_spell` D:1510. V.
- 1.3: 0x8E0B5, flags 0x803. 2e: 120 yds, 30 ft cube, d20 check per effect against
  the level of its caster.
- Visual: missile vclip 176 (rspray.abm, set by the fixup) at 1/16 (S2); line light 0.
- Verdict: correct rules (1.3 kept: no level check); port bug in the visual (S2).

### 14 Fear (mage 4)
- Port: AREA radius 90 around the caster, the caster spared, undead and `no_scare`
  immune, save negates, L minutes cut by wisdom; weapon drop on d100 <= 65 - 5T; the
  player also gets the gated view and Hold (D:2182, 1755). V.
- 1.3: 0x8E2D8, flags 0x103. 2e: a 60 ft cone, save negates, drop 60 % at 1 HD - 5 %
  per HD.
- Visual: 1.3 vclip 103 (light.abm) on the caster (S1), `cast_on_top` 130 (fear.abm)
  on each target (S6). Port: none of them.
- Verdict: correct rules (area around the caster kept, 1.3); missing visual (S1, S6).

### 15 Feather Fall (mage 1)
- Port: AREA radius 20 around the caster, L minutes (D:2272, 1757). V.
- 1.3: 0x8E464. 2e: a 10 ft cube, 1 round/level.
- Visual: 1.3 vclip 118 (fethfal.abm) on the caster (S1). Port: none.
- Verdict: correct; missing visual (S1).

### 16 Fireball (mage 3), owned by the other agent
- Port: missile vclip 14, AREA radius 40, any object type, fire min(L, 10)d6, save
  half, the caster too; blind and deaf within 6.0 (D:2276, 1822, 2636).
- 1.3: 0x8E55C, the same (`spells-wizard.md`). 2e: 20 ft radius, Ld6 at most 10d6,
  save half.
- Visual: 1.3 missile vclip 14 (firbal1.abm) at size 3.0, trail 135, light 8.0. Port:
  size 1/16 (S2, seen in the game run), no trail (S4), no light (S3). The wall hit of a
  vclip missile shows no vclip in 1.3 either (0x26C2F tests for a model); where 1.3
  draws the explosion 111 (fbxpld.abm) of an area hit was not traced (G).
- Verdict: left to the other agent; S2, S3, S4 are in `dtuobj_missile.c` (fix group A).

### 17 Produce Fire (cleric 4)
- Port: missile vclip 39, AREA radius 24 where it lands, any object type, fire 1d4 +
  L, save half, the caster spared (D:2098, rule D:1865). V.
- 1.3: 0x8E5B1 (renamed Flaming Sphere, fixup). 2e: 40 yds, a 12 ft square of fire,
  1d4 + 1/level, 1 round.
- Visual: missile vclip 39 at size 3.2 in 1.3; port 1/16 (S2), no trail 42 (S4), no
  light (S3).
- Verdict: correct rules; port bug in the visual (S2, S3, S4).

### 18 Flame Walk (cleric 3), 43 Resist Fire (cleric 2)
- Port: self, bit 10 for (L + 1) or L minutes: half fire damage (D:2284, 2462). V.
- 1.3: 0x8E5D1, 0x8F3CD. 2e: Flame Walk: immune to normal fire, +2 saves and half
  damage from magical fire, 1 + 1/level rounds; Resist Fire: +3 saves, half damage,
  1 round/level.
- Visual: 1.3 vclip 92 (flmwlk.abm) or 98 (resfire.abm) (S1). Port: none.
- Verdict: correct; missing visual (S1).

### 19 Fly (mage 3)
- Port: self, 10 (1d6 + L) minutes (D:2288, 1759). V.
- 1.3: 0x8E665. 2e: touch, 1 turn/level + 1d6 turns. Same.
- Visual: 1.3 vclip 124 (fly.abm) (S1). Port: none.
- Verdict: correct; missing visual (S1).

### 20 Free Action (cleric 4)
- Port: self only, ends Hold, 10L minutes (D:2292, 1760). V.
- 1.3: 0x8E6F2. 2e: touch, 1 turn/level.
- Visual: 1.3 vclip 24 (level1.abm) (S1). Port: none.
- Verdict: correct; missing visual (S1).

### 21 Haste (mage 3)
- Port: AREA radius 80 around the caster, each player or creature in it (caster
  included), ends Slow, (L + 3) minutes, no counter (D:2302, 1761). V.
- 1.3: 0x8E79C: returns when hasted; needs the counter (L from 0x56A0D), one per
  target (R2).
- 2e: 60 yds, 40 ft cube, 1 creature per level, 3 + 1/level rounds.
- Visual: 1.3 vclip 103 (light.abm) on the caster (S1). Port: none.
- Verdict: port bug (R2: at most L targets); missing visual (S1). R3 (enemies hasted)
  is a 1.3 rule.

### 22 Hold Person (mage 3), 52 Hold Person (cleric 2)
- Port: missile, AREA radius 40 where it lands, the caster spared, humanoid creatures
  or the player, not undead, Free Action immune, save negates, at most 1d4 targets
  (counter), 2L minutes cut by wisdom (D:2308, 2878, 1762). V.
- 1.3: 0x8E85A, the same (`spells-priest.md`). 2e: 1 to 4 humans or humanoids in a 20
  ft cube, 2 rounds/level, save with -2 for a single target.
- Visual: missile vclip 24 at 1/16 (S2); 1.3 `cast_on_top` 133 (holdp.abm) on each
  held target (S6). Port: none.
- Verdict: correct rules; port bug in the visual (S2, S6). Open from the priest audit:
  the stand animation and the input flush of a held player.

### 23 Hold Undead (mage 3)
- Port: missile, AREA radius 80, undead creatures of level L or less, save only for T
  > 2, (1d4 + L) minutes, no counter (D:2330, 1763, 1928). V.
- 1.3: 0x8EA21: also returns when the target has Hold or Free Action (0x5000), counter
  rand() % 3 + 1 (R2). The level budget at `S+0x76` is lowered by T for each hold
  but never tested (V, pseudocode line 936): only T <= L counts.
- 2e: 60 ft, 1d3 undead whose total HD is at most the caster level, mindless undead
  get no save.
- Visual: missile vclip 24 at 1/16 (S2), 1.3 `cast_on_top` 131 (holdun.abm) (S6).
- Verdict: port bug (R2: at most 1d3; and the Free Action test); port bug in the
  visual (S2, S6). The unused budget is a 1.3 defect (the 2e total HD rule); low
  priority.

### 24 Improved Invisibility (mage 4), 26 Invisibility (mage 2)
- Port: self; (L + 4) minutes, or until an attack or a cast (D:2341, 2356, 2991). V.
- 1.3: 0x8EB53, 0x8ECB4 (one timer, they end together). 2e: touch; 4 + 1/level rounds;
  until an attack.
- Visual: 1.3 vclip 93 (invis.abm) or 123 (conivis.abm) (S1). Port: none.
- Verdict: correct; missing visual (S1).

### 25 Infravision (mage 3)
- Port: self, a human player only, not a creature with infravision; (L + 2) hours,
  at most 4 h 33 min (`Dtu_fix_spell_hours`) (D:2345, 1765). V (1.3 0x8EBE8 tests race
  0 and Robot_info flag 0x80, pseudocode lines 965-984).
- 2e: touch, 60 ft in the dark, 2 h + 1 h/level.
- Visual: 1.3 vclip 119 (infrav.abm) (S1). Port: none.
- Verdict: correct; missing visual (S1).

### 27 Jump (mage 1), 29 Levitate (mage 2), 48 Spider Climb (mage 1)
- Port: self; (1d3 + L), 10L, (L + 3) minutes (D:2362, 2369, 2511). Levitate stops
  casting. V.
- 1.3: 0x8ED20, 0x8EDB2, 0x8FBEE. 2e: touch; 1d3 + 1/level rounds; 1 turn/level;
  3 + 1/level rounds.
- Visual: 1.3 vclip 82 (jump.abm), 120 (levit.abm), 117 (spidcl.abm) (S1). Port: none.
- Verdict: correct; missing visual (S1).

### 28 Knock (mage 2)
- Port: missile vclip 84, no case; the wall hit opens a locked door
  (`dtuobj_missile.c:281`). V.
- 1.3: 0x26C2F calls the door function for spell 0x1C (pseudocode line 51). 2e: 60 yds,
  opens a door of 10 sq ft per level.
- Visual: missile vclip 84 (knock.abm) at 1/16 (S2).
- Verdict: correct rule; port bug in the visual (S2).

### 30 Light (Mage) (mage 1), 31 Light (Cleric) (cleric 1)
- Port: as 8 and 9 with a light of 20.0 for 10L minutes (30) or 10 (L + 6) minutes
  (31), and the same blindness time (D:2136, 1751-1752). V.
- 1.3: 0x8DC1D. 2e: a 20 ft globe; wizard 1 turn/level, priest 1 hour + 1 turn/level;
  cast at the eyes it blinds.
- Visual: as 8 (S2, S3, S8).
- Verdict: correct rules; port bug in the visual.

### 32 Lightning Bolt (mage 3)
- Port: model ltning.pof, life 2.0 s, not an area, electric min(L, 10)d6, save half;
  the missile ends at the first wall or body (D:2373, 1823, 2937). V.
- 1.3: 0x8EE76, the same dice; the bolt bounces and goes on at a wall (R1).
- 2e: 40 yds + 10/level, a 40 or 80 ft bolt that bounces, Ld6 at most 10d6, save half.
- Visual: model, no wall vclip in the line. Text "Area: One target" (with R1 it can
  hit more than one).
- Verdict: port bug (R1). The description needs a line when R1 is fixed.

### 33 Magic Missile (mage 1)
- Port: model miss2.pof, min(L/2 + 1, 5) missiles, homing for a mage, 1d4 + 1, no
  save (`Dtu_fix_damage_save`), Shield stops it (D:2381, 2891, 2929). V.
- 1.3: 0x8EED5, save for half (the setting gives it back). 2e: 1d4 + 1, no save, 1 to
  5 missiles (one more at 3, 5, 7, 9).
- Visual: model, hit vclip 101 (mmring.abm) on a creature (D:1201). The wall hit 101
  is missing (S5).
- Verdict: correct (kept: the 1.3 missile count); port bug in the visual (S5).

### 34 Magical Stone (cleric 1)
- Port: 1d3 missiles, vclip 16, an attack roll with the caster THAC0, 1d4, one more
  d4 against undead, no save (D:2391, 2892, 1882). V.
- 1.3: 0x8EEFE, save for half. 2e: 3 stones, thrown, attack roll, 1d4, 2d4 against
  undead.
- Visual: missile vclip 16 (Pebble.abm) at 1/16 (S2); hit vclip 101.
- Verdict: correct; port bug in the visual (S2).

### 35 Magical Vestment (cleric 3)
- Port: self, bit 31 for 5L minutes, AC max(5 - (L - 5)/3, 1) (D:2395, 1769). V.
- 1.3: 0x8EF49 (AC 6 at levels 1 and 2 by integer division, `spells-priest.md`). 2e:
  AC 5, +1 per 3 levels above 5, 5 rounds/level.
- Visual: 1.3 vclip 94 (mgvest.abm) (S1). Port: none.
- Verdict: correct; missing visual (S1).

### 36 Melf's Acid Arrows (mage 2)
- Port: model acid3.pof, `Dtu_fix_melf_acid`: an attack roll (fighter THAC0), 2d4 at
  once and 2d4 in each of the next L/3 rounds, no save (D:2399, 1635). V.
- 1.3: 0x8EFE0: one hit of 2(L/3) + 2 d4, save half; life (L/3 + 1) minutes.
- 2e: 180 yds, one target, attack roll, 2d4 and 2d4 each round for 1 round per 3
  levels.
- Visual: model, hit vclip 116 (acdxpld.abm) on a creature; wall hit 116 missing
  (S5), trail 129 missing (S4). Text "Duration: Instant" while the acid lasts L/3
  rounds (from level 3).
- Verdict: correct (fixed earlier to 2e); port bug in the visual (S4, S5); the text
  needs the duration of the acid.

### 37 Melf's Minute Meteors (mage 3)
- Port: min(L, 5) meteors, vclip 90, an attack roll +2, fire 1d4, no save (D:2101,
  2893, 1885). V.
- 1.3: 0x8F021, save half. 2e: one meteor per level, +2 to hit, 1d4 each.
- Visual: missile vclip 90 (mmmet.abm) at 1/16 (S2), hit vclip 109 (mmexpld.abm),
  trail 135 missing (S4), no light (S3).
- Verdict: correct rules; port bug in the visual (S2, S3, S4).

### 38 Prayer (cleric 3)
- Port: self only, Bless for L minutes (D:2411, 1770). V.
- 1.3: 0x8F078; 0x56A0D writes the flags 0x102 (no area) at each cast (V, pseudocode
  line 151). 2e: 60 ft radius, allies +1 and enemies -1 to hit, damage and saves.
- Visual: 1.3 vclip 89 (prayer.abm) (S1). Port: none.
- Verdict: correct for 1.3 (the radius 120 of the fixup is never used); missing
  visual (S1).

### 39 Protection From Evil (mage 1), 53 Protection From Evil (cleric 1)
- Port: self, bit 23 for 2L or 3L minutes (D:2417, 1771-1772). V.
- 1.3: 0x8F116. 2e: touch, -2 to the attacks of evil creatures, +2 saves; 2 or 3
  rounds/level.
- Visual: 1.3 vclip 95 (proevil.abm) (S1). Port: none.
- Verdict: correct; missing visual (S1).

### 40 Protection From Fire (cleric 3, class mask 0x27)
- Port: self, pool of 12L fire points, no second pool while bit 9 is set (D:2423). V.
- 1.3: 0x8F1B2. 2e: touch, absorbs 12 points per level on the caster.
- Visual: 1.3 vclip 96 (protfire.abm) (S1). Port: none.
- Verdict: correct; missing visual (S1).

### 41 Rary's Mnemonic Enhancer (mage 4)
- Port: self, a choice box: three 1st, one 1st and one 2nd, or one 3rd; ends at the
  rest (`Dtu_fix_rary_manual`) (D:2255). V.
- 1.3: 0x8F21D. 2e: three spell levels, 1 day.
- Visual: 1.3 vclip 24 (S1). Port: none.
- Verdict: correct; missing visual (S1).

### 42 Cure Blindness or Deafness (cleric 3)
- Port: self, ends blindness (timer 0) and deafness (D:2436). V.
- 1.3: 0x8F2FA (renamed by the fixup, `cast_vclip` copied from 13, which is -1). 2e:
  touch, cures blindness or deafness.
- Visual: 1.3 vclip 97 (rescold.abm, the art of the old Resist Cold) (S1). Port: none.
- Verdict: correct; missing visual (S1).

### 44 Scare (mage 2)
- Port: missile, AREA radius 30 where it lands, level 6 or less, not undead or
  extraplanar, save only for an elf, a half-elf or a cleric, (1d4 + L) minutes; drop
  on 1 in 2 (D:2182, 1756, 1892). V.
- 1.3: 0x8F467. 2e: 15 ft radius, fewer than 6 HD, only elves, half-elves and priests
  save, 1d4 + 1/level rounds.
- Visual: missile vclip 24 at 1/16 (S2), `cast_on_top` 24 on each target (S6).
- Verdict: correct rules; port bug in the visual (S2, S6).

### 45 Shield (mage 1)
- Port: self, 5L minutes, AC 4 against melee and 2 against missiles
  (`Dtu_fix_shield_ac`), stops Magic Missile (D:2466, 1774). V.
- 1.3: 0x8F6AE, AC 3. 2e: AC 2 hand-hurled, 3 small missiles, 4 other, +1 frontal saves.
- Visual: 1.3 vclip 24 (S1). Port: none.
- Verdict: correct; missing visual (S1).

### 46 Sleep (mage 1)
- Port: missile, AREA radius 60, level 3 or less, not undead or `no_sleep`, no save;
  elves resist on 9 in 10, half-elves 3 in 10; the pool (rand() % 7) * 2 + 4 pays the
  max hit points / 4 of each creature; 5L minutes (D:2470, 2872, 1775). V.
- 1.3: 0x8F766. 2e: 30 yds, 2d4 HD, none above 4+3 HD, 5 rounds/level, undead immune.
- Visual: missile vclip 24 at 1/16 (S2), hit vclip 132 (sleep.abm) not played: the
  line has no `cast_on_top`, and the hit vclip plays only with damage in 1.3 too
  (V, 0x272DD).
- Verdict: correct rules; port bug in the visual (S2). The text does not name the
  pool.

### 47 Slow (mage 3)
- Port: missile, AREA radius 80, the caster spared, save negates, ends Haste, (L + 3)
  minutes, no counter (D:2503, 1776). V.
- 1.3: 0x8FAD8: counter L (R2); a slowed player gets the model animation time doubled.
- 2e: 90 yds + 10/level, 40 ft cube, 1 creature per level, save at -4, 3 + 1/level
  rounds.
- Visual: missile vclip 24 at 1/16 (S2).
- Verdict: port bug (R2); port bug in the visual (S2).

### 49 Stone Skin (mage 4)
- Port: self, 1d4 + L/2 hits; `Dtu_fix_stoneskin_word` keeps the other effects
  (D:2515, 1801). V.
- 1.3: 0x8FC7A writes the whole effect word. 2e: touch, 1d4 + 1 per 2 levels attacks.
- Visual: 1.3 vclip 121 (stone.abm) (S1). Port: none.
- Verdict: correct; missing visual (S1).

### 50 Strength (mage 2)
- Port: self, +1d8 fighter, +1d6 thief or cleric, else +1d4, at most 18; L hours, at
  most 4 h 33 min (`Dtu_fix_spell_hours`) (D:2525, 1807, 1778). V (1.3 0x8FCFA,
  pseudocode lines 1580-1620: the same order of class tests).
- 2e: warrior 1d8, priest and rogue 1d6, wizard 1d4, 1 hour/level.
- Visual: 1.3 vclip 126 (streng.abm) (S1). Port: none.
- Verdict: correct; missing visual (S1).

### 51 Find Traps (cleric 2)
- Port: self, bit 20 for 30 minutes (D:2541, 1779); the thief code reads the bit. V.
- 1.3: 0x8E4F6 (renamed by the fixup). 2e: 3 turns, a 10 ft path of 30 yds.
- Visual: the line has `weapon_pof=ice.pof` and no vclip: 1.3 shows nothing. Same.
- Verdict: correct.

## Scrolls, wands and the rod

- A scroll is a spell line held as an item (type 17). Any class reads it; no cast
  time; template weapon 4; caster level of the class bits of the spell (0x91EC1:
  the mage level, else the level of the class that is not the cleric) (V, D:2772,
  2920; `spells-wizard.md`). Visual: as the spell, with the S2 size of template 4
  (`blob_size=0.0`).
- The wands and the rod (V, `$WEAPON` after the fixup, dtutblfix.c:428-443): 55 Wand
  of Magic Missiles (33, 15 uses), 56 Wand of Confusion (7), 57 Wand of Fear (14), 58
  Wand of Fireballs (16), 59 Wand of Lightning Bolts (32), 60 Wand of Magic Detection
  (12), 61 Wand of Paralyzation (22, Hold Person), 62 Rod of Lightning (32); 6 uses
  each except 55. A wand is a weapon with `PC_spell` and attack type PROJECTILE
  (D:2790); it casts with template 3, no cast time, one charge per use.
- Game run: the Wand of Fireballs and the Wand of Confusion created a vclip missile
  of size 0.06 (S2). The Wand of Magic Missiles created the model (size 0.45, right).
- 2e (G, magic items are not on the spell site): a Wand of Paralyzation is a cone
  that holds any creature that fails a save; 1.3 casts Hold Person (humanoids, 1d4).
  Kept as 1.3.
- Verdict: the wands share every finding of their spell; no wand rule of their own is
  wrong.

## Summary

| # | Spell | Verdict | Findings |
|---|---|---|---|
| 0 | Abjure | correct, missing visual | S4 |
| 1 | Aid | correct, missing visual | S1 |
| 2 | Armor | correct, missing visual | S1 |
| 3 | Bless | correct, missing visual | S1 |
| 4 | Burning Hands | other agent | |
| 5 | Chill Touch | correct | |
| 6 | Color Spray | port bug (visual) | S2, S7 |
| 7 | Confusion | port bug (visual), open | S2, S6, R4 |
| 8, 9 | Continual Light | port bug (visual) | S2, S3, S8 |
| 10, 11 | Cure Light / Serious Wounds | correct, missing visual | S1 |
| 12, 54 | Detect Magic | correct, missing visual | S1 |
| 13, 55 | Dispel Magic | port bug (visual) | S2 |
| 14 | Fear | correct, missing visual | S1, S6 |
| 15 | Feather Fall | correct, missing visual | S1 |
| 16 | Fireball | other agent | S2, S3, S4 apply |
| 17 | Produce Fire | port bug (visual) | S2, S3, S4 |
| 18, 43 | Flame Walk, Resist Fire | correct, missing visual | S1 |
| 19 | Fly | correct, missing visual | S1 |
| 20 | Free Action | correct, missing visual | S1 |
| 21 | Haste | port bug | R2, S1, (R3) |
| 22, 52 | Hold Person | port bug (visual) | S2, S6 |
| 23 | Hold Undead | port bug | R2, S2, S6 |
| 24, 26 | (Improved) Invisibility | correct, missing visual | S1 |
| 25 | Infravision | correct, missing visual | S1 |
| 27, 29, 48 | Jump, Levitate, Spider Climb | correct, missing visual | S1 |
| 28 | Knock | port bug (visual) | S2 |
| 30, 31 | Light | port bug (visual) | S2, S3, S8 |
| 32 | Lightning Bolt | port bug | R1 |
| 33 | Magic Missile | port bug (visual) | S5 |
| 34 | Magical Stone | port bug (visual) | S2 |
| 35 | Magical Vestment | correct, missing visual | S1 |
| 36 | Melf's Acid Arrows | port bug (visual), text | S4, S5 |
| 37 | Melf's Minute Meteors | port bug (visual) | S2, S3, S4 |
| 38 | Prayer | correct, missing visual | S1 |
| 39, 53 | Protection From Evil | correct, missing visual | S1 |
| 40 | Protection From Fire | correct, missing visual | S1 |
| 41 | Rary's Mnemonic Enhancer | correct, missing visual | S1 |
| 42 | Cure Blindness or Deafness | correct, missing visual | S1 |
| 44 | Scare | port bug (visual) | S2, S6 |
| 45 | Shield | correct, missing visual | S1 |
| 46 | Sleep | port bug (visual) | S2 |
| 47 | Slow | port bug | R2, S2 |
| 49 | Stone Skin | correct, missing visual | S1 |
| 50 | Strength | correct, missing visual | S1 |
| 51 | Find Traps | correct | |

No new 1.3 defect needs a setting for a rule that players see, except the two low
priority candidates R3 (Haste on enemies) and the unused Hold Undead budget.

## Fixes, in order, by group

The groups touch different files. Group A and group B can run at the same time.
Group C waits for B (it reads the new facts). The Fireball and Burning Hands agent
works in `dtuspell.c` (cases 4 and 16, the damage, the area) and maybe in
`dtuobj_missile.c` (the area wall hit): each group must rebase on its branch and keep
to the functions named here.

### Group A: the missile look (`engine/main/dtuobj_missile.c`, `lighting.c`, `bmread.c`)

1. S2: in `missile_look`, for a spell missile drawn as a vclip, take the size from
   `weapon_vclip_size` of the spell line (default 1.0), as 0x56FF6 writes it into
   `blob_size`. Port bug, no setting. Test: the wand run above must give size 3.00 for
   Fireball and 1.00 for Confusion; a screenshot of a Sleep or Hold Person missile.
2. S5: in `missile_hit_wall`, for a missile with a spell, play the `wall_hit_vclip`
   and `wall_hit_sound` of the spell line (0x26C2F), still only for a model.
3. R1: Lightning Bolt (spell 32): at a wall do not remove the missile; bounce it
   (template bounce 1, life 2.0), and after a body hit let it go on. Needs a rule
   against a second hit of the same body in one bounce (the port has `pass_obj`).
   Port bug, no setting. Describe the bounce in `docs/re/spells.md`.
4. S3: the light of a spell missile is `player_light` of the line (0x56FF6, 0x577FE):
   keep it per missile (the template is shared) and add it in `lighting.c` for an
   `OBJ_WEAPON` with a spell.
5. S4: parse `vclip_trail` (spell and weapon lines) and create the trail vclip behind
   the missile in each frame (0x5656C). Check the rate against 1.3 before; G: once per
   frame.
6. S7: Color Spray (6): the missile vclip is 175 + rand() % 6 at each cast.

### Group B: the cast effects and the counters (`engine/main/dtuspell.c`)

1. S1: port 0x8CAD3 as one function: the `cast_vclip`, else the `weapon_vclip`, fixed
   to the caster, and a light object of `player_light`. Call it from `cast_now`,
   `creature_cast`, `dtu_spell_cast_vclip` (so the potions and items get it) and the
   light case. Read the lifetime of the light object and the vclip flags in the bytes
   of 0x8CB9F to 0x8CCCF first. Port bug, no setting. Test: a screenshot of Armor,
   Cure Light Wounds and a potion.
2. R2: the counters of Haste (L at the self cast), Slow (L at the launch) and Hold
   Undead (1d3 at the launch), with the tests of 0x8E79C, 0x8FAD8 and 0x8EA21; Hold
   Undead also returns on Free Action (0x5000). Use the `Hold_count` pattern. Port bug,
   no setting. Extend `tools/priestcheck.py` or the spelltest with a count of targets.
3. S6: port 0x8CCED: the `cast_on_top` vclip 8 units above each target of Confusion,
   Fear, Hold Person, Hold Undead and Scare, fixed to it until the end of the effect.
   The port may need a vclip object that follows an object (a new small helper in
   `fireball.c` or `dtuobj.c`: then it is group A's file; agree first).
4. S8: the light spells add `player_light` to the light of the target until the
   blindness ends (+0x1A6), and call the step 1 function.
5. R4: trace bit 5 (Confusion) in the creature AI and the player flags of 0x8DB82
   before any change.

Done (2026-11-25), with what the bytes showed:

- S1: `cast_effects` (0x8CAD3). The vclip is at the position plus 0.9 size forward,
  size/2 large; a line with the cast on the caster (+0x37 bit 0) keeps it at one size in
  front of the caster (0x7C474, +0x1A8 = 8). The cast sound of a cast on the caster is
  played here (volume 1/3). The light object lives the play time of the vclip
  `cast_vclip`, which is -1: 1.3 reads the word before the Vclip array (0x396F9A, no
  reference found, G); the port takes the play time of the vclip shown.
- S6: `top_vclip` (0x8CCED), size/4, 8.0 above the target, until the effect ends. 1.3
  also reaches the call when Hold Person or Hold Undead finds no humanoid or undead
  (the jump to 0x8FFE5 with a stale duration): no effect for the player, not ported.
- S8: the target light is no light. 0x8DCC8 adds the low word of `player_light` (0
  for 20.0 and 60.0) to +0x1A6, the integer part of the drawn light +0x1A4, and the
  draw (0x67523) moves it back to the light of the segment each frame. The cast effects
  (0x8DCD3) give the target the vclip and a light object of `player_light`.
- R4: bit 0x20 on a creature: the morale function 0x10ADF rolls rand() % 10 (wanders,
  stands confused, attacks, acts normal); the end (0x9026D) makes it hostile again and
  checks the morale. On the player: `type_flags` 0x10 makes `render_frame` (0x76C7C)
  skew the view rows with two bouncing counters. The stun of Color Spray sets the same
  bits on the player. A mushroom (0x25FB0) sets 0x10 for 16.0 s: not in the port.
- The port calls no morale check at the common end of 0x8CFFF (0x90024) for a
  creature target; the port adds it only for Confusion.

### Group C: the texts (`engine/main/dtudesc.c`, after group B)

1. Haste, Slow, Hold Undead: "Up to N targets" from the new counters (L, L, 1 to 3).
2. Hold Person: "Holds up to 1 to 4 humanoids." reads badly; "Holds 1 to 4 humanoids."
3. Lightning Bolt: a line for the bounce (after group A item 3).
4. Melf's Acid Arrows: the duration of the acid (L/3 more rounds).
5. Sleep: the pool (4 to 16) and the cost of a target.
6. The Spanish texts of these lines (`dtulang` tables), and `tools/check_desc.py` must
   pass for all four forms.

### Low priority, decide with the user

- R3: Haste hastes hostile creatures in its area. Candidate `Dtu_fix_haste_friends`
  (only the player and creatures that are not hostile), `-origcombat` clears it.
- Hold Undead: the budget of hit dice that 1.3 writes but never reads. Candidate
  `Dtu_fix_hold_undead_hd` (total target level at most L, the 2e rule).

## Not verified

- The lifetime of the light object of 0x8CAD3 and the flags of its vclip object.
- The spread offsets of 0x564F7 (S9).
- The physics of a bouncing weapon in 1.3 after a creature hit (R1).
- What bit 5 (Confusion) does to a creature in 1.3 (R4).
- Where 1.3 draws the Fireball explosion (vclip 111) of an area hit.
- No screenshot was taken; S1 to S8 are from the code and the one game log run.
- The older level tables (BASE, MINES1, PRISON1, SPIDER, TRAIN1) without the spell art.

## Appendix: the description texts (port, English, caster level 1)

Lines of the window, separated by " | ". The first lines (the name, the class and level,
the cast time) are left out when they repeat the block above.

| # | Text |
|---|---|
| 0 | Range: 1500 ft, Area: One target, Duration: Instant, Saving throw: None, An extraplanar creature may leave the level or die. |
| 1 | Range: Self, Duration: 2 minutes, Adds 1 to 8 hit points above the maximum, and Bless. |
| 2 | Range: Self, Duration: Until used up, Protects like worn armor of armor class 6. No effect if your armor is better. Ends after 9 points of weapon damage. |
| 3 | Range: Self, Duration: 6 minutes, Gives Bless. |
| 4 | Range: 1500 ft, Area: 5 ft where it lands, Missiles: 6, Saving throw: Half damage, Deals 1d3+2 fire damage. |
| 5 | Range: Self, Duration: 4 minutes, Gives Chill Touch. Each melee hit: the target saves or takes 1d4 more damage; undead get Fear. |
| 6 | Range: 1500 ft, Area: 20 ft where it lands, Missiles: 1 to 6, Duration: 1 to 8 minutes, Saving throw: Some targets, Puts to sleep, blinds or stuns, by the level of the target. |
| 7 | Range: 1500 ft, Area: One target, Missiles: 2 to 5, Duration: 3 minutes, Saving throw: Negates, Causes Confusion. |
| 8, 9 | Range: 1500 ft, Area: 1.5 ft where it lands, Duration: No end, Saving throw: Negates, Makes a light where it lands. Blinds a creature that it hits. |
| 10 | Range: Self, Duration: Instant, Heals 1 to 8 hit points. |
| 11 | Range: Self, Duration: Instant, Heals 3 to 17 hit points. |
| 12 | Range: Self, Duration: 2 minutes, Gives Detect Magic. |
| 13, 55 | Range: 1500 ft, Area: 30 ft where it lands, Duration: Instant, Saving throw: None, Ends all spell effects in the area, yours too. |
| 14 | Range: Self, Area: 45 ft around you, Duration: 1 minute, Saving throw: Negates, Causes Fear. The target may drop its weapon. |
| 15 | Range: Self, Area: 10 ft around you, Duration: 1 minute, Gives Feather Fall. |
| 16 | Range: 1500 ft, Area: 20 ft where it lands, Also hits you when you are in the area. Saving throw: Half damage, Deals 1d6 fire damage. With more caster levels: up to 10d6. Blinds each creature within 3 ft of the point for up to 30 minutes, and deafens it unless it saves. |
| 17 | Range: 1500 ft, Area: 12 ft where it lands, Saving throw: Half damage, Deals 1d4+1 fire damage. |
| 18 | Range: Self, Duration: 2 minutes, Gives Resist Fire. |
| 19 | Range: Self, Duration: 20 to 70 minutes, Gives Flying. |
| 20 | Range: Self, Duration: 10 minutes, Gives Free Action. |
| 21 | Range: Self, Area: 40 ft around you, Duration: 4 minutes, Gives Haste. |
| 22, 52 | Range: 1500 ft, Area: 20 ft where it lands, Duration: 2 minutes, Saving throw: Negates, Holds up to 1 to 4 humanoids. |
| 23 | Range: 1500 ft, Area: 40 ft where it lands, Duration: 2 to 5 minutes, Saving throw: Some targets, Holds undead of level 1 or less. |
| 24 | Range: Self, Duration: 5 minutes, Gives Improved Invisibility. |
| 25 | Range: Self, Duration: 3 hours, Gives Infravision. |
| 26 | Range: Self, Duration: Until you attack, Gives Invisibility. |
| 27 | Range: Self, Duration: 2 to 4 minutes, Gives Jump. |
| 28 | Range: 1500 ft, Area: One target, Duration: Instant, Opens a locked door. |
| 29 | Range: Self, Duration: 10 minutes, Gives Levitate. |
| 30 | Range: 1500 ft, Area: 1.5 ft where it lands, Duration: 10 minutes, Saving throw: Negates, Makes a light where it lands. Blinds a creature that it hits. |
| 31 | As 30, Duration: 70 minutes. |
| 32 | Range: 100 ft, Area: One target, Duration: Instant, Saving throw: Half damage, Deals 1d6 electric damage. With more caster levels: up to 10d6. |
| 33 | Range: 1500 ft, Area: One target, Saving throw: None, Deals 1d4+1 damage. With more caster levels: up to 5 missiles. |
| 34 | Range: 1500 ft, Area: One target, Missiles: 1 to 3, Saving throw: None, Each missile needs an attack roll. Deals 1d4 damage. 1d4 more damage against undead. |
| 35 | Range: Self, Duration: 5 minutes, Protects like worn armor of armor class 5. No effect if your armor is better. |
| 36 | Range: 3000 ft, Area: One target, Duration: Instant, Saving throw: None, An attack roll; a hit deals 2d4 acid damage. |
| 37 | Range: 1500 ft, Area: One target, Saving throw: None, Each missile needs an attack roll, +2 to hit. Deals 1d4 fire damage. With more caster levels: up to 5 missiles. |
| 38 | Range: Self, Duration: 1 minute, Gives Bless. |
| 39 | Range: Self, Duration: 2 minutes, Gives Protection From Evil. |
| 40 | Range: Self, Duration: Until used up, Takes the first 12 points of fire damage. |
| 41 | Range: Self, Duration: Until the next rest, Gives more spell slots: three of level 1, one of level 1 and one of level 2, or one of level 3. |
| 42 | Range: Self, Duration: Instant, Cures blindness and deafness. |
| 43 | Range: Self, Duration: 1 minute, Gives Resist Fire. |
| 44 | Range: 1500 ft, Area: 15 ft where it lands, Duration: 2 to 5 minutes, Saving throw: Some targets, Causes Fear on creatures of level 6 or less. The target may drop its weapon. |
| 45 | Range: Self, Duration: 5 minutes, Armor class 4 against melee weapons and 2 against missiles. No effect if your armor is better. Stops Magic Missile. |
| 46 | Range: 1500 ft, Area: 30 ft where it lands, Duration: 5 minutes, Saving throw: None, Causes Sleep on creatures of level 3 or less. |
| 47 | Range: 1500 ft, Area: 40 ft where it lands, Duration: 4 minutes, Saving throw: Negates, Causes Slow. |
| 48 | Range: Self, Duration: 4 minutes, Gives Spider Climb. |
| 49 | Range: Self, Duration: Until used up, Stops the next 1 to 4 attacks. |
| 50 | Range: Self, Duration: 1 hour, Adds 1 to 8 to strength. |
| 51 | Range: Self, Duration: 30 minutes, Gives Find Traps. |
| 53 | Range: Self, Duration: 3 minutes, Gives Protection From Evil. |
| 54 | Range: Self, Duration: 10 minutes, Gives Detect Magic. |

Wands: "Charges: N | Classes: ... | Each use casts SPELL." for the eight wands and the rod.

## Durations

Added 2026-11-25 (the stream report: Protection From Evil ended fast, a Potion of Flying
lasted long).

### Game time and real time

- 1.3: `calc_frame_time` (0x40E91) takes FrameTime from the timer and limits it to 0x1B4 to
  0x3333 (1/150 to 1/5 second); `game_frame` (0x435A6) and `game_frame_reduced` (0x4391D, the
  frame while a conversation waits) add it to GameTime. V (pseudocode).
- Port: `game.c:1313-1314` has the same limits; `game.c:4323` and `:4344` add FrameTime to
  GameTime, and the fixed step (`Dtu_fixed_step`) adds the same real time in steps. V (code).
- So one game second is one real second (slower only below 5 frames per second, or when a
  picture takes longer than the step limit). The turbo option doubles FrameTime in both.
- A round of 2e is 60 game seconds (`MINUTE`, `dtuspell.c:94`; 0x3C0000 in 0x8CFFF), a turn
  600. An effect stores its end time (GameTime + duration) and 0x9002C (port `fx_expire`)
  compares it with GameTime in each frame, so the frame rate does not change a duration.
- Game run: Protection From Evil (mage) at caster level 1, no `-fixedfps`, measured with the
  wall clock: 119.9 s from the cast to "Protection From Evil expired." (log: cast at GameTime
  0.95, end at 120.95). V.
- Effects stay over a level change and a save and load (game run: PfE and Fly cast on BANE,
  warp to KOBOLDS, save slot 3, load: effect bits 0x00800802 after the load). V. The timers
  after the load were not read.
- Not checked: whether GameTime runs while the inventory or the menus are open, in 1.3 and
  in the port.

### What the player sees

The cast message ("You have Protection From Evil for N minutes.", whole minutes rounded down;
Fly, Invisibility, Armor and Stone Skin give no time), the description window (the duration
at the caster level, as in the appendix) and "NAME expired." at the end. The port has no
display of the time left. 1.3 shows none in the code read (G: the HUD code of 1.3 was not
searched for one).

### Spells

L = caster level (`dtu_spell_player_level`, `dtuspell.c:272`: the level of the class plus 1,
the higher class for a spell of both books; a scroll, wand, rod or potion that casts a spell
uses the same level, a creature `creature_level`). 1.3 = the line of `Spell_duration` in the
pseudocode of 0x8CFFF. Real time = game time. Port lines: `spell_time` in
`engine/main/dtuspell.c`.

| Spell | Port | 1.3 (0x8CFFF line) | 2e | Real time at L 1 / L 5 | Verdict |
|---|---|---|---|---|---|
| 1 Aid (Bless) | (L+1) rounds, :1747 | (L+1)*0x3C0000, 207 | 1 rd + 1/level | 2 / 6 min | same |
| 2 Armor | pool 8 + L, no timer, :2080 | pool, no timer | until dispelled or more than 8 + L damage | until used up | same (see below) |
| 3 Bless | 360 s, :1748 | 0x1680000, 249 | 6 rds | 6 min | same |
| 5 Chill Touch | (L+3), :1749 | 270 | 3 rds + 1/level | 4 / 8 min | same |
| 6 Color Spray | sleep 2 to 8, blind 1d4, stun 1 round, :1734 | 371, 332, 298 | unconscious 2d4, blind 1d4, stun 1 rd | 1 to 8 min | same |
| 7 Confusion | (L+2), :1750 | 438 | 2 rds + 1/level | 3 / 7 min | same |
| 8, 9 Continual Light | no end, :1751 | 0x3FFFFFFF, 475 | permanent | no end | same |
| 12 Detect Magic (mage) | 2L, :1754 | L*0x780000, 595 | 2 rds/level | 2 / 10 min | same |
| 54 Detect Magic (cleric) | 600 s, :1755 | 0x2580000, 598 | 1 turn | 10 min | same |
| 14 Fear | L, :1756 (on the player / (wisdom / 2)) | 699, 701 | rounds = caster level | 1 / 5 min | same |
| 15 Feather Fall | L, :1758 | 743 | 1 rd/level | 1 / 5 min | same |
| 18 Flame Walk | (L+1), :1759 | 788 | 1 rd + 1/level | 2 / 6 min | same |
| 19 Fly | 10 (1d6 + L), :1760 | (rand%6+1+L)*0x2580000, 804 | 1 turn/level + 1d6 turns | 20 to 70 / 60 to 110 min | same |
| 20 Free Action | 10L, :1761 | L*0x2580000, 826 | 1 turn/level | 10 / 50 min | same |
| 21 Haste | (L+3), :1762 | 848 | 3 rds + 1/level | 4 / 8 min | same |
| 22, 52 Hold Person | 2L, :1763 (on the player / (wisdom / 2)) | L*0x780000, 879 | 2 rds/level | 2 / 10 min | same |
| 23 Hold Undead | 1d4 + L, :1764 | 938 | 1d4 rds + 1/level | 2 to 5 / 6 to 9 min | same |
| 24 Improved Invisibility | (L+4), :1765 | 951 | 4 rds + 1/level | 5 / 9 min | same |
| 25 Infravision | (6L+12) turns = L+2 hours, :1766 | (L*6+0xC)*0x2580000, 981 | 2 hrs + 1/level | 3 / 7 hours | same (`Dtu_fix_spell_hours` from L 8) |
| 26 Invisibility | no end, :1767 | 0x3FFFFFFF, 996 | until an attack, 24 hours at most | until an attack | same, no 24 hour cap |
| 27 Jump | 1d3 + L, :1768 | 1018 | 1d3 rds + 1/level | 2 to 4 / 6 to 8 min | same |
| 29 Levitate | 10L, :1769 | L*0x2580000, 1035 | 1 turn/level | 10 / 50 min | same |
| 30 Light (mage) | 10L, :1752 | L*0x2580000, 468 | 1 turn/level | 10 / 50 min | same |
| 31 Light (cleric) | 10 (L+6), :1753 | 468 | 1 hr + 1 turn/level | 70 / 110 min | same |
| 35 Magical Vestment | 5L, :1770 | L*0x12C0000, 1089 | 5 rds/level | 5 / 25 min | same |
| 38 Prayer | L, :1771 | 1125 | 1 rd/level | 1 / 5 min | same |
| 39 Protection From Evil (mage) | 2L, :1772 | L*0x780000, 1141 | 2 rds/level | 2 / 10 min | same |
| 53 Protection From Evil (cleric) | 3L, :1773 | L*0xB40000, 1144 | 3 rds/level | 3 / 15 min | same |
| 40 Protection From Fire | pool 12L, no timer | pool, no timer | pool, 1 turn/level at most | until used up | 1.3 has no time limit (kept) |
| 43 Resist Fire | L, :1774 | 1241 | 1 rd/level | 1 / 5 min | same |
| 44 Scare | 1d4 + L, :1757 (on the player / (wisdom / 2)) | 1306 | 1d4 rds + 1/level | 2 to 5 / 6 to 9 min | same |
| 45 Shield | 5L, :1775 | L*0x12C0000, 1361 | 5 rds/level | 5 / 25 min | same |
| 46 Sleep | 5L, :1776 | 1457 | 5 rds/level | 5 / 25 min | same |
| 47 Slow | (L+3), :1777 | 1534 | 3 rds + 1/level | 4 / 8 min | same |
| 48 Spider Climb | (L+3), :1778 | 1555 | 3 rds + 1/level | 4 / 8 min | same |
| 49 Stone Skin | hits 1d4 + L/2, no timer | no timer | until the attacks are used | until used up | same |
| 50 Strength | L hours, :1779 | L*0xE100000, 1678 | 1 hr/level | 1 / 5 hours | same (`Dtu_fix_spell_hours` from L 10) |
| 51 Find Traps | 1800 s, :1780 | 0x7080000, 1693 | 3 turns | 30 min | same |
| 41 Rary's Mnemonic Enhancer | no timer | no end | 1 day | until the second rest (`Dtu_fix_rary_manual`) | decided earlier |

Each port value is the 1.3 value, and each 1.3 value is the 2e value with 1 round = 1 minute.
V (the pseudocode lines above and `spell_time`; the 2e column from `WizardSpells.json` and
`PriestSpells.json` of the 2e site, fetched 2026-11-25).

Armor: the port takes weapon damage from the pool (0x29F3E, 0x2A4FB, 0x913AF for a creature;
`dtu_spell_armor_hit`, `dtuspell.c:838`) and 0x9002C ends the effect when the pool is below
1.0, so it ends after 8 + L points (2e: more than 8 + L). Spell damage on the player does not
take from the pool (the player branch of 0x913AF has no such step). A 1 point difference,
kept as 1.3. Game run: a scimitar hit of 12 on Armor of level 1: "the Armor pool of obj 0
you takes 12.0, left 0.0". V.

### Potions and items

Port: the table `Misc_use` (`engine/main/dtuchar.c:2099` to `:2115`), times
(rand() % t_mod + t_add) * t_unit seconds. 1.3: `inventory_use_item` (0x37A6A),
DTU-Decomp `docs/items.md` section 7. In both, a new end replaces an old one only when it is
later. 2e (G, not checked against the book; the potion rules are not on the spell site): a
potion lasts 4 + 1d4 turns unless the item says otherwise.

| Potion | Port and 1.3 | 2e (G) | Real time |
|---|---|---|---|
| 72 Agility | 600 s | no 2e item | 10 min |
| 74 Dragon's Blood | (rand % (class 1 level + 1) + 1) rounds | no 2e item | 1 to L min |
| 76 Fire Resistance | Resist Fire 600 s | 5 to 8 turns | 10 min |
| 77 Flying | the spell Fly at the drinker's level | 5 to 8 turns | 10 (1d6 + L) min |
| 78 Freedom | Free Action (rand % 4 + 5) turns | 5 to 8 turns | 50 to 80 min |
| 80 Hill Giant Strength | (rand % 4 + 5) turns | 5 to 8 turns | 50 to 80 min |
| 81 Hill Giant Intelligence | (rand % 4 + 5) turns | no 2e item | 50 to 80 min |
| 82 Infravision | (rand % 8 + 1) turns | 5 to 8 turns | 10 to 80 min |
| 83 Invisibility | (rand % 4 + 3) turns | 5 to 8 turns | 30 to 60 min |
| 84 Invulnerability | (rand % 16 + 5) rounds | 5 to 8 turns | 5 to 20 min |
| 85 Jumping | the spell Jump | no 2e item | 1d3 + L min |
| 86 Luck | 3600 s | no 2e item | 60 min |
| 87 Speed | Haste (rand % 16 + 5) rounds | 5 to 8 turns | 5 to 20 min |
| 88 Toughening | Bless (rand % 6 + 7) turns | no 2e item | 70 to 120 min |

The Potion of Flying of the stream: the spell Fly at the level of the drinker, so 10 times
(1d6 + L) minutes of real time: 20 to 70 minutes at level 1, 50 to 100 at level 4. It is
close to the 2e potion (50 to 80 minutes, G), so the 1.3 rule stays.

Worn items (rings, boots, cloaks, the amulet, the gauntlet, weapon 54) give their effects
with no end while worn (0xB08A8, `worn_effects` in `dtuchar.c`); the Assembled Spider gives
four effects with no end (case 0x19 of `use_misc`). No other item has a timer.

### Verdict on the stream report

No port bug in the durations or in the count. Protection From Evil lasts 2 rounds per level
(mage) or 3 (cleric) in 1.3 and in 2e: 2 real minutes per level of a mage. Fly lasts 1d6 + L
turns, so a potion lasts about an hour or more. Both are right; the difference is the 2e rule.

### Rest (`Dtu_fix_rest_effects`, new)

1.3: `player_rest_button` (0x360D5) calls `spell_restore_slots` (0x3551B, the slots only)
and `player_rest_stop` (0x3E1F6: the alert, the spell table shuffle 0x92FF2, the hit points,
the sound, the appearance vclip 0x4D95D, the rest time of the place, the fire). No effect
and no potion timer ends. V (pseudocode).

The rule of the user: a rest ends the spell effects on the party, except Armor. The port
(`dtu_spell_rest_effects`, `dtuspell.c:3277`, called by `dtu_char_rest`, `dtuchar.c`) ends:

- each effect with a timer: the spells, the creature spells on the player and the effects of
  the potions (they set the same bits, so a potion effect cannot be told from a spell);
- Protection From Fire (a pool; 2e: 1 turn per level at most) and Invisibility (no timer in
  1.3; 2e: 24 hours at most);
- the five potion timers (Agility, Dragon's Blood, Hill Giant Strength and Intelligence,
  Luck): each is shorter than a rest.

It keeps the effects with no time limit in 2e: Armor and Stone Skin (until damage or Dispel
Magic), the blindness of Continual Light (permanent) and deafness (no timer, cured by the
spell); Rary's Mnemonic Enhancer (its own rest rule, `Dtu_fix_rary_manual`); regeneration;
and every effect of an item: the worn items (passed as a mask) and the Assembled Spider (a
timer with no end). `-origcombat` clears the setting.

Game runs (BANE, rest place segment 68, `-chartest`, `-objlog`): Protection From Evil, Fly,
Armor, Stone Skin, Invisibility, Shield, Bless, the Potions of Agility and Speed and a
blindness of 600 s, then a rest: 7 effects end with their "expired." texts, the potion
timers end, the bits after the rest are 0x20000001 (Armor and Stone Skin). With
`-origcombat` nothing ends. With the Ring of Free Action and the Elven Cloak worn plus
Improved Invisibility and Protection From Evil: the two spells end, Free Action and the
Invisibility of the cloak stay (bits 0x00021001). One text glitch, as in 1.3 at a normal
end: the two invisibilities share a timer and end together (0x906B9), so "Invisibility
expired." shows and the cloak sets Invisibility again at once.
