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

#include <assert.h>

#include "atlas.h"

/**
 * @brief GDestroyNotify for atlas nodes.
 */
static void Atlas_FreeNode(gpointer data) {

	atlas_node_t *node = data;

	g_free(node->surfaces);
	g_free(node);
}

/**
 * @brief Creates a new layered atlas.
 */
atlas_t *Atlas_Create(int32_t layers) {

	atlas_t *atlas = g_malloc0(sizeof(*atlas));
	if (atlas) {
		atlas->layers = layers;
		assert(atlas->layers);

		atlas->nodes = g_ptr_array_new_with_free_func(Atlas_FreeNode);
		atlas->tag = -1;
	}

	return atlas;
}

/**
 * @brief Inserts one or more layered surfaces into the atlas.
 * @param atlas The atlas.
 * @param ... A list of layered surfaces to insert. This list must be `atlas->layers` in length.
 * @return The atlas node for the inserted surfaces.
 */
atlas_node_t *Atlas_Insert(atlas_t *atlas, ...) {

	assert(atlas);

	atlas_node_t *node = g_malloc0(sizeof(*node));
	if (node) {
		node->surfaces = g_malloc0(atlas->layers * sizeof(SDL_Surface *));
		assert(node->surfaces);

		node->tag = -1;

		va_list args;
		va_start(args, atlas);

		for (int32_t i = 0; i < atlas->layers; i++) {
			node->surfaces[i] = va_arg(args, SDL_Surface *);
		}

		va_end(args);
		assert(node->surfaces[0]);

		g_ptr_array_add(atlas->nodes, node);
	}

	return node;
}

/**
 * @return The node for `surface` or `NULL`.
 */
atlas_node_t *Atlas_Find(atlas_t *atlas, int32_t layer, SDL_Surface *surface) {

	assert(atlas);
	assert(atlas->layers > layer);

	for (size_t i = 0; i < atlas->nodes->len; i++) {
		atlas_node_t *node = g_ptr_array_index(atlas->nodes, i);
		if (node->surfaces[layer] == surface) {
			return node;
		}
	}

	return NULL;
}

/**
 * @brief GCompareFunc for node sorting. Note that g_ptr_array_sort provides pointers to pointers.
 */
static gint Atlas_NodeComparator(gconstpointer a, gconstpointer b) {

	const SDL_Surface *a_surf = (*(atlas_node_t **) a)->surfaces[0];
	const SDL_Surface *b_surf = (*(atlas_node_t **) b)->surfaces[0];

	return b_surf->h - a_surf->h;
}

/**
 * @brief Compiles they layered `atlas` into the given list of equally sized surfaces.
 * @details If all nodes fit in the given surface(s), `0` is returned. If a node overflows,
 * the index of that node is returned. If a node exceeds the bounds of the output surfaces,
 * `-1` is returned.
 * @param atlas The atlas.
 * @param start The node index to start packing from.
 * @param ... The layered surfaces list to blit nodes to, which must be `atlas->layers` in length.
 * @return `0` on success, or the index of the next `start` node. `-1` on error.
 */
int32_t Atlas_Compile(atlas_t *atlas, int32_t start, ...) {

	assert(atlas);

	SDL_Surface *surfaces[atlas->layers];

	va_list args;
	va_start(args, start);

	for (int32_t i = 0; i < atlas->layers; i++) {
		surfaces[i] = va_arg(args, SDL_Surface *);
	}

	va_end(args);
	assert(surfaces[0]);

	atlas->tag++;

	g_ptr_array_sort(atlas->nodes, Atlas_NodeComparator);

	int32_t x = 0, y = 0, row = 0;

	for (int32_t i = start; i < (int32_t) atlas->nodes->len; i++) {
		atlas_node_t *node = g_ptr_array_index(atlas->nodes, i);

		if (node->surfaces[0]->w > surfaces[0]->w ||
			node->surfaces[0]->h > surfaces[0]->h) {
			return -1;
		}

		if (x + node->surfaces[0]->w > surfaces[0]->w) {
			if (y + node->surfaces[0]->h > surfaces[0]->h) {
				return i;
			}
			x = 0;
			y += row;
			row = 0;
		}

		node->x = x;
		node->y = y;
		node->tag = atlas->tag;

		for (int32_t layer = 0; layer < atlas->layers; layer++) {

			SDL_Surface *src = node->surfaces[layer];
			SDL_Surface *dest = surfaces[layer];

			if (src && dest) {
				SDL_BlitSurface(src, NULL, dest, &(SDL_Rect) {
					node->x, node->y, src->w, src->h
				});
			}
		}

		x += node->surfaces[0]->w;
		row = MAX(row, node->surfaces[0]->h);
	}

	return 0;
}

/**
 * @brief Destroys `atlas`, freeing its memory.
 */
void Atlas_Destroy(atlas_t *atlas) {

	if (atlas) {
		g_ptr_array_free(atlas->nodes, 1);
		g_free(atlas);
	}
}
