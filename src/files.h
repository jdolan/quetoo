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

#ifndef __FILES_H__
#define __FILES_H__

#include "shared.h"


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
#define MD3_MAX_PATH		64  // relative file references
#define MD3_MAX_ANIMATIONS	32  // see entity_animation_t

// vertex scales from origin
#define	MD3_XYZ_SCALE		(1.0 / 64)

typedef struct {
	vec2_t st;
} d_md3_texcoord_t;

typedef struct {
	short point[3];
	short norm;
} d_md3_vertex_t;

typedef struct {
	vec3_t mins;
	vec3_t maxs;
	vec3_t translate;
	vec_t radius;
	char name[16];
} d_md3_frame_t;

typedef struct {
	vec3_t origin;
	vec3_t axis[3];
} d_md3_orientation_t;

typedef struct {
	char name[MD3_MAX_PATH];
	d_md3_orientation_t orient;
} d_md3_tag_t;

typedef struct {
	char name[MD3_MAX_PATH];
	int unused;  // shader
} d_md3_skin_t;

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

	int size;
} d_md3_mesh_t;

typedef struct {
	int id;
	int version;

	char file_name[MD3_MAX_PATH];

	int flags;

	int num_frames;
	int num_tags;
	int num_meshes;
	int num_skins;

	int ofs_frames;
	int ofs_tags;
	int ofs_meshes;
	int ofs_end;
} d_md3_t;


/*

  .BSP file format

*/

#define BSP_HEADER	(('P'<<24)+('S'<<16)+('B'<<8)+'I')
// little-endian "IBSP"

#define BSP_VERSION	38
#define BSP_VERSION_ 69  // haha, 69..

// upper design bounds
// leaf faces, leaf brushes, planes, and vertexes are still bounded by
// 16 bit short limits
#define MAX_BSP_MODELS			1024
#define MAX_BSP_BRUSHES			16384
#define MAX_BSP_ENTITIES		2048
#define MAX_BSP_ENT_STRING		0x40000
#define MAX_BSP_TEXINFO			16384

#define MAX_BSP_AREAS			256
#define MAX_BSP_AREA_PORTALS	1024
#define MAX_BSP_PLANES			65536
#define MAX_BSP_NODES			65536
#define MAX_BSP_BRUSH_SIDES		65536
#define MAX_BSP_LEAFS			65536
#define MAX_BSP_VERTS			65536
#define MAX_BSP_FACES			65536
#define MAX_BSP_LEAF_FACES		65536
#define MAX_BSP_LEAF_BRUSHES 	65536
#define MAX_BSP_PORTALS			65536
#define MAX_BSP_EDGES			128000
#define MAX_BSP_FACE_EDGES		256000
#define MAX_BSP_LIGHTING		0x10000000  // increased from 0x200000
#define MAX_BSP_LIGHTMAP		(512 * 512)
#define MAX_BSP_VISIBILITY		0x400000 // increased from 0x100000

// key / value pair sizes

#define MAX_KEY		32
#define MAX_VALUE	1024

typedef struct {
	int file_ofs, file_len;
} d_bsp_lump_t;

#define LUMP_ENTITIES		0
#define LUMP_PLANES			1
#define LUMP_VERTEXES		2
#define LUMP_VISIBILITY		3
#define LUMP_NODES			4
#define LUMP_TEXINFO		5
#define LUMP_FACES			6
#define LUMP_LIGHMAPS		7
#define LUMP_LEAFS			8
#define LUMP_LEAF_FACES		9
#define LUMP_LEAF_BRUSHES	10
#define LUMP_EDGES			11
#define LUMP_FACE_EDGES		12
#define LUMP_MODELS			13
#define LUMP_BRUSHES		14
#define LUMP_BRUSH_SIDES	15
#define LUMP_POP			16
#define LUMP_AREAS			17
#define LUMP_AREA_PORTALS	18
#define LUMP_NORMALS		19  // new for q2w
#define HEADER_LUMPS		20

typedef struct {
	int ident;
	int version;
	d_bsp_lump_t lumps[HEADER_LUMPS];
} d_bsp_header_t;

typedef struct {
	vec3_t mins, maxs;
	vec3_t origin;  // for sounds or lights
	int head_node;
	int first_face, num_faces;  // submodels just draw faces
	// without walking the bsp tree
} d_bsp_model_t;

typedef struct {
	vec3_t point;
} d_bsp_vertex_t;

typedef struct {
	vec3_t normal;
} d_bsp_normal_t;

// if a brush just barely pokes onto the other side,
// let it slide by without chopping
#define	SIDE_EPSILON	0.001

#define	SIDE_FRONT		1
#define	SIDE_BACK		2
#define	SIDE_BOTH		3
#define	SIDE_FACING		4

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
} d_bsp_plane_t;

typedef struct {
	int plane_num;
	int children[2];  // negative numbers are -(leafs+1), not nodes
	short mins[3];  // for frustum culling
	short maxs[3];
	unsigned short first_face;
	unsigned short num_faces;  // counting both sides
} d_bsp_node_t;

typedef struct {
	float vecs[2][4];  // [s/t][xyz offset]
	int flags;  // surface values
	int value;  // light emission, etc
	char texture[32];  // texture name (textures/*.tga)
	int next_texinfo;  // no longer used, here to maintain compatibility
} d_bsp_texinfo_t;

// note that edge 0 is never used, because negative edge nums are used for
// counterclockwise use of the edge in a face
typedef struct {
	unsigned short v[2];  // vertex numbers
} d_bsp_edge_t;

typedef struct {
	unsigned short plane_num;
	short side;

	int first_edge;  // we must support > 64k edges
	short num_edges;
	short texinfo;

	byte unused[4];  // was light styles
	int light_ofs;  // start of samples in lighting lump
} d_bsp_face_t;

typedef struct {
	int contents;  // OR of all brushes(not needed?)

	short cluster;
	short area;

	short mins[3];  // for frustum culling
	short maxs[3];

	unsigned short first_leaf_face;
	unsigned short num_leaf_faces;

	unsigned short first_leaf_brush;
	unsigned short num_leaf_brushes;
} d_bsp_leaf_t;

typedef struct {
	unsigned short plane_num;  // facing out of the leaf
	unsigned short surf_num;
} d_bsp_brush_side_t;

typedef struct {
	int first_side;
	int num_sides;
	int contents;
} d_bsp_brush_t;

// the visibility lump consists of a header with a count, then
// byte offsets for the PVS and PHS of each cluster, then the raw
// compressed bit vectors
#define DVIS_PVS	0
#define DVIS_PHS	1
typedef struct {
	int num_clusters;
	int bit_offsets[8][2];  // bit_offsets[num_clusters][2]
} d_bsp_vis_t;

// each area has a list of portals that lead into other areas
// when portals are closed, other areas may not be visible or
// hearable even if the vis info says that it should be
typedef struct {
	int portal_num;
	int other_area;
} d_bsp_area_portal_t;

typedef struct {
	int num_area_portals;
	int first_area_portal;
} d_bsp_area_t;

#endif /*__FILES_H__*/
