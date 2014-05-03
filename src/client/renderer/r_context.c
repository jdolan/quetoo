/*
 * Copyright(c) 1997-2001 id Software, Inc.
 * Copyright(c) 2002 The Quakeforge Project.
 * Copyright(c) 2006 Quake2World.
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

/*
 * @brief
 */
static void R_SetIcon(void) {
	SDL_Surface *surf;

	if (!Img_LoadImage("pics/icon", &surf))
		return;

	SDL_SetWindowIcon(r_context.window, surf);

	SDL_FreeSurface(surf);
}

/*
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

	uint32_t flags = SDL_WINDOW_SHOWN | SDL_WINDOW_OPENGL /*| SDL_WINDOW_ALLOW_HIGHDPI*/;

	if (r_fullscreen->integer) {
		w = r_width->integer > 0 ? r_width->integer : 0;
		h = r_height->integer > 0 ? r_height->integer : 0;

		flags |= SDL_WINDOW_FULLSCREEN;
	} else {
		w = r_windowed_width->integer > 0 ? r_windowed_width->integer : 0;
		h = r_windowed_height->integer > 0 ? r_windowed_height->integer : 0;

		flags |= SDL_WINDOW_RESIZABLE;
	}

	if ((r_context.window = SDL_CreateWindow(PACKAGE_STRING, SDL_WINDOWPOS_CENTERED,
			SDL_WINDOWPOS_CENTERED, w, h, flags)) == NULL) {
		Com_Error(ERR_FATAL, "Failed to set video mode: %s\n", SDL_GetError());
	}

	if ((r_context.context = SDL_GL_CreateContext(r_context.window)) == NULL) {
		Com_Error(ERR_FATAL, "Failed to create GL context: %s\n", SDL_GetError());
	}

	if (SDL_GL_SetSwapInterval(r_swap_interval->integer) == -1) {
		Com_Warn("Failed to set swap interval %d: %s\n", r_swap_interval->integer, SDL_GetError());
	}

	if (SDL_SetWindowBrightness(r_context.window, r_gamma->value) == -1) {
		Com_Warn("Failed to set gamma %1.1f: %s\n", r_gamma->value, SDL_GetError());
	}

	SDL_GetWindowSize(r_context.window, &w, &h);

	r_context.width = w;
	r_context.height = h;

	r_context.fullscreen = r_fullscreen->integer;

	SDL_GL_GetAttribute(SDL_GL_RED_SIZE, &r_context.red_bits);
	SDL_GL_GetAttribute(SDL_GL_GREEN_SIZE, &r_context.green_bits);
	SDL_GL_GetAttribute(SDL_GL_BLUE_SIZE, &r_context.blue_bits);
	SDL_GL_GetAttribute(SDL_GL_ALPHA_SIZE, &r_context.alpha_bits);

	SDL_GL_GetAttribute(SDL_GL_STENCIL_SIZE, &r_context.stencil_bits);
	SDL_GL_GetAttribute(SDL_GL_DEPTH_SIZE, &r_context.depth_bits);
	SDL_GL_GetAttribute(SDL_GL_DOUBLEBUFFER, &r_context.double_buffer);

	// don't show SDL cursor because the game will draw one
	SDL_ShowCursor(false);

	R_SetIcon();
}

/*
 * @brief
 */
void R_ShutdownContext(void) {

	if (r_context.context) {
		SDL_GL_DeleteContext(r_context.context);
		r_context.context = NULL;
	}

	if (r_context.window) {
		SDL_DestroyWindow(r_context.window);
		r_context.context = NULL;
	}

	if (SDL_WasInit(SDL_INIT_EVERYTHING) == SDL_INIT_VIDEO)
		SDL_Quit();
	else
		SDL_QuitSubSystem(SDL_INIT_VIDEO);
}
