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
static SDL_SpinLock *faces_locks;
static int32_t largest_winding = 0;

// FIXME/TODO temp fix since we're using old glib
#if defined(_WIN32)
static gboolean
g_ptr_array_find(GPtrArray *haystack,
				 gconstpointer needle,
				 guint *index_) {
	for (guint i = 0; i < haystack->len; i++) {
		gconstpointer value = g_ptr_array_index(haystack, i);

		if (value == needle) {
			if (index_)
				*index_ = i;

			return true;
		}
	}

	return false;
}
#endif

/**
 * @brief
 */
static void FixTJunctions_(int32_t face_num) {
	static _Thread_local cm_winding_t *face_winding, *f_winding;

	if (!face_winding) {
		face_winding = Cm_AllocWinding(largest_winding);
		f_winding = Cm_AllocWinding(largest_winding);
	}

	face_t *face = g_ptr_array_index(faces, face_num);

	SDL_SpinLock *face_lock = &faces_locks[face_num];

	const plane_t *plane = &planes[face->plane_num];

	// Make a copy of face->w for testing
	SDL_AtomicLock(face_lock);
	memcpy(face_winding, face->w, sizeof(cm_winding_t) + (face->w->num_points * sizeof(vec3_t)));
	SDL_AtomicUnlock(face_lock);

	for (size_t s = 0; s < faces->len; s++) {

		const face_t *f = g_ptr_array_index(faces, s);
		if (face == f) {
			continue;
		}
		
		SDL_SpinLock *f_lock = &faces_locks[s];

		SDL_AtomicLock(f_lock);
		memcpy(f_winding, f->w, sizeof(cm_winding_t) + (f->w->num_points * sizeof(vec3_t)));
		SDL_AtomicUnlock(f_lock);

		for (int32_t i = 0; i < f_winding->num_points; i++) {
			const vec3_t v = f_winding->points[i];

			const double d = Vec3_Dot(v, plane->normal) - plane->dist;
			if (d > ON_EPSILON || d < -ON_EPSILON) {
				continue; // v is not on face's plane
			}

			// v is on face's plane, so test it against face's edges

			for (int32_t j = 0; j < face_winding->num_points; j++) {

				const vec3_t v0 = face_winding->points[(j + 0) % face_winding->num_points];
				const vec3_t v1 = face_winding->points[(j + 1) % face_winding->num_points];

				vec3_t a;
				const float a_dist = Vec3_DistanceDir(v0, v, &a);

				vec3_t b;
				const float b_dist = Vec3_DistanceDir(v1, v, &b);

				if (a_dist < VERTEX_EPSILON || b_dist < VERTEX_EPSILON) {
					break; // face already includes v
				}

				const float d = Vec3_Dot(a, b);
				if (d > -1.0 + COLINEAR_EPSILON) {
					continue; // v is not on the edge v0 <-> v1
				}

				// v sits between v0 and v1, so add it to the face
				cm_winding_t *w = Cm_AllocWinding(face_winding->num_points + 1);
				w->num_points = face_winding->num_points + 1;

				for (int32_t k = 0; k < w->num_points; k++) {
					if (k <= j) {
						w->points[k] = face_winding->points[k];
					} else if (k == j + 1) {
						w->points[k] = v;
					} else {
						w->points[k] = face_winding->points[k - 1];
					}
				}

				SDL_AtomicLock(face_lock);

				// Copy back to face, and copy to temp winding
				Cm_FreeWinding(face->w);
				face->w = w;
				memcpy(face_winding, face->w, sizeof(cm_winding_t) + (face->w->num_points * sizeof(vec3_t)));

				SDL_AtomicUnlock(face_lock);

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

		largest_winding = MAX(largest_winding, face->w->num_points);
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

	largest_winding = sizeof(cm_winding_t) + (sizeof(vec3_t) * largest_winding);

	faces_locks = Mem_Malloc(sizeof(SDL_SpinLock) * faces->len);

	Work(entity_num == 0 ? "Fixing t-junctions" : NULL, FixTJunctions_, faces->len);

	Com_Verbose("%5i fixed tjunctions\n", SDL_AtomicGet(&c_tjunctions));

	Mem_Free(faces_locks);
	g_ptr_array_free(faces, true);
}
