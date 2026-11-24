/*
 * ASCENT: new file. The table fixup of DTUGAME.EXE 1.3 (0x1A344).
 *
 * bm_read_all (0x1B2F5) calls 0x1A344 at its end (0x1C8F9), after the
 * level table and the player table are read. 0x1A344 writes fixed values
 * into fixed line numbers of the item, weapon, armor and spell tables:
 * names, values, class masks, spell levels, ranges, radii, flags, damage
 * dice, sounds, and four copies of whole spell lines and two of whole
 * weapon lines. The write has no condition, so 1.3 never shows the table
 * text of these fields. See docs/re/objects.md, "The table fixup".
 *
 * The port reads the table text (bm_tbl_get, bm_tbl_has_flag). So the
 * fixup is ported as a change of the text of the records: each write of
 * 0x1A344 becomes "key=value", a flag word or a name, in the order of the
 * executable (a later write can copy a field that an earlier one changed).
 * The list is Fixups[] in dtutblfix.c. tools/tblfix.py reads it, applies
 * it to a table for the wiki (tools/wiki_rules.py) and prints the change
 * of each record (tools/check_tblfix.py).
 *
 * The line numbers are fixed. Seven older tables (BASE, DEMO, MINES1,
 * MOIN1, PRISON1, SPIDER, TRAIN1) have other lines at some numbers, so
 * 1.3 writes onto other items there. The port applies the writes of a
 * table section (items, armor, weapons, spells) only when each line of
 * the section that the fixup changes or reads has the name of the 39
 * mission tables (the list Guard[] in dtutblfix.c). Else the section
 * stays as it is. BASE, MINES1, PRISON1, SPIDER and TRAIN1 have the weapon
 * and spell lines of the mission tables, so they get those writes.
 *
 * This is 1.3 behavior and always on. The debug option -rawtables skips
 * it (Dtu_raw_tables).
 *
 * This file has no reference to the engine: bmread.c gives the records
 * through dtu_tblfix_io, and test_dtutblfix.c gives its own.
 */
#ifndef _DTUTBLFIX_H
#define _DTUTBLFIX_H

// The tables. The values are the letters of the rows of dtutblfix.c.
#define DTU_TK_MISC		'M'		// $OBJECT
#define DTU_TK_ARMOR		'A'		// $ARMOR
#define DTU_TK_WEAPON	'W'		// $WEAPON
#define DTU_TK_SPELL		'S'		// $SPELL

// 1: -rawtables, the fixup is not applied.
extern int Dtu_raw_tables;

// The records of the tables.
typedef struct dtu_tblfix_io {
	int	(*count)(int kind);								// the number of lines of the table
	char	*(*text)(int kind, int id);					// the text of a line, or NULL
	void	(*set_text)(int kind, int id, char *text);	// a new text (malloc), the call frees the old one
	char	*(*name)(int kind, int id);					// the name buffer of a line (name_size bytes), or NULL
	int	name_size;
	void	(*copy_line)(int kind, int dst, int src);	// all decoded fields but the name and the record
	void	(*sync)(int kind, int id);						// decode the fields that the reader takes from the text again
	void	(*log)(const char *text);						// or NULL
} dtu_tblfix_io;

// Apply the fixup to the tables of io. Returns the number of writes, or -1
// when a line has another name than in the mission tables (nothing is
// changed then).
int dtu_tblfix_apply(const dtu_tblfix_io *io);

// One write of 0x1A344.
typedef struct dtu_tblfix_op {
	int			op;		// T_
	int			kind;		// DTU_TK_, 0 for a loop over a table
	int			id;		// the line
	int			src;		// the line that the write copies from, or -1
	const char	*key;
	const char	*val;
} dtu_tblfix_op;

int dtu_tblfix_n_ops(void);
const dtu_tblfix_op *dtu_tblfix_op_row(int i);

// Text of a record (for the test). Each returns a new text (malloc) or
// NULL when nothing changes.
int dtu_tblfix_get(const char *text, const char *key, char *value, int size);	// 1 if key= is there
int dtu_tblfix_has(const char *text, const char *word);
char *dtu_tblfix_set(const char *text, const char *key, const char *value);	// value NULL: remove key=
char *dtu_tblfix_word(const char *text, const char *word, int on);
int dtu_tblfix_spell_flags(const char *text);		// the word +0x36 of Spell_info that the reader makes of the text
char *dtu_tblfix_set_spell_flags(const char *text, int flags);
int dtu_tblfix_class(const char *text);				// the class mask +0x00
char *dtu_tblfix_set_class(const char *text, int mask);

// Frees a text that this file allocated (C library, not the mem library).
void dtu_tblfix_free(char *text);

#endif
