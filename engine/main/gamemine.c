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
 * $Source: f:/miner/source/main/rcs/gamemine.c $
 * $Revision: 2.2 $
 * $Author: john $
 * $Date: 1995/03/06 15:23:14 $
 * 
 * Functions for loading mines in the game
 * 
 * $Log: gamemine.c $
 * Revision 2.2  1995/03/06  15:23:14  john
 * New screen techniques.
 * 
 * Revision 2.1  1995/02/27  13:13:37  john
 * Removed floating point.
 * 
 * Revision 2.0  1995/02/27  11:27:45  john
 * New version 2.0, which has no anonymous unions, builds with
 * Watcom 10.0, and doesn't require parsing BITMAPS.TBL.
 * 
 * Revision 1.70  1995/02/13  20:35:09  john
 * Lintized
 * 
 * Revision 1.69  1995/02/07  17:12:03  rob
 * Added ifdef's for Editor.
 * 
 * Revision 1.68  1995/02/07  16:51:48  mike
 * fix gray rock josh problem.
 * 
 * Revision 1.67  1995/02/01  15:46:26  yuan
 * Fixed matcen_nums.
 * 
 * Revision 1.66  1995/01/19  15:19:28  mike
 * new super-compressed registered file format.
 * 
 * Revision 1.65  1994/12/10  16:44:59  matt
 * Added debugging code to track down door that turns into rock
 * 
 * Revision 1.64  1994/12/10  14:58:24  yuan
 * *** empty log message ***
 * 
 * Revision 1.63  1994/12/08  17:19:10  yuan
 * Cfiling stuff.
 * 
 * Revision 1.62  1994/12/07  14:05:33  yuan
 * Fixed wall assert problem... Bashed highest_segment
 * _index before WALL_IS_DOORWAY check.
 * 
 * Revision 1.61  1994/11/27  23:14:52  matt
 * Made changes for new mprintf calling convention
 * 
 * Revision 1.60  1994/11/27  18:05:20  matt
 * Compile out LVL reader when editor compiled out
 * 
 * Revision 1.59  1994/11/26  22:51:45  matt
 * Removed editor-only fields from segment structure when editor is compiled
 * out, and padded segment structure to even multiple of 4 bytes.
 * 
 * Revision 1.58  1994/11/26  21:48:02  matt
 * Fixed saturation in short light value 
 * 
 * Revision 1.57  1994/11/20  22:11:49  mike
 * comment out an apparently unnecessary call to fuelcen_reset().
 * 
 * Revision 1.56  1994/11/18  21:56:42  john
 * Added a better, leaner pig format.
 * 
 * Revision 1.55  1994/11/17  20:09:18  john
 * Added new compiled level format.
 * 
 * Revision 1.54  1994/11/17  15:40:17  mike
 * Comment out mprintf which was causing important information to scroll away.
 * 
 * Revision 1.53  1994/11/17  14:56:37  mike
 * moved segment validation functions from editor to main.
 * 
 * Revision 1.52  1994/11/17  11:39:35  matt
 * Ripped out code to load old mines
 * 
 * Revision 1.51  1994/11/14  20:47:53  john
 * Attempted to strip out all the code in the game 
 * directory that uses any ui code.
 * 
 * Revision 1.50  1994/11/14  16:05:38  matt
 * Fixed, maybe, again, errors when can't find texture during remap
 * 
 * Revision 1.49  1994/11/14  14:34:03  matt
 * Fixed up handling when textures can't be found during remap
 * 
 * Revision 1.48  1994/11/14  13:01:55  matt
 * Added Int3() when can't find texture
 * 
 * Revision 1.47  1994/10/30  14:12:21  mike
 * rip out local segments stuff.
 * 
 * Revision 1.46  1994/10/27  19:43:07  john
 * Disable the piglet option.
 * 
 * Revision 1.45  1994/10/27  18:51:42  john
 * Added -piglet option that only loads needed textures for a 
 * mine.  Only saved ~1MB, and code still doesn't free textures
 * before you load a new mine.
 * 
 * Revision 1.44  1994/10/20  12:47:22  matt
 * Replace old save files (MIN/SAV/HOT) with new LVL files
 * 
 * Revision 1.43  1994/10/19  16:46:40  matt
 * Made tmap overrides for robots remap texture numbers
 * 
 * Revision 1.42  1994/10/03  23:37:01  mike
 * Adapt to changed fuelcen_activate parameters.
 * 
 * Revision 1.41  1994/09/23  22:14:49  matt
 * Took out obsolete structure fields
 * 
 * Revision 1.40  1994/08/01  11:04:11  yuan
 * New materialization centers.
 * 
 * Revision 1.39  1994/07/21  19:01:47  mike
 * Call Lsegment stuff.
 * 
 * 
 */

#pragma off (unreferenced)
static char rcsid[] = "$Id: gamemine.c 2.2 1995/03/06 15:23:14 john Exp $";
#pragma on (unreferenced)

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <ctype.h>		// ASCENT: tolower()
#include <stdint.h>		// ASCENT: int64_t

#include "mono.h"

#include "inferno.h"
#include "segment.h"
#include "textures.h"
#include "wall.h"
#include "object.h"
#include "gamemine.h"
#include "error.h"
#include "gameseg.h"
#include "switch.h"

#include "game.h"
#include "newmenu.h"

#ifdef EDITOR
#include "editor\editor.h"
#endif

#include "cfile.h"		
#include "fuelcen.h"

#include "hash.h"
#include "key.h"
#include "piggy.h"
#include "gamesave.h"	// ASCENT: OBJECT_DISK_SIZE
#include "dataio.h"		// ASCENT: little endian field reads and the record sizes

#define REMOVE_EXT(s)  (*(strchr( (s), '.' ))='\0')

struct mtfi mine_top_fileinfo;    // Should be same as first two fields below...
struct mfi mine_fileinfo;
struct mh mine_header;
struct me mine_editor;

int CreateDefaultNewSegment();

// ASCENT: the editor mine format loader (load_mine_data) was built only with
// EDITOR. The levels of the target game use that format, so it is always
// built now. The caller selects the format: load_mine_data() reads the editor
// format, load_mine_data_compiled() reads the compiled (RDL) format. See
// Gamesave_level_format in gamesave.c for load_level().
//
// The file structures (mine_top_fileinfo, mine_fileinfo, mine_header,
// mine_editor) and the records are read one field at a time, little endian.
// The sizes on disk are the constants below and in dataio.h.

int load_mine_data_compiled_new(CFILE *LoadFile);

// Reads the bytes of a file structure. "size_in_file" is the size that the
// file gives for the structure, "max" is the size that this program knows.
// Returns the number of bytes that are valid in buf. The original read
// size_in_file bytes over the structure, so a file with a smaller (older)
// structure left the defaults in the fields that it did not have.
static int mine_read_struct(ubyte *buf, int size_in_file, int max, CFILE *LoadFile)
{
	int n = size_in_file;

	if (n > max) n = max;
	if (n < 0) n = 0;
	memset(buf, 0, max);
	if (n > 0)
		if (cfread( buf, n, 1, LoadFile )!=1)
			return -1;
	return n;
}

// Reads the next int of the buffer into "field" when the buffer has it.
#define MINE_GET_INT(field) do { if ((p - buf) + 4 <= n) (field) = dio_get_int(&p); else p += 4; } while (0)

// Reads one vertex record of "size_in_file" bytes. The fields that the
// record does not have keep their values.
static int mine_read_vertex(vms_vector *v, int size_in_file, CFILE *LoadFile)
{
	ubyte buf[VERTEX_DISK_SIZE];
	const ubyte *p = buf;
	int n;

	n = mine_read_struct(buf, size_in_file, VERTEX_DISK_SIZE, LoadFile);
	if (n < 0)
		return 0;
	MINE_GET_INT(v->x);
	MINE_GET_INT(v->y);
	MINE_GET_INT(v->z);
	if (size_in_file > VERTEX_DISK_SIZE)		//skip the part that we do not know
		cfseek( LoadFile, size_in_file - VERTEX_DISK_SIZE, SEEK_CUR );
	return 1;
}

static char old_tmap_list[MAX_TEXTURES][13];
short tmap_xlate_table[MAX_TEXTURES];
static short tmap_times_used[MAX_TEXTURES];

// -----------------------------------------------------------------------------
//loads from an already-open file
// returns 0=everything ok, 1=old version, -1=error
int load_mine_data(CFILE *LoadFile)
{
	int   i, j;
	short tmap_xlate;
	int 	translate;
	char 	*temptr;
	int	mine_start = cftell(LoadFile);

	fuelcen_reset();

	for (i=0; i<MAX_TEXTURES; i++ )
		tmap_times_used[i] = 0;
	
	#ifdef EDITOR
	// Create a new mine to initialize things.
	//texpage_goto_first();
	create_new_mine();
	#endif

	//===================== READ FILE INFO ========================

	// These are the default values... version and fileinfo_sizeof
	// don't have defaults.
	mine_fileinfo.header_offset     =   -1;
	// ASCENT: the sizes on disk, not sizeof() of the structures in memory
	mine_fileinfo.header_size       =   MINE_HEADER_DISK_SIZE;
	mine_fileinfo.editor_offset     =   -1;
	mine_fileinfo.editor_size       =   MINE_EDITOR_DISK_SIZE;
	mine_fileinfo.vertex_offset     =   -1;
	mine_fileinfo.vertex_howmany    =   0;
	mine_fileinfo.vertex_sizeof     =   VERTEX_DISK_SIZE;
	mine_fileinfo.segment_offset    =   -1;
	mine_fileinfo.segment_howmany   =   0;
	mine_fileinfo.segment_sizeof    =   SEGMENT_EDITOR_DISK_SIZE;
	mine_fileinfo.newseg_verts_offset     =   -1;
	mine_fileinfo.newseg_verts_howmany    =   0;
	mine_fileinfo.newseg_verts_sizeof     =   VERTEX_DISK_SIZE;
	mine_fileinfo.group_offset		  =	-1;
	mine_fileinfo.group_howmany	  =	0;
	mine_fileinfo.group_sizeof		  =	GROUP_DISK_SIZE;
	mine_fileinfo.texture_offset    =   -1;
	mine_fileinfo.texture_howmany   =   0;
	mine_fileinfo.texture_sizeof    =   13;  // num characters in a name
 	mine_fileinfo.walls_offset		  =	-1;
	mine_fileinfo.walls_howmany	  =	0;
	mine_fileinfo.walls_sizeof		  =	WALL_DISK_SIZE;
 	mine_fileinfo.triggers_offset	  =	-1;
	mine_fileinfo.triggers_howmany  =	0;
	mine_fileinfo.triggers_sizeof	  =	TRIGGER_DISK_SIZE;
	mine_fileinfo.object_offset		=	-1;
	mine_fileinfo.object_howmany		=	1;
	mine_fileinfo.object_sizeof		=	OBJECT_DISK_SIZE;

	// Read in mine_top_fileinfo to get size of saved fileinfo.

	memset( &mine_top_fileinfo, 0, sizeof(mine_top_fileinfo) );

	if (cfseek( LoadFile, mine_start, SEEK_SET ))
		Error( "Error moving to top of file in gamemine.c" );

	// ASCENT: field reads of the 8 bytes of mine_top_fileinfo
	{
		ubyte buf[MINE_TOP_FILEINFO_DISK_SIZE];
		const ubyte *p = buf;

		if (cfread( buf, MINE_TOP_FILEINFO_DISK_SIZE, 1, LoadFile )!=1)
			Error( "Error reading mine_top_fileinfo in gamemine.c" );
		mine_top_fileinfo.fileinfo_signature = dio_get_ushort(&p);
		mine_top_fileinfo.fileinfo_version = dio_get_ushort(&p);
		mine_top_fileinfo.fileinfo_sizeof = dio_get_int(&p);
	}

	if (mine_top_fileinfo.fileinfo_signature != 0x2884)
		return -1;

	// Check version number
	if (mine_top_fileinfo.fileinfo_version < COMPATIBLE_VERSION )
		return -1;

	// Now, Read in the fileinfo
	if (cfseek( LoadFile, mine_start, SEEK_SET ))
		Error( "Error seeking to top of file in gamemine.c" );

	// ASCENT: field reads of mine_fileinfo. The file gives the size of its
	// structure (fileinfo_sizeof). A larger size than MINE_FILEINFO_DISK_SIZE
	// went over the end of the structure in the original. The rest is
	// ignored now.
	{
		ubyte buf[MINE_FILEINFO_DISK_SIZE];
		const ubyte *p = buf;
		int n;

		n = mine_read_struct(buf, mine_top_fileinfo.fileinfo_sizeof, MINE_FILEINFO_DISK_SIZE, LoadFile);
		if (n <= 0)
			Error( "Error reading mine_fileinfo in gamemine.c" );

		if (n >= 8) {
			mine_fileinfo.fileinfo_signature = dio_get_ushort(&p);
			mine_fileinfo.fileinfo_version = dio_get_ushort(&p);
			mine_fileinfo.fileinfo_sizeof = dio_get_int(&p);
		} else
			p += 8;
		MINE_GET_INT(mine_fileinfo.header_offset);
		MINE_GET_INT(mine_fileinfo.header_size);
		MINE_GET_INT(mine_fileinfo.editor_offset);
		MINE_GET_INT(mine_fileinfo.editor_size);
		MINE_GET_INT(mine_fileinfo.segment_offset);
		MINE_GET_INT(mine_fileinfo.segment_howmany);
		MINE_GET_INT(mine_fileinfo.segment_sizeof);
		MINE_GET_INT(mine_fileinfo.newseg_verts_offset);
		MINE_GET_INT(mine_fileinfo.newseg_verts_howmany);
		MINE_GET_INT(mine_fileinfo.newseg_verts_sizeof);
		MINE_GET_INT(mine_fileinfo.group_offset);
		MINE_GET_INT(mine_fileinfo.group_howmany);
		MINE_GET_INT(mine_fileinfo.group_sizeof);
		MINE_GET_INT(mine_fileinfo.vertex_offset);
		MINE_GET_INT(mine_fileinfo.vertex_howmany);
		MINE_GET_INT(mine_fileinfo.vertex_sizeof);
		MINE_GET_INT(mine_fileinfo.texture_offset);
		MINE_GET_INT(mine_fileinfo.texture_howmany);
		MINE_GET_INT(mine_fileinfo.texture_sizeof);
		MINE_GET_INT(mine_fileinfo.walls_offset);
		MINE_GET_INT(mine_fileinfo.walls_howmany);
		MINE_GET_INT(mine_fileinfo.walls_sizeof);
		MINE_GET_INT(mine_fileinfo.triggers_offset);
		MINE_GET_INT(mine_fileinfo.triggers_howmany);
		MINE_GET_INT(mine_fileinfo.triggers_sizeof);
		MINE_GET_INT(mine_fileinfo.links_offset);
		MINE_GET_INT(mine_fileinfo.links_howmany);
		MINE_GET_INT(mine_fileinfo.links_sizeof);
		MINE_GET_INT(mine_fileinfo.object_offset);
		MINE_GET_INT(mine_fileinfo.object_howmany);
		MINE_GET_INT(mine_fileinfo.object_sizeof);
		MINE_GET_INT(mine_fileinfo.unused_offset);
		MINE_GET_INT(mine_fileinfo.unused_howmamy);
		MINE_GET_INT(mine_fileinfo.unused_sizeof);
		Assert(p - buf == MINE_FILEINFO_DISK_SIZE);
	}

	//===================== READ HEADER INFO ========================

	// Set default values.
	mine_header.num_vertices        =   0;
	mine_header.num_segments        =   0;

	if (mine_fileinfo.header_offset > -1 )
	{
		ubyte buf[MINE_HEADER_DISK_SIZE];
		const ubyte *p = buf;
		int n;

		if (cfseek( LoadFile, mine_fileinfo.header_offset, SEEK_SET ))
			Error( "Error seeking to header_offset in gamemine.c" );

		// ASCENT: field reads of mine_header
		n = mine_read_struct(buf, mine_fileinfo.header_size, MINE_HEADER_DISK_SIZE, LoadFile);
		if (n <= 0)
			Error( "Error reading mine_header in gamemine.c" );
		MINE_GET_INT(mine_header.num_vertices);
		MINE_GET_INT(mine_header.num_segments);
	}

	//===================== READ EDITOR INFO ==========================

	// Set default values
	mine_editor.current_seg         =   0;
	mine_editor.newsegment_offset   =   -1; // To be written
	mine_editor.newsegment_size     =   SEGMENT_EDITOR_DISK_SIZE;		// ASCENT: was sizeof(segment)
	mine_editor.Curside             =   0;
	mine_editor.Markedsegp          =   -1;
	mine_editor.Markedside          =   0;

	if (mine_fileinfo.editor_offset > -1 )
	{
		ubyte buf[MINE_EDITOR_DISK_SIZE];
		const ubyte *p = buf;
		int n;

		if (cfseek( LoadFile, mine_fileinfo.editor_offset, SEEK_SET ))
			Error( "Error seeking to editor_offset in gamemine.c" );

		// ASCENT: field reads of mine_editor
		n = mine_read_struct(buf, mine_fileinfo.editor_size, MINE_EDITOR_DISK_SIZE, LoadFile);
		if (n <= 0)
			Error( "Error reading mine_editor in gamemine.c" );
		MINE_GET_INT(mine_editor.current_seg);
		MINE_GET_INT(mine_editor.newsegment_offset);
		MINE_GET_INT(mine_editor.newsegment_size);
		MINE_GET_INT(mine_editor.Curside);
		MINE_GET_INT(mine_editor.Markedsegp);
		MINE_GET_INT(mine_editor.Markedside);
		for (i=0;i<10;i++)
			MINE_GET_INT(mine_editor.Groupsegp[i]);
		for (i=0;i<10;i++)
			MINE_GET_INT(mine_editor.Groupside[i]);
		MINE_GET_INT(mine_editor.num_groups);
		MINE_GET_INT(mine_editor.current_group);
		Assert(p - buf == MINE_EDITOR_DISK_SIZE);
	}

	//===================== READ TEXTURE INFO ==========================

	if ( (mine_fileinfo.texture_offset > -1) && (mine_fileinfo.texture_howmany > 0))
	{
		if (cfseek( LoadFile, mine_fileinfo.texture_offset, SEEK_SET ))
			Error( "Error seeking to texture_offset in gamemine.c" );

		// ASCENT: the original did not check these two values. Larger values
		// wrote past the end of old_tmap_list and tmap_xlate_table.
		if ( mine_fileinfo.texture_howmany > MAX_TEXTURES )
			Error( "Too many textures (%d, maximum %d) in the mine file", mine_fileinfo.texture_howmany, MAX_TEXTURES );
		if ( mine_fileinfo.texture_sizeof < 1 || mine_fileinfo.texture_sizeof > 13 )
			Error( "Bad texture name size (%d) in the mine file", mine_fileinfo.texture_sizeof );

		for (i=0; i< mine_fileinfo.texture_howmany; i++ )
		{
			if (cfread( &old_tmap_list[i], mine_fileinfo.texture_sizeof, 1, LoadFile )!=1)
				Error( "Error reading old_tmap_list[i] in gamemine.c" );
		}
	}

	//=============== GENERATE TEXTURE TRANSLATION TABLE ===============

	translate = 0;
	
	Assert (NumTextures < MAX_TEXTURES);

	{
		hashtable ht;
	
		hashtable_init( &ht, NumTextures );
	
		// Remove all the file extensions in the textures list
	
		for (i=0;i<NumTextures;i++)	{
			temptr = strchr(TmapInfo[i].filename, '.');
			if (temptr) *temptr = '\0';
			hashtable_insert( &ht, TmapInfo[i].filename, i );
		}
	
		// For every texture, search through the texture list
		// to find a matching name.
		for (j=0;j<mine_fileinfo.texture_howmany;j++) 	{
			// Remove this texture name's extension
			temptr = strchr(old_tmap_list[j], '.');
			if (temptr) *temptr = '\0';
	
			tmap_xlate_table[j] = hashtable_search( &ht,old_tmap_list[j]);
			if (tmap_xlate_table[j]	< 0 )	{
				//tmap_xlate_table[j] = 0;
				// mprintf( (0, "Couldn't find texture '%s'\n", old_tmap_list[j] ));
				;
			}
			if (tmap_xlate_table[j] != j ) translate = 1;
			if (tmap_xlate_table[j] >= 0)
				tmap_times_used[tmap_xlate_table[j]]++;
		}
	
		{
			int count = 0;
			for (i=0; i<MAX_TEXTURES; i++ )
				if (tmap_times_used[i])
					count++;
			mprintf( (0, "This mine has %d unique textures in it (~%d KB)\n", count, (count*4096) /1024 ));
		}
	
		mprintf( (0, "Translate=%d\n", translate ));
	
		hashtable_free( &ht );
	}

	//====================== READ VERTEX INFO ==========================

	// New check added to make sure we don't read in too many vertices.
	if ( mine_fileinfo.vertex_howmany > MAX_VERTICES )
		{
		mprintf((0, "Num vertices exceeds maximum.  Loading MAX %d vertices\n", MAX_VERTICES));
		mine_fileinfo.vertex_howmany = MAX_VERTICES;
		}

	if ( (mine_fileinfo.vertex_offset > -1) && (mine_fileinfo.vertex_howmany > 0))
	{
		if (cfseek( LoadFile, mine_fileinfo.vertex_offset, SEEK_SET ))
			Error( "Error seeking to vertex_offset in gamemine.c" );

		for (i=0; i< mine_fileinfo.vertex_howmany; i++ )
		{
			// Set the default values for this vertex
			Vertices[i].x = 1;
			Vertices[i].y = 1;
			Vertices[i].z = 1;

			// ASCENT: field read of the vertex
			if (!mine_read_vertex( &Vertices[i], mine_fileinfo.vertex_sizeof, LoadFile ))
				Error( "Error reading Vertices[i] in gamemine.c" );
		}
	}

	//==================== READ SEGMENT INFO ===========================

	// New check added to make sure we don't read in too many segments.
	if ( mine_fileinfo.segment_howmany > MAX_SEGMENTS ) {
		mprintf((0, "Num segments exceeds maximum.  Loading MAX %d segments\n", MAX_SEGMENTS));
		mine_fileinfo.segment_howmany = MAX_SEGMENTS;
	}

	// [commented out by mk on 11/20/94 (weren't we supposed to hit final in October?) because it looks redundant.  I think I'll test it now...]  fuelcen_reset();

	if ( (mine_fileinfo.segment_offset > -1) && (mine_fileinfo.segment_howmany > 0))	{

		if (cfseek( LoadFile, mine_fileinfo.segment_offset,SEEK_SET ))

			Error( "Error seeking to segment_offset in gamemine.c" );

		Highest_segment_index = mine_fileinfo.segment_howmany-1;

		for (i=0; i< mine_fileinfo.segment_howmany; i++ ) {
			segment v16_seg;

			// Set the default values for this segment (clear to zero )
			//memset( &Segments[i], 0, sizeof(segment) );

			if (mine_top_fileinfo.fileinfo_version >= 16) {
				// ASCENT: the record on disk is the segment of the editor build
				// (522 bytes, with segnum and group). It is converted one field
				// at a time. The game structure has no segnum and no group.
				ubyte seg_buf[SEGMENT_EDITOR_DISK_SIZE];

				Assert(mine_fileinfo.segment_sizeof == SEGMENT_EDITOR_DISK_SIZE);

				if (cfread( seg_buf, SEGMENT_EDITOR_DISK_SIZE, 1, LoadFile )!=1)
					Error( "Error reading segments in gamemine.c" );

				segment_from_disk_editor( &v16_seg, seg_buf, NULL, NULL );
			}				
			else 
				Error("Invalid mine version");

			Segments[i] = v16_seg;

			Segments[i].objects = -1;
			#ifdef EDITOR
			Segments[i].group = -1;
			#endif

			if (mine_top_fileinfo.fileinfo_version < 15) {	//used old uvl ranges
				int sn,uvln;

				for (sn=0;sn<MAX_SIDES_PER_SEGMENT;sn++)
					for (uvln=0;uvln<4;uvln++) {
						Segments[i].sides[sn].uvls[uvln].u /= 64;
						Segments[i].sides[sn].uvls[uvln].v /= 64;
						Segments[i].sides[sn].uvls[uvln].l /= 32;
					}
			}

			fuelcen_activate( &Segments[i], Segments[i].special );

			if (translate == 1)
				for (j=0;j<MAX_SIDES_PER_SEGMENT;j++) {
					unsigned short orient;
					tmap_xlate = Segments[i].sides[j].tmap_num;
					Segments[i].sides[j].tmap_num = tmap_xlate_table[tmap_xlate];
					if ((WALL_IS_DOORWAY(&Segments[i],j) & WID_RENDER_FLAG))
						if (Segments[i].sides[j].tmap_num < 0)	{
							mprintf( (0, "Couldn't find texture '%s' for Segment %d, side %d\n", old_tmap_list[tmap_xlate],i,j));
							Int3();
							Segments[i].sides[j].tmap_num = 0;
						}
					tmap_xlate = Segments[i].sides[j].tmap_num2 & 0x3FFF;
					orient = Segments[i].sides[j].tmap_num2 & (~0x3FFF);
					if (tmap_xlate != 0) {
						int xlated_tmap = tmap_xlate_table[tmap_xlate];

						if ((WALL_IS_DOORWAY(&Segments[i],j) & WID_RENDER_FLAG))
							if (xlated_tmap <= 0)	{
								mprintf( (0, "Couldn't find texture '%s' for Segment %d, side %d\n", old_tmap_list[tmap_xlate],i,j));
								Int3();
								Segments[i].sides[j].tmap_num2 = 0;
							}
						Segments[i].sides[j].tmap_num2 = xlated_tmap | orient;
					}
				}
		}
	}

	//===================== READ NEWSEGMENT INFO =====================

	#ifdef EDITOR

	{		// Default segment created.
		vms_vector	sizevec;
		med_create_new_segment(vm_vec_make(&sizevec,DEFAULT_X_SIZE,DEFAULT_Y_SIZE,DEFAULT_Z_SIZE));		// New_segment = Segments[0];
		//memset( &New_segment, 0, sizeof(segment) );
	}

	if (mine_editor.newsegment_offset > -1)
	{
		if (cfseek( LoadFile, mine_editor.newsegment_offset,SEEK_SET ))
			Error( "Error seeking to newsegment_offset in gamemine.c" );
		// ASCENT: field read of the segment record
		{
			ubyte seg_buf[SEGMENT_EDITOR_DISK_SIZE];

			Assert(mine_editor.newsegment_size == SEGMENT_EDITOR_DISK_SIZE);
			if (cfread( seg_buf, SEGMENT_EDITOR_DISK_SIZE,1,LoadFile )!=1)
				Error( "Error reading new_segment in gamemine.c" );
			segment_from_disk_editor( &New_segment, seg_buf, NULL, NULL );
		}
	}

	if ( (mine_fileinfo.newseg_verts_offset > -1) && (mine_fileinfo.newseg_verts_howmany > 0))
	{
		if (cfseek( LoadFile, mine_fileinfo.newseg_verts_offset, SEEK_SET ))
			Error( "Error seeking to newseg_verts_offset in gamemine.c" );
		for (i=0; i< mine_fileinfo.newseg_verts_howmany; i++ )
		{
			// Set the default values for this vertex
			Vertices[NEW_SEGMENT_VERTICES+i].x = 1;
			Vertices[NEW_SEGMENT_VERTICES+i].y = 1;
			Vertices[NEW_SEGMENT_VERTICES+i].z = 1;
			
			// ASCENT: field read of the vertex
			if (!mine_read_vertex( &Vertices[NEW_SEGMENT_VERTICES+i], mine_fileinfo.newseg_verts_sizeof, LoadFile ))
				Error( "Error reading Vertices[NEW_SEGMENT_VERTICES+i] in gamemine.c" );

			New_segment.verts[i] = NEW_SEGMENT_VERTICES+i;
		}
	}

	#endif
															
	//========================= UPDATE VARIABLES ======================

	#ifdef EDITOR

	// Setting to Markedsegp to NULL ignores Curside and Markedside, which
	// we want to do when reading in an old file.
	
 	Markedside = mine_editor.Markedside;
	Curside = mine_editor.Curside;
	for (i=0;i<10;i++)
		Groupside[i] = mine_editor.Groupside[i];

	if ( mine_editor.current_seg != -1 )
		Cursegp = mine_editor.current_seg + Segments;
	else
 		Cursegp = NULL;

	if (mine_editor.Markedsegp != -1 ) 
		Markedsegp = mine_editor.Markedsegp + Segments;
	else
		Markedsegp = NULL;

	num_groups = 0;
	current_group = -1;

	#endif

	Num_vertices = mine_fileinfo.vertex_howmany;
	Num_segments = mine_fileinfo.segment_howmany;
	Highest_vertex_index = Num_vertices-1;
	Highest_segment_index = Num_segments-1;

	reset_objects(1);		//one object, the player

	#ifdef EDITOR
	Highest_vertex_index = MAX_SEGMENT_VERTICES-1;
	Highest_segment_index = MAX_SEGMENTS-1;
	set_vertex_counts();
	Highest_vertex_index = Num_vertices-1;
	Highest_segment_index = Num_segments-1;

	warn_if_concave_segments();
	#endif

	#ifdef EDITOR
		validate_segment_all();
	#endif

	//create_local_segment_data();

	//gamemine_find_textures();

	if (mine_top_fileinfo.fileinfo_version < MINE_VERSION )
		return 1;		//old version
	else
		return 0;

}
// ASCENT: the "#endif" of the "#ifdef EDITOR" around load_mine_data() was here.

// -----------------------------------------------------------------------------
// ASCENT: new. The mine data of a Descent to Undermountain level (mine
// version 18, see docs/formats/lvl.md). It is load_mine_data() for the
// records of that game, and it reads from memory. The differences:
//  - the segment record has 536 bytes (segment_from_disk_dtu in dataio.h)
//  - the texture name table of the level is kept (Level_tmap_names) and the
//    texture numbers of the sides are not translated here. The translation
//    is level_translate_textures(), which the caller runs when the texture
//    table of the game is loaded.
//  - the "new segment" of the editor and its vertices are not read.

char Level_tmap_names[MAX_LEVEL_TMAP_NAMES][13];
int Num_level_tmap_names = 0;
short Level_tmap_xlate[MAX_LEVEL_TMAP_NAMES];
int Level_tmaps_translated = 0;
int Level_validate_type_changes = 0;
int Level_validate_normal_changes = 0;

// The record of a file structure at "offset". Stops the program if the
// structure is not inside the data.
static const ubyte *mine_dtu_range(const ubyte *data, int size, int offset, int count, int record_size, char *what)
{
	if (offset < 0 || count < 0 || record_size < 0 || offset > size ||
		 (int64_t)offset + (int64_t)count * record_size > size)
		Error( "The level file is damaged: %s (offset %d, %d records of %d bytes, file size %d)", what, offset, count, record_size, size );
	return data + offset;
}

// returns 0=everything ok, -1=error
int load_mine_data_dtu(const ubyte *data, int size, int mine_start)
{
	const ubyte *p;
	int i, j;

	fuelcen_reset();

	//===================== READ FILE INFO ========================

	p = mine_dtu_range(data, size, mine_start, 1, MINE_FILEINFO_DISK_SIZE, "mine fileinfo");

	memset( &mine_fileinfo, 0, sizeof(mine_fileinfo) );
	mine_fileinfo.fileinfo_signature = dio_get_ushort(&p);
	mine_fileinfo.fileinfo_version = dio_get_ushort(&p);
	mine_fileinfo.fileinfo_sizeof = dio_get_int(&p);
	mine_top_fileinfo.fileinfo_signature = mine_fileinfo.fileinfo_signature;
	mine_top_fileinfo.fileinfo_version = mine_fileinfo.fileinfo_version;
	mine_top_fileinfo.fileinfo_sizeof = mine_fileinfo.fileinfo_sizeof;

	if (mine_fileinfo.fileinfo_signature != 0x2884)
		return -1;

	// The executable of the target game converts the versions 16 and 17
	// (a segment record of 522 bytes). No level file has them.
	if (mine_fileinfo.fileinfo_version != MINE_VERSION_DTU) {
		printf( "Mine version %d is not supported (only %d)\n", mine_fileinfo.fileinfo_version, MINE_VERSION_DTU );
		return -1;
	}
	if (mine_fileinfo.fileinfo_sizeof != MINE_FILEINFO_DISK_SIZE) {
		printf( "Mine fileinfo size %d is not supported\n", mine_fileinfo.fileinfo_sizeof );
		return -1;
	}

	mine_fileinfo.header_offset = dio_get_int(&p);
	mine_fileinfo.header_size = dio_get_int(&p);
	mine_fileinfo.editor_offset = dio_get_int(&p);
	mine_fileinfo.editor_size = dio_get_int(&p);
	mine_fileinfo.segment_offset = dio_get_int(&p);
	mine_fileinfo.segment_howmany = dio_get_int(&p);
	mine_fileinfo.segment_sizeof = dio_get_int(&p);
	mine_fileinfo.newseg_verts_offset = dio_get_int(&p);
	mine_fileinfo.newseg_verts_howmany = dio_get_int(&p);
	mine_fileinfo.newseg_verts_sizeof = dio_get_int(&p);
	mine_fileinfo.group_offset = dio_get_int(&p);
	mine_fileinfo.group_howmany = dio_get_int(&p);
	mine_fileinfo.group_sizeof = dio_get_int(&p);
	mine_fileinfo.vertex_offset = dio_get_int(&p);
	mine_fileinfo.vertex_howmany = dio_get_int(&p);
	mine_fileinfo.vertex_sizeof = dio_get_int(&p);
	mine_fileinfo.texture_offset = dio_get_int(&p);
	mine_fileinfo.texture_howmany = dio_get_int(&p);
	mine_fileinfo.texture_sizeof = dio_get_int(&p);
	mine_fileinfo.walls_offset = dio_get_int(&p);
	mine_fileinfo.walls_howmany = dio_get_int(&p);
	mine_fileinfo.walls_sizeof = dio_get_int(&p);
	mine_fileinfo.triggers_offset = dio_get_int(&p);
	mine_fileinfo.triggers_howmany = dio_get_int(&p);
	mine_fileinfo.triggers_sizeof = dio_get_int(&p);
	mine_fileinfo.links_offset = dio_get_int(&p);
	mine_fileinfo.links_howmany = dio_get_int(&p);
	mine_fileinfo.links_sizeof = dio_get_int(&p);
	mine_fileinfo.object_offset = dio_get_int(&p);
	mine_fileinfo.object_howmany = dio_get_int(&p);
	mine_fileinfo.object_sizeof = dio_get_int(&p);
	mine_fileinfo.unused_offset = dio_get_int(&p);
	mine_fileinfo.unused_howmamy = dio_get_int(&p);
	mine_fileinfo.unused_sizeof = dio_get_int(&p);

	//===================== READ HEADER INFO ========================

	mine_header.num_vertices        =   0;
	mine_header.num_segments        =   0;

	if (mine_fileinfo.header_offset > -1 )
	{
		p = mine_dtu_range(data, size, mine_fileinfo.header_offset, 1, MINE_HEADER_DISK_SIZE, "mine header");
		mine_header.num_vertices = dio_get_int(&p);
		mine_header.num_segments = dio_get_int(&p);
	}

	//===================== READ EDITOR INFO ==========================

	// Only the offset and the size of the "new segment" are used, to keep
	// the facts of the file. The editor state is not restored.
	memset( &mine_editor, 0, sizeof(mine_editor) );
	mine_editor.newsegment_offset   =   -1;
	mine_editor.Markedsegp          =   -1;

	if (mine_fileinfo.editor_offset > -1 )
	{
		p = mine_dtu_range(data, size, mine_fileinfo.editor_offset, 1, MINE_EDITOR_DISK_SIZE, "mine editor block");
		mine_editor.current_seg = dio_get_int(&p);
		mine_editor.newsegment_offset = dio_get_int(&p);
		mine_editor.newsegment_size = dio_get_int(&p);
		mine_editor.Curside = dio_get_int(&p);
		mine_editor.Markedsegp = dio_get_int(&p);
		mine_editor.Markedside = dio_get_int(&p);
		for (i=0;i<10;i++)
			mine_editor.Groupsegp[i] = dio_get_int(&p);
		for (i=0;i<10;i++)
			mine_editor.Groupside[i] = dio_get_int(&p);
		mine_editor.num_groups = dio_get_int(&p);
		mine_editor.current_group = dio_get_int(&p);
	}

	//===================== READ TEXTURE INFO ==========================

	Num_level_tmap_names = 0;
	Level_tmaps_translated = 0;

	if ( (mine_fileinfo.texture_offset > -1) && (mine_fileinfo.texture_howmany > 0))
	{
		if ( mine_fileinfo.texture_howmany > MAX_LEVEL_TMAP_NAMES )
			Error( "Too many textures (%d, maximum %d) in the mine file", mine_fileinfo.texture_howmany, MAX_LEVEL_TMAP_NAMES );
		if ( mine_fileinfo.texture_sizeof != 13 )
			Error( "Bad texture name size (%d) in the mine file", mine_fileinfo.texture_sizeof );

		p = mine_dtu_range(data, size, mine_fileinfo.texture_offset, mine_fileinfo.texture_howmany, 13, "texture names");

		for (i=0; i< mine_fileinfo.texture_howmany; i++ )
		{
			dio_get_bytes(&p, Level_tmap_names[i], 13);
			Level_tmap_names[i][12] = 0;
		}
		Num_level_tmap_names = mine_fileinfo.texture_howmany;
	}

	for (i=0; i<MAX_LEVEL_TMAP_NAMES; i++ )
		Level_tmap_xlate[i] = -1;

	//====================== READ VERTEX INFO ==========================

	// The original loaded the first MAX_VERTICES vertices of a larger mine.
	// The segments of such a mine have vertex numbers past the end.
	if ( mine_fileinfo.vertex_howmany > MAX_VERTICES )
		Error( "Too many vertices (%d, maximum %d) in the mine file", mine_fileinfo.vertex_howmany, MAX_VERTICES );
	if ( mine_fileinfo.vertex_howmany < 0 || mine_fileinfo.vertex_sizeof != VERTEX_DISK_SIZE )
		Error( "Bad vertex table (%d records of %d bytes) in the mine file", mine_fileinfo.vertex_howmany, mine_fileinfo.vertex_sizeof );

	if ( (mine_fileinfo.vertex_offset > -1) && (mine_fileinfo.vertex_howmany > 0))
	{
		p = mine_dtu_range(data, size, mine_fileinfo.vertex_offset, mine_fileinfo.vertex_howmany, VERTEX_DISK_SIZE, "vertices");

		for (i=0; i< mine_fileinfo.vertex_howmany; i++ )
			dio_get_vector(&p, &Vertices[i]);
	}

	//==================== READ SEGMENT INFO ===========================

	if ( mine_fileinfo.segment_howmany > MAX_SEGMENTS )
		Error( "Too many segments (%d, maximum %d) in the mine file", mine_fileinfo.segment_howmany, MAX_SEGMENTS );
	if ( mine_fileinfo.segment_howmany < 0 || mine_fileinfo.segment_sizeof != SEGMENT_DTU_DISK_SIZE )
		Error( "Bad segment table (%d records of %d bytes) in the mine file", mine_fileinfo.segment_howmany, mine_fileinfo.segment_sizeof );

	if ( (mine_fileinfo.segment_offset > -1) && (mine_fileinfo.segment_howmany > 0))	{

		p = mine_dtu_range(data, size, mine_fileinfo.segment_offset, mine_fileinfo.segment_howmany, SEGMENT_DTU_DISK_SIZE, "segments");

		Highest_segment_index = mine_fileinfo.segment_howmany-1;

		for (i=0; i< mine_fileinfo.segment_howmany; i++ ) {

			segment_from_disk_dtu( &Segments[i], p );
			p += SEGMENT_DTU_DISK_SIZE;

			Segments[i].objects = -1;
			Segments[i].group = -1;

			// The numbers that index arrays are tested here once, so that
			// the game code can trust them.
			for (j=0;j<MAX_VERTICES_PER_SEGMENT;j++)
				if (Segments[i].verts[j] < 0 || Segments[i].verts[j] >= mine_fileinfo.vertex_howmany)
					Error( "Segment %d has the vertex number %d (%d vertices)", i, Segments[i].verts[j], mine_fileinfo.vertex_howmany );
			for (j=0;j<MAX_SIDES_PER_SEGMENT;j++) {
				if (Segments[i].children[j] < -2 || Segments[i].children[j] >= mine_fileinfo.segment_howmany)
					Error( "Segment %d has the child number %d (%d segments)", i, Segments[i].children[j], mine_fileinfo.segment_howmany );
				if (Segments[i].sides[j].wall_num < -1 || Segments[i].sides[j].wall_num >= MAX_WALLS)
					Error( "Segment %d, side %d has the wall number %d", i, j, Segments[i].sides[j].wall_num );
				// Until level_translate_textures() runs, each side has
				// texture 0 of the game and is marked as "no texture".
				Segments[i].sides[j].tmap_num = 0;
				Segments[i].sides[j].tmap_num2 = 0;
			}
			Segments[i].dtu_tmap_missing = 0x3f;
			Segments[i].dtu_tmap2_missing = 0;

			fuelcen_activate( &Segments[i], Segments[i].special );
		}
	}

	//========================= UPDATE VARIABLES ======================

	Num_vertices = mine_fileinfo.vertex_howmany;
	Num_segments = mine_fileinfo.segment_howmany;
	Highest_vertex_index = Num_vertices-1;
	Highest_segment_index = Num_segments-1;

	reset_objects(1);		//one object, the player

	// The executable of the target game is an editor build. Its
	// load_mine_data() ends with validate_segment_all(), which computes the
	// type and the normals of each side again, and does not use the values
	// of the file. validate_segment_all() there leaves out a segment with
	// segnum -1. The same is done here. The counters tell how many sides
	// got another type or another normal than the file has.
	Level_validate_type_changes = 0;
	Level_validate_normal_changes = 0;
	for (i=0; i<=Highest_segment_index; i++)
		if (Segments[i].segnum != -1) {
			byte file_type[MAX_SIDES_PER_SEGMENT];
			vms_vector file_normals[MAX_SIDES_PER_SEGMENT][2];

			for (j=0;j<MAX_SIDES_PER_SEGMENT;j++) {
				file_type[j] = Segments[i].sides[j].type;
				file_normals[j][0] = Segments[i].sides[j].normals[0];
				file_normals[j][1] = Segments[i].sides[j].normals[1];
			}

			validate_segment(&Segments[i]);

			for (j=0;j<MAX_SIDES_PER_SEGMENT;j++) {
				int n;
				if (file_type[j] != Segments[i].sides[j].type)
					Level_validate_type_changes++;
				for (n=0;n<2;n++)
					if (abs(file_normals[j][n].x - Segments[i].sides[j].normals[n].x) > 0x100 ||
						 abs(file_normals[j][n].y - Segments[i].sides[j].normals[n].y) > 0x100 ||
						 abs(file_normals[j][n].z - Segments[i].sides[j].normals[n].z) > 0x100) {
						Level_validate_normal_changes++;
						break;
					}
			}
		}

	return 0;
}

// Compares a texture name of a level with a file name of the texture table
// of the game. The case of the letters and the extension are ignored.
static int level_tmap_name_match(char *level_name, char *game_name)
{
	int i;

	for (i=0; i<13; i++) {
		char a = level_name[i], b = game_name[i];
		if (a == '.') a = 0;
		if (b == '.') b = 0;
		if (tolower(a) != tolower(b))
			return 0;
		if (a == 0)
			return 1;
	}
	return 1;
}

// ASCENT: new. Translates the texture numbers of the loaded level to the
// texture table of the game (NumTextures, TmapInfo), by name, as
// load_mine_data() does it. The numbers of the level file stay in the
// segments (dtu_tmap_num, dtu_tmap_num2) and in the objects
// (dtu.tmap_override_level), so this function can run again when the
// texture table changes. A side with a name that the game does not have
// gets texture 0 and its bit in dtu_tmap_missing.
// If "report" is not 0, the function prints each name that it cannot find
// and that a visible side or an object uses.
// Returns the number of those names.
int level_translate_textures(int report)
{
	static short times_missing[MAX_LEVEL_TMAP_NAMES];
	int i, j, segnum, sidenum;
	int n_missing = 0;

	for (j=0; j<MAX_LEVEL_TMAP_NAMES; j++) {
		Level_tmap_xlate[j] = -1;
		times_missing[j] = 0;
	}

	for (j=0; j<Num_level_tmap_names; j++)
		for (i=0; i<NumTextures; i++)
			if (level_tmap_name_match(Level_tmap_names[j], TmapInfo[i].filename)) {
				Level_tmap_xlate[j] = i;
				break;
			}

	for (segnum=0; segnum<=Highest_segment_index; segnum++) {
		segment *segp = &Segments[segnum];

		segp->dtu_tmap_missing = 0;
		segp->dtu_tmap2_missing = 0;

		for (sidenum=0; sidenum<MAX_SIDES_PER_SEGMENT; sidenum++) {
			side *sidep = &segp->sides[sidenum];
			int visible = (segp->children[sidenum] == -1) || (sidep->wall_num != -1);
			int tmap = segp->dtu_tmap_num[sidenum];
			int tmap2 = segp->dtu_tmap_num2[sidenum] & 0x3FFF;
			int orient = segp->dtu_tmap_num2[sidenum] & (~0x3FFF);

			if (tmap >= 0 && tmap < Num_level_tmap_names && Level_tmap_xlate[tmap] >= 0)
				sidep->tmap_num = Level_tmap_xlate[tmap];
			else {
				sidep->tmap_num = 0;
				segp->dtu_tmap_missing |= 1 << sidenum;
				if (visible && tmap >= 0 && tmap < Num_level_tmap_names)
					times_missing[tmap]++;
			}

			sidep->tmap_num2 = 0;
			if (tmap2 != 0) {
				// as in load_mine_data(): number 0 of the game is "no overlay"
				if (tmap2 < Num_level_tmap_names && Level_tmap_xlate[tmap2] > 0)
					sidep->tmap_num2 = Level_tmap_xlate[tmap2] | orient;
				else {
					segp->dtu_tmap2_missing |= 1 << sidenum;
					if (visible && tmap2 < Num_level_tmap_names)
						times_missing[tmap2]++;
				}
			}
		}
	}

	for (i=0; i<=Highest_object_index; i++) {
		object *obj = &Objects[i];

		if (obj->type != OBJ_NONE && (obj->render_type == RT_POLYOBJ || obj->render_type == RT_MORPH)) {
			int tmo = obj->dtu.tmap_override_level;

			// as in read_object(): a number outside the table is kept
			if (tmo < 0 || tmo >= Num_level_tmap_names)
				obj->rtype.pobj_info.tmap_override = (tmo < 0) ? tmo : -1;
			else if (Level_tmap_xlate[tmo] >= 0)
				obj->rtype.pobj_info.tmap_override = Level_tmap_xlate[tmo];
			else {
				obj->rtype.pobj_info.tmap_override = -1;
				times_missing[tmo]++;
			}
		}
	}

	// Make sure non-transparent doors are set correctly.
	// (This is in load_game_data() of the original. It needs the door
	// animations of the game, so it is done with the textures.)
	for (segnum=0; segnum<=Highest_segment_index; segnum++)
		for (sidenum=0;sidenum<MAX_SIDES_PER_SEGMENT;sidenum++) {
			side	*sidep = &Segments[segnum].sides[sidenum];
			if ((sidep->wall_num != -1) && (Walls[sidep->wall_num].clip_num != -1) && (Walls[sidep->wall_num].clip_num < Num_wall_anims)) {
				if (WallAnims[Walls[sidep->wall_num].clip_num].flags & WCF_TMAP1) {
					sidep->tmap_num = WallAnims[Walls[sidep->wall_num].clip_num].frames[0];
					sidep->tmap_num2 = 0;
					Segments[segnum].dtu_tmap_missing &= ~(1 << sidenum);
					Segments[segnum].dtu_tmap2_missing &= ~(1 << sidenum);
				}
			}
		}

	for (j=0; j<Num_level_tmap_names; j++)
		if (times_missing[j]) {
			n_missing++;
			if (report && NumTextures > 0)
				printf( "Level texture %d '%s' is not in the texture table of the game (used %d times)\n", j, Level_tmap_names[j], times_missing[j] );
		}

	if (report && NumTextures <= 0)
		printf( "The texture table of the game is empty. The sides of the level have no texture (%d texture names in use).\n", n_missing );
	else if (report && n_missing)
		printf( "%d texture names of the level are not in the texture table of the game (%d textures)\n", n_missing, NumTextures );

	Level_tmaps_translated = 1;

	return n_missing;
}

#define COMPILED_MINE_VERSION 0

int	New_file_format_load = 1;

int load_mine_data_compiled(CFILE *LoadFile)
{
	int i,segnum,sidenum;
	ubyte version;
	short temp_short;
	ushort temp_ushort;

#ifndef SHAREWARE
	if (New_file_format_load)
		return load_mine_data_compiled_new(LoadFile);
#endif

	//	For compiled levels, textures map to themselves, prevent tmap_override always being gray,
	//	bug which Matt and John refused to acknowledge, so here is Mike, fixing it.
	// 
	// Although in a cloud of arrogant glee, he forgot to ifdef it on EDITOR!
	// (Matt told me to write that!)
	// ASCENT: tmap_xlate_table is always built now (it was under EDITOR),
	// because the editor format loader and read_object() use it.
	for (i=0; i<MAX_TEXTURES; i++)
		tmap_xlate_table[i] = i;
//	memset( Segments, 0, sizeof(segment)*MAX_SEGMENTS );
	fuelcen_reset();

	//=============================== Reading part ==============================
	// ASCENT: all reads in this function are little endian field reads now
	// (cf_get_*), in place of cfread() into the variables.
	version = cf_get_ubyte( LoadFile );						// 1 byte = compiled version
	Assert( version==COMPILED_MINE_VERSION );
	Num_vertices = cf_get_int( LoadFile );					// 4 bytes = Num_vertices
	Assert( Num_vertices <= MAX_VERTICES );
	Num_segments = cf_get_int( LoadFile );					// 4 bytes = Num_segments
	Assert( Num_segments <= MAX_SEGMENTS );
	cf_get_vectors( LoadFile, Vertices, Num_vertices );

	for (segnum=0; segnum<Num_segments; segnum++ )	{
		#ifdef EDITOR
		Segments[segnum].segnum = segnum;
		Segments[segnum].group = 0;
		#endif

		// Read short Segments[segnum].children[MAX_SIDES_PER_SEGMENT]
 		cf_get_shorts( LoadFile, Segments[segnum].children, MAX_SIDES_PER_SEGMENT );
		// Read short Segments[segnum].verts[MAX_VERTICES_PER_SEGMENT]
		cf_get_shorts( LoadFile, Segments[segnum].verts, MAX_VERTICES_PER_SEGMENT );
		Segments[segnum].objects = -1;

		// Read ubyte	Segments[segnum].special
		Segments[segnum].special = cf_get_ubyte( LoadFile );
		// Read byte	Segments[segnum].matcen_num
		Segments[segnum].matcen_num = cf_get_byte( LoadFile );
		// Read short	Segments[segnum].value
		Segments[segnum].value = cf_get_short( LoadFile );

		// Read fix	Segments[segnum].static_light (shift down 5 bits, write as short)
		temp_ushort = cf_get_ushort( LoadFile );
		Segments[segnum].static_light	= ((fix)temp_ushort) << 4;
		//cfread( &Segments[segnum].static_light, sizeof(fix), 1, LoadFile );

		// Read the walls as a 6 byte array
		for (sidenum=0; sidenum<MAX_SIDES_PER_SEGMENT; sidenum++ )	{
			ubyte byte_wallnum;
			Segments[segnum].sides[sidenum].pad = 0;
			byte_wallnum = cf_get_ubyte( LoadFile );
if ( byte_wallnum == 255 )			
				Segments[segnum].sides[sidenum].wall_num = -1;
			else		
				Segments[segnum].sides[sidenum].wall_num = byte_wallnum;
		}

		for (sidenum=0; sidenum<MAX_SIDES_PER_SEGMENT; sidenum++ )	{

			if ( (Segments[segnum].children[sidenum]==-1) || (Segments[segnum].sides[sidenum].wall_num!=-1) )	{
				// Read short Segments[segnum].sides[sidenum].tmap_num;
				Segments[segnum].sides[sidenum].tmap_num = cf_get_short( LoadFile );
				// Read short Segments[segnum].sides[sidenum].tmap_num2;
				Segments[segnum].sides[sidenum].tmap_num2 = cf_get_short( LoadFile );
				// Read uvl Segments[segnum].sides[sidenum].uvls[4] (u,v>>5, write as short, l>>1 write as short)
				for (i=0; i<4; i++ )	{
					temp_short = cf_get_short( LoadFile );
					Segments[segnum].sides[sidenum].uvls[i].u = ((fix)temp_short) << 5;
					temp_short = cf_get_short( LoadFile );
					Segments[segnum].sides[sidenum].uvls[i].v = ((fix)temp_short) << 5;
					temp_ushort = cf_get_ushort( LoadFile );
					Segments[segnum].sides[sidenum].uvls[i].l = ((fix)temp_ushort) << 1;
					//cfread( &Segments[segnum].sides[sidenum].uvls[i].l, sizeof(fix), 1, LoadFile );
				}	
			} else {
				Segments[segnum].sides[sidenum].tmap_num = 0;
				Segments[segnum].sides[sidenum].tmap_num2 = 0;
				for (i=0; i<4; i++ )	{
					Segments[segnum].sides[sidenum].uvls[i].u = 0;
					Segments[segnum].sides[sidenum].uvls[i].v = 0;
					Segments[segnum].sides[sidenum].uvls[i].l = 0;
				}	
			}
		}
	}

	Highest_vertex_index = Num_vertices-1;
	Highest_segment_index = Num_segments-1;

	validate_segment_all();			// Fill in side type and normals.

	// Activate fuelcentes
	for (i=0; i< Num_segments; i++ ) {
		fuelcen_activate( &Segments[i], Segments[i].special );
	}

	reset_objects(1);		//one object, the player

	return 0;
}

#ifndef SHAREWARE
int load_mine_data_compiled_new(CFILE *LoadFile)
{
	int		i,segnum,sidenum;
	ubyte		version;
	short		temp_short;
	ushort	temp_ushort;
	ubyte		bit_mask;

	//	For compiled levels, textures map to themselves, prevent tmap_override always being gray,
	//	bug which Matt and John refused to acknowledge, so here is Mike, fixing it.
	// ASCENT: tmap_xlate_table is always built now (it was under EDITOR),
	// because the editor format loader and read_object() use it.
	for (i=0; i<MAX_TEXTURES; i++)
		tmap_xlate_table[i] = i;

//	memset( Segments, 0, sizeof(segment)*MAX_SEGMENTS );
	fuelcen_reset();

	//=============================== Reading part ==============================
	// ASCENT: all reads in this function are little endian field reads now
	// (cf_get_*), in place of cfread() into the variables.
	version = cf_get_ubyte( LoadFile );						// 1 byte = compiled version
	Assert( version==COMPILED_MINE_VERSION );

	temp_ushort = cf_get_ushort( LoadFile );					// 2 bytes = Num_vertices
	Num_vertices = temp_ushort;
	Assert( Num_vertices <= MAX_VERTICES );

	temp_ushort = cf_get_ushort( LoadFile );					// 2 bytes = Num_segments
	Num_segments = temp_ushort;
	Assert( Num_segments <= MAX_SEGMENTS );

	cf_get_vectors( LoadFile, Vertices, Num_vertices );

	for (segnum=0; segnum<Num_segments; segnum++ )	{
		int	bit;

		#ifdef EDITOR
		Segments[segnum].segnum = segnum;
		Segments[segnum].group = 0;
		#endif

 		bit_mask = cf_get_ubyte( LoadFile );

		for (bit=0; bit<MAX_SIDES_PER_SEGMENT; bit++) {
			if (bit_mask & (1 << bit))
		 		Segments[segnum].children[bit] = cf_get_short( LoadFile );
			else
				Segments[segnum].children[bit] = -1;
		}

		// Read short Segments[segnum].verts[MAX_VERTICES_PER_SEGMENT]
		cf_get_shorts( LoadFile, Segments[segnum].verts, MAX_VERTICES_PER_SEGMENT );
		Segments[segnum].objects = -1;

		if (bit_mask & (1 << MAX_SIDES_PER_SEGMENT)) {
			// Read ubyte	Segments[segnum].special
			Segments[segnum].special = cf_get_ubyte( LoadFile );
			// Read byte	Segments[segnum].matcen_num
			Segments[segnum].matcen_num = cf_get_byte( LoadFile );
			// Read short	Segments[segnum].value
			Segments[segnum].value = cf_get_short( LoadFile );
		} else {
			Segments[segnum].special = 0;
			Segments[segnum].matcen_num = -1;
			Segments[segnum].value = 0;
		}

		// Read fix	Segments[segnum].static_light (shift down 5 bits, write as short)
		temp_ushort = cf_get_ushort( LoadFile );
		Segments[segnum].static_light	= ((fix)temp_ushort) << 4;
		//cfread( &Segments[segnum].static_light, sizeof(fix), 1, LoadFile );

		// Read the walls as a 6 byte array
		for (sidenum=0; sidenum<MAX_SIDES_PER_SEGMENT; sidenum++ )	{
			Segments[segnum].sides[sidenum].pad = 0;
		}

 		bit_mask = cf_get_ubyte( LoadFile );
		for (sidenum=0; sidenum<MAX_SIDES_PER_SEGMENT; sidenum++) {
			ubyte byte_wallnum;

			if (bit_mask & (1 << sidenum)) {
				byte_wallnum = cf_get_ubyte( LoadFile );
				if ( byte_wallnum == 255 )			
					Segments[segnum].sides[sidenum].wall_num = -1;
				else		
					Segments[segnum].sides[sidenum].wall_num = byte_wallnum;
			} else
					Segments[segnum].sides[sidenum].wall_num = -1;
		}

		for (sidenum=0; sidenum<MAX_SIDES_PER_SEGMENT; sidenum++ )	{

			if ( (Segments[segnum].children[sidenum]==-1) || (Segments[segnum].sides[sidenum].wall_num!=-1) )	{
				// Read short Segments[segnum].sides[sidenum].tmap_num;
				temp_ushort = cf_get_ushort( LoadFile );

				Segments[segnum].sides[sidenum].tmap_num = temp_ushort & 0x7fff;

				if (!(temp_ushort & 0x8000))
					Segments[segnum].sides[sidenum].tmap_num2 = 0;
				else {
					// Read short Segments[segnum].sides[sidenum].tmap_num2;
					Segments[segnum].sides[sidenum].tmap_num2 = cf_get_short( LoadFile );
				}

				// Read uvl Segments[segnum].sides[sidenum].uvls[4] (u,v>>5, write as short, l>>1 write as short)
				for (i=0; i<4; i++ )	{
					temp_short = cf_get_short( LoadFile );
					Segments[segnum].sides[sidenum].uvls[i].u = ((fix)temp_short) << 5;
					temp_short = cf_get_short( LoadFile );
					Segments[segnum].sides[sidenum].uvls[i].v = ((fix)temp_short) << 5;
					temp_ushort = cf_get_ushort( LoadFile );
					Segments[segnum].sides[sidenum].uvls[i].l = ((fix)temp_ushort) << 1;
					//cfread( &Segments[segnum].sides[sidenum].uvls[i].l, sizeof(fix), 1, LoadFile );
				}	
			} else {
				Segments[segnum].sides[sidenum].tmap_num = 0;
				Segments[segnum].sides[sidenum].tmap_num2 = 0;
				for (i=0; i<4; i++ )	{
					Segments[segnum].sides[sidenum].uvls[i].u = 0;
					Segments[segnum].sides[sidenum].uvls[i].v = 0;
					Segments[segnum].sides[sidenum].uvls[i].l = 0;
				}	
			}
		}
	}

	Highest_vertex_index = Num_vertices-1;
	Highest_segment_index = Num_segments-1;

	validate_segment_all();			// Fill in side type and normals.

	// Activate fuelcentes
	for (i=0; i< Num_segments; i++ ) {
		fuelcen_activate( &Segments[i], Segments[i].special );
	}

	reset_objects(1);		//one object, the player

	return 0;
}

#endif
