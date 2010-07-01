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

// actual text colors as ABGR unsigned integers
unsigned r_char_colors[MAX_COLORS];

#define MAX_CHARS 8192

// characters are batched per frame and drawn in one shot
// accumulate coordinates and colors as vertex arrays
typedef struct char_arrays_s {
	GLfloat texcoords[MAX_CHARS * 4 * 2];
	int texcoord_index;

	GLshort verts[MAX_CHARS * 4 * 2];
	int vert_index;

	GLbyte colors[MAX_CHARS * 4 * 4];
	int color_index;

} char_arrays_t;

static char_arrays_t r_char_arrays;

#define MAX_FILLS 512

// fills (alpha-blended quads) are also batched per frame
typedef struct fill_arrays_s {
	GLshort verts[MAX_FILLS * 4 * 2];
	int vert_index;

	GLbyte colors[MAX_FILLS * 4 * 4];
	int color_index;
} fill_arrays_t;

static fill_arrays_t r_fill_arrays;

// hash pics for fast lookup
hashtable_t pics_hashtable;

// chars
image_t *draw_chars;

/*
 * R_InitDraw
 */
void R_InitDraw(void){

	draw_chars = R_LoadImage("pics/conchars", it_chars);

	// set ABGR color values
	r_char_colors[CON_COLOR_BLACK] 		= 0xFF000000;
	r_char_colors[CON_COLOR_RED] 		= 0xFF0000FF;
	r_char_colors[CON_COLOR_GREEN] 		= 0xFF00FF00;
	r_char_colors[CON_COLOR_YELLOW] 	= 0xFF00FFFF;
	r_char_colors[CON_COLOR_BLUE] 		= 0xFFFF0000;
	r_char_colors[CON_COLOR_CYAN] 		= 0xFFFFFF00;
	r_char_colors[CON_COLOR_MAGENTA]	= 0xFFFF00FF;
	r_char_colors[CON_COLOR_WHITE] 		= 0xFFFFFFFF;

	Com_HashInit(&pics_hashtable);
}


/*
 * R_DrawChar
 */
void R_DrawChar(int x, int y, char c, int color){
	int row, col;
	float frow, fcol;

	if(y <= -32)
		return;  // totally off screen

	if(c == ' ')
		return;  // small optimization for space

	color = color & (MAX_COLORS - 1);	// resolve color array

	row = (int) c >> 4;
	col = (int) c & 15;

	frow = row * 0.0625;
	fcol = col * 0.0625;

	memcpy(&r_char_arrays.colors[r_char_arrays.color_index + 0], &r_char_colors[color], 4);
	memcpy(&r_char_arrays.colors[r_char_arrays.color_index + 4], &r_char_colors[color], 4);
	memcpy(&r_char_arrays.colors[r_char_arrays.color_index + 8], &r_char_colors[color], 4);
	memcpy(&r_char_arrays.colors[r_char_arrays.color_index + 12], &r_char_colors[color], 4);

	r_char_arrays.color_index += 16;

	r_char_arrays.texcoords[r_char_arrays.texcoord_index + 0] = fcol;
	r_char_arrays.texcoords[r_char_arrays.texcoord_index + 1] = frow;

	r_char_arrays.texcoords[r_char_arrays.texcoord_index + 2] = fcol + 0.0625;
	r_char_arrays.texcoords[r_char_arrays.texcoord_index + 3] = frow;

	r_char_arrays.texcoords[r_char_arrays.texcoord_index + 4] = fcol + 0.0625;
	r_char_arrays.texcoords[r_char_arrays.texcoord_index + 5] = frow + 0.0625;

	r_char_arrays.texcoords[r_char_arrays.texcoord_index + 6] = fcol;
	r_char_arrays.texcoords[r_char_arrays.texcoord_index + 7] = frow + 0.0625;

	r_char_arrays.texcoord_index += 8;

	r_char_arrays.verts[r_char_arrays.vert_index + 0] = x;
	r_char_arrays.verts[r_char_arrays.vert_index + 1] = y;

	r_char_arrays.verts[r_char_arrays.vert_index + 2] = x + 16;
	r_char_arrays.verts[r_char_arrays.vert_index + 3] = y;

	r_char_arrays.verts[r_char_arrays.vert_index + 4] = x + 16;
	r_char_arrays.verts[r_char_arrays.vert_index + 5] = y + 32;

	r_char_arrays.verts[r_char_arrays.vert_index + 6] = x;
	r_char_arrays.verts[r_char_arrays.vert_index + 7] = y + 32;

	r_char_arrays.vert_index += 8;
}


/*
 * R_DrawChars
 */
void R_DrawChars(void){

	R_BindTexture(draw_chars->texnum);

	R_EnableColorArray(true);

	// alter the array pointers
	R_BindArray(GL_COLOR_ARRAY, GL_UNSIGNED_BYTE, r_char_arrays.colors);
	R_BindArray(GL_TEXTURE_COORD_ARRAY, GL_FLOAT, r_char_arrays.texcoords);
	R_BindArray(GL_VERTEX_ARRAY, GL_SHORT, r_char_arrays.verts);

	glDrawArrays(GL_QUADS, 0, r_char_arrays.vert_index / 2);

	r_char_arrays.color_index = 0;
	r_char_arrays.texcoord_index = 0;
	r_char_arrays.vert_index = 0;

	// and restore them
	R_BindDefaultArray(GL_TEXTURE_COORD_ARRAY);
	R_BindDefaultArray(GL_VERTEX_ARRAY);
	R_BindDefaultArray(GL_COLOR_ARRAY);

	R_EnableColorArray(false);

	// restore draw color
	glColor4ubv(color_white);
}


/*
 * R_StringWidth
 */
int R_StringWidth(const char *s){
	return strlen(s) * 16;
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
		x += 16;  // next char position in line

		i++;
		j++;
		s++;
	}

	return i;
}


/*
 * R_FreePics
 */
void R_FreePics(void){
	Com_HashFree(&pics_hashtable);
	Com_HashInit(&pics_hashtable);
}


/*
 * R_LoadPic
 */
image_t *R_LoadPic(const char *name){
	int i;
	image_t *image;

	if((image = Com_HashValue(&pics_hashtable, name)))
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

	Com_HashInsert(&pics_hashtable, name, image);

	return image;
}


/*
 * R_DrawScaledPic
 */
void R_DrawScaledPic(int x, int y, float scale, const char *name){
	image_t *pic;

	pic = R_LoadPic(name);
	if(!pic){
		Com_Warn("R_DrawScaledPic: Can't find %s.\n", name);
		return;
	}

	R_BindTexture(pic->texnum);

	// our texcoords are already setup, just set verts and draw

	r_state.vertex_array_2d[0] = x;
	r_state.vertex_array_2d[1] = y;

	r_state.vertex_array_2d[2] = x + pic->width * scale;
	r_state.vertex_array_2d[3] = y;

	r_state.vertex_array_2d[4] = x + pic->width * scale;
	r_state.vertex_array_2d[5] = y + pic->height * scale;

	r_state.vertex_array_2d[6] = x;
	r_state.vertex_array_2d[7] = y + pic->height * scale;

	glDrawArrays(GL_QUADS, 0, 4);
}


/*
 * R_DrawPic
 */
void R_DrawPic(int x, int y, const char *name){
	R_DrawScaledPic(x, y, 1.0, name);
}


/*
 * R_DrawFillAlpha
 *
 * The color can be specified as an index into the palette with positive alpha
 * value for a, or as an RGBA value (32 bit) by passing -1.0 for a.
 */
void R_DrawFillAlpha(int x, int y, int w, int h, int c, float a){
	byte color[4];

	if(a > 1.0){
		Com_Warn("R_DrawFillAlpha: Bad alpha %f.\n", a);
		return;
	}

	if(a < 0.0){  // RGBA integer
		memcpy(&color, &c, 4);
	}
	else {  // palette index
		memcpy(&color, &palette[c], sizeof(color));
		color[3] = a * 255;
	}

	// duplicate color data to all 4 verts
	memcpy(&r_fill_arrays.colors[r_fill_arrays.color_index +  0], color, 4);
	memcpy(&r_fill_arrays.colors[r_fill_arrays.color_index +  4], color, 4);
	memcpy(&r_fill_arrays.colors[r_fill_arrays.color_index +  8], color, 4);
	memcpy(&r_fill_arrays.colors[r_fill_arrays.color_index + 12], color, 4);
	r_fill_arrays.color_index += 16;

	// populate verts
	r_fill_arrays.verts[r_fill_arrays.vert_index + 0] = x;
	r_fill_arrays.verts[r_fill_arrays.vert_index + 1] = y;

	r_fill_arrays.verts[r_fill_arrays.vert_index + 2] = x + w;
	r_fill_arrays.verts[r_fill_arrays.vert_index + 3] = y;

	r_fill_arrays.verts[r_fill_arrays.vert_index + 4] = x + w;
	r_fill_arrays.verts[r_fill_arrays.vert_index + 5] = y + h;

	r_fill_arrays.verts[r_fill_arrays.vert_index + 6] = x;
	r_fill_arrays.verts[r_fill_arrays.vert_index + 7] = y + h;
	r_fill_arrays.vert_index += 8;
}


/*
 * R_DrawFill
 *
 * Fills a box of pixels with a single color
 */
void R_DrawFill(int x, int y, int w, int h, int c){
	R_DrawFillAlpha(x, y, w, h, c, 1.0);
}


/*
 * R_DrawFillAlphas
 */
void R_DrawFillAlphas(void){

	R_EnableTexture(&texunit_diffuse, false);

	R_EnableColorArray(true);

	// alter the array pointers
	R_BindArray(GL_VERTEX_ARRAY, GL_SHORT, r_fill_arrays.verts);
	R_BindArray(GL_COLOR_ARRAY, GL_UNSIGNED_BYTE, r_fill_arrays.colors);

	glDrawArrays(GL_QUADS, 0, r_fill_arrays.vert_index / 2);

	// and restore them
	R_BindDefaultArray(GL_VERTEX_ARRAY);
	R_BindDefaultArray(GL_COLOR_ARRAY);

	R_EnableColorArray(false);

	R_EnableTexture(&texunit_diffuse, true);

	r_fill_arrays.vert_index = r_fill_arrays.color_index = 0;
}


/**
 * @brief Draws a rect to the screen. Also has support for stippled rendering of the rect.
 *
 * @param[in] x X-position of the rect
 * @param[in] y Y-position of the rect
 * @param[in] w Width of the rect
 * @param[in] h Height of the rect
 * @param[in] color RGBA color of the rect
 * @param[in] lineWidth Line strength in pixel of the rect
 * @param[in] pattern Specifies a 16-bit integer whose bit pattern determines
 * which fragments of a line will be drawn when the line is rasterized.
 * Bit zero is used first; the default pattern is all 1's
 * @note The stipple factor is @c 2 for this function
 */
void R_DrawRect (int x, int y, int w, int h, const vec4_t color,
		float lineWidth, int pattern)
{
	const float nx = x * r_state.rx;
	const float ny = y * r_state.ry;
	const float nw = w * r_state.rx;
	const float nh = h * r_state.ry;

	glColor4fv(color);

	glDisable(GL_TEXTURE_2D);
	glLineWidth(lineWidth);
	glLineStipple(2, pattern);
	glEnable(GL_LINE_STIPPLE);

	glBegin(GL_LINE_LOOP);
	glVertex2f(nx, ny);
	glVertex2f(nx + nw, ny);
	glVertex2f(nx + nw, ny + nh);
	glVertex2f(nx, ny + nh);
	glEnd();

	glEnable(GL_TEXTURE_2D);
	glLineWidth(1.0f);
	glDisable(GL_LINE_STIPPLE);

	// restore draw color
	glColor4ubv(color_white);
}
