# BIN table text (the BITMAPS.TBL dialect of Descent to Undermountain)

Status: the text of all 85 table files of the disc was decoded and read. The parser of
the port (`engine/main/bmread.c`) loads 81 of them with their archives. The texture
numbering is verified against the texture name table of all 34 level files that have a
table (0 names differ). The values of the item classes are stored but only partly
decoded. Addresses are for DTUGAME.EXE 1.3 with object 1 at 0x10000.

Tools: `python -I tools/egg.py bin FILE` prints the decoded text.
`build-d1/engine/main/test_dtudata` loads a table with the port code and prints counts.

A BIN file (packed on the disc as `NAME.HEN`) is the game table of one level or of one
player character. It is the Descent 1 `BITMAPS.TBL` text with new sections. The
source file is still `bm.c` in the assert strings of DTUGAME.EXE. The file layout and
the byte encoding are in [egg.md](egg.md): 13 bytes with the name of the EGG archive,
then the encoded text.

## Which table and archive a level uses

- The table of a level has the base name of the level: `YPINN.LVL` uses `YPINN.BIN`.
  Code: `load_level` passes the level name to the table loader (call at 0x499E7), and
  the loader changes the extension to `bin` (0x1B5D2, log text "Looking for %s"). If
  the name is the same as the last one it does not load again ("Don't need to
  re-load.").
- The first 13 bytes of the table name the archive. Many levels share one archive:
  `YPINN`, `PORTAL` and `HUB1` to `HUB4` name `portal.egg`. `CRYPTS`, `MINE`, `MINES1`,
  `PRISON` and `PRISON1` name `prison.egg`.
- If there is no `NAME.BIN`, the loader reads the plain text file with the name it was
  given. Its first line is the archive name and its other lines can be names of
  include files. No such file ships (the strings `dtu.tbl` and `ypinn.TBL` are in the
  executable).
- Of the 35 levels of the disc, `IBBALAR1.LVL` has no table of its own name.
- `MOIN1.BIN` names `mon1.egg`, which is not on the disc. `DMM.HEN`, `FFM.HEN` and
  `FMM.HEN` are empty files (2 bytes).

## The table of the player character

A player character has an archive and a table with a 3-letter name, for example
`HMC.EGG` and `HMC.BIN`. The code at 0x1B76D builds the name:

| Letter | Source | Values |
|---|---|---|
| 1 | race, index into the string `HDEKFS` at 0x13A82D | six races |
| 2 | sex | `F` or `M` |
| 3 | class, index into the string `CMFTRT` at 0x13A834 | six classes |

Guess from the file names and the game rules: H human, D dwarf, E elf, C cleric, M
mage, F fighter, T thief. The other letters are not identified.

The function at 0x25792 opens the table. Before that it replaces five names (the same
code is at 0x4995E): `KFF` to `EFF`, `KMT` to `EMT`, `EMF` to `EMT`, `KMC` to `KMM`,
`KMF` to `KMM`.

A player table has one `$PLAYER` line and six `$SOUND` lines. When the parser of the
level table reaches the `$PLAYER` line of the level table and a player character is
set, it reads the player table in place of that line (call at 0x1C4A8, log text
"gonna read a player %s."). The `$PLAYER` line of a level table names place holder
models (`a01temp2.pof` and so on, 1 or 2 textures each), which are in the level
archive.

The archive loader reads the headers of the player archives before those of the level
archive: bitmaps, then sounds, then models, each time the player entries first
(function at 0x6F887, log texts "Bitmaps Player #%d", "done with player polys"). The
port does the same in `bm_load_level_tables`, so the 21 models of a player archive
have the numbers 0 to 20 and the models of the level archive follow.

## Text syntax

- Lines end with LF. A line can be long: the longest has 8244 characters. DTUGAME.EXE
  reads a line into a buffer of 0x8000 bytes.
- `;` starts a comment that goes to the end of the line.
- A word that starts with `$` starts a section or is a record of an object class.
- `@` before the first word of a line is the Descent 1 mark for lines that the
  shareware game skips. It is on all `$MONSTER` lines and on some bitmap lines. It has
  no effect in the port.
- A word that starts with `!` is a mark for the tools: `!METALS_FLAG`, `!LIGHTS_FLAG`,
  `!EFFECTS_FLAG` (texture pages of the editor), `!BITMAP_REMAP_OFF`,
  `!BITMAP_REMAP_ON` (see flag 0x40 in egg.md). The port skips them.
- `key=value`. In the clip sections the parser also accepts `key value`, because it
  splits words at spaces and at `=`, as in Descent 1.
- A list of numbers is `key=1 2 3`: the words after the first value that are numbers
  belong to the key (`speed=`, `speeds=`, `state_times=`, `fire_frames=`).
- A name is in quotes and can have spaces: `name="Ring of Protection +2"`. DTUGAME.EXE
  changes each space inside quotes to the byte 0x84 before it splits the line
  (0x1BB7F). The port does the same and changes the byte back.
- `superx=N` anywhere on a line sets the super transparent color for the bitmaps of
  the line (Descent 1). The archive bitmaps are already remapped, so it has no effect.
- A word that is not a keyword is a bitmap file name: `name.bbm` (one bitmap) or
  `name.abm` (animation). The archive name of a bitmap is the file name without
  extension. Frames of an animation are `name#0`, `name#1`, ...
- `%N` in a list of model textures or as a value of `picture=` or `bitmap=` means:
  the bitmap that eclip N animates (Descent 1).
- Some keys start with `#` (`#damage_dice=1` after `damage_dice=4`). The port keeps
  them in the record text and does not decode them.

## Sections

The parser of DTUGAME.EXE knows 20 section words (strings from 0x120E40). The table
shows the count in `YPINN.BIN` and what the port does.

| Word | Count | Meaning | Port |
|---|---|---|---|
| `$TEXTURES` | 2 | The bitmap lines that follow are wall textures. `lighting=`, `damage=`, `volatile` after a name set the Descent 1 `TmapInfo` values. | `Textures[]`, `TmapInfo[]` |
| `$VCLIP` | 117 | Video clip: `clip_num= time= abm_flag=1 vlighting= sound_num=`, flag `rod_flag`, then the bitmap line. | `Vclip[]` |
| `$EFFECTS` | 1 | `Num_effects=23`. Starts the wall effect clips. | value not used |
| `$ECLIP` | 63 | Effect clip: `clip_num= time= abm_flag=1`, with `obj_eclip=1` for a clip that animates an object bitmap. | `Effects[]` |
| `$WALL_ANIMS` | 1 | `Num_wall_anims=62`. `$DOOR_ANIMS` is the same word. | value not used (the clips go up to 81) |
| `$WCLIP` | 81 | Door clip: `clip_num= time= abm_flag=1 open_sound= close_sound= vlighting= hidden=`. | `WallAnims[]` |
| `$SOUND` | 160 | `$SOUND number alt_number name.rca`. | `Sounds[]`, `AltSounds[]` |
| `$PLAYER` | 1 | Player character: models with their textures, movement values, sounds, ability scores. | `Player_type_info[]`, `Player_ship` |
| `$MONSTER` | 9 | Creature type. Takes the place of `$ROBOT`. | `Robot_info[]`, `Monster_info[]` |
| `$MONSTER_AI` | 9 | 31 numbers for the creature of the `$MONSTER` line before. Takes the place of `$ROBOT_AI`. | `Robot_info[]`, `Monster_info[]` |
| `$WEAPON` | 89 | Weapon type: the Descent 1 keywords plus item and rule values. | `Weapon_info[]`, `Weapon_item_info[]` |
| `$OBJECT` | 128 | Item type (object type 16). | `Misc_info[]` |
| `$ARMOR` | 53 | Armor item type (object type 15). | `Armor_info[]` |
| `$SPELL` | 56 | `$SPELL number ...`: spell type (object type 17). | `Spell_info[]` |
| `$DRESSING` | 38 | Scenery type (object type 18): a model (`pof=`), a vclip (`vclip=`) or one bitmap (`bitmap=`). | `Dressing_info[]` |
| `$INTERFACE` | 1 | Bitmap lines of the interface. Empty in the level tables. | `Interface_bitmaps[]` |
| `$ICONS` | 1 | Bitmap lines of icons (`cursor.bbm`). | `Icon_bitmaps[]` |
| `$GUYPICS`, `$GIRLYPICS` | 0 | In the parser, in no table. | bitmap lines are skipped with a message |

A `$` word that is not in this list makes the port skip the line and print a message
that names the word. No table of the disc has such a word.

The Descent 1 sections `$COCKPIT`, `$GAUGES`, `$POWERUP`, `$HOSTAGE`,
`$HOSTAGE_FACE`, `$ROBOT`, `$ROBOT_AI`, `$OBJECT` (in its Descent 1 form) and
`$PLAYER_SHIP` are not in the parser of DTUGAME.EXE and not in the port.

## Numbering

### Textures

As in Descent 1, each of these gets the next texture number:

1. each bitmap line of a `$TEXTURES` section,
2. the bitmap line of an `$ECLIP` without `obj_eclip=1` (one number, for frame 0),
3. each frame of the animation of a `$WCLIP` (`name#0`, `name#1`, ...).

The name of a texture (`TmapInfo[].filename`) is the file name without extension, or
`name#frame` for a door frame. The same bitmap can be in the table many times
(`green` is texture 0 to 4).

Verified: for all 34 levels, texture i of the table has the name of entry i of the
texture name table of the LVL file (lvl.md). Each of the 35 level files has 1489
names. The tables
that name `portal.egg` define 1638 textures: the last 149 are the interface bitmaps of
the second `$TEXTURES` section, which the level files do not have. The other tables
define 1489.

### Clips

`clip_num=` is the number of the clip in its array. The highest numbers in the tables
are 180 (`$VCLIP`), 91 (`$ECLIP`) and 81 (`$WCLIP`). An animation has at most 30
frames (`$VCLIP`), 20 (`$ECLIP`) and 13 (`$WCLIP`) in the archives.

### Sounds

`$SOUND 2100 2100 abjure.rca` gives sound number 2100 the sound `abjure` of the
archives. The game code uses the sound number. The highest number is 4413. The second
number is the Descent 1 "alternate" sound for low memory (0 means the same sound, a
negative number means none). If a table names a sound that the archives do not have,
the port leaves the sound number empty and prints a message. This does not happen in
the 81 tables that load.

### Object bitmap slots

A model draws with the bitmaps `ObjBitmaps[ObjBitmapPtrs[first_texture + n]]`
(Descent 1). The parser fills the slots in the order of the table. In the port a slot
is taken by:

- each bitmap name or `%N` in the texture list after `model=`, `pof=`, `weapon_pof=`
  or `embedded_model=`,
- the value of `picture=` of a `$OBJECT`, `$ARMOR`, `$SPELL` or `$WEAPON` line (before
  the models of the line),
- the value of `bitmap=` of a `$DRESSING` line.

`picture=` of a `$PLAYER` or `$MONSTER` line takes no slot.

With these rules a simulation gives the `first_texture` value of the EGG header for
all 58 models of `PORTAL.EGG` and all 21 of `HMC.EGG` (see egg.md). Other tables give
a few differences (10 slots in front of the first monster in `BANE.BIN`), so the
rules of DTUGAME.EXE are not fully known. The port therefore does not use the header
value: a model gets the slots of its own texture list.

A model that a second line names again keeps the textures of the first line
(DTUGAME.EXE finds the model by name and returns its number, 0x72DD4).

### Models

The model number is the place of the model in the archives: player archive first,
then level archive, in file order. A table names a model by its POF name. A name that
is not in the archives gives model number -1 in the port and a message. The last
model of every `$PLAYER` line is such a name (`a01temp5.pof` in `HMC.BIN`,
`a11temp1.pof` in `YPINN.BIN`).

### Object types

The port appends one entry to `ObjType[]` and `ObjId[]` for each `$MONSTER`,
`$DRESSING`, `$OBJECT`, `$ARMOR`, `$SPELL` and `$WEAPON` line, as Descent 1 does for
its classes. Entry 0 is the player. DTUGAME.EXE asserts `Num_total_object_types <=
MAX_OBJTYPE` (430) in each of these readers.

## Keywords of the object classes

The port keeps the whole text of each record (`Tbl_records[]`). `bm_tbl_get()` returns
the value of any keyword and `bm_tbl_has_flag()` finds a flag word. The lists below say
which keywords the port also decodes into a structure.

### $PLAYER

Decoded: `model=` (many, each with its texture list), `name`, `picture`, `eye_offset`,
`gravity`, `state_times` (18 numbers), `speeds` (9), `fire_frames` (4), `mass`, `drag`,
`max_thrust`, `max_rotthrust`.

Stored only: `age`, `class`, `class_level`, `race`, `armor_class`, `wisdom`,
`intelligence`, `charisma`, `constitution`, `dexterity`, `strength`, `money`,
`hit_sound`, `die_sound`, `jump_sound`, `happy_sound`, `banish_sound`,
`land_and_die_sound` and the same names with `_rand`, `hard_sound_rand`.

### $MONSTER

Decoded into `robot_info`: `model=` with its textures, `lighting`, `weapon_type`,
`mass`, `drag`, `score_value`, `see_sound`, `attack_sound`, `claw_sound`,
`attack_type` (`melee` is the Descent 1 value 1, `projectile` is 0), `bossiness`
(boss flag), `max_hit_points` (also stored as `strength`, guess).

Decoded into `monster_info`: `name`, `picture`, `thaco`, `armor_class`,
`max_hit_points`, `str`, `iq`, `moola`, `die_sound`, `hit_sound`, `gravity`,
`state_times` (10 numbers), `speeds` (3), `fire_frames`, and the flag words `UNDEAD`,
`HUMANOID`, `FLYING`, `NO_SCARE`, `NO_SLEEP`, `FIRE_IMMUNE`, `ELECTRICITY_IMMUNE`,
`WEAPON_RESISTANT`, `EXTRAPLANAR`, `BLIND`, `LIGHT_SENSITIVE`, `dark_lover`,
`light_lover`, `pass_thru`, `noattack` (upper or lower case).

Stored only: `regeneration`, `dead_adjust`, `blood`, `rat`, `regonzo`, `spell_level`,
`spell_points`, `cleric_points`, `monster_hit_vclip`, `battle_cry_sound`,
`fall_sound`, `happy_sound`, `hard_sound`, `jump_sound`, `land_sound`,
`land_and_die_sound`, `push_sound`, and the flag words `jumping_jack`, `patient`,
`impatient`, `pain_flee`, `prison_boy`.

A word without `=` after `model=` is a texture if it is `%N`, has a dot, or is the
name of an archive bitmap (`g_a01`).

### $MONSTER_AI

31 numbers. DTUGAME.EXE (0x1DC02) reads the first one and does not use it (values 4,
5, 6 in the tables). The entry it fills is the next one of a counter, as in Descent
1. Then:

| Numbers | Type | Port |
|---|---|---|
| 2 to 6 | fix | `field_of_view` (degrees, converted to a cosine as in Descent 1) |
| 7 to 11 | fix | `firing_wait` |
| 12 to 16 | fix | `turn_time` (guess from the Descent 1 order, values 3.0 or 7.0) |
| 17 to 21 | byte | `rapidfire_count` |
| 22 to 26 | s16 | unknown, `Monster_info[].ai_values[0]` (-100 to -500) |
| 27 to 31 | s16 | unknown, `Monster_info[].ai_values[1]` (0 to -400) |

One group has five numbers, one for each difficulty level. The comment at the end of
each line says "attack value". The Descent 1 values `fire_power`, `shield`,
`max_speed`, `circle_distance` and `evade_speed` are not in the line. The port leaves
them 0.

### $DRESSING

Decoded: `pof=` with its textures, `vclip`, `bitmap`, `name`, `size`, `lightcast`,
`explode_sound`, `break_vclip`, `pushable`, `pass_thru`, flag words `magic`,
`cant_break`. These are all the keywords of the tables.

### $OBJECT, $ARMOR, $SPELL

Decoded: `name`, `picture`, `icon`, `inv_icon`, `value`, `mass` or `weight`,
`stack_limit`, and for a spell `weapon_pof=` with its textures and `weapon_vclip`.
The number after `$SPELL` is the spell number (0 to 55 in `YPINN.BIN`, one line for
each number, in rising order).

Stored only, `$OBJECT`: `lightcast`, `player_light`, `duration`, `pc_spell`,
`pc_spell1`, `pc_spell2`, `plot_item`, `to_hit_bonus`, `adjust_strength`,
`set_strength`, `set_dexterity`, `set_intelligence`, flag words `all_use`,
`fighter_use`, `thief_use`, `cleric_use`, `stackable`, `non_hotkey_item`, `quest`,
`plot_item`, `bow`, `crossbow`, `necklace`, `cloak`, `belt`, `boot`, `gauntlet`.

Stored only, `$ARMOR`: `armor_class_decrement`, `pc_spell`, `player_light`,
`set_constitution`, `set_dexterity`, `set_strength`, flag words `armor`, `shield`,
`ring`, `cloak`, `Necklace`, `boot`, `gauntlet`, `belt`, `magic`, `magic_ok`,
`thief_ok`, `stackable`, and the `_use` words.

Stored only, `$SPELL`: `level`, `range`, `level_range`, `cast_time`, `duration`,
`level_duration`, `type` (`TARGET`, `AREA`, ...), `uses`, `uses_per_level`, `radius`,
`size`, `modifier`, `damage_dice`, `damage_bonus`, `damage_percent`,
`fire_damage_dice`, `fire_damage_level`, `cold_damage_dice`, `cold_damage_level`,
`elec_damage_dice`, `elec_damage_level`, `acid_damage_level`, `to_hit_bonus`,
`combustion`, `spell_object`, `vclip_trail`, `weapon_vclip_size`, `cast_on_top`,
`cast_sound`, `armor_hit_sound`, `monster_hit_sound`, `monster_hit_vclip`,
`wall_hit_sound`, `wall_hit_vclip`, `weapon_hit_vclip`, `lightcast`, `player_light`,
flag words `mage_use`, `cleric_use`, `cast_with_hands`, `cast_with_wand`,
`cast_in_front`, `self_cast_ok`, `always`, `homing`, `stackable`.

### $WEAPON

Decoded into `weapon_info` (Descent 1 meaning): `weapon_pof=` with its textures,
`embedded_model=` (as inner model, guess), `weapon_vclip`, `picture`, `speed` (5
numbers), `mass`, `drag`, `thrust`, `matter`, `bounce`, `flash_vclip`, `flash_sound`,
`flash_size`, `blob_size`, `monster_hit_vclip` (also spelled `monster_hit_vcilp`,
`monster_hitvclip`, `monsster_hit_vclip` in the tables), `monster_hit_sound`,
`wall_hit_vclip`, `wall_hit_sound`, `impact_size`, `lighted`, `lw_ratio`, `lightcast`,
`persistent`, `ammo_usage`, `fire_wait`, `fire_count`, `damage_radius`, `lifetime`,
`destroyable`, `homing`. Decoded into the item part: `name`, `icon`, `inv_icon`,
`value`, `stack_limit`.

Stored only: `weapon_name`, `weapon_char`, `uses`, `damage_dice`, `damage_bonus`,
`damage_percent`, `fire_damage_dice`, `to_hit_bonus`, `ac_adjustment`,
`adjust_dexterity`, `adjust_strength`, `ammo_initial`, `armor_hit_sound`,
`attack_type`, `enemy_spell`, `fire_delay`, `gravity`, `levels_per_use`, `magnitude`,
`min_speed`, `modifier`, `monster_kill_sound`, `monster_miss_sound`, `pc_spell`,
`player_light`, `size`, `speed_range`, `stick`, `thrown`, `vclip_range`, flag words
`stackable`, `magic`, `ARROW`, `BOLT` and the `_use` words.

## Limits in DTUGAME.EXE

Constants of the asserts in `bm.c` and of the loops that clear the tables (0x1B2F5):

| Name | Value | Source |
|---|---|---|
| `MAX_TEXTURES` | 2500 | assert |
| `MAX_OBJ_BITMAPS`, `MAX_OBJ_BITMAP_PTRS` | 1700 | assert |
| `MAX_OBJTYPE` | 430 | assert |
| `MAX_ROBOT_TYPES` | 40 | assert |
| `MAX_WEAPON_TYPES` | 90 | assert |
| `MAX_ARMOR_TYPES` | 80 | assert |
| `MAX_MISC_TYPES` | 250 | assert |
| `MAX_DRESSING_TYPES` | 60 | assert |
| `MAX_MODEL_VARIANTS` (models of one line) | 256 | assert |
| sound numbers | 5500 | loop that fills two tables of 16-bit entries with 10000 |
| vclips (guess) | 250 | loop that clears entries of 142 bytes |
| eclips (guess) | 140 | loop that clears entries of 194 bytes |

The values of `MAX_EFFECTS`, `MAX_WALL_ANIMS`, `VCLIP_MAXNUM`, `MAX_SPELL_TYPES` and
`MAX_PLAYER_TYPES` are in assert strings whose compare was not found. The port uses
250 vclips, 140 eclips, 100 wall clips, 100 spells and 4 player types.

## Oddities of the tables

The port prints a message for each and goes on. For `YPINN.BIN` with `HMC.BIN`:

- `picture=pum01.bbm` of the `$PLAYER` line of `HMC.BIN` is not in the archives.
- The last model of the `$PLAYER` line is not in the archives (see "Models").
- `HMC.BIN` line 24 holds only `pgant3bf.bbm`, outside a bitmap section.
- Line 481 is the end of the `$SPELL` line 480, split off: `vclip_trail=104`.
- Line 1025 is `wall_hit_sound=2118` alone.
- The `$MONSTER_AI` line 1318 has 30 numbers.
- The last line holds `g=1`.
- 280 `$VCLIP` lines end with the word `rod_flag` without a value. Descent 1 wants
  `rod_flag=1`. The port takes the word alone as 1.

Six tables are of an older dialect: `BASE`, `DEMO`, `MINES1`, `PRISON1`, `SPIDER` and
`TRAIN1` have four `$PLAYER` lines (and 83 weapons, but for `BASE`). `MINES1`, `PRISON1` and `TRAIN1`
name archives of the newer kind and give about 600 messages each (bitmaps and models
that the archive does not have). `PRISON1` and `SPIDER` are levels of the disc that
are not in the mission list.

## Unknown

- The rule values of the item classes (most keywords are stored only).
- The two groups of 16-bit numbers of `$MONSTER_AI`.
- The exact slot rules of DTUGAME.EXE for the object bitmaps.
- Which model of a `$PLAYER` line is used when (the names look like body and
  equipment variants: `hmc__p`, `hmcg_p`, `hmci_p`, ...).
- The race and class letters that are not listed above.
- `$GUYPICS` and `$GIRLYPICS`.
