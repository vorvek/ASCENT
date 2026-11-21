/*
 * ASCENT platform layer: window, frame presentation, events and time.
 */
#include <SDL3/SDL.h>

#include "platform.h"

static SDL_Window *Window;
static SDL_Renderer *Renderer;
static SDL_Texture *Frame_texture;
static int Frame_w, Frame_h;
static int Quit_requested;

int plat_set_frame_size(int w, int h)
{
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

int plat_init(const char *title, int w, int h, int scale)
{
	if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS))
		return -1;
	if (!SDL_CreateWindowAndRenderer(title, w * scale, h * scale,
			SDL_WINDOW_RESIZABLE, &Window, &Renderer))
		return -1;
	SDL_SetRenderVSync(Renderer, 1);
	Quit_requested = 0;
	return plat_set_frame_size(w, h);
}

void plat_shutdown(void)
{
	if (Frame_texture)
		SDL_DestroyTexture(Frame_texture);
	if (Renderer)
		SDL_DestroyRenderer(Renderer);
	if (Window)
		SDL_DestroyWindow(Window);
	Frame_texture = NULL;
	Renderer = NULL;
	Window = NULL;
	SDL_Quit();
}

void plat_present(const uint8_t *pixels, int w, int h, int pitch, const uint8_t *pal6)
{
	uint32_t pal32[256];
	void *dest;
	int dest_pitch;
	int i, x, y;

	if (plat_set_frame_size(w, h) != 0)
		return;

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

int plat_pump(void)
{
	SDL_Event ev;

	while (SDL_PollEvent(&ev)) {
		if (ev.type == SDL_EVENT_QUIT)
			Quit_requested = 1;
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
