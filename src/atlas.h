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

#pragma once

#include <glib.h>
#include <SDL_image.h>

/**
 * @brief An atlas node locates a surface within the atlas.
 */
typedef struct atlas_node_s {

	/**
	 * @brief The surface.
	 */
	SDL_Surface *surface;

	/**
	 * @brief The surface coordinates within the compiled atlas.
	 */
	int32_t x, y;

} atlas_node_t;

/**
 * @brief An atlas efficiently packs multiple surfaces into a single large surface.
 */
typedef struct atlas_s {

	/**
	 * @brief The atlas nodes.
	 */
	GPtrArray *nodes;
} atlas_t;

atlas_t *Atlas_Create(void);
atlas_node_t *Atlas_Insert(atlas_t *atlas, SDL_Surface *surface);
atlas_node_t *Atlas_Find(atlas_t *atlas, SDL_Surface *surface);
int32_t Atlas_Compile(atlas_t *atlas, SDL_Surface *surface, int32_t start);
void Atlas_Destroy(atlas_t *atlas);
