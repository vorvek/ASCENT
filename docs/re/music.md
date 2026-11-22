# Music and sound playback of DTUGAME.EXE

Source: the version 1.3 executable (`gamedata/re/dtu_flat.bin`, image base 0x10000),
read with `tools/dtudis.py`. All addresses are addresses in that image.

**V** = verified: read in the disassembly, or checked against the data files.
**G** = guess.

Nobody listened to the original game or to the port for this document.

## 1. Summary

- The music is streamed from the `.TUN` files by the code of `str_mus.c`. A track is
  named by its entry number in `DISCO.JRG` (0 to 39). **V**
- In a level the music is started by the warp table `DTU.WRP`: a line with destination
  level -2 and a command code from 100 to 198 is a music command for one segment. The
  command runs when the player moves into that segment. There is no music command at
  the start of a level. **V**
- There is no combat music. The executable has seven direct calls of the play function
  (section 4), and none is in the AI or combat code. **V** for the list of direct calls.
- A track loops by starting the same file again when it has ended. A new track waits in
  a queue until the track that plays has ended. There is no cross fade. A fade to
  silence exists as its own command. **V**
- `DTU.SNG` and `songs.c` are the CD audio (Red Book) table of the Descent code. The
  game disc has no audio track, so they play nothing. **V** for the code, **G** for
  "nothing" (it follows from the disc layout in `docs/formats/packed.md`).
- The u32 at offset 16 of a `DISCO.JRG` entry is not read by the executable. **V**
- The sound effects are played as signed 16-bit samples at a driver rate of 22050 Hz.
  **V** for the format code, **G** for the meaning of the rate variable.
- The player has footstep sounds, one each 0.625 s of game time while the forward
  control acts. **V**

## 2. Functions of str_mus.c

The names are made from the assert texts (`SMusSampleHandle() != BAD_HANDLE`,
`NUM_SMUS_STREAMS`). Arguments are in eax, edx, ebx, ecx (Watcom register calls).

| Address | Name (made up) | What it does |
|---|---|---|
| 0xA36E7 | SMusInit | Opens `disco.jrg`, reads the count, stops with "File %s is corrupt of out of date" if it is not 40 (0xA3760), reads 40 entries of 20 bytes to the table at 0x1454C8. Allocates two play buffers of 0x4000 bytes (0x1454CC, 0x1454D0), two sample structures of 0x74 bytes, and three queues of 8 entries: SoundJobs 0x1454E0, JobLoops 0x1454E4, Volumes 0x1454E8 (0xA3877). **V** |
| 0xA3A82 | | Releases the table. **V** |
| 0xA3AA0 | | Returns 1 if the music system is ready. All functions below return at once if it is 0. **V** |
| 0xA3AB0 | SMusFlush | Sets the loop count of the track that plays (0x3CE060) to 0 and empties the three queues. **V** |
| 0xA3BBA | SMusIsPlaying | 1 if the sample handle is valid and the driver says that the sample is not done. If it is done, closes the file (0x1454FC). **V** |
| 0xA3C15 | SMusSampleHandle | The driver handle of the sample with the id 0x2712. **V** |
| 0xA3C37 | SMusCurrent | The entry number that plays (0x1454B8), or -1. **V** |
| 0xA3D83 | SMusStop | Calls 0xA3DB9 and closes the file. **V** |
| 0xA3DB9 | SMusStopJob | Fade target = -1, done flag (0x3CE078) = 1, loop count = 0, stops the sample in the driver. Log text "Stoping Music !!!!". **V** |
| 0xA3E40 | | Starts the next entry of the queues: takes one value from each queue and calls 0xA40B3 (code at 0xA4396). **V** |
| 0xA3E5D | SMusIsQueued | 1 if the entry number (eax) is in the SoundJobs queue. **V** |
| 0xA3ED1 | | If a track plays and no fade to 0 is active: calls SMusPlay with the current entry (0x3CE050), the current loop count and the volume 100. One caller, 0x585F6. **V** for the code. **G**: it puts the current track in the queue again after a change of settings. |
| 0xA3F2E | SMusPlay(index, loops, volume) | See section 3. **V** |
| 0xA40B3 | SMusStartJob(index, loops, volume) | Fade target = -1, current volume (0x3CE044) = volume. Opens the file whose name is at `table + index * 20` (0xA4118), opens the decoder (0xE6F28) with a read function (0xA4D7B), fills a driver sample structure (format `2 + (channels != 1)`, volume 0x7FFF, sample id 0x2712, callback 0xA43FC), decodes 0x4000 bytes into each of the two buffers (0xE6E20) and starts the sample (0x2DC80). **V** |
| 0xA43FC | sample callback | Driver event 2 (sample done): done flag = 1, fade target = -1, log text "CALLBACK BARFAGE: Music sample is done". Event 0 (buffer done): gives the other buffer to the driver (0xE4C90) and decodes the next 0x4000 bytes into the buffer that is free. **V** |
| 0xA451E | SMusFade(volume) | Fade target (0x1454F0) = `Volume_table[MusicVolume] * volume / 100`, volume clamped to 0..100. Returns the old target. **V** |
| 0xA456A | SMusIsFading | 0 if the target is negative. If the target equals the current volume: target = -1, returns 0. Else returns SMusIsPlaying. **V** |
| 0xA45AE | fade step | If a fade is active: the current volume moves to the target by 512 up (0x1454F4) or 2048 down (0x1454F8) and is clamped to the target. If it is above 0 the driver volume of the sample is set (0xE4ECC), else the job is stopped (0xA3DB9). Target = -1 when the volume has reached it. Log texts "STR MUS: Fading....", "Volume will be %d", "Stopping Job". **V** |
| 0xA46A9 | SMusService | See section 3. **V** |
| 0xA48D0 | stop all streams | Flushes the dialog, effect and music queues, SMusFade(0), and the same for the effects (0xA291F). **V** for the calls. |
| 0xA4F83 | stream service | Calls 0xA46A9 and the service functions of `str_dlg.c` and `str_sfx.c`. Called from 20 places: the game loop and the wait loops of the game and the front end. **V** |
| 0xA4FAD | timer event | Calls the fade step of the music (0xA45AE) and of the effects (0xA29AF) unless the flag 0x14551C is set. **V** |
| 0xA4FC1 | | Registers 0xA4FAD as a timer event with the value 10 in eax (0xE4364). **V** **G**: the value is the rate in Hz, as in the timer functions of the HMI sound library. Then a fade from the full volume 0x7FFF to 0 lasts 16 steps = 1.6 s. |

`str_sfx.c` (0xA1BF8 to 0xA2A00, `whammo.jrg`) and `str_dlg.c` (0xA2AB7 to 0xA3620,
`werdz.jrg`) have the same structure. They are not described here.

## 3. Play, loop, queue, volume

### SMusPlay (0xA3F2E)

eax = entry number, edx = loop count, ebx = volume in percent.

1. The entry number must be 0 to 39 and the loop count must be -1 (`LOOP_CONTINUALLY`)
   or 0 and more. Else the function returns 1. **V**
2. The volume is clamped to 0..100. `v = Volume_table[MusicVolume] * volume / 100`.
   If `v < 1` the function returns 1 and nothing plays. **V**
3. The entry number goes into the SoundJobs queue and `v` into the Volumes queue. **V**
4. If the JobLoops queue is not empty, all its values are set to 0. If it is empty, the
   loop count of the track that plays (0x3CE060) is set to 0 (0xA40A8). **V**
5. The loop count goes into the JobLoops queue. **V**

So a call never stops the track that plays. The track plays to its end, does not repeat
again, and then the new track starts. **V** A caller that wants a fast change first
calls SMusFade(0) or the stop function.

`MusicVolume` is the byte at 0x13BFEC (key `MusicVolume` of `DTU.CFG`, 5 in the
install, 0x10 in the image). 0x2D7DD clamps it to 0..8 and reads the table at 0x13ACB4:

| Setting | 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 |
|---|---|---|---|---|---|---|---|---|---|
| Driver volume | 0 | 0x1000 | 0x2000 | 0x3000 | 0x4000 | 0x5000 | 0x6000 | 0x7000 | 0x7FFF |

**V** When the option menu changes the setting, the code at 0x58BF8 stores the new value
and calls SMusFade(100): the track fades to the new volume. **V**

### SMusService (0xA46A9)

Runs when the stream service runs (each frame of the game loop). If the done flag
(0x3CE078) is 1:

1. Releases the decoder and closes the file. **V**
2. If the loop count is above 0 or is -1: a count above 0 is reduced by 1, and the
   same entry starts again with the current volume (0xA40B3). **V**
3. Else, if the flag at 0x145498 is not 0, the next entry of the queues starts
   (0xA3E40). **V** The flag has a set function at 0xA3BA0. No instruction calls it or
   writes the flag, and its value in the data of the executable is 1, so the queue
   always runs. **V** The same holds for the flag 0x1453C8 of the ambient sounds (set
   function 0xA2075, value 1).

So a loop is a new start of the same file from its first sample. The gap between the
last sample and the first sample is the time to the next service call plus the time to
open the file and decode the first two buffers. **V** for the mechanism. The length of
the gap was not measured.

## 4. Who starts music

All direct calls of SMusPlay (call instructions with a 32-bit offset, found by a scan of
the code): **V**

| Call | Entry | Loops | Volume | Context |
|---|---|---|---|---|
| 0x5B6DF | value of the warp line | 0 or -1 | 100 | Music command of `DTU.WRP`, see below. **V** |
| 0x4249D | 7 `deathtag.tun` | 0 | 100 | In the function at 0x4225A/0x4244B, after a wait loop and after the front end mode (0x1468CC) is set to 6. **G**: the death of the player. The name of the file fits. |
| 0x43DAF | 9 `drowdead.tun` | 0 | 100 | In the player frame code (0x43A5A), when the word at player offset 0x1D33FF is 0. The meaning of the event is not known. |
| 0x2C678 | 2 `khelben.tun` | -1 | 100 | The credits (log text "Do Credits!"). **V** |
| 0x83150 | 2 `khelben.tun` | -1 | 25 | A front end function at 0x8313C. Not traced. |
| 0x8C21B | from the caller | -1 | 100 | `fewdeep.c` (the Waterdeep screens of the front end). Not traced. |
| 0xA3F21 | current entry | current | 100 | 0xA3ED1, see section 2. |

There is no call in the level load code. A level is silent until the player enters a
segment with a music command. **V**

The 15 `.TUN` files that are not in `DISCO.JRG` (FECGEN, FECREDIT, FEWDEEP, IGACT1 to 3,
IGBGND1 to 3 and the six TAG files) cannot be opened by this code. The executable does
not contain the text `.tun` or one of these names in any case (search of the whole
image), and SMusStartJob takes the name only from the table. **V** So the version 1.3
executable does not play them.

### The music commands of DTU.WRP

- 0x599C3 reads `DTU.WRP` with `"%d,%d %d,%d %d"` (at least 4 values). For each line
  whose first value is the index of the current level in `DTU.MSN`, it writes one entry
  of 4 bytes to a table that is indexed by the segment number (0x296494 + segment * 4):
  destination level (s8), destination segment or command code (s16), value (u8). A later
  line for the same segment replaces the earlier one. **V**
- 0x69BC8, in the move function of an object (0x697F7): the function stores the segment
  of the object at its start (0x13D41B). After the move, if the object is the player
  (type 4) and its segment is not the stored one, it calls the warp check 0x59C3C with
  the new segment. **V** So a command runs when the player moves into the segment. It
  does not run for the segment in which the player starts.
- 0x59C3C: destination level -1: nothing. -2: calls 0x5B3AC with the code and the value
  (0x59EFC). Other values are a warp to another level. **V**
- 0x5B3AC: code 0 and codes 1 to 19 are game events (jump table at 0x5B360). Codes 100
  to 198 are music commands (0x5B679). **V** The code in front of 0x5B679 handles the
  other codes and ends with a fade of the streamed effects of `str_sfx.c` (0xA291F).
  It was not traced. **G**: the codes from 200 are the same bits for the effects.

Music command, `bits = code - 100`, in the order of the code at 0x5B694: **V**

| Bit | Meaning |
|---|---|
| 0x10 | Do nothing if the entry `value` is the one that plays now (0xA3C37). |
| 0x08 | Do nothing if the entry `value` is in the queue (0xA3E5D). |
| 0x20 | The loop count is -1. Without this bit it is 0. |
| 0x02 | SMusFlush (0xA3AB0). |
| 0x01 | SMusPlay(value, loop count, 100). |
| 0x04 | SMusFade(0). |

The file uses two codes: 151 (bits 0x33: if the entry does not play now, flush and play
it in a loop; 235 lines) and 106 (bits 0x06: flush and fade to silence, 78 lines). **V**
Example, level 0 (YPINN): segments 3, 28 and 22 start entry 20 `startpub.tun`, segment
153 fades out, segment 184 starts entry 26 `DROWCAV2.tun`.

With code 151 a track that plays is not stopped: it plays to its end and then the new
one starts (section 3). The level designers put a line with code 106 in front of some
changes. **V** for the code, **G** for the intent.

### DISCO.JRG offset 16

The table pointer 0x1454C8 is used in three places: the load (0xA36E7), the release
(0xA3A82) and the start of a job, which passes `table + index * 20` as the file name
(0xA4122). No instruction reads `table + index * 20 + 16`. **V** The values in the file
(1 and 2 for `khelben.tun`, 2 for `deathtag.tun`, 1 for `ud_aggr.tun`, 4 for
`febstaff.tun`, 0 for the others) have no effect in this executable. **G**: a loop or
group number of an earlier version of the code.

## 5. songs.c and DTU.SNG

- 0x78D87 `songs_init`: opens `dtu.sng` ("Couldn't open DTU.SNG"), reads each line
  with `"%hd %u"` into a track table (0x390D92, s16) and a time table (0x390D28), at
  most `MAX_SONGS` lines, then prints "RedBook Audio enabled" or "RedBook Audio not
  enabled" after a test of the CD audio driver (0x75052). **V**
- 0x78EB1 `songs_play_song(songnum, repeat)`: if the song is the current one and its
  time (the seconds of the table) has not passed, nothing. Else, if the CD audio test
  gives 1, it stops the CD (0x74D97) and plays the track (0x74D1B). Three callers
  (0x3A0B4, 0x4DF52, 0x58416). **V**

This is the CD audio form of the Descent song code. It does not touch the streamed
music.

## 6. Sound effects

- 0x2D955 (`digi_play_sample`) fills the same sample structure of 0x74 bytes as the
  music code (0x2E091 and 0x2E3D5 fill it too). The format field (offset 0x10) is
  `2 + (channels == 2)` at 0x2DA6E, where the channel count comes from the sound cache (0x2F7A0
  gives rate, channels and size). The music code writes the same field with
  `2 + (channels != 1)` for its decoded 16-bit ACM data. **V** So the effects are given
  to the driver in the same format as the 16-bit music: signed 16-bit, mono or stereo.
  **G** for the names of the format values: the header of the sound library is not
  available. Other evidence: the source sounds have the extensions `.RAW` and `.R16`
  (text at 0x120BC2), the archives store them as ACM streams, which decode to 16 bits,
  and the loader refuses a sound that is not an ACM stream.
- The variable at 0x13AC80 holds 22050 and the one at 0x13AC7C holds 8 (clamped to 1..16
  by 0x2DF3A). **G**: the driver rate and the maximum number of channels, the DTU values
  of `digi_driver_rate` (11025 in Descent 1) and `digi_max_channels`.
- Volume: the sample volume is `volume * digi_volume / 0x10000` (0x2DA88).
  `digi_set_digi_volume` (0x2DF7E) sets `digi_volume = dvolume * 0x8000 / 0x7FFF`,
  at most 0x8000. In Descent 1 the maximum is 16384. **V** So a sound with the volume
  1.0 plays at the whole range when the setting is at its maximum.

### Footsteps (0x433E0)

Called one time on each frame from the game loop (0x437E7), before the frame time is
added to the game time. The call is skipped if the player object has a flag of
0x200800 (fly, levitate) or a state bit of 0xE0 (fall, jump, climb), unless the flag at
0x145305 is set (meaning not known). **V**

1. If the game time (0x13CD30) is not above the time of the next step (0x13B7A7):
   return. **V**
2. If the forward control time (0x1E9A7C, `Controls.forward_thrust_time`) is not 0:
   - next step = game time + 0xA000 (0.625 s);
   - position = position of the player, with x and z changed by a value that is
     0x200 and -0x200 in turn (0x13B7B4);
   - floor type = the two low bits of byte 1 of the bottom side of the segment below
     the player, or of the segment of the player if it has no segment below;
   - sound number = base + `rand() % 4`, base 2232 (`foot4`) for floor type 0, 2224
     (`foot2`) for type 1, 2228 (`foot3`) for type 2, 2220 (`foot1`) for type 3;
   - for floor type 1 (and when the flag 0x145305 is set) an object of type 10 is
     created at the position (0x3BD4C). **G**: a splash;
   - `digi_link_sound_to_pos(sound, segment, 3, position, 0, 0x8000)` (0x2E900). **V**
3. If the move mode (0x1452F0) is 2 (run): next step = game time + 0x5000 (0.3125 s).
   **V**

The rule uses the game time, so the rate of the steps does not depend on the frame
rate. Strafing alone gives no steps. 0x433E0 has one caller (0x437E7) and plays one
sound for each step. **V** It also sets the switch 0x145360 to 1 on each frame on
which the game time is above the time of the next step (also while the player stands
still); the creature move reads it for the splash object only (below). **V**

The position: the sound code (0x2E23B, as `digi_get_sound_loc` of Descent 1) takes
the pan from the direction to the sound, not from its distance. With the offset of
0x200 in x and z the direction is the world diagonal (1, 0, 1) or (-1, 0, -1), so
the steps are panned hard to the left and to the right in turn, by the heading of the
player. The volume is 0.5 less the path distance / 256 (about 0.5). **V** (code),
port log: pan 8936 and 56598 of 65535, volume 16383 of 32767.

Rate: the walk speed limit is 49.33 and the run speed limit 74.0 (physics.md; two
units are one foot). One step each 0.625 s is 1.6 steps a second, one each 0.3125 s
is 3.2: the cadence of a human walk and run. By distance the steps are long (30.8 and
23.1 units). So the rate of the player is not too fast. **G** (judgement)

The samples (`PORTAL.EGG`, decoded): each holds one footfall. `foot1`, `foot3` and
`foot4` are 0.61 to 0.91 s long, with sound in the first 0.11 to 0.37 s; the `foot2`
set (floor type 1) has sound for 0.8 s of 1.15 to 1.92 s. **V**

### Footsteps of the creatures (0x9FCF1 to 0x9FDFB)

At the end of the creature move 0x9F153 (pmon.c), for a creature with activity 2
(0x1CC) whose animation state (0x170) is 2 (WALK) or 5 (RUN) and whose animation
frame (0x172) is the first frame of that state (word table 0x13D9E0): **V**

- not for a creature with `flying` (`Robot_info+0x98` bit 0x20) or object flag 0x800
  (byte 0x89 bit 0x08), not for the monster type with `rat=` (0x13A820);
- sound = base of the floor type (as for the player, the floor of the segment below
  the creature, 0x9F236) + `(clock + objnum) % 4` (player struct 0x1D339F, ai.md);
- `0x2E900(sound, segment, 3, creature position, 0, str << 10)` (`str`,
  `Robot_info+0xDE`; volume str / 64, at most 0.5).

The test has no timer: the sound plays on each game frame that shows the first frame.
The AI shows the first frame for about one half of a frame time of the cycle
(`state time / (2 * (frames - 1))`) after the start of each cycle. So one cycle gives
several steps a few game frames apart. Port, CRYPTS soak with the original rule:
1.32 steps a cycle at 20 fps, 3.40 at 60 fps (at most 5), with a cycle of 1.05 s; the
steps of one cycle are 16 ms apart at 60 fps (log: `foot4dd` at 5.552, 5.568, 5.585,
5.602 and 5.618 s). **V** (port measure of the original rule). This is the "too
often, repeated too quickly" sound, and as the steps are at the creature they are
heard at a distance.

The floor type 1 block (0x9F35F) halves the acceleration of a creature that does not
fly and, when the switch 0x145360 is set, creates a splash object (type 10).

## 7. The port

| Original | Port |
|---|---|
| `str_mus.c` | `engine/main/dtumusic.c`. The queue, the loop count, the volume table, the fade steps and the command bits are as above. |
| Double buffer of the driver, filled from its callback | The compressed file is read into memory when the track starts. The callback of the platform mixer decodes it block by block into a ring buffer of 4096 frames (`acm_stream_read` of `engine/main/acm.c`). |
| Loop: new start of the file by the service function | The callback starts the decoder again at the first sample when the track has ended. There is no gap. |
| Fade timer of 10 Hz | `dtumusic_frame` makes the steps from the real time (`timer_get_fixed_seconds`), one for each 0.1 s. |
| Flag 0x145498 | Not in the port. The queue always runs (the flag is always 1). |
| `str_sfx.c` (0xA1BF8 to 0xA2AB6) | The second part of `dtumusic.c`: WHAMMO.JRG and WHAMMO.DAT, a queue of its own, the loop, the fade (the same steps as the music) and the volume of the effects setting (0x13BFEB through the table 0x13ACB4). It plays on the second stream of the mixer (`PLAT_AUDIO_STREAM_AMBIENT`). |
| Warp table commands 200 to 298 | The same bits as the music (docs/formats/wrp.md). Correction `Dtu_fix_wrp_sound` (on): a segment keeps its music line and its ambient line; the original keeps only the last line of a segment. |
| Warp table commands 100 to 198 | `dtumusic_level_start` reads them from `DTU.WRP`, `dtumusic_frame` runs the command of a segment when the segment of the player changes. |
| CD audio of `songs.c` | `engine/main/songs.c` reads `DTU.SNG` and plays nothing. |
| 16-bit effects | `digi.c` plays `piggy_get_sound_16` data with `plat_audio_play16` at the rate of the sound. `-sound8` or `Dtu_sound_16bit = 0` selects the 8-bit copy. |
| Footsteps | `engine/main/dtufoot.c`, setting `Dtu_footsteps`. The splash is `dtu_water_splash` (dtuobj.c), with no light and to the side of the walk (bugs.md D13). The flag 0x145305 is taken as 0. The rate of the player is the original one. Correction `Dtu_fix_foot_pan` (on): the step of the player plays in the middle (pan 0.5) at the fixed volume 0.5, with no offset. |
| Creature footsteps | `dtufoot_creature`, called at the end of `dtu_creature_physics_sim` (dtuobj.c) for a creature with activity 2. The sound choice is `rand() % 4`, not the clock. The `rat=` test is any monster type with the keyword. Correction `Dtu_fix_creature_steps` (on): one step when the creature enters the first frame of the WALK or RUN cycle, not one on each game frame that shows it. |

The death music (0x4249D) plays in `dtu_player_death_frame` (dtuobj.c) and on the death
screen of the front end (dtufe.c). The speech of `str_dlg.c` (WERDZ.DAT) plays for
Khelben in the front end (dtufe.c).

Not in the port: the call at 0x43DAF has no call site.

The ambient sounds (`str_sfx.c`) play since 2026-11-22. `dtumusic_stop` also stops the
ambient sound and empties its queue (the callers are the places where the original stops
all its streams, 0xA48D0). `-nomusic` does not turn the ambient sounds off, `-noambient`
does. BONES, `-testtour -musiclog`: 23 ambient lines in 33 s; 223 fades the sound that
plays (1.6 s) and starts the new one at its end, 255 loops, 206 fades out.

Options of the port: `-nomusic`, `-noambient`, `-nosound`, `-sound8`, `-music N` (play entry N in a
loop from the start of the level), `-musiclog`, `-soundlog`,
`-musicscript T:CODE:VALUE[,...]` (music command at T seconds of game time),
`-musicwav TRACK FILE SECONDS` (write the output of the stream code to a WAV file),
`-origmusic` (no music at the start of a level, section 9, and one command for each segment of DTU.WRP), `-safemusic` and
`-nosafemusic` (the replacement music on or off for one run, section 10),
`-origsound` (the footsteps of the original: `Dtu_fix_foot_pan` and
`Dtu_fix_creature_steps` are 0).

## 8. Checks of the port

- `ascent -musicwav` output against `python -I tools/tun.py wav`, value by value:
  STARTPUB (whole track, 4475834 values), FEBSTAFF (first 10 s, a file with blocks of
  16384 values), FECREDIT (first 10 s, mono), DEATHTAG (whole track, 467834 values): 0
  values differ.
- Loop: the values after the end of DEATHTAG (61366 values) and of STARTPUB (375166
  values) are equal to the values from the start of the track.
- Game run, `-level YPINN -music 20 -musiclog -frames 600`: 220160 frames of music
  given to the mixer in 9.98 s of game time (22050 Hz), 0 underruns, exit status 0.
- Game run with `-musicscript 1:151:7,3:151:20,16:106:0,19:151:7,20:101:9`: entry 7
  starts at 1.02 s in a loop, plays one whole pass after the command of 3 s (233917
  frames, stop at 11.61 s), entry 20 starts at 11.61 s, the fade of 16.01 s stops it at
  16.83 s (8 steps from the volume 0x4000), entry 9 waits for the end of entry 7.
- Game run with `-testmove forward:2000ms -soundlog`: a footstep sound (`foot3bd`,
  `foot3cd`, `foot3dd`, 16 bits, 22050 Hz) starts at 0.017, 0.650, 1.283 and 1.916 s,
  with the pan on the left and on the right in turn. The player walks through segment
  28 and `startpub.tun` starts there from the line of `DTU.WRP`.
- Footsteps of the player, YPINN, `-testmove forward:2500ms,forward+run:2000ms
  -soundlog`, time between steps (walk / run): 20 fps 0.650 / 0.350 s, 60 fps 0.633 /
  0.317 s, 144 fps 0.632 / 0.319 s (the 0.625 and 0.3125 s rounded up to a frame), the
  same with and without `-origsound`. Pan and volume: 8936 and 56598 in turn, volume
  16383 with `-origsound`; 32767 (middle), volume 16384 with the correction.
- Footsteps of the creatures, CRYPTS, `-soak 1 -objlog` for 40 s: steps for each walk
  cycle (cycle 1.05 s) with `-origsound` 1.32 at 20 fps, 3.40 at 60 fps, 6.09 at 144
  fps (at most 10); with the
  correction 1.00 at 20, 60 and 144 fps.
- Nobody listened to the result.

To listen: `ascent -datadir <install> -datadir <disc DTU> -level YPINN -music 20`, and
walk with the forward key. Or write a file and play it:
`ascent -datadir <install> -datadir <disc DTU> -musicwav 20 startpub.wav 30`.

## 9. The silent start of a level (port correction)

The original starts no music when a level loads (section 4), and 0x69BC8 runs the
command of a segment only when the player moves into it from another segment. The
segment in which the player starts never runs its command. **V** So each level is
silent until the player crosses a segment with a command. In YPINN the player starts in
segment 2 (between the door to Waterdeep, segment 1, and the room); the music starts
only when the player walks into segment 3, 28 or 22. A player who turns the other way
hears nothing. When the player comes back from Waterdeep the start is segment 1, three
steps from segment 3.

`Dtu_fix_start_music` (dtumusic.c, on, not in a menu, cleared by `-origmusic`): at the
first game frame after a level load (a new level, a warp, the return from Waterdeep, a
saved game), the port runs the command of the start segment, or else the command of the
nearest segment with a command (breadth first search through `children`). If that
command starts a track and another track plays, the other track fades out (as with code
106), so that the change does not wait for the end of the old track.

Result at the start position of each level file (`-level NAME -musiclog`, the port):

| Level | Start segment | Nearest command (segment, steps) | Track that the correction starts |
|---|---|---|---|
| YPINN | 2 | 151 (3, 1) | 20 startpub |
| HUB1 | 136 | 151 (141, 1) | 16 healroom |
| KOBOLDS | 12 | 151 (76, 19) | 8 drowcav3 |
| BONES | 307 | 151 (307, 0) | 21 ud_aggr |
| BAT | 2 | 151 (39, 19) | 33 UD_DWF2 |
| THIEF0 | 2 | 151 (30, 11) | 25 DROWCAV1 |
| THIEF1 | 320 | 151 (24, 14) | 11 ecorrid1 |
| THIEF2 | 328 | 106 (379, 14) | none (fade) |
| HUB2 | 188 | 151 (41, 1) | 28 GENMAG2B |
| BANE | 1 | 151 (1, 0) | 13 genmag1b |
| PRISON | 428 | 151 (427, 1) | 27 ECORRID2 |
| MINE | 0 | 151 (2, 6) | 27 ECORRID2 |
| CRYPTS | 384 | 151 (29, 1) | 12 genmag1a |
| MULHOR1 | 1 | 151 (1, 0) | 11 ecorrid1 |
| MULHOR2 | 75 | 151 (75, 0) | 35 UD_GEN1 |
| HUB3 | 3 | 151 (7, 1) | 28 GENMAG2B |
| MYRKUL | 113 | 151 (114, 1) | 18 insidep2 |
| CLAN1 | 2 | 106 (34, 5) | none (fade) |
| CLAN2 | 8 | no command in the level | none |
| CLAN3 | 94 | 151 (26, 3) | 22 ud_dwf1 |
| DROW1 | 422 | 151 (421, 1) | 25 DROWCAV1 |
| DROW2 | 1 | 151 (2, 1) | 18 insidep2 |
| DROW3 | 311 | 151 (267, 4) | 8 drowcav3 |
| DROW4 | 235 | no command in the level | none |
| HUB4 | 51 | 151 (51, 0) | 25 DROWCAV1 |
| BEHOLDER | 5 | 106 (5, 0) | none (fade) |
| BEHOLDR2 | 0 | no command in the level | none |
| DROW5 | 1 | 106 (75, 5) | none (fade) |
| IBBALAR | 723 | 151 (790, 2) | 19 spectat |
| IBBALAR2 | 53 | no command in the level | none |
| FLAME | 3 | 151 (4, 1) | 31 LOLTH2 |

MADLANDS and PLAYGRND are in `DTU.MSN` but have no level file in the install or on the
disc. In the original all 31 levels start silent, also BONES, BANE, MULHOR1, MULHOR2 and
HUB4, whose start segment has the command. A warp puts the player in the destination
segment of the warp line, not at this start position; the correction uses that segment.
KOBOLDS at its start position warps the player to HUB1 in the first frame.

## 10. Replacement music (port option)

The setting "Streaming safe music" (`Dtu_safe_music`, Improvements menu, `SafeMusic` in
`ascent.cfg`, off by default) plays `<hd>/music/NAME.ogg` in place of `NAME.TUN` when
the file exists. It is for a track that a video platform claims: STARTPUB (entry 20) is
claimed by a Content ID check. The Ogg Vorbis file is decoded by stb_vorbis
(`engine/main/dtuogg.c`) in the same player as the ACM tracks: same queue, loop and fade,
and the mixer resamples it. A change of the setting starts the track that plays again in
the other form. `tools/music_replace.py TRACK INPUT` makes the file: 44100 Hz stereo,
Vorbis quality 6, the silence at both ends removed, the RMS matched to the original TUN.

Checks: `-safemusic -musicwav 20 safe.wav 240` against the ffmpeg decode of
`STARTPUB.ogg`: 4899590 frames in one pass, at most 1 of difference in a value (the
two Vorbis decoders round differently), three passes, the loop starts again at the first
frame with no gap. RMS: original -20.7 dB, input -20.4 dB, file -20.7 dB.
