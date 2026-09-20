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

#pragma once

#include "shared/shared.h"

#include "cm_material.h"

/**
 * @brief Plane side epsilon. Because plane side tests scrutinize values around
 * and across zero, `FLT_EPSILON` is appropriate and accurate.
 */
#define SIDE_EPSILON     FLT_EPSILON

/**
 * @brief Colinear points dot product epsilon.
 */
#define COLINEAR_EPSILON .00001f

/**
 * @brief Point equality epsilon.
 */
#define ON_EPSILON       .1f

/**
 * @brief Vertex equality epsilon.
 */
#define VERTEX_EPSILON   .875f

/**
 * @brief Bounding box epsilon.
 */
#define BOX_EPSILON      1.f

/**
 * @brief Trace collision epsilon.
 */
#define TRACE_EPSILON    .125f

/**
 * @brief Plane side constants used for BSP recursion.
 */
#define SIDE_FRONT       1
#define SIDE_BACK        2
#define SIDE_BOTH        3
#define SIDE_ON          4

/**
 * @brief Plane type constants for axial plane optimizations.
 */
#define PLANE_X          0
#define PLANE_Y          1
#define PLANE_Z          2
#define PLANE_ANY_X      3
#define PLANE_ANY_Y      4
#define PLANE_ANY_Z      5

/**
 * @brief BSP planes are essential to collision detection as well as rendering.
 * Quake stores planes in front and back facing pairs, but most references to
 * planes in the game prefer the "positive planes," or the ones with a normal
 * vector such that all components are >= 0.
 */
typedef struct {

  /**
   * @brief Plane normal vector.
   */
  Vec3 normal;

  /**
   * @brief Plane distance from origin.
   */
  float dist;

  /**
   * @brief Plane type constant for axial optimizations (`PLANE_X`, `PLANE_Y`, etc.).
   */
  int32_t type;

  /**
   * @brief Sign bit mask of normal components, used for fast plane side tests.
   */
  int32_t signBits;
} CmBspPlane;

/**
 * @brief Returns true if the specified plane is axially aligned.
 */
#define AXIAL(p) ((p)->type < PLANE_ANY_X)

/**
 * @brief Inline BSP models are segments of the collision model that may move.
 * They are treated as their own sub-trees and recursed separately.
 */
typedef struct {

  /**
   * @brief The entity definition of this inline model.
   */
  struct CmEntity *entity;

  /**
   * @brief The index of the head node in the BSP file.
   */
  int32_t headNode;

  /**
   * @brief The model bounds.
   */
  Box3 bounds;
} CmBspModel;

/**
 * @brief The maximum length of an entity pair key, in characters.
 */
#define MAX_BSP_ENTITY_KEY    32

/**
 * @brief The maximum length of an entity pair value, in characters.
 */
#define MAX_BSP_ENTITY_VALUE  128

/**
 * @brief Entity pair parsed types.
 */
typedef enum {
    ENTITY_STRING = 0x1,
    ENTITY_INTEGER = 0x2,
    ENTITY_FLOAT = 0x4,
    ENTITY_VEC2 = 0x8,
    ENTITY_VEC3 = 0x10,
    ENTITY_COLOR = ENTITY_VEC3,
    ENTITY_VEC4 = 0x20,
} CmEntityParsed;

/**
 * @brief Entities are, essentially, linked lists of key-value pairs.
 */
typedef struct CmEntity {

  /**
   * @brief A bitmask of entity pair parsed types.
   */
  CmEntityParsed parsed;

  /**
   * @brief The entity pair key.
   */
  char key[MAX_BSP_ENTITY_KEY];

  /**
   * @brief The entity pair value, as a string.
   * @remarks This will always be a null-termianted C string.
   */
  char string[MAX_BSP_ENTITY_VALUE];

  /**
   * @brief The entity pair value, as a nullable string pointer.
   * @remarks This will be `NULL` if no string was present.
   */
  char *nullableString;

  /**
   * @brief The entity pair value, as an integer.
   */
  int32_t integer;

  /**
   * @brief Floating point values parsed from the entity string.
   */
  union {

    /**
     * @brief The entity pair value, as a float.
     */
    float value;

    /**
     * @brief The entity pair value, as a two component vector.
     */
    Vec2 vec2;

    /**
     * @brief The entity pair value, as a three component vector.
     */
    Vec3 vec3;

    /**
     * @brief The entity pair value, as a four component vector.
     */
    Vec4 vec4;

    /**
     * @brief The entity pair value, as a four component color.
     */
    Color color;
  };

  /**
   * @brief When in editor mode, the brushes belonging to the entity are saved here so that they
   * may be re-serialized to the .map.
   */
  char *brushes;

  /**
   * @brief The previous entity pair in this entity, or `NULL`.
   */
  struct CmEntity *prev;

  /**
   * @brief The next entity pair in this entity, or `NULL`.
   */
  struct CmEntity *next;
} CmEntity;

/**
 * @brief Brush sides are represented as unbounded planes, and the materials covering those planes.
 * @remarks Brush sides do not directly provide vertex information. They are used for collision,
 * not for rendering. During the BSP process, all sides in each brush are clipped against each
 * other to produce their windings (ordered vertices). Visible windings are then onto portals,
 * and portals in turn generate faces (rendered geometry).
 */
typedef struct CmBspBrushSide {

  /**
   * @brief The plane.
   */
  CmBspPlane *plane;

  /**
   * @brief The material definition.
   */
  struct CmMaterial *material;

  /**
   * @brief The contents mask (`CONTENTS_`*).
   */
  int32_t contents;

  /**
   * @brief The surface mask (`SURF_`*).
   */
  int32_t surface;

  /**
   * @brief The surface value (e.g. light radius).
   */
  int32_t value;
} CmBspBrushSide;

/**
 * @brief Brushes are convex volumes defined by the clipping planes of their sides.
 */
typedef struct CmBspBrush {

  /**
   * @brief The entity this brush belongs to.
   * @remarks Brushes may reside within the world model's BSP tree, but may have been
   * defined in a different entity (`func_group`, `misc_dust`, etc).
   */
  CmEntity *entity;

  /**
   * @brief The contents mask (`CONTENTS_*`).
   */
  int32_t contents;

  /**
   * @brief The brush sides.
   */
  CmBspBrushSide *brushSides;

  /**
   * @brief The number of brush sides.
   */
  int32_t numBrushSides;

  /**
   * @brief The brush bounds.
   */
  Box3 bounds;
} CmBspBrush;

/**
 * @brief Leafs are the terminating nodes of the BSP tree.
 * @details When a node has been partitioned until no brush sides occupy its bounds,
 * it becomes a leaf. The leaf brushes are those brushes which bound the leaf. Leafs
 * with non-solid contents comprise the parts of the world the player may occupy.
 */
typedef struct {

  /**
   * @brief The leaf `CONTENTS_*`.
   */
  int32_t contents;

  /**
   * @brief The index of the first leaf-brush reference.
   */
  int32_t firstLeafBrush;

  /**
   * @brief The number of leaf-brush references for this leaf.
   */
  int32_t numLeafBrushes;
} CmBspLeaf;

/**
 * @brief The BSP node structure.
 */
typedef struct {

  /**
   * @brief The positive plane that separates this node's children.
   */
  CmBspPlane *plane;

  /**
   * @brief The child node indexes, where positive values are nodes, and negative are leafs.
   * @remarks Because 0 can not be negated, the BSP is padded with an empty first leaf.
   */
  int32_t children[2];
} CmBspNode;

/**
 * @brief Per-voxel data decoded from the BSP voxel lump.
 */
typedef struct {

  /**
   * @brief World-space center of the voxel cell.
   */
  Vec3 origin;

  /**
   * @brief Caustics direction and strength, encoded as a normalized direction
   * scaled by intensity in [-1, 1] per component.
   */
  Vec3 caustics;

  /**
   * @brief Sky exposure in [0, 1]; 1 means fully open to sky.
   */
  float exposure;

  /**
   * @brief Spatial enclosure in [0, 1]; 1 means fully enclosed. Used for
   * audio reverb and renderer ambient occlusion.
   */
  float occlusion;
} CmVoxel;

/**
 * @brief The BSP model structure.
 */
typedef struct {

  /**
   * @brief The Quake path of the .bsp, e.g. `maps/edge.bsp`.
   */
  char name[MAX_QPATH];

  /**
   * @brief A pointer to the backing file on disk.
   */
  struct BspFile *file;

  /**
   * @brief File size, for compatibility checking.
   */
  int64_t size;

  /**
   * @brief File modification time, for compatibility checking.
   */
  int64_t modTime;

  /**
   * @brief Number of planes.
   */
  int32_t numPlanes;

  /**
   * @brief Plane array.
   */
  CmBspPlane *planes;

  /**
   * @brief Number of BSP nodes.
   */
  int32_t numNodes;

  /**
   * @brief Node array.
   */
  CmBspNode *nodes;

  /**
   * @brief Number of BSP leafs.
   */
  int32_t numLeafs;

  /**
   * @brief Leaf array.
   */
  CmBspLeaf *leafs;

  /**
   * @brief Number of brushes.
   */
  int32_t numBrushes;

  /**
   * @brief Brush array.
   */
  CmBspBrush *brushes;

  /**
   * @brief Number of brush sides.
   */
  int32_t numBrushSides;

  /**
   * @brief Brush side array.
   */
  CmBspBrushSide *brushSides;

  /**
   * @brief Number of leaf-brush references.
   */
  int32_t numLeafBrushes;

  /**
   * @brief Leaf-brush reference array.
   */
  int32_t *leafBrushes;

  /**
   * @brief Number of inline models.
   */
  int32_t numModels;

  /**
   * @brief Inline model array.
   */
  CmBspModel *models;

  /**
   * @brief Number of parsed entities.
   */
  int32_t numEntities;

  /**
   * @brief Parsed entity array.
   */
  CmEntity **entities;

  /**
   * @brief Number of materials referenced by brush sides.
   */
  int32_t numMaterials;

  /**
   * @brief Material pointer array.
   */
  CmMaterial **materials;

  /**
   * @brief Voxel grid dimensions.
   */
  Vec3i voxelSize;

  /**
   * @brief Voxel grid world bounds.
   */
  Box3 voxelBounds;

  /**
   * @brief Number of voxels (voxel_size.x * y * z).
   */
  int32_t numVoxels;

  /**
   * @brief Decoded voxel array, indexed by (z*size.y + y)*size.x + x.
   */
  CmVoxel *voxels;

} CmBsp;

/**
 * @brief Traces are discrete movements through world space, clipped to the
 * BSP planes they intersect. This is the basis for all collision detection
 * within Quake.
 */
typedef struct {

  /**
   * @brief True if the trace started and ended within the same solid.
   */
  bool allSolid;

  /**
   * @brief True if the trace started within a solid but exited it.
   */
  bool startSolid;

  /**
   * @brief The fraction of the desired distance traveled (0.0 - 1.0).
   */
  float fraction;

  /**
   * @brief The destination position.
   */
  Vec3 end;

  /**
   * @brief The impacted or enclosing brush; prefer derived fields.
   */
  const struct CmBspBrush *brush;

  /**
   * @brief The impacted brush side; prefer derived fields.
   */
  const struct CmBspBrushSide *brushSide;

  /**
   * @brief The impacted plane, transformed by the matrix provided to `Cm_BoxTrace`.
   */
  CmBspPlane plane;

  /**
   * @brief The contents mask of the impacted brush side.
   */
  int32_t contents;

  /**
   * @brief The surface mask of the impacted brush side.
   */
  int32_t surface;

  /**
   * @brief The material of the impacted brush side.
   */
  const struct CmMaterial *material;

  /**
   * @brief The impacted entity, or `NULL`; set by `Sv_Trace` / `Cl_Trace`, not by `Cm_BoxTrace`.
   */
  void *ent;
} CmTrace;
