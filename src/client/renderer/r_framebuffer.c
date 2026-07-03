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
 * TODO(#864): the legacy GL framebuffer object is retired. A framebuffer now
 * wraps an ObjectivelyGPU Framebuffer (color + shared depth); the depth pre-pass
 * and main 3D passes render into it for early-Z, and R_DrawPost composites its
 * color into the present framebuffer. The HDR color + MSAA + post/bloom chain
 * and the remaining Blit/Resolve/Read helpers are ported in later steps; those
 * are still stubs.
 */

/**
 * @brief Creates a scene render target: a swapchain-format color attachment and
 * a sampleable D32F depth attachment, shared by the depth pre-pass and the main
 * passes.
 */
r_framebuffer_t R_CreateFramebuffer(int32_t width, int32_t height, int32_t attachments) {

  // Swapchain color format so the 3D pipelines (built against the present
  // framebuffer's format) are compatible with this target.
  const SDL_GPUTextureFormat format = $(r_device.device, getSwapchainTextureFormat);

  // Size to the present framebuffer (device pixels) so the scene renders at full
  // resolution and R_DrawPost composites it 1:1. The requested width/height are
  // logical points; the present size already accounts for pixel density.
  // TODO(#864): honor width/height for auxiliary views (player-model) via a
  // scaled composite pass, and add r_framebuffer_scale here.
  const SDL_Size size = r_device.device->framebuffer
      ? r_device.device->framebuffer->size
      : MakeSize(width, height);

  Framebuffer *framebuffer = $(r_device.device, createFramebuffer, &(GPU_FramebufferCreateInfo) {
    .size = size,
    .colorFormats = { format },
    .numColorTargets = 1,
    .depthFormat = SDL_GPU_TEXTUREFORMAT_D32_FLOAT,
    .sampleCount = SDL_GPU_SAMPLECOUNT_1,
  });

  return (r_framebuffer_t) {
    .name = 1,
    .framebuffer = framebuffer,
    .width = size.w,
    .height = size.h,
    .attachments = attachments,
  };
}

/**
 * @brief
 */
void R_ClearFramebuffer(r_framebuffer_t *framebuffer) {
}

/**
 * @brief
 */
void R_CopyFramebufferAttachment(const r_framebuffer_t *framebuffer, r_attachment_t attachment, uint32_t *texture) {
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
                 int32_t x, int32_t y, int32_t w, int32_t h) {
}

/**
 * @brief
 */
void R_BlitFramebuffer(const r_framebuffer_t *framebuffer, int32_t x, int32_t y, int32_t w, int32_t h) {
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
    release(framebuffer->framebuffer);
    memset(framebuffer, 0, sizeof(*framebuffer));
  }
}
