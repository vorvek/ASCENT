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
 * $Source: f:/miner/source/mem/rcs/mem.c $
 * $Revision: 1.18 $
 * $Author: matt $
 * $Date: 1995/01/24 20:49:18 $
 * 
 * Files for debugging memory allocator
 * 
 * $Log: mem.c $
 * Revision 1.18  1995/01/24  20:49:18  matt
 * Made some stuff return when get errors
 * 
 * Revision 1.17  1994/11/29  15:40:38  matt
 * Added extra newline after message
 * 
 * Revision 1.16  1994/11/15  18:27:56  john
 * Took away show mem info by default.
 * 
 * Revision 1.15  1994/11/10  10:00:37  john
 * Made it show_mem_info by default.
 * 
 * Revision 1.14  1994/11/10  09:53:06  john
 * Put in more efficient, but less debugging info version.
 * 
 * Revision 1.13  1994/10/27  00:56:45  john
 * Reduced number of blocks; made show mem usage by default.
 * 
 * Revision 1.12  1994/10/06  19:15:17  john
 * Upped the number of blocks.
 * 
 * 
 * Revision 1.11  1994/07/27  20:04:22  john
 * Neatend printed output.
 * 
 * Revision 1.10  1994/07/20  10:22:17  yuan
 * Added overwrite error
 * 
 * Revision 1.9  1994/03/23  12:58:43  john
 * Made message global.
 * 
 * Revision 1.8  1994/03/15  11:12:59  john
 * Made calloc fill block with zeros like it's
 * supposed to.
 * 
 * Revision 1.7  1994/02/18  12:43:21  john
 * Only print mem debugging info if show_mem_info
 * is true. Need to set in debugger, btw.
 * 
 * Revision 1.6  1994/02/17  17:01:34  john
 * Took out MEM_LEAKAGE warning!
 * 
 * Revision 1.5  1994/01/24  16:04:47  john
 * Added mem_print_all function to dump all
 * allocated memory to inferno.mem file.
 * 
 * 
 * Revision 1.4  1994/01/18  11:01:41  john
 * *** empty log message ***
 * 
 * Revision 1.3  1993/12/10  12:20:50  john
 * Speed up by replacing mem_find_unused_id with a table lookup.
 * 
 * Revision 1.2  1993/12/08  12:38:22  mike
 * Change 10000 to MAX_INDEX
 * Use LargestIndex in place of MAX_INDEX as appropriate.
 * 
 * Revision 1.1  1993/11/02  17:45:28  john
 * Initial revision
 * 
 * 
 */

// ASCENT: the Watcom "#pragma off (unreferenced)" lines are removed.
static char rcsid[] = "$Id: mem.c 1.18 1995/01/24 20:49:18 matt Exp $";


// Warning( "MEM: Too many malloc's!" );
// Warning( "MEM: Malloc returnd an already alloced block!" );
// Warning( "MEM: Malloc Failed!" );
// Warning( "MEM: Freeing the NULL pointer!" );
// Warning( "MEM: Freeing a non-malloced pointer!" );
// Warning( "MEM: %d/%d check bytes were overwritten at the end of %8x", ec, CHECKSIZE, buffer  );
// Warning( "MEM: %d blocks were left allocated!", numleft );

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <stdint.h>

#include "error.h"

// ASCENT: this file was written again. The original had two allocators.
// The one with FULL_MEM_CHECKING kept the address of each block in an
// "unsigned int" table with 10000 entries. The other one (the one that was
// in use) read the block size from the 4 bytes before the block, which is
// a detail of the Watcom heap, and its overwrite check was turned off.
// The two of them are wrong on a 64-bit system. The DOS memory report (the
// distance of the heap from the 4 megabyte load address) is removed too.
//
// The new allocator does what the FULL_MEM_CHECKING one did:
//   - it puts CHECKSIZE check bytes after each block
//   - it keeps the size, the variable name, the file and the line of each block
//   - mem_free() checks the check bytes and refuses a pointer that it does not know
//   - mem_display_blocks() (called at exit) reports the blocks that are left
// The blocks are in a hash table that has the block address as the key.
// The table grows, so the number of blocks has no limit.
// The functions are not safe for use from two threads, as before.

#define CHECKSIZE 16
#define CHECKBYTE 0xFC

typedef struct mem_block {
	void		*base;			// address of the block, NULL = entry is free
	size_t	size;				// size that the caller asked for
	char		*varname;
	char		*filename;
	int		line;
} mem_block;

#define INITIAL_TABLE_SIZE 1024		// must be a power of 2

static mem_block *Blocks = NULL;		// hash table with linear probing
static size_t Table_size = 0;			// number of entries, a power of 2
static size_t num_blocks = 0;
static size_t BytesMalloced = 0;
static size_t LargestBytesMalloced = 0;

int show_mem_info = 0;
int mem_report_leaks = 1;		// ASCENT: see mem.h

static int Initialized = 0;

int out_of_memory = 0;

void mem_display_blocks();

static size_t mem_hash( void * ptr )
{
	uint64_t h = (uint64_t)(uintptr_t)ptr;

	h *= 0x9E3779B97F4A7C15ull;
	return (size_t)(h >> 32) & (Table_size-1);
}

void mem_init()
{
	Initialized = 1;

	num_blocks = 0;
	BytesMalloced = 0;
	LargestBytesMalloced = 0;

	atexit(mem_display_blocks);
}

static void PrintInfo( mem_block * b )
{
	fprintf( stderr, "\tBlock '%s' created in %s, line %d.\n", b->varname, b->filename, b->line );
}

// Returns the table entry of a block, or NULL if the block is not in the table.
static mem_block * mem_find_block( void * buffer )
{
	size_t i;

	if (Table_size==0) return NULL;

	for (i=mem_hash(buffer); Blocks[i].base!=NULL; i=(i+1)&(Table_size-1) )
		if (Blocks[i].base == buffer )
			return &Blocks[i];

	// Didn't find it.
	return NULL;
}

// Puts a block in a free table entry. The table must have a free entry.
static void mem_insert_block( mem_block * b )
{
	size_t i;

	for (i=mem_hash(b->base); Blocks[i].base!=NULL; i=(i+1)&(Table_size-1) )
		;
	Blocks[i] = *b;
}

// Makes the table two times as large. Returns 0 if there is no memory.
static int mem_grow_table()
{
	mem_block * old_blocks = Blocks;
	size_t old_size = Table_size, i;
	mem_block * new_blocks;

	new_blocks = calloc( old_size ? old_size*2 : INITIAL_TABLE_SIZE, sizeof(mem_block) );
	if (new_blocks==NULL) return 0;

	Blocks = new_blocks;
	Table_size = old_size ? old_size*2 : INITIAL_TABLE_SIZE;

	for (i=0; i<old_size; i++ )
		if (old_blocks[i].base!=NULL)
			mem_insert_block( &old_blocks[i] );

	free( old_blocks );

	return 1;
}

// Removes a table entry. Moves the entries that follow it, so that each
// entry can still be found from its hash position.
static void mem_remove_block( mem_block * b )
{
	size_t mask = Table_size-1;
	size_t i = b - Blocks, j = i, k;

	for (;;) {
		j = (j+1) & mask;
		if (Blocks[j].base==NULL) break;
		k = mem_hash( Blocks[j].base );
		if ( (i<=j) ? (i<k && k<=j) : (i<k || k<=j) )
			continue;		// entry j is not after its hash position if it moves to i
		Blocks[i] = Blocks[j];
		i = j;
	}
	Blocks[i].base = NULL;
}

void * mem_malloc( unsigned int size, char * var, char * filename, int line, int fill_zero )
{
	mem_block b;
	void *ptr;
	char * pc;
	int i;

	if (Initialized==0)
		mem_init();

	if (size==0)	{
		fprintf( stderr, "\nMEM_MALLOC_ZERO: Attempting to malloc 0 bytes.\n" );
		fprintf( stderr, "\tVar %s, file %s, line %d.\n", var, filename, line );
		Error( "MEM_MALLOC_ZERO" );
	}

	// Keep the table less than half full.
	if ( (num_blocks+1)*2 > Table_size )
		if (!mem_grow_table())	{
			out_of_memory = 1;
			fprintf( stderr,"\nMEM_OUT_OF_SLOTS: Not enough space in mem.c to hold all the mallocs.\n" );
			fprintf( stderr, "\tBlock '%s' created in %s, line %d.\n", var, filename, line );
			Error( "MEM_OUT_OF_SLOTS" );
		}

	ptr = malloc( (size_t)size+CHECKSIZE );

	if (ptr==NULL)
	{
		out_of_memory = 1;
		fprintf( stderr, "\nMEM_OUT_OF_MEMORY: Malloc returned NULL\n" );
		fprintf( stderr, "\tBlock '%s' created in %s, line %d.\n", var, filename, line );
		Error( "MEM_OUT_OF_MEMORY" );
	}

	b.base = ptr;
	b.size = size;
	b.varname = var;
	b.filename = filename;
	b.line = line;
	mem_insert_block( &b );
	num_blocks++;

	pc = (char *)ptr;

	BytesMalloced += size;
	if (BytesMalloced > LargestBytesMalloced) LargestBytesMalloced = BytesMalloced;

	for (i=0; i<CHECKSIZE; i++ )
		pc[size+i] = CHECKBYTE;

	if (fill_zero)
		memset( ptr, 0, size );

	return ptr;

}

// Returns the number of check bytes that are overwritten.
static int mem_check_integrity( mem_block * b )
{
	int i, ErrorCount;
	unsigned char * CheckData;

	CheckData = (unsigned char *)b->base + b->size;

	ErrorCount = 0;

	for (i=0; i<CHECKSIZE; i++ )
		if (CheckData[i] != CHECKBYTE ) {
			ErrorCount++;
			fprintf( stderr, "OA: %p ", (void *)&CheckData[i] );
		}

	if (ErrorCount &&  (!out_of_memory))	{
		fprintf( stderr, "\nMEM_OVERWRITE: Memory after the end of allocated block overwritten.\n" );
		PrintInfo( b );
		fprintf( stderr, "\t%d/%d check bytes were overwritten.\n", ErrorCount, CHECKSIZE );
	}

	return ErrorCount;

}

void mem_free( void * buffer )
{
	mem_block * b;

	if (Initialized==0)
		mem_init();

	if (buffer==NULL  &&  (!out_of_memory))
	{
		fprintf( stderr, "\nMEM_FREE_NULL: An attempt was made to free the null pointer.\n" );
		Warning( "MEM: Freeing the NULL pointer!" );
		Int3();
		return;
	}

	b = mem_find_block( buffer );

	if (b==NULL)
	{
		// ASCENT: the original went on with the index -1 when out_of_memory was set.
		if (!out_of_memory)	{
			fprintf( stderr, "\nMEM_FREE_NOMALLOC: An attempt was made to free a ptr that wasn't\nallocated with mem.h included.\n" );
			Warning( "MEM: Freeing a non-malloced pointer!" );
			Int3();
		}
		return;
	}

	mem_check_integrity( b );

	BytesMalloced -= b->size;

	free( buffer );

	mem_remove_block( b );
	num_blocks--;
}

// ASCENT: reports the blocks that are left. It does not free them, because
// an exit function that runs later can still use them.
void mem_display_blocks()
{
	size_t i;
	int numleft;

	if (Initialized==0) return;
	if (!mem_report_leaks) return;		// ASCENT: the release build

	numleft = 0;
	for (i=0; i<Table_size; i++ )
	{
		if (Blocks[i].base!=NULL &&  (!out_of_memory))
		{
			numleft++;
			if (show_mem_info)	{
				fprintf( stderr, "\nMEM_LEAKAGE: Memory block has not been freed.\n" );
				PrintInfo( &Blocks[i] );
			}
			mem_check_integrity( &Blocks[i] );
		}
	}

	if (numleft &&  (!out_of_memory))
	{
		fprintf( stderr, "\nMEM_LEAKAGE: %d blocks (%u bytes) of memory have not been freed.\n", numleft, (unsigned int)BytesMalloced );
	}

	if (show_mem_info)	{
		fprintf( stderr, "\n\nMEMORY USAGE:\n" );
		fprintf( stderr, "  %u Kbytes dynamic data in use\n", (unsigned int)((BytesMalloced+512)/1024) );
		fprintf( stderr, "  %u Kbytes dynamic data at the largest\n\n", (unsigned int)((LargestBytesMalloced+512)/1024) );
	}
}

int mem_validate_heap()
{
	size_t i;
	int n_bad = 0;

	for (i=0; i<Table_size; i++  )
		if (Blocks[i].base!=NULL )
			if (mem_check_integrity( &Blocks[i] ))
				n_bad++;

	return n_bad;
}

void mem_print_all()
{
	FILE * ef;
	size_t i, size = 0;

	ef = fopen( "DESCENT.MEM", "wt" );
	if (ef==NULL) return;

	for (i=0; i<Table_size; i++  )
		if (Blocks[i].base!=NULL )	{
			size += Blocks[i].size;
			//fprintf( ef, "Var:%s\t File:%s\t Line:%d\t Size:%d Base:%x\n", Varname[i], Filename[i], Line[i], MallocSize[i], MallocBase[i] );
			fprintf( ef, "%12u bytes in %s declared in %s, line %d\n", (unsigned int)Blocks[i].size, Blocks[i].varname, Blocks[i].filename, Blocks[i].line  );
		}
	fprintf( ef, "%u bytes (%u Kbytes) allocated.\n", (unsigned int)size, (unsigned int)(size/1024) );
	fclose(ef);
}

int mem_num_blocks()
{
	return (int)num_blocks;
}

unsigned int mem_bytes_malloced()
{
	return (unsigned int)BytesMalloced;
}
