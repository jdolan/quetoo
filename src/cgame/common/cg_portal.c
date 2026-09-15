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
 * @remarks This runs before anything is added to the scene, because the renderer repeats each
 * addition into the views of the portals the main view holds. A portal's own brushwork is drawn
 * by whatever entity owns it, as any other brushwork is.
 */
void Cg_AddPortals(void) {

  const r_model_t *world = cgi.WorldModel();
  if (!world) {
    return;
  }

  const int32_t num_portals = world->bsp->num_portals;

  // the renderer has fewer views than a map may hold portals, and offers them before the scene
  // has been culled, so the ones nearest the camera are offered first
  int32_t order[MAX_BSP_PORTALS];
  for (int32_t i = 0; i < num_portals; i++) {
    order[i] = i;
  }

  for (int32_t i = 1; i < num_portals; i++) {
    const int32_t o = order[i];
    const float d = Vec3_DistanceSquared(world->bsp->portals[o].origin, cgi.view->origin);

    int32_t j = i;
    while (j > 0 && Vec3_DistanceSquared(world->bsp->portals[order[j - 1]].origin, cgi.view->origin) > d) {
      order[j] = order[j - 1];
      j--;
    }

    order[j] = o;
  }

  for (int32_t i = 0; i < num_portals; i++) {

    r_bsp_portal_t *p = &world->bsp->portals[order[i]];

    r_view_t *view = cgi.AddPortal(cgi.view, p);
    if (!view) {
      continue;
    }

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
    // Vec3_Euler recovers pitch and yaw but leaves roll at zero, while the carried basis has
    // whatever roll the two frames differ by. Anything rebuilding a basis from these angles,
    // such as an all-axis sprite, would otherwise be rotated wrongly in a rolled portal
    view->angles = Vec3_Euler(view->forward);

    vec3_t right, up;
    Vec3_Vectors(view->angles, NULL, &right, &up);

    view->angles.z = Degrees(atan2f(Vec3_Dot(view->up, right), Vec3_Dot(view->up, up)));
  }
}
