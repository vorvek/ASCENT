# The description window

New in the port. DTUGAME.EXE 1.3 shows only the name of an item in the
description bar of the inventory screen. The description window shows what
an item or a spell does. The code is `engine/main/dtudesc.c`.

## Opening and closing

| Place | Mouse | Keys |
|---|---|---|
| Inventory screen: an entry of the list, a slot of the paper doll, a hot key box | right click | `/` for the entry or the spell that comma and period choose |
| Spell lists of the inventory screen (the SPELLS button) | right click | `/` |
| Market: a line of the stock or of the goods of the player | right click | none |
| Interface screens: the experience field; statistics screen: a class line | right click: the next level window (`dtuxp.c`) | none |

A key or a click of either mouse button closes the window. The press of the
button goes to the window and the play screen sees only the release, so the
click does not act on the screen behind the window. In the game the window
stops the game time, as the scroll text screen does.

A right click opens no window while the cursor holds an item or a spell,
while a button or a scroll bar is held, on an empty cell, in the immersive
mode, and while the conversation window or the scroll text screen is open.
A new level and a loaded game close the window (`dtu_desc_reset`).

Why the right button: 1.3 reads any mouse button as a click of the
interface (0x2FB10 tests the button mask 0x1E9C70, and the front end feature
loop reads the same mask). So in 1.3 the right button is a second left
button. The port reads only the left button for these clicks, so the right
button had no use on these screens. In the play screen the right button is
the Activate button of the key table, but the mouse items other than Attack
and Immersive Mode act only in the immersive mode (0x52E19), and the
inventory screen has no mouse cursor in that mode.

The gamepad has no way to choose an entry of the inventory screen, so the
window has no gamepad button. The catalog of the market has no keys in 1.3.

## Look

The window is the picture of the scroll text screen: FEBGND 152 in the
320x200 interface, FEBGND 151 in the 640x480 interface and in the market
(`dtu_scroll_box` of `dtuscroll.c`). It is centered on the screen. The text
is in the text box of the scroll text screen; in the high resolution box it
starts a twelfth of the width further right, past the shadow of the left
roller. The upscaled picture of the HD interface pack shows where the frame
has the pixels of the picture (`dtu_hdui_blit_key`); the text is 8-bit.

A short text is in the middle of the height of the box. A long text starts
a tenth of the height below the top of the box, or higher if it needs the
room (`draw_text_box`). A text that is too high for the box loses the pixel between its lines, then
the gap after the name (`spacing`).

Fonts: the game font in the game, the small font of the front end
(FNTEMFS_.FNT) in the market. Colors of the scroll text: the name yellow
(~c7), the other lines white (~c1), each with a shadow of color 0. In the
market the scroll gets the closest colors of the palette of the market
picture.

## Layout

The first line is the name as the player sees it (the corrected names of
`dtuitemfix.c`, in the language). The other lines come in this order. A
line with no value is left out.

- Weapon: damage dice and bonus (0 sides: only the bonus, as the Flame
  Tongue), fire dice, to hit, then "Melee weapon" with the time from one
  attack to the next, or thrown or arrow or bolt or missile with the range
  and the time from one shot to the next, the count of a stack, magic or
  not, classes (none for a weapon of the creatures), then the powers: armor
  class in the hand, the effects in the hand, the skills, and the special
  rules of Flame Tongue, the Mace of Disruption and the Dagger of Impaling.
  The times are the ones of the weapon alone: Haste or Slow count only if
  the weapon itself gives the effect in the hand. A missile whose table line
  has no life of its own shows no range. The time of a shot shows only with
  the setting Dtu_fix_missile_rate.
- Wand (a weapon with `pc_spell` that is not melee): charges, classes, the
  spell of each use.
- Bow and crossbow: the bonus of the arrows, or of the bolts and the thrown
  weapons (the crossbow slot serves each missile weapon with no `arrow`
  flag, 0x2AB0A), slot, classes, the effects in the slot. No line for magic:
  no code reads that flag of a misc line.
- Armor, shield and worn items: armor class (body armor: the value, 10 plus
  the table value; other items: how much better), slot, classes, the effects
  while worn, the powers (abilities, skills, the rules of the rings and the
  cloaks of protection).
- Potion, scroll and other usable items: count (charges for a Spider
  Piece; none for a scroll, whose count is the number of its text, and none
  for the Gem of Curing, the Cursed Scroll and the Orb of Insight, which are
  not used up), classes, one line for what one use does.
- Key items: "Key.". Plot items: they cannot be dropped or sold. Other
  items: "Item.".
- A spell in the item list: charges, classes (`dtu_spell_use_classes`), the
  spell of each use.
- Spell: class and spell level, cast time (of a cast from the book; a scroll
  or a wand casts at once), range (the flight of the missile, or "Self"),
  area, missiles (if more than one), duration, saving throw, the caster
  level (only when a value depends on it), the effect at that level, and
  the highest damage dice or missile count if the spell has a limit that
  the caster level has not reached (Fireball: up to 10d6).
- Market: the price last ("Price" when the player buys, "Sale price" when
  the player sells), with "each" for a stack.

A healing value is a range ("Heals 6 to 27 hit points") where the code
takes one random number (`rand() % 22 + 6`, 0x37A6A), and dice ("1d8+1")
where the code rolls dice. No value is shown as dice that the code does not
roll as dice.

Distances are feet: 2 units of the game are 1 foot, as in the rules wiki.
Times are game seconds: seconds below a minute, hours for whole hours, else
minutes.

## Where the values come from

The window reads the values that the game code reads where the code has
such a value:

- `dtu_item_get` and `bm_tbl_get` of the table line, after the table fixup
  (`dtutblfix.c`) and the "Rulebook items" setting (`dtuitemfix.c`).
- `dtu_char_misc_use` (`dtuchar.c`): the table of the uses of the misc items.
  `use_misc` reads the same table for its values, rolls and times.
- `dtu_char_item_slot`, `dtu_char_item_ac`, `dtu_char_item_fx`,
  `dtu_char_item_powers`, `dtu_char_lockpick_bonus` (`dtuchar.c`): the slot
  rules, the armor class of a slot, the effect bits of the worn items and
  the values that `dtu_char_update` sets. `dtu_char_update` and
  `worn_item_bits` use the same helpers and constants.
- `dtu_spell_get_facts` (`dtuspell.c`): the duration (`spell_time`), the
  amount (`spell_amount`), the damage dice (`spell_damage_args`), the
  missile count and the armor class caps (`effects_ac`), which the cases of
  the effect function call; and `Spell_rule`, the kind, the saving throw
  and the effect bit of each spell. `apply_spell` reads the line of its
  spell: it sets and tests the effect bit of the line, rolls the save of a
  case only when the line has one, and a damage spell saves for half when
  its line says so. The amount also gives the targets of Hold Person, the
  highest level of a target of Hold Undead, Scare and Sleep and the later
  rounds of Melf's Acid Arrows; the duration gives the three times of
  Color Spray (`color_time`); `DTU_FIREBALL_BLIND_DIST` and
  `DTU_FIREBALL_BLIND_SECONDS` (`dtucombat.h`) give the blindness and the
  deafness near the point of Fireball.
- `dtu_missile_weapon_range` (`dtuobj.c`), and `dtu_atk_state_time`,
  `dtu_atk_melee_wait` and `dtu_atk_missile_wait` (`dtuatk.c`) with the
  `fire_wait` of `Weapon_info[]`, as `dtucombat.c` calls them.
- `dtu_char_dragon_minutes` and `dtu_char_spider_turn_level` (`dtuchar.c`),
  `dtu_spell_use_classes` and `dtu_spell_fx_ac` (`dtuspell.c`).
- The conditions of each line of `Spell_rule` (`DTU_SR_` of `dtuspell.h`):
  the spell spares its caster, undead only, no undead, humanoids only, the
  level limit (`max_level`, from `spell_amount`), the save only for some
  kin (Scare), the effect that stops the spell (Shield stops Magic
  Missile), the blindness near the point (Fireball). The cases of
  `apply_spell` test them through `rule_reaches`, `dtu_spell_area` reads
  the blindness flag, and the creature spell choice reads the effect that
  stops Magic Missile. The window prints "Also hits you when you are in
  the area." for an area that lands and does not spare the caster, the
  level of the limit, and "Stops Magic Missile." from these flags.
- The save conditions: `save_level` (Color Spray: a target above the
  caster level or of level 6 or more; Hold Undead: above level 2) and
  `DTU_SR_SAVE_KIN`. `rule_save` applies them, and the facts give "Some
  targets" for a line that has one. The table has no `DTU_SS_SOME`.
- The messages of the casts give the minutes of the time that the case
  set (`dur`, from `spell_time`), and the life of the light object is the
  time of the blindness of Light. No message has a typed formula.

## One source

A rule that the window states lives in one place, and the game and the
window both read it there:

| Rule | Where it lives | Read by |
|---|---|---|
| Effect bit, save kind, sentence kind | `Spell_rule` | `apply_spell`, `rule_save`, `spell_damage`, `dtu_spell_get_facts` |
| Caster spared, undead only, no undead, humanoid, level limit, stopped by | `Spell_rule` flags, `stopped_by` | `rule_reaches` (the cases), creature spell choice, the facts |
| Who saves | `save_level`, `DTU_SR_SAVE_KIN`, `kin_saves` | `rule_save`, the facts (`DTU_SS_SOME`) |
| Blindness near the point | `DTU_SR_BURST_BLINDS`, `DTU_FIREBALL_BLIND_*` | `dtu_spell_area`, the facts |
| Duration | `spell_time`, `color_time` | the cases, their messages, `light_object`, the facts |
| Amount, level limit, targets, acid rounds | `spell_amount` | the cases, `rule_reaches`, the facts |
| Damage dice, undead die | `spell_damage_args`, `spell_undead_die` | the cases, the facts |
| Armor class caps | `effects_ac`, `dtu_spell_shield_ac` | the armor class code, the facts, `fx_line` |

To add or change a spell:

1. Set its line of `Spell_rule`: the kind (the sentence), the save, the
   flags and the effect bit. Do not test a condition of a flag by hand in
   the case: call `rule_reaches` and `rule_save`.
2. Give its time in `spell_time` and its amount in `spell_amount`. The case
   sets `dur` from `spell_time` and its message prints `MINUTES(dur)`.
3. Build and run `-chartest 5:desc`: the line `desc: rule check: 0
   problems` must show. `dtu_spell_rule_check` fails when the kind does not
   fit the other values (an effect, a time, an amount, a level limit with
   no sentence that prints it, a stopped spell with no sentence). A
   debug build also asserts it at the first level.
4. Run `tools/check_desc.py`: the game runs compare the numbers and the
   condition lines with the game.

A new condition that the window must print needs a flag, a test in
`rule_reaches` or `rule_save`, a line in `dtudesc.c`, a case in
`dtu_spell_rule_check` and a check in `check_conditions` of
`tools/check_desc.py`. A new pattern needs its Spanish text in
`engine/lang/es_port.catalog`.

To add or change an item: the numbers come from `dtu_char_misc_use`,
`dtu_char_item_powers`, `dtu_char_item_fx`, `dtu_char_item_ac` and the
table line. Change the rule there, not in `dtudesc.c`.

## Typed by hand

These parts are typed by hand and can go out of step with the game. A
change of the rule needs a change here:

- The sentence of each kind (`spell_effect`) and of each item use and
  power (`use_line`, `power_lines`). The numbers in them come from the
  shared functions; the words are typed.
- The level thresholds of the three effects of Color Spray (T <= L,
  T <= L + 2) are in case 6; the sentence names them only as "by the level
  of the target".
- The immunity flags of the creatures that the window does not name
  (Cure: extraplanar; Fear and Scare: MONSTER_F_NO_SCARE; Sleep:
  MONSTER_F_NO_SLEEP, the elf and the half-elf; Infravision: a creature
  with infravision; the Sleep pool) are in the cases.
- Scare makes the target drop its weapon on a d2 as Fear does, and its
  sentence does not say it: no pattern of the catalog has a level limit
  and the weapon together.
- The Chill Touch line: the melee hit of `dtucombat.c` (0x29E3D) applies
  the 1d4 and the Fear of undead.
- `dtudesc.c` items, which need a function of `dtuchar.c` or `dtuobj.c`:
  `counted` and `charged` repeat which uses `use_misc` uses up and which
  use one charge; the lines of the weapons 45, 70 and 71 repeat the
  numbers of `dtu_weapon_extra_damage` (0x2AB9F, 0x2B15B, 0x2B1F2); "Attacks
  twice as fast" repeats the factor of the speed items; the DRAGON,
  TOUGH and GEM extra lines repeat rules of `use_misc`.
- The classes line of a potion is the class word of its table line. The
  inventory marks the potion as not usable for another class, but
  `dtu_char_use` has no class test (as 0x37A6A): each class can drink the
  Potion of Hill Giant Strength and the Potion of Invulnerability.

`tools/check_desc.py` guards the numbers and the condition lines: its game
runs compare the texts with what the game does (see Tests).

The values that depend on the character use the character now: the caster
level, the class level of the Potion of Dragon's Blood and of the Spider
Piece of Turn Undead, the class of the player for the spell Strength.

## Hidden information

1.3 has no identification and no unknown items: the name and the icon of
each item are the ones of its table line. The nine Spider Pieces have the
same name but each has its own icon (188 to 196), so the player can tell
them apart, and the window says what each one does. The Cursed Scroll has
its own name in the table, so the window names its harm in one line. A quest
item shows only that it is one, and a key only that it is a key.

## Text and language

Each line is one of a small set of patterns (`"Damage: %s"`, `"Heals %s hit
points."`, `"Slot: %s"`). Each pattern goes through `LT` of `dtulang.h`. The
Spanish text is in `engine/lang/es_port.catalog` (CA; puntos de golpe;
tirada de salvación). A number with a decimal place goes through the
pattern `"%d.%d"` (Spanish `"%d,%d"`). The Spanish patterns do not depend on
the gender of the item ("Con magia.", "Al llevar: %s.", "Efecto: %s.").

## Tests

- `-chartest 5:desc` writes each text (all lines of the three item tables
  and all spells) to stdout and to the `-charlog` file, with a check that
  the text is not broken and fits the box: the inventory form with the game
  font of the run, the market form with the front end font in the high
  resolution box.
  The check also fails on a row that is wider than the box. Then it makes
  the text of each number from -2 to 259 of each item type, with each kind
  of count and in a buffer of 24 bytes, for a crash or a text with no end.
  `desc=N` makes the spell texts at caster level N.
- `tools/check_desc.py` runs that for the four forms (English and Spanish,
  640x480 and 320x200) and fails on any problem. `--dump FILE` writes all
  texts. Then two game runs (`-rulebookitems` and `-origitems`) compare the
  numbers of the texts with the game: the run wears each of the 53 armors
  (the armor class of the character), drinks each of the 17 potions (the
  hit points, the effect and its time, the ability, the spell), fires each
  of the 8 wands (the spell of the cast) and casts each of the 56 spells at
  caster level 5 (the damage on a creature, the healing, the time of the
  effect). For every spell each effect that the game gives must be named by
  the text, each effect that the text gives (`Gives ...`) must happen, and
  its time must be in the duration of the text. The
  results come from the `-charlog` and `-objlog` lines that the game code
  writes where it applies the value. Two more runs check the condition
  lines (`check_conditions`): each area spell that lands is cast at the
  player (hit with "Also hits you when you are in the area.", else never),
  a creature casts Magic Missile at the player with Shield and without it,
  Hold Undead, Scare and Sleep go on six creatures of BANE (no target
  above the level limit and no living target of Hold Undead gets the
  effect, one below it does), and a spell with "Saving throw: None" rolls
  no save. Each text run needs the line `desc: rule check: 0 problems`.
- `tools/uitest.py --only desc_window [--shots DIR]`: right clicks on a
  weapon, an armor and a potion, the key `/`, a spell (`-chartest
  30:descspell=16`) and a line of the market (`-testmarket`); no window for
  an empty cell, for a second right click, and with an item on the cursor.
  `--shots` also writes the longest item and spell texts in the 320x200
  interface in Spanish and in English.
- The test commands `descitem=TYPE.ID` and `descspell=N` of `-chartest`
  open the window.
