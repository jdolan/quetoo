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

#include "cg_local.h"

/**
 * @brief Resolves the model matrix of the entity drawing @p portal's face this frame.
 * @details A portal face's frame is baked in the space of the model containing it, so a portal
 * on a mover -- a `func_bob` teleporter, say -- must be drawn with that entity's
 * current transform.
 * @return `false` if the entity that contains @c portal is not in the current frame (@c SVF_NO_CLIENT, etc.).
 */
static bool Cg_PortalMatrix(const ClientFrame *frame, const RenderSubview *portal, Mat4 *matrix) {

  assert(portal->model);

  for (int32_t i = 0; i < frame->numEntities; i++) {

    const uint32_t snum = (frame->entityState + i) & ENTITY_STATE_MASK;
    const EntityState *s = &cgi.client->entityStates[snum];

    if (cgi.client->models[s->model1] == portal->model) {
      const ClientEntity *ent = &cgi.client->entities[s->number];

      *matrix = Mat4_FromRotationTranslationScale(ent->angles, ent->origin, 1.f);
      return true;
    }
  }

  return false;
}

/**
 * @brief Offers every portal of the world to the main view.
 * @details The renderer keeps the nearest of them, places their cameras, and repeats the scene
 * into each. All that is wanted here is the transform of the entity drawing each portal's face,
 * which only the client game can resolve.
 */
void Cg_AddPortals(const ClientFrame *frame) {

  const RenderModel *world = cgi.WorldModel();
  if (!world) {
    return;
  }

  for (int32_t i = 0; i < world->bsp->numPortals; i++) {

    RenderSubview *p = &world->bsp->portals[i];

    Mat4 matrix;
    if (Cg_PortalMatrix(frame, p, &matrix)) {
      cgi.AddPortal(cgi.view, p, matrix);
    }
  }
}
