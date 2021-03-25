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

#include "shared/shared.h"

/**
 * @brief Quake3 .md3 model format.
 */
#define MD3_ID				(('3'<<24)+('P'<<16)+('D'<<8)+'I')
#define MD3_VERSION			15

#define MD3_MAX_LODS		0x4 // per model
#define	MD3_MAX_TRIANGLES	0x2000 // per mesh
#define MD3_MAX_VERTEXES	0x1000 // per mesh
#define MD3_MAX_SHADERS		0x100 // per mesh
#define MD3_MIN_FRAMES		0x1 // per model
#define MD3_MAX_FRAMES		0x400 // per model
#define	MD3_MAX_SURFACES	0x20 // per model
#define MD3_MAX_TAGS		0x10 // per frame
#define MD3_MAX_PATH		0x40 // relative file references
#define MD3_MAX_ANIMATIONS	0x20 // see entity_animation_t
// vertex scales from origin
#define	MD3_XYZ_SCALE		(1.0 / 64)

typedef struct {
	vec2_t st;
} d_md3_texcoord_t;

typedef struct {
	vec3s_t point;
	int16_t norm;
} d_md3_vertex_t;

typedef struct {
	uint32_t indexes[3];
} d_md3_triangle_t;

typedef struct {
	box3_t bounds;
	vec3_t translate;
	float radius;
	char name[16];
} d_md3_frame_t;

typedef struct {
	char name[MD3_MAX_PATH];
	vec3_t origin;
	vec3_t axis[3];
} d_md3_tag_t;

typedef struct {
	char name[MD3_MAX_PATH];
	int32_t index;
} d_md3_shader_t;

typedef struct {
	int32_t id;

	char name[MD3_MAX_PATH];

	int32_t flags;

	int32_t num_frames;
	int32_t num_shaders;
	int32_t num_vertexes;
	int32_t num_triangles;

	int32_t ofs_triangles;
	int32_t ofs_shaders;
	int32_t ofs_texcoords;
	int32_t ofs_vertexes;
	int32_t ofs_end;
} d_md3_surface_t;

typedef struct {
	int32_t id;
	int32_t version;

	char filename[MD3_MAX_PATH];

	int32_t flags;

	int32_t num_frames;
	int32_t num_tags;
	int32_t num_surfaces;
	int32_t num_shaders;

	int32_t ofs_frames;
	int32_t ofs_tags;
	int32_t ofs_surfaces;
	int32_t ofs_end;
} d_md3_t;
