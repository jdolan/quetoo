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

#include "r_types.h"

r_image_t *R_LoadPic(const char *name);
void R_DrawPic(r_pixel_t x, r_pixel_t y, float scale, const char *name);
void R_DrawCursor(r_pixel_t x, r_pixel_t y);
void R_BindFont(const char *name, r_pixel_t *cw, r_pixel_t *ch);
r_pixel_t R_StringWidth(const char *s);
size_t R_DrawString(r_pixel_t x, r_pixel_t y, const char *s, int color);
size_t R_DrawBytes(r_pixel_t x, r_pixel_t y, const char *s, size_t size, int color);
size_t R_DrawSizedString(r_pixel_t x, r_pixel_t y, const char *s, size_t len, size_t size, int color);
void R_DrawChar(r_pixel_t x, r_pixel_t y, char c, int color);
void R_DrawChars(void);
void R_DrawFill(r_pixel_t x, r_pixel_t y, r_pixel_t w, r_pixel_t h, int c, float a);
void R_DrawFills(void);
void R_DrawLine(r_pixel_t x1, r_pixel_t y1, r_pixel_t x2, r_pixel_t y2, int c, float a);
void R_DrawLines(void);
void R_FreePics(void);

#ifdef __R_LOCAL_H__
void R_InitDraw(void);
#endif /* __R_LOCAL_H__ */

#endif /* __R_DRAW_H__ */
