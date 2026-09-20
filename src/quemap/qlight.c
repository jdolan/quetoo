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

#include "qlight.h"

// we use a subset of the collision detection facilities for lighting
static CmBspModel *bspModels[MAX_BSP_MODELS];

/**
 * @brief Box trace data encapsulation and context management.
 */
typedef struct {

  /**
   * @brief The trace start and end points, as provided by the user.
   */
  Vec3 start, end;

  /**
   * @brief The absolute bounds of the trace, spanning the start and end points.
   */
  Box3 absBounds;

  /**
   * @brief The contents mask to collide with, as provided by the user.
   */
  int32_t contents;

  /**
   * @brief The brush cache, to avoid multiple tests against the same brush.
   */
  int32_t brushCache[128];

  /**
   * @brief The trace result.
   */
  CmTrace trace;

  /**
   * @brief The trace fraction not taking any epsilon nudging into account.
   */
  float unnudgedFraction;
} CmTraceData;

/**
 * @brief Returns true if the given brush number has already been tested in this trace, using a hash cache.
 */
static inline bool Light_BrushAlreadyTested(CmTraceData *data, int32_t brushNum) {
  const int32_t hash = brushNum & (lengthof(data->brushCache) - 1);

  const bool skip = (data->brushCache[hash] == brushNum);

  data->brushCache[hash] = brushNum;

  return skip;
}

/**
 * @brief Clips the bounded box to all brush sides for the given brush.
 */
static inline void Light_TraceToBrush(CmTraceData *data, const CmBspBrush *brush) {

  if (!brush->numBrushSides) {
    return;
  }

  if (!Box3_Intersects(data->absBounds, brush->bounds)) {
    return;
  }

  float enterFraction = -1.f;
  float leaveFraction = 1.f;
  float nudgedEnterFraction = -1.f;

  CmBspPlane plane = { };
  const CmBspBrushSide *side = NULL;

  bool startOutside = false, endOutside = false;

  const CmBspBrushSide *s = brush->brushSides + brush->numBrushSides - 1;
  for (int32_t i = brush->numBrushSides - 1; i >= 0; i--, s--) {

    CmBspPlane *p = s->plane;

    const float dist = p->dist;

    const float d1 = Vec3_Dot(data->start, p->normal) - dist;
    const float d2 = Vec3_Dot(data->end, p->normal) - dist;

    if (d1 > 0.f) {
      startOutside = true;
    }
    if (d2 > 0.f) {
      endOutside = true;
    }

    // if completely in front of plane, the trace does not intersect with the brush
    if (d1 > 0.f && d2 >= d1) {
      return;
    }

    // if completely behind plane, the trace does not intersect with this side
    if (d1 <= 0.f && d2 <= d1) {
      continue;
    }

    // the trace intersects this side
    const float d2d1Dist = (d1 - d2);

    if (d1 > d2) { // enter
      const float f = d1 / d2d1Dist;
      if (f > enterFraction) {
        enterFraction = f;
        plane = *p;
        side = s;
        nudgedEnterFraction = (d1 - TRACE_EPSILON) / d2d1Dist;
      }
    } else { // leave
      const float f = d1 / d2d1Dist;
      if (f < leaveFraction) {
        leaveFraction = f;
      }
    }
  }

  // some sort of collision has occurred

  if (!startOutside) { // original point was inside brush
    data->trace.startSolid = true;
    if (!endOutside) {
      data->trace.allSolid = true;
      data->trace.contents = brush->contents;
      data->trace.fraction = 0.f;
      data->unnudgedFraction = 0.f;
    }
  } else if (enterFraction < leaveFraction) { // pierced brush
    if (enterFraction > -1.f && enterFraction < data->unnudgedFraction && nudgedEnterFraction < data->trace.fraction) {
      data->unnudgedFraction = enterFraction;
      data->trace.fraction = nudgedEnterFraction;
      data->trace.brushSide = side;
      data->trace.plane = plane;
      data->trace.contents = side->contents;
      data->trace.surface = side->surface;
      data->trace.material = side->material;
    }
  }
}

/**
 * @brief Traces through a single BSP leaf, testing all brushes within against the bounding box.
 */
static inline void Light_TraceToLeaf(CmTraceData *data, int32_t leafNum) {

  const CmBspLeaf *leaf = &Cm_Bsp()->leafs[leafNum];

  if (!(leaf->contents & data->contents)) {
    return;
  }

  // trace line against all brushes in the leaf
  for (int32_t i = 0; i < leaf->numLeafBrushes; i++) {
    const int32_t brushNum = Cm_Bsp()->leafBrushes[leaf->firstLeafBrush + i];

    if (Light_BrushAlreadyTested(data, brushNum)) {
      continue; // already checked this brush in another leaf
    }

    const CmBspBrush *b = &Cm_Bsp()->brushes[brushNum];

    if (!(b->contents & data->contents)) {
      continue;
    }

    Light_TraceToBrush(data, b);

    if (data->trace.allSolid) {
      return;
    }
  }
}

/**
 * @brief Recursively traces the bounding box through the BSP tree from p1 to p2.
 */
static inline void Light_TraceToNode(CmTraceData *data, int32_t num, float p1f, float p2f,
                                     const Vec3 p1, const Vec3 p2) {

  next:;
  // find the point distances to the separating plane
  // and the offset for the size of the box
  const CmBspNode *node = Cm_Bsp()->nodes + num;
  const CmBspPlane plane = *node->plane;

  float d1, d2;
  if (AXIAL(&plane)) {
    d1 = p1.xyz[plane.type] - plane.dist;
    d2 = p2.xyz[plane.type] - plane.dist;
  } else {
    d1 = Vec3_Dot(plane.normal, p1) - plane.dist;
    d2 = Vec3_Dot(plane.normal, p2) - plane.dist;
  }

  // see which sides we need to consider
  if (d1 >= 0 && d2 >= 0) {
    num = node->children[0];

    // if < 0, we are in a leaf node
    if (num < 0) {
      Light_TraceToLeaf(data, -1 - num);
      return;
    }

    goto next;
  }
  if (d1 < -0 && d2 < -0) {
    num = node->children[1];

    // if < 0, we are in a leaf node
    if (num < 0) {
      Light_TraceToLeaf(data, -1 - num);
      return;
    }

    goto next;
  }

  int32_t side;
  float frac1, frac2;

  if (d1 < d2) {
    const float idist = 1.f / (d1 - d2);
    side = 1;
    frac2 = d1 * idist;
    frac1 = d1 * idist;
  } else if (d1 > d2) {
    const float idist = 1.f / (d1 - d2);
    side = 0;
    frac2 = d1 * idist;
    frac1 = d1 * idist;
  } else {
    side = 0;
    frac1 = 1.f;
    frac2 = 0.f;
  }

  // move up to the node if we can potentially hit it
  if (p1f < data->unnudgedFraction) {
    frac1 = Clampf01(frac1);

    const float midf1 = p1f + (p2f - p1f) * frac1;

    const Vec3 mid = Vec3_Mix(p1, p2, frac1);

    num = node->children[side];

    // if < 0, we are in a leaf node
    if (num < 0) {
      Light_TraceToLeaf(data, -1 - num);
    } else {
      Light_TraceToNode(data, num, p1f, midf1, p1, mid);
    }
  }

  // go past the node
  frac2 = Clampf01(frac2);

  const float midf2 = p1f + (p2f - p1f) * frac2;

  if (midf2 < data->unnudgedFraction) {
    const Vec3 mid = Vec3_Mix(p1, p2, frac2);

    num = node->children[side ^ 1];

    // if < 0, we are in a leaf node
    if (num < 0) {
      Light_TraceToLeaf(data, -1 - num);
    } else {
      Light_TraceToNode(data, num, midf2, p2f, mid, p2);
    }
  }
}

/**
 * @brief Primary collision detection entry point. This function recurses down
 * the BSP tree from the specified head node, clipping the desired movement to
 * brushes that match the specified contents mask.
 *
 * @param start The starting point.
 * @param end The desired end point.
 * @param bounds The bounding box, in model space.
 * @param headNode The BSP head node to recurse down. For inline BSP models,
 * the head node is the root of the model's subtree. For mesh models, a
 * special reserved box hull and head node are used.
 * @param contents The contents mask to clip to.
 * @param matrix The matrix to adjust tested planes by.
 *
 * @return The trace.
 */
static inline CmTrace Light_Trace_(Vec3 start, Vec3 end, int32_t headNode, int32_t contents) {

  CmTraceData data;

  data.trace = (CmTrace) {
    .fraction = 1.f
  };

  data.start = start;
  data.end = end;
  data.absBounds = Box3_FromPoints((const Vec3 []) { start, end }, 2);
  data.contents = contents;
  data.unnudgedFraction = 1.f + TRACE_EPSILON;

  memset(data.brushCache, 0xff, sizeof(data.brushCache));

  Light_TraceToNode(&data, headNode, 0.f, 1.f, data.start, data.end);

  data.trace.fraction = Maxf(0.f, data.trace.fraction);

  if (data.trace.fraction == 0.f) {
    data.trace.end = data.start;
  } else if (data.trace.fraction == 1.f) {
    data.trace.end = data.end;
  } else {
    data.trace.end = Vec3_Mix(data.start, data.end, data.trace.fraction);
  }

  return data.trace;
}

/**
 * @brief Returns the combined brush contents at point p for the world and the optional inline model head node.
 */
int32_t Light_PointContents(const Vec3 p, int32_t headNode) {

  int32_t contents = Cm_PointContents(p, 0, Mat4_Identity());

  if (headNode) {
    contents |= Cm_PointContents(p, headNode, Mat4_Identity());
  }

  return contents;
}

/**
 * @brief Lighting collision detection.
 * @param start The starting point.
 * @param end The desired end point.
 * @param mask The contents mask to clip to.
 * @return The trace.
 */
CmTrace Light_Trace(const Vec3 start, const Vec3 end, int32_t headNode, int32_t mask) {
  CmTrace trace = Light_Trace_(start, end, 0, mask);
  if (trace.startSolid) {
    trace.fraction = 0.f;
  }

  if (headNode) {
    CmTrace tr = Light_Trace_(start, end, headNode, mask);
    if (tr.startSolid) {
      tr.fraction = 0.f;
    }
    if (tr.fraction < trace.fraction) {
      trace = tr;
    }
  }

  return trace;
}

/**
 * @brief Builds voxels, bakes light, and emits all lightmap, voxel, and entity data into the BSP file.
 */
static void LightWorld(void) {

  // build voxel
  const size_t numVoxel = BuildVoxels();

  // build lights out of entities and brush sides
  BuildLights();

  // calculate direct lighting
  Work("Lighting", LightVoxel, (int32_t) numVoxel);

  // feather lights into neighboring voxels to smooth boundaries
  FloodLights();

  // calculate exposure from sky visibility
  Work("Exposure", ExposureVoxel, (int32_t) numVoxel);

  // calculate caustics from liquid contents
  Work("Caustics", CausticsVoxel, (int32_t) numVoxel);

  // calculate reverb enclosure
  Work("Occlusion", OccludeVoxel, (int32_t) numVoxel);

  // smooth voxel grid to reduce 32-unit grid discontinuities
  SmoothVoxels();

  // resolve which voxels each world block touches, for tight occlusion query geometry
  AssignBlockVoxels();

  // emit light sources to the bsp
  EmitLights();

  // resolve which voxels each light touches, for tight occlusion query geometry
  AssignLightVoxels();

  // emit voxels
  EmitVoxels();

  // free the voxels
  FreeVoxels();

  // free the light sources
  FreeLights();
}

/**
 * @brief `LIGHT` stage entry point: builds and bakes all lights, and writes the updated BSP.
 * @details `BSP_Main()` always runs immediately before this in the same process, so `bspFile`
 * is already fully populated in memory; there is no need to reload it from disk here. The
 * collision model, however, is a distinct representation that must be built from the .bsp file
 * `BSP_Main()` just wrote.
 */
int32_t LIGHT_Main(void) {

  Com_Print("\n------------------------------------------\n");
  Com_Print("\nLighting %s\n\n", bspName);

  const uint32_t start = (uint32_t) SDL_GetTicks();

  if (bspFile.numNodes == 0 || bspFile.numFaces == 0) {
    Com_Error(ERROR_FATAL, "Empty map\n");
  }

  bspModels[0] = Cm_LoadBspModel(bspName, NULL);
  for (int32_t i = 1; i < Cm_NumModels(); i++) {
    bspModels[i] = Cm_Model(va("*%d", i));
  }

  LightWorld();

  WriteBSPFile(va("maps/%s.bsp", mapBase));

  for (int32_t tag = MEM_TAG_QLIGHT; tag < MEM_TAG_QMAT; tag++) {
    Mem_FreeTag(tag);
  }

  const uint32_t end = (uint32_t) SDL_GetTicks();
  Com_Print("\nLit %s in %d ms\n", bspName, (end - start));

  return 0;
}
