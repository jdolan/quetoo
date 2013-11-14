/*
 * Copyright(c) 1997-2001 Id Software, Inc.
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

#include "r_local.h"

static vec3_t r_bsp_model_org; // relative to r_view.origin

/*
 * @brief Returns true if the specified bounding box is completely culled by the
 * view frustum, false otherwise.
 */
_Bool R_CullBox(const vec3_t mins, const vec3_t maxs) {
	int32_t i;

	if (!r_cull->value)
		return false;

	for (i = 0; i < 4; i++) {
		if (BoxOnPlaneSide(mins, maxs, &r_locals.frustum[i]) != SIDE_BACK)
			return false;
	}

	return true;
}

/*
 * @brief Returns true if the specified entity is completely culled by the view
 * frustum, false otherwise.
 */
_Bool R_CullBspModel(const r_entity_t *e) {
	vec3_t mins, maxs;
	int32_t i;

	if (!e->model->bsp_inline->num_surfaces) // no surfaces
		return true;

	if (e->angles[0] || e->angles[1] || e->angles[2]) {
		for (i = 0; i < 3; i++) {
			mins[i] = e->origin[i] - e->model->radius;
			maxs[i] = e->origin[i] + e->model->radius;
		}
	} else {
		VectorAdd(e->origin, e->model->mins, mins);
		VectorAdd(e->origin, e->model->maxs, maxs);
	}

	e->model->bsp_inline->lights = 0; // reset lights mask each frame

	return R_CullBox(mins, maxs);
}

#define BACK_PLANE_EPSILON 0.01

/*
 * @brief Draws all BSP surfaces for the specified entity. This is a condensed
 * version of the world drawing routine that relies on setting the visibility
 * counters to -2 to safely iterate the sorted surfaces arrays.
 */
static void R_DrawBspInlineModel_(const r_entity_t *e) {
	static int16_t frame = -2;
	r_bsp_surface_t *surf;
	uint16_t i;

	// temporarily swap the view frame so that the surface drawing
	// routines pickup only the inline model's surfaces

	const int16_t f = r_locals.frame;
	r_locals.frame = frame--;

	if (frame == INT16_MIN) {
		frame = -2;
	}

	surf = &r_model_state.world->bsp->surfaces[e->model->bsp_inline->first_surface];

	for (i = 0; i < e->model->bsp_inline->num_surfaces; i++, surf++) {
		const c_bsp_plane_t *plane = surf->plane;
		vec_t dot;

		// find which side of the surf we are on
		if (AXIAL(plane))
			dot = r_bsp_model_org[plane->type] - plane->dist;
		else
			dot = DotProduct(r_bsp_model_org, plane->normal) - plane->dist;

		if (surf->flags & R_SURF_SIDE_BACK)
			dot = -dot;

		if (dot > BACK_PLANE_EPSILON) { // visible, flag for rendering
			surf->frame = r_locals.frame;
			surf->back_frame = -1;
		} else { // back-facing
			surf->frame = -1;
			surf->back_frame = r_locals.frame;
		}
	}

	const r_sorted_bsp_surfaces_t *surfs = r_model_state.world->bsp->sorted_surfaces;

	R_DrawOpaqueBspSurfaces(&surfs->opaque);

	R_DrawOpaqueWarpBspSurfaces(&surfs->opaque_warp);

	R_DrawAlphaTestBspSurfaces(&surfs->alpha_test);

	R_EnableBlend(true);

	R_DrawBackBspSurfaces(&surfs->back);

	R_DrawMaterialBspSurfaces(&surfs->material);

	R_DrawFlareBspSurfaces(&surfs->flare);

	R_DrawBlendBspSurfaces(&surfs->blend);

	R_DrawBlendWarpBspSurfaces(&surfs->blend_warp);

	R_EnableBlend(false);

	r_locals.frame = f; // undo the swap
}

/*
 * @brief Draws the BSP model for the specified entity, taking translation and
 * rotation into account.
 */
void R_DrawBspInlineModel(const r_entity_t *e) {

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

	R_RotateLightsForEntity(e, e->model->bsp_inline->lights);

	R_RotateForEntity(e);

	R_DrawBspInlineModel_(e);

	R_RotateForEntity(NULL);

	R_RotateLightsForEntity(NULL, e->model->bsp_inline->lights);
}

/*
 * @brief Developer tool for viewing static BSP light sources.
 */
void R_DrawBspLights(void) {
	int32_t i;

	if (!r_draw_bsp_lights->value)
		return;

	const r_bsp_light_t *l = r_model_state.world->bsp->bsp_lights;
	for (i = 0; i < r_model_state.world->bsp->num_bsp_lights; i++, l++) {
		r_corona_t c;

		VectorCopy(l->origin, c.origin);
		c.radius = l->radius * r_draw_bsp_lights->value;
		c.flicker = 0.0;
		VectorCopy(l->color, c.color);

		R_AddCorona(&c);
	}
}

/*
 * @brief Developer tool for viewing BSP vertex normals. Only Phong-interpolated
 * surfaces show their normals when r_draw_bsp_normals is 2.
 */
void R_DrawBspNormals(void) {
	const vec4_t red = { 1.0, 0.0, 0.0, 1.0 };
	int32_t i, j, k;

	if (!r_draw_bsp_normals->value)
		return;

	R_EnableTexture(&texunit_diffuse, false);

	R_ResetArrayState(); // default arrays

	R_Color(red);

	k = 0;
	const r_bsp_surface_t *surf = r_model_state.world->bsp->surfaces;
	for (i = 0; i < r_model_state.world->bsp->num_surfaces; i++, surf++) {

		if (surf->vis_frame != r_locals.vis_frame)
			continue; // not visible

		if (surf->texinfo->flags & (SURF_SKY | SURF_WARP))
			continue; // don't care

		if ((r_draw_bsp_normals->integer & 2) && !(surf->texinfo->flags & SURF_PHONG))
			continue; // don't care

		if (k > MAX_GL_ARRAY_LENGTH - 512) { // avoid overflows, draw in batches
			glDrawArrays(GL_LINES, 0, k / 3);
			k = 0;
		}

		for (j = 0; j < surf->num_edges; j++) {
			vec3_t end;

			const GLfloat *vertex = &r_model_state.world->verts[(surf->index + j) * 3];
			const GLfloat *normal = &r_model_state.world->normals[(surf->index + j) * 3];

			VectorMA(vertex, 12.0, normal, end);

			memcpy(&r_state.vertex_array_3d[k], vertex, sizeof(vec3_t));
			memcpy(&r_state.vertex_array_3d[k + 3], end, sizeof(vec3_t));
			k += sizeof(vec3_t) / sizeof(vec_t) * 2;
		}
	}

	glDrawArrays(GL_LINES, 0, k / 3);

	R_EnableTexture(&texunit_diffuse, true);

	R_Color(NULL);
}

/*
 * @brief Developer tool for viewing BSP leafs and clusters.
 */
void R_DrawBspLeafs(void) {
	const vec4_t leaf_colors[] = {
	// assign each leaf a color
			{ 0.2, 0.2, 0.2, 0.4 },
			{ 0.8, 0.2, 0.2, 0.4 },
			{ 0.2, 0.8, 0.2, 0.4 },
			{ 0.2, 0.2, 0.8, 0.4 },
			{ 0.8, 0.8, 0.2, 0.4 },
			{ 0.2, 0.8, 0.8, 0.4 },
			{ 0.8, 0.2, 0.8, 0.4 },
			{ 0.8, 0.8, 0.8, 0.4 } };

	if (!r_draw_bsp_leafs->value)
		return;

	R_SetArrayState(r_model_state.world);

	R_EnableTexture(&texunit_diffuse, false);

	glEnable(GL_POLYGON_OFFSET_FILL);

	glPolygonOffset(-2.0, 0.0);

	const r_bsp_leaf_t *l = r_model_state.world->bsp->leafs;
	uint16_t i;

	for (i = 0; i < r_model_state.world->bsp->num_leafs; i++, l++) {

		if (l->vis_frame != r_locals.vis_frame)
			continue;

		if (r_draw_bsp_leafs->integer == 2)
			R_Color(leaf_colors[l->cluster % lengthof(leaf_colors)]);
		else
			R_Color(leaf_colors[i % lengthof(leaf_colors)]);

		r_bsp_surface_t **s = l->first_leaf_surface;
		uint16_t j;

		for (j = 0; j < l->num_leaf_surfaces; j++, s++) {

			if ((*s)->vis_frame != r_locals.vis_frame)
				continue;

			glDrawArrays(GL_POLYGON, (*s)->index, (*s)->num_edges);
		}
	}

	glPolygonOffset(0.0, 0.0);
	glDisable(GL_POLYGON_OFFSET_FILL);

	R_EnableTexture(&texunit_diffuse, true);

	R_Color(NULL);
}

/*
 * @brief Top-down BSP node recursion. Nodes identified as within the PVS by
 * R_MarkLeafs are first frustum-culled; those which fail immediately
 * return.
 *
 * For the rest, the front-side child node is recursed. Any surfaces marked
 * in that recursion must then pass a dot-product test to resolve sidedness.
 * Finally, the back-side child node is recursed.
 */
static void R_MarkBspSurfaces_(r_bsp_node_t *node) {
	int32_t i, side, side_bit;
	vec_t dot;

	if (node->contents == CONTENTS_SOLID)
		return; // solid

	if (node->vis_frame != r_locals.vis_frame)
		return; // not in view

	if (R_CullBox(node->mins, node->maxs))
		return; // culled out

	// if leaf node, flag surfaces to draw this frame
	if (node->contents != CONTENTS_NODE) {
		r_bsp_leaf_t *leaf = (r_bsp_leaf_t *) node;

		if (r_view.area_bits) { // check for door connected areas
			if (!(r_view.area_bits[leaf->area >> 3] & (1 << (leaf->area & 7))))
				return; // not visible
		}

		r_bsp_surface_t **s = leaf->first_leaf_surface;

		for (i = 0; i < leaf->num_leaf_surfaces; i++, s++) {
			(*s)->vis_frame = r_locals.vis_frame;
		}

		return;
	}

	// otherwise, traverse down the appropriate sides of the node

	if (AXIAL(node->plane))
		dot = r_view.origin[node->plane->type] - node->plane->dist;
	else
		dot = DotProduct(r_view.origin, node->plane->normal) - node->plane->dist;

	if (dot >= 0.0) {
		side = 0;
		side_bit = 0;
	} else {
		side = 1;
		side_bit = R_SURF_SIDE_BACK;
	}

	// recurse down the children, front side first
	R_MarkBspSurfaces_(node->children[side]);

	// prune all marked surfaces to just those which are front-facing
	r_bsp_surface_t *s = r_model_state.world->bsp->surfaces + node->first_surface;

	for (i = 0; i < node->num_surfaces; i++, s++) {

		if (s->vis_frame == r_locals.vis_frame) { // it's been marked

			if ((s->flags & R_SURF_SIDE_BACK) != side_bit) { // but back-facing
				s->frame = -1;
				s->back_frame = r_locals.frame;
			} else { // draw it
				s->frame = r_locals.frame;
				s->back_frame = -1;
			}
		}
	}

	// recurse down the back side
	R_MarkBspSurfaces_(node->children[!side]);
}

/*
 * @brief Entry point for BSP recursion and surface-level visibility test.
 */
void R_MarkBspSurfaces(void) {

	if (++r_locals.frame == INT16_MAX) // avoid overflows, negatives are reserved
		r_locals.frame = 0;

	// clear the bounds of the sky box
	R_ClearSkyBox();

	// flag all visible world surfaces
	R_MarkBspSurfaces_(r_model_state.world->bsp->nodes);
}

/*
 * @brief Returns the leaf for the specified point.
 */
const r_bsp_leaf_t *R_LeafForPoint(const vec3_t p, const r_bsp_model_t *bsp) {

	if (!bsp)
		bsp = r_model_state.world->bsp;

	return &bsp->leafs[Cm_PointLeafnum(p)];
}

/*
 * @brief Returns true if the specified leaf is in the PVS for the current frame.
 */
_Bool R_LeafVisible(const r_bsp_leaf_t *leaf) {
	int32_t c;

	if ((c = leaf->cluster) == -1)
		return false;

	return r_locals.vis_data_pvs[c >> 3] & (1 << (c & 7));
}

/*
 * @brief Returns true if the specified leaf is in the PHS for the current frame.
 */
_Bool R_LeafHearable(const r_bsp_leaf_t *leaf) {
	int32_t c;

	if ((c = leaf->cluster) == -1)
		return false;

	return r_locals.vis_data_phs[c >> 3] & (1 << (c & 7));
}

/*
 * @brief Returns the cluster of any opaque contents transitions the view origin is
 * currently spanning. This allows us to bit-wise-OR in the PVS data from
 * another cluster. Returns -1 if no transition is taking place.
 */
static int16_t R_CrossingContents(void) {
	const r_bsp_leaf_t *leaf;
	vec3_t org;

	VectorCopy(r_view.origin, org);

	org[2] -= 16.0;
	leaf = R_LeafForPoint(org, NULL);

	if (!(leaf->contents & CONTENTS_SOLID) && leaf->cluster != r_locals.cluster)
		return leaf->cluster;

	org[2] += 32.0;
	leaf = R_LeafForPoint(org, NULL);

	if (!(leaf->contents & CONTENTS_SOLID) && leaf->cluster != r_locals.cluster)
		return leaf->cluster;

	return (int16_t) -1;
}

/*
 * @brief Mark the leafs that are in the PVS for the current cluster, creating the
 * recursion path for R_MarkSurfaces. Leafs marked for the current cluster
 * will still be frustum-culled, and surfaces therein must still pass a
 * dot-product test in order to be marked as visible for the current frame.
 */
void R_UpdateVis(void) {
	int16_t cluster;
	int32_t i;

	if (r_lock_vis->value)
		return;

	// resolve current cluster
	r_locals.cluster = R_LeafForPoint(r_view.origin, NULL)->cluster;

	if (!r_no_vis->value && (r_locals.old_cluster == r_locals.cluster))
		return;

	r_locals.old_cluster = r_locals.cluster;

	r_locals.vis_frame++;

	if (r_locals.vis_frame == INT16_MAX) // avoid overflows, negatives are reserved
		r_locals.vis_frame = 0;

	// if we have no vis, mark everything and return
	if (r_no_vis->value || !r_model_state.world->bsp->num_clusters || r_locals.cluster == -1) {

		memset(r_locals.vis_data_pvs, 0xff, sizeof(r_locals.vis_data_pvs));
		memset(r_locals.vis_data_phs, 0xff, sizeof(r_locals.vis_data_phs));

		for (i = 0; i < r_model_state.world->bsp->num_leafs; i++)
			r_model_state.world->bsp->leafs[i].vis_frame = r_locals.vis_frame;

		for (i = 0; i < r_model_state.world->bsp->num_nodes; i++)
			r_model_state.world->bsp->nodes[i].vis_frame = r_locals.vis_frame;

		r_view.num_bsp_clusters = r_model_state.world->bsp->num_clusters;
		r_view.num_bsp_leafs = r_model_state.world->bsp->num_leafs;

		return;
	}

	// resolve pvs for the current cluster
	const byte *pvs = Cm_ClusterPVS(r_locals.cluster);
	memcpy(r_locals.vis_data_pvs, pvs, sizeof(r_locals.vis_data_pvs));

	// check above or below the origin in case we are crossing opaque contents
	if ((cluster = R_CrossingContents()) != -1) {
		pvs = Cm_ClusterPVS(cluster);

		for (i = 0; i < MAX_BSP_LEAFS >> 3; i++) {
			r_locals.vis_data_pvs[i] |= pvs[i];
		}
	}

	const byte *phs = Cm_ClusterPHS(r_locals.cluster);
	memcpy(r_locals.vis_data_phs, phs, sizeof(r_locals.vis_data_phs));

	// recurse up the BSP from the visible leafs, marking a path via the nodes
	const r_bsp_leaf_t *leaf = r_model_state.world->bsp->leafs;

	r_view.num_bsp_leafs = 0;
	r_view.num_bsp_clusters = 0;

	for (i = 0; i < r_model_state.world->bsp->num_leafs; i++, leaf++) {

		if (!R_LeafVisible(leaf))
			continue;

		r_view.num_bsp_leafs++;

		// keep track of the number of clusters rendered each frame
		r_bsp_cluster_t *cl = &r_model_state.world->bsp->clusters[leaf->cluster];

		if (cl->vis_frame != r_locals.vis_frame) {
			cl->vis_frame = r_locals.vis_frame;
			r_view.num_bsp_clusters++;
		}

		r_bsp_node_t *node = (r_bsp_node_t *) leaf;
		while (node) {

			if (node->vis_frame == r_locals.vis_frame)
				break;

			node->vis_frame = r_locals.vis_frame;
			node = node->parent;
		}
	}
}
