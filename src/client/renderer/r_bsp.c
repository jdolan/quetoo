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
			r_view.cull_fails++;
			return false;
		}
	}

	r_view.cull_passes++;
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
			r_view.cull_passes++;
			return true;
		}
	}

	r_view.cull_fails++;
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
	static vec3_t light_origins[MAX_LIGHTS];
	static int16_t frame;

	// for each frame, backup the light origins
	if (frame != r_locals.frame) {
		for (uint16_t i = 0; i < r_view.num_lights; i++) {
			VectorCopy(r_view.lights[i].origin, light_origins[i]);
		}
		frame = r_locals.frame;
	}

	// for malformed inline models, simply return
	if (e && e->model->bsp_inline->head_node == -1) {
		return;
	}

	// for well-formed models, iterate the lights, transforming them into model
	// space and marking surfaces, or restoring them if the model is NULL

	const r_bsp_node_t *nodes = r_model_state.world->bsp->nodes;
	r_light_t *l = r_view.lights;

	for (uint16_t i = 0; i < r_view.num_lights; i++, l++) {
		if (e) {
			Matrix4x4_Transform(&e->inverse_matrix, light_origins[i], l->origin);
			R_MarkLight(l, nodes + e->model->bsp_inline->head_node);
		} else {
			VectorCopy(light_origins[i], l->origin);
		}
	}
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

		r_view.current_entity = e;

		R_AddBspInlineModelFlares_(e);
	}

	r_view.current_entity = NULL;
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

		r_view.current_entity = e;

		R_DrawBspInlineModel(e);
	}

	r_view.current_entity = NULL;
}

/**
 * @brief Developer tool for viewing BSP vertex normals. Only Phong-interpolated
 * surfaces show their normals when r_draw_bsp_normals is 2.
 */
void R_DrawBspNormals(void) {

	if (!r_draw_bsp_normals->value) {
		return;
	}

//	R_EnableColorArray(true);
//
//	R_BindDiffuseTexture(r_image_state.null->texnum);
//
//	R_ResetArrayState(); // default arrays
//
//	R_BindAttributeInterleaveBuffer(&r_model_state.bound_vertice_buffer, R_ATTRIB_MASK_ALL);
//
//	R_BindAttributeBuffer(R_ATTRIB_ELEMENTS, &r_model_state.bound_element_buffer);
//
//	const r_bsp_face_t *surf = r_model_state.world->bsp->surfaces;
//
//	static matrix4x4_t mat, modelview;
//
//	R_GetMatrix(R_MATRIX_MODELVIEW, &modelview);
//
//	for (uint16_t i = 0; i < r_model_state.world->bsp->num_faces; i++, surf++) {
//
//		if (surf->vis_frame != r_locals.vis_frame) {
//			continue;    // not visible
//		}
//
//		if (surf->texinfo->flags & (SURF_SKY | SURF_WARP)) {
//			continue;    // don't care
//		}
//
//		if ((r_draw_bsp_normals->integer & 2) && !(surf->texinfo->flags & SURF_PHONG)) {
//			continue;    // don't care
//		}
//
//		const r_bsp_vertex_t *v = r_model_state.world->bsp->vertexes + surf->first_vertex;
//		for (uint16_t j = 0; j < surf->num_vertexes; j++, v++) {
//
//			vec3_t angles;
//			VectorAngles(v->normal, angles);
//			Matrix4x4_CreateFromQuakeEntity(&mat,
//											v->position[0], v->position[1], v->position[2],
//											angles[0], angles[1], angles[2],
//											1.0);
//
//			Matrix4x4_Concat(&mat, &modelview, &mat);
//
//			R_SetMatrix(R_MATRIX_MODELVIEW, &mat);
//
//			R_DrawArrays(GL_LINES, (GLint) r_model_state.bound_element_count - 6, 2);
//		}
//	}
//
//	R_SetMatrix(R_MATRIX_MODELVIEW, &modelview);
//
//	R_EnableTexture(texunit_diffuse, true);
//
//	R_EnableColorArray(false);
}

/**
 * @brief Developer tool for viewing BSP leafs and clusters.
 */
void R_DrawBspLeafs(void) {
//	const vec4_t leaf_colors[] = { // assign each leaf a color
//		{ 0.2, 0.2, 0.2, 0.4 },
//		{ 0.8, 0.2, 0.2, 0.4 },
//		{ 0.2, 0.8, 0.2, 0.4 },
//		{ 0.2, 0.2, 0.8, 0.4 },
//		{ 0.8, 0.8, 0.2, 0.4 },
//		{ 0.2, 0.8, 0.8, 0.4 },
//		{ 0.8, 0.2, 0.8, 0.4 },
//		{ 0.8, 0.8, 0.8, 0.4 }
//	};

	if (!r_draw_bsp_leafs->value) {
		return;
	}
//
//	R_SetArrayState(r_model_state.world);
//
//	R_EnableTexture(texunit_diffuse, false);
//
//	R_BindDiffuseTexture(r_image_state.null->texnum);
//
//	R_EnablePolygonOffset(true);
//
//	R_PolygonOffset(R_OFFSET_FACTOR, R_OFFSET_UNITS);
//
//	const r_bsp_leaf_t *l = r_model_state.world->bsp->leafs;
//
//	for (uint16_t i = 0; i < r_model_state.world->bsp->num_leafs; i++, l++) {
//
//		if (l->vis_frame != r_locals.vis_frame) {
//			continue;
//		}
//
//		if (r_draw_bsp_leafs->integer == 2) {
//			R_Color(leaf_colors[l->cluster % lengthof(leaf_colors)]);
//		} else {
//			R_Color(leaf_colors[i % lengthof(leaf_colors)]);
//		}
//
//		r_bsp_face_t **s = l->first_leaf_surface;
//
//		for (uint16_t j = 0; j < l->num_leaf_faces; j++, s++) {
//
//			if ((*s)->vis_frame != r_locals.vis_frame) {
//				continue;
//			}
//
//			R_DrawArrays(GL_TRIANGLES, (*s)->first_element, (*s)->num_elements);
//		}
//	}
//
//	R_EnablePolygonOffset(false);
//
//	R_EnableTexture(texunit_diffuse, true);
//
//	R_Color(NULL);
}

/**
 * @brief Developer tool for viewing static BSP light sources.
 */
void R_DrawBspLights(void) {

	if (!r_draw_bsp_lights->value) {
		return;
	}

	r_bsp_light_t *light = r_model_state.world->bsp->lights;
	for (int32_t i = 0; i < r_model_state.world->bsp->num_lights; i++, light++) {

		if (light->type < LIGHT_POINT) {
			continue;
		}
		
		if (light->leaf->vis_frame != r_locals.vis_frame) {
			continue;
		}

		R_AddParticle(&light->debug);
	}
}

/**
 * @brief Top-down BSP node recursion. Nodes identified as within the PVS by
 * R_MarkLeafs are first frustum-culled; those which fail immediately
 * return.
 *
 * For the rest, the front-side child node is recursed. Any surfaces marked
 * in that recursion must then pass a dot-product test to resolve sidedness.
 * Finally, the back-side child node is recursed.
 */
static void R_MarkBspFaces_(r_bsp_node_t *node) {
	int32_t side;

	if (node->contents == CONTENTS_SOLID) {
		return;    // solid
	}

	if (node->vis_frame != r_locals.vis_frame) {
		return;    // not in pvs
	}

	if (R_CullBox(node->mins, node->maxs)) {
		return;    // culled out
	}

	// if leaf node, flag surfaces to draw this frame
	if (node->contents != CONTENTS_NODE) {
		r_bsp_leaf_t *leaf = (r_bsp_leaf_t *) node;

		if (r_view.area_bits) { // check for door connected areas
			if (!(r_view.area_bits[leaf->area >> 3] & (1 << (leaf->area & 7)))) {
				return;    // not visible
			}
		}

		r_bsp_face_t **s = leaf->leaf_faces;

		for (int32_t i = 0; i < leaf->num_leaf_faces; i++, s++) {
			(*s)->vis_frame = r_locals.vis_frame;
		}

		return;
	}

	// otherwise, traverse down the appropriate sides of the node

	const vec_t dist = Cm_DistanceToPlane(r_view.origin, node->plane);
	if (dist > SIDE_EPSILON) {
		side = 0;
	} else {
		side = 1;
	}

	// recurse down the children, front side first
	R_MarkBspFaces_(node->children[side]);

	// prune all marked surfaces to just those which are front-facing
	r_bsp_face_t *s = node->faces;
	for (int32_t i = 0; i < node->num_faces; i++, s++) {
		if (s->vis_frame == r_locals.vis_frame) { // it's been marked
			if ((s->flags & R_SURF_BACK_SIDE) == side) { // and it's on the right side
				s->frame = r_locals.frame;
			}
		}
	}

	// recurse down the back side
	R_MarkBspFaces_(node->children[!side]);
}

/**
 * @brief Entry point for BSP recursion and surface-level visibility test.
 */
void R_MarkBspFaces(void) {

	if (++r_locals.frame == INT16_MAX) { // avoid overflows, negatives are reserved
		r_locals.frame = 0;
	}

	// flag all visible world surfaces
	R_MarkBspFaces_(r_model_state.world->bsp->nodes);
}

/**
 * @return The distance from the specified point to the given surface.
 */
vec_t R_DistanceToSurface(const vec3_t p, const r_bsp_face_t *surf) {
	return Cm_DistanceToPlane(p, surf->plane);
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

	const int32_t c = leaf->cluster;

	if (c == -1) {
		return false;
	}

	return r_locals.vis_data_pvs[c >> 3] & (1 << (c & 7));
}

/**
 * @brief Returns true if the specified leaf is in the PHS for the current frame.
 */
_Bool R_LeafHearable(const r_bsp_leaf_t *leaf) {

	const int32_t c = leaf->cluster;

	if (c == -1) {
		return false;
	}

	return r_locals.vis_data_phs[c >> 3] & (1 << (c & 7));
}

#define R_CROSSING_CONTENTS_DIST 16.0

/**
 * @brief Returns the cluster of any opaque contents transitions the view
 * origin is currently spanning. This allows us to bit-wise-OR in the PVS and
 * PHS data from another cluster. Returns -1 if no transition is taking place.
 */
static int32_t R_CrossingContents(int32_t contents) {

	vec3_t org;
	VectorCopy(r_view.origin, org);

	if (contents) {
		org[2] += R_CROSSING_CONTENTS_DIST;
	} else {
		org[2] -= R_CROSSING_CONTENTS_DIST;
	}

	const r_bsp_leaf_t *leaf = R_LeafForPoint(org, NULL);

	if (!(leaf->contents & CONTENTS_SOLID) && leaf->contents != contents) {
		return leaf->cluster;
	}

	return -1;
}

/**
 * @brief Mark the leafs that are in the PVS for the current cluster, creating the
 * recursion path for R_MarkSurfaces. Leafs marked for the current cluster
 * will still be frustum-culled, and surfaces therein must still pass a
 * dot-product test in order to be marked as visible for the current frame.
 */
void R_UpdateVis(void) {
	int16_t clusters[2];

	if (r_lock_vis->value) {
		return;
	}

	clusters[0] = clusters[1] = -1;

	// resolve current leaf and derive the PVS clusters
	if (r_model_state.world->bsp->num_clusters && !r_no_vis->integer) {

		const r_bsp_leaf_t *leaf = R_LeafForPoint(r_view.origin, NULL);
		if (leaf->cluster != -1) {

			clusters[0] = leaf->cluster;
			clusters[1] = R_CrossingContents(leaf->contents);

			// if we have the same, valid PVS as the last frame, we're done
			if (memcmp(clusters, r_locals.clusters, sizeof(clusters)) == 0) {
				return;
			}
		}
	}

	memcpy(r_locals.clusters, clusters, sizeof(r_locals.clusters));

	r_locals.vis_frame++;

	if (r_locals.vis_frame == INT16_MAX) { // avoid overflows, negatives are reserved
		r_locals.vis_frame = 0;
	}

	// if we have no vis, mark everything and return
	if (clusters[0] == -1 || r_no_vis->integer) {

		memset(r_locals.vis_data_pvs, 0xff, sizeof(r_locals.vis_data_pvs));
		memset(r_locals.vis_data_phs, 0xff, sizeof(r_locals.vis_data_phs));

		for (int32_t i = 0; i < r_model_state.world->bsp->num_leafs; i++) {
			r_model_state.world->bsp->leafs[i].vis_frame = r_locals.vis_frame;
		}

		for (int32_t i = 0; i < r_model_state.world->bsp->num_nodes; i++) {
			r_model_state.world->bsp->nodes[i].vis_frame = r_locals.vis_frame;
		}

		r_view.num_bsp_clusters = r_model_state.world->bsp->num_clusters;
		r_view.num_bsp_leafs = r_model_state.world->bsp->num_leafs;

		return;
	}

	// resolve PVS for the current cluster
	Cm_ClusterPVS(clusters[0], r_locals.vis_data_pvs);

	// resolve PHS for the current cluster
	Cm_ClusterPHS(clusters[0], r_locals.vis_data_phs);

	// if we crossed contents, merge in the other cluster's PVS and PHS data
	if (clusters[1] != -1 && clusters[1] != clusters[0]) {
		byte pvs[MAX_BSP_LEAFS >> 3], phs[MAX_BSP_LEAFS >> 3];

		Cm_ClusterPVS(clusters[1], pvs);
		Cm_ClusterPHS(clusters[1], phs);

		for (size_t i = 0; i < sizeof(r_locals.vis_data_pvs); i++) {
			r_locals.vis_data_pvs[i] |= pvs[i];
			r_locals.vis_data_phs[i] |= phs[i];
		}
	}

	// recurse up the BSP from the visible leafs, marking a path via the nodes
	const r_bsp_leaf_t *leaf = r_model_state.world->bsp->leafs;

	r_view.num_bsp_leafs = 0;
	r_view.num_bsp_clusters = 0;

	for (uint16_t i = 0; i < r_model_state.world->bsp->num_leafs; i++, leaf++) {

		if (!R_LeafVisible(leaf)) {
			continue;
		}

		r_view.num_bsp_leafs++;

		// keep track of the number of clusters rendered each frame
		r_bsp_cluster_t *cl = &r_model_state.world->bsp->clusters[leaf->cluster];

		if (cl->vis_frame != r_locals.vis_frame) {
			cl->vis_frame = r_locals.vis_frame;
			r_view.num_bsp_clusters++;
		}

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
