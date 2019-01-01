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

static SDL_atomic_t c_active_brushes;

/**
 * @brief
 */
csg_brush_t *AllocBrush(int32_t num_sides) {

	csg_brush_t *brush = Mem_TagMalloc(sizeof(csg_brush_t), MEM_TAG_BRUSH);
	brush->sides = Mem_LinkMalloc(sizeof(brush_side_t) * num_sides, brush);

	SDL_AtomicAdd(&c_active_brushes, 1);

	return brush;
}

/**
 * @brief
 */
void FreeBrush(csg_brush_t *brush) {

	SDL_AtomicAdd(&c_active_brushes, -1);
	
	for (int32_t i = 0; i < brush->num_sides; i++) {
		if (brush->sides[i].winding) {
			FreeWinding(brush->sides[i].winding);
		}
	}

	Mem_Free(brush);
}

/**
 * @brief
 */
void FreeBrushes(csg_brush_t *brushes) {
	csg_brush_t *next;

	for (; brushes; brushes = next) {
		next = brushes->next;
		FreeBrush(brushes);
	}
}

/**
 * @brief
 */
size_t CountBrushes(csg_brush_t *brushes) {

	size_t i = 0;
	for (; brushes; brushes = brushes->next) {
		i++;
	}
	return i;
}

/**
 * @brief Duplicates the brush, the sides, and the windings
 */
csg_brush_t *CopyBrush(const csg_brush_t *brush) {

	csg_brush_t *copy = AllocBrush(brush->num_sides);

	brush_side_t *sides = copy->sides;
	memcpy(copy, brush, sizeof(csg_brush_t));

	copy->sides = sides;
	memcpy(copy->sides, brush->sides, sizeof(brush_side_t) * copy->num_sides);

	for (int32_t i = 0; i < copy->num_sides; i++) {
		if (copy->sides[i].winding) {
			copy->sides[i].winding = CopyWinding(copy->sides[i].winding);
		}
	}

	return copy;
}

/**
 * @brief Sets the mins/maxs based on the windings
 */
static void SetBrushBounds(csg_brush_t *brush) {

	ClearBounds(brush->mins, brush->maxs);
	for (int32_t i = 0; i < brush->num_sides; i++) {
		const winding_t *w = brush->sides[i].winding;
		if (!w) {
			continue;
		}
		for (int32_t j = 0; j < w->num_points; j++) {
			AddPointToBounds(w->points[j], brush->mins, brush->maxs);
		}
	}
}

/**
 * @brief
 */
static void CreateBrushWindings(csg_brush_t *brush) {

	for (int32_t i = 0; i < brush->num_sides; i++) {
		brush_side_t *side = &brush->sides[i];
		const plane_t *plane = &planes[side->plane_num];
		winding_t *w = WindingForPlane(plane->normal, plane->dist);
		for (int32_t j = 0; j < brush->num_sides; j++) {
			if (!w) {
				break;
			}
			if (i == j) {
				continue;
			}
			if (brush->sides[j].bevel) {
				continue;
			}
			plane = &planes[brush->sides[j].plane_num ^ 1];
			ChopWindingInPlace(&w, plane->normal, plane->dist, 0); //CLIP_EPSILON);
		}

		side->winding = w;
	}

	SetBrushBounds(brush);
}

/**
 * @brief Creates a new axial brush
 */
csg_brush_t *BrushFromBounds(const vec3_t mins, const vec3_t maxs) {

	csg_brush_t *b = AllocBrush(6);
	b->num_sides = 6;

	for (int32_t i = 0; i < 3; i++) {
		vec3_t normal;
		VectorClear(normal);

		normal[i] = 1;
		b->sides[i].plane_num = FindPlane(normal, maxs[i]);

		normal[i] = -1;
		b->sides[3 + i].plane_num = FindPlane(normal, -mins[i]);
	}

	CreateBrushWindings(b);
	return b;
}

/**
 * @brief
 */
vec_t BrushVolume(csg_brush_t *brush) {
	int32_t i;
	vec3_t corner;

	if (!brush) {
		return 0;
	}

	// grab the first valid point as the corner

	winding_t *w = NULL;
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

	vec_t volume = 0.0;
	for (; i < brush->num_sides; i++) {
		w = brush->sides[i].winding;
		if (!w) {
			continue;
		}
		plane_t *plane = &planes[brush->sides[i].plane_num];
		const vec_t d = -(DotProduct(corner, plane->normal) - plane->dist);
		const vec_t area = WindingArea(w);
		volume += d * area;
	}

	volume /= 3;
	return volume;
}

/**
 * @return SIDE_FRONT, SIDE_BACK, or SIDE_BOTH
 */
static int32_t BoxOnPlaneSide(vec3_t mins, vec3_t maxs, plane_t *plane) {
	int32_t side = 0;

	// axial planes are easy
	if (AXIAL(plane)) {
		if (maxs[plane->type] > plane->dist + SIDE_EPSILON) {
			side |= SIDE_FRONT;
		}
		if (mins[plane->type] < plane->dist - SIDE_EPSILON) {
			side |= SIDE_BACK;
		}
		return side;
	}
	// create the proper leading and trailing verts for the box

	vec3_t corners[2];
	for (int32_t i = 0; i < 3; i++) {
		if (plane->normal[i] < 0) {
			corners[0][i] = mins[i];
			corners[1][i] = maxs[i];
		} else {
			corners[1][i] = mins[i];
			corners[0][i] = maxs[i];
		}
	}

	const vec_t dist1 = DotProduct(plane->normal, corners[0]) - plane->dist;
	const vec_t dist2 = DotProduct(plane->normal, corners[1]) - plane->dist;

	if (dist1 >= SIDE_EPSILON) {
		side |= SIDE_FRONT;
	}
	if (dist2 < SIDE_EPSILON) {
		side |= SIDE_BACK;
	}

	return side;
}

/**
 * @brief
 */
int32_t TestBrushToPlane(csg_brush_t *brush, int32_t plane_num, int32_t *num_splits,
                                   _Bool *hint_split, int32_t *epsilon_brush) {

	*num_splits = 0;
	*hint_split = false;

	// if the brush actually uses the plane_num, we can tell the side for sure
	for (int32_t i = 0; i < brush->num_sides; i++) {
		const int32_t num = brush->sides[i].plane_num;
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
	plane_t *plane = &planes[plane_num];
	const int32_t s = BoxOnPlaneSide(brush->mins, brush->maxs, plane);

	if (s != SIDE_BOTH) {
		return s;
	}

	// if both sides, count the visible faces split
	vec_t d_front = 0.0, d_back = 0.0;

	for (int32_t i = 0; i < brush->num_sides; i++) {
		if (brush->sides[i].texinfo == TEXINFO_NODE) {
			continue;    // on node, don't worry about splits
		}
		if (!brush->sides[i].visible) {
			continue;    // we don't care about non-visible
		}
		const winding_t *w = brush->sides[i].winding;
		if (!w) {
			continue;
		}

		int32_t front = 0, back = 0;

		for (int32_t j = 0; j < w->num_points; j++) {
			const vec_t d = DotProduct(w->points[j], plane->normal) - plane->dist;
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
				(*num_splits)++;
				if (brush->sides[i].surf & SURF_HINT) {
					*hint_split = true;
				}
			}
		}
	}

	if ((d_front > 0.0 && d_front < 1.0) || (d_back < 0.0 && d_back > -1.0)) {
		(*epsilon_brush)++;
	}

	return s;
}

/**
 * @brief
 */
static int32_t BrushMostlyOnSide(csg_brush_t *brush, plane_t *plane) {

	vec_t max = 0.0;
	int32_t side = SIDE_FRONT;

	for (int32_t i = 0; i < brush->num_sides; i++) {
		winding_t *w = brush->sides[i].winding;
		if (!w) {
			continue;
		}
		for (int32_t j = 0; j < w->num_points; j++) {
			const vec_t d = DotProduct(w->points[j], plane->normal) - plane->dist;
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
 * @brief Generates two new brushes, leaving the original unchanged.
 */
void SplitBrush(csg_brush_t *brush, int32_t plane_num, csg_brush_t **front, csg_brush_t **back) {

	csg_brush_t *cb[2];
	winding_t *cw[2];

	*front = *back = NULL;
	plane_t *plane = &planes[plane_num];

	// check all points
	vec_t d_front = 0.0, d_back = 0.0;
	for (int32_t i = 0; i < brush->num_sides; i++) {
		winding_t *w = brush->sides[i].winding;
		if (!w) {
			continue;
		}
		for (int32_t j = 0; j < w->num_points; j++) {
			const vec_t d = DotProduct(w->points[j], plane->normal) - plane->dist;
			if (d > 0.0 && d > d_front) {
				d_front = d;
			}
			if (d < 0.0 && d < d_back) {
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

	winding_t *w = WindingForPlane(plane->normal, plane->dist);

	for (int32_t i = 0; i < brush->num_sides && w; i++) {
		plane_t *plane2 = &planes[brush->sides[i].plane_num ^ 1];
		ChopWindingInPlace(&w, plane2->normal, plane2->dist, 0); // PLANESIDE_EPSILON);
	}

	if (!w || WindingIsTiny(w)) { // the brush isn't really split

		const int32_t side = BrushMostlyOnSide(brush, plane);
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

	winding_t *mid_winding = w;

	// split it for real

	for (int32_t i = 0; i < 2; i++) {
		cb[i] = AllocBrush(brush->num_sides + 1);
		cb[i]->original = brush->original;
	}

	// split all the current windings

	for (int32_t i = 0; i < brush->num_sides; i++) {
		brush_side_t *s = &brush->sides[i];
		winding_t *w = s->winding;
		if (!w) {
			continue;
		}
		ClipWindingEpsilon(w, plane->normal, plane->dist, 0 /*PLANESIDE_EPSILON */, &cw[0], &cw[1]);
		for (int32_t j = 0; j < 2; j++) {
			if (!cw[j]) {
				continue;
			}

			brush_side_t *cs = &cb[j]->sides[cb[j]->num_sides];
			cb[j]->num_sides++;
			*cs = *s;

			cs->winding = cw[j];
			cs->tested = false;
		}
	}

	// see if we have valid polygons on both sides
	for (int32_t i = 0; i < 2; i++) {
		SetBrushBounds(cb[i]);
		int32_t j;
		for (j = 0; j < 3; j++) {
			if (cb[i]->mins[j] < MIN_WORLD_COORD || cb[i]->maxs[j] > MAX_WORLD_COORD) {
				Com_Debug(DEBUG_ALL, "bogus brush after clip\n");
				break;
			}
		}

		if (cb[i]->num_sides < 3 || j < 3) {
			FreeBrush(cb[i]);
			cb[i] = NULL;
		}
	}

	if (!(cb[0] && cb[1])) {
		if (!cb[0] && !cb[1]) {
			Com_Debug(DEBUG_ALL, "split removed brush\n");
		} else {
			Com_Debug(DEBUG_ALL, "split not on both sides\n");
		}
		if (cb[0]) {
			FreeBrush(cb[0]);
			*front = CopyBrush(brush);
		}
		if (cb[1]) {
			FreeBrush(cb[1]);
			*back = CopyBrush(brush);
		}
		return;
	}

	// add the mid winding to both sides
	for (int32_t i = 0; i < 2; i++) {
		brush_side_t *cs = &cb[i]->sides[cb[i]->num_sides];
		cb[i]->num_sides++;

		cs->plane_num = plane_num ^ i ^ 1;
		cs->texinfo = TEXINFO_NODE;
		cs->visible = false;
		cs->tested = false;
		if (i == 0) {
			cs->winding = CopyWinding(mid_winding);
		} else {
			cs->winding = mid_winding;
		}
	}

	for (int32_t i = 0; i < 2; i++) {
		const vec_t v1 = BrushVolume(cb[i]);
		if (v1 < 1.0) {
			FreeBrush(cb[i]);
			cb[i] = NULL;
			Com_Debug(DEBUG_ALL, "tiny volume after clip\n");
		}
	}

	*front = cb[0];
	*back = cb[1];
}
