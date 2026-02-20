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
 * @brief Convert error source into a string
*/
static const char *R_Debug_Source(const GLenum source) {
  switch(source) {
    case GL_DEBUG_SOURCE_API:
      return "API";
    case GL_DEBUG_SOURCE_WINDOW_SYSTEM:
      return "Window System";
    case GL_DEBUG_SOURCE_SHADER_COMPILER:
      return "Shader Compiler";
    case GL_DEBUG_SOURCE_THIRD_PARTY:
      return "Third Party";
    case GL_DEBUG_SOURCE_APPLICATION:
      return "Application";
    case GL_DEBUG_SOURCE_OTHER:
    default:
      return "Other";
  }
}

/**
 * @brief Convert error type into a string
*/
static const char *R_Debug_Type(const GLenum type) {
  switch(type) {
    case GL_DEBUG_TYPE_ERROR:
      return "Error";
    case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR:
      return "Deprecated Behaviour";
    case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:
      return "Undefined Behaviour";
    case GL_DEBUG_TYPE_PORTABILITY:
      return "Portability";
    case GL_DEBUG_TYPE_PERFORMANCE:
      return "Performance";
    case GL_DEBUG_TYPE_MARKER:
      return "Marker";
    case GL_DEBUG_TYPE_PUSH_GROUP:
      return "Push Group";
    case GL_DEBUG_TYPE_POP_GROUP:
      return "Pop Group";
    case GL_DEBUG_TYPE_OTHER:
    default:
      return "Other";
  }
}

/**
 * @brief Convert error severity into a string
*/
static const char *R_Debug_Severity(const GLenum severity) {
  switch(severity) {
    case GL_DEBUG_SEVERITY_HIGH:
      return "High";
    case GL_DEBUG_SEVERITY_MEDIUM:
      return "Medium";
    case GL_DEBUG_SEVERITY_LOW:
      return "Low";
    case GL_DEBUG_SEVERITY_NOTIFICATION:
    default:
      return "Notification";
  }
}

/**
 * @brief Callback for OpenGL's debug system.
*/
static void GLAPIENTRY R_Debug_Callback(const GLenum source, const GLenum type, const GLuint id, const GLenum severity, const GLsizei length, const GLchar *message, const void *userParam) {

  char temp[length + 1];
  GString *backtrace = Sys_Backtrace(0, UINT32_MAX);

  if (length > 0) {
    strncpy(temp, message, length);
    temp[length] = 0;
    message = temp;
  } else if (!length) {
    message = "";
  }

  const char *trace = backtrace->str;
  // we have to do this a bit different because it's driver-dependent as
  // to how deep in the call stack this function will be.
  const char *last_gl_func = g_strrstr(backtrace->str, "???");

  if (last_gl_func) {
    last_gl_func = strchr(last_gl_func, '\n') + 1;

    if (last_gl_func) {
      trace = last_gl_func;
      char* c = strchr(last_gl_func, '\n');
      if (c) {
        *c = 0;
      }
    }
  }

  const bool is_fatal = type == GL_DEBUG_TYPE_ERROR || type == GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR || type == GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR;

  if (is_fatal) {
    Com_Warn("^1OpenGL (%s; %s) %s [id %i]: %s\n source: %s\n", R_Debug_Source(source), R_Debug_Severity(severity), R_Debug_Type(type), id, message, trace);

    if (r_get_error->integer == 2) {
      SDL_TriggerBreakpoint();
    }
  } else {
    const int32_t error_relative_severity = (severity == GL_DEBUG_SEVERITY_HIGH) ? 3 : (severity == GL_DEBUG_SEVERITY_MEDIUM) ? 2 : (severity == GL_DEBUG_SEVERITY_LOW) ? 1 : 0;

    if (error_relative_severity >= r_error_level->integer) {
    Com_Warn("OpenGL (%s; %s) %s [id %i]: %s\n source: %s\n", R_Debug_Source(source), R_Debug_Severity(severity), R_Debug_Type(type), id, message, trace);
  }
  }

  g_string_free(backtrace, true);
}

/**
 * @brief Convert error severity into a string
*/
static const char *R_Debug_Error(const GLenum error_code) {
  switch(error_code) {
    case GL_NO_ERROR:
      return "No Error";
    case GL_INVALID_ENUM:
      return "INVALID_ENUM";
    case GL_INVALID_OPERATION:
      return "INVALID_OPERATION";
    case GL_STACK_OVERFLOW:
      return "STACK_OVERFLOW";
    case GL_STACK_UNDERFLOW:
      return "STACK_UNDERFLOW";
    case GL_OUT_OF_MEMORY:
      return "OUT_OF_MEMORY";
    case GL_INVALID_FRAMEBUFFER_OPERATION:
      return "INVALID_FRAMEBUFFER_OPERATION";
    default:
      return va("Unknown Error (%x)", error_code);
  }
}

/**
 * @brief No-op callback
 */
static void R_Debug_GladPostCallbackNull(void *ret, const char *name, GLADapiproc apiproc, int len_args, ...) {
}

/**
 * @brief No-op callback
 */
static void R_Debug_GladPreCallbackNull(const char *name, GLADapiproc apiproc, int len_args, ...) {
}

/**
 * @brief Callback for non-`KHR_debug` GPUs to log debug error sources and callstacks.
 */
void R_Debug_GladPostCallback(void *ret, const char *name, GLADapiproc apiproc, int len_args, ...) {
  const GLenum error_code = glad_glGetError();

  if (error_code != GL_NO_ERROR) {
    GString *backtrace = Sys_Backtrace(0, UINT32_MAX);
    Com_Warn("^1OpenGL (%s): %s\n source: %s\n", name, R_Debug_Error(error_code), backtrace->str);
    g_string_free(backtrace, true);
  
    if (r_get_error->integer == 2) {
      SDL_TriggerBreakpoint();
    }

    r_error_count++;

    if (r_error_count >= r_max_errors->integer) {
      Com_Warn("Too many errors encountered; skipping handler until next error boundary.\n");
      gladUninstallGLDebug();
    }
  }
}

/**
 * @brief Initialize the OpenGL context, returning true on success, false on failure.
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

  SDL_WindowFlags window_flags = SDL_WINDOW_OPENGL;

  if (r_allow_high_dpi->value) {
    window_flags |= SDL_WINDOW_HIGH_PIXEL_DENSITY;
  }

  SDL_DisplayID display_id = SDL_GetPrimaryDisplay();

  int32_t w, h;
  
  const SDL_DisplayMode *mode = SDL_GetDesktopDisplayMode(display_id);
  
  w = mode->w;
  h = mode->h;

  if (r_fullscreen->integer) {
    window_flags |= SDL_WINDOW_FULLSCREEN;
    if (r_fullscreen->integer == 2) {
      window_flags |= SDL_WINDOW_BORDERLESS;
    }
  } else {
    window_flags |= SDL_WINDOW_RESIZABLE;
    w = r_width->integer ?: w;
    h = r_height->integer ?: h;
  }

  Com_Print("  Trying %dx%d..\n", w, h);

  if ((r_context.window = SDL_CreateWindow(PACKAGE_STRING, w, h, window_flags)) == NULL) {
    Com_Error(ERROR_FATAL, "Failed to set video mode: %s\n", SDL_GetError());
  }

  SDL_GetWindowSize(r_context.window, &r_context.w, &r_context.h);
  SDL_GetWindowSizeInPixels(r_context.window, &r_context.pw, &r_context.ph);

  r_context.display = SDL_GetDisplayForWindow(r_context.window);
  r_context.display_mode = SDL_GetCurrentDisplayMode(r_context.display);
  r_context.display_scale = SDL_GetWindowDisplayScale(r_context.window);

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

  if (r_get_error->integer) {
    if (GLAD_GL_KHR_debug) {
      glEnable(GL_DEBUG_OUTPUT);
      glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
      glDebugMessageCallback(R_Debug_Callback, NULL);
      glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, NULL, GL_TRUE);
      gladUninstallGLDebug();
      gladSetGLPostCallback(R_Debug_GladPostCallbackNull);
      gladSetGLPreCallback(R_Debug_GladPreCallbackNull);
    } else {
      Com_Warn("GL_KHR_debug not supported: slower debug handler attached\n");
      gladInstallGLDebug();
      gladSetGLPostCallback(R_Debug_GladPostCallback);
      gladSetGLPreCallback(R_Debug_GladPreCallbackNull);
    }
  } else {
    gladUninstallGLDebug();
    gladSetGLPostCallback(R_Debug_GladPostCallbackNull);
    gladSetGLPreCallback(R_Debug_GladPreCallbackNull);
  }

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
