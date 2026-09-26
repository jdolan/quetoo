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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include "r_local.h"

/**
 * @brief Tests whether a box is outside the view frustum.
 */
bool R_CullBox(const RenderView *view, const Box3 bounds) {

  if (!r_cull->value) {
    return false;
  }

  if (view->type == VIEW_PLAYER_MODEL) {
    return false;
  }

  // a box is behind a plane only if even its corner farthest along the plane's normal is, so that
  // one corner is tested rather than all eight
  const CmBspPlane *plane = view->frustum;
  for (size_t i = 0; i < lengthof(view->frustum); i++, plane++) {

    const Vec3 corner = MakeVec3(plane->normal.x >= 0.f ? bounds.maxs.x : bounds.mins.x,
                                 plane->normal.y >= 0.f ? bounds.maxs.y : bounds.mins.y,
                                 plane->normal.z >= 0.f ? bounds.maxs.z : bounds.mins.z);

    if (Cm_DistanceToPlane(corner, plane) < 0.f) {
      return true;
    }
  }

  return false;
}

/**
 * @brief Tests whether a sphere is outside the view frustum.
 */
bool R_CullSphere(const RenderView *view, const Vec3 point, const float radius) {

  if (!r_cull->value) {
    return false;
  }

  if (view->type == VIEW_PLAYER_MODEL) {
    return false;
  }

  const CmBspPlane *plane = view->frustum;
  for (size_t i = 0 ; i < lengthof(view->frustum) ; i++, plane++)  {
    const float dist = Cm_DistanceToPlane(point, plane);
    if (dist < -radius) {
      return true;
    }
  }

  return false;
}

/**
 * @brief Updates the view frustum planes.
 */
void R_UpdateFrustum(RenderView *view) {

  if (!r_cull->value) {
    return;
  }

  CmBspPlane *p = view->frustum;

  float hs = sinf(Radians(view->fov.x));
  float hc = cosf(Radians(view->fov.x));

  p[0].normal = Vec3_Scale(view->forward, hs);
  p[0].normal = Vec3_Fmaf(p[0].normal, -hc, view->right);

  p[1].normal = Vec3_Scale(view->forward, hs);
  p[1].normal = Vec3_Fmaf(p[1].normal, hc, view->right);

  float vs = sinf(Radians(view->fov.y));
  float vc = cosf(Radians(view->fov.y));

  p[2].normal = Vec3_Scale(view->forward, vs);
  p[2].normal = Vec3_Fmaf(p[2].normal, -vc, view->up);

  p[3].normal = Vec3_Scale(view->forward, vs);
  p[3].normal = Vec3_Fmaf(p[3].normal, vc, view->up);

  for (size_t i = 0; i < lengthof(view->frustum); i++) {
    p[i].normal = Vec3_Normalize(p[i].normal);
    p[i].dist = Vec3_Dot(view->origin, p[i].normal);
    p[i].type = Cm_PlaneTypeForNormal(p[i].normal);
    p[i].signBits = Cm_SignBitsForNormal(p[i].normal);
  }
}
