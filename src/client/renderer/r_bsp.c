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
int32_t R_BlendDepthForPoint(r_view_t *view, const vec3_t p, const r_blend_depth_type_t type) {

	if (r_blend_depth_sorting->value) {

		vec3_t mins, maxs;
		Cm_TraceBounds(view->origin, p, Vec3_Zero(), Vec3_Zero(), &mins, &maxs);

		const r_bsp_inline_model_t *in = r_world_model->bsp->inline_models;
		for (guint i = 0; i < in->blend_elements->len; i++) {

			r_bsp_draw_elements_t *draw = g_ptr_array_index(in->blend_elements, i);

			if (draw->texinfo->flags & SURF_DECAL) {
				continue;
			}

			if (Vec3_BoxIntersect(mins, maxs, draw->mins, draw->maxs)) {
				if (Cm_DistanceToPlane(p, draw->plane->cm) < 0.f) {

					draw->blend_depth_types |= type;

					return (int32_t) (draw - r_world_model->bsp->draw_elements);
				}
			}
		}
	}

	return -1;
}

/**
 * @brief Recurses the specified node, back to front, sorting alpha blended draw elements.
 * @details The node is transformed by the matrix of the entity to which it belongs, if any,
 * to ensure that alpha blended elements on inline models are visible, and sorted correctly.
 */
static void R_UpdateBspInlineModelBlendDepth_r(r_view_t *view,
											   const r_entity_t *e,
											   const r_bsp_inline_model_t *in,
											   r_bsp_node_t *node) {

	if (node->contents != CONTENTS_NODE) {
		return;
	}

	vec3_t transformed_mins, transformed_maxs;
	if (e) {
		Matrix4x4_Transform(&e->matrix, node->mins.xyz, transformed_mins.xyz);
		Matrix4x4_Transform(&e->matrix, node->maxs.xyz, transformed_maxs.xyz);
	} else {
		transformed_mins = node->mins;
		transformed_maxs = node->maxs;
	}

	if (R_OccludeBox(view, transformed_mins, transformed_maxs)) {
		return;
	}

	if (R_CullBox(view, transformed_mins, transformed_maxs)) {
		return;
	}

	r_bsp_plane_t *plane = node->plane;

	cm_bsp_plane_t transformed_plane;
	if (e) {
		transformed_plane = Cm_TransformPlane(&e->matrix, plane->cm);
	} else {
		transformed_plane = *plane->cm;
	}

	int32_t back_side;
	if (Cm_DistanceToPlane(view->origin, &transformed_plane) > 0.f) {
		back_side = 1;
	} else {
		back_side = 0;
	}

	R_UpdateBspInlineModelBlendDepth_r(view, e, in, node->children[back_side]);

	for (guint i = 0; i < plane->blend_elements->len; i++) {
		r_bsp_draw_elements_t *draw = g_ptr_array_index(plane->blend_elements, i);

		if (draw->plane_side == back_side) {
			continue;
		}

		if (!Vec3_BoxIntersect(draw->mins, draw->maxs, node->mins, node->maxs)) {
			continue;
		}

		if (g_ptr_array_find(in->blend_elements, draw, NULL)) {
			continue;
		}

		draw->blend_depth_types = BLEND_DEPTH_NONE;
		g_ptr_array_add(in->blend_elements, draw);
	}

	R_UpdateBspInlineModelBlendDepth_r(view, e, in, node->children[!back_side]);
}

/**
 * @brief Recurses the specified model's tree, sorting alpha blended faces from back to front.
 */
static void R_UpdateBspInlineModelBlendDepth(r_view_t *view,
											 const r_entity_t *e,
											 const r_bsp_inline_model_t *in) {

	g_ptr_array_set_size(in->blend_elements, 0);

	R_UpdateBspInlineModelBlendDepth_r(view, e, in, in->head_node);
}

/**
 * @brief
 */
void R_UpdateBlendDepth(r_view_t *view) {

	const r_bsp_inline_model_t *in = r_world_model->bsp->inline_models;

	R_UpdateBspInlineModelBlendDepth(view, NULL, in);

	r_entity_t *e = view->entities;
	for (int32_t i = 0; i < view->num_entities; i++, e++) {
		if (IS_BSP_INLINE_MODEL(e->model)) {
			R_UpdateBspInlineModelBlendDepth(view, e, e->model->bsp_inline);
		}
	}
}
