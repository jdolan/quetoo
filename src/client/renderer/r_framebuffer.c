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
 * @brief Creates a scene render target: a swapchain-format color attachment and
 * a sampleable D32F depth attachment, shared by the depth pre-pass and the main
 * passes.
 * @remarks @p attachments is currently advisory; a color + depth target is always
 * created. Sized to the present framebuffer (device pixels) so the scene renders
 * at full resolution and R_DrawPost composites it 1:1; the requested logical
 * width/height are ignored. TODO(#864): honor them for auxiliary views
 * (player-model) via a scaled composite, and add r_framebuffer_scale here.
 */
Framebuffer *R_CreateFramebuffer(int32_t width, int32_t height, int32_t attachments) {

  const SDL_GPUTextureFormat format = $(r_device.device, getSwapchainTextureFormat);

  const SDL_Size size = r_device.device->framebuffer
      ? r_device.device->framebuffer->size
      : MakeSize(width, height);

  return $(r_device.device, createFramebuffer, &(GPU_FramebufferCreateInfo) {
    .size = size,
    .colorFormats = { format },
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
