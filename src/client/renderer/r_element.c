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
	r_element_t *elements; // the elements pool
	size_t count; // the number of elements in the current frame
	size_t size; // the total size (max) allocated for this level

	r_bsp_surfaces_t surfs; // a bucket for depth-sorted BSP surfaces
} r_element_state_t;

static r_element_state_t r_element_state;

/*
 * @brief Adds the depth-sorted element to the current frame.
 */
void R_AddElement(const r_element_t *e) {
	vec3_t delta;

	if (r_element_state.count == r_element_state.size) {
		Com_Warn("r_element_state.size %zu reached\n", r_element_state.size);
		return;
	}

	r_element_t *el = &r_element_state.elements[r_element_state.count++];

	// copy the element in
	*el = *e;

	// and resolve its depth
	VectorSubtract(r_view.origin, el->origin, delta);
	el->depth = VectorLength(delta);
}

/*
 * @brief Adds elements for the specified blended surfaces lists.
 */
static void R_AddBspSurfaceElements(void) {
	static r_element_t e;

	r_bsp_surface_t *s = r_model_state.world->bsp->surfaces;

	uint16_t i;
	for (i = 0; i < r_model_state.world->bsp->num_surfaces; i++, s++) {

		if (s->texinfo->flags & (SURF_BLEND_33 | SURF_BLEND_66)) {
			if (s->frame == r_locals.frame) {

				if (s->texinfo->flags & SURF_WARP) {
					e.type = ELEMENT_BSP_SURFACE_BLEND_WARP;
				} else {
					e.type = ELEMENT_BSP_SURFACE_BLEND;
				}

				e.element = (const void *) s;
				e.origin = (const vec_t *) s->center;

				R_AddElement(&e);
			}
		}
	}
}

/*
 * @brief Qsort comparator for render elements. Qsort sorts elements into
 * ascending order, so we return negative values when a is behind b (b - a).
 */
static int R_SortElements_Compare(const void *a, const void *b) {
	return ((r_element_t *) b)->depth - ((r_element_t *) a)->depth;
}

/*
 * @brief Sorts the specified elements array by their distance from the origin.
 * Elements are sorted farthest-first so that they are rendered back-to-front.
 */
static void R_SortElements_(r_element_t *e, const size_t count) {
	qsort(e, count, sizeof(r_element_t), R_SortElements_Compare);
}

/*
 * @brief Sorts the draw elements for the current frame. Once elements are
 * sorted, particles for the current frame are also updated.
 */
void R_SortElements(void *data __attribute__((unused))) {

	R_AddBspSurfaceElements();

	if (!r_element_state.count)
		return;

	R_SortElements_(r_element_state.elements, r_element_state.count);

	R_UpdateParticles(r_element_state.elements, r_element_state.count);
}

/*
 * @brief Populates an r_bsp_surfaces_t with the specified elements.
 */
static void R_DrawBspSurfaceElements(const r_element_t *e, const size_t count, BspSurfacesDrawFunc func) {
	r_bsp_surfaces_t *surfs = &r_element_state.surfs;
	size_t i;

	for (i = 0; i < count; i++, e++) {
		surfs->surfaces[i] = (r_bsp_surface_t *) e->element;
	}

	surfs->count = count;

	func(surfs);
}

/*
 * @brief Draws the specified subset of elements. The type
 */
static void R_DrawElements_(const r_element_t *e, const size_t count) {

	switch (e->type) {
		case ELEMENT_BSP_SURFACE_BLEND:
			R_DrawBspSurfaceElements(e, count, R_DrawBlendBspSurfaces);
			break;

		case ELEMENT_BSP_SURFACE_BLEND_WARP:
			R_DrawBspSurfaceElements(e, count, R_DrawBlendWarpBspSurfaces);
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

	if (!r_element_state.count)
		return;

	const r_element_t *e = r_element_state.elements;

#if 0
	static r_corona_t c;

	VectorCopy(e->origin, c.origin);
	VectorSet(c.color, 1.0, 0.0, 1.0);
	c.radius = 64.0;

	R_AddCorona(&c);
#endif

	r_element_type_t type = ELEMENT_NONE;

	for (i = j = 0; i < r_element_state.count; i++, e++) {

		if (e->type != type) { // draw pending
			if (i > j) {
				R_DrawElements_(r_element_state.elements + j, i - j);
				j = i;
			}
			type = e->type;
		}
	}

	if (i > j) { // draw remaining
		R_DrawElements_(r_element_state.elements + j, i - j);
	}

	r_element_state.count = 0;
}

/*
 * @brief Initializes the elements pool, which is allocated based on the
 * geometry of the current level.
 */
void R_InitElements(r_bsp_model_t *bsp) {

	memset(&r_element_state, 0, sizeof(r_element_state));

	r_bsp_surfaces_t *surfs = &r_element_state.surfs;

	surfs->count += bsp->sorted_surfaces->blend.count;
	surfs->count += bsp->sorted_surfaces->blend_warp.count;

	surfs->surfaces = Z_LinkMalloc(surfs->count * sizeof(r_bsp_surface_t **), bsp);

	r_element_state.size = MIN_ELEMENTS + r_element_state.surfs.count;
	r_element_state.elements = Z_LinkMalloc(r_element_state.size * sizeof(r_element_t), bsp);
}
