# Movies of DTUGAME.EXE

Source: the version 1.3 executable (`gamedata/re/dtu_flat.bin`, image base
0x10000), read with `tools/dtudis.py`. All addresses are addresses in that
image. The file format is in [../formats/mve.md](../formats/mve.md).

**V** = verified in the disassembly or in the data. **G** = guess.

Nobody watched the original game for this document, and nobody watched or
listened to the port in real time: the checks are decoded frames, window
captures and sample counts.

## 1. Summary

| Movie | When | Code |
|---|---|---|
| LOGO22S.MVE | program start, before the title picture, unless `-notitles` | 0xB8513 **V** |
| INTRO2.MVE | after LOGO22S, if no key was pressed in it | 0xB8536 **V** |
| TLOGOS.MVE | after INTRO2, if no key was pressed in LOGO22S or INTRO2 | 0xB8562 **V** |
| FLAM1.MVE | dialog statement D3 with sound 0xFFFF (script 320 of GABE, level FLAME), the game goes on after it | 0xA9A5B **V** |
| CREDITV1.MVE | the Flamesword (object 3 of level 0x20, FLAME) breaks: the end of the game, then the front end | 0xA6A87 **V** |

IPLOGO.MVE on the disc is a byte copy of LOGO22S.MVE; no string of the
executable names it **V**. The executable builds each path with `"%s\%s"`
from the buffer 0x3D5088, the value of the configuration key `CDPath`
(0x123189, read at 0x2BACF) **V**: the movies come from the disc. The
install has LOGO22S, INTRO2 and TLOGOS too, FLAM1 and CREDITV1 are only on
the disc.

## 2. The play function 0x96439 ("MPLAY32")

The argument is the path. Steps, all **V**:

1. Debug text "MPLAY32: Playing %s", open the file (0xF6CCF). If it does not
   open: "Unable to open %s" and return.
2. Memory callbacks (0xF3290), sound setup 0x963AD: if the sound setting
   0x13ACD8 is 1 or more and the driver handle 0x13AAEC is not 0xFFFF, the
   MVE library gets that handle (0xF3A30); else it starts its own timer
   (0xE4284 with 0xFF00) and gets -1 (no sound).
3. The file reader callback 0x96378 (0xF3670) and the control callback
   0x9640C (0xF5610).
4. Graphics mode 0x101 (VESA 640x480, 256 colors) (0xF5340).
5. `MVE_RunMovie(file, -1, -1, 0)` (0xF5FA0). x = -1 and y = -1 center the
   picture (0xF4490: x = (640 - 432) / 2 rounded down to a multiple of 4 =
   104, y = (480 - 320) / 2 = 80). The picture is not scaled.
6. "Movie %s returned code:%d", close the library (0xF60A0, 0xF5570) and
   the file, close the timer of step 2 (0x96405).

The control callback 0x9640C runs once for each frame:

- `key_inkey()` (0xDA3E4) returns a key: clear the flag 0x142254 and return
  1 (stop the movie). Any key stops it; the mouse is not read.
- the word 0x3AA880 is not 0: return -1 (the library pauses). The play
  function sets it to 0 at 0x96512. **G**: a pause flag.
- else count the frame (0x13CD34) and return 0.

The flag 0x142254 is 1 at the program start (data) and is set to 0 after
the start movies (0xB85A8).

## 3. The start (0xB84D6 to 0xB85B3, in main)

All **V**:

1. Graphics mode 0, then 0x0E (640x480).
2. If `-notitles` (0x132D67) is given, skip to 0xB85B8 (the title picture).
3. If the sound setting 0x13ACD8 is below 1: 0xDFD69 (**G**: the timer for
   the movies without sound).
4. Play LOGO22S.MVE. Play INTRO2.MVE if 0x142254 is not 0. Play TLOGOS.MVE
   if 0x142254 is not 0. So a key in one movie skips all the rest.
5. Without sound: 0xDFC56 and 0xDFB42(0x13ACE8).
6. 0x142254 = 0, 0x2D656 twice, then the title picture (0xB85B8).

## 4. FLAM1.MVE (0xA9A5B)

Statement D3 (play a sound, 0xA97B3) with the sound 0xFFFF. Script 320 of
GABE.RLH (level FLAME, 0x20) has it once, in the branch `var == 2` after
`SET gflag 17` and an empty `SAY`. Steps, all **V**:

1. Only if 0x2EF31 returns true: the sound setting 0x13ACD8 is 1 or more,
   the sound system is ready (0x13AADC) and its start did not fail
   (0x13AAE4 is 0). Without digital sound the whole sequence is skipped.
2. Stop the game time (0x40DF9), stop all streams (0xA48D0: dialog,
   effects, music), 0x45A60(1).
3. Wait while a stream plays (0xA3BBA, 0xA208F), then empty the key buffer.
4. Graphics mode 0, then 0x0E. Play FLAM1.MVE.
5. Back to the game mode (0x13B27C), load PALETTE.256 and reset the
   palette state, 0x69FFB, 0x6A282, 0x1065D, start the game time (0x40E2A),
   0x45A60(0).
6. In both cases the script goes on (0xAC055 with 0x145537).

## 5. CREDITV1.MVE (0xA6786)

0xA6ACB breaks a scenery object. At 0xA6AF8 it tests the level byte of the
player record (+0x1D338D) for 0x20 (FLAME) and the object number for 3. In
FLAME.LV2 object 3 is type 18 (scenery), model `sword23.pof` on the altar
(object 2, `altar_a.pof`) in segment 487: the Flamesword. Then 0xA6786
runs in place of the normal break **V**:

1. Not decoded in detail (**G** for the meaning): work on the player
   record (0x40974, 0x34B26), a new object of type 0x0D at the sword
   (0x68AC8), a loop with game frames (0x435A6) while a value of that object
   is below 0x2666 (**G**: a light that grows), a sound (0x2E600).
2. 50 times: a position moves by a random offset, a sound 0xA8D +
   rand() % 5 plays there (0x2E600) and one game frame runs (0x435A6) **V**.
   **G**: the sword explodes.
3. If 0x2EF31 (digital sound, see 4.1): stop the game time, stop all
   streams, 0x2E816(3), wait while streams play, empty the key buffer, play
   CREDITV1.MVE, `Function_mode` (0x1468CC) = 6 (back from the game:
   Waterdeep picture, then the main menu, see frontend.md), byte
   0x13B62E = 1, start the game time **V**.
4. Without digital sound step 3 does not run: the game goes on in FLAME and
   the end never comes (a defect of 1.3) **V** (the code path).

## 6. The port

`engine/main/dtumovie.c` (player), `engine/main/mvedec.c` (decoder),
`engine/platform/rgbpresent.c` (32-bit frames for the upscaled movies).

- Start: `dtufe_title()` calls `dtumovie_title()` before the title picture.
  `-notitles` and the skip rule of 0x142254 are as in the original.
- FLAM1: statement D3 with sound 0xFFFF calls `dtumovie_flame()`
  (dtuscript.c), with the game time stopped. The port plays it also
  without a sound device.
- CREDITV1: `dtu_break_dressing()` (dtuobj.c) calls `dtumovie_end_credits()`
  for object 3 of level 0x20 and returns. After the movie the game ends to
  the main menu (`dtu_hook_player_death_screen`, the hook that leaves the
  game; the player is not dead, so the front end shows the main menu). The
  steps 1 and 2 of 0xA6786 (the light and the 50 sounds) are not in the port.
- Skip: any key except Alt (Alt+Tab and Alt+Enter do not skip), and also
  the left or right mouse button (a port change; the original reads only
  the keys). Closing the window stops the movie.
- Timing: frame k is shown at k times the frame time of opcode 02 after the
  start. A frame that is more than one frame time late is not shown (the
  original counts "Dropped Frames" too).
- Sound: through the music stream of the mixer (`plat_audio_music_start`),
  at the digital volume (`Config_digi_volume`). The music and the sounds
  stop before the movie (as 0xA48D0).
- Picture: by default the 432x320 picture fills the window with its own
  aspect (square pixels, as on the 640x480 screen). `-movienative` shows it
  as the original: 1:1 in the middle of a 640x480 frame.
- Upscaled pictures: see dtumovie.h (`-hddir`, `-nohdmovies`) and
  `tools/movies_hd.py`. The sound and the timing stay those of the MVE file.
  All five movies use a gaussian resample (ffmpeg `flags=gauss`) and a
  gaussian blur of sigma 1.5 output pixels. Real-ESRGAN (`--method esrgan`)
  upscales each picture alone, so the details changed from picture to
  picture and the movies wobbled; it also made hard, smeared shapes of the
  rendered 3D pictures of the two logos, `LOGO22S` (Interplay) and `TLOGOS`
  (TSR). Without the blur the dither grain of the 256 colors stays.
- `-nomovies` plays no movie. `-playmovie NAME [-movieframes N]
  [-movieshot FILE.ppm] [-moviefast]` is the test option. The shot is the
  picture that was shown last, with the subtitles.
- Subtitles (not in the original): with a language other than English the
  cues of `<lang dir>/<MOVIE>.<code>.srt` are drawn over the picture, in the
  three forms (the movie picture, `-movienative`, the upscaled picture).
  INTRO2 and CREDITV1 have speech. See docs/formats/lang.md.
