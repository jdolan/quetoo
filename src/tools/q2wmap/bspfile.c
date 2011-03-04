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

void GetLeafNums(void);


int nummodels;
d_bsp_model_t dmodels[MAX_BSP_MODELS];

int visdatasize;
byte dvisdata[MAX_BSP_VISIBILITY];
d_bsp_vis_t *dvis = (d_bsp_vis_t *)dvisdata;

int lightmap_data_size;
byte dlightmap_data[MAX_BSP_LIGHTING];

int entdatasize;
char dentdata[MAX_BSP_ENTSTRING];

int num_leafs;
d_bsp_leaf_t dleafs[MAX_BSP_LEAFS];

int num_planes;
d_bsp_plane_t dplanes[MAX_BSP_PLANES];

int num_vertexes;
d_bsp_vertex_t dvertexes[MAX_BSP_VERTS];

int numnormals;
d_bsp_normal_t dnormals[MAX_BSP_VERTS];

int num_nodes;
d_bsp_node_t dnodes[MAX_BSP_NODES];

int num_texinfo;
d_bsp_texinfo_t texinfo[MAX_BSP_TEXINFO];

int num_faces;
d_bsp_face_t dfaces[MAX_BSP_FACES];

int num_edges;
d_bsp_edge_t dedges[MAX_BSP_EDGES];

int num_leaf_faces;
unsigned short dleaffaces[MAX_BSP_LEAFFACES];

int num_leaf_brushes;
unsigned short dleafbrushes[MAX_BSP_LEAFBRUSHES];

int num_surfedges;
int dsurfedges[MAX_BSP_SURFEDGES];

int numareas;
d_bsp_area_t dareas[MAX_BSP_AREAS];

int num_area_portals;
d_bsp_area_portal_t dareaportals[MAX_BSP_AREAPORTALS];

int numbrushes;
d_bsp_brush_t dbrushes[MAX_BSP_BRUSHES];

int numbrushsides;
d_bsp_brush_side_t dbrushsides[MAX_BSP_BRUSHSIDES];

byte dpop[256];


/*
 * CompressVis
 */
int CompressVis(byte *vis, byte *dest){
	int j;
	int rep;
	int visrow;
	byte *dest_p;

	dest_p = dest;
	visrow = (dvis->num_clusters + 7) >> 3;

	for(j=0 ; j<visrow ; j++){
		*dest_p++ = vis[j];
		if(vis[j])
			continue;

		rep = 1;
		for(j++; j<visrow ; j++)
			if(vis[j] || rep == 255)
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
void DecompressVis(byte *in, byte *decompressed){
	int c;
	byte *out;
	int row;

	row = (dvis->num_clusters + 7) >> 3;
	out = decompressed;

	do {
		if(*in){
			*out++ = *in++;
			continue;
		}

		c = in[1];
		if(!c)
			Com_Error(ERR_FATAL, "DecompressVis: 0 repeat\n");
		in += 2;
		while(c){
			*out++ = 0;
			c--;
		}
	} while(out - decompressed < row);
}


/*
 * SwapBSPFile
 *
 * Byte swaps all data in a bsp file.
 */
static void SwapBSPFile(qboolean todisk){
	int i, j;

	// models
	for(i = 0; i < nummodels; i++){
		d_bsp_model_t *d = &dmodels[i];

		d->first_face = LittleLong(d->first_face);
		d->num_faces = LittleLong(d->num_faces);
		d->head_node = LittleLong(d->head_node);

		for(j = 0; j < 3; j++){
			d->mins[j] = LittleFloat(d->mins[j]);
			d->maxs[j] = LittleFloat(d->maxs[j]);
			d->origin[j] = LittleFloat(d->origin[j]);
		}
	}

	// vertexes
	for(i = 0; i < num_vertexes; i++){
		for(j = 0; j < 3; j++)
			dvertexes[i].point[j] = LittleFloat(dvertexes[i].point[j]);
	}

	// planes
	for(i = 0; i < num_planes; i++){
		for(j = 0; j < 3; j++)
			dplanes[i].normal[j] = LittleFloat(dplanes[i].normal[j]);
		dplanes[i].dist = LittleFloat(dplanes[i].dist);
		dplanes[i].type = LittleLong(dplanes[i].type);
	}

	// texinfos
	for(i = 0; i < num_texinfo; i++){
		for(j = 0; j < 8; j++)
			texinfo[i].vecs[0][j] = LittleFloat(texinfo[i].vecs[0][j]);
		texinfo[i].flags = LittleLong(texinfo[i].flags);
		texinfo[i].value = LittleLong(texinfo[i].value);
		texinfo[i].next_texinfo = LittleLong(texinfo[i].next_texinfo);
	}

	// faces
	for(i = 0; i < num_faces; i++){
		dfaces[i].texinfo = LittleShort(dfaces[i].texinfo);
		dfaces[i].plane_num = LittleShort(dfaces[i].plane_num);
		dfaces[i].side = LittleShort(dfaces[i].side);
		dfaces[i].light_ofs = LittleLong(dfaces[i].light_ofs);
		dfaces[i].first_edge = LittleLong(dfaces[i].first_edge);
		dfaces[i].num_edges = LittleShort(dfaces[i].num_edges);
	}

	// nodes
	for(i = 0; i < num_nodes; i++){
		dnodes[i].plane_num = LittleLong(dnodes[i].plane_num);
		for(j = 0; j < 3; j++){
			dnodes[i].mins[j] = LittleShort(dnodes[i].mins[j]);
			dnodes[i].maxs[j] = LittleShort(dnodes[i].maxs[j]);
		}
		dnodes[i].children[0] = LittleLong(dnodes[i].children[0]);
		dnodes[i].children[1] = LittleLong(dnodes[i].children[1]);
		dnodes[i].first_face = LittleShort(dnodes[i].first_face);
		dnodes[i].num_faces = LittleShort(dnodes[i].num_faces);
	}

	// leafs
	for(i = 0; i < num_leafs; i++){
		dleafs[i].contents = LittleLong(dleafs[i].contents);
		dleafs[i].cluster = LittleShort(dleafs[i].cluster);
		dleafs[i].area = LittleShort(dleafs[i].area);
		for(j = 0; j < 3; j++){
			dleafs[i].mins[j] = LittleShort(dleafs[i].mins[j]);
			dleafs[i].maxs[j] = LittleShort(dleafs[i].maxs[j]);
		}

		dleafs[i].first_leaf_face = LittleShort(dleafs[i].first_leaf_face);
		dleafs[i].num_leaf_faces = LittleShort(dleafs[i].num_leaf_faces);
		dleafs[i].first_leaf_brush = LittleShort(dleafs[i].first_leaf_brush);
		dleafs[i].num_leaf_brushes = LittleShort(dleafs[i].num_leaf_brushes);
	}

	// leaffaces
	for(i = 0; i < num_leaf_faces; i++)
		dleaffaces[i] = LittleShort(dleaffaces[i]);

	// leafbrushes
	for(i = 0; i < num_leaf_brushes; i++)
		dleafbrushes[i] = LittleShort(dleafbrushes[i]);

	// surfedges
	for(i = 0; i < num_surfedges; i++)
		dsurfedges[i] = LittleLong(dsurfedges[i]);

	// edges
	for(i = 0; i < num_edges; i++){
		dedges[i].v[0] = LittleShort(dedges[i].v[0]);
		dedges[i].v[1] = LittleShort(dedges[i].v[1]);
	}

	// brushes
	for(i = 0; i < numbrushes; i++){
		dbrushes[i].firstside = LittleLong(dbrushes[i].firstside);
		dbrushes[i].numsides = LittleLong(dbrushes[i].numsides);
		dbrushes[i].contents = LittleLong(dbrushes[i].contents);
	}

	// areas
	for(i = 0; i < numareas; i++){
		dareas[i].num_area_portals = LittleLong(dareas[i].num_area_portals);
		dareas[i].first_area_portal = LittleLong(dareas[i].first_area_portal);
	}

	// areasportals
	for(i = 0; i < num_area_portals; i++){
		dareaportals[i].portal_num = LittleLong(dareaportals[i].portal_num);
		dareaportals[i].other_area = LittleLong(dareaportals[i].other_area);
	}

	// brushsides
	for(i = 0; i < numbrushsides; i++){
		dbrushsides[i].plane_num = LittleShort(dbrushsides[i].plane_num);
		dbrushsides[i].surf_num = LittleShort(dbrushsides[i].surf_num);
	}

	// visibility
	if(todisk)
		j = dvis->num_clusters;
	else
		j = LittleLong(dvis->num_clusters);
	dvis->num_clusters = LittleLong(dvis->num_clusters);
	for(i = 0; i < j; i++){
		dvis->bit_ofs[i][0] = LittleLong(dvis->bit_ofs[i][0]);
		dvis->bit_ofs[i][1] = LittleLong(dvis->bit_ofs[i][1]);
	}
}


static d_bsp_header_t *header;

static int CopyLump(int lump, void *dest, int size){
	int length, ofs;

	length = header->lumps[lump].file_len;
	ofs = header->lumps[lump].file_ofs;

	if(length % size)
		Com_Error(ERR_FATAL, "LoadBSPFile: odd lump size\n");

	memcpy(dest, (byte *)header + ofs, length);

	return length / size;
}


/*
 * LoadBSPFile
 */
void LoadBSPFile(char *file_name){
	int i;

	// load the file header
	if(Fs_LoadFile(file_name, (void **)(char *)&header) == -1)
		Com_Error(ERR_FATAL, "Failed to open %s\n", file_name);

	// swap the header
	for(i = 0; i < sizeof(d_bsp_header_t) / 4; i++)
		((int *)header)[i] = LittleLong(((int *)header)[i]);

	if(header->ident != BSP_HEADER)
		Com_Error(ERR_FATAL, "%s is not a IBSP file\n", file_name);

	if(header->version != BSP_VERSION && header->version != BSP_VERSION_)
		Com_Error(ERR_FATAL, "%s is unsupported version %i\n", file_name, header->version);

	nummodels = CopyLump(LUMP_MODELS, dmodels, sizeof(d_bsp_model_t));
	num_vertexes = CopyLump(LUMP_VERTEXES, dvertexes, sizeof(d_bsp_vertex_t));

	memset(dnormals, 0, sizeof(dnormals));
	numnormals = num_vertexes;

	if(header->version == BSP_VERSION_)  // enhanced format
		numnormals = CopyLump(LUMP_NORMALS, dnormals, sizeof(d_bsp_normal_t));

	num_planes = CopyLump(LUMP_PLANES, dplanes, sizeof(d_bsp_plane_t));
	num_leafs = CopyLump(LUMP_LEAFS, dleafs, sizeof(d_bsp_leaf_t));
	num_nodes = CopyLump(LUMP_NODES, dnodes, sizeof(d_bsp_node_t));
	num_texinfo = CopyLump(LUMP_TEXINFO, texinfo, sizeof(d_bsp_texinfo_t));
	num_faces = CopyLump(LUMP_FACES, dfaces, sizeof(d_bsp_face_t));
	num_leaf_faces = CopyLump(LUMP_LEAFFACES, dleaffaces, sizeof(dleaffaces[0]));
	num_leaf_brushes = CopyLump(LUMP_LEAFBRUSHES, dleafbrushes, sizeof(dleafbrushes[0]));
	num_surfedges = CopyLump(LUMP_SURFEDGES, dsurfedges, sizeof(dsurfedges[0]));
	num_edges = CopyLump(LUMP_EDGES, dedges, sizeof(d_bsp_edge_t));
	numbrushes = CopyLump(LUMP_BRUSHES, dbrushes, sizeof(d_bsp_brush_t));
	numbrushsides = CopyLump(LUMP_BRUSHSIDES, dbrushsides, sizeof(d_bsp_brush_side_t));
	numareas = CopyLump(LUMP_AREAS, dareas, sizeof(d_bsp_area_t));
	num_area_portals = CopyLump(LUMP_AREAPORTALS, dareaportals, sizeof(d_bsp_area_portal_t));

	visdatasize = CopyLump(LUMP_VISIBILITY, dvisdata, 1);
	lightmap_data_size = CopyLump(LUMP_LIGHMAPS, dlightmap_data, 1);
	entdatasize = CopyLump(LUMP_ENTITIES, dentdata, 1);

	CopyLump(LUMP_POP, dpop, 1);

	Z_Free(header); // everything has been copied out

	// swap everything
	SwapBSPFile(false);

	if(debug)
		PrintBSPFileSizes();
}


/*
 * LoadBSPFileTexinfo
 *
 * Only loads the texinfo lump, so we can scan for textures.
 */
void LoadBSPFileTexinfo(char *file_name){
	int i;
	FILE *f;
	int length, ofs;

	header = Z_Malloc(sizeof(*header));

	if(Fs_OpenFile(file_name, &f, FILE_READ) == -1)
		Com_Error(ERR_FATAL, "Could not open %s\n", file_name);

	Fs_Read(header, sizeof(*header), 1, f);

	// swap the header
	for(i = 0; i < sizeof(*header) / 4; i++)
		((int *)header)[i] = LittleLong(((int *)header)[i]);

	if(header->ident != BSP_HEADER)
		Com_Error(ERR_FATAL, "%s is not a bsp file\n", file_name);

	if(header->version != BSP_VERSION && header->version != BSP_VERSION_)
		Com_Error(ERR_FATAL, "%s is unsupported version %i\n", file_name, header->version);

	length = header->lumps[LUMP_TEXINFO].file_len;
	ofs = header->lumps[LUMP_TEXINFO].file_ofs;

	fseek(f, ofs, SEEK_SET);
	Fs_Read(texinfo, length, 1, f);
	Fs_CloseFile(f);

	num_texinfo = length / sizeof(d_bsp_texinfo_t);

	Z_Free(header);		// everything has been copied out

	SwapBSPFile(false);
}


FILE *wadfile;
d_bsp_header_t outheader;


/*
 * AddLump
 */
static void AddLump(int lumpnum, void *data, int len){
	d_bsp_lump_t *lump;

	lump = &header->lumps[lumpnum];

	lump->file_ofs = LittleLong(ftell(wadfile));
	lump->file_len = LittleLong(len);

	Fs_Write(data, 1, (len + 3) & ~3, wadfile);
}


/*
 * WriteBSPFile
 *
 * Swaps the bsp file in place, so it should not be referenced again
 */
void WriteBSPFile(char *file_name){
	header = &outheader;
	memset(header, 0, sizeof(d_bsp_header_t));

	SwapBSPFile(true);

	header->ident = LittleLong(BSP_HEADER);

	if(legacy)  // quake2 .bsp format
		header->version = LittleLong(BSP_VERSION);
	else  // enhanced format
		header->version = LittleLong(BSP_VERSION_);

	if(Fs_OpenFile(file_name, &wadfile, FILE_WRITE) == -1)
		Com_Error(ERR_FATAL, "Could not open %s\n", file_name);

	Fs_Write(header, 1, sizeof(d_bsp_header_t), wadfile);

	AddLump(LUMP_PLANES, dplanes, num_planes*sizeof(d_bsp_plane_t));
	AddLump(LUMP_LEAFS, dleafs, num_leafs*sizeof(d_bsp_leaf_t));
	AddLump(LUMP_VERTEXES, dvertexes, num_vertexes*sizeof(d_bsp_vertex_t));

	if(!legacy)  // write vertex normals
		AddLump(LUMP_NORMALS, dnormals, numnormals*sizeof(d_bsp_normal_t));

	AddLump(LUMP_NODES, dnodes, num_nodes*sizeof(d_bsp_node_t));
	AddLump(LUMP_TEXINFO, texinfo, num_texinfo*sizeof(d_bsp_texinfo_t));
	AddLump(LUMP_FACES, dfaces, num_faces*sizeof(d_bsp_face_t));
	AddLump(LUMP_BRUSHES, dbrushes, numbrushes*sizeof(d_bsp_brush_t));
	AddLump(LUMP_BRUSHSIDES, dbrushsides, numbrushsides*sizeof(d_bsp_brush_side_t));
	AddLump(LUMP_LEAFFACES, dleaffaces, num_leaf_faces*sizeof(dleaffaces[0]));
	AddLump(LUMP_LEAFBRUSHES, dleafbrushes, num_leaf_brushes*sizeof(dleafbrushes[0]));
	AddLump(LUMP_SURFEDGES, dsurfedges, num_surfedges*sizeof(dsurfedges[0]));
	AddLump(LUMP_EDGES, dedges, num_edges*sizeof(d_bsp_edge_t));
	AddLump(LUMP_MODELS, dmodels, nummodels*sizeof(d_bsp_model_t));
	AddLump(LUMP_AREAS, dareas, numareas*sizeof(d_bsp_area_t));
	AddLump(LUMP_AREAPORTALS, dareaportals, num_area_portals*sizeof(d_bsp_area_portal_t));

	AddLump(LUMP_LIGHMAPS, dlightmap_data, lightmap_data_size);
	AddLump(LUMP_VISIBILITY, dvisdata, visdatasize);
	AddLump(LUMP_ENTITIES, dentdata, entdatasize);
	AddLump(LUMP_POP, dpop, sizeof(dpop));

	fseek(wadfile, 0, SEEK_SET);
	Fs_Write(header, 1, sizeof(d_bsp_header_t), wadfile);
	Fs_CloseFile(wadfile);
}


/*
 * PrintBSPFileSizes
 *
 * Dumps info about current file
 */
void PrintBSPFileSizes(void){

	if(!num_entities)
		ParseEntities();

	Com_Verbose("%5i models       %7i\n"
	       ,nummodels, (int)(nummodels*sizeof(d_bsp_model_t)));
	Com_Verbose("%5i brushes      %7i\n"
	       ,numbrushes, (int)(numbrushes*sizeof(d_bsp_brush_t)));
	Com_Verbose("%5i brushsides   %7i\n"
	       ,numbrushsides, (int)(numbrushsides*sizeof(d_bsp_brush_side_t)));
	Com_Verbose("%5i planes       %7i\n"
	       ,num_planes, (int)(num_planes*sizeof(d_bsp_plane_t)));
	Com_Verbose("%5i texinfo      %7i\n"
	       ,num_texinfo, (int)(num_texinfo*sizeof(d_bsp_texinfo_t)));
	Com_Verbose("%5i entdata      %7i\n", num_entities, entdatasize);

	Com_Verbose("\n");

	Com_Verbose("%5i vertexes     %7i\n"
	       ,num_vertexes, (int)(num_vertexes*sizeof(d_bsp_vertex_t)));
	Com_Verbose("%5i normals      %7i\n"
		   ,numnormals, (int)(numnormals*sizeof(d_bsp_normal_t)));
	Com_Verbose("%5i nodes        %7i\n"
	       ,num_nodes, (int)(num_nodes*sizeof(d_bsp_node_t)));
	Com_Verbose("%5i faces        %7i\n"
	       ,num_faces, (int)(num_faces*sizeof(d_bsp_face_t)));
	Com_Verbose("%5i leafs        %7i\n"
	       ,num_leafs, (int)(num_leafs*sizeof(d_bsp_leaf_t)));
	Com_Verbose("%5i leaffaces    %7i\n"
	       ,num_leaf_faces, (int)(num_leaf_faces*sizeof(dleaffaces[0])));
	Com_Verbose("%5i leafbrushes  %7i\n"
	       ,num_leaf_brushes, (int)(num_leaf_brushes*sizeof(dleafbrushes[0])));
	Com_Verbose("%5i surfedges    %7i\n"
	       ,num_surfedges, (int)(num_surfedges*sizeof(dsurfedges[0])));
	Com_Verbose("%5i edges        %7i\n"
	       ,num_edges, (int)(num_edges*sizeof(d_bsp_edge_t)));
	Com_Verbose("      lightmap_data    %7i\n", lightmap_data_size);
	Com_Verbose("      visdata      %7i\n", visdatasize);
}


int num_entities;
entity_t entities[MAX_BSP_ENTITIES];


/*
 * StripTrailing
 */
static void StripTrailing(char *e){
	char *s;

	s = e + strlen(e)-1;
	while(s >= e && *s <= 32){
		*s = 0;
		s--;
	}
}


/*
 * ParseEpair
 */
epair_t *ParseEpair(void){
	epair_t	*e;

	e = Z_Malloc(sizeof(*e));
	memset(e, 0, sizeof(*e));

	if(strlen(token) >= MAX_KEY - 1)
		Com_Error(ERR_FATAL, "ParseEpar: token too long\n");
	e->key = Z_CopyString(token);
	GetToken(false);
	if(strlen(token) >= MAX_VALUE - 1)
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
static qboolean ParseEntity(void){
	epair_t *e;
	entity_t *mapent;

	if(!GetToken(true))
		return false;

	if(strcmp(token, "{"))
		Com_Error(ERR_FATAL, "ParseEntity: { not found\n");

	if(num_entities == MAX_BSP_ENTITIES)
		Com_Error(ERR_FATAL, "num_entities == MAX_BSP_ENTITIES\n");

	mapent = &entities[num_entities];
	num_entities++;

	do {
		if(!GetToken(true))
			Com_Error(ERR_FATAL, "ParseEntity: EOF without closing brace\n");
		if(!strcmp(token, "}"))
			break;
		e = ParseEpair();
		e->next = mapent->epairs;
		mapent->epairs = e;
	} while(true);

	return true;
}


/*
 * ParseEntities
 *
 * Parses the dentdata string into entities
 */
void ParseEntities(void){
	int subdivide;

	ParseFromMemory(dentdata, entdatasize);

	num_entities = 0;
	while(ParseEntity()){}

	subdivide = atoi(ValueForKey(&entities[0], "subdivide"));

	if(subdivide >= 256 && subdivide <= 2048){
		Com_Verbose("Using subdivide %d from worldspawn.\n", subdivide);
		subdivide_size = subdivide;
	}
}


/*
 * UnparseEntities
 *
 * Generates the entdata string from all the entities
 */
void UnparseEntities(void){
	char *buf, *end;
	epair_t	*ep;
	char line[2048];
	int i;
	char key[1024], value[1024];

	buf = dentdata;
	end = buf;
	*end = 0;

	for(i = 0; i < num_entities; i++){
		ep = entities[i].epairs;
		if(!ep)
			continue;	// ent got removed

		strcat(end,"{\n");
		end += 2;

		for(ep = entities[i].epairs; ep; ep = ep->next){
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

		if(end > buf + MAX_BSP_ENTSTRING)
			Com_Error(ERR_FATAL, "Entity text too long\n");
	}
	entdatasize = end - buf + 1;
}

void SetKeyValue(entity_t *ent, const char *key, const char *value){
	epair_t	*ep;

	for(ep = ent->epairs; ep; ep = ep->next){
		if(!strcmp(ep->key, key)){
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

const char *ValueForKey(const entity_t *ent, const char *key){
	const epair_t	*ep;

	for(ep = ent->epairs; ep; ep = ep->next)
		if(!strcmp(ep->key, key))
			return ep->value;
	return "";
}

vec_t FloatForKey(const entity_t *ent, const char *key){
	const char *k;

	k = ValueForKey(ent, key);
	return atof(k);
}

void GetVectorForKey(const entity_t *ent, const char *key, vec3_t vec){
	const char *k;

	k = ValueForKey(ent, key);

	if(sscanf(k, "%f %f %f", &vec[0], &vec[1], &vec[2]) != 3)
		VectorClear(vec);
}

