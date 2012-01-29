/*
 * Copyright(c) 1997-2001 Id Software, Inc.
 * Copyright(c) 2002 The Quakeforge Project.
 * Copyright(c) 2006 Quake2World.
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

#define MAX_CHARS 8192  // per font
// characters are batched per frame and drawn in one shot
// accumulate coordinates and colors as vertex arrays
typedef struct r_char_arrays_s {
	GLfloat texcoords[MAX_CHARS * 4 * 2];
	unsigned int texcoord_index;

	GLshort verts[MAX_CHARS * 4 * 2];
	unsigned int vert_index;

	GLbyte colors[MAX_CHARS * 4 * 4];
	unsigned int color_index;
} r_char_arrays_t;

#define MAX_FILLS 512

// fills (alpha-blended quads) are also batched per frame
typedef struct r_fill_arrays_s {
	GLshort verts[MAX_FILLS * 4 * 2];
	unsigned int vert_index;

	GLbyte colors[MAX_FILLS * 4 * 4];
	unsigned int color_index;
} r_fill_arrays_t;

#define MAX_LINES 512

// lines are batched per frame too
typedef struct r_line_arrays_s {
	GLshort verts[MAX_LINES * 2 * 2];
	unsigned int vert_index;

	GLbyte colors[MAX_LINES * 2 * 4];
	unsigned int color_index;
} r_line_arrays_t;

// each font has vertex arrays of characters to draw each frame
typedef struct r_font_s {
	char name[MAX_QPATH];

	r_image_t *image;

	r_pixel_t char_width;
	r_pixel_t char_height;
} r_font_t;

#define MAX_FONTS 3

// pull it all together in one structure
typedef struct r_draw_s {

	r_image_t *cursor;

	// registered fonts
	unsigned short num_fonts;
	r_font_t fonts[MAX_FONTS];

	// active font
	r_font_t *font;

	// actual text colors as ABGR unsigned integers
	unsigned int colors[MAX_COLORS];

	r_char_arrays_t char_arrays[MAX_FONTS];
	r_fill_arrays_t fill_arrays;
	r_line_arrays_t line_arrays;

	// hash pics for fast lookup
	hash_table_t hash_table;
} r_draw_t;

r_draw_t r_draw;

/*
 * R_LoadPic
 */
r_image_t *R_LoadPic(const char *name) {
	r_image_t *image;

	if ((image = Hash_Get(&r_draw.hash_table, name)))
		return image;

	if (*name == '#')
		image = R_LoadImage(name + 1, it_pic);
	else
		image = R_LoadImage(va("pics/%s", name), it_pic);

	Hash_Put(&r_draw.hash_table, name, image);

	return image;
}

/*
 * R_DrawImage
 */
static void R_DrawImage(r_pixel_t x, r_pixel_t y, float scale, r_image_t *image) {

	R_BindTexture(image->texnum);

	// our texcoords are already setup, just set verts and draw

	r_state.vertex_array_2d[0] = x;
	r_state.vertex_array_2d[1] = y;

	r_state.vertex_array_2d[2] = x + image->width * scale;
	r_state.vertex_array_2d[3] = y;

	r_state.vertex_array_2d[4] = x + image->width * scale;
	r_state.vertex_array_2d[5] = y + image->height * scale;

	r_state.vertex_array_2d[6] = x;
	r_state.vertex_array_2d[7] = y + image->height * scale;

	glDrawArrays(GL_QUADS, 0, 4);
}

/*
 * R_DrawPic
 */
void R_DrawPic(r_pixel_t x, r_pixel_t y, float scale, const char *name) {
	r_image_t *pic;

	pic = R_LoadPic(name);

	if (!pic) {
		Com_Warn("R_DrawScaledPic: Can't find %s.\n", name);
		return;
	}

	R_DrawImage(x, y, scale, pic);
}

/*
 * R_DrawCursor
 */
void R_DrawCursor(r_pixel_t x, r_pixel_t y) {

	x -= (r_draw.cursor->width / 2.0);
	y -= (r_draw.cursor->height / 2.0);

	R_DrawImage(x, y, 1.0, r_draw.cursor);
}

/*
 * R_BindFont
 *
 * Binds the specified font, returning the character width and height.
 */
void R_BindFont(const char *name, r_pixel_t *cw, r_pixel_t *ch) {

	r_draw.font = &r_draw.fonts[1]; // medium is the default font

	if (name) { // try to find it
		unsigned short i;

		for (i = 0; i < r_draw.num_fonts; i++) {
			if (!strcmp(name, r_draw.fonts[i].name)) {
				r_draw.font = &r_draw.fonts[i];
				break;
			}
		}

		if (i == r_draw.num_fonts) {
			Com_Warn("R_BindFont: Unknown font: %s\n", name);
		}
	}

	if (cw)
		*cw = r_draw.font->char_width;

	if (ch)
		*ch = r_draw.font->char_height;
}

/*
 * R_StringWidth
 *
 * Return the width of the specified string in pixels. This will vary based
 * on the currently bound font. Color escapes are omitted.
 */
r_pixel_t R_StringWidth(const char *s) {
	char stripped[MAX_STRING_CHARS];

	StripColor(s, stripped);

	return strlen(stripped) * r_draw.font->char_width;
}

/*
 * R_DrawString
 */
size_t R_DrawString(r_pixel_t x, r_pixel_t y, const char *s, int color) {
	return R_DrawSizedString(x, y, s, 999, 999, color);
}

/*
 * R_DrawBytes
 */
size_t R_DrawBytes(r_pixel_t x, r_pixel_t y, const char *s, size_t size, int color) {
	return R_DrawSizedString(x, y, s, size, size, color);
}

/*
 * R_DrawSizedString
 *
 * Draws at most len chars or size bytes of the specified string.  Color escape
 * sequences are not visible chars.  Returns the number of chars drawn.
 */
size_t R_DrawSizedString(r_pixel_t x, r_pixel_t y, const char *s, size_t len, size_t size,
		int color) {
	size_t i, j;

	i = j = 0;
	while (*s && i < len && j < size) {

		if (IS_COLOR(s)) { // color escapes
			color = *(s + 1) - '0';
			j += 2;
			s += 2;
			continue;
		}

		if (IS_LEGACY_COLOR(s)) { // legacy colors
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

/*
 * R_DrawChar
 */
void R_DrawChar(r_pixel_t x, r_pixel_t y, char c, int color) {

	if (x > r_context.width || y > r_context.height)
		return;

	if (c == ' ')
		return; // small optimization for space

	r_char_arrays_t *chars = &r_draw.char_arrays[r_draw.font - r_draw.fonts];

	const unsigned int row = (unsigned int) c >> 4;
	const unsigned int col = (unsigned int) c & 15;

	const float frow = row * 0.1250;
	const float fcol = col * 0.0625;

	// resolve ABGR color
	const unsigned int *abgr = &r_draw.colors[color & (MAX_COLORS - 1)];

	memcpy(&chars->colors[chars->color_index + 0], abgr, 4);
	memcpy(&chars->colors[chars->color_index + 4], abgr, 4);
	memcpy(&chars->colors[chars->color_index + 8], abgr, 4);
	memcpy(&chars->colors[chars->color_index + 12], abgr, 4);

	chars->color_index += 16;

	chars->texcoords[chars->texcoord_index + 0] = fcol;
	chars->texcoords[chars->texcoord_index + 1] = frow;

	chars->texcoords[chars->texcoord_index + 2] = fcol + 0.0625;
	chars->texcoords[chars->texcoord_index + 3] = frow;

	chars->texcoords[chars->texcoord_index + 4] = fcol + 0.0625;
	chars->texcoords[chars->texcoord_index + 5] = frow + 0.1250;

	chars->texcoords[chars->texcoord_index + 6] = fcol;
	chars->texcoords[chars->texcoord_index + 7] = frow + 0.1250;

	chars->texcoord_index += 8;

	chars->verts[chars->vert_index + 0] = x;
	chars->verts[chars->vert_index + 1] = y;

	chars->verts[chars->vert_index + 2] = x + r_draw.font->char_width;
	chars->verts[chars->vert_index + 3] = y;

	chars->verts[chars->vert_index + 4] = x + r_draw.font->char_width;
	chars->verts[chars->vert_index + 5] = y + r_draw.font->char_height;

	chars->verts[chars->vert_index + 6] = x;
	chars->verts[chars->vert_index + 7] = y + r_draw.font->char_height;

	chars->vert_index += 8;
}

/*
 * R_DrawChars
 */
void R_DrawChars(void) {
	unsigned short i;

	for (i = 0; i < r_draw.num_fonts; i++) {
		r_char_arrays_t *chars = &r_draw.char_arrays[i];

		if (!chars->vert_index)
			continue;

		R_BindTexture(r_draw.fonts[i].image->texnum);

		R_EnableColorArray(true);

		// alter the array pointers
		R_BindArray(GL_COLOR_ARRAY, GL_UNSIGNED_BYTE, chars->colors);
		R_BindArray(GL_TEXTURE_COORD_ARRAY, GL_FLOAT, chars->texcoords);
		R_BindArray(GL_VERTEX_ARRAY, GL_SHORT, chars->verts);

		glDrawArrays(GL_QUADS, 0, chars->vert_index / 2);

		chars->color_index = 0;
		chars->texcoord_index = 0;
		chars->vert_index = 0;
	}

	// restore array pointers
	R_BindDefaultArray(GL_TEXTURE_COORD_ARRAY);
	R_BindDefaultArray(GL_VERTEX_ARRAY);
	R_BindDefaultArray(GL_COLOR_ARRAY);

	R_EnableColorArray(false);

	// restore draw color
	glColor4ubv(color_white);
}

/*
 * R_DrawFill
 *
 * The color can be specified as an index into the palette with positive alpha
 * value for a, or as an RGBA value (32 bit) by passing -1.0 for a.
 */
void R_DrawFill(r_pixel_t x, r_pixel_t y, r_pixel_t w, r_pixel_t h, int c, float a) {
	byte color[4];

	if (a > 1.0) {
		Com_Warn("R_DrawFill: Bad alpha %f.\n", a);
		return;
	}

	if (a < 0.0) { // RGBA integer
		memcpy(color, &c, 4);
	} else { // palette index
		memcpy(color, &palette[c], sizeof(color));
		color[3] = a * 255;
	}

	// duplicate color data to all 4 verts
	memcpy(&r_draw.fill_arrays.colors[r_draw.fill_arrays.color_index + 0], color, 4);
	memcpy(&r_draw.fill_arrays.colors[r_draw.fill_arrays.color_index + 4], color, 4);
	memcpy(&r_draw.fill_arrays.colors[r_draw.fill_arrays.color_index + 8], color, 4);
	memcpy(&r_draw.fill_arrays.colors[r_draw.fill_arrays.color_index + 12], color, 4);

	r_draw.fill_arrays.color_index += 16;

	// populate verts
	r_draw.fill_arrays.verts[r_draw.fill_arrays.vert_index + 0] = x;
	r_draw.fill_arrays.verts[r_draw.fill_arrays.vert_index + 1] = y;

	r_draw.fill_arrays.verts[r_draw.fill_arrays.vert_index + 2] = x + w;
	r_draw.fill_arrays.verts[r_draw.fill_arrays.vert_index + 3] = y;

	r_draw.fill_arrays.verts[r_draw.fill_arrays.vert_index + 4] = x + w;
	r_draw.fill_arrays.verts[r_draw.fill_arrays.vert_index + 5] = y + h;

	r_draw.fill_arrays.verts[r_draw.fill_arrays.vert_index + 6] = x;
	r_draw.fill_arrays.verts[r_draw.fill_arrays.vert_index + 7] = y + h;

	r_draw.fill_arrays.vert_index += 8;
}

/*
 * R_DrawFills
 */
void R_DrawFills(void) {

	if (!r_draw.fill_arrays.vert_index)
		return;

	R_EnableTexture(&texunit_diffuse, false);

	R_EnableColorArray(true);

	// alter the array pointers
	R_BindArray(GL_VERTEX_ARRAY, GL_SHORT, r_draw.fill_arrays.verts);
	R_BindArray(GL_COLOR_ARRAY, GL_UNSIGNED_BYTE, r_draw.fill_arrays.colors);

	glDrawArrays(GL_QUADS, 0, r_draw.fill_arrays.vert_index / 2);

	// and restore them
	R_BindDefaultArray(GL_VERTEX_ARRAY);
	R_BindDefaultArray(GL_COLOR_ARRAY);

	R_EnableColorArray(false);

	R_EnableTexture(&texunit_diffuse, true);

	r_draw.fill_arrays.vert_index = r_draw.fill_arrays.color_index = 0;
}

/*
 * R_DrawLine
 */
void R_DrawLine(r_pixel_t x1, r_pixel_t y1, r_pixel_t x2, r_pixel_t y2, int c, float a) {
	byte color[4];

	if (a > 1.0) {
		Com_Warn("R_DrawLine: Bad alpha %f.\n", a);
		return;
	}

	if (a < 0.0) { // RGBA integer
		memcpy(color, &c, 4);
	} else { // palette index
		memcpy(color, &palette[c], sizeof(color));
		color[3] = a * 255;
	}

	// duplicate color data to all 4 verts
	memcpy(&r_draw.line_arrays.colors[r_draw.line_arrays.color_index + 0], color, 4);
	memcpy(&r_draw.line_arrays.colors[r_draw.line_arrays.color_index + 4], color, 4);

	r_draw.line_arrays.color_index += 8;

	// populate verts
	r_draw.line_arrays.verts[r_draw.line_arrays.vert_index + 0] = x1;
	r_draw.line_arrays.verts[r_draw.line_arrays.vert_index + 1] = y1;

	r_draw.line_arrays.verts[r_draw.line_arrays.vert_index + 2] = x2;
	r_draw.line_arrays.verts[r_draw.line_arrays.vert_index + 3] = y2;

	r_draw.line_arrays.vert_index += 4;
}

/*
 * R_DrawLines
 */
void R_DrawLines(void) {

	if (!r_draw.line_arrays.vert_index)
		return;

	R_EnableTexture(&texunit_diffuse, false);

	R_EnableColorArray(true);

	// alter the array pointers
	R_BindArray(GL_VERTEX_ARRAY, GL_SHORT, r_draw.line_arrays.verts);
	R_BindArray(GL_COLOR_ARRAY, GL_UNSIGNED_BYTE, r_draw.line_arrays.colors);

	glDrawArrays(GL_LINES, 0, r_draw.line_arrays.vert_index / 2);

	// and restore them
	R_BindDefaultArray(GL_VERTEX_ARRAY);
	R_BindDefaultArray(GL_COLOR_ARRAY);

	R_EnableColorArray(false);

	R_EnableTexture(&texunit_diffuse, true);

	r_draw.line_arrays.vert_index = r_draw.line_arrays.color_index = 0;
}

/*
 * R_FreePics
 */
void R_FreePics(void) {
	Hash_Free(&r_draw.hash_table);
	Hash_Init(&r_draw.hash_table);
}

/*
 * R_InitFont
 *
 * Initializes the specified bitmap font. The layout of the font is square,
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
		Com_Error(ERR_DROP, "R_InitFont: MAX_FONTS reached.\n");
		return;
	}

	r_font_t *font = &r_draw.fonts[r_draw.num_fonts++];

	strncpy(font->name, name, sizeof(font->name) - 1);

	font->image = R_LoadImage(va("fonts/%s", name), it_font);

	font->char_width = font->image->width / 16;
	font->char_height = font->image->height / 8;

	Com_Debug("R_InitFont: %s (%dx%d)\n", font->name, font->char_width,
			font->char_height);
}

/*
 * R_InitDraw
 */
void R_InitDraw(void) {

	memset(&r_draw, 0, sizeof(r_draw));

	r_draw.cursor = R_LoadImage("fonts/cursor", it_font);

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

	Hash_Init(&r_draw.hash_table);
}
