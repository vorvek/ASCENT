/*
 * ASCENT: new file. See dtuobj.h.
 *
 * The functions follow DTUGAME.EXE 1.3. The address of the original code
 * is at each function. Addresses are linear addresses of the image with
 * object 1 at 0x10000. The marks in the comments:
 *   (code)  read in the disassembly
 *   (doc)   taken from docs/re and not read again
 *   (guess) not in the executable in this form
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
#include "dtuobj_int.h"	// ASCENT: the shared declarations of the object files

//------------------------------------------------------------------------
// Settings

int Dtu_fix_idle_anim = 1;
int Dtu_fix_ai_ticks = 1;
int Dtu_fix_creature_move = 1;
int Dtu_creatures_attack = 1;
int Dtu_idle_anim_near = 0;
int Dtu_player_dead = 0;
int Dtu_fix_path_follow = 1;
int Dtu_fix_fire_once = 1;

// The sound of a creature that sees the player (attack_sound of the monster
// table). Original: the code is in compute_vis_and_vec (0x12E2F to 0x12F3C)
// and needs the visibility 2, which the sight function 0x109E7 never gives.
// So the sound is never played by the AI. Corrected (1, default): the sound
// is played by the rules of that code when the creature sees the player in
// its field of view. The option -origobj clears it.
int Dtu_fix_attack_sound = 1;

// 1: a creature sees the player if a ray from it to the player is free.
// 0: the rule of the executable: the bit of the BRN file of the level for
// the two segments (0x9810D). The BRN files of the disc are copies of one
// matrix that fits no level (docs/formats/brn.md), so the default is 1.
int Dtu_fix_brain = 1;

// The corpse of a creature. Original (0x6A48B): at the end of DIE the
// position goes to the floor by the torso box of the frame, and a creature
// that flies has no gravity in DIE, so its corpse stays where it died, in
// the air. Corrected (1, default): the dying body falls, and the corpse
// lies with the lowest drawn point of its last frame on the floor (the
// joint angles of the frame and the body center that the drawing takes
// away, not the box of the file), also a corpse of a loaded level. The
// option -origobj clears it. See docs/re/ai.md, "The corpse".
int Dtu_fix_corpse_floor = 1;

// The peace of the members of the Shadow Thieves with quest flag 67
// (dtuobj.h). The options -origobj and -origguildpeace clear it.
int Dtu_fix_guild_peace = 1;

// The shields of the hatchling of DROW5 (object 83) at the level fixes
// (dtuobj.h). The options -origobj and -origdrow5 clear it.
int Dtu_fix_drow5_shields = 1;

// ASCENT: a pick up of a stack that does not fit (dtuobj.h). -origobj
// clears it.
int Dtu_fix_partial_stack = 1;
int Dtu_inventory_left = 0;
// A dead creature that regenerates gets up again (dtuobj.h). The options
// -origobj and -origrevive clear it.
int Dtu_fix_revive = 1;

// The weapon of the spellcaster of KOBOLDS also with a saved state, the hit
// points of scenery at a restore, the texture of the spinning scenery of
// BANE (dtuobj.h). -origobj clears them.
int Dtu_fix_kobold_weapon = 1;
int Dtu_fix_scenery_restore = 1;
int Dtu_fix_bane_texture = 1;

// The option -origobj (or -noimprovedpaths, a test option that changes
// nothing else): Dtu_improved_pathfinding has no effect.
static int Orig_obj = 0;

// Dtu_improved_pathfinding (dtuopts.h): paths for the body of a person and
// a body that stops at the reach of its weapon. See docs/re/ai.md,
// "Improved pathfinding". (The corpse on the floor is Dtu_fix_corpse_floor.)
static int improved_paths(void)
{
	return Dtu_improved_pathfinding && !Orig_obj;
}

// The reach of the weapon of each model, see model_weapon_reach(). 0: not
// computed, -1: no weapon box. Cleared at the setup of a level.
static fix Model_reach[MAX_POLYGON_MODELS];

// The box of the drawn points of one frame of each model, see
// corpse_depth(). ok 0: not computed, 1: computed, 2: no points. Cleared
// at the setup of a level.
static struct {
	short frame;
	ubyte ok;
	vms_vector mn, mx;
} Model_points[MAX_POLYGON_MODELS];

//------------------------------------------------------------------------
// Tables

// 0x13D9E0
short Dtu_anim_first[DTU_N_ANIM_STATES+1] = {
	0, 0, 4, 20, 24, 28, 44, 60, 76, 84, 96, 108, 124, 140, 152, 164, 168, 176, 180, 192 };

static char *Anim_state_names[] = { "REST", "STAND", "WALK", "SIDLE_R", "SIDLE_L", "RUN",
	"ATTACK", "ATTACK2", "FLINCH", "DIE", "FIDGET" };
static char *Ai_state_names[] = { "NONE", "REST", "SRCH", "LOCK", "FLIN", "ATAK", "RECO", "DEAD" };
static char *Ai_mode_names[] = { "STILL", "WANDER", "FOL_PATH", "CHASE_OB", "RUN_FROM", "HOME", "OPENDOOR" };

monster_ext Monster_ext[MAX_ROBOT_TYPES];

dtu_ai_local Ail[MAX_OBJECTS];

dtu_missile Missile[MAX_OBJECTS];

// Scenery that moves (movement type 1 of the executable): pushed scenery
// (0x9FA41, 0x9E5BE) and the scenery that falls after a break (0xA6ACB).
// pcons.c 0x9FE68 moves it.
static ubyte Dressing_fall[MAX_OBJECTS];
static int Dressing_sig[MAX_OBJECTS];
static int Dressing_str[MAX_OBJECTS];		// strength of the push (Dtu_fix_push_body)
static vms_vector Dressing_vpush[MAX_OBJECTS];	// velocity of the mover at the push (Dtu_fix_push_rate)
static vms_vector Dressing_slide[MAX_OBJECTS];	// velocity of the slide at the end of a push (Dtu_fix_push_rate)
static ubyte Dressing_slid[MAX_OBJECTS];		// the slide of this push has started
// Pushed scenery and the bodies of the player, the creatures and other
// scenery. The executable stops the move of scenery whose center is inside
// of the body of the player (twice the radius of its move query, plus half
// of the radius of the scenery), and the player stops closer than that to
// a chair unless its last step was long: the push of a chair works only
// below about 30 frames per second (docs/re/objects.md). 1 (default): a
// body that the scenery starts in does not stop a move away from it, and
// the player is as large as its move query (dressing_hit_object). 0
// (-origobj): the rule of the executable.
int Dtu_fix_push_body = 1;
// Pushed scenery and the walls. 1 (default): the box of the model stays
// out of the walls (dressing_wall_frac). 0 (-origobj): the center with
// half of the radius of the body, a table goes into a wall.
int Dtu_fix_push_wall = 1;
// The lead of pushed scenery at the end of a push. With the rule of the
// executable the scenery ends up about 1.84 steps of the mover ahead of
// the place where the mover stopped (the gap of the last hit, 0.2 to 1.2
// steps, and the 8/7 step of the move after it), and a step is the
// velocity times the frame time: 4 units at 20 frames per second, 0.4 at
// 200. 1 (default): at the end of a push the scenery slides the
// difference to the step of the reference frame (DTU_REF_FRAMETIME), so
// that a push moves scenery as far at each frame rate as at 21 frames per
// second. 0 (-origobj): no slide.
int Dtu_fix_push_rate = 1;
#define DTU_PUSH_LEAD	1.3

dtu_inv_item Dtu_inventory[DTU_MAX_INVENTORY];
int Dtu_n_inventory = 0;
int Dtu_gold = 0;

// The clock of the player structure (0x1D339F): the sum of the frame times,
// wraps at 3600.0.
fix Dtu_clock = 0;
// The reference ticks (3/64 second) since the start of the level, and how
// many of them belong to this frame.
static int Ref_tick_count = 0, Ref_ticks_now = 0;
static fix Ref_tick_acc = 0;

static ubyte *Brain = NULL;			// 800 rows of 100 bytes, or NULL

// ASCENT: the BRN file of the last level is freed at the exit.
static void brain_free(void)
{
	if (Brain)
		free(Brain);
	Brain = NULL;
}
static int Player_armor_class = 10;
fix Player_max_hp = 0;

// The death of the player: 0 alive, 1 dead and waiting for the death
// screen, 2 after the death screen.
int Player_death_stage = 0;
fix Player_death_time = 0;

void sound_at(int sound,int segnum,vms_vector *pos,fix volume);
void sound_on_object(int sound,object *obj,fix volume);
void vclip_at(int segnum,vms_vector *pos,fix size,int vclip);
int weapon_int(int w,char *key,int def);
int weapon_is_melee(int w);
int player_type_sound(char *key);
void blood(object *obj);
void setup_creature(object *obj);
static void cloak_reset(void);
static void morale_check(object *obj);
static int N_aware_events;		// the awareness events of the frame (0x13A6BC)
fix creature_floor_dist(object *obj,fix radius,fix half_height);
void log_corpse_height(object *obj,const char *what);
int corpse_on_floor(object *obj);
static fix corpse_draw_drop(object *obj);
static fix corpse_depth(object *obj,int frame,vms_vector *mn,vms_vector *mx);
item_info *item_table_entry(int type,int id);
static int dressing_pushable(object *d);
fix damage_player_w(object *player, object *attacker, fix damage, int weapon);
fix damage_creature_w(object *creature, object *attacker, fix damage, int weapon);
fix spell_hit(int spell,object *target,object *attacker);
static void push_dressing(object *mover,object *d,int strength);
int is_hostile(object *obj);
static fix model_weapon_reach(object *obj);
static fix creature_stop_dist(object *obj);

// The attack of the player
int Player_swing_done = 0;

FILE *Obj_log = NULL;
int Obj_log_frames = 0;		// option -objlogframes: a line for each change of the animation frame
static int Obj_log_pos = 0;			// option -objlogpos N: the place of each active creature each N frames
int Setup_log = 0;				// option -setuplog (test): one session log line for each creature setup by name
static int Test_kill = 0;				// option -testkill N[,W] (test): each N frames the nearest active creature dies
static int Test_kill_wall = 0;		// W 1: only a creature that hit a wall in the last half second
int Test_pos_pending;

#define MAX_IGNORE_OBJS 100		// as in physics.c

//------------------------------------------------------------------------
// Log

void dtu_obj_log(char *format, ...)
{
	va_list args;

	if (!Obj_log)
		return;
	fprintf(Obj_log,"%9.3f %6d ",f2fl(GameTime),FrameCount);
	va_start(args,format);
	vfprintf(Obj_log,format,args);
	va_end(args);
	fprintf(Obj_log,"\n");
	fflush(Obj_log);
}

char *dtu_object_name(object *obj)
{
	static char unknown[] = "something";

	switch (obj->type) {
		case OBJ_ROBOT:		if (obj->id < N_robot_types) return Monster_info[obj->id].name; break;
		case OBJ_DRESSING:	if (obj->id < N_dressing_types) return Dressing_info[obj->id].name; break;
		case OBJ_MISC:			if (obj->id < N_misc_types) return Misc_info[obj->id].name; break;
		case OBJ_ARMOR:		if (obj->id < N_armor_types) return Armor_info[obj->id].name; break;
		case OBJ_SPELL:		if (obj->id < N_spell_types) return Spell_info[obj->id].name; break;
		case OBJ_WEAPON:		if (obj->id < N_weapon_types) return Weapon_item_info[obj->id].name; break;
		case OBJ_PLAYER:		return "player";
	}
	return unknown;
}

// ASCENT: the English name of an item as the player sees it: name (the
// name of the table line) or its correction (dtuitemfix.h,
// Dtu_fix_item_names). In a language that has no translation of the
// corrected name the name of the table stays, so that its translation is
// shown. The result goes to dtu_tr_name.
char *dtu_item_shown_name(int type, int id, char *name)
{
	int kind = type == OBJ_WEAPON ? DTU_IK_WEAPON : type == OBJ_ARMOR ? DTU_IK_ARMOR : type == OBJ_MISC ? DTU_IK_MISC : -1;
	char *fixed;

	if (kind < 0 || name == NULL)
		return name;
	fixed = (char *) dtu_itemfix_name(kind, id, name);
	if (fixed != name && Dtu_language != DTU_LANG_EN && dtu_tr_name(fixed) == fixed)
		return name;
	return fixed;
}

// The names of the creatures in the messages (0xA504A, 26 bytes for each
// entry: the table name in 16 bytes, at 0x10 a pointer to the string
// pointer, at 0x14 a signed byte; a negative byte ends the list). 184
// entries point into Text_string[] (0x395DAC), the strings 845 to 905 of
// DTUE.TEX; 7 named characters point to strings of the executable.
// 0xA7E47 compares the table name without case (0xD89C1); with no entry
// the executable prints a debug line (0x131537) and shows the table name.
static const struct {
	const char *name;
	int text;			// Text_string index, or -1
	const char *fixed;	// the string when text is -1
} Creature_names[] = {
	{ "banegard", 845, NULL },
	{ "beetle", 846, NULL },
	{ "beholder", 847, NULL },
	{ "xabash", 848, NULL },
	{ "spectatr", 849, NULL },
	{ "dragon", 850, NULL },
	{ "hatchlng", 851, NULL },
	{ "gargoyle", 852, NULL },
	{ "ghast", 853, NULL },
	{ "ghoul", 854, NULL },
	{ "blackbat", 855, NULL },
	{ "giantbat", 856, NULL },
	{ "gobahaxe", 857, NULL },
	{ "gobascim", 857, NULL },
	{ "gobatdag", 857, NULL },
	{ "gbchaxe", 857, NULL },
	{ "gbcscim", 857, NULL },
	{ "gbcsswrd", 857, NULL },
	{ "gbctdagr", 857, NULL },
	{ "gbcthaxe", 857, NULL },
	{ "gbloser", 857, NULL },
	{ "goblin", 857, NULL },
	{ "gobhaxe", 857, NULL },
	{ "gobscim", 857, NULL },
	{ "gobsswrd", 857, NULL },
	{ "gobtdagr", 857, NULL },
	{ "gobthaxe", 857, NULL },
	{ "gbscscim", 857, NULL },
	{ "gbscsswd", 857, NULL },
	{ "gbsctdgr", 857, NULL },
	{ "gbscthxe", 857, NULL },
	{ "gskelsci", 858, NULL },
	{ "gskelswd", 858, NULL },
	{ "gskelpus", 858, NULL },
	{ "halaster", 859, NULL },
	{ "ibbalar", 860, NULL },
	{ "kobaswrd", 861, NULL },
	{ "kob_bow", 861, NULL },
	{ "kob_spel", 861, NULL },
	{ "kob_swrd", 861, NULL },
	{ "lich1", 862, NULL },
	{ "lich2", 862, NULL },
	{ "lich3", 862, NULL },
	{ "fmephit", 863, NULL },
	{ "imephit", 864, NULL },
	{ "lmephit", 865, NULL },
	{ "flayer", 866, NULL },
	{ "mummy", 867, NULL },
	{ "ogre", 868, NULL },
	{ "ogre2", 868, NULL },
	{ "armogre", 868, NULL },
	{ "orcaaxe", 869, NULL },
	{ "orcascim", 869, NULL },
	{ "orcataxe", 869, NULL },
	{ "orc_axe", 869, NULL },
	{ "orc_scim", 869, NULL },
	{ "orc_taxe", 869, NULL },
	{ "gorcaaxe", 869, NULL },
	{ "gorcasci", 869, NULL },
	{ "gorcatax", 869, NULL },
	{ "gorc_axe", 869, NULL },
	{ "gorcscim", 869, NULL },
	{ "gorc_tax", 869, NULL },
	{ "rat", 870, NULL },
	{ "giantrat", 871, NULL },
	{ "scarab", 872, NULL },
	{ "sfiend", 873, NULL },
	{ "ske_axe", 874, NULL },
	{ "ske_bow", 874, NULL },
	{ "ske_sci", 874, NULL },
	{ "ske_swd", 874, NULL },
	{ "ske_taxe", 874, NULL },
	{ "sspider", 875, NULL },
	{ "gspider", 876, NULL },
	{ "qspider", 877, NULL },
	{ "avatar", 878, NULL },
	{ "illtroll", 879, NULL },
	{ "troll", 879, NULL },
	{ "armtroll", 879, NULL },
	{ "zombie", 880, NULL },
	{ "rffb1", 881, NULL },
	{ "rffg1", 881, NULL },
	{ "rffi1", 881, NULL },
	{ "rffq1", 881, NULL },
	{ "rfft1", 882, NULL },
	{ "rfft4", 882, NULL },
	{ "rfft6", 882, NULL },
	{ "rfft9", 882, NULL },
	{ "rffk1", 882, NULL },
	{ "rffk5", 882, NULL },
	{ "rffk8", 882, NULL },
	{ "rffk8b", 882, NULL },
	{ "rffk9", 882, NULL },
	{ "rftj1", 883, NULL },
	{ "rfte1", 883, NULL },
	{ "rftg1", 883, NULL },
	{ "rfti1", 883, NULL },
	{ "rftk1", 883, NULL },
	{ "rftq1", 883, NULL },
	{ "rmfb1", 881, NULL },
	{ "rmfb3", 881, NULL },
	{ "rmfb5", 881, NULL },
	{ "rmfb9", 881, NULL },
	{ "rmfg1", 881, NULL },
	{ "rmfg3", 881, NULL },
	{ "rmfg5", 881, NULL },
	{ "rmfg8", 881, NULL },
	{ "rmfg9", 881, NULL },
	{ "rmfi1", 881, NULL },
	{ "rmfi5", 881, NULL },
	{ "rmfq1", 881, NULL },
	{ "rmft1", 884, NULL },
	{ "rmfk1", 884, NULL },
	{ "rmfk4", 884, NULL },
	{ "rmfk5", 884, NULL },
	{ "rmfk9", 884, NULL },
	{ "rmme1", 885, NULL },
	{ "rmmf1", 885, NULL },
	{ "rmmf3", 885, NULL },
	{ "rmmq1", 885, NULL },
	{ "rmmk1", 885, NULL },
	{ "rmmk3", 885, NULL },
	{ "rmmk5", 885, NULL },
	{ "rmmk7", 885, NULL },
	{ "rmmk9", 885, NULL },
	{ "rmmk9b", 885, NULL },
	{ "hffb1", 886, NULL },
	{ "hffc1", 886, NULL },
	{ "hffg1", 886, NULL },
	{ "hffg4", 886, NULL },
	{ "hffk1", 887, NULL },
	{ "hfft1", 888, NULL },
	{ "hfft4", 888, NULL },
	{ "hfft9", 888, NULL },
	{ "hmct1", 888, NULL },
	{ "hmfc1", 886, NULL },
	{ "hmfb1", 886, NULL },
	{ "hmfv1", 886, NULL },
	{ "hmfg1", 886, NULL },
	{ "hmfh1", 886, NULL },
	{ "hmfk1", 888, NULL },
	{ "hmfm1", 886, NULL },
	{ "hmfr1", 886, NULL },
	{ "hmft4", 888, NULL },
	{ "hmft7", 888, NULL },
	{ "hmfy1", 886, NULL },
	{ "hmmk1", 889, NULL },
	{ "hfmk1", 889, NULL },
	{ "hmmnpc", 889, NULL },
	{ "dmfb1", 890, NULL },
	{ "dmfc1", 890, NULL },
	{ "dmfe1", 890, NULL },
	{ "dmff1", 890, NULL },
	{ "dmfg1", 890, NULL },
	{ "dmfs1", 890, NULL },
	{ "dmfw1", 890, NULL },
	{ "dmfx1", 890, NULL },
	{ "dmfnpc", 890, NULL },
	{ "sfmtd2", 891, NULL },
	{ "sfmtf2", 891, NULL },
	{ "shmfb1", 892, NULL },
	{ "shmfb5", 892, NULL },
	{ "shmfc3", 892, NULL },
	{ "shmfg1", 892, NULL },
	{ "shmfg5", 892, NULL },
	{ "shmtd2", 893, NULL },
	{ "shmtd4", 893, NULL },
	{ "shmtd6", 893, NULL },
	{ "shmtf2", 893, NULL },
	{ "shmtf4", 893, NULL },
	{ "shmtf6", 893, NULL },
	{ "shdleadf", 894, NULL },
	{ "shdleadg", 894, NULL },
	{ "prstsprt", 895, NULL },
	{ "Mage Spirit", 896, NULL },
	{ "thashasa", 897, NULL },
	{ "flaynpc", 898, NULL },
	{ "priestes", 899, NULL },
	{ "durnan", 900, NULL },
	{ "Aleena", 901, NULL },
	{ "fmtnpc", -1, "Nalbas Oddpepper" },
	{ "tolem", 903, NULL },
	{ "hffnpc", 886, NULL },
	{ "drowfem", 904, NULL },
	{ "mirt", 905, NULL },
	{ "Jeryth", -1, "Meryth Phaulkon" },
	{ "Taria", -1, "Taria" },
	{ "Dwarro", -1, "Dwarro" },
	{ "Daland", -1, "Daland" },
	{ "Zintoth", -1, "Zintoth" },
	{ "Daspel", -1, "Daspel" },
};

char *dtu_object_display_name(object *obj)
{
	char *name = dtu_object_name(obj);
	int i;

	if (obj->type != OBJ_ROBOT)
		return dtu_tr_name(dtu_item_shown_name(obj->type, obj->id, name));		// ASCENT: the language (dtulang.h), the corrected item names
	for (i = 0; i < (int) (sizeof(Creature_names) / sizeof(Creature_names[0])); i++)
		if (!stricmp(Creature_names[i].name, name)) {
			if (Creature_names[i].text < 0)
				return (char *) Creature_names[i].fixed;
			if (Text_string[Creature_names[i].text])
				return Text_string[Creature_names[i].text];
			break;
		}
	return name;
}

// ASCENT: the gender of the name of dtu_object_display_name: the field
// gender of the creature name (tex.N) or of the item name (tbl.*) of the
// catalog
int dtu_object_name_gender(object *obj)
{
	char *name = dtu_object_name(obj);
	int i;

	if (obj->type != OBJ_ROBOT)
		return dtu_name_gender(dtu_item_shown_name(obj->type, obj->id, name));
	for (i = 0; i < (int) (sizeof(Creature_names) / sizeof(Creature_names[0])); i++)
		if (!stricmp(Creature_names[i].name, name))
			return Creature_names[i].text < 0 ? 0 : dtu_text_gender(Creature_names[i].text);
	return 0;
}

static char *anim_name(int s)
{
	static char buf[16];

	if (s >= 0 && s <= DAS_FIDGET)
		return Anim_state_names[s];
	sprintf(buf,"%d",s);
	return buf;
}

// Writes the state of a creature to the log if it changed.
void log_creature_state(object *obj,char *why)
{
	dtu_ai_local *ail = &Ail[obj-Objects];
	ai_static *aip = &obj->ctype.ai_info;
	int key = 0;

	if (!Obj_log)
		return;
	if (Obj_log_frames)
		key = DTU_ANIM_FRAME(obj) * 0x01000193;
	key ^= DTU_ANIM_STATE(obj) | (aip->CURRENT_STATE << 8) | (aip->GOAL_STATE << 12) | (ail->mode << 16) | (ail->activity << 20) | (ail->awareness << 24);
	if (key == ail->last_logged_state)
		return;
	ail->last_logged_state = key;
	dtu_obj_log("obj %d %s: anim %s frame %d cur %s goal %s mode %s act %d awr %d morale %d hp %d (%s)",
		(int)(obj-Objects),dtu_object_name(obj),anim_name(DTU_ANIM_STATE(obj)),DTU_ANIM_FRAME(obj),
		Ai_state_names[aip->CURRENT_STATE & 7],Ai_state_names[aip->GOAL_STATE & 7],
		Ai_mode_names[ail->mode % 7],ail->activity,ail->awareness,ail->morale,f2i(obj->shields),why);
}

//------------------------------------------------------------------------
// Helpers

polymodel *obj_model(object *obj)
{
	int n = obj->rtype.pobj_info.model_num;

	if (obj->render_type != RT_POLYOBJ || n < 0 || n >= N_polygon_models)
		return NULL;
	if (Polygon_models[n].dtu_block == NULL)
		return NULL;
	return &Polygon_models[n];
}

int is_hostile(object *obj)
{
	return (obj->dtu.m_1b0 & DTU_HOSTILE) != 0;
}

int dtu_creature_is_hostile(object *obj)
{
	return obj->type == OBJ_ROBOT && is_hostile(obj);
}

static int is_flying(object *obj)
{
	return (Monster_info[obj->id].flags & MONSTER_F_FLYING) || (obj->dtu.m_88 & DTU_OF_FLY);
}

int tbl_int(int record,char *key,int def)
{
	char value[64];

	if (record < 0 || !bm_tbl_get(record,key,value,sizeof(value)) || !value[0])
		return def;
	return atoi(value);
}

// The time of an animation state of a model in seconds (0xE4 of the model).
static fix state_time(polymodel *po,int state)
{
	if (po == NULL || state < 0 || state >= DTU_N_ANIM_STATES)
		return 0;
	return po->state_times[state];
}

// 0x74AA7. The speed of the movement of an animation state.
static fix anim_speed(object *obj,int state)
{
	polymodel *po = obj_model(obj);
	fix v;

	if (po == NULL)
		return 0;
	if (state == -1)
		state = DTU_ANIM_STATE(obj);
	if (state >= DAS_WALK && state <= DAS_SIDLE_L)
		v = po->speeds[0];
	else if (state == DAS_RUN)
		v = po->speeds[2];
	else if (state >= 13 && state <= 18)
		v = po->speeds[state-10];
	else
		return 0;
	if (obj->dtu.m_88 & DTU_OF_FAST)
		v *= 2;
	else if (obj->dtu.m_88 & DTU_OF_SLOW)
		v >>= 1;
	return v;
}

void set_anim(object *obj,int state)
{
	DTU_ANIM_STATE(obj) = state;
	DTU_ANIM_FRAME(obj) = Dtu_anim_first[state];
}

// 0x746B3
void dtu_get_body_size(object *obj, fix *radius, fix *half_height)
{
	polymodel *po = obj_model(obj);

	if (obj->type == OBJ_PLAYER) {
		*radius = DTU_BODY_RAD;
		*half_height = DTU_BODY_HALF_HEIGHT;
	}
	else if (po && po->body_radius) {
		*radius = po->body_radius[0];
		*half_height = po->body_half_height[0];
	}
	else
		*radius = *half_height = obj->size;
}

// 0x748D9. The cylinder of the body for collisions. A model with a torso
// box gives the torso. The executable uses model 16 for a player (0x748F4):
// the 17th model of the archive of the player, with the frame of the model
// of the hand. Without that model the port has the constants of physics.h.
void dtu_get_body_cylinder(object *obj, vms_vector *center, fix *radius, fix *half_height)
{
	polymodel *po = obj_model(obj);
	int frame = DTU_ANIM_FRAME(obj);

	*center = obj->pos;

	if (obj->type == OBJ_PLAYER) {
		po = N_polygon_models > 16 ? &Polygon_models[16] : NULL;
		frame = dtu_combat_view_frame();
		if (Dtu_player_walk && po && po->dtu_block && (po->box_flags & DTU_BOX_BODY) && po->body_radius && po->n_frames > 0) {
			if (frame < 0 || frame >= po->n_frames)
				frame = 0;
			if ((po->box_flags & DTU_BOX_TORSO) && po->torso_center && po->torso_radius && po->torso_half_height) {
				vms_matrix m;
				vms_vector ofs;

				vm_copy_transpose_matrix(&m,&obj->orient);
				vm_vec_rotate(&ofs,&po->torso_center[0],&m);
				vm_vec_add2(center,&ofs);
				*radius = po->torso_radius[0];
				*half_height = po->torso_half_height[frame];
			}
			else {
				*radius = po->body_radius[0];
				*half_height = po->body_half_height[0];
			}
			return;
		}
		*radius = DTU_BODY_RAD;
		*half_height = DTU_BODY_HALF_HEIGHT;
		return;
	}

	if (po && po->torso_center && po->n_frames > 0) {
		vms_matrix m;
		vms_vector ofs;

		if (frame < 0 || frame >= po->n_frames)
			frame = 0;
		vm_copy_transpose_matrix(&m,&obj->orient);
		vm_vec_rotate(&ofs,&po->torso_center[0],&m);
		vm_vec_add2(center,&ofs);
		*radius = po->torso_radius[0];
		*half_height = po->torso_half_height[frame];
		return;
	}

	dtu_get_body_size(obj,radius,half_height);
}

// 0x7473F. The sphere of the weapon of the current frame. Returns 0 if
// the model has no weapon box.
int get_weapon_sphere(object *obj,int frame,vms_vector *center,fix *radius)
{
	polymodel *po = obj_model(obj);
	vms_matrix m;
	vms_vector ofs;

	if (po == NULL || !(po->box_flags & DTU_BOX_BODY) || po->weapon_center == NULL)
		return 0;
	if (frame < 0 || frame >= po->n_frames)
		return 0;

	vm_copy_transpose_matrix(&m,&obj->orient);
	vm_vec_rotate(&ofs,&po->weapon_center[frame],&m);
	vm_vec_add(center,&obj->pos,&ofs);
	*radius = po->weapon_radius[frame];
	return 1;
}

// A test of the AI that the executable writes as "(clock + objnum) % n ==
// 0". The clock is a time in 16.16 seconds, so the test is true in one
// game frame of n.
static int ai_tick(int objnum,int n)
{
	int i;

	if (!Dtu_fix_ai_ticks)
		return ((Dtu_clock + objnum) % n) == 0;

	for (i=0;i<Ref_ticks_now;i++)
		if (((Ref_tick_count - i + objnum) % n) == 0)
			return 1;
	return 0;
}

// The distance from the feet of an object down to the floor (0x6C30E).
// A negative value: the feet are in the floor.
fix creature_floor_dist(object *obj,fix radius,fix half_height)
{
	fvi_query fq;
	fvi_info hit;
	vms_vector p1;
	int fate;
	fix result;

	p1 = obj->pos;
	p1.y -= DTU_FLOOR_PROBE + half_height;

	// A creature stands with its center at the floor plus the half height
	// of the rest pose (0x13FCD puts a creature that is not active there).
	// So the probe is thin. The radius of the body is not used.
	(void)radius;
	fq.p0 = &obj->pos;
	fq.p1 = &p1;
	fq.startseg = obj->segnum;
	fq.rad = 0x2000;
	fq.thisobjnum = obj-Objects;
	fq.ignore_obj_list = NULL;
	fq.flags = 0;

	Fvi_dtu_body = 2;
	fate = find_vector_intersection(&fq,&hit);
	Fvi_dtu_body = 0;

	if (fate == HIT_WALL)
		result = (obj->pos.y - (hit.hit_pnt.y - fq.rad/2)) - half_height;
	else if (fate == HIT_BAD_P0)
		result = 0;
	else
		result = DTU_FLOOR_PROBE;

	if (abs(result) < DTU_FLOOR_EPS)
		result = 0;
	return result;
}

// 1 if a ray from p0 (in segment seg0) to p1 hits no wall.
static int ray_is_free(vms_vector *p0,int seg0,vms_vector *p1,int objnum)
{
	fvi_query fq;
	fvi_info hit;

	fq.p0 = p0;
	fq.p1 = p1;
	fq.startseg = seg0;
	fq.rad = 0;
	fq.thisobjnum = objnum;
	fq.ignore_obj_list = NULL;
	fq.flags = 0;

	return find_vector_intersection(&fq,&hit) == HIT_NONE;
}

// 0x9810D. The bit of the BRN file: segment a sees segment b.
static int brain_bit(int a,int b)
{
	if (Dtu_fix_brain || Brain == NULL)
		return 1;
	if (a < 0 || a >= 800 || b < 0 || b >= 800)
		return 0;
	return (Brain[a*100 + b/8] >> (b & 7)) & 1;
}

// 0x10815. The ray from a creature to the player. A creature that does not
// fly looks from 5.0 above its center. A blocked ray counts as free for a
// melee creature that is closer than 100.0.
// ASCENT: creature_ray_to() takes the end point (the alert of
// Dtu_fix_alert_ray casts it to the caller).
static int creature_ray_to(object *obj,vms_vector *to)
{
	vms_vector p0 = obj->pos;
	int seg;

	if (!is_flying(obj))
		p0.y += i2f(5);
	seg = find_point_seg(&p0,obj->segnum);
	if (seg < 0) {
		p0 = obj->pos;
		seg = obj->segnum;
	}
	if (ray_is_free(&p0,seg,to,obj-Objects))
		return 1;
	if (Robot_info[obj->id].attack_type == 1 && Monster_ext[obj->id].spell_level == 0)
		if (vm_vec_dist(&obj->pos,to) < i2f(100))
			return 1;
	return 0;
}

static int creature_ray_to_player(object *obj)
{
	return creature_ray_to(obj,&ConsoleObject->pos);
}

// 0x9810D(creature segment, player segment) for other code (the Spider
// Piece 0x14): the brain bit; with Dtu_fix_brain a ray.
int dtu_brain_sees_player(object *obj)
{
	if (!Dtu_fix_brain && Brain)
		return brain_bit(obj->segnum,ConsoleObject->segnum);
	return ray_is_free(&obj->pos,obj->segnum,&ConsoleObject->pos,obj-Objects);
}

// 0x109E7 player_is_visible_from_object: the brain bit and nothing else in
// the executable. The corrected rule casts a ray.
static int player_is_visible(object *obj)
{
	// 0x165C4: Hide In Shadows hides the player (dtuthief.c)
	if (dtu_thief_hidden_from(obj))
		return 0;
	if (!Dtu_fix_brain && Brain)
		return brain_bit(obj->segnum,ConsoleObject->segnum);
	return ray_is_free(&obj->pos,obj->segnum,&ConsoleObject->pos,obj-Objects);
}

//------------------------------------------------------------------------
// Setup

void dtu_objects_init(void)
{
	int t;

	if (FindArg("-noobjects"))
		Dtu_objects_enabled = 0;
	if (FindArg("-origobj"))
		Dtu_fix_idle_anim = Dtu_fix_ai_ticks = Dtu_fix_creature_move = Dtu_fix_brain =
			Dtu_fix_path_follow = Dtu_fix_fire_once = Dtu_fix_attack_sound = Dtu_fix_cant_break =
			Dtu_fix_push_body = Dtu_fix_push_wall = Dtu_fix_push_rate = Dtu_fix_corpse_floor =
			Dtu_fix_magic_tint = Dtu_fix_guild_peace = Dtu_fix_drow5_shields =
			Dtu_fix_kobold_weapon = Dtu_fix_scenery_restore = Dtu_fix_bane_texture = Dtu_fix_partial_stack = Dtu_fix_revive =
			Dtu_fix_break_spread = Dtu_fix_noise_walls = Dtu_fix_alert_ray = Dtu_fix_alert_last = 0;
	if (FindArg("-origrevive"))
		Dtu_fix_revive = 0;
	if (FindArg("-origdrow5"))
		Dtu_fix_drow5_shields = 0;
	if (FindArg("-origguildpeace"))
		Dtu_fix_guild_peace = 0;
	if (FindArg("-setuplog"))
		Setup_log = 1;
	// -origobj: the rules of the executable, also with Dtu_improved_pathfinding
	if (FindArg("-origobj") || FindArg("-noimprovedpaths"))
		Orig_obj = 1;
	if (FindArg("-noattack"))
		Dtu_creatures_attack = 0;
	if (FindArg("-idleanim"))
		Dtu_idle_anim_near = 1;
	if ((t = FindArg("-objlog")) != 0 && t+1 < Num_args)
		Obj_log = fopen(Args[t+1],"w");
	if (FindArg("-objlogframes"))
		Obj_log_frames = 1;
	if ((t = FindArg("-objlogpos")) != 0 && t+1 < Num_args)
		Obj_log_pos = atoi(Args[t+1]);
	if ((t = FindArg("-testkill")) != 0 && t+1 < Num_args)
		sscanf(Args[t+1],"%d,%d",&Test_kill,&Test_kill_wall);

	if (!Dtu_objects_enabled)
		return;

	// The pairs of the collision table of the executable (0x292D7) that
	// the port handles. All other pairs do nothing.
	memset(CollisionResult, RESULT_NOTHING, sizeof(CollisionResult));
	#define DTU_COLLIDE(a,b) do { CollisionResult[a][b] = RESULT_CHECK; CollisionResult[b][a] = RESULT_CHECK; } while (0)
	DTU_COLLIDE(OBJ_PLAYER,OBJ_WALL);
	DTU_COLLIDE(OBJ_ROBOT,OBJ_WALL);
	DTU_COLLIDE(OBJ_PLAYER,OBJ_ROBOT);
	DTU_COLLIDE(OBJ_ROBOT,OBJ_ROBOT);
	DTU_COLLIDE(OBJ_PLAYER,OBJ_WEAPON);
	DTU_COLLIDE(OBJ_PLAYER,OBJ_ARMOR);
	DTU_COLLIDE(OBJ_PLAYER,OBJ_MISC);
	DTU_COLLIDE(OBJ_PLAYER,OBJ_SPELL);
	DTU_COLLIDE(OBJ_PLAYER,OBJ_DRESSING);
	DTU_COLLIDE(OBJ_ROBOT,OBJ_DRESSING);
	#undef DTU_COLLIDE
}

// The entries of the types after the setup of the level file: a restored
// level gets them again (the executable runs the setup on the objects of
// the level file and then reads the state, 0x5AA75).
static monster_ext Setup_ext[MAX_ROBOT_TYPES];
static int Setup_ext_level = -1, Setup_ext_count = 0;

// dtustate.c: the setup of a restored level runs no object fixes (0xACCDB
// runs before the restore in the executable).
void dtu_objects_level_setup_restore(void)
{
	Level_setup_restore = 1;
	dtu_objects_level_setup();
	Level_setup_restore = 0;
}

static void end_sequence_reset(void);		// ASCENT: the end of the game (0xA6786)

void dtu_objects_level_setup(void)
{
	int i, has_state;
	ubyte *data;
	int size;
	char name[32];

	if (!Dtu_objects_enabled)
		return;

	Dtu_clock = 0;
	N_aware_events = 0;
	// ASCENT: the believed position of a hidden player starts at the player
	// (C20; the start value of 1.3 was not found)
	cloak_reset();
	Ref_tick_count = Ref_ticks_now = 0;
	end_sequence_reset();
	Ref_tick_acc = 0;
	memset(Model_reach,0,sizeof(Model_reach));
	memset(Model_points,0,sizeof(Model_points));

	// 0xA7E97 at the end of each $MONSTER line: the NPC table replaces
	// thaco and armor_class (and the spell levels and points, which
	// dtuspell.c reads with dtu_npc_row)
	for (i=0;i<N_robot_types;i++) {
		int v[6];

		if (dtu_npc_row(Monster_info[i].name,v) && (Monster_info[i].thaco != v[0] || Monster_info[i].armor_class != v[1])) {
			dtu_obj_log("npc table: '%s' thaco %d -> %d, armor class %d -> %d, mage %d/%d, cleric %d/%d",Monster_info[i].name,
				Monster_info[i].thaco,v[0],Monster_info[i].armor_class,v[1],v[2],v[3],v[4],v[5]);
			Monster_info[i].thaco = v[0];
			Monster_info[i].armor_class = v[1];
		}
	}
	Player_swing_done = 0;
	Test_pos_pending = 1;
	Dtu_player_dead = 0;

	for (i=0;i<N_robot_types;i++) {
		int rec = Monster_info[i].record;

		Monster_ext[i].damage_dice = tbl_int(rec,"damage_dice",0);
		Monster_ext[i].n_damage_dice = tbl_int(rec,"#damage_dice",0);
		Monster_ext[i].to_hit_bonus = tbl_int(rec,"to_hit_bonus",0);
		Monster_ext[i].damage_bonus = tbl_int(rec,"damage_bonus",0);
		Monster_ext[i].regeneration = tbl_int(rec,"regeneration",0);
		Monster_ext[i].weapon_type = tbl_int(rec,"weapon_type",-1);
		Monster_ext[i].spell_level = tbl_int(rec,"spell_level",0);
		Monster_ext[i].door_opening = (tbl_int(rec,"door_opening",0) == 1) || (Monster_info[i].flags & MONSTER_F_HUMANOID);
		Monster_ext[i].blood_vclip = tbl_int(rec,"monster_hit_vclip",9);
		Monster_ext[i].touch_spell = -1;
	}
	memset(Missile,0,sizeof(Missile));
	Player_death_stage = 0;
	if (dtu_player_hit_points() > Player_max_hp)
		Player_max_hp = dtu_player_hit_points();

	Dtu_script_level = Current_level_num - 1;

	{
		extern int Dtu_level_has_state[];
		int l = Dtu_script_level;

		has_state = Level_setup_restore || (l >= 0 && l < 100 && Dtu_level_has_state[l]);
	}
	// 0xACCDB, before the creature setup and add_npc (level_start_npcs)
	level_fixes_tables(has_state);
	// A restored level: the entries of the types as the setup of the level
	// file left them
	if (Level_setup_restore && Setup_ext_level == Current_level_num && Setup_ext_count == N_robot_types) {
		memcpy(Monster_ext,Setup_ext,sizeof(Monster_ext[0]) * N_robot_types);
		dtu_obj_log("creature setup (0xAC7B2): the restored level keeps the types of the level file");
	}

	Player_armor_class = 10;
	if (N_player_types > 0)
		Player_armor_class = tbl_int(Player_type_info[0].record,"armor_class",10);

	// The BRN file of the level (loader 0x97774)
	if (Brain) {
		free(Brain);
		Brain = NULL;
	}
	{
		char *p;

		strncpy(name,Gamesave_current_filename,sizeof(name)-5);
		name[sizeof(name)-5] = 0;
		if ((p = strchr(name,'.')) != NULL)
			*p = 0;
		strcat(name,".BRN");
		data = dtu_load_file(name,&size);
		if (data && size >= 80000) {
			static int registered = 0;		// ASCENT: brain_free
			Brain = data;
			if (!registered)
				atexit(brain_free);
			registered = 1;
		}
		else if (data)
			free(data);
	}

	Level_setup_pass = 1;
	for (i=0;i<=Highest_object_index;i++) {
		object *obj = &Objects[i];

		switch (obj->type) {

			case OBJ_ROBOT:
				setup_creature(obj);
				// the restore of a level: 0xAE3F2 for the saved creature
				if (Level_setup_restore) {
					if (obj->control_type == CT_AI)
						Ail[i].spell_time = GameTime;
					dtu_spell_creature_restore(obj);
				}
				break;

			case OBJ_DRESSING:
				// setup_object: shields 1.0. fix_object_segs 0x69FFB: the model
				// or the bitmap of the table entry. A restore (0xAE3F2 at
				// 0xBB1A7) also sets 1.0; Dtu_fix_scenery_restore keeps the
				// saved hit points (DROW5 object 13, IBBALAR object 2: 256).
				if (!Level_setup_restore || !Dtu_fix_scenery_restore)
					obj->shields = F1_0;
				if (obj->id >= N_dressing_types) {
					obj->render_type = RT_NONE;
					break;
				}
				if (obj->render_type == RT_POLYOBJ) {
					obj->rtype.pobj_info.model_num = Dressing_info[obj->id].model_num;
					obj->rtype.pobj_info.tmap_override = -1;
					if (obj_model(obj) == NULL)
						obj->render_type = RT_NONE;
				}
				else if (obj->render_type == RT_ITEM) {
					obj->dtu.item_bitmap = Dressing_info[obj->id].obj_bitmap;
					if (obj->dtu.item_bitmap < 0)
						obj->render_type = RT_NONE;
				}
				else if (obj->render_type == RT_WEAPON_VCLIP) {
					if (Dressing_info[obj->id].vclip_num >= 0)
						obj->rtype.vclip_info.vclip_num = Dressing_info[obj->id].vclip_num;
				}
				break;

			case OBJ_MISC:
			case OBJ_ARMOR:
			case OBJ_SPELL:
			case OBJ_WEAPON:
				if (obj->render_type == RT_ITEM) {
					item_info *it = item_table_entry(obj->type,obj->id);

					obj->dtu.item_bitmap = it ? it->obj_bitmap : -1;
					if (obj->dtu.item_bitmap < 0)
						obj->render_type = RT_NONE;
				}
				else if (obj->type != OBJ_WEAPON)
					obj->render_type = RT_NONE;
				break;

			case OBJ_PLAYER:
				DTU_ANIM_STATE(obj) = DAS_STAND;
				DTU_ANIM_GOAL(obj) = DAS_STAND;
				DTU_ANIM_FRAME(obj) = 0;
				break;

			case OBJ_POWERUP:
				// verify_object: a powerup of a level file is deleted
				obj->flags |= OF_SHOULD_BE_DEAD;
				break;
		}
	}

	Level_setup_pass = 0;

	// 0xACCDB, the objects (the executable runs it after the setup of the
	// objects at the load)
	if (!Level_setup_restore) {
		// ASCENT: 0xA674A, called at 0xAD63B when the level has no saved
		// state (0x5A742 gives 0), before the level fixes: the script word
		// +0x1B0 of each object from 1 that is not a creature becomes 0. So
		// the stale script ids of the level files (the chairs, tables and
		// beds of YPINN, D-074) never run; the scenery scripts come from the
		// level fixes only. A saved state keeps the cleared words.
		if (!has_state)
			for (i=1;i<=Highest_object_index;i++)
				if (Objects[i].type != OBJ_ROBOT)
					Objects[i].dtu.m_1b0 = 0;
		level_fixes_objects(has_state);
		lava_textures_lookup();		// 0xAD744
		dtu_thief_level_textures();	// 0xAD744: the rest of 0xA6F89
	}

	// level_start 0xAD5BF: the creature setup by name and add_npc
	dtu_script_test_flags();
	if (!Level_setup_restore)
		level_start_npcs(1);
	else
		level_start_npcs(Setup_ext_level != Current_level_num || Setup_ext_count != N_robot_types);
	if (!Level_setup_restore) {
		memcpy(Setup_ext,Monster_ext,sizeof(Monster_ext[0]) * N_robot_types);
		Setup_ext_level = Current_level_num;
		Setup_ext_count = N_robot_types;
	}

	if (!Level_setup_restore)
		level_fixes_scenery_shields();

	if (Obj_log) {
		int n[MAX_OBJECT_TYPES];

		memset(n,0,sizeof(n));
		for (i=0;i<=Highest_object_index;i++)
			if (Objects[i].type < MAX_OBJECT_TYPES)
				n[Objects[i].type]++;
		dtu_obj_log("level %s (number %d): %d creatures, %d dressing, %d misc, %d armor, %d spell, %d weapon objects, brain %s",
			Gamesave_current_filename,Current_level_num,n[OBJ_ROBOT],n[OBJ_DRESSING],n[OBJ_MISC],n[OBJ_ARMOR],n[OBJ_SPELL],n[OBJ_WEAPON],
			Brain ? (Dtu_fix_brain ? "loaded, not used" : "used") : "none");
		for (i=0;i<=Highest_object_index;i++)
			if (Objects[i].type == OBJ_DRESSING) {
				polymodel *po = obj_model(&Objects[i]);
				int b = po && po->dtu_block && (po->box_flags & DTU_BOX_BODY) && po->body_radius;

				dtu_obj_log("obj %d %s: dressing %d shields %.1f pushable %d mass %.2f drag %.4f size %.2f seg %d pos %.2f %.2f %.2f body flags %d r %.2f hh %.2f",
					i,dtu_object_name(&Objects[i]),Objects[i].id,f2fl(Objects[i].shields),dressing_pushable(&Objects[i]),
					f2fl(Objects[i].mtype.phys_info.mass),f2fl(Objects[i].mtype.phys_info.drag),f2fl(Objects[i].size),
					Objects[i].segnum,f2fl(Objects[i].pos.x),f2fl(Objects[i].pos.y),f2fl(Objects[i].pos.z),
					po ? po->box_flags : -1,b ? f2fl(po->body_radius[0]) : 0.0,b ? f2fl(po->body_half_height[0]) : 0.0);
			}
			else if (Objects[i].type == OBJ_MISC || Objects[i].type == OBJ_ARMOR || Objects[i].type == OBJ_SPELL || Objects[i].type == OBJ_WEAPON)
				dtu_obj_log("obj %d %s: item type %d id %d seg %d pos %.2f %.2f %.2f render %d size %.2f life %.1f count %d flags 0x%x parent %d/%d",
					i,dtu_object_name(&Objects[i]),Objects[i].type,Objects[i].id,
					Objects[i].segnum,f2fl(Objects[i].pos.x),f2fl(Objects[i].pos.y),f2fl(Objects[i].pos.z),
					Objects[i].render_type,f2fl(Objects[i].size),f2fl(Objects[i].lifeleft),(signed char) Objects[i].dtu.m_1d5,
					Objects[i].flags,Objects[i].ctype.laser_info.parent_num,Objects[i].ctype.laser_info.parent_signature);
		for (i=0;i<=Highest_object_index;i++)
			if (Objects[i].type == OBJ_ROBOT)
				dtu_obj_log("obj %d %s: hp %d hostile %d behavior 0x%x attack %d weapon %d spell_level %d door %d seg %d contains %d/%d/%d script %d model %d size %.2f pos %.2f %.2f %.2f",
					i,dtu_object_name(&Objects[i]),f2i(Objects[i].shields),is_hostile(&Objects[i]),
					Objects[i].ctype.ai_info.behavior,Robot_info[Objects[i].id].attack_type,Monster_ext[Objects[i].id].weapon_type,
					Monster_ext[Objects[i].id].spell_level,Monster_ext[Objects[i].id].door_opening,Objects[i].segnum,
					Objects[i].contains_type,Objects[i].contains_id,Objects[i].contains_count,Objects[i].dtu.m_1b0 & DTU_SCRIPT_MASK,
					Objects[i].rtype.pobj_info.model_num,f2fl(Objects[i].size),
					f2fl(Objects[i].pos.x),f2fl(Objects[i].pos.y),f2fl(Objects[i].pos.z));
		for (i=0;i<=Highest_object_index;i++)
			if (Objects[i].type == OBJ_ROBOT) {
				polymodel *po = obj_model(&Objects[i]);
				int l = Dtu_anim_first[DAS_DIE+1] - 1;

				if (po && po->body_half_height && po->body_center && po->box1_min && l < po->n_frames)
					dtu_obj_log("body: obj %d %s model %d: rest hh %.2f center %.2f min %.2f r %.2f, dead hh %.2f center %.2f min %.2f max %.2f r %.2f, torso hh(last) %.2f r(0) %.2f, reach %.2f stop %.2f attack %.2f",
						i,dtu_object_name(&Objects[i]),Objects[i].rtype.pobj_info.model_num,
						f2fl(po->body_half_height[0]),f2fl(po->body_center[0].y),f2fl(po->box1_min[0].y),f2fl(po->body_radius[0]),
						f2fl(po->body_half_height[l+1]),f2fl(po->body_center[l+1].y),f2fl(po->box1_min[l+1].y),f2fl(po->box1_max[l+1].y),f2fl(po->body_radius[l+1]),
						po->torso_half_height ? f2fl(po->torso_half_height[l]) : -1.0,po->torso_radius ? f2fl(po->torso_radius[0]) : -1.0,
						f2fl(model_weapon_reach(&Objects[i])),f2fl(creature_stop_dist(&Objects[i])),f2fl(Objects[i].size + ConsoleObject->size + i2f(2)));
			}
	}
}

//------------------------------------------------------------------------
// Rendering

// The normal case of draw_polygon_object (0x676ED to 0x67A1C).
void dtu_draw_polygon_object(object *obj, fix light, fix *glow, bitmap_index *alt_textures)
{
	polymodel *po = obj_model(obj);
	int frame = DTU_ANIM_FRAME(obj);
	int show_hidden = 1;
	vms_vector draw_pos;

	if (po == NULL)
		return;

	// "The missile in the hand" (0x677EB to 0x67982): a submodel with the
	// flag 0x10 is not drawn from the fire frame of an attack to its end.
	// The test of the weapon table (Weapon_info + 0x0A is 3) is not in the
	// port: each creature with no weapon type counts.
	if (obj->type == OBJ_ROBOT && obj->id < N_robot_types && Monster_ext[obj->id].weapon_type < 0) {
		int s = DTU_ANIM_STATE(obj);

		if (s == DAS_ATTACK && frame >= po->fire_frames[0])
			show_hidden = 0;
		if (s == DAS_ATTACK2 && frame >= po->fire_frames[1])
			show_hidden = 0;
	}

	if (obj->type != OBJ_ROBOT && obj->type != OBJ_PLAYER)
		frame = -1;

	// Dtu_fix_corpse_floor: a body that lies lower than its position can
	// go (in the floor) is drawn lower.
	draw_pos = obj->pos;
	draw_pos.y += corpse_draw_drop(obj);

	// ASCENT: animation interpolation (Dtu_anim_interp). The frame of the
	// game rules stays; only the picture is between the frames. An active
	// creature counts its frames by the game time (0x11018), so the time of
	// one frame is known at the start of a state; the other animations
	// learn it from the frame changes.
	if (Dtu_anim_interp && frame >= 0) {
		static polyobj_anim_track track[MAX_OBJECTS];
		int objnum = obj-Objects, s = DTU_ANIM_STATE(obj), next = -1;
		fix frac = 0, est = 0;

		if (s >= 0 && s < DTU_N_ANIM_STATES) {
			int first = Dtu_anim_first[s], last = Dtu_anim_first[s+1];

			if (obj->type == OBJ_ROBOT && Ail[objnum].activity == 2 && last - first > 1 && state_time(po,s) > 0)
				est = state_time(po,s) / (last - first - 1);
			frac = polyobj_anim_track_update(&track[objnum],obj->signature,s,frame,first,last,est,
				(obj->type == OBJ_ROBOT && (Ail[objnum].flags & AILF_BACKWARD)) ? -1 : 1,GameTime,&next);
		}
		draw_polygon_model_frame_lerp(&draw_pos,&obj->orient,obj->rtype.pobj_info.model_num,frame,next,frac,show_hidden,light,glow,alt_textures);
		return;
	}

	draw_polygon_model_frame(&draw_pos,&obj->orient,obj->rtype.pobj_info.model_num,frame,show_hidden,light,glow,alt_textures);
}

// Render type 8 (0x67E2D): one bitmap of the object bitmaps as a rod that
// stands on the up vector of the object. Height and width are two times
// the size. The executable takes the light of the segment and not the
// light function of Descent 1 (0x67273).
void dtu_draw_item(object *obj)
{
	int slot = obj->dtu.item_bitmap;

	if (slot < 0 || slot >= MAX_OBJ_BITMAPS)
		return;
	draw_object_tmap_rod(obj,ObjBitmaps[ObjBitmapPtrs[slot]],1);
}

// Render type 7 of scenery (0x7C734, not read): the frames of the vclip
// by the game time. (guess)
void dtu_draw_dressing_vclip(object *obj)
{
	int n = obj->rtype.vclip_info.vclip_num;
	vclip *vc;
	int frame;

	if (n < 0 || n >= VCLIP_MAXNUM)
		return;
	vc = &Vclip[n];
	if (vc->num_frames <= 0 || vc->frame_time <= 0)
		return;
	frame = (GameTime / vc->frame_time) % vc->num_frames;
	draw_object_blob(obj,vc->frames[frame]);
}

//------------------------------------------------------------------------
// Sounds and effects

// 0x2E900 digi_link_sound_to_pos: the sound can be heard as far as 256.0,
// and the volume is 0.5 at most (0x2E967).
void sound_at(int sound,int segnum,vms_vector *pos,fix volume)
{
	if (sound < 0 || sound >= MAX_SOUNDS || segnum < 0 || segnum > Highest_segment_index)
		return;
	if (volume > F1_0/2)
		volume = F1_0/2;
	digi_link_sound_to_pos2(sound,segnum,0,pos,0,volume,i2f(256));
}

// 0x2E600 digi_link_sound_to_object: 200.0, volume 0.5 at most (0x2E639).
void sound_on_object(int sound,object *obj,fix volume)
{
	if (sound < 0 || sound >= MAX_SOUNDS)
		return;
	if (volume > F1_0/2)
		volume = F1_0/2;
	digi_link_sound_to_object2(sound,obj-Objects,0,volume,i2f(200));
}

// 0x3BD4C: a vclip that plays one time at a point.
void vclip_at(int segnum,vms_vector *pos,fix size,int vclip)
{
	if (vclip < 0 || vclip >= VCLIP_MAXNUM || size <= 0 || segnum < 0)
		return;
	if (Vclip[vclip].num_frames <= 0 || Vclip[vclip].play_time <= 0)
		return;
	object_create_explosion(segnum,pos,size,vclip);
}

// ASCENT: the splash of a step of the player on floor type 1 (water), code
// 0x434AB to 0x4351B of the footstep function 0x433E0: vclip 10 (the
// sparkles generic.abm) at the player with the size of the player, the
// position half the size lower and 0x100 times the side step added to x
// and to z, the life halved. The flags 0x40 and 0x1AE bit 2 (no
// collisions) and the call 0x6B9B0 (it does nothing for a fireball) are
// left out: a fireball does not move. side_step is the value after the
// change of its sign.
//
// The white flashes in water of the original come from this splash:
// - vclip 10 has vlighting=10.0 in all tables, the light of the large
//   explosions. The splash starts at 5.0 (half life) and lights the floor,
//   the walls and the arms near the player to full light for one second
//   at each step (0.625 s, 0.3125 s in a run).
// - x and z are world axes, so for half of the headings every second
//   splash is 2.8 units in front of the player, and the player walks into
//   the sprite (radius 4.7): white sparkles fill the bottom of the view.
// The port gives the splash no light (dtu_object_light, lighting.c) and
// puts it to the side of the walk, on the right vector of the player.
#define N_SPLASH_SIG	16
static int Splash_sig[N_SPLASH_SIG];
static int Splash_next;

void dtu_water_splash(object *obj,fix side_step)
{
	object *o;

	if (obj == NULL || obj->segnum < 0 || Vclip[10].num_frames <= 0 || Vclip[10].play_time <= 0)
		return;
	o = object_create_explosion(obj->segnum,&obj->pos,obj->size,10);
	if (o == NULL)
		return;
	o->pos.y -= obj->size >> 1;
	o->lifeleft >>= 1;
	// the length of the offset of the original: 0x100 * |side_step| * sqrt(2)
	vm_vec_scale_add2(&o->pos,&obj->orient.rvec,fixmul(side_step << 8,0x16A0A));
	Splash_sig[Splash_next] = o->signature;
	Splash_next = (Splash_next + 1) % N_SPLASH_SIG;
}

int dtu_is_water_splash(object *obj)
{
	int i;

	if (obj->type != OBJ_FIREBALL || obj->id != 10)
		return 0;
	for (i=0;i<N_SPLASH_SIG;i++)
		if (Splash_sig[i] == obj->signature && obj->signature != 0)
			return 1;
	return 0;
}

// A number of the line of a weapon ($WEAPON).
int weapon_int(int w,char *key,int def)
{
	if (w < 0 || w >= N_weapon_types)
		return def;
	return tbl_int(Weapon_item_info[w].record,key,def);
}

// Weapon_info +0x12 attack_type 1 (melee); 0 is projectile
int weapon_is_melee(int w)
{
	char value[32];

	if (w < 0 || w >= N_weapon_types || Weapon_item_info[w].record < 0)
		return 0;
	if (bm_tbl_get(Weapon_item_info[w].record,"attack_type",value,sizeof(value)))
		return stricmp(value,"projectile") != 0;
	return 1;
}

// 0x25C76: the blood of a creature that is hit, the vclip
// monster_hit_vclip of the monster table with the size 5.0. Nothing for
// the player.
void blood(object *obj)
{
	if (obj->type == OBJ_ROBOT)
		vclip_at(obj->segnum,&obj->pos,i2f(5),Monster_ext[obj->id].blood_vclip);
}

// A sound of the line of the player type (land_and_die_sound, 0x1D4E3B).
int player_type_sound(char *key)
{
	if (N_player_types <= 0)
		return -1;
	return tbl_int(Player_type_info[0].record,key,-1);
}

// The end of the DIE animation (0x6A3CC): the creature is a corpse. The
// executable puts it into the list at 0x2AEF54 and never takes it away
// (0x695E5 only holds it on the last frame of DIE). So the corpse stays.
fix creature_floor_dist(object *obj,fix radius,fix half_height);

// The test log of a corpse: the lowest point of the body of the last frame
// of DIE and the floor under the center, from a ray that starts above the
// body (a body that is in the floor has its center below the floor).
void log_corpse_height(object *obj,const char *what)
{
	int fate, seg, k;
	fvi_query fq;
	fvi_info hit;
	vms_vector p0, p1, mn, mx;
	fix bottom;

	// the lowest drawn point (Dtu_fix_corpse_floor: with the drop of the
	// drawing, see corpse_draw_drop())
	if (corpse_depth(obj,DTU_ANIM_FRAME(obj),&mn,&mx) < 0)
		return;
	bottom = obj->pos.y + corpse_draw_drop(obj) + mn.y;

	p0 = obj->pos;
	seg = obj->segnum;
	for (k=12;k>0;k-=2) {
		vms_vector p = obj->pos;
		int s;

		p.y += i2f(k);
		if ((s = find_point_seg(&p,obj->segnum)) >= 0 && ray_is_free(&p,s,&obj->pos,obj-Objects)) {
			p0 = p;
			seg = s;
			break;
		}
	}
	p1 = p0;
	p1.y -= i2f(60);
	fq.p0 = &p0;
	fq.p1 = &p1;
	fq.startseg = seg;
	fq.rad = 0;
	fq.thisobjnum = obj-Objects;
	fq.ignore_obj_list = NULL;
	fq.flags = 0;
	fate = find_vector_intersection(&fq,&hit);
	if (fate != HIT_WALL) {
		dtu_obj_log("%s: obj %d %s frame %d seg %d pos %.2f %.2f %.2f bottom %.2f no floor (fate %d)",what,(int)(obj-Objects),dtu_object_name(obj),DTU_ANIM_FRAME(obj),
			obj->segnum,f2fl(obj->pos.x),f2fl(obj->pos.y),f2fl(obj->pos.z),f2fl(bottom),fate);
		return;
	}
	dtu_obj_log("%s: obj %d %s frame %d seg %d pos %.2f %.2f %.2f bottom %.2f floor %.2f (seg %d side %d ny %.2f) height %.2f",
		what,(int)(obj-Objects),dtu_object_name(obj),DTU_ANIM_FRAME(obj),obj->segnum,f2fl(obj->pos.x),f2fl(obj->pos.y),f2fl(obj->pos.z),f2fl(bottom),
		f2fl(hit.hit_pnt.y),hit.hit_side_seg,hit.hit_side,f2fl(hit.hit_wallnorm.y),f2fl(bottom - hit.hit_pnt.y));
}

// ASCENT: Dtu_improved_pathfinding. The height of the floor under the
// point p of the body of obj: a ray of radius 0 straight down from a point
// above p. The center of the body, raised by the same height, must see that
// point, so that a floor behind a wall does not count. Returns 0 if no
// floor is found.
static int floor_under_point(object *obj,vms_vector *p,fix *floor_y)
{
	vms_vector top, c, p1;
	int k, seg = -1, fate;
	fvi_query fq;
	fvi_info hit;

	for (k=12;k>=0;k-=3) {
		int s, cs;

		top = *p;
		top.y = obj->pos.y + i2f(k);
		c = obj->pos;
		c.y = top.y;
		if ((cs = find_point_seg(&c,obj->segnum)) < 0 || (s = find_point_seg(&top,cs)) < 0)
			continue;
		if (s != cs && !ray_is_free(&c,cs,&top,obj-Objects))
			continue;
		seg = s;
		break;
	}
	if (seg < 0)
		return 0;

	p1 = top;
	p1.y -= i2f(200);
	fq.p0 = &top;
	fq.p1 = &p1;
	fq.startseg = seg;
	fq.rad = 0;
	fq.thisobjnum = obj-Objects;
	fq.ignore_obj_list = NULL;
	fq.flags = 0;
	fate = find_vector_intersection(&fq,&hit);
	if (fate != HIT_WALL)
		return 0;
	*floor_y = hit.hit_pnt.y;
	return 1;
}

// ASCENT: Dtu_fix_corpse_floor. The position of a creature is the center
// of the body box of its frame: the drawing takes body_center of the frame
// away (0x729C1). The box of the file is not the drawn body in DIE: for the
// Ghoul the last frame has box1_min.y -6.16 (the feet of the first frame
// of DIE), but the drawn points of that frame are -0.55 to 2.53 from the
// model origin, and body_center.y is -1.81. So the box put the corpse 7.4
// above the floor. The port takes the box of the drawn points of the frame
// (polyobj_frame_points_box()), cached for one frame of each model
// (Model_points).
#define DTU_CORPSE_LIFT	(F1_0/4)	// the lowest position above the floor

// The distance from the position down to the floor under a lying body:
// the depth of its lowest drawn point, but at least DTU_CORPSE_LIFT, so
// that the position stays in the segment above the floor. A body whose
// lowest point is higher than that is drawn lower, see corpse_draw_drop().
// mn and mx: the box of the drawn points, relative to the position.
// Returns -1 if the model has no points.
static fix corpse_depth(object *obj,int frame,vms_vector *mn,vms_vector *mx)
{
	int n = obj->rtype.pobj_info.model_num;
	polymodel *po = obj_model(obj);

	if (po == NULL || n < 0 || n >= MAX_POLYGON_MODELS)
		return -1;
	if (frame < -1 || frame >= po->n_frames)
		frame = po->n_frames - 1;
	if (Model_points[n].ok == 0 || Model_points[n].frame != frame) {
		Model_points[n].frame = frame;
		Model_points[n].ok = polyobj_frame_points_box(n,frame,&Model_points[n].mn,&Model_points[n].mx) ? 1 : 2;
	}
	if (Model_points[n].ok != 1)
		return -1;
	*mn = Model_points[n].mn;
	*mx = Model_points[n].mx;
	return (-mn->y > DTU_CORPSE_LIFT) ? -mn->y : DTU_CORPSE_LIFT;
}

// Dtu_fix_corpse_floor: the drawing of a dying or dead creature goes down
// by the part of its depth that the position cannot go (0 or negative).
static fix corpse_draw_drop(object *obj)
{
	vms_vector mn, mx;
	fix depth;

	if (!Dtu_fix_corpse_floor || obj->type != OBJ_ROBOT || DTU_ANIM_STATE(obj) != DAS_DIE)
		return 0;
	if ((depth = corpse_depth(obj,DTU_ANIM_FRAME(obj),&mn,&mx)) < 0)
		return 0;
	return -(depth + mn.y);
}

// ASCENT: Dtu_fix_corpse_floor. The corpse lies with its lowest drawn point
// on the floor. The floor is the highest floor under the center of the
// drawn body and under four points of it (at 0.6 of its half size), but
// not more than 3.0 above the floor under the position, so that a body on
// stairs lies on a step and is not drawn through one. Only y changes, and
// the new position must be in a segment, so the body goes into no wall.
// Returns 0 if no floor is found (the body stays).
int corpse_on_floor(object *obj)
{
	static const signed char sx[5] = { 0, -1, 1, -1, 1 }, sz[5] = { 0, -1, -1, 1, 1 };
	int i, seg;
	vms_vector mn, mx, q, ofs, p;
	vms_matrix m;
	fix base, best, f, depth;

	if ((depth = corpse_depth(obj,DTU_ANIM_FRAME(obj),&mn,&mx)) < 0)
		return 0;
	if (!floor_under_point(obj,&obj->pos,&base))
		return 0;
	best = base;

	vm_copy_transpose_matrix(&m,&obj->orient);
	for (i=0;i<5;i++) {
		q.x = (mn.x + mx.x)/2 + sx[i] * fixmul(mx.x - mn.x,F1_0*3/10);
		q.y = 0;
		q.z = (mn.z + mx.z)/2 + sz[i] * fixmul(mx.z - mn.z,F1_0*3/10);
		vm_vec_rotate(&ofs,&q,&m);
		p = obj->pos;
		p.x += ofs.x;
		p.z += ofs.z;
		if (floor_under_point(obj,&p,&f) && f > best && f - base < i2f(3))
			best = f;
	}

	p = obj->pos;
	p.y = best + depth;
	if ((seg = find_point_seg(&p,obj->segnum)) < 0)
		return 0;
	obj->pos = p;
	if (seg != obj->segnum)
		obj_relink(obj-Objects,seg);
	return 1;
}

// 0x10040: the special attack of a creature with a weapon_type below 0. It
// casts the spell with the number -weapon_type as a missile with the
// weapon entry 5 (0x56FF6). The next cast: 180.0 seconds later for the
// spell 16 (Fireball), else 0 to 10 seconds plus 4 seconds for each level
// of the spell.
static void creature_cast(object *obj,int spell,vms_vector *dir,vms_vector *fire_point)
{
	dtu_ai_local *ail = &Ail[obj-Objects];
	int seg;

	if (!is_hostile(obj) || ail->spell_time > GameTime || ail->activity != 2)
		return;
	if (spell < 0 || spell >= N_spell_types)
		return;
	seg = find_point_seg(fire_point,obj->segnum);
	if (seg < 0)
		return;
	dtu_obj_log("attack: obj %d %s casts spell %d '%s'",(int)(obj-Objects),dtu_object_name(obj),spell,Spell_info[spell].name);
	dtu_create_missile(dir,fire_point,seg,obj-Objects,5,spell,1);
	if (spell == 16)
		ail->spell_time = GameTime + i2f(180);
	else
		ail->spell_time = GameTime + (rand() % 11) * F1_0 + tbl_int(Spell_info[spell].record,"level",1) * 4 * F1_0;
}

// ASCENT: the believed position of a hidden player (audit C20, Ai_cloak_info
// 0x153A30: 8 entries of time and position, the entry objnum % 8 for a
// creature). compute_vis_and_vec (0x12BE8): when the player is hidden from
// the creature (0x165C4) or the creature has the status bit 4, the entry
// moves each 2.0 seconds by a random vector (flat, 0x127F3; in three
// directions, 0x127BF, when the player flies: effects 0x200800 or +0x1AE
// bit 0x80) times 8 times the time since its last move (16.0 for 2.0 s), and
// the creature uses it in place of the player (0x146CE): the distance, the
// turn, the walk and the attack. 0x15FBC (the awareness events of the types 2
// to 4, 0x16055) puts all entries at the player. The creature never sees the
// player: its sight is the ray to the believed position, and a path never
// goes to the segment of the player (the port's rule that keeps the
// invisibility of the player, audit C1).
static struct { fix last_time; vms_vector pos; } Ai_cloak[8];
static vms_vector *Ai_believed = NULL;		// the aim of ai_fire for a hidden player

static void cloak_reset(void)
{
	int i;

	if (ConsoleObject == NULL)
		return;
	for (i=0;i<8;i++) {
		Ai_cloak[i].pos = ConsoleObject->pos;		// 0x15FBC: Believed_player_pos
		Ai_cloak[i].last_time = GameTime;
	}
	dtu_obj_log("believed position: at the player %.1f %.1f %.1f (0x15FBC)",f2fl(ConsoleObject->pos.x),f2fl(ConsoleObject->pos.y),
		f2fl(ConsoleObject->pos.z));
}

// 1 if the creature uses the believed position (0x12C11 to 0x12C25)
static int cloak_applies(object *obj)
{
	return dtu_thief_hidden_from(obj) || (obj->dtu.m_88 & 4);
}

// 0x12C2B to 0x12CB9: the move of the entry of the creature
static vms_vector *cloak_position(object *obj)
{
	int k = (obj-Objects) % 8;
	fix elapsed = GameTime - Ai_cloak[k].last_time;

	if (elapsed < 0) {
		Ai_cloak[k].last_time = GameTime;
		elapsed = 0;
	}
	if (elapsed > 0x20000) {
		vms_vector r;

		Ai_cloak[k].last_time = GameTime;
		// (the bit 0x80 of the player +0x1AE has no place in the port)
		if (ConsoleObject->dtu.m_88 & 0x200800)
			make_random_vector(&r);
		else {
			r.x = (rand() - 0x4000) | 1;
			r.y = 0;
			r.z = rand() - 0x4000;
			vm_vec_normalize_quick(&r);
		}
		vm_vec_scale_add2(&Ai_cloak[k].pos,&r,elapsed << 3);
		dtu_obj_log("obj %d %s: believes the hidden player at %.1f %.1f %.1f",(int)(obj-Objects),dtu_object_name(obj),
			f2fl(Ai_cloak[k].pos.x),f2fl(Ai_cloak[k].pos.y),f2fl(Ai_cloak[k].pos.z));
	}
	return &Ai_cloak[k].pos;
}

void melee_hit(object *target,object *attacker);

// 0x11B06 ai_fire. "AI: critter fires."
static void ai_fire_body(object *obj,fix dist);

static void ai_fire(object *obj,fix dist)
{
	if (!Dtu_creatures_attack || !is_hostile(obj) || Robot_info[obj->id].attack_type == 1)
		return;
	ai_fire_body(obj,dist);
	// ASCENT: 0x11EB7, the end of ai_fire calls the alert function
	// 0x160AB(creature, 1)
	dtu_creature_alert(obj,1);
}

static void ai_fire_body(object *obj,fix dist)
{
	int objnum = obj-Objects;
	robot_info *ri = &Robot_info[obj->id];
	int w = Monster_ext[obj->id].weapon_type;
	object *target = ConsoleObject;
	vms_vector fire_point, aim, dir;
	fix reach, r1, h1, r2, h2;
	fvi_query fq;
	fvi_info hit;
	int fate;

	if (!Dtu_creatures_attack || !is_hostile(obj) || ri->attack_type == 1)
		return;

	dtu_get_body_size(obj,&r1,&h1);
	dtu_get_body_size(target,&r2,&h2);
	reach = r1 + r2 + i2f(2);

	// The fire point: 1.0 in front of the creature (0x11018, step 9)
	vm_vec_scale_add(&fire_point,&obj->pos,&obj->orient.fvec,F1_0);

	// The aim. A target that is slower than 1.0 in x and z: its position.
	// Else the position plus the velocity (x and z) times one half of the
	// time of the flight. The y part of the lead is not set in the
	// executable (0x11CC4). It is 0 here.
	aim = target->pos;
	// ASCENT: a hidden player: the believed position, with no lead (C20).
	// The touch below needs the real distance.
	if (Ai_believed) {
		aim = *Ai_believed;
		dist = vm_vec_dist(&obj->pos,&target->pos);
	}
	else if (w >= 0 && !(target->mtype.phys_info.velocity.x < F1_0 && target->mtype.phys_info.velocity.z < F1_0)) {
		fix speed = Weapon_info[w < N_weapon_types ? w : 0].speed[Difficulty_level];

		if (speed > 0) {
			fix t = fixdiv(dist,speed) / 2;

			aim.x += fixmul(target->mtype.phys_info.velocity.x,t);
			aim.z += fixmul(target->mtype.phys_info.velocity.z,t);
		}
	}
	vm_vec_sub(&dir,&aim,&fire_point);
	if (vm_vec_normalize(&dir) < 0x100)
		dir = obj->orient.fvec;

	if (w < 0) {
		if (dist <= reach) {
			dtu_obj_log("attack: obj %d %s touches the player",objnum,dtu_object_name(obj));
			melee_hit(target,obj);
		}
		else
			creature_cast(obj,-w,&dir,&fire_point);
		return;
	}

	if (obj->control_type == CT_MORPH || Dtu_player_dead || Player_is_dead)
		return;

	// A monster with a mage or cleric level does not shoot (0x11DE9). It
	// touches a target in reach. Its spells come from 0x92D99 and 0x91FDC,
	// which are not in the port.
	if (Monster_ext[obj->id].spell_level > 0) {
		if (dist <= reach)
			melee_hit(target,obj);
		return;
	}

	// Laser_create_new_easy (0x55B23): the way from the creature to the
	// fire point must be free.
	fq.p0 = &obj->pos;
	fq.p1 = &fire_point;
	fq.startseg = obj->segnum;
	fq.rad = 0;
	fq.thisobjnum = objnum;
	fq.ignore_obj_list = NULL;
	fq.flags = 0;
	fate = find_vector_intersection(&fq,&hit);
	if (fate != HIT_NONE || hit.hit_seg < 0) {
		dtu_obj_log("attack: obj %d %s cannot fire, the fire point is in a wall",objnum,dtu_object_name(obj));
		return;
	}

	dtu_obj_log("attack: obj %d %s fires weapon %d, frame %d",objnum,dtu_object_name(obj),w,DTU_ANIM_FRAME(obj));
	dtu_create_missile(&dir,&fire_point,hit.hit_seg,objnum,w,-1,1);
}

//------------------------------------------------------------------------
// ASCENT: Dtu_improved_pathfinding, the body of a creature at the player

// The reach of the melee weapon of a model: the largest distance from the
// center of the creature to the center of the player, straight in front of
// the creature, at which the weapon test of weapon_test() touches the
// cylinder of the player in a frame that can hit (frame 8 of ATTACK and
// ATTACK2 to the end of the state). weapon_test() moves the test position
// forward by the square root of 2, takes the weapon sphere of the frame
// (get_weapon_sphere()) and tests the segment from its center outward by
// its radius, with its radius, against the cylinder of the player. The
// test itself does not change.

static fix model_weapon_reach(object *obj)
{
	polymodel *po = obj_model(obj);
	int n = obj->rtype.pobj_info.model_num, s, f, k;
	fix best = -1;

	if (po == NULL || n < 0 || n >= MAX_POLYGON_MODELS)
		return 0;
	if (Model_reach[n])
		return Model_reach[n] > 0 ? Model_reach[n] : 0;

	if ((po->box_flags & DTU_BOX_BODY) && po->weapon_center && po->weapon_radius)
		for (s=DAS_ATTACK;s<=DAS_ATTACK2;s++)
			for (f=Dtu_anim_first[s]+8;f<Dtu_anim_first[s+1] && f<po->n_frames;f++) {
				vms_vector c = po->weapon_center[f], dir = c;
				fix wr = po->weapon_radius[f], hit_r = DTU_BODY_RAD + wr;

				vm_vec_normalize_quick(&dir);
				c.z += 0x16a0a;		// the test position of weapon_test()
				for (k=0;k<2;k++) {
					fix x = c.x + (k ? fixmul(dir.x,wr) : 0);
					fix z = c.z + (k ? fixmul(dir.z,wr) : 0);
					fix d;

					if (abs(x) >= hit_r)
						continue;
					d = z + fix_sqrt(fixmul(hit_r,hit_r) - fixmul(x,x));
					if (d > best)
						best = d;
				}
			}

	Model_reach[n] = (best > 0) ? best : -1;
	return (best > 0) ? best : 0;
}

// The distance in x and z from the player at which a creature that walks
// to the player stops: the reach of its weapon minus 1.0 (the creature
// turns to the player at a rate, and the player moves), but not more than
// the distance at which it starts an attack (choose_attack_goal()) minus
// 1.0, and not less than the bodies (torso cylinder and player cylinder).
// 0: no stop (a creature with a ranged attack or with no weapon box).
static fix creature_stop_dist(object *obj)
{
	fix reach, attack, r, hh;
	vms_vector c;

	if (Robot_info[obj->id].attack_type != 1)
		return 0;
	reach = model_weapon_reach(obj);
	if (reach <= 0)
		return 0;
	attack = obj->size + ConsoleObject->size + i2f(2);
	dtu_get_body_cylinder(obj,&c,&r,&hh);
	reach -= F1_0;
	if (reach > attack - F1_0)
		reach = attack - F1_0;
	if (reach < r + DTU_BODY_RAD)
		reach = r + DTU_BODY_RAD;
	return reach;
}

// A hostile creature that sees the player does not walk nearer to the
// player than creature_stop_dist(): the part of its velocity toward the player is cut
// so that it stops there. The test of a hit does not change, and the
// player can still walk up to the creature.
static void stop_at_player(object *obj)
{
	physics_info *pi = &obj->mtype.phys_info;
	vms_vector d;
	fix stop, dist, toward, allowed, r, hh;

	if (!is_hostile(obj) || ConsoleObject == NULL || Player_is_dead || Dtu_player_dead)
		return;
	// only a creature that sees the player (not one behind a door)
	if (!Ail[obj-Objects].previous_visibility)
		return;
	if ((stop = creature_stop_dist(obj)) <= 0)
		return;
	dtu_get_body_size(obj,&r,&hh);
	if (abs(obj->pos.y - ConsoleObject->pos.y) > hh + DTU_BODY_HALF_HEIGHT)
		return;

	vm_vec_sub(&d,&obj->pos,&ConsoleObject->pos);
	d.y = 0;
	dist = vm_vec_normalize(&d);
	if (dist < 0x100)
		return;
	toward = -vm_vec_dot(&pi->velocity,&d);
	if (toward <= 0 || dist - stop > fixmul(toward,FrameTime))
		return;
	allowed = (dist > stop && FrameTime > 0) ? fixdiv(dist - stop,FrameTime) : 0;
	if (toward > allowed)
		vm_vec_scale_add2(&pi->velocity,&d,toward - allowed);
}

//------------------------------------------------------------------------
// Movement of a creature (pmon.c, 0x9F153)

static int opens_doors(object *obj);

// 0x6C662 for a creature: gravity and the lift out of the floor.
static void creature_gravity(object *obj,vms_vector *accel,fix radius,fix half_height)
{
	dtu_ai_local *ail = &Ail[obj-Objects];
	fix d = creature_floor_dist(obj,radius,half_height);

	if (d < 0) {
		fix push = DTU_PUSH_UP_MAX, ad = -d;
		vms_vector new_pos;

		if (Dtu_fix_creature_move) {
			ail->push_acc += fixmuldiv(DTU_PUSH_UP_MAX,FrameTime,DTU_REF_FRAMETIME);
			if (ail->push_acc > DTU_PUSH_UP_MAX*4)
				ail->push_acc = DTU_PUSH_UP_MAX*4;
			push = ail->push_acc;
		}
		if (push > ad)
			push = ad;
		if (Dtu_fix_creature_move)
			ail->push_acc -= push;

		new_pos = obj->pos;
		new_pos.y += push;
		if (find_point_seg(&new_pos,obj->segnum) >= 0)
			obj->pos = new_pos;
		if (obj->mtype.phys_info.velocity.y < 0)
			obj->mtype.phys_info.velocity.y = 0;
	}
	else {
		ail->push_acc = 0;
		// Dtu_fix_corpse_floor: a creature that flies falls when it dies.
		// The executable has no gravity for it, so its body stays where it
		// died, in the air.
		if (d != 0 && (!is_flying(obj) || (Dtu_fix_corpse_floor && DTU_ANIM_STATE(obj) == DAS_DIE)))
			accel->y -= fixmul(d,DTU_GRAVITY_K);
	}
}

// 0x6CB68 for a creature, see dtu_integrate() in physics.c.
static void creature_integrate(object *obj,vms_vector *accel)
{
	physics_info *pi = &obj->mtype.phys_info;
	int count;

	if (Dtu_fix_creature_move) {
		// All steps of 1/64 second. The part of a step that is left goes
		// to the next frame of this object.
		fix *acc = &Ail[obj-Objects].step_acc;

		*acc += FrameTime;
		count = *acc / DTU_FT;
		*acc -= count * DTU_FT;
		if (count > 32)
			count = 32;
	}
	else {
		count = FrameTime / DTU_FT;
		if (count > 2)
			count = 2;
	}

	while (count--) {
		vm_vec_add2(&pi->velocity,accel);
		vm_vec_scale(&pi->velocity,F1_0-pi->drag);
	}

	if (pi->velocity.y < DTU_TERMINAL_VEL)
		pi->velocity.y = DTU_TERMINAL_VEL;
}

void dtu_creature_physics_sim(object *obj)
{
	int objnum = obj-Objects;
	dtu_ai_local *ail = &Ail[objnum];
	physics_info *pi = &obj->mtype.phys_info;
	vms_vector accel, start_pos, frame_vec, new_pos, center;
	fix radius, half_height, cyl_radius, cyl_half;
	int tries = 0, fate;
	int ignore_obj_list[MAX_IGNORE_OBJS], n_ignore_objs = 0;
	fvi_query fq;
	fvi_info hit;
	// Dtu_improved_pathfinding: a body that hits a wall slides along it for
	// the rest of the frame time (sim_time), as the ship of Descent 1 does.
	int imp = improved_paths(), hits = 0;
	fix sim_time = FrameTime, want = 0, moved = 0;

	if (obj->type != OBJ_ROBOT || obj->control_type != CT_AI)
		return;
	if (obj->dtu.m_88 & DTU_OF_HELD)
		return;
	// An inactive creature does not move (0x9F196).
	if (ail->activity == 0)
		return;
	if (obj->flags & (OF_EXPLODING|OF_SHOULD_BE_DEAD|OF_DESTROYED))
		return;
	// ASCENT: 0x9F174, object flag 0x40: no move
	if (dtu_creature_frozen(obj))
		return;

	// ASCENT: the move query of a rat (0x9F153) starts in the body of the
	// player when the player walks over the rat: the object test of the
	// query (0x3EE70) takes a start point in a body as a hit, so the rat
	// touches the player (0x9FB4C, collide_robot_and_player 0x26FC1). The
	// player walks through the rat, its own queries skip it (0x3F38F). The
	// move of the port does not take a start point in a body as a hit, so
	// this is the same test with the body of the player (dtu_player_body).
	if (ConsoleObject && ConsoleObject->type == OBJ_PLAYER && dtu_rat_type(obj->id) && DTU_ANIM_STATE(obj) != DAS_DIE) {
		dtu_body b;
		vms_vector d;
		fix across, r, hh;

		dtu_player_body(&b);
		dtu_get_body_size(obj,&r,&hh);
		vm_vec_sub(&d,&obj->pos,&ConsoleObject->pos);
		across = fix_sqrt(fixmul(d.x,d.x) + fixmul(d.z,d.z));
		if (across < b.move_rad + r && d.y < b.move_rad2 + hh && d.y > -(b.floor_height + hh)) {
			collide_two_objects(obj,ConsoleObject,&obj->pos);
			if (obj->flags & OF_SHOULD_BE_DEAD || DTU_ANIM_STATE(obj) == DAS_DIE)
				return;
		}
	}

	// 0x9F286: the velocity becomes one half on each frame, and the AI
	// adds direction * speed on each frame (0x11EC9). So the velocity goes
	// to the speed of the animation state: v' = (v + add) / 2. The
	// corrected rule does the same step for the reference frame time.
	if (Dtu_fix_creature_move) {
		// factor = 0.5 ^ (FrameTime / DTU_REF_FRAMETIME), from 16 steps
		static const fix half_pow[17] = { 0x10000, 0xf525, 0xeac0, 0xe0cc, 0xd744, 0xce24, 0xc567, 0xbd08,
			0xb504, 0xad58, 0xa5fe, 0x9ef5, 0x9837, 0x91c3, 0x8b95, 0x85aa, 0x8000 };
		fix t = fixdiv(FrameTime,DTU_REF_FRAMETIME);
		fix f = F1_0;

		while (t >= F1_0) {
			f >>= 1;
			t -= F1_0;
		}
		f = fixmul(f,half_pow[(t >> 12) & 15]);
		vm_vec_scale(&pi->velocity,f);
		if (ail->has_move)
			vm_vec_scale_add2(&pi->velocity,&ail->move_add,F1_0-f);
	}
	else {
		if (ail->has_move)
			vm_vec_add2(&pi->velocity,&ail->move_add);
		vm_vec_scale(&pi->velocity,F1_0/2);
	}
	ail->has_move = 0;

	// 0x6CFF8
	weapon_test(obj);
	if (obj->control_type != CT_AI)
		return;

	dtu_get_body_size(obj,&radius,&half_height);
	dtu_get_body_cylinder(obj,&center,&cyl_radius,&cyl_half);

	// A body that dies goes down with its box. (guess: the executable
	// calls the gravity function from the animation step of DIE, 0x112A5)
	if (DTU_ANIM_STATE(obj) == DAS_DIE) {
		polymodel *po = obj_model(obj);
		int frame = DTU_ANIM_FRAME(obj);

		if (po && po->body_half_height && frame >= 0 && frame < po->n_frames) {
			vms_vector mn, mx;
			fix depth;

			half_height = po->body_half_height[frame+1];
			// Dtu_fix_corpse_floor: the depth of the lowest drawn point of
			// the frame (see corpse_depth())
			if (Dtu_fix_corpse_floor && (depth = corpse_depth(obj,frame,&mn,&mx)) >= 0)
				half_height = depth;
		}
	}

	vm_vec_copy_scale(&accel,&pi->thrust,fixdiv(F1_0,pi->mass));

	creature_gravity(obj,&accel,cyl_radius,half_height);

	// A dying creature that does not fly falls: one eighth of the velocity
	// in x and z on each frame (0x9F4E2).
	if ((!is_flying(obj) || Dtu_fix_corpse_floor) && obj->ctype.ai_info.CURRENT_STATE == DAIS_DEAD)
		ail->falling = 1;
	if (ail->falling) {
		pi->velocity.x /= 8;
		pi->velocity.z /= 8;
	}

	creature_integrate(obj,&accel);

	if (!is_flying(obj))
		vm_vec_zero(&pi->thrust);

	if (imp && DTU_ANIM_STATE(obj) != DAS_DIE)
		stop_at_player(obj);

	start_pos = obj->pos;
	want = fixmul(vm_vec_mag(&pi->velocity),FrameTime);

	for (;;) {

		// 0x9F67C
		if (vm_vec_mag(&pi->velocity) <= 10) {
			ail->retry_count = 0;
			break;
		}

		frame_vec.x = fixmul(pi->velocity.x,sim_time);
		frame_vec.y = fixmul(pi->velocity.y,sim_time);
		frame_vec.z = fixmul(pi->velocity.z,sim_time);

		if (++tries > 5) {
			ail->retry_count = 1;
			break;
		}

		vm_vec_add(&new_pos,&obj->pos,&frame_vec);

		// 0x9F757: after more than 30 blocked frames the creature can walk
		// through its blocker if the step leads away from it.
		if (ail->blocked_count > 30 && ail->blocker >= 0 && n_ignore_objs < MAX_IGNORE_OBJS-1) {
			vms_vector np = new_pos;

			np.y = obj->pos.y;
			if (vm_vec_dist(&np,&Objects[ail->blocker].pos) > vm_vec_dist(&obj->pos,&Objects[ail->blocker].pos))
				ignore_obj_list[n_ignore_objs++] = ail->blocker;
			else
				ail->retry_count = 1;
		}

		ignore_obj_list[n_ignore_objs] = -1;

		// The radius of the query is one fourth of the radius of the body
		// (0x9F605).
		fq.p0 = &obj->pos;
		fq.p1 = &new_pos;
		fq.startseg = obj->segnum;
		fq.rad = cyl_radius/4;
		fq.thisobjnum = objnum;
		fq.ignore_obj_list = ignore_obj_list;
		fq.flags = (DTU_ANIM_STATE(obj) == DAS_DIE) ? 0 : FQ_CHECK_OBJS;

		Fvi_dtu_body = 1;
		Fvi_dtu_rad2 = half_height;
		// Dtu_improved_pathfinding: the top side is tested at the top of the
		// body (half height above the center). The executable tests it at
		// 1.5 times the half height, so a tall creature (an ogre, 28.7 from
		// the floor) cannot enter a corridor that its body fits in.
		if (imp)
			Fvi_dtu_rad2 = half_height*2/3;
		fate = find_vector_intersection(&fq,&hit);
		Fvi_dtu_body = 0;

		if (fate == HIT_WALL && hit.hit_side == WBOTTOM)
			fate = 5;		// the floor
		if (fate != HIT_NONE) {
			int k = (fate == 5) ? 2 : (fate == HIT_OBJECT) ? 3 : (fate == HIT_WALL && hit.hit_side == WTOP) ? 1 : 0;

			ail->log_hits[k]++;
			if (k == 0)
				ail->log_wall_time = GameTime;
			if (fate != HIT_OBJECT) {
				ail->log_hit_seg = hit.hit_side_seg;
				ail->log_hit_side = hit.hit_side;
			}
			else
				ail->log_hit_obj = hit.hit_object;
		}

		if (fate == HIT_NONE) {
			int segnum = find_point_seg(&new_pos,obj->segnum);

			if (segnum != -1) {
				moved += vm_vec_dist(&obj->pos,&new_pos);
				obj->pos = new_pos;
				if (segnum != obj->segnum)
					obj_relink(objnum,segnum);
			}
			ail->retry_count = 0;
			ail->blocked_count = 0;
			ail->blocker = -1;
			// a slide that did not get far: the AI may take another way
			if (imp && hits && moved*4 < want)
				ail->retry_count = 1;
			break;
		}
		else if (fate == 5) {				// 0x6CD45
			vms_vector c1;
			fix feet = obj->pos.y - half_height, diff;

			// (obj->pos is start_pos here, but after a slide)
			if (!imp)
				obj->pos = start_pos;

			// A floor that is less than 5.5 above the feet lifts the
			// creature (0x6C186, see dtu_knee_high() in physics.c).
			compute_center_point_on_side(&c1,&Segments[hit.hit_side_seg],hit.hit_side);
			diff = c1.y - feet;
			if (diff > 0 && diff < DTU_KNEE_HIGH)
				pi->velocity.y = c1.y - obj->pos.y + half_height + DTU_KNEE_LIFT;
			else {
				fix d = vm_vec_dot(&pi->velocity,&hit.hit_wallnorm);

				vm_vec_scale_add2(&pi->velocity,&hit.hit_wallnorm,-d);
			}
		}
		else if (fate == HIT_WALL && hit.hit_side == WTOP && !imp) {
			// 0x9F97A: a hit on a top side (or a bottom side, above) is
			// not a wall hit.
			obj->pos = start_pos;
			ail->retry_count = 0;
			break;
		}
		else if (fate == HIT_WALL) {		// 0x9E88D
			fix d = vm_vec_dot(&pi->velocity,&hit.hit_wallnorm);

			// collide_robot_and_wall (0x25CAA): a creature with the door
			// bit opens a door that it walks into. The door must be closed,
			// with no key and no lock. This is the only place where a
			// creature opens a door.
			if (opens_doors(obj) && hit.hit_side_seg >= 0 && hit.hit_side >= 0) {
				int wn = Segments[hit.hit_side_seg].sides[hit.hit_side].wall_num;

				if (wn >= 0 && Walls[wn].type == WALL_DOOR && Walls[wn].keys == KEY_NONE &&
					 Walls[wn].state == WALL_DOOR_CLOSED && !(Walls[wn].flags & WALL_DOOR_LOCKED)) {
					dtu_obj_log("obj %d %s: opens the door seg %d side %d (wall %d)",objnum,dtu_object_name(obj),
						hit.hit_side_seg,hit.hit_side,wn);
					wall_open_door(&Segments[hit.hit_side_seg],hit.hit_side);
				}
			}

			if (imp) {
				// Dtu_improved_pathfinding: the body goes to the point where
				// it touches the wall (or the ceiling), loses the part of the
				// velocity into the wall, and moves on with the rest of the
				// frame time. The executable takes 1.75 times that part away
				// and stops the body for the frame, so a body that walks into
				// the corner of an opening stays at the corner.
				fix len = vm_vec_mag(&frame_vec);
				int seg = find_point_seg(&hit.hit_pnt,obj->segnum);

				hits++;
				if (seg >= 0 && len > 0) {
					fix m = vm_vec_dist(&obj->pos,&hit.hit_pnt);

					if (m > len)
						m = len;
					moved += m;
					sim_time -= fixmul(sim_time,fixdiv(m,len));
					obj->pos = hit.hit_pnt;
					if (seg != obj->segnum)
						obj_relink(objnum,seg);
				}
				if (d < 0)
					vm_vec_scale_add2(&pi->velocity,&hit.hit_wallnorm,-d);
				if (sim_time <= 0)
					break;
				continue;
			}
			vm_vec_scale_add2(&pi->velocity,&hit.hit_wallnorm,fixmul(-d,DTU_WALL_BOUNCE));
			obj->pos = start_pos;
			ail->retry_count = 1;
			break;
		}
		else if (fate == HIT_OBJECT) {	// 0x9F9C0 and 0x9FB56
			object *other = &Objects[hit.hit_object];

			if (!imp)
				obj->pos = start_pos;
			ail->retry_count = ai_tick(objnum,2);

			if (ail->blocker != hit.hit_object) {
				ail->blocker = hit.hit_object;
				ail->blocked_count = 1;
			}
			else if (!Dtu_fix_creature_move || Ref_ticks_now > 0)
				ail->blocked_count++;

			// 0x9FA41: the creature pushes scenery when its strength is
			// above the value "pushable"
			if (other->type == OBJ_DRESSING)
				push_dressing(obj,other,Monster_info[obj->id].str);

			collide_two_objects(obj,other,&hit.hit_pnt);

			if (obj->flags & OF_SHOULD_BE_DEAD)
				break;
			if (other->type == OBJ_ROBOT && n_ignore_objs < MAX_IGNORE_OBJS-1) {
				ignore_obj_list[n_ignore_objs++] = hit.hit_object;
				continue;
			}
			// Dtu_improved_pathfinding: the body goes around scenery. It
			// loses the part of the velocity toward the center of the
			// scenery and tries again.
			if (imp && other->type == OBJ_DRESSING && !(other->flags & OF_SHOULD_BE_DEAD)) {
				vms_vector n;
				fix vn;

				vm_vec_sub(&n,&obj->pos,&other->pos);
				n.y = 0;
				if (vm_vec_normalize(&n) > 0x100 && (vn = vm_vec_dot(&pi->velocity,&n)) < 0) {
					hits++;
					vm_vec_scale_add2(&pi->velocity,&n,-vn);
					continue;
				}
			}
			// The body stops at the player and at scenery.
			pi->velocity.x = pi->velocity.z = 0;
			break;
		}
		else {									// "bad p0"
			vm_vec_negate(&pi->velocity);
			ail->retry_count = 1;
			break;
		}
	}

	if (get_seg_masks(&obj->pos,obj->segnum,0).centermask != 0) {
		if (!update_object_seg(obj)) {
			int n = find_point_seg(&start_pos,obj->segnum);

			if (n != -1) {
				obj->pos = start_pos;
				obj_relink(objnum,n);
			}
		}
	}

	// ASCENT: 0x9FCF1, the footstep of a walking creature (dtufoot.c)
	if (ail->activity == 2)
		dtufoot_creature(obj);
}

//------------------------------------------------------------------------
// The AI

// 0x102B8. The AI state that belongs to the animation state.
static int anim_to_ai_state(object *obj)
{
	switch (DTU_ANIM_STATE(obj)) {
		case DAS_REST:			return DAIS_REST;
		case DAS_WALK:
		case DAS_SIDLE_R:
		case DAS_SIDLE_L:
		case DAS_RUN:			return is_hostile(obj) ? DAIS_SRCH : DAIS_NONE;
		case DAS_ATTACK:
		case DAS_ATTACK2:		return DAIS_ATAK;
		case DAS_FLINCH:		return DAIS_FLIN;
		case DAS_DIE:			return DAIS_DEAD;
	}
	return DAIS_NONE;
}

// 0x10148. The animation state that belongs to an AI state.
static int ai_to_anim_state(object *obj,int ai_state,fix dist)
{
	dtu_ai_local *ail = &Ail[obj-Objects];
	int station = (obj->ctype.ai_info.behavior == DAIB_STATION);

	switch (ai_state) {
		case DAIS_SRCH:	return station ? DAS_STAND : DAS_WALK;
		case DAIS_LOCK:
			if (station)
				return DAS_STAND;
			if (dist > i2f(30) && ail->time_player_seen + F1_0/2 >= GameTime)
				return DAS_RUN;
			return DAS_WALK;
		case DAIS_FLIN:	return DAS_FLINCH;
		case DAIS_ATAK:	return (rand() & 0x100) ? DAS_ATTACK2 : DAS_ATTACK;
		case DAIS_DEAD:	return DAS_DIE;
	}
	return DAS_STAND;
}

// 0x106BA ai_turn_towards_vector. The creature keeps the y part of its
// forward vector.
static void turn_towards_vector(vms_vector *goal,object *obj,fix rate)
{
	vms_vector new_fvec;
	fix dot;

	new_fvec = *goal;
	new_fvec.y = obj->orient.fvec.y;

	dot = vm_vec_dot(&new_fvec,&obj->orient.fvec);

	if (dot < F1_0 - FrameTime/2 && rate > 0) {
		fix mag;

		vm_vec_scale(&new_fvec,fixdiv(FrameTime,rate));
		vm_vec_add2(&new_fvec,&obj->orient.fvec);
		new_fvec.y = obj->orient.fvec.y;
		mag = vm_vec_normalize(&new_fvec);
		if (mag < 0x100) {
			new_fvec = *goal;
			new_fvec.y = obj->orient.fvec.y;
		}
	}

	if (vm_vec_mag(&new_fvec) < 0x100)
		return;
	vm_vector_2_matrix(&obj->orient,&new_fvec,NULL,NULL);
	check_and_fix_matrix(&obj->orient);
}

// 0x11EC9. The AI adds direction * speed of the animation state to the
// velocity. A creature that does not fly moves in x and z only.
static void add_move(object *obj,vms_vector *dir)
{
	dtu_ai_local *ail = &Ail[obj-Objects];
	vms_vector v = *dir;

	if (!is_flying(obj))
		v.y = 0;
	vm_vec_normalize_quick(&v);
	vm_vec_copy_scale(&ail->move_add,&v,anim_speed(obj,-1));
	ail->has_move = 1;
}

// 0x11F32 move_towards_player. The creature turns to the direction at once.
static void move_towards(object *obj,vms_vector *vec_to_player)
{
	dtu_ai_local *ail = &Ail[obj-Objects];
	vms_vector v;

	if (obj->ctype.ai_info.CURRENT_STATE == DAIS_ATAK)
		if (!((Monster_info[obj->id].flags & MONSTER_F_FLYING) && Robot_info[obj->id].attack_type == 1))
			return;

	v.x = vec_to_player->x;
	v.y = obj->orient.fvec.y;
	v.z = vec_to_player->z;
	if (vm_vec_normalize(&v) > 0x100) {
		vm_vector_2_matrix(&obj->orient,&v,NULL,NULL);
		check_and_fix_matrix(&obj->orient);
	}
	ail->flags &= ~AILF_BACKWARD;
	add_move(obj,vec_to_player);
}

// 0x12547 move_away_from_player
static void move_away(object *obj,vms_vector *vec_to_player)
{
	dtu_ai_local *ail = &Ail[obj-Objects];
	vms_vector v;

	if (obj->ctype.ai_info.CURRENT_STATE == DAIS_ATAK || (ail->flags & AILF_AWAY))
		return;
	v = *vec_to_player;
	vm_vec_negate(&v);
	add_move(obj,&v);
}

// 0x125B9 ai_move_relative_to_player without the part that evades a
// weapon of the player.
static void move_relative_to_player(object *obj,fix dist,vms_vector *vec_to_player,fix circle_distance,int evade_only)
{
	dtu_ai_local *ail = &Ail[obj-Objects];

	if (obj->ctype.ai_info.behavior == DAIB_STATION || (obj->dtu.m_88 & DTU_OF_HELD))
		return;
	if (evade_only)
		return;

	if (dist < circle_distance || (ail->flags & AILF_AWAY)) {
		move_away(obj,vec_to_player);
		ail->flags |= AILF_AWAY;
	}
	else if (dist < circle_distance*2 || (ail->flags & AILF_AROUND)) {
		// 0x12000 move_around_player with fast_flag -1: the creature moves
		// across the line to the player and looks at the player, or walks
		// that way. The executable takes one of four directions from the
		// frame count. The port takes it from the game time: a new one each
		// 1.5 seconds. (the timing is a rule of the port)
		if (obj->ctype.ai_info.CURRENT_STATE != DAIS_ATAK) {
			int d = ((GameTime / (F1_0*3/2)) + (int)(obj-Objects)*13) & 3;
			vms_vector e, b;

			e.y = 0;
			switch (d) {
				case 0:	e.x = vec_to_player->z;		e.z = -vec_to_player->x;	break;
				case 1:	e.x = -vec_to_player->z;	e.z = vec_to_player->x;		break;
				case 2:	e.x = -vec_to_player->x;	e.z = vec_to_player->z;		break;
				default:	e.x = vec_to_player->x;		e.z = -vec_to_player->z;	break;
			}
			b.x = vec_to_player->x; b.y = 0; b.z = vec_to_player->z;
			if (vm_vec_normalize(&e) > 0x100 && vm_vec_normalize(&b) > 0x100) {
				fix dot = vm_vec_dot(&e,&b);
				int limit = (Robot_info[obj->id].attack_type == 1) ? -F1_0/2 : F1_0/2;		// 120 or 60 degrees

				if (dot > limit) {
					// in front: step to the side with the face to the player
					int s = (vm_vec_dot(&e,&obj->orient.rvec) < 0) ? DAS_SIDLE_L : DAS_SIDLE_R;

					if (DTU_ANIM_STATE(obj) != s)
						set_anim(obj,s);
					b.y = obj->orient.fvec.y;
					vm_vector_2_matrix(&obj->orient,&b,NULL,NULL);
				}
				else {
					if (DTU_ANIM_STATE(obj) != DAS_WALK)
						set_anim(obj,DAS_WALK);
					e.y = obj->orient.fvec.y;
					vm_vector_2_matrix(&obj->orient,&e,NULL,NULL);
				}
				check_and_fix_matrix(&obj->orient);
				e.y = 0;
				add_move(obj,&e);
			}
		}
		ail->flags |= AILF_AROUND;
	}
	else {
		move_towards(obj,vec_to_player);
		ail->flags = (ail->flags & ~(AILF_BACKWARD|AILF_TOWARD)) | AILF_TOWARD;
	}
}

//------------------------------------------------------------------------
// Paths (aipath.c of the executable, 0x1686D to 0x17FFF)

// 0x1320F ai_door_is_openable: a creature with the door bit can go through
// a door with no key and no lock. The state of the door is not tested.
// ASCENT: 1 while dtu_script_walk_start() makes the path of a creature
// with no door bit that walks for its script (Dtu_fix_script_walk)
static int Script_walk_doors;

// 1 if the creature can open a door: the door bit, or a walk of its script
static int opens_doors(object *obj)
{
	return Monster_ext[obj->id].door_opening || Script_walk_doors || Ail[obj-Objects].script_walk == 1;
}

static int door_is_openable(object *obj,segment *segp,int side)
{
	int wn = segp->sides[side].wall_num;

	if (wn < 0 || obj->type != OBJ_ROBOT || !opens_doors(obj))
		return 0;
	return Walls[wn].type == WALL_DOOR && Walls[wn].keys == KEY_NONE && !(Walls[wn].flags & WALL_DOOR_LOCKED);
}

// 0x132AA openable_doors_in_segment: the first side of the segment of the
// object with a closed door that has no key and no lock, or -1.
static int openable_door_in_segment(object *obj)
{
	int side;

	for (side=0;side<MAX_SIDES_PER_SEGMENT;side++) {
		int wn = Segments[obj->segnum].sides[side].wall_num;

		if (wn >= 0 && Walls[wn].type == WALL_DOOR && Walls[wn].keys == KEY_NONE &&
			 Walls[wn].state == WALL_DOOR_CLOSED && !(Walls[wn].flags & WALL_DOOR_LOCKED))
			return side;
	}
	return -1;
}

// 0x1686D create_path_points. A breadth first search from "start" to
// "end". end -2: the search ends at the depth max_depth. As in Descent 1
// the last segment of the queue is the end if the depth is reached or the
// queue is empty, so the caller must test the last segment of the path.
// New in the executable: a segment with special 1 or 2 is never on a
// path, and a creature with no door bit gets no path through a segment
// that has a wall on any side (but for the first and the last segment).
// Returns the number of segments in out[], 0 if there is no path.
static int find_path(object *obj,int start,int end,short *out,int max_out,int max_depth,int random_flag,int avoid_seg)
{
	static ubyte visited[MAX_SEGMENTS];
	static short parent[MAX_SEGMENTS], depth[MAX_SEGMENTS], queue[MAX_SEGMENTS];
	int head = 0, tail = 0, found = -1, i, n, cur;
	int has_door_bit = (obj->type != OBJ_ROBOT) || Monster_ext[obj->id].door_opening || Script_walk_doors;
	int walker = improved_paths() && obj->type == OBJ_ROBOT && !is_flying(obj);

	if (start < 0 || start > Highest_segment_index)
		return 0;

	// 0x168D9: a creature in the mode RUN_FROM takes a random way that
	// does not go through the segment of the player.
	if (obj->type == OBJ_ROBOT && Ail[obj-Objects].mode == DAIM_RUN_FROM) {
		random_flag = 1;
		avoid_seg = ConsoleObject->segnum;
	}
	if (max_depth < 0)
		max_depth = 30;

	memset(visited,0,Highest_segment_index+1);
	if (avoid_seg >= 0 && avoid_seg != start && avoid_seg != end)
		visited[avoid_seg] = 1;

	// 0x16999 to 0x16AEE. The loop of the executable leaves out the last
	// segment of the level.
	for (i=0;i<Highest_segment_index;i++) {
		if (i == start || i == end)
			continue;
		if (Segments[i].special == 1 || Segments[i].special == 2)
			visited[i] = 1;
		else if (!has_door_bit) {
			int side;

			for (side=0;side<MAX_SIDES_PER_SEGMENT;side++)
				if (Segments[i].sides[side].wall_num != -1)
					visited[i] = 1;
		}
	}

	queue[tail++] = start;
	visited[start] = 1;
	depth[start] = 0;
	parent[start] = -1;

	while (head < tail) {
		segment *segp;
		int k, first_side = random_flag ? rand() % MAX_SIDES_PER_SEGMENT : 0;

		cur = queue[head++];
		if (cur == end) {
			found = cur;
			break;
		}
		if (depth[cur] >= max_depth) {
			found = queue[tail-1];
			break;
		}
		segp = &Segments[cur];
		for (k=0;k<MAX_SIDES_PER_SEGMENT;k++) {
			int side = (first_side + k) % MAX_SIDES_PER_SEGMENT;
			int child = segp->children[side];

			if (child < 0 || visited[child])
				continue;
			// Dtu_improved_pathfinding: a creature that walks does not go
			// up through the top side of a segment (an opening in the
			// ceiling, side 1 in the levels of the game). The search of the
			// executable is the one of the ships of Descent 1.
			if (side == WTOP && walker)
				continue;
			if ((WALL_IS_DOORWAY(segp,side) & WID_FLY_FLAG) || door_is_openable(obj,segp,side)) {
				visited[child] = 1;
				parent[child] = cur;
				depth[child] = depth[cur] + 1;
				queue[tail++] = child;
			}
		}
	}
	if (found < 0) {
		if (tail < 2)
			return 0;
		found = queue[tail-1];
	}

	n = depth[found] + 1;
	while (n > max_out) {			// keep the first part
		found = parent[found];
		n--;
	}
	for (i=n-1,cur=found;i>=0;i--,cur=parent[cur])
		out[i] = cur;
	return n;
}

static void path_set(object *obj,int len,int flag)
{
	dtu_ai_local *ail = &Ail[obj-Objects];

	ail->path_len = len;
	ail->path_idx = 0;
	ail->path_dir = 1;
	if (flag != 2)
		ail->path_flag = flag;
	ail->path_tgt = 0;
	ail->prog_tgt = -1;
	ail->replans = 0;
	ail->stall_tgt = -1;
}

// 0x172DE create_n_segment_path
static void create_n_segment_path(object *obj,int n,int avoid_seg)
{
	dtu_ai_local *ail = &Ail[obj-Objects];
	int len = 0;

	while (n > 0 && (len = find_path(obj,obj->segnum,-2,ail->path,DTU_PATH_MAX,n,1,avoid_seg)) == 0) {
		n--;
		avoid_seg = -1;
	}
	path_set(obj,len,-1);
	ail->mode = DAIM_FOL_PATH;
}

// 0x16FD9 create_path: to the goal segment, 30 segments at most
static void create_path(object *obj)
{
	dtu_ai_local *ail = &Ail[obj-Objects];

	if (ail->goal_segment < 0) {
		create_n_segment_path(obj,3,-1);
		return;
	}
	path_set(obj,find_path(obj,obj->segnum,ail->goal_segment,ail->path,DTU_PATH_MAX,-1,0,-1),1);
}

// 0x170BC create_path_to_player
static void create_path_to_player(object *obj,int max_length)
{
	dtu_ai_local *ail = &Ail[obj-Objects];

	if (max_length < 0)
		max_length = 10;
	ail->time_player_seen = GameTime;
	ail->goal_segment = ConsoleObject->segnum;
	path_set(obj,find_path(obj,obj->segnum,ail->goal_segment,ail->path,DTU_PATH_MAX,max_length,1,-1),0);
	ail->mode = DAIM_FOL_PATH;
	ail->awareness = 0;
}

// 0x171F0 create_path_to_station: the way home
static void create_path_to_station(object *obj,int max_length)
{
	dtu_ai_local *ail = &Ail[obj-Objects];
	int home = obj->ctype.ai_info.hide_segment;

	if (home < 0 || home > Highest_segment_index)
		return;
	ail->time_player_seen = GameTime;
	ail->goal_segment = home;
	path_set(obj,find_path(obj,obj->segnum,home,ail->path,DTU_PATH_MAX,max_length,1,-1),2);
	ail->mode = DAIM_FOL_PATH;
	ail->awareness = 0;
}

// The point of entry k of a path. The executable has the centers of the
// segments. The port has the center of the side to the next segment, and
// the center of the last segment: a straight line from a point of a
// segment to the center of the next segment can leave the two segments.
static void path_point(dtu_ai_local *ail,int k,vms_vector *p)
{
	if (k < ail->path_len-1) {
		segment *segp = &Segments[ail->path[k]];
		int side;

		for (side=0;side<MAX_SIDES_PER_SEGMENT;side++)
			if (segp->children[side] == ail->path[k+1]) {
				compute_center_point_on_side(p,segp,side);
				return;
			}
	}
	compute_segment_center(p,&Segments[ail->path[k]]);
}

static void move_towards(object *obj,vms_vector *vec_to_player);
static void add_move(object *obj,vms_vector *dir);
static void turn_towards_vector(vms_vector *goal,object *obj,fix rate);

//------------------------------------------------------------------------
// ASCENT: Dtu_improved_pathfinding, paths for the body of a person.
//
// The path of the executable goes through the centers of the segments, and
// the port of Dtu_fix_path_follow through the centers of the sides between
// two segments. A creature that walks to the center of an opening from the
// side touches the corner of the opening with its body, and the move of
// the executable stops a body at a wall. Here each opening of the path has
// two points: one in front of the opening and one behind it, both on the
// line through the center of the opening that goes from the center of one
// segment to the center of the next, at the radius of the body from the
// opening. So the body goes through the opening straight. A creature goes
// to a later point at once when the line to it is free for its body
// (body_line_free()). When it does not come nearer to its point for 1.5
// seconds it goes back to the point in front of the opening, and after
// that it plans a new path that leaves out the segment behind the opening.
// The move of the body slides along a wall (dtu_creature_physics_sim()).

// The radius of the body for the paths: the torso cylinder of the model,
// 1.0 to 6.0.
static fix path_body_radius(object *obj)
{
	vms_vector c;
	fix r, hh;

	dtu_get_body_cylinder(obj,&c,&r,&hh);
	if (r < F1_0)
		r = F1_0;
	if (r > i2f(6))
		r = i2f(6);
	return r;
}

// 1 if a body of the radius of obj can go in a straight line from its
// center to p (walls only, objects do not count). The walls get the radius
// of the body, the floor one half of it, the ceiling nothing (the query of
// fvi.c with Fvi_dtu_body 1 adds 1.0 to the radius for the walls).
static int body_line_free(object *obj,vms_vector *p)
{
	fvi_query fq;
	fvi_info hit;
	fix r = path_body_radius(obj) - F1_0;
	int fate;

	if (r < 0x1000)
		r = 0x1000;
	int k;

	// The line to p, and the line at the height of the center of the body
	// (the body walks level, a line that goes up a stair can be free where
	// the body is not)
	for (k=0;k<2;k++) {
		vms_vector q = *p;

		if (k)
			q.y = obj->pos.y;
		fq.p0 = &obj->pos;
		fq.p1 = &q;
		fq.startseg = obj->segnum;
		fq.rad = r;
		fq.thisobjnum = obj-Objects;
		fq.ignore_obj_list = NULL;
		fq.flags = 0;
		Fvi_dtu_body = 1;
		Fvi_dtu_rad2 = 0;
		fate = find_vector_intersection(&fq,&hit);
		Fvi_dtu_body = 0;
		if (fate != HIT_NONE)
			return 0;
		if (is_flying(obj))
			break;
	}
	return 1;
}

// The point t of the path of obj: 2k is in front of the opening from
// path[k] to path[k+1], 2k+1 behind it, and 2(len-1) is the center of the
// last segment.
static void path_target(object *obj,int t,vms_vector *p)
{
	dtu_ai_local *ail = &Ail[obj-Objects];
	int k = t >> 1, side;
	segment *segp;
	vms_vector c, a, b, d;
	fix off, lim;

	if (k >= ail->path_len-1) {
		compute_segment_center(p,&Segments[ail->path[ail->path_len-1]]);
		return;
	}
	segp = &Segments[ail->path[k]];
	for (side=0;side<MAX_SIDES_PER_SEGMENT;side++)
		if (segp->children[side] == ail->path[k+1])
			break;
	if (side == MAX_SIDES_PER_SEGMENT) {
		compute_segment_center(p,segp);
		return;
	}
	compute_center_point_on_side(&c,segp,side);
	compute_segment_center(&a,segp);
	compute_segment_center(&b,&Segments[ail->path[k+1]]);
	vm_vec_sub(&d,&b,&a);
	d.y = 0;
	if (vm_vec_normalize(&d) < 0x100) {
		*p = c;
		return;
	}
	// the radius of the body plus 0.5, but not farther than 0.4 of the way
	// to the center of the segment
	off = path_body_radius(obj) + F1_0/2;
	vm_vec_sub(&a,(t & 1) ? &b : &a,&c);
	a.y = 0;
	lim = fixmul(vm_vec_mag(&a),F1_0*2/5);
	if (off > lim)
		off = lim;
	vm_vec_scale_add(p,&c,&d,(t & 1) ? off : -off);
}

static int find_path(object *obj,int start,int end,short *out,int max_out,int max_depth,int random_flag,int avoid_seg);

// path_advance() of Dtu_improved_pathfinding. Returns 0 at the end of the
// path, else 1 with the point to walk to in *goal.
static int path_advance_body(object *obj,vms_vector *goal)
{
	dtu_ai_local *ail = &Ail[obj-Objects];
	int last = 2*(ail->path_len-1), t = ail->path_tgt, k, j;
	fix threshold = fixmul(vm_vec_mag_quick(&obj->mtype.phys_info.velocity),FrameTime)*2 + F1_0 + F1_0/2;
	vms_vector v;
	fix dist;

	if (ail->path_len < 2)
		return 0;
	if (t < 0)
		t = 0;

	// A creature that is in a later segment of the path is through the
	// openings before it (it still walks to the point behind the last one).
	for (k=ail->path_len-1;k>0 && 2*k-1>t;k--)
		if (ail->path[k] == obj->segnum) {
			t = 2*k-1;
			break;
		}

	// the points that the creature has reached
	for (;;) {
		if (t > last)
			return 0;
		path_target(obj,t,goal);
		vm_vec_sub(&v,goal,&obj->pos);
		if (!is_flying(obj))
			v.y = 0;
		if (vm_vec_mag(&v) >= threshold)
			break;
		t++;
		if (t > ail->stall_tgt)
			ail->replans = 0;
	}

	// A later point that the body can walk to in a straight line: the
	// creature takes the short way (three points at most, on a reference
	// tick).
	if (Ref_ticks_now > 0)
		for (j=(t+3 < last) ? t+3 : last;j>t;j--) {
			vms_vector p;

			path_target(obj,j,&p);
			if (body_line_free(obj,&p)) {
				t = j;
				*goal = p;
				break;
			}
		}

	// The progress to the point. A creature that does not get 1.0 nearer
	// in 1.5 seconds while it walks is blocked.
	vm_vec_sub(&v,goal,&obj->pos);
	if (!is_flying(obj))
		v.y = 0;
	dist = vm_vec_mag(&v);
	if (t != ail->prog_tgt || anim_speed(obj,-1) == 0 || obj->ctype.ai_info.CURRENT_STATE == DAIS_ATAK) {
		ail->prog_tgt = t;
		ail->prog_time = GameTime;
		ail->prog_dist = dist;
	}
	else if (dist < ail->prog_dist - F1_0) {
		ail->prog_dist = dist;
		ail->prog_time = GameTime;
	}
	else if (GameTime - ail->prog_time > F1_0*3/2) {
		int goal_seg = ail->path[ail->path_len-1], avoid = -1, len;

		ail->replans++;
		if ((t & 1) && ail->replans == 1) {
			// back to the point in front of the opening
			ail->stall_tgt = t;
			t--;
			dtu_obj_log("path: obj %d %s: blocked at the opening from %d to %d, goes back in front of it",
				(int)(obj-Objects),dtu_object_name(obj),ail->path[t>>1],ail->path[(t>>1)+1]);
		}
		else if (ail->replans > 3) {
			// no way for the body: the AI goes on without the path
			dtu_obj_log("path: obj %d %s: blocked, gives up the path",(int)(obj-Objects),dtu_object_name(obj));
			ail->replans = 0;
			ail->stall_tgt = -1;
			ail->path_len = 0;
			ail->path_tgt = 0;
			return 0;
		}
		else {
			// a new path, without the segment behind the opening
			if ((t >> 1) + 1 < ail->path_len && ail->path[(t >> 1) + 1] != goal_seg)
				avoid = ail->path[(t >> 1) + 1];
			len = find_path(obj,obj->segnum,goal_seg,ail->path,DTU_PATH_MAX,-1,0,avoid);
			if (len < 2 || ail->path[len-1] != goal_seg)
				len = find_path(obj,obj->segnum,goal_seg,ail->path,DTU_PATH_MAX,-1,0,-1);
			dtu_obj_log("path: obj %d %s: blocked, new path of %d segments from %d to %d without %d",
				(int)(obj-Objects),dtu_object_name(obj),len,obj->segnum,goal_seg,avoid);
			ail->path_len = len;
			ail->path_dir = 1;
			ail->stall_tgt = 2*DTU_PATH_MAX;
			t = 0;
			if (len < 2) {
				ail->path_tgt = 0;
				return 0;
			}
		}
		ail->prog_tgt = -1;
		path_target(obj,t,goal);
	}

	ail->path_tgt = t;
	ail->path_idx = t >> 1;
	return 1;
}

// 1 if the corrected rule of Dtu_fix_path_follow holds for this creature:
// a creature that is not hostile and does not flee keeps the rule of the
// executable (it turns to the player), so that the people of a level stay
// where they are.
static int path_fix_applies(object *obj)
{
	dtu_ai_local *ail = &Ail[obj-Objects];

	if (!Dtu_fix_path_follow)
		return 0;
	// ASCENT: a creature on a path of statement E5 of its script walks to
	// the points of the path. With the rule of the executable it walks to
	// the player and does not come to the end of the path, where its script
	// goes on (the blocks CB E7 of the scripts 21 and 60).
	if (ail->script_walk == 1)
		return 1;
	return is_hostile(obj) || ail->mode == DAIM_RUN_FROM || obj->ctype.ai_info.behavior == DAIB_PATROL;
}

// 0x17CA8 ai_path_set_orient_and_vel. A creature that does not fly walks
// to the player and not to the point (0x17CFC). The corrected rule: it
// walks to the point.
static void path_steer(object *obj,vms_vector *goal)
{
	dtu_ai_local *ail = &Ail[obj-Objects];
	robot_info *ri = &Robot_info[obj->id];
	physics_info *pi = &obj->mtype.phys_info;
	vms_vector to_goal, vel;

	if (DTU_ANIM_STATE(obj) == DAS_DIE)
		return;

	if (!(Monster_info[obj->id].flags & MONSTER_F_FLYING)) {
		if (path_fix_applies(obj))
			vm_vec_sub(&to_goal,goal,&obj->pos);
		else
			vm_vec_sub(&to_goal,&ConsoleObject->pos,&obj->pos);
		to_goal.y = 0;
		if (vm_vec_normalize(&to_goal) > 0x100)
			move_towards(obj,&to_goal);
		return;
	}

	vm_vec_sub(&to_goal,goal,&obj->pos);
	vm_vec_normalize_quick(&to_goal);
	vel = pi->velocity;
	vm_vec_normalize_quick(&vel);
	if (vm_vec_dot(&to_goal,&obj->orient.fvec) < -15*F1_0/16)
		vel = to_goal;
	else
		vm_vec_scale_add2(&vel,&to_goal,F1_0/2);
	vm_vec_normalize_quick(&vel);
	add_move(obj,&vel);
	turn_towards_vector(&to_goal,obj,(ail->mode == DAIM_RUN_FROM) ? ri->firing_wait[4]/2 : ri->firing_wait[Difficulty_level]);
}

// Moves the index of the path to the point that the creature walks to
// next. Returns 0 at the end of the path.
static int path_advance(object *obj,vms_vector *goal)
{
	dtu_ai_local *ail = &Ail[obj-Objects];
	fix threshold = fixmul(vm_vec_mag_quick(&obj->mtype.phys_info.velocity),FrameTime)*2 + i2f(2);
	int tries = 0, k;

	// Dtu_improved_pathfinding: the points of the body (not on the way back
	// of a patrol)
	if (improved_paths() && ail->path_dir > 0 && path_fix_applies(obj))
		return path_advance_body(obj,goal);

	// A creature that is in a later segment of the path is past the
	// points before it. (port)
	if (ail->path_dir > 0)
		for (k=ail->path_len-1;k>ail->path_idx;k--)
			if (ail->path[k] == obj->segnum) {
				ail->path_idx = k;
				break;
			}

	while (tries++ <= DTU_PATH_MAX) {
		vms_vector v;

		if (ail->path_idx < 0 || ail->path_idx >= ail->path_len)
			return 0;
		path_point(ail,ail->path_idx,goal);
		vm_vec_sub(&v,goal,&obj->pos);
		// The executable takes the distance in three dimensions. A
		// creature that walks is never as near as that to the center of a
		// high segment.
		if (!is_flying(obj))
			v.y = 0;
		if (vm_vec_mag(&v) >= threshold)
			return 1;
		ail->path_idx += ail->path_dir;
	}
	return 0;
}

// 0x17505 ai_follow_path
static void follow_path(object *obj,int visible,fix dist_to_player)
{
	dtu_ai_local *ail = &Ail[obj-Objects];
	ai_static *aip = &obj->ctype.ai_info;
	int A = Monster_info[obj->id].ai_values[0][Difficulty_level];
	int afraid = (ail->mode == DAIM_RUN_FROM || A >= ail->morale);
	int pseg = ConsoleObject->segnum, beh = aip->behavior, k;
	vms_vector goal;


	if (ail->path_len == 0) {
		if (afraid) {
			create_n_segment_path(obj,5,-1);
			ail->mode = DAIM_RUN_FROM;
		}
		else
			create_path(obj);
	}

	if (ail->path_len < 2) {
		if (ail->mode == DAIM_RUN_FROM) {
			create_n_segment_path(obj,7,(obj->segnum == pseg) ? -1 : pseg);
			ail->mode = DAIM_RUN_FROM;
		}
		else
			ail->mode = DAIM_STILL;
		return;
	}

	if (ail->path_flag == 1 && (beh == DAIB_GUARD || beh == DAIB_HOME || (beh == DAIB_STATION && !visible)))
		return;

	// 0x17734: a creature that is farther than 200.0 from the player does
	// not move. The executable still counts its path index up (0x17411).
	// ASCENT: a creature on a path of statement E5 of its script
	// (Dtu_fix_script_walk) walks on, so that it comes to the end of the
	// path where its script goes on.
	if (dist_to_player > i2f(200) && ail->script_walk != 1) {
		if (ail->mode == DAIM_HOME)
			ail->mode = DAIM_STILL;
		return;
	}

	if (ail->mode == DAIM_RUN_FROM) {
		if (!visible && ail->awareness == 0) {
			fix f = F1_0 - FrameTime/2;

			vm_vec_scale(&obj->mtype.phys_info.velocity,f < F1_0/2 ? F1_0/2 : f);
			return;
		}
		for (k=ail->path_idx;k<ail->path_len;k++)
			if (k >= 0 && ail->path[k] == pseg) {
				create_n_segment_path(obj,7,(obj->segnum == pseg) ? -1 : pseg);
				ail->mode = DAIM_RUN_FROM;
				break;
			}
		if (visible) {
			ail->awareness = 1;
			ail->awareness_time = F1_0;
			morale_check(obj);			// 0x178AE
		}
		if (ail->path_len < 2)
			return;
	}

	if (ail->path_idx < 0)
		ail->path_idx = 0;

	if (!path_advance(obj,&goal)) {
		// ASCENT: the end of a path of statement E5 of a script (0x174C3
		// in the path index function 0x17411: the creature turns back on
		// the path and, with script flag 0, the script runs in mode 2).
		// The port stops the creature here; dtuscript.c runs the script.
		if (ail->script_walk == 1) {
			ail->script_walk = 2;
			ail->mode = DAIM_STILL;
			vm_vec_zero(&obj->mtype.phys_info.velocity);
			aip->GOAL_STATE = aip->CURRENT_STATE = DAIS_NONE;
			set_anim(obj,DAS_STAND);
			dtu_obj_log("obj %d %s: at the end of the path of its script (seg %d)",(int)(obj-Objects),dtu_object_name(obj),obj->segnum);
			return;
		}
		// 0x179C0: the end of the path
		if (ail->mode == DAIM_HOME) {
			ail->mode = DAIM_STILL;
			return;
		}
		if (beh == DAIB_GUARD || beh == DAIB_HOME || (beh == DAIB_STATION && !visible)) {
			create_path_to_station(obj,15);
			if (ail->path_len == 0 || ail->path[ail->path_len-1] != aip->hide_segment)
				ail->mode = DAIM_STILL;
			return;
		}
		if (ail->mode == DAIM_FOL_PATH && beh != DAIB_PATROL) {
			create_path_to_player(obj,10);
			return;
		}
		if (afraid) {
			create_n_segment_path(obj,7,(obj->segnum == pseg) ? -1 : pseg);
			ail->mode = DAIM_RUN_FROM;
			return;
		}
		// PATROL: back along the path. ASCENT: an empty path (cleared by
		// the step code) has no point to go back to; the index -1 read
		// path[-1], before the array.
		if (ail->path_len <= 0)
			return;
		ail->path_dir = -ail->path_dir;
		ail->path_idx += 2*ail->path_dir;
		if (ail->path_idx < 0) ail->path_idx = 0;
		if (ail->path_idx >= ail->path_len) ail->path_idx = ail->path_len-1;
		path_point(ail,ail->path_idx,&goal);
	}

	path_steer(obj,&goal);
}

// The corrected rule of Dtu_fix_path_follow for the mode CHASE_OB: a
// creature that does not see the player walks along a path to the segment
// of the player. The executable walks along the straight line and stops
// at the wall. Returns 1 if the creature has a path and moves on it.
static int chase_by_path(object *obj)
{
	int objnum = obj-Objects;
	dtu_ai_local *ail = &Ail[objnum];
	int pseg = ConsoleObject->segnum, need, k, len;
	vms_vector goal;

	if (obj->segnum == pseg) {
		ail->path_len = 0;
		return 0;
	}

	need = (ail->path_len < 2 || ail->path[ail->path_len-1] != pseg || ail->path_dir != 1);
	if (!need) {
		need = 1;
		for (k=0;k<ail->path_len;k++)
			if (ail->path[k] == obj->segnum)
				need = 0;
	}
	if (need) {
		if (ail->path_fail_time && GameTime >= ail->path_fail_time && GameTime - ail->path_fail_time < F1_0)
			return 0;
		len = find_path(obj,obj->segnum,pseg,ail->path,DTU_PATH_MAX,-1,0,-1);
		if (len < 2 || ail->path[len-1] != pseg) {
			dtu_obj_log("path: obj %d %s: no way from %d to the player in %d",objnum,dtu_object_name(obj),obj->segnum,pseg);
			ail->path_len = 0;
			ail->path_fail_time = GameTime ? GameTime : 1;
			return 0;
		}
		path_set(obj,len,0);
		ail->path_fail_time = 0;
		dtu_obj_log("path: obj %d %s: %d segments from %d to the player in %d",objnum,dtu_object_name(obj),len,obj->segnum,pseg);
	}

	if (!path_advance(obj,&goal))
		return 0;
	path_steer(obj,&goal);
	return 1;
}

// 0x123EF: the creature runs away from the player. If it moved less than
// 3.0 in three calls it walks to the side for 27 calls. The executable
// has one counter for all creatures and counts calls. The port has one
// for each creature, and with Dtu_fix_creature_move it counts reference
// frame times.
static void flee_from_player(object *obj,vms_vector *vec_to_player)
{
	int objnum = obj-Objects;
	dtu_ai_local *ail = &Ail[objnum];
	static int shared_counter;
	static vms_vector shared_pos, shared_dir;
	int *counter = Dtu_fix_creature_move ? &ail->flee_counter : &shared_counter;
	vms_vector *pos = Dtu_fix_creature_move ? &ail->flee_pos : &shared_pos;
	vms_vector *dir = Dtu_fix_creature_move ? &ail->flee_dir : &shared_dir;
	int steps = Dtu_fix_creature_move ? Ref_ticks_now : 1;

	if (steps == 0 && vm_vec_mag_quick(dir) < 0x100) {
		*dir = *vec_to_player;
		vm_vec_negate(dir);
	}

	while (steps-- > 0) {
		if (*counter >= 30) {
			*pos = obj->pos;
			*counter = 0;
			DTU_ANIM_STATE(obj) = DAS_RUN;
		}
		if (*counter < 3) {
			if (vm_vec_dist(pos,&obj->pos) < i2f(3))
				(*counter)++;
			else {
				*pos = obj->pos;
				*counter = 0;
			}
			*dir = *vec_to_player;
			vm_vec_negate(dir);
		}
		else if (*counter == 3) {
			ail->retry_count = 1;
			if (is_flying(obj))
				make_random_vector(dir);
			else {
				*dir = obj->orient.rvec;
				if (rand() & 1)
					vm_vec_negate(dir);
				DTU_ANIM_STATE(obj) = DAS_WALK;
			}
			(*counter)++;
		}
		else
			(*counter)++;
	}

	turn_towards_vector(dir,obj,Robot_info[obj->id].firing_wait[Difficulty_level]);
	add_move(obj,dir);
}

// The scenery that does not break. The executable ignores the keyword
// cant_break (0x229FC) and has the bit 0x04 of Dressing_info+0x04 only
// for a few objects of some levels (0xAD3AF). 1 (default): cant_break
// works. 0 (-origobj): every scenery object can break.
int Dtu_fix_cant_break = 1;

int dtu_dressing_unbreakable(object *d)
{
	if (d->type != OBJ_DRESSING || d->id >= N_dressing_types)
		return 1;
	if (Dressing_info[d->id].flags & DRESSING_F_LEVEL_FIX)
		return 1;
	return Dtu_fix_cant_break && (Dressing_info[d->id].flags & DRESSING_F_CANT_BREAK);
}

//------------------------------------------------------------------------
// The end of the game (0xA6786, called by 0xA6ACB for object 3 of level
// 0x20, the Flamesword). Verified in the disassembly 0xA6786 to 0xA6ACB:
// 1. 0x40974 and 0x34B26: the full view and the hands of the interface
//    (the port draws them each frame). 0x417ED and 0x40E50: the player
//    stops and its controls and keys are cleared. 0xD74C3 and 0x6B088:
//    the orientation of the player is made again from its forward vector.
// 2. A light object (0x68AC8: type 13, id 0, the segment and the place of
//    the sword, control type 14 CT_LIGHT): lifeleft 0x3FFFFFFF, the
//    intensity (+0x140) 0, +0x1AE bit 2, object flags 0xC0. "DLOG:
//    couldn't make light!!!" when the object list is full.
// 3. While the spin rate y of the sword (+0x100, 0x147 from the level
//    fixes) is not above 0x2666, one game frame 0x435A6(1, 0) for each
//    step: the view bob byte 0x14530D = 0, the velocity z of the player =
//    9.0, the spin rate y + 0x41, the intensity of the light + 3.0 (the
//    word +0x142), spin_object (0x69777) when the sword spins (movement
//    type 3, so it turns twice in a frame). 147 steps: light 441.0, spin
//    0x269A.
// 4. The explode_sound of the sword type (Dressing_info +0x0A) linked to
//    the sword, forever, volume 1.0 (0x2E600 makes it 0.5). Render type
//    of the sword 0 (it is gone), object flag 0x40.
// 5. 50 steps, one game frame each: an explosion of vclip 0 at the sword
//    (0x3BD4C), size 1.0 to 10.0 (rand() % 10 + 1), its +0x1AE bit 2 and
//    flag 0x40; x and z += rand() % 11.0 - 3.0, y += rand() % 0.6875 -
//    0.6875 (rand() is at most 0x7FFF, so the modulo does nothing and the
//    explosions are 3.0 to 2.5 to the side and 0.69 to 0.19 lower, kept as
//    in dtu_break_dressing); sound 0xA8D + rand() % 5 (2701 to 2705)
//    linked to the explosion at volume rand() % 1.0 (0 to 0.5).
// 6. With no digital sound (0x2EF31) the function returns: no movie and
//    the game goes on. Else stop_time, the streams stop, the sound of
//    object 3 stops (0x2E816(3)), frames while the music or the sound
//    stream plays and while keys come, CREDITV1.MVE (0x96439), Function_
//    mode 6 (the front end), start_time.
// The port steps 3 and 5 once in each reference tick (DTU_REF_FRAMETIME,
// 3/64 s, the corrected rules of the AI), so the sequence takes 9.2 s at
// each frame rate. The player keeps the controls of a game frame, as in
// 1.3 (0x435A6 reads them). The port plays the movie also with no sound.
// The test log lines: "end sequence: ..." (tools/questtest.py).

static int End_seq_step = 0;		// 0 none, 1 the spin, 2 the explosions, 3 done
static int End_seq_light = -1, End_seq_light_sig;
static int End_seq_n, End_seq_sounds;
static fix End_seq_acc;

int spin_object(object *obj);		// object.c

static void end_sequence_reset(void)
{
	End_seq_step = 0;
	End_seq_light = -1;
}

static void end_sequence_start(object *sword)
{
	object *p = ConsoleObject;
	vms_vector f;
	int n;

	if (End_seq_step)
		return;
	End_seq_step = 1;
	End_seq_n = End_seq_sounds = 0;
	End_seq_acc = 0;
	if (p) {
		vm_vec_zero(&p->mtype.phys_info.velocity);			// 0x417ED
		f = p->orient.fvec;
		vm_vector_2_matrix(&p->orient,&f,NULL,NULL);		// 0xD74C3
		check_and_fix_matrix(&p->orient);					// 0x6B088
	}
	game_flush_inputs();										// 0x40E50
	n = obj_create(OBJ_LIGHT,0,sword->segnum,&sword->pos,NULL,0,CT_LIGHT,MT_NONE,RT_NONE);
	End_seq_light = n;
	if (n < 0)
		printf("end sequence: DLOG: couldn't make light!!!\n");
	else {
		object *l = &Objects[n];

		l->lifeleft = 0x3FFFFFFF;
		l->ctype.light_info.intensity = 0;
		l->flags |= OF_DTU_NOHIT;
		End_seq_light_sig = l->signature;
	}
	printf("end sequence: start (0xA6786), sword in seg %d, light obj %d, spin 0x%x\n",sword->segnum,n,sword->mtype.spin_rate.y);
	dtu_obj_log("end sequence (0xA6786): light obj %d",n);
}

static object *end_sequence_light(void)
{
	if (End_seq_light < 0 || Objects[End_seq_light].type != OBJ_LIGHT || Objects[End_seq_light].signature != End_seq_light_sig)
		return NULL;
	return &Objects[End_seq_light];
}

// Each game frame while the sequence runs
static void end_sequence_frame(void)
{
	object *s = &Objects[3], *p = ConsoleObject, *l = end_sequence_light();

	if (End_seq_step == 0 || End_seq_step == 3)
		return;
	End_seq_acc += FrameTime;
	if (End_seq_step == 1) {
		if (p)
			p->mtype.phys_info.velocity.z = i2f(9);
		if (s->movement_type == MT_SPINNING)
			spin_object(s);									// 0x69777
		while (End_seq_acc >= DTU_REF_FRAMETIME && End_seq_step == 1) {
			End_seq_acc -= DTU_REF_FRAMETIME;
			if (s->mtype.spin_rate.y > 0x2666) {
				dressing_info *di = (s->id < N_dressing_types) ? &Dressing_info[s->id] : NULL;

				printf("end sequence: %d steps, light %.1f, spin 0x%x\n",End_seq_n,l ? f2fl(l->ctype.light_info.intensity) : 0.0,
					s->mtype.spin_rate.y);
				printf("end sequence: sound %d on the sword\n",di ? di->explode_sound : -1);
				if (di && di->explode_sound >= 0 && di->explode_sound < MAX_SOUNDS)
					digi_link_sound_to_object2(di->explode_sound,3,1,F1_0/2,i2f(200));		// 0x2E600(sound, 3, 1, 1.0)
				s->render_type = RT_NONE;
				s->flags |= OF_DTU_NOHIT;
				End_seq_step = 2;
				End_seq_n = 0;
				break;
			}
			s->mtype.spin_rate.y += 0x41;
			if (l)
				l->ctype.light_info.intensity += i2f(3);
			End_seq_n++;
		}
	}
	while (End_seq_step == 2 && End_seq_acc >= DTU_REF_FRAMETIME) {
		int e = -1, size = i2f(rand() % 10 + 1), snd;
		fix vol;

		End_seq_acc -= DTU_REF_FRAMETIME;
		if (Vclip[0].num_frames > 0 && Vclip[0].play_time > 0 && s->segnum >= 0) {
			object *x = object_create_explosion(s->segnum,&s->pos,size,0);

			if (x) {
				e = x - Objects;
				x->flags |= OF_DTU_NOHIT;
				x->pos.x += (rand() % i2f(11)) - i2f(3);
				x->pos.y += (rand() % 0xB000) - 0xB000;
				x->pos.z += (rand() % i2f(11)) - i2f(3);
			}
		}
		vol = rand() % F1_0;
		snd = 0xA8D + rand() % 5;
		if (e >= 0 && snd < MAX_SOUNDS) {
			digi_link_sound_to_object2(snd,e,0,vol > F1_0/2 ? F1_0/2 : vol,i2f(200));
			End_seq_sounds++;
		}
		if (++End_seq_n >= 50) {
			printf("end sequence: %d explosions, %d sounds\n",End_seq_n,End_seq_sounds);
			digi_kill_sound_linked_to_object(3);					// 0x2E816(3)
			End_seq_step = 3;
			dtumovie_end_credits();
		}
	}
}

// 0xA6ACB (explode 0) for scenery: explode_sound at volume 1.0, nine
// explosions of break_vclip (default 137) at the size of the object, the
// scenery of the segment falls (movement type 1), then 0x6A3CC removes the
// object and object_create_egg drops what it contains. The nine places of
// the explosions: each one is made at the center of the object, then its
// x and z += rand() % 11.0 - 5.0, y += rand() % 0.6875 - 0.3125 (0xA6C90 to
// 0xA6CD6). rand() is at most 0x7FFF, so the modulo does nothing and all
// nine are 5.0 to 4.5 to one side and -0.31 to 0.19 up and down (D-035).
// ASCENT: Dtu_fix_break_spread (default) scales rand() to the range, so
// the places are -5.0 to 6.0 on x and z, as the constants say. -origobj
// gives the 1.3 places. explode 1: 0x911AA with 0x3CA7E: the explosion
// vclip 2 at 2.5 times the size and explode_sound (no debris in the port).
int Dtu_fix_break_spread = 1;

static fix break_offset(fix range,fix low)
{
	int r = rand() & 0x7FFF;		// the rand() of Watcom C (0xD7AC4)

	if (Dtu_fix_break_spread)
		return (fix) (((long long) r * range) >> 15) - low;
	return (r % range) - low;
}

void dtu_break_dressing(object *d,object *by,int explode)
{
	dressing_info *di;
	vms_vector p;
	int i, vclip;

	if (d->type != OBJ_DRESSING || d->id >= N_dressing_types || (d->flags & (OF_SHOULD_BE_DEAD|OF_DESTROYED)))
		return;
	// ASCENT: 0xA6AF8: in level 0x20 (FLAME) object 3, the Flamesword, ends
	// the game (0xA6786): the light, the spin, the 50 explosions with their
	// sounds, then the credits movie and the front end (end_sequence_frame).
	if (!explode && Current_level_num - 1 == 0x20 && d - Objects == 3) {
		end_sequence_start(d);
		return;
	}
	di = &Dressing_info[d->id];
	if (!explode && dtu_dressing_unbreakable(d))
		return;
	dtu_obj_log("obj %d %s breaks obj %d %s (%s)",by ? (int)(by-Objects) : -1,by ? dtu_object_name(by) : "nothing",(int)(d-Objects),
		dtu_object_name(d),explode ? "0x3CA7E" : "0xA6ACB");
	if (explode) {
		dtu_creature_drop_contents(d);
		vclip_at(d->segnum,&d->pos,fixmul(d->size,0x28000),2);
		sound_at(di->explode_sound,d->segnum,&d->pos,F1_0);
	}
	else {
		if (di->explode_sound >= 0)
			sound_at(di->explode_sound,d->segnum,&d->pos,F1_0);
		vclip = (di->break_vclip >= 0) ? di->break_vclip : 137;
		// ASCENT: each explosion at the center plus its own offset, as
		// 0xA6C7E moves the new object (the port added up the offsets)
		for (i=0;i<9;i++) {
			p = d->pos;
			p.x += break_offset(i2f(11),i2f(5));
			p.z += break_offset(i2f(11),i2f(5));
			p.y += break_offset(0xB000,0x5000);
			vclip_at(d->segnum,&p,d->size,vclip);
			dtu_obj_log("break explosion: offset %.2f %.2f %.2f",f2fl(p.x-d->pos.x),f2fl(p.y-d->pos.y),f2fl(p.z-d->pos.z));
		}
		// the other scenery of the segment falls (guess for the meaning)
		for (i=0;i<=Highest_object_index;i++)
			if (Objects[i].type == OBJ_DRESSING && Objects[i].segnum == d->segnum && &Objects[i] != d)
				Dressing_fall[i] = 1;
	}
	d->flags |= OF_SHOULD_BE_DEAD;
	if (!explode) {
		dtu_creature_drop_contents(d);
		dtu_fire_trap(d);		// 0xA6D5A: level 0 object 0x29 (dtucombat.c)
	}
}

static void move_relative_to_player(object *obj,fix dist,vms_vector *vec_to_player,fix circle_distance,int evade_only);
static int ai_to_anim_state(object *obj,int ai_state,fix dist);

// The blocked move of do_ai_frame (0x14BA1). Returns 1: the mode switch
// does not run in this frame.
// Dtu_improved_pathfinding: a hostile creature on a path of the body
// (path_advance_body()) slides along the wall and plans again by itself.
// The blocked move would make a new path (which starts the path again) or
// send it to a random side. Scenery in the way is still attacked.
static int body_path_handles_block(object *obj)
{
	dtu_ai_local *ail = &Ail[obj-Objects];

	if (!improved_paths() || !path_fix_applies(obj) || ail->path_len < 2 || ail->path_dir <= 0)
		return 0;
	if (ail->mode != DAIM_CHASE_OB && ail->mode != DAIM_FOL_PATH)
		return 0;
	if (ail->blocker >= 0 && Objects[ail->blocker].type == OBJ_DRESSING && ail->blocked_count >= 5)
		return 0;
	return 1;
}

static int blocked_move(object *obj,fix dist,vms_vector *vec_to_player,int visible)
{
	int objnum = obj-Objects;
	dtu_ai_local *ail = &Ail[objnum];
	ai_static *aip = &obj->ctype.ai_info;
	robot_info *ri = &Robot_info[obj->id];
	physics_info *pi = &obj->mtype.phys_info;
	object *blocker = (ail->blocker < 0) ? ConsoleObject : &Objects[ail->blocker];
	fix rate = ri->firing_wait[Difficulty_level];
	vms_vector vec;
	int have_vec = 0, r, left;

	ail->retry_count = 0;

	// Scenery in the way for 5 blocked frames: the creature attacks it,
	// and after 10 it breaks.
	if (blocker->type == OBJ_DRESSING && ail->blocked_count >= 5) {
		vm_vec_sub(&vec,&blocker->pos,&obj->pos);
		vm_vec_normalize_quick(&vec);
		turn_towards_vector(&vec,obj,rate);
		set_anim(obj,ai_tick(objnum,2) ? DAS_ATTACK2 : DAS_ATTACK);
		ail->anim_start = GameTime;
		if (blocker->id < N_dressing_types && !(Dressing_info[blocker->id].flags & DRESSING_F_CANT_BREAK) && ail->blocked_count >= 10) {
			dtu_break_dressing(blocker,obj,0);
			aip->GOAL_STATE = DAIS_SRCH;
			ail->blocker = -1;
			ail->blocked_count = 0;
		}
		move_relative_to_player(obj,dist,vec_to_player,ri->turn_time[Difficulty_level],0);
		return 1;
	}

	if (ail->mode == DAIM_STILL)
		return 1;
	if (ail->mode == DAIM_RUN_FROM) {
		flee_from_player(obj,vec_to_player);
		return 1;
	}
	if (aip->behavior == DAIB_STATION)
		return 1;

	if (!visible || ail->activity == 0) {
		// "AI: %s looking for player!"
		ail->goal_segment = ConsoleObject->segnum;
		create_path(obj);
		ail->mode = DAIM_FOL_PATH;
		dtu_obj_log("obj %d %s: blocked, looks for the player, path of %d segments",objnum,dtu_object_name(obj),ail->path_len);
		return 1;
	}

	// The player is on the left side of the creature
	left = vm_vec_dot(vec_to_player,&obj->orient.rvec) < 0;

	r = Dtu_fix_ai_ticks ? rand() % 100 : (int)(((uint)Dtu_clock + (uint)GameTime + objnum) % 100);
	switch (r) {
		case 0:		// 0x14E8D: a new orientation with the right vector in front (guess: a turn of 90 degrees)
			vec = obj->orient.rvec;
			vm_vector_2_matrix(&obj->orient,&vec,NULL,NULL);
			check_and_fix_matrix(&obj->orient);
			DTU_ANIM_STATE(obj) = DAS_WALK;
			break;
		case 1:		// a random direction
			make_random_vector(&vec);
			if (!is_flying(obj))
				vec.y = 0;
			vm_vec_normalize_quick(&vec);
			turn_towards_vector(&vec,obj,rate);
			DTU_ANIM_STATE(obj) = DAS_WALK;
			have_vec = 1;
			break;
		case 2:
			move_relative_to_player(obj,dist,vec_to_player,ri->turn_time[Difficulty_level]*2,0);
			break;
		case 3:		// a turn to the side of the player
			if (is_flying(obj))
				make_random_vector(&vec);
			else {
				vec = obj->orient.rvec;
				if (left)
					vm_vec_negate(&vec);
				vec.y = 0;
			}
			DTU_ANIM_STATE(obj) = DAS_WALK;
			turn_towards_vector(&vec,obj,rate);
			have_vec = 1;
			break;
		case 4:
			aip->GOAL_STATE = DAIS_SRCH;
			DTU_ANIM_GOAL(obj) = ai_to_anim_state(obj,aip->CURRENT_STATE,dist);
			ail->anim_start = GameTime;
			break;
		default:		// 5 to 99: the step to the side
			if (is_flying(obj))
				make_random_vector(&vec);
			else {
				vec = obj->orient.rvec;
				DTU_ANIM_STATE(obj) = DAS_SIDLE_R;
				if (left) {
					vm_vec_negate(&vec);
					DTU_ANIM_STATE(obj) = DAS_SIDLE_L;
				}
				vec.y = 0;
			}
			have_vec = 1;
			break;
	}

	// 0x15030: the velocity is the vector times the speed of the
	// animation state (set, not added). The executable does this for the
	// cases 0, 2 and 4 too, with a vector that it did not set.
	if (have_vec) {
		vm_vec_normalize_quick(&vec);
		vm_vec_copy_scale(&pi->velocity,&vec,anim_speed(obj,-1));
		ail->move_add = pi->velocity;
		ail->has_move = 1;
		if (!is_flying(obj))
			pi->thrust = pi->velocity;
	}
	return 1;
}

// The sound of a creature that sees the player, see Dtu_fix_attack_sound
// (compute_vis_and_vec 0x12E2F to 0x12F3C). Call it before the time of the
// last sight is set for this frame.
static void creature_sight_sound(object *obj,int visible,vms_vector *vec_to_player)
{
	dtu_ai_local *ail = &Ail[obj-Objects];
	robot_info *ri = &Robot_info[obj->id];
	int in_fov;

	if (!Dtu_fix_attack_sound || !is_hostile(obj) || Dtu_player_dead)
		return;
	in_fov = visible && vm_vec_dot(&obj->orient.fvec,vec_to_player) > ri->field_of_view[Difficulty_level];

	if (in_fov && !ail->prev_fov) {
		if (!ail->previous_visibility) {
			if (ail->time_player_seen + F1_0/2 < GameTime) {
				ail->time_attack_sound = GameTime;
				ail->next_misc_sound = GameTime + F1_0 + rand()*4;
			}
		}
		else if (ail->time_attack_sound + F1_0/4 < GameTime) {
			sound_at(ri->attack_sound,obj->segnum,&obj->pos,F1_0);
			ail->time_attack_sound = GameTime;
		}
	}
	if (in_fov && ail->next_misc_sound < GameTime) {
		ail->next_misc_sound = GameTime + (7 - Difficulty_level) * (rand() + F1_0) / 2;
		sound_at(ri->attack_sound,obj->segnum,&obj->pos,F1_0);
		dtu_obj_log("sound: obj %d %s attack sound %d",(int)(obj-Objects),dtu_object_name(obj),ri->attack_sound);
	}
	ail->prev_fov = in_fov;
}

// 0x10763: the "random turn". In 9 of 100 calls the creature turns to the
// player if it saw the player before. The rest of the function changes a
// rotation velocity that is 0 and stays 0.
static void random_turn(object *obj,vms_vector *vec_to_player)
{
	dtu_ai_local *ail = &Ail[obj-Objects];

	if (ail->previous_visibility && rand() > 0x7400)
		turn_towards_vector(vec_to_player,obj,Robot_info[obj->id].firing_wait[Difficulty_level]);
}

// 0x10ADF. The morale check. The parts with values that the port does not
// have (the level of the creature 0xAE3BB, the level of the player, the
// function 0x359F4) count as 0.
static void morale_check(object *obj)
{
	dtu_ai_local *ail = &Ail[obj-Objects];
	ai_static *aip = &obj->ctype.ai_info;
	monster_info *mi = &Monster_info[obj->id];
	int A = mi->ai_values[0][Difficulty_level], B = mi->ai_values[1][Difficulty_level];
	int mor, roll;

	if (obj->type != OBJ_ROBOT || obj->control_type != CT_AI || mi->max_hit_points <= 0)
		return;
	if (aip->GOAL_STATE == DAIS_DEAD || aip->CURRENT_STATE == DAIS_DEAD || DTU_ANIM_STATE(obj) == DAS_DIE)
		return;

	if (mi->flags & MONSTER_F_UNDEAD)
		mor = (obj->dtu.m_1b0 & DTU_FLEEING) ? 0 : 20;
	else {
		int wound;

		mor = (300 - mi->iq) / 8;
		wound = 4 - (int)(((int64_t)obj->shields * 4) / i2f(mi->max_hit_points));
		if (wound < 0) wound = 0;
		if (wound > 4) wound = 4;
		mor -= wound;
		if (aip->behavior != DAIB_WANDER && aip->behavior != DAIB_HOME)
			mor += 3;
		mor += Monster_ext[obj->id].spell_level;
		mor += 2 * ail->awareness;
		mor += (Player_armor_class - mi->armor_class) / 2;
		mor += Current_level_num / 2;
	}

	roll = rand() % 19 + 2;

	if (roll <= mor) {
		obj->dtu.m_1b0 &= ~DTU_FLEEING;
		if (is_hostile(obj) || ail->target > 0) {
			ail->morale = B;
			ail->mode = DAIM_CHASE_OB;
			aip->GOAL_STATE = DAIS_SRCH;
			obj->dtu.m_1b0 |= DTU_HOSTILE;
		}
		else {
			ail->morale = (A + B) / 2;
			aip->GOAL_STATE = DAIS_NONE;
		}
	}
	else {
		ail->morale = A;
		ail->mode = DAIM_RUN_FROM;
		obj->dtu.m_1b0 &= ~DTU_HOSTILE;
		obj->dtu.m_1b0 |= DTU_FLEEING;
		DTU_ANIM_STATE(obj) = DAS_RUN;
	}

	dtu_obj_log("morale: obj %d %s: number %d roll %d, %s",(int)(obj-Objects),dtu_object_name(obj),mor,roll,
		roll <= mor ? "passed" : "failed");
}

// The choice of step 25 point 4 of do_ai_frame (docs/re/ai.md): the goal
// state of a creature that wants to attack.
static void choose_attack_goal(object *obj,fix dist)
{
	ai_static *aip = &obj->ctype.ai_info;

	if (Robot_info[obj->id].attack_type == 1) {
		fix reach = obj->size + ConsoleObject->size + i2f(2);

		aip->GOAL_STATE = (dist <= reach) ? DAIS_ATAK : DAIS_LOCK;
	}
	else if (Monster_ext[obj->id].spell_level == 0)
		aip->GOAL_STATE = DAIS_ATAK;
}

// 0x11018. The animation of an active creature, computed from the game
// time. Returns 1 if the creature is dead and done.
static void ai_animation_step(object *obj,fix dist)
{
	dtu_ai_local *ail = &Ail[obj-Objects];
	ai_static *aip = &obj->ctype.ai_info;
	polymodel *po = obj_model(obj);
	fix elapsed = GameTime - ail->anim_start;
	int s, first, last, n, old_frame, ended;
	fix st;

	if (po == NULL)
		return;
	if (elapsed < 0)
		elapsed = 0;

	// A hostile melee creature that is far from a player that it sees runs.
	if (is_hostile(obj) && aip->GOAL_STATE != DAIS_DEAD && DTU_ANIM_STATE(obj) != DAS_DIE &&
		 Robot_info[obj->id].attack_type == 1 && dist > i2f(30) && ail->time_player_seen + F1_0/2 >= GameTime) {
		if (aip->behavior != DAIB_STATION)
			DTU_ANIM_STATE(obj) = DAS_RUN;
		aip->GOAL_STATE = DAIS_LOCK;
	}

	old_frame = DTU_ANIM_FRAME(obj);
	(void)old_frame;
	s = DTU_ANIM_STATE(obj);
	first = Dtu_anim_first[s];
	last = Dtu_anim_first[s+1];
	n = last - first - 1;
	st = state_time(po,s);

	// The executable divides by the state time with no test. A state with
	// the time 0 (REST) or with no frames ends at once here.
	if (st > 0 && n >= 0) {
		int step = (int)((((int64_t)elapsed * ((int64_t)n << 16)) / st + 0x8000) >> 16);

		if (ail->flags & AILF_BACKWARD)
			DTU_ANIM_FRAME(obj) = last - 1 - step;
		else
			DTU_ANIM_FRAME(obj) = first + step;

		// 0x110F0: a ranged creature that attacks does not jump over the
		// fire frame of the state.
		if (Robot_info[obj->id].attack_type != 1 && aip->CURRENT_STATE == DAIS_ATAK && (s == DAS_ATTACK || s == DAS_ATTACK2)) {
			int ff = po->fire_frames[s - DAS_ATTACK];

			if (old_frame < ff && DTU_ANIM_FRAME(obj) > ff && old_frame >= first)
				DTU_ANIM_FRAME(obj) = ff;
		}
	}
	else
		DTU_ANIM_FRAME(obj) = (ail->flags & AILF_BACKWARD) ? first - 1 : last;

	if (ail->flags & AILF_BACKWARD)
		ended = (DTU_ANIM_FRAME(obj) < first);
	else
		ended = (DTU_ANIM_FRAME(obj) >= last);

	if (!ended) {
		if (aip->CURRENT_STATE == aip->GOAL_STATE)
			return;
		if (aip->GOAL_STATE != DAIS_FLIN && !(ail->flags & AILF_HIT))
			return;
	}

	if (DTU_ANIM_STATE(obj) == DAS_DIE) {
		// The last frame stays (0x11350).
		DTU_ANIM_FRAME(obj) = Dtu_anim_first[DAS_DIE+1] - 1;
		creature_is_corpse(obj);
		return;
	}

	// The end of the state (0x1137D)
	ail->flags &= ~(AILF_BACKWARD|AILF_AROUND|AILF_TOWARD|AILF_AWAY|AILF_HIT);
	aip->CURRENT_STATE = aip->GOAL_STATE;
	ail->swing_done = 0;
	ail->fired = 0;

	s = ai_to_anim_state(obj,aip->CURRENT_STATE,dist);
	if (ail->mode == DAIM_RUN_FROM)
		s = DAS_RUN;
	else if (s == DAS_ATTACK || s == DAS_ATTACK2) {
		// ATTACK2 if the player is higher or lower by more than half of
		// the distance in x and z (0x113F9).
		fix dy = abs(ConsoleObject->pos.y - obj->pos.y);
		vms_vector p = ConsoleObject->pos;
		fix hd;

		s = DAS_ATTACK;
		p.y = obj->pos.y;
		hd = vm_vec_dist(&p,&obj->pos);
		if (hd < 0x100 || fixdiv(dy,hd) > F1_0/2)
			s = DAS_ATTACK2;
	}
	set_anim(obj,s);
	ail->anim_start = GameTime;
}

// The animation of a creature that is not active (0x14013). One call is
// one step of the executable.
static void idle_animation_tick(object *obj,int tenth)
{
	dtu_ai_local *ail = &Ail[obj-Objects];
	ai_static *aip = &obj->ctype.ai_info;
	int s = DTU_ANIM_STATE(obj);

	if (s < 0 || s > DAS_FIDGET) {
		set_anim(obj,DAS_STAND);
		s = DAS_STAND;
	}

	if ((obj->flags & OF_SHOULD_BE_DEAD) || s == DAS_WALK || s > DAS_SIDLE_L || tenth)
		DTU_ANIM_FRAME(obj)++;

	if (DTU_ANIM_FRAME(obj) == Dtu_anim_first[s+1]) {
		int div = Monster_info[obj->id].iq >> (ail->awareness + 1);
		int next = DAS_STAND;

		// The executable takes the remainder of FrameTime + objnum, which
		// is a random number where the frame time changes. With a divisor
		// of 0 it stops with a division error.
		if (div > 0) {
			if (Dtu_fix_idle_anim) {
				if ((rand() % div) == 0)
					next = DAS_FIDGET;
			}
			else if (((FrameTime + (int)(obj-Objects)) % div) == 0)
				next = DAS_FIDGET;
		}

		aip->GOAL_STATE = aip->CURRENT_STATE = DAIS_NONE;
		DTU_ANIM_GOAL(obj) = next;
		set_anim(obj,next);
	}

	aip->CURRENT_STATE = anim_to_ai_state(obj);
}

static void idle_animation(object *obj)
{
	int objnum = obj-Objects;
	dtu_ai_local *ail = &Ail[objnum];

	if (!Dtu_fix_idle_anim) {
		idle_animation_tick(obj,((Dtu_clock + objnum) % 10) == 0);
		if (Obj_log_frames)
			log_creature_state(obj,"idle step");
		return;
	}

	// One step for each reference frame time, and the tenth step is the
	// one that moves the states that are slow.
	ail->idle_acc += FrameTime;
	while (ail->idle_acc >= DTU_REF_FRAMETIME) {
		ail->idle_acc -= DTU_REF_FRAMETIME;
		ail->idle_ticks++;
		idle_animation_tick(obj,((ail->idle_ticks + objnum) % 10) == 0);
	}
	if (Obj_log_frames)
		log_creature_state(obj,"idle step");
}

// The test of do_ai_frame that makes a creature active (0x13CE0, and
// 0x14230 for a creature that is active).
static int wake_test(object *obj)
{
	int objnum = obj-Objects;
	dtu_ai_local *ail = &Ail[objnum];
	vms_vector v;
	fix dot;

	if (ail->target == -1 || (obj->dtu.m_88 & 4) || Player_is_dead || Dtu_player_dead)
		return 0;
	// 0x13D1A: no test while the player is hidden from the creature
	// (0x165C4: invisible or hidden in shadows)
	if (dtu_thief_hidden_from(obj))
		return 0;
	if (!brain_bit(obj->segnum,ConsoleObject->segnum))
		return 0;

	vm_vec_sub(&v,&ConsoleObject->pos,&obj->pos);
	vm_vec_normalize_quick(&v);
	dot = vm_vec_dot(&v,&obj->orient.fvec);

	// The player is behind the creature: it notices the player with a
	// chance that grows with its iq.
	if (dot <= 0) {
		// 0x13D8E: Move Silently (P+0xA0 bit 4) and a deaf creature
		if ((Dtu_thief_bits & DTU_TS_SILENT) || (obj->dtu.m_88 & 0x10))
			return 0;
		if (Dtu_fix_ai_ticks) {
			if ((rand() % 255) > Monster_info[obj->id].iq)
				return 0;
		}
		else if (((Dtu_clock + objnum + dot) % 255) > Monster_info[obj->id].iq)
			return 0;
	}

	return creature_ray_to_player(obj);
}

// 0x13BF4
void dtu_do_ai_frame(object *obj)
{
	int objnum = obj-Objects;
	dtu_ai_local *ail = &Ail[objnum];
	ai_static *aip = &obj->ctype.ai_info;
	robot_info *ri;
	monster_info *mi;
	vms_vector vec_to_player, *believed;
	fix dist;
	int A, B, visible = -1, dying, blocked, hidden;

	if (obj->type != OBJ_ROBOT || obj->id >= N_robot_types)
		return;
	// ASCENT: 0x13C8B, object flag 0x40 (FLAME object 35)
	if (dtu_creature_frozen(obj))
		return;
	ri = &Robot_info[obj->id];
	mi = &Monster_info[obj->id];
	A = mi->ai_values[0][Difficulty_level];
	B = mi->ai_values[1][Difficulty_level];

	dying = (DTU_ANIM_STATE(obj) == DAS_DIE);

	if (dying)
		ail->activity = 2;
	else if (ail->activity != 2) {

		// The test that makes the creature active (0x13CE0)
		if (wake_test(obj)) {
			ail->target = is_hostile(obj) ? 0 : -1;
			ail->activity = 2;
			set_anim(obj,DAS_STAND);
			if (is_hostile(obj)) {
				ail->awareness = 4;
				ail->mode = DAIM_CHASE_OB;
				aip->CURRENT_STATE = DAIS_REST;
				aip->GOAL_STATE = DAIS_SRCH;
			}
			else {
				ail->awareness = 0;
				aip->CURRENT_STATE = aip->GOAL_STATE = DAIS_NONE;
			}
			dtu_obj_log("obj %d %s: sees the player",objnum,dtu_object_name(obj));
			// 0x13EFF: the alert 0x160AB(creature, 1); 0x13F09 and the
			// alert: the conditions of the spells
			dtu_creature_alert(obj,1);
			dtu_spell_creature_event(obj,DTU_CSC_WAKE|DTU_CSC_ALERT);
			// 0x13E25: the see sound, volume str * 0x1000
			sound_at(ri->see_sound,obj->segnum,&obj->pos,mi->str * 0x1000);
			morale_check(obj);
		}

		if (ail->activity != 2) {
			// 0x13F96: a creature that is not active stands on the floor
			// of its segment: the center of the bottom side plus the half
			// height of the body (0x746B3). The executable deletes the
			// creature if that point is outside of the mine ("pushed
			// critter %d through mine"). The port leaves it where it is.
			if (obj->control_type != CT_MORPH) {
				vms_vector c, new_pos;
				fix radius, half_height;

				dtu_get_body_size(obj,&radius,&half_height);
				compute_center_point_on_side(&c,&Segments[obj->segnum],WBOTTOM);
				new_pos = obj->pos;
				new_pos.y = c.y + half_height;
				if (find_point_seg(&new_pos,obj->segnum) >= 0)
					obj->pos = new_pos;
			}
			idle_animation(obj);
		}

		// 0x14134
		ail->time_player_seen = GameTime;
		ail->awareness_time = i2f(3);
		ail->anim_start = GameTime;
	}
	else if (ai_tick(objnum,20)) {
		// 0x14206: an active creature looks for the player again
		ail->activity = 0;
		if (wake_test(obj))
			ail->activity = 2;
		// Dtu_improved_pathfinding: a hostile creature that follows a path
		// to the segment of the player stays active. The test above fails at
		// random when the player is behind the creature, which is often the
		// case on a path around a wall, and the creature then stands for
		// one animation before it sees the player again.
		// ASCENT: not for a hidden player (C20: the creature does not know
		// the segment of the player)
		if (ail->activity == 0 && improved_paths() && is_hostile(obj) && ail->path_len >= 2 && !cloak_applies(obj) &&
			 ail->path[ail->path_len-1] == ConsoleObject->segnum && (ail->mode == DAIM_CHASE_OB || ail->mode == DAIM_FOL_PATH) &&
			 ail->target != -1 && !Player_is_dead && !Dtu_player_dead)
			ail->activity = 2;
		// ASCENT: a creature on a path of statement E5 of its script stays
		// active to the end of the path (a port decision: the wake test fails
		// for a creature that walks away from the player, and the script
		// waits for the end of the path).
		if (ail->activity == 0 && ail->script_walk == 1)
			ail->activity = 2;
		if (ail->activity == 0) {
			ail->anim_start = GameTime + FrameTime;
			aip->CURRENT_STATE = aip->GOAL_STATE = DAIS_NONE;
			DTU_ANIM_GOAL(obj) = DAS_REST;
			set_anim(obj,DAS_REST);
			dtu_obj_log("obj %d %s: lost the player",objnum,dtu_object_name(obj));
		}
	}

	// 0x1440C: a creature with "regeneration" gets one hit point each
	// "regeneration" seconds. The damage function clears the status bit
	// when the creature dies (0x2A1D0), so the part of this code that lets
	// a dead creature get up never runs for a creature that was killed.
	if (Monster_ext[obj->id].regeneration > 0 && (obj->dtu.m_88 & 0x01000000) && !dying) {
		if (GameTime < ail->regen_time)
			ail->regen_time = GameTime;
		if (GameTime - ail->regen_time >= i2f(Monster_ext[obj->id].regeneration)) {
			ail->regen_time = GameTime;
			if (obj->shields < i2f(mi->max_hit_points)) {
				obj->shields += F1_0;
				dtu_obj_log("obj %d %s: regenerates, hp %d",objnum,dtu_object_name(obj),f2i(obj->shields));
			}
		}
	}

	// 0x146D0: the distance to the player. ASCENT: 0x146CE, a hidden
	// player: to the believed position (cloak_position, C20)
	ail->time_since_processed += FrameTime;
	hidden = !dying && cloak_applies(obj);
	believed = hidden ? cloak_position(obj) : &ConsoleObject->pos;
	vm_vec_sub(&vec_to_player,believed,&obj->pos);
	dist = vm_vec_mag(&vec_to_player);
	if (dist > 0x100)
		vm_vec_normalize(&vec_to_player);
	else
		vec_to_player = obj->orient.fvec;

	// 0x14825: the awareness goes down by one each 2.0 seconds
	if (ail->awareness) {
		ail->awareness_time -= FrameTime;
		if (ail->awareness_time <= 0) {
			ail->awareness_time = i2f(2);
			ail->awareness--;
		}
	}

	// 0x1487A
	if (ail->awareness >= 1 && ail->target != -1 && !(obj->dtu.m_1b0 & DTU_FLEEING))
		ail->mode = DAIM_CHASE_OB;
	if (aip->GOAL_STATE == DAIS_FLIN && aip->CURRENT_STATE == DAIS_FLIN)
		aip->GOAL_STATE = DAIS_LOCK;

	if (!dying && (obj->dtu.m_88 & DTU_OF_HELD))
		return;

	// 0x148D5
	if (aip->GOAL_STATE != DAIS_NONE || aip->CURRENT_STATE != DAIS_NONE)
		ai_animation_step(obj,dist);

	if (DTU_ANIM_STATE(obj) == DAS_DIE || obj->control_type != CT_AI) {
		log_creature_state(obj,"dying");
		return;
	}

	// 0x1490B: a creature with a mage or a cleric level casts (dtuspell.c)
	dtu_spell_creature_frame(obj,dist,ail->activity == 2);

	// 0x1498B: a creature that is far away is not processed on each frame.
	// ASCENT: a creature on a path of its script is (Dtu_fix_script_walk),
	// else it walks only in the frames of the AI.
	if (ail->awareness < 3 && ail->script_walk != 1) {
		if (dist > i2f(150) && ail->time_since_processed <= F1_0)
			return;
		if (dist > i2f(100) && ail->time_since_processed <= F1_0/2)
			return;
	}
	ail->time_since_processed = -((objnum & 3) * FrameTime) / 2;

	// 0x14A1C: a creature that is not active stands and looks at the player
	if (ail->activity == 0) {
		aip->CURRENT_STATE = aip->GOAL_STATE = DAIS_NONE;
		// The executable sets STAND and its first frame here (0x14A54).
		// A creature that is nearer than 100.0 comes here on each frame,
		// so its idle animation never leaves the first frame of STAND. A
		// creature that is farther away comes here each half second or
		// second and plays the idle animation between two visits.
		if (!Dtu_idle_anim_near)
			set_anim(obj,DAS_STAND);

		// ASCENT: a hidden player (C20): the creature turns to the believed
		// position when it sees that place (0x14A1C calls compute_vis_and_vec)
		if (hidden)
			visible = ray_is_free(&obj->pos,obj->segnum,believed,objnum);
		else
			visible = player_is_visible(obj);
		if (visible)
			ail->time_player_seen = GameTime;
		if (visible || ail->previous_visibility)
			turn_towards_vector(&vec_to_player,obj,ri->firing_wait[Difficulty_level]);
		// else: the random turn (0x10763), which does nothing here
		ail->previous_visibility = visible;
		log_creature_state(obj,"idle");
		return;
	}

	// 0x15074: the sight. ASCENT: a hidden player (C20): the ray to the
	// believed position (0x12CEF tests the brain bit there); the player is
	// not seen, the place where the creature believes it is.
	if (hidden)
		visible = ray_is_free(&obj->pos,obj->segnum,believed,objnum);
	else
		visible = player_is_visible(obj);
	creature_sight_sound(obj,hidden ? 0 : visible,&vec_to_player);
	if (visible)
		ail->time_player_seen = GameTime;

	// The awareness of a creature goes up by the awareness events
	// (awareness_events_frame, 0x16366). An earlier guess of the port in
	// their place (a hostile creature that sees the player has the
	// awareness 1 at least) is gone.

	// 0x14AF2: the door opener, for each creature with the door bit. It
	// only sends the creature to a door. The door opens when the creature
	// walks into it (dtu_creature_physics_sim). ASCENT: a creature on the
	// path of its script (Dtu_fix_script_walk) is not sent to a door: the
	// mode OPENDOOR would end the walk, and a door on its path opens when it
	// walks into it.
	if (Monster_ext[obj->id].door_opening && aip->behavior != DAIB_STATION && ail->mode != DAIM_CHASE_OB && ail->script_walk != 1) {
		if (ail->mode == DAIM_STILL) {
			if (visible)
				create_n_segment_path(obj,8+Difficulty_level,-1);
		}
		else {
			int side = openable_door_in_segment(obj);

			if (side != -1) {
				ail->mode = DAIM_OPENDOOR;
				aip->GOALSIDE = side;
			}
			else if (ail->mode != DAIM_FOL_PATH)
				create_n_segment_path(obj,8+Difficulty_level,-1);
		}
	}

	// 0x14BA1: the blocked move. The corrected rule runs it one time for
	// each reference frame time.
	blocked = 0;
	if (ail->retry_count && (DTU_ANIM_STATE(obj) == DAS_WALK || DTU_ANIM_STATE(obj) == DAS_RUN) && ail->blocker != 0 &&
		 (!Dtu_fix_creature_move || Ref_ticks_now > 0) && !body_path_handles_block(obj))
		blocked = blocked_move(obj,dist,&vec_to_player,visible);

	// 0x15074: the mode
	if (!blocked)
	switch (ail->mode) {

		case DAIM_RUN_FROM:			// 0x14D65, 0x123EF
			flee_from_player(obj,&vec_to_player);
			break;

		case DAIM_FOL_PATH:			// 0x153B0
			follow_path(obj,visible,dist);
			if (aip->behavior != DAIB_PATROL && ail->morale >= B && is_hostile(obj) && aip->GOAL_STATE != DAIS_FLIN)
				aip->GOAL_STATE = DAIS_LOCK;
			// ASCENT: Dtu_fix_script_walk, the walk animation (the goal SRCH
			// gives WALK, 0x10148) for a creature on a path of its script
			else if (ail->script_walk == 1 && aip->GOAL_STATE != DAIS_FLIN)
				aip->GOAL_STATE = DAIS_SRCH;
			break;

		case DAIM_HOME:				// 0x1541A
			follow_path(obj,visible,dist);
			if (ail->morale >= B && is_hostile(obj) && !(ri->attack_type != 1 && aip->GOAL_STATE == DAIS_ATAK))
				aip->GOAL_STATE = DAIS_LOCK;
			break;

		case DAIM_OPENDOOR: {		// 0x156A5
			vms_vector c, v;

			compute_center_point_on_side(&c,&Segments[obj->segnum],aip->GOALSIDE % MAX_SIDES_PER_SEGMENT);
			vm_vec_sub(&v,&c,&obj->pos);
			vm_vec_normalize_quick(&v);
			turn_towards_vector(&v,obj,ri->firing_wait[Difficulty_level]);
			add_move(obj,&v);
			break;
		}

		case DAIM_WANDER:				// 0x15096
			if (!is_hostile(obj)) {
				if (ail->path_len == 0) {
					ail->goal_segment = rand() % (Highest_segment_index+1);
					create_path(obj);
				}
				follow_path(obj,visible,dist);
				break;
			}
			// a hostile creature: as CHASE_OB

		case DAIM_CHASE_OB:			// 0x150DD
		default: {
			fix circle = ConsoleObject->size;

			if (!is_hostile(obj))
				break;
			if (ri->attack_type != 1)
				circle = ri->turn_time[Difficulty_level] + ConsoleObject->size;		// the third group of $MONSTER_AI

			if (!visible && dist > i2f(80) && aip->behavior != DAIB_STATION) {
				// Dtu_improved_pathfinding: a hostile creature that knows
				// where the player is walks to the player on a path. The
				// executable makes a random path of 5 segments here on each
				// frame and goes back to CHASE_OB on the next frame (the
				// awareness is 1 or more), so the creature never moves.
				// ASCENT: no path to the segment of a hidden player (C20)
				if (!hidden && improved_paths() && Dtu_fix_path_follow && aip->behavior != DAIB_HOME && aip->behavior != DAIB_GUARD &&
					 ail->morale >= B && chase_by_path(obj))
					;
				else if (aip->behavior == DAIB_HOME || aip->behavior == DAIB_GUARD)
					create_path_to_station(obj,15);
				else if (improved_paths() && Dtu_fix_path_follow) {
					// with no way to the player: the creature walks along
					// the random path, and keeps it until its end
					int k, on = 0;

					for (k=0;k<ail->path_len;k++)
						if (ail->path[k] == obj->segnum)
							on = 1;
					if (!on || ail->path_flag != -1 || ail->path_len < 2)
						create_n_segment_path(obj,5,-1);
					follow_path(obj,visible,dist);
				}
				else
					create_n_segment_path(obj,5,-1);
				aip->GOAL_STATE = DAIS_SRCH;
			}
			else if (aip->CURRENT_STATE == DAIS_REST && aip->GOAL_STATE == DAIS_REST && visible && ail->morale >= B)
				aip->GOAL_STATE = DAIS_SRCH;
			else if (!hidden && GameTime - ail->time_player_seen > i2f(8)) {
				if (ail->morale >= B)
					create_path_to_player(obj,10);
			}
			else if (aip->CURRENT_STATE != DAIS_REST && aip->GOAL_STATE != DAIS_REST) {
				// Dtu_improved_pathfinding: also a creature that sees the
				// player but cannot walk to the player in a straight line
				// (the line is not free for its body) takes the path.
				// ASCENT: a hidden player: the line to the believed position,
				// and no path (C20)
				if (improved_paths() && Ref_ticks_now > 0)
					ail->body_line = hidden ? body_line_free(obj,believed) :
						(obj->segnum == ConsoleObject->segnum) || body_line_free(obj,&ConsoleObject->pos);
				if (!hidden && Dtu_fix_path_follow && (!visible || (improved_paths() && !ail->body_line)) && aip->behavior != DAIB_STATION && chase_by_path(obj)) {
					// as in the mode FOL_PATH (0x153F0)
					if (ail->morale >= B && aip->GOAL_STATE != DAIS_FLIN)
						aip->GOAL_STATE = DAIS_LOCK;
				}
				else
					move_relative_to_player(obj,dist,&vec_to_player,circle,0);
				if (ail->morale < B) {
					// "Bad moralle (%d)"
					ail->activity = 0;
					morale_check(obj);
				}
			}
			break;
		}

		case DAIM_STILL:				// 0x154BC
			if ((dist < i2f(120) + i2f(20)*Difficulty_level || ail->awareness >= 3) && is_hostile(obj)) {
				if ((visible || ail->previous_visibility) && ail->morale >= B)
					turn_towards_vector(&vec_to_player,obj,ri->firing_wait[Difficulty_level]);
				if (visible && ri->attack_type == 1)
					move_relative_to_player(obj,dist,&vec_to_player,0,0);
				else if (!visible && obj->segnum != aip->hide_segment && dist > i2f(80) &&
							(aip->behavior == DAIB_PATROL || aip->behavior == DAIB_HOME))
					create_path_to_station(obj,15);
			}
			else if (visible)
				turn_towards_vector(&vec_to_player,obj,ri->firing_wait[Difficulty_level]);
			break;
	}

	// do_firing_stuff (0x12916)
	if (visible && is_hostile(obj) && ail->morale >= B) {
		fix dot = vm_vec_dot(&obj->orient.fvec,&vec_to_player);

		// 0x1293D: or the player is hidden (0x165C4), C20
		if ((dot >= 3*F1_0/4 || hidden) && aip->GOAL_STATE <= DAIS_ATAK && aip->GOAL_STATE != DAIS_FLIN) {
			choose_attack_goal(obj,dist);
			if (ail->awareness < 1) {
				ail->awareness = 1;
				ail->awareness_time = i2f(3);
			}
		}
		else if (dot >= F1_0/2 && aip->GOAL_STATE <= DAIS_SRCH)
			aip->GOAL_STATE = DAIS_LOCK;
	}

	ail->previous_visibility = visible;

	// 0x15876: the choice of the goal state
	if (ail->awareness) {
		if (ail->awareness == 5) {
			aip->GOAL_STATE = (aip->CURRENT_STATE == DAIS_FLIN) ? DAIS_LOCK : DAIS_FLIN;
			ail->awareness = 2;
			ail->awareness_time = i2f(3);
		}
		if (aip->CURRENT_STATE == DAIS_DEAD)
			aip->GOAL_STATE = DAIS_DEAD;
		if ((obj->dtu.m_88 & 0x100) || ail->morale <= A)
			ail->mode = DAIM_RUN_FROM;
		if ((aip->GOAL_STATE == DAIS_REST || aip->GOAL_STATE == DAIS_SRCH || aip->GOAL_STATE == DAIS_LOCK || aip->GOAL_STATE == DAIS_ATAK) && ail->morale >= B)
			choose_attack_goal(obj,dist);
		if (aip->GOAL_STATE == DAIS_FLIN && aip->CURRENT_STATE == DAIS_FLIN)
			aip->GOAL_STATE = DAIS_LOCK;
		if (aip->GOAL_STATE == DAIS_RECO && aip->CURRENT_STATE == DAIS_RECO && ail->morale >= B)
			choose_attack_goal(obj,dist);
	}

	if (aip->GOAL_STATE == DAIS_FLIN) {
		log_creature_state(obj,"flinch");
		return;
	}

	// 0x15B2F: the current state
	switch (aip->CURRENT_STATE) {

		case DAIS_NONE:
			if (is_hostile(obj))
				aip->GOAL_STATE = DAIS_SRCH;
			break;

		case DAIS_REST:
			if (aip->GOAL_STATE == DAIS_REST) {
				if (visible && ail->morale >= B)
					choose_attack_goal(obj,dist);
				else if (!is_hostile(obj))
					aip->GOAL_STATE = DAIS_NONE;
			}
			break;

		case DAIS_SRCH:
		case DAIS_LOCK:
		case DAIS_RECO:
			// ASCENT: a creature on a path of its script faces the way it
			// walks (path_steer turns it)
			if (ail->script_walk == 1)
				break;
			if (visible && ail->morale >= B)
				turn_towards_vector(&vec_to_player,obj,ri->firing_wait[Difficulty_level]);
			else
				random_turn(obj,&vec_to_player);
			break;

		case DAIS_ATAK:
			if (visible && ail->morale >= B)
				turn_towards_vector(&vec_to_player,obj,ri->firing_wait[(ri->attack_type == 1) ? Difficulty_level : 4]);
			else
				random_turn(obj,&vec_to_player);
			// ai_do_actual_firing_stuff (0x1396E): a melee creature hits in
			// weapon_test(). A ranged creature fires in each call in which
			// the animation is on a fire frame of the model (0x13A2A). The
			// corrected rule of Dtu_fix_fire_once: one shot for each attack.
			if (is_hostile(obj)) {
				if (visible && vm_vec_dot(&obj->orient.fvec,&vec_to_player) >= 3*F1_0/4) {
					polymodel *po = obj_model(obj);

					if (ri->attack_type != 1 && po &&
						 (DTU_ANIM_FRAME(obj) == po->fire_frames[0] || DTU_ANIM_FRAME(obj) == po->fire_frames[1]) &&
						 !(Dtu_fix_fire_once && ail->fired)) {
						ail->fired = 1;
						Ai_believed = hidden ? believed : NULL;		// ASCENT: C20
						ai_fire(obj,dist);
						Ai_believed = NULL;
					}
					if (aip->behavior != DAIB_PATROL && (ail->mode == DAIM_STILL || ail->mode == DAIM_FOL_PATH)) {
						if (ail->morale >= B)
							ail->mode = DAIM_CHASE_OB;
						else if (ail->morale <= A)
							ail->mode = DAIM_RUN_FROM;
					}
				}
				aip->GOAL_STATE = DAIS_RECO;
			}
			break;
	}

	log_creature_state(obj,"ai");
}

//------------------------------------------------------------------------
// Calls from dtuhook.c

// ASCENT: for the scripts (dtuscript.c). The anger byte of the scripts is
// ai_local +0x00 (docs/re/scripts.md).
ubyte *dtu_script_anger(object *obj)
{
	return &Ail[obj-Objects].awareness;
}

// ASCENT: the end of the prison sequence of the scripts (0xAA61E to
// 0xAA661, dtuscript.c): behavior 0x80 and AI mode 0 for a creature of
// THIEF2; the new guard also has the player as its target (+0x1A2 = 0).
void dtu_script_prison_guard(object *obj,int set_target)
{
	if (obj->type != OBJ_ROBOT)
		return;
	obj->ctype.ai_info.behavior = DAIB_GUARD;
	Ail[obj-Objects].mode = DAIM_STILL;
	if (set_target)
		Ail[obj-Objects].target = 0;
}

// ASCENT: statement E5 of the scripts (walk a path, dtuscript.c). 0xAA74E
// for each segment of the statement: the segment is the station (+0x14C)
// and the goal (ai_local +0x06) of the creature and create_path (0x16FD9)
// makes the path to it, so the path of the last segment stays. 0xAA810
// after the list: no target, AI mode 2 (follow the path), the bits 0x30 of
// ai_local +0x05 and the morale (A + B) / 2.
void dtu_script_walk_segment(object *obj,int seg)
{
	if (obj->type != OBJ_ROBOT || seg < 0 || seg > Highest_segment_index)
		return;
	obj->ctype.ai_info.hide_segment = seg;
	Ail[obj-Objects].goal_segment = seg;
	create_path(obj);
}

// fix 1 (Dtu_fix_script_walk): the creature walks along the path with the
// walk animation (goal SRCH, animation WALK) and stays active to its end,
// also farther than 200.0 from the player. With fix 0 the rules of 1.3
// stay: the goal NONE of a creature that is not hostile gives the
// animation STAND and no speed, and the path follower turns it to the
// player (0x17CFC).
void dtu_script_walk_start(object *obj,int fix)
{
	dtu_ai_local *ail = &Ail[obj-Objects];
	char segs[96];
	int k, n = 0;

	if (obj->type != OBJ_ROBOT)
		return;
	ail->target = -1;
	ail->mode = DAIM_FOL_PATH;
	ail->flags |= 0x30;
	if (obj->id < N_robot_types)
		ail->morale = (Monster_info[obj->id].ai_values[0][Difficulty_level] + Monster_info[obj->id].ai_values[1][Difficulty_level]) / 2;
	ail->script_walk = 0;
	// ASCENT: a creature with no door bit gets no path through a segment
	// with a wall (0x16999), so a prisoner of PRISON (script 103) cannot
	// leave its cell. On the path of its script it opens a door with no key
	// and no lock as a creature with the door bit does.
	if (fix && ail->path_len < 2 && obj->id < N_robot_types && !Monster_ext[obj->id].door_opening &&
		 ail->goal_segment >= 0 && obj->segnum != ail->goal_segment) {
		Script_walk_doors = 1;
		create_path(obj);
		Script_walk_doors = 0;
	}
	if (fix && ail->path_len >= 2 && DTU_ANIM_STATE(obj) != DAS_DIE) {
		ail->script_walk = 1;
		ail->activity = 2;
		obj->ctype.ai_info.GOAL_STATE = obj->ctype.ai_info.CURRENT_STATE = DAIS_SRCH;
		DTU_ANIM_GOAL(obj) = DAS_WALK;
		set_anim(obj,DAS_WALK);
		ail->anim_start = GameTime;
	}
	else if (fix && DTU_ANIM_STATE(obj) != DAS_DIE && obj->segnum == ail->goal_segment)
		ail->script_walk = 2;		// in the segment already: at the end of the path
	segs[0] = 0;
	for (k=0;k<ail->path_len && n < (int)sizeof(segs)-8;k++)
		n += sprintf(segs+n,"%s%d",k ? " " : "",ail->path[k]);
	dtu_obj_log("obj %d %s: walks a path of %d segments to seg %d for its script (%s): %s",(int)(obj-Objects),dtu_object_name(obj),
		ail->path_len,ail->goal_segment,ail->script_walk ? "walk" : "1.3 rule",segs);
}

// 0: the creature is not on a path of a script. 1: it walks. 2: it is at
// the end of the path (the state goes back to 0).
int dtu_script_walk_state(object *obj)
{
	dtu_ai_local *ail = &Ail[obj-Objects];

	if (obj->type != OBJ_ROBOT)
		return 0;
	if (ail->script_walk == 2) {
		ail->script_walk = 0;
		return 2;
	}
	if (ail->script_walk == 1 && (ail->mode != DAIM_FOL_PATH || ail->path_len < 2)) {
		// another rule of the AI took the creature off the path
		ail->script_walk = 0;
		return 0;
	}
	return ail->script_walk;
}

// ASCENT: the test of the warp check at 0x59DF2 (WEAPON 13 in the hand,
// dtuscript.c): the flee value A of the creature (Robot_info +0x80 for the
// difficulty) is below its morale (ai_local +0x28).
int dtu_script_creature_brave(object *obj)
{
	if (obj->type != OBJ_ROBOT || obj->id >= N_robot_types)
		return 0;
	return Monster_info[obj->id].ai_values[0][Difficulty_level] < Ail[obj-Objects].morale;
}

// ASCENT: 0xA9418, a script makes the object hostile: chase mode, goal
// SRCH, animation RUN, anger 4, hostile bit, target the player, morale.
void dtu_script_make_hostile(object *obj)
{
	dtu_ai_local *ail;

	if (obj->type != OBJ_ROBOT || obj->id >= N_robot_types || obj->shields < F1_0 || DTU_ANIM_STATE(obj) == DAS_DIE)
		return;
	dtu_creature_wake(obj,1);
	ail = &Ail[obj-Objects];
	ail->mode = DAIM_CHASE_OB;
	obj->ctype.ai_info.GOAL_STATE = DAIS_SRCH;
	set_anim(obj,DAS_RUN);
	ail->awareness = 4;
	ail->target = 0;
	dtu_creature_alert(obj,4);		// 0xA94A1
	morale_check(obj);
}

static void creature_wake(object *obj, int make_hostile, int sound);

// 0xA6505 (not read in full): makes a creature active, as the wake up of
// do_ai_frame does (0x13D80).
void dtu_creature_wake(object *obj, int make_hostile)
{
	creature_wake(obj,make_hostile,1);
}

// ASCENT: the group part of the alert function 0x160AB (ai.md, "Alert
// function"). Levels 3 to 5 only: the executable calls it with 3 when the
// player hits a creature in melee (0x27B3F, caller the player), when a
// missile of the player hits a creature (0x274B7, caller the creature) and
// when the player runs into a hostile creature (0x2707A, 0x9E70D, caller the
// creature); with 4 when a body becomes a corpse (0x6A45A), when a script
// makes a creature hostile (0xA94A1) and when a creature gets up again
// (0x14602); with 5 at a rest place (0x3E2C1, caller the player) and for
// the script statement 3/0xC5 (0xAB080). Each other creature (index 1 to
// Highest_object_index - 1) that is not active, not peaceful and not
// fleeing, whose segment sees the segment of the caller (the brain bit,
// 0x9810D) and that has a free ray to the player (0x10815) turns to the
// caller, becomes hostile and wakes (0xA6505). A deaf creature (effect bit
// 4) must face the caller. The test 0x161BC (rand() % distance <= iq * 1.0)
// always passes, since rand() is at most 0x7FFF. The see sound plays for
// every other creature: the executable takes the parity of a player field
// (0x1D339F) plus the object index; the port takes the parity of the index.
// Each level first adds an awareness event (0x16055, below). The agitation
// (0x167174) and the bits of +0x15E are not in the port.
//
// ASCENT: the awareness events (0x16055, 0x16304, 0x162A1, 0x16366, ai.md
// "Awareness events"). 0x16055 keeps up to 64 events (segment, position,
// type = the level of the alert) for the frame. Each frame 0x16366 (called
// by the game frame, 0x438AC) clears New_awareness of all segments, and for
// each event 0x162A1(segment, type, 1) stores the type in the segment when
// it is higher and, while the depth is 5 or less, calls itself for each
// child of the 6 sides with depth + 1 and type 5 made 4. So the event
// reaches the segments up to 5 steps away, through any wall. Then each AI
// object with the hostile bit whose awareness is below New_awareness of its
// segment gets that awareness, the awareness time 3.0 and the morale check.
// The recursion has no visited set (up to 6^5 calls for one event); the port
// gives the same values with a search by distance.
//
// ASCENT: settings of the alert (D-112 to D-114 of docs/audit/defects-1.3.md,
// verified in the bytes). -origobj clears them.
// Dtu_fix_noise_walls: 0x162A1 follows each child with no wall test, so the
// noise goes through closed doors and walls. 1 (default): the noise passes
// only a side that a body can pass or that can be seen through (an open
// door, a grate); a closed door stops it.
// Dtu_fix_alert_ray: 0x161CA casts the ray of the wake test to the player
// (0x10815), also when the caller is a corpse or a hit creature. 1
// (default): the ray goes to the caller.
// Dtu_fix_alert_last: the wake loop ends before Highest_object_index
// (0x1628D, jl), so the creature in the last slot never wakes. 1 (default):
// the loop includes it.
int Dtu_fix_noise_walls = 1;
int Dtu_fix_alert_ray = 1;
int Dtu_fix_alert_last = 1;

#define AWARE_MAX_EVENTS	64
static struct { short segnum; short type; } Aware_events[AWARE_MAX_EVENTS];

static void awareness_event(object *obj, int type)
{
	// ASCENT: 0x16055: the types 2 to 4 call 0x15FBC, which puts the
	// believed position of a hidden player at the player (cloak_reset)
	if (type >= 2 && type <= 4)
		cloak_reset();
	if (N_aware_events < AWARE_MAX_EVENTS && obj->segnum >= 0 && obj->segnum <= Highest_segment_index) {
		Aware_events[N_aware_events].segnum = obj->segnum;
		Aware_events[N_aware_events].type = type;
		N_aware_events++;
	}
}

static void awareness_events_frame(void)
{
	static ubyte new_aware[MAX_SEGMENTS], depth[MAX_SEGMENTS];
	static short queue[MAX_SEGMENTS];
	int e, i;

	if (N_aware_events == 0)
		return;
	memset(new_aware,0,sizeof(new_aware));
	for (e=0;e<N_aware_events;e++) {
		int type = Aware_events[e].type, near = (type == 5) ? 4 : type, head = 0, tail = 0;

		memset(depth,0xff,sizeof(depth));
		queue[tail++] = Aware_events[e].segnum;
		depth[Aware_events[e].segnum] = 1;
		while (head < tail) {
			int s = queue[head++], side, t = (depth[s] == 1) ? type : near;

			if (new_aware[s] < t)
				new_aware[s] = (ubyte) t;
			if (depth[s] > 5)
				continue;
			for (side=0;side<MAX_SIDES_PER_SEGMENT;side++) {
				int c = Segments[s].children[side];

				if (Dtu_fix_noise_walls && c >= 0 &&
					 !(WALL_IS_DOORWAY(&Segments[s],side) & (WID_FLY_FLAG|WID_RENDPAST_FLAG)))
					continue;
				if (c >= 0 && c <= Highest_segment_index && depth[c] == 0xff) {
					depth[c] = depth[s] + 1;
					queue[tail++] = c;
				}
			}
		}
		dtu_obj_log("noise: type %d at seg %d reaches %d segments",type,Aware_events[e].segnum,tail);
	}
	N_aware_events = 0;

	for (i=0;i<=Highest_object_index;i++) {
		object *obj = &Objects[i];

		if (obj->type != OBJ_ROBOT || obj->control_type != CT_AI || obj->id >= N_robot_types || !(obj->dtu.m_1b0 & DTU_HOSTILE))
			continue;
		if (obj->segnum < 0 || obj->segnum > Highest_segment_index || Ail[i].awareness >= new_aware[obj->segnum])
			continue;
		dtu_obj_log("awareness: obj %d %s %d -> %d",i,dtu_object_name(obj),Ail[i].awareness,new_aware[obj->segnum]);
		Ail[i].awareness = new_aware[obj->segnum];
		Ail[i].awareness_time = i2f(3);
		morale_check(obj);
	}
}

void dtu_creature_alert(object *caller, int level)
{
	int i;

	if (!Dtu_objects_enabled || caller == NULL || ConsoleObject == NULL)
		return;
	awareness_event(caller,level);
	if (level < 3)
		return;
	for (i=1;i<Highest_object_index + (Dtu_fix_alert_last ? 1 : 0);i++) {
		object *obj = &Objects[i];
		vms_vector v;

		if (obj == caller || obj->type != OBJ_ROBOT || obj->control_type != CT_AI || obj->id >= N_robot_types)
			continue;
		if (obj->flags & (OF_EXPLODING|OF_SHOULD_BE_DEAD|OF_DESTROYED) || DTU_ANIM_STATE(obj) == DAS_DIE)
			continue;
		if (Ail[i].activity == 2 || (obj->dtu.m_1b0 & (DTU_PEACEFUL|DTU_FLEEING)))
			continue;
		if (!brain_bit(obj->segnum,caller->segnum) ||
			 !(Dtu_fix_alert_ray ? creature_ray_to(obj,&caller->pos) : creature_ray_to_player(obj)))
			continue;
		vm_vec_sub(&v,&caller->pos,&obj->pos);
		if ((obj->dtu.m_88 & DTU_FX_DEAF) && vm_vec_dot(&v,&obj->orient.fvec) <= 0)
			continue;
		v.y = 0;
		if (vm_vec_mag_quick(&v) > F1_0/16) {
			vm_vec_normalize_quick(&v);
			vm_vector_2_matrix(&obj->orient,&v,NULL,NULL);
		}
		obj->dtu.m_1b0 |= DTU_HOSTILE;
		dtu_obj_log("alert: level %d from obj %d %s wakes obj %d %s",level,(int)(caller-Objects),dtu_object_name(caller),i,dtu_object_name(obj));
		creature_wake(obj,0,i & 1);
	}
}

static void creature_wake(object *obj, int make_hostile, int sound)
{
	dtu_ai_local *ail;
	ai_static *aip;

	if (obj->type != OBJ_ROBOT || obj->control_type != CT_AI || obj->id >= N_robot_types || DTU_ANIM_STATE(obj) == DAS_DIE)
		return;
	ail = &Ail[obj-Objects];
	aip = &obj->ctype.ai_info;

	if (make_hostile) {
		obj->dtu.m_1b0 |= DTU_HOSTILE;
		obj->dtu.m_1b0 &= ~(DTU_PEACEFUL|DTU_FLEEING);
	}
	if (ail->activity == 2)
		return;

	ail->target = is_hostile(obj) ? 0 : -1;
	ail->activity = 2;
	set_anim(obj,DAS_STAND);
	ail->anim_start = ail->time_player_seen = GameTime;
	ail->awareness_time = i2f(3);
	if (sound)
		sound_at(Robot_info[obj->id].see_sound,obj->segnum,&obj->pos,Monster_info[obj->id].str * 0x1000);
	if (is_hostile(obj)) {
		ail->awareness = 4;
		ail->mode = DAIM_CHASE_OB;
		aip->CURRENT_STATE = DAIS_REST;
		aip->GOAL_STATE = DAIS_SRCH;
		morale_check(obj);
	}
	else {
		ail->awareness = 0;
		aip->CURRENT_STATE = aip->GOAL_STATE = DAIS_NONE;
	}
	dtu_obj_log("obj %d %s: woken, hostile %d",(int)(obj-Objects),dtu_object_name(obj),is_hostile(obj));
}

// ASCENT: the state check of the soak test (dtusoak.c): the AI values of
// each creature. Writes the first problem to msg and returns 1, or 0.
int dtu_objects_check(char *msg, int size)
{
	int i;

	for (i=0; i<=Highest_object_index; i++) {
		object *obj = &Objects[i];
		dtu_ai_local *ail = &Ail[i];
		polymodel *po;
		int s, f;

		if (obj->type != OBJ_ROBOT || obj->control_type != CT_AI)
			continue;
		if (obj->id >= N_robot_types) {
			snprintf(msg,size,"obj %d: robot id %d of %d types",i,obj->id,N_robot_types);
			return 1;
		}
		s = DTU_ANIM_STATE(obj);
		if (s < 0 || s >= DTU_N_ANIM_STATES) {
			snprintf(msg,size,"obj %d %s: animation state %d",i,dtu_object_name(obj),s);
			return 1;
		}
		f = DTU_ANIM_FRAME(obj);
		po = obj_model(obj);
		if (po && po->n_frames > 0 && (f < -1 || f >= Dtu_anim_first[DTU_N_ANIM_STATES])) {
			snprintf(msg,size,"obj %d %s: frame %d in state %d",i,dtu_object_name(obj),f,s);
			return 1;
		}
		if (ail->target < -1 || ail->target > Highest_object_index) {
			snprintf(msg,size,"obj %d %s: target %d",i,dtu_object_name(obj),ail->target);
			return 1;
		}
		// (a path index past the end of an empty path is not used)
		if (ail->path_len < 0 || ail->path_len > DTU_PATH_MAX || ail->path_idx < -1 || ail->path_idx > DTU_PATH_MAX) {
			snprintf(msg,size,"obj %d %s: path index %d of %d",i,dtu_object_name(obj),ail->path_idx,ail->path_len);
			return 1;
		}
		for (s=0; s<ail->path_len; s++)
			if (ail->path[s] < 0 || ail->path[s] > Highest_segment_index) {
				snprintf(msg,size,"obj %d %s: path point %d is segment %d",i,dtu_object_name(obj),s,ail->path[s]);
				return 1;
			}
		if (ail->mode > DAIM_OPENDOOR) {
			snprintf(msg,size,"obj %d %s: mode %d",i,dtu_object_name(obj),ail->mode);
			return 1;
		}
	}
	return 0;
}

// Turn undead (0xADC46, dtucombat.c): the result for one creature. The
// executable sets the flags, calls 0xA6505 (the creature starts again,
// also when it is active) and the morale function 0x10ADF.
void dtu_creature_morale(object *obj)
{
	if (obj && obj->type == OBJ_ROBOT && obj->id < N_robot_types)
		morale_check(obj);
}

void dtu_creature_turn(object *obj, int turned)
{
	dtu_ai_local *ail;

	if (obj->type != OBJ_ROBOT || obj->control_type != CT_AI || obj->id >= N_robot_types || DTU_ANIM_STATE(obj) == DAS_DIE)
		return;
	ail = &Ail[obj-Objects];
	if (turned) {
		// 0xAE1DD: target -1, fleeing, not hostile
		ail->target = -1;
		obj->dtu.m_1b0 = (obj->dtu.m_1b0 & ~DTU_HOSTILE) | DTU_FLEEING;
	}
	else
		obj->dtu.m_1b0 |= DTU_HOSTILE;		// 0xAE0DC, 0xAE22B
	ail->activity = 0;
	dtu_creature_wake(obj,0);
	// dtu_creature_wake() makes the morale check of a hostile creature
	if (!is_hostile(obj))
		morale_check(obj);
}

// gate_in_robot (0x1374D, 0x13407): a new creature at the center of the
// segment. The limit is 2 * difficulty + 3 creatures that were made this
// way. The executable does not set the hostile bit of the new creature
// here and what sets it was not found. The port makes it hostile. (guess)
int dtu_gate_in_creature(int type, int segnum)
{
	int i, count = 0, objnum;
	vms_vector pos, v;
	object *obj;

	if (segnum < 0 || segnum > Highest_segment_index || type < 0 || type >= N_robot_types)
		return -1;

	for (i=0;i<=Highest_object_index;i++)
		if (Objects[i].type == OBJ_ROBOT && Ail[i].gated && Objects[i].control_type == CT_AI)
			count++;
	if (count > 2*Difficulty_level + 3) {
		dtu_obj_log("gate: no creature, %d gated creatures live",count);
		return -1;
	}

	compute_segment_center(&pos,&Segments[segnum]);

	// the place must be free
	for (i=Segments[segnum].objects;i!=-1;i=Objects[i].next)
		if ((Objects[i].type == OBJ_ROBOT || Objects[i].type == OBJ_PLAYER) && !(Objects[i].flags & OF_DESTROYED) &&
			 vm_vec_dist_quick(&Objects[i].pos,&pos) < Objects[i].size + i2f(4)) {
			dtu_obj_log("gate: no creature, obj %d is at the center of segment %d",i,segnum);
			return -1;
		}

	objnum = obj_create(OBJ_ROBOT,type,segnum,&pos,&vmd_identity_matrix,i2f(4),CT_AI,MT_PHYSICS,RT_POLYOBJ);
	if (objnum < 0)
		return -1;
	obj = &Objects[objnum];
	obj->shields = i2f(Monster_info[type].max_hit_points > 0 ? Monster_info[type].max_hit_points : 1);
	obj->ctype.ai_info.behavior = DAIB_WANDER;
	obj->ctype.ai_info.hide_segment = segnum;
	obj->dtu.m_1b0 = DTU_HOSTILE;
	obj->contains_count = 0;
	ail_keep_bits = 1;
	setup_creature(obj);
	ail_keep_bits = 0;
	if (obj->control_type != CT_AI) {
		obj->flags |= OF_SHOULD_BE_DEAD;
		return -1;
	}
	Ail[objnum].gated = 1;

	vm_vec_sub(&v,&ConsoleObject->pos,&pos);
	v.y = 0;
	if (vm_vec_normalize(&v) > 0x100)
		vm_vector_2_matrix(&obj->orient,&v,NULL,NULL);

	dtu_obj_log("gate: obj %d %s (type %d) in segment %d at %.1f %.1f %.1f, hp %d",objnum,dtu_object_name(obj),type,segnum,
		f2fl(pos.x),f2fl(pos.y),f2fl(pos.z),f2i(obj->shields));
	return objnum;
}

// 0x9493A: the floor of the segment opened. A creature that lives wakes up
// and falls by its gravity. The executable gives the other objects a
// movement with gravity. The port puts them on the floor below at once.
void dtu_drop_objects_in_segment(int segnum)
{
	int objnum, next;
	vms_vector floor;

	if (segnum < 0 || segnum > Highest_segment_index)
		return;
	compute_center_point_on_side(&floor,&Segments[segnum],WBOTTOM);

	for (objnum=Segments[segnum].objects;objnum!=-1;objnum=next) {
		object *obj = &Objects[objnum];
		fvi_query fq;
		fvi_info hit;
		vms_vector p0, p1;

		next = obj->next;
		if (obj->type == OBJ_PLAYER || (obj->flags & OF_SHOULD_BE_DEAD))
			continue;
		if (obj->type == OBJ_ROBOT && obj->control_type == CT_AI) {
			dtu_creature_wake(obj,0);
			continue;
		}
		if (obj->type != OBJ_ROBOT && obj->type != OBJ_DRESSING && obj->type != OBJ_MISC && obj->type != OBJ_ARMOR &&
			 obj->type != OBJ_SPELL && obj->type != OBJ_WEAPON)
			continue;

		p0 = obj->pos;
		p0.y = floor.y + F1_0/4;
		if (find_point_seg(&p0,segnum) < 0)
			p0 = obj->pos;
		p1 = p0;
		p1.y -= i2f(400);
		fq.p0 = &p0;
		fq.p1 = &p1;
		fq.startseg = find_point_seg(&p0,segnum);
		fq.rad = 0;
		fq.thisobjnum = objnum;
		fq.ignore_obj_list = NULL;
		fq.flags = 0;
		if (fq.startseg >= 0 && find_vector_intersection(&fq,&hit) == HIT_WALL && hit.hit_seg >= 0) {
			vms_vector np = obj->pos;

			np.y += hit.hit_pnt.y - floor.y;
			if (find_point_seg(&np,hit.hit_seg) >= 0) {
				obj->pos = np;
				obj_relink(objnum,find_point_seg(&np,hit.hit_seg));
				dtu_obj_log("drop: obj %d %s falls by %.1f",objnum,dtu_object_name(obj),f2fl(floor.y - hit.hit_pnt.y));
			}
		}
	}
}

//------------------------------------------------------------------------
// Pushed scenery (pmon.c 0x9FA41, pcons.c 0x9E5BE, the move 0x9FE68)

static int dressing_pushable(object *d)
{
	int p;

	if (d->id >= N_dressing_types)
		return 999;
	p = Dressing_info[d->id].pushable;
	return p > 0 ? p : 999;		// the default of the reader is 999
}

// The body of scenery (0x748D9): radius body_radius[0] in the x-z plane and
// half height body_half_height[0] of its model, else the size.
static void dressing_body(object *d,fix *r,fix *hh)
{
	polymodel *po = obj_model(d);

	if (po && po->dtu_block && (po->box_flags & DTU_BOX_BODY) && po->body_radius && po->body_half_height) {
		*r = po->body_radius[0];
		*hh = po->body_half_height[0];
	}
	else
		*r = *hh = d->size;
}

// The velocity and the thrust of the scenery become 8 times those of the
// mover in x and z; vel.y stays; movement type 1.
static void push_dressing(object *mover,object *d,int strength)
{
	physics_info *pi = &d->mtype.phys_info;
	int n = d-Objects;

	if (d->type != OBJ_DRESSING || strength <= dressing_pushable(d))
		return;
	pi->velocity.x = 8 * mover->mtype.phys_info.velocity.x;
	pi->velocity.z = 8 * mover->mtype.phys_info.velocity.z;
	pi->thrust.x = 8 * mover->mtype.phys_info.thrust.x;
	pi->thrust.z = 8 * mover->mtype.phys_info.thrust.z;
	Dressing_fall[n] = 1;
	Dressing_sig[n] = d->signature;
	Dressing_str[n] = strength;
	// the fastest push of this row of pushes: the mover slows down at the
	// end and its last pushes are slow
	{
		vms_vector v = mover->mtype.phys_info.velocity;

		v.y = 0;
		if (Dressing_slid[n] || vm_vec_mag_quick(&v) > vm_vec_mag_quick(&Dressing_vpush[n]))
			Dressing_vpush[n] = v;
	}
	vm_vec_zero(&Dressing_slide[n]);
	Dressing_slid[n] = 0;
	dtu_obj_log("push: obj %d %s (strength %d) pushes obj %d %s (pushable %d), velocity %.1f %.1f",(int)(mover-Objects),
		dtu_object_name(mover),strength,n,dtu_object_name(d),dressing_pushable(d),f2fl(pi->velocity.x),f2fl(pi->velocity.z));
}

// The body of another object that pushed scenery tests, as an upright
// cylinder about its position: radius r, half height hh. Executable
// (0x3EE70 with 0x748D9): the player has body_radius[0] of model 16 (twice
// the radius of its move query) and the half height of its move query, a
// creature or scenery body_radius[0] and the half height of its model.
// Corrected (Dtu_fix_push_body): the player has the radius of its move
// query, so that scenery and the player stop at the same distance.
static int dressing_other_body(object *o,fix *r,fix *hh)
{
	if (o->flags & (OF_SHOULD_BE_DEAD|OF_DESTROYED))
		return 0;
	if (o->type == OBJ_PLAYER) {
		dtu_body b;

		if (o != ConsoleObject)
			return 0;
		dtu_player_body(&b);
		*r = Dtu_fix_push_body ? b.move_rad : 2*b.move_rad;
		*hh = b.move_rad2;
		return 1;
	}
	if (o->type == OBJ_ROBOT) {
		polymodel *po = obj_model(o);

		if (o->id < N_robot_types && (Monster_info[o->id].flags & MONSTER_F_PASS_THRU))
			return 0;
		if (po && po->dtu_block && (po->box_flags & DTU_BOX_BODY) && po->body_radius && po->body_half_height) {
			*r = po->body_radius[0];
			*hh = po->body_half_height[0];
		}
		else
			*r = *hh = o->size;
		return 1;
	}
	if (o->type == OBJ_DRESSING) {
		if (o->render_type != RT_POLYOBJ)
			return 0;
		if (o->id < N_dressing_types && Dressing_info[o->id].pass_thru)
			return 0;
		dressing_body(o,r,hh);
		return 1;
	}
	return 0;
}

// Dtu_fix_push_body: other scenery is the box of its model (box 1 of the
// rest pose in the x-z plane of the object), not the circle of
// body_radius[0] around the corners of the box: a chair at a table is
// already inside of that circle. Only an upright object has a box: a
// barrel on its side has no box in x-z.
static int dressing_has_box(object *o)
{
	polymodel *po = obj_model(o);

	return po && po->box1_min && po->box1_max && o->orient.uvec.y >= F1_0*9/10;
}

// The move mv of the point a against the box of o about the position c,
// grown by r. *t is the part (0 to 1) of the move up to the box, or -1 for
// no hit. A start inside of the box is a hit at 0 unless the move goes
// away from the center.
static void dressing_box_sweep(object *o,vms_vector *c,vms_vector *a_w,vms_vector *mv,double r,double *t)
{
	polymodel *po = obj_model(o);
	double a[2], m[2], lo[2], hi[2], tin = 0, tout = 1;
	vms_vector rel;
	int k;

	vm_vec_sub(&rel,a_w,c);
	a[0] = f2fl(vm_vec_dot(&rel,&o->orient.rvec));
	a[1] = f2fl(vm_vec_dot(&rel,&o->orient.fvec));
	m[0] = f2fl(vm_vec_dot(mv,&o->orient.rvec));
	m[1] = f2fl(vm_vec_dot(mv,&o->orient.fvec));
	lo[0] = f2fl(po->box1_min[0].x) - r; hi[0] = f2fl(po->box1_max[0].x) + r;
	lo[1] = f2fl(po->box1_min[0].z) - r; hi[1] = f2fl(po->box1_max[0].z) + r;
	*t = -1;
	if (a[0] > lo[0] && a[0] < hi[0] && a[1] > lo[1] && a[1] < hi[1]) {
		double c0 = (lo[0]+hi[0])/2 - a[0], c1 = (lo[1]+hi[1])/2 - a[1];

		if (m[0]*c0 + m[1]*c1 > 0)
			*t = 0;
		return;
	}
	for (k=0;k<2;k++) {
		if (fabs(m[k]) < 1e-9) {
			if (a[k] <= lo[k] || a[k] >= hi[k])
				return;
		}
		else {
			double t0 = (lo[k] - a[k]) / m[k], t1 = (hi[k] - a[k]) / m[k];

			if (t0 > t1) { double s = t0; t0 = t1; t1 = s; }
			if (t0 > tin) tin = t0;
			if (t1 < tout) tout = t1;
			if (tin > tout)
				return;
		}
	}
	*t = tin;
}

// The corners and the mid points of the edges of the box of o about the
// position c, in the plane of the middle of the box.
static void dressing_box_points(object *o,vms_vector *c,vms_vector pts[8])
{
	static const signed char px[8] = {-1,1,1,-1, 0,1,0,-1}, pz[8] = {-1,-1,1,1, -1,0,1,0};
	polymodel *po = obj_model(o);
	vms_matrix m;
	int i;

	vm_copy_transpose_matrix(&m,&o->orient);
	for (i=0;i<8;i++) {
		vms_vector local, ofs;

		local.x = px[i] < 0 ? po->box1_min[0].x : px[i] > 0 ? po->box1_max[0].x : (po->box1_min[0].x + po->box1_max[0].x)/2;
		local.z = pz[i] < 0 ? po->box1_min[0].z : pz[i] > 0 ? po->box1_max[0].z : (po->box1_min[0].z + po->box1_max[0].z)/2;
		local.y = (po->box1_min[0].y + po->box1_max[0].y)/2;
		vm_vec_rotate(&ofs,&local,&m);
		vm_vec_add(&pts[i],c,&ofs);
	}
}

// The move of scenery d from p0 to p1 against the box of scenery o. Two
// boxes: the points of the box of d against the box of o, and the points
// of the box of o against the box of d (the move the other way). Else the
// circle of radius rad. Returns 0 when o has no box.
static int dressing_box_hit(object *d,object *o,vms_vector *p0,vms_vector *p1,fix rad,double *t)
{
	vms_vector mv;

	if (!dressing_has_box(o))
		return 0;
	vm_vec_sub(&mv,p1,p0);
	if (dressing_has_box(d)) {
		vms_vector pts[8], back;
		double best = -1, u;
		int i;

		back = mv;
		vm_vec_negate(&back);
		dressing_box_points(d,p0,pts);
		for (i=0;i<8;i++) {
			dressing_box_sweep(o,&o->pos,&pts[i],&mv,0,&u);
			if (u >= 0 && (best < 0 || u < best))
				best = u;
		}
		dressing_box_points(o,&o->pos,pts);
		for (i=0;i<8;i++) {
			dressing_box_sweep(d,p0,&pts[i],&back,0,&u);
			if (u >= 0 && (best < 0 || u < best))
				best = u;
		}
		*t = best;
		return 1;
	}
	dressing_box_sweep(o,&o->pos,p0,&mv,f2fl(rad),t);
	return 1;
}

// The object that the move of scenery d from p0 to p1 hits first, and the
// part (0 to 1) of the move up to it. rad and rad2 are the radii of the
// query of d. Executable (0x4F7D5): a start point inside of a body is a
// hit at once. Corrected (Dtu_fix_push_body): a body that d is in already
// does not stop a move away from it, so that two bodies that overlap can
// move apart.
static int dressing_hit_object(object *d,vms_vector *p0,vms_vector *p1,fix rad,fix rad2,fix *frac)
{
	int i, best = -1;
	fix best_f = F1_0+1;
	double mx = f2fl(p1->x - p0->x), mz = f2fl(p1->z - p0->z);
	double len2 = mx*mx + mz*mz;

	for (i=0;i<=Highest_object_index;i++) {
		object *o = &Objects[i];
		fix r, hh;
		double cx, cz, R, b, c, t;

		if (o == d || o->type == OBJ_NONE || !dressing_other_body(o,&r,&hh))
			continue;
		if (abs(p0->y - o->pos.y) > hh + rad2)
			continue;
		if (Dtu_fix_push_body && o->type == OBJ_DRESSING && dressing_box_hit(d,o,p0,p1,rad,&t)) {
			if (t < 0)
				continue;
			if (fl2f(t) < best_f) {
				best_f = fl2f(t);
				best = i;
			}
			continue;
		}
		R = f2fl(r + rad);
		cx = f2fl(o->pos.x - p0->x);
		cz = f2fl(o->pos.z - p0->z);
		c = cx*cx + cz*cz - R*R;
		if (c < 0) {
			// the start point is inside of the body
			if (Dtu_fix_push_body && mx*cx + mz*cz <= 0)
				continue;
			t = 0;
		}
		else {
			if (len2 < 1e-9)
				continue;
			b = mx*cx + mz*cz;		// toward the body
			if (b <= 0)
				continue;
			// |m t - c|^2 = R^2: t^2 len2 - 2 t b + |c|^2 - R^2 = 0
			if (b*b - len2*c < 0)
				continue;
			t = (b - sqrt(b*b - len2*c)) / len2;
			if (t < 0 || t > 1)
				continue;
		}
		if (fl2f(t) < best_f) {
			best_f = fl2f(t);
			best = i;
		}
	}
	*frac = best_f;
	return best;
}

// Dtu_fix_push_wall: the part (0 to 1) of the move frame_vec of scenery d
// that keeps its box out of the walls. The executable tests only the
// center with half of body_radius[0] (rad + 1.0 for a wall), so a table
// (body_radius 11.2, 10 from the center to an end) goes 3.4 into a wall.
// The port moves the four corners and the four mid points of the edges of
// the box of the model (box 1 of the rest pose, at the height of its
// middle) with the move and stops at
// the first wall that one of them hits, DTU_PUSH_WALL_GAP before it. A
// point that is outside of the mine at the start (scenery that the level
// puts into a wall) is not tested.
#define DTU_PUSH_WALL_GAP	(F1_0/4)

static fix dressing_wall_frac(object *d,vms_vector *frame_vec)
{
	polymodel *po = obj_model(d);
	fix best = F1_0, len = vm_vec_mag(frame_vec);
	vms_matrix m;
	int i;

	if (len <= 0)
		return F1_0;
	if (!po || !po->box1_min || !po->box1_max) {
		// no box: a sphere of the size of the object
		fvi_query fq;
		fvi_info hit;
		vms_vector p1;

		vm_vec_add(&p1,&d->pos,frame_vec);
		fq.p0 = &d->pos; fq.p1 = &p1; fq.startseg = d->segnum; fq.rad = d->size;
		fq.thisobjnum = d-Objects; fq.ignore_obj_list = NULL; fq.flags = 0;
		if (find_vector_intersection(&fq,&hit) != HIT_NONE)
			return 0;
		return F1_0;
	}
	vm_copy_transpose_matrix(&m,&d->orient);
	for (i=0;i<8;i++) {
		static const signed char px[8] = {-1,1,1,-1, 0,1,0,-1}, pz[8] = {-1,-1,1,1, -1,0,1,0};
		vms_vector local, ofs, p0, p1;
		fvi_query fq;
		fvi_info hit;
		int seg, fate;

		local.x = px[i] < 0 ? po->box1_min[0].x : px[i] > 0 ? po->box1_max[0].x : (po->box1_min[0].x + po->box1_max[0].x)/2;
		local.z = pz[i] < 0 ? po->box1_min[0].z : pz[i] > 0 ? po->box1_max[0].z : (po->box1_min[0].z + po->box1_max[0].z)/2;
		local.y = (po->box1_min[0].y + po->box1_max[0].y)/2;
		vm_vec_rotate(&ofs,&local,&m);
		vm_vec_add(&p0,&d->pos,&ofs);
		seg = find_point_seg(&p0,d->segnum);
		if (seg < 0)
			continue;
		vm_vec_add(&p1,&p0,frame_vec);
		fq.p0 = &p0; fq.p1 = &p1; fq.startseg = seg; fq.rad = 0;
		fq.thisobjnum = d-Objects; fq.ignore_obj_list = NULL; fq.flags = 0;
		fate = find_vector_intersection(&fq,&hit);
		if (fate == HIT_WALL) {
			fix f = fixdiv(vm_vec_dist(&hit.hit_pnt,&p0) - DTU_PUSH_WALL_GAP,len);

			if (f < 0)
				f = 0;
			if (f < best)
				best = f;
		}
		else if (fate != HIT_NONE)
			best = 0;
	}
	return best;
}

// 0x9FE68: the move of scenery with movement type 1. The velocity is
// halved and x and z are divided by 4, then gravity and the move. The
// sound 2706 plays while the speed is above 0x70. The move ends at a stop.
//
// The slow down is in each frame, as in the executable. The push gives 8
// times the velocity of the mover, so the scenery moves the step of the
// mover in the frame after the push and one eighth of it in the next. The
// mover that walks into scenery stays where it is in the frame of a hit
// and walks one step in the next, so mover and scenery go on at one half
// of the walk speed, at each frame rate: all distances of that cycle are
// in proportion to the step. (An earlier version of the port slowed the
// scenery down once in each reference frame; above 21 frames per second the
// scenery then kept the speed of the mover for some frames and ran ahead,
// and the push grew with the frame rate.)
static void dressing_move_frame(void)
{
	int i;

	for (i=0;i<=Highest_object_index;i++) {
		object *d = &Objects[i];
		physics_info *pi = &d->mtype.phys_info;
		vms_vector p1, frame_vec;
		fix speed, frac, br, bhh;
		int hit_obj;

		if (!Dressing_fall[i])
			continue;
		if (d->type != OBJ_DRESSING || d->signature != Dressing_sig[i] || (d->flags & OF_SHOULD_BE_DEAD)) {
			Dressing_fall[i] = 0;
			continue;
		}
		pi->velocity.x = (pi->velocity.x / 2) >> 2;
		pi->velocity.z = (pi->velocity.z / 2) >> 2;
		pi->velocity.y /= 2;
		// gravity (0x6C662): down to the floor, 3.0 times the distance
		{
			fvi_query gq;
			fvi_info gh;
			vms_vector down = d->pos;

			down.y -= i2f(200);
			gq.p0 = &d->pos; gq.p1 = &down; gq.startseg = d->segnum; gq.rad = 0;
			gq.thisobjnum = i; gq.ignore_obj_list = NULL; gq.flags = 0;
			if (find_vector_intersection(&gq,&gh) == HIT_WALL) {
				fix fd = d->pos.y - gh.hit_pnt.y - d->size;
				if (fd > F1_0/16)
					pi->velocity.y -= fixmul(fd,i2f(3));
				else
					pi->velocity.y = 0;
			}
		}
		speed = vm_vec_mag_quick(&pi->velocity);
		// ASCENT: Dtu_fix_push_rate. The push has ended: the slide to the
		// lead of the reference frame, 1.843 * velocity * (reference frame
		// time - frame time), with a velocity that goes down by e in each
		// reference frame time.
		if (speed <= 0x70 && Dtu_fix_push_rate && !Dressing_slid[i]) {
			Dressing_slid[i] = 1;
			vm_vec_zero(&Dressing_slide[i]);
			if (FrameTime > 0 && FrameTime < DTU_REF_FRAMETIME) {
				double dt = f2fl(FrameTime), tref = f2fl(DTU_REF_FRAMETIME);
				double lead = DTU_PUSH_LEAD * (tref - dt);
				double k = lead * (1 - exp(-dt / tref)) / dt;

				Dressing_slide[i].x = fl2f(f2fl(Dressing_vpush[i].x) * k);
				Dressing_slide[i].z = fl2f(f2fl(Dressing_vpush[i].z) * k);
			}
		}
		if (Dtu_fix_push_rate && Dressing_slid[i]) {
			vm_vec_add2(&pi->velocity,&Dressing_slide[i]);
			speed = vm_vec_mag_quick(&pi->velocity);
		}
		if (speed <= 0x70) {
			vm_vec_zero(&pi->velocity);
			vm_vec_zero(&Dressing_slide[i]);
			Dressing_fall[i] = 0;
			dtu_obj_log("push: obj %d %s stops at %.2f %.2f %.2f",i,dtu_object_name(d),f2fl(d->pos.x),f2fl(d->pos.y),f2fl(d->pos.z));
			continue;
		}
		vm_vec_copy_scale(&frame_vec,&pi->velocity,FrameTime);
		// 0xA007F: the executable doubles x and z of the move, each one in
		// 1 of 7 frames (rand() % 7 == 2)
		if (!Dtu_fix_push_body) {
			if (rand() % 7 == 2)
				frame_vec.x *= 2;
			if (rand() % 7 == 2)
				frame_vec.z *= 2;
		}
		dressing_body(d,&br,&bhh);

		// The bodies of the player, the creatures and other scenery
		// (0x3EE70). Executable: the query of the move has the radius
		// body_radius[0] / 2 (0x9FFD6), and a hit stops the move. The hit
		// of scenery pushes it with the velocity of this one (0xA02A8, no
		// test of "pushable"). Corrected (Dtu_fix_push_body): the whole
		// radius, other scenery is a box, and other scenery is pushed only
		// with the strength of the first push.
		vm_vec_add(&p1,&d->pos,&frame_vec);
		hit_obj = dressing_hit_object(d,&d->pos,&p1,Dtu_fix_push_body ? br : br/2,bhh,&frac);
		if (hit_obj >= 0) {
			object *o = &Objects[hit_obj];

			if (o->type == OBJ_DRESSING) {
				// the velocity of this frame, 8 times (0x9E5F1)
				vms_vector v = pi->velocity, t = pi->thrust;
				int str = Dtu_fix_push_body ? Dressing_str[i] : 0x7FFF;

				push_dressing(d,o,str);
				pi->velocity = v; pi->thrust = t;
			}
			if (!Dtu_fix_push_body)
				frac = 0;
			else {
				// stop at the body, a little before it
				fix len = vm_vec_mag(&frame_vec);

				if (len > 0)
					frac -= fixdiv(DTU_PUSH_WALL_GAP,len);
				if (frac < 0)
					frac = 0;
			}
		}
		else
			frac = F1_0;

		// The walls
		if (frac > 0) {
			if (Dtu_fix_push_wall) {
				vms_vector v;
				fix fw;

				vm_vec_copy_scale(&v,&frame_vec,frac);
				fw = dressing_wall_frac(d,&v);
				if (fw < F1_0) {
					frac = fixmul(frac,fw);
					hit_obj = -2;
				}
			}
			else {
				// executable: the query of the center with the radius
				// body_radius[0] / 2, the wall sides take rad + 1.0
				fvi_query fq;
				fvi_info hit;
				int fate;

				fq.p0 = &d->pos; fq.p1 = &p1; fq.startseg = d->segnum; fq.rad = br/2;
				fq.thisobjnum = i; fq.ignore_obj_list = NULL; fq.flags = 0;
				Fvi_dtu_body = 1;
				Fvi_dtu_rad2 = bhh;
				fate = find_vector_intersection(&fq,&hit);
				Fvi_dtu_body = 0;
				if (fate != HIT_NONE) {
					frac = 0;
					hit_obj = -2;
					if (fate == HIT_WALL && hit.hit_wallnorm.y > F1_0/2)
						pi->velocity.y = 0;
				}
			}
		}

		if (frac > 0) {
			int seg;

			vm_vec_scale_add(&p1,&d->pos,&frame_vec,frac);
			seg = find_point_seg(&p1,d->segnum);
			if (seg >= 0) {
				d->pos = p1;
				if (seg != d->segnum)
					obj_relink(i,seg);
			}
		}
		if (Dtu_fix_push_rate && Dressing_slid[i]) {
			// the slide is not a part of the velocity that the next frame
			// slows down
			fix f = fl2f(exp(-f2fl(FrameTime) / f2fl(DTU_REF_FRAMETIME)));

			pi->velocity.x -= Dressing_slide[i].x;
			pi->velocity.z -= Dressing_slide[i].z;
			Dressing_slide[i].x = fixmul(Dressing_slide[i].x,f);
			Dressing_slide[i].z = fixmul(Dressing_slide[i].z,f);
		}
		if (hit_obj != -1) {
			// a stop at a wall or a body (0xA00DE: after 5 tries the
			// velocity is 0)
			pi->velocity.x = pi->velocity.z = 0;
			vm_vec_zero(&Dressing_slide[i]);
		}
		if (FrameCount % 8 == 0)
			sound_at(2706,d->segnum,&d->pos,F1_0/2);
	}
}

// ASCENT: collide_robot_and_player 0x26FE0 to 0x27054: a creature of the
// rat type touches the player, and the player wears boots (the item
// index of the boot slot, byte P+0x1AEC, is not negative). Only the move
// of the rat finds the contact: each query skips a pass_thru creature
// (flag 0x40, 0x3F38F), so the player walks through rats. vclip 9 at the
// rat with its size (0x3BD4C), then 0x25ADF: the rat is hostile, inactive
// and goes to the center of a random segment whose bottom side has
// pad & 3 not 1 or 3 and the texture number 0 or less,
// with the behavior wander (0x1038B), animation state 1 and object flags
// 0. After 100 tries with no segment the rat plays its die sound
// (0x2E900) and is removed (0x6A3CC). No experience, no corpse.
static void rat_squash(object *rat)
{
	int n = rat-Objects, tries, seg = -1;
	vms_vector pos;

	vclip_at(rat->segnum,&rat->pos,rat->size,9);
	rat->dtu.m_1b0 |= DTU_HOSTILE;
	for (tries=0;tries<100 && seg < 0;tries++) {
		int s = Highest_segment_index > 0 ? rand() % Highest_segment_index : 0;
		int floor = Segments[s].sides[WBOTTOM].pad & 3;

		if (floor == 1 || floor == 3 || Segments[s].sides[WBOTTOM].tmap_num > 0)
			continue;
		compute_segment_center(&pos,&Segments[s]);
		if (find_point_seg(&pos,s) < 0)
			continue;
		seg = s;
	}
	if (seg < 0) {
		dtu_obj_log("rat: obj %d is squashed and removed",n);
		if (Monster_info[rat->id].die_sound > 0)
			sound_at(Monster_info[rat->id].die_sound,rat->segnum,&rat->pos,F1_0);
		rat->flags |= OF_SHOULD_BE_DEAD;
		return;
	}
	dtu_obj_log("rat: obj %d is squashed and goes to segment %d",n,seg);
	rat->pos = pos;
	obj_relink(n,seg);
	rat->ctype.ai_info.behavior = DAIB_WANDER;
	ail_keep_bits = 1;
	init_creature_ai(rat);
	ail_keep_bits = 0;
	DTU_ANIM_STATE(rat) = DTU_ANIM_GOAL(rat) = DAS_STAND;
	DTU_ANIM_FRAME(rat) = Dtu_anim_first[DAS_STAND];
	rat->flags &= ~OF_DTU_NOHIT;
}

int dtu_collide_two_objects(object *a, object *b, vms_vector *collision_point)
{
	object *t;

	if (!Dtu_objects_enabled)
		return 0;

	if (a->flags & OF_SHOULD_BE_DEAD)
		return 1;

	// the player is "a"
	if (b->type == OBJ_PLAYER && a->type != OBJ_PLAYER) {
		t = a; a = b; b = t;
	}

	if (a->type == OBJ_PLAYER) {
		switch (b->type) {
			case OBJ_MISC:
			case OBJ_ARMOR:
			case OBJ_SPELL:
			case OBJ_WEAPON:
				pick_up_item(a,b);
				break;
			case OBJ_ROBOT:
				// ASCENT: 0x26FE0: a rat that touches the player with boots
				// is squashed
				if (dtu_rat_type(b->id) && Dtu_char.slot[DTU_SLOT_BOOT].n >= 0 && Dtu_char.slot[DTU_SLOT_BOOT].n < Dtu_n_inventory) {
					rat_squash(b);
					break;
				}
				// collide_robot_and_player (0x26FC1): a hostile creature
				// that the player touches is alerted. The bump (0x2627C) is
				// not in the port: the bodies stop in the movement code.
				if (is_hostile(b) && Ail[b-Objects].awareness < 3) {
					Ail[b-Objects].awareness = 3;
					Ail[b-Objects].awareness_time = i2f(3);
				}
				// 0x2707A: the alert function 0x160AB(creature, 3)
				if (is_hostile(b))
					dtu_creature_alert(b,3);
				break;
			case OBJ_DRESSING:
				// 0x9E5BE (pcons.c): the player pushes scenery when the
				// strength (P+0xB7) is above "pushable", at each hit. The
				// count of 0x1452F1 (hits of the object 0x1452F5 in a row)
				// decides only whether the player goes back to the start of
				// the frame (0x9E598), not the push. The player stays out
				// of the scenery (physics.c).
				push_dressing(a,b,Dtu_char.created ? Dtu_char.pa.strength : 18);
				break;
		}
	}

	(void)collision_point;
	return 1;
}

//------------------------------------------------------------------------
// The player

// ASCENT: Dtu_fix_revive. Step 10 of do_ai_frame (0x1440C to 0x14655) for
// the corpse of a creature with "regeneration" that kept its status bit
// 0x01000000: one hit point each "regeneration" seconds, and with all hit
// points, the player farther away than the sum of both body radii, the
// creature gets up: CURRENT_STATE REST, GOAL_STATE SRCH, animation REST
// with the backward flag, activity 0, the effect Sleep cleared, hostile,
// not fleeing, awareness 4, the morale check, the alert of level 4, and it
// turns to the player. The executable runs this in do_ai_frame, which does
// not return early for a dead creature with regeneration (0x13C96). There
// the hit points go into the object (+0x6C); the port keeps them apart
// until the creature is up, so that a corpse has no hit points.
static void corpse_regen_frame(void)
{
	int i;

	if (!Dtu_fix_revive)
		return;

	for (i=0;i<=Highest_object_index;i++) {
		object *obj = &Objects[i];
		dtu_ai_local *ail = &Ail[i];
		ai_static *aip = &obj->ctype.ai_info;
		fix r1, h1, r2, h2;
		vms_vector v;
		int regen, max;

		if (obj->type != OBJ_ROBOT || !(obj->flags & OF_DESTROYED) || (obj->flags & (OF_EXPLODING|OF_SHOULD_BE_DEAD)))
			continue;
		if (obj->id >= N_robot_types || !(obj->dtu.m_88 & 0x01000000) || aip->CURRENT_STATE != DAIS_DEAD || DTU_ANIM_STATE(obj) != DAS_DIE)
			continue;
		regen = Monster_ext[obj->id].regeneration;
		max = Monster_info[obj->id].max_hit_points;
		if (regen <= 0 || max <= 0)
			continue;

		if (GameTime < ail->regen_time)
			ail->regen_time = GameTime;
		if (GameTime - ail->regen_time < i2f(regen))
			continue;
		ail->regen_time = GameTime;
		if (ail->regen_hp < i2f(max)) {
			ail->regen_hp += F1_0;
			if (f2i(ail->regen_hp) % 10 == 0 || ail->regen_hp >= i2f(max))
				dtu_obj_log("obj %d %s: the corpse regenerates, hp %d of %d",i,dtu_object_name(obj),f2i(ail->regen_hp),max);
		}
		if (ail->regen_hp < i2f(max) || ConsoleObject == NULL)
			continue;

		// 0x1449C: not while the player stands on the body
		dtu_get_body_size(obj,&r1,&h1);
		dtu_get_body_size(ConsoleObject,&r2,&h2);
		if (vm_vec_dist_quick(&obj->pos,&ConsoleObject->pos) <= r1 + r2)
			continue;

		obj->flags &= ~OF_DESTROYED;
		obj->control_type = CT_AI;
		obj->movement_type = MT_PHYSICS;
		obj->shields = i2f(max);
		ail->regen_hp = 0;
		ail->flags |= AILF_BACKWARD;
		ail->anim_start = GameTime + FrameTime;
		aip->CURRENT_STATE = DAIS_REST;
		aip->GOAL_STATE = DAIS_SRCH;
		DTU_ANIM_GOAL(obj) = DAS_REST;
		set_anim(obj,DAS_REST);
		ail->activity = 0;
		obj->dtu.m_88 &= ~DTU_FX_SLEEP;
		obj->dtu.m_1b0 = (obj->dtu.m_1b0 | DTU_HOSTILE) & ~DTU_FLEEING;
		ail->awareness = 4;
		dtu_obj_log("obj %d %s: gets up again with %d hit points (Dtu_fix_revive)",i,dtu_object_name(obj),max);
		HUD_init_message("%s gets up again.",dtu_object_display_name(obj));		// ASCENT: the message of the revive
		morale_check(obj);
		dtu_creature_alert(obj,4);		// 0x14602: the alert of level 4
		dtu_spell_creature_event(obj,DTU_CSC_ALERT);
		// 0x1463C: it faces the player
		vm_vec_sub(&v,&ConsoleObject->pos,&obj->pos);
		v.y = 0;
		if (vm_vec_mag_quick(&v) > F1_0/16) {
			vm_vec_normalize_quick(&v);
			vm_vector_2_matrix(&obj->orient,&v,NULL,NULL);
		}
		log_creature_state(obj,"revive");
	}
}

void dtu_objects_frame(void)
{
	if (!Dtu_objects_enabled)
		return;

	if (Test_pos_pending)
		test_place_player();

	// 0x4D81A
	Dtu_clock += FrameTime;
	if (Dtu_clock >= i2f(3600))
		Dtu_clock -= i2f(3600);

	Ref_tick_acc += FrameTime;
	Ref_ticks_now = Ref_tick_acc / DTU_REF_FRAMETIME;
	Ref_tick_acc -= Ref_ticks_now * DTU_REF_FRAMETIME;
	if (Ref_ticks_now > 8)
		Ref_ticks_now = 8;
	Ref_tick_count += Ref_ticks_now;

	dtu_player_check_death(NULL);
	missiles_frame();
	dressing_move_frame();
	end_sequence_frame();			// ASCENT: the end of the game (0xA6786)
	corpse_regen_frame();
	awareness_events_frame();		// 0x16366 of the game frame (0x438AC)

	// The option -testkill N (test): each N frames the active creature that
	// is nearest to the player dies, for the test of the corpses.
	if (Test_kill > 0 && FrameCount > 0 && (FrameCount % Test_kill) == 0) {
		int i, best = -1;
		fix best_d = 0;

		for (i=0;i<=Highest_object_index;i++) {
			object *o = &Objects[i];
			fix d;

			if (o->type != OBJ_ROBOT || o->control_type != CT_AI || Ail[i].activity != 2 || DTU_ANIM_STATE(o) == DAS_DIE)
				continue;
			if (o->flags & (OF_EXPLODING|OF_SHOULD_BE_DEAD|OF_DESTROYED))
				continue;
			if (Test_kill_wall && (Ail[i].log_wall_time == 0 || GameTime - Ail[i].log_wall_time > F1_0/2))
				continue;
			d = vm_vec_dist(&o->pos,&ConsoleObject->pos);
			if (best < 0 || d < best_d) {
				best = i;
				best_d = d;
			}
		}
		if (best >= 0) {
			dtu_obj_log("testkill: obj %d %s at %.2f %.2f %.2f seg %d",best,dtu_object_name(&Objects[best]),
				f2fl(Objects[best].pos.x),f2fl(Objects[best].pos.y),f2fl(Objects[best].pos.z),Objects[best].segnum);
			dtu_damage_creature_direct(&Objects[best],ConsoleObject,i2f(10000));
		}
	}

	// The option -objlogpos N (test): the place of each active creature
	if (Obj_log && Obj_log_pos > 0 && (FrameCount % Obj_log_pos) == 0) {
		int i;

		for (i=0;i<=Highest_object_index;i++) {
			object *o = &Objects[i];

			// a dying body: the height of its lowest drawn point
			if (o->type == OBJ_ROBOT && DTU_ANIM_STATE(o) == DAS_DIE && !(o->flags & OF_DESTROYED))
				log_corpse_height(o,"dying");
			if (o->type == OBJ_ROBOT && o->control_type == CT_AI && Ail[i].activity == 2)
				dtu_obj_log("pos: obj %d %s: seg %d pos %.1f %.1f %.1f dist %.1f mode %s path %d/%d player seg %d anim %s vis %d retry %d blocked %d/%d(%s) hits %d %d %d %d last %d:%d obj %d",i,dtu_object_name(o),o->segnum,
					f2fl(o->pos.x),f2fl(o->pos.y),f2fl(o->pos.z),f2fl(vm_vec_dist(&o->pos,&ConsoleObject->pos)),
					Ai_mode_names[Ail[i].mode % 7],Ail[i].path_idx,Ail[i].path_len,ConsoleObject->segnum,
					anim_name(DTU_ANIM_STATE(o)),Ail[i].previous_visibility,Ail[i].retry_count,Ail[i].blocked_count,Ail[i].blocker,Ail[i].blocker >= 0 ? dtu_object_name(&Objects[Ail[i].blocker]) : "-",
					Ail[i].log_hits[0],Ail[i].log_hits[1],Ail[i].log_hits[2],Ail[i].log_hits[3],Ail[i].log_hit_seg,Ail[i].log_hit_side,Ail[i].log_hit_obj);
				memset(Ail[i].log_hits,0,sizeof(Ail[i].log_hits));
		}
	}
}

//------------------------------------------------------------------------
// ASCENT: the AI state of a creature in a state file of the original
// (SAVE\TEMP<L>.LVL, docs/re/levelstate.md): the 47-byte ai_local record
// (0x153EB0) and the fields of the 479-byte object image that the AI
// uses (docs/re/ai.md).

static int img_s16(const ubyte *p) { return (short)(p[0] | (p[1] << 8)); }
static int img_s32(const ubyte *p) { return (int)((uint)p[0] | ((uint)p[1] << 8) | ((uint)p[2] << 16) | ((uint)p[3] << 24)); }
static void img_put16(ubyte *p, int v) { p[0] = (ubyte)v; p[1] = (ubyte)(v >> 8); }
static void img_put32(ubyte *p, int v) { p[0] = (ubyte)v; p[1] = (ubyte)(v >> 8); p[2] = (ubyte)(v >> 16); p[3] = (ubyte)(v >> 24); }

void dtu_ai_to_image(int objnum, ubyte *al, ubyte *img)
{
	dtu_ai_local *ail = &Ail[objnum];

	al[0x00] = ail->awareness;
	al[0x01] = ail->retry_count;
	al[0x03] = ail->mode;
	al[0x04] = ail->previous_visibility;
	al[0x05] = ail->flags;
	img_put16(al+0x06,ail->goal_segment);
	img_put32(al+0x10,ail->awareness_time);
	img_put32(al+0x14,ail->time_player_seen);
	img_put32(al+0x18,ail->time_attack_sound);
	img_put32(al+0x1C,ail->next_misc_sound);
	img_put32(al+0x20,ail->time_since_processed);
	img_put32(al+0x24,ail->anim_start);
	img_put32(al+0x28,ail->morale);

	img[0x1CC] = ail->activity;
	img[0x1AF] = ail->swing_done;
	img[0x1AE] = (img[0x1AE] & ~0x20) | (ail->falling ? 0x20 : 0);
	img_put16(img+0x138,ail->blocker);
	img_put16(img+0x13A,ail->blocked_count);
	img_put32(img+0x1A2,ail->target);
	img_put32(img+0x8C,ail->regen_time);
	img_put32(img+0x1C0,ail->spell_time);
}

void dtu_ai_from_image(int objnum, const ubyte *al, const ubyte *img)
{
	object *obj = &Objects[objnum];
	dtu_ai_local *ail = &Ail[objnum];
	ai_static *aip = &obj->ctype.ai_info;
	int s = img_s16(img+0x170), f = img_s32(img+0x172), g = img_s32(img+0x176);

	// A creature that setup_creature() found dead stays so.
	if (obj->type != OBJ_ROBOT || obj->control_type != CT_AI)
		return;
	ail->awareness = al[0x00] > 5 ? 5 : al[0x00];
	ail->retry_count = al[0x01];
	ail->previous_visibility = al[0x04];
	ail->flags = al[0x05];
	ail->goal_segment = img_s16(al+0x06);
	ail->awareness_time = img_s32(al+0x10);
	ail->time_player_seen = img_s32(al+0x14);
	ail->time_attack_sound = img_s32(al+0x18);
	ail->next_misc_sound = img_s32(al+0x1C);
	ail->time_since_processed = img_s32(al+0x20);
	ail->anim_start = img_s32(al+0x24);
	ail->morale = img_s32(al+0x28);
	// The port keeps the path in its own list, not in Point_segs: a
	// creature that walked a path plans a new one.
	if (al[0x03] == DAIM_STILL || al[0x03] == DAIM_WANDER || al[0x03] == DAIM_CHASE_OB || al[0x03] == DAIM_RUN_FROM)
		ail->mode = al[0x03];
	ail->path_len = ail->path_idx = 0;

	ail->activity = img[0x1CC] == 2 ? 2 : 0;
	ail->swing_done = img[0x1AF];
	ail->falling = (img[0x1AE] & 0x20) != 0;
	ail->target = img_s32(img+0x1A2);
	if (ail->target != 0)
		ail->target = -1;
	ail->regen_time = img_s32(img+0x8C);
	// 0xAE3F2 at the restore (0xBB1A7) sets +0x1A8 and +0x1C0 to the time
	// of the restore, after the copy of the saved object
	ail->spell_time = GameTime;
	// The hostile and script bits as the game left them (setup_creature()
	// made them again from the script of the level).
	obj->dtu.m_1b0 = img_s32(img+0x1B0);

	// The AI state and the animation, if the values fit the model
	if (img[0x142] <= DAIS_RECO && img[0x143] <= DAIS_RECO) {
		aip->CURRENT_STATE = img[0x142];
		aip->GOAL_STATE = img[0x143];
	}
	if (s >= 0 && s < DTU_N_ANIM_STATES && s != DAS_DIE && f >= Dtu_anim_first[s] && f < Dtu_anim_first[s+1]
		&& g >= 0 && g < DTU_N_ANIM_STATES && g != DAS_DIE) {
		DTU_ANIM_STATE(obj) = s;
		DTU_ANIM_FRAME(obj) = f;
		DTU_ANIM_GOAL(obj) = g;
	}

	// ASCENT: a creature on a path (AI mode 2). 1.3 keeps the path in
	// Point_segs of the state file, so the creature walks on after a load.
	// The port makes the path to the goal segment again, and a walk of a
	// script walks on and runs the script at its end. Statement E5
	// (0xAA74E) makes the segment both the station (+0x14C) and the goal
	// (ai_local +0x06) of a creature with a script; the bits 0x30 that
	// 0xAA810 sets do not mark it, the end of an animation state clears
	// them.
	if (al[0x03] == DAIM_FOL_PATH && ail->goal_segment >= 0 && ail->goal_segment <= Highest_segment_index &&
		 DTU_ANIM_STATE(obj) != DAS_DIE) {
		ail->mode = DAIM_FOL_PATH;
		create_path(obj);
		if (aip->hide_segment == ail->goal_segment && (obj->dtu.m_1b0 & DTU_SCRIPT_MASK) != 0) {
			extern int Dtu_fix_script_walk;		// dtuscript.c

			dtu_script_walk_start(obj,Dtu_fix_script_walk);
		}
	}
}
