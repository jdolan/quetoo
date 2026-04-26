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
  box3_t bounds; ///< The AABB of this tessellated face.
  bsp_vertex_t vertexes[(PATCH_SUBDIVISIONS + 1) * (PATCH_SUBDIVISIONS + 1)]; ///< The tessellated vertexes.
  int32_t num_vertexes; ///< The count of vertexes.
  int32_t elements[PATCH_SUBDIVISIONS * PATCH_SUBDIVISIONS * 6]; ///< The tessellated triangle elements (local 0-based indices).
  int32_t num_elements; ///< The count of elements.
  struct patch_s *patch; ///< The owning patch.
  bsp_face_t *out; ///< The emitted BSP face.
  struct patch_face_s *next; ///< Linked list pointer for node assignment.
} patch_face_t;

/**
 * @brief The map file representation of a Bézier surface patch (patchDef2).
 */
typedef struct patch_s {
  char texture[MAX_QPATH]; ///< The texture name.
  int32_t width, height; ///< The control point grid dimensions.
  patch_control_point_t control_points[MAX_PATCH_WIDTH * MAX_PATCH_HEIGHT]; ///< The control point grid.
  int32_t entity; ///< The entity number within the map.
  int32_t material; ///< The BSP material number.
  int32_t contents; ///< The CONTENTS_* mask.
  int32_t surface; ///< The SURF_* mask.
  box3_t bounds; ///< The AABB of all tessellated faces.
  bsp_patch_t *out; ///< The emitted BSP patch, assigned during EmitPatches.
  patch_face_t *faces; ///< The pre-tessellated faces.
  int32_t num_faces; ///< The count of pre-tessellated faces.
} patch_t;

extern int32_t num_patches;
extern patch_t patches[MAX_PATCHES];

patch_t *ParsePatch(parser_t *parser, int32_t entity);
void EmitPatchCollisionBrushes(patch_t *patch, entity_t *entity);
void TessellatePatches(int32_t entity_num);
void AssignPatchFacesToNodes(struct node_s *head_node, int32_t entity_num);
void FreePatchFaces(int32_t entity_num);
void EmitPatches(const bsp_model_t *mod);
