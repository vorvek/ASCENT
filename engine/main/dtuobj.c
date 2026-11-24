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
#include "texmap.h"	// ASCENT: Interpolation_method, Dtu_fix_model_mapper
#include "dtuchar.h"
#include "digi.h"
#include "sounds.h"
#include "dtufoot.h"		// ASCENT: the footsteps of the creatures
#include "fireball.h"
#include "palette.h"
#include "dtumusic.h"
#include "key.h"		// ASCENT: the key that ends the wait of the death
#include "dtuchar.h"
#include "dtustep.h"
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
int Dtu_fix_npc_walk = 1;		// dtuobj.h
int Dtu_fix_npc_body = 1;		// dtuobj.h
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
// With 0 and no BRN file each bit is 0 (D-119): ai_brain_load (0x97774)
// clears its 80000 byte buffer at 0x977AE and reads nothing when the file
// does not open (0x977F4), and 0x9810D does not test the flag 0x142958. So
// no creature sees the player in such a level. The option -origobj clears
// the setting.
int Dtu_fix_brain = 1;

// The morale of a creature of the level file (D-111). Original: the
// Descent 1 init does nothing at a level start: init_ai_objects (0x1065D)
// skips each robot at 0x10685, and init_ai_object (0x1038B) returns for any
// other object. A level creature keeps the morale (ai_local +0x28) that
// its object number had before (from an earlier level, 0 at the start of
// the program) until its first morale check (0x10ADF), and the script test
// 0x98 (dtu_script_creature_brave) can read it. Corrected (1, default):
// the morale is the mean of the flee and the attack value at the setup
// (init_creature_ai). The option -origobj clears it.
int Dtu_fix_ai_init = 1;

// A flying creature that walks into a wall (D-181). Original: 0x25D43 in
// collide_robot_and_wall (0x25CAA) writes 0 to the circle distance of the
// difficulty (Robot_info+0x48) of its type when the type has the flag 0x20
// (flying): each creature of the type then comes to the player and does
// not keep its distance. Corrected (1, default): the table stays. The
// option -origobj clears it.
int Dtu_fix_wall_circle = 1;

// A missile that hits a creature (D-180). Original: when the hit takes
// hit points and the monster_hit_vclip of the weapon is below 0 (0x277CD),
// collide_robot_and_weapon (0x272DD) skips maybe_kill_weapon (0x2709A),
// and only a missile that is a model is removed (0x27883). So a missile
// that is a vclip goes on. Corrected (1, default): each missile goes away
// at the hit. The option -origobj clears it.
int Dtu_fix_hit_keep = 1;

// The burst of a weapon (D-182). Original: combat_weapon_burst (0x93294)
// writes base + rand() % vclip_range to Weapon_info.weapon_vclip before
// each obj_create (0x93306) and writes the base back at the end (0x93496).
// When obj_create fails it leaves at 0x9333B before that, so the weapon
// type keeps a random vclip for its later missiles. Corrected (1,
// default): the base comes back also then. The option -origobj clears it.
int Dtu_fix_burst_vclip = 1;

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
int improved_paths(void)
{
	return Dtu_improved_pathfinding && !Orig_obj;
}

// The reach of the weapon of each model, see model_weapon_reach(). 0: not
// computed, -1: no weapon box. Cleared at the setup of a level.
fix Model_reach[MAX_POLYGON_MODELS];

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

dtu_inv_item Dtu_inventory[DTU_MAX_INVENTORY];
int Dtu_n_inventory = 0;
int Dtu_gold = 0;

// The clock of the player structure (0x1D339F): the sum of the frame times,
// wraps at 3600.0.
fix Dtu_clock = 0;
// The reference ticks (3/64 second) since the start of the level, and how
// many of them belong to this frame.
int Ref_tick_count = 0, Ref_ticks_now = 0;
static fix Ref_tick_acc = 0;

static ubyte *Brain = NULL;			// 800 rows of 100 bytes, or NULL

// ASCENT: the BRN file of the last level is freed at the exit.
static void brain_free(void)
{
	if (Brain)
		free(Brain);
	Brain = NULL;
}
int Player_armor_class = 10;
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
void cloak_reset(void);
void morale_check(object *obj);
int N_aware_events;		// the awareness events of the frame (0x13A6BC)
fix creature_floor_dist(object *obj,fix radius,fix half_height);
void log_corpse_height(object *obj,const char *what);
int corpse_on_floor(object *obj);
static fix corpse_draw_drop(object *obj);
fix corpse_depth(object *obj,int frame,vms_vector *mn,vms_vector *mx);
item_info *item_table_entry(int type,int id);
int dressing_pushable(object *d);
void dressing_level_reset(void);
fix damage_player_w(object *player, object *attacker, fix damage, int weapon);
fix damage_creature_w(object *creature, object *attacker, fix damage, int weapon);
fix spell_hit(int spell,object *target,object *attacker);
void push_dressing(object *mover,object *d,int strength);
int is_hostile(object *obj);
fix model_weapon_reach(object *obj);
fix creature_stop_dist(object *obj);

// The attack of the player
int Player_swing_done = 0;

FILE *Obj_log = NULL;
int Obj_log_frames = 0;		// option -objlogframes: a line for each change of the animation frame
static int Obj_log_pos = 0;			// option -objlogpos N: the place of each active creature each N frames
int Setup_log = 0;				// option -setuplog (test): one session log line for each creature setup by name
static int Test_kill = 0;				// option -testkill N[,W] (test): each N frames the nearest active creature dies
static int Test_kill_wall = 0;		// W 1: only a creature that hit a wall in the last half second
int Test_pos_pending;

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

int is_flying(object *obj)
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
fix state_time(polymodel *po,int state)
{
	if (po == NULL || state < 0 || state >= DTU_N_ANIM_STATES)
		return 0;
	return po->state_times[state];
}

// 0x74AA7. The speed of the movement of an animation state.
fix anim_speed(object *obj,int state)
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
int ai_tick(int objnum,int n)
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
int ray_is_free(vms_vector *p0,int seg0,vms_vector *p1,int objnum)
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
int brain_bit(int a,int b)
{
	if (Dtu_fix_brain)
		return 1;
	if (Brain == NULL)		// D-119: the cleared buffer of 0x97774
		return 0;
	if (a < 0 || a >= 800 || b < 0 || b >= 800)
		return 0;
	return (Brain[a*100 + b/8] >> (b & 7)) & 1;
}

// 0x10815. The ray from a creature to the player. A creature that does not
// fly looks from 5.0 above its center. A blocked ray counts as free for a
// melee creature that is closer than 100.0.
// ASCENT: creature_ray_to() takes the end point (the alert of
// Dtu_fix_alert_ray casts it to the caller).
int creature_ray_to(object *obj,vms_vector *to)
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

int creature_ray_to_player(object *obj)
{
	return creature_ray_to(obj,&ConsoleObject->pos);
}

// 0x9810D(creature segment, player segment) for other code (the Spider
// Piece 0x14): the brain bit; with Dtu_fix_brain a ray.
int dtu_brain_sees_player(object *obj)
{
	if (!Dtu_fix_brain)
		return brain_bit(obj->segnum,ConsoleObject->segnum);
	return ray_is_free(&obj->pos,obj->segnum,&ConsoleObject->pos,obj-Objects);
}

// 0x109E7 player_is_visible_from_object: the brain bit and nothing else in
// the executable. The corrected rule casts a ray.
int player_is_visible(object *obj)
{
	// 0x165C4: Hide In Shadows hides the player (dtuthief.c)
	if (dtu_thief_hidden_from(obj))
		return 0;
	if (!Dtu_fix_brain)
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
		Dtu_fix_idle_anim = Dtu_fix_ai_ticks = Dtu_fix_creature_move = Dtu_fix_brain = Dtu_fix_ai_init =
			Dtu_fix_wall_circle = Dtu_fix_hit_keep = Dtu_fix_burst_vclip =
			Dtu_fix_path_follow = Dtu_fix_fire_once = Dtu_fix_attack_sound = Dtu_fix_cant_break =
			Dtu_fix_push_body = Dtu_fix_push_wall = Dtu_fix_push_rate = Dtu_fix_corpse_floor =
			Dtu_fix_magic_tint = Dtu_fix_guild_peace = Dtu_fix_drow5_shields =
			Dtu_fix_kobold_weapon = Dtu_fix_scenery_restore = Dtu_fix_bane_texture = Dtu_fix_partial_stack = Dtu_fix_revive =
			Dtu_fix_break_spread = Dtu_fix_noise_walls = Dtu_fix_alert_ray = Dtu_fix_alert_last =
			Dtu_fix_water_splash = Dtu_fix_model_mapper = Dtu_fix_rat_die = Dtu_fix_npc_walk = Dtu_fix_npc_body = 0;
	if (FindArg("-orignpcwalk"))
		Dtu_fix_npc_walk = 0;
	if (FindArg("-orignpcbody"))
		Dtu_fix_npc_body = 0;
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

void end_sequence_reset(void);		// ASCENT: the end of the game (0xA6786)

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
	dtu_step_reset();			// ASCENT: Dtu_fixed_step (dtustep.c)
	dressing_level_reset();		// ASCENT: Dtu_improved_physics (dtuobj_scenery.c)
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

				dtu_obj_log("obj %d %s: dressing %d shields %.1f pushable %d mass %.2f drag %.4f size %.2f seg %d pos %.2f %.2f %.2f body flags %d r %.2f hh %.2f box %.2f %.2f %.2f to %.2f %.2f %.2f center y %.2f up %.2f",
					i,dtu_object_name(&Objects[i]),Objects[i].id,f2fl(Objects[i].shields),dressing_pushable(&Objects[i]),
					f2fl(Objects[i].mtype.phys_info.mass),f2fl(Objects[i].mtype.phys_info.drag),f2fl(Objects[i].size),
					Objects[i].segnum,f2fl(Objects[i].pos.x),f2fl(Objects[i].pos.y),f2fl(Objects[i].pos.z),
					po ? po->box_flags : -1,b ? f2fl(po->body_radius[0]) : 0.0,b ? f2fl(po->body_half_height[0]) : 0.0,
					po && po->box1_min ? f2fl(po->box1_min[0].x) : 0.0,po && po->box1_min ? f2fl(po->box1_min[0].y) : 0.0,po && po->box1_min ? f2fl(po->box1_min[0].z) : 0.0,
					po && po->box1_max ? f2fl(po->box1_max[0].x) : 0.0,po && po->box1_max ? f2fl(po->box1_max[0].y) : 0.0,po && po->box1_max ? f2fl(po->box1_max[0].z) : 0.0,
					po && po->body_center ? f2fl(po->body_center[0].y) : 0.0,f2fl(Objects[i].orient.uvec.y));
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
	int show_hidden = 1, im_save;
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

	// Dtu_fix_model_mapper (polyobj.h): with -origobj each object but the
	// viewer is drawn with the linear texture mapper, as 1.3 does (0x675D4
	// to 0x675EC).
	im_save = Interpolation_method;
	if (!Dtu_fix_model_mapper && obj != Viewer)
		Interpolation_method = 1;

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
	}
	else
		draw_polygon_model_frame(&draw_pos,&obj->orient,obj->rtype.pobj_info.model_num,frame,show_hidden,light,glow,alt_textures);

	Interpolation_method = im_save;
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
// Corrected (Dtu_fix_water_splash, 1, default): the port gives the splash
// no light (dtu_object_light, lighting.c) and puts it to the side of the
// walk, on the right vector of the player. The option -origobj clears it:
// the splash lights as vclip 10 and the offset is on world x and z, as in
// 1.3 (0x43510 to 0x4351B).
int Dtu_fix_water_splash = 1;

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
	if (!Dtu_fix_water_splash) {
		// 1.3: 0x100 * side_step on world x and z, and the light of vclip 10
		o->pos.x += side_step << 8;
		o->pos.z += side_step << 8;
		return;
	}
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
fix corpse_depth(object *obj,int frame,vms_vector *mn,vms_vector *mx)
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
