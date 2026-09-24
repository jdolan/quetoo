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

#include "cm_local.h"
#include "cm_light.h"

/**
 * @brief Returns true if the point, which lies on the plane of the convex winding, is inside it.
 */
static bool Cm_PointInWinding(const CmWinding *w, const Vec3 normal, const Vec3 p) {

  const Vec3 center = Cm_WindingCenter(w);

  for (int32_t i = 0; i < w->numPoints; i++) {

    const Vec3 a = w->points[i];
    const Vec3 b = w->points[(i + 1) % w->numPoints];

    const Vec3 edge = Vec3_Cross(normal, Vec3_Subtract(b, a));
    const float side = Vec3_Dot(edge, Vec3_Subtract(center, a)) > 0.f ? 1.f : -1.f;

    if (Vec3_Dot(edge, Vec3_Subtract(p, a)) * side < -ON_EPSILON) {
      return false;
    }
  }

  return true;
}

/**
 * @brief Appends the light at the given point on the brush side, unless it is in solid.
 */
static size_t Cm_AppendMaterialLight(const Vec3 point, const Vec3 normal, int32_t brushSide,
                                     int32_t material, int32_t model, Vector *lights) {

  const Vec3 origin = Vec3_Fmaf(point, MATERIAL_LIGHT_OFFSET, normal);

  if (Cm_PointContents(origin, 0, Mat4_Identity()) & CONTENTS_SOLID) {
    return 0;
  }

  $(lights, add, &(CmMaterialLight) {
    .origin = origin,
    .normal = normal,
    .brushSide = brushSide,
    .material = material,
    .model = model,
  });

  return 1;
}

/**
 * @brief Places the lights of one brush side on a grid across its winding.
 */
static size_t Cm_BrushSideLights(const CmBsp *bsp, int32_t brushSide, int32_t model,
                                 const CmStage *stage, Vector *lights) {

  const BspBrushSide *side = &bsp->file->brushSides[brushSide];
  const Vec3 normal = bsp->file->planes[side->plane].normal;

  CmWinding *w = Cm_WindingForBrushSide(bsp->file, side);
  if (w == NULL) {
    return 0;
  }

  Vec3 u = Vec3_Zero();
  for (int32_t i = 0; i < w->numPoints; i++) {
    const Vec3 edge = Vec3_Subtract(w->points[(i + 1) % w->numPoints], w->points[i]);
    if (Vec3_Length(edge) > Vec3_Length(u)) {
      u = edge;
    }
  }

  u = Vec3_Normalize(u);
  const Vec3 v = Vec3_Cross(normal, u);

  float minU = FLT_MAX, maxU = -FLT_MAX, minV = FLT_MAX, maxV = -FLT_MAX;
  for (int32_t i = 0; i < w->numPoints; i++) {
    const float pu = Vec3_Dot(w->points[i], u);
    const float pv = Vec3_Dot(w->points[i], v);
    minU = Minf(minU, pu);
    maxU = Maxf(maxU, pu);
    minV = Minf(minV, pv);
    maxV = Maxf(maxV, pv);
  }

  const float spacing = stage->light.radius;
  const int32_t numU = Maxi(1, (int32_t) ceilf((maxU - minU) / spacing));
  const int32_t numV = Maxi(1, (int32_t) ceilf((maxV - minV) / spacing));

  const float d = Vec3_Dot(w->points[0], normal);

  size_t count = 0, inside = 0;

  for (int32_t j = 0; j < numV; j++) {
    for (int32_t i = 0; i < numU; i++) {

      const float pu = minU + (maxU - minU) * (i + .5f) / numU;
      const float pv = minV + (maxV - minV) * (j + .5f) / numV;

      const Vec3 point = Vec3_Fmaf(Vec3_Fmaf(Vec3_Scale(normal, d), pu, u), pv, v);
      if (!Cm_PointInWinding(w, normal, point)) {
        continue;
      }

      inside++;
      count += Cm_AppendMaterialLight(point, normal, brushSide, side->material, model, lights);
    }
  }

  if (inside == 0) {
    count += Cm_AppendMaterialLight(Cm_WindingCenter(w), normal, brushSide, side->material, model, lights);
  }

  Cm_FreeWinding(w);
  return count;
}

/**
 * @brief Places the lights for every visible brush side whose material has a `STAGE_LIGHT` stage.
 */
size_t Cm_MaterialLights(const CmBsp *bsp, CmMaterial *const *materials, int32_t material, Vector *lights) {

  const BspFile *file = bsp->file;

  int32_t *models = Mem_Malloc(sizeof(int32_t) * Maxi(1, file->numBrushSides));
  for (int32_t i = 0; i < file->numBrushSides; i++) {
    models[i] = -1;
  }

  const BspModel *mod = file->models;
  for (int32_t i = 0; i < file->numModels; i++, mod++) {

    const BspFace *face = file->faces + mod->firstFace;
    for (int32_t j = 0; j < mod->numFaces; j++, face++) {
      if (face->brushSide >= 0 && models[face->brushSide] == -1) {
        models[face->brushSide] = i;
      }
    }
  }

  size_t count = 0;

  const BspBrushSide *side = file->brushSides;
  for (int32_t i = 0; i < file->numBrushSides; i++, side++) {

    if (models[i] == -1) {
      continue;
    }

    if (material != -1 && side->material != material) {
      continue;
    }

    if (side->material < 0 || side->material >= file->numMaterials) {
      continue;
    }

    const CmStage *stage = Cm_MaterialLightStage(materials[side->material]);
    if (stage == NULL) {
      continue;
    }

    count += Cm_BrushSideLights(bsp, i, models[i], stage, lights);
  }

  Mem_Free(models);
  return count;
}
