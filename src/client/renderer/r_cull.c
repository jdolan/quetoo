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
bool R_CullBox(const r_view_t *view, const box3_t bounds) {

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
bool R_CullSphere(const r_view_t *view, const vec3_t point, const float radius) {

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
 * @details The frustum plane normals point inward (toward the visible volume).
 * A point with positive distance is inside the frustum; negative is outside.
 */
void R_UpdateFrustum(r_view_t *view) {

  if (!r_cull->value) {
    return;
  }

  cm_bsp_plane_t *p = view->frustum;

  // view->fov.x/y are half-angles (see Cg_UpdateFov).
  // Plane normal sin(θ)*forward ∓ cos(θ)*right clips exactly at the half-FOV angle θ:
  //   distance = sin(θ-φ), negative when φ > θ (outside frustum).
  float hs = sinf(Radians(view->fov.x));
  float hc = cosf(Radians(view->fov.x));

  // Right boundary plane (clips objects beyond the right edge)
  p[0].normal = Vec3_Scale(view->forward, hs);
  p[0].normal = Vec3_Fmaf(p[0].normal, -hc, view->right);

  // Left boundary plane (clips objects beyond the left edge)
  p[1].normal = Vec3_Scale(view->forward, hs);
  p[1].normal = Vec3_Fmaf(p[1].normal, hc, view->right);

  float vs = sinf(Radians(view->fov.y));
  float vc = cosf(Radians(view->fov.y));

  // Top boundary plane (clips objects beyond the top edge)
  p[2].normal = Vec3_Scale(view->forward, vs);
  p[2].normal = Vec3_Fmaf(p[2].normal, -vc, view->up);

  // Bottom boundary plane (clips objects beyond the bottom edge)
  p[3].normal = Vec3_Scale(view->forward, vs);
  p[3].normal = Vec3_Fmaf(p[3].normal, vc, view->up);

  for (size_t i = 0; i < lengthof(view->frustum); i++) {
    p[i].normal = Vec3_Normalize(p[i].normal);
    p[i].dist = Vec3_Dot(view->origin, p[i].normal);
    p[i].type = Cm_PlaneTypeForNormal(p[i].normal);
    p[i].sign_bits = Cm_SignBitsForNormal(p[i].normal);
  }
}
