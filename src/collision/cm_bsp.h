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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#pragma once

#include "cm_types.h"

/**
 * @brief BSP file identification.
 */
#define BSP_IDENT             (('P' << 24) + ('S' << 16) + ('B' << 8) + 'I') // "IBSP"
#define BSP_VERSION           84

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
#define MAX_BSP_ELEMENTS      0x800000
#define MAX_BSP_FACES         0x20000
#define MAX_BSP_NODES         0x20000
#define MAX_BSP_LEAF_BRUSHES  0x20000
#define MAX_BSP_LEAFS         0x20000
#define MAX_BSP_DRAW_ELEMENTS 0x20000
#define MAX_BSP_BLOCKS        0x400
#define MAX_BSP_MODELS        0x100
#define MAX_BSP_LIGHTS        0x200
#define MAX_BSP_PORTALS       0x40
#define MAX_BSP_REFLECTIONS   0x40
#define MAX_BSP_PATCHES       0x400
#define MAX_BSP_VOXELS_SIZE   0x4000000
#define MAX_BSP_LIGHT_VOXELS  0x800000
#define MAX_BSP_BLOCK_VOXELS  0x800000

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
  BSP_LUMP_LIGHT_VOXELS,
  BSP_LUMP_BLOCK_VOXELS,
  BSP_LUMP_PORTALS,
  BSP_LUMP_REFLECTIONS,
  BSP_LUMP_LAST
} BspLumpId;

#define BSP_LUMPS_ALL ((1 << BSP_LUMP_LAST) - 1)

/**
 * @brief The BSP lump type.
 */
typedef struct {

  /**
   * @brief The lump offset in bytes.
   */
  int32_t fileOfs;

  /**
   * @brief The lump length in bytes.
   */
  int32_t fileLen;
} BspLump;

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
  BspLump lumps[BSP_LUMP_LAST];
} BspHeader;

/**
 * @brief Material references.
 */
typedef struct {

  /**
   * @brief The material name path.
   */
  char name[MAX_QPATH];
} BspMaterial;

/**
 * @brief Planes are stored in opposing pairs, with positive normal vectors first in each pair.
 */
typedef struct {

  /**
   * @brief The normal vector.
   */
  Vec3 normal;

  /**
   * @brief The distance, or offset, from the origin.
   */
  float dist;
} BspPlane;

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
  Vec4 axis[2];

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
} BspBrushSide;

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
  int32_t firstBrushSide;

  /**
   * @brief The count of brush sides, including bevels.
   */
  int32_t numBrushSides;

  /**
   * @brief The AABB of this brush.
   */
  Box3 bounds;
} BspBrush;

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
  Vec3 position;

  /**
   * @brief Texture coordinates at this control point.
   */
  Vec2 st;
} BspPatchControlPoint;

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
  BspPatchControlPoint controlPoints[MAX_PATCH_CONTROL_POINTS];
} BspPatch;


/**
 * @brief The BSP vertex type.
 */
typedef struct {

  /**
   * @brief Vertex position in model space.
   */
  Vec3 position;

  /**
   * @brief Vertex normal vector.
   */
  Vec3 normal;

  /**
   * @brief Tangent vector for normal mapping.
   */
  Vec3 tangent;

  /**
   * @brief Bitangent vector for normal mapping.
   */
  Vec3 bitangent;

  /**
   * @brief Diffusemap texture coordinates.
   */
  Vec2 diffusemap;

  /**
   * @brief Vertex color (lightmap contribution).
   */
  Color32 color;
} BspVertex;

/**
 * @brief Faces are polygon primitives, stored as both vertex and element arrays.
 */
typedef struct {

  /**
   * @brief The index of the brush side which created this face, or -1 for patch faces.
   */
  int32_t brushSide;

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
  Box3 bounds;

  /**
   * @brief The index of the first vertex within this face.
   */
  int32_t firstVertex;

  /**
   * @brief The count of vertexes.
   */
  int32_t numVertexes;

  /**
   * @brief The index of the first element of this face.
   */
  int32_t firstElement;

  /**
   * @brief The count of elements.
   */
  int32_t numElements;
} BspFace;

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
  Box3 bounds;

  /**
   * @brief AABB of visible faces on this node; typically smaller than bounds, used for frustum culling.
   */
  Box3 visibleBounds;

  /**
   * @brief The index of the first face within this node.
   */
  int32_t firstFace;

  /**
   * @brief The count of faces, front and back, on this node.
   */
  int32_t numFaces;
} BspNode;

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
  Box3 bounds;

  /**
   * @brief The index of the first leaf-brush reference.
   */
  int32_t firstLeafBrush;

  /**
   * @brief The number of leaf-brush references for this leaf.
   */
  int32_t numLeafBrushes;
} BspLeaf;

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
  Box3 bounds;

  /**
   * @brief The index of the first element.
   */
  int32_t firstElement;

  /**
   * @brief The count of elements.
   */
  int32_t numElements;

  /**
   * @brief The index of the reflection these elements show, or `-1` for none.
   * @details A portal is found the other way about, from `BspPortal::drawElements`, because a
   * portal owns exactly one draw elements while a reflection owns as many as it has blocks.
   */
  int32_t reflection;
} BspDrawElements;

/**
 * @brief A portal: a `SURF_PORTAL` face, and the point the world is viewed from to fill it.
 * @details The compiler resolves both frames, so the renderer carries the player's camera from
 * the entry frame into the exit frame to place the view a portal shows. `right` is the cross
 * product of `forward` and `up` in both frames, and is not stored, so that it can never be
 * baked inconsistently with them.
 */
typedef struct {

  /**
   * @brief The index of the brush side that defined this portal.
   */
  int32_t brushSide;

  /**
   * @brief The index of the draw elements this portal's face was emitted to.
   */
  int32_t drawElements;

  /**
   * @brief The center of the portal face.
   */
  Vec3 entryOrigin;

  /**
   * @brief The direction of travel through the portal face, which is the reverse of its
   * outward normal, and the face's up, from the way its texture reads.
   */
  Vec3 entryForward, entryUp;

  /**
   * @brief The origin of the entity this portal views the world from.
   */
  Vec3 exitOrigin;

  /**
   * @brief The direction that entity faces, and its up.
   */
  Vec3 exitForward, exitUp;
} BspPortal;

/**
 * @brief A reflection: the plane of one or more `SURF_REFLECT` faces of an inline model, which
 * the renderer mirrors the camera about to fill them.
 * @details One per plane per model rather than one per face. Draw elements are emitted per BSP
 * block, so a pool spanning several blocks is several of them, and a subview each would spend the
 * renderer's whole budget on one pond. The plane is resolved here because the compiler holds it
 * exactly: deriving it from a winding would have to guess the facing from a vertex normal that
 * Phong shading may have smoothed away from the face.
 *
 * `dist` is not stored. It is `origin` dotted with `normal`, and `origin` lies on the plane, so
 * it cannot be baked inconsistently with them.
 */
typedef struct {

  /**
   * @brief The index of the inline model whose faces show this reflection.
   */
  int32_t model;

  /**
   * @brief A point on the plane, in the model's space, being the centroid of the faces.
   */
  Vec3 origin;

  /**
   * @brief The outward normal of the plane, in the model's space.
   */
  Vec3 normal;

  /**
   * @brief The bounds of the faces showing this reflection, in the model's space.
   */
  Box3 bounds;
} BspReflection;

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
  int32_t firstDrawElement;

  /**
   * @brief The count of draw elements within this block.
   */
  int32_t numDrawElements;

  /**
   * @brief AABB of all draw elements within this block; larger than the node's own `visibleBounds`.
   */
  Box3 visibleBounds;

  /**
   * @brief The index of the first voxel touched by this block, within `BSP_LUMP_BLOCK_VOXELS`.
   */
  int32_t firstVoxel;

  /**
   * @brief The count of voxels touched by this block.
   */
  int32_t numVoxels;
} BspBlock;

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
  int32_t headNode;

  /**
   * @brief The AABB of this model.
   */
  Box3 bounds;

  /**
   * @brief The AABB of this model's visible faces.
   * @remarks Often smaller than `bounds`, and useful for frustum culling.
   */
  Box3 visibleBounds;

  /**
   * @brief The index of the first face belonging to this model.
   */
  int32_t firstFace;

  /**
   * @brief The count of faces belonging to this model.
   */
  int32_t numFaces;

  /**
   * @brief The index of the first depth pass draw elements belonging to this model.
   * @details Entry 0 lumps all opaque faces into a single draw elements, with a sentinel
   * material of -1. Each subsequent entry is a unique alpha-tested material (foliage, fences,
   * grates), so that its diffuse texture may be sampled and discarded per-pixel.
   */
  int32_t firstDepthPassElements;

  /**
   * @brief The count of depth pass draw elements.
   */
  int32_t numDepthPassElements;

  /**
   * @brief The index of the first draw element of this model.
   */
  int32_t firstDrawElements;

  /**
   * @brief The count of draw elements.
   */
  int32_t numDrawElements;

  /**
   * @brief The index of the first block of this model.
   */
  int32_t firstBlock;

  /**
   * @brief The count of blocks.
   */
  int32_t numBlocks;
} BspModel;

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
  Vec3 origin;

  /**
   * @brief The light radius.
   */
  float radius;

  /**
   * @brief The light color.
   */
  Vec3 color;

  /**
   * @brief The light intensity.
   */
  float intensity;

  /**
   * @brief The light's visible bounds, clipped to world geometry.
   */
  Box3 bounds;

  /**
   * @brief The style string, a-z (26 levels), animated at 10Hz.
   */
  char style[MAX_BSP_ENTITY_VALUE];

  /**
   * @brief Per-light style animation phase offset (0-1 fraction of the style cycle).
   * @details Derived by quemap from the `drift` entity key and the light's world origin.
   * Lights with the same style string flicker independently when their drift values differ.
   */
  float drift;

  /**
   * @brief The index of the first draw elements of this light's shadow geometry.
   * @details One draw elements is emitted for all opaque faces lumped together
   * (`material == -1`), plus one per unique alpha-test material touched by
   * the light, so alpha-tested faces (foliage, fences, grates) cast holes.
   */
  int32_t firstDrawElements;

  /**
   * @brief The count of draw elements of this light's shadow geometry.
   */
  int32_t numDrawElements;

  /**
   * @brief The entity number of the inline model entity this light is attached to, or `-1`.
   * @details When set, this light is treated as a dynamic light that translates with the
   * entity. No shadow geometry is generated. `origin` remains the light's own authored
   * world position, from which the client derives an offset against the target entity's
   * `origin` key, which a `common/origin` brush makes non-zero.
   */
  int32_t targetEntity;

  /**
   * @brief The material index of the brush side that emits this light, or `-1` for a light entity.
   * @details A light with a material is animated at runtime by the `STAGE_LIGHT` stage of that
   * material: its intensity is read from the stage, and scaled by the stage pulse.
   */
  int32_t material;

  /**
   * @brief The index of the first voxel touched by this light, within `BSP_LUMP_LIGHT_VOXELS`.
   */
  int32_t firstVoxel;

  /**
   * @brief The count of voxels touched by this light.
   */
  int32_t numVoxels;
} BspLight;

/**
 * @brief The voxels lump header.
 */
typedef struct {

  /**
   * @brief The voxel grid dimensions.
   */
  Vec3i size;

  /**
   * @brief The total count of light indices for all voxels.
   */
  int32_t numLightIndices;

  /**
   * @brief The world bounds used to build the voxel grid, aligned to BSP_VOXEL_SIZE.
   */
  Box3 bounds;
} BspVoxels;

/**
 * @brief BSP file lumps in their native file formats. The data is stored as pointers
 * so that we don't take up an ungodly amount of space.
 */
typedef struct BspFile {

  /**
   * @brief Length of the entity string in bytes.
   */
  int32_t entityStringSize;

  /**
   * @brief The raw entity string.
   */
  char *entityString;

  /**
   * @brief Number of material references.
   */
  int32_t numMaterials;

  /**
   * @brief Material reference array.
   */
  BspMaterial *materials;

  /**
   * @brief Number of planes.
   */
  int32_t numPlanes;

  /**
   * @brief Plane array.
   */
  BspPlane *planes;

  /**
   * @brief Number of brush sides.
   */
  int32_t numBrushSides;

  /**
   * @brief Brush side array.
   */
  BspBrushSide *brushSides;

  /**
   * @brief Number of brushes.
   */
  int32_t numBrushes;

  /**
   * @brief Brush array.
   */
  BspBrush *brushes;

  /**
   * @brief Number of Bezier patch surfaces.
   */
  int32_t numPatches;

  /**
   * @brief Patch array.
   */
  BspPatch *patches;

  /**
   * @brief Number of vertices.
   */
  int32_t numVertexes;

  /**
   * @brief Vertex array.
   */
  BspVertex *vertexes;

  /**
   * @brief Number of element indices.
   */
  int32_t numElements;

  /**
   * @brief Element index array.
   */
  int32_t *elements;

  /**
   * @brief Number of faces.
   */
  int32_t numFaces;

  /**
   * @brief Face array.
   */
  BspFace *faces;

  /**
   * @brief Number of BSP nodes.
   */
  int32_t numNodes;

  /**
   * @brief BSP node array.
   */
  BspNode *nodes;

  /**
   * @brief Number of leaf-brush references.
   */
  int32_t numLeafBrushes;

  /**
   * @brief Leaf-brush reference index array.
   */
  int32_t *leafBrushes;

  /**
   * @brief Number of BSP leafs.
   */
  int32_t numLeafs;

  /**
   * @brief BSP leaf array.
   */
  BspLeaf *leafs;

  /**
   * @brief Number of draw element commands.
   */
  int32_t numDrawElements;

  /**
   * @brief Draw element array.
   */
  BspDrawElements *drawElements;

  /**
   * @brief Number of block nodes.
   */
  int32_t numBlocks;

  /**
   * @brief Block node array.
   */
  BspBlock *blocks;

  /**
   * @brief Number of inline models.
   */
  int32_t numModels;

  /**
   * @brief Inline model array.
   */
  BspModel *models;

  /**
   * @brief Number of light sources.
   */
  int32_t numPortals;

  /**
   * @brief The portals.
   */
  BspPortal *portals;

  /**
   * @brief Number of reflections.
   */
  int32_t numReflections;

  /**
   * @brief The reflections.
   */
  BspReflection *reflections;

  int32_t numLights;

  /**
   * @brief Light source array.
   */
  BspLight *lights;

  /**
   * @brief Total size of the voxels lump in bytes.
   */
  int32_t voxelsSize;

  /**
   * @brief Voxel light grid header.
   */
  BspVoxels *voxels;

  /**
   * @brief Number of light voxel indices.
   */
  int32_t numLightVoxels;

  /**
   * @brief Light voxel index array, sliced per-light via `BspLight.firstVoxel`/`numVoxels`.
   */
  int32_t *lightVoxels;

  /**
   * @brief Number of block voxel indices.
   */
  int32_t numBlockVoxels;

  /**
   * @brief Block voxel index array, sliced per-block via `BspBlock.firstVoxel`/`numVoxels`.
   */
  int32_t *blockVoxels;

  /**
   * @brief Bitmask of loaded lump identifiers.
   */
  BspLumpId loadedLumps;
} BspFile;

/**
 * @brief Verifies the BSP file header; returns 0 on success, -1 on error.
 */
int32_t Bsp_Verify(const BspHeader *file);

/**
 * @brief Returns the total size in bytes of the BSP file described by the header.
 */
int64_t Bsp_Size(const BspHeader *file);

/**
 * @brief Returns true if the given lump has been loaded into the BSP.
 */
bool Bsp_LumpLoaded(const BspFile *bsp, const BspLumpId lumpId);

/**
 * @brief Unloads the specified lump from the BSP, freeing its memory.
 */
void Bsp_UnloadLump(BspFile *bsp, const BspLumpId lumpId);

/**
 * @brief Unloads all lumps matching the given bitmask from the BSP.
 */
void Bsp_UnloadLumps(BspFile *bsp, const BspLumpId lumpBits);

/**
 * @brief Loads the specified lump from the BSP file into the bsp structure.
 * @return true on success, false on failure.
 */
bool Bsp_LoadLump(const BspHeader *file, BspFile *bsp, const BspLumpId lumpId);

/**
 * @brief Loads all lumps matching the given bitmask from the BSP file.
 * @return true on success, false on failure.
 */
bool Bsp_LoadLumps(const BspHeader *file, BspFile *bsp, const BspLumpId lumpBits);

/**
 * @brief Allocates memory for the specified lump in the BSP with the given element count.
 */
void Bsp_AllocLump(BspFile *bsp, const BspLumpId lumpId, const size_t count);

/**
 * @brief Serializes the BSP to disk.
 */
void Bsp_Write(File *file, const BspFile *bsp);
