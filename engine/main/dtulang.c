/*
 * ASCENT: new file. The language of the text (see dtulang.h and
 * docs/formats/lang.md).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "types.h"
#include "args.h"
#include "text.h"
#include "dtufile.h"
#include "dtuopts.h"
#include "dtulog.h"
#include "dtulang.h"

int Dtu_language = DTU_LANG_EN;

static const char *Lang_codes[DTU_LANG_COUNT] = { "en", "es" };
static const char *Lang_labels[DTU_LANG_COUNT] = { "English", "Espa\xF1ol" };

// The translations of the text of the code (engine/lang/<code>_port.catalog,
// made into C by cmake/embed_text.cmake). The text is UTF-8.
extern const unsigned char Lang_port_es[];

const char *dtu_lang_code(int lang)
{
	return lang >= 0 && lang < DTU_LANG_COUNT ? Lang_codes[lang] : "en";
}

const char *dtu_lang_label(int lang)
{
	return lang >= 0 && lang < DTU_LANG_COUNT ? Lang_labels[lang] : "English";
}

int dtu_lang_from_code(const char *code)
{
	int i;

	for (i = 0; i < DTU_LANG_COUNT; i++)
		if (!stricmp(code, Lang_codes[i]))
			return i;
	return -1;
}

//------------------------------------------------------------------------
// The table of the entries: an open hash table. The key of a text of a
// game file is its id; the key of a text of the code or of a name is the
// byte 1 and the English text.

typedef struct {
	char *key;
	char *en;		// the English text of the catalog (game bytes), or NULL
	char *tr;		// the translation (Latin-1)
} lang_entry;

static lang_entry *Tab;
static int Tab_size, Tab_used;
static int Loaded_lang = -1;

static unsigned hash_str(const char *s)
{
	unsigned h = 2166136261u;

	while (*s)
		h = (h ^ (ubyte) *s++) * 16777619u;
	return h;
}

static void tab_clear(void)
{
	int i;

	for (i = 0; i < Tab_size; i++)
		if (Tab[i].key) {
			free(Tab[i].key);
			free(Tab[i].en);
			free(Tab[i].tr);
		}
	free(Tab);
	Tab = NULL;
	Tab_size = Tab_used = 0;
}

static lang_entry *tab_find(const char *key)
{
	unsigned i;

	if (!Tab_size)
		return NULL;
	for (i = hash_str(key) & (Tab_size - 1); Tab[i].key; i = (i + 1) & (Tab_size - 1))
		if (!strcmp(Tab[i].key, key))
			return &Tab[i];
	return NULL;
}

static void tab_put(const char *key, const char *en, const char *tr)
{
	lang_entry *e;
	unsigned i;

	if (Tab_used * 2 >= Tab_size) {
		lang_entry *old = Tab;
		int old_size = Tab_size, k;
		Tab_size = Tab_size ? Tab_size * 2 : 4096;
		Tab = calloc(Tab_size, sizeof(lang_entry));
		if (!Tab) {
			Tab = old;
			Tab_size = old_size;
			return;
		}
		for (k = 0; k < old_size; k++)
			if (old[k].key) {
				for (i = hash_str(old[k].key) & (Tab_size - 1); Tab[i].key; i = (i + 1) & (Tab_size - 1))
					;
				Tab[i] = old[k];
			}
		free(old);
	}
	if ((e = tab_find(key)) != NULL) {
		free(e->en);
		free(e->tr);
	} else {
		for (i = hash_str(key) & (Tab_size - 1); Tab[i].key; i = (i + 1) & (Tab_size - 1))
			;
		e = &Tab[i];
		e->key = strdup(key);
		Tab_used++;
	}
	e->en = en ? strdup(en) : NULL;
	e->tr = strdup(tr);
}

//------------------------------------------------------------------------
// The catalog file (docs/formats/lang.md): UTF-8, entries of lines
// "key: value"; the keys id, en and tr are read. The values have the
// escapes \n, \t, \\ and \xNN (a byte of the game text), and the ids gab.*
// the words {name}, {class}, ... of the script tokens 0x09 to 0x11.

static const char *Gab_tokens[9] = { "name", "class", "classes", "race", "races", "boy", "man", "sir", "gentleman" };

// One code point of UTF-8 to Latin-1. Returns the number of bytes read.
static int utf8_char(const ubyte *s, int *out)
{
	int c = s[0], n, k, cp;

	if (c < 0x80) { *out = c; return 1; }
	if ((c & 0xE0) == 0xC0) { n = 1; cp = c & 0x1F; }
	else if ((c & 0xF0) == 0xE0) { n = 2; cp = c & 0x0F; }
	else if ((c & 0xF8) == 0xF0) { n = 3; cp = c & 0x07; }
	else { *out = '?'; return 1; }
	for (k = 1; k <= n; k++) {
		if ((s[k] & 0xC0) != 0x80) { *out = '?'; return k; }
		cp = (cp << 6) | (s[k] & 0x3F);
	}
	switch (cp) {
		case 0x2018: case 0x2019: case 0x201A: case 0x2032: cp = '\''; break;
		case 0x201C: case 0x201D: case 0x201E: case 0x2033: cp = '"'; break;
		case 0x2013: case 0x2014: case 0x2212: cp = '-'; break;
		case 0x00A0: cp = ' '; break;
	}
	*out = cp < 256 ? cp : '?';
	return n + 1;
}

static int hexval(int c)
{
	if (c >= '0' && c <= '9') return c - '0';
	if (c >= 'a' && c <= 'f') return c - 'a' + 10;
	if (c >= 'A' && c <= 'F') return c - 'A' + 10;
	return -1;
}

// The value of a catalog line to game bytes. tokens: the words of the
// script tokens are read.
static void unescape(const char *in, char *out, int size, int tokens)
{
	const ubyte *s = (const ubyte *) in;
	int n = 0, c;

	while (*s && n < size - 4) {
		if (*s == '\\' && s[1]) {
			s++;
			if (*s == 'n') { out[n++] = '\n'; s++; }
			else if (*s == 't') { out[n++] = '\t'; s++; }
			else if (*s == 'x' && hexval(s[1]) >= 0 && hexval(s[2]) >= 0) {
				out[n++] = (char) (hexval(s[1]) * 16 + hexval(s[2]));
				s += 3;
			} else
				out[n++] = (char) *s++;
			continue;
		}
		if (tokens && *s == '{') {
			int k, done = 0;
			for (k = 0; k < 9; k++) {
				int l = (int) strlen(Gab_tokens[k]);
				if (!strnicmp((const char *) s + 1, Gab_tokens[k], l) && s[1 + l] == '}') {
					out[n++] = (char) (9 + k);
					s += l + 2;
					done = 1;
					break;
				}
			}
			if (done)
				continue;
		}
		s += utf8_char(s, &c);
		out[n++] = (char) c;
	}
	out[n] = 0;
}

static int Load_entries;

// One entry: put it into the table
static void add_entry(const char *id, const char *en_raw, const char *tr_raw)
{
	static char en[8192], tr[8192], key[8200];
	int tokens = !strncmp(id, "gab.", 4);

	if (!id[0] || !tr_raw || (!tr_raw[0] && en_raw && en_raw[0]))
		return;		// an empty translation: no translation
	unescape(tr_raw, tr, sizeof(tr), tokens);
	if (en_raw)
		unescape(en_raw, en, sizeof(en), tokens);
	if (!strncmp(id, "port.", 5) || !strncmp(id, "tbl.", 4)) {
		// found by the English text
		if (!en_raw)
			return;
		key[0] = !strncmp(id, "tbl.", 4) ? 2 : 1;
		snprintf(key + 1, sizeof(key) - 1, "%s", en);
		tab_put(key, NULL, tr);
	} else
		tab_put(id, en_raw ? en : NULL, tr);
	Load_entries++;
}

// Reads a catalog from memory (the text ends with a 0 byte).
static void parse_catalog(char *text)
{
	char *line = text, *next, id[128] = "", *en = NULL, *tr = NULL;

	for (; line && *line; line = next) {
		char *e;
		next = strchr(line, '\n');
		if (next)
			*next++ = 0;
		e = line + strlen(line);
		while (e > line && e[-1] == '\r')
			*--e = 0;
		if (line[0] == '#' )
			continue;
		if (!line[0]) {
			if (id[0] && tr)
				add_entry(id, en, tr);
			id[0] = 0;
			en = tr = NULL;
			continue;
		}
		if (!strncmp(line, "id:", 3)) {
			if (id[0] && tr)
				add_entry(id, en, tr);
			en = tr = NULL;
			snprintf(id, sizeof(id), "%s", line + 3 + (line[3] == ' '));
		} else if (!strncmp(line, "en:", 3))
			en = line + 3 + (line[3] == ' ');
		else if (!strncmp(line, "tr:", 3))
			tr = line + 3 + (line[3] == ' ');
	}
	if (id[0] && tr)
		add_entry(id, en, tr);
}

static int is_dir(const char *path)
{
	struct stat st;

	return stat(path, &st) == 0 && (st.st_mode & S_IFMT) == S_IFDIR;
}

static int file_exists(const char *path)
{
	struct stat st;

	return stat(path, &st) == 0 && (st.st_mode & S_IFMT) == S_IFREG;
}

char *dtu_lang_path(char *out, int size, const char *file)
{
	char dir[700];
	const char *d;
	int i;

	if ((i = FindArg("-langdir")) != 0 && i + 1 < Num_args) {
		snprintf(out, size, "%s/%s", Args[i + 1], file);
		if (file_exists(out))
			return out;
	}
	if (dtu_user_dir()) {
		snprintf(out, size, "%s/lang/%s", dtu_user_dir(), file);
		if (file_exists(out))
			return out;
	} else {
		snprintf(out, size, "lang/%s", file);
		if (file_exists(out))
			return out;
	}
	if ((d = dtu_hd_dir()) != NULL) {
		snprintf(out, size, "%s/lang/%s", d, file);
		if (file_exists(out))
			return out;
	}
	for (i = 0; (d = dtu_data_dir(i)) != NULL; i++) {
		snprintf(dir, sizeof(dir), "%s/lang", d);
		if (!is_dir(dir))
			continue;
		snprintf(out, size, "%s/%s", dir, file);
		if (file_exists(out))
			return out;
	}
	if ((d = dtu_install_dir()) != NULL) {
		snprintf(out, size, "%s/lang/%s", d, file);
		if (file_exists(out))
			return out;
	}
	return NULL;
}

static char *read_text_file(const char *path)
{
	FILE *f = fopen(path, "rb");
	long size;
	char *p;

	if (!f)
		return NULL;
	fseek(f, 0, SEEK_END);
	size = ftell(f);
	fseek(f, 0, SEEK_SET);
	p = malloc(size > 0 ? size + 1 : 1);
	if (p && size > 0 && fread(p, 1, size, f) != (size_t) size) {
		free(p);
		p = NULL;
	}
	if (p)
		p[size > 0 ? size : 0] = 0;
	fclose(f);
	return p;
}

static void load_language(int lang)
{
	char path[800], name[32];
	char *text;

	if (Loaded_lang == lang)
		return;
	tab_clear();
	Loaded_lang = lang;
	if (lang == DTU_LANG_EN)
		return;
	Load_entries = 0;
	if (lang == DTU_LANG_ES) {
		text = strdup((const char *) Lang_port_es);
		if (text) {
			parse_catalog(text);
			free(text);
		}
	}
	dtu_log("LANG: %s: %d entries of the program\n", dtu_lang_code(lang), Load_entries);
	// The text of the game files (it can also change the text of the code)
	snprintf(name, sizeof(name), "%s.catalog", dtu_lang_code(lang));
	Load_entries = 0;
	if (dtu_lang_path(path, sizeof(path), name) && (text = read_text_file(path)) != NULL) {
		parse_catalog(text);
		free(text);
		dtu_log("LANG: %s: %d entries\n", path, Load_entries);
		dtulog_event("Language %s: %s, %d entries", dtu_lang_code(lang), path, Load_entries);
	} else {
		dtu_log("LANG: no file %s: the text of the game files stays English\n", name);
		dtulog_event("Language %s: no file %s (the text of the game files stays English)", dtu_lang_code(lang), name);
	}
}

//------------------------------------------------------------------------
// Lookups

char *dtu_tr(const char *en)
{
	char key[1024];
	lang_entry *e;

	if (!en || !en[0] || Dtu_language == DTU_LANG_EN)
		return (char *) en;
	load_language(Dtu_language);
	key[0] = 1;
	snprintf(key + 1, sizeof(key) - 1, "%s", en);
	e = tab_find(key);
	return e ? e->tr : (char *) en;
}

char *dtu_tr_name(const char *en)
{
	char key[256];
	lang_entry *e;

	if (!en || !en[0] || Dtu_language == DTU_LANG_EN)
		return (char *) en;
	load_language(Dtu_language);
	key[0] = 2;
	snprintf(key + 1, sizeof(key) - 1, "%s", en);
	e = tab_find(key);
	return e ? e->tr : (char *) en;
}

char *dtu_tr_id(const char *id, const char *en)
{
	lang_entry *e;

	if (Dtu_language == DTU_LANG_EN)
		return (char *) en;
	load_language(Dtu_language);
	e = tab_find(id);
	if (!e || (en && e->en && strcmp(e->en, en)))
		return (char *) en;
	return e->tr;
}

//------------------------------------------------------------------------
// The text file (text.c)

static char *Text_en[N_TEXT_STRINGS];

static void apply_text(void)
{
	char id[16];
	int i;

	for (i = 0; i < N_TEXT_STRINGS; i++) {
		if (!Text_en[i])
			continue;
		snprintf(id, sizeof(id), "tex.%d", i);
		Text_string[i] = dtu_tr_id(id, Text_en[i]);
	}
}

void dtu_lang_text_loaded(void)
{
	int i;

	for (i = 0; i < N_TEXT_STRINGS; i++)
		Text_en[i] = Text_string[i];
	apply_text();
}

char *dtu_lang_text_en(int n)
{
	if (n < 0 || n >= N_TEXT_STRINGS)
		return "";
	return Text_en[n] ? Text_en[n] : Text_string[n];
}

void dtu_lang_set(int lang)
{
	if (lang < 0 || lang >= DTU_LANG_COUNT)
		lang = DTU_LANG_EN;
	Dtu_language = lang;
	load_language(lang);
	apply_text();
}

//------------------------------------------------------------------------
// Subtitles (SRT): "N", "HH:MM:SS,mmm --> HH:MM:SS,mmm", the lines of the
// text, a blank line.

#define MAX_CUES	200
static dtu_sub_cue Cues[MAX_CUES];

static int srt_time(const char *s, int *ms)
{
	int h, m, sec, f;

	if (sscanf(s, "%d:%d:%d%*[,.]%d", &h, &m, &sec, &f) != 4)
		return 0;
	*ms = ((h * 60 + m) * 60 + sec) * 1000 + f;
	return 1;
}

int dtu_lang_subtitles(const char *movie, const dtu_sub_cue **cues)
{
	char name[80], path[800], base[64], *text, *line, *next;
	int n = 0, state = 0, t;
	const char *dot;

	*cues = Cues;
	if (Dtu_language == DTU_LANG_EN || FindArg("-nosubtitles"))
		return 0;
	dot = strrchr(movie, '.');
	t = dot ? (int) (dot - movie) : (int) strlen(movie);
	if (t >= (int) sizeof(base))
		return 0;
	memcpy(base, movie, t);
	base[t] = 0;
	snprintf(name, sizeof(name), "%s.%s.srt", base, dtu_lang_code(Dtu_language));
	if (!dtu_lang_path(path, sizeof(path), name)) {
		// the names on the disc are upper case
		strupr(base);
		snprintf(name, sizeof(name), "%s.%s.srt", base, dtu_lang_code(Dtu_language));
		if (!dtu_lang_path(path, sizeof(path), name))
			return 0;
	}
	if ((text = read_text_file(path)) == NULL)
		return 0;
	line = text;
	if ((ubyte) line[0] == 0xEF && (ubyte) line[1] == 0xBB && (ubyte) line[2] == 0xBF)
		line += 3;
	for (; line && n < MAX_CUES; line = next) {
		char *e, *arrow;
		next = strchr(line, '\n');
		if (next)
			*next++ = 0;
		e = line + strlen(line);
		while (e > line && (e[-1] == '\r' || e[-1] == ' '))
			*--e = 0;
		if (!line[0]) {
			if (state == 2)
				n++;
			state = 0;
			continue;
		}
		if (state == 0) {
			state = 1;			// the number of the cue
			continue;
		}
		if (state == 1) {
			arrow = strstr(line, "-->");
			if (!arrow || !srt_time(line, &Cues[n].start_ms) || !srt_time(arrow + 3 + strspn(arrow + 3, " "), &Cues[n].end_ms)) {
				state = 3;		// not a cue: skip to the blank line
				continue;
			}
			Cues[n].text[0] = 0;
			state = 2;
			continue;
		}
		if (state == 2) {
			char buf[160];
			int len = (int) strlen(Cues[n].text);
			unescape(line, buf, sizeof(buf), 0);
			snprintf(Cues[n].text + len, sizeof(Cues[n].text) - len, "%s%s", len ? "\n" : "", buf);
		}
	}
	if (state == 2 && n < MAX_CUES)
		n++;
	free(text);
	dtu_log("LANG: %s: %d subtitles\n", path, n);
	return n;
}
