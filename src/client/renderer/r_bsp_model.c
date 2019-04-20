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
#include "r_gl.h"
#include "client.h"

/**
 * @brief
 */
static void R_LoadBspEntities(r_bsp_model_t *bsp) {
	const char *c;

	bsp->luxel_size = DEFAULT_BSP_LIGHTMAP_LUXEL_SIZE;

	if ((c = Cm_EntityValue(Cm_Worldspawn(), "luxel_size"))) {
		bsp->luxel_size = strtol(c, NULL, 10);
		if (bsp->luxel_size <= 0) {
			bsp->luxel_size = DEFAULT_BSP_LIGHTMAP_LUXEL_SIZE;
		}
		Com_Debug(DEBUG_RENDERER, "Resolved luxel_size: %d\n", bsp->luxel_size);
	}
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

	for (int32_t i = 0; i < bsp->num_texinfo; i++, in++, out++) {

		Vector4Copy(in->vecs[0], out->vecs[0]);
		Vector4Copy(in->vecs[1], out->vecs[1]);

		out->flags = in->flags;
		out->value = in->value;

		g_strlcpy(out->texture, in->texture, sizeof(out->texture));

		out->material = R_LoadMaterial(out->texture, ASSET_CONTEXT_TEXTURES);
	}
}

/**
 * @brief
 */
static void R_LoadBspPlanes(r_bsp_model_t *bsp) {
	bsp->plane_shadows = Mem_LinkMalloc(bsp->file->num_planes * sizeof(int32_t), bsp);
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

	for (int32_t i = 0; i < bsp->num_nodes; i++, in++, out++) {

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
 * @brief Loads all r_bsp_leaf_t for the specified BSP model.
 */
static void R_LoadBspLeafs(r_bsp_model_t *bsp) {
	r_bsp_leaf_t *out;

	const bsp_leaf_t *in = bsp->file->leafs;

	bsp->num_leafs = bsp->file->num_leafs;
	bsp->leafs = out = Mem_LinkMalloc(bsp->num_leafs * sizeof(*out), bsp);

	for (int32_t i = 0; i < bsp->num_leafs; i++, in++, out++) {

		VectorCopy(in->mins, out->mins);
		VectorCopy(in->maxs, out->maxs);

		out->contents = in->contents;

		out->cluster = in->cluster;
		out->area = in->area;

		const int32_t f = in->first_leaf_face;
		out->first_leaf_surface = bsp->leaf_surfaces + f;
		out->num_leaf_surfaces = in->num_leaf_faces;
	}
}

/**
 * @brief
 */
static void R_LoadBspLeafSurfaces(r_bsp_model_t *bsp) {
	r_bsp_surface_t **out;

	const int32_t *in = bsp->file->leaf_faces;

	bsp->num_leaf_surfaces = bsp->file->num_leaf_faces;
	bsp->leaf_surfaces = out = Mem_LinkMalloc(bsp->num_leaf_surfaces * sizeof(*out), bsp);

	for (int32_t i = 0; i < bsp->num_leaf_surfaces; i++) {

		const int32_t j = in[i];

		if (j >= bsp->num_surfaces) {
			Com_Error(ERROR_DROP, "Bad surface number: %d\n", j);
		}

		out[i] = bsp->surfaces + j;
	}
}


/**
 * @brief
 */
static void R_LoadBspVertexes(r_bsp_model_t *bsp) {

	bsp->num_vertexes = bsp->file->num_vertexes;
	r_bsp_vertex_t *out = bsp->vertexes = Mem_LinkMalloc(bsp->num_vertexes * sizeof(*out), bsp);

	const bsp_vertex_t *in = bsp->file->vertexes;
	for (int32_t i = 0; i < bsp->num_vertexes; i++, in++, out++) {

		VectorCopy(in->position, out->position);
		VectorCopy(in->normal, out->normal);
		VectorCopy(in->tangent, out->tangent);
		VectorCopy(in->bitangent, out->bitangent);

		Vector2Copy(in->diffuse, out->diffuse);
		Vector2Copy(in->lightmap, out->lightmap);
	}
}

/**
 * @brief
 */
static void R_LoadBspElements(r_bsp_model_t *bsp) {

	bsp->num_elements = bsp->file->num_elements;
	int32_t *out = bsp->elements = Mem_LinkMalloc(bsp->num_elements * sizeof(*out), bsp);

	const int32_t *in = bsp->file->elements;
	for (int32_t i = 0; i < bsp->num_elements; i++, in++, out++) {
		*out = *in;
	}
}

/**
 * @brief
 */
static void R_LoadBspLights(r_bsp_model_t *bsp) {

	bsp->num_lights = bsp->file->num_lights;
	r_bsp_light_t *out = bsp->lights = Mem_LinkMalloc(sizeof(r_bsp_light_t) * bsp->num_lights, bsp);

	const bsp_light_t *in = bsp->file->lights;
	for (int32_t i = 0; i < bsp->num_lights; i++, in++, out++) {

		out->type = in->type;
		out->atten = in->atten;

		VectorCopy(in->origin, out->origin);
		VectorCopy(in->color, out->color);
		VectorCopy(in->normal, out->normal);

		out->radius = in->radius;
		out->theta = in->theta;

		out->leaf = R_LeafForPoint(out->origin, bsp);

		out->debug.type = PARTICLE_CORONA;
		out->debug.blend = GL_ONE;
		out->debug.color[3] = 1.0;
		VectorCopy(out->origin, out->debug.org);
		VectorCopy(out->color, out->debug.color);
		out->debug.scale = out->radius * r_draw_bsp_lights->value;
	}
}

/**
 * @brief
 */
static void R_LoadBspLightmaps(r_bsp_model_t *bsp) {

	bsp->num_lightmaps = bsp->file->num_lightmaps;
	r_bsp_lightmap_t *out = bsp->lightmaps = Mem_LinkMalloc(sizeof(r_bsp_lightmap_t) * bsp->num_lightmaps, bsp);

	const bsp_lightmap_t *in = bsp->file->lightmaps;
	for (int32_t i = 0; i < bsp->num_lightmaps; i++, in++, out++) {
		char name[MAX_QPATH];

		g_snprintf(name, sizeof(name), "lightmap %d", i);
		out->lightmaps = (r_image_t *) R_AllocMedia(name, sizeof(r_image_t), MEDIA_IMAGE);
		out->lightmaps->media.Free = R_FreeImage;
		out->lightmaps->type = IT_LIGHTMAP;
		out->lightmaps->width = BSP_LIGHTMAP_WIDTH;
		out->lightmaps->height = BSP_LIGHTMAP_WIDTH;
		out->lightmaps->layers = BSP_LIGHTMAP_LAYERS;

		R_UploadImage(out->lightmaps, GL_RGB8, (byte *) in->layers);

		g_snprintf(name, sizeof(name), "stainmap %d", i);
		out->stainmaps = (r_image_t *) R_AllocMedia(name, sizeof(r_image_t), MEDIA_IMAGE);
		out->stainmaps->media.Free = R_FreeImage;
		out->stainmaps->type = IT_STAINMAP;
		out->stainmaps->width = BSP_LIGHTMAP_WIDTH;
		out->stainmaps->height = BSP_LIGHTMAP_WIDTH;

		g_strlcat(name, " framebuffer", sizeof(name));
		out->framebuffer = R_CreateFramebuffer(name);

		R_UploadImage(out->stainmaps, GL_RGBA8, NULL);

		R_AttachFramebufferImage(out->framebuffer, out->stainmaps);

		R_BindFramebuffer(NULL);

		Matrix4x4_FromOrtho(&out->projection, 0.0,
							out->stainmaps->width,
							out->stainmaps->height,
							0.0, -1.0, 1.0);
	}

	if (bsp->num_lightmaps == 0) {
		bsp->lightmaps = Mem_LinkMalloc(sizeof(bsp_lightmap_t), bsp);
		bsp->lightmaps->lightmaps = r_image_state.null;
		bsp->lightmaps->stainmaps = r_image_state.null;
	}
}

/**
 * @brief Loads all r_bsp_surface_t for the specified BSP model. Lightmap and
 * deluxemap creation is driven by this function.
 */
static void R_LoadBspSurfaces(r_bsp_model_t *bsp) {

	static r_bsp_texinfo_t null_texinfo;

	const bsp_face_t *in = bsp->file->faces;
	r_bsp_surface_t *out;

	bsp->num_surfaces = bsp->file->num_faces;
	bsp->surfaces = out = Mem_LinkMalloc(bsp->num_surfaces * sizeof(*out), bsp);

	for (int32_t i = 0; i < bsp->num_surfaces; i++, in++, out++) {

		// resolve plane
		out->plane = bsp->cm->planes + in->plane_num;
		if (in->plane_num & 1) {
			out->flags |= R_SURF_BACK_SIDE;
		}

		// then texinfo
		if (in->texinfo == -1) {
			out->texinfo = &null_texinfo;
		} else {
			if (in->texinfo >= bsp->num_texinfo) {
				Com_Error(ERROR_DROP, "Bad texinfo number: %d\n", in->texinfo);
			}
			out->texinfo = bsp->texinfo + in->texinfo;
		}

		out->first_vertex = in->first_vertex;
		out->num_vertexes = in->num_vertexes;

		out->first_element = in->first_element;
		out->num_elements = in->num_elements;

		if (in->lightmap.num > -1) {
			out->lightmap.atlas = bsp->lightmaps + in->lightmap.num;

			out->lightmap.s = in->lightmap.s;
			out->lightmap.t = in->lightmap.t;
			out->lightmap.w = in->lightmap.w;
			out->lightmap.h = in->lightmap.h;
		} else {
			out->lightmap.atlas = bsp->lightmaps;
		}
	}
}

/**
 * @brief Sets up the bsp_surface_t for rendering.
 */
static void R_SetupBspSurface(r_bsp_model_t *bsp, r_bsp_leaf_t *leaf, r_bsp_surface_t *surf) {

	ClearBounds(surf->mins, surf->maxs);

	ClearStBounds(surf->st_mins, surf->st_maxs);
	ClearStBounds(surf->lightmap.st_mins, surf->lightmap.st_maxs);

	const r_bsp_vertex_t *v = bsp->vertexes + surf->first_vertex;
	for (int32_t i = 0; i < surf->num_vertexes; i++, v++) {
		AddPointToBounds(v->position, surf->mins, surf->maxs);
		AddStToBounds(v->diffuse, surf->st_mins, surf->st_maxs);
		AddStToBounds(v->lightmap, surf->lightmap.st_mins, surf->lightmap.st_maxs);
	}

	if (leaf->contents & MASK_LIQUID) {
		if (!(surf->texinfo->flags & (SURF_SKY | SURF_BLEND_33 | SURF_BLEND_66 | SURF_WARP))) {
			surf->flags |= R_SURF_IN_LIQUID;
		}
	}

	R_CreateBspSurfaceFlare(bsp, surf);
}

/**
 * @brief Iterates r_bsp_surface_t by their respective leafs, preparing them for rendering.
 */
static void R_SetupBspSurfaces(r_bsp_model_t *bsp) {

	r_bsp_leaf_t *leaf = bsp->leafs;
	for (int32_t i = 0; i < bsp->num_leafs; i++, leaf++) {

		r_bsp_surface_t **s = leaf->first_leaf_surface;
		for (int32_t j = 0; j < leaf->num_leaf_surfaces; j++, s++) {

			R_SetupBspSurface(bsp, leaf, *s);
		}
	}
}

/**
 * @brief Loads all r_bsp_cluster_t for the specified BSP model. Note that
 * no information is actually loaded at this point. Rather, space for the
 * clusters is allocated, to be utilized by the PVS algorithm.
 */
static void R_LoadBspClusters(r_bsp_model_t *bsp) {

	if (!bsp->file->vis_size) {
		return;
	}

	bsp->num_clusters = bsp->file->vis->num_clusters;
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

	for (int32_t i = 0; i < mod->bsp->num_inline_models; i++) {
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

	for (int32_t i = 0; i < bsp->num_inline_models; i++, in++, out++) {

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

static r_buffer_layout_t r_bsp_buffer_layout[] = {
	{ .attribute = R_ATTRIB_POSITION, .type = R_TYPE_FLOAT, .count = 3 },
	{ .attribute = R_ATTRIB_NORMAL, .type = R_TYPE_FLOAT, .count = 3 },
	{ .attribute = R_ATTRIB_TANGENT, .type = R_TYPE_FLOAT, .count = 3 },
	{ .attribute = R_ATTRIB_BITANGENT, .type = R_TYPE_FLOAT, .count = 3 },
	{ .attribute = R_ATTRIB_DIFFUSE, .type = R_TYPE_FLOAT, .count = 2 },
	{ .attribute = R_ATTRIB_LIGHTMAP, .type = R_TYPE_FLOAT, .count = 2 },
	{ .attribute = -1 }
};

/**
 * @brief Function for exporting a BSP to an OBJ.
 */
void R_ExportBsp_f(void) {
	const r_model_t *world = R_WorldModel();

	if (!world) {
		return;
	}
	
	Com_Print("Exporting BSP...\n");
	
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
	for (int32_t i = 0; i < world->bsp->num_surfaces; i++) {
		const r_bsp_surface_t *surf = &world->bsp->surfaces[i];
		
		if (!surf->texinfo->material) {
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
	
	Com_Print("Calculating extents...\n");

	vec3_t mins, maxs;
	ClearBounds(mins, maxs);

	const r_bsp_vertex_t *v = world->bsp->vertexes;
	for (int32_t i = 0; i < world->bsp->num_vertexes; i++, v++) {
		AddPointToBounds(v->position, mins, maxs);
	}

	Com_Print("Writing vertexes...\n");

	Fs_Print(file, "# Wavefront OBJ exported by Quetoo\n\n");
	Fs_Print(file, "mtllib %s\n\n", mtlname);

	v = world->bsp->vertexes;
	for (int32_t i = 0; i <  world->bsp->num_vertexes; i++, v++) {

		Fs_Print(file, "v %f %f %f\nvt %f %f\nvn %f %f %f\n",
				-v->position[0], v->position[2], v->position[1],
				 v->diffuse[0], -v->diffuse[1],
				-v->normal[0], v->normal[2], v->normal[1]);
	}
	
	Fs_Print(file, "# %d vertexes\n\n", world->bsp->num_vertexes);
	
	Com_Print("Writing faces...\n");

	GHashTableIter iter;
	gpointer key;
	g_hash_table_iter_init(&iter, materials);

	while (g_hash_table_iter_next (&iter, &key, NULL)) {
		const r_material_t *material = (const r_material_t *) key;
		
		Fs_Print(file, "g %s\n", material->diffuse->media.name);
		Fs_Print(file, "usemtl %s\n", material->diffuse->media.name);

		for (int32_t i = 0; i < world->bsp->num_surfaces; i++) {
			const r_bsp_surface_t *surf = &world->bsp->surfaces[i];

			if (!surf->texinfo->material) {
				continue;
			}

			if (surf->texinfo->material != material) {
				continue;
			}

			Fs_Print(file, "f ");

			for (int32_t v = surf->first_vertex; v < surf->num_vertexes; v++) {
				Fs_Print(file, "%d/%d/%d", v, v, v);
			}

			Fs_Print(file, "\n");
		}

		Fs_Print(file, "\n");
	}

	Fs_Print(file, "# %d faces\n\n", world->bsp->num_surfaces);
	Fs_Close(file);

	g_hash_table_destroy(materials);

	glUnmapBuffer(GL_ARRAY_BUFFER);

	Com_Print("Done!\n");
}

/**
 * @brief Creates the static vertex buffer for the given model.
 */
static void R_LoadBspVertexArrays(r_model_t *mod) {

	mod->num_verts = mod->bsp->num_vertexes;

	R_CreateInterleaveBuffer(&mod->bsp->vertex_buffer, &(const r_create_interleave_t) {
		.struct_size = sizeof(r_bsp_vertex_t),
		.layout = r_bsp_buffer_layout,
		.hint = GL_STATIC_DRAW,
		.size = mod->num_verts * sizeof(r_bsp_vertex_t),
		.data = mod->bsp->vertexes
	});

	mod->num_elements = mod->bsp->num_elements;

	R_CreateElementBuffer(&mod->bsp->element_buffer, &(const r_create_element_t) {
		.type = R_TYPE_UNSIGNED_INT,
		.hint = GL_STATIC_DRAW,
		.size = mod->num_elements * sizeof(int32_t),
		.data = mod->bsp->elements
	});

	R_GetError(mod->media.name);
}

/**
 * @brief Qsort comparator for R_SortBspSurfaceArrays.
 */
static int R_SortBspSurfacesArrays_Compare(const void *s1, const void *s2) {

	const r_bsp_texinfo_t *t1 = (*(r_bsp_surface_t **) s1)->texinfo;
	const r_bsp_texinfo_t *t2 = (*(r_bsp_surface_t **) s2)->texinfo;
	
	return t1->material < t2->material ? -1 : t1->material > t2->material ? 1 : 0;
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
	for (int32_t i = 0; i < mod->bsp->num_surfaces; i++, surf++) {

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
	for (int32_t i = 0; i < mod->bsp->num_surfaces; i++, surf++) {

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
	}

	// now sort them by texture
	R_SortBspSurfacesArrays(mod->bsp);
}

/**
 * @brief Extra lumps we need to load for the R subsystem.
 */
#define R_BSP_LUMPS \
	(1 << BSP_LUMP_LEAF_FACES) | \
	(1 << BSP_LUMP_VERTEXES) | \
	(1 << BSP_LUMP_ELEMENTS) | \
	(1 << BSP_LUMP_FACES) | \
	(1 << BSP_LUMP_LIGHTS) | \
	(1 << BSP_LUMP_LIGHTMAPS)

/**
 * @brief
 */
void R_LoadBspModel(r_model_t *mod, void *buffer) {

	bsp_header_t *file = (bsp_header_t *) buffer;

	mod->bsp = Mem_LinkMalloc(sizeof(r_bsp_model_t), mod);

	mod->bsp->cm = Cm_Bsp();
	mod->bsp->file = &mod->bsp->cm->bsp;

	// load in lumps that the renderer needs
	Bsp_LoadLumps(file, mod->bsp->file, R_BSP_LUMPS);

	Cl_LoadingProgress(2, "materials");
	R_LoadModelMaterials(mod);

	Cl_LoadingProgress(6, "entities");
	R_LoadBspEntities(mod->bsp);

	Cl_LoadingProgress(8, "texinfo");
	R_LoadBspTexinfo(mod->bsp);

	Cl_LoadingProgress(12, "planes");
	R_LoadBspPlanes(mod->bsp);

	Cl_LoadingProgress(16, "vertexes");
	R_LoadBspVertexes(mod->bsp);

	Cl_LoadingProgress(18, "elements");
	R_LoadBspElements(mod->bsp);

	Cl_LoadingProgress(20, "lightmaps");
	R_LoadBspLightmaps(mod->bsp);

	Cl_LoadingProgress(24, "faces");
	R_LoadBspSurfaces(mod->bsp);

	Cl_LoadingProgress(30, "leaf faces");
	R_LoadBspLeafSurfaces(mod->bsp);

	Cl_LoadingProgress(32, "leafs");
	R_LoadBspLeafs(mod->bsp);

	Cl_LoadingProgress(36, "nodes");
	R_LoadBspNodes(mod->bsp);

	Cl_LoadingProgress(40, "face extents");
	R_SetupBspSurfaces(mod->bsp);

	Cl_LoadingProgress(46, "inline models");
	R_LoadBspInlineModels(mod->bsp);

	Cl_LoadingProgress(48, "clusters");
	R_LoadBspClusters(mod->bsp);

	Cl_LoadingProgress(50, "lights");
	R_LoadBspLights(mod->bsp);

	Cl_LoadingProgress(52, "inline models");
	R_SetupBspInlineModels(mod);

	Cl_LoadingProgress(54, "vertex arrays");
	R_LoadBspVertexArrays(mod);

	Cl_LoadingProgress(58, "sorted surfaces");
	R_LoadBspSurfacesArrays(mod);

	Bsp_UnloadLumps(mod->bsp->file, R_BSP_LUMPS);

	R_InitElements(mod->bsp);

	Com_Debug(DEBUG_RENDERER, "!================================\n");
	Com_Debug(DEBUG_RENDERER, "!R_LoadBspModel: %s\n", mod->media.name);
	Com_Debug(DEBUG_RENDERER, "!  Verts:          %d\n", mod->num_verts);
	Com_Debug(DEBUG_RENDERER, "!  Elements:       %d\n", mod->num_elements);
	Com_Debug(DEBUG_RENDERER, "!  Faces:          %d\n", mod->bsp->num_surfaces);
	Com_Debug(DEBUG_RENDERER, "!  Nodes:          %d\n", mod->bsp->num_nodes);
	Com_Debug(DEBUG_RENDERER, "!  Leafs:          %d\n", mod->bsp->num_leafs);
	Com_Debug(DEBUG_RENDERER, "!  Leaf surfaces:  %d\n", mod->bsp->num_leaf_surfaces);
	Com_Debug(DEBUG_RENDERER, "!  Clusters:       %d\n", mod->bsp->num_clusters);
	Com_Debug(DEBUG_RENDERER, "!  Inline models   %d\n", mod->bsp->num_inline_models);
	Com_Debug(DEBUG_RENDERER, "!  Lights:         %d\n", mod->bsp->num_lights);
	Com_Debug(DEBUG_RENDERER, "!================================\n");
}
