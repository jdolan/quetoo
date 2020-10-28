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
 * @brief Create a list of csg_brush_t for the brush_t between start and start + count.
 */
csg_brush_t *MakeBrushes(int32_t start, int32_t count) {

	csg_brush_t *list = NULL;

	const brush_t *in = &brushes[start];
	for (int32_t i = 0; i < count; i++, in++) {

		if (!in->num_sides) {
			continue;
		}
		
		csg_brush_t *out = AllocBrush(in->num_sides);

		out->original = in;
		out->num_sides = in->num_sides;

		for (int32_t j = 0; j < out->num_sides; j++) {

			out->sides[j] = in->sides[j];
			out->sides[j].original = &in->sides[j];

			if (in->sides[j].winding) {
				out->sides[j].winding = Cm_CopyWinding(in->sides[j].winding);
			}
		}
		
		out->mins = in->mins;
		out->maxs = in->maxs;

		out->next = list;
		list = out;
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
csg_brush_t *SubtractBrushes(csg_brush_t *head) {

	size_t head_count = CountBrushes(head);

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

	Com_Verbose("ChopBrushes: %zi / %zi\n", head_count, CountBrushes(keep));
	return keep;
}
