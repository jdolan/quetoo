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
 * @brief Creates a new atlas.
 */
atlas_t *Atlas_Create(void) {

	atlas_t *atlas = g_malloc0(sizeof(*atlas));
	if (atlas) {
		atlas->nodes = g_ptr_array_new_with_free_func(g_free);
	}

	return atlas;
}

/**
 * @brief Inserts `surface` into `atlas`.
 */
atlas_node_t *Atlas_Insert(atlas_t *atlas, SDL_Surface *surface) {

	atlas_node_t *node = g_malloc0(sizeof(*node));
	if (node) {
		node->surface = surface;
		g_ptr_array_add(atlas->nodes, node);
	}

	return node;
}

/**
 * @return The node for `surface` or `NULL`.
 */
atlas_node_t *Atlas_Find(atlas_t *atlas, SDL_Surface *surface) {

	for (guint i = 0; i < atlas->nodes->len; i++) {
		atlas_node_t *node = g_ptr_array_index(atlas->nodes, i);
		if (node->surface == surface) {
			return node;
		}
	}

	return NULL;
}

/**
 * @brief GCompareFunc for node sorting. Note that g_ptr_array_sort provides pointers to pointers.
 */
static gint Atlas_NodeComparator(gconstpointer a, gconstpointer b) {

	const int32_t a_size = (*(atlas_node_t **) a)->surface->w * (*(atlas_node_t **) a)->surface->h;
	const int32_t b_size = (*(atlas_node_t **) b)->surface->w * (*(atlas_node_t **) b)->surface->h;

	return b_size - a_size;
}

/**
 * @brief Compiles `atlas` into the given surface, packing nodes as efficiently as possible.
 * @details If all nodes fit in `surface`, `0` is returned. If a node overflows, the index
 * of that node is returned. If a node exceeds the bounds of `surface`, `-1` is returned.
 * @param atlas The atlas.
 * @param surface The surface to pack nodes into.
 * @param start The node index to start packing from.
 * @return `0` on success, or the index of the next `start` node. `-1` on error.
 */
int32_t Atlas_Compile(atlas_t *atlas, SDL_Surface *surface, int32_t start) {

	g_ptr_array_sort(atlas->nodes, Atlas_NodeComparator);

	atlas_node_t *row = g_ptr_array_index(atlas->nodes, 0);
	int32_t x = 0, y = 0;

	for (int32_t i = start; i < (int32_t) atlas->nodes->len; i++) {
		atlas_node_t *node = g_ptr_array_index(atlas->nodes, i);

		if (node->surface->w > surface->w ||
			node->surface->h > surface->h) {
			return -1;
		}

		if (x + node->surface->w > surface->w) {
			if (y + node->surface->h > surface->h) {
				return i;
			}
			x = 0;
			y += row->surface->h;
			row = node;
		}

		node->x = x;
		node->y = y;

		SDL_BlitSurface(node->surface, &(SDL_Rect) {
			0, 0, node->surface->w, node->surface->h
		}, surface, &(SDL_Rect) {
			node->x, node->y, node->surface->w, node->surface->h
		});

		x += node->surface->w;
	}

	return 0;
}

/**
 * @brief Destroys `atlas`, freeing its memory.
 */
void Atlas_Destroy(atlas_t *atlas) {

	g_ptr_array_free(atlas->nodes, 1);

	g_free(atlas);
}
