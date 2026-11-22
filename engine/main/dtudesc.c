/*
 * ASCENT: new file. The description window (dtudesc.h). New in the port:
 * DTUGAME.EXE 1.3 shows only the name of an item in the description bar.
 *
 * The text comes from the values that the game code reads: the table
 * accessors (dtu_item_get, bm_tbl_get, after the table fixup of
 * dtutblfix.c and the item corrections of dtuitemfix.c), the rules of the
 * items of dtuchar.c (dtu_char_misc_use, dtu_char_item_powers,
 * dtu_char_item_fx, dtu_char_item_ac) and the rules of the spells of
 * dtuspell.c (dtu_spell_get_facts). Each line is one of a small set of
 * patterns that go through the language layer (LT, dtulang.h).
 *
 * The window is the picture of the scroll text screen (dtuscroll.c,
 * FEBGND 152 low and 151 high) with the text in its text box, in the game
 * font. docs/description_window.md has the layout rules.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include "inferno.h"
#include "types.h"
#include "fix.h"
#include "gr.h"
#include "key.h"
#include "mouse.h"
#include "timer.h"
#include "gamefont.h"
#include "game.h"
#include "object.h"
#include "bm.h"
#include "weapon.h"		// N_weapon_types
#include "dtuobj.h"
#include "dtuchar.h"
#include "dtuspell.h"
#include "dtuatk.h"			// dtu_atk_state_time: the time of a melee swing
#include "dtudisp.h"		// Dtu_hires, dtu_febgnd_load
#include "dtufe_int.h"		// Fe_font_s
#include "dtuhdui.h"		// the upscaled picture
#include "dtuscroll.h"		// the picture and the text box of the scroll
#include "dtulang.h"
#include "dtufile.h"		// dtu_find_file: the font of the front end for the test aid
#include "dtudesc.h"

#define TEXT_MAX	2048

static char Text[TEXT_MAX];
static int Open = 0;					// 0 closed, 1 in the game, 2 in the front end
static int Time_stopped;
static int Prev_buttons;
static int Pressed;					// a mouse button went down while the window is open
static grs_bitmap *Clean[2];			// the scroll as loaded, low and high
static grs_bitmap *Fe_pic;				// the high scroll in the palette of the front end screen
static ubyte Fe_pal[768];				// the palette that Fe_pic was made for

//------------------------------------------------------------------------
// The text

static char *Out;
static int Out_size, Out_lines;

static void line(const char *format, ...)
{
	int n = (int) strlen(Out);
	va_list ap;

	if (n >= Out_size - 2)
		return;
	if (n)
		Out[n++] = '\n';
	va_start(ap, format);
	vsnprintf(Out + n, Out_size - n, format, ap);
	va_end(ap);
	Out_lines++;
}

// "1d8", "1d8+1", "2d4-1"; dice with 0 sides add 0 (the divide error
// handler of 1.3, see spell_dice in dtuobj.c): only the bonus
static char *dice(char *buf, int n, int sides, int add)
{
	if (n > 0 && sides > 0) {
		sprintf(buf, "%dd%d", n, sides);
		if (add)
			sprintf(buf + strlen(buf), "%+d", add);
	}
	else
		sprintf(buf, "%d", add);
	return buf;
}

static char *range_text(char *buf, int lo, int hi)
{
	if (lo == hi)
		sprintf(buf, "%d", lo);
	else
		sprintf(buf, LT("%d to %d"), lo, hi);
	return buf;
}

// A time in game seconds: seconds below one minute, hours for whole hours,
// else minutes
static char *time_text(char *buf, int lo, int hi)
{
	if (hi < 60) {
		if (lo == hi)
			sprintf(buf, lo == 1 ? LT("1 second") : LT("%d seconds"), lo);
		else
			sprintf(buf, LT("%d to %d seconds"), lo, hi);
	}
	else if (lo % 3600 == 0 && hi % 3600 == 0) {
		if (lo == hi)
			sprintf(buf, lo == 3600 ? LT("1 hour") : LT("%d hours"), lo / 3600);
		else
			sprintf(buf, LT("%d to %d hours"), lo / 3600, hi / 3600);
	}
	else {
		if (lo / 60 == hi / 60)
			sprintf(buf, lo / 60 == 1 ? LT("1 minute") : LT("%d minutes"), lo / 60);
		else
			sprintf(buf, LT("%d to %d minutes"), lo / 60, hi / 60);
	}
	return buf;
}

// A distance in units of the game: 2 units are 1 foot (tools/wiki_rules.py)
static char *feet(char *buf, int units)
{
	sprintf(buf, LT("%d ft"), units / 2);
	return buf;
}

static char *classes(char *buf, int mask)
{
	static const struct { int bit; const char *name; } c[4] = {
		{ DTU_USE_FIGHTER, LN("Fighter") }, { DTU_USE_THIEF, LN("Thief") }, { DTU_USE_MAGE, LN("Mage") }, { DTU_USE_CLERIC, LN("Cleric") } };
	int i, all = 1;

	buf[0] = 0;
	for (i = 0; i < 4; i++)
		if (!(mask & c[i].bit))
			all = 0;
	if (all)
		return strcpy(buf, LT("All classes"));
	for (i = 0; i < 4; i++)
		if (mask & c[i].bit)
			sprintf(buf + strlen(buf), "%s%s", buf[0] ? ", " : "", dtu_tr(c[i].name));
	if (!buf[0])
		strcpy(buf, LT("None"));
	return buf;
}

// The names of the effect bits of a mask, in the language
static char *fx_names(char *buf, int size, int mask)
{
	int b;

	buf[0] = 0;
	for (b = 0; b < 32; b++)
		if (mask & (1 << b)) {
			int n = (int) strlen(buf);
			snprintf(buf + n, size - n, "%s%s", n ? ", " : "", dtu_tr(dtu_spell_fx_name(1 << b)));
		}
	return buf;
}

static char *spell_list(char *buf, int size, const short *s, int n)
{
	int i;

	buf[0] = 0;
	for (i = 0; i < n; i++) {
		int k = (int) strlen(buf);
		snprintf(buf + k, size - k, "%s%s", k ? ", " : "", dtu_spell_name(s[i]));
	}
	return buf;
}

static item_info *item_entry(int type, int id)
{
	if (id < 0)
		return NULL;
	switch (type) {
		case OBJ_MISC:		return id < N_misc_types ? &Misc_info[id] : NULL;
		case OBJ_ARMOR:	return id < N_armor_types ? &Armor_info[id] : NULL;
		case OBJ_SPELL:	return id < N_spell_types ? &Spell_info[id] : NULL;
		case OBJ_WEAPON:	return id < N_weapon_types ? &Weapon_item_info[id] : NULL;
	}
	return NULL;
}

static int tbl_int(int record, char *key, int def)
{
	char v[32];

	if (record < 0 || !bm_tbl_get(record, key, v, sizeof(v)))
		return def;
	return atoi(v);
}

static const char *slot_name(int slot)
{
	static const char *n[DTU_N_SLOTS + 1] = { LN("Lockpicks"), LN("Cloak"), LN("Neck"), LN("Body"), LN("Hands"),
		LN("Shield arm"), LN("Ring finger"), LN("Ring finger"), LN("Waist"), LN("Feet"), LN("Bow"), LN("Crossbow"), LN("Hand") };

	return (slot >= 0 && slot <= DTU_N_SLOTS) ? dtu_tr(n[slot]) : "";
}

// The lines of the powers of a worn item (dtu_char_item_powers)
static void power_lines(int type, int id)
{
	dtu_item_power p[12];
	char buf[64];
	int n = dtu_char_item_powers(type, id, p, 12), i;

	for (i = 0; i < n; i++)
		switch (p[i].kind) {
			case DTU_IP_STRENGTH:
				if (p[i].value2 == 100)
					sprintf(buf, "%d/00", p[i].value);
				else
					sprintf(buf, "%d", p[i].value);
				line(LT("Sets strength to %s."), buf);
				break;
			case DTU_IP_CONSTITUTION:	line(LT("Sets constitution to %d."), p[i].value); break;
			case DTU_IP_DEXTERITY:		line(LT("Raises dexterity.")); break;
			case DTU_IP_HIDE:			line(LT("Sets Hide In Shadows to %d%%."), p[i].value); break;
			case DTU_IP_LOCKS_THIEF:	line(LT("Open Locks +%d%% for a thief."), p[i].value); break;
			case DTU_IP_LOCKS_OTHER:	line(LT("Open Locks %d%% for other classes."), p[i].value); break;
			case DTU_IP_NO_SKILLS:
				line(p[i].value == 1 ? LT("A thief cannot Hide In Shadows, Move Silently or Climb Walls in it.") :
					LT("Find/Remove Traps and Open Locks do not work."));
				break;
			case DTU_IP_TRAPS_CLIMB:	line(LT("Sets Find/Remove Traps and Climb Walls to %d%%."), p[i].value); break;
			case DTU_IP_PROT_CLOAK:	line(LT("No armor class bonus with a shield, magic armor or heavy armor.")); break;
			case DTU_IP_PROT_RING:		line(LT("Of two rings of protection only the better one counts.")); break;
			case DTU_IP_RING_NO_MAGIC:	line(LT("No armor class bonus with magic armor.")); break;
			case DTU_IP_ARMORING:		line(LT("A mage gets 1 better armor class after Armor, Magical Vestment and Shield.")); break;
			case DTU_IP_WIZARDRY:		line(LT("Doubles the mage spell slots of levels 1 to 3.")); break;
			case DTU_IP_USE_SPELL:		line(LT("Use: casts %s around you."), dtu_spell_name(p[i].value)); break;
			case DTU_IP_TURN_UNDEAD:	line(LT("The Turn Undead key works with it.")); break;
			case DTU_IP_INFRA_HUMAN:	line(LT("Infravision only for a human.")); break;
		}
}

static void fx_line(int type, int id, int hand)
{
	char buf[256];
	int fx = dtu_char_item_fx(type, id);

	if (fx)
		line(hand ? LT("In the hand: %s.") : LT("While worn: %s."), fx_names(buf, sizeof(buf), fx));
}

// What one use of a misc item does (dtu_char_misc_use)
static void use_line(const dtu_misc_use *u)
{
	char a[64], t[64], s[256], s2[256];
	int lo = u->t_add * u->t_unit, hi = (u->t_mod > 1 ? u->t_mod - 1 + u->t_add : u->t_add) * u->t_unit;

	if (u->t_mod)
		time_text(t, lo, hi);
	else
		t[0] = 0;
	range_text(a, u->lo, u->hi);
	spell_list(s, sizeof(s), u->spells, u->n_spells);
	switch (u->kind) {
		case DTU_MU_HEAL:		line(LT("Heals %s hit points."), a); break;
		case DTU_MU_DEXTERITY:	line(LT("Dexterity becomes at least %d for %s."), u->lo, t); break;
		case DTU_MU_DRAGON:		line(LT("Strength becomes at least %d for 1 to %d minutes."), u->lo, Dtu_char.pa.class1_level + 1); break;
		case DTU_MU_GIANT:		line(LT("Strength becomes at least %d for %s."), u->lo, t); break;
		case DTU_MU_INTEL:		line(LT("Intelligence becomes %s for %s."), a, t); break;
		case DTU_MU_EFFECT:		line(LT("Gives %s for %s."), fx_names(s2, sizeof(s2), u->fx), t); break;
		case DTU_MU_SPELLS:		line(LT("Casts %s on you."), s); break;
		case DTU_MU_DISPEL:		line(LT("Ends all spell and potion effects on you.")); break;
		case DTU_MU_LUCK:		line(LT("For %s, one attack roll is a 20 or one damage roll is the highest."), t); break;
		case DTU_MU_TOUGH:
			line(LT("Adds %s hit points above the maximum and Bless for %s."), a, t);
			line(LT("A second one while above the maximum can kill (system shock)."));
			break;
		case DTU_MU_GEM:		line(LT("Cures blindness, else casts %s. One use for each mission."), dtu_spell_name(u->lo)); break;
		case DTU_MU_SCROLL:		line(LT("Shows a text.")); break;
		case DTU_MU_CURSED:		line(LT("Harms you by chance: spells, the map, your abilities and hit points. It is not used up.")); break;
		case DTU_MU_TURN:		line(LT("Turns undead as a cleric of level %d."), (Dtu_char.pa.class1_level + 1) * 2 + 1); break;
		case DTU_MU_MAP:		line(LT("Shows the whole map.")); break;
		case DTU_MU_ATTACK:		line(LT("Fires a missile at each creature in sight.")); break;
		case DTU_MU_AREA_SPELLS:	line(LT("Casts %s around you, then %s on you."), dtu_spell_name(u->lo), s); break;
		case DTU_MU_TOUR:		line(LT("The view visits each creature of the level.")); break;
		case DTU_MU_ABILITY:	line(LT("Raises an ability by one point.")); break;
		case DTU_MU_FULL_HEAL:	line(LT("Heals all hit points and casts %s on you."), s); break;
		case DTU_MU_SPELLS_KEEP:	line(LT("Casts %s on you, with no end."), s); break;
		case DTU_MU_QUEST:		line(LT("Quest item. It can be used.")); break;
	}
}

static int charged(const dtu_misc_use *u)
{
	return u && ((u->kind >= DTU_MU_TURN && u->kind <= DTU_MU_FULL_HEAL) || u->kind == DTU_MU_SPELLS_KEEP);
}

static void weapon_text(int id, dtu_item_data *d, int count, int r)
{
	char buf[256], b2[64];
	int pc_spell = tbl_int(r, "pc_spell", -1), range;

	if (pc_spell >= 0 && !d->melee) {
		// a wand: the usable item layout (0x56A0D casts pc_spell, one charge)
		if (count >= 0)
			line(LT("Charges: %d"), count);
		line(LT("Classes: %s"), classes(buf, d->class_mask));
		line(LT("Each use casts %s."), dtu_spell_name(pc_spell));
		return;
	}
	// weapon_attack (dtuchar.c): a weapon with no dice attacks as the empty
	// hand, with no bonus and no fire
	if (d->dice_sides <= 0)
		line(LT("Damage: as the empty hand"));
	else
		line(LT("Damage: %s"), dice(buf, d->dice_n, d->dice_sides, d->damage_bonus));
	if (d->dice_sides > 0 && tbl_int(r, "fire_damage_dice", 0) && tbl_int(r, "#fire_damage_dice", 0))
		line(LT("Fire damage: %s"), dice(buf, tbl_int(r, "#fire_damage_dice", 0), tbl_int(r, "fire_damage_dice", 0), 0));
	sprintf(buf, "%+d", d->to_hit_bonus);
	line(LT("To hit: %s"), buf);
	if (d->melee) {
		// dtucombat.c: the time of the attack state of a melee weapon
		fix t = dtu_atk_state_time(1, d->attack_wait, F1_0, 0, 0);
		sprintf(buf, "%.1f", f2fl(t));
		line(LT("Speed: one attack in %s seconds"), buf);
		line(LT("Melee weapon."));
	}
	else {
		range = f2i(dtu_missile_weapon_range(id));
		feet(b2, range);
		if (tbl_int(r, "thrown", 0) == 1)
			line(LT("Thrown weapon, range %s."), b2);
		else if (bm_tbl_has_flag(r, "arrow"))
			line(LT("Arrow for a bow, range %s."), b2);
		else if (bm_tbl_has_flag(r, "bolt"))
			line(LT("Bolt for a crossbow, range %s."), b2);
		else
			line(LT("Missile, range %s."), b2);
		if (count >= 0 && d->stackable)
			line(LT("Count: %d"), count);
	}
	line(bm_tbl_has_flag(r, "magic") ? LT("Magic.") : LT("Not magic."));
	line(LT("Classes: %s"), classes(buf, d->class_mask));
	if (dtu_char_item_ac(OBJ_WEAPON, id))
		line(LT("Armor class %d better while in the hand."), -dtu_char_item_ac(OBJ_WEAPON, id));
	fx_line(OBJ_WEAPON, id, 1);
	power_lines(OBJ_WEAPON, id);
	// the weapons that the damage code knows by number (0x2AB9F, 0x2B15B,
	// 0x2B1F2, dtuobj.c dtu_weapon_extra_damage)
	if (id == 71)
		line(LT("More damage and to hit against undead and regenerating creatures."));
	if (id == 70)
		line(LT("Can destroy undead creatures."));
	if (id == 45)
		line(LT("A backstab with a roll of 19 or 20 does more damage."));
}

int dtu_desc_item_text(int type, int id, int count, int price, int sell, char *out, int size)
{
	dtu_item_data d;
	item_info *it = item_entry(type, id);
	const dtu_misc_use *u;
	char buf[256];
	int r, slot;

	Out = out;
	Out_size = size;
	Out_lines = 0;
	out[0] = 0;
	if (!it || !dtu_item_get(type, id, &d)) {
		line("?");
		return Out_lines;
	}
	r = it->record;
	// the name as the player sees it (dtuitemfix.h), in the language
	line("%s", dtu_tr_name(dtu_item_shown_name(type, id, d.name)));
	slot = dtu_char_item_slot(type, id);
	u = type == OBJ_MISC ? dtu_char_misc_use(id) : NULL;
	if (type == OBJ_WEAPON)
		weapon_text(id, &d, count, r);
	else if (type == OBJ_SPELL) {
		// a spell in the item list: a scroll that casts it (0x56A0D)
		if (count > 0)
			line(LT("Charges: %d"), count);
		line(LT("Classes: %s"), classes(buf, DTU_USE_ALL));
		line(LT("Each use casts %s."), dtu_spell_name(id));
	}
	else if (slot == DTU_SLOT_BOW || slot == DTU_SLOT_CROSSBOW) {
		// 0x2AB36, 0x2B0EE: the to_hit_bonus of the launcher goes to the
		// attack and the damage of the arrows or the bolts
		sprintf(buf, "%+d", d.to_hit_bonus);
		line(slot == DTU_SLOT_BOW ? LT("Arrows get %s to hit and damage.") : LT("Bolts get %s to hit and damage."), buf);
		line(LT("Slot: %s"), slot_name(slot));
		line(bm_tbl_has_flag(r, "magic") ? LT("Magic.") : LT("Not magic."));
		line(LT("Classes: %s"), classes(buf, d.class_mask));
		fx_line(type, id, 0);
	}
	else if (slot >= 0) {
		int ac = dtu_char_item_ac(type, id);

		if (slot == DTU_SLOT_ARMOR && ac)
			line(LT("Armor class: %d"), 10 + ac);
		else if (ac)
			line(LT("Armor class: %d better"), -ac);
		line(LT("Slot: %s"), slot_name(slot));
		line(LT("Classes: %s"), classes(buf, d.class_mask));
		fx_line(type, id, 0);
		power_lines(type, id);
	}
	else if (u) {
		if (count >= 0)
			line(charged(u) ? LT("Charges: %d") : LT("Count: %d"), count);
		line(LT("Classes: %s"), classes(buf, d.class_mask));
		use_line(u);
	}
	else if (type == OBJ_MISC && DTU_IS_KEY_ITEM(type, id))
		line(LT("Key."));
	else if (tbl_int(r, "plot_item", 0))
		line(LT("Quest item. It cannot be dropped or sold."));		// 0xB611C, the market lists 0x870A0
	else
		line(LT("Item."));
	if (price >= 0)
		line(sell ? LT("Sale price: %d gold") : LT("Price: %d gold"), price);
	return Out_lines;
}

static void spell_effect(int spell, dtu_spell_facts *f)
{
	static const char *types[4] = { LN("Deals %s damage."), LN("Deals %s fire damage."), LN("Deals %s acid damage."),
		LN("Deals %s electric damage.") };
	char a[64], b[256];
	int g, any = 0;

	range_text(a, f->amount_lo, f->amount_hi);
	fx_names(b, sizeof(b), f->fx);
	switch (f->kind) {
		case DTU_SK_DAMAGE:
			for (g = 0; g < 4; g++)
				if ((f->dice_n[g] > 0 && f->dice_sides[g] > 0) || f->dice_add[g]) {
					line(dtu_tr(types[g]), dice(a, f->dice_n[g], f->dice_sides[g], f->dice_add[g]));
					any = 1;
				}
			if (!any)
				line(LT("Deals no damage."));
			if (f->undead_die) {
				sprintf(a, "1d%d", f->undead_die);
				line(LT("%s more damage against undead."), a);
			}
			break;
		case DTU_SK_HEAL:		line(LT("Heals %s hit points."), a); break;
		case DTU_SK_GIVE:		line(LT("Gives %s."), b); break;
		case DTU_SK_CAUSE:		line(LT("Causes %s."), b); break;
		case DTU_SK_AC:
			line(LT("Armor class %d at most."), f->ac);
			if (f->fx == DTU_FX_SHIELD)
				line(LT("Stops Magic Missile."));		// case 33
			break;
		case DTU_SK_ARMOR:		line(LT("Armor class %d at most, until %s points of weapon damage."), f->ac, a); break;
		case DTU_SK_AID:		line(LT("Adds %s hit points above the maximum, and Bless."), a); break;
		case DTU_SK_DISPEL:		line(LT("Ends all spell effects on the target.")); break;
		case DTU_SK_ABJURE:		line(LT("An extraplanar creature leaves the level or dies.")); break;
		case DTU_SK_RARY:		line(LT("Gives more spell slots: three of level 1, one of level 1 and one of level 2, or one of level 3.")); break;
		case DTU_SK_KNOCK:		line(LT("Opens a locked door.")); break;
		case DTU_SK_CURE_SENSES:	line(LT("Cures blindness and deafness.")); break;
		case DTU_SK_FIRE_POOL:	line(LT("Takes the first %s points of fire damage."), a); break;
		case DTU_SK_STONESKIN:	line(LT("Stops the next %s attacks."), a); break;
		case DTU_SK_STRENGTH:	line(LT("Adds %s to strength."), a); break;
		case DTU_SK_BLIND:		line(LT("Blinds the target.")); break;
		case DTU_SK_COLOR:		line(LT("Puts to sleep, blinds or stuns, by the level of the target.")); break;
		case DTU_SK_SLEEP:
		case DTU_SK_SCARE:		line(LT("Causes %s on creatures of level %d or less."), b, f->amount_hi); break;
		case DTU_SK_HOLD_UNDEAD:	line(LT("Holds undead of level %d or less."), f->amount_hi); break;
		case DTU_SK_FEAR:		line(LT("Causes %s. The target may drop its weapon."), b); break;
		case DTU_SK_ACID:
			if (f->amount_hi > 0)
				line(LT("An attack roll; a hit deals %s acid damage, and again in each of the next %d rounds."),
					dice(a, f->dice_n[2], f->dice_sides[2], 0), f->amount_hi);
			else
				line(LT("An attack roll; a hit deals %s acid damage."), dice(a, f->dice_n[2], f->dice_sides[2], 0));
			break;
		case DTU_SK_HOLD:		line(LT("Holds up to %s humanoids."), a); break;
		default:					line(LT("It has no effect in this game.")); break;
	}
}

// 1 if a value of the facts changes with the caster level
static int level_dependent(int spell, dtu_spell_facts *f)
{
	dtu_spell_facts g;

	if (!dtu_spell_get_facts(spell, f->caster_level + 1, &g))
		return 0;
	g.caster_level = f->caster_level;
	return memcmp(&g, f, sizeof(g)) != 0;
}

int dtu_desc_spell_text(int spell, int L, char *out, int size)
{
	dtu_spell_facts f;
	char a[64], b[64];

	Out = out;
	Out_size = size;
	Out_lines = 0;
	out[0] = 0;
	if (!dtu_spell_get_facts(spell, L, &f)) {
		line("?");
		return Out_lines;
	}
	line("%s", dtu_spell_name(spell));
	if (f.book == 1)
		line(LT("Cleric spell, level %d"), f.level);
	else if (f.book == 0)
		line(LT("Mage spell, level %d"), f.level);
	else
		line(LT("Spell, level %d"), f.level);
	line(LT("Cast time: %s"), time_text(a, f.cast_time, f.cast_time));
	line(LT("Range: %s"), f.self ? LT("You") : feet(a, f.range));
	if (f.area)
		line(LT("Area: %s"), (sprintf(b, f.self ? LT("%s around you") : LT("%s where it lands"), feet(a, f.radius)), b));
	else
		line(LT("Area: %s"), f.self ? LT("Only you") : LT("One target"));
	if (f.missiles_hi > 1)
		line(LT("Missiles: %s"), range_text(a, f.missiles_lo, f.missiles_hi));
	if (f.kind == DTU_SK_ARMOR || f.kind == DTU_SK_FIRE_POOL || f.kind == DTU_SK_STONESKIN)
		strcpy(a, LT("Until used up"));
	else if (f.kind == DTU_SK_RARY)
		strcpy(a, Dtu_fix_rary_manual ? LT("Until the next rest") : LT("Until Dispel Magic"));
	else if (f.time_hi < 0 && f.fx == DTU_FX_INVISIBLE)
		strcpy(a, LT("Until you attack"));
	else if (f.time_hi < 0)
		strcpy(a, LT("No end"));
	else if (f.time_hi == 0)
		strcpy(a, LT("Instant"));
	else
		time_text(a, f.time_lo, f.time_hi);
	line(LT("Duration: %s"), a);
	line(LT("Saving throw: %s"), f.save == DTU_SS_HALF ? LT("Half damage") : f.save == DTU_SS_NEGATES ? LT("Negates") :
		f.save == DTU_SS_SOME ? LT("Some targets") : LT("None"));
	if (level_dependent(spell, &f))
		line(LT("Caster level: %d"), f.caster_level);
	spell_effect(spell, &f);
	return Out_lines;
}

//------------------------------------------------------------------------
// Layout and drawing

// The text box of the scroll text screen. In the high resolution the text
// starts a twelfth of the width further right, past the shadow of the left
// roller; the low resolution box needs all its width.
static void text_box(int hi, int *art, int *x, int *y, int *w, int *h)
{
	dtu_scroll_box(hi, art, x, y, w, h);
	if (hi) {
		*x += *w / 12;
		*w -= *w / 12;
	}
}

static grs_bitmap *clean_pic(int hi)
{
	int art, x, y, w, h;

	text_box(hi, &art, &x, &y, &w, &h);
	if (!Clean[hi])
		Clean[hi] = dtu_febgnd_load(art, NULL);
	return Clean[hi];
}

// Lays out Text in the box of the current canvas: the first line (the name)
// centered, the others from the left, words that do not fit go to the
// next line. draw 0: only measures. Returns the height used.
static int layout(int w, int draw, int c_title, int c_text, int c_shadow)
{
	grs_font *font = grd_curcanv->cv_font;
	int lh = font->ft_h + 1, y = 0, first = 1;
	char *p = Text;

	while (*p) {
		char ln[512], row[512];
		int n = 0, x;
		char *q;

		while (*p && *p != '\n' && n < (int) sizeof(ln) - 1)
			ln[n++] = *p++;
		ln[n] = 0;
		if (*p == '\n')
			p++;
		// the words of the line
		row[0] = 0;
		for (q = strtok(ln, " "); ; q = strtok(NULL, " ")) {
			char test[512];
			int tw, th, aw;

			if (q) {
				snprintf(test, sizeof(test), "%s%s%s", row, row[0] ? " " : "", q);
				gr_get_string_size(test, &tw, &th, &aw);
			}
			if (!q || (tw > w && row[0])) {
				if (draw) {
					gr_get_string_size(row, &tw, &th, &aw);
					x = first ? (w - tw) / 2 : 0;
					gr_set_fontcolor(c_shadow, -1);
					gr_string(x + 1, y + 1, row);
					gr_set_fontcolor(first ? c_title : c_text, -1);
					gr_string(x, y, row);
				}
				y += lh;
				if (!q)
					break;
				strcpy(row, q);
			}
			else
				strcpy(row, test);
		}
		if (first)
			y += lh / 2;		// a gap after the name
		first = 0;
	}
	return y;
}

static void draw_text_box(grs_canvas *parent, int x, int y, int w, int h, grs_font *font)
{
	grs_canvas box, *save = grd_curcanv;

	gr_init_sub_canvas(&box, parent, x, y, w, h);
	gr_set_current_canvas(&box);
	gr_set_curfont(font);
	box.cv_drawmode = 0;
	// the colors of the scroll text (0x9CB62): ~c7 yellow for the name,
	// ~c1 white for the text, the shadow of color 0
	layout(w, 1, gr_find_closest_color(63, 63, 0), gr_find_closest_color(63, 63, 63), gr_find_closest_color(0, 0, 0));
	gr_set_current_canvas(save);
}

static void open_common(int where)
{
	Open = where;
	Prev_buttons = mouse_get_btns() & 3;
	Pressed = 0;
	key_flush();
	dtu_char_log("desc: open\n%s", Text);
}

static int open_game(void)
{
	if (Open || !clean_pic(Dtu_hires != 0) || !GAME_FONT)
		return 0;
	// the game time stops, as with the scroll text screen (0x9CE68)
	Time_stopped = !((Game_mode & GM_MULTI) && Function_mode == FMODE_GAME);
	if (Time_stopped)
		stop_time();
	open_common(1);
	return 1;
}

int dtu_desc_open_item(int type, int id, int count, int price, int sell)
{
	if (Open)
		return 0;
	dtu_desc_item_text(type, id, count, price, sell, Text, sizeof(Text));
	return open_game();
}

int dtu_desc_open_entry(int index)
{
	dtu_inv_item *e;

	if (index < 0 || index >= Dtu_n_inventory)
		return 0;
	e = &Dtu_inventory[index];
	return dtu_desc_open_item(e->type, e->id, e->count, -1, 0);
}

int dtu_desc_open_spell(int spell)
{
	if (Open)
		return 0;
	dtu_desc_spell_text(spell, -1, Text, sizeof(Text));
	return open_game();
}

int dtu_desc_open_fe(int type, int id, int count, int price, int sell)
{
	if (Open || !clean_pic(1))
		return 0;
	dtu_desc_item_text(type, id, count, price, sell, Text, sizeof(Text));
	open_common(2);
	return 1;
}

int dtu_desc_is_open(void)
{
	return Open != 0;
}

void dtu_desc_close(void)
{
	if (!Open)
		return;
	if (Open == 1 && Time_stopped)
		start_time();
	Time_stopped = 0;
	Open = 0;
	dtu_char_log("desc: closed");
}

void dtu_desc_frame(void)
{
	int key, b, k;

	if (Open != 1)
		return;
	// one key closes the window; ` and Print Screen write a screenshot, as
	// on the scroll text screen. The key states are cleared, so that the
	// key does not act on the play screen too. A mouse button that went
	// down while the window was open closes it when it goes up: the press
	// went to the window, and the play screen sees only the release, which
	// does nothing there.
	key = key_inkey();
	if (key == KEY_LAPOSTRO || key == KEY_PRINT_SCREEN) {
		save_screen_shot(0);
		key = 0;
	}
	while ((k = key_inkey()) != 0)
		;
	b = mouse_get_btns() & 3;
	if (b & ~Prev_buttons)
		Pressed = 1;
	if (key) {
		key_flush();
		dtu_desc_close();
	}
	else if (Pressed && !b)
		dtu_desc_close();
	Prev_buttons = b;
}

void dtu_desc_draw(void)
{
	grs_canvas *save = grd_curcanv;
	grs_bitmap *pic, *scr;
	int hi = Dtu_hires != 0, art, bx, by, bw, bh, x0, y0;

	if (Open != 1 || !(pic = clean_pic(hi)) || !grd_curscreen)
		return;
	text_box(hi, &art, &bx, &by, &bw, &bh);
	gr_set_current_canvas(&VR_screen_pages[0]);
	scr = &grd_curcanv->cv_bitmap;
	x0 = (scr->bm_w - pic->bm_w) / 2;
	y0 = (scr->bm_h - pic->bm_h) / 2;
	// color 255 is not drawn (0xE1280); the upscaled picture where the
	// pixels are the ones of the picture
	gr_bm_ubitbltm(pic->bm_w, pic->bm_h, x0, y0, 0, 0, pic, scr);
	dtu_hdui_blit_key(pic, 0, 0, pic->bm_w, pic->bm_h, scr, x0, y0, pic->bm_w, pic->bm_h, 255);
	draw_text_box(&VR_screen_pages[0], x0 + bx, y0 + by, bw, bh, GAME_FONT);
	gr_set_current_canvas(save);
}

void dtu_desc_draw_fe(void)
{
	grs_canvas *save = grd_curcanv;
	grs_bitmap *pic;
	int art, bx, by, bw, bh, x0, y0;

	if (Open != 2 || !clean_pic(1) || !grd_curscreen)
		return;
	text_box(1, &art, &bx, &by, &bw, &bh);
	// The front end shows each screen in the palette of its picture: the
	// scroll gets the closest colors of the palette of the screen.
	if (!Fe_pic || memcmp(Fe_pal, gr_palette, 768)) {
		ubyte pal[768], map[256];
		int i;

		if (Fe_pic)
			gr_free_bitmap(Fe_pic);
		Fe_pic = NULL;
		dtu_febgnd_load(art, pal);
		if (!memcmp(pal, gr_palette, 768))
			pic = Clean[1];
		else {
			for (i = 0; i < 255; i++)
				map[i] = gr_find_closest_color(pal[3*i], pal[3*i+1], pal[3*i+2]);
			map[255] = 255;
			Fe_pic = gr_create_bitmap(Clean[1]->bm_w, Clean[1]->bm_h);
			for (i = 0; i < Clean[1]->bm_w * Clean[1]->bm_h; i++)
				Fe_pic->bm_data[i] = map[Clean[1]->bm_data[i]];
		}
		memcpy(Fe_pal, gr_palette, 768);
	}
	pic = Fe_pic ? Fe_pic : Clean[1];
	x0 = (FE_W - pic->bm_w) / 2;
	y0 = (FE_H - pic->bm_h) / 2;
	fe_blit(pic, x0, y0, 255);
	draw_text_box(&grd_curscreen->sc_canvas, x0 + bx, y0 + by, bw, bh, Fe_font_s ? Fe_font_s : GAME_FONT);
	gr_set_current_canvas(save);
}

//------------------------------------------------------------------------
// The test aid

// The height of Text in the box of the scroll with a font, against the box
static int fits(grs_font *font, int hi, int *used)
{
	grs_canvas c, *save = grd_curcanv;
	int art, bx, by, bw, bh;

	*used = 0;
	if (!font)
		return 1;
	text_box(hi, &art, &bx, &by, &bw, &bh);
	gr_init_canvas(&c, NULL, BM_LINEAR, bw, bh);
	gr_set_current_canvas(&c);
	gr_set_curfont(font);
	*used = layout(bw, 0, 0, 0, 0);
	gr_set_current_canvas(save);
	return *used <= bh;
}

// Checks Text: a broken text (empty, no name, a pattern that was not
// filled, a "?") or a text that does not fit the box of the scroll with the
// font (hi: the high resolution box). Writes the result and the text.
static int check(char *what, grs_font *font, int hi)
{
	char *p, flat[TEXT_MAX];
	int bad = 0, used, art, bx, by, bw, bh;

	if (!Text[0] || Text[0] == '\n' || strstr(Text, "%s") || strstr(Text, "%d") || strstr(Text, "(null)") ||
			!strcmp(Text, "?") || !strchr(Text, '\n'))
		bad = 1;
	for (p = Text; *p; p++)
		if (*p == '\n' && (p[1] == '\n' || p[1] == 0))
			bad = 1;
	text_box(hi, &art, &bx, &by, &bw, &bh);
	if (!fits(font, hi, &used))
		bad |= 2;
	printf("desc: %s %s, %d of %d pixels%s\n", what, bad ? "FAIL" : "ok", used, bh,
		(bad & 2) ? " (does not fit)" : (bad & 1) ? " (broken text)" : "");
	dtu_char_log("desc: %s %s\n%s", what, bad ? "FAIL" : "ok", Text);
	snprintf(flat, sizeof(flat), "%s", Text);
	for (p = flat; *p; p++)
		if (*p == '\n')
			*p = '|';
	printf("desc:   %s\n", flat);
	return bad != 0;
}

int dtu_desc_log_all(void)
{
	static const int types[3] = { OBJ_WEAPON, OBJ_ARMOR, OBJ_MISC };
	static const int *counts[3] = { &N_weapon_types, &N_armor_types, &N_misc_types };
	char what[64];
	int t, i, fail = 0, n = 0;
	grs_font *fe_font = Fe_font_s;
	char path[600];

	// a run that starts at a level has not loaded the fonts of the front end
	if (!fe_font && dtu_find_file("FNTEMFS_.FNT", path, sizeof(path)))
		fe_font = gr_init_font(path);

	for (t = 0; t < 3; t++)
		for (i = 0; i < *counts[t]; i++) {
			item_info *it = item_entry(types[t], i);

			if (!it || it->record < 0)
				continue;
			// the inventory (game font, the interface of the run) and a line of
			// the market with a price (the small font of the front end, the
			// high box), with a count: the longest forms
			dtu_desc_item_text(types[t], i, 3, -1, 0, Text, sizeof(Text));
			sprintf(what, "item %d.%d", types[t], i);
			fail += check(what, GAME_FONT, Dtu_hires != 0);
			n++;
			if (fe_font) {
				dtu_desc_item_text(types[t], i, 3, 99999, 1, Text, sizeof(Text));
				sprintf(what, "market %d.%d", types[t], i);
				fail += check(what, fe_font, 1);
				n++;
			}
		}
	for (i = 0; i < N_spell_types; i++) {
		if (Spell_info[i].record < 0)
			continue;
		dtu_desc_spell_text(i, -1, Text, sizeof(Text));
		sprintf(what, "spell %d", i);
		fail += check(what, GAME_FONT, Dtu_hires != 0);
		n++;
	}
	if (!fe_font)
		printf("desc: the market texts were not checked: no font of the front end\n");
	printf("desc: %d texts, %d failed\n", n, fail);
	dtu_char_log("desc: %d texts, %d failed", n, fail);
	Text[0] = 0;
	return fail;
}
