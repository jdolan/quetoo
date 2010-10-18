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
static bspbrush_t *SubtractBrush(bspbrush_t * a, bspbrush_t * b){										 // a - b = out (list)
	int i;
	bspbrush_t *front, *back;
	bspbrush_t *out, *in;

	in = a;
	out = NULL;
	for(i = 0; i < b->numsides && in; i++){
		SplitBrush(in, b->sides[i].planenum, &front, &back);
		if(in != a)
			FreeBrush(in);
		if(front){					 // add to list
			front->next = out;
			out = front;
		}
		in = back;
	}
	if(in)
		FreeBrush(in);
	else {							  // didn't really intersect
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
static qboolean BrushesDisjoint(const bspbrush_t * a, const bspbrush_t * b){
	int i, j;

	// check bounding boxes
	for(i = 0; i < 3; i++)
		if(a->mins[i] >= b->maxs[i]
		        || a->maxs[i] <= b->mins[i])
			return true;			  // bounding boxes don't overlap

	// check for opposing planes
	for(i = 0; i < a->numsides; i++){
		for(j = 0; j < b->numsides; j++){
			if(a->sides[i].planenum == (b->sides[j].planenum ^ 1))
				return true;		  // opposite planes, so not touching
		}
	}

	return false;					  // might intersect
}

static int minplanenums[3];
static int maxplanenums[3];

/*
 * ===============
 * ClipBrushToBox
 *
 * Any planes shared with the box edge will be set to no texinfo
 * ===============
 */
static bspbrush_t *ClipBrushToBox(bspbrush_t * brush, vec3_t clipmins, vec3_t clipmaxs){
	int i, j;
	bspbrush_t *front, *back;
	int p;

	for(j = 0; j < 2; j++){
		if(brush->maxs[j] > clipmaxs[j]){
			SplitBrush(brush, maxplanenums[j], &front, &back);
			FreeBrush(brush);
			if(front)
				FreeBrush(front);
			brush = back;
			if(!brush)
				return NULL;
		}
		if(brush->mins[j] < clipmins[j]){
			SplitBrush(brush, minplanenums[j], &front, &back);
			FreeBrush(brush);
			if(back)
				FreeBrush(back);
			brush = front;
			if(!brush)
				return NULL;
		}
	}

	// remove any colinear faces

	for(i = 0; i < brush->numsides; i++){
		p = brush->sides[i].planenum & ~1;
		if(p == maxplanenums[0] || p == maxplanenums[1]
		        || p == minplanenums[0] || p == minplanenums[1]){
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
bspbrush_t *MakeBspBrushList(int startbrush, int endbrush,
                             vec3_t clipmins, vec3_t clipmaxs){
	bspbrush_t *brushlist, *newbrush;
	int i, j;
	int c_faces;
	int c_brushes;
	int numsides;
	int vis;
	vec3_t normal;
	float dist;

	for(i = 0; i < 2; i++){
		VectorClear(normal);
		normal[i] = 1;
		dist = clipmaxs[i];
		maxplanenums[i] = FindFloatPlane(normal, dist);
		dist = clipmins[i];
		minplanenums[i] = FindFloatPlane(normal, dist);
	}

	brushlist = NULL;
	c_faces = 0;
	c_brushes = 0;

	for(i = startbrush; i < endbrush; i++){
		mapbrush_t *mb = &mapbrushes[i];

		numsides = mb->numsides;
		if(!numsides)
			continue;
		// make sure the brush has at least one face showing
		vis = 0;
		for(j = 0; j < numsides; j++)
			if(mb->original_sides[j].visible && mb->original_sides[j].winding)
				vis++;
#if 0
		if(!vis)
			continue;				  // no faces at all
#endif
		// if the brush is outside the clip area, skip it
		for(j = 0; j < 3; j++)
			if(mb->mins[j] >= clipmaxs[j]
			        || mb->maxs[j] <= clipmins[j])
				break;
		if(j != 3)
			continue;

		//
		// make a copy of the brush
		//
		newbrush = AllocBrush(mb->numsides);
		newbrush->original = mb;
		newbrush->numsides = mb->numsides;
		memcpy(newbrush->sides, mb->original_sides, numsides * sizeof(side_t));
		for(j = 0; j < numsides; j++){
			if(newbrush->sides[j].winding)
				newbrush->sides[j].winding =
				    CopyWinding(newbrush->sides[j].winding);
			if(newbrush->sides[j].surf & SURF_HINT)
				newbrush->sides[j].visible = true;	// hints are always visible
		}
		VectorCopy(mb->mins, newbrush->mins);
		VectorCopy(mb->maxs, newbrush->maxs);

		//
		// carve off anything outside the clip box
		//
		newbrush = ClipBrushToBox(newbrush, clipmins, clipmaxs);
		if(!newbrush)
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
static bspbrush_t *AddBrushListToTail(bspbrush_t * list, bspbrush_t * tail){
	bspbrush_t *walk, *next;

	for(walk = list; walk; walk = next){	// add to end of list
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
static bspbrush_t *CullList(bspbrush_t * list, const bspbrush_t * skip1){
	bspbrush_t *newlist;
	bspbrush_t *next;

	newlist = NULL;

	for(; list; list = next){
		next = list->next;
		if(list == skip1){
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
static inline qboolean BrushGE(const bspbrush_t * b1, const bspbrush_t * b2){
	// detail brushes never bite structural brushes
	if((b1->original->contents & CONTENTS_DETAIL)
	        && !(b2->original->contents & CONTENTS_DETAIL))
		return false;
	if(b1->original->contents & CONTENTS_SOLID)
		return true;
	return false;
}

/*
 * ChopBrushes
 *
 * Carves any intersecting solid brushes into the minimum number
 * of non-intersecting brushes.
 */
bspbrush_t *ChopBrushes(bspbrush_t * head){
	bspbrush_t *b1, *b2, *next;
	bspbrush_t *tail;
	bspbrush_t *keep;
	bspbrush_t *sub, *sub2;
	int c1, c2;

	Com_Verbose("---- ChopBrushes ----\n");
	Com_Verbose("original brushes: %i\n", CountBrushList(head));

	keep = NULL;

newlist:
	// find tail
	if(!head)
		return NULL;
	for(tail = head; tail->next; tail = tail->next);

	for(b1 = head; b1; b1 = next){
		next = b1->next;
		for(b2 = b1->next; b2; b2 = b2->next){
			if(BrushesDisjoint(b1, b2))
				continue;

			sub = NULL;
			sub2 = NULL;
			c1 = 999999;
			c2 = 999999;

			if(BrushGE(b2, b1)){
				sub = SubtractBrush(b1, b2);
				if(sub == b1)
					continue;		  // didn't really intersect
				if(!sub){			 // b1 is swallowed by b2
					head = CullList(b1, b1);
					goto newlist;
				}
				c1 = CountBrushList(sub);
			}

			if(BrushGE(b1, b2)){
				sub2 = SubtractBrush(b2, b1);
				if(sub2 == b2)
					continue;		  // didn't really intersect
				if(!sub2){			 // b2 is swallowed by b1
					FreeBrushList(sub);
					head = CullList(b1, b2);
					goto newlist;
				}
				c2 = CountBrushList(sub2);
			}

			if(!sub && !sub2)
				continue;			  // neither one can bite

			// only accept if it didn't fragment
			// (commening this out allows full fragmentation)
			if(c1 > 1 && c2 > 1){
				if(sub2)
					FreeBrushList(sub2);
				if(sub)
					FreeBrushList(sub);
				continue;
			}

			if(c1 < c2){
				if(sub2)
					FreeBrushList(sub2);
				tail = AddBrushListToTail(sub, tail);
				head = CullList(b1, b1);
				goto newlist;
			} else {
				if(sub)
					FreeBrushList(sub);
				tail = AddBrushListToTail(sub2, tail);
				head = CullList(b1, b2);
				goto newlist;
			}
		}

		if(!b2){  // b1 is no longer intersecting anything, so keep it
			b1->next = keep;
			keep = b1;
		}
	}

	Com_Verbose("output brushes: %i\n", CountBrushList(keep));
	return keep;
}
