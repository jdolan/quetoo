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
#include "cm_voxel.h"

/**
 * @brief Returns a pointer to the decoded voxel for the given world position,
 * or NULL if voxel data is not available.
 * @param pos The world-space query position.
 * @return Pointer into cmBsp.voxels[], clamped to grid bounds. Safe to call from any thread.
 */
const CmVoxel *Cm_VoxelForPoint(const Vec3 pos) {

  if (!cmBsp.voxels) {
    return NULL;
  }

  const Vec3 rel = Vec3_Subtract(pos, cmBsp.voxelBounds.mins);

  int32_t xi = (int32_t) (rel.x / BSP_VOXEL_SIZE);
  int32_t yi = (int32_t) (rel.y / BSP_VOXEL_SIZE);
  int32_t zi = (int32_t) (rel.z / BSP_VOXEL_SIZE);

  if (xi < 0) { xi = 0; } else if (xi >= cmBsp.voxelSize.x) { xi = cmBsp.voxelSize.x - 1; }
  if (yi < 0) { yi = 0; } else if (yi >= cmBsp.voxelSize.y) { yi = cmBsp.voxelSize.y - 1; }
  if (zi < 0) { zi = 0; } else if (zi >= cmBsp.voxelSize.z) { zi = cmBsp.voxelSize.z - 1; }

  return &cmBsp.voxels[(zi * cmBsp.voxelSize.y + yi) * cmBsp.voxelSize.x + xi];
}
