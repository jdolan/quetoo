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

#include <SDL_atomic.h>

#include "cm_local.h"

static SDL_atomic_t c_windings;

/**
 * @brief
 */
cm_winding_t *Cm_AllocWinding(int32_t num_points) {

	SDL_AtomicAdd(&c_windings, 1);

	return Mem_TagMalloc(sizeof(int32_t) + sizeof(vec3_t) * num_points, MEM_TAG_POLYLIB);
}

/**
 * @brief
 */
void Cm_FreeWinding(cm_winding_t *w) {

	SDL_AtomicAdd(&c_windings, -1);

	Mem_Free(w);
}

/**
 * @brief
 */
cm_winding_t *Cm_CopyWinding(const cm_winding_t *w) {

	cm_winding_t *c = Cm_AllocWinding(w->num_points);

	c->num_points = w->num_points;

	memcpy(c->points, w->points, c->num_points * sizeof(vec3_t));

	return c;
}

/**
 * @brief
 */
cm_winding_t *Cm_ReverseWinding(const cm_winding_t *w) {

	cm_winding_t *c = Cm_AllocWinding(w->num_points);

	for (int32_t i = 0; i < w->num_points; i++) {
		c->points[i] = w->points[w->num_points - 1 - i];
	}

	c->num_points = w->num_points;
	return c;
}

/**
 * @brief
 */
box3_t Cm_WindingBounds(const cm_winding_t *w) {

	return Box3_FromPoints(w->points, w->num_points);
}

/**
 * @brief
 */
vec3_t Cm_WindingCenter(const cm_winding_t *w) {

	vec3_t center = Vec3_Zero();

	for (int32_t i = 0; i < w->num_points; i++) {
		center = Vec3_Add(w->points[i], center);
	}

	return Vec3_Scale(center, 1.0 / w->num_points);
}

/**
 * @brief
 */
float Cm_WindingArea(const cm_winding_t *w) {
	float area = 0.0;

	for (int32_t i = 2; i < w->num_points; i++) {
		area += Cm_TriangleArea(w->points[0], w->points[i - 1], w->points[i]);
	}

	return area;
}

/**
 * @brief Calculates the distance from `p` to `w`.
 * @see https://stackoverflow.com/questions/849211/shortest-distance-between-a-point-and-a-line-segment
 */
float Cm_DistanceToWinding(const cm_winding_t *w, const vec3_t p, vec3_t *dir) {

	float distance = FLT_MAX;

	for (int32_t i = 0; i < w->num_points; i++) {

		const vec3_t a = w->points[(i + 0) % w->num_points];
		const vec3_t b = w->points[(i + 1) % w->num_points];

		const float dist_squared = Vec3_DistanceSquared(a, b);
		if (dist_squared == 0.f) {
			continue;
		}

		const vec3_t pa = Vec3_Subtract(p, a);
		const vec3_t ba = Vec3_Subtract(b, a);

		const float f = Maxf(0.f, Minf(1.f, Vec3_Dot(pa, ba) / dist_squared));
		const vec3_t q = Vec3_Fmaf(a, f, ba);

		vec3_t dir0;
		const float dist = Vec3_DistanceDir(q, p, &dir0);

		if (dist < distance) {
			distance = dist;
			if (dir) {
				*dir = dir0;
			}
		}
	}

	return distance;
}

/**
 * @brief
 */
void Cm_PlaneForWinding(const cm_winding_t *w, vec3_t *normal, double *dist) {

	const vec3d_t a = Vec3_CastVec3d(w->points[0]);
	const vec3d_t b = Vec3_CastVec3d(w->points[1]);
	const vec3d_t c = Vec3_CastVec3d(w->points[2]);

	const vec3d_t ba = Vec3d_Subtract(b, a);
	const vec3d_t ca = Vec3d_Subtract(c, a);

	const vec3d_t n = Vec3d_Normalize(Vec3d_Cross(ca, ba));

	*normal = Vec3d_CastVec3(n);
	*dist = Vec3d_Dot(a, n);
}

/**
 * @brief Create a massive polygon for the specified plane.
 */
cm_winding_t *Cm_WindingForPlane(const vec3_t normal, double dist) {
	vec3_t org, right, up;

	// find the major axis
	float max = 0.0;
	int32_t x = -1;
	for (int32_t i = 0; i < 3; i++) {
		const float v = fabsf(normal.xyz[i]);
		if (v > max) {
			x = i;
			max = v;
		}
	}
	if (x == -1) {
		Com_Error(ERROR_FATAL, "No axis found\n");
	}

	switch (x) {
		case 0:
		case 1:
			right = Vec3(-normal.y, normal.x, 0.0);
			break;
		case 2:
			right = Vec3(0.0, -normal.z, normal.y);
			break;
		default:
			Com_Error(ERROR_FATAL, "Bad axis\n");
	}

	right = Vec3_Scale(right, MAX_WORLD_DIST);

	up = Vec3_Cross(normal, right);

	org = Vec3_Fmaf(Vec3_Zero(), dist, normal);

	// project a really big	axis aligned box onto the plane
	cm_winding_t *w = Cm_AllocWinding(4);

	w->points[0] = Vec3_Subtract(org, right);
	w->points[0] = Vec3_Add(w->points[0], up);

	w->points[1] = Vec3_Add(org, right);
	w->points[1] = Vec3_Add(w->points[1], up);

	w->points[2] = Vec3_Add(org, right);
	w->points[2] = Vec3_Subtract(w->points[2], up);

	w->points[3] = Vec3_Subtract(org, right);
	w->points[3] = Vec3_Subtract(w->points[3], up);

	w->num_points = 4;

	return w;
}

/**
 * @brief Creates a winding for the given face, removing any collinear points.
 */
cm_winding_t *Cm_WindingForFace(const bsp_file_t *file, const bsp_face_t *face) {

	cm_winding_t *w = Cm_AllocWinding(face->num_vertexes);
	const int32_t v = face->first_vertex;

	for (int32_t i = 0; i < face->num_vertexes; i++) {

		const bsp_vertex_t *v0 = &file->vertexes[(v + (i + 0) % face->num_vertexes)];
		const bsp_vertex_t *v1 = &file->vertexes[(v + (i + 1) % face->num_vertexes)];
		const bsp_vertex_t *v2 = &file->vertexes[(v + (i + 2) % face->num_vertexes)];

		w->points[w->num_points] = v0->position;
		w->num_points++;

		vec3_t a, b;
		a = Vec3_Subtract(v1->position, v0->position);
		b = Vec3_Subtract(v2->position, v1->position);

		a = Vec3_Normalize(a);
		b = Vec3_Normalize(b);

		if (Vec3_Dot(a, b) > 1.0f - COLINEAR_EPSILON) { // skip v1
			i++;
		}
	}

	return w;
}

/**
 * @brief
 */
cm_winding_t *Cm_WindingForBrushSide(const bsp_file_t *file, const bsp_brush_side_t *brush_side) {

	const bsp_plane_t *plane = file->planes + brush_side->plane;
	cm_winding_t *winding = Cm_WindingForPlane(plane->normal, plane->dist);

	const int32_t side = (int32_t) (brush_side - file->brush_sides);

	const bsp_brush_t *brush = file->brushes;
	for (int32_t i = 0; i < file->num_brushes; i++, brush++) {

		if (side >= brush->first_brush_side
			&& side < brush->first_brush_side + brush->num_brush_sides) {
			break;
		}
	}

	const bsp_brush_side_t *s = file->brush_sides + brush->first_brush_side;
	for (int32_t i = 0; i < brush->num_brush_sides; i++, s++) {
		if (s == brush_side) {
			continue;
		}
		if (s->surface & SURF_BEVEL) {
			continue;
		}
		const bsp_plane_t *p = &file->planes[s->plane ^ 1];
		Cm_ClipWinding(&winding, p->normal, p->dist, SIDE_EPSILON);

		if (winding == NULL) {
			break;
		}
	}

	return winding;
}

/**
 * @brief
 */
static cm_winding_t *Cm_FixWinding(cm_winding_t *w) {

	for (int32_t i = 0; i < w->num_points; i++) {
		const vec3_t a = w->points[(i + 0) % w->num_points];
		const vec3_t b = w->points[(i + 1) % w->num_points];

		if (Vec3_EqualEpsilon(a, b, FLT_EPSILON)) {

			for (int32_t j = i + 1; j < w->num_points; j++) {
				w->points[j] = w->points[(j + 1) % w->num_points];
			}

			w->num_points--;
		}
	}

	if (w->num_points < 3) {
		Cm_FreeWinding(w);
		return NULL;
	}

	return w;
}

/**
 * @brief
 */
void Cm_SplitWinding(const cm_winding_t *in, const vec3_t normal, double dist, double epsilon,
						cm_winding_t **front, cm_winding_t **back) {

	assert(in->num_points);
	const int32_t max_points = in->num_points + 4;

	cm_clip_point_t clip_points[max_points];
	memset(&clip_points, 0, max_points * sizeof(cm_clip_point_t));

	int32_t side_front = 0, side_back = 0, side_on = 0;

	cm_clip_point_t *c = clip_points;
	for (int32_t i = 0; i < in->num_points; i++, c++) {
		c->point = in->points[i];
		c->dist = (double) Vec3_Dot(c->point, normal) - dist;
		if (c->dist > epsilon) {
			c->side = SIDE_FRONT;
			side_front++;
		} else if (c->dist < -epsilon) {
			c->side = SIDE_BACK;
			side_back++;
		} else {
			c->side = SIDE_ON;
			side_on++;
		}
	}

	if (side_front == 0) {
		*front = NULL;
		*back = Cm_CopyWinding(in);
		return;
	}

	if (side_back == 0) {
		*front = Cm_CopyWinding(in);
		*back = NULL;
		return;
	}

	cm_winding_t *f = Cm_AllocWinding(max_points);
	cm_winding_t *b = Cm_AllocWinding(max_points);

	for (int32_t i = 0; i < in->num_points; i++) {
		const cm_clip_point_t *c = clip_points + i;

		if (c->side == SIDE_ON) {
			f->points[f->num_points] = c->point;
			f->num_points++;

			b->points[b->num_points] = c->point;
			b->num_points++;

			continue;
		}

		if (c->side == SIDE_FRONT) {
			f->points[f->num_points] = c->point;
			f->num_points++;
		}

		if (c->side == SIDE_BACK) {
			b->points[b->num_points] = c->point;
			b->num_points++;
		}

		const cm_clip_point_t *d = clip_points + ((i + 1) % in->num_points);

		if (d->side == SIDE_ON || d->side == c->side) {
			continue;
		}

		vec3d_t mid = Vec3d_Zero();
		const double dot = c->dist / (c->dist - d->dist);
		for (int32_t j = 0; j < 3; j++) { // avoid round off error when possible
			if (normal.xyz[j] > 1.f - FLT_EPSILON) {
				mid.xyz[j] = dist;
			} else if (normal.xyz[j] < -1.f + FLT_EPSILON) {
				mid.xyz[j] = -dist;
			} else {
				mid.xyz[j] = (double) c->point.xyz[j] + dot * (double) (d->point.xyz[j] - c->point.xyz[j]);
			}
		}

		f->points[f->num_points] = Vec3d_CastVec3(mid);
		f->num_points++;

		b->points[b->num_points] = Vec3d_CastVec3(mid);
		b->num_points++;

		if (f->num_points == max_points || b->num_points == max_points) {
			Com_Error(ERROR_FATAL, "Points exceeded estimate\n");
		}
	}

	*front = Cm_FixWinding(f);
	*back = Cm_FixWinding(b);
}

/**
 * @brief Clips the winding against the given plane.
 */
void Cm_ClipWinding(cm_winding_t **in_out, const vec3_t normal, double dist, double epsilon) {

	cm_winding_t *in = *in_out;
	
	assert(in->num_points);
	const int32_t max_points = in->num_points + 4;

	cm_clip_point_t clip_points[max_points];
	memset(clip_points, 0, max_points * sizeof(cm_clip_point_t));

	int32_t side_front = 0, side_back = 0, side_both = 0;

	cm_clip_point_t *c = clip_points;
	for (int32_t i = 0; i < in->num_points; i++, c++) {
		c->point = in->points[i];
		c->dist = (double) Vec3_Dot(c->point, normal) - dist;
		if (c->dist > epsilon) {
			c->side = SIDE_FRONT;
			side_front++;
		} else if (c->dist < -epsilon) {
			c->side = SIDE_BACK;
			side_back++;
		} else {
			c->side = SIDE_BOTH;
			side_both++;
		}
	}

	if (side_front == 0) {
		Cm_FreeWinding(in);
		*in_out = NULL;
		return;
	}

	if (side_back == 0) {
		return;
	}

	cm_winding_t *out = Cm_AllocWinding(max_points);

	for (int32_t i = 0; i < in->num_points; i++) {
		const cm_clip_point_t *c = clip_points + i;

		if (c->side == SIDE_BOTH) {
			out->points[out->num_points] = c->point;
			out->num_points++;
			continue;
		}

		if (c->side == SIDE_FRONT) {
			out->points[out->num_points] = c->point;
			out->num_points++;
		}

		const cm_clip_point_t *d = clip_points + ((i + 1) % in->num_points);

		if (d->side == SIDE_BOTH || d->side == c->side) {
			continue;
		}

		vec3d_t mid = Vec3d_Zero();
		const double dot = c->dist / (c->dist - d->dist);
		for (int32_t j = 0; j < 3; j++) { // avoid round off error when possible
			if (normal.xyz[j] > 1.f - FLT_EPSILON) {
				mid.xyz[j] = dist;
			} else if (normal.xyz[j] < -1.f + FLT_EPSILON) {
				mid.xyz[j] = -dist;
			} else {
				mid.xyz[j] = (double) c->point.xyz[j] + dot * (double) (d->point.xyz[j] - c->point.xyz[j]);
			}
		}

		out->points[out->num_points] = Vec3d_CastVec3(mid);
		out->num_points++;

		if (out->num_points == max_points) {
			Com_Error(ERROR_FATAL, "Points exceeded estimate\n");
		}
	}

	Cm_FreeWinding(in);
	*in_out = Cm_FixWinding(out);
}

/**
 * @brief If two polygons share a common edge and the edges that meet at the
 * common points are both inside the other polygons, merge them
 *
 * Returns NULL if the faces couldn't be merged, or the new face.
 * The originals will NOT be freed.
 */
cm_winding_t *Cm_MergeWindings(const cm_winding_t *a, const cm_winding_t *b, const vec3_t normal) {
	vec3_t p1, p2, back;
	int32_t i, j, k, l;
	vec3_t cross, delta;
	float dot;

	// find a common edge
	p1 = p2 = Vec3_Zero();
	j = 0;

	for (i = 0; i < a->num_points; i++) {
		p1 = a->points[i];
		p2 = a->points[(i + 1) % a->num_points];
		for (j = 0; j < b->num_points; j++) {
			vec3_t p3 = b->points[j];
			vec3_t p4 = b->points[(j + 1) % b->num_points];
			for (k = 0; k < 3; k++) {
				if (fabsf(p1.xyz[k] - p4.xyz[k]) > ON_EPSILON) {
					break;
				}
				if (fabsf(p2.xyz[k] - p3.xyz[k]) > ON_EPSILON) {
					break;
				}
			}
			if (k == 3) {
				break;
			}
		}
		if (j < b->num_points) {
			break;
		}
	}

	if (i == a->num_points) {
		return NULL; // no matching edges
	}

	// if the slopes are colinear, the point can be removed
	back = a->points[(i + a->num_points - 1) % a->num_points];
	delta = Vec3_Subtract(p1, back);
	cross = Vec3_Cross(normal, delta);
	cross = Vec3_Normalize(cross);

	back = b->points[(j + 2) % b->num_points];
	delta = Vec3_Subtract(back, p1);
	dot = Vec3_Dot(delta, cross);
	if (dot > COLINEAR_EPSILON) {
		return NULL; // not a convex polygon
	}
	const _Bool keep1 = dot < -COLINEAR_EPSILON;

	back = a->points[(i + 2) % a->num_points];
	delta = Vec3_Subtract(back, p2);
	cross = Vec3_Cross(normal, delta);
	cross = Vec3_Normalize(cross);

	back = b->points[(j + b->num_points - 1) % b->num_points];
	delta = Vec3_Subtract(back, p2);
	dot = Vec3_Dot(delta, cross);
	if (dot > COLINEAR_EPSILON) {
		return NULL; // not a convex polygon
	}
	const _Bool keep2 = dot < -COLINEAR_EPSILON;

	// build the new polygon
	cm_winding_t *merged = Cm_AllocWinding(a->num_points + b->num_points);

	// copy first polygon
	for (k = (i + 1) % a->num_points; k != i; k = (k + 1) % a->num_points) {
		if (k == (i + 1) % a->num_points && !keep2) {
			continue;
		}

		merged->points[merged->num_points] = a->points[k];
		merged->num_points++;
	}

	// copy second polygon
	for (l = (j + 1) % b->num_points; l != j; l = (l + 1) % b->num_points) {
		if (l == (j + 1) % b->num_points && !keep1) {
			continue;
		}
		merged->points[merged->num_points] = b->points[l];
		merged->num_points++;
	}

	return Cm_FixWinding(merged);
}

/**
 * @brief Creates a vertex element array of triangles for the given winding.
 * @details This function uses an ear-clipping algorithm to clip triangles from
 * the given winding. Invalid triangles due to colinear points are skipped over.
 * @param w The winding.
 * @param elements The output array, which must be `>= (w->num_points - 2) * 3` in length.
 * @return The number of vertex elements written to tris.
 */
int32_t Cm_ElementsForWinding(const cm_winding_t *w, int32_t *elements) {

	int32_t *out = elements;

	typedef struct {
		vec3_t position;
		int32_t index;
		int32_t corner;
	} point_t;

	int32_t num_points = w->num_points;
	point_t points[num_points];

	for (int32_t i = 0; i < num_points; i++) {
		points[i].position = w->points[i];
		points[i].index = i;
	}

	while (num_points > 2) {

		// find the corners, or points without collinear neighbors

		int32_t num_corners = 0;
		for (int32_t i = 0; i < num_points; i++) {

			point_t *a = &points[(i + 0) % num_points];
			point_t *b = &points[(i + 1) % num_points];
			point_t *c = &points[(i + 2) % num_points];

			const vec3_t ba = Vec3_Direction(b->position, a->position);
			const vec3_t cb = Vec3_Direction(c->position, b->position);

			const float dot = Vec3_Dot(ba, cb);
			if (dot > 1.f - COLINEAR_EPSILON) {
				b->corner = 0;
			} else {
				b->corner = ++num_corners;
			}
		}

		// if we don't find 3 corners, this is a degenerate winding (a line segment)

		if (num_corners < 3) {
			Com_Warn("Invalid winding: %d corners found in %d points\n", num_corners, num_points);
			break;
		}

		// chip away at edges with colinear points first

		const point_t *clip = NULL;
		if (num_corners < num_points) {
			float best = FLT_MAX;

			for (int32_t i = 0; i < num_points; i++) {
				const point_t *a = &points[(i + 0) % num_points];
				const point_t *b = &points[(i + 1) % num_points];
				const point_t *c = &points[(i + 2) % num_points];

				if (!a->corner && b->corner) {
					const float area = Cm_TriangleArea(a->position, b->position, c->position);
					if (area < best) {
						best = area;
						clip = b;
					}
				}
			}
			assert(clip);
		} else {
			clip = points + 1;
		}

		const int32_t i = (int32_t) (ptrdiff_t) (clip - points);
		const int32_t j = (i - 1 + num_points) % num_points;

		for (int32_t k = 0; k < 3; k++) {
			*out++ = points[(j + k) % num_points].index;
		}

		for (int32_t k = i; k < num_points - 1; k++) {
			points[k] = points[k + 1];
		}

		num_points--;
	}

	return (int32_t) (ptrdiff_t) (out - elements);
}

/**
* @return The area of the triangle defined by a, b and c.
*/
float Cm_TriangleArea(const vec3_t a, const vec3_t b, const vec3_t c) {

   const vec3_t ba = Vec3_Subtract(b, a);
   const vec3_t ca = Vec3_Subtract(c, a);
   const vec3_t cross = Vec3_Cross(ba, ca);

   return Vec3_Length(cross) * 0.5f;
}

/**
* @brief Calculates barycentric coordinates for p in the triangle defined by a, b and c.
* @remarks The max_area checks ensure that p is (approximately) inside the triangle abc.
* @see https://www.scratchapixel.com/lessons/3d-basic-rendering/ray-tracing-rendering-a-triangle/barycentric-coordinates
*/
float Cm_Barycentric(const vec3_t a, const vec3_t b, const vec3_t c, const vec3_t p, vec3_t *out) {

	const float abc = Cm_TriangleArea(a, b, c);
	if (abc) {
		const float max_area = abc * 1.f;

		const float bcp = Cm_TriangleArea(b, c, p);
		if (bcp > max_area) {
			return FLT_MAX;
		}

		const float cap = Cm_TriangleArea(c, a, p);
		if (cap > max_area) {
			return FLT_MAX;
		}

		const float abp = Cm_TriangleArea(a, b, p);
		if (abp > max_area) {
			return FLT_MAX;
		}

		out->x = bcp / abc;
		out->y = cap / abc;
		out->z = abp / abc;

		return out->x + out->y + out->z;
	} else {
	   *out = Vec3_Zero();
	}

	return FLT_MAX;
}

/**
 * @brief Calculates the tangent vectors for the given vertexes and triangle elements.
 * @see http://foundationsofgameenginedev.com/FGED2-sample.pdf
 */
void Cm_Tangents(cm_vertex_t *vertexes, int32_t num_vertexes, const int32_t *elements, int32_t num_elements) {

	for (int32_t i = 0; i < num_elements; i += 3) {

		const int32_t i0 = *(elements + i + 0);
		const int32_t i1 = *(elements + i + 1);
		const int32_t i2 = *(elements + i + 2);

		cm_vertex_t *v0 = vertexes + i0;
		cm_vertex_t *v1 = vertexes + i1;
		cm_vertex_t *v2 = vertexes + i2;

		const vec3_t e1 = Vec3_Subtract(*v1->position, *v0->position);
		const vec3_t e2 = Vec3_Subtract(*v2->position, *v0->position);

		const float x1 = v1->st->x - v0->st->x;
		const float x2 = v2->st->x - v0->st->x;

		const float y1 = v1->st->y - v0->st->y;
		const float y2 = v2->st->y - v0->st->y;

		const float r = 1.f / (x1 * y2 - x2 * y1);

		if (r == INFINITY || r == -INFINITY) {
			continue;
		}

		const vec3_t t = Vec3_Scale(Vec3_Subtract(Vec3_Scale(e1, y2), Vec3_Scale(e2, y1)), r);
		const vec3_t b = Vec3_Scale(Vec3_Subtract(Vec3_Scale(e2, x1), Vec3_Scale(e1, x2)), r);

		*v0->tangent = Vec3_Add(*v0->tangent, t);
		*v1->tangent = Vec3_Add(*v1->tangent, t);
		*v2->tangent = Vec3_Add(*v2->tangent, t);

		*v0->bitangent = Vec3_Add(*v0->bitangent, b);
		*v1->bitangent = Vec3_Add(*v1->bitangent, b);
		*v2->bitangent = Vec3_Add(*v2->bitangent, b);

		v0->num_tris++;
		v1->num_tris++;
		v2->num_tris++;
	}

	cm_vertex_t *v = vertexes;
	for (int32_t i = 0; i < num_vertexes; i++, v++) {

		const vec3_t sdir = *v->tangent;
		const vec3_t tdir = *v->bitangent;

		Vec3_Tangents(*v->normal, sdir, tdir, v->tangent, v->bitangent);
	}
}
