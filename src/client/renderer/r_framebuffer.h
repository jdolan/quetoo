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

#ifdef __R_LOCAL_H__
typedef struct {
	const r_framebuffer_t *current_framebuffer;
} r_framebuffer_state_t;

extern r_framebuffer_state_t r_framebuffer_state;

r_framebuffer_t *R_CreateFramebuffer(const char *key);
void R_BindFramebuffer(const r_framebuffer_t *fb);
void R_AttachFramebufferImage(r_framebuffer_t *fb, r_image_t *image);
void R_CreateFramebufferDepthStencilBuffers(r_framebuffer_t *fb);
_Bool R_FramebufferReady(const r_framebuffer_t *fb);

#endif
