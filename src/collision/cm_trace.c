/*
 * Copyright(c) 1997-2001 id Software, Inc.
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

#include "cm_local.h"

/*
 * @brief (1.0 / 32.0) epsilon to keep floating point happy.
 */
#define DIST_EPSILON 0.03125

/*
 * @brief Box trace data encapsulation and context management.
 */
typedef struct {
	vec3_t start, end;
	vec3_t mins, maxs;
	vec3_t extents;
	vec3_t offsets[8];
	vec3_t box_mins, box_maxs;

	int32_t contents;
	_Bool is_point;

	cm_trace_t trace;

	int32_t mailbox[32]; // used to avoid multiple intersection tests with brushes
} cm_trace_data_t;

/*
 * @brief
 */
static _Bool Cm_BrushAlreadyTested(cm_trace_data_t *data, const int32_t brush_num) {

	const int32_t hash = brush_num & 31;
	const _Bool skip = (data->mailbox[hash] == brush_num);

	data->mailbox[hash] = brush_num;

	return skip;
}

/*
 * @brief Clips the bounded box to all brush sides for the given brush.
 */
static void Cm_TraceToBrush(cm_trace_data_t *data, const cm_bsp_brush_t *brush) {

	if (!brush->num_sides)
		return;

	if (!BoxIntersect(data->box_mins, data->box_maxs, brush->mins, brush->maxs))
		return;

	vec_t enter_fraction = -1.0;
	vec_t leave_fraction = 1.0;

	const cm_bsp_plane_t *clip_plane = NULL;

	_Bool end_outside = false, start_outside = false;

	const cm_bsp_brush_side_t *lead_side = NULL;
	const cm_bsp_brush_side_t *side = &cm_bsp.brush_sides[brush->first_brush_side];

	for (int32_t i = 0; i < brush->num_sides; i++, side++) {
		const cm_bsp_plane_t *plane = side->plane;

		const vec_t dist = plane->dist - DotProduct(data->offsets[plane->sign_bits], plane->normal);

		const vec_t d1 = DotProduct(data->start, plane->normal) - dist;
		const vec_t d2 = DotProduct(data->end, plane->normal) - dist;

		if (d2 > 0.0)
			end_outside = true; // end point is not in solid
		if (d1 > 0.0)
			start_outside = true;

		// if completely in front of face, no intersection
		if (d1 > 0.0 && d2 >= d1)
			return;

		if (d1 <= 0.0 && d2 <= 0.0)
			continue;

		// crosses face
		if (d1 > d2) { // enter
			const vec_t f = (d1 - DIST_EPSILON) / (d1 - d2);
			if (f > enter_fraction) {
				enter_fraction = f;
				clip_plane = plane;
				lead_side = side;
			}
		} else { // leave
			const vec_t f = (d1 + DIST_EPSILON) / (d1 - d2);
			if (f < leave_fraction)
				leave_fraction = f;
		}
	}

	// some sort of collision has occurred

	if (!start_outside) { // original point was inside brush
		data->trace.start_solid = true;
		if (!end_outside) {
			data->trace.all_solid = true;
			data->trace.fraction = 0.0;
			data->trace.contents = brush->contents;
		}
	} else if (enter_fraction < leave_fraction) { // pierced brush
		if (enter_fraction > -1.0 && enter_fraction < data->trace.fraction) {
			data->trace.fraction = MAX(0.0, enter_fraction);
			data->trace.plane = *clip_plane;
			data->trace.surface = lead_side->surface;
			data->trace.contents = brush->contents;
		}
	}
}

/*
 * @brief
 */
static void Cm_TestBoxInBrush(cm_trace_data_t *data, const cm_bsp_brush_t *brush) {

	if (!brush->num_sides)
		return;

	if (!BoxIntersect(data->box_mins, data->box_maxs, brush->mins, brush->maxs))
		return;

	const cm_bsp_brush_side_t *side = &cm_bsp.brush_sides[brush->first_brush_side];

	for (int32_t i = 0; i < brush->num_sides; i++, side++) {
		const cm_bsp_plane_t *plane = side->plane;

		const vec_t dist = plane->dist - DotProduct(data->offsets[plane->sign_bits], plane->normal);

		const vec_t d1 = DotProduct(data->start, plane->normal) - dist;

		// if completely in front of face, no intersection
		if (d1 > 0.0)
			return;
	}

	// inside this brush
	data->trace.start_solid = data->trace.all_solid = true;
	data->trace.fraction = 0.0;
	data->trace.contents = brush->contents;
}

/*
 * @brief
 */
static void Cm_TraceToLeaf(cm_trace_data_t *data, int32_t leaf_num) {

	const cm_bsp_leaf_t *leaf = &cm_bsp.leafs[leaf_num];

	if (!(leaf->contents & data->contents))
		return;

	// trace line against all brushes in the leaf
	for (int32_t i = 0; i < leaf->num_leaf_brushes; i++) {
		const int32_t brush_num = cm_bsp.leaf_brushes[leaf->first_leaf_brush + i];

		if (Cm_BrushAlreadyTested(data, brush_num))
			continue; // already checked this brush in another leaf

		const cm_bsp_brush_t *b = &cm_bsp.brushes[brush_num];

		if (!(b->contents & data->contents))
			continue;

		Cm_TraceToBrush(data, b);

		if (data->trace.all_solid)
			return;
	}
}

/*
 * @brief
 */
static void Cm_TestInLeaf(cm_trace_data_t *data, int32_t leaf_num) {

	const cm_bsp_leaf_t *leaf = &cm_bsp.leafs[leaf_num];

	if (!(leaf->contents & data->contents))
		return;

	// trace line against all brushes in the leaf
	for (int32_t i = 0; i < leaf->num_leaf_brushes; i++) {
		const int32_t brush_num = cm_bsp.leaf_brushes[leaf->first_leaf_brush + i];

		if (Cm_BrushAlreadyTested(data, brush_num))
			continue; // already checked this brush in another leaf

		const cm_bsp_brush_t *b = &cm_bsp.brushes[brush_num];

		if (!(b->contents & data->contents))
			continue;

		Cm_TestBoxInBrush(data, b);

		if (data->trace.all_solid)
			return;
	}
}

/*
 * @brief
 */
static void Cm_TraceToNode(cm_trace_data_t *data, int32_t num, vec_t p1f, vec_t p2f,
		const vec3_t p1, const vec3_t p2) {

	if (data->trace.fraction <= p1f)
		return; // already hit something nearer

	// if < 0, we are in a leaf node
	if (num < 0) {
		Cm_TraceToLeaf(data, -1 - num);
		return;
	}

	// find the point distances to the separating plane
	// and the offset for the size of the box
	const cm_bsp_node_t *node = cm_bsp.nodes + num;
	const cm_bsp_plane_t *plane = node->plane;

	vec_t d1, d2, offset;
	if (AXIAL(plane)) {
		d1 = p1[plane->type] - plane->dist;
		d2 = p2[plane->type] - plane->dist;
		offset = data->extents[plane->type];
	} else {
		d1 = DotProduct(plane->normal, p1) - plane->dist;
		d2 = DotProduct(plane->normal, p2) - plane->dist;
		if (data->is_point)
			offset = 0.0;
		else
			offset = fabsf(data->extents[0] * plane->normal[0]) + fabsf(
					data->extents[1] * plane->normal[1]) + fabsf(
					data->extents[2] * plane->normal[2]);
	}

	// see which sides we need to consider
	if (d1 >= offset && d2 >= offset) {
		Cm_TraceToNode(data, node->children[0], p1f, p2f, p1, p2);
		return;
	}
	if (d1 <= -offset && d2 <= -offset) {
		Cm_TraceToNode(data, node->children[1], p1f, p2f, p1, p2);
		return;
	}

	// put the cross point DIST_EPSILON pixels on the near side
	int32_t side;
	vec_t frac1, frac2;

	if (d1 < d2) {
		const vec_t idist = 1.0 / (d1 - d2);
		side = 1;
		frac2 = (d1 + offset + DIST_EPSILON) * idist;
		frac1 = (d1 - offset + DIST_EPSILON) * idist;
	} else if (d1 > d2) {
		const vec_t idist = 1.0 / (d1 - d2);
		side = 0;
		frac2 = (d1 - offset - DIST_EPSILON) * idist;
		frac1 = (d1 + offset + DIST_EPSILON) * idist;
	} else {
		side = 0;
		frac1 = 1.0;
		frac2 = 0.0;
	}

	vec3_t mid;

	// move up to the node
	frac1 = Clamp(frac1, 0.0, 1.0);

	const vec_t midf1 = p1f + (p2f - p1f) * frac1;

	for (int32_t i = 0; i < 3; i++)
		mid[i] = p1[i] + frac1 * (p2[i] - p1[i]);

	Cm_TraceToNode(data, node->children[side], p1f, midf1, p1, mid);

	// go past the node
	frac2 = Clamp(frac2, 0.0, 1.0);

	const vec_t midf2 = p1f + (p2f - p1f) * frac2;

	for (int32_t i = 0; i < 3; i++)
		mid[i] = p1[i] + frac2 * (p2[i] - p1[i]);

	Cm_TraceToNode(data, node->children[side ^ 1], midf2, p2f, mid, p2);
}

/*
 * @brief Primary collision detection entry point. This function recurses down
 * the BSP tree from the specified head node, clipping the desired movement to
 * brushes that match the specified contents mask.
 *
 * @param start The starting point.
 * @param end The desired end point.
 * @param mins The bounding box mins, in model space.
 * @param maxs The bounding box maxs, in model space.
 * @param head_node The BSP head node to recurse down.
 * @param contents The contents mask to clip to.
 *
 * @return The trace.
 */
cm_trace_t Cm_BoxTrace(const vec3_t start, const vec3_t end, const vec3_t mins, const vec3_t maxs,
		const int32_t head_node, const int32_t contents) {

	cm_trace_data_t data;
	memset(&data, 0, sizeof(data));

	data.trace.fraction = 1.0;

	if (!cm_bsp.num_nodes) { // map not loaded
		return data.trace;
	}

	VectorCopy(start, data.start);
	VectorCopy(end, data.end);

	VectorCopy(mins, data.mins);
	VectorCopy(maxs, data.maxs);

	data.contents = contents;

	// check for point special case
	if (VectorCompare(mins, vec3_origin) && VectorCompare(maxs, vec3_origin)) {
		data.is_point = true;
	} else {
		data.is_point = false;

		// extents allow planes to be shifted to account for the box size
		data.extents[0] = -mins[0] > maxs[0] ? -mins[0] : maxs[0];
		data.extents[1] = -mins[1] > maxs[1] ? -mins[1] : maxs[1];
		data.extents[2] = -mins[2] > maxs[2] ? -mins[2] : maxs[2];

		// offsets provide sign bit lookups for fast plane tests
		data.offsets[0][0] = mins[0];
		data.offsets[0][1] = mins[1];
		data.offsets[0][2] = mins[2];

		data.offsets[1][0] = maxs[0];
		data.offsets[1][1] = mins[1];
		data.offsets[1][2] = mins[2];

		data.offsets[2][0] = mins[0];
		data.offsets[2][1] = maxs[1];
		data.offsets[2][2] = mins[2];

		data.offsets[3][0] = maxs[0];
		data.offsets[3][1] = maxs[1];
		data.offsets[3][2] = mins[2];

		data.offsets[4][0] = mins[0];
		data.offsets[4][1] = mins[1];
		data.offsets[4][2] = maxs[2];

		data.offsets[5][0] = maxs[0];
		data.offsets[5][1] = mins[1];
		data.offsets[5][2] = maxs[2];

		data.offsets[6][0] = mins[0];
		data.offsets[6][1] = maxs[1];
		data.offsets[6][2] = maxs[2];

		data.offsets[7][0] = maxs[0];
		data.offsets[7][1] = maxs[1];
		data.offsets[7][2] = maxs[2];
	}

	for (int32_t i = 0; i < 3; i++) {
		if (start[i] < end[i]) {
			data.box_mins[i] = start[i] + mins[i] - 1.0;
			data.box_maxs[i] = end[i] + maxs[i] + 1.0;
		} else {
			data.box_mins[i] = end[i] + mins[i] - 1.0;
			data.box_maxs[i] = start[i] + maxs[i] + 1.0;
		}
	}

	memset(data.mailbox, 0xff, sizeof(data.mailbox));

	// check for position test special case
	if (VectorCompare(start, end)) {
		int32_t leafs[1024];

		const size_t len = Cm_BoxLeafnums(data.box_mins, data.box_maxs, leafs, lengthof(leafs),
				NULL, head_node);

		for (size_t i = 0; i < len; i++) {
			Cm_TestInLeaf(&data, leafs[i]);

			if (data.trace.all_solid)
				break;
		}

		VectorCopy(start, data.trace.end);
		return data.trace;
	}

	Cm_TraceToNode(&data, head_node, 0.0, 1.0, start, end);

	if (data.trace.fraction == 0.0) {
		VectorCopy(start, data.trace.end);
	} else if (data.trace.fraction == 1.0) {
		VectorCopy(end, data.trace.end);
	} else {
		VectorMix(start, end, data.trace.fraction, data.trace.end);
	}

	return data.trace;
}

/*
 * @brief Collision detection for non-world models. Rotates the specified end
 * points into the model's space, and traces down the relevant subset of the
 * BSP tree. For inline BSP models, the head node is the root of the model's
 * subtree. For mesh models, a special reserved box hull and head node are
 * used.
 *
 * @param start The trace start point, in world space.
 * @param end The trace end point, in world space.
 * @param mins The trace bounding box mins.
 * @param maxs The trace bounding box maxs.
 * @param head_node The BSP head node to recurse down.
 * @param contents The contents mask to clip to.
 * @param matrix The matrix of the entity to be clipped to.
 * @param inverse_matrix The inverse matrix of the entity to be clipped to.
 *
 * @return The trace.
 */
cm_trace_t Cm_TransformedBoxTrace(const vec3_t start, const vec3_t end, const vec3_t mins,
		const vec3_t maxs, const int32_t head_node, const int32_t contents,
		const matrix4x4_t *matrix, const matrix4x4_t *inverse_matrix) {

	vec3_t start0, end0;

	Matrix4x4_Transform(inverse_matrix, start, start0);
	Matrix4x4_Transform(inverse_matrix, end, end0);

	// sweep the box through the model
	cm_trace_t trace = Cm_BoxTrace(start0, end0, mins, maxs, head_node, contents);

	if (trace.fraction < 1.0) {
		vec4_t plane;

		const cm_bsp_plane_t *p = &trace.plane;
		const vec_t *n = p->normal;

		Matrix4x4_TransformPositivePlane(matrix, n[0], n[1], n[2], p->dist, plane);

		VectorCopy(plane, trace.plane.normal);
		trace.plane.dist = plane[3];
	}

	// and calculate the final end point
	VectorMix(start, end, trace.fraction, trace.end);

	return trace;
}
