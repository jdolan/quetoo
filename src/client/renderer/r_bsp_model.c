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
 * @brief
 */
static void R_LoadBspEntities(r_bsp_model_t *bsp) {
	const char *c;

	bsp->luxel_size = BSP_LIGHTMAP_LUXEL_SIZE;

	if ((c = Cm_EntityValue(Cm_Worldspawn(), "luxel_size"))) {
		bsp->luxel_size = atoi(c);
		if (bsp->luxel_size <= 0) {
			bsp->luxel_size = BSP_LIGHTMAP_LUXEL_SIZE;
		}
		Com_Debug(DEBUG_RENDERER, "Resolved luxel_size: %d\n", bsp->luxel_size);
	}
}

/**
 * @brief Loads all r_bsp_texinfo_t for the specified BSP model. Texinfo's
 * are shared by one or more r_bsp_face_t.
 */
static void R_LoadBspTexinfo(r_bsp_model_t *bsp) {
	r_bsp_texinfo_t *out;

	const bsp_texinfo_t *in = bsp->cm->file.texinfo;

	bsp->num_texinfo = bsp->cm->file.num_texinfo;
	bsp->texinfo = out = Mem_LinkMalloc(bsp->num_texinfo * sizeof(*out), bsp);

	for (int32_t i = 0; i < bsp->num_texinfo; i++, in++, out++) {

		out->vecs[0] = in->vecs[0];
		out->vecs[1] = in->vecs[1];

		out->flags = in->flags;
		out->value = in->value;

		g_strlcpy(out->texture, in->texture, sizeof(out->texture));

		out->material = R_LoadMaterial(out->texture, ASSET_CONTEXT_TEXTURES);
	}
}

/**
 * @brief
 */
static void R_LoadBspVertexes(r_bsp_model_t *bsp) {

	bsp->num_vertexes = bsp->cm->file.num_vertexes;
	r_bsp_vertex_t *out = bsp->vertexes = Mem_LinkMalloc(bsp->num_vertexes * sizeof(*out), bsp);

	const bsp_vertex_t *in = bsp->cm->file.vertexes;
	for (int32_t i = 0; i < bsp->num_vertexes; i++, in++, out++) {

		out->position = in->position;
		out->normal = in->normal;
		out->tangent = in->tangent;
		out->bitangent = in->bitangent;

		out->diffusemap = in->diffusemap;
		out->lightmap = in->lightmap;

		float alpha = 1.0;

		const r_bsp_texinfo_t *texinfo = bsp->texinfo + in->texinfo;
		switch (texinfo->flags & (SURF_BLEND_33 | SURF_BLEND_66)) {
			case SURF_BLEND_33:
				alpha = 0.333;
				break;
			case SURF_BLEND_66:
				alpha = 0.666;
			default:
				break;
		}
		out->color = Color_Color32(Color4f(1.0, 1.0, 1.0, alpha));
	}
}

/**
 * @brief
 */
static void R_LoadBspElements(r_bsp_model_t *bsp) {

	bsp->num_elements = bsp->cm->file.num_elements;
	GLuint *out = bsp->elements = Mem_LinkMalloc(bsp->num_elements * sizeof(*out), bsp);

	const int32_t *in = bsp->cm->file.elements;
	for (int32_t i = 0; i < bsp->num_elements; i++, in++, out++) {
		*out = *in;
	}
}

static r_bsp_texinfo_t null_texinfo;

/**
 * @brief Loads all r_bsp_face_t for the specified BSP model.
 */
static void R_LoadBspFaces(r_bsp_model_t *bsp) {

	const bsp_face_t *in = bsp->cm->file.faces;
	r_bsp_face_t *out;

	bsp->num_faces = bsp->cm->file.num_faces;
	bsp->faces = out = Mem_LinkMalloc(bsp->num_faces * sizeof(*out), bsp);

	for (int32_t i = 0; i < bsp->num_faces; i++, in++, out++) {

		// resolve plane
		out->plane = bsp->cm->planes + in->plane_num;
		out->plane_side = in->plane_num & 1;

		// then texinfo
		if (in->texinfo > -1) {
			if (in->texinfo >= bsp->num_texinfo) {
				Com_Error(ERROR_DROP, "Bad texinfo number: %d\n", in->texinfo);
			}
			out->texinfo = bsp->texinfo + in->texinfo;
		} else {
			out->texinfo = &null_texinfo;
		}

		out->vertexes = bsp->vertexes + in->first_vertex;
		out->num_vertexes = in->num_vertexes;

		out->elements = (GLvoid *) (in->first_element * sizeof(GLuint));
		out->num_elements = in->num_elements;

		if (out->texinfo->flags & SURF_NO_LIGHTMAP) {
			continue;
		}

		r_bsp_face_lightmap_t *lm = &out->lightmap;

		out->lightmap.s = in->lightmap.s;
		out->lightmap.t = in->lightmap.t;
		out->lightmap.w = in->lightmap.w;
		out->lightmap.h = in->lightmap.h;

		out->lightmap.matrix = in->lightmap.matrix;

		out->lightmap.st_mins = in->lightmap.st_mins;
		out->lightmap.st_maxs = in->lightmap.st_maxs;

		lm->stainmap = Mem_LinkMalloc(lm->w * lm->h * BSP_LIGHTMAP_BPP, bsp->faces);
	}
}

/**
 * @brief
 */
static int32_t R_DrawElementsCmp(const void *a, const void *b) {

	r_bsp_draw_elements_t *a_draw = *(r_bsp_draw_elements_t **) a;
	r_bsp_draw_elements_t *b_draw = *(r_bsp_draw_elements_t **) b;

	int32_t order = strcmp(a_draw->texinfo->texture, b_draw->texinfo->texture);
	if (order == 0) {
		const int32_t a_flags = (a_draw->texinfo->flags & SURF_TEXINFO_CMP);
		const int32_t b_flags = (b_draw->texinfo->flags & SURF_TEXINFO_CMP);

		order = a_flags - b_flags;
	}

	return order;
}

/**
 * @brief
 */
static void R_LoadBspDrawElements(r_bsp_model_t *bsp) {
	r_bsp_draw_elements_t *out;

	bsp->num_draw_elements = bsp->cm->file.num_draw_elements;
	bsp->draw_elements = out = Mem_LinkMalloc(bsp->num_draw_elements * sizeof(*out), bsp);
	bsp->draw_elements_sorted = Mem_LinkMalloc(bsp->num_draw_elements * sizeof (out), bsp);

	const bsp_draw_elements_t *in = bsp->cm->file.draw_elements;
	for (int32_t i = 0; i < bsp->num_draw_elements; i++, in++, out++) {

		if (in->texinfo > -1) {
			if (in->texinfo >= bsp->num_texinfo) {
				Com_Error(ERROR_DROP, "Bad texinfo number: %d\n", in->texinfo);
			}
			out->texinfo = bsp->texinfo + in->texinfo;
		} else {
			out->texinfo = &null_texinfo;
		}

		out->num_faces = in->num_faces;
		out->faces = bsp->faces + in->first_face;

		out->num_elements = in->num_elements;
		out->elements = (GLvoid *) (in->first_element * sizeof(GLuint));

		bsp->draw_elements_sorted[i] = out;
	}

	qsort(bsp->draw_elements_sorted, bsp->num_draw_elements, sizeof(out), R_DrawElementsCmp);
}

/**
 * @brief
 */
static void R_LoadBspLeafFaces(r_bsp_model_t *bsp) {
	r_bsp_face_t **out;

	const int32_t *in = bsp->cm->file.leaf_faces;

	bsp->num_leaf_faces = bsp->cm->file.num_leaf_faces;
	bsp->leaf_faces = out = Mem_LinkMalloc(bsp->num_leaf_faces * sizeof(*out), bsp);

	for (int32_t i = 0; i < bsp->num_leaf_faces; i++) {

		const int32_t j = in[i];

		if (j < 0 || j >= bsp->num_faces) {
			Com_Error(ERROR_DROP, "Bad face number: %d\n", j);
		}

		out[i] = bsp->faces + j;
	}
}

/**
 * @brief Loads all r_bsp_node_t for the specified BSP model.
 */
static void R_LoadBspNodes(r_bsp_model_t *bsp) {
	r_bsp_node_t *out;

	const bsp_node_t *in = bsp->cm->file.nodes;

	bsp->num_nodes = bsp->cm->file.num_nodes;
	bsp->nodes = out = Mem_LinkMalloc(bsp->num_nodes * sizeof(*out), bsp);

	for (int32_t i = 0; i < bsp->num_nodes; i++, in++, out++) {

		out->mins = Vec3s_CastVec3(in->mins);
		out->maxs = Vec3s_CastVec3(in->maxs);

		const int32_t p = in->plane_num;
		out->plane = bsp->cm->planes + p;

		out->faces = bsp->faces + in->first_face;
		out->num_faces = in->num_faces;

		out->draw_elements = bsp->draw_elements + in->first_draw_elements;
		out->num_draw_elements = in->num_draw_elements;

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
}

/**
 * @brief Loads all r_bsp_leaf_t for the specified BSP model.
 */
static void R_LoadBspLeafs(r_bsp_model_t *bsp) {
	r_bsp_leaf_t *out;

	const bsp_leaf_t *in = bsp->cm->file.leafs;

	bsp->num_leafs = bsp->cm->file.num_leafs;
	bsp->leafs = out = Mem_LinkMalloc(bsp->num_leafs * sizeof(*out), bsp);

	for (int32_t i = 0; i < bsp->num_leafs; i++, in++, out++) {

		out->mins = Vec3s_CastVec3(in->mins);
		out->maxs = Vec3s_CastVec3(in->maxs);

		out->contents = in->contents;

		out->cluster = in->cluster;
		out->area = in->area;

		out->leaf_faces = bsp->leaf_faces + in->first_leaf_face;
		out->num_leaf_faces = in->num_leaf_faces;
	}
}

/**
 * @brief
 */
static void R_SetupBspNode(r_bsp_node_t *node, r_bsp_node_t *parent, r_bsp_inline_model_t *model) {

	node->parent = parent;
	node->model = model;

	if (node->contents != CONTENTS_NODE) {
		return;
	}

	r_bsp_face_t *face = node->faces;
	for (int32_t i = 0; i < node->num_faces; i++, face++) {
		face->node = node;
	}

	r_bsp_draw_elements_t *draw = node->draw_elements;
	for (int32_t i = 0; i < node->num_draw_elements; i++, draw++) {
		draw->node = node;
	}

	R_SetupBspNode(node->children[0], node, model);
	R_SetupBspNode(node->children[1], node, model);
}

/**
 * @brief
 */
static void R_LoadBspInlineModels(r_bsp_model_t *bsp) {
	r_bsp_inline_model_t *out;

	const bsp_model_t *in = bsp->cm->file.models;

	bsp->num_inline_models = bsp->cm->file.num_models;
	bsp->inline_models = out = Mem_LinkMalloc(bsp->num_inline_models * sizeof(*out), bsp);

	for (int32_t i = 0; i < bsp->num_inline_models; i++, in++, out++) {

		out->head_node = bsp->nodes + in->head_node;

		out->mins = Vec3s_CastVec3(in->mins);
		out->maxs = Vec3s_CastVec3(in->maxs);

		out->faces = bsp->faces + in->first_face;
		out->num_faces = in->num_faces;

		out->draw_elements = bsp->draw_elements + in->first_draw_elements;
		out->num_draw_elements = in->num_draw_elements;

		R_SetupBspNode(out->head_node, NULL, out);
	}
}

/**
 * @brief Creates an r_model_t for each inline model so that entities may reference them.
 */
static void R_SetupBspInlineModels(r_model_t *mod) {

	r_bsp_inline_model_t *in = mod->bsp->inline_models;
	for (int32_t i = 0; i < mod->bsp->num_inline_models; i++, in++) {

		r_model_t *out = Mem_TagMalloc(sizeof(r_model_t), MEM_TAG_RENDERER);

		g_snprintf(out->media.name, sizeof(mod->media.name), "%s#%d", mod->media.name, i);
		out->type = MOD_BSP_INLINE;
		out->bsp_inline = in;

		out->maxs = in->maxs;
		out->mins = in->mins;

		mod->mins = Vec3_Minf(mod->mins, out->mins);
		mod->maxs = Vec3_Maxf(mod->maxs, out->maxs);

		R_RegisterDependency(&mod->media, &out->media);
	}
}

/**
 * @brief Loads the lightmap layers to a 2D array texture, appending a layer for the stainmap.
 */
static void R_LoadBspLightmap(r_model_t *mod) {
	r_bsp_lightmap_t *out;

	const bsp_lightmap_t *in = mod->bsp->cm->file.lightmap;
	if (in == NULL) {
		return;
	}

	mod->bsp->lightmap = out = Mem_LinkMalloc(sizeof(*out), mod->bsp);

	out->width = in->width;

	out->atlas = (r_image_t *) R_AllocMedia("lightmap", sizeof(r_image_t), MEDIA_IMAGE);
	out->atlas->media.Free = R_FreeImage;
	out->atlas->type = IT_LIGHTMAP;
	out->atlas->width = out->width;
	out->atlas->height = out->width;
	out->atlas->depth = BSP_LIGHTMAP_LAYERS + BSP_STAINMAP_LAYERS;

	const size_t in_size = in->width * in->width * BSP_LIGHTMAP_LAYERS * BSP_LIGHTMAP_BPP;
	const size_t out_size = out->atlas->width * out->atlas->height * out->atlas->depth * BSP_LIGHTMAP_BPP;

	byte *data = Mem_Malloc(out_size);
	memcpy(data, (byte *) in + sizeof(bsp_lightmap_t), in_size);

	R_UploadImage(out->atlas, GL_RGB, data);

	Mem_Free(data);
}

/**
 * @brief
 */
static void R_LoadBspLightgrid(r_model_t *mod) {
	r_bsp_lightgrid_t *out;

	const bsp_lightgrid_t *in = mod->bsp->cm->file.lightgrid;
	if (in == NULL) {
		return;
	}

	mod->bsp->lightgrid = out = Mem_LinkMalloc(sizeof(*out), mod->bsp);

	out->size = in->size;

	const vec3_t grid_size = Vec3_Scale(Vec3i_CastVec3(out->size), BSP_LIGHTGRID_LUXEL_SIZE);
	const vec3_t world_size = Vec3_Subtract(mod->maxs, mod->mins);
	const vec3_t padding = Vec3_Scale(Vec3_Subtract(grid_size, world_size), 0.5);

	out->mins = Vec3_Subtract(mod->mins, padding);
	out->maxs = Vec3_Add(mod->maxs, padding);

	const size_t texture_size = out->size.x * out->size.y * out->size.z * BSP_LIGHTGRID_BPP;

	byte *data = (byte *) in + sizeof(bsp_lightgrid_t);

	for (int32_t i = 0; i < BSP_LIGHTGRID_TEXTURES; i++, data += texture_size) {

		out->textures[i] = (r_image_t *) R_AllocMedia(va("lightgrid[%d]", i), sizeof(r_image_t), MEDIA_IMAGE);
		out->textures[i]->media.Free = R_FreeImage;
		out->textures[i]->type = IT_LIGHTGRID;
		out->textures[i]->width = out->size.x;
		out->textures[i]->height = out->size.y;
		out->textures[i]->depth = out->size.z;

		R_UploadImage(out->textures[i], GL_RGB, data);
	}
}

/**
 * @brief
 */
static void R_LoadBspVertexArray(r_model_t *mod) {

	glGenVertexArrays(1, &mod->bsp->vertex_array);
	glBindVertexArray(mod->bsp->vertex_array);

	glGenBuffers(1, &mod->bsp->vertex_buffer);
	glBindBuffer(GL_ARRAY_BUFFER, mod->bsp->vertex_buffer);
	glBufferData(GL_ARRAY_BUFFER, mod->bsp->num_vertexes * sizeof(r_bsp_vertex_t), mod->bsp->vertexes, GL_STATIC_DRAW);

	glGenBuffers(1, &mod->bsp->elements_buffer);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mod->bsp->elements_buffer);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, mod->bsp->num_elements * sizeof(GLuint), mod->bsp->elements, GL_STATIC_DRAW);

	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(r_bsp_vertex_t), (void *) offsetof(r_bsp_vertex_t, position));
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(r_bsp_vertex_t), (void *) offsetof(r_bsp_vertex_t, normal));
	glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(r_bsp_vertex_t), (void *) offsetof(r_bsp_vertex_t, tangent));
	glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, sizeof(r_bsp_vertex_t), (void *) offsetof(r_bsp_vertex_t, bitangent));
	glVertexAttribPointer(4, 2, GL_FLOAT, GL_FALSE, sizeof(r_bsp_vertex_t), (void *) offsetof(r_bsp_vertex_t, diffusemap));
	glVertexAttribPointer(5, 2, GL_FLOAT, GL_FALSE, sizeof(r_bsp_vertex_t), (void *) offsetof(r_bsp_vertex_t, lightmap));
	glVertexAttribPointer(6, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(r_bsp_vertex_t), (void *) offsetof(r_bsp_vertex_t, color));

	R_GetError(mod->media.name);

	glBindVertexArray(0);
}

/**
 * @brief Extra lumps we need to load for the renderer.
 */
#define R_BSP_LUMPS ( \
	(1 << BSP_LUMP_VERTEXES) | \
	(1 << BSP_LUMP_ELEMENTS) | \
	(1 << BSP_LUMP_FACES) | \
	(1 << BSP_LUMP_DRAW_ELEMENTS) | \
	(1 << BSP_LUMP_LEAF_FACES) | \
	(1 << BSP_LUMP_LIGHTMAP) | \
	(1 << BSP_LUMP_LIGHTGRID) \
)

/**
 * @brief
 */
void R_LoadBspModel(r_model_t *mod, void *buffer) {

	bsp_header_t *file = (bsp_header_t *) buffer;

	mod->bsp = Mem_LinkMalloc(sizeof(r_bsp_model_t), mod);
	mod->bsp->cm = Cm_Bsp();

	// load in lumps that the renderer needs
	Bsp_LoadLumps(file, &mod->bsp->cm->file, R_BSP_LUMPS);

	Cl_LoadingProgress(2, "materials");
	R_LoadModelMaterials(mod);

	Cl_LoadingProgress(6, "entities");
	R_LoadBspEntities(mod->bsp);

	Cl_LoadingProgress(10, "texinfo");
	R_LoadBspTexinfo(mod->bsp);

	Cl_LoadingProgress(14, "vertexes");
	R_LoadBspVertexes(mod->bsp);

	Cl_LoadingProgress(18, "elements");
	R_LoadBspElements(mod->bsp);

	Cl_LoadingProgress(22, "faces");
	R_LoadBspFaces(mod->bsp);

	Cl_LoadingProgress(26, "draw elements");
	R_LoadBspDrawElements(mod->bsp);

	Cl_LoadingProgress(30, "leaf faces");
	R_LoadBspLeafFaces(mod->bsp);

	Cl_LoadingProgress(34, "leafs");
	R_LoadBspLeafs(mod->bsp);

	Cl_LoadingProgress(38, "nodes");
	R_LoadBspNodes(mod->bsp);

	Cl_LoadingProgress(46, "inline models");
	R_LoadBspInlineModels(mod->bsp);

	Cl_LoadingProgress(50, "inline models");
	R_SetupBspInlineModels(mod);

	Cl_LoadingProgress(54, "arrays");
	R_LoadBspVertexArray(mod);

	Cl_LoadingProgress(58, "lightmap");
	R_LoadBspLightmap(mod);

	Cl_LoadingProgress(62, "lightgrid");
	R_LoadBspLightgrid(mod);

	if (r_draw_bsp_lightgrid->value) {
		Bsp_UnloadLumps(&mod->bsp->cm->file, R_BSP_LUMPS & ~(1 << BSP_LUMP_LIGHTGRID));
	} else {
		Bsp_UnloadLumps(&mod->bsp->cm->file, R_BSP_LUMPS);
	}

	Com_Debug(DEBUG_RENDERER, "!================================\n");
	Com_Debug(DEBUG_RENDERER, "!R_LoadBspModel:   %s\n", mod->media.name);
	Com_Debug(DEBUG_RENDERER, "!  Vertexes:       %d\n", mod->bsp->num_vertexes);
	Com_Debug(DEBUG_RENDERER, "!  Elements:       %d\n", mod->bsp->num_elements);
	Com_Debug(DEBUG_RENDERER, "!  Faces:          %d\n", mod->bsp->num_faces);
	Com_Debug(DEBUG_RENDERER, "!  Draw elements:  %d\n", mod->bsp->num_draw_elements);
	Com_Debug(DEBUG_RENDERER, "!  Leafs:          %d\n", mod->bsp->num_leafs);
	Com_Debug(DEBUG_RENDERER, "!  Leaf faces:     %d\n", mod->bsp->num_leaf_faces);
	Com_Debug(DEBUG_RENDERER, "!================================\n");
}

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

	file_t *file = Fs_OpenWrite(mtlpath);

	GHashTable *materials = g_hash_table_new(g_direct_hash, g_direct_equal);
	size_t num_materials = 0;

	// write out unique materials
	for (int32_t i = 0; i < world->bsp->num_faces; i++) {
		const r_bsp_face_t *surf = &world->bsp->faces[i];

		if (!surf->texinfo->material) {
			continue;
		}

		if (g_hash_table_contains(materials, surf->texinfo->material)) {
			continue;
		}

		const r_image_t *diffuse = surf->texinfo->material->diffusemap;

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

	Com_Print("Writing vertexes...\n");

	Fs_Print(file, "# Wavefront OBJ exported by Quetoo\n\n");
	Fs_Print(file, "mtllib %s\n\n", mtlname);

	const r_bsp_vertex_t *v = world->bsp->vertexes;
	for (int32_t i = 0; i <  world->bsp->num_vertexes; i++, v++) {

		Fs_Print(file, "v %f %f %f\nvt %f %f\nvn %f %f %f\n",
				-v->position.x, v->position.z, v->position.y,
				 v->diffusemap.x, -v->diffusemap.y,
				-v->normal.x, v->normal.z, v->normal.y);
	}

	Fs_Print(file, "# %d vertexes\n\n", world->bsp->num_vertexes);

	Com_Print("Writing faces...\n");

	GHashTableIter iter;
	gpointer key;
	g_hash_table_iter_init(&iter, materials);

	while (g_hash_table_iter_next (&iter, &key, NULL)) {
		const r_material_t *material = (const r_material_t *) key;

		Fs_Print(file, "g %s\n", material->diffusemap->media.name);
		Fs_Print(file, "usemtl %s\n", material->diffusemap->media.name);

		for (int32_t i = 0; i < world->bsp->num_faces; i++) {
			const r_bsp_face_t *face = &world->bsp->faces[i];

			if (!face->texinfo->material) {
				continue;
			}

			if (face->texinfo->material != material) {
				continue;
			}

			Fs_Print(file, "f ");

			const r_bsp_vertex_t *vertex = face->vertexes;
			for (int32_t j = 0; j < face->num_vertexes; j++, vertex++) {
				const int32_t v = (int32_t) (ptrdiff_t) (vertex - world->bsp->vertexes);
				Fs_Print(file, "%d/%d/%d", v, v, v);
			}

			Fs_Print(file, "\n");
		}

		Fs_Print(file, "\n");
	}

	Fs_Print(file, "# %d faces\n\n", world->bsp->num_faces);
	Fs_Close(file);

	g_hash_table_destroy(materials);

	glUnmapBuffer(GL_ARRAY_BUFFER);

	Com_Print("Done!\n");
}
