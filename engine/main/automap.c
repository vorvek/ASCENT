/*
THE COMPUTER CODE CONTAINED HEREIN IS THE SOLE PROPERTY OF PARALLAX
SOFTWARE CORPORATION ("PARALLAX").  PARALLAX, IN DISTRIBUTING THE CODE TO
END-USERS, AND SUBJECT TO ALL OF THE TERMS AND CONDITIONS HEREIN, GRANTS A
ROYALTY-FREE, PERPETUAL LICENSE TO SUCH END-USERS FOR USE BY SUCH END-USERS
IN USING, DISPLAYING,  AND CREATING DERIVATIVE WORKS THEREOF, SO LONG AS
SUCH USE, DISPLAY OR CREATION IS FOR NON-COMMERCIAL, ROYALTY OR REVENUE
FREE PURPOSES.  IN NO EVENT SHALL THE END-USER USE THE COMPUTER CODE
CONTAINED HEREIN FOR REVENUE-BEARING PURPOSES.  THE END-USER UNDERSTANDS
AND AGREES TO THE TERMS HEREIN AND ACCEPTS THE SAME BY USE OF THIS FILE.  
COPYRIGHT 1993-1998 PARALLAX SOFTWARE CORPORATION.  ALL RIGHTS RESERVED.
*/
/*
 * $Source: f:/miner/source/main/rcs/automap.c $
 * $Revision: 2.2 $
 * $Author: john $
 * $Date: 1995/03/21 14:41:26 $
 * 
 * Routines for displaying the auto-map.
 * 
 * $Log: automap.c $
 * Revision 2.2  1995/03/21  14:41:26  john
 * Ifdef'd out the NETWORK code.
 * 
 * Revision 2.1  1995/03/20  18:16:06  john
 * Added code to not store the normals in the segment structure.
 * 
 * Revision 2.0  1995/02/27  11:32:55  john
 * New version 2.0, which has no anonymous unions, builds with
 * Watcom 10.0, and doesn't require parsing BITMAPS.TBL.
 * 
 * Revision 1.117  1995/02/22  14:11:31  allender
 * remove anonymous unions from object structure
 * 
 * Revision 1.116  1995/02/22  13:24:39  john
 * Removed the vecmat anonymous unions.
 * 
 * Revision 1.115  1995/02/09  14:57:02  john
 * Reduced mem usage. Made automap slide farther.
 * 
 * Revision 1.114  1995/02/07  20:40:44  rob
 * Allow for anarchy automap of player pos by option.
 * 
 * Revision 1.113  1995/02/07  15:45:33  john
 * Made automap memory be static.
 * 
 * Revision 1.112  1995/02/02  12:24:00  adam
 * played with automap labels
 * 
 * Revision 1.111  1995/02/02  01:52:52  john
 * Made the automap use small font.
 * 
 * Revision 1.110  1995/02/02  01:34:34  john
 * Made Reset in automap not change segmentlimit.
 * 
 * Revision 1.109  1995/02/02  01:23:11  john
 * Finalized the new automap partial viewer.
 * 
 * Revision 1.108  1995/02/02  00:49:45  mike
 * new automap segment-depth functionality.
 * 
 * Revision 1.107  1995/02/02  00:23:04  john
 * Half of the code for new connected distance stuff in automap.
 * 
 * Revision 1.106  1995/02/01  22:54:00  john
 * Made colored doors not fade in automap. Made default
 * viewing area be maxxed.
 * 
 * Revision 1.105  1995/02/01  13:16:13  john
 * Added great grates. 
 * 
 * Revision 1.104  1995/01/31  12:47:06  john
 * Made Alt+F only work with cheats enabled.
 * 
 * Revision 1.103  1995/01/31  12:41:23  john
 * Working with new controls.
 * 
 * Revision 1.102  1995/01/31  12:04:19  john
 * Version 2 of new key control.
 * 
 * Revision 1.101  1995/01/31  11:32:00  john
 * First version of new automap system.
 * 
 * Revision 1.100  1995/01/28  16:55:48  john
 * Made keys draw in automap in the segments that you have
 * visited.
 * 
 * Revision 1.99  1995/01/28  14:44:51  john
 * Made hostage doors show up on automap.
 * 
 * Revision 1.98  1995/01/22  17:03:49  rob
 * Fixed problem drawing playerships in automap coop/team mode
 * 
 * Revision 1.97  1995/01/21  17:23:11  john
 * Limited S movement in map. Made map bitmap load from disk
 * and then freed it.
 * 
 * Revision 1.96  1995/01/19  18:55:38  john
 * Don't draw players in automap if not obj_player.
 * 
 * Revision 1.95  1995/01/19  18:48:13  john
 * Made player colors better in automap.
 * 
 * Revision 1.94  1995/01/19  17:34:52  rob
 * Added team colorizations in automap.
 * 
 * Revision 1.93  1995/01/19  17:15:36  rob
 * Trying to add player ships into map for coop and team mode.
 * 
 * Revision 1.92  1995/01/19  17:11:09  john
 * Added code for Rob to draw Multiplayer ships in automap.
 * 
 * Revision 1.91  1995/01/12  13:35:20  john
 * Fixed bug with Segment 0 not getting displayed 
 * in automap if you have EDITOR compiled in.
 * 
 * Revision 1.90  1995/01/08  16:17:14  john
 * Added code to draw player's up vector while in automap.
 * 
 * Revision 1.89  1995/01/08  16:09:41  john
 * Fixed problems with grate.
 * 
 * Revision 1.88  1994/12/14  22:54:17  john
 * Fixed bug that didn't show hostages in automap.
 * 
 * Revision 1.87  1994/12/09  00:41:03  mike
 * fix hang in automap print screen
 * 
 * Revision 1.86  1994/12/05  23:37:15  matt
 * Took out calls to warning() function
 * 
 * Revision 1.85  1994/12/03  22:35:28  yuan
 * Localization 412
 * 
 * Revision 1.84  1994/12/02  15:05:45  matt
 * Added new "official" cheats
 * 
 * Revision 1.83  1994/11/30  12:10:49  adam
 * added support for PCX titles/brief screens
 * 
 * Revision 1.82  1994/11/27  23:15:12  matt
 * Made changes for new mprintf calling convention
 * 
 * Revision 1.81  1994/11/27  15:35:52  matt
 * Enable screen shots even when debugging is turned off
 * 
 * Revision 1.80  1994/11/26  22:51:43  matt
 * Removed editor-only fields from segment structure when editor is compiled
 * out, and padded segment structure to even multiple of 4 bytes.
 * 
 * Revision 1.79  1994/11/26  16:22:48  matt
 * Reduced leave_time
 * 
 * Revision 1.78  1994/11/23  22:00:10  mike
 * show level number.
 * 
 * Revision 1.77  1994/11/21  11:40:33  rob
 * Tweaked the game-loop for automap in multiplayer games.
 * 
 * Revision 1.76  1994/11/18  16:42:06  adam
 * removed a font
 * 
 * Revision 1.75  1994/11/17  13:06:48  adam
 * changed font
 * 
 * Revision 1.74  1994/11/14  20:47:17  john
 * Attempted to strip out all the code in the game 
 * directory that uses any ui code.
 * 
 */


#pragma off (unreferenced)
static char rcsid[] = "$Id: automap.c 2.2 1995/03/21 14:41:26 john Exp $";
#pragma on (unreferenced)

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "error.h"
#include "3d.h"
#include "inferno.h"
#include "mem.h"
#include "render.h"
#include "object.h"
#include "vclip.h"
#include "game.h"
#include "mono.h"
#include "polyobj.h"
#include "sounds.h"
#include "player.h"
#include "bm.h"
#include "key.h"
#include "screens.h"
#include "textures.h"
#include "mouse.h"
#include "timer.h"
#include "segpoint.h"
#include "joy.h"
#include "iff.h"
#include "pcx.h"
#include "palette.h"
#include "wall.h"
#include "hostage.h"
#include "fuelcen.h"
#include "gameseq.h"
#include "gamefont.h"
#include "network.h"
#include "kconfig.h"
#include "multi.h"
#include "endlevel.h"
#include "text.h"
#include "gauges.h"
#include "powerup.h"
#include "network.h" 
#include "platform.h"		// ASCENT: plat_pump reports that the window is closed
#include "args.h"			// ASCENT: FindArg for -origui

#define EF_USED			1		// This edge is used
#define EF_DEFINING		2		// A structure defining edge that should always draw.
#define EF_FRONTIER		4		// An edge between the known and the unknown.
#define EF_SECRET			8		// An edge that is part of a secret wall.
#define EF_GRATE			16		// A grate... draw it all the time.
#define EF_NO_FADE		32		// An edge that doesn't fade with distance
#define EF_TOO_FAR		64		// An edge that is too far away
#define EF_FLICKER		128	// ASCENT: the target game: the brightness of the edge changes at random (0x192ED)

typedef struct Edge_info {
	union {
		short	verts[2];		// 4 bytes
		int vv;					// ASCENT: was long. It must have the size of verts (4 bytes); long has 8 bytes on some 64-bit systems.
	};
	ubyte sides[4];			// 4 bytes
	short segnum[4];			// 8 bytes	// This might not need to be stored... If you can access the normals of a side.
	ubyte flags;				// 1 bytes 	// See the EF_??? defines above.
	ubyte color;				// 1 bytes
	ubyte num_faces;			// 1 bytes	// 19 bytes...
} Edge_info;

//OLD BUT GOOD -- #define MAX_EDGES_FROM_VERTS(v)   ((v*5)/2)
// THE following was determined by John by loading levels 1-14 and recording
// numbers on 10/26/94. 
//#define MAX_EDGES_FROM_VERTS(v)   (((v)*21)/10)
#define MAX_EDGES_FROM_VERTS(v)		((v)*4)
//#define MAX_EDGES (MAX_EDGES_FROM_VERTS(MAX_VERTICES))

#define MAX_EDGES 6000		// Determined by loading all the levels by John & Mike, Feb 9, 1995

#define	WALL_NORMAL_COLOR 				BM_XRGB( 29, 29, 29 )
#define	WALL_DOOR_COLOR					BM_XRGB( 21, 31, 11 )
#define	WALL_DOOR_BLUE						BM_XRGB( 0, 0, 31)
#define	WALL_DOOR_GOLD						BM_XRGB( 31, 31, 0)
#define	WALL_DOOR_RED						BM_XRGB( 31, 0, 0)

// Segment visited list
ubyte Automap_visited[MAX_SEGMENTS];

// Edge list variables
static int Num_edges=0;
static int Max_edges;		//set each frame
static int Highest_edge_index = -1;
static Edge_info Edges[MAX_EDGES];
static short DrawingListBright[MAX_EDGES];

//static short DrawingListBright[MAX_EDGES];
//static short Edge_used_list[MAX_EDGES];				//which entries in edge_list have been used

// Map movement defines
#define PITCH_DEFAULT 9000
#define ZOOM_DEFAULT i2f(20*10)
#define ZOOM_MIN_VALUE i2f(20*5)
#define ZOOM_MAX_VALUE i2f(20*100)

#define SLIDE_SPEED 				(350)
#define ZOOM_SPEED_FACTOR		(1500)
#define ROT_SPEED_DIVISOR		(115000)

// Screen anvas variables
static int current_page=0;
static grs_canvas Pages[2];
static grs_canvas DrawingPages[2];

// Flags
static int Automap_cheat = 0;		// If set, show everything

// Rendering variables
static fix Automap_zoom = 0x9000;
static vms_vector view_target;
static fix Automap_farthest_dist = (F1_0 * 20 * 50);		// 50 segments away
static vms_matrix	ViewMatrix;
static fix ViewDist=ZOOM_DEFAULT;			// ASCENT: 0x13A804 starts with 200.0

// ASCENT: were locals of do_automap. The map of the target game keeps
// them between frames (0x13A808, 0x13A816) and draw_all_edges reads the
// first one.
static int Max_segments_away = 1;
static int SegmentLimit = 1;
static int Edge_flicker = 0;				// 0x13A80C: add_one_edge sets EF_FLICKER

// ASCENT: prototypes for functions that this file calls before their
// definition. Watcom C accepted the implicit declarations.
void draw_all_edges();
void automap_build_edge_list();
void adjust_segment_limit(int SegmentLimit);

// ASCENT: min is a macro of stdlib.h in Watcom C only.
#ifndef min
#define min(a,b) (((a)<(b))?(a):(b))
#endif

void automap_clear_visited()	
{
	int i;
	for (i=0; i<MAX_SEGMENTS; i++ )
		Automap_visited[i] = 0;
}

grs_canvas *name_canv;

// ASCENT: the player of the target game (0x18567): a triangle without the
// sphere, the shaft and the up line. The tip is 3 sizes forward, the base
// corners are 2/3 size forward and 2/3 size to each side (the factors are
// the doubles 0.6667 and -1.0 at 0x120A90, truncated to an integer).
static void draw_player_dtu( object * obj )
{
	vms_vector arrow_pos, head_pos;
	g3s_point arrow_point, head_point, head_point2;
	fix s = (fix) (obj->size * (2.0 / 3.0));

	vm_vec_scale_add( &arrow_pos, &obj->pos, &obj->orient.fvec, obj->size*3 );
	g3_rotate_point(&arrow_point,&arrow_pos);

	vm_vec_scale_add( &head_pos, &obj->pos, &obj->orient.fvec, s );
	vm_vec_scale_add2( &head_pos, &obj->orient.rvec, s );
	g3_rotate_point(&head_point,&head_pos);
	g3_draw_line( &head_point, &arrow_point );

	vm_vec_scale_add( &head_pos, &obj->pos, &obj->orient.fvec, s );
	vm_vec_scale_add2( &head_pos, &obj->orient.rvec, (fix) (obj->size * (2.0 / 3.0) * -1.0) );
	g3_rotate_point(&head_point2,&head_pos);
	g3_draw_line( &head_point2, &arrow_point );
	g3_draw_line( &head_point2, &head_point );
}

void draw_player( object * obj )
{
	vms_vector arrow_pos, head_pos;
	g3s_point sphere_point, arrow_point, head_point;

	// Draw Console player -- shaped like a ellipse with an arrow.
	g3_rotate_point(&sphere_point,&obj->pos);
	g3_draw_sphere(&sphere_point,obj->size);

	// Draw shaft of arrow
	vm_vec_scale_add( &arrow_pos, &obj->pos, &obj->orient.fvec, obj->size*3 );
	g3_rotate_point(&arrow_point,&arrow_pos);
	g3_draw_line( &sphere_point, &arrow_point );

	// Draw right head of arrow
	vm_vec_scale_add( &head_pos, &obj->pos, &obj->orient.fvec, obj->size*2 );
	vm_vec_scale_add2( &head_pos, &obj->orient.rvec, obj->size*1 );
	g3_rotate_point(&head_point,&head_pos);
	g3_draw_line( &arrow_point, &head_point );

	// Draw left head of arrow
	vm_vec_scale_add( &head_pos, &obj->pos, &obj->orient.fvec, obj->size*2 );
	vm_vec_scale_add2( &head_pos, &obj->orient.rvec, obj->size*(-1) );
	g3_rotate_point(&head_point,&head_pos);
	g3_draw_line( &arrow_point, &head_point );

	// Draw player's up vector
	vm_vec_scale_add( &arrow_pos, &obj->pos, &obj->orient.uvec, obj->size*2 );
	g3_rotate_point(&arrow_point,&arrow_pos);
	g3_draw_line( &sphere_point, &arrow_point );
}


void draw_automap()
{
	int i;
	int color;
	object * objp;
	vms_vector viewer_position;
	g3s_point sphere_point;

	current_page ^= 1;
	gr_set_current_canvas(&DrawingPages[current_page]);

	gr_clear_canvas(0);

	g3_start_frame();
	render_start_frame();
	
	vm_vec_scale_add(&viewer_position,&view_target,&ViewMatrix.fvec,-ViewDist );

	g3_set_view_matrix(&viewer_position,&ViewMatrix,Automap_zoom);

	draw_all_edges();

	// Draw player...
#ifdef NETWORK
	if (Game_mode & GM_TEAM)
		color = get_team(Player_num);
	else
#endif	
		color = Player_num;	// Note link to above if!
	gr_setcolor(gr_getcolor(player_rgb[color].r,player_rgb[color].g,player_rgb[color].b));
	draw_player(&Objects[Players[Player_num].objnum]);
				
	// Draw player(s)...
#ifdef NETWORK
	if ( (Game_mode & (GM_TEAM | GM_MULTI_COOP)) || (Netgame.game_flags & NETGAME_FLAG_SHOW_MAP) )	{
		for (i=0; i<N_players; i++)		{
			if ( (i != Player_num) && ((Game_mode & GM_MULTI_COOP) || (get_team(Player_num) == get_team(i)) || (Netgame.game_flags & NETGAME_FLAG_SHOW_MAP)) )	{
				if ( Objects[Players[i].objnum].type == OBJ_PLAYER )	{
					if (Game_mode & GM_TEAM)
						color = get_team(i);
					else
						color = i;
					gr_setcolor(gr_getcolor(player_rgb[color].r,player_rgb[color].g,player_rgb[color].b));
					draw_player(&Objects[Players[i].objnum]);
				}
			}
		}
	}
#endif

	objp = &Objects[0];
	for (i=0;i<=Highest_object_index;i++,objp++) {
		switch( objp->type )	{
		case OBJ_HOSTAGE:
			gr_setcolor(BM_XRGB(0,31,0));
			g3_rotate_point(&sphere_point,&objp->pos);
			g3_draw_sphere(&sphere_point,objp->size);	
			break;
		case OBJ_POWERUP:
			if ( Automap_visited[objp->segnum] )	{
				if ( (objp->id==POW_KEY_RED) || (objp->id==POW_KEY_BLUE) || (objp->id==POW_KEY_GOLD) )	{
					switch (objp->id) {
					case POW_KEY_RED:		gr_setcolor(gr_getcolor(63, 5, 5));	break;
					case POW_KEY_BLUE:	gr_setcolor(gr_getcolor(5, 5, 63)); break;
					case POW_KEY_GOLD:	gr_setcolor(gr_getcolor(63, 63, 10)); break;
					}
					g3_rotate_point(&sphere_point,&objp->pos);
					g3_draw_sphere(&sphere_point,objp->size*4);	
				}
			}
			break;
		}
	}

	g3_end_frame();

	gr_bitmapm(5,5,&name_canv->cv_bitmap);

	gr_show_canvas( &Pages[current_page] );
	
}

#define LEAVE_TIME 0x4000

//print to canvas & double height
grs_canvas *print_to_canvas(char *s,grs_font *font, int fc, int bc)
{
	int y;
	ubyte *data;
	int rs;
	grs_canvas *temp_canv,*save_canv;

	save_canv = grd_curcanv;

	temp_canv = gr_create_canvas(font->ft_w*strlen(s),font->ft_h*2);

	gr_set_current_canvas(temp_canv);
	gr_set_curfont(font);
	gr_clear_canvas(255);						//trans color
	gr_set_fontcolor(fc,bc);
	gr_printf(0,0,s);

	//now double it, since we're drawing to 400-line modex screen

	data = temp_canv->cv_bitmap.bm_data;
	rs = temp_canv->cv_bitmap.bm_rowsize;

	for (y=temp_canv->cv_bitmap.bm_h/2;y--;) {
		memcpy(data+(rs*y*2),data+(rs*y),temp_canv->cv_bitmap.bm_w);
		memcpy(data+(rs*(y*2+1)),data+(rs*y),temp_canv->cv_bitmap.bm_w);
	}

	gr_set_current_canvas(save_canv);

	return temp_canv;
}

//print to buffer, double heights, and blit bitmap to screen
modex_printf(int x,int y,char *s,int fontnum)
{
	grs_canvas *temp_canv;

	temp_canv = print_to_canvas(s,Gamefonts[fontnum], BM_XRGB(20,20,20), -1);

	gr_bitmapm(x,y,&temp_canv->cv_bitmap);

	gr_free_canvas(temp_canv);
}

create_name_canv()
{
	char	name_level[128];

	if (Current_level_num > 0)
		sprintf(name_level, "%s %i: ",TXT_LEVEL, Current_level_num);
	else
		name_level[0] = 0;

	strcat(name_level, Current_level_name);

	gr_set_fontcolor(BM_XRGB(0,31,0),-1);
	name_canv = print_to_canvas(name_level,Gamefonts[GFONT_SMALL], BM_XRGB(0,31,0), -1);

}

void modex_print_message(int x, int y, char *str)
{
	int	i;

	for (i=0; i<2; i++ )	{
		gr_set_current_canvas(&Pages[i]);
		modex_printf(x, y, str, GFONT_MEDIUM_1);
	}

	gr_set_current_canvas(&DrawingPages[current_page]);
}

extern void GameLoop(int, int );
extern int set_segment_depths(int start_seg, ubyte *segbuf);

void do_automap( int key_code )	{
	int done=0;
	vms_matrix	tempm;
	vms_angvec	tangles;
	int leave_mode=0;
	int first_time=1;
	int pcx_error;
	int i;
	int c;
	char filename[] = "MAP.PCX";
	fix entry_time;
	int pause_game=1;		// Set to 1 if everything is paused during automap...No pause during net.
	fix t1, t2;
	control_info saved_control_info;
	grs_bitmap Automap_background;

	key_code = key_code;	// disable warning...

	if ((Game_mode & GM_MULTI) && (Function_mode == FMODE_GAME) && (!Endlevel_sequence))
		pause_game = 0;

	if (pause_game)
		stop_time();

	create_name_canv();

	Max_edges = min(MAX_EDGES_FROM_VERTS(Num_vertices),MAX_EDGES);			//make maybe smaller than max
	//Edges	= malloc( sizeof(Edge_info)*Max_edges);
	//if ( Edges == NULL )	{
	//	mprintf((0, "Couldn't get %dK for automap!", sizeof(Edge_info)*Max_edges/1024));
	//	return;
	//}
	//DrawingListBright = malloc( sizeof(short)*Max_edges);
	//if ( DrawingListBright == NULL )	{
	//	mprintf((0, "Couldn't get %dK for automap!", sizeof(short)*Max_edges/1024));
	//	return;
	//}

	mprintf( (0, "Num_vertices=%d, Max_edges=%d, (MAX:%d)\n", Num_vertices, Max_edges, MAX_EDGES ));
	mprintf( (0, "Allocated %d K for automap edge list\n", (sizeof(Edge_info)+sizeof(short))*Max_edges/1024 ));

	gr_set_mode( SM_320x400U );
	gr_palette_clear();

	gr_init_sub_canvas(&Pages[0],grd_curcanv,0,0,320,400);
	gr_init_sub_canvas(&Pages[1],grd_curcanv,0,401,320,400);
	gr_init_sub_canvas(&DrawingPages[0],&Pages[0],16,69,288,272);
	gr_init_sub_canvas(&DrawingPages[1],&Pages[1],16,69,288,272);

	Automap_background.bm_data = NULL;
	pcx_error = pcx_read_bitmap(filename,&Automap_background,BM_LINEAR,NULL);
	if ( pcx_error != PCX_ERROR_NONE )	{
		printf("File %s - PCX error: %s",filename,pcx_errormsg(pcx_error));
		Error("File %s - PCX error: %s",filename,pcx_errormsg(pcx_error));
		return;
	}

	for (i=0; i<2; i++ )	{
		gr_set_current_canvas(&Pages[i]);
		gr_bitmap( 0, 0, &Automap_background );
		modex_printf( 40, 22,TXT_AUTOMAP,GFONT_BIG_1);
		modex_printf( 70,353,TXT_TURN_SHIP,GFONT_SMALL);
		modex_printf( 70,369,TXT_SLIDE_UPDOWN,GFONT_SMALL);
		modex_printf( 70,385,TXT_VIEWING_DISTANCE,GFONT_SMALL);
	}
	if ( Automap_background.bm_data )
		free( Automap_background.bm_data );
	Automap_background.bm_data = NULL;

	gr_set_current_canvas(&DrawingPages[current_page]);

	automap_build_edge_list();

	if ( ViewDist==0 ) 
		ViewDist = ZOOM_DEFAULT;
	ViewMatrix = Objects[Players[Player_num].objnum].orient;

	tangles.p = PITCH_DEFAULT;
	tangles.h  = 0;
	tangles.b  = 0;

	done = 0;

	view_target = Objects[Players[Player_num].objnum].pos;

	t1 = entry_time = timer_get_fixed_seconds();
	t2 = t1;

	//Fill in Automap_visited from Objects[Players[Player_num].objnum].segnum
	Max_segments_away = set_segment_depths(Objects[Players[Player_num].objnum].segnum, Automap_visited);
	SegmentLimit = Max_segments_away;

	adjust_segment_limit(SegmentLimit);

	while(!done)	{
		if ( leave_mode==0 && Controls.automap_state && (timer_get_fixed_seconds()-entry_time)>LEAVE_TIME)
			leave_mode = 1;

		if ( !Controls.automap_state && (leave_mode==1) )
			done=1;

		if (!pause_game)	{
			ushort old_wiggle;
			saved_control_info = Controls;				// Save controls so we can zero them
			memset(&Controls,0,sizeof(control_info));	// Clear everything...
			old_wiggle = ConsoleObject->mtype.phys_info.flags & PF_WIGGLE;	// Save old wiggle
			ConsoleObject->mtype.phys_info.flags &= ~PF_WIGGLE;		// Turn off wiggle
			#ifdef NETWORK
			if (multi_menu_poll())
				done = 1;
			#endif
//			GameLoop( 0, 0 );		// Do game loop with no rendering and no reading controls.
			ConsoleObject->mtype.phys_info.flags |= old_wiggle;	// Restore wiggle
			Controls = saved_control_info;
		} 

		controls_read_all();		
		if ( Controls.automap_down_count )	{
			if (leave_mode==0)
				done = 1;
			c = 0;
		}

		while( (c=key_inkey()) )	{
			switch( c ) {
			#ifndef NDEBUG
			case KEY_BACKSP: Int3(); break;
			#endif
	
			case KEY_PRINT_SCREEN: save_screen_shot(1); break;
	
			case KEY_ESC:
				if (leave_mode==0)
					done = 1;
				 break;
			case KEY_ALTED+KEY_F:		// Alt+F shows full map, if cheats enabled
				if (Cheats_enabled) 
				{
					uint t;	
					t = Players[Player_num].flags;
					Players[Player_num].flags |= PLAYER_FLAGS_MAP_ALL_CHEAT;
					automap_build_edge_list();
					Players[Player_num].flags=t;
			 	}
				break;
	
	#ifndef NDEBUG
		  	case KEY_DEBUGGED+KEY_F: 	{
				for (i=0; i<=Highest_segment_index; i++ )
					Automap_visited[i] = 1;
				automap_build_edge_list();
				Max_segments_away = set_segment_depths(Objects[Players[Player_num].objnum].segnum, Automap_visited);
				SegmentLimit = Max_segments_away;
				adjust_segment_limit(SegmentLimit);
				}
				break;
	#endif
			case KEY_MINUS:
				if (SegmentLimit > 1) 		{
					SegmentLimit--;
					adjust_segment_limit(SegmentLimit);
				}
				break;
			case KEY_EQUAL:
				if (SegmentLimit < Max_segments_away) 	{
					SegmentLimit++;
					adjust_segment_limit(SegmentLimit);
				}
				break;
			}
		}

		if ( Controls.fire_primary_down_count )	{
			// Reset orientation
			ViewDist = ZOOM_DEFAULT;
			tangles.p = PITCH_DEFAULT;
			tangles.h  = 0;
			tangles.b  = 0;
			view_target = Objects[Players[Player_num].objnum].pos;
		}

		ViewDist -= Controls.forward_thrust_time*ZOOM_SPEED_FACTOR;

		tangles.p += fixdiv( Controls.pitch_time, ROT_SPEED_DIVISOR );
		tangles.h  += fixdiv( Controls.heading_time, ROT_SPEED_DIVISOR );
		tangles.b  += fixdiv( Controls.bank_time, ROT_SPEED_DIVISOR*2 );
		
		if ( Controls.vertical_thrust_time || Controls.sideways_thrust_time )	{
			vms_angvec	tangles1;
			vms_vector	old_vt;
			old_vt = view_target;
			tangles1 = tangles;
			vm_angles_2_matrix(&tempm,&tangles1);
			vm_matrix_x_matrix(&ViewMatrix,&Objects[Players[Player_num].objnum].orient,&tempm);
			vm_vec_scale_add2( &view_target, &ViewMatrix.uvec, Controls.vertical_thrust_time*SLIDE_SPEED );
			vm_vec_scale_add2( &view_target, &ViewMatrix.rvec, Controls.sideways_thrust_time*SLIDE_SPEED );
			if ( vm_vec_dist_quick( &view_target, &Objects[Players[Player_num].objnum].pos) > i2f(1000) )	{
				view_target = old_vt;
			}
		} 

		vm_angles_2_matrix(&tempm,&tangles);
		vm_matrix_x_matrix(&ViewMatrix,&Objects[Players[Player_num].objnum].orient,&tempm);

		if ( ViewDist < ZOOM_MIN_VALUE ) ViewDist = ZOOM_MIN_VALUE;
		if ( ViewDist > ZOOM_MAX_VALUE ) ViewDist = ZOOM_MAX_VALUE;

		draw_automap();

		if ( first_time )	{
			first_time = 0;
			gr_palette_load( gr_palette );
		}

		// ASCENT: draw_automap shows the frame (gr_show_canvas). The page
		// flip of the VGA waited for the vertical retrace (70 Hz in this
		// mode). This wait keeps the loop at 70 frames in each second or
		// less, so that it does not use the full processor speed.
		{
			fix left = F1_0/70 - (timer_get_fixed_seconds()-t1);
			if ( (left > 0) && (left <= F1_0/70) )
				timer_delay(left);
		}
		// ASCENT: leave the automap if the user closed the window.
		if ( !plat_pump() )
			done = 1;

		t2 = timer_get_fixed_seconds();
		if (pause_game)
			FrameTime=t2-t1;
		t1 = t2;
	}

	//free(Edges);
	//free(DrawingListBright);
	gr_free_canvas(name_canv);  name_canv=NULL;

	mprintf( (0, "Automap memory freed\n" ));

	game_flush_inputs();

	if (pause_game)
		start_time();
}

void adjust_segment_limit(int SegmentLimit)
{
	int i,e1;
	Edge_info * e;

	mprintf(( 0, "Seglimit: %d\n", SegmentLimit ));
	
	for (i=0; i<=Highest_edge_index; i++ )	{
		e = &Edges[i];
		e->flags |= EF_TOO_FAR;
		for (e1=0; e1<e->num_faces; e1++ )	{
			if ( Automap_visited[e->segnum[e1]] <= SegmentLimit )	{
				e->flags &= (~EF_TOO_FAR);
				break;
			}
		}
	}
	
}

void draw_all_edges()	
{
	g3s_codes cc;
	int i,j,nbright;
	ubyte nfacing,nnfacing;
	Edge_info *e;
	vms_vector *tv1;
	fix distance;
	fix min_distance = 0x7fffffff;
	g3s_point *p1, *p2;
	
	
	nbright=0;

	for (i=0; i<=Highest_edge_index; i++ )	{
		//e = &Edges[Edge_used_list[i]];
		e = &Edges[i];
		if (!(e->flags & EF_USED)) continue;

		if ( e->flags & EF_TOO_FAR) continue;

		if (e->flags&EF_FRONTIER)	{						// A line that is between what we have seen and what we haven't
			if ( (!(e->flags&EF_SECRET))&&(e->color==WALL_NORMAL_COLOR))
				continue;		// If a line isn't secret and is normal color, then don't draw it
		}

		cc=rotate_list(2,e->verts);
		distance = Segment_points[e->verts[1]].z;

		if (min_distance>distance )
			min_distance = distance;

		if (!cc.and) 	{	//all off screen?
			nfacing = nnfacing = 0;
			tv1 = &Vertices[e->verts[0]];
			j = 0;
			while( j<e->num_faces && (nfacing==0 || nnfacing==0) )	{
				#ifdef COMPACT_SEGS
				vms_vector temp_v;
				get_side_normal(&Segments[e->segnum[j]], e->sides[j], 0, &temp_v );
				if (!g3_check_normal_facing( tv1, &temp_v ) )
				#else
				if (!g3_check_normal_facing( tv1, &Segments[e->segnum[j]].sides[e->sides[j]].normals[0] ) )
				#endif
					nfacing++;
				else
					nnfacing++;
				j++;
			}

			if ( nfacing && nnfacing )	{
				// a contour line
				DrawingListBright[nbright++] = e-Edges;
			} else if ( e->flags&(EF_DEFINING|EF_GRATE) )	{
				if ( nfacing == 0 )	{
					if ( e->flags & EF_NO_FADE )
						gr_setcolor( e->color );
					else
						gr_setcolor( gr_fade_table[e->color+256*8] );
					g3_draw_line( &Segment_points[e->verts[0]], &Segment_points[e->verts[1]] );
				} 	else {
					DrawingListBright[nbright++] = e-Edges;
				}
			}
		}
	}
		
///	mprintf( (0, "Min distance=%.2f, ViewDist=%.2f, Delta=%.2f\n", f2fl(min_distance), f2fl(ViewDist), f2fl(min_distance)- f2fl(ViewDist) ));

	if ( min_distance < 0 ) min_distance = 0;

	// Sort the bright ones using a shell sort
	{
		int t;
		int i, j, incr, v1, v2;
	
		incr = nbright / 2;
		while( incr > 0 )	{
			for (i=incr; i<nbright; i++ )	{
				j = i - incr;
				while (j>=0 )	{
					// compare element j and j+incr
					v1 = Edges[DrawingListBright[j]].verts[0];
					v2 = Edges[DrawingListBright[j+incr]].verts[0];

					if (Segment_points[v1].z < Segment_points[v2].z) {
						// If not in correct order, them swap 'em
						t=DrawingListBright[j+incr];
						DrawingListBright[j+incr]=DrawingListBright[j];
						DrawingListBright[j]=t;
						j -= incr;
					}
					else
						break;
				}
			}
			incr = incr / 2;
		}
	}
					
	// Draw the bright ones
	for (i=0; i<nbright; i++ )	{
		int color;
		fix dist;
		e = &Edges[DrawingListBright[i]];
		p1 = &Segment_points[e->verts[0]];
		p2 = &Segment_points[e->verts[1]];
		dist = p1->z - min_distance;
		// Make distance be 1.0 to 0.0, where 0.0 is 10 segments away;
		if ( dist < 0 ) dist=0;
		if ( dist >= Automap_farthest_dist ) continue;

		if ( e->flags & EF_NO_FADE )	{
			gr_setcolor( e->color );
		} else {
			// ASCENT: the target game (0x19022) fades an edge with the
			// depth of its first segment (half of the 31 levels at the
			// largest depth) and with the height of the segment center
			// above or below the player (one level each 5.0 units). An
			// edge with EF_FLICKER takes a random level from the result
			// to full brightness in each frame.
			vms_vector center;
			fix dy;
			object *plr = &Objects[Players[Player_num].objnum];

			compute_segment_center(&center, &Segments[e->segnum[0]]);
			color = Automap_visited[e->segnum[0]] * 31 / (Max_segments_away ? Max_segments_away : 1) / 2;
			dy = center.y - plr->pos.y;
			if (dy < 0) dy = -dy;
			color += f2i(dy) / 5;
			if (color >= 31) color = 31;
			color = 31 - color;
			if ( e->flags & EF_FLICKER )
				color = 31 - rand() % (color + 1);
			gr_setcolor( gr_fade_table[e->color+color*256] );
		}
		g3_draw_line( p1, p2 );
	}

}


//==================================================================
//
// All routines below here are used to build the Edge list
//
//==================================================================


//finds edge, filling in edge_ptr. if found old edge, returns index, else return -1
static int automap_find_edge(int v0,int v1,Edge_info **edge_ptr)
{
	int vv;		// ASCENT: was long, see Edge_info
	short hash,oldhash;
	int ret;

	vv = (v1<<16) + v0;

	oldhash = hash = ((v0*5+v1) % Max_edges);

	ret = -1;

	while (ret==-1) {
		if (Edges[hash].num_faces == 0 ) ret=0;
		else if (Edges[hash].vv == vv) ret=1;
		else {
			if (++hash==Max_edges) hash=0;
			if (hash==oldhash) Error("Edge list full!");
		}
	}

	*edge_ptr = &Edges[hash];

	if (ret == 0)
		return -1;
	else
		return hash;

}


void add_one_edge( short va, short vb, ubyte color, ubyte side, short segnum, int hidden, int grate, int no_fade )	{
	int found;
	Edge_info *e;
	short tmp;

	if ( Num_edges >= Max_edges)	{
		// GET JOHN! (And tell him that his 
		// MAX_EDGES_FROM_VERTS formula is hosed.)
		// If he's not around, save the mine, 
		// and send him  mail so he can look 
		// at the mine later. Don't modify it.
		// This is important if this happens.
		Int3();		// LOOK ABOVE!!!!!!
		return;
	}

	if ( va > vb )	{
		tmp = va;
		va = vb;
		vb = tmp;
	}

	found = automap_find_edge(va,vb,&e);
		
	if (found == -1) {
		e->verts[0] = va;
		e->verts[1] = vb;
		e->color = color;
		e->num_faces = 1;
		e->flags = EF_USED | EF_DEFINING;			// Assume a normal line
		e->sides[0] = side;
		e->segnum[0] = segnum;
		//Edge_used_list[Num_edges] = e-Edges;
		if ( (e-Edges) > Highest_edge_index )
			Highest_edge_index = e - Edges;
		Num_edges++;
	} else {
		//Assert(e->num_faces < 8 );

		if ( color != WALL_NORMAL_COLOR )
			e->color = color;
		if ( e->num_faces < 4 ) {
			e->sides[e->num_faces] = side;					
			e->segnum[e->num_faces] = segnum;
			e->num_faces++;
		}
	}

	if ( grate )
		e->flags |= EF_GRATE;

	if ( hidden )
		e->flags|=EF_SECRET;		// Mark this as a hidden edge
	if ( no_fade )
		e->flags |= EF_NO_FADE;
	if ( Edge_flicker )
		e->flags |= EF_FLICKER;		// ASCENT: 0x192E1
}

void add_one_unknown_edge( short va, short vb )	{
	int found;
	Edge_info *e;
	short tmp;

	if ( va > vb )	{
		tmp = va;
		va = vb;
		vb = tmp;
	}

	found = automap_find_edge(va,vb,&e);
	if (found != -1) 	
		e->flags|=EF_FRONTIER;		// Mark as a border edge
}

extern obj_position Player_init[];

// ASCENT: the edges of a segment in the target game (0x19335). It replaces
// the Descent 1 function: the colors and the rules of the walls are new,
// there are no grate diagonals, no no-fade edges and no start segment.
// The colors are 0 to 63 values of gr_find_closest_color (0xD9ADC).
#define DTU_WALL_SECRET			0x40		// wall flag: secret door (wall.c)
#define DTU_SIDE_TRIGGER_FOUND	0x04		// side flag (pad): a found trigger (docs/re/combat.md)
#define DTU_SIDE_TRAP_FOUND		0x08		// side flag (pad): a found trap
#define DTU_SIDE_SECRET_FOUND		0x10		// side flag (pad): a found secret door

void add_segment_edges(segment *seg)
{
	int	sn;
	int	segnum = seg-Segments;
	int	normal = BM_XRGB( 29, 29, 29 );
	int	blue = gr_find_closest_color( 0, 0, 62 );
	int	green = gr_find_closest_color( 0, 62, 0 );
	int	magenta = gr_find_closest_color( 62, 0, 62 );
	int	dark_cyan = gr_find_closest_color( 0, 30, 30 );

	for (sn=0;sn<MAX_SIDES_PER_SEGMENT;sn++) {
		short	vertex_list[4];
		int	child = seg->children[sn];
		segment *cseg = NULL;
		int	cside = -1;
		int	flags = seg->sides[sn].pad, cflags = 0;
		int	wid = WALL_IS_DOORWAY(seg, sn), cwid = 0;
		int	hidden = 0;
		int	color = normal;

		Edge_flicker = 0;

		if (child > -1) {
			cseg = &Segments[child];
			cside = find_connect_side(seg, cseg);
			if (cside != -1) {
				cflags = cseg->sides[cside].pad;
				cwid = WALL_IS_DOORWAY(cseg, cside);
			}
		}

		if (seg->sides[sn].wall_num > -1) {
			wall *w = &Walls[seg->sides[sn].wall_num];

			switch (w->type) {
			case WALL_DOOR:		// 0x1952D
				if (w->state != WALL_DOOR_CLOSED)
					color = 255;
				else if (!cseg)
					color = normal;
				else if (w->flags & DTU_WALL_SECRET) {
					hidden = 1;
					if (!Automap_visited[segnum] || !Automap_visited[child])
						color = normal;
					else
						color = (w->keys == KEY_NONE) ? magenta : dark_cyan;
				} else if (wid == WID_TRANSILLUSORY_WALL && cwid == wid)
					color = 255;
				// The original reads the side -1 of the other segment when
				// no side connects. The port takes the normal color.
				else if (cside < 0 || seg->sides[sn].tmap_num2 != cseg->sides[cside].tmap_num2)
					color = normal;
				else
					color = (w->keys == KEY_NONE) ? gr_find_closest_color( 20, 62, 62 ) : dark_cyan;
				break;
			case WALL_ILLUSION:	// 0x195FE
				if (w->flags & WALL_ILLUSION_OFF) {
					color = 255;
					break;
				}
				hidden = 1;
				if (!Automap_visited[segnum])
					break;
				if (!cseg) {
					color = magenta;
					break;
				}
				if (!Automap_visited[child])
					break;
				if (wid == WID_TRANSILLUSORY_WALL && cwid == wid)
					color = 255;
				else if (wid == WID_TRANSILLUSORY_WALL || cwid == WID_TRANSILLUSORY_WALL ||
						wid == WID_ILLUSORY_WALL || cwid == WID_ILLUSORY_WALL)
					color = magenta;
				else
					color = normal;
				break;
			case WALL_OPEN:		// 0x194CC
				color = 255;
				break;
			case WALL_CLOSED:		// 0x194F9
				if (!cseg)
					break;
				if (wid == WID_TRANSILLUSORY_WALL || cwid == WID_TRANSILLUSORY_WALL ||
						wid == WID_ILLUSORY_WALL || cwid == WID_ILLUSORY_WALL)
					color = 255;
				else
					color = normal;
				break;
			}
		}

		// 0x194D1: the special segments
		if (seg->special == 6 || seg->special == 2)
			color = gr_find_closest_color( 58, 54, 26 );
		else if (seg->special == 3)
			color = gr_find_closest_color( 62, 32, 0 );

		// 0x1968A: the found triggers, traps and secret doors of this side
		// or the side on the other segment
		if ((flags | cflags) & DTU_SIDE_TRIGGER_FOUND)
			color = blue;
		else if ((flags | cflags) & DTU_SIDE_TRAP_FOUND)
			color = green;
		else if ((flags | cflags) & DTU_SIDE_SECRET_FOUND)
			color = magenta;

		if (color == blue || color == green || color == magenta || color == dark_cyan)
			Edge_flicker = 1;

		// 0x1972D: the segment of the player
		if (segnum == Objects[Players[Player_num].objnum].segnum) {
			color = gr_find_closest_color( 62, 20, 20 );
			hidden = 1;
			Edge_flicker = 1;
		}

		// 0x1977A: a side of the normal color only where there is no
		// segment behind it, or with the hidden flag
		if (color == normal) {
			if (child != -1 && !hidden)
				continue;
		} else if (color == 255)
			continue;

		// If they have a map powerup, draw unvisited areas in dark blue.
		if ((Players[Player_num].flags & PLAYER_FLAGS_MAP_ALL) && !Automap_visited[segnum])
			color = gr_find_closest_color( 0, 0, 50 );

		get_side_verts(vertex_list,segnum,sn);
		add_one_edge( vertex_list[0], vertex_list[1], color, sn, segnum, hidden, 0, 0 );
		add_one_edge( vertex_list[1], vertex_list[2], color, sn, segnum, hidden, 0, 0 );
		add_one_edge( vertex_list[2], vertex_list[3], color, sn, segnum, hidden, 0, 0 );
		add_one_edge( vertex_list[3], vertex_list[0], color, sn, segnum, hidden, 0, 0 );
	}
	Edge_flicker = 0;
}


// Adds all the edges from a segment we haven't visited yet.

void add_unknown_segment_edges(segment *seg)
{
	int sn;
	int segnum = seg-Segments;
	
	for (sn=0;sn<MAX_SIDES_PER_SEGMENT;sn++) {
		short	vertex_list[4];

		// Only add edges that have no children
		if (seg->children[sn] == -1) {
			get_side_verts(vertex_list,segnum,sn);
	
			add_one_unknown_edge( vertex_list[0], vertex_list[1] );
			add_one_unknown_edge( vertex_list[1], vertex_list[2] );
			add_one_unknown_edge( vertex_list[2], vertex_list[3] );
			add_one_unknown_edge( vertex_list[3], vertex_list[0] );
		}


	}

}

void automap_build_edge_list()
{	
	int	i,e1,e2,s;
	Edge_info * e;

	Automap_cheat = 0;

	if ( Players[Player_num].flags & PLAYER_FLAGS_MAP_ALL_CHEAT )
		Automap_cheat = 1;		// Damn cheaters...

	// clear edge list
	for (i=0; i<Max_edges; i++) {
		Edges[i].num_faces = 0;
		Edges[i].flags = 0;
	}
	Num_edges = 0;
	Highest_edge_index = -1;

	// ASCENT: the target game (0x19968) tests the segnum of each segment and
	// does not add the unknown edges (add_unknown_segment_edges).
	if (Automap_cheat || (Players[Player_num].flags & PLAYER_FLAGS_MAP_ALL) )	{
		// Cheating, add all edges as visited
		for (s=0; s<=Highest_segment_index; s++)
			if (Segments[s].segnum != -1)
			{
				add_segment_edges(&Segments[s]);
			}
	} else {
		// Not cheating, add visited edges
		for (s=0; s<=Highest_segment_index; s++)
			if (Segments[s].segnum != -1)
				if (Automap_visited[s]) {
					add_segment_edges(&Segments[s]);
				}
	}

	// Find unnecessary lines (These are lines that don't have to be drawn because they have small curvature)
	for (i=0; i<=Highest_edge_index; i++ )	{
		e = &Edges[i];
		if (!(e->flags&EF_USED)) continue;

		for (e1=0; e1<e->num_faces; e1++ )	{
			for (e2=1; e2<e->num_faces; e2++ )	{
				if ( (e1 != e2) && (e->segnum[e1] != e->segnum[e2]) )	{
					#ifdef COMPACT_SEGS
					vms_vector v1, v2;
					get_side_normal(&Segments[e->segnum[e1]], e->sides[e1], 0, &v1 );
					get_side_normal(&Segments[e->segnum[e2]], e->sides[e2], 0, &v2 );
					if ( vm_vec_dot(&v1,&v2) > (F1_0-(F1_0/10))  )	{
					#else
					// ASCENT: the target game (0x19B3B) keeps the edges that
					// do not have the normal color
					if ( vm_vec_dot( &Segments[e->segnum[e1]].sides[e->sides[e1]].normals[0], &Segments[e->segnum[e2]].sides[e->sides[e2]].normals[0] ) > (F1_0-(F1_0/10))
							&& e->color == WALL_NORMAL_COLOR )	{
					#endif
						e->flags &= (~EF_DEFINING);
						break;
					}
				}
			}
			if (!(e->flags & EF_DEFINING))
				break;
		}
	}	

	mprintf( (0, "Automap used %d / %d edges\n", Num_edges, Max_edges  ));

}

//==================================================================
//
// ASCENT: the automap of the target game (docs/re/automap.md)
//
//==================================================================

// The map is a part of the map screen (interface mode 4), not a mode of
// its own: the game goes on and the view shows the 3D image. Each frame of
// the screen (0x414D9) calls 0x1887E. While the map does not have the
// controls, the frame starts the map again: the edge list, the depths of
// the segments and the view of the player, so that the map follows the
// player. While it has the controls, the keys and the flight controls move
// the map and the player gets no controls (0x40E50).

#define DTU_ROT_SPEED_DIVISOR	11500		// 0x18AB9 (Descent 1: 115000)
#define DTU_ZOOM_SPEED_FACTOR	500		// 0x18A9F (Descent 1: 1500)

int Dtu_automap_active = 0;
int Dtu_fix_automap_reveal = 1;

static vms_angvec Dtu_tangles = { PITCH_DEFAULT, 0, 0 };		// 0x13A810
static control_info Dtu_map_controls;

static void dtu_automap_args(void)
{
	static int done = 0;

	if (done)
		return;
	done = 1;
	if (FindArg("-origui"))
		Dtu_fix_automap_reveal = 0;
}

// 0x18A6D: the start of the map from the player
static void dtu_automap_start(object *plr)
{
	Max_edges = min(MAX_EDGES_FROM_VERTS(Num_vertices),MAX_EDGES);
	automap_build_edge_list();
	ViewMatrix = plr->orient;
	Dtu_tangles.p = PITCH_DEFAULT;
	Dtu_tangles.b = 0;
	Dtu_tangles.h = 0;
	view_target = plr->pos;
	Max_segments_away = set_segment_depths(plr->segnum, Automap_visited);
	SegmentLimit = Max_segments_away;
	adjust_segment_limit(SegmentLimit);
}

// The keys of 0x1893F (key_inkey while the map has the controls)
void dtu_automap_key(int key)
{
	object *plr = &Objects[Players[Player_num].objnum];
	int i;

	dtu_automap_args();
	if (!Dtu_automap_active)
		return;
	switch (key) {
	case KEY_ESC:
		Dtu_automap_active = 0;
		break;
	case KEY_ALTED+KEY_F:		// Alt+F shows the full map, if cheats are on
		if (Cheats_enabled) {
			uint t = Players[Player_num].flags;
			Players[Player_num].flags |= PLAYER_FLAGS_MAP_ALL_CHEAT;
			automap_build_edge_list();
			Players[Player_num].flags = t;
		}
		break;
	case KEY_D:					// 0x189E2: all segments visited
		if (Dtu_fix_automap_reveal)
			break;
		for (i=0; i<=Highest_segment_index; i++)
			Automap_visited[i] = 1;
		automap_build_edge_list();
		Max_segments_away = set_segment_depths(plr->segnum, Automap_visited);
		SegmentLimit = Max_segments_away;
		adjust_segment_limit(SegmentLimit);
		break;
	case KEY_MINUS:
		if (SegmentLimit > 1) {
			SegmentLimit--;
			adjust_segment_limit(SegmentLimit);
		}
		break;
	case KEY_EQUAL:
		if (SegmentLimit < Max_segments_away) {
			SegmentLimit++;
			adjust_segment_limit(SegmentLimit);
		}
		break;
	}
}

int dtu_automap_owns_key(int key)
{
	dtu_automap_args();
	if (!Dtu_automap_active)
		return 0;
	switch (key) {
	case KEY_ESC:
	case KEY_ALTED+KEY_F:
	case KEY_MINUS:
	case KEY_EQUAL:
		return 1;
	case KEY_D:
		return !Dtu_fix_automap_reveal;
	}
	return 0;
}

// The original map uses the Descent control values of the frame (pitch,
// heading, bank, forward, vertical and sideways thrust, the attack count)
// and then clears them and the keys (0x40E50), so the player does not
// move. The port reads the walking controls in dtu_read_controls; this
// function gives the map the same values from the actions: forward and
// back zoom, turn left and right turn, slide left and right and float up
// and down move the center, look up and down pitch (a guess: the key
// table of the executable has no pitch item), attack resets the view.
int dtu_automap_read_controls(void)
{
	static int attack_was = 0;
	control_info *c = &Dtu_map_controls;
	fix ft = FrameTime;
	int a = dtu_action_down(DTU_ACT_ATTACK);

	if (!Dtu_automap_active) {
		attack_was = a;
		return 0;
	}
	memset(c, 0, sizeof(*c));
	if (dtu_action_down(DTU_ACT_FORWARD)) c->forward_thrust_time += ft;
	if (dtu_action_down(DTU_ACT_BACK)) c->forward_thrust_time -= ft;
	if (dtu_action_down(DTU_ACT_TURN_LEFT)) c->heading_time -= ft;
	if (dtu_action_down(DTU_ACT_TURN_RIGHT)) c->heading_time += ft;
	if (dtu_action_down(DTU_ACT_SLIDE_LEFT)) c->sideways_thrust_time -= ft;
	if (dtu_action_down(DTU_ACT_SLIDE_RIGHT)) c->sideways_thrust_time += ft;
	if (dtu_action_down(DTU_ACT_FLOAT_UP)) c->vertical_thrust_time += ft;
	if (dtu_action_down(DTU_ACT_FLOAT_DOWN)) c->vertical_thrust_time -= ft;
	if (dtu_action_down(DTU_ACT_LOOK_UP)) c->pitch_time -= ft;
	if (dtu_action_down(DTU_ACT_LOOK_DOWN)) c->pitch_time += ft;
	c->forward_thrust_time += fixmul(ft, dtu_pad_amount(DTU_ACT_FORWARD) - dtu_pad_amount(DTU_ACT_BACK));
	c->heading_time += fixmul(ft, dtu_pad_amount(DTU_ACT_TURN_RIGHT) - dtu_pad_amount(DTU_ACT_TURN_LEFT));
	c->sideways_thrust_time += fixmul(ft, dtu_pad_amount(DTU_ACT_SLIDE_RIGHT) - dtu_pad_amount(DTU_ACT_SLIDE_LEFT));
	c->pitch_time += fixmul(ft, dtu_pad_amount(DTU_ACT_LOOK_DOWN) - dtu_pad_amount(DTU_ACT_LOOK_UP));
	if (a && !attack_was)
		c->fire_primary_down_count = 1;
	attack_was = a;
	memset(&Controls, 0, sizeof(Controls));
	return 1;
}

void dtu_automap_frame(grs_canvas *parent, int x, int y, int w, int h)
{
	object *plr;
	control_info *c = &Dtu_map_controls;
	vms_matrix tempm;
	vms_vector viewer_position;
	grs_canvas canv;
	int color;

	if (Players[Player_num].objnum < 0 || Players[Player_num].objnum > Highest_object_index)
		return;
	plr = &Objects[Players[Player_num].objnum];
	if (plr->segnum < 0 || plr->segnum > Highest_segment_index)
		return;

	if (!Dtu_automap_active) {
		dtu_automap_start(plr);
		memset(c, 0, sizeof(*c));
	} else {
		if (c->fire_primary_down_count) {
			// 0x18A76: the attack key resets the distance and the angles
			ViewDist = ZOOM_DEFAULT;
			Dtu_tangles.p = PITCH_DEFAULT;
			Dtu_tangles.h = 0;
			Dtu_tangles.b = 0;
		}
		ViewDist -= c->forward_thrust_time*DTU_ZOOM_SPEED_FACTOR;
		Dtu_tangles.p += fixdiv( c->pitch_time, DTU_ROT_SPEED_DIVISOR );
		Dtu_tangles.h += fixdiv( c->heading_time, DTU_ROT_SPEED_DIVISOR );
		Dtu_tangles.b += fixdiv( c->bank_time, DTU_ROT_SPEED_DIVISOR*2 );
		if ( c->vertical_thrust_time || c->sideways_thrust_time ) {
			vms_vector old_vt = view_target;
			vm_angles_2_matrix(&tempm,&Dtu_tangles);
			vm_matrix_x_matrix(&ViewMatrix,&plr->orient,&tempm);
			vm_vec_scale_add2( &view_target, &ViewMatrix.uvec, c->vertical_thrust_time*SLIDE_SPEED );
			vm_vec_scale_add2( &view_target, &ViewMatrix.rvec, c->sideways_thrust_time*SLIDE_SPEED );
			if ( vm_vec_dist_quick( &view_target, &plr->pos) > i2f(1000) )
				view_target = old_vt;
		}
		// the map uses the controls of a frame of the player once
		memset(c, 0, sizeof(*c));
	}

	vm_angles_2_matrix(&tempm,&Dtu_tangles);
	vm_matrix_x_matrix(&ViewMatrix,&plr->orient,&tempm);
	if ( ViewDist < ZOOM_MIN_VALUE ) ViewDist = ZOOM_MIN_VALUE;
	if ( ViewDist > ZOOM_MAX_VALUE ) ViewDist = ZOOM_MAX_VALUE;

	// 0x186BC: the map in a canvas of the screen, cleared with color 255
	gr_init_sub_canvas(&canv, parent, x, y, w, h);
	gr_set_current_canvas(&canv);
	gr_clear_canvas(255);
	g3_start_frame();
	render_start_frame();
	vm_vec_scale_add(&viewer_position,&view_target,&ViewMatrix.fvec,-ViewDist );
	g3_set_view_matrix(&viewer_position,&ViewMatrix,Automap_zoom);
	draw_all_edges();
	// The color of the player is entry Player_num + 1 of player_rgb, a
	// red (27, 0, 0) in a single player game. The objects of Descent 1
	// (hostages, keys) and the texts are not drawn.
	color = Player_num + 1;
	gr_setcolor(BM_XRGB(player_rgb[color].r,player_rgb[color].g,player_rgb[color].b));
	draw_player_dtu(plr);
	g3_end_frame();
	gr_set_current_canvas(parent);
}
