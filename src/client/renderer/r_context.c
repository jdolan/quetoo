/*
 * Copyright(c) 1997-2001 Id Software, Inc.
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

#include <SDL.h>

#include "renderer.h"

/*
 * R_SetIcon
 */
static void R_SetIcon(void){
	SDL_Surface *surf;

	if(!Img_LoadImage("pics/icon", &surf))
		return;

	SDL_WM_SetIcon(surf, NULL);

	SDL_FreeSurface(surf);
}


/*
 * R_InitContext
 */
boolean_t R_InitContext(int width, int height, boolean_t fullscreen){
	unsigned flags;
	int i;
	SDL_Surface *surface;

	if(SDL_WasInit(SDL_INIT_EVERYTHING) == 0){
		if(SDL_Init(SDL_INIT_VIDEO) < 0){
			Com_Warn("R_InitContext: %s.\n", SDL_GetError());
			return false;
		}
	} else if(SDL_WasInit(SDL_INIT_VIDEO) == 0){
		if(SDL_InitSubSystem(SDL_INIT_VIDEO) < 0){
			Com_Warn("R_InitContext: %s.\n", SDL_GetError());
			return false;
		}
	}

	if(width < 0)
		width = 0;
	if(height < 0)
		height = 0;

	SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);

	SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

	i = r_multisample->integer;

	if(i < 0)
		i = 0;

	SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, i ? 1 : 0);
	SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, i);

	i = r_swap_interval->integer;
	if(i < 0)
		i = 0;
	if(i > 2)
		i = 2;

	SDL_GL_SetAttribute(SDL_GL_SWAP_CONTROL, i);

	flags = SDL_OPENGL;

	if(fullscreen)
		flags |= SDL_FULLSCREEN;
	else
		flags |= SDL_RESIZABLE;

	if((surface = SDL_SetVideoMode(width, height, 0, flags)) == NULL)
		return false;

	r_state.width = surface->w;
	r_state.height = surface->h;

	r_state.fullscreen = fullscreen;

	r_state.virtual_width = surface->w;
	r_state.virtual_height = surface->h;

	r_state.rx = (float)r_state.width / r_state.virtual_width;
	r_state.ry = (float)r_state.height / r_state.virtual_height;

	SDL_GL_GetAttribute(SDL_GL_RED_SIZE, &r_state.red_bits);
	SDL_GL_GetAttribute(SDL_GL_GREEN_SIZE, &r_state.green_bits);
	SDL_GL_GetAttribute(SDL_GL_BLUE_SIZE, &r_state.blue_bits);
	SDL_GL_GetAttribute(SDL_GL_ALPHA_SIZE, &r_state.alpha_bits);

	SDL_GL_GetAttribute(SDL_GL_STENCIL_SIZE, &r_state.stencil_bits);
	SDL_GL_GetAttribute(SDL_GL_DEPTH_SIZE, &r_state.depth_bits);
	SDL_GL_GetAttribute(SDL_GL_DOUBLEBUFFER, &r_state.double_buffer);

	SDL_EnableKeyRepeat(SDL_DEFAULT_REPEAT_DELAY,
			SDL_DEFAULT_REPEAT_INTERVAL);

	SDL_WM_SetCaption("Quake2World", "Quake2World");

	// don't show SDL cursor because the game will draw one
	SDL_ShowCursor(false);
	
	SDL_EnableUNICODE(1);

	R_SetIcon();

	return true;
}


/*
 * R_ShutdownContext
 */
void R_ShutdownContext(void){
	if(SDL_WasInit(SDL_INIT_EVERYTHING) == SDL_INIT_VIDEO)
		SDL_Quit();
	else
		SDL_QuitSubSystem(SDL_INIT_VIDEO);
}
