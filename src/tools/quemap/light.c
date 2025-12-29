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

#include "light.h"
#include "points.h"
#include "qlight.h"

GPtrArray *lights = NULL;

/**
 * @brief
 */
static light_t *AllocLight(void) {
  return Mem_TagMalloc(sizeof(light_t), MEM_TAG_LIGHT);
}

/**
 * @brief
 */
static void FreeLight(light_t *light) {
  Mem_Free(light);
}

/**
 * @brief
 */
const cm_entity_t *EntityTarget(const cm_entity_t *entity) {

  const char *targetname = Cm_EntityValue(entity, "target")->nullable_string;
  if (targetname) {

    cm_entity_t **e = Cm_Bsp()->entities;
    for (int32_t i = 0; i < Cm_Bsp()->num_entities; i++, e++) {
      if (!g_strcmp0(targetname, Cm_EntityValue(*e, "targetname")->nullable_string)) {
        return *e;
      }
    }
  }

  return NULL;
}

/**
 * @brief
 */
static light_t *LightForEntity(const cm_entity_t *entity) {

  const char *class_name = Cm_EntityValue(entity, "classname")->string;
  if (!g_strcmp0(class_name, "light")) {

    light_t *light = AllocLight();

    light->entity = Cm_EntityNumber(entity);
    light->origin = Cm_EntityValue(entity, "origin")->vec3;
    light->radius = Cm_EntityValue(entity, "radius")->value ?: LIGHT_RADIUS;
    light->intensity = Cm_EntityValue(entity, "intensity")->value ?: LIGHT_INTENSITY;
    light->color = Cm_EntityValue(entity, "color")->vec3;

    if (Vec3_Equal(Vec3_Zero(), light->color)) {
      light->color = LIGHT_COLOR;
    }

    light->bounds = Box3_Null();

    return light;
  } else {
    return NULL;
  }
}

/**
 * @brief
 */
void FreeLights(void) {

  if (!lights) {
    return;
  }

  g_ptr_array_free(lights, true);
  lights = NULL;
}

/**
 * @brief
 */
void BuildLights(void) {

  const uint32_t start = (uint32_t) SDL_GetTicks();

  Progress("Building lights", 0);

  lights = lights ?: g_ptr_array_new_with_free_func((GDestroyNotify) FreeLight);

  const int32_t count = Cm_Bsp()->num_entities + Cm_Bsp()->num_brush_sides;

  cm_entity_t **entity = Cm_Bsp()->entities;
  for (int32_t i = 0; i < Cm_Bsp()->num_entities; i++, entity++) {
    light_t *light = LightForEntity(*entity);
    if (light) {
      g_ptr_array_add(lights, light);
    }
    Progress("Building lights", i * 100.f / count);
  }

  Com_Print("\r%-24s [100%%] %d ms\n", "Building lights", (uint32_t) SDL_GetTicks() - start);

  Com_Verbose("Lighting for %d lights\n", lights->len);
}

/**
 * @brief
 */
void EmitLights(void) {

  if (!lights) {
    return;
  }

  const uint32_t start = (uint32_t) SDL_GetTicks();

  bsp_file.num_lights = lights->len;

  if (bsp_file.num_lights >= MAX_BSP_LIGHTS) {
    Com_Error(ERROR_FATAL, "MAX_BSP_LIGHTS\n");
  }

  Bsp_AllocLump(&bsp_file, BSP_LUMP_ELEMENTS, MAX_BSP_ELEMENTS);
  Bsp_AllocLump(&bsp_file, BSP_LUMP_LIGHTS, bsp_file.num_lights);

  bsp_light_t *out = bsp_file.lights;
  for (guint i = 0; i < lights->len; i++) {

    light_t *light = g_ptr_array_index(lights, i);

    if (Box3_IsNull(light->bounds)) {
      continue;
    }

    light->out = out;

    out->entity = light->entity;
    out->origin = light->origin;
    out->radius = light->radius;
    out->color = light->color;
    out->intensity = light->intensity;
    out->bounds = light->bounds;

    out->bounds = Box3_Expand(out->bounds, BSP_VOXEL_SIZE * .5f);

    out->first_depth_pass_element = bsp_file.num_elements;

    const bsp_model_t *worldspawn = bsp_file.models;
    const bsp_face_t *face = &bsp_file.faces[worldspawn->first_face];
    for (int32_t j = 0; j < worldspawn->num_faces; j++, face++) {

      if (!Box3_Intersects(face->bounds, out->bounds)) {
        continue;
      }

      const bsp_brush_side_t *side = &bsp_file.brush_sides[face->brush_side];
      if (side->contents & CONTENTS_MIST) {
        continue;
      }

      if (side->surface & SURF_MASK_NO_DRAW_ELEMENTS) {
        continue;
      }

      if (side->surface & SURF_MASK_TRANSLUCENT) {
        continue;
      }

      if (bsp_file.num_elements + face->num_elements >= MAX_BSP_ELEMENTS) {
        Com_Error(ERROR_FATAL, "MAX_BSP_ELEMENTS\n");
      }

      memcpy(bsp_file.elements + bsp_file.num_elements,
             bsp_file.elements + face->first_element,
             sizeof(int32_t) * face->num_elements);

      bsp_file.num_elements += face->num_elements;
      out->num_depth_pass_elements += face->num_elements;
    }

    out++;

    Progress("Emitting lights", 100.f * i / lights->len);
  }

  Com_Print("\r%-24s [100%%] %d ms\n\n", "Emitting lights", (uint32_t) SDL_GetTicks() - start);
}

/**
 * @brief Emit a clustered light grid into the BSP file so the renderer can load it directly.
 * Layout: bsp_lightgrid_header_t header; int32_t meta[num_cells * 2]; int32_t indices[total_indices];
 */
void EmitLightGrid(void) {
  if (!lights) {
    return;
  }

  // use the tool-side voxel representation (voxels) and its STU-space matrix
  const voxels_t *vox = &voxels;
  if (!vox || vox->num_voxels == 0) {
    return;
  }

  const int vx = vox->size.x;
  const int vy = vox->size.y;
  const int vz = vox->size.z;
  const int num_cells = vx * vy * vz;

  // Compute world-space bounds to match what the renderer will calculate
  // The renderer uses: visible_bounds + padding to fit voxel grid
  const bsp_model_t *worldspawn = &bsp_file.models[0];
  const vec3_t voxels_size = Vec3_Scale(Vec3i_CastVec3(vox->size), BSP_VOXEL_SIZE);
  const vec3_t world_size = Box3_Size(worldspawn->visible_bounds);
  const vec3_t padding = Vec3_Scale(Vec3_Subtract(voxels_size, world_size), 0.5f);
  const box3_t world_bounds = Box3_Expand3(worldspawn->visible_bounds, padding);
  const vec3_t world_voxel_size = Vec3(BSP_VOXEL_SIZE, BSP_VOXEL_SIZE, BSP_VOXEL_SIZE);

  // counts per cell
  int32_t *counts = Mem_TagMalloc(num_cells * sizeof(int32_t), MEM_TAG_QLIGHT);
  memset(counts, 0, num_cells * sizeof(int32_t));

  // We need the BSP lights array populated in bsp_file.lights
  for (int li = 0; li < bsp_file.num_lights; li++) {
    const bsp_light_t *L = &bsp_file.lights[li];

    const vec3_t min_world = Vec3_Subtract(L->origin, Vec3(L->radius, L->radius, L->radius));
    const vec3_t max_world = Vec3_Add(L->origin, Vec3(L->radius, L->radius, L->radius));

    // Use world space directly, like the renderer does at runtime
    const vec3_t rel_min = Vec3_Subtract(min_world, world_bounds.mins);
    const vec3_t rel_max = Vec3_Subtract(max_world, world_bounds.mins);

    const int ix0 = CLAMP((int) floorf(rel_min.x / world_voxel_size.x), 0, vx - 1);
    const int iy0 = CLAMP((int) floorf(rel_min.y / world_voxel_size.y), 0, vy - 1);
    const int iz0 = CLAMP((int) floorf(rel_min.z / world_voxel_size.z), 0, vz - 1);
    const int ix1 = CLAMP((int) floorf(rel_max.x / world_voxel_size.x), 0, vx - 1);
    const int iy1 = CLAMP((int) floorf(rel_max.y / world_voxel_size.y), 0, vy - 1);
    const int iz1 = CLAMP((int) floorf(rel_max.z / world_voxel_size.z), 0, vz - 1);

    const float r2 = L->radius * L->radius;

    for (int z = iz0; z <= iz1; z++) {
      for (int y = iy0; y <= iy1; y++) {
        for (int x = ix0; x <= ix1; x++) {
          const int cell = (z * vy + y) * vx + x;

          const vec3_t center = Vec3_Add(world_bounds.mins, 
                                         Vec3_Multiply(world_voxel_size, Vec3(x + 0.5f, y + 0.5f, z + 0.5f)));

          if (Vec3_DistanceSquared(center, L->origin) <= r2) {
            counts[cell]++;
          }
        }
      }
    }
  }

  int total = 0;
  int32_t *offsets = Mem_TagMalloc(num_cells * sizeof(int32_t), MEM_TAG_QLIGHT);
  for (int i = 0; i < num_cells; i++) {
    offsets[i] = total;
    total += counts[i];
  }

  int32_t *indices = NULL;
  int32_t *meta = NULL;

  if (total > 0) {
    indices = Mem_TagMalloc(total * sizeof(int32_t), MEM_TAG_QLIGHT);
    meta = Mem_TagMalloc(num_cells * 2 * sizeof(int32_t), MEM_TAG_QLIGHT);
    memset(meta, 0, num_cells * 2 * sizeof(int32_t));

    int32_t *cursor = Mem_TagMalloc(num_cells * sizeof(int32_t), MEM_TAG_QLIGHT);
    memcpy(cursor, offsets, num_cells * sizeof(int32_t));

    for (int li = 0; li < bsp_file.num_lights; li++) {
      const bsp_light_t *L = &bsp_file.lights[li];

      const vec3_t min_world = Vec3_Subtract(L->origin, Vec3(L->radius, L->radius, L->radius));
      const vec3_t max_world = Vec3_Add(L->origin, Vec3(L->radius, L->radius, L->radius));

      // Use world space directly, like the renderer does at runtime
      const vec3_t rel_min = Vec3_Subtract(min_world, world_bounds.mins);
      const vec3_t rel_max = Vec3_Subtract(max_world, world_bounds.mins);

      const int ix0 = CLAMP((int) floorf(rel_min.x / world_voxel_size.x), 0, vx - 1);
      const int iy0 = CLAMP((int) floorf(rel_min.y / world_voxel_size.y), 0, vy - 1);
      const int iz0 = CLAMP((int) floorf(rel_min.z / world_voxel_size.z), 0, vz - 1);
      const int ix1 = CLAMP((int) floorf(rel_max.x / world_voxel_size.x), 0, vx - 1);
      const int iy1 = CLAMP((int) floorf(rel_max.y / world_voxel_size.y), 0, vy - 1);
      const int iz1 = CLAMP((int) floorf(rel_max.z / world_voxel_size.z), 0, vz - 1);

      const float r2 = L->radius * L->radius;

      for (int z = iz0; z <= iz1; z++) {
        for (int y = iy0; y <= iy1; y++) {
          for (int x = ix0; x <= ix1; x++) {
            const int cell = (z * vy + y) * vx + x;

            const vec3_t center = Vec3_Add(world_bounds.mins, 
                                           Vec3_Multiply(world_voxel_size, Vec3(x + 0.5f, y + 0.5f, z + 0.5f)));

            if (Vec3_DistanceSquared(center, L->origin) <= r2) {
              const int pos = cursor[cell]++;
              indices[pos] = li;
              meta[cell * 2 + 1]++;
            }
          }
        }
      }
    }

    for (int i = 0; i < num_cells; i++) {
      meta[i * 2 + 0] = offsets[i];
    }

    Mem_Free(cursor);
  }

  // write into bsp_file.lightgrid
  if (bsp_file.lightgrid) {
    Mem_Free(bsp_file.lightgrid);
    bsp_file.lightgrid = NULL;
    bsp_file.lightgrid_size = 0;
  }

  const size_t header_size = sizeof(bsp_lightgrid_header_t);
  const size_t meta_size = num_cells * 2 * sizeof(int32_t);
  const size_t indices_size = total * sizeof(int32_t);

  bsp_file.lightgrid_size = (int32_t) (header_size + meta_size + indices_size);
  bsp_file.lightgrid = Mem_TagMalloc(bsp_file.lightgrid_size, MEM_TAG_QLIGHT);

  byte *out = bsp_file.lightgrid;
  bsp_lightgrid_header_t hdr;
  hdr.size_x = vx;
  hdr.size_y = vy;
  hdr.size_z = vz;
  hdr.total_indices = total;

  memcpy(out, &hdr, header_size);
  out += header_size;

  if (meta) {
    memcpy(out, meta, meta_size);
    out += meta_size;
  }

  if (indices) {
    memcpy(out, indices, indices_size);
    out += indices_size;
  }

  Mem_Free(counts);
  Mem_Free(offsets);
  Mem_Free(indices);
  Mem_Free(meta);

  Com_Debug(DEBUG_ALL, "Emitted light grid: cells=%d indices=%d\n", num_cells, total);
}
