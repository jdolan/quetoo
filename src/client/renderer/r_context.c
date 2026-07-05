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

r_context_t r_context;

/**
 * @brief Objectively `ResourceProvider` bridging the GPU library's Resource
 * lookups (e.g. `loadShader`) to Quetoo's virtual filesystem.
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
 * @brief Loads and sets the application window icon from `icons/quetoo`.
 */
static void R_SetWindowIcon(void) {

  SDL_Surface *surf = Img_LoadSurface("icons/quetoo");

  if (!surf) {
    return;
  }

  SDL_SetWindowIcon(r_context.window, surf);

  SDL_DestroySurface(surf);
}

/**
 * @brief Updates the renderer device to reflect the current SDL window state and size.
 */
void R_UpdateContext(void) {

  assert(r_context.window);

  r_context.window_flags = SDL_GetWindowFlags(r_context.window);

  SDL_GetWindowPosition(r_context.window, &r_context.window_bounds.x, &r_context.window_bounds.y);
  SDL_GetWindowSize(r_context.window, &r_context.window_bounds.w, &r_context.window_bounds.h);

  if (!(r_context.window_flags & (SDL_WINDOW_FULLSCREEN | SDL_WINDOW_BORDERLESS))) {
    Cvar_ForceSetInteger("r_window_width", r_context.window_bounds.w);
    Cvar_ForceSetInteger("r_window_height", r_context.window_bounds.h);
    r_window_width->modified = false;
    r_window_height->modified = false;
  }

  const float scale = Clampf(r_draw_scale->value, .5f, 4.f);

  r_context.w = r_context.window_bounds.w / scale;
  r_context.h = r_context.window_bounds.h / scale;

  r_context.display = SDL_GetDisplayForWindow(r_context.window);
  r_context.display_mode = SDL_GetCurrentDisplayMode(r_context.display);

  r_context.viewport = (SDL_Rect) {
    0,
    0,
    r_context.window_bounds.w * r_context.display_mode->pixel_density,
    r_context.window_bounds.h * r_context.display_mode->pixel_density
  };

  // TODO(#864): R_UpdateUniforms(NULL) once the uniforms UBO is ported to the GPU device.
}

/**
 * @brief Creates the `SDL_Window` and the ObjectivelyGPU `RenderDevice`.
 */
void R_InitContext(void) {
  
  memset(&r_context, 0, sizeof(r_context));

  if (SDL_WasInit(SDL_INIT_VIDEO) == 0) {
    if (!SDL_InitSubSystem(SDL_INIT_VIDEO)) {
      Com_Error(ERROR_FATAL, "%s\n", SDL_GetError());
    }
  }

  r_context.display = SDL_GetPrimaryDisplay();

  SDL_Rect bounds;
  SDL_GetDisplayUsableBounds(r_context.display, &bounds);

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

  if ((r_context.window = SDL_CreateWindow(PACKAGE_STRING, w, h, window_flags)) == NULL) {
    Com_Error(ERROR_FATAL, "Failed to create window: %s\n", SDL_GetError());
  }

  R_SetWindowIcon();

  SDL_SyncWindow(r_context.window);

  if (SDL_GetWindowFlags(r_context.window) & SDL_WINDOW_FULLSCREEN) {

    if (r_fullscreen_width->integer > 0 && r_fullscreen_height->integer > 0) {

      SDL_DisplayMode mode;
      if (SDL_GetClosestFullscreenDisplayMode(r_context.display, w, h, 0.f, false, &mode)) {
        Com_Print("  Setting fullscreen display mode %dx%d@%gHz\n", mode.w, mode.h, mode.refresh_rate);

        if (SDL_SetWindowFullscreenMode(r_context.window, &mode)) {
          SDL_SyncWindow(r_context.window);
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

  r_context.device = $(alloc(RenderDevice), initWithWindow, r_context.window);
  if (r_context.device == NULL) {
    Com_Error(ERROR_FATAL, "Failed to create GPU render device: %s\n", SDL_GetError());
  }

  Com_Verbose("   GPU driver: %s\n", SDL_GetGPUDeviceDriver(r_context.device->device));

  // Bridge the GPU library's Resource lookups (loadShader, ...) to Quetoo's filesystem.
  $$(Resource, addResourceProvider, R_ResourceProvider);

  R_UpdateContext();

  // The present-target framebuffer, sized to the full device viewport at native
  // resolution. The UI and 2D overlays draw directly into this; the composited
  // 3D scene (its own, separately-scaled framebuffer -- see R_CreateFramebuffer
  // below) is blitted in by R_DrawPost. endFrame presents this to the swapchain.
  const SDL_GPUTextureFormat format = $(r_context.device, getSwapchainTextureFormat);

  Framebuffer *framebuffer = $(r_context.device, createFramebuffer, &(GPU_FramebufferCreateInfo) {
    .size = MakeSize(r_context.viewport.w, r_context.viewport.h),
    .colorFormats = { format },
    .numColorTargets = 1,
    .depthFormat = SDL_GPU_TEXTUREFORMAT_D32_FLOAT,
    .sampleCount = SDL_GPU_SAMPLECOUNT_1,
  });

  $(r_context.device, setFramebuffer, framebuffer);
  release(framebuffer);

  // A 1x1 opaque white texture, shared by every pipeline that needs to bind
  // *something* to a sampler slot it never reads (see the r_context_t field doc).
  r_context.null_texture = $(r_context.device, createTexture, &(SDL_GPUTextureCreateInfo) {
    .type = SDL_GPU_TEXTURETYPE_2D,
    .format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
    .usage = SDL_GPU_TEXTUREUSAGE_SAMPLER,
    .width = 1,
    .height = 1,
    .layer_count_or_depth = 1,
    .num_levels = 1,
  }, &(const uint32_t) { 0xffffffff });
}

/**
 * @brief Releases the `RenderDevice` and destroys the SDL window.
 */
void R_ShutdownContext(void) {

  r_context.null_texture = release(r_context.null_texture);
  r_context.device = release(r_context.device);

  if (r_context.window) {
    SDL_DestroyWindow(r_context.window);
    r_context.window = NULL;
  }

  SDL_QuitSubSystem(SDL_INIT_VIDEO);
}

/**
 * @brief Creates a 3D scene render target: an HDR color attachment, a float
 * depth copy, and a sampleable D32F depth attachment.
 * @remarks @p attachments is currently advisory; a full 2-color+depth target
 * is always created, since the shared mesh/bsp pipelines are built with that
 * exact target count/format/sample-count baked in. @p width and @p height are
 * scaled by r_framebuffer_scale, same as the main scene FB -- this uniformly
 * lets a reduced render scale also reduce the resolution of secondary 3D
 * views like the player-model preview.
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
