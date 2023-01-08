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

/**
 * @return True if the specified bounding box is culled by the view frustum, false otherwise.
 * @see http://www.lighthouse3d.com/tutorials/view-frustum-culling/geometric-approach-testing-boxes/
 */
_Bool R_CullBox(const r_view_t *view, const box3_t bounds) {

	if (!r_cull->value) {
		return false;
	}

	if (view->type == VIEW_PLAYER_MODEL) {
		return false;
	}

	vec3_t points[8];

	Box3_ToPoints(bounds, points);

	const cm_bsp_plane_t *plane = view->frustum;
	for (size_t i = 0; i < lengthof(view->frustum); i++, plane++) {

		size_t j;
		for (j = 0; j < lengthof(points); j++) {
			const float dist = Cm_DistanceToPlane(points[j], plane);
			if (dist >= 0.f) {
				break;
			}
		}

		if (j == lengthof(points)) {
			return true;
		}
	}

	return false;
}

/**
 * @return True if the specified sphere is culled by the view frustum, false otherwise.
 */
_Bool R_CullSphere(const r_view_t *view, const vec3_t point, const float radius) {

	if (!r_cull->value) {
		return false;
	}

	if (view->type == VIEW_PLAYER_MODEL) {
		return false;
	}

	const cm_bsp_plane_t *plane = view->frustum;
	for (size_t i = 0 ; i < lengthof(view->frustum) ; i++, plane++)  {
		const float dist = Cm_DistanceToPlane(point, plane);
		if (dist < -radius) {
			return true;
		}
	}

	return false;
}

/**
 * @brief Updates the clipping planes for the view frustum for the current frame.
 * @details The frustum planes are outward facing. Thus, any object that appears
 * partially behind any of the frustum planes should be visible.
 */
void R_UpdateFrustum(r_view_t *view) {

	if (!r_cull->value) {
		return;
	}

	cm_bsp_plane_t *p = view->frustum;

	float ang = Radians(view->fov.x);
	float xs = sinf(ang);
	float xc = cosf(ang);

	p[0].normal = Vec3_Scale(view->forward, xs);
	p[0].normal = Vec3_Fmaf(p[0].normal, xc, view->right);

	p[1].normal = Vec3_Scale(view->forward, xs);
	p[1].normal = Vec3_Fmaf(p[1].normal, -xc, view->right);

	ang = Radians(view->fov.y);
	xs = sinf(ang);
	xc = cosf(ang);

	p[2].normal = Vec3_Scale(view->forward, xs);
	p[2].normal = Vec3_Fmaf(p[2].normal, xc, view->up);

	p[3].normal = Vec3_Scale(view->forward, xs);
	p[3].normal = Vec3_Fmaf(p[3].normal, -xc, view->up);

	for (size_t i = 0; i < lengthof(view->frustum); i++) {
		p[i].normal = Vec3_Normalize(p[i].normal);
		p[i].dist = Vec3_Dot(view->origin, p[i].normal);
		p[i].type = Cm_PlaneTypeForNormal(p[i].normal);
		p[i].sign_bits = Cm_SignBitsForNormal(p[i].normal);
	}
}
