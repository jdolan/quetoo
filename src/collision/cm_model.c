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
#include "parse.h"

cm_bsp_t cm_bsp;

/**
 * @brief
 */
static void Cm_LoadBspEntities(void) {

	GList *entities = Cm_LoadEntities(cm_bsp.bsp.entity_string);

	cm_bsp.num_entities = g_list_length(entities);
	cm_bsp.entities = Mem_TagMalloc(sizeof(cm_entity_t *) * cm_bsp.num_entities, MEM_TAG_CMODEL);

	cm_entity_t **out = cm_bsp.entities;
	for (const GList *list = entities; list; list = list->next, out++) {
		*out = list->data;
	}

	g_list_free(entities);
}

/**
 * @brief
 */
static void Cm_LoadBspTexinfos(void) {

	const int32_t num_texinfo = cm_bsp.bsp.num_texinfo;
	const bsp_texinfo_t *in = cm_bsp.bsp.texinfo;

	cm_bsp_texinfo_t *out = cm_bsp.texinfos = Mem_TagMalloc(sizeof(cm_bsp_texinfo_t) * num_texinfo, MEM_TAG_CMODEL);

	for (int32_t i = 0; i < num_texinfo; i++, in++, out++) {

		g_strlcpy(out->name, in->texture, sizeof(out->name));
		out->flags = in->flags;
		out->value = in->value;

		for (size_t i = 0; i < cm_bsp.num_materials; i++) {
			cm_material_t *material = cm_bsp.materials[i];
			if (!g_strcmp0(out->name, material->name)) {
				out->material = material;
				break;
			}
		}

		assert(out->material);
	}
}


/**
 * @brief
 */
static void Cm_LoadBspPlanes(void) {

	const int32_t num_planes = cm_bsp.bsp.num_planes;
	const bsp_plane_t *in = cm_bsp.bsp.planes;

	cm_bsp_plane_t *out = cm_bsp.planes = Mem_TagMalloc(sizeof(cm_bsp_plane_t) * (num_planes + 12),
	                                      MEM_TAG_CMODEL); // extra for box hull

	for (int32_t i = 0; i < num_planes; i++, in++, out++) {

		VectorCopy(in->normal, out->normal);
		out->dist = in->dist;
		out->type = Cm_PlaneTypeForNormal(out->normal);
		out->sign_bits = Cm_SignBitsForPlane(out);
		out->num = (i >> 1) + 1;
	}
}

/**
 * @brief
 */
static void Cm_LoadBspNodes(void) {

	const int32_t num_nodes = cm_bsp.bsp.num_nodes;
	const bsp_node_t *in = cm_bsp.bsp.nodes;

	cm_bsp_node_t *out = cm_bsp.nodes = Mem_TagMalloc(sizeof(cm_bsp_node_t) * (num_nodes + 6),
	                                    MEM_TAG_CMODEL); // extra for box hull

	for (int32_t i = 0; i < num_nodes; i++, in++, out++) {

		out->plane = cm_bsp.planes + in->plane_num;

		for (int32_t j = 0; j < 2; j++) {
			out->children[j] = in->children[j];
		}
	}
}


/**
 * @brief
 */
static void Cm_LoadBspLeafs(void) {

	const int32_t num_leafs = cm_bsp.bsp.num_leafs;
	const bsp_leaf_t *in = cm_bsp.bsp.leafs;

	cm_bsp_leaf_t *out = cm_bsp.leafs = Mem_TagMalloc(sizeof(cm_bsp_leaf_t) * (num_leafs + 1),
	                                    MEM_TAG_CMODEL); // extra for box hull

	for (int32_t i = 0; i < num_leafs; i++, in++, out++) {

		out->contents = in->contents;
		out->cluster = in->cluster;
		out->area = in->area;
		out->first_leaf_brush = in->first_leaf_brush;
		out->num_leaf_brushes = in->num_leaf_brushes;
	}
}

/**
 * @brief
 */
static void Cm_LoadBspLeafBrushes(void) {

	const int32_t num_leaf_brushes = cm_bsp.bsp.num_leaf_brushes;
	const int32_t *in = cm_bsp.bsp.leaf_brushes;

	int32_t *out = cm_bsp.leaf_brushes = Mem_TagMalloc(sizeof(int32_t) * (num_leaf_brushes + 1),
	                                      MEM_TAG_CMODEL); // extra for box hull

	for (int32_t i = 0; i < num_leaf_brushes; i++, in++, out++) {

		*out = *in;
	}
}

/**
 * @brief
 */
static void Cm_LoadBspBrushes(void) {

	const int32_t num_brushes = cm_bsp.bsp.num_brushes;
	const bsp_brush_t *in = cm_bsp.bsp.brushes;

	cm_bsp_brush_t *out = cm_bsp.brushes = Mem_TagMalloc(sizeof(cm_bsp_brush_t) * (num_brushes + 1),
	                                       MEM_TAG_CMODEL); // extra for box hull

	for (int32_t i = 0; i < num_brushes; i++, in++, out++) {

		out->first_brush_side = in->first_brush_side;
		out->num_sides = in->num_sides;
		out->contents = in->contents;
	}
}

/**
 * @brief
 */
static void Cm_LoadBspBrushSides(void) {

	static cm_bsp_texinfo_t null_texinfo;

	const int32_t num_brush_sides = cm_bsp.bsp.num_brush_sides;
	const bsp_brush_side_t *in = cm_bsp.bsp.brush_sides;

	cm_bsp_brush_side_t *out = cm_bsp.brush_sides = Mem_TagMalloc(sizeof(cm_bsp_brush_side_t) *
				(num_brush_sides + 6), MEM_TAG_CMODEL); // extra for box hull

	for (int32_t i = 0; i < num_brush_sides; i++, in++, out++) {

		const int32_t p = in->plane_num;

		if (p >= cm_bsp.bsp.num_planes) {
			Com_Error(ERROR_DROP, "Brush side %d has invalid plane %d\n", i, p);
		}

		out->plane = &cm_bsp.planes[p];

		if (in->texinfo == -1) {
			out->surface = &null_texinfo;
		} else {
			if (in->texinfo >= cm_bsp.bsp.num_texinfo) {
				Com_Error(ERROR_DROP, "Brush side %d has invalid texinfo %d\n", i, in->texinfo);
			}

			out->surface = &cm_bsp.texinfos[in->texinfo];
		}
	}
}

/**
 * @brief Sets brush bounds for fast trace tests.
 */
static void Cm_SetupBspBrushes(void) {
	cm_bsp_brush_t *b = cm_bsp.brushes;

	for (int32_t i = 0; i < cm_bsp.bsp.num_brushes; i++, b++) {
		const cm_bsp_brush_side_t *bs = cm_bsp.brush_sides + b->first_brush_side;

		b->mins[0] = -bs[0].plane->dist;
		b->mins[1] = -bs[2].plane->dist;
		b->mins[2] = -bs[4].plane->dist;

		b->maxs[0] = bs[1].plane->dist;
		b->maxs[1] = bs[3].plane->dist;
		b->maxs[2] = bs[5].plane->dist;
	}
}

/**
 * @brief
 */
static void Cm_LoadBspInlineModels(void) {

	const int32_t num_models = cm_bsp.bsp.num_models;
	const bsp_model_t *in = cm_bsp.bsp.models;

	cm_bsp_model_t *out = cm_bsp.models = Mem_TagMalloc(sizeof(cm_bsp_model_t) * num_models, MEM_TAG_CMODEL);

	for (int32_t i = 0; i < num_models; i++, in++, out++) {

		for (int32_t j = 0; j < 3; j++) {
			out->mins[j] = in->mins[j] - 1.0;
			out->maxs[j] = in->maxs[j] + 1.0;
			out->origin[j] = in->origin[j];
		}

		out->head_node = in->head_node;
	}
}

/**
 * @brief
 */
static void Cm_LoadBspAreaPortals(void) {

	const int32_t num_area_portals = cm_bsp.bsp.num_area_portals;

	cm_bsp.area_portals = Mem_TagMalloc(sizeof(bool) * num_area_portals, MEM_TAG_CMODEL);
}

/**
 * @brief
 */
static void Cm_LoadBspAreas(void) {

	const int32_t num_areas = cm_bsp.bsp.num_areas;
	const bsp_area_t *in = cm_bsp.bsp.areas;

	cm_bsp_area_t *out = cm_bsp.areas = Mem_TagMalloc(sizeof(cm_bsp_area_t) * num_areas, MEM_TAG_CMODEL);

	for (int32_t i = 0; i < num_areas; i++, in++, out++) {

		out->num_area_portals = in->num_area_portals;
		out->first_area_portal = in->first_area_portal;
		out->flood_valid = 0;
		out->flood_num = 0;
	}
}

/**
 * @brief
 */
static void Cm_LoadBspMaterials(const char *name) {

	char path[MAX_QPATH];
	StripExtension(Basename(name), path);

	g_snprintf(path, sizeof(path), "materials/%s.mat", path);

	GList *materials = NULL;
	Cm_LoadMaterials(path, &materials);

	const bsp_texinfo_t *in = cm_bsp.bsp.texinfo;
	for (int32_t i = 0; i < cm_bsp.bsp.num_texinfo; i++, in++) {

		cm_material_t *material = NULL;

		for (GList *list = materials; list; list = list->next) {
			if (!g_strcmp0(((cm_material_t *) list->data)->name, in->texture)) {
				material = list->data;
				break;
			}
		}

		if (material == NULL) {
			material = Cm_AllocMaterial(in->texture);
			materials = g_list_prepend(materials, material);
		}
	}

	cm_bsp.num_materials = g_list_length(materials);
	cm_bsp.materials = Mem_TagMalloc(sizeof(cm_material_t *) * cm_bsp.num_materials, MEM_TAG_CMODEL);

	cm_material_t **out = cm_bsp.materials;
	for (const GList *list = materials; list; list = list->next, out++) {
		*out = Mem_Link(list->data, cm_bsp.materials);
	}

	g_list_free(materials);
}

/**
 * @brief Lumps we need to load for the CM subsystem.
 */
#define CM_BSP_LUMPS \
	(1 << BSP_LUMP_ENTITIES) | \
	(1 << BSP_LUMP_TEXINFO) | \
	(1 << BSP_LUMP_PLANES) | \
	(1 << BSP_LUMP_NODES) | \
	(1 << BSP_LUMP_LEAFS) | \
	(1 << BSP_LUMP_LEAF_BRUSHES) | \
	(1 << BSP_LUMP_BRUSHES) | \
	(1 << BSP_LUMP_BRUSH_SIDES) | \
	(1 << BSP_LUMP_MODELS) | \
	(1 << BSP_LUMP_AREA_PORTALS) | \
	(1 << BSP_LUMP_AREAS) | \
	(1 << BSP_LUMP_VISIBILITY)

/**
 * @brief Loads in the BSP and all sub-models for collision detection. This
 * function can also be used to initialize or clean up the collision model by
 * invoking with NULL.
 */
cm_bsp_model_t *Cm_LoadBspModel(const char *name, int64_t *size) {

	// don't re-load if we don't have to
	if (name && !g_strcmp0(name, cm_bsp.name)) {

		if (size) {
			*size = cm_bsp.size;
		}

		return &cm_bsp.models[0];
	}

	Bsp_UnloadLumps(&cm_bsp.bsp, BSP_LUMPS_ALL);

	// free dynamic memory
	Mem_Free(cm_bsp.texinfos);
	Mem_Free(cm_bsp.planes);
	Mem_Free(cm_bsp.nodes);
	Mem_Free(cm_bsp.leafs);
	Mem_Free(cm_bsp.leaf_brushes);
	Mem_Free(cm_bsp.brushes);
	Mem_Free(cm_bsp.brush_sides);
	Mem_Free(cm_bsp.models);
	Mem_Free(cm_bsp.area_portals);
	Mem_Free(cm_bsp.areas);

	Mem_Free(cm_bsp.entities);
	Mem_Free(cm_bsp.materials);

	memset(&cm_bsp, 0, sizeof(cm_bsp));

	// clean up and return
	if (!name) {
		if (size) {
			*size = 0;
		}
		return &cm_bsp.models[0];
	}

	// load the common BSP structure and the lumps we need
	bsp_header_t *file;

	if (Fs_Load(name, (void **) &file) == -1) {
		Com_Error(ERROR_DROP, "Failed to load %s\n", name);
	}

	if (Bsp_Verify(file) == -1) {
		Fs_Free(file);
		Com_Error(ERROR_DROP, "Failed to verify %s\n", name);
	}

	if (!Bsp_LoadLumps(file, &cm_bsp.bsp, CM_BSP_LUMPS)) {
		Fs_Free(file);
		Com_Error(ERROR_DROP, "Lump error loading %s\n", name);
	}

	// in theory, by this point the BSP is valid - now we have to create the cm_
	// structures out of the raw file data
	if (size) {
		cm_bsp.size = *size = Bsp_Size(file);
		cm_bsp.mod_time = Fs_LastModTime(name);
	}

	g_strlcpy(cm_bsp.name, name, sizeof(cm_bsp.name));

	Fs_Free(file);

	Cm_LoadBspMaterials(name);

	Cm_LoadBspEntities();
	Cm_LoadBspTexinfos();
	Cm_LoadBspPlanes();
	Cm_LoadBspNodes();
	Cm_LoadBspLeafs();
	Cm_LoadBspLeafBrushes();
	Cm_LoadBspBrushes();
	Cm_LoadBspBrushSides();
	Cm_LoadBspInlineModels();
	Cm_LoadBspAreaPortals();
	Cm_LoadBspAreas();

	Cm_SetupBspBrushes();

	Cm_InitBoxHull();

	Cm_FloodAreas();

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

	if (num < 1 || num >= cm_bsp.bsp.num_models) {
		Com_Error(ERROR_DROP, "Bad number: %d\n", num);
	}

	return &cm_bsp.models[num];
}

/**
 * @brief
 */
int32_t Cm_NumClusters(void) {

	if (cm_bsp.bsp.vis_data) {
		return cm_bsp.bsp.vis_data->num_clusters;
	}
	
	return 0;
}

/**
 * @brief
 */
int32_t Cm_NumModels(void) {
	return cm_bsp.bsp.num_models;
}

/**
 * @brief
 */
const char *Cm_EntityString(void) {
	return cm_bsp.bsp.entity_string;
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

	if (leaf_num < 0 || leaf_num >= cm_bsp.bsp.num_leafs) {
		Com_Error(ERROR_DROP, "Bad number: %d\n", leaf_num);
	}

	return cm_bsp.leafs[leaf_num].contents;
}

/**
 * @brief
 */
int32_t Cm_LeafCluster(const int32_t leaf_num) {

	if (leaf_num < 0 || leaf_num >= cm_bsp.bsp.num_leafs) {
		Com_Error(ERROR_DROP, "Bad number: %d\n", leaf_num);
	}

	return cm_bsp.leafs[leaf_num].cluster;
}

/**
 * @brief
 */
int32_t Cm_LeafArea(const int32_t leaf_num) {

	if (leaf_num < 0 || leaf_num >= cm_bsp.bsp.num_leafs) {
		Com_Error(ERROR_DROP, "Bad number: %d\n", leaf_num);
	}

	return cm_bsp.leafs[leaf_num].area;
}

/**
 * @brief
 */
cm_bsp_t *Cm_Bsp(void) {
	return &cm_bsp;
}
