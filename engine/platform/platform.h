/*
 * ASCENT platform layer.
 *
 * This is the only part of the engine that calls SDL. The engine code
 * does not include SDL headers. It includes only this header.
 */
#ifndef _PLATFORM_H
#define _PLATFORM_H

#include <stdint.h>

// Open the window. w and h are the size of the game frame in pixels.
// scale is the initial window size as a multiple of the frame size.
// Returns 0 if successful.
int plat_init(const char *title, int w, int h, int scale);

void plat_shutdown(void);

// Change the size of the game frame. Returns 0 if successful.
int plat_set_frame_size(int w, int h);

// Show one frame of 8-bit pixels.
// pal6 is 256 RGB entries with 6 bits for each component (VGA format).
void plat_present(const uint8_t *pixels, int w, int h, int pitch, const uint8_t *pal6);

// Show one frame of 32-bit pixels (0xXXRRGGBB, the bytes B, G, R, X in
// memory), scaled with a linear filter to the largest size with the aspect
// of the frame (rgbpresent.c, for the upscaled movies). pitch is in bytes.
// The next plat_present shows the 8-bit frame again.
void plat_present_xrgb(const uint32_t *pixels, int w, int h, int pitch);

// Process the window events. Returns 0 after the user closes the window.
int plat_pump(void);

// Time since plat_init in microseconds.
uint64_t plat_time_us(void);

void plat_sleep_ms(int ms);

// ---------------------------------------------------------------------
// Input (input.c). plat_pump translates the window events and calls the
// plat_input_* functions. Each plat_input_* function calls the handler
// that the engine registered, if there is one. A test can call the
// plat_input_* functions directly, without a window.

// Keyboard. scancode is a PC scan code (set 1) from 1 to 255. A key with
// the E0 prefix has bit 7 set (right ctrl is 0x9D), as in the KEY_ constants
// of key.h. Pause is 0x61. down is 1 for a press and 0 for a release.
// The operating system repeats the press event while the key is held.
// The handler gets each repeat as one more press with no release between.
typedef void (*plat_key_handler)(int scancode, int down);
void plat_set_key_handler(plat_key_handler handler);
void plat_input_key(int scancode, int down);

// Mouse. button is 0 for left, 1 for right, 2 for middle.
// dx and dy are the relative motion in device units (not scaled to the frame).
// x and y are the position in frame pixels.
typedef void (*plat_mouse_button_handler)(int button, int down);
typedef void (*plat_mouse_motion_handler)(int dx, int dy, int x, int y);
void plat_set_mouse_handlers(plat_mouse_button_handler button, plat_mouse_motion_handler motion);
void plat_input_mouse_button(int button, int down);
void plat_input_mouse_motion(int dx, int dy, int x, int y);

// Mouse wheel. plat_input_mouse_wheel adds steps (positive: the wheel
// turned away from the user). plat_mouse_wheel returns the steps since its
// last call and clears them.
void plat_input_mouse_wheel(int steps);
int plat_mouse_wheel(void);

// Relative mouse mode: the pointer is hidden and held in the window, and
// the motion has no limit at the window border. If there is no window yet,
// the mode starts when plat_pump first sees the window.
void plat_mouse_set_relative(int on);
int plat_mouse_get_relative(void);

// Joystick. The first joystick or gamepad that SDL finds is used.
// A device that is connected later is opened by plat_pump.
#define PLAT_JOY_AXES    4
#define PLAT_HAT_UP      1
#define PLAT_HAT_RIGHT   2
#define PLAT_HAT_DOWN    4
#define PLAT_HAT_LEFT    8

// Start the joystick support. Returns 1 if a device is open.
int plat_joy_init(void);
void plat_joy_close(void);
// Returns 1 if a device is open.
int plat_joy_present(void);
// Read the first 4 axes. Each value is from -32768 to 32767.
// Returns a mask with bit n set if axis n exists.
int plat_joy_read_axes(int axes[PLAT_JOY_AXES]);
// The current state of the first 32 buttons, bit n for button n.
uint32_t plat_joy_read_buttons(void);
// The current state of the first hat (PLAT_HAT_ bits).
int plat_joy_read_hat(void);
// Button and hat changes. button is from 0 to 31.
typedef void (*plat_joy_button_handler)(int button, int down);
typedef void (*plat_joy_hat_handler)(int hat);
void plat_set_joy_handlers(plat_joy_button_handler button, plat_joy_hat_handler hat);
void plat_input_joy_button(int button, int down);
void plat_input_joy_hat(int hat);

// ASCENT: gamepad (SDL_Gamepad: XInput controllers and each device with a
// gamepad mapping). The first gamepad is used. The state is kept here and
// is changed only by plat_input_pad_button and plat_input_pad_axis, which
// plat_pump calls for the gamepad events (a test calls them too).
// The button numbers are those of SDL_GamepadButton, the axis numbers
// those of SDL_GamepadAxis.
#define PLAT_PAD_A           0
#define PLAT_PAD_B           1
#define PLAT_PAD_X           2
#define PLAT_PAD_Y           3
#define PLAT_PAD_BACK        4
#define PLAT_PAD_GUIDE       5
#define PLAT_PAD_START       6
#define PLAT_PAD_LSTICK      7
#define PLAT_PAD_RSTICK      8
#define PLAT_PAD_LSHOULDER   9
#define PLAT_PAD_RSHOULDER   10
#define PLAT_PAD_DPAD_UP     11
#define PLAT_PAD_DPAD_DOWN   12
#define PLAT_PAD_DPAD_LEFT   13
#define PLAT_PAD_DPAD_RIGHT  14
#define PLAT_PAD_BUTTONS     26
#define PLAT_PAD_LEFTX       0
#define PLAT_PAD_LEFTY       1
#define PLAT_PAD_RIGHTX      2
#define PLAT_PAD_RIGHTY      3
#define PLAT_PAD_LTRIGGER    4
#define PLAT_PAD_RTRIGGER    5
#define PLAT_PAD_AXES        6
// Start the gamepad support. Returns 1 if a gamepad is open.
int plat_pad_init(void);
// 1 if a gamepad is open or a test sent a gamepad event
int plat_pad_present(void);
// The state: a button is 0 or 1, an axis -32768 to 32767 (the triggers 0
// to 32767).
int plat_pad_button(int button);
int plat_pad_axis(int axis);
// The presses of a button since the last call for it
int plat_pad_button_presses(int button);
void plat_input_pad_button(int button, int down);
void plat_input_pad_axis(int axis, int value);
// The menus: while no game frame was seen for 0.1 s, the d-pad and the
// left stick give the arrow keys, A Enter and B Escape (as key events).
// Start gives Escape in the game and in the menus. The game calls
// plat_pad_game_frame in each frame that reads its controls.
void plat_pad_game_frame(void);

// ---------------------------------------------------------------------
// Audio (audio.c). One output device and a software mixer that runs on
// the audio thread. Each function can be called from the game thread at
// any time. Before plat_audio_init and after plat_audio_close the
// functions do nothing and plat_audio_play returns -1.

// The number of sample channels of the mixer.
#define PLAT_AUDIO_CHANNELS     32
// Volume is from 0 to PLAT_AUDIO_VOLUME_MAX. A larger value is clamped.
#define PLAT_AUDIO_VOLUME_MAX   0x7fff
// Pan is from 0 (left) to 0xffff (right). The pan law is constant power:
// a sample in the center plays at 0.707 of its volume on each side.
#define PLAT_AUDIO_PAN_LEFT     0
#define PLAT_AUDIO_PAN_CENTER   0x8000
#define PLAT_AUDIO_PAN_RIGHT    0xffff

// Open the default output device and start the mixer. Returns 0 if successful.
int plat_audio_init(void);
// Stop all sound and close the device.
void plat_audio_close(void);

// Play unsigned 8-bit mono PCM. length is the number of samples and rate
// is the sample rate in Hz. If loop is not 0, the sample repeats until
// plat_audio_stop. Returns a channel handle (0 or more), or -1 if there is
// no free channel. The mixer does not copy the data: the data must stay
// valid until plat_audio_close, or for 100 ms after the channel stops.
int plat_audio_play(const uint8_t *data, int length, int rate, int volume, int pan, int loop);
// Stop a channel with a short fade. A handle of a channel that ended is ignored.
void plat_audio_stop(int handle);
void plat_audio_stop_all(void);
// Release all the channels now, with no fade. After this call the mixer
// does not read sample data, so the caller can release that data.
void plat_audio_flush(void);
void plat_audio_set_volume(int handle, int volume);
void plat_audio_set_pan(int handle, int pan);
// Returns 1 while the channel plays. Returns 0 after the sample ended or
// after plat_audio_stop.
int plat_audio_is_playing(int handle);
// The volume of the whole output (samples and music).
void plat_audio_set_master_volume(int volume);

// Streamed music. The mixer calls the callback on the audio thread when it
// needs more data. The callback writes up to `frames` frames of signed
// 16-bit stereo data (left, right) at the rate given to
// plat_audio_music_start, and returns the number of frames that it wrote.
// The mixer plays silence for the frames that the callback did not write.
// The callback must not call a plat_audio function.
typedef int (*plat_audio_music_callback)(void *user, int16_t *buffer, int frames);
// Start the music source. It replaces the current source.
void plat_audio_music_start(plat_audio_music_callback callback, void *user, int rate);
// Stop the music source. The mixer does not call the callback after this
// function returns.
void plat_audio_music_stop(void);
void plat_audio_music_set_volume(int volume);

// Play signed 16-bit mono PCM. The other arguments, the return value and
// the rule for the data are those of plat_audio_play. A sample value of
// 32767 gives the same output as the value 255 of an 8-bit sample, within
// one part in 256.
int plat_audio_play16(const int16_t *data, int length, int rate, int volume, int pan, int loop);

// ---------------------------------------------------------------------
// Window (video.c)

// Call before plat_init. 1: the window starts as a window. 0 (the default):
// the window starts in borderless full screen at the desktop resolution.
// Alt+Enter switches between the two forms.
void plat_set_start_windowed(int windowed);
// A hidden window for automated runs (tests). It is not full screen and
// does not take the focus. Call it before plat_init.
void plat_set_start_hidden(int hidden);
// The window icon: a Windows .ICO file (DTU.ICO of the game disc). NULL or
// a file that cannot be read: the default icon. Call it before plat_init.
void plat_set_window_icon_file(const char *path);
// Send silence to the audio device (the mixer still runs). Automated test
// runs and the option -mute use it. It can be called before the audio init.
void plat_audio_set_muted(int muted);
// 1 when the window is in the borderless full screen form.
int plat_is_fullscreen(void);

// ---------------------------------------------------------------------
// GPU renderer (gpu.c, gputex.c). OpenGL 3.3 core. The GPU draws the 3D
// view into its own image, at a multiple of the size of the view canvas.
// The image has one of two forms:
// - 8-bit: palette indexes. The colors are those of the software renderer
//   (the fade table gives the light).
// - RGB: 32-bit color. The textures are RGBA (the bitmaps expanded with the
//   palette of the view, or the upscaled texture set), with mipmaps and
//   filters. The light is a curve for each color channel and each fade
//   table row, made from the fade table and the palette (gpu.c).
// plat_present composes that image with the 8-bit frame: in the rectangle
// given by plat_gpu_set_view_dest, a frame pixel of color 255 (the
// transparent color) shows the GPU image. The palette of the frame (pal6)
// is applied to the image of both forms (for the RGB form, a curve for each
// channel maps the palette of the view to pal6), so the palette effects
// (flashes, fades) show on the 3D view. A screen shader can process the
// composed image (plat_gpu_bind_settings).

// Returns 1 if the GPU renderer can be used (the OpenGL context exists).
int plat_gpu_available(void);

// The settings that the presentation reads at each frame: pointers to ints
// of the engine, which can change at any time. The shader is used when
// *enable is not 0. *shader is one of PLAT_GPU_SHADER_*. NULL: off. It can
// be called before plat_init.
#define PLAT_GPU_SHADER_OFF		0
#define PLAT_GPU_SHADER_SUBTLE	1		// a light bloom and a soft blend of the pixel edges
#define PLAT_GPU_SHADER_VGA		2		// a mid range VGA monitor of 1997
void plat_gpu_bind_settings(const int *enable, const int *shader);

// A vertex of a polygon. x and y are canvas pixels (the screen position of
// the 3d library) times z, z is the view depth. The position is
// homogeneous, so a vertex can lie behind the eye: the GPU clips the
// polygon at the depth 1/16. u and v are texture coordinates (1.0 is the
// width or the height of the bitmap). l is the light (1.0 is full light;
// the fade table row is l * 32), interpolated with perspective. When lq[0]
// is 0 or more, the light is the bilinear mix of the four corner lights
// lq[0] to lq[3] at the place s, t of the face (corner 0 at 0,0, 1 at 1,0,
// 2 at 1,1, 3 at 0,1), and l is not used.
typedef struct plat_gpu_vertex {
	float x, y, z;
	float u, v, l;
	float s, t;
	float lq[4];
} plat_gpu_vertex;

#define PLAT_GPU_TEXTURED	0		// bitmap, optional fade table light
#define PLAT_GPU_FLAT		1		// one palette color
#define PLAT_GPU_DARKEN		2		// dest = fade table[level][dest]

typedef struct plat_gpu_material {
	int mode;			// PLAT_GPU_*
	int texture;		// TEXTURED: a texture handle of the form of the view
	int lit;				// TEXTURED: apply the fade table row of the light
	int transparent;	// TEXTURED: color 255 (alpha 0 in RGBA) is not drawn
	int clamp;			// TEXTURED: no wrap of u and v
	int color;			// FLAT: the palette index. DARKEN: the fade table row
	int heat;			// TEXTURED, FLAT: PLAT_GPU_HEAT_* (only with plat_gpu_set_infravision)
	int effects;		// TEXTURED: the texels of the effect colors (see plat_gpu_fx_mark) take
						// their color from the image under the polygon (texmap/tmapfx.c)
	int fx_row;			// TEXTURED with effects: the fade table row of effect color 0xCF
	int dim;				// TEXTURED, lit: the dark view of the conversation window (0 to 15,
						// texmap/tmapfx.c): a texel of brightness step <= dim becomes
						// (fade[max(row - 5, 0)][c] & 15) | 8
} plat_gpu_material;

// The effect colors of the GPU renderer (texmap/tmapfx.c,
// docs/re/effectcolors.md): 0xCF, 0xBF, 0xBE, 0x6F, 0x6E, 0x6D, 0x4F, 0x4E
// are effect k = 1 to 8. In an RGBA texture an effect texel has the alpha
// 255 - 8 * k (all above 0.5, so it is not transparent) and the color of
// the texels next to it (so that the filters do not mix the palette color
// of the effect color, a bright red, into the texture). The 8-bit form
// tests the palette index. plat_gpu_fx_code: k for a palette index, or 0.
int plat_gpu_fx_code(int c);
// Mark the effect texels of an RGBA image of w x h pixels from the 8-bit
// pixels index (iw x ih, the same picture at another size). Any thread.
void plat_gpu_fx_mark(uint8_t *rgba, int w, int h, const uint8_t *index, int iw, int ih);

// The heat of a polygon for the improved infravision of the RGB form.
#define PLAT_GPU_HEAT_COLD	0		// walls, undead, things: dark and cool
#define PLAT_GPU_HEAT_WARM	1		// living creatures and characters: red
#define PLAT_GPU_HEAT_HOT	2		// light sources and fire: the bright texels orange to yellow
#define PLAT_GPU_HEAT_GLOW	3		// lava: the whole surface, dark red to yellow

// The flags of plat_gpu_begin_view
#define PLAT_GPU_VIEW_DEPTH		1	// a depth buffer hides the pixels behind a nearer polygon (else painter's order)
#define PLAT_GPU_VIEW_RGB		2	// the RGB form (textures of plat_gpu_texture_create_rgba)
#define PLAT_GPU_VIEW_ANISO		4	// RGB form: mipmaps and 8x anisotropic filtering

// Start the 3D view. canvas_w and canvas_h give the coordinate space of the
// vertices. The image is target_w by target_h pixels. flags: PLAT_GPU_VIEW_*.
// clear_color is the palette index of the background.
void plat_gpu_begin_view(int canvas_w, int canvas_h, int target_w, int target_h, int flags, int clear_color);
// Draw a convex polygon (n from 3 to 128) in the order of the calls.
void plat_gpu_draw(const plat_gpu_vertex *v, int n, const plat_gpu_material *m);
// The later polygons are drawn over the earlier ones (depth buffer cleared).
void plat_gpu_clear_depth(void);
void plat_gpu_end_view(void);
// The improved infravision of the RGB form: 1 colors the polygons of the
// next views by their heat (plat_gpu_material.heat), 0 is the normal view.
// Read at plat_gpu_begin_view. The 8-bit form ignores it.
void plat_gpu_set_infravision(int on);
// The fade table: levels rows of 256 palette indexes.
void plat_gpu_set_fade_table(const uint8_t *table, int levels);
// The palette of the bitmaps of the 3D view (768 bytes, 6 bits for each
// component). The RGB form uses it for the flat colors and the light
// curves, and maps it to the palette of the frame.
void plat_gpu_set_view_palette(const uint8_t *pal6);
// 8-bit textures of w by h pixels, rows from the top, no padding.
// plat_gpu_texture_create returns a handle (more than 0), or 0.
int plat_gpu_texture_create(int w, int h, const uint8_t *pixels);
void plat_gpu_texture_update(int texture, int w, int h, const uint8_t *pixels);
// RGBA textures (8 bits for each component, rows from the top) for the RGB
// form. The mipmaps are made. Returns a handle (more than 0), or 0.
#define PLAT_GPU_TEX_SMOOTH		1	// linear magnification (an upscaled texture)
int plat_gpu_texture_create_rgba(int w, int h, const uint8_t *rgba, int flags);
void plat_gpu_texture_delete(int texture);
// Load an upscaled texture in the background (gputex.c) and put it into the
// RGBA texture when it is ready, at a later plat_gpu_begin_view. The data of
// the job is copied. If no file of the job is valid, the texture keeps its
// content. A layer is a PNG file (path) that is scale times the original
// size, or, if path is NULL or not valid, the original pixels (rgba). The
// alpha of the result is 0 or 255: the alpha of the file cut at 128 (the
// mask upscaled with smooth edges), or the alpha of the original pixels;
// the top is blended over the base with the alpha of its file.
typedef struct plat_gpu_hd_layer {
	const char *path;		// the PNG file, or NULL
	int w, h;				// the original size
	const uint8_t *rgba;	// the original pixels (w*h*4)
	const uint8_t *index;	// the original 8-bit pixels (w*h): color 254 for super transparency, or NULL
} plat_gpu_hd_layer;
typedef struct plat_gpu_hd_job {
	plat_gpu_hd_layer base;
	plat_gpu_hd_layer top;	// a second layer drawn over the base (texmerge.c), or rgba NULL
	int orient;				// the rotation of the top layer, 0 to 3 (texmerge.c)
	int super;				// color 254 of the top layer is a hole
	int scale;				// the scale of the files
	int max_size;			// the largest width or height on the GPU (an image is halved until it fits)
	const uint8_t *fx_index;	// the 8-bit pixels of the merged bitmap for plat_gpu_fx_mark, or NULL
	int fx_w, fx_h;
} plat_gpu_hd_job;
void plat_gpu_texture_load_hd(int texture, const plat_gpu_hd_job *job);
// The number of upscaled texture loads that are not finished.
int plat_gpu_hd_pending(void);
// The memory of the RGBA textures on the GPU, in bytes (with the mipmaps).
int64_t plat_gpu_texture_bytes(void);
// Where the next presented frames show the GPU image: a rectangle of frame
// pixels. w or h 0: no GPU image. It also ends the darkened rectangles.
void plat_gpu_set_view_dest(int x, int y, int w, int h);
// Darken the GPU image in a rectangle of frame pixels (x1, y1 not included)
// with the fade table row level, as the software code darkens the 8-bit
// pixels of the frame (the fade table keeps the color 255 that shows the
// GPU image). Up to 4 rectangles, until the next plat_gpu_set_view_dest.
void plat_gpu_view_darken(int x0, int y0, int x1, int y1, int level);
// The composed image (the frame with the GPU image, as the window shows
// it, with the screen shader) at the resolution of the GPU image: the
// frame size times the scale of the GPU image. pixels, w, h, pitch and pal6
// are as for plat_present. Returns a buffer of *out_w * *out_h RGB pixels
// (free it with free), or NULL if no GPU image is shown.
uint8_t *plat_gpu_screenshot(const uint8_t *pixels, int w, int h, int pitch, const uint8_t *pal6, int *out_w, int *out_h);

// The upscaled interface pictures (gpu.c). The engine tells where the 8-bit
// frame has the pixels of a picture; the presentation shows the upscaled
// file of the picture at the window resolution there, and the 8-bit pixel
// where the frame pixel is different from the picture pixel (text, icons,
// the cursor, the view). The palette effects apply as to the frame.
//
// The 8-bit pixels of picture id (w by h, rows from the top, copied) as
// the game loaded it, and its upscaled file: a PNG of an integer multiple
// of the size, loaded at the first draw. A picture that is given again
// keeps its file and its texture. overlay (or NULL): a PNG of the size of
// the file, drawn over it with its alpha (the picture of the language,
// docs/formats/lang.md); a new overlay makes the texture again.
void plat_gpu_ui_picture(int id, int w, int h, const uint8_t *pixels, const char *path, const char *overlay);
// Picture id was drawn into the frame: frame pixel (x, y) of the rectangle
// (dx, dy, dw, dh) has the picture pixel (sx + (x - dx) * sw / dw,
// sy + (y - dy) * sh / dh). The draw stays valid at the next frames while
// most of its pixels still have the picture colors, or until a later draw
// covers its rectangle.
void plat_gpu_ui_draw(int id, int sx, int sy, int sw, int sh, int dx, int dy, int dw, int dh);
// The same for a draw that skipped the picture pixels of index key (the
// frame keeps the pixels below there). The file has alpha 0 at those
// pixels, so the upscaled draws below show through.
void plat_gpu_ui_draw_key(int id, int sx, int sy, int sw, int sh, int dx, int dy, int dw, int dh, int key);
// The frame pixels in the rectangle (x1, y1 not included) went through the
// table (256 palette indexes): the darkening of the 2d library.
void plat_gpu_ui_darken(int x0, int y0, int x1, int y1, const uint8_t *table);
// Forget all draws.
void plat_gpu_ui_clear(void);
// The palette (768 bytes, 6 bits) of the pictures at full light, read at
// each present: the colors of the upscaled files go through the curve from
// it to the palette of the frame.
void plat_gpu_ui_bind_palette(const uint8_t *pal6);

// ---------------------------------------------------------------------
// Directories (paths.c). Each function writes the name without the
// separator at the end and returns 1, or returns 0 if it has no name.

// The directory of the executable.
int plat_base_dir(char *out, int size);
// The directory of the user for the files of the program app (for example
// %APPDATA%\ASCENT on Windows). The function makes the directory.
int plat_pref_dir(const char *app, char *out, int size);
// Show an error message in a window of the system (no game window needed).
void plat_message_box(const char *title, const char *text);

// ---------------------------------------------------------------------
// The session log and the crash handler (plat_log.c). Each run of the game
// writes one log file, which a player can send when something goes wrong.
// Each line is written to the file at once (a crash does not lose it).
// The functions can be called from any thread, and before plat_init.

// Open the log: dir/ascent-YYYY-MM-DD_HH-MM-SS.log (the start time). The
// function makes dir and deletes the oldest logs (and their crash dumps)
// so that keep logs remain. It notes in the new log when the newest older
// log has no end line (the session before did not end cleanly). Returns 1
// if the log is open.
int plat_log_open(const char *dir, int keep);
// The file name of the log, or NULL.
const char *plat_log_path(void);
// One message: a line with the time since the start. A message with line
// ends gives more lines.
void plat_log(const char *format, ...);
// plat_log, and the message also goes to stderr.
void plat_log_error(const char *format, ...);
// The last line ("Session end: how") and close the log. A log without it
// is the log of a session that did not end cleanly.
void plat_log_close(const char *how);
// The system: the operating system, the CPU, the memory, the SDL version.
void plat_log_system(void);
// The video driver, the displays, the window and the OpenGL renderer.
// Call it after plat_init.
void plat_log_video(void);
// Report the OpenGL errors (glGetError) that are pending, with where.
void plat_log_gl_errors(const char *where);
// plat_pump calls it: a check of the OpenGL errors every few seconds.
void plat_log_tick(void);
// Install the crash handler: an exception, abort() or a fatal signal
// writes the place, the registers and the stack to the log and, on
// Windows, a minidump (the name of the log with .dmp). quiet 0: a message
// box tells the player where the files are.
void plat_crash_install(int quiet);

#endif
