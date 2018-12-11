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
#include "client.h"

#define MAX_CHARS 0x10000  // per font
#define MAX_CHAR_VERTS MAX_CHARS * 4
#define MAX_CHAR_ELEMENTS MAX_CHARS * 6

typedef struct {
	s16vec2_t position;
	u16vec2_t texcoord;
	u8vec4_t color;
} r_char_interleave_vertex_t;

static r_buffer_layout_t r_char_buffer_layout[] = {
	{ .attribute = R_ATTRIB_POSITION, .type = R_TYPE_SHORT, .count = 2 },
	{ .attribute = R_ATTRIB_DIFFUSE, .type = R_TYPE_UNSIGNED_SHORT, .count = 2, .normalized = true },
	{ .attribute = R_ATTRIB_COLOR, .type = R_TYPE_UNSIGNED_BYTE, .count = 4, .normalized = true },
	{ .attribute = -1 }
};

// characters are batched per frame and drawn in one shot
// accumulate coordinates and colors as vertex arrays
typedef struct r_char_arrays_s {
	r_char_interleave_vertex_t verts[MAX_CHAR_VERTS];
	uint32_t vert_index;
	r_buffer_t vert_buffer;

	GLuint elements[MAX_CHAR_ELEMENTS];
	uint32_t element_index;
	r_buffer_t element_buffer;

	uint32_t num_chars;
} r_char_arrays_t;

#define MAX_FILLS 512
#define MAX_FILL_VERTS MAX_FILLS * 4
#define MAX_FILL_ELEMENTS MAX_FILLS * 6

typedef struct {
	vec2_t position;
	u8vec4_t color;
} r_fill_interleave_vertex_t;

static r_buffer_layout_t r_fill_buffer_layout[] = {
	{ .attribute = R_ATTRIB_POSITION, .type = R_TYPE_FLOAT, .count = 2 },
	{ .attribute = R_ATTRIB_COLOR, .type = R_TYPE_UNSIGNED_BYTE, .count = 4, .normalized = true },
	{ .attribute = -1 }
};

// fills (alpha-blended quads) are also batched per frame
typedef struct r_fill_arrays_s {
	r_fill_interleave_vertex_t verts[MAX_FILL_VERTS];
	uint32_t vert_index;
	r_buffer_t vert_buffer;

	GLuint elements[MAX_FILL_ELEMENTS];
	uint32_t element_index;
	r_buffer_t element_buffer;

	uint32_t num_fills;

	// buffer used for immediately-rendered fills
	r_buffer_t ui_vert_buffer;
} r_fill_arrays_t;

#define MAX_LINES 512
#define MAX_LINE_VERTS MAX_LINES * 4

// lines are batched per frame too
typedef struct r_line_arrays_s {
	r_fill_interleave_vertex_t verts[MAX_LINE_VERTS];
	uint32_t vert_index;
	r_buffer_t vert_buffer;

	// buffer used for immediately-rendered lines
	r_buffer_t ui_vert_buffer;
} r_line_arrays_t;

// each font has vertex arrays of characters to draw each frame
typedef struct r_font_s {
	char name[MAX_QPATH];

	r_image_t *image;

	r_pixel_t char_width;
	r_pixel_t char_height;
} r_font_t;

#define MAX_FONTS 3

typedef struct {
	s16vec2_t position;
	u16vec2_t texcoord;
} r_image_interleave_vertex_t;

static r_buffer_layout_t r_image_buffer_layout[] = {
	{ .attribute = R_ATTRIB_POSITION, .type = R_TYPE_SHORT, .count = 2 },
	{ .attribute = R_ATTRIB_DIFFUSE, .type = R_TYPE_UNSIGNED_SHORT, .count = 2 },
	{ .attribute = -1 }
};

// pull it all together in one structure
typedef struct r_draw_s {

	// registered fonts
	uint16_t num_fonts;
	r_font_t fonts[MAX_FONTS];

	// active font
	r_font_t *font;

	// actual text colors as ABGR uint32_tegers
	uint32_t colors[MAX_COLORS];

	r_char_arrays_t char_arrays[MAX_FONTS];
	r_fill_arrays_t fill_arrays;
	r_line_arrays_t line_arrays;

	r_buffer_t image_buffer;
	r_image_interleave_vertex_t image_vertices[4];

	r_buffer_t supersample_buffer;
} r_draw_t;

static r_draw_t r_draw;

/**
 * @brief Make a quad by passing index to first vertex and last element ID.
 */
static void R_MakeQuadU32(uint32_t *indices, const uint32_t vertex_id) {
	*(indices)++ = vertex_id + 0;
	*(indices)++ = vertex_id + 1;
	*(indices)++ = vertex_id + 2;

	*(indices)++ = vertex_id + 0;
	*(indices)++ = vertex_id + 2;
	*(indices)++ = vertex_id + 3;
}

/**
 * @brief
 */
static void R_DrawImage_(r_pixel_t x, r_pixel_t y, r_pixel_t w, r_pixel_t h, const GLuint texnum,
                         const r_buffer_t *buffer) {

	R_BindDiffuseTexture(texnum);

	R_BindAttributeInterleaveBuffer(buffer, R_ATTRIB_MASK_ALL);

	R_DrawArrays(GL_TRIANGLE_FAN, 0, 4);
}

/**
 * @brief
 */
void R_DrawImageResized(r_pixel_t x, r_pixel_t y, r_pixel_t w, r_pixel_t h, const r_image_t *image) {

	Vector2Set(r_draw.image_vertices[0].position, x, y);
	Vector2Set(r_draw.image_vertices[1].position, x + w, y);
	Vector2Set(r_draw.image_vertices[2].position, x + w, y + h);
	Vector2Set(r_draw.image_vertices[3].position, x, y + h);

	R_UploadToBuffer(&r_draw.image_buffer, sizeof(r_draw.image_vertices), r_draw.image_vertices);

	R_DrawImage_(x, y, w, h, image->texnum, &r_draw.image_buffer);
}

/**
 * @brief
 */
void R_DrawImage(r_pixel_t x, r_pixel_t y, vec_t scale, const r_image_t *image) {

	R_DrawImageResized(x, y, image->width * scale, image->height * scale, image);
}

/**
 * @brief
 */
void R_DrawSupersample(void) {

	if (!r_state.supersample_fb) {
		return;
	}

	R_DrawImage_(0, 0, r_context.width, r_context.height, r_state.supersample_image->texnum, &r_draw.supersample_buffer);
}

/**
 * @brief Binds the specified font, returning the character width and height.
 */
void R_BindFont(const char *name, r_pixel_t *cw, r_pixel_t *ch) {

	if (name == NULL) {
		name = "medium";
	}

	uint16_t i;
	for (i = 0; i < r_draw.num_fonts; i++) {
		if (!g_strcmp0(name, r_draw.fonts[i].name)) {
			if (r_context.high_dpi && i < r_draw.num_fonts - 1) {
				r_draw.font = &r_draw.fonts[i + 1];
			} else {
				r_draw.font = &r_draw.fonts[i];
			}
			break;
		}
	}

	if (i == r_draw.num_fonts) {
		Com_Warn("Unknown font: %s\n", name);
	}

	if (cw) {
		*cw = r_draw.font->char_width;
	}

	if (ch) {
		*ch = r_draw.font->char_height;
	}
}

/**
 * @brief Return the width of the specified string in pixels. This will vary based
 * on the currently bound font. Color escapes are omitted.
 */
r_pixel_t R_StringWidth(const char *s) {
	return StrColorLen(s) * r_draw.font->char_width;
}

/**
 * @brief
 */
size_t R_DrawString(r_pixel_t x, r_pixel_t y, const char *s, int32_t color) {
	return R_DrawSizedString(x, y, s, UINT16_MAX, UINT16_MAX, color);
}

/**
 * @brief
 */
size_t R_DrawBytes(r_pixel_t x, r_pixel_t y, const char *s, size_t size, int32_t color) {
	return R_DrawSizedString(x, y, s, size, size, color);
}

/**
 * @brief Draws at most len chars or size bytes of the specified string. Color escape
 * sequences are not visible chars. Returns the number of chars drawn.
 */
size_t R_DrawSizedString(r_pixel_t x, r_pixel_t y, const char *s, size_t len, size_t size,
                         int32_t color) {
	size_t i, j;

	i = j = 0;
	while (*s && i < len && j < size) {

		if (IS_COLOR(s)) {
			color = *(s + 1) - '0';
			j += 2;
			s += 2;
			continue;
		}

		if (IS_LEGACY_COLOR(s)) {
			color = CON_COLOR_ALT;
			j++;
			s++;
			continue;
		}

		R_DrawChar(x, y, *s, color);
		x += r_draw.font->char_width; // next char position in line

		i++;
		j++;
		s++;
	}

	return i;
}

/**
 * @brief
 */
void R_DrawChar(r_pixel_t x, r_pixel_t y, char c, int32_t color) {

	if (x > r_context.width || y > r_context.height) {
		return;
	}

	if (c == ' ') {
		return;    // small optimization for space
	}

	r_char_arrays_t *chars = &r_draw.char_arrays[r_draw.font - r_draw.fonts];

	const uint32_t row = (uint32_t) c >> 4;
	const uint32_t col = (uint32_t) c & 15;

	const u16vec_t frow = PackTexcoord(row * 0.1250);
	const u16vec_t fcol = PackTexcoord(col * 0.0625);
	const u16vec_t frowe = PackTexcoord((row + 1) * 0.1250);
	const u16vec_t fcole = PackTexcoord((col + 1) * 0.0625);

	// resolve ABGR color
	const uint32_t *abgr = &r_draw.colors[color & (MAX_COLORS - 1)];

	// copy to all 4 verts
	for (uint32_t i = 0; i < 4; ++i) {
		memcpy(&chars->verts[chars->vert_index + i].color, abgr, sizeof(u8vec4_t));
	}

	Vector2Set(chars->verts[chars->vert_index].texcoord, fcol, frow);
	Vector2Set(chars->verts[chars->vert_index + 1].texcoord, fcole, frow);
	Vector2Set(chars->verts[chars->vert_index + 2].texcoord, fcole, frowe);
	Vector2Set(chars->verts[chars->vert_index + 3].texcoord, fcol, frowe);

	Vector2Set(chars->verts[chars->vert_index].position, x, y);
	Vector2Set(chars->verts[chars->vert_index + 1].position, x + r_draw.font->char_width, y);
	Vector2Set(chars->verts[chars->vert_index + 2].position, x + r_draw.font->char_width, y + r_draw.font->char_height);
	Vector2Set(chars->verts[chars->vert_index + 3].position, x, y + r_draw.font->char_height);

	R_MakeQuadU32(&chars->elements[chars->element_index], chars->vert_index);

	chars->vert_index += 4;
	chars->element_index += 6;
	chars->num_chars++;
}

/**
 * @brief
 */
static void R_DrawChars(void) {

	for (uint16_t i = 0; i < r_draw.num_fonts; i++) {
		r_char_arrays_t *chars = &r_draw.char_arrays[i];

		if (!chars->vert_index) {
			continue;
		}

		R_UploadToBuffer(&r_draw.char_arrays[i].vert_buffer,
		                 r_draw.char_arrays[i].vert_index * sizeof(r_char_interleave_vertex_t),
		                 r_draw.char_arrays[i].verts);

		R_UploadToBuffer(&r_draw.char_arrays[i].element_buffer, r_draw.char_arrays[i].element_index * sizeof(GLuint),
		                 r_draw.char_arrays[i].elements);

		R_BindDiffuseTexture(r_draw.fonts[i].image->texnum);

		R_EnableColorArray(true);

		// alter the array pointers
		R_BindAttributeInterleaveBuffer(&chars->vert_buffer, R_ATTRIB_MASK_ALL);

		R_BindAttributeBuffer(R_ATTRIB_ELEMENTS, &chars->element_buffer);

		R_DrawArrays(GL_TRIANGLES, 0, chars->element_index);

		chars->vert_index = 0;
		chars->element_index = 0;
		chars->num_chars = 0;
	}

	// restore array pointers
	R_UnbindAttributeBuffer(R_ATTRIB_COLOR);
	R_UnbindAttributeBuffer(R_ATTRIB_DIFFUSE);
	R_UnbindAttributeBuffer(R_ATTRIB_POSITION);

	R_UnbindAttributeBuffer(R_ATTRIB_ELEMENTS);

	R_EnableColorArray(false);

	// restore draw color
	R_Color(NULL);
}

/**
 * @brief The color can be specified as an index into the palette with positive alpha
 * value for a, or as an RGBA value (32 bit) by passing -1.0 for a.
 */
void R_DrawFill(r_pixel_t x, r_pixel_t y, r_pixel_t w, r_pixel_t h, int32_t c, vec_t a) {

	if (a > 1.0) {
		Com_Warn("Bad alpha %f\n", a);
		return;
	}

	if (a >= 0.0) { // palette index
		c = img_palette[c] & 0x00FFFFFF;
		c |= (((u8vec_t) (a * 255.0)) & 0xFF) << 24;
	}

	// duplicate color data to all 4 verts
	for (uint32_t i = 0; i < 4; ++i) {
		memcpy(&r_draw.fill_arrays.verts[r_draw.fill_arrays.vert_index + i].color, &c, sizeof(u8vec4_t));
	}

	// populate verts
	Vector2Set(r_draw.fill_arrays.verts[r_draw.fill_arrays.vert_index].position, x + 0.5, y + 0.5);
	Vector2Set(r_draw.fill_arrays.verts[r_draw.fill_arrays.vert_index + 1].position, (x + w) + 0.5, y + 0.5);
	Vector2Set(r_draw.fill_arrays.verts[r_draw.fill_arrays.vert_index + 2].position, (x + w) + 0.5, (y + h) + 0.5);
	Vector2Set(r_draw.fill_arrays.verts[r_draw.fill_arrays.vert_index + 3].position, x + 0.5, (y + h) + 0.5);

	r_draw.fill_arrays.vert_index += 4;

	const GLuint fill_index = r_draw.fill_arrays.num_fills * 4;

	R_MakeQuadU32(&r_draw.fill_arrays.elements[r_draw.fill_arrays.element_index], fill_index);
	r_draw.fill_arrays.element_index += 6;

	r_draw.fill_arrays.num_fills++;
}

/**
 * @brief
 */
static void R_DrawFills(void) {

	if (!r_draw.fill_arrays.vert_index) {
		return;
	}

	R_BindDiffuseTexture(r_image_state.null->texnum);

	// upload the changed data
	R_UploadToBuffer(&r_draw.fill_arrays.vert_buffer, r_draw.fill_arrays.vert_index * sizeof(r_fill_interleave_vertex_t),
	                 r_draw.fill_arrays.verts);

	R_UploadToBuffer(&r_draw.fill_arrays.element_buffer, r_draw.fill_arrays.element_index * sizeof(GLuint),
	                 r_draw.fill_arrays.elements);

	R_EnableColorArray(true);

	// alter the array pointers
	R_BindAttributeInterleaveBuffer(&r_draw.fill_arrays.vert_buffer, R_ATTRIB_MASK_ALL);
	R_BindAttributeBuffer(R_ATTRIB_ELEMENTS, &r_draw.fill_arrays.element_buffer);

	R_DrawArrays(GL_TRIANGLES, 0, r_draw.fill_arrays.element_index);

	// and restore them
	R_UnbindAttributeBuffer(R_ATTRIB_POSITION);
	R_UnbindAttributeBuffer(R_ATTRIB_COLOR);
	R_UnbindAttributeBuffer(R_ATTRIB_ELEMENTS);

	R_EnableColorArray(false);

	r_draw.fill_arrays.vert_index = r_draw.fill_arrays.element_index = r_draw.fill_arrays.num_fills = 0;
}

/**
 * @brief
 */
void R_DrawLine(r_pixel_t x1, r_pixel_t y1, r_pixel_t x2, r_pixel_t y2, int32_t c, vec_t a) {

	if (a > 1.0) {
		Com_Warn("Bad alpha %f\n", a);
		return;
	}

	if (a >= 0.0) { // palette index
		c = img_palette[c] & 0x00FFFFFF;
		c |= (((u8vec_t) (a * 255.0)) & 0xFF) << 24;
	}

	// duplicate color data to 2 verts
	for (uint32_t i = 0; i < 4; ++i) {
		memcpy(&r_draw.line_arrays.verts[r_draw.line_arrays.vert_index + i].color, &c, sizeof(u8vec4_t));
	}

	// populate verts
	Vector2Set(r_draw.line_arrays.verts[r_draw.line_arrays.vert_index + 0].position, x1 + 0.5, y1 + 0.5);
	Vector2Set(r_draw.line_arrays.verts[r_draw.line_arrays.vert_index + 1].position, x2 + 0.5, y2 + 0.5);

	r_draw.line_arrays.vert_index += 2;
}

/**
 * @brief
 */
static void R_DrawLines(void) {

	if (!r_draw.line_arrays.vert_index) {
		return;
	}

	R_BindDiffuseTexture(r_image_state.null->texnum);

	// upload the changed data
	R_UploadToBuffer(&r_draw.line_arrays.vert_buffer, r_draw.line_arrays.vert_index * sizeof(r_fill_interleave_vertex_t),
	                 r_draw.line_arrays.verts);

	R_EnableColorArray(true);

	// alter the array pointers
	R_BindAttributeInterleaveBuffer(&r_draw.line_arrays.vert_buffer, R_ATTRIB_MASK_ALL);

	R_DrawArrays(GL_LINES, 0, r_draw.line_arrays.vert_index);

	// and restore them
	R_UnbindAttributeBuffer(R_ATTRIB_POSITION);
	R_UnbindAttributeBuffer(R_ATTRIB_COLOR);

	R_EnableColorArray(false);

	r_draw.line_arrays.vert_index = 0;
}

/**
 * @brief Draws a filled rect, for MVC. Uses current color.
 */
void R_DrawFillUI(const SDL_Rect *rect) {

	const vec2_t verts[] = {
		{ rect->x - 1, rect->y - 1 },
		{ rect->x + rect->w + 1, rect->y - 1 },
		{ rect->x + rect->w + 1, rect->y + rect->h + 1 },
		{ rect->x - 1, rect->y + rect->h + 1 }
	};

	R_EnableColorArray(false);

	R_BindDiffuseTexture(r_image_state.null->texnum);

	// upload the changed data
	R_UploadToBuffer(&r_draw.fill_arrays.ui_vert_buffer, sizeof(verts), verts);

	// alter the array pointers
	R_BindAttributeBuffer(R_ATTRIB_POSITION, &r_draw.fill_arrays.ui_vert_buffer);

	// draw!
	R_DrawArrays(GL_TRIANGLE_FAN, 0, 4);

	// and restore them
	R_UnbindAttributeBuffer(R_ATTRIB_POSITION);
}

void R_DrawLinesUI(const SDL_Point *points, const size_t count, const _Bool loop) {

	vec2_t point_buffer[count];

	for (size_t i = 0; i < count; ++i) {
		Vector2Set(point_buffer[i], points[i].x + 0.5, points[i].y + 0.5);
	}

	R_EnableColorArray(false);

	R_BindDiffuseTexture(r_image_state.null->texnum);

	// upload the changed data
	R_UploadToBuffer(&r_draw.line_arrays.ui_vert_buffer, sizeof(point_buffer), point_buffer);

	// alter the array pointers
	R_BindAttributeBuffer(R_ATTRIB_POSITION, &r_draw.line_arrays.ui_vert_buffer);

	// draw!
	R_DrawArrays(loop ? GL_LINE_LOOP : GL_LINE_STRIP, 0, (GLsizei) count);

	// and restore them
	R_UnbindAttributeBuffer(R_ATTRIB_POSITION);
}

/**
 * @brief Draw all 2D geometry accumulated for the current frame.
 */
void R_Draw2D(void) {

	R_DrawLines();

	R_DrawFills();

	R_DrawChars();
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

	if (r_draw.num_fonts == MAX_FONTS) {
		Com_Error(ERROR_DROP, "MAX_FONTS reached\n");
		return;
	}

	r_font_t *font = &r_draw.fonts[r_draw.num_fonts++];

	g_strlcpy(font->name, name, sizeof(font->name));

	font->image = R_LoadImage(va("fonts/%s", name), IT_FONT);

	font->char_width = font->image->width / 16;
	font->char_height = font->image->height / 8;

	Com_Debug(DEBUG_RENDERER, "%s (%dx%d)\n", font->name, font->char_width, font->char_height);
}

/**
 * @brief
 */
void R_InitDraw(void) {

	memset(&r_draw, 0, sizeof(r_draw));

	R_InitFont("small");
	R_InitFont("medium");
	R_InitFont("large");

	R_BindFont(NULL, NULL, NULL);

	// set ABGR color values
	r_draw.colors[CON_COLOR_BLACK] = 0xff000000;
	r_draw.colors[CON_COLOR_RED] = 0xff0000ff;
	r_draw.colors[CON_COLOR_GREEN] = 0xff00ff00;
	r_draw.colors[CON_COLOR_YELLOW] = 0xff00ffff;
	r_draw.colors[CON_COLOR_BLUE] = 0xffff0000;
	r_draw.colors[CON_COLOR_CYAN] = 0xffffff00;
	r_draw.colors[CON_COLOR_MAGENTA] = 0xffff00ff;
	r_draw.colors[CON_COLOR_WHITE] = 0xffffffff;

	for (int32_t i = 0; i < MAX_FONTS; ++i) {

		R_CreateInterleaveBuffer(&r_draw.char_arrays[i].vert_buffer, &(const r_create_interleave_t) {
			.struct_size = sizeof(r_char_interleave_vertex_t),
			.layout = r_char_buffer_layout,
			.hint = GL_DYNAMIC_DRAW,
			.size = sizeof(r_draw.char_arrays[i].verts)
		});

		R_CreateElementBuffer(&r_draw.char_arrays[i].element_buffer, &(const r_create_element_t) {
			.type = R_TYPE_UNSIGNED_INT,
			.hint = GL_DYNAMIC_DRAW,
			.size = sizeof(r_draw.char_arrays[i].elements)
		});
	}

	R_CreateInterleaveBuffer(&r_draw.fill_arrays.vert_buffer, &(const r_create_interleave_t) {
		.struct_size = sizeof(r_fill_interleave_vertex_t),
		.layout = r_fill_buffer_layout,
		.hint = GL_DYNAMIC_DRAW,
		.size = sizeof(r_draw.fill_arrays.verts)
	});

	R_CreateElementBuffer(&r_draw.fill_arrays.element_buffer, &(const r_create_element_t) {
		.type = R_TYPE_UNSIGNED_INT,
		.hint = GL_DYNAMIC_DRAW,
		.size = sizeof(r_draw.fill_arrays.elements)
	});

	R_CreateInterleaveBuffer(&r_draw.line_arrays.vert_buffer, &(const r_create_interleave_t) {
		.struct_size = sizeof(r_fill_interleave_vertex_t),
		.layout = r_fill_buffer_layout,
		.hint = GL_DYNAMIC_DRAW,
		.size = sizeof(r_draw.line_arrays.verts)
	});

	// fill buffer only needs 4 verts
	R_CreateDataBuffer(&r_draw.fill_arrays.ui_vert_buffer, &(const r_create_buffer_t) {
		.element = {
			.type = R_TYPE_FLOAT,
			.count = 2
		},
		.hint = GL_DYNAMIC_DRAW,
		.size = sizeof(vec2_t) * 4
	});

	R_CreateDataBuffer(&r_draw.line_arrays.ui_vert_buffer, &(const r_create_buffer_t) {
		.element = {
			.type = R_TYPE_FLOAT,
			.count = 2
		},
		.hint = GL_DYNAMIC_DRAW,
		.size = sizeof(vec2_t) * MAX_LINE_VERTS
	});

	Vector2Set(r_draw.image_vertices[0].texcoord, 0, 0);
	Vector2Set(r_draw.image_vertices[1].texcoord, 1, 0);
	Vector2Set(r_draw.image_vertices[2].texcoord, 1, 1);
	Vector2Set(r_draw.image_vertices[3].texcoord, 0, 1);

	R_CreateInterleaveBuffer(&r_draw.image_buffer, &(const r_create_interleave_t) {
		.struct_size = sizeof(r_image_interleave_vertex_t),
		.layout = r_image_buffer_layout,
		.hint = GL_DYNAMIC_DRAW,
		.size = sizeof(r_draw.image_vertices)
	});
	
	const r_image_interleave_vertex_t supersample_vertices[4] = {
		{ .position = { 0, 0 }, .texcoord = { 0, 1 } },
		{ .position = { r_context.width, 0 }, .texcoord = { 1, 1 } },
		{ .position = { r_context.width, r_context.height }, .texcoord = { 1, 0 } },
		{ .position = { 0, r_context.height }, .texcoord = { 0, 0 } },
	};

	R_CreateInterleaveBuffer(&r_draw.supersample_buffer, &(const r_create_interleave_t) {
		.struct_size = sizeof(r_image_interleave_vertex_t),
		.layout = r_image_buffer_layout,
		.hint = GL_STATIC_DRAW,
		.size = sizeof(supersample_vertices),
		.data = supersample_vertices
	});
}

/**
 * @brief
 */
void R_ShutdownDraw(void) {

	for (int32_t i = 0; i < MAX_FONTS; ++i) {

		R_DestroyBuffer(&r_draw.char_arrays[i].vert_buffer);
		R_DestroyBuffer(&r_draw.char_arrays[i].element_buffer);
	}

	R_DestroyBuffer(&r_draw.fill_arrays.vert_buffer);
	R_DestroyBuffer(&r_draw.fill_arrays.element_buffer);
	R_DestroyBuffer(&r_draw.line_arrays.vert_buffer);

	R_DestroyBuffer(&r_draw.fill_arrays.ui_vert_buffer);
	R_DestroyBuffer(&r_draw.line_arrays.ui_vert_buffer);

	R_DestroyBuffer(&r_draw.image_buffer);
	R_DestroyBuffer(&r_draw.supersample_buffer);
}
