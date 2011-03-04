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

typedef struct {
	cplane_t *plane;
	int children[2];  // negative numbers are leafs
} cnode_t;

typedef struct {
	cplane_t *plane;
	csurface_t *surface;
} cbrushside_t;

typedef struct {
	int contents;
	int cluster;
	int area;
	unsigned short first_leaf_brush;
	unsigned short num_leaf_brushes;
} cleaf_t;

typedef struct {
	int contents;
	int numsides;
	int firstbrushside;
} cbrush_t;

typedef struct {
	int num_area_portals;
	int first_area_portal;
	int floodnum;  // if two areas have equal floodnums, they are connected
	int floodvalid;
} carea_t;

static char map_name[MAX_QPATH];

static int numbrushsides;
static cbrushside_t map_brushsides[MAX_BSP_BRUSHSIDES];

static int num_texinfo;
static csurface_t csurfaces[MAX_BSP_TEXINFO];

static int num_planes;
static cplane_t map_planes[MAX_BSP_PLANES + 6];  // extra for box hull

static int num_nodes;
static cnode_t map_nodes[MAX_BSP_NODES + 6];  // extra for box hull

static int num_leafs = 1;  // allow leaf funcs to be called without a map
static cleaf_t map_leafs[MAX_BSP_LEAFS];
static int emptyleaf, solidleaf;

static int num_leaf_brushes;
static unsigned short map_leafbrushes[MAX_BSP_LEAFBRUSHES];

int num_cmodels;
cmodel_t map_cmodels[MAX_BSP_MODELS];

static int numbrushes;
static cbrush_t map_brushes[MAX_BSP_BRUSHES];

static int num_visibility;
static byte map_visibility[MAX_BSP_VISIBILITY];
static d_bsp_vis_t *map_vis = (d_bsp_vis_t *)map_visibility;

int num_entity_chars;
char map_entitystring[MAX_BSP_ENTSTRING];

static int numareas = 1;
static carea_t map_areas[MAX_BSP_AREAS];

int num_area_portals;
d_bsp_area_portal_t map_areaportals[MAX_BSP_AREAPORTALS];

static int num_clusters = 1;

static csurface_t nullsurface;

static int floodvalid;

static qboolean portalopen[MAX_BSP_AREAPORTALS];

static cvar_t *map_noareas;

static void Cm_InitBoxHull(void);
static void Cm_FloodAreaConnections(void);


int c_pointcontents;
int c_traces, c_brush_traces;


/*
 *
 * MAP LOADING
 *
 */

static const byte *cmod_base;

/*
 * Cm_LoadSubmodels
 */
static void Cm_LoadSubmodels(const d_bsp_lump_t *l){
	const d_bsp_model_t *in;
	int i, j, count;

	in = (const void *)(cmod_base + l->file_ofs);
	if(l->file_len % sizeof(*in)){
		Com_Error(ERR_DROP, "Cm_LoadSubmodels: Funny lump size.\n");
	}
	count = l->file_len / sizeof(*in);

	if(count < 1){
		Com_Error(ERR_DROP, "Cm_LoadSubmodels: Map with no models.\n");
	}
	if(count > MAX_BSP_MODELS){
		Com_Error(ERR_DROP, "Cm_LoadSubmodels: Map has too many models.\n");
	}

	num_cmodels = count;

	for(i = 0; i < count; i++, in++){
		cmodel_t *out = &map_cmodels[i];

		for(j = 0; j < 3; j++){  // spread the mins / maxs by a pixel
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
static void Cm_LoadSurfaces(const d_bsp_lump_t *l){
	const d_bsp_texinfo_t *in;
	csurface_t *out;
	int i, count;

	in = (const void *)(cmod_base + l->file_ofs);
	if(l->file_len % sizeof(*in)){
		Com_Error(ERR_DROP, "Cm_LoadSurfaces: Funny lump size.\n");
		return;
	}
	count = l->file_len / sizeof(*in);
	if(count < 1){
		Com_Error(ERR_DROP, "Cm_LoadSurfaces: Map with no surfaces.\n");
	}
	if(count > MAX_BSP_TEXINFO){
		Com_Error(ERR_DROP, "Cm_LoadSurfaces: Map has too many surfaces.\n");
	}
	num_texinfo = count;
	out = csurfaces;

	for(i = 0; i < count; i++, in++, out++){
		strncpy(out->name, in->texture, sizeof(out->name) - 1);
		out->flags = LittleLong(in->flags);
		out->value = LittleLong(in->value);
	}
}


/*
 * Cm_LoadNodes
 */
static void Cm_LoadNodes(const d_bsp_lump_t *l){
	const d_bsp_node_t *in;
	int child;
	cnode_t *out;
	int i, j, count;

	in = (const void *)(cmod_base + l->file_ofs);
	if(l->file_len % sizeof(*in)){
		Com_Error(ERR_DROP, "Cm_LoadNodes: Funny lump size.\n");
	}
	count = l->file_len / sizeof(*in);

	if(count < 1){
		Com_Error(ERR_DROP, "Cm_LoadNodes: Map has no nodes.\n");
	}
	if(count > MAX_BSP_NODES){
		Com_Error(ERR_DROP, "Cm_LoadNodes: Map has too many nodes.\n");
	}

	out = map_nodes;

	num_nodes = count;

	for(i = 0; i < count; i++, out++, in++){
		out->plane = map_planes + LittleLong(in->plane_num);
		for(j = 0; j < 2; j++){
			child = LittleLong(in->children[j]);
			out->children[j] = child;
		}
	}
}


/*
 * Cm_LoadBrushes
 */
static void Cm_LoadBrushes(const d_bsp_lump_t *l){
	const d_bsp_brush_t *in;
	cbrush_t *out;
	int i, count;

	in = (const void *)(cmod_base + l->file_ofs);
	if(l->file_len % sizeof(*in)){
		Com_Error(ERR_DROP, "Cm_LoadBrushes: Funny lump size.\n");
	}
	count = l->file_len / sizeof(*in);

	if(count > MAX_BSP_BRUSHES){
		Com_Error(ERR_DROP, "Cm_LoadBrushes: Map has too many brushes.\n");
	}

	out = map_brushes;

	numbrushes = count;

	for(i = 0; i < count; i++, out++, in++){
		out->firstbrushside = LittleLong(in->firstside);
		out->numsides = LittleLong(in->numsides);
		out->contents = LittleLong(in->contents);
	}
}


/*
 * Cm_LoadLeafs
 */
static void Cm_LoadLeafs(const d_bsp_lump_t *l){
	int i;
	cleaf_t *out;
	const d_bsp_leaf_t *in;
	int count;

	in = (const void *)(cmod_base + l->file_ofs);
	if(l->file_len % sizeof(*in)){
		Com_Error(ERR_DROP, "Cm_LoadLeafs: Funny lump size.\n");
	}
	count = l->file_len / sizeof(*in);

	if(count < 1){
		Com_Error(ERR_DROP, "Cm_LoadLeafs: Map with no leafs.\n");
	}
	// need to save space for box planes
	if(count > MAX_BSP_PLANES){
		Com_Error(ERR_DROP, "Cm_LoadLeafs: Map has too many planes.\n");
	}

	out = map_leafs;
	num_leafs = count;
	num_clusters = 0;

	for(i = 0; i < count; i++, in++, out++){
		out->contents = LittleLong(in->contents);
		out->cluster = LittleShort(in->cluster);
		out->area = LittleShort(in->area);
		out->first_leaf_brush = LittleShort(in->first_leaf_brush);
		out->num_leaf_brushes = LittleShort(in->num_leaf_brushes);

		if(out->cluster >= num_clusters)
			num_clusters = out->cluster + 1;
	}

	if(map_leafs[0].contents != CONTENTS_SOLID){
		Com_Error(ERR_DROP, "Cm_LoadLeafs: Map leaf 0 is not CONTENTS_SOLID.\n");
	}
	solidleaf = 0;
	emptyleaf = -1;
	for(i = 1; i < num_leafs; i++){
		if(!map_leafs[i].contents){
			emptyleaf = i;
			break;
		}
	}
	if(emptyleaf == -1)
		Com_Error(ERR_DROP, "Cm_LoadLeafs: Map does not have an empty leaf.\n");
}


/*
 * Cm_LoadPlanes
 */
static void Cm_LoadPlanes(const d_bsp_lump_t *l){
	int i, j;
	cplane_t *out;
	const d_bsp_plane_t *in;
	int count;

	in = (const void *)(cmod_base + l->file_ofs);
	if(l->file_len % sizeof(*in)){
		Com_Error(ERR_DROP, "Cm_LoadPlanes: Funny lump size.\n");
	}
	count = l->file_len / sizeof(*in);

	if(count < 1){
		Com_Error(ERR_DROP, "Cm_LoadPlanes: Map with no planes.\n");
	}
	// need to save space for box planes
	if(count > MAX_BSP_PLANES){
		Com_Error(ERR_DROP, "Cm_LoadPlanes: Map has too many planes.\n");
	}

	out = map_planes;
	num_planes = count;

	for(i = 0; i < count; i++, in++, out++){
		int bits = 0;
		for(j = 0; j < 3; j++){
			out->normal[j] = LittleFloat(in->normal[j]);
			if(out->normal[j] < 0)
				bits |= 1 << j;
		}

		out->dist = LittleFloat(in->dist);
		out->type = LittleLong(in->type);
		out->sign_bits = bits;
	}
}


/*
 * Cm_LoadLeafBrushes
 */
static void Cm_LoadLeafBrushes(const d_bsp_lump_t *l){
	int i;
	unsigned short *out;
	const unsigned short *in;
	int count;

	in = (const void *)(cmod_base + l->file_ofs);
	if(l->file_len % sizeof(*in)){
		Com_Error(ERR_DROP, "Cm_LoadLeafBrushes: Funny lump size.\n");
	}
	count = l->file_len / sizeof(*in);

	if(count < 1){
		Com_Error(ERR_DROP, "Cm_LoadLeafBrushes: Map with no planes.\n");
	}
	// need to save space for box planes
	if(count > MAX_BSP_LEAFBRUSHES){
		Com_Error(ERR_DROP, "Cm_LoadLeafBrushes: Map has too many leafbrushes.\n");
	}

	out = map_leafbrushes;
	num_leaf_brushes = count;

	for(i = 0; i < count; i++, in++, out++)
		*out = LittleShort(*in);
}


/*
 * Cm_LoadBrushSides
 */
static void Cm_LoadBrushSides(const d_bsp_lump_t *l){
	int i;
	cbrushside_t *out;
	const d_bsp_brush_side_t *in;
	int count;
	int num;

	in = (const void *)(cmod_base + l->file_ofs);
	if(l->file_len % sizeof(*in)){
		Com_Error(ERR_DROP, "Cm_LoadBrushSides: Funny lump size.\n");
	}
	count = l->file_len / sizeof(*in);

	// need to save space for box planes
	if(count > MAX_BSP_BRUSHSIDES){
		Com_Error(ERR_DROP, "Cm_LoadBrushSides: Map has too many planes.\n");
	}

	out = map_brushsides;
	numbrushsides = count;

	for(i = 0; i < count; i++, in++, out++){
		num = LittleShort(in->plane_num);
		out->plane = &map_planes[num];
		num = LittleShort(in->surf_num);
		if(num >= num_texinfo){
			Com_Error(ERR_DROP, "Cm_LoadBrushSides: Bad brushside surf_num.\n");
		}
		out->surface = &csurfaces[num];
	}
}


/*
 * Cm_LoadAreas
 */
static void Cm_LoadAreas(const d_bsp_lump_t *l){
	int i;
	carea_t *out;
	const d_bsp_area_t *in;
	int count;

	in = (const void *)(cmod_base + l->file_ofs);
	if(l->file_len % sizeof(*in)){
		Com_Error(ERR_DROP, "MOD_LoadAreas: Funny lump size.\n");
	}
	count = l->file_len / sizeof(*in);

	if(count > MAX_BSP_AREAS){
		Com_Error(ERR_DROP, "Cm_LoadAreas: Map has too many areas.\n");
	}

	out = map_areas;
	numareas = count;

	for(i = 0; i < count; i++, in++, out++){
		out->num_area_portals = LittleLong(in->num_area_portals);
		out->first_area_portal = LittleLong(in->first_area_portal);
		out->floodvalid = 0;
		out->floodnum = 0;
	}
}


/*
 * Cm_LoadAreaPortals
 */
static void Cm_LoadAreaPortals(const d_bsp_lump_t *l){
	int i;
	d_bsp_area_portal_t *out;
	const d_bsp_area_portal_t *in;
	int count;

	in = (const void *)(cmod_base + l->file_ofs);
	if(l->file_len % sizeof(*in)){
		Com_Error(ERR_DROP, "Cm_LoadAreaPortals: Funny lump size.\n");
	}
	count = l->file_len / sizeof(*in);

	if(count > MAX_BSP_AREAS){
		Com_Error(ERR_DROP, "Cm_LoadAreaPortals: Map has too many areas.\n");
	}

	out = map_areaportals;
	num_area_portals = count;

	for(i = 0; i < count; i++, in++, out++){
		out->portal_num = LittleLong(in->portal_num);
		out->other_area = LittleLong(in->other_area);
	}
}


/*
 * Cm_LoadVisibility
 */
static void Cm_LoadVisibility(const d_bsp_lump_t *l){
	int i;

	num_visibility = l->file_len;
	if(l->file_len > MAX_BSP_VISIBILITY){
		Com_Error(ERR_DROP, "Cm_LOadVisibility: Map has too large visibility lump.\n");
	}

	memcpy(map_visibility, cmod_base + l->file_ofs, l->file_len);

	map_vis->num_clusters = LittleLong(map_vis->num_clusters);
	for(i = 0; i < map_vis->num_clusters; i++){
		map_vis->bit_ofs[i][0] = LittleLong(map_vis->bit_ofs[i][0]);
		map_vis->bit_ofs[i][1] = LittleLong(map_vis->bit_ofs[i][1]);
	}
}


/*
 * Cm_LoadEntityString
 */
static void Cm_LoadEntityString(const d_bsp_lump_t *l){

	num_entity_chars = l->file_len;

	if(l->file_len > MAX_BSP_ENTSTRING){
		Com_Error(ERR_DROP, "Cm_LoadEntityString: Map has too large entity lump.\n");
	}

	memcpy(map_entitystring, cmod_base + l->file_ofs, l->file_len);
}


/*
 * Cm_LoadMap
 *
 * Loads in the map and all submodels
 */
cmodel_t *Cm_LoadMap(const char *name, int *mapsize){
	void *buf;
	int i;
	d_bsp_header_t header;

	map_noareas = Cvar_Get("map_noareas", "0", 0, NULL);

	// free old stuff
	num_planes = 0;
	num_nodes = 0;
	num_leafs = 0;
	num_cmodels = 0;
	num_visibility = 0;
	num_entity_chars = 0;
	map_entitystring[0] = 0;
	map_name[0] = 0;

	// if we've been asked to load a demo, just clean up and return
	if(!name){
		num_leafs = num_clusters = numareas = 1;
		*mapsize = 0;
		return &map_cmodels[0];
	}

	// load the file
	*mapsize = Fs_LoadFile(name, &buf);
	if(!buf){
		Com_Error(ERR_DROP, "Cm_LoadMap: Couldn't load %s.\n", name);
	}

	header = *(d_bsp_header_t *)buf;
	for(i = 0; i < sizeof(d_bsp_header_t) / 4; i++)
		((int *)&header)[i] = LittleLong(((int *)&header)[i]);

	if(header.version != BSP_VERSION && header.version != BSP_VERSION_){
		Fs_FreeFile(buf);
		Com_Error(ERR_DROP, "Cm_LoadMap: %s has unsupported version: %d.\n",
				name, header.version);
	}

	cmod_base = (const byte *)buf;

	// load into heap
	Cm_LoadSurfaces(&header.lumps[LUMP_TEXINFO]);
	Cm_LoadLeafs(&header.lumps[LUMP_LEAFS]);
	Cm_LoadLeafBrushes(&header.lumps[LUMP_LEAFBRUSHES]);
	Cm_LoadPlanes(&header.lumps[LUMP_PLANES]);
	Cm_LoadBrushes(&header.lumps[LUMP_BRUSHES]);
	Cm_LoadBrushSides(&header.lumps[LUMP_BRUSHSIDES]);
	Cm_LoadSubmodels(&header.lumps[LUMP_MODELS]);
	Cm_LoadNodes(&header.lumps[LUMP_NODES]);
	Cm_LoadAreas(&header.lumps[LUMP_AREAS]);
	Cm_LoadAreaPortals(&header.lumps[LUMP_AREAPORTALS]);
	Cm_LoadVisibility(&header.lumps[LUMP_VISIBILITY]);
	Cm_LoadEntityString(&header.lumps[LUMP_ENTITIES]);

	Fs_FreeFile(buf);

	Cm_InitBoxHull();

	memset(portalopen, 0, sizeof(portalopen));
	Cm_FloodAreaConnections();

	strcpy(map_name, name);

	return &map_cmodels[0];
}


/*
 * Cm_InlineModel
 */
cmodel_t *Cm_InlineModel(const char *name){
	int num;

	if(!name || name[0] != '*'){
		Com_Error(ERR_DROP, "Cm_InlineModel: Bad name.\n");
	}
	num = atoi(name + 1);
	if(num < 1 || num >= num_cmodels){
		Com_Error(ERR_DROP, "Cm_InlineModel: Bad number: %d\n.", num);
	}

	return &map_cmodels[num];
}

int Cm_NumClusters(void){
	return num_clusters;
}

int Cm_NumInlineModels(void){
	return num_cmodels;
}

char *Cm_EntityString(void){
	return map_entitystring;
}

int Cm_LeafContents(int leaf_num){
	if(leaf_num < 0 || leaf_num >= num_leafs){
		Com_Error(ERR_DROP, "Cm_LeafContents: Bad number.\n");
	}
	return map_leafs[leaf_num].contents;
}

int Cm_LeafCluster(int leaf_num){
	if(leaf_num < 0 || leaf_num >= num_leafs){
		Com_Error(ERR_DROP, "Cm_LeafCluster: Bad number.\n");
	}
	return map_leafs[leaf_num].cluster;
}

int Cm_LeafArea(int leaf_num){
	if(leaf_num < 0 || leaf_num >= num_leafs){
		Com_Error(ERR_DROP, "Cm_LeafArea: Bad number.\n");
	}
	return map_leafs[leaf_num].area;
}


static cplane_t *box_planes;
static int box_head_node;
static cbrush_t *box_brush;
static cleaf_t *box_leaf;

/*
 * Cm_InitBoxHull
 *
 * Set up the planes and nodes so that the six floats of a bounding box
 * can just be stored out and get a proper clipping hull structure.
 */
static void Cm_InitBoxHull(void){
	int i;

	box_head_node = num_nodes;
	box_planes = &map_planes[num_planes];
	if(num_nodes + 6 > MAX_BSP_NODES
			|| numbrushes + 1 > MAX_BSP_BRUSHES
			|| num_leaf_brushes + 1 > MAX_BSP_LEAFBRUSHES
			|| numbrushsides + 6 > MAX_BSP_BRUSHSIDES
			|| num_planes + 12 > MAX_BSP_PLANES){
		Com_Error(ERR_DROP, "Cm_InitBoxHull: Not enough room for box tree.\n");
	}

	box_brush = &map_brushes[numbrushes];
	box_brush->numsides = 6;
	box_brush->firstbrushside = numbrushsides;
	box_brush->contents = CONTENTS_MONSTER;

	box_leaf = &map_leafs[num_leafs];
	box_leaf->contents = CONTENTS_MONSTER;
	box_leaf->first_leaf_brush = num_leaf_brushes;
	box_leaf->num_leaf_brushes = 1;

	map_leafbrushes[num_leaf_brushes] = numbrushes;

	for(i = 0; i < 6; i++){
		const int side = i & 1;
		cnode_t *c;
		cplane_t *p;
		cbrushside_t *s;

		// brush sides
		s = &map_brushsides[numbrushsides + i];
		s->plane = map_planes + (num_planes + i * 2 + side);
		s->surface = &nullsurface;

		// nodes
		c = &map_nodes[box_head_node + i];
		c->plane = map_planes + (num_planes + i * 2);
		c->children[side] = -1 - emptyleaf;
		if(i != 5)
			c->children[side ^ 1] = box_head_node + i + 1;
		else
			c->children[side ^ 1] = -1 - num_leafs;

		// planes
		p = &box_planes[i * 2];
		p->type = i >> 1;
		p->sign_bits = 0;
		VectorClear(p->normal);
		p->normal[i >> 1] = 1;

		p = &box_planes[i * 2 + 1];
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
int Cm_HeadnodeForBox(const vec3_t mins, const vec3_t maxs){
	box_planes[0].dist = maxs[0];
	box_planes[1].dist = -maxs[0];
	box_planes[2].dist = mins[0];
	box_planes[3].dist = -mins[0];
	box_planes[4].dist = maxs[1];
	box_planes[5].dist = -maxs[1];
	box_planes[6].dist = mins[1];
	box_planes[7].dist = -mins[1];
	box_planes[8].dist = maxs[2];
	box_planes[9].dist = -maxs[2];
	box_planes[10].dist = mins[2];
	box_planes[11].dist = -mins[2];

	return box_head_node;
}


/*
 * Cm_PointLeafnum
 */
static int Cm_PointLeafnum_r(const vec3_t p, int num){

	while(num >= 0){
		float d;
		cnode_t *node = map_nodes + num;
		cplane_t *plane = node->plane;

		if(AXIAL(plane))
			d = p[plane->type] - plane->dist;
		else
			d = DotProduct(plane->normal, p) - plane->dist;

		if(d < 0)
			num = node->children[1];
		else
			num = node->children[0];
	}

	c_pointcontents++;  // optimize counter, TOOD: atomic increment for thread-safety

	return -1 - num;
}

int Cm_PointLeafnum(const vec3_t p){
	if(!num_planes)
		return 0;  // sound may call this without map loaded
	return Cm_PointLeafnum_r(p, 0);
}


/*
 * Cm_BoxLeafnums
 *
 * Fills in a list of all the leafs touched
 */
 
typedef struct { 
	int count, maxcount;
	int *list;
	const float *mins, *maxs;
	int topnode;
} leafdata_t;

static void Cm_BoxLeafnums_r(int nodenum, leafdata_t *data) {
	cplane_t *plane;
	cnode_t *node;
	int s;

	while(true){
		if(nodenum < 0){
			if(data->count >= data->maxcount){
				return;
			}
			data->list[data->count++] = -1 - nodenum;
			return;
		}

		node = &map_nodes[nodenum];
		plane = node->plane;
		s = BOX_ON_PLANE_SIDE(data->mins, data->maxs, plane);
		if(s == 1)
			nodenum = node->children[0];
		else if(s == 2)
			nodenum = node->children[1];
		else {  // go down both
			if(data->topnode == -1)
				data->topnode = nodenum;
			Cm_BoxLeafnums_r(node->children[0], data);
			nodenum = node->children[1];
		}
	}
}

static int Cm_BoxLeafnums_head_node(const vec3_t mins, const vec3_t maxs, int *list, int listsize, int head_node, int *topnode){
	leafdata_t data;
	data.list = list;
	data.count = 0;
	data.maxcount = listsize;
	data.mins = mins;
	data.maxs = maxs;

	data.topnode = -1;

	Cm_BoxLeafnums_r(head_node, &data);

	if(topnode)
		*topnode = data.topnode;

	return data.count;
}

int Cm_BoxLeafnums(const vec3_t mins, const vec3_t maxs, int *list, int listsize, int *topnode){
	return Cm_BoxLeafnums_head_node(mins, maxs, list, listsize, map_cmodels[0].head_node, topnode);
}


/*
 * Cm_PointContents
 */
int Cm_PointContents(const vec3_t p, int head_node){
	int l;

	if(!num_nodes)  // map not loaded
		return 0;

	l = Cm_PointLeafnum_r(p, head_node);

	return map_leafs[l].contents;
}


/*
 * Cm_TransformedPointContents
 *
 * Handles offseting and rotation of the end points for moving and
 * rotating entities
 */
int Cm_TransformedPointContents(const vec3_t p, int head_node, const vec3_t origin, const vec3_t angles){
	vec3_t p_l;
	vec3_t temp;
	vec3_t forward, right, up;
	int l;

	// subtract origin offset
	VectorSubtract(p, origin, p_l);

	// rotate start and end into the models frame of reference
	if(head_node != box_head_node &&
			(angles[0] || angles[1] || angles[2])){
		AngleVectors(angles, forward, right, up);

		VectorCopy(p_l, temp);
		p_l[0] = DotProduct(temp, forward);
		p_l[1] = -DotProduct(temp, right);
		p_l[2] = DotProduct(temp, up);
	}

	l = Cm_PointLeafnum_r(p_l, head_node);

	return map_leafs[l].contents;
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

	trace_t trace;
	int contents;
	qboolean ispoint;  // optimized case
	
	int mailbox[16]; // used to avoid multiple intersection tests with brushes
} tracedata_t;


/*
 * Cm_BrushAlreadyTested
 */
static qboolean Cm_BrushAlreadyTested(int brushnum, tracedata_t *data) {
	int hash = brushnum & 15;
	qboolean skip = (data->mailbox[hash] == brushnum);
	data->mailbox[hash] = brushnum;
	return skip;
}

/*
 * Cm_ClipBoxToBrush
 *
 * Clips the bounded box to all brush sides for the given brush.  Returns
 * true if the box was clipped, false otherwise.
 */
static void Cm_ClipBoxToBrush(vec3_t mins, vec3_t maxs, vec3_t p1, vec3_t p2,
			trace_t *trace, cleaf_t *leaf, cbrush_t *brush, qboolean ispoint){
	int i, j;
	cplane_t *clipplane;
	float dist;
	float enterfrac, leavefrac;
	vec3_t ofs;
	float d1, d2;
	qboolean getout, startout;
	cbrushside_t *leadside;

	enterfrac = -1;
	leavefrac = 1;
	clipplane = NULL;

	if(!brush->numsides)
		return;

	c_brush_traces++;

	getout = startout = false;
	leadside = NULL;

	for(i = 0; i < brush->numsides; i++){
		cbrushside_t *side = &map_brushsides[brush->firstbrushside + i];
		cplane_t *plane = side->plane;

		// FIXME: special case for axial

		if(!ispoint){  // general box case

			// push the plane out apropriately for mins/maxs

			// FIXME: use sign_bits into 8 way lookup for each mins/maxs
			for(j = 0; j < 3; j++){
				if(plane->normal[j] < 0.0)
					ofs[j] = maxs[j];
				else
					ofs[j] = mins[j];
			}
			dist = DotProduct(ofs, plane->normal);
			dist = plane->dist - dist;
		} else {  // special point case
			dist = plane->dist;
		}

		d1 = DotProduct(p1, plane->normal) - dist;
		d2 = DotProduct(p2, plane->normal) - dist;

		if(d2 > 0.0)
			getout = true;  // endpoint is not in solid
		if(d1 > 0.0)
			startout = true;

		// if completely in front of face, no intersection
		if(d1 > 0.0 && d2 >= d1)
			return;

		if(d1 <= 0.0 && d2 <= 0.0)
			continue;

		// crosses face
		if(d1 > d2){  // enter
			const float f = (d1 - DIST_EPSILON) / (d1 - d2);
			if(f > enterfrac){
				enterfrac = f;
				clipplane = plane;
				leadside = side;
			}
		} else {  // leave
			const float f = (d1 + DIST_EPSILON) / (d1 - d2);
			if(f < leavefrac)
				leavefrac = f;
		}
	}

	if(!startout){  // original point was inside brush
		trace->start_solid = true;
		if(!getout)
			trace->all_solid = true;
		trace->leaf_num = leaf - map_leafs;
	}

	if(enterfrac < leavefrac){  // pierced brush
		if(enterfrac > -1.0 && enterfrac < trace->fraction){
			if(enterfrac < 0.0)
				enterfrac = 0.0;
			trace->fraction = enterfrac;
			trace->plane = *clipplane;
			trace->surface = leadside->surface;
			trace->contents = brush->contents;
			trace->leaf_num = leaf - map_leafs;
		}
	}
}


/*
 * Cm_TestBoxInBrush
 */
static void Cm_TestBoxInBrush(vec3_t mins, vec3_t maxs, vec3_t p1, trace_t *trace, cbrush_t *brush){
	int i, j;
	cplane_t *plane;
	float dist;
	vec3_t ofs;
	float d1;
	cbrushside_t *side;

	if(!brush->numsides)
		return;

	for(i = 0; i < brush->numsides; i++){
		side = &map_brushsides[brush->firstbrushside + i];
		plane = side->plane;

		// FIXME: special case for axial

		// general box case

		// push the plane out apropriately for mins/maxs

		// FIXME: use sign_bits into 8 way lookup for each mins/maxs
		for(j = 0; j < 3; j++){
			if(plane->normal[j] < 0.0)
				ofs[j] = maxs[j];
			else
				ofs[j] = mins[j];
		}
		dist = DotProduct(ofs, plane->normal);
		dist = plane->dist - dist;

		d1 = DotProduct(p1, plane->normal) - dist;

		// if completely in front of face, no intersection
		if(d1 > 0.0)
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
static void Cm_TraceToLeaf(int leaf_num, tracedata_t *data){
	int k;
	cleaf_t *leaf;

	leaf = &map_leafs[leaf_num];

	if(!(leaf->contents & data->contents))
		return;

	// trace line against all brushes in the leaf
	for(k = 0; k < leaf->num_leaf_brushes; k++){
		const int brushnum = map_leafbrushes[leaf->first_leaf_brush + k];
		cbrush_t *b = &map_brushes[brushnum];

		if(Cm_BrushAlreadyTested(brushnum, data))
			continue; // already checked this brush in another leaf

		if(!(b->contents & data->contents))
			continue;

		Cm_ClipBoxToBrush(data->mins, data->maxs, data->start, data->end,
				&data->trace, leaf, b, data->ispoint);

		if(data->trace.all_solid)
			return;
	}
}


/*
 * Cm_TestInLeaf
 */
static void Cm_TestInLeaf(int leaf_num, tracedata_t *data) {
	int k;
	cleaf_t *leaf;

	leaf = &map_leafs[leaf_num];
	if(!(leaf->contents & data->contents))
		return;

	// trace line against all brushes in the leaf
	for(k = 0; k < leaf->num_leaf_brushes; k++){
		const int brushnum = map_leafbrushes[leaf->first_leaf_brush + k];
		cbrush_t *b = &map_brushes[brushnum];
		if(Cm_BrushAlreadyTested(brushnum, data))
			continue; // already checked this brush in another leaf

		if(!(b->contents & data->contents))
			continue;
		Cm_TestBoxInBrush(data->mins, data->maxs, data->start, &data->trace, b);
		if(data->trace.all_solid)
			return;
	}
}


/*
 * Cm_RecursiveHullCheck
 */
static void Cm_RecursiveHullCheck(int num, float p1f, float p2f, const vec3_t p1, const vec3_t p2, tracedata_t *data) {
	const cnode_t *node;
	const cplane_t *plane;
	float t1, t2, offset;
	float frac, frac2;
	int i;
	vec3_t mid;
	int side;
	float midf;

	if(data->trace.fraction <= p1f)
		return;  // already hit something nearer

	// if < 0, we are in a leaf node
	if(num < 0){
		Cm_TraceToLeaf(-1 - num, data);
		return;
	}

	// find the point distances to the seperating plane
	// and the offset for the size of the box
	node = map_nodes + num;
	plane = node->plane;

	if(AXIAL(plane)){
		t1 = p1[plane->type] - plane->dist;
		t2 = p2[plane->type] - plane->dist;
		offset = data->extents[plane->type];
	} else {
		t1 = DotProduct(plane->normal, p1) - plane->dist;
		t2 = DotProduct(plane->normal, p2) - plane->dist;
		if(data->ispoint)
			offset = 0;
		else
			offset = fabsf(data->extents[0] * plane->normal[0]) +
					 fabsf(data->extents[1] * plane->normal[1]) +
					 fabsf(data->extents[2] * plane->normal[2]);
	}

	// see which sides we need to consider
	if(t1 >= offset && t2 >= offset){
		Cm_RecursiveHullCheck(node->children[0], p1f, p2f, p1, p2, data);
		return;
	}
	if(t1 <= -offset && t2 <= -offset){
		Cm_RecursiveHullCheck(node->children[1], p1f, p2f, p1, p2, data);
		return;
	}

	// put the crosspoint DIST_EPSILON pixels on the near side
	if(t1 < t2){
		const float idist = 1.0 / (t1 - t2);
		side = 1;
		frac2 = (t1 + offset + DIST_EPSILON) * idist;
		frac = (t1 - offset + DIST_EPSILON) * idist;
	} else if(t1 > t2){
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
	if(frac < 0)
		frac = 0;
	if(frac > 1)
		frac = 1;

	midf = p1f + (p2f - p1f) * frac;
	for(i = 0; i < 3; i++)
		mid[i] = p1[i] + frac * (p2[i] - p1[i]);

	Cm_RecursiveHullCheck(node->children[side], p1f, midf, p1, mid, data);

	// go past the node
	if(frac2 < 0)
		frac2 = 0;
	if(frac2 > 1)
		frac2 = 1;

	midf = p1f + (p2f - p1f) * frac2;
	for(i = 0; i < 3; i++)
		mid[i] = p1[i] + frac2 * (p2[i] - p1[i]);

	Cm_RecursiveHullCheck(node->children[side ^ 1], midf, p2f, mid, p2, data);
}

/*
 * Cm_BoxTrace
 */
trace_t Cm_BoxTrace(const vec3_t start, const vec3_t end,
			const vec3_t mins, const vec3_t maxs, int head_node, int brushmask){
	int i, pointleafs[1024];

	tracedata_t data;

	c_traces++;  // for statistics

	// fill in a default trace
	memset(&data.trace, 0, sizeof(data.trace));
	data.trace.fraction = 1.0;
	data.trace.surface = &nullsurface;

	if(!num_nodes)  // map not loaded
		return data.trace;

	memset(&data.mailbox, 0xffffffff, sizeof(data.mailbox));
	data.contents = brushmask;
	VectorCopy(start, data.start);
	VectorCopy(end, data.end);
	VectorCopy(mins, data.mins);
	VectorCopy(maxs, data.maxs);

	// check for position test special case
	if(VectorCompare(start, end)){
		int i, num_leafs;
		vec3_t c1, c2;
		int topnode;

		VectorAdd(start, mins, c1);
		VectorAdd(start, maxs, c2);
		for(i = 0; i < 3; i++){
			c1[i] -= 1.0;
			c2[i] += 1.0;
		}

		num_leafs = Cm_BoxLeafnums_head_node(c1, c2, pointleafs,
				sizeof(pointleafs) / sizeof(int), head_node, &topnode); // NOTE: was * sizeof(int)

		for(i = 0; i < num_leafs; i++){
			Cm_TestInLeaf(pointleafs[i], &data);
			if(data.trace.all_solid)
				break;
		}
		VectorCopy(start, data.trace.end);
		return data.trace;
	}

	// check for point special case
	if(VectorCompare(mins, vec3_origin) && VectorCompare(maxs, vec3_origin)){
		data.ispoint = true;
		VectorClear(data.extents);
	} else {
		data.ispoint = false;
		data.extents[0] = -mins[0] > maxs[0] ? -mins[0] : maxs[0];
		data.extents[1] = -mins[1] > maxs[1] ? -mins[1] : maxs[1];
		data.extents[2] = -mins[2] > maxs[2] ? -mins[2] : maxs[2];
	}

	// general sweeping through world
	Cm_RecursiveHullCheck(head_node, 0, 1, start, end, &data);

	if(data.trace.fraction == 1){
		VectorCopy(end, data.trace.end);
	} else {
		for(i = 0; i < 3; i++)
			data.trace.end[i] = start[i] + data.trace.fraction * (end[i] - start[i]);
	}
	return data.trace;
}


/*
 * Cm_TransformedBoxTrace
 *
 * Handles offseting and rotation of the end points for moving and
 * rotating entities
 */
trace_t Cm_TransformedBoxTrace(const vec3_t start, const vec3_t end,
		const vec3_t mins, const vec3_t maxs, int head_node, int brushmask,
		const vec3_t origin, const vec3_t angles){

	trace_t trace;
	vec3_t start_l, end_l;
	vec3_t a;
	vec3_t forward, right, up;
	vec3_t temp;
	qboolean rotated;

	// subtract origin offset
	VectorSubtract(start, origin, start_l);
	VectorSubtract(end, origin, end_l);

	// rotate start and end into the models frame of reference
	if(head_node != box_head_node &&
			(angles[0] || angles[1] || angles[2]))
		rotated = true;
	else
		rotated = false;

	if(rotated){
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
	trace = Cm_BoxTrace(start_l, end_l, mins, maxs, head_node, brushmask);

	if(rotated && trace.fraction != 1.0){
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
static void Cm_DecompressVis(const byte *in, byte *out){
	int c;
	byte *out_p;
	int row;

	row = (num_clusters + 7) >> 3;
	out_p = out;

	if(!in || !num_visibility){  // no vis info, so make all visible
		while(row){
			*out_p++ = 0xff;
			row--;
		}
		return;
	}

	do {
		if(*in){
			*out_p++ = *in++;
			continue;
		}

		c = in[1];
		in += 2;
		if((out_p - out) + c > row){
			c = row -(out_p - out);
			Com_Warn("Cm_DecompressVis: Overrun.\n");
		}
		while(c){
			*out_p++ = 0;
			c--;
		}
	} while(out_p - out < row);
}

static byte pvsrow[MAX_BSP_LEAFS / 8];
static byte phsrow[MAX_BSP_LEAFS / 8];

byte *Cm_ClusterPVS(int cluster){
	if(cluster == -1)
		memset(pvsrow, 0, (num_clusters + 7) >> 3);
	else
		Cm_DecompressVis(map_visibility + map_vis->bit_ofs[cluster][DVIS_PVS], pvsrow);
	return pvsrow;
}

byte *Cm_ClusterPHS(int cluster){
	if(cluster == -1)
		memset(phsrow, 0, (num_clusters + 7) >> 3);
	else
		Cm_DecompressVis(map_visibility + map_vis->bit_ofs[cluster][DVIS_PHS], phsrow);
	return phsrow;
}


/*
 *
 * AREAPORTALS
 *
 */

static void Cm_FloodArea(carea_t *area, int floodnum){
	int i;
	const d_bsp_area_portal_t *p;

	if(area->floodvalid == floodvalid){
		if(area->floodnum == floodnum)
			return;
		Com_Error(ERR_DROP, "Cm_FloodArea: Reflooded.\n");
	}

	area->floodnum = floodnum;
	area->floodvalid = floodvalid;
	p = &map_areaportals[area->first_area_portal];
	for(i = 0; i < area->num_area_portals; i++, p++){
		if(portalopen[p->portal_num])
			Cm_FloodArea(&map_areas[p->other_area], floodnum);
	}
}


/*
 * Cm_FloodAreaConnections
 */
static void Cm_FloodAreaConnections(void){
	int i;
	int floodnum;

	// all current floods are now invalid
	floodvalid++;
	floodnum = 0;

	// area 0 is not used
	for(i = 1; i < numareas; i++){
		carea_t *area = &map_areas[i];
		if(area->floodvalid == floodvalid)
			continue;  // already flooded into
		floodnum++;
		Cm_FloodArea(area, floodnum);
	}
}

void Cm_SetAreaPortalState(int portal_num, qboolean open){
	if(portal_num > num_area_portals){
		Com_Error(ERR_DROP, "Cm_SetAreaPortalState: areaportal > num_area_portals.\n");
	}

	portalopen[portal_num] = open;
	Cm_FloodAreaConnections();
}

qboolean Cm_AreasConnected(int area1, int area2){
	if(map_noareas->value)
		return true;

	if(area1 > numareas || area2 > numareas){
		Com_Error(ERR_DROP, "Cm_AreasConnected: area > numareas.\n");
	}

	if(map_areas[area1].floodnum == map_areas[area2].floodnum)
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
int Cm_WriteAreaBits(byte *buffer, int area){
	int i;
	const int bytes = (numareas + 7) >> 3;

	if(map_noareas->value){  // for debugging, send everything
		memset(buffer, 255, bytes);
	} else {
		const int floodnum = map_areas[area].floodnum;
		memset(buffer, 0, bytes);

		for(i = 0; i < numareas; i++){
			if(map_areas[i].floodnum == floodnum || !area)
				buffer[i >> 3] |= 1 <<(i & 7);
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
qboolean Cm_HeadnodeVisible(int nodenum, byte *visbits){
	const cnode_t *node;

	if(nodenum < 0){  // at a leaf, check it
		const int leaf_num = -1 - nodenum;
		const int cluster = map_leafs[leaf_num].cluster;
		if(cluster == -1)
			return false;
		if(visbits[cluster >> 3] & (1 << (cluster & 7)))
			return true;
		return false;
	}

	node = &map_nodes[nodenum];

	if(Cm_HeadnodeVisible(node->children[0], visbits))
		return true;

	return Cm_HeadnodeVisible(node->children[1], visbits);
}
