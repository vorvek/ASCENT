/*
 * ASCENT: the language of the text (not in the executable).
 *
 * The English text comes from the game files and from the code. A
 * translation is a catalog (docs/formats/lang.md): for each text an id,
 * the English text and the translation. The text of the game files (the
 * catalog <code>.catalog, made with tools/lang_extract.py from the files of
 * the game, never in the repository) is found by its id: tex.N (DTUE.TEX),
 * fe.N (FETXTE), gab.ID.OFFSET (GABE), say.LEVEL.N (NAME.SAY). The text of
 * the code and the names of the tables (items, spells, scenery) are found
 * by their English text: dtu_tr() and dtu_tr_name(). The translation of
 * the text of the code is part of the program (engine/lang/<code>_port.catalog).
 *
 * Each text that has no translation, or whose English text differs from
 * the English text of the catalog (another version of a file), stays
 * English.
 *
 * The text of a translation is Latin-1 (the fonts get the letters at load
 * time, 2d/font.c).
 */
#ifndef _DTULANG_H
#define _DTULANG_H

#define DTU_LANG_EN		0
#define DTU_LANG_ES		1
#define DTU_LANG_COUNT	2

// The language of the text. ascent.cfg keeps it as "Language=en" or
// "Language=es" (dtuopts.c). The option -language CODE sets it for one run.
extern int Dtu_language;

const char *dtu_lang_code(int lang);		// "en", "es"
const char *dtu_lang_label(int lang);		// the name of the language in that language
int dtu_lang_from_code(const char *code);	// -1 if unknown

// Change the language: load the catalogs and change the strings of the
// text file. The menus that are open keep their text until they are built
// again.
void dtu_lang_set(int lang);

// text.c calls it after it loaded the strings of the text file: it keeps
// the English strings and puts the translations in Text_string[].
void dtu_lang_text_loaded(void);

// The English string of the text file (Text_string[] holds the translation)
char *dtu_lang_text_en(int n);

// Text of the code, by its English text. Returns the translation or en.
char *dtu_tr(const char *en);
// A name of an item, a spell or a scenery object (the name= of a table)
char *dtu_tr_name(const char *en);
// Text of a game file by its id. en is the English text of the file: the
// translation is used only if the catalog has the same English text.
char *dtu_tr_id(const char *id, const char *en);

// The sex of the player in a translation: the token {sex:MALE|FEMALE}
// (docs/formats/lang.md). Only that exact form is a token: "{sex:", the
// male form, one "|", the female form, "}"; a form has no '{', '}' or '|'
// and may be empty. A lookup returns the text with each token resolved for
// the sex of the player (dtu_lang_female()).
//
// The sex of the player: the character being created (the front end sets
// Dtu_lang_sex_ptr to it), else the character (Dtu_char.pa.female).
extern const int *Dtu_lang_sex_ptr;
int dtu_lang_female(void);
// The tokens of s resolved in place (female 0 or 1). Each token is
// resolved once: the result is not read again for tokens.
void dtu_lang_sex_resolve(char *s, int female);
// The tokens of a text that is shown (a formatted message): resolved in
// place for the sex of the player. Does nothing in English.
void dtu_lang_sex_text(char *s);
// Text of the code with the tokens resolved for a given sex (a message
// whose subject is not the player uses the male form).
char *dtu_tr_sex(const char *en, int female);
// The strings of the text file follow a change of the sex of the player
// (called each frame of the game and of the front end).
void dtu_lang_sync(void);

// Markers for tools/lang_extract.py. LT(s) translates at run time. LN(s)
// only marks a string that a menu, a message box or the message box of the
// game translates when it shows it.
#define LT(s)	dtu_tr(s)
#define LN(s)	(s)

// The path of a file of the language directory: -langdir DIR, then "lang"
// in the directory of the user, in the hd directory (dtu_hd_dir), in each
// data directory and next to the executable. Returns NULL if no directory
// has the file.
char *dtu_lang_path(char *out, int size, const char *file);

// Subtitles of a movie: <lang dir>/<MOVIE>.<code>.srt (MOVIE is the name
// without the extension, for example INTRO2). Returns the number of cues;
// the cues stay until the next call.
typedef struct {
	int start_ms, end_ms;
	char text[160];		// Latin-1, lines separated by '\n'
} dtu_sub_cue;
int dtu_lang_subtitles(const char *movie, const dtu_sub_cue **cues);

#endif
