/*
 * Copyright(c) 1997-2001 id Software, Inc.
 * Copyright(c) 2002 The Quakeforge Project.
 * Copyright(c) 2006-2011 Quetoo.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#include "r_local.h"

/*
 * TODO(#864): the legacy GL framebuffer object is retired. The scene renders
 * into the device present framebuffer (ObjectivelyGPU) directly, and the
 * dedicated HDR/MSAA scene target + post chain are ported in a later step.
 * These are stubs preserving the API used by the client and cgame; the
 * `r_framebuffer_t` returned by R_CreateFramebuffer carries a non-zero `name`
 * sentinel so consumers that assert/guard on it keep working.
 */

/**
 * @brief
 */
r_framebuffer_t R_CreateFramebuffer(GLint width, GLint height, int32_t attachments) {
  return (r_framebuffer_t) { .name = 1, .width = width, .height = height, .attachments = attachments };
}

/**
 * @brief
 */
void R_ClearFramebuffer(r_framebuffer_t *framebuffer) {
}

/**
 * @brief
 */
void R_CopyFramebufferAttachment(const r_framebuffer_t *framebuffer, r_attachment_t attachment, GLuint *texture) {
}

/**
 * @brief
 */
void R_ResolveFramebuffer(const r_framebuffer_t *framebuffer) {
}

/**
 * @brief
 */
void R_BlitFramebufferAttachment(const r_framebuffer_t *framebuffer,
                 r_attachment_t attachment,
                 GLint x, GLint y, GLint w, GLint h) {
}

/**
 * @brief
 */
void R_BlitFramebuffer(const r_framebuffer_t *framebuffer, GLint x, GLint y, GLint w, GLint h) {
}

/**
 * @brief
 */
void R_ReadFramebufferAttachment(const r_framebuffer_t *framebuffer, r_attachment_t attachment, SDL_Surface **surface) {
}

/**
 * @brief
 */
void R_DestroyFramebuffer(r_framebuffer_t *framebuffer) {

  if (framebuffer) {
    memset(framebuffer, 0, sizeof(*framebuffer));
  }
}
