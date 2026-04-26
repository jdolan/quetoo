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
  BSP_LUMP_ENTITIES = BSP_LUMP_FIRST,
  BSP_LUMP_MATERIALS,
  BSP_LUMP_PLANES,
  BSP_LUMP_BRUSH_SIDES,
  BSP_LUMP_BRUSHES,
  BSP_LUMP_PATCHES,
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
  /**
   * @brief The material name path.
   */
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
   * @brief Texture projection in .map format: two 4-element vectors [s/t][xyz + offset].
   */
  vec4_t axis[2];
  /**
   * @brief Contents bitmask; must be uniform across all sides of a given brush.
   */
  int32_t contents;
  /**
   * @brief Surface bitmask; may vary from side to side on a given brush.
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
  /**
   * @brief Control point position in model space.
   */
  vec3_t position;
  /**
   * @brief Texture coordinates at this control point.
   */
  vec2_t st;
} bsp_patch_control_point_t;

/**
 * @brief BSP representation of a patchDef2 Bézier surface.
 */
typedef struct {
  /**
   * @brief The entity number that defined this patch.
   */
  int32_t entity;
  /**
   * @brief The material index.
   */
  int32_t material;
  /**
   * @brief The contents bitmask.
   */
  int32_t contents;
  /**
   * @brief The surface bitmask.
   */
  int32_t surface;
  /**
   * @brief The control point grid dimensions.
   */
  int32_t width, height;
  /**
   * @brief The control points in row-major order (width × height).
   */
  bsp_patch_control_point_t control_points[MAX_PATCH_CONTROL_POINTS];
} bsp_patch_t;


/**
 * @brief The BSP vertex type.
 */
typedef struct {
  /**
   * @brief Vertex position in model space.
   */
  vec3_t position;
  /**
   * @brief Vertex normal vector.
   */
  vec3_t normal;
  /**
   * @brief Tangent vector for normal mapping.
   */
  vec3_t tangent;
  /**
   * @brief Bitangent vector for normal mapping.
   */
  vec3_t bitangent;
  /**
   * @brief Diffusemap texture coordinates.
   */
  vec2_t diffusemap;
  /**
   * @brief Vertex color (lightmap contribution).
   */
  color32_t color;
} bsp_vertex_t;

/**
 * @brief Faces are polygon primitives, stored as both vertex and element arrays.
 */
typedef struct {
  /**
   * @brief The index of the brush side which created this face, or -1 for patch faces.
   */
  int32_t brush_side;
  /**
   * @brief Index of the plane, or -1 for patch faces; may be negated for translucent brushes.
   */
  int32_t plane;
  /**
   * @brief The index of the patch which created this face, or -1 for brush faces.
   */
  int32_t patch;
  /**
   * @brief The index of the BSP node containing this face.
   */
  int32_t node;
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
   * @brief Child node indexes; negative values are leaf indexes encoded as -(index + 1).
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
   * @brief AABB of visible faces on this node; typically smaller than bounds, used for frustum culling.
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
   * @brief AABB of all draw elements within this block; larger than the node's own visible_bounds.
   */
  box3_t visible_bounds;
} bsp_block_t;

/**
 * @brief The BSP inline model type.
 * @details Each map is comprised of 1 or more inline models. The first is the _worldspawn_ entity.
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
  /**
   * @brief Length of the entity string in bytes.
   */
  int32_t entity_string_size;
  /**
   * @brief The raw entity string.
   */
  char *entity_string;

  /**
   * @brief Number of material references.
   */
  int32_t num_materials;
  /**
   * @brief Material reference array.
   */
  bsp_material_t *materials;

  /**
   * @brief Number of planes.
   */
  int32_t num_planes;
  /**
   * @brief Plane array.
   */
  bsp_plane_t *planes;

  /**
   * @brief Number of brush sides.
   */
  int32_t num_brush_sides;
  /**
   * @brief Brush side array.
   */
  bsp_brush_side_t *brush_sides;

  /**
   * @brief Number of brushes.
   */
  int32_t num_brushes;
  /**
   * @brief Brush array.
   */
  bsp_brush_t *brushes;

  /**
   * @brief Number of Bezier patch surfaces.
   */
  int32_t num_patches;
  /**
   * @brief Patch array.
   */
  bsp_patch_t *patches;

  /**
   * @brief Number of vertices.
   */
  int32_t num_vertexes;
  /**
   * @brief Vertex array.
   */
  bsp_vertex_t *vertexes;

  /**
   * @brief Number of element indices.
   */
  int32_t num_elements;
  /**
   * @brief Element index array.
   */
  int32_t *elements;

  /**
   * @brief Number of faces.
   */
  int32_t num_faces;
  /**
   * @brief Face array.
   */
  bsp_face_t *faces;

  /**
   * @brief Number of BSP nodes.
   */
  int32_t num_nodes;
  /**
   * @brief BSP node array.
   */
  bsp_node_t *nodes;

  /**
   * @brief Number of leaf-brush references.
   */
  int32_t num_leaf_brushes;
  /**
   * @brief Leaf-brush reference index array.
   */
  int32_t *leaf_brushes;

  /**
   * @brief Number of BSP leafs.
   */
  int32_t num_leafs;
  /**
   * @brief BSP leaf array.
   */
  bsp_leaf_t *leafs;

  /**
   * @brief Number of draw element commands.
   */
  int32_t num_draw_elements;
  /**
   * @brief Draw element array.
   */
  bsp_draw_elements_t *draw_elements;

  /**
   * @brief Number of block nodes.
   */
  int32_t num_blocks;
  /**
   * @brief Block node array.
   */
  bsp_block_t *blocks;

  /**
   * @brief Number of inline models.
   */
  int32_t num_models;
  /**
   * @brief Inline model array.
   */
  bsp_model_t *models;

  /**
   * @brief Number of light sources.
   */
  int32_t num_lights;
  /**
   * @brief Light source array.
   */
  bsp_light_t *lights;

  /**
   * @brief Total size of the voxels lump in bytes.
   */
  int32_t voxels_size;
  /**
   * @brief Voxel light grid header.
   */
  bsp_voxels_t *voxels;

  /**
   * @brief Bitmask of loaded lump identifiers.
   */
  bsp_lump_id_t loaded_lumps;
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
