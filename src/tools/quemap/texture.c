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
#include "map.h"
#include "texture.h"

/**
 * @brief
 */
static void TextureAxisForPlane(const plane_t *plane, vec3_t *xv, vec3_t *yv) {
	static const vec3_t base_axis[18] = {
		{ {  0,  0,  1 } },
		{ {  1,  0,  0 } },
		{ {  0, -1,  0 } }, // floor
		{ {  0,  0, -1 } },
		{ {  1,  0,  0 } },
		{ {  0, -1,  0 } }, // ceiling
		{ {  1,  0,  0 } },
		{ {  0,  1,  0 } },
		{ {  0,  0, -1 } }, // west wall
		{ { -1,  0,  0 } },
		{ {  0,  1,  0 } },
		{ {  0,  0, -1 } }, // east wall
		{ {  0,  1,  0 } },
		{ {  1,  0,  0 } },
		{ {  0,  0, -1 } }, // south wall
		{ {  0, -1,  0 } },
		{ {  1,  0,  0 } },
		{ {  0,  0, -1 } }, // north wall
	};

	int32_t best_axis = 0;
	float best = 0.0;

	for (int32_t i = 0; i < 6; i++) {
		const float dot = Vec3_Dot(plane->normal, base_axis[i * 3]);
		if (dot > best) {
			best = dot;
			best_axis = i;
		}
	}

	*xv = base_axis[best_axis * 3 + 1];
	*yv = base_axis[best_axis * 3 + 2];
}

/**
 * @brief
 */
void TextureVectorsForBrushSide(brush_side_t *side, const vec3_t origin) {

	vec3_t axis[2];
	TextureAxisForPlane(&planes[side->plane], &axis[0], &axis[1]);

	vec2_t offset;
	offset.x = Vec3_Dot(origin, axis[0]);
	offset.y = Vec3_Dot(origin, axis[1]);

	vec2_t scale;
	scale.x = side->scale.x ?: 1.f;
	scale.y = side->scale.y ?: 1.f;

	// rotate axis
	const double sinv = sin(Radians(side->rotate));
	const double cosv = cos(Radians(side->rotate));

	int32_t sv;
	if (axis[0].x) {
		sv = 0;
	} else if (axis[0].y) {
		sv = 1;
	} else {
		sv = 2;
	}

	int32_t tv;
	if (axis[1].x) {
		tv = 0;
	} else if (axis[1].y) {
		tv = 1;
	} else {
		tv = 2;
	}

	for (int32_t i = 0; i < 2; i++) {
		const float ns = cosv * axis[i].xyz[sv] - sinv * axis[i].xyz[tv];
		const float nt = sinv * axis[i].xyz[sv] + cosv * axis[i].xyz[tv];
		axis[i].xyz[sv] = ns;
		axis[i].xyz[tv] = nt;
	}

	for (int32_t i = 0; i < 2; i++) {
		for (int32_t j = 0; j < 3; j++) {
			side->axis[i].xyzw[j] = axis[i].xyz[j] / scale.xy[i];
		}
	}

	side->axis[0].w = side->shift.x + offset.x;
	side->axis[1].w = side->shift.y + offset.y;
}
