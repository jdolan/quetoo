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

#include "bsp.h"
#include "writebsp.h"

/**
 * @brief The triangles that the depth pass and the draw elements of a model are emitted from:
 * those of one brush side in one block, or those of one patch face.
 */
typedef struct {

  /**
   * @brief A face of the group, which gives its material, surface, reflection plane and portal.
   */
  const BspFace *face;

  /**
   * @brief The `CONTENTS_BLOCK` node that holds the group, or -1.
   */
  int32_t blockNode;

  /**
   * @brief The triangles, as indexes into the vertexes lump.
   */
  const int32_t *elements;

  /**
   * @brief The count of elements.
   */
  int32_t numElements;

  /**
   * @brief The bounds of the faces of the group.
   */
  Box3 bounds;
} DrawFace;

extern DrawFace *drawFaces;
extern int32_t numDrawFaces;

/**
 * @brief Resolves the material index for the given BSP face.
 */
static inline int32_t FaceMaterial(const BspFace *face) {
  if (face->brushSide >= 0) {
    return bspFile.brushSides[face->brushSide].material;
  }
  return bspFile.patches[face->patch].material;
}

/**
 * @brief Resolves the contents mask for the given BSP face.
 */
static inline int32_t FaceContents(const BspFace *face) {
  if (face->brushSide >= 0) {
    return bspFile.brushSides[face->brushSide].contents;
  }
  return bspFile.patches[face->patch].contents;
}

/**
 * @brief Resolves the surface mask for the given BSP face.
 */
static inline int32_t FaceSurface(const BspFace *face) {
  if (face->brushSide >= 0) {
    return bspFile.brushSides[face->brushSide].surface;
  }
  return bspFile.patches[face->patch].surface;
}

void EmitDrawFaces(const BspModel *mod);
int32_t DrawFaceBlockNode(int32_t face);
void FreeDrawFaces(void);
