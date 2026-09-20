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

#include <stdlib.h>

#include "box.h"

/**
 * @see box.h
 */
size_t Box3_Merge(const Box3 *boxes, size_t count, Box3 **out) {

  if (!count) {
    *out = NULL;
    return 0;
  }

  const Vec3 cell = Box3_Size(boxes[0]);

  Vec3 origin = boxes[0].mins;
  for (size_t i = 1; i < count; i++) {
    origin = Vec3_Minf(origin, boxes[i].mins);
  }

  // Quantize each box's mins to an integer cell coordinate, scoped to the
  // touched region only (not any larger grid the boxes may belong to).

  Vec3i *coords = malloc(count * sizeof(Vec3i));

  int32_t sizeX = 0, sizeY = 0, sizeZ = 0;

  for (size_t i = 0; i < count; i++) {
    const Vec3 rel = Vec3_Subtract(boxes[i].mins, origin);

    const int32_t x = (int32_t) roundf(rel.x / cell.x);
    const int32_t y = (int32_t) roundf(rel.y / cell.y);
    const int32_t z = (int32_t) roundf(rel.z / cell.z);

    coords[i] = MakeVec3i(x, y, z);

    sizeX = Maxi(sizeX, x + 1);
    sizeY = Maxi(sizeY, y + 1);
    sizeZ = Maxi(sizeZ, z + 1);
  }

  const int32_t xy = sizeX * sizeY;

  uint8_t *occupied = calloc(1, xy * sizeZ);

  for (size_t i = 0; i < count; i++) {
    const Vec3i c = coords[i];
    occupied[c.z * xy + c.y * sizeX + c.x] = 1;
  }

  free(coords);

  // Greedily merge contiguous runs of occupied cells into boxes: extend as far as
  // possible along X, then Y, then Z, consuming every cell the merged box covers.

  Box3 *merged = malloc(count * sizeof(Box3));
  size_t numMerged = 0;

  for (int32_t z = 0; z < sizeZ; z++) {
    for (int32_t y = 0; y < sizeY; y++) {
      for (int32_t x = 0; x < sizeX; x++) {

        if (!occupied[z * xy + y * sizeX + x]) {
          continue;
        }

        int32_t ex = x;
        while (ex + 1 < sizeX && occupied[z * xy + y * sizeX + (ex + 1)]) {
          ex++;
        }

        int32_t ey = y;
        while (ey + 1 < sizeY) {
          bool rowOccupied = true;
          for (int32_t xi = x; xi <= ex; xi++) {
            if (!occupied[z * xy + (ey + 1) * sizeX + xi]) {
              rowOccupied = false;
              break;
            }
          }
          if (!rowOccupied) {
            break;
          }
          ey++;
        }

        int32_t ez = z;
        while (ez + 1 < sizeZ) {
          bool planeOccupied = true;
          for (int32_t yi = y; yi <= ey && planeOccupied; yi++) {
            for (int32_t xi = x; xi <= ex; xi++) {
              if (!occupied[(ez + 1) * xy + yi * sizeX + xi]) {
                planeOccupied = false;
                break;
              }
            }
          }
          if (!planeOccupied) {
            break;
          }
          ez++;
        }

        for (int32_t zi = z; zi <= ez; zi++) {
          for (int32_t yi = y; yi <= ey; yi++) {
            for (int32_t xi = x; xi <= ex; xi++) {
              occupied[zi * xy + yi * sizeX + xi] = 0;
            }
          }
        }

        const Vec3 mins = Vec3_Add(origin, Vec3_Multiply(Vec3i_CastVec3(MakeVec3i(x, y, z)), cell));
        const Vec3 maxs = Vec3_Add(origin, Vec3_Multiply(Vec3i_CastVec3(MakeVec3i(ex + 1, ey + 1, ez + 1)), cell));

        merged[numMerged++] = MakeBox3(mins, maxs);
      }
    }
  }

  free(occupied);

  *out = realloc(merged, numMerged * sizeof(Box3));
  return numMerged;
}
