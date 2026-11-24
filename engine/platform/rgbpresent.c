/*
 * ASCENT platform layer: plat_present_xrgb, the presentation of a frame of
 * 32-bit pixels (the upscaled movies of dtumovie.c).
 *
 * New file. It has no original. It does not use the state of video.c or
 * gpu.c: with an OpenGL context (gpu.c) it uploads the frame into its own
 * texture and copies it to the window with glBlitFramebuffer, and it puts
 * back the OpenGL state that it changed. Without a context it draws with
 * the SDL_Renderer of the window.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>

#include "platform.h"

#ifndef GL_READ_FRAMEBUFFER
#define GL_READ_FRAMEBUFFER			0x8CA8
#define GL_DRAW_FRAMEBUFFER			0x8CA9
#define GL_READ_FRAMEBUFFER_BINDING	0x8CAA
#define GL_DRAW_FRAMEBUFFER_BINDING	0x8CA6
#define GL_COLOR_ATTACHMENT0		0x8CE0
#define GL_FRAMEBUFFER_COMPLETE		0x8CD5
#endif
#ifndef GL_BGRA
#define GL_BGRA						0x80E1
#endif
#ifndef GL_UNPACK_ROW_LENGTH
#define GL_UNPACK_ROW_LENGTH		0x0CF2
#endif

#define RGB_GL_FUNCS(X) \
	X(void, GenTextures, (GLsizei, GLuint *)) \
	X(void, DeleteTextures, (GLsizei, const GLuint *)) \
	X(void, BindTexture, (GLenum, GLuint)) \
	X(void, TexImage2D, (GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum, GLenum, const void *)) \
	X(void, TexSubImage2D, (GLenum, GLint, GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, const void *)) \
	X(void, TexParameteri, (GLenum, GLenum, GLint)) \
	X(void, PixelStorei, (GLenum, GLint)) \
	X(void, GetIntegerv, (GLenum, GLint *)) \
	X(void, GetFloatv, (GLenum, GLfloat *)) \
	X(void, GenFramebuffers, (GLsizei, GLuint *)) \
	X(void, DeleteFramebuffers, (GLsizei, const GLuint *)) \
	X(void, BindFramebuffer, (GLenum, GLuint)) \
	X(void, FramebufferTexture2D, (GLenum, GLenum, GLenum, GLuint, GLint)) \
	X(GLenum, CheckFramebufferStatus, (GLenum)) \
	X(void, BlitFramebuffer, (GLint, GLint, GLint, GLint, GLint, GLint, GLint, GLint, GLbitfield, GLenum)) \
	X(void, Viewport, (GLint, GLint, GLsizei, GLsizei)) \
	X(void, ClearColor, (GLfloat, GLfloat, GLfloat, GLfloat)) \
	X(void, Clear, (GLbitfield))

#define X(ret, name, args) static ret (APIENTRY *rgl##name) args;
RGB_GL_FUNCS(X)
#undef X

static int Gl_loaded;			// 1 loaded, -1 failed
static GLuint Tex, Fbo;
static int Tex_w, Tex_h;
static SDL_Texture *Sdl_tex;
static SDL_Renderer *Sdl_tex_renderer;

static int load_gl(void)
{
	if (Gl_loaded)
		return Gl_loaded > 0;
#define X(ret, name, args) \
	rgl##name = (ret (APIENTRY *) args) SDL_GL_GetProcAddress("gl" #name); \
	if (!rgl##name) { Gl_loaded = -1; return 0; }
	RGB_GL_FUNCS(X)
#undef X
	Gl_loaded = 1;
	return 1;
}

// The largest rectangle with the aspect of the frame, in the middle.
static void letterbox(int pw, int ph, int w, int h, int *x, int *y, int *dw, int *dh)
{
	float s = (float) pw / w;
	if ((float) ph / h < s)
		s = (float) ph / h;
	*dw = (int) (w * s);
	*dh = (int) (h * s);
	*x = (pw - *dw) / 2;
	*y = (ph - *dh) / 2;
}

static void present_gl(SDL_Window *win, const uint32_t *pixels, int w, int h, int pitch)
{
	GLint old_tex = 0, old_read = 0, old_draw = 0, old_vp[4] = { 0 }, old_align = 4, old_row = 0;
	GLfloat old_clear[4] = { 0 };
	int pw = 0, ph = 0, x, y, dw, dh;

	rglGetIntegerv(GL_TEXTURE_BINDING_2D, &old_tex);
	rglGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &old_read);
	rglGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &old_draw);
	rglGetIntegerv(GL_VIEWPORT, old_vp);
	rglGetIntegerv(GL_UNPACK_ALIGNMENT, &old_align);
	rglGetIntegerv(GL_UNPACK_ROW_LENGTH, &old_row);
	rglGetFloatv(GL_COLOR_CLEAR_VALUE, old_clear);

	if (!Tex || w != Tex_w || h != Tex_h) {
		if (Tex)
			rglDeleteTextures(1, &Tex);
		if (!Fbo)
			rglGenFramebuffers(1, &Fbo);
		rglGenTextures(1, &Tex);
		rglBindTexture(GL_TEXTURE_2D, Tex);
		rglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		rglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		rglTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_BGRA, GL_UNSIGNED_BYTE, NULL);
		rglBindFramebuffer(GL_READ_FRAMEBUFFER, Fbo);
		rglFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, Tex, 0);
		Tex_w = w;
		Tex_h = h;
	}
	rglBindTexture(GL_TEXTURE_2D, Tex);
	rglPixelStorei(GL_UNPACK_ALIGNMENT, 4);
	rglPixelStorei(GL_UNPACK_ROW_LENGTH, pitch / 4);
	rglTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, w, h, GL_BGRA, GL_UNSIGNED_BYTE, pixels);

	SDL_GetWindowSizeInPixels(win, &pw, &ph);
	rglBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
	rglViewport(0, 0, pw, ph);
	rglClearColor(0, 0, 0, 1);
	rglClear(GL_COLOR_BUFFER_BIT);
	rglBindFramebuffer(GL_READ_FRAMEBUFFER, Fbo);
	if (pw > 0 && ph > 0 && rglCheckFramebufferStatus(GL_READ_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE) {
		letterbox(pw, ph, w, h, &x, &y, &dw, &dh);
		// The rows of the frame go from the top; OpenGL counts from the bottom.
		rglBlitFramebuffer(0, 0, w, h, x, y + dh, x + dw, y, GL_COLOR_BUFFER_BIT, GL_LINEAR);
	}
	// A capture hook on the swap (OBS Studio) reads from the read framebuffer
	// that is bound. Bind the window, or the hook copies the movie texture.
	rglBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
	SDL_GL_SwapWindow(win);

	rglBindTexture(GL_TEXTURE_2D, old_tex);
	rglBindFramebuffer(GL_READ_FRAMEBUFFER, old_read);
	rglBindFramebuffer(GL_DRAW_FRAMEBUFFER, old_draw);
	rglViewport(old_vp[0], old_vp[1], old_vp[2], old_vp[3]);
	rglPixelStorei(GL_UNPACK_ALIGNMENT, old_align);
	rglPixelStorei(GL_UNPACK_ROW_LENGTH, old_row);
	rglClearColor(old_clear[0], old_clear[1], old_clear[2], old_clear[3]);
}

static void present_renderer(SDL_Renderer *r, const uint32_t *pixels, int w, int h, int pitch)
{
	int ow = 0, oh = 0, x, y, dw, dh;
	SDL_RendererLogicalPresentation mode;
	SDL_FRect dst;

	if (!Sdl_tex || Sdl_tex_renderer != r || Sdl_tex->w != w || Sdl_tex->h != h) {
		if (Sdl_tex)
			SDL_DestroyTexture(Sdl_tex);
		Sdl_tex = SDL_CreateTexture(r, SDL_PIXELFORMAT_XRGB8888, SDL_TEXTUREACCESS_STREAMING, w, h);
		Sdl_tex_renderer = r;
		if (!Sdl_tex)
			return;
		SDL_SetTextureScaleMode(Sdl_tex, SDL_SCALEMODE_LINEAR);
	}
	SDL_UpdateTexture(Sdl_tex, NULL, pixels, pitch);

	// Draw in output pixels, then put back the logical size of video.c.
	SDL_GetRenderLogicalPresentation(r, &ow, &oh, &mode);
	SDL_SetRenderLogicalPresentation(r, 0, 0, SDL_LOGICAL_PRESENTATION_DISABLED);
	{
		int pw = 0, ph = 0;
		SDL_GetCurrentRenderOutputSize(r, &pw, &ph);
		if (pw <= 0 || ph <= 0)
			pw = w, ph = h;
		letterbox(pw, ph, w, h, &x, &y, &dw, &dh);
	}
	dst.x = (float) x;
	dst.y = (float) y;
	dst.w = (float) dw;
	dst.h = (float) dh;
	SDL_SetRenderDrawColor(r, 0, 0, 0, 255);
	SDL_RenderClear(r);
	SDL_RenderTexture(r, Sdl_tex, NULL, &dst);
	SDL_RenderPresent(r);
	SDL_SetRenderLogicalPresentation(r, ow, oh, mode);
}

void plat_present_xrgb(const uint32_t *pixels, int w, int h, int pitch)
{
	SDL_Window *win;

	if (!pixels || w <= 0 || h <= 0)
		return;
	if (SDL_GL_GetCurrentContext() && (win = SDL_GL_GetCurrentWindow()) != NULL) {
		if (load_gl())
			present_gl(win, pixels, w, h, pitch);
		return;
	}
	{
		int n = 0, i;
		SDL_Window **list = SDL_GetWindows(&n);
		for (i = 0; list && i < n; i++) {
			SDL_Renderer *r = SDL_GetRenderer(list[i]);
			if (r) {
				present_renderer(r, pixels, w, h, pitch);
				break;
			}
		}
		SDL_free(list);
	}
}
