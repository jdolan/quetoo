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

void R_BindFont(const char *name, GLint *cw, GLint *ch);
void R_SetClippingFrame(GLint x, GLint y, GLint w, GLint h);
void R_Draw2DChar(GLint x, GLint y, char c, const color_t color);
size_t R_Draw2DBytes(GLint x, GLint y, const char *s, size_t size, const color_t color);
void R_Draw2DFill(GLint x, GLint y, GLint w, GLint h, const color_t color);
void R_Draw2DImage(GLint x, GLint y, GLint w, GLint h, const r_image_t *image, const color_t color);
void R_Draw2DFramebuffer(GLint x, GLint y, GLint w, GLint h, const r_framebuffer_t *framebuffer, const color_t color);
void R_Draw2DLines(const GLint *points, size_t count, const color_t color);
size_t R_Draw2DSizedString(GLint x, GLint y, const char *s, size_t len, size_t size, const color_t color);
size_t R_Draw2DString(GLint x, GLint y, const char *s, const color_t color);
void R_Draw2D(void);
GLint R_StringWidth(const char *s);

#ifdef __R_LOCAL_H__
void R_InitDraw2D(void);
void R_ShutdownDraw2D(void);
#endif
