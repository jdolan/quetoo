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

r_lights_t r_lights;

/**
 * @brief Adds a light source to the view's light list.
 */
void R_AddLight(r_view_t *view, const r_light_t *l) {

  if (view->num_lights == MAX_LIGHTS) {
    Com_Debug(DEBUG_RENDERER, "MAX_LIGHTS\n");
    return;
  }

  r_light_t *out = &view->lights[view->num_lights++];

  *out = *l;
}

/**
 * @brief Transform lights into their uniform representation and upload them.
 */
void R_UpdateLights(r_view_t *view) {

  r_light_uniform_block_t *out = &r_lights.block;
  memset(out, 0, sizeof(*out));

  cm_trace_t tr = { 0 };
  if (r_draw_light_bounds->value) {
    const vec3_t end = Vec3_Fmaf(view->origin, MAX_WORLD_DIST, view->forward);
    tr = Cm_BoxTrace(view->origin, end, Box3_Zero(), 0, CONTENTS_SOLID);
  }

  const float inv_layer = r_shadow_atlas.layer_size > 0 ? 1.f / (float) r_shadow_atlas.layer_size : 0.f;

  r_light_t *l = view->lights;
  for (int32_t i = 0; i < view->num_lights; i++, l++) {

    if (l->query) {
      l->occluded = l->query->result == 0;
    } else {
      l->occluded = R_CulludeBox(view, l->bounds);
    }

    if (l->occluded) {
      r_stats.lights_occluded++;
    } else {
      r_stats.lights_visible++;

      r_light_uniform_t *uniform = &out->lights[i];
      uniform->origin = Vec3_ToVec4(l->origin, l->radius);
      uniform->color = Vec3_ToVec4(l->color, l->intensity);

      if (r_shadow_atlas.tile_size > 0) {
        const int32_t local_index = i % r_shadow_atlas.lights_per_layer;
        const int32_t light_col = local_index % r_shadow_atlas.lights_per_row;
        const int32_t light_row = local_index / r_shadow_atlas.lights_per_row;
        const float base_x = (float) (light_col * 3 * r_shadow_atlas.tile_size) * inv_layer;
        const float base_y = (float) (light_row * 2 * r_shadow_atlas.tile_size) * inv_layer;
        const float tile_uv = (float) r_shadow_atlas.tile_size * inv_layer;
        const float layer = (float) (i / r_shadow_atlas.lights_per_layer);
        uniform->shadow = Vec4(base_x, base_y, tile_uv, layer);
      } else {
        uniform->shadow = Vec4(0.f, 0.f, 0.f, 0.f);
      }
    }

    if (r_draw_light_bounds->value && Vec3_Distance(tr.end, l->origin) < 64.f) {
      R_Draw3DBox(l->bounds, Color3fv(l->color), false);
    }
  }

  const GLsizei size = view->num_lights * sizeof(r_light_uniform_t);

  glBindBuffer(GL_UNIFORM_BUFFER, r_lights.buffer);
  glBufferSubData(GL_UNIFORM_BUFFER, 0, size, &r_lights.block);
  glBindBuffer(GL_UNIFORM_BUFFER, 0);

  R_GetError(NULL);
}

/**
 * @brief Writes the indexes of active lights intersecting bounds to the given uniform name.
 * @details In normal gameplay, these are dynamic light sources (rockets, explosions, etc).
 * @details When the in-game editor is enabled, all lights use this code path.
 * @remarks The uniform must be an `int[MAX_LIGHTS]`.
 */
void R_ActiveLights(const r_view_t *view, const box3_t bounds, GLint name) {

  GLint active_lights[MAX_LIGHTS];
  GLint len = 0;

  const r_light_t *l = view->lights;
  for (int32_t i = 0; i < view->num_lights; i++, l++) {

    if (l->bsp_light) {
      continue;
    }

    if (Box3_Intersects(l->bounds, bounds)) {
      active_lights[len++] = i;
    }
  }

  if (len < MAX_LIGHTS) {
    active_lights[len++] = -1;
  }

  glUniform1iv(name, len, active_lights);
  R_GetError(NULL);
}

/**
 * @brief Initializes the light uniform buffer object.
 */
void R_InitLights(void) {

  memset(&r_lights, 0, sizeof(r_lights));

  glGenBuffers(1, &r_lights.buffer);
  glBindBuffer(GL_UNIFORM_BUFFER, r_lights.buffer);
  glBufferData(GL_UNIFORM_BUFFER, sizeof(r_lights.block), &r_lights.block, GL_DYNAMIC_DRAW);
  glBindBufferBase(GL_UNIFORM_BUFFER, 1, r_lights.buffer);
  glBindBuffer(GL_UNIFORM_BUFFER, 0);

  R_GetError(NULL);
}

/**
 * @brief Frees the light uniform buffer object.
 */
void R_ShutdownLights(void) {

  glDeleteBuffers(1, &r_lights.buffer);
}
