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
 * @brief How far behind its own plane a portal's camera is set, in world units.
 * @details Carried to the exit, this puts the camera that far in front of the destination, so
 * that brushwork sitting on the destination's plane -- the far face of a double-walled
 * teleporter, say -- falls behind the camera and is clipped rather than filling the view.
 */
#define PORTAL_CAMERA_OFFSET 16.f

/**
 * @brief Resolves the model matrix of the entity drawing @p portal's face this frame.
 * @details A portal face's frame is baked in the space of the model that draws it, so a portal
 * on a mover -- a `func_bob` teleporter, say -- reaches the world only through that entity's
 * current transform.
 * @return `false` if nothing in the frame draws this portal's face, in which case it must not be
 * offered: an inline model built around an origin brush has geometry nowhere near where it sits
 * in the world, so guessing the identity for an entity the server did not send would place the
 * portal at the world origin, where it could crowd a real one out of the distance sort.
 */
static bool Cg_PortalMatrix(const cl_frame_t *frame, const r_bsp_portal_t *portal, mat4_t *matrix) {

  if (!portal->model) {
    return false;
  }

  // worldspawn is never sent as an entity, and never moves
  if (portal->model == cgi.WorldModel()->bsp->worldspawn) {
    *matrix = Mat4_Identity();
    return true;
  }

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
 * rather than pinning it to the target is what gives a portal parallax: leaning to the left of
 * one shows more of what lies to the right of its exit, as a window does.
 * @remarks A portal's own brushwork is drawn by whatever entity owns it, as any other brushwork
 * is. Every portal of the world is offered; the renderer keeps the nearest of them, and repeats
 * the scene into each once it is complete.
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

    cgi.UpdatePortal(p, matrix);

    r_view_t *view = cgi.AddPortal(cgi.view, p);
    if (!view) {
      continue;
    }

    view->type = VIEW_PORTAL;
    view->viewport = cgi.view->viewport;
    view->fov = cgi.view->fov;
    view->depth_range = cgi.view->depth_range;
    view->ticks = cgi.view->ticks;
    view->ambient = cgi.view->ambient;

    // the camera is flattened onto the portal's own plane before being carried, rather than
    // carried in full. At the exit it then sits on the destination's plane, so nothing behind the
    // destination is ever in front of the camera -- whatever brushwork surrounds it, and with no
    // clip plane to pay for. Depth parallax goes with it, since walking up to a portal no longer
    // opens the view; clamping the flattened point to the face keeps the lateral parallax, which
    // is the part that sells the effect
    vec3_t origin = Box3_ClampPoint(p->abs_bounds, cgi.view->origin);

    // Cm_DistanceToPlane, which the client game does not see
    const float dist = Vec3_Dot(origin, p->abs_plane.normal) - p->abs_plane.dist;

    origin = Vec3_Subtract(origin, Vec3_Scale(p->abs_plane.normal, dist + PORTAL_CAMERA_OFFSET));

    view->origin = Mat4_Transform(p->matrix, origin);
    view->forward = Mat4_RotateVector(p->matrix, cgi.view->forward);
    view->right = Mat4_RotateVector(p->matrix, cgi.view->right);
    view->up = Mat4_RotateVector(p->matrix, cgi.view->up);
    // Vec3_Euler recovers pitch and yaw but leaves roll at zero, while the carried basis has
    // whatever roll the two frames differ by. Anything rebuilding a basis from these angles,
    // such as an all-axis sprite, would otherwise be rotated wrongly in a rolled portal
    view->angles = Vec3_Euler(view->forward);

    vec3_t right, up;
    Vec3_Vectors(view->angles, NULL, &right, &up);

    view->angles.z = Degrees(atan2f(Vec3_Dot(view->up, right), Vec3_Dot(view->up, up)));
  }
}
