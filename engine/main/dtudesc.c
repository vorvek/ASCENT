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
#include <stdint.h>

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
#include "dtuatk.h"			// the attack timing rules
#include "dtucombat.h"		// Dtu_fix_haste_attack, Dtu_fix_missile_rate
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

// A number with one decimal place, with the decimal mark of the language
static char *decimal(char *buf, int tenths)
{
	if (tenths % 10)
		sprintf(buf, LT("%d.%d"), tenths / 10, tenths % 10);
	else
		sprintf(buf, "%d", tenths / 10);
	return buf;
}

// A time of the attack rules in seconds, to a tenth
static char *seconds(char *buf, fix t)
{
	return decimal(buf, (int) (((int64_t) t * 10 + F1_0 / 2) / F1_0));
}

// A distance in units of the game: 2 units are 1 foot (tools/wiki_rules.py)
static char *feet(char *buf, int units)
{
	char n[32];

	sprintf(buf, LT("%s ft"), decimal(n, units * 5));
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
			case DTU_IP_DEXTERITY:		line(LT("Raises your dexterity by %d."), p[i].value); break;
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
			case DTU_IP_INFRA_HUMAN:	break;	// the effect list names Infravision: each other race has it, so no extra line
			case DTU_IP_FAST_ATTACK:	line(LT("Attacks twice as fast. Does not add to Haste.")); break;
		}
}

static void shield_lines(int ac, int ac_missile);

static void fx_line(int type, int id, int hand)
{
	char buf[256];
	int fx = dtu_char_item_fx(type, id);

	if (fx)
		line(hand ? LT("In the hand: %s.") : LT("While worn: %s."), fx_names(buf, sizeof(buf), fx));
	// the armor class of the effects Armor, Magical Vestment and Shield
	if (fx & DTU_FX_SHIELD)
		shield_lines(dtu_spell_fx_ac(fx), dtu_spell_fx_ac_vs(fx, 1));
	else if (dtu_spell_fx_ac(fx) < 10)
		line(LT("Protects like worn armor of armor class %d."), dtu_spell_fx_ac(fx));
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
		case DTU_MU_DRAGON:
			line(LT("Strength becomes at least %d for %s."), u->lo, time_text(t, 60, 60 * dtu_char_dragon_minutes()));
			line(LT("A fighter gets a random percent value with it."));
			break;
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
		case DTU_MU_TURN:		line(LT("Turns undead as a cleric of level %d."), dtu_char_spider_turn_level() + 1); break;
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

// 1 if the count of the entry is a number of items or of uses. The count
// of a scroll is the number of its text (0x9DC42), and the Gem of Curing,
// the Cursed Scroll and the Orb of Insight are not used up (use_misc).
static int counted(const dtu_misc_use *u)
{
	return u->kind != DTU_MU_SCROLL && u->kind != DTU_MU_GEM && u->kind != DTU_MU_CURSED && u->kind != DTU_MU_QUEST;
}

static void class_line(int mask)
{
	char buf[128];

	line(LT("Classes: %s"), classes(buf, mask));
}

static void weapon_text(int id, dtu_item_data *d, int count, int r)
{
	char buf[256], b2[64] = "";
	int pc_spell = tbl_int(r, "pc_spell", -1), range, fx, fh;
	fix wait;

	if (pc_spell >= 0 && !d->melee) {
		// a wand: the usable item layout (0x56A0D casts pc_spell, one charge)
		if (count >= 0)
			line(LT("Charges: %d"), count);
		if (d->class_mask & 0xff)
			class_line(d->class_mask);
		line(LT("Each use casts %s."), dtu_spell_name(pc_spell));
		return;
	}
	// weapon_attack and dtu_char_weapon_damage (dtuchar.c): the dice and
	// the bonus of the table line; a weapon with 0 sides rolls no dice
	line(LT("Damage: %s"), dice(buf, d->dice_n, d->dice_sides, d->damage_bonus));
	// dtu_weapon_extra_damage (dtuobj.c, 0x2B327)
	if (tbl_int(r, "fire_damage_dice", 0) > 0 && tbl_int(r, "#fire_damage_dice", 0) > 0)
		line(LT("Fire damage: %s"), dice(buf, tbl_int(r, "#fire_damage_dice", 0), tbl_int(r, "fire_damage_dice", 0), 0));
	sprintf(buf, "%+d", d->to_hit_bonus);
	line(LT("To hit: %s"), buf);
	// the Haste or the Slow that the weapon itself gives in the hand
	fx = dtu_char_item_fx(OBJ_WEAPON, id) & (DTU_ATK_HASTE | DTU_ATK_SLOW);
	// ASCENT: with the speed of the weapon (Dtu_fix_speed_items, dtuatk.c)
	fh = dtu_atk_rate_fix_haste(fx, dtu_char_item_fast(OBJ_WEAPON, id), Dtu_fix_haste_attack);
	fx = dtu_atk_rate_effects(fx, dtu_char_item_fast(OBJ_WEAPON, id));
	if (d->melee) {
		// dtu_combat_swing_time and the gate of the next attack
		// (dtucombat.c, docs/re/combat.md A8)
		fix t = dtu_atk_state_time(1, Weapon_info[id].fire_wait, F1_0, fx, fh);

		line(LT("Melee weapon, %s s for each attack."), seconds(buf, dtu_atk_melee_wait(t, fx, fh)));
	}
	else {
		// the life of the missile at its speed (missiles_frame, dtuobj.c);
		// a missile with no life of its own (a spell missile) has no range
		// of the weapon line
		range = f2i(dtu_missile_weapon_range(id));
		if (range > 0)
			feet(b2, range);
		if (tbl_int(r, "thrown", 0) == 1)
			line(range > 0 ? LT("Thrown weapon, range %s.") : LT("Thrown weapon."), b2);
		else if (bm_tbl_has_flag(r, "arrow"))
			line(range > 0 ? LT("Arrow for a bow, range %s.") : LT("Arrow for a bow."), b2);
		else if (bm_tbl_has_flag(r, "bolt"))
			line(range > 0 ? LT("Bolt for a crossbow, range %s.") : LT("Bolt for a crossbow."), b2);
		else
			line(range > 0 ? LT("Missile, range %s.") : LT("Missile."), b2);
		// missile_wait (dtucombat.c): 0 with the rule of the executable,
		// where the time of the arm model sets the rate
		wait = dtu_atk_missile_wait(Weapon_info[id].fire_wait, fx, Dtu_fix_missile_rate, fh);
		if (wait > 0)
			line(LT("%s s for each shot."), seconds(buf, wait));
		if (count >= 0 && d->stackable)
			line(LT("Count: %d"), count);
	}
	line(bm_tbl_has_flag(r, "magic") ? LT("Magic.") : LT("Not magic."));
	// a line with no class is a weapon of the creatures
	if (d->class_mask & 0xff)
		class_line(d->class_mask);
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
		class_line(dtu_spell_use_classes(id));		// dtu_spell_usable
		line(LT("Each use casts %s."), dtu_spell_name(id));
	}
	else if (slot == DTU_SLOT_BOW || slot == DTU_SLOT_CROSSBOW) {
		// 0x2AB36, 0x2B0EE: the to_hit_bonus of the launcher goes to the
		// attack and the damage of the arrows or the bolts
		sprintf(buf, "%+d", d.to_hit_bonus);
		// launcher_bonus (dtuchar.c, 0x2AB0A): the bow for a weapon with the
		// flag arrow, the crossbow for each other missile weapon. No code
		// reads the flag magic of a misc line.
		line(slot == DTU_SLOT_BOW ? LT("Arrows get %s to hit and damage.") :
			LT("Bolts and thrown weapons get %s to hit and damage."), buf);
		line(LT("Slot: %s"), slot_name(slot));
		class_line(d.class_mask);
		fx_line(type, id, 0);
		power_lines(type, id);		// ASCENT: the Short Bow of Speed
	}
	else if (slot >= 0) {
		int ac = dtu_char_item_ac(type, id);

		if (slot == DTU_SLOT_ARMOR && ac)
			line(LT("Armor class: %d"), 10 + ac);
		else if (ac)
			line(LT("Armor class: %d better"), -ac);
		line(LT("Slot: %s"), slot_name(slot));
		class_line(d.class_mask);
		fx_line(type, id, 0);
		power_lines(type, id);
	}
	else if (u) {
		if (count >= 0 && counted(u))
			line(charged(u) ? LT("Charges: %d") : LT("Count: %d"), count);
		class_line(d.class_mask);
		use_line(u);
	}
	else if (type == OBJ_MISC && DTU_IS_KEY_ITEM(type, id))
		line(LT("Key."));
	else if (tbl_int(r, "plot_item", 0))
		line(LT("Quest item. It cannot be dropped or sold."));		// 0xB611C, the market lists 0x870A0
	else
		line(LT("Item."));
	// the market takes the price for each item of a stack
	if (price >= 0 && d.stackable && count != 1)
		line(sell ? LT("Sale price: %d gold each") : LT("Price: %d gold each"), price);
	else if (price >= 0)
		line(sell ? LT("Sale price: %d gold") : LT("Price: %d gold"), price);
	return Out_lines;
}

// The line of the highest damage of a spell whose dice grow with the
// caster level up to a limit: the facts of two high levels are the same
// and are not the ones of the caster level.
static void damage_cap(int spell, dtu_spell_facts *f)
{
	dtu_spell_facts a, b;
	char buf[64];
	int g;

	if (!dtu_spell_get_facts(spell, 30, &a) || !dtu_spell_get_facts(spell, 40, &b))
		return;
	for (g = 0; g < 4; g++)
		if (a.dice_n[g] == b.dice_n[g] && a.dice_add[g] == b.dice_add[g] &&
				(a.dice_n[g] != f->dice_n[g] || a.dice_add[g] != f->dice_add[g]))
			line(LT("With more caster levels: up to %s."), dice(buf, a.dice_n[g], a.dice_sides[g], a.dice_add[g]));
	if (a.missiles_hi == b.missiles_hi && a.missiles_hi > f->missiles_hi)
		line(LT("With more caster levels: up to %d missiles."), a.missiles_hi);
}

// The armor class spells (Armor, Shield, Magical Vestment) give the armor
// class of a worn armor: the value replaces the value of the worn items when
// that is worse (dtu_spell_player_ac stage 0), and the other adjustments of
// the character apply as with any armor.
static void ac_lines(int ac)
{
	line(LT("Protects like worn armor of armor class %d."), ac);
	line(LT("No effect if your armor is better."));
}

// ASCENT: Shield and the Amulet of Shield. Dtu_fix_shield_ac: 4 against
// melee weapons and 2 against missiles; -origcombat: the one value of 1.3.
static void shield_lines(int ac, int ac_missile)
{
	if (ac != ac_missile)
		line(LT("Armor class %d against melee weapons and %d against missiles."), ac, ac_missile);
	else
		line(LT("Protects like worn armor of armor class %d."), ac);
	line(LT("No effect if your armor is better."));
	line(LT("Stops Magic Missile."));		// case 33
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
			damage_cap(spell, f);
			if (f->undead_die) {
				sprintf(a, "1d%d", f->undead_die);
				line(LT("%s more damage against undead."), a);
			}
			// ASCENT: dtu_fireball_blind_deaf: no save against the blindness
			if (f->blind_dist)
				line(LT("Blinds each creature within %s of the point for up to %s, and deafens it unless it saves."),
					feet(a, f->blind_dist), time_text(b, f->blind_seconds, f->blind_seconds));
			break;
		case DTU_SK_HEAL:		line(LT("Heals %s hit points."), a); break;
		case DTU_SK_GIVE:
			line(LT("Gives %s."), b);
			// 0x29E3D (dtucombat.c): the effect acts in the melee hit of the player
			if (f->fx == DTU_FX_CHILL_TOUCH)
				line(LT("Each melee hit: the target saves or takes 1d4 more damage; undead get Fear."));
			break;
		case DTU_SK_CAUSE:		line(LT("Causes %s."), b); break;
		case DTU_SK_AC:
			if (f->fx == DTU_FX_SHIELD)
				shield_lines(f->ac, f->ac_missile);
			else
				ac_lines(f->ac);
			break;
		case DTU_SK_ARMOR:
			ac_lines(f->ac);
			line(LT("Ends after %s points of weapon damage."), a);
			break;
		case DTU_SK_AID:		line(LT("Adds %s hit points above the maximum, and Bless."), a); break;
		// case 13 has no test of the caster: the area takes the player too
		case DTU_SK_DISPEL:		line(LT("Ends all spell effects in the area, yours too.")); break;
		// case 0: a d20 roll by the levels, then the creature leaves or dies
		case DTU_SK_ABJURE:		line(LT("An extraplanar creature may leave the level or die.")); break;
		case DTU_SK_RARY:		line(LT("Gives more spell slots: three of level 1, one of level 1 and one of level 2, or one of level 3.")); break;
		case DTU_SK_KNOCK:		line(LT("Opens a locked door.")); break;
		case DTU_SK_CURE_SENSES:	line(LT("Cures blindness and deafness.")); break;
		case DTU_SK_FIRE_POOL:	line(LT("Takes the first %s points of fire damage."), a); break;
		case DTU_SK_STONESKIN:	line(LT("Stops the next %s attacks."), a); break;
		case DTU_SK_STRENGTH:	line(LT("Adds %s to strength."), a); break;
		// light_object (dtuspell.c): the light stays where the missile lands
		case DTU_SK_BLIND:		line(LT("Makes a light where it lands. Blinds a creature that it hits.")); break;
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
	line(LT("Range: %s"), f.self ? LT("Self") : feet(a, f.range));
	if (f.area)
		line(LT("Area: %s"), (sprintf(b, f.self ? LT("%s around you") : LT("%s where it lands"), feet(a, f.radius)), b));
	// ASCENT: the object loop of 0x90CEF has no test of the caster (0x90D54
	// to 0x90DDA), and the Fireball case (0x8E55C) has none either: a burst
	// near the caster burns the caster too. The cases of Burning Hands,
	// Produce Fire and Melf's Minute Meteors skip the caster.
	if (f.area && spell == 16)
		line(LT("Also hits you when you are in the area."));
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

// The pictures stay until the exit of the program
static void free_pics(void)
{
	int i;

	for (i = 0; i < 2; i++)
		if (Clean[i]) {
			gr_free_bitmap(Clean[i]);
			Clean[i] = NULL;
		}
	if (Fe_pic)
		gr_free_bitmap(Fe_pic);
	Fe_pic = NULL;
}

static grs_bitmap *clean_pic(int hi)
{
	static int at_exit = 0;
	int art, x, y, w, h;

	text_box(hi, &art, &x, &y, &w, &h);
	if (!Clean[hi]) {
		Clean[hi] = dtu_febgnd_load(art, NULL);
		if (!at_exit)
			atexit(free_pics);
		at_exit = 1;
	}
	return Clean[hi];
}

// Lays out Text in the box of the current canvas: the first line (the name)
// centered, the others from the left, words that do not fit go to the
// next line. tight 0: one pixel between the lines and a gap after the name;
// 1: no pixel between the lines; 2: no gap too. draw 0: only measures.
// Returns the height used; *wide gets the width of the widest row.
static int layout(int w, int tight, int draw, int c_title, int c_text, int c_shadow, int *wide)
{
	grs_font *font = grd_curcanv->cv_font;
	int lh = font->ft_h + (tight ? 0 : 1), y = 0, first = 1;
	char *p = Text;

	*wide = 0;

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
				gr_get_string_size(row, &tw, &th, &aw);
				if (tw > *wide)
					*wide = tw;
				if (draw) {
					x = first ? (w - tw) / 2 : 0;
					if (x < 0)
						x = 0;
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
		if (first && tight < 2)
			y += lh / 2;		// a gap after the name
		first = 0;
	}
	return y;
}

// The loosest line spacing with which Text fits the height h, the tightest
// one if none fits.
static int spacing(int w, int h, int *used, int *wide)
{
	int tight;

	for (tight = 0; tight < 3; tight++)
		if ((*used = layout(w, tight, 0, 0, 0, 0, wide)) <= h)
			return tight;
	return 2;
}

static void draw_text_box(grs_canvas *parent, int x, int y, int w, int h, grs_font *font)
{
	grs_canvas box, *save = grd_curcanv;
	int used, wide, tight, top;

	// The place of the text in the box: a short text is in the middle of
	// the height; a long one starts a tenth of the height below the top, or
	// higher if it needs the room.
	gr_init_sub_canvas(&box, parent, x, y, w, h);
	gr_set_current_canvas(&box);
	gr_set_curfont(font);
	tight = spacing(w, h, &used, &wide);
	top = (h - used) / 2;
	if (top < h / 10)
		top = h - used < h / 10 ? h - used : h / 10;
	if (top < 0)
		top = 0;
	gr_init_sub_canvas(&box, parent, x, y + top, w, h - top);
	gr_set_current_canvas(&box);
	gr_set_curfont(font);
	box.cv_drawmode = 0;
	// the colors of the scroll text (0x9CB62): ~c7 yellow for the name,
	// ~c1 white for the text, the shadow of color 0
	layout(w, tight, 1, gr_find_closest_color(63, 63, 0), gr_find_closest_color(63, 63, 63),
		gr_find_closest_color(0, 0, 0), &wide);
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

// A new level or a loaded game: no window (as dtu_scroll_reset)
void dtu_desc_reset(void)
{
	dtu_desc_close();
	Pressed = 0;
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

// The height and the width of Text in the box of the scroll with a font,
// against the box. A row is wider than the box if one word is.
static int fits(grs_font *font, int hi, int *used)
{
	grs_canvas c, *save = grd_curcanv;
	int art, bx, by, bw, bh, wide = 0;

	*used = 0;
	if (!font)
		return 1;
	text_box(hi, &art, &bx, &by, &bw, &bh);
	gr_init_canvas(&c, NULL, BM_LINEAR, bw, bh);
	gr_set_current_canvas(&c);
	gr_set_curfont(font);
	spacing(bw, bh, used, &wide);
	gr_set_current_canvas(save);
	return *used <= bh && wide <= bw;
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

int dtu_desc_log_all(int L)
{
	static const int types[3] = { OBJ_WEAPON, OBJ_ARMOR, OBJ_MISC };
	static const int *counts[3] = { &N_weapon_types, &N_armor_types, &N_misc_types };
	char what[64];
	static const int counts_tried[4] = { -1, 0, 1, 30000 };
	int t, i, c, fail = 0, n = 0, extra = 0;
	grs_font *fe_font = Fe_font_s, *own_font = NULL;
	char path[600];

	if (L < 1)
		L = -1;

	// a run that starts at a level has not loaded the fonts of the front end
	if (!fe_font && dtu_find_file("FNTEMFS_.FNT", path, sizeof(path)))
		fe_font = own_font = gr_init_font(path);

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
		dtu_desc_spell_text(i, L, Text, sizeof(Text));
		sprintf(what, "spell %d", i);
		fail += check(what, GAME_FONT, Dtu_hires != 0);
		n++;
	}
	// Each number that a caller can pass, also the ones with no table line
	// and the numbers outside the tables, with each kind of count, in a
	// small buffer too: the text must end and nothing must be read outside
	// a table. A spell as an item is a scroll.
	for (t = 0; t < 5; t++) {
		int type = t < 3 ? types[t] : t == 3 ? OBJ_SPELL : 99;

		for (i = -2; i < 260; i++)
			for (c = 0; c < 4; c++) {
				char small[24];

				dtu_desc_item_text(type, i, counts_tried[c], c == 3 ? 2000000000 : -1, c & 1, Text, sizeof(Text));
				dtu_desc_item_text(type, i, counts_tried[c], 0, c & 1, small, sizeof(small));
				if (!Text[0] || strlen(Text) >= sizeof(Text) || strlen(small) >= sizeof(small))
					fail++;
				extra++;
			}
	}
	for (i = -2; i < 260; i++)
		for (c = 0; c < 41; c += 8) {
			char small[24];

			dtu_desc_spell_text(i, c, Text, sizeof(Text));
			dtu_desc_spell_text(i, c, small, sizeof(small));
			if (!Text[0] || strlen(Text) >= sizeof(Text) || strlen(small) >= sizeof(small))
				fail++;
			extra++;
		}
	printf("desc: %d texts of numbers of all kinds\n", extra);
	if (!fe_font)
		printf("desc: the market texts were not checked: no font of the front end\n");
	printf("desc: %d texts, %d failed\n", n, fail);
	dtu_char_log("desc: %d texts, %d failed", n, fail);
	Text[0] = 0;
	if (own_font)
		gr_close_font(own_font);
	return fail;
}
