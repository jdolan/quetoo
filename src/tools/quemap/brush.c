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
csg_brush_t *AllocBrush(int32_t num_brush_sides) {

	csg_brush_t *brush = Mem_TagMalloc(sizeof(csg_brush_t), MEM_TAG_BRUSH);

	brush->brush_sides = Mem_LinkMalloc(sizeof(brush_side_t) * num_brush_sides, brush);

	SDL_AtomicAdd(&c_active_brushes, 1);

	return brush;
}

/**
 * @brief
 */
void FreeBrush(csg_brush_t *brush) {

	assert(brush);

	SDL_AtomicAdd(&c_active_brushes, -1);
	
	for (int32_t i = 0; i < brush->num_brush_sides; i++) {
		if (brush->brush_sides[i].winding) {
			Cm_FreeWinding(brush->brush_sides[i].winding);
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
size_t CountBrushes(const csg_brush_t *brushes) {

	size_t i = 0;
	for (; brushes; brushes = brushes->next) {
		i++;
	}
	return i;
}

/**
 * @brief Duplicates the brush, the sides, and the windings.
 */
csg_brush_t *CopyBrush(const csg_brush_t *brush) {

	csg_brush_t *copy = AllocBrush(brush->num_brush_sides);

	copy->bounds = brush->bounds;
	copy->original = brush->original;
	copy->num_brush_sides = brush->num_brush_sides;

	memcpy(copy->brush_sides, brush->brush_sides, sizeof(brush_side_t) * brush->num_brush_sides);

	for (int32_t i = 0; i < brush->num_brush_sides; i++) {
		if (brush->brush_sides[i].winding) {
			copy->brush_sides[i].winding = Cm_CopyWinding(brush->brush_sides[i].winding);
		}
	}

	return copy;
}

/**
 * @brief Sets the mins/maxs based on the windings
 */
static void SetBrushBounds(csg_brush_t *brush) {

	brush->bounds = Box3_Null();

	for (int32_t i = 0; i < brush->num_brush_sides; i++) {
		const cm_winding_t *w = brush->brush_sides[i].winding;
		if (w) {
			brush->bounds = Box3_Union(brush->bounds, Cm_WindingBounds(w));
		}
	}
}

/**
 * @brief
 */
static void MakeBrushWindings(csg_brush_t *brush) {

	brush_side_t *side = brush->brush_sides;
	for (int32_t i = 0; i < brush->num_brush_sides; i++, side++) {

		const plane_t *plane = &planes[side->plane];
		side->winding = Cm_WindingForPlane(plane->normal, plane->dist);

		const brush_side_t *s = brush->brush_sides;
		for (int32_t j = 0; j < brush->num_brush_sides; j++, s++) {
			if (side == s) {
				continue;
			}
			const plane_t *p = &planes[s->plane ^ 1];
			Cm_ClipWinding(&side->winding, p->normal, p->dist, SIDE_EPSILON);
		}

		assert(side->winding);
	}

	SetBrushBounds(brush);
}

/**
 * @brief Creates a new axial brush
 */
csg_brush_t *BrushFromBounds(const box3_t bounds) {

	csg_brush_t *b = AllocBrush(6);
	b->num_brush_sides = 6;

	for (int32_t i = 0; i < 3; i++) {
		vec3_t normal = Vec3_Zero();

		normal.xyz[i] = 1.f;
		b->brush_sides[i].plane = FindPlane(normal, bounds.maxs.xyz[i]);

		normal.xyz[i] = -1.f;
		b->brush_sides[3 + i].plane = FindPlane(normal, -bounds.mins.xyz[i]);
	}

	MakeBrushWindings(b);
	return b;
}

/**
 * @brief
 */
float BrushVolume(csg_brush_t *brush) {
	int32_t i;
	vec3_t corner;

	if (!brush) {
		return 0;
	}

	// grab the first valid point as the corner

	cm_winding_t *w = NULL;
	for (i = 0; i < brush->num_brush_sides; i++) {
		w = brush->brush_sides[i].winding;
		if (w) {
			break;
		}
	}
	if (!w) {
		return 0;
	}

	corner = w->points[0];

	// make tetrahedrons to all other faces

	float volume = 0.0;
	for (; i < brush->num_brush_sides; i++) {
		w = brush->brush_sides[i].winding;
		if (!w) {
			continue;
		}
		plane_t *plane = &planes[brush->brush_sides[i].plane];
		const float d = -(Vec3_Dot(corner, plane->normal) - plane->dist);
		const float area = Cm_WindingArea(w);
		volume += d * area;
	}

	volume /= 3;
	return volume;
}

/**
 * @brief
 */
int32_t BrushOnPlaneSide(const csg_brush_t *brush, int32_t plane) {

	// if the brush actually uses the plane, we can tell the side for sure
	for (int32_t i = 0; i < brush->num_brush_sides; i++) {
		const int32_t p = brush->brush_sides[i].plane;
		if (p == plane) {
			return SIDE_BACK | SIDE_ON;
		}
		if (p == (plane ^ 1)) {
			return SIDE_FRONT | SIDE_ON;
		}
	}

	const cm_bsp_plane_t tmp = Cm_Plane(planes[plane].normal, planes[plane].dist);

	return Cm_BoxOnPlaneSide(brush->bounds, &tmp);
}

/**
 * @brief
 */
int32_t BrushOnPlaneSideSplits(const csg_brush_t *brush, int32_t plane, int32_t *num_split_sides) {

	*num_split_sides = 0;

	const int32_t s = BrushOnPlaneSide(brush, plane);
	if (s == SIDE_BOTH) {

		const plane_t *p = planes + plane;
		const brush_side_t *side = brush->brush_sides;
		for (int32_t i = 0; i < brush->num_brush_sides; i++, side++) {

			if (side->surface & SURF_BEVEL) {
				continue;
			}
			if (side->surface & SURF_NODE) {
				continue;
			}

			int32_t front = 0, back = 0;
			const cm_winding_t *w = side->winding;

			for (int32_t j = 0; j < w->num_points; j++) {
				const double d = Vec3_Dot(w->points[j], p->normal) - p->dist;
				
				if (d > SIDE_EPSILON) {
					front++;
				}
				if (d < -SIDE_EPSILON) {
					back++;
				}
			}

			if (front && back) {
				(*num_split_sides)++;
			}
		}
	}

	return s;
}

/**
 * @brief
 */
static int32_t BrushMostlyOnSide(const csg_brush_t *brush, const plane_t *plane) {

	double max = 0.0;
	int32_t side = SIDE_FRONT;

	for (int32_t i = 0; i < brush->num_brush_sides; i++) {
		cm_winding_t *w = brush->brush_sides[i].winding;
		if (!w) {
			continue;
		}
		for (int32_t j = 0; j < w->num_points; j++) {
			const double d = Vec3_Dot(w->points[j], plane->normal) - plane->dist;
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
void SplitBrush(const csg_brush_t *brush, int32_t plane, csg_brush_t **front, csg_brush_t **back) {

	csg_brush_t *cb[2];
	cm_winding_t *cw[2];

	*front = *back = NULL;
	const plane_t *split = &planes[plane];

	// check all points
	double d_front = 0.0, d_back = 0.0;
	for (int32_t i = 0; i < brush->num_brush_sides; i++) {
		cm_winding_t *w = brush->brush_sides[i].winding;
		if (!w) {
			continue;
		}
		for (int32_t j = 0; j < w->num_points; j++) {
			const double d = Vec3_Dot(w->points[j], split->normal) - split->dist;
			if (d > d_front) {
				d_front = d;
			}
			if (d < d_back) {
				d_back = d;
			}
		}
	}
	if (d_front < ON_EPSILON) {
		*back = CopyBrush(brush);
		return;
	}
	if (d_back > -ON_EPSILON) {
		*front = CopyBrush(brush);
		return;
	}

	// create a new winding from the split plane

	cm_winding_t *w = Cm_WindingForPlane(split->normal, split->dist);

	for (int32_t i = 0; i < brush->num_brush_sides && w; i++) {
		const plane_t *p = &planes[brush->brush_sides[i].plane ^ 1];
		Cm_ClipWinding(&w, p->normal, p->dist, SIDE_EPSILON);
	}

	if (!w || WindingIsSmall(w)) { // the brush isn't really split

		const int32_t side = BrushMostlyOnSide(brush, split);
		if (side == SIDE_FRONT) {
			*front = CopyBrush(brush);
		}
		if (side == SIDE_BACK) {
			*back = CopyBrush(brush);
		}
		return;
	}

	if (WindingIsLarge(w)) {
		Mon_SendWinding(MON_WARN, (const vec3_t *) w->points, w->num_points, "Large winding");
	}

	cm_winding_t *mid_winding = w;

	// split it for real

	for (int32_t i = 0; i < 2; i++) {
		cb[i] = AllocBrush(brush->num_brush_sides + 1);
		cb[i]->original = brush->original;
	}

	// split all the current windings

	for (int32_t i = 0; i < brush->num_brush_sides; i++) {
		brush_side_t *s = &brush->brush_sides[i];
		cm_winding_t *w = s->winding;
		if (!w) {
			continue;
		}
		Cm_SplitWinding(w, split->normal, split->dist, SIDE_EPSILON, &cw[0], &cw[1]);
		for (int32_t j = 0; j < 2; j++) {
			if (!cw[j]) {
				continue;
			}

			brush_side_t *cs = &cb[j]->brush_sides[cb[j]->num_brush_sides];
			cb[j]->num_brush_sides++;
			*cs = *s;

			cs->winding = cw[j];
		}
	}

	// see if we have valid polygons on both sides
	for (int32_t i = 0; i < 2; i++) {
		SetBrushBounds(cb[i]);
		int32_t j;
		for (j = 0; j < 3; j++) {
			if (cb[i]->bounds.mins.xyz[j] < MIN_WORLD_COORD || cb[i]->bounds.maxs.xyz[j] > MAX_WORLD_COORD) {
				Com_Debug(DEBUG_ALL, "Invalid brush after clip\n");
				break;
			}
		}

		if (cb[i]->num_brush_sides < 3 || j < 3) {
			FreeBrush(cb[i]);
			cb[i] = NULL;
		}
	}

	if (!(cb[0] && cb[1])) {
		if (!cb[0] && !cb[1]) {
			Com_Debug(DEBUG_ALL, "Split removed brush\n");
		} else {
			Com_Debug(DEBUG_ALL, "Split not on both sides\n");
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
		brush_side_t *cs = &cb[i]->brush_sides[cb[i]->num_brush_sides];
		cb[i]->num_brush_sides++;

		cs->plane = plane ^ i ^ 1;
		cs->material = BSP_MATERIAL_NODE;
		cs->surface = SURF_NODE;

		if (i == 0) {
			cs->winding = Cm_CopyWinding(mid_winding);
		} else {
			cs->winding = mid_winding;
		}
	}

	for (int32_t i = 0; i < 2; i++) {
		const float v1 = BrushVolume(cb[i]);
		if (v1 < 1.0) {
			FreeBrush(cb[i]);
			cb[i] = NULL;
			Com_Debug(DEBUG_ALL, "Tiny volume after clip\n");
		}
	}

	*front = cb[0];
	*back = cb[1];
}
