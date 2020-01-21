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

#pragma once

#include "r_types.h"

void R_DrawImage(r_pixel_t x, r_pixel_t y, vec_t scale, const r_image_t *image);
void R_DrawImageRect(r_pixel_t x, r_pixel_t y, r_pixel_t w, r_pixel_t h, const r_image_t *image);
void R_BindFont(const char *name, r_pixel_t *cw, r_pixel_t *ch);
r_pixel_t R_StringWidth(const char *s);
size_t R_DrawString(r_pixel_t x, r_pixel_t y, const char *s, int32_t color);
size_t R_DrawBytes(r_pixel_t x, r_pixel_t y, const char *s, size_t size, int32_t color);
size_t R_DrawSizedString(r_pixel_t x, r_pixel_t y, const char *s, size_t len, size_t size, int32_t color);
void R_DrawChar(r_pixel_t x, r_pixel_t y, char c, int32_t color);
void R_DrawFill(r_pixel_t x, r_pixel_t y, r_pixel_t w, r_pixel_t h, int32_t c, vec_t a);
void R_DrawLines(const r_pixel_t *points, size_t count, int32_t c, vec_t a);
void R_Draw2D(void);

#ifdef __R_LOCAL_H__
void R_InitDraw(void);
void R_ShutdownDraw(void);
#endif /* __R_LOCAL_H__ */
