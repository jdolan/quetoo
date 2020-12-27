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

#include "r_local.h"

/**
 * @return The leaf for the specified point.
 */
const r_bsp_leaf_t *R_LeafForPoint(const vec3_t p) {

	const int32_t leaf_num = Cm_PointLeafnum(p, 0);

	assert(leaf_num >= 0);

	return &r_world_model->bsp->leafs[leaf_num];
}

/**
 * @return The blend depth at which the specified point should be rendered for alpha blending.
 */
int32_t R_BlendDepthForPoint(const vec3_t p, const r_blend_depth_type_t type) {

	if (r_blend_depth_sorting->value) {

		vec3_t mins, maxs;
		Cm_TraceBounds(r_view.origin, p, Vec3_Zero(), Vec3_Zero(), &mins, &maxs);

		const r_bsp_inline_model_t *in = r_world_model->bsp->inline_models;
		for (guint i = 0; i < in->blend_nodes->len; i++) {

			r_bsp_node_t *node = g_ptr_array_index(in->blend_nodes, i);

			if (SignOf(Cm_DistanceToPlane(p, node->plane)) !=
				SignOf(Cm_DistanceToPlane(r_view.origin, node->plane))) {

				if (Vec3_BoxIntersect(mins, maxs, node->mins, node->maxs)) {
					node->blend_depth_types |= type;
					return node->blend_depth;
				}
			}
		}
	}

	return 0;
}

/**
 * @brief Recurses the specified node, back to front, sorting alpha blended faces.
 * @details The node is transformed by the matrix of the entity to which it belongs,
 * if any, to ensure that alpha blended faces on inline models are sorted correctly.
 */
static void R_UpdateBspInlineModelBlendDepth_r(const r_entity_t *e, r_bsp_node_t *node, int32_t *blend_depth) {
	int32_t side;

	vec3_t mins, maxs;
	if (e) {
		Matrix4x4_Transform(&e->matrix, node->mins.xyz, mins.xyz);
		Matrix4x4_Transform(&e->matrix, node->maxs.xyz, maxs.xyz);
	} else {
		mins = node->mins;
		maxs = node->maxs;
	}

	if (R_CullBox(mins, maxs)) {
		return;
	}

	if (R_OccludeBox(mins, maxs)) {
		return;
	}

	if (node->contents != CONTENTS_NODE) {
		return;
	}

	cm_bsp_plane_t plane;
	if (e) {
		vec4_t out;
		Matrix4x4_TransformQuakePlane(&e->matrix, node->plane->normal, node->plane->dist, &out);
		plane = Cm_Plane(Vec4_XYZ(out), out.w);
	} else {
		plane = *node->plane;
	}

	if (Cm_DistanceToPlane(r_view.origin, &plane) > 0.f) {
		side = 1;
	} else {
		side = 0;
	}

	R_UpdateBspInlineModelBlendDepth_r(e, node->children[side], blend_depth);

	if (node->num_blend_faces && !R_OccludeBox(node->blend_mins, node->blend_maxs)) {

		node->blend_depth = *blend_depth = (*blend_depth) + 1;
		node->blend_depth_types = BLEND_DEPTH_NONE;

		g_ptr_array_add(node->model->blend_nodes, node);
	}

	R_UpdateBspInlineModelBlendDepth_r(e, node->children[!side], blend_depth);
}

/**
 * @brief Recurses the specified model's tree, sorting alpha blended faces from back to front.
 */
static void R_UpdateBspInlineModelBlendDepth(const r_entity_t *e, const r_bsp_inline_model_t *in) {

	g_ptr_array_set_size(in->blend_nodes, 0);

	int32_t blend_depth = 1;
	R_UpdateBspInlineModelBlendDepth_r(e, in->head_node, &blend_depth);
}

/**
 * @brief
 */
void R_UpdateBspInlineEntities(void) {

	const r_bsp_inline_model_t *in = r_world_model->bsp->inline_models;

	R_UpdateBspInlineModelBlendDepth(NULL, in);
	r_view.count_bsp_blend_nodes = in->blend_nodes->len;

	r_entity_t *e = r_view.entities;
	for (int32_t i = 0; i < r_view.num_entities; i++, e++) {

		if (IS_BSP_INLINE_MODEL(e->model)) {
			R_UpdateBspInlineModelBlendDepth(e, e->model->bsp_inline);
			r_view.count_bsp_inline_models++;
		}
	}
}
