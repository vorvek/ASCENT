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
  44 tables), but the game does not use it (section 1).
- The code: 0x41A4D (game.c, the cast), 0x24C13 (the table reader), 0x1A344 (the
  table fixup), 0x8CFFF (the cases), 0x9002C (the ends), the strings 0x12D88A to
  0x12E5E5.

## 1. Cast time (Code, DOS game)

- Reader 0x24BEC: `cast_time=N` is `atoi` into the word Spell_info+0x04 (record
  0x1B567A + spell * 127). No scale. Default 0 (0x243C6).
- Fixup 0x1A344, called at the end of each table read (0x1C8F9, after the `$SPELL`
  reader 0x242F6 that the dispatcher calls at 0x1C45C): for each spell line read
  (0 to N_spell_types 0x13A853 - 1), word 0x1B567A + spell * 127 = word 0x13A8DB +
  spell * 2 (0x1AED5 to 0x1AEF2). No condition. So `cast_time=` of the tables never
  reaches the game: the cast time is the table of 56 words at 0x13A8DB (initialized
  data, no other writer; the next variable starts at 0x13A94B).
- 0x41C9F (the first frame of the attack key with a spell of a book, after the checks
  of 0x356E0, the armor and the climbing tests): `0x153EBC = GameTime + (word << 16)`.
  GameTime (0x13CD30) is the Descent game clock: `GameTime += FrameTime` (0x43800,
  0x43A24), FrameTime is the timer delta in seconds, between 0x1B4 and 0x3333 (0.2 s;
  0x40F1A). So the unit is one second of game time. The words are the AD&D casting
  times: a number of segments as seconds, 60 for 1 round, 600 for 1 turn.
- Text (0x41CC2, the word): `> 60`: "Casting time: %d minutes ..." with the value / 60;
  `> 1`: "Casting time: %d seconds ..."; else "Casting time: 1 second ...".
- Each later frame: while `0x153EBC > GameTime` nothing (0x41D27); then
  `0x153EBC = 0x3FFFFFFF` and the spell fires (0x41E36 returns 1). A creature (B10,
  0x9231F: `+0x1C0 = GameTime + word`) uses the same word. A scroll and a wand skip
  it. The other readers of the record word (0x255DE, 0x28312, 0x875DE, 0xB64CC) read
  the byte at +0x00, the class, and 0x56FF6 prints it in a debug text only.
- The request flag 0x13B69F stays set while the time runs (the key need not be
  held); the hot keys refuse a change in that time (character.md, 0x43A5A).

DOS game (DTUGAME.EXE 1.3, sha256 be645b64..., in DOSBox-X 2026 with `-nosound
-notitles`, the shipped save `SG0.SG` of Elmac with the book entry of Spider Climb
changed to Armor in a scratch copy; keys sent with AUTOTYPE, the window captured
each 0.5 s or 1 s):

- Armor: "Casting time: 60 seconds ..." at 78.4 to 79.4 s of the capture, "You have
  9 armor hit points." at 138.4 to 139.4 s: 60 s (+/- 1 s).
- Shield (`cast_time=10` in the table): "Casting time: 1 second ..." at 78.5 to 79.0
  s, "You are Shielded for 5 minutes." at 79.5 to 80.0 s: 1 s, not 10.
- After that, hot key 3 (Burning Hands in the hand) and the attack key ended the
  program with a page fault about 3 s later ("Crash address (unrelocated) =
  1:000D51FF"); not examined.

The cast time of each spell (word of 0x13A8DB, game seconds, and the text of the
cast) beside `cast_time=` of the tables, which has no effect:

| # | Spell | Class, level | `cast_time=` | Seconds | Text |
|---|---|---|---|---|---|
| 0 | Abjure | cleric 4 | 60 | 7 | "7 seconds" |
| 1 | Aid | cleric 2 | 30 | 5 | "5 seconds" |
| 2 | Armor | mage 1 | 0 | 60 | "60 seconds" |
| 3 | Bless | cleric 1 | 60 | 60 | "60 seconds" |
| 4 | Burning Hands | mage 1 | 0 | 1 | "1 second" |
| 5 | Chill Touch | mage 1 | 10 | 1 | "1 second" |
| 6 | Color Spray | mage 1 | 0 | 1 | "1 second" |
| 7 | Confusion | mage 4 | 10 | 4 | "4 seconds" |
| 8 | Continual Light (Cleric) | cleric 3 | 36 | 6 | "6 seconds" |
| 9 | Continual Light (Mage) | mage 2 | 36 | 2 | "2 seconds" |
| 10 | Cure Light Wounds | cleric 1 | 30 | 5 | "5 seconds" |
| 11 | Cure Serious Wounds | cleric 4 | 42 | 7 | "7 seconds" |
| 12 | Detect Magic | mage 1 | 60 | 1 | "1 second" |
| 13 | Dispel Magic | mage 3 | 36 | 3 | "3 seconds" |
| 14 | Fear | mage 4 | 0 | 4 | "4 seconds" |
| 15 | Feather Fall | mage 1 | 10 | 1 | "1 second" |
| 16 | Fireball | mage 3 | 10 | 3 | "3 seconds" |
| 17 | Flaming Sphere | mage 2 | 36 | 7 | "7 seconds" |
| 18 | Flame Walk | cleric 3 | 30 | 5 | "5 seconds" |
| 19 | Fly | mage 3 | 0 | 3 | "3 seconds" |
| 20 | Free Action | cleric 4 | 42 | 7 | "7 seconds" |
| 21 | Haste | mage 3 | 10 | 3 | "3 seconds" |
| 22 | Hold Person | mage 3 | 10 | 3 | "3 seconds" |
| 23 | Hold Undead | mage 3 | 10 | 5 | "5 seconds" |
| 24 | Improved Invisibility | mage 4 | 1 | 4 | "4 seconds" |
| 25 | Infravision | mage 3 | 10 | 60 | "60 seconds" |
| 26 | Invisibility | mage 2 | 1 | 2 | "2 seconds" |
| 27 | Jump | mage 1 | 10 | 1 | "1 second" |
| 28 | Knock | mage 2 | 10 | 1 | "1 second" |
| 29 | Levitate | mage 2 | 10 | 2 | "2 seconds" |
| 30 | Light (Mage) | mage 1 | 10 | 1 | "1 second" |
| 31 | Light (Cleric) | cleric 1 | 24 | 4 | "4 seconds" |
| 32 | Lightning Bolt | mage 3 | none | 3 | "3 seconds" |
| 33 | Magic Missile | mage 1 | none | 1 | "1 second" |
| 34 | Magical Stone | cleric 1 | 24 | 4 | "4 seconds" |
| 35 | Magical Vestment | cleric 3 | 60 | 60 | "60 seconds" |
| 36 | Melf's Acid Arrows | mage 2 | 10 | 2 | "2 seconds" |
| 37 | Melf's Minute Meteors | mage 3 | none | 3 | "3 seconds" |
| 38 | Prayer | cleric 3 | 36 | 6 | "6 seconds" |
| 39 | Protection From Evil | mage 1 | 10 | 1 | "1 second" |
| 40 | Protection From Fire | cleric 3 | 10 | 6 | "6 seconds" |
| 41 | Rary's Mnemonic Enhancer | mage 4 | 0 | 600 | "10 minutes" |
| 42 | Resist Cold | cleric 2 | 10 | 60 | "60 seconds" |
| 43 | Resist Fire | cleric 2 | 10 | 5 | "5 seconds" |
| 44 | Scare | mage 2 | none | 2 | "2 seconds" |
| 45 | Shield | mage 1 | 10 | 1 | "1 second" |
| 46 | Sleep | mage 1 | 10 | 1 | "1 second" |
| 47 | Slow | mage 3 | 60 | 3 | "3 seconds" |
| 48 | Spider Climb | mage 1 | 10 | 1 | "1 second" |
| 49 | Stone Skin | mage 4 | 10 | 1 | "1 second" |
| 50 | Strength | mage 2 | 10 | 600 | "10 minutes" |
| 51 | Burning Hands | mage 1 | 0 | 5 | "5 seconds" |
| 52 | Hold Person | cleric 2 | 10 | 5 | "5 seconds" |
| 53 | Protection From Evil | cleric 1 | 10 | 4 | "4 seconds" |
| 54 | Detect Magic | cleric 1 | 60 | 60 | "60 seconds" |
| 55 | Dispel Magic | cleric 3 | 36 | 6 | "6 seconds" |

Spells 41 (Rary's Mnemonic Enhancer) and 50 (Strength) are the only ones above 60
seconds: "Casting time: 10 minutes ...". The port before this audit read
`cast_time=` of the line (Armor in the next frame); it now uses the words of
0x13A8DB (`spell_cast_time` in `dtuspell.c`), checked with `-testspell 2`:
the cast starts at 2.033 s and Armor acts at 62.052 s; Shield at 2.033 s and 3.066 s.

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
| 0 | Abjure | C4, target; 7 s | same; text only for the player caster | text by 0x8CEAE |
| 1 | Aid | C2, 5 s; +1d8 hp with no cap (0x8D46A), Bless (L+1) min, the Bless timer replaced; sound 0x2E900 with the sound word P+0x1B33 | hp cut to the maximum, "You feel better.", Bless not renewed when on | hp above the maximum (`dtu_char_add_hit_points`), timer renewed, the Aid text; the sound is not in the port |
| 2 | Armor | M1, 60 s; pool 8 + L, AC 6 | same | text by 0x8CEAE |
| 3 | Bless | C1, 60 s; 360 s | generic "Bless for 6 minutes." | original text |
| 4 | Burning Hands | M1, 1 s; 6 missiles, fire 1d3 + 2L, save half | same | same |
| 5 | Chill Touch | M1, 1 s; self, (L+3) min | generic text | original text |
| 6 | Color Spray | M1, 1 s; 1d6 missiles; sleep, blind or stun by level | generic texts | original texts |
| 7 | Confusion | M4, 4 s; 1d4 + L missiles; save; (L+2) min | generic text | original text |
| 8, 9 | Continual Light | C3, M2, 6 s / 2 s; area; blind, no end | "Blindness." | original text |
| 10, 11 | Cure Light / Serious Wounds | C1, C4, 5 s / 7 s, 42 s; 1d8, 3..17, at most the maximum | "You feel better." | original text |
| 12, 54 | Detect Magic | M1, C1, 1 s / 60 s; 2L min, 10 min | generic text | original text, only for the player |
| 13, 55 | Dispel Magic | M3, C3, 3 s / 6 s | same | same |
| 14 | Fear | M4, 4 s; save; L min | generic texts (Fear and Hold) | "Fear cast on %s by %s." |
| 15 | Feather Fall | M1, 1 s; L min; no end text | generic texts, "Feather Fall expired." | original text, no end text |
| 16 | Fireball | M3, 3 s; fire min(L,10) dice of 0 sides (each adds 0) + 6L | same | same |
| 17 | Flaming Sphere | M2, 7 s | same | same |
| 18 | Flame Walk | C3, 5 s; (L+1) min | generic text | original text |
| 19 | Fly | M3, 3 s; 10 (1d6 + L) min | generic text | original text |
| 20 | Free Action | C4, 7 s; self; 10L min | generic text | original text |
| 21 | Haste | M3, 3 s; (L+3) min | generic text | original text |
| 22, 52 | Hold Person | M3, C2, 3 s / 5 s; 2L min, / (wisdom/2) for the player | generic text | original text with seconds below 60 s |
| 23 | Hold Undead | M3, 5 s; (1d4 + L) min | generic text | original text |
| 24 | Improved Invisibility | M4, 4 s; (L+4) min | generic text; end "Improved Invisibility expired." | original texts, end sound 161 |
| 25 | Infravision | M3, 60 s; (6L + 12) * 10 min | generic text in minutes | original text in hours |
| 26 | Invisibility | M2, 2 s; no end | "Invisibility." | original text, end sound 161 |
| 27 | Jump | M1, 1 s; (1d3 + L) min | generic text | original text |
| 28 | Knock | M2, 1 s; nothing | same | same |
| 29 | Levitate | M2, 2 s; 10L min | generic text | original text |
| 30, 31 | Light | M1, C1, 1 s / 4 s, 24 s; 10L, 10(L+6) min | "Blindness for N minutes." | original text |
| 32 | Lightning Bolt | M3, 3 s | same | same |
| 33 | Magic Missile | M1, 1 s; min(L/2 + 1, 5) homing missiles, 1d4 | same | same |
| 34 | Magical Stone | C1, 4 s; 1d3 missiles, 1d4 (2d4 undead) | same | same |
| 35 | Magical Vestment | C3, 60 s; 5L min | generic text | original text |
| 36 | Melf's Acid Arrows | M2, 2 s; one missile, 4 L acid, save for half, no attack roll | same | the AD&D 2nd edition rule, `Dtu_fix_melf_acid` (section 10) |
| 37 | Melf's Minute Meteors | M3, 3 s; min(L, 5) missiles | same | same |
| 38 | Prayer | C3, 6 s; Bless L min | generic text | original text |
| 39, 53 | Protection From Evil | M1, C1, 1 s / 4 s; 2L, 3L min | generic text | original text |
| 40 | Protection From Fire | C3, 6 s; pool += 12L | "Protection From Fire for N hit points." | original text with the pool |
| 41 | Rary's Mnemonic Enhancer | M4, 600 s | "You have ... for 1 day." for the player only | original text by 0x8CEAE |
| 42 | Cure Blindness or Deafness | C2, 60 s | "You are no longer blind or deaf." | original texts |
| 43 | Resist Fire | C2, 5 s; L min | generic text | original text |
| 44 | Scare | M2, 2 s; (1d4 + L) min | generic texts | "Scare cast on %s by %s." |
| 45 | Shield | M1, 1 s; 5L min, AC 3 | "Shield for 5 minutes." | "You are Shielded for 5 minutes." |
| 46 | Sleep | M1, 1 s; 5L min, level 3 or less | generic text | original text |
| 47 | Slow | M3, 3 s; (L+3) min | generic text | original text |
| 48 | Spider Climb | M1, 1 s; (L+3) min | generic text | original text |
| 49 | Stone Skin | M4, 1 s; 1d4 + L/2 attacks | "Stoneskin for N attacks." for the player | original text by 0x8CEAE |
| 50 | Strength | M2, 600 s; L hours: L * 225 << 20 in 32 bits, which wraps from level 10 on (the effect ends at once) | `L*3600*F1_0` (signed overflow in C) | the 32 bit wrap of the executable, the text in hours |
| 51 | Find Traps | M1, 5 s; 30 min | generic text | original text |

## 5. The cast (0x41A4D, 0x9305D)

| Rule | Executable | Port before | Port after |
|---|---|---|---|
| Cast time unit and text | seconds of game time, texts of section 1 | same | same (the log line has the text) |
| Refusal of 0x356E0 and of the armor test | sound 3000, volume 1.0 (0x41B0D, 0x41C2D), then the text | text only | sound and text |
| "You cannot cast a spell while climbing." | player +0x1AE bit 0x80: Levitate (0x8EDB2) or the climb of Climb Walls (0x9531D, 0x95413); no sound | Levitate only | Levitate or the climb |
| Interruption 0x9305D (player) | clears the request 0x13B69F and 0x13B6A3, arms state 8 (flinch) at once | the request stayed: the next frame started the cast again (a new "Casting time" text, no slot used) | request cleared (`dtu_combat_cast_stopped`), flinch |
| Texts for a cast between two creatures | 0x8CEAE | none | 0x8CEAE with the segments of the last rendered frame as the visible list (guess for 0x38C060) |

## 6. Kept differences

Settings (combat.md C3), not changed: `Dtu_fix_stoneskin_word`,
`Dtu_fix_rary_manual`, `Dtu_fix_wizardry_ring`. New: `Dtu_fix_chill_touch` (section 8),
`Dtu_fix_melf_acid` (section 10).

Not in the port (combat.md C2): the view of the player in the gated creature of Fear
and Scare, the blindness and deafness of Fireball within 6.0, the global counters
Spell_info+0x49 of Haste, Hold Person, Slow and the hit dice pool of Hold Undead (the
pool of Sleep is in the port, section 8), the `cast_on_top` vclips (0x8CCED), the light +0x1A6 of the blindness end, the
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

## 8. The 1.3 rules of the second audit (Code)

**Caster level (0x91EC1).** One function for a spell of a book, a scroll and a
wand (callers 0x56D4E, 0x57054, 0x8D1C4, 0x8D232, 0x90D01). c = the level of a
cleric class (class byte 0), o = the level of the other class (class bytes 1 to 3,
the second class over the first), 0 if none. cleric_use and mage_use: max(c, o) +
1; cleric_use: c + 1; else o + 1. So a fighter or a thief casts a mage wand or
scroll at the level of the class (a fighter/thief: the thief level); a character
with a mage class always gets the mage level (the class order of the creation is
fighter, thief, mage, cleric). The port used level 1 without a mage class. Checked:
a fighter of level 4 with the Wand of Magic Missiles casts at level 4, 3 missiles
(before: level 1, one missile).

**cast_in_front (0x253C7).** The reader sets +0x37 |= 5 for `cast_in_front` and
|= 1 for `self_cast_ok` (0x25535); no code tests bit 2. 0x56D36 (player) and
0x92104 (creature) test bit 0: the spell acts on the caster, an AREA line around
the caster (0x90CEF, which does not skip the caster; the cases of the lights
skip the caster themselves). The lines 8, 9 Continual Light, 13 Dispel Magic, 30,
31 Light have `cast_in_front` in 40 of the 46 tables (not in MINES1, PRISON1,
SPIDER, TRAIN1, DEMO, MOIN1). So the mage's Dispel Magic ends all effects of the
caster (also the bad ones: blindness, Slow, Fear); kept as in 1.3. Checked: Light
blinds three creatures around the caster, Dispel Magic ends Bless and Protection
From Evil of the caster.

**Prayer (0x56D6A).** Before the test of the AREA bit the player's cast writes the
word +0x36 of spell 38 as 0x102 (AREA bit clear, self bit set): Prayer blesses
the caster only (0x8CFFF). The line is TARGET in every table, so the creature's
cast (0x92104) also reaches only the caster. The port cast it as an area.

**Sleep (0x8F766, the pool 0x5742C).** 0x56FF6 sets Spell_info[46]+0x49 to
4 + 2 (rand() % 7) (4 to 16, even) at each cast. After the tests of 0x8F766 (not
the caster, not asleep, not undead or no_sleep, T <= 3, the elf d10 <= 9, the
half-elf d10 <= 3, a creature with Robot_info+0xD9 bit 3 d10 <= 9) the cost is
max hit points >> 2 for a creature (Robot_info word +0x78), 2 T for the player.
A cost above the pool: no effect, no text. Else the pool goes down by the cost.
The port had no pool and no d10 of the creatures with bit 3 (the names rf, rm, av,
dro: the human and drow fighters and casters). Checked: pool 4, the Giant Rat
(max 4 hit points) costs 1, 3 left.

**Chill Touch (`Dtu_fix_chill_touch`).** No table has `self_cast_ok` on line 5,
so the cast fires a missile; the case 0x8D633 returns unless the target is the
caster (and the effect bit 3 is clear), so in 1.3 the spell does nothing. The
effect: bit 3 of +0x88, timer +0x9C, (L + 3) minutes. A melee hit of the holder
(0x29E3D player, 0x2A49B creature) makes the target save (category 4); on a
failure an undead target gets Fear for 1d4 + L minutes (L of 0x91EC1 for spell
5), another target 1d4 more damage. Corrected: the cast is on the caster. Checked:
"You have chill touch for 4 minutes." at level 1; a dagger hit on a skeleton that
failed its save: Fear for 180 s.

**Detect Magic (0x670D4, 0x7569F, the texture mappers).** The effect bit 6 of the
viewer (0x670D4) or of the player (0x75726) gives a tint value 0x40 at 0x14C4CD
with 0x14C4D0 = 15 for:

- an object for which 0x670D4 returns 1: a spell item (type 17); armor with
  `magic` (0x19FD34 bit 0, 0x23392); a misc item with the flag 0x10 of 0x1A344 (a
  plot item, '+' in the name, or one of the name parts uring, yrkul, rystal,
  carab, speed, nsight, anegem, ursed, lamesword, otion); scenery with `magic`
  (0x19B340 bit 0); a weapon object whose type has W+0x3E bit 0x10 (`magic`, and
  the spell missile templates, which 0x571B2 sets at each cast). A creature gives
  2 (weapon type below 0 or magic): the model then tints the submodels with flag
  0x20, which no model file has, so creatures show nothing. Draw sites: the
  polygon model 0x67995 (cleared to 0 at 0x67A0A after the model), the rod
  0x67347 and the bitmap 0x671E2 (both never clear it).
- a wall side (0x75707 to 0x75827): side byte 1 bit 0x20 (no level has it), a
  segment of special 2, 3 or 6 (special 2: 31 segments in 22 levels), or an
  illusion wall that is on (wall type 3, no flag 0x20) and draws as an illusory
  wall (WALL_IS_DOORWAY 3): the false walls. The side saves the tint and gives it
  back after it.

The pixel rule of the perspective mapper (0x105C60 setup, 0x105BC8 to 0x105C35):
a lit texel c that is not 255 or an effect color gets fade[row][(c & 15) | 0x20],
the blue ramp at the brightness step of the texel, still lit. The linear copy
(0xEC6BC, 0xEC4D7) is broken: its setup clears 0x14C4E2 for each scanline and
sets it again only when 0x14C4CD changed since the last scanline, and its rule
uses row 0 and the test c & 0x1F <= 15, so only the first scanline after a change
gets near black texels; with the tint on, all its other texels take row & 15.
1.3 draws models (not the viewer) with the linear mapper (bugs.md D12), so in the
DOS game Detect Magic shows mostly on nearby items (rods), walls and missiles.

Port (`Dtu_fix_magic_tint`, `-origobj` clears it): the tint of 0x14C4CD is
`Tmap_tint` (texmap/tmapfx.c) and `G3_gpu_tint`; both renderers draw the blue
ramp for every lit texture of a tinted object or side. Corrected: the bitmap and
the rod give the tint back after the object (1.3 leaves it on for the next
polygons until a model clears it), and the linear mapper uses the rule of the
perspective one. With 0 the port keeps the leak and the scanline rule of the
linear mapper. Not done: the effect colors of a tinted model (the tint turns on
0x14C4D8). Checked: the Wand of Magic Missiles on the floor of BANE turns blue in
the GPU and the software renderer, the dagger in the hand does not.

In player terms: while Detect Magic lasts, magic things in view are drawn in
shades of blue: spell scrolls, magic weapons, magic armor, potions, keys and
other quest items, magic scenery, and spell missiles in flight; and the false
(illusory) walls that can be walked through, so they can be found. Creatures and
normal items keep their colors. Duration: 2 minutes per level (mage), 10 minutes
(cleric version).

## 9. The spell lists (0xB65BB, 0xB75A8)

- The insert 0xB65BB (pickup, purchase, gifts, the new character): a spell (type
  17) with the cleric bit for a cleric or the mage bit for a mage goes to that
  list (0xB69AD, 0xB660C to 0xB667F); result 1 ends the insert, result 2 (already
  in the list) puts it into the item list as a scroll. The port put every spell
  into the item list, so the spells of a new mage were scrolls.
- 0xB75A8(L): every spell with the cleric bit whose level byte (+0x39) is L + 1
  goes to the cleric list (not twice, at most 127). L must be 0 to 4.
- The new character (0x8BDB4) and each new level (0xAF8F5, the end of 0xAF694):
  a cleric gets 0xB75A8(0x145ED8[k]) for k = 0 to the cleric level (0 based). The
  table is 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 5, 6...: spell level 1 at the
  cleric levels 1 and 2, 2 at 3 and 4, 3 at 5 and 6, 4 at 7 and 8, 5 at 9 and 10.
  The port gave a new cleric spell 0 (Abjure, a level 4 spell) as an item and
  nothing at a new level.
- `Dtu_fix_spell_save_repair` (dtuchar.c, `-origchar` clears it): at the first
  level start after a load, the learnable spells of the item list that a list has
  not go to it (one item each), and a cleric gets the cleric spells of the levels.
  A 1.3 save has nothing to change; it repairs saves of older versions of the
  port.
- In player terms: a cleric knows every cleric spell of each spell level that the
  character can cast: level 1 spells from the start, level 2 at cleric level 3,
  level 3 at 5, level 4 at 7. Spell slots per level are unchanged.

## 10. Melf's Acid Arrows by the AD&D 2nd edition rule (`Dtu_fix_melf_acid`)

1.3 (0x8EFE0, checked in the Ghidra export of 0x8CFFF: the case writes
`#acid_damage_dice` = 2 (L/3) + 2 and calls 0x911AA, with no attack roll): one
missile, 4 L acid damage in one hit (the dice have 0 sides and add 0), a save for
half, acid ends regeneration. The missile lives (L/3 + 1) minutes (0x56FF6). The game
has no text of the spell and the manual tells little, so the port follows the 2nd
edition rule. `-origcombat` gives the 1.3 rule.

- One arrow (`missile_count` is 1 for line 36 in both rules).
- The attack roll: d20 against the THAC0 of a fighter of the caster level (21 - L, the
  fighter row of the table 0x146590, 20 at most, 3 at least) and the armor class of
  the target, through `dtu_hit_test` (dtuobj.c, the port of 0x2AA5B to 0x2AF4F): the
  effects of the attacker and of the target count (Bless, Slow, Levitate, Protection
  From Evil, an unseen target -4, the armor class spells of a creature, and Stone Skin
  takes the hit). No strength, dexterity, weapon or range modifier. A natural 1 misses,
  a natural 20 hits. A miss does nothing; the player gets "You miss %s with %s." or
  "%s misses you with %s." (new port texts, es_port.catalog).
- A hit: 2d4 acid at once, then 2d4 at the start of each of the next L/3 rounds
  (`ACID_ROUND` in dtuspell.c, 60.0 seconds; one constant so that the length can
  change). Levels 1-2 one round, 3-5 two, 6-8 three, 9-11 four. No save. Each round
  takes the acid part of 0x91BC3 (the regeneration bit 24 ends; no table has an acid
  immunity or resistance) and the rest of 0x911AA (Invulnerability of the player, the
  Armor pool, the sound and vclip, the damage texts, the interruption of a cast).
- The later rounds are a list of the port (`Acid` in dtuspell.c, 32 entries) by object
  number and signature. The acid ends when the target dies or goes, at the level
  start, and when the game time goes back (a load). It is not in the level state file
  (the object record of 1.3 has no room for it): a save and load ends the acid.

Check (`-hidden -mute -fixedfps 30 -noattack`, BANE, `-char SG0.SG` (thief/mage),
`-testslots 5 -testspell 36 -testmove wait:30,attack:2`, the mage level by
`-chartest 1:xp=N`; `-objlog` lines `spell:`):

- Level 9 (`xp=300000`) at the large skeleton (obj 14, 72 hp, armor class 0, `-testpos
  -135,17,35,0`): d20 15 against THAC0 12: hit; round 1 of 4 at 11.20 (4), round 2 at
  71.22 (4), round 3 at 131.20 (6), round 4 at 191.22 (7), then the end.
- Level 6 (`xp=80000`), same target: d20 18 against THAC0 15: hit; rounds 1 to 3 at
  11.20, 71.22, 131.20 (6, 4, 4).
- Level 3 (`xp=10000`): at the large skeleton d20 5, 12 and 2 against THAC0 18 miss
  (armor class 0); at the skeleton of `-testpos 25,-1,215,0` (8 hp, armor class 7) d20
  12: hit, round 1 of 2 (5), round 2 at 71.22 (7, the skeleton dies, the acid ends).
- Level 9 at that skeleton: round 1 (4), round 2 at 71.22 kills it; the next frame
  logs that the acid ends after round 2 of 4.
- A level 1 mage does not cast the spell of level 2 (no cast starts). Caster level 2:
  a drow mage (DROW1 obj 13 `rmmk5`, `-testpos 105,-3,10,0` and others, no
  `-noattack`) casts it at the player of armor class 6: d20 17 against THAC0 19: hit,
  round 1 of 1 (5); in another run a natural 1 misses.
- `-origcombat`, the large skeleton: level 9 "8 dice with 0 sides add 0", the save
  succeeds, 18 damage in one hit; level 3 the save fails, 12 damage.

## 11. Wizard spells of the third audit (Code)

The wizard spell audit is `docs/audit/spells-wizard.md` (each spell with its case
address and its rule after the table fixup).

**The table fixup 0x1A344 is the 1.3 rule.** After each table read 0x1A344 writes
fixed values into the spell records: names, class masks, levels, flags, range,
radius and dice (D:\dev\DTU-Decomp `analysis/sweep/range1-gaps.md`, gap G1 and its
appendix). Section 1 describes only its last step, the cast times. The port reads the
table text; an override layer under `bm_tbl_get` ports the rest. The 1.3 rules of the
wizard spells after the fixup:

- **Fireball (0x8E55C) and Lightning Bolt (0x8EE76).** The fixup sets the fire dice of
  spell 16 and the electric dice of spell 32 to 6 sides, 1 die, 0 per level. The cases
  write min(L, 10) into the dice count (record +0x3E, 0x1B56B4; +0x47, 0x1B56BD), so
  1.3 rolls Ld6, at most 10d6, a save for half: the AD&D rule. The table text alone
  (0 sides, 6 per level) gives 6L. Fireball is AREA with radius 40 (flags 0x803).
- **Magic Missile (0x8EED5).** The case passes level 1 in ECX (0x8EEEE) to 0x911AA. The
  fixup sets dice 4 sides, 1 die, 1 per level: 1d4 + 1 for each missile, a save for
  half. `damage_bonus=` of the line goes to a word 2 bytes before the record (the
  reader 0x242F6) that no code reads; the fixup gives the +1 instead.
- **Melf's Acid Arrows (0x8EFE0).** The fixup sets acid 4 sides, 2 dice, 0 per level;
  the case writes 2 (L/3) + 2 into the count: (2 (L/3) + 2)d4 in one hit, a save for
  half. Section 10 calls the 1.3 rule "4 L" from the table text; that is not the game.
  `Dtu_fix_melf_acid` (the 2nd edition rule over rounds) stays; its `-origcombat` path
  gives the 1.3 rule once the override layer is in.
- **Melf's Minute Meteors (0x8F021).** Fire 4 sides, 1 die: 1d4 fire for each of
  min(L, 5) meteors, a save for half.
- **Knock.** Flags 2: no cast on the caster, so the spell fires a missile, and the wall
  hit 0x26C2F calls the door function 0x7E859 with unlock 1. Checked with a test patch
  that fires the missile: BANE, wall 84 (seg 488 side 4, a locked door), "Knock hits
  seg 488 side 4". The table text (`self_cast_ok`) casts it on the caster: nothing.
- **Chill Touch.** The fixup sets the cast on the caster (flags+1 |= 1). So 1.3 casts
  it on the caster; `Dtu_fix_chill_touch` (section 8) gives the 1.3 result by default,
  and its `-origcombat` path gives a missile that 1.3 never had.
- **Area spells.** After the fixup Fear (radius 90), Haste (80) and Feather Fall (20)
  are AREA around the caster (flags 0x103); Light (3), Continual Light (3), Dispel
  Magic (60), Hold Person (40), Hold Undead (80), Scare (30), Sleep (60) and Slow (80)
  fire a missile and act in an AREA where it lands (flags 3 or 0x803). The mage's
  Dispel Magic is then no longer cast on the caster (section 8).
- **Spells 17 and 51** become the cleric spells "Produce Fire" (level 4, fire 1d4 + L,
  AREA radius 24) and "Find Traps" (level 2). They are not wizard spells.

**Strength and Infravision (`Dtu_fix_spell_hours`, a defect).** 0x8FF5D computes
L * 225 << 20 (L hours) and the Infravision case (L + 2) hours in 32 bits. From caster
level 10 (Strength) and 8 (Infravision) the value wraps below 0 and the effect ends at
once. The port also reads a duration of 0x3FFFFFFF (16383 s) or more as no end, so at
levels 5 to 9 (Strength) and 3 to 7 (Infravision) the effect never ended. Corrected:
no wrap, and the duration stops at 16383 seconds (4 hours 33 minutes), the longest
timer below the no end value. `-origcombat` clears it. Checked: `-origcombat`
Infravision L3 "no end", L8 -29536 s; corrected 16383 s for both.

Kept differences from the manual and AD&D 2nd edition: Magic Missile has a save for
half and one more missile at levels 2, 4, 6 and 8 (0x56FF6); Melf's Minute Meteors
at most 5 meteors with no attack roll; Scare affects level 6 (the manual: level 6 or
more is safe); Sleep spares level 4 and up.

The test aid `-chartest FRAME:spelltest=SPELL.LEVEL.OBJ.TRIALS` (`dtu_spell_test`):
the player casts SPELL at caster LEVEL on object OBJ (-1: the player; a spell cast on
the caster acts on the player), with each missile of 0x56FF6 and the area of an AREA
line, and no cast time, slot or flight. The `-objlog` lines `spelltest:` give the hit
points, the effects and their times. TRIALS 1 or more puts the target and the
character back after each trial and prints the mean damage; 0 keeps one cast, so
the later rounds of acid run. The player cannot be the target of a spell that skips
the caster (Hold Person, Sleep). Rary's Mnemonic Enhancer opens its choice box.
## 12. Priest spells (third audit, Code)

The audit table is [docs/audit/spells-priest.md](../audit/spells-priest.md).
After the table fixup 0x1A344 the cleric spells are 0, 1, 3, 8, 10, 11, 17
(Produce Fire), 18, 20, 31, 34, 35, 38, 40, 42 (Cure Blindness or Deafness,
level 3), 43, 51 (Find Traps, level 2), 52 to 55 (copies of 22, 39, 12, 13).
Rules read from the code, not given in sections 4 and 8:

- Aid (0x8D3F7): 0x2E900 plays the word P+0x1B33 at the position of the
  player object (0x30DF14), volume 1.0, also when another object is the target.
  The reader 0x1F495 stores `happy_sound` of the player type at P+0x1A53+0xE0 =
  P+0x1B33. The cures (0x8DEA8, 0x8DF97) play it only when the target is the
  player.
- Cure Light and Cure Serious Wounds (0x8DE65 to 0x8DEEF): shields += points,
  then shields = the whole maximum if above. So a cure takes away the hit points
  of Aid above the maximum. Port: `Dtu_fix_cure_aid` keeps them.
- Light and Continual Light (0x8DC1D): blind, `blind` type and the caster
  return; a made save returns (0x8DC54) before the blindness and before the
  damage of a `light_sensitive_2` creature (Robot_info+0x98 bit 0x200, 0x8DD5C). A
  blinded player: 0x13D50C, 0x1F3B00, 0x1F3AF8, 0x46C688, 0x46C68C = 0, and the
  palette add values 0x1C7F90 to 0x1C7F98 += 254, at most 30 (a white flash).
- Free Action (0x8E6F2): returns if bit 12 is set, then if the target is not
  the caster; with Hold the Hold timer becomes 0 and +0x1DB = GameTime (the
  end comes from 0x9002C in the next frame).
- Hold Person (0x8E85A): bits 12 or 14 set, the caster, an undead creature and
  a made save return; then only a humanoid creature or the player; then the
  counter Spell_info+0x49 (a byte) must be above 0 and goes down by one. 0x56FF6
  sets it to 1d4 at each cast, the lich touch (0x2A670) to 1. Player:
  +0x176 = 1, 0x13B6A3 = 0x13B69F = 0, 0x40E50 (input flush), 0x14530D = 0.
  Creature: byte 0x153EB5 + 63 objnum = 0.
- Magical Stone (0x8EEFE): Spell_info[34]+0x3B (die count) = 2 against an
  undead creature, else 1, then 0x911AA.
- Protection From Fire (0x8F1B2): returns while bit 9 is set; the pool word
  +0x82 += 12 L.
- Cure Blindness or Deafness (0x8F2FA): needs bit 2 or 4. Blind: timer 0x98 = 0
  (bit 2 stays until 0x9002C ends it with "Blinded expired."). Deaf: bit 4
  cleared, and the player gets the volume (byte 0x13BFEB << 15) / 8 (0x2DF7E).
- The end of Bless (0x900A1): the player is cut to the whole maximum (P+0xA1)
  and P+0x53 written; a creature is cut to Robot_info+0x78.
- Abjure (0x8D297): the creature velocity is set along the line to the player
  scaled to 10.0 and then overwritten by the spin (0, 0.05, 0) with movement
  type 3. The sound of a creature sent away has volume 1.0 (0x2E900 cuts to 0.5).

Turn undead (0xADC46, see bugs.md section 2): a turn takes one of the 2 to 12
(0xAE226 jumps to 0xAE17F, `dec` at 0xAE1A0); the T entry writes 0xFF over the
d20 at esp+0x2C4 (0xAE1B8) and 0xAE1C8 reads it as an unsigned byte, so after a
T every undead with a number is turned (`Dtu_fix_turn_undead` keeps the d20);
the jump table 0xADC36 sends -4 to 0xAE0DC, -3 to 0xAE14C, -2 to 0xAE158, -1 to
0xAE1AC. An undead without a name match gets the class max(class levels) + 1,
8 from level 7 (0xAC4C1).

Checks: `-chartest 30:priestcheck` and `tools/priestcheck.py` (23 checks).
