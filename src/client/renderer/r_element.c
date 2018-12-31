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

#include "r_local.h"

#define MIN_ELEMENTS (MAX_PARTICLES + MAX_ENTITIES)

typedef struct {
	r_element_t *elements; // the elements pool
	size_t count; // the number of elements in the current frame
	size_t size; // the total size (max) allocated for this level

	r_bsp_surfaces_t surfs; // a bucket for depth-sorted BSP surfaces
} r_element_state_t;

static r_element_state_t r_element_state;

/**
 * @brief Adds the depth-sorted element to the current frame.
 */
void R_AddElement(const r_element_t *e) {
	vec3_t delta;

	if (r_element_state.count == r_element_state.size) {
		Com_Warn("r_element_state.size %u reached\n", (uint32_t) r_element_state.size);
		return;
	}

	r_element_t *el = &r_element_state.elements[r_element_state.count++];

	// copy the element in
	*el = *e;

	if (e->type == ELEMENT_PARTICLE && (((r_particle_t *) e->element)->flags & PARTICLE_FLAG_NO_DEPTH)) {
		el->depth = 0;
	} else {
		// and resolve its depth
		VectorSubtract(r_view.origin, el->origin, delta);
		el->depth = VectorLengthSquared(delta);
	}
}

/**
 * @brief Adds elements for the specified blended surfaces lists.
 */
static void R_AddBspSurfaceElements(const r_bsp_surfaces_t *surfs, const r_element_type_t type) {
	static r_element_t e;

	for (size_t i = 0; i < surfs->count; i++) {

		const r_bsp_surface_t *s = surfs->surfaces[i];

		if (s->frame == r_locals.frame) {

			e.type = type;
			e.element = (const void *) s;
			e.origin = (r_model_state.world->bsp->vertexes + s->vertex)->position;

			R_AddElement(&e);
		}
	}
}

/**
 * @brief Qsort comparator for render elements. Qsort sorts elements into
 * ascending order, so we return negative values when a is behind b (b - a).
 *
 * Example:
 *
 * (a) is a particle with depth 255.0.
 * (b) is a surface with depth 64.0
 *
 * 64.0 - 255.0 = -191.0
 *
 * Thus (a) is sorted before (b) so that we may render back-to-front by
 * iterating the sorted array from 0 to length.
 */
static int32_t R_SortElements_Compare(const void *a, const void *b) {
	return Sign(((r_element_t *) b)->depth - ((r_element_t *) a)->depth);
}

/**
 * @brief Sorts the specified elements array by their distance from the view.
 * Elements are sorted farthest-first so that they are rendered back-to-front.
 */
static void R_SortElements_(r_element_t *e, const size_t count) {
	qsort(e, count, sizeof(r_element_t), R_SortElements_Compare);
}

/**
 * @brief Qsort comparator for particles elements.
 * This batches particles between elements by type, then by image.
 */
static int32_t R_SortParticles_Compare(const void *a, const void *b) {
	const r_element_t *ae = ((const r_element_t *) a);
	const r_element_t *be = ((const r_element_t *) b);

	const r_particle_t *ap = ((const r_particle_t *) ae->element);
	const r_particle_t *bp = ((const r_particle_t *) be->element);

	if (bp->type == ap->type) {
		return (int32_t) (intptr_t) (bp->image - ap->image);
	}

	return bp->type - ap->type;
}

/**
 * @brief Sorts particle ranges by their material, to prevent texture swaps.
 * This also updates the particles' positions, etc while it loops.
 */
static void R_SortParticles_(r_element_t *e, const size_t count) {
	r_element_t *start = NULL;
	size_t c = 0;

	for (r_element_t *p = e; ; p++, c++) {

		if (c < count && p->type == ELEMENT_PARTICLE) {

			if (start == NULL) {
				start = p;
			}
		} else {

			if (start != NULL) {

				const size_t length = p - start;
				qsort(start, length, sizeof(r_element_t), R_SortParticles_Compare);

				R_UpdateParticles(start, length);

				start = NULL;
			}
		}

		if (c == count) {
			break;
		}
	}
}

/**
 * @brief Sorts the draw elements for the current frame. Once elements are
 * sorted, particles for the current frame are also updated.
 */
void R_SortElements(void *data) {

	R_AddBspSurfaceElements(&r_model_state.world->bsp->sorted_surfaces->blend, ELEMENT_BSP_SURFACE_BLEND);
	R_AddBspSurfaceElements(&r_model_state.world->bsp->sorted_surfaces->blend_warp, ELEMENT_BSP_SURFACE_BLEND_WARP);

	if (!r_element_state.count) {
		return;
	}

	R_SortElements_(r_element_state.elements, r_element_state.count);

	R_UpdateParticleState();

	R_SortParticles_(r_element_state.elements, r_element_state.count);
}

/**
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

/**
 * @brief Draws the specified subset of elements.
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

/**
 * @brief Draws all elements for the current frame. It is expected that the
 * elements are already depth-sorted. See above.
 */
void R_DrawElements(void) {
	size_t i, j;

	if (!r_element_state.count) {
		return;
	}

	R_UploadParticles();

	const r_element_t *e = r_element_state.elements;

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

/**
 * @brief Initializes the elements pool, which is allocated based on the
 * geometry of the current level.
 */
void R_InitElements(r_bsp_model_t *bsp) {

	memset(&r_element_state, 0, sizeof(r_element_state));

	r_bsp_surfaces_t *surfs = &r_element_state.surfs;

	surfs->count += bsp->sorted_surfaces->blend.count;
	surfs->count += bsp->sorted_surfaces->blend_warp.count;

	surfs->surfaces = Mem_LinkMalloc(surfs->count * sizeof(r_bsp_surface_t **), bsp);

	r_element_state.size = MIN_ELEMENTS + r_element_state.surfs.count;
	r_element_state.elements = Mem_LinkMalloc(r_element_state.size * sizeof(r_element_t), bsp);
}
