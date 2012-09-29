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

vec3_t r_bsp_model_org; // relative to r_view.origin

/*
 * @brief Parses values from the worldspawn entity definition.
 */
const char *R_WorldspawnValue(const char *key) {
	const char *c, *v;

	c = strstr(Cm_EntityString(), va("\"%s\"", key));

	if (c) {
		v = ParseToken(&c); // parse the key itself
		v = ParseToken(&c); // parse the value
	} else {
		v = NULL;
	}

	return v;
}

/*
 * @brief Resolves the contents mask at the specified point.
 */
int32_t R_PointContents(const vec3_t point) {
	int32_t i, contents = Cm_PointContents(point, r_models.world->bsp->first_node);

	for (i = 0; i < r_view.num_entities; i++) {
		r_entity_t *ent = &r_view.entities[i];
		const r_model_t *m = ent->model;

		if (!m || m->type != mod_bsp_submodel || !m->bsp->nodes)
			continue;

		contents |= Cm_TransformedPointContents(point, m->bsp->first_node, ent->origin, ent->angles);
	}

	return contents;
}

/*
 * @brief Traces to world and BSP models. If a BSP entity is hit, it is saved as
 * r_view.trace_ent.
 */
c_trace_t R_Trace(const vec3_t start, const vec3_t end, const vec3_t mins, const vec3_t maxs,
		int32_t mask) {

	// check world
	r_view.trace = Cm_BoxTrace(start, end, mins, maxs, r_models.world->bsp->first_node, mask);
	r_view.trace_ent = NULL;

	float frac = r_view.trace.fraction;
	uint16_t i;

	// check bsp entities
	for (i = 0; i < r_view.num_entities; i++) {

		r_entity_t *ent = &r_view.entities[i];
		const r_model_t *m = ent->model;

		if (!m || m->type != mod_bsp_submodel || !m->bsp->nodes)
			continue;

		c_trace_t tr = Cm_TransformedBoxTrace(start, end, mins, maxs, m->bsp->first_node, mask,
				ent->origin, ent->angles);

		if (tr.fraction < frac) { // we've hit one
			r_view.trace = tr;
			r_view.trace_ent = ent;

			frac = tr.fraction;
		}
	}

	return r_view.trace;
}

/*
 * @brief Returns true if the specified bounding box is completely culled by the
 * view frustum, false otherwise.
 */
bool R_CullBox(const vec3_t mins, const vec3_t maxs) {
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
bool R_CullBspModel(const r_entity_t *e) {
	vec3_t mins, maxs;
	int32_t i;

	if (!e->model->bsp->num_model_surfaces) // no surfaces
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

	return R_CullBox(mins, maxs);
}

/*
 * @brief Draws all BSP surfaces for the specified entity. This is a condensed
 * version of the world drawing routine that relies on setting the visibility
 * counters to -1 to safely iterate the sorted surfaces arrays.
 */
static void R_DrawBspModelSurfaces(const r_entity_t *e) {
	r_bsp_surface_t *surf;
	int32_t i;

	// temporarily swap the view frame so that the surface drawing
	// routines pickup only the bsp model's surfaces

	const int32_t f = r_locals.frame;
	r_locals.frame = -2;

	surf = &e->model->bsp->surfaces[e->model->bsp->first_model_surface];

	for (i = 0; i < e->model->bsp->num_model_surfaces; i++, surf++) {
		const c_bsp_plane_t *plane = surf->plane;
		float dot;

		// find which side of the surf we are on
		if (AXIAL(plane))
			dot = r_bsp_model_org[plane->type] - plane->dist;
		else
			dot = DotProduct(r_bsp_model_org, plane->normal) - plane->dist;

		if (((surf->flags & R_SURF_SIDE_BACK) && (dot < -BACK_PLANE_EPSILON))
				|| (!(surf->flags & R_SURF_SIDE_BACK) && (dot > BACK_PLANE_EPSILON))) {
			// visible, flag for rendering
			surf->frame = r_locals.frame;
			surf->back_frame = -1;
		} else { // back-facing
			surf->frame = -1;
			surf->back_frame = r_locals.frame;
		}
	}

	R_DrawOpaqueSurfaces(&e->model->bsp->sorted_surfaces->opaque);

	R_DrawOpaqueWarpSurfaces(&e->model->bsp->sorted_surfaces->opaque_warp);

	R_DrawAlphaTestSurfaces(&e->model->bsp->sorted_surfaces->alpha_test);

	R_EnableBlend(true);

	R_DrawBackSurfaces(&e->model->bsp->sorted_surfaces->back);

	R_DrawMaterialSurfaces(&e->model->bsp->sorted_surfaces->material);

	R_DrawFlareSurfaces(&e->model->bsp->sorted_surfaces->flare);

	R_DrawBlendSurfaces(&e->model->bsp->sorted_surfaces->blend);

	R_DrawBlendWarpSurfaces(&e->model->bsp->sorted_surfaces->blend_warp);

	R_EnableBlend(false);

	r_locals.frame = f; // undo the swap
}

/*
 * @brief Draws the BSP model for the specified entity, taking translation and
 * rotation into account.
 */
void R_DrawBspModel(const r_entity_t *e) {
	vec3_t forward, right, up;
	vec3_t temp;

	// set the relative origin, accounting for rotation if necessary
	VectorSubtract(r_view.origin, e->origin, r_bsp_model_org);
	if (e->angles[0] || e->angles[1] || e->angles[2]) {

		VectorCopy(r_bsp_model_org, temp);
		AngleVectors(e->angles, forward, right, up);

		r_bsp_model_org[0] = DotProduct(temp, forward);
		r_bsp_model_org[1] = -DotProduct(temp, right);
		r_bsp_model_org[2] = DotProduct(temp, up);
	}

	R_ShiftLights(e->origin);

	R_RotateForEntity(e);

	R_DrawBspModelSurfaces(e);

	R_RotateForEntity(NULL);

	R_ShiftLights(vec3_origin);
}

/*
 * @brief Developer tool for viewing static BSP light sources.
 */
void R_DrawBspLights(void) {
	r_bsp_light_t *l;
	int32_t i;

	if (!r_draw_bsp_lights->value)
		return;

	l = r_models.world->bsp->bsp_lights;
	for (i = 0; i < r_models.world->bsp->num_bsp_lights; i++, l++) {
		r_corona_t c;

		VectorCopy(l->origin, c.origin);
		c.radius = l->radius;
		c.flicker = 0.0;
		VectorCopy(l->color, c.color);

		R_AddCorona(&c);
	}
}

/*
 * @brief Developer tool for viewing BSP vertex normals. Only Phong-interpolated
 * surfaces show their normals when r_show_normals == 2.
 */
void R_DrawBspNormals(void) {
	r_bsp_surface_t *surf;
	GLfloat *vertex, *normal;
	vec3_t end;
	int32_t i, j, k;

	vec4_t red = { 1.0, 0.0, 0.0, 1.0 };

	if (!r_draw_bsp_normals->value)
		return;

	R_EnableTexture(&texunit_diffuse, false);

	R_ResetArrayState(); // default arrays

	R_Color(red);

	k = 0;
	surf = r_models.world->bsp->surfaces;
	for (i = 0; i < r_models.world->bsp->num_surfaces; i++, surf++) {

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
			vertex = &r_models.world->verts[(surf->index + j) * 3];
			normal = &r_models.world->normals[(surf->index + j) * 3];

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
		{ 0.8, 0.2, 0.2, 0.4 },
		{ 0.2, 0.8, 0.2, 0.4 },
		{ 0.2, 0.2, 0.8, 0.4 },
		{ 0.8, 0.8, 0.2, 0.4 },
		{ 0.2, 0.8, 0.8, 0.4 },
		{ 0.8, 0.2, 0.8, 0.4 },
		{ 0.8, 0.8, 0.8, 0.4 }
	};

	if (!r_draw_bsp_leafs->value)
		return;

	R_SetArrayState(r_models.world);

	R_EnableTexture(&texunit_diffuse, false);

	glEnable(GL_POLYGON_OFFSET_FILL);

	glPolygonOffset(-2.0, 0.0);

	const r_bsp_leaf_t *l = r_models.world->bsp->leafs;
	uint16_t i;

	for (i = 0; i < r_models.world->bsp->num_leafs; i++, l++) {

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
static void R_MarkSurfaces_(r_bsp_node_t *node) {
	int32_t i, side, side_bit;
	float dot;

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
	R_MarkSurfaces_(node->children[side]);

	// prune all marked surfaces to just those which are front-facing
	r_bsp_surface_t *s = r_models.world->bsp->surfaces + node->first_surface;

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
	R_MarkSurfaces_(node->children[!side]);
}

/*
 * @brief Entry point for BSP recursion and surface-level visibility test. This
 * is also where the infamous r_optimize strategy is implemented.
 */
void R_MarkSurfaces(void) {
	static vec3_t old_origin, old_angles;
	static vec2_t old_fov;
	static int16_t old_vis_frame;
	vec3_t o, a;

	VectorSubtract(r_view.origin, old_origin, o);
	VectorSubtract(r_view.angles, old_angles, a);

	// only recurse after cluster change AND significant movement
	if (r_optimize->value && (r_locals.vis_frame == old_vis_frame) && // same pvs
			(r_view.fov[0] == old_fov[0] && r_view.fov[1] == old_fov[1]) && // same fov
			VectorLength(o) < 5.0 && VectorLength(a) < 2.0) // little movement
		return;

	old_vis_frame = r_locals.vis_frame;

	old_fov[0] = r_view.fov[0];
	old_fov[1] = r_view.fov[1];

	r_locals.frame++;

	if (r_locals.frame == 0x7fff) // avoid overflows, negatives are reserved
		r_locals.frame = 0;

	VectorCopy(r_view.origin, old_origin);
	VectorCopy(r_view.angles, old_angles);

	R_ClearSkyBox();

	// flag all visible world surfaces
	R_MarkSurfaces_(r_models.world->bsp->nodes);
}

/*
 * @brief Returns the leaf for the specified point.
 */
const r_bsp_leaf_t *R_LeafForPoint(const vec3_t p, const r_model_t *model) {

	if (!model)
		model = r_models.world;

	return &model->bsp->leafs[Cm_PointLeafnum(p)];
}

/*
 * @brief Returns true if the specified leaf is in the given PVS/PHS vector.
 */
static inline bool R_LeafInVis(const r_bsp_leaf_t *leaf, const byte *vis) {
	int32_t c;

	if ((c = leaf->cluster) == -1)
		return false;

	return vis[c >> 3] & (1 << (c & 7));
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
	r_locals.cluster = R_LeafForPoint(r_view.origin, NULL )->cluster;

	if (!r_no_vis->value && (r_locals.old_cluster == r_locals.cluster))
		return;

	r_locals.old_cluster = r_locals.cluster;

	r_locals.vis_frame++;

	if (r_locals.vis_frame == 0x7fff) // avoid overflows, negatives are reserved
		r_locals.vis_frame = 0;

	// if we have no vis, mark everything and return
	if (r_no_vis->value || !r_models.world->bsp->num_clusters || r_locals.cluster == -1) {

		for (i = 0; i < r_models.world->bsp->num_leafs; i++)
			r_models.world->bsp->leafs[i].vis_frame = r_locals.vis_frame;

		for (i = 0; i < r_models.world->bsp->num_nodes; i++)
			r_models.world->bsp->nodes[i].vis_frame = r_locals.vis_frame;

		r_view.num_bsp_clusters = r_models.world->bsp->num_clusters;
		r_view.num_bsp_leafs = r_models.world->bsp->num_leafs;

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

	// recurse up the bsp from the visible leafs, marking a path via the nodes
	const r_bsp_leaf_t *leaf = r_models.world->bsp->leafs;

	r_view.num_bsp_leafs = 0;
	r_view.num_bsp_clusters = 0;

	for (i = 0; i < r_models.world->bsp->num_leafs; i++, leaf++) {

		if (!R_LeafInVis(leaf, r_locals.vis_data_pvs))
			continue;

		r_view.num_bsp_leafs++;

		r_bsp_cluster_t *cl = &r_models.world->bsp->clusters[leaf->cluster];

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

/*
 * @brief Returns true if the specified leaf is in the PVS for the current frame.
 */
bool R_LeafInPvs(const r_bsp_leaf_t *leaf) {
	return R_LeafInVis(leaf, r_locals.vis_data_pvs);
}

/*
 * @brief Returns true if the specified leaf is in the PHS for the current frame.
 */
bool R_LeafInPhs(const r_bsp_leaf_t *leaf) {
	return R_LeafInVis(leaf, r_locals.vis_data_phs);
}
