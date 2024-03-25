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
 * @return The PLANE_ type for the given normal vector.
 */
int32_t Cm_PlaneTypeForNormal(const vec3_t normal) {

	const float x = fabsf(normal.x);
	if (x > 1.f - FLT_EPSILON) {
		return PLANE_X;
	}

	const float y = fabsf(normal.y);
	if (y > 1.f - FLT_EPSILON) {
		return PLANE_Y;
	}

	const float z = fabsf(normal.z);
	if (z > 1.f - FLT_EPSILON) {
		return PLANE_Z;
	}

	if (x >= y && x >= z) {
		return PLANE_ANY_X;
	}
	if (y >= x && y >= z) {
		return PLANE_ANY_Y;
	}

	return PLANE_ANY_Z;
}

/**
 * @return A bit mask hinting at the sign of each normal vector component. This
 * can be used to optimize plane side tests.
 */
int32_t Cm_SignBitsForNormal(const vec3_t normal) {
	int32_t bits = 0;

	for (int32_t i = 0; i < 3; i++) {
		if (normal.xyz[i] < 0.0f) {
			bits |= 1 << i;
		}
	}

	return bits;
}

/**
 * @return A constructed plane struct.
 */
cm_bsp_plane_t Cm_Plane(const vec3_t normal, float dist) {

	return (cm_bsp_plane_t) {
		.normal = normal,
		.dist = dist,
		.type = Cm_PlaneTypeForNormal(normal),
		.sign_bits = Cm_SignBitsForNormal(normal)
	};
}

/**
 * @return The plane transformed by the input matrix.
 */
cm_bsp_plane_t Cm_TransformPlane(const mat4_t matrix, const cm_bsp_plane_t plane) {
	const vec4_t out = Mat4_TransformPlane(matrix, plane.normal, plane.dist);
	return Cm_Plane(Vec4_XYZ(out), out.w);
}

/**
 * @return The `point` projected onto `plane`.
 */
vec3_t Cm_ProjectPointToPlane(const vec3_t point, const cm_bsp_plane_t *plane) {
	const float dist = Cm_DistanceToPlane(point, plane);
	return Vec3_Subtract(point, Vec3_Scale(plane->normal, dist));
}

/**
 * @return `true` if `point` resides inside `brush`, `false` otherwise.
 */
bool Cm_PointInsideBrush(const vec3_t point, const cm_bsp_brush_t *brush) {

	if (Box3_ContainsPoint(brush->bounds, point)) {

		const cm_bsp_brush_side_t *side = brush->brush_sides;
		for (int32_t i = 0; i < brush->num_brush_sides; i++, side++) {
			if (Cm_DistanceToPlane(point, side->plane) > 0.f) {
				return false;
			}
		}

		return true;
	}

	return false;
}

/**
 * @return The sidedness of the given bounds relative to the specified plane.
 * If the box straddles the plane, SIDE_BOTH is returned.
 */
int32_t Cm_BoxOnPlaneSide(const box3_t bounds, const cm_bsp_plane_t *p) {

	if (AXIAL(p)) {
		if (bounds.mins.xyz[p->type] - p->dist >= 0.f) {
			return SIDE_FRONT;
		}
		if (bounds.maxs.xyz[p->type] - p->dist <  0.f) {
			return SIDE_BACK;
		}
		return SIDE_BOTH;
	}

	float dist1, dist2;
	switch (p->sign_bits) {
		case 0:
			dist1 = Vec3_Dot(p->normal, bounds.maxs);
			dist2 = Vec3_Dot(p->normal, bounds.mins);
			break;
		case 1:
			dist1 = p->normal.x * bounds.mins.x + p->normal.y * bounds.maxs.y + p->normal.z * bounds.maxs.z;
			dist2 = p->normal.x * bounds.maxs.x + p->normal.y * bounds.mins.y + p->normal.z * bounds.mins.z;
			break;
		case 2:
			dist1 = p->normal.x * bounds.maxs.x + p->normal.y * bounds.mins.y + p->normal.z * bounds.maxs.z;
			dist2 = p->normal.x * bounds.mins.x + p->normal.y * bounds.maxs.y + p->normal.z * bounds.mins.z;
			break;
		case 3:
			dist1 = p->normal.x * bounds.mins.x + p->normal.y * bounds.mins.y + p->normal.z * bounds.maxs.z;
			dist2 = p->normal.x * bounds.maxs.x + p->normal.y * bounds.maxs.y + p->normal.z * bounds.mins.z;
			break;
		case 4:
			dist1 = p->normal.x * bounds.maxs.x + p->normal.y * bounds.maxs.y + p->normal.z * bounds.mins.z;
			dist2 = p->normal.x * bounds.mins.x + p->normal.y * bounds.mins.y + p->normal.z * bounds.maxs.z;
			break;
		case 5:
			dist1 = p->normal.x * bounds.mins.x + p->normal.y * bounds.maxs.y + p->normal.z * bounds.mins.z;
			dist2 = p->normal.x * bounds.maxs.x + p->normal.y * bounds.mins.y + p->normal.z * bounds.maxs.z;
			break;
		case 6:
			dist1 = p->normal.x * bounds.maxs.x + p->normal.y * bounds.mins.y + p->normal.z * bounds.mins.z;
			dist2 = p->normal.x * bounds.mins.x + p->normal.y * bounds.maxs.y + p->normal.z * bounds.maxs.z;
			break;
		case 7:
			dist1 = Vec3_Dot(p->normal, bounds.mins);
			dist2 = Vec3_Dot(p->normal, bounds.maxs);
			break;
		default:
			dist1 = dist2 = 0.0; // shut up compiler
			break;
	}

	int32_t side = 0;

	if (dist1 - p->dist >= 0.f) {
		side = SIDE_FRONT;
	}
	if (dist2 - p->dist <  0.f) {
		side |= SIDE_BACK;
	}

	return side;
}

/**
 * @brief Bounding box to BSP tree structure for box positional testing.
 */
typedef struct {
	int32_t head_node;
	cm_bsp_plane_t *planes;
	cm_bsp_brush_t *brush;
	cm_bsp_leaf_t *leaf;
} cm_box_t;

static cm_box_t cm_box;

/**
 * @brief Appends a brush (6 nodes, 12 planes) opaquely to the primary BSP
 * structure to represent the bounding box used for Cm_BoxLeafnums. This brush
 * is never tested by the rest of the collision detection code, as it resides
 * just beyond the parsed size of the map.
 */
void Cm_InitBoxHull(cm_bsp_t *bsp) {

	if (bsp->num_planes + 12 > MAX_BSP_PLANES) {
		Com_Error(ERROR_DROP, "MAX_BSP_PLANES\n");
	}

	if (bsp->num_nodes + 6 > MAX_BSP_NODES) {
		Com_Error(ERROR_DROP, "MAX_BSP_NODES\n");
	}

	if (bsp->num_leafs + 1 > MAX_BSP_LEAFS) {
		Com_Error(ERROR_DROP, "MAX_BSP_LEAFS\n");
	}

	if (bsp->num_leaf_brushes + 1 > MAX_BSP_LEAF_BRUSHES) {
		Com_Error(ERROR_DROP, "MAX_BSP_LEAF_BRUSHES\n");
	}

	if (bsp->num_brushes + 1 > MAX_BSP_BRUSHES) {
		Com_Error(ERROR_DROP, "MAX_BSP_BRUSHES\n");
	}

	if (bsp->num_brush_sides + 6 > MAX_BSP_BRUSH_SIDES) {
		Com_Error(ERROR_DROP, "MAX_BSP_BRUSH_SIDES\n");
	}

	// head node
	cm_box.head_node = bsp->num_nodes;

	// planes
	cm_box.planes = &bsp->planes[bsp->num_planes];

	// leaf
	cm_box.leaf = &bsp->leafs[bsp->num_leafs];
	cm_box.leaf->contents = CONTENTS_MONSTER;
	cm_box.leaf->first_leaf_brush = bsp->num_leaf_brushes;
	cm_box.leaf->num_leaf_brushes = 1;

	// leaf brush
	bsp->leaf_brushes[bsp->num_leaf_brushes] = bsp->num_brushes;

	// brush
	cm_box.brush = &bsp->brushes[bsp->num_brushes];
	cm_box.brush->num_brush_sides = 6;
	cm_box.brush->brush_sides = bsp->brush_sides + bsp->num_brush_sides;
	cm_box.brush->contents = CONTENTS_MONSTER;

	for (int32_t i = 0; i < 6; i++) {

		// fill in planes, two per side
		cm_bsp_plane_t *plane = &cm_box.planes[i * 2];
		plane->normal = Vec3_Zero();
		plane->normal.xyz[i >> 1] = 1.f;
		plane->sign_bits = Cm_SignBitsForNormal(plane->normal);
		plane->type = Cm_PlaneTypeForNormal(plane->normal);

		plane = &cm_box.planes[i * 2 + 1];
		plane->normal = Vec3_Zero();
		plane->normal.xyz[i >> 1] = -1.f;
		plane->sign_bits = Cm_SignBitsForNormal(plane->normal);
		plane->type = Cm_PlaneTypeForNormal(plane->normal);

		const int32_t s = i & 1;

		// fill in nodes, one per side
		cm_bsp_node_t *node = &bsp->nodes[cm_box.head_node + i];
		node->plane = bsp->planes + (bsp->num_planes + i * 2);
		node->children[s] = -1 - bsp->num_leafs;
		if (i != 5) {
			node->children[s ^ 1] = cm_box.head_node + i + 1;
		} else {
			node->children[s ^ 1] = -1 - bsp->num_leafs;
		}

		// fill in brush sides, one per side
		cm_bsp_brush_side_t *side = &bsp->brush_sides[bsp->num_brush_sides + i];
		side->plane = bsp->planes + (bsp->num_planes + i * 2 + s);
	}
}

/**
 * @brief Initializes the box hull for the specified bounds, returning the
 * head node for the resulting box hull tree.
 */
int32_t Cm_SetBoxHull(const box3_t bounds, const int32_t contents) {

	cm_box.brush->bounds = bounds;

	cm_box.planes[0].dist = bounds.maxs.x;
	cm_box.planes[1].dist = -bounds.maxs.x;
	cm_box.planes[2].dist = bounds.mins.x;
	cm_box.planes[3].dist = -bounds.mins.x;
	cm_box.planes[4].dist = bounds.maxs.y;
	cm_box.planes[5].dist = -bounds.maxs.y;
	cm_box.planes[6].dist = bounds.mins.y;
	cm_box.planes[7].dist = -bounds.mins.y;
	cm_box.planes[8].dist = bounds.maxs.z;
	cm_box.planes[9].dist = -bounds.maxs.z;
	cm_box.planes[10].dist = bounds.mins.z;
	cm_box.planes[11].dist = -bounds.mins.z;

	cm_box.leaf->contents = cm_box.brush->contents = contents;

	return cm_box.head_node;
}

/**
 * @return The leaf number containing the specified point.
 */
int32_t Cm_PointLeafnum(const vec3_t p, int32_t head_node) {

	if (!cm_bsp.num_nodes) {
		return 0;
	}

	int32_t num = head_node;
	while (num >= 0) {
		const cm_bsp_node_t *node = cm_bsp.nodes + num;
		const float dist = Cm_DistanceToPlane(p, node->plane);
		if (dist < 0.f) {
			num = node->children[1];
		} else {
			num = node->children[0];
		}
	}

	return -1 - num;
}

/**
 * @brief Point contents check.
 *
 * @param p The point to check.
 * @param head_node The BSP head node to recurse down.
 * @param inverse_matrix The inverse matrix of the entity to be tested.
 *
 * @return The contents mask at the specified point.
 *
 * @remarks The input point is transformed because node planes can't be transformed.
 */
int32_t Cm_PointContents(const vec3_t p, int32_t head_node, const mat4_t inverse_matrix) {

	if (!cm_bsp.num_nodes) {
		return 0;
	}

	vec3_t p0 = p;

	if (!Mat4_Equal(inverse_matrix, Mat4_Identity())) {
		p0 = Mat4_Transform(inverse_matrix, p);
	}

	const int32_t leaf_num = Cm_PointLeafnum(p0, head_node);

	return cm_bsp.leafs[leaf_num].contents;
}

/**
 * @brief Data binding structure for box to leaf tests.
 */
typedef struct {
	box3_t bounds;
	int32_t *list;
	size_t count, length;
	int32_t top_node;
	int32_t contents;
} cm_box_leafnum_data;

/**
 * @brief Recurse the BSP tree from the specified node, accumulating leafs the
 * given box occupies in the data structure.
 */
static void Cm_BoxLeafnums_r(cm_box_leafnum_data *data, int32_t node_num) {

	while (true) {
		if (node_num < 0) {
			const int32_t leaf_num = -1 - node_num;
			data->contents |= cm_bsp.leafs[leaf_num].contents;

			if (data->count < data->length) {
				data->list[data->count++] = leaf_num;
			}

			return;
		}

		const cm_bsp_node_t *node = &cm_bsp.nodes[node_num];
		const cm_bsp_plane_t plane = *node->plane;
		const int32_t side = Cm_BoxOnPlaneSide(data->bounds, &plane);

		if (side == SIDE_FRONT) {
			node_num = node->children[0];
		} else if (side == SIDE_BACK) {
			node_num = node->children[1];
		} else { // go down both
			if (data->top_node == -1) {
				data->top_node = node_num;
			}
			Cm_BoxLeafnums_r(data, node->children[0]);
			node_num = node->children[1];
		}
	}
}

/**
 * @brief Populates the list of leafs the specified bounding box touches. If
 * top_node is not NULL, it will contain the top node of the BSP tree that
 * fully contains the box.
 *
 * @param bounds The bounds in world space.
 * @param list The list of leaf numbers to populate.
 * @param length The maximum number of leafs to return.
 * @param top_node If not null, this will contain the top node for the box.
 * @param head_node The head node to recurse from.
 * @param matrix The matrix by which to transform planes.
 *
 * @return The number of leafs accumulated to the list.
 */
size_t Cm_BoxLeafnums(const box3_t bounds, int32_t *list, size_t length, int32_t *top_node,
					  int32_t head_node) {

	cm_box_leafnum_data data = {
		.bounds = bounds,
		.list = list,
		.length = length,
		.top_node = -1
	};

	if (cm_bsp.num_nodes) {
		Cm_BoxLeafnums_r(&data, head_node);
	}

	if (top_node) {
		*top_node = data.top_node;
	}

	if (data.count == data.length) {
		Com_Debug(DEBUG_COLLISION, "length (%" PRIuPTR ") reached\n", data.length);
	}

	return data.count;
}

/**
 * @brief Contents check for a bounded box.
 * @param bounds The bounding box to check for contents.
 * @param head_node The BSP head node to recurse down.
 * @param matrix The matrix to transform the bounds by.
 * @return The contents mask of all leafs within the transformed bounds.
 */
int32_t Cm_BoxContents(const box3_t bounds, int32_t head_node) {
	cm_box_leafnum_data data = {
		.bounds = bounds,
		.list = NULL,
		.length = 0,
		.top_node = -1
	};

	Cm_BoxLeafnums_r(&data, head_node);

	return data.contents;
}
