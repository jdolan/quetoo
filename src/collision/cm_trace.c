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

#include "cm_local.h"

/**
 * @brief Box trace data encapsulation and context management.
 */
typedef struct {
	/**
	 * @brief The trace start and end points, as provided by the user.
	 */
	vec3_t start, end;

	/**
	 * @brief The trace bounds, as provided by the user.
	 */
	box3_t bounds;

	/**
	 * @brief The absolute bounds of the trace, spanning the start and end points.
	 */
	box3_t abs_bounds;

	/**
	 * @brief The trace size, expanded to a symmetrical box to account for rotations.
	 */
	vec3_t size;

	/**
	 * @brief The "corners" of the trace bounds, used for fast plane sidedness tests.
	 */
	vec3_t offsets[8];

	/**
	 * @brief The contents mask to collide with, as provided by the user.
	 */
	int32_t contents;

	/**
	 * @brief The transformation matrix for plane collisions, as provided by the user.
	 */
	mat4_t matrix;

	/**
	 * @brief True if matrix is not the identity.
	 */
	_Bool is_transformed;

	/**
	 * @brief The brush cache, to avoid multiple tests against the same brush.
	 */
	int32_t brush_cache[128];

	/**
	 * @brief The trace result.
	 */
	cm_trace_t trace;
} cm_trace_data_t;

/**
 * @brief
 */
static _Bool Cm_BrushAlreadyTested(cm_trace_data_t *data, int32_t brush_num) {
	const int32_t hash = brush_num & (lengthof(data->brush_cache) - 1);

	const _Bool skip = (data->brush_cache[hash] == brush_num);

	data->brush_cache[hash] = brush_num;

	return skip;
}

/**
 * @brief 
 */
static inline _Bool Cm_TraceIntersect(cm_trace_data_t *data, const cm_bsp_brush_t *brush) {

	box3_t brush_bounds = brush->bounds;

	if (data->is_transformed) {
		brush_bounds = Mat4_TransformBounds(data->matrix, brush_bounds);
	}

	return Box3_Intersects(data->abs_bounds, brush_bounds);
}

/**
 * @brief Clips the bounded box to all brush sides for the given brush.
 */
static void Cm_TraceToBrush(cm_trace_data_t *data, const cm_bsp_brush_t *brush) {

	if (!brush->num_brush_sides) {
		return;
	}

	if (!Cm_TraceIntersect(data, brush)) {
		return;
	}

	float enter_fraction = -1.f;
	float leave_fraction = 1.f;

	cm_bsp_plane_t plane = { };
	const cm_bsp_brush_side_t *side = NULL;

	_Bool start_outside = false, end_outside = false;

	const cm_bsp_brush_side_t *s = brush->brush_sides;
	for (int32_t i = 0; i < brush->num_brush_sides; i++, s++) {

		cm_bsp_plane_t p = *s->plane;

		if (data->is_transformed) {
			p = Cm_TransformPlane(data->matrix, &p);
		}

		const float dist = p.dist - Vec3_Dot(data->offsets[p.sign_bits], p.normal);

		const float d1 = Vec3_Dot(data->start, p.normal) - dist;
		const float d2 = Vec3_Dot(data->end, p.normal) - dist;

		if (d1 > 0.f) {
			start_outside = true;
		}
		if (d2 > 0.f) {
			end_outside = true;
		}

		// if completely in front of plane, the trace does not intersect with the brush
		if (d1 > 0.f && d2 >= d1) {
			return;
		}

		// if completely behind plane, the trace does not intersect with this side
		if (d1 <= 0.f && d2 <= d1) {
			continue;
		}

		// the trace intersects this side
		if (d1 > d2) { // enter
			const float f = Maxf(0.f, (d1 - TRACE_EPSILON) / (d1 - d2));
			if (f > enter_fraction) {
				enter_fraction = f;
				plane = p;
				side = s;
			}
		} else { // leave
			const float f = Minf(1.f, (d1 + TRACE_EPSILON) / (d1 - d2));
			if (f < leave_fraction) {
				leave_fraction = f;
			}
		}
	}

	// some sort of collision has occurred

	if (!start_outside) { // original point was inside brush
		data->trace.start_solid = true;
		if (!end_outside) {
			data->trace.all_solid = true;
			data->trace.contents = brush->contents;
			data->trace.fraction = 0.f;
		}
	} else if (enter_fraction < leave_fraction) { // pierced brush
		if (enter_fraction > -1.f && enter_fraction < data->trace.fraction) {
			data->trace.fraction = Maxf(0.f, enter_fraction);
			data->trace.brush_side = side;
			data->trace.plane = plane;
			data->trace.contents = side->contents;
			data->trace.surface = side->surface;
			data->trace.material = side->material;
		}
	}
}

/**
 * @brief
 */
static void Cm_TestBoxInBrush(cm_trace_data_t *data, const cm_bsp_brush_t *brush) {

	if (!brush->num_brush_sides) {
		return;
	}

	if (!Cm_TraceIntersect(data, brush)) {
		return;
	}

	const cm_bsp_brush_side_t *side = brush->brush_sides;
	for (int32_t i = 0; i < brush->num_brush_sides; i++, side++) {

		cm_bsp_plane_t plane = *side->plane;

		if (data->is_transformed) {
			plane = Cm_TransformPlane(data->matrix, &plane);
		}

		const float dist = plane.dist - Vec3_Dot(data->offsets[plane.sign_bits], plane.normal);

		const float d1 = Vec3_Dot(data->start, plane.normal) - dist;

		// if completely in front of face, no intersection
		if (d1 > 0.0f) {
			return;
		}
	}

	// inside this brush
	data->trace.start_solid = data->trace.all_solid = true;
	data->trace.fraction = 0.0f;
	data->trace.contents = brush->contents;
}

/**
 * @brief
 */
static void Cm_TraceToLeaf(cm_trace_data_t *data, int32_t leaf_num) {

	const cm_bsp_leaf_t *leaf = &cm_bsp.leafs[leaf_num];

	if (!(leaf->contents & data->contents)) {
		return;
	}

	// trace line against all brushes in the leaf
	for (int32_t i = 0; i < leaf->num_leaf_brushes; i++) {
		const int32_t brush_num = cm_bsp.leaf_brushes[leaf->first_leaf_brush + i];

		if (Cm_BrushAlreadyTested(data, brush_num)) {
			continue; // already checked this brush in another leaf
		}

		const cm_bsp_brush_t *b = &cm_bsp.brushes[brush_num];

		if (!(b->contents & data->contents)) {
			continue;
		}

		Cm_TraceToBrush(data, b);

		if (data->trace.all_solid) {
			return;
		}
	}
}

/**
 * @brief
 */
static void Cm_TestInLeaf(cm_trace_data_t *data, int32_t leaf_num) {

	const cm_bsp_leaf_t *leaf = &cm_bsp.leafs[leaf_num];

	if (!(leaf->contents & data->contents)) {
		return;
	}

	// trace line against all brushes in the leaf
	for (int32_t i = 0; i < leaf->num_leaf_brushes; i++) {
		const int32_t brush_num = cm_bsp.leaf_brushes[leaf->first_leaf_brush + i];

		if (Cm_BrushAlreadyTested(data, brush_num)) {
			continue; // already checked this brush in another leaf
		}

		const cm_bsp_brush_t *b = &cm_bsp.brushes[brush_num];

		if (!(b->contents & data->contents)) {
			continue;
		}

		Cm_TestBoxInBrush(data, b);

		if (data->trace.all_solid) {
			return;
		}
	}
}

/**
 * @brief
 */
static void Cm_TraceToNode(cm_trace_data_t *data, int32_t num, float p1f, float p2f,
                           const vec3_t p1, const vec3_t p2) {

	if (data->trace.fraction <= p1f) {
		return; // already hit something nearer
	}

	// if < 0, we are in a leaf node
	if (num < 0) {
		Cm_TraceToLeaf(data, -1 - num);
		return;
	}

	// find the point distances to the separating plane
	// and the offset for the size of the box
	const cm_bsp_node_t *node = cm_bsp.nodes + num;
	cm_bsp_plane_t plane = *node->plane;

	if (data->is_transformed) {
		plane = Cm_TransformPlane(data->matrix, &plane);
	}

	float d1, d2, offset;
	if (AXIAL(&plane)) {
		d1 = p1.xyz[plane.type] - plane.dist;
		d2 = p2.xyz[plane.type] - plane.dist;
		offset = data->size.xyz[plane.type];
	} else {
		d1 = Vec3_Dot(plane.normal, p1) - plane.dist;
		d2 = Vec3_Dot(plane.normal, p2) - plane.dist;
		offset = (fabsf(data->size.x * plane.normal.x) +
				  fabsf(data->size.y * plane.normal.y) +
				  fabsf(data->size.z * plane.normal.z)) * 3.f;
	}

	// see which sides we need to consider
	if (d1 >= offset && d2 >= offset) {
		Cm_TraceToNode(data, node->children[0], p1f, p2f, p1, p2);
		return;
	}
	if (d1 < -offset && d2 < -offset) {
		Cm_TraceToNode(data, node->children[1], p1f, p2f, p1, p2);
		return;
	}

	// put the cross point DIST_EPSILON pixels on the near side
	int32_t side;
	float frac1, frac2;

	if (d1 < d2) {
		const float idist = 1.f / (d1 - d2);
		side = 1;
		frac2 = (d1 + offset) * idist;
		frac1 = (d1 - offset) * idist;
	} else if (d1 > d2) {
		const float idist = 1.f / (d1 - d2);
		side = 0;
		frac2 = (d1 - offset) * idist;
		frac1 = (d1 + offset) * idist;
	} else {
		side = 0;
		frac1 = 1.f;
		frac2 = 0.f;
	}

	// move up to the node
	frac1 = Clampf(frac1, 0.f, 1.f);

	const float midf1 = p1f + (p2f - p1f) * frac1;

	vec3_t mid = Vec3_Mix(p1, p2, frac1);

	Cm_TraceToNode(data, node->children[side], p1f, midf1, p1, mid);

	// go past the node
	frac2 = Clampf(frac2, 0.f, 1.f);

	const float midf2 = p1f + (p2f - p1f) * frac2;

	mid = Vec3_Mix(p1, p2, frac2);

	Cm_TraceToNode(data, node->children[side ^ 1], midf2, p2f, mid, p2);
}

/**
 * @brief Primary collision detection entry point. This function recurses down
 * the BSP tree from the specified head node, clipping the desired movement to
 * brushes that match the specified contents mask.
 * 
 * @remarks For non-world brush models: if the trace is a line trace (empty mins/maxs)
 * the trace itself is rotated before tracing down the relevant subset of the BSP
 * tree, and the resulting plane is un-rotated back into world space; if the trace
 * is a box trace, then planes are individually rotated and tested as the trace makes
 * its way through the tree.
 *
 * @param start The starting point.
 * @param end The desired end point.
 * @param bounds The bounding box, in model space.
 * @param head_node The BSP head node to recurse down. For inline BSP models,
 * the head node is the root of the model's subtree. For mesh models, a
 * special reserved box hull and head node are used.
 * @param contents The contents mask to clip to.
 * @param matrix The matrix to adjust tested planes by.
 *
 * @return The trace.
 */
cm_trace_t Cm_BoxTrace(const vec3_t start, const vec3_t end, const box3_t bounds, int32_t head_node,
					   int32_t contents, const mat4_t matrix) {

	cm_trace_data_t data = {
		.start = start,
		.end = end,
		.bounds = bounds,
		.abs_bounds = Cm_TraceBounds(start, end, bounds),
		.size = Box3_Symetrical(Box3_Expand(bounds, BOX_EPSILON)),
		.contents = contents,
		.matrix = matrix,
		.is_transformed = !Mat4_Equal(matrix, Mat4_Identity()),
		.trace = {
			.fraction = 1.f
		}
	};

	if (!cm_bsp.num_nodes) { // map not loaded
		return data.trace;
	}

	Box3_ToPoints(data.bounds, data.offsets);

	memset(data.brush_cache, 0xff, sizeof(data.brush_cache));

	// check for position test special case
	if (Vec3_Equal(start, end)) {
		static __thread int32_t leafs[MAX_BSP_LEAFS];
		const size_t num_leafs = Cm_BoxLeafnums(data.abs_bounds,
												leafs,
												lengthof(leafs),
												NULL,
												head_node,
												matrix);

		for (size_t i = 0; i < num_leafs; i++) {
			Cm_TestInLeaf(&data, leafs[i]);

			if (data.trace.all_solid) {
				break;
			}
		}

		data.trace.end = start;
		return data.trace;
	}

	Cm_TraceToNode(&data, head_node, 0.f, 1.f, data.start, data.end);

	if (data.trace.fraction == 0.f) {
		data.trace.end = start;
	} else if (data.trace.fraction == 1.f) {
		data.trace.end = end;
	} else {
		data.trace.end = Vec3_Mix(start, end, data.trace.fraction);
	}

	return data.trace;
}

/**
 * @brief Calculates a suitable bounding box for tracing to an entity.
 * @param solid The entity's solid type.
 * @param origin The entity's origin.
 * @param angles The entity's angles.
 * @param bounds The entity's bounds, in model space.
 * @return The resulting bounds, in world space.
 * @remarks BSP entities can be rotated, requiring special attention.
 */
box3_t Cm_EntityBounds(const solid_t solid, const vec3_t origin, const vec3_t angles,
					   const mat4_t matrix, const box3_t bounds) {

	box3_t result = Box3_FromCenter(origin);

	if (solid == SOLID_BSP) {
		
		if (!Vec3_Equal(angles, Vec3_Zero())) {
			// TODO ??? why is mins/maxs already offset by origin in this case?
			result = Mat4_TransformBounds(matrix, bounds);
		} else {
			result = Box3_ExpandBox(result, bounds);
		}

		// epsilon, so bmodels can catch riders
		result = Box3_Expand(result, BOX_EPSILON);
	} else {
		result = Box3_ExpandBox(result, bounds);
	}
	
	return result;
}

/**
 * @brief Calculates the bounding box for a trace.
 * @param start The trace start point, in world space.
 * @param end The trace end point, in world space.
 * @param bounds The bounding box, in model space.
 * @return The resulting bounding box, in world space.
 */
box3_t Cm_TraceBounds(const vec3_t start, const vec3_t end, const box3_t bounds) {

	return Box3_Expand(
		Box3_ExpandBox(
			Box3_FromPoints((const vec3_t []) { start, end }, 2),
			bounds
		), BOX_EPSILON);
}
