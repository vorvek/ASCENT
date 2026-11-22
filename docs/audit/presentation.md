# Audit: rendering, audio and the platform

Scope: the software and GPU renderers (8-bit and RGB forms), the upscaled
textures and interface pictures, the palette effects, sound, music, movies,
the window and input layer, frame timing, memory, the session log and the
crash handler. Base: main at 6e8dea2. Build: `build` (RelWithDebInfo) and
`build-ubsan` (`-fsanitize=undefined -fsanitize-trap=undefined
-fno-sanitize=alignment -fstack-protector-strong`).

All runs: `ascent.exe -datadir E:/UNDAMONTAN/DTU -datadir gamedata/cd/DTU
[-hddir gamedata/hd] -nomovies -mute -hidden -userdir <scratch>`. Nobody
looked at a visible window or listened to the sound.

## Findings

Most important first. Size of an open item: S (under a day), M (1 to 3
days), L (more).

| Id | Description | Evidence | Status |
|---|---|---|---|
| A1 | The ambient sound streams are not played. `DTU.WRP` has 157 lines with the codes 223 and 255 (start an ambient stream) and 59 with code 206 (fade it). The original plays them with `str_sfx.c` (0xA1BF8 to 0xA2A00) from WHAMMO.JRG/WHAMMO.DAT (74 stereo ACM streams, docs/formats/jrg.md). The port reads only the codes 100 to 198. | `dtumusic_level_start` (dtumusic.c) keeps `code >= 100 && code < 199`; `dtumusic_command` returns for other codes. 0x5B3AC ends the other codes with a fade of the effect streams (0xA291F), docs/re/music.md section 3. | Open, M: decode the bits of the codes 200 to 298 in 0x5B3AC and the queue of `str_sfx.c`, add a second stream to the mixer (audio.c has one music stream). docs/re/music.md now says so. |
| A2 | GPU renderer with the upscaled set: a static wall seen for the first time (after a turn) showed its 8-bit form for a frame or more, then went sharp. | `ASCENT_GPU_HDLOG=1`, BANE `-fixedfps 30 -testmove wait:60,left:150,forward+run:100,right:80`: after "the upscaled loads are done" the walls `eroc42 + empty` and `stone223` were still requested at first sight. | Fixed, 549cfd4. `gpu_preload_level` (render.c) now also preloads the textures of every side with `WID_RENDER_FLAG`: 30 to 100 pairs and 10 to 30 MB more per level (CRYPTS 247 pairs, 254 MB; no sweep). Same run after: only the sprite frames `cswound_*` are requested late. `ASCENT_GPU_NOPRELOAD=2` gives the old behavior. docs/hd_textures.md updated. |
| A3 | The shadow of a menu box over the 3D view was missing with the GPU renderer. The fade table keeps color 255, the frame color that shows the GPU image, so a darkening over the view did nothing. `show_help` (F1, game.c) draws the box with x2 and y2 one pixel outside its canvas (as 0x632E8 of the original), so the outer column and row of the shadow fall on the 3D view in the software renderer. | gdb: `call show_help()` at frame 40 of BANE, screenshot while the box is open. Mean luminance of the column right of the box (x 466, y 195 to 240): software 20 (view 40), GPU 8-bit 38, GPU upscaled 38. | Fixed, 7095b39. `dtu_hdui_menu_shadow` (dtuhdui.c) also calls `g3_gpu_view_darken` for the 12 shadow rectangles; gpu.c takes 16 dark rectangles (was 4). After: GPU 8-bit 21, GPU upscaled 20; after the box closes the view is 42 as before (no trace). The 6 corner pixels that the software darkens twice are darkened once on the GPU. |
| A4 | UBSan trap in gpu3d.c (`bitmap_texture`, `merge_texture`) on the first frame of every level with the GPU renderer (docs/re/bugs.md). | No trap now: BANE 100 frames, CRYPTS, DROW1, HUB1, MYRKUL, FLAME, YPINN 600 frames each with movement, GPU and upscaled set, exit 0, "Session end: normal exit". | Fixed before this audit by a8e7583 (the merge table `Mt` was written past its end: an index up to 4095 into 1024 entries, a bounds trap). docs/re/bugs.md updated (a4297fc). |
| A5 | MEM_LEAKAGE at exit: 17 blocks, 1467399 bytes (16 in the earlier note). | Stacks from a temporary `RtlCaptureStackBackTrace` in mem.c: 13 blocks are interface pictures loaded once by `dtu_display_init` and `load_portrait` (dtudisp.c `febgnd_load`: icons, cursor, button sheet, interface, portrait, `HRISBTN.PCH`) and the newmenu background (newmenu.c); 4 are files kept for the session: GABE.RLH and GABE.DAT loaded twice (dtuhook.c `gab_load` and dtuscript.c `dtu_script_init`, 370 KB each time) and the BRN file of the level (dtuobj.c, freed at the next level). | Open, S. Not a growth: each is loaded once per session or replaced. Free them at exit and load GABE once. The files belong to the in-game UI and world areas, so not changed here. |
| A6 | Frame time: the GPU renderer has a hitch at the level start; no slow frame after it. | `ASCENT_GPU_STATS=1`, 900 frames with movement, no frame limit: HUB1 GPU 10 frames over 25 ms, 7 over 50 ms, max 186 ms, the last at present 77; software 4, 2, 115 ms, the last at present 73. CRYPTS GPU 15, 9, 274 ms, last at 82; software 4, 2, 114 ms. Average 17.2 to 17.4 ms (vsync 60 Hz). Upscaled uploads on the main thread: HUB1 329 uploads 119 ms, CRYPTS 710 uploads 277 ms. | Measurement added, 97ec2d2 (the count of frames over 25 and 50 ms in the GPU statistics). Open, S: spread the upscaled uploads of the preload over several frames (a budget in ms per present). |
| A7 | The effect colors of models: the original turns the effect color rule on for a tinted object (0x14C4CD) and with 0x14C4CF; the port only for merged walls. | docs/re/effectcolors.md, "Not done". | Open, M. |
| A8 | The end of the game: steps 1 and 2 of 0xA6786 (a growing light at the Flamesword and 50 explosion sounds 0xA8D + rand() % 5) are not played before CREDITV1. | docs/re/movies.md section 6. | Open, S to M (step 1 is not decoded). |
| A9 | Infravision with the software renderer looks red (the original effect) while the GPU RGB form shows the improved (blue, heat) form. | CRYPTS `-infravision` screenshots. By design: the improved form needs the GPU RGB form (docs/re/infravision.md). With `-noimprovedinfra` the mean color of the view: software (63.3, 16.8, 10.9), GPU (62.3, 18.0, 12.0). | Not a defect. Infravision is per face, not per texel (docs/re/infravision.md), open M. |
| A10 | Gamepad and joystick: SDL sends joystick events for a gamepad too. With a joystick control type and a gamepad, `joyc.c` opens the same device as a joystick and the pad layer reads it as a pad, so a button can act twice. | Code read: input.c `joy_open_first` and `pad_open_first` take the first device of each list; no `SDL_IsGamepad` test. Not tested (no device). | Open, S: skip gamepads in `joy_open_first` when the pad layer has one. |
| A11 | The pan law of the mixer is constant power (each side 0.707 in the middle). The law of the HMI SOS driver of the original was not read. | audio.c `channel_set_target`. | Open, S: read the pan code of the driver in dtu_flat.bin and match it. |
| A12 | The upscaled texture of the first person sword (BANE) blurs the red pattern of the 8-bit blade into a pink smear. | Screenshots of BANE frame 60, software, GPU 8-bit and GPU upscaled (blade region). | Open, S: a matter of the upscaled set (tools), not of the renderer. |
| A13 | Renderer parity: mean color of the 3D view, software against GPU. | BANE and CRYPTS frame 60, YPINN frame 150: within 2 of 255 on each channel; the 8-bit GPU form and the software form match in geometry (FOV, arm place) and light. | Checked, no defect. |
| A14 | Crash handler and session log. | `-testcrash 20` with `null`, `abort`, `error`, `overflow`, `divide`: each writes the report and a minidump (`error`: "Session end: exit after an error", no dump, as designed). No OpenGL error or shader error in 32 logs of this audit. | Checked, no defect. |
| A15 | Docs out of date: docs/re/frontend.md lists "Movies (MVE): not done" (the movies play, dtumovie.c); docs/re/music.md said the death music had no call site. | Code read. | music.md fixed (a4297fc). frontend.md belongs to the front end audit. |

Other areas: an `Int3` at object.c line 2264 shows in 10 of the run logs
(the world area). The localization work owns dtumovie.c and the fonts; the
movie player was not changed here.

## Tests of the fixes

- A2: `ASCENT_GPU_HDLOG=1 ascent ... -hddir gamedata/hd -level BANE -frames
  400 -fixedfps 30 -testmove wait:60,left:150,forward+run:100,right:80`, then
  the "GPU hd: bitmap" and "GPU hd: merge" lines after "the upscaled loads
  are done". With `ASCENT_GPU_NOPRELOAD=2` the wall textures are among them.
- A3: gdb with `-level BANE -frames 90 -fixedfps 30 -screenshot after.ppm`:
  `break game_render_frame_mono`, `run`, `ignore 1 40`, `continue`,
  `delete 1`, `break key_inkey`, `call (void) show_help()`, `delete`,
  `call (int) dtu_write_screenshot("open.ppm")`, `return (int) 27`,
  `continue`. Compare the column right of the box in `open.ppm` of
  `-software` and of the GPU run.
- A4: the UBSan build, the levels and frames of the table.
- A6: `ASCENT_GPU_STATS=1`, the line "frames over 25 ms".
- `ctest` passes (16 tests).
