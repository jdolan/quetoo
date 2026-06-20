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
#include "voxel.h"

typedef struct {
  int32_t *indices;
  size_t count;
} voxel_light_indices_t;

static void Voxel_CollectLightIndex(const HashTable *table, ident key, ident value, ident data) {
  voxel_light_indices_t *collector = data;
  const light_t *light = key;

  if (light->out) {
    collector->indices[collector->count++] = (int32_t) (ptrdiff_t) (light->out - bsp_file.lights);
  }
}

voxels_t voxels;

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

  SDL_Surface *out = NULL;

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

  if (!out) {
    return -1;
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
 * @brief Allocates voxel cells for the entire grid, computing origins and bounds for each cell.
 */
static void BuildVoxelVoxels(void) {

  voxels.num_voxels = voxels.size.x * voxels.size.y * voxels.size.z;

  if (voxels.num_voxels > MAX_BSP_VOXELS) {
    Com_Error(ERROR_FATAL, "MAX_BSP_VOXELS\n");
  }

  voxels.voxels = Mem_TagMalloc(voxels.num_voxels * sizeof(voxel_t), (mem_tag_t) MEM_TAG_VOXEL);

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

        v->lights = $(alloc(HashTable), init, HashTableHashDirect, HashTableEqualDirect);
      }
    }
  }
}

/**
 * @brief Authors a .map file which can be imported into Radiant to view the voxel projections.
 */
static void DebugVoxels(void) {
#if 0
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
 * @brief Initializes the voxel grid, assigns lights to voxels, and returns the total voxel count.
 * @return The number of voxels in the grid.
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

  for (size_t i = 0; i < lights->count; i++) {

    light_t *light = VectorValue(lights, light_t *, i);
    if (light->target_entity != -1) {
      continue;
    }
    if (!Box3_Intersects(light->bounds, voxel->bounds)) {
      continue;
    }

    for (size_t j = 0; j < lengthof(points); j++) {

      const cm_trace_t to_voxel = Light_Trace(light->origin, points[j], 0, CONTENTS_MASK_SHADOW);
      if (to_voxel.fraction == 1.f || Box3_ContainsPoint(voxel->bounds, to_voxel.end)) {
        $(voxel->lights, set, light, light);
        break;
      }
    }
  }
}

/**
 * @brief Calculates light visible bounds by taking the union of all visible voxels and feathering
 * those bounds to ensure smooth shadowing with no visible voxel boundaries in-game.
 */
void FloodLights(void) {

  voxel_t *v = voxels.voxels;

  for (size_t i = 0; i < lights->count; i++) {
    light_t *l = VectorValue(lights, light_t *, i);

    l->visible_bounds = Box3_Null();

    if (l->target_entity != -1) {
      continue;
    }

    box3_t bounds = Box3_Null();

    for (size_t j = 0; j < voxels.num_voxels; j++) {
      if ($(v[j].lights, get, l) != NULL) {
        bounds = Box3_Union(bounds, v[j].bounds);
      }
    }

    bounds = Box3_Expand(bounds, BSP_VOXEL_SIZE * .5f);

    for (size_t j = 0; j < voxels.num_voxels; j++) {
      if (Box3_Intersects(bounds, v[j].bounds)) {
        $(v[j].lights, set, l, l);
      }
    }

    for (size_t j = 0; j < voxels.num_voxels; j++) {
      if ($(v[j].lights, get, l) != NULL) {
        l->visible_bounds = Box3_Union(l->visible_bounds, v[j].bounds);
      }
    }
  }
}

#define CAUSTICS_RADIUS 256.f

/**
 * @brief Calculates caustics direction and intensity based on visibility to nearby liquid brushes.
 */
void CausticsVoxel(int32_t voxel_num) {

  voxel_t *voxel = &voxels.voxels[voxel_num];
  
  const int32_t contents = Cm_BoxContents(voxel->bounds, 0);

  if (contents & CONTENTS_MASK_LIQUID) {
    voxel->caustics = Vec3_Down();
    return;
  }
  
  vec3_t points[9];
  points[0] = voxel->origin;
  Box3_ToPoints(voxel->bounds, &points[1]);

  voxel->caustics = Vec3_Zero();
  const float weight = 1.f / lengthof(points);

  for (int32_t i = 0; i < bsp_file.num_brushes; i++) {
    const bsp_brush_t *brush = &bsp_file.brushes[i];
    
    if (!(brush->contents & CONTENTS_MASK_LIQUID)) {
      continue;
    }

    const vec3_t brush_center = Box3_Center(brush->bounds);
    
    for (size_t j = 0; j < lengthof(points); j++) {

      vec3_t liquid_point = Box3_ClampPoint(brush->bounds, points[j]);
      const float dist = Vec3_Distance(points[j], liquid_point);
      
      if (dist > CAUSTICS_RADIUS) {
        continue;
      }

      // ClampPoint can land on brush AABB bounds outside the actual liquid brush volume.
      // Nudge toward the brush center and require that the target is truly liquid.
      const vec3_t to_center = Vec3_Subtract(brush_center, liquid_point);
      float to_center_length;
      const vec3_t to_center_dir = Vec3_NormalizeLength(to_center, &to_center_length);
      if (to_center_length > 0.f) {
        liquid_point = Vec3_Fmaf(liquid_point, 1.f, to_center_dir);
      }

      if (!(Light_PointContents(liquid_point, 0) & CONTENTS_MASK_LIQUID)) {
        continue;
      }
      
      const cm_trace_t trace = Light_Trace(points[j], liquid_point, 0, CONTENTS_MASK_SHADOW);
      if (trace.fraction == 1.f) {
        const float strength = Clampf01(1.f - dist / CAUSTICS_RADIUS) * weight;
        const vec3_t dir = Vec3_Normalize(Vec3_Subtract(liquid_point, points[j]));
        voxel->caustics = Vec3_Fmaf(voxel->caustics, strength, dir);
      }
    }
  }

  float intensity;
  voxel->caustics = Vec3_NormalizeLength(voxel->caustics, &intensity);
  voxel->caustics = Vec3_Scale(voxel->caustics, Clampf01(intensity));
}

/**
 * @brief Calculates exposure based on sky visibility.
 */
void ExposureVoxel(int32_t voxel_num) {

  voxel_t *voxel = &voxels.voxels[voxel_num];
  
  // Use dome vectors to sample hemisphere for better coverage
  static const vec3_t dome_vectors[] = DOME_UNIFORM_16X;
  
  float exposure_sum = 0.f;
  
  for (size_t i = 0; i < lengthof(dome_vectors); i++) {
    const vec3_t start = voxel->origin;
    const vec3_t dir = Vec3_Scale(dome_vectors[i], MAX_WORLD_AXIAL);
    const vec3_t end = Vec3_Add(start, dir);
    
    const cm_trace_t trace = Light_Trace(start, end, 0, CONTENTS_MASK_SHADOW);
    
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

#define OCCLUSION_RADIUS 256.f

/**
 * @brief Calculates spatial occlusion based on enclosure. Uses full-sphere
 * rays: low average fraction (walls close) yields high occlusion; open areas
 * yield low occlusion. Rays are distance capped so large rooms and outdoor
 * areas produce meaningfully low values rather than collapsing toward 1.0.
 * Used for both audio reverb and ambient occlusion in the renderer.
 */
void OccludeVoxel(int32_t voxel_num) {

  voxel_t *voxel = &voxels.voxels[voxel_num];

  static const vec3_t sphere_vectors[] = SPHERE_UNIFORM_32X;

  float fraction_sum = 0.f;

  for (size_t i = 0; i < lengthof(sphere_vectors); i++) {
    const vec3_t start = voxel->origin;
    const vec3_t dir = Vec3_Scale(sphere_vectors[i], OCCLUSION_RADIUS);
    const vec3_t end = Vec3_Add(start, dir);

    const cm_trace_t trace = Light_Trace(start, end, 0, CONTENTS_MASK_SOLID | CONTENTS_MASK_LIQUID);
    fraction_sum += trace.fraction;
  }

  voxel->occlusion = 1.f - (fraction_sum / (float) lengthof(sphere_vectors));
}

/**
 * @brief Applies a 3×3×3 box blur to the occlusion, exposure, and caustics fields
 * of all voxels. This smooths the sharp 32-unit grid discontinuities that result
 * from sampling each voxel only at its center.
 */
void SmoothVoxels(void) {

  const size_t n = voxels.num_voxels;

  float *smooth_occ = Mem_TagMalloc(n * sizeof(float), (mem_tag_t) MEM_TAG_VOXEL);
  float *smooth_exp = Mem_TagMalloc(n * sizeof(float), (mem_tag_t) MEM_TAG_VOXEL);
  vec3_t *smooth_caust = Mem_TagMalloc(n * sizeof(vec3_t), (mem_tag_t) MEM_TAG_VOXEL);

  for (int32_t z = 0; z < voxels.size.z; z++) {
    for (int32_t y = 0; y < voxels.size.y; y++) {
      for (int32_t x = 0; x < voxels.size.x; x++) {

        float occ_sum = 0.f, exp_sum = 0.f;
        vec3_t caust_sum = Vec3_Zero();
        int32_t count = 0;

        for (int32_t dz = -1; dz <= 1; dz++) {
          for (int32_t dy = -1; dy <= 1; dy++) {
            for (int32_t dx = -1; dx <= 1; dx++) {
              const int32_t nx = x + dx, ny = y + dy, nz = z + dz;
              if (nx < 0 || nx >= voxels.size.x ||
                  ny < 0 || ny >= voxels.size.y ||
                  nz < 0 || nz >= voxels.size.z) {
                continue;
              }
              const int32_t ni = (nz * voxels.size.y + ny) * voxels.size.x + nx;
              occ_sum += voxels.voxels[ni].occlusion;
              exp_sum += voxels.voxels[ni].exposure;
              caust_sum = Vec3_Add(caust_sum, voxels.voxels[ni].caustics);
              count++;
            }
          }
        }

        const int32_t i = (z * voxels.size.y + y) * voxels.size.x + x;
        smooth_occ[i] = occ_sum / (float) count;
        smooth_exp[i] = exp_sum / (float) count;
        smooth_caust[i] = Vec3_Scale(caust_sum, 1.f / (float) count);
      }
    }
  }

  for (size_t i = 0; i < n; i++) {
    voxels.voxels[i].occlusion = smooth_occ[i];
    voxels.voxels[i].exposure = smooth_exp[i];
    voxels.voxels[i].caustics = smooth_caust[i];
  }

  Mem_Free(smooth_occ);
  Mem_Free(smooth_exp);
  Mem_Free(smooth_caust);
}

static int IntCompare(const void *a, const void *b) {
  return *(const int32_t *) a - *(const int32_t *) b;
}

/**
 * @brief Serializes the voxel grid (caustics direction/strength, exposure, and light indices) into the BSP voxels lump.
 */
void EmitVoxels(void) {

  const uint32_t start = (uint32_t) SDL_GetTicks();

  voxels.num_light_indices = 0;

  voxel_t *v = voxels.voxels;
  int32_t min_lights = INT32_MAX, max_lights = 0;
  size_t total_lights = 0;
  
  for (size_t i = 0; i < voxels.num_voxels; i++, v++) {
    v->lights_offset = (int32_t) voxels.num_light_indices;
    v->lights_count = (int32_t) v->lights->count;

    voxels.num_light_indices += v->lights_count;
    
    total_lights += v->lights_count;
    if (v->lights_count < min_lights) {
      min_lights = v->lights_count;
    }
    if (v->lights_count > max_lights) {
      max_lights = v->lights_count;
    }
  }
  
  Com_Verbose("Voxel light stats: min=%d max=%d avg=%.1f total=%zd\n",
              min_lights, max_lights, (float)total_lights / voxels.num_voxels, total_lights);

  bsp_file.voxels_size = sizeof(bsp_voxels_t);
  bsp_file.voxels_size += voxels.num_voxels * sizeof(byte) * 3; // caustics xyz (RGB)
  bsp_file.voxels_size += voxels.num_voxels * sizeof(int32_t) * 2; // light indices offset and count
  bsp_file.voxels_size += voxels.num_light_indices * sizeof(int32_t);
  bsp_file.voxels_size += voxels.num_voxels * sizeof(byte) * 2; // occlusion + exposure (RG)

  Bsp_AllocLump(&bsp_file, BSP_LUMP_VOXELS, bsp_file.voxels_size);
  memset(bsp_file.voxels, 0, bsp_file.voxels_size);

  bsp_file.voxels->size = voxels.size;
  bsp_file.voxels->num_light_indices = (int32_t) voxels.num_light_indices;
  bsp_file.voxels->bounds = voxels.stu_bounds;

  byte *out = (byte *) bsp_file.voxels + sizeof(bsp_voxels_t);
  
  byte *out_data = out;
  out += voxels.num_voxels * sizeof(byte) * 3; // RGB = caustics xyz
  
  for (int32_t z = 0; z < voxels.size.z; z++) {

    Progress("Emitting voxels", 100.f * z / voxels.size.z);
    
    for (int32_t y = 0; y < voxels.size.y; y++) {
      for (int32_t x = 0; x < voxels.size.x; x++) {

        const int32_t index = (z * voxels.size.y + y) * voxels.size.x + x;
        const voxel_t *voxel = &voxels.voxels[index];

        const vec3_t caustics = Vec3_Clamp(voxel->caustics, Vec3_Negate(Vec3_One()), Vec3_One());
        *out_data++ = (byte)((caustics.x * 0.5f + 0.5f) * 255.f);
        *out_data++ = (byte)((caustics.y * 0.5f + 0.5f) * 255.f);
        *out_data++ = (byte)((caustics.z * 0.5f + 0.5f) * 255.f);
      }
    }
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

        int32_t *indices = out_light_indices;
        voxel_light_indices_t collector = {
          .indices = out_light_indices
        };

        $(voxel->lights, enumerate, Voxel_CollectLightIndex, &collector);
        out_light_indices += collector.count;

        const int32_t count = (int32_t) (out_light_indices - indices);
        if (count > 1) {
          qsort(indices, count, sizeof(int32_t), IntCompare);
        }
      }
    }
  }

  byte *out_occlusion = (byte *) out_light_indices;

  for (int32_t z = 0; z < voxels.size.z; z++) {
    for (int32_t y = 0; y < voxels.size.y; y++) {
      for (int32_t x = 0; x < voxels.size.x; x++) {

        const int32_t index = (z * voxels.size.y + y) * voxels.size.x + x;
        const voxel_t *voxel = &voxels.voxels[index];

        *out_occlusion++ = (byte)(Clampf01(voxel->occlusion) * 255.f);
        *out_occlusion++ = (byte)(Clampf01(voxel->exposure) * 255.f);
      }
    }
  }

  Com_Debug(DEBUG_ALL, "Emitted voxels=%zd indices=%zd\n", voxels.num_voxels, voxels.num_light_indices);

  Com_Print("\r%-24s [100%%] %d ms\n", "Emitting voxels", (uint32_t) SDL_GetTicks() - start);
}

/**
 * @brief Frees all per-voxel light hash tables and releases the voxel memory pool.
 */
void FreeVoxels(void) {

  voxel_t *v = voxels.voxels;
  for (size_t i = 0; i < voxels.num_voxels; i++, v++) {
    release(v->lights);
  }

  Mem_FreeTag((mem_tag_t) MEM_TAG_VOXEL);
}
