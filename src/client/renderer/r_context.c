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

#include <Objectively/Resource.h>

RenderContext rContext;

/**
 * @brief Loads Objectively resources from the Quetoo VFS.
 */
static Data *R_ResourceProvider(const char *name) {

  void *buffer;
  const int64_t length = Fs_Load(name, &buffer);
  if (length == -1) {
    return NULL;
  }

  Data *data = $(alloc(Data), initWithBytes, buffer, (size_t) length);

  Fs_Free(buffer);

  return data;
}

/**
 * @brief Sets the application window icon.
 */
static void R_SetWindowIcon(void) {

  SDL_Surface *surf = Img_LoadSurface("icons/quetoo");

  if (!surf) {
    return;
  }

  SDL_SetWindowIcon(rContext.window, surf);

  SDL_DestroySurface(surf);
}

/**
 * @brief Updates renderer state from the SDL window.
 */
void R_UpdateContext(void) {

  assert(rContext.window);

  rContext.windowFlags = SDL_GetWindowFlags(rContext.window);

  SDL_GetWindowPosition(rContext.window, &rContext.windowBounds.x, &rContext.windowBounds.y);
  SDL_GetWindowSize(rContext.window, &rContext.windowBounds.w, &rContext.windowBounds.h);

  if (!(rContext.windowFlags & (SDL_WINDOW_FULLSCREEN | SDL_WINDOW_BORDERLESS))) {
    Cvar_ForceSetInteger("r_windowWidth", rContext.windowBounds.w);
    Cvar_ForceSetInteger("r_windowHeight", rContext.windowBounds.h);
    r_windowWidth->modified = false;
    r_windowHeight->modified = false;
  }

  rContext.display = SDL_GetDisplayForWindow(rContext.window);
  rContext.displayMode = SDL_GetCurrentDisplayMode(rContext.display);

  R_UpdateUniforms(NULL);
}

/**
 * @brief Creates the SDL window and render device.
 */
void R_InitContext(void) {
  
  memset(&rContext, 0, sizeof(rContext));

  if (SDL_WasInit(SDL_INIT_VIDEO) == 0) {
    if (!SDL_InitSubSystem(SDL_INIT_VIDEO)) {
      Com_Error(ERROR_FATAL, "%s\n", SDL_GetError());
    }
  }

  rContext.display = SDL_GetPrimaryDisplay();

  SDL_Rect bounds;
  SDL_GetDisplayUsableBounds(rContext.display, &bounds);

  int32_t w = bounds.w;
  int32_t h = bounds.h;

  SDL_WindowFlags windowFlags = SDL_WINDOW_HIGH_PIXEL_DENSITY;

  switch (r_fullscreen->integer) {
    case 0:
      windowFlags |= SDL_WINDOW_RESIZABLE;
      w = r_windowWidth->integer ?: w;
      h = r_windowHeight->integer ?: h;
      break;
    case 1:
      windowFlags |= SDL_WINDOW_BORDERLESS;
      break;
    case 2:
      windowFlags |= SDL_WINDOW_FULLSCREEN;
      w = r_fullscreenWidth->integer ?: w;
      h = r_fullscreenHeight->integer ?: h;
      break;
  }

  if ((rContext.window = SDL_CreateWindow(PACKAGE_STRING, w, h, windowFlags)) == NULL) {
    Com_Error(ERROR_FATAL, "Failed to create window: %s\n", SDL_GetError());
  }

  R_SetWindowIcon();

  SDL_SyncWindow(rContext.window);

  if (SDL_GetWindowFlags(rContext.window) & SDL_WINDOW_FULLSCREEN) {

    if (r_fullscreenWidth->integer > 0 && r_fullscreenHeight->integer > 0) {

      SDL_DisplayMode mode;
      if (SDL_GetClosestFullscreenDisplayMode(rContext.display, w, h, 0.f, false, &mode)) {
        Com_Print("  Setting fullscreen display mode %dx%d@%gHz\n", mode.w, mode.h, mode.refresh_rate);

        if (SDL_SetWindowFullscreenMode(rContext.window, &mode)) {
          SDL_SyncWindow(rContext.window);
          Com_Print("  Set fullscreen display mode %dx%d@%gHz\n", mode.w, mode.h, mode.refresh_rate);
        } else {
          Com_Warn("Failed to set fullscreen display mode %dx%d@%gHz\n", mode.w, mode.h, mode.refresh_rate);
        }
      } else {
        Com_Warn("No matching fullscreen display mode found for %dx%d\n", w, h);
      }
    }
  }

  const char *driver = NULL;
  if (r_gpuDriver->string[0]) {
    Com_Print("  Forcing GPU driver \"%s\"..\n", r_gpuDriver->string);
    driver = r_gpuDriver->string;
  } else {
#if defined (_WIN32)
    driver = "vulkan";
#endif
  }

  Com_Print("  Creating GPU render device..\n");

  rContext.device = $(alloc(RenderDevice), initWithWindow, rContext.window, driver);
  if (rContext.device == NULL) {
    Com_Error(ERROR_FATAL, "Failed to create GPU render device: %s\n", SDL_GetError());
  }

  rContext.device->maxAnisotropy = Clampf(r_anisotropy->value, 0.f, 16.f);

  $$(Resource, addResourceProvider, R_ResourceProvider);

  R_UpdateContext();

  const SDL_GPUTextureFormat format = $(rContext.device, getSwapchainTextureFormat);

  Framebuffer *framebuffer = $(rContext.device, createFramebuffer, &(GPU_FramebufferCreateInfo) {
    .size = MakeSize(rContext.windowBounds.w, rContext.windowBounds.h),
    .colorAttachments = { { .format = format, .clearColor = { 0.f, 0.f, 0.f, 1.f } } },
    .numColorTargets = 1,
    .sampleCount = SDL_GPU_SAMPLECOUNT_1,
  });

  $(rContext.device, setFramebuffer, framebuffer);
  release(framebuffer);

  rContext.nullTexture = $(rContext.device, createSolidColorTexture, SDL_GPU_TEXTURETYPE_2D, 1, 0xffffffff);
}

/**
 * @brief Releases the render device and destroys the SDL window.
 * @details Releasing a GPU resource only queues its destruction, which the device performs
 * once no submitted work refers to it. The device is drained first, so that everything the
 * renderer released on the way here is destroyed while the device that owns it still lives.
 */
void R_ShutdownContext(void) {

  $(rContext.device, waitForIdle);

  rContext.nullTexture = release(rContext.nullTexture);
  rContext.device = release(rContext.device);

  if (rContext.window) {
    SDL_DestroyWindow(rContext.window);
    rContext.window = NULL;
  }

  SDL_QuitSubSystem(SDL_INIT_VIDEO);
}

/**
 * @brief Creates a framebuffer from @p info using the renderer's scaled size and scene sample count.
 */
Framebuffer *R_CreateFramebuffer(const GPU_FramebufferCreateInfo *info) {

  const float scale = Clampf(r_framebufferScale->value, .125f, 4.f) * rContext.displayMode->pixel_density;

  GPU_FramebufferCreateInfo create = *info;

  create.size = MakeSize(
    Maxi((int32_t) (info->size.w * scale), 1),
    Maxi((int32_t) (info->size.h * scale), 1)
  );

  create.sampleCount = rSceneSamples;

  return $(rContext.device, createFramebuffer, &create);
}

/**
 * @brief Releases a framebuffer.
 */
void R_DestroyFramebuffer(Framebuffer *framebuffer) {
  release(framebuffer);
}
