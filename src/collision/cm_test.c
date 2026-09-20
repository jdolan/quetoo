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

/**
 * @return The `PLANE_` type for the given normal vector.
 */
int32_t Cm_PlaneTypeForNormal(const Vec3 normal) {

  const float x = fabsf(normal.x);
  if (x > 1.f - FLT_EPSILON) {
    return PLANE_X;
  }

  const float y = fabsf(normal.y);
  if (y > 1.f - FLT_EPSILON) {
    return PLANE_Y;
  }

  const float z = fabsf(normal.z);
  if (z > 1.f - FLT_EPSILON) {
    return PLANE_Z;
  }

  if (x >= y && x >= z) {
    return PLANE_ANY_X;
  }
  if (y >= x && y >= z) {
    return PLANE_ANY_Y;
  }

  return PLANE_ANY_Z;
}

/**
 * @return A bit mask hinting at the sign of each normal vector component. This
 * can be used to optimize plane side tests.
 */
int32_t Cm_SignBitsForNormal(const Vec3 normal) {
  int32_t bits = 0;

  for (int32_t i = 0; i < 3; i++) {
    if (normal.xyz[i] < 0.0f) {
      bits |= 1 << i;
    }
  }

  return bits;
}

/**
 * @return A constructed plane struct.
 */
CmBspPlane Cm_Plane(const Vec3 normal, float dist) {

  return (CmBspPlane) {
    .normal = normal,
    .dist = dist,
    .type = Cm_PlaneTypeForNormal(normal),
    .signBits = Cm_SignBitsForNormal(normal)
  };
}

/**
 * @return The plane transformed by the input matrix.
 */
CmBspPlane Cm_TransformPlane(const Mat4 matrix, const CmBspPlane plane) {
  const Vec4 out = Mat4_TransformPlane(matrix, plane.normal, plane.dist);
  return Cm_Plane(out.xyz, out.w);
}

/**
 * @return The `point` projected onto `plane`.
 */
Vec3 Cm_ProjectPointToPlane(const Vec3 point, const CmBspPlane *plane) {
  const float dist = Cm_DistanceToPlane(point, plane);
  return Vec3_Subtract(point, Vec3_Scale(plane->normal, dist));
}

/**
 * @return `true` if `point` resides inside `brush`, `false` otherwise.
 */
bool Cm_PointInsideBrush(const Vec3 point, const CmBspBrush *brush) {

  if (Box3_ContainsPoint(brush->bounds, point)) {

    const CmBspBrushSide *side = brush->brushSides;
    for (int32_t i = 0; i < brush->numBrushSides; i++, side++) {
      if (Cm_DistanceToPlane(point, side->plane) > 0.f) {
        return false;
      }
    }

    return true;
  }

  return false;
}

/**
 * @return The sidedness of the given bounds relative to the specified plane.
 * If the box straddles the plane, `SIDE_BOTH` is returned.
 */
int32_t Cm_BoxOnPlaneSide(const Box3 bounds, const CmBspPlane *p) {

  if (AXIAL(p)) {
    if (bounds.mins.xyz[p->type] - p->dist >= 0.f) {
      return SIDE_FRONT;
    }
    if (bounds.maxs.xyz[p->type] - p->dist <  0.f) {
      return SIDE_BACK;
    }
    return SIDE_BOTH;
  }

  float dist1, dist2;
  switch (p->signBits) {
    case 0:
      dist1 = Vec3_Dot(p->normal, bounds.maxs);
      dist2 = Vec3_Dot(p->normal, bounds.mins);
      break;
    case 1:
      dist1 = p->normal.x * bounds.mins.x + p->normal.y * bounds.maxs.y + p->normal.z * bounds.maxs.z;
      dist2 = p->normal.x * bounds.maxs.x + p->normal.y * bounds.mins.y + p->normal.z * bounds.mins.z;
      break;
    case 2:
      dist1 = p->normal.x * bounds.maxs.x + p->normal.y * bounds.mins.y + p->normal.z * bounds.maxs.z;
      dist2 = p->normal.x * bounds.mins.x + p->normal.y * bounds.maxs.y + p->normal.z * bounds.mins.z;
      break;
    case 3:
      dist1 = p->normal.x * bounds.mins.x + p->normal.y * bounds.mins.y + p->normal.z * bounds.maxs.z;
      dist2 = p->normal.x * bounds.maxs.x + p->normal.y * bounds.maxs.y + p->normal.z * bounds.mins.z;
      break;
    case 4:
      dist1 = p->normal.x * bounds.maxs.x + p->normal.y * bounds.maxs.y + p->normal.z * bounds.mins.z;
      dist2 = p->normal.x * bounds.mins.x + p->normal.y * bounds.mins.y + p->normal.z * bounds.maxs.z;
      break;
    case 5:
      dist1 = p->normal.x * bounds.mins.x + p->normal.y * bounds.maxs.y + p->normal.z * bounds.mins.z;
      dist2 = p->normal.x * bounds.maxs.x + p->normal.y * bounds.mins.y + p->normal.z * bounds.maxs.z;
      break;
    case 6:
      dist1 = p->normal.x * bounds.maxs.x + p->normal.y * bounds.mins.y + p->normal.z * bounds.mins.z;
      dist2 = p->normal.x * bounds.mins.x + p->normal.y * bounds.maxs.y + p->normal.z * bounds.maxs.z;
      break;
    case 7:
      dist1 = Vec3_Dot(p->normal, bounds.mins);
      dist2 = Vec3_Dot(p->normal, bounds.maxs);
      break;
    default:
      dist1 = dist2 = 0.0; // shut up compiler
      break;
  }

  int32_t side = 0;

  if (dist1 - p->dist >= 0.f) {
    side = SIDE_FRONT;
  }
  if (dist2 - p->dist <  0.f) {
    side |= SIDE_BACK;
  }

  return side;
}

/**
 * @brief Bounding box to BSP tree structure for box positional testing.
 */
typedef struct {

  /**
   * @brief Head node of the appended box hull subtree.
   */
  int32_t headNode;

  /**
   * @brief The 12 planes defining the box hull faces.
   */
  CmBspPlane *planes;

  /**
   * @brief The single brush representing the box.
   */
  CmBspBrush *brush;

  /**
   * @brief The single leaf enclosing the box.
   */
  CmBspLeaf *leaf;
} CmBox;

static CmBox cmBox;

/**
 * @brief Appends a brush (6 nodes, 12 planes) opaquely to the primary BSP
 * structure to represent the bounding box used for `Cm_BoxLeafnums`. This brush
 * is never tested by the rest of the collision detection code, as it resides
 * just beyond the parsed size of the map.
 */
void Cm_InitBoxHull(CmBsp *bsp) {

  if (bsp->numPlanes + 12 > MAX_BSP_PLANES) {
    Com_Error(ERROR_DROP, "MAX_BSP_PLANES\n");
  }

  if (bsp->numNodes + 6 > MAX_BSP_NODES) {
    Com_Error(ERROR_DROP, "MAX_BSP_NODES\n");
  }

  if (bsp->numLeafs + 1 > MAX_BSP_LEAFS) {
    Com_Error(ERROR_DROP, "MAX_BSP_LEAFS\n");
  }

  if (bsp->numLeafBrushes + 1 > MAX_BSP_LEAF_BRUSHES) {
    Com_Error(ERROR_DROP, "MAX_BSP_LEAF_BRUSHES\n");
  }

  if (bsp->numBrushes + 1 > MAX_BSP_BRUSHES) {
    Com_Error(ERROR_DROP, "MAX_BSP_BRUSHES\n");
  }

  if (bsp->numBrushSides + 6 > MAX_BSP_BRUSH_SIDES) {
    Com_Error(ERROR_DROP, "MAX_BSP_BRUSH_SIDES\n");
  }

  // head node
  cmBox.headNode = bsp->numNodes;

  // planes
  cmBox.planes = &bsp->planes[bsp->numPlanes];

  // leaf
  cmBox.leaf = &bsp->leafs[bsp->numLeafs];
  cmBox.leaf->contents = CONTENTS_MONSTER;
  cmBox.leaf->firstLeafBrush = bsp->numLeafBrushes;
  cmBox.leaf->numLeafBrushes = 1;

  // leaf brush
  bsp->leafBrushes[bsp->numLeafBrushes] = bsp->numBrushes;

  // brush
  cmBox.brush = &bsp->brushes[bsp->numBrushes];
  cmBox.brush->numBrushSides = 6;
  cmBox.brush->brushSides = bsp->brushSides + bsp->numBrushSides;
  cmBox.brush->contents = CONTENTS_MONSTER;

  for (int32_t i = 0; i < 6; i++) {

    // fill in planes, two per side
    CmBspPlane *plane = &cmBox.planes[i * 2];
    plane->normal = Vec3_Zero();
    plane->normal.xyz[i >> 1] = 1.f;
    plane->signBits = Cm_SignBitsForNormal(plane->normal);
    plane->type = Cm_PlaneTypeForNormal(plane->normal);

    plane = &cmBox.planes[i * 2 + 1];
    plane->normal = Vec3_Zero();
    plane->normal.xyz[i >> 1] = -1.f;
    plane->signBits = Cm_SignBitsForNormal(plane->normal);
    plane->type = Cm_PlaneTypeForNormal(plane->normal);

    const int32_t s = i & 1;

    // fill in nodes, one per side
    CmBspNode *node = &bsp->nodes[cmBox.headNode + i];
    node->plane = bsp->planes + (bsp->numPlanes + i * 2);
    node->children[s] = -1 - bsp->numLeafs;
    if (i != 5) {
      node->children[s ^ 1] = cmBox.headNode + i + 1;
    } else {
      node->children[s ^ 1] = -1 - bsp->numLeafs;
    }

    // fill in brush sides, one per side
    CmBspBrushSide *side = &bsp->brushSides[bsp->numBrushSides + i];
    side->plane = bsp->planes + (bsp->numPlanes + i * 2 + s);
  }
}

/**
 * @brief Initializes the box hull for the specified bounds, returning the
 * head node for the resulting box hull tree.
 */
int32_t Cm_SetBoxHull(const Box3 bounds, const int32_t contents) {

  cmBox.brush->bounds = bounds;

  cmBox.planes[0].dist = bounds.maxs.x;
  cmBox.planes[1].dist = -bounds.maxs.x;
  cmBox.planes[2].dist = bounds.mins.x;
  cmBox.planes[3].dist = -bounds.mins.x;
  cmBox.planes[4].dist = bounds.maxs.y;
  cmBox.planes[5].dist = -bounds.maxs.y;
  cmBox.planes[6].dist = bounds.mins.y;
  cmBox.planes[7].dist = -bounds.mins.y;
  cmBox.planes[8].dist = bounds.maxs.z;
  cmBox.planes[9].dist = -bounds.maxs.z;
  cmBox.planes[10].dist = bounds.mins.z;
  cmBox.planes[11].dist = -bounds.mins.z;

  cmBox.leaf->contents = cmBox.brush->contents = contents;

  return cmBox.headNode;
}

/**
 * @return The leaf number containing the specified point.
 */
int32_t Cm_PointLeafnum(const Vec3 p, int32_t headNode) {

  if (!cmBsp.numNodes) {
    return 0;
  }

  int32_t num = headNode;
  while (num >= 0) {
    const CmBspNode *node = cmBsp.nodes + num;
    const float dist = Cm_DistanceToPlane(p, node->plane);
    if (dist < 0.f) {
      num = node->children[1];
    } else {
      num = node->children[0];
    }
  }

  return -1 - num;
}

/**
 * @brief Point contents check.
 *
 * @param p The point to check.
 * @param headNode The BSP head node to recurse down.
 * @param inverseMatrix The inverse matrix of the entity to be tested.
 *
 * @return The contents mask at the specified point.
 *
 * @remarks The input point is transformed because node planes can't be transformed.
 */
int32_t Cm_PointContents(const Vec3 p, int32_t headNode, const Mat4 inverseMatrix) {

  if (!cmBsp.numNodes) {
    return 0;
  }

  Vec3 p0 = p;

  if (!Mat4_Equal(inverseMatrix, Mat4_Identity())) {
    p0 = Mat4_Transform(inverseMatrix, p);
  }

  const int32_t leafNum = Cm_PointLeafnum(p0, headNode);

  return cmBsp.leafs[leafNum].contents;
}

/**
 * @brief Data binding structure for box to leaf tests.
 */
typedef struct {

  /**
   * @brief The AABB being tested.
   */
  Box3 bounds;

  /**
   * @brief Output list of leaf numbers.
   */
  int32_t *list;

  /**
   * @brief Number of leafs found and maximum list capacity.
   */
  size_t count, length;

  /**
   * @brief Index of the topmost node that fully contains the box.
   */
  int32_t topNode;

  /**
   * @brief Accumulated contents from all touched leafs.
   */
  int32_t contents;
} cm_box_leafnum_data;

/**
 * @brief Recurse the BSP tree from the specified node, accumulating leafs the
 * given box occupies in the data structure.
 */
static void Cm_BoxLeafnums_r(cm_box_leafnum_data *data, int32_t nodeNum) {

  while (true) {
    if (nodeNum < 0) {
      const int32_t leafNum = -1 - nodeNum;
      data->contents |= cmBsp.leafs[leafNum].contents;

      if (data->count < data->length) {
        data->list[data->count++] = leafNum;
      }

      return;
    }

    const CmBspNode *node = &cmBsp.nodes[nodeNum];
    const CmBspPlane plane = *node->plane;
    const int32_t side = Cm_BoxOnPlaneSide(data->bounds, &plane);

    if (side == SIDE_FRONT) {
      nodeNum = node->children[0];
    } else if (side == SIDE_BACK) {
      nodeNum = node->children[1];
    } else { // go down both
      if (data->topNode == -1) {
        data->topNode = nodeNum;
      }
      Cm_BoxLeafnums_r(data, node->children[0]);
      nodeNum = node->children[1];
    }
  }
}

/**
 * @brief Populates the list of leafs the specified bounding box touches. If
 * `topNode` is not `NULL`, it will contain the top node of the BSP tree that
 * fully contains the box.
 *
 * @param bounds The bounds in world space.
 * @param list The list of leaf numbers to populate.
 * @param length The maximum number of leafs to return.
 * @param topNode If not null, this will contain the top node for the box.
 * @param headNode The head node to recurse from.
 * @param matrix The matrix by which to transform planes.
 *
 * @return The number of leafs accumulated to the list.
 */
size_t Cm_BoxLeafnums(const Box3 bounds, int32_t *list, size_t length, int32_t *topNode,
            int32_t headNode) {

  cm_box_leafnum_data data = {
    .bounds = bounds,
    .list = list,
    .length = length,
    .topNode = -1
  };

  if (cmBsp.numNodes) {
    Cm_BoxLeafnums_r(&data, headNode);
  }

  if (topNode) {
    *topNode = data.topNode;
  }

  if (data.length > 0 && data.count == data.length) {
    Com_Warn("%zd leafs exceeded\n", data.length);
  }

  return data.count;
}

/**
 * @brief Contents check for a bounded box.
 * @param bounds The bounding box to check for contents.
 * @param headNode The BSP head node to recurse down.
 * @param matrix The matrix to transform the bounds by.
 * @return The contents mask of all leafs within the transformed bounds.
 */
int32_t Cm_BoxContents(const Box3 bounds, int32_t headNode) {
  cm_box_leafnum_data data = {
    .bounds = bounds,
    .list = NULL,
    .length = 0,
    .topNode = -1
  };

  Cm_BoxLeafnums_r(&data, headNode);

  return data.contents;
}
