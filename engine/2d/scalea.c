/*
 * ASCENT: C replacement for scalea.asm (run length slice bitmap
 * scan line stretcher).
 *
 * scalea.asm stays in the tree as the reference. It is not built.
 *
 * The assembly "compiled" the run lengths of one scan line into a block
 * of machine code (it changed its own instructions). The C code keeps
 * the run lengths in a table. The pixels are the same.
 */

#include <string.h>

#include "types.h"
#include "gr.h"
#include "error.h"

char scale_trans_color;
int scale_error_term;
int scale_initial_pixel_count;
int scale_adj_up;
int scale_adj_down;
int scale_final_pixel_count;
int scale_ydelta_minus_1;
int scale_whole_step;
ubyte * scale_source_ptr;
ubyte * scale_dest_ptr;

// The assembly had space for 320 source pixels between the first and
// the last. A larger count ran code before the block.
// ASCENT: the table has the size of the row buffer in scale.c.
#define SCALE_CC_MAX_RUNS 640

static int scale_cc_run_length[SCALE_CC_MAX_RUNS];
static int scale_cc_num_runs;

// Writes count pixels of color c. Does not write the transparent color.
static ubyte * scale_run( ubyte * dest, ubyte c, int count )
{
	if (count <= 0)
		return dest;
	if (c != (ubyte)scale_trans_color)
		memset( dest, c, (size_t)count );
	return dest + count;
}

// Stretches one scan line. Uses the scale_ variables that
// rls_stretch_scanline_setup (scale.c) set.
// ASCENT: the assembly does the middle loop one time or more. With
// scale_ydelta_minus_1 less than 1 it does not stop. The C function
// does the loop scale_ydelta_minus_1 times.
void rls_stretch_scanline_asm(void)
{
	ubyte * src = scale_source_ptr;
	ubyte * dest = scale_dest_ptr;
	int error_term = scale_error_term;
	int i, count;

	// Draw the first, partial run of pixels
	dest = scale_run( dest, *src++, scale_initial_pixel_count );

	// Draw all full runs
	for (i=0; i<scale_ydelta_minus_1; i++ )	{
		count = scale_whole_step;
		error_term += scale_adj_up;
		if (error_term > 0)	{
			count++;
			error_term -= scale_adj_down;
		}
		dest = scale_run( dest, *src++, count );
	}

	// Draw the final run of pixels
	scale_run( dest, *src, scale_final_pixel_count );
}

// Prepares the run lengths for scale_do_cc_scanline.
void rls_do_cc_setup_asm(void)
{
	int error_term = scale_error_term;
	int i;

	scale_cc_num_runs = scale_ydelta_minus_1;
	if (scale_cc_num_runs < 0)
		scale_cc_num_runs = 0;
	if (scale_cc_num_runs > SCALE_CC_MAX_RUNS)
		Error( "scale: source row of %d pixels is too long\n", scale_cc_num_runs+2 );

	for (i=0; i<scale_cc_num_runs; i++ )	{
		error_term += scale_adj_up;
		if (error_term > 0)	{
			scale_cc_run_length[i] = scale_whole_step + 1;
			error_term -= scale_adj_down;
		} else
			scale_cc_run_length[i] = scale_whole_step;
	}
}

// Stretches one scan line from scale_source_ptr to scale_dest_ptr
// with the run lengths that rls_do_cc_setup_asm prepared.
void scale_do_cc_scanline(void)
{
	ubyte * src = scale_source_ptr;
	ubyte * dest = scale_dest_ptr;
	int i;

	// Do the first texture pixel
	dest = scale_run( dest, *src++, scale_initial_pixel_count );

	// Do the middle pixels
	for (i=0; i<scale_cc_num_runs; i++ )
		dest = scale_run( dest, *src++, scale_cc_run_length[i] );

	// Do the last texture pixel
	scale_run( dest, *src, scale_final_pixel_count );
}
