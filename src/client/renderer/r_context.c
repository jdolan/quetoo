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

r_context_t r_context;

/**
 * @brief
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
 * @brief
 */
void R_UpdateContext(void) {

  r_context.window = SDL_GL_GetCurrentWindow();
  assert(r_context.window);
  
  r_context.window_flags = SDL_GetWindowFlags(r_context.window);
  
  SDL_GetWindowPosition(r_context.window, &r_context.window_bounds.x, &r_context.window_bounds.y);
  SDL_GetWindowSize(r_context.window, &r_context.window_bounds.w, &r_context.window_bounds.h);

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
    
  R_UpdateUniforms(NULL);
}

/**
 * @brief Initialize the `SDL_Window` and OpenGL context, returning true on success, false on failure.
 */
void R_InitContext(void) {

#if __APPLE__
  // Stop Xcode from launching multiple instances of the application
  // https://developer.apple.com/forums/thread/765445
  usleep(500000);
#endif

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
  
  SDL_WindowFlags window_flags = SDL_WINDOW_OPENGL | SDL_WINDOW_HIGH_PIXEL_DENSITY;

  switch (r_fullscreen->integer) {
    case 0:
      window_flags |= SDL_WINDOW_RESIZABLE;
      w = Maxi(r_window_width->integer, w);
      h = Maxi(r_window_height->integer, h);
      break;
    case 1:
      window_flags |= SDL_WINDOW_BORDERLESS;
      break;
    case 2:
      window_flags |= SDL_WINDOW_FULLSCREEN;
      break;
  }

  Com_Print("  Trying %dx%d..\n", w, h);

  if ((r_context.window = SDL_CreateWindow(PACKAGE_STRING, w, h, window_flags)) == NULL) {
    Com_Error(ERROR_FATAL, "Failed to set video mode: %s\n", SDL_GetError());
  }
  
  R_SetWindowIcon();
  
  Com_Print("  Setting up OpenGL context..\n");

  SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
  SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
  SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
  SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);
  SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 32);

  SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

  SDL_GLContextFlag context_flags = SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG;

  if (r_get_error->integer) {
    context_flags |= SDL_GL_CONTEXT_DEBUG_FLAG;
  }

  SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, context_flags);

  if ((r_context.context = SDL_GL_CreateContext(r_context.window)) == NULL) {
    Com_Warn("Failed to create 32 bit OpenGL context: %s\n", SDL_GetError());

    SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 0);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

    if ((r_context.context = SDL_GL_CreateContext(r_context.window)) == NULL) {
      Com_Error(ERROR_FATAL, "Failed to create 24 bit OpenGL context: %s\n", SDL_GetError());
    }
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

  gladLoaderLoadGL();
  
  R_UpdateContext();

  glDepthFunc(GL_LEQUAL);

  R_GetError(NULL);
}

/**
 * @brief
 */
void R_ShutdownContext(void) {

  if (r_context.context) {
    SDL_GL_DestroyContext(r_context.context);
    r_context.context = NULL;
  }

  if (r_context.window) {
    SDL_DestroyWindow(r_context.window);
    r_context.window = NULL;
  }

  SDL_QuitSubSystem(SDL_INIT_VIDEO);

  gladLoaderUnloadGL();
}
