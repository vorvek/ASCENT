/*
 * ASCENT platform layer: window, frame presentation, events and time.
 *
 * The window has an OpenGL 3.3 core context. gpu.c shows the frames and
 * draws the 3D view of the GPU renderer. If the context cannot be made,
 * the frames are shown with SDL_Renderer, and plat_gpu_available
 * returns 0.
 */
#include <stdio.h>
#include <stdlib.h>

#include <SDL3/SDL.h>

#include "platform.h"
#include "plat_input.h"
#include "plat_video.h"

static SDL_Window *Window;
static SDL_GLContext Gl_context;
static int Use_gl;
static SDL_Renderer *Renderer;
static SDL_Texture *Frame_texture;
static int Frame_w, Frame_h;
static int Quit_requested;
static int Start_windowed;
static int Win_w, Win_h;		// the size of the window form

// ASCENT: the shape of the frame on the screen (plat_bind_vga_aspect)
static const int *Vga_aspect;
// ASCENT: the rectangle of the SDL_Renderer form that shows the frame, in
// window coordinates (the OpenGL form: gpu_frame_rect)
static float Sdl_rect_x, Sdl_rect_y, Sdl_rect_w, Sdl_rect_h;

void plat_bind_vga_aspect(const int *on)
{
	Vga_aspect = on;
}

// The DOS VGA modes that filled a 4:3 monitor with pixels that are not
// square. A 640x480 frame is 4:3 with square pixels; a frame of another
// size (the 432x320 movies, shown 1:1 in a 640x480 mode) keeps its shape.
float plat_frame_aspect(int fw, int fh)
{
	if (fw <= 0 || fh <= 0)
		return 1.0f;
	if ((!Vga_aspect || *Vga_aspect) &&
			((fw == 320 && (fh == 200 || fh == 400)) || (fw == 640 && fh == 400)))
		return 4.0f / 3.0f;
	return (float) fw / fh;
}

void plat_fit_frame(int fw, int fh, int ow, int oh, float *x, float *y, float *w, float *h)
{
	float a = plat_frame_aspect(fw, fh);

	*w = (float) ow;
	*h = *w / a;
	if (*h > oh) {
		*h = (float) oh;
		*w = *h * a;
	}
	*x = (ow - *w) / 2;
	*y = (oh - *h) / 2;
}

void plat_set_start_windowed(int windowed)
{
	Start_windowed = (windowed != 0);
}

// ASCENT: a hidden window for automated runs (tests), so that a test does
// not cover the desktop or take the focus. A hidden window is not full
// screen.
static int Start_hidden;

void plat_set_start_hidden(int hidden)
{
	Start_hidden = (hidden != 0);
}

// ASCENT: the window icon, a Windows .ICO file (DTU.ICO of the game disc,
// docs/formats/misc.md). The image with the most colors is used. The file
// has BMP images without compression (1, 4 or 8 bits for each pixel, rows
// from the bottom up) and a mask of 1 bit for each pixel (1: transparent).
static char Icon_path[1024];

void plat_set_window_icon_file(const char *path)
{
	SDL_snprintf(Icon_path, sizeof(Icon_path), "%s", path ? path : "");
}

static unsigned rd16(const Uint8 *p) { return p[0] | (p[1] << 8); }
static Uint32 rd32(const Uint8 *p) { return p[0] | (p[1] << 8) | (p[2] << 16) | ((Uint32) p[3] << 24); }

static void apply_window_icon(void)
{
	size_t size;
	Uint8 *d;
	unsigned n, i, best = 0, best_bpp = 0;
	SDL_Surface *s;

	if (!Icon_path[0] || !Window)
		return;
	d = SDL_LoadFile(Icon_path, &size);
	if (!d)
		return;
	n = size >= 6 && rd16(d) == 0 && rd16(d + 2) == 1 ? rd16(d + 4) : 0;
	for (i = 0; i < n && 6 + 16 * (i + 1) <= size; i++) {
		Uint32 off = rd32(d + 6 + 16 * i + 12);
		unsigned bpp;
		if (off + 40 > size || rd32(d + off) != 40 || rd32(d + off + 16) != 0)
			continue;
		bpp = rd16(d + off + 14);
		if ((bpp == 1 || bpp == 4 || bpp == 8) && bpp > best_bpp) {
			best_bpp = bpp;
			best = i;
		}
	}
	if (best_bpp) {
		Uint32 off = rd32(d + 6 + 16 * best + 12);
		int w = (int) rd32(d + off + 4), h = (int) rd32(d + off + 8) / 2;
		unsigned ncol = rd32(d + off + 32) ? rd32(d + off + 32) : 1u << best_bpp;
		const Uint8 *pal = d + off + 40;
		const Uint8 *xor_rows = pal + 4 * ncol;
		size_t xor_pitch = ((size_t) w * best_bpp + 31) / 32 * 4;
		size_t and_pitch = ((size_t) w + 31) / 32 * 4;
		const Uint8 *and_rows = xor_rows + xor_pitch * h;

		if (w > 0 && w <= 256 && h > 0 && h <= 256 && ncol <= 256 &&
				(size_t) (and_rows - d) + and_pitch * h <= size &&
				(s = SDL_CreateSurface(w, h, SDL_PIXELFORMAT_ARGB8888)) != NULL) {
			int x, y;
			for (y = 0; y < h; y++) {
				const Uint8 *xr = xor_rows + xor_pitch * (h - 1 - y);
				const Uint8 *ar = and_rows + and_pitch * (h - 1 - y);
				Uint32 *out = (Uint32 *) ((Uint8 *) s->pixels + s->pitch * y);
				for (x = 0; x < w; x++) {
					unsigned c, bit = x * best_bpp;
					const Uint8 *q;
					c = (xr[bit / 8] >> (8 - best_bpp - bit % 8)) & ((1u << best_bpp) - 1);
					q = pal + 4 * (c < ncol ? c : 0);		// blue, green, red, 0
					out[x] = ((ar[x / 8] >> (7 - x % 8)) & 1) ? 0 :
						0xFF000000u | ((Uint32) q[2] << 16) | ((Uint32) q[1] << 8) | q[0];
				}
			}
			SDL_SetWindowIcon(Window, s);
			SDL_DestroySurface(s);
		}
	}
	SDL_free(d);
}

int plat_set_frame_size(int w, int h)
{
	if (Use_gl) {
		Frame_w = w;
		Frame_h = h;
		return 0;
	}
	if (Frame_texture && w == Frame_w && h == Frame_h)
		return 0;
	if (Frame_texture)
		SDL_DestroyTexture(Frame_texture);
	Frame_texture = SDL_CreateTexture(Renderer, SDL_PIXELFORMAT_XRGB8888,
		SDL_TEXTUREACCESS_STREAMING, w, h);
	if (!Frame_texture)
		return -1;
	SDL_SetTextureScaleMode(Frame_texture, SDL_SCALEMODE_NEAREST);
	// ASCENT: no logical presentation: plat_present places the frame with
	// the shape of plat_frame_aspect, and plat_pump maps the mouse.
	Frame_w = w;
	Frame_h = h;
	return 0;
}

// ASCENT: the full screen form is a borderless window that covers its
// monitor exactly. It does not use the SDL full screen state: with that
// state some graphics drivers treat the OpenGL window as exclusive full
// screen and change the display configuration, which turned another
// monitor black. A borderless window changes no display mode.
static int Borderless;

static void enter_borderless(void)
{
	SDL_DisplayID d = SDL_GetDisplayForWindow(Window);
	SDL_Rect r;

	if (!d)
		d = SDL_GetPrimaryDisplay();
	if (!d || !SDL_GetDisplayBounds(d, &r))
		return;
	SDL_SetWindowBordered(Window, false);
	SDL_SetWindowResizable(Window, false);
	// One line (one row of pixels) more than the monitor: a window that
	// covers the monitor exactly can still make a driver use exclusive
	// full screen.
	SDL_SetWindowSize(Window, r.w, r.h + 1);
	SDL_SetWindowPosition(Window, r.x, r.y);
	SDL_RaiseWindow(Window);
	Borderless = 1;
}

static void leave_borderless(void)
{
	SDL_SetWindowBordered(Window, true);
	SDL_SetWindowResizable(Window, true);
	SDL_SetWindowSize(Window, Win_w, Win_h);
	SDL_SetWindowPosition(Window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
	Borderless = 0;
}

int plat_is_fullscreen(void)
{
	return Borderless;
}

// The OpenGL window. Returns 0 if successful.
static int open_gl_window(const char *title, int ww, int wh)
{
	const char *e;
	SDL_WindowFlags flags = SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE;

	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 0);
	SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 0);

	if (Start_hidden) {
		int w, h;
		flags |= SDL_WINDOW_HIDDEN;
		// ASCENT_WINDOW_SIZE=WxH: the size of the hidden window of a test
		// (for the measurement of the scale to a large window)
		e = SDL_getenv("ASCENT_WINDOW_SIZE");
		if (e && SDL_sscanf(e, "%dx%d", &w, &h) == 2 && w >= 64 && h >= 64 && w <= 8192 && h <= 8192) {
			ww = w;
			wh = h;
		}
	}
	Window = SDL_CreateWindow(title, ww, wh, flags);
	if (!Window)
		return -1;
	if (!Start_hidden && !Start_windowed)
		enter_borderless();
	Gl_context = SDL_GL_CreateContext(Window);
	if (!Gl_context || !SDL_GL_MakeCurrent(Window, Gl_context) || gpu_init(Window) != 0) {
		if (Gl_context)
			SDL_GL_DestroyContext(Gl_context);
		Gl_context = NULL;
		SDL_DestroyWindow(Window);
		Window = NULL;
		return -1;
	}
	// ASCENT_VSYNC=0 turns off the wait for the vertical blank (for the
	// measurement of the frame time).
	e = SDL_getenv("ASCENT_VSYNC");
	SDL_GL_SetSwapInterval((e && e[0] == '0') ? 0 : 1);
	return 0;
}

int plat_init(const char *title, int w, int h, int scale)
{
	// ASCENT: the relative mode (the immersive mode) holds the system
	// pointer at the center of the window (input.c, plat_input_hold_pointer)
	SDL_SetHint(SDL_HINT_MOUSE_RELATIVE_MODE_CENTER, "1");
	if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS))
		return -1;
	Quit_requested = 0;
	Win_w = w * scale;
	Win_h = (int) (Win_w / plat_frame_aspect(w, h) + 0.5f);	// ASCENT: 960x720 for 320x200 at 4:3

	// ASCENT_NO_GL=1: the SDL_Renderer form (a test aid for the systems
	// without OpenGL 3.3)
	if (!SDL_getenv("ASCENT_NO_GL") && open_gl_window(title, Win_w, Win_h) == 0) {
		Use_gl = 1;
		apply_window_icon();
		// The game draws its own cursor.
		SDL_HideCursor();
		return plat_set_frame_size(w, h);
	}

	// ASCENT: the session log (plat_log.c) tells why the GPU renderer is off
	plat_log("OpenGL 3.3 window not available (%s): the SDL renderer is used", SDL_GetError());
	Use_gl = 0;
	if (Start_hidden) {
		// ASCENT: ASCENT_WINDOW_SIZE as in open_gl_window
		const char *e = SDL_getenv("ASCENT_WINDOW_SIZE");
		int ew, eh;
		if (e && SDL_sscanf(e, "%dx%d", &ew, &eh) == 2 && ew >= 64 && eh >= 64 && ew <= 8192 && eh <= 8192) {
			Win_w = ew;
			Win_h = eh;
		}
	}
	if (!SDL_CreateWindowAndRenderer(title, Win_w, Win_h,
			SDL_WINDOW_RESIZABLE | (Start_hidden ? SDL_WINDOW_HIDDEN : 0),
			&Window, &Renderer))
		return -1;
	if (!Start_hidden && !Start_windowed)
		enter_borderless();
	apply_window_icon();
	SDL_SetRenderVSync(Renderer, 1);
	SDL_HideCursor();
	return plat_set_frame_size(w, h);
}

void plat_shutdown(void)
{
	if (Use_gl)
		gpu_shutdown();
	if (Gl_context)
		SDL_GL_DestroyContext(Gl_context);
	if (Frame_texture)
		SDL_DestroyTexture(Frame_texture);
	if (Renderer)
		SDL_DestroyRenderer(Renderer);
	if (Window)
		SDL_DestroyWindow(Window);
	Gl_context = NULL;
	Frame_texture = NULL;
	Renderer = NULL;
	Window = NULL;
	Use_gl = 0;
	SDL_Quit();
}

// ASCENT: a test aid. ASCENT_SHOT_PRESENT="N:FILE.ppm;N2:FILE2.ppm" writes
// the frame of the Nth call of plat_present with the palette of that
// call (the palette effects are in it), as the window shows it: the
// composed image of the GPU renderer, or the 8-bit frame repeated
// ASCENT_SHOT_SCALE times (default 3).
static void shot_at_present(const uint8_t *pixels, int w, int h, int pitch, const uint8_t *pal6)
{
	static int count;
	const char *e = SDL_getenv("ASCENT_SHOT_PRESENT");
	char file[300];
	int n, len;

	count++;
	while (e && *e) {
		uint8_t *rgb = NULL;
		int rw = 0, rh = 0;
		FILE *f;

		if (sscanf(e, "%d:%299[^;]%n", &n, file, &len) != 2)
			return;
		e += len;
		if (*e == ';')
			e++;
		if (n != count)
			continue;
		if (Use_gl)
			rgb = plat_gpu_screenshot(pixels, w, h, pitch, pal6, &rw, &rh);
		if (!rgb) {
			const char *s = SDL_getenv("ASCENT_SHOT_SCALE");
			int k = (s && SDL_atoi(s) >= 1 && SDL_atoi(s) <= 8) ? SDL_atoi(s) : 3, x, y;
			rw = w * k;
			rh = (int) (rw / plat_frame_aspect(w, h) + 0.5f);	// ASCENT: the shape on the screen
			rgb = (uint8_t *) malloc((size_t) rw * rh * 3);
			if (!rgb)
				return;
			for (y = 0; y < rh; y++)
				for (x = 0; x < rw; x++) {
					int c = pixels[(size_t) (y * h / rh) * pitch + x / k], i;
					for (i = 0; i < 3; i++) {
						int v = pal6[c * 3 + i] & 63;
						rgb[((size_t) y * rw + x) * 3 + i] = (uint8_t) ((v << 2) | (v >> 4));
					}
				}
		}
		f = fopen(file, "wb");
		if (f) {
			fprintf(f, "P6\n%d %d\n255\n", rw, rh);
			fwrite(rgb, 3, (size_t) rw * rh, f);
			fclose(f);
			printf("Wrote %s at present %d\n", file, count);
		}
		free(rgb);
	}
}

void plat_present(const uint8_t *pixels, int w, int h, int pitch, const uint8_t *pal6)
{
	shot_at_present(pixels, w, h, pitch, pal6);
	uint32_t pal32[256];
	void *dest;
	int dest_pitch;
	int i, x, y;

	if (plat_set_frame_size(w, h) != 0)
		return;

	if (Use_gl) {
		gpu_present(pixels, w, h, pitch, pal6);
		return;
	}

	for (i = 0; i < 256; i++) {
		// Expand 6 bits to 8 bits.
		uint32_t r = pal6[i * 3] & 63, g = pal6[i * 3 + 1] & 63, b = pal6[i * 3 + 2] & 63;
		r = (r << 2) | (r >> 4);
		g = (g << 2) | (g >> 4);
		b = (b << 2) | (b >> 4);
		pal32[i] = 0xFF000000u | (r << 16) | (g << 8) | b;
	}

	if (!SDL_LockTexture(Frame_texture, NULL, &dest, &dest_pitch))
		return;
	for (y = 0; y < h; y++) {
		const uint8_t *src = pixels + (size_t) y * pitch;
		uint32_t *row = (uint32_t *) ((uint8_t *) dest + (size_t) y * dest_pitch);
		for (x = 0; x < w; x++)
			row[x] = pal32[src[x]];
	}
	SDL_UnlockTexture(Frame_texture);

	SDL_SetRenderDrawColor(Renderer, 0, 0, 0, 255);
	SDL_RenderClear(Renderer);
	{
		// ASCENT: the frame with its shape on the screen (plat_frame_aspect)
		int ow = 0, oh = 0, ww = 0, wh = 0;
		SDL_FRect dst;
		SDL_GetCurrentRenderOutputSize(Renderer, &ow, &oh);
		SDL_GetWindowSize(Window, &ww, &wh);
		if (ow > 0 && oh > 0) {
			plat_fit_frame(w, h, ow, oh, &dst.x, &dst.y, &dst.w, &dst.h);
			SDL_RenderTexture(Renderer, Frame_texture, NULL, &dst);
			if (ww > 0 && wh > 0) {
				Sdl_rect_x = dst.x * ww / ow;
				Sdl_rect_y = dst.y * wh / oh;
				Sdl_rect_w = dst.w * ww / ow;
				Sdl_rect_h = dst.h * wh / oh;
			}
		}
	}
	SDL_RenderPresent(Renderer);
}

void plat_get_present_rect(float *x, float *y, float *w, float *h, int *frame_w, int *frame_h, int *win_w, int *win_h)
{
	*x = *y = *w = *h = 0;
	*frame_w = Frame_w;
	*frame_h = Frame_h;
	*win_w = *win_h = 0;
	if (Window)
		SDL_GetWindowSize(Window, win_w, win_h);
	if (Use_gl)
		gpu_frame_rect(x, y, w, h, frame_w, frame_h);
	else {
		*x = Sdl_rect_x;
		*y = Sdl_rect_y;
		*w = Sdl_rect_w;
		*h = Sdl_rect_h;
	}
}

void plat_inject_window_mouse(float x, float y, int button, int down)
{
	SDL_Event ev;
	static const Uint8 buttons[3] = { SDL_BUTTON_LEFT, SDL_BUTTON_RIGHT, SDL_BUTTON_MIDDLE };

	SDL_zero(ev);
	if (button < 0) {
		ev.type = SDL_EVENT_MOUSE_MOTION;
		ev.motion.windowID = Window ? SDL_GetWindowID(Window) : 0;
		ev.motion.x = x;
		ev.motion.y = y;
	} else if (button < 3) {
		ev.type = down ? SDL_EVENT_MOUSE_BUTTON_DOWN : SDL_EVENT_MOUSE_BUTTON_UP;
		ev.button.windowID = Window ? SDL_GetWindowID(Window) : 0;
		ev.button.button = buttons[button];
		ev.button.down = down != 0;
		ev.button.clicks = 1;
		ev.button.x = x;
		ev.button.y = y;
	} else
		return;
	SDL_PushEvent(&ev);
}

// Window coordinates to frame pixels.
static void window_to_frame(float *x, float *y)
{
	float rx, ry, rw, rh;
	int fw, fh;

	// ASCENT: also the SDL_Renderer form (its rectangle of plat_present)
	if (Use_gl)
		gpu_frame_rect(&rx, &ry, &rw, &rh, &fw, &fh);
	else {
		rx = Sdl_rect_x;
		ry = Sdl_rect_y;
		rw = Sdl_rect_w;
		rh = Sdl_rect_h;
		fw = Frame_w;
		fh = Frame_h;
	}
	if (rw <= 0 || rh <= 0 || fw <= 0 || fh <= 0) {
		rx = ry = 0;
		rw = (float) Win_w;
		rh = (float) Win_h;
		fw = Frame_w;
		fh = Frame_h;
		if (rw <= 0 || rh <= 0)
			return;
	}
	*x = (*x - rx) * fw / rw;
	*y = (*y - ry) * fh / rh;
}

// ASCENT: put the system pointer on the frame pixel (x, y), so that the
// next motion event outside the relative mode starts there (mouse.c). The
// inverse of window_to_frame, to the middle of the pixel.
void plat_mouse_warp_frame(int x, int y)
{
	float rx, ry, rw, rh;
	int fw, fh;

	if (!Window || !(SDL_GetWindowFlags(Window) & SDL_WINDOW_INPUT_FOCUS))
		return;
	if (Use_gl)
		gpu_frame_rect(&rx, &ry, &rw, &rh, &fw, &fh);
	else {
		rx = Sdl_rect_x;
		ry = Sdl_rect_y;
		rw = Sdl_rect_w;
		rh = Sdl_rect_h;
		fw = Frame_w;
		fh = Frame_h;
	}
	if (rw <= 0 || rh <= 0 || fw <= 0 || fh <= 0)
		return;
	SDL_WarpMouseInWindow(Window, rx + ((float) x + 0.5f) * rw / fw, ry + ((float) y + 0.5f) * rh / fh);
}

// Alt+Enter: full screen or window.
static void toggle_fullscreen(void)
{
	int fs;

	if (!Window)
		return;
	fs = Borderless;
	if (fs)
		leave_borderless();
	else
		enter_borderless();
	plat_input_window_changed();
}

int plat_pump(void)
{
	SDL_Event ev;
	static int enter_swallowed;

	plat_log_tick();		// ASCENT: the OpenGL error check of the session log
	while (SDL_PollEvent(&ev)) {
		if (ev.type == SDL_EVENT_QUIT) {
			Quit_requested = 1;
			continue;
		}
		if ((ev.type == SDL_EVENT_KEY_DOWN || ev.type == SDL_EVENT_KEY_UP)
				&& (ev.key.scancode == SDL_SCANCODE_RETURN || ev.key.scancode == SDL_SCANCODE_KP_ENTER)) {
			if (ev.type == SDL_EVENT_KEY_DOWN && (ev.key.mod & SDL_KMOD_ALT)) {
				if (!ev.key.repeat)
					toggle_fullscreen();
				enter_swallowed = 1;
				continue;
			}
			if (ev.type == SDL_EVENT_KEY_UP && enter_swallowed) {
				enter_swallowed = 0;
				continue;
			}
		}

		// Pass the input events to input.c. The mouse position
		// becomes frame pixels. The relative motion stays in
		// device units, so that it does not depend on the window size.
		// ASCENT: both forms map with the rectangle of the last present.
		if (ev.type == SDL_EVENT_MOUSE_MOTION)
			window_to_frame(&ev.motion.x, &ev.motion.y);
		else if (ev.type == SDL_EVENT_MOUSE_BUTTON_DOWN || ev.type == SDL_EVENT_MOUSE_BUTTON_UP)
			window_to_frame(&ev.button.x, &ev.button.y);
		plat_input_sdl_event(&ev);
	}
	plat_input_hold_pointer();		// ASCENT: the immersive mode
	return !Quit_requested;
}

uint64_t plat_time_us(void)
{
	return SDL_GetTicksNS() / 1000;
}

void plat_sleep_ms(int ms)
{
	SDL_Delay(ms);
}
