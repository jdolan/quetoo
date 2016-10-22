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

#include <ObjectivelyMVC/Types.h>

#include "r_local.h"

r_context_t r_context;

/**
 * @brief
 */
static void R_SetWindowIcon(void) {
	SDL_Surface *surf;

	if (!Img_LoadImage("icons/quetoo", &surf))
		return;

	SDL_SetWindowIcon(r_context.window, surf);

	SDL_FreeSurface(surf);
}

/**
 * @brief Attempts to create the specified versioned context, returns true on success and false on failure.
 */
static _Bool R_InitGLContext(const int major, const int minor, const SDL_GLcontextFlag context_flags, const SDL_GLprofile profile) {

	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, major);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, minor);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, profile);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, context_flags);
		
	return (r_context.context = SDL_GL_CreateContext(r_context.window)) != NULL;
}

extern cvar_t *verbose;

/**
 * @brief Initialize the OpenGL context, returning true on success, false on failure.
 */
void R_InitContext(void) {
	int32_t w, h;

	memset(&r_context, 0, sizeof(r_context));

	if (SDL_WasInit(SDL_INIT_VIDEO) == 0) {
		if (SDL_InitSubSystem(SDL_INIT_VIDEO) < 0) {
			Com_Error(ERR_FATAL, "%s\n", SDL_GetError());
		}
	}

	uint32_t flags = SDL_WINDOW_OPENGL;

	if (r_allow_high_dpi->integer) {
		flags |= SDL_WINDOW_ALLOW_HIGHDPI;
	}

	if (r_fullscreen->integer) {
		w = MAX(0, r_width->integer);
		h = MAX(0, r_height->integer);

		if (r_width->integer == 0 && r_height->integer == 0) {
			SDL_DisplayMode best;
			SDL_GetDesktopDisplayMode(0, &best);

			w = best.w;
			h = best.h;
		}
		flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
	} else {
		w = MAX(0, r_windowed_width->integer);
		h = MAX(0, r_windowed_height->integer);

		flags |= SDL_WINDOW_RESIZABLE;
	}

	flags |= SDL_WINDOW_HIDDEN;

	Com_Print("  Trying %dx%d..\n", w, h);

	// set common window parameters
	// (need to be done first)
	SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);

	SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
	
	const int32_t s = Clamp(r_multisample->integer, 0, 8);

	SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, s ? 1 : 0);
	SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, s);

	if ((r_context.window = SDL_CreateWindow(PACKAGE_STRING,
			SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, w, h, flags)) == NULL) {
		Com_Error(ERR_FATAL, "Failed to set video mode: %s\n", SDL_GetError());
	}

	// Prepare GL stuff
	Com_Print("  Setting up OpenGL context..\n");
	
	int default_flags, default_profile;
	
	SDL_GL_GetAttribute(SDL_GL_CONTEXT_FLAGS, &default_flags);
	SDL_GL_GetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, &default_profile);

	// first, try a 3.0 core forward compatible context.
	// this will be the most performant for most implementations
	// since we are not using any fixed pipeline.
	if (!R_InitGLContext(3, 0, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG, SDL_GL_CONTEXT_PROFILE_CORE)) {

		Com_Print("  Failed with 3.0 Forward Compatible Core, (%s) attempting 2.1 Core..\n", SDL_GetError());

		// if that fails, try 2.1 core. Technically, 2.1 does not support "core/compat"
		// but some implementors (OSX for instance) actually respect this flag.
		if (!R_InitGLContext(2, 1, default_flags, SDL_GL_CONTEXT_PROFILE_CORE)) {
			Com_Print("  Failed with 2.1 Core (%s), attempting 2.1..\n", SDL_GetError());
	
			// If all else fails, just try base 2.1.
			if (!R_InitGLContext(2, 1, default_flags, default_profile)) {

				Com_Error(ERR_FATAL, "Failed to create OpenGL context: %s\n", SDL_GetError());
			}
		}
	}
	else {

		Com_Print("  Succeeded with 3.0 Forward Compatible Core.\n");
		r_context.is_core = true;
	}

	SDL_ShowWindow(r_context.window);

	if (verbose->integer) {

		int attribs[SDL_GL_CONTEXT_RELEASE_BEHAVIOR];
		
		for (int i = 0; i < SDL_GL_CONTEXT_RELEASE_BEHAVIOR; ++i)
			SDL_GL_GetAttribute(i, &attribs[i]);

		Com_Print("   Color Buffer: r%ig%ib%ia%i\n", attribs[SDL_GL_RED_SIZE], attribs[SDL_GL_GREEN_SIZE], attribs[SDL_GL_BLUE_SIZE], attribs[SDL_GL_ALPHA_SIZE]);
		Com_Print("   Framebuffer: %i\n", attribs[SDL_GL_BUFFER_SIZE]);
		Com_Print("   Double-buffered: %s\n", attribs[SDL_GL_DOUBLEBUFFER] ? "yes" : "no");
		Com_Print("   Depth Buffer: %i\n", attribs[SDL_GL_DEPTH_SIZE]);
		Com_Print("   Stencil Buffer: %i\n", attribs[SDL_GL_STENCIL_SIZE]);
		Com_Print("   Multisample: %i buffers, %i samples\n", attribs[SDL_GL_MULTISAMPLEBUFFERS], attribs[SDL_GL_MULTISAMPLESAMPLES]);
		Com_Print("   Version: %i.%i (%i flags, %i profile)\n", attribs[SDL_GL_CONTEXT_MAJOR_VERSION], attribs[SDL_GL_CONTEXT_MINOR_VERSION], attribs[SDL_GL_CONTEXT_FLAGS], attribs[SDL_GL_CONTEXT_PROFILE_MASK]);
	}

	if (SDL_GL_SetSwapInterval(r_swap_interval->integer) == -1) {
		Com_Warn("Failed to set swap interval %d: %s\n", r_swap_interval->integer, SDL_GetError());
	}

	if (SDL_SetWindowBrightness(r_context.window, r_gamma->value) == -1) {
		Com_Warn("Failed to set gamma %1.1f: %s\n", r_gamma->value, SDL_GetError());
	}

	int32_t dw, dh;
	SDL_GL_GetDrawableSize(r_context.window, &dw, &dh);

	r_context.width = dw;
	r_context.height = dh;

	int32_t ww, wh;
	SDL_GetWindowSize(r_context.window, &ww, &wh);

	r_context.window_width = ww;
	r_context.window_height = wh;

	r_context.high_dpi = dw > ww && dh > wh;

	r_context.fullscreen = SDL_GetWindowFlags(r_context.window) & SDL_WINDOW_FULLSCREEN;

	R_SetWindowIcon();
}

/**
 * @brief
 */
void R_ShutdownContext(void) {

	if (r_context.context) {
		SDL_GL_DeleteContext(r_context.context);
		r_context.context = NULL;
	}

	if (r_context.window) {
		SDL_DestroyWindow(r_context.window);
		r_context.window = NULL;
	}

	SDL_QuitSubSystem(SDL_INIT_VIDEO);
}
