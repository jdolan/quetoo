/*
* Copyright(c) 1997-2001 Id Software, Inc.
* Copyright(c) 2002 The Quakeforge Project.
* Copyright(c) 2006 Quake2World.
*
* This program is free software; you can redistribute it and/or
* modify it under the terms of the GNU General Public License
* as published by the Free Software Foundation; either version 2
* of the License, or(at your option) any later version.
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

#ifndef __FILES_H__
#define __FILES_H__

/*

.MD2 triangle model file format

*/

#define MD2_HEADER			(('2'<<24) + ('P'<<16) + ('D'<<8) + 'I')
#define MD2_VERSION			8

#define MD2_MAX_TRIANGLES	4096
#define MD2_MAX_VERTS		2048
#define MD2_MAX_FRAMES		512
#define MD2_MAX_SKINS		32
#define MD2_MAX_SKINNAME	64

typedef struct {
	short index_xyz[3];
	short index_st[3];
} dmd2triangle_t;

typedef struct {
	byte v[3];  // vertex scaled to fit in frame mins/maxs
	byte n;  // normal index into anorms.h
} dmd2vertex_t;

typedef struct {
	short s;  // divide by skin dimensions for actual coords
	short t;
} dmd2texcoord_t;

typedef struct {
	vec3_t scale;  // multiply byte verts by this
	vec3_t translate;  // then add this
	char name[16];  // frame name from grabbing
	dmd2vertex_t verts[1];  // variable sized
} dmd2frame_t;

// the glcmd format:
// a positive integer starts a tristrip command, followed by that many
// vertex structures.
// a negative integer starts a trifan command, followed by -x vertexes
// a zero indicates the end of the command list.
// a vertex consists of a floating point s, a floating point t,
// and an integer vertex index.

typedef struct {
	int ident;
	int version;

	int skinwidth;
	int skinheight;
	int framesize;  // byte size of each frame

	int num_skins;
	int num_xyz;
	int num_st;  // greater than num_xyz for seams
	int num_tris;
	int num_glcmds;  // dwords in strip/fan command list
	int num_frames;

	int ofs_skins;  // each skin is a MAX_SKINNAME string
	int ofs_st;  // byte offset from start for stverts
	int ofs_tris;  // offset for dtriangles
	int ofs_frames;  // offset for first frame
	int ofs_glcmds;
	int ofs_end;  // end of file
} dmd2_t;


/*

  .MD3 model format

*/


#define MD3_HEADER			(('3'<<24)+('P'<<16)+('D'<<8)+'I')
#define MD3_VERSION			15

#define MD3_MAX_LODS		4  // per model
#define	MD3_MAX_TRIANGLES	8192  // per mesh
#define MD3_MAX_VERTS		4096  // per mesh
#define MD3_MAX_SHADERS		256  // per mesh
#define MD3_MAX_FRAMES		1024  // per model
#define	MD3_MAX_MESHES		32  // per model
#define MD3_MAX_TAGS		16  // per frame
#define MD3_MAX_PATH		64

// vertex scales from origin
#define	MD3_XYZ_SCALE		(1.0 / 64)

typedef struct {
	vec2_t st;
} dmd3coord_t;

typedef struct {
	short point[3];
	short norm;
} dmd3vertex_t;

typedef struct {
	vec3_t mins;
	vec3_t maxs;
	vec3_t translate;
	vec_t radius;
	char creator[16];
} dmd3frame_t;

typedef struct {
	vec3_t origin;
	vec3_t axis[3];
} dmd3orientation_t;

typedef struct {
	char name[MD3_MAX_PATH];
	dmd3orientation_t orient;
} dmd3tag_t;

typedef struct {
	char name[MD3_MAX_PATH];
	int unused;  // shader
} dmd3skin_t;

typedef struct {
	int id;

	char name[MD3_MAX_PATH];

	int flags;

	int num_frames;
	int num_skins;
	int num_verts;
	int num_tris;

	int ofs_tris;
	int ofs_skins;
	int ofs_tcs;
	int ofs_verts;

	int meshsize;
} dmd3mesh_t;

typedef struct {
	int id;
	int version;

	char filename[MD3_MAX_PATH];

	int flags;

	int num_frames;
	int num_tags;
	int num_meshes;
	int num_skins;

	int ofs_frames;
	int ofs_tags;
	int ofs_meshes;
	int ofs_end;
} dmd3_t;


/*

  .BSP file format

*/

#define BSP_HEADER	(('P'<<24)+('S'<<16)+('B'<<8)+'I')
// little-endian "IBSP"

#define BSP_VERSION	38
#define BSP_VERSION_ 69  // haha, 69..

// upper design bounds
// leaffaces, leafbrushes, planes, and verts are still bounded by
// 16 bit short limits
#define MAX_BSP_MODELS		1024
#define MAX_BSP_BRUSHES		16384
#define MAX_BSP_ENTITIES	2048
#define MAX_BSP_ENTSTRING	0x40000
#define MAX_BSP_TEXINFO		16384

#define MAX_BSP_AREAS		256
#define MAX_BSP_AREAPORTALS	1024
#define MAX_BSP_PLANES		65536
#define MAX_BSP_NODES		65536
#define MAX_BSP_BRUSHSIDES	65536
#define MAX_BSP_LEAFS		65536
#define MAX_BSP_VERTS		65536
#define MAX_BSP_FACES		65536
#define MAX_BSP_LEAFFACES	65536
#define MAX_BSP_LEAFBRUSHES 65536
#define MAX_BSP_PORTALS		65536
#define MAX_BSP_EDGES		128000
#define MAX_BSP_SURFEDGES	256000
#define MAX_BSP_LIGHTING	0x2000000  // increased from 0x200000
#define MAX_BSP_LIGHTMAP	(512 * 512)
#define MAX_BSP_VISIBILITY	0x400000 // increased from 0x100000

// key / value pair sizes

#define MAX_KEY		32
#define MAX_VALUE	1024

typedef struct {
	int fileofs, filelen;
} lump_t;

#define LUMP_ENTITIES		0
#define LUMP_PLANES			1
#define LUMP_VERTEXES		2
#define LUMP_VISIBILITY		3
#define LUMP_NODES			4
#define LUMP_TEXINFO		5
#define LUMP_FACES			6
#define LUMP_LIGHTING		7
#define LUMP_LEAFS			8
#define LUMP_LEAFFACES		9
#define LUMP_LEAFBRUSHES	10
#define LUMP_EDGES			11
#define LUMP_SURFEDGES		12
#define LUMP_MODELS			13
#define LUMP_BRUSHES		14
#define LUMP_BRUSHSIDES		15
#define LUMP_POP			16
#define LUMP_AREAS			17
#define LUMP_AREAPORTALS	18
#define LUMP_NORMALS		19  // new for q2w
#define HEADER_LUMPS		20

typedef struct {
	int ident;
	int version;
	lump_t lumps[HEADER_LUMPS];
} dbspheader_t;

typedef struct {
	vec3_t mins, maxs;
	vec3_t origin;  // for sounds or lights
	int headnode;
	int firstface, numfaces;  // submodels just draw faces
	// without walking the bsp tree
} dbspmodel_t;

typedef struct {
	vec3_t point;
} dbspvertex_t;

typedef struct {
	vec3_t normal;
} dbspnormal_t;

// if a brush just barely pokes onto the other side,
// let it slide by without chopping
#define	PLANESIDE_EPSILON	0.001

#define	PSIDE_FRONT		1
#define	PSIDE_BACK		2
#define	PSIDE_BOTH		(PSIDE_FRONT|PSIDE_BACK)
#define	PSIDE_FACING	4

// 0-2 are axial planes
#define PLANE_X			0
#define PLANE_Y			1
#define PLANE_Z			2

#define AXIAL(p) ((p)->type < PLANE_ANYX)

// 3-5 are non-axial planes snapped to the nearest
#define PLANE_ANYX		3
#define PLANE_ANYY		4
#define PLANE_ANYZ		5
#define PLANE_NONE		6

// lightmap information is 1/n texture resolution
#define DEFAULT_LIGHTMAP_SCALE 16

// planes (x & ~1) and (x & ~1) + 1 are always opposites

typedef struct {
	float normal[3];
	float dist;
	int type;  // PLANE_X - PLANE_ANYZ ?remove? trivial to regenerate
} dplane_t;

typedef struct {
	int planenum;
	int children[2];  // negative numbers are -(leafs+1), not nodes
	short mins[3];  // for frustum culling
	short maxs[3];
	unsigned short firstface;
	unsigned short numfaces;  // counting both sides
} dnode_t;

typedef struct {
	float vecs[2][4];  // [s/t][xyz offset]
	int flags;  // surface values
	int value;  // light emission, etc
	char texture[32];  // texture name (textures/*.tga)
	int nexttexinfo;  // no longer used, here to maintain compatibility
} dtexinfo_t;

// note that edge 0 is never used, because negative edge nums are used for
// counterclockwise use of the edge in a face
typedef struct {
	unsigned short v[2];  // vertex numbers
} dedge_t;

typedef struct {
	unsigned short planenum;
	short side;

	int firstedge;  // we must support > 64k edges
	short numedges;
	short texinfo;

	byte unused[4];  // was lightstyles
	int lightofs;  // start of samples in lighting lump
} dface_t;

typedef struct {
	int contents;  // OR of all brushes(not needed?)

	short cluster;
	short area;

	short mins[3];  // for frustum culling
	short maxs[3];

	unsigned short firstleafface;
	unsigned short numleaffaces;

	unsigned short firstleafbrush;
	unsigned short numleafbrushes;
} dleaf_t;

typedef struct {
	unsigned short planenum;  // facing out of the leaf
	unsigned short surfnum;
} dbrushside_t;

typedef struct {
	int firstside;
	int numsides;
	int contents;
} dbrush_t;

// the visibility lump consists of a header with a count, then
// byte offsets for the PVS and PHS of each cluster, then the raw
// compressed bit vectors
#define DVIS_PVS	0
#define DVIS_PHS	1
typedef struct {
	int numclusters;
	int bitofs[8][2];  // bitofs[numclusters][2]
} dvis_t;

// each area has a list of portals that lead into other areas
// when portals are closed, other areas may not be visible or
// hearable even if the vis info says that it should be
typedef struct {
	int portalnum;
	int otherarea;
} dareaportal_t;

typedef struct {
	int numareaportals;
	int firstareaportal;
} darea_t;

#endif /*__FILES_H__*/
