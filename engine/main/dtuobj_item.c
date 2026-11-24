/*
 * ASCENT: new file, split from dtuobj.c. The items: the inventory of the
 * player, a new item on the floor, the pickup of an item and the tint of
 * Detect Magic. The shared declarations are in dtuobj_int.h.
 */

#include "dtulang.h"		// ASCENT: the language of the text
#include "dtuitemfix.h"	// ASCENT: the corrected item names
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <math.h>

#include "inferno.h"
#include "types.h"
#include "fix.h"
#include "vecmat.h"
#include "gr.h"
#include "3d.h"
#include "mono.h"
#include "error.h"
#include "args.h"
#include "game.h"
#include "segment.h"
#include "gameseg.h"
#include "object.h"
#include "physics.h"
#include "fvi.h"
#include "collide.h"
#include "robot.h"
#include "polyobj.h"
#include "bm.h"
#include "piggy.h"
#include "vclip.h"
#include "weapon.h"
#include "player.h"
#include "gauges.h"
#include "lighting.h"
#include "wall.h"
#include "gamesave.h"
#include "gameseq.h"
#include "dtufile.h"
#include "dtugame.h"
#include "dtuobj.h"
#include "dtutrig.h"
#include "dtuspell.h"	// ASCENT: the spells of the creatures, the area of a missile
#include "dtuthief.h"	// ASCENT: Hide In Shadows and Move Silently in the AI
#include "dtuscript.h"	// ASCENT: quest flag 67 in the creature setup
#include "textures.h"	// ASCENT: the lava textures of BANE
#include "tmapfx.h"	// ASCENT: the tint of Detect Magic
#include "dtuchar.h"
#include "digi.h"
#include "sounds.h"
#include "dtufoot.h"		// ASCENT: the footsteps of the creatures
#include "fireball.h"
#include "palette.h"
#include "dtumusic.h"
#include "key.h"		// ASCENT: the key that ends the wait of the death
#include "dtuchar.h"
#include "dtuopts.h"	// ASCENT: Dtu_improved_pathfinding
#include "dtucombat.h"	// ASCENT: the frame of the player for its body
#include "dtumovie.h"
#include "text.h"		// ASCENT: Text_string, the names of the creatures
#include "mem.h"		// ASCENT: dtu_load_file() allocates with mem_malloc, so free() must be mem_free()
#include "dtulog.h"		// ASCENT: the session log
#include "dtuxp.h"		// ASCENT: the kill line, the death lines of the session log
#include "dtumiss.h"		// ASCENT: the miss sound of a creature
#include "dtuobj_int.h"

//------------------------------------------------------------------------
// Inventory

void dtu_inventory_clear(void)
{
	memset(Dtu_inventory,0,sizeof(Dtu_inventory));
	Dtu_n_inventory = 0;
	Dtu_gold = 0;
}

item_info *item_table_entry(int type,int id)
{
	switch (type) {
		case OBJ_MISC:		return (id < N_misc_types) ? &Misc_info[id] : NULL;
		case OBJ_ARMOR:	return (id < N_armor_types) ? &Armor_info[id] : NULL;
		case OBJ_SPELL:	return (id < N_spell_types) ? &Spell_info[id] : NULL;
		case OBJ_WEAPON:	return (id < N_weapon_types) ? &Weapon_item_info[id] : NULL;
	}
	return NULL;
}

// 0xB61B2: the type has the word "stackable" in its table line.
static int item_is_stackable(item_info *it)
{
	return it && it->record >= 0 && bm_tbl_has_flag(it->record,"stackable");
}

int dtu_inventory_add(int type, int id, int count)
{
	item_info *it = item_table_entry(type,id);
	int i;

	Dtu_inventory_left = 0;
	if (it == NULL || count <= 0)
		return -1;

	// 0xB660C: a spell of a class of the character goes to its spell list
	// (0xB69AD); a spell that the list has already goes to the item list
	if (type == OBJ_SPELL) {
		int r = dtu_spell_insert(id);

		if (r == 0) {
			dtu_obj_log("inventory: spell %d '%s' goes to the spell list",id,it->name);
			return DTU_INV_SPELL_LIST;
		}
		if (r == 1)
			return -1;
	}

	if (item_is_stackable(it))
		for (i=0;i<Dtu_n_inventory;i++)
			if (Dtu_inventory[i].type == type && Dtu_inventory[i].id == id) {
				int limit = it->stack_limit > 0 ? it->stack_limit : 10;

				// ASCENT: 0xB67D3: a full stack or an endless one (count -1)
				// takes nothing; else the count is added and cut to the limit
				// (the rest is lost). Dtu_fix_partial_stack: the rest stays.
				if (Dtu_inventory[i].count >= limit || Dtu_inventory[i].count < 0)
					return -1;
				if (Dtu_inventory[i].count + count > limit) {
					if (Dtu_fix_partial_stack)
						Dtu_inventory_left = Dtu_inventory[i].count + count - limit;
					dtu_obj_log("inventory: stack of %d '%s' full at %d, %d left%s",id,it->name,limit,
						Dtu_inventory[i].count + count - limit,Dtu_fix_partial_stack ? "" : " (lost, the rule of 0xB65BB)");
					Dtu_inventory[i].count = limit;
				}
				else
					Dtu_inventory[i].count += count;
				return i;
			}

	return dtu_inventory_append(type,id,count);
}

// The limits of the two lists of the executable (0xB65BB: 127 items, 22
// key items).
static int inventory_full(int type, int id)
{
	int i, n = 0, key = DTU_IS_KEY_ITEM(type,id);

	if (Dtu_n_inventory >= DTU_MAX_INVENTORY)
		return 1;
	for (i=0;i<Dtu_n_inventory;i++)
		if (DTU_IS_KEY_ITEM(Dtu_inventory[i].type,Dtu_inventory[i].id) == key)
			n++;
	return n >= (key ? DTU_MAX_KEY_ENTRIES : DTU_MAX_ITEM_ENTRIES);
}

int dtu_inventory_append(int type, int id, int count)
{
	item_info *it = item_table_entry(type,id);
	dtu_inv_item *inv;

	if (it == NULL || inventory_full(type,id))
		return -1;

	inv = &Dtu_inventory[Dtu_n_inventory];
	memset(inv,0,sizeof(*inv));
	inv->type = type;
	inv->id = id;
	inv->count = count;
	inv->icon = it->icon;
	inv->inv_icon = it->inv_icon;
	inv->obj_bitmap = it->obj_bitmap;
	inv->level = Current_level_num;
	// ASCENT: the name that the player sees (the name of an entry is only
	// shown; the rules read the name of the table line)
	strncpy(inv->name,dtu_item_shown_name(type,id,it->name),sizeof(inv->name)-1);
	return Dtu_n_inventory++;
}

void dtu_inventory_remove(int index, int count)
{
	if (index < 0 || index >= Dtu_n_inventory)
		return;
	Dtu_inventory[index].count -= count;
	if (Dtu_inventory[index].count <= 0) {
		memmove(&Dtu_inventory[index],&Dtu_inventory[index+1],(Dtu_n_inventory-index-1)*sizeof(dtu_inv_item));
		Dtu_n_inventory--;
		memset(&Dtu_inventory[Dtu_n_inventory],0,sizeof(dtu_inv_item));
	}
}

// A new item object (object_create_egg 0x3C4BD for the types 5, 15, 16 and
// 17): no control, no movement, render type 8, the byte 0x1D5 is the count.
// The executable puts it at the position of the creature plus half of a
// size in y. The port puts it on the floor under the point. (guess for y)
// A size above 0 is the size of the object; 0 takes the size of the table.
// The size of the table (0x3C55C, 0x3C663, 0x3C700): `size=` of the line
// as a whole number (atoi, shifted to a fix), else the default of the
// reader: 5 for a weapon (0x207E7, +0x58), an armor (0x22BB1) and a misc
// item (0x23780), 3 for a spell (0x242F6). The port had 2 for all, and the
// move query of the player (size + its radius, 0x3ED76) did not reach a
// dropped item on the floor (the armor of a kobold).
int dtu_create_item_size(int type, int id, int count, int segnum, vms_vector *pos, fix size_wanted)
{
	item_info *it = item_table_entry(type,id);
	int objnum;
	object *obj;
	fix size = type == OBJ_SPELL ? i2f(3) : i2f(5);
	char value[32];

	if (it == NULL || segnum < 0 || segnum > Highest_segment_index)
		return -1;
	if (size_wanted > 0)
		size = size_wanted;
	else if (it->record >= 0 && bm_tbl_get(it->record,"size",value,sizeof(value)))
		size = i2f(atoi(value));

	objnum = obj_create(type,id,segnum,pos,&vmd_identity_matrix,size,CT_NONE,MT_NONE,RT_ITEM);
	if (objnum < 0)
		return -1;
	obj = &Objects[objnum];
	obj->dtu.item_bitmap = it->obj_bitmap;
	if (obj->dtu.item_bitmap < 0)
		obj->render_type = RT_NONE;
	obj->dtu.m_1d5 = count;
	obj->shields = F1_0;

	{
		fix d = creature_floor_dist(obj,0,size);

		if (d != DTU_FLOOR_PROBE && d > 0) {
			vms_vector p = obj->pos;

			p.y -= d;
			if (find_point_seg(&p,obj->segnum) == obj->segnum)
				obj->pos = p;
		}
	}
	return objnum;
}

int dtu_create_item(int type, int id, int count, int segnum, vms_vector *pos)
{
	return dtu_create_item_size(type,id,count,segnum,pos,0);
}

//------------------------------------------------------------------------
// Collisions

// Bit 0x10 of the flag word that the pickup passes to 0xB772D. Misc: the
// flag that the table fixup 0x1A344 sets for a plot item, a name with '+'
// and nine name parts. Armor: "stackable". Weapon: "magic", also set by
// 0x1A344 for a plot item or a name with '+'. Spell: "cast_with_arrow",
// the last cast_with_ word of the line wins (0x253A8 to 0x2545D).
static int pickup_flag_10(int type,item_info *it)
{
	static char *misc_names[] = {"uring","yrkul","rystal","carab","speed",
		"nsight","anegem","ursed","lamesword","otion"};
	static char *cast_with[] = {"cast_with_hands","cast_with_wand",
		"cast_with_scroll","cast_with_arrow"};
	int i,best,pos,arrow;

	switch (type) {
		case OBJ_MISC:
			if (tbl_int(it->record,"plot_item",0) != 0 || strchr(it->name,'+'))
				return 1;
			for (i=0;i<(int)(sizeof(misc_names)/sizeof(misc_names[0]));i++)
				if (strstr(it->name,misc_names[i]))
					return 1;
			return 0;
		case OBJ_ARMOR:
			return bm_tbl_has_flag(it->record,"stackable");
		case OBJ_WEAPON:
			return bm_tbl_has_flag(it->record,"magic") || tbl_int(it->record,"plot_item",0) != 0
				|| strchr(it->name,'+') != NULL;
		case OBJ_SPELL:
			best = -1;
			arrow = 0;
			for (i=0;i<4;i++) {
				pos = bm_tbl_flag_pos(it->record,cast_with[i]);
				if (pos > best) {
					best = pos;
					arrow = (i == 3);
				}
			}
			return arrow;
	}
	return 0;
}

// 0xB772D, called by each pickup: sound 2216 if bit 0x10 of the flag word
// is set, then sound 2215, both with the volume 0.5 (0x2D955).
static void pickup_sound(int flag_10)
{
	if (flag_10)
		digi_play_sample(0x8A8,F1_0/2);
	digi_play_sample(0x8A7,F1_0/2);
}

// The weapon types that a spell missile has used: 0x56FF6 writes the flag
// word W+0x3E of the template (2 hands, 3 wand, 4 scroll, 5 arrow) to 0x10
// at each cast (0x571B2), so from then on the type is magic.
static unsigned Spell_template_magic;

void dtu_spell_template_used(int weapon_type)
{
	if (weapon_type >= 0 && weapon_type < 32)
		Spell_template_magic |= 1u << weapon_type;
}

// 0x670D4: the magic look of an object for the view of Viewer. 0 when the
// viewer has no Detect Magic (+0x88 bit 0x40). Else 1 for: a spell item
// (type 17); armor with "magic" (0x19FD34 bit 0, 0x23392); a misc item
// with flag 0x10 of 0x1A344 (a plot item, a name with '+' or one of the
// nine name parts, as the pickup sound); scenery with "magic" (0x19B340
// bit 0, 0x229EE); a weapon object whose type has W+0x3E bit 0x10 ("magic",
// a plot item, a name with '+', or a spell missile template). 2 for a
// creature whose weapon type is below 0 or magic: draw_polygon_model then
// tints the submodels with flag 0x20, which no model file has. Else 0.
int dtu_magic_look(object *obj)
{
	item_info *it;

	if (obj == NULL || Viewer == NULL || !(Viewer->dtu.m_88 & DTU_FX_DETECT_MAGIC))
		return 0;
	switch (obj->type) {
		case OBJ_SPELL:
			return 1;
		case OBJ_ARMOR:
			it = item_table_entry(obj->type,obj->id);
			return it && it->record >= 0 && bm_tbl_has_flag(it->record,"magic");
		case OBJ_MISC:
			it = item_table_entry(obj->type,obj->id);
			return it && pickup_flag_10(obj->type,it);
		case OBJ_DRESSING:
			return obj->id < N_dressing_types && (Dressing_info[obj->id].flags & DRESSING_F_MAGIC) != 0;
		case OBJ_WEAPON:
			if (obj->id < 32 && (Spell_template_magic & (1u << obj->id)))
				return 1;
			it = item_table_entry(obj->type,obj->id);
			return it && pickup_flag_10(obj->type,it);
		case OBJ_ROBOT:
			if (obj->id < N_robot_types) {
				int w = Robot_info[obj->id].weapon_type;
				if (w < 0)
					return 2;
				it = item_table_entry(OBJ_WEAPON,w);
				return (it && pickup_flag_10(OBJ_WEAPON,it)) ? 2 : 0;
			}
			return 0;
	}
	return 0;
}

// The tint of the current object (0x14C4CD) for both renderers.
int Dtu_fix_magic_tint = 1;

void dtu_set_tint(int tint)
{
	Tmap_tint = tint;
	G3_gpu_tint = tint;
	Tmap_tint_fix = Dtu_fix_magic_tint;
}

int dtu_get_tint(void)
{
	return Tmap_tint;
}

// draw_object_blob (0x671D6) and draw_object_tmap_rod (0x67273) set the
// tint for a magic object and do not clear it: the value stays for the
// next polygons (the walls of the nearer segments, other objects) until a
// polygon model clears it (0x67A0A). Dtu_fix_magic_tint: the tint is only
// for the object. Returns the value to give back to dtu_object_tint_end.
int dtu_object_tint_begin(object *obj)
{
	int old = Tmap_tint;

	if (dtu_magic_look(obj) != 0)
		dtu_set_tint(0x40);
	return old;
}

void dtu_object_tint_end(int old)
{
	if (Dtu_fix_magic_tint)
		dtu_set_tint(old);
}

int dtu_weapon_is_item(object *obj)
{
	return obj->type == OBJ_WEAPON && obj->render_type == RT_ITEM && obj->lifeleft == IMMORTAL_TIME;
}

// The four pickup functions of collide.c (0x2805B misc, 0x283DC armor,
// 0x28273 spell, 0x284E0 weapon).
void pick_up_item(object *player,object *item)
{
	item_info *it = item_table_entry(item->type,item->id);
	int count = (signed char) item->dtu.m_1d5;
	int index;

	if (player->id != Player_num || (item->flags & OF_SHOULD_BE_DEAD) || it == NULL)
		return;

	// Gold is misc id 1 (0x2806B). The amount is the hit points. The gold
	// path comes before the pickup delay, and it passes the flag word 0 to
	// the sound function 0xB772D (0x28089).
	if (item->type == OBJ_MISC && item->id == 1) {
		int amount = f2i(item->shields);

		Dtu_gold += amount;
		HUD_init_message("You pick up %d gold.",amount);
		pickup_sound(0);
		item->flags |= OF_SHOULD_BE_DEAD;
		dtu_obj_log("pickup: obj %d, %d gold, sound 2215",(int)(item-Objects),amount);
		return;
	}

	// 0x31A0E: no pick up for 5 seconds after a drop
	if (!dtu_char_pickup_allowed())
		return;

	// A weapon is an item only if it does not move (0x28502).
	if (item->type == OBJ_WEAPON && !dtu_weapon_is_item(item))
		return;

	if (count == 0) {
		if (item_is_stackable(it) || item->type == OBJ_ARMOR)
			count = 1;
		else
			count = tbl_int(it->record,"uses",1);
	}
	if (count == 0 && !(item->type == OBJ_MISC && item->id == 102))
		return;
	if (count <= 0)
		count = 1;

	index = dtu_inventory_add(item->type,item->id,count);
	if (index >= 0) {
		int flag_10 = pickup_flag_10(item->type,it);

		// ASCENT: Dtu_fix_partial_stack: what did not fit stays on the floor
		if (Dtu_inventory_left > 0 && Dtu_inventory_left < 128)
			item->dtu.m_1d5 = Dtu_inventory_left;
		else
			item->flags |= OF_SHOULD_BE_DEAD;
		HUD_init_message("You pick up %s.",dtu_tr_name(dtu_item_shown_name(item->type,item->id,it->name)));
		pickup_sound(flag_10);
		dtu_obj_log("pickup: obj %d type %d id %d '%s' count %d, inventory entry %d, sound %s",(int)(item-Objects),
			item->type,item->id,it->name,count,index,flag_10 ? "2216 2215" : "2215");
	}
	else {
		static fix last_message = 0;

		if (GameTime - last_message > F1_0*2 || GameTime < last_message) {
			HUD_init_message("You cannot carry %s.",dtu_tr_name(dtu_item_shown_name(item->type,item->id,it->name)));
			last_message = GameTime;
		}
	}
}
