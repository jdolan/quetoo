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

r_framebuffer_t R_CreateFramebuffer(GLint width, GLint height, int32_t attachments);
void R_CopyFramebufferAttachment(const r_framebuffer_t *framebuffer, r_attachment_t attachment, GLuint *texture);
void R_BlurFramebufferAttachment(r_framebuffer_t *framebuffer, r_attachment_t attachment, GLuint level, float blur);
void R_BlitFramebuffer(const r_framebuffer_t *framebuffer, GLint x, GLint y, GLint w, GLint h);
void R_ReadFramebufferAttachment(const r_framebuffer_t *framebuffer, r_attachment_t attachment, SDL_Surface **surface);
void R_DestroyFramebuffer(r_framebuffer_t *framebuffer);
void R_InitFramebuffer(void);
void R_ShutdownFramebuffer(void);
#ifdef __R_LOCAL_H__
#endif
