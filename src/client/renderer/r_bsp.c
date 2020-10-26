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
 * @return True if the specified leaf is in the PVS for the current frame.
 */
_Bool R_LeafVisible(const r_bsp_leaf_t *leaf) {

	const int32_t cluster = leaf->cluster;

	if (cluster == -1) {
		return false;
	}

	if (r_view.area_bits) {
		if (!(r_view.area_bits[leaf->area >> 3] & (1 << (leaf->area & 7)))) {
			return false;
		}
	}

	return r_locals.vis_data_pvs[cluster >> 3] & (1 << (cluster & 7));
}

/**
 * @return True if the specified leaf is in the PHS for the current frame.
 */
_Bool R_LeafHearable(const r_bsp_leaf_t *leaf) {

	const int32_t cluster = leaf->cluster;

	if (cluster == -1) {
		return false;
	}

	return r_locals.vis_data_phs[cluster >> 3] & (1 << (cluster & 7));
}

/**
 * @return The node behind which the specified point should be rendered for alpha blending.
 */
int32_t R_BlendDepthForPoint(const vec3_t p) {

	const r_bsp_inline_model_t *in = r_world_model->bsp->inline_models;
	for (guint i = 0; i < in->alpha_blend_draw_elements->len; i++) {

		const r_bsp_draw_elements_t *draw = g_ptr_array_index(in->alpha_blend_draw_elements, i);

		if (draw->node->vis_frame != r_locals.vis_frame) {
			continue;
		}

		if (SignOf(Cm_DistanceToPlane(p, draw->node->plane)) !=
			SignOf(Cm_DistanceToPlane(r_view.origin, draw->node->plane))) {

			vec3_t mins, maxs;
			Cm_TraceBounds(r_view.origin, p, Vec3_Zero(), Vec3_Zero(), &mins, &maxs);

			if (Vec3_BoxIntersect(mins, maxs, draw->node->mins, draw->node->maxs)) {
				draw->node->blend_depth_count++;
				
				return draw->node->blend_depth;
			}
		}
	}

	return 0;
}

/**
 * @brief Recurses the specified node, back to front, resolving each node's depth.
 */
static void R_UpdateNodeBlendDepth_r(r_bsp_node_t *node, int32_t *depth) {
	int32_t side;

	if (node->contents != CONTENTS_NODE) {
		return;
	}

	if (node->vis_frame != r_locals.vis_frame) {
		return;
	}

	if (Cm_DistanceToPlane(r_view.origin, node->plane) > 0.f) {
		side = 1;
	} else {
		side = 0;
	}

	R_UpdateNodeBlendDepth_r(node->children[side], depth);

	node->blend_depth = *depth = (*depth) + 1;

	r_bsp_draw_elements_t *draw = node->draw_elements;
	for (int32_t i = 0; i < node->num_draw_elements; i++, draw++) {

		if (draw->texinfo->flags & SURF_MASK_BLEND) {
			g_ptr_array_add(node->model->alpha_blend_draw_elements, draw);
		}
	}

	R_UpdateNodeBlendDepth_r(node->children[!side], depth);
}

/**
 * @brief Recurses the specified model's tree, sorting alpha blended draw elements from back to front.
 */
static void R_UpdateNodeBlendDepth(const r_bsp_inline_model_t *in) {

	g_ptr_array_set_size(in->alpha_blend_draw_elements, 0);

	int32_t depth = 0;

	R_UpdateNodeBlendDepth_r(in->head_node, &depth);
}

/**
 * @brief Resolve the current leaf, PVS and PHS for the view origin.
 */
void R_UpdateVis(void) {

	if (r_lock_vis->value) {
		return;
	}

	r_locals.leaf = R_LeafForPoint(r_view.origin);

	if (r_locals.leaf->cluster == -1 || r_no_vis->integer) {

		memset(r_locals.vis_data_pvs, 0xff, sizeof(r_locals.vis_data_pvs));
		memset(r_locals.vis_data_phs, 0xff, sizeof(r_locals.vis_data_phs));

	} else {

		memset(r_locals.vis_data_pvs, 0x00, sizeof(r_locals.vis_data_pvs));
		memset(r_locals.vis_data_phs, 0x00, sizeof(r_locals.vis_data_phs));

		const vec3_t mins = Vec3_Add(r_view.origin, Vec3(-2.f, -2.f, -4.f));
		const vec3_t maxs = Vec3_Add(r_view.origin, Vec3( 2.f,  2.f,  4.f));

		Cm_BoxPVS(mins, maxs, r_locals.vis_data_pvs);
		Cm_BoxPHS(mins, maxs, r_locals.vis_data_phs);
	}

	r_locals.vis_frame++;

	r_bsp_leaf_t *leaf = r_world_model->bsp->leafs;
	for (int32_t i = 0; i < r_world_model->bsp->num_leafs; i++, leaf++) {

		if (R_LeafVisible(leaf) || r_locals.leaf->cluster == -1) {

			if (R_CullBox(leaf->mins, leaf->maxs)) {
				continue;
			}

			leaf->vis_frame = r_locals.vis_frame;
			r_view.count_bsp_leafs++;

			for (r_bsp_node_t *node = leaf->parent; node; node = node->parent) {

				if (R_CullBox(node->mins, node->maxs)) {
					continue;
				}

				if (node->vis_frame == r_locals.vis_frame) {
					break;
				}

				node->vis_frame = r_locals.vis_frame;
				node->lights_mask = 0;
				node->blend_depth = 0;
				node->blend_depth_count = 0;

				if (node->num_draw_elements) {
					r_view.count_bsp_nodes++;
				}
			}
		}
	}

	R_UpdateNodeBlendDepth(r_world_model->bsp->inline_models);

	const r_entity_t *e = r_view.entities;
	for (int32_t i = 0; i < r_view.num_entities; i++, e++) {

		if (IS_BSP_INLINE_MODEL(e->model)) {

			const r_bsp_inline_model_t *in = e->model->bsp_inline;
			const r_bsp_draw_elements_t *draw = in->draw_elements;
			
			for (int32_t j = 0; j < in->num_draw_elements; j++, draw++) {

				for (r_bsp_node_t *node = draw->node; node; node = node->parent) {

					if (node->vis_frame == r_locals.vis_frame) {
						break;
					}

					node->vis_frame = r_locals.vis_frame;
					node->lights_mask = 0;
					node->blend_depth = 0;
					node->blend_depth_count = 0;
				}
			}

			R_UpdateNodeBlendDepth(in);

			r_view.count_bsp_inline_models++;
		}
	}
}
