/*
 * ASCENT: new file. The phylactery of Morik Stormhand, the lich of CLAN3.
 * See dtulich.h and docs/re/lich.md. Nothing of this file is in
 * DTUGAME.EXE.
 *
 * The user's design: with the caster AI the lich gets his contingency at
 * the first notice of the player (Stone Skin, Shield, Minor Globe of
 * Invulnerability, Mirror Image), and a fighter or a thief cannot take it
 * away. So a phylactery is hidden before his room on the path of the
 * player: an altar of Velsharoon (Morik was a priest of that god, script
 * 170) in the empty crypt cell of segment 557 of the east wing of CLAN3.
 * The wing is behind the locked door of wall 104 (segment 263 side 5): a
 * thief picks the lock, the others put the bastard sword of segment 292 on
 * the shield of segment 268 (trigger 10, trap 8). One blow breaks the
 * altar (the scenery rule of dtucombat.c), and Dispel Magic too (a magic
 * piece of scenery, save 5). A Dispel Magic scroll lies in front of it.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "inferno.h"
#include "args.h"
#include "dtuobj_int.h"
#include "game.h"
#include "segment.h"
#include "gameseg.h"
#include "polyobj.h"
#include "digi.h"
#include "dtuopts.h"
#include "dtulang.h"
#include "dtuscript.h"
#include "dtuspell.h"
#include "dtudisp.h"
#include "dtufile.h"
#include "dtulich.h"

#define LICH_LEVEL			19			// CLAN3 (Dtu_script_level, the line of DTU.MSN)
#define LICH_SCRIPT			190			// Morik Stormhand, object 10
#define LICH_MISSION_FLAG	4			// mission 13: the creature with script 190 is dead
#define LICH_N_DRESSING		33			// the $DRESSING lines of the table of CLAN3
#define LICH_SCROLL			55			// Dispel Magic, the version of object 30 of CLAN3 (docs/re/lich.md)
#define BARANDER_SCRIPT		170

// The cell of segment 557: x 285 to 305, z 295 to 315, floor at y 2.5, its
// door on the side of x 305. The altar (altar_a.pof, 13.0 wide, 7.1 high,
// 6.8 deep, its front at +z of the model) stands at the back wall and
// faces the door; the scroll lies between it and the door.
static const vms_vector Altar_pos = { 289 * F1_0 + F1_0 / 2, 6 * F1_0, 305 * F1_0 };
static const vms_vector Scroll_pos = { 299 * F1_0, 12 * F1_0 + F1_0 / 2, 298 * F1_0 };

int Dtu_lich_phylactery = 1;		// the menu line (dtuopts.h)
static int Lich_arg = -1;			// -origobj, -nophylactery: 0 for this run
static int Lich_dressing = -1;		// the line of the phylactery in Dressing_info, -1: none

int dtu_lich_on(void)
{
	if (Lich_arg < 0)
		Lich_arg = !FindArg("-origobj") && !FindArg("-nophylactery");
	return Dtu_lich_phylactery && Lich_arg;
}

static int in_level(void)
{
	return Dtu_script_level == LICH_LEVEL;
}

//------------------------------------------------------------------------
// The table line

void dtu_lich_tables(const char *level_name, int n_dressing, void (*parse_line)(char *line))
{
	char line[400];

	Lich_dressing = -1;
	if (level_name == NULL || strnicmp(level_name, "clan3", 5) || (level_name[5] != 0 && level_name[5] != '.'))
		return;
	if (n_dressing != LICH_N_DRESSING) {
		dtu_log("Lich phylactery: the table of %s has %d $DRESSING lines, not %d: no phylactery\n", level_name, n_dressing,
			LICH_N_DRESSING);
		return;
	}
	// the altar of line 13, a magic piece of scenery with its own name
	snprintf(line, sizeof(line), "$DRESSING pof=altar_a.pof name=\"%s\" alt_01.bbm alt_02.bbm alt_03.bbm alt_04.bbm alt_05.bbm "
		"alt_06.bbm alt_07.bbm magic explode_sound=2218 break_vclip=62", LN("an altar of Velsharoon"));
	parse_line(line);
	if (N_dressing_types != n_dressing + 1 || Dressing_info[n_dressing].model_num < 0) {
		dtu_log("Lich phylactery: the line did not go in\n");
		return;
	}
	Lich_dressing = n_dressing;
	dtu_log("Lich phylactery: $DRESSING line %d, model %d\n", Lich_dressing, Dressing_info[Lich_dressing].model_num);
}

//------------------------------------------------------------------------
// The objects

static int alive(object *o)
{
	return o->type == OBJ_ROBOT && !(o->flags & (OF_SHOULD_BE_DEAD|OF_DESTROYED|OF_EXPLODING)) && o->shields >= F1_0 &&
		DTU_ANIM_STATE(o) != DAS_DIE;
}

static object *find_lich(void)
{
	int i;

	for (i=0;i<=Highest_object_index;i++)
		if (Objects[i].type == OBJ_ROBOT && (Objects[i].dtu.m_1b0 & DTU_SCRIPT_MASK) == LICH_SCRIPT && alive(&Objects[i]))
			return &Objects[i];
	return NULL;
}

static object *find_phylactery(void)
{
	int i;

	if (Lich_dressing < 0)
		return NULL;
	for (i=0;i<=Highest_object_index;i++)
		if (Objects[i].type == OBJ_DRESSING && Objects[i].id == Lich_dressing && !(Objects[i].flags & (OF_SHOULD_BE_DEAD|OF_DESTROYED)))
			return &Objects[i];
	return NULL;
}

void dtu_lich_level_setup(int has_state, int restore)
{
	vms_vector pos;
	vms_angvec a = { 0, 0, 0x4000 };		// heading 90 degrees: the front of the model to +x, the door
	vms_matrix m;
	int seg, n, model;

	if (!in_level() || !dtu_lich_on() || Lich_dressing < 0)
		return;
	if (has_state && !restore)
		return;
	if (dtu_script_gflag(DTU_LICH_GF_PLACED) || dtu_script_gflag(DTU_LICH_GF_BROKEN))
		return;
	if (dtu_mission_flag(LICH_MISSION_FLAG) || find_lich() == NULL) {
		dtu_obj_log("lich: no phylactery, the lich is dead (mission flag %d: %d)",LICH_MISSION_FLAG,dtu_mission_flag(LICH_MISSION_FLAG));
		return;
	}
	pos = Altar_pos;
	seg = find_point_seg(&pos,-1);
	model = Dressing_info[Lich_dressing].model_num;
	if (seg < 0 || model < 0) {
		dtu_obj_log("lich: no segment at the place of the phylactery");
		return;
	}
	vm_angles_2_matrix(&m,&a);
	n = obj_create(OBJ_DRESSING,Lich_dressing,seg,&pos,&m,Polygon_models[model].rad,CT_NONE,MT_NONE,RT_POLYOBJ);
	if (n < 0)
		return;
	Objects[n].rtype.pobj_info.model_num = model;
	Objects[n].rtype.pobj_info.tmap_override = -1;
	Objects[n].shields = F1_0;
	Objects[n].dtu.m_1b0 = 0;
	dtu_script_set_gflag(DTU_LICH_GF_PLACED,1);
	dtu_obj_log("lich: the phylactery is obj %d in segment %d at %.2f %.2f %.2f",n,seg,f2fl(pos.x),f2fl(pos.y),f2fl(pos.z));
	pos = Scroll_pos;
	seg = find_point_seg(&pos,-1);
	if (seg >= 0 && LICH_SCROLL < N_spell_types && (n = dtu_create_item(OBJ_SPELL,LICH_SCROLL,1,seg,&pos)) >= 0)
		dtu_obj_log("lich: the scroll of Dispel Magic (spell %d) is obj %d in segment %d",LICH_SCROLL,n,seg);
}

int dtu_lich_phylactery_destroyed(void)
{
	return dtu_lich_on() && dtu_script_gflag(DTU_LICH_GF_BROKEN);
}

static int is_lich(object *obj)
{
	return obj && obj->type == OBJ_ROBOT && in_level() && (obj->dtu.m_1b0 & DTU_SCRIPT_MASK) == LICH_SCRIPT;
}

int dtu_lich_no_contingency(object *obj)
{
	// the caster log of the start of the fight says "contingency: none"
	return is_lich(obj) && dtu_lich_phylactery_destroyed();
}

static void message(const char *text)
{
	dtu_obj_log("lich: message \"%s\"",text);
	dtu_display_add_message((char *) text);
}

void dtu_lich_dressing_broken(object *d, object *by)
{
	object *lich;
	int protected;

	if (d == NULL || d->type != OBJ_DRESSING || Lich_dressing < 0 || d->id != Lich_dressing || !in_level() || !dtu_lich_on())
		return;
	if (dtu_script_gflag(DTU_LICH_GF_BROKEN))
		return;
	dtu_script_set_gflag(DTU_LICH_GF_BROKEN,1);
	lich = find_lich();
	protected = lich && (dtu_spell_images(lich) > 0 || dtu_spell_globe(lich) || (lich->dtu.m_88 & (DTU_FX_STONESKIN|DTU_FX_SHIELD)));
	dtu_obj_log("lich: obj %d breaks the phylactery obj %d; the lich %s",by ? (int)(by-Objects) : -1,(int)(d-Objects),
		lich == NULL ? "is dead" : protected ? "loses his protections" : "has no protections now");
	if (protected)
		dtu_spell_end_contingency(lich);
	message(protected ? LT("The altar of Velsharoon cracks apart. With a howl of rage the wards of Morik Stormhand fail!")
		: LT("The altar of Velsharoon cracks apart. A thin scream rises from the crypts far below."));
}

//------------------------------------------------------------------------
// The texts

const char *dtu_lich_script_text(int script, int offset, const char *en)
{
	if (!dtu_lich_on() || en == NULL)
		return NULL;
	// Barander (CLAN1 object 141), label 12: the answer to "How can he be
	// stopped?" (after "Who's Morik?")
	if (script == BARANDER_SCRIPT && offset == 3845 && !strncmp(en, "I am attempting a ritual consecration", 37))
		return LN("Not by steel alone. A lich binds his life to a phylactery, and Morik was a priest of Velsharoon: his will be an altar of his dark god, hidden in some crypt near his lair. While it stands, his wards rise at his call. Break it before you face him. As for me, I am attempting a ritual consecration of this chamber. Once completed, this room will be a strongpoint that I can use to remove undead influences from this place. But the ritual is a very, very long one. If you can keep the undead at bay, I would be grateful.");
	// the lich, label 4 ("I cannot die, child"), after the break
	if (script == LICH_SCRIPT && offset == 1513 && in_level() && dtu_script_gflag(DTU_LICH_GF_BROKEN) &&
			!strncmp(en, "You have less brains than I have flesh.", 39))
		return LN("So you found my altar and broke it, you meddling worm. No matter. I shall raise another from your bones.");
	return NULL;
}

const char *dtu_lich_say_text(const char *level, int n)
{
	if (!dtu_lich_on() || level == NULL || stricmp(level, "CLAN3") || n != 1 || dtu_script_gflag(DTU_LICH_GF_BROKEN))
		return NULL;
	dtu_obj_log("lich: the text of the port for message 1 of CLAN3");
	return LT("The air here is a bit stale and musty. The floor is covered in dust that reveals the tracks of many different creatures that have passed here. One set of tracks, made by bare bone, leads again and again to the east crypts.");
}

//------------------------------------------------------------------------
// Test

void dtu_lich_test(int n)
{
	object *p = find_phylactery(), *pl = ConsoleObject;

	if (n == 1 || p == NULL) {
		dtu_obj_log("lich: test: phylactery %s, placed %d, broken %d, on %d, lich %s",p ? "in the level" : "not in the level",
			dtu_script_gflag(DTU_LICH_GF_PLACED),dtu_script_gflag(DTU_LICH_GF_BROKEN),dtu_lich_on(),find_lich() ? "alive" : "dead");
		if (p)
			dtu_obj_log("lich: test: phylactery obj %d seg %d at %.2f %.2f %.2f",(int)(p-Objects),p->segnum,f2fl(p->pos.x),f2fl(p->pos.y),
				f2fl(p->pos.z));
		return;
	}
	if (n == 0)
		dtu_break_dressing(p,pl,0);
	else if (n == 2 && pl) {
		// at the door of the cell, looking at the altar
		vms_vector pos = { 300 * F1_0, 10 * F1_0, 305 * F1_0 };
		vms_angvec a = { 0, 0, (fixang) 0xC000 };
		int seg = find_point_seg(&pos,-1);

		if (seg >= 0) {
			pl->pos = pos;
			vm_angles_2_matrix(&pl->orient,&a);
			obj_relink(pl-Objects,seg);
		}
	}
}
