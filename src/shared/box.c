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

#include <stdlib.h>

#include "box.h"

/**
 * @see box.h
 */
size_t Box3_Merge(const box3_t *boxes, size_t count, box3_t **out) {

  if (!count) {
    *out = NULL;
    return 0;
  }

  const vec3_t cell = Box3_Size(boxes[0]);

  vec3_t origin = boxes[0].mins;
  for (size_t i = 1; i < count; i++) {
    origin = Vec3_Minf(origin, boxes[i].mins);
  }

  // Quantize each box's mins to an integer cell coordinate, scoped to the
  // touched region only (not any larger grid the boxes may belong to).

  vec3i_t *coords = malloc(count * sizeof(vec3i_t));

  int32_t size_x = 0, size_y = 0, size_z = 0;

  for (size_t i = 0; i < count; i++) {
    const vec3_t rel = Vec3_Subtract(boxes[i].mins, origin);

    const int32_t x = (int32_t) roundf(rel.x / cell.x);
    const int32_t y = (int32_t) roundf(rel.y / cell.y);
    const int32_t z = (int32_t) roundf(rel.z / cell.z);

    coords[i] = Vec3i(x, y, z);

    size_x = Maxi(size_x, x + 1);
    size_y = Maxi(size_y, y + 1);
    size_z = Maxi(size_z, z + 1);
  }

  const int32_t xy = size_x * size_y;

  uint8_t *occupied = calloc(1, xy * size_z);

  for (size_t i = 0; i < count; i++) {
    const vec3i_t c = coords[i];
    occupied[c.z * xy + c.y * size_x + c.x] = 1;
  }

  free(coords);

  // Greedily merge contiguous runs of occupied cells into boxes: extend as far as
  // possible along X, then Y, then Z, consuming every cell the merged box covers.

  box3_t *merged = malloc(count * sizeof(box3_t));
  size_t num_merged = 0;

  for (int32_t z = 0; z < size_z; z++) {
    for (int32_t y = 0; y < size_y; y++) {
      for (int32_t x = 0; x < size_x; x++) {

        if (!occupied[z * xy + y * size_x + x]) {
          continue;
        }

        int32_t ex = x;
        while (ex + 1 < size_x && occupied[z * xy + y * size_x + (ex + 1)]) {
          ex++;
        }

        int32_t ey = y;
        while (ey + 1 < size_y) {
          bool row_occupied = true;
          for (int32_t xi = x; xi <= ex; xi++) {
            if (!occupied[z * xy + (ey + 1) * size_x + xi]) {
              row_occupied = false;
              break;
            }
          }
          if (!row_occupied) {
            break;
          }
          ey++;
        }

        int32_t ez = z;
        while (ez + 1 < size_z) {
          bool plane_occupied = true;
          for (int32_t yi = y; yi <= ey && plane_occupied; yi++) {
            for (int32_t xi = x; xi <= ex; xi++) {
              if (!occupied[(ez + 1) * xy + yi * size_x + xi]) {
                plane_occupied = false;
                break;
              }
            }
          }
          if (!plane_occupied) {
            break;
          }
          ez++;
        }

        for (int32_t zi = z; zi <= ez; zi++) {
          for (int32_t yi = y; yi <= ey; yi++) {
            for (int32_t xi = x; xi <= ex; xi++) {
              occupied[zi * xy + yi * size_x + xi] = 0;
            }
          }
        }

        const vec3_t mins = Vec3_Add(origin, Vec3_Multiply(Vec3i_CastVec3(Vec3i(x, y, z)), cell));
        const vec3_t maxs = Vec3_Add(origin, Vec3_Multiply(Vec3i_CastVec3(Vec3i(ex + 1, ey + 1, ez + 1)), cell));

        merged[num_merged++] = Box3(mins, maxs);
      }
    }
  }

  free(occupied);

  *out = realloc(merged, num_merged * sizeof(box3_t));
  return num_merged;
}
