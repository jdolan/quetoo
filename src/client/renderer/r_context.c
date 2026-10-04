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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include "r_local.h"

#include <Objectively/Resource.h>

RenderContext renderContext;

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

  SDL_SetWindowIcon(renderContext.window, surf);

  SDL_DestroySurface(surf);
}

/**
 * @brief Updates renderer state from the SDL window.
 */
void R_UpdateContext(void) {

  assert(renderContext.window);

  renderContext.windowFlags = SDL_GetWindowFlags(renderContext.window);

  SDL_GetWindowPosition(renderContext.window, &renderContext.windowBounds.x, &renderContext.windowBounds.y);
  SDL_GetWindowSize(renderContext.window, &renderContext.windowBounds.w, &renderContext.windowBounds.h);

  if (!(renderContext.windowFlags & (SDL_WINDOW_FULLSCREEN | SDL_WINDOW_BORDERLESS))) {
    Cvar_ForceSetInteger("r_windowWidth", renderContext.windowBounds.w);
    Cvar_ForceSetInteger("r_windowHeight", renderContext.windowBounds.h);
    r_windowWidth->modified = false;
    r_windowHeight->modified = false;
  }

  renderContext.display = SDL_GetDisplayForWindow(renderContext.window);
  renderContext.displayMode = SDL_GetCurrentDisplayMode(renderContext.display);

  R_UpdateUniforms(NULL);
}

/**
 * @brief Creates the SDL window and render device.
 */
void R_InitContext(void) {
  
  memset(&renderContext, 0, sizeof(renderContext));

  if (SDL_WasInit(SDL_INIT_VIDEO) == 0) {
    if (!SDL_InitSubSystem(SDL_INIT_VIDEO)) {
      Com_Error(ERROR_FATAL, "%s\n", SDL_GetError());
    }
  }

  renderContext.display = SDL_GetPrimaryDisplay();

  SDL_Rect bounds;
  SDL_GetDisplayUsableBounds(renderContext.display, &bounds);

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
      break;
  }

  if ((renderContext.window = SDL_CreateWindow(PACKAGE_STRING, w, h, windowFlags)) == NULL) {
    Com_Error(ERROR_FATAL, "Failed to create window: %s\n", SDL_GetError());
  }

  R_SetWindowIcon();

  SDL_SyncWindow(renderContext.window);

  if (SDL_GetWindowFlags(renderContext.window) & SDL_WINDOW_FULLSCREEN) {

    const SDL_DisplayMode *mode = SDL_GetDesktopDisplayMode(renderContext.display);
    if (mode) {
      if (SDL_SetWindowFullscreenMode(renderContext.window, mode)) {
        SDL_SyncWindow(renderContext.window);
        Com_Print("  Set fullscreen display mode %dx%d@%gHz\n", mode->w, mode->h, mode->refresh_rate);
      } else {
        Com_Warn("Failed to set fullscreen display mode %dx%d@%gHz: %s\n", mode->w, mode->h, mode->refresh_rate, SDL_GetError());
      }
    } else {
      Com_Warn("Failed to query the desktop display mode: %s\n", SDL_GetError());
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

  renderContext.device = $(alloc(RenderDevice), initWithWindow, renderContext.window, driver);
  if (renderContext.device == NULL) {
    Com_Error(ERROR_FATAL, "Failed to create GPU render device: %s\n", SDL_GetError());
  }

  renderContext.device->maxAnisotropy = Clampf(r_anisotropy->value, 0.f, 16.f);

  $$(Resource, addResourceProvider, R_ResourceProvider);

  R_UpdateContext();

  const SDL_GPUTextureFormat format = $(renderContext.device, getSwapchainTextureFormat);

  Framebuffer *framebuffer = $(renderContext.device, createFramebuffer, &(GPU_FramebufferCreateInfo) {
    .size = MakeSize(renderContext.windowBounds.w, renderContext.windowBounds.h),
    .colorAttachments = { { .format = format, .clearColor = { 0.f, 0.f, 0.f, 1.f } } },
    .numColorTargets = 1,
    .sampleCount = SDL_GPU_SAMPLECOUNT_1,
  });

  $(renderContext.device, setFramebuffer, framebuffer);
  release(framebuffer);

  renderContext.nullTexture = $(renderContext.device, createSolidColorTexture, SDL_GPU_TEXTURETYPE_2D, 1, 0xffffffff);
}

/**
 * @brief Releases the render device and destroys the SDL window.
 * @details Releasing a GPU resource only queues its destruction, which the device performs
 * once no submitted work refers to it. The device is drained first, so that everything the
 * renderer released on the way here is destroyed while the device that owns it still lives.
 */
void R_ShutdownContext(void) {

  $(renderContext.device, waitForIdle);

  renderContext.nullTexture = release(renderContext.nullTexture);
  renderContext.device = release(renderContext.device);

  if (renderContext.window) {
    SDL_DestroyWindow(renderContext.window);
    renderContext.window = NULL;
  }

  SDL_QuitSubSystem(SDL_INIT_VIDEO);
}

/**
 * @brief Applies render scale and display pixel density to framebuffer dimensions.
 */
SDL_Size R_FramebufferSize(const SDL_Size size) {

  const float scale = Clampf(r_framebufferScale->value, .125f, 4.f) * renderContext.displayMode->pixel_density;

  return MakeSize(
    Maxi((int32_t) (size.w * scale), 1),
    Maxi((int32_t) (size.h * scale), 1)
  );
}

/**
 * @brief Creates a framebuffer using scaled dimensions and the scene sample count.
 */
Framebuffer *R_CreateFramebuffer(const GPU_FramebufferCreateInfo *info) {

  GPU_FramebufferCreateInfo create = *info;

  create.size = R_FramebufferSize(info->size);
  create.sampleCount = renderSceneSamples;

  return $(renderContext.device, createFramebuffer, &create);
}

/**
 * @brief Releases a framebuffer.
 */
void R_DestroyFramebuffer(Framebuffer *framebuffer) {
  release(framebuffer);
}
