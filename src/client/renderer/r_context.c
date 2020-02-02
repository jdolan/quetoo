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
	SDL_Surface *surf;

	if (!Img_LoadImage("icons/quetoo", &surf)) {
		return;
	}

	SDL_SetWindowIcon(r_context.window, surf);

	SDL_FreeSurface(surf);
}

/**
 * @brief Initialize the OpenGL context, returning true on success, false on failure.
 */
void R_InitContext(void) {

	memset(&r_context, 0, sizeof(r_context));

	if (SDL_WasInit(SDL_INIT_VIDEO) == 0) {
		if (SDL_InitSubSystem(SDL_INIT_VIDEO) < 0) {
			Com_Error(ERROR_FATAL, "%s\n", SDL_GetError());
		}
	}

	uint32_t flags = SDL_WINDOW_OPENGL;

	const int display = Clamp(r_display->integer, 0, SDL_GetNumVideoDisplays() - 1);

	if (r_allow_high_dpi->integer) {
		flags |= SDL_WINDOW_ALLOW_HIGHDPI;
	}

	int32_t w = Max(0, r_width->integer);
	int32_t h = Max(0, r_height->integer);

	if (w == 0 || h == 0) {
		SDL_DisplayMode best;
		SDL_GetDesktopDisplayMode(display, &best);

		w = best.w;
		h = best.h;
	}

	if (r_fullscreen->integer) {
		if (r_fullscreen->integer == 2) {
			flags |= SDL_WINDOW_BORDERLESS;
		} else {
			flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
		}
	} else {
		flags |= SDL_WINDOW_RESIZABLE;
	}

	Com_Print("  Trying %dx%d..\n", w, h);

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

	SDL_GL_SetAttribute(SDL_GL_FRAMEBUFFER_SRGB_CAPABLE, 1);

	if ((r_context.window = SDL_CreateWindow(PACKAGE_STRING,
			SDL_WINDOWPOS_CENTERED_DISPLAY(display),
			SDL_WINDOWPOS_CENTERED_DISPLAY(display), w, h, flags)) == NULL) {
		Com_Error(ERROR_FATAL, "Failed to set video mode: %s\n", SDL_GetError());
	}

	Cvar_ForceSetInteger(r_display->name, SDL_GetWindowDisplayIndex(r_context.window));

	Com_Print("  Setting up OpenGL context..\n");

	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG);

	if ((r_context.context = SDL_GL_CreateContext(r_context.window)) == NULL) {
		Com_Error(ERROR_FATAL, "Failed to create OpenGL context: %s\n", SDL_GetError());
	}

	const int32_t valid_attribs[] = {
		SDL_GL_RED_SIZE, SDL_GL_GREEN_SIZE, SDL_GL_BLUE_SIZE, SDL_GL_ALPHA_SIZE,
		SDL_GL_DEPTH_SIZE, SDL_GL_STENCIL_SIZE, SDL_GL_BUFFER_SIZE,
		SDL_GL_DOUBLEBUFFER,
		SDL_GL_MULTISAMPLEBUFFERS, SDL_GL_MULTISAMPLESAMPLES,
		SDL_GL_CONTEXT_MAJOR_VERSION, SDL_GL_CONTEXT_MINOR_VERSION,
		SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_PROFILE_MASK
	};

	int32_t attr[SDL_GL_CONTEXT_RELEASE_BEHAVIOR];
	for (size_t i = 0; i < lengthof(valid_attribs); i++) {
		SDL_GL_GetAttribute(valid_attribs[i], &attr[valid_attribs[i]]);
	}

	Com_Verbose("   Buffer Sizes: r %i g %i b %i a %i depth %i stencil %i framebuffer %i\n",
				attr[SDL_GL_RED_SIZE],
	            attr[SDL_GL_GREEN_SIZE],
				attr[SDL_GL_BLUE_SIZE],
				attr[SDL_GL_ALPHA_SIZE],
				attr[SDL_GL_DEPTH_SIZE],
	            attr[SDL_GL_STENCIL_SIZE],
				attr[SDL_GL_BUFFER_SIZE]);

	Com_Verbose("   Double-buffered: %s\n", attr[SDL_GL_DOUBLEBUFFER] ? "yes" : "no");

	Com_Verbose("   Multisample: %i buffers, %i samples\n",
				attr[SDL_GL_MULTISAMPLEBUFFERS],
	            attr[SDL_GL_MULTISAMPLESAMPLES]);

	Com_Verbose("   Version: %i.%i (%i flags, %i profile)\n",
				attr[SDL_GL_CONTEXT_MAJOR_VERSION],
	            attr[SDL_GL_CONTEXT_MINOR_VERSION],
				attr[SDL_GL_CONTEXT_FLAGS],
				attr[SDL_GL_CONTEXT_PROFILE_MASK]);

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

	gladLoadGL();

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
