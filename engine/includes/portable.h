/*
 * ASCENT: the functions of the Watcom C and Windows C libraries that the
 * code calls and that the C library of Linux does not have.
 *
 * This file is new. The build includes it in each file of the engine on
 * the systems other than Windows (the -include option of d_common in
 * engine/CMakeLists.txt), so the original code needs no edit. On Windows
 * the C library has the functions and the file is empty.
 */

#ifndef _PORTABLE_H
#define _PORTABLE_H

#ifndef _WIN32

#include <string.h>
#include <strings.h>	// strcasecmp, strncasecmp
#include <ctype.h>

// The rand() of Watcom C and of the C library of Windows gives 0 to
// 0x7FFF. The code depends on it: rand() * 4 as a fix time, rand() > 0x7400
// as a chance of 1 in 10, (rand() * n) >> 15 as an index. The rand() of the
// GNU C library gives 0 to 0x7FFFFFFF. stdlib.h comes first, so that its
// RAND_MAX does not replace this one.
#include <stdlib.h>
#undef RAND_MAX
#define RAND_MAX 0x7FFF
#define rand() (rand() & RAND_MAX)

#define stricmp strcasecmp
#define strnicmp strncasecmp
#define strcmpi strcasecmp

static inline char *strlwr(char *s)
{
	char *p;

	for (p = s; *p; p++)
		*p = (char) tolower((unsigned char) *p);
	return s;
}

static inline char *strupr(char *s)
{
	char *p;

	for (p = s; *p; p++)
		*p = (char) toupper((unsigned char) *p);
	return s;
}

// The text of value in the base radix (2 to 36). As in Watcom C, only base
// 10 has a sign; the other bases show the bits of an unsigned value.
static inline char *itoa(int value, char *buf, int radix)
{
	char tmp[34];
	unsigned int v;
	int n = 0, neg = 0;
	char *p = buf;

	if (radix < 2 || radix > 36) {
		buf[0] = 0;
		return buf;
	}
	if (radix == 10 && value < 0) {
		neg = 1;
		v = 0u - (unsigned int) value;
	} else
		v = (unsigned int) value;
	do {
		int d = (int) (v % (unsigned int) radix);
		tmp[n++] = (char) (d < 10 ? '0' + d : 'a' + d - 10);
		v /= (unsigned int) radix;
	} while (v != 0);
	if (neg)
		*p++ = '-';
	while (n > 0)
		*p++ = tmp[--n];
	*p = 0;
	return buf;
}

#endif	// _WIN32

#endif	// _PORTABLE_H
