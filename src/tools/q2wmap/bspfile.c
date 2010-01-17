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
dbspmodel_t dmodels[MAX_BSP_MODELS];

int visdatasize;
byte dvisdata[MAX_BSP_VISIBILITY];
dvis_t *dvis = (dvis_t *)dvisdata;

int lightdatasize;
byte dlightdata[MAX_BSP_LIGHTING];

int entdatasize;
char dentdata[MAX_BSP_ENTSTRING];

int numleafs;
dleaf_t dleafs[MAX_BSP_LEAFS];

int numplanes;
dplane_t dplanes[MAX_BSP_PLANES];

int numvertexes;
dbspvertex_t dvertexes[MAX_BSP_VERTS];

int numnormals;
dbspnormal_t dnormals[MAX_BSP_VERTS];

int numnodes;
dnode_t dnodes[MAX_BSP_NODES];

int numtexinfo;
dtexinfo_t texinfo[MAX_BSP_TEXINFO];

int numfaces;
dface_t dfaces[MAX_BSP_FACES];

int numedges;
dedge_t dedges[MAX_BSP_EDGES];

int numleaffaces;
unsigned short dleaffaces[MAX_BSP_LEAFFACES];

int numleafbrushes;
unsigned short dleafbrushes[MAX_BSP_LEAFBRUSHES];

int numsurfedges;
int dsurfedges[MAX_BSP_SURFEDGES];

int numareas;
darea_t dareas[MAX_BSP_AREAS];

int numareaportals;
dareaportal_t dareaportals[MAX_BSP_AREAPORTALS];

int numbrushes;
dbrush_t dbrushes[MAX_BSP_BRUSHES];

int numbrushsides;
dbrushside_t dbrushsides[MAX_BSP_BRUSHSIDES];

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
	visrow = (dvis->numclusters + 7) >> 3;

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

	row = (dvis->numclusters + 7) >> 3;
	out = decompressed;

	do {
		if(*in){
			*out++ = *in++;
			continue;
		}

		c = in[1];
		if(!c)
			Error("DecompressVis: 0 repeat\n");
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
		dbspmodel_t *d = &dmodels[i];

		d->firstface = LittleLong(d->firstface);
		d->numfaces = LittleLong(d->numfaces);
		d->headnode = LittleLong(d->headnode);

		for(j = 0; j < 3; j++){
			d->mins[j] = LittleFloat(d->mins[j]);
			d->maxs[j] = LittleFloat(d->maxs[j]);
			d->origin[j] = LittleFloat(d->origin[j]);
		}
	}

	// vertexes
	for(i = 0; i < numvertexes; i++){
		for(j = 0; j < 3; j++)
			dvertexes[i].point[j] = LittleFloat(dvertexes[i].point[j]);
	}

	// planes
	for(i = 0; i < numplanes; i++){
		for(j = 0; j < 3; j++)
			dplanes[i].normal[j] = LittleFloat(dplanes[i].normal[j]);
		dplanes[i].dist = LittleFloat(dplanes[i].dist);
		dplanes[i].type = LittleLong(dplanes[i].type);
	}

	// texinfos
	for(i = 0; i < numtexinfo; i++){
		for(j = 0; j < 8; j++)
			texinfo[i].vecs[0][j] = LittleFloat(texinfo[i].vecs[0][j]);
		texinfo[i].flags = LittleLong(texinfo[i].flags);
		texinfo[i].value = LittleLong(texinfo[i].value);
		texinfo[i].nexttexinfo = LittleLong(texinfo[i].nexttexinfo);
	}

	// faces
	for(i = 0; i < numfaces; i++){
		dfaces[i].texinfo = LittleShort(dfaces[i].texinfo);
		dfaces[i].planenum = LittleShort(dfaces[i].planenum);
		dfaces[i].side = LittleShort(dfaces[i].side);
		dfaces[i].lightofs = LittleLong(dfaces[i].lightofs);
		dfaces[i].firstedge = LittleLong(dfaces[i].firstedge);
		dfaces[i].numedges = LittleShort(dfaces[i].numedges);
	}

	// nodes
	for(i = 0; i < numnodes; i++){
		dnodes[i].planenum = LittleLong(dnodes[i].planenum);
		for(j = 0; j < 3; j++){
			dnodes[i].mins[j] = LittleShort(dnodes[i].mins[j]);
			dnodes[i].maxs[j] = LittleShort(dnodes[i].maxs[j]);
		}
		dnodes[i].children[0] = LittleLong(dnodes[i].children[0]);
		dnodes[i].children[1] = LittleLong(dnodes[i].children[1]);
		dnodes[i].firstface = LittleShort(dnodes[i].firstface);
		dnodes[i].numfaces = LittleShort(dnodes[i].numfaces);
	}

	// leafs
	for(i = 0; i < numleafs; i++){
		dleafs[i].contents = LittleLong(dleafs[i].contents);
		dleafs[i].cluster = LittleShort(dleafs[i].cluster);
		dleafs[i].area = LittleShort(dleafs[i].area);
		for(j = 0; j < 3; j++){
			dleafs[i].mins[j] = LittleShort(dleafs[i].mins[j]);
			dleafs[i].maxs[j] = LittleShort(dleafs[i].maxs[j]);
		}

		dleafs[i].firstleafface = LittleShort(dleafs[i].firstleafface);
		dleafs[i].numleaffaces = LittleShort(dleafs[i].numleaffaces);
		dleafs[i].firstleafbrush = LittleShort(dleafs[i].firstleafbrush);
		dleafs[i].numleafbrushes = LittleShort(dleafs[i].numleafbrushes);
	}

	// leaffaces
	for(i = 0; i < numleaffaces; i++)
		dleaffaces[i] = LittleShort(dleaffaces[i]);

	// leafbrushes
	for(i = 0; i < numleafbrushes; i++)
		dleafbrushes[i] = LittleShort(dleafbrushes[i]);

	// surfedges
	for(i = 0; i < numsurfedges; i++)
		dsurfedges[i] = LittleLong(dsurfedges[i]);

	// edges
	for(i = 0; i < numedges; i++){
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
		dareas[i].numareaportals = LittleLong(dareas[i].numareaportals);
		dareas[i].firstareaportal = LittleLong(dareas[i].firstareaportal);
	}

	// areasportals
	for(i = 0; i < numareaportals; i++){
		dareaportals[i].portalnum = LittleLong(dareaportals[i].portalnum);
		dareaportals[i].otherarea = LittleLong(dareaportals[i].otherarea);
	}

	// brushsides
	for(i = 0; i < numbrushsides; i++){
		dbrushsides[i].planenum = LittleShort(dbrushsides[i].planenum);
		dbrushsides[i].surfnum = LittleShort(dbrushsides[i].surfnum);
	}

	// visibility
	if(todisk)
		j = dvis->numclusters;
	else
		j = LittleLong(dvis->numclusters);
	dvis->numclusters = LittleLong(dvis->numclusters);
	for(i = 0; i < j; i++){
		dvis->bitofs[i][0] = LittleLong(dvis->bitofs[i][0]);
		dvis->bitofs[i][1] = LittleLong(dvis->bitofs[i][1]);
	}
}


static dbspheader_t *header;

static int CopyLump(int lump, void *dest, int size){
	int length, ofs;

	length = header->lumps[lump].filelen;
	ofs = header->lumps[lump].fileofs;

	if(length % size)
		Error("LoadBSPFile: odd lump size\n");

	memcpy(dest, (byte *)header + ofs, length);

	return length / size;
}


/*
 * LoadBSPFile
 */
void LoadBSPFile(char *filename){
	int i;

	// load the file header
	if(Fs_LoadFile(filename, (void **)(char *)&header) == -1)
		Error("Failed to open %s\n", filename);

	// swap the header
	for(i = 0; i < sizeof(dbspheader_t) / 4; i++)
		((int *)header)[i] = LittleLong(((int *)header)[i]);

	if(header->ident != BSP_HEADER)
		Error("%s is not a IBSP file\n", filename);

	if(header->version != BSP_VERSION && header->version != BSP_VERSION_)
		Error("%s is unsupported version %i\n", filename, header->version);

	nummodels = CopyLump(LUMP_MODELS, dmodels, sizeof(dbspmodel_t));
	numvertexes = CopyLump(LUMP_VERTEXES, dvertexes, sizeof(dbspvertex_t));

	memset(dnormals, 0, sizeof(dnormals));
	numnormals = numvertexes;

	if(header->version == BSP_VERSION_)  // enhanced format
		numnormals = CopyLump(LUMP_NORMALS, dnormals, sizeof(dbspnormal_t));

	numplanes = CopyLump(LUMP_PLANES, dplanes, sizeof(dplane_t));
	numleafs = CopyLump(LUMP_LEAFS, dleafs, sizeof(dleaf_t));
	numnodes = CopyLump(LUMP_NODES, dnodes, sizeof(dnode_t));
	numtexinfo = CopyLump(LUMP_TEXINFO, texinfo, sizeof(dtexinfo_t));
	numfaces = CopyLump(LUMP_FACES, dfaces, sizeof(dface_t));
	numleaffaces = CopyLump(LUMP_LEAFFACES, dleaffaces, sizeof(dleaffaces[0]));
	numleafbrushes = CopyLump(LUMP_LEAFBRUSHES, dleafbrushes, sizeof(dleafbrushes[0]));
	numsurfedges = CopyLump(LUMP_SURFEDGES, dsurfedges, sizeof(dsurfedges[0]));
	numedges = CopyLump(LUMP_EDGES, dedges, sizeof(dedge_t));
	numbrushes = CopyLump(LUMP_BRUSHES, dbrushes, sizeof(dbrush_t));
	numbrushsides = CopyLump(LUMP_BRUSHSIDES, dbrushsides, sizeof(dbrushside_t));
	numareas = CopyLump(LUMP_AREAS, dareas, sizeof(darea_t));
	numareaportals = CopyLump(LUMP_AREAPORTALS, dareaportals, sizeof(dareaportal_t));

	visdatasize = CopyLump(LUMP_VISIBILITY, dvisdata, 1);
	lightdatasize = CopyLump(LUMP_LIGHTING, dlightdata, 1);
	entdatasize = CopyLump(LUMP_ENTITIES, dentdata, 1);

	CopyLump(LUMP_POP, dpop, 1);

	Z_Free(header); // everything has been copied out

	// swap everything
	SwapBSPFile(false);
}


/*
 * LoadBSPFileTexinfo
 *
 * Only loads the texinfo lump, so we can scan for textures.
 */
void LoadBSPFileTexinfo(char *filename){
	int i;
	FILE *f;
	int length, ofs;

	header = Z_Malloc(sizeof(*header));

	if(Fs_OpenFile(filename, &f, FILE_READ) == -1)
		Error("Could not open %s\n", filename);

	Fs_Read(header, sizeof(*header), 1, f);

	// swap the header
	for(i = 0; i < sizeof(*header) / 4; i++)
		((int *)header)[i] = LittleLong(((int *)header)[i]);

	if(header->ident != BSP_HEADER)
		Error("%s is not a bsp file\n", filename);

	if(header->version != BSP_VERSION && header->version != BSP_VERSION_)
		Error("%s is unsupported version %i\n", filename, header->version);

	length = header->lumps[LUMP_TEXINFO].filelen;
	ofs = header->lumps[LUMP_TEXINFO].fileofs;

	fseek(f, ofs, SEEK_SET);
	Fs_Read(texinfo, length, 1, f);
	Fs_CloseFile(f);

	numtexinfo = length / sizeof(dtexinfo_t);

	Z_Free(header);		// everything has been copied out

	SwapBSPFile(false);
}


FILE *wadfile;
dbspheader_t outheader;


/*
 * AddLump
 */
static void AddLump(int lumpnum, void *data, int len){
	lump_t *lump;

	lump = &header->lumps[lumpnum];

	lump->fileofs = LittleLong(ftell(wadfile));
	lump->filelen = LittleLong(len);

	Fs_Write(data, 1, (len + 3) & ~3, wadfile);
}


/*
 * WriteBSPFile
 *
 * Swaps the bsp file in place, so it should not be referenced again
 */
void WriteBSPFile(char *filename){
	header = &outheader;
	memset(header, 0, sizeof(dbspheader_t));

	SwapBSPFile(true);

	header->ident = LittleLong(BSP_HEADER);

	if(legacy)  // quake2 .bsp format
		header->version = LittleLong(BSP_VERSION);
	else  // enhanced format
		header->version = LittleLong(BSP_VERSION_);

	if(Fs_OpenFile(filename, &wadfile, FILE_WRITE) == -1)
		Error("Could not open %s\n", filename);

	Fs_Write(header, 1, sizeof(dbspheader_t), wadfile);

	AddLump(LUMP_PLANES, dplanes, numplanes*sizeof(dplane_t));
	AddLump(LUMP_LEAFS, dleafs, numleafs*sizeof(dleaf_t));
	AddLump(LUMP_VERTEXES, dvertexes, numvertexes*sizeof(dbspvertex_t));

	if(!legacy)  // write vertex normals
		AddLump(LUMP_NORMALS, dnormals, numnormals*sizeof(dbspnormal_t));

	AddLump(LUMP_NODES, dnodes, numnodes*sizeof(dnode_t));
	AddLump(LUMP_TEXINFO, texinfo, numtexinfo*sizeof(dtexinfo_t));
	AddLump(LUMP_FACES, dfaces, numfaces*sizeof(dface_t));
	AddLump(LUMP_BRUSHES, dbrushes, numbrushes*sizeof(dbrush_t));
	AddLump(LUMP_BRUSHSIDES, dbrushsides, numbrushsides*sizeof(dbrushside_t));
	AddLump(LUMP_LEAFFACES, dleaffaces, numleaffaces*sizeof(dleaffaces[0]));
	AddLump(LUMP_LEAFBRUSHES, dleafbrushes, numleafbrushes*sizeof(dleafbrushes[0]));
	AddLump(LUMP_SURFEDGES, dsurfedges, numsurfedges*sizeof(dsurfedges[0]));
	AddLump(LUMP_EDGES, dedges, numedges*sizeof(dedge_t));
	AddLump(LUMP_MODELS, dmodels, nummodels*sizeof(dbspmodel_t));
	AddLump(LUMP_AREAS, dareas, numareas*sizeof(darea_t));
	AddLump(LUMP_AREAPORTALS, dareaportals, numareaportals*sizeof(dareaportal_t));

	AddLump(LUMP_LIGHTING, dlightdata, lightdatasize);
	AddLump(LUMP_VISIBILITY, dvisdata, visdatasize);
	AddLump(LUMP_ENTITIES, dentdata, entdatasize);
	AddLump(LUMP_POP, dpop, sizeof(dpop));

	fseek(wadfile, 0, SEEK_SET);
	Fs_Write(header, 1, sizeof(dbspheader_t), wadfile);
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

	Verbose("%5i models       %7i\n"
	       ,nummodels, (int)(nummodels*sizeof(dbspmodel_t)));
	Verbose("%5i brushes      %7i\n"
	       ,numbrushes, (int)(numbrushes*sizeof(dbrush_t)));
	Verbose("%5i brushsides   %7i\n"
	       ,numbrushsides, (int)(numbrushsides*sizeof(dbrushside_t)));
	Verbose("%5i planes       %7i\n"
	       ,numplanes, (int)(numplanes*sizeof(dplane_t)));
	Verbose("%5i texinfo      %7i\n"
	       ,numtexinfo, (int)(numtexinfo*sizeof(dtexinfo_t)));
	Verbose("%5i entdata      %7i\n", num_entities, entdatasize);

	Verbose("\n");

	Verbose("%5i vertexes     %7i\n"
	       ,numvertexes, (int)(numvertexes*sizeof(dbspvertex_t)));
	Verbose("%5i normals      %7i\n"
		   ,numnormals, (int)(numnormals*sizeof(dbspnormal_t)));
	Verbose("%5i nodes        %7i\n"
	       ,numnodes, (int)(numnodes*sizeof(dnode_t)));
	Verbose("%5i faces        %7i\n"
	       ,numfaces, (int)(numfaces*sizeof(dface_t)));
	Verbose("%5i leafs        %7i\n"
	       ,numleafs, (int)(numleafs*sizeof(dleaf_t)));
	Verbose("%5i leaffaces    %7i\n"
	       ,numleaffaces, (int)(numleaffaces*sizeof(dleaffaces[0])));
	Verbose("%5i leafbrushes  %7i\n"
	       ,numleafbrushes, (int)(numleafbrushes*sizeof(dleafbrushes[0])));
	Verbose("%5i surfedges    %7i\n"
	       ,numsurfedges, (int)(numsurfedges*sizeof(dsurfedges[0])));
	Verbose("%5i edges        %7i\n"
	       ,numedges, (int)(numedges*sizeof(dedge_t)));
	Verbose("      lightdata    %7i\n", lightdatasize);
	Verbose("      visdata      %7i\n", visdatasize);
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
		Error("ParseEpar: token too long\n");
	e->key = CopyString(token);
	GetToken(false);
	if(strlen(token) >= MAX_VALUE - 1)
		Error("ParseEpar: token too long\n");
	e->value = CopyString(token);

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
		Error("ParseEntity: { not found\n");

	if(num_entities == MAX_BSP_ENTITIES)
		Error("num_entities == MAX_BSP_ENTITIES\n");

	mapent = &entities[num_entities];
	num_entities++;

	do {
		if(!GetToken(true))
			Error("ParseEntity: EOF without closing brace\n");
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
		Verbose("Using subdivide %d from worldspawn.\n", subdivide);
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
			Error("Entity text too long\n");
	}
	entdatasize = end - buf + 1;
}

void SetKeyValue(entity_t *ent, const char *key, const char *value){
	epair_t	*ep;

	for(ep = ent->epairs; ep; ep = ep->next){
		if(!strcmp(ep->key, key)){
			Z_Free(ep->value);
			ep->value = CopyString(value);
			return;
		}
	}

	ep = Z_Malloc(sizeof(*ep));
	ep->next = ent->epairs;
	ent->epairs = ep;
	ep->key = CopyString(key);
	ep->value = CopyString(value);
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

