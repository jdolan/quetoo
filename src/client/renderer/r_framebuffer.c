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
 * TODO(#864): the present framebuffer (r_context.device->framebuffer, the
 * swapchain-format target the 2D/UI pass and R_DrawPost's composite draw
 * into) is created separately in r_context.c. This factory is for 3D render
 * targets requested by cgame: the main game view's scene framebuffer
 * (Cg_CreateFramebuffer, sized to the window) and the player-model preview
 * (PlayerModelView.c, sized to its widget). Both pass ATTACHMENT_ALL and get
 * the same shape as the main scene FB, since the shared mesh/bsp pipelines
 * are built with that exact target count/format/sample-count baked in.
 */

/**
 * @brief Creates a 3D scene render target: an HDR color attachment, a float
 * depth copy, and a sampleable D32F depth attachment.
 * @remarks @p attachments is currently advisory; a full 2-color+depth target
 * is always created (see the file comment above for why). @p width and
 * @p height are scaled by r_framebuffer_scale, same as the main scene FB --
 * this uniformly lets a reduced render scale also reduce the resolution of
 * secondary 3D views like the player-model preview.
 */
Framebuffer *R_CreateFramebuffer(int32_t width, int32_t height, int32_t attachments) {

  const float scale = Clampf(r_framebuffer_scale->value, .125f, 4.f);

  const SDL_Size size = MakeSize(
    Maxi((int32_t) (width * scale), 1),
    Maxi((int32_t) (height * scale), 1)
  );

  return $(r_context.device, createFramebuffer, &(GPU_FramebufferCreateInfo) {
    .size = size,
    // Color 0: the HDR scene. Color 1: a float depth copy (gl_FragCoord.z, written
    // by the opaque lit shaders) the sprite pass samples for soft particles -- the
    // real depth buffer cannot be sampled under MSAA, but color attachments resolve.
    .colorFormats = { R_SCENE_COLOR_FORMAT, SDL_GPU_TEXTUREFORMAT_R32_FLOAT },
    .numColorTargets = 2,
    .depthFormat = SDL_GPU_TEXTUREFORMAT_D32_FLOAT,
    .sampleCount = r_scene_samples,
  });
}

/**
 * @brief Releases the given scene framebuffer.
 */
void R_DestroyFramebuffer(Framebuffer *framebuffer) {
  release(framebuffer);
}
