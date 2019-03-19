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

#include "tjunction.h"
#include "portal.h"
#include "qbsp.h"

static SDL_atomic_t c_tjunctions;
static GPtrArray *faces;

/**
 * @brief
 */
static void FixTJunctions_(int32_t face_num) {
	static SDL_SpinLock lock;

	face_t *face = g_ptr_array_index(faces, face_num);

	for (size_t s = 0; s < faces->len; s++) {

		const face_t *f = g_ptr_array_index(faces, s);
		if (face == f) {
			continue;
		}

		const plane_t *plane = &planes[face->plane_num];

		for (int32_t i = 0; i < f->w->num_points; i++) {
			const vec_t *v = f->w->points[i];

			const dvec_t d = DotProduct(v, plane->normal) - plane->dist;
			if (d > ON_EPSILON || d < -ON_EPSILON) {
				continue; // v is not on face's plane
			}

			// v is on face's plane, so test it against face's edges

			for (int32_t j = 0; j < face->w->num_points; j++) {

				const vec_t *v0 = face->w->points[(j + 0) % face->w->num_points];
				const vec_t *v1 = face->w->points[(j + 1) % face->w->num_points];

				vec3_t a, b;
				VectorSubtract(v0, v, a);
				const vec_t a_dist = VectorNormalize(a);

				VectorSubtract(v1, v, b);
				const vec_t b_dist = VectorNormalize(b);

				if (a_dist <= ON_EPSILON || b_dist <= ON_EPSILON) {
					break; // face already includes v
				}

				const vec_t d = DotProduct(a, b);
				if (d > -1.0 + SIDE_EPSILON) {
					continue; // v is not on the edge v0 <-> v1
				}

				// v sits between v0 and v1, so add it to the face

				cm_winding_t *w = Cm_AllocWinding(face->w->num_points + 1);
				w->num_points = face->w->num_points + 1;

				for (int32_t k = 0; k < w->num_points; k++) {
					if (k <= j) {
						VectorCopy(face->w->points[k], w->points[k]);
					} else if (k == j + 1) {
						VectorCopy(v, w->points[k]);
					} else {
						VectorCopy(face->w->points[k - 1], w->points[k]);
					}
				}

				SDL_AtomicLock(&lock);

				Cm_FreeWinding(face->w);
				face->w = w;

				SDL_AtomicUnlock(&lock);

				SDL_AtomicAdd(&c_tjunctions, 1);
				break;
			}
		}
	}
}

/**
 * @brief
 */
static void FixTJunctions_r(node_t *node) {

	if (node->plane_num != PLANE_NUM_LEAF) {
		FixTJunctions_r(node->children[0]);
		FixTJunctions_r(node->children[1]);
	}

	for (face_t *face = node->faces; face; face = face->next) {
		if (face->merged) {
			continue;
		}
		if (g_ptr_array_find(faces, face, NULL)) {
			continue;
		}
		g_ptr_array_add(faces, face);
	}
}

/**
 * @brief
 */
void FixTJunctions(node_t *node) {

	Com_Verbose("--- FixTJunctions ---\n");
	SDL_AtomicSet(&c_tjunctions, 0);

	faces = g_ptr_array_new();
	FixTJunctions_r(node);

	Work(entity_num == 0 ? "Fixing t-junctions" : NULL, FixTJunctions_, faces->len);

	Com_Verbose("%5i fixed tjunctions\n", SDL_AtomicGet(&c_tjunctions));

	g_ptr_array_free(faces, true);
}
