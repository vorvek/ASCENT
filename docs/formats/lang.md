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

Values are one line. Escapes: `\n` line break, `\t` tab, `\\` backslash,
`\xNN` one byte of the game text (for example the icons of the fonts at 128 to
135, and a space at the start or the end of a value, `\x20`). In the ids
`gab.*` the script tokens are words in braces: `{name}` (byte 9, the name of the
player), `{class}` (10), `{classes}` (11), `{race}` (12), `{races}` (13),
`{boy}` (14), `{man}` (15), `{sir}` (16), `{gentleman}` (17)
(docs/re/scripts.md, section 3.1).

The program reads `id`, `en` and `tr` and ignores the other keys. The text of
a translation is changed to Latin-1 (ISO 8859-1); a character outside it
shows as `?`, except the typographic quotes and dashes, which become `'`, `"`
and `-`.

Files:

| File | Content | In the repository |
|---|---|---|
| `gamedata/lang/en.catalog` | all English text with `src`, `ctx`, `max`, `dup` | no (game text) |
| `gamedata/lang/en.part<X>.catalog` | the parts for the translators (`parts.md`) | no |
| `gamedata/lang/<code>.part<X>.catalog` | a translated part | no |
| `gamedata/lang/<code>.catalog` | the merged translation of the game files: `id`, `en`, `tr` | no |
| `gamedata/lang/<MOVIE>.en.srt` | the English transcript of the speech of a movie (SRT) | no |
| `gamedata/lang/<MOVIE>.<code>.srt` | the subtitles of a language | no |
| `engine/lang/<code>_port.catalog` | the translation of the text of the code | yes (our text) |

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
