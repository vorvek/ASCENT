/*
 * ASCENT: new file. The footstep sounds of the player in Descent to
 * Undermountain. Descent 1 has no footsteps. The rule is the one of the
 * function at 0x433E0 of DTUGAME.EXE (see docs/re/music.md, "Footsteps").
 *
 * The rule uses the game time, not the frame count: one step each 0.625
 * seconds while the forward or the back key acts, and one each 0.3125
 * seconds in run mode. It does not depend on the frame rate, so there is
 * no corrected rule and no Dtu_fix setting for it.
 */
#include <stdlib.h>

#include "types.h"
#include "fix.h"
#include "vecmat.h"
#include "segment.h"
#include "object.h"
#include "game.h"
#include "controls.h"
#include "kconfig.h"
#include "physics.h"
#include "digi.h"
#include "dtufoot.h"

// 1: the player has footstep sounds, as in the original. 0: none.
int Dtu_footsteps = 1;

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

// Call it one time on each frame, after the objects have moved (the move
// of the player reads the controls).
void dtufoot_frame(void)
{
	object *obj = ConsoleObject;
	vms_vector pos;
	int segnum, below, floor_type, soundnum;

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

		pos = obj->pos;
		segnum = obj->segnum;
		soundnum = SOUND_FOOT_1 + rand() % 4;
		Side_step = -Side_step;
		pos.x += Side_step;
		pos.z += Side_step;

		// The floor type is the one of the segment below if there is one.
		below = Segments[segnum].children[WBOTTOM];
		if ( below > -1 )
			segnum = below;
		floor_type = Segments[segnum].sides[WBOTTOM].pad & 3;

		// For floor type 1 the original also creates an object of type 10
		// at the feet of the player (guess: a splash). The port does not.
		if ( floor_type == 1 )
			soundnum = SOUND_FOOT_2 + rand() % 4;
		if ( floor_type == 0 )
			soundnum = SOUND_FOOT_4 + rand() % 4;
		if ( floor_type == 2 )
			soundnum = SOUND_FOOT_3 + rand() % 4;

		digi_link_sound_to_pos( soundnum, obj->segnum, WBOTTOM, &pos, 0, FOOT_VOLUME );
	}

	if ( Dtu_move_mode == 2 )
		Next_step_time = GameTime + FOOT_TIME_RUN;
}
