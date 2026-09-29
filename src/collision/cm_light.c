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
 * @brief The distance within which the windings of two coplanar brush sides touch.
 */
#define MATERIAL_LIGHT_TOUCH_EPSILON 1.f

/**
 * @brief Returns true if the point, which lies on the plane of the convex winding, is inside it,
 * or within the epsilon of it.
 */
static bool MaterialLight_PointInWinding(const Winding *w, const Vec3 normal, const Vec3 p, float epsilon) {

  const Vec3 center = Winding_Center(w);

  for (int32_t i = 0; i < w->numPoints; i++) {

    const Vec3 a = w->points[i];
    const Vec3 b = w->points[(i + 1) % w->numPoints];

    const Vec3 edge = Vec3_Normalize(Vec3_Cross(normal, Vec3_Subtract(b, a)));
    const float side = Vec3_Dot(edge, Vec3_Subtract(center, a)) > 0.f ? 1.f : -1.f;

    if (Vec3_Dot(edge, Vec3_Subtract(p, a)) * side < -epsilon) {
      return false;
    }
  }

  return true;
}

/**
 * @brief A drawn brush side whose material emits light, and the cluster that it joins.
 */
typedef struct {
  int32_t brushSide;
  int32_t plane;
  int32_t material;
  int32_t model;
  Vec3 offset;
  Winding *winding;
  Box3 bounds;
  int32_t parent;
} MaterialLightSide;

/**
 * @return The root of the cluster of the side.
 */
static int32_t MaterialLight_ClusterRoot(MaterialLightSide *sides, int32_t i) {

  while (sides[i].parent != i) {
    sides[i].parent = sides[sides[i].parent].parent;
    i = sides[i].parent;
  }

  return i;
}

/**
 * @brief Returns true if the two sides emit together: the same material, plane and model, and
 * windings that touch.
 */
static bool MaterialLight_SidesTouch(const BspFile *file, const MaterialLightSide *a, const MaterialLightSide *b) {

  if (a->material != b->material || a->plane != b->plane || a->model != b->model) {
    return false;
  }

  if (!Box3_Intersects(a->bounds, b->bounds)) {
    return false;
  }

  const Vec3 normal = file->planes[a->plane].normal;

  for (int32_t i = 0; i < a->winding->numPoints; i++) {
    if (MaterialLight_PointInWinding(b->winding, normal, a->winding->points[i], MATERIAL_LIGHT_TOUCH_EPSILON)) {
      return true;
    }
  }

  for (int32_t i = 0; i < b->winding->numPoints; i++) {
    if (MaterialLight_PointInWinding(a->winding, normal, b->winding->points[i], MATERIAL_LIGHT_TOUCH_EPSILON)) {
      return true;
    }
  }

  return false;
}

/**
 * @brief Appends the light at the given point on the brush side, unless it is in solid.
 */
static size_t MaterialLight_Append(const Vec3 point, const Vec3 normal, const MaterialLightSide *side, Vector *lights) {

  const Vec3 origin = Vec3_Add(Vec3_Fmaf(point, MATERIAL_LIGHT_OFFSET, normal), side->offset);

  if (Cm_PointContents(origin, 0, Mat4_Identity()) & CONTENTS_SOLID) {
    return 0;
  }

  $(lights, add, &(MaterialLight) {
    .origin = origin,
    .normal = normal,
    .brushSide = side->brushSide,
    .material = side->material,
    .model = side->model,
  });

  return 1;
}

/**
 * @brief Places the lights of one cluster on a single grid across the windings of its sides.
 * @param sides The sides.
 * @param count The number of sides.
 * @param root The root of the cluster, which is also its first side.
 */
static size_t MaterialLight_EmitCluster(const BspFile *file, MaterialLightSide *sides, int32_t count,
                               int32_t root, const MaterialStage *stage, Vector *lights) {

  const Vec3 normal = file->planes[sides[root].plane].normal;
  const float d = file->planes[sides[root].plane].dist;

  Vec3 u = Vec3_Zero();
  int32_t largest = root;
  float largestArea = -1.f;

  for (int32_t i = root; i < count; i++) {
    if (MaterialLight_ClusterRoot(sides, i) != root) {
      continue;
    }

    const Winding *w = sides[i].winding;
    for (int32_t j = 0; j < w->numPoints; j++) {
      const Vec3 edge = Vec3_Subtract(w->points[(j + 1) % w->numPoints], w->points[j]);
      if (Vec3_Length(edge) > Vec3_Length(u)) {
        u = edge;
      }
    }

    const float area = Winding_Area(w);
    if (area > largestArea) {
      largestArea = area;
      largest = i;
    }
  }

  u = Vec3_Normalize(u);
  const Vec3 v = Vec3_Cross(normal, u);

  float minU = FLT_MAX, maxU = -FLT_MAX, minV = FLT_MAX, maxV = -FLT_MAX;

  for (int32_t i = root; i < count; i++) {
    if (MaterialLight_ClusterRoot(sides, i) != root) {
      continue;
    }

    const Winding *w = sides[i].winding;
    for (int32_t j = 0; j < w->numPoints; j++) {
      const float pu = Vec3_Dot(w->points[j], u);
      const float pv = Vec3_Dot(w->points[j], v);
      minU = Minf(minU, pu);
      maxU = Maxf(maxU, pu);
      minV = Minf(minV, pv);
      maxV = Maxf(maxV, pv);
    }
  }

  const float spacing = stage->light.radius;
  const int32_t numU = Maxi(1, (int32_t) ceilf((maxU - minU) / spacing));
  const int32_t numV = Maxi(1, (int32_t) ceilf((maxV - minV) / spacing));

  size_t emitted = 0, inside = 0;

  for (int32_t j = 0; j < numV; j++) {
    for (int32_t i = 0; i < numU; i++) {

      const float pu = minU + (maxU - minU) * (i + .5f) / numU;
      const float pv = minV + (maxV - minV) * (j + .5f) / numV;

      const Vec3 point = Vec3_Fmaf(Vec3_Fmaf(Vec3_Scale(normal, d), pu, u), pv, v);

      for (int32_t k = root; k < count; k++) {
        if (MaterialLight_ClusterRoot(sides, k) != root) {
          continue;
        }

        if (MaterialLight_PointInWinding(sides[k].winding, normal, point, ON_EPSILON)) {
          inside++;
          emitted += MaterialLight_Append(point, normal, &sides[k], lights);
          break;
        }
      }
    }
  }

  if (inside == 0) {
    emitted += MaterialLight_Append(Winding_Center(sides[largest].winding), normal, &sides[largest], lights);
  }

  return emitted;
}

/**
 * @brief Returns the inline model index of the entity, or 0 for an entity merged into the world.
 */
static int32_t MaterialLight_Model(int32_t entity) {

  if (entity <= 0 || entity >= Cm_Bsp()->numEntities) {
    return 0;
  }

  const char *model = Entity_Value(Cm_Bsp()->entities[entity], "model")->nullableString;
  if (model && *model == '*') {
    return (int32_t) strtol(model + 1, NULL, 10);
  }

  return 0;
}

/**
 * @brief Places the lights for every drawn brush side whose material has a `STAGE_LIGHT` stage.
 * @details Coplanar sides of one material and model that touch form a cluster, which is sampled
 * on one grid, so that a surface split into several brushes places the same lights as one brush.
 * Clusters are placed in the order of their first brush side, so that the output is stable.
 */
size_t Material_Lights(const BspFile *file, Material *const *materials, int32_t material, Vector *lights) {

  Vector *candidates = $(alloc(Vector), initWithSize, sizeof(MaterialLightSide));

  const BspBrush *brush = file->brushes;
  for (int32_t i = 0; i < file->numBrushes; i++, brush++) {

    const int32_t model = MaterialLight_Model(brush->entity);

    Vec3 offset = Vec3_Zero();
    if (model) {
      offset = Entity_Value(Cm_Bsp()->entities[brush->entity], "origin")->vec3;
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

      if (Material_LightStage(materials[side->material]) == NULL) {
        continue;
      }

      Winding *w = Winding_ForBrushSide(file, side);
      if (w == NULL) {
        continue;
      }

      $(candidates, add, &(MaterialLightSide) {
        .brushSide = brushSide,
        .plane = side->plane,
        .material = side->material,
        .model = model,
        .offset = offset,
        .winding = w,
        .bounds = Box3_Expand(Winding_Bounds(w), MATERIAL_LIGHT_TOUCH_EPSILON),
        .parent = (int32_t) candidates->count,
      });
    }
  }

  MaterialLightSide *sides = candidates->elements;
  const int32_t count = (int32_t) candidates->count;

  for (int32_t i = 0; i < count; i++) {
    for (int32_t j = i + 1; j < count; j++) {
      if (MaterialLight_SidesTouch(file, &sides[i], &sides[j])) {
        const int32_t a = MaterialLight_ClusterRoot(sides, i);
        const int32_t b = MaterialLight_ClusterRoot(sides, j);
        if (a != b) {
          sides[Maxi(a, b)].parent = Mini(a, b);
        }
      }
    }
  }

  size_t emitted = 0;

  for (int32_t i = 0; i < count; i++) {
    if (MaterialLight_ClusterRoot(sides, i) == i) {
      const MaterialStage *stage = Material_LightStage(materials[sides[i].material]);
      emitted += MaterialLight_EmitCluster(file, sides, count, i, stage, lights);
    }
  }

  for (int32_t i = 0; i < count; i++) {
    Winding_Free(sides[i].winding);
  }

  release(candidates);
  return emitted;
}

/**
 * @brief Resolves the default color of a stage light from the brightest pixels of its texture.
 */
Vec3 Material_LightColor(const Material *material, const MaterialStage *stage) {

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
