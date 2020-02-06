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
 * @brief Returns the leaf for the specified point.
 */
const r_bsp_leaf_t *R_LeafForPoint(const vec3_t p, const r_bsp_model_t *bsp) {

	if (!bsp) {
		bsp = r_model_state.world->bsp;
	}

	return &bsp->leafs[Cm_PointLeafnum(p, 0)];
}

/**
 * @brief Returns true if the specified leaf is in the PVS for the current frame.
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
 * @brief Returns true if the specified leaf is in the PHS for the current frame.
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

	if (!VectorCompare(e->angles, vec3_zero().xyz)) {
		for (int32_t i = 0; i < 3; i++) {
			mins[i] = e->origin[i] - e->model->radius;
			maxs[i] = e->origin[i] + e->model->radius;
		}
	} else {
		VectorAdd(e->origin, e->model->mins, mins);
		VectorAdd(e->origin, e->model->maxs, maxs);
	}

	return R_CullBox(mins, maxs);
}

/**
 * @brief Resolve the current leaf, PVS and PHS for the view origin.
 */
void R_UpdateVis(void) {
	int32_t leafs[MAX_BSP_LEAFS];

	if (r_lock_vis->value) {
		return;
	}

	r_locals.leaf = R_LeafForPoint(r_view.origin, NULL);

	if (r_locals.leaf->cluster == -1 || r_no_vis->integer) {

		memset(r_locals.vis_data_pvs, 0xff, sizeof(r_locals.vis_data_pvs));
		memset(r_locals.vis_data_phs, 0xff, sizeof(r_locals.vis_data_phs));

	} else {

		memset(r_locals.vis_data_pvs, 0x00, sizeof(r_locals.vis_data_pvs));
		memset(r_locals.vis_data_phs, 0x00, sizeof(r_locals.vis_data_phs));

		vec3_t mins, maxs;
		VectorAdd(r_view.origin, ((vec3_t) { -2.f, -2.f, -4.f }), mins);
		VectorAdd(r_view.origin, ((vec3_t) {  2.f,  2.f,  4.f }), maxs);

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
					r_view.count_bsp_nodes++;
				}

				node = node->parent;
			}
		}
	}

	const r_entity_t *e = r_view.entities;
	for (int32_t i = 0; i < r_view.num_entities; i++, e++) {
		if (e->model && e->model->type == MOD_BSP_INLINE) {

			if (R_CullBspEntity(e)) {
				continue;
			}

			const r_bsp_draw_elements_t *draw = e->model->bsp_inline->draw_elements;
			for (int32_t i = 0; i < e->model->bsp_inline->num_draw_elements; i++, draw++) {
				draw->node->vis_frame = r_locals.vis_frame;
			}
		}
	}
}
