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
 * @return True if the server is sending an entity that draws @p model.
 * @details A portal is any brushwork with a `SURF_PORTAL` face, which may well belong to an
 * entity the game already spawns and draws, such as a door. Only a portal that nothing else
 * draws -- one with no counterpart on the server -- is added here.
 */
static bool Cg_IsServerEntity(const r_model_t *model) {

  for (int32_t i = 1; i < MAX_MODELS; i++) {

    if (q_strcmp(cgi.client->config_strings[CS_MODELS + i], model->media.name)) {
      continue;
    }

    for (int32_t j = 0; j < MAX_ENTITIES; j++) {
      if (cgi.client->entities[j].current.model1 == i) {
        return true;
      }
    }

    break;
  }

  return false;
}

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
  for (int32_t i = 0; i < world->bsp->num_portals; i++, p++) {

    r_view_t *view = p->view;

    cgi.InitView(view);

    view->type = VIEW_PORTAL;
    view->viewport = cgi.view->viewport;
    view->fov = cgi.view->fov;
    view->depth_range = cgi.view->depth_range;
    view->ticks = cgi.view->ticks;
    view->ambient = cgi.view->ambient;

    view->origin = Mat4_Transform(p->matrix, cgi.view->origin);
    view->forward = Mat4_TransformVector(p->matrix, cgi.view->forward);
    view->right = Mat4_TransformVector(p->matrix, cgi.view->right);
    view->up = Mat4_TransformVector(p->matrix, cgi.view->up);
    view->angles = Vec3_Euler(view->forward);

    cgi.AddPortal(cgi.view, p);
  }

  // and only then the portals themselves, so that each one reaches every portal's view
  p = world->bsp->portals;
  for (int32_t i = 0; i < world->bsp->num_portals; i++, p++) {

    if (!p->model || Cg_IsServerEntity(p->model)) {
      continue;
    }

    // a model showing more than one portal is still only drawn once
    bool added = false;
    for (int32_t j = 0; j < i; j++) {
      if (world->bsp->portals[j].model == p->model) {
        added = true;
        break;
      }
    }

    if (added) {
      continue;
    }

    cgi.AddEntity(cgi.view, &(const r_entity_t) {
      .model = p->model,
      .scale = 1.f,
      .color = Vec4_One(),
      .lerp = 1.f,
    });
  }
}
