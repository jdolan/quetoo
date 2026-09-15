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
 * @brief Places the camera of each portal's view, and adds the portal to the main view.
 * @details The camera is the player's own, carried into the portal's target frame. Carrying it
 * rather than pinning it to the target is what gives a portal parallax: leaning to the left of
 * one shows more of what lies to the right of its exit, as a window does.
 * @remarks The cameras are placed before anything is added to the scene, because the renderer
 * repeats each addition into the views of the portals the main view holds. The portals' own
 * brushwork is added afterwards, and unconditionally: nothing on the server references it, so
 * without this a portal face is not drawn at all, feature enabled or not.
 */
void Cg_AddPortals(void) {

  const r_model_t *world = cgi.WorldModel();
  if (!world) {
    return;
  }

  r_bsp_portal_t *p = world->bsp->portals;

  if (cg_portals->integer) {
    for (int32_t i = 0; i < world->bsp->num_portals; i++, p++) {

      if (!p->target_entity) {
        continue;
      }

      r_view_t *view = p->view;

      cgi.InitView(view);

      view->type = VIEW_PORTAL;
      view->viewport = cgi.view->viewport;
      view->fov = cgi.view->fov;
      view->depth_range = cgi.view->depth_range;
      view->ticks = cgi.view->ticks;
      view->ambient = cgi.view->ambient;
      view->clip_plane = p->clip_plane;

      view->origin = Mat4_Transform(p->matrix, cgi.view->origin);
      view->forward = Mat4_TransformVector(p->matrix, cgi.view->forward);
      view->right = Mat4_TransformVector(p->matrix, cgi.view->right);
      view->up = Mat4_TransformVector(p->matrix, cgi.view->up);
      view->angles = Vec3_Euler(view->forward);

      cgi.AddPortal(cgi.view, p);
    }
  }

  // and only then the portals themselves, so that each one reaches every portal's view
  p = world->bsp->portals;
  for (int32_t i = 0; i < world->bsp->num_portals; i++, p++) {

    if (!p->model) {
      continue;
    }

    cgi.AddEntity(cgi.view, &(const r_entity_t) {
      .model = p->model,
      .origin = cgi.EntityValue(p->entity, "origin")->vec3,
      .angles = cgi.EntityValue(p->entity, "angles")->vec3,
      .scale = 1.f,
      .color = Vec4_One(),
      .lerp = 1.f,
    });
  }
}
