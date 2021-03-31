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

r_framebuffer_t R_CreateFramebuffer(r_pixel_t width, r_pixel_t height, _Bool multisampled);
void R_DestroyFramebuffer(r_framebuffer_t *framebuffer);
void R_BlitFramebuffer(const r_framebuffer_t *framebuffer, r_pixel_t x, r_pixel_t y, r_pixel_t w, r_pixel_t h);

#ifdef __R_LOCAL_H__
#endif /* __R_LOCAL_H__ */
