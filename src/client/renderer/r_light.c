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
  out->mins = Vec3_ToVec4(in->bounds.mins, 1.f);
  out->maxs = Vec3_ToVec4(in->bounds.maxs, 1.f);
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

    if (l->query && l->query->result == 0) {
      r_stats.lights_occluded++;
      continue;
    }

    R_AddLightUniform(view, l);
    r_stats.lights_visible++;

    if (r_draw_light_bounds->value && Vec3_Distance(tr.end, l->origin) < 64.f) {
      R_Draw3DBox(l->bounds, Color3fv(l->color), false);
    }
  }
}

/**
 * @brief Writes the indexes of the lights that intersect bounds to the given uniform name.
 */
void R_ActiveLights(const r_view_t *view, const box3_t bounds, GLint name) {

  GLint active_lights[ MAX_LIGHTS ];
  GLint num_active_lights = 0;

  memset(active_lights, -1, sizeof(active_lights));

#if 0
  const r_bsp_model_t *bsp = r_models.world ? r_models.world->bsp : NULL;

  // If we have a CPU-side light grid, use it to accelerate selection.
  if (bsp && bsp->voxel_light_data && bsp->voxels_voxel_indices && bsp->voxels_voxel_num_cells > 0 && bsp->voxels) {

    const int32_t num_bsp_lights = bsp->num_lights;

    // Map BSP light index -> view light index
    int32_t *bsp_map = Mem_Malloc(num_bsp_lights * sizeof(int32_t));
    for (int32_t i = 0; i < num_bsp_lights; i++) {
      bsp_map[i] = -1;
    }

    for (int32_t i = 0; i < view->num_lights; i++) {
      const r_light_t *vl = &view->lights[i];
      if (vl->bsp_light) {
        const ptrdiff_t idx = vl->bsp_light - bsp->lights;
        if (idx >= 0 && idx < num_bsp_lights) {
          bsp_map[idx] = i;
        }
      }
    }

    // visited flags to avoid duplicates
    uint8_t *visited = Mem_Malloc(num_bsp_lights * sizeof(uint8_t));
    memset(visited, 0, num_bsp_lights * sizeof(uint8_t));

    const r_bsp_voxels_t *vox = bsp->voxels;
    vec3_t rel_min = Vec3_Subtract(bounds.mins, vox->bounds.mins);
    vec3_t rel_max = Vec3_Subtract(bounds.maxs, vox->bounds.mins);

    vec3i_t imin = Vec3_CastVec3i(Vec3_Divide(rel_min, vox->voxel_size));
    vec3i_t imax = Vec3_CastVec3i(Vec3_Divide(rel_max, vox->voxel_size));

    imin.x = CLAMP(imin.x, 0, vox->size.x - 1);
    imin.y = CLAMP(imin.y, 0, vox->size.y - 1);
    imin.z = CLAMP(imin.z, 0, vox->size.z - 1);
    imax.x = CLAMP(imax.x, 0, vox->size.x - 1);
    imax.y = CLAMP(imax.y, 0, vox->size.y - 1);
    imax.z = CLAMP(imax.z, 0, vox->size.z - 1);

    const int vx = vox->size.x;
    const int vy = vox->size.y;

    for (int z = imin.z; z <= imax.z && num_active_lights < MAX_LIGHTS - 1; z++) {
      for (int y = imin.y; y <= imax.y && num_active_lights < MAX_LIGHTS - 1; y++) {
        for (int x = imin.x; x <= imax.x && num_active_lights < MAX_LIGHTS - 1; x++) {
          const int cell = (z * vy + y) * vx + x;
          const int32_t offset = bsp->voxel_light_data[cell * 2 + 0];
          const int32_t count  = bsp->voxel_light_data[cell * 2 + 1];

          for (int32_t k = 0; k < count && num_active_lights < MAX_LIGHTS - 1; k++) {
            const int32_t bsp_idx = bsp->voxels_voxel_indices[offset + k];
            if (bsp_idx < 0 || bsp_idx >= num_bsp_lights) {
              continue;
            }
            if (visited[bsp_idx]) {
              continue;
            }

            visited[bsp_idx] = 1;

            const int view_idx = bsp_map[bsp_idx];
            if (view_idx >= 0) {
              active_lights[num_active_lights++] = view_idx;
            }
          }
        }
      }
    }

    Mem_Free(bsp_map);
    Mem_Free(visited);

    // terminate
    if (num_active_lights < MAX_LIGHTS) {
      active_lights[num_active_lights++] = -1;
    }

    glUniform1iv(name, num_active_lights, active_lights);
    R_GetError(NULL);
    return;
  }

#endif
  // Fallback: brute force
  num_active_lights = 0;
  for (int32_t i = 0; i < view->num_lights; i++) {
    const r_light_t *l = &view->lights[i];
    if (l->query && l->query->result == 0) {
      continue;
    }
    if (Box3_Intersects(l->bounds, bounds)) {
      active_lights[num_active_lights++] = i;
    }
  }

  if (num_active_lights < MAX_LIGHTS) {
    active_lights[num_active_lights++] = -1;
  }

  glUniform1iv(name, num_active_lights, active_lights);
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
