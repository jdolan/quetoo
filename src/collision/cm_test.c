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

	if (normal.x == 1.0 || normal.x == -1.0) {
		return PLANE_X;
	}
	if (normal.y == 1.0 || normal.y == -1.0) {
		return PLANE_Y;
	}
	if (normal.z == 1.0 || normal.z == -1.0) {
		return PLANE_Z;
	}

	const float ax = fabsf(normal.x);
	const float ay = fabsf(normal.y);
	const float az = fabsf(normal.z);

	if (ax >= ay && ax >= az) {
		return PLANE_ANY_X;
	}
	if (ay >= ax && ay >= az) {
		return PLANE_ANY_Y;
	}

	return PLANE_ANY_Z;
}

/**
 * @return A bit mask hinting at the sign of each normal vector component. This
 * can be used to optimize plane side tests.
 */
int32_t Cm_SignBitsForPlane(const cm_bsp_plane_t *p) {
	int32_t bits = 0;

	for (int32_t i = 0; i < 3; i++) {
		if (p->normal.xyz[i] < 0.0) {
			bits |= 1 << i;
		}
	}

	return bits;
}

/**
 * @return The distance from `point` to `plane`.
 */
float Cm_DistanceToPlane(const vec3_t point, const cm_bsp_plane_t *plane) {

	if (AXIAL(plane)) {
		return point.xyz[plane->type] - plane->dist;
	}

	return Vec3_Dot(point, plane->normal) - plane->dist;
}

/**
 * @return The distance from `point` to `plane` from the specified side.
 */
float Cm_DistanceToFace(const vec3_t point, const cm_bsp_plane_t *plane, int32_t plane_side) {

	if (plane_side) {
		return -Cm_DistanceToPlane(point, plane);
	} else {
		return Cm_DistanceToPlane(point, plane);
	}
}

/**
 * @return Non-zero if the bounding boxes intersect, zero otherwise.
 */
int32_t Cm_BoxIntersect(const vec3_t amins, const vec3_t amaxs, const vec3_t bmins, const vec3_t bmaxs) {

	if (amins.x >= bmaxs.x || amins.y >= bmaxs.y || amins.z >= bmaxs.z) {
		return 0;
	}

	if (amaxs.x <= bmins.x || amaxs.y <= bmins.y || amaxs.z <= bmins.z) {
		return 0;
	}

	return 1;
}

/**
 * @return The sidedness of the given bounds relative to the specified plane.
 * If the box straddles the plane, SIDE_BOTH is returned.
 */
int32_t Cm_BoxOnPlaneSide(const vec3_t mins, const vec3_t maxs, const cm_bsp_plane_t *p) {
	float dist1, dist2;

	// axial planes
	if (AXIAL(p)) {
		if (p->dist - SIDE_EPSILON <= mins.xyz[p->type]) {
			return SIDE_FRONT;
		}
		if (p->dist + SIDE_EPSILON >= maxs.xyz[p->type]) {
			return SIDE_BACK;
		}
		return SIDE_BOTH;
	}

	// general case
	switch (p->sign_bits) {
		case 0:
			dist1 = Vec3_Dot(p->normal, maxs);
			dist2 = Vec3_Dot(p->normal, mins);
			break;
		case 1:
			dist1 = p->normal.x * mins.x + p->normal.y * maxs.y + p->normal.z * maxs.z;
			dist2 = p->normal.x * maxs.x + p->normal.y * mins.y + p->normal.z * mins.z;
			break;
		case 2:
			dist1 = p->normal.x * maxs.x + p->normal.y * mins.y + p->normal.z * maxs.z;
			dist2 = p->normal.x * mins.x + p->normal.y * maxs.y + p->normal.z * mins.z;
			break;
		case 3:
			dist1 = p->normal.x * mins.x + p->normal.y * mins.y + p->normal.z * maxs.z;
			dist2 = p->normal.x * maxs.x + p->normal.y * maxs.y + p->normal.z * mins.z;
			break;
		case 4:
			dist1 = p->normal.x * maxs.x + p->normal.y * maxs.y + p->normal.z * mins.z;
			dist2 = p->normal.x * mins.x + p->normal.y * mins.y + p->normal.z * maxs.z;
			break;
		case 5:
			dist1 = p->normal.x * mins.x + p->normal.y * maxs.y + p->normal.z * mins.z;
			dist2 = p->normal.x * maxs.x + p->normal.y * mins.y + p->normal.z * maxs.z;
			break;
		case 6:
			dist1 = p->normal.x * maxs.x + p->normal.y * mins.y + p->normal.z * mins.z;
			dist2 = p->normal.x * mins.x + p->normal.y * maxs.y + p->normal.z * maxs.z;
			break;
		case 7:
			dist1 = Vec3_Dot(p->normal, mins);
			dist2 = Vec3_Dot(p->normal, maxs);
			break;
		default:
			dist1 = dist2 = 0.0; // shut up compiler
			break;
	}

	int32_t sides = 0;

	if (dist1 >= p->dist) {
		sides = SIDE_FRONT;
	}
	if (dist2 < p->dist) {
		sides |= SIDE_BACK;
	}

	return sides;
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
void Cm_InitBoxHull(void) {
	static cm_bsp_texinfo_t null_surface;

	if (cm_bsp.file.num_planes + 12 > MAX_BSP_PLANES) {
		Com_Error(ERROR_DROP, "MAX_BSP_PLANES\n");
	}

	if (cm_bsp.file.num_nodes + 6 > MAX_BSP_NODES) {
		Com_Error(ERROR_DROP, "MAX_BSP_NODES\n");
	}

	if (cm_bsp.file.num_leafs + 1 > MAX_BSP_LEAFS) {
		Com_Error(ERROR_DROP, "MAX_BSP_LEAFS\n");
	}

	if (cm_bsp.file.num_leaf_brushes + 1 > MAX_BSP_LEAF_BRUSHES) {
		Com_Error(ERROR_DROP, "MAX_BSP_LEAF_BRUSHES\n");
	}

	if (cm_bsp.file.num_brushes + 1 > MAX_BSP_BRUSHES) {
		Com_Error(ERROR_DROP, "MAX_BSP_BRUSHES\n");
	}

	if (cm_bsp.file.num_brush_sides + 6 > MAX_BSP_BRUSH_SIDES) {
		Com_Error(ERROR_DROP, "MAX_BSP_BRUSH_SIDES\n");
	}

	// head node
	cm_box.head_node = cm_bsp.file.num_nodes;

	// planes
	cm_box.planes = &cm_bsp.planes[cm_bsp.file.num_planes];

	// leaf
	cm_box.leaf = &cm_bsp.leafs[cm_bsp.file.num_leafs];
	cm_box.leaf->contents = CONTENTS_MONSTER;
	cm_box.leaf->first_leaf_brush = cm_bsp.file.num_leaf_brushes;
	cm_box.leaf->num_leaf_brushes = 1;

	// leaf brush
	cm_bsp.leaf_brushes[cm_bsp.file.num_leaf_brushes] = cm_bsp.file.num_brushes;

	// brush
	cm_box.brush = &cm_bsp.brushes[cm_bsp.file.num_brushes];
	cm_box.brush->num_sides = 6;
	cm_box.brush->first_brush_side = cm_bsp.file.num_brush_sides;
	cm_box.brush->contents = CONTENTS_MONSTER;

	for (int32_t i = 0; i < 6; i++) {

		// fill in planes, two per side
		cm_bsp_plane_t *plane = &cm_box.planes[i * 2];
		plane->type = i >> 1;
		plane->normal = Vec3_Zero();
		plane->normal.xyz[i >> 1] = 1.0;
		plane->sign_bits = Cm_SignBitsForPlane(plane);

		plane = &cm_box.planes[i * 2 + 1];
		plane->type = PLANE_ANY_X + (i >> 1);
		plane->normal = Vec3_Zero();
		plane->normal.xyz[i >> 1] = -1.0;
		plane->sign_bits = Cm_SignBitsForPlane(plane);

		const int32_t side = i & 1;

		// fill in nodes, one per side
		cm_bsp_node_t *node = &cm_bsp.nodes[cm_box.head_node + i];
		node->plane = cm_bsp.planes + (cm_bsp.file.num_planes + i * 2);
		node->children[side] = -1 - cm_bsp.file.num_leafs;
		if (i != 5) {
			node->children[side ^ 1] = cm_box.head_node + i + 1;
		} else {
			node->children[side ^ 1] = -1 - cm_bsp.file.num_leafs;
		}

		// fill in brush sides, one per side
		cm_bsp_brush_side_t *bside = &cm_bsp.brush_sides[cm_bsp.file.num_brush_sides + i];
		bside->plane = cm_bsp.planes + (cm_bsp.file.num_planes + i * 2 + side);
		bside->surface = &null_surface;
	}
}

/**
 * @brief Initializes the box hull for the specified bounds, returning the
 * head node for the resulting box hull tree.
 */
int32_t Cm_SetBoxHull(const vec3_t mins, const vec3_t maxs, const int32_t contents) {

	cm_box.brush->mins = mins;
	cm_box.brush->maxs = maxs;

	cm_box.planes[0].dist = maxs.x;
	cm_box.planes[1].dist = -maxs.x;
	cm_box.planes[2].dist = mins.x;
	cm_box.planes[3].dist = -mins.x;
	cm_box.planes[4].dist = maxs.y;
	cm_box.planes[5].dist = -maxs.y;
	cm_box.planes[6].dist = mins.y;
	cm_box.planes[7].dist = -mins.y;
	cm_box.planes[8].dist = maxs.z;
	cm_box.planes[9].dist = -maxs.z;
	cm_box.planes[10].dist = mins.z;
	cm_box.planes[11].dist = -mins.z;

	cm_box.leaf->contents = cm_box.brush->contents = contents;

	return cm_box.head_node;
}

/**
 * @return The leaf number containing the specified point.
 */
int32_t Cm_PointLeafnum(const vec3_t p, int32_t head_node) {

	if (!cm_bsp.file.num_nodes) {
		return 0;
	}

	int32_t num = head_node;
	while (num >= 0) {
		const cm_bsp_node_t *node = cm_bsp.nodes + num;
		const float dist = Cm_DistanceToPlane(p, node->plane);
		if (dist < 0.0) {
			num = node->children[1];
		} else {
			num = node->children[0];
		}
	}

	return -1 - num;
}

/**
 * @brief Contents check against the world model.
 *
 * @param p The point to check.
 * @param head_node The BSP head node to recurse down.
 *
 * @return The contents mask at the specified point.
 */
int32_t Cm_PointContents(const vec3_t p, int32_t head_node) {

	if (!cm_bsp.file.num_nodes) {
		return 0;
	}

	const int32_t leaf_num = Cm_PointLeafnum(p, head_node);

	return cm_bsp.leafs[leaf_num].contents;
}

/**
 * @brief Contents check for non-world models. Rotates and translates the point
 * into the model's space, and recurses the BSP tree. For inline BSP models,
 * the head node is the root of the model's subtree. For mesh models, a special
 * reserver box hull is used.
 *
 * @param p The point, in world space.
 * @param head_hode The BSP head node to recurse down.
 * @param inverse_matrix The inverse matrix of the entity to be tested.
 *
 * @return The contents mask at the specified point.
 */
int32_t Cm_TransformedPointContents(const vec3_t p, int32_t head_node, const mat4_t *inverse_matrix) {
	vec3_t p0;

	Matrix4x4_Transform(inverse_matrix, p.xyz, p0.xyz);

	return Cm_PointContents(p0, head_node);
}

/**
 * @brief Data binding structure for box to leaf tests.
 */
typedef struct {
	vec3_t mins, maxs;
	int32_t *list;
	size_t len, max_len;
	int32_t top_node;
} cm_box_leafnum_data;

/**
 * @brief Recurse the BSP tree from the specified node, accumulating leafs the
 * given box occupies in the data structure.
 */
static void Cm_BoxLeafnums_r(cm_box_leafnum_data *data, int32_t node_num) {

	while (true) {
		if (node_num < 0) {
			if (data->len < data->max_len) {
				data->list[data->len++] = -1 - node_num;
			}
			return;
		}

		const cm_bsp_node_t *node = &cm_bsp.nodes[node_num];
		const cm_bsp_plane_t *plane = node->plane;

		const int32_t side = Cm_BoxOnPlaneSide(data->mins, data->maxs, plane);

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
 * @param mins The box mins in world space.
 * @param maxs The box maxs in world space.
 * @param list The list of leaf numbers to populate.
 * @param len The maximum number of leafs to return.
 * @param top_node If not null, this will contain the top node for the box.
 * @param head_node The head node to recurse from.
 *
 * @return The number of leafs accumulated to the list.
 */
size_t Cm_BoxLeafnums(const vec3_t mins, const vec3_t maxs, int32_t *list, size_t len,
                      int32_t *top_node, int32_t head_node) {

	cm_box_leafnum_data data;

	data.mins = mins;
	data.maxs = maxs;
	data.list = list;
	data.len = 0;
	data.max_len = len;
	data.top_node = -1;

	Cm_BoxLeafnums_r(&data, head_node);

	if (top_node) {
		*top_node = data.top_node;
	}

	if (data.len == data.max_len) {
		Com_Debug(DEBUG_COLLISION, "max_len (%" PRIuPTR ") reached\n", data.max_len);
	}

	return data.len;
}
