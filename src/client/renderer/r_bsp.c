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

	//TODO R_DrawBspModelSurfaces(e);

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
 * @brief Developer tool for viewing BSP clusters.
 */
void R_DrawBspClusters(void) {
	const vec4_t leaf_colors[] = { { 0.8, 0.2, 0.2, 0.4 }, { 0.2, 0.8, 0.2, 0.4 }, { 0.2, 0.2, 0.8,
			0.4 }, { 0.8, 0.8, 0.2, 0.4 }, { 0.2, 0.8, 0.8, 0.4 }, { 0.8, 0.2, 0.8, 0.4 }, { 0.8,
			0.8, 0.8, 0.4 } };

	if (!r_draw_bsp_leafs->value)
		return;

	R_SetArrayState(r_models.world);

	R_EnableTexture(&texunit_diffuse, false);

	glEnable(GL_POLYGON_OFFSET_FILL);

	glPolygonOffset(-2.0, 0.0);

	/*TODO const r_bsp_leaf_t *l = r_models.world->leafs;
	uint16_t i;

	for (i = 0; i < r_models.world->num_leafs; i++, l++) {

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
	}*/

	glPolygonOffset(0.0, 0.0);
	glDisable(GL_POLYGON_OFFSET_FILL);

	R_EnableTexture(&texunit_diffuse, true);

	R_Color(NULL);
}

/*
 * @brief Draws the world model for the current frame.
 */
void R_DrawBspWorld(void) {
	uint16_t i, j;

	r_view.num_bsp_arrays = 0;

	R_Color(NULL);

	R_EnableLighting(r_state.default_program, true);

	R_SetArrayState(r_models.world);

	const r_bsp_cluster_t *cluster = r_models.world->bsp->clusters;
	for (i = 0; i < r_models.world->bsp->num_clusters; i++, cluster++) {

		if (cluster->vis_frame != r_locals.vis_frame)
			continue;

		const r_bsp_array_t *array = cluster->arrays;
		for (j = 0; j < cluster->num_arrays; j++, array++) {

			R_UseBspArray(array);
			glDrawArrays(GL_TRIANGLES, array->index, array->count);

			r_view.num_bsp_arrays++;
		}
	}

	R_UseBspArray(NULL);

	R_EnableLighting(NULL, false);

	R_ResetArrayState();

	R_Color(NULL);
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
 * @brief Resolves PVS and PHS for the current frame.
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

	if (r_locals.vis_frame == 0x7fff) // avoid overflows, negatives are reserved
		r_locals.vis_frame = 0;

	// if we have no vis, mark everything and return
	if (r_no_vis->value || !r_models.world->bsp->num_clusters || r_locals.cluster == -1) {

		for (i = 0; i < r_models.world->bsp->num_clusters; i++)
			r_models.world->bsp->clusters[i].vis_frame = r_locals.vis_frame;

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
