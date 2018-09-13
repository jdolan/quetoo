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

#pragma once

#include "shared.h"

/**
 * @brief Quake2 .wal legacy texture format.
 */
typedef struct {
	char name[32];
	uint32_t width;
	uint32_t height;
	uint32_t offsets[4]; // four mip maps stored
	char anim_name[32]; // next frame in animation chain, not used
	uint32_t flags;
	int32_t contents;
	int32_t value;
} d_wal_t;

/**
 * @brief Quake3 .md3 model format.
 */
#define MD3_HEADER			(('3'<<24)+('P'<<16)+('D'<<8)+'I')
#define MD3_VERSION			15

#define MD3_MAX_LODS		0x4 // per model
#define	MD3_MAX_TRIANGLES	0x2000 // per mesh
#define MD3_MAX_VERTS		0x1000 // per mesh
#define MD3_MAX_SHADERS		0x100 // per mesh
#define MD3_MAX_FRAMES		0x400 // per model
#define	MD3_MAX_MESHES		0x20 // per model
#define MD3_MAX_TAGS		0x10 // per frame
#define MD3_MAX_PATH		0x40 // relative file references
#define MD3_MAX_ANIMATIONS	0x20 // see entity_animation_t
// vertex scales from origin
#define	MD3_XYZ_SCALE		(1.0 / 64)

typedef struct {
	vec2_t st;
} d_md3_texcoord_t;

typedef struct {
	int16_t point[3];
	int16_t norm;
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
	int32_t unused; // shader
} d_md3_skin_t;

typedef struct {
	int32_t id;

	char name[MD3_MAX_PATH];

	int32_t flags;

	int32_t num_frames;
	int32_t num_skins;
	int32_t num_verts;
	int32_t num_tris;

	int32_t ofs_tris;
	int32_t ofs_skins;
	int32_t ofs_tcs;
	int32_t ofs_verts;

	int32_t size;
} d_md3_mesh_t;

typedef struct {
	int32_t id;
	int32_t version;

	char filename[MD3_MAX_PATH];

	int32_t flags;

	int32_t num_frames;
	int32_t num_tags;
	int32_t num_meshes;
	int32_t num_skins;

	int32_t ofs_frames;
	int32_t ofs_tags;
	int32_t ofs_meshes;
	int32_t ofs_end;
} d_md3_t;

/**
 * @brief Represents the data to find and read in a lump from the disk. This is shared
 * between BSP and AAS.
 */
typedef struct {
	int32_t file_ofs;
	int32_t file_len;
} d_bsp_lump_t;

/**
 * @brief .aas format. Under heavy construction.
 */

#define AAS_IDENT (('S' << 24) + ('A' << 16) + ('A' << 8) + 'Q') // "QAAS"
#define AAS_VERSION	1

#define AAS_LUMP_NODES 0
#define AAS_LUMP_PORTALS 1
#define AAS_LUMP_PATHS 2
#define AAS_LUMPS (AAS_LUMP_PATHS + 1)

// AAS uses BSP as a base, so we need cm_bsp
#include "collision/cmodel.h"

typedef struct {
	uint32_t ident;
	uint32_t version;
	d_bsp_lump_t lumps[AAS_LUMPS];
} d_aas_header_t;

#define AAS_PORTAL_WALK		0x1
#define AAS_PORTAL_CROUCH	0x2
#define AAS_PORTAL_STAIR 	0x4
#define AAS_PORTAL_JUMP		0x8
#define AAS_PORTAL_FALL		0x10
#define AAS_PORTAL_IMPASS	0x10000

// portals are polygons that split two nodes
typedef struct {
	uint16_t plane_num; // the plane this portal lives on
	uint16_t nodes[2]; // the nodes this portal connects (front and back)
	uint32_t flags[2]; // the travel flags to cross this portal from either side
} d_aas_portal_t;

#define AAS_INVALID_LEAF INT32_MIN

typedef struct {
	uint16_t plane_num;
	int32_t children[2]; // negative children are leafs, just like BSP
	int16_t mins[3];
	int16_t maxs[3];
	uint16_t first_path;
	uint16_t num_paths;
} d_aas_node_t;

typedef struct {
	uint16_t plane_num;
	int16_t mins[3];
	int16_t maxs[3];
} d_aas_leaf_t;
