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

#include "cm_types.h"

/**
 * @brief BSP file identification.
 */
#define BSP_IDENT (('P' << 24) + ('S' << 16) + ('B' << 8) + 'I') // "IBSP"
#define BSP_VERSION	71

/**
 * @brief BSP file format limits.
 */
#define MAX_BSP_ENTITIES_SIZE		0x40000
#define MAX_BSP_ENTITIES			0x800
#define MAX_BSP_MATERIALS			0x400
#define MAX_BSP_PLANES				0x20000
#define MAX_BSP_BRUSH_SIDES			0x20000
#define MAX_BSP_BRUSHES				0x8000
#define MAX_BSP_VERTEXES			0x80000
#define MAX_BSP_ELEMENTS			0x200000
#define MAX_BSP_FACES				0x20000
#define MAX_BSP_DRAW_ELEMENTS		0x20000
#define MAX_BSP_NODES				0x20000
#define MAX_BSP_LEAF_BRUSHES 		0x20000
#define MAX_BSP_LEAF_FACES			0x20000
#define MAX_BSP_LEAFS				0x20000
#define MAX_BSP_MODELS				0x400
#define MAX_BSP_LIGHTS				0x1000
#define MAX_BSP_LIGHTMAP_SIZE		0x60000000
#define MAX_BSP_LIGHTGRID_SIZE		0x2400000

/**
 * @brief Lightmap luxel size in world units.
 */
#define BSP_LIGHTMAP_LUXEL_SIZE 4

/**
 * @brief Lightmap patch (indirect light) size in luxels.
 */
#define BSP_LIGHTMAP_PATCH_SIZE 8

/**
 * @brief Lightmap diffuse and directional channels.
 */
#define BSP_LIGHTMAP_CHANNELS 2

/**
 * @brief Smallest lightmap atlas width in luxels.
 */
#define MIN_BSP_LIGHTMAP_WIDTH 1024

/**
 * @brief Largest lightmap atlas width in luxels.
 */
#define MAX_BSP_LIGHTMAP_WIDTH 4096

/**
 * @brief The lightmap textures.
 */
typedef enum {
	BSP_LIGHTMAP_FIRST,
	BSP_LIGHTMAP_AMBIENT = BSP_LIGHTMAP_FIRST,
	BSP_LIGHTMAP_DIFFUSE_0,
	BSP_LIGHTMAP_DIRECTION_0,
	BSP_LIGHTMAP_DIFFUSE_1,
	BSP_LIGHTMAP_DIRECTION_1,
	BSP_LIGHTMAP_CAUSTICS,
	BSP_LIGHTMAP_LAST,
} bsp_lightmap_texture_t;

/**
 * @brief Lightgrid luxel size in world units.
 */
#define BSP_LIGHTGRID_LUXEL_SIZE 32

/**
 * @brief Largest lightgrid width in luxels (8192 / 32 = 256).
 */
#define MAX_BSP_LIGHTGRID_WIDTH (MAX_WORLD_AXIAL / BSP_LIGHTGRID_LUXEL_SIZE)

/**
 * @brief Largest lightgrid texture size in luxels.
 */
#define MAX_BSP_LIGHTGRID_LUXELS (MAX_BSP_LIGHTGRID_WIDTH * MAX_BSP_LIGHTGRID_WIDTH * MAX_BSP_LIGHTGRID_WIDTH)

/**
 * @brief The lightgrid textures.
 */
typedef enum {
	BSP_LIGHTGRID_FIRST,
	BSP_LIGHTGRID_AMBIENT = BSP_LIGHTGRID_FIRST,
	BSP_LIGHTGRID_DIFFUSE,
	BSP_LIGHTGRID_DIRECTION,
	BSP_LIGHTGRID_CAUSTICS,
	BSP_LIGHTGRID_FOG,
	BSP_LIGHTGRID_LAST
} bsp_lightgrid_texture_t;

/**
 * @brief BSP file format lump identifiers.
 */
typedef enum {
	BSP_LUMP_FIRST,
	BSP_LUMP_ENTITIES = BSP_LUMP_FIRST,
	BSP_LUMP_MATERIALS,
	BSP_LUMP_PLANES,
	BSP_LUMP_BRUSH_SIDES,
	BSP_LUMP_BRUSHES,
	BSP_LUMP_VERTEXES,
	BSP_LUMP_ELEMENTS,
	BSP_LUMP_FACES,
	BSP_LUMP_DRAW_ELEMENTS,
	BSP_LUMP_NODES,
	BSP_LUMP_LEAF_BRUSHES,
	BSP_LUMP_LEAF_FACES,
	BSP_LUMP_LEAFS,
	BSP_LUMP_MODELS,
	BSP_LUMP_LIGHTS,
	BSP_LUMP_LIGHTMAP,
	BSP_LUMP_LIGHTGRID,
	BSP_LUMP_LAST
} bsp_lump_id_t;

#define BSP_LUMPS_ALL ((1 << BSP_LUMP_LAST) - 1)

/**
 * @brief Represents the data to find and read in a lump from the disk.
 */
typedef struct {
	int32_t file_ofs;
	int32_t file_len;
} bsp_lump_t;

/**
 * @brief Represents the header of a BSP file.
 */
typedef struct {
	int32_t ident;
	int32_t version;
	bsp_lump_t lumps[BSP_LUMP_LAST];
} bsp_header_t;

/**
 * @brief Material references.
 */
typedef struct {
	char name[MAX_QPATH];
} bsp_material_t;

/**
 * @brief Planes are stored in opposing pairs, with positive normal vectors first in each pair.
 */
typedef struct {
	vec3_t normal;
	float dist;
} bsp_plane_t;

/**
 * @brief Sentinel value for brush sides created from BSP.
 */
#define BSP_MATERIAL_NODE -1

/**
 * @brief Brush sides are defined by map input, and by BSP tree generation. Map brushes
 * may be split into multiple BSP brushes, producing new sides where they are split.
 * Non-axial map brushes are also "beveled" to optimize collision detection.
 */
typedef struct {
	int32_t plane; // facing out of the leaf
	int32_t material;
	vec4_t axis[2]; // [s/t][xyz + offset]
	int32_t contents;
	int32_t surface;
	int32_t value; // light emission, etc
} bsp_brush_side_t;

/**
 * @brief Brushes are convex volumes defined by four or more clipping planes.
 */
typedef struct {
	int32_t entity; // the entity that defined this brush
	int32_t contents;
	int32_t first_brush_side;
	int32_t num_brush_sides; // the number of total brush sides, including bevel sides
	box3_t bounds;
} bsp_brush_t;

typedef struct {
	vec3_t position;
	vec3_t normal;
	vec3_t tangent;
	vec3_t bitangent;
	vec2_t diffusemap;
	vec2_t lightmap;
	color32_t color;
} bsp_vertex_t;

/**
 * @brief Face lightmaps contain atlas offsets and dimensions.
 */
typedef struct {
	int32_t s, t;
	int32_t w, h;

	vec2_t st_mins, st_maxs;
	mat4_t matrix;
} bsp_face_lightmap_t;

/**
 * @brief Faces are polygon primitives, stored as both vertex and element arrays.
 */
typedef struct {
	int32_t brush_side; // the brush side that produced this face
	int32_t plane; // the plane; for translucent brushes, this may be the negation of the side plane

	box3_t bounds;

	int32_t first_vertex;
	int32_t num_vertexes;

	int32_t first_element;
	int32_t num_elements;

	bsp_face_lightmap_t lightmap;
} bsp_face_t;

typedef struct {
	int32_t plane;
	int32_t children[2]; // negative numbers are -(leafs + 1), not nodes

	box3_t bounds; // for collision
	box3_t visible_bounds; // for frustum culling

	int32_t first_face;
	int32_t num_faces; // counting both sides
} bsp_node_t;

typedef struct {
	int32_t contents; // OR of all brushes
	int32_t cluster;

	box3_t bounds; // for collision
	box3_t visible_bounds; // for frustum culling

	int32_t first_leaf_face;
	int32_t num_leaf_faces;

	int32_t first_leaf_brush;
	int32_t num_leaf_brushes;
} bsp_leaf_t;

/**
 * @brief Draw elements are OpenGL draw commands, serialized directly within the BSP.
 * @details For each model, all opaque faces sharing material and contents are grouped
 * into a single draw elements. All blend faces sharing plane, material and contents
 * are also grouped.
 */
typedef struct {
	int32_t plane;
	int32_t material;
	int32_t surface;

	box3_t bounds;

	int32_t first_element;
	int32_t num_elements;
} bsp_draw_elements_t;

typedef struct {
	int32_t entity;
	int32_t head_node;

	box3_t bounds;

	int32_t first_face;
	int32_t num_faces;

	int32_t first_draw_elements;
	int32_t num_draw_elements;
} bsp_model_t;

typedef enum {
	LIGHT_INVALID  = 0x0,
	LIGHT_AMBIENT  = 0x1,
	LIGHT_SUN      = 0x2,
	LIGHT_POINT    = 0x4,
	LIGHT_SPOT     = 0x8,
	LIGHT_FACE     = 0x10,
	LIGHT_PATCH    = 0x20,
	LIGHT_DYNAMIC  = 0x40,
} light_type_t;

typedef enum {
	LIGHT_ATTEN_NONE,
	LIGHT_ATTEN_LINEAR,
	LIGHT_ATTEN_INVERSE_SQUARE,
} light_atten_t;

/**
 * @brief BSP representation of light sources.
 */
typedef struct {
	light_type_t type;
	light_atten_t atten;
	vec3_t origin;
	vec3_t color;
	vec3_t normal;
	float radius;
	float size;
	float intensity;
	float shadow;
	float cone;
	float falloff;
	box3_t bounds;
} bsp_light_t;

/**
 * @brief Lightmaps are atlas-packed, layered texture objects of variable size.
 * @details Each layer stores either a color or a directional vector.
 */
typedef struct {
	int32_t width;
} bsp_lightmap_t;

/**
 * @brief Lightgrids are layered 3D texture objects of variable size.
 * @details Each layer is up to 256x256x256.
 */
typedef struct {
	vec3i_t size;
} bsp_lightgrid_t;

/**
 * @brief BSP file lumps in their native file formats. The data is stored as pointers
 * so that we don't take up an ungodly amount of space (285 MB of memory!).
 * You can safely edit the data within the boundaries of the num_x values, but
 * if you want to expand the space required use the Bsp_* functions.
 */
typedef struct bsp_file_s {
	int32_t entity_string_size;
	char *entity_string;

	int32_t num_materials;
	bsp_material_t *materials;

	int32_t num_planes;
	bsp_plane_t *planes;

	int32_t num_brush_sides;
	bsp_brush_side_t *brush_sides;

	int32_t num_brushes;
	bsp_brush_t *brushes;

	int32_t num_vertexes;
	bsp_vertex_t *vertexes;

	int32_t num_elements;
	int32_t *elements;

	int32_t num_faces;
	bsp_face_t *faces;

	int32_t num_draw_elements;
	bsp_draw_elements_t *draw_elements;

	int32_t num_nodes;
	bsp_node_t *nodes;

	int32_t num_leaf_brushes;
	int32_t *leaf_brushes;

	int32_t num_leaf_faces;
	int32_t *leaf_faces;

	int32_t num_leafs;
	bsp_leaf_t *leafs;

	int32_t num_models;
	bsp_model_t *models;

	int32_t num_lights;
	bsp_light_t *lights;

	int32_t lightmap_size;
	bsp_lightmap_t *lightmap;

	int32_t lightgrid_size;
	bsp_lightgrid_t *lightgrid;

	bsp_lump_id_t loaded_lumps;
} bsp_file_t;

int32_t Bsp_Verify(const bsp_header_t *file);
int64_t Bsp_Size(const bsp_header_t *file);
_Bool Bsp_LumpLoaded(const bsp_file_t *bsp, const bsp_lump_id_t lump_id);
void Bsp_UnloadLump(bsp_file_t *bsp, const bsp_lump_id_t lump_id);
void Bsp_UnloadLumps(bsp_file_t *bsp, const bsp_lump_id_t lump_bits);
_Bool Bsp_LoadLump(const bsp_header_t *file, bsp_file_t *bsp, const bsp_lump_id_t lump_id);
_Bool Bsp_LoadLumps(const bsp_header_t *file, bsp_file_t *bsp, const bsp_lump_id_t lump_bits);
void Bsp_AllocLump(bsp_file_t *bsp, const bsp_lump_id_t lump_id, const size_t count);
void Bsp_Write(file_t *file, const bsp_file_t *bsp);
