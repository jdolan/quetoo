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

#include <SDL3/SDL_timer.h>

#include "brush.h"
#include "map.h"

/*
 *
 * tag all brushes with original contents
 * brushes may contain multiple contents
 * there will be no brush overlap after csg phase
 *
 * each side has a count of the other sides it splits
 *
 * the best split will be the one that minimizes the total split counts
 * of all remaining sides
 *
 * precalc side on plane table
 *
 * evaluate split side
 * {
 * cost = 0
 * for all sides
 *   for all sides
 *     get
 *     if side splits side and splitside is on same child
 *       cost++;
 * }
 */

/**
 * @brief Subtracts brush B from brush A by clipping A against all of B's planes.
 * @return A list of brushes that remain after B is subtracted from A.
 * @remark May by empty if A is contained inside B.
 * @remark The originals are undisturbed.
 */
static CsgBrush *SubtractBrush(CsgBrush *a, CsgBrush *b) {

  CsgBrush *in = a;
  CsgBrush *out = NULL;

  CsgBrush *front = NULL, *back = NULL;

  for (int32_t i = 0; i < b->numBrushSides && in; i++) {
    SplitBrush(in, b->brushSides[i].plane, &front, &back);
    if (in != a) {
      FreeBrush(in);
    }
    if (front) { // add to list
      front->next = out;
      out = front;
    }
    in = back;
  }

  if (in) {
    FreeBrush(in);
  } else { // no intersection
    FreeBrushes(out);
    return a;
  }

  return out;
}

/**
 * @return True if the two brushes do not intersect.
 * @remarks There will be false negatives for some non-axial combinations.
 */
static bool BrushesDisjoint(const CsgBrush *a, const CsgBrush *b) {

  // check bounding boxes
  if (!Box3_Intersects(a->bounds, b->bounds)) {
    return true; // bounding boxes don't overlap
  }

  // check for opposing planes
  for (int32_t i = 0; i < a->numBrushSides; i++) {
    for (int32_t j = 0; j < b->numBrushSides; j++) {
      if (a->brushSides[i].plane == (b->brushSides[j].plane ^ 1)) {
        return true; // opposite planes, so not touching
      }
    }
  }

  return false; // might intersect
}

/**
 * @brief Create a list of `CsgBrush` for the `Brush` between start and start + count.
 */
CsgBrush *MakeBrushes(int32_t index, int32_t count) {

  const uint32_t start = (uint32_t) SDL_GetTicks();

  CsgBrush *list = NULL;

  const Brush *in = &brushes[index];
  for (int32_t i = 0; i < count; i++, in++) {

    if (!in->numBrushSides) {
      continue;
    }
    
    CsgBrush *out = AllocBrush(in->numBrushSides);

    out->original = in;
    out->numBrushSides = in->numBrushSides;

    for (int32_t j = 0; j < out->numBrushSides; j++) {

      out->brushSides[j] = in->brushSides[j];
      out->brushSides[j].original = &in->brushSides[j];

      if (in->brushSides[j].winding) {
        out->brushSides[j].winding = Cm_CopyWinding(in->brushSides[j].winding);
      }
    }
    
    out->bounds = in->bounds;

    out->next = list;
    list = out;

    Progress("Creating brushes", i * 100.f / count);
  }

  Com_Print("\r%-24s [100%%] %d ms\n", "Creating brushes", (uint32_t) SDL_GetTicks() - start);

  return list;
}

/**
 * @brief Appends brushes from the list tail onto the accumulator list; returns the new tail.
 */
static CsgBrush *AddBrushToBrushes(CsgBrush *list, CsgBrush *tail) {
  CsgBrush *walk, *next;

  for (walk = list; walk; walk = next) { // add to end of list
    next = walk->next;
    walk->next = NULL;
    tail->next = walk;
    tail = walk;
  }

  return tail;
}

/**
 * @brief Builds a new list that doesn't hold the given brush.
 */
static CsgBrush *RemoveBrushFromBrushes(CsgBrush *list, const CsgBrush *skip) {
  CsgBrush *next;
  CsgBrush *newList = NULL;

  for (; list; list = next) {
    next = list->next;
    if (list == skip) {
      FreeBrush(list);
      continue;
    }
    list->next = newList;
    newList = list;
  }
  return newList;
}

/**
 * @brief Returns true if b1 is allowed to bite b2
 */
static inline bool BrushGE(const CsgBrush *b1, const CsgBrush *b2) {
  // detail brushes never bite structural brushes
  if ((b1->original->contents & CONTENTS_DETAIL) && !(b2->original->contents & CONTENTS_DETAIL)) {
    return false;
  }
//   caulk (nodraw) brushes never bite anything
//  if (b1->original->brush_sides[0].surface & SURF_NO_DRAW) {
//    return false;
//  }
  if (b1->original->contents & CONTENTS_SOLID) {
    return true;
  }
  return false;
}

/**
 * @brief Carves any intersecting solid brushes into the minimum number
 * of non-intersecting brushes.
 */
CsgBrush *SubtractBrushes(CsgBrush *head) {

  const uint32_t start = (uint32_t) SDL_GetTicks();

  size_t headCount = CountBrushes(head);

  CsgBrush *keep = NULL;

newlist:
  if (!head) {
    return NULL;
  }

  CsgBrush *tail = head;
  for (; tail->next; tail = tail->next) {
    // find tail
  }

  CsgBrush *next;
  for (CsgBrush *b1 = head; b1; b1 = next) {
    next = b1->next;
    CsgBrush *b2 = next;
    for (; b2; b2 = b2->next) {
      if (BrushesDisjoint(b1, b2)) {
        continue;
      }

      CsgBrush *sub1 = NULL;
      CsgBrush *sub2 = NULL;

      size_t c1 = SIZE_MAX;
      size_t c2 = SIZE_MAX;

      if (BrushGE(b2, b1)) {
        sub1 = SubtractBrush(b1, b2);
        if (sub1 == b1) {
          continue; // didn't really intersect
        }
        if (!sub1) { // b1 is swallowed by b2
          head = RemoveBrushFromBrushes(b1, b1);
          goto newlist;
        }
        c1 = CountBrushes(sub1);
      }

      if (BrushGE(b1, b2)) {
        sub2 = SubtractBrush(b2, b1);
        if (sub2 == b2) {
          continue; // didn't really intersect
        }
        if (!sub2) { // b2 is swallowed by b1
          FreeBrushes(sub1);
          head = RemoveBrushFromBrushes(b1, b2);
          goto newlist;
        }
        c2 = CountBrushes(sub2);
      }

      if (!sub1 && !sub2) {
        continue;  // neither one can bite
      }

      // only accept if it didn't fragment
      // (commening this out allows full fragmentation)
      if (c1 > 1 && c2 > 1) {
        if (sub2) {
          FreeBrushes(sub2);
        }
        if (sub1) {
          FreeBrushes(sub1);
        }
        continue;
      }

      if (c1 < c2) {
        if (sub2) {
          FreeBrushes(sub2);
        }
        tail = AddBrushToBrushes(sub1, tail);
        head = RemoveBrushFromBrushes(b1, b1);
        goto newlist;
      } else {
        if (sub1) {
          FreeBrushes(sub1);
        }
        tail = AddBrushToBrushes(sub2, tail);
        head = RemoveBrushFromBrushes(b1, b2);
        goto newlist;
      }
    }

    if (!b2) { // b1 is no longer intersecting anything, so keep it
      b1->next = keep;
      keep = b1;
    }

    Progress("Subtracting brushes", -1);
  }

  Com_Verbose("SubtractBrushes: %zi / %zi\n", headCount, CountBrushes(keep));

  Com_Print("\r%-24s [100%%] %d ms\n", "Subtracting brushes", (uint32_t) SDL_GetTicks() - start);

  return keep;
}
