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
	SDL_SetRenderLogicalPresentation(Renderer, w, h, SDL_LOGICAL_PRESENTATION_LETTERBOX);
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
	if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS))
		return -1;
	Quit_requested = 0;
	Win_w = w * scale;
	Win_h = h * scale;

	if (open_gl_window(title, Win_w, Win_h) == 0) {
		Use_gl = 1;
		// The game draws its own cursor.
		SDL_HideCursor();
		return plat_set_frame_size(w, h);
	}

	Use_gl = 0;
	if (!SDL_CreateWindowAndRenderer(title, Win_w, Win_h,
			SDL_WINDOW_RESIZABLE | (Start_hidden ? SDL_WINDOW_HIDDEN : 0),
			&Window, &Renderer))
		return -1;
	if (!Start_hidden && !Start_windowed)
		enter_borderless();
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
			rh = h * k;
			rgb = (uint8_t *) malloc((size_t) rw * rh * 3);
			if (!rgb)
				return;
			for (y = 0; y < rh; y++)
				for (x = 0; x < rw; x++) {
					int c = pixels[(size_t) (y / k) * pitch + x / k], i;
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
	SDL_RenderTexture(Renderer, Frame_texture, NULL, NULL);
	SDL_RenderPresent(Renderer);
}

// Window coordinates to frame pixels (the OpenGL form).
static void window_to_frame(float *x, float *y)
{
	float rx, ry, rw, rh;
	int fw, fh;

	gpu_frame_rect(&rx, &ry, &rw, &rh, &fw, &fh);
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
		if (Use_gl) {
			if (ev.type == SDL_EVENT_MOUSE_MOTION)
				window_to_frame(&ev.motion.x, &ev.motion.y);
			else if (ev.type == SDL_EVENT_MOUSE_BUTTON_DOWN || ev.type == SDL_EVENT_MOUSE_BUTTON_UP)
				window_to_frame(&ev.button.x, &ev.button.y);
		} else {
			float xrel = 0, yrel = 0;
			if (ev.type == SDL_EVENT_MOUSE_MOTION) {
				xrel = ev.motion.xrel;
				yrel = ev.motion.yrel;
			}
			if (Renderer)
				SDL_ConvertEventToRenderCoordinates(Renderer, &ev);
			if (ev.type == SDL_EVENT_MOUSE_MOTION) {
				ev.motion.xrel = xrel;
				ev.motion.yrel = yrel;
			}
		}
		plat_input_sdl_event(&ev);
	}
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
