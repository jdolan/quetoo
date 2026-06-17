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

/**
 * @file
 * @brief Vulkan rendering backend. This is the foundation of the optional Vulkan /
 * RTX renderer; see VULKAN_RTX.md for the architecture and roadmap. The entire
 * translation unit is compiled only when `BUILD_VULKAN` is defined (via the
 * `--enable-vulkan` configure option), so default OpenGL builds are unaffected.
 */

#if BUILD_VULKAN

#include <SDL3/SDL.h>
#include <vulkan/vulkan.h>

/**
 * @brief The Vulkan backend context. Mirrors how `r_context_t` owns the OpenGL
 * context, but for the Vulkan device. Populated by `R_Vk_Init`.
 */
typedef struct {
  /**
   * @brief The Vulkan instance.
   */
  VkInstance instance;

  /**
   * @brief The presentation surface, created from the SDL window.
   */
  VkSurfaceKHR surface;

  /**
   * @brief The selected physical device (GPU).
   */
  VkPhysicalDevice physical_device;

  /**
   * @brief Cached properties of the selected physical device.
   */
  VkPhysicalDeviceProperties physical_device_properties;

  /**
   * @brief The logical device.
   */
  VkDevice device;

  /**
   * @brief The graphics queue family index.
   */
  uint32_t graphics_queue_family;

  /**
   * @brief The presentation queue family index.
   */
  uint32_t present_queue_family;

  /**
   * @brief The graphics queue.
   */
  VkQueue graphics_queue;

  /**
   * @brief The presentation queue.
   */
  VkQueue present_queue;

  /**
   * @brief True if the selected device exposes the hardware ray tracing extensions
   * (`VK_KHR_acceleration_structure` and `VK_KHR_ray_tracing_pipeline`), i.e. the
   * RTX lighting path (phase B) is available on this GPU.
   */
  _Bool rtx;

  /**
   * @brief The number of frames that may be recorded concurrently.
   */
#define R_VK_MAX_FRAMES_IN_FLIGHT 2

  /**
   * @brief The presentation swapchain and its surface format / extent.
   */
  VkSwapchainKHR swapchain;
  VkFormat swapchain_format;
  VkExtent2D swapchain_extent;

  /**
   * @brief The swapchain color images and their views (one per image), and the
   * framebuffers wrapping them for the render pass.
   */
  uint32_t swapchain_image_count;
  VkImage *swapchain_images;
  VkImageView *swapchain_image_views;
  VkFramebuffer *framebuffers;

  /**
   * @brief The render pass used to clear and present a frame (phase 1).
   */
  VkRenderPass render_pass;

  /**
   * @brief The command pool and per-frame primary command buffers.
   */
  VkCommandPool command_pool;
  VkCommandBuffer command_buffers[R_VK_MAX_FRAMES_IN_FLIGHT];

  /**
   * @brief Per-frame synchronization primitives.
   */
  VkSemaphore image_available[R_VK_MAX_FRAMES_IN_FLIGHT];
  VkSemaphore render_finished[R_VK_MAX_FRAMES_IN_FLIGHT];
  VkFence in_flight[R_VK_MAX_FRAMES_IN_FLIGHT];

  /**
   * @brief The frame-in-flight index, in `[0, R_VK_MAX_FRAMES_IN_FLIGHT)`.
   */
  uint32_t current_frame;
} r_vk_t;

extern r_vk_t r_vk;

#if defined(__R_LOCAL_H__)
void R_Vk_Init(SDL_Window *window);
void R_Vk_InitSwapchain(SDL_Window *window);
void R_Vk_DrawClear(float r, float g, float b);
void R_Vk_Shutdown(void);

// r_vk_rtx.c — in-engine hardware ray tracing
_Bool R_Vk_RtxAvailable(void);
_Bool R_Vk_RtxRenderView(const r_view_t *view);
void R_Vk_RtxShutdown(void);

// r_vk_image.c — texture upload and the bindless combined-image-sampler array
uint32_t R_Vk_UploadSurface(SDL_Surface *surface);
uint32_t R_Vk_LoadTexture(const char *path);
uint32_t R_Vk_WhiteTexture(void);
uint32_t R_Vk_TextureCount(void);
VkDescriptorSetLayout R_Vk_TextureSetLayout(void);
VkDescriptorSet R_Vk_TextureSet(void);
void R_Vk_ShutdownImages(void);

// r_draw_2d.c — Vulkan 2D UI pass (defined alongside the GL 2D code)
void R_Vk_InitDraw2D(void);
void R_Vk_Draw2D(VkCommandBuffer cb, uint32_t image_index, _Bool overlay);
void R_Vk_ShutdownDraw2D(void);
#endif /* __R_LOCAL_H__ */

#endif /* BUILD_VULKAN */
