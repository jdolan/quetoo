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

#include "polylib.h"

/**
 * @brief The in-tree representation of BSP faces.
 */
typedef struct Face {

  /**
   * @brief Faces are chained on the node on which they reside (either side may be chained together).
   */
  struct Face *next;

  /**
   * @brief If set, this face has been merged and should not be emitted to the BSP.
   */
  struct Face *merged;

  /**
   * @brief The original brush side that created this face.
   */
  const struct BrushSide *brush_side;

  /**
   * @brief The plane number.
   */
  int32_t plane;

  /**
   * @brief The ordered, welded face winding, used to emit BSP vertexes.
   */
  CmWinding *w;

  /**
   * @brief The output face in the BSP, so that node faces may emit leaf faces.
   */
  BspFace *out;
} Face;

extern int32_t num_welds;

Face *AllocFace(void);
void FreeFace(Face *f);
Face *MergeFaces(Face *a, Face *b);
void ClearWeldingSpatialHash(void);
BspFace *EmitFace(const Face *face);
void PhongShading(const BspModel *mod);
void TangentVectors(void);
