#pragma once

#include "cm_types.h"



/**
 * @brief BSP file lumps in their native file formats. The data is stored as pointers
 * so that we don't take up an ungodly amount of space (285 MB of memory!).
 * You can safely edit the data within the boundaries of the num_x values, but
 * if you want to expand the space required use the Bsp_* functions.
 */
typedef struct {
	// BSP lumps
	int32_t entity_string_size;
	char *entity_string;//[MAX_BSP_ENT_STRING];

	int32_t num_planes;
	d_bsp_plane_t *planes;//[MAX_BSP_PLANES];

	int32_t num_vertexes;
	d_bsp_vertex_t *vertexes;//[MAX_BSP_VERTS];

	int32_t vis_data_size;
	union {
		d_bsp_vis_t *vis;
		byte *raw;
	} vis_data;//[MAX_BSP_VISIBILITY];

	int32_t num_nodes;
	d_bsp_node_t *nodes;//[MAX_BSP_NODES];

	int32_t num_texinfo;
	d_bsp_texinfo_t *texinfo;//[MAX_BSP_TEXINFO];

	int32_t num_faces;
	d_bsp_face_t *faces;//[MAX_BSP_FACES];

	int32_t lightmap_data_size;
	byte *lightmap_data;//[MAX_BSP_LIGHTING];

	int32_t num_leafs;
	d_bsp_leaf_t *leafs;//[MAX_BSP_LEAFS];

	int32_t num_leaf_faces;
	uint16_t *leaf_faces;//[MAX_BSP_LEAF_FACES];

	int32_t num_leaf_brushes;
	uint16_t *leaf_brushes;//[MAX_BSP_LEAF_BRUSHES];

	int32_t num_edges;
	d_bsp_edge_t *edges;//[MAX_BSP_EDGES];

	int32_t num_face_edges;
	int32_t *face_edges;//[MAX_BSP_FACE_EDGES];

	int32_t num_models;
	d_bsp_model_t *models;//[MAX_BSP_MODELS];

	int32_t num_brushes;
	d_bsp_brush_t *brushes;//[MAX_BSP_BRUSHES];

	int32_t num_brush_sides;
	d_bsp_brush_side_t *brush_sides;//[MAX_BSP_BRUSH_SIDES];

	// FIXME: never used by the game?
	//byte dpop[256];

	int32_t num_areas;
	d_bsp_area_t *areas;//[MAX_BSP_AREAS];

	int32_t num_area_portals;
	d_bsp_area_portal_t *area_portals;//[MAX_BSP_AREA_PORTALS];

	int32_t num_normals;
	d_bsp_normal_t *normals;//[MAX_BSP_VERTS];

	// local to bsp_file_t
	uint32_t loaded_lumps;
} bsp_file_t;

int32_t Bsp_Verify(file_t *file);
_Bool Bsp_LumpLoaded(bsp_file_t *bsp, const int32_t lump_id);
void Bsp_UnloadLump(bsp_file_t *bsp, const int32_t lump_id);
void Bsp_UnloadLumps(bsp_file_t *bsp, const uint32_t lump_bits);
_Bool Bsp_LoadLump(file_t *file, bsp_file_t *bsp, const int32_t lump_id);
_Bool Bsp_LoadLumps(file_t *file, bsp_file_t *bsp, const uint32_t lump_bits);
void Bsp_OverwriteLump(file_t *file, bsp_file_t *bsp, const int32_t lump_id);
void Bsp_Write(file_t *file, bsp_file_t *bsp, const int32_t version);
