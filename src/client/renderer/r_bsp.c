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

	const cm_bsp_plane_t *plane = NULL;

	const r_bsp_inline_model_t *in = r_world_model->bsp->inline_models;
	for (guint i = 0; i < in->alpha_blend_draw_elements->len; i++) {

		const r_bsp_draw_elements_t *draw = g_ptr_array_index(in->alpha_blend_draw_elements, i);

		if (draw->node->vis_frame != r_locals.vis_frame) {
			continue;
		}

		if (draw->node->plane != plane) {
			plane = draw->node->plane;

			if (SignOf(Cm_DistanceToPlane(p, plane)) !=
				SignOf(Cm_DistanceToPlane(r_view.origin, plane))) {

				return draw->node->blend_depth;
			}
		}
	}

	return 0;
}

/**
 * @brief GCompareFunc for sorting draw elements by blend depth (desc) and then material (asc).
 */
static gint R_DrawElementsDepthCmp(gconstpointer a, gconstpointer b) {

	const r_bsp_draw_elements_t *a_draw = *(r_bsp_draw_elements_t **) a;
	const r_bsp_draw_elements_t *b_draw = *(r_bsp_draw_elements_t **) b;

	gint order = b_draw->node->blend_depth - a_draw->node->blend_depth;
	if (order == 0) {

		order = strcmp(a_draw->texinfo->texture, b_draw->texinfo->texture);
		if (order == 0) {

			const gint a_flags = (a_draw->texinfo->flags & SURF_MASK_TEXINFO_CMP);
			const gint b_flags = (b_draw->texinfo->flags & SURF_MASK_TEXINFO_CMP);

			order = a_flags - b_flags;
		}
	}

	return order;
}

/**
 * @brief Recurses the specified node, front to back, resolving each node's depth.
 */
static void R_UpdateNodeDepth_r(r_bsp_node_t *node, int32_t *depth) {
	int32_t side;

	if (node->contents != CONTENTS_NODE) {
		return;
	}

	if (node->vis_frame != r_locals.vis_frame) {
		return;
	}

	if (Cm_DistanceToPlane(r_view.origin, node->plane) > 0.f) {
		side = 0;
	} else {
		side = 1;
	}

	R_UpdateNodeDepth_r(node->children[side], depth);

	node->blend_depth = *depth = (*depth) + 1;

	R_UpdateNodeDepth_r(node->children[!side], depth);
}

/**
 * @brief Recurses the specified model's tree, sorting alpha blended draw elements from back to front.
 */
static void R_UpdateNodeDepth(r_bsp_inline_model_t *in) {

	int32_t depth = 0;

	R_UpdateNodeDepth_r(in->head_node, &depth);

	g_ptr_array_sort(in->alpha_blend_draw_elements, R_DrawElementsDepthCmp);
}

/**
 * @brief Resolve the current leaf, PVS and PHS for the view origin.
 */
void R_UpdateVis(void) {
	int32_t leafs[MAX_BSP_LEAFS];

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

		const size_t count = Cm_BoxLeafnums(mins, maxs, leafs, lengthof(leafs), NULL, 0);
		for (size_t i = 0; i < count; i++) {

			const int32_t cluster = Cm_LeafCluster(leafs[i]);
			if (cluster != -1) {
				byte pvs[MAX_BSP_LEAFS >> 3];
				byte phs[MAX_BSP_LEAFS >> 3];

				Cm_ClusterPVS(cluster, pvs);
				Cm_ClusterPHS(cluster, phs);

				for (size_t i = 0; i < sizeof(r_locals.vis_data_pvs); i++) {
					r_locals.vis_data_pvs[i] |= pvs[i];
					r_locals.vis_data_phs[i] |= phs[i];
				}
			}
		}
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
				node->lights_mask = node->blend_depth = 0;

				r_view.count_bsp_nodes++;
			}
		}
	}

	R_UpdateNodeDepth(r_world_model->bsp->inline_models);

	const r_entity_t *e = r_view.entities;
	for (int32_t i = 0; i < r_view.num_entities; i++, e++) {
		if (IS_BSP_INLINE_MODEL(e->model)) {

			r_bsp_inline_model_t *in = e->model->bsp_inline;

			const r_bsp_draw_elements_t *draw = in->draw_elements;
			for (int32_t i = 0; i < in->num_draw_elements; i++, draw++) {

				draw->node->vis_frame = r_locals.vis_frame;
				draw->node->lights_mask = draw->node->blend_depth = 0;
			}

			R_UpdateNodeDepth(in);
		}
	}
}
