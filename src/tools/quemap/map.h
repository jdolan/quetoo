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

#include "brush.h"
#include "entity.h"

/**
 * @brief The map file representation of a plane.
 */
typedef struct plane_s {
  vec3_t normal;              ///< The plane normal vector.
  double dist;                ///< The plane distance, with full double precision.
  int32_t type;               ///< The plane type, for axial optimizations.
  struct plane_s *hash_chain; ///< The plane hash chain, for fast plane lookups.
} plane_t;

/**
 * @brief The map file reprensetation of a brush side.
 * @details Beyond the original sides defined in the .map file, additional brush sides
 * may be allocated to provide axial clipping planes for all brushes. These are known as
 * bevels. Bevels should not be used for BSP splitting and face generation. They are
 * only used for collision detection.
 */
typedef struct brush_side_s {
  char texture[MAX_QPATH];             ///< The texture name.
  vec2_t shift;                        ///< The texture shift, in pixels.
  float rotate;                        ///< The texture rotation, in Euler degrees.
  vec2_t scale;                        ///< The texture scale.
  vec4_t axis[2];                      ///< The texture axis for S and T, in xyz + offset notation.
  int32_t contents;                    ///< The CONTENTS_* mask.
  int32_t surface;                     ///< The SURF_* mask.
  int32_t value;                       ///< The value, for e.g. `SURF_PHONG`.
  int32_t plane;                       ///< The BSP plane number.
  int32_t material;                    ///< The BSP material number.
  cm_winding_t *winding;               ///< All brush sides will have a valid winding.
  const struct brush_side_s *original; ///< Points to the original side from which this split side was derived.
  bsp_brush_side_t *out;               ///< The BSP brush side emitted from this map brush side.
} brush_side_t;

/**
 * @brief The map file representation of a brush.
 */
typedef struct brush_s {
  int32_t entity;            ///< The entity number within the map.
  int32_t brush;             ///< The brush number within the entity.
  int32_t contents;          ///< The combined CONTENTS_* mask (bitwise OR) of all sides of this brush.
  box3_t bounds;             ///< The brush bounds, calculated by clipping all side planes against each other.
  brush_side_t *brush_sides; ///< The brush sides (pointer to a statically allocated global array).
  int32_t num_brush_sides;   ///< The number of brush sides.
  bsp_brush_t *out;          ///< The BSP brush emitted from this map brush.
} brush_t;

extern int32_t num_entities;
extern entity_t entities[MAX_BSP_ENTITIES];

extern plane_t planes[MAX_BSP_PLANES];
extern int32_t num_planes;

extern int32_t num_brushes;
extern brush_t brushes[MAX_BSP_BRUSHES];

extern int32_t num_brush_sides;
extern brush_side_t brush_sides[MAX_BSP_BRUSH_SIDES];

extern box3_t map_bounds;

int32_t FindPlane(const vec3_t normal, double dist);
void MakeBrushWindings(brush_t *brush);
void AddBrushBevels(brush_t *b);
void LoadMapFile(const char *filename);
