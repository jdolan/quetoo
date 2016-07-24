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

#ifndef __UI_TYPES_H__
#define __UI_TYPES_H__

#include <SDL2/SDL_events.h>
#include <SDL2/SDL_opengl.h>
#include <SDL2/SDL_render.h>

#include "../renderer/renderer.h"

#ifdef __UI_LOCAL_H__

/**
 * @brief The user interface OpenGL context.
 */
typedef struct {

	/**
	 * @brief The OpenGL 2.1 context, reserved for rendering the user interface.
	 */
	SDL_GLContext *context;

	/**
	 * @brief The SDL renderer.
	 */
	SDL_Renderer *renderer;

	/**
	 * @brief The SDL renderer target.
	 */
	SDL_Texture *texture;

	/**
	 * @brief An image container to blit the rendered texture.
	 */
	r_image_t image;

} ui_context_t;

extern ui_context_t ui_context;

#endif /* __UI_LOCAL_H__ */

#endif /* __UI_TYPES_H__ */
