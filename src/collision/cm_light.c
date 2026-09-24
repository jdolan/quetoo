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
static size_t Cm_AppendMaterialLight(const Vec3 point, const Vec3 normal, const Vec3 offset,
                                     int32_t brushSide, int32_t material, int32_t model, Vector *lights) {

  const Vec3 origin = Vec3_Add(Vec3_Fmaf(point, MATERIAL_LIGHT_OFFSET, normal), offset);

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
static size_t Cm_BrushSideLights(const BspFile *file, int32_t brushSide, int32_t model,
                                 const Vec3 offset, const CmStage *stage, Vector *lights) {

  const BspBrushSide *side = &file->brushSides[brushSide];
  const Vec3 normal = file->planes[side->plane].normal;

  CmWinding *w = Cm_WindingForBrushSide(file, side);
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
      count += Cm_AppendMaterialLight(point, normal, offset, brushSide, side->material, model, lights);
    }
  }

  if (inside == 0) {
    count += Cm_AppendMaterialLight(Cm_WindingCenter(w), normal, offset, brushSide, side->material, model, lights);
  }

  Cm_FreeWinding(w);
  return count;
}

/**
 * @brief Returns the inline model index of the entity, or 0 for an entity merged into the world.
 */
static int32_t Cm_MaterialLightModel(int32_t entity) {

  if (entity <= 0 || entity >= Cm_Bsp()->numEntities) {
    return 0;
  }

  const char *model = Cm_EntityValue(Cm_Bsp()->entities[entity], "model")->nullableString;
  if (model && *model == '*') {
    return (int32_t) strtol(model + 1, NULL, 10);
  }

  return 0;
}

/**
 * @brief Places the lights for every drawn brush side whose material has a `STAGE_LIGHT` stage.
 */
size_t Cm_MaterialLights(const BspFile *file, CmMaterial *const *materials, int32_t material, Vector *lights) {

  size_t count = 0;

  const BspBrush *brush = file->brushes;
  for (int32_t i = 0; i < file->numBrushes; i++, brush++) {

    const int32_t model = Cm_MaterialLightModel(brush->entity);

    Vec3 offset = Vec3_Zero();
    if (model) {
      offset = Cm_EntityValue(Cm_Bsp()->entities[brush->entity], "origin")->vec3;
    }

    for (int32_t j = 0; j < brush->numBrushSides; j++) {

      const int32_t brushSide = brush->firstBrushSide + j;
      const BspBrushSide *side = &file->brushSides[brushSide];

      if (side->surface & (SURF_MASK_NO_DRAW_ELEMENTS | SURF_SKIP | SURF_BEVEL | SURF_NODE)) {
        continue;
      }

      if (side->material < 0 || side->material >= file->numMaterials) {
        continue;
      }

      if (material != -1 && side->material != material) {
        continue;
      }

      const CmStage *stage = Cm_MaterialLightStage(materials[side->material]);
      if (stage == NULL) {
        continue;
      }

      count += Cm_BrushSideLights(file, brushSide, model, offset, stage, lights);
    }
  }

  return count;
}

/**
 * @brief Resolves the default color of a stage light from the brightest pixels of its texture.
 */
Vec3 Cm_MaterialLightColor(const CmMaterial *material, const CmStage *stage) {

  const char *path = *stage->asset.path ? stage->asset.path : material->diffusemap.path;

  SDL_Surface *surface = Img_LoadSurface(path);
  if (surface == NULL) {
    Com_Warn("Failed to load %s for the light color of %s\n", path, material->name);
    return Vec3_Normalize(MakeVec3(1.f, 1.f, 1.f));
  }

  const Vec3 color = Vec3_Normalize(Img_ColorHighPass(surface, .5f).vec3);

  SDL_DestroySurface(surface);
  return color;
}
