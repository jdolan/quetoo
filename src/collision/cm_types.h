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

#pragma once

#include "shared/shared.h"

#include "cm_material.h"

/**
 * @brief Plane side epsilon. Because plane side tests scrutinize values around
 * and across zero, `FLT_EPSILON` is appropriate and accurate.
 */
#define SIDE_EPSILON      FLT_EPSILON

/**
 * @brief Colinear points dot product epsilon.
 */
#define COLINEAR_EPSILON    .00001f

/**
 * @brief Point equality epsilon.
 */
#define ON_EPSILON        .1f

/**
 * @brief Vertex equality epsilon.
 */
#define VERTEX_EPSILON      .875f

/**
 * @brief Bounding box epsilon.
 */
#define BOX_EPSILON        1.f

/**
 * @brief Trace collision epsilon.
 */
#define TRACE_EPSILON      .125f

/**
 * @brief Plane side constants used for BSP recursion.
 */
#define  SIDE_FRONT        1
#define  SIDE_BACK        2
#define  SIDE_BOTH        3
#define  SIDE_ON          4

/**
 * @brief Plane type constants for axial plane optimizations.
 */
#define PLANE_X          0
#define PLANE_Y          1
#define PLANE_Z          2
#define PLANE_ANY_X        3
#define PLANE_ANY_Y        4
#define PLANE_ANY_Z        5

/**
 * @brief BSP planes are essential to collision detection as well as rendering.
 * Quake stores planes in front and back facing pairs, but most references to
 * planes in the game prefer the "positive planes," or the ones with a normal
 * vector such that all components are >= 0.
 */
typedef struct {
  vec3_t normal;     ///< Plane normal vector.
  float dist;        ///< Plane distance from origin.
  int32_t type;      ///< Plane type constant for axial optimizations (PLANE_X, PLANE_Y, etc.).
  int32_t sign_bits; ///< Sign bit mask of normal components, used for fast plane side tests.
} cm_bsp_plane_t;

/**
 * @brief Returns true if the specified plane is axially aligned.
 */
#define AXIAL(p) ((p)->type < PLANE_ANY_X)

/**
 * @brief Inline BSP models are segments of the collision model that may move.
 * They are treated as their own sub-trees and recursed separately.
 */
typedef struct {
  struct cm_entity_s *entity; ///< The entity definition of this inline model.
  int32_t head_node;          ///< The index of the head node in the BSP file.
  box3_t bounds;              ///< The model bounds.
} cm_bsp_model_t;

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
    ENTITY_STRING = 0x1,        ///< A value of one or more characters is available.

    ENTITY_INTEGER = 0x2,       ///< An integer value is available.

    ENTITY_FLOAT = 0x4,         ///< A float value is available.

    ENTITY_VEC2 = 0x8,          ///< A two component vector value is available.

    ENTITY_VEC3 = 0x10,         ///< A three component vector value is available.

    ENTITY_COLOR = ENTITY_VEC3, ///< An alias for `ENTITY_VEC3`.

    ENTITY_VEC4 = 0x20,         ///< A four component vector is available.

} cm_entity_parsed_t;

/**
 * @brief Entities are, essentially, linked lists of key-value pairs.
 */
typedef struct cm_entity_s {
  cm_entity_parsed_t parsed;    ///< A bitmask of entity pair parsed types.
  char key[MAX_BSP_ENTITY_KEY]; ///< The entity pair key.

  /**
   * @brief The entity pair value, as a string.
   * @remarks This will always be a null-termianted C string.
   */
  char string[MAX_BSP_ENTITY_VALUE];

  /**
   * @brief The entity pair value, as a nullable string pointer.
   * @remarks This will be `NULL` if no string was present.
   */
  char *nullable_string;
  int32_t integer;              ///< The entity pair value, as an integer.

  /**
   * @brief Floating point values parsed from the entity string.
   */
  union {
    float value;   ///< The entity pair value, as a float.
    vec2_t vec2;   ///< The entity pair value, as a two component vector.
    vec3_t vec3;   ///< The entity pair value, as a three component vector.
    vec4_t vec4;   ///< The entity pair value, as a four component vector.
    color_t color; ///< The entity pair value, as a four component color.
  };

  /**
   * @brief When in editor mode, the brushes belonging to the entity are saved here so that they
   * may be re-serialized to the .map.
   */
  char *brushes;
  struct cm_entity_s *prev;     ///< The previous entity pair in this entity, or `NULL`.
  struct cm_entity_s *next;     ///< The next entity pair in this entity, or `NULL`.
} cm_entity_t;

/**
 * @brief Brush sides are represented as unbounded planes, and the materials covering those planes.
 * @remarks Brush sides do not directly provide vertex information. They are used for collision,
 * not for rendering. During the BSP process, all sides in each brush are clipped against each
 * other to produce their windings (ordered vertices). Visible windings are then onto portals,
 * and portals in turn generate faces (rendered geometry).
 */
typedef struct cm_bsp_brush_side_s {
  cm_bsp_plane_t *plane;          ///< The plane.
  struct cm_material_s *material; ///< The material definition.
  int32_t contents;               ///< The contents mask (CONTENTS_*).
  int32_t surface;                ///< The surface mask (SURF_*).
  int32_t value;                  ///< The surface value (e.g. light radius).
} cm_bsp_brush_side_t;

/**
 * @brief Brushes are convex volumes defined by the clipping planes of their sides.
 */
typedef struct cm_bsp_brush_s {
  /**
   * @brief The entity this brush belongs to.
   * @remarks Brushes may reside within the world model's BSP tree, but may have been
   * defined in a different entity (`func_group`, `misc_fog`, etc).
   */
  cm_entity_t *entity;
  int32_t contents;                 ///< The contents mask (`CONTENTS_*`).
  cm_bsp_brush_side_t *brush_sides; ///< The brush sides.
  int32_t num_brush_sides;          ///< The number of brush sides.
  box3_t bounds;                    ///< The brush bounds.
} cm_bsp_brush_t;

/**
 * @brief Leafs are the terminating nodes of the BSP tree.
 * @details When a node has been partitioned until no brush sides occupy its bounds,
 * it becomes a leaf. The leaf brushes are those brushes which bound the leaf. Leafs
 * with non-solid contents comprise the parts of the world the player may occupy.
 */
typedef struct {
  int32_t contents;         ///< The leaf `CONTENTS_*`.
  int32_t first_leaf_brush; ///< The index of the first leaf-brush reference.
  int32_t num_leaf_brushes; ///< The number of leaf-brush references for this leaf.
} cm_bsp_leaf_t;

/**
 * @brief The BSP node structure.
 */
typedef struct {
  cm_bsp_plane_t *plane; ///< The positive plane that separates this node's children.

  /**
   * @brief The child node indexes, where positive values are nodes, and negative are leafs.
   * @remarks Because 0 can not be negated, the BSP is padded with an empty first leaf.
   */
  int32_t children[2];
} cm_bsp_node_t;

/**
 * @brief The BSP model structure.
 */
typedef struct {
  char name[MAX_QPATH];             ///< The Quake path of the .bsp, e.g. `maps/edge.bsp`.
  struct bsp_file_s *file;          ///< A pointer to the backing file on disk.
  int64_t size;                     ///< File size, for compatibility checking.
  int64_t mod_time;                 ///< File modification time, for compatibility checking.
  int32_t num_planes;               ///< Number of planes.
  cm_bsp_plane_t *planes;           ///< Plane array.
  int32_t num_nodes;                ///< Number of BSP nodes.
  cm_bsp_node_t *nodes;             ///< Node array.
  int32_t num_leafs;                ///< Number of BSP leafs.
  cm_bsp_leaf_t *leafs;             ///< Leaf array.
  int32_t num_brushes;              ///< Number of brushes.
  cm_bsp_brush_t *brushes;          ///< Brush array.
  int32_t num_brush_sides;          ///< Number of brush sides.
  cm_bsp_brush_side_t *brush_sides; ///< Brush side array.
  int32_t num_leaf_brushes;         ///< Number of leaf-brush references.
  int32_t *leaf_brushes;            ///< Leaf-brush reference array.
  int32_t num_models;               ///< Number of inline models.
  cm_bsp_model_t *models;           ///< Inline model array.
  int32_t num_entities;             ///< Number of parsed entities.
  cm_entity_t **entities;           ///< Parsed entity array.
  int32_t num_materials;            ///< Number of materials referenced by brush sides.
  cm_material_t **materials;        ///< Material pointer array.

} cm_bsp_t;

/**
 * @brief Traces are discrete movements through world space, clipped to the
 * BSP planes they intersect. This is the basis for all collision detection
 * within Quake.
 */
typedef struct {
  bool all_solid;                               ///< True if the trace started and ended within the same solid.
  bool start_solid;                             ///< True if the trace started within a solid but exited it.
  float fraction;                               ///< The fraction of the desired distance traveled (0.0 - 1.0).
  vec3_t end;                                   ///< The destination position.
  const struct cm_bsp_brush_s *brush;           ///< The impacted or enclosing brush; prefer derived fields.
  const struct cm_bsp_brush_side_s *brush_side; ///< The impacted brush side; prefer derived fields.
  cm_bsp_plane_t plane;                         ///< The impacted plane, transformed by the matrix provided to Cm_BoxTrace.
  int32_t contents;                             ///< The contents mask of the impacted brush side.
  int32_t surface;                              ///< The surface mask of the impacted brush side.
  const struct cm_material_s *material;         ///< The material of the impacted brush side.
  void *ent;                                    ///< The impacted entity, or `NULL`; set by `Sv_Trace` / `Cl_Trace`, not by Cm_BoxTrace.
} cm_trace_t;
