/*
 * ASCENT: new file. The Ogg Vorbis decoder for the replacement music
 * (dtumusic.c): the implementation of stb_vorbis, public domain or MIT
 * (engine/third_party/stb/stb_vorbis.c, LICENSE.stb_vorbis). It replaces
 * nothing of the original.
 */
#define STB_VORBIS_NO_STDIO
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wunused-value"
#pragma GCC diagnostic ignored "-Wsign-compare"
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#endif
#include "../third_party/stb/stb_vorbis.c"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
