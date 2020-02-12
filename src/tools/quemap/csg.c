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
 * 	for all sides
 * 		get
 * 		if side splits side and splitside is on same child
 * 			cost++;
 * }
 */

/**
 * @brief
 * @return A list of brushes that remain after B is subtracted from A.
 * @remark May by empty if A is contained inside B.
 * @remark The originals are undisturbed.
 */
static csg_brush_t *SubtractBrush(csg_brush_t *a, csg_brush_t *b) {

	csg_brush_t *in = a;
	csg_brush_t *out = NULL;

	csg_brush_t *front = NULL, *back = NULL;

	for (int32_t i = 0; i < b->num_sides && in; i++) {
		SplitBrush(in, b->sides[i].plane_num, &front, &back);
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
 * @return True if the two brushes definately do not intersect.
 * @remark There will be false negatives for some non-axial combinations.
 */
static _Bool BrushesDisjoint(const csg_brush_t *a, const csg_brush_t *b) {

	// check bounding boxes
	for (int32_t i = 0; i < 3; i++)
		if (a->mins.xyz[i] >= b->maxs.xyz[i] || a->maxs.xyz[i] <= b->mins.xyz[i]) {
			return true;    // bounding boxes don't overlap
		}

	// check for opposing planes
	for (int32_t i = 0; i < a->num_sides; i++) {
		for (int32_t j = 0; j < b->num_sides; j++) {
			if (a->sides[i].plane_num == (b->sides[j].plane_num ^ 1)) {
				return true;    // opposite planes, so not touching
			}
		}
	}

	return false; // might intersect
}

/**
 * @brief Any planes shared with the box edge will be set to TEXINFO_NODE.
 */
static csg_brush_t *ClipBrushToBox(csg_brush_t *brush, const vec3_t mins, const vec3_t maxs, int32_t box_planes[6]) {

	csg_brush_t *front = NULL, *back = NULL;

	for (int32_t i = 0; i < 2; i++) {
		if (brush->maxs.xyz[i] > maxs.xyz[i]) {
			SplitBrush(brush, box_planes[i + 3], &front, &back);
			FreeBrush(brush);
			if (front) {
				FreeBrush(front);
			}
			brush = back;
			if (!brush) {
				return NULL;
			}
		}
		if (brush->mins.xyz[i] < mins.xyz[i]) {
			SplitBrush(brush, box_planes[i], &front, &back);
			FreeBrush(brush);
			if (back) {
				FreeBrush(back);
			}
			brush = front;
			if (!brush) {
				return NULL;
			}
		}
	}

	// remove any colinear faces

	for (int32_t i = 0; i < brush->num_sides; i++) {
		const int32_t plane_num = brush->sides[i].plane_num & ~1;
		for (int32_t j = 0; j < 6; j++) {
			if (plane_num == box_planes[j]) {
				brush->sides[i].texinfo = TEXINFO_NODE;
				brush->sides[i].visible = false;
				break;
			}
		}
	}

	return brush;
}

/**
 * @brief Create a list of csg_brush_t of the brush_t occupying, and optionally
 * clipped to, the given bounding box.
 */
csg_brush_t *MakeBrushes(int32_t start, int32_t end, const vec3_t mins, const vec3_t maxs) {
	csg_brush_t *list = NULL;

	int32_t box_planes[6];

	for (int32_t i = 0; i < 3; i++) {
		vec3_t normal;
		normal = vec3_zero();
		normal.xyz[i] = 1.0;
		box_planes[i] = FindPlane(normal, mins.xyz[i]);
		box_planes[i + 3] = FindPlane(normal, maxs.xyz[i]);
	}

	int32_t c_faces = 0;
	int32_t c_brushes = 0;

	for (int32_t i = start; i < end; i++) {
		brush_t *b = &brushes[i];

		if (!b->num_sides) {
			continue;
		}
		// make sure the brush has at least one face showing
		int32_t vis = 0;
		for (int32_t j = 0; j < b->num_sides; j++)
			if (b->original_sides[j].visible && b->original_sides[j].winding) {
				vis++;
			}
#if 0
		if (!vis) {
			continue;    // no faces at all
		}
#endif
		// if the brush is outside the clip area, skip it
		int32_t j;
		for (j = 0; j < 3; j++)
			if (b->mins.xyz[j] >= maxs.xyz[j] || b->maxs.xyz[j] <= mins.xyz[j]) {
				break;
			}
		if (j != 3) {
			continue;
		}

		// create a csg_brush_t for the brush_t
		csg_brush_t *brush = AllocBrush(b->num_sides);
		brush->original = b;
		brush->num_sides = b->num_sides;
		memcpy(brush->sides, b->original_sides, brush->num_sides * sizeof(brush_side_t));
		for (int32_t j = 0; j < b->num_sides; j++) {
			if (brush->sides[j].winding) {
				brush->sides[j].winding = Cm_CopyWinding(brush->sides[j].winding);
			}
			if (brush->sides[j].surf & SURF_HINT) {
				brush->sides[j].visible = true; // hints are always visible
			}
		}
		brush->mins = b->mins;
		brush->maxs = b->maxs;

		// carve off anything outside the clip box
		brush = ClipBrushToBox(brush, mins, maxs, box_planes);
		if (!brush) {
			continue;
		}

		c_faces += vis;
		c_brushes++;

		brush->next = list;
		list = brush;
	}

	return list;
}

/**
 * @brief
 */
static csg_brush_t *AddBrushToBrushes(csg_brush_t *list, csg_brush_t *tail) {
	csg_brush_t *walk, *next;

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
static csg_brush_t *RemoveBrushFromBrushes(csg_brush_t *list, const csg_brush_t *skip) {
	csg_brush_t *next;
	csg_brush_t *new_list = NULL;

	for (; list; list = next) {
		next = list->next;
		if (list == skip) {
			FreeBrush(list);
			continue;
		}
		list->next = new_list;
		new_list = list;
	}
	return new_list;
}

/**
 * @brief Returns true if b1 is allowed to bite b2
 */
static inline _Bool BrushGE(const csg_brush_t *b1, const csg_brush_t *b2) {
	// detail brushes never bite structural brushes
	if ((b1->original->contents & CONTENTS_DETAIL) && !(b2->original->contents & CONTENTS_DETAIL)) {
		return false;
	}
	if (b1->original->contents & CONTENTS_SOLID) {
		return true;
	}
	return false;
}

/**
 * @brief Carves any intersecting solid brushes into the minimum number
 * of non-intersecting brushes.
 */
csg_brush_t *ChopBrushes(csg_brush_t *head) {

	Com_Verbose("---- ChopBrushes ----\n");
	Com_Verbose("original brushes: %zi\n", CountBrushes(head));

	csg_brush_t *keep = NULL;

newlist:
	if (!head) {
		return NULL;
	}

	csg_brush_t *tail = head;
	for (; tail->next; tail = tail->next) {
		// find tail
	}

	csg_brush_t *next;
	for (csg_brush_t *b1 = head; b1; b1 = next) {
		next = b1->next;
		csg_brush_t *b2 = next;
		for (; b2; b2 = b2->next) {
			if (BrushesDisjoint(b1, b2)) {
				continue;
			}

			csg_brush_t *sub1 = NULL;
			csg_brush_t *sub2 = NULL;

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
	}

	Com_Verbose("output brushes: %zi\n", CountBrushes(keep));
	return keep;
}
