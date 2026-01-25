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

#include "bsp.h"
#include "points.h"
#include "qlight.h"
#include "simplex.h"
#include "voxel.h"

voxels_t voxels;

/**
 * @brief Accumulates light color into the voxel for fog absorption calculations.
 */
static void IlluminateVoxel(voxel_t *voxel, light_t *light) {

  const float dist = Vec3_Distance(voxel->origin, light->origin);
  const float atten = Clampf01(1.f - dist / light->radius);
  const float lumens = atten * atten;

  const vec3_t color = Vec3_Scale(light->color, light->intensity);
  voxel->diffuse.xyz = Vec3_Fmaf(voxel->diffuse.xyz, lumens, color);

  light->visible_bounds = Box3_Union(light->visible_bounds, voxel->bounds);

  g_hash_table_add(voxel->lights, light);
}

/**
 * @brief Create an `SDL_Surface` with the given voxel data.
 */
SDL_Surface *CreateVoxelSurface(int32_t w, int32_t h, size_t voxel_size, void *voxels) {

  SDL_Surface *surface = SDL_malloc(sizeof(SDL_Surface));

  surface->flags = SDL_SURFACE_PREALLOCATED;
  surface->w = w;
  surface->h = h;
  surface->reserved = (void *) voxel_size;
  surface->pixels = voxels;

  return surface;
}

/**
 * @brief Blits the `src` to the given `rect` in `dest`.
 */
int32_t BlitVoxelSurface(const SDL_Surface *src, SDL_Surface *dest, const SDL_Rect *rect) {

  assert(src);
  assert(src->pixels);

  assert(dest);
  assert(dest->pixels);

  assert(src->reserved == dest->reserved);

  assert(rect);

  assert(src->w == rect->w);
  assert(src->h == rect->h);

  const size_t voxel_size = (size_t) src->reserved;

  const byte *in = src->pixels;
  byte *out = dest->pixels;

  out += rect->y * dest->w * voxel_size + rect->x * voxel_size;

  for (int32_t x = 0; x < src->w; x++) {
    for (int32_t y = 0; y < src->h; y++) {

      const byte *in_voxel = in + y * src->w * voxel_size + x * voxel_size;
      byte *out_voxel = out + y * dest->w * voxel_size + x * voxel_size;

      memcpy(out_voxel, in_voxel, voxel_size);
    }
  }

  return 0;
}

/**
 * @brief Writes the voxel surface to a temporary file for debugging.
 */
int32_t WriteVoxelSurface(const SDL_Surface *in, const char *name) {

  assert(in);
  assert(in->pixels);

  SDL_Surface *out;

  const size_t voxel_size = (size_t) in->reserved;
  switch (voxel_size) {

    case sizeof(vec3_t): {
      const vec3_t *in_voxel = (vec3_t *) in->pixels;

      out = SDL_CreateSurface(in->w, in->h, SDL_PIXELFORMAT_RGB24);
      color24_t *out_voxel = (color24_t *) out->pixels;

      for (int32_t x = 0; x < in->w; x++) {
        for (int32_t y = 0; y < in->h; y++) {
          *out_voxel++ = Color_Color24(Color3fv(*in_voxel++));
        }
      }
    }
      break;

    case sizeof(color24_t): {
      const color24_t *in_voxel = (color24_t *) in->pixels;

      out = SDL_CreateSurface(in->w, in->h, SDL_PIXELFORMAT_RGB24);
      color24_t *out_voxel = (color24_t *) out->pixels;

      for (int32_t x = 0; x < in->w; x++) {
        for (int32_t y = 0; y < in->h; y++) {
          *out_voxel++ = *in_voxel++;
        }
      }
    }
      break;

    case sizeof(color32_t): {
      const color32_t *in_voxel = (color32_t *) in->pixels;

      out = SDL_CreateSurface(in->w, in->h, SDL_PIXELFORMAT_RGBA32);
      color32_t *out_voxel = (color32_t *) out->pixels;

      for (int32_t x = 0; x < in->w; x++) {
        for (int32_t y = 0; y < in->h; y++) {
          *out_voxel++ = *in_voxel++;
        }
      }
    }
      break;
  }

  const int32_t err = IMG_SavePNG(out, name);

  SDL_DestroySurface(out);

  return err;
}

/**
 * @brief Builds the voxel grid aligned to world coordinates at `BSP_VOXEL_SIZE` intervals.
 * Voxels are placed at ..., -64, -32, 0, 32, 64, 96, ... in all axes.
 */
static void BuildVoxelExtents(void) {

  const bsp_model_t *world = bsp_file.models;
  
  // Align mins to voxel grid (round down to nearest multiple of BSP_VOXEL_SIZE)
  voxels.stu_bounds.mins.x = floorf(world->visible_bounds.mins.x / BSP_VOXEL_SIZE) * BSP_VOXEL_SIZE;
  voxels.stu_bounds.mins.y = floorf(world->visible_bounds.mins.y / BSP_VOXEL_SIZE) * BSP_VOXEL_SIZE;
  voxels.stu_bounds.mins.z = floorf(world->visible_bounds.mins.z / BSP_VOXEL_SIZE) * BSP_VOXEL_SIZE;
  
  // Align maxs to voxel grid (round up to nearest multiple of BSP_VOXEL_SIZE)
  voxels.stu_bounds.maxs.x = ceilf(world->visible_bounds.maxs.x / BSP_VOXEL_SIZE) * BSP_VOXEL_SIZE;
  voxels.stu_bounds.maxs.y = ceilf(world->visible_bounds.maxs.y / BSP_VOXEL_SIZE) * BSP_VOXEL_SIZE;
  voxels.stu_bounds.maxs.z = ceilf(world->visible_bounds.maxs.z / BSP_VOXEL_SIZE) * BSP_VOXEL_SIZE;
  
  // Calculate grid size (number of voxels in each dimension)
  for (int32_t i = 0; i < 3; i++) {
    voxels.size.xyz[i] = (int32_t) ((voxels.stu_bounds.maxs.xyz[i] - voxels.stu_bounds.mins.xyz[i]) / BSP_VOXEL_SIZE);
  }
}

/**
 * @brief
 */
static void BuildVoxelVoxels(void) {

  voxels.num_voxels = voxels.size.x * voxels.size.y * voxels.size.z;

  if (voxels.num_voxels > MAX_BSP_VOXELS) {
    Com_Error(ERROR_FATAL, "MAX_BSP_VOXELS\n");
  }

  voxels.voxels = Mem_TagMalloc(voxels.num_voxels * sizeof(voxel_t), MEM_TAG_VOXEL);

  voxel_t *v = voxels.voxels;

  for (int32_t z = 0; z < voxels.size.z; z++) {
    for (int32_t y = 0; y < voxels.size.y; y++) {
      for (int32_t x = 0; x < voxels.size.x; x++) {

        const int32_t index = (z * voxels.size.y + y) * voxels.size.x + x;
        v = &voxels.voxels[index];

        v->xyz = Vec3i(x, y, z);

        v->origin = Vec3(
          voxels.stu_bounds.mins.x + (v->xyz.x + 0.5f) * BSP_VOXEL_SIZE,
          voxels.stu_bounds.mins.y + (v->xyz.y + 0.5f) * BSP_VOXEL_SIZE,
          voxels.stu_bounds.mins.z + (v->xyz.z + 0.5f) * BSP_VOXEL_SIZE
        );

        v->bounds = Box3_FromCenterRadius(v->origin, BSP_VOXEL_SIZE * .5f);

        v->lights = g_hash_table_new(g_direct_hash, g_direct_equal);
      }
    }
  }
}

/**
 * @brief Authors a .map file which can be imported into Radiant to view the voxel projections.
 */
static void DebugVoxels(void) {
#if 1
  const char *path = va("maps/%s.voxel.map", map_base);

  file_t *file = Fs_OpenWrite(path);
  if (file == NULL) {
    Com_Warn("Failed to open %s\n", path);
    return;
  }

  voxel_t *v = voxels.voxels;
  for (size_t i = 0; i < voxels.num_voxels; i++, v++) {

    Fs_Print(file, "{\n");
    Fs_Print(file, "  \"classname\" \"info_voxel\"\n");
    Fs_Print(file, "  \"origin\" \"%g %g %g\"\n", v->origin.x, v->origin.y, v->origin.z);
    Fs_Print(file, "  \"x\" \"%d\"\n", v->xyz.x);
    Fs_Print(file, "  \"y\" \"%d\"\n", v->xyz.y);
    Fs_Print(file, "  \"z\" \"%d\"\n", v->xyz.z);
    Fs_Print(file, "}\n");
  }

  Fs_Close(file);
#endif
}

/**
 * @brief
 */
size_t BuildVoxels(void) {

  memset(&voxels, 0, sizeof(voxels));

  BuildVoxelExtents();

  BuildVoxelVoxels();

  DebugVoxels();

  return voxels.num_voxels;
}

/**
 * @brief Assigns lights to a voxel based on visibility traces to corners and center.
 */
void LightVoxel(int32_t voxel_num) {

  voxel_t *voxel = &voxels.voxels[voxel_num];

  vec3_t points[9];
  points[0] = voxel->origin;
  Box3_ToPoints(voxel->bounds, &points[1]);

  for (guint i = 0; i < lights->len; i++) {

    light_t *light = g_ptr_array_index(lights, i);

    if (!Box3_Intersects(light->bounds, voxel->bounds)) {
      continue;
    }

    for (size_t j = 0; j < lengthof(points); j++) {
      const cm_trace_t trace = Light_Trace(light->origin, points[j], 0, CONTENTS_SOLID);
      if (trace.fraction == 1.f || Box3_ContainsPoint(voxel->bounds, trace.end)) {
        IlluminateVoxel(voxel, light);
        break;
      }
    }
  }
}

/**
 * @brief Applies fog to a voxel by sampling corners and center.
 */
void FogVoxel(int32_t voxel_num) {

  voxel_t *voxel = &voxels.voxels[voxel_num];

  vec3_t points[9];
  points[0] = voxel->origin;
  Box3_ToPoints(voxel->bounds, &points[1]);

  const float weight = 1.f / lengthof(points);

  const fog_t *fog = (fog_t *) fogs->data;
  for (guint i = 0; i < fogs->len; i++, fog++) {

    for (size_t j = 0; j < lengthof(points); j++) {

      float density = Clampf01(fog->density * weight * FOG_DENSITY_SCALAR);

      switch (fog->type) {
        case FOG_VOLUME:
          if (!PointInsideFog(points[j], fog)) {
            density = 0.f;
          }
          break;
        default:
          break;
      }

      if (density == 0.f) {
        continue;
      }

      const vec3_t color = Vec3_Fmaf(fog->color, fog->absorption, voxel->diffuse.xyz);
      voxel->fog = Vec4_Add(voxel->fog, Vec3_ToVec4(color, density));
    }
  }
}

/**
 * @brief Calculates caustics intensity based on proximity to liquid contents.
 */
void CausticsVoxel(int32_t voxel_num) {

  voxel_t *voxel = &voxels.voxels[voxel_num];
  
  const int32_t contents = Cm_BoxContents(voxel->bounds, 0);
  
  // If this voxel is liquid, full caustics
  if (contents & CONTENTS_MASK_LIQUID) {
    voxel->caustics = 1.f;
    return;
  }
  
  // Search nearby voxels for liquid, attenuate by distance
  const int32_t search_radius = 4; // Search up to 4 voxels away
  float min_dist = (float)(search_radius + 1);
  
  for (int32_t dz = -search_radius; dz <= search_radius; dz++) {
    for (int32_t dy = -search_radius; dy <= search_radius; dy++) {
      for (int32_t dx = -search_radius; dx <= search_radius; dx++) {
        
        if (dx == 0 && dy == 0 && dz == 0) continue;
        
        const vec3i_t neighbor_xyz = Vec3i_Add(voxel->xyz, Vec3i(dx, dy, dz));
        
        // Check bounds
        if (neighbor_xyz.x < 0 || neighbor_xyz.x >= voxels.size.x ||
            neighbor_xyz.y < 0 || neighbor_xyz.y >= voxels.size.y ||
            neighbor_xyz.z < 0 || neighbor_xyz.z >= voxels.size.z) {
          continue;
        }
        
        const int32_t neighbor_index = (neighbor_xyz.z * voxels.size.y + neighbor_xyz.y) * voxels.size.x + neighbor_xyz.x;
        const voxel_t *neighbor = &voxels.voxels[neighbor_index];
        
        const int32_t neighbor_contents = Cm_BoxContents(neighbor->bounds, 0);
        if (neighbor_contents & CONTENTS_MASK_LIQUID) {
          const float dist = sqrtf((float)(dx * dx + dy * dy + dz * dz));
          if (dist < min_dist) {
            min_dist = dist;
          }
        }
      }
    }
  }
  
  // Linear attenuation: 1.0 at distance 0, 0.0 at search_radius
  if (min_dist <= search_radius) {
    voxel->caustics = Clampf01(1.f - min_dist / (float)search_radius);
  }
}

/**
 * @brief Calculates exposure based on sky visibility.
 */
void ExposureVoxel(int32_t voxel_num) {

  voxel_t *voxel = &voxels.voxels[voxel_num];
  
  // Use dome vectors to sample hemisphere for better coverage
  static const vec3_t dome_vectors[] = DOME_COSINE_16X;
  
  float exposure_sum = 0.f;
  
  for (size_t i = 0; i < lengthof(dome_vectors); i++) {
    const vec3_t start = voxel->origin;
    const vec3_t dir = Vec3_Scale(dome_vectors[i], 32768.f);
    const vec3_t end = Vec3_Add(start, dir);
    
    const cm_trace_t trace = Light_Trace(start, end, 0, CONTENTS_SOLID);
    
    // If we hit sky or nothing, count as exposed
    if (trace.surface & SURF_SKY || trace.fraction == 1.f) {
      exposure_sum += 1.f;
    } else {
      // Partial exposure based on distance traveled
      exposure_sum += trace.fraction * 0.5f;
    }
  }
  
  voxel->exposure = exposure_sum / (float)lengthof(dome_vectors);
}

/**
 * @brief
 */
void EmitVoxels(void) {

  const uint32_t start = (uint32_t) SDL_GetTicks();

  voxels.num_light_indices = 0;

  voxel_t *v = voxels.voxels;
  int32_t min_lights = INT32_MAX, max_lights = 0;
  size_t total_lights = 0;
  
  for (size_t i = 0; i < voxels.num_voxels; i++, v++) {
    v->lights_offset = (int32_t) voxels.num_light_indices;
    v->lights_count = (int32_t) g_hash_table_size(v->lights);
    voxels.num_light_indices += v->lights_count;
    
    // Track statistics
    total_lights += v->lights_count;
    if (v->lights_count < min_lights) min_lights = v->lights_count;
    if (v->lights_count > max_lights) max_lights = v->lights_count;
  }
  
  Com_Verbose("Voxel light stats: min=%d max=%d avg=%.1f total=%zd\n",
              min_lights, max_lights, (float)total_lights / voxels.num_voxels, total_lights);

  bsp_file.voxels_size = sizeof(bsp_voxels_t);
  bsp_file.voxels_size += voxels.num_voxels * sizeof(byte) * 2; // caustics + exposure (RG)
  bsp_file.voxels_size += voxels.num_voxels * sizeof(color32_t); // fog
  bsp_file.voxels_size += voxels.num_voxels * sizeof(int32_t) * 2; // light indices offset and count
  bsp_file.voxels_size += voxels.num_light_indices * sizeof(int32_t);

  Bsp_AllocLump(&bsp_file, BSP_LUMP_VOXELS, bsp_file.voxels_size);
  memset(bsp_file.voxels, 0, bsp_file.voxels_size);

  bsp_file.voxels->size = voxels.size;
  bsp_file.voxels->num_light_indices = (int32_t) voxels.num_light_indices;

  byte *out = (byte *) bsp_file.voxels + sizeof(bsp_voxels_t);
  
  byte *out_caustics = out;
  out += voxels.num_voxels * sizeof(byte) * 2; // RG = caustics + exposure

  color32_t *out_fog = (color32_t *) out;
  out += voxels.num_voxels * sizeof(color32_t);
  
  for (int32_t z = 0; z < voxels.size.z; z++) {

    Progress("Emitting voxels", 100.f * z / voxels.size.z);

    SDL_Surface *fog = CreateVoxelSurface(voxels.size.x, voxels.size.y, sizeof(color32_t), out_fog);
    
    for (int32_t y = 0; y < voxels.size.y; y++) {
      for (int32_t x = 0; x < voxels.size.x; x++) {

        const int32_t index = (z * voxels.size.y + y) * voxels.size.x + x;
        const voxel_t *voxel = &voxels.voxels[index];

        *out_caustics++ = (byte)(Clampf01(voxel->caustics) * 255.f);
        *out_caustics++ = (byte)(Clampf01(voxel->exposure) * 255.f);

        *out_fog++ = Color_Color32(Color4fv(voxel->fog));
      }
    }

    if (debug) {
      WriteVoxelSurface(fog, va("/tmp/%s_lg_fog_%d.png", map_base, z));
    }

    SDL_DestroySurface(fog);
  }

  int32_t *out_light_data = (int32_t *) out;
  out += voxels.num_voxels * sizeof(int32_t) * 2;

  for (int32_t z = 0; z < voxels.size.z; z++) {
    for (int32_t y = 0; y < voxels.size.y; y++) {
      for (int32_t x = 0; x < voxels.size.x; x++) {

        const int32_t index = (z * voxels.size.y + y) * voxels.size.x + x;
        const voxel_t *voxel = &voxels.voxels[index];

        *out_light_data++ = voxel->lights_offset;
        *out_light_data++ = voxel->lights_count;
      }
    }
  }

  int32_t *out_light_indices = (int32_t *) out;
  out += voxels.num_light_indices * sizeof(int32_t);

  for (int32_t z = 0; z < voxels.size.z; z++) {
    for (int32_t y = 0; y < voxels.size.y; y++) {
      for (int32_t x = 0; x < voxels.size.x; x++) {

        const int32_t index = (z * voxels.size.y + y) * voxels.size.x + x;
        const voxel_t *voxel = &voxels.voxels[index];

        GHashTableIter iter;
        light_t *light;

        g_hash_table_iter_init(&iter, voxel->lights);
        while (g_hash_table_iter_next(&iter, (gpointer *) &light, NULL)) {
          *out_light_indices++ = (int32_t) (ptrdiff_t) (light->out - bsp_file.lights);
        }
      }
    }
  }
  
  Com_Debug(DEBUG_ALL, "Emitted voxels=%zd indices=%zd\n", voxels.num_voxels, voxels.num_light_indices);

  Com_Print("\r%-24s [100%%] %d ms\n", "Emitting voxels", (uint32_t) SDL_GetTicks() - start);
}

/**
 * @brief
 */
void FreeVoxels(void) {

  voxel_t *v = voxels.voxels;
  for (size_t i = 0; i < voxels.num_voxels; i++, v++) {
    g_hash_table_destroy(v->lights);
  }

  Mem_FreeTag(MEM_TAG_VOXEL);
}
