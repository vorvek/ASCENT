# Spell ranges

New in the port: the setting `Dtu_fix_spell_range` (`engine/main/dtuspell.c`,
on by default, `-origcombat` clears it). It gives each spell that flies as a
missile the range of the AD&D 2nd edition Player's Handbook, for the cast of the
player and of a creature.

## The range in 1.3

- A spell missile gets the life 30.0 seconds (0x56FF6 writes `lifetime`
  0x1E0000 to the template) and the speed of the spell line (`speed=`, default
  100.0; no spell line has the key). So it flies 3000 units.
- Lightning Bolt (32) gets 2.0 seconds and bounces; Melf's Acid Arrow (36)
  gets (L/3 + 1) minutes (0x56FF6 cases 0x20 and 0x24).
- `range=` and `level_range=` of the spell lines (Spell_info +0x06 and +0x0A)
  have no reader. Their values are the ranges of the book in units for most
  spells (Hold Person 720, Fireball 60 + 60 a level), but no code uses them.

## The scale

Two units of the game are one foot. Evidence: the fall damage of 1.3
(0x6C8C8) computes `fell = (start_y - pos.y) >> 17`, that is units / 2, and
prints "You fell %d feet!". The description window uses the same scale
(`feet()` in `dtudesc.c`). The book gives yards: 1 yard is 3 feet.

## The port

`Spell_range[]` in `dtuspell.c` has the range in feet and the feet for each
caster level. The life of the missile is the range divided by the speed
(`spell_missile_life`); the caster level is the level of the cast (the player
level of the spell book, the level of a creature). With the setting:

- an area spell (`type=AREA`) takes effect where its missile ends, as at a wall
  (`dtuobj_missile.c`); a missile with one target goes away;
- a creature does not choose a missile spell whose range is shorter than its
  distance to the player (`creature_choose`);
- the description window shows the range at the caster level of the player
  (`spell_flight`).

Spells cast on the caster (Aid, Bless, Haste, Protection From Evil, ...) and
Chill Touch (touch, cast on the caster with `Dtu_fix_chill_touch`) keep their
rules. Burning Hands has the arc of `Dtu_fix_burning_hands`; the missiles of a
creature that casts it keep the 1.3 range.

## The table

Source: Manual del Jugador, AD&D 2nd edition (Spanish edition), appendix 3
(wizard spells, pages 175 to 200 of the PDF) and appendix 4 (priest spells,
pages 254 to 284). L is the caster level.

| Spell | 1.3 range | Book range | Range of the port |
|---|---|---|---|
| 0 Abjure (priest 4) | 1500 ft | 10 yards | 30 ft |
| 6 Color Spray | 1500 ft | 0, a wedge 20 ft long | 20 ft |
| 7 Confusion | 1500 ft | 120 yards | 360 ft |
| 8 Continual Light (priest) | 1500 ft | 120 yards | 360 ft |
| 9 Continual Light (wizard) | 1500 ft | 60 yards | 180 ft |
| 13 Dispel Magic (wizard) | 1500 ft | 120 yards | 360 ft |
| 16 Fireball | 1500 ft | 10 yards + 10 yards/L | 30 + 30 L ft |
| 17 Produce Fire (priest 4) | 1500 ft | 40 yards | 120 ft |
| 22 Hold Person (wizard) | 1500 ft | 120 yards | 360 ft |
| 23 Hold Undead | 1500 ft | 60 feet | 60 ft |
| 28 Knock | 1500 ft | 60 yards | 180 ft |
| 30 Light (wizard) | 1500 ft | 60 yards | 180 ft |
| 31 Light (priest) | 1500 ft | 120 yards | 360 ft |
| 32 Lightning Bolt | 100 ft, with bounces | 40 yards + 10 yards/L | 120 + 30 L ft |
| 33 Magic Missile | 1500 ft | 60 yards + 10 yards/L | 180 + 30 L ft |
| 34 Magical Stone | 1500 ft | thrown up to 30 yards | 90 ft |
| 36 Melf's Acid Arrow | 3000 (L/3 + 1) ft | 180 yards | 540 ft |
| 37 Melf's Minute Meteors | 1500 ft | 70 yards + 10 yards/L | 210 + 30 L ft |
| 44 Scare | 1500 ft | 30 yards + 10 yards/L | 90 + 30 L ft |
| 46 Sleep | 1500 ft | 30 yards | 90 ft |
| 47 Slow | 1500 ft | 90 yards + 10 yards/L | 270 + 30 L ft |
| 52 Hold Person (priest) | 1500 ft | 120 yards | 360 ft |
| 55 Dispel Magic (priest) | 1500 ft | 60 yards | 180 ft |

No spell had a 1.3 range that agrees with the book. The `range=` values of the
lines agree with the book except Abjure (120 units, 60 ft), Color Spray (240),
Confusion (240) and the priest Dispel Magic (720, a copy of the wizard line).
The PDF pages of the entries: Sleep 175, Burning Hands and Light 179, Color
Spray and Magic Missile 181, Knock 183, Melf's Acid Arrow and Scare 186,
Continual Light 188, Fireball 192, Dispel Magic 194, Hold Undead and Hold
Person 196, Melf's Minute Meteors 197, Slow 199, Lightning Bolt 200,
Confusion 203; priest: Light 259, Magical Stone 260, Hold Person 266, Dispel
Magic 272, Continual Light 274, Abjure 278, Produce Fire 284.

## Check

A run with `-objlog` logs the life of each missile of the player
("spell: missile obj N of 'Hold Person', speed 100, life 7.20"). The wands of
YPINN at player level 9 (2026-11-26): Hold Person 7.20 s (720 units, 360 ft),
Fireball 6.00 s (300 ft), Magic Missile 9.00 s (450 ft), Lightning Bolt 7.80 s
(390 ft), Confusion 7.20 s; with `-origcombat` 30.00 s each and 2.00 s for
Lightning Bolt.
