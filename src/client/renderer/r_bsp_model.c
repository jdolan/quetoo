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

/*
 * The beginning of the BSP model (disk format) in memory. All lumps are
 * resolved by a relative offset from this address.
 */
static const byte *mod_base;

/*
 * @brief
 */
static void R_LoadBspLightmaps(const d_bsp_lump_t *l) {
	const char *c;

	if (!l->file_len) {
		r_models.load->bsp->lightmap_data = NULL;
		r_models.load->bsp->lightmap_scale = DEFAULT_LIGHTMAP_SCALE;
	} else {
		r_models.load->bsp->lightmap_data_size = l->file_len;
		r_models.load->bsp->lightmap_data = R_HunkAlloc(l->file_len);

		memcpy(r_models.load->bsp->lightmap_data, mod_base + l->file_ofs, l->file_len);
	}

	r_models.load->bsp->lightmap_scale = DEFAULT_LIGHTMAP_SCALE;

	// resolve lightmap scale
	if ((c = R_WorldspawnValue("lightmap_scale"))) {
		r_models.load->bsp->lightmap_scale = strtoul(c, NULL, 0);
		Com_Debug("Resolved lightmap_scale: %d\n", r_models.load->bsp->lightmap_scale);
	}
}

/*
 * @brief Ensures all textures appearing in the specified leaf are allocated an
 * array in the given cluster. We rely on R_HunkAlloc to provide contiguous
 * blocks here, as we assign the first array and then simply concatenate for
 * each thereafter.
 */
static void R_LeafToCluster(r_bsp_cluster_t *c, const r_bsp_leaf_t *l) {
	uint16_t i, j;

	r_bsp_surface_t **s = l->first_leaf_surface;
	for (i = 0; i < l->num_leaf_surfaces; i++, s++) {

		r_bsp_texinfo_t *t = (*s)->texinfo;

		r_bsp_array_t *a = c->arrays;
		for (j = 0; j < c->num_arrays; j++, a++) {
			if (t == a->texinfo) {
				break;
			}
		}

		if (j == c->num_arrays) {
			a = R_HunkAlloc(sizeof(r_bsp_array_t));
			a->texinfo = t;
			if (0 == c->num_arrays) {
				c->arrays = a;
			}
			c->num_arrays++;
		}
	}
}

/*
 * @brief
 */
static void R_LoadBspClusters(const d_bsp_lump_t *l) {
	r_bsp_cluster_t *c;
	uint16_t i, j;

	if (!l->file_len) {
		return;
	}

	d_bsp_vis_t *vis = (d_bsp_vis_t *) (mod_base + l->file_ofs);

	r_models.load->bsp->num_clusters = LittleLong(vis->num_clusters);
	c = r_models.load->bsp->clusters = R_HunkAlloc(r_models.load->bsp->num_clusters * sizeof(r_bsp_cluster_t));

	for (i = 0; i < r_models.load->bsp->num_clusters; i++, c++) {

		const r_bsp_leaf_t *l = r_models.load->bsp->leafs;
		for (j = 0; j < r_models.load->bsp->num_leafs; j++, l++) {

			if (i == l->cluster) {
				R_LeafToCluster(c, l);
			}
		}
	}
}

/*
 * @brief
 */
static void R_LoadBspVertexes(const d_bsp_lump_t *l) {
	const d_bsp_vertex_t *in;
	r_bsp_vertex_t *out;
	int32_t i, count;

	in = (const void *) (mod_base + l->file_ofs);
	if (l->file_len % sizeof(*in)) {
		Com_Error(ERR_DROP, "R_LoadBspVertexes: Funny lump size in %s.\n", r_models.load->name);
	}
	count = l->file_len / sizeof(*in);
	out = R_HunkAlloc(count * sizeof(*out));

	r_models.load->bsp->vertexes = out;
	r_models.load->bsp->num_vertexes = count;

	for (i = 0; i < count; i++, in++, out++) {
		out->position[0] = LittleFloat(in->point[0]);
		out->position[1] = LittleFloat(in->point[1]);
		out->position[2] = LittleFloat(in->point[2]);
	}
}

/*
 * @brief
 */
static void R_LoadBspNormals(const d_bsp_lump_t *l) {
	const d_bsp_normal_t *in;
	r_bsp_vertex_t *out;
	int32_t i, count;

	in = (const void *) (mod_base + l->file_ofs);
	if (l->file_len % sizeof(*in)) {
		Com_Error(ERR_DROP, "R_LoadBspNormals: Funny lump size in %s.\n", r_models.load->name);
	}
	count = l->file_len / sizeof(*in);

	if (count != r_models.load->bsp->num_vertexes) { // ensure sane normals count
		Com_Error(ERR_DROP, "R_LoadBspNormals: unexpected normals count in %s: (%d != %d).\n",
				r_models.load->name, count, r_models.load->bsp->num_vertexes);
	}

	out = r_models.load->bsp->vertexes;

	for (i = 0; i < count; i++, in++, out++) {
		out->normal[0] = LittleFloat(in->normal[0]);
		out->normal[1] = LittleFloat(in->normal[1]);
		out->normal[2] = LittleFloat(in->normal[2]);
	}
}

/*
 * @brief
 */
static float R_RadiusFromBounds(const vec3_t mins, const vec3_t maxs) {
	int32_t i;
	vec3_t corner;

	for (i = 0; i < 3; i++) {
		corner[i] = fabsf(mins[i]) > fabsf(maxs[i]) ? fabsf(mins[i]) : fabsf(maxs[i]);
	}

	return VectorLength(corner);
}

/*
 * @brief
 */
static void R_LoadBspSubmodels(const d_bsp_lump_t *l) {
	const d_bsp_model_t *in;
	r_bsp_submodel_t *out;
	int32_t i, j, count;

	in = (const void *) (mod_base + l->file_ofs);
	if (l->file_len % sizeof(*in)) {
		Com_Error(ERR_DROP, "R_LoadBspSubmodels: Funny lump size in %s.\n", r_models.load->name);
	}

	count = l->file_len / sizeof(*in);
	out = R_HunkAlloc(count * sizeof(*out));

	r_models.load->bsp->submodels = out;
	r_models.load->bsp->num_submodels = count;

	for (i = 0; i < count; i++, in++, out++) {
		for (j = 0; j < 3; j++) { // spread the mins / maxs by 1 unit
			out->mins[j] = LittleFloat(in->mins[j]) - 1.0;
			out->maxs[j] = LittleFloat(in->maxs[j]) + 1.0;
			out->origin[j] = LittleFloat(in->origin[j]);
		}
		out->radius = R_RadiusFromBounds(out->mins, out->maxs);

		out->head_node = LittleLong(in->head_node);

		out->first_face = LittleLong(in->first_face);
		out->num_faces = LittleLong(in->num_faces);
	}
}

/*
 * @brief Recurses the specified sub-model nodes, assigning the model so that it can
 * be quickly resolved during traces and dynamic light processing.
 */
static void R_SetupBspSubmodel(r_bsp_node_t *node, r_model_t *model) {

	node->model = model;

	if (node->contents != CONTENTS_NODE)
		return;

	R_SetupBspSubmodel(node->children[0], model);
	R_SetupBspSubmodel(node->children[1], model);
}

/*
 * @brief The sub-models have been loaded into memory, but are not yet
 * represented as r_model_t. Convert them.
 */
static void R_SetupBspSubmodels(void) {
	int32_t i;

	for (i = 0; i < r_models.load->bsp->num_submodels; i++) {

		r_model_t *mod = &r_models.bsp_models[i];
		const r_bsp_submodel_t *sub = &r_models.load->bsp->submodels[i];

		*mod = *r_models.load; // copy most info from world
		mod->type = mod_bsp_submodel;

		mod->bsp = R_HunkAlloc(sizeof(r_bsp_model_t)); // including all bsp lumps
		*mod->bsp = *r_models.load->bsp;

		snprintf(mod->name, sizeof(mod->name), "*%d", i);

		// copy the rest from the submodel
		VectorCopy(sub->maxs, mod->maxs);
		VectorCopy(sub->mins, mod->mins);
		mod->radius = sub->radius;

		// Some (old) maps have invalid sub-model head nodes
		if (sub->head_node < 0 || sub->head_node >= r_models.load->bsp->num_nodes) {
			Com_Warn("R_LoadBspSubmodels: Invalid head_node for %d: %d\n", i, sub->head_node);
			mod->bsp->first_node = 0;
			mod->bsp->nodes = NULL;
		} else {
			mod->bsp->first_node = sub->head_node;
			mod->bsp->nodes = &r_models.load->bsp->nodes[mod->bsp->first_node];

			R_SetupBspSubmodel(mod->bsp->nodes, mod);
		}

		mod->bsp->first_model_surface = sub->first_face;
		mod->bsp->num_model_surfaces = sub->num_faces;
	}
}

/*
 * @brief
 */
static void R_LoadBspEdges(const d_bsp_lump_t *l) {
	const d_bsp_edge_t *in;
	r_bsp_edge_t *out;
	int32_t i, count;

	in = (const void *) (mod_base + l->file_ofs);
	if (l->file_len % sizeof(*in)) {
		Com_Error(ERR_DROP, "R_LoadBspEdges: Funny lump size in %s.\n", r_models.load->name);
	}

	count = l->file_len / sizeof(*in);
	out = R_HunkAlloc((count + 1) * sizeof(*out));

	r_models.load->bsp->edges = out;
	r_models.load->bsp->num_edges = count;

	for (i = 0; i < count; i++, in++, out++) {
		out->v[0] = (uint16_t) LittleShort(in->v[0]);
		out->v[1] = (uint16_t) LittleShort(in->v[1]);
	}
}

/*
 * @brief
 */
static void R_LoadBspTexinfo(const d_bsp_lump_t *l) {
	const d_bsp_texinfo_t *in;
	r_bsp_texinfo_t *out;
	int32_t i, j, count;

	in = (const void *) (mod_base + l->file_ofs);
	if (l->file_len % sizeof(*in)) {
		Com_Error(ERR_DROP, "R_LoadBspTexinfo: Funny lump size in %s.\n", r_models.load->name);
	}

	count = l->file_len / sizeof(*in);
	out = R_HunkAlloc(count * sizeof(*out));

	r_models.load->bsp->texinfo = out;
	r_models.load->bsp->num_texinfo = count;

	for (i = 0; i < count; i++, in++, out++) {
		strncpy(out->name, in->texture, sizeof(in->texture));

		for (j = 0; j < 8; j++)
			out->vecs[0][j] = LittleFloat(in->vecs[0][j]);

		out->flags = LittleLong(in->flags);
		out->value = LittleLong(in->value);

		out->material = R_LoadMaterial(va("textures/%s", out->name));
	}
}

/*
 * @brief Resolves the surface bounding box and lightmap texture coordinates.
 */
static void R_SetupBspSurface(r_bsp_surface_t *surf) {
	vec2_t st_mins, st_maxs;
	int32_t i, j;

	ClearBounds(surf->mins, surf->maxs);

	st_mins[0] = st_mins[1] = 999999.0;
	st_maxs[0] = st_maxs[1] = -999999.0;

	const r_bsp_texinfo_t *tex = surf->texinfo;

	for (i = 0; i < surf->num_edges; i++) {
		const int32_t e = r_models.load->bsp->surface_edges[surf->first_edge + i];
		const r_bsp_vertex_t *v;

		if (e >= 0)
			v = &r_models.load->bsp->vertexes[r_models.load->bsp->edges[e].v[0]];
		else
			v = &r_models.load->bsp->vertexes[r_models.load->bsp->edges[-e].v[1]];

		AddPointToBounds(v->position, surf->mins, surf->maxs); // calculate mins, maxs

		for (j = 0; j < 2; j++) { // calculate st_mins, st_maxs
			const float val = DotProduct(v->position, tex->vecs[j]) + tex->vecs[j][3];
			if (val < st_mins[j])
				st_mins[j] = val;
			if (val > st_maxs[j])
				st_maxs[j] = val;
		}
	}

	VectorMix(surf->mins, surf->maxs, 0.5, surf->center); // calculate the center

	// bump the texture coordinate vectors to ensure we don't split samples
	for (i = 0; i < 2; i++) {
		const int32_t bmins = floor(st_mins[i] / r_models.load->bsp->lightmap_scale);
		const int32_t bmaxs = ceil(st_maxs[i] / r_models.load->bsp->lightmap_scale);

		surf->st_mins[i] = bmins * r_models.load->bsp->lightmap_scale;
		surf->st_maxs[i] = bmaxs * r_models.load->bsp->lightmap_scale;

		surf->st_center[i] = (surf->st_maxs[i] + surf->st_mins[i]) / 2.0;
		surf->st_extents[i] = surf->st_maxs[i] - surf->st_mins[i];
	}
}

/*
 * @brief
 */
static void R_LoadBspSurfaces(const d_bsp_lump_t *l) {
	const d_bsp_face_t *in;
	r_bsp_surface_t *out;
	int32_t i, count, surf_num;
	int32_t plane_num, side;
	int32_t ti;

	in = (const void *) (mod_base + l->file_ofs);
	if (l->file_len % sizeof(*in)) {
		Com_Error(ERR_DROP, "R_LoadBspSurfaces: Funny lump size in %s.\n", r_models.load->name);
	}

	count = l->file_len / sizeof(*in);
	out = R_HunkAlloc(count * sizeof(*out));

	r_models.load->bsp->surfaces = out;
	r_models.load->bsp->num_surfaces = count;

	R_BeginBuildingLightmaps();

	for (surf_num = 0; surf_num < count; surf_num++, in++, out++) {

		out->first_edge = LittleLong(in->first_edge);
		out->num_edges = LittleShort(in->num_edges);

		// resolve plane
		plane_num = LittleShort(in->plane_num);
		out->plane = r_models.load->bsp->planes + plane_num;

		// and sidedness
		side = LittleShort(in->side);
		if (side) {
			out->flags |= R_SURF_SIDE_BACK;
			VectorNegate(out->plane->normal, out->normal);
		} else
			VectorCopy(out->plane->normal, out->normal);

		// then texinfo
		ti = LittleShort(in->texinfo);
		if (ti < 0 || ti >= r_models.load->bsp->num_texinfo) {
			Com_Error(ERR_DROP, "R_LoadBspSurfaces: Bad texinfo number: %d.\n", ti);
		}
		out->texinfo = r_models.load->bsp->texinfo + ti;

		if (!(out->texinfo->flags & (SURF_WARP | SURF_SKY)))
			out->flags |= R_SURF_LIGHTMAP;

		// and size, texcoords, etc
		R_SetupBspSurface(out);

		// lastly lighting info
		i = LittleLong(in->light_ofs);

		if (i != -1)
			out->samples = r_models.load->bsp->lightmap_data + i;
		else
			out->samples = NULL;

		// create lightmaps
		R_CreateSurfaceLightmap(out);

		// and flare
		R_CreateSurfaceFlare(out);
	}

	R_EndBuildingLightmaps();
}

/*
 * @brief
 */
static void R_SetupBspNode(r_bsp_node_t *node, r_bsp_node_t *parent) {

	node->parent = parent;

	if (node->contents != CONTENTS_NODE) // leaf
		return;

	R_SetupBspNode(node->children[0], node);
	R_SetupBspNode(node->children[1], node);
}

/*
 * @brief
 */
static void R_LoadBspNodes(const d_bsp_lump_t *l) {
	int32_t i, j, count, p;
	const d_bsp_node_t *in;
	r_bsp_node_t *out;

	in = (const void *) (mod_base + l->file_ofs);
	if (l->file_len % sizeof(*in)) {
		Com_Error(ERR_DROP, "R_LoadBspNodes: Funny lump size in %s.\n", r_models.load->name);
	}

	count = l->file_len / sizeof(*in);
	out = R_HunkAlloc(count * sizeof(*out));

	r_models.load->bsp->nodes = out;
	r_models.load->bsp->num_nodes = count;

	for (i = 0; i < count; i++, in++, out++) {

		for (j = 0; j < 3; j++) {
			out->mins[j] = LittleShort(in->mins[j]);
			out->maxs[j] = LittleShort(in->maxs[j]);
		}

		p = LittleLong(in->plane_num);
		out->plane = r_models.load->bsp->planes + p;

		out->first_surface = LittleShort(in->first_face);
		out->num_surfaces = LittleShort(in->num_faces);

		out->contents = CONTENTS_NODE; // differentiate from leafs

		for (j = 0; j < 2; j++) {
			p = LittleLong(in->children[j]);
			if (p >= 0)
				out->children[j] = r_models.load->bsp->nodes + p;
			else
				out->children[j] = (r_bsp_node_t *) (r_models.load->bsp->leafs + (-1 - p));
		}
	}

	R_SetupBspNode(r_models.load->bsp->nodes, NULL); // sets nodes and leafs
}

/*
 * @brief
 */
static void R_LoadBspLeafs(const d_bsp_lump_t *l) {
	const d_bsp_leaf_t *in;
	r_bsp_leaf_t *out;
	int32_t i, j, count;

	in = (const void *) (mod_base + l->file_ofs);
	if (l->file_len % sizeof(*in)) {
		Com_Error(ERR_DROP, "R_LoadBspLeafs: Funny lump size in %s.\n", r_models.load->name);
	}

	count = l->file_len / sizeof(*in);
	out = R_HunkAlloc(count * sizeof(*out));

	r_models.load->bsp->leafs = out;
	r_models.load->bsp->num_leafs = count;

	for (i = 0; i < count; i++, in++, out++) {

		for (j = 0; j < 3; j++) {
			out->mins[j] = LittleShort(in->mins[j]);
			out->maxs[j] = LittleShort(in->maxs[j]);
		}

		out->contents = LittleLong(in->contents);

		out->cluster = LittleShort(in->cluster);
		out->area = LittleShort(in->area);

		out->first_leaf_surface = r_models.load->bsp->leaf_surfaces + ((uint16_t) LittleShort(
				in->first_leaf_face));

		out->num_leaf_surfaces = LittleShort(in->num_leaf_faces);
	}
}

/*
 * @brief
 */
static void R_LoadBspLeafSurfaces(const d_bsp_lump_t *l) {
	int32_t i, count;
	const uint16_t *in;
	r_bsp_surface_t **out;

	in = (const void *) (mod_base + l->file_ofs);
	if (l->file_len % sizeof(*in)) {
		Com_Error(ERR_DROP, "R_LoadBspLeafSurfaces: Funny lump size in %s.\n", r_models.load->name);
	}

	count = l->file_len / sizeof(*in);
	out = R_HunkAlloc(count * sizeof(*out));

	r_models.load->bsp->leaf_surfaces = out;
	r_models.load->bsp->num_leaf_surfaces = count;

	for (i = 0; i < count; i++) {

		const uint16_t j = LittleShort(in[i]);

		if (j >= r_models.load->bsp->num_surfaces) {
			Com_Error(ERR_DROP, "R_LoadBspLeafSurfaces: Bad surface number: %d.", j);
		}

		out[i] = r_models.load->bsp->surfaces + j;
	}
}

/*
 * @brief
 */
static void R_LoadBspSurfaceEdges(const d_bsp_lump_t *l) {
	int32_t i, count;
	const int32_t *in;
	int32_t *out;

	in = (const void *) (mod_base + l->file_ofs);
	if (l->file_len % sizeof(*in)) {
		Com_Error(ERR_DROP, "R_LoadBspSurfaceEdges: Funny lump size in %s.\n", r_models.load->name);
	}

	count = l->file_len / sizeof(*in);
	if (count < 1 || count >= MAX_BSP_FACE_EDGES) {
		Com_Error(ERR_DROP, "R_LoadBspSurfaceEdges: Bad surfface edges count: %i.\n", count);
	}

	out = R_HunkAlloc(count * sizeof(*out));

	r_models.load->bsp->surface_edges = out;
	r_models.load->bsp->num_surface_edges = count;

	for (i = 0; i < count; i++)
		out[i] = LittleLong(in[i]);
}

/*
 * @brief
 */
static void R_LoadBspPlanes(const d_bsp_lump_t *l) {
	int32_t i, j;
	c_bsp_plane_t *out;
	const d_bsp_plane_t *in;
	int32_t count;

	in = (const void *) (mod_base + l->file_ofs);
	if (l->file_len % sizeof(*in)) {
		Com_Error(ERR_DROP, "R_LoadBspPlanes: Funny lump size in %s.\n", r_models.load->name);
	}

	count = l->file_len / sizeof(*in);
	out = R_HunkAlloc(count * 2 * sizeof(*out));

	r_models.load->bsp->planes = out;
	r_models.load->bsp->num_planes = count;

	for (i = 0; i < count; i++, in++, out++) {

		for (j = 0; j < 3; j++) {
			out->normal[j] = LittleFloat(in->normal[j]);
		}

		out->dist = LittleFloat(in->dist);
		out->type = LittleLong(in->type);
		out->sign_bits = SignBitsForPlane(out);
	}
}


#define COPY_VERT(mod, v0, v1) {\
	memcpy(&mod->verts[(v0) * 3], &mod->verts[(v1) * 3], sizeof(vec3_t)); \
	memcpy(&mod->texcoords[(v0) * 2], &mod->texcoords[(v1) * 2], sizeof(vec2_t)); \
	memcpy(&mod->lightmap_texcoords[(v0) * 2], &mod->lightmap_texcoords[(v1) * 2], sizeof(vec2_t)); \
	memcpy(&mod->normals[(v0) * 3], &mod->normals[(v1) * 3], sizeof(vec3_t)); \
	memcpy(&mod->tangents[(v0) * 4], &mod->tangents[(v1) * 4], sizeof(vec4_t)); \
}

/*
 * @brief Writes vertex data for the given surface to the load model's arrays.
 *
 * @param count The current vertex count for the load model.
 */
static void R_LoadBspVertexArrays_Surface(const r_bsp_surface_t *surf, GLuint *count) {
	uint16_t i;

	for (i = 0; i < surf->num_edges; i++) {

		if (i > 2) {
			COPY_VERT(r_models.load, (*count), (*count) - i);
			COPY_VERT(r_models.load, (*count) + 1, (*count) - 1);
			(*count) += 2;
		}

		const int32_t index = r_models.load->bsp->surface_edges[surf->first_edge + i];

		const r_bsp_edge_t *edge;
		r_bsp_vertex_t *vert;

		// vertex
		if (index > 0) { // negative indices to differentiate which end of the edge
			edge = &r_models.load->bsp->edges[index];
			vert = &r_models.load->bsp->vertexes[edge->v[0]];
		} else {
			edge = &r_models.load->bsp->edges[-index];
			vert = &r_models.load->bsp->vertexes[edge->v[1]];
		}

		memcpy(&r_models.load->verts[(*count) * 3], vert->position, sizeof(vec3_t));

		// texture directional vectors and offsets
		const float *sdir = surf->texinfo->vecs[0];
		const float soff = surf->texinfo->vecs[0][3];

		const float *tdir = surf->texinfo->vecs[1];
		const float toff = surf->texinfo->vecs[1][3];

		// texture coordinates
		float s = DotProduct(vert->position, sdir) + soff;
		s /= surf->texinfo->material->diffuse->width;

		float t = DotProduct(vert->position, tdir) + toff;
		t /= surf->texinfo->material->diffuse->height;

		r_models.load->texcoords[(*count) * 2 + 0] = s;
		r_models.load->texcoords[(*count) * 2 + 1] = t;

		// lightmap texture coordinates
		if (surf->flags & R_SURF_LIGHTMAP) {
			s = DotProduct(vert->position, sdir) + soff;
			s -= surf->st_mins[0];
			s += surf->light_s * r_models.load->bsp->lightmap_scale;
			s += r_models.load->bsp->lightmap_scale / 2.0;
			s /= r_lightmaps.block_size * r_models.load->bsp->lightmap_scale;

			t = DotProduct(vert->position, tdir) + toff;
			t -= surf->st_mins[1];
			t += surf->light_t * r_models.load->bsp->lightmap_scale;
			t += r_models.load->bsp->lightmap_scale / 2.0;
			t /= r_lightmaps.block_size * r_models.load->bsp->lightmap_scale;
		}

		r_models.load->lightmap_texcoords[(*count) * 2 + 0] = s;
		r_models.load->lightmap_texcoords[(*count) * 2 + 1] = t;

		// normal vector, which is per-vertex for SURF_PHONG

		const float *normal;
		if ((surf->texinfo->flags & SURF_PHONG) && !VectorCompare(vert->normal, vec3_origin))
			normal = vert->normal;
		else
			normal = surf->normal;

		memcpy(&r_models.load->normals[(*count) * 3], normal, sizeof(vec3_t));

		// tangent vectors
		vec4_t tangent;
		vec3_t bitangent;

		TangentVectors(normal, sdir, tdir, tangent, bitangent);
		memcpy(&r_models.load->tangents[(*count) * 4], tangent, sizeof(vec4_t));

		(*count)++;
	}
}

/*
 * @brief Generates vertex primitives for the world model by iterating clusters
 * and then the arrays within each cluster. This produces contiguous geometry
 * for each array, conveniently ordered by texture.
 */
static void R_LoadBspVertexArrays(void) {
	uint16_t i, j, k, l;

	R_AllocVertexArrays(r_models.load);

	GLuint count = 0;

	const r_bsp_cluster_t *cluster = r_models.load->bsp->clusters;
	for (i = 0; i < r_models.load->bsp->num_clusters; i++, cluster++) {

		r_bsp_array_t *array = cluster->arrays;
		for (j = 0; j < cluster->num_arrays; j++, array++) {

			array->index = count;

			const r_bsp_leaf_t *leaf = r_models.load->bsp->leafs;
			for (k = 0; k < r_models.load->bsp->num_leafs; k++, leaf++) {

				if (i == leaf->cluster) {

					r_bsp_surface_t **s = leaf->first_leaf_surface;
					for (l = 0; l < leaf->num_leaf_surfaces; l++, s++) {

						if ((*s)->texinfo == array->texinfo) {
							R_LoadBspVertexArrays_Surface(*s, &count);
						}
					}
				}
			}
			array->count = count - array->index;
		}
	}
}

/*
 * @brief
 */
void R_LoadBspModel(r_model_t *mod, void *buffer) {
	extern void Cl_LoadProgress(int32_t percent);
	d_bsp_header_t header;
	uint32_t i;

	if (r_models.world) {
		Com_Error(ERR_DROP, "R_LoadBspModel: Load BSP model after world\n");
	}

	header = *(d_bsp_header_t *) buffer;
	for (i = 0; i < sizeof(d_bsp_header_t) / 4; i++)
		((int32_t *) &header)[i] = LittleLong(((int32_t *) &header)[i]);

	if (header.version != BSP_VERSION && header.version != BSP_VERSION_) {
		Com_Error(ERR_DROP, "R_LoadBspModel: %s has unsupported version: %d\n", mod->name,
				header.version);
	}

	mod->type = mod_bsp;
	mod->version = header.version;

	mod->bsp = R_HunkAlloc(sizeof(r_bsp_model_t));

	// set the base pointer for lump loading
	mod_base = (byte *) buffer;

	// load into heap
	R_LoadBspVertexes(&header.lumps[LUMP_VERTEXES]);
	Cl_LoadProgress(4);

	if (header.version == BSP_VERSION_) // enhanced format
		R_LoadBspNormals(&header.lumps[LUMP_NORMALS]);

	R_LoadBspEdges(&header.lumps[LUMP_EDGES]);
	Cl_LoadProgress(8);

	R_LoadBspSurfaceEdges(&header.lumps[LUMP_FACE_EDGES]);
	Cl_LoadProgress(12);

	R_LoadBspLightmaps(&header.lumps[LUMP_LIGHMAPS]);
	Cl_LoadProgress(16);

	R_LoadBspPlanes(&header.lumps[LUMP_PLANES]);
	Cl_LoadProgress(20);

	R_LoadBspTexinfo(&header.lumps[LUMP_TEXINFO]);
	Cl_LoadProgress(24);

	R_LoadBspSurfaces(&header.lumps[LUMP_FACES]);
	Cl_LoadProgress(28);

	R_LoadBspLeafSurfaces(&header.lumps[LUMP_LEAF_FACES]);
	Cl_LoadProgress(32);

	R_LoadBspLeafs(&header.lumps[LUMP_LEAFS]);
	Cl_LoadProgress(36);

	R_LoadBspNodes(&header.lumps[LUMP_NODES]);
	Cl_LoadProgress(40);

	R_LoadBspClusters(&header.lumps[LUMP_VISIBILITY]);
	Cl_LoadProgress(44);

	R_LoadBspSubmodels(&header.lumps[LUMP_MODELS]);
	Cl_LoadProgress(48);

	R_SetupBspSubmodels();

	Com_Debug("================================\n");
	Com_Debug("R_LoadBspModel: %s\n", r_models.load->name);
	Com_Debug("  Verts:          %d\n", r_models.load->bsp->num_vertexes);
	Com_Debug("  Edges:          %d\n", r_models.load->bsp->num_edges);
	Com_Debug("  Surface edges:  %d\n", r_models.load->bsp->num_surface_edges);
	Com_Debug("  Faces:          %d\n", r_models.load->bsp->num_surfaces);
	Com_Debug("  Nodes:          %d\n", r_models.load->bsp->num_nodes);
	Com_Debug("  Leafs:          %d\n", r_models.load->bsp->num_leafs);
	Com_Debug("  Leaf surfaces:  %d\n", r_models.load->bsp->num_leaf_surfaces);
	Com_Debug("  Clusters:       %d\n", r_models.load->bsp->num_clusters);
	Com_Debug("  Models:         %d\n", r_models.load->bsp->num_submodels);
	Com_Debug("  Lightmaps:      %d\n", r_models.load->bsp->lightmap_data_size);
	Com_Debug("================================\n");

	R_LoadBspVertexArrays();

	R_LoadBspLights();

	r_locals.old_cluster = -1; // force bsp iteration
}
