/*
 * ASCENT: new file. The objects of Descent to Undermountain other than the
 * player: the setup of creatures, scenery and items after a level is
 * loaded, the animation of the polygon models, the behavior of the
 * creatures (ai.c, pmon.c of DTUGAME.EXE), the melee fight, the pickup of
 * items and the inventory.
 *
 * The facts are in docs/re/models.md, docs/re/objects.md and docs/re/ai.md.
 * The address of the original code is at each function of dtuobj.c.
 */
#ifndef _DTUOBJ_H
#define _DTUOBJ_H

#include "types.h"
#include "fix.h"
#include "vecmat.h"
#include "object.h"

//------------------------------------------------------------------------
// Animation states (the names of the table at 0x13A764 of DTUGAME.EXE).
// The states 11 to 18 have no name. 11 and 12 are attacks of the player.

#define DAS_REST			0
#define DAS_STAND			1
#define DAS_WALK			2
#define DAS_SIDLE_R		3
#define DAS_SIDLE_L		4
#define DAS_RUN			5
#define DAS_ATTACK		6
#define DAS_ATTACK2		7
#define DAS_FLINCH		8
#define DAS_DIE			9
#define DAS_FIDGET		10
#define DAS_ATTACK3		11
#define DAS_ATTACK4		12
#define DTU_N_ANIM_STATES	19

// The first frame of each state. Entry s+1 is the end of state s (the
// table at 0x13D9E0).
extern short Dtu_anim_first[DTU_N_ANIM_STATES+1];

// The animation values of an object (0x170, 0x172, 0x176 of the object of
// the executable).
#define DTU_ANIM_STATE(obj)	((obj)->dtu.m_170)
#define DTU_ANIM_FRAME(obj)	((obj)->dtu.m_172)
#define DTU_ANIM_GOAL(obj)		((obj)->dtu.m_176)

// AI states (0x13A73C), AI modes (0x13A6D0), behaviors (0x13A70F)
#define DAIS_NONE		0
#define DAIS_REST		1
#define DAIS_SRCH		2
#define DAIS_LOCK		3
#define DAIS_FLIN		4
#define DAIS_ATAK		5
#define DAIS_RECO		6
#define DAIS_DEAD		7

#define DAIM_STILL		0
#define DAIM_WANDER		1
#define DAIM_FOL_PATH	2
#define DAIM_CHASE_OB	3
#define DAIM_RUN_FROM	4
#define DAIM_HOME			5
#define DAIM_OPENDOOR	6

#define DAIB_GUARD		0x80
#define DAIB_WANDER		0x81
#define DAIB_HOME			0x82
#define DAIB_PATROL		0x83
#define DAIB_STATION		0x84

// Bits of the int at 0x1B0 of the object (dtu.m_1b0)
#define DTU_SCRIPT_MASK		0x000001ff
#define DTU_PEACEFUL			0x00000200
#define DTU_HOSTILE			0x00400000
#define DTU_FLEEING			0x00800000

//------------------------------------------------------------------------
// Settings. 1 (default) is the corrected rule, 0 is the rule of the
// executable, which depends on the frame rate. The corrected rules give the
// behavior of the executable at a frame time of DTU_REF_FRAMETIME (3/64
// second, physics.h). The option -origobj clears them all.

// The animation of a creature that is not active (0x14013). Original: one
// step for each game frame (STAND: for each tenth game frame). Corrected:
// one step for each reference frame time.
extern int Dtu_fix_idle_anim;

// The tests "(clock + objnum) % N == 0" of the AI, which are true in one
// game frame of N. Corrected: true one time in N reference frame times.
extern int Dtu_fix_ai_ticks;

// The movement of a creature (pmon.c). Original: the velocity becomes one
// half on each frame, two steps of thrust, drag and gravity at most on each
// frame, the lift out of the floor is 2.0 on each frame, the blocked move
// counter counts frames. Corrected: each of these per reference frame time.
extern int Dtu_fix_creature_move;

// The sight of a creature. Original: the bit of the BRN file of the level
// for the two segments (0x9810D). The BRN files of the disc are copies of
// one matrix that fits no level. Corrected: a ray from the creature to the
// player.
extern int Dtu_fix_brain;
// 0x9810D for the segment of the creature and of the player (the rule of
// Dtu_fix_brain).
int dtu_brain_sees_player(object *obj);

// 0 (default) is the executable: do_ai_frame puts a creature that is not
// active back to the first frame of STAND each time it is processed
// (0x14A54), so the idle animation of a creature that is nearer than 100.0
// does not run. 1: the idle animation runs at all distances. This is not a
// rule that depends on the frame rate. Option -idleanim.
extern int Dtu_idle_anim_near;

// 1 after the player has less than one hit point. The player object stays
// in the level and does not move, see dtu_player_death_frame().
extern int Dtu_player_dead;

// 0 stops all attacks of creatures (the switch at 0x13A6B4 of the
// executable). Option -noattack.
extern int Dtu_creatures_attack;

// A walking creature on a path. Original (0x17CA8): it does not walk to the
// point of the path, it walks to the player, and so into the wall. Only a
// flying creature follows the path. Corrected: a walking creature walks to
// the point of the path, and a hostile creature in the mode CHASE_OB that
// does not see the player takes a path to the segment of the player in
// place of the straight line.
extern int Dtu_fix_path_follow;

// The shot of a ranged creature. Original (0x13A2A): a shot in each game
// frame in which the animation is on the fire frame, so the number of
// missiles of one attack is the number of game frames that the fire frame
// lasts. Corrected: one shot for each attack animation.
extern int Dtu_fix_fire_once;

// The attack sound of a creature that sees the player (compute_vis_and_vec
// 0x12E2F to 0x12F3C). Original: the code needs the visibility 2, which the
// sight function never gives, so the sound never plays. Corrected: it
// plays by the rules of that code.
extern int Dtu_fix_attack_sound;

// The corpse of a creature (0x6A48B). Original: the body goes to the floor
// by its torso box, which leaves a body drawn in the air or in the floor,
// and a creature that flies stays in the air when it dies. Corrected: the
// dying body falls and the corpse lies with its lowest drawn point on the
// floor, also a corpse of a loaded level.
extern int Dtu_fix_corpse_floor;

// The tint of Detect Magic (0x14C4CD, docs/re/spells.md). Original: a
// magic bitmap or rod object leaves the tint on for the next polygons until
// a polygon model clears it, and the linear texture mapper tints only the
// first scanline after a change, near black. Corrected: the tint is for the
// object only, and both mappers draw the blue ramp. -origobj clears it.
extern int Dtu_fix_magic_tint;

// The members of the Shadow Thieves at the level start when the player
// is in the guild (quest flag 67). Original: the creature setup by name
// (0xAC7B2) makes 15 of their creature types peaceful, then add_npc
// (0xAC431) makes each one without a "say" script hostile again, with no
// act of the player. Corrected: the peace of the setup stays. -origobj
// and -origguildpeace clear it. See docs/re/ai.md, "Creature setup by name".
extern int Dtu_fix_guild_peace;

// The revive of a creature with "regeneration" (the trolls). Original: the
// damage function clears all effects of a creature that dies (0x2A1D0,
// 0x916D9), also the regeneration bit 0x01000000 that the revive code of
// do_ai_frame needs (0x1440C). So a killed troll never gets up, and a kill
// with the bit set gives no experience (0x2A0C5, 0x9162B). Corrected (1,
// default): the bit stays unless fire or acid damage took it (0x91E3D), the
// corpse regenerates one hit point each "regeneration" seconds and gets up
// with all hit points, as the code of the executable does (and as the troll
// of AD&D 2nd edition does). A troll that took fire or acid damage stays
// dead and gives its experience. The options -origobj and -origrevive clear
// it. See docs/re/ai.md, "Regeneration and revive".
// ASCENT: with the setting, a creature that falls and can get up gives no
// experience and is not counted as a kill. It is destroyed for good, with
// its experience one time, when fire or acid hurts the fallen body (the
// troll of the Monstrous Compendium) or when weapons hack the fallen body
// apart (damage equal to its full hit points while it is down). See
// docs/audit/monsters.md, "Dtu_fix_revive".
extern int Dtu_fix_revive;

// The hatchling of DROW5 (object 83) at the level fixes (0xAD2A1).
// Original: its shields come from the hit points of the type with the
// object number as the index (past the monster table, a word that is 0),
// so it starts with 0 hit points. Corrected: the hit points of its type
// (100, set by the same fix). -origobj and -origdrow5 clear it.
extern int Dtu_fix_drow5_shields;

// ASCENT: the pick up of a stack that does not fit (0xB65BB, 0xB67D3 to
// 0xB68C1). Original: the count is added and cut to stack_limit, the
// items above the limit are lost. Corrected: the inventory takes what
// fits, the rest stays on the floor. 0 (-origobj): the rule of the
// executable. Dtu_inventory_left: the items of the last
// dtu_inventory_add() that it did not take.
extern int Dtu_fix_partial_stack;
extern int Dtu_inventory_left;

// The spellcaster of KOBOLDS (object 19). Original: the level fix 0xACF23
// gives its type weapon 18 only on a level with no saved state, so after
// the player comes back the creature setup makes the spell weapon -33 of
// the table weapon 33 with 1d4. Corrected: weapon 18 at each visit.
extern int Dtu_fix_kobold_weapon;

// The hit points of scenery at the restore of a level (0xAE3F2 at
// 0xBB1A7). Original: each piece gets 1.0, also one that the level fix
// gave 256 (DROW5 object 13, IBBALAR object 2) or that was hit. Corrected:
// the saved hit points stay.
extern int Dtu_fix_scenery_restore;

// The texture of the two spinning pieces of scenery of BANE (0xAD003).
// Original: one of four lava texture numbers that the start of the
// previous level looked up (0 if that level has no such texture).
// Corrected: the numbers of BANE.
extern int Dtu_fix_bane_texture;

// ASCENT: the nine explosions of broken scenery (0xA6ACB). Original: all
// on one side of the object. Corrected: spread around it. -origobj clears it.
extern int Dtu_fix_break_spread;

// ASCENT: the alert and the noise (0x160AB, 0x162A1, see dtuobj.c).
// Corrected: noise stops at a closed door, the wake ray goes to the caller,
// the last object slot wakes too. -origobj clears them.
extern int Dtu_fix_noise_walls;
extern int Dtu_fix_alert_ray;
extern int Dtu_fix_alert_last;

// ASCENT: the strength bonus of a creature (0x2A789 from 0x2ACA7). 1: the
// plain bonus. 0 (-origcombat): with the 18/xx part when the player is a
// fighter, as 1.3.
extern int Dtu_fix_creature_strength;
// 0x670D4: 0, 1 a magic object, 2 a creature with a magic weapon.
int dtu_magic_look(object *obj);
void dtu_set_tint(int tint);
int dtu_get_tint(void);
// The blob and the rod: dtu_object_tint_end(dtu_object_tint_begin(obj))
// around the draw.
int dtu_object_tint_begin(object *obj);
void dtu_object_tint_end(int old);
// The weapon type of a spell missile (0x571B2).
void dtu_spell_template_used(int weapon_type);

// Seconds from the death of the player to the death screen (3.0 in the
// executable, 0x423F5), and from there to the new start of the level in
// the port (the executable shows the death menu there).
#define DTU_DEATH_WAIT			(3*F1_0)
#define DTU_DEATH_RESTART_WAIT	(4*F1_0)

//------------------------------------------------------------------------
// The inventory of the player. The executable keeps it in the player
// structure (insert function 0xB65BB, not ported). This is a plain list
// for the play screen: one entry for each stack of items.

// ASCENT: the executable has two lists: 127 items and 22 key items (misc
// numbers 107 to 124, see docs/re/character.md). The port keeps both in
// this list and has the same limits, so a save game of the original fits.
#define DTU_MAX_ITEM_ENTRIES	127
#define DTU_MAX_KEY_ENTRIES	22
#define DTU_MAX_INVENTORY		(DTU_MAX_ITEM_ENTRIES + DTU_MAX_KEY_ENTRIES)
#define DTU_IS_KEY_ITEM(type,id)	((type) == OBJ_MISC && (id) >= 107 && (id) <= 124)

typedef struct dtu_inv_item {
	ubyte		type;				// OBJ_WEAPON, OBJ_ARMOR, OBJ_MISC or OBJ_SPELL. 0: the entry is free
	ubyte		id;				// index of the table of the type (Weapon_item_info, Armor_info, Misc_info, Spell_info)
	short		count;			// number of items of the stack, or the charges of one item
	short		icon;				// icon= of the table line, or -1
	short		inv_icon;		// inv_icon= of the table line, or -1
	short		obj_bitmap;		// picture= of the table line: slot of ObjBitmapPtrs, or -1
	short		level;			// number of the level where the item was found
	char		name[40];		// name= of the table line
	// ASCENT: the 16-byte record of the save game that the entry came from
	// (dtu_char_load_save). dtu_char_pack writes it back with the values
	// of the entry, so the bytes that the port does not use stay.
	ubyte		has_record;
	ubyte		record[16];
} dtu_inv_item;

extern dtu_inv_item Dtu_inventory[DTU_MAX_INVENTORY];
extern int Dtu_n_inventory;		// entries in use, they are at the start of the list
extern int Dtu_gold;					// the gold of the player (misc id 1)

// Adds "count" items. Returns the index of the entry, or -1 if the list is
// full. A stackable type goes to the entry that has it already. A spell of
// a class of the character goes to its spell list (0xB65BB): the result is
// then DTU_INV_SPELL_LIST, not an index.
#define DTU_INV_SPELL_LIST	9999
int dtu_inventory_add(int type, int id, int count);
// ASCENT: a new entry at the end of the list, with no stacking (the load of
// a save game keeps the entries of the file). Returns the index or -1.
int dtu_inventory_append(int type, int id, int count);
// Takes "count" items from entry "index". The entry goes away at count 0.
void dtu_inventory_remove(int index, int count);
void dtu_inventory_clear(void);

//------------------------------------------------------------------------
// Calls from the game.

// One time, after init_game(): the collision table for the new types and
// the options -objlog FILE, -origobj, -noattack, -noobjects.
void dtu_objects_init(void);

// After a level is loaded and its models have their numbers: sets the
// model, the bitmap, the size and the animation of each object from the
// tables (verify_object 0x477D9, setup_object 0xAE3F2, place_creatures
// 0x6A6FB, fix_object_segs 0x69FFB).
void dtu_objects_level_setup(void);
// The same for the restore of a level state: no level fixes of the objects
// (0xACCDB runs before the restore in the executable).
void dtu_objects_level_setup_restore(void);

// ASCENT: the AI state of creature objnum in a state file of the original:
// al is its 47-byte ai_local record, img its 479-byte object image
// (docs/re/levelstate.md). dtu_ai_from_image runs after
// dtu_objects_level_setup().
void dtu_ai_to_image(int objnum, ubyte *al, ubyte *img);
void dtu_ai_from_image(int objnum, const ubyte *al, const ubyte *img);

// The control of a creature for one frame (do_ai_frame 0x13BF4).
void dtu_do_ai_frame(object *obj);

// The movement of a creature for one frame (pmon.c 0x9F153).
void dtu_creature_physics_sim(object *obj);

// Called one time per frame before the objects move (the animation and the
// melee attack of the player).
void dtu_objects_frame(void);
void dtu_test_place_player(void);	// -testpos (dtu_trig_frame)

// Rendering. dtu_draw_polygon_object() is the normal case of
// draw_polygon_object (0x676ED). dtu_draw_item() is render type 8
// (0x67E2D). dtu_draw_dressing_vclip() is render type 7 of scenery.
void dtu_draw_polygon_object(object *obj, fix light, fix *glow, bitmap_index *alt_textures);
void dtu_draw_item(object *obj);

// The splash of a step of the player in water (dtufoot.c)
void dtu_water_splash(object *obj, fix side_step);
// 1 for a splash of dtu_water_splash(): it casts no light (lighting.c)
int dtu_is_water_splash(object *obj);
void dtu_draw_dressing_vclip(object *obj);

// Collision handlers (collide.c of the executable). Each returns 1 if it
// handled the pair.
int dtu_collide_two_objects(object *a, object *b, vms_vector *collision_point);

// An object of type 5 that lies as an item (not a shot): the item render
// type and no life time (0x28502).
int dtu_weapon_is_item(object *obj);

// Damage (0x29A9C). "attacker" can be NULL. With an attacker these two
// functions throw the to-hit roll and the dice of the weapon of the
// attacker (the weapon in the hand for the player) and do not use
// "damage". Without an attacker "damage" is taken as it is.
void dtu_damage_player(object *player, object *attacker, fix damage);
void dtu_damage_creature(object *creature, object *attacker, fix damage);

// Damage with no to-hit roll and no dice: the caller has rolled already
// (the attacks and spells of the player in dtucombat.c and dtuspell.c,
// traps). "attacker" can be NULL. With the player as the attacker the
// creature gets hostile, the player gets the message, and the experience
// if the creature dies. The creature flinches, and dies below 1.0 (drops,
// sound, corpse). Returns the damage that was done (0 for a dead target).
fix dtu_damage_creature_direct(object *creature, object *attacker, fix damage);
fix dtu_damage_player_direct(object *player, object *attacker, fix damage);

// The effect of a spell on a target (spells.c 0x8CFFF, 0x90CEF). The
// missile code calls it when a missile that carries a spell hits an
// object (the spell of dtu_create_missile, or enemy_spell of the weapon).
// "caster" can be NULL. "level" is the level of the caster: the mage level
// of a monster, dtu_player_level() for the player, 1 for a trap. Returns
// the damage that was done. dtuobj.c has a weak definition with only the
// damage (dtu_spell_damage_roll and dtu_damage_*_direct). The spell code
// can define it with all effects.
fix dtu_spell_hit(int spell, object *target, object *caster, int level);

// 0x91BC3: the damage of a spell on a target in hit points: the dice
// damage_dice, fire_damage_dice, acid_damage_dice and elec_damage_dice of
// the spell table, each plus its *_damage_level times "level", with the
// immunity and resistance of the target. It changes the counter of
// Protection From Fire and ends the regeneration of the target on fire or
// acid damage.
int dtu_spell_damage_roll(int spell, object *target, int level);
// The same with the number of dice "count" for the group "group" (0 base,
// 1 fire, 2 acid, 3 electricity), as the cases of 0x8CFFF write it.
int dtu_spell_damage_roll_count(int spell, object *target, int level, int group, int count);


// The body cylinder of an object for collisions (0x748D9) and the size for
// physics (0x746B3).
void dtu_get_body_cylinder(object *obj, vms_vector *center, fix *radius, fix *half_height);
void dtu_get_body_size(object *obj, fix *radius, fix *half_height);

// The name of an object from its table ("chair", "kob_swrd"). The logs
// use it.
char *dtu_object_name(object *obj);

// ASCENT: 1 for the monster type with the word "rat" of the table (bit 4
// of +0xD9, set by the table parser 0x1DDE6, which also keeps the type in
// 0x13A820). The "Rat" of each table also has pass_thru. Such a creature
// runs its AI frame with the flag 0x40 (0x13C8B, 0x9F174), does 1 damage
// (0x2B58D) and is squashed when it touches a player with boots (0x26FE0).
int dtu_rat_type(int id);

// 0x2AA5B to 0x2AF4F of the to-hit roll 0x2A7F3: the effects of the
// attacker and of the target (Levitate, hidden or invisible, Bless,
// Protection From Evil, the armor class of a creature, Slow), the test
// thac0 - ac <= roll, and Stone Skin, which takes a hit. roll is the d20
// with the bonuses of the attacker. Returns 1 for a hit, 0 for a miss.
int dtu_hit_test(object *target, object *attacker, int roll, int thac0, int *ac_out);
// ASCENT: the same against a melee attack (missile 0) or a missile (1): the
// armor class of Shield (Dtu_fix_shield_ac). dtu_hit_by_missile() is 1 while
// the hit of a weapon missile is applied.
int dtu_hit_test_vs(object *target, object *attacker, int roll, int thac0, int *ac_out, int missile);
int dtu_hit_by_missile(void);
// 0x2AF5E: Flame Tongue, Mace of Disruption and the fire dice of a weapon
// (any attacker). *damage is the normal part. Returns the fire part.
int dtu_weapon_extra_damage(object *attacker, object *target, int weapon, int *damage);

// The name of an object in the messages to the player (0xA7F03): for a
// creature the name of the list 0xA504A ("Kobold"), else dtu_object_name.
char *dtu_object_display_name(object *obj);
// ASCENT: the grammatical gender of that name in the language of the text
// (dtulang.h: DTU_GENDER_M, DTU_GENDER_F, or 0 when not known)
int dtu_object_name_gender(object *obj);
// ASCENT: the English name of an item as the player sees it: name= of the
// table line, or its correction (dtuitemfix.h). type is OBJ_WEAPON,
// OBJ_ARMOR or OBJ_MISC; another type gives name.
char *dtu_item_shown_name(int type, int id, char *name);

//------------------------------------------------------------------------
// The values of the player character that the fight needs. dtuobj.c has a
// weak definition of each function with the values of a first level
// fighter with no weapon. The character code defines them for real.

int dtu_player_thac0(void);				// the byte at 0x1D4D78
int dtu_player_armor_class(void);		// the word at 0x1D33C9
// 0x2A7F3 for the player as the attacker: 2 a critical hit, 1 a hit, 0 a
// miss, -1 a fumble. weapon: -1 for the empty hand.
int dtu_player_attack_roll(object *target, int weapon);
// 0x2AF5E for the player as the attacker: the damage of one roll.
int dtu_player_weapon_damage(int weapon, object *target);
int dtu_player_level(void);				// the word at 0x1D33CB plus 1
fix dtu_player_hit_points(void);			// 0x1D336F
fix dtu_player_max_hit_points(void);	// the word at 0x1D33BD as a fix
// Takes "amount" hit points (a negative amount gives hit points, up to the
// maximum). Returns the hit points that are left. The caller starts the
// death of the player below 1.0.
fix dtu_player_damage(fix amount);
// Experience points for a kill (0x4EA50 with the score value of the monster)
void dtu_player_award_xp(int amount);

// The front end can define this function. It is called one time when the
// death screen of the executable would come (0x423F5: the picture, the
// death menu "Load Game", "Abort Game"). Return 1 if the front end takes
// over. The weak definition returns 0, and the port starts the level again.
int dtu_hook_player_death_screen(void);

// Called by the game loop after each frame: the death sequence of the
// player. Returns 1 if it started the level again.
int dtu_player_death_frame(void);

// 1 while the game waits after the death of the player (0x423F5): the
// world does not move, and a key ends the wait.
int dtu_player_death_wait(void);

// Starts the death of the player if the hit points are below 1.0 (0x33C5E:
// the status display tests them each frame). dtu_objects_frame() calls it
// on each frame. "killer" can be NULL.
void dtu_player_check_death(object *killer);

//------------------------------------------------------------------------
// Calls from dtuhook.c (traps, triggers, the use action).

// The state check of the soak test (dtusoak.c): the AI values of each
// creature. Writes the first problem to msg and returns 1, or returns 0.
int dtu_objects_check(char *msg, int size);

// Turn undead (0xADC46): turned 1 makes the creature flee (not hostile,
// no target), turned 0 makes it hostile (a failed turn, or "unable to
// affect"). Both start the creature again and check its morale.
void dtu_creature_turn(object *obj, int turned);
// 0x10ADF: the morale function of a creature
void dtu_creature_morale(object *obj);

// 0xA6505: makes a creature active. With make_hostile it attacks the player.
void dtu_creature_wake(object *obj, int make_hostile);

// 0x160AB: the alert function, levels 3 to 5 wake the creatures that see
// the segment of the caller (docs/re/ai.md, "Alert function").
void dtu_creature_alert(object *caller, int level);

// gate_in_robot (0x1374D): a new creature of the type at the center of the
// segment. Returns the object number or -1.
int dtu_gate_in_creature(int type, int segnum);

// Laser_create_new (0x55511). parent is an object number or -1. spell is
// the number of the spell that the missile carries, or -1. Returns the
// object number of the missile or -1.
int dtu_create_missile(vms_vector *dir, vms_vector *pos, int segnum, int parent, int weapon_type, int spell, int make_sound);

// ASCENT: the target hint (Dtu_target_hint, dtucombat.c). The object that
// a missile fired now from pos along dir would hit first (the test of the
// missile movement: the walls, then the bodies on the line), or -1. range
// is the length of its flight. Nothing is created.
int dtu_missile_path_target(vms_vector *pos, int segnum, vms_vector *dir, int weapon_type, int spell, int parent, fix range);
// The length of the flight of a missile weapon: its speed for its life.
fix dtu_missile_weapon_range(int weapon_type);
// 1 if the object is a creature that is hostile to the player.
int dtu_creature_is_hostile(object *obj);

// The burst of a weapon with a min_speed (0x93294): fragments in random
// directions.
void dtu_create_burst(int weapon_type, int segnum, vms_vector *pos);

// A new item object (object_create_egg 0x3C4BD). Returns the object number.
int dtu_create_item(int type, int id, int count, int segnum, vms_vector *pos);

// 0x9493A: the floor of the segment opened. Its objects fall.
void dtu_drop_objects_in_segment(int segnum);

// The script of a creature (dtuhook.c): 1 if the script with this id has a
// "say" statement, 0 if it has none, -1 if there is no script.
int dtu_script_has_say(int id);

// Values of the character for the use action and the scripts (weak
// definitions in dtuhook.c): the skill "climb walls" in percent
// (0x1D33EF), the class (0 to 3), the race (0 to 5) and the sex (0, 1) as
// the terms of a script count them.
int dtu_player_climb_skill(void);
int dtu_player_script_class(void);
int dtu_player_script_race(void);
int dtu_player_script_sex(void);

// The use action on an object (0xAC155): the ray of 30.0 along the view of
// the player. Returns 1 if it hit a creature (talk or "No response...").
int dtu_use_object(object *player);

// The log of the option -objlog (state changes of creatures, damage,
// pickup). The text starts with the game time and the frame number.
void dtu_obj_log(char *format, ...);

// Scenery (0xA6ACB, 0x9FA41): settings, see dtuobj.c. -origobj clears them.
extern int Dtu_fix_cant_break;		// 1: the keyword cant_break works
extern int Dtu_fix_push_body;		// 1: pushed scenery and the bodies: no stop inside of the player
extern int Dtu_fix_push_rate;		// 1: pushed scenery: the lead at the end of a push of the reference frame
extern int Dtu_fix_push_wall;		// 1: pushed scenery: the box of the model stays out of the walls
// 1 if the scenery cannot break
int dtu_dressing_unbreakable(object *d);
// 0xA6ACB (explode 0) or 0x911AA with 0x3CA7E (explode 1): the scenery
// breaks. by is the object that breaks it, or NULL.
void dtu_break_dressing(object *d, object *by, int explode);

// The spell that a missile carries, or -1.
int dtu_missile_spell(object *obj);
// The life of a missile (Weapon_info+0xA8 that 0x56FF6 writes).
void dtu_missile_set_life(int objnum, fix life);
// object_create_egg (0x3C4BD): the object drops what it contains.
void dtu_creature_drop_contents(object *obj);
// 0xA9F58: the creature leaves the level (Abjure).
void dtu_creature_vanish(object *obj);
// 1 if the creature is active (+0x1CC == 2).
int dtu_creature_is_active(object *obj);

// ASCENT: Dtu_fix_revive. 1 if obj is the fallen body of a creature that
// can get up again (dying or lying down, the regeneration bit kept).
int dtu_creature_down(object *obj);
// A hit on such a body: damage of weapons (hacks the body), burn 1 for fire
// or acid (destroys it). The attacker gets the experience.
void dtu_creature_down_hit(object *obj, object *attacker, int damage, int burn);
// A spell on such a body (an area spell that reaches it): the dice of the
// spell; fire or acid destroys it.
void dtu_creature_down_spell(object *obj, object *caster, int spell, int level);
// The swing of the player hits such a body (dtucombat.c): no roll (the body
// is helpless), the damage of the weapon; fire of the weapon destroys it.
void dtu_creature_down_swing(object *obj, int weapon);
// The hit points of a creature in the level state file (+0x6C). A fallen
// body that can get up keeps its regenerated hit points there as
// regen_hp - (max + 1), a value below 0, so that it loads dead.
fix dtu_creature_saved_shields(object *obj);
// The query radii of a moving object (0x748D9 for the object of the query,
// outputs edx and ebx): rad_h, rad_v of the body test 0x4F7D5.
void dtu_query_radii(object *obj, fix *rad_h, fix *rad_v);

#endif
