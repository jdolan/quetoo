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

	return &r_model_state.world->bsp->leafs[leaf_num];
}

/**
 * @return The node behind which the specified point should be rendered for alpha blending.
 * FIXME: This is called while the scene is being populated, i.e. before R_UpdateVis.
 * FIXME: It is therefore operating on slightly out of date PVS, and might fail in corner cases like teleporting.
 * FIXME: Fixing this will require either a lazy update of particles, sprites, etc, before they are drawn,
 * FIXME: or adding a separate R_UpdateParticles(), R_UpdateSprites, etc.
 */
r_bsp_node_t *R_BlendNodeForPoint(const vec3_t p) {

	const r_bsp_inline_model_t *in = r_model_state.world->bsp->inline_models;
	for (guint i = 0; i < in->alpha_blend_draw_elements->len; i++) {

		const r_bsp_draw_elements_t *draw = g_ptr_array_index(in->alpha_blend_draw_elements, i);

		if (draw->node->vis_frame != r_locals.vis_frame) {
			break;
		}

		if (SignOf(Cm_DistanceToPlane(p, draw->node->plane)) !=
			SignOf(Cm_DistanceToPlane(r_view.origin, draw->node->plane))) {
			return draw->node;
		}
	}

	return NULL;
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
 * @brief
 */
static _Bool R_CullBspEntity(const r_entity_t *e) {
	vec3_t mins, maxs;

	if (!Vec3_Equal(e->angles, Vec3_Zero())) {
		const vec3_t radius = Vec3(e->model->radius,
								   e->model->radius,
								   e->model->radius);

		mins = Vec3_Subtract(e->origin, radius);
		maxs = Vec3_Add(e->origin, radius);

	} else {
		mins = Vec3_Add(e->origin, e->model->mins);
		maxs = Vec3_Add(e->origin, e->model->maxs);
	}

	return R_CullBox(mins, maxs);
}

/**
 * @brief GCompareFunc for sorting draw elements by depth, back to front.
 */
static gint R_DrawElementsDepthCmp(gconstpointer a, gconstpointer b) {

	const r_bsp_draw_elements_t *a_draw = *(r_bsp_draw_elements_t **) a;
	const r_bsp_draw_elements_t *b_draw = *(r_bsp_draw_elements_t **) b;

	if (a_draw->node == b_draw->node) {
		return 0;
	} else {
		if (a_draw->node->vis_frame == r_locals.vis_frame) {
			if (b_draw->node->vis_frame == r_locals.vis_frame) {

				const int32_t a_dist = Cm_DistanceToPlane(r_view.origin, a_draw->node->plane);
				const int32_t b_dist = Cm_DistanceToPlane(r_view.origin, b_draw->node->plane);

				return SignOf(abs(b_dist) - abs(a_dist));
			} else {
				return -1;
			}
		} else if (b_draw->node->vis_frame == r_locals.vis_frame) {
			return 1;
		}
	}

	return 0;
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

		vec3_t mins, maxs;
		mins = Vec3_Add(r_view.origin, Vec3(-2.f, -2.f, -4.f));
		maxs = Vec3_Add(r_view.origin, Vec3( 2.f,  2.f,  4.f));

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

	r_bsp_leaf_t *leaf = r_model_state.world->bsp->leafs;
	for (int32_t i = 0; i < r_model_state.world->bsp->num_leafs; i++, leaf++) {

		if (R_LeafVisible(leaf) || r_locals.leaf->cluster == -1) {

			r_bsp_node_t *node = (r_bsp_node_t *) leaf;
			while (node) {
				
				if (node->vis_frame == r_locals.vis_frame) {
					break;
				}

				if (!R_CullBox(node->mins, node->maxs)) {
					node->vis_frame = r_locals.vis_frame;
					node->lights = 0;
					r_view.count_bsp_nodes++;
				}

				node = node->parent;
			}
		}
	}

	const r_bsp_inline_model_t *in = r_model_state.world->bsp->inline_models;
	g_ptr_array_sort(in->alpha_blend_draw_elements, R_DrawElementsDepthCmp);

	const r_entity_t *e = r_view.entities;
	for (int32_t i = 0; i < r_view.num_entities; i++, e++) {
		if (e->model && e->model->type == MOD_BSP_INLINE) {

			if (R_CullBspEntity(e)) {
				continue;
			}

			in = e->model->bsp_inline;

			const r_bsp_draw_elements_t *draw = in->draw_elements;
			for (int32_t i = 0; i < in->num_draw_elements; i++, draw++) {
				draw->node->vis_frame = r_locals.vis_frame;
			}

			g_ptr_array_sort(in->alpha_blend_draw_elements, R_DrawElementsDepthCmp);
		}
	}
}
