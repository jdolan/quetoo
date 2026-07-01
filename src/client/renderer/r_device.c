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

r_device_t r_device;

/**
 * @brief Loads and sets the application window icon from `icons/quetoo`.
 */
static void R_SetWindowIcon(void) {

  SDL_Surface *surf = Img_LoadSurface("icons/quetoo");

  if (!surf) {
    return;
  }

  SDL_SetWindowIcon(r_device.window, surf);

  SDL_DestroySurface(surf);
}

/**
 * @brief Updates the renderer device to reflect the current SDL window state and size.
 */
void R_UpdateDevice(void) {

  assert(r_device.window);

  r_device.window_flags = SDL_GetWindowFlags(r_device.window);

  SDL_GetWindowPosition(r_device.window, &r_device.window_bounds.x, &r_device.window_bounds.y);
  SDL_GetWindowSize(r_device.window, &r_device.window_bounds.w, &r_device.window_bounds.h);

  if (!(r_device.window_flags & (SDL_WINDOW_FULLSCREEN | SDL_WINDOW_BORDERLESS))) {
    Cvar_ForceSetInteger("r_window_width", r_device.window_bounds.w);
    Cvar_ForceSetInteger("r_window_height", r_device.window_bounds.h);
    r_window_width->modified = false;
    r_window_height->modified = false;
  }

  const float scale = Clampf(r_draw_scale->value, .5f, 4.f);

  r_device.w = r_device.window_bounds.w / scale;
  r_device.h = r_device.window_bounds.h / scale;

  r_device.display = SDL_GetDisplayForWindow(r_device.window);
  r_device.display_mode = SDL_GetCurrentDisplayMode(r_device.display);

  r_device.viewport = (SDL_Rect) {
    0,
    0,
    r_device.window_bounds.w * r_device.display_mode->pixel_density,
    r_device.window_bounds.h * r_device.display_mode->pixel_density
  };

  // TODO(#864): R_UpdateUniforms(NULL) once the uniforms UBO is ported to the GPU device.
}

/**
 * @brief Creates the `SDL_Window` and the ObjectivelyGPU `RenderDevice`.
 */
void R_InitDevice(void) {

#if __APPLE__
  // Stop Xcode from launching multiple instances of the application
  // https://developer.apple.com/forums/thread/765445
  usleep(500000);
#endif

  memset(&r_device, 0, sizeof(r_device));

  if (SDL_WasInit(SDL_INIT_VIDEO) == 0) {
    if (!SDL_InitSubSystem(SDL_INIT_VIDEO)) {
      Com_Error(ERROR_FATAL, "%s\n", SDL_GetError());
    }
  }

  r_device.display = SDL_GetPrimaryDisplay();

  SDL_Rect bounds;
  SDL_GetDisplayUsableBounds(r_device.display, &bounds);

  int32_t w = bounds.w;
  int32_t h = bounds.h;

  SDL_WindowFlags window_flags = SDL_WINDOW_HIGH_PIXEL_DENSITY;

  switch (r_fullscreen->integer) {
    case 0:
      window_flags |= SDL_WINDOW_RESIZABLE;
      w = r_window_width->integer ?: w;
      h = r_window_height->integer ?: h;
      break;
    case 1:
      window_flags |= SDL_WINDOW_BORDERLESS;
      break;
    case 2:
      window_flags |= SDL_WINDOW_FULLSCREEN;
      w = r_fullscreen_width->integer ?: w;
      h = r_fullscreen_height->integer ?: h;
      break;
  }

  if ((r_device.window = SDL_CreateWindow(PACKAGE_STRING, w, h, window_flags)) == NULL) {
    Com_Error(ERROR_FATAL, "Failed to create window: %s\n", SDL_GetError());
  }

  R_SetWindowIcon();

  SDL_SyncWindow(r_device.window);

  if (SDL_GetWindowFlags(r_device.window) & SDL_WINDOW_FULLSCREEN) {

    if (r_fullscreen_width->integer > 0 && r_fullscreen_height->integer > 0) {

      SDL_DisplayMode mode;
      if (SDL_GetClosestFullscreenDisplayMode(r_device.display, w, h, 0.f, false, &mode)) {
        Com_Print("  Setting fullscreen display mode %dx%d@%gHz\n", mode.w, mode.h, mode.refresh_rate);

        if (SDL_SetWindowFullscreenMode(r_device.window, &mode)) {
          SDL_SyncWindow(r_device.window);
          Com_Print("  Set fullscreen display mode %dx%d@%gHz\n", mode.w, mode.h, mode.refresh_rate);
        } else {
          Com_Warn("Failed to set fullscreen display mode %dx%d@%gHz\n", mode.w, mode.h, mode.refresh_rate);
        }
      } else {
        Com_Warn("No matching fullscreen display mode found for %dx%d\n", w, h);
      }
    }
  }

  Com_Print("  Creating GPU render device..\n");

  r_device.device = $(alloc(RenderDevice), initWithWindow, r_device.window);
  if (r_device.device == NULL) {
    Com_Error(ERROR_FATAL, "Failed to create GPU render device: %s\n", SDL_GetError());
  }

  Com_Verbose("   GPU driver: %s\n", SDL_GetGPUDeviceDriver(r_device.device->device));

  R_UpdateDevice();

  // The present-target framebuffer. The UI (and, later, the resolved 3D scene)
  // composite into this; RenderDevice::endFrame blits it to the swapchain.
  const SDL_GPUTextureFormat format = $(r_device.device, getSwapchainTextureFormat);

  Framebuffer *framebuffer = $(r_device.device, createFramebuffer, &(GPU_FramebufferCreateInfo) {
    .size = MakeSize(r_device.viewport.w, r_device.viewport.h),
    .colorFormats = { format },
    .numColorTargets = 1,
    .depthFormat = SDL_GPU_TEXTUREFORMAT_INVALID,
    .sampleCount = SDL_GPU_SAMPLECOUNT_1,
  });

  $(r_device.device, setFramebuffer, framebuffer);
  release(framebuffer);
}

/**
 * @brief Releases the `RenderDevice` and destroys the SDL window.
 */
void R_ShutdownDevice(void) {

  r_device.device = release(r_device.device);

  if (r_device.window) {
    SDL_DestroyWindow(r_device.window);
    r_device.window = NULL;
  }

  SDL_QuitSubSystem(SDL_INIT_VIDEO);
}
