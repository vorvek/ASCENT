/*
 * ASCENT: new file. The streamed music of Descent to Undermountain.
 *
 * It replaces the MIDI songs of Descent 1 (digi_play_midi_song and the
 * song table of songs.c) and it follows the source file str_mus.c of the
 * original game. docs/re/music.md has the addresses and says what is
 * verified. The comments below give the address of the original function.
 *
 * The music is in the NAME.TUN files, one Interplay ACM stream each
 * (docs/formats/tun.md). DISCO.JRG gives a number from 0 to 39 to 37 of
 * them. The game code and the warp table DTU.WRP name a track by that
 * number.
 *
 * Threads. The game thread calls all functions of this file but one:
 * music_callback runs on the audio thread, called by the platform mixer.
 * The two threads share one music_player. The game thread fills it while
 * the mixer has no music source, gives it to the mixer with
 * plat_audio_music_start, and reads or releases it again only after
 * plat_audio_music_stop, which returns when the callback cannot run any
 * more. While the callback can run, the game thread only writes the flag
 * no_more_loops and reads the flag done and the counters.
 *
 * The compressed file is in memory (0.2 to 6.4 MB). The callback decodes
 * it block by block into a ring buffer of RING_FRAMES frames when the
 * mixer asks for data, so no file is read on the audio thread and the
 * supply of data does not depend on the frame rate of the game.
 *
 * ASCENT: replacement music. With the setting Dtu_safe_music on, the file
 * <hd>/music/NAME.ogg (Ogg Vorbis) plays in place of NAME.TUN if it
 * exists (dtu_hd_dir of dtuopts.c). It goes through the same player,
 * queue, loop and fade; stb_vorbis decodes it block by block in the
 * callback, as the ACM decoder does. The mixer resamples it from its own
 * rate. tools/music_replace.py matches its loudness to the original track
 * when it makes the file, so the player applies no gain.
 *
 * ASCENT: the music at the start of a level (Dtu_fix_start_music), see
 * start_music below and docs/re/music.md.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "types.h"
#include "fix.h"
#include "mem.h"
#include "args.h"
#include "timer.h"
#include "segment.h"
#include "object.h"
#include "acm.h"
#include "dtufile.h"
#include "dtumusic.h"
#include "dtuopts.h"
#include "platform.h"

// stb_vorbis (public domain, engine/third_party/stb). dtuogg.c compiles
// the implementation.
#define STB_VORBIS_HEADER_ONLY
#define STB_VORBIS_NO_STDIO
#include "../third_party/stb/stb_vorbis.c"

#define RING_FRAMES		4096			// stereo frames of decoded music
#define QUEUE_SIZE		8				// 0xA3877: three queues of 8 entries
#define ACM_HEADER_SIZE	14

// 0x13ACB4: the volume of the sound driver for each value of the music
// volume setting.
static const int Volume_table[9] = { 0, 0x1000, 0x2000, 0x3000, 0x4000, 0x5000, 0x6000, 0x7000, 0x7fff };

#define FADE_TICK			(F1_0/10)	// 0xA4FF5: a timer event of 10 Hz
#define FADE_STEP_UP		512			// 0x1454F4
#define FADE_STEP_DOWN	2048			// 0x1454F8

typedef struct disco_entry {
	char		name[16];
	uint		value;						// not used by the original (see music.md)
} disco_entry;

typedef struct music_player {
	ubyte			*file;					// the compressed file
	int			file_size;
	int			file_pos;
	acm_stream	*acm;
	stb_vorbis	*ogg;					// ASCENT: a replacement file, in place of acm
	acm_info		info;					// rate and channels (also of the ogg file)
	int			pass_frames;			// frames decoded in this pass
	int			loops;					// repeats that remain, -1 = no limit
	volatile int	no_more_loops;		// the game thread sets it: do not repeat again
	volatile int	done;					// the callback sets it: the last pass has ended
	short			ring[RING_FRAMES*2];
	int			ring_read, ring_count;	// in frames
	// Counters for the log.
	volatile unsigned	frames_out;		// frames given to the mixer
	volatile int	passes;				// times that the track started again
	volatile int	underruns;			// frames that the callback could not give
	volatile int	calls;
} music_player;

static disco_entry Disco[DTUMUSIC_STREAMS];
static int Music_ready = 0;				// DISCO.JRG is loaded and the music is on
static int Atexit_done = 0;

static music_player Player;
static int Playing = 0;					// the mixer has the player as its source
static int Cur_index = -1;				// 0x1454B8
static int Cur_volume = 0;				// 0x3CE044, from 0 to 0x7fff
static int Fade_target = -1;			// 0x1454F0, -1 = no fade
static fix Fade_time;					// time of the last fade step
static int Volume_level = 8;			// 0x13BFEC, from 0 to 8

static int Queue_index[QUEUE_SIZE];	// 0x1454E0
static int Queue_loops[QUEUE_SIZE];	// 0x1454E4
static int Queue_volume[QUEUE_SIZE];	// 0x1454E8
static int Queue_count = 0;

// The music commands of DTU.WRP for the segments of the level. code 0: none.
static short Wrp_code[MAX_SEGMENTS];
static ubyte Wrp_value[MAX_SEGMENTS];
static int Wrp_last_seg = -1;

// ASCENT: the music at the start of a level, see start_music. The option
// -origmusic clears it: the level is silent until the player enters a
// segment with a music command, as in the original.
int Dtu_fix_start_music = 1;
static int Start_pending = 0;			// dtumusic_level_start ran, no game frame since
static fix Start_gametime;

// ASCENT: -safemusic and -nosafemusic set Dtu_safe_music for this run
// only (-1: the value of the setting). Safe_used is the value with which
// the track that plays now was opened.
static int Safe_override = -1;
static int Safe_used = 0;

// Totals for the line that dtumusic_close prints.
static unsigned Total_frames;
static int Total_underruns, Total_tracks, Total_passes;

// The debug options. -musiclog prints the start and the stop of each track
// with the time. -musicscript T:CODE:VALUE[,...] gives the music command
// CODE with VALUE at T seconds of game time, as a segment of DTU.WRP does.
#define SCRIPT_MAX	16
static int Music_log = 0;
static struct { fix time; int code, value; } Script[SCRIPT_MAX];
static int Script_count = 0, Script_next = 0;
extern fix GameTime;

//-----------------------------------------------------------------------
// The audio thread.

static int player_read(void *user, ubyte *buf, int size)
{
	music_player *p = (music_player *)user;
	int n = p->file_size - p->file_pos;

	if ( n > size ) n = size;
	if ( n <= 0 ) return 0;
	memcpy( buf, p->file + p->file_pos, n );
	p->file_pos += n;
	return n;
}

// Decode the next part of the track into the ring buffer. At the end of
// the track it starts again at the first sample, with a new decoder state,
// if the track repeats. The original starts the same file again when its
// service function sees that the sample is done (0xA46FD).
static void player_fill(music_player *p)
{
	short mono[512];
	int pos, room, n, i;

	while ( p->ring_count < RING_FRAMES && !p->done ) {
		pos = (p->ring_read + p->ring_count) % RING_FRAMES;
		room = RING_FRAMES - p->ring_count;
		if ( room > RING_FRAMES - pos )
			room = RING_FRAMES - pos;

		if ( p->ogg )
			n = stb_vorbis_get_samples_short_interleaved( p->ogg, 2, p->ring + pos*2, room*2 );
		else if ( p->info.channels == 2 )
			n = acm_stream_read( p->acm, p->ring + pos*2, room*2 ) / 2;
		else {
			if ( room > 512 ) room = 512;
			n = acm_stream_read( p->acm, mono, room );
			for (i=0; i<n; i++ )
				p->ring[(pos+i)*2] = p->ring[(pos+i)*2+1] = mono[i];
		}
		p->ring_count += n;
		p->pass_frames += n;

		if ( n < room ) {
			// The end of the track, or a stream that is not valid. A pass
			// without a frame is an error too (no endless loop).
			if ( (p->acm && acm_stream_error( p->acm )) || p->pass_frames == 0 || p->no_more_loops || p->loops == 0 )
				p->done = 1;
			else {
				if ( p->loops > 0 )
					p->loops--;
				if ( p->ogg )
					stb_vorbis_seek_start( p->ogg );
				else {
					p->file_pos = ACM_HEADER_SIZE;
					acm_stream_rewind( p->acm );
				}
				p->pass_frames = 0;
				p->passes++;
			}
		}
	}
}

// The source of the mixer: frames of signed 16-bit stereo data.
static int music_callback(void *user, int16_t *buffer, int frames)
{
	music_player *p = (music_player *)user;
	int got = 0, n;

	p->calls++;
	while ( got < frames ) {
		if ( p->ring_count == 0 ) {
			player_fill( p );
			if ( p->ring_count == 0 )
				break;
		}
		n = frames - got;
		if ( n > p->ring_count ) n = p->ring_count;
		if ( n > RING_FRAMES - p->ring_read ) n = RING_FRAMES - p->ring_read;
		memcpy( buffer + got*2, p->ring + p->ring_read*2, n * 2 * sizeof(short) );
		p->ring_read = (p->ring_read + n) % RING_FRAMES;
		p->ring_count -= n;
		got += n;
	}
	p->frames_out += got;
	if ( got < frames && !p->done )
		p->underruns += frames - got;
	return got;
}

//-----------------------------------------------------------------------
// The game thread.

// ASCENT: the options -safemusic and -nosafemusic (for a test; they do not
// change the saved setting).
static void safe_music_args(void)
{
	if ( FindArg( "-safemusic" ) )
		Safe_override = 1;
	if ( FindArg( "-nosafemusic" ) )
		Safe_override = 0;
}

static int safe_music_on(void)
{
	return Safe_override >= 0 ? Safe_override : Dtu_safe_music;
}

// ASCENT: load a whole file by its path. NULL if it cannot be read.
static ubyte *load_path(const char *path, int *size_out)
{
	FILE *f = fopen( path, "rb" );
	ubyte *data;
	long n;

	if ( f == NULL )
		return NULL;
	fseek( f, 0, SEEK_END );
	n = ftell( f );
	fseek( f, 0, SEEK_SET );
	data = (n > 0 && n < 0x10000000) ? malloc( n ) : NULL;
	if ( data && fread( data, 1, n, f ) != (size_t)n ) {
		free( data );
		data = NULL;
	}
	fclose( f );
	*size_out = (int)n;
	return data;
}

// ASCENT: the replacement of a track, <hd>/music/NAME.ogg, where NAME is
// the name of the TUN file without its extension. The name is tried in
// upper case, as written in DISCO.JRG and in lower case (for file systems
// with case). Returns the file in memory, or NULL; path gets its name.
static ubyte *load_replacement(const char *name, int *size, char *path, int path_size)
{
	const char *hd = dtu_hd_dir();
	char base[32];
	ubyte *data;
	int i, n, form;

	if ( hd == NULL )
		return NULL;
	for (n=0; name[n] && name[n] != '.' && n < (int)sizeof(base)-1; n++ )
		;
	for (form=0; form<3; form++ ) {
		for (i=0; i<n; i++ ) {
			base[i] = name[i];
			if ( form == 0 && base[i] >= 'a' && base[i] <= 'z' ) base[i] -= 'a' - 'A';
			if ( form == 2 && base[i] >= 'A' && base[i] <= 'Z' ) base[i] += 'a' - 'A';
		}
		base[n] = 0;
		snprintf( path, path_size, "%s/music/%s.ogg", hd, base );
		if ( (data = load_path( path, size )) != NULL )
			return data;
	}
	return NULL;
}

// ASCENT: open the replacement of a track. Returns 1 if it can play.
static int player_open_ogg(music_player *p, const char *name)
{
	char path[700];
	stb_vorbis_info vi;
	int error = 0;

	p->file = load_replacement( name, &p->file_size, path, sizeof(path) );
	if ( p->file == NULL )
		return 0;
	p->ogg = stb_vorbis_open_memory( p->file, p->file_size, &error, NULL );
	if ( p->ogg == NULL ) {
		dtu_log( "MUSIC: '%s' is not an Ogg Vorbis file (error %d)\n", path, error );
		free( p->file );
		p->file = NULL;
		return 0;
	}
	vi = stb_vorbis_get_info( p->ogg );
	p->info.rate = vi.sample_rate;
	p->info.channels = vi.channels;
	p->info.total = (int)stb_vorbis_stream_length_in_samples( p->ogg ) * vi.channels;
	dtu_log( "MUSIC: replacement '%s' for '%s', %d Hz, %d channels\n", path, name, p->info.rate, p->info.channels );
	if ( Music_log )
		printf( "MUSIC: replacement '%s' for '%s': %d Hz, %d channels, %d frames\n",
			path, name, p->info.rate, p->info.channels, vi.channels > 0 ? p->info.total / vi.channels : 0 );
	return 1;
}

// Load a track and make the player ready. The mixer does not have the
// player now. Returns 1 if the track can play.
static int player_open(music_player *p, const char *name, int loops)
{
	memset( p, 0, sizeof(*p) );
	// ASCENT: the replacement of the track, if the setting is on.
	if ( safe_music_on() && player_open_ogg( p, name ) ) {
		p->loops = loops;
		return 1;
	}
	p->file = dtu_load_file( name, &p->file_size );
	if ( p->file == NULL ) {
		dtu_log( "MUSIC: the file '%s' is not in the data directories\n", name );
		return 0;
	}
	p->acm = acm_stream_open( player_read, p, &p->info );
	if ( p->acm == NULL || p->info.rate <= 0 ) {
		dtu_log( "MUSIC: '%s' is not an ACM stream\n", name );
		if ( p->acm ) acm_stream_close( p->acm );
		p->acm = NULL;
		free( p->file );
		return 0;
	}
	p->loops = loops;
	return 1;
}

static void player_close(music_player *p)
{
	if ( p->acm ) acm_stream_close( p->acm );
	p->acm = NULL;
	if ( p->ogg ) stb_vorbis_close( p->ogg );
	p->ogg = NULL;
	if ( p->file ) free( p->file );
	p->file = NULL;
}

// 0xA40B3: start a job.
static void start_job(int index, int loops, int volume)
{
	Fade_target = -1;
	Cur_volume = volume;

	Safe_used = safe_music_on();
	if ( !player_open( &Player, Disco[index].name, loops ) )
		return;
	Cur_index = index;
	Playing = 1;
	Total_tracks++;
	plat_audio_music_set_volume( Cur_volume );
	plat_audio_music_start( music_callback, &Player, Player.info.rate );
	dtu_log( "MUSIC: start %d '%s'%s, loops %d, volume %d\n", index, Disco[index].name,
		Player.ogg ? " (replacement)" : "", loops, volume );
	if ( Music_log )
		printf( "MUSIC: t %.2f start %d '%s'%s, loops %d, volume %d\n", f2fl(GameTime), index, Disco[index].name,
			Player.ogg ? " (replacement)" : "", loops, volume );
}

// 0xA3DB9: stop the job.
void dtumusic_stop(void)
{
	Fade_target = -1;
	if ( !Playing )
		return;
	plat_audio_music_stop();		// the callback does not run after this call
	Playing = 0;
	dtu_log( "MUSIC: stop %d '%s' after %u frames, %d repeats, %d underruns\n", Cur_index,
		Disco[Cur_index].name, Player.frames_out, Player.passes, Player.underruns );
	if ( Music_log )
		printf( "MUSIC: t %.2f stop %d '%s' after %u frames (%.2f s), %d repeats, %d underruns, volume %d\n", f2fl(GameTime), Cur_index,
			Disco[Cur_index].name, Player.frames_out, (double)Player.frames_out / Player.info.rate, Player.passes, Player.underruns, Cur_volume );
	Total_frames += Player.frames_out;
	Total_underruns += Player.underruns;
	Total_passes += Player.passes;
	player_close( &Player );
}

int dtumusic_enabled(void)
{
	return Music_ready;
}

int dtumusic_current(void)
{
	if ( !Music_ready || !Playing || Player.done )
		return -1;
	return Cur_index;
}

int dtumusic_is_queued(int index)
{
	int i;

	for (i=0; i<Queue_count; i++ )
		if ( Queue_index[i] == index )
			return 1;
	return 0;
}

void dtumusic_flush(void)
{
	if ( !Music_ready )
		return;
	if ( Playing )
		Player.no_more_loops = 1;
	Queue_count = 0;
}

static int level_volume(int percent)
{
	if ( percent < 0 ) percent = 0;
	if ( percent > 100 ) percent = 100;
	return Volume_table[Volume_level] * percent / 100;
}

int dtumusic_play(int index, int loops, int volume)
{
	int i;

	if ( !Music_ready )
		return 1;
	if ( index < 0 || index >= DTUMUSIC_STREAMS || loops < -1 )
		return 1;
	if ( Disco[index].name[0] == 0 )		// entry 3 has no name
		return 1;
	volume = level_volume( volume );
	if ( volume < 1 )
		return 1;
	if ( Queue_count >= QUEUE_SIZE )
		return 1;

	// The tracks that wait and the track that plays do not repeat again.
	if ( Queue_count > 0 ) {
		for (i=0; i<Queue_count; i++ )
			Queue_loops[i] = 0;
	} else if ( Playing )
		Player.no_more_loops = 1;

	Queue_index[Queue_count] = index;
	Queue_volume[Queue_count] = volume;
	Queue_loops[Queue_count] = loops;
	Queue_count++;
	return 0;
}

void dtumusic_fade(int volume)
{
	if ( !Music_ready )
		return;
	Fade_target = level_volume( volume );
	Fade_time = timer_get_fixed_seconds();
}

void dtumusic_set_volume_level(int level)
{
	if ( level < 0 ) level = 0;
	if ( level > 8 ) level = 8;
	if ( level == Volume_level )
		return;
	Volume_level = level;
	dtumusic_fade( 100 );		// 0x58C0C
}

// 0xA45AE: one step of the fade.
static void fade_step(void)
{
	if ( Fade_target < 0 )
		return;
	if ( Fade_target == Cur_volume || !Playing ) {		// 0xA456A
		Fade_target = -1;
		return;
	}

	if ( Fade_target > Cur_volume ) {
		Cur_volume += FADE_STEP_UP;
		if ( Cur_volume > Fade_target )
			Cur_volume = Fade_target;
	} else {
		Cur_volume -= FADE_STEP_DOWN;
		if ( Cur_volume < Fade_target )
			Cur_volume = Fade_target;
	}

	if ( Cur_volume > 0 )
		plat_audio_music_set_volume( Cur_volume );
	else {
		dtumusic_stop();
		return;
	}
	if ( Fade_target == Cur_volume )
		Fade_target = -1;
}

void dtumusic_command(int code, int value)
{
	int bits, loops = 0;

	if ( code < 100 || code >= 199 )
		return;
	bits = code - 100;
	value &= 0xff;

	if ( (bits & 0x10) && dtumusic_current() == value )
		return;
	if ( (bits & 8) && dtumusic_is_queued( value ) )
		return;
	if ( Music_log )
		printf( "MUSIC: t %.2f command %d value %d\n", f2fl(GameTime), code, value );
	if ( bits & 0x20 )
		loops = DTUMUSIC_LOOP_FOREVER;
	if ( bits & 2 )
		dtumusic_flush();
	if ( bits & 1 )
		dtumusic_play( value, loops, 100 );
	if ( bits & 4 )
		dtumusic_fade( 0 );
}

// 0x599C3: the lines of DTU.WRP for one level. A line is
// "level,segment dest_level,dest_segment value". The table of the original
// has one entry for each segment, and a later line for the same segment
// replaces the earlier one. This function keeps only the music commands:
// dest_level -2 and a command code from 100 to 198.
void dtumusic_level_start(int level_index)
{
	char *text, *p, *end, line[104];
	int size, n, level, seg, dest_level, code, value, count = 0, t;

	memset( Wrp_code, 0, sizeof(Wrp_code) );
	Wrp_last_seg = -1;

	if ( !Music_ready )
		return;

	text = (char *)dtu_load_file( "DTU.WRP", &size );
	if ( text != NULL ) {
		p = text;
		end = text + size;
		while ( p < end ) {
			n = 0;
			while ( p < end && *p != '\n' ) {
				if ( n < 100 ) line[n++] = *p;
				p++;
			}
			p++;
			line[n] = 0;
			value = 0;
			if ( sscanf( line, "%d,%d %d,%d %d", &level, &seg, &dest_level, &code, &value ) < 4 )
				continue;
			if ( (level & 0xff) != level_index || seg < 0 || seg >= MAX_SEGMENTS )
				continue;
			Wrp_code[seg] = 0;
			if ( dest_level == -2 && code >= 100 && code < 199 ) {
				Wrp_code[seg] = (short)code;
				Wrp_value[seg] = (ubyte)value;
				count++;
			}
		}
		free( text );
	} else
		dtu_log( "MUSIC: the file DTU.WRP is not in the data directories\n" );
	dtu_log( "MUSIC: level %d has %d music commands in DTU.WRP\n", level_index, count );

	// ASCENT: the option -music N plays entry N of DISCO.JRG from the start
	// of the level, for a test by ear. The original starts no music here:
	// the first music command of a segment starts it.
	if ( (t = FindArg( "-music" )) != 0 && t+1 < Num_args ) {
		dtumusic_flush();
		dtumusic_play( atoi( Args[t+1] ), DTUMUSIC_LOOP_FOREVER, 100 );
	} else {
		// ASCENT: start_music runs at the first game frame, when the
		// player is at the start position (also after a load).
		Start_pending = 1;
		Start_gametime = GameTime;
	}
}

// ASCENT: the music at the start of a level (Dtu_fix_start_music).
//
// The original runs a music command of DTU.WRP only when the player moves
// into a segment that has one (0x69BC8 compares the segment after the move
// with the segment before it). Nothing runs for the segment in which the
// player starts, and the level load starts no music. So a level is silent
// until the player crosses a segment with a command; in YPINN the commands
// are in the doors of the rooms (segments 3, 28, 22), not in the room
// where the player starts.
//
// The correction runs, at the first game frame of the level, the command
// of the start segment, or else the command of the nearest segment that
// has one (breadth first search through the connected segments, the
// first found at the smallest number of steps). If that command starts a
// track and another track plays now (from the level before), the other
// track fades out (as code 106 does) so that the new one starts in 1.6 s
// and not at the end of the old one.
static void start_music(int start_seg)
{
	static short queue[MAX_SEGMENTS];
	static short dist[MAX_SEGMENTS];
	int head = 0, tail = 0, s, side, c, found = -1, code, value, cur;

	if ( start_seg < 0 || start_seg > Highest_segment_index )
		return;
	memset( dist, -1, sizeof(dist) );
	dist[start_seg] = 0;
	queue[tail++] = (short)start_seg;
	while ( head < tail ) {
		s = queue[head++];
		if ( Wrp_code[s] ) {
			found = s;
			break;
		}
		for (side=0; side<MAX_SIDES_PER_SEGMENT; side++ ) {
			c = Segments[s].children[side];
			if ( c >= 0 && c <= Highest_segment_index && dist[c] < 0 ) {
				dist[c] = dist[s] + 1;
				queue[tail++] = (short)c;
			}
		}
	}

	if ( Music_log ) {
		if ( found < 0 )
			printf( "MUSIC: t %.2f level start, segment %d, no music command in %d segments that connect\n",
				f2fl(GameTime), start_seg, tail );
		else
			printf( "MUSIC: t %.2f level start, segment %d, command %d value %d of segment %d at %d steps%s\n",
				f2fl(GameTime), start_seg, Wrp_code[found], Wrp_value[found], found, dist[found],
				Dtu_fix_start_music ? "" : " (not run: -origmusic)" );
	}
	if ( found < 0 || !Dtu_fix_start_music )
		return;

	code = Wrp_code[found];
	value = Wrp_value[found];
	cur = dtumusic_current();
	dtumusic_command( code, value );
	if ( ((code - 100) & 1) && cur >= 0 && cur != value )
		dtumusic_fade( 0 );
}

void dtumusic_frame(void)
{
	fix now;
	int seg, i, index, loops, volume;

	if ( !Music_ready )
		return;

	// 0x69BC8: the command of the segment that the player entered.
	if ( ConsoleObject != NULL ) {
		seg = ConsoleObject->segnum;
		// ASCENT: the first game frame of the level: GameTime moves only
		// in the game loop, not in the front end, which also calls this
		// function.
		if ( Start_pending && GameTime != Start_gametime && seg >= 0 && seg < MAX_SEGMENTS ) {
			Start_pending = 0;
			start_music( seg );
			Wrp_last_seg = seg;
		}
		if ( seg >= 0 && seg < MAX_SEGMENTS && seg != Wrp_last_seg ) {
			if ( Wrp_last_seg >= 0 && Wrp_code[seg] )
				dtumusic_command( Wrp_code[seg], Wrp_value[seg] );
			Wrp_last_seg = seg;
		}
	}

	while ( Script_next < Script_count && GameTime >= Script[Script_next].time ) {
		dtumusic_command( Script[Script_next].code, Script[Script_next].value );
		Script_next++;
	}

	// 0xA4FAD: the fade, ten steps each second.
	now = timer_get_fixed_seconds();
	if ( Fade_target >= 0 ) {
		if ( now < Fade_time || now - Fade_time > F1_0 )		// the timer rolled over, or a long stop
			Fade_time = now - FADE_TICK;
		while ( Fade_target >= 0 && now - Fade_time >= FADE_TICK ) {
			Fade_time += FADE_TICK;
			fade_step();
		}
	}

	// ASCENT: the setting "Streaming safe music" changed in a menu: the
	// track that plays starts again from its start in the other form, if
	// it has a replacement.
	if ( Playing && !Player.done && Fade_target < 0 && safe_music_on() != Safe_used ) {
		char path[700];
		int size;
		ubyte *data = load_replacement( Disco[Cur_index].name, &size, path, sizeof(path) );

		Safe_used = safe_music_on();
		if ( data != NULL ) {
			free( data );
			index = Cur_index;
			loops = Player.no_more_loops ? 0 : Player.loops;
			volume = Cur_volume;
			dtumusic_stop();
			start_job( index, loops, volume );
		}
	}

	// 0xA46A9: the track has ended, and the next track of the queue.
	if ( Playing && Player.done && Player.ring_count == 0 )
		dtumusic_stop();
	if ( !Playing && Queue_count > 0 ) {
		index = Queue_index[0];
		loops = Queue_loops[0];
		volume = Queue_volume[0];
		Queue_count--;
		for (i=0; i<Queue_count; i++ ) {
			Queue_index[i] = Queue_index[i+1];
			Queue_loops[i] = Queue_loops[i+1];
			Queue_volume[i] = Queue_volume[i+1];
		}
		start_job( index, loops, volume );
	}
}

void dtumusic_close(void)
{
	if ( !Music_ready )
		return;
	dtumusic_stop();
	Queue_count = 0;
	Music_ready = 0;
	if ( Total_tracks > 0 && Dtu_debug )		// ASCENT: -debug or the debug build (dtufile.h)
		printf( "MUSIC: %d tracks started, %u frames played, %d repeats, %d underruns\n",
			Total_tracks, Total_frames, Total_passes, Total_underruns );
}

// 0xA36E7: read DISCO.JRG.
static int disco_load(void)
{
	ubyte *data;
	int size, i, count;

	data = dtu_load_file( "DISCO.JRG", &size );
	if ( data == NULL ) {
		dtu_log( "MUSIC: the file DISCO.JRG is not in the data directories\n" );
		return 0;
	}
	count = (size >= 4) ? (int)(data[0] | (data[1]<<8) | (data[2]<<16) | ((unsigned)data[3]<<24)) : 0;
	// The original stops with an error if the count is not 40.
	if ( count != DTUMUSIC_STREAMS || size < 4 + 20*DTUMUSIC_STREAMS ) {
		dtu_log( "MUSIC: DISCO.JRG does not have %d entries\n", DTUMUSIC_STREAMS );
		free( data );
		return 0;
	}
	for (i=0; i<DTUMUSIC_STREAMS; i++ ) {
		ubyte *e = data + 4 + 20*i;
		memcpy( Disco[i].name, e, 16 );
		Disco[i].name[15] = 0;
		Disco[i].value = e[16] | (e[17]<<8) | (e[18]<<16) | ((unsigned)e[19]<<24);
	}
	free( data );
	return 1;
}

void dtumusic_init(void)
{
	extern int digi_driver_board;
	int t;

	if ( Music_ready )
		return;
	if ( FindArg( "-nomusic" ) || FindArg( "-nosound" ) || digi_driver_board < 1 )
		return;
	if ( !disco_load() )
		return;
	Queue_count = 0;
	Cur_index = -1;
	Fade_target = -1;
	Playing = 0;
	Music_ready = 1;

	if ( FindArg( "-musiclog" ) )
		Music_log = 1;
	if ( FindArg( "-origmusic" ) )		// ASCENT: the start of a level as in the original
		Dtu_fix_start_music = 0;
	safe_music_args();
	if ( (t = FindArg( "-musicscript" )) != 0 && t+1 < Num_args ) {
		char *p = Args[t+1];
		float time;
		while ( Script_count < SCRIPT_MAX && sscanf( p, "%f:%d:%d", &time, &Script[Script_count].code, &Script[Script_count].value ) == 3 ) {
			Script[Script_count++].time = fl2f(time);
			p = strchr( p, ',' );
			if ( p == NULL ) break;
			p++;
		}
	}

	if ( !Atexit_done ) {
		atexit( dtumusic_close );
		Atexit_done = 1;
	}
}

//-----------------------------------------------------------------------
// The debug option -musicwav.

static void put16(FILE *f, int v) { fputc( v & 0xff, f ); fputc( (v>>8) & 0xff, f ); }
static void put32(FILE *f, unsigned v) { put16( f, v & 0xffff ); put16( f, v >> 16 ); }

int dtumusic_render_wav(const char *track, const char *wav_name, int seconds)
{
	static music_player p;		// not on the stack: it holds the ring buffer
	const char *name = track;
	int16_t buf[512*2];			// the mixer asks for 512 frames in one call
	unsigned frames, done = 0;
	int n, i;
	FILE *f;

	if ( track[0] >= '0' && track[0] <= '9' && strchr( track, '.' ) == NULL ) {
		i = atoi( track );
		if ( !disco_load() || i < 0 || i >= DTUMUSIC_STREAMS || Disco[i].name[0] == 0 ) {
			printf( "MUSIC: no entry %s in DISCO.JRG\n", track );
			return 1;
		}
		name = Disco[i].name;
	}
	safe_music_args();
	if ( !player_open( &p, name, DTUMUSIC_LOOP_FOREVER ) ) {
		printf( "MUSIC: cannot open '%s'\n", name );
		return 1;
	}
	f = fopen( wav_name, "wb" );
	if ( f == NULL ) {
		printf( "MUSIC: cannot write '%s'\n", wav_name );
		player_close( &p );
		return 1;
	}

	frames = (unsigned)seconds * p.info.rate;
	fwrite( "RIFF", 1, 4, f );	put32( f, 36 + frames*4 );
	fwrite( "WAVEfmt ", 1, 8, f );	put32( f, 16 );
	put16( f, 1 );	put16( f, 2 );
	put32( f, p.info.rate );	put32( f, p.info.rate*4 );
	put16( f, 4 );	put16( f, 16 );
	fwrite( "data", 1, 4, f );	put32( f, frames*4 );

	while ( done < frames ) {
		n = (frames - done < 512) ? (int)(frames - done) : 512;
		n = music_callback( &p, buf, n );
		if ( n <= 0 )
			break;
		for (i=0; i<n*2; i++ )
			put16( f, buf[i] );
		done += n;
	}
	fclose( f );

	printf( "MUSIC: wrote %s: '%s'%s, %d Hz, %d channels in the file, %d values in one pass, %u frames written, %d repeats, %d underruns\n",
		wav_name, name, p.ogg ? " (replacement)" : "", p.info.rate, p.info.channels, p.info.total, done, p.passes, p.underruns );
	player_close( &p );
	return done == frames ? 0 : 1;
}
