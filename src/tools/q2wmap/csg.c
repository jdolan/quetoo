/*
 * Copyright(c) 1997-2001 Id Software, Inc.
 * Copyright(c) 2002 The Quakeforge Project.
 * Copyright(c) 2006 Quake2World.
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

#include "qbsp.h"

/*
 *
 * tag all brushes with original contents
 * brushes may contain multiple contents
 * there will be no brush overlap after csg phase
 *
 *
 *
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
 *
 *
 */

/*
 * ===============
 * SubtractBrush
 *
 * Returns a list of brushes that remain after B is subtracted from A.
 * May by empty if A is contained inside B.
 *
 * The originals are undisturbed.
 * ===============
 */
static bsp_brush_t *SubtractBrush(bsp_brush_t * a, bsp_brush_t * b) { // a - b = out (list)
	int i;
	bsp_brush_t *front, *back;
	bsp_brush_t *out, *in;

	in = a;
	out = NULL;
	for (i = 0; i < b->num_sides && in; i++) {
		SplitBrush(in, b->sides[i].plane_num, &front, &back);
		if (in != a)
			FreeBrush(in);
		if (front) { // add to list
			front->next = out;
			out = front;
		}
		in = back;
	}
	if (in)
		FreeBrush(in);
	else { // didn't really intersect
		FreeBrushList(out);
		return a;
	}
	return out;
}

/*
 * ===============
 * BrushesDisjoint
 *
 * Returns true if the two brushes definately do not intersect.
 * There will be false negatives for some non-axial combinations.
 * ===============
 */
static boolean_t BrushesDisjoint(const bsp_brush_t * a, const bsp_brush_t * b) {
	int i, j;

	// check bounding boxes
	for (i = 0; i < 3; i++)
		if (a->mins[i] >= b->maxs[i] || a->maxs[i] <= b->mins[i])
			return true; // bounding boxes don't overlap

	// check for opposing planes
	for (i = 0; i < a->num_sides; i++) {
		for (j = 0; j < b->num_sides; j++) {
			if (a->sides[i].plane_num == (b->sides[j].plane_num ^ 1))
				return true; // opposite planes, so not touching
		}
	}

	return false; // might intersect
}

static int minplane_nums[3];
static int maxplane_nums[3];

/*
 * ===============
 * ClipBrushToBox
 *
 * Any planes shared with the box edge will be set to no texinfo
 * ===============
 */
static bsp_brush_t *ClipBrushToBox(bsp_brush_t * brush, vec3_t clipmins,
		vec3_t clipmaxs) {
	int i, j;
	bsp_brush_t *front, *back;
	int p;

	for (j = 0; j < 2; j++) {
		if (brush->maxs[j] > clipmaxs[j]) {
			SplitBrush(brush, maxplane_nums[j], &front, &back);
			FreeBrush(brush);
			if (front)
				FreeBrush(front);
			brush = back;
			if (!brush)
				return NULL;
		}
		if (brush->mins[j] < clipmins[j]) {
			SplitBrush(brush, minplane_nums[j], &front, &back);
			FreeBrush(brush);
			if (back)
				FreeBrush(back);
			brush = front;
			if (!brush)
				return NULL;
		}
	}

	// remove any colinear faces

	for (i = 0; i < brush->num_sides; i++) {
		p = brush->sides[i].plane_num & ~1;
		if (p == maxplane_nums[0] || p == maxplane_nums[1] || p
				== minplane_nums[0] || p == minplane_nums[1]) {
			brush->sides[i].texinfo = TEXINFO_NODE;
			brush->sides[i].visible = false;
		}
	}
	return brush;
}

/*
 * ===============
 * MakeBspBrushList
 * ===============
 */
bsp_brush_t *MakeBspBrushList(int startbrush, int endbrush, vec3_t clipmins,
		vec3_t clipmaxs) {
	bsp_brush_t *brushlist, *newbrush;
	int i, j;
	int c_faces;
	int c_brushes;
	int num_sides;
	int vis;
	vec3_t normal;
	float dist;

	for (i = 0; i < 2; i++) {
		VectorClear(normal);
		normal[i] = 1;
		dist = clipmaxs[i];
		maxplane_nums[i] = FindFloatPlane(normal, dist);
		dist = clipmins[i];
		minplane_nums[i] = FindFloatPlane(normal, dist);
	}

	brushlist = NULL;
	c_faces = 0;
	c_brushes = 0;

	for (i = startbrush; i < endbrush; i++) {
		map_brush_t *mb = &map_brushes[i];

		num_sides = mb->num_sides;
		if (!num_sides)
			continue;
		// make sure the brush has at least one face showing
		vis = 0;
		for (j = 0; j < num_sides; j++)
			if (mb->original_sides[j].visible && mb->original_sides[j].winding)
				vis++;
#if 0
		if(!vis)
		continue; // no faces at all
#endif
		// if the brush is outside the clip area, skip it
		for (j = 0; j < 3; j++)
			if (mb->mins[j] >= clipmaxs[j] || mb->maxs[j] <= clipmins[j])
				break;
		if (j != 3)
			continue;

		//
		// make a copy of the brush
		//
		newbrush = AllocBrush(mb->num_sides);
		newbrush->original = mb;
		newbrush->num_sides = mb->num_sides;
		memcpy(newbrush->sides, mb->original_sides, num_sides * sizeof(side_t));
		for (j = 0; j < num_sides; j++) {
			if (newbrush->sides[j].winding)
				newbrush->sides[j].winding = CopyWinding(
						newbrush->sides[j].winding);
			if (newbrush->sides[j].surf & SURF_HINT)
				newbrush->sides[j].visible = true; // hints are always visible
		}
		VectorCopy(mb->mins, newbrush->mins);
		VectorCopy(mb->maxs, newbrush->maxs);

		//
		// carve off anything outside the clip box
		//
		newbrush = ClipBrushToBox(newbrush, clipmins, clipmaxs);
		if (!newbrush)
			continue;

		c_faces += vis;
		c_brushes++;

		newbrush->next = brushlist;
		brushlist = newbrush;
	}

	return brushlist;
}

/*
 * ===============
 * AddBspBrushListToTail
 * ===============
 */
static bsp_brush_t *AddBrushListToTail(bsp_brush_t * list, bsp_brush_t * tail) {
	bsp_brush_t *walk, *next;

	for (walk = list; walk; walk = next) { // add to end of list
		next = walk->next;
		walk->next = NULL;
		tail->next = walk;
		tail = walk;
	}

	return tail;
}

/*
 * ===========
 * CullList
 *
 * Builds a new list that doesn't hold the given brush
 * ===========
 */
static bsp_brush_t *CullList(bsp_brush_t * list, const bsp_brush_t * skip1) {
	bsp_brush_t *newlist;
	bsp_brush_t *next;

	newlist = NULL;

	for (; list; list = next) {
		next = list->next;
		if (list == skip1) {
			FreeBrush(list);
			continue;
		}
		list->next = newlist;
		newlist = list;
	}
	return newlist;
}

/*
 * BrushGE
 *
 * Returns true if b1 is allowed to bite b2
 */
static inline boolean_t BrushGE(const bsp_brush_t * b1, const bsp_brush_t * b2) {
	// detail brushes never bite structural brushes
	if ((b1->original->contents & CONTENTS_DETAIL) && !(b2->original->contents
			& CONTENTS_DETAIL))
		return false;
	if (b1->original->contents & CONTENTS_SOLID)
		return true;
	return false;
}

/*
 * ChopBrushes
 *
 * Carves any intersecting solid brushes into the minimum number
 * of non-intersecting brushes.
 */
bsp_brush_t *ChopBrushes(bsp_brush_t * head) {
	bsp_brush_t *b1, *b2, *next;
	bsp_brush_t *tail;
	bsp_brush_t *keep;
	bsp_brush_t *sub, *sub2;
	int c1, c2;

	Com_Verbose("---- ChopBrushes ----\n");
	Com_Verbose("original brushes: %i\n", CountBrushList(head));

	keep = NULL;

	newlist:
	// find tail
	if (!head)
		return NULL;
	for (tail = head; tail->next; tail = tail->next)
		;

	for (b1 = head; b1; b1 = next) {
		next = b1->next;
		for (b2 = b1->next; b2; b2 = b2->next) {
			if (BrushesDisjoint(b1, b2))
				continue;

			sub = NULL;
			sub2 = NULL;
			c1 = 999999;
			c2 = 999999;

			if (BrushGE(b2, b1)) {
				sub = SubtractBrush(b1, b2);
				if (sub == b1)
					continue; // didn't really intersect
				if (!sub) { // b1 is swallowed by b2
					head = CullList(b1, b1);
					goto newlist;
				}
				c1 = CountBrushList(sub);
			}

			if (BrushGE(b1, b2)) {
				sub2 = SubtractBrush(b2, b1);
				if (sub2 == b2)
					continue; // didn't really intersect
				if (!sub2) { // b2 is swallowed by b1
					FreeBrushList(sub);
					head = CullList(b1, b2);
					goto newlist;
				}
				c2 = CountBrushList(sub2);
			}

			if (!sub && !sub2)
				continue; // neither one can bite

			// only accept if it didn't fragment
			// (commening this out allows full fragmentation)
			if (c1 > 1 && c2 > 1) {
				if (sub2)
					FreeBrushList(sub2);
				if (sub)
					FreeBrushList(sub);
				continue;
			}

			if (c1 < c2) {
				if (sub2)
					FreeBrushList(sub2);
				tail = AddBrushListToTail(sub, tail);
				head = CullList(b1, b1);
				goto newlist;
			} else {
				if (sub)
					FreeBrushList(sub);
				tail = AddBrushListToTail(sub2, tail);
				head = CullList(b1, b2);
				goto newlist;
			}
		}

		if (!b2) { // b1 is no longer intersecting anything, so keep it
			b1->next = keep;
			keep = b1;
		}
	}

	Com_Verbose("output brushes: %i\n", CountBrushList(keep));
	return keep;
}
