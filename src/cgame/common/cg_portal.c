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

#include "cg_local.h"

/**
 * @brief Resolves the model matrix of the entity drawing @p portal's face this frame.
 * @details A portal face's frame is baked in the space of the model containing it, so a portal
 * on a mover -- a `func_bob` teleporter, say -- must be drawn with that entity's
 * current transform.
 * @return `false` if the entity that contains @c portal is not in the current frame (@c SVF_NO_CLIENT, etc.).
 */
static bool Cg_PortalMatrix(const cl_frame_t *frame, const r_bsp_portal_t *portal, mat4_t *matrix) {

  assert(portal->model);

  for (int32_t i = 0; i < frame->num_entities; i++) {

    const uint32_t snum = (frame->entity_state + i) & ENTITY_STATE_MASK;
    const entity_state_t *s = &cgi.client->entity_states[snum];

    if (cgi.client->models[s->model1] == portal->model) {
      const cl_entity_t *ent = &cgi.client->entities[s->number];

      *matrix = Mat4_FromRotationTranslationScale(ent->angles, ent->origin, 1.f);
      return true;
    }
  }

  return false;
}

/**
 * @brief Places the camera of each portal's view, and adds the portal to the main view.
 * @details The camera is the player's own, carried into the portal's target frame. Carrying it
 * rather than pinning it to the target is what gives a portal parallax.
 */
void Cg_AddPortals(const cl_frame_t *frame) {

  const r_model_t *world = cgi.WorldModel();
  if (!world) {
    return;
  }

  for (int32_t i = 0; i < world->bsp->num_portals; i++) {

    r_bsp_portal_t *p = &world->bsp->portals[i];

    mat4_t matrix;
    if (!Cg_PortalMatrix(frame, p, &matrix)) {
      continue;
    }

    r_view_t *view = cgi.AddPortal(cgi.view, p, matrix);
    if (!view) {
      continue;
    }

    view->type = VIEW_PORTAL;
    view->viewport = cgi.view->viewport;
    view->fov = cgi.view->fov;
    view->depth_range = cgi.view->depth_range;
    view->ticks = cgi.view->ticks;
    view->ambient = cgi.view->ambient;

    // project the main view's origin on the portal's plane, and clamped to the portal bounds
    vec3_t origin = Box3_ClampPoint(p->abs_bounds, cgi.view->origin);

    const float dist = Vec3_Dot(origin, p->abs_plane.normal) - p->abs_plane.dist;

    origin = Vec3_Subtract(origin, Vec3_Scale(p->abs_plane.normal, dist));

    view->origin = Mat4_Transform(p->matrix, origin);
    view->forward = Mat4_RotateVector(p->matrix, cgi.view->forward);
    view->right = Mat4_RotateVector(p->matrix, cgi.view->right);
    view->up = Mat4_RotateVector(p->matrix, cgi.view->up);
    view->angles = Vec3_Euler(view->forward);

    vec3_t right, up;
    Vec3_Vectors(view->angles, NULL, &right, &up);
    view->angles.z = Degrees(atan2f(Vec3_Dot(view->up, right), Vec3_Dot(view->up, up)));
  }
}
