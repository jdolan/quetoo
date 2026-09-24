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

#include "brush.h"
#include "bsp.h"
#include "map.h"
#include "material.h"
#include "patch.h"
#include "qbsp.h"
#include "texture.h"

MapFormat mapFormat;

int32_t numEntities;
Entity entities[MAX_BSP_ENTITIES];

int32_t numBrushes;
Brush brushes[MAX_BSP_BRUSHES];

int32_t numBrushSides;
BrushSide brushSides[MAX_BSP_BRUSH_SIDES];

int32_t numPlanes;
Plane planes[MAX_BSP_PLANES];

#define  PLANE_HASHES (size_t) MAX_WORLD_COORD
static Plane *planeHash[PLANE_HASHES];

Box3 mapBounds;

#define NORMAL_EPSILON 0.0001
#define DIST_EPSILON   0.005

/**
 * @brief Returns true if the two planes are equal within `NORMAL_EPSILON` and `DIST_EPSILON`.
 */
static bool PlaneEqual(const Plane *p, const Vec3 normal, double dist) {

  if (EqualEpsilon(p->dist, dist, DIST_EPSILON) &&
    Vec3_EqualEpsilon(p->normal, normal, NORMAL_EPSILON)) {
    return true;
  }

  return false;
}

/**
 * @brief Inserts a plane into the hash table for fast lookup by distance.
 */
static inline void AddPlaneToHash(Plane *p) {

  const int32_t hash = ((int32_t) fabs(p->dist)) & (PLANE_HASHES - 1);

  p->hashChain = planeHash[hash];
  planeHash[hash] = p;
}

/**
 * @brief Creates and registers a new plane and its mirror in the global planes table.
 */
static int32_t CreatePlane(const Vec3 normal, double dist) {

  // bad plane
  if (Vec3_Length(normal) < 1.f - FLT_EPSILON) {
    Com_Error(ERROR_FATAL, "Malformed plane\n");
  }

  // create a new plane
  if (numPlanes + 2 > MAX_BSP_PLANES) {
    Com_Error(ERROR_FATAL, "MAX_BSP_PLANES\n");
  }

  Plane *a = &planes[numPlanes++];
  a->normal = normal;
  a->dist = dist;
  a->type = Cm_PlaneTypeForNormal(a->normal);

  Plane *b = &planes[numPlanes++];
  b->normal = Vec3_Negate(normal);
  b->dist = -dist;
  b->type = Cm_PlaneTypeForNormal(b->normal);

  // always put axial planes facing positive first
  if (AXIAL(a)) {
    if (Cm_SignBitsForNormal(a->normal)) {
      Plane temp = *a;
      *a = *b;
      *b = temp;

      AddPlaneToHash(a);
      AddPlaneToHash(b);
      return numPlanes - 1;
    }
  }

  AddPlaneToHash(a);
  AddPlaneToHash(b);
  return numPlanes - 2;
}

/**
 * @brief If the specified normal is very close to an axis, align with it.
 */
static Vec3 SnapNormal(const Vec3 normal) {

  Vec3 snapped = normal;

  bool snap = false;
  for (int32_t i = 0; i < 3; i++) {
    if (snapped.xyz[i] != 0.f) {
      if (fabsf(snapped.xyz[i]) < NORMAL_EPSILON) {
        snapped.xyz[i] = 0.f;
        snap = true;
      }
    }
  }

  if (snap) {
    snapped = Vec3_Normalize(snapped);
  }

  return snapped;
}

/**
 * @brief Snaps normals within `NORMAL_EPSILON` to axial, and snaps axial plane
 * distances to integers, rounding towards the origin.
 */
static void SnapPlane(Vec3 *normal, double *dist) {

  *normal = SnapNormal(*normal);

  // snap axial planes to integer distances
  if (Cm_PlaneTypeForNormal(*normal) <= PLANE_Z) {
    const double d = floor(*dist + 0.5);
    if (fabs(*dist - d) < DIST_EPSILON) {
      *dist = d;
    }
  }
}

/**
 * @brief Finds or creates a plane matching the given normal and distance; returns its index.
 */
int32_t FindPlane(const Vec3 normal, double dist) {

  Vec3 snapped = normal;
  SnapPlane(&snapped, &dist);

  const int32_t hash = ((int32_t) fabs(dist)) & (PLANE_HASHES - 1);

  // search the adjacent bins as well
  for (int32_t i = -1; i <= 1; i++) {
    const int32_t h = (hash + i) & (PLANE_HASHES - 1);
    
    const Plane *p = planeHash[h];
    while (p) {
      if (PlaneEqual(p, snapped, dist)) {
        return (int32_t) (ptrdiff_t) (p - planes);
      }
      p = p->hashChain;
    }
  }

  return CreatePlane(snapped, dist);
}

/**
 * @brief Derives a plane index from three coplanar points, or returns -1 if the points are degenerate.
 */
static int32_t PlaneFromPoints(const Vec3d p0, const Vec3d p1, const Vec3d p2) {

  const Vec3d t1 = Vec3d_Subtract(p0, p1);
  const Vec3d t2 = Vec3d_Subtract(p2, p1);

  const Vec3d cross = Vec3d_Cross(t1, t2);
  const double length = Vec3d_Length(cross);

  if (length < 1e-6) {
    return -1;
  }

  const Vec3d normal = Vec3d_Scale(cross, 1.0 / length);
  const double dist = Vec3d_Dot(p0, normal);

  return FindPlane(Vec3d_CastVec3(normal), dist);
}

/**
 * @brief Find the largest contents mask within the brush and force all sides to it.
 */
static int32_t BrushContents(const Brush *b) {

  int32_t contents = 0;

  BrushSide *s = b->brushSides;
  for (int32_t i = 0; i < b->numBrushSides; i++, s++) {
    if (s->contents > contents) {
      contents = s->contents;
    }
  }

  s = b->brushSides;
  for (int32_t i = 0; i < b->numBrushSides; i++, s++) {
    s->contents = contents;
  }

  return contents;
}

/**
 * @brief qsort comparator to sort a brushes sides by plane type. This ensures
 * the first 6 sides of the brush are axial.
 */
static int32_t SortBrushSides(const void *a, const void *b) {

  const BrushSide *aSide = a;
  const BrushSide *bSide = b;

  return planes[aSide->plane].type - planes[bSide->plane].type;
}

/**
 * @brief Adds a bevel side referencing `plane` to the specified brush. The bevel will
 * borrow surface, contents and material from the nearest original brush side.
 * @details The slot is cleared first, since it can hold a side of a brush that `UnparseBrush`
 * removed, such as an origin brush.
 */
static void AddBrushBevel(Brush *b, int32_t plane) {

  if (numBrushSides >= MAX_BSP_BRUSH_SIDES) {
    Com_Error(ERROR_FATAL, "MAX_BSP_BRUSH_SIDES\n");
  }


  float dot = -1.f;
  const BrushSide *side = NULL, *s = b->brushSides;
  for (int32_t i = 0; i < b->numBrushSides; i++, s++) {
    if (s->surface & SURF_BEVEL) {
      continue;
    }

    const float d = Vec3_Dot(planes[plane].normal, planes[s->plane].normal);
    if (d > dot) {
      dot = d;
      side = s;
    }
  }

  assert(side);

  BrushSide *bevel = &b->brushSides[b->numBrushSides++];
  memset(bevel, 0, sizeof(*bevel));

  bevel->plane = plane;
  bevel->contents = side->contents;
  bevel->surface = side->surface | SURF_BEVEL;
  bevel->material = side->material;

  numBrushSides++;
}

/**
 * @brief Adds any additional planes necessary to allow the brush to be expanded
 * against axial bounding boxes. Ensures that the first 6 sides of every brush
 * are axial, which allows some optimizations in collision detection.
 */
void AddBrushBevels(Brush *b) {

  for (int32_t axis = 0; axis < 3; axis++) {
    for (int32_t side = -1; side <= 1; side += 2) {

      Vec3 normal = Vec3_Zero();
      normal.xyz[axis] = side;

      float dist;
      if (side == -1) {
        dist = -b->bounds.mins.xyz[axis];
      } else {
        dist = b->bounds.maxs.xyz[axis];
      }

      const int32_t plane = FindPlane(normal, dist);

      int32_t j;
      for (j = 0; j < b->numBrushSides; j++) {
        if (b->brushSides[j].plane == plane) {
          break;
        }
      }

      if (j == b->numBrushSides) {
        AddBrushBevel(b, plane);
      }
    }
  }

  qsort(b->brushSides, b->numBrushSides, sizeof(BrushSide), SortBrushSides);
}

/**
 * @brief Frees the brush sides allocated to `brush`, leaving an "emtpy" brush in place.
 * This is because, for error reporting, we want to preserve the indexes of brushes.
 * @details The side slots are given back only if they are the last ones allocated. When the
 * windings of an entity are made again for its origin, later brushes of the entity follow them.
 */
static void UnparseBrush(Brush *brush, Parser *parser) {

  BrushSide *side = brush->brushSides;
  for (int32_t i = 0; i < brush->numBrushSides; i++, side++) {
    if (side->winding) {
      Cm_FreeWinding(side->winding);
      side->winding = NULL;
    }
  }

  if (brush->brushSides + brush->numBrushSides == brushSides + numBrushSides) {
    numBrushSides -= brush->numBrushSides;
  }

  brush->numBrushSides = 0;
  brush->bounds = Box3_Null();

  // If parser is provided, skip to the end of the brush in the file
  // This is needed when aborting mid-parse to prevent parser corruption
  if (parser) {
    char token[MAX_TOKEN_CHARS];
    while (true) {
      Parse_Token(parser, PARSE_DEFAULT, token, sizeof(token));
      if (!q_strcmp(token, "}")) {
        break;
      }
    }
  }
}

/**
 * @brief Makes windings for sides and mins / maxs for the brush
 */
void MakeBrushWindings(Brush *brush) {

  assert(brush->numBrushSides);

  brush->bounds = Box3_Null();

  BrushSide *side = brush->brushSides;
  for (int32_t i = 0; i < brush->numBrushSides; i++, side++) {

    if (side->surface & SURF_BEVEL) {
      continue;
    }

    if (side->winding) {
      Cm_FreeWinding(side->winding);
    }

    const Plane *plane = &planes[side->plane];
    side->winding = Cm_WindingForPlane(plane->normal, plane->dist);

    const BrushSide *s = brush->brushSides;
    for (int32_t j = 0; j < brush->numBrushSides; j++, s++) {
      if (side == s) {
        continue;
      }
      if (s->surface & SURF_BEVEL) {
        continue;
      }
      const Plane *p = &planes[s->plane ^ 1];
      Cm_ClipWinding(&side->winding, p->normal, p->dist, SIDE_EPSILON);

      if (side->winding == NULL) {
        break;
      }
    }

    if (side->winding) {
      brush->bounds = Box3_Union(brush->bounds, Cm_WindingBounds(side->winding));
    } else {
      Com_Warn("Entity %d brush %d @ %s: Malformed brush\n", brush->entity, brush->brush, vtos(Box3_Center(brush->bounds)));
      UnparseBrush(brush, NULL);
      return;
    }
  }

  for (int32_t i = 0; i < 3; i++) {
    //IDBUG: all the indexes into the mins and maxs were zero (not using i)
    if (brush->bounds.mins.xyz[i] < MIN_WORLD_COORD || brush->bounds.maxs.xyz[i] > MAX_WORLD_COORD) {
      Com_Warn("Entity %d brush %d: Brush exceeds world bounds\n", brush->entity, brush->brush);
      UnparseBrush(brush, NULL);
      return;
    }
    if (brush->bounds.mins.xyz[i] > MAX_WORLD_COORD || brush->bounds.maxs.xyz[i] < MIN_WORLD_COORD) {
      Com_Warn("Entity %d brush %d: No visible sides on brush\n", brush->entity, brush->brush);
      UnparseBrush(brush, NULL);
      return;
    }
  }
}

/**
 * @brief Applies material-derived surface and contents flags to a brush side.
 */
static void SetMaterialFlags(BrushSide *side) {

  const Material *material = &materials[side->material];
  if (material->cm->contents) {
    if (side->contents == 0) {
      side->contents = material->cm->contents;
    }
  }
  if (material->cm->surface) {
    if (side->surface == 0) {
      side->surface = material->cm->surface;
    }
  }

  if (!q_strcmp(side->texture, "common/caulk")) {
    side->surface |= SURF_NO_DRAW;
  } else if (!q_strcmp(side->texture, "common/clip")) {
    side->contents |= CONTENTS_PLAYER_CLIP;
  } else if (!q_strcmp(side->texture, "common/dust")) {
    side->contents |= CONTENTS_ATMOSPHERIC;
  } else if (!q_strcmp(side->texture, "common/hint")) {
    side->surface |= SURF_HINT;
  } else if (!q_strcmp(side->texture, "common/ladder")) {
    side->contents |= CONTENTS_LADDER | CONTENTS_PLAYER_CLIP;
  } else if (!q_strcmp(side->texture, "common/monsterclip")) {
    side->contents |= CONTENTS_MONSTER_CLIP;
  } else if (!q_strcmp(side->texture, "common/origin")) {
    side->contents |= CONTENTS_ORIGIN;
  } else if (!q_strcmp(side->texture, "common/portal")) {
    side->surface |= SURF_PORTAL;
  } else if (!q_strcmp(side->texture, "common/skip")) {
    side->surface |= SURF_SKIP;
  } else if (!q_strcmp(side->texture, "common/sky")) {
    side->surface |= SURF_SKY;
  } else if (!q_strcmp(side->texture, "common/trigger")) {
    side->surface |= SURF_NO_DRAW;
  } else if (!q_strcmp(side->texture, "common/weather")) {
    side->contents |= CONTENTS_ATMOSPHERIC;
  }

  if (side->contents & CONTENTS_MASK_LIQUID) {
    side->surface |= SURF_LIQUID;
  }
}

/**
 * @brief Parses a single brush or patchDef2 block from the map file and adds it to the entity.
 */
static Brush *ParseBrush(Parser *parser, Entity *entity) {
  char token[MAX_TOKEN_CHARS];

  Parse_Token(parser, PARSE_DEFAULT, token, sizeof(token));

  if (q_strcmp(token, "{")) {
    return NULL;
  }

  // Check if this is a patchDef2 block
  if (Parse_Token(parser, PARSE_DEFAULT | PARSE_PEEK, token, sizeof(token))) {
    if (!q_strcmp(token, "patchDef2")) {
      const int32_t entityNum = (int32_t) (entity - entities);
      Patch *patch = ParsePatch(parser, entityNum);
      if (patch) {
        entity->numPatches++;
        //EmitPatchCollisionBrushes(patch, entity);
      }
      return NULL;
    }
  }

  if (numBrushes == MAX_BSP_BRUSHES) {
    Com_Error(ERROR_FATAL, "MAX_BSP_BRUSHES\n");
  }

  Brush *brush = &brushes[numBrushes];
  memset(brush, 0, sizeof(*brush));

  brush->entity = (int32_t) (entity - entities);
  brush->brush = numBrushes - entity->firstBrush;
  brush->brushSides = &brushSides[numBrushSides];

  numBrushes++;

  while (true) {

    if (!Parse_Token(parser, PARSE_DEFAULT | PARSE_PEEK, token, sizeof(token))) {
      Com_Error(ERROR_FATAL, "EOF without closing brush\n");
    }

    if (!q_strcmp(token, "}")) {
      Parse_SkipToken(parser, PARSE_DEFAULT);
      break;
    }

    if (numBrushSides == MAX_BSP_BRUSH_SIDES) {
      Com_Error(ERROR_FATAL, "MAX_BSP_BRUSH_SIDES\n");
    }

    BrushSide *side = &brushSides[numBrushSides];
    memset(side, 0, sizeof(*side));

    Vec3d points[3];

    // read the three point plane definition
    for (int32_t i = 0; i < 3; i++) {

      Parse_Token(parser, PARSE_DEFAULT, token, sizeof(token));
      if (q_strcmp(token, "(")) {
        Com_Error(ERROR_FATAL, "Invalid brush %d (%s)\n", numBrushes, token);
      }

      Parse_Primitive(parser, PARSE_NO_WRAP, PARSE_DOUBLE, &points[i].x, 1);
      Parse_Primitive(parser, PARSE_NO_WRAP, PARSE_DOUBLE, &points[i].y, 1);
      Parse_Primitive(parser, PARSE_NO_WRAP, PARSE_DOUBLE, &points[i].z, 1);

      Parse_Token(parser, PARSE_DEFAULT, token, sizeof(token));
      if (q_strcmp(token, ")")) {
        Com_Error(ERROR_FATAL, "Invalid brush %d (%s)\n", numBrushes, token);
      }
    }

    // read the texture name
    Parse_Token(parser, PARSE_DEFAULT, token, sizeof(token));

    if (q_strlen(token) > sizeof(side->texture) - 1) {
      Com_Error(ERROR_FATAL, "Texture name \"%s\" is too long.\n", token);
    }

    q_strlcpy(side->texture, token, sizeof(side->texture));

    // detect Valve-220 vs standard Q1/Q3 format by peeking for '['
    Parse_PeekToken(parser, PARSE_NO_WRAP, token, sizeof(token));

    if (!q_strcmp(token, "[")) {
      // Valve-220: [ ux uy uz shift_x ] [ vx vy vz shift_y ] rotation scale_x scale_y
      if (mapFormat == MAP_FORMAT_UNKNOWN) {
        mapFormat = MAP_FORMAT_VALVE;
      } else if (mapFormat != MAP_FORMAT_VALVE) {
        Com_Error(ERROR_FATAL, "Mixed map format: Valve-220 brush side in a non-Valve map (brush %d)\n", numBrushes);
      }
      for (int32_t i = 0; i < 2; i++) {
        Parse_Token(parser, PARSE_NO_WRAP, token, sizeof(token));
        if (q_strcmp(token, "[")) {
          Com_Error(ERROR_FATAL, "Invalid brush %d (%s)\n", numBrushes, token);
        }
        Parse_Primitive(parser, PARSE_NO_WRAP, PARSE_FLOAT, &side->axis[i].x, 1);
        Parse_Primitive(parser, PARSE_NO_WRAP, PARSE_FLOAT, &side->axis[i].y, 1);
        Parse_Primitive(parser, PARSE_NO_WRAP, PARSE_FLOAT, &side->axis[i].z, 1);
        Parse_Primitive(parser, PARSE_NO_WRAP, PARSE_FLOAT, &side->axis[i].w, 1); // shift
        Parse_Token(parser, PARSE_NO_WRAP, token, sizeof(token));
        if (q_strcmp(token, "]")) {
          Com_Error(ERROR_FATAL, "Invalid brush %d (%s)\n", numBrushes, token);
        }
      }
      Parse_Primitive(parser, PARSE_NO_WRAP, PARSE_FLOAT, &side->rotate, 1);
      Parse_Primitive(parser, PARSE_NO_WRAP, PARSE_FLOAT, &side->scale.x, 1);
      Parse_Primitive(parser, PARSE_NO_WRAP, PARSE_FLOAT, &side->scale.y, 1);
    } else {
      // Standard Q1/Q3: shift_x shift_y rotation scale_x scale_y
      if (mapFormat == MAP_FORMAT_UNKNOWN) {
        mapFormat = MAP_FORMAT_Q3;
      } else if (mapFormat == MAP_FORMAT_VALVE) {
        Com_Error(ERROR_FATAL, "Mixed map format: standard brush side in a Valve-220 map (brush %d)\n", numBrushes);
      }
      Parse_Primitive(parser, PARSE_NO_WRAP, PARSE_FLOAT, &side->shift.x, 1);
      Parse_Primitive(parser, PARSE_NO_WRAP, PARSE_FLOAT, &side->shift.y, 1);
      Parse_Primitive(parser, PARSE_NO_WRAP, PARSE_FLOAT, &side->rotate, 1);
      Parse_Primitive(parser, PARSE_NO_WRAP, PARSE_FLOAT, &side->scale.x, 1);
      Parse_Primitive(parser, PARSE_NO_WRAP, PARSE_FLOAT, &side->scale.y, 1);
    }

    if (!Parse_IsEOL(parser)) {
      Parse_Primitive(parser, PARSE_NO_WRAP, PARSE_INT32, &side->contents, 1);
      Parse_Primitive(parser, PARSE_NO_WRAP, PARSE_INT32, &side->surface, 1);
      Parse_Primitive(parser, PARSE_NO_WRAP, PARSE_INT32, &side->value, 1);
    }

    // find the plane number
    side->plane = PlaneFromPoints(points[0], points[1], points[2]);
    if (side->plane == -1) {
      Com_Warn("Entity %d brush %d: Invalid plane\n", brush->entity, brush->brush);
      UnparseBrush(brush, parser);
      return brush;
    }

    // ensure that no other side on the brush references the same plane
    bool duplicate = false;
    const BrushSide *other = brush->brushSides;
    for (int32_t i = 0; i < brush->numBrushSides; i++, other++) {
      if (other->plane == side->plane) {
        Com_Warn("Entity %d brush %d: Duplicate plane within brush, skipping\n", brush->entity, brush->brush);
        duplicate = true;
        break;
      }
      if (other->plane == (side->plane ^ 1)) {
        Com_Warn("Entity %d brush %d: Mirrored plane within brush, skipping\n", brush->entity, brush->brush);
        duplicate = true;
        break;
      }
    }

    // Skip this side if it was a duplicate, but continue parsing the brush
    if (duplicate) {
      continue;
    }

    // resolve the material
    side->material = LoadMaterial(side->texture);

    // resolve the texture vectors
    TextureVectorsForBrushSide(side, Vec3_Zero());
    
    // resolve material-based surface and contents flags
    SetMaterialFlags(side);

    // translucent faces are inherently details beacuse they can not occlude
    if ((side->surface & SURF_MASK_TRANSLUCENT) || (side->contents & CONTENTS_WINDOW)) {
      side->contents |= CONTENTS_TRANSLUCENT | CONTENTS_DETAIL;
    }

    // FIXME: Opaque water must be made detail, or it eats solids (?) and fails to
    // generate the desired faces; it'd be nice to fix this correctly some rainy day
    if (side->contents & CONTENTS_MASK_LIQUID) {
      side->contents |= CONTENTS_DETAIL;
    }

    // clip brushes are not drawn and should not be splitters
    if (side->contents & CONTENTS_MASK_CLIP) {
      side->contents |= CONTENTS_DETAIL;
    }

    // and the same goes for atmospherics like dust
    if (side->contents & CONTENTS_ATMOSPHERIC) {
      side->contents |= CONTENTS_DETAIL;
    }

    // default brushes with no explicit contents to either solid or window
    if (!(side->contents & CONTENTS_MASK_VISIBLE) &&
      !(side->contents & CONTENTS_MASK_FUNCTIONAL) &&
      !(side->contents & CONTENTS_ATMOSPHERIC)) {

      if (side->contents & CONTENTS_TRANSLUCENT) {
        side->contents |= CONTENTS_WINDOW;
        side->contents &= ~CONTENTS_SOLID;
      } else {
        side->contents |= CONTENTS_SOLID;
      }
    }

    // hints and skips have no contents
    if (side->surface & (SURF_HINT | SURF_SKIP)) {
      side->contents = CONTENTS_NONE;
    }

    // and skips should never be splitters
    if (side->surface & SURF_SKIP) {
      side->contents |= CONTENTS_DETAIL;
    }

    brush->numBrushSides++;
    numBrushSides++;
  }

  // get the content for the entire brush
  brush->contents = BrushContents(brush);

  // allow detail brushes to be removed
  if (noDetail && (brush->contents & CONTENTS_DETAIL)) {
    UnparseBrush(brush, NULL);
    return brush;
  }

  // allow liquid brushes to be removed
  if (noLiquid && (brush->contents & CONTENTS_MASK_LIQUID)) {
    UnparseBrush(brush, NULL);
    return brush;
  }

  // create windings for sides and bounds for brush
  MakeBrushWindings(brush);

  if (!brush->numBrushSides) {
    return brush;
  }

  // origin brushes are removed, but they set the rotation origin for the rest of the brushes
  // in the entity. After the entire entity is parsed, the planes and textures will be adjusted for
  // the origin brush
  if (brush->contents & CONTENTS_ORIGIN) {

    if (brush->entity == 0) {
      Com_Warn("Entity %d brush %d: Origin brush in worldspawn\n", brush->entity, brush->brush);
    } else {
      const Vec3 origin = Box3_Center(brush->bounds);
      SetValueForKey(entity, "origin", va("%g %g %g", origin.x, origin.y, origin.z));
    }

    UnparseBrush(brush, NULL);
    return brush;
  }

  // add brush bevels, which are required for collision
  AddBrushBevels(brush);
  return brush;
}

/**
 * @brief Some entities are merged into the world, e.g. `func_group`.
 */
static void MoveBrushesToWorld(Entity *ent) {

  const int32_t newBrushes = ent->numBrushes;
  const int32_t worldBrushes = entities[0].numBrushes;

  Brush *temp = Mem_TagMalloc(newBrushes * sizeof(Brush), (MemTag) MEM_TAG_BRUSH);
  memcpy(temp, brushes + ent->firstBrush, newBrushes * sizeof(Brush));

  // make space to move the brushes (overlapped copy)
  memmove(brushes + worldBrushes + newBrushes,
          brushes + worldBrushes,
          sizeof(Brush) * (numBrushes - worldBrushes - newBrushes));

  // copy the new brushes down
  memcpy(brushes + worldBrushes, temp, sizeof(Brush) * newBrushes);

  // fix up indexes
  entities[0].numBrushes += newBrushes;
  for (int32_t i = 1; i < numEntities; i++) {
    entities[i].firstBrush += newBrushes;
  }
  Mem_Free(temp);

  ent->numBrushes = 0;
  ent->numBrushSides = 0;
}

/**
 * @brief Some entities are merged into the world, e.g. `func_group`.
 */
static void MovePatchesToWorld(Entity *ent) {

  const int32_t newPatches = ent->numPatches;
  const int32_t worldPatches = entities[0].numPatches;

  Patch *temp = Mem_TagMalloc(newPatches * sizeof(Patch), (MemTag) MEM_TAG_PATCH);
  memcpy(temp, patches + ent->firstPatch, newPatches * sizeof(Patch));

  // make space to move the patches (overlapped copy)
  memmove(patches + worldPatches + newPatches,
          patches + worldPatches,
          sizeof(Patch) * (numPatches - worldPatches - newPatches));

  // copy the new patches down
  memcpy(patches + worldPatches, temp, sizeof(Patch) * newPatches);

  // fix up entity references
  for (int32_t i = 0; i < newPatches; i++) {
    patches[worldPatches + i].entity = 0;
  }

  // fix up indexes
  entities[0].numPatches += newPatches;
  for (int32_t i = 1; i < numEntities; i++) {
    entities[i].firstPatch += newPatches;
  }
  Mem_Free(temp);

  ent->numPatches = 0;
}

/**
 * @brief Parses one entity block (key-value pairs and brushes) from the map file.
 */
static Entity *ParseEntity(Parser *parser) {
  char token[MAX_TOKEN_CHARS];

  Entity *entity = NULL;

  if (Parse_IsEOF(parser)) {
    return NULL;
  }

  Parse_Token(parser, PARSE_DEFAULT, token, sizeof(token));

  if (!q_strcmp(token, "{")) {

    if (numEntities == MAX_BSP_ENTITIES) {
      Com_Error(ERROR_FATAL, "MAX_BSP_ENTITIES\n");
    }

    entity = &entities[numEntities];
    numEntities++;

    entity->bounds = Box3_Null();

    entity->firstBrush = numBrushes;
    entity->firstBrushSide = numBrushSides;
    entity->firstPatch = numPatches;

    while (true) {

      if (!Parse_Token(parser, PARSE_DEFAULT | PARSE_PEEK, token, sizeof(token))) {
        Com_Error(ERROR_FATAL, "EOF without closing entity\n");
      }

      if (!q_strcmp(token, "}")) {
        Parse_SkipToken(parser, PARSE_DEFAULT);
        break;
      }

      if (!q_strcmp(token, "{")) {
        Brush *brush = ParseBrush(parser, entity);
        if (brush) {
          entity->numBrushes++;
          entity->numBrushSides += brush->numBrushSides;
          entity->bounds = Box3_Union(entity->bounds, brush->bounds);
        }

      } else {
        EntityKeyValue *e = Mem_TagMalloc(sizeof(*e), (MemTag) MEM_TAG_EPAIR);

        if (!Parse_Token(parser, PARSE_DEFAULT, e->key, sizeof(e->key))) {
          Com_Error(ERROR_FATAL, "Invalid entity key in entity %d\n", numEntities);
        }

        Parse_Token(parser, PARSE_DEFAULT | PARSE_ALLOW_OVERRUN, e->value, sizeof(e->value));
        
        e->next = entity->values;
        entity->values = e;
      }
    }

    const Vec3 origin = VectorForKey(entity, "origin", Vec3_Zero());

    // if there was an origin brush, offset all of the planes and texture
    if (!Vec3_Equal(origin, Vec3_Zero())) {

      Brush *brush = brushes + entity->firstBrush;
      for (int32_t i = 0; i < entity->numBrushes; i++, brush++) {

        if (!brush->numBrushSides) {
          continue;
        }

        BrushSide *side = brush->brushSides;
        for (int32_t j = 0; j < brush->numBrushSides; j++, side++) {

          const Plane *plane = &planes[side->plane];
          const double dist = plane->dist - Vec3_Dot(plane->normal, origin);

          side->plane = FindPlane(plane->normal, dist);
          if (!(side->surface & SURF_BEVEL)) {
            TextureVectorsForBrushSide(side, origin);
          }
        }

        MakeBrushWindings(brush);
      }
    }

    // jdolan: Some entities have their brushes merged into the world so that they are
    // CSG subtracted and included in the world's BSP tree. However, these brushes will
    // maintain a reference to their entity definition, so that any entity pairs
    // associated with them will still be available. Their brushes will point to their
    // defining CmEntity.
    const char *classname = ValueForKey(entity, "classname", NULL);
    if (!q_strcmp(classname, "func_group") ||
      !q_strcmp(classname, "misc_dust") ||
      !q_strcmp(classname, "misc_sprite") ||
      !q_strcmp(classname, "misc_weather")) {
      MoveBrushesToWorld(entity);
      MovePatchesToWorld(entity);
    }
  }

  return entity;
}

/**
 * @brief Loads and parses the .map file, populating the global entities, brushes, planes, and patches arrays.
 * @return The resolved map format, also stored in the global `mapFormat`.
 */
MapFormat LoadMapFile(const char *filename) {

  Com_Verbose("--- LoadMapFile ---\n");

  mapFormat = MAP_FORMAT_UNKNOWN;

  memset(entities, 0, sizeof(entities));
  numEntities = 0;

  memset(brushes, 0, sizeof(brushes));
  numBrushes = 0;

  memset(brushSides, 0, sizeof(brushSides));
  numBrushSides = 0;

  memset(planes, 0, sizeof(planes));
  numPlanes = 0;

  memset(patches, 0, sizeof(patches));
  numPatches = 0;

  memset(planeHash, 0, sizeof(planeHash));

  void *buffer;
  if (Fs_Load(filename, &buffer) == -1) {
    Com_Error(ERROR_FATAL, "Failed to load %s\n", filename);
  }

  Parser parser = Parse_Init(buffer, PARSER_DEFAULT);

  for (int32_t i = 0, models = 0; i < MAX_BSP_ENTITIES; i++) {

    Entity *entity = ParseEntity(&parser);
    if (!entity) {
      break;
    }

    if (entity->numBrushSides) {
      SetValueForKey(entity, "model", va("*%d", models++));
    }
  }

  mapBounds = entities[0].bounds;

  Com_Verbose("%5i brushes\n", numBrushes);
  Com_Verbose("%5i brush sides\n", numBrushSides);
  Com_Verbose("%5i patches\n", numPatches);
  Com_Verbose("%5i entities\n", numEntities);
  Com_Verbose("%5i planes\n", numPlanes);
  Com_Verbose("size: %5.0f,%5.0f,%5.0f to %5.0f,%5.0f,%5.0f\n",
        mapBounds.mins.x, mapBounds.mins.y, mapBounds.mins.z,
        mapBounds.maxs.x, mapBounds.maxs.y, mapBounds.maxs.z);

  Fs_Free(buffer);

  return mapFormat;
}
