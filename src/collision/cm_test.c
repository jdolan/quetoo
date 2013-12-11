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
 * @brief Bounding box to BSP tree structure for box positional testing.
 */
typedef struct {
	int32_t head_node;
	cm_bsp_plane_t *planes;
	cm_bsp_brush_t *brush;
	cm_bsp_leaf_t *leaf;
} cm_box_t;

static cm_box_t cm_box;

/*
 * @brief Appends a brush (6 nodes, 12 planes) opaquely to the primary BSP
 * structure to represent the bounding box used for Cm_BoxLeafnums. This brush
 * is never tested by the rest of the collision detection code, as it resides
 * just beyond the parsed size of the map.
 */
void Cm_InitBoxHull(void) {
	int32_t i;

	if (cm_bsp.num_planes + 12 > MAX_BSP_PLANES)
		Com_Error(ERR_DROP, "MAX_BSP_PLANES\n");

	if (cm_bsp.num_nodes + 6 > MAX_BSP_NODES)
		Com_Error(ERR_DROP, "MAX_BSP_NODES\n");

	if (cm_bsp.num_leafs + 1 > MAX_BSP_LEAFS)
		Com_Error(ERR_DROP, "MAX_BSP_LEAFS\n");

	if (cm_bsp.num_leaf_brushes + 1 > MAX_BSP_LEAF_BRUSHES)
		Com_Error(ERR_DROP, "MAX_BSP_LEAF_BRUSHES\n");

	if (cm_bsp.num_brushes + 1 > MAX_BSP_BRUSHES)
		Com_Error(ERR_DROP, "MAX_BSP_BRUSHES\n");

	if (cm_bsp.num_brush_sides + 6 > MAX_BSP_BRUSH_SIDES)
		Com_Error(ERR_DROP, "MAX_BSP_BRUSH_SIDES\n");

	// head node
	cm_box.head_node = cm_bsp.num_nodes;

	// planes
	cm_box.planes = &cm_bsp.planes[cm_bsp.num_planes];

	// leaf
	cm_box.leaf = &cm_bsp.leafs[cm_bsp.num_leafs];
	cm_box.leaf->contents = CONTENTS_MONSTER;
	cm_box.leaf->first_leaf_brush = cm_bsp.num_leaf_brushes;
	cm_box.leaf->num_leaf_brushes = 1;

	// leaf brush
	cm_bsp.leaf_brushes[cm_bsp.num_leaf_brushes] = cm_bsp.num_brushes;

	// brush
	cm_box.brush = &cm_bsp.brushes[cm_bsp.num_brushes];
	cm_box.brush->num_sides = 6;
	cm_box.brush->first_brush_side = cm_bsp.num_brush_sides;
	cm_box.brush->contents = CONTENTS_MONSTER;

	for (i = 0; i < 6; i++) {

		// fill in planes, two per side
		cm_bsp_plane_t *plane = &cm_box.planes[i * 2];
		plane->type = i >> 1;
		plane->sign_bits = 0;
		VectorClear(plane->normal);
		plane->normal[i >> 1] = 1.0;

		plane = &cm_box.planes[i * 2 + 1];
		plane->type = PLANE_ANY_X + (i >> 1);
		plane->sign_bits = 0;
		VectorClear(plane->normal);
		plane->normal[i >> 1] = -1.0;

		const int32_t side = i & 1;

		// fill in nodes, one per side
		cm_bsp_node_t *node = &cm_bsp.nodes[cm_box.head_node + i];
		node->plane = cm_bsp.planes + (cm_bsp.num_planes + i * 2);
		node->children[side] = -1 - cm_bsp.empty_leaf;
		if (i < 5)
			node->children[side ^ 1] = cm_box.head_node + i + 1;
		else
			node->children[side ^ 1] = -1 - cm_bsp.num_leafs;

		// fill in brush sides, one per side
		cm_bsp_brush_side_t *bside = &cm_bsp.brush_sides[cm_bsp.num_brush_sides + i];
		bside->plane = cm_bsp.planes + (cm_bsp.num_planes + i * 2 + side);
		bside->surface = &cm_bsp.null_surface;
	}
}

/*
 * @brief Initializes the box hull for the specified bounds, returning the
 * head node for the resulting box hull tree.
 */
int32_t Cm_SetBoxHull(const vec3_t mins, const vec3_t maxs) {

	cm_box.planes[0].dist = maxs[0];
	cm_box.planes[1].dist = -maxs[0];
	cm_box.planes[2].dist = mins[0];
	cm_box.planes[3].dist = -mins[0];
	cm_box.planes[4].dist = maxs[1];
	cm_box.planes[5].dist = -maxs[1];
	cm_box.planes[6].dist = mins[1];
	cm_box.planes[7].dist = -mins[1];
	cm_box.planes[8].dist = maxs[2];
	cm_box.planes[9].dist = -maxs[2];
	cm_box.planes[10].dist = mins[2];
	cm_box.planes[11].dist = -mins[2];

	return cm_box.head_node;
}

/*
 * @brief
 */
static int32_t Cm_PointLeafnum_r(const vec3_t p, int32_t num) {

	while (num >= 0) {
		const cm_bsp_node_t *node = cm_bsp.nodes + num;
		cm_bsp_plane_t *plane = node->plane;

		vec_t d;
		if (AXIAL(plane))
			d = p[plane->type] - plane->dist;
		else
			d = DotProduct(plane->normal, p) - plane->dist;

		if (d < 0.0)
			num = node->children[1];
		else
			num = node->children[0];
	}

	return -1 - num;
}

/*
 * @return The leaf number containing the specified point.
 */
int32_t Cm_PointLeafnum(const vec3_t p, int32_t head_node) {

	if (!cm_bsp.num_nodes)
		return 0;

	return Cm_PointLeafnum_r(p, head_node);
}

/*
 * @brief Contents check against the world model.
 *
 * @param p The point to check.
 * @param head_node The BSP head node to recurse down.
 *
 * @return The contents mask at the specified point.
 */
int32_t Cm_PointContents(const vec3_t p, int32_t head_node) {

	if (!cm_bsp.num_nodes)
		return 0;

	const int32_t leaf_num = Cm_PointLeafnum(p, head_node);

	return cm_bsp.leafs[leaf_num].contents;
}

/*
 * @brief Contents check for non-world models. Rotates and translates the point
 * into the model's space, and recurses the BSP tree. For inline BSP models,
 * the head node is the root of the model's subtree. For mesh models, a special
 * reserver box hull is used.
 *
 * @param p The point, in world space.
 * @param head_hode The BSP head node to recurse down.
 * @param origin The origin of the entity to be clipped against.
 * @param angles The angles of the entity to be clipped against.
 *
 * @return The contents mask at the specified point.
 */
int32_t Cm_TransformedPointContents(const vec3_t p, int32_t head_node, const vec3_t origin,
		const vec3_t angles) {
	vec3_t p0;

	// subtract origin offset
	VectorSubtract(p, origin, p0);

	// rotate start and end into the models frame of reference
	if (head_node != cm_box.head_node && (angles[0] || angles[1] || angles[2])) {
		vec3_t forward, right, up, temp;

		AngleVectors(angles, forward, right, up);

		VectorCopy(p0, temp);
		p0[0] = DotProduct(temp, forward);
		p0[1] = -DotProduct(temp, right);
		p0[2] = DotProduct(temp, up);
	}

	return Cm_PointContents(p0, head_node);
}

/*
 * @brief Data binding structure for box to leaf tests.
 */
typedef struct {
	const vec_t *mins, *maxs;
	int32_t *list;
	size_t len, max_len;
	int32_t top_node;
} cm_box_leafnum_data;

/*
 * @brief Recurse the BSP tree from the specified node, accumulating leafs the
 * given box occupies in the data structure.
 */
static void Cm_BoxLeafnums_r(cm_box_leafnum_data *data, int32_t node_num) {

	while (true) {
		if (node_num < 0) {
			if (data->len >= data->max_len) {
				return;
			}
			data->list[data->len++] = -1 - node_num;
			return;
		}

		const cm_bsp_node_t *node = &cm_bsp.nodes[node_num];
		const cm_bsp_plane_t *plane = node->plane;

		const int32_t s = BoxOnPlaneSide(data->mins, data->maxs, plane);

		if (s == 1)
			node_num = node->children[0];
		else if (s == 2)
			node_num = node->children[1];
		else { // go down both
			if (data->top_node == -1)
				data->top_node = node_num;
			Cm_BoxLeafnums_r(data, node->children[0]);
			node_num = node->children[1];
		}
	}
}

/*
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
int32_t Cm_BoxLeafnums(const vec3_t mins, const vec3_t maxs, int32_t *list, size_t len,
		int32_t *top_node, int32_t head_node) {

	cm_box_leafnum_data data;

	data.mins = mins;
	data.maxs = maxs;
	data.list = list;
	data.len = 0;
	data.max_len = len;
	data.top_node = -1;

	Cm_BoxLeafnums_r(&data, head_node);

	if (top_node)
		*top_node = data.top_node;

	return data.len;
}
