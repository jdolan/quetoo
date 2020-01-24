#pragma once

#include "filesystem.h"

#include "cm_types.h"

/**
 * @brief BSP file identification.
 */
#define BSP_IDENT (('P' << 24) + ('S' << 16) + ('B' << 8) + 'I') // "IBSP"
#define BSP_VERSION	71

/**
 * @brief BSP file format limits.
 */
#define MAX_BSP_ENT_STRING		0x40000
#define MAX_BSP_ENTITIES		0x800
#define MAX_BSP_TEXINFO			0x4000
#define MAX_BSP_PLANES			0x20000
#define MAX_BSP_NODES			0x20000
#define MAX_BSP_LEAFS			0x20000
#define MAX_BSP_LEAF_FACES		0x20000
#define MAX_BSP_LEAF_BRUSHES 	0x20000
#define MAX_BSP_DRAW_ELEMENTS	0x20000
#define MAX_BSP_BRUSHES			0x8000
#define MAX_BSP_BRUSH_SIDES		0x20000
#define MAX_BSP_VERTEXES		0x80000
#define MAX_BSP_ELEMENTS		0x200000
#define MAX_BSP_FACES			0x20000
#define MAX_BSP_MODELS			0x400
#define MAX_BSP_AREA_PORTALS	0x400
#define MAX_BSP_AREAS			0x100
#define MAX_BSP_PORTALS			0x20000
#define MAX_BSP_VISIBILITY		0x200000
#define MAX_BSP_LIGHTS			0x1000
#define MAX_BSP_LIGHTMAPS		0x10
#define MAX_BSP_LIGHTGRID		0x1200000

/**
 * @brief Lightmap luxel size, in world units.
 */
#define DEFAULT_BSP_LIGHTMAP_LUXEL_SIZE 4

/**
 * @brief Largest single lightmap allowed for a face, in luxels.
 */
#define MAX_BSP_LIGHTMAP_LUXELS (1024 * 1024)

/**
 * @brief Lightmap atlas width and height.
 */
#define BSP_LIGHTMAP_WIDTH 2048

/**
 * @brief Lightmap atlas bytes per pixel.
 */
#define BSP_LIGHTMAP_BPP 3

/**
 * @brief Lightmap atlas size in bytes.
 */
#define BSP_LIGHTMAP_LAYER_SIZE (BSP_LIGHTMAP_WIDTH * BSP_LIGHTMAP_WIDTH * BSP_LIGHTMAP_BPP)

/**
 * @brief Lightmap and deluxemap.
 */
#define BSP_LIGHTMAP_LAYERS 2

/**
 * @brief Lightmap layered atlas size in bytes.
 */
#define BSP_LIGHTMAP_SIZE (BSP_LIGHTMAP_SIZE * BSP_LIGHTMAP_LAYERS)

/**
 * @brief Lightgrid luxel size in world units.
 */
#define BSP_LIGHTGRID_LUXEL_SIZE 64

/**
 * @brief Lightgrid bytes per pixel.
 */
#define BSP_LIGHTGRID_BPP 3

/**
 * @brief Lightgrid ambient, diffuse and direction textures.
 */
#define BSP_LIGHTGRID_TEXTURES 3

/**
 * @brief Largest lightgrid width in luxels (8192 / 64 = 128).
 */
#define MAX_BSP_LIGHTGRID_WIDTH (MAX_WORLD_AXIAL / BSP_LIGHTGRID_LUXEL_SIZE)

/**
 * @brief Largest lightgrid texture size in luxels.
 */
#define MAX_BSP_LIGHTGRID_LUXELS (MAX_BSP_LIGHTGRID_WIDTH * MAX_BSP_LIGHTGRID_WIDTH * MAX_BSP_LIGHTGRID_WIDTH)

/**
 * @brief BSP file format lump identifiers.
 */
typedef enum {
	BSP_LUMP_FIRST,
	BSP_LUMP_ENTITIES = BSP_LUMP_FIRST,
	BSP_LUMP_TEXINFO,
	BSP_LUMP_PLANES,
	BSP_LUMP_BRUSH_SIDES,
	BSP_LUMP_BRUSHES,
	BSP_LUMP_VERTEXES,
	BSP_LUMP_ELEMENTS,
	BSP_LUMP_FACES,
	BSP_LUMP_NODES,
	BSP_LUMP_LEAF_BRUSHES,
	BSP_LUMP_LEAF_FACES,
	BSP_LUMP_LEAFS,
	BSP_LUMP_MODELS,
	BSP_LUMP_AREA_PORTALS,
	BSP_LUMP_AREAS,
	BSP_LUMP_VISIBILITY,
	BSP_LUMP_LIGHTMAPS,
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

typedef struct {
	vec4_t vecs[2]; // [s/t][xyz offset]
	int32_t flags; // SURF_* flags
	int32_t value; // light emission, etc
	char texture[32]; // texture name (e.g. torn/metal1)
} bsp_texinfo_t;

// planes (x & ~1) and (x & ~1) + 1 are always opposites

typedef struct {
	vec3_t normal;
	vec_t dist;
} bsp_plane_t;

typedef struct {
	int32_t plane_num; // facing out of the leaf
	int16_t texinfo;
} bsp_brush_side_t;

typedef struct {
	int32_t first_brush_side;
	int32_t num_sides;
	int32_t contents;
} bsp_brush_t;

typedef struct {
	vec3_t position;
	vec3_t normal;
	vec3_t tangent;
	vec3_t bitangent;
	vec2_t diffuse;
	vec2_t lightmap;
	int16_t texinfo;
} bsp_vertex_t;

typedef struct {
	int32_t plane_num;
	int16_t texinfo;

	int32_t first_vertex; // vertex array for polygon or triangle fan
	int32_t num_vertexes;

	int32_t first_element; // element array for triangles
	int32_t num_elements;

	struct {
		int32_t num;
		int32_t s, t;
		int32_t w, h;
	} lightmap;
} bsp_face_t;

typedef struct {
	int32_t plane_num;
	int32_t children[2]; // negative numbers are -(leafs+1), not nodes
	int16_t mins[3]; // for frustum culling
	int16_t maxs[3];
	int32_t first_face;
	int32_t num_faces; // counting both sides
} bsp_node_t;

typedef struct {
	int32_t contents; // OR of all brushes (not needed?)

	int32_t cluster;
	int32_t area;

	int16_t mins[3]; // for frustum culling
	int16_t maxs[3];

	int32_t first_leaf_face;
	int32_t num_leaf_faces;

	int32_t first_leaf_brush;
	int32_t num_leaf_brushes;
} bsp_leaf_t;

typedef struct {
	vec3_t mins, maxs;
	vec3_t origin; // for sounds or lights
	int32_t head_node;
	int32_t first_face, num_faces; // inline models just draw faces without walking the bsp tree
} bsp_model_t;

// each area has a list of portals that lead into other areas
// when portals are closed, other areas may not be visible or
// hearable even if the vis info says that it should be
typedef struct {
	int32_t portal_num;
	int32_t other_area;
} bsp_area_portal_t;

typedef struct {
	int32_t num_area_portals;
	int32_t first_area_portal;
} bsp_area_t;

// the visibility lump consists of a header with a count, then
// byte offsets for the PVS and PHS of each cluster, then the raw
// compressed bit vectors
#define VIS_PVS	0
#define VIS_PHS	1

typedef struct {
	int32_t num_clusters;
	int32_t bit_offsets[8][2]; // bit_offsets[num_clusters][2]
} bsp_vis_t;

typedef enum {
	LIGHT_INVALID = -1,
	LIGHT_AMBIENT,
	LIGHT_SUN,
	LIGHT_POINT,
	LIGHT_SPOT,
	LIGHT_PATCH,
	LIGHT_INDIRECT
} bsp_light_type_t;

typedef enum {
	LIGHT_ATTEN_NONE,
	LIGHT_ATTEN_LINEAR,
	LIGHT_ATTEN_INVERSE_SQUARE,
} bsp_light_atten_t;

typedef struct {
	bsp_light_type_t type;
	bsp_light_atten_t atten;
	vec3_t origin;
	vec3_t color;
	vec3_t normal;
	vec_t radius;
	vec_t theta;
} bsp_light_t;

/**
 * @brief Lightmaps are atlas-packed, layered 24 bit texture objects, inlined in the BSP.
 * @details Each layer is 2048x2048 RGB at 24bpp. The first layer contains diffuse light
 * color, while the second layer contains diffuse light direction.
 */
typedef struct {
	byte layers[BSP_LIGHTMAP_LAYERS][BSP_LIGHTMAP_LAYER_SIZE];
} bsp_lightmap_t;

/**
 * @brief Lightgrids are layered 24 bit 3D texture objects of variable size.
 * @details Each layer is up to 128x128x128 RGB at 24bpp. The first layer contains
 * ambient light color, the second contains diffuse light color, and the third contains
 * diffuse light direction.
 */
typedef struct {
	int32_t size[3];
} bsp_lightgrid_t;

/**
 * @brief BSP file lumps in their native file formats. The data is stored as pointers
 * so that we don't take up an ungodly amount of space (285 MB of memory!).
 * You can safely edit the data within the boundaries of the num_x values, but
 * if you want to expand the space required use the Bsp_* functions.
 */
typedef struct {
	int32_t entity_string_size;
	char *entity_string;

	int32_t num_texinfo;
	bsp_texinfo_t *texinfo;

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

	int32_t num_nodes;
	bsp_node_t *nodes;

	int32_t num_leafs;
	bsp_leaf_t *leafs;

	int32_t num_leaf_brushes;
	int32_t *leaf_brushes;

	int32_t num_leaf_faces;
	int32_t *leaf_faces;

	int32_t num_models;
	bsp_model_t *models;

	int32_t num_area_portals;
	bsp_area_portal_t *area_portals;

	int32_t num_areas;
	bsp_area_t *areas;

	int32_t vis_size;
	bsp_vis_t *vis;

	int32_t num_lightmaps;
	bsp_lightmap_t *lightmaps;

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
int32_t Bsp_CompressVis(const bsp_file_t *bsp, const byte *in, byte *out);
int32_t Bsp_DecompressVis(const bsp_file_t *bsp, const byte *in, byte *out);
