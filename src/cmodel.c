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

#include "cmodel.h"

typedef struct c_bsp_node_s {
	c_bsp_plane_t *plane;
	int children[2]; // negative numbers are leafs
} c_bsp_node_t;

typedef struct c_bsp_brush_side_s {
	c_bsp_plane_t *plane;
	c_bsp_surface_t *surface;
} c_bsp_brush_side_t;

typedef struct c_bsp_leaf_s {
	int contents;
	int cluster;
	int area;
	unsigned short first_leaf_brush;
	unsigned short num_leaf_brushes;
} c_bsp_leaf_t;

typedef struct c_bsp_brush_s {
	int contents;
	int num_sides;
	int first_brush_side;
} c_bsp_brush_t;

typedef struct c_bsp_area_s {
	int num_area_portals;
	int first_area_portal;
	int flood_num; // if two areas have equal flood_nums, they are connected
	int flood_valid;
} c_bsp_area_t;

typedef struct c_bsp_s {
	char name[MAX_QPATH];
	byte *base;

	int num_brush_sides;
	c_bsp_brush_side_t brush_sides[MAX_BSP_BRUSH_SIDES];

	int num_surfaces;
	c_bsp_surface_t surfaces[MAX_BSP_TEXINFO];

	int num_planes;
	c_bsp_plane_t planes[MAX_BSP_PLANES + 6]; // extra for box hull

	int num_nodes;
	c_bsp_node_t nodes[MAX_BSP_NODES + 6]; // extra for box hull

	int num_leafs;
	c_bsp_leaf_t leafs[MAX_BSP_LEAFS];
	int empty_leaf, solid_leaf;

	int num_leaf_brushes;
	unsigned short leaf_brushes[MAX_BSP_LEAF_BRUSHES];

	int num_models;
	c_model_t models[MAX_BSP_MODELS];

	int num_brushes;
	c_bsp_brush_t brushes[MAX_BSP_BRUSHES];

	int num_visibility;
	byte visibility[MAX_BSP_VISIBILITY];

	int entity_string_len;
	char entity_string[MAX_BSP_ENT_STRING];

	int num_areas;
	c_bsp_area_t areas[MAX_BSP_AREAS];

	int num_area_portals;
	d_bsp_area_portal_t area_portals[MAX_BSP_AREA_PORTALS];

	c_bsp_surface_t null_surface;

	int flood_valid;

	boolean_t portal_open[MAX_BSP_AREA_PORTALS];
} c_bsp_t;

static c_bsp_t c_bsp;
static d_bsp_vis_t *c_vis = (d_bsp_vis_t *) c_bsp.visibility;

static cvar_t *c_no_areas;

static void Cm_InitBoxHull(void);
static void Cm_FloodAreaConnections(void);

int c_point_contents;
int c_traces, c_bsp_brush_traces;

/*
 * Cm_LoadSubmodels
 */
static void Cm_LoadSubmodels(const d_bsp_lump_t *l) {
	const d_bsp_model_t *in;
	int i, j, count;

	in = (const void *) (c_bsp.base + l->file_ofs);
	if (l->file_len % sizeof(*in)) {
		Com_Error(ERR_DROP, "Cm_LoadSubmodels: Funny lump size.\n");
	}
	count = l->file_len / sizeof(*in);

	if (count < 1) {
		Com_Error(ERR_DROP, "Cm_LoadSubmodels: Map with no models.\n");
	}
	if (count > MAX_BSP_MODELS) {
		Com_Error(ERR_DROP, "Cm_LoadSubmodels: Map has too many models.\n");
	}

	c_bsp.num_models = count;

	for (i = 0; i < count; i++, in++) {
		c_model_t *out = &c_bsp.models[i];

		for (j = 0; j < 3; j++) { // spread the mins / maxs by a pixel
			out->mins[j] = LittleFloat(in->mins[j]) - 1;
			out->maxs[j] = LittleFloat(in->maxs[j]) + 1;
			out->origin[j] = LittleFloat(in->origin[j]);
		}
		out->head_node = LittleLong(in->head_node);
	}
}

/*
 * Cm_LoadSurfaces
 */
static void Cm_LoadSurfaces(const d_bsp_lump_t *l) {
	const d_bsp_texinfo_t *in;
	c_bsp_surface_t *out;
	int i, count;

	in = (const void *) (c_bsp.base + l->file_ofs);
	if (l->file_len % sizeof(*in)) {
		Com_Error(ERR_DROP, "Cm_LoadSurfaces: Funny lump size.\n");
		return;
	}
	count = l->file_len / sizeof(*in);
	if (count < 1) {
		Com_Error(ERR_DROP, "Cm_LoadSurfaces: Map with no surfaces.\n");
	}
	if (count > MAX_BSP_TEXINFO) {
		Com_Error(ERR_DROP, "Cm_LoadSurfaces: Map has too many surfaces.\n");
	}
	c_bsp.num_surfaces = count;
	out = c_bsp.surfaces;

	for (i = 0; i < count; i++, in++, out++) {
		strncpy(out->name, in->texture, sizeof(out->name) - 1);
		out->flags = LittleLong(in->flags);
		out->value = LittleLong(in->value);
	}
}

/*
 * Cm_LoadNodes
 */
static void Cm_LoadNodes(const d_bsp_lump_t *l) {
	const d_bsp_node_t *in;
	int child;
	c_bsp_node_t *out;
	int i, j, count;

	in = (const void *) (c_bsp.base + l->file_ofs);
	if (l->file_len % sizeof(*in)) {
		Com_Error(ERR_DROP, "Cm_LoadNodes: Funny lump size.\n");
	}
	count = l->file_len / sizeof(*in);

	if (count < 1) {
		Com_Error(ERR_DROP, "Cm_LoadNodes: Map has no nodes.\n");
	}
	if (count > MAX_BSP_NODES) {
		Com_Error(ERR_DROP, "Cm_LoadNodes: Map has too many nodes.\n");
	}

	out = c_bsp.nodes;

	c_bsp.num_nodes = count;

	for (i = 0; i < count; i++, out++, in++) {
		out->plane = c_bsp.planes + LittleLong(in->plane_num);
		for (j = 0; j < 2; j++) {
			child = LittleLong(in->children[j]);
			out->children[j] = child;
		}
	}
}

/*
 * Cm_LoadBrushes
 */
static void Cm_LoadBrushes(const d_bsp_lump_t *l) {
	const d_bsp_brush_t *in;
	c_bsp_brush_t *out;
	int i, count;

	in = (const void *) (c_bsp.base + l->file_ofs);
	if (l->file_len % sizeof(*in)) {
		Com_Error(ERR_DROP, "Cm_LoadBrushes: Funny lump size.\n");
	}
	count = l->file_len / sizeof(*in);

	if (count > MAX_BSP_BRUSHES) {
		Com_Error(ERR_DROP, "Cm_LoadBrushes: Map has too many brushes.\n");
	}

	out = c_bsp.brushes;

	c_bsp.num_brushes = count;

	for (i = 0; i < count; i++, out++, in++) {
		out->first_brush_side = LittleLong(in->first_side);
		out->num_sides = LittleLong(in->num_sides);
		out->contents = LittleLong(in->contents);
	}
}

/*
 * Cm_LoadLeafs
 */
static void Cm_LoadLeafs(const d_bsp_lump_t *l) {
	int i;
	c_bsp_leaf_t *out;
	const d_bsp_leaf_t *in;
	int count;

	in = (const void *) (c_bsp.base + l->file_ofs);
	if (l->file_len % sizeof(*in)) {
		Com_Error(ERR_DROP, "Cm_LoadLeafs: Funny lump size.\n");
	}
	count = l->file_len / sizeof(*in);

	if (count < 1) {
		Com_Error(ERR_DROP, "Cm_LoadLeafs: Map with no leafs.\n");
	}
	// need to save space for box planes
	if (count > MAX_BSP_PLANES) {
		Com_Error(ERR_DROP, "Cm_LoadLeafs: Map has too many planes.\n");
	}

	out = c_bsp.leafs;
	c_bsp.num_leafs = count;

	for (i = 0; i < count; i++, in++, out++) {
		out->contents = LittleLong(in->contents);
		out->cluster = LittleShort(in->cluster);
		out->area = LittleShort(in->area);
		out->first_leaf_brush = LittleShort(in->first_leaf_brush);
		out->num_leaf_brushes = LittleShort(in->num_leaf_brushes);
	}

	if (c_bsp.leafs[0].contents != CONTENTS_SOLID) {
		Com_Error(ERR_DROP, "Cm_LoadLeafs: Map leaf 0 is not CONTENTS_SOLID.\n");
	}
	c_bsp.solid_leaf = 0;
	c_bsp.empty_leaf = -1;
	for (i = 1; i < c_bsp.num_leafs; i++) {
		if (!c_bsp.leafs[i].contents) {
			c_bsp.empty_leaf = i;
			break;
		}
	}
	if (c_bsp.empty_leaf == -1)
		Com_Error(ERR_DROP, "Cm_LoadLeafs: Map does not have an empty leaf.\n");
}

/*
 * Cm_LoadPlanes
 */
static void Cm_LoadPlanes(const d_bsp_lump_t *l) {
	int i, j;
	c_bsp_plane_t *out;
	const d_bsp_plane_t *in;
	int count;

	in = (const void *) (c_bsp.base + l->file_ofs);
	if (l->file_len % sizeof(*in)) {
		Com_Error(ERR_DROP, "Cm_LoadPlanes: Funny lump size.\n");
	}
	count = l->file_len / sizeof(*in);

	if (count < 1) {
		Com_Error(ERR_DROP, "Cm_LoadPlanes: Map with no planes.\n");
	}
	// need to save space for box planes
	if (count > MAX_BSP_PLANES) {
		Com_Error(ERR_DROP, "Cm_LoadPlanes: Map has too many planes.\n");
	}

	out = c_bsp.planes;
	c_bsp.num_planes = count;

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
 * Cm_LoadLeafBrushes
 */
static void Cm_LoadLeafBrushes(const d_bsp_lump_t *l) {
	int i;
	unsigned short *out;
	const unsigned short *in;
	int count;

	in = (const void *) (c_bsp.base + l->file_ofs);
	if (l->file_len % sizeof(*in)) {
		Com_Error(ERR_DROP, "Cm_LoadLeafBrushes: Funny lump size.\n");
	}
	count = l->file_len / sizeof(*in);

	if (count < 1) {
		Com_Error(ERR_DROP, "Cm_LoadLeafBrushes: Map with no planes.\n");
	}
	// need to save space for box planes
	if (count > MAX_BSP_LEAF_BRUSHES) {
		Com_Error(ERR_DROP,
				"Cm_LoadLeafBrushes: Map has too many leafbrushes.\n");
	}

	out = c_bsp.leaf_brushes;
	c_bsp.num_leaf_brushes = count;

	for (i = 0; i < count; i++, in++, out++)
		*out = LittleShort(*in);
}

/*
 * Cm_LoadBrushSides
 */
static void Cm_LoadBrushSides(const d_bsp_lump_t *l) {
	int i;
	c_bsp_brush_side_t *out;
	const d_bsp_brush_side_t *in;
	int count;
	int num;

	in = (const void *) (c_bsp.base + l->file_ofs);
	if (l->file_len % sizeof(*in)) {
		Com_Error(ERR_DROP, "Cm_LoadBrushSides: Funny lump size.\n");
	}
	count = l->file_len / sizeof(*in);

	// need to save space for box planes
	if (count > MAX_BSP_BRUSH_SIDES) {
		Com_Error(ERR_DROP, "Cm_LoadBrushSides: Map has too many planes.\n");
	}

	out = c_bsp.brush_sides;
	c_bsp.num_brush_sides = count;

	for (i = 0; i < count; i++, in++, out++) {
		num = LittleShort(in->plane_num);
		out->plane = &c_bsp.planes[num];
		num = LittleShort(in->surf_num);
		if (num >= c_bsp.num_surfaces) {
			Com_Error(ERR_DROP, "Cm_LoadBrushSides: Bad brush_side surf_num.\n");
		}
		out->surface = &c_bsp.surfaces[num];
	}
}

/*
 * Cm_LoadAreas
 */
static void Cm_LoadAreas(const d_bsp_lump_t *l) {
	int i;
	c_bsp_area_t *out;
	const d_bsp_area_t *in;
	int count;

	in = (const void *) (c_bsp.base + l->file_ofs);
	if (l->file_len % sizeof(*in)) {
		Com_Error(ERR_DROP, "MOD_LoadAreas: Funny lump size.\n");
	}
	count = l->file_len / sizeof(*in);

	if (count > MAX_BSP_AREAS) {
		Com_Error(ERR_DROP, "Cm_LoadAreas: Map has too many areas.\n");
	}

	out = c_bsp.areas;
	c_bsp.num_areas = count;

	for (i = 0; i < count; i++, in++, out++) {
		out->num_area_portals = LittleLong(in->num_area_portals);
		out->first_area_portal = LittleLong(in->first_area_portal);
		out->flood_valid = 0;
		out->flood_num = 0;
	}
}

/*
 * Cm_LoadAreaPortals
 */
static void Cm_LoadAreaPortals(const d_bsp_lump_t *l) {
	int i;
	d_bsp_area_portal_t *out;
	const d_bsp_area_portal_t *in;
	int count;

	in = (const void *) (c_bsp.base + l->file_ofs);
	if (l->file_len % sizeof(*in)) {
		Com_Error(ERR_DROP, "Cm_LoadAreaPortals: Funny lump size.\n");
	}
	count = l->file_len / sizeof(*in);

	if (count > MAX_BSP_AREAS) {
		Com_Error(ERR_DROP, "Cm_LoadAreaPortals: Map has too many areas.\n");
	}

	out = c_bsp.area_portals;
	c_bsp.num_area_portals = count;

	for (i = 0; i < count; i++, in++, out++) {
		out->portal_num = LittleLong(in->portal_num);
		out->other_area = LittleLong(in->other_area);
	}
}

/*
 * Cm_LoadVisibility
 */
static void Cm_LoadVisibility(const d_bsp_lump_t *l) {
	int i;

	c_bsp.num_visibility = l->file_len;
	if (l->file_len > MAX_BSP_VISIBILITY) {
		Com_Error(ERR_DROP,
				"Cm_LoadVisibility: Map has too large visibility lump.\n");
	}

	memcpy(c_bsp.visibility, c_bsp.base + l->file_ofs, l->file_len);

	c_vis->num_clusters = LittleLong(c_vis->num_clusters);

	for (i = 0; i < c_vis->num_clusters; i++) {
		c_vis->bit_offsets[i][0] = LittleLong(c_vis->bit_offsets[i][0]);
		c_vis->bit_offsets[i][1] = LittleLong(c_vis->bit_offsets[i][1]);
	}
}

/*
 * Cm_LoadEntityString
 */
static void Cm_LoadEntityString(const d_bsp_lump_t *l) {

	c_bsp.entity_string_len = l->file_len;

	if (l->file_len > MAX_BSP_ENT_STRING) {
		Com_Error(ERR_DROP,
				"Cm_LoadEntityString: Map has too large entity lump.\n");
	}

	memcpy(c_bsp.entity_string, c_bsp.base + l->file_ofs, l->file_len);
}

/*
 * Cm_LoadBsp
 *
 * Loads in the BSP and all submodels for collision detection.
 */
c_model_t *Cm_LoadBsp(const char *name, int *size) {
	d_bsp_header_t header;
	void *buf;
	unsigned int i;

	c_no_areas = Cvar_Get("c_no_areas", "0", 0, NULL);

	memset(&c_bsp, 0, sizeof(c_bsp));

	// if we've been asked to load a demo, just clean up and return
	if (!name) {
		c_bsp.num_leafs = c_bsp.num_areas = 1;
		c_vis->num_clusters = 1;
		*size = 0;
		return &c_bsp.models[0];
	}

	// load the file
	*size = Fs_LoadFile(name, &buf);

	if (!buf)
		Com_Error(ERR_DROP, "Cm_LoadMap: Couldn't load %s.\n", name);

	header = *(d_bsp_header_t *) buf;
	for (i = 0; i < sizeof(d_bsp_header_t) / 4; i++)
		((int *) &header)[i] = LittleLong(((int *) &header)[i]);

	if (header.version != BSP_VERSION && header.version != BSP_VERSION_) {
		Fs_FreeFile(buf);
		Com_Error(ERR_DROP, "Cm_LoadMap: %s has unsupported version: %d.\n",
				name, header.version);
	}

	strncpy(c_bsp.name, name, sizeof(c_bsp.name));

	c_bsp.base = (byte *) buf;

	// load into heap
	Cm_LoadSurfaces(&header.lumps[LUMP_TEXINFO]);
	Cm_LoadLeafs(&header.lumps[LUMP_LEAFS]);
	Cm_LoadLeafBrushes(&header.lumps[LUMP_LEAF_BRUSHES]);
	Cm_LoadPlanes(&header.lumps[LUMP_PLANES]);
	Cm_LoadBrushes(&header.lumps[LUMP_BRUSHES]);
	Cm_LoadBrushSides(&header.lumps[LUMP_BRUSH_SIDES]);
	Cm_LoadSubmodels(&header.lumps[LUMP_MODELS]);
	Cm_LoadNodes(&header.lumps[LUMP_NODES]);
	Cm_LoadAreas(&header.lumps[LUMP_AREAS]);
	Cm_LoadAreaPortals(&header.lumps[LUMP_AREA_PORTALS]);
	Cm_LoadVisibility(&header.lumps[LUMP_VISIBILITY]);
	Cm_LoadEntityString(&header.lumps[LUMP_ENTITIES]);

	Fs_FreeFile(buf);

	Cm_InitBoxHull();

	Cm_FloodAreaConnections();

	return &c_bsp.models[0];
}

/*
 * Cm_Model
 */
c_model_t *Cm_Model(const char *name) {
	int num;

	if (!name || name[0] != '*') {
		Com_Error(ERR_DROP, "Cm_Model: Bad name.\n");
	}

	num = atoi(name + 1);

	if (num < 1 || num >= c_bsp.num_models) {
		Com_Error(ERR_DROP, "Cm_Model: Bad number: %d\n.", num);
	}

	return &c_bsp.models[num];
}

/*
 * Cm_NumClusters
 */
int Cm_NumClusters(void) {
	return c_vis->num_clusters;
}

/*
 * Cm_NumModels
 */
int Cm_NumModels(void) {
	return c_bsp.num_models;
}

/*
 * Cm_EntityString
 */
char *Cm_EntityString(void) {
	return c_bsp.entity_string;
}

/*
 * Cm_LeafContents
 */
int Cm_LeafContents(int leaf_num) {

	if (leaf_num < 0 || leaf_num >= c_bsp.num_leafs) {
		Com_Error(ERR_DROP, "Cm_LeafContents: Bad number.\n");
	}

	return c_bsp.leafs[leaf_num].contents;
}

/*
 * Cm_LeafCluster
 */
int Cm_LeafCluster(int leaf_num) {

	if (leaf_num < 0 || leaf_num >= c_bsp.num_leafs) {
		Com_Error(ERR_DROP, "Cm_LeafCluster: Bad number.\n");
	}

	return c_bsp.leafs[leaf_num].cluster;
}

/*
 * Cm_LeafArea
 */
int Cm_LeafArea(int leaf_num) {

	if (leaf_num < 0 || leaf_num >= c_bsp.num_leafs) {
		Com_Error(ERR_DROP, "Cm_LeafArea: Bad number.\n");
	}

	return c_bsp.leafs[leaf_num].area;
}

// bounding box to bsp tree structure for tracing
typedef struct c_bounding_box_s {
	int head_node;
	c_bsp_plane_t *planes;
	c_bsp_brush_t *brush;
	c_bsp_leaf_t *leaf;
} c_bounding_box_t;

static c_bounding_box_t cm_box;

/*
 * Cm_InitBoxHull
 *
 * Set up the planes and nodes so that the six floats of a bounding box
 * can just be stored out and get a proper clipping hull structure.
 */
static void Cm_InitBoxHull(void) {
	int i;

	cm_box.head_node = c_bsp.num_nodes;
	cm_box.planes = &c_bsp.planes[c_bsp.num_planes];
	if (c_bsp.num_nodes + 6 > MAX_BSP_NODES || c_bsp.num_brushes + 1
			> MAX_BSP_BRUSHES || c_bsp.num_leaf_brushes + 1
			> MAX_BSP_LEAF_BRUSHES || c_bsp.num_brush_sides + 6
			> MAX_BSP_BRUSH_SIDES || c_bsp.num_planes + 12 > MAX_BSP_PLANES) {
		Com_Error(ERR_DROP, "Cm_InitBoxHull: Not enough room for box tree.\n");
	}

	cm_box.brush = &c_bsp.brushes[c_bsp.num_brushes];
	cm_box.brush->num_sides = 6;
	cm_box.brush->first_brush_side = c_bsp.num_brush_sides;
	cm_box.brush->contents = CONTENTS_MONSTER;

	cm_box.leaf = &c_bsp.leafs[c_bsp.num_leafs];
	cm_box.leaf->contents = CONTENTS_MONSTER;
	cm_box.leaf->first_leaf_brush = c_bsp.num_leaf_brushes;
	cm_box.leaf->num_leaf_brushes = 1;

	c_bsp.leaf_brushes[c_bsp.num_leaf_brushes] = c_bsp.num_brushes;

	for (i = 0; i < 6; i++) {
		const int side = i & 1;
		c_bsp_node_t *c;
		c_bsp_plane_t *p;
		c_bsp_brush_side_t *s;

		// brush sides
		s = &c_bsp.brush_sides[c_bsp.num_brush_sides + i];
		s->plane = c_bsp.planes + (c_bsp.num_planes + i * 2 + side);
		s->surface = &c_bsp.null_surface;

		// nodes
		c = &c_bsp.nodes[cm_box.head_node + i];
		c->plane = c_bsp.planes + (c_bsp.num_planes + i * 2);
		c->children[side] = -1 - c_bsp.empty_leaf;
		if (i != 5)
			c->children[side ^ 1] = cm_box.head_node + i + 1;
		else
			c->children[side ^ 1] = -1 - c_bsp.num_leafs;

		// planes
		p = &cm_box.planes[i * 2];
		p->type = i >> 1;
		p->sign_bits = 0;
		VectorClear(p->normal);
		p->normal[i >> 1] = 1;

		p = &cm_box.planes[i * 2 + 1];
		p->type = PLANE_ANYX + (i >> 1);
		p->sign_bits = 0;
		VectorClear(p->normal);
		p->normal[i >> 1] = -1;
	}
}

/*
 * Cm_HeadnodeForBox
 *
 * To keep everything totally uniform, bounding boxes are turned into small
 * BSP trees instead of being compared directly.
 */
int Cm_HeadnodeForBox(const vec3_t mins, const vec3_t maxs) {
	cm_box.planes[0].dist = maxs[0];
	cm_box.planes[1].dist = -maxs[0];
	cm_box.planes[2].dist = mins[0];
	cm_box.planes[3].dist = -mins[0];
	cm_box.planes[4].dist = maxs[1];
	cm_box.planes[5].dist = -maxs[1];
	cm_box.planes[6].dist = mins[1];
	cm_box.planes[7].dist = -mins[1];
	cm_box.planes[8].dist = maxs[2];
	cm_box.planes[9].dist = -maxs[2];
	cm_box.planes[10].dist = mins[2];
	cm_box.planes[11].dist = -mins[2];

	return cm_box.head_node;
}

/*
 * Cm_PointLeafnum
 */
static int Cm_PointLeafnum_r(const vec3_t p, int num) {

	while (num >= 0) {
		float d;
		c_bsp_node_t *node = c_bsp.nodes + num;
		c_bsp_plane_t *plane = node->plane;

		if (AXIAL(plane))
			d = p[plane->type] - plane->dist;
		else
			d = DotProduct(plane->normal, p) - plane->dist;

		if (d < 0)
			num = node->children[1];
		else
			num = node->children[0];
	}

	c_point_contents++; // optimize counter, TOOD: atomic increment for thread-safety

	return -1 - num;
}

/*
 * Cm_PointLeafnum
 */
int Cm_PointLeafnum(const vec3_t p) {

	if (!c_bsp.num_planes)
		return 0; // sound may call this without map loaded

	return Cm_PointLeafnum_r(p, 0);
}

/*
 * Cm_BoxLeafnums
 *
 * Fills in a list of all the leafs touched
 */

typedef struct c_bsp_leaf_data_s {
	int count, max_count;
	int *list;
	const float *mins, *maxs;
	int top_node;
} c_bsp_leaf_data_t;

static void Cm_BoxLeafnums_r(int nodenum, c_bsp_leaf_data_t *data) {
	c_bsp_plane_t *plane;
	c_bsp_node_t *node;
	int s;

	while (true) {
		if (nodenum < 0) {
			if (data->count >= data->max_count) {
				return;
			}
			data->list[data->count++] = -1 - nodenum;
			return;
		}

		node = &c_bsp.nodes[nodenum];
		plane = node->plane;
		s = BoxOnPlaneSide(data->mins, data->maxs, plane);
		if (s == 1)
			nodenum = node->children[0];
		else if (s == 2)
			nodenum = node->children[1];
		else { // go down both
			if (data->top_node == -1)
				data->top_node = nodenum;
			Cm_BoxLeafnums_r(node->children[0], data);
			nodenum = node->children[1];
		}
	}
}

/*
 * Cm_BoxLeafnums_head_node
 */
static int Cm_BoxLeafnums_head_node(const vec3_t mins, const vec3_t maxs,
		int *list, int list_size, int head_node, int *top_node) {
	c_bsp_leaf_data_t data;
	data.list = list;
	data.count = 0;
	data.max_count = list_size;
	data.mins = mins;
	data.maxs = maxs;

	data.top_node = -1;

	Cm_BoxLeafnums_r(head_node, &data);

	if (top_node)
		*top_node = data.top_node;

	return data.count;
}

/*
 * Cm_BoxLeafnums
 *
 * Populates the list of leafs the specified bounding box touches. Returns the
 * length of the populated list.
 */
int Cm_BoxLeafnums(const vec3_t mins, const vec3_t maxs, int *list,
		int list_size, int *top_node) {
	return Cm_BoxLeafnums_head_node(mins, maxs, list, list_size,
			c_bsp.models[0].head_node, top_node);
}

/*
 * Cm_PointContents
 */
int Cm_PointContents(const vec3_t p, int head_node) {
	int l;

	if (!c_bsp.num_nodes) // map not loaded
		return 0;

	l = Cm_PointLeafnum_r(p, head_node);

	return c_bsp.leafs[l].contents;
}

/*
 * Cm_TransformedPointContents
 *
 * Handles offseting and rotation of the end points for moving and
 * rotating entities
 */
int Cm_TransformedPointContents(const vec3_t p, int head_node,
		const vec3_t origin, const vec3_t angles) {
	vec3_t p_l;
	vec3_t temp;
	vec3_t forward, right, up;
	int l;

	// subtract origin offset
	VectorSubtract(p, origin, p_l);

	// rotate start and end into the models frame of reference
	if (head_node != cm_box.head_node && (angles[0] || angles[1] || angles[2])) {
		AngleVectors(angles, forward, right, up);

		VectorCopy(p_l, temp);
		p_l[0] = DotProduct(temp, forward);
		p_l[1] = -DotProduct(temp, right);
		p_l[2] = DotProduct(temp, up);
	}

	l = Cm_PointLeafnum_r(p_l, head_node);

	return c_bsp.leafs[l].contents;
}

/*
 *
 * BOX TRACING
 *
 */

// 1/32 epsilon to keep floating point happy
#define DIST_EPSILON	(0.03125)

typedef struct {
	vec3_t start, end;
	vec3_t mins, maxs;
	vec3_t extents;

	c_trace_t trace;
	int contents;
	boolean_t is_point; // optimized case

	int mailbox[16]; // used to avoid multiple intersection tests with brushes
} c_trace_data_t;

/*
 * Cm_BrushAlreadyTested
 */
static boolean_t Cm_BrushAlreadyTested(int brush_num, c_trace_data_t *data) {
	int hash = brush_num & 15;
	boolean_t skip = (data->mailbox[hash] == brush_num);
	data->mailbox[hash] = brush_num;
	return skip;
}

/*
 * Cm_ClipBoxToBrush
 *
 * Clips the bounded box to all brush sides for the given brush.  Returns
 * true if the box was clipped, false otherwise.
 */
static void Cm_ClipBoxToBrush(vec3_t mins, vec3_t maxs, vec3_t p1, vec3_t p2,
		c_trace_t *trace, c_bsp_leaf_t *leaf, c_bsp_brush_t *brush,
		boolean_t is_point) {
	int i, j;
	c_bsp_plane_t *clip_plane;
	float dist;
	float enter_fraction, leave_fraction;
	vec3_t ofs;
	float d1, d2;
	boolean_t end_outside, start_outside;
	c_bsp_brush_side_t *lead_side;

	enter_fraction = -1;
	leave_fraction = 1;
	clip_plane = NULL;

	if (!brush->num_sides)
		return;

	c_bsp_brush_traces++;

	end_outside = start_outside = false;
	lead_side = NULL;

	for (i = 0; i < brush->num_sides; i++) {
		c_bsp_brush_side_t *side = &c_bsp.brush_sides[brush->first_brush_side
				+ i];
		c_bsp_plane_t *plane = side->plane;

		// FIXME: special case for axial

		if (!is_point) { // general box case

			// push the plane out appropriately for mins/maxs

			// FIXME: use sign_bits into 8 way lookup for each mins/maxs
			for (j = 0; j < 3; j++) {
				if (plane->normal[j] < 0.0)
					ofs[j] = maxs[j];
				else
					ofs[j] = mins[j];
			}
			dist = DotProduct(ofs, plane->normal);
			dist = plane->dist - dist;
		} else { // special point case
			dist = plane->dist;
		}

		d1 = DotProduct(p1, plane->normal) - dist;
		d2 = DotProduct(p2, plane->normal) - dist;

		if (d2 > 0.0)
			end_outside = true; // end point is not in solid
		if (d1 > 0.0)
			start_outside = true;

		// if completely in front of face, no intersection
		if (d1 > 0.0 && d2 >= d1)
			return;

		if (d1 <= 0.0 && d2 <= 0.0)
			continue;

		// crosses face
		if (d1 > d2) { // enter
			const float f = (d1 - DIST_EPSILON) / (d1 - d2);
			if (f > enter_fraction) {
				enter_fraction = f;
				clip_plane = plane;
				lead_side = side;
			}
		} else { // leave
			const float f = (d1 + DIST_EPSILON) / (d1 - d2);
			if (f < leave_fraction)
				leave_fraction = f;
		}
	}

	if (!start_outside) { // original point was inside brush
		trace->start_solid = true;
		if (!end_outside)
			trace->all_solid = true;
		trace->leaf_num = leaf - c_bsp.leafs;
	}

	if (enter_fraction < leave_fraction) { // pierced brush
		if (enter_fraction > -1.0 && enter_fraction < trace->fraction) {
			if (enter_fraction < 0.0)
				enter_fraction = 0.0;
			trace->fraction = enter_fraction;
			trace->plane = *clip_plane;
			trace->surface = lead_side->surface;
			trace->contents = brush->contents;
			trace->leaf_num = leaf - c_bsp.leafs;
		}
	}
}

/*
 * Cm_TestBoxInBrush
 */
static void Cm_TestBoxInBrush(vec3_t mins, vec3_t maxs, vec3_t p1,
		c_trace_t *trace, c_bsp_brush_t *brush) {
	int i, j;
	c_bsp_plane_t *plane;
	float dist;
	vec3_t offset;
	float d1;
	c_bsp_brush_side_t *side;

	if (!brush->num_sides)
		return;

	for (i = 0; i < brush->num_sides; i++) {
		side = &c_bsp.brush_sides[brush->first_brush_side + i];
		plane = side->plane;

		// FIXME: special case for axial

		// general box case

		// push the plane out appropriately for mins/maxs

		// FIXME: use sign_bits into 8 way lookup for each mins/maxs
		for (j = 0; j < 3; j++) {
			if (plane->normal[j] < 0.0)
				offset[j] = maxs[j];
			else
				offset[j] = mins[j];
		}
		dist = DotProduct(offset, plane->normal);
		dist = plane->dist - dist;

		d1 = DotProduct(p1, plane->normal) - dist;

		// if completely in front of face, no intersection
		if (d1 > 0.0)
			return;
	}

	// inside this brush
	trace->start_solid = trace->all_solid = true;
	trace->fraction = 0.0;
	trace->contents = brush->contents;
}

/*
 * Cm_TraceToLeaf
 */
static void Cm_TraceToLeaf(int leaf_num, c_trace_data_t *data) {
	int k;
	c_bsp_leaf_t *leaf;

	leaf = &c_bsp.leafs[leaf_num];

	if (!(leaf->contents & data->contents))
		return;

	// trace line against all brushes in the leaf
	for (k = 0; k < leaf->num_leaf_brushes; k++) {
		const int brush_num = c_bsp.leaf_brushes[leaf->first_leaf_brush + k];
		c_bsp_brush_t *b = &c_bsp.brushes[brush_num];

		if (Cm_BrushAlreadyTested(brush_num, data))
			continue; // already checked this brush in another leaf

		if (!(b->contents & data->contents))
			continue;

		Cm_ClipBoxToBrush(data->mins, data->maxs, data->start, data->end,
				&data->trace, leaf, b, data->is_point);

		if (data->trace.all_solid)
			return;
	}
}

/*
 * Cm_TestInLeaf
 */
static void Cm_TestInLeaf(int leaf_num, c_trace_data_t *data) {
	int k;
	c_bsp_leaf_t *leaf;

	leaf = &c_bsp.leafs[leaf_num];
	if (!(leaf->contents & data->contents))
		return;

	// trace line against all brushes in the leaf
	for (k = 0; k < leaf->num_leaf_brushes; k++) {
		const int brush_num = c_bsp.leaf_brushes[leaf->first_leaf_brush + k];
		c_bsp_brush_t *b = &c_bsp.brushes[brush_num];

		if (Cm_BrushAlreadyTested(brush_num, data))
			continue; // already checked this brush in another leaf

		if (!(b->contents & data->contents))
			continue;

		Cm_TestBoxInBrush(data->mins, data->maxs, data->start, &data->trace, b);

		if (data->trace.all_solid)
			return;
	}
}

/*
 * Cm_RecursiveHullCheck
 */
static void Cm_RecursiveHullCheck(int num, float p1f, float p2f,
		const vec3_t p1, const vec3_t p2, c_trace_data_t *data) {
	const c_bsp_node_t *node;
	const c_bsp_plane_t *plane;
	float t1, t2, offset;
	float frac, frac2;
	int i;
	vec3_t mid;
	int side;
	float midf;

	if (data->trace.fraction <= p1f)
		return; // already hit something nearer

	// if < 0, we are in a leaf node
	if (num < 0) {
		Cm_TraceToLeaf(-1 - num, data);
		return;
	}

	// find the point distances to the seperating plane
	// and the offset for the size of the box
	node = c_bsp.nodes + num;
	plane = node->plane;

	if (AXIAL(plane)) {
		t1 = p1[plane->type] - plane->dist;
		t2 = p2[plane->type] - plane->dist;
		offset = data->extents[plane->type];
	} else {
		t1 = DotProduct(plane->normal, p1) - plane->dist;
		t2 = DotProduct(plane->normal, p2) - plane->dist;
		if (data->is_point)
			offset = 0;
		else
			offset = fabsf(data->extents[0] * plane->normal[0]) + fabsf(
					data->extents[1] * plane->normal[1]) + fabsf(
					data->extents[2] * plane->normal[2]);
	}

	// see which sides we need to consider
	if (t1 >= offset && t2 >= offset) {
		Cm_RecursiveHullCheck(node->children[0], p1f, p2f, p1, p2, data);
		return;
	}
	if (t1 <= -offset && t2 <= -offset) {
		Cm_RecursiveHullCheck(node->children[1], p1f, p2f, p1, p2, data);
		return;
	}

	// put the crosspoint DIST_EPSILON pixels on the near side
	if (t1 < t2) {
		const float idist = 1.0 / (t1 - t2);
		side = 1;
		frac2 = (t1 + offset + DIST_EPSILON) * idist;
		frac = (t1 - offset + DIST_EPSILON) * idist;
	} else if (t1 > t2) {
		const float idist = 1.0 / (t1 - t2);
		side = 0;
		frac2 = (t1 - offset - DIST_EPSILON) * idist;
		frac = (t1 + offset + DIST_EPSILON) * idist;
	} else {
		side = 0;
		frac = 1;
		frac2 = 0;
	}

	// move up to the node
	if (frac < 0)
		frac = 0;
	if (frac > 1)
		frac = 1;

	midf = p1f + (p2f - p1f) * frac;
	for (i = 0; i < 3; i++)
		mid[i] = p1[i] + frac * (p2[i] - p1[i]);

	Cm_RecursiveHullCheck(node->children[side], p1f, midf, p1, mid, data);

	// go past the node
	if (frac2 < 0)
		frac2 = 0;
	if (frac2 > 1)
		frac2 = 1;

	midf = p1f + (p2f - p1f) * frac2;
	for (i = 0; i < 3; i++)
		mid[i] = p1[i] + frac2 * (p2[i] - p1[i]);

	Cm_RecursiveHullCheck(node->children[side ^ 1], midf, p2f, mid, p2, data);
}

/*
 * Cm_BoxTrace
 */
c_trace_t Cm_BoxTrace(const vec3_t start, const vec3_t end, const vec3_t mins,
		const vec3_t maxs, int head_node, int brush_mask) {
	int i, point_leafs[1024];

	c_trace_data_t data;

	c_traces++; // for statistics

	// fill in a default trace
	memset(&data.trace, 0, sizeof(data.trace));
	data.trace.fraction = 1.0;
	data.trace.surface = &c_bsp.null_surface;

	if (!c_bsp.num_nodes) // map not loaded
		return data.trace;

	memset(&data.mailbox, 0xffffffff, sizeof(data.mailbox));
	data.contents = brush_mask;
	VectorCopy(start, data.start);
	VectorCopy(end, data.end);
	VectorCopy(mins, data.mins);
	VectorCopy(maxs, data.maxs);

	// check for position test special case
	if (VectorCompare(start, end)) {
		int i, leafs;
		vec3_t c1, c2;
		int top_node;

		VectorAdd(start, mins, c1);
		VectorAdd(start, maxs, c2);
		for (i = 0; i < 3; i++) {
			c1[i] -= 1.0;
			c2[i] += 1.0;
		}

		leafs = Cm_BoxLeafnums_head_node(c1, c2, point_leafs,
				sizeof(point_leafs) / sizeof(int), head_node, &top_node); // NOTE: was * sizeof(int)

		for (i = 0; i < leafs; i++) {
			Cm_TestInLeaf(point_leafs[i], &data);
			if (data.trace.all_solid)
				break;
		}
		VectorCopy(start, data.trace.end);
		return data.trace;
	}

	// check for point special case
	if (VectorCompare(mins, vec3_origin) && VectorCompare(maxs, vec3_origin)) {
		data.is_point = true;
		VectorClear(data.extents);
	} else {
		data.is_point = false;
		data.extents[0] = -mins[0] > maxs[0] ? -mins[0] : maxs[0];
		data.extents[1] = -mins[1] > maxs[1] ? -mins[1] : maxs[1];
		data.extents[2] = -mins[2] > maxs[2] ? -mins[2] : maxs[2];
	}

	// general sweeping through world
	Cm_RecursiveHullCheck(head_node, 0, 1, start, end, &data);

	if (data.trace.fraction == 1) {
		VectorCopy(end, data.trace.end);
	} else {
		for (i = 0; i < 3; i++)
			data.trace.end[i] = start[i] + data.trace.fraction * (end[i]
					- start[i]);
	}
	return data.trace;
}

/*
 * Cm_TransformedBoxTrace
 *
 * Handles translation and rotation of the end points for moving and
 * rotating entities.
 */
c_trace_t Cm_TransformedBoxTrace(const vec3_t start, const vec3_t end,
		const vec3_t mins, const vec3_t maxs, int head_node, int brush_mask,
		const vec3_t origin, const vec3_t angles) {

	c_trace_t trace;
	vec3_t start_l, end_l;
	vec3_t a;
	vec3_t forward, right, up;
	vec3_t temp;
	boolean_t rotated;

	// subtract origin offset
	VectorSubtract(start, origin, start_l);
	VectorSubtract(end, origin, end_l);

	// rotate start and end into the models frame of reference
	if (head_node != cm_box.head_node && (angles[0] || angles[1] || angles[2]))
		rotated = true;
	else
		rotated = false;

	if (rotated) {
		AngleVectors(angles, forward, right, up);

		VectorCopy(start_l, temp);
		start_l[0] = DotProduct(temp, forward);
		start_l[1] = -DotProduct(temp, right);
		start_l[2] = DotProduct(temp, up);

		VectorCopy(end_l, temp);
		end_l[0] = DotProduct(temp, forward);
		end_l[1] = -DotProduct(temp, right);
		end_l[2] = DotProduct(temp, up);
	}

	// sweep the box through the model
	trace = Cm_BoxTrace(start_l, end_l, mins, maxs, head_node, brush_mask);

	if (rotated && trace.fraction != 1.0) {
		// FIXME: figure out how to do this with existing angles
		VectorNegate(angles, a);
		AngleVectors(a, forward, right, up);

		VectorCopy(trace.plane.normal, temp);
		trace.plane.normal[0] = DotProduct(temp, forward);
		trace.plane.normal[1] = -DotProduct(temp, right);
		trace.plane.normal[2] = DotProduct(temp, up);
	}

	trace.end[0] = start[0] + trace.fraction * (end[0] - start[0]);
	trace.end[1] = start[1] + trace.fraction * (end[1] - start[1]);
	trace.end[2] = start[2] + trace.fraction * (end[2] - start[2]);

	return trace;
}

/*
 *
 * PVS / PHS
 *
 */

/*
 * Cm_DecompressVis
 */
static void Cm_DecompressVis(const byte *in, byte *out) {
	int c;
	byte *out_p;
	int row;

	row = (c_vis->num_clusters + 7) >> 3;
	out_p = out;

	if (!in || !c_bsp.num_visibility) { // no vis info, so make all visible
		while (row) {
			*out_p++ = 0xff;
			row--;
		}
		return;
	}

	do {
		if (*in) {
			*out_p++ = *in++;
			continue;
		}

		c = in[1];
		in += 2;
		if ((out_p - out) + c > row) {
			c = row - (out_p - out);
			Com_Warn("Cm_DecompressVis: Overrun.\n");
		}
		while (c) {
			*out_p++ = 0;
			c--;
		}
	} while (out_p - out < row);
}

/*
 * Cm_ClusterPVS
 */
byte *Cm_ClusterPVS(int cluster) {
	static byte pvs_row[MAX_BSP_LEAFS / 8];

	if (cluster == -1)
		memset(pvs_row, 0, (c_vis->num_clusters + 7) >> 3);
	else
		Cm_DecompressVis(
				c_bsp.visibility + c_vis->bit_offsets[cluster][DVIS_PVS],
				pvs_row);

	return pvs_row;
}

/*
 * Cm_ClusterPHS
 */
byte *Cm_ClusterPHS(int cluster) {
	static byte phs_row[MAX_BSP_LEAFS / 8];

	if (cluster == -1)
		memset(phs_row, 0, (c_vis->num_clusters + 7) >> 3);
	else
		Cm_DecompressVis(
				c_bsp.visibility + c_vis->bit_offsets[cluster][DVIS_PHS],
				phs_row);

	return phs_row;
}

/*
 *
 * AREA_PORTALS
 *
 */

static void Cm_FloodArea(c_bsp_area_t *area, int flood_num) {
	int i;
	const d_bsp_area_portal_t *p;

	if (area->flood_valid == c_bsp.flood_valid) {
		if (area->flood_num == flood_num)
			return;
		Com_Error(ERR_DROP, "Cm_FloodArea: Reflooded.\n");
	}

	area->flood_num = flood_num;
	area->flood_valid = c_bsp.flood_valid;
	p = &c_bsp.area_portals[area->first_area_portal];
	for (i = 0; i < area->num_area_portals; i++, p++) {
		if (c_bsp.portal_open[p->portal_num])
			Cm_FloodArea(&c_bsp.areas[p->other_area], flood_num);
	}
}

/*
 * Cm_FloodAreaConnections
 */
static void Cm_FloodAreaConnections(void) {
	int i;
	int flood_num;

	// all current floods are now invalid
	c_bsp.flood_valid++;
	flood_num = 0;

	// area 0 is not used
	for (i = 1; i < c_bsp.num_areas; i++) {
		c_bsp_area_t *area = &c_bsp.areas[i];
		if (area->flood_valid == c_bsp.flood_valid)
			continue; // already flooded into
		flood_num++;
		Cm_FloodArea(area, flood_num);
	}
}

/*
 * Cm_SetAreaPortalState
 */
void Cm_SetAreaPortalState(int portal_num, boolean_t open) {
	if (portal_num > c_bsp.num_area_portals) {
		Com_Error(ERR_DROP,
				"Cm_SetAreaPortalState: portal_num > cm.num_area_portals.\n");
	}

	c_bsp.portal_open[portal_num] = open;
	Cm_FloodAreaConnections();
}

/*
 * Cm_AreasConnected
 */
boolean_t Cm_AreasConnected(int area1, int area2) {
	if (c_no_areas->value)
		return true;

	if (area1 > c_bsp.num_areas || area2 > c_bsp.num_areas) {
		Com_Error(ERR_DROP, "Cm_AreasConnected: area > cm.num_areas.\n");
	}

	if (c_bsp.areas[area1].flood_num == c_bsp.areas[area2].flood_num)
		return true;
	return false;
}

/*
 * Cm_WriteAreaBits
 *
 * Writes a length byte followed by a bit vector of all the areas
 * that are in the same flood as the area parameter
 *
 * This is used by the client view to cull visibility
 */
int Cm_WriteAreaBits(byte *buffer, int area) {
	int i;
	const int bytes = (c_bsp.num_areas + 7) >> 3;

	if (c_no_areas->value) { // for debugging, send everything
		memset(buffer, 0xff, bytes);
	} else {
		const int flood_num = c_bsp.areas[area].flood_num;
		memset(buffer, 0, bytes);

		for (i = 0; i < c_bsp.num_areas; i++) {
			if (c_bsp.areas[i].flood_num == flood_num || !area)
				buffer[i >> 3] |= 1 << (i & 7);
		}
	}

	return bytes;
}

/*
 * Cm_HeadnodeVisible
 *
 * Returns true if any leaf under head_node has a cluster that
 * is potentially visible
 */
boolean_t Cm_HeadnodeVisible(int node_num, byte *vis) {
	const c_bsp_node_t *node;

	if (node_num < 0) { // at a leaf, check it
		const int leaf_num = -1 - node_num;
		const int cluster = c_bsp.leafs[leaf_num].cluster;

		if (cluster == -1)
			return false;

		if (vis[cluster >> 3] & (1 << (cluster & 7)))
			return true;

		return false;
	}

	node = &c_bsp.nodes[node_num];

	if (Cm_HeadnodeVisible(node->children[0], vis))
		return true;

	return Cm_HeadnodeVisible(node->children[1], vis);
}
