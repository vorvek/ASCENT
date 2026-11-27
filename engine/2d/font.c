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
/* $Source: f:/miner/source/2d/rcs/font.c $
 * $Revision: 1.30 $
 * $Author: john $
 * $Date: 1995/01/25 20:02:03 $
 *
 * Graphical routines for drawing fonts.
 *
 * $Log: font.c $
 * Revision 1.30  1995/01/25  20:02:03  john
 * Fixed bug with mono clipped fonts only drawing 1 row.
 * 
 * Revision 1.29  1994/12/08  19:03:54  john
 * Made functions use cfile.
 * 
 * Revision 1.28  1994/11/27  23:08:22  matt
 * Changes for new mprintf calling convention
 * 
 * Revision 1.27  1994/11/18  23:54:50  john
 * Made centering work on x = 0x8000 not -0x8000.
 * 
 * Revision 1.26  1994/11/18  22:50:23  john
 * Changed shorts to ints in parameters.
 * 
 * Revision 1.25  1994/11/04  11:10:31  john
 * Took the & out of string sizing.
 * 
 * Revision 1.24  1994/11/04  10:06:36  john
 * Added fade table for fading fonts. Made font that partially clips
 * not print a warning message.
 * 
 * Revision 1.23  1994/09/29  10:09:06  john
 * Added real clipping to text.
 * 
 * Revision 1.22  1994/09/12  19:27:51  john
 * Fixed bug with unclipped fonts clipping.
 * 
 * Revision 1.21  1994/09/12  17:16:59  john
 * Added clipping.
 * 
 * Revision 1.20  1994/08/28  16:43:01  matt
 * Added centering if x==0x8000
 * 
 * Revision 1.19  1994/08/15  15:01:11  matt
 * Close font file after reading
 * 
 * Revision 1.18  1994/08/12  18:17:42  matt
 * Fixed stupid bug that cause first char in font not to print
 * 
 * Revision 1.17  1994/08/11  18:00:17  matt
 * Cleaned up code, fixed string width bug.
 * 
 * Revision 1.16  1994/08/10  18:46:11  matt
 * Fixed a couple of little things
 * 
 * Revision 1.15  1994/08/10  18:36:20  matt
 * Changed font file format.  Made chars not in font plot as spaces.  Font file
 * now stores palette for color font & remaps on load.
 * 
 * Revision 1.14  1994/08/10  12:27:27  matt
 * Took out unneeded int3
 * 
 * Revision 1.13  1994/08/10  12:25:26  matt
 * Added support for colors fonts & kerned fonts
 * Made our own font file format
 * 
 * Revision 1.12  1994/07/22  17:19:00  john
 * made proportional font width be the max width.
 * 
 * Revision 1.11  1994/05/06  12:50:26  john
 * Added supertransparency; neatend things up; took out warnings.
 * 
 * Revision 1.10  1994/01/31  11:08:12  john
 * fixed bug with order of frees.
 * 
 * Revision 1.9  1994/01/31  10:36:40  john
 * Free'd variable size font data.
 * 
 * Revision 1.8  1994/01/27  17:17:40  john
 * Made error if font file doesn't exist.
 * 
 * Revision 1.7  1993/10/26  13:17:48  john
 * *** empty log message ***
 * 
 * Revision 1.6  1993/10/15  16:23:25  john
 * y
 * 
 * Revision 1.5  1993/09/28  19:05:50  john
 * added support for \n in gr_string and gr_get_string_size
 * 
 * Revision 1.4  1993/09/28  12:46:49  matt
 * On error, now call Error() instead of exit().
 * 
 * Revision 1.3  1993/09/20  11:35:31  john
 * *** empty log message ***
 * 
 * Revision 1.2  1993/09/08  15:54:39  john
 * renamed ReadFile to readfontfile to prevent conflicts with others.
 * 
 * Revision 1.1  1993/09/08  11:43:34  john
 * Initial revision
 * 
 *
 */

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ASCENT: removed <conio.h>, <fcntl.h>, <io.h>, <sys\types.h> and
// <sys\stat.h> (not used).

#include "mem.h"

#include "gr.h"
#include "grdef.h"
#include "error.h"

#include "cfile.h"
#include "mono.h"

#define FONT        grd_curcanv->cv_font
#define FG_COLOR    grd_curcanv->cv_font_fg_color
#define BG_COLOR    grd_curcanv->cv_font_bg_color
#define FWIDTH       FONT->ft_w
#define FHEIGHT      FONT->ft_h
#define FBASELINE    FONT->ft_baseline
#define FFLAGS       FONT->ft_flags
#define FMINCHAR     FONT->ft_minchar
#define FMAXCHAR     FONT->ft_maxchar
#define FDATA        FONT->ft_data
#define FCHARS       FONT->ft_chars
#define FWIDTHS      FONT->ft_widths

#define BITS_TO_BYTES(x)    (((x)+7)>>3)

int gr_internal_string_clipped(int x, int y, char *s );
int gr_internal_string_clipped_m(int x, int y, char *s );

char *find_kern_entry(grs_font *font,int first,int second)
{
	ubyte *p=font->ft_kerndata;

	while (*p!=255)
		if (p[0]==first && p[1]==second)
			return p;
		else p+=3;

	return NULL;

}

//takes the character AFTER being offset into font
#define INFONT(_c) ((_c >= 0) && (_c <= FMAXCHAR-FMINCHAR))

//takes the character BEFORE being offset into current font
void get_char_width(int c,int c2,int *width,int *spacing)
{
	int letter;

	// ASCENT: the callers pass a char, which is signed: the letters of
	// Latin-1 (0xA0 to 0xFF) are bytes above 127.
	c &= 0xFF;
	c2 &= 0xFF;
	letter = c-FMINCHAR;

	if (!INFONT(letter)) {				//not in font, draw as space
		*width=0;
		if (FFLAGS & FT_PROPORTIONAL)
			*spacing = FWIDTH/2;
		else
			*spacing = FWIDTH;
		return;
	}

	if (FFLAGS & FT_PROPORTIONAL)
		*width = FWIDTHS[letter];
	else
		*width = FWIDTH;

	*spacing = *width;

	if (FFLAGS & FT_KERNED)  {
		ubyte *p;

		if (!(c2==0 || c2=='\n')) {
			int letter2;

			letter2 = c2-FMINCHAR;

			if (INFONT(letter2)) {

				p = find_kern_entry(FONT,letter,letter2);

				if (p)
					*spacing = p[2];
			}
		}
	}
}

int get_centered_x(char *s)
{
	int w,w2,s2;

	for (w=0;*s!=0 && *s!='\n';s++) {
		get_char_width(s[0],s[1],&w2,&s2);
		w += s2;
	}

	return ((grd_curcanv->cv_bitmap.bm_w - w) / 2);
}



int gr_internal_string0(int x, int y, char *s )
{
	unsigned char * fp;
	char * text_ptr, * next_row, * text_ptr1;
	int r, BitMask, i, bits, width, spacing, letter, underline;

	unsigned int VideoOffset, VideoOffset1;

	VideoOffset1 = y * ROWSIZE + x;

	next_row = s;

	while (next_row != NULL )
	{
		text_ptr1 = next_row;
		next_row = NULL;

		if (x==0x8000) {			//centered
			int xx = get_centered_x(text_ptr1);
			VideoOffset1 = y * ROWSIZE + xx;
		}

		for (r=0; r<FHEIGHT; r++)
		{

			text_ptr = text_ptr1;

			VideoOffset = VideoOffset1;

			while (*text_ptr)
			{
				if (*text_ptr == '\n' )
				{
					next_row = &text_ptr[1];
					break;
				}

				underline = 0;
				if (*text_ptr == '&' )
				{
					if ((r==FBASELINE+2) || (r==FBASELINE+3))
						underline = 1;
					text_ptr++;
				}

				get_char_width(text_ptr[0],text_ptr[1],&width,&spacing);

				letter = (ubyte) *text_ptr - FMINCHAR;	// ASCENT: unsigned, for the Latin-1 letters

				if (!INFONT(letter)) {	//not in font, draw as space
					VideoOffset += spacing;
					text_ptr++;
					continue;
				}

				if (FFLAGS & FT_PROPORTIONAL)
					fp = FCHARS[letter];
				else
					fp = FDATA + letter * BITS_TO_BYTES(width)*FHEIGHT;

				if (underline)
					for (i=0; i< width; i++ )
						DATA[VideoOffset++] = FG_COLOR;
				else
				{
					fp += BITS_TO_BYTES(width)*r;

					BitMask = 0;

					for (i=0; i< width; i++ )
					{
						if (BitMask==0) {
							bits = *fp++;
							BitMask = 0x80;
						}

						if (bits & BitMask)
							DATA[VideoOffset++] = FG_COLOR;
						else
							DATA[VideoOffset++] = BG_COLOR;
						BitMask >>= 1;
					}
				}

				VideoOffset += spacing-width;		//for kerning

				text_ptr++;
			}

			VideoOffset1 += ROWSIZE; y++;
		}
	}
	return 0;
}

int gr_internal_string0m(int x, int y, char *s )
{
	unsigned char * fp;
	char * text_ptr, * next_row, * text_ptr1;
	int r, BitMask, i, bits, width, spacing, letter, underline;

	unsigned int VideoOffset, VideoOffset1;

	VideoOffset1 = y * ROWSIZE + x;

	next_row = s;

	while (next_row != NULL )
	{
		text_ptr1 = next_row;
		next_row = NULL;

		if (x==0x8000) {			//centered
			int xx = get_centered_x(text_ptr1);
			VideoOffset1 = y * ROWSIZE + xx;
		}

		for (r=0; r<FHEIGHT; r++)
		{

			text_ptr = text_ptr1;

			VideoOffset = VideoOffset1;

			while (*text_ptr)
			{
				if (*text_ptr == '\n' )
				{
					next_row = &text_ptr[1];
					break;
				}

				underline = 0;
				if (*text_ptr == '&' )
				{
					if ((r==FBASELINE+2) || (r==FBASELINE+3))
						underline = 1;
					text_ptr++;
				}

				get_char_width(text_ptr[0],text_ptr[1],&width,&spacing);

				letter = (ubyte) *text_ptr - FMINCHAR;	// ASCENT: unsigned, for the Latin-1 letters

				if (!INFONT(letter)) {	//not in font, draw as space
					VideoOffset += spacing;
					text_ptr++;
					continue;
				}

				if (FFLAGS & FT_PROPORTIONAL)
					fp = FCHARS[letter];
				else
					fp = FDATA + letter * BITS_TO_BYTES(width)*FHEIGHT;

				if (underline)
					for (i=0; i< width; i++ )
						DATA[VideoOffset++] = FG_COLOR;
				else
				{
					fp += BITS_TO_BYTES(width)*r;

					BitMask = 0;

					for (i=0; i< width; i++ )
					{
						if (BitMask==0) {
							bits = *fp++;
							BitMask = 0x80;
						}

						if (bits & BitMask)
							DATA[VideoOffset++] = FG_COLOR;
						else
							VideoOffset++;
						BitMask >>= 1;
					}
				}
				text_ptr++;

				VideoOffset += spacing-width;
			}

			VideoOffset1 += ROWSIZE; y++;
		}
	}
	return 0;
}


// ASCENT: removed gr_internal_string2 and gr_internal_string2m. They
// wrote text through the 64K VESA window. An SVGA canvas is a linear
// frame in memory and uses gr_internal_string0 and gr_internal_string0m.

//a bitmap for the character
grs_bitmap char_bm = {
				0,0,0,0,						//x,y,w,h
				BM_LINEAR,					//type
				BM_FLAG_TRANSPARENT,		//flags
				0,								//rowsize
				NULL,							//data
				0								//selector
};

int gr_internal_color_string(int x, int y, char *s )
{
	unsigned char * fp;
	char * text_ptr, * next_row, * text_ptr1;
	int width, spacing,letter;
	int xx,yy;

	char_bm.bm_h = FHEIGHT;		//set height for chars of this font

	next_row = s;

	yy = y;

	while (next_row != NULL)
	{
		text_ptr1 = next_row;
		next_row = NULL;

		text_ptr = text_ptr1;

		xx = x;

		if (xx==0x8000)			//centered
			xx = get_centered_x(text_ptr);

		while (*text_ptr)
		{
			if (*text_ptr == '\n' )
			{
				next_row = &text_ptr[1];
				yy += FHEIGHT;
				break;
			}

			letter = (ubyte) *text_ptr - FMINCHAR;	// ASCENT: unsigned, for the Latin-1 letters

			get_char_width(text_ptr[0],text_ptr[1],&width,&spacing);

			if (!INFONT(letter)) {	//not in font, draw as space
				xx += spacing;
				text_ptr++;
				continue;
			}

			if (FFLAGS & FT_PROPORTIONAL)
				fp = FCHARS[letter];
			else
				fp = FDATA + letter * BITS_TO_BYTES(width)*FHEIGHT;

			char_bm.bm_w = char_bm.bm_rowsize = width;

			char_bm.bm_data = fp;
			gr_bitmapm(xx,yy,&char_bm);

			xx += spacing;

			text_ptr++;
		}

	}
	return 0;
}

int gr_string(int x, int y, char *s )
{
	int w, h, aw;
	int clipped=0;

	if ( x == 0x8000 )	{
		if ( y<0 ) clipped |= 1;
		gr_get_string_size(s, &w, &h, &aw );
		// for x, since this will be centered, only look at
		// width.
		if ( w > grd_curcanv->cv_bitmap.bm_w ) clipped |= 1;	
		if ( (y+h) > grd_curcanv->cv_bitmap.bm_h ) clipped |= 1;	

		if ( (y+h) < 0 ) clipped |= 2;	
		if ( y > grd_curcanv->cv_bitmap.bm_h ) clipped |= 2;	

	} else {
		if ( (x<0) || (y<0) ) clipped |= 1;
		gr_get_string_size(s, &w, &h, &aw );
		if ( (x+w) > grd_curcanv->cv_bitmap.bm_w ) clipped |= 1;	
		if ( (y+h) > grd_curcanv->cv_bitmap.bm_h ) clipped |= 1;	
		if ( (x+w) < 0 ) clipped |= 2;	
		if ( (y+h) < 0 ) clipped |= 2;	
		if ( x > grd_curcanv->cv_bitmap.bm_w ) clipped |= 2;	
		if ( y > grd_curcanv->cv_bitmap.bm_h ) clipped |= 2;	
	}

	if ( !clipped )
		return gr_ustring(x, y, s );
	
	if ( clipped & 2 )	{
		// Completely clipped...
		mprintf( (1, "Text '%s' at (%d,%d) is off screen!\n", s, x, y ));
		return 0;
	}

	if ( clipped & 1 )	{
		// Partially clipped...
		//mprintf( (0, "Text '%s' at (%d,%d) is getting clipped!\n", s, x, y ));
	}

	// Partially clipped...

	if (FFLAGS & FT_COLOR) 
		return gr_internal_color_string( x, y, s);

	if ( BG_COLOR == -1)
		return gr_internal_string_clipped_m( x, y, s );
	
	return gr_internal_string_clipped( x, y, s );
}

int gr_ustring(int x, int y, char *s )
{
	if (FFLAGS & FT_COLOR) {

		return gr_internal_color_string(x,y,s);

	}
	else
		// ASCENT: the Mode X and SVGA types are linear frames (see
		// grdef.h). The original drew no text on a Mode X canvas.
		switch( TYPE )
		{
		case BM_LINEAR:
		case BM_SVGA:
		case BM_MODEX:
			if ( BG_COLOR == -1)
				return gr_internal_string0m(x,y,s);
			else
				return gr_internal_string0(x,y,s);
		}

	return 0;
}


void gr_get_string_size(char *s, int *string_width, int *string_height, int *average_width )
{
	int i = 0, longest_width = 0;
	int width,spacing;

	*string_height = FHEIGHT;
	*string_width = 0;
	*average_width = FWIDTH;

	if (s != NULL )
	{
		*string_width = 0;
		while (*s)
		{
//			if (*s == '&')
//				s++;
			while (*s == '\n')
			{
				s++;
				*string_height += FHEIGHT;
				*string_width = 0;
			}

			if (*s == 0) break;

			get_char_width(s[0],s[1],&width,&spacing);

			*string_width += spacing;

			if (*string_width > longest_width)
				longest_width = *string_width;

			i++;
			s++;
		}
	}
	*string_width = longest_width;
}


int gr_uprintf( int x, int y, char * format, ... )
{
	char buffer[1000];
	va_list args;

	va_start(args, format );
	vsprintf(buffer,format,args);
	return gr_ustring( x, y, buffer );
}

int gr_printf( int x, int y, char * format, ... )
{
	char buffer[1000];
	va_list args;

	va_start(args, format );
	vsprintf(buffer,format,args);
	return gr_string( x, y, buffer );
}

static void latin1_store_free(grs_font *font);		// ASCENT

void gr_close_font( grs_font * font )
{
	if (font)
	{
		latin1_store_free(font);		// ASCENT: the added glyphs
		if ( font->ft_chars )
			free( font->ft_chars );
		free( font );
	}
}

void build_colormap_good( ubyte * palette, ubyte * colormap, int * freq );
// ASCENT: decode_data_asm is a C function in bitmap.c now.
void decode_data_asm(ubyte *data, int num_pixels, ubyte * colormap, int * count );

// ASCENT: the font file. All numbers are little endian.
//
//   offset  size
//   0       4     id, the characters "PSFN"
//   4       4     datasize: size of the font data that follows
//   8       ...   the font data (datasize bytes)
//   8+datasize    768 bytes of palette (red, green, blue with 6 bits),
//                 only if the font has the flag FT_COLOR
//
// The font data starts with a header of 28 bytes. It is the grs_font
// structure of the 32-bit compiler. The offsets in it are from the
// start of the font data.
//
//   0   2   ft_w
//   2   2   ft_h
//   4   2   ft_flags
//   6   2   ft_baseline
//   8   1   ft_minchar
//   9   1   ft_maxchar
//   10  2   ft_bytewidth
//   12  4   offset of the character bitmaps (proportional fonts)
//   16  4   not used (the ft_chars pointer)
//   20  4   offset of the table of widths, 2 bytes for each character
//           (proportional fonts)
//   24  4   offset of the kerning table (fonts with FT_KERNED)
//
// The bitmaps of a font that is not proportional start at offset 28.
//
// The original code read the font data into memory and used it as a
// grs_font, then added the address to the offsets. The structure now has
// pointers of the size of the machine, so the code reads each field.
#define FONT_FILE_HEADER_SIZE	28
#define FONT_MAX_CHARS			256

//------------------------------------------------------------------------
// ASCENT: the letters of the translations (docs/formats/lang.md). The text
// of a translation is Latin-1. The fonts of the game have the characters
// 32 to 126 (and some icons at 127 to 135), except FNTEMFS_, which has all
// 256 characters of code page 437. At load time each font gets the
// letters below: a copy of the code page 437 glyph when the font has all
// 256 characters, else the base letter with an accent drawn from its
// pixels. The font files do not change.

#define ACC_ACUTE		1
#define ACC_GRAVE		2
#define ACC_CIRC		3
#define ACC_DIAER		4
#define ACC_TILDE		5
#define ACC_CEDIL		6
#define ACC_FLIP		7		// the base glyph upside down (inverted ! and ?)

static const struct { ubyte code, base, accent, cp437; } Latin1_letters[] = {
	{ 0xE1, 'a', ACC_ACUTE, 160 }, { 0xE9, 'e', ACC_ACUTE, 130 }, { 0xED, 'i', ACC_ACUTE, 161 },
	{ 0xF3, 'o', ACC_ACUTE, 162 }, { 0xFA, 'u', ACC_ACUTE, 163 }, { 0xFC, 'u', ACC_DIAER, 129 },
	{ 0xF1, 'n', ACC_TILDE, 164 }, { 0xC1, 'A', ACC_ACUTE, 0 },   { 0xC9, 'E', ACC_ACUTE, 144 },
	{ 0xCD, 'I', ACC_ACUTE, 0 },   { 0xD3, 'O', ACC_ACUTE, 0 },   { 0xDA, 'U', ACC_ACUTE, 0 },
	{ 0xDC, 'U', ACC_DIAER, 154 }, { 0xD1, 'N', ACC_TILDE, 165 }, { 0xBF, '?', ACC_FLIP, 168 },
	{ 0xA1, '!', ACC_FLIP, 173 },
	{ 0xE0, 'a', ACC_GRAVE, 133 }, { 0xE8, 'e', ACC_GRAVE, 138 }, { 0xEC, 'i', ACC_GRAVE, 141 },
	{ 0xF2, 'o', ACC_GRAVE, 149 }, { 0xF9, 'u', ACC_GRAVE, 151 }, { 0xE2, 'a', ACC_CIRC, 131 },
	{ 0xEA, 'e', ACC_CIRC, 136 },  { 0xEE, 'i', ACC_CIRC, 140 },  { 0xF4, 'o', ACC_CIRC, 147 },
	{ 0xFB, 'u', ACC_CIRC, 150 },  { 0xE4, 'a', ACC_DIAER, 132 }, { 0xEB, 'e', ACC_DIAER, 137 },
	{ 0xEF, 'i', ACC_DIAER, 139 }, { 0xF6, 'o', ACC_DIAER, 148 }, { 0xE7, 'c', ACC_CEDIL, 135 },
	{ 0xC7, 'C', ACC_CEDIL, 128 },
};
#define N_LATIN1_LETTERS	((int) (sizeof(Latin1_letters) / sizeof(Latin1_letters[0])))

int gr_latin1_drawn(int c)
{
	int i;

	for (i = 0; i < N_LATIN1_LETTERS; i++)
		if (Latin1_letters[i].code == c)
			return 1;
	return 0;
}

#define GL_EMPTY	255		// an empty pixel of a glyph in the work form

// A glyph in the work form: one byte for each pixel, GL_EMPTY or a color
// (a mono font uses 1).
static int glyph_get(grs_font *font, int c, ubyte *px, int *w)
{
	int letter = c - font->ft_minchar, x, y, h = font->ft_h;
	ubyte *fp;

	if (letter < 0 || letter > font->ft_maxchar - font->ft_minchar)
		return 0;
	if (font->ft_flags & FT_PROPORTIONAL) {
		*w = font->ft_widths[letter];
		fp = font->ft_chars[letter];
	} else {
		*w = font->ft_w;
		fp = font->ft_data + letter * ((font->ft_flags & FT_COLOR) ? font->ft_w : BITS_TO_BYTES(font->ft_w)) * h;
	}
	if (*w <= 0 || *w > 64 || !fp)
		return 0;
	for (y = 0; y < h; y++)
		for (x = 0; x < *w; x++) {
			if (font->ft_flags & FT_COLOR)
				px[y * *w + x] = fp[y * *w + x];
			else
				px[y * *w + x] = (fp[y * BITS_TO_BYTES(*w) + (x >> 3)] & (0x80 >> (x & 7))) ? 1 : GL_EMPTY;
		}
	return 1;
}

// Writes a glyph of the work form in the form of the font.
static void glyph_put(grs_font *font, ubyte *dst, const ubyte *px, int w)
{
	int x, y, h = font->ft_h;

	if (font->ft_flags & FT_COLOR) {
		memcpy(dst, px, w * h);
		return;
	}
	memset(dst, 0, BITS_TO_BYTES(w) * h);
	for (y = 0; y < h; y++)
		for (x = 0; x < w; x++)
			if (px[y * w + x] != GL_EMPTY)
				dst[y * BITS_TO_BYTES(w) + (x >> 3)] |= 0x80 >> (x & 7);
}

static int row_empty(const ubyte *px, int w, int y)
{
	int x;

	for (x = 0; x < w; x++)
		if (px[y * w + x] != GL_EMPTY)
			return 0;
	return 1;
}

// Makes the letter in the rows top to bottom lower by need rows: its
// bottom row stays, its top moves down. First a row equal to the row
// under it goes (the pair nearest the middle first), so the shape stays;
// then the rows that are left are taken evenly. With dup_only the letter
// changes only when the equal rows are enough. Returns 1 when it changed.
static int squeeze_rows(ubyte *px, int w, int top, int bottom, int need, int dup_only)
{
	ubyte tmp[64 * 64];
	int rows[64], cnt = bottom - top + 1, m = cnt - need, i, y;

	if (need <= 0 || m < 1 || cnt > 64)
		return 0;
	for (i = 0; i < cnt; i++)
		rows[i] = top + i;
	while (cnt > m) {
		int best = -1, bestd = 1 << 30;
		for (i = 0; i + 1 < cnt; i++)
			if (!memcmp(px + rows[i] * w, px + rows[i + 1] * w, w)) {
				int d = abs(2 * i + 1 - (cnt - 1));
				if (d < bestd) {
					bestd = d;
					best = i;
				}
			}
		if (best < 0)
			break;
		memmove(rows + best + 1, rows + best + 2, (cnt - best - 2) * sizeof(int));
		cnt--;
	}
	if (dup_only && cnt > m)
		return 0;
	// then a row (not the first or the last) equal to another row, nearest
	// the middle: E keeps its middle bar
	while (cnt > m) {
		int best = -1, bestd = 1 << 30, j;
		for (i = 1; i + 1 < cnt; i++)
			for (j = 0; j < cnt; j++)
				if (j != i && !memcmp(px + rows[i] * w, px + rows[j] * w, w)) {
					int d = abs(2 * i - (cnt - 1));
					if (d < bestd) {
						bestd = d;
						best = i;
					}
					break;
				}
		if (best < 0)
			break;
		memmove(rows + best, rows + best + 1, (cnt - best - 1) * sizeof(int));
		cnt--;
	}
	memcpy(tmp, px, w * (bottom + 1));
	for (y = top; y <= bottom; y++)
		memset(px + y * w, GL_EMPTY, w);
	for (y = 0; y < m; y++)
		memcpy(px + (top + need + y) * w, tmp + rows[(y * cnt + cnt / 2) / m] * w, w);
	return 1;
}

// The base glyph with the accent, in place. h is the height of the font.
static void glyph_compose(ubyte *px, int w, int h, int accent)
{
	ubyte tmp[64 * 64];
	int top, bottom, x, y, minx = w, maxx = -1, color = 1, t, ah, gap, cx;
	int counts[256];

	memset(counts, 0, sizeof(counts));
	for (y = 0; y < h; y++)
		for (x = 0; x < w; x++)
			if (px[y * w + x] != GL_EMPTY) {
				counts[px[y * w + x]]++;
				if (x < minx) minx = x;
				if (x > maxx) maxx = x;
			}
	if (maxx < 0)
		return;
	for (x = 0; x < 255; x++)
		if (counts[x] > counts[color])
			color = x;
	for (top = 0; top < h && row_empty(px, w, top); top++)
		;
	for (bottom = h - 1; bottom > top && row_empty(px, w, bottom); bottom--)
		;

	if (accent == ACC_FLIP) {
		// turned by 180 degrees in the box of its pixels
		memcpy(tmp, px, w * h);
		for (y = top; y <= bottom; y++)
			for (x = minx; x <= maxx; x++)
				px[y * w + x] = tmp[(top + bottom - y) * w + (minx + maxx - x)];
		return;
	}

	// The stroke: the shortest run of pixels in the rows of the glyph,
	// 1 to 3 pixels
	t = 64;
	for (y = top; y <= bottom; y++) {
		int run = 0;
		for (x = 0; x <= w; x++) {
			if (x < w && px[y * w + x] != GL_EMPTY)
				run++;
			else {
				if (run > 0 && run < t)
					t = run;
				run = 0;
			}
		}
	}
	if (t > 3) t = 3;
	if (t < 1) t = 1;
	cx = (minx + maxx) / 2;

	if (accent == ACC_CEDIL) {
		// a hook below the letter, where the font has the rows
		if (bottom + 2 < h) {
			for (x = cx; x < cx + t && x < w; x++)
				px[(bottom + 1) * w + x] = color;
			for (x = cx - 1; x < cx + t && x < w; x++)
				if (x >= 0)
					px[(bottom + 2) * w + x] = color;
		}
		return;
	}

	// The dot of i: the rows from the top to the first empty row in the
	// upper half
	for (y = top + 1; y <= (top + bottom) / 2; y++)
		if (row_empty(px, w, y)) {
			int k;
			for (k = top; k < y; k++)
				memset(px + k * w, GL_EMPTY, w);
			while (y < h && row_empty(px, w, y))
				y++;
			top = y;
			break;
		}

	// The height of the accent and the empty row under it
	ah = h <= 8 ? 1 : h <= 12 ? 2 : h <= 18 ? 3 : 4;
	gap = 1;		// an empty row under the accent where there is room
	if (top < ah + gap) {
		// Not enough rows above the letter (a capital). The letter is made
		// lower (its rows are taken evenly) so that a shorter accent and an
		// empty row fit above it, if the letter keeps 5 rows. Else the
		// accent goes into the rows that there are, without the empty row.
		int n = bottom - top + 1, need, m, a2 = ah > 2 ? ah - 1 : ah;
		need = a2 + 1 - top;
		if (n - need >= 5) {
			ah = a2;
			gap = 1;
		} else if (n - need >= 3 && squeeze_rows(px, w, top, bottom, need, 1)) {
			// ASCENT: a small font (FNTEMLS_, capitals of 5 rows): a letter
			// with equal rows (U, O, I) loses them and keeps its shape, so
			// the accent gets the empty row. Else the accent on the top of
			// U reads as an O.
			ah = a2;
			top += need;
			goto draw;
		} else if (top >= ah) {
			gap = 0;
			need = 0;
		} else if (top >= 1) {
			ah = top;
			gap = 0;
			need = 0;
		} else {
			ah = 1;
			gap = 0;
			need = 1;
		}
		m = n - need;
		if (m < 2)
			return;
		if (need > 0) {
			squeeze_rows(px, w, top, bottom, need, 0);		// ASCENT: equal rows first
			top += need;
		}
	}

draw:
	{
		int y0 = top - gap - ah, r, k;

#define PUT(xx, yy) do { int _x = (xx), _y = (yy); if (_x >= 0 && _x < w && _y >= 0 && _y < h) px[_y * w + _x] = color; } while (0)
		switch (accent) {
			case ACC_ACUTE:
			case ACC_GRAVE:
				if (ah == 1 && gap == 0) {
					// ASCENT: one row on the letter: one stroke to the side
					// of the middle, over an empty pixel of the top row when
					// there is one (Á, Ó of FNTEMLS_), so that it does not
					// make the letter taller.
					int s = accent == ACC_ACUTE ? 1 : -1, c = cx + s, j;
					for (j = 0; j < 4; j++) {
						static const int step[4] = { 1, 2, 0, 3 };
						int cc = cx + s * step[j];
						if (cc >= minx && cc <= maxx && px[top * w + cc] == GL_EMPTY) {
							c = cc;
							break;
						}
					}
					for (k = 0; k < t; k++)
						PUT(c + s * k, y0);
					break;
				}
				for (r = 0; r < ah; r++) {
					int d = accent == ACC_ACUTE ? ah - 1 - r : r;
					for (k = 0; k < t + (ah == 1); k++)
						PUT(cx - ah / 2 + d + k, y0 + r);
				}
				break;
			case ACC_CIRC:
				if (ah == 1) {
					for (k = -1; k <= 1; k++)
						PUT(cx + k, y0);
				} else
					for (r = 0; r < ah; r++)
						for (k = 0; k < t; k++) {
							PUT(cx - r - k, y0 + r);
							PUT(cx + r + k, y0 + r);
						}
				break;
			case ACC_DIAER: {
				int space = (maxx - minx + 1) / 3, x1, dh = ah < t ? ah : t;
				if (space < 1) space = 1;
				x1 = cx - space / 2 - t;
				for (r = 0; r < dh; r++)
					for (k = 0; k < t; k++) {
						PUT(x1 + k, y0 + ah - 1 - r);
						PUT(x1 + t + space + k, y0 + ah - 1 - r);
					}
				break;
			}
			case ACC_TILDE: {
				int tw = maxx - minx + 1, x0;
				if (tw > 2 * ah + 4) tw = 2 * ah + 4;
				if (tw < 3) tw = 3;
				x0 = cx - (tw - 1) / 2;
				if (ah == 1) {
					for (k = 0; k < tw; k++)
						PUT(x0 + k, y0);
				} else {
					// row 1: the first half rises, row 2: the second half
					for (k = 1; k <= tw / 2; k++)
						PUT(x0 + k, y0 + ah - 2);
					PUT(x0 + tw - 1, y0 + ah - 2);
					PUT(x0, y0 + ah - 1);
					for (k = tw / 2; k < tw - 1; k++)
						PUT(x0 + k, y0 + ah - 1);
				}
				break;
			}
		}
#undef PUT
	}
}

// ASCENT: the memory of the added glyphs of each font, which
// gr_close_font frees.
#define LATIN1_STORES 16
static struct { grs_font *font; ubyte *store; } Latin1_stores[LATIN1_STORES];

static void latin1_store_free(grs_font *font)
{
	int i;

	for (i = 0; i < LATIN1_STORES; i++)
		if (Latin1_stores[i].font == font) {
			free(Latin1_stores[i].store);
			Latin1_stores[i].font = NULL;
		}
}

// Adds the letters of Latin1_letters to the font (see above).
static void font_add_latin1(grs_font *font)
{
	static ubyte px[64 * 64];
	int i, w, h = font->ft_h, nchars, old_n, total = 0;
	ubyte *store, *p;
	ubyte **chars;

	if (h <= 0 || h > 64 || font->ft_minchar > 'a')
		return;

	// A font with all 256 characters of code page 437 (FNTEMFS_): copy
	// the glyphs in place, and compose the letters that it does not have.
	if (font->ft_maxchar == 255 && !(font->ft_flags & FT_PROPORTIONAL)) {
		int size = ((font->ft_flags & FT_COLOR) ? font->ft_w : BITS_TO_BYTES(font->ft_w)) * h;
		for (i = 0; i < N_LATIN1_LETTERS; i++) {
			int c = Latin1_letters[i].code;
			if (Latin1_letters[i].cp437 && glyph_get(font, Latin1_letters[i].cp437, px, &w))
				;
			else if (glyph_get(font, Latin1_letters[i].base, px, &w))
				glyph_compose(px, w, h, Latin1_letters[i].accent);
			else
				continue;
			glyph_put(font, font->ft_data + (c - font->ft_minchar) * size, px, w);
		}
		return;
	}
	if (font->ft_maxchar >= 0xA1)
		return;		// a font of its own with these characters

	// Else the font goes up to 255. The new characters that are not in
	// the list are drawn as a space.
	old_n = font->ft_maxchar - font->ft_minchar + 1;
	nchars = 256 - font->ft_minchar;
	chars = (ubyte **) malloc(nchars * sizeof(ubyte *));
	if (!chars)
		return;
	if (!(font->ft_flags & FT_PROPORTIONAL)) {
		// a fixed font becomes a proportional font with equal widths
		int size = ((font->ft_flags & FT_COLOR) ? font->ft_w : BITS_TO_BYTES(font->ft_w)) * h;
		font->ft_widths = (short *) (font + 1);		// the table of gr_init_font
		for (i = 0; i < old_n; i++) {
			chars[i] = font->ft_data + i * size;
			font->ft_widths[i] = font->ft_w;
		}
	} else
		for (i = 0; i < old_n; i++)
			chars[i] = font->ft_chars[i];
	for (i = old_n; i < nchars; i++) {
		int sp = ' ' - font->ft_minchar;
		chars[i] = sp >= 0 && sp < old_n ? chars[sp] : chars[0];
		font->ft_widths[i] = sp >= 0 && sp < old_n ? font->ft_widths[sp] : 0;
	}
	if (font->ft_flags & FT_PROPORTIONAL)
		free(font->ft_chars);
	font->ft_chars = chars;
	font->ft_flags |= FT_PROPORTIONAL;
	font->ft_maxchar = 255;

	for (i = 0; i < N_LATIN1_LETTERS; i++)
		if (glyph_get(font, Latin1_letters[i].base, px, &w))
			total += ((font->ft_flags & FT_COLOR) ? w : BITS_TO_BYTES(w)) * h;
	// The memory of the new glyphs stays until gr_close_font.
	store = p = (ubyte *) malloc(total > 0 ? total : 1);
	if (!store)
		return;
	for (i = 0; i < LATIN1_STORES && Latin1_stores[i].font; i++)
		;
	if (i < LATIN1_STORES) {
		Latin1_stores[i].font = font;
		Latin1_stores[i].store = store;
	}
	for (i = 0; i < N_LATIN1_LETTERS; i++) {
		int c = Latin1_letters[i].code;
		if (!glyph_get(font, Latin1_letters[i].base, px, &w))
			continue;
		glyph_compose(px, w, h, Latin1_letters[i].accent);
		glyph_put(font, p, px, w);
		font->ft_chars[c - font->ft_minchar] = p;
		font->ft_widths[c - font->ft_minchar] = w;
		p += ((font->ft_flags & FT_COLOR) ? w : BITS_TO_BYTES(w)) * h;
	}
}

static int font_get_short( ubyte * p )
{
	return (short)(p[0] | (p[1] << 8));
}

static unsigned int font_get_uint( ubyte * p )
{
	return (unsigned int)p[0] | ((unsigned int)p[1] << 8) | ((unsigned int)p[2] << 16) | ((unsigned int)p[3] << 24);
}

grs_font * gr_init_font( char * fontname )
{
	grs_font *font;
	int i;
	unsigned char * ptr;
	int nchars;
	CFILE *fontfile;
	ubyte file_header[8];
	ubyte * raw;				// the font data, as it is in the file
	short * widths;
	unsigned int datasize;		//size up to (but not including) palette
	unsigned int data_offset, widths_offset, kern_offset;

	fontfile = cfopen(fontname, "rb");

	if (!fontfile)
		Error( "Can't open font file %s", fontname );

	if (cfread(file_header,1,8,fontfile) != 8)
		Error( "File %s is not a font file", fontname );

	datasize = font_get_uint(file_header+4);

	if ( memcmp(file_header,"PSFN",4) != 0 || datasize < FONT_FILE_HEADER_SIZE || datasize > 0x1000000 )
		Error( "File %s is not a font file", fontname );

	// One block of memory: the grs_font, then the table of widths (with
	// the alignment of the machine), then the font data.
	// gr_close_font frees the block.
	font = (grs_font *) malloc(sizeof(grs_font) + FONT_MAX_CHARS*sizeof(short) + datasize);
	widths = (short *)(font+1);
	raw = (ubyte *)(widths + FONT_MAX_CHARS);

	if (cfread(raw,1,datasize,fontfile) != datasize)
		Error( "Font file %s is too short", fontname );

	font->ft_w = font_get_short(raw+0);
	font->ft_h = font_get_short(raw+2);
	font->ft_flags = font_get_short(raw+4);
	font->ft_baseline = font_get_short(raw+6);
	font->ft_minchar = raw[8];
	font->ft_maxchar = raw[9];
	font->ft_bytewidth = font_get_short(raw+10);
	data_offset = font_get_uint(raw+12);
	widths_offset = font_get_uint(raw+20);
	kern_offset = font_get_uint(raw+24);

	nchars = font->ft_maxchar-font->ft_minchar+1;

	if (nchars < 1)
		Error( "Font file %s has no characters", fontname );

	if (font->ft_flags & FT_PROPORTIONAL) {

		if ( data_offset > datasize || widths_offset > datasize || nchars*2 > datasize-widths_offset )
			Error( "Font file %s has a bad header", fontname );

		for (i=0; i< nchars; i++ )
			widths[i] = font_get_short(raw+widths_offset+i*2);

		font->ft_widths = widths;

		font->ft_data = raw + data_offset;

		font->ft_chars = (unsigned char **)malloc( nchars * sizeof(unsigned char *));

		ptr = font->ft_data;

		for (i=0; i< nchars; i++ ) {
			font->ft_chars[i] = ptr;
			if (font->ft_flags & FT_COLOR)
				ptr += font->ft_widths[i] * font->ft_h;
			else
				ptr += BITS_TO_BYTES(font->ft_widths[i]) * font->ft_h;
		}

	} else  {

		font->ft_data = raw + FONT_FILE_HEADER_SIZE;

		font->ft_chars	= NULL;
		font->ft_widths = NULL;

		ptr = font->ft_data + (nchars * font->ft_w * font->ft_h);
	}

	if (font->ft_flags & FT_KERNED) {
		if ( kern_offset >= datasize )
			Error( "Font file %s has a bad header", fontname );
		font->ft_kerndata = raw + kern_offset;
	} else
		font->ft_kerndata = NULL;		// ASCENT: the original kept the value of the file


	if (font->ft_flags & FT_COLOR) {		//remap palette
		ubyte palette[256*3];
		ubyte colormap[256];
		int freq[256];

		// ASCENT: the color bitmaps must be inside the font data.
		if ( ptr > raw + datasize )
			Error( "Font file %s is too short", fontname );

		cfread(palette,3,256,fontfile);		//read the palette

		build_colormap_good( palette, colormap, freq );

		colormap[255] = 255;

		decode_data_asm(font->ft_data, ptr-font->ft_data, colormap, freq );
	}

	cfclose(fontfile);

	font_add_latin1(font);		// ASCENT: the letters of the translations

	//set curcanv vars

	FONT        = font;
	FG_COLOR    = 0;
	BG_COLOR    = 0;

	return font;

}


void gr_set_fontcolor( int fg, int bg )
{
	FG_COLOR    = fg;
	BG_COLOR    = bg;
}

void gr_set_curfont( grs_font * new )
{
	FONT = new;
}


int gr_internal_string_clipped(int x, int y, char *s )
{
	unsigned char * fp;
	char * text_ptr, * next_row, * text_ptr1;
	int r, BitMask, i, bits, width, spacing, letter, underline;
	int x1 = x, last_x;
	
	next_row = s;

	while (next_row != NULL )
	{
		text_ptr1 = next_row;
		next_row = NULL;

		x = x1;
		if (x==0x8000)			//centered
			x = get_centered_x(text_ptr1);

		last_x = x;

		for (r=0; r<FHEIGHT; r++)	{
			text_ptr = text_ptr1;
			x = last_x;

			while (*text_ptr)	{
				if (*text_ptr == '\n' )	{
					next_row = &text_ptr[1];
					break;
				}

				underline = 0;
				if (*text_ptr == '&' )	{
					if ((r==FBASELINE+2) || (r==FBASELINE+3))
						underline = 1;
					text_ptr++;
				}

				get_char_width(text_ptr[0],text_ptr[1],&width,&spacing);

				letter = (ubyte) *text_ptr - FMINCHAR;	// ASCENT: unsigned, for the Latin-1 letters

				if (!INFONT(letter)) {	//not in font, draw as space
					x += spacing;
					text_ptr++;
					continue;
				}

				if (FFLAGS & FT_PROPORTIONAL)
					fp = FCHARS[letter];
				else
					fp = FDATA + letter * BITS_TO_BYTES(width)*FHEIGHT;

				if (underline)	{
					for (i=0; i< width; i++ )	{
						gr_setcolor(FG_COLOR);
						gr_pixel( x++, y );						
					}
				} else {
					fp += BITS_TO_BYTES(width)*r;

					BitMask = 0;

					for (i=0; i< width; i++ )	{
						if (BitMask==0) {
							bits = *fp++;
							BitMask = 0x80;
						}
						if (bits & BitMask)	
							gr_setcolor(FG_COLOR);
						else
							gr_setcolor(BG_COLOR);
						gr_pixel( x++, y );						
						BitMask >>= 1;
					}
				}

				x += spacing-width;		//for kerning

				text_ptr++;
			}
			y++;
		}
	}
	return 0;
}

int gr_internal_string_clipped_m(int x, int y, char *s )
{
	unsigned char * fp;
	char * text_ptr, * next_row, * text_ptr1;
	int r, BitMask, i, bits, width, spacing, letter, underline;
	int x1 = x, last_x;
	
	next_row = s;

	while (next_row != NULL )
	{
		text_ptr1 = next_row;
		next_row = NULL;

		x = x1;
		if (x==0x8000)			//centered
			x = get_centered_x(text_ptr1);

		last_x = x;

		for (r=0; r<FHEIGHT; r++)	{
			x = last_x;

			text_ptr = text_ptr1;

			while (*text_ptr)	{
				if (*text_ptr == '\n' )	{
					next_row = &text_ptr[1];
					break;
				}

				underline = 0;
				if (*text_ptr == '&' )	{
					if ((r==FBASELINE+2) || (r==FBASELINE+3))
						underline = 1;
					text_ptr++;
				}

				get_char_width(text_ptr[0],text_ptr[1],&width,&spacing);

				letter = (ubyte) *text_ptr - FMINCHAR;	// ASCENT: unsigned, for the Latin-1 letters

				if (!INFONT(letter)) {	//not in font, draw as space
					x += spacing;
					text_ptr++;
					continue;
				}

				if (FFLAGS & FT_PROPORTIONAL)
					fp = FCHARS[letter];
				else
					fp = FDATA + letter * BITS_TO_BYTES(width)*FHEIGHT;

				if (underline)	{
					for (i=0; i< width; i++ )	{
						gr_setcolor(FG_COLOR);
						gr_pixel( x++, y );						
					}
				} else {
					fp += BITS_TO_BYTES(width)*r;

					BitMask = 0;

					for (i=0; i< width; i++ )	{
						if (BitMask==0) {
							bits = *fp++;
							BitMask = 0x80;
						}
						if (bits & BitMask)	{
							gr_setcolor(FG_COLOR);
							gr_pixel( x++, y );
						} else {
							x++;
						}						
						BitMask >>= 1;
					}
				}

				x += spacing-width;		//for kerning

				text_ptr++;
			}
			y++;
		}
	}
	return 0;
}

