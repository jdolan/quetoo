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
#define BSP_VERSION 74

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
#define MAX_BSP_LIGHTS        0x200
#define MAX_BSP_PATCHES       0x400
#define MAX_BSP_VOXELS_SIZE   0x4000000

/**
 * @brief The BSP block node size.
 */
#define BSP_BLOCK_SIZE 512.f

/**
 * @brief Voxel size in world units.
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
 * @brief BSP file format lump identifiers.
 */
typedef enum {
  BSP_LUMP_FIRST,
  BSP_LUMP_ENTITIES = BSP_LUMP_FIRST, ///< Entity string lump.
  BSP_LUMP_MATERIALS,                 ///< Material name references lump.
  BSP_LUMP_PLANES,                    ///< Planes lump.
  BSP_LUMP_BRUSH_SIDES,               ///< Brush sides lump.
  BSP_LUMP_BRUSHES,                   ///< Brushes lump.
  BSP_LUMP_PATCHES,                   ///< Bezier patch surfaces lump.
  BSP_LUMP_VERTEXES,                  ///< Vertex array lump.
  BSP_LUMP_ELEMENTS,                  ///< Element (index) array lump.
  BSP_LUMP_FACES,                     ///< Faces lump.
  BSP_LUMP_NODES,                     ///< BSP nodes lump.
  BSP_LUMP_LEAF_BRUSHES,              ///< Leaf-brush reference index lump.
  BSP_LUMP_LEAFS,                     ///< BSP leafs lump.
  BSP_LUMP_DRAW_ELEMENTS,             ///< Draw elements (GL draw commands) lump.
  BSP_LUMP_BLOCKS,                    ///< Block nodes lump.
  BSP_LUMP_MODELS,                    ///< Inline models lump.
  BSP_LUMP_LIGHTS,                    ///< Light sources lump.
  BSP_LUMP_VOXELS,                    ///< Voxel light grid lump.
  BSP_LUMP_LAST                       ///< Sentinel; total number of lumps.
} bsp_lump_id_t;

#define BSP_LUMPS_ALL ((1 << BSP_LUMP_LAST) - 1)

/**
 * @brief The BSP lump type.
 */
typedef struct {
  int32_t file_ofs; ///< The lump offset in bytes.
  int32_t file_len; ///< The lump length in bytes.
} bsp_lump_t;

/**
 * @brief The BSP header type.
 */
typedef struct {
  int32_t ident;                   ///< `BSP_IDENT`
  int32_t version;                 ///< `BSP_VERSION`
  bsp_lump_t lumps[BSP_LUMP_LAST]; ///< The lump table of contents.
} bsp_header_t;

/**
 * @brief Material references.
 */
typedef struct {
  char name[MAX_QPATH]; ///< The material name path.
} bsp_material_t;

/**
 * @brief Planes are stored in opposing pairs, with positive normal vectors first in each pair.
 */
typedef struct {
  vec3_t normal; ///< The normal vector.
  float dist;    ///< The distance, or offset, from the origin.
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
  int32_t plane;    ///< The index of the plane of this brush side.
  int32_t material; ///< The index of the material.
  vec4_t axis[2];   ///< Texture projection in .map format: two 4-element vectors [s/t][xyz + offset].
  int32_t contents; ///< Contents bitmask; must be uniform across all sides of a given brush.
  int32_t surface;  ///< Surface bitmask; may vary from side to side on a given brush.
  int32_t value;    ///< The surface value, used for light emission, Phong grouping, etc.
} bsp_brush_side_t;

/**
 * @brief Brushes are convex volumes defined by four or more clipping planes.
 */
typedef struct {
  int32_t entity;           ///< The index of the entity that defined this brush in the source .map.
  int32_t contents;         ///< The contents bitmask.
  int32_t first_brush_side; ///< The index of the first brush side belonging to this brush.
  int32_t num_brush_sides;  ///< The count of brush sides, including bevels.
  box3_t bounds;            ///< The AABB of this brush.
} bsp_brush_t;

/**
 * @brief The maximum patch control point grid dimensions.
 */
#define MAX_PATCH_SIZE 31

/**
 * @brief The maximum number of control points in a patch.
 */
#define MAX_PATCH_CONTROL_POINTS (MAX_PATCH_SIZE * MAX_PATCH_SIZE)

/**
 * @brief A patch control point for the BSP patches lump.
 */
typedef struct {
  vec3_t position; ///< Control point position in model space.
  vec2_t st;       ///< Texture coordinates at this control point.
} bsp_patch_control_point_t;

/**
 * @brief BSP representation of a patchDef2 Bézier surface.
 */
typedef struct {
  int32_t entity;                                                     ///< The entity number that defined this patch.
  int32_t material;                                                   ///< The material index.
  int32_t contents;                                                   ///< The contents bitmask.
  int32_t surface;                                                    ///< The surface bitmask.
  int32_t width, height;                                              ///< The control point grid dimensions.
  bsp_patch_control_point_t control_points[MAX_PATCH_CONTROL_POINTS]; ///< The control points in row-major order (width × height).
} bsp_patch_t;


/**
 * @brief The BSP vertex type.
 */
typedef struct {
  vec3_t position;   ///< Vertex position in model space.
  vec3_t normal;     ///< Vertex normal vector.
  vec3_t tangent;    ///< Tangent vector for normal mapping.
  vec3_t bitangent;  ///< Bitangent vector for normal mapping.
  vec2_t diffusemap; ///< Diffusemap texture coordinates.
  color32_t color;   ///< Vertex color (lightmap contribution).
} bsp_vertex_t;

/**
 * @brief Faces are polygon primitives, stored as both vertex and element arrays.
 */
typedef struct {
  int32_t brush_side;    ///< The index of the brush side which created this face, or -1 for patch faces.
  int32_t plane;         ///< Index of the plane, or -1 for patch faces; may be negated for translucent brushes.
  int32_t patch;         ///< The index of the patch which created this face, or -1 for brush faces.
  int32_t node;          ///< The index of the BSP node containing this face.
  int32_t block;         ///< The index of the block node containing this face.
  box3_t bounds;         ///< The AABB of this face.
  int32_t first_vertex;  ///< The index of the first vertex within this face.
  int32_t num_vertexes;  ///< The count of vertexes.
  int32_t first_element; ///< The index of the first element of this face.
  int32_t num_elements;  ///< The count of elements.
} bsp_face_t;

/**
 * @brief The BSP node type.
 * @details Nodes are created by planes in the .map file, selected by a heuristic that prefers
 * planes which include visible faces and split as few brushes as possible.
 */
typedef struct {
  int32_t plane;         ///< The index of the plane that created this node.
  int32_t children[2];   ///< Child node indexes; negative values are leaf indexes encoded as -(index + 1).
  int32_t contents;      ///< The node contents, either `CONTENTS_NODE` or `CONTENTS_BLOCK`.
  box3_t bounds;         ///< The AABB of this node used for collision.
  box3_t visible_bounds; ///< AABB of visible faces on this node; typically smaller than bounds, used for frustum culling.
  int32_t first_face;    ///< The index of the first face within this node.
  int32_t num_faces;     ///< The count of faces, front and back, on this node.
} bsp_node_t;

/**
 * @brief The BSP leaf type.
 */
typedef struct {
  int32_t contents;         ///< The contents of the leaf, which is the bitwise OR of all brushes inside the leaf.
  box3_t bounds;            ///< The AABB of this leaf used for collision.
  int32_t first_leaf_brush; ///< The index of the first leaf-brush reference.
  int32_t num_leaf_brushes; ///< The number of leaf-brush references for this leaf.
} bsp_leaf_t;

/**
 * @brief Draw elements are OpenGL draw commands, serialized directly within the BSP.
 * @details For each model, all opaque faces sharing material and contents are grouped
 * into a single draw elements. All blend faces sharing plane, material and contents
 * are also grouped.
 */
typedef struct {
  int32_t material;      ///< The material index.
  int32_t surface;       ///< The surface flags bitmask.
  box3_t bounds;         ///< The AABB for occlusion and frustum culling.
  int32_t first_element; ///< The index of the first element.
  int32_t num_elements;  ///< The count of elements.
} bsp_draw_elements_t;

/**
 * @brief Blocks are large, uniform, axial-aligned and grid-like nodes used to aggregate
 * rendering operations.
 */
typedef struct {
  int32_t node;               ///< The `CONTENTS_BLOCK` node defining this block.
  int32_t first_draw_element; ///< The index of the first draw elements within this block.
  int32_t num_draw_elements;  ///< The count of draw elements within this block.
  box3_t visible_bounds;      ///< AABB of all draw elements within this block; larger than the node's own visible_bounds.
} bsp_block_t;

/**
 * @brief The BSP inline model type.
 * @details Each map is comprised of 1 or more inline models. The first is the _worldspawn_ entity.
 */
typedef struct {
  int32_t entity;                   ///< The index of the entity that defined this model.
  int32_t head_node;                ///< The index of the head node of this model's BSP tree.
  box3_t bounds;                    ///< The AABB of this model.

  /**
   * @brief The AABB of this model's visible faces.
   * @remarks Often smaller than `bounds`, and useful for frustum culling.
   */
  box3_t visible_bounds;
  int32_t first_face;               ///< The index of the first face belonging to this model.
  int32_t num_faces;                ///< The count of faces belonging to this model.
  int32_t first_depth_pass_element; ///< The index of the first depth pass element belonging to this model.
  int32_t num_depth_pass_elements;  ///< The count of depth pass elements.
  int32_t first_draw_elements;      ///< The index of the first draw element of this model.
  int32_t num_draw_elements;        ///< The count of draw elements.
  int32_t first_block;              ///< The index of the first block of this model.
  int32_t num_blocks;               ///< The count of blocks.
} bsp_model_t;

/**
 * @brief BSP representation of light sources.
 */
typedef struct {
  int32_t entity;                   ///< The entity number.
  vec3_t origin;                    ///< The light origin.
  float radius;                     ///< The light radius.
  vec3_t color;                     ///< The light color.
  float intensity;                  ///< The light intensity.
  box3_t bounds;                    ///< The light's visible bounds, clipped to world geometry.
  char style[MAX_BSP_ENTITY_VALUE]; ///< The style string, a-z (26 levels), animated at 10Hz.
  int32_t first_depth_pass_element; ///< The index of the first element of this light's depth pass geometry.
  int32_t num_depth_pass_elements;  ///< The count of depth pass geometry elements.

  /**
   * @brief The entity number of the inline model entity this light is attached to, or 0.
   * @details When > 0, this light is treated as a dynamic light that moves with the
   * entity. No shadow geometry is generated, and the origin is an offset from the
   * entity's initial position.
   */
  int32_t target_entity;
} bsp_light_t;

/**
 * @brief The voxels lump header.
 */
typedef struct {
  vec3i_t size;              ///< The voxel grid dimensions.
  int32_t num_light_indices; ///< The total count of light indices for all voxels.
} bsp_voxels_t;

/**
 * @brief BSP file lumps in their native file formats. The data is stored as pointers
 * so that we don't take up an ungodly amount of space.
 */
typedef struct bsp_file_s {
  int32_t entity_string_size;         ///< Length of the entity string in bytes.
  char *entity_string;                ///< The raw entity string.

  int32_t num_materials;              ///< Number of material references.
  bsp_material_t *materials;          ///< Material reference array.

  int32_t num_planes;                 ///< Number of planes.
  bsp_plane_t *planes;                ///< Plane array.

  int32_t num_brush_sides;            ///< Number of brush sides.
  bsp_brush_side_t *brush_sides;      ///< Brush side array.

  int32_t num_brushes;                ///< Number of brushes.
  bsp_brush_t *brushes;               ///< Brush array.

  int32_t num_patches;                ///< Number of Bezier patch surfaces.
  bsp_patch_t *patches;               ///< Patch array.

  int32_t num_vertexes;               ///< Number of vertices.
  bsp_vertex_t *vertexes;             ///< Vertex array.

  int32_t num_elements;               ///< Number of element indices.
  int32_t *elements;                  ///< Element index array.

  int32_t num_faces;                  ///< Number of faces.
  bsp_face_t *faces;                  ///< Face array.

  int32_t num_nodes;                  ///< Number of BSP nodes.
  bsp_node_t *nodes;                  ///< BSP node array.

  int32_t num_leaf_brushes;           ///< Number of leaf-brush references.
  int32_t *leaf_brushes;              ///< Leaf-brush reference index array.

  int32_t num_leafs;                  ///< Number of BSP leafs.
  bsp_leaf_t *leafs;                  ///< BSP leaf array.

  int32_t num_draw_elements;          ///< Number of draw element commands.
  bsp_draw_elements_t *draw_elements; ///< Draw element array.

  int32_t num_blocks;                 ///< Number of block nodes.
  bsp_block_t *blocks;                ///< Block node array.

  int32_t num_models;                 ///< Number of inline models.
  bsp_model_t *models;                ///< Inline model array.

  int32_t num_lights;                 ///< Number of light sources.
  bsp_light_t *lights;                ///< Light source array.

  int32_t voxels_size;                ///< Total size of the voxels lump in bytes.
  bsp_voxels_t *voxels;               ///< Voxel light grid header.

  bsp_lump_id_t loaded_lumps;         ///< Bitmask of loaded lump identifiers.
} bsp_file_t;

/**
 * @brief Verifies the BSP file header; returns 0 on success, -1 on error.
 */
int32_t Bsp_Verify(const bsp_header_t *file);

/**
 * @brief Returns the total size in bytes of the BSP file described by the header.
 */
int64_t Bsp_Size(const bsp_header_t *file);

/**
 * @brief Returns true if the given lump has been loaded into the BSP.
 */
bool Bsp_LumpLoaded(const bsp_file_t *bsp, const bsp_lump_id_t lump_id);

/**
 * @brief Unloads the specified lump from the BSP, freeing its memory.
 */
void Bsp_UnloadLump(bsp_file_t *bsp, const bsp_lump_id_t lump_id);

/**
 * @brief Unloads all lumps matching the given bitmask from the BSP.
 */
void Bsp_UnloadLumps(bsp_file_t *bsp, const bsp_lump_id_t lump_bits);

/**
 * @brief Loads the specified lump from the BSP file into the bsp structure.
 * @return true on success, false on failure.
 */
bool Bsp_LoadLump(const bsp_header_t *file, bsp_file_t *bsp, const bsp_lump_id_t lump_id);

/**
 * @brief Loads all lumps matching the given bitmask from the BSP file.
 * @return true on success, false on failure.
 */
bool Bsp_LoadLumps(const bsp_header_t *file, bsp_file_t *bsp, const bsp_lump_id_t lump_bits);

/**
 * @brief Allocates memory for the specified lump in the BSP with the given element count.
 */
void Bsp_AllocLump(bsp_file_t *bsp, const bsp_lump_id_t lump_id, const size_t count);

/**
 * @brief Serializes the BSP to disk.
 */
void Bsp_Write(file_t *file, const bsp_file_t *bsp);
