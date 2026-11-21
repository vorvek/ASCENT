/*
 * ASCENT: new file. The streamed music of Descent to Undermountain (the
 * source file str_mus.c of the original, see docs/re/music.md), and the
 * music commands of the warp table DTU.WRP. It replaces the MIDI songs of
 * Descent 1.
 */
#ifndef _DTUMUSIC_H
#define _DTUMUSIC_H

// The number of entries of DISCO.JRG (NUM_SMUS_STREAMS in the original).
#define DTUMUSIC_STREAMS	40
#define DTUMUSIC_LOOP_FOREVER	(-1)

// Entries of DISCO.JRG that the code of the original names by number.
#define DTUMUSIC_CREDITS	2		// khelben.tun: the credits, looped
#define DTUMUSIC_DEATH		7		// deathtag.tun: the death of the player, one time
#define DTUMUSIC_DROWDEAD	9		// drowdead.tun: one event in the player code, one time

// Load DISCO.JRG. Call it after digi_init(). The music stays off if the
// sound is off or the command line has -nomusic.
void dtumusic_init(void);

// Stop the music and release the memory.
void dtumusic_close(void);

// Returns 1 if the music can play.
int dtumusic_enabled(void);

// Put a track in the queue (0xA3F2E). index is the entry of DISCO.JRG,
// loops is the number of times that the track repeats after the first time
// (DTUMUSIC_LOOP_FOREVER: until another call), volume is from 0 to 100.
// The track that plays now does not repeat again and the new track starts
// when it ends. Returns 0 if the track is in the queue.
int dtumusic_play(int index, int loops, int volume);

// Empty the queue. The track that plays now does not repeat again (0xA3AB0).
void dtumusic_flush(void);

// Move the volume to "volume" (0 to 100) in steps, ten steps each second.
// A track that reaches the volume 0 stops (0xA451E, 0xA45AE).
void dtumusic_fade(int volume);

// Stop the track now (0xA3DB9).
void dtumusic_stop(void);

// The entry that plays now, or -1 (0xA3C37).
int dtumusic_current(void);

// Returns 1 if the entry is in the queue (0xA3E5D).
int dtumusic_is_queued(int index);

// The music volume setting, from 0 to 8 ("MusicVolume" of DTU.CFG). The
// music fades to the new volume.
void dtumusic_set_volume_level(int level);

// A music command of DTU.WRP: code is from 100 to 198 (0x5B679).
void dtumusic_command(int code, int value);

// Read the music commands of one level from DTU.WRP. level_index is the
// position of the level in DTU.MSN, from 0.
void dtumusic_level_start(int level_index);

// Call it one time on each frame of the game: the music command of the
// segment that the player entered, the fade and the queue.
void dtumusic_frame(void);

// The debug option -musicwav: decode "seconds" seconds of a track through
// the stream code and write them to a WAV file. track is an entry number
// of DISCO.JRG or a file name. The track repeats at its end. Returns 0 if
// the file was written.
int dtumusic_render_wav(const char *track, const char *wav_name, int seconds);

#endif
