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

#define MIN_ELEMENTS (MAX_PARTICLES + MAX_ENTITIES)

typedef struct {
	r_element_t *elements;
	size_t size; // the total size (max) allocated for this level
	size_t num_elements; // the current count for this frame

	r_bsp_surfaces_t surfs; // a bucket for depth-sorted BSP surfaces
} r_elements_t;

static r_elements_t r_elements;

/*
 * @brief Adds the depth-sorted element to the current frame.
 */
void R_AddElement(const r_element_t *e) {
	vec3_t delta;

	if (r_elements.num_elements == r_elements.size) {
		Com_Warn("r_elements.size reached\n");
		return;
	}

	// copy the element in
	r_elements.elements[r_elements.num_elements] = *e;

	// and resolve its depth
	VectorSubtract(r_view.origin, e->origin, delta);
	r_elements.elements[r_elements.num_elements++].depth = VectorLength(delta);
}

/*
 * @brief Qsort comparator for render elements. Qsort sorts elements into
 * ascending order, so we return positive values when a is behind b.
 */
static int R_SortElements_Compare(const void *a, const void *b) {
	return ((r_element_t *) b)->depth - ((r_element_t *) a)->depth;
}

/*
 * @brief Sorts the specified elements array by their distance from the origin.
 * Elements are sorted farthest-first so that they are rendered back-to-front.
 * This is optionally run in a separate thread.
 */
void R_SortElements(void *data __attribute__((unused))) {

	if (!r_elements.num_elements)
		return;

	qsort(r_elements.elements, r_elements.num_elements, sizeof(r_element_t), R_SortElements_Compare);

	// generate particle primitives against the sorted element list
	R_UpdateParticles(r_elements.elements, r_elements.num_elements);
}

/*
 * @brief Populates an r_bsp_surfaces_t with the specified elements.
 */
static void R_DrawElements_PopulateSurfaces(const r_element_t *e, const size_t count) {
	r_bsp_surfaces_t *surfs = &r_elements.surfs;
	size_t i;

	for (i = 0; i < count; i++, e++) {
		surfs->surfaces[i] = (r_bsp_surface_t *) e->element;
	}

	surfs->count = count;
}

/*
 * @brief Draws the specified subset of elements. The type
 */
static void R_DrawElements_(const r_element_t *e, const size_t count) {

	switch (e->type) {
		case ELEMENT_BSP_SURFACE_BLEND:
			R_DrawElements_PopulateSurfaces(e, count);
			R_DrawBlendBspSurfaces(&r_elements.surfs);
			break;

		case ELEMENT_BSP_SURFACE_BLEND_WARP:
			R_DrawElements_PopulateSurfaces(e, count);
			R_DrawBlendWarpBspSurfaces(&r_elements.surfs);
			break;

		case ELEMENT_ENTITY:
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

	if (!r_elements.num_elements)
		return;

	const r_element_t *e = r_elements.elements;

#if 0
	static r_corona_t c;

	VectorCopy(e->origin, c.origin);
	VectorSet(c.color, 1.0, 0.0, 1.0);
	c.radius = 64.0;

	R_AddCorona(&c);
#endif

	r_element_type_t type = ELEMENT_NONE;

	for (i = j = 0; i < r_elements.num_elements; i++, e++) {

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

	r_elements.num_elements = 0;
}

/*
 * @brief Initializes the elements pool, which is allocated based on the
 * geometry of the current level.
 */
void R_InitElements(r_bsp_model_t *bsp) {

	memset(&r_elements, 0, sizeof(r_elements));

	r_bsp_surfaces_t *surfs = &r_elements.surfs;

	surfs->count += bsp->sorted_surfaces->blend.count;
	surfs->count += bsp->sorted_surfaces->blend_warp.count;

	surfs->surfaces = Z_LinkMalloc(surfs->count * sizeof(r_bsp_surface_t **), bsp);

	r_elements.size = MIN_ELEMENTS + r_elements.surfs.count;
	r_elements.elements = Z_LinkMalloc(r_elements.size * sizeof(r_element_t), bsp);
}
