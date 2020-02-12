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

#include "bsp.h"
#include "polylib.h"

/**
 * @brief True if the winding would be crunched out of existence by vertex snapping.
 */
_Bool WindingIsSmall(const cm_winding_t *w) {

	int32_t valid_edges = 0;
	for (int32_t i = 0; i < w->num_points; i++) {
		const float dist = vec3_distance(w->points[i], w->points[(i + 1) % w->num_points]);
		if (dist > ON_EPSILON) {
			if (++valid_edges == 3) {
				return false;
			}
		}
	}
	return true;
}

/**
 * @brief Returns true if the winding still has one of the points from basewinding for plane
 */
_Bool WindingIsLarge(const cm_winding_t *w) {

	for (int32_t i = 0; i < w->num_points; i++) {
		for (int32_t j = 0; j < 3; j++)
			if (w->points[i].xyz[j] < -MAX_WORLD_COORD || w->points[i].xyz[j] > MAX_WORLD_COORD) {
				return true;
			}
	}
	return false;
}

/**
 * @brief
 */
void FreeWindings(void) {
	Mem_FreeTag(MEM_TAG_POLYLIB);
}
