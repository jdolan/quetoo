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

#include "cm_local.h"

cm_bsp_t cm_bsp = {};

/**
 * @brief
 */
static void Cm_LoadBspEntities(cm_bsp_t *bsp) {

	GList *entities = Cm_LoadEntities(bsp->file->entity_string);

	bsp->num_entities = g_list_length(entities);
	bsp->entities = Mem_TagMalloc(sizeof(cm_entity_t *) * bsp->num_entities, MEM_TAG_COLLISION);

	cm_entity_t **out = bsp->entities;
	for (const GList *list = entities; list; list = list->next, out++) {
		*out = list->data;
	}

	g_list_free(entities);
}

/**
 * @brief
 */
static void Cm_LoadBspPlanes(cm_bsp_t *bsp) {

	bsp->num_planes = bsp->file->num_planes;
	const bsp_plane_t *in = bsp->file->planes;

	cm_bsp_plane_t *out = bsp->planes = Mem_TagMalloc(sizeof(cm_bsp_plane_t) * (bsp->num_planes + 12), MEM_TAG_COLLISION); // extra for box hull

	for (int32_t i = 0; i < bsp->num_planes; i++, in++, out++) {
		*out = Cm_Plane(in->normal, in->dist);
	}
}

/**
 * @brief
 */
static void Cm_LoadBspNodes(cm_bsp_t *bsp) {

	bsp->num_nodes = bsp->file->num_nodes;
	const bsp_node_t *in = bsp->file->nodes;

	cm_bsp_node_t *out = bsp->nodes = Mem_TagMalloc(sizeof(cm_bsp_node_t) * (bsp->num_nodes + 6), MEM_TAG_COLLISION); // extra for box hull

	for (int32_t i = 0; i < bsp->num_nodes; i++, in++, out++) {

		out->plane = bsp->planes + in->plane;

		for (int32_t j = 0; j < 2; j++) {
			out->children[j] = in->children[j];
		}
	}
}


/**
 * @brief
 */
static void Cm_LoadBspLeafs(cm_bsp_t *bsp) {

	bsp->num_leafs = bsp->file->num_leafs;
	const bsp_leaf_t *in = bsp->file->leafs;

	cm_bsp_leaf_t *out = bsp->leafs = Mem_TagMalloc(sizeof(cm_bsp_leaf_t) * (bsp->num_leafs + 1), MEM_TAG_COLLISION); // extra for box hull

	for (int32_t i = 0; i < bsp->num_leafs; i++, in++, out++) {

		out->contents = in->contents;
		out->cluster = in->cluster;
		out->first_leaf_brush = in->first_leaf_brush;
		out->num_leaf_brushes = in->num_leaf_brushes;
	}
}

/**
 * @brief
 */
static void Cm_LoadBspLeafBrushes(cm_bsp_t *bsp) {

	bsp->num_leaf_brushes = bsp->file->num_leaf_brushes;
	const int32_t *in = bsp->file->leaf_brushes;

	int32_t *out = bsp->leaf_brushes = Mem_TagMalloc(sizeof(int32_t) * (bsp->num_leaf_brushes + 1), MEM_TAG_COLLISION); // extra for box hull

	for (int32_t i = 0; i < bsp->num_leaf_brushes; i++, in++, out++) {
		*out = *in;
	}
}

/**
 * @brief
 */
static void Cm_LoadBspBrushSides(cm_bsp_t *bsp) {

	bsp->num_brush_sides = bsp->file->num_brush_sides;
	const bsp_brush_side_t *in = bsp->file->brush_sides;

	cm_bsp_brush_side_t *out = bsp->brush_sides = Mem_TagMalloc(sizeof(cm_bsp_brush_side_t) *
				(bsp->num_brush_sides + 6), MEM_TAG_COLLISION); // extra for box hull

	for (int32_t i = 0; i < bsp->num_brush_sides; i++, in++, out++) {

		const int32_t p = in->plane;

		if (p >= bsp->num_planes) {
			Com_Error(ERROR_DROP, "Brush side %d has invalid plane %d\n", i, p);
		}

		out->plane = &bsp->planes[p];

		if (in->material > -1) {
			if (in->material >= bsp->num_materials) {
				Com_Error(ERROR_DROP, "Brush side %d has invalid material %d\n", i, in->material);
			}

			out->material = bsp->materials[in->material];
		}

		out->contents = in->contents;
		out->surface = in->surface;
		out->value = in->value;
	}
}

/**
 * @brief
 */
static void Cm_LoadBspBrushes(cm_bsp_t *bsp) {

	bsp->num_brushes = bsp->file->num_brushes;
	const bsp_brush_t *in = bsp->file->brushes;

	cm_bsp_brush_t *out = bsp->brushes = Mem_TagMalloc(sizeof(cm_bsp_brush_t) * (bsp->num_brushes + 1), MEM_TAG_COLLISION); // extra for box hull

	for (int32_t i = 0; i < bsp->num_brushes; i++, in++, out++) {

		out->entity = bsp->entities[in->entity];
		out->contents = in->contents;
		out->brush_sides = bsp->brush_sides + in->first_brush_side;
		out->num_brush_sides = in->num_brush_sides;
		out->bounds = in->bounds;
	}
}

/**
 * @brief
 */
static void Cm_LoadBspInlineModels(cm_bsp_t *bsp) {

	bsp->num_models = bsp->file->num_models;
	const bsp_model_t *in = bsp->file->models;

	cm_bsp_model_t *out = bsp->models = Mem_TagMalloc(sizeof(cm_bsp_model_t) * bsp->num_models, MEM_TAG_COLLISION);

	for (int32_t i = 0; i < bsp->num_models; i++, in++, out++) {
		
		out->head_node = in->head_node;
		out->bounds = in->bounds;
	}
}

/**
 * @brief
 */
static void Cm_LoadBspMaterials(cm_bsp_t *bsp) {

	char path[MAX_QPATH];
	StripExtension(bsp->name, path);
	g_snprintf(path, sizeof(path), "%s.mat", path);

	GList *materials = NULL;
	Cm_LoadMaterials(path, &materials);

	bsp->num_materials = bsp->file->num_materials;

	cm_material_t **out = bsp->materials = Mem_TagMalloc(sizeof(cm_material_t *) * bsp->num_materials, MEM_TAG_COLLISION);

	const bsp_material_t *in = bsp->file->materials;
	for (int32_t i = 0; i < bsp->num_materials; i++, in++, out++) {

		for (GList *list = materials; list; list = list->next) {
			if (!g_strcmp0(((cm_material_t *) list->data)->name, in->name)) {
				*out = list->data;
				break;
			}
		}

		if (*out == NULL) {
			*out = Cm_AllocMaterial(in->name);
		}

		*out = Mem_Link(*out, bsp->materials);
	}

	g_list_free(materials);
}

/**
 * @brief Lumps we need to load for the CM subsystem.
 */
#define CM_BSP_LUMPS \
	(1 << BSP_LUMP_ENTITIES) | \
	(1 << BSP_LUMP_MATERIALS) | \
	(1 << BSP_LUMP_PLANES) | \
	(1 << BSP_LUMP_NODES) | \
	(1 << BSP_LUMP_LEAFS) | \
	(1 << BSP_LUMP_LEAF_BRUSHES) | \
	(1 << BSP_LUMP_BRUSHES) | \
	(1 << BSP_LUMP_BRUSH_SIDES) | \
	(1 << BSP_LUMP_MODELS)

/**
 * @brief Loads in the BSP and all sub-models for collision detection. This
 * function can also be used to initialize or clean up the collision model by
 * invoking with NULL.
 */
cm_bsp_model_t *Cm_LoadBspModel(const char *name, int64_t *size) {
	static bsp_file_t file;

	// don't re-load if we don't have to
	if (name && !g_strcmp0(name, cm_bsp.name)) {

		if (size) {
			*size = cm_bsp.size;
		}

		return &cm_bsp.models[0];
	}

	Bsp_UnloadLumps(&file, BSP_LUMPS_ALL);

	// free dynamic memory
	Mem_Free(cm_bsp.planes);
	Mem_Free(cm_bsp.nodes);
	Mem_Free(cm_bsp.leafs);
	Mem_Free(cm_bsp.leaf_brushes);
	Mem_Free(cm_bsp.brushes);
	Mem_Free(cm_bsp.brush_sides);
	Mem_Free(cm_bsp.models);
	Mem_Free(cm_bsp.entities);
	Mem_Free(cm_bsp.materials);

	memset(&cm_bsp, 0, sizeof(cm_bsp));
	cm_bsp.file = &file;

	// clean up and return
	if (!name) {
		if (size) {
			*size = 0;
		}
		return &cm_bsp.models[0];
	}

	// load the common BSP structure and the lumps we need
	bsp_header_t *header;

	if (Fs_Load(name, (void **) &header) == -1) {
		Com_Error(ERROR_DROP, "Failed to load %s\n", name);
	}

	if (Bsp_Verify(header) == -1) {
		Fs_Free(header);
		Com_Error(ERROR_DROP, "Failed to verify %s\n", name);
	}

	if (!Bsp_LoadLumps(header, &file, CM_BSP_LUMPS)) {
		Fs_Free(header);
		Com_Error(ERROR_DROP, "Lump error loading %s\n", name);
	}

	// in theory, by this point the BSP is valid - now we have to create the cm_
	// structures out of the raw file data
	if (size) {
		cm_bsp.size = *size = Bsp_Size(header);
		cm_bsp.mod_time = Fs_LastModTime(name);
	}

	g_strlcpy(cm_bsp.name, name, sizeof(cm_bsp.name));

	Fs_Free(header);

	Cm_LoadBspMaterials(&cm_bsp);
	Cm_LoadBspEntities(&cm_bsp);
	Cm_LoadBspPlanes(&cm_bsp);
	Cm_LoadBspNodes(&cm_bsp);
	Cm_LoadBspLeafs(&cm_bsp);
	Cm_LoadBspLeafBrushes(&cm_bsp);
	Cm_LoadBspBrushSides(&cm_bsp);
	Cm_LoadBspBrushes(&cm_bsp);
	Cm_LoadBspInlineModels(&cm_bsp);

	Cm_InitBoxHull(&cm_bsp);

	return &cm_bsp.models[0];
}

/**
 * @brief
 */
cm_bsp_model_t *Cm_Model(const char *name) {

	if (!name || name[0] != '*') {
		Com_Error(ERROR_DROP, "Bad name\n");
	}

	const int32_t num = atoi(name + 1);

	if (num < 1 || num >= cm_bsp.num_models) {
		Com_Error(ERROR_DROP, "Bad number: %d\n", num);
	}

	return &cm_bsp.models[num];
}

/**
 * @brief
 */
int32_t Cm_NumModels(void) {
	return cm_bsp.file->num_models;
}

/**
 * @brief
 */
const char *Cm_EntityString(void) {
	return cm_bsp.file->entity_string;
}

/**
 * @brief
 */
cm_entity_t *Cm_Worldspawn(void) {
	return *cm_bsp.entities;
}

/**
 * @brief
 */
int32_t Cm_LeafContents(const int32_t leaf_num) {

	if (leaf_num < 0 || leaf_num >= cm_bsp.num_leafs) {
		Com_Error(ERROR_DROP, "Bad number: %d\n", leaf_num);
	}

	return cm_bsp.leafs[leaf_num].contents;
}

/**
 * @brief
 */
int32_t Cm_LeafCluster(const int32_t leaf_num) {

	if (leaf_num < 1 || leaf_num >= cm_bsp.num_leafs) {
		Com_Error(ERROR_DROP, "Bad number: %d\n", leaf_num);
	}

	return cm_bsp.leafs[leaf_num].cluster;
}

/**
 * @brief
 */
const cm_bsp_t *Cm_Bsp(void) {
	return &cm_bsp;
}
