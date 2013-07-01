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

#include "r_local.h"

typedef struct {
	r_element_t *elements;
	size_t size; // the total size (max) allocated for this level
	size_t count; // the current count for this frame
} r_elements_t;

static r_elements_t r_elements;

/*
 * @brief Adds the specified element to the current frame.
 */
void R_AddElement(const r_element_type_t type, const void *element) {
	vec_t *pos;
	vec3_t delta;

	if (r_elements.count == r_elements.size) {
		Com_Warn("r_elements.size reached\n");
		return;
	}

	r_element_t *e = &r_elements.elements[r_elements.count++];

	e->type = type;
	e->element = element;

	switch (e->type) {
		case ELEMENT_BSP_SURFACE:
			pos = ((r_bsp_surface_t *) e->element)->center;
			break;

		case ELEMENT_MESH_MODEL:
			pos = ((r_entity_t *) e->element)->origin;
			break;

		case ELEMENT_PARTICLE:
			pos = ((r_particle_t *) e->element)->org;
			break;

		default:
			VectorCopy(r_view.origin, pos);
			break;
	}

	VectorSubtract(pos, r_view.origin, delta);
	e->dist = VectorLength(delta);
}

/*
 * @brief Qsort comparator for render elements.
 */
static int R_SortElements_Compare(const void *e1, const void *e2) {
	return ((r_element_t *) e1)->dist > ((r_element_t *) e2)->dist;
}

/*
 * @brief Sorts the specified elements array by their distance from the origin.
 * Elements are sorted farthest-first so that they are rendered back-to-front.
 * This is optionally run in a separate thread.
 */
void R_SortElements(void *data __attribute__((unused))) {

	if (!r_elements.count)
		return;

	qsort(r_elements.elements, r_elements.count, sizeof(r_element_t), R_SortElements_Compare);

	R_UpdateParticles(r_elements.elements, r_elements.count);
}

/*
 * @brief Draws the specified subset of elements. The type
 */
static void R_DrawElements_(const r_element_t *e, const size_t count) {

	switch (e->type) {
		case ELEMENT_BSP_SURFACE:
			break;

		case ELEMENT_MESH_MODEL:
			break;

		case ELEMENT_PARTICLE:
			R_DrawParticles(e, count);
			break;

		default:
			break;
	}
}

/*
 * @brief Draws all elements for the current frame. It is expected that the
 * elements are already depth-sorted. See above.
 */
void R_DrawElements(void) {
	size_t i, j;

	if (!r_elements.count)
		return;

	const r_element_t *e = r_elements.elements;
	r_element_type_t type = ELEMENT_NONE;

	for (i = j = 0; i < r_elements.count; i++, e++) {

		if (e->type != type) { // draw pending
			if (i > j) {
				R_DrawElements_(r_elements.elements + j, i - j);
				j = i;
			}
			type = e->type;
		}
	}

	if (i > j) { // draw remaining
		R_DrawElements_(r_elements.elements + j, i - j);
	}

	r_elements.count = 0;
}

/*
 * @brief Initializes the elements pool, which is allocated based on the
 * geometry of the current level.
 */
void R_InitElements(void) {

	if (r_elements.elements) {
		Z_Free(r_elements.elements);
	}

	memset(&r_elements, 0, sizeof(r_elements));

	r_elements.size = MAX_ENTITIES + MAX_PARTICLES;

	const r_sorted_bsp_surfaces_t *surfs = r_model_state.world->bsp->sorted_surfaces;

	r_elements.size += surfs->blend.count;
	r_elements.size += surfs->blend_warp.count;
	r_elements.size += surfs->flare.count;

	r_elements.elements = Z_TagMalloc(r_elements.size * sizeof(r_element_t), Z_TAG_RENDERER);
}
