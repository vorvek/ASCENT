/*
 * ASCENT: new file. The script patches of the class gear (Dtu_class_gear,
 * dtugear.h): Aleena Paladinstar (script 4) sells to a single class mage,
 * Nalbas Oddpepper (script 5) and Mirt the Merchant (script 2) to a single
 * class thief. Not in DTUGAME.EXE 1.3. docs/re/scripts.md, section 10, has
 * the blocks and the global flags.
 *
 * The patches are of the kind of patch_devlin and patch_aleena
 * (dtuscript.c): each checks the size of the record of GABE 1.3 and the bytes
 * that it changes, changes a GOTO or a REPLY to a label of the same length,
 * and appends new blocks at the end of the record. No string of the record
 * moves (the translations know a string by its offset). The new strings
 * have no entry gab.N.OFFSET: text() of dtuscript.c takes their translation
 * by the English text (engine/lang/es_port.catalog).
 *
 * The new blocks are written here with a small emitter of the byte code of
 * docs/re/scripts.md, section 3. A test of a single class uses the class
 * tests 8B to 8E (true if either class of the character matches):
 * "mage AND NOT cleric AND NOT fighter AND NOT thief". The port term FE
 * (dtuscript.c, sc_term) is true if a spell book of the character has the
 * spell. A one-time sale sets a global flag (DTU_GEAR_GF_ of dtugear.h).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "inferno.h"
#include "types.h"
#include "args.h"
#include "dtulang.h"		// LN
#include "dtufile.h"		// dtu_log
#include "dtugear.h"
#include "dtuclassgear.h"
#include "object.h"		// OBJ_WEAPON
#include "dtuchar.h"	// the classes of the character
#include "mem.h"			// the records are allocated by mem_malloc (dtuscript.c): free() must be mem_free()

//------------------------------------------------------------------------
// The emitter

typedef struct {
	ubyte	*b;
	int		n, cap, depth, bad;
} em;

static void eb(em *e, int c)
{
	if (e->bad)
		return;
	if (e->n >= e->cap) {
		int cap = e->cap ? e->cap * 2 : 4096;
		ubyte *b = malloc(cap);		// mem.h has no realloc
		if (!b) {
			e->bad = 1;
			return;
		}
		if (e->b) {
			memcpy(b, e->b, e->n);
			free(e->b);
		}
		e->b = b;
		e->cap = cap;
	}
	e->b[e->n++] = (ubyte) c;
}

// A number: hex digits (lower case) and a 0
static void ehex(em *e, unsigned v)
{
	char s[16];
	int i;

	snprintf(s, sizeof(s), "%x", v);
	for (i = 0; s[i]; i++)
		eb(e, s[i]);
	eb(e, 0);
}

// A text: the bytes and a 0. The first byte must be below 0x7A (text() of
// dtuscript.c reads strings while the next byte is below 0x7A).
static void estr(em *e, const char *s)
{
	if ((ubyte) s[0] >= 0x7A)
		e->bad = 1;
	for (; *s; s++)
		eb(e, (ubyte) *s);
	eb(e, 0);
}

// Statements
static void label(em *e, int l)				{ eb(e, 0xCB); ehex(e, l); e->depth = 0; }
static void endblock(em *e)					{ if (e->depth) e->bad = 1; eb(e, 0xCC); }
static void go(em *e, int l)				{ eb(e, 0xC7); ehex(e, l); }
static void end(em *e)						{ eb(e, 0xC8); }
static void say(em *e, const char *s)		{ eb(e, 0xCD); estr(e, s); }
static void reply(em *e, int l, const char *s)	{ eb(e, 0xCF); ehex(e, l); estr(e, s); }
static void wait_reply(em *e)				{ eb(e, 0xCE); }
static void set_gf(em *e, int n)			{ eb(e, 0xC5); eb(e, 0xF1); ehex(e, n); }
static void clear_gf(em *e, int n)			{ eb(e, 0xC6); eb(e, 0xF1); ehex(e, n); }
static void take_gold(em *e, int n)			{ eb(e, 0xC0); eb(e, 0xA8); ehex(e, n); }

// GIVE class id from segment 0xFFFF: the item goes to the player (the
// classes A4 weapon, A5 armor, A6 misc, A7 spell)
static void give(em *e, int cls, int id, int times)
{
	while (times-- > 0) {
		eb(e, 0xBF);
		eb(e, cls);
		ehex(e, id);
		eb(e, 0xE4);
		ehex(e, 0xFFFF);
	}
}
#define WEAPON	0xA4
#define ARMOR	0xA5
#define MISC	0xA6
#define SPELL	0xA7

// Conditions: IF, the terms, THEN (F3), ELSE and ENDIF of the depth
static void if_(em *e)		{ eb(e, 0xF2); }
static void and_(em *e)		{ eb(e, 0xE1); }
static void then(em *e)		{ eb(e, 0xF3); e->depth++; }
static void else_(em *e)	{ eb(e, 0xF3 + e->depth); }
static void endif(em *e)	{ if (e->depth <= 0) e->bad = 1; eb(e, 0xF8 + e->depth); e->depth--; }

static void t_class(em *e, int c, int neg)	{ if (neg) eb(e, 0xE9); eb(e, 0x8B + c); }
static void t_gf(em *e, int n, int neg)		{ if (neg) eb(e, 0xE9); eb(e, 0xF1); ehex(e, n); }
static void t_gold(em *e, int n, int neg)	{ if (neg) eb(e, 0xE9); eb(e, 0xD1); eb(e, 0xA8); ehex(e, n); }
// the mission number (Fe_khelben_mission) is m or more: "mission + 1 > m"
static void t_mission(em *e, int m)			{ eb(e, 0xA1); eb(e, 0x83); ehex(e, m); }
// the talk counter of the object of the script is 0 (the first talk)
static void t_talks0(em *e)					{ eb(e, 0x97); eb(e, 0x81); ehex(e, 0); }
// the port term FE: a spell book of the character has the spell
static void t_knows(em *e, int spell)		{ eb(e, 0xFE); ehex(e, spell); }

// the character has class c and no other class
static void t_single(em *e, int c)
{
	int k;

	t_class(e, c, 0);
	for (k = 0; k < 4; k++)
		if (k != c) {
			and_(e);
			t_class(e, k, 1);
		}
}

#define CLERIC	0
#define MAGE	1
#define THIEF	3

// "IF the mission is m or more AND NOT gflag f"
static void if_mission_not(em *e, int m, int f)
{
	if_(e);
	t_mission(e, m);
	and_(e);
	t_gf(e, f, 1);
	then(e);
}

// "IF the mission is m or more AND NOT gflag f THEN SET gflag s ENDIF"
static void avail(em *e, int m, int f, int s)
{
	if_mission_not(e, m, f);
	set_gf(e, s);
	endif(e);
}

// "IF the mission is m or more AND NOT gflag f THEN REPLY ENDIF"
static void reply_if(em *e, int m, int f, int l, const char *s)
{
	if_mission_not(e, m, f);
	reply(e, l, s);
	endif(e);
}

static void reply_if_gf(em *e, int f, int l, const char *s)
{
	if_(e);
	t_gf(e, f, 0);
	then(e);
	reply(e, l, s);
	endif(e);
}

// A sale: the gold test (label "short" when the gold is short), the gold,
// the item, the flag of a one-time sale (-1: none), and a GOTO
static void sale(em *e, int l, int price, int short_l, int cls, int id, int times, int flag, int next)
{
	label(e, l);
	if_(e);
	t_gold(e, price, 1);
	then(e);
	go(e, short_l);
	endif(e);
	take_gold(e, price);
	give(e, cls, id, times);
	if (flag >= 0)
		set_gf(e, flag);
	go(e, next);
	endblock(e);
}

// Appends the blocks to a copy of the record. Returns the new record (p is
// freed), or NULL (p stays as it is).
static ubyte *append(ubyte *p, int *size, em *e, int id, const char *what)
{
	ubyte *q = NULL;

	if (e->bad || e->depth)
		dtu_log("Class gear: the blocks of script %d are bad; the script stays as it is\n", id);
	else if ((q = malloc(*size + e->n + 1)) != NULL) {
		memcpy(q, p, *size);
		memcpy(q + *size, e->b, e->n);
		*size += e->n;
		q[*size] = 0;
		free(p);
		dtu_log("Class gear: script %d: %s\n", id, what);
	}
	if (e->b)
		free(e->b);
	return q;
}

//------------------------------------------------------------------------
// Aleena Paladinstar, script 4 (16710 bytes). Label 25 ("Yes, my friend!
// You, indeed, heard correctly.") goes to her list, label 30; the replies
// "What else do you have?" of label 950 (after a purchase) and of label 960
// (not enough gold) go there too. The three now go to label 0x30: a single
// class mage gets the menu of label 0x31, another character label 30.
// Labels 0x30 to 0x48 are free in the record. A wand is sold once at each
// reward stage (DTU_GEAR_GF_MM_WAND + stage: the first stage at the first
// visit, then missions 3, 7, 10, 11, 14, 16, 20 and 29, as the stages of the
// market); her free wand of label 19 is the first one.

#define AL_SIZE			16710
#define AL_GOTO30		0x20C0		// label 25: GOTO 30
#define AL_REPLY950		0x3BC8		// label 950: REPLY -> 30 "What else do you have?"
#define AL_REPLY960		0x3CE2		// label 960: REPLY -> 30 "What else do you have?"
#define AL_ANYTHING		0x3B6		// label 950: "Anything else?"
#define AL_FAREWELL		0x3DE		// label 990: "Ahh...very well."
#define AL_SCROLLS		0x1E		// label 30: her list

static const int Mm_stage[9] = { 0, 3, 7, 10, 11, 14, 16, 20, 29 };

static void aleena_blocks(em *e)
{
	int k;

	// 0x30: the way in
	label(e, 0x30);
	if_(e);
	t_single(e, MAGE);
	then(e);
	go(e, 0x31);
	endif(e);
	go(e, AL_SCROLLS);
	endblock(e);

	// 0x31: the scratch flags of the talk, then the menu
	label(e, 0x31);
	clear_gf(e, DTU_GEAR_GF_TMP_MM);
	clear_gf(e, DTU_GEAR_GF_TMP_PEARL);
	clear_gf(e, DTU_GEAR_GF_TMP_WAND);
	clear_gf(e, DTU_GEAR_GF_TMP_SCROLL);
	for (k = 0; k < 9; k++)
		avail(e, Mm_stage[k], DTU_GEAR_GF_MM_WAND + k, DTU_GEAR_GF_TMP_MM);
	if_(e); t_gf(e, DTU_GEAR_GF_TMP_MM, 0); then(e); set_gf(e, DTU_GEAR_GF_TMP_WAND); endif(e);
	avail(e, 7, DTU_GEAR_GF_PARALYZE, DTU_GEAR_GF_TMP_WAND);
	avail(e, 10, DTU_GEAR_GF_FIREBALLS, DTU_GEAR_GF_TMP_WAND);
	if_(e); t_knows(e, 33); then(e); set_gf(e, DTU_GEAR_GF_TMP_SCROLL); endif(e);
	if_(e); t_mission(e, 10); and_(e); t_knows(e, 32); then(e); set_gf(e, DTU_GEAR_GF_TMP_SCROLL); endif(e);
	if (Dtu_class_gear_loaded) {
		avail(e, 3, DTU_GEAR_GF_PEARL_SOLD, DTU_GEAR_GF_TMP_PEARL);
		avail(e, 10, DTU_GEAR_GF_PEARL_SOLD + 1, DTU_GEAR_GF_TMP_PEARL);
		avail(e, 16, DTU_GEAR_GF_PEARL_SOLD + 2, DTU_GEAR_GF_TMP_PEARL);
	}
	say(e, LN("Ahh, a fellow student of the Art! For you, \x09" ", I keep a few things besides my scrolls. Can I interest you in any of them?"));
	reply(e, AL_SCROLLS, LN("I'd like to see your scrolls."));
	reply_if_gf(e, DTU_GEAR_GF_TMP_WAND, 0x32, LN("Have you any wands to spare?"));
	reply_if_gf(e, DTU_GEAR_GF_TMP_SCROLL, 0x35, LN("Scrolls of spells I already know, perchance?"));
	reply_if_gf(e, DTU_GEAR_GF_TMP_PEARL, 0x36, LN("What is that pearl you wear?"));
	reply_if(e, 10, DTU_GEAR_GF_BRACERS, 0x37, LN("Those bracers on your wrists..."));
	reply(e, AL_FAREWELL, LN("Nothing today."));
	wait_reply(e);
	endblock(e);

	// 0x32 to 0x34: the wands, the first time with her first line
	label(e, 0x32);
	if_(e);
	t_gf(e, DTU_GEAR_GF_ALEENA, 1);
	then(e);
	set_gf(e, DTU_GEAR_GF_ALEENA);
	go(e, 0x33);
	endif(e);
	go(e, 0x34);
	endblock(e);
	label(e, 0x33);
	say(e, LN("A true wizard, with no sword arm to lean on: just like me! We spellcasters should look after each other. I can let you have this wand for half of what they ask in the Marketplace."));
	wait_reply(e);
	go(e, 0x34);
	endblock(e);
	label(e, 0x34);
	say(e, LN("These are the wands I can spare. Each one is fully charged; I checked them myself."));
	reply_if_gf(e, DTU_GEAR_GF_TMP_MM, 0x40, LN("The Wand of Magic Missiles, for 750 gold."));
	reply_if(e, 7, DTU_GEAR_GF_PARALYZE, 0x41, LN("The Wand of Paralyzation, for 1000 gold."));
	reply_if(e, 10, DTU_GEAR_GF_FIREBALLS, 0x42, LN("The Wand of Fireballs, for 1500 gold."));
	reply(e, 0x31, LN("Something else."));
	wait_reply(e);
	endblock(e);

	// 0x35: the scroll bundles of spells that the mage knows (a GIVE of a
	// known spell gives a scroll in the item list)
	label(e, 0x35);
	say(e, LN("Copies of spells you already know. I pen them myself, so they work flawlessly, every time! Read one, and your memory stays just as it was."));
	if_(e); t_knows(e, 33); then(e); reply(e, 0x43, LN("Five Magic Missile scrolls, for 250 gold.")); endif(e);
	if_(e); t_mission(e, 10); and_(e); t_knows(e, 32); then(e);
	reply(e, 0x44, LN("Three Lightning Bolt scrolls, for 1500 gold."));
	endif(e);
	reply(e, 0x31, LN("Something else."));
	wait_reply(e);
	endblock(e);

	// 0x36: the Pearls of Power
	label(e, 0x36);
	say(e, LN("This is a Pearl of Power. Hold it, think of a spell you have already cast, and it comes back to you! But only once each time you go below, I'm afraid."));
	if (Dtu_class_gear_loaded) {
		reply_if(e, 3, DTU_GEAR_GF_PEARL_SOLD, 0x45, LN("A pearl of the first circle, for 2500 gold."));
		reply_if(e, 10, DTU_GEAR_GF_PEARL_SOLD + 1, 0x46, LN("A pearl of the second circle, for 6000 gold."));
		reply_if(e, 16, DTU_GEAR_GF_PEARL_SOLD + 2, 0x47, LN("A pearl of the third circle, for 12000 gold."));
	}
	reply(e, 0x31, LN("Something else."));
	wait_reply(e);
	endblock(e);

	// 0x37: the Bracers of Defense AC2
	label(e, 0x37);
	say(e, LN("My Bracers of Defense. Father had a pair made so I would be safe in the city, and then a second pair, just in case. They turn a blade as well as any mail, and they never get in the way of a spell. Five thousand gold."));
	reply(e, 0x48, LN("I'll take them."));
	reply(e, 0x31, LN("Something else."));
	wait_reply(e);
	endblock(e);

	// 0x3D: after a pearl, 0x3E: after another sale (then label 950,
	// "Anything else?"), 0x3F: not enough gold
	label(e, 0x3D);
	say(e, LN("Keep it close. It grows warm when it is ready again."));
	wait_reply(e);
	go(e, AL_ANYTHING);
	endblock(e);
	label(e, 0x3E);
	say(e, LN("There you are! Use it well, and come back and tell me all about it. I envy you, \x10" "."));
	wait_reply(e);
	go(e, AL_ANYTHING);
	endblock(e);
	label(e, 0x3F);
	say(e, LN("I'm afraid you don't have enough currency. I can't go any lower, not even for a fellow wizard."));
	reply(e, 0x31, LN("What else do you have?"));
	reply(e, AL_FAREWELL, LN("I'll be back."));
	wait_reply(e);
	endblock(e);

	// 0x40: the Wand of Magic Missiles (weapon 55) sets the first flag of
	// the stages that is clear
	label(e, 0x40);
	if_(e); t_gold(e, 750, 1); then(e); go(e, 0x3F); endif(e);
	take_gold(e, 750);
	give(e, WEAPON, 55, 1);
	for (k = 0; k < 9; k++) {
		if_(e);
		t_gf(e, DTU_GEAR_GF_MM_WAND + k, 1);
		then(e);
		set_gf(e, DTU_GEAR_GF_MM_WAND + k);
		go(e, 0x3E);
		endif(e);
	}
	go(e, 0x3E);
	endblock(e);
	sale(e, 0x41, 1000, 0x3F, WEAPON, 61, 1, DTU_GEAR_GF_PARALYZE, 0x3E);		// Wand of Paralyzation
	sale(e, 0x42, 1500, 0x3F, WEAPON, 58, 1, DTU_GEAR_GF_FIREBALLS, 0x3E);	// Wand of Fireballs
	sale(e, 0x43, 250, 0x3F, SPELL, 33, 5, -1, 0x3E);							// 5 scrolls of Magic Missile
	sale(e, 0x44, 1500, 0x3F, SPELL, 32, 3, -1, 0x3E);							// 3 scrolls of Lightning Bolt
	if (Dtu_class_gear_loaded) {
		sale(e, 0x45, 2500, 0x3F, MISC, DTU_GEAR_PEARL_POWER, 1, DTU_GEAR_GF_PEARL_SOLD, 0x3D);
		sale(e, 0x46, 6000, 0x3F, MISC, DTU_GEAR_PEARL_POWER + 1, 1, DTU_GEAR_GF_PEARL_SOLD + 1, 0x3D);
		sale(e, 0x47, 12000, 0x3F, MISC, DTU_GEAR_PEARL_POWER + 2, 1, DTU_GEAR_GF_PEARL_SOLD + 2, 0x3D);
	}
	sale(e, 0x48, 5000, 0x3F, ARMOR, 42, 1, DTU_GEAR_GF_BRACERS, 0x3E);		// Bracers of Defense AC2
}

ubyte *dtu_class_gear_aleena(ubyte *p, int *size, int orig)
{
	static const ubyte go30[4] = { 0xC7, '1', 'e', 0 }, re30[4] = { 0xCF, '1', 'e', 0 };
	em e;
	ubyte *q;

	if (!dtu_class_gear_on() || orig != AL_SIZE || memcmp(p + AL_GOTO30, go30, 4) || memcmp(p + AL_REPLY950, re30, 4) ||
			memcmp(p + AL_REPLY960, re30, 4))
		return p;
	memset(&e, 0, sizeof(e));
	aleena_blocks(&e);
	if ((q = append(p, size, &e, 4, Dtu_class_gear_loaded ? "Aleena sells to a single class mage (Dtu_class_gear)" :
			"Aleena sells to a single class mage, no pearls (Dtu_class_gear, the class gear lines are not loaded)")) == NULL)
		return p;
	q[AL_GOTO30 + 1] = '3'; q[AL_GOTO30 + 2] = '0';
	q[AL_REPLY950 + 1] = '3'; q[AL_REPLY950 + 2] = '0';
	q[AL_REPLY960 + 1] = '3'; q[AL_REPLY960 + 2] = '0';
	return q;
}

//------------------------------------------------------------------------
// Nalbas Oddpepper, script 5 (6055 bytes). At the first talk the labels 1,
// 2 and 3 go to label 15 (the Potion of Healing for 40 gold); at a later
// talk label 999 goes to label 4 (the diamond, then the news). The four
// GOTO now go to label 0: a single class thief gets his offer (label 0x20
// at the first talk, then label 15; label 0x21 later, with the reply "Just
// saying hello." to label 4). Labels 0 and 0x20 to 0x2F are free.

#define NB_SIZE		6055
static const int Nb_goto_f[3] = { 0x254, 0x3B1, 0x57B };		// labels 1, 2, 3: GOTO 15
#define NB_GOTO4	0x141										// label 999: GOTO 4

static void nalbas_blocks(em *e)
{
	int loaded = Dtu_class_gear_loaded && dtu_gear_poison_arrow_line() >= 0;

	// 0: the way in
	label(e, 0);
	if_(e);
	t_talks0(e);
	then(e);
	if (loaded) {
		if_(e);
		t_single(e, THIEF);
		then(e);
		go(e, 0x20);
		endif(e);
	}
	go(e, 0xF);
	endif(e);
	clear_gf(e, DTU_GEAR_GF_TMP_NALBAS);
	if (loaded) {
		if_(e); t_single(e, THIEF); then(e); set_gf(e, DTU_GEAR_GF_TMP_NALBAS); endif(e);
	}
	avail(e, 7, DTU_GEAR_GF_INVIS + 2, DTU_GEAR_GF_TMP_NALBAS);
	if_(e);
	t_single(e, THIEF);
	and_(e);
	t_gf(e, DTU_GEAR_GF_TMP_NALBAS, 0);
	then(e);
	go(e, 0x21);
	endif(e);
	go(e, 4);
	endblock(e);

	// 0x20: the first talk, 0x24: the bow later
	if (loaded) {
		label(e, 0x20);
		say(e, LN("Ah, a professional! I can always tell; I was one myself, once. For a rogue of your obvious refinement, I have a little bow and sixteen arrows dipped in something unkind. Three hundred gold, and I never saw you."));
		reply(e, 0x27, LN("I'll take the bow and the arrows."));
		reply(e, 0xF, LN("No thanks."));
		wait_reply(e);
		endblock(e);
		label(e, 0x24);
		say(e, LN("Ah, a professional! I can always tell; I was one myself, once. For a rogue of your obvious refinement, I have a little bow and sixteen arrows dipped in something unkind. Three hundred gold, and I never saw you."));
		reply(e, 0x27, LN("I'll take the bow and the arrows."));
		reply(e, 0x21, LN("Something else."));
		wait_reply(e);
		endblock(e);
	}

	// 0x21: the menu of a later talk
	label(e, 0x21);
	say(e, LN("Hullo, friend. I hope you are profiting from your adventures. Might a professional need anything from my little stock?"));
	if (loaded) {
		if_(e); t_gf(e, DTU_GEAR_GF_BOW, 1); then(e); reply(e, 0x24, LN("About that bow and the arrows...")); endif(e);
		reply_if_gf(e, DTU_GEAR_GF_BOW, 0x25, LN("More of those arrows."));
	}
	reply_if(e, 7, DTU_GEAR_GF_INVIS + 2, 0x26, LN("Have you any potions of invisibility?"));
	reply(e, 4, LN("Just saying hello."));
	wait_reply(e);
	endblock(e);

	if (loaded) {
		label(e, 0x25);
		say(e, LN("More arrows? Eight for one hundred sixty. The recipe is an Oddpepper family secret, and the family is not proud of it."));
		reply(e, 0x28, LN("Eight more, then."));
		reply(e, 0x21, LN("Something else."));
		wait_reply(e);
		endblock(e);
	}
	label(e, 0x26);
	say(e, LN("Potions of invisibility. I put three by for a rainy day, but you seem to have more rainy days than I. Four hundred gold each."));
	reply(e, 0x29, LN("One, please."));
	reply(e, 0x21, LN("Something else."));
	wait_reply(e);
	endblock(e);

	// 0x27: the bow (misc 89) and 16 Poisoned Arrows, then label 15 at the
	// first talk
	if (loaded) {
		label(e, 0x27);
		if_(e); t_gold(e, 300, 1); then(e); go(e, 0x2F); endif(e);
		take_gold(e, 300);
		give(e, MISC, 89, 1);
		give(e, WEAPON, dtu_gear_poison_arrow_line(), 16);
		set_gf(e, DTU_GEAR_GF_BOW);
		say(e, LN("The bargain is sealed! Aim for the soft parts, and whatever you do, do not lick your fingers."));
		wait_reply(e);
		if_(e); t_talks0(e); then(e); go(e, 0xF); endif(e);
		go(e, 0x21);
		endblock(e);
		label(e, 0x28);
		if_(e); t_gold(e, 160, 1); then(e); go(e, 0x2F); endif(e);
		take_gold(e, 160);
		give(e, WEAPON, dtu_gear_poison_arrow_line(), 8);
		say(e, LN("Eight more, freshly dipped. Mind the tips."));
		wait_reply(e);
		go(e, 0x21);
		endblock(e);
	}
	// 0x29: one Potion of Invisibility (misc 83), three in all
	label(e, 0x29);
	if_(e); t_gold(e, 400, 1); then(e); go(e, 0x2F); endif(e);
	take_gold(e, 400);
	give(e, MISC, 83, 1);
	if_(e); t_gf(e, DTU_GEAR_GF_INVIS, 1); then(e); set_gf(e, DTU_GEAR_GF_INVIS); go(e, 0x2A); endif(e);
	if_(e); t_gf(e, DTU_GEAR_GF_INVIS + 1, 1); then(e); set_gf(e, DTU_GEAR_GF_INVIS + 1); go(e, 0x2A); endif(e);
	set_gf(e, DTU_GEAR_GF_INVIS + 2);
	go(e, 0x2A);
	endblock(e);
	label(e, 0x2A);
	say(e, LN("One potion of invisibility. If anyone asks, you never saw me either."));
	wait_reply(e);
	go(e, 0x21);
	endblock(e);

	// 0x2F: not enough gold
	label(e, 0x2F);
	say(e, LN("Alas, your purse is lighter than your fingers, friend. Come back when it has caught up with them."));
	wait_reply(e);
	if_(e); t_talks0(e); then(e); go(e, 0xF); endif(e);
	end(e);
	endblock(e);
}

static ubyte *patch_nalbas(ubyte *p, int *size)
{
	static const ubyte gof[3] = { 0xC7, 'f', 0 }, go4[3] = { 0xC7, '4', 0 };
	em e;
	ubyte *q;
	int k;

	if (*size != NB_SIZE || p[*size - 1] != 0xCC || memcmp(p + NB_GOTO4, go4, 3))
		return p;
	for (k = 0; k < 3; k++)
		if (memcmp(p + Nb_goto_f[k], gof, 3))
			return p;
	memset(&e, 0, sizeof(e));
	nalbas_blocks(&e);
	if ((q = append(p, size, &e, 5, Dtu_class_gear_loaded ? "Nalbas sells to a single class thief (Dtu_class_gear)" :
			"Nalbas sells potions to a single class thief, no bow (Dtu_class_gear, the class gear lines are not loaded)")) == NULL)
		return p;
	for (k = 0; k < 3; k++)
		q[Nb_goto_f[k] + 1] = '0';
	q[NB_GOTO4 + 1] = '0';
	return q;
}

//------------------------------------------------------------------------
// Mirt the Merchant, script 2 (10877 bytes). At a talk after the first,
// label 999 goes to label 6 (the curse quest, the Banegem, the news). The
// GOTO now goes to label 0: when he has a good for a single class thief
// (from mission 3), label 0x40, his menu, with the reply "Any news, Mirt?"
// to label 6. Labels 0 and 0x40 to 0x51 are free.

#define MI_SIZE		10877
#define MI_GOTO6	0x1D8		// label 999: GOTO 6
#define MI_BYE		0x1C		// label 28: "Then good day to you, {sir}."

static void mirt_blocks(em *e)
{
	int loaded = Dtu_class_gear_loaded;

	label(e, 0);
	clear_gf(e, DTU_GEAR_GF_TMP_MIRT);
	avail(e, 3, DTU_GEAR_GF_LEATHER, DTU_GEAR_GF_TMP_MIRT);
	avail(e, 3, DTU_GEAR_GF_LOCKPICKS, DTU_GEAR_GF_TMP_MIRT);
	if (loaded)
		avail(e, 7, DTU_GEAR_GF_BOOTS, DTU_GEAR_GF_TMP_MIRT);
	if (loaded)
		avail(e, 10, DTU_GEAR_GF_ARCHERY, DTU_GEAR_GF_TMP_MIRT);
	avail(e, 14, DTU_GEAR_GF_SWORD, DTU_GEAR_GF_TMP_MIRT);
	avail(e, 14, DTU_GEAR_GF_GIRDLE, DTU_GEAR_GF_TMP_MIRT);
	if_(e);
	t_single(e, THIEF);
	and_(e);
	t_gf(e, DTU_GEAR_GF_TMP_MIRT, 0);
	then(e);
	go(e, 0x40);
	endif(e);
	go(e, 6);
	endblock(e);

	label(e, 0x40);
	if_(e);
	t_gf(e, DTU_GEAR_GF_MIRT, 1);
	then(e);
	set_gf(e, DTU_GEAR_GF_MIRT);
	go(e, 0x41);
	endif(e);
	go(e, 0x42);
	endblock(e);
	label(e, 0x41);
	say(e, LN("So you work alone, \x0E" ". No spells, no sword-brother at your back. I remember that life, before I repented. I kept a few things from my guild days. They aren't free, mind you; I'm a merchant now."));
	wait_reply(e);
	go(e, 0x42);
	endblock(e);

	label(e, 0x42);
	say(e, LN("Here's what I kept. Have a look, and keep your voice down; Durnan has big ears."));
	reply_if(e, 3, DTU_GEAR_GF_LEATHER, 0x43, LN("The leather armor."));
	reply_if(e, 3, DTU_GEAR_GF_LOCKPICKS, 0x44, LN("The lockpicks."));
	if (loaded)
		reply_if(e, 7, DTU_GEAR_GF_BOOTS, 0x45, LN("The boots."));
	if (loaded)
		reply_if(e, 10, DTU_GEAR_GF_ARCHERY, 0x46, LN("The bracers."));
	reply_if(e, 14, DTU_GEAR_GF_SWORD, 0x47, LN("The short sword."));
	reply_if(e, 14, DTU_GEAR_GF_GIRDLE, 0x50, LN("The girdle."));
	reply(e, 6, LN("Any news, Mirt?"));
	reply(e, MI_BYE, LN("Farewell, Mirt."));
	wait_reply(e);
	endblock(e);

	{
		static const struct { int l; const char *text; int buy, lines; } Offer[6] = {
			{ 0x43, LN("Leather armor, enchanted twice over. It creaks less than you'd think, and in our line of work that matters. Two thousand gold."), 0x48, 0 },
			{ 0x44, LN("My old picks. They opened every lock in the Dock Ward once, and a few in the Castle Ward. Six hundred gold, and don't tell the Lords where you got them."), 0x49, 0 },
			{ 0x45, LN("Elven boots. Walk where you like; nobody hears a thing. I'd wear them still if I could fit into them. Five thousand gold."), 0x4A, 1 },
			{ 0x46, LN("Bracers of Archery. The drow are fond of bowmen in the shadows; with these you can answer them in kind. Seven thousand five hundred gold."), 0x4B, 1 },
			{ 0x47, LN("The Short Sword of Quickness. It strikes twice before the other fellow blinks. Four thousand five hundred gold."), 0x4C, 0 },
			{ 0x50, LN("A Girdle of Hill Giant Strength. It never did much for my waist, but it puts an ogre's arm on a slip of a thief. Six thousand gold."), 0x51, 0 },
		};
		int k;

		for (k = 0; k < 6; k++) {
			if (Offer[k].lines && !loaded)
				continue;
			label(e, Offer[k].l);
			say(e, Offer[k].text);
			reply(e, Offer[k].buy, LN("I accept your bargain."));
			reply(e, 0x42, LN("Let me see the rest."));
			wait_reply(e);
			endblock(e);
		}
	}
	sale(e, 0x48, 2000, 0x4F, ARMOR, 6, 1, DTU_GEAR_GF_LEATHER, 0x4E);		// Leather Armor +2
	sale(e, 0x49, 600, 0x4F, MISC, 12, 1, DTU_GEAR_GF_LOCKPICKS, 0x4E);		// Master Lockpicks
	if (loaded)
		sale(e, 0x4A, 5000, 0x4F, ARMOR, DTU_GEAR_BOOTS_ELVEN, 1, DTU_GEAR_GF_BOOTS, 0x4E);
	if (loaded)
		sale(e, 0x4B, 7500, 0x4F, ARMOR, DTU_GEAR_BRACERS_ARCHERY, 1, DTU_GEAR_GF_ARCHERY, 0x4E);
	sale(e, 0x4C, 4500, 0x4F, WEAPON, 69, 1, DTU_GEAR_GF_SWORD, 0x4E);		// Short Sword of Quickness
	sale(e, 0x51, 6000, 0x4F, ARMOR, 38, 1, DTU_GEAR_GF_GIRDLE, 0x4E);		// Girdle of Hill Giant Strength

	// 0x4E: after a sale (then label 0: the menu, or the news when he has
	// nothing left), 0x4F: not enough gold
	label(e, 0x4E);
	say(e, LN("Very good. If Khelben asks, you found it in the dungeon. Let me know if I can help you further."));
	wait_reply(e);
	go(e, 0);
	endblock(e);
	label(e, 0x4F);
	say(e, LN("You're short, \x0E" ". I gave up charity when I gave up the guild."));
	reply(e, 0x42, LN("Let me see the rest."));
	reply(e, MI_BYE, LN("Farewell, Mirt."));
	wait_reply(e);
	endblock(e);
}

static ubyte *patch_mirt(ubyte *p, int *size)
{
	static const ubyte go6[3] = { 0xC7, '6', 0 };
	em e;
	ubyte *q;

	if (*size != MI_SIZE || p[*size - 1] != 0xCC || memcmp(p + MI_GOTO6, go6, 3))
		return p;
	memset(&e, 0, sizeof(e));
	mirt_blocks(&e);
	if ((q = append(p, size, &e, 2, Dtu_class_gear_loaded ? "Mirt sells to a single class thief (Dtu_class_gear)" :
			"Mirt sells to a single class thief, no boots (Dtu_class_gear, the class gear lines are not loaded)")) == NULL)
		return p;
	q[MI_GOTO6 + 1] = '0';
	return q;
}

ubyte *dtu_class_gear_patch(int id, ubyte *p, int *size)
{
	if (!dtu_class_gear_on())
		return p;
	if (id == 2)
		return patch_mirt(p, size);
	if (id == 5)
		return patch_nalbas(p, size);
	return p;
}

int dtu_class_gear_key(void)
{
	return dtu_class_gear_on() | (Dtu_class_gear_loaded << 1);
}

// The wands of Aleena (label 0x40 to 0x42 above) and her prices
int dtu_class_gear_resale_cap(int type, int id)
{
	static const int wand[][2] = { { 55, 750 }, { 61, 1000 }, { 58, 1500 } };
	int i;

	if (!dtu_class_gear_on() || type != OBJ_WEAPON)
		return 0;
	// a single class mage only: no other character buys from her
	if (Dtu_char.pa.class1 != DTU_CLASS_MAGE || (Dtu_char.pa.class2 >= 0 && Dtu_char.pa.class2 <= 3))
		return 0;
	for (i = 0; i < (int) (sizeof(wand) / sizeof(wand[0])); i++)
		if (wand[i][0] == id)
			return wand[i][1];
	return 0;
}
