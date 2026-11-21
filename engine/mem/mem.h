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
 * $Source: f:/miner/source/mem/rcs/mem.h $
 * $Revision: 1.6 $
 * $Author: matt $
 * $Date: 1995/02/12 18:40:50 $
 * 
 * Headers for safe malloc stuff.
 * 
 * $Log: mem.h $
 * Revision 1.6  1995/02/12  18:40:50  matt
 * Made free() work the way it used to when debugging is on
 * 
 * Revision 1.5  1995/02/12  04:07:36  matt
 * Made free() set ptrs to NULL even when no debugging
 * 
 * Revision 1.4  1994/11/27  21:10:58  matt
 * Now supports NDEBUG to turn off all special mem checking
 * 
 * Revision 1.3  1994/03/15  11:12:40  john
 * Made calloc fill block with zeros like it's
 * supposed to.
 * 
 * Revision 1.2  1993/11/04  14:02:39  matt
 * Added calloc() macro
 * 
 * Revision 1.1  1993/11/02  17:45:33  john
 * Initial revision
 * 
 * 
 */

#ifndef _MEM_H
#define _MEM_H

// ASCENT: the system headers that declare malloc() and free() must come
// before the macros below. A system header that comes after the macros
// would not compile.
#include <stdlib.h>
#if defined(__has_include)
#if __has_include(<malloc.h>)
#include <malloc.h>
#endif
#endif

extern int show_mem_info;//moved out of the ifdef by KRB

// ASCENT: the functions are declared with and without NDEBUG. mem.c has
// one allocator (see the comment there). mem_display_blocks() returns void,
// as its definition always did.
void mem_display_blocks();
extern void * mem_malloc( unsigned int size, char * var, char * file, int line, int fill_zero );
extern void mem_free( void * buffer );

// Checks to see if any blocks are overwritten
// ASCENT: returns the number of blocks that are overwritten
int mem_validate_heap();

// Writes the list of the blocks to the file DESCENT.MEM
void mem_print_all();

// ASCENT: new functions, for tests and for statistics
int mem_num_blocks();						// number of blocks that are allocated now
unsigned int mem_bytes_malloced();		// number of bytes that are allocated now

#ifndef NDEBUG

//extern int show_mem_info;

#define malloc(size)    mem_malloc((size),"Unknown", __FILE__,__LINE__, 0 )
#define calloc(n,size)  mem_malloc((n*size),"Unknown", __FILE__,__LINE__, 1 )
#define free(ptr)       do{ mem_free(ptr); ptr=NULL; } while(0)

#define MALLOC( var, type, count )   (var=(type *)mem_malloc((count)*sizeof(type),#var, __FILE__,__LINE__,0 ))

#else

#define free(ptr)       do{ free(ptr); ptr=NULL; } while(0)

#define MALLOC( var, type, count )   (var=(type *)malloc((count)*sizeof(type)))

#endif

#endif
