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

#ifndef __R_DRAW_H__
#define __R_DRAW_H__

#include "r_image.h"

// each font has its vertex arrays of characters to draw each frame
typedef struct r_font_s {
	r_image_t *image;

	int char_width;
	int char_height;
} r_font_t;

#define MAX_FONTS			3

extern r_font_t *r_font_small;
extern r_font_t *r_font_medium;
extern r_font_t *r_font_large;

// r_draw.c
void R_BindFont(r_font_t *font);
int R_StringWidth(const char *s);
int R_DrawString(int x, int y, const char *s, int color);
int R_DrawBytes(int x, int y, const char *s, size_t size, int color);
int R_DrawSizedString(int x, int y, const char *s, size_t len, size_t size, int color);

r_image_t *R_LoadPic(const char *name);

void R_DrawPic(int x, int y, float scale, const char *name);
void R_DrawCursor(int x, int y);

void R_DrawChar(int x, int y, char c, int color);
void R_DrawChars(void);

void R_DrawFill(int x, int y, int w, int h, int c, float a);
void R_DrawFills(void);

void R_DrawLine(int x1, int y1, int x2, int y2, int c, float a);
void R_DrawLines(void);

void R_FreePics(void);
void R_InitDraw(void);

#endif /* __R_DRAW_H__ */
