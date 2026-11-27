/*
 * ASCENT: new file, split from dtuobj.c. The test aids that place the
 * player and the objects: -testpos, -testobjpos and -testtour. The
 * shared declarations are in dtuobj_int.h.
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

static void test_place_objects(void);

// The option -testpos X,Y,Z[,HEADING[,PITCH]]: puts the player at a position
// of the level at the first frame, for tests and screenshots. The heading is
// in degrees, 0 looks along +z, 90 along +x. The pitch is the look pitch in
// degrees, positive down, limited as the look keys limit it (about -67 to
// 67).
void test_place_player(void)
{
	int t = FindArg("-testpos");
	float x, y, z, h = 0, p = 0;
	vms_vector pos;
	vms_angvec a;
	int seg, n;

	Test_pos_pending = 0;
	test_place_objects();
	if (!t || t+1 >= Num_args)
		return;
	if ((n = sscanf(Args[t+1],"%f,%f,%f,%f,%f",&x,&y,&z,&h,&p)) < 3)
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
	if (n >= 5) {
		int lp = (int) (p * 65536.0 / 360.0);
		if (lp < DTU_LOOK_MIN) lp = DTU_LOOK_MIN;
		if (lp > DTU_LOOK_MAX) lp = DTU_LOOK_MAX;
		Dtu_look_pitch = (fixang) lp;
	}
	// the new segment is not a move into it: no warp or floor trigger
	dtu_trig_player_placed();
}

// The option -testobjpos N,X,Y,Z[,HEADING][;N,X,Y,Z[,HEADING]...]: puts the
// objects N at these positions at the first frame (the test of pushed
// scenery at a wall).
static void test_place_objects(void)
{
	int t = FindArg("-testobjpos");
	const char *s;

	if (!t || t+1 >= Num_args)
		return;
	for (s = Args[t+1]; s && *s; s = strchr(s,';') ? strchr(s,';')+1 : NULL) {
		int n, seg;
		float x, y, z, h = 0;
		vms_vector pos;
		vms_angvec a;

		if (sscanf(s,"%d,%f,%f,%f,%f",&n,&x,&y,&z,&h) < 4 || n < 0 || n > Highest_object_index || Objects[n].type == OBJ_NONE)
			continue;
		pos.x = fl2f(x); pos.y = fl2f(y); pos.z = fl2f(z);
		seg = find_point_seg(&pos,-1);
		if (seg < 0) {
			printf("-testobjpos: the point of object %d is in no segment\n",n);
			continue;
		}
		Objects[n].pos = pos;
		obj_relink(n,seg);
		a.p = a.b = 0;
		a.h = (fixang)(int)(h * 65536.0 / 360.0);
		vm_angles_2_matrix(&Objects[n].orient,&a);
		dtu_obj_log("testobjpos: obj %d %s at %.2f %.2f %.2f seg %d heading %.0f",n,dtu_object_name(&Objects[n]),x,y,z,seg,h);
	}
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
		// the jump from segment to segment is not a fall (the damage of a
		// fall from the center of a high segment killed the player)
		Dtu_player_state &= ~DTU_PS_FALLING;
		dtu_trig_player_placed();
		if (++step / 4 > Highest_segment_index)
			printf("-testtour: %d segments visited\n", seg + 1);
	}
}
