/*
 * Copyright(c) 1997-2001 id Software, Inc.
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
static const byte *r_bsp_base;

/*
 * @brief Loads the lightmap and deluxemap information into memory so that it
 * may be parsed into GL textures for r_bsp_surface_t. This memory is actually
 * freed once all surfaces are fully loaded.
 */
static void R_LoadBspLightmaps(r_bsp_model_t *bsp, const d_bsp_lump_t *l) {
	const char *c;

	bsp->lightmaps = Mem_LinkMalloc(sizeof(r_bsp_lightmaps_t), bsp);

	if (!l->file_len) {
		bsp->lightmaps->size = 0;
		bsp->lightmaps->data = NULL;
	} else {
		bsp->lightmaps->size = l->file_len;
		bsp->lightmaps->data = Mem_LinkMalloc(l->file_len, bsp);

		memcpy(bsp->lightmaps->data, r_bsp_base + l->file_ofs, l->file_len);
	}

	bsp->lightmaps->scale = DEFAULT_LIGHTMAP_SCALE;

	// resolve lightmap scale
	if ((c = Cm_WorldspawnValue("lightmap_scale"))) {
		bsp->lightmaps->scale = strtoul(c, NULL, 0);
		Com_Debug("Resolved lightmap_scale: %d\n", bsp->lightmaps->scale);
	}
}

/*
 * @brief Loads all r_bsp_cluster_t for the specified BSP model. Note that
 * no information is actually loaded at this point. Rather, space for the
 * clusters is allocated, to be utilized by the PVS algorithm.
 */
static void R_LoadBspClusters(r_bsp_model_t *bsp, const d_bsp_lump_t *l) {

	if (!l->file_len) {
		return;
	}

	d_bsp_vis_t *vis = (d_bsp_vis_t *) (r_bsp_base + l->file_ofs);

	bsp->num_clusters = LittleLong(vis->num_clusters);
	bsp->clusters = Mem_LinkMalloc(bsp->num_clusters * sizeof(r_bsp_cluster_t), bsp);
}

/*
 * @brief Loads all r_bsp_vertex_t for the specified BSP model.
 */
static void R_LoadBspVertexes(r_bsp_model_t *bsp, const d_bsp_lump_t *l) {
	r_bsp_vertex_t *out;

	const d_bsp_vertex_t *in = (const void *) (r_bsp_base + l->file_ofs);

	if (l->file_len % sizeof(*in)) {
		Com_Error(ERR_DROP, "Funny lump size\n");
	}

	bsp->num_vertexes = l->file_len / sizeof(*in);
	bsp->vertexes = out = Mem_LinkMalloc(bsp->num_vertexes * sizeof(*out), bsp);

	for (uint16_t i = 0; i < bsp->num_vertexes; i++, in++, out++) {
		out->position[0] = LittleFloat(in->point[0]);
		out->position[1] = LittleFloat(in->point[1]);
		out->position[2] = LittleFloat(in->point[2]);
	}
}

/*
 * @brief Populates r_bsp_vertex_t normals for the specified BSP model. Vertex
 * normals are packed in a separate lump to maintain compatibility with legacy
 * Quake2 levels.
 */
static void R_LoadBspNormals(r_bsp_model_t *bsp, const d_bsp_lump_t *l) {

	const d_bsp_normal_t *in = (const void *) (r_bsp_base + l->file_ofs);

	if (l->file_len % sizeof(*in)) {
		Com_Error(ERR_DROP, "Funny lump size\n");
	}

	const uint16_t count = l->file_len / sizeof(*in);

	if (count != bsp->num_vertexes) { // ensure sane normals count
		Com_Error(ERR_DROP, "Bad count (%d != %d)\n", count, bsp->num_vertexes);
	}

	r_bsp_vertex_t *out = bsp->vertexes;
	for (uint16_t i = 0; i < bsp->num_vertexes; i++, in++, out++) {
		out->normal[0] = LittleFloat(in->normal[0]);
		out->normal[1] = LittleFloat(in->normal[1]);
		out->normal[2] = LittleFloat(in->normal[2]);
	}
}

/*
 * @brief Returns an approximate radius from the specified bounding box.
 */
static vec_t R_RadiusFromBounds(const vec3_t mins, const vec3_t maxs) {
	vec3_t corner;

	for (int32_t i = 0; i < 3; i++) {
		corner[i] = fabsf(mins[i]) > fabsf(maxs[i]) ? fabsf(mins[i]) : fabsf(maxs[i]);
	}

	return VectorLength(corner);
}

/*
 * @brief Loads all r_bsp_inline_model_t for the specified BSP model. These are
 * later registered as first-class r_model_t's in R_SetupBspInlineModels.
 */
static void R_LoadBspInlineModels(r_bsp_model_t *bsp, const d_bsp_lump_t *l) {
	r_bsp_inline_model_t *out;

	const d_bsp_model_t *in = (const void *) (r_bsp_base + l->file_ofs);

	if (l->file_len % sizeof(*in)) {
		Com_Error(ERR_DROP, "Funny lump size\n");
	}

	bsp->num_inline_models = l->file_len / sizeof(*in);
	bsp->inline_models = out = Mem_LinkMalloc(bsp->num_inline_models * sizeof(*out), bsp);

	for (uint16_t i = 0; i < bsp->num_inline_models; i++, in++, out++) {

		for (int32_t j = 0; j < 3; j++) { // spread the bounds slightly
			out->mins[j] = LittleFloat(in->mins[j]) - 1.0;
			out->maxs[j] = LittleFloat(in->maxs[j]) + 1.0;

			out->origin[j] = LittleFloat(in->origin[j]);
		}
		out->radius = R_RadiusFromBounds(out->mins, out->maxs);

		out->head_node = LittleLong(in->head_node);

		// some (old) maps have invalid inline model head_nodes
		if (out->head_node < 0 || out->head_node >= bsp->num_nodes) {
			Com_Warn("Bad head_node for %d: %d\n", i, out->head_node);
			out->head_node = -1;
		}

		out->first_surface = (uint16_t) LittleLong(in->first_face);
		out->num_surfaces = (uint16_t) LittleLong(in->num_faces);
	}
}

/*
 * @brief Recurses the specified sub-model nodes, assigning the model so that it can
 * be quickly resolved during traces and dynamic light processing.
 */
static void R_SetupBspInlineModel(r_bsp_node_t *node, r_model_t *model) {

	node->model = model;

	if (node->contents != CONTENTS_NODE)
		return;

	R_SetupBspInlineModel(node->children[0], model);
	R_SetupBspInlineModel(node->children[1], model);
}

/*
 * @brief The inline models have been loaded into memory, but are not yet
 * represented as r_model_t. Convert them, and take ownership of their nodes.
 */
static void R_SetupBspInlineModels(r_model_t *mod) {

	for (uint16_t i = 0; i < mod->bsp->num_inline_models; i++) {
		r_model_t *m = Mem_TagMalloc(sizeof(r_model_t), MEM_TAG_RENDERER);

		g_snprintf(m->media.name, sizeof(m->media.name), "%s#%d", mod->media.name, i);
		m->type = MOD_BSP_INLINE;

		m->bsp_inline = &mod->bsp->inline_models[i];

		// copy bounds from the inline model
		VectorCopy(m->bsp_inline->maxs, m->maxs);
		VectorCopy(m->bsp_inline->mins, m->mins);
		m->radius = m->bsp_inline->radius;

		// setup the nodes
		if (m->bsp_inline->head_node != -1) {
			r_bsp_node_t *nodes = &mod->bsp->nodes[m->bsp_inline->head_node];
			R_SetupBspInlineModel(nodes, m);
		}

		// register with the subsystem
		R_RegisterDependency((r_media_t *) mod, (r_media_t *) m);
	}
}

/*
 * @brief Loads all r_bsp_edge_t for the specified BSP model.
 */
static void R_LoadBspEdges(r_bsp_model_t *bsp, const d_bsp_lump_t *l) {
	r_bsp_edge_t *out;

	const d_bsp_edge_t *in = (const void *) (r_bsp_base + l->file_ofs);

	if (l->file_len % sizeof(*in)) {
		Com_Error(ERR_DROP, "Funny lump size\n");
	}

	bsp->num_edges = l->file_len / sizeof(*in);
	bsp->edges = out = Mem_LinkMalloc(bsp->num_edges * sizeof(*out), bsp);

	for (uint32_t i = 0; i < bsp->num_edges; i++, in++, out++) {
		out->v[0] = (uint16_t) LittleShort(in->v[0]);
		out->v[1] = (uint16_t) LittleShort(in->v[1]);
	}
}

/*
 * @brief Loads all r_bsp_texinfo_t for the specified BSP model. Texinfo's
 * are shared by one or more r_bsp_surface_t.
 */
static void R_LoadBspTexinfo(r_bsp_model_t *bsp, const d_bsp_lump_t *l) {
	r_bsp_texinfo_t *out;

	const d_bsp_texinfo_t *in = (const void *) (r_bsp_base + l->file_ofs);

	if (l->file_len % sizeof(*in)) {
		Com_Error(ERR_DROP, "Funny lump size\n");
	}

	bsp->num_texinfo = l->file_len / sizeof(*in);
	bsp->texinfo = out = Mem_LinkMalloc(bsp->num_texinfo * sizeof(*out), bsp);

	for (uint16_t i = 0; i < bsp->num_texinfo; i++, in++, out++) {
		g_strlcpy(out->name, in->texture, sizeof(out->name));

		for (int32_t j = 0; j < 4; j++) {
			out->vecs[0][j] = LittleFloat(in->vecs[0][j]);
			out->vecs[1][j] = LittleFloat(in->vecs[1][j]);
		}

		out->flags = LittleLong(in->flags);
		out->value = LittleLong(in->value);

		out->material = R_LoadMaterial(va("textures/%s", out->name));

		// hack to down-scale high-res textures for legacy levels
		if (bsp->version == BSP_VERSION) {
			void *buffer;

			int64_t len = Fs_Load(va("textures/%s.wal", out->name), &buffer);
			if (len != -1) {
				d_wal_t *wal = (d_wal_t *) buffer;

				out->material->diffuse->width = LittleLong(wal->width);
				out->material->diffuse->height = LittleLong(wal->height);

				Fs_Free(buffer);
			}
		}

		// resolve emissive lighting
		if ((out->flags & SURF_LIGHT) && out->value) {
			VectorScale(out->material->diffuse->color, out->value, out->emissive);
			out->light = ColorNormalize(out->emissive, out->emissive);
		}
	}
}

/*
 * @brief Convenience for resolving r_bsp_vertex_t from surface edges.
 */
#define R_BSP_VERTEX(b, e) ((e) >= 0 ? \
	(&b->vertexes[b->edges[(e)].v[0]]) : (&b->vertexes[b->edges[-(e)].v[1]]) \
)

/*
 * @brief Unwinds the surface, iterating all non-collinear vertices.
 *
 * @return The next winding point, or NULL if the face is completely unwound.
 */
static const vec_t *R_UnwindBspSurface(const r_bsp_model_t *bsp, const r_bsp_surface_t *surf,
		const vec_t *last) {

	const int32_t *se = &bsp->surface_edges[surf->first_edge];
	const vec_t *v0 = R_BSP_VERTEX(bsp, *se)->position;

	if (!last)
		return v0;

	uint16_t i;
	for (i = 0; i < surf->num_edges; i++) {
		const vec_t *v = R_BSP_VERTEX(bsp, se[i])->position;

		if (VectorCompare(last, v)) {
			break;
		}
	}

	while (i < surf->num_edges - 1) {
		const vec_t *v1 = R_BSP_VERTEX(bsp, se[(i + 1) % surf->num_edges])->position;
		const vec_t *v2 = R_BSP_VERTEX(bsp, se[(i + 2) % surf->num_edges])->position;

		vec3_t delta1, delta2;

		VectorSubtract(v1, v0, delta1);
		VectorSubtract(v2, v0, delta2);

		VectorNormalize(delta1);
		VectorNormalize(delta2);

		if (DotProduct(delta1, delta2) < 1.0 - SIDE_EPSILON) {
			return v1;
		}

		i++;
	}

	return NULL;
}

/*
 * @brief Resolves the surface bounding box and lightmap texture coordinates.
 */
static void R_SetupBspSurface(r_bsp_model_t *bsp, r_bsp_surface_t *surf) {
	vec2_t st_mins, st_maxs;

	ClearBounds(surf->mins, surf->maxs);

	st_mins[0] = st_mins[1] = 999999.0;
	st_maxs[0] = st_maxs[1] = -999999.0;

	const r_bsp_texinfo_t *tex = surf->texinfo;

	const int32_t *e = &bsp->surface_edges[surf->first_edge];

	for (uint16_t i = 0; i < surf->num_edges; i++, e++) {
		const r_bsp_vertex_t *v = R_BSP_VERTEX(bsp, *e);

		AddPointToBounds(v->position, surf->mins, surf->maxs); // calculate mins, maxs

		for (int32_t j = 0; j < 2; j++) { // calculate st_mins, st_maxs
			const vec_t val = DotProduct(v->position, tex->vecs[j]) + tex->vecs[j][3];
			if (val < st_mins[j])
				st_mins[j] = val;
			if (val > st_maxs[j])
				st_maxs[j] = val;
		}
	}

	VectorMix(surf->mins, surf->maxs, 0.5, surf->center); // calculate the center

	if (surf->texinfo->light) { // resolve surface area

		const vec_t *v0 = R_UnwindBspSurface(bsp, surf, NULL);
		const vec_t *v1 = R_UnwindBspSurface(bsp, surf, v0);
		const vec_t *v2 = R_UnwindBspSurface(bsp, surf, v1);

		uint16_t tris = 0;
		while (v1 && v2) {
			tris++;
			vec3_t delta1, delta2, cross;

			VectorSubtract(v1, v0, delta1);
			VectorSubtract(v2, v0, delta2);

			CrossProduct(delta1, delta2, cross);
			surf->area += 0.5 * VectorLength(cross);

			v1 = v2;
			v2 = R_UnwindBspSurface(bsp, surf, v1);
		}
	}

	// bump the texture coordinate vectors to ensure we don't split samples
	for (int32_t i = 0; i < 2; i++) {

		const int32_t bmins = floor(st_mins[i] / bsp->lightmaps->scale);
		const int32_t bmaxs = ceil(st_maxs[i] / bsp->lightmaps->scale);

		surf->st_mins[i] = bmins * bsp->lightmaps->scale;
		surf->st_maxs[i] = bmaxs * bsp->lightmaps->scale;

		surf->st_center[i] = (surf->st_maxs[i] + surf->st_mins[i]) / 2.0;
		surf->st_extents[i] = surf->st_maxs[i] - surf->st_mins[i];
	}
}

/*
 * @brief Loads all r_bsp_surface_t for the specified BSP model. Lightmap and
 * deluxemap creation is driven by this function.
 */
static void R_LoadBspSurfaces(r_bsp_model_t *bsp, const d_bsp_lump_t *l) {
	r_bsp_surface_t *out;

	const d_bsp_face_t *in = (const void *) (r_bsp_base + l->file_ofs);

	if (l->file_len % sizeof(*in)) {
		Com_Error(ERR_DROP, "Funny lump size\n");
	}

	bsp->num_surfaces = l->file_len / sizeof(*in);
	bsp->surfaces = out = Mem_LinkMalloc(bsp->num_surfaces * sizeof(*out), bsp);

	R_BeginBspSurfaceLightmaps(bsp);

	for (uint16_t i = 0; i < bsp->num_surfaces; i++, in++, out++) {

		out->first_edge = LittleLong(in->first_edge);
		out->num_edges = LittleShort(in->num_edges);

		// resolve plane
		const uint16_t plane_num = (uint16_t) LittleShort(in->plane_num);
		out->plane = bsp->planes + plane_num;

		// and sidedness
		const int16_t side = LittleShort(in->side);
		if (side) {
			out->flags |= R_SURF_SIDE_BACK;
			VectorNegate(out->plane->normal, out->normal);
		} else
			VectorCopy(out->plane->normal, out->normal);

		// then texinfo
		const uint16_t ti = LittleShort(in->texinfo);
		if (ti >= bsp->num_texinfo) {
			Com_Error(ERR_DROP, "Bad texinfo number: %d\n", ti);
		}
		out->texinfo = bsp->texinfo + ti;

		if (!(out->texinfo->flags & (SURF_WARP | SURF_SKY)))
			out->flags |= R_SURF_LIGHTMAP;

		// and size, texcoords, etc
		R_SetupBspSurface(bsp, out);

		// lastly lighting info
		const int32_t ofs = LittleLong(in->light_ofs);
		const byte *data = (ofs == -1) ? NULL : bsp->lightmaps->data + ofs;

		// to create the lightmap and deluxemap
		R_CreateBspSurfaceLightmap(bsp, out, data);

		// and flare
		R_CreateBspSurfaceFlare(bsp, out);
	}

	R_EndBspSurfaceLightmaps(bsp);

	// free the lightmap lump, we're done with it
	if (bsp->lightmaps->size) {
		Mem_Free(bsp->lightmaps->data);
		bsp->lightmaps->size = 0;
	}
}

/*
 * @brief Recurses the BSP tree, setting the parent field on each node. The
 * recursion stops at leafs. This is used by the PVS algorithm to mark a path
 * to the BSP leaf in which the view origin resides.
 */
static void R_SetupBspNode(r_bsp_node_t *node, r_bsp_node_t *parent) {

	node->parent = parent;

	if (node->contents != CONTENTS_NODE) // leaf
		return;

	R_SetupBspNode(node->children[0], node);
	R_SetupBspNode(node->children[1], node);
}

/*
 * @brief Loads all r_bsp_node_t for the specified BSP model.
 */
static void R_LoadBspNodes(r_bsp_model_t *bsp, const d_bsp_lump_t *l) {
	r_bsp_node_t *out;

	const d_bsp_node_t *in = (const void *) (r_bsp_base + l->file_ofs);

	if (l->file_len % sizeof(*in)) {
		Com_Error(ERR_DROP, "Funny lump size\n");
	}

	bsp->num_nodes = l->file_len / sizeof(*in);
	bsp->nodes = out = Mem_LinkMalloc(bsp->num_nodes * sizeof(*out), bsp);

	for (uint16_t i = 0; i < bsp->num_nodes; i++, in++, out++) {

		for (int32_t j = 0; j < 3; j++) {
			out->mins[j] = LittleShort(in->mins[j]);
			out->maxs[j] = LittleShort(in->maxs[j]);
		}

		const int32_t p = LittleLong(in->plane_num);
		out->plane = bsp->planes + p;

		out->first_surface = (uint16_t) LittleShort(in->first_face);
		out->num_surfaces = (uint16_t) LittleShort(in->num_faces);

		out->contents = CONTENTS_NODE; // differentiate from leafs

		for (int32_t j = 0; j < 2; j++) {
			const int32_t c = LittleLong(in->children[j]);
			if (c >= 0)
				out->children[j] = bsp->nodes + c;
			else
				out->children[j] = (r_bsp_node_t *) (bsp->leafs + (-1 - c));
		}
	}

	R_SetupBspNode(bsp->nodes, NULL); // sets nodes and leafs
}

/*
 * @brief Loads all r_bsp_leaf_t for the specified BSP model.
 */
static void R_LoadBspLeafs(r_bsp_model_t *bsp, const d_bsp_lump_t *l) {
	r_bsp_leaf_t *out;

	const d_bsp_leaf_t *in = (const void *) (r_bsp_base + l->file_ofs);

	if (l->file_len % sizeof(*in)) {
		Com_Error(ERR_DROP, "Funny lump size");
	}

	bsp->num_leafs = l->file_len / sizeof(*in);
	bsp->leafs = out = Mem_LinkMalloc(bsp->num_leafs * sizeof(*out), bsp);

	for (uint16_t i = 0; i < bsp->num_leafs; i++, in++, out++) {

		for (int32_t j = 0; j < 3; j++) {
			out->mins[j] = LittleShort(in->mins[j]);
			out->maxs[j] = LittleShort(in->maxs[j]);
		}

		out->contents = LittleLong(in->contents);

		out->cluster = LittleShort(in->cluster);
		out->area = LittleShort(in->area);

		const uint16_t f = ((uint16_t) LittleShort(in->first_leaf_face));
		out->first_leaf_surface = bsp->leaf_surfaces + f;

		out->num_leaf_surfaces = (uint16_t) LittleShort(in->num_leaf_faces);
	}
}

/*
 * @brief
 */
static void R_LoadBspLeafSurfaces(r_bsp_model_t *bsp, const d_bsp_lump_t *l) {
	r_bsp_surface_t **out;

	const uint16_t *in = (const void *) (r_bsp_base + l->file_ofs);

	if (l->file_len % sizeof(*in)) {
		Com_Error(ERR_DROP, "Funny lump size\n");
	}

	bsp->num_leaf_surfaces = l->file_len / sizeof(*in);
	bsp->leaf_surfaces = out = Mem_LinkMalloc(bsp->num_leaf_surfaces * sizeof(*out), bsp);

	for (uint16_t i = 0; i < bsp->num_leaf_surfaces; i++) {

		const uint16_t j = (uint16_t) LittleShort(in[i]);

		if (j >= bsp->num_surfaces) {
			Com_Error(ERR_DROP, "Bad surface number: %d\n", j);
		}

		out[i] = bsp->surfaces + j;
	}
}

/*
 * @brief
 */
static void R_LoadBspSurfaceEdges(r_bsp_model_t *bsp, const d_bsp_lump_t *l) {

	const int32_t *in = (const void *) (r_bsp_base + l->file_ofs);

	if (l->file_len % sizeof(*in)) {
		Com_Error(ERR_DROP, "Funny lump size\n");
	}

	const int32_t count = l->file_len / sizeof(*in);

	if (count < 1 || count >= MAX_BSP_FACE_EDGES) {
		Com_Error(ERR_DROP, "Bad surfface edges count: %i\n", count);
	}

	int32_t *out = Mem_LinkMalloc(count * sizeof(*out), bsp);

	bsp->surface_edges = out;
	bsp->num_surface_edges = count;

	for (int32_t i = 0; i < count; i++)
		out[i] = LittleLong(in[i]);
}

/*
 * @brief
 */
static void R_LoadBspPlanes(r_bsp_model_t *bsp, const d_bsp_lump_t *l) {

	const d_bsp_plane_t *in = (const void *) (r_bsp_base + l->file_ofs);

	if (l->file_len % sizeof(*in)) {
		Com_Error(ERR_DROP, "Funny lump size\n");
	}

	const int32_t count = l->file_len / sizeof(*in);
	cm_bsp_plane_t *out = Mem_LinkMalloc(count * sizeof(*out), bsp);

	bsp->planes = out;
	bsp->num_planes = count;

	for (int32_t i = 0; i < count; i++, in++, out++) {

		for (int32_t j = 0; j < 3; j++) {
			out->normal[j] = LittleFloat(in->normal[j]);
		}

		out->dist = LittleFloat(in->dist);
		out->type = LittleLong(in->type);
		out->sign_bits = Cm_SignBitsForPlane(out);
		out->num = i;
	}
}

/*
 * @brief Writes vertex data for the given surface to the load model's arrays.
 *
 * @param count The current vertex count for the load model.
 */
static void R_LoadBspVertexArrays_Surface(r_model_t *mod, r_bsp_surface_t *surf, GLuint *count) {

	surf->index = *count;

	const int32_t *e = &mod->bsp->surface_edges[surf->first_edge];

	for (uint16_t i = 0; i < surf->num_edges; i++, e++) {
		const r_bsp_vertex_t *vert = R_BSP_VERTEX(mod->bsp, *e);

		memcpy(&mod->verts[(*count) * 3], vert->position, sizeof(vec3_t));

		// texture directional vectors and offsets
		const vec_t *sdir = surf->texinfo->vecs[0];
		const vec_t soff = surf->texinfo->vecs[0][3];

		const vec_t *tdir = surf->texinfo->vecs[1];
		const vec_t toff = surf->texinfo->vecs[1][3];

		// texture coordinates
		vec_t s = DotProduct(vert->position, sdir) + soff;
		s /= surf->texinfo->material->diffuse->width;

		vec_t t = DotProduct(vert->position, tdir) + toff;
		t /= surf->texinfo->material->diffuse->height;

		mod->texcoords[(*count) * 2 + 0] = s;
		mod->texcoords[(*count) * 2 + 1] = t;

		// lightmap texture coordinates
		if (surf->flags & R_SURF_LIGHTMAP) {
			s = DotProduct(vert->position, sdir) + soff;
			s -= surf->st_mins[0];
			s += surf->lightmap_s * mod->bsp->lightmaps->scale;
			s += mod->bsp->lightmaps->scale / 2.0;
			s /= surf->lightmap->width * mod->bsp->lightmaps->scale;

			t = DotProduct(vert->position, tdir) + toff;
			t -= surf->st_mins[1];
			t += surf->lightmap_t * mod->bsp->lightmaps->scale;
			t += mod->bsp->lightmaps->scale / 2.0;
			t /= surf->lightmap->height * mod->bsp->lightmaps->scale;
		}

		mod->lightmap_texcoords[(*count) * 2 + 0] = s;
		mod->lightmap_texcoords[(*count) * 2 + 1] = t;

		// normal vector, which is per-vertex for SURF_PHONG

		const vec_t *normal;
		if ((surf->texinfo->flags & SURF_PHONG) && !VectorCompare(vert->normal, vec3_origin))
			normal = vert->normal;
		else
			normal = surf->normal;

		memcpy(&mod->normals[(*count) * 3], normal, sizeof(vec3_t));

		// tangent vectors
		vec4_t tangent;
		vec3_t bitangent;

		TangentVectors(normal, sdir, tdir, tangent, bitangent);
		memcpy(&mod->tangents[(*count) * 4], tangent, sizeof(vec4_t));

		(*count)++;
	}
}

/*
 * @brief Generates vertex primitives for the world model by iterating leafs.
 */
static void R_LoadBspVertexArrays(r_model_t *mod) {

	R_AllocVertexArrays(mod);

	GLuint count = 0;

	const r_bsp_leaf_t *leaf = mod->bsp->leafs;
	for (uint16_t i = 0; i < mod->bsp->num_leafs; i++, leaf++) {

		r_bsp_surface_t **s = leaf->first_leaf_surface;
		for (uint16_t j = 0; j < leaf->num_leaf_surfaces; j++, s++) {

			R_LoadBspVertexArrays_Surface(mod, *s, &count);
		}
	}
}

/*
 * @brief Qsort comparator for R_SortBspSurfaceArrays.
 */
static int R_SortBspSurfacesArrays_Compare(const void *s1, const void *s2) {

	const r_bsp_texinfo_t *t1 = (*(r_bsp_surface_t **) s1)->texinfo;
	const r_bsp_texinfo_t *t2 = (*(r_bsp_surface_t **) s2)->texinfo;

	return g_strcmp0(t1->name, t2->name);
}

/*
 * @brief Reorders all surfaces arrays for the world model, grouping the surface
 * pointers by texture. This dramatically reduces glBindTexture calls.
 */
static void R_SortBspSurfacesArrays(r_bsp_model_t *bsp) {

	// sort the surfaces arrays into the per-texture arrays
	const size_t len = sizeof(r_sorted_bsp_surfaces_t) / sizeof(r_bsp_surfaces_t);
	r_bsp_surfaces_t *surfs = (r_bsp_surfaces_t *) bsp->sorted_surfaces;

	for (size_t i = 0; i < len; i++, surfs++) {
		qsort(surfs->surfaces, surfs->count, sizeof(r_bsp_surface_t *),
				R_SortBspSurfacesArrays_Compare);
	}
}

/*
 * @brief Allocate, populate and sort the surfaces arrays for the world model.
 */
static void R_LoadBspSurfacesArrays(r_model_t *mod) {

	// allocate the surfaces array structures
	r_sorted_bsp_surfaces_t *sorted = Mem_LinkMalloc(sizeof(r_sorted_bsp_surfaces_t), mod->bsp);
	mod->bsp->sorted_surfaces = sorted;

	// determine the maximum counts for each rendered type in order to
	// allocate only what is necessary for the specified model
	r_bsp_surface_t *surf = mod->bsp->surfaces;
	for (size_t i = 0; i < mod->bsp->num_surfaces; i++, surf++) {

		if (surf->texinfo->flags & SURF_SKY) {
			sorted->sky.count++;
			continue;
		}

		if (surf->texinfo->flags & (SURF_BLEND_33 | SURF_BLEND_66)) {
			if (surf->texinfo->flags & SURF_WARP)
				sorted->blend_warp.count++;
			else
				sorted->blend.count++;
		} else {
			if (surf->texinfo->flags & SURF_WARP)
				sorted->opaque_warp.count++;
			else if (surf->texinfo->flags & SURF_ALPHA_TEST)
				sorted->alpha_test.count++;
			else
				sorted->opaque.count++;
		}

		if (surf->texinfo->material->flags & STAGE_DIFFUSE)
			sorted->material.count++;

		if (surf->texinfo->material->flags & STAGE_FLARE)
			sorted->flare.count++;

		if (!(surf->texinfo->flags & SURF_WARP))
			sorted->back.count++;
	}

	// allocate the surfaces pointers based on the counts
	const size_t len = sizeof(r_sorted_bsp_surfaces_t) / sizeof(r_bsp_surfaces_t);
	r_bsp_surfaces_t *surfs = (r_bsp_surfaces_t *) sorted;
	for (size_t i = 0; i < len; i++, surfs++) {

		if (surfs->count) {
			surfs->surfaces = Mem_LinkMalloc(surfs->count * sizeof(r_bsp_surface_t **), mod->bsp);
			surfs->count = 0;
		}
	}

	// iterate the surfaces again, populating the allocated arrays based
	// on primary render type
	surf = mod->bsp->surfaces;
	for (size_t i = 0; i < mod->bsp->num_surfaces; i++, surf++) {

		if (surf->texinfo->flags & SURF_SKY) {
			R_SURFACE_TO_SURFACES(&sorted->sky, surf);
			continue;
		}

		if (surf->texinfo->flags & (SURF_BLEND_33 | SURF_BLEND_66)) {
			if (surf->texinfo->flags & SURF_WARP)
				R_SURFACE_TO_SURFACES(&sorted->blend_warp, surf);
			else
				R_SURFACE_TO_SURFACES(&sorted->blend, surf);
		} else {
			if (surf->texinfo->flags & SURF_WARP)
				R_SURFACE_TO_SURFACES(&sorted->opaque_warp, surf);
			else if (surf->texinfo->flags & SURF_ALPHA_TEST)
				R_SURFACE_TO_SURFACES(&sorted->alpha_test, surf);
			else
				R_SURFACE_TO_SURFACES(&sorted->opaque, surf);
		}

		if (surf->texinfo->material->flags & STAGE_DIFFUSE)
			R_SURFACE_TO_SURFACES(&sorted->material, surf);

		if (surf->texinfo->material->flags & STAGE_FLARE)
			R_SURFACE_TO_SURFACES(&sorted->flare, surf);

		if (!(surf->texinfo->flags & SURF_WARP))
			R_SURFACE_TO_SURFACES(&sorted->back, surf);
	}

	// now sort them by texture
	R_SortBspSurfacesArrays(mod->bsp);
}

/*
 * @brief
 */
void R_LoadBspModel(r_model_t *mod, void *buffer) {
	extern void Cl_LoadProgress(int32_t percent);

	// byte-swap the entire header
	d_bsp_header_t header = *(d_bsp_header_t *) buffer;

	for (size_t i = 0; i < sizeof(d_bsp_header_t) / sizeof(int32_t); i++) {
		((int32_t *) &header)[i] = LittleLong(((int32_t *) &header)[i]);
	}

	if (header.version != BSP_VERSION && header.version != BSP_VERSION_Q2W) {
		Com_Error(ERR_DROP, "%s has unsupported version: %d\n", mod->media.name, header.version);
	}

	mod->bsp = Mem_LinkMalloc(sizeof(r_bsp_model_t), mod);
	mod->bsp->version = header.version;

	// set the base pointer for lump loading
	r_bsp_base = (byte *) buffer;

	R_LoadBspVertexes(mod->bsp, &header.lumps[BSP_LUMP_VERTEXES]);
	Cl_LoadProgress(4);

	if (header.version == BSP_VERSION_Q2W) // enhanced format
		R_LoadBspNormals(mod->bsp, &header.lumps[BSP_LUMP_NORMALS]);

	R_LoadBspEdges(mod->bsp, &header.lumps[BSP_LUMP_EDGES]);
	Cl_LoadProgress(8);

	R_LoadBspSurfaceEdges(mod->bsp, &header.lumps[BSP_LUMP_FACE_EDGES]);
	Cl_LoadProgress(12);

	R_LoadBspLightmaps(mod->bsp, &header.lumps[BSP_LUMP_LIGHTMAPS]);
	Cl_LoadProgress(16);

	R_LoadBspPlanes(mod->bsp, &header.lumps[BSP_LUMP_PLANES]);
	Cl_LoadProgress(20);

	R_LoadBspTexinfo(mod->bsp, &header.lumps[BSP_LUMP_TEXINFO]);
	Cl_LoadProgress(24);

	R_LoadBspSurfaces(mod->bsp, &header.lumps[BSP_LUMP_FACES]);
	Cl_LoadProgress(28);

	R_LoadBspLeafSurfaces(mod->bsp, &header.lumps[BSP_LUMP_LEAF_FACES]);
	Cl_LoadProgress(32);

	R_LoadBspLeafs(mod->bsp, &header.lumps[BSP_LUMP_LEAFS]);
	Cl_LoadProgress(36);

	R_LoadBspNodes(mod->bsp, &header.lumps[BSP_LUMP_NODES]);
	Cl_LoadProgress(40);

	R_LoadBspClusters(mod->bsp, &header.lumps[BSP_LUMP_VISIBILITY]);
	Cl_LoadProgress(44);

	R_LoadBspInlineModels(mod->bsp, &header.lumps[BSP_LUMP_MODELS]);
	Cl_LoadProgress(48);

	R_LoadBspLights(mod->bsp);
	Cl_LoadProgress(50);

	Com_Debug("!================================\n");
	Com_Debug("!R_LoadBspModel: %s\n", mod->media.name);
	Com_Debug("!  Verts:          %d\n", mod->bsp->num_vertexes);
	Com_Debug("!  Edges:          %d\n", mod->bsp->num_edges);
	Com_Debug("!  Surface edges:  %d\n", mod->bsp->num_surface_edges);
	Com_Debug("!  Faces:          %d\n", mod->bsp->num_surfaces);
	Com_Debug("!  Nodes:          %d\n", mod->bsp->num_nodes);
	Com_Debug("!  Leafs:          %d\n", mod->bsp->num_leafs);
	Com_Debug("!  Leaf surfaces:  %d\n", mod->bsp->num_leaf_surfaces);
	Com_Debug("!  Clusters:       %d\n", mod->bsp->num_clusters);
	Com_Debug("!  Inline models   %d\n", mod->bsp->num_inline_models);
	Com_Debug("!  Lights:         %d\n", mod->bsp->num_bsp_lights);
	Com_Debug("!================================\n");

	R_SetupBspInlineModels(mod);
	Cl_LoadProgress(52);

	R_LoadBspVertexArrays(mod);
	Cl_LoadProgress(54);

	R_LoadBspSurfacesArrays(mod);
	Cl_LoadProgress(58);

	R_InitElements(mod->bsp);
}
