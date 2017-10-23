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
#include "client.h"

/**
 * @brief Structures used for intermediate representation of data
 * to compile unique vertex list and element array
 */
typedef struct {
	vec3_t position;
	vec3_t normal;
} r_bsp_vertex_t;

typedef struct {
	r_model_t *mod;
	GHashTable *hash_table;
	size_t count;

	uint16_t num_vertexes;
	r_bsp_vertex_t *vertexes;
} r_bsp_unique_verts_t;

static r_bsp_unique_verts_t r_unique_vertices;

/**
 * @brief Loads all r_bsp_vertex_t for the specified BSP model.
 */
static void R_LoadBspVertexes(r_bsp_model_t *bsp) {
	r_bsp_vertex_t *out;

	const bsp_vertex_t *in = bsp->file->vertexes;

	r_unique_vertices.num_vertexes = bsp->file->num_vertexes;
	r_unique_vertices.vertexes = out = Mem_LinkMalloc(r_unique_vertices.num_vertexes * sizeof(*out), bsp);

	for (uint16_t i = 0; i < r_unique_vertices.num_vertexes; i++, in++, out++) {
		VectorCopy(in->point, out->position);
	}
}

/**
 * @brief Populates r_bsp_vertex_t normals for the specified BSP model. Vertex
 * normals are packed in a separate lump to maintain compatibility with legacy
 * Quake2 levels.
 */
static void R_LoadBspNormals(r_bsp_model_t *bsp) {
	const bsp_normal_t *in = bsp->file->normals;

	if (bsp->file->num_normals != r_unique_vertices.num_vertexes) { // ensure sane normals count
		Com_Error(ERROR_DROP, "Bad count (%d != %d)\n", bsp->file->num_normals, r_unique_vertices.num_vertexes);
	}

	r_bsp_vertex_t *out = r_unique_vertices.vertexes;

	for (uint16_t i = 0; i < r_unique_vertices.num_vertexes; i++, in++, out++) {
		VectorCopy(in->normal, out->normal);
	}
}

/**
 * @brief Loads the lightmap and deluxemap information into memory so that it
 * may be parsed into GL textures for r_bsp_surface_t. This memory is actually
 * freed once all surfaces are fully loaded.
 */
static void R_LoadBspLightmaps(r_bsp_model_t *bsp) {
	const char *c;

	bsp->lightmap_scale = BSP_DEFAULT_LIGHTMAP_SCALE;

	// resolve lightmap scale
	if ((c = Cm_WorldspawnValue("lightmap_scale"))) {
		bsp->lightmap_scale = strtof(c, NULL);
		Com_Debug(DEBUG_RENDERER, "Resolved lightmap_scale: %1.3f\n", bsp->lightmap_scale);
	}
}

/**
 * @brief
 */
static void R_LoadBspPlanes(r_bsp_model_t *bsp) {

	bsp->plane_shadows = Mem_LinkMalloc(bsp->file->num_planes * sizeof(uint16_t), bsp);
}

/**
 * @brief Loads all r_bsp_texinfo_t for the specified BSP model. Texinfo's
 * are shared by one or more r_bsp_surface_t.
 */
static void R_LoadBspTexinfo(r_bsp_model_t *bsp) {
	r_bsp_texinfo_t *out;

	const bsp_texinfo_t *in = bsp->file->texinfo;

	bsp->num_texinfo = bsp->file->num_texinfo;
	bsp->texinfo = out = Mem_LinkMalloc(bsp->num_texinfo * sizeof(*out), bsp);

	GHashTable *wal_files = NULL;

	for (uint16_t i = 0; i < bsp->num_texinfo; i++, in++, out++) {
		g_strlcpy(out->name, in->texture, sizeof(out->name));

		Vector4Copy(in->vecs[0], out->vecs[0]);
		Vector4Copy(in->vecs[1], out->vecs[1]);

		out->scale[0] = 1.0 / VectorLength(out->vecs[0]);
		out->scale[1] = 1.0 / VectorLength(out->vecs[1]);

		out->flags = in->flags;
		out->value = in->value;

		out->material = R_LoadMaterial(out->name, ASSET_CONTEXT_TEXTURES);

		// hack to down-scale high-res textures for legacy levels
		if (bsp->version == BSP_VERSION) {

			if (!wal_files) {
				wal_files = g_hash_table_new(g_direct_hash, g_direct_equal);
			}

			if (!g_hash_table_contains(wal_files, out->material)) {
				g_hash_table_add(wal_files, out->material);

				void *buffer;

				int64_t len = Fs_Load(va("textures/%s.wal", out->name), &buffer);
				if (len != -1) {
					d_wal_t *wal = (d_wal_t *) buffer;

					out->material->diffuse->width = LittleLong(wal->width);
					out->material->diffuse->height = LittleLong(wal->height);

					Fs_Free(buffer);
				}
			}
		}

		// resolve emissive lighting
		if ((out->flags & SURF_LIGHT) && out->value) {
			VectorScale(out->material->diffuse->color, out->value, out->emissive);
			out->light = ColorNormalize(out->emissive, out->emissive);
		}
	}

	if (wal_files) {
		g_hash_table_destroy(wal_files);
	}
}

/**
 * @brief Convenience for resolving r_bsp_vertex_t from surface edges.
 */
#define R_BSP_VERTEX(b, e) ((e) >= 0 ? \
	(&r_unique_vertices.vertexes[b->file->edges[(e)].v[0]]) : (&r_unique_vertices.vertexes[b->file->edges[-(e)].v[1]]) \
)

/**
 * @brief Unwinds the surface, iterating all non-collinear vertices.
 *
 * @return The next winding point, or `NULL` if the face is completely unwound.
 */
static const r_bsp_vertex_t *R_UnwindBspSurface(const r_bsp_model_t *bsp,
        const r_bsp_surface_t *surf, uint16_t *index) {

	const int32_t *edges = &bsp->file->face_edges[surf->first_edge];

	const r_bsp_vertex_t *v0 = R_BSP_VERTEX(bsp, edges[*index]);
	while (*index < surf->num_edges - 1) {

		const r_bsp_vertex_t *v1 = R_BSP_VERTEX(bsp, edges[(*index + 1) % surf->num_edges]);
		const r_bsp_vertex_t *v2 = R_BSP_VERTEX(bsp, edges[(*index + 2) % surf->num_edges]);

		*index = *index + 1;

		vec3_t delta1, delta2;

		VectorSubtract(v1->position, v0->position, delta1);
		VectorSubtract(v2->position, v0->position, delta2);

		VectorNormalize(delta1);
		VectorNormalize(delta2);

		if (DotProduct(delta1, delta2) < 1.0 - SIDE_EPSILON) {
			return v1;
		}
	}

	return NULL;
}

/**
 * @brief Resolves the surface bounding box and lightmap texture coordinates.
 */
static void R_SetupBspSurface(r_bsp_model_t *bsp, r_bsp_surface_t *surf) {
	vec2_t st_mins, st_maxs;

	ClearBounds(surf->mins, surf->maxs);

	st_mins[0] = st_mins[1] = 999999.0;
	st_maxs[0] = st_maxs[1] = -999999.0;

	const r_bsp_texinfo_t *tex = surf->texinfo;

	const int32_t *e = &bsp->file->face_edges[surf->first_edge];

	for (uint16_t i = 0; i < surf->num_edges; i++, e++) {
		const r_bsp_vertex_t *v = R_BSP_VERTEX(bsp, *e);

		AddPointToBounds(v->position, surf->mins, surf->maxs); // calculate mins, maxs

		for (int32_t j = 0; j < 2; j++) { // calculate st_mins, st_maxs
			const vec_t val = DotProduct(v->position, tex->vecs[j]) + tex->vecs[j][3];
			if (val < st_mins[j]) {
				st_mins[j] = val;
			}
			if (val > st_maxs[j]) {
				st_maxs[j] = val;
			}
		}
	}

	VectorMix(surf->mins, surf->maxs, 0.5, surf->center); // calculate the center

	if (surf->texinfo->light) { // resolve surface area

		uint16_t vert = 0;

		const r_bsp_vertex_t *v0 = R_UnwindBspSurface(bsp, surf, &vert);
		const r_bsp_vertex_t *v1 = R_UnwindBspSurface(bsp, surf, &vert);
		const r_bsp_vertex_t *v2 = R_UnwindBspSurface(bsp, surf, &vert);

		uint16_t tris = 0;
		while (v1 && v2) {
			tris++;
			vec3_t delta1, delta2, cross;

			VectorSubtract(v1->position, v0->position, delta1);
			VectorSubtract(v2->position, v0->position, delta2);

			CrossProduct(delta1, delta2, cross);
			surf->area += 0.5 * VectorLength(cross);

			v1 = v2;
			v2 = R_UnwindBspSurface(bsp, surf, &vert);
		}
	}

	// bump the texture coordinate vectors to ensure we don't split samples
	for (int32_t i = 0; i < 2; i++) {

		const int32_t bmins = floor(st_mins[i] * bsp->lightmap_scale);
		const int32_t bmaxs = ceil(st_maxs[i] * bsp->lightmap_scale);

		surf->st_mins[i] = bmins / bsp->lightmap_scale;
		surf->st_maxs[i] = bmaxs / bsp->lightmap_scale;

		surf->st_center[i] = (surf->st_maxs[i] + surf->st_mins[i]) / 2.0;

		const vec_t size = surf->st_maxs[i] - surf->st_mins[i];

		surf->lightmap_size[i] = (r_pixel_t) ((size * bsp->lightmap_scale) + 1.0);
	}
}

/**
 * @brief Loads all r_bsp_surface_t for the specified BSP model. Lightmap and
 * deluxemap creation is driven by this function.
 */
static void R_LoadBspSurfaces(r_bsp_model_t *bsp) {

	static r_bsp_texinfo_t null_texinfo;

	r_bsp_surface_t *out;
	const bsp_face_t *in = bsp->file->faces;

	uint32_t start = SDL_GetTicks();

	bsp->num_surfaces = bsp->file->num_faces;
	bsp->surfaces = out = Mem_LinkMalloc(bsp->num_surfaces * sizeof(*out), bsp);

	for (uint16_t i = 0; i < bsp->num_surfaces; i++, in++, out++) {

		out->first_edge = in->first_edge;
		out->num_edges = in->num_edges;

		// resolve plane
		const uint16_t plane_num = in->plane_num;
		out->plane = bsp->cm->planes + plane_num;

		// and sidedness
		const int16_t side = in->side;
		if (side) {
			out->flags |= R_SURF_PLANE_BACK;
			VectorNegate(out->plane->normal, out->normal);
		} else {
			VectorCopy(out->plane->normal, out->normal);
		}

		// then texinfo
		const uint16_t ti = in->texinfo;
		if (ti == USHRT_MAX) {
			out->texinfo = &null_texinfo;
		} else {
			if (ti >= bsp->num_texinfo) {
				Com_Error(ERROR_DROP, "Bad texinfo number: %d\n", ti);
			}
			out->texinfo = bsp->texinfo + ti;
		}

		if (!(out->texinfo->flags & (SURF_WARP | SURF_SKY))) {
			out->flags |= R_SURF_LIGHTMAP;
		}

		// and size, texcoords, etc
		R_SetupBspSurface(bsp, out);

		// make room for elements
		out->elements = Mem_LinkMalloc(sizeof(GLuint) * out->num_edges, bsp);

		// lastly lighting info
		const int32_t ofs = in->light_ofs;
		const byte *data = (ofs == -1) ? NULL : bsp->file->lightmap_data + ofs;

		// to create the lightmap and deluxemap
		R_CreateBspSurfaceLightmap(bsp, out, data);

		// and flare
		R_CreateBspSurfaceFlare(bsp, out);
	}

	R_EndBspSurfaceLightmaps(bsp);

	// free the lightmap lump, we're done with it
	if (bsp->file->lightmap_data_size) {
		out = bsp->surfaces;

		for (uint16_t i = 0; i < bsp->num_surfaces; i++, out++) {
			out->lightmap_input = NULL;
		}
	}

	uint32_t end = SDL_GetTicks();
	Com_Verbose("Generated lightmaps in %u ms\n", end - start);
}

/**
 * @brief
 */
static void R_LoadBspLeafSurfaces(r_bsp_model_t *bsp) {
	r_bsp_surface_t **out;

	const uint16_t *in = bsp->file->leaf_faces;

	bsp->num_leaf_surfaces = bsp->file->num_leaf_faces;
	bsp->leaf_surfaces = out = Mem_LinkMalloc(bsp->num_leaf_surfaces * sizeof(*out), bsp);

	for (uint16_t i = 0; i < bsp->num_leaf_surfaces; i++) {

		const uint16_t j = in[i];

		if (j >= bsp->num_surfaces) {
			Com_Error(ERROR_DROP, "Bad surface number: %d\n", j);
		}

		out[i] = bsp->surfaces + j;
	}
}

/**
 * @brief Loads all r_bsp_leaf_t for the specified BSP model.
 */
static void R_LoadBspLeafs(r_bsp_model_t *bsp) {
	r_bsp_leaf_t *out;

	const bsp_leaf_t *in = bsp->file->leafs;

	bsp->num_leafs = bsp->file->num_leafs;
	bsp->leafs = out = Mem_LinkMalloc(bsp->num_leafs * sizeof(*out), bsp);

	for (uint16_t i = 0; i < bsp->num_leafs; i++, in++, out++) {

		VectorCopy(in->mins, out->mins);
		VectorCopy(in->maxs, out->maxs);

		out->contents = in->contents;

		out->cluster = in->cluster;
		out->area = in->area;

		const uint16_t f = in->first_leaf_face;
		out->first_leaf_surface = bsp->leaf_surfaces + f;

		out->num_leaf_surfaces = in->num_leaf_faces;

		if (out->contents & MASK_LIQUID) {
			for (int32_t j = 0; j < out->num_leaf_surfaces; j++) {
				if ((out->first_leaf_surface[j]->texinfo->flags & (SURF_SKY | SURF_BLEND_33 | SURF_BLEND_66 | SURF_WARP))) {
					continue;
				}

				out->first_leaf_surface[j]->flags |= R_SURF_UNDERLIQUID;
			}
		}
	}
}

/**
 * @brief Recurses the BSP tree, setting the parent field on each node. The
 * recursion stops at leafs. This is used by the PVS algorithm to mark a path
 * to the BSP leaf in which the view origin resides.
 */
static void R_SetupBspNode(r_bsp_node_t *node, r_bsp_node_t *parent) {

	node->parent = parent;

	if (node->contents != CONTENTS_NODE) { // leaf
		return;
	}

	R_SetupBspNode(node->children[0], node);
	R_SetupBspNode(node->children[1], node);
}

/**
 * @brief Loads all r_bsp_node_t for the specified BSP model.
 */
static void R_LoadBspNodes(r_bsp_model_t *bsp) {
	r_bsp_node_t *out;

	const bsp_node_t *in = bsp->file->nodes;

	bsp->num_nodes = bsp->file->num_nodes;
	bsp->nodes = out = Mem_LinkMalloc(bsp->num_nodes * sizeof(*out), bsp);

	for (uint16_t i = 0; i < bsp->num_nodes; i++, in++, out++) {

		VectorCopy(in->mins, out->mins);
		VectorCopy(in->maxs, out->maxs);

		const int32_t p = in->plane_num;
		out->plane = bsp->cm->planes + p;

		out->first_surface = in->first_face;
		out->num_surfaces = in->num_faces;

		out->contents = CONTENTS_NODE; // differentiate from leafs

		for (int32_t j = 0; j < 2; j++) {
			const int32_t c = in->children[j];

			if (c >= 0) {
				out->children[j] = bsp->nodes + c;
			} else {
				out->children[j] = (r_bsp_node_t *) (bsp->leafs + (-1 - c));
			}
		}
	}

	R_SetupBspNode(bsp->nodes, NULL); // sets nodes and leafs
}

/**
 * @brief Loads all r_bsp_cluster_t for the specified BSP model. Note that
 * no information is actually loaded at this point. Rather, space for the
 * clusters is allocated, to be utilized by the PVS algorithm.
 */
static void R_LoadBspClusters(r_bsp_model_t *bsp) {

	if (!bsp->file->vis_data_size) {
		return;
	}

	bsp_vis_t *vis = bsp->file->vis_data.vis;

	bsp->num_clusters = vis->num_clusters;
	bsp->clusters = Mem_LinkMalloc(bsp->num_clusters * sizeof(r_bsp_cluster_t), bsp);
}

/**
 * @brief Recurses the specified sub-model nodes, assigning the model so that it can
 * be quickly resolved during traces and dynamic light processing.
 */
static void R_SetupBspInlineModel(r_bsp_node_t *node, r_model_t *model) {

	node->model = model;

	if (node->contents != CONTENTS_NODE) {
		return;
	}

	R_SetupBspInlineModel(node->children[0], model);
	R_SetupBspInlineModel(node->children[1], model);
}

/**
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

/**
 * @brief Loads all r_bsp_inline_model_t for the specified BSP model. These are
 * later registered as first-class r_model_t's in R_SetupBspInlineModels.
 */
static void R_LoadBspInlineModels(r_bsp_model_t *bsp) {
	r_bsp_inline_model_t *out;

	const bsp_model_t *in = bsp->file->models;

	bsp->num_inline_models = bsp->file->num_models;;
	bsp->inline_models = out = Mem_LinkMalloc(bsp->num_inline_models * sizeof(*out), bsp);

	for (uint16_t i = 0; i < bsp->num_inline_models; i++, in++, out++) {

		for (int32_t j = 0; j < 3; j++) { // spread the bounds slightly
			out->mins[j] = in->mins[j] - 1.0;
			out->maxs[j] = in->maxs[j] + 1.0;

			out->origin[j] = in->origin[j];
		}

		out->radius = RadiusFromBounds(out->mins, out->maxs);

		out->head_node = in->head_node;

		// some (old) maps have invalid inline model head_nodes
		if (out->head_node < 0 || out->head_node >= bsp->num_nodes) {
			Com_Warn("Bad head_node for %d: %d\n", i, out->head_node);
			out->head_node = -1;
		}

		out->first_surface = in->first_face;
		out->num_surfaces = in->num_faces;
	}
}

#define BSP_VERTEX_INDEX_FOR_KEY(ptr) ((GLuint) (ptrdiff_t) (ptr))
#define BSP_VERTEX_INDEX_AS_KEY(i) ((gpointer) (ptrdiff_t) *(i))

#define vec2_hash(v) ((uint32_t)((v)[0] * 73856093) ^ (uint32_t)((v)[1] * 83492791))
#define vec3_hash(v) ((uint32_t)((v)[0] * 73856093) ^ (uint32_t)((v)[1] * 19349663) ^ (uint32_t)((v)[2] * 83492791))

/**
 * @brief The hash function for unique vertices.
 * @see R_LoadBspVertexArrays_VertexElement
 */
static guint R_UniqueVerts_HashFunc(gconstpointer key) {
	const GLuint vi = BSP_VERTEX_INDEX_FOR_KEY(key);

	return	vec3_hash(r_unique_vertices.mod->bsp->verts[vi]) ^
	        vec3_hash(r_unique_vertices.mod->bsp->normals[vi]) ^
	        vec2_hash(r_unique_vertices.mod->bsp->texcoords[vi]);
}

/**
 * @brief GEqualFunc for resolving unique vertices.
 * @param a A vertex index.
 * @param b A vertex index.
 * @return True of the vertices at indices `a` and `b` are equal.
 * @remarks While the vertex indices are used as the hash keys, equality is determined by a deep
 * comparison of the vertex attributes at those indices.
 * @see R_LoadBspVertexArrays_VertexElement
 */
static gboolean R_UniqueVerts_EqualFunc(gconstpointer a, gconstpointer b) {

	const GLuint va = BSP_VERTEX_INDEX_FOR_KEY(a);
	const GLuint vb = BSP_VERTEX_INDEX_FOR_KEY(b);

	if (va != vb) {

		return false;
	}

	const r_bsp_model_t *bsp = r_unique_vertices.mod->bsp;

	return	memcmp(bsp->verts[va], bsp->verts[vb], sizeof(vec3_t)) == 0 &&
	        memcmp(bsp->normals[va], bsp->normals[vb], sizeof(vec3_t)) == 0 &&
	        memcmp(bsp->texcoords[va], bsp->texcoords[vb], sizeof(vec2_t)) == 0 &&
	        memcmp(bsp->lightmap_texcoords[va], bsp->lightmap_texcoords[vb], sizeof(vec2_t)) == 0 &&
	        memcmp(bsp->tangents[va], bsp->tangents[vb], sizeof(vec4_t)) == 0;
}

/**
 * @brief Attempts to find an element for the vertex at `vertex_index`. If none exists, a new
 * element is inserted, and its index returned.
 *
 * @returns If a vertex has already been written to the vertex array list that matches
 * the vertex at index *vertex_index, the index of that vertex will be returned. Otherwise,
 * it will return *vertex_index and increase it by 1.
 *
 * @note The hash table lookup uses indexes into the various vertex arrays to act as the
 * key and value storages. The hash functions convert the pointer to an index for comparison,
 * and in here it searches the table by *vertex_index (and inserts it if need be) to trigger
 * the comparison functions that do the magic. It's easy to get lost in the indirection.
 */
static GLuint R_LoadBspVertexArrays_VertexElement(GLuint *vertex_index) {

	gpointer value = g_hash_table_lookup(r_unique_vertices.hash_table, BSP_VERTEX_INDEX_AS_KEY(vertex_index));

	if (value) {
		return BSP_VERTEX_INDEX_FOR_KEY(value);
	}

	g_hash_table_add(r_unique_vertices.hash_table, BSP_VERTEX_INDEX_AS_KEY(vertex_index));
	return (*vertex_index)++;
}

/**
 * @brief Writes vertex data for the given surface to the load model's arrays.
 *
 * @param elements The current element count for the load model.
 * @param vertices The current unique vertex count for the load model.
 */
static void R_LoadBspVertexArrays_Surface(r_model_t *mod, r_bsp_surface_t *surf, GLuint *elements, GLuint *vertices) {

	surf->index = *elements;

	const int32_t *e = &mod->bsp->file->face_edges[surf->first_edge];

	for (uint16_t i = 0; i < surf->num_edges; i++, e++) {
		const r_bsp_vertex_t *vert = R_BSP_VERTEX(mod->bsp, *e);

		VectorCopy(vert->position, mod->bsp->verts[*vertices]);

		// texture directional vectors and offsets
		const vec_t *sdir = surf->texinfo->vecs[0];
		const vec_t soff = surf->texinfo->vecs[0][3];

		const vec_t *tdir = surf->texinfo->vecs[1];
		const vec_t toff = surf->texinfo->vecs[1][3];

		// texture coordinates
		vec_t s = DotProduct(vert->position, sdir) + soff;
		vec_t t = DotProduct(vert->position, tdir) + toff;

		mod->bsp->texcoords[*vertices][0] = s / surf->texinfo->material->diffuse->width;
		mod->bsp->texcoords[*vertices][1] = t / surf->texinfo->material->diffuse->height;

		// lightmap texture coordinates
		if (surf->flags & R_SURF_LIGHTMAP) {
			s -= surf->st_mins[0];
			s += surf->lightmap_s / mod->bsp->lightmap_scale;
			s += (1.0 / mod->bsp->lightmap_scale) / 2.0;
			s /= surf->lightmap->width / mod->bsp->lightmap_scale;

			t -= surf->st_mins[1];
			t += surf->lightmap_t / mod->bsp->lightmap_scale;
			t += (1.0 / mod->bsp->lightmap_scale) / 2.0;
			t /= surf->lightmap->height / mod->bsp->lightmap_scale;
		}

		mod->bsp->lightmap_texcoords[*vertices][0] = s;
		mod->bsp->lightmap_texcoords[*vertices][1] = t;

		// normal vector, which is per-vertex for SURF_PHONG

		const vec_t *normal;
		if ((surf->texinfo->flags & SURF_PHONG) && !VectorCompare(vert->normal, vec3_origin)) {
			normal = vert->normal;
		} else {
			normal = surf->normal;
		}

		VectorCopy(normal, mod->bsp->normals[*vertices]);

		// tangent vectors
		vec4_t tangent;
		vec3_t bitangent;

		TangentVectors(normal, sdir, tdir, tangent, bitangent);
		Vector4Copy(tangent, mod->bsp->tangents[*vertices]);

		// find the index that this vertex belongs to.
		surf->elements[i] = R_LoadBspVertexArrays_VertexElement(vertices);
		(*elements)++;
	}
}

typedef struct {
	vec3_t vertex;
	vec3_t normal;
	vec4_t tangent;
	vec2_t diffuse;
	vec2_t lightmap;
} r_bsp_interleave_vertex_t;

static r_buffer_layout_t r_bsp_buffer_layout[] = {
	{ .attribute = R_ATTRIB_POSITION, .type = R_TYPE_FLOAT, .count = 3 },
	{ .attribute = R_ATTRIB_NORMAL, .type = R_TYPE_FLOAT, .count = 3 },
	{ .attribute = R_ATTRIB_TANGENT, .type = R_TYPE_FLOAT, .count = 4 },
	{ .attribute = R_ATTRIB_DIFFUSE, .type = R_TYPE_FLOAT, .count = 2 },
	{ .attribute = R_ATTRIB_LIGHTMAP, .type = R_TYPE_FLOAT, .count = 2 },
	{ .attribute = -1 }
};

/**
 * @brief Function for exporting a BSP to an OBJ.
 */
void R_ExportBSP_f(void) {
	const r_model_t *world = R_WorldModel();

	if (!world) {
		return;
	}
	
	Com_Print("Dumping BSP...\n");
	
	char modelname[MAX_QPATH];
	g_snprintf(modelname, sizeof(modelname), "export/%s.obj", Basename(world->media.name));
	
	char mtlname[MAX_QPATH], mtlpath[MAX_QPATH];
	g_snprintf(mtlname, sizeof(mtlname), "%s.mtl", Basename(world->media.name));
	g_snprintf(mtlpath, sizeof(mtlpath), "export/%s.mtl", Basename(world->media.name));
	
	R_BindBuffer(&world->bsp->vertex_buffer);

	file_t *file = Fs_OpenWrite(mtlpath);

	GHashTable *materials = g_hash_table_new(g_direct_hash, g_direct_equal);
	size_t num_materials = 0;

	// write out unique materials
	for (size_t i = 0; i < world->bsp->num_surfaces; i++) {
		const r_bsp_surface_t *surf = &world->bsp->surfaces[i];
		
		if (!surf->num_edges || !surf->texinfo->material) {
			continue;
		}

		if (g_hash_table_contains(materials, surf->texinfo->material)) {
			continue;
		}

		const r_image_t *diffuse = surf->texinfo->material->diffuse;
		
		Fs_Print(file, "newmtl %s\n", diffuse->media.name);
		Fs_Print(file, "map_Ka %s.png\n", diffuse->media.name);
		Fs_Print(file, "map_Kd %s.png\n\n", diffuse->media.name);

		char path[MAX_OS_PATH];
		g_snprintf(path, sizeof(path), "export/%s.png", diffuse->media.name);

		R_DumpImage(diffuse, path);

		g_hash_table_insert(materials, surf->texinfo->material, (gpointer) num_materials);
		num_materials++;
	}
	
	Fs_Close(file);

	file = Fs_OpenWrite(modelname);
	
	const r_bsp_interleave_vertex_t *vbuff = (const r_bsp_interleave_vertex_t *) glMapBufferRange(GL_ARRAY_BUFFER, 0, world->bsp->vertex_buffer.size, GL_MAP_READ_BIT);
	const size_t num_vertices = world->bsp->vertex_buffer.size / sizeof(r_bsp_interleave_vertex_t);

	Com_Print("Calculating extents...\n");
	vec3_t mins, maxs;
	ClearBounds(mins, maxs);

	const r_bsp_interleave_vertex_t *vertex = vbuff;
	for (size_t i = 0; i < num_vertices; i++, vertex++) {
		AddPointToBounds(vertex->vertex, mins, maxs);
	}

	Com_Print("Writing vertices...\n");

	Fs_Print(file, "# Wavefront OBJ exported by Quetoo\n\n");
	Fs_Print(file, "mtllib %s\n\n", mtlname);

	vertex = vbuff;
	for (size_t i = 0; i < num_vertices; i++, vertex++) {

		Fs_Print(file, "v %f %f %f\nvt %f %f\nvn %f %f %f\n",	-vertex->vertex[0], vertex->vertex[2], vertex->vertex[1],
																vertex->diffuse[0], vertex->diffuse[1],
																-vertex->normal[0], vertex->normal[2], vertex->normal[1]);
	}
	
	Fs_Print(file, "# %" PRIdMAX " vertices\n\n", num_vertices);
	
	Com_Print("Writing elements...\n");

	size_t num_surfs = 0;

	GHashTableIter iter;
	gpointer key;
	g_hash_table_iter_init(&iter, materials);

	while (g_hash_table_iter_next (&iter, &key, NULL)) {
		const r_material_t *material = (const r_material_t *) key;
		
		Fs_Print(file, "g %s\n", material->diffuse->media.name);
		Fs_Print(file, "usemtl %s\n", material->diffuse->media.name);

		for (size_t i = 0; i < world->bsp->num_surfaces; i++) {
			const r_bsp_surface_t *surf = &world->bsp->surfaces[i];

			if (!surf->num_edges || !surf->texinfo->material) {
				continue;
			}

			if (surf->texinfo->material != material) {
				continue;
			}

			num_surfs++;

			const uint32_t *element = surf->elements + surf->num_edges - 1;

			Fs_Print(file, "f ");

			for (size_t i = 0; i < surf->num_edges; i++, element--) {
				const uint32_t e = (*element) + 1;
				Fs_Print(file, "%u/%u/%u ", e, e, e);
			}

			Fs_Print(file, "\n");
		}

		Fs_Print(file, "\n");
	}
	
	Fs_Print(file, "# %" PRIdMAX " surfaces\n\n", num_surfs);
	Fs_Close(file);

	g_hash_table_destroy(materials);

	glUnmapBuffer(GL_ARRAY_BUFFER);

	Com_Print("Done!\n");
}

/**
 * @brief Generates vertex primitives for the world model by iterating leafs.
 */
static void R_LoadBspVertexArrays(r_model_t *mod) {

	mod->num_verts = 0;

	const r_bsp_leaf_t *leaf = mod->bsp->leafs;
	for (uint16_t i = 0; i < mod->bsp->num_leafs; i++, leaf++) {

		r_bsp_surface_t **s = leaf->first_leaf_surface;
		for (uint16_t j = 0; j < leaf->num_leaf_surfaces; j++, s++) {
			mod->num_verts += (*s)->num_edges;
		}
	}

	// start with worst case scenario # of vertices
	GLsizei v = mod->num_verts * sizeof(vec3_t);
	GLsizei st = mod->num_verts * sizeof(vec2_t);
	GLsizei t = mod->num_verts * sizeof(vec4_t);

	mod->bsp->verts = Mem_LinkMalloc(v, mod);
	mod->bsp->texcoords = Mem_LinkMalloc(st, mod);
	mod->bsp->lightmap_texcoords = Mem_LinkMalloc(st, mod);
	mod->bsp->normals = Mem_LinkMalloc(v, mod);
	mod->bsp->tangents = Mem_LinkMalloc(t, mod);

	// make lookup table
	r_unique_vertices.count = 0;
	r_unique_vertices.mod = mod;
	r_unique_vertices.hash_table = g_hash_table_new(R_UniqueVerts_HashFunc, R_UniqueVerts_EqualFunc);

	GLuint num_vertices = 0, num_elements = 0;

	leaf = mod->bsp->leafs;
	for (uint16_t i = 0; i < mod->bsp->num_leafs; i++, leaf++) {

		r_bsp_surface_t **s = leaf->first_leaf_surface;
		for (uint16_t j = 0; j < leaf->num_leaf_surfaces; j++, s++) {

			R_LoadBspVertexArrays_Surface(mod, *s, &num_elements, &num_vertices);
		}
	}

	// hash table no longer required, get rid
	g_hash_table_destroy(r_unique_vertices.hash_table);

	// resize our garbage now that we know exactly how much
	// storage we needed
	mod->num_verts = num_vertices;

	v = mod->num_verts * sizeof(vec3_t);
	st = mod->num_verts * sizeof(vec2_t);
	t = mod->num_verts * sizeof(vec4_t);

	mod->bsp->verts = Mem_Realloc(mod->bsp->verts, v);
	mod->bsp->texcoords = Mem_Realloc(mod->bsp->texcoords, st);
	mod->bsp->lightmap_texcoords = Mem_Realloc(mod->bsp->lightmap_texcoords, st);
	mod->bsp->normals = Mem_Realloc(mod->bsp->normals, v);
	mod->bsp->tangents = Mem_Realloc(mod->bsp->tangents, t);

	// load the element buffer object
	mod->num_elements = num_elements;

	const size_t e = mod->num_elements * sizeof(GLuint);
	GLuint *elements = Mem_LinkMalloc(e, mod);
	GLuint ei = 0;

	leaf = mod->bsp->leafs;
	for (uint16_t i = 0; i < mod->bsp->num_leafs; i++, leaf++) {

		r_bsp_surface_t **s = leaf->first_leaf_surface;
		for (uint16_t j = 0; j < leaf->num_leaf_surfaces; j++, s++) {

			r_bsp_surface_t *surf = (*s);
			for (uint16_t k = 0; k < surf->num_edges; k++, ei++) {
				elements[ei] = surf->elements[k];
			}
		}
	}

	R_CreateElementBuffer(&mod->bsp->element_buffer, &(const r_create_element_t) {
		.type = R_TYPE_UNSIGNED_INT,
		.hint = GL_STATIC_DRAW,
		.size = e,
		.data = elements
	});

	Mem_Free(elements);

	// make interleave buffer
	r_bsp_interleave_vertex_t *interleaved = Mem_LinkMalloc(sizeof(r_bsp_interleave_vertex_t) * mod->num_verts, mod);

	for (GLsizei i = 0; i < mod->num_verts; ++i) {
		VectorCopy(mod->bsp->verts[i], interleaved[i].vertex);
		VectorCopy(mod->bsp->normals[i], interleaved[i].normal);
		Vector4Copy(mod->bsp->tangents[i], interleaved[i].tangent);
		Vector2Copy(mod->bsp->texcoords[i], interleaved[i].diffuse);
		Vector2Copy(mod->bsp->lightmap_texcoords[i], interleaved[i].lightmap);
	}

	R_CreateInterleaveBuffer(&mod->bsp->vertex_buffer, &(const r_create_interleave_t) {
		.struct_size = sizeof(r_bsp_interleave_vertex_t),
		.layout = r_bsp_buffer_layout,
		.hint = GL_STATIC_DRAW,
		.size = mod->num_verts * sizeof(r_bsp_interleave_vertex_t),
		.data = interleaved
	});

	Mem_Free(interleaved);

	R_GetError(mod->media.name);
}

/**
 * @brief Qsort comparator for R_SortBspSurfaceArrays.
 */
static int R_SortBspSurfacesArrays_Compare(const void *s1, const void *s2) {

	const r_bsp_texinfo_t *t1 = (*(r_bsp_surface_t **) s1)->texinfo;
	const r_bsp_texinfo_t *t2 = (*(r_bsp_surface_t **) s2)->texinfo;

	return g_strcmp0(t1->name, t2->name);
}

/**
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

/**
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
			if (surf->texinfo->flags & SURF_WARP) {
				sorted->blend_warp.count++;
			} else {
				sorted->blend.count++;
			}
		} else {
			if (surf->texinfo->flags & SURF_WARP) {
				sorted->opaque_warp.count++;
			} else if (surf->texinfo->flags & SURF_ALPHA_TEST) {
				sorted->alpha_test.count++;
			} else {
				sorted->opaque.count++;
			}
		}

		if (surf->texinfo->material->cm->flags & STAGE_DIFFUSE) {
			sorted->material.count++;
		}

		if (surf->texinfo->material->cm->flags & STAGE_FLARE) {
			sorted->flare.count++;
		}

		if (!(surf->texinfo->flags & SURF_WARP)) {
			sorted->back.count++;
		}
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
			if (surf->texinfo->flags & SURF_WARP) {
				R_SURFACE_TO_SURFACES(&sorted->blend_warp, surf);
			} else {
				R_SURFACE_TO_SURFACES(&sorted->blend, surf);
			}
		} else {
			if (surf->texinfo->flags & SURF_WARP) {
				R_SURFACE_TO_SURFACES(&sorted->opaque_warp, surf);
			} else if (surf->texinfo->flags & SURF_ALPHA_TEST) {
				R_SURFACE_TO_SURFACES(&sorted->alpha_test, surf);
			} else {
				R_SURFACE_TO_SURFACES(&sorted->opaque, surf);
			}
		}

		if (surf->texinfo->material->cm->flags & STAGE_DIFFUSE) {
			R_SURFACE_TO_SURFACES(&sorted->material, surf);
		}

		if (surf->texinfo->material->cm->flags & STAGE_FLARE) {
			R_SURFACE_TO_SURFACES(&sorted->flare, surf);
		}

		if (!(surf->texinfo->flags & SURF_WARP)) {
			R_SURFACE_TO_SURFACES(&sorted->back, surf);
		}
	}

	// now sort them by texture
	R_SortBspSurfacesArrays(mod->bsp);
}

/**
 * @brief Extra lumps we need to load for the R subsystem.
 */
#define R_BSP_LUMPS \
	(1 << BSP_LUMP_VERTEXES) | \
	(1 << BSP_LUMP_EDGES) | \
	(1 << BSP_LUMP_FACE_EDGES) | \
	(1 << BSP_LUMP_LIGHTMAPS) | \
	(1 << BSP_LUMP_FACES) | \
	(1 << BSP_LUMP_LEAF_FACES)

/**
 * @brief Extra lumps we need to load for the R subsystem.
 */
#define R_BSP_LUMPS_ENHANCED \
	(1 << BSP_LUMP_NORMALS)

/**
 * @brief
 */
void R_LoadBspModel(r_model_t *mod, void *buffer) {

	bsp_header_t *file = (bsp_header_t *) buffer;

	mod->bsp = Mem_LinkMalloc(sizeof(r_bsp_model_t), mod);

	mod->bsp->cm = Cm_Bsp();
	mod->bsp->file = &mod->bsp->cm->bsp;

	int32_t version = Bsp_Verify(file);

	// load in lumps that the renderer needs
	Bsp_LoadLumps(file, mod->bsp->file, R_BSP_LUMPS);

	if (version == BSP_VERSION_QUETOO) { // enhanced format
		Bsp_LoadLumps(file, mod->bsp->file, R_BSP_LUMPS_ENHANCED);
	}

	mod->bsp->version = version;

	Cl_LoadingProgress(2, "materials");
	R_LoadModelMaterials(mod);

	Cl_LoadingProgress(4, "vertices");
	R_LoadBspVertexes(mod->bsp);

	if (version == BSP_VERSION_QUETOO) {
		Cl_LoadingProgress(6, "normals");
		R_LoadBspNormals(mod->bsp);
	}

	Cl_LoadingProgress(8, "lightmaps");
	R_LoadBspLightmaps(mod->bsp);

	Cl_LoadingProgress(14, "planes");
	R_LoadBspPlanes(mod->bsp);

	Cl_LoadingProgress(24, "texinfo");
	R_LoadBspTexinfo(mod->bsp);

	Cl_LoadingProgress(28, "faces");
	R_LoadBspSurfaces(mod->bsp);

	Cl_LoadingProgress(32, "leaf faces");
	R_LoadBspLeafSurfaces(mod->bsp);

	Cl_LoadingProgress(36, "leafs");
	R_LoadBspLeafs(mod->bsp);

	Cl_LoadingProgress(40, "nodes");
	R_LoadBspNodes(mod->bsp);

	Cl_LoadingProgress(44, "clusters");
	R_LoadBspClusters(mod->bsp);

	Cl_LoadingProgress(48, "inline models");
	R_LoadBspInlineModels(mod->bsp);

	Cl_LoadingProgress(50, "lights");
	R_LoadBspLights(mod->bsp);

	Cl_LoadingProgress(52, "inline models");
	R_SetupBspInlineModels(mod);

	Cl_LoadingProgress(54, "vertex arrays");
	R_LoadBspVertexArrays(mod);

	Cl_LoadingProgress(58, "sorted surfaces");
	R_LoadBspSurfacesArrays(mod);

	R_InitElements(mod->bsp);

	Com_Debug(DEBUG_RENDERER, "!================================\n");
	Com_Debug(DEBUG_RENDERER, "!R_LoadBspModel: %s\n", mod->media.name);
	Com_Debug(DEBUG_RENDERER, "!  Verts:          %d (%d unique, %d elements)\n", r_unique_vertices.num_vertexes,
	          mod->num_verts,
	          mod->num_elements);
	Com_Debug(DEBUG_RENDERER, "!  Edges:          %d\n", mod->bsp->file->num_edges);
	Com_Debug(DEBUG_RENDERER, "!  Surface edges:  %d\n", mod->bsp->file->num_face_edges);
	Com_Debug(DEBUG_RENDERER, "!  Faces:          %d\n", mod->bsp->num_surfaces);
	Com_Debug(DEBUG_RENDERER, "!  Nodes:          %d\n", mod->bsp->num_nodes);
	Com_Debug(DEBUG_RENDERER, "!  Leafs:          %d\n", mod->bsp->num_leafs);
	Com_Debug(DEBUG_RENDERER, "!  Leaf surfaces:  %d\n", mod->bsp->num_leaf_surfaces);
	Com_Debug(DEBUG_RENDERER, "!  Clusters:       %d\n", mod->bsp->num_clusters);
	Com_Debug(DEBUG_RENDERER, "!  Inline models   %d\n", mod->bsp->num_inline_models);
	Com_Debug(DEBUG_RENDERER, "!  Lights:         %d\n", mod->bsp->num_bsp_lights);
	Com_Debug(DEBUG_RENDERER, "!================================\n");

	// unload r_unique_vertices.vertexes, as they are no longer required
	Mem_Free(r_unique_vertices.vertexes);
	r_unique_vertices.vertexes = NULL;
}
