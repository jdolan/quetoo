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

#include "cm_local.h"

cm_bsp_t cm_bsp = { { '\0' } };
cm_vis_t *cm_vis = (cm_vis_t *) &cm_bsp.visibility;

/*
 * @brief
 */
static void Cm_LoadBspInlineModels(const d_bsp_lump_t *l) {
	const d_bsp_model_t *in;
	int32_t i, j, count;

	in = (const void *) (cm_bsp.base + l->file_ofs);
	if (l->file_len % sizeof(*in)) {
		Com_Error(ERR_DROP, "Funny lump size\n");
	}
	count = l->file_len / sizeof(*in);

	if (count < 1) {
		Com_Error(ERR_DROP, "Map with no models\n");
	}
	if (count > MAX_BSP_MODELS) {
		Com_Error(ERR_DROP, "Map has too many models\n");
	}

	cm_bsp.num_models = count;

	for (i = 0; i < count; i++, in++) {
		cm_bsp_model_t *out = &cm_bsp.models[i];

		for (j = 0; j < 3; j++) { // spread the mins / maxs by a pixel
			out->mins[j] = LittleFloat(in->mins[j]) - 1;
			out->maxs[j] = LittleFloat(in->maxs[j]) + 1;
			out->origin[j] = LittleFloat(in->origin[j]);
		}
		out->head_node = LittleLong(in->head_node);
	}
}

/*
 * @brief
 */
static void Cm_LoadBspSurfaces(const d_bsp_lump_t *l) {
	const d_bsp_texinfo_t *in;
	cm_bsp_surface_t *out;
	int32_t i, count;

	in = (const void *) (cm_bsp.base + l->file_ofs);
	if (l->file_len % sizeof(*in)) {
		Com_Error(ERR_DROP, "Funny lump size\n");
		return;
	}
	count = l->file_len / sizeof(*in);
	if (count < 1) {
		Com_Error(ERR_DROP, "Map with no surfaces\n");
	}
	if (count > MAX_BSP_TEXINFO) {
		Com_Error(ERR_DROP, "Map has too many surfaces\n");
	}
	cm_bsp.num_surfaces = count;
	out = cm_bsp.surfaces;

	for (i = 0; i < count; i++, in++, out++) {
		g_strlcpy(out->name, in->texture, sizeof(out->name));
		out->flags = LittleLong(in->flags);
		out->value = LittleLong(in->value);
	}
}

/*
 * @brief
 */
static void Cm_LoadBspNodes(const d_bsp_lump_t *l) {
	const d_bsp_node_t *in;
	int32_t child;
	cm_bsp_node_t *out;
	int32_t i, j, count;

	in = (const void *) (cm_bsp.base + l->file_ofs);
	if (l->file_len % sizeof(*in)) {
		Com_Error(ERR_DROP, "Funny lump size\n");
	}
	count = l->file_len / sizeof(*in);

	if (count < 1) {
		Com_Error(ERR_DROP, "Map has no nodes\n");
	}
	if (count > MAX_BSP_NODES) {
		Com_Error(ERR_DROP, "Map has too many nodes\n");
	}

	out = cm_bsp.nodes;

	cm_bsp.num_nodes = count;

	for (i = 0; i < count; i++, out++, in++) {
		out->plane = cm_bsp.planes + LittleLong(in->plane_num);
		for (j = 0; j < 2; j++) {
			child = LittleLong(in->children[j]);
			out->children[j] = child;
		}
	}
}

/*
 * @brief
 */
static void Cm_LoadBspBrushes(const d_bsp_lump_t *l) {
	const d_bsp_brush_t *in;
	cm_bsp_brush_t *out;
	int32_t i, count;

	in = (const void *) (cm_bsp.base + l->file_ofs);
	if (l->file_len % sizeof(*in)) {
		Com_Error(ERR_DROP, "Funny lump size\n");
	}
	count = l->file_len / sizeof(*in);

	if (count > MAX_BSP_BRUSHES) {
		Com_Error(ERR_DROP, "Map has too many brushes\n");
	}

	out = cm_bsp.brushes;

	cm_bsp.num_brushes = count;

	for (i = 0; i < count; i++, out++, in++) {
		out->first_brush_side = LittleLong(in->first_side);
		out->num_sides = LittleLong(in->num_sides);
		out->contents = LittleLong(in->contents);
	}
}

/*
 * @brief
 */
static void Cm_LoadBspLeafs(const d_bsp_lump_t *l) {
	int32_t i;
	cm_bsp_leaf_t *out;
	const d_bsp_leaf_t *in;
	int32_t count;

	in = (const void *) (cm_bsp.base + l->file_ofs);
	if (l->file_len % sizeof(*in)) {
		Com_Error(ERR_DROP, "Funny lump size\n");
	}
	count = l->file_len / sizeof(*in);

	if (count < 1) {
		Com_Error(ERR_DROP, "Map with no leafs\n");
	}
	// need to save space for box planes
	if (count > MAX_BSP_PLANES) {
		Com_Error(ERR_DROP, "Map has too many planes\n");
	}

	out = cm_bsp.leafs;
	cm_bsp.num_leafs = count;

	for (i = 0; i < count; i++, in++, out++) {
		out->contents = LittleLong(in->contents);
		out->cluster = LittleShort(in->cluster);
		out->area = LittleShort(in->area);
		out->first_leaf_brush = LittleShort(in->first_leaf_brush);
		out->num_leaf_brushes = LittleShort(in->num_leaf_brushes);
	}

	if (cm_bsp.leafs[0].contents != CONTENTS_SOLID) {
		Com_Error(ERR_DROP, "Map leaf 0 is not CONTENTS_SOLID\n");
	}
	cm_bsp.solid_leaf = 0;
	cm_bsp.empty_leaf = -1;
	for (i = 1; i < cm_bsp.num_leafs; i++) {
		if (!cm_bsp.leafs[i].contents) {
			cm_bsp.empty_leaf = i;
			break;
		}
	}
	if (cm_bsp.empty_leaf == -1)
		Com_Error(ERR_DROP, "Map does not have an empty leaf\n");
}

/*
 * @brief
 */
static void Cm_LoadBspPlanes(const d_bsp_lump_t *l) {
	int32_t i, j;
	cm_bsp_plane_t *out;
	const d_bsp_plane_t *in;
	int32_t count;

	in = (const void *) (cm_bsp.base + l->file_ofs);
	if (l->file_len % sizeof(*in)) {
		Com_Error(ERR_DROP, "Funny lump size\n");
	}
	count = l->file_len / sizeof(*in);

	if (count < 1) {
		Com_Error(ERR_DROP, "Map with no planes\n");
	}
	// need to save space for box planes
	if (count > MAX_BSP_PLANES) {
		Com_Error(ERR_DROP, "Map has too many planes\n");
	}

	out = cm_bsp.planes;
	cm_bsp.num_planes = count;

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
 * @brief
 */
static void Cm_LoadBspLeafBrushes(const d_bsp_lump_t *l) {
	int32_t i;
	uint16_t *out;
	const uint16_t *in;
	int32_t count;

	in = (const void *) (cm_bsp.base + l->file_ofs);
	if (l->file_len % sizeof(*in)) {
		Com_Error(ERR_DROP, "Funny lump size\n");
	}
	count = l->file_len / sizeof(*in);

	if (count < 1) {
		Com_Error(ERR_DROP, "Map with no planes\n");
	}
	// need to save space for box planes
	if (count > MAX_BSP_LEAF_BRUSHES) {
		Com_Error(ERR_DROP, "Map has too many leaf brushes\n");
	}

	out = cm_bsp.leaf_brushes;
	cm_bsp.num_leaf_brushes = count;

	for (i = 0; i < count; i++, in++, out++)
		*out = LittleShort(*in);
}

/*
 * @brief
 */
static void Cm_LoadBspBrushSides(const d_bsp_lump_t *l) {
	int32_t i;
	cm_bsp_brush_side_t *out;
	const d_bsp_brush_side_t *in;
	int32_t count;
	int32_t num;

	in = (const void *) (cm_bsp.base + l->file_ofs);
	if (l->file_len % sizeof(*in)) {
		Com_Error(ERR_DROP, "Funny lump size\n");
	}
	count = l->file_len / sizeof(*in);

	// need to save space for box planes
	if (count > MAX_BSP_BRUSH_SIDES) {
		Com_Error(ERR_DROP, "Map has too many planes\n");
	}

	out = cm_bsp.brush_sides;
	cm_bsp.num_brush_sides = count;

	for (i = 0; i < count; i++, in++, out++) {
		num = LittleShort(in->plane_num);
		out->plane = &cm_bsp.planes[num];
		num = LittleShort(in->surf_num);
		if (num >= cm_bsp.num_surfaces) {
			Com_Error(ERR_DROP, "Bad brush side surface index\n");
		}
		out->surface = &cm_bsp.surfaces[num];
	}
}

/*
 * @brief
 */
static void Cm_LoadBspAreas(const d_bsp_lump_t *l) {
	int32_t i;
	cm_bsp_area_t *out;
	const d_bsp_area_t *in;
	int32_t count;

	in = (const void *) (cm_bsp.base + l->file_ofs);
	if (l->file_len % sizeof(*in)) {
		Com_Error(ERR_DROP, "Funny lump size\n");
	}
	count = l->file_len / sizeof(*in);

	if (count > MAX_BSP_AREAS) {
		Com_Error(ERR_DROP, "Map has too many areas\n");
	}

	out = cm_bsp.areas;
	cm_bsp.num_areas = count;

	for (i = 0; i < count; i++, in++, out++) {
		out->num_area_portals = LittleLong(in->num_area_portals);
		out->first_area_portal = LittleLong(in->first_area_portal);
		out->flood_valid = 0;
		out->flood_num = 0;
	}
}

/*
 * @brief
 */
static void Cm_LoadBspAreaPortals(const d_bsp_lump_t *l) {
	int32_t i;
	d_bsp_area_portal_t *out;
	const d_bsp_area_portal_t *in;
	int32_t count;

	in = (const void *) (cm_bsp.base + l->file_ofs);
	if (l->file_len % sizeof(*in)) {
		Com_Error(ERR_DROP, "Funny lump size\n");
	}
	count = l->file_len / sizeof(*in);

	if (count > MAX_BSP_AREAS) {
		Com_Error(ERR_DROP, "Map has too many areas\n");
	}

	out = cm_bsp.area_portals;
	cm_bsp.num_area_portals = count;

	for (i = 0; i < count; i++, in++, out++) {
		out->portal_num = LittleLong(in->portal_num);
		out->other_area = LittleLong(in->other_area);
	}
}

/*
 * @brief
 */
static void Cm_LoadBspVisibility(const d_bsp_lump_t *l) {
	int32_t i;

	cm_bsp.num_visibility = l->file_len;
	if (l->file_len > MAX_BSP_VISIBILITY) {
		Com_Error(ERR_DROP, "Map has too large visibility lump\n");
	}

	memcpy(cm_bsp.visibility, cm_bsp.base + l->file_ofs, l->file_len);

	cm_vis = (d_bsp_vis_t *) cm_bsp.visibility;
	cm_vis->num_clusters = LittleLong(cm_vis->num_clusters);

	for (i = 0; i < cm_vis->num_clusters; i++) {
		cm_vis->bit_offsets[i][0] = LittleLong(cm_vis->bit_offsets[i][0]);
		cm_vis->bit_offsets[i][1] = LittleLong(cm_vis->bit_offsets[i][1]);
	}

	// If we have no visibility data, pad the clusters so that Cm_DecompressVis
	// produces correctly-sized rows. If we don't do this, non-VIS'ed maps will
	// not produce any visible entities.
	if (cm_bsp.num_visibility == 0) {
		cm_vis->num_clusters = cm_bsp.num_leafs;
	}
}

/*
 * @brief
 */
static void Cm_LoadEntityString(const d_bsp_lump_t *l) {

	cm_bsp.entity_string_len = l->file_len;

	if (l->file_len > MAX_BSP_ENT_STRING) {
		Com_Error(ERR_DROP, "Map has too large entity lump\n");
	}

	memcpy(cm_bsp.entity_string, cm_bsp.base + l->file_ofs, l->file_len);
}

/*
 * @brief Loads in the BSP and all submodels for collision detection.
 */
cm_bsp_model_t *Cm_LoadBsp(const char *name, int32_t *size) {
	d_bsp_header_t header;
	void *buf;
	uint32_t i;

	memset(&cm_bsp, 0, sizeof(cm_bsp));
	cm_vis = (d_bsp_vis_t *) cm_bsp.visibility;

	// if we've been asked to load a demo, just clean up and return
	if (!name) {
		cm_bsp.num_leafs = cm_bsp.num_areas = 1;
		cm_vis->num_clusters = 1;
		*size = 0;
		return &cm_bsp.models[0];
	}

	// load the file
	*size = Fs_Load(name, &buf);

	if (!buf) {
		Com_Error(ERR_DROP, "Couldn't load %s\n", name);
	}

	header = *(d_bsp_header_t *) buf;
	for (i = 0; i < sizeof(d_bsp_header_t) / sizeof(int32_t); i++)
		((int32_t *) &header)[i] = LittleLong(((int32_t *) &header)[i]);

	if (header.version != BSP_VERSION && header.version != BSP_VERSION_Q2W) {
		Com_Error(ERR_DROP, "%s has unsupported version: %d\n", name, header.version);
	}

	g_strlcpy(cm_bsp.name, name, sizeof(cm_bsp.name));

	cm_bsp.base = (byte *) buf;

	// load into heap
	Cm_LoadBspSurfaces(&header.lumps[BSP_LUMP_TEXINFO]);
	Cm_LoadBspLeafs(&header.lumps[BSP_LUMP_LEAFS]);
	Cm_LoadBspLeafBrushes(&header.lumps[BSP_LUMP_LEAF_BRUSHES]);
	Cm_LoadBspPlanes(&header.lumps[BSP_LUMP_PLANES]);
	Cm_LoadBspBrushes(&header.lumps[BSP_LUMP_BRUSHES]);
	Cm_LoadBspBrushSides(&header.lumps[BSP_LUMP_BRUSH_SIDES]);
	Cm_LoadBspInlineModels(&header.lumps[BSP_LUMP_MODELS]);
	Cm_LoadBspNodes(&header.lumps[BSP_LUMP_NODES]);
	Cm_LoadBspAreas(&header.lumps[BSP_LUMP_AREAS]);
	Cm_LoadBspAreaPortals(&header.lumps[BSP_LUMP_AREA_PORTALS]);
	Cm_LoadBspVisibility(&header.lumps[BSP_LUMP_VISIBILITY]);
	Cm_LoadEntityString(&header.lumps[BSP_LUMP_ENTITIES]);

	Fs_Free(buf);

	Cm_InitBoxHull();

	Cm_FloodAreas();

	return &cm_bsp.models[0];
}

/*
 * @brief
 */
cm_bsp_model_t *Cm_Model(const char *name) {
	int32_t num;

	if (!name || name[0] != '*') {
		Com_Error(ERR_DROP, "Bad name\n");
	}

	num = atoi(name + 1);

	if (num < 1 || num >= cm_bsp.num_models) {
		Com_Error(ERR_DROP, "Bad number: %d\n", num);
	}

	return &cm_bsp.models[num];
}

/*
 * @brief
 */
int32_t Cm_NumClusters(void) {
	return cm_vis->num_clusters;
}

/*
 * @brief
 */
int32_t Cm_NumModels(void) {
	return cm_bsp.num_models;
}

/*
 * @brief
 */
const char *Cm_EntityString(void) {
	return cm_bsp.entity_string;
}

/*
 * @brief Parses values from the worldspawn entity definition.
 */
const char *Cm_WorldspawnValue(const char *key) {
	const char *c, *v;

	c = strstr(Cm_EntityString(), va("\"%s\"", key));

	if (c) {
		ParseToken(&c); // parse the key itself
		v = ParseToken(&c); // parse the value
	} else {
		v = NULL;
	}

	return v;
}

/*
 * @brief
 */
int32_t Cm_LeafContents(const int32_t leaf_num) {

	if (leaf_num < 0 || leaf_num >= cm_bsp.num_leafs) {
		Com_Error(ERR_DROP, "Bad number: %d\n", leaf_num);
	}

	return cm_bsp.leafs[leaf_num].contents;
}

/*
 * @brief
 */
int32_t Cm_LeafCluster(const int32_t leaf_num) {

	if (leaf_num < 0 || leaf_num >= cm_bsp.num_leafs) {
		Com_Error(ERR_DROP, "Bad number: %d\n", leaf_num);
	}

	return cm_bsp.leafs[leaf_num].cluster;
}

/*
 * @brief
 */
int32_t Cm_LeafArea(const int32_t leaf_num) {

	if (leaf_num < 0 || leaf_num >= cm_bsp.num_leafs) {
		Com_Error(ERR_DROP, "Bad number: %d\n", leaf_num);
	}

	return cm_bsp.leafs[leaf_num].area;
}
