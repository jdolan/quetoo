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
 * The beginning of the BSP model (disk format) in memory.  All lumps are
 * resolved by a relative offset from this address.
 */
static const byte *mod_base;

/*
 * R_LoadBspLightmaps
 */
static void R_LoadBspLightmaps(const d_bsp_lump_t *l) {
	const char *c;

	if (!l->file_len) {
		r_load_model->lightmap_data = NULL;
		r_load_model->lightmap_scale = DEFAULT_LIGHTMAP_SCALE;
	} else {
		r_load_model->lightmap_data_size = l->file_len;
		r_load_model->lightmap_data = R_HunkAlloc(l->file_len);

		memcpy(r_load_model->lightmap_data, mod_base + l->file_ofs, l->file_len);
	}

	r_load_model->lightmap_scale = DEFAULT_LIGHTMAP_SCALE;

	// resolve lightmap scale
	if ((c = R_WorldspawnValue("lightmap_scale"))) {
		r_load_model->lightmap_scale = strtoul(c, NULL, 0);
		Com_Debug("Resolved lightmap_scale: %d\n", r_load_model->lightmap_scale);
	}
}

/*
 * R_LoadBspVisibility
 */
static void R_LoadBspVisibility(const d_bsp_lump_t *l) {
	int i;

	if (!l->file_len) {
		r_load_model->vis = NULL;
		return;
	}

	r_load_model->vis_size = l->file_len;
	r_load_model->vis = R_HunkAlloc(l->file_len);
	memcpy(r_load_model->vis, mod_base + l->file_ofs, l->file_len);

	r_load_model->vis->num_clusters = LittleLong(
			r_load_model->vis->num_clusters);

	for (i = 0; i < r_load_model->vis->num_clusters; i++) {
		r_load_model->vis->bit_offsets[i][0] = LittleLong(
				r_load_model->vis->bit_offsets[i][0]);
		r_load_model->vis->bit_offsets[i][1] = LittleLong(
				r_load_model->vis->bit_offsets[i][1]);
	}
}

/*
 * R_LoadBspVertexes
 */
static void R_LoadBspVertexes(const d_bsp_lump_t *l) {
	const d_bsp_vertex_t *in;
	r_bsp_vertex_t *out;
	int i, count;

	in = (const void *) (mod_base + l->file_ofs);
	if (l->file_len % sizeof(*in)) {
		Com_Error(ERR_DROP, "R_LoadBspVertexes: Funny lump size in %s.",
				r_load_model->name);
	}
	count = l->file_len / sizeof(*in);
	out = R_HunkAlloc(count * sizeof(*out));

	r_load_model->vertexes = out;
	r_load_model->num_vertexes = count;

	for (i = 0; i < count; i++, in++, out++) {
		out->position[0] = LittleFloat(in->point[0]);
		out->position[1] = LittleFloat(in->point[1]);
		out->position[2] = LittleFloat(in->point[2]);
	}
}

/*
 * R_LoadBspNormals
 */
static void R_LoadBspNormals(const d_bsp_lump_t *l) {
	const d_bsp_normal_t *in;
	r_bsp_vertex_t *out;
	int i, count;

	in = (const void *) (mod_base + l->file_ofs);
	if (l->file_len % sizeof(*in)) {
		Com_Error(ERR_DROP, "R_LoadBspNormals: Funny lump size in %s.",
				r_load_model->name);
	}
	count = l->file_len / sizeof(*in);

	if (count != r_load_model->num_vertexes) { // ensure sane normals count
		Com_Error(
				ERR_DROP,
				"R_LoadBspNormals: unexpected normals count in %s: (%d != %d).",
				r_load_model->name, count, r_load_model->num_vertexes);
	}

	out = r_load_model->vertexes;

	for (i = 0; i < count; i++, in++, out++) {
		out->normal[0] = LittleFloat(in->normal[0]);
		out->normal[1] = LittleFloat(in->normal[1]);
		out->normal[2] = LittleFloat(in->normal[2]);
	}
}

/*
 * R_RadiusFromBounds
 */
static float R_RadiusFromBounds(const vec3_t mins, const vec3_t maxs) {
	int i;
	vec3_t corner;

	for (i = 0; i < 3; i++) {
		corner[i] = fabsf(mins[i]) > fabsf(maxs[i]) ? fabsf(mins[i]) : fabsf(
				maxs[i]);
	}

	return VectorLength(corner);
}

/*
 * R_LoadBspSubmodels
 */
static void R_LoadBspSubmodels(const d_bsp_lump_t *l) {
	const d_bsp_model_t *in;
	r_bsp_submodel_t *out;
	int i, j, count;

	in = (const void *) (mod_base + l->file_ofs);
	if (l->file_len % sizeof(*in)) {
		Com_Error(ERR_DROP, "R_LoadBspSubmodels: Funny lump size in %s.",
				r_load_model->name);
	}

	count = l->file_len / sizeof(*in);
	out = R_HunkAlloc(count * sizeof(*out));

	r_load_model->submodels = out;
	r_load_model->num_submodels = count;

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
 * R_SetupBspSubmodel
 *
 * Recurses the specified submodel nodes, assigning the model so that it can
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
 * R_SetupBspSubmodels
 *
 * The submodels have been loaded into memory, but are not yet
 * represented as model_t.  Convert them.
 */
static void R_SetupBspSubmodels(void) {
	int i;

	for (i = 0; i < r_load_model->num_submodels; i++) {

		r_model_t *mod = &r_inline_models[i];
		const r_bsp_submodel_t *sub = &r_load_model->submodels[i];

		*mod = *r_load_model; // copy most info from world
		mod->type = mod_bsp_submodel;

		snprintf(mod->name, sizeof(mod->name), "*%d", i);

		// copy the rest from the submodel
		VectorCopy(sub->maxs, mod->maxs);
		VectorCopy(sub->mins, mod->mins);
		mod->radius = sub->radius;

		mod->first_node = sub->head_node;
		mod->nodes = &r_load_model->nodes[mod->first_node];

		R_SetupBspSubmodel(mod->nodes, mod);

		mod->first_model_surface = sub->first_face;
		mod->num_model_surfaces = sub->num_faces;
	}
}

/*
 * R_LoadBspEdges
 */
static void R_LoadBspEdges(const d_bsp_lump_t *l) {
	const d_bsp_edge_t *in;
	r_bsp_edge_t *out;
	int i, count;

	in = (const void *) (mod_base + l->file_ofs);
	if (l->file_len % sizeof(*in)) {
		Com_Error(ERR_DROP, "R_LoadBspEdges: Funny lump size in %s.",
				r_load_model->name);
	}

	count = l->file_len / sizeof(*in);
	out = R_HunkAlloc((count + 1) * sizeof(*out));

	r_load_model->edges = out;
	r_load_model->num_edges = count;

	for (i = 0; i < count; i++, in++, out++) {
		out->v[0] = (unsigned short) LittleShort(in->v[0]);
		out->v[1] = (unsigned short) LittleShort(in->v[1]);
	}
}

/*
 * R_LoadBspTexinfo
 */
static void R_LoadBspTexinfo(const d_bsp_lump_t *l) {
	const d_bsp_texinfo_t *in;
	r_bsp_texinfo_t *out;
	int i, j, count;

	in = (const void *) (mod_base + l->file_ofs);
	if (l->file_len % sizeof(*in)) {
		Com_Error(ERR_DROP, "R_LoadBspTexinfo: Funny lump size in %s.",
				r_load_model->name);
	}

	count = l->file_len / sizeof(*in);
	out = R_HunkAlloc(count * sizeof(*out));

	r_load_model->texinfo = out;
	r_load_model->num_texinfo = count;

	for (i = 0; i < count; i++, in++, out++) {
		for (j = 0; j < 8; j++)
			out->vecs[0][j] = LittleFloat(in->vecs[0][j]);

		out->flags = LittleLong(in->flags);
		out->value = LittleLong(in->value);

		strncpy(out->name, in->texture, sizeof(in->texture));
		out->image = R_LoadImage(va("textures/%s", out->name), it_world);
	}
}

/*
 * R_SetupBspSurface
 *
 * Sets in s->mins, s->maxs, s->st_mins, s->st_maxs, ..
 */
static void R_SetupBspSurface(r_bsp_surface_t *surf) {
	vec3_t mins, maxs;
	vec2_t st_mins, st_maxs;
	int i, j;
	const r_bsp_vertex_t *v;
	const r_bsp_texinfo_t *tex;

	VectorSet(mins, 999999, 999999, 999999);
	VectorSet(maxs, -999999, -999999, -999999);

	st_mins[0] = st_mins[1] = 999999;
	st_maxs[0] = st_maxs[1] = -999999;

	tex = surf->texinfo;

	for (i = 0; i < surf->num_edges; i++) {
		const int e = r_load_model->surface_edges[surf->first_edge + i];
		if (e >= 0)
			v = &r_load_model->vertexes[r_load_model->edges[e].v[0]];
		else
			v = &r_load_model->vertexes[r_load_model->edges[-e].v[1]];

		for (j = 0; j < 3; j++) { // calculate mins, maxs
			if (v->position[j] > maxs[j])
				maxs[j] = v->position[j];
			if (v->position[j] < mins[j])
				mins[j] = v->position[j];
		}

		for (j = 0; j < 2; j++) { // calculate st_mins, st_maxs
			const float val = DotProduct(v->position, tex->vecs[j])
					+ tex->vecs[j][3];
			if (val < st_mins[j])
				st_mins[j] = val;
			if (val > st_maxs[j])
				st_maxs[j] = val;
		}
	}

	VectorCopy(mins, surf->mins);
	VectorCopy(maxs, surf->maxs);

	for (i = 0; i < 3; i++)
		surf->center[i] = (surf->mins[i] + surf->maxs[i]) / 2.0;

	for (i = 0; i < 2; i++) {
		const int bmins = floor(st_mins[i] / r_load_model->lightmap_scale);
		const int bmaxs = ceil(st_maxs[i] / r_load_model->lightmap_scale);

		surf->st_mins[i] = bmins * r_load_model->lightmap_scale;
		surf->st_maxs[i] = bmaxs * r_load_model->lightmap_scale;

		surf->st_center[i] = (surf->st_maxs[i] + surf->st_mins[i]) / 2.0;
		surf->st_extents[i] = surf->st_maxs[i] - surf->st_mins[i];
	}
}

/*
 * R_LoadBspSurfaces
 */
static void R_LoadBspSurfaces(const d_bsp_lump_t *l) {
	const d_bsp_face_t *in;
	r_bsp_surface_t *out;
	int i, count, surf_num;
	int plane_num, side;
	int ti;

	in = (const void *) (mod_base + l->file_ofs);
	if (l->file_len % sizeof(*in)) {
		Com_Error(ERR_DROP, "R_LoadBspSurfaces: Funny lump size in %s.",
				r_load_model->name);
	}

	count = l->file_len / sizeof(*in);
	out = R_HunkAlloc(count * sizeof(*out));

	r_load_model->surfaces = out;
	r_load_model->num_surfaces = count;

	R_BeginBuildingLightmaps();

	for (surf_num = 0; surf_num < count; surf_num++, in++, out++) {

		out->first_edge = LittleLong(in->first_edge);
		out->num_edges = LittleShort(in->num_edges);

		// resolve plane
		plane_num = LittleShort(in->plane_num);
		out->plane = r_load_model->planes + plane_num;

		// and sidedness
		side = LittleShort(in->side);
		if (side) {
			out->flags |= R_SURF_SIDE_BACK;
			VectorNegate(out->plane->normal, out->normal);
		} else
			VectorCopy(out->plane->normal, out->normal);

		// then texinfo
		ti = LittleShort(in->texinfo);
		if (ti < 0 || ti >= r_load_model->num_texinfo) {
			Com_Error(ERR_DROP, "R_LoadBspSurfaces: Bad texinfo number: %d.",
					ti);
		}
		out->texinfo = r_load_model->texinfo + ti;

		if (!(out->texinfo->flags & (SURF_WARP | SURF_SKY)))
			out->flags |= R_SURF_LIGHTMAP;

		// and size, texcoords, etc
		R_SetupBspSurface(out);

		// lastly lighting info
		i = LittleLong(in->light_ofs);

		if (i != -1)
			out->samples = r_load_model->lightmap_data + i;
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
 * R_SetupBspNode
 */
static void R_SetupBspNode(r_bsp_node_t *node, r_bsp_node_t *parent) {

	node->parent = parent;

	if (node->contents != CONTENTS_NODE) // leaf
		return;

	R_SetupBspNode(node->children[0], node);
	R_SetupBspNode(node->children[1], node);
}

/*
 * R_LoadBspNodes
 */
static void R_LoadBspNodes(const d_bsp_lump_t *l) {
	int i, j, count, p;
	const d_bsp_node_t *in;
	r_bsp_node_t *out;

	in = (const void *) (mod_base + l->file_ofs);
	if (l->file_len % sizeof(*in)) {
		Com_Error(ERR_DROP, "R_LoadBspNodes: Funny lump size in %s.",
				r_load_model->name);
	}

	count = l->file_len / sizeof(*in);
	out = R_HunkAlloc(count * sizeof(*out));

	r_load_model->nodes = out;
	r_load_model->num_nodes = count;

	for (i = 0; i < count; i++, in++, out++) {

		for (j = 0; j < 3; j++) {
			out->mins[j] = LittleShort(in->mins[j]);
			out->maxs[j] = LittleShort(in->maxs[j]);
		}

		p = LittleLong(in->plane_num);
		out->plane = r_load_model->planes + p;

		out->first_surface = LittleShort(in->first_face);
		out->num_surfaces = LittleShort(in->num_faces);

		out->contents = CONTENTS_NODE; // differentiate from leafs

		for (j = 0; j < 2; j++) {
			p = LittleLong(in->children[j]);
			if (p >= 0)
				out->children[j] = r_load_model->nodes + p;
			else
				out->children[j] = (r_bsp_node_t *) (r_load_model->leafs + (-1
						- p));
		}
	}

	R_SetupBspNode(r_load_model->nodes, NULL); // sets nodes and leafs
}

/*
 * R_LoadBspLeafs
 */
static void R_LoadBspLeafs(const d_bsp_lump_t *l) {
	const d_bsp_leaf_t *in;
	r_bsp_leaf_t *out;
	int i, j, count;

	in = (const void *) (mod_base + l->file_ofs);
	if (l->file_len % sizeof(*in)) {
		Com_Error(ERR_DROP, "R_LoadBspLeafs: Funny lump size in %s.",
				r_load_model->name);
	}

	count = l->file_len / sizeof(*in);
	out = R_HunkAlloc(count * sizeof(*out));

	r_load_model->leafs = out;
	r_load_model->num_leafs = count;

	for (i = 0; i < count; i++, in++, out++) {

		for (j = 0; j < 3; j++) {
			out->mins[j] = LittleShort(in->mins[j]);
			out->maxs[j] = LittleShort(in->maxs[j]);
		}

		out->contents = LittleLong(in->contents);

		out->cluster = LittleShort(in->cluster);
		out->area = LittleShort(in->area);

		out->first_leaf_surface = r_load_model->leaf_surfaces
				+ ((unsigned short) LittleShort(in->first_leaf_face));

		out->num_leaf_surfaces = LittleShort(in->num_leaf_faces);
	}
}

/*
 * R_LoadBspLeafSurfaces
 */
static void R_LoadBspLeafSurfaces(const d_bsp_lump_t *l) {
	int i, count;
	const unsigned short *in;
	r_bsp_surface_t **out;

	in = (const void *) (mod_base + l->file_ofs);
	if (l->file_len % sizeof(*in)) {
		Com_Error(ERR_DROP, "R_LoadBspLeafSurfaces: Funny lump size in %s.",
				r_load_model->name);
	}

	count = l->file_len / sizeof(*in);
	out = R_HunkAlloc(count * sizeof(*out));

	r_load_model->leaf_surfaces = out;
	r_load_model->num_leaf_surfaces = count;

	for (i = 0; i < count; i++) {

		const unsigned short j = LittleShort(in[i]);

		if (j >= r_load_model->num_surfaces) {
			Com_Error(ERR_DROP,
					"R_LoadBspLeafSurfaces: Bad surface number: %d.", j);
		}

		out[i] = r_load_model->surfaces + j;
	}
}

/*
 * R_LoadBspSurfaceEdges
 */
static void R_LoadBspSurfaceEdges(const d_bsp_lump_t *l) {
	int i, count;
	const int *in;
	int *out;

	in = (const void *) (mod_base + l->file_ofs);
	if (l->file_len % sizeof(*in)) {
		Com_Error(ERR_DROP, "R_LoadBspSurfaceEdges: Funny lump size in %s.",
				r_load_model->name);
	}

	count = l->file_len / sizeof(*in);
	if (count < 1 || count >= MAX_BSP_FACE_EDGES) {
		Com_Error(ERR_DROP,
				"R_LoadBspSurfaceEdges: Bad surfface edges count: %i.", count);
	}

	out = R_HunkAlloc(count * sizeof(*out));

	r_load_model->surface_edges = out;
	r_load_model->num_surface_edges = count;

	for (i = 0; i < count; i++)
		out[i] = LittleLong(in[i]);
}

/*
 * R_LoadBspPlanes
 */
static void R_LoadBspPlanes(const d_bsp_lump_t *l) {
	int i, j;
	c_bsp_plane_t *out;
	const d_bsp_plane_t *in;
	int count;

	in = (const void *) (mod_base + l->file_ofs);
	if (l->file_len % sizeof(*in)) {
		Com_Error(ERR_DROP, "R_LoadBspPlanes: Funny lump size in %s.",
				r_load_model->name);
	}

	count = l->file_len / sizeof(*in);
	out = R_HunkAlloc(count * 2 * sizeof(*out));

	r_load_model->planes = out;
	r_load_model->num_planes = count;

	for (i = 0; i < count; i++, in++, out++) {

		for (j = 0; j < 3; j++) {
			out->normal[j] = LittleFloat(in->normal[j]);
		}

		out->dist = LittleFloat(in->dist);
		out->type = LittleLong(in->type);
		out->sign_bits = SignBitsForPlane(out);
	}
}

/*
 * R_AddBspVertexColor
 */
static void R_AddBspVertexColor(r_bsp_vertex_t *vert,
		const r_bsp_surface_t *surf) {
	int i;

	vert->surfaces++;

	for (i = 0; i < 4; i++) {

		const float vc = vert->color[i] * (vert->surfaces - 1) / vert->surfaces;
		const float sc = surf->color[i] / vert->surfaces;

		vert->color[i] = vc + sc;
	}
}

/*
 * R_LoadBspVertexArrays
 */
static void R_LoadBspVertexArrays(void) {
	int i, j;
	int vert_index, texcoord_index, tangent_index, color_index;
	float soff, toff, s, t;
	float *point, *normal, *sdir, *tdir;
	vec4_t tangent;
	vec3_t bitangent;
	r_bsp_surface_t *surf;
	r_bsp_edge_t *edge;
	r_bsp_vertex_t *vert;

	R_AllocVertexArrays(r_load_model); // allocate the arrays

	vert_index = texcoord_index = tangent_index = 0;
	surf = r_load_model->surfaces;

	for (i = 0; i < r_load_model->num_surfaces; i++, surf++) {

		surf->index = vert_index / 3;

		for (j = 0; j < surf->num_edges; j++) {
			const int index = r_load_model->surface_edges[surf->first_edge + j];

			// vertex
			if (index > 0) { // negative indices to differentiate which end of the edge
				edge = &r_load_model->edges[index];
				vert = &r_load_model->vertexes[edge->v[0]];
			} else {
				edge = &r_load_model->edges[-index];
				vert = &r_load_model->vertexes[edge->v[1]];
			}

			point = vert->position;
			memcpy(&r_load_model->verts[vert_index], point, sizeof(vec3_t));

			// texture directional vectors and offsets
			sdir = surf->texinfo->vecs[0];
			soff = surf->texinfo->vecs[0][3];

			tdir = surf->texinfo->vecs[1];
			toff = surf->texinfo->vecs[1][3];

			// texture coordinates
			s = DotProduct(point, sdir) + soff;
			s /= surf->texinfo->image->width;

			t = DotProduct(point, tdir) + toff;
			t /= surf->texinfo->image->height;

			r_load_model->texcoords[texcoord_index + 0] = s;
			r_load_model->texcoords[texcoord_index + 1] = t;

			if (surf->flags & R_SURF_LIGHTMAP) { // lightmap coordinates
				s = DotProduct(point, sdir) + soff;
				s -= surf->st_mins[0];
				s += surf->light_s * r_load_model->lightmap_scale;
				s += r_load_model->lightmap_scale / 2.0;
				s /= r_lightmaps.size * r_load_model->lightmap_scale;

				t = DotProduct(point, tdir) + toff;
				t -= surf->st_mins[1];
				t += surf->light_t * r_load_model->lightmap_scale;
				t += r_load_model->lightmap_scale / 2.0;
				t /= r_lightmaps.size * r_load_model->lightmap_scale;
			}

			r_load_model->lmtexcoords[texcoord_index + 0] = s;
			r_load_model->lmtexcoords[texcoord_index + 1] = t;

			// normal vector
			if ((surf->texinfo->flags & SURF_PHONG) && !VectorCompare(
					vert->normal, vec3_origin)) // phong shaded
				normal = vert->normal;
			else
				// per-plane
				normal = surf->normal;

			memcpy(&r_load_model->normals[vert_index], normal, sizeof(vec3_t));

			// tangent vector
			TangentVectors(normal, sdir, tdir, tangent, bitangent);
			memcpy(&r_load_model->tangents[tangent_index], tangent, sizeof(vec4_t));

			// accumulate colors
			R_AddBspVertexColor(vert, surf);

			vert_index += 3;
			texcoord_index += 2;
			tangent_index += 4;
		}
	}

	color_index = 0;
	surf = r_load_model->surfaces;

	// now iterate over the verts again, assembling the accumulated colors
	for (i = 0; i < r_load_model->num_surfaces; i++, surf++) {

		for (j = 0; j < surf->num_edges; j++) {
			const int index = r_load_model->surface_edges[surf->first_edge + j];

			// vertex
			if (index > 0) { // negative indices to differentiate which end of the edge
				edge = &r_load_model->edges[index];
				vert = &r_load_model->vertexes[edge->v[0]];
			} else {
				edge = &r_load_model->edges[-index];
				vert = &r_load_model->vertexes[edge->v[1]];
			}

			memcpy(&r_load_model->colors[color_index], vert->color, sizeof(vec4_t));
			color_index += 4;
		}
	}
}

// temporary space for sorting surfaces by texnum
static r_bsp_surfaces_t *r_sorted_surfaces[MAX_GL_TEXTURES];

/*
 * R_SortBspSurfacesArrays_
 */
static void R_SortBspSurfacesArrays_(r_bsp_surfaces_t *surfs) {
	unsigned int i, j;

	for (i = 0; i < surfs->count; i++) {

		const int texnum = surfs->surfaces[i]->texinfo->image->texnum;

		R_SurfaceToSurfaces(r_sorted_surfaces[texnum], surfs->surfaces[i]);
	}

	surfs->count = 0;

	for (i = 0; i < r_num_images; i++) {

		r_bsp_surfaces_t *sorted = r_sorted_surfaces[r_images[i].texnum];

		if (sorted && sorted->count) {

			for (j = 0; j < sorted->count; j++)
				R_SurfaceToSurfaces(surfs, sorted->surfaces[j]);

			sorted->count = 0;
		}
	}
}

/*
 * R_SortBspSurfacesArrays
 *
 * Reorders all surfaces arrays for the specified model, grouping the surface
 * pointers by texture.  This dramatically reduces glBindTexture calls.
 */
static void R_SortBspSurfacesArrays(r_model_t *mod) {
	r_bsp_surface_t *surf, *s;
	int i, ns;

	// resolve the start surface and total surface count
	if (mod->type == mod_bsp) { // world model
		s = mod->surfaces;
		ns = mod->num_surfaces;
	} else { // submodels
		s = &mod->surfaces[mod->first_model_surface];
		ns = mod->num_model_surfaces;
	}

	memset(r_sorted_surfaces, 0, sizeof(r_sorted_surfaces));

	// allocate the per-texture surfaces arrays and determine counts
	for (i = 0, surf = s; i < ns; i++, surf++) {

		r_bsp_surfaces_t *surfs =
				r_sorted_surfaces[surf->texinfo->image->texnum];

		if (!surfs) { // allocate it
			surfs = (r_bsp_surfaces_t *) Z_Malloc(sizeof(*surfs));
			r_sorted_surfaces[surf->texinfo->image->texnum] = surfs;
		}

		surfs->count++;
	}

	// allocate the surfaces pointers based on counts
	for (i = 0; i < r_num_images; i++) {

		r_bsp_surfaces_t *surfs = r_sorted_surfaces[r_images[i].texnum];

		if (surfs) {
			surfs->surfaces = (r_bsp_surface_t **) Z_Malloc(
					sizeof(r_bsp_surface_t *) * surfs->count);
			surfs->count = 0;
		}
	}

	// sort the model's surfaces arrays into the per-texture arrays
	for (i = 0; i < NUM_SURFACES_ARRAYS; i++) {

		if (mod->sorted_surfaces[i]->count)
			R_SortBspSurfacesArrays_(mod->sorted_surfaces[i]);
	}

	// free the per-texture surfaces arrays
	for (i = 0; i < r_num_images; i++) {

		r_bsp_surfaces_t *surfs = r_sorted_surfaces[r_images[i].texnum];

		if (surfs) {
			if (surfs->surfaces)
				Z_Free(surfs->surfaces);
			Z_Free(surfs);
		}
	}
}

/*
 * R_LoadBspSurfacesArrays_
 */
static void R_LoadBspSurfacesArrays_(r_model_t *mod) {
	r_bsp_surface_t *surf, *s;
	int i, ns;

	// allocate the surfaces array structures
	for (i = 0; i < NUM_SURFACES_ARRAYS; i++)
		mod->sorted_surfaces[i] = (r_bsp_surfaces_t *) R_HunkAlloc(
				sizeof(r_bsp_surfaces_t));

	// resolve the start surface and total surface count
	if (mod->type == mod_bsp) { // world model
		s = mod->surfaces;
		ns = mod->num_surfaces;
	} else { // submodels
		s = &mod->surfaces[mod->first_model_surface];
		ns = mod->num_model_surfaces;
	}

	// determine the maximum counts for each rendered type in order to
	// allocate only what is necessary for the specified model
	for (i = 0, surf = s; i < ns; i++, surf++) {

		if (surf->texinfo->flags & SURF_SKY) {
			mod->sky_surfaces->count++;
			continue;
		}

		if (surf->texinfo->flags & (SURF_BLEND_33 | SURF_BLEND_66)) {
			if (surf->texinfo->flags & SURF_WARP)
				mod->blend_warp_surfaces->count++;
			else
				mod->blend_surfaces->count++;
		} else {
			if (surf->texinfo->flags & SURF_WARP)
				mod->opaque_warp_surfaces->count++;
			else if (surf->texinfo->flags & SURF_ALPHA_TEST)
				mod->alpha_test_surfaces->count++;
			else
				mod->opaque_surfaces->count++;
		}

		if (surf->texinfo->image->material.flags & STAGE_RENDER)
			mod->material_surfaces->count++;

		if (surf->texinfo->image->material.flags & STAGE_FLARE)
			mod->flare_surfaces->count++;

		if (!(surf->texinfo->flags & SURF_WARP))
			mod->back_surfaces->count++;
	}

	// allocate the surfaces pointers based on the counts
	for (i = 0; i < NUM_SURFACES_ARRAYS; i++) {

		if (mod->sorted_surfaces[i]->count) {
			mod->sorted_surfaces[i]->surfaces
					= (r_bsp_surface_t **) R_HunkAlloc(
							sizeof(r_bsp_surface_t *)
									* mod->sorted_surfaces[i]->count);

			mod->sorted_surfaces[i]->count = 0;
		}
	}

	// iterate the surfaces again, populating the allocated arrays based
	// on primary render type
	for (i = 0, surf = s; i < ns; i++, surf++) {

		if (surf->texinfo->flags & SURF_SKY) {
			R_SurfaceToSurfaces(mod->sky_surfaces, surf);
			continue;
		}

		if (surf->texinfo->flags & (SURF_BLEND_33 | SURF_BLEND_66)) {
			if (surf->texinfo->flags & SURF_WARP)
				R_SurfaceToSurfaces(mod->blend_warp_surfaces, surf);
			else
				R_SurfaceToSurfaces(mod->blend_surfaces, surf);
		} else {
			if (surf->texinfo->flags & SURF_WARP)
				R_SurfaceToSurfaces(mod->opaque_warp_surfaces, surf);
			else if (surf->texinfo->flags & SURF_ALPHA_TEST)
				R_SurfaceToSurfaces(mod->alpha_test_surfaces, surf);
			else
				R_SurfaceToSurfaces(mod->opaque_surfaces, surf);
		}

		if (surf->texinfo->image->material.flags & STAGE_RENDER)
			R_SurfaceToSurfaces(mod->material_surfaces, surf);

		if (surf->texinfo->image->material.flags & STAGE_FLARE)
			R_SurfaceToSurfaces(mod->flare_surfaces, surf);

		if (!(surf->texinfo->flags & SURF_WARP))
			R_SurfaceToSurfaces(mod->back_surfaces, surf);
	}

	// now sort them by texture
	R_SortBspSurfacesArrays(mod);
}

/*
 * R_LoadBspSurfacesArrays
 */
static void R_LoadBspSurfacesArrays(void) {
	int i;

	R_LoadBspSurfacesArrays_(r_load_model);

	for (i = 0; i < r_load_model->num_submodels; i++) {
		R_LoadBspSurfacesArrays_(&r_inline_models[i]);
	}
}

/*
 * R_LoadBspModel
 */
void R_LoadBspModel(r_model_t *mod, void *buffer) {
	extern void Cl_LoadProgress(int percent);
	d_bsp_header_t header;
	unsigned int i;

	if (r_world_model)
		Com_Error(ERR_DROP, "R_LoadBspModel: Loaded bsp model after world.");

	header = *(d_bsp_header_t *) buffer;
	for (i = 0; i < sizeof(d_bsp_header_t) / 4; i++)
		((int *) &header)[i] = LittleLong(((int *) &header)[i]);

	if (header.version != BSP_VERSION && header.version != BSP_VERSION_) {
		Com_Error(ERR_DROP, "R_LoadBspModel: %s has unsupported version: %d",
				mod->name, header.version);
	}

	mod->type = mod_bsp;
	mod->version = header.version;

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

	R_LoadBspVisibility(&header.lumps[LUMP_VISIBILITY]);
	Cl_LoadProgress(36);

	R_LoadBspLeafs(&header.lumps[LUMP_LEAFS]);
	Cl_LoadProgress(40);

	R_LoadBspNodes(&header.lumps[LUMP_NODES]);
	Cl_LoadProgress(44);

	R_LoadBspSubmodels(&header.lumps[LUMP_MODELS]);
	Cl_LoadProgress(48);

	R_SetupBspSubmodels();

	Com_Debug("================================\n");
	Com_Debug("R_LoadBspModel: %s\n", r_load_model->name);
	Com_Debug("  Verts:          %d\n", r_load_model->num_vertexes);
	Com_Debug("  Edges:          %d\n", r_load_model->num_edges);
	Com_Debug("  Surface edges:  %d\n", r_load_model->num_surface_edges);
	Com_Debug("  Faces:          %d\n", r_load_model->num_surfaces);
	Com_Debug("  Nodes:          %d\n", r_load_model->num_nodes);
	Com_Debug("  Leafs:          %d\n", r_load_model->num_leafs);
	Com_Debug("  Leaf surfaces:  %d\n", r_load_model->num_leaf_surfaces);
	Com_Debug("  Models:         %d\n", r_load_model->num_submodels);
	Com_Debug("  Lightmaps:      %d\n", r_load_model->lightmap_data_size);
	Com_Debug("  Vis:            %d\n", r_load_model->vis_size);

	if (r_load_model->vis)
		Com_Debug("  Clusters:   %d\n", r_load_model->vis->num_clusters);

	Com_Debug("================================\n");

	R_LoadBspVertexArrays();

	R_LoadBspSurfacesArrays();

	R_LoadBspLights();

	r_locals.old_cluster = -1; // force bsp iteration
}
