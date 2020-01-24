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

static vec3_t r_bsp_model_org; // relative to r_view.origin

/**
 * @brief Returns true if the specified bounding box is completely culled by the
 * view frustum, false otherwise.
 */
_Bool R_CullBox(const vec3_t mins, const vec3_t maxs) {
	int32_t i;

	if (!r_cull->value) {
		return false;
	}

	for (i = 0; i < 4; i++) {
		if (Cm_BoxOnPlaneSide(mins, maxs, &r_locals.frustum[i]) != SIDE_BACK) {
			return false;
		}
	}

	return true;
}

/**
 * @brief Returns true if the specified sphere (point and radius) is completely culled by the
 * view frustum, false otherwise.
 */
_Bool R_CullSphere(const vec3_t point, const vec_t radius) {

	if (!r_cull->value) {
		return false;
	}

	for (int32_t i = 0 ; i < 4 ; i++)  {
		const cm_bsp_plane_t *p = &r_locals.frustum[i];
		const vec_t dist = DotProduct(point, p->normal) - p->dist;

		if (dist < -radius) {
			return true;
		}
	}

	return false;
}

/**
 * @brief Returns true if the specified entity is completely culled by the view
 * frustum, false otherwise.
 */
_Bool R_CullBspInlineModel(const r_entity_t *e) {
	vec3_t mins, maxs;
	int32_t i;

	if (!e->model->bsp_inline->num_faces) { // no surfaces
		return true;
	}

	if (e->angles[0] || e->angles[1] || e->angles[2]) {
		for (i = 0; i < 3; i++) {
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
 * @brief Rotates the frame's light sources into the model's space and recurses
 * down the model's tree. Surfaces that should receive light are marked so that
 * the draw routines will enable the lights. This must be called with NULL to
 * restore light origins after the model has been drawn.
 */
static void R_RotateLightsForBspInlineModel(const r_entity_t *e) {
//	static vec3_t light_origins[MAX_LIGHTS];
//	static int16_t frame;
//
//	// for each frame, backup the light origins
//	if (frame != r_locals.frame) {
//		for (uint16_t i = 0; i < r_view.num_lights; i++) {
//			VectorCopy(r_view.lights[i].origin, light_origins[i]);
//		}
//		frame = r_locals.frame;
//	}
//
//	// for malformed inline models, simply return
//	if (e && e->model->bsp_inline->head_node == -1) {
//		return;
//	}
//
//	// for well-formed models, iterate the lights, transforming them into model
//	// space and marking surfaces, or restoring them if the model is NULL
//
//	const r_bsp_node_t *nodes = r_model_state.world->bsp->nodes;
//	r_light_t *l = r_view.lights;
//
//	for (uint16_t i = 0; i < r_view.num_lights; i++, l++) {
//		if (e) {
//			Matrix4x4_Transform(&e->inverse_matrix, light_origins[i], l->origin);
//			R_MarkLight(l, nodes + e->model->bsp_inline->head_node);
//		} else {
//			VectorCopy(light_origins[i], l->origin);
//		}
//	}
}

/**
 * @brief Draws all BSP surfaces for the specified entity. This is a condensed
 * version of the world drawing routine that relies on setting the view frame
 * counter to negative to safely iterate the sorted surfaces arrays.
 */
static void R_DrawBspInlineModel_(const r_entity_t *e) {
//	static int16_t frame = -1;
//
//	// temporarily swap the view frame so that the surface drawing
//	// routines pickup only the inline model's surfaces
//
//	const int16_t f = r_locals.frame;
//	r_locals.frame = frame--;
//
//	if (frame == INT16_MIN) {
//		frame = -1;
//	}
//
//	r_bsp_face_t *surf = &r_model_state.world->bsp->surfaces[e->model->bsp_inline->first_surface];
//
//	for (int32_t i = 0; i < e->model->bsp_inline->num_faces; i++, surf++) {
//
//		const vec_t dist = R_DistanceToSurface(r_bsp_model_org, surf);
//		if (dist > SIDE_EPSILON) { // visible, flag for rendering
//			surf->frame = r_locals.frame;
//		}
//	}
//
//	const r_sorted_bsp_faces_t *surfs = r_model_state.world->bsp->sorted_surfaces;
//
//	R_DrawOpaqueBspFaces(&surfs->opaque);
//
//	R_DrawOpaqueWarpBspFaces(&surfs->opaque_warp);
//
//	R_DrawAlphaTestBspFaces(&surfs->alpha_test);
//
//	R_EnableBlend(true);
//
//	R_EnableDepthMask(false);
//
//	R_DrawMaterialBspFaces(&surfs->material);
//
//	R_DrawBlendBspFaces(&surfs->blend);
//
//	R_DrawBlendWarpBspFaces(&surfs->blend_warp);
//
//	R_EnableBlend(false);
//
//	R_EnableDepthMask(true);
//
//	r_locals.frame = f; // undo the swap
}

/**
 * @brief Draws the BSP model for the specified entity, taking translation and
 * rotation into account.
 */
static void R_DrawBspInlineModel(const r_entity_t *e) {

	// set the relative origin, accounting for rotation if necessary
	VectorSubtract(r_view.origin, e->origin, r_bsp_model_org);
	if (e->angles[0] || e->angles[1] || e->angles[2]) {
		vec3_t forward, right, up;
		vec3_t temp;

		VectorCopy(r_bsp_model_org, temp);
		AngleVectors(e->angles, forward, right, up);

		r_bsp_model_org[0] = DotProduct(temp, forward);
		r_bsp_model_org[1] = -DotProduct(temp, right);
		r_bsp_model_org[2] = DotProduct(temp, up);
	}

	R_RotateLightsForBspInlineModel(e);

	R_RotateForEntity(e);

	R_DrawBspInlineModel_(e);

	R_RotateForEntity(NULL);

	R_RotateLightsForBspInlineModel(NULL);
}

/**
 * @brief
 */
static void R_AddBspInlineModelFlares_(const r_entity_t *e) {
//	static int16_t frame = -1;
//
//	// temporarily swap the view frame so that the surface drawing
//	// routines pickup only the inline model's surfaces
//
//	const int16_t f = r_locals.frame;
//	r_locals.frame = frame--;
//
//	if (frame == INT16_MIN) {
//		frame = -1;
//	}
//
//	r_bsp_face_t *surf = &r_model_state.world->bsp->surfaces[e->model->bsp_inline->first_surface];
//
//	for (int32_t i = 0; i < e->model->bsp_inline->num_faces; i++, surf++) {
//		const vec_t dist = R_DistanceToSurface(r_bsp_model_org, surf);
//		if (dist > SIDE_EPSILON) { // visible, flag for rendering
//			surf->frame = r_locals.frame;
//		}
//	}
//
//	const r_sorted_bsp_faces_t *surfs = r_model_state.world->bsp->sorted_surfaces;
//
//	R_AddFlareBspFaces(&surfs->flare);
//
//	r_locals.frame = f; // undo the swap
}

/**
 * @brief
 */
void R_AddBspInlineModelFlares(const r_entities_t *ents) {

	for (size_t i = 0; i < ents->count; i++) {
		const r_entity_t *e = ents->entities[i];

		if (e->effects & EF_NO_DRAW) {
			continue;
		}

//		r_view.current_entity = e;

		R_AddBspInlineModelFlares_(e);
	}

//	r_view.current_entity = NULL;
}

/**
 * @brief
 */
void R_DrawBspInlineModels(const r_entities_t *ents) {

	for (size_t i = 0; i < ents->count; i++) {
		const r_entity_t *e = ents->entities[i];

		if (e->effects & EF_NO_DRAW) {
			continue;
		}

//		r_view.current_entity = e;

		R_DrawBspInlineModel(e);
	}

//	r_view.current_entity = NULL;
}

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

			leaf->lights = 0;

			r_bsp_node_t *node = (r_bsp_node_t *) leaf;
			while (node) {
				if (node->vis_frame == r_locals.vis_frame) {
					break;
				}
				node->vis_frame = r_locals.vis_frame;
				node = node->parent;
			}
		}
	}
}
