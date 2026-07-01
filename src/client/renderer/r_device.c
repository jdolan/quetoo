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
 * @brief Updates the renderer context to reflect the current SDL window state and size.
 */
void R_UpdateDevice(void) {

  r_device.window = SDL_GL_GetCurrentWindow();
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
    
  R_UpdateUniforms(NULL);
}

/**
 * @brief Initialize the `SDL_Window` and OpenGL context, returning true on success, false on failure.
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
  
  SDL_WindowFlags window_flags = SDL_WINDOW_OPENGL | SDL_WINDOW_HIGH_PIXEL_DENSITY;

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

  Com_Print("  Setting up OpenGL context..\n");

  SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
  SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
  SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
  SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

  SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

  if ((r_device.context = SDL_GL_CreateContext(r_device.window)) == NULL) {
    Com_Error(ERROR_FATAL, "Failed to create OpenGL context: %s\n", SDL_GetError());
  }

  const SDL_GLAttr attr_names[] = {
    SDL_GL_RED_SIZE,
    SDL_GL_GREEN_SIZE,
    SDL_GL_BLUE_SIZE,
    SDL_GL_ALPHA_SIZE,
    SDL_GL_DEPTH_SIZE,
    SDL_GL_BUFFER_SIZE,
    SDL_GL_DOUBLEBUFFER,
    SDL_GL_CONTEXT_MAJOR_VERSION,
    SDL_GL_CONTEXT_MINOR_VERSION,
    SDL_GL_CONTEXT_FLAGS,
    SDL_GL_CONTEXT_PROFILE_MASK
  };

  int32_t attr_values[SDL_GL_EGL_PLATFORM];
  for (size_t i = 0; i < lengthof(attr_names); i++) {
    SDL_GL_GetAttribute(attr_names[i], &attr_values[attr_names[i]]);
  }

  Com_Verbose("   Buffer Sizes: r %i g %i b %i a %i depth %i framebuffer %i\n",
      attr_values[SDL_GL_RED_SIZE],
      attr_values[SDL_GL_GREEN_SIZE],
      attr_values[SDL_GL_BLUE_SIZE],
      attr_values[SDL_GL_ALPHA_SIZE],
      attr_values[SDL_GL_DEPTH_SIZE],
      attr_values[SDL_GL_BUFFER_SIZE]);

  Com_Verbose("   Double-buffered: %s\n", attr_values[SDL_GL_DOUBLEBUFFER] ? "yes" : "no");

  Com_Verbose("   Version: %i.%i (%i flags, %i profile)\n",
      attr_values[SDL_GL_CONTEXT_MAJOR_VERSION],
      attr_values[SDL_GL_CONTEXT_MINOR_VERSION],
      attr_values[SDL_GL_CONTEXT_FLAGS],
      attr_values[SDL_GL_CONTEXT_PROFILE_MASK]);

  if (!SDL_GL_SetSwapInterval(r_swap_interval->integer)) {
    Com_Warn("Failed to set swap interval %d: %s\n", r_swap_interval->integer, SDL_GetError());
  }

  if (!gladLoaderLoadGL()) {
    Com_Error(ERROR_FATAL, "Failed to load OpenGL functions\n");
  }

  // Drain any stale GL errors left by GLAD's loader probing
  while (glGetError() != GL_NO_ERROR) { }

  R_UpdateDevice();

  glDepthFunc(GL_LEQUAL);

  R_GetError(NULL);
}

/**
 * @brief Destroys the OpenGL context and SDL window.
 */
void R_ShutdownDevice(void) {

  if (r_device.context) {
    SDL_GL_DestroyContext(r_device.context);
    r_device.context = NULL;
  }

  if (r_device.window) {
    SDL_DestroyWindow(r_device.window);
    r_device.window = NULL;
  }

  SDL_QuitSubSystem(SDL_INIT_VIDEO);

  gladLoaderUnloadGL();
}
