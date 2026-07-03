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
 * TODO(#864): a scene framebuffer is now a plain ObjectivelyGPU Framebuffer
 * (color + shared depth); the depth pre-pass and main 3D passes render into it
 * for early-Z, and R_DrawPost composites its color into the present framebuffer.
 * The HDR color + MSAA + post/bloom chain and depth read-back are ported later.
 */

/**
 * @brief Creates a scene render target: an HDR color attachment and a sampleable
 * D32F depth attachment, shared by the depth pre-pass and the main passes.
 * @remarks @p attachments is currently advisory; a color + depth target is always
 * created. The color target is R_SCENE_COLOR_FORMAT (HDR) rather than the swapchain
 * format so lighting can exceed 1.0 and feed bloom; R_DrawPost composites and
 * clamps it into the LDR present framebuffer. Sized to the present framebuffer
 * (device pixels) scaled by r_framebuffer_scale, so the 3D scene renders at a
 * reduced (or increased) resolution and the composite up/downscales it while the
 * UI stays native; the requested logical width/height are ignored. The scene FB is
 * recreated when r_framebuffer_scale changes (R_BeginFrame pushes a pixel-size event).
 */
Framebuffer *R_CreateFramebuffer(int32_t width, int32_t height, int32_t attachments) {

  const SDL_Size present = r_context.device->framebuffer
      ? r_context.device->framebuffer->size
      : MakeSize(width, height);

  const float scale = Clampf(r_framebuffer_scale->value, .125f, 4.f);

  const SDL_Size size = MakeSize(
    Maxi((int32_t) (present.w * scale), 1),
    Maxi((int32_t) (present.h * scale), 1)
  );

  return $(r_context.device, createFramebuffer, &(GPU_FramebufferCreateInfo) {
    .size = size,
    .colorFormats = { R_SCENE_COLOR_FORMAT },
    .numColorTargets = 1,
    .depthFormat = SDL_GPU_TEXTUREFORMAT_D32_FLOAT,
    .sampleCount = SDL_GPU_SAMPLECOUNT_1,
  });
}

/**
 * @brief Releases the given scene framebuffer.
 */
void R_DestroyFramebuffer(Framebuffer *framebuffer) {
  release(framebuffer);
}
