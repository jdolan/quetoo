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

#include "quemap.h"

struct node_s;

#define MAX_PATCH_WIDTH   15
#define MAX_PATCH_HEIGHT  15
#define MAX_PATCHES       0x4000
#define PATCH_SUBDIVISIONS 8

/**
 * @brief A Bézier patch control point with position and texture coordinates.
 */
typedef struct {
  vec3_t position;
  vec2_t st;
} patch_control_point_t;

/**
 * @brief A pre-tessellated patch face (one per 3×3 sub-patch).
 */
typedef struct patch_face_s {

  /**
   * @brief The AABB of this tessellated face.
   */
  box3_t bounds;

  /**
   * @brief The tessellated vertexes.
   */
  bsp_vertex_t vertexes[(PATCH_SUBDIVISIONS + 1) * (PATCH_SUBDIVISIONS + 1)];

  /**
   * @brief The count of vertexes.
   */
  int32_t num_vertexes;

  /**
   * @brief The tessellated triangle elements (local 0-based indices).
   */
  int32_t elements[PATCH_SUBDIVISIONS * PATCH_SUBDIVISIONS * 6];

  /**
   * @brief The count of elements.
   */
  int32_t num_elements;

  /**
   * @brief The owning patch.
   */
  struct patch_s *patch;

  /**
   * @brief The emitted BSP face.
   */
  bsp_face_t *out;

  /**
   * @brief Linked list pointer for node assignment.
   */
  struct patch_face_s *next;
} patch_face_t;

/**
 * @brief The map file representation of a Bézier surface patch (patchDef2).
 */
typedef struct patch_s {

  /**
   * @brief The texture name.
   */
  char texture[MAX_QPATH];

  /**
   * @brief The control point grid dimensions.
   */
  int32_t width, height;

  /**
   * @brief The control point grid.
   */
  patch_control_point_t control_points[MAX_PATCH_WIDTH * MAX_PATCH_HEIGHT];

  /**
   * @brief The entity number within the map.
   */
  int32_t entity;

  /**
   * @brief The BSP material number.
   */
  int32_t material;

  /**
   * @brief The CONTENTS_* mask.
   */
  int32_t contents;

  /**
   * @brief The SURF_* mask.
   */
  int32_t surface;

  /**
   * @brief The AABB of all tessellated faces.
   */
  box3_t bounds;

  /**
   * @brief The emitted BSP patch, assigned during EmitPatches.
   */
  bsp_patch_t *out;

  /**
   * @brief The pre-tessellated faces.
   */
  patch_face_t *faces;

  /**
   * @brief The count of pre-tessellated faces.
   */
  int32_t num_faces;
} patch_t;

extern int32_t num_patches;
extern patch_t patches[MAX_PATCHES];

patch_t *ParsePatch(parser_t *parser, int32_t entity);
void EmitPatchCollisionBrushes(patch_t *patch, entity_t *entity);
void TessellatePatches(int32_t entity_num);
void AssignPatchFacesToNodes(struct node_s *head_node, int32_t entity_num);
void FreePatchFaces(int32_t entity_num);
void EmitPatches(const bsp_model_t *mod);
