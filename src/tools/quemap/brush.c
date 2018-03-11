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

#include "qbsp.h"

/**
 * @brief Sets the mins/maxs based on the windings
 */
static void BoundBrush(brush_t *brush) {
	int32_t i, j;
	winding_t *w;

	ClearBounds(brush->mins, brush->maxs);
	for (i = 0; i < brush->num_sides; i++) {
		w = brush->sides[i].winding;
		if (!w) {
			continue;
		}
		for (j = 0; j < w->num_points; j++) {
			AddPointToBounds(w->points[j], brush->mins, brush->maxs);
		}
	}
}

/**
 * @brief
 */
static void CreateBrushWindings(brush_t *brush) {
	int32_t i, j;
	winding_t *w;
	side_t *side;
	map_plane_t *plane;

	for (i = 0; i < brush->num_sides; i++) {
		side = &brush->sides[i];
		plane = &map_planes[side->plane_num];
		w = WindingForPlane(plane->normal, plane->dist);
		for (j = 0; j < brush->num_sides && w; j++) {
			if (i == j) {
				continue;
			}
			if (brush->sides[j].bevel) {
				continue;
			}
			plane = &map_planes[brush->sides[j].plane_num ^ 1];
			ChopWindingInPlace(&w, plane->normal, plane->dist, 0); //CLIP_EPSILON);
		}

		side->winding = w;
	}

	BoundBrush(brush);
}

/**
 * @brief Creates a new axial brush
 */
static brush_t *BrushFromBounds(vec3_t mins, vec3_t maxs) {
	brush_t *b;
	int32_t i;
	vec3_t normal;
	vec_t dist;

	b = AllocBrush(6);
	b->num_sides = 6;
	for (i = 0; i < 3; i++) {
		VectorClear(normal);
		normal[i] = 1;
		dist = maxs[i];
		b->sides[i].plane_num = FindPlane(normal, dist);

		normal[i] = -1;
		dist = -mins[i];
		b->sides[3 + i].plane_num = FindPlane(normal, dist);
	}

	CreateBrushWindings(b);

	return b;
}

/**
 * @brief
 */
static vec_t BrushVolume(brush_t *brush) {
	int32_t i;
	winding_t *w;
	vec3_t corner;
	vec_t d, area, volume;
	map_plane_t *plane;

	if (!brush) {
		return 0;
	}

	// grab the first valid point as the corner

	w = NULL;
	for (i = 0; i < brush->num_sides; i++) {
		w = brush->sides[i].winding;
		if (w) {
			break;
		}
	}
	if (!w) {
		return 0;
	}
	VectorCopy(w->points[0], corner);

	// make tetrahedrons to all other faces

	volume = 0;
	for (; i < brush->num_sides; i++) {
		w = brush->sides[i].winding;
		if (!w) {
			continue;
		}
		plane = &map_planes[brush->sides[i].plane_num];
		d = -(DotProduct(corner, plane->normal) - plane->dist);
		area = WindingArea(w);
		volume += d * area;
	}

	volume /= 3;
	return volume;
}

/**
 * @brief
 */
int32_t CountBrushList(brush_t *brushes) {
	int32_t c;

	c = 0;
	for (; brushes; brushes = brushes->next) {
		c++;
	}
	return c;
}

/**
 * @brief
 */
tree_t *AllocTree(void) {
	tree_t *tree;

	tree = Mem_TagMalloc(sizeof(*tree), MEM_TAG_TREE);
	ClearBounds(tree->mins, tree->maxs);

	return tree;
}

/**
 * @brief
 */
node_t *AllocNode(void) {

	if (debug) {
		SDL_SemPost(semaphores.active_nodes);
	}

	return Mem_TagMalloc(sizeof(node_t), MEM_TAG_NODE);
}

/**
 * @brief
 */
void FreeNode(node_t *node) {

	if (debug) {
		SDL_SemWait(semaphores.active_nodes);
	}

	Mem_Free(node);
}

/**
 * @brief
 */
brush_t *AllocBrush(int32_t num_sides) {

	const size_t size = (size_t) & (((brush_t *) 0)->sides[num_sides]);

	if (debug) {
		SDL_SemPost(semaphores.active_brushes);
	}

	return Mem_TagMalloc(size, MEM_TAG_BRUSH);
}

/**
 * @brief
 */
void FreeBrush(brush_t *brush) {
	int32_t i;

	for (i = 0; i < brush->num_sides; i++)
		if (brush->sides[i].winding) {
			FreeWinding(brush->sides[i].winding);
		}

	if (debug) {
		SDL_SemWait(semaphores.active_brushes);
	}

	Mem_Free(brush);
}

/**
 * @brief
 */
void FreeBrushList(brush_t *brushes) {
	brush_t *next;

	for (; brushes; brushes = next) {
		next = brushes->next;

		FreeBrush(brushes);
	}
}

/**
 * @brief Duplicates the brush, the sides, and the windings
 */
brush_t *CopyBrush(brush_t *brush) {
	brush_t *newbrush;
	size_t size;
	int32_t i;

	size = (size_t) & (((brush_t *) 0)->sides[brush->num_sides]);

	newbrush = AllocBrush(brush->num_sides);
	memcpy(newbrush, brush, size);

	for (i = 0; i < brush->num_sides; i++) {
		if (brush->sides[i].winding) {
			newbrush->sides[i].winding = CopyWinding(brush->sides[i].winding);
		}
	}

	return newbrush;
}

/**
 * @brief Returns PSIDE_FRONT, PSIDE_BACK, or PSIDE_BOTH
 */
static int32_t Map_BoxOnPlaneSide(vec3_t mins, vec3_t maxs, map_plane_t *plane) {
	int32_t side;
	int32_t i;
	vec3_t corners[2];
	vec_t dist1, dist2;

	// axial planes are easy
	if (AXIAL(plane)) {
		side = 0;
		if (maxs[plane->type] > plane->dist + SIDE_EPSILON) {
			side |= SIDE_FRONT;
		}
		if (mins[plane->type] < plane->dist - SIDE_EPSILON) {
			side |= SIDE_BACK;
		}
		return side;
	}
	// create the proper leading and trailing verts for the box

	for (i = 0; i < 3; i++) {
		if (plane->normal[i] < 0) {
			corners[0][i] = mins[i];
			corners[1][i] = maxs[i];
		} else {
			corners[1][i] = mins[i];
			corners[0][i] = maxs[i];
		}
	}

	dist1 = DotProduct(plane->normal, corners[0]) - plane->dist;
	dist2 = DotProduct(plane->normal, corners[1]) - plane->dist;
	side = 0;
	if (dist1 >= SIDE_EPSILON) {
		side = SIDE_FRONT;
	}
	if (dist2 < SIDE_EPSILON) {
		side |= SIDE_BACK;
	}

	return side;
}

/**
 * @brief
 */
static int32_t TestBrushToPlanenum(brush_t *brush, int32_t plane_num, int32_t *numsplits,
                                   _Bool *hintsplit, int32_t *epsilonbrush) {
	int32_t i, j, num;
	map_plane_t *plane;
	int32_t s;
	winding_t *w;
	vec_t d, d_front, d_back;
	int32_t front, back;

	*numsplits = 0;
	*hintsplit = false;

	// if the brush actually uses the plane_num, we can tell the side for sure
	for (i = 0; i < brush->num_sides; i++) {
		num = brush->sides[i].plane_num;
		if (num >= MAX_BSP_PLANES) {
			Mon_SendSelect(MON_ERROR, brush->original->entity_num, brush->original->brush_num, "Bad plane");
		}
		if (num == plane_num) {
			return SIDE_BACK | SIDE_FACING;
		}
		if (num == (plane_num ^ 1)) {
			return SIDE_FRONT | SIDE_FACING;
		}
	}

	// box on plane side
	plane = &map_planes[plane_num];
	s = Map_BoxOnPlaneSide(brush->mins, brush->maxs, plane);

	if (s != SIDE_BOTH) {
		return s;
	}

	// if both sides, count the visible faces split
	d_front = d_back = 0;

	for (i = 0; i < brush->num_sides; i++) {
		if (brush->sides[i].texinfo == TEXINFO_NODE) {
			continue;    // on node, don't worry about splits
		}
		if (!brush->sides[i].visible) {
			continue;    // we don't care about non-visible
		}
		w = brush->sides[i].winding;
		if (!w) {
			continue;
		}
		front = back = 0;
		for (j = 0; j < w->num_points; j++) {
			d = DotProduct(w->points[j], plane->normal) - plane->dist;
			if (d > d_front) {
				d_front = d;
			}
			if (d < d_back) {
				d_back = d;
			}

			if (d > 0.1) { // PLANESIDE_EPSILON)
				front = 1;
			}
			if (d < -0.1) { // PLANESIDE_EPSILON)
				back = 1;
			}
		}
		if (front && back) {
			if (!(brush->sides[i].surf & SURF_SKIP)) {
				(*numsplits)++;
				if (brush->sides[i].surf & SURF_HINT) {
					*hintsplit = true;
				}
			}
		}
	}

	if ((d_front > 0.0 && d_front < 1.0) || (d_back < 0.0 && d_back > -1.0)) {
		(*epsilonbrush)++;
	}

	return s;
}

/**
 * @brief Returns true if the winding would be crunched out of
 * existance by the vertex snapping.
 */
#define	EDGE_LENGTH	0.2
_Bool WindingIsTiny(const winding_t *w) {
	int32_t i, j;
	vec_t len;
	vec3_t delta;
	int32_t edges;

	edges = 0;
	for (i = 0; i < w->num_points; i++) {
		j = i == w->num_points - 1 ? 0 : i + 1;
		VectorSubtract(w->points[j], w->points[i], delta);
		len = (vec_t) VectorLength(delta);
		if (len > EDGE_LENGTH) {
			if (++edges == 3) {
				return false;
			}
		}
	}
	return true;
}

/**
 * @brief Returns true if the winding still has one of the points
 * from basewinding for plane
 */
static _Bool WindingIsHuge(winding_t *w) {
	int32_t i, j;

	for (i = 0; i < w->num_points; i++) {
		for (j = 0; j < 3; j++)
			if (w->points[i][j] < -8000 || w->points[i][j] > 8000) {
				return true;
			}
	}
	return false;
}

/**
 * @brief
 */
static void LeafNode(node_t *node, brush_t *brushes) {
	brush_t *b;
	int32_t i;

	node->plane_num = PLANENUM_LEAF;
	node->contents = 0;

	for (b = brushes; b; b = b->next) {
		// if the brush is solid and all of its sides are on nodes,
		// it eats everything
		if (b->original->contents & CONTENTS_SOLID) {
			for (i = 0; i < b->num_sides; i++)
				if (b->sides[i].texinfo != TEXINFO_NODE) {
					break;
				}
			if (i == b->num_sides) {
				node->contents = CONTENTS_SOLID;
				break;
			}
		}
		node->contents |= b->original->contents;
	}

	node->brushes = brushes;
}

static void CheckPlaneAgainstParents(int32_t pnum, node_t *node) {
	node_t *p;

	for (p = node->parent; p; p = p->parent) {
		if (p->plane_num == pnum) {
			Com_Error(ERROR_FATAL, "Tried parent\n");
		}
	}
}

static _Bool CheckPlaneAgainstVolume(int32_t pnum, node_t *node) {
	brush_t *front, *back;
	_Bool good;

	SplitBrush(node->volume, pnum, &front, &back);

	good = (front && back);

	if (front) {
		FreeBrush(front);
	}
	if (back) {
		FreeBrush(back);
	}

	return good;
}

/**
 * @brief Using a heuristic, chooses one of the sides out of the brush list
 * to partition the brushes with.
 * Returns NULL if there are no valid planes to split with..
 */
static side_t *SelectSplitSide(brush_t *brushes, node_t *node) {
	int32_t value, bestvalue;
	brush_t *brush, *test;
	side_t *side, *bestside;
	int32_t i, j, pass, numpasses;
	int32_t pnum;
	int32_t s;
	int32_t front, back, both, facing, splits;
	int32_t bsplits;
	int32_t epsilonbrush;
	_Bool hintsplit;

	bestside = NULL;
	bestvalue = -99999;

	// the search order goes: visible-structural, visible-detail,
	// nonvisible-structural, nonvisible-detail.
	// If any valid plane is available in a pass, no further
	// passes will be tried.
	numpasses = 4;
	for (pass = 0; pass < numpasses; pass++) {
		for (brush = brushes; brush; brush = brush->next) {
			if ((pass & 1) && !(brush->original->contents & CONTENTS_DETAIL)) {
				continue;
			}
			if (!(pass & 1) && (brush->original->contents & CONTENTS_DETAIL)) {
				continue;
			}
			for (i = 0; i < brush->num_sides; i++) {
				side = brush->sides + i;
				if (side->bevel) {
					continue;    // never use a bevel as a splitter
				}
				if (!side->winding) {
					continue;    // nothing visible, so it can't split
				}
				if (side->texinfo == TEXINFO_NODE) {
					continue;    // already a node splitter
				}
				if (side->tested) {
					continue;    // we already have metrics for this plane
				}
				if (side->surf & SURF_SKIP) {
					continue;    // skip surfaces are never chosen
				}
				if (side->visible ^ (pass < 2)) {
					continue;    // only check visible faces on first pass
				}

				pnum = side->plane_num;
				pnum &= ~1; // always use positive facing plane

				CheckPlaneAgainstParents(pnum, node);

				if (!CheckPlaneAgainstVolume(pnum, node)) {
					continue;    // would produce a tiny volume
				}

				front = 0;
				back = 0;
				both = 0;
				facing = 0;
				splits = 0;
				epsilonbrush = 0;
				hintsplit = false;

				for (test = brushes; test; test = test->next) {
					s = TestBrushToPlanenum(test, pnum, &bsplits, &hintsplit, &epsilonbrush);

					splits += bsplits;
					if (bsplits && (s & SIDE_FACING)) {
						Com_Error(ERROR_FATAL, "SIDE_FACING with splits\n");
					}

					test->test_side = s;
					// if the brush shares this face, don't bother
					// testing that facenum as a splitter again
					if (s & SIDE_FACING) {
						facing++;
						for (j = 0; j < test->num_sides; j++) {
							if ((test->sides[j].plane_num & ~1) == pnum) {
								test->sides[j].tested = true;
							}
						}
					}
					if (s & SIDE_FRONT) {
						front++;
					}
					if (s & SIDE_BACK) {
						back++;
					}
					if (s == SIDE_BOTH) {
						both++;
					}
				}

				// give a value estimate for using this plane

				value = 5 * facing - 5 * splits - abs(front - back);
				if (AXIAL(&map_planes[pnum])) {
					value += 5;    // axial is better
				}
				value -= epsilonbrush * 1000; // avoid!

				// never split a hint side except with another hint
				if (hintsplit && !(side->surf & SURF_HINT)) {
					value = -9999999;
				}

				// save off the side test so we don't need
				// to recalculate it when we actually seperate
				// the brushes
				if (value > bestvalue) {
					bestvalue = value;
					bestside = side;
					for (test = brushes; test; test = test->next) {
						test->side = test->test_side;
					}
				}
			}
		}

		// if we found a good plane, don't bother trying any other passes
		if (bestside) {
			if (pass > 1) {
				if (debug) {
					SDL_SemPost(semaphores.nonvis_nodes);
				}
			}
			if (pass > 0) {
				node->detail_seperator = true;    // not needed for vis
			}
			break;
		}
	}

	// clear all the tested flags we set
	for (brush = brushes; brush; brush = brush->next) {
		for (i = 0; i < brush->num_sides; i++) {
			brush->sides[i].tested = false;
		}
	}

	return bestside;
}

/**
 * @brief
 */
static int32_t BrushMostlyOnSide(brush_t *brush, map_plane_t *plane) {
	int32_t i, j;
	winding_t *w;
	vec_t d, max;
	int32_t side;

	max = 0;
	side = SIDE_FRONT;
	for (i = 0; i < brush->num_sides; i++) {
		w = brush->sides[i].winding;
		if (!w) {
			continue;
		}
		for (j = 0; j < w->num_points; j++) {
			d = DotProduct(w->points[j], plane->normal) - plane->dist;
			if (d > max) {
				max = d;
				side = SIDE_FRONT;
			}
			if (-d > max) {
				max = -d;
				side = SIDE_BACK;
			}
		}
	}
	return side;
}

/**
 * @brief Generates two new brushes, leaving the original unchanged
 */
void SplitBrush(brush_t *brush, int32_t plane_num, brush_t **front, brush_t **back) {
	brush_t *b[2];
	int32_t i, j;
	winding_t *w, *cw[2], *midwinding;
	map_plane_t *plane, *plane2;
	side_t *s, *cs;
	vec_t d, d_front, d_back;

	*front = *back = NULL;
	plane = &map_planes[plane_num];

	// check all points
	d_front = d_back = 0;
	for (i = 0; i < brush->num_sides; i++) {
		w = brush->sides[i].winding;
		if (!w) {
			continue;
		}
		for (j = 0; j < w->num_points; j++) {
			d = DotProduct(w->points[j], plane->normal) - plane->dist;
			if (d > 0 && d > d_front) {
				d_front = d;
			}
			if (d < 0 && d < d_back) {
				d_back = d;
			}
		}
	}
	if (d_front < 0.1) { // PLANESIDE_EPSILON)
		// only on back
		*back = CopyBrush(brush);
		return;
	}
	if (d_back > -0.1) { // PLANESIDE_EPSILON)
		// only on front
		*front = CopyBrush(brush);
		return;
	}
	// create a new winding from the split plane

	w = WindingForPlane(plane->normal, plane->dist);
	for (i = 0; i < brush->num_sides && w; i++) {
		plane2 = &map_planes[brush->sides[i].plane_num ^ 1];
		ChopWindingInPlace(&w, plane2->normal, plane2->dist, 0); // PLANESIDE_EPSILON);
	}

	if (!w || WindingIsTiny(w)) { // the brush isn't really split
		int32_t side;

		side = BrushMostlyOnSide(brush, plane);
		if (side == SIDE_FRONT) {
			*front = CopyBrush(brush);
		}
		if (side == SIDE_BACK) {
			*back = CopyBrush(brush);
		}
		return;
	}

	if (WindingIsHuge(w)) {
		Mon_SendWinding(MON_WARN, (const vec3_t *) w->points, w->num_points, "Large winding");
	}

	midwinding = w;

	// split it for real

	for (i = 0; i < 2; i++) {
		b[i] = AllocBrush(brush->num_sides + 1);
		b[i]->original = brush->original;
	}

	// split all the current windings

	for (i = 0; i < brush->num_sides; i++) {
		s = &brush->sides[i];
		w = s->winding;
		if (!w) {
			continue;
		}
		ClipWindingEpsilon(w, plane->normal, plane->dist, 0 /*PLANESIDE_EPSILON */, &cw[0], &cw[1]);
		for (j = 0; j < 2; j++) {
			if (!cw[j]) {
				continue;
			}

			cs = &b[j]->sides[b[j]->num_sides];
			b[j]->num_sides++;
			*cs = *s;

			cs->winding = cw[j];
			cs->tested = false;
		}
	}

	// see if we have valid polygons on both sides
	for (i = 0; i < 2; i++) {
		BoundBrush(b[i]);
		for (j = 0; j < 3; j++) {
			if (b[i]->mins[j] < MIN_WORLD_COORD || b[i]->maxs[j] > MAX_WORLD_COORD) {
				Com_Debug(DEBUG_ALL, "bogus brush after clip\n");
				break;
			}
		}

		if (b[i]->num_sides < 3 || j < 3) {
			FreeBrush(b[i]);
			b[i] = NULL;
		}
	}

	if (!(b[0] && b[1])) {
		if (!b[0] && !b[1]) {
			Com_Debug(DEBUG_ALL, "split removed brush\n");
		} else {
			Com_Debug(DEBUG_ALL, "split not on both sides\n");
		}
		if (b[0]) {
			FreeBrush(b[0]);
			*front = CopyBrush(brush);
		}
		if (b[1]) {
			FreeBrush(b[1]);
			*back = CopyBrush(brush);
		}
		return;
	}

	// add the midwinding to both sides
	for (i = 0; i < 2; i++) {
		cs = &b[i]->sides[b[i]->num_sides];
		b[i]->num_sides++;

		cs->plane_num = plane_num ^ i ^ 1;
		cs->texinfo = TEXINFO_NODE;
		cs->visible = false;
		cs->tested = false;
		if (i == 0) {
			cs->winding = CopyWinding(midwinding);
		} else {
			cs->winding = midwinding;
		}
	}

	{
		vec_t v1;

		for (i = 0; i < 2; i++) {
			v1 = BrushVolume(b[i]);
			if (v1 < 1.0) {
				FreeBrush(b[i]);
				b[i] = NULL;
				Com_Debug(DEBUG_ALL, "tiny volume after clip\n");
			}
		}
	}

	*front = b[0];
	*back = b[1];
}

/**
 * @brief
 */
static void SplitBrushList(brush_t *brushes, node_t *node, brush_t **front,
                           brush_t **back) {
	brush_t *brush, *newbrush, *newbrush2;
	side_t *side;
	int32_t sides;
	int32_t i;

	*front = *back = NULL;

	for (brush = brushes; brush; brush = brush->next) {
		sides = brush->side;

		if (sides == SIDE_BOTH) { // split into two brushes
			SplitBrush(brush, node->plane_num, &newbrush, &newbrush2);
			if (newbrush) {
				newbrush->next = *front;
				*front = newbrush;
			}
			if (newbrush2) {
				newbrush2->next = *back;
				*back = newbrush2;
			}
			continue;
		}

		newbrush = CopyBrush(brush);

		// if the plane_num is actualy a part of the brush
		// find the plane and flag it as used so it won't be tried
		// as a splitter again
		if (sides & SIDE_FACING) {
			for (i = 0; i < newbrush->num_sides; i++) {
				side = newbrush->sides + i;
				if ((side->plane_num & ~1) == node->plane_num) {
					side->texinfo = TEXINFO_NODE;
				}
			}
		}

		if (sides & SIDE_FRONT) {
			newbrush->next = *front;
			*front = newbrush;
			continue;
		}
		if (sides & SIDE_BACK) {
			newbrush->next = *back;
			*back = newbrush;
			continue;
		}
	}
}

/*
 * ================
 * BuildTree_r
 * ================
 */
static node_t *BuildTree_r(node_t *node, brush_t *brushes) {
	node_t *newnode;
	side_t *bestside;
	int32_t i;
	brush_t *children[2];

	if (debug) {
		SDL_SemPost(semaphores.vis_nodes);
	}

	// find the best plane to use as a splitter
	bestside = SelectSplitSide(brushes, node);
	if (!bestside) {
		// leaf node
		node->side = NULL;
		node->plane_num = -1;
		LeafNode(node, brushes);
		return node;
	}
	// this is a splitplane node
	node->side = bestside;
	node->plane_num = bestside->plane_num & ~1; // always use front facing

	SplitBrushList(brushes, node, &children[0], &children[1]);
	FreeBrushList(brushes);

	// allocate children before recursing
	for (i = 0; i < 2; i++) {
		newnode = AllocNode();
		newnode->parent = node;
		node->children[i] = newnode;
	}

	SplitBrush(node->volume, node->plane_num, &node->children[0]->volume,
	           &node->children[1]->volume);

	// recursively process children
	for (i = 0; i < 2; i++) {
		node->children[i] = BuildTree_r(node->children[i], children[i]);
	}

	return node;
}

//===========================================================

/*
 * =================
 * BrushBSP
 *
 * The incoming list will be freed before exiting
 * =================
 */
tree_t *BrushBSP(brush_t *brushlist, vec3_t mins, vec3_t maxs) {
	node_t *node;
	brush_t *b;
	int32_t c_faces, c_nonvisfaces;
	int32_t c_brushes;
	tree_t *tree;
	int32_t i;
	vec_t volume;

	Com_Debug(DEBUG_ALL, "--- BrushBSP ---\n");

	tree = AllocTree();

	c_faces = 0;
	c_nonvisfaces = 0;
	c_brushes = 0;
	for (b = brushlist; b; b = b->next) {
		c_brushes++;

		volume = BrushVolume(b);
		if (volume < micro_volume) {
			Mon_SendSelect(MON_WARN, b->original->entity_num, b->original->brush_num, "Microbrush");
		}

		for (i = 0; i < b->num_sides; i++) {
			if (b->sides[i].bevel) {
				continue;
			}
			if (!b->sides[i].winding) {
				continue;
			}
			if (b->sides[i].texinfo == TEXINFO_NODE) {
				continue;
			}
			if (b->sides[i].visible) {
				c_faces++;
			} else {
				c_nonvisfaces++;
			}
		}

		AddPointToBounds(b->mins, tree->mins, tree->maxs);
		AddPointToBounds(b->maxs, tree->mins, tree->maxs);
	}

	Com_Debug(DEBUG_ALL, "%5i brushes\n", c_brushes);
	Com_Debug(DEBUG_ALL, "%5i visible faces\n", c_faces);
	Com_Debug(DEBUG_ALL, "%5i nonvisible faces\n", c_nonvisfaces);

	SDL_DestroySemaphore(semaphores.vis_nodes);
	semaphores.vis_nodes = SDL_CreateSemaphore(0);

	SDL_DestroySemaphore(semaphores.nonvis_nodes);
	semaphores.nonvis_nodes = SDL_CreateSemaphore(0);

	node = AllocNode();

	node->volume = BrushFromBounds(mins, maxs);

	tree->head_node = node;

	node = BuildTree_r(node, brushlist);

	const uint32_t vis_nodes = SDL_SemValue(semaphores.vis_nodes);
	const uint32_t nonvis_nodes = SDL_SemValue(semaphores.nonvis_nodes);

	Com_Debug(DEBUG_ALL, "%5i visible nodes\n", vis_nodes / 2 - nonvis_nodes);
	Com_Debug(DEBUG_ALL, "%5i nonvis nodes\n", nonvis_nodes);
	Com_Debug(DEBUG_ALL, "%5i leafs\n", (vis_nodes + 1) / 2);

	return tree;
}
