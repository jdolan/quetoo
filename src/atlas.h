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
 * @brief An atlas node locates one or more layered surfaces within an atlas.
 */
typedef struct {

	/**
	 * @brief The surfaces, which must all be of the same size. The first surface
	 * (first layer) must not be `NULL`.
	 */
	SDL_Surface **surfaces;

	/**
	 * @brief The node coordinates within the compiled atlas.
	 */
	int32_t x, y;

	/**
	 * @brief The atlas tag at which this node was compiled.
	 */
	int32_t tag;

} atlas_node_t;

/**
 * @brief An atlas efficiently packs multiple surfaces into a single large surface.
 * @details Atlases are 3 dimensional, and can act on layers of surfaces. For example,
 * an atlas that is 2 layers deep can be used to pack lightmaps and deluxemaps into
 * two separate surfaces, where each input node occupies the same coordinates in both
 * surfaces. All layers in a given node must be of the same size.
 */
typedef struct atlas_s {

	/**
	 * @brief The number of layers in the atlas.
	 */
	int32_t layers;

	/**
	 * @brief The atlas nodes.
	 */
	GPtrArray *nodes;

	/**
	 * @brief The iteration identifier, which is written to nodes as they are compiled.
	 */
	int32_t tag;
} atlas_t;

atlas_t *Atlas_Create(int32_t layers);
atlas_node_t *Atlas_Insert(atlas_t *atlas, ...);
atlas_node_t *Atlas_Find(atlas_t *atlas, int32_t layer, SDL_Surface *surface);
int32_t Atlas_Compile(atlas_t *atlas, int32_t start, ...);
void Atlas_Destroy(atlas_t *atlas);
