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
#define BSP_VERSION 72

/**
 * @brief BSP file format limits.
 */
#define MAX_BSP_ENTITIES_SIZE 0x40000
#define MAX_BSP_ENTITIES      0x800
#define MAX_BSP_MATERIALS     0x400
#define MAX_BSP_PLANES        0x20000
#define MAX_BSP_BRUSH_SIDES   0x20000
#define MAX_BSP_BRUSHES       0x8000
#define MAX_BSP_VERTEXES      0x80000
#define MAX_BSP_ELEMENTS      0x400000
#define MAX_BSP_FACES         0x20000
#define MAX_BSP_NODES         0x20000
#define MAX_BSP_LEAF_BRUSHES  0x20000
#define MAX_BSP_LEAFS         0x20000
#define MAX_BSP_DRAW_ELEMENTS 0x20000
#define MAX_BSP_BLOCKS        0x400
#define MAX_BSP_MODELS        0x100
#define MAX_BSP_LIGHTS        0x100
#define MAX_BSP_VOXELS_SIZE   0x4000000

/**
 * @brief The BSP block node size.
 */
#define BSP_BLOCK_SIZE 512.f

/**
 * @brief Voxel voxel size in world units.
 */
#define BSP_VOXEL_SIZE 32.f

/**
 * @brief Largest voxel texture width in voxels (8192 / 32 = 256).
 */
#define MAX_BSP_VOXELS_AXIAL (MAX_WORLD_AXIAL / BSP_VOXEL_SIZE)

/**
 * @brief Largest voxel texture size in voxels.
 */
#define MAX_BSP_VOXELS (MAX_BSP_VOXELS_AXIAL * MAX_BSP_VOXELS_AXIAL * MAX_BSP_VOXELS_AXIAL)

/**
 * @brief The voxel textures.
 */
typedef enum {
  BSP_VOXELS_FIRST,
  BSP_VOXELS_DATA = BSP_VOXELS_FIRST,
  BSP_VOXELS_FOG,
  BSP_VOXELS_LAST
} bsp_voxel_texture_t;

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
  BSP_LUMP_NODES,
  BSP_LUMP_LEAF_BRUSHES,
  BSP_LUMP_LEAFS,
  BSP_LUMP_DRAW_ELEMENTS,
  BSP_LUMP_BLOCKS,
  BSP_LUMP_MODELS,
  BSP_LUMP_LIGHTS,
  BSP_LUMP_VOXELS,
  BSP_LUMP_LAST
} bsp_lump_id_t;

#define BSP_LUMPS_ALL ((1 << BSP_LUMP_LAST) - 1)

/**
 * @brief The BSP lump type.
 */
typedef struct {
  /**
   * @brief The lump offset in bytes.
   */
  int32_t file_ofs;

  /**
   * @brief The lump length in bytes.
   */
  int32_t file_len;
} bsp_lump_t;

/**
 * @brief The BSP header type.
 */
typedef struct {
  /**
   * @brief `BSP_IDENT`
   */
  int32_t ident;

  /**
   * @brief `BSP_VERSION`
   */
  int32_t version;

  /**
   * @brief The lump table of contents.
   */
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
  /**
   * @brief The normal vector.
   */
  vec3_t normal;

  /**
   * @brief The distance, or offset, from the origin.
   */
  float dist;
} bsp_plane_t;

/**
 * @brief Sentinel value for brush sides created from BSP.
 */
#define BSP_MATERIAL_NODE -1

/**
 * @brief Brush sides are defined by map input, and by BSP tree generation. Map brushes
 * may be split into multiple BSP brushes, producing new sides where they are split.
 * Non-axial map brushes are also "beveled" (padded to axial with additional non-visible sides)
 * to optimize collision detection.
 */
typedef struct {
  /**
   * @brief The index of the plane of this brush side.
   */
  int32_t plane;

  /**
   * @brief The index of the material.
   */
  int32_t material;

  /**
   * @brief The texture projection in .map format.
   * @details A pair of 4-element vectors in the form: `[s/t][xyz + offset]`
   */
  vec4_t axis[2];

  /**
   * @brief The contents bitmask.
   * @remarks This must be uniform on all sides of any given brush. The editor enforces this.
   */
  int32_t contents;

  /**
   * @brief The surface bitmask.
   * @remarks Unlike contents, these may vary from side to side on a given brush.
   */
  int32_t surface;

  /**
   * @brief The surface value, used for light emission, Phong grouping, etc.
   */
  int32_t value;
} bsp_brush_side_t;

/**
 * @brief Brushes are convex volumes defined by four or more clipping planes.
 */
typedef struct {
  /**
   * @brief The index of the entity that defined this brush in the source .map.
   */
  int32_t entity;

  /**
   * @brief The contents bitmask.
   */
  int32_t contents;

  /**
   * @brief The index of the first brush side belonging to this brush.
   */
  int32_t first_brush_side;

  /**
   * @brief The count of brush sides, including bevels.
   */
  int32_t num_brush_sides;

  /**
   * @brief The AABB of this brush.
   */
  box3_t bounds;
} bsp_brush_t;

/**
 * @brief The BSP vertex type.
 */
typedef struct {
  vec3_t position;
  vec3_t normal;
  vec3_t tangent;
  vec3_t bitangent;
  vec2_t diffusemap;
  color32_t color;
} bsp_vertex_t;

/**
 * @brief Faces are polygon primitives, stored as both vertex and element arrays.
 */
typedef struct {
  /**
   * @brief The index of the brush side which created this face.
   */
  int32_t brush_side;

  /**
   * @brief The index of the plane.
   * @details For translucent brushes, this may be the negation of the node's plane.
   */
  int32_t plane;

  /**
   * @brief The index of the block node containing this face.
   */
  int32_t block;

  /**
   * @brief The AABB of this face.
   */
  box3_t bounds;

  /**
   * @brief The index of the first vertex within this face.
   */
  int32_t first_vertex;

  /**
   * @brief The count of vertexes.
   */
  int32_t num_vertexes;

  /**
   * @brief The index of the first element of this face.
   */
  int32_t first_element;

  /**
   * @brief The count of elements.
   */
  int32_t num_elements;
} bsp_face_t;

/**
 * @brief The BSP node type.
 * @details Nodes are created by planes in the .map file, selected by a heuristic that prefers
 * planes which include visible faces and split as few brushes as possible.
 */
typedef struct {
  /**
   * @brief The index of the plane that created this node.
   */
  int32_t plane;

  /**
   * @brief The indexes of the child nodes.
   * @details Negative values are leaf indexes, `-(index + 1)` to account for padding.
   */
  int32_t children[2];

  /**
   * @brief The node contents, either `CONTENTS_NODE` or `CONTENTS_BLOCK`.
   */
  int32_t contents;

  /**
   * @brief The AABB of this node used for collision.
   */
  box3_t bounds;

  /**
   * @brief The AABB of visible faces on this node.
   * @remarks Often smaller than bounds, and useful for frustum culling.
   */
  box3_t visible_bounds;

  /**
   * @brief The index of the first face within this node.
   */
  int32_t first_face;

  /**
   * @brief The count of faces, front and back, on this node.
   */
  int32_t num_faces;
} bsp_node_t;

/**
 * @brief The BSP leaf type.
 */
typedef struct {
  /**
   * @brief The contents of the leaf, which is the bitwise OR of all brushes inside the leaf.
   */
  int32_t contents;

  /**
   * @brief The AABB of this leaf used for collision.
   */
  box3_t bounds;

  /**
   * @brief The index of the first leaf-brush reference.
   */
  int32_t first_leaf_brush;

  /**
   * @brief The number of leaf-brush references for this leaf.
   */
  int32_t num_leaf_brushes;
} bsp_leaf_t;

/**
 * @brief Draw elements are OpenGL draw commands, serialized directly within the BSP.
 * @details For each model, all opaque faces sharing material and contents are grouped
 * into a single draw elements. All blend faces sharing plane, material and contents
 * are also grouped.
 */
typedef struct {
  /**
   * @brief The plane index.
   * @details Alpha blended elements are grouped by plane so that they may be depth sorted.
   */
  int32_t plane;

  /**
   * @brief The material index.
   */
  int32_t material;

  /**
   * @brief The surface flags bitmask.
   */
  int32_t surface;

  /**
   * @brief The AABB for occlusion and frustum culling.
   */
  box3_t bounds;

  /**
   * @brief The index of the first element.
   */
  int32_t first_element;

  /**
   * @brief The count of elements.
   */
  int32_t num_elements;
} bsp_draw_elements_t;

/**
 * @brief Blocks are large, uniform, axial-aligned and grid-like nodes used to aggregate
 * rendering operations.
 */
typedef struct {
  /**
   * @brief The `CONTENTS_BLOCK` node defining this block.
   */
  int32_t node;

  /**
   * @brief The index of the first draw elements within this block.
   */
  int32_t first_draw_element;

  /**
   * @brief The count of draw elements within this block.
   */
  int32_t num_draw_elements;

  /**
   * @brief The visible bounds of all draw elements within this block.
   * @remarks This is different from the visible bounds of the node, which are the bounds of
   * only the faces on that node's plane.
   */
  box3_t visible_bounds;
} bsp_block_t;

/**
 * @brief The BSP inline model type.
 * @details Each map is comprised of 1 or more inline models. The first is the _wordlspawn_ entity.
 */
typedef struct {
  /**
   * @brief The index of the entity that defined this model.
   */
  int32_t entity;

  /**
   * @brief The index of the head node of this model's BSP tree.
   */
  int32_t head_node;

  /**
   * @brief The AABB of this model.
   */
  box3_t bounds;

  /**
   * @brief The AABB of this model's visible faces.
   * @remarks Often smaller than `bounds`, and useful for frustum culling.
   */
  box3_t visible_bounds;

  /**
   * @brief The index of the first face belonging to this model.
   */
  int32_t first_face;

  /**
   * @brief The count of faces belonging to this model.
   */
  int32_t num_faces;

  /**
   * @brief The index of the first depth pass element belonging to this model.
   */
  int32_t first_depth_pass_element;

  /**
   * @brief The count of depth pass elements.
   */
  int32_t num_depth_pass_elements;

  /**
   * @brief The index of the first draw element of this model.
   */
  int32_t first_draw_elements;

  /**
   * @brief The count of draw elements.
   */
  int32_t num_draw_elements;

  /**
   * @brief The index of the first block of this model.
   */
  int32_t first_block;

  /**
   * @brief The count of blocks.
   */
  int32_t num_blocks;
} bsp_model_t;

/**
 * @brief BSP representation of light sources.
 */
typedef struct {
  /**
   * @brief The entity number.
   */
  int32_t entity;

  /**
   * @brief The light origin.
   */
  vec3_t origin;

  /**
   * @brief The light radius.
   */
  float radius;

  /**
   * @brief The light color.
   */
  vec3_t color;

  /**
   * @brief The light intensity.
   */
  float intensity;

  /**
   * @brief The light's visible bounds, clipped to world geometry.
   */
  box3_t bounds;

  /**
   * @brief The style string, a-z (26 levels), animated at 10Hz.
   */
  char style[MAX_BSP_ENTITY_VALUE];

  /**
   * @brief The index of the first element of this light's depth pass geometry.
   */
  int32_t first_depth_pass_element;

  /**
   * @brief The count of depth pass geometry elements.
   */
  int32_t num_depth_pass_elements;
} bsp_light_t;

/**
 * @brief The voxels lump header.
 */
typedef struct {
  /**
   * @brief The voxel grid dimensions.
   */
  vec3i_t size;

  /**
   * @brief The total count of light indices for all voxels.
   */
  int32_t num_light_indices;
} bsp_voxels_t;

/**
 * @brief BSP file lumps in their native file formats. The data is stored as pointers
 * so that we don't take up an ungodly amount of space.
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

  int32_t num_nodes;
  bsp_node_t *nodes;

  int32_t num_leaf_brushes;
  int32_t *leaf_brushes;

  int32_t num_leafs;
  bsp_leaf_t *leafs;

  int32_t num_draw_elements;
  bsp_draw_elements_t *draw_elements;

  int32_t num_blocks;
  bsp_block_t *blocks;

  int32_t num_models;
  bsp_model_t *models;

  int32_t num_lights;
  bsp_light_t *lights;

  int32_t voxels_size;
  bsp_voxels_t *voxels;

  bsp_lump_id_t loaded_lumps;
} bsp_file_t;

int32_t Bsp_Verify(const bsp_header_t *file);
int64_t Bsp_Size(const bsp_header_t *file);
bool Bsp_LumpLoaded(const bsp_file_t *bsp, const bsp_lump_id_t lump_id);
void Bsp_UnloadLump(bsp_file_t *bsp, const bsp_lump_id_t lump_id);
void Bsp_UnloadLumps(bsp_file_t *bsp, const bsp_lump_id_t lump_bits);
bool Bsp_LoadLump(const bsp_header_t *file, bsp_file_t *bsp, const bsp_lump_id_t lump_id);
bool Bsp_LoadLumps(const bsp_header_t *file, bsp_file_t *bsp, const bsp_lump_id_t lump_bits);
void Bsp_AllocLump(bsp_file_t *bsp, const bsp_lump_id_t lump_id, const size_t count);
void Bsp_Write(file_t *file, const bsp_file_t *bsp);
