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
 * @brief
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
 * @brief
 */
static void R_AddLightUniform(r_view_t *view, r_light_t *in) {

  const ptrdiff_t index = in - view->lights;
  r_light_uniform_t *out = &r_lights.block.lights[index];

  out->origin = Vec3_ToVec4(in->origin, in->radius);
  out->color = Vec3_ToVec4(in->color, in->intensity);
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
      R_AddLightUniform(view, l);
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
 * @brief Writes the indexes of dynamic lights intersecting bounds to the given uniform name.
 */
void R_DynamicLights(const r_view_t *view, const box3_t bounds, GLint name) {

  GLint dynamic_lights[MAX_LIGHTS];
  GLint len = 0;

  const r_light_t *l = view->lights;
  for (int32_t i = 0; i < view->num_lights; i++, l++) {

    if (l->bsp_light) {
      continue;
    }

    if (Box3_Intersects(l->bounds, bounds)) {
      dynamic_lights[len++] = i;
    }
  }

  if (len < MAX_LIGHTS) {
    dynamic_lights[len++] = -1;
  }

  glUniform1iv(name, len, dynamic_lights);
  R_GetError(NULL);
}

/**
 * @brief
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
 * @brief
 */
void R_ShutdownLights(void) {

  glDeleteBuffers(1, &r_lights.buffer);
}
