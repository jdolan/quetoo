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

#include "cm_local.h"

/**
 * @brief Box trace data encapsulation and context management.
 */
typedef struct {

  /**
   * @brief The trace start and end points, as provided by the user.
   */
  Vec3 start, end;

  /**
   * @brief The trace bounds, as provided by the user.
   */
  Box3 bounds;

  /**
   * @brief The head node, as provided by the user.
   */
  int32_t headNode;

  /**
   * @brief The absolute bounds of the trace, spanning the start and end points.
   */
  Box3 absBounds;

  /**
   * @brief absBounds in model space; equals absBounds for non-transformed traces.
   * Pre-computed once to avoid per-brush Mat4_TransformBounds calls.
   */
  Box3 modelAbsBounds;

  /**
   * @brief The trace size, expanded to a symmetrical box to account for rotations.
   */
  Vec3 size;

  /**
   * @brief The "corners" of the trace bounds, used for fast plane sidedness tests.
   */
  Vec3 offsets[8];

  /**
   * @brief The contents mask to collide with, as provided by the user.
   */
  int32_t contents;

  /**
   * @brief The transformation matrix for plane collisions, as provided by the user.
   */
  Mat4 matrix;

  /**
   * @brief The transformation matrix for start/end/bounds, for the node tests.
   */
  Mat4 inverseMatrix;

  /**
   * @brief True if matrix is not the identity.
   */
  bool isTransformed;

  /**
   * @brief The brush cache, to avoid multiple tests against the same brush.
   */
  int32_t brushCache[256];

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
 * @brief Returns true if this brush was already tested in the current trace,
 *   preventing duplicate work when a brush spans multiple leaves.
 */
static inline bool Cm_BrushAlreadyTested(CmTraceData *data, int32_t brushNum) {
  const int32_t hash = brushNum & (lengthof(data->brushCache) - 1);

  const bool skip = (data->brushCache[hash] == brushNum);

  data->brushCache[hash] = brushNum;

  return skip;
}

/**
 * @brief Clips the bounded box to all brush sides for the given brush.
 *
 * This implements swept box vs convex brush collision using the separating axis theorem.
 * For each brush plane:
 *  - Calculate signed distance from trace start/end to the plane (accounting for box size)
 *  - Track the latest "enter" fraction (where we cross from front to back of a plane)
 *  - Track the earliest "leave" fraction (where we cross from back to front)
 *  - If start is in front of any plane and stays there, no collision
 *  - If start is behind all planes: inside the brush (`startSolid`)
 *  - If enter < leave: pierced the brush, record the impact at enter fraction
 * The offsets[] array provides the box corner in the direction of each plane normal,
 * effectively expanding each plane outward by the box's radius in that direction.
 */
static void Cm_TraceToBrush_(CmTraceData *data, const CmBspBrush *brush) {

  if (!brush->numBrushSides) {
    return;
  }

  if (!Box3_Intersects(data->modelAbsBounds, brush->bounds)) {
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

    CmBspPlane p;

    if (data->isTransformed) {
      p = Cm_TransformPlane(data->matrix, *s->plane);
    } else {
      p = *s->plane;
    }

    const float dist = p.dist - Vec3_Dot(data->offsets[p.signBits], p.normal);

    const float d1 = Vec3_Dot(data->start, p.normal) - dist;
    const float d2 = Vec3_Dot(data->end, p.normal) - dist;

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
        plane = p;
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
      data->trace.brush = brush;
      data->trace.contents = brush->contents;
      data->trace.fraction = 0.f;
      data->unnudgedFraction = 0.f;
    }
  } else if (enterFraction < leaveFraction) { // pierced brush
    if (enterFraction > -1.f && enterFraction < data->unnudgedFraction && nudgedEnterFraction < data->trace.fraction) {
      data->unnudgedFraction = enterFraction;
      data->trace.fraction = nudgedEnterFraction;
      data->trace.brush = brush;
      data->trace.brushSide = side;
      data->trace.plane = plane;
      data->trace.contents = side->contents;
      data->trace.surface = side->surface;
      data->trace.material = side->material;
    }
  }
}

/**
 * @brief Tests whether the trace start point is inside the given brush.
 */
static void Cm_TestBoxInBrush(CmTraceData *data, const CmBspBrush *brush) {

  if (!brush->numBrushSides) {
    return;
  }

  if (!Box3_Intersects(data->modelAbsBounds, brush->bounds)) {
    return;
  }

  const CmBspBrushSide *side = brush->brushSides;
  for (int32_t i = 0; i < brush->numBrushSides; i++, side++) {

    CmBspPlane plane;

    if (data->isTransformed) {
      plane = Cm_TransformPlane(data->matrix, *side->plane);
    } else {
      plane = *side->plane;
    }

    const float dist = plane.dist - Vec3_Dot(data->offsets[plane.signBits], plane.normal);

    const float d1 = Vec3_Dot(data->start, plane.normal) - dist;

    // if completely in front of face, no intersection
    if (d1 > 0.0f) {
      return;
    }
  }

  // inside this brush
  data->trace.startSolid = data->trace.allSolid = true;
  data->trace.brush = brush;
  data->trace.fraction = 0.0f;
  data->trace.contents = brush->contents;
}

/**
 * @brief Clips the trace against all brushes within the given leaf.
 */
static void Cm_TraceToLeaf(CmTraceData *data, int32_t leafNum) {

  const CmBspLeaf *leaf = &cmBsp.leafs[leafNum];

  if (!(leaf->contents & data->contents)) {
    return;
  }

  // trace line against all brushes in the leaf
  for (int32_t i = 0; i < leaf->numLeafBrushes; i++) {
    const int32_t brushNum = cmBsp.leafBrushes[leaf->firstLeafBrush + i];

    if (Cm_BrushAlreadyTested(data, brushNum)) {
      continue; // already checked this brush in another leaf
    }

    const CmBspBrush *b = &cmBsp.brushes[brushNum];

    if (!(b->contents & data->contents)) {
      continue;
    }

    Cm_TraceToBrush_(data, b);

    if (data->trace.allSolid) {
      return;
    }
  }
}

/**
 * @brief Tests the trace start position against all brushes within the given leaf.
 */
static void Cm_TestInLeaf(CmTraceData *data, int32_t leafNum) {

  const CmBspLeaf *leaf = &cmBsp.leafs[leafNum];

  if (!(leaf->contents & data->contents)) {
    return;
  }

  // trace line against all brushes in the leaf
  for (int32_t i = 0; i < leaf->numLeafBrushes; i++) {
    const int32_t brushNum = cmBsp.leafBrushes[leaf->firstLeafBrush + i];

    if (Cm_BrushAlreadyTested(data, brushNum)) {
      continue; // already checked this brush in another leaf
    }

    const CmBspBrush *b = &cmBsp.brushes[brushNum];

    if (!(b->contents & data->contents)) {
      continue;
    }

    Cm_TestBoxInBrush(data, b);

    if (data->trace.allSolid) {
      return;
    }
  }
}

/**
 * @brief Recursively descends the BSP tree, testing brushes in leaves that the trace intersects.
 *
 * The BSP tree partitions 3D space with planes. Each node has two children representing the
 * front and back half-spaces. This function:
 *  - Projects the trace line segment onto the node's splitting plane
 *  - Determines which side(s) of the plane the swept box intersects (accounting for box size)
 *  - If entirely on one side, recurses to that child only (tail-call via goto)
 *  - If straddling the plane, splits the trace at the plane and recurses to both children
 *  - Negative child indices indicate leaves, which contain brushes to test
 * The fractions p1f and p2f track how far along the original trace [0,1] each recursive
 * segment represents, allowing early-out when we've already found a closer hit.
 */
static void Cm_TraceToNode(CmTraceData *data, int32_t num, float p1f, float p2f,
                           const Vec3 p1, const Vec3 p2) {

next:;
  // find the point distances to the separating plane
  // and the offset for the size of the box
  const CmBspNode *node = cmBsp.nodes + num;
  const CmBspPlane plane = *node->plane;

  float d1, d2, offset;
  if (AXIAL(&plane)) {
    d1 = p1.xyz[plane.type] - plane.dist;
    d2 = p2.xyz[plane.type] - plane.dist;
    offset = data->size.xyz[plane.type];
  } else {
    d1 = Vec3_Dot(plane.normal, p1) - plane.dist;
    d2 = Vec3_Dot(plane.normal, p2) - plane.dist;
    offset = (fabsf(data->size.x * plane.normal.x) +
          fabsf(data->size.y * plane.normal.y) +
          fabsf(data->size.z * plane.normal.z));
  }

  // see which sides we need to consider
  if (d1 >= offset && d2 >= offset) {
    num = node->children[0];

    // if < 0, we are in a leaf node
    if (num < 0) {
      Cm_TraceToLeaf(data, -1 - num);
      return;
    }

    goto next;
  }
  if (d1 < -offset && d2 < -offset) {
    num = node->children[1];

    // if < 0, we are in a leaf node
    if (num < 0) {
      Cm_TraceToLeaf(data, -1 - num);
      return;
    }

    goto next;
  }

  int32_t side;
  float frac1, frac2;

  if (d1 < d2) {
    const float idist = 1.f / (d1 - d2);
    side = 1;
    frac2 = (d1 + offset) * idist;
    frac1 = (d1 - offset) * idist;
  } else if (d1 > d2) {
    const float idist = 1.f / (d1 - d2);
    side = 0;
    frac2 = (d1 - offset) * idist;
    frac1 = (d1 + offset) * idist;
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
      Cm_TraceToLeaf(data, -1 - num);
    } else {
      Cm_TraceToNode(data, num, p1f, midf1, p1, mid);
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
      Cm_TraceToLeaf(data, -1 - num);
    } else {
      Cm_TraceToNode(data, num, midf2, p2f, mid, p2);
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
static inline CmTrace Cm_BoxTrace_(CmTraceData *data) {

  if (!cmBsp.numNodes) { // map not loaded
    return data->trace;
  }

  Box3_ToPoints(data->bounds, data->offsets);

  memset(data->brushCache, 0xff, sizeof(data->brushCache));

  data->modelAbsBounds = data->isTransformed
    ? Mat4_TransformBounds(data->inverseMatrix, data->absBounds)
    : data->absBounds;

  // check for position test special case
  if (Vec3_Equal(data->start, data->end)) {
    static __thread int32_t leafs[MAX_BSP_LEAFS];

    const size_t numLeafs = Cm_BoxLeafnums(data->modelAbsBounds,
                        leafs,
                        lengthof(leafs),
                        NULL,
                        data->headNode);

    for (size_t i = 0; i < numLeafs; i++) {
      Cm_TestInLeaf(data, leafs[i]);

      if (data->trace.allSolid) {
        break;
      }
    }

    data->trace.end = data->start;
    return data->trace;
  }

  if (data->isTransformed) {
    data->size = Box3_Symetrical(Mat4_TransformBounds(data->inverseMatrix, Box3_Expand(data->bounds, BOX_EPSILON)));
    Cm_TraceToNode(data, data->headNode, 0.f, 1.f, Mat4_Transform(data->inverseMatrix, data->start), Mat4_Transform(data->inverseMatrix, data->end));
  } else {
    data->size = Box3_Symetrical(Box3_Expand(data->bounds, BOX_EPSILON));
    Cm_TraceToNode(data, data->headNode, 0.f, 1.f, data->start, data->end);
  }

  data->trace.fraction = Maxf(0.f, data->trace.fraction);

  if (data->trace.fraction == 0.f) {
    data->trace.end = data->start;
  } else if (data->trace.fraction == 1.f) {
    data->trace.end = data->end;
  } else {
    data->trace.end = Vec3_Mix(data->start, data->end, data->trace.fraction);
  }

  return data->trace;
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
 * @param inverseMatrix The inverse matrix to adjust the inputs by.
 *
 * @return The trace.
 */
CmTrace Cm_TransformedBoxTrace(const Vec3 start, const Vec3 end, const Box3 bounds, int32_t headNode,
                        int32_t contents, const Mat4 matrix, const Mat4 inverseMatrix) {

  return Cm_BoxTrace_(&(CmTraceData) {
    .start = start,
    .end = end,
    .bounds = bounds,
    .headNode = headNode,
    .matrix = matrix,
    .inverseMatrix = inverseMatrix,
    .absBounds = Cm_TraceBounds(start, end, bounds),
    .contents = contents,
    .isTransformed = true,
    .trace = (CmTrace) {
      .fraction = 1.f
    },
    .unnudgedFraction = 1.f + TRACE_EPSILON
  });
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
 *
 * @return The trace.
 */
CmTrace Cm_BoxTrace(const Vec3 start, const Vec3 end, const Box3 bounds, int32_t headNode, int32_t contents) {
  
  return Cm_BoxTrace_(&(CmTraceData) {
    .start = start,
    .end = end,
    .bounds = bounds,
    .headNode = headNode,
    .absBounds = Cm_TraceBounds(start, end, bounds),
    .contents = contents,
    .isTransformed = false,
    .trace = (CmTrace) {
      .fraction = 1.f
    },
    .unnudgedFraction = 1.f + TRACE_EPSILON
  });
}

/**
 * @brief Traces a point ray from `start` to `end` against a single brush.
 * @param start The trace start point.
 * @param end The trace end point.
 * @param brush The brush to test.
 * @return The trace result. `startSolid` is set if the origin is inside the brush; callers
 *   should skip `startSolid` results when selecting entities to avoid selecting brushes
 *   that geometrically contain the view origin.
 */
CmTrace Cm_TraceToBrush(const Vec3 start, const Vec3 end, const CmBspBrush *brush) {

  const Box3 absBounds = Cm_TraceBounds(start, end, Box3_Zero());

  CmTraceData data = {
    .start = start,
    .end = end,
    .bounds = Box3_Zero(),
    .absBounds = absBounds,
    .modelAbsBounds = absBounds,
    .trace = {
      .fraction = 1.f
    },
    .unnudgedFraction = 1.f + TRACE_EPSILON
  };

  Box3_ToPoints(Box3_Zero(), data.offsets);

  Cm_TraceToBrush_(&data, brush);

  data.trace.fraction = Maxf(0.f, data.trace.fraction);

  if (data.trace.fraction == 0.f) {
    data.trace.end = start;
  } else if (data.trace.fraction == 1.f) {
    data.trace.end = end;
  } else {
    data.trace.end = Vec3_Mix(start, end, data.trace.fraction);
  }

  return data.trace;
}

/**
 * @brief Calculates a suitable bounding box for tracing to an entity.
 * @param solid The entity's solid type.
 * @param bounds The entity's bounds, in model space.
 * @return The resulting bounds, in world space.
 * @remarks BSP entities can be rotated, requiring special attention.
 */
Box3 Cm_EntityBounds(const Solid solid, const Mat4 matrix, const Box3 bounds) {

  Box3 result = Mat4_TransformBounds(matrix, bounds);

  // epsilon, so bmodels can catch riders
  if (solid == SOLID_BSP) {
    result = Box3_Expand(result, BOX_EPSILON);
  }

  return result;
}

/**
 * @brief Calculates the bounding box for a trace.
 * @param start The trace start point, in world space.
 * @param end The trace end point, in world space.
 * @param bounds The bounding box, in model space.
 * @return The resulting bounding box, in world space.
 */
Box3 Cm_TraceBounds(const Vec3 start, const Vec3 end, const Box3 bounds) {

  return Box3_Expand(
    Box3_ExpandBox(
      Box3_FromPoints((const Vec3 []) { start, end }, 2),
      bounds
    ), BOX_EPSILON);
}
