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

#include "bspfile.h"
#include "scriplib.h"

d_bsp_t d_bsp;
d_bsp_vis_t *d_vis = (d_bsp_vis_t *) d_bsp.vis_data;

/*
 * CompressVis
 */
int CompressVis(byte *vis, byte *dest) {
	int j;
	int rep;
	int visrow;
	byte *dest_p;

	dest_p = dest;
	visrow = (d_vis->num_clusters + 7) >> 3;

	for (j = 0; j < visrow; j++) {
		*dest_p++ = vis[j];
		if (vis[j])
			continue;

		rep = 1;
		for (j++; j < visrow; j++)
			if (vis[j] || rep == 255)
				break;
			else
				rep++;
		*dest_p++ = rep;
		j--;
	}

	return dest_p - dest;
}

/*
 * DecompressVis
 */
void DecompressVis(byte *in, byte *decompressed) {
	int c;
	byte *out;
	int row;

	row = (d_vis->num_clusters + 7) >> 3;
	out = decompressed;

	do {
		if (*in) {
			*out++ = *in++;
			continue;
		}

		c = in[1];
		if (!c)
			Com_Error(ERR_FATAL, "DecompressVis: 0 repeat\n");
		in += 2;
		while (c) {
			*out++ = 0;
			c--;
		}
	} while (out - decompressed < row);
}

/*
 * SwapBSPFile
 *
 * Byte swaps all data in a bsp file.
 */
static void SwapBSPFile(boolean_t todisk) {
	int i, j;

	// models
	for (i = 0; i < d_bsp.num_models; i++) {
		d_bsp_model_t *d = &d_bsp.models[i];

		d->first_face = LittleLong(d->first_face);
		d->num_faces = LittleLong(d->num_faces);
		d->head_node = LittleLong(d->head_node);

		for (j = 0; j < 3; j++) {
			d->mins[j] = LittleFloat(d->mins[j]);
			d->maxs[j] = LittleFloat(d->maxs[j]);
			d->origin[j] = LittleFloat(d->origin[j]);
		}
	}

	// vertexes
	for (i = 0; i < d_bsp.num_vertexes; i++) {
		for (j = 0; j < 3; j++)
			d_bsp.vertexes[i].point[j]
					= LittleFloat(d_bsp.vertexes[i].point[j]);
	}

	// planes
	for (i = 0; i < d_bsp.num_planes; i++) {
		for (j = 0; j < 3; j++)
			d_bsp.planes[i].normal[j] = LittleFloat(d_bsp.planes[i].normal[j]);
		d_bsp.planes[i].dist = LittleFloat(d_bsp.planes[i].dist);
		d_bsp.planes[i].type = LittleLong(d_bsp.planes[i].type);
	}

	// texinfos
	for (i = 0; i < d_bsp.num_texinfo; i++) {
		for (j = 0; j < 8; j++)
			d_bsp.texinfo[i].vecs[0][j] = LittleFloat(
					d_bsp.texinfo[i].vecs[0][j]);
		d_bsp.texinfo[i].flags = LittleLong(d_bsp.texinfo[i].flags);
		d_bsp.texinfo[i].value = LittleLong(d_bsp.texinfo[i].value);
		d_bsp.texinfo[i].next_texinfo = LittleLong(
				d_bsp.texinfo[i].next_texinfo);
	}

	// faces
	for (i = 0; i < d_bsp.num_faces; i++) {
		d_bsp.faces[i].texinfo = LittleShort(d_bsp.faces[i].texinfo);
		d_bsp.faces[i].plane_num = LittleShort(d_bsp.faces[i].plane_num);
		d_bsp.faces[i].side = LittleShort(d_bsp.faces[i].side);
		d_bsp.faces[i].light_ofs = LittleLong(d_bsp.faces[i].light_ofs);
		d_bsp.faces[i].first_edge = LittleLong(d_bsp.faces[i].first_edge);
		d_bsp.faces[i].num_edges = LittleShort(d_bsp.faces[i].num_edges);
	}

	// nodes
	for (i = 0; i < d_bsp.num_nodes; i++) {
		d_bsp.nodes[i].plane_num = LittleLong(d_bsp.nodes[i].plane_num);
		for (j = 0; j < 3; j++) {
			d_bsp.nodes[i].mins[j] = LittleShort(d_bsp.nodes[i].mins[j]);
			d_bsp.nodes[i].maxs[j] = LittleShort(d_bsp.nodes[i].maxs[j]);
		}
		d_bsp.nodes[i].children[0] = LittleLong(d_bsp.nodes[i].children[0]);
		d_bsp.nodes[i].children[1] = LittleLong(d_bsp.nodes[i].children[1]);
		d_bsp.nodes[i].first_face = LittleShort(d_bsp.nodes[i].first_face);
		d_bsp.nodes[i].num_faces = LittleShort(d_bsp.nodes[i].num_faces);
	}

	// leafs
	for (i = 0; i < d_bsp.num_leafs; i++) {
		d_bsp.leafs[i].contents = LittleLong(d_bsp.leafs[i].contents);
		d_bsp.leafs[i].cluster = LittleShort(d_bsp.leafs[i].cluster);
		d_bsp.leafs[i].area = LittleShort(d_bsp.leafs[i].area);
		for (j = 0; j < 3; j++) {
			d_bsp.leafs[i].mins[j] = LittleShort(d_bsp.leafs[i].mins[j]);
			d_bsp.leafs[i].maxs[j] = LittleShort(d_bsp.leafs[i].maxs[j]);
		}

		d_bsp.leafs[i].first_leaf_face = LittleShort(
				d_bsp.leafs[i].first_leaf_face);
		d_bsp.leafs[i].num_leaf_faces = LittleShort(
				d_bsp.leafs[i].num_leaf_faces);
		d_bsp.leafs[i].first_leaf_brush = LittleShort(
				d_bsp.leafs[i].first_leaf_brush);
		d_bsp.leafs[i].num_leaf_brushes = LittleShort(
				d_bsp.leafs[i].num_leaf_brushes);
	}

	// leaf faces
	for (i = 0; i < d_bsp.num_leaf_faces; i++)
		d_bsp.leaf_faces[i] = LittleShort(d_bsp.leaf_faces[i]);

	// leaf brushes
	for (i = 0; i < d_bsp.num_leaf_brushes; i++)
		d_bsp.leaf_brushes[i] = LittleShort(d_bsp.leaf_brushes[i]);

	// surf edges
	for (i = 0; i < d_bsp.num_face_edges; i++)
		d_bsp.face_edges[i] = LittleLong(d_bsp.face_edges[i]);

	// edges
	for (i = 0; i < d_bsp.num_edges; i++) {
		d_bsp.edges[i].v[0] = LittleShort(d_bsp.edges[i].v[0]);
		d_bsp.edges[i].v[1] = LittleShort(d_bsp.edges[i].v[1]);
	}

	// brushes
	for (i = 0; i < d_bsp.num_brushes; i++) {
		d_bsp.brushes[i].first_side = LittleLong(d_bsp.brushes[i].first_side);
		d_bsp.brushes[i].num_sides = LittleLong(d_bsp.brushes[i].num_sides);
		d_bsp.brushes[i].contents = LittleLong(d_bsp.brushes[i].contents);
	}

	// areas
	for (i = 0; i < d_bsp.num_areas; i++) {
		d_bsp.areas[i].num_area_portals = LittleLong(
				d_bsp.areas[i].num_area_portals);
		d_bsp.areas[i].first_area_portal = LittleLong(
				d_bsp.areas[i].first_area_portal);
	}

	// area portals
	for (i = 0; i < d_bsp.num_area_portals; i++) {
		d_bsp.area_portals[i].portal_num = LittleLong(
				d_bsp.area_portals[i].portal_num);
		d_bsp.area_portals[i].other_area = LittleLong(
				d_bsp.area_portals[i].other_area);
	}

	// brush sides
	for (i = 0; i < d_bsp.num_brush_sides; i++) {
		d_bsp.brush_sides[i].plane_num = LittleShort(
				d_bsp.brush_sides[i].plane_num);
		d_bsp.brush_sides[i].surf_num = LittleShort(
				d_bsp.brush_sides[i].surf_num);
	}

	// visibility
	if (todisk)
		j = d_vis->num_clusters;
	else
		j = LittleLong(d_vis->num_clusters);
	d_vis->num_clusters = LittleLong(d_vis->num_clusters);
	for (i = 0; i < j; i++) {
		d_vis->bit_offsets[i][0] = LittleLong(d_vis->bit_offsets[i][0]);
		d_vis->bit_offsets[i][1] = LittleLong(d_vis->bit_offsets[i][1]);
	}
}

static d_bsp_header_t *header;

static int CopyLump(int lump, void *dest, int size) {
	int length, ofs;

	length = header->lumps[lump].file_len;
	ofs = header->lumps[lump].file_ofs;

	if (length % size)
		Com_Error(ERR_FATAL, "LoadBSPFile: odd lump size\n");

	memcpy(dest, (byte *)header + ofs, length);

	return length / size;
}

/*
 * LoadBSPFile
 */
void LoadBSPFile(char *file_name) {
	unsigned int i;

	// load the file header
	if (Fs_LoadFile(file_name, (void **) (char *) &header) == -1)
		Com_Error(ERR_FATAL, "Failed to open %s\n", file_name);

	// swap the header
	for (i = 0; i < sizeof(d_bsp_header_t) / 4; i++)
		((int *) header)[i] = LittleLong(((int *) header)[i]);

	if (header->ident != BSP_HEADER)
		Com_Error(ERR_FATAL, "%s is not a IBSP file\n", file_name);

	if (header->version != BSP_VERSION && header->version != BSP_VERSION_)
		Com_Error(ERR_FATAL, "%s is unsupported version %i\n", file_name,
				header->version);

	d_bsp.num_models = CopyLump(LUMP_MODELS, d_bsp.models,
			sizeof(d_bsp_model_t));
	d_bsp.num_vertexes = CopyLump(LUMP_VERTEXES, d_bsp.vertexes,
			sizeof(d_bsp_vertex_t));

	d_bsp.num_normals = d_bsp.num_vertexes;

	if (header->version == BSP_VERSION_) // enhanced format
		d_bsp.num_normals = CopyLump(LUMP_NORMALS, d_bsp.normals,
				sizeof(d_bsp_normal_t));

	d_bsp.num_planes = CopyLump(LUMP_PLANES, d_bsp.planes,
			sizeof(d_bsp_plane_t));
	d_bsp.num_leafs = CopyLump(LUMP_LEAFS, d_bsp.leafs, sizeof(d_bsp_leaf_t));
	d_bsp.num_nodes = CopyLump(LUMP_NODES, d_bsp.nodes, sizeof(d_bsp_node_t));
	d_bsp.num_texinfo = CopyLump(LUMP_TEXINFO, d_bsp.texinfo,
			sizeof(d_bsp_texinfo_t));
	d_bsp.num_faces = CopyLump(LUMP_FACES, d_bsp.faces, sizeof(d_bsp_face_t));
	d_bsp.num_leaf_faces = CopyLump(LUMP_LEAF_FACES, d_bsp.leaf_faces,
			sizeof(d_bsp.leaf_faces[0]));
	d_bsp.num_leaf_brushes = CopyLump(LUMP_LEAF_BRUSHES, d_bsp.leaf_brushes,
			sizeof(d_bsp.leaf_brushes[0]));
	d_bsp.num_face_edges = CopyLump(LUMP_FACE_EDGES, d_bsp.face_edges,
			sizeof(d_bsp.face_edges[0]));
	d_bsp.num_edges = CopyLump(LUMP_EDGES, d_bsp.edges, sizeof(d_bsp_edge_t));
	d_bsp.num_brushes = CopyLump(LUMP_BRUSHES, d_bsp.brushes,
			sizeof(d_bsp_brush_t));
	d_bsp.num_brush_sides = CopyLump(LUMP_BRUSH_SIDES, d_bsp.brush_sides,
			sizeof(d_bsp_brush_side_t));
	d_bsp.num_areas = CopyLump(LUMP_AREAS, d_bsp.areas, sizeof(d_bsp_area_t));
	d_bsp.num_area_portals = CopyLump(LUMP_AREA_PORTALS, d_bsp.area_portals,
			sizeof(d_bsp_area_portal_t));

	d_bsp.vis_data_size = CopyLump(LUMP_VISIBILITY, d_bsp.vis_data, 1);
	d_bsp.lightmap_data_size = CopyLump(LUMP_LIGHMAPS, d_bsp.lightmap_data, 1);
	d_bsp.entity_string_len = CopyLump(LUMP_ENTITIES, d_bsp.entity_string, 1);

	CopyLump(LUMP_POP, d_bsp.dpop, 1);

	Z_Free(header); // everything has been copied out

	// swap everything
	SwapBSPFile(false);

	if (verbose)
		PrintBSPFileSizes();
}

/*
 * LoadBSPFileTexinfo
 *
 * Only loads the texinfo lump, so we can scan for textures.
 */
void LoadBSPFileTexinfo(char *file_name) {
	unsigned int i;
	FILE *f;
	int length, ofs;

	header = Z_Malloc(sizeof(*header));

	if (Fs_OpenFile(file_name, &f, FILE_READ) == -1)
		Com_Error(ERR_FATAL, "Could not open %s\n", file_name);

	Fs_Read(header, sizeof(*header), 1, f);

	// swap the header
	for (i = 0; i < sizeof(*header) / 4; i++)
		((int *) header)[i] = LittleLong(((int *) header)[i]);

	if (header->ident != BSP_HEADER)
		Com_Error(ERR_FATAL, "%s is not a bsp file\n", file_name);

	if (header->version != BSP_VERSION && header->version != BSP_VERSION_)
		Com_Error(ERR_FATAL, "%s is unsupported version %i\n", file_name,
				header->version);

	length = header->lumps[LUMP_TEXINFO].file_len;
	ofs = header->lumps[LUMP_TEXINFO].file_ofs;

	fseek(f, ofs, SEEK_SET);
	Fs_Read(d_bsp.texinfo, length, 1, f);
	Fs_CloseFile(f);

	d_bsp.num_texinfo = length / sizeof(d_bsp_texinfo_t);

	Z_Free(header); // everything has been copied out

	SwapBSPFile(false);
}

static FILE *fp;

/*
 * AddLump
 */
static void AddLump(int lump_num, void *data, int len) {
	d_bsp_lump_t *lump;

	lump = &header->lumps[lump_num];

	lump->file_ofs = LittleLong(ftell(fp));
	lump->file_len = LittleLong(len);

	Fs_Write(data, 1, (len + 3) & ~3, fp);
}

/*
 * WriteBSPFile
 *
 * Swaps the bsp file in place, so it should not be referenced again
 */
void WriteBSPFile(char *file_name) {
	static d_bsp_header_t h;
	header = &h;

	if (verbose)
		PrintBSPFileSizes();

	SwapBSPFile(true);

	header->ident = LittleLong(BSP_HEADER);

	if (legacy) // quake2 .bsp format
		header->version = LittleLong(BSP_VERSION);
	else
		// enhanced format
		header->version = LittleLong(BSP_VERSION_);

	if (Fs_OpenFile(file_name, &fp, FILE_WRITE) == -1)
		Com_Error(ERR_FATAL, "Could not open %s\n", file_name);

	Fs_Write(header, 1, sizeof(d_bsp_header_t), fp);

	AddLump(LUMP_PLANES, d_bsp.planes, d_bsp.num_planes * sizeof(d_bsp_plane_t));
	AddLump(LUMP_LEAFS, d_bsp.leafs, d_bsp.num_leafs * sizeof(d_bsp_leaf_t));
	AddLump(LUMP_VERTEXES, d_bsp.vertexes,
			d_bsp.num_vertexes * sizeof(d_bsp_vertex_t));

	if (!legacy) // write vertex normals
		AddLump(LUMP_NORMALS, d_bsp.normals,
				d_bsp.num_normals * sizeof(d_bsp_normal_t));

	AddLump(LUMP_NODES, d_bsp.nodes, d_bsp.num_nodes * sizeof(d_bsp_node_t));
	AddLump(LUMP_TEXINFO, d_bsp.texinfo,
			d_bsp.num_texinfo * sizeof(d_bsp_texinfo_t));
	AddLump(LUMP_FACES, d_bsp.faces, d_bsp.num_faces * sizeof(d_bsp_face_t));
	AddLump(LUMP_BRUSHES, d_bsp.brushes,
			d_bsp.num_brushes * sizeof(d_bsp_brush_t));
	AddLump(LUMP_BRUSH_SIDES, d_bsp.brush_sides,
			d_bsp.num_brush_sides * sizeof(d_bsp_brush_side_t));
	AddLump(LUMP_LEAF_FACES, d_bsp.leaf_faces,
			d_bsp.num_leaf_faces * sizeof(d_bsp.leaf_faces[0]));
	AddLump(LUMP_LEAF_BRUSHES, d_bsp.leaf_brushes,
			d_bsp.num_leaf_brushes * sizeof(d_bsp.leaf_brushes[0]));
	AddLump(LUMP_FACE_EDGES, d_bsp.face_edges,
			d_bsp.num_face_edges * sizeof(d_bsp.face_edges[0]));
	AddLump(LUMP_EDGES, d_bsp.edges, d_bsp.num_edges * sizeof(d_bsp_edge_t));
	AddLump(LUMP_MODELS, d_bsp.models, d_bsp.num_models * sizeof(d_bsp_model_t));
	AddLump(LUMP_AREAS, d_bsp.areas, d_bsp.num_areas * sizeof(d_bsp_area_t));
	AddLump(LUMP_AREA_PORTALS, d_bsp.area_portals,
			d_bsp.num_area_portals * sizeof(d_bsp_area_portal_t));

	AddLump(LUMP_LIGHMAPS, d_bsp.lightmap_data, d_bsp.lightmap_data_size);
	AddLump(LUMP_VISIBILITY, d_bsp.vis_data, d_bsp.vis_data_size);
	AddLump(LUMP_ENTITIES, d_bsp.entity_string, d_bsp.entity_string_len);
	AddLump(LUMP_POP, d_bsp.dpop, sizeof(d_bsp.dpop));

	fseek(fp, 0, SEEK_SET);
	Fs_Write(header, 1, sizeof(d_bsp_header_t), fp);
	Fs_CloseFile(fp);
}

/*
 * PrintBSPFileSizes
 *
 * Dumps info about current file
 */
void PrintBSPFileSizes(void) {

	if (!num_entities)
		ParseEntities();

	Com_Verbose("%5i models       %7i\n", d_bsp.num_models,
			(int) (d_bsp.num_models * sizeof(d_bsp_model_t)));

	Com_Verbose("%5i brushes      %7i\n", d_bsp.num_brushes,
			(int) (d_bsp.num_brushes * sizeof(d_bsp_brush_t)));

	Com_Verbose("%5i brush_sides  %7i\n", d_bsp.num_brush_sides,
			(int) (d_bsp.num_brush_sides * sizeof(d_bsp_brush_side_t)));

	Com_Verbose("%5i planes       %7i\n", d_bsp.num_planes,
			(int) (d_bsp.num_planes * sizeof(d_bsp_plane_t)));

	Com_Verbose("%5i texinfo      %7i\n", d_bsp.num_texinfo,
			(int) (d_bsp.num_texinfo * sizeof(d_bsp_texinfo_t)));

	Com_Verbose("%5i entdata      %7i\n", num_entities, d_bsp.entity_string_len);

	Com_Verbose("\n");

	Com_Verbose("%5i vertexes     %7i\n", d_bsp.num_vertexes,
			(int) (d_bsp.num_vertexes * sizeof(d_bsp_vertex_t)));

	Com_Verbose("%5i normals      %7i\n", d_bsp.num_normals,
			(int) (d_bsp.num_normals * sizeof(d_bsp_normal_t)));

	Com_Verbose("%5i nodes        %7i\n", d_bsp.num_nodes,
			(int) (d_bsp.num_nodes * sizeof(d_bsp_node_t)));

	Com_Verbose("%5i faces        %7i\n", d_bsp.num_faces,
			(int) (d_bsp.num_faces * sizeof(d_bsp_face_t)));

	Com_Verbose("%5i leafs        %7i\n", d_bsp.num_leafs,
			(int) (d_bsp.num_leafs * sizeof(d_bsp_leaf_t)));

	Com_Verbose("%5i leaf_faces   %7i\n", d_bsp.num_leaf_faces,
			(int) (d_bsp.num_leaf_faces * sizeof(d_bsp.leaf_faces[0])));

	Com_Verbose("%5i leaf_brushes %7i\n", d_bsp.num_leaf_brushes,
			(int) (d_bsp.num_leaf_brushes * sizeof(d_bsp.leaf_brushes[0])));

	Com_Verbose("%5i surf_edges   %7i\n", d_bsp.num_face_edges,
			(int) (d_bsp.num_face_edges * sizeof(d_bsp.face_edges[0])));

	Com_Verbose("%5i edges        %7i\n", d_bsp.num_edges,
			(int) (d_bsp.num_edges * sizeof(d_bsp_edge_t)));

	Com_Verbose("      lightmap     %7i\n", d_bsp.lightmap_data_size);

	Com_Verbose("      vis          %7i\n", d_bsp.vis_data_size);
}

int num_entities;
entity_t entities[MAX_BSP_ENTITIES];

/*
 * StripTrailing
 */
static void StripTrailing(char *e) {
	char *s;

	s = e + strlen(e) - 1;
	while (s >= e && *s <= 32) {
		*s = 0;
		s--;
	}
}

/*
 * ParseEpair
 */
epair_t *ParseEpair(void) {
	epair_t *e;

	e = Z_Malloc(sizeof(*e));
	memset(e, 0, sizeof(*e));

	if (strlen(token) >= MAX_KEY - 1)
		Com_Error(ERR_FATAL, "ParseEpar: token too long\n");
	e->key = Z_CopyString(token);
	GetToken(false);
	if (strlen(token) >= MAX_VALUE - 1)
		Com_Error(ERR_FATAL, "ParseEpar: token too long\n");
	e->value = Z_CopyString(token);

	// strip trailing spaces
	StripTrailing(e->key);
	StripTrailing(e->value);

	return e;
}

/*
 * ParseEntity
 */
static boolean_t ParseEntity(void) {
	epair_t *e;
	entity_t *mapent;

	if (!GetToken(true))
		return false;

	if (strcmp(token, "{"))
		Com_Error(ERR_FATAL, "ParseEntity: { not found\n");

	if (num_entities == MAX_BSP_ENTITIES)
		Com_Error(ERR_FATAL, "num_entities == MAX_BSP_ENTITIES\n");

	mapent = &entities[num_entities];
	num_entities++;

	do {
		if (!GetToken(true))
			Com_Error(ERR_FATAL, "ParseEntity: EOF without closing brace\n");
		if (!strcmp(token, "}"))
			break;
		e = ParseEpair();
		e->next = mapent->epairs;
		mapent->epairs = e;
	} while (true);

	return true;
}

/*
 * ParseEntities
 *
 * Parses the d_bsp.entity_string string into entities
 */
void ParseEntities(void) {
	int subdivide;

	ParseFromMemory(d_bsp.entity_string, d_bsp.entity_string_len);

	num_entities = 0;
	while (ParseEntity()) {
	}

	subdivide = atoi(ValueForKey(&entities[0], "subdivide"));

	if (subdivide >= 256 && subdivide <= 2048) {
		Com_Verbose("Using subdivide %d from worldspawn.\n", subdivide);
		subdivide_size = subdivide;
	}
}

/*
 * UnparseEntities
 *
 * Generates the entdata string from all the entities
 */
void UnparseEntities(void) {
	char *buf, *end;
	epair_t *ep;
	char line[2048];
	int i;
	char key[1024], value[1024];

	buf = d_bsp.entity_string;
	end = buf;
	*end = 0;

	for (i = 0; i < num_entities; i++) {
		ep = entities[i].epairs;
		if (!ep)
			continue; // ent got removed

		strcat(end,"{\n");
		end += 2;

		for (ep = entities[i].epairs; ep; ep = ep->next) {
			strcpy(key, ep->key);
			StripTrailing(key);
			strcpy(value, ep->value);
			StripTrailing(value);

			sprintf(line, "\"%s\" \"%s\"\n", key, value);
			strcat(end, line);
			end += strlen(line);
		}
		strcat(end,"}\n");
		end += 2;

		if (end > buf + MAX_BSP_ENT_STRING)
			Com_Error(ERR_FATAL, "Entity text too long\n");
	}

	d_bsp.entity_string_len = end - buf + 1;
}

void SetKeyValue(entity_t *ent, const char *key, const char *value) {
	epair_t *ep;

	for (ep = ent->epairs; ep; ep = ep->next) {
		if (!strcmp(ep->key, key)) {
			Z_Free(ep->value);
			ep->value = Z_CopyString(value);
			return;
		}
	}

	ep = Z_Malloc(sizeof(*ep));
	ep->next = ent->epairs;
	ent->epairs = ep;
	ep->key = Z_CopyString(key);
	ep->value = Z_CopyString(value);
}

const char *ValueForKey(const entity_t *ent, const char *key) {
	const epair_t *ep;

	for (ep = ent->epairs; ep; ep = ep->next)
		if (!strcmp(ep->key, key))
			return ep->value;
	return "";
}

vec_t FloatForKey(const entity_t *ent, const char *key) {
	const char *k;

	k = ValueForKey(ent, key);
	return atof(k);
}

void GetVectorForKey(const entity_t *ent, const char *key, vec3_t vec) {
	const char *k;

	k = ValueForKey(ent, key);

	if (sscanf(k, "%f %f %f", &vec[0], &vec[1], &vec[2]) != 3)
		VectorClear(vec);
}

