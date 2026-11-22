# Spells: audit of the port against DTUGAME.EXE 1.3

This page compares each spell of the port (`engine/main/dtuspell.c`) with the
executable. The rules of the spells are in [combat.md](combat.md), part B (B1 cast,
B2 and B3 the cases of 0x8CFFF, B4 the effects and 0x9002C, B5 saves, B10 the
creatures). This page adds the cast time check, the texts of each case and of each
end, and the list of differences.

Sources:

- The `$SPELL` lines of all 46 level tables (`gamedata/unpacked/*.BIN`, read with
  `tools/objcheck.py` `table_lines`; the installed `YPINN.BIN` has the same lines).
  `cast_time=` is the same in all 46 tables for every spell (spells 52 to 55 are in
  44 tables).
- The code: 0x41A4D (game.c, the cast), 0x24C13 (the table reader), 0x8CFFF (the
  cases), 0x9002C (the ends), the strings 0x12D88A to 0x12E5E5.

## 1. Cast time (Code)

- Reader 0x24BEC: `cast_time=N` is `atoi` into the word Spell_info+0x04 (record
  0x1B567A + spell * 127). No scale. Default 0.
- 0x41C9F (the first frame of the attack key with a spell of a book, after the checks
  of 0x356E0, the armor and the climbing tests): `0x153EBC = GameTime + (cast_time
  << 16)`. GameTime (0x13CD30) is the Descent game clock: `GameTime += FrameTime`
  (0x43800, 0x43A24), FrameTime is the timer delta in seconds, between 0x1B4 and
  0x3333 (0.2 s; 0x40F1A). So the unit is one second of game time.
- Text: `cast_time > 60`: "Casting time: %d minutes ..." with `cast_time / 60`;
  `> 1`: "Casting time: %d seconds ..."; else "Casting time: 1 second ...".
- Each later frame: while `0x153EBC > GameTime` nothing (0x41D27); then
  `0x153EBC = 0x3FFFFFFF` and the spell fires (0x41E36 returns 1). A scroll, a wand or
  a creature (B10: `+0x1C0 = GameTime + cast_time`) use the same value; a scroll and
  a wand skip it.
- The request flag 0x13B69F stays set while the time runs (the key need not be
  held); the hot keys refuse a change in that time (character.md, 0x43A5A).

`cast_time=` of the tables (seconds; the text that the cast shows):

| s | Spells |
|---|---|
| 0 (none for 32, 33, 37, 44) | 2 Armor, 4 Burning Hands, 6 Color Spray, 14 Fear, 19 Fly, 41 Rary's Mnemonic Enhancer, 51 Burning Hands (cold), 32 Lightning Bolt, 33 Magic Missile, 37 Melf's Minute Meteors, 44 Scare: "1 second", the cast in the next frame |
| 1 | 24 Improved Invisibility, 26 Invisibility: "1 second" |
| 10 | 5, 7, 15, 16, 21, 22, 23, 25, 27, 28, 29, 30, 36, 39, 40, 42, 43, 45 Shield, 46 Sleep, 48, 49, 50, 52, 53: "10 seconds" |
| 24 | 31 Light (Cleric), 34 Magical Stone |
| 30 | 1 Aid, 10 Cure Light Wounds, 18 Flame Walk |
| 36 | 8, 9 Continual Light, 13, 55 Dispel Magic, 17 Flaming Sphere, 38 Prayer |
| 42 | 11 Cure Serious Wounds, 20 Free Action |
| 60 | 0 Abjure, 3 Bless, 12, 54 Detect Magic, 35 Magical Vestment, 47 Slow: "60 seconds" (not above 60, so no minutes text) |

No line has a cast time above 60, so the minutes text never shows.

Armor: `cast_time=0` in all 46 tables, the code adds no other wait: the executable
shows "Casting time: 1 second ..." and casts Armor in the frame after the key. The
AD&D rule (1 round, one minute) is not in the data or the code. The port did the same
before this audit and does now (checked: the cast starts at 1.033 s, Armor at 1.066 s
at 30 frames per second). A report of one minute in the original is not explained by
DTUGAME.EXE 1.3 with these tables; it needs a check in the DOS game.

## 2. The texts of 0x8CFFF (Code)

Names and verbs (0x8D08D to 0x8D1B4): target 0x3A6584: the player "You", type 13 (a
trap) "a trap", a creature its name (0xA7F03), another object its name (0xB8C0F);
0x3A6514 "were" (the player) or "was". Caster 0x3A6548: "You", "a trap" or the name;
0x3A6528 "are"/"is" and 0x3A6500 "have"/"has" by the caster. The text goes to the
message box (0x347A2) only when 0x8CEAE gave 1 (combat.md B2): the player is the
target or the caster; else the segment of the caster is visible, the caster is seen
(0x16636) and d20 <= intelligence - 2.

Minutes are `(duration >> 16) / 60`, hours `/ 3600` (idiv, rounded down). "self" is
target == caster.

| # | Spell | On self (address) | On another |
|---|---|---|---|
| 0 | Abjure | "%s was not affected by abjure." (target) on a failed roll | same |
| 1 | Aid | "%s %s %d aid hit points for %d minutes." (have, hp, min) 0x12D8CD | "%d aid hit points cast on %s by %s for %d minutes." |
| 2 | Armor | "%s %s %d armor hit points." (have) | "%d armor hit points cast on %s by %s." |
| 3 | Bless | "%s %s blessed for %d minutes." (are) | "Bless cast on %s by %s for %d minutes." |
| 5 | Chill Touch | "%s %s chill touch for %d minutes." (have); self only | none |
| 6 | Color Spray | | "%s %s struck unconscious by %s." / "%s %s blinded by %s." / "%s %s stunned by %s." (were) |
| 7 | Confusion | "Confusion cast on %s by %s for %d minutes." | same |
| 8, 9, 30, 31 | Light, Continual Light | | "%s %s blinded by %s indefinitely." / "%s %s blinded by %s for %d minutes." (were) |
| 10, 11 | Cure Light / Serious Wounds | "Cure light wounds cast on %s by %s." / "Cure serious wounds cast on %s by %s." | same |
| 12, 54 | Detect Magic | "%s can detect magic for %d minutes." only when the target is the player | none |
| 14 | Fear | "Fear cast on %s by %s." | same |
| 15 | Feather Fall | "%s %s feather fall for %d minutes." (have) | "Feather fall cast on %s by %s for %d minutes." |
| 18 | Flame Walk | "%s %s fire tolerance for %d minutes." (have) | "Flame Walk cast on %s by %s for %d minutes." |
| 19 | Fly | "%s %s Flying." (have) | "Flying cast on %s by %s." |
| 20 | Free Action | "%s %s Free Action for %d minutes." (have) | "Free Action cast on ... for %d minutes." |
| 21 | Haste | "%s %s Haste for %d minutes." (have) | "Haste cast on %s by %s for %d minutes." |
| 22, 52 | Hold Person | "%s %s held for %d seconds." below 60 s, else "... minutes." (are) | "Hold Person cast on %s by %s for %d seconds." / "... minutes." |
| 23 | Hold Undead | "Hold Undead cast on %s by %s for %d minutes." | same |
| 24 | Improved Invisibility | "%s %s invisible for %d minutes." (are) | "Improved Invisibility cast on %s by %s for %d minutes." |
| 25 | Infravision | "%s %s Infravision for %d hours." (have) | "Infravision cast on %s by %s for %d hours." |
| 26 | Invisibility | "%s %s invisible." (are) | "Invisibility cast on %s by %s." |
| 27 | Jump | "%s may leap for some time." | "Jump cast on %s by %s." |
| 29 | Levitate | "%s can levitate for %d minutes." | "Levitate cast on %s by %s for %d minutes." |
| 35 | Magical Vestment | "%s %s Magical Vestment for %d minutes." (have) | "Magical Vestment cast on ..." |
| 38 | Prayer | "%s %s Prayer for %d minutes." (have) | "Prayer cast on %s by %s for %d minutes." |
| 39, 53 | Protection From Evil | "%s %s Protection From Evil for %d minutes." (have) | "Protection From Evil cast on ..." |
| 40 | Protection From Fire | "%s %s %d hit points of Fire Immunity." (have, the pool +0x82 after the cast) | "%d hit points of Protection From Fire cast on %s by %s." |
| 41 | Rary's Mnemonic Enhancer | "%s %s Rary's Mnemonic Enhancer for 1 day." (have) | "Rary's Mnemonic Enhancer cast on %s by %s for 1 day." |
| 42 | "Resist Cold" (Cure Blindness or Deafness) | "%s %s no longer blind." then "%s %s no longer deaf." into the same buffer (the caster name and "are": the last one shows) | "Cure Blindness or Deafness cast on %s by %s." |
| 43 | Resist Fire | "%s %s Fire Resistance for %d minutes." (have) | "Resist Fire cast on %s by %s for %d minutes." |
| 44 | Scare | "%s %s Scared." (are) | "Scare cast on %s by %s." |
| 45 | Shield | "%s %s Shielded for %d minutes." (are) | "Shield cast on %s by %s for %d minutes." |
| 46 | Sleep | "%s will be Sleeping for %d minutes." | "Sleep cast on %s by %s for %d minutes." |
| 47 | Slow | "%s %s Slowed for %d minutes." (are) | "Slow cast on %s by %s for %d minutes." |
| 48 | Spider Climb | "%s may Spider Climb for %d minutes." | "Spider Climb cast on %s by %s for %d minutes." |
| 49 | Stone Skin | "%s %s Stone Skin for %d attacks." (have) | "Stone Skin cast on %s by %s for %d attacks." |
| 50 | Strength | "%s %s Strength for %d hours." (have) | "Strength cast on %s by %s for %d hours." |
| 51 | Find Traps | "%s can Find Traps for %d minutes." (30) | "Find Traps cast on %s by %s for %d minutes." |

No text: 4, 16, 17, 32, 33, 34, 36, 37 (the damage texts of 0x911AA, combat.md B2),
13 and 55 Dispel Magic, 28 Knock. A case that returns early (the effect is on
already, a save made) prints nothing.

## 3. The end texts of 0x9002C (Code)

"NAME expired." for the player (0x347A2) with NAME: Armor, Bless, Blinded, Chill
Touch, Confusion, Detect Magic, Fear, Protection From Fire, Resist Fire, Flying, Free
Action, Haste, Hold Person, Infravision, Invisibility, Invulnerability, Jump, Find
Traps, Levitate, Rary's Mnemonic Enhancer, Protection From Evil, Shield, Sleep, Slow,
Spider Climb, Stoneskin, Strength, Magical Vestment. No text for bit 4 (deaf), bit 7
(Feather Fall, 0x9037C) and bit 24. Bits 16 and 17 end together: 0x906B9 clears both
bits before it tests bit 16, so the text is always "Invisibility expired."; then sound
161 (0x2D955(0xA1, 1.0)), also for a creature. Invulnerability: sound 163 (0x9073B).

## 4. The rules of each spell

Port before = the port at d9d72f6. "same" = the port follows B3 of combat.md.

| # | Spell | Executable (combat.md B3, B1) | Port before | Port after |
|---|---|---|---|---|
| 0 | Abjure | C4, target; 60 s | same; text only for the player caster | text by 0x8CEAE |
| 1 | Aid | C2, 30 s; +1d8 hp with no cap (0x8D46A), Bless (L+1) min, the Bless timer replaced; sound 0x2E900 with the sound word P+0x1B33 | hp cut to the maximum, "You feel better.", Bless not renewed when on | hp above the maximum (`dtu_char_add_hit_points`), timer renewed, the Aid text; the sound is not in the port |
| 2 | Armor | M1, 0 s; pool 8 + L, AC 6 | same | text by 0x8CEAE |
| 3 | Bless | C1, 60 s; 360 s | generic "Bless for 6 minutes." | original text |
| 4 | Burning Hands | M1, 0 s; 6 missiles, fire 1d3 + 2L, save half | same | same |
| 5 | Chill Touch | M1, 10 s; self, (L+3) min | generic text | original text |
| 6 | Color Spray | M1, 0 s; 1d6 missiles; sleep, blind or stun by level | generic texts | original texts |
| 7 | Confusion | M4, 10 s; 1d4 + L missiles; save; (L+2) min | generic text | original text |
| 8, 9 | Continual Light | C3, M2, 36 s; area; blind, no end | "Blindness." | original text |
| 10, 11 | Cure Light / Serious Wounds | C1, C4, 30 s, 42 s; 1d8, 3..17, at most the maximum | "You feel better." | original text |
| 12, 54 | Detect Magic | M1, C1, 60 s; 2L min, 10 min | generic text | original text, only for the player |
| 13, 55 | Dispel Magic | M3, C3, 36 s | same | same |
| 14 | Fear | M4, 0 s; save; L min | generic texts (Fear and Hold) | "Fear cast on %s by %s." |
| 15 | Feather Fall | M1, 10 s; L min; no end text | generic texts, "Feather Fall expired." | original text, no end text |
| 16 | Fireball | M3, 10 s; fire min(L,10) dice + 6L | same (`Dtu_fix_zero_dice`) | same |
| 17 | Flaming Sphere | M2, 36 s | same | same |
| 18 | Flame Walk | C3, 30 s; (L+1) min | generic text | original text |
| 19 | Fly | M3, 0 s; 10 (1d6 + L) min | generic text | original text |
| 20 | Free Action | C4, 42 s; self; 10L min | generic text | original text |
| 21 | Haste | M3, 10 s; (L+3) min | generic text | original text |
| 22, 52 | Hold Person | M3, C2, 10 s; 2L min, / (wisdom/2) for the player | generic text | original text with seconds below 60 s |
| 23 | Hold Undead | M3, 10 s; (1d4 + L) min | generic text | original text |
| 24 | Improved Invisibility | M4, 1 s; (L+4) min | generic text; end "Improved Invisibility expired." | original texts, end sound 161 |
| 25 | Infravision | M3, 10 s; (6L + 12) * 10 min | generic text in minutes | original text in hours |
| 26 | Invisibility | M2, 1 s; no end | "Invisibility." | original text, end sound 161 |
| 27 | Jump | M1, 10 s; (1d3 + L) min | generic text | original text |
| 28 | Knock | M2, 10 s; nothing | same | same |
| 29 | Levitate | M2, 10 s; 10L min | generic text | original text |
| 30, 31 | Light | M1, C1, 10 s, 24 s; 10L, 10(L+6) min | "Blindness for N minutes." | original text |
| 32 | Lightning Bolt | M3, 0 s | same | same |
| 33 | Magic Missile | M1, 0 s; min(L/2 + 1, 5) homing missiles, 1d4 | same | same |
| 34 | Magical Stone | C1, 24 s; 1d3 missiles, 1d4 (2d4 undead) | same | same |
| 35 | Magical Vestment | C3, 60 s; 5L min | generic text | original text |
| 36 | Melf's Acid Arrows | M2, 10 s | same | same |
| 37 | Melf's Minute Meteors | M3, 0 s; min(L, 5) missiles | same | same |
| 38 | Prayer | C3, 36 s; Bless L min | generic text | original text |
| 39, 53 | Protection From Evil | M1, C1, 10 s; 2L, 3L min | generic text | original text |
| 40 | Protection From Fire | C3, 10 s; pool += 12L | "Protection From Fire for N hit points." | original text with the pool |
| 41 | Rary's Mnemonic Enhancer | M4, 0 s | "You have ... for 1 day." for the player only | original text by 0x8CEAE |
| 42 | Cure Blindness or Deafness | C2, 10 s | "You are no longer blind or deaf." | original texts |
| 43 | Resist Fire | C2, 10 s; L min | generic text | original text |
| 44 | Scare | M2, 0 s; (1d4 + L) min | generic texts | "Scare cast on %s by %s." |
| 45 | Shield | M1, 10 s; 5L min, AC 3 | "Shield for 5 minutes." | "You are Shielded for 5 minutes." |
| 46 | Sleep | M1, 10 s; 5L min, level 3 or less | generic text | original text |
| 47 | Slow | M3, 60 s; (L+3) min | generic text | original text |
| 48 | Spider Climb | M1, 10 s; (L+3) min | generic text | original text |
| 49 | Stone Skin | M4, 10 s; 1d4 + L/2 attacks | "Stoneskin for N attacks." for the player | original text by 0x8CEAE |
| 50 | Strength | M2, 10 s; L hours: L * 225 << 20 in 32 bits, which wraps from level 10 on (the effect ends at once) | `L*3600*F1_0` (signed overflow in C) | the 32 bit wrap of the executable, the text in hours |
| 51 | Find Traps | M1, 0 s; 30 min | generic text | original text |

## 5. The cast (0x41A4D, 0x9305D)

| Rule | Executable | Port before | Port after |
|---|---|---|---|
| Cast time unit and text | seconds of game time, texts of section 1 | same | same (the log line has the text) |
| Refusal of 0x356E0 and of the armor test | sound 3000, volume 1.0 (0x41B0D, 0x41C2D), then the text | text only | sound and text |
| "You cannot cast a spell while climbing." | player +0x1AE bit 0x80: Levitate (0x8EDB2) or the climb of Climb Walls (0x9531D, 0x95413); no sound | Levitate only | Levitate or the climb |
| Interruption 0x9305D (player) | clears the request 0x13B69F and 0x13B6A3, arms state 8 (flinch) at once | the request stayed: the next frame started the cast again (a new "Casting time" text, no slot used) | request cleared (`dtu_combat_cast_stopped`), flinch |
| Texts for a cast between two creatures | 0x8CEAE | none | 0x8CEAE with the segments of the last rendered frame as the visible list (guess for 0x38C060) |

## 6. Kept differences

Settings (combat.md C3), not changed: `Dtu_fix_zero_dice`, `Dtu_fix_stoneskin_word`,
`Dtu_fix_rary_manual`, `Dtu_fix_wizardry_ring`.

Not in the port (combat.md C2): the view of the player in the gated creature of Fear
and Scare, the blindness and deafness of Fireball within 6.0, the global counters
Spell_info+0x49 of Haste, Hold Person, Slow and the hit dice pools of Hold Undead and
Sleep, the `cast_on_top` vclips (0x8CCED), the light +0x1A6 of the blindness end, the
volume of the player at the end of the deafness, the sound of Aid (0x2E900 with the
word P+0x1B33).

Not checked: the generic texts of `dtu_spell_set_effect` for the items (potions, worn
items; character.md), the text "%s %s %s." of 0x911AA.

## 7. Check in the port

BANE, `-char SG0.SG` (thief/mage of level 1), `-testslots 5 -fixedfps 30 -hidden
-mute`, `-objlog` lines `spell:`:

- Armor (`-testspell 2`): "Casting time: 1 second ..." at 1.033 s, the cast at 1.066
  s, "You have 9 armor hit points.", armor class 2.
- Shield (45): "Casting time: 10 seconds ..." at 1.033 s, the cast at 11.064 s, "You
  are Shielded for 5 minutes.", armor class -1.
- Magic Missile (33, `-testpos 25,-1,215,0`): 1 second text, one homing missile, the
  skeleton fails its save, 4 damage.
- Sleep (46, `-testpos -185.6,18.7,122,0` with a look down): "Sleep cast on Giant Rat
  by You for 5 minutes."
- Light (30): "Skeleton was blinded by You for 10 minutes.", the Ghast saves.
- Detect Magic (12): "Casting time: 60 seconds ...", the cast at 61.05 s, "You can
  detect magic for 2 minutes."
- Cleric spells by `-chartest cast=`: Bless "You are blessed for 6 minutes."; Aid "You
  have 1 aid hit points for 2 minutes." (8 of 7 hit points, Bless 120 s); Cure Light
  Wounds "Cure light wounds cast on You by You."; spell 42 "You are no longer blind.".
- Ends (`-chartest fx=`): "Blinded expired.", "Invisibility expired." for Improved
  Invisibility, no text for Feather Fall.
- Shield with the skeleton in reach (no `-noattack`): the hit at 1.300 s stops the
  cast, one slot used, the cast does not start again.
