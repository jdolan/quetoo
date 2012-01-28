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
 * R_WorldspawnValue
 *
 * Parses values from the worldspawn entity definition.
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
 * R_CullBox
 *
 * Returns true if the specified bounding box is completely culled by the
 * view frustum, false otherwise.
 */
boolean_t R_CullBox(const vec3_t mins, const vec3_t maxs) {
	int i;

	if (!r_cull->value)
		return false;

	for (i = 0; i < 4; i++) {
		if (BoxOnPlaneSide(mins, maxs, &r_locals.frustum[i]) == SIDE_BACK)
			return true;
	}

	return false;
}

/*
 * R_CullBspModel
 *
 * Returns true if the specified entity is completely culled by the view
 * frustum, false otherwise.
 */
boolean_t R_CullBspModel(const r_entity_t *e) {
	vec3_t mins, maxs;
	int i;

	if (!e->model->num_model_surfaces) // no surfaces
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
 * R_DrawBspModelSurfaces
 */
static void R_DrawBspModelSurfaces(const r_entity_t *e) {
	r_bsp_surface_t *surf;
	int i;

	// temporarily swap the view frame so that the surface drawing
	// routines pickup only the bsp model's surfaces

	const int f = r_locals.frame;
	r_locals.frame = -2;

	surf = &e->model->surfaces[e->model->first_model_surface];

	for (i = 0; i < e->model->num_model_surfaces; i++, surf++) {
		const c_bsp_plane_t *plane = surf->plane;
		float dot;

		// find which side of the surf we are on
		if (AXIAL(plane))
			dot = r_bsp_model_org[plane->type] - plane->dist;
		else
			dot = DotProduct(r_bsp_model_org, plane->normal) - plane->dist;

		if (((surf->flags & R_SURF_SIDE_BACK) && (dot < -BACK_PLANE_EPSILON))
				|| (!(surf->flags & R_SURF_SIDE_BACK) && (dot
						> BACK_PLANE_EPSILON))) {
			// visible, flag for rendering
			surf->frame = r_locals.frame;
			surf->back_frame = -1;
		} else { // back-facing
			surf->frame = -1;
			surf->back_frame = r_locals.frame;
		}
	}

	R_DrawOpaqueSurfaces(e->model->opaque_surfaces);

	R_DrawOpaqueWarpSurfaces(e->model->opaque_warp_surfaces);

	R_DrawAlphaTestSurfaces(e->model->alpha_test_surfaces);

	R_EnableBlend(true);

	R_DrawBackSurfaces(e->model->back_surfaces);

	R_DrawMaterialSurfaces(e->model->material_surfaces);

	R_DrawFlareSurfaces(e->model->flare_surfaces);

	R_DrawBlendSurfaces(e->model->blend_surfaces);

	R_DrawBlendWarpSurfaces(e->model->blend_warp_surfaces);

	R_EnableBlend(false);

	r_locals.frame = f; // undo the swap
}

/*
 * R_DrawBspModel
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
 * R_DrawBspLights
 *
 * Developer tool for viewing static BSP light sources.
 */
void R_DrawBspLights(void) {
	r_bsp_light_t *l;
	int i;

	if (!r_draw_bsp_lights->value)
		return;

	l = r_world_model->bsp_lights;
	for (i = 0; i < r_world_model->num_bsp_lights; i++, l++) {
		r_corona_t c;

		VectorCopy(l->origin, c.org);
		c.radius = l->radius;
		c.flicker = 0.0;
		VectorCopy(l->color, c.color);

		R_AddCorona(&c);
	}
}

/*
 * R_DrawBspNormals
 *
 * Developer tool for viewing BSP vertex normals.  Only Phong interpolated
 * surfaces show their normals when r_shownormals > 1.0.
 */
void R_DrawBspNormals(void) {
	r_bsp_surface_t *surf;
	GLfloat *vertex, *normal;
	vec3_t end;
	int i, j, k;

	if (!r_draw_bsp_normals->value)
		return;

	R_EnableTexture(&texunit_diffuse, false);

	R_ResetArrayState(); // default arrays

	glColor3f(1.0, 0.0, 0.0);

	k = 0;
	surf = r_world_model->surfaces;
	for (i = 0; i < r_world_model->num_surfaces; i++, surf++) {

		if (surf->vis_frame != r_locals.vis_frame)
			continue; // not visible

		if (surf->texinfo->flags & (SURF_SKY | SURF_WARP))
			continue; // don't care

		if ((r_draw_bsp_normals->integer & 2) && !(surf->texinfo->flags
				& SURF_PHONG))
			continue; // don't care

		if (k > MAX_GL_ARRAY_LENGTH - 512) { // avoid overflows, draw in batches
			glDrawArrays(GL_LINES, 0, k / 3);
			k = 0;
		}

		for (j = 0; j < surf->num_edges; j++) {
			vertex = &r_world_model->verts[(surf->index + j) * 3];
			normal = &r_world_model->normals[(surf->index + j) * 3];

			VectorMA(vertex, 12.0, normal, end);

			memcpy(&r_state.vertex_array_3d[k], vertex, sizeof(vec3_t));
			memcpy(&r_state.vertex_array_3d[k + 3], end, sizeof(vec3_t));
			k += sizeof(vec3_t) / sizeof(vec_t) * 2;
		}
	}

	glDrawArrays(GL_LINES, 0, k / 3);

	R_EnableTexture(&texunit_diffuse, true);

	glColor4ubv(color_white);
}

/*
 * R_MarkSurfaces_
 *
 * Top-down BSP node recursion.  Nodes identified as within the PVS by
 * R_MarkLeafs are first frustum-culled; those which fail immediately
 * return.
 *
 * For the rest, the front-side child node is recursed.  Any surfaces marked
 * in that recursion must then pass a dot-product test to resolve sidedness.
 * Finally, the back-side child node is recursed.
 */
static void R_MarkSurfaces_(r_bsp_node_t *node) {
	int i, side, side_bit;
	r_bsp_surface_t *surf, **lsurf;
	c_bsp_plane_t *plane;
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

		lsurf = leaf->first_leaf_surface;

		for (i = 0; i < leaf->num_leaf_surfaces; i++, lsurf++) {
			(*lsurf)->vis_frame = r_locals.vis_frame;
		}

		return;
	}

	// otherwise, traverse down the appropriate sides of the node

	plane = node->plane;

	if (AXIAL(plane))
		dot = r_view.origin[plane->type] - plane->dist;
	else
		dot = DotProduct(r_view.origin, plane->normal) - plane->dist;

	if (dot >= 0) {
		side = 0;
		side_bit = 0;
	} else {
		side = 1;
		side_bit = R_SURF_SIDE_BACK;
	}

	// recurse down the children, front side first
	R_MarkSurfaces_(node->children[side]);

	// prune all marked surfaces to just those which are front-facing
	surf = r_world_model->surfaces + node->first_surface;

	for (i = 0; i < node->num_surfaces; i++, surf++) {

		if (surf->vis_frame == r_locals.vis_frame) { // it's been marked

			if ((surf->flags & R_SURF_SIDE_BACK) != side_bit) { // but back-facing
				surf->frame = -1;
				surf->back_frame = r_locals.frame;
			} else { // draw it
				surf->frame = r_locals.frame;
				surf->back_frame = -1;
			}
		}
	}

	// recurse down the back side
	R_MarkSurfaces_(node->children[!side]);
}

/*
 * R_MarkSurfaces
 *
 * Entry point for BSP recursion and surface-level visibility test.  This
 * is also where the infamous r_optimize strategy is implemented.
 */
void R_MarkSurfaces(void) {
	static vec3_t old_origin, old_angles;
	static vec2_t old_fov;
	static short old_vis_frame;
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
	R_MarkSurfaces_(r_world_model->nodes);
}

/*
 * R_LeafForPoint
 */
const r_bsp_leaf_t *R_LeafForPoint(const vec3_t p, const r_model_t *model) {
	const r_bsp_node_t *node;
	const c_bsp_plane_t *plane;
	float dot;

	if (!model || !model->nodes) {
		Com_Error(ERR_DROP, "R_LeafForPoint: Bad model.");
	}

	node = model->nodes;
	while (true) {

		if (node->contents != CONTENTS_NODE)
			return (r_bsp_leaf_t *) node;

		plane = node->plane;

		if (AXIAL(plane))
			dot = p[plane->type] - plane->dist;
		else
			dot = DotProduct(p, plane->normal) - plane->dist;

		if (dot > 0)
			node = node->children[0];
		else
			node = node->children[1];
	}

	Com_Error(ERR_DROP, "R_LeafForPoint: NULL");
	return NULL;
}

/*
 * R_DecompressVis
 */
static byte *R_DecompressVis(const byte *in) {
	static byte decompressed[MAX_BSP_LEAFS / 8];
	int c;
	byte *out;
	int row;

	row = (r_world_model->vis->num_clusters + 7) >> 3;
	out = decompressed;

	if (!in) { // no vis info, so make all visible
		while (row) {
			*out++ = 0xff;
			row--;
		}
		return decompressed;
	}

	do {
		if (*in) {
			*out++ = *in++;
			continue;
		}

		c = in[1];
		in += 2;
		while (c) {
			*out++ = 0;
			c--;
		}
	} while (out - decompressed < row);

	return decompressed;
}

/*
 * R_LeafInVis
 */
static inline boolean_t R_LeafInVis(const r_bsp_leaf_t *leaf, const byte *vis) {
	int c;

	if (!vis)
		return true;

	if ((c = leaf->cluster) == -1)
		return false;

	return vis[c >> 3] & (1 << (c & 7));
}

/*
 * R_MarkLeafs
 *
 * Mark the leafs that are in the PVS for the current cluster, creating the
 * recursion path for R_MarkSurfaces.  Leafs marked for the current cluster
 * will still be frustum-culled, and surfaces therein must still pass a
 * dot-product test in order to be marked as visible for the current frame.
 */
void R_MarkLeafs(void) {
	byte *vis;
	r_bsp_leaf_t *leaf;
	r_bsp_node_t *node;
	int i;

	if (r_lock_vis->value)
		return;

	// resolve current cluster
	r_locals.cluster = R_LeafForPoint(r_view.origin, r_world_model)->cluster;

	if (!r_no_vis->value && (r_locals.old_cluster == r_locals.cluster))
		return;

	r_locals.old_cluster = r_locals.cluster;

	r_locals.vis_frame++;

	if (r_locals.vis_frame == 0x7fff) // avoid overflows, negatives are reserved
		r_locals.vis_frame = 0;

	if (r_no_vis->value || !r_world_model->vis) { // mark everything
		for (i = 0; i < r_world_model->num_leafs; i++)
			r_world_model->leafs[i].vis_frame = r_locals.vis_frame;
		for (i = 0; i < r_world_model->num_nodes; i++)
			r_world_model->nodes[i].vis_frame = r_locals.vis_frame;
		return;
	}

	// resolve pvs for the current cluster
	if (r_locals.cluster != -1)
		vis
				= R_DecompressVis(
						(byte *) r_world_model->vis
								+ r_world_model->vis->bit_offsets[r_locals.cluster][DVIS_PVS]);
	else
		vis = NULL;

	// recurse up the bsp from the visible leafs, marking a path via the nodes
	leaf = r_world_model->leafs;

	for (i = 0; i < r_world_model->num_leafs; i++, leaf++) {

		if (!R_LeafInVis(leaf, vis))
			continue;

		node = (r_bsp_node_t *) leaf;
		while (node) {

			if (node->vis_frame == r_locals.vis_frame)
				break;

			node->vis_frame = r_locals.vis_frame;
			node = node->parent;
		}
	}
}

/*
 * R_LeafInPvs
 *
 * Returns true if the specified leaf is in the PVS for the current frame.
 */
boolean_t R_LeafInPvs(const r_bsp_leaf_t *leaf) {
	return leaf->vis_frame == r_locals.vis_frame;
}
