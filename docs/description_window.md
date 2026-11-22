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

A key or a click of either mouse button closes the window. The press of the
button goes to the window and the play screen sees only the release, so the
click does not act on the screen behind the window. In the game the window
stops the game time, as the scroll text screen does.

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

Fonts: the game font in the game, the small font of the front end
(FNTEMFS_.FNT) in the market. Colors of the scroll text: the name yellow
(~c7), the other lines white (~c1), each with a shadow of color 0. In the
market the scroll gets the closest colors of the palette of the market
picture.

## Layout

The first line is the name as the player sees it (the corrected names of
`dtuitemfix.c`, in the language). The other lines come in this order. A
line with no value is left out.

- Weapon: damage dice and bonus (a weapon with no dice attacks as the empty
  hand), fire dice, to hit, the time of a melee swing, melee or thrown or
  arrow or bolt with the range, the count of a stack, magic or not,
  classes, then the powers: armor class in the hand, the effects in the
  hand, the skills, and the special rules of Flame Tongue, the Mace of
  Disruption and the Dagger of Impaling.
- Wand (a weapon with `pc_spell` that is not melee): charges, classes, the
  spell of each use.
- Bow and crossbow: the bonus of the arrows or bolts, slot, magic, classes,
  the effects in the slot.
- Armor, shield and worn items: armor class (body armor: the value, 10 plus
  the table value; other items: how much better), slot, classes, the effects
  while worn, the powers (abilities, skills, the rules of the rings and the
  cloaks of protection).
- Potion, scroll and other usable items: count (charges for a Spider
  Piece), classes, one line for what one use does.
- Key items: "Key.". Plot items: they cannot be dropped or sold. Other
  items: "Item.".
- A spell in the item list: charges, classes, the spell of each use.
- Spell: class and spell level, cast time, range (the flight of the missile,
  or "You"), area, missiles (if more than one), duration, saving throw, the
  caster level (only when a value depends on it), the effect.
- Market: the price last ("Price" when the player buys, "Sale price" when
  the player sells).

Distances are feet: 2 units of the game are 1 foot, as in the rules wiki.
Times are game seconds: seconds below a minute, hours for whole hours, else
minutes.

## Where the values come from

The window reads the values that the game code reads, so the text and the
game cannot disagree:

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
  and the effect bit of each case.
- `dtu_missile_weapon_range` (`dtuobj.c`) and `dtu_atk_state_time`
  (`dtuatk.c`).

The values that depend on the character use the character now: the caster
level, the class level of the Potion of Dragon's Blood and of the Spider
Piece of Turn Undead, the class of the player for the spell Strength.

## Hidden information

1.3 has no identification and no unknown items: the name and the icon of
each item are the ones of its table line. The nine Spider Pieces have the
same name but each has its own icon (188 to 196), so the player can tell
them apart, and the window says what each one does.

## Text and language

Each line is one of a small set of patterns (`"Damage: %s"`, `"Heals %s hit
points."`, `"Slot: %s"`). Each pattern goes through `LT` of `dtulang.h`. The
Spanish text is in `engine/lang/es_port.catalog` (categoría de armadura,
CA; puntos de golpe; tirada de salvación).

## Tests

- `-chartest 5:desc` writes each text (all lines of the three item tables
  and all spells) to stdout and to the `-charlog` file, with a check that
  the text is not broken and fits the box: the inventory form with the game
  font of the run, the market form with the front end font in the high
  resolution box.
- `tools/check_desc.py` runs that for the four forms (English and Spanish,
  640x480 and 320x200) and fails on any problem. `--dump FILE` writes all
  texts.
- `tools/uitest.py --only desc_window [--shots DIR]`: right clicks on a
  weapon, an armor and a potion, the key `/`, a spell (`-chartest
  30:descspell=16`) and a line of the market (`-testmarket`).
- The test commands `descitem=TYPE.ID` and `descspell=N` of `-chartest`
  open the window.
