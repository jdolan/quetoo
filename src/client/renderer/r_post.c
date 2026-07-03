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

#include "r_local.h"

/*
 * TODO(#864): the post-processing pipeline (bloom, tonemap, FXAA, depth
 * resolve) is not yet ported to SDL_gpu. These are stubs preserving the API.
 */

/**
 * @brief Composites the rendered scene into the present framebuffer.
 * @details The 3D passes render into the view's dedicated scene framebuffer; this
 * copies its color into the present framebuffer, over which the UI is then drawn.
 * TODO(#864): replace the 1:1 copy with a scaled/tonemapped fullscreen pass for
 * r_framebuffer_scale and HDR bloom/tonemap.
 */
void R_DrawPost(const r_view_t *view) {

  if (!r_models.world) {
    return; // no 3D scene this frame; the present clear shows through for the UI
  }

  if (!view->framebuffer || !view->framebuffer->framebuffer) {
    return;
  }

  CommandBuffer *commands = r_device.device->commands;
  if (!commands) {
    return;
  }

  Framebuffer *scene = view->framebuffer->framebuffer;
  Framebuffer *present = r_device.device->framebuffer;

  // Sizes should match (the scene FB is created at the present size); guard against
  // a transient mismatch during a resize rather than copying out of bounds.
  if (scene->size.w != present->size.w || scene->size.h != present->size.h) {
    return;
  }

  CopyPass *copy = $(commands, beginCopyPass);
  $(copy, copyTextureToTexture,
    &(SDL_GPUTextureLocation) { .texture = scene->colorTextures[0]->texture },
    &(SDL_GPUTextureLocation) { .texture = present->colorTextures[0]->texture },
    (Uint32) present->size.w, (Uint32) present->size.h, 1, false);
  copy = release(copy);
}

/**
 * @brief
 */
void R_ResolveFramebufferDepth(const r_framebuffer_t *framebuffer) {
}

/**
 * @brief
 */
void R_InitPost(void) {
}

/**
 * @brief
 */
void R_ShutdownPost(void) {
}
