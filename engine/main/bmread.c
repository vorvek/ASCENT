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
 * $Source: f:/miner/source/main/rcs/bmread.c $
 * $Revision: 2.4 $
 * $Author: john $
 * $Date: 1995/03/28 18:05:29 $
 * 
 * Routines to parse bitmaps.tbl
 * 
 * $Log: bmread.c $
 * Revision 2.4  1995/03/28  18:05:29  john
 * Fixed it so you don't have to delete pig after changing bitmaps.tbl
 * 
 * Revision 2.3  1995/03/07  16:52:03  john
 * Fixed robots not moving without edtiro bug.
 * 
 * Revision 2.2  1995/03/06  16:10:20  mike
 * Fix compile errors if building without editor.
 * 
 * Revision 2.1  1995/03/02  14:55:40  john
 * Fixed bug with EDITOR never defined.
 * 
 * Revision 2.0  1995/02/27  11:33:10  john
 * New version 2.0, which has no anonymous unions, builds with
 * Watcom 10.0, and doesn't require parsing BITMAPS.TBL.
 * 
 * Revision 1.1  1995/02/25  14:02:36  john
 * Initial revision
 * 
 * 
 */


#pragma off (unreferenced)
static char rcsid[] = "$Id: bmread.c 2.4 1995/03/28 18:05:29 john Exp $";
#pragma on (unreferenced)

#include "settings.h"

// ASCENT: the original built this whole file only with EDITOR. The target
// game loads its tables from BITMAPS.TBL, so the parser is now always built.
// The parts that need the editor (texture page marks, bm_write_all and the
// PIG dump) are still under EDITOR. conio.h, io.h and cflib.h are removed.

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

#include "types.h"
#include "inferno.h"
#include "gr.h"
#include "bm.h"
#include "mem.h"
#include "mono.h"
#include "error.h"
#include "object.h"
#include "vclip.h"
#include "effects.h"
#include "polyobj.h"
#include "wall.h"
#include "textures.h"
#include "game.h"
#include "multi.h"

#include "iff.h"
#include "cfile.h"

#include "hostage.h"
#include "powerup.h"
#include "laser.h"
#include "sounds.h"
#include "piggy.h"
#include "aistruct.h"
#include "robot.h"
#include "weapon.h"
#include "gauges.h"
#include "player.h"
#include "fuelcen.h"
#include "endlevel.h"
#include "cntrlcen.h"
#include "compbit.h"
#include "args.h"


#include <ctype.h>
#include "dtufile.h"		// ASCENT: the game data files
#include "dtuitemfix.h"	// ASCENT: the rulebook values of the item lines
#include "dtugear.h"		// ASCENT: the new gear (Dtu_new_gear)
#include "dtutblfix.h"	// ASCENT: the table fixup of 1.3 (0x1A344)
#include "dtuspell.h"	// ASCENT: dtu_spell_table_read
#include "dtulich.h"		// ASCENT: the phylactery of the lich of CLAN3
#include "texmerge.h"	// ASCENT: texmerge_mark_wall_effects

// ASCENT: from here on this file is changed in most places. Descent 1
// parsed BITMAPS.TBL only in the editor build, loaded IFF bitmaps and POF
// models from single files, and wrote the result to the PIG file.
// Descent to Undermountain parses the table text of the level each time
// it loads a level. Bitmaps, sounds and models come from the EGG archives
// by name. The table dialect is in docs/formats/tbl.md.
//
// Kept from Descent 1: the keyword loop, the readers of $TEXTURES, $VCLIP,
// $ECLIP, $WCLIP and $SOUND, the texture numbering and the object bitmap
// slots (ObjBitmaps, ObjBitmapPtrs).
// Removed (no table of the game has them): $COCKPIT, $GAUGES, $POWERUP,
// $HOSTAGE, $HOSTAGE_FACE, $ROBOT, $ROBOT_AI, $PLAYER_SHIP, the IFF and
// RAW file loaders and the PIG dump.
// New: $PLAYER, $OBJECT, $ARMOR, $SPELL, $WEAPON (new keywords), $MONSTER,
// $MONSTER_AI, $DRESSING, $INTERFACE, $ICONS.

#define BM_NONE			-1
#define BM_ICONS			 1
#define BM_TEXTURES		 2
#define BM_VCLIP		 	 4
#define BM_EFFECTS	    5
#define BM_ECLIP 	 		 6
#define BM_WALL_ANIMS	12
#define BM_WCLIP 			13
#define BM_INTERFACE		15
#define BM_GUYPICS		16		// in the parser of DTUGAME.EXE, in no table
#define BM_GIRLYPICS		17		// in the parser of DTUGAME.EXE, in no table

#define MAX_BITMAPS_PER_BRUSH 30

extern player_ship only_player_ship;		// In bm.c

static short		N_ObjBitmaps=0;
static short		N_ObjBitmapPtrs=0;
static int			Num_robot_ais = 0;
int	TmapList[MAX_TEXTURES];
char	Powerup_names[MAX_POWERUP_TYPES][POWERUP_NAME_LENGTH];
char	Robot_names[MAX_ROBOT_TYPES][ROBOT_NAME_LENGTH];

char	Bm_level_name[16] = "";
char	Bm_player_name[16] = "";
char	Bm_egg_name[16] = "";
// ASCENT: the number of the first model of the level archive. The models
// of the player archive come before it in the port; DTUGAME.EXE numbers
// the models of the level archive from 0 (dtustate.c: the model numbers of
// a state file).
int	Bm_level_first_model = 0;
int	Bm_tbl_warnings = 0;

//---------------- Internal variables ---------------------------
static int			SuperX = -1;
static int			Installed=0;
static char 		*arg;
static short 		tmap_count = 0;
static short 		texture_count = 0;
static short 		clip_count = 0;
static short 		clip_num;
static short 		sound_num;
static short 		frames;
static float 		time;
static int			hit_sound = -1;
static byte 		bm_flag = BM_NONE;
static int 			abm_flag = 0;
static int 			rod_flag = 0;
static short		wall_open_sound, wall_close_sound,wall_explodes,wall_blastable, wall_hidden;
float		vlighting=0;
static int			obj_eclip;
static char 		*dest_bm;		//clip number to play when destroyed
static int			dest_vclip;		//what vclip to play when exploding
static int			dest_eclip;		//what eclip to play when exploding
static fix			dest_size;		//3d size of explosion
static int			crit_clip;		//clip number to play when destroyed
static int			crit_flag;		//flag if this is a destroyed eclip
static int			tmap1_flag;		//flag if this is used as tmap_num (not tmap_num2)
static int			num_sounds=0;

// ASCENT: the text of the player character table, while the level table
// is parsed. See bm_parse_line().
static char			*Player_tbl_text = NULL;
static int			In_player_text = 0;

// ASCENT: the parser of DTUGAME.EXE puts this character in place of each
// space inside quotes, so that a name stays one token (code at 0x1BB7F).
#define QUOTED_SPACE		((char)0x84)

void verify_textures();
void bm_read_eclip();
void bm_read_wclip();
void bm_read_vclip();
int bm_read_sound();
void bm_read_some_file();
static void bm_read_player();
static void bm_read_monster();
static void bm_read_monster_ai();
static void bm_read_dressing();
static void bm_read_item(int kind);
static void bm_read_dtu_weapon();
static void bm_parse_text(char *text);

extern int linenum;		// defined below

// ASCENT: a table problem that does not stop the load. The original code
// stopped with Error() or with an assert.
static void bm_warning( char * format, ... )
{
	char buf[300];
	va_list args;

	va_start( args, format );
	vsnprintf( buf, sizeof(buf), format, args );
	va_end( args );
	Bm_tbl_warnings++;
	dtu_log( "Table line %d%s: %s\n", linenum, In_player_text ? " (player table)" : "", buf );
}

// ASCENT: replaces _splitpath(filename,NULL,NULL,fname,NULL) of the Watcom
// library: copies the file name without drive, directory and extension.
// The extension starts at the last dot, as in _splitpath. fname has room
// for 20 characters in all callers.
static void bm_get_fname( char * filename, char * fname )
{
	char *p, *start, *dot;
	int len;

	start = filename;
	for (p=filename; *p; p++ )
		if ( (*p==':') || (*p=='/') || (*p=='\\') )
			start = p+1;
	dot = strrchr( start, '.' );
	len = dot ? (int)(dot-start) : (int)strlen(start);
	if (len > 19) len = 19;
	memcpy( fname, start, len );
	fname[len] = 0;
}



//------------------- Useful macros and variables ---------------
#define REMOVE_EOL(s)		remove_char((s),'\n')
#define REMOVE_COMMENTS(s)	remove_char((s),';')
#define REMOVE_DOTS(s)  	remove_char((s),'.')

#define IFTOK(str) if (!strcmp(arg, str))
char *space = { " \t" };
//--unused-- char *equal = { "=" };
char *equal_space = { " \t=" };

void remove_char( char * s, char c )
{
	char *p;
	p = strchr(s,c);
	if (p) *p = '\0';
}

//---------------------------------------------------------------
int compute_average_pixel(grs_bitmap *new)
{
	int	row, column, color;
	char	*pptr;
	int	total_red, total_green, total_blue;

	pptr = new->bm_data;

	total_red = 0;
	total_green = 0;
	total_blue = 0;

	for (row=0; row<new->bm_h; row++)
		for (column=0; column<new->bm_w; column++) {
			color = *pptr++;
			total_red += gr_palette[color*3];
			total_green += gr_palette[color*3+1];
			total_blue += gr_palette[color*3+2];
		}

	total_red /= (new->bm_h * new->bm_w);
	total_green /= (new->bm_h * new->bm_w);
	total_blue /= (new->bm_h * new->bm_w);

	return BM_XRGB(total_red/2, total_green/2, total_blue/2);
}

//---------------------------------------------------------------
// ASCENT: finds a bitmap of the EGG archives by its file name. Descent 1
// also loaded an IFF file when the PIG file did not have the bitmap. A
// name that is not in the archives gives bitmap 0 (the bogus bitmap).

bitmap_index bm_load_sub( char * filename )
{
	bitmap_index bitmap_num;
	char fname[20];

	bitmap_num.index = 0;

	bm_get_fname( filename, fname );		// ASCENT: was _splitpath

	bitmap_num=piggy_find_bitmap( fname );
	if (!bitmap_num.index)
		bm_warning( "bitmap '%s' is not in the EGG archive", filename );

	return bitmap_num;
}

// ASCENT: finds the frames NAME#0, NAME#1, ... of an animation in the EGG
// archives. No frame gives one frame of bitmap 0.
void ab_load( char * filename, bitmap_index bmp[], int *nframes )
{
	bitmap_index bi;
	int i;
	char fname[20];
	char tempname[32];

	bm_get_fname( filename, fname );		// ASCENT: was _splitpath

	for (i=0; i<MAX_BITMAPS_PER_BRUSH; i++ )	{
		sprintf( tempname, "%s#%d", fname, i );
		bi = piggy_find_bitmap( tempname );
		if ( !bi.index )
			break;
		bmp[i] = bi;
	}

	if (i) {
		*nframes = i;
		return;
	}

	bm_warning( "animation '%s' is not in the EGG archive", filename );
	bmp[0].index = 0;
	*nframes = 1;
}

// ASCENT: finds a sound of the EGG archives. Returns SOUND_FILE_NONE if
// the archives do not have it. Descent 1 also loaded NAME.RAW.
int ds_load( char * filename )	{
	char fname[20];

	bm_get_fname( filename, fname );		// ASCENT: was _splitpath

	return piggy_find_sound( fname );
}

//parse a float
// ASCENT: a missing value gives 0. The original gave a NULL pointer to atof().
float get_float()
{
	char *xarg;

	xarg = strtok( NULL, space );
	if (!xarg) return 0;
	return atof( xarg );
}

//parse an int
int get_int()
{
	char *xarg;

	xarg = strtok( NULL, space );
	if (!xarg) return 0;
	return atoi( xarg );
}

// ASCENT: "rod_flag" stands at the end of 280 $VCLIP lines of the game
// tables without "=1". A flag without a value is 1.
static int get_flag_int()
{
	char *xarg;

	xarg = strtok( NULL, space );
	if (!xarg) return 1;
	return atoi( xarg );
}

// ASCENT: was 600. The longest table line has 8244 characters. DTUGAME.EXE
// has a buffer of 0x8000 bytes.
#define LINEBUF_SIZE 0x8000

int	linenum;

// ASCENT: the decoding of a BIN table: every byte but the line end is
// rotated left one bit, changed with XOR 0xD3 and rotated again. This is
// encode_rotate_left() and BITMAP_TBL_XOR of Descent 1 with a new key.
void bm_decode_bin_text( ubyte * text, int len )
{
	int i;
	ubyte b;

	for (i=0; i<len; i++ ) {
		b = text[i];
		if ( b == 0x0A )
			continue;
		b = (ubyte)((b << 1) | (b >> 7));
		b ^= 0xD3;
		b = (ubyte)((b << 1) | (b >> 7));
		text[i] = b;
	}
}

// ASCENT: the start values of all tables, from the first part of the
// Descent 1 bm_init_use_tbl().
static void bm_reset_tables()
{
	int i;

	for (i=0; i<N_tbl_records; i++ )
		if ( Tbl_records[i].text ) {
			free( Tbl_records[i].text );
			Tbl_records[i].text = NULL;
		}
	N_tbl_records = 0;

	ObjType[0] = OL_PLAYER;
	ObjId[0] = 0;
	Num_total_object_types = 1;

	for (i=0; i<MAX_SOUNDS; i++ )	{
		Sounds[i] = SOUND_FILE_NONE;
		AltSounds[i] = SOUND_FILE_NONE;
	}
	num_sounds = 0;

	for (i=0; i<MAX_TEXTURES; i++ ) {
		memset( &TmapInfo[i], 0, sizeof(TmapInfo[i]) );
		TmapInfo[i].eclip_num = -1;
		TmapInfo[i].flags = 0;
		Textures[i].index = 0;
	}

	Num_effects = 0;
	for (i=0; i<MAX_EFFECTS; i++ ) {
		memset( &Effects[i], 0, sizeof(Effects[i]) );
		Effects[i].changing_wall_texture = -1;
		Effects[i].changing_object_texture = -1;
		Effects[i].segnum = -1;
		Effects[i].vc.num_frames = -1;		//another mark of being unused
	}

	for (i=0;i<MAX_POLYGON_MODELS;i++)
		Dying_modelnums[i] = Dead_modelnums[i] = -1;

	Num_vclips = 0;
	for (i=0; i<VCLIP_MAXNUM; i++ )	{
		memset( &Vclip[i], 0, sizeof(Vclip[i]) );
		Vclip[i].num_frames = -1;
		Vclip[i].flags = 0;
	}

	for (i=0; i<MAX_WALL_ANIMS; i++ ) {
		memset( &WallAnims[i], 0, sizeof(WallAnims[i]) );
		WallAnims[i].num_frames = -1;
	}
	Num_wall_anims = 0;

	for (i=0; i<MAX_OBJ_BITMAPS; i++ ) {
		ObjBitmaps[i].index = 0;
		ObjBitmapPtrs[i] = 0;
	}
	N_ObjBitmaps = 0;
	N_ObjBitmapPtrs = 0;

	N_robot_types = 0;
	Num_robot_ais = 0;
	memset( Robot_info, 0, sizeof(robot_info)*MAX_ROBOT_TYPES );
	memset( Monster_info, 0, sizeof(monster_info)*MAX_ROBOT_TYPES );
	memset( Robot_names, 0, sizeof(Robot_names) );
	for (i=0; i<MAX_ROBOT_TYPES; i++ )
		Robot_info[i].model_num = -1;

	N_weapon_types = 0;
	memset( Weapon_info, 0, sizeof(weapon_info)*MAX_WEAPON_TYPES );
	memset( Weapon_item_info, 0, sizeof(item_info)*MAX_WEAPON_TYPES );

	N_powerup_types = 0;

	N_player_types = 0;
	memset( Player_type_info, 0, sizeof(Player_type_info) );
	N_dressing_types = 0;
	memset( Dressing_info, 0, sizeof(Dressing_info) );
	N_misc_types = 0;
	memset( Misc_info, 0, sizeof(Misc_info) );
	N_armor_types = 0;
	memset( Armor_info, 0, sizeof(Armor_info) );
	N_spell_types = 0;
	memset( Spell_info, 0, sizeof(Spell_info) );
	N_interface_bitmaps = 0;
	N_icon_bitmaps = 0;

	Player_ship->model_num = -1;
	Player_ship->expl_vclip_num = -1;

	tmap_count = 0;
	texture_count = 0;
	clip_count = 0;
	NumTextures = 0;
	Num_tmaps = 0;
	bm_flag = BM_NONE;
	abm_flag = 0;
	rod_flag = 0;
	Bm_tbl_warnings = 0;
}

// ASCENT: one line of table text. This is the body of the line loop of
// the Descent 1 bm_init_use_tbl(). The line is changed in place.
static void bm_parse_line( char * inputline )
{
	char *temp_ptr, *p;
	int in_quote;

	REMOVE_EOL(inputline);
	remove_char( inputline, '\r' );
	if (strchr(inputline, ';')!=NULL) REMOVE_COMMENTS(inputline);

	SuperX = -1;

	if ( (temp_ptr=strstr( inputline, "superx=" )) )	{
		SuperX = atoi( &temp_ptr[7] );
	}

	// ASCENT: keep the words of a quoted name together, as DTUGAME.EXE does.
	in_quote = 0;
	for (p=inputline; *p; p++ ) {
		if ( *p == '"' )
			in_quote = !in_quote;
		else if ( *p == ' ' && in_quote )
			*p = QUOTED_SPACE;
	}

	arg = strtok( inputline, space );
	if ( arg == NULL )		// ASCENT: an empty line. The original read arg[0] of a NULL pointer.
		return;
	// ASCENT: '@' marked a line that the shareware version of Descent 1
	// skipped. Some lines of the game tables still have it. It has no effect.
	if (arg[0] == '@')
		arg++;

	// ASCENT: the table of the level has a $PLAYER line with place holder
	// models. DTUGAME.EXE reads the table of the player character in its
	// place (function at 0x25792, called at 0x1C4A8).
	if ( !strcmp(arg, "$PLAYER") && Player_tbl_text != NULL && !In_player_text ) {
		int save_linenum = linenum;
		char *text = Player_tbl_text;

		Player_tbl_text = NULL;		// only the first $PLAYER line
		In_player_text = 1;
		bm_parse_text( text );
		In_player_text = 0;
		linenum = save_linenum;
		free( text );
		bm_flag = BM_NONE;
		return;
	}

	while (arg != NULL )
		{
		// Check all possible flags and defines.
		if (*arg == '$') bm_flag = BM_NONE; // reset to no flags as default.

		IFTOK("$INTERFACE") 			bm_flag = BM_INTERFACE;
		else IFTOK("$ICONS")			bm_flag = BM_ICONS;
		else IFTOK("$GUYPICS")		bm_flag = BM_GUYPICS;
		else IFTOK("$GIRLYPICS")	bm_flag = BM_GIRLYPICS;
		else IFTOK("$SOUND") 		{bm_read_sound(); return;}
		else IFTOK("$DOOR_ANIMS")	bm_flag = BM_WALL_ANIMS;
		else IFTOK("$WALL_ANIMS")	bm_flag = BM_WALL_ANIMS;
		else IFTOK("$TEXTURES") 	bm_flag = BM_TEXTURES;
		else IFTOK("$VCLIP")			{bm_flag = BM_VCLIP;		vlighting = 0;	clip_count = 0; rod_flag = 0;}
		else IFTOK("$ECLIP")			{bm_flag = BM_ECLIP;		vlighting = 0;	clip_count = 0; obj_eclip=0; dest_bm=NULL; dest_vclip=-1; dest_eclip=-1; dest_size=-1; crit_clip=-1; crit_flag=0; sound_num=-1;}
		else IFTOK("$WCLIP")			{bm_flag = BM_WCLIP;		vlighting = 0;	clip_count = 0; wall_explodes = wall_blastable = 0; wall_open_sound=wall_close_sound=-1; tmap1_flag=0; wall_hidden=0;}

		else IFTOK("$EFFECTS")		{bm_flag = BM_EFFECTS;	clip_num = 0;}

		// ASCENT: '!' words mark texture pages for the editor (!METALS_FLAG,
		// !LIGHTS_FLAG, !EFFECTS_FLAG) and a build switch of the EGG tool
		// (!BITMAP_REMAP_OFF, !BITMAP_REMAP_ON). They do nothing here.
		else if (arg[0] == '!')		{ }

		else IFTOK("lighting") 			{ fix v = fl2f(get_float()); if (texture_count > 0) TmapInfo[texture_count-1].lighting = v; }
		else IFTOK("damage") 			{ fix v = fl2f(get_float()); if (texture_count > 0) TmapInfo[texture_count-1].damage = v; }
		else IFTOK("volatile") 			{ if (texture_count > 0) TmapInfo[texture_count-1].flags |= TMI_VOLATILE; }
		// ASCENT: Descent 1 had this keyword in a comment. The game tables
		// have "$EFFECTS Num_effects=23". The clips set the real count.
		else IFTOK("Num_effects")		get_int();
		else IFTOK("Num_wall_anims")	get_int();		// ASCENT: was Num_wall_anims = get_int(). The table says 62 and uses clips up to 81.
		else IFTOK("clip_num")			clip_num = get_int();
		else IFTOK("dest_bm")			dest_bm = strtok( NULL, space );
		else IFTOK("dest_vclip")		dest_vclip = get_int();
		else IFTOK("dest_eclip")		dest_eclip = get_int();
		else IFTOK("dest_size")			dest_size = fl2f(get_float());
		else IFTOK("crit_clip")			crit_clip = get_int();
		else IFTOK("crit_flag")			crit_flag = get_int();
		else IFTOK("sound_num") 		sound_num = get_int();
		else IFTOK("frames") 			frames = get_int();
		else IFTOK("time") 				time = get_float();
		else IFTOK("obj_eclip")			obj_eclip = get_int();
		else IFTOK("hit_sound") 		hit_sound = get_int();
		else IFTOK("abm_flag")			abm_flag = get_int();
		else IFTOK("tmap1_flag")		tmap1_flag = get_int();
		else IFTOK("vlighting")			vlighting = get_float();
		else IFTOK("rod_flag")			rod_flag = get_flag_int();
		else IFTOK("superx") 			get_int();
		else IFTOK("open_sound") 		wall_open_sound = get_int();
		else IFTOK("close_sound") 		wall_close_sound = get_int();
		else IFTOK("explodes")	 		wall_explodes = get_int();
		else IFTOK("blastable")	 		wall_blastable = get_int();
		else IFTOK("hidden")	 			wall_hidden = get_int();

		else IFTOK("$PLAYER")			{bm_read_player();		return;}
		else IFTOK("$MONSTER")			{bm_read_monster();		return;}
		else IFTOK("$MONSTER_AI")		{bm_read_monster_ai();	return;}
		else IFTOK("$DRESSING")			{bm_read_dressing();		return;}
		else IFTOK("$OBJECT")			{bm_read_item(OL_MISC);	return;}
		else IFTOK("$ARMOR")				{bm_read_item(OL_ARMOR);	return;}
		else IFTOK("$SPELL")				{bm_read_item(OL_SPELL);	return;}
		else IFTOK("$WEAPON")			{bm_read_dtu_weapon();	return;}

		else if (arg[0] == '$')	{
			// ASCENT: a section that this parser does not know. The line is
			// skipped. Bitmap lines that follow it are skipped too (BM_NONE).
			bm_warning( "unknown section %s, skipped", arg );
			return;
		}

		else	{		//not a special token, must be a bitmap!

			// Remove any illegal/unwanted spaces and tabs at this point.
			while ((*arg=='\t') || (*arg==' ')) arg++;
			if (*arg == '\0') { break; }

			// Otherwise, 'arg' is apparently a bitmap filename.
			// Load bitmap and process it below:
			bm_read_some_file();

		}

		arg = strtok( NULL, equal_space );
		continue;
      }
}

// ASCENT: parse a text of many lines. The text is changed in place.
static void bm_parse_text( char * text )
{
	char *line, *next;

	// One line buffer for each nesting level (level table, player table).
	char *buf = (char *) malloc( LINEBUF_SIZE );
	if ( buf == NULL )
		return;

	linenum = 0;
	for (line=text; line && *line; line=next ) {
		int len;

		next = strchr( line, '\n' );
		len = next ? (int)(next-line) : (int)strlen(line);
		if ( next ) next++;
		linenum++;

		if ( len >= LINEBUF_SIZE-1 ) {
			bm_warning( "line too long (%d characters), skipped", len );
			continue;
		}
		memcpy( buf, line, len );
		buf[len] = 0;

		bm_parse_line( buf );
	}

	free( buf );
}

// ASCENT: load one table file and return its text (decoded, with a 0 at
// the end) in memory that the caller releases. egg_name gets the name of
// the EGG archive from the first 13 bytes.
static char *bm_load_bin( char * name, char * egg_name )
{
	char filename[32];
	ubyte *data;
	char *text;
	int size;

	egg_name[0] = 0;
	if ( strlen(name) > 8 )
		return NULL;
	sprintf( filename, "%s.BIN", name );
	data = dtu_load_file( filename, &size );
	if ( data == NULL )
		return NULL;
	if ( size < 14 ) {
		free( data );
		return NULL;
	}
	memcpy( egg_name, data, 13 );
	egg_name[13] = 0;

	text = (char *) malloc( size - 13 + 1 );
	if ( text == NULL ) {
		free( data );
		return NULL;
	}
	memcpy( text, data+13, size-13 );
	text[size-13] = 0;
	free( data );
	bm_decode_bin_text( (ubyte *) text, size-13 );
	return text;
}

// ASCENT: the records of the table fixup of 1.3 (dtutblfix.h)
static item_info *tblfix_item( int kind, int id )
{
	if ( id < 0 ) return NULL;
	switch ( kind ) {
		case DTU_TK_MISC:		return id < N_misc_types ? &Misc_info[id] : NULL;
		case DTU_TK_ARMOR:	return id < N_armor_types ? &Armor_info[id] : NULL;
		case DTU_TK_WEAPON:	return id < N_weapon_types ? &Weapon_item_info[id] : NULL;
		case DTU_TK_SPELL:	return id < N_spell_types ? &Spell_info[id] : NULL;
	}
	return NULL;
}

static int tblfix_count( int kind )
{
	switch ( kind ) {
		case DTU_TK_MISC:		return N_misc_types;
		case DTU_TK_ARMOR:	return N_armor_types;
		case DTU_TK_WEAPON:	return N_weapon_types;
		case DTU_TK_SPELL:	return N_spell_types;
	}
	return 0;
}

static char *tblfix_text( int kind, int id )
{
	item_info *it = tblfix_item( kind, id );

	if ( it == NULL || it->record < 0 || it->record >= N_tbl_records )
		return NULL;
	return Tbl_records[it->record].text;
}

static void tblfix_set_text( int kind, int id, char * text )
{
	item_info *it = tblfix_item( kind, id );

	char *copy;

	// ASCENT: text comes from the C library (dtutblfix.c has no mem.h) and
	// the records are freed with the mem library: keep a copy of the mem
	// library.
	if ( text == NULL )
		return;
	if ( it == NULL || it->record < 0 || it->record >= N_tbl_records ) {
		dtu_tblfix_free( text );
		return;
	}
	copy = malloc( strlen(text) + 1 );
	strcpy( copy, text );
	dtu_tblfix_free( text );
	if ( Tbl_records[it->record].text )
		free( Tbl_records[it->record].text );
	Tbl_records[it->record].text = copy;
}

static char *tblfix_name( int kind, int id )
{
	item_info *it = tblfix_item( kind, id );

	return it ? it->name : NULL;
}

// The copy of a whole line (memcpy 0xD7CD2): the record and the name stay.
static void tblfix_copy_line( int kind, int dst, int src )
{
	item_info *d = tblfix_item( kind, dst ), *s = tblfix_item( kind, src );
	item_info keep;

	if ( d == NULL || s == NULL )
		return;
	keep = *d;
	*d = *s;
	d->record = keep.record;
	memcpy( d->name, keep.name, sizeof(d->name) );
	if ( kind == DTU_TK_WEAPON )
		Weapon_info[dst] = Weapon_info[src];
}

// The fields of item_info that the readers take from the text
static void tblfix_sync( int kind, int id )
{
	item_info *it = tblfix_item( kind, id );
	char *t = tblfix_text( kind, id ), v[32];

	if ( it == NULL || t == NULL )
		return;
	if ( dtu_tblfix_get( t, "value", v, sizeof(v) ) )		it->value = atoi(v);
	if ( dtu_tblfix_get( t, "icon", v, sizeof(v) ) )		it->icon = atoi(v);
	if ( dtu_tblfix_get( t, "inv_icon", v, sizeof(v) ) )	it->inv_icon = atoi(v);
	if ( kind == DTU_TK_SPELL ) {
		it->vclip_num = dtu_tblfix_get( t, "weapon_vclip", v, sizeof(v) ) ? atoi(v) : -1;
		if ( !dtu_tblfix_get( t, "weapon_pof", v, sizeof(v) ) )
			it->model_num = -1;
	}
}

static void tblfix_log( const char * text )
{
	dtu_log( "%s\n", text );
}

static const dtu_tblfix_io Tblfix_io = {
	tblfix_count, tblfix_text, tblfix_set_text, tblfix_name, DTU_NAME_LENGTH,
	tblfix_copy_line, tblfix_sync, tblfix_log
};

// ASCENT: Dtu_caster_ai (dtuopts.h, a decision of the user of 2026-11-27):
// the drow priestesses hold a weapon. The data of 1.3 has two models of the
// drow woman with the hands free, dlf_ffk.pof (rffk8, rffk8b, rffk9,
// drowfem) and dlf_ffk5.pof (rffk5), in DROW.EGG, FLAME.EGG, NETHER.EGG and
// PORTAL.EGG, and one with a mace in the right hand, dlf_fft.pof, the
// model of the type rfft6 of the table NETHER.BIN. It is only in
// NETHER.EGG, which no level of 1.3 loads (the disc has it as NETHER.COW;
// a folder without the file keeps the models of 1.3). Its textures are the
// ten of dlf_ffk.pof and the texture of the mace (m_a01.bbm), which the
// level archives of the drow have, in the order of the line of rfft6. The
// creature types with a spell list of a drow priestess (dtucaster.h) whose
// line has one of the two models get dlf_fft.pof.
grs_bitmap *load_polymodel_bitmap(char *name);
static void line_models_start();
static void line_model_add( char * name );
static void line_models_load( int * model_nums, robot_info * r );
int dtu_caster_ai(void);		// dtuobj.h

static int priestess_line( int i )
{
	int mn = Robot_info[i].model_num;

	if ( mn < 0 || mn >= N_polygon_models || ( stricmp( Pof_names[mn], "dlf_ffk.pof" ) && stricmp( Pof_names[mn], "dlf_ffk5.pof" ) ) )
		return 0;
	return !strnicmp( Monster_info[i].name, "rff", 3 ) || !stricmp( Monster_info[i].name, "drowfem" );
}

static void bm_port_priestess()
{
	static char *tex[13] = { "dlf_ff01.bbm", "dlf_ff02.bbm", "dlf_ff03.bbm", "dlf_ff02.bbm", "dlf_ff03.bbm", "dlf_ff04.bbm", "dlf_ff05.bbm",
		"dlf_ff06.bbm", "dlf_ff07.bbm", "dlf_ff08.bbm", "m_a01.bbm", "dlf_ff09.bbm", "dlf_ff10.bbm" };
	int i, k, any = 0, model_nums[MAX_PLAYER_MODELS];

	if ( !dtu_caster_ai() )
		return;
	for (i=0; i<N_robot_types; i++ )
		any |= priestess_line( i );
	if ( !any )
		return;
	if ( polyobj_find_model( "dlf_fft.pof" ) < 0 && piggy_load_egg_model( "NETHER.EGG", "dlf_fft.pof" ) < 0 ) {
		dtu_log( "The model dlf_fft.pof of NETHER.EGG is not there: the drow priestesses keep their model (Dtu_caster_ai)\n" );
		return;
	}
	line_models_start();
	line_model_add( "dlf_fft.pof" );
	for (k=0; k<13; k++ )
		load_polymodel_bitmap( tex[k] );
	line_models_load( model_nums, NULL );
	if ( model_nums[0] < 0 )
		return;
	for (i=0; i<N_robot_types; i++ )
		if ( priestess_line( i ) ) {
			dtu_log( "Monster '%s': the model dlf_fft.pof (a mace in the hand) in place of %s (Dtu_caster_ai)\n", Monster_info[i].name,
				Pof_names[Robot_info[i].model_num] );
			Robot_info[i].model_num = model_nums[0];
		}
}

int bm_load_level_tables( char * level_name, char * player_name )
{
	char egg_name[16], player_egg_name[16];
	char *text, *player_text = NULL;
	int i;

	if ( level_name == NULL || level_name[0] == 0 )
		return 0;
	if ( player_name != NULL && player_name[0] == 0 )
		player_name = NULL;

	if ( !stricmp( level_name, Bm_level_name ) && !stricmp( player_name ? player_name : "", Bm_player_name ) )
		return 1;		// "Don't need to re-load.", as DTUGAME.EXE says

	bm_init();

	dtu_log_clear();

	text = bm_load_bin( level_name, egg_name );
	if ( text == NULL || egg_name[0] == 0 ) {
		dtu_log( "Cannot load the table file of level '%s'\n", level_name );
		if ( text ) free( text );
		return 0;
	}

	player_egg_name[0] = 0;
	if ( player_name ) {
		player_text = bm_load_bin( player_name, player_egg_name );
		if ( player_text == NULL || player_egg_name[0] == 0 ) {
			dtu_log( "Cannot load the table file of player character '%s'\n", player_name );
			if ( player_text ) free( player_text );
			player_text = NULL;
			player_egg_name[0] = 0;
		}
	}

	// From here on the tables of the level before are gone.
	piggy_free_all();
	bm_reset_tables();
	Bm_level_name[0] = 0;
	Bm_player_name[0] = 0;
	Bm_egg_name[0] = 0;

	if ( !Installed ) {
		Installed = 1;
		atexit(bm_close);
	}

	// DTUGAME.EXE registers the bitmaps, sounds and models of the player
	// archives before those of the level archive (piggy.c, 0x70303).
	if ( player_egg_name[0] && piggy_load_egg( player_egg_name ) < 0 ) {
		dtu_log( "Cannot load the archive of player character '%s'\n", player_name );
		free( player_text );
		player_text = NULL;
	}

	Bm_level_first_model = N_polygon_models;
	if ( piggy_load_egg( egg_name ) < 0 ) {
		free( text );
		if ( player_text ) free( player_text );
		return 0;
	}

	Player_tbl_text = player_text;		// bm_parse_line() releases it
	In_player_text = 0;

	bm_parse_text( text );
	free( text );

	if ( Player_tbl_text ) {		// the level table had no $PLAYER line
		In_player_text = 1;
		bm_parse_text( Player_tbl_text );
		In_player_text = 0;
		free( Player_tbl_text );
		Player_tbl_text = NULL;
	}

	// ASCENT: the table fixup of 1.3 (0x1A344, the end of bm_read_all
	// 0x1C8F9), after both tables. -rawtables keeps the table text.
	Dtu_raw_tables = FindArg( "-rawtables" ) != 0;
	if ( !Dtu_raw_tables ) {
		dtu_tblfix_apply( &Tblfix_io );
		// ASCENT: the new gear (Dtu_new_gear, dtugear.h): the wands, two
		// values and the new item lines, after the fixup
		dtu_gear_tables( &Tblfix_io, bm_parse_text );
	}
	else {
		dtu_log( "Table fixup of 1.3 not applied (-rawtables)\n" );
		Dtu_gear_loaded = 0;
	}
	// ASCENT: the spells of the port (dtuspell.h): five lines after the 56
	// of 1.3, read as lines of the table. A table that has a line at these
	// numbers keeps it.
	{
		const char *l;
		int k;

		for (k = 0; (l = dtu_spell_port_line(k)) != NULL; k++) {
			char buf[1024];
			int n = atoi(l + 7);

			if (n < N_spell_types && Spell_info[n].record >= 0)
				continue;
			strncpy(buf, l, sizeof(buf) - 1);
			buf[sizeof(buf) - 1] = 0;
			bm_parse_line(buf);
		}
	}
	dtu_lich_tables( level_name, N_dressing_types, bm_parse_line );	// ASCENT: the phylactery of the lich of CLAN3 (dtulich.h)
	bm_port_priestess();		// ASCENT: Dtu_caster_ai: the drow priestesses with a weapon
	dtu_spell_table_read();		// ASCENT: the writes of the casts into the spell records end (D-104, D-105)

	NumTextures = texture_count;
	Num_tmaps = tmap_count;

	if ( N_robot_types != Num_robot_ais )		//should be one ai info per robot
		dtu_log( "Table: %d $MONSTER lines and %d $MONSTER_AI lines\n", N_robot_types, Num_robot_ais );

	verify_textures();

	//check for refereced but unused clip count
	for (i=0; i<MAX_EFFECTS; i++ )
		if (	(
				  (Effects[i].changing_wall_texture!=-1) ||
				  (Effects[i].changing_object_texture!=-1)
             )
			 && (Effects[i].vc.num_frames==-1) ) {
			// ASCENT: was Error(). The slot keeps bitmap 0.
			dtu_log( "EClip %d referenced (by polygon object?), but not defined\n", i );
			Bm_tbl_warnings++;
			Effects[i].changing_wall_texture = -1;
			Effects[i].changing_object_texture = -1;
		}

	// ASCENT: the effect colors of the wall textures without an overlay
	// (Dtu_fix_wall_effects, texmerge.c)
	texmerge_mark_wall_effects();

	strncpy( Bm_level_name, level_name, 15 );
	strncpy( Bm_player_name, player_name ? player_name : "", 15 );
	strncpy( Bm_egg_name, egg_name, 15 );

	dtu_log( "Tables of '%s': %d textures, %d vclips, %d eclips, %d wall clips, %d monsters, %d weapons, %d warnings\n",
		level_name, NumTextures, Num_vclips, Num_effects, Num_wall_anims, N_robot_types, N_weapon_types, Bm_tbl_warnings );

	return 1;
}

//-----------------------------------------------------------------
// ASCENT: Descent 1 read BITMAPS.TBL here. See bm.h.
int bm_init_use_tbl()
{
	bm_init();
	return bm_load_level_tables( "ypinn", NULL ) ? 0 : 1;
}

// ASCENT: Descent 1 stopped the program if a texture was not 64 x 64. The
// second $TEXTURES section of the game tables holds interface art of other
// sizes, so this only counts.
int Bm_num_odd_size_textures = 0;

void verify_textures()
{
	grs_bitmap * bmp;
	int i,j;
	j=0;
	for (i=0; i<Num_tmaps; i++ )	{
		bmp = &GameBitmaps[Textures[i].index];
		if ( (bmp->bm_w!=64)||(bmp->bm_h!=64)||(bmp->bm_rowsize!=64) )	{
			j++;
		}
	}
	Bm_num_odd_size_textures = j;
}

void bm_close()
{
	if (Installed)
	{
		int i;

		// ASCENT: release the text of the table records
		for (i=0; i<N_tbl_records; i++ )
			if ( Tbl_records[i].text ) {
				free( Tbl_records[i].text );
				Tbl_records[i].text = NULL;
			}
		N_tbl_records = 0;

		piggy_close();
		Installed=0;
 	}
}

void set_lighting_flag(byte *bp)
{
	if (vlighting < 0)
		*bp |= BM_FLAG_NO_LIGHTING;
	else
		*bp &= (0xff ^ BM_FLAG_NO_LIGHTING);
}

set_texture_name(char *name)
{
	// ASCENT: the name is cut to the size of the field
	strncpy ( TmapInfo[texture_count].filename, name, 12 );
	TmapInfo[texture_count].filename[12] = 0;
	REMOVE_DOTS(TmapInfo[texture_count].filename);
}

// ASCENT: 1 if there is room for n more textures. The original had asserts.
static int bm_texture_room( int n )
{
	if ( texture_count + n >= MAX_TEXTURES || tmap_count + n >= MAX_TEXTURES ) {
		bm_warning( "too many textures (%d)", MAX_TEXTURES );
		return 0;
	}
	return 1;
}

void bm_read_eclip()
{
	bitmap_index bitmap;

	if ( clip_num < 0 || clip_num >= MAX_EFFECTS ) {
		bm_warning( "eclip number %d is too high", clip_num );
		abm_flag = 0;
		return;
	}
	if ( !bm_texture_room(2) )
		return;

	if (clip_num+1 > Num_effects)
		Num_effects = clip_num+1;

	Effects[clip_num].flags = 0;

	if (!abm_flag)	{
		if ( frames < 1 || frames > VCLIP_MAX_FRAMES || clip_count >= frames ) {
			bm_warning( "eclip %d: bad frame count", clip_num );
			return;
		}
		bitmap = bm_load_sub(arg);

		Effects[clip_num].vc.play_time = fl2f(time);
		Effects[clip_num].vc.num_frames = frames;
		Effects[clip_num].vc.frame_time = fl2f(time)/frames;

		Effects[clip_num].vc.frames[clip_count] = bitmap;
		set_lighting_flag(&GameBitmaps[bitmap.index].bm_flags);

		if (clip_count == 0) {
			Effects[clip_num].changing_wall_texture = texture_count;
	  		TmapList[tmap_count++] = texture_count;
			Textures[texture_count] = bitmap;
			set_texture_name(arg);
			// ASCENT: Descent 1 set eclip_num of the next texture here (after
			// the increment). No game table has an eclip without abm_flag.
			TmapInfo[texture_count].eclip_num = clip_num;
			texture_count++;
			NumTextures = texture_count;
		}

		clip_count++;

	} else {
		bitmap_index bm[MAX_BITMAPS_PER_BRUSH];
		abm_flag = 0;

		ab_load( arg, bm, &Effects[clip_num].vc.num_frames );

		//printf("EC%d.", clip_num);
		Effects[clip_num].vc.play_time = fl2f(time);
		Effects[clip_num].vc.frame_time = Effects[clip_num].vc.play_time/Effects[clip_num].vc.num_frames;

		clip_count = 0;
		set_lighting_flag( &GameBitmaps[bm[clip_count].index].bm_flags);
		Effects[clip_num].vc.frames[clip_count] = bm[clip_count];

		if (!obj_eclip && !crit_flag) {
			Effects[clip_num].changing_wall_texture = texture_count;
  			TmapList[tmap_count++] = texture_count;
			Textures[texture_count] = bm[clip_count];
			set_texture_name( arg );
			TmapInfo[texture_count].eclip_num = clip_num;
			texture_count++;
			NumTextures = texture_count;
		}

		if (obj_eclip) {

			if (Effects[clip_num].changing_object_texture == -1) {		//first time referenced
				if ( N_ObjBitmaps < MAX_OBJ_BITMAPS ) {
					Effects[clip_num].changing_object_texture = N_ObjBitmaps;		// XChange ObjectBitmaps
					N_ObjBitmaps++;
				}
			}

			if ( Effects[clip_num].changing_object_texture != -1 )
				ObjBitmaps[Effects[clip_num].changing_object_texture] = Effects[clip_num].vc.frames[0];
		}

		//if for an object, Effects_bm_ptrs set in object load

		for(clip_count=1;clip_count < Effects[clip_num].vc.num_frames; clip_count++) {
			set_lighting_flag( &GameBitmaps[bm[clip_count].index].bm_flags);
			Effects[clip_num].vc.frames[clip_count] = bm[clip_count];
		}

	}

	Effects[clip_num].crit_clip = crit_clip;
	Effects[clip_num].sound_num = sound_num;

	if (dest_bm) {			//deal with bitmap for blown up clip
		char short_name[13];
		int i;
		strncpy(short_name,dest_bm,12);
		short_name[12] = 0;
		REMOVE_DOTS(short_name);
		for (i=0;i<texture_count;i++)
			if (!stricmp(TmapInfo[i].filename,short_name))
				break;
		if (i==texture_count) {
			Textures[texture_count] = bm_load_sub(dest_bm);
			strcpy( TmapInfo[texture_count].filename, short_name);
			texture_count++;
			NumTextures = texture_count;
		}
		Effects[clip_num].dest_bm_num = i;

		if (dest_vclip==-1 || dest_size==-1)
			bm_warning( "eclip %d: destruction vclip or size is missing", clip_num );

		Effects[clip_num].dest_vclip = dest_vclip;
		Effects[clip_num].dest_size = dest_size;

		Effects[clip_num].dest_eclip = dest_eclip;
	}
	else {
		Effects[clip_num].dest_bm_num = -1;
		Effects[clip_num].dest_eclip = -1;
	}

	if (crit_flag)
		Effects[clip_num].flags |= EF_CRITICAL;
}


void bm_read_wclip()
{
	bitmap_index bitmap;

	if ( clip_num < 0 || clip_num >= MAX_WALL_ANIMS ) {
		bm_warning( "wall clip number %d is too high", clip_num );
		abm_flag = 0;
		return;
	}

	WallAnims[clip_num].flags = 0;

	if (wall_explodes)	WallAnims[clip_num].flags |= WCF_EXPLODES;
	if (wall_blastable)	WallAnims[clip_num].flags |= WCF_BLASTABLE;
	if (wall_hidden)		WallAnims[clip_num].flags |= WCF_HIDDEN;
	if (tmap1_flag)		WallAnims[clip_num].flags |= WCF_TMAP1;

	if (!abm_flag)	{
		if ( (WallAnims[clip_num].num_frames>-1) && (clip_count==0) )
			bm_warning( "Wall Clip %d is already used!", clip_num );
		if ( frames < 1 || frames > MAX_CLIP_FRAMES || clip_count >= frames || !bm_texture_room(1) ) {
			bm_warning( "wall clip %d: bad frame count", clip_num );
			return;
		}
		bitmap = bm_load_sub(arg);
		WallAnims[clip_num].play_time = fl2f(time);
		WallAnims[clip_num].num_frames = frames;
		//WallAnims[clip_num].frame_time = fl2f(time)/frames;
		WallAnims[clip_num].frames[clip_count++] = texture_count;
		WallAnims[clip_num].open_sound = wall_open_sound;
		WallAnims[clip_num].close_sound = wall_close_sound;
		Textures[texture_count] = bitmap;
		set_lighting_flag(&GameBitmaps[bitmap.index].bm_flags);
		set_texture_name( arg );
		texture_count++;
		NumTextures = texture_count;
		if (clip_num >= Num_wall_anims) Num_wall_anims = clip_num+1;
	} else {
		bitmap_index bm[MAX_BITMAPS_PER_BRUSH];
		int nframes;
		char fname[20];
		if ( (WallAnims[clip_num].num_frames>-1)  )
			bm_warning( "AB_Wall clip %d is already used!", clip_num );
		abm_flag = 0;
		ab_load( arg, bm, &nframes );
		if ( nframes > MAX_CLIP_FRAMES ) {
			bm_warning( "wall clip %d has %d frames, %d are used", clip_num, nframes, MAX_CLIP_FRAMES );
			nframes = MAX_CLIP_FRAMES;
		}
		if ( !bm_texture_room(nframes) )
			return;
		WallAnims[clip_num].num_frames = nframes;
		//printf("WC");
		WallAnims[clip_num].play_time = fl2f(time);
		//WallAnims[clip_num].frame_time = fl2f(time)/nframes;
		WallAnims[clip_num].open_sound = wall_open_sound;
		WallAnims[clip_num].close_sound = wall_close_sound;

		bm_get_fname( arg, fname );
		strncpy(WallAnims[clip_num].filename, fname, 12);
		WallAnims[clip_num].filename[12] = 0;

		if (clip_num >= Num_wall_anims) Num_wall_anims = clip_num+1;

		for (clip_count=0;clip_count < WallAnims[clip_num].num_frames; clip_count++)	{
			//printf("%d", clip_count);
			Textures[texture_count] = bm[clip_count];
			set_lighting_flag(&GameBitmaps[bm[clip_count].index].bm_flags);
			WallAnims[clip_num].frames[clip_count] = texture_count;
			// ASCENT: the name is "name#frame" and fits the 13 bytes of the
			// field (8 + 1 + 2 + 0). The original used sprintf.
			snprintf( TmapInfo[texture_count].filename, 13, "%s#%d", fname, clip_count);
			texture_count++;
			NumTextures = texture_count;
		}
	}
}

void bm_read_vclip()
{
	bitmap_index bi;

	if ( clip_num < 0 || clip_num >= VCLIP_MAXNUM ) {
		bm_warning( "vclip number %d is too high", clip_num );
		abm_flag = 0;
		return;
	}

	if (clip_num >= Num_vclips)		// ASCENT: Descent 1 never set Num_vclips in this file
		Num_vclips = clip_num+1;

	if (!abm_flag)	{
		if ( (Vclip[clip_num].num_frames>-1) && (clip_count==0)  )
			bm_warning( "Vclip %d is already used!", clip_num );
		if ( frames < 1 || frames > VCLIP_MAX_FRAMES || clip_count >= frames ) {
			bm_warning( "vclip %d: bad frame count", clip_num );
			return;
		}
		bi = bm_load_sub(arg);
		Vclip[clip_num].play_time = fl2f(time);
		Vclip[clip_num].num_frames = frames;
		Vclip[clip_num].frame_time = fl2f(time)/frames;
		Vclip[clip_num].light_value = fl2f(vlighting);
		Vclip[clip_num].sound_num = sound_num;
		set_lighting_flag(&GameBitmaps[bi.index].bm_flags);
		Vclip[clip_num].frames[clip_count++] = bi;
		if (rod_flag) {
			rod_flag=0;
			Vclip[clip_num].flags |= VF_ROD;
		}

	} else	{
		bitmap_index bm[MAX_BITMAPS_PER_BRUSH];
		abm_flag = 0;
		if ( (Vclip[clip_num].num_frames>-1)  )
			bm_warning( "AB_Vclip %d is already used!", clip_num );
		ab_load( arg, bm, &Vclip[clip_num].num_frames );

		if (rod_flag) {
			//int i;
			rod_flag=0;
			Vclip[clip_num].flags |= VF_ROD;
		}
		//printf("VC");
		Vclip[clip_num].play_time = fl2f(time);
		Vclip[clip_num].frame_time = fl2f(time)/Vclip[clip_num].num_frames;
		Vclip[clip_num].light_value = fl2f(vlighting);
		Vclip[clip_num].sound_num = sound_num;

		for (clip_count=0;clip_count < Vclip[clip_num].num_frames; clip_count++) {
			//printf("%d", clip_count);
			set_lighting_flag(&GameBitmaps[bm[clip_count].index].bm_flags);
			Vclip[clip_num].frames[clip_count] = bm[clip_count];
		}
	}
}

// ------------------------------------------------------------------------------
//	Convert field of view from an angle in 0..360 to cosine.
void adjust_field_of_view(fix *fovp)
{
	int		i;
	fixang	tt;
	float		ff;
	fix		temp;

	for (i=0; i<NDL; i++) {
		ff = - f2fl(fovp[i]);
		if (ff > 179) {
			mprintf((1, "Warning: Bogus field of view (%7.3f).  Must be in 0..179.\n", ff));
			ff = 179;
		}
		ff = ff/360;
		tt = fl2f(ff);
		fix_sincos(tt, &temp, &fovp[i]);
	}
}

bm_read_sound()
{
	int sound_num;
	int alt_sound_num;
	int file_num;

	sound_num = get_int();
	alt_sound_num = get_int();

	arg = strtok(NULL, space);

	if ( sound_num < 0 || sound_num>=MAX_SOUNDS || arg == NULL ) {
		bm_warning( "bad $SOUND line (sound %d)", sound_num );
		return 0;
	}

	if (sound_num >= num_sounds)
		num_sounds = sound_num+1;

	file_num = ds_load(arg);
	Sounds[sound_num] = file_num;

	if ( alt_sound_num == 0 )
		AltSounds[sound_num] = sound_num;
	else if (alt_sound_num < 0 || alt_sound_num >= MAX_SOUNDS )
		AltSounds[sound_num] = SOUND_FILE_NONE;
	else
		AltSounds[sound_num] = alt_sound_num;

	// ASCENT: was Error(). A table can name a sound that only another
	// archive has.
	if (file_num == SOUND_FILE_NONE)
		bm_warning( "sound '%s' is not in the EGG archive", arg );

	return 0;
}

//	----------------------------------------------------------------------------------------------
//this will load a bitmap for a polygon models.  it puts the bitmap into
//the array ObjBitmaps[], and also deals with animating bitmaps
//returns a pointer to the bitmap
// ASCENT: returns NULL too if the tables are full
grs_bitmap *load_polymodel_bitmap(char *name)
{
	if ( N_ObjBitmaps >= MAX_OBJ_BITMAPS || N_ObjBitmapPtrs >= MAX_OBJ_BITMAPS ) {
		bm_warning( "too many object bitmaps (%d)", MAX_OBJ_BITMAPS );
		return NULL;
	}

	if (name[0] == '%') {		//an animating bitmap!
		int eclip_num;

		eclip_num = atoi(name+1);
		if ( eclip_num < 0 || eclip_num >= MAX_EFFECTS ) {
			bm_warning( "eclip number %d of an object bitmap is too high", eclip_num );
			eclip_num = 0;
		}

		if (Effects[eclip_num].changing_object_texture == -1) {		//first time referenced
			Effects[eclip_num].changing_object_texture = N_ObjBitmaps;
			ObjBitmapPtrs[N_ObjBitmapPtrs++] = N_ObjBitmaps;
			N_ObjBitmaps++;
		} else {
			ObjBitmapPtrs[N_ObjBitmapPtrs++] = Effects[eclip_num].changing_object_texture;
		}
		return NULL;
	}
	else 	{
		ObjBitmaps[N_ObjBitmaps] = bm_load_sub(name);
		ObjBitmapPtrs[N_ObjBitmapPtrs++] = N_ObjBitmaps;
		N_ObjBitmaps++;
		return &GameBitmaps[ObjBitmaps[N_ObjBitmaps-1].index];
	}
}

void bm_read_some_file()
{

	switch (bm_flag) {
	case BM_INTERFACE:	{
		bitmap_index bitmap;
		bitmap = bm_load_sub(arg);
		if ( N_interface_bitmaps < MAX_INTERFACE_BITMAPS )
			Interface_bitmaps[N_interface_bitmaps++] = bitmap;
		}
		break;
	case BM_ICONS:	{
		bitmap_index bitmap;
		bitmap = bm_load_sub(arg);
		if ( N_icon_bitmaps < MAX_ICON_BITMAPS )
			Icon_bitmaps[N_icon_bitmaps++] = bitmap;
		}
		break;
	case BM_GUYPICS:
	case BM_GIRLYPICS:
		bm_warning( "bitmap '%s' of $GUYPICS or $GIRLYPICS is skipped", arg );
		break;
	case BM_VCLIP:
		bm_read_vclip();
		break;
	case BM_ECLIP:
		bm_read_eclip();
		break;
	case BM_TEXTURES:			{
		bitmap_index bitmap;
		if ( !bm_texture_room(1) )
			break;
		bitmap = bm_load_sub(arg);
  		TmapList[tmap_count++] = texture_count;
		Textures[texture_count] = bitmap;
		set_texture_name( arg );
		texture_count++;
		NumTextures = texture_count;
		}
		break;
	case BM_WCLIP:
		bm_read_wclip();
		break;
	default:
		// ASCENT: a word outside a bitmap section. Descent 1 ignored it too.
		bm_warning( "unknown word '%s'", arg );
		break;
	}
}

// ----------------------------------------------------------------------------
// ASCENT: the code below is new: the object classes of Descent to
// Undermountain. A line of such a class is a list of words: "key=value",
// numbers that continue the value before, flag words, and bitmap names.

#define MAX_LINE_TOKENS		1200

typedef struct tbl_token {
	char	*key;		// the word, or the part before '='
	char	*val;		// the part after '=', or NULL for a word without '='
} tbl_token;

static tbl_token Tokens[MAX_LINE_TOKENS];
static int N_tokens;

// Keep the rest of the current line as a record. Call it before the
// tokens are made (strtok changes the text). Returns the record number.
static int bm_add_record( char * section, int id )
{
	tbl_record *r;
	char *rest, *p;

	rest = strtok( NULL, "" );		// the rest of the line
	if ( rest == NULL )
		rest = "";
	while ( *rest == ' ' || *rest == '\t' ) rest++;

	if ( N_tbl_records >= MAX_TBL_RECORDS ) {
		bm_warning( "too many table records" );
		N_tokens = 0;
		return -1;
	}
	r = &Tbl_records[N_tbl_records];
	strncpy( r->section, section, 11 );
	r->section[11] = 0;
	r->linenum = linenum;
	r->id = id;
	r->text = (char *) malloc( strlen(rest)+1 );
	if ( r->text ) {
		strcpy( r->text, rest );
		for (p=r->text; *p; p++ )
			if ( *p == QUOTED_SPACE ) *p = ' ';
	}

	// Make the tokens from the line itself.
	N_tokens = 0;
	p = rest;
	while ( *p ) {
		char *start, *eq;

		while ( *p == ' ' || *p == '\t' ) p++;
		if ( !*p ) break;
		start = p;
		while ( *p && *p != ' ' && *p != '\t' ) p++;
		if ( *p ) *p++ = 0;
		if ( N_tokens >= MAX_LINE_TOKENS ) {
			bm_warning( "too many words on the line" );
			break;
		}
		eq = strchr( start, '=' );
		if ( eq ) {
			*eq = 0;
			Tokens[N_tokens].val = eq+1;
		} else
			Tokens[N_tokens].val = NULL;
		Tokens[N_tokens].key = start;
		N_tokens++;
	}

	return N_tbl_records++;
}

static int tok_is_number( char * s )
{
	if ( *s == '-' || *s == '+' ) s++;
	if ( !*s ) return 0;
	for (; *s; s++ )
		if ( !((*s >= '0' && *s <= '9') || *s == '.') )
			return 0;
	return 1;
}

// A quoted name: remove the quotes, put the spaces back.
static void tok_get_name( char * val, char * dest, int size )
{
	int n = 0;

	if ( val == NULL ) val = "";
	for (; *val && n < size-1; val++ ) {
		if ( *val == '"' ) continue;
		dest[n++] = (*val == QUOTED_SPACE) ? ' ' : *val;
	}
	dest[n] = 0;
}

// Read up to count numbers of a list: the value of token i and the number
// words that follow it. Returns how many it read.
static int tok_get_fix_list( int i, fix * dest, int count )
{
	int n = 0;

	if ( Tokens[i].val && Tokens[i].val[0] )
		dest[n++] = fl2f(atof(Tokens[i].val));
	for (i++; i<N_tokens && n<count; i++ ) {
		if ( Tokens[i].val != NULL || !tok_is_number(Tokens[i].key) )
			break;
		dest[n++] = fl2f(atof(Tokens[i].key));
	}
	return n;
}

static int tok_get_short_list( int i, short * dest, int count )
{
	int n = 0;

	if ( Tokens[i].val && Tokens[i].val[0] )
		dest[n++] = atoi(Tokens[i].val);
	for (i++; i<N_tokens && n<count; i++ ) {
		if ( Tokens[i].val != NULL || !tok_is_number(Tokens[i].key) )
			break;
		dest[n++] = atoi(Tokens[i].key);
	}
	return n;
}

// 1 if the word without '=' is a bitmap of a model: an animating bitmap
// (%N), a file name, or a bitmap name of the archives without extension
// (six $MONSTER lines have "g_a01").
static int tok_is_bitmap( char * word )
{
	if ( word[0] == '%' )
		return 1;
	if ( word[0] == '#' || tok_is_number(word) )
		return 0;
	if ( strchr( word, '.' ) )
		return 1;
	if ( strlen(word) <= 8 && piggy_find_bitmap( word ).index != 0 )
		return 1;
	return 0;
}

// 1 if the word is an animating bitmap (%N) or a file name.
static int tok_is_file( char * word )
{
	if ( word[0] == '%' )
		return 1;
	if ( word[0] == '#' || tok_is_number(word) )
		return 0;
	return strchr( word, '.' ) != NULL;
}

// The value of "picture=" on the line gets its slot of the object bitmaps
// before the models of the line get theirs. Returns the slot, or -1.
static int line_picture_slot()
{
	int i, slot;

	for (i=0; i<N_tokens; i++ )
		if ( Tokens[i].val != NULL && !stricmp( Tokens[i].key, "picture" ) ) {
			slot = N_ObjBitmapPtrs;
			if ( load_polymodel_bitmap( Tokens[i].val ) == NULL && Tokens[i].val[0] != '%' )
				return -1;
			return slot;
		}
	return -1;
}

// The models of one line: each "model=" (or "pof=", "weapon_pof=",
// "embedded_model=") starts a model. The bitmap words up to the next one
// are its textures.
#define MAX_LINE_MODELS		MAX_PLAYER_MODELS

static int Line_n_models;
static char *Line_model_name[MAX_LINE_MODELS];
static int Line_model_first[MAX_LINE_MODELS+1];
static int Line_model_was_bound[MAX_LINE_MODELS];	// an earlier line gave the model its slots

static void line_models_start()
{
	Line_n_models = 0;
}

static void line_model_add( char * name )
{
	if ( Line_n_models >= MAX_LINE_MODELS ) {
		bm_warning( "too many models on the line" );
		return;
	}
	Line_model_name[Line_n_models] = name;
	Line_model_first[Line_n_models] = N_ObjBitmapPtrs;
	Line_n_models++;
}

// Load the models of the line. model_nums gets Line_n_models numbers (-1
// for a model that is not in the archives).
static void line_models_load( int * model_nums, robot_info * r )
{
	int i;

	Line_model_first[Line_n_models] = N_ObjBitmapPtrs;
	for (i=0; i<Line_n_models; i++ ) {
		int n_textures = Line_model_first[i+1] - Line_model_first[i];
		int mn = polyobj_find_model( Line_model_name[i] );

		Line_model_was_bound[i] = mn >= 0 && Polygon_models[mn].dtu_bound;
		model_nums[i] = load_polygon_model( Line_model_name[i], n_textures, Line_model_first[i], (i==0)?r:NULL );
		if ( model_nums[i] < 0 )
			bm_warning( "model '%s' is not in the EGG archive", Line_model_name[i] );
	}
}

// ASCENT: D-054. The texture slots of a $WEAPON or $SPELL model. The
// archive loader of 1.3 points po->textures at the first_texture of the
// EGG header, plus 1 for a model of the level archive (0x70EED to
// 0x70F09; 0x70B89 for the player archive), and the readers of $WEAPON
// (0x2252F) and $SPELL (0x25591) pass their slots to load_polygon_model
// but do not store them after it, so the header value stays. The slots of
// the running table parse differ from the header, and in MINES1, PRISON1
// and TRAIN1 some arrows, bolts and spell models get bitmap 0 (docs/re/
// models.md, "Textures"). Corrected (1, default): the model draws with the
// slots of its table line, as the creature and scenery models do in 1.3.
// -origobj clears it: the header rule. A model that a $MONSTER or
// $DRESSING line names later still gets the slots of that line, as in 1.3.
int Dtu_fix_model_slots = 1;

static void line_models_header_slots( int * model_nums )
{
	static int done = 0;
	int i, first_level_model = 0;

	if ( !done ) {
		done = 1;
		if ( FindArg( "-origobj" ) )
			Dtu_fix_model_slots = 0;
	}
	if ( Dtu_fix_model_slots )
		return;

	piggy_get_egg_counts( piggy_get_num_eggs() - 1, NULL, NULL, NULL, NULL, NULL, &first_level_model );
	for (i=0; i<Line_n_models; i++ ) {
		polymodel *pm;
		int first;

		// a model that a $MONSTER or $DRESSING line bound before keeps its
		// slots (1.3 stores them there)
		if ( model_nums[i] < 0 || Line_model_was_bound[i] )
			continue;
		pm = &Polygon_models[model_nums[i]];
		first = pm->dtu_first_texture_file + (model_nums[i] >= first_level_model ? 1 : 0);
		if ( first < 0 || first + pm->dtu_n_textures_file > MAX_OBJ_BITMAPS || pm->dtu_n_textures_file >= MAX_POLYOBJ_TEXTURES )
			continue;
		pm->first_texture = first;
		pm->n_textures = pm->dtu_n_textures_file;
		pm->dtu_bound = 0;		// a later $MONSTER or $DRESSING line stores its slots
	}
}

// A word without '=' that is not a flag of the class: a texture of the
// current model, or an unknown word.
static void line_bare_word( char * word, int lighted )
{
	if ( tok_is_bitmap( word ) ) {
		grs_bitmap *bm = load_polymodel_bitmap( word );
		if ( bm && !lighted )
			bm->bm_flags |= BM_FLAG_NO_LIGHTING;
		if ( Line_n_models == 0 )
			bm_warning( "bitmap '%s' stands before the model of the line", word );
	} else if ( !tok_is_number(word) && word[0] != '#' )
		bm_warning( "unknown word '%s'", word );
}

static void add_object_type( int type, int id )
{
	if ( Num_total_object_types < MAX_OBJTYPE ) {
		ObjType[Num_total_object_types] = type;
		ObjId[Num_total_object_types] = id;
		ObjStrength[Num_total_object_types] = 0;
		Num_total_object_types++;
	}
}

static int word_in_list( char * word, char ** list )
{
	int i;
	for (i=0; list[i]; i++ )
		if ( !stricmp( word, list[i] ) )
			return i;
	return -1;
}

// ------------------------------------------------------------------------------
// $PLAYER: the body models of the player character and its movement.
static void bm_read_player()
{
	player_type_info *pt;
	int i, n, rec;
	int model_nums[MAX_LINE_MODELS];

	n = N_player_types;
	rec = bm_add_record( "$PLAYER", n );
	if ( n >= MAX_PLAYER_TYPES ) {
		bm_warning( "too many $PLAYER lines" );
		return;
	}
	N_player_types++;
	pt = &Player_type_info[n];
	memset( pt, 0, sizeof(*pt) );
	pt->record = rec;

	if ( n == 0 ) {
		Player_ship->mass = Player_ship->drag = 0;	//stupid defaults
		Player_ship->expl_vclip_num = -1;
	}

	line_models_start();

	for (i=0; i<N_tokens; i++ ) {
		char *key = Tokens[i].key, *val = Tokens[i].val;

		if ( val == NULL ) {
			line_bare_word( key, 1 );
			continue;
		}

		if (!stricmp( key, "model" ))					line_model_add( val );
		else if (!stricmp( key, "name" ))			tok_get_name( val, pt->name, DTU_NAME_LENGTH );
		else if (!stricmp( key, "picture" ))		pt->picture = bm_load_sub( val );
		else if (!stricmp( key, "eye_offset" ))	pt->eye_offset = fl2f(atof(val));
		else if (!stricmp( key, "gravity" ))		pt->gravity = fl2f(atof(val));
		else if (!stricmp( key, "state_times" ))	tok_get_fix_list( i, pt->state_times, DTU_N_STATE_TIMES );
		else if (!stricmp( key, "speeds" ))			tok_get_fix_list( i, pt->speeds, DTU_N_SPEEDS );
		else if (!stricmp( key, "fire_frames" ))	tok_get_short_list( i, pt->fire_frames, DTU_N_FIRE_FRAMES );
		else if ( n == 0 ) {
			// The first player type also fills the Descent 1 "player ship".
			if (!stricmp( key, "mass" ))						Player_ship->mass = fl2f(atof(val));
			else if (!stricmp( key, "drag" ))				Player_ship->drag = fl2f(atof(val));
			else if (!stricmp( key, "max_thrust" ))		Player_ship->max_thrust = fl2f(atof(val));
			else if (!stricmp( key, "max_rotthrust" ))	Player_ship->max_rotthrust = fl2f(atof(val));
		}
		// The other keywords (age, class, race, the ability scores, the
		// sounds) stay in the record.
	}

	line_models_load( model_nums, NULL );
	pt->n_models = Line_n_models;
	for (i=0; i<Line_n_models; i++ )
		pt->model_nums[i] = model_nums[i];

	if ( n == 0 && Line_n_models > 0 )
		Player_ship->model_num = model_nums[0];
}

static char *Monster_flag_names[] = {
	"undead", "humanoid", "flying", "no_scare", "no_sleep", "fire_immune",
	"electricity_immune", "weapon_resistant", "extraplanar", "blind",
	"light_sensitive", "dark_lover", "light_lover", "pass_thru", "noattack",
	"magic_resistant", "fire_resistant",	// ASCENT: so that the creature setup can set them (lich, 0xAC7B2)
	NULL };

// Flag words of $MONSTER that have no bit in monster_info.flags. They stay
// in the record (bm_tbl_has_flag).
static char *Monster_other_words[] = {
	"jumping_jack", "patient", "impatient", "pain_flee", "prison_boy", NULL };

// ------------------------------------------------------------------------------
// $MONSTER: a creature. It takes the place of the Descent 1 $ROBOT.
static void bm_read_monster()
{
	robot_info *ri;
	monster_info *mi;
	int i, n, g, s, rec, f;
	int model_nums[MAX_LINE_MODELS];

	n = N_robot_types;
	rec = bm_add_record( "$MONSTER", n );
	if ( n >= MAX_ROBOT_TYPES ) {
		bm_warning( "too many $MONSTER lines (%d)", MAX_ROBOT_TYPES );
		return;
	}
	N_robot_types++;

	ri = &Robot_info[n];
	mi = &Monster_info[n];
	memset( ri, 0, sizeof(*ri) );
	memset( mi, 0, sizeof(*mi) );
	mi->record = rec;
	mi->ai_record = -1;

	// The defaults of the Descent 1 bm_read_robot()
	ri->model_num = -1;
	ri->exp1_vclip_num = ri->exp2_vclip_num = -1;
	ri->exp1_sound_num = ri->exp2_sound_num = -1;
	ri->lighting = F1_0/2;
	ri->strength = F1_0*10;
	ri->mass = f1_0*4;
	ri->drag = f1_0/2;
	ri->score_value = 1000;
	ri->see_sound = ROBOT_SEE_SOUND_DEFAULT;
	ri->attack_sound = ROBOT_ATTACK_SOUND_DEFAULT;
	ri->claw_sound = ROBOT_CLAW_SOUND_DEFAULT;
	ri->contains_type = OBJ_POWERUP;
	mi->die_sound = mi->hit_sound = -1;
	// ASCENT: the defaults of the monster reader of DTUGAME.EXE (0x1DDE6,
	// docs/re/ai.md): thaco 5, max_hit_points 24, iq 70, str 10.
	mi->thaco = 5;
	mi->max_hit_points = 24;
	ri->strength = i2f(24);
	mi->iq = 70;
	mi->str = 10;

	line_models_start();

	for (i=0; i<N_tokens; i++ ) {
		char *key = Tokens[i].key, *val = Tokens[i].val;

		if ( val == NULL ) {
			if ( (f = word_in_list( key, Monster_flag_names )) >= 0 )
				mi->flags |= 1 << f;
			else if ( word_in_list( key, Monster_other_words ) >= 0 )
				;
			else
				line_bare_word( key, 1 );
			continue;
		}

		if (!stricmp( key, "model" ))					line_model_add( val );
		else if (!stricmp( key, "name" )) {
			tok_get_name( val, mi->name, DTU_NAME_LENGTH );
			strncpy( Robot_names[n], mi->name, ROBOT_NAME_LENGTH-1 );
		}
		else if (!stricmp( key, "picture" ))		mi->picture = bm_load_sub( val );
		else if (!stricmp( key, "lighting" ))		ri->lighting = fl2f(atof(val));
		else if (!stricmp( key, "weapon_type" ))	ri->weapon_type = atoi(val);
		else if (!stricmp( key, "mass" ))			ri->mass = fl2f(atof(val));
		else if (!stricmp( key, "drag" ))			ri->drag = fl2f(atof(val));
		else if (!stricmp( key, "score_value" ))	ri->score_value = atoi(val);
		else if (!stricmp( key, "see_sound" ))		ri->see_sound = atoi(val);
		else if (!stricmp( key, "attack_sound" ))	ri->attack_sound = atoi(val);
		else if (!stricmp( key, "claw_sound" ))	ri->claw_sound = atoi(val);
		// Descent 1: attack_type 0 fires, 1 attacks with claws.
		else if (!stricmp( key, "attack_type" ))	ri->attack_type = !stricmp( val, "melee" ) ? 1 : atoi(val);
		else if (!stricmp( key, "bossiness" ))		ri->boss_flag = atoi(val);
		else if (!stricmp( key, "max_hit_points" )) {
			mi->max_hit_points = atoi(val);
			ri->strength = i2f(mi->max_hit_points);		// Guess: the hit points are the Descent 1 "strength"
		}
		else if (!stricmp( key, "thaco" ) || !stricmp( key, "thac0" ))			mi->thaco = atoi(val);
		else if (!stricmp( key, "armor_class" ))	mi->armor_class = atoi(val);
		else if (!stricmp( key, "str" ))				mi->str = atoi(val);
		else if (!stricmp( key, "iq" ))				mi->iq = atoi(val);
		else if (!stricmp( key, "moola" ))			mi->moola = atoi(val);
		else if (!stricmp( key, "die_sound" ))		mi->die_sound = atoi(val);
		else if (!stricmp( key, "hit_sound" ))		mi->hit_sound = atoi(val);
		else if (!stricmp( key, "gravity" ))		mi->gravity = fl2f(atof(val));
		else if (!stricmp( key, "pass_thru" ))		{ if (atoi(val)) mi->flags |= MONSTER_F_PASS_THRU; }
		// ASCENT: the monster reader 0x1DDE6 compares the keyword with "rat"
		// (0x1E9E2, string 0x1218EE) and sets bit 4 of the flags that go to
		// Robot_info+0xD9 (0x1E9F5), whatever the value. The squash of
		// collide_robot_and_player (0x26FE0) tests it.
		else if (!stricmp( key, "rat" ))				mi->flags |= MONSTER_F_RAT;
		else if (!stricmp( key, "state_times" ))	tok_get_fix_list( i, mi->state_times, DTU_N_STATE_TIMES );
		else if (!stricmp( key, "speeds" ))			tok_get_fix_list( i, mi->speeds, DTU_N_SPEEDS );
		else if (!stricmp( key, "fire_frames" ))	tok_get_short_list( i, mi->fire_frames, DTU_N_FIRE_FRAMES );
		// The other keywords stay in the record.
	}

	//clear out anim info
	for (g=0;g<MAX_GUNS+1;g++)
		for (s=0;s<N_ANIM_STATES;s++)
			ri->anim_states[g][s].n_joints = 0;	//inialize to zero

	line_models_load( model_nums, ri );
	if ( Line_n_models > 0 )
		ri->model_num = model_nums[0];
	else
		bm_warning( "$MONSTER line without a model" );

	add_object_type( OL_ROBOT, n );
}

// ------------------------------------------------------------------------------
// $MONSTER_AI: 31 numbers. DTUGAME.EXE (0x1DC02) reads the first one and
// does not use it, then three groups of 5 fixed point numbers, one group
// of 5 bytes and two groups of 5 16-bit numbers. The line belongs to the
// $MONSTER line before it (a counter, as Num_robot_ais in Descent 1).
static void bm_read_monster_ai()
{
	robot_info *ri;
	monster_info *mi;
	fix values[31];
	int n, i, count, rec;

	n = Num_robot_ais;
	rec = bm_add_record( "$MONSTER_AI", n );
	if ( n >= MAX_ROBOT_TYPES ) {
		bm_warning( "too many $MONSTER_AI lines" );
		return;
	}
	Num_robot_ais++;
	ri = &Robot_info[n];
	mi = &Monster_info[n];
	mi->ai_record = rec;

	count = 0;
	for (i=0; i<N_tokens && count<31; i++ )
		if ( Tokens[i].val == NULL && tok_is_number(Tokens[i].key) )
			values[count++] = fl2f(atof(Tokens[i].key));
	if ( count != 31 ) {
		bm_warning( "$MONSTER_AI line with %d numbers, 31 expected", count );
		for (; count<31; count++ ) values[count] = 0;
	}

	mi->ai_first_number = f2i(values[0]);
	for (i=0; i<NDL; i++ ) {
		ri->field_of_view[i] = values[1+i];
		ri->firing_wait[i] = values[6+i];
		// Guess: the third group is turn_time. It is at the place that
		// turn_time has in Descent 1 after the two groups before it.
		ri->turn_time[i] = values[11+i];
		ri->rapidfire_count[i] = f2i(values[16+i]);
		mi->ai_values[0][i] = f2i(values[21+i]);
		mi->ai_values[1][i] = f2i(values[26+i]);
	}
	// Descent 1 also read fire_power, shield, max_speed, circle_distance
	// and evade_speed here. The game tables do not have them. They are 0.

	ri->always_0xabcd	= 0xabcd;

	adjust_field_of_view(ri->field_of_view);
}

// ------------------------------------------------------------------------------
// $DRESSING: scenery. An entry has a model (pof=), a vclip (vclip=) or one
// bitmap (bitmap=).
static void bm_read_dressing()
{
	dressing_info *di;
	int i, n, rec;
	int model_nums[MAX_LINE_MODELS];

	n = N_dressing_types;
	rec = bm_add_record( "$DRESSING", n );
	if ( n >= MAX_DRESSING_TYPES ) {
		bm_warning( "too many $DRESSING lines (%d)", MAX_DRESSING_TYPES );
		return;
	}
	N_dressing_types++;
	di = &Dressing_info[n];
	memset( di, 0, sizeof(*di) );
	di->record = rec;
	di->model_num = -1;
	di->vclip_num = -1;
	di->obj_bitmap = -1;
	di->explode_sound = -1;
	di->break_vclip = -1;

	line_models_start();

	for (i=0; i<N_tokens; i++ ) {
		char *key = Tokens[i].key, *val = Tokens[i].val;

		if ( val == NULL ) {
			if (!stricmp( key, "magic" ))					di->flags |= DRESSING_F_MAGIC;
			else if (!stricmp( key, "cant_break" ))	di->flags |= DRESSING_F_CANT_BREAK;
			else if (!stricmp( key, "pass_thru" ))		di->pass_thru = 1;
			else if (!stricmp( key, "pushable" ))		di->pushable = 1;
			else line_bare_word( key, 1 );
			continue;
		}

		if (!stricmp( key, "pof" ))					line_model_add( val );
		else if (!stricmp( key, "name" ))			tok_get_name( val, di->name, DTU_NAME_LENGTH );
		else if (!stricmp( key, "vclip" ))			di->vclip_num = atoi(val);
		else if (!stricmp( key, "bitmap" )) {
			// The bitmap gets a slot of the object bitmaps, so that it can be
			// an animating one ("bitmap=%42").
			di->obj_bitmap = N_ObjBitmapPtrs;
			load_polymodel_bitmap( val );
		}
		else if (!stricmp( key, "size" ))			di->size = fl2f(atof(val));
		else if (!stricmp( key, "lightcast" ))		di->lightcast = fl2f(atof(val));
		else if (!stricmp( key, "explode_sound" ))	di->explode_sound = atoi(val);
		else if (!stricmp( key, "break_vclip" ))	di->break_vclip = atoi(val);
		else if (!stricmp( key, "pushable" ))		di->pushable = atoi(val);
		else if (!stricmp( key, "pass_thru" ))		di->pass_thru = atoi(val);
		else bm_warning( "unknown $DRESSING keyword '%s'", key );
	}

	line_models_load( model_nums, NULL );
	if ( Line_n_models > 0 )
		di->model_num = model_nums[0];

	add_object_type( OL_DRESSING, n );
}

// ------------------------------------------------------------------------------
// $OBJECT, $ARMOR, $SPELL: items. Only the values of item_info are
// decoded. All keywords stay in the record.
static void bm_read_item(int kind)
{
	item_info *it;
	int i, n, rec, max, first = 0;
	char *section;
	int model_nums[MAX_LINE_MODELS];

	if ( kind == OL_MISC )			{ section = "$OBJECT"; n = N_misc_types; max = MAX_MISC_TYPES; it = Misc_info; }
	else if ( kind == OL_ARMOR )	{ section = "$ARMOR"; n = N_armor_types; max = MAX_ARMOR_TYPES; it = Armor_info; }
	else									{ section = "$SPELL"; n = N_spell_types; max = MAX_SPELL_TYPES; it = Spell_info; }

	rec = bm_add_record( section, n );

	// "$SPELL 4 name=...": the number after $SPELL is the spell number.
	// The lines of a table are in the order of their numbers, with gaps.
	if ( kind == OL_SPELL && N_tokens > 0 && Tokens[0].val == NULL && tok_is_number(Tokens[0].key) ) {
		n = atoi( Tokens[0].key );
		first = 1;
		if ( rec >= 0 ) Tbl_records[rec].id = n;
	}

	if ( n < 0 || n >= max ) {
		bm_warning( "too many %s lines (%d)", section, max );
		return;
	}
	if ( kind == OL_MISC )			N_misc_types = n+1;
	else if ( kind == OL_ARMOR )	N_armor_types = n+1;
	else if ( n >= N_spell_types )	N_spell_types = n+1;

	it += n;
	memset( it, 0, sizeof(*it) );
	it->record = rec;
	it->obj_bitmap = -1;
	it->icon = -1;
	it->inv_icon = -1;
	it->model_num = -1;
	it->vclip_num = -1;

	it->obj_bitmap = line_picture_slot();

	line_models_start();

	for (i=first; i<N_tokens; i++ ) {
		char *key = Tokens[i].key, *val = Tokens[i].val;

		if ( val == NULL ) {
			// A model texture (after weapon_pof=). The other words are flags
			// (all_use, stackable, magic, ...) and stay in the record.
			if ( Line_n_models > 0 && tok_is_file( key ) )
				load_polymodel_bitmap( key );
			continue;
		}

		if (!stricmp( key, "name" ))					tok_get_name( val, it->name, DTU_NAME_LENGTH );
		else if (!stricmp( key, "picture" ))		;		// done above
		else if (!stricmp( key, "icon" ))			it->icon = atoi(val);
		else if (!stricmp( key, "inv_icon" ))		it->inv_icon = atoi(val);
		else if (!stricmp( key, "value" ))			it->value = atoi(val);
		else if (!stricmp( key, "mass" ))			it->mass = fl2f(atof(val));
		else if (!stricmp( key, "weight" ))			it->mass = fl2f(atof(val));
		else if (!stricmp( key, "stack_limit" ))	it->stack_limit = atoi(val);
		else if (!stricmp( key, "weapon_pof" ))	line_model_add( val );
		else if (!stricmp( key, "weapon_vclip" ))	it->vclip_num = atoi(val);
	}

	line_models_load( model_nums, NULL );
	if ( kind == OL_SPELL )
		line_models_header_slots( model_nums );		// Dtu_fix_model_slots
	if ( Line_n_models > 0 )
		it->model_num = model_nums[0];

	add_object_type( kind, n );
}

// ------------------------------------------------------------------------------
// $WEAPON: the Descent 1 weapon with new keywords. The start values and
// the Descent 1 keywords are those of the Descent 1 bm_read_weapon().
static void bm_read_dtu_weapon()
{
	weapon_info *wi;
	item_info *it;
	int i, n, rec, k;
	int lighted;					//flag for whether is a texture is lighted
	int model_nums[MAX_LINE_MODELS];
	fix list[NDL];

	n = N_weapon_types;
	rec = bm_add_record( "$WEAPON", n );
	if ( n >= MAX_WEAPON_TYPES ) {
		bm_warning( "too many $WEAPON lines (%d)", MAX_WEAPON_TYPES );
		return;
	}
	N_weapon_types++;

	wi = &Weapon_info[n];
	it = &Weapon_item_info[n];
	memset( it, 0, sizeof(*it) );
	it->record = rec;
	it->obj_bitmap = -1;
	it->icon = -1;
	it->inv_icon = -1;
	it->model_num = -1;
	it->vclip_num = -1;

	// Initialize weapon array
	memset( wi, 0, sizeof(*wi) );
	wi->render_type = WEAPON_RENDER_NONE;		// 0=laser, 1=blob, 2=object
	wi->bitmap.index = 0;
	wi->model_num = -1;
	wi->model_num_inner = -1;
	wi->blob_size = 0x1000;									// size of blob
	wi->flash_vclip = -1;
	wi->flash_sound = SOUND_LASER_FIRED;
	wi->flash_size = 0;
	wi->robot_hit_vclip = -1;
	wi->robot_hit_sound = -1;
	wi->wall_hit_vclip = -1;
	wi->wall_hit_sound = -1;
	wi->impact_size = 0;
	for (i=0; i<NDL; i++) {
		wi->strength[i] = F1_0;
		wi->speed[i] = F1_0*10;
	}
	wi->mass = F1_0;
	wi->thrust = 0;
	wi->drag = 0;
	wi->persistent = 0;
	wi->energy_usage = 0;
	wi->ammo_usage = 0;
	wi->fire_wait = F1_0/4;
	wi->fire_count = 1;
	wi->damage_radius = 0;
	wi->destroyable = 1;
	wi->matter = 0;
	wi->bounce = 0;
	wi->lifetime = WEAPON_DEFAULT_LIFETIME;
	wi->po_len_to_width_ratio = F1_0*10;
	wi->picture.index = 0;
	wi->homing_flag = 0;

	lighted = 1;			//assume first texture is lighted

	it->obj_bitmap = line_picture_slot();

	line_models_start();

	for (i=0; i<N_tokens; i++ ) {
		char *key = Tokens[i].key, *val = Tokens[i].val;

		if ( val == NULL ) {
			// A model texture (%N or a file name). The other words are flags
			// (stackable, magic, fighter_use, ARROW, ...) and stay in the record.
			if ( Line_n_models > 0 && tok_is_file( key ) ) {
				grs_bitmap *bm = load_polymodel_bitmap( key );
				if ( bm && !lighted )
					bm->bm_flags |= BM_FLAG_NO_LIGHTING;
				lighted = 1;			//default for next bitmap is lighted
			}
			continue;
		}

		if (!stricmp( key, "weapon_vclip" ))	{
			// Set vclip to play for this weapon.
			wi->bitmap.index = 0;
			wi->render_type = WEAPON_RENDER_VCLIP;
			wi->weapon_vclip = atoi(val);
			it->vclip_num = wi->weapon_vclip;
		} else if (!stricmp( key, "weapon_pof" ) || !stricmp( key, "embedded_model" ))	{
			line_model_add( val );
		} else if (!stricmp( key, "picture" )) {
			// The picture is in Weapon_info as in Descent 1, and it has a
			// slot of the object bitmaps as the picture of each item has.
			if ( val[0] != '%' && it->obj_bitmap >= 0 )
				wi->picture = ObjBitmaps[ObjBitmapPtrs[it->obj_bitmap]];
		} else if (!stricmp( key, "name" ) || !stricmp( key, "weapon_name" )) {
			// 0x20AD7: weapon_name= and name= both write the name table
			// 0x1B45AC, the last one wins
			tok_get_name( val, it->name, DTU_NAME_LENGTH );
		} else if (!stricmp( key, "speed" )) {
			k = tok_get_fix_list( i, list, NDL );
			for (; k>0 && k<NDL; k++ ) list[k] = list[k-1];
			if ( k == NDL )
				for (k=0; k<NDL; k++ )
					wi->speed[k] = i2f(f2i(list[k]));		// whole numbers, as in Descent 1
		}
		else if (!stricmp( key, "mass" ))				{ wi->mass = fl2f(atof(val)); it->mass = wi->mass; }
		else if (!stricmp( key, "drag" ))				wi->drag = fl2f(atof(val));
		else if (!stricmp( key, "thrust" ))				wi->thrust = fl2f(atof(val));
		else if (!stricmp( key, "matter" ))				wi->matter = atoi(val);
		else if (!stricmp( key, "bounce" ))				wi->bounce = atoi(val);
		else if (!stricmp( key, "flash_vclip" ))		wi->flash_vclip = atoi(val);
		else if (!stricmp( key, "flash_sound" ))		wi->flash_sound = atoi(val);
		else if (!stricmp( key, "flash_size" ))		wi->flash_size = fl2f(atof(val));
		else if (!stricmp( key, "blob_size" ))			wi->blob_size = fl2f(atof(val));
		// The game tables say "monster" where Descent 1 said "robot". Three
		// spellings of monster_hit_vclip are in the tables.
		else if (!stricmp( key, "monster_hit_vclip" ) || !stricmp( key, "monster_hit_vcilp" ) ||
					!stricmp( key, "monster_hitvclip" ) || !stricmp( key, "monsster_hit_vclip" ) ||
					!stricmp( key, "robot_hit_vclip" ))		wi->robot_hit_vclip = atoi(val);
		else if (!stricmp( key, "monster_hit_sound" ) || !stricmp( key, "robot_hit_sound" ))	wi->robot_hit_sound = atoi(val);
		else if (!stricmp( key, "wall_hit_vclip" ))	wi->wall_hit_vclip = atoi(val);
		else if (!stricmp( key, "wall_hit_sound" ))	wi->wall_hit_sound = atoi(val);
		else if (!stricmp( key, "impact_size" ))		wi->impact_size = fl2f(atof(val));
		else if (!stricmp( key, "lighted" ))			lighted = atoi(val);
		else if (!stricmp( key, "lw_ratio" ))			wi->po_len_to_width_ratio = fl2f(atof(val));
		else if (!stricmp( key, "lightcast" ))			wi->light = fl2f(atof(val));
		else if (!stricmp( key, "persistent" ))		wi->persistent = atoi(val);
		else if (!stricmp( key, "ammo_usage" ))		wi->ammo_usage = atoi(val);
		else if (!stricmp( key, "fire_wait" ))			wi->fire_wait = fl2f(atof(val));
		else if (!stricmp( key, "fire_count" ))		wi->fire_count = atoi(val);
		else if (!stricmp( key, "damage_radius" ))	wi->damage_radius = fl2f(atof(val));
		else if (!stricmp( key, "lifetime" ))			wi->lifetime = fl2f(atof(val));
		else if (!stricmp( key, "destroyable" ))		wi->destroyable = atoi(val);
		else if (!stricmp( key, "homing" ))				wi->homing_flag = !!atoi(val);
		else if (!stricmp( key, "icon" ))				it->icon = atoi(val);
		else if (!stricmp( key, "inv_icon" ))			it->inv_icon = atoi(val);
		else if (!stricmp( key, "value" ))				it->value = atoi(val);
		else if (!stricmp( key, "stack_limit" ))		it->stack_limit = atoi(val);
		// The other keywords (damage dice, to_hit_bonus, ...) stay in the record.
	}

	line_models_load( model_nums, NULL );
	line_models_header_slots( model_nums );		// Dtu_fix_model_slots
	if ( Line_n_models > 0 && model_nums[0] >= 0 ) {
		wi->render_type = WEAPON_RENDER_POLYMODEL;
		wi->model_num = model_nums[0];
		it->model_num = model_nums[0];
		if ( Line_n_models > 1 )
			wi->model_num_inner = model_nums[1];		// embedded_model=. Guess: it takes the place of weapon_pof_inner.
	}

	add_object_type( OL_WEAPON, n );
}

// ------------------------------------------------------------------------------
// Compare n characters without case (strnicmp of the Watcom library).
static int bm_strnicmp( const char * a, const char * b, int n )
{
	int i, d;
	for (i=0; i<n; i++ ) {
		d = tolower((unsigned char)a[i]) - tolower((unsigned char)b[i]);
		if ( d || !a[i] ) return d;
	}
	return 0;
}

// Find "key=" in the text of a record.
static char *record_find_key( int record, char * key )
{
	char *p;
	int len, in_quote = 0;

	if ( record < 0 || record >= N_tbl_records || Tbl_records[record].text == NULL )
		return NULL;
	len = (int)strlen(key);
	p = Tbl_records[record].text;
	while ( *p ) {
		while ( *p == ' ' || *p == '\t' ) p++;
		if ( !*p ) break;
		if ( !bm_strnicmp( p, key, len ) && p[len] == '=' )
			return p+len+1;
		// skip the word, with a quoted part if it has one
		for (; *p; p++ ) {
			if ( *p == '"' ) in_quote = !in_quote;
			else if ( (*p == ' ' || *p == '\t') && !in_quote ) break;
		}
	}
	return NULL;
}

// ASCENT: the item line of a record for the rulebook values
// (dtuitemfix.h, the setting "Rulebook items"): the table and the number of
// the item, and its name. Only the lines that have a rule are tested.
static int record_item_rule( int record, int flag, char * key, const char ** value )
{
	int i;

	if ( !Dtu_rulebook_items || record < 0 )
		return -1;
	for (i=0; i<dtu_itemfix_n_rules(); i++ ) {
		const dtu_item_rule *r = dtu_itemfix_rule(i);
		item_info *it = NULL;

		switch ( r->kind ) {
			case DTU_IK_WEAPON:	if ( r->id < N_weapon_types ) it = &Weapon_item_info[r->id]; break;
			case DTU_IK_ARMOR:	if ( r->id < N_armor_types ) it = &Armor_info[r->id]; break;
			case DTU_IK_MISC:		if ( r->id < N_misc_types ) it = &Misc_info[r->id]; break;
		}
		if ( it == NULL || it->record != record )
			continue;
		if ( flag )
			return dtu_itemfix_flag( r->kind, r->id, it->name, key );
		*value = dtu_itemfix_value( r->kind, r->id, it->name, key );
		return *value != NULL;
	}
	return -1;
}

int bm_tbl_get( int record, char * key, char * value, int value_size )
{
	char *p, *q;
	int n = 0;
	const char *rule;

	if ( value_size > 0 ) value[0] = 0;
	// ASCENT: the rulebook value of the key (dtuitemfix.h)
	if ( record_item_rule( record, 0, key, &rule ) == 1 && value_size > 0 ) {
		strncpy( value, rule, value_size-1 );
		value[value_size-1] = 0;
		return 1;
	}
	p = record_find_key( record, key );
	if ( p == NULL )
		return 0;

	if ( *p == '"' ) {
		for (p++; *p && *p != '"' && n < value_size-1; p++ )
			value[n++] = *p;
		value[n] = 0;
		return 1;
	}

	for (; *p && *p != ' ' && *p != '\t' && n < value_size-1; p++ )
		value[n++] = *p;
	// The numbers that follow belong to the value ("speed=120 120 120").
	for (;;) {
		char word[40];
		int k = 0;

		q = p;
		while ( *q == ' ' || *q == '\t' ) q++;
		for (; q[k] && q[k] != ' ' && q[k] != '\t' && k < 39; k++ )
			word[k] = q[k];
		word[k] = 0;
		if ( !tok_is_number(word) || n + k + 2 > value_size )
			break;
		value[n++] = ' ';
		memcpy( value+n, word, k );
		n += k;
		p = q + k;
	}
	value[n] = 0;
	return 1;
}

int bm_tbl_flag_pos( int record, char * flag )
{
	char *p;
	int len, in_quote = 0, pos = 0, found = -1;

	if ( record < 0 || record >= N_tbl_records || Tbl_records[record].text == NULL )
		return -1;
	len = (int)strlen(flag);
	p = Tbl_records[record].text;
	while ( *p ) {
		while ( *p == ' ' || *p == '\t' ) p++;
		if ( !*p ) break;
		if ( !bm_strnicmp( p, flag, len ) && (p[len] == 0 || p[len] == ' ' || p[len] == '\t') )
			found = pos;
		for (; *p; p++ ) {
			if ( *p == '"' ) in_quote = !in_quote;
			else if ( (*p == ' ' || *p == '\t') && !in_quote ) break;
		}
		pos++;
	}
	return found;
}

int bm_tbl_has_flag( int record, char * flag )
{
	// ASCENT: the rulebook state of the word (dtuitemfix.h)
	int rule = record_item_rule( record, 1, flag, NULL );

	if ( rule >= 0 )
		return rule;
	return bm_tbl_flag_pos( record, flag ) >= 0;
}
