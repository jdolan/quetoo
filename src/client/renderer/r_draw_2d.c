/*
 * Copyright(c) 1997-2001 id Software, Inc.
 * Copyright(c) 2002 The Quakeforge Project.
 * Copyright(c) 2006 Quetoo.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#include "r_local.h"

/**
 * @brief The font type.
 */
typedef struct {
	char name[MAX_QPATH];

	r_image_t *image;

	r_pixel_t char_width;
	r_pixel_t char_height;
} r_font_t;

#define MAX_DRAW_FONTS 3

/**
 * @brief glDrawArrays commands.
 */
typedef struct {
	GLenum mode;
	GLuint texture;

	GLint first_vertex;
	GLsizei num_vertexes;
} r_draw_2d_arrays_t;

#define MAX_DRAW_2D_ARRAYS 4096

/**
 * @brief 2D vertex struct.
 */
typedef struct {
	vec2s_t position;
	vec2_t diffusemap;
	color32_t color;
} r_draw_2d_vertex_t;

#define MAX_DRAW_2D_VERTEXES (MAX_DRAW_2D_ARRAYS * 6)

/**
 * @brief 2D draw struct.
 */
static struct {

	// registered fonts
	int32_t num_fonts;
	r_font_t fonts[MAX_DRAW_FONTS];

	// active font
	r_font_t *font;

	// the null texture
	r_image_t *null_texture;

	// accumulated draw arrays to draw for this frame
	r_draw_2d_arrays_t draw_arrays[MAX_DRAW_2D_ARRAYS];
	int32_t num_draw_arrays;

	// accumulated vertexes backing the draw arrays
	r_draw_2d_vertex_t vertexes[MAX_DRAW_2D_VERTEXES];
	int32_t num_vertexes;

	// the vertex array
	GLuint vertex_array;

	// the vertex buffer
	GLuint vertex_buffer;

} r_draw_2d;

/**
 * @brief The draw program.
 */
static struct {
	GLuint name;

	GLuint uniforms_block;

	GLint in_position;
	GLint in_diffusemap;
	GLint in_color;

	GLint texture_diffusemap;
} r_draw_2d_program;

/**
 * @brief
 */
static void R_AddDraw2DArrays(const r_draw_2d_arrays_t *draw) {

	if (r_draw_2d.num_draw_arrays == MAX_DRAW_2D_ARRAYS) {
		Com_Warn("MAX_DRAW_2D_ARRAYS\n");
		return;
	}

	if (draw->num_vertexes == 0) {
		return;
	}

	if (r_draw_2d.num_draw_arrays && (draw->mode == GL_LINES || draw->mode == GL_TRIANGLES || draw->mode == GL_POINTS)) {
		r_draw_2d_arrays_t *last_draw = &r_draw_2d.draw_arrays[r_draw_2d.num_draw_arrays - 1];

		if (last_draw->mode == draw->mode && last_draw->texture == draw->texture) {
			last_draw->num_vertexes += draw->num_vertexes;
			return;
		}
	}

	r_draw_2d.draw_arrays[r_draw_2d.num_draw_arrays] = *draw;
	r_draw_2d.num_draw_arrays++;
}

/**
 * @brief Emits GL_TRIANGLES data from the specified quad.
 */
static void R_EmitDrawVertexes2D_Quad(const r_draw_2d_vertex_t *quad) {

	if (r_draw_2d.num_vertexes + 6 > MAX_DRAW_2D_VERTEXES) {
		Com_Warn("MAX_DRAW_2D_VERTEXES\n");
		return;
	}

	r_draw_2d.vertexes[r_draw_2d.num_vertexes++] = quad[0];
	r_draw_2d.vertexes[r_draw_2d.num_vertexes++] = quad[1];
	r_draw_2d.vertexes[r_draw_2d.num_vertexes++] = quad[2];

	r_draw_2d.vertexes[r_draw_2d.num_vertexes++] = quad[0];
	r_draw_2d.vertexes[r_draw_2d.num_vertexes++] = quad[2];
	r_draw_2d.vertexes[r_draw_2d.num_vertexes++] = quad[3];
}

/**
 * @brief
 */
static void R_Draw2DChar_(r_pixel_t x, r_pixel_t y, char c, const color_t color) {

	if (isspace(c) && c != 0x0b) {
		return;
	}

	const uint32_t row = (uint32_t) c >> 4;
	const uint32_t col = (uint32_t) c & 15;

	const float s0 = col * 0.0625;
	const float t0 = row * 0.1250;
	const float s1 = (col + 1) * 0.0625;
	const float t1 = (row + 1) * 0.1250;

	const r_pixel_t cw = r_draw_2d.font->char_width;
	const r_pixel_t ch = r_draw_2d.font->char_height;

	r_draw_2d_vertex_t quad[4];

	quad[0].position = Vec2s(x, y);
	quad[1].position = Vec2s(x + cw, y);
	quad[2].position = Vec2s(x + cw, y + ch);
	quad[3].position = Vec2s(x, y + ch);

	quad[0].diffusemap = Vec2(s0, t0);
	quad[1].diffusemap = Vec2(s1, t0);
	quad[2].diffusemap = Vec2(s1, t1);
	quad[3].diffusemap = Vec2(s0, t1);

	quad[0].color = Color_Color32(color);
	quad[1].color = Color_Color32(color);
	quad[2].color = Color_Color32(color);
	quad[3].color = Color_Color32(color);

	R_EmitDrawVertexes2D_Quad(quad);

	r_stats.count_draw_chars++;
}

/**
 * @brief
 */
void R_Draw2DChar(r_pixel_t x, r_pixel_t y, char c, const color_t color) {

	if (isspace(c) && c != 0x0b) {
		return;
	}

	r_draw_2d_arrays_t draw = {
		.mode = GL_TRIANGLES,
		.texture = r_draw_2d.font->image->texnum,
		.first_vertex = r_draw_2d.num_vertexes,
		.num_vertexes = 6
	};

	R_Draw2DChar_(x, y, c, color);
	R_AddDraw2DArrays(&draw);
}

/**
 * @brief Return the width of the specified string in pixels. This will vary based
 * on the currently bound font. Color escapes are omitted.
 */
r_pixel_t R_StringWidth(const char *s) {
	return StrColorLen(s) * r_draw_2d.font->char_width;
}

/**
 * @brief
 */
size_t R_Draw2DString(r_pixel_t x, r_pixel_t y, const char *s, const color_t color) {
	return R_Draw2DSizedString(x, y, s, UINT16_MAX, UINT16_MAX, color);
}

/**
 * @brief
 */
size_t R_Draw2DBytes(r_pixel_t x, r_pixel_t y, const char *s, size_t size, const color_t color) {
	return R_Draw2DSizedString(x, y, s, size, size, color);
}

/**
 * @brief Draws at most len chars or size bytes of the specified string. Color escape
 * sequences are not visible chars. Returns the number of chars drawn.
 */
size_t R_Draw2DSizedString(r_pixel_t x, r_pixel_t y, const char *s, size_t len, size_t size, const color_t color) {
	size_t i, j;

	r_draw_2d_arrays_t draw = {
		.mode = GL_TRIANGLES,
		.texture = r_draw_2d.font->image->texnum,
		.first_vertex = r_draw_2d.num_vertexes
	};

	color_t c = color;

	i = j = 0;
	while (*s && i < len && j < size) {

		if (StrIsColor(s)) {
			c = ColorEsc(StrColor(s));
			j += 2;
			s += 2;
			continue;
		}

		R_Draw2DChar_(x, y, *s, c);
		x += r_draw_2d.font->char_width; // next char position in line

		i++;
		j++;
		s++;
	}

	draw.num_vertexes = r_draw_2d.num_vertexes - draw.first_vertex;
	R_AddDraw2DArrays(&draw);

	return i;
}


/**
 * @brief Binds the specified font, returning the character width and height.
 */
void R_BindFont(const char *name, r_pixel_t *cw, r_pixel_t *ch) {

	if (name == NULL) {
		name = "medium";
	}

	int32_t i;
	for (i = 0; i < r_draw_2d.num_fonts; i++) {
		if (!g_strcmp0(name, r_draw_2d.fonts[i].name)) {
			if (r_context.window_scale > 1.f && i < r_draw_2d.num_fonts - 1) {
				r_draw_2d.font = &r_draw_2d.fonts[i + 1];
			} else {
				r_draw_2d.font = &r_draw_2d.fonts[i];
			}
			break;
		}
	}

	if (i == r_draw_2d.num_fonts) {
		Com_Warn("Unknown font: %s\n", name);
	}

	if (cw) {
		*cw = r_draw_2d.font->char_width;
	}

	if (ch) {
		*ch = r_draw_2d.font->char_height;
	}
}

/**
 * @brief
 */
void R_Draw2DImage(r_pixel_t x, r_pixel_t y, r_pixel_t w, r_pixel_t h, const r_image_t *image, const color_t color) {

	r_draw_2d_arrays_t draw = {
		.mode = GL_TRIANGLES,
		.texture = image->texnum,
		.first_vertex = r_draw_2d.num_vertexes,
		.num_vertexes = 6
	};

	r_draw_2d_vertex_t quad[4];

	quad[0].position = Vec2s(x, y);
	quad[1].position = Vec2s(x + w, y);
	quad[2].position = Vec2s(x + w, y + h);
	quad[3].position = Vec2s(x, y + h);

	quad[0].diffusemap = Vec2(0, 0);
	quad[1].diffusemap = Vec2(1, 0);
	quad[2].diffusemap = Vec2(1, 1);
	quad[3].diffusemap = Vec2(0, 1);

	quad[0].color = Color_Color32(color);
	quad[1].color = Color_Color32(color);
	quad[2].color = Color_Color32(color);
	quad[3].color = Color_Color32(color);

	R_EmitDrawVertexes2D_Quad(quad);
	R_AddDraw2DArrays(&draw);

	r_stats.count_draw_images++;
}

/**
 * @brief The color can be specified as an index into the palette with positive alpha
 * value for a, or as an RGBA value (32 bit) by passing -1.0 for a.
 */
void R_Draw2DFill(r_pixel_t x, r_pixel_t y, r_pixel_t w, r_pixel_t h, const color_t color) {

	r_draw_2d_arrays_t draw = {
		.mode = GL_TRIANGLES,
		.texture = r_draw_2d.null_texture->texnum,
		.first_vertex = r_draw_2d.num_vertexes,
		.num_vertexes = 6
	};

	r_draw_2d_vertex_t quad[4];

	quad[0].position = Vec2s(x, y);
	quad[1].position = Vec2s(x + w, y);
	quad[2].position = Vec2s(x + w, y + h);
	quad[3].position = Vec2s(x, y + h);

	quad[0].color = Color_Color32(color);
	quad[1].color = Color_Color32(color);
	quad[2].color = Color_Color32(color);
	quad[3].color = Color_Color32(color);

	R_EmitDrawVertexes2D_Quad(quad);
	R_AddDraw2DArrays(&draw);

	r_stats.count_draw_fills++;
}

/**
 * @brief
 */
void R_Draw2DLines(const r_pixel_t *points, size_t count, const color_t color) {

	r_draw_2d_arrays_t draw = {
		.mode = GL_LINE_STRIP,
		.texture = r_draw_2d.null_texture->texnum,
		.first_vertex = r_draw_2d.num_vertexes,
		.num_vertexes = (GLsizei) count
	};

	r_draw_2d_vertex_t *out = r_draw_2d.vertexes + r_draw_2d.num_vertexes;
	
	const r_pixel_t *in = points;
	for (size_t i = 0; i < count; i++, in += 2, out++) {

		out->position.x = *(in + 0);
		out->position.y = *(in + 1);

		out->color = Color_Color32(color);
	}

	r_draw_2d.num_vertexes += count;

	R_AddDraw2DArrays(&draw);

	r_stats.count_draw_lines += count >> 1;
}

/**
 * @brief Draw all 2D geometry accumulated for the current frame.
 */
void R_Draw2D(void) {
	
	r_stats.count_draw_arrays = r_draw_2d.num_draw_arrays;

	if (r_draw_2d.num_draw_arrays == 0) {
		return;
	}

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glUseProgram(r_draw_2d_program.name);

	glBindBufferBase(GL_UNIFORM_BUFFER, 0, r_uniforms.buffer);

	glBindVertexArray(r_draw_2d.vertex_array);
	
	glBindBuffer(GL_ARRAY_BUFFER, r_draw_2d.vertex_buffer);
	glBufferData(GL_ARRAY_BUFFER, r_draw_2d.num_vertexes * sizeof(r_draw_2d_vertex_t), r_draw_2d.vertexes, GL_DYNAMIC_DRAW);

	glEnableVertexAttribArray(r_draw_2d_program.in_position);
	glEnableVertexAttribArray(r_draw_2d_program.in_diffusemap);
	glEnableVertexAttribArray(r_draw_2d_program.in_color);

	const r_draw_2d_arrays_t *draw = r_draw_2d.draw_arrays;
	for (int32_t i = 0; i < r_draw_2d.num_draw_arrays; i++, draw++) {
		glBindTexture(GL_TEXTURE_2D, draw->texture);
		glDrawArrays(draw->mode, draw->first_vertex, draw->num_vertexes);
	}

	glBindBuffer(GL_ARRAY_BUFFER, 0);

	glBindVertexArray(0);

	glUseProgram(0);
	
	r_draw_2d.num_draw_arrays = 0;
	r_draw_2d.num_vertexes = 0;

	glBlendFunc(GL_ONE, GL_ZERO);
	glDisable(GL_BLEND);

	R_GetError(NULL);
}

/**
 * @brief Initializes the specified bitmap font. The layout of the font is square,
 * 2^n (e.g. 256x256, 512x512), and 8 rows by 16 columns. See below:
 *
 *
 *  !"#$%&'()*+,-./
 * 0123456789:;<=>?
 * @ABCDEFGHIJKLMNO
 * PQRSTUVWXYZ[\]^_
 * 'abcdefghijklmno
 * pqrstuvwxyz{|}"
 */
static void R_InitFont(char *name) {

	if (r_draw_2d.num_fonts == MAX_DRAW_FONTS) {
		Com_Error(ERROR_DROP, "MAX_DRAW_FONTS\n");
	}

	r_font_t *font = &r_draw_2d.fonts[r_draw_2d.num_fonts++];

	g_strlcpy(font->name, name, sizeof(font->name));

	font->image = R_LoadImage(va("fonts/%s", name), IT_FONT);

	font->char_width = font->image->width / r_context.window_scale / 16;
	font->char_height = font->image->height / r_context.window_scale / 8;

	Com_Debug(DEBUG_RENDERER, "%s (%dx%d)\n", font->name, font->char_width, font->char_height);
}

/**
 * @brief
 */
static void R_InitDraw2DProgram(void) {

	memset(&r_draw_2d_program, 0, sizeof(r_draw_2d_program));

	r_draw_2d_program.name = R_LoadProgram(
			R_ShaderDescriptor(GL_VERTEX_SHADER, "draw_2d_vs.glsl", NULL),
			R_ShaderDescriptor(GL_FRAGMENT_SHADER, "draw_2d_fs.glsl", NULL),
			NULL);

	glUseProgram(r_draw_2d_program.name);

	r_draw_2d_program.uniforms_block = glGetUniformBlockIndex(r_draw_2d_program.name, "uniforms_block");
	glUniformBlockBinding(r_draw_2d_program.name, r_draw_2d_program.uniforms_block, 0);

	r_draw_2d_program.in_position = glGetAttribLocation(r_draw_2d_program.name, "in_position");
	r_draw_2d_program.in_diffusemap = glGetAttribLocation(r_draw_2d_program.name, "in_diffusemap");
	r_draw_2d_program.in_color = glGetAttribLocation(r_draw_2d_program.name, "in_color");

	r_draw_2d_program.texture_diffusemap = glGetUniformLocation(r_draw_2d_program.name, "texture_diffusemap");

	glUniform1i(r_draw_2d_program.texture_diffusemap, TEXTURE_DIFFUSEMAP);

	glUseProgram(0);

	R_GetError(NULL);
}

/**
 * @brief
 */
void R_InitDraw2D(void) {

	memset(&r_draw_2d, 0, sizeof(r_draw_2d));

	R_InitFont("small");
	R_InitFont("medium");
	R_InitFont("large");

	R_BindFont(NULL, NULL, NULL);

	r_draw_2d.null_texture = R_LoadImage("textures/common/white", IT_PROGRAM);
	assert(r_draw_2d.null_texture);
	
	glGenVertexArrays(1, &r_draw_2d.vertex_array);
	glBindVertexArray(r_draw_2d.vertex_array);

	glGenBuffers(1, &r_draw_2d.vertex_buffer);
	glBindBuffer(GL_ARRAY_BUFFER, r_draw_2d.vertex_buffer);

	glVertexAttribPointer(0, 2, GL_SHORT, GL_FALSE, sizeof(r_draw_2d_vertex_t), (void *) offsetof(r_draw_2d_vertex_t, position));
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(r_draw_2d_vertex_t), (void *) offsetof(r_draw_2d_vertex_t, diffusemap));
	glVertexAttribPointer(2, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(r_draw_2d_vertex_t), (void *) offsetof(r_draw_2d_vertex_t, color));

	glBindBuffer(GL_ARRAY_BUFFER, 0);
	
	glBindVertexArray(0);

	R_GetError(NULL);

	R_InitDraw2DProgram();
}

/**
 * @brief
 */
static void R_ShutdownDraw2DProgram(void) {

	glDeleteProgram(r_draw_2d_program.name);

	r_draw_2d_program.name = 0;
}

/**
 * @brief
 */
void R_ShutdownDraw2D(void) {

	glDeleteVertexArrays(1, &r_draw_2d.vertex_array);
	glDeleteBuffers(1, &r_draw_2d.vertex_buffer);

	R_ShutdownDraw2DProgram();
}
