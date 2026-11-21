/*
 * ASCENT: new file. The footstep sounds of Descent to Undermountain.
 * Descent 1 has no footsteps. The player rule is the one of the function at
 * 0x433E0 of DTUGAME.EXE, the creature rule is the end of the creature move
 * 0x9F153 (0x9FCF1 to 0x9FDFB). See docs/re/music.md, "Footsteps".
 *
 * The player rule uses the game time, not the frame count: one step each
 * 0.625 seconds while the forward or the back key acts, and one each 0.3125
 * seconds in run mode. It does not depend on the frame rate. The rate is the
 * one of the original (a walk cadence of 1.6 steps a second, a run of 3.2).
 *
 * Two corrections, on by default, cleared by the option -origsound:
 * - Dtu_fix_foot_pan: the original puts the step of the player 0x200 to the
 *   left or right of the player in turn. The sound code takes the direction
 *   to the sound, not its distance, for the pan, so each step is panned hard
 *   to one side. The port plays the step in the middle at a fixed volume.
 * - Dtu_fix_creature_steps: the original plays a step for a walking creature
 *   on each game frame that shows the first frame of its walk or run cycle,
 *   so one cycle gives 1 to 10 steps by the frame rate. The port plays one
 *   step when the creature enters that frame.
 */
#include <stdlib.h>
#include <string.h>

#include "types.h"
#include "fix.h"
#include "vecmat.h"
#include "segment.h"
#include "object.h"
#include "game.h"
#include "args.h"
#include "controls.h"
#include "kconfig.h"
#include "physics.h"
#include "robot.h"
#include "bm.h"
#include "digi.h"
#include "dtuobj.h"
#include "dtufoot.h"

// 1: the player has footstep sounds, as in the original. 0: none.
int Dtu_footsteps = 1;

// See the top of the file. -origsound clears them.
int Dtu_fix_foot_pan = 1;
int Dtu_fix_creature_steps = 1;

#define FOOT_TIME_WALK		0xa000		// 0.625 seconds (0x43418)
#define FOOT_TIME_RUN		0x5000		// 0.3125 seconds (0x43594)
#define FOOT_VOLUME			(F1_0/2)		// 0x43561
#define FOOT_SIDE_STEP		0x200			// 0x13B7B4: the sound is left and right of the player in turn

// The sound numbers of the tables: four sounds for each kind of floor
// ($SOUND 2220 foot1ad.rca to $SOUND 2235 foot4dd.rca).
#define SOUND_FOOT_1			2220			// floor type 3
#define SOUND_FOOT_2			2224			// floor type 1, with a splash object in the original
#define SOUND_FOOT_3			2228			// floor type 2
#define SOUND_FOOT_4			2232			// floor type 0

static fix Next_step_time = 0;			// 0x13B7A7
static fix Side_step = FOOT_SIDE_STEP;

static void foot_args(void)
{
	static int done = 0;

	if (done)
		return;
	done = 1;
	if (FindArg("-origsound"))
		Dtu_fix_foot_pan = Dtu_fix_creature_steps = 0;
}

// The floor type is the one of the bottom side of the segment below the
// segment, if there is one (0x43468 and 0x9F236).
static int floor_type(int segnum)
{
	int below = Segments[segnum].children[WBOTTOM];

	if (below > -1)
		segnum = below;
	return Segments[segnum].sides[WBOTTOM].pad & 3;
}

static int floor_sound_base(int type)
{
	switch (type) {
		case 0:	return SOUND_FOOT_4;
		case 1:	return SOUND_FOOT_2;
		case 2:	return SOUND_FOOT_3;
		default:	return SOUND_FOOT_1;
	}
}

// Call it one time on each frame, after the objects have moved (the move
// of the player reads the controls).
void dtufoot_frame(void)
{
	object *obj = ConsoleObject;
	vms_vector pos;
	int soundnum;

	foot_args();
	if ( !Dtu_footsteps || !Dtu_player_walk || obj == NULL || obj->type != OBJ_PLAYER )
		return;

	// 0x437C9: no steps in flight, in levitation, in a jump, in a fall and
	// on a ladder. The original has one more condition that lets the steps
	// through (the flag at 0x145305, meaning not known). It is taken as 0.
	if ( (obj->dtu.m_88 & (DTU_OF_FLY|DTU_OF_LEVITATE)) || (Dtu_player_state & (DTU_PS_FALLING|DTU_PS_JUMPING|DTU_PS_CLIMBING)) )
		return;

	// ASCENT: the game time starts again at 0 with a new level. The
	// original does not reset the time of the next step.
	if ( Next_step_time > GameTime + FOOT_TIME_WALK )
		Next_step_time = 0;

	if ( GameTime <= Next_step_time )
		return;

	if ( Controls.forward_thrust_time != 0 ) {
		Next_step_time = GameTime + FOOT_TIME_WALK;

		// The original takes rand() % 4 first and a second rand() % 4
		// for floor types 0, 1 and 2.
		soundnum = rand() % 4;
		Side_step = -Side_step;
		pos = obj->pos;
		pos.x += Side_step;
		pos.z += Side_step;

		// For floor type 1 the original also creates a splash at the feet
		// of the player (dtuobj.c)
		if ( floor_type(obj->segnum) == 1 )
			dtu_water_splash( obj, Side_step );
		if ( floor_type(obj->segnum) != 3 )
			soundnum = rand() % 4;
		soundnum += floor_sound_base(floor_type(obj->segnum));

		if ( Dtu_fix_foot_pan )
			digi_play_sample( soundnum, FOOT_VOLUME );
		else
			digi_link_sound_to_pos( soundnum, obj->segnum, WBOTTOM, &pos, 0, FOOT_VOLUME );
	}

	if ( Dtu_move_mode == 2 )
		Next_step_time = GameTime + FOOT_TIME_RUN;
}

// The state of the last call for each object, for Dtu_fix_creature_steps.
static struct {
	int	signature;
	short	state, frame;
} Creature_last[MAX_OBJECTS];

// 0x9FCF1 to 0x9FDFB: the step of a creature. Call it at the end of the
// move of an active creature (activity 2) on each frame.
void dtufoot_creature(object *obj)
{
	int objnum = obj-Objects;
	int s = DTU_ANIM_STATE(obj), frame = DTU_ANIM_FRAME(obj), entered, soundnum;
	monster_info *mi;
	char value[16];

	foot_args();
	if (objnum < 0 || objnum >= MAX_OBJECTS || obj->id >= N_robot_types)
		return;
	mi = &Monster_info[obj->id];

	entered = Creature_last[objnum].signature != obj->signature || Creature_last[objnum].state != s ||
		Creature_last[objnum].frame != frame;
	Creature_last[objnum].signature = obj->signature;
	Creature_last[objnum].state = s;
	Creature_last[objnum].frame = frame;

	if (!Dtu_footsteps)
		return;
	if ((s != DAS_WALK && s != DAS_RUN) || frame != Dtu_anim_first[s])
		return;
	if (Dtu_fix_creature_steps && !entered)
		return;
	// Not for a creature that flies (flag flying, or the object flag
	// 0x800), and not for the monster type with "rat=" (0x13A820).
	if ((mi->flags & MONSTER_F_FLYING) || (obj->dtu.m_88 & DTU_OF_FLY))
		return;
	if (mi->record >= 0 && bm_tbl_get(mi->record,"rat",value,sizeof(value)))
		return;

	// The original takes (clock + objnum) % 4 with the clock of the AI
	// (player struct 0x1D339F). The port takes rand() % 4.
	soundnum = floor_sound_base(floor_type(obj->segnum)) + rand() % 4;
	// The volume is str * 1/64 (str << 10); 0x2E900 makes it 0.5 at most.
	digi_link_sound_to_pos2(soundnum,obj->segnum,WBOTTOM,&obj->pos,0,
		mi->str << 10 > F1_0/2 ? F1_0/2 : mi->str << 10,i2f(256));
	dtu_obj_log("foot: obj %d %s step, sound %d, frame %d, state %d",objnum,dtu_object_name(obj),soundnum,frame,s);
}
