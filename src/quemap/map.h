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

#include "brush.h"
#include "entity.h"

/**
 * @brief The map file representation of a plane.
 */
typedef struct Plane {

  /**
   * @brief The plane normal vector.
   */
  Vec3 normal;

  /**
   * @brief The plane distance, with full double precision.
   */
  double dist;

  /**
   * @brief The plane type, for axial optimizations.
   */
  int32_t type;

  /**
   * @brief The plane hash chain, for fast plane lookups.
   */
  struct Plane *hashChain;
} Plane;

/**
 * @brief The map file reprensetation of a brush side.
 * @details Beyond the original sides defined in the .map file, additional brush sides
 * may be allocated to provide axial clipping planes for all brushes. These are known as
 * bevels. Bevels should not be used for BSP splitting and face generation. They are
 * only used for collision detection.
 */
typedef struct BrushSide {

  /**
   * @brief The texture name.
   */
  char texture[MAX_QPATH];

  /**
   * @brief The texture shift, in pixels.
   */
  Vec2 shift;

  /**
   * @brief The texture rotation, in Euler degrees.
   */
  float rotate;

  /**
   * @brief The texture scale.
   */
  Vec2 scale;

  /**
   * @brief The texture axis for S and T, in xyz + offset notation.
   */
  Vec4 axis[2];

  /**
   * @brief The `CONTENTS_`* mask.
   */
  int32_t contents;

  /**
   * @brief The `SURF_`* mask.
   */
  int32_t surface;

  /**
   * @brief The value, for e.g. `SURF_PHONG`.
   */
  int32_t value;

  /**
   * @brief The BSP plane number.
   */
  int32_t plane;

  /**
   * @brief The BSP material number.
   */
  int32_t material;

  /**
   * @brief All brush sides will have a valid winding.
   */
  CmWinding *winding;

  /**
   * @brief Points to the original side from which this split side was derived.
   */
  const struct BrushSide *original;

  /**
   * @brief The BSP brush side emitted from this map brush side.
   */
  BspBrushSide *out;
} BrushSide;

/**
 * @brief The map file representation of a brush.
 */
typedef struct Brush {

  /**
   * @brief The entity number within the map.
   */
  int32_t entity;

  /**
   * @brief The brush number within the entity.
   */
  int32_t brush;

  /**
   * @brief The combined `CONTENTS_`* mask (bitwise OR) of all sides of this brush.
   */
  int32_t contents;

  /**
   * @brief The brush bounds, calculated by clipping all side planes against each other.
   */
  Box3 bounds;

  /**
   * @brief The brush sides (pointer to a statically allocated global array).
   */
  BrushSide *brushSides;

  /**
   * @brief The number of brush sides.
   */
  int32_t numBrushSides;

  /**
   * @brief The BSP brush emitted from this map brush.
   */
  BspBrush *out;
} Brush;

/**
 * @brief Map file format.
 */
typedef enum {
    MAP_FORMAT_UNKNOWN = 0,
    MAP_FORMAT_Q2,   // Quake II style map/bsp
    MAP_FORMAT_Q3,   // Quake III / Radiant style map
    MAP_FORMAT_VALVE // Valve / Source style map (optional)
} MapFormat;

extern MapFormat mapFormat;

extern int32_t numEntities;
extern Entity entities[MAX_BSP_ENTITIES];

extern Plane planes[MAX_BSP_PLANES];
extern int32_t numPlanes;

extern int32_t numBrushes;
extern Brush brushes[MAX_BSP_BRUSHES];

extern int32_t numBrushSides;
extern BrushSide brushSides[MAX_BSP_BRUSH_SIDES];

extern Box3 mapBounds;

int32_t FindPlane(const Vec3 normal, double dist);
void MakeBrushWindings(Brush *brush);
void AddBrushBevels(Brush *b);
MapFormat LoadMapFile(const char *filename);
