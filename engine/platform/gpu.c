/*
 * ASCENT platform layer: the GPU renderer of the 3D view and the
 * presentation of the frame with OpenGL 3.3 core.
 *
 * New file. It has no original: the DOS game drew the 3D view with the
 * software texture mapper into the VGA memory.
 *
 * The 3D view is drawn into an image of one of two forms (platform.h):
 *
 * 8-bit: an R8 texture of palette indexes, so that the colors are those of
 * the software renderer: the fragment shader reads the palette index of
 * the bitmap, applies the fade table row of the light and writes the
 * index. A darkening polygon (Gr_scanline_darkening_level) reads the index
 * under it from a copy of the image and writes fade_table[level][index].
 *
 * RGB: an RGBA8 texture. The textures are RGBA (expanded bitmaps or the
 * upscaled set), with mipmaps and filters. The light is the fade table
 * for each palette color: for a texel color c, p is the nearest palette
 * color (Near_tex, a table of the 64x64x64 colors of 6 bits), and the lit
 * color is palette[fade_table[row][p]] + (c - palette[p]) * k[row], where
 * k[row] is the average scale of the row. A texel of an original bitmap
 * has a palette color, so it gets the color of the fade table exactly; a
 * filtered or upscaled texel gets the light of the nearest palette color
 * (the fade table of PALETTE.256 is not a plain scale: the dark rows are
 * warm, red falls slower than blue, and the scale depends on the hue).
 * The light row is not rounded to an integer: the colors of the two rows
 * are mixed, so the RGB form has no light steps.
 *
 * The palette is applied only when the frame is shown. For the RGB form a
 * curve for each channel (Plut_tex) maps the palette of the view (the
 * palette of the bitmaps, plat_gpu_set_view_palette) to the palette of the
 * frame (the VGA registers, pal6). The palette effects of the game change
 * each channel by itself (gr_palette_step_up adds a value, the fades scale
 * the values, the gamma adds a value), so the curve gives them exactly.
 *
 * gpu_present shows the 8-bit frame in the window with the palette. In the
 * view rectangle (plat_gpu_set_view_dest), a frame pixel of color 255 (the
 * transparent color, the value that the 3d glue puts in the view canvas)
 * shows the GPU image. The composed image can go through a screen shader
 * (Subtle or VGA, see the comment at Post_*).
 *
 * The OpenGL functions are loaded with SDL_GL_GetProcAddress. No other
 * library is used.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>

#include "platform.h"
#include "plat_video.h"

#ifndef GL_TEXTURE_MAX_ANISOTROPY
#define GL_TEXTURE_MAX_ANISOTROPY		0x84FE
#endif
#ifndef GL_MAX_TEXTURE_MAX_ANISOTROPY
#define GL_MAX_TEXTURE_MAX_ANISOTROPY	0x84FF
#endif

// ---------------------------------------------------------------------
// OpenGL functions

#define GL_FUNCS(X) \
	X(const GLubyte *, GetString, (GLenum)) \
	X(const GLubyte *, GetStringi, (GLenum, GLuint)) \
	X(void, GetIntegerv, (GLenum, GLint *)) \
	X(void, GetFloatv, (GLenum, GLfloat *)) \
	X(GLenum, GetError, (void)) \
	X(void, Viewport, (GLint, GLint, GLsizei, GLsizei)) \
	X(void, ClearColor, (GLfloat, GLfloat, GLfloat, GLfloat)) \
	X(void, ClearDepth, (GLdouble)) \
	X(void, Clear, (GLbitfield)) \
	X(void, Enable, (GLenum)) \
	X(void, Disable, (GLenum)) \
	X(void, BlendFuncSeparate, (GLenum, GLenum, GLenum, GLenum)) \
	X(void, DepthFunc, (GLenum)) \
	X(void, DepthMask, (GLboolean)) \
	X(void, PixelStorei, (GLenum, GLint)) \
	X(void, GenTextures, (GLsizei, GLuint *)) \
	X(void, DeleteTextures, (GLsizei, const GLuint *)) \
	X(void, BindTexture, (GLenum, GLuint)) \
	X(void, TexImage2D, (GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum, GLenum, const void *)) \
	X(void, TexImage3D, (GLenum, GLint, GLint, GLsizei, GLsizei, GLsizei, GLint, GLenum, GLenum, const void *)) \
	X(void, Uniform1fv, (GLint, GLsizei, const GLfloat *)) \
	X(void, TexSubImage2D, (GLenum, GLint, GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, const void *)) \
	X(void, TexParameteri, (GLenum, GLenum, GLint)) \
	X(void, GenerateMipmap, (GLenum)) \
	X(void, CopyTexSubImage2D, (GLenum, GLint, GLint, GLint, GLint, GLint, GLsizei, GLsizei)) \
	X(void, ReadPixels, (GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, void *)) \
	X(void, DrawArrays, (GLenum, GLint, GLsizei)) \
	X(void, ActiveTexture, (GLenum)) \
	X(GLuint, CreateShader, (GLenum)) \
	X(void, ShaderSource, (GLuint, GLsizei, const GLchar *const *, const GLint *)) \
	X(void, CompileShader, (GLuint)) \
	X(void, GetShaderiv, (GLuint, GLenum, GLint *)) \
	X(void, GetShaderInfoLog, (GLuint, GLsizei, GLsizei *, GLchar *)) \
	X(void, DeleteShader, (GLuint)) \
	X(GLuint, CreateProgram, (void)) \
	X(void, AttachShader, (GLuint, GLuint)) \
	X(void, LinkProgram, (GLuint)) \
	X(void, GetProgramiv, (GLuint, GLenum, GLint *)) \
	X(void, GetProgramInfoLog, (GLuint, GLsizei, GLsizei *, GLchar *)) \
	X(void, DeleteProgram, (GLuint)) \
	X(void, UseProgram, (GLuint)) \
	X(GLint, GetUniformLocation, (GLuint, const GLchar *)) \
	X(void, Uniform1i, (GLint, GLint)) \
	X(void, Uniform1f, (GLint, GLfloat)) \
	X(void, Uniform2f, (GLint, GLfloat, GLfloat)) \
	X(void, Uniform4f, (GLint, GLfloat, GLfloat, GLfloat, GLfloat)) \
	X(void, Uniform1iv, (GLint, GLsizei, const GLint *)) \
	X(void, Uniform4fv, (GLint, GLsizei, const GLfloat *)) \
	X(void, GenVertexArrays, (GLsizei, GLuint *)) \
	X(void, BindVertexArray, (GLuint)) \
	X(void, DeleteVertexArrays, (GLsizei, const GLuint *)) \
	X(void, GenBuffers, (GLsizei, GLuint *)) \
	X(void, BindBuffer, (GLenum, GLuint)) \
	X(void, BufferData, (GLenum, GLsizeiptr, const void *, GLenum)) \
	X(void, DeleteBuffers, (GLsizei, const GLuint *)) \
	X(void, VertexAttribPointer, (GLuint, GLint, GLenum, GLboolean, GLsizei, const void *)) \
	X(void, EnableVertexAttribArray, (GLuint)) \
	X(void, GenFramebuffers, (GLsizei, GLuint *)) \
	X(void, BindFramebuffer, (GLenum, GLuint)) \
	X(void, FramebufferTexture2D, (GLenum, GLenum, GLenum, GLuint, GLint)) \
	X(void, FramebufferRenderbuffer, (GLenum, GLenum, GLenum, GLuint)) \
	X(GLenum, CheckFramebufferStatus, (GLenum)) \
	X(void, DeleteFramebuffers, (GLsizei, const GLuint *)) \
	X(void, GenRenderbuffers, (GLsizei, GLuint *)) \
	X(void, BindRenderbuffer, (GLenum, GLuint)) \
	X(void, RenderbufferStorage, (GLenum, GLenum, GLsizei, GLsizei)) \
	X(void, DeleteRenderbuffers, (GLsizei, const GLuint *)) \
	X(void, GenSamplers, (GLsizei, GLuint *)) \
	X(void, BindSampler, (GLuint, GLuint)) \
	X(void, SamplerParameteri, (GLuint, GLenum, GLint)) \
	X(void, SamplerParameterf, (GLuint, GLenum, GLfloat)) \
	X(void, DeleteSamplers, (GLsizei, const GLuint *)) \
	X(void, GenQueries, (GLsizei, GLuint *)) \
	X(void, DeleteQueries, (GLsizei, const GLuint *)) \
	X(void, BeginQuery, (GLenum, GLuint)) \
	X(void, EndQuery, (GLenum)) \
	X(void, GetQueryObjectui64v, (GLuint, GLenum, GLuint64 *))

#define X(ret, name, args) static ret (APIENTRY *qgl##name) args;
GL_FUNCS(X)
#undef X

static int load_gl(void)
{
#define X(ret, name, args) \
	qgl##name = (ret (APIENTRY *) args) SDL_GL_GetProcAddress("gl" #name); \
	if (!qgl##name) { plat_log_error("GPU: no gl%s\n", #name); return -1; }
	GL_FUNCS(X)
#undef X
	return 0;
}

// ---------------------------------------------------------------------
// Shaders

// The 3D view. The position is the canvas position of the 3d library times
// the depth (homogeneous, see plat_gpu_vertex), so that the GPU clips the
// polygons and interpolates u and v with perspective. The light is
// interpolated with perspective too, so the light of a point of a face does
// not depend on the view (see the comment at gpu_tmap in gpu3d.c). v_lq: the
// corner lights of a quad (bilinear at v_st), or -1.
static const char *Scene_vs =
	"#version 330 core\n"
	"layout(location = 0) in vec3 a_pos;\n"
	"layout(location = 1) in vec3 a_uvl;\n"
	"layout(location = 2) in vec2 a_st;\n"
	"layout(location = 3) in vec4 a_lq;\n"
	"uniform vec2 u_canvas;\n"
	"uniform float u_znear;\n"
	"out vec2 v_uv;\n"
	"out float v_l;\n"
	"out vec2 v_st;\n"
	"flat out vec4 v_lq;\n"
	"void main() {\n"
	"	float z = a_pos.z;\n"
	"	gl_Position = vec4(a_pos.x / u_canvas.x * 2.0 - z, z - a_pos.y / u_canvas.y * 2.0, z - 2.0 * u_znear, z);\n"
	"	v_uv = a_uvl.xy;\n"
	"	v_l = a_uvl.z;\n"
	"	v_st = a_st;\n"
	"	v_lq = a_lq;\n"
	"}\n";

// The light of the RGB form (see the comment at the top). u_near: the
// nearest palette color of each 6-bit color. u_fade: the fade table.
// u_vpal: the palette of the view. u_rowk: the average scale of each row.
// The row is not rounded: the result is mixed between two rows.
#define LIGHT_GLSL \
	"uniform usampler3D u_near;\n" \
	"uniform sampler2D u_fade;\n" \
	"uniform sampler2D u_vpal;\n" \
	"uniform int u_levels;\n" \
	"uniform float u_rowk[64];\n" \
	"vec3 vpal(int c) { return texelFetch(u_vpal, ivec2(c, 0), 0).rgb; }\n" \
	"int fade(int c, int row) { return int(texelFetch(u_fade, ivec2(c, row), 0).r * 255.0 + 0.5); }\n" \
	"vec3 light(vec3 c, float row) {\n" \
	"	ivec3 q = clamp(ivec3(c * 255.0 + 0.5), ivec3(0), ivec3(255)) >> 2;\n" \
	"	int i = int(texelFetch(u_near, q, 0).r);\n" \
	"	int r0 = clamp(int(floor(row)), 0, u_levels - 1);\n" \
	"	int r1 = min(r0 + 1, u_levels - 1);\n" \
	"	float t = clamp(row - float(r0), 0.0, 1.0);\n" \
	"	vec3 f = mix(vpal(fade(i, r0)), vpal(fade(i, r1)), t);\n" \
	"	return max(f + (c - vpal(i)) * mix(u_rowk[r0], u_rowk[r1], t), 0.0);\n" \
	"}\n"

// u_mode 0: bitmap, 1: flat color, 2: darken. The light row of the 8-bit
// form is that of the software mapper: l * 32 clamped to 31.5, the integer
// part. The RGB form uses l * 32 - 0.5 (the same average) without the
// rounding.
static const char *Scene_fs =
	"#version 330 core\n"
	"uniform int u_mode;\n"
	"uniform int u_lit;\n"
	"uniform int u_transparent;\n"
	"uniform int u_color;\n"
	"uniform int u_rgb;\n"
	"uniform sampler2D u_tex;\n"
	"uniform sampler2D u_dest;\n"
	"uniform int u_infra;\n"
	"uniform int u_heat;\n"
	"uniform int u_fx;\n"
	"uniform int u_fxrow;\n"
	"uniform int u_fxdy;\n"
	"uniform int u_dim;\n"
	"uniform int u_tint;\n"
	LIGHT_GLSL
	// The improved infravision (plat_gpu_set_infravision): c is the lit
	// color, b the color without light. A cold polygon is a dark blue of
	// the lit brightness. A warm or a hot polygon shows in the dark too.
	// Warm is a red ramp of the brightness of the texel; grey texels (metal:
	// weapons, armor) get less of it. Hot goes from orange to yellow white
	// on the bright texels (the flame, not the wall around a lamp). Glow (lava)
	// colors the whole surface from dark red to yellow by the brightest
	// component of the texel: the red of lava is dark in luminance.
	"float lum(vec3 v) { return dot(v, vec3(0.299, 0.587, 0.114)); }\n"
	"vec3 infra(vec3 c, vec3 b) {\n"
	"	vec3 cold = vec3(0.28, 0.42, 0.75) * (lum(c) * 0.55);\n"
	"	if (u_heat == 1) {\n"
	"		float s = max(b.r, max(b.g, b.b)) - min(b.r, min(b.g, b.b));\n"
	"		vec3 w = mix(vec3(0.30, 0.02, 0.0), vec3(1.0, 0.42, 0.12), clamp(0.25 + lum(b) * 1.2, 0.0, 1.0));\n"
	"		return mix(cold, w, clamp(0.15 + s * 3.0, 0.0, 1.0));\n"
	"	}\n"
	"	if (u_heat == 2) {\n"
	"		vec3 h = mix(vec3(0.95, 0.30, 0.02), vec3(1.0, 0.95, 0.70), clamp(lum(b) * 1.3 - 0.3, 0.0, 1.0));\n"
	"		return mix(cold, h, smoothstep(0.35, 0.65, lum(b)));\n"
	"	}\n"
	"	if (u_heat == 3) {\n"
	"		float v = max(b.r, max(b.g, b.b));\n"
	"		vec3 h = mix(vec3(0.45, 0.06, 0.0), vec3(1.0, 0.50, 0.08), clamp(v * 1.6, 0.0, 1.0));\n"
	"		return mix(h, vec3(1.0, 0.95, 0.70), clamp(v * 2.0 - 1.0, 0.0, 1.0));\n"
	"	}\n"
	"	return cold;\n"
	"}\n"
	// The dark view of the conversation window (texmap/tmapfx.c): u_dim is
	// 1 to 15 for a lit texture. A texel with the brightness step s <=
	// u_dim (s: the low 4 bits of the palette index, 0 the brightest) gets
	// the color (fade[max(row - 5, 0)][c] & 15) | 8, the dark half of
	// palette row 0. The RGB form takes the steps of the nearest palette
	// colors of the texel and of its light five rows brighter, and keeps the
	// detail of a filtered or upscaled texel at the scale of the new color.
	"int nearidx(vec3 c) {\n"
	"	ivec3 q = clamp(ivec3(c * 255.0 + 0.5), ivec3(0), ivec3(255)) >> 2;\n"
	"	return int(texelFetch(u_near, q, 0).r);\n"
	"}\n"
	"int dimidx(int c, int row) { return (fade(c, max(row - 5, 0)) & 15) | 8; }\n"
	"vec3 dimrgb(vec3 t, float row) {\n"
	"	vec3 lc = light(t, max(row - 5.0, 0.0));\n"
	"	int i = nearidx(lc);\n"
	"	vec3 o = vpal((i & 15) | 8), p = vpal(i);\n"
	"	float k = clamp(lum(o) / max(lum(p), 1.0 / 64.0), 0.0, 1.0);\n"
	"	return clamp(o + (lc - p) * k, 0.0, 1.0);\n"
	"}\n"
	// The tint of Detect Magic (texmap/tmapfx.c, u_tint 1 for a lit
	// texture): a texel c gets (c & 15) | 0x20, the blue ramp at its
	// brightness step, before the light. The RGB form takes the step of the
	// nearest palette color and keeps the detail of the texel at the scale
	// of the new color.
	"vec3 tintrgb(vec3 t) {\n"
	"	int i = nearidx(t);\n"
	"	vec3 o = vpal((i & 15) | 0x20), p = vpal(i);\n"
	"	float k = clamp(lum(o) / max(lum(p), 1.0 / 64.0), 0.0, 1.0);\n"
	"	return clamp(o + (t - p) * k, 0.0, 1.0);\n"
	"}\n"
	"int fxcolor(int k) {\n"
	"	if (k == 1) return 0xCF; if (k == 2) return 0xBF; if (k == 3) return 0xBE; if (k == 4) return 0x6F;\n"
	"	if (k == 5) return 0x6E; if (k == 6) return 0x6D; if (k == 7) return 0x4F; return 0x4E;\n"
	"}\n"
	"in vec2 v_uv;\n"
	"in float v_l;\n"
	"in vec2 v_st;\n"
	"flat in vec4 v_lq;\n"
	"out vec4 o_color;\n"
	"int idx(vec4 c) { return int(c.r * 255.0 + 0.5); }\n"
	// The effect colors of a merged wall texture (texmap/tmapfx.c): the
	// color comes from the image under the face (behind) and one canvas
	// row lower (below). k: 1 to 8 for 0xCF, 0xBF, 0xBE, 0x6F, 0x6E, 0x6D,
	// 0x4F, 0x4E. The RGB form finds the brightness step (the low 4 bits of
	// a palette index) of a color by the nearest palette color.
	"int fxcode(int c) {\n"
	"	if (c == 0xCF) return 1; if (c == 0xBF) return 2; if (c == 0xBE) return 3; if (c == 0x6F) return 4;\n"
	"	if (c == 0x6E) return 5; if (c == 0x6D) return 6; if (c == 0x4F) return 7; if (c == 0x4E) return 8;\n"
	"	return 0;\n"
	"}\n"
	"int fxrow() { return (u_fxrow - 1) & 31; }\n"
	"ivec2 fxbelow() { ivec2 p = ivec2(gl_FragCoord.xy); return ivec2(p.x, max(p.y - u_fxdy, 0)); }\n"
	"int fxramp(int k) { return (k == 2 || k == 5) ? 0x50 : ((k == 3 || k == 6) ? 0xA0 : 0x20); }\n"
	"int fxstep(vec3 c) {\n"
	"	ivec3 q = clamp(ivec3(c * 255.0 + 0.5), ivec3(0), ivec3(255)) >> 2;\n"
	"	return int(texelFetch(u_near, q, 0).r) & 15;\n"
	"}\n"
	"vec3 fxrgb(int k) {\n"
	"	vec3 b = texelFetch(u_dest, ivec2(gl_FragCoord.xy), 0).rgb;\n"
	"	vec3 d = texelFetch(u_dest, fxbelow(), 0).rgb;\n"
	"	float r = float(fxrow());\n"
	"	if (k == 1) return light(light(b, r), r);\n"
	"	if (k == 8) return d;\n"
	"	return vpal(fxramp(k) | fxstep(k <= 4 ? b : d));\n"
	"}\n"
	"int fxidx(int k) {\n"
	"	int b = idx(texelFetch(u_dest, ivec2(gl_FragCoord.xy), 0));\n"
	"	int d = idx(texelFetch(u_dest, fxbelow(), 0));\n"
	"	if (k == 1) {\n"
	"		int f = fade(b, fxrow());\n"
	"		int kk = fxcode(f);\n"
	"		if (kk < 2) return fade(f, fxrow());\n"
	"		k = kk;\n"
	"	}\n"
	"	if (k == 8) return d;\n"
	"	return fxramp(k) | ((k <= 4 ? b : d) & 15);\n"
	"}\n"
	"void main() {\n"
	"	float l = v_lq.x >= 0.0 ? mix(mix(v_lq.x, v_lq.y, v_st.x), mix(v_lq.w, v_lq.z, v_st.x), v_st.y) : v_l;\n"
	"	if (u_rgb != 0) {\n"
	"		vec3 c, b;\n"
		"		if (u_mode == 0) {\n"
	"			if (u_fx != 0) {\n"
	"				ivec2 ts = textureSize(u_tex, 0);\n"
	"				ivec2 tc = ivec2(floor(v_uv * vec2(ts)));\n"
	"				tc = ((tc % ts) + ts) % ts;\n"
	"				int a = int(texelFetch(u_tex, tc, 0).a * 255.0 + 0.5);\n"
	"				if (a > 0 && a < 255 && (255 - a) % 8 == 0 && (255 - a) / 8 <= 8) {\n"
	"					int fc = fxcolor((255 - a) / 8);\n"
	"					if (u_dim != 0 && (fc & 15) <= u_dim)\n"
	"						o_color = vec4(vpal(dimidx(fc, int(clamp(l * 32.0, 0.0, 31.5)))), 1.0);\n"
	"					else\n"
	"						o_color = vec4(fxrgb((255 - a) / 8), 1.0);\n"
	"					return;\n"
	"				}\n"
	"			}\n"
	"			vec4 t = texture(u_tex, v_uv);\n"
	"			if (u_transparent != 0 && t.a < 0.5) discard;\n"
	"			if (u_dim != 0 && (nearidx(t.rgb) & 15) <= u_dim) {\n"
	"				o_color = vec4(dimrgb(t.rgb, clamp(l * 32.0 - 0.5, 0.0, 31.0)), 1.0);\n"
	"				return;\n"
	"			}\n"
	"			c = b = t.rgb;\n"
	"			if (u_tint != 0) c = b = tintrgb(c);\n"
	"			if (u_lit != 0)\n"
	"				c = light(c, clamp(l * 32.0 - 0.5, 0.0, 31.0));\n"
	"		} else if (u_mode == 1) {\n"
	"			c = b = vpal(u_color);\n"
	"		} else {\n"
	"			c = b = light(texelFetch(u_dest, ivec2(gl_FragCoord.xy), 0).rgb, float(u_color));\n"
	"		}\n"
	"		if (u_infra != 0 && u_mode != 2) c = infra(c, b);\n"
	"		o_color = vec4(c, 1.0);\n"
	"		return;\n"
	"	}\n"
	"	int c;\n"
	"	if (u_mode == 0) {\n"
	"		c = idx(texture(u_tex, v_uv));\n"
	"		if (u_transparent != 0 && c == 255) discard;\n"
	"		if (u_dim != 0 && c != 255 && (c & 15) <= u_dim) {\n"
	"			o_color = vec4(float(dimidx(c, int(clamp(l * 32.0, 0.0, 31.5)))) / 255.0, 0.0, 0.0, 1.0);\n"
	"			return;\n"
	"		}\n"
	"		if (u_fx != 0 && fxcode(c) != 0) {\n"
	"			o_color = vec4(float(fxidx(fxcode(c))) / 255.0, 0.0, 0.0, 1.0);\n"
	"			return;\n"
	"		}\n"
	"		if (u_lit != 0) {\n"
	"			int row = int(clamp(l * 32.0, 0.0, 31.5));\n"
	"			if (u_tint != 0 && c != 255 && fxcode(c) == 0) c = (c & 15) | 0x20;\n"
	"			c = fade(c, row);\n"
	"		}\n"
	"	} else if (u_mode == 1) {\n"
	"		c = u_color;\n"
	"	} else {\n"
	"		int d = idx(texelFetch(u_dest, ivec2(gl_FragCoord.xy), 0));\n"
	"		c = fade(d, u_color);\n"
	"	}\n"
	"	o_color = vec4(float(c) / 255.0, 0.0, 0.0, 1.0);\n"
	"}\n";

// One triangle that covers the viewport.
static const char *Full_vs =
	"#version 330 core\n"
	"void main() {\n"
	"	vec2 p = vec2(float((gl_VertexID & 1) * 4 - 1), float((gl_VertexID & 2) * 2 - 1));\n"
	"	gl_Position = vec4(p, 0.0, 1.0);\n"
	"}\n";

// The frame. u_dst: the viewport (x, y, w, h in pixels of the target, y from
// the bottom). u_vrect: the view rectangle in frame pixels. A frame pixel of
// color 255 in that rectangle shows the GPU image (u_view). u_vrgb: the
// image has the RGB form (then u_plut maps it to the palette of the frame).
// u_drect, u_dlevel: the darkened rectangles (plat_gpu_view_darken).
//
// ASCENT: the scale to the output (u_filter 0) is a Gaussian filter. Each
// source pixel is a square, and the squares are blurred by a Gaussian: the
// weight of a pixel is the part of the Gaussian over its square (the
// difference of two values of the normal distribution function). The
// color is flat inside a pixel and changes at its edges over about 2.5
// sigma, at the exact place of the edge, so all pixels keep the same width
// at a scale that is not an integer (the nearest pixel makes some columns
// one output pixel wider than others). u_sigma: the sigma of the frame in
// frame pixels, 3 by 3 pixels. The view has its own filter in view pixels
// (u_vsigma, u_vrad pixels on each side): the view is rendered at a higher
// resolution than the frame, and the frame filter would blur it. Inside
// the view (all the frame pixels under the filter show the view) only the
// view filter applies; the upscaled interface pictures (u_hd) are read at
// the output pixel, so inside a picture the filter does not blur it.
// u_filter 1: the nearest pixel, 2: bilinear (for comparisons, see
// ASCENT_SCALE_FILTER).
static const char *Comp_fs =
	"#version 330 core\n"
	"uniform sampler2D u_frame;\n"
	"uniform sampler2D u_pal;\n"
	"uniform sampler2D u_view;\n"
	"uniform sampler2D u_plut;\n"
	LIGHT_GLSL
	"uniform vec4 u_dst;\n"
	"uniform vec2 u_fsize;\n"
	"uniform vec4 u_vrect;\n"
	"uniform vec2 u_vsize;\n"
	"uniform int u_key;\n"
	"uniform int u_vrgb;\n"
	"uniform int u_ndark;\n"
	"uniform vec4 u_drect[16];\n"
	"uniform int u_dlevel[16];\n"
	"uniform int u_filter;\n"
	"uniform float u_sigma;\n"
	"uniform vec2 u_vsigma;\n"
	"uniform int u_vrad;\n"
	"out vec4 o_color;\n"
	"int idx(vec4 c) { return int(c.r * 255.0 + 0.5); }\n"
	"vec3 pal(int c) { return texelFetch(u_pal, ivec2(c, 0), 0).rgb; }\n"
	// the normal distribution function (the tanh approximation, error
	// below 0.0002)
	"float cdf(float x) { return 0.5 + 0.5 * tanh(clamp(0.7978846 * (x + 0.044715 * x * x * x), -10.0, 10.0)); }\n"
	// the weight of the pixel at offset i from the pixel of the point, f:
	// the place of the point in its pixel (0 to 1)
	"float gw(float i, float f, float s) { return cdf((i + 1.0 - f) / s) - cdf((i - f) / s); }\n"
	"vec3 plut(vec3 c) {\n"
	"	vec3 x = (clamp(c, 0.0, 1.0) * 255.0 + 0.5) / 256.0;\n"
	"	return vec3(texture(u_plut, vec2(x.r, 0.5)).r, texture(u_plut, vec2(x.g, 0.5)).g,\n"
	"		texture(u_plut, vec2(x.b, 0.5)).b);\n"
	"}\n"
	"int dark(vec2 fp) {\n"
	"	for (int i = 0; i < u_ndark; i++)\n"
	"		if (fp.x >= u_drect[i].x && fp.y >= u_drect[i].y && fp.x < u_drect[i].z && fp.y < u_drect[i].w)\n"
	"			return u_dlevel[i];\n"
	"	return -1;\n"
	"}\n"
	"bool inview(vec2 fp) {\n"
	"	return fp.x >= u_vrect.x && fp.y >= u_vrect.y && fp.x < u_vrect.x + u_vrect.z && fp.y < u_vrect.y + u_vrect.w;\n"
	"}\n"
	// one texel of the view (y from the top), darkened
	"vec3 vtexel(ivec2 iv, int dl) {\n"
	"	iv = clamp(iv, ivec2(0), ivec2(u_vsize) - 1);\n"
	"	ivec2 t = ivec2(iv.x, int(u_vsize.y) - 1 - iv.y);\n"
	"	if (u_vrgb == 0) {\n"
	"		int c = idx(texelFetch(u_view, t, 0));\n"
	"		if (dl >= 0) c = fade(c, dl);\n"
	"		return pal(c);\n"
	"	}\n"
	"	vec3 c = texelFetch(u_view, t, 0).rgb;\n"
	"	if (dl >= 0) c = c * u_rowk[dl];\n"
	"	return plut(c);\n"
	"}\n"
	// ASCENT: the same for the Gaussian filter: the RGB form gives the
	// color before the darkening and the palette curve, which view_at
	// applies once to the sum (3 lookups less for each pixel under the
	// filter)
	"vec3 vtexel_raw(ivec2 iv, int dl) {\n"
	"	iv = clamp(iv, ivec2(0), ivec2(u_vsize) - 1);\n"
	"	ivec2 t = ivec2(iv.x, int(u_vsize.y) - 1 - iv.y);\n"
	"	if (u_vrgb == 0) {\n"
	"		int c = idx(texelFetch(u_view, t, 0));\n"
	"		if (dl >= 0) c = fade(c, dl);\n"
	"		return pal(c);\n"
	"	}\n"
	"	return texelFetch(u_view, t, 0).rgb;\n"
	"}\n"
	// the view at the frame position fp
	"vec3 view_at(vec2 fp) {\n"
	"	vec2 vp = (fp - u_vrect.xy) / u_vrect.zw * u_vsize;\n"
	"	int dl = dark(fp);\n"
	"	if (u_filter == 1)\n"
	"		return vtexel(ivec2(floor(vp)), dl);\n"
	"	if (u_filter == 2) {\n"
	"		vec2 s = vp - 0.5;\n"
	"		ivec2 b = ivec2(floor(s));\n"
	"		vec2 f = fract(s);\n"
	"		vec3 a = mix(vtexel(b, dl), vtexel(b + ivec2(1, 0), dl), f.x);\n"
	"		vec3 c = mix(vtexel(b + ivec2(0, 1), dl), vtexel(b + ivec2(1, 1), dl), f.x);\n"
	"		return mix(a, c, f.y);\n"
	"	}\n"
	"	ivec2 b = ivec2(floor(vp));\n"
	"	vec2 f = vp - vec2(b);\n"
	"	float wx[7];\n"
	"	for (int i = -u_vrad; i <= u_vrad; i++)\n"
	"		wx[i + 3] = gw(float(i), f.x, u_vsigma.x);\n"
	"	vec3 sum = vec3(0.0);\n"
	"	float wsum = 0.0;\n"
	"	for (int j = -u_vrad; j <= u_vrad; j++) {\n"
	"		float wy = gw(float(j), f.y, u_vsigma.y);\n"
	"		if (wy < 0.001) continue;\n"
	"		for (int i = -u_vrad; i <= u_vrad; i++) {\n"
	"			float w = wx[i + 3] * wy;\n"
	"			if (w < 0.001) continue;\n"
	"			sum += w * vtexel_raw(b + ivec2(i, j), dl);\n"
	"			wsum += w;\n"
	"		}\n"
	"	}\n"
	"	vec3 c = sum / max(wsum, 1e-6);\n"
	"	if (u_vrgb == 0)\n"
	"		return c;\n"
	"	if (dl >= 0) c = c * u_rowk[dl];\n"
	"	return plut(c);\n"
	"}\n"
	"bool keyed(ivec2 ip) {\n"
	"	ip = clamp(ip, ivec2(0), ivec2(u_fsize) - 1);\n"
	"	return u_key != 0 && idx(texelFetch(u_frame, ip, 0)) == 255 && inview(vec2(ip) + 0.5);\n"
	"}\n"
	// ASCENT: the upscaled interface pictures (see the comment at Ui_*).
	// u_ref: for each frame pixel, r the expected index, g 1 if a picture
	// with a loaded file covers it, b the index of the picture before a
	// darkening. A frame pixel with the expected index shows the upscaled
	// layer (u_hd, the size of the viewport) unless 5 or more of its 8
	// neighbors are covered pixels that differ (a lone pixel in an icon or
	// a portrait that has the color of the picture by chance). A darkened
	// pixel gets the luminance ratio of the two indexes. u_uplut is the
	// curve from the palette of the pictures (u_upal) to the frame palette.
	"uniform int u_ui;\n"
	"uniform sampler2D u_ref;\n"
	"uniform sampler2D u_hd;\n"
	"uniform sampler2D u_uplut;\n"
	"uniform sampler2D u_upal;\n"
	"bool ui_diff(ivec2 q) {\n"
	"	q = clamp(q, ivec2(0), ivec2(u_fsize) - 1);\n"
	"	vec4 r = texelFetch(u_ref, q, 0);\n"
	"	return r.g > 0.5 && idx(texelFetch(u_frame, q, 0)) != idx(r);\n"
	"}\n"
	"float lum(vec3 c) { return dot(c, vec3(0.299, 0.587, 0.114)); }\n"
	"bool ui_at(ivec2 ip, int c, out vec3 o) {\n"
	"	vec4 r = texelFetch(u_ref, ip, 0);\n"
	"	if (r.g < 0.5 || c != idx(r)) return false;\n"
	"	int n = 0;\n"
	"	for (int dy = -1; dy <= 1; dy++)\n"
	"		for (int dx = -1; dx <= 1; dx++)\n"
	"			if ((dx != 0 || dy != 0) && ui_diff(ip + ivec2(dx, dy))) n++;\n"
	"	if (n >= 5) return false;\n"
	"	vec3 h = texelFetch(u_hd, ivec2(gl_FragCoord.xy - u_dst.xy), 0).rgb;\n"
	"	int b = int(r.b * 255.0 + 0.5);\n"
	"	if (b != c)\n"
	"		h *= lum(texelFetch(u_upal, ivec2(c, 0), 0).rgb) / max(lum(texelFetch(u_upal, ivec2(b, 0), 0).rgb), 1.0 / 255.0);\n"
	"	vec3 x = (clamp(h, 0.0, 1.0) * 255.0 + 0.5) / 256.0;\n"
	"	o = vec3(texture(u_uplut, vec2(x.r, 0.5)).r, texture(u_uplut, vec2(x.g, 0.5)).g,\n"
	"		texture(u_uplut, vec2(x.b, 0.5)).b);\n"
	"	return true;\n"
	"}\n"
	"vec3 ftexel(ivec2 ip, vec2 fp) {\n"
	"	ip = clamp(ip, ivec2(0), ivec2(u_fsize) - 1);\n"
	"	int c = idx(texelFetch(u_frame, ip, 0));\n"
	"	if (u_key != 0 && c == 255 && inview(vec2(ip) + 0.5))\n"
	"		return view_at(clamp(fp, vec2(ip), vec2(ip) + 0.999));\n"
	"	vec3 o;\n"
	"	if (u_ui != 0 && ui_at(ip, c, o))\n"
	"		return o;\n"
	"	return pal(c);\n"
	"}\n"
	"void main() {\n"
	"	vec2 p = (gl_FragCoord.xy - u_dst.xy) / u_dst.zw;\n"
	"	vec2 fp = vec2(p.x, 1.0 - p.y) * u_fsize;\n"
	"	if (u_filter == 1) {\n"
	"		ivec2 ip = clamp(ivec2(floor(fp)), ivec2(0), ivec2(u_fsize) - 1);\n"
	"		o_color = vec4(ftexel(ip, fp), 1.0);\n"
	"		return;\n"
	"	}\n"
	"	if (u_filter == 2) {\n"
	"		vec2 s = fp - 0.5;\n"
	"		ivec2 b = ivec2(floor(s));\n"
	"		if (keyed(b) && keyed(b + ivec2(1, 0)) && keyed(b + ivec2(0, 1)) && keyed(b + ivec2(1, 1))) {\n"
	"			o_color = vec4(view_at(fp), 1.0);\n"
	"			return;\n"
	"		}\n"
	"		vec2 f = fract(s);\n"
	"		vec3 a = mix(ftexel(b, fp), ftexel(b + ivec2(1, 0), fp), f.x);\n"
	"		vec3 c = mix(ftexel(b + ivec2(0, 1), fp), ftexel(b + ivec2(1, 1), fp), f.x);\n"
	"		o_color = vec4(mix(a, c, f.y), 1.0);\n"
	"		return;\n"
	"	}\n"
	"	ivec2 b = ivec2(floor(fp));\n"
	"	vec2 f = fp - vec2(b);\n"
	"	vec3 wx = vec3(gw(-1.0, f.x, u_sigma), gw(0.0, f.x, u_sigma), gw(1.0, f.x, u_sigma));\n"
	"	vec3 wy = vec3(gw(-1.0, f.y, u_sigma), gw(0.0, f.y, u_sigma), gw(1.0, f.y, u_sigma));\n"
	"	bool inside = u_key != 0;\n"
	"	for (int j = 0; j < 3 && inside; j++)\n"
	"		for (int i = 0; i < 3; i++)\n"
	"			if (wx[i] * wy[j] >= 0.001 && !keyed(b + ivec2(i - 1, j - 1))) {\n"
	"				inside = false;\n"
	"				break;\n"
	"			}\n"
	"	if (inside) {\n"
	"		o_color = vec4(view_at(fp), 1.0);\n"
	"		return;\n"
	"	}\n"
	"	vec3 sum = vec3(0.0);\n"
	"	float wsum = 0.0;\n"
	"	for (int j = 0; j < 3; j++)\n"
	"		for (int i = 0; i < 3; i++) {\n"
	"			float w = wx[i] * wy[j];\n"
	"			if (w < 0.001) continue;\n"
	"			sum += w * ftexel(b + ivec2(i - 1, j - 1), fp);\n"
	"			wsum += w;\n"
	"		}\n"
	"	o_color = vec4(sum / max(wsum, 1e-6), 1.0);\n"
	"}\n";

// The screen shaders work on the composed image (Post_tex, the size of the
// output).
//
// Down: a quarter size copy for the bloom and the glow: the average of 4x4
// pixels, with a soft threshold (only the part of the color above
// u_thresh, with a knee of u_knee, passes).
static const char *Down_fs =
	"#version 330 core\n"
	"uniform sampler2D u_src;\n"
	"uniform vec2 u_texel;\n"
	"uniform float u_thresh;\n"
	"uniform float u_knee;\n"
	"out vec4 o_color;\n"
	"void main() {\n"
	"	vec2 uv = gl_FragCoord.xy * 4.0 * u_texel;\n"
	"	vec3 c = 0.25 * (texture(u_src, uv + u_texel * vec2(-1.0, -1.0)).rgb\n"
	"		+ texture(u_src, uv + u_texel * vec2(1.0, -1.0)).rgb\n"
	"		+ texture(u_src, uv + u_texel * vec2(-1.0, 1.0)).rgb\n"
	"		+ texture(u_src, uv + u_texel * vec2(1.0, 1.0)).rgb);\n"
	"	float l = max(c.r, max(c.g, c.b));\n"
	"	float soft = clamp(l - u_thresh + u_knee, 0.0, 2.0 * u_knee);\n"
	"	soft = soft * soft / (4.0 * u_knee + 1e-4);\n"
	"	float w = max(soft, l - u_thresh) / max(l, 1e-4);\n"
	"	o_color = vec4(c * w, 1.0);\n"
	"}\n";

// Blur: a separable Gaussian, 13 taps in the direction u_dir (texels).
static const char *Blur_fs =
	"#version 330 core\n"
	"uniform sampler2D u_src;\n"
	"uniform vec2 u_dir;\n"
	"uniform float u_sigma;\n"
	"out vec4 o_color;\n"
	"void main() {\n"
	"	vec2 size = vec2(textureSize(u_src, 0));\n"
	"	vec2 uv = gl_FragCoord.xy / size;\n"
	"	vec3 sum = vec3(0.0);\n"
	"	float wsum = 0.0;\n"
	"	float step = max(u_sigma / 2.0, 1.0);\n"
	"	for (int i = -6; i <= 6; i++) {\n"
	"		float x = float(i) * step;\n"
	"		float w = exp(-0.5 * x * x / (u_sigma * u_sigma));\n"
	"		sum += w * texture(u_src, uv + u_dir * x / size).rgb;\n"
	"		wsum += w;\n"
	"	}\n"
	"	o_color = vec4(sum / wsum, 1.0);\n"
	"}\n";

// Final: the output. u_mode 1 (Subtle): the image plus the bloom.
// u_mode 2 (VGA): a horizontal Gaussian of u_beam output pixels (the
// electron beam), the gamma, a faint line structure at the frame lines,
// a fine shadow mask (dot triads of 3 output pixels, the rows of triads
// shifted), a gain that keeps the white level, and the phosphor glow.
static const char *Final_fs =
	"#version 330 core\n"
	"uniform sampler2D u_src;\n"
	"uniform sampler2D u_bloom;\n"
	"uniform vec4 u_dst;\n"
	"uniform vec2 u_fsize;\n"
	"uniform int u_mode;\n"
	"uniform float u_bloom_k;\n"
	"uniform float u_beam;\n"
	"uniform float u_gamma;\n"
	"uniform float u_scan;\n"
	"uniform float u_mask;\n"
	"uniform float u_gain;\n"
	"out vec4 o_color;\n"
	"void main() {\n"
	"	vec2 px = gl_FragCoord.xy - u_dst.xy;\n"
	"	vec2 uv = px / u_dst.zw;\n"
	"	ivec2 ip = ivec2(floor(px));\n"
	"	ivec2 sz = textureSize(u_src, 0);\n"
	"	vec3 bloom = texture(u_bloom, uv).rgb;\n"
	"	if (u_mode == 1) {\n"
	"		o_color = vec4(texelFetch(u_src, ip, 0).rgb + bloom * u_bloom_k, 1.0);\n"
	"		return;\n"
	"	}\n"
	"	vec3 c = vec3(0.0);\n"
	"	float wsum = 0.0;\n"
	"	for (int i = -4; i <= 4; i++) {\n"
	"		float w = exp(-0.5 * float(i * i) / (u_beam * u_beam));\n"
	"		c += w * texelFetch(u_src, ivec2(clamp(ip.x + i, 0, sz.x - 1), ip.y), 0).rgb;\n"
	"		wsum += w;\n"
	"	}\n"
	"	c = pow(c / wsum, vec3(u_gamma));\n"
	"	float fy = (1.0 - uv.y) * u_fsize.y;\n"
	"	float d = fract(fy) - 0.5;\n"
	"	c *= 1.0 - u_scan * 4.0 * d * d;\n"
	"	int row = ip.y / 2;\n"
	"	int ph = (ip.x + (row & 1) * 2) % 3;\n"
	"	vec3 m = vec3(1.0 - u_mask);\n"
	"	m[ph] = 1.0;\n"
	"	c *= m * u_gain;\n"
	"	c += bloom * u_bloom_k;\n"
	"	o_color = vec4(c, 1.0);\n"
	"}\n";

// ASCENT: one upscaled interface picture into the layer. u_rect: the
// rectangle in normalized device coordinates (x0, y0, x1, y1), u_uv: the
// part of the picture (u0, v0, u1, v1, v from the top row).
static const char *Ui_vs =
	"#version 330 core\n"
	"uniform vec4 u_rect;\n"
	"uniform vec4 u_uv;\n"
	"out vec2 v_uv;\n"
	"void main() {\n"
	"	vec2 t = vec2(float(gl_VertexID & 1), float((gl_VertexID >> 1) & 1));\n"
	"	gl_Position = vec4(mix(u_rect.xy, u_rect.zw, t), 0.0, 1.0);\n"
	"	v_uv = mix(u_uv.xy, u_uv.zw, t);\n"
	"}\n";

static const char *Ui_fs =
	"#version 330 core\n"
	"uniform sampler2D u_tex;\n"
	"in vec2 v_uv;\n"
	"out vec4 o_color;\n"
	"void main() {\n"
	"	o_color = texture(u_tex, v_uv);\n"
	"}\n";

static GLuint compile(GLenum type, const char *src)
{
	GLuint s = qglCreateShader(type);
	GLint ok = 0;
	char log[1024];

	qglShaderSource(s, 1, &src, NULL);
	qglCompileShader(s);
	qglGetShaderiv(s, GL_COMPILE_STATUS, &ok);
	if (!ok) {
		qglGetShaderInfoLog(s, sizeof(log), NULL, log);
		plat_log_error("GPU: shader error: %s\n", log);
		qglDeleteShader(s);
		return 0;
	}
	return s;
}

static GLuint link_program(const char *vs, const char *fs)
{
	GLuint v = compile(GL_VERTEX_SHADER, vs);
	GLuint f = compile(GL_FRAGMENT_SHADER, fs);
	GLuint p;
	GLint ok = 0;
	char log[1024];

	if (!v || !f)
		return 0;
	p = qglCreateProgram();
	qglAttachShader(p, v);
	qglAttachShader(p, f);
	qglLinkProgram(p);
	qglDeleteShader(v);
	qglDeleteShader(f);
	qglGetProgramiv(p, GL_LINK_STATUS, &ok);
	if (!ok) {
		qglGetProgramInfoLog(p, sizeof(log), NULL, log);
		plat_log_error("GPU: link error: %s\n", log);
		qglDeleteProgram(p);
		return 0;
	}
	return p;
}

// ---------------------------------------------------------------------
// State

static int Gpu_ready;
static SDL_Window *Gpu_window;

static GLuint Scene_prog, Comp_prog, Down_prog, Blur_prog, Final_prog;
static struct {
	GLint canvas, znear, mode, lit, transparent, color, rgb, tex, fade, dest, vpal, nearc, levels, rowk;
	GLint infra, heat, fx, fxrow, fxdy, dim, tint;
} Su;
static struct {
	GLint frame, pal, view, fade, plut, vpal, nearc, levels, rowk, dst, fsize, vrect, vsize, key, vrgb;
	GLint ndark, drect, dlevel, filter, sigma, vsigma, vrad;
	GLint ui, ref, hd, uplut, upal;		// ASCENT: the upscaled interface pictures
} Cu;
static GLuint Ui_prog;
static struct { GLint rect, uv, tex; } Uu;
static void ui_shutdown(void);
static struct { GLint src, texel, thresh, knee; } Du;
static struct { GLint src, dir, sigma; } Bu;
static struct { GLint src, bloom, dst, fsize, mode, bloom_k, beam, gamma, scan, mask, gain; } Fu;

static GLuint Scene_vao, Scene_vbo, Comp_vao;
static GLuint Sampler_repeat, Sampler_clamp;	// the 8-bit textures: nearest
static GLuint Sampler_rgb[2][2][2];				// [clamp][smooth][aniso]
static float Max_aniso;							// 0: no anisotropic filtering
static GLuint Fade_tex;
static uint8_t Fade_copy[256 * 64];
static int Fade_levels;

// The RGB form
static uint8_t View_pal[768];			// the palette of the bitmaps (6 bits)
static int View_pal_set;
static GLuint Vpal_tex;					// View_pal as RGBA8, 256x1
static GLuint Near_tex;					// the nearest palette color of each 6-bit color, R8UI 64x64x64
static float Row_k[64];					// the average scale of the colors of each fade table row
static int Light_dirty = 1;
static int Infra_on;		// plat_gpu_set_infravision
static uint8_t Near_pal[768];
static int Near_valid_pal;
static GLuint Plut_tex;					// View_pal to the frame palette, RGBA16F, 256x1
static uint8_t Plut_src[768], Plut_dst[768];
static int Plut_valid;

// The view image
static GLuint View_fbo, View_tex, View_depth, Dest_tex;
static int View_w, View_h;				// size of the image
static int View_rgb;					// the form of the image
static float Canvas_w, Canvas_h;		// coordinate space of the vertices
static int In_view;
static int View_flags;
static int View_exists;					// an image was drawn

// Where the frame shows the view image
static int Dest_x, Dest_y, Dest_w, Dest_h;

// The darkened rectangles of the view
#define MAX_DARK 16		// the size of u_drect and u_dlevel of Comp_fs
static int Dark_n;
static float Dark_rect[MAX_DARK * 4];
static int Dark_level[MAX_DARK];

// The frame
static GLuint Frame_tex, Pal_tex;
static int Frame_tex_w, Frame_tex_h;
static float Rect_x, Rect_y, Rect_w, Rect_h;	// window coordinates
static int Rect_fw, Rect_fh;

// The screen shader targets: the composed image, and two images of a
// quarter of its size for the bloom and the glow.
static GLuint Post_fbo, Post_tex, Bloom_fbo[2], Bloom_tex[2];
static int Post_w, Post_h, Bloom_w, Bloom_h;

// The settings (plat_gpu_bind_settings)
static const int *Set_enable, *Set_shader;

// The screenshot target
static GLuint Shot_fbo, Shot_tex;
static int Shot_w, Shot_h;

// The RGBA textures: flags, the serial number of the upscaled loads, the
// memory. Indexed by the OpenGL name.
typedef struct tex_info {
	uint8_t rgba, flags;
	unsigned serial;
	int64_t bytes;
} tex_info;
static tex_info *Tex_info;
static int Tex_info_n;
static int64_t Tex_bytes;

// The names of the RGBA textures, made 256 at a time: on the test machine
// one glGenTextures call cost about 0.45 ms, so the preload of render.c
// (hundreds of textures at a level start) waited 0.3 s for the names alone.
static GLuint Name_pool[256];
static int Name_pool_n;

// The batch of polygons
typedef struct {
	int mode, texture, lit, transparent, clamp, color, depth_write, smooth, heat, fx, fx_row, dim, tint;
} draw_state;

typedef struct {
	draw_state st;
	int first, count;
} draw_cmd;

static plat_gpu_vertex *Verts;
static int N_verts, Max_verts;
static draw_cmd *Cmds;
static int N_cmds, Max_cmds;

// Statistics (environment variable ASCENT_GPU_STATS=1)
static int Stats_on;
static GLuint Stats_query[4];
static int Stats_query_used[4];
static int Stats_query_next;
static uint64_t Stats_frames, Stats_views, Stats_gpu_ns, Stats_gpu_samples;
static uint64_t Stats_view_cpu_us, Stats_frame_us_sum, Stats_frame_samples;
static uint64_t Stats_last_present_us, Stats_view_begin_us;
static uint64_t Stats_frame_us_max;
// the frames longer than 25 ms and 50 ms, and the present count of the
// last of them
static uint64_t Stats_slow25, Stats_slow50, Stats_slow_last;
static uint64_t Stats_hd_uploads, Stats_hd_upload_us;
// the polygons and triangles of the views, the CPU time of the longest
// view, and the GPU time of the output (the scale to the window and the
// screen shader)
static uint64_t Stats_polys, Stats_tris, Stats_view_polys, Stats_view_polys_max;
static uint64_t Stats_view_cpu_us_max;
static GLuint Stats_out_query[4];
static int Stats_out_used[4], Stats_out_next;
static uint64_t Stats_out_ns, Stats_out_samples;
static int Stats_out_w, Stats_out_h;

static uint64_t now_us(void)
{
	return SDL_GetTicksNS() / 1000;
}

static tex_info *tex_info_get(GLuint t)
{
	if ((int) t >= Tex_info_n) {
		int n = Tex_info_n ? Tex_info_n : 1024;
		tex_info *ni;
		while (n <= (int) t)
			n *= 2;
		ni = (tex_info *) realloc(Tex_info, (size_t) n * sizeof(*ni));
		if (!ni)
			return NULL;
		memset(ni + Tex_info_n, 0, (size_t) (n - Tex_info_n) * sizeof(*ni));
		Tex_info = ni;
		Tex_info_n = n;
	}
	return &Tex_info[t];
}

// ---------------------------------------------------------------------
// Init

static GLuint make_r8_texture(int w, int h, const void *data)
{
	GLuint t;

	qglGenTextures(1, &t);
	qglBindTexture(GL_TEXTURE_2D, t);
	qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
	qglPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	qglTexImage2D(GL_TEXTURE_2D, 0, GL_R8, w, h, 0, GL_RED, GL_UNSIGNED_BYTE, data);
	return t;
}

// A texture without mipmaps for a render target or a table. linear: the
// linear filter.
static GLuint make_texture(GLint ifmt, int w, int h, GLenum fmt, GLenum type, const void *data, int linear)
{
	GLuint t;

	qglGenTextures(1, &t);
	qglBindTexture(GL_TEXTURE_2D, t);
	qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, linear ? GL_LINEAR : GL_NEAREST);
	qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, linear ? GL_LINEAR : GL_NEAREST);
	qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
	qglPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	qglTexImage2D(GL_TEXTURE_2D, 0, ifmt, w, h, 0, fmt, type, data);
	return t;
}

// 8x anisotropic filtering: OpenGL 4.6 or one of the two extensions.
static void find_anisotropy(void)
{
	GLint major = 0, minor = 0, n = 0, i;
	int found = 0;

	qglGetIntegerv(GL_MAJOR_VERSION, &major);
	qglGetIntegerv(GL_MINOR_VERSION, &minor);
	if (major > 4 || (major == 4 && minor >= 6))
		found = 1;
	qglGetIntegerv(GL_NUM_EXTENSIONS, &n);
	for (i = 0; i < n && !found; i++) {
		const char *e = (const char *) qglGetStringi(GL_EXTENSIONS, (GLuint) i);
		if (e && (!strcmp(e, "GL_EXT_texture_filter_anisotropic") || !strcmp(e, "GL_ARB_texture_filter_anisotropic")))
			found = 1;
	}
	Max_aniso = 0;
	if (found) {
		GLfloat m = 0;
		qglGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY, &m);
		qglGetError();
		if (m >= 2.0f)
			Max_aniso = m < 8.0f ? m : 8.0f;
	}
}

static void make_samplers(void)
{
	int c, s, a;

	qglGenSamplers(1, &Sampler_repeat);
	qglGenSamplers(1, &Sampler_clamp);
	qglSamplerParameteri(Sampler_repeat, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	qglSamplerParameteri(Sampler_repeat, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	qglSamplerParameteri(Sampler_repeat, GL_TEXTURE_WRAP_S, GL_REPEAT);
	qglSamplerParameteri(Sampler_repeat, GL_TEXTURE_WRAP_T, GL_REPEAT);
	qglSamplerParameteri(Sampler_clamp, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	qglSamplerParameteri(Sampler_clamp, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	qglSamplerParameteri(Sampler_clamp, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	qglSamplerParameteri(Sampler_clamp, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	// The RGBA textures. Magnification: nearest for the original bitmaps
	// (the sharp pixels near the player), linear for the upscaled set.
	// Minification: always trilinear mipmaps, so that far textures do not
	// shimmer; the anisotropic filtering setting adds the 8x anisotropy.
	for (c = 0; c < 2; c++)
		for (s = 0; s < 2; s++)
			for (a = 0; a < 2; a++) {
				GLuint sm;
				qglGenSamplers(1, &sm);
				qglSamplerParameteri(sm, GL_TEXTURE_WRAP_S, c ? GL_CLAMP_TO_EDGE : GL_REPEAT);
				qglSamplerParameteri(sm, GL_TEXTURE_WRAP_T, c ? GL_CLAMP_TO_EDGE : GL_REPEAT);
				qglSamplerParameteri(sm, GL_TEXTURE_MAG_FILTER, s ? GL_LINEAR : GL_NEAREST);
				qglSamplerParameteri(sm, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
				if (a && Max_aniso > 0)
					qglSamplerParameterf(sm, GL_TEXTURE_MAX_ANISOTROPY, Max_aniso);
				Sampler_rgb[c][s][a] = sm;
			}
}

int gpu_init(SDL_Window *window)
{
	const char *e;
	int i;

	if (load_gl() != 0)
		return -1;

	Scene_prog = link_program(Scene_vs, Scene_fs);
	Comp_prog = link_program(Full_vs, Comp_fs);
	Down_prog = link_program(Full_vs, Down_fs);
	Blur_prog = link_program(Full_vs, Blur_fs);
	Final_prog = link_program(Full_vs, Final_fs);
	if (!Scene_prog || !Comp_prog)
		return -1;

	Su.canvas = qglGetUniformLocation(Scene_prog, "u_canvas");
	Su.znear = qglGetUniformLocation(Scene_prog, "u_znear");
	Su.mode = qglGetUniformLocation(Scene_prog, "u_mode");
	Su.lit = qglGetUniformLocation(Scene_prog, "u_lit");
	Su.transparent = qglGetUniformLocation(Scene_prog, "u_transparent");
	Su.color = qglGetUniformLocation(Scene_prog, "u_color");
	Su.rgb = qglGetUniformLocation(Scene_prog, "u_rgb");
	Su.tex = qglGetUniformLocation(Scene_prog, "u_tex");
	Su.fade = qglGetUniformLocation(Scene_prog, "u_fade");
	Su.dest = qglGetUniformLocation(Scene_prog, "u_dest");
	Su.vpal = qglGetUniformLocation(Scene_prog, "u_vpal");
	Su.nearc = qglGetUniformLocation(Scene_prog, "u_near");
	Su.rowk = qglGetUniformLocation(Scene_prog, "u_rowk");
	Su.levels = qglGetUniformLocation(Scene_prog, "u_levels");
	Su.infra = qglGetUniformLocation(Scene_prog, "u_infra");
	Su.heat = qglGetUniformLocation(Scene_prog, "u_heat");
	Su.fx = qglGetUniformLocation(Scene_prog, "u_fx");
	Su.fxrow = qglGetUniformLocation(Scene_prog, "u_fxrow");
	Su.fxdy = qglGetUniformLocation(Scene_prog, "u_fxdy");
	Su.dim = qglGetUniformLocation(Scene_prog, "u_dim");
	Su.tint = qglGetUniformLocation(Scene_prog, "u_tint");
	Cu.frame = qglGetUniformLocation(Comp_prog, "u_frame");
	Cu.pal = qglGetUniformLocation(Comp_prog, "u_pal");
	Cu.view = qglGetUniformLocation(Comp_prog, "u_view");
	Cu.fade = qglGetUniformLocation(Comp_prog, "u_fade");
	Cu.plut = qglGetUniformLocation(Comp_prog, "u_plut");
	Cu.nearc = qglGetUniformLocation(Comp_prog, "u_near");
	Cu.rowk = qglGetUniformLocation(Comp_prog, "u_rowk");
	Cu.vpal = qglGetUniformLocation(Comp_prog, "u_vpal");
	Cu.levels = qglGetUniformLocation(Comp_prog, "u_levels");
	Cu.dst = qglGetUniformLocation(Comp_prog, "u_dst");
	Cu.fsize = qglGetUniformLocation(Comp_prog, "u_fsize");
	Cu.vrect = qglGetUniformLocation(Comp_prog, "u_vrect");
	Cu.vsize = qglGetUniformLocation(Comp_prog, "u_vsize");
	Cu.key = qglGetUniformLocation(Comp_prog, "u_key");
	Cu.vrgb = qglGetUniformLocation(Comp_prog, "u_vrgb");
	Cu.ndark = qglGetUniformLocation(Comp_prog, "u_ndark");
	Cu.drect = qglGetUniformLocation(Comp_prog, "u_drect");
	Cu.dlevel = qglGetUniformLocation(Comp_prog, "u_dlevel");
	Cu.filter = qglGetUniformLocation(Comp_prog, "u_filter");
	Cu.sigma = qglGetUniformLocation(Comp_prog, "u_sigma");
	Cu.vsigma = qglGetUniformLocation(Comp_prog, "u_vsigma");
	Cu.vrad = qglGetUniformLocation(Comp_prog, "u_vrad");
	Cu.ui = qglGetUniformLocation(Comp_prog, "u_ui");
	Cu.ref = qglGetUniformLocation(Comp_prog, "u_ref");
	Cu.hd = qglGetUniformLocation(Comp_prog, "u_hd");
	Cu.uplut = qglGetUniformLocation(Comp_prog, "u_uplut");
	Cu.upal = qglGetUniformLocation(Comp_prog, "u_upal");
	Ui_prog = link_program(Ui_vs, Ui_fs);
	if (Ui_prog) {
		Uu.rect = qglGetUniformLocation(Ui_prog, "u_rect");
		Uu.uv = qglGetUniformLocation(Ui_prog, "u_uv");
		Uu.tex = qglGetUniformLocation(Ui_prog, "u_tex");
	}
	if (Down_prog && Blur_prog && Final_prog) {
		Du.src = qglGetUniformLocation(Down_prog, "u_src");
		Du.texel = qglGetUniformLocation(Down_prog, "u_texel");
		Du.thresh = qglGetUniformLocation(Down_prog, "u_thresh");
		Du.knee = qglGetUniformLocation(Down_prog, "u_knee");
		Bu.src = qglGetUniformLocation(Blur_prog, "u_src");
		Bu.dir = qglGetUniformLocation(Blur_prog, "u_dir");
		Bu.sigma = qglGetUniformLocation(Blur_prog, "u_sigma");
		Fu.src = qglGetUniformLocation(Final_prog, "u_src");
		Fu.bloom = qglGetUniformLocation(Final_prog, "u_bloom");
		Fu.dst = qglGetUniformLocation(Final_prog, "u_dst");
		Fu.fsize = qglGetUniformLocation(Final_prog, "u_fsize");
		Fu.mode = qglGetUniformLocation(Final_prog, "u_mode");
		Fu.bloom_k = qglGetUniformLocation(Final_prog, "u_bloom_k");
		Fu.beam = qglGetUniformLocation(Final_prog, "u_beam");
		Fu.gamma = qglGetUniformLocation(Final_prog, "u_gamma");
		Fu.scan = qglGetUniformLocation(Final_prog, "u_scan");
		Fu.mask = qglGetUniformLocation(Final_prog, "u_mask");
		Fu.gain = qglGetUniformLocation(Final_prog, "u_gain");
	}

	qglGenVertexArrays(1, &Scene_vao);
	qglGenBuffers(1, &Scene_vbo);
	qglBindVertexArray(Scene_vao);
	qglBindBuffer(GL_ARRAY_BUFFER, Scene_vbo);
	qglVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(plat_gpu_vertex), (void *) 0);
	qglVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(plat_gpu_vertex), (void *) (3 * sizeof(float)));
	qglVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(plat_gpu_vertex), (void *) (6 * sizeof(float)));
	qglVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, sizeof(plat_gpu_vertex), (void *) (8 * sizeof(float)));
	qglEnableVertexAttribArray(0);
	qglEnableVertexAttribArray(1);
	qglEnableVertexAttribArray(2);
	qglEnableVertexAttribArray(3);
	qglGenVertexArrays(1, &Comp_vao);
	qglBindVertexArray(0);

	find_anisotropy();
	make_samplers();

	// An identity fade table until the engine gives one.
	Fade_levels = 34;
	for (i = 0; i < 256 * Fade_levels; i++)
		Fade_copy[i] = (uint8_t) (i & 255);
	Fade_tex = make_r8_texture(256, Fade_levels, Fade_copy);

	Pal_tex = make_texture(GL_RGBA8, 256, 1, GL_RGBA, GL_UNSIGNED_BYTE, NULL, 0);
	Vpal_tex = make_texture(GL_RGBA8, 256, 1, GL_RGBA, GL_UNSIGNED_BYTE, NULL, 0);
	qglGenTextures(1, &Near_tex);
	qglBindTexture(GL_TEXTURE_3D, Near_tex);
	qglTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	qglTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	qglTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAX_LEVEL, 0);
	qglPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	qglTexImage3D(GL_TEXTURE_3D, 0, GL_R8UI, 64, 64, 64, 0, GL_RED_INTEGER, GL_UNSIGNED_BYTE, NULL);
	for (i = 0; i < 64; i++)
		Row_k[i] = 1.0f;
	Plut_tex = make_texture(GL_RGBA16F, 256, 1, GL_RGBA, GL_FLOAT, NULL, 1);
	Light_dirty = 1;
	Plut_valid = 0;

	gputex_start();

	e = SDL_getenv("ASCENT_GPU_STATS");
	Stats_on = (e && e[0] == '1');
	if (Stats_on) {
		qglGenQueries(4, Stats_query);
		qglGenQueries(4, Stats_out_query);
		printf("GPU: %s, anisotropic filtering %s (%.0fx)\n", (const char *) qglGetString(GL_RENDERER),
			Max_aniso > 0 ? "yes" : "no", Max_aniso);
	}

	Gpu_window = window;
	Gpu_ready = 1;
	return 0;
}

static void delete_view_target(void)
{
	if (View_fbo) qglDeleteFramebuffers(1, &View_fbo);
	if (View_tex) qglDeleteTextures(1, &View_tex);
	if (Dest_tex) qglDeleteTextures(1, &Dest_tex);
	if (View_depth) qglDeleteRenderbuffers(1, &View_depth);
	View_fbo = View_tex = Dest_tex = View_depth = 0;
	View_w = View_h = 0;
	View_exists = 0;
}

static void delete_post_targets(void)
{
	int i;

	if (Post_fbo) qglDeleteFramebuffers(1, &Post_fbo);
	if (Post_tex) qglDeleteTextures(1, &Post_tex);
	for (i = 0; i < 2; i++) {
		if (Bloom_fbo[i]) qglDeleteFramebuffers(1, &Bloom_fbo[i]);
		if (Bloom_tex[i]) qglDeleteTextures(1, &Bloom_tex[i]);
		Bloom_fbo[i] = Bloom_tex[i] = 0;
	}
	Post_fbo = Post_tex = 0;
	Post_w = Post_h = 0;
}

static void stats_print(void)
{
	if (!Stats_on || !Stats_frames)
		return;
	printf("GPU stats: %d frames, %d views of %dx%d (%s)\n", (int) Stats_frames, (int) Stats_views, View_w, View_h,
		View_rgb ? "RGB" : "8-bit");
	if (Stats_frame_samples)
		printf("GPU stats: frame time avg %.3f ms, max %.3f ms (present to present)\n",
			Stats_frame_us_sum / 1000.0 / Stats_frame_samples, Stats_frame_us_max / 1000.0);
	if (Stats_frame_samples)
		printf("GPU stats: %d frames over 25 ms, %d over 50 ms, the last at present %d\n",
			(int) Stats_slow25, (int) Stats_slow50, (int) Stats_slow_last);
	if (Stats_views) {
		printf("GPU stats: 3D view CPU time avg %.3f ms, max %.3f ms (begin to end of the view)\n",
			Stats_view_cpu_us / 1000.0 / Stats_views, Stats_view_cpu_us_max / 1000.0);
		printf("GPU stats: polygons per view avg %.0f, max %d; triangles per view avg %.0f\n",
			(double) Stats_polys / Stats_views, (int) Stats_view_polys_max, (double) Stats_tris / Stats_views);
	}
	if (Stats_gpu_samples)
		printf("GPU stats: 3D view GPU time avg %.3f ms\n", Stats_gpu_ns / 1e6 / Stats_gpu_samples);
	if (Stats_out_samples)
		printf("GPU stats: output GPU time avg %.3f ms (the scale to the window of %dx%d and the shader)\n",
			Stats_out_ns / 1e6 / Stats_out_samples, Stats_out_w, Stats_out_h);
	printf("GPU stats: RGBA textures %.1f MB, %d upscaled uploads (%.1f ms in all)\n",
		Tex_bytes / 1048576.0, (int) Stats_hd_uploads, Stats_hd_upload_us / 1000.0);
	fflush(stdout);
}

void gpu_shutdown(void)
{
	int c, s, a;

	if (!Gpu_ready)
		return;
	stats_print();
	gputex_stop();
	delete_view_target();
	delete_post_targets();
	if (Shot_fbo) qglDeleteFramebuffers(1, &Shot_fbo);
	if (Shot_tex) qglDeleteTextures(1, &Shot_tex);
	if (Frame_tex) qglDeleteTextures(1, &Frame_tex);
	if (Pal_tex) qglDeleteTextures(1, &Pal_tex);
	if (Vpal_tex) qglDeleteTextures(1, &Vpal_tex);
	if (Near_tex) qglDeleteTextures(1, &Near_tex);
	if (Plut_tex) qglDeleteTextures(1, &Plut_tex);
	if (Fade_tex) qglDeleteTextures(1, &Fade_tex);
	if (Name_pool_n) qglDeleteTextures(Name_pool_n, Name_pool);
	Name_pool_n = 0;
	if (Stats_on) {
		qglDeleteQueries(4, Stats_query);
		qglDeleteQueries(4, Stats_out_query);
	}
	qglDeleteSamplers(1, &Sampler_repeat);
	qglDeleteSamplers(1, &Sampler_clamp);
	for (c = 0; c < 2; c++)
		for (s = 0; s < 2; s++)
			for (a = 0; a < 2; a++)
				qglDeleteSamplers(1, &Sampler_rgb[c][s][a]);
	qglDeleteBuffers(1, &Scene_vbo);
	qglDeleteVertexArrays(1, &Scene_vao);
	qglDeleteVertexArrays(1, &Comp_vao);
	qglDeleteProgram(Scene_prog);
	qglDeleteProgram(Comp_prog);
	if (Down_prog) qglDeleteProgram(Down_prog);
	if (Blur_prog) qglDeleteProgram(Blur_prog);
	if (Final_prog) qglDeleteProgram(Final_prog);
	ui_shutdown();
	free(Verts);
	free(Cmds);
	free(Tex_info);
	Verts = NULL;
	Cmds = NULL;
	Tex_info = NULL;
	Tex_info_n = 0;
	Tex_bytes = 0;
	N_verts = Max_verts = N_cmds = Max_cmds = 0;
	Shot_fbo = Shot_tex = Frame_tex = Pal_tex = Fade_tex = Vpal_tex = Near_tex = Plut_tex = 0;
	Frame_tex_w = Frame_tex_h = 0;
	Gpu_ready = 0;
	View_exists = 0;
}

int plat_gpu_available(void)
{
	return Gpu_ready;
}

void plat_gpu_bind_settings(const int *enable, const int *shader)
{
	Set_enable = enable;
	Set_shader = shader;
}

static int cur_shader(void)
{
	int s;

	if (!Set_shader || (Set_enable && !*Set_enable) || !Down_prog || !Blur_prog || !Final_prog)
		return PLAT_GPU_SHADER_OFF;
	s = *Set_shader;
	if (s != PLAT_GPU_SHADER_SUBTLE && s != PLAT_GPU_SHADER_VGA)
		return PLAT_GPU_SHADER_OFF;
	return s;
}

// ---------------------------------------------------------------------
// Textures

int plat_gpu_texture_create(int w, int h, const uint8_t *pixels)
{
	GLuint t;
	tex_info *ti;

	if (!Gpu_ready || w <= 0 || h <= 0)
		return 0;
	t = make_r8_texture(w, h, pixels);
	ti = tex_info_get(t);
	if (ti) {
		ti->rgba = 0;
		ti->flags = 0;
		ti->bytes = 0;
	}
	return (int) t;
}

void plat_gpu_texture_update(int texture, int w, int h, const uint8_t *pixels)
{
	if (!Gpu_ready || texture <= 0)
		return;
	qglBindTexture(GL_TEXTURE_2D, (GLuint) texture);
	qglPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	qglTexImage2D(GL_TEXTURE_2D, 0, GL_R8, w, h, 0, GL_RED, GL_UNSIGNED_BYTE, pixels);
}

// Put RGBA pixels (already bled) into the texture t with the mipmaps.
static void upload_rgba(GLuint t, int w, int h, const uint8_t *rgba, int flags)
{
	tex_info *ti = tex_info_get(t);
	int64_t bytes = (int64_t) w * h * 4 * 4 / 3;

	qglBindTexture(GL_TEXTURE_2D, t);
	qglPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	qglTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
	qglGenerateMipmap(GL_TEXTURE_2D);
	if (ti) {
		Tex_bytes += bytes - ti->bytes;
		ti->bytes = bytes;
		ti->rgba = 1;
		ti->flags = (uint8_t) flags;
	}
}

static GLuint gen_texture_name(void)
{
	if (Name_pool_n == 0) {
		qglGenTextures(256, Name_pool);
		Name_pool_n = 256;
	}
	return Name_pool[--Name_pool_n];
}

int plat_gpu_texture_create_rgba(int w, int h, const uint8_t *rgba, int flags)
{
	GLuint t;
	uint8_t *copy;

	if (!Gpu_ready || w <= 0 || h <= 0 || !rgba)
		return 0;
	copy = (uint8_t *) malloc((size_t) w * h * 4);
	if (!copy)
		return 0;
	memcpy(copy, rgba, (size_t) w * h * 4);
	gputex_bleed(copy, w, h);
	t = gen_texture_name();
	qglBindTexture(GL_TEXTURE_2D, t);
	qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	upload_rgba(t, w, h, copy, flags);
	free(copy);
	return (int) t;
}

void plat_gpu_texture_delete(int texture)
{
	GLuint t = (GLuint) texture;
	tex_info *ti;

	if (!Gpu_ready || texture <= 0)
		return;
	ti = tex_info_get(t);
	if (ti) {
		Tex_bytes -= ti->bytes;
		ti->bytes = 0;
		ti->rgba = 0;
		ti->flags = 0;
		ti->serial++;		// a load in progress is dropped
	}
	qglDeleteTextures(1, &t);
}

void plat_gpu_texture_load_hd(int texture, const plat_gpu_hd_job *job)
{
	tex_info *ti;

	if (!Gpu_ready || texture <= 0 || !job)
		return;
	ti = tex_info_get((GLuint) texture);
	if (!ti || !ti->rgba)
		return;
	ti->serial++;
	gputex_queue(texture, ti->serial, job);
}

int plat_gpu_hd_pending(void)
{
	return gputex_pending();
}

int64_t plat_gpu_texture_bytes(void)
{
	return Tex_bytes;
}

// Upload the finished loads, up to about 24 MB and Upload_budget_us of
// time in one frame (at least one load), so that a level start does not
// stop for all of them at once: the preload of a level gives 300 to 700
// loads. ASCENT_GPU_UPLOAD_MS sets the time (0: no time limit).
static int64_t Upload_budget_us = -1;

static void take_hd_results(void)
{
	int64_t budget = 24 << 20;
	uint64_t start = now_us();
	gputex_result *r;

	if (Upload_budget_us < 0) {
		const char *e = SDL_getenv("ASCENT_GPU_UPLOAD_MS");
		Upload_budget_us = e ? (int64_t) (atof(e) * 1000.0) : 4000;
		if (Upload_budget_us < 0)
			Upload_budget_us = 0;
	}
	while (budget > 0 && (r = gputex_take()) != NULL) {
		tex_info *ti = tex_info_get((GLuint) r->texture);
		if (ti && ti->rgba && ti->serial == r->serial) {
			uint64_t t0 = now_us();
			upload_rgba((GLuint) r->texture, r->w, r->h, r->rgba, PLAT_GPU_TEX_SMOOTH);
			Stats_hd_upload_us += now_us() - t0;
			Stats_hd_uploads++;
			budget -= (int64_t) r->w * r->h * 4;
		}
		gputex_free(r);
		if (Upload_budget_us > 0 && (int64_t) (now_us() - start) >= Upload_budget_us)
			break;
	}
}

void plat_gpu_set_infravision(int on)
{
	Infra_on = on != 0;
}

void plat_gpu_set_fade_table(const uint8_t *table, int levels)
{
	if (!Gpu_ready || levels <= 0 || levels > 64)
		return;
	if (levels == Fade_levels && !memcmp(table, Fade_copy, 256 * levels))
		return;
	memcpy(Fade_copy, table, 256 * levels);
	Fade_levels = levels;
	qglBindTexture(GL_TEXTURE_2D, Fade_tex);
	qglPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	qglTexImage2D(GL_TEXTURE_2D, 0, GL_R8, 256, levels, 0, GL_RED, GL_UNSIGNED_BYTE, Fade_copy);
	Light_dirty = 1;
}

static int expand6(int v)
{
	v &= 63;
	return (v << 2) | (v >> 4);
}

void plat_gpu_set_view_palette(const uint8_t *pal6)
{
	uint8_t rgba[256 * 4];
	int i;

	if (!Gpu_ready || !pal6)
		return;
	if (View_pal_set && !memcmp(View_pal, pal6, 768))
		return;
	memcpy(View_pal, pal6, 768);
	View_pal_set = 1;
	for (i = 0; i < 256; i++) {
		rgba[i * 4] = (uint8_t) expand6(pal6[i * 3]);
		rgba[i * 4 + 1] = (uint8_t) expand6(pal6[i * 3 + 1]);
		rgba[i * 4 + 2] = (uint8_t) expand6(pal6[i * 3 + 2]);
		rgba[i * 4 + 3] = 255;
	}
	qglBindTexture(GL_TEXTURE_2D, Vpal_tex);
	qglPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	qglTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 256, 1, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
	Light_dirty = 1;
	Plut_valid = 0;
}

// A curve of 256 values (0 to 1) from pairs (x, y) of 8-bit values: the
// average y for each x; between the known x, linear; below the first and
// above the last known x, proportional to x.
static void make_curve(const int *sum, const int *cnt, float *out, int stride)
{
	int x, prev = -1, k;
	float yv[256];
	int known[256], nk = 0;

	for (x = 0; x < 256; x++)
		if (cnt[x]) {
			yv[x] = (float) sum[x] / cnt[x];
			known[nk++] = x;
		}
	if (!nk) {
		for (x = 0; x < 256; x++)
			out[x * stride] = x / 255.0f;
		return;
	}
	for (x = 0, k = 0; x < 256; x++) {
		float v;
		while (k < nk && known[k] < x)
			prev = known[k++];
		if (k < nk && known[k] == x)
			v = yv[x];
		else if (prev < 0)
			v = known[0] > 0 ? yv[known[0]] * x / known[0] : yv[known[0]];
		else if (k >= nk)
			v = prev > 0 ? yv[prev] * x / prev : yv[prev];
		else
			v = yv[prev] + (yv[known[k]] - yv[prev]) * (x - prev) / (known[k] - prev);
		if (v > 255) v = 255;
		if (v < 0) v = 0;
		out[x * stride] = v / 255.0f;
	}
}

// The tables of the light of the RGB form (see the comment at the top):
// the nearest palette color (not 255, the transparent color) of each
// 6-bit color, and the average scale of each fade table row (the
// luminance of the faded colors over the luminance of the colors).
static void update_light(void)
{
	static uint8_t nearc[64 * 64 * 64];
	int r, g, b, c, best, bd;

	if (!Light_dirty || !View_pal_set)
		return;
	Light_dirty = 0;
	if (Near_valid_pal && !memcmp(Near_pal, View_pal, 768))
		goto rows;
	for (r = 0; r < 64; r++)
		for (g = 0; g < 64; g++)
			for (b = 0; b < 64; b++) {
				best = 0;
				bd = 1 << 30;
				for (c = 0; c < 255; c++) {
					int dr = r - View_pal[c * 3], dg = g - View_pal[c * 3 + 1], db = b - View_pal[c * 3 + 2];
					int d = 3 * dr * dr + 4 * dg * dg + 2 * db * db;
					if (d < bd) {
						bd = d;
						best = c;
						if (!d)
							break;
					}
				}
				nearc[(b * 64 + g) * 64 + r] = (uint8_t) best;
			}
	qglBindTexture(GL_TEXTURE_3D, Near_tex);
	qglPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	qglTexImage3D(GL_TEXTURE_3D, 0, GL_R8UI, 64, 64, 64, 0, GL_RED_INTEGER, GL_UNSIGNED_BYTE, nearc);
	memcpy(Near_pal, View_pal, 768);
	Near_valid_pal = 1;
rows:
	for (r = 0; r < Fade_levels; r++) {
		double s0 = 0, s1 = 0;
		for (c = 0; c < 255; c++) {
			int f = Fade_copy[r * 256 + c];
			s0 += 0.299 * View_pal[c * 3] + 0.587 * View_pal[c * 3 + 1] + 0.114 * View_pal[c * 3 + 2];
			s1 += 0.299 * View_pal[f * 3] + 0.587 * View_pal[f * 3 + 1] + 0.114 * View_pal[f * 3 + 2];
		}
		Row_k[r] = s0 > 0 ? (float) (s1 / s0) : 1.0f;
	}
}

// The curves from the palette of the view to the palette of the frame.
static void update_plut(const uint8_t *pal6)
{
	float data[256 * 4];
	int ch, c;

	if (Plut_valid && !memcmp(Plut_src, View_pal, 768) && !memcmp(Plut_dst, pal6, 768))
		return;
	memcpy(Plut_src, View_pal, 768);
	memcpy(Plut_dst, pal6, 768);
	Plut_valid = 1;
	for (ch = 0; ch < 3; ch++) {
		int sum[256], cnt[256];
		memset(sum, 0, sizeof(sum));
		memset(cnt, 0, sizeof(cnt));
		for (c = 0; c < 256; c++) {
			int x = expand6(View_pal[c * 3 + ch]);
			sum[x] += expand6(pal6[c * 3 + ch]);
			cnt[x]++;
		}
		make_curve(sum, cnt, data + ch, 4);
	}
	for (c = 0; c < 256; c++)
		data[c * 4 + 3] = 1.0f;
	qglBindTexture(GL_TEXTURE_2D, Plut_tex);
	qglTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, 256, 1, 0, GL_RGBA, GL_FLOAT, data);
}

// ---------------------------------------------------------------------
// The 3D view

static int make_view_target(int w, int h, int rgb)
{
	if (View_fbo && w == View_w && h == View_h && rgb == View_rgb)
		return 0;
	delete_view_target();

	if (rgb) {
		View_tex = make_texture(GL_RGBA8, w, h, GL_RGBA, GL_UNSIGNED_BYTE, NULL, 0);
		Dest_tex = make_texture(GL_RGBA8, w, h, GL_RGBA, GL_UNSIGNED_BYTE, NULL, 0);
	} else {
		View_tex = make_r8_texture(w, h, NULL);
		Dest_tex = make_r8_texture(w, h, NULL);
	}
	qglGenRenderbuffers(1, &View_depth);
	qglBindRenderbuffer(GL_RENDERBUFFER, View_depth);
	qglRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, w, h);
	qglGenFramebuffers(1, &View_fbo);
	qglBindFramebuffer(GL_FRAMEBUFFER, View_fbo);
	qglFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, View_tex, 0);
	qglFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, View_depth);
	if (qglCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
		qglBindFramebuffer(GL_FRAMEBUFFER, 0);
		delete_view_target();
		return -1;
	}
	View_w = w;
	View_h = h;
	View_rgb = rgb;
	return 0;
}

static draw_state Cur_state;
static int Cur_state_valid;

static GLuint sampler_for(const draw_state *st)
{
	if (!View_rgb)
		return st->clamp ? Sampler_clamp : Sampler_repeat;
	return Sampler_rgb[st->clamp != 0][st->smooth != 0][(View_flags & PLAT_GPU_VIEW_ANISO) != 0];
}

static void apply_state(const draw_state *st)
{
	if (Cur_state_valid && !memcmp(st, &Cur_state, sizeof(*st)))
		return;
	if (!Cur_state_valid || st->mode != Cur_state.mode)
		qglUniform1i(Su.mode, st->mode);
	if (!Cur_state_valid || st->lit != Cur_state.lit)
		qglUniform1i(Su.lit, st->lit);
	if (!Cur_state_valid || st->transparent != Cur_state.transparent)
		qglUniform1i(Su.transparent, st->transparent);
	if (!Cur_state_valid || st->color != Cur_state.color)
		qglUniform1i(Su.color, st->color);
	if (!Cur_state_valid || st->heat != Cur_state.heat)
		qglUniform1i(Su.heat, st->heat);
	if (!Cur_state_valid || st->fx != Cur_state.fx)
		qglUniform1i(Su.fx, st->fx);
	if (!Cur_state_valid || st->fx_row != Cur_state.fx_row)
		qglUniform1i(Su.fxrow, st->fx_row);
	if (!Cur_state_valid || st->dim != Cur_state.dim)
		qglUniform1i(Su.dim, st->dim);
	if (!Cur_state_valid || st->tint != Cur_state.tint)
		qglUniform1i(Su.tint, st->tint);
	if (st->mode == PLAT_GPU_TEXTURED && (!Cur_state_valid || st->texture != Cur_state.texture)) {
		qglActiveTexture(GL_TEXTURE0);
		qglBindTexture(GL_TEXTURE_2D, (GLuint) st->texture);
	}
	if (!Cur_state_valid || st->clamp != Cur_state.clamp || st->smooth != Cur_state.smooth)
		qglBindSampler(0, sampler_for(st));
	if (!Cur_state_valid || st->depth_write != Cur_state.depth_write)
		qglDepthMask(st->depth_write ? GL_TRUE : GL_FALSE);
	Cur_state = *st;
	Cur_state_valid = 1;
}

// Draw the polygons of the batch.
static void flush(void)
{
	int i;

	if (!N_cmds)
		return;
	qglBindBuffer(GL_ARRAY_BUFFER, Scene_vbo);
	qglBufferData(GL_ARRAY_BUFFER, (GLsizeiptr) N_verts * sizeof(plat_gpu_vertex), Verts, GL_STREAM_DRAW);
	for (i = 0; i < N_cmds; i++) {
		apply_state(&Cmds[i].st);
		qglDrawArrays(GL_TRIANGLES, Cmds[i].first, Cmds[i].count);
	}
	N_verts = 0;
	N_cmds = 0;
}

void plat_gpu_begin_view(int canvas_w, int canvas_h, int target_w, int target_h, int flags, int clear_color)
{
	int rgb = (flags & PLAT_GPU_VIEW_RGB) != 0;

	if (!Gpu_ready || canvas_w <= 0 || canvas_h <= 0 || target_w <= 0 || target_h <= 0)
		return;
	if (target_w > 8192) target_w = 8192;
	if (target_h > 8192) target_h = 8192;
	if (make_view_target(target_w, target_h, rgb) != 0)
		return;

	take_hd_results();
	if (rgb)
		update_light();

	if (Stats_on) {
		int q = Stats_query_next;
		GLuint64 ns = 0;
		if (Stats_query_used[q]) {
			qglGetQueryObjectui64v(Stats_query[q], GL_QUERY_RESULT, &ns);
			Stats_gpu_ns += ns;
			Stats_gpu_samples++;
		}
		qglBeginQuery(GL_TIME_ELAPSED, Stats_query[q]);
		Stats_query_used[q] = 1;
		Stats_view_begin_us = now_us();
		Stats_view_polys = 0;
	}

	Canvas_w = (float) canvas_w;
	Canvas_h = (float) canvas_h;
	View_flags = flags;

	qglBindFramebuffer(GL_FRAMEBUFFER, View_fbo);
	qglViewport(0, 0, View_w, View_h);
	qglDisable(GL_BLEND);
	qglDisable(GL_CULL_FACE);
	qglDisable(GL_SCISSOR_TEST);
	qglEnable(GL_DEPTH_CLAMP);
	qglDepthMask(GL_TRUE);
	if (rgb) {
		int c = clear_color & 255;
		qglClearColor(expand6(View_pal[c * 3]) / 255.0f, expand6(View_pal[c * 3 + 1]) / 255.0f,
			expand6(View_pal[c * 3 + 2]) / 255.0f, 1);
	} else
		qglClearColor((clear_color & 255) / 255.0f, 0, 0, 1);
	qglClearDepth(1.0);
	qglClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	if (flags & PLAT_GPU_VIEW_DEPTH) {
		qglEnable(GL_DEPTH_TEST);
		qglDepthFunc(GL_LEQUAL);
	} else
		qglDisable(GL_DEPTH_TEST);

	qglUseProgram(Scene_prog);
	qglUniform2f(Su.canvas, Canvas_w, Canvas_h);
	qglUniform1f(Su.znear, 1.0f / 16.0f);
	qglUniform1i(Su.rgb, rgb);
	qglUniform1i(Su.infra, rgb && Infra_on);
	qglUniform1i(Su.tex, 0);
	qglUniform1i(Su.fade, 1);
	qglUniform1i(Su.dest, 2);
	// the image rows of one canvas row: the pixel "below" of the effect colors
	qglUniform1i(Su.fxdy, Canvas_h > 0 && View_h > Canvas_h ? (View_h + Canvas_h / 2) / Canvas_h : 1);
	qglUniform1i(Su.vpal, 3);
	qglUniform1i(Su.nearc, 4);
	qglUniform1i(Su.levels, Fade_levels);
	qglUniform1fv(Su.rowk, 64, Row_k);
	qglActiveTexture(GL_TEXTURE1);
	qglBindTexture(GL_TEXTURE_2D, Fade_tex);
	qglActiveTexture(GL_TEXTURE2);
	qglBindTexture(GL_TEXTURE_2D, Dest_tex);
	qglActiveTexture(GL_TEXTURE3);
	qglBindTexture(GL_TEXTURE_2D, Vpal_tex);
	qglActiveTexture(GL_TEXTURE4);
	qglBindTexture(GL_TEXTURE_3D, Near_tex);
	qglActiveTexture(GL_TEXTURE0);
	qglBindVertexArray(Scene_vao);
	Cur_state_valid = 0;
	N_verts = N_cmds = 0;
	In_view = 1;
}

void plat_gpu_draw(const plat_gpu_vertex *v, int n, const plat_gpu_material *m)
{
	draw_state st;
	int i, ntri;

	if (!In_view || n < 3 || n > 128)
		return;

	memset(&st, 0, sizeof(st));
	st.mode = m->mode;
	st.heat = (View_rgb && Infra_on) ? m->heat : 0;
	st.depth_write = (m->mode != PLAT_GPU_DARKEN);
	if (m->mode == PLAT_GPU_TEXTURED) {
		tex_info *ti;
		if (m->texture <= 0)
			return;
		st.texture = m->texture;
		st.lit = m->lit != 0;
		st.transparent = m->transparent != 0;
		st.clamp = m->clamp != 0;
		ti = tex_info_get((GLuint) m->texture);
		// a texture of the other form cannot be read here
		if (!ti || ti->rgba != View_rgb)
			return;
		st.smooth = (ti->flags & PLAT_GPU_TEX_SMOOTH) != 0;
		if (m->effects) {
			st.fx = 1;
			st.fx_row = m->fx_row;
		}
		if (st.lit && m->dim > 0)
			st.dim = m->dim > 15 ? 15 : m->dim;
		st.tint = st.lit && m->tint == 0x40;
	} else
		st.color = m->color & 255;
	if (m->mode == PLAT_GPU_DARKEN && st.color >= Fade_levels)
		return;

	if (m->mode == PLAT_GPU_DARKEN) {
		// The darkening reads the image under the polygon: draw the batch,
		// then copy the bounding box of the polygon to Dest_tex.
		// (a flat polygon of the 3d library: clipped, z more than 0)
		float x0 = v[0].x / v[0].z, x1 = x0, y0 = v[0].y / v[0].z, y1 = y0;
		int ix0, ix1, iy0, iy1;

		for (i = 1; i < n; i++) {
			float x = v[i].x / v[i].z, y = v[i].y / v[i].z;
			if (x < x0) x0 = x;
			if (x > x1) x1 = x;
			if (y < y0) y0 = y;
			if (y > y1) y1 = y;
		}
		ix0 = (int) (x0 / Canvas_w * View_w) - 1;
		ix1 = (int) (x1 / Canvas_w * View_w) + 2;
		// GL rows count from the bottom
		iy0 = View_h - ((int) (y1 / Canvas_h * View_h) + 2);
		iy1 = View_h - ((int) (y0 / Canvas_h * View_h) - 1);
		if (ix0 < 0) ix0 = 0;
		if (iy0 < 0) iy0 = 0;
		if (ix1 > View_w) ix1 = View_w;
		if (iy1 > View_h) iy1 = View_h;
		if (ix1 <= ix0 || iy1 <= iy0)
			return;
		flush();
		qglActiveTexture(GL_TEXTURE2);
		qglBindTexture(GL_TEXTURE_2D, Dest_tex);
		qglCopyTexSubImage2D(GL_TEXTURE_2D, 0, ix0, iy0, ix0, iy0, ix1 - ix0, iy1 - iy0);
		qglActiveTexture(GL_TEXTURE0);
	}

	if (st.fx) {
		// The effect colors read the image under the face (and one canvas
		// row below it): draw the batch, then copy the bounding box of the
		// face, or the whole view when a corner is near or behind the eye.
		int ix0 = 0, ix1 = View_w, iy0 = 0, iy1 = View_h, all = 0;

		for (i = 0; i < n; i++)
			if (v[i].z <= 1.0f / 16.0f)
				all = 1;
		if (!all) {
			float x0 = v[0].x / v[0].z, x1 = x0, y0 = v[0].y / v[0].z, y1 = y0;
			for (i = 1; i < n; i++) {
				float x = v[i].x / v[i].z, y = v[i].y / v[i].z;
				if (x < x0) x0 = x;
				if (x > x1) x1 = x;
				if (y < y0) y0 = y;
				if (y > y1) y1 = y;
			}
			ix0 = (int) (x0 / Canvas_w * View_w) - 1;
			ix1 = (int) (x1 / Canvas_w * View_w) + 2;
			iy0 = View_h - ((int) (y1 / Canvas_h * View_h) + 2) - 8;
			iy1 = View_h - ((int) (y0 / Canvas_h * View_h) - 1);
			if (ix0 < 0) ix0 = 0;
			if (iy0 < 0) iy0 = 0;
			if (ix1 > View_w) ix1 = View_w;
			if (iy1 > View_h) iy1 = View_h;
		}
		if (ix1 > ix0 && iy1 > iy0) {
			flush();
			qglActiveTexture(GL_TEXTURE2);
			qglBindTexture(GL_TEXTURE_2D, Dest_tex);
			qglCopyTexSubImage2D(GL_TEXTURE_2D, 0, ix0, iy0, ix0, iy0, ix1 - ix0, iy1 - iy0);
			qglActiveTexture(GL_TEXTURE0);
		}
	}

	ntri = n - 2;
	if (Stats_on) {
		Stats_polys++;
		Stats_view_polys++;
		Stats_tris += ntri;
	}
	if (N_verts + ntri * 3 > Max_verts) {
		int nm = Max_verts ? Max_verts * 2 : 16384;
		plat_gpu_vertex *nv;
		while (nm < N_verts + ntri * 3) nm *= 2;
		nv = (plat_gpu_vertex *) realloc(Verts, (size_t) nm * sizeof(*Verts));
		if (!nv)
			return;
		Verts = nv;
		Max_verts = nm;
	}
	if (N_cmds + 1 > Max_cmds) {
		int nm = Max_cmds ? Max_cmds * 2 : 1024;
		draw_cmd *nc = (draw_cmd *) realloc(Cmds, (size_t) nm * sizeof(*Cmds));
		if (!nc)
			return;
		Cmds = nc;
		Max_cmds = nm;
	}

	// a fan of triangles
	for (i = 0; i < ntri; i++) {
		Verts[N_verts++] = v[0];
		Verts[N_verts++] = v[i + 1];
		Verts[N_verts++] = v[i + 2];
	}
	if (N_cmds > 0 && !memcmp(&Cmds[N_cmds - 1].st, &st, sizeof(st)))
		Cmds[N_cmds - 1].count += ntri * 3;
	else {
		Cmds[N_cmds].st = st;
		Cmds[N_cmds].first = N_verts - ntri * 3;
		Cmds[N_cmds].count = ntri * 3;
		N_cmds++;
	}
}

void plat_gpu_clear_depth(void)
{
	if (!In_view)
		return;
	flush();
	qglDepthMask(GL_TRUE);
	Cur_state_valid = 0;
	qglClear(GL_DEPTH_BUFFER_BIT);
}

void plat_gpu_end_view(void)
{
	if (!In_view)
		return;
	flush();
	qglDepthMask(GL_TRUE);
	qglDisable(GL_DEPTH_TEST);
	qglDisable(GL_DEPTH_CLAMP);
	qglBindSampler(0, 0);
	qglBindVertexArray(0);
	qglBindFramebuffer(GL_FRAMEBUFFER, 0);
	In_view = 0;
	View_exists = 1;
	if (Stats_on) {
		qglEndQuery(GL_TIME_ELAPSED);
		Stats_query_next = (Stats_query_next + 1) & 3;
		uint64_t d = now_us() - Stats_view_begin_us;
		Stats_view_cpu_us += d;
		if (d > Stats_view_cpu_us_max)
			Stats_view_cpu_us_max = d;
		if (Stats_view_polys > Stats_view_polys_max)
			Stats_view_polys_max = Stats_view_polys;
		Stats_views++;
	}
}

void plat_gpu_set_view_dest(int x, int y, int w, int h)
{
	if (w <= 0 || h <= 0)
		x = y = w = h = 0;
	Dest_x = x;
	Dest_y = y;
	Dest_w = w;
	Dest_h = h;
	Dark_n = 0;
}

void plat_gpu_view_darken(int x0, int y0, int x1, int y1, int level)
{
	if (Dark_n >= MAX_DARK || x1 <= x0 || y1 <= y0 || level < 0 || level >= Fade_levels)
		return;
	Dark_rect[Dark_n * 4] = (float) x0;
	Dark_rect[Dark_n * 4 + 1] = (float) y0;
	Dark_rect[Dark_n * 4 + 2] = (float) x1;
	Dark_rect[Dark_n * 4 + 3] = (float) y1;
	Dark_level[Dark_n] = level;
	Dark_n++;
}

// ---------------------------------------------------------------------
// ASCENT: the upscaled interface pictures (plat_gpu_ui_*).
//
// The engine gives the 8-bit pixels of each picture that has an upscaled
// file, and at each draw of such a picture into the frame its place (a
// draw), and the darkenings of the 2d library over it. The draws make a
// map of the frame (Ui_ref, made again when the list changes): for each
// covered pixel the index that it has if nothing was drawn over the
// picture, the index before a darkening, and if a loaded file covers it.
// At each present the draws are checked against the frame: a draw of
// which less than 40% of the pixels (not counting color 255, the view)
// still have the expected index is removed, so that a screen drawn over
// the picture ends it. The layer (Ui_layer_tex, the size of the viewport
// of compose) has the upscaled files at their places, drawn with the
// linear filter and the mipmaps. The compose shader shows the layer where
// the frame pixel has the expected index (see ui_at in Comp_fs), with the
// curve from the palette of the pictures to the palette of the frame, so
// the fades and the flashes apply as to the rest of the frame.

#define UI_MAX_PICS		64
#define UI_MAX_LOADED	12		// files on the GPU at one time
#define UI_MAX_OPS		64

typedef struct ui_pic {
	int id, w, h;
	uint8_t *pix;
	char *path;
	char *overlay;				// ASCENT: the picture of the language over it (RGBA), or NULL
	GLuint tex;
	int tw, th;
	int state;					// 0: the file is not loaded, 1: loaded, -1: not valid
	uint64_t used;
} ui_pic;

typedef struct ui_op {
	int pic;					// index in Ui_pic, -1 for a darkening
	int sx, sy, sw, sh;
	int dx, dy, dw, dh;
	int key;					// the transparent index of the draw, or -1
	uint8_t table[256];			// the darkening
} ui_op;

static ui_pic Ui_pic[UI_MAX_PICS];
static int Ui_npic;
static ui_op Ui_op[UI_MAX_OPS], Ui_built[UI_MAX_OPS];
static int Ui_nop, Ui_nbuilt = -1;
static int Ui_map_w, Ui_map_h;
static uint8_t *Ui_exp, *Ui_org, *Ui_own, *Ui_ref_data;
static GLuint Ui_ref_tex, Ui_layer_fbo, Ui_layer_tex, Uplut_tex, Upal_tex;
static int Ui_layer_w, Ui_layer_h;
static int Ui_active;			// a loaded file covers pixels of the map
static int Ui_built_active;		// the same for the map that ui_build made
static int Ui_scale;			// the most pixels of a file for one frame pixel in the map
static const uint8_t *Ui_pal_src;
static uint8_t Uplut_src[768], Uplut_dst[768];
static int Uplut_valid;
static uint64_t Ui_clock;

static ui_pic *ui_find(int id)
{
	int i;

	for (i = 0; i < Ui_npic; i++)
		if (Ui_pic[i].id == id)
			return &Ui_pic[i];
	return NULL;
}

// ASCENT: the overlay of p changes to overlay (NULL: none); the texture
// is made again at the next draw.
static void ui_set_overlay(ui_pic *p, const char *overlay)
{
	char *c = NULL;

	if (!p->overlay == !overlay && (!overlay || !strcmp(p->overlay, overlay)))
		return;
	if (overlay) {
		c = (char *) malloc(strlen(overlay) + 1);
		if (!c)
			return;
		strcpy(c, overlay);
	}
	free(p->overlay);
	p->overlay = c;
	if (p->state == 1)
		qglDeleteTextures(1, &p->tex);
	p->tex = 0;
	p->state = 0;
}

void plat_gpu_ui_picture(int id, int w, int h, const uint8_t *pixels, const char *path, const char *overlay)
{
	ui_pic *p = ui_find(id);
	size_t n = (size_t) w * h;

	if (w <= 0 || h <= 0 || !pixels || !path)
		return;
	if (p) {
		ui_set_overlay(p, overlay);
		if (p->w == w && p->h == h && !memcmp(p->pix, pixels, n))
			return;
		if (p->w * p->h != w * h) {
			uint8_t *np = (uint8_t *) realloc(p->pix, n);
			if (!np)
				return;
			p->pix = np;
		}
	} else {
		char *pc;
		if (Ui_npic >= UI_MAX_PICS)
			return;
		pc = (char *) malloc(strlen(path) + 1);
		p = &Ui_pic[Ui_npic];
		memset(p, 0, sizeof(*p));
		p->pix = (uint8_t *) malloc(n);
		if (!p->pix || !pc) {
			free(p->pix);
			free(pc);
			p->pix = NULL;
			return;
		}
		strcpy(pc, path);
		p->path = pc;
		p->id = id;
		Ui_npic++;
		ui_set_overlay(p, overlay);
	}
	p->w = w;
	p->h = h;
	memcpy(p->pix, pixels, n);
	Ui_nbuilt = -1;
}

// Load the file of p into a texture (once). A file that is not an integer
// multiple of the picture size is not valid.
static void ui_load(ui_pic *p)
{
	uint8_t *rgba;
	int tw = 0, th = 0, loaded = 0, i, k;

	if (p->state != 0)
		return;
	p->state = -1;
	rgba = gputex_load_png(p->path, &tw, &th);
	if (!rgba) {
		fprintf(stderr, "GPU: cannot load %s\n", p->path);
		return;
	}
	k = tw / p->w;
	if (k < 1 || tw != k * p->w || th != k * p->h) {
		fprintf(stderr, "GPU: %s is %dx%d, not a multiple of %dx%d\n", p->path, tw, th, p->w, p->h);
		free(rgba);
		return;
	}
	// ASCENT: the picture of the language, drawn over the file with its alpha
	if (p->overlay) {
		int ow = 0, oh = 0;
		uint8_t *ov = gputex_load_png(p->overlay, &ow, &oh);
		if (!ov)
			fprintf(stderr, "GPU: cannot load %s\n", p->overlay);
		else if (ow != tw || oh != th)
			fprintf(stderr, "GPU: %s is %dx%d, not %dx%d\n", p->overlay, ow, oh, tw, th);
		else
			for (i = 0; i < tw * th; i++) {
				int a = ov[i * 4 + 3], c;
				for (c = 0; c < 3; c++)
					rgba[i * 4 + c] = (uint8_t) ((rgba[i * 4 + c] * (255 - a) + ov[i * 4 + c] * a + 127) / 255);
			}
		free(ov);
	}
	// Keep at most UI_MAX_LOADED files: free the one used last the longest
	// time ago that no draw of the list uses.
	for (i = 0; i < Ui_npic; i++)
		loaded += Ui_pic[i].state == 1;
	while (loaded >= UI_MAX_LOADED) {
		ui_pic *old = NULL;
		int j;
		for (i = 0; i < Ui_npic; i++) {
			ui_pic *q = &Ui_pic[i];
			int in_list = 0;
			if (q->state != 1)
				continue;
			for (j = 0; j < Ui_nop; j++)
				in_list |= Ui_op[j].pic == i;
			if (!in_list && (!old || q->used < old->used))
				old = q;
		}
		if (!old)
			break;
		qglDeleteTextures(1, &old->tex);
		old->tex = 0;
		old->state = 0;
		loaded--;
	}
	qglGenTextures(1, &p->tex);
	qglBindTexture(GL_TEXTURE_2D, p->tex);
	qglPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	qglTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, tw, th, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
	qglGenerateMipmap(GL_TEXTURE_2D);
	free(rgba);
	p->tw = tw;
	p->th = th;
	p->state = 1;
}

static void ui_add(const ui_op *op)
{
	if (Ui_nop >= UI_MAX_OPS) {
		memmove(Ui_op, Ui_op + 1, (UI_MAX_OPS - 1) * sizeof(ui_op));
		Ui_nop--;
	}
	Ui_op[Ui_nop++] = *op;
}

void plat_gpu_ui_draw(int id, int sx, int sy, int sw, int sh, int dx, int dy, int dw, int dh)
{
	plat_gpu_ui_draw_key(id, sx, sy, sw, sh, dx, dy, dw, dh, -1);
}

void plat_gpu_ui_draw_key(int id, int sx, int sy, int sw, int sh, int dx, int dy, int dw, int dh, int key)
{
	ui_pic *p;
	ui_op op;
	int i, n;

	if (!Gpu_ready || !Ui_prog || (p = ui_find(id)) == NULL)
		return;
	if (sw <= 0 || sh <= 0 || dw <= 0 || dh <= 0 || sx < 0 || sy < 0 || sx + sw > p->w || sy + sh > p->h)
		return;
	ui_load(p);
	p->used = ++Ui_clock;
	// The draw covers the older entries inside its rectangle.
	for (i = n = 0; i < Ui_nop; i++) {
		ui_op *o = &Ui_op[i];
		if (o->dx >= dx && o->dy >= dy && o->dx + o->dw <= dx + dw && o->dy + o->dh <= dy + dh)
			continue;
		Ui_op[n++] = *o;
	}
	Ui_nop = n;
	memset(&op, 0, sizeof(op));
	op.pic = (int) (p - Ui_pic);
	op.sx = sx; op.sy = sy; op.sw = sw; op.sh = sh;
	op.dx = dx; op.dy = dy; op.dw = dw; op.dh = dh;
	op.key = key;
	ui_add(&op);
}

void plat_gpu_ui_darken(int x0, int y0, int x1, int y1, const uint8_t *table)
{
	ui_op op;

	if (!Ui_nop || !table || x1 <= x0 || y1 <= y0)
		return;
	memset(&op, 0, sizeof(op));
	op.pic = -1;
	op.key = -1;
	op.dx = x0; op.dy = y0; op.dw = x1 - x0; op.dh = y1 - y0;
	memcpy(op.table, table, 256);
	ui_add(&op);
}

void plat_gpu_ui_clear(void)
{
	Ui_nop = 0;
}

void plat_gpu_ui_bind_palette(const uint8_t *pal6)
{
	Ui_pal_src = pal6;
}

// The map of the frame from the list (see the comment above).
static void ui_build(int w, int h)
{
	size_t n = (size_t) w * h;
	int i, x, y;

	if (w != Ui_map_w || h != Ui_map_h || !Ui_exp) {
		free(Ui_exp);
		free(Ui_org);
		free(Ui_own);
		free(Ui_ref_data);
		Ui_exp = (uint8_t *) malloc(n);
		Ui_org = (uint8_t *) malloc(n);
		Ui_own = (uint8_t *) malloc(n);
		Ui_ref_data = (uint8_t *) malloc(n * 4);
		if (!Ui_exp || !Ui_org || !Ui_own || !Ui_ref_data) {
			free(Ui_exp); free(Ui_org); free(Ui_own); free(Ui_ref_data);
			Ui_exp = Ui_org = Ui_own = Ui_ref_data = NULL;
			Ui_map_w = Ui_map_h = 0;
			Ui_nop = 0;
			return;
		}
		Ui_map_w = w;
		Ui_map_h = h;
		if (Ui_ref_tex)
			qglDeleteTextures(1, &Ui_ref_tex);
		Ui_ref_tex = make_texture(GL_RGBA8, w, h, GL_RGBA, GL_UNSIGNED_BYTE, NULL, 0);
	}
	memset(Ui_own, 0, n);
	for (i = 0; i < Ui_nop; i++) {
		const ui_op *o = &Ui_op[i];
		int x0 = o->dx > 0 ? o->dx : 0, y0 = o->dy > 0 ? o->dy : 0;
		int x1 = o->dx + o->dw < w ? o->dx + o->dw : w, y1 = o->dy + o->dh < h ? o->dy + o->dh : h;
		for (y = y0; y < y1; y++) {
			size_t row = (size_t) y * w;
			if (o->pic < 0) {
				for (x = x0; x < x1; x++)
					if (Ui_own[row + x])
						Ui_exp[row + x] = o->table[Ui_exp[row + x]];
			} else {
				const ui_pic *p = &Ui_pic[o->pic];
				const uint8_t *src = p->pix + (size_t) (o->sy + (y - o->dy) * o->sh / o->dh) * p->w;
				for (x = x0; x < x1; x++) {
					uint8_t c = src[o->sx + (x - o->dx) * o->sw / o->dw];
					if (c == o->key)
						continue;		// the draw left the pixel below
					Ui_exp[row + x] = Ui_org[row + x] = c;
					Ui_own[row + x] = (uint8_t) (i + 1);
				}
			}
		}
	}
	Ui_built_active = 0;
	Ui_scale = 1;
	for (i = 0; i < Ui_nop; i++)
		if (Ui_op[i].pic >= 0 && Ui_pic[Ui_op[i].pic].state == 1) {
			const ui_pic *p = &Ui_pic[Ui_op[i].pic];
			int k = (p->tw * Ui_op[i].sw / p->w + Ui_op[i].dw / 2) / Ui_op[i].dw;
			Ui_built_active = 1;
			if (k > Ui_scale)
				Ui_scale = k;
		}
	for (i = 0; i < (int) n; i++) {
		int o = Ui_own[i];
		Ui_ref_data[i * 4] = Ui_exp[i];
		Ui_ref_data[i * 4 + 1] = (o && Ui_pic[Ui_op[o - 1].pic].state == 1) ? 255 : 0;
		Ui_ref_data[i * 4 + 2] = Ui_org[i];
		Ui_ref_data[i * 4 + 3] = 255;
	}
	qglBindTexture(GL_TEXTURE_2D, Ui_ref_tex);
	qglPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	qglTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, Ui_ref_data);
}

// Before a present: the map of the list, and the check of the draws
// against the frame.
static int ui_changed(int w, int h)
{
	return w != Ui_map_w || h != Ui_map_h || Ui_nop != Ui_nbuilt || memcmp(Ui_op, Ui_built, Ui_nop * sizeof(ui_op));
}

static void ui_prepare(const uint8_t *pixels, int w, int h, int pitch)
{
	static int cnt[UI_MAX_OPS], match[UI_MAX_OPS], any[UI_MAX_OPS];
	int pass, i, x, y, n, blits, kept;

	Ui_active = 0;
	if (!Ui_prog)
		return;
	for (pass = 0; pass < 4 && Ui_nop > 0; pass++) {
		if (ui_changed(w, h)) {
			ui_build(w, h);
			memcpy(Ui_built, Ui_op, Ui_nop * sizeof(ui_op));
			Ui_nbuilt = Ui_nop;
			if (!Ui_exp)
				return;
		}
		memset(cnt, 0, sizeof(cnt));
		memset(match, 0, sizeof(match));
		memset(any, 0, sizeof(any));
		for (y = 0; y < h; y++) {
			const uint8_t *row = pixels + (size_t) y * pitch;
			const uint8_t *own = Ui_own + (size_t) y * w, *exp = Ui_exp + (size_t) y * w;
			for (x = 0; x < w; x++) {
				int o = own[x];
				if (!o)
					continue;
				any[o - 1]++;
				if (row[x] == 255)
					continue;
				cnt[o - 1]++;
				match[o - 1] += row[x] == exp[x];
			}
		}
		// a darkening goes with the draw before it
		for (i = n = blits = kept = 0; i < Ui_nop; i++) {
			if (Ui_op[i].pic >= 0) {
				kept = any[i] && match[i] * 10 >= cnt[i] * 4;
				if (!kept)
					continue;
				blits++;
			} else if (!kept)
				continue;
			Ui_op[n++] = Ui_op[i];
		}
		if (!blits)
			n = 0;
		if (n == Ui_nop)
			break;
		Ui_nop = n;
	}
	if (Ui_nop && ui_changed(w, h)) {
		ui_build(w, h);
		memcpy(Ui_built, Ui_op, Ui_nop * sizeof(ui_op));
		Ui_nbuilt = Ui_nop;
	}
	Ui_active = Ui_nop && Ui_exp ? Ui_built_active : 0;
}

// The curve from the palette of the pictures to the palette of the frame,
// and the palette of the pictures for the darkening (Comp_fs).
static void ui_update_plut(const uint8_t *pal6)
{
	const uint8_t *src = Ui_pal_src ? Ui_pal_src : pal6;
	float data[256 * 4];
	uint8_t rgba[256 * 4];
	int ch, c;

	if (!Uplut_tex) {
		Uplut_tex = make_texture(GL_RGBA16F, 256, 1, GL_RGBA, GL_FLOAT, NULL, 1);
		Upal_tex = make_texture(GL_RGBA8, 256, 1, GL_RGBA, GL_UNSIGNED_BYTE, NULL, 0);
	}
	if (Uplut_valid && !memcmp(Uplut_src, src, 768) && !memcmp(Uplut_dst, pal6, 768))
		return;
	memcpy(Uplut_src, src, 768);
	memcpy(Uplut_dst, pal6, 768);
	Uplut_valid = 1;
	for (ch = 0; ch < 3; ch++) {
		int sum[256], cnt[256];
		memset(sum, 0, sizeof(sum));
		memset(cnt, 0, sizeof(cnt));
		for (c = 0; c < 256; c++) {
			int x = expand6(Uplut_src[c * 3 + ch]);
			sum[x] += expand6(pal6[c * 3 + ch]);
			cnt[x]++;
		}
		make_curve(sum, cnt, data + ch, 4);
	}
	for (c = 0; c < 256; c++) {
		data[c * 4 + 3] = 1.0f;
		rgba[c * 4] = (uint8_t) expand6(Uplut_src[c * 3]);
		rgba[c * 4 + 1] = (uint8_t) expand6(Uplut_src[c * 3 + 1]);
		rgba[c * 4 + 2] = (uint8_t) expand6(Uplut_src[c * 3 + 2]);
		rgba[c * 4 + 3] = 255;
	}
	qglBindTexture(GL_TEXTURE_2D, Uplut_tex);
	qglTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, 256, 1, 0, GL_RGBA, GL_FLOAT, data);
	qglBindTexture(GL_TEXTURE_2D, Upal_tex);
	qglPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	qglTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 256, 1, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
}

// Draw the layer for a viewport of w by h pixels that shows a frame of
// fw by fh pixels. The framebuffer of the layer stays bound.
static int ui_layer(int fw, int fh, int w, int h)
{
	int i;

	if (!Ui_layer_fbo || w != Ui_layer_w || h != Ui_layer_h) {
		if (Ui_layer_fbo) qglDeleteFramebuffers(1, &Ui_layer_fbo);
		if (Ui_layer_tex) qglDeleteTextures(1, &Ui_layer_tex);
		Ui_layer_tex = make_texture(GL_RGBA8, w, h, GL_RGBA, GL_UNSIGNED_BYTE, NULL, 0);
		qglGenFramebuffers(1, &Ui_layer_fbo);
		qglBindFramebuffer(GL_FRAMEBUFFER, Ui_layer_fbo);
		qglFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, Ui_layer_tex, 0);
		if (qglCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
			qglDeleteFramebuffers(1, &Ui_layer_fbo);
			qglDeleteTextures(1, &Ui_layer_tex);
			Ui_layer_fbo = Ui_layer_tex = 0;
			Ui_layer_w = Ui_layer_h = 0;
			return 0;
		}
		Ui_layer_w = w;
		Ui_layer_h = h;
	}
	qglBindFramebuffer(GL_FRAMEBUFFER, Ui_layer_fbo);
	qglViewport(0, 0, w, h);
	qglDisable(GL_DEPTH_TEST);
	qglDisable(GL_BLEND);
	qglClearColor(0, 0, 0, 1);
	qglClear(GL_COLOR_BUFFER_BIT);
	qglUseProgram(Ui_prog);
	// A file of a draw with a transparent index has alpha 0 there (the
	// draws below show). The alpha of the layer stays 1.
	qglEnable(GL_BLEND);
	qglBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ZERO, GL_ONE);
	qglUniform1i(Uu.tex, 0);
	qglActiveTexture(GL_TEXTURE0);
	qglBindSampler(0, Sampler_rgb[1][1][1]);
	qglBindVertexArray(Comp_vao);
	for (i = 0; i < Ui_nop; i++) {
		const ui_op *o = &Ui_op[i];
		const ui_pic *p;
		if (o->pic < 0 || (p = &Ui_pic[o->pic])->state != 1)
			continue;
		qglUniform4f(Uu.rect, 2.0f * o->dx / fw - 1.0f, 1.0f - 2.0f * o->dy / fh,
			2.0f * (o->dx + o->dw) / fw - 1.0f, 1.0f - 2.0f * (o->dy + o->dh) / fh);
		qglUniform4f(Uu.uv, (float) o->sx / p->w, (float) o->sy / p->h,
			(float) (o->sx + o->sw) / p->w, (float) (o->sy + o->sh) / p->h);
		qglBindTexture(GL_TEXTURE_2D, p->tex);
		qglDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
	}
	qglDisable(GL_BLEND);
	qglBindVertexArray(0);
	qglBindSampler(0, 0);
	return 1;
}

static void ui_shutdown(void)
{
	int i;

	for (i = 0; i < Ui_npic; i++) {
		if (Ui_pic[i].tex)
			qglDeleteTextures(1, &Ui_pic[i].tex);
		free(Ui_pic[i].pix);
		free(Ui_pic[i].path);
	}
	memset(Ui_pic, 0, sizeof(Ui_pic));
	Ui_npic = Ui_nop = 0;
	Ui_nbuilt = -1;
	if (Ui_ref_tex) qglDeleteTextures(1, &Ui_ref_tex);
	if (Ui_layer_fbo) qglDeleteFramebuffers(1, &Ui_layer_fbo);
	if (Ui_layer_tex) qglDeleteTextures(1, &Ui_layer_tex);
	if (Uplut_tex) qglDeleteTextures(1, &Uplut_tex);
	if (Upal_tex) qglDeleteTextures(1, &Upal_tex);
	if (Ui_prog) qglDeleteProgram(Ui_prog);
	Ui_ref_tex = Ui_layer_fbo = Ui_layer_tex = Uplut_tex = Upal_tex = Ui_prog = 0;
	Ui_layer_w = Ui_layer_h = 0;
	free(Ui_exp); free(Ui_org); free(Ui_own); free(Ui_ref_data);
	Ui_exp = Ui_org = Ui_own = Ui_ref_data = NULL;
	Ui_map_w = Ui_map_h = 0;
	Uplut_valid = 0;
	Ui_active = 0;
}

// ---------------------------------------------------------------------
// The frame

static void upload_frame(const uint8_t *pixels, int w, int h, int pitch, const uint8_t *pal6)
{
	uint8_t pal[256 * 4];
	int i;

	if (!Frame_tex || w != Frame_tex_w || h != Frame_tex_h) {
		if (Frame_tex)
			qglDeleteTextures(1, &Frame_tex);
		Frame_tex = make_r8_texture(w, h, NULL);
		Frame_tex_w = w;
		Frame_tex_h = h;
	}
	qglBindTexture(GL_TEXTURE_2D, Frame_tex);
	qglPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	qglPixelStorei(GL_UNPACK_ROW_LENGTH, pitch);
	qglTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, w, h, GL_RED, GL_UNSIGNED_BYTE, pixels);
	qglPixelStorei(GL_UNPACK_ROW_LENGTH, 0);

	for (i = 0; i < 256; i++) {
		// Expand 6 bits to 8 bits, as the software presentation does.
		pal[i * 4] = (uint8_t) expand6(pal6[i * 3]);
		pal[i * 4 + 1] = (uint8_t) expand6(pal6[i * 3 + 1]);
		pal[i * 4 + 2] = (uint8_t) expand6(pal6[i * 3 + 2]);
		pal[i * 4 + 3] = 255;
	}
	qglBindTexture(GL_TEXTURE_2D, Pal_tex);
	qglTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 256, 1, GL_RGBA, GL_UNSIGNED_BYTE, pal);

	if (View_rgb && View_pal_set) {
		update_plut(pal6);
		update_light();
	}
}

// 1 if the frame shows the view image. The rectangle stays valid while at
// least one pixel in 8 of it has the transparent color. A screen that
// covers the view (the automap, the front end) ends it.
static int view_keyed(const uint8_t *pixels, int w, int h, int pitch)
{
	int x, y, count = 0;

	if (!View_exists || !View_fbo || Dest_w <= 0 || Dest_h <= 0)
		return 0;
	if (Dest_x < 0 || Dest_y < 0 || Dest_x + Dest_w > w || Dest_y + Dest_h > h) {
		Dest_w = Dest_h = 0;
		return 0;
	}
	for (y = Dest_y; y < Dest_y + Dest_h; y++) {
		const uint8_t *row = pixels + (size_t) y * pitch + Dest_x;
		for (x = 0; x < Dest_w; x++)
			count += (row[x] == 255);
	}
	if (count * 8 < Dest_w * Dest_h) {
		Dest_w = Dest_h = 0;
		return 0;
	}
	return 1;
}

// ASCENT: the sigma of the Gaussian scale filter (see Comp_fs) in source
// pixels: sharp with the shader Off and VGA, medium with Subtle. The
// sigma is at least SCALE_SIGMA_MIN output pixels, so that a small output
// (less than about 2 output pixels for one source pixel) does not alias.
// The view filter never uses more than SCALE_VSIGMA_MAX view pixels (7 by
// 7 pixels): a view of more than 2.5 view pixels for one output pixel
// (render scale 6 in a small window) is a little aliased.
#define SCALE_SIGMA_SHARP	0.15f
#define SCALE_SIGMA_MEDIUM	0.35f
#define SCALE_SIGMA_MIN		0.40f
#define SCALE_SIGMA_MAX		0.45f
#define SCALE_VSIGMA_MAX	1.00f

// ASCENT_SCALE_FILTER=nearest or bilinear: the scale filter for
// comparisons (a test aid). Default: the Gaussian filter.
static int scale_filter(void)
{
	static int f = -1;

	if (f < 0) {
		const char *e = SDL_getenv("ASCENT_SCALE_FILTER");
		f = 0;
		if (e && !strcmp(e, "nearest"))
			f = 1;
		else if (e && !strcmp(e, "bilinear"))
			f = 2;
	}
	return f;
}

// The sigma for a source pixel of k output pixels.
static float scale_sigma(float base, float k, float max)
{
	float s = base;

	if (k > 0 && SCALE_SIGMA_MIN / k > s)
		s = SCALE_SIGMA_MIN / k;
	return s < max ? s : max;
}

// Draw the frame into the viewport (x, y, w, h) of the bound framebuffer.
// medium: the medium Gaussian scale filter (Subtle), else the sharp one.
static void compose(int key, int fw, int fh, int x, int y, int w, int h, int medium)
{
	int vrgb = key && View_rgb;
	float base = medium ? SCALE_SIGMA_MEDIUM : SCALE_SIGMA_SHARP;
	float kx = (float) w / (fw > 0 ? fw : 1), ky = (float) h / (fh > 0 ? fh : 1);
	float vsx = base, vsy = base, vs;
	int vrad;

	// the view: output pixels for one view pixel
	if (key && View_w > 0 && View_h > 0) {
		vsx = scale_sigma(base, kx * Dest_w / View_w, SCALE_VSIGMA_MAX);
		vsy = scale_sigma(base, ky * Dest_h / View_h, SCALE_VSIGMA_MAX);
	}
	vs = vsx > vsy ? vsx : vsy;
	vrad = (int) ceilf(2.5f * vs);
	if (vrad < 1) vrad = 1;
	if (vrad > 3) vrad = 3;

	qglViewport(x, y, w, h);
	qglDisable(GL_DEPTH_TEST);
	qglDisable(GL_BLEND);
	qglUseProgram(Comp_prog);
	qglUniform1i(Cu.frame, 0);
	qglUniform1i(Cu.pal, 1);
	qglUniform1i(Cu.view, 2);
	qglUniform1i(Cu.fade, 3);
	qglUniform1i(Cu.plut, 4);
	qglUniform1i(Cu.nearc, 5);
	qglUniform1i(Cu.vpal, 6);
	qglUniform1i(Cu.levels, Fade_levels);
	qglUniform1fv(Cu.rowk, 64, Row_k);
	qglUniform4f(Cu.dst, (float) x, (float) y, (float) w, (float) h);
	qglUniform2f(Cu.fsize, (float) fw, (float) fh);
	qglUniform4f(Cu.vrect, (float) Dest_x, (float) Dest_y, (float) Dest_w, (float) Dest_h);
	qglUniform2f(Cu.vsize, (float) (View_w ? View_w : 1), (float) (View_h ? View_h : 1));
	qglUniform1i(Cu.key, key);
	qglUniform1i(Cu.vrgb, vrgb);
	qglUniform1i(Cu.ndark, key ? Dark_n : 0);
	if (Dark_n) {
		qglUniform4fv(Cu.drect, Dark_n, Dark_rect);
		qglUniform1iv(Cu.dlevel, Dark_n, Dark_level);
	}
	qglUniform1i(Cu.filter, scale_filter());
	qglUniform1f(Cu.sigma, scale_sigma(base, ky < kx ? ky : kx, SCALE_SIGMA_MAX));
	qglUniform2f(Cu.vsigma, vsx, vsy);
	qglUniform1i(Cu.vrad, vrad);
	qglActiveTexture(GL_TEXTURE0);
	qglBindTexture(GL_TEXTURE_2D, Frame_tex);
	qglActiveTexture(GL_TEXTURE1);
	qglBindTexture(GL_TEXTURE_2D, Pal_tex);
	qglActiveTexture(GL_TEXTURE2);
	qglBindTexture(GL_TEXTURE_2D, key ? View_tex : Frame_tex);
	qglActiveTexture(GL_TEXTURE3);
	qglBindTexture(GL_TEXTURE_2D, Fade_tex);
	qglActiveTexture(GL_TEXTURE4);
	qglBindTexture(GL_TEXTURE_2D, Plut_tex);
	qglActiveTexture(GL_TEXTURE5);
	qglBindTexture(GL_TEXTURE_3D, Near_tex);
	qglActiveTexture(GL_TEXTURE6);
	qglBindTexture(GL_TEXTURE_2D, Vpal_tex);
	// ASCENT: the upscaled interface pictures (ui_layer drew the layer)
	qglUniform1i(Cu.ui, Ui_active && Ui_layer_w == w && Ui_layer_h == h);
	qglUniform1i(Cu.ref, 7);
	qglUniform1i(Cu.hd, 8);
	qglUniform1i(Cu.uplut, 9);
	qglUniform1i(Cu.upal, 10);
	qglActiveTexture(GL_TEXTURE7);
	qglBindTexture(GL_TEXTURE_2D, Ui_active ? Ui_ref_tex : Frame_tex);
	qglActiveTexture(GL_TEXTURE8);
	qglBindTexture(GL_TEXTURE_2D, Ui_active ? Ui_layer_tex : Frame_tex);
	qglActiveTexture(GL_TEXTURE9);
	qglBindTexture(GL_TEXTURE_2D, Ui_active ? Uplut_tex : Plut_tex);
	qglActiveTexture(GL_TEXTURE10);
	qglBindTexture(GL_TEXTURE_2D, Ui_active ? Upal_tex : Pal_tex);
	qglActiveTexture(GL_TEXTURE0);
	qglBindVertexArray(Comp_vao);
	qglDrawArrays(GL_TRIANGLES, 0, 3);
	qglBindVertexArray(0);
}

static GLuint make_target(GLuint *tex, int w, int h)
{
	GLuint fbo;

	*tex = make_texture(GL_RGBA8, w, h, GL_RGBA, GL_UNSIGNED_BYTE, NULL, 1);
	qglGenFramebuffers(1, &fbo);
	qglBindFramebuffer(GL_FRAMEBUFFER, fbo);
	qglFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, *tex, 0);
	if (qglCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
		qglDeleteFramebuffers(1, &fbo);
		qglDeleteTextures(1, tex);
		*tex = 0;
		return 0;
	}
	return fbo;
}

static int make_post_targets(int w, int h)
{
	int i;

	if (Post_fbo && w == Post_w && h == Post_h)
		return 0;
	delete_post_targets();
	Post_fbo = make_target(&Post_tex, w, h);
	Bloom_w = (w + 3) / 4;
	Bloom_h = (h + 3) / 4;
	for (i = 0; i < 2; i++)
		Bloom_fbo[i] = make_target(&Bloom_tex[i], Bloom_w, Bloom_h);
	if (!Post_fbo || !Bloom_fbo[0] || !Bloom_fbo[1]) {
		delete_post_targets();
		return -1;
	}
	Post_w = w;
	Post_h = h;
	return 0;
}

static void draw_full(GLuint prog)
{
	(void) prog;
	qglBindVertexArray(Comp_vao);
	qglDrawArrays(GL_TRIANGLES, 0, 3);
	qglBindVertexArray(0);
}

// The screen shaders. The parameters are given for the output size: p is
// the number of output pixels for one frame pixel (3 for 1920x1440 and a
// 640x480 frame).
//
// The scale to the output comes first (compose, the Gaussian filter of
// Comp_fs: sharp for Off and VGA, medium for Subtle), then the shader
// works on the scaled image at the output size.
//
// Subtle: the medium Gaussian scale filter, and a bloom: the
// part of the color above 0.55 (knee 0.20) at a quarter size, a Gaussian of
// sigma p quarter pixels (about 4 frame pixels at any size), added with
// the weight 0.45.
//
// VGA: the beam (a horizontal Gaussian of sigma 0.25 frame pixels: the
// edges of a pixel are soft from left to right but the lines stay sharp),
// gamma 1.12 (a 2.5 CRT shown on a 2.2 display, made milder), lines with a
// dip of 7% at their edges (the beam spot is round: 480 lines of a VGA
// monitor touch each other, so no dark scanline gaps), a shadow mask of 3
// output pixel dot triads with a depth of 9% (the dot pitch of a 0.28 mm
// monitor is finer than a frame pixel), a gain that brings white back to 1,
// and a phosphor glow: the whole image at a quarter size, a Gaussian of
// sigma 1.5 p quarter pixels, weight 0.07. The line structure and the mask
// fade out when p is less than 2.5 (no moire at small sizes).
static void post_process(int shader, GLuint target_fbo, int fw, int fh, int x, int y, int w, int h)
{
	float p = (float) h / (fh > 0 ? fh : 1);
	float detail = (p - 1.5f) / 1.0f;
	float sigma, thresh, knee, bloom_k, scan, mask, gain;

	if (detail < 0) detail = 0;
	if (detail > 1) detail = 1;
	if (shader == PLAT_GPU_SHADER_SUBTLE) {
		thresh = 0.55f;
		knee = 0.20f;
		sigma = 1.0f * p;
		bloom_k = 0.45f;
	} else {
		thresh = 0.0f;
		knee = 0.0f;
		sigma = 1.5f * p;
		bloom_k = 0.07f;
	}
	if (sigma < 1.0f)
		sigma = 1.0f;

	// the quarter size copy
	qglBindFramebuffer(GL_FRAMEBUFFER, Bloom_fbo[0]);
	qglViewport(0, 0, Bloom_w, Bloom_h);
	qglUseProgram(Down_prog);
	qglUniform1i(Du.src, 0);
	qglUniform2f(Du.texel, 1.0f / Post_w, 1.0f / Post_h);
	qglUniform1f(Du.thresh, thresh);
	qglUniform1f(Du.knee, knee);
	qglActiveTexture(GL_TEXTURE0);
	qglBindTexture(GL_TEXTURE_2D, Post_tex);
	draw_full(Down_prog);

	// the blur
	qglUseProgram(Blur_prog);
	qglUniform1i(Bu.src, 0);
	qglUniform1f(Bu.sigma, sigma);
	qglBindFramebuffer(GL_FRAMEBUFFER, Bloom_fbo[1]);
	qglUniform2f(Bu.dir, 1, 0);
	qglBindTexture(GL_TEXTURE_2D, Bloom_tex[0]);
	draw_full(Blur_prog);
	qglBindFramebuffer(GL_FRAMEBUFFER, Bloom_fbo[0]);
	qglUniform2f(Bu.dir, 0, 1);
	qglBindTexture(GL_TEXTURE_2D, Bloom_tex[1]);
	draw_full(Blur_prog);

	// the output
	scan = 0.07f * detail;
	mask = 0.09f * detail;
	// the average of the line dip (4 d^2 over a line is 1/3) and of the mask
	gain = 1.0f / ((1.0f - scan / 3.0f) * (1.0f - mask * 2.0f / 3.0f));
	qglBindFramebuffer(GL_FRAMEBUFFER, target_fbo);
	qglViewport(x, y, w, h);
	qglUseProgram(Final_prog);
	qglUniform1i(Fu.src, 0);
	qglUniform1i(Fu.bloom, 1);
	qglUniform4f(Fu.dst, (float) x, (float) y, (float) w, (float) h);
	qglUniform2f(Fu.fsize, (float) fw, (float) fh);
	qglUniform1i(Fu.mode, shader);
	qglUniform1f(Fu.bloom_k, bloom_k);
	qglUniform1f(Fu.beam, p * 0.25f > 0.35f ? p * 0.25f : 0.35f);
	qglUniform1f(Fu.gamma, 1.12f);
	qglUniform1f(Fu.scan, scan);
	qglUniform1f(Fu.mask, mask);
	qglUniform1f(Fu.gain, gain);
	qglActiveTexture(GL_TEXTURE0);
	qglBindTexture(GL_TEXTURE_2D, Post_tex);
	qglActiveTexture(GL_TEXTURE1);
	qglBindTexture(GL_TEXTURE_2D, Bloom_tex[0]);
	qglActiveTexture(GL_TEXTURE0);
	draw_full(Final_prog);
}

// The composed image with the shader into the viewport (x, y, w, h) of
// target_fbo.
static void output(GLuint target_fbo, int key, int fw, int fh, int x, int y, int w, int h)
{
	int shader = cur_shader();

	// ASCENT: the layer of the upscaled interface pictures
	if (Ui_active && !ui_layer(fw, fh, w, h))
		Ui_active = 0;
	if (shader != PLAT_GPU_SHADER_OFF && make_post_targets(w, h) == 0) {
		qglBindFramebuffer(GL_FRAMEBUFFER, Post_fbo);
		compose(key, fw, fh, 0, 0, w, h, shader == PLAT_GPU_SHADER_SUBTLE);
		post_process(shader, target_fbo, fw, fh, x, y, w, h);
		return;
	}
	qglBindFramebuffer(GL_FRAMEBUFFER, target_fbo);
	compose(key, fw, fh, x, y, w, h, 0);
}

void gpu_present(const uint8_t *pixels, int w, int h, int pitch, const uint8_t *pal6)
{
	int pw = 0, ph = 0, ww = 0, wh = 0, key;
	float s, dw, dh, dx, dy;

	if (!Gpu_ready || w <= 0 || h <= 0)
		return;
	if (In_view)
		plat_gpu_end_view();

	upload_frame(pixels, w, h, pitch, pal6);
	key = view_keyed(pixels, w, h, pitch);
	ui_prepare(pixels, w, h, pitch);		// ASCENT: the upscaled interface pictures
	if (Ui_active)
		ui_update_plut(pal6);

	SDL_GetWindowSizeInPixels(Gpu_window, &pw, &ph);
	SDL_GetWindowSize(Gpu_window, &ww, &wh);
	if (pw <= 0 || ph <= 0)
		return;

	// letterbox: the largest size with the aspect of the frame
	s = (float) pw / w;
	if ((float) ph / h < s)
		s = (float) ph / h;
	dw = w * s;
	dh = h * s;
	dx = (pw - dw) / 2;
	dy = (ph - dh) / 2;

	if (Stats_on) {
		int q = Stats_out_next;
		GLuint64 ns = 0;
		if (Stats_out_used[q]) {
			qglGetQueryObjectui64v(Stats_out_query[q], GL_QUERY_RESULT, &ns);
			Stats_out_ns += ns;
			Stats_out_samples++;
		}
		qglBeginQuery(GL_TIME_ELAPSED, Stats_out_query[q]);
		Stats_out_used[q] = 1;
		Stats_out_w = pw;
		Stats_out_h = ph;
	}
	qglBindFramebuffer(GL_FRAMEBUFFER, 0);
	qglViewport(0, 0, pw, ph);
	qglClearColor(0, 0, 0, 1);
	qglClear(GL_COLOR_BUFFER_BIT);
	output(0, key, w, h, (int) dx, (int) dy, (int) dw, (int) dh);
	if (Stats_on) {
		qglEndQuery(GL_TIME_ELAPSED);
		Stats_out_next = (Stats_out_next + 1) & 3;
	}

	// the rectangle in window coordinates, for the mouse
	if (ww > 0 && wh > 0) {
		float kx = (float) ww / pw, ky = (float) wh / ph;
		Rect_x = (int) dx * kx;
		Rect_y = (int) dy * ky;
		Rect_w = (int) dw * kx;
		Rect_h = (int) dh * ky;
	}
	Rect_fw = w;
	Rect_fh = h;

	SDL_GL_SwapWindow(Gpu_window);

	if (Stats_on) {
		uint64_t t = now_us();
		if (Stats_last_present_us) {
			uint64_t d = t - Stats_last_present_us;
			Stats_frame_us_sum += d;
			Stats_frame_samples++;
			if (d > Stats_frame_us_max)
				Stats_frame_us_max = d;
			if (d > 25000) {
				Stats_slow25++;
				Stats_slow_last = Stats_frames;
			}
			if (d > 50000)
				Stats_slow50++;
		}
		Stats_last_present_us = t;
		Stats_frames++;
	}
}

void gpu_frame_rect(float *x, float *y, float *w, float *h, int *frame_w, int *frame_h)
{
	*x = Rect_x;
	*y = Rect_y;
	*w = Rect_w;
	*h = Rect_h;
	*frame_w = Rect_fw;
	*frame_h = Rect_fh;
}

uint8_t *plat_gpu_screenshot(const uint8_t *pixels, int w, int h, int pitch, const uint8_t *pal6, int *out_w, int *out_h)
{
	uint8_t *rgba, *rgb;
	int scale, sw, sh, y, x, key, ox, oy, ow, oh;
	const char *e;

	if (!Gpu_ready || In_view || w <= 0 || h <= 0)
		return NULL;
	upload_frame(pixels, w, h, pitch, pal6);
	key = view_keyed(pixels, w, h, pitch);
	// ASCENT: also a frame with upscaled interface pictures, at their scale
	ui_prepare(pixels, w, h, pitch);
	if (Ui_active)
		ui_update_plut(pal6);
	if (!key && !Ui_active)
		return NULL;

	scale = key ? (View_w + Dest_w / 2) / Dest_w : Ui_scale;
	if (scale < 1)
		scale = 1;
	if (scale > 8)
		scale = 8;
	// ASCENT_SHOT_SCALE=n: the screenshot is n times the frame size (for
	// the tests of the screen shaders at a fixed output size)
	e = SDL_getenv("ASCENT_SHOT_SCALE");
	if (e && atoi(e) >= 1 && atoi(e) <= 8)
		scale = atoi(e);
	sw = w * scale;
	sh = h * scale;
	ox = oy = 0;
	ow = sw;
	oh = sh;
	// ASCENT_SHOT_SIZE=WxH: the screenshot is a window of W by H pixels,
	// with the frame letterboxed as gpu_present shows it (for the tests of
	// the scale filter at sizes that are not a multiple of the frame)
	e = SDL_getenv("ASCENT_SHOT_SIZE");
	if (e && sscanf(e, "%dx%d", &x, &y) == 2 && x >= 64 && y >= 64 && x <= 8192 && y <= 8192) {
		float s = (float) x / w;
		if ((float) y / h < s)
			s = (float) y / h;
		sw = x;
		sh = y;
		ow = (int) (w * s);
		oh = (int) (h * s);
		ox = (int) ((sw - w * s) / 2);
		oy = (int) ((sh - h * s) / 2);
	}
	if (!Shot_fbo || sw != Shot_w || sh != Shot_h) {
		if (Shot_fbo) qglDeleteFramebuffers(1, &Shot_fbo);
		if (Shot_tex) qglDeleteTextures(1, &Shot_tex);
		Shot_fbo = make_target(&Shot_tex, sw, sh);
		if (!Shot_fbo) {
			qglBindFramebuffer(GL_FRAMEBUFFER, 0);
			return NULL;
		}
		Shot_w = sw;
		Shot_h = sh;
	}
	qglBindFramebuffer(GL_FRAMEBUFFER, Shot_fbo);
	qglViewport(0, 0, sw, sh);
	qglClearColor(0, 0, 0, 1);
	qglClear(GL_COLOR_BUFFER_BIT);
	output(Shot_fbo, key, w, h, ox, oy, ow, oh);

	rgba = (uint8_t *) malloc((size_t) sw * sh * 4);
	rgb = (uint8_t *) malloc((size_t) sw * sh * 3);
	if (!rgba || !rgb) {
		free(rgba);
		free(rgb);
		qglBindFramebuffer(GL_FRAMEBUFFER, 0);
		return NULL;
	}
	qglBindFramebuffer(GL_FRAMEBUFFER, Shot_fbo);
	qglPixelStorei(GL_PACK_ALIGNMENT, 1);
	qglReadPixels(0, 0, sw, sh, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
	qglBindFramebuffer(GL_FRAMEBUFFER, 0);
	// GL rows count from the bottom
	for (y = 0; y < sh; y++) {
		const uint8_t *s = rgba + (size_t) (sh - 1 - y) * sw * 4;
		uint8_t *d = rgb + (size_t) y * sw * 3;
		for (x = 0; x < sw; x++) {
			d[x * 3] = s[x * 4];
			d[x * 3 + 1] = s[x * 4 + 1];
			d[x * 3 + 2] = s[x * 4 + 2];
		}
	}
	free(rgba);
	*out_w = sw;
	*out_h = sh;
	return rgb;
}
