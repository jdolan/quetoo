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

void R_BindFont(const char *name, int32_t *cw, int32_t *ch);
void R_SetClippingFrame(int32_t x, int32_t y, int32_t w, int32_t h);
void R_Draw2DChar(int32_t x, int32_t y, char c, const color_t color);
size_t R_Draw2DBytes(int32_t x, int32_t y, const char *s, size_t size, const color_t color);
void R_Draw2DFill(int32_t x, int32_t y, int32_t w, int32_t h, const color_t color);
void R_Draw2DImage(int32_t x, int32_t y, int32_t w, int32_t h, const r_image_t *image, const color_t color);
void R_Draw2DFramebuffer(int32_t x, int32_t y, int32_t w, int32_t h, const Framebuffer *framebuffer, const color_t color);
void R_Draw2DLines(const int32_t *points, size_t count, const color_t color);
size_t R_Draw2DSizedString(int32_t x, int32_t y, const char *s, size_t len, size_t size, const color_t color);
size_t R_Draw2DString(int32_t x, int32_t y, const char *s, const color_t color);
void R_Draw2D(void);
int32_t R_StringWidth(const char *s);

#if defined(__R_LOCAL_H__)
void R_InitDraw2D(void);
void R_ShutdownDraw2D(void);
#endif
