# Text catalogs of the translations (ASCENT)

The port shows the text of the game in another language when a catalog for
that language exists. The English text stays in the game files and in the
code. A catalog maps each English text to its translation. The format and
the ids below are those of the port. The original game has no such file.

Code: `engine/main/dtulang.c` (loader and lookups), `tools/lang_extract.py`
(extraction, parts, merge, check), `cmake/embed_text.cmake` (the translation
of the text of the code goes into the program).

## Language files on the disc (checked)

The disc has the language variants `DTU?.TEX` (E, F, G, I, M, N, S, and the
older `DTU.TEX`, `DTU1.TEX`), `FETXT?.DAT/.JRG` (E, F, G, I, M, N, S, U and no
letter), `GAB?.DAT/.RLH` (E, F, G, I, M, N, S), `KCFG.*` and `WERDZ`. None of
them is a translation:

- `DTUS.TEX` (and F, G, I, M, N) is `DTUE.TEX` with joke words in some of the
  strings (764 of 985 lines are equal to the English file; for example "New
  Taco", "Cannot set tortilla mode for Taco"). The other files replace other
  words ("perrier", "bratwurst", "pasta", "gameazoid", "glitzig").
- `FETXTS` (and F, I) is the English text with `SPANISH SPANISH SPANISH` (or the
  name of the language) in front of the changed entries; `FETXTU` has
  `<placeholder>`. No entry has Spanish words.
- `GABF`, `GABG`, `GABI`, `GABM`, `GABN`, `GABS` are equal (340610 bytes, the
  same MD5) and differ from `GABE` (353459 bytes): an older English version.
- `KCFG.SPA` has key names with joke changes (`ElEsc`, `LAlto`, `PgUpo`).

So the translations of the port are new text.

## Catalog file

UTF-8, LF line ends. A line that starts with `#` is a comment. An entry is a
group of lines `key: value` (one space after the colon), and a blank line
ends it. The keys:

| Key | Meaning |
|---|---|
| `id` | The id of the text (below). It starts the entry. |
| `src` | Where the English text is (file, line, entry, offset). For the people who translate. |
| `ctx` | Where the program shows the text, the speaker, the reply it answers. For the people who translate. |
| `max` | The room: `N bytes (message box)`, `N characters`, `N lines, longest L characters`. Checked by `lang_extract.py check`. |
| `dup` | The id of the first entry with the same English text (same source kind). |
| `en` | The English text. |
| `tr` | The translation. An empty `tr` (with a non-empty `en`) is no translation. |
| `gender` | Only for a name of the tables (`tbl.*`) and a creature name of the text file (`tex.*`): the grammatical gender of the translation, `m` or `f` (below). Optional. |
| `plural` | Only for a name of the tables (`tbl.*`): the plural of the translation (below). Optional. |

Values are one line. Escapes: `\n` line break, `\t` tab, `\\` backslash,
`\xNN` one byte of the game text (for example the icons of the fonts at 128 to
135, and a space at the start or the end of a value, `\x20`). In the ids
`gab.*` the script tokens are words in braces: `{name}` (byte 9, the name of the
player), `{class}` (10), `{classes}` (11), `{race}` (12), `{races}` (13),
`{boy}` (14), `{man}` (15), `{sir}` (16), `{gentleman}` (17)
(docs/re/scripts.md, section 3.1).

The program reads `id`, `en`, `tr`, `gender` and `plural` and ignores the
other keys. The text of
a translation is changed to Latin-1 (ISO 8859-1); a character outside it
shows as `?`, except the typographic quotes and dashes, which become `'`, `"`
and `-`.

Files:

| File | Content | In the repository |
|---|---|---|
| `gamedata/lang/en.catalog` | all English text with `src`, `ctx`, `max`, `dup` | no (game text) |
| `gamedata/lang/en.part<X>.catalog` | the parts for the translators (`parts.md`): A to F made by `split`, then G, H, ... with the text added to `en.catalog` later (the merge reads them after F) | no |
| `gamedata/lang/<code>.part<X>.catalog` | a translated part | no |
| `gamedata/lang/<code>.catalog` | the merged translation of the game files: `id`, `en`, `tr` | no |
| `gamedata/lang/<MOVIE>.en.srt` | the English transcript of the speech of a movie (SRT) | no |
| `gamedata/lang/<MOVIE>.<code>.srt` | the subtitles of a language | no |
| `engine/lang/<code>_port.catalog` | the translation of the text of the code | yes (our text) |

The port catalog also holds the corrected item names of
`engine/main/dtuitemfix.c` (docs/audit/items.md): entries with the id
`tbl.fix.NAME`, the corrected English name in `en`, and `tr`, `gender` and
`plural` as for an item name of the tables. They are written by hand; `merge`
keeps them, and it keeps a `port.*` entry of the file that no part translates
while `en.catalog` has its English text. The game finds them as it finds a
name of the tables, by the English text. The names that the table fixup of
1.3 gives (the `T_NAME` rows of `engine/main/dtutblfix.c`, for example
`Produce Fire`, `Drowslayer`) are not in the tables of the game files, so
they have `tbl.fix.*` entries too. `tools/check_itemfix.py` (a test of
`ctest`) fails when a corrected name or a name of the fixup has no entry.

## The sex of the player: {sex:MALE|FEMALE}

A translation (any id: `tex`, `fe`, `gab`, `say`, `tbl`, `port`, `sub`) can
have the token `{sex:MALE|FEMALE}`: the program shows MALE for a male player
and FEMALE for a female player, for example `Has sido {sex:bendecido|bendecida}.`
or `Bienvenid{sex:o|a}`. Only the exact form is a token: `{sex:` (small
letters), the male form, one `|`, the female form, `}`. A form has no `{`,
`}` or `|` and may be empty (`{sex:|a}`). A text can have several tokens.
Anything else stays as it is. (The English text has braces only in the nine
script tokens.)

The sex: the character being created in the front end (from the choice of
the first screen; male before a choice), else the character
(`Dtu_char.pa.female`, a character not yet loaded is male). The program
keeps both forms of each entry: a lookup returns the form for the sex at
that time, and the strings of the text file (`tex`) change with the sex of
the player (`dtu_lang_sync`, each frame). The places that show a formatted
text (the HUD message, the message box of the game, `nm_messagebox`, the
text of a script) resolve the tokens once more after the values are
filled in; the subtitles are resolved when they are read. A token is
resolved once: the result is not read again.

The spell messages of `dtuspell.c` (`say`) resolve the tokens of the format
for the subject of the message: the player's sex when the subject is the
player ("You"), else the grammatical gender of the name of the creature or
the item (the field `gender` of its catalog entry, below: `f` gives the
second form), else the male form (a trap, a name with no gender). For
example the format `%s %s {sex:bendecido|bendecida} durante %d minutos.`
gives `Tú estás bendecida durante 6 minutos.` for a female player, and
`%s %s {sex:noqueado|noqueada} (origen: %s).` gives `Araña gigante ha sido
noqueada (origen: Tú).`

## Gender of the creature names

The creature names of the text file (`tex.845` to `tex.905`, the names of
the messages, `Creature_names` of `dtuobj.c`) can have the field `gender`
(`m` or `f`, the gender of the head noun: `Araña gigante` is `f`, `Guerrero
drow` is `m`), and no `plural`. English has none. The program reads it with
`dtu_text_gender()` and `dtu_object_name_gender()` (the creature or the item
of an object). `lang_extract.py` reports a `plural` on a creature name as an
error.

Character creation (`dtufe_char.c`) joins the sex and the race of a choice
with the formats `Male %s` and `Female %s` (the race word as the value):
English shows `Female Human`. The race word (`Human`, `Half Elven`, ...) is
resolved for the sex of that choice, not for the sex chosen so far, so the
help line of the female button names the female form. Spanish translates
the race words as `{sex:Humano|Humana}` and both formats as `%s`, which
gives `Haz clic aquí para elegir Humana`. The word `Drow` has one form for
both sexes, so the race word of the code is `{sex:Drow (hombre)|Drow
(mujer)}`: the two buttons of the race screen say which one they choose. The help lines that hold the sex
and the race are whole formats of the code (`Click here to select a %s`,
`You have selected a %s`, `Quickly generate a %s character`,
`Quickly generate a %s %s character`, `I am a %s %s`) with the English text
of the executable (the texts 607, 608, 629, 632 and 633 of `DTUE.TEX` with
the words in between), so that a translation can order the words.

The race words of `DTUE.TEX` (`tex.649` to `tex.653`, `tex.917`, and
`A Human` ... `tex.693` to `tex.698`) can hold the token too: they follow
the sex of the player (the statistics screen, the script token `{race}`)
or of the character being created.

The class words can hold the token as well: `tex.642` to `tex.647`
(`Cleric`, `Mage`, `Fighter`, `Paladin`, `Ranger`, `Thief`: the statistics
screen, the box of a new level with `tex.844`, the script token `{class}`),
`tex.689` to `tex.692` (`fighter`, `thief`, `mage`, `cleric`: character
creation, joined with `/` for a character with two classes; `cleric` is also
a word of `{class}`) and `tex.910` to `tex.912` (`wizard`, `warrior`,
`rogue`: `{class}`). Character creation resolves the class words for the sex
chosen (`dtu_lang_text_sex`), as the race words. Spanish: `{sex:Guerrero|Guerrera}`,
`{sex:mago|maga}`, which gives `Soy: Humana, guerrera/maga`. The plural
words of `{classes}` (`tex.906` to `tex.909`, `tex.913` to `tex.916`) name
a group and have one form.

The script tokens and their words (`token_word` of `dtuscript.c`, the
executable at 0xA7582): `{class}` is `tex.642`, `643`, `644`, `647` or
`tex.692`, `910`, `911`, `912` (one of the two at random, by the first
class: cleric, mage, fighter, thief), `{classes}` is `tex.906` to `909` or
`tex.913` to `916`, `{race}` is `tex.649`, `650`, `651`, `957`, `653` or
`tex.917` (half-elf), `{races}` is `tex.954` to `tex.959`. The drow have
the text 957 `drow` for both tokens. A translation has a singular and a
plural, so the port fills `{race}` of a drow with the text of the code
`drow` (`port.drow_2`, found by its English text) when the text 957 of the
file is `drow`, and `{races}` with `tex.957`. Spanish: `drow` and `drows`.

`lang_extract.py merge` and `check` report a token that is not well formed
(an error), check the placeholders of each form, and check the room with the
longer form.

## Gender and plural of the item names

A language with grammatical gender and plural forms gives each translated
item name (`tbl.weapon.*`, `tbl.armor.*`, `tbl.misc.*`) two more fields:

```
id: tbl.weapon.dagger
en: Dagger
tr: Daga
gender: f
plural: Dagas
```

`gender` is `m` or `f`: the gender of the head noun of the name
(`Poción de sanación` is `f`, plural `Pociones de sanación`). `plural` is
the whole name in the plural, with the same `\xNN` escapes as `tr`. A name
that is plural already has its own text as the plural (`Botas`). A
language without gender leaves both fields out (English has neither, and
its output does not change). A name without `gender` counts as the first
form (masculine); a name without `plural` uses `tr`.

Where the program uses them (`engine/main/dtufe_town.c`, the market): the
list shows the plural name when the quantity is not 1 (also for "many"),
and the texts of the market resolve the item tokens below for the item of
the line. The fields are read by `dtu_name_gender()` and
`dtu_tr_name_plural()` (`dtulang.h`).

`lang_extract.py merge` copies both fields from the parts into
`<code>.catalog` (and to the entries with the same English text, `dup`).
`merge` and `check` report an error for a `gender` other than `m` or `f`,
for the fields on an id that is not `tbl.*`, and for a plural with a
placeholder, a brace, a `|`, other `\xNN` escapes than `tr` or characters
the fonts cannot show. When any entry of the language has the fields, each
translated `tbl.weapon.*`, `tbl.armor.*` and `tbl.misc.*` name without
both gets a warning (a warning is not an error).

## The item shown: {item:MASC|FEM}, {qty:ONE|MANY}, {item}

The texts of the market can name the item of the line and agree with it.
Three tokens, with the same form rules as `{sex:}` (a form has no `{`, `}`
or `|` and may be empty; anything else stays as it is):

| Token | Gives |
|---|---|
| `{item:MASC\|FEM}` | MASC for a masculine name or a name with no `gender`, FEM for a feminine name |
| `{qty:ONE\|MANY}` | ONE when the quantity is 1, MANY for any other quantity (also "many") |
| `{item}` | the name: the translation for a quantity of 1, else the `plural`, with a small first letter (the name is inside a sentence) |

The quantity is the stock of the line (the buy box, "many" for a stock of
100 or more or no limit) or the number the player has (the sell box and
the equipped warning). The program fills the `printf` values first and
then resolves the tokens (`dtu_lang_item_text`), so a token can come in
with a value: `tex.701` ("many") is `{item:muchos|muchas}` and goes into the
`%s` of `tex.704`. Only these texts resolve the tokens: `tex.701` (the list
and the buy box), `tex.703` (the warning of an equipped item), `tex.704`
(the buy box) and `tex.705` (the sell box). `lang_extract.py` reports a
token in any other text as an error, and a token that is not well formed.

The quantity window (FEBGND 25) draws `tex.704` and `tex.705` centered in a
box 339 pixels wide, 6 lines of the medium font (the English `tex.704` has 6
lines). The name in `{item}` has no fixed length, so in a language other
than English the market breaks a line that is wider than the box at its
last space that fits (`mk_reflow`); the line breaks of the text stay.
English text is not changed. `lang_extract.py` checks these two texts by
breaking them at the longest English line (35 characters) with the longest
item name or plural of the catalog in place of `{item}`: no more than 6
lines.

The list of the market draws the word "many" and the name after a column
as wide as the word: with the token the column is as wide as the wider
form.

Spanish:

```
tex.701  {item:muchos|muchas}
tex.703  Tu personaje lleva {item:uno|una}\nequipad{item:o|a}. Si {item:lo|la} vendes,\nperderás sus ventajas.
tex.704  Hay %s {item} {qty:disponible|disponibles} a %u de oro{qty:| por pieza}. Con tu oro y el sitio de tu bolsa de contención puedes comprar %d.\n¿Cuánt{item:os|as} quieres?
tex.705  Tienes %d {item} en tu bolsa de contención.\nPuedes vender{item:lo|la}{qty:|s} {qty:por|a} %d de oro{qty:| por pieza}.\n¿Cuánt{item:os|as} quieres vender?
```

which gives `Hay muchas dagas disponibles a 2 de oro por pieza.`, `Hay 3
martillos de guerra disponibles ...`, `¿Cuántos quieres?` and `Tienes 1
espada ancha en tu bolsa de contención. Puedes venderla por 7 de oro.`

## Ids

| Id | Source | English text at run time | Lookup |
|---|---|---|---|
| `tex.N` | line N + 1 of `DTUE.TEX` (N = the index of `Text_string[]`, 0 to 978) | the string after the escapes of `load_text` | id |
| `fe.N` | entry N of `FETXTE.JRG/.DAT` | the entry without the NUL | id |
| `gab.S.O` | the string at byte offset O of the decoded record of script S (`GABE.RLH`, id & 0x1FF) | the bytes of the string | id |
| `say.LEVEL.N` | message N of `LEVEL.SAY` | the message bytes | id |
| `tbl.CLASS.NAME` | `name=` (or the last of `name=`, `weapon_name=`) of a `$OBJECT`, `$ARMOR`, `$SPELL`, `$WEAPON` or `$DRESSING` line of the tables | the name | English text |
| `port.NAME` | a string of the code (below) | the string | English text |
| `sub.MOVIE.N` | cue N of `<MOVIE>.en.srt` | (not used by the program; the merge writes `<MOVIE>.<code>.srt`) | |

The lookup by id uses a translation only when the `en` of the catalog is the
English text that the program has at that place (another version of a game
file keeps its English text). The names of the tables and the text of the code
are found by their English text, because the code also uses the English names
for its rules (for example `strstr(name, "otion")`): the names stay English in
the tables and are translated where they are shown.

## Where the program translates

- `tex`: `load_text` keeps the English strings and puts the translations in
  `Text_string[]` (`dtu_lang_text_loaded`). A change of the language changes
  them again.
- `fe`: `fe_fetxt()` (race text, Khelben in Blackstaff tower).
- `gab`: the text operand of the script interpreter (`text()` of
  `dtuscript.c`): conversations, replies, narration, the status lines.
- `say`: `dtu_trig_say_text()` (the messages of the triggers).
- `tbl`: the description bar of the inventory, the market lines, the
  messages that name an object (`dtu_object_display_name`, pick up), the spell
  names.
- `port`: `HUD_init_message` (the format), `newmenu` and `nm_messagebox` (the
  title, the items, the format), `fe_menu` (title, items, choice labels), the
  help bar of the front end, the message box of the game
  (`dtu_display_add_message`), the choice box (`dtu_char_choice`), the spell
  messages of `dtuspell.c` (`say`), and each `LT("...")` of the code. `LN("...")`
  marks a string that one of these places translates.

`tools/lang_extract.py extract` collects the strings of the code: the
arguments of `LT`, `LN`, `HUD_init_message` (the first), `say`, `say_effect`,
`dtu_char_choice`, `nm_messagebox`, `fe_help_message`,
`dtu_display_add_message` in the files of `MAIN_SOURCES`
(`engine/main/CMakeLists.txt`). A new string of the code needs one of these
forms to be translated.

## Where the program reads a catalog

The catalog of a language: `<code>.catalog` in the first of: the directory of
the option `-langdir DIR`, `lang` in the directory of the user (with a
`-datadir` run: `lang` in the current directory), `<hd>/lang` (the hd
directory, `dtu_hd_dir`), `lang` in each data directory, `lang` next to the
executable. The content pack `lang_es` (`tools/content_pack.py`) installs
`hd/lang/es.catalog` and the subtitles. The translation of the text of the code
is in the program; the file can change it too (its later entries win).

The setting: `Language=en` or `Language=es` in `ascent.cfg`; the item
"Language..." of the options menu of the front end and of the options menu of
the game (F2) changes it at once. The option `-language CODE` sets it for one
run, `-nosubtitles` turns the subtitles off. An installed game first reads
`default.cfg` next to the executable (same format; the Spanish setup program
writes `Language=es` there when it installs `lang_es`), then `ascent.cfg`,
whose `Language=` wins (docs/install.md).

## Subtitles

`<lang dir>/<MOVIE>.<code>.srt` (for example `INTRO2.es.srt`), standard SRT:
the cue number, `HH:MM:SS,mmm --> HH:MM:SS,mmm`, the lines of the text, a blank
line. UTF-8. Times count from the first picture of the movie (ffprobe gives a
start time of 0 for the video and the audio of every MVE). Only a language
other than English shows subtitles. The text is drawn with the medium font of
the front end (`FNTEMFM_`), white with a black outline, centered, the last line
8 pixels above the bottom of the movie picture, in the 8-bit picture (original
and native forms) and scaled into the upscaled picture. The time of a cue is
the time of the picture (frame number times the frame time).

Speech checked with whisper.cpp (large-v3, Silero voice detection): `INTRO2.MVE`
(9 cues, 3.7 to 33.7 s) and `CREDITV1.MVE` (20 cues, 14.2 to 88.0 s) have
speech; `LOGO22S.MVE` (equal to `IPLOGO.MVE`), `TLOGOS.MVE` and `FLAM1.MVE`
have none.

## Fonts

The fonts of the game have the characters 32 to 126 (and some icons at 127 to
135), except `FNTEMFS_` (6x8, mono, fixed), which has all 256 characters of code
page 437. Checked: the glyphs at 128 to 135 of `FNTEMFM_`, `FNTECLMS`,
`FNTEMLS_` are icons (circles, frames, arrows), not the letters of code page 437;
`FNTEMFS_` has the code page 437 letters (for example 160 a acute, 164 n tilde,
168 the inverted question mark). `FNTEMFL_` (large) and `FNTEMLS_` have capitals
in place of the small letters.

At load time (`gr_init_font`, `engine/2d/font.c`) each font gets the Latin-1
letters a e i o u with acute, grave, circumflex and diaeresis, n with tilde,
A E I O U with acute, U with diaeresis, N with tilde, c and C with cedilla,
and the inverted ! and ?: in `FNTEMFS_` a copy of the code page 437 glyph (the
letters it has), else the base letter with an accent drawn in the color of the
glyph (an i loses its dot first; a letter with no free row above it is made
lower by the height of the accent; the inverted marks are the glyphs upside
down). The other new characters (161 to 255) are drawn as a space. The font
files do not change. The text functions read the bytes of a string as unsigned
values (the Latin-1 letters are above 127).

## Pictures of the language

Some interface pictures have English words painted into them (the buttons of
the play screen, the labels of the character creation, the market buttons).
The list of the pictures and of the language versions on the disc is in
`docs/re/frontend.md`, section "The words in the pictures". The pictures of a
language are in `<lang dir>/<code>/` (the lang dir is the one of the catalog,
above; the content pack installs them as `<hd>/lang/es/`):

| File | Content |
|---|---|
| `orig/febgnd_<n>.png` | the 8-bit picture n of `FEBGND.DAT` |
| `orig/egg_<name>.png` | the 8-bit bitmap `<name>` (small letters) of `PORTAL.EGG` |
| `ui/febgnd_<n>.png` | the upscaled picture n: RGBA of the size of `<hd>/ui/febgnd_<n>.png`, drawn over it with its alpha when the texture is made |

An 8-bit file is a PNG (gray and alpha, 8 bits) of the size of the picture:
the gray value is the palette index, a pixel with alpha 0 keeps the pixel of
the game file (alpha 128 or more: the index of the file). A file with alpha
255 on every pixel is the whole picture. The program reads the files with
stb_image (`engine/main/dtulangart.c`); a file of another size is not used.
English has no files.

Where the program uses them:

- `febgnd_load` (`dtudisp.c`, all FEBGND pictures of the play screen, the
  menus and the front end) changes the pixels of picture n after the load,
  before the pixels become the reference of the upscaled picture. So the
  upscaled picture is shown where the frame has the pixels of the language
  picture.
- The button sheet of the play screen is FEBGND 70 + language (640x480) or
  78 + language (320x200), language 4 for Spanish (the tables 0x145718 and
  0x145734 of DTUGAME.EXE; the sheets on the disc for the languages 4 to 6
  are empty templates). The port loads the sheet of the language if the
  language has the file `orig/febgnd_74.png` (or 82), else the English sheet.
  A change of the language loads the sheet and the picture of the play
  screen again at the next change of the mode.
- `fe_egg_bitmap` (`dtufe.c`): a name whose last letter is `e` (the English
  letter of 0xAEDC3) takes the letter of the language (`efgi123`, 0x145750:
  `1` for Spanish) if the language has the file `orig/egg_<that name>.png`;
  the file is the whole bitmap (the Spanish bitmaps on the disc are empty
  templates).
- The upscaled interface (`dtuhdui.c`, `plat_gpu_ui_picture`) draws
  `ui/febgnd_<n>.png` over the upscaled file when it makes the texture; a
  change of the language makes the texture again.

`tools/ui_lang.py` makes the files (Codex CLI image generation on crops of
the upscaled pictures, see its comment); `tools/content_pack.py lang_es`
packs them. The name of the city on the Waterdeep picture (FEBGND 10, the
job `p10_title`) is drawn by `tools/ui_lang_title.py` (the old letters
filled with the parchment, the new words in a bold serif font with the
color and the shading of the old letters), then `ui_lang.py build` makes
the files as for the other jobs.

## The thief skills of a language

The statistics screen and the thief points screen show the long official
names of the skills in a language other than English (`Esconderse en las
sombras`, `Encontrar/desactivar trampas`). The statistics screen
(`dtudisp.c`, `skill_row`) draws the percentage right-aligned in the row
and the name in the room left of it, in two lines when it is wider; the low
resolution has no room for two lines and cuts the name. The thief points
screen (`dtuthiefpts.c`) makes its rows wider, into the free parchment (at
most 22 pixels on each side in the high resolution, 10 in the low). Both
screens use the texts 678 to 682 and the port strings of the names (also in
the low resolution, where English shows the short `Find Traps`); the port
string `Find Traps` is the name of the spell (`dtuspell.c`). English does
not change.
