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
 * @brief
 */
void IlluminateVoxel(voxel_t *voxel, light_t *light, float lumens) {

  const vec3_t color = Vec3_Scale(light->color, light->intensity);
  voxel->diffuse.xyz = Vec3_Fmaf(voxel->diffuse.xyz, lumens, color);

  light->bounds = Box3_Append(light->bounds, voxel->origin);
}

/**
 * @brief Create an `SDL_Surface` with the given voxel data.
 */
SDL_Surface *CreateVoxelSurface(int32_t w, int32_t h, size_t voxel_size, void *voxels) {

  SDL_Surface *surface = SDL_malloc(sizeof(SDL_Surface));

  surface->flags = SDL_DONTFREE;
  surface->w = w;
  surface->h = h;
  surface->userdata = (void *) voxel_size;
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

  assert(src->userdata == dest->userdata);

  assert(rect);

  assert(src->w == rect->w);
  assert(src->h == rect->h);

  const size_t voxel_size = (size_t) src->userdata;

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

  const size_t voxel_size = (size_t) in->userdata;
  switch (voxel_size) {

    case sizeof(vec3_t): {
      const vec3_t *in_voxel = (vec3_t *) in->pixels;

      out = SDL_CreateRGBSurfaceWithFormat(0, in->w, in->h, 24, SDL_PIXELFORMAT_RGB24);
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

      out = SDL_CreateRGBSurfaceWithFormat(0, in->w, in->h, 24, SDL_PIXELFORMAT_RGB24);
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

      out = SDL_CreateRGBSurfaceWithFormat(0, in->w, in->h, 32, SDL_PIXELFORMAT_RGBA32);
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

  SDL_FreeSurface(out);

  return err;
}


/**
 * @brief
 */
static void BuildVoxelMatrices(void) {

  const bsp_model_t *world = bsp_file.models;

  voxels.matrix = Mat4_FromTranslation(Vec3_Negate(world->visible_bounds.mins));
  voxels.matrix = Mat4_ConcatScale(voxels.matrix, 1.f / BSP_VOXEL_SIZE);
  voxels.inverse_matrix = Mat4_Inverse(voxels.matrix);
}

/**
 * @brief
 */
static void BuildVoxelExtents(void) {

  const bsp_model_t *world = bsp_file.models;

  voxels.stu_bounds = Mat4_TransformBounds(voxels.matrix, world->visible_bounds);

  for (int32_t i = 0; i < 3; i++) {
    voxels.size.xyz[i] = floorf(voxels.stu_bounds.maxs.xyz[i] - voxels.stu_bounds.mins.xyz[i]) + 2;
  }
}

/**
 * @brief
 */
static int32_t ProjectVoxel(voxel_t *l, float soffs, float toffs, float uoffs) {

  const float padding_s = ((voxels.stu_bounds.maxs.x - voxels.stu_bounds.mins.x) - voxels.size.x) * .5f;
  const float padding_t = ((voxels.stu_bounds.maxs.y - voxels.stu_bounds.mins.y) - voxels.size.y) * .5f;
  const float padding_u = ((voxels.stu_bounds.maxs.z - voxels.stu_bounds.mins.z) - voxels.size.z) * .5f;

  const float s = voxels.stu_bounds.mins.x + padding_s + l->s + .5f + soffs;
  const float t = voxels.stu_bounds.mins.y + padding_t + l->t + .5f + toffs;
  const float u = voxels.stu_bounds.mins.z + padding_u + l->u + .5f + uoffs;

  l->origin = Mat4_Transform(voxels.inverse_matrix, Vec3(s, t, u));

  return Light_PointContents(l->origin, 0);
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

  voxel_t *l = voxels.voxels;

  for (int32_t u = 0; u < voxels.size.z; u++) {
    for (int32_t t = 0; t < voxels.size.y; t++) {
      for (int32_t s = 0; s < voxels.size.x; s++, l++) {

        l->s = s;
        l->t = t;
        l->u = u;

        ProjectVoxel(l, 0.f, 0.f, 0.f);
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

  BuildVoxelMatrices();

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

    const float dist = Maxf(0.f, Vec3_Distance(light->origin, voxel->origin));
    if (dist >= light->radius) {
      continue;
    }

    const float atten = Clampf01(1.f - dist / light->radius);
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
    Vec3(+0.00f, +0.00f, +0.00f),
    Vec3(-0.25f, -0.25f, -0.25f), Vec3(-0.25f, +0.25f, -0.25f),
    Vec3(+0.25f, -0.25f, -0.25f), Vec3(+0.25f, +0.25f, -0.25f),
    Vec3(-0.25f, -0.25f, +0.25f), Vec3(-0.25f, +0.25f, +0.25f),
    Vec3(+0.25f, -0.25f, +0.25f), Vec3(+0.25f, +0.25f, +0.25f),
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

    LightVoxel_(lights, l, weight);
  }
}

/**
 * @brief
 */
static void CausticsVoxel_(voxel_t *voxel, float scale) {

  float c = 0;

  const int32_t contents = Light_PointContents(voxel->origin, 0);
  if (contents & CONTENTS_MASK_LIQUID) {
    c = 1.f;
  }
  
  const vec3_t points[] = CUBE_8;
  float sample_fraction = 1.f / lengthof(points);

  for (size_t i = 0; i < lengthof(points); i++) {

    const vec3_t point = Vec3_Fmaf(voxel->origin, 128.f, points[i]);

    const cm_trace_t tr = Light_Trace(voxel->origin, point, 0, CONTENTS_SOLID | CONTENTS_MASK_LIQUID);
    if ((tr.contents & CONTENTS_MASK_LIQUID) && (tr.surface & SURF_MASK_TRANSLUCENT)) {

      float f = sample_fraction * (1.f - tr.fraction);
      c += f;
    }
  }

  voxel->diffuse.w += c * scale;
}

/**
 * @brief
 */
void CausticsVoxel(int32_t voxel_num) {

  const vec3_t offsets[] = {
    Vec3(+0.00f, +0.00f, +0.00f),
    Vec3(-0.25f, -0.25f, -0.25f), Vec3(-0.25f, +0.25f, -0.25f),
    Vec3(+0.25f, -0.25f, -0.25f), Vec3(+0.25f, +0.25f, -0.25f),
    Vec3(-0.25f, -0.25f, +0.25f), Vec3(-0.25f, +0.25f, +0.25f),
    Vec3(+0.25f, -0.25f, +0.25f), Vec3(+0.25f, +0.25f, +0.25f),
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

    CausticsVoxel_(l, weight);
  }
}

/**
 * @brief
 */
static void FogVoxel_(GArray *fogs, voxel_t *l, float scale) {

  const fog_t *fog = (fog_t *) fogs->data;
  for (guint i = 0; i < fogs->len; i++, fog++) {

    float density = Clampf01(fog->density * scale * FOG_DENSITY_SCALAR);

    switch (fog->type) {
      case FOG_VOLUME:
        if (!PointInsideFog(l->origin, fog)) {
          density = 0.f;
        }
        break;
      default:
        break;
    }

    if (density == 0.f) {
      continue;
    }

    const vec3_t color = Vec3_Multiply(fog->color, Vec3_Scale(l->diffuse.xyz, fog->absorption));

    l->fog = Vec4_Add(l->fog, Vec3_ToVec4(color, density));
  }
}

/**
 * @brief
 */
void FogVoxel(int32_t voxel_num) {

  const vec3_t offsets[] = {
    Vec3(+0.00f, +0.00f, +0.00f),
    Vec3(-0.25f, -0.25f, -0.25f), Vec3(-0.25f, +0.25f, -0.25f),
    Vec3(+0.25f, -0.25f, -0.25f), Vec3(+0.25f, +0.25f, -0.25f),
    Vec3(-0.25f, -0.25f, +0.25f), Vec3(-0.25f, +0.25f, +0.25f),
    Vec3(+0.25f, -0.25f, +0.25f), Vec3(+0.25f, +0.25f, +0.25f),
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

  bsp_file.voxels_size = sizeof(bsp_voxels_t);
  bsp_file.voxels_size += voxels.num_voxels * sizeof(color32_t);
  bsp_file.voxels_size += voxels.num_voxels * sizeof(color32_t);

  Bsp_AllocLump(&bsp_file, BSP_LUMP_VOXELS, bsp_file.voxels_size);
  memset(bsp_file.voxels, 0, bsp_file.voxels_size);

  bsp_file.voxels->size = voxels.size;

  byte *out = (byte *) bsp_file.voxels + sizeof(bsp_voxels_t);
  
  color32_t *out_diffuse = (color32_t *) out;
  out += voxels.num_voxels * sizeof(color32_t);

  color32_t *out_fog = (color32_t *) out;
  out += voxels.num_voxels * sizeof(color32_t);
  
  voxel_t *voxel = voxels.voxels;
  for (int32_t u = 0; u < voxels.size.z; u++) {
    
    SDL_Surface *diffuse = CreateVoxelSurface(voxels.size.x, voxels.size.y, sizeof(color32_t), out_diffuse);
    SDL_Surface *fog = CreateVoxelSurface(voxels.size.x, voxels.size.y, sizeof(color32_t), out_fog);
    
    for (int32_t t = 0; t < voxels.size.y; t++) {
      for (int32_t s = 0; s < voxels.size.x; s++, voxel++) {
        *out_diffuse++ = Color_Color32(Color4fv(voxel->diffuse));
        *out_fog++ = Color_Color32(Color4fv(voxel->fog));
      }
    }

    if (debug) {
      WriteVoxelSurface(diffuse, va("/tmp/%s_lg_diffuse_%d.png", map_base, u));
      WriteVoxelSurface(fog, va("/tmp/%s_lg_fog_%d.png", map_base, u));
    }

    SDL_FreeSurface(diffuse);
    SDL_FreeSurface(fog);
  }
}
