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

#include "client.h"
#include "hash.h"


#define MAX_CHARS 2048  // per font

// characters are batched per frame and drawn in one shot
// accumulate coordinates and colors as vertex arrays
typedef struct r_char_arrays_s {
	GLfloat texcoords[MAX_CHARS * 4 * 2];
	int texcoord_index;

	GLshort verts[MAX_CHARS * 4 * 2];
	int vert_index;

	GLbyte colors[MAX_CHARS * 4 * 4];
	int color_index;
} r_char_arrays_t;

#define MAX_FILLS 512

// fills (alpha-blended quads) are also batched per frame
typedef struct r_fill_arrays_s {
	GLshort verts[MAX_FILLS * 4 * 2];
	int vert_index;

	GLbyte colors[MAX_FILLS * 4 * 4];
	int color_index;
} r_fill_arrays_t;

#define MAX_LINES 512

// lines are batched per frame too
typedef struct r_line_arrays_s {
	GLshort verts[MAX_LINES * 2 * 2];
	int vert_index;

	GLbyte colors[MAX_LINES * 2 * 4];
	int color_index;
} r_line_arrays_t;

// pull it all together in one structure
typedef struct r_draw_s {

	r_image_t *cursor;

	// registered fonts
	r_font_t fonts[MAX_FONTS];
	r_font_t *font;

	// actual text colors as ABGR unsigned integers
	unsigned colors[MAX_COLORS];

	r_char_arrays_t char_arrays[MAX_FONTS];
	r_fill_arrays_t fill_arrays;
	r_line_arrays_t line_arrays;

	// hash pics for fast lookup
	hash_table_t hash_table;
} r_draw_t;

r_draw_t r_draw;


/*
 * R_BindFont
 */
void R_BindFont(r_font_t *font){

	if(!font){
		r_draw.font = r_font_medium;
		return;
	}

	r_draw.font = font;
}


/*
 * R_StringWidth
 */
int R_StringWidth(const char *s){
	return strlen(s) * r_draw.font->char_width;
}


/*
 * R_DrawString
 */
int R_DrawString(int x, int y, const char *s, int color){
	return R_DrawSizedString(x, y, s, 999, 999, color);
}


/*
 * R_DrawBytes
 */
int R_DrawBytes(int x, int y, const char *s, size_t size, int color){
	return R_DrawSizedString(x, y, s, size, size, color);
}


/*
 * R_DrawSizedString
 *
 * Draws at most len chars or size bytes of the specified string.  Color escape
 * sequences are not visible chars.  Returns the number of chars drawn.
 */
int R_DrawSizedString(int x, int y, const char *s, size_t len, size_t size, int color){
	int i, j;

	i = j = 0;
	while(*s && i < len && j < size){

		if(IS_COLOR(s)){  // color escapes
			color = *(s + 1) - '0';
			j += 2;
			s += 2;
			continue;
		}

		if(IS_LEGACY_COLOR(s)){  // legacy colors
			color = CON_COLOR_ALT;
			j++;
			s++;
			continue;
		}

		R_DrawChar(x, y, *s, color);
		x += r_draw.font->char_width;  // next char position in line

		i++;
		j++;
		s++;
	}

	return i;
}


/*
 * R_LoadPic
 */
r_image_t *R_LoadPic(const char *name){
	int i;
	r_image_t *image;

	if((image = Hash_Get(&r_draw.hash_table, name)))
		return image;

	for(i = 0; i < MAX_IMAGES; i++){

		if(!cl.image_precache[i])
			break;

		if(!strcmp(name, cl.image_precache[i]->name + 5))
			return cl.image_precache[i];
	}

	image = R_LoadImage(va("pics/%s", name), it_pic);

	if(i < MAX_IMAGES)  // insert to precache
		cl.image_precache[i] = image;

	Hash_Put(&r_draw.hash_table, name, image);

	return image;
}


/*
 * R_DrawImage
 */
static void R_DrawImage(int x, int y, float scale, r_image_t *image){

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
void R_DrawPic(int x, int y, float scale, const char *name){
	r_image_t *pic;

	pic = R_LoadPic(name);

	if(!pic){
		Com_Warn("R_DrawScaledPic: Can't find %s.\n", name);
		return;
	}

	R_DrawImage(x, y, scale, pic);
}


/*
 * R_DrawCursor
 */
void R_DrawCursor(int x, int y){

	x -= (r_draw.cursor->width / 2.0);
	y -= (r_draw.cursor->height / 2.0);

	R_DrawImage(x, y, 1.0, r_draw.cursor);
}


/*
 * R_DrawChar
 */
void R_DrawChar(int x, int y, char c, int color){

	if(y <= -r_draw.font->char_height)
		return;  // totally off screen

	if(c == ' ')
		return;  // small optimization for space

	const int ca = (r_draw.font - r_draw.fonts) / sizeof(r_font_t);
	r_char_arrays_t *chars = &r_draw.char_arrays[ca];

	const int row = (int) c >> 4;
	const int col = (int) c & 15;

	const float frow = row * 0.1250;
	const float fcol = col * 0.0625;

	// resolve ABGR color
	const unsigned *abgr = &r_draw.colors[color & (MAX_COLORS - 1)];

	memcpy(&chars->colors[chars->color_index +  0], abgr, 4);
	memcpy(&chars->colors[chars->color_index +  4], abgr, 4);
	memcpy(&chars->colors[chars->color_index +  8], abgr, 4);
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
void R_DrawChars(void){
	int i;

	for(i = 0; i < MAX_FONTS; i++){
		r_char_arrays_t *chars = &r_draw.char_arrays[i];

		if(!chars->vert_index)
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

	// and revert to the default font
	R_BindFont(r_font_medium);
}


/*
 * R_DrawFill
 *
 * The color can be specified as an index into the palette with positive alpha
 * value for a, or as an RGBA value (32 bit) by passing -1.0 for a.
 */
void R_DrawFill(int x, int y, int w, int h, int c, float a){
	byte color[4];

	if(a > 1.0){
		Com_Warn("R_DrawFill: Bad alpha %f.\n", a);
		return;
	}

	if(a < 0.0){  // RGBA integer
		memcpy(color, &c, 4);
	}
	else {  // palette index
		memcpy(color, &palette[c], sizeof(color));
		color[3] = a * 255;
	}

	// duplicate color data to all 4 verts
	memcpy(&r_draw.fill_arrays.colors[r_draw.fill_arrays.color_index +  0], color, 4);
	memcpy(&r_draw.fill_arrays.colors[r_draw.fill_arrays.color_index +  4], color, 4);
	memcpy(&r_draw.fill_arrays.colors[r_draw.fill_arrays.color_index +  8], color, 4);
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
void R_DrawFills(void){

	if(!r_draw.fill_arrays.vert_index)
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
void R_DrawLine(int x1, int y1, int x2, int y2, int c, float a) {
	byte color[4];

	if(a > 1.0){
		Com_Warn("R_DrawLine: Bad alpha %f.\n", a);
		return;
	}

	if(a < 0.0){  // RGBA integer
		memcpy(color, &c, 4);
	}
	else {  // palette index
		memcpy(color, &palette[c], sizeof(color));
		color[3] = a * 255;
	}

	// duplicate color data to all 4 verts
	memcpy(&r_draw.line_arrays.colors[r_draw.line_arrays.color_index +  0], color, 4);
	memcpy(&r_draw.line_arrays.colors[r_draw.line_arrays.color_index +  4], color, 4);

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
void R_DrawLines(void){

	if(!r_draw.line_arrays.vert_index)
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
void R_FreePics(void){
	Hash_Free(&r_draw.hash_table);
	Hash_Init(&r_draw.hash_table);
}


/*
 * R_InitFont
 *
 * Initializes the specified font face.
 */
static void R_InitFont(r_font_t *font, char *name){

	font->image = R_LoadImage(name, it_font);

	font->char_width = font->image->width / 16;
	font->char_height = font->image->height / 8;
}


/*
 * R_InitDraw
 */
void R_InitDraw(void){

	memset(&r_draw, 0, sizeof(r_draw));

	r_draw.cursor = R_LoadImage("pics/cursor", it_font);

	R_InitFont(r_font_small, "fonts/small");
	R_InitFont(r_font_medium, "fonts/medium");
	R_InitFont(r_font_large, "fonts/large");

	r_draw.font = r_font_medium;

	// set ABGR color values
	r_draw.colors[CON_COLOR_BLACK] 		= 0xff000000;
	r_draw.colors[CON_COLOR_RED] 		= 0xff0000ff;
	r_draw.colors[CON_COLOR_GREEN] 		= 0xff00ff00;
	r_draw.colors[CON_COLOR_YELLOW] 	= 0xff00ffff;
	r_draw.colors[CON_COLOR_BLUE] 		= 0xffff0000;
	r_draw.colors[CON_COLOR_CYAN] 		= 0xffffff00;
	r_draw.colors[CON_COLOR_MAGENTA]	= 0xffff00ff;
	r_draw.colors[CON_COLOR_WHITE] 		= 0xffffffff;

	Hash_Init(&r_draw.hash_table);
}
