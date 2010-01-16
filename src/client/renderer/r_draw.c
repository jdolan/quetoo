/*
 * Copyright(c) 1997-2001 Id Software, Inc.
 * Copyright(c) 2002 The Quakeforge Project.
 * Copyright(c) 2006 Quake2World.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or(at your option) any later version.
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

#include "../client.h"
#include "../../hash.h"

#define MAX_CHARS 8192

// chars are batched per frame so that they are drawn in one shot
// colors are stored in a color array
float char_texcoords[MAX_CHARS * 4 * 2];
short char_verts[MAX_CHARS * 4 * 2];
byte char_colors[MAX_CHARS * 4 * 4];

// working indices into vertex, texcoord and color arrays
static int vertind, coordind, colorind;

// actual text colors
unsigned colors[MAX_COLORS];

#define MAX_FILLS 512

// fills are also batched per frame using a color array
short fill_verts[MAX_FILLS * 4 * 2];
byte fill_colors[MAX_FILLS * 4 * 4];

// working indices into fill vertex and color arrays
static int fvertind, fcolind;

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
	colors[CON_COLOR_BLACK] 	= 0xFF000000;
	colors[CON_COLOR_RED] 		= 0xFF0000FF;
	colors[CON_COLOR_GREEN] 	= 0xFF00FF00;
	colors[CON_COLOR_YELLOW] 	= 0xFF00FFFF;
	colors[CON_COLOR_BLUE] 		= 0xFFFF0000;
	colors[CON_COLOR_CYAN] 		= 0xFFFFFF00;
	colors[CON_COLOR_MAGENTA] 	= 0xFFFF00FF;
	colors[CON_COLOR_WHITE] 	= 0xFFFFFFFF;

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

	memcpy(&char_colors[colorind + 0], &colors[color], 4);
	memcpy(&char_colors[colorind + 4], &colors[color], 4);
	memcpy(&char_colors[colorind + 8], &colors[color], 4);
	memcpy(&char_colors[colorind + 12], &colors[color], 4);
	colorind += 16;

	char_texcoords[coordind + 0] = fcol;
	char_texcoords[coordind + 1] = frow;

	char_texcoords[coordind + 2] = fcol + 0.0625;
	char_texcoords[coordind + 3] = frow;

	char_texcoords[coordind + 4] = fcol + 0.0625;
	char_texcoords[coordind + 5] = frow + 0.0625;

	char_texcoords[coordind + 6] = fcol;
	char_texcoords[coordind + 7] = frow + 0.0625;
	coordind += 8;

	char_verts[vertind + 0] = x;
	char_verts[vertind + 1] = y;

	char_verts[vertind + 2] = x + 16;
	char_verts[vertind + 3] = y;

	char_verts[vertind + 4] = x + 16;
	char_verts[vertind + 5] = y + 32;

	char_verts[vertind + 6] = x;
	char_verts[vertind + 7] = y + 32;

	vertind += 8;
}


/*
 * R_DrawChars
 */
void R_DrawChars(void){

	R_BindTexture(draw_chars->texnum);

	R_EnableColorArray(true);

	// alter the array pointers
	R_BindArray(GL_COLOR_ARRAY, GL_UNSIGNED_BYTE, char_colors);
	R_BindArray(GL_TEXTURE_COORD_ARRAY, GL_FLOAT, char_texcoords);
	R_BindArray(GL_VERTEX_ARRAY, GL_SHORT, char_verts);

	glDrawArrays(GL_QUADS, 0, vertind / 2);

	coordind = vertind = colorind = 0;

	// and restore them
	R_BindDefaultArray(GL_TEXTURE_COORD_ARRAY);
	R_BindDefaultArray(GL_VERTEX_ARRAY);
	R_BindDefaultArray(GL_COLOR_ARRAY);

	R_EnableColorArray(false);

	// restore draw color
	glColor4ubv(color_white);
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
int R_DrawBytes(int x, int y, const char *s, int size, int color){
	return R_DrawSizedString(x, y, s, size, size, color);
}


/*
 * R_DrawSizedString
 *
 * Draws at most len chars or size bytes of the specified string.  Color escape
 * sequences are not visible chars.  Returns the number of chars drawn.
 */
int R_DrawSizedString(int x, int y, const char *s, int len, int size, int color){
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
 */
void R_DrawFillAlpha(int x, int y, int w, int h, int c, float a){
	byte color[4];

	if(c > 255 || c < 0){
		Com_Warn("R_DrawFillAlpha: Bad color %d.\n", c);
		return;
	}

	if(a > 1.0 || a < 0.0){
		Com_Warn("R_DrawFillAlpha: Bad alpha %f.\n", a);
		return;
	}

	memcpy(&color, &palette[c], sizeof(color));
	color[3] = a * 255;

	// duplicate color data to all 4 verts
	memcpy(&fill_colors[fcolind +  0], color, 4);
	memcpy(&fill_colors[fcolind +  4], color, 4);
	memcpy(&fill_colors[fcolind +  8], color, 4);
	memcpy(&fill_colors[fcolind + 12], color, 4);
	fcolind += 16;

	// populate verts
	fill_verts[fvertind + 0] = x;
	fill_verts[fvertind + 1] = y;

	fill_verts[fvertind + 2] = x + w;
	fill_verts[fvertind + 3] = y;

	fill_verts[fvertind + 4] = x + w;
	fill_verts[fvertind + 5] = y + h;

	fill_verts[fvertind + 6] = x;
	fill_verts[fvertind + 7] = y + h;
	fvertind += 8;
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
	R_BindArray(GL_VERTEX_ARRAY, GL_SHORT, fill_verts);
	R_BindArray(GL_COLOR_ARRAY, GL_UNSIGNED_BYTE, fill_colors);

	glDrawArrays(GL_QUADS, 0, fvertind / 2);

	// and restore them
	R_BindDefaultArray(GL_VERTEX_ARRAY);
	R_BindDefaultArray(GL_COLOR_ARRAY);

	R_EnableColorArray(false);

	R_EnableTexture(&texunit_diffuse, true);

	fvertind = fcolind = 0;
}
