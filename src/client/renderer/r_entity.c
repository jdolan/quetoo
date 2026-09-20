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
 * @brief Updates the entity's world-space bounds.
 */
static void R_SetEntityBounds(RenderEntity *e) {
  if (e->model && !Box3_IsNull(e->model->bounds)) {
    e->absModelBounds = Mat4_TransformBounds(e->matrix, e->model->bounds);
  } else {
    e->absModelBounds = e->absBounds;
  }
}

/**
 * @brief Tests whether the entity should be culled.
 */
bool R_CullEntity(const RenderView *view, const RenderEntity *e) {

  if (view->type == VIEW_PLAYER_MODEL) {
    return false;
  }

  if (e->parent) {
    return false;
  }

  if (e->effects & (EF_WORLD | EF_SELF | EF_WEAPON)) {
    return false;
  }

  if (Box3_IsNull(e->absModelBounds)) {
    return true;
  }

  if (R_CulludeBox(view, e->absModelBounds)) {
    return true;
  }

  return false;
}

/**
 * @brief Adds an entity to the view and returns the copied entry.
 */
RenderEntity *R_AddEntity(RenderView *view, const RenderEntity *ent) {

  assert(view);
  assert(ent);

  if (view->numEntities == MAX_ENTITIES) {
    Com_Warn("MAX_ENTITIES\n");
    return NULL;
  }

  RenderEntity *e = &view->entities[view->numEntities];
  *e = *ent;

  e->matrix = Mat4_FromRotationTranslationScale(e->angles, e->origin, e->scale);

  if (IS_MESH_MODEL(e->model)) {

    if (e->parent && e->tag) {
      R_ApplyMeshTag(e);
    }

    R_ApplyMeshConfig(e);
  }

  e->inverseMatrix = Mat4_Inverse(e->matrix);

  R_SetEntityBounds(e);

  view->numEntities++;

  return e;
}

/**
 * @brief Updates entity state for the frame.
 */
void R_UpdateEntities(RenderView *view, CopyPass *pass) {

  RenderEntity *e = view->entities;
  for (int32_t i = 0; i < view->numEntities; i++, e++) {

    if (e->model == NULL) {
      continue;
    }

    R_ActiveDynamicLights(view, e->absModelBounds, &e->activeDynamicLights);
  }
}

/**
 * @brief Draws the view's entities.
 */
void R_DrawEntities(const RenderView *view, RenderPass *pass) {

  R_DrawOpaqueBspEntities(view, pass);

  R_DrawMeshEntities(view, pass);

  R_DrawDecals(view, pass);

  R_DrawBlendBspEntities(view, pass);
}
