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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

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
#include "dtuchar.h"
#include "digi.h"
#include "sounds.h"
#include "dtufoot.h"		// ASCENT: the footsteps of the creatures
#include "fireball.h"
#include "palette.h"
#include "dtumusic.h"
#include "dtuchar.h"
#include "dtuopts.h"	// ASCENT: Dtu_improved_pathfinding
#include "dtucombat.h"	// ASCENT: the frame of the player for its body
#include "dtumovie.h"
#include "text.h"		// ASCENT: Text_string, the names of the creatures
#include "mem.h"		// ASCENT: dtu_load_file() allocates with mem_malloc, so free() must be mem_free()

#define DTU_WEAK __attribute__((weak))

int check_and_fix_matrix(vms_matrix *m);		// gamesave.c
void make_random_vector(vms_vector *vec);		// ai.c

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

// The option -origobj (or -noimprovedpaths, a test option that changes
// nothing else): Dtu_improved_pathfinding has no effect.
static int Orig_obj = 0;

// Dtu_improved_pathfinding (dtuopts.h): paths for the body of a person, a
// body that stops at the reach of its weapon, and a corpse that lies on the
// floor. See docs/re/ai.md, "Improved pathfinding".
static int improved_paths(void)
{
	return Dtu_improved_pathfinding && !Orig_obj;
}

// The reach of the weapon of each model, see model_weapon_reach(). 0: not
// computed, -1: no weapon box. Cleared at the setup of a level.
static fix Model_reach[MAX_POLYGON_MODELS];

//------------------------------------------------------------------------
// Tables

// 0x13D9E0
short Dtu_anim_first[DTU_N_ANIM_STATES+1] = {
	0, 0, 4, 20, 24, 28, 44, 60, 76, 84, 96, 108, 124, 140, 152, 164, 168, 176, 180, 192 };

// 0x13A956: the bonus of the strength for the to-hit roll
static const signed char Str_to_hit[20] = {
	0, -5, -3, -3, -2, -2, -1, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 3 };

static char *Anim_state_names[] = { "REST", "STAND", "WALK", "SIDLE_R", "SIDLE_L", "RUN",
	"ATTACK", "ATTACK2", "FLINCH", "DIE", "FIDGET" };
static char *Ai_state_names[] = { "NONE", "REST", "SRCH", "LOCK", "FLIN", "ATAK", "RECO", "DEAD" };
static char *Ai_mode_names[] = { "STILL", "WANDER", "FOL_PATH", "CHASE_OB", "RUN_FROM", "HOME", "OPENDOOR" };

// The values of a $MONSTER line that monster_info does not have.
typedef struct monster_ext {
	int	damage_dice, n_damage_dice, to_hit_bonus, damage_bonus;
	int	regeneration;
	int	weapon_type;			// -1 if the line has none (a natural weapon). Below 0: the spell with the number -weapon_type
	int	spell_level;
	int	door_opening;			// +0x63 bit 0, set for each HUMANOID too (0x1061B)
	int	blood_vclip;			// monster_hit_vclip, +0xDA
} monster_ext;

static monster_ext Monster_ext[MAX_ROBOT_TYPES];

// The data of the AI for each object (Ai_local_info at 0x153EB0, and
// fields of the object of the executable that the port has no place for).
#define AILF_BACKWARD	0x02		// play the animation backward (revive)
#define AILF_AROUND		0x04
#define AILF_TOWARD		0x08
#define AILF_AWAY			0x10
#define AILF_HIT			0x20		// the hit landed, end the animation state early

#define DTU_PATH_MAX		32

typedef struct dtu_ai_local {
	ubyte		awareness;				// +0x00
	ubyte		retry_count;			// +0x01
	ubyte		mode;						// +0x03
	ubyte		previous_visibility;	// +0x04
	ubyte		flags;					// +0x05, AILF_
	ubyte		activity;				// object +0x1CC: 0 inactive, 2 active
	ubyte		swing_done;				// object +0x1AF: the swing has hit
	ubyte		falling;					// object +0x1AE bit 0x20
	short		blocker;					// object +0x138
	short		blocked_count;			// object +0x13A
	int		target;					// object +0x1A2: 0 the player, -1 none
	fix		awareness_time;		// +0x10
	fix		time_player_seen;		// +0x14
	fix		time_since_processed;	// +0x20
	fix		anim_start;				// +0x24
	int		morale;					// +0x28
	fix		regen_time;				// object +0x8C
	// values of the corrected rules
	fix		idle_acc;
	int		idle_ticks;
	fix		push_acc;
	fix		step_acc;
	// the velocity that the AI adds in this frame (0x11EC9)
	vms_vector move_add;
	int		has_move;
	int		last_logged_state;
	// The path (the executable keeps the points in Point_segs, 0x16FD9).
	// path[] has the segments, the first is the segment of the creature.
	short		path[DTU_PATH_MAX];
	short		path_len, path_idx;
	signed char	path_dir;			// object +0x144 PATH_DIR
	signed char	path_flag;			// object +0x145: 1 after create_path, 0 path to the player, -1 path of n segments
	short		goal_segment;			// +0x06
	fix		path_fail_time;		// corrected rule: the time of the last search with no result
	fix		time_attack_sound;	// +0x18
	fix		next_misc_sound;		// +0x1C
	fix		spell_time;				// object +0x1C0
	ubyte		fired;					// corrected rule: the shot of this attack animation is done
	ubyte		gated;					// made by a trap (matcen_creator -1 of the executable)
	ubyte		prev_fov;				// corrected rule: the player was in the field of view (visibility 2 of Descent 1)
	// 0x123EF: the executable has one counter, position and direction for
	// all creatures (0x13A79C, 0x13A790, 0x167184)
	int		flee_counter;
	vms_vector flee_pos, flee_dir;
	// Dtu_improved_pathfinding: the point of the path that the creature
	// walks to (see path_target()), the last progress to it, and the number
	// of new plans since the last point that it reached.
	short		path_tgt;
	short		prog_tgt;
	fix		prog_time, prog_dist;
	ubyte		replans;
	short		stall_tgt;				// the point where the creature was blocked
	ubyte		body_line;				// the line to the player is free for the body
	// test log of -objlogpos: the hits of the move since the last line
	short		log_hits[4];			// wall, top, floor, object
	short		log_hit_seg, log_hit_side;
	fix		log_wall_time;			// the game time of the last wall hit (-testkill N,1)
	short		log_hit_obj;			// the object of the last object hit
} dtu_ai_local;

static dtu_ai_local Ail[MAX_OBJECTS];

// A missile (object type OBJ_WEAPON that flies). The entry belongs to the
// object with the same number while the signature is the same.
typedef struct dtu_missile {
	int		signature;				// 0: no missile
	int		parent, parent_sig;	// object +0x142, +0x144. -1: no parent
	short		spell;					// the spell that the missile carries (enemy_spell), or -1
	ubyte		bounce;					// weapon +0x0A: 1 bounce, 2 stick, 3 thrown
	ubyte		harmless;				// object flag 0x20
	fix		tumble_acc;
} dtu_missile;

static dtu_missile Missile[MAX_OBJECTS];

// Scenery that moves (movement type 1 of the executable): pushed scenery
// (0x9FA41, 0x9E5BE) and the scenery that falls after a break (0xA6ACB).
// pcons.c 0x9FE68 moves it.
static ubyte Dressing_fall[MAX_OBJECTS];
static int Dressing_sig[MAX_OBJECTS];
static ubyte Dressing_pushed[MAX_OBJECTS];	// pushed in this frame (Dtu_fix_push_frame)
// The push of scenery depends on the frame rate: 0x9FE68 halves the
// velocity and divides x and z by 4 in each frame. 1 (default): once in
// each reference frame (3/64 second). 0 (-origobj): in each frame.
int Dtu_fix_push_frame = 1;

dtu_inv_item Dtu_inventory[DTU_MAX_INVENTORY];
int Dtu_n_inventory = 0;
int Dtu_gold = 0;

// The clock of the player structure (0x1D339F): the sum of the frame times,
// wraps at 3600.0.
static fix Dtu_clock = 0;
// The reference ticks (3/64 second) since the start of the level, and how
// many of them belong to this frame.
static int Ref_tick_count = 0, Ref_ticks_now = 0;
static fix Ref_tick_acc = 0;

static ubyte *Brain = NULL;			// 800 rows of 100 bytes, or NULL
static int Player_armor_class = 10;
static fix Player_max_hp = 0;

// The death of the player: 0 alive, 1 dead and waiting for the death
// screen, 2 after the death screen.
static int Player_death_stage = 0;
static fix Player_death_time = 0;

static void sound_at(int sound,int segnum,vms_vector *pos,fix volume);
static void sound_on_object(int sound,object *obj,fix volume);
static void vclip_at(int segnum,vms_vector *pos,fix size,int vclip);
static int weapon_int(int w,char *key,int def);
static int player_type_sound(char *key);
static void blood(object *obj);
static void setup_creature(object *obj);
static void morale_check(object *obj);
static void player_dies(object *player,object *attacker);
static fix creature_floor_dist(object *obj,fix radius,fix half_height);
static item_info *item_table_entry(int type,int id);
static int dressing_pushable(object *d);
static fix damage_player_w(object *player, object *attacker, fix damage, int weapon);
static fix damage_creature_w(object *creature, object *attacker, fix damage, int weapon);
static fix spell_hit(int spell,object *target,object *attacker);
static void push_dressing(object *mover,object *d,int strength);
static int is_hostile(object *obj);
static fix model_weapon_reach(object *obj);
static fix creature_stop_dist(object *obj);

// The attack of the player
static int Player_swing_done = 0;

static FILE *Obj_log = NULL;
static int Obj_log_frames = 0;		// option -objlogframes: a line for each change of the animation frame
static int Obj_log_pos = 0;			// option -objlogpos N: the place of each active creature each N frames
static int Test_kill = 0;				// option -testkill N[,W] (test): each N frames the nearest active creature dies
static int Test_kill_wall = 0;		// W 1: only a creature that hit a wall in the last half second
static int Test_pos_pending;

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
		return name;
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

static char *anim_name(int s)
{
	static char buf[16];

	if (s >= 0 && s <= DAS_FIDGET)
		return Anim_state_names[s];
	sprintf(buf,"%d",s);
	return buf;
}

// Writes the state of a creature to the log if it changed.
static void log_creature_state(object *obj,char *why)
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

static polymodel *obj_model(object *obj)
{
	int n = obj->rtype.pobj_info.model_num;

	if (obj->render_type != RT_POLYOBJ || n < 0 || n >= N_polygon_models)
		return NULL;
	if (Polygon_models[n].dtu_block == NULL)
		return NULL;
	return &Polygon_models[n];
}

static int is_hostile(object *obj)
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

static int tbl_int(int record,char *key,int def)
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

static void set_anim(object *obj,int state)
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
static int get_weapon_sphere(object *obj,int frame,vms_vector *center,fix *radius)
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
static fix creature_floor_dist(object *obj,fix radius,fix half_height)
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
static int creature_ray_to_player(object *obj)
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
	if (ray_is_free(&p0,seg,&ConsoleObject->pos,obj-Objects))
		return 1;
	if (Robot_info[obj->id].attack_type == 1 && Monster_ext[obj->id].spell_level == 0)
		if (vm_vec_dist(&obj->pos,&ConsoleObject->pos) < i2f(100))
			return 1;
	return 0;
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
// Inventory

void dtu_inventory_clear(void)
{
	memset(Dtu_inventory,0,sizeof(Dtu_inventory));
	Dtu_n_inventory = 0;
	Dtu_gold = 0;
}

static item_info *item_table_entry(int type,int id)
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
	dtu_inv_item *inv;
	int i;

	if (it == NULL || count <= 0)
		return -1;

	if (item_is_stackable(it))
		for (i=0;i<Dtu_n_inventory;i++)
			if (Dtu_inventory[i].type == type && Dtu_inventory[i].id == id) {
				int limit = it->stack_limit > 0 ? it->stack_limit : 10;

				if (Dtu_inventory[i].count + count > limit)
					return -1;
				Dtu_inventory[i].count += count;
				return i;
			}

	if (Dtu_n_inventory >= DTU_MAX_INVENTORY)
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
	strncpy(inv->name,it->name,sizeof(inv->name)-1);
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

//------------------------------------------------------------------------
// Setup

void dtu_objects_init(void)
{
	int t;

	if (FindArg("-noobjects"))
		Dtu_objects_enabled = 0;
	if (FindArg("-origobj"))
		Dtu_fix_idle_anim = Dtu_fix_ai_ticks = Dtu_fix_creature_move = Dtu_fix_brain =
			Dtu_fix_path_follow = Dtu_fix_fire_once = Dtu_fix_attack_sound = Dtu_fix_cant_break = Dtu_fix_push_frame = 0;
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

// init_ai_object (0x1038B) for a creature of a level, with the values that
// place_creatures (0x6A6FB) and add_npc (0xAC431) set.
static int ail_keep_bits = 0;		// 1 while a creature is made in the game
static int Dtu_script_level = 0;	// the level byte of the player (0x1D338D): the number of the level in DTU.MSN, from 0

static void init_creature_ai(object *obj)
{
	int objnum = obj-Objects;
	dtu_ai_local *ail = &Ail[objnum];
	ai_static *aip = &obj->ctype.ai_info;
	monster_info *mi = &Monster_info[obj->id];

	memset(ail,0,sizeof(*ail));
	ail->last_logged_state = -1;
	ail->blocker = -1;

	if (aip->behavior < DAIB_GUARD || aip->behavior > DAIB_STATION)
		aip->behavior = DAIB_WANDER;
	if (aip->behavior == DAIB_STATION && Robot_info[obj->id].attack_type == 1)
		aip->behavior = DAIB_WANDER;

	ail->mode = (aip->behavior == DAIB_PATROL) ? DAIM_FOL_PATH : DAIM_STILL;
	ail->activity = 0;
	ail->awareness = 0;
	ail->awareness_time = i2f(3);
	ail->morale = (mi->ai_values[0][Difficulty_level] + mi->ai_values[1][Difficulty_level]) / 2;
	ail->anim_start = ail->time_player_seen = ail->regen_time = GameTime;

	vm_vec_zero(&obj->mtype.phys_info.velocity);
	vm_vec_zero(&obj->mtype.phys_info.thrust);
	vm_vec_zero(&obj->mtype.phys_info.rotvel);

	// add_npc (0xAC431, code):
	// 1. A script id that does not belong to the level is cleared: the id
	//    divided by 10 is the number of the level in the mission file. The
	//    ids 0x37, 0x3F and 0x1FC to 0x1FF belong to each level.
	// 2. The creature is peaceful only if it has a script id and the
	//    script has a "say" statement (a byte 0xCD), or the bit "peaceful"
	//    is set.
	// 3. Each other creature is hostile, with the awareness 4.
	// A creature that is made in the game (a trap, a drop) has no level
	// record and keeps its bits.
	if (!ail_keep_bits) {
		int id = obj->dtu.m_1b0 & DTU_SCRIPT_MASK;
		int has_say;

		if (id != 0 && id != 0x37 && id != 0x3f && id < 0x1fc && id / 10 != Dtu_script_level) {
			obj->dtu.m_1b0 &= ~DTU_SCRIPT_MASK;
			id = 0;
		}
		// With no script file each creature with a script id is peaceful.
		has_say = (id != 0) ? dtu_script_has_say(id) : 0;
		if (id != 0 && (has_say != 0 || (obj->dtu.m_1b0 & DTU_PEACEFUL)))
			obj->dtu.m_1b0 &= ~DTU_HOSTILE;
		else
			obj->dtu.m_1b0 |= DTU_HOSTILE;
	}
	if (is_hostile(obj)) {
		ail->target = 0;
		ail->awareness = ail_keep_bits ? 0 : 4;
	}
	else
		ail->target = -1;

	aip->CURRENT_STATE = aip->GOAL_STATE = DAIS_NONE;
}

static void setup_creature(object *obj)
{
	robot_info *ri;
	polymodel *po;

	if (N_robot_types <= 0) {
		obj->render_type = RT_NONE;
		obj->control_type = CT_NONE;
		obj->movement_type = MT_NONE;
		return;
	}

	// verify_object 0x477D9
	if (obj->id >= N_robot_types)
		obj->id = obj->id % N_robot_types;
	ri = &Robot_info[obj->id];

	if (obj->render_type == RT_POLYOBJ) {
		obj->rtype.pobj_info.model_num = ri->model_num;
		po = obj_model(obj);
		if (po == NULL) {
			obj->render_type = RT_NONE;
			obj->control_type = CT_NONE;
			obj->movement_type = MT_NONE;
			return;
		}
		obj->size = po->rad;
	}
	obj->rtype.pobj_info.tmap_override = -1;

	if (obj->movement_type == MT_PHYSICS) {
		obj->mtype.phys_info.drag = ri->drag;
		// setup_object 0xAE3F2: the mass is the size, 4.0 at least
		obj->mtype.phys_info.mass = obj->size;
		if (obj->mtype.phys_info.mass < i2f(4))
			obj->mtype.phys_info.mass = i2f(4);
	}

	// place_creatures 0x6A6FB
	DTU_ANIM_STATE(obj) = DAS_STAND;
	DTU_ANIM_GOAL(obj) = DAS_STAND;
	DTU_ANIM_FRAME(obj) = Dtu_anim_first[DAS_STAND];
	check_and_fix_matrix(&obj->orient);

	if (obj->control_type == CT_AI)
		init_creature_ai(obj);

	// The status bit of a creature that regenerates (set at creation if
	// "regeneration" of the monster table is not 0).
	if (Monster_ext[obj->id].regeneration)
		obj->dtu.m_88 |= 0x01000000;

	// A creature with less than one hit point is loaded dead (0x4787A).
	if (obj->shields < F1_0) {
		obj->ctype.ai_info.CURRENT_STATE = obj->ctype.ai_info.GOAL_STATE = DAIS_DEAD;
		DTU_ANIM_STATE(obj) = DAS_DIE;
		DTU_ANIM_FRAME(obj) = Dtu_anim_first[DAS_DIE+1] - 1;
		obj->flags |= OF_DESTROYED;
		obj->control_type = CT_NONE;
		obj->movement_type = MT_NONE;
	}
}

void dtu_objects_level_setup(void)
{
	int i;
	ubyte *data;
	int size;
	char name[32];

	if (!Dtu_objects_enabled)
		return;

	Dtu_clock = 0;
	Ref_tick_count = Ref_ticks_now = 0;
	Ref_tick_acc = 0;
	memset(Model_reach,0,sizeof(Model_reach));

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
	}
	memset(Missile,0,sizeof(Missile));
	Player_death_stage = 0;
	if (dtu_player_hit_points() > Player_max_hp)
		Player_max_hp = dtu_player_hit_points();

	Dtu_script_level = Current_level_num - 1;

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
		if (data && size >= 80000)
			Brain = data;
		else if (data)
			free(data);
	}

	for (i=0;i<=Highest_object_index;i++) {
		object *obj = &Objects[i];

		switch (obj->type) {

			case OBJ_ROBOT:
				setup_creature(obj);
				break;

			case OBJ_DRESSING:
				// setup_object: shields 1.0. fix_object_segs 0x69FFB: the model
				// or the bitmap of the table entry.
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
			if (Objects[i].type == OBJ_DRESSING)
				dtu_obj_log("obj %d %s: dressing %d pushable %d mass %.2f drag %.4f size %.2f seg %d pos %.2f %.2f %.2f",
					i,dtu_object_name(&Objects[i]),Objects[i].id,dressing_pushable(&Objects[i]),
					f2fl(Objects[i].mtype.phys_info.mass),f2fl(Objects[i].mtype.phys_info.drag),f2fl(Objects[i].size),
					Objects[i].segnum,f2fl(Objects[i].pos.x),f2fl(Objects[i].pos.y),f2fl(Objects[i].pos.z));
			else if (Objects[i].type == OBJ_MISC || Objects[i].type == OBJ_ARMOR || Objects[i].type == OBJ_SPELL || Objects[i].type == OBJ_WEAPON)
				dtu_obj_log("obj %d %s: item type %d id %d seg %d pos %.2f %.2f %.2f",i,dtu_object_name(&Objects[i]),Objects[i].type,Objects[i].id,
					Objects[i].segnum,f2fl(Objects[i].pos.x),f2fl(Objects[i].pos.y),f2fl(Objects[i].pos.z));
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
		draw_polygon_model_frame_lerp(&obj->pos,&obj->orient,obj->rtype.pobj_info.model_num,frame,next,frac,show_hidden,light,glow,alt_textures);
		return;
	}

	draw_polygon_model_frame(&obj->pos,&obj->orient,obj->rtype.pobj_info.model_num,frame,show_hidden,light,glow,alt_textures);
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
// Damage

// 0x2A789 without the percent part of strength 18
static int str_bonus(int str)
{
	if (str < 1) str = 1;
	if (str > 19) str = 19;
	return Str_to_hit[str];
}

// 0x2A7F3. The to-hit roll. Returns 2 for a critical hit, 1 for a hit, 0
// for a miss, -1 for a fumble.
static int to_hit_roll(object *target,object *attacker,int weapon,int *roll_out)
{
	int roll = rand() % 20 + 1;
	int thac0, ac;

	*roll_out = roll;
	if (roll == 20)
		return 2;
	if (roll == 1)
		return -1;

	if (attacker == NULL || (attacker->type != OBJ_PLAYER && attacker->type != OBJ_ROBOT)) {
		// The missile of a trap is its own parent (0x934A8). What the
		// executable reads as the THAC0 of a weapon object is not known. 20
		// is the value of an attacker of the lowest level. (guess)
		thac0 = 20;
		roll += weapon_int(weapon,"to_hit_bonus",0);
	}
	else if (attacker->type == OBJ_PLAYER) {
		// The byte at 0x1D4D78 of the player structure and the bonuses of
		// the strength, the weapon and the skill: the character code.
		thac0 = dtu_player_thac0();
		roll += dtu_player_to_hit_bonus();
	}
	else {
		thac0 = Monster_info[attacker->id].thaco;
		// the bonus of the weapon (Weapon_info + 0x36), or of the creature
		// for a natural weapon
		if (weapon >= 0 && weapon < N_weapon_types && Weapon_item_info[weapon].record >= 0)
			roll += tbl_int(Weapon_item_info[weapon].record,"to_hit_bonus",0);
		else
			roll += Monster_ext[attacker->id].to_hit_bonus;
		roll += str_bonus(Monster_info[attacker->id].str);
	}

	if (target->type == OBJ_PLAYER)
		ac = dtu_player_armor_class();
	else
		ac = Monster_info[target->id].armor_class;

	*roll_out = roll;
	return (thac0 - ac > roll) ? 0 : 1;
}

// The weapon of a melee attack of an object: the weapon_type of the
// monster table, or -1 for a natural weapon.
static int attack_weapon(object *attacker)
{
	if (attacker && attacker->type == OBJ_ROBOT)
		return Monster_ext[attacker->id].weapon_type;
	return -1;
}

// 0x2AF5E. Returns the damage as an integer. A weapon of 0 or more gives
// the dice of the weapon table (+0x0D sides, +0x0E number, +0x38 bonus) and
// the fire dice (+0x0F, +0x10). Else the dice of the creature.
static int damage_dice(object *attacker,int weapon)
{
	int n, sides, bonus, i, sum = 0;

	if (weapon >= 0 && weapon < N_weapon_types && Weapon_item_info[weapon].record >= 0) {
		int fn = weapon_int(weapon,"#fire_damage_dice",0), fs = weapon_int(weapon,"fire_damage_dice",0);

		n = weapon_int(weapon,"#damage_dice",1);
		sides = weapon_int(weapon,"damage_dice",6);
		bonus = weapon_int(weapon,"damage_bonus",0);
		// The fire part. The executable gives none of it to a creature
		// with fire_immune (not in the port).
		if (fn > 0 && fs > 0)
			for (i=0;i<fn;i++)
				sum += rand() % fs + 1;
	}
	else if (attacker && attacker->type == OBJ_PLAYER) {
		// The dice of the hand of the player are in the player structure
		// (0x1D33C1 to 0x1D33C7): the character code.
		return dtu_player_damage_roll();
	}
	else if (attacker && attacker->type == OBJ_ROBOT) {
		n = Monster_ext[attacker->id].n_damage_dice;
		sides = Monster_ext[attacker->id].damage_dice;
		bonus = Monster_ext[attacker->id].damage_bonus;
	}
	else
		return 0;
	// The executable stops with "RF.damage_dice > 0".
	if (sides < 1) sides = 1;
	if (n < 1) n = 1;
	for (i=0;i<n;i++)
		sum += rand() % sides + 1;
	return sum + bonus;
}

//------------------------------------------------------------------------
// The values of the player character: the calls of the fight code go
// through these functions to the character code (dtuchar.c).

DTU_WEAK int dtu_player_thac0(void)
{
	int t = dtu_char_thac0();

	return (t > 0) ? t : 20;
}

DTU_WEAK int dtu_player_armor_class(void)
{
	return dtu_char_armor_class();
}

DTU_WEAK int dtu_player_to_hit_bonus(void)
{
	dtu_attack a;

	dtu_char_get_attack(&a);
	return a.to_hit_bonus;
}

DTU_WEAK int dtu_player_damage_roll(void)
{
	return dtu_char_damage_roll();
}

DTU_WEAK int dtu_player_level(void)
{
	return 1;
}

DTU_WEAK fix dtu_player_hit_points(void)
{
	return Players[Player_num].shields;
}

DTU_WEAK fix dtu_player_max_hit_points(void)
{
	int m = dtu_char_max_hit_points();

	return (m > 0) ? i2f(m) : Player_max_hp;
}

DTU_WEAK fix dtu_player_damage(fix amount)
{
	fix hp = Players[Player_num].shields - amount;

	if (amount < 0 && hp > dtu_player_max_hit_points())
		hp = dtu_player_max_hit_points();
	if (hp < 0)
		hp = 0;
	Players[Player_num].shields = hp;
	return hp;
}

DTU_WEAK void dtu_player_award_xp(int amount)
{
	// 0xAF3CF adds the points to the experience of the character
	// (0x1D3393) and tests the level.
	dtu_char_award_xp(amount);
	dtu_obj_log("player: %d experience points",amount);
}

DTU_WEAK int dtu_hook_player_death_screen(void)
{
	return 0;
}

//------------------------------------------------------------------------
// Sounds and effects

// 0x2E900 digi_link_sound_to_pos: the sound can be heard as far as 256.0,
// and the volume is 0.5 at most (0x2E967).
static void sound_at(int sound,int segnum,vms_vector *pos,fix volume)
{
	if (sound < 0 || sound >= MAX_SOUNDS || segnum < 0 || segnum > Highest_segment_index)
		return;
	if (volume > F1_0/2)
		volume = F1_0/2;
	digi_link_sound_to_pos2(sound,segnum,0,pos,0,volume,i2f(256));
}

// 0x2E600 digi_link_sound_to_object: 200.0, volume 0.5 at most (0x2E639).
static void sound_on_object(int sound,object *obj,fix volume)
{
	if (sound < 0 || sound >= MAX_SOUNDS)
		return;
	if (volume > F1_0/2)
		volume = F1_0/2;
	digi_link_sound_to_object2(sound,obj-Objects,0,volume,i2f(200));
}

// 0x3BD4C: a vclip that plays one time at a point.
static void vclip_at(int segnum,vms_vector *pos,fix size,int vclip)
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
static int weapon_int(int w,char *key,int def)
{
	if (w < 0 || w >= N_weapon_types)
		return def;
	return tbl_int(Weapon_item_info[w].record,key,def);
}

// 0x25C76: the blood of a creature that is hit, the vclip
// monster_hit_vclip of the monster table with the size 5.0. Nothing for
// the player.
static void blood(object *obj)
{
	if (obj->type == OBJ_ROBOT)
		vclip_at(obj->segnum,&obj->pos,i2f(5),Monster_ext[obj->id].blood_vclip);
}

// A sound of the line of the player type (land_and_die_sound, 0x1D4E3B).
static int player_type_sound(char *key)
{
	if (N_player_types <= 0)
		return -1;
	return tbl_int(Player_type_info[0].record,key,-1);
}

// A new item object (object_create_egg 0x3C4BD for the types 5, 15, 16 and
// 17): no control, no movement, render type 8, the byte 0x1D5 is the count.
// The executable puts it at the position of the creature plus half of a
// size in y. The port puts it on the floor under the point. (guess for y)
int dtu_create_item(int type, int id, int count, int segnum, vms_vector *pos)
{
	item_info *it = item_table_entry(type,id);
	int objnum;
	object *obj;
	fix size = i2f(2);
	char value[32];

	if (it == NULL || segnum < 0 || segnum > Highest_segment_index)
		return -1;
	if (it->record >= 0 && bm_tbl_get(it->record,"size",value,sizeof(value)) && atof(value) > 0)
		size = fl2f(atof(value));

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

// The end of the DIE animation (0x6A3CC): the creature is a corpse. The
// executable puts it into the list at 0x2AEF54 and never takes it away
// (0x695E5 only holds it on the last frame of DIE). So the corpse stays.
static fix creature_floor_dist(object *obj,fix radius,fix half_height);

// The test log of a corpse: the lowest point of the body of the last frame
// of DIE and the floor under the center, from a ray that starts above the
// body (a body that is in the floor has its center below the floor).
static void log_corpse_height(object *obj)
{
	polymodel *po = obj_model(obj);
	int l = Dtu_anim_first[DAS_DIE+1] - 1, fate, seg, k;
	fvi_query fq;
	fvi_info hit;
	vms_vector p0, p1;
	fix bottom;

	if (po == NULL || po->box1_min == NULL || po->n_frames <= 0)
		return;
	if (l >= po->n_frames)
		l = po->n_frames - 1;
	bottom = obj->pos.y + po->box1_min[l+1].y;

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
		dtu_obj_log("corpse: obj %d %s seg %d pos %.2f %.2f %.2f bottom %.2f no floor (fate %d)",(int)(obj-Objects),dtu_object_name(obj),
			obj->segnum,f2fl(obj->pos.x),f2fl(obj->pos.y),f2fl(obj->pos.z),f2fl(bottom),fate);
		return;
	}
	dtu_obj_log("corpse: obj %d %s seg %d pos %.2f %.2f %.2f bottom %.2f floor %.2f (seg %d side %d ny %.2f) height %.2f",
		(int)(obj-Objects),dtu_object_name(obj),obj->segnum,f2fl(obj->pos.x),f2fl(obj->pos.y),f2fl(obj->pos.z),f2fl(bottom),
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
	p1.y -= i2f(80);
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

// ASCENT: Dtu_improved_pathfinding. The model of a creature has its origin
// at the center of the standing body, and the box of the last frame of DIE
// (the body on the ground) is far below that center: box1_min.y is -6.16
// for the Ghoul, and body_half_height is only 4.35 because the box is not
// centered on the origin (body_center.y -1.81). So the body is put with the
// lowest point of that box on the floor. The floor is the highest floor
// under the center of the box and under four points of the box (at 0.6 of
// its half size), but not more than 3.0 above the floor under the origin,
// so that a body on stairs lies on a step and is not drawn through one.
// Returns 0 if no floor is found.
static int corpse_on_floor(object *obj)
{
	static const signed char sx[5] = { 0, -1, 1, -1, 1 }, sz[5] = { 0, -1, -1, 1, 1 };
	polymodel *po = obj_model(obj);
	int l = Dtu_anim_first[DAS_DIE+1] - 1, i, seg;
	vms_vector mn, mx, q, ofs, p;
	vms_matrix m;
	fix base, best, f;

	if (po == NULL || po->box1_min == NULL || po->box1_max == NULL || po->n_frames <= 0)
		return 0;
	// a model with fewer frames (fMephit): its last frame
	if (l >= po->n_frames)
		l = po->n_frames - 1;
	mn = po->box1_min[l+1];
	mx = po->box1_max[l+1];
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
	p.y = best - mn.y;
	if ((seg = find_point_seg(&p,obj->segnum)) < 0)
		return 0;
	obj->pos = p;
	if (seg != obj->segnum)
		obj_relink(obj-Objects,seg);
	return 1;
}

static void creature_is_corpse(object *obj)
{
	polymodel *po = obj_model(obj);
	int frame = DTU_ANIM_FRAME(obj);
	monster_info *mi = &Monster_info[obj->id];

	// 0x6A48B to 0x6A510: the body is put on the floor ("RLH: pushing (%d)
	// into floor by %.2f"). The executable takes d from 0x6C30E (the
	// distance from the bottom of the torso box of the frame to the floor,
	// a negative d is in the floor) and moves the body up by -d, or down by
	// d plus torso_radius[0] of the model (the first value on the stack
	// that 0x748D9 fills). The port uses the half height of the box of the
	// body of the last frame (guess for the height). Both leave the body
	// in the floor, see corpse_on_floor().
	if (improved_paths() && corpse_on_floor(obj))
		;
	else if (po && po->body_half_height && frame >= 0 && frame < po->n_frames) {
		fix d = creature_floor_dist(obj,0,po->body_half_height[frame+1]);

		if (d != DTU_FLOOR_PROBE)
			obj->pos.y -= d;
	}

	// 0x6A639: a HUMANOID leaves gold in 4 of 10 cases (clock % 10 >= 6,
	// here a random number): 1 to moola / 4 pieces, a misc object with
	// the id 1 and the amount as its hit points.
	if ((mi->flags & MONSTER_F_HUMANOID) && (mi->moola >> 2) > 0) {
		int r = Dtu_fix_ai_ticks ? (rand() % 10) : (int)((uint)Dtu_clock % 10);

		if (r >= 6) {
			int amount = rand() % (mi->moola >> 2) + 1;
			int n = dtu_create_item(OBJ_MISC,1,1,obj->segnum,&obj->pos);

			if (n >= 0) {
				Objects[n].shields = i2f(amount);
				dtu_obj_log("drop: obj %d %s leaves %d gold (obj %d)",(int)(obj-Objects),dtu_object_name(obj),amount,n);
			}
		}
	}

	// MONSTER_F_EXTRAPLANAR: the body is not drawn (0x6A5A0)
	if (mi->flags & MONSTER_F_EXTRAPLANAR)
		obj->render_type = RT_NONE;

	obj->shields = 0;
	obj->flags |= OF_DESTROYED;
	obj->control_type = CT_NONE;
	obj->movement_type = MT_NONE;
	dtu_obj_log("obj %d %s: is a corpse",(int)(obj-Objects),dtu_object_name(obj));
	if (Obj_log)
		log_corpse_height(obj);
}

// The death of a creature in the damage function (0x2A091 to 0x2A245).
static void creature_dies(object *obj,object *attacker)
{
	dtu_ai_local *ail = &Ail[obj-Objects];
	ai_static *aip = &obj->ctype.ai_info;
	monster_info *mi = &Monster_info[obj->id];

	if (attacker && attacker->type == OBJ_PLAYER) {
		HUD_init_message("You killed %s.",dtu_object_display_name(obj));
		// 0x4EA50 with the score value. Not for a creature that
		// regenerates (status bit 0x01000000).
		if (!(obj->dtu.m_88 & 0x01000000))
			dtu_player_award_xp(Robot_info[obj->id].score_value);
	}
	// the words at 0x1D33AB and 0x1D33AD count each death
	Players[Player_num].num_kills_level++;
	Players[Player_num].num_kills_total++;

	// 0x2A157: the die sound, if the number is not 0
	if (mi->die_sound > 0)
		sound_at(mi->die_sound,obj->segnum,&obj->pos,F1_0);

	aip->CURRENT_STATE = aip->GOAL_STATE = DAIS_DEAD;
	DTU_ANIM_GOAL(obj) = DAS_DIE;
	set_anim(obj,DAS_DIE);
	ail->flags = 0;
	ail->anim_start = GameTime;
	ail->mode = DAIM_STILL;
	ail->activity = 2;
	ail->path_len = 0;
	obj->dtu.m_88 = 0;
	obj->shields = 0;
	vm_vec_zero(&obj->mtype.phys_info.velocity);

	dtu_obj_log("obj %d %s: dies",(int)(obj-Objects),dtu_object_name(obj));

	dtu_creature_drop_contents(obj);

	log_creature_state(obj,"death");
}

// object_create_egg (0x3C4BD): what the object of the level file
// contains. One object with the count in its byte 0x1D5. The defaults
// of the monster table are not used. The weapons 10, 28, 40, 74 and 75
// are not dropped.
void dtu_creature_drop_contents(object *obj)
{
	if (obj->contains_count > 0) {
		int type = obj->contains_type, id = obj->contains_id, count = obj->contains_count, n = -1;

		if (type == OBJ_ROBOT)
			n = dtu_gate_in_creature(id,obj->segnum);
		else if (!(type == OBJ_WEAPON && (id == 10 || id == 0x1c || id == 0x28 || id == 0x4a || id == 0x4b)))
			n = dtu_create_item(type,id,count,obj->segnum,&obj->pos);
		dtu_obj_log("drop: obj %d %s leaves type %d id %d count %d (obj %d)",(int)(obj-Objects),dtu_object_name(obj),type,id,count,n);
		obj->contains_type = obj->contains_id = -1;
		obj->contains_count = 0;
	}
}

// 0xA9F58: the creature leaves the level (Abjure): no control, no
// movement, no picture, the flags 0xC0. The port takes the object away.
void dtu_creature_vanish(object *obj)
{
	dtu_obj_log("obj %d %s: leaves the level (0xA9F58)",(int)(obj-Objects),dtu_object_name(obj));
	obj->control_type = CT_NONE;
	obj->movement_type = MT_NONE;
	obj->render_type = RT_NONE;
	obj->flags |= OF_SHOULD_BE_DEAD;
}

int dtu_creature_is_active(object *obj)
{
	return obj && obj->type == OBJ_ROBOT && Ail[obj-Objects].activity == 2;
}

// The weapon number of the damage functions for damage with no to-hit
// roll and no dice (spells, traps, dtu_damage_creature_direct).
#define DTU_DIRECT	(-2)

// The dice of a spell: "count" rolls of rand() % sides + 1, plus
// per_level * level (0x91BC3). The executable divides by 0 for a count
// with no sides. The port counts 0.
static int spell_dice(int record,char *sides_key,char *count_key,char *level_key,int level)
{
	int sides = tbl_int(record,sides_key,0), count = tbl_int(record,count_key,0), sum = 0, i;

	if (sides == 0 && count == 0)
		return 0;
	if (sides > 0)
		for (i=0;i<count;i++)
			sum += rand() % sides + 1;
	return sum + tbl_int(record,level_key,0) * level;
}

// 0x91BC3 (spells.c), verified: the damage of a spell on a target. Four
// groups of dice of the spell table: damage_dice, fire_damage_dice,
// acid_damage_dice and elec_damage_dice (Spell_info +0x3A, +0x3D, +0x43,
// +0x46: sides, number, plus for each level). cold_damage_dice (+0x40) is
// not used here. Fire: 0 for a monster with fire_immune, one half with
// fire_resistant, one half with the effect Resist Fire (bit 10), and the
// effect Protection From Fire (bit 9) takes it from its counter (+0x82).
// Electricity: 0 for a monster with electricity_immune. Fire or acid
// damage ends the regeneration (bit 24) of the target.
int dtu_spell_damage_roll(int spell, object *target, int level)
{
	int rec, base, fire, acid, elec;

	if (spell < 0 || spell >= N_spell_types || target == NULL)
		return 0;
	rec = Spell_info[spell].record;
	base = spell_dice(rec,"damage_dice","#damage_dice","damage_level",level);
	fire = spell_dice(rec,"fire_damage_dice","#fire_damage_dice","fire_damage_level",level);
	acid = spell_dice(rec,"acid_damage_dice","#acid_damage_dice","acid_damage_level",level);
	elec = spell_dice(rec,"elec_damage_dice","#elec_damage_dice","elec_damage_level",level);

	if (target->type == OBJ_ROBOT && target->id < N_robot_types) {
		monster_info *mi = &Monster_info[target->id];

		if (mi->flags & MONSTER_F_ELECTRICITY_IMMUNE)
			elec = 0;
		if (mi->flags & MONSTER_F_FIRE_IMMUNE)
			fire = 0;
		else if (bm_tbl_has_flag(mi->record,"fire_resistant"))
			fire /= 2;
	}
	if (target->dtu.m_88 & (1 << 10))			// Resist Fire
		fire /= 2;
	if (target->dtu.m_88 & (1 << 9)) {			// Protection From Fire
		target->dtu.m_82 -= fire;
		fire = (target->dtu.m_82 < 0) ? -target->dtu.m_82 : 0;
	}
	if (fire || acid)
		target->dtu.m_88 &= ~0x01000000;
	return base + fire + acid + elec;
}

// The level of the caster of a spell missile: the mage level of the
// monster table for a creature, the level of the character for the
// player, 1 for a trap.
static int caster_level(object *caster)
{
	if (caster && caster->type == OBJ_ROBOT && caster->id < N_robot_types && Monster_ext[caster->id].spell_level > 0)
		return Monster_ext[caster->id].spell_level;
	if (caster && caster->type == OBJ_PLAYER)
		return dtu_player_level();
	return 1;
}

static fix damage_creature_w(object *creature, object *attacker, fix damage, int weapon);
static fix damage_player_w(object *player, object *attacker, fix damage, int weapon);

// spells.c 0x8CFFF and 0x90CEF: the effect of a spell on a target. The
// weak definition has the damage of the spell (0x91BC3, through the
// damage function with no to-hit roll) and none of the other effects
// (sleep, hold, fear, poison and the rest), which belong to the spell
// code (dtuspell.c can define this function). The saving throw of the
// executable (0xAE596, one half of the damage) is not read.
DTU_WEAK fix dtu_spell_hit(int spell, object *target, object *caster, int level)
{
	fix damage;

	if (spell < 0 || spell >= N_spell_types || target == NULL)
		return 0;
	damage = i2f(dtu_spell_damage_roll(spell,target,level));
	dtu_obj_log("spell: spell %d '%s' level %d hits %s: damage %d (the other effects are not in the port)",spell,
		Spell_info[spell].name,level,dtu_object_name(target),f2i(damage));
	if (damage <= 0)
		return 0;
	if (target->type == OBJ_PLAYER)
		return damage_player_w(target,caster,damage,DTU_DIRECT);
	if (target->type == OBJ_ROBOT)
		return damage_creature_w(target,caster,damage,DTU_DIRECT);
	return 0;
}

static fix spell_hit(int spell,object *target,object *attacker)
{
	return dtu_spell_hit(spell,target,attacker,caster_level(attacker));
}

// 0x29A9C for a creature as the target. damage is used if there is no
// roll. With an attacker or a weapon the function throws the dice.
// Returns the damage that was done.
static fix damage_creature_w(object *creature, object *attacker, fix damage, int weapon)
{
	dtu_ai_local *ail = &Ail[creature-Objects];
	ai_static *aip = &creature->ctype.ai_info;

	if (creature->type != OBJ_ROBOT || creature->shields < F1_0 || (creature->flags & (OF_EXPLODING|OF_SHOULD_BE_DEAD|OF_DESTROYED)))
		return 0;

	if (weapon != DTU_DIRECT && (attacker || weapon >= 0)) {
		int roll, hit = to_hit_roll(creature,attacker,weapon,&roll);

		if (hit <= 0) {
			dtu_obj_log("attack: %s misses obj %d %s (roll %d, result %d)",attacker ? dtu_object_name(attacker) : "a missile",
				(int)(creature-Objects),dtu_object_name(creature),roll,hit);
			return 0;
		}
		// 0x29D9D: a creature with weapon_resistant takes no damage from
		// a natural weapon or from a weapon that is not magic.
		if ((Monster_info[creature->id].flags & MONSTER_F_WEAPON_RESISTANT) &&
			 (weapon < 0 || !bm_tbl_has_flag(Weapon_item_info[weapon].record,"magic"))) {
			dtu_obj_log("attack: obj %d %s is weapon resistant",(int)(creature-Objects),dtu_object_name(creature));
			return 0;
		}
		damage = i2f(damage_dice(attacker,weapon));
		if (hit == 2)
			damage += i2f(damage_dice(attacker,weapon));
	}

	if (damage <= 0)
		return 0;

	creature->shields -= damage;

	dtu_obj_log("damage: obj %d %s takes %d from %s, hp %d",(int)(creature-Objects),dtu_object_name(creature),
		f2i(damage),attacker ? dtu_object_name(attacker) : "nothing",f2i(creature->shields));

	// 0x29FB8: the creature flinches
	if (aip->CURRENT_STATE != DAIS_DEAD) {
		aip->CURRENT_STATE = DAIS_FLIN;
		ail->anim_start = GameTime + FrameTime;
	}
	if (attacker && attacker->type == OBJ_PLAYER) {
		HUD_init_message("You hit %s for %d.",dtu_object_display_name(creature),f2i(damage));
		// A creature that is hit knows where the player is, and a creature
		// that was not hostile is hostile now. (guess: the executable does
		// this in the script code 0xA94AD and in the alert function)
		creature->dtu.m_1b0 |= DTU_HOSTILE;
		creature->dtu.m_1b0 &= ~(DTU_PEACEFUL|DTU_FLEEING);
		ail->target = 0;
		if (ail->awareness < 4) ail->awareness = 4;
		ail->awareness_time = i2f(3);
		ail->morale = Monster_info[creature->id].ai_values[1][Difficulty_level];
	}

	if (creature->shields < F1_0)
		creature_dies(creature,attacker);
	return damage;
}

void dtu_damage_creature(object *creature, object *attacker, fix damage)
{
	damage_creature_w(creature,attacker,damage,attack_weapon(attacker));
}

fix dtu_damage_creature_direct(object *creature, object *attacker, fix damage)
{
	return damage_creature_w(creature,attacker,damage,DTU_DIRECT);
}

// The death of the player in the damage function (0x2A4D0 and after): the
// sound land_and_die_sound of the player type, the hit points and the
// status flags become 0, and the object is taken out of the game
// (0x6A3CC). The executable has no death camera. The rest of the
// sequence is dtu_player_death_frame().
static void player_dies(object *player,object *attacker)
{
	sound_at(player_type_sound("land_and_die_sound"),player->segnum,&player->pos,F1_0);
	Players[Player_num].killer_objnum = attacker ? (int)(attacker-Objects) : -1;
	dtu_player_damage(dtu_player_hit_points());
	player->dtu.m_88 = 0;
	player->control_type = CT_NONE;
	player->movement_type = MT_NONE;
	vm_vec_zero(&player->mtype.phys_info.velocity);
	Dtu_player_dead = 1;
	Player_death_stage = 1;
	Player_death_time = GameTime;
	dtu_obj_log("player: dies, killed by %s",attacker ? dtu_object_name(attacker) : "nothing");
}

// 0x33C5E: the status display finds the hit points of the player below
// 1.0 in the next frame and starts the death. This catches the damage
// that does not go through the damage function (the hit points of a trap,
// the code of other files).
void dtu_player_check_death(object *killer)
{
	if (!Dtu_objects_enabled || Dtu_player_dead || Player_is_dead || ConsoleObject == NULL || ConsoleObject->type != OBJ_PLAYER)
		return;
	if (dtu_player_hit_points() < F1_0)
		player_dies(ConsoleObject,killer);
}

// The game after the death of the player (game() 0x423F5): the streams
// stop, the game waits 3.0 seconds or for a key, the screen fades out, the
// death picture comes (febgnd.jrg record 161), the death music plays
// (0x4249D) and the death menu offers "Load Game" and "Abort Game". The
// port waits 3.0 seconds, fades out and plays the music. Then the front
// end takes over (dtu_hook_player_death_screen), or the level starts
// again after DTU_DEATH_RESTART_WAIT.
int dtu_player_death_frame(void)
{
	if (!Dtu_objects_enabled || !Dtu_player_dead || Player_death_stage == 0)
		return 0;

	if (GameTime < Player_death_time)
		Player_death_time = GameTime;

	if (Player_death_stage == 1) {
		if (GameTime - Player_death_time < DTU_DEATH_WAIT)
			return 0;
		dtu_obj_log("player: death screen: fade out, death music");
		digi_stop_all();
		gr_palette_fade_out(gr_palette,32,0);
		dtumusic_play(DTUMUSIC_DEATH,0,100);
		Player_death_time = GameTime;
		if (dtu_hook_player_death_screen()) {
			dtu_obj_log("player: the front end has the death screen");
			Player_death_stage = 3;
			return 0;
		}
		Player_death_stage = 2;
		return 0;
	}

	if (Player_death_stage == 2 && GameTime - Player_death_time >= DTU_DEATH_RESTART_WAIT) {
		int level = Current_level_num;

		Player_death_stage = 0;
		Dtu_player_dead = 0;
		init_player_stats_new_ship();
		dtu_player_damage(-dtu_player_max_hit_points());
		StartNewLevelSub(level,0);
		gr_palette_load(gr_palette);
		dtu_obj_log("player: the level starts again, hp %d",f2i(dtu_player_hit_points()));
		return 1;
	}
	return 0;
}

// 0x29A9C for the player as the target (0x2A24A). Without an attacker and
// without a weapon the damage is taken as it is: no roll, no armor. Returns
// the damage that was done.
static fix damage_player_w(object *player, object *attacker, fix damage, int weapon)
{
	fix hp;

	if (player->type != OBJ_PLAYER || Player_is_dead || Dtu_player_dead || dtu_player_hit_points() < F1_0)
		return 0;

	if (weapon != DTU_DIRECT && ((attacker && attacker != player) || weapon >= 0)) {
		int roll, hit = to_hit_roll(player,attacker,weapon,&roll);

		if (hit <= 0) {
			dtu_obj_log("attack: %s misses the player (roll %d, result %d)",
				attacker ? dtu_object_name(attacker) : "a missile",roll,hit);
			return 0;
		}
		// 0x2A45F: a hit stops the cast of the player (0x9305D)
		dtu_spell_interrupt(player,1);
		damage = i2f(damage_dice(attacker,weapon));
		if (hit == 2)
			damage += i2f(damage_dice(attacker,weapon));
	}

	if (damage <= 0)
		return 0;

	if (attacker && attacker != player)
		HUD_init_message("%s hits you for %d.",dtu_object_display_name(attacker),f2i(damage));
	else
		HUD_init_message("You took %d damage.",f2i(damage));

	// 0x2A3C0: red plus 4 for each point, green and blue minus one half
	hp = dtu_player_damage(damage);
	PALETTE_FLASH_ADD(f2i(damage)*4,-f2i(damage/2),-f2i(damage/2));

	dtu_obj_log("damage: the player takes %d from %s%s%s, hp %d",f2i(damage),
		attacker ? dtu_object_name(attacker) : "nothing",weapon >= 0 ? ", weapon " : "",
		weapon >= 0 ? Weapon_item_info[weapon].name : "",f2i(hp));

	if (hp < F1_0)
		player_dies(player,attacker);
	return damage;
}

void dtu_damage_player(object *player, object *attacker, fix damage)
{
	damage_player_w(player,attacker,damage,attack_weapon(attacker));
}

fix dtu_damage_player_direct(object *player, object *attacker, fix damage)
{
	return damage_player_w(player,attacker,damage,DTU_DIRECT);
}

// 0x27D82. A melee weapon touched the target.
static void melee_hit(object *target,object *attacker)
{
	int weapon = attack_weapon(attacker);
	fix dealt, hp;
	int sound = -1;

	if (target->flags & OF_EXPLODING)
		return;

	if (weapon >= 0 && weapon_int(weapon,"enemy_spell",-1) >= 0)
		spell_hit(weapon_int(weapon,"enemy_spell",-1),target,attacker);

	if (target->type == OBJ_PLAYER) {
		dealt = damage_player_w(target,attacker,0,weapon);
		hp = dtu_player_hit_points();
		if (attacker->type == OBJ_ROBOT) {
			// The creature is active after a hit (0x28B34)
			Ail[attacker-Objects].activity = 2;
		}
	}
	else if (target->type == OBJ_ROBOT) {
		dealt = damage_creature_w(target,attacker,0,weapon);
		hp = target->shields;
	}
	else
		return;

	// The push of 0.2 along the line of the attack (0x6B70D) is not in
	// the port.
	if (dealt > 0) {
		blood(target);
		if (weapon < 0) {
			// A natural weapon: the hit sound of the target. The die sound
			// of a creature is played by its death. The key of the hit
			// sound of a player type (0x1D4E2B) is a guess.
			if (target->type == OBJ_ROBOT)
				sound = (hp >= F1_0) ? Monster_info[target->id].hit_sound : -1;
			else
				sound = player_type_sound("hit_sound");
		}
		else {
			sound = weapon_int(weapon,"monster_hit_sound",-1);
			if (hp < F1_0 && weapon_int(weapon,"monster_kill_sound",-1) >= 0)
				sound = weapon_int(weapon,"monster_kill_sound",-1);
		}
		if (attacker->type == OBJ_PLAYER && target->type == OBJ_ROBOT)
			sound_on_object(sound,target,F1_0/2);		// 0x27C0E
		else
			sound_at(sound,target->segnum,&target->pos,F1_0);
	}
	else if (weapon >= 0) {
		vclip_at(target->segnum,&target->pos,Weapon_info[weapon].impact_size,weapon_int(weapon,"monster_miss_vclip",-1));
		sound_at(weapon_int(weapon,"monster_miss_sound",-1),target->segnum,&target->pos,F1_0/4);
	}
}

// 0x6CFF8 (physics.c of the executable). The test of the weapon in an
// attack state. The executable calls it from the movement of each
// creature (0x9F2B4) and of the player. The weapon sphere of the frame
// moves from its center away from the body by its radius. The first
// object that it touches is hit, one time for each swing.
static void weapon_test(object *obj)
{
	int objnum = obj-Objects;
	int s = DTU_ANIM_STATE(obj);
	int n, rel, fate;
	ubyte *swing_done = (obj->type == OBJ_PLAYER) ? (ubyte *)NULL : &Ail[objnum].swing_done;
	vms_vector center, dir, p1, test_pos, save_pos;
	fix radius;
	fvi_query fq;
	fvi_info hit;
	int seg;

	if (!(s == DAS_ATTACK || s == DAS_ATTACK2 || s == DAS_ATTACK3 || s == DAS_ATTACK4)) {
		if (swing_done) *swing_done = 0; else Player_swing_done = 0;
		return;
	}

	n = Dtu_anim_first[s+1] - Dtu_anim_first[s];
	rel = DTU_ANIM_FRAME(obj) - Dtu_anim_first[s];

	// A creature hits from frame 8 of the attack to its end, the player
	// in the frames 6 to 8 (0x6D0CF to 0x6D127).
	if (obj->type == OBJ_ROBOT) {
		if (rel < 8)
			return;
	}
	else if (rel < 6 || rel > n - rel)
		return;

	// 0x6D01A to 0x6D066: for the test the position of the object is
	// moved forward in the x-z plane by the square root of 2 (the code
	// adds sin + cos and cos - sin of the heading minus 45 degrees).
	{
		vms_angvec a;
		fix sn, cs;

		vm_extract_angles_vector(&a,&obj->orient.fvec);
		fix_sincos(a.h - 0x2000,&sn,&cs);
		test_pos = obj->pos;
		test_pos.x += sn + cs;
		test_pos.z -= sn - cs;
	}
	save_pos = obj->pos;
	obj->pos = test_pos;

	if (!get_weapon_sphere(obj,DTU_ANIM_FRAME(obj),&center,&radius)) {
		obj->pos = save_pos;
		if (obj->type != OBJ_PLAYER)
			return;
		// A player model with no weapon box: a sphere in front of the body
		// takes the place of the weapon box. (guess)
		vm_vec_scale_add(&center,&test_pos,&obj->orient.fvec,i2f(4));
		radius = i2f(3);
	}
	obj->pos = save_pos;

	seg = find_point_seg(&center,obj->segnum);
	if (seg < 0)
		return;

	vm_vec_sub(&dir,&center,&test_pos);
	vm_vec_normalize_quick(&dir);
	vm_vec_scale_add(&p1,&center,&dir,radius);

	// The walls: the weapon stops at a wall.
	fq.p0 = &center;
	fq.p1 = &p1;
	fq.startseg = seg;
	fq.rad = radius;
	fq.thisobjnum = objnum;
	fq.ignore_obj_list = NULL;
	fq.flags = 0;

	fate = find_vector_intersection(&fq,&hit);
	if (fate == HIT_WALL)
		p1 = hit.hit_pnt;
	else if (fate != HIT_NONE)
		return;

	// The objects. The executable does this in the same query (flag 1 of
	// 0x3EFD9). The port tests each body by itself, see
	// dtu_object_hit_test() in fvi.c.
	{
		int i, best = -1;
		fix best_d = 0x7fffffff;
		vms_vector ip;

		for (i=0;i<=Highest_object_index;i++) {
			object *o = &Objects[i];
			fix d;

			if (i == objnum || (o->type != OBJ_ROBOT && o->type != OBJ_PLAYER && o->type != OBJ_DRESSING))
				continue;
			if (o->flags & (OF_SHOULD_BE_DEAD|OF_DESTROYED))
				continue;
			if (vm_vec_dist_quick(&o->pos,&center) > i2f(40))
				continue;
			d = dtu_object_hit_test(&ip,&center,&p1,radius,o,obj);
			if (d > 0 && d < best_d) {
				best_d = d;
				best = i;
			}
		}

		if (Obj_log_frames && obj->type == OBJ_PLAYER)
			dtu_obj_log("player: weapon test frame %d, radius %.2f, hits object %d",DTU_ANIM_FRAME(obj),f2fl(radius),best);

		if (best < 0)
			return;
		hit.hit_object = best;
	}

	{
		object *other = &Objects[hit.hit_object];

		if (other->type == OBJ_DRESSING) {
			// 0x6D4EC: scenery that can break: its hit points are cut to
			// 20.0, then 0x29A9C with no attacker takes 100.0 and 0xA6ACB
			// breaks it. One hit breaks it.
			if (!dtu_dressing_unbreakable(other)) {
				if (other->shields > i2f(20))
					other->shields = i2f(20);
				other->shields -= i2f(100);
				dtu_break_dressing(other,obj,0);
			}
			if (swing_done) *swing_done = 0;
		}
		else if (other->type == OBJ_ROBOT) {
			if (obj->type == OBJ_ROBOT)
				return;
			// 0x278B5: the player hits a creature
			if (!Player_swing_done) {
				Player_swing_done = 1;
				melee_hit(other,obj);
			}
		}
		else if (other->type == OBJ_PLAYER) {
			if (swing_done && !*swing_done && Dtu_creatures_attack) {
				*swing_done = 1;
				dtu_obj_log("attack: obj %d %s swings at the player, frame %d",objnum,dtu_object_name(obj),DTU_ANIM_FRAME(obj));
				melee_hit(other,obj);
				// The function 0x1153A (do_ai_robot_hit_attack) sets the flag
				// AILF_HIT here, which ends the swing early. No code of
				// version 1.3 calls that function, so the flag is never set
				// and the attack animation plays to its end.
			}
		}
	}
}

//------------------------------------------------------------------------
// Missiles (laser.c 0x55511, pweapon.c 0xA03A8, collide.c 0x284E0, 0x272DD,
// 0x26C2F). A missile is an object of the type OBJ_WEAPON. The port moves
// it here and not with the weapon code of Descent 1: it flies in a
// straight line with the same speed, with no gravity and no drag.

static void missile_remove(object *obj)
{
	Missile[obj-Objects].signature = 0;
	obj->flags |= OF_SHOULD_BE_DEAD;
}

static fix Missile_life[MAX_OBJECTS];		// lifeleft (object +0x84)

// The look and the size of a missile (part of 0x55511): the render type,
// the model or the vclip, and the size.
static fix missile_look(int weapon_type,int spell,int *rtype,int *model,int *vclip)
{
	weapon_info *wi = &Weapon_info[weapon_type];
	fix size = F1_0;

	*rtype = RT_NONE;
	*model = *vclip = -1;
	if (wi->render_type == WEAPON_RENDER_POLYMODEL)
		*model = wi->model_num;
	else if (wi->render_type == WEAPON_RENDER_VCLIP)
		*vclip = wi->weapon_vclip;

	// A spell missile has the look of the spell: the executable copies
	// the render values of the spell to a weapon entry (0x56FF6, 0x946FA).
	if (spell >= 0 && spell < N_spell_types) {
		if (Spell_info[spell].model_num >= 0) {
			*model = Spell_info[spell].model_num;
			*vclip = -1;
		}
		else if (Spell_info[spell].vclip_num >= 0) {
			*vclip = Spell_info[spell].vclip_num;
			*model = -1;
		}
	}

	if (*model >= 0 && *model < N_polygon_models && Polygon_models[*model].dtu_block) {
		// the radius of the model divided by lw_ratio
		*rtype = RT_POLYOBJ;
		size = fixdiv(Polygon_models[*model].rad,wi->po_len_to_width_ratio > 0 ? wi->po_len_to_width_ratio : i2f(10));
	}
	else if (*vclip >= 0 && *vclip < VCLIP_MAXNUM) {
		*rtype = RT_WEAPON_VCLIP;
		size = wi->blob_size;
	}
	if (size < F1_0/16)
		size = F1_0/16;
	return size;
}

// The speed of the difficulty level, one half for a weapon with thrust.
static fix missile_speed(weapon_info *wi)
{
	fix speed = wi->speed[Difficulty_level];

	if (wi->thrust)
		speed /= 2;
	return speed;
}

// Laser_create_new (0x55511)
int dtu_create_missile(vms_vector *dir, vms_vector *pos, int segnum, int parent, int weapon_type, int spell, int make_sound)
{
	weapon_info *wi;
	int objnum, rtype, model, vclip, bounce;
	fix size, speed;
	object *obj;
	dtu_missile *m;
	vms_matrix orient = vmd_identity_matrix;

	if (weapon_type < 0 || weapon_type >= N_weapon_types)
		weapon_type = 0;
	if (N_weapon_types <= 0 || segnum < 0 || segnum > Highest_segment_index)
		return -1;
	wi = &Weapon_info[weapon_type];

	size = missile_look(weapon_type,spell,&rtype,&model,&vclip);
	if (rtype == RT_POLYOBJ)
		vm_vector_2_matrix(&orient,dir,(parent >= 0) ? &Objects[parent].orient.uvec : NULL,NULL);

	objnum = obj_create(OBJ_WEAPON,weapon_type,segnum,pos,&orient,size,CT_NONE,MT_NONE,rtype);
	if (objnum < 0)
		return -1;
	obj = &Objects[objnum];
	if (rtype == RT_POLYOBJ) {
		obj->rtype.pobj_info.model_num = model;
		obj->rtype.pobj_info.tmap_override = -1;
	}
	else if (rtype == RT_WEAPON_VCLIP)
		obj->rtype.vclip_info.vclip_num = vclip;

	// weapon +0x0A: bounce 1, stick 2, thrown 3
	bounce = 0;
	if (weapon_int(weapon_type,"bounce",0)) bounce = 1;
	if (weapon_int(weapon_type,"stick",0)) bounce = 2;
	if (weapon_int(weapon_type,"thrown",0)) bounce = 3;

	m = &Missile[objnum];
	memset(m,0,sizeof(*m));
	m->signature = obj->signature;
	m->parent = parent;
	m->parent_sig = (parent >= 0) ? Objects[parent].signature : -1;
	m->spell = spell;
	m->bounce = bounce;

	// The time to live. The port keeps it here: the object code of
	// Descent 1 would explode a weapon with a damage radius at its end.
	Missile_life[objnum] = wi->lifetime;
	// 0x56FF6 writes 30.0 to the template of a spell missile (the caster
	// sets the life of Lightning Bolt and Acid Arrow, dtu_missile_set_life)
	if (spell >= 0)
		Missile_life[objnum] = i2f(30);
	if (bounce == 2)
		Missile_life[objnum] += (rand() * 4) - F1_0;

	// The velocity of the parent is not added.
	speed = missile_speed(wi);
	vm_vec_copy_scale(&obj->mtype.phys_info.velocity,dir,speed);

	vclip_at(obj->segnum,&obj->pos,wi->flash_size,wi->flash_vclip);
	if (make_sound && wi->flash_sound > -1)
		sound_at(wi->flash_sound,obj->segnum,&obj->pos,F1_0);

	dtu_obj_log("missile: obj %d weapon %d '%s' spell %d from obj %d, render %d size %.2f speed %.1f life %.1f, at %.1f %.1f %.1f",
		objnum,weapon_type,Weapon_item_info[weapon_type].name,spell,parent,rtype,f2fl(size),f2fl(speed),f2fl(Missile_life[objnum]),
		f2fl(pos->x),f2fl(pos->y),f2fl(pos->z));
	return objnum;
}

int dtu_missile_spell(object *obj)
{
	int n = obj-Objects;

	if (obj->type != OBJ_WEAPON || Missile[n].signature != obj->signature)
		return -1;
	return Missile[n].spell;
}

void dtu_missile_set_life(int objnum, fix life)
{
	if (objnum >= 0 && objnum < MAX_OBJECTS)
		Missile_life[objnum] = life;
}

// 0x93294: the burst of a weapon with a min_speed (the bomb of a trap):
// "magnitude" fragments that are drawn as the vclip, with a size of 8.0 to
// 17.0, in random directions. They bounce.
void dtu_create_burst(int weapon_type, int segnum, vms_vector *pos)
{
	int n = weapon_int(weapon_type,"magnitude",0), i;
	int min_speed = weapon_int(weapon_type,"min_speed",0), range = weapon_int(weapon_type,"speed_range",0);

	if (n > 40)
		n = 40;
	for (i=0;i<n;i++) {
		vms_vector dir;
		int objnum;

		make_random_vector(&dir);
		objnum = dtu_create_missile(&dir,pos,segnum,-1,weapon_type,-1,0);
		if (objnum < 0)
			break;
		vm_vec_copy_scale(&Objects[objnum].mtype.phys_info.velocity,&dir,i2f(min_speed + (range > 0 ? rand() % range : 0)) / 4);
		Objects[objnum].size = i2f(8 + rand() % 10);
		Missile[objnum].bounce = 1;
		Missile_life[objnum] -= rand() * 2;
	}
	dtu_obj_log("burst: weapon %d, %d fragments",weapon_type,n);
}

// collide_weapon_and_wall (0x26C2F) and the end of 0xA0731
static void missile_hit_wall(object *obj,vms_vector *point,vms_vector *wallnorm,int segnum,int side)
{
	dtu_missile *m = &Missile[obj-Objects];
	weapon_info *wi = &Weapon_info[obj->id];

	// 0x26C2F: the spell of the missile. Knock (28) calls the door function
	// 0x7E859(seg, side, 1, parent); an area spell has its area here
	// (0x90CEF at 0x26D63), which also removes the missile.
	if (m->spell >= 0 && !m->harmless) {
		object *parent = (m->parent >= 0 && Objects[m->parent].signature == m->parent_sig) ? &Objects[m->parent] : NULL;

		if (m->spell == 28 && side >= 0 && segnum >= 0) {
			dtu_obj_log("missile: Knock hits seg %d side %d",segnum,side);
			dtu_wall_use_door(&Segments[segnum],side,parent,1);
		}
		else if (dtu_spell_is_area(m->spell)) {
			vms_vector p = *point;
			int seg = segnum;

			// the point just in front of the wall
			if (wallnorm) {
				vm_vec_scale_add2(&p,wallnorm,F1_0/4);
				if ((seg = find_point_seg(&p,segnum)) < 0) { p = *point; seg = segnum; }
			}
			dtu_obj_log("missile: the area spell %d of obj %d hits a wall",m->spell,(int)(obj-Objects));
			missile_remove(obj);
			dtu_spell_area(m->spell,&p,seg,parent,parent ? caster_level(parent) : 1);
			return;
		}
	}

	if (m->bounce == 1 && wallnorm) {
		// 0xA07D7 negates the velocity of a missile that bounces
		vm_vec_negate(&obj->mtype.phys_info.velocity);
		return;
	}
	// A missile that is not a model goes away with no sound and no vclip.
	if (obj->render_type == RT_POLYOBJ) {
		sound_at(wi->wall_hit_sound,segnum,point,F1_0/2);
		vclip_at(segnum,point,obj->size*2,wi->wall_hit_vclip);
	}
	dtu_obj_log("missile: obj %d hits a wall at %.1f %.1f %.1f",(int)(obj-Objects),f2fl(point->x),f2fl(point->y),f2fl(point->z));
	missile_remove(obj);
}

// collide_player_and_weapon (0x284E0) and collide_robot_and_weapon
// (0x272DD). The executable lets a missile with "stick" or "thrown" drop
// after the hit. The port takes each missile away.
static void missile_hit_object(object *obj,object *other,vms_vector *point)
{
	dtu_missile *m = &Missile[obj-Objects];
	weapon_info *wi = &Weapon_info[obj->id];
	int weapon = obj->id, spell, sound;
	object *parent = NULL;
	fix dealt, hp, spell_dealt;

	if (m->parent >= 0 && Objects[m->parent].signature == m->parent_sig)
		parent = &Objects[m->parent];

	if (other->type == OBJ_DRESSING) {
		// 0x2641C: the missile casts its spell on the scenery (the spells
		// decide whether it breaks); the weapon does no damage. The missile
		// stops like at a wall. (guess for the stop)
		if (m->spell >= 0 && !m->harmless) {
			spell_hit(m->spell,other,parent);
			if (obj->flags & OF_SHOULD_BE_DEAD)
				return;
			if (dtu_spell_is_area(m->spell)) {
				missile_remove(obj);
				return;
			}
		}
		missile_hit_wall(obj,point,NULL,obj->segnum,-1);
		return;
	}
	if (m->harmless)
		return;

	// A spell missile does the damage of the spell (dtu_spell_hit). A
	// weapon with enemy_spell does the effect of the spell and the damage
	// of the weapon.
	spell = (m->spell >= 0) ? m->spell : weapon_int(weapon,"enemy_spell",-1);
	spell_dealt = (spell >= 0) ? spell_hit(spell,other,parent) : 0;

	if (other->type == OBJ_PLAYER) {
		dealt = (m->spell >= 0) ? spell_dealt : damage_player_w(other,parent,0,weapon);
		hp = dtu_player_hit_points();
		if (dealt > 0) {
			sound = weapon_int(weapon,"monster_hit_sound",-1);
			if (hp < F1_0 && weapon_int(weapon,"monster_kill_sound",-1) >= 0)
				sound = weapon_int(weapon,"monster_kill_sound",-1);
			sound_at(sound,other->segnum,point,0x5555);
			vclip_at(obj->segnum,point,wi->impact_size,wi->robot_hit_vclip);
		}
		else {
			sound_at(weapon_int(weapon,"monster_miss_sound",-1),other->segnum,point,0x5555);
			vclip_at(obj->segnum,point,wi->impact_size,weapon_int(weapon,"monster_miss_vclip",-1));
		}
	}
	else if (other->type == OBJ_ROBOT) {
		Ail[other-Objects].activity = 2;
		dealt = (m->spell >= 0) ? spell_dealt : damage_creature_w(other,parent,0,weapon);
		hp = other->shields;
		if (dealt > 0) {
			blood(other);
			if (hp > i2f(2) && Monster_info[other->id].hit_sound > 0)
				sound_at(Monster_info[other->id].hit_sound,other->segnum,point,F1_0);
			// ASCENT: the original plays no weapon sound when the hit
			// kills (0x27650: die_sound, or monster_kill_sound, which no
			// shipped table sets). The port plays the impact sound on the
			// killing hit too (docs/re/bugs.md).
			else
				sound_at(weapon_int(weapon,"monster_hit_sound",-1),other->segnum,point,F1_0/4);
		}
		else
			sound_at(weapon_int(weapon,"monster_miss_sound",-1),other->segnum,point,F1_0/4);
	}
	missile_remove(obj);
}

// The objects on the way of a missile from its position to end (part of
// 0xA03A8): the number of the object that it hits first, or -1, and the
// hit point. The executable does this in the same query as the walls. The
// port tests each body, see weapon_test(). step is the length of the move
// in this frame. The missile can be a stand in object that is not in the
// list (dtu_missile_path_target).
static int missile_path_object(object *obj,vms_vector *end,fix step,int parent,int parent_sig,vms_vector *best_ip)
{
	int j, best = -1;
	fix best_d = 0x7fffffff;
	vms_vector ip;

	for (j=0;j<=Highest_object_index;j++) {
		object *o = &Objects[j];
		fix d;

		if (o == obj || (o->type != OBJ_ROBOT && o->type != OBJ_PLAYER && o->type != OBJ_DRESSING))
			continue;
		if (j == parent && o->signature == parent_sig)
			continue;
		if (o->flags & (OF_SHOULD_BE_DEAD|OF_DESTROYED))
			continue;
		if (o->type == OBJ_ROBOT && DTU_ANIM_STATE(o) == DAS_DIE)
			continue;
		if (o->type == OBJ_DRESSING && (o->id >= N_dressing_types || Dressing_info[o->id].pass_thru))
			continue;
		if (vm_vec_dist_quick(&o->pos,&obj->pos) > step + o->size + i2f(30))
			continue;
		d = dtu_object_hit_test(&ip,&obj->pos,end,obj->size,o,obj);
		if (d > 0 && d < best_d) {
			best_d = d;
			best = j;
			*best_ip = ip;
		}
	}
	return best;
}

// ASCENT: the target hint (dtuobj.h). The way of a new missile as
// missiles_frame moves it: a straight line at the same speed, the wall
// test with the radius of the missile up to 1.0, then the bodies on the
// line up to the wall.
int dtu_missile_path_target(vms_vector *pos, int segnum, vms_vector *dir, int weapon_type, int spell, int parent, fix range)
{
	static object stand_in;
	int rtype, model, vclip, fate;
	vms_vector p1, end, ip;
	fvi_query fq;
	fvi_info hit;

	if (weapon_type < 0 || weapon_type >= N_weapon_types || segnum < 0 || parent < 0 || range <= 0)
		return -1;
	memset(&stand_in,0,sizeof(stand_in));
	stand_in.type = OBJ_WEAPON;
	stand_in.id = weapon_type;
	stand_in.pos = *pos;
	stand_in.segnum = segnum;
	stand_in.size = missile_look(weapon_type,spell,&rtype,&model,&vclip);

	vm_vec_scale_add(&p1,pos,dir,range);
	fq.p0 = pos;
	fq.p1 = &p1;
	fq.startseg = segnum;
	fq.rad = (stand_in.size < F1_0) ? stand_in.size : F1_0;
	fq.thisobjnum = parent;
	fq.ignore_obj_list = NULL;
	fq.flags = 0;
	fate = find_vector_intersection(&fq,&hit);
	if (fate != HIT_NONE && fate != HIT_WALL)
		return -1;
	end = (fate == HIT_NONE) ? p1 : hit.hit_pnt;
	return missile_path_object(&stand_in,&end,range,parent,Objects[parent].signature,&ip);
}

// The range of a missile weapon: its speed for its life.
fix dtu_missile_weapon_range(int weapon_type)
{
	if (weapon_type < 0 || weapon_type >= N_weapon_types)
		return 0;
	return fixmul(missile_speed(&Weapon_info[weapon_type]),Weapon_info[weapon_type].lifetime);
}

// The movement of all missiles for one frame (0xA03A8 and 0x5656C).
static void missiles_frame(void)
{
	int i;

	for (i=0;i<=Highest_object_index;i++) {
		object *obj = &Objects[i];
		dtu_missile *m = &Missile[i];
		vms_vector p1, end, best_ip, *vel;
		fvi_query fq;
		fvi_info hit;
		int fate, best;
		fix step;

		if (m->signature == 0)
			continue;
		if (obj->type != OBJ_WEAPON || obj->signature != m->signature || (obj->flags & OF_SHOULD_BE_DEAD)) {
			m->signature = 0;
			continue;
		}

		Missile_life[i] -= FrameTime;
		if (Missile_life[i] < 0) {
			missile_remove(obj);
			continue;
		}

		vel = &obj->mtype.phys_info.velocity;
		// 0x5656C: a missile that is slower than 2.0 goes away
		if (m->bounce < 2 && vm_vec_mag_quick(vel) < i2f(2)) {
			missile_remove(obj);
			continue;
		}

		// A thrown weapon turns by 45 degrees in each frame (0x6BD68). The
		// corrected rule: in each reference frame time.
		if (m->bounce == 3 && obj->render_type == RT_POLYOBJ) {
			int turns = 1;

			if (Dtu_fix_creature_move) {
				m->tumble_acc += FrameTime;
				turns = m->tumble_acc / DTU_REF_FRAMETIME;
				m->tumble_acc -= turns * DTU_REF_FRAMETIME;
			}
			while (turns-- > 0) {
				vms_angvec a;
				vms_matrix rot, t;

				a.p = 0x2000; a.b = a.h = 0;
				vm_angles_2_matrix(&rot,&a);
				vm_matrix_x_matrix(&t,&obj->orient,&rot);
				obj->orient = t;
			}
		}

		vm_vec_scale_add(&p1,&obj->pos,vel,FrameTime);
		step = vm_vec_dist(&p1,&obj->pos);

		fq.p0 = &obj->pos;
		fq.p1 = &p1;
		fq.startseg = obj->segnum;
		fq.rad = (obj->size < F1_0) ? obj->size : F1_0;
		fq.thisobjnum = i;
		fq.ignore_obj_list = NULL;
		fq.flags = 0;
		fate = find_vector_intersection(&fq,&hit);
		end = (fate == HIT_NONE) ? p1 : hit.hit_pnt;

		best = missile_path_object(obj,&end,step,m->parent,m->parent_sig,&best_ip);
		if (best >= 0) {
			missile_hit_object(obj,&Objects[best],&best_ip);
			if (obj->flags & OF_SHOULD_BE_DEAD)
				continue;
		}

		if (fate == HIT_WALL)
			missile_hit_wall(obj,&hit.hit_pnt,&hit.hit_wallnorm,hit.hit_seg >= 0 ? hit.hit_seg : obj->segnum,hit.hit_side_seg >= 0 ? hit.hit_side : -1);
		else if (fate == HIT_NONE) {
			int seg = (hit.hit_seg >= 0) ? hit.hit_seg : find_point_seg(&p1,obj->segnum);

			if (seg < 0)
				missile_remove(obj);
			else {
				obj->pos = p1;
				if (seg != obj->segnum)
					obj_relink(i,seg);
			}
		}
		else
			missile_remove(obj);
	}
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

static void melee_hit(object *target,object *attacker);

// 0x11B06 ai_fire. "AI: critter fires."
static void ai_fire(object *obj,fix dist)
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
	if (w >= 0 && !(target->mtype.phys_info.velocity.x < F1_0 && target->mtype.phys_info.velocity.z < F1_0)) {
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
		// Dtu_improved_pathfinding: a creature that flies falls when it
		// dies. The executable has no gravity for it, so its body stays
		// where it died, in the air.
		if (d != 0 && (!is_flying(obj) || (improved_paths() && DTU_ANIM_STATE(obj) == DAS_DIE)))
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
			half_height = po->body_half_height[frame+1];
			// Dtu_improved_pathfinding: the distance from the origin down to
			// the bottom of the box of the frame (see corpse_on_floor())
			if (improved_paths() && po->box1_min && po->box1_min[frame+1].y < 0)
				half_height = -po->box1_min[frame+1].y;
		}
	}

	vm_vec_copy_scale(&accel,&pi->thrust,fixdiv(F1_0,pi->mass));

	creature_gravity(obj,&accel,cyl_radius,half_height);

	// A dying creature that does not fly falls: one eighth of the velocity
	// in x and z on each frame (0x9F4E2).
	if ((!is_flying(obj) || improved_paths()) && obj->ctype.ai_info.CURRENT_STATE == DAIS_DEAD)
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
			if (Monster_ext[obj->id].door_opening && hit.hit_side_seg >= 0 && hit.hit_side >= 0) {
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
static int door_is_openable(object *obj,segment *segp,int side)
{
	int wn = segp->sides[side].wall_num;

	if (wn < 0 || obj->type != OBJ_ROBOT || !Monster_ext[obj->id].door_opening)
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
	int has_door_bit = (obj->type != OBJ_ROBOT) || Monster_ext[obj->id].door_opening;
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
	if (dist_to_player > i2f(200)) {
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
	return Dtu_fix_cant_break && (Dressing_info[d->id].flags & DRESSING_F_CANT_BREAK);
}

// 0xA6ACB (explode 0) for scenery: explode_sound at volume 1.0, nine
// explosions of break_vclip (default 137) at the size of the object, the
// scenery of the segment falls (movement type 1), then 0x6A3CC removes the
// object and object_create_egg drops what it contains. The nine places of
// the explosions: x and z += rand() % 11.0 - 5.0, y += rand() % 0.6875 -
// 0.3125; rand() is at most 0x7FFF, so the modulo does nothing and the
// places are 5.0 to 4.5 to the side and -0.31 to 0.19 up and down (a
// defect of 1.3, kept). explode 1: 0x911AA with 0x3CA7E: the explosion
// vclip 2 at 2.5 times the size and explode_sound (no debris in the port).
void dtu_break_dressing(object *d,object *by,int explode)
{
	dressing_info *di;
	vms_vector p;
	int i, vclip;

	if (d->type != OBJ_DRESSING || d->id >= N_dressing_types || (d->flags & (OF_SHOULD_BE_DEAD|OF_DESTROYED)))
		return;
	// ASCENT: 0xA6AF8: in level 0x20 (FLAME) object 3, the Flamesword, ends
	// the game (0xA6786): the credits movie and the front end (dtumovie.c).
	// The light and the 50 sounds of 0xA6786 before the movie are not in
	// the port (docs/re/movies.md).
	if (!explode && Current_level_num - 1 == 0x20 && d - Objects == 3) {
		dtumovie_end_credits();
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
		p = d->pos;
		for (i=0;i<9;i++) {
			vclip_at(d->segnum,&p,d->size,vclip);
			p.x += (rand() % i2f(11)) - i2f(5);
			p.z += (rand() % i2f(11)) - i2f(5);
			p.y += (rand() % 0xB000) - 0x5000;
		}
		// the other scenery of the segment falls (guess for the meaning)
		for (i=0;i<=Highest_object_index;i++)
			if (Objects[i].type == OBJ_DRESSING && Objects[i].segnum == d->segnum && &Objects[i] != d)
				Dressing_fall[i] = 1;
	}
	d->flags |= OF_SHOULD_BE_DEAD;
	if (!explode)
		dtu_creature_drop_contents(d);
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
	vms_vector vec_to_player;
	fix dist;
	int A, B, visible = -1, dying, blocked;

	if (obj->type != OBJ_ROBOT || obj->id >= N_robot_types)
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
			// 0x13F09 and the alert 0x160AB: the conditions of the spells
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
		if (ail->activity == 0 && improved_paths() && is_hostile(obj) && ail->path_len >= 2 &&
			 ail->path[ail->path_len-1] == ConsoleObject->segnum && (ail->mode == DAIM_CHASE_OB || ail->mode == DAIM_FOL_PATH) &&
			 ail->target != -1 && !Player_is_dead && !Dtu_player_dead)
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

	// 0x146D0: the distance to the player
	ail->time_since_processed += FrameTime;
	vm_vec_sub(&vec_to_player,&ConsoleObject->pos,&obj->pos);
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

	// 0x1498B: a creature that is far away is not processed on each frame
	if (ail->awareness < 3) {
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

	// 0x15074: the sight
	visible = player_is_visible(obj);
	creature_sight_sound(obj,visible,&vec_to_player);
	if (visible)
		ail->time_player_seen = GameTime;

	// The awareness of a creature goes up by the awareness events of the
	// executable (0x16055, 0x16366: noise of the player), which are not in
	// the port. In their place a hostile creature that sees the player has
	// the awareness 1 at least. (guess)
	if (visible && is_hostile(obj) && ail->awareness < 1) {
		ail->awareness = 1;
		ail->awareness_time = i2f(3);
	}

	// 0x14AF2: the door opener, for each creature with the door bit. It
	// only sends the creature to a door. The door opens when the creature
	// walks into it (dtu_creature_physics_sim).
	if (Monster_ext[obj->id].door_opening && aip->behavior != DAIB_STATION && ail->mode != DAIM_CHASE_OB) {
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
				if (improved_paths() && Dtu_fix_path_follow && aip->behavior != DAIB_HOME && aip->behavior != DAIB_GUARD &&
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
			else if (GameTime - ail->time_player_seen > i2f(8)) {
				if (ail->morale >= B)
					create_path_to_player(obj,10);
			}
			else if (aip->CURRENT_STATE != DAIS_REST && aip->GOAL_STATE != DAIS_REST) {
				// Dtu_improved_pathfinding: also a creature that sees the
				// player but cannot walk to the player in a straight line
				// (the line is not free for its body) takes the path.
				if (improved_paths() && Ref_ticks_now > 0)
					ail->body_line = (obj->segnum == ConsoleObject->segnum) || body_line_free(obj,&ConsoleObject->pos);
				if (Dtu_fix_path_follow && (!visible || (improved_paths() && !ail->body_line)) && aip->behavior != DAIB_STATION && chase_by_path(obj)) {
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

		if (dot >= 3*F1_0/4 && aip->GOAL_STATE <= DAIS_ATAK && aip->GOAL_STATE != DAIS_FLIN) {
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
						ai_fire(obj,dist);
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
	morale_check(obj);
}

// 0xA6505 (not read in full): makes a creature active, as the wake up of
// do_ai_frame does (0x13D80).
void dtu_creature_wake(object *obj, int make_hostile)
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

// The four pickup functions of collide.c (0x2805B misc, 0x283DC armor,
// 0x28273 spell, 0x284E0 weapon).
static void pick_up_item(object *player,object *item)
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
	if (item->type == OBJ_WEAPON && (item->render_type != RT_ITEM || item->lifeleft != IMMORTAL_TIME))
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

		item->flags |= OF_SHOULD_BE_DEAD;
		HUD_init_message("You pick up %s.",it->name);
		pickup_sound(flag_10);
		dtu_obj_log("pickup: obj %d type %d id %d '%s' count %d, inventory entry %d, sound %s",(int)(item-Objects),
			item->type,item->id,it->name,count,index,flag_10 ? "2216 2215" : "2215");
	}
	else {
		static fix last_message = 0;

		if (GameTime - last_message > F1_0*2 || GameTime < last_message) {
			HUD_init_message("You cannot carry %s.",it->name);
			last_message = GameTime;
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
	Dressing_pushed[n] = 1;
	dtu_obj_log("push: obj %d %s (strength %d) pushes obj %d %s (pushable %d), velocity %.1f %.1f",(int)(mover-Objects),
		dtu_object_name(mover),strength,n,dtu_object_name(d),dressing_pushable(d),f2fl(pi->velocity.x),f2fl(pi->velocity.z));
}

// 0x9FE68: the move of scenery with movement type 1. The velocity is
// halved and x and z are divided by 4, then gravity and the move. The
// sound 2706 plays while the speed is above 0x70. The move ends at a stop.
static void dressing_move_frame(void)
{
	static fix acc = 0;
	int i, ticks = 1;

	if (Dtu_fix_push_frame) {
		acc += FrameTime;
		ticks = acc / DTU_REF_FRAMETIME;
		acc -= ticks * DTU_REF_FRAMETIME;
	}
	for (i=0;i<=Highest_object_index;i++) {
		object *d = &Objects[i];
		physics_info *pi = &d->mtype.phys_info;
		vms_vector p1, frame_vec;
		fvi_query fq;
		fvi_info hit;
		fix speed;
		int k, fate, n_ticks;

		if (!Dressing_fall[i])
			continue;
		if (d->type != OBJ_DRESSING || d->signature != Dressing_sig[i] || (d->flags & OF_SHOULD_BE_DEAD)) {
			Dressing_fall[i] = 0;
			continue;
		}
		// ASCENT: Dtu_fix_push_frame. The push gives 8 times the velocity
		// of the mover, and the slow down before the move of the frame
		// divides it by 8, so that the scenery moves with the mover in
		// that frame. Then the velocity goes down by 8 times in each
		// reference frame. A frame of a push has that one slow down at each
		// frame rate (without it, a frame that has no reference tick moved
		// the scenery with 8 times the velocity of the mover).
		n_ticks = ticks;
		if (Dtu_fix_push_frame && Dressing_pushed[i])
			n_ticks = 1;
		Dressing_pushed[i] = 0;
		for (k=0;k<n_ticks;k++) {
			pi->velocity.x = (pi->velocity.x / 2) >> 2;
			pi->velocity.z = (pi->velocity.z / 2) >> 2;
			pi->velocity.y /= 2;
		}
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
		if (speed <= 0x70) {
			vm_vec_zero(&pi->velocity);
			Dressing_fall[i] = 0;
			dtu_obj_log("push: obj %d %s stops at %.1f %.1f %.1f",i,dtu_object_name(d),f2fl(d->pos.x),f2fl(d->pos.y),f2fl(d->pos.z));
			continue;
		}
		vm_vec_copy_scale(&frame_vec,&pi->velocity,FrameTime);
		vm_vec_add(&p1,&d->pos,&frame_vec);
		fq.p0 = &d->pos; fq.p1 = &p1; fq.startseg = d->segnum; fq.rad = d->size / 2;
		fq.thisobjnum = i; fq.ignore_obj_list = NULL; fq.flags = 0;
		fate = find_vector_intersection(&fq,&hit);
		if (fate == HIT_NONE) {
			int seg = find_point_seg(&p1,d->segnum);
			if (seg >= 0) {
				d->pos = p1;
				if (seg != d->segnum)
					obj_relink(i,seg);
			}
		}
		else {
			pi->velocity.x = pi->velocity.z = 0;
			if (fate == HIT_WALL && hit.hit_wallnorm.y > F1_0/2)
				pi->velocity.y = 0;
		}
		if (FrameCount % 8 == 0)
			sound_at(2706,d->segnum,&d->pos,F1_0/2);
	}
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
				// collide_robot_and_player (0x26FC1): a hostile creature
				// that the player touches is alerted. The bump (0x2627C) is
				// not in the port: the bodies stop in the movement code.
				if (is_hostile(b) && Ail[b-Objects].awareness < 3) {
					Ail[b-Objects].awareness = 3;
					Ail[b-Objects].awareness_time = i2f(3);
				}
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

// The option -testpos X,Y,Z[,HEADING]: puts the player at a position of the
// level at the first frame, for tests and screenshots. The heading is in
// degrees, 0 looks along +z, 90 along +x.
static void test_place_player(void)
{
	int t = FindArg("-testpos");
	float x, y, z, h = 0;
	vms_vector pos;
	vms_angvec a;
	int seg;

	Test_pos_pending = 0;
	if (!t || t+1 >= Num_args)
		return;
	if (sscanf(Args[t+1],"%f,%f,%f,%f",&x,&y,&z,&h) < 3)
		return;
	pos.x = fl2f(x); pos.y = fl2f(y); pos.z = fl2f(z);
	seg = find_point_seg(&pos,-1);
	if (seg < 0) {
		printf("-testpos: the point is in no segment\n");
		return;
	}
	ConsoleObject->pos = pos;
	obj_relink(ConsoleObject-Objects,seg);
	a.p = a.b = 0;
	a.h = (fixang)(int)(h * 65536.0 / 360.0);
	vm_angles_2_matrix(&ConsoleObject->orient,&a);
	vm_vec_zero(&ConsoleObject->mtype.phys_info.velocity);
	// the new segment is not a move into it: no warp or floor trigger
	dtu_trig_player_placed();
}

// The triggers of a frame run before the objects (switch.c): -testpos must
// move the player before the warp test of the start segment.
void dtu_test_place_player(void)
{
	static int tour = -1, step = 0, delay = 0;

	if (Dtu_objects_enabled && Test_pos_pending)
		test_place_player();
	// -testtour [FRAMES]: the player visits the center of each segment, one
	// frame for each of 4 headings, so that a test sees every side of the
	// level (the requests of the upscaled textures: ASCENT_GPU_HDLOG=1).
	// With FRAMES the tour starts after that many frames (after a warp of
	// the start segment, for example).
	if (tour < 0) {
		int t = FindArg("-testtour");
		tour = t != 0;
		if (t && t+1 < Num_args && Args[t+1][0] >= '0' && Args[t+1][0] <= '9')
			delay = atoi(Args[t+1]);
	}
	if (tour && delay > 0) {
		delay--;
		return;
	}
	if (tour && ConsoleObject && step / 4 <= Highest_segment_index) {
		int seg = step / 4;
		vms_angvec a;

		compute_segment_center(&ConsoleObject->pos,&Segments[seg]);
		if (seg != ConsoleObject->segnum)
			obj_relink(ConsoleObject-Objects,seg);
		a.p = a.b = 0;
		a.h = (fixang) ((step % 4) * 0x4000);
		vm_angles_2_matrix(&ConsoleObject->orient,&a);
		vm_vec_zero(&ConsoleObject->mtype.phys_info.velocity);
		dtu_trig_player_placed();
		if (++step / 4 > Highest_segment_index)
			printf("-testtour: %d segments visited\n", seg + 1);
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
