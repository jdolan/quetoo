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
void IlluminateVoxel(voxel_t *voxel, light_t *light, float lumens) {

  const vec3_t color = Vec3_Scale(light->color, light->intensity);
  voxel->diffuse.xyz = Vec3_Fmaf(voxel->diffuse.xyz, lumens, color);

  light->bounds = Box3_Union(light->bounds, voxel->bounds);
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
 * @brief Builds the voxel grid aligned to world coordinates at BSP_VOXEL_SIZE intervals.
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
    voxels.size.xyz[i] = (int32_t)((voxels.stu_bounds.maxs.xyz[i] - voxels.stu_bounds.mins.xyz[i]) / BSP_VOXEL_SIZE);
  }
}

/**
 * @brief Projects a voxel sample point to world space for contents sampling.
 * Voxels are aligned to world grid at 0, ±32, ±64, etc.
 */
static int32_t ProjectVoxel(voxel_t *v, float soffs, float toffs, float uoffs) {

  v->origin = Vec3(
     voxels.stu_bounds.mins.x + (v->s + 0.5f + soffs) * BSP_VOXEL_SIZE,
     voxels.stu_bounds.mins.y + (v->t + 0.5f + toffs) * BSP_VOXEL_SIZE,
     voxels.stu_bounds.mins.z + (v->u + 0.5f + uoffs) * BSP_VOXEL_SIZE
  );

  v->bounds = Box3_FromCenterRadius(v->origin, BSP_VOXEL_SIZE * .5f);

  return Light_PointContents(v->origin, 0);
}

/**
 * @brief
 */
static void BuildVoxelVoxels(void) {

  // 4 tetrahedral sample points within each voxel for sub-voxel gradient information
  const vec3_t offsets[] = {
    Vec3(-0.25f, -0.25f, -0.25f),
    Vec3(+0.25f, -0.25f, +0.25f),
    Vec3(-0.25f, +0.25f, +0.25f),
    Vec3(+0.25f, +0.25f, -0.25f),
  };

  voxels.num_voxels = voxels.size.x * voxels.size.y * voxels.size.z;

  if (voxels.num_voxels > MAX_BSP_VOXELS) {
    Com_Error(ERROR_FATAL, "MAX_BSP_VOXELS\n");
  }

  voxels.voxels = Mem_TagMalloc(voxels.num_voxels * sizeof(voxel_t), MEM_TAG_VOXEL);

  voxel_t *v = voxels.voxels;

  // Initialize in XYZ order to match OpenGL texture layout
  for (int32_t u = 0; u < voxels.size.z; u++) {
    for (int32_t t = 0; t < voxels.size.y; t++) {
      for (int32_t s = 0; s < voxels.size.x; s++) {
        
        const int32_t idx = (u * voxels.size.y + t) * voxels.size.x + s;
        v = &voxels.voxels[idx];

        v->s = s;
        v->t = t;
        v->u = u;
        
        v->lights = g_hash_table_new(g_direct_hash, g_direct_equal);

        // Sample 4 points within voxel for gradient information
        for (size_t i = 0; i < lengthof(offsets); i++) {

          const float soffs = offsets[i].x;
          const float toffs = offsets[i].y;
          const float uoffs = offsets[i].z;

          v->contents[i] = ProjectVoxel(v, soffs, toffs, uoffs);
        }
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

  voxel_t *l = lg.voxels;
  for (size_t i = 0; i < lg.num_voxels; i++, l++) {

    ProjectVoxelVoxel(l, 0.0, 0.0, 0.0);

    Fs_Print(file, "{\n");
    Fs_Print(file, "  \"classname\" \"info_voxel\"\n");
    Fs_Print(file, "  \"origin\" \"%g %g %g\"\n", l->origin.x, l->origin.y, l->origin.z);
    Fs_Print(file, "  \"s\" \"%d\"\n", l->s);
    Fs_Print(file, "  \"t\" \"%d\"\n", l->t);
    Fs_Print(file, "  \"u\" \"%d\"\n", l->u);
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
 * @brief Iterates lights, accumulating color and direction on the specified voxel.
 * @param lights The light sources to iterate.
 * @param voxel The voxel to light.
 * @param scale A scalar applied to both color and direction.
 */
static inline void LightVoxel_(const GPtrArray *lights, voxel_t *voxel, float scale) {

  for (guint i = 0; i < lights->len; i++) {

    light_t *light = g_ptr_array_index(lights, i);

    const vec3_t closest_point = Box3_ClampPoint(voxel->bounds, light->origin);
    const float dist_to_voxel = Vec3_Distance(light->origin, closest_point);
    
    if (dist_to_voxel >= light->radius) {
      continue;
    }

    const float dist_to_center = Vec3_Distance(light->origin, voxel->origin);
    const float atten = Clampf01(1.f - dist_to_center / light->radius);
    const float lumens = atten * atten * scale;

    const cm_trace_t trace = Light_Trace(voxel->origin, light->origin, 0, CONTENTS_SOLID);
    if (trace.fraction == 1.f) {
      IlluminateVoxel(voxel, light, lumens);
    }
  }
}

/**
 * @brief
 */
void LightVoxel(int32_t voxel_num) {

  const vec3_t offsets[] = {
    Vec3(-0.5f, -0.5f, -0.5f), Vec3(-0.5f, +0.5f, -0.5f),
    Vec3(+0.5f, -0.5f, -0.5f), Vec3(+0.5f, +0.5f, -0.5f),
    Vec3(-0.5f, -0.5f, +0.5f), Vec3(-0.5f, +0.5f, +0.5f),
    Vec3(+0.5f, -0.5f, +0.5f), Vec3(+0.5f, +0.5f, +0.5f),
    Vec3(+0.0f, +0.0f, +0.0f),
  };

  const float weight = 1.f / lengthof(offsets);

  voxel_t *v = &voxels.voxels[voxel_num];

  for (size_t i = 0; i < lengthof(offsets); i++) {

    const float soffs = offsets[i].x;
    const float toffs = offsets[i].y;
    const float uoffs = offsets[i].z;

    if (ProjectVoxel(v, soffs, toffs, uoffs) == CONTENTS_SOLID) {
      continue;
    }

    LightVoxel_(lights, v, weight);
  }
}

/**
 * @brief
 */
static void FogVoxel_(GArray *fogs, voxel_t *v, float scale) {

  const fog_t *fog = (fog_t *) fogs->data;
  for (guint i = 0; i < fogs->len; i++, fog++) {

    float density = Clampf01(fog->density * scale * FOG_DENSITY_SCALAR);

    switch (fog->type) {
      case FOG_VOLUME:
        if (!PointInsideFog(v->origin, fog)) {
          density = 0.f;
        }
        break;
      default:
        break;
    }

    if (density == 0.f) {
      continue;
    }

    const vec3_t color = Vec3_Multiply(fog->color, Vec3_Scale(v->diffuse.xyz, fog->absorption));

    v->fog = Vec4_Add(v->fog, Vec3_ToVec4(color, density));
  }
}

/**
 * @brief
 */
void FogVoxel(int32_t voxel_num) {

  const vec3_t offsets[] = {
    Vec3(-0.5f, -0.5f, -0.5f), Vec3(-0.5f, +0.5f, -0.5f),
    Vec3(+0.5f, -0.5f, -0.5f), Vec3(+0.5f, +0.5f, -0.5f),
    Vec3(-0.5f, -0.5f, +0.5f), Vec3(-0.5f, +0.5f, +0.5f),
    Vec3(+0.5f, -0.5f, +0.5f), Vec3(+0.5f, +0.5f, +0.5f),
    Vec3(+0.0f, +0.0f, +0.0f),
  };

  const float weight = 1.f / lengthof(offsets);

  voxel_t *l = &voxels.voxels[voxel_num];

  for (size_t i = 0; i < lengthof(offsets); i++) {

    const float soffs = offsets[i].x;
    const float toffs = offsets[i].y;
    const float uoffs = offsets[i].z;

    if (ProjectVoxel(l, soffs, toffs, uoffs) == CONTENTS_SOLID) {
      continue;
    }

    FogVoxel_(fogs, l, weight);
  }
}

/**
 * @brief
 */
void EmitVoxels(void) {

  const uint32_t start = (uint32_t) SDL_GetTicks();

  voxels.num_light_indices = 0;

  voxel_t *v = voxels.voxels;
  for (size_t i = 0; i < voxels.num_voxels; i++, v++) {
    v->lights_offset = (int32_t) voxels.num_light_indices;
    v->lights_count = (int32_t) g_hash_table_size(v->lights);
    voxels.num_light_indices += v->lights_count;
  }

  bsp_file.voxels_size = sizeof(bsp_voxels_t);
  bsp_file.voxels_size += voxels.num_voxels * sizeof(int32_t) * 4; // contents (4 samples per voxel)
  bsp_file.voxels_size += voxels.num_voxels * sizeof(color32_t); // fog
  bsp_file.voxels_size += voxels.num_voxels * sizeof(int32_t) * 2; // light indices offset and count
  bsp_file.voxels_size += voxels.num_light_indices * sizeof(int32_t);

  Bsp_AllocLump(&bsp_file, BSP_LUMP_VOXELS, bsp_file.voxels_size);
  memset(bsp_file.voxels, 0, bsp_file.voxels_size);

  bsp_file.voxels->size = voxels.size;
  bsp_file.voxels->num_light_indices = (int32_t) voxels.num_light_indices;

  byte *out = (byte *) bsp_file.voxels + sizeof(bsp_voxels_t);
  
  int32_t *out_contents = (int32_t *) out;
  out += voxels.num_voxels * sizeof(int32_t) * 4;

  color32_t *out_fog = (color32_t *) out;
  out += voxels.num_voxels * sizeof(color32_t);
  
  for (int32_t u = 0; u < voxels.size.z; u++) {

    Progress("Emitting voxels", 100.f * u / voxels.size.z);

    SDL_Surface *fog = CreateVoxelSurface(voxels.size.x, voxels.size.y, sizeof(color32_t), out_fog);
    
    for (int32_t t = 0; t < voxels.size.y; t++) {
      for (int32_t s = 0; s < voxels.size.x; s++) {
        const int32_t idx = (u * voxels.size.y + t) * voxels.size.x + s;
        voxel_t *voxel = &voxels.voxels[idx];
        
        // Write 4 contents samples as RGBA
        for (int32_t i = 0; i < 4; i++) {
          *out_contents++ = voxel->contents[i];
        }
        *out_fog++ = Color_Color32(Color4fv(voxel->fog));
      }
    }

    if (debug) {
      WriteVoxelSurface(fog, va("/tmp/%s_lg_fog_%d.png", map_base, u));
    }

    SDL_DestroySurface(fog);
  }

  int32_t *meta_out = (int32_t *) out;
  out += voxels.num_voxels * sizeof(int32_t) * 2;

  v = voxels.voxels;
  for (size_t i = 0; i < voxels.num_voxels; i++, v++) {
    *meta_out++ = v->lights_offset;
    *meta_out++ = v->lights_count;
  }

  int32_t *indices_out = (int32_t *) out;
  out += voxels.num_light_indices * sizeof(int32_t);

  v = voxels.voxels;
  for (size_t i = 0; i < voxels.num_voxels; i++, v++) {

    GHashTableIter iter;
    gpointer key;

    g_hash_table_iter_init(&iter, v->lights);
    while (g_hash_table_iter_next(&iter, &key, NULL)) {
      light_t *light = key;

      *indices_out++ = (int32_t) (ptrdiff_t) (light->out - bsp_file.lights);
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
