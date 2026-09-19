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

#include <stdalign.h>

#include <SDL3/SDL_video.h>

#include <ObjectivelyGPU.h>

#include "common/atlas.h"
#include "common/files.h"
#include "collision/cm_bsp.h"

/**
 * @brief Media types.
 */
typedef enum {
  R_MEDIA_GENERIC,
  R_MEDIA_IMAGE,
  R_MEDIA_ATLAS,
  R_MEDIA_ATLAS_IMAGE,
  R_MEDIA_ANIMATION,
  R_MEDIA_MODEL,
  R_MEDIA_MATERIAL,
  R_MEDIA_TOTAL
} RenderMediaType;

/**
 * @brief Images, atlases, models, materials, etc. are all managed as media.
 */
typedef struct RenderMedia {

  /**
   * @brief The media name.
   */
  char name[MAX_QPATH];

  /**
   * @brief The media type.
   */
  RenderMediaType type;

  /**
   * @brief The media on which this media depends.
   */
  List *dependencies;

  /**
   * @brief The media registration callback.
   */
  void (*Register)(struct RenderMedia *self);

  /**
   * @brief The media retain callback, to avoid being freed.
   */
  bool (*Retain)(struct RenderMedia *self);

  /**
   * @brief The free callback, to release any system resources.
   */
  void (*Free)(struct RenderMedia *self);

  /**
   * @brief The media seed, to determine if this media is current.
   */
  int32_t seed;
} RenderMedia;

/**
 * @brief Model types.
 */
typedef enum {
  MOD_INVALID,
  MODEL_BSP,
  MODEL_BSP_INLINE,
  MODEL_MESH
} RenderModelType;

/**
 * @brief Image types.
 */
typedef enum {
  IMG_PROGRAM = 1,
  IMG_PIC,
  IMG_SPRITE,
  IMG_ATLAS,
  IMG_MATERIAL,
  IMG_CUBEMAP,
  IMG_VOXELS,
} RenderImageType;

/**
 * @brief Images are referenced by materials, models, entities, particles, etc.
 */
typedef struct {

  /**
   * @brief The media.
   */
  RenderMedia media;

  /**
   * @brief The image type.
   */
  RenderImageType type;

  /**
   * @brief The image width, height and depth (or layers, faces, etc..).
   */
  int32_t width, height, depth;

  /**
   * @brief The GPU texture (ObjectivelyGPU). Owns the sampled texture.
   */
  Texture *texture;
} RenderImage;

/**
 * @brief An image atlas.
 */
typedef struct {

  /**
   * @brief The media.
   */
  RenderMedia media;

  /**
   * @brief The atlas.
   */
  Atlas *atlas;

  /**
   * @brief The compiled image atlas containing all nodes.
   */
  RenderImage *image;

  /**
   * @brief True if this at atlas should be recompiled.
   */
  bool dirty;
} RenderAtlas;

/**
 * @brief An atlas image, castable to `RenderImage` and `RenderMedia`.
 */
typedef struct {

  /**
   * @brief The image.
   */
  RenderImage image;

  /**
   * @brief The atlas node that created this atlas image.
   */
  AtlasNode *node;

  /**
   * @brief The texture coordinates of this atlas image within the atlas.
   */
  Vec4 texcoords;
} RenderAtlasImage;

/**
 * @brief An animation, castable to `RenderMedia`.
 */
typedef struct {

  /**
   * @brief The media.
   */
  RenderMedia media;

  /**
   * @brief The number of frames in this animation.
   */
  int32_t numFrames;

  /**
   * @brief The frames in this animation.
   */
  const RenderImage **frames;
} RenderAnimation;

/**
 * @brief Material stages.
 */
typedef struct RenderStage {

  /**
   * @brief The backing collision material stage.
   */
  const CmStage *cm;

  /**
   * @brief The stage flags, which are the collision stage's plus what the renderer resolves.
   * @details A stage naming the material's own diffusemap samples the portal its face shows,
   *   rather than that texture, and so gains `STAGE_PORTAL` here.
   */
  int32_t flags;

  /**
   * @brief Stages with a render pass will reference an image, atlas image, material, animation, etc.
   */
  RenderMedia *media;

  /**
   * @brief The next stage in the material.
   */
  struct RenderStage *next;
} RenderStage;

/**
 * @brief Materials define texture, animation and lighting properties for BSP and mesh models.
 */
typedef struct RenderMaterial {

  /**
   * @brief Materials are media.
   */
  RenderMedia media;

  /**
   * @brief The collision material definition.
   */
  CmMaterial *cm;

  /**
   * @brief The layered texture containing the diffusemap, normalmap and specularmap.
   */
  RenderImage *texture;

  /**
   * @brief Animated stage definitions.
   */
  RenderStage *stages;

  /**
   * @brief The time when this material was last animated.
   */
  uint32_t ticks;

  /**
   * @brief The diffusemap color.
   */
  Color color;
} RenderMaterial;

/**
 * @brief Decals are projected textures that conform to BSP geometry.
 */
typedef struct RenderDecal {

  /**
   * @brief The decal origin.
   */
  Vec3 origin;

  /**
   * @brief The decal radius.
   */
  float radius;

  /**
   * @brief The decal color.
   */
  Color color;

  /**
   * @brief The decal atlas image.
   */
  RenderAtlasImage *image;

  /**
   * @brief The decal creation time in ticks.
   */
  uint32_t time;

  /**
   * @brief The decal lifetime in ticks.
   */
  uint32_t lifetime;

  /**
   * @brief The decal rotation angle in radians.
   */
  float rotation;
} RenderDecal;

#define MAX_DECALS 0x800

/**
 * @brief Hardware occlusion queries.
 */
typedef struct RenderOcclusionQuery {

  /**
   * @brief The query bounds used for CPU-side culling.
   */
  Box3 bounds;

  /**
   * @brief The first instance index in the shared per-instance box buffer.
   */
  int32_t firstBox;

  /**
   * @brief The count of instanced boxes drawn for this query.
   */
  int32_t numBoxes;

  /**
   * @brief True if the query produced visible fragments.
   */
  bool result;
} RenderOcclusionQuery;

/**
 * @brief BSP plane structure.
 */
typedef struct {

  /**
   * @brief The collision plane.
   */
  const CmBspPlane *cm;
} RenderBspPlane;

/**
 * @brief BSP brush side structure.
 */
typedef struct {

  /**
   * @brief The plane.
   */
  const RenderBspPlane *plane;

  /**
   * @brief The material.
   */
  const RenderMaterial *material;

  /**
   * @brief The texture axis for S and T, in xyz + offset notation.
   */
  Vec4 axis[2];

  /**
   * @brief The brush contents.
   */
  int32_t contents;

  /**
   * @brief The surface flags.
   */
  int32_t surface;

  /**
   * @brief The surface value, for lights or Phong grouping.
   */
  int32_t value;
} RenderBspBrushSide;

/**
 * @brief BSP patch structure, resolved from `BspPatch`.
 */
typedef struct {

  /**
   * @brief The material.
   */
  const RenderMaterial *material;

  /**
   * @brief The brush contents.
   */
  int32_t contents;

  /**
   * @brief The surface flags.
   */
  int32_t surface;
} RenderBspPatch;

/**
 * @brief BSP vertex structure.
 */
typedef struct {

  /**
   * @brief The position.
   */
  Vec3 position;

  /**
   * @brief The normal, for Phong shading.
   */
  Vec3 normal;

  /**
   * @brief The tangent, for per-pixel lighting.
   */
  Vec3 tangent;

  /**
   * @brief The bitangent, for per-pixel lighting.
   */
  Vec3 bitangent;

  /**
   * @brief The diffusemap texture coordinate.
   */
  Vec2 diffusemap;

  /**
   * @brief The color, for alpha blending and vertex lighting effects.
   */
  Color32 color;
} RenderBspVertex;

/**
 * @brief BSP faces, which may reside on the front or back of their node.
 */
typedef struct RenderBspFace {

  /**
   * @brief The brush side which generated this face, or `NULL` for patch faces.
   */
  RenderBspBrushSide *brushSide;

  /**
   * @brief The plane on which this face resides (to disambiguate `node`).
   */
  RenderBspPlane *plane;

  /**
   * @brief The patch which generated this face, or `NULL` for brush faces.
   */
  RenderBspPatch *patch;

  /**
   * @brief The node containing this face.
   */
  struct RenderBspNode *node;

  /**
   * @brief The block containing this face.
   */
  struct RenderBspBlock *block;

  /**
   * @brief The AABB of this face.
   */
  Box3 bounds;

  /**
   * @brief The vertexes.
   */
  RenderBspVertex *vertexes;

  /**
   * @brief The count of vertexes.
   */
  int32_t numVertexes;

  /**
   * @brief The elements.
   */
  void *elements;

  /**
   * @brief The count of elements.
   */
  int32_t numElements;
} RenderBspFace;

/**
 * @brief BSP draw elements for one material within an inline model.
 */
typedef struct {

  /**
   * @brief The material.
   */
  RenderMaterial *material;

  /**
   * @brief The surface flags.
   */
  int32_t surface;

  /**
   * @brief The AABB of the elements.
   */
  Box3 bounds;

  /**
   * @brief An offset pointer (in bytes) into the BSP elements array.
   */
  void *elements;

  /**
   * @brief The count of elements.
   */
  int32_t numElements;

  /**
   * @brief Texture coordinate origin for stage transforms (scale, stretch, rotate).
   */
  Vec2 stOrigin;

  /**
   * @brief The portal these elements show, or `NULL` if they are not a portal face.
   */
  struct RenderBspPortal *portal;
} RenderBspDrawElements;

/**
 * @brief BSP nodes comprise the tree representation of the world.
 */
typedef struct RenderBspNode {

  /**
   * @brief The contents mask; one of `CONTENTS_NODE` or `CONTENTS_BLOCK` for nodes.
   */
  int32_t contents;

  /**
   * @brief The AABB.
   */
  Box3 bounds;

  /**
   * @brief The parent node.
   */
  struct RenderBspNode *parent;

  /**
   * @brief The inline model. Each inline model contains its own sub-tree.
   */
  struct RenderBspInlineModel *model;

  /**
   * @brief The plane that created this node.
   */
  RenderBspPlane *plane;

  /**
   * @brief The child nodes, which may be leaves.
   */
  struct RenderBspNode *children[2];

  /**
   * @brief The AABB of visible faces within this node.
   */
  Box3 visibleBounds;

  /**
   * @brief The faces within this node.
   */
  RenderBspFace *faces;

  /**
   * @brief The count of faces.
   */
  int32_t numFaces;
} RenderBspNode;

/**
 * @brief BSP leafs terminate BSP tree branches.
 * @remarks Leafs can be cast to `RenderBspNode`.
 */
typedef struct {

  /**
   * @brief The contents mask. Valid for leafs, always `CONTENTS_NODE` for nodes.
   */
  int32_t contents;

  /**
   * @brief The AABB.
   */
  Box3 bounds;

  /**
   * @brief The parent node.
   */
  struct RenderBspNode *parent;

  /**
   * @brief The inline model. Each inline model contains its own sub-tree.
   */
  struct RenderBspInlineModel *model;
} RenderBspLeaf;

/**
 * @brief The maximum number of decals that can be attached to a single BSP block.
 */
#define MAX_BSP_BLOCK_DECALS 0x1000

/**
 * @brief The maximum number of dynamic lights per scene.
 * @remarks MUST be a multiple of 128, which the shaders' `uvec4` bitmask assumes, and
 * MUST leave `MAX_LIGHTS` within the shadow atlas, which holds
 * `SHADOW_ATLAS_LIGHTS_PER_ROW` squared tiles.
 */
#define MAX_DYNAMIC_LIGHTS 512
typedef struct {
  uint32_t mask[MAX_DYNAMIC_LIGHTS / 32];
} RenderActiveDynamicLights;

/**
 * @brief The width of the lighting LOD blend zone beyond r_lightingDistance.
 * @remarks Must match LIGHTING_LOD_BLEND_DIST in shaders/light.glsl.
 */
#define LIGHTING_LOD_BLEND_DIST 128.f

/**
 * @brief Decals are aggregated at the BSP block level.
 */
typedef struct {

  /**
   * @brief The decal atlas, cast to `RenderImage` for texture binding.
   */
  RenderImage *image;

  /**
   * @brief The triangles of the decals attached to the containing block.
   */
  Vector *triangles;

  /**
   * @brief The decal vertex buffer, and its capacity in vertices.
   */
  Buffer *vertexBuffer;
  int32_t vertexBufferCapacity;

  /**
   * @brief True if the containing block's decals require uploading.
   */
  bool dirty;

} RenderBspBlockDecals;

/**
 * @brief BSP blocks are large, axial-aligned, gridded nodes used to aggregate rendering operations.
 */
typedef struct RenderBspBlock {

  /**
   * @brief The `CONTENTS_BLOCK` node defining this block.
   */
  RenderBspNode *node;

  /**
   * @brief The draw elements within this block.
   */
  RenderBspDrawElements *drawElements;

  /**
   * @brief The count of draw elements.
   */
  int32_t numDrawElements;

  /**
   * @brief The visible bounds of this block, used for occlusion query and culling.
   */
  Box3 visibleBounds;

  /**
   * @brief The occlusion query for this block.
   */
  RenderOcclusionQuery *query;

  /**
   * @brief The bitwise OR of all draw element surface flags for this block.
   */
  int32_t surface;

  /**
   * @brief The decals for this block.
   */
  RenderBspBlockDecals decals;

  /**
   * @brief The cached dynamic light bitmask for this block.
   */
  RenderActiveDynamicLights activeDynamicLights;

} RenderBspBlock;

/**
 * @brief A BSP inline model.
 */
typedef struct RenderBspInlineModel {

  /**
   * @brief The backing entity definition for this inline model.
   */
  CmEntity *entity;

  /**
   * @brief The head node of this inline model.
   */
  RenderBspNode *headNode;

  /**
   * @brief For frustum culling.
   */
  Box3 visibleBounds;

  /**
   * @brief The faces of this inline model.
   */
  RenderBspFace *faces;

  /**
   * @brief The count of faces.
   */
  int32_t numFaces;

  /**
   * @brief The depth pass draw elements of this inline model.
   * @details Entry 0 lumps all opaque faces into a single draw elements, with a sentinel
   * material of NULL. Each subsequent entry is a unique alpha-tested material.
   */
  RenderBspDrawElements *depthPassElements;

  /**
   * @brief The count of depth pass draw elements.
   */
  int32_t numDepthPassElements;

  /**
   * @brief The draw elements of this inline model.
   */
  RenderBspDrawElements *drawElements;

  /**
   * @brief The count of draw elements.
   */
  int32_t numDrawElements;

  /**
   * @brief The blocks of this inline model.
   */
  RenderBspBlock *blocks;

  /**
   * @brief The count of blocks.
   */
  int32_t numBlocks;

} RenderBspInlineModel;

/**
 * @brief A BSP portal: a `SURF_PORTAL` face, and the point the world is viewed from to fill it.
 * @details Resolved by the compiler into `BSP_LUMP_PORTALS`; see `BspPortal`.
 */
typedef struct RenderBspPortal {

  /**
   * @brief The inline model whose faces show this portal.
   */
  struct RenderModel *model;

  /**
   * @brief The center of the portal face, in the model's space.
   */
  Vec3 origin;

  /**
   * @brief The bounds of the portal face, in the model's space.
   */
  Box3 bounds;

  /**
   * @brief The portal face's outward normal, in the model's space.
   * @details Every draw element of a portal is a fragment of one brush side, so they are all
   *   coplanar and this one normal describes the whole portal. It is the negation of the baked
   *   `entry` forward, which points the way travel through the portal runs rather than the way
   *   the face is seen from.
   */
  Vec3 normal;

  /**
   * @brief The portal face's frame, in the model's space.
   * @details The compiler offsets a brush entity's geometry by its origin brush, so a face's
   *   frame is baked in the space of the model that draws it, not in the world. A mover carries
   *   its portal faces with it, so the frame reaches the world only through the model matrix of
   *   the entity drawing it that frame.
   */
  Mat4 entry;

  /**
   * @brief The frame of the entity this portal views the world from, in world space.
   * @details The exit is a point entity, which the compiler resolves once and which nothing
   *   moves, so unlike `entry` this is already where it belongs.
   */
  Mat4 exit;

  /**
   * @brief The center of the portal face this frame, in world space.
   */
  Vec3 absOrigin;

  /**
   * @brief The bounds of the portal face this frame, in world space, for culling.
   */
  Box3 absBounds;

  /**
   * @brief The portal face's plane this frame, in world space, for culling.
   */
  CmBspPlane absPlane;

  /**
   * @brief Carries a point or direction from the portal face's frame into the frame of the
   * entity it views the world from.
   * @details Composed each frame from `entry`, `exit` and the model matrix of the entity drawing
   *   the face, so a portal on a mover tracks it. Transforming the camera by this places the view
   *   that the face shows, which is what gives a portal parallax rather than the flatness of a
   *   fixed camera.
   */
  Mat4 matrix;

  /**
   * @brief The view of this portal's destination, from the renderer's pool, or `NULL` if this
   * portal was not added to a view this frame.
   */
  struct RenderView *view;

  /**
   * @brief The layer of the portal texture this portal was drawn into this frame, or `-1`.
   * @details Cleared for every portal each frame, so a portal that was not added, or was added
   *   but culled, leaves its face on its own material rather than sampling a stale layer.
   */
  int32_t layer;

} RenderBspPortal;

/**
 * @brief A BSP light source, including shadow, style, and entity data.
 */
typedef struct {

  /**
   * @brief The entity that defines this light.
   */
  CmEntity *entity;

  /**
   * @brief The light origin.
   */
  Vec3 origin;

  /**
   * @brief The light color.
   */
  Vec3 color;

  /**
   * @brief The light radius.
   */
  float radius;

  /**
   * @brief The light intensity.
   */
  float intensity;

  /**
   * @brief The light bounds (sphere).
   */
  Box3 bounds;

  /**
   * @brief The occlusion query for this light.
   */
  RenderOcclusionQuery *query;

  /**
   * @brief The draw elements of this light's shadow geometry, into `bsp->draw_elements`.
   * @details One draw elements is emitted for all opaque faces lumped together, plus one per
   * unique alpha-test material visible to the light, so alpha-tested faces (foliage, fences,
   * grates) cast pixel-correct shadows.
   */
  RenderBspDrawElements *drawElements;

  /**
   * @brief The count of draw elements.
   */
  int32_t numDrawElements;

  /**
   * @brief The style string, a-z (26 levels), animated at 10Hz.
   */
  char style[MAX_BSP_ENTITY_VALUE];

  /**
   * @brief Phase offset (0-1 fraction of style cycle) to desynchronize instances with the same style.
   */
  float drift;

  /**
   * @brief The target entity for dynamic lights attached to inline model entities, or `NULL`.
   */
  CmEntity *targetEntity;
} RenderBspLight;

/**
 * @brief Individual voxel data for CPU-side access.
 */
typedef struct {

  /**
   * @brief The voxel's world-space bounds.
   */
  Box3 bounds;

  /**
   * @brief The voxel's combined contents mask.
   */
  int32_t contents;

  /**
   * @brief The lights affecting this voxel.
   */
  const RenderBspLight **lights;

  /**
   * @brief The number of lights affecting this voxel.
   */
  int32_t numLights;
} RenderBspVoxel;

/**
 * @brief The BSP voxel grid, including light index data for clustered forward lighting.
 */
typedef struct {

  /**
   * @brief The grid dimensions in voxels.
   */
  Vec3i size;

  /**
   * @brief The total number of voxels.
   */
  int32_t numVoxels;

  /**
   * @brief The voxel bounds in world space.
   */
  Box3 bounds;

  /**
   * @brief Array of individual voxel data (for CPU-side access and debugging).
   */
  RenderBspVoxel *voxels;

  /**
   * @brief The voxel caustics 3D texture (`RGB8`): caustics direction+strength.
   */
  RenderImage *caustics;

  /**
   * @brief The voxel occlusion 3D texture (`RG8`): spatial occlusion (r) and sky exposure (g).
   */
  RenderImage *occlusion;

  /**
   * @brief Media placeholder for the per-voxel light data (see `light_data_buffer`).
   */
  RenderImage *lightData;

  /**
   * @brief The storage buffer of per-voxel light ranges.
   */
  Buffer *lightDataBuffer;

  /**
   * @brief Voxel light index texture to sample the index buffer (`R32I`).
   */
  RenderImage *lightIndices;

  /**
   * @brief The storage buffer backing the light index vector (`R32I`).
   */
  Buffer *lightIndicesBuffer;

  /**
   * @brief The length of `light_indices_buffer`.
   */
  int32_t numLightIndices;

} RenderBspVoxels;

/**
 * @brief The renderer representation of the BSP model.
 */
typedef struct {

  /**
   * @brief The backing collision BSP model.
   */
  const CmBsp *cm;

  /**
   * @brief The count of planes.
   */
  int32_t numPlanes;

  /**
   * @brief The planes array.
   */
  RenderBspPlane *planes;

  /**
   * @brief The count of materials.
   */
  int32_t numMaterials;

  /**
   * @brief The materials array.
   */
  RenderMaterial **materials;

  /**
   * @brief The count of brush sides.
   */
  int32_t numBrushSides;

  /**
   * @brief The brush sides array.
   */
  RenderBspBrushSide *brushSides;

  /**
   * @brief The count of patches.
   */
  int32_t numPatches;

  /**
   * @brief The patches array.
   */
  RenderBspPatch *patches;

  /**
   * @brief The count of vertexes.
   */
  int32_t numVertexes;

  /**
   * @brief The vertexes array.
   */
  RenderBspVertex *vertexes;

  /**
   * @brief The count of elements.
   */
  int32_t numElements;

  /**
   * @brief The elements array.
   */
  uint32_t *elements;

  /**
   * @brief The count of faces.
   */
  int32_t numFaces;

  /**
   * @brief The faces array.
   */
  RenderBspFace *faces;

  /**
   * @brief The count of draw elements.
   */
  int32_t numDrawElements;

  /**
   * @brief The draw elements array.
   */
  RenderBspDrawElements *drawElements;

  /**
   * @brief The count of nodes.
   */
  int32_t numNodes;

  /**
   * @brief The nodes array.
   */
  RenderBspNode *nodes;

  /**
   * @brief The count of leafs.
   */
  int32_t numLeafs;

  /**
   * @brief The leafs array.
   */
  RenderBspLeaf *leafs;

  /**
   * @brief The count of blocks.
   */
  int32_t numBlocks;

  /**
   * @brief The blocks array.
   */
  RenderBspBlock *blocks;

  /**
   * @brief The count of inline models.
   */
  int32_t numInlineModels;

  /**
   * @brief The inline models array.
   */
  RenderBspInlineModel *inlineModels;

  /**
   * @brief The count of lights.
   */
  int32_t numLights;

  /**
   * @brief The lights array.
   */
  RenderBspLight *lights;

  /**
   * @brief The count of portals.
   */
  int32_t numPortals;

  /**
   * @brief The portals array.
   */
  RenderBspPortal *portals;

  /**
   * @brief The voxel data.
   */
  RenderBspVoxels voxels;

  /**
   * @brief The vertex array (VAO) name.
   */
  uint32_t vertexArray;

  /**
   * @brief The vertex buffer.
   */
  Buffer *vertexBuffer;

  /**
   * @brief The elements (index) buffer.
   */
  Buffer *elementsBuffer;
  struct {

    /**
     * @brief The depth pass vertex array (VAO) name.
     */
    uint32_t vertexArray;

  /**
   * @brief The depth pass vertex array.
   */
  } depthPass;

  /**
   * @brief The first inline BSP model, aka worldspawn.
   */
  struct RenderModel *worldspawn;

  /**
   * @brief The sky cubemap texture (RGB8).
   */
  RenderImage *sky;

} RenderBspModel;

/**
 * @brief The mesh vertex type.
 */
typedef struct {

  /**
   * @brief The vertex position.
   */
  Vec3 position;

  /**
   * @brief The vertex normal.
   */
  Vec3 normal;

  /**
   * @brief The vertex tangent, for per-pixel lighting.
   */
  Vec3 tangent;

  /**
   * @brief The vertex bitangent, for per-pixel lighting.
   */
  Vec3 bitangent;

  /**
   * @brief The diffusemap texture coordinate.
   */
  Vec2 diffusemap;
} RenderMeshVertex;

/**
 * @brief The mesh frame type.
 */
typedef struct {

  /**
   * @brief The frame bounds.
   */
  Box3 bounds;

  /**
   * @brief The frame translation offset.
   */
  Vec3 translate;
} RenderMeshFrame;

/**
 * @brief A mesh attachment tag.
 */
typedef struct {

  /**
   * @brief The tag name.
   */
  char name[MAX_QPATH];

  /**
   * @brief The tag matrix.
   */
  Mat4 matrix;
} RenderMeshTag;

/**
 * @brief A mesh face.
 */
typedef struct {

  /**
   * @brief The face name. This is used to resolve the material.
   */
  char name[MAX_QPATH];

  /**
   * @brief The material.
   */
  RenderMaterial *material;

  /**
   * @brief The vertexes.
   */
  RenderMeshVertex *vertexes;

  /**
   * @brief The count of vertexes.
   */
  int32_t numVertexes;

  /**
   * @brief The elements.
   */
  uint32_t *elements;

  /**
   * @brief The count of elements.
   */
  int32_t numElements;

  /**
   * @brief The base vertex in the shared mesh VAO.
   */
  int32_t baseVertex;

  /**
   * @brief The elements pointer in the shared mesh VAO.
   */
  void *indices;
} RenderMeshFace;

/**
 * @brief Max mesh model faces (matches MD3_MAX_SURFACES, the larger of the
 * two format-specific per-model face/surface limits, so that fixed-size
 * per-face skins arrays are never overrun by a legitimately oversized MD3).
 */
#define MAX_MESH_FACES MD3_MAX_SURFACES

/**
 * @brief The mesh animation type.
 */
typedef struct {

  /**
   * @brief The index of the first frame.
   */
  int32_t firstFrame;

  /**
   * @brief The total number of frames.
   */
  int32_t numFrames;

  /**
   * @brief The number of frames that loop.
   */
  int32_t loopedFrames;

  /**
   * @brief The animation playback rate in frames per second.
   */
  int32_t hz;
} RenderMeshAnimation;

/**
 * @brief Provides load-time normalization of mesh models.
 */
typedef struct {
  /**
   * @brief The translation component.
   */
  Vec3 translate;

  /**
   * @brief The rotation component (Euler angles).
   */
  Vec3 rotate;

  /**
   * @brief The scale component.
   */
  float scale;

  /**
   * @brief The muzzle position in model space.
   */
  Vec3 muzzle;

  /**
   * @brief The normalization transform matrix.
   */
  Mat4 transform;
} RenderMeshConfig;

/**
 * @brief Mesh model flags, parsed from `animation.cfg`.
 */
typedef enum {
  /**
   * @brief The legs do not rotate independently of the movement direction; they always
   * follow the torso's yaw. Set by the `fixedlegs` directive.
   */
  MESH_MODEL_FIXED_LEGS = (1 << 0),

  /**
   * @brief The torso does not pitch independently of the view angle. Set by the
   * `fixedtorso` directive.
   */
  MESH_MODEL_FIXED_TORSO = (1 << 1)
} RenderMeshModelFlags;

/**
 * @brief The mesh model type.
 */
typedef struct {

  /**
   * @brief The mesh model flags (see `RenderMeshModelFlags`).
   */
  uint32_t flags;

  /**
   * @brief The model's `sounds` directive from `animation.cfg`.
   */
  char sounds[MAX_QPATH];

  /**
   * @brief The vertex array.
   */
  RenderMeshVertex *vertexes;

  /**
   * @brief The count of vertexes.
   */
  int32_t numVertexes;

  /**
   * @brief The elements array.
   */
  uint32_t *elements;

  /**
   * @brief The count of elements.
   */
  int32_t numElements;

  /**
   * @brief The animation frames.
   */
  RenderMeshFrame *frames;

  /**
   * @brief The count of frames.
   */
  int32_t numFrames;

  /**
   * @brief The model tags.
   */
  RenderMeshTag *tags;

  /**
   * @brief The count of tags.
   */
  int32_t numTags;

  /**
   * @brief The model faces.
   */
  RenderMeshFace *faces;

  /**
   * @brief The count of faces.
   */
  int32_t numFaces;

  /**
   * @brief The animations.
   */
  RenderMeshAnimation *animations;

  /**
   * @brief The count of animations.
   */
  int32_t numAnimations;

  /**
   * @brief The base vertex in the shared mesh VAO.
   */
  int32_t baseVertex;

  /**
   * @brief The indices pointer in the shared mesh VAO.
   */
  void *indices;

  /**
   * @brief The transform and normalization configurations.
   */
  struct {
    /**
     * @brief The world-space normalization configuration.
     */
    RenderMeshConfig world;

    /**
     * @brief The view-space normalization configuration.
     */
    RenderMeshConfig view;

    /**
     * @brief The link-space normalization configuration.
     */
    RenderMeshConfig link;

  /**
   * @brief The mesh normalization configurations.
   */
  } config;

  /**
   * @brief The GPU vertex buffer holding all frames of all faces.
   */
  Buffer *vertexBuffer;

  /**
   * @brief The GPU elements (index) buffer.
   */
  Buffer *elementsBuffer;
} RenderMeshModel;

/**
 * @brief Models represent a subset of the BSP or a mesh.
 */
typedef struct RenderModel {

  /**
   * @brief The media.
   */
  RenderMedia media;

  /**
   * @brief The model type.
   */
  RenderModelType type;
  union {

    /**
     * @brief The BSP model data.
     */
    RenderBspModel *bsp;

    /**
     * @brief The inline BSP model data.
     */
    RenderBspInlineModel *bspInline;

    /**
     * @brief The mesh model data.
     */
    RenderMeshModel *mesh;
  };

  /**
   * @brief The model bounds.
   */
  Box3 bounds;

  /**
   * @brief The model bounding radius.
   */
  float radius;
} RenderModel;

#define IS_BSP_MODEL(m) (m && m->type == MODEL_BSP)
#define IS_BSP_INLINE_MODEL(m) (m && m->type == MODEL_BSP_INLINE)
#define IS_MESH_MODEL(m) (m && m->type == MODEL_MESH)
#define IS_WORLDSPAWN(m) (IS_BSP_MODEL(rModels.world) && IS_BSP_INLINE_MODEL(m) && rModels.world->bsp->worldspawn == m)

/**
 * @brief The model format type.
 */
typedef struct {

  /**
   * @brief The file extension.
   */
  const char *extension;

  /**
   * @brief The model type.
   */
  RenderModelType type;

  /**
   * @brief The load function.
   */
  void (*Load)(RenderModel *mod, void *buffer);

  /**
   * @brief The media registration callback.
   */
  void (*Register)(RenderMedia *self);

  /**
   * @brief The media free callback.
   */
  void (*Free)(RenderMedia *self);
} RenderModelFormat;

/**
 * @brief The models type.
 */
typedef struct {

  /**
   * @brief The currently loaded BSP model, if any.
   */
  RenderModel *world;

} RenderModels;

/**
 * @brief The models instance.
 */
extern RenderModels rModels;

/**
 * @brief Sprite rendering flags.
 */
enum {
  SPRITE_BEAM_REPEAT = 1 << 0,
  SPRITE_AXIAL       = 1 << 1,
  SPRITE_CGAME       = 1 << 16
};

typedef uint32_t RenderSpriteFlags;

/**
 * @brief Sprite billboard axis constraints.
 */
typedef enum {
  SPRITE_AXIS_ALL = 0,
  SPRITE_AXIS_X   = 1,
  SPRITE_AXIS_Y   = 2,
  SPRITE_AXIS_Z   = 4
} RenderSpriteBillboardAxis;

/**
 * @brief Sprites are billboarded alpha blended quads, optionally animated.
 */
typedef struct {

  /**
   * @brief The sprite origin.
   */
  Vec3 origin;

  /**
   * @brief The sprite size; if set, this is used for both width & height, otherwise width/height are used.
   */
  float size;

  /**
   * @brief The sprite width.
   */
  float width;

  /**
   * @brief The sprite width.
   */
  float height;

  /**
   * @brief The sprite media (an `RenderAnimation`, `RenderImage`, etc).
   */
  RenderMedia *media;

  /**
   * @brief The sprite's rotation, for non-beam sprites.
   */
  float rotation;

  /**
   * @brief The sprite color. Components above 1 are permitted, and reach the
   * HDR scene target unscaled.
   */
  Vec3 color;

  /**
   * @brief The sprite's life from 0 to 1.
   */
  float life;

  /**
   * @brief Direction of the sprite. { 0, 0, 0 } is billboard.
   */
  Vec3 dir;

  /**
   * @brief Axis modifier for billboard sprites.
   */
  RenderSpriteBillboardAxis axis;

  /**
   * @brief Sprite flags
   */
  RenderSpriteFlags flags;

  /**
   * @brief Sprite lighting mix factor. 0 is fullbright, 1 is fully affected by light.
   */
  float lighting;
} RenderSprite;

#define MAX_SPRITES    0x8000

/**
 * @brief Beams are segmented sprites.
 */
typedef struct {

  /**
   * @brief The beam start.
   */
  Vec3 start;

  /**
   * @brief The beam end.
   */
  Vec3 end;

  /**
   * @brief The beam size.
   */
  float size;

  /**
   * @brief The beam texture.
   */
  RenderImage *image;

  /**
   * @brief The beam color. Components above 1 are permitted, and reach the
   * HDR scene target unscaled.
   */
  Vec3 color;

  /**
   * @brief The beam texture translation.
   */
  float translate;

  /**
   * @brief The beam texture stretch.
   */
  float stretch;

  /**
   * @brief The beam flags.
   */
  RenderSpriteFlags flags;

  /**
   * @brief Beam lighting mix factor. 0 is fullbright, 1 is fully affected by light.
   */
  float lighting;
} RenderBeam;

#define MAX_BEAMS 0x200

/**
 * @brief The maximum number of portals drawn for a single view.
 * @details Each portal is a whole scene, rendered into its own layer of one texture, so this
 *   bounds both the per-frame cost and the memory a map can demand. It is deliberately far
 *   below `MAX_BSP_PORTALS`, which bounds only how many a map may contain: the client game
 *   offers the nearest of them, and the renderer draws those it can see.
 */
#define MAX_PORTALS 8

/**
 * @brief Vec4-aligned instance of a sprite or beam quad, as consumed by sprite_vs.
 * @remarks Sprites and beams reduce to the same quad, a center and two half
 * axes, so both are drawn from this one type. The four corners are
 * `center + (±a) + (±b)`, which sprite_vs derives from `gl_VertexIndex`. Must
 * match `sprite_instance_t` in sprite_vs.glsl.
 */
typedef struct {

  /**
   * @brief The quad center, and the animation interpolation factor in `w`.
   */
  alignas(16) Vec4 center;

  /**
   * @brief The first half axis, and the lighting intensity in `w`.
   */
  Vec4 a;

  /**
   * @brief The second half axis.
   */
  Vec4 b;

  /**
   * @brief The diffusemap rect: `xy` min, `zw` max.
   */
  Vec4 texcoords;

  /**
   * @brief The next diffusemap rect, for animation.
   */
  Vec4 nextTexcoords;

  /**
   * @brief The quad color.
   */
  Vec4 color;
} RenderSpriteInstance;

static_assert(sizeof(RenderSpriteInstance) == 96, "RenderSpriteInstance must match sprite_instance_t in sprite_vs.glsl");

/**
 * @brief The batching state for a sprite instance, parallel to it by index.
 */
typedef struct {

  /**
   * @brief The diffusemap texture.
   */
  const RenderImage *diffusemap;

  /**
   * @brief The next diffusemap texture, for animation interpolation.
   */
  const RenderImage *nextDiffusemap;

  /**
   * @brief The sprite bounds.
   */
  Box3 bounds;
} RenderSpriteBatch;

#define MAX_SPRITE_INSTANCES (MAX_SPRITES + MAX_BEAMS)

/**
 * @brief Mirrors the game modules' `EF_MODULATE` (`g_types.h`), which is identical
 * across all three modules. The renderer has no visibility into a per-module
 * header, so this bit is redeclared here rather than shared by inclusion.
 */
#define EF_MODULATE (EF_GAME << 17)

/**
 * @brief Renderer-local entity effect bits.
 */
#define EF_SELF      (1 << 23)
#define EF_WEAPON    (1 << 24)
#define EF_SHELL     (1 << 25)
#define EF_BLEND     (1 << 26)
#define EF_NO_SHADOW (1 << 27)
#define EF_NO_DRAW   (1 << 28)

/**
 * @brief A renderable entity instance.
 */
typedef struct RenderEntity {

  /**
   * @brief The entity identifier.
   */
  const void *id;

  /**
   * @brief The parent entity, if any, for linked mesh models.
   */
  const struct RenderEntity *parent;

  /**
   * @brief The tag name, if any, for linked mesh models.
   */
  const char *tag;

  /**
   * @brief The entity origin.
   */
  Vec3 origin;

  /**
   * @brief The entity termination for beams.
   */
  Vec3 termination;

  /**
   * @brief The entity angles.
   */
  Vec3 angles;

  /**
   * @brief The entity scale, for mesh models.
   */
  float scale;

  /**
   * @brief The relative entity bounds, as known by the client.
   */
  Box3 bounds;

  /**
   * @brief The absolute entity bounds, as known by the client.
   */
  Box3 absBounds;

  /**
   * @brief The visual model bounds, in world space, for frustum culling.
   */
  Box3 absModelBounds;

  /**
   * @brief The cached dynamic light bitmask for this entity.
   */
  RenderActiveDynamicLights activeDynamicLights;

  /**
   * @brief The model matrix.
   */
  Mat4 matrix;

  /**
   * @brief The inverse model matrix.
   */
  Mat4 inverseMatrix;

  /**
   * @brief The model, if any.
   */
  const RenderModel *model;

  /**
   * @brief Frame animations.
   */
  int32_t frame, oldFrame;

  /**
   * @brief Frame interpolation.
   */
  float lerp, backLerp;

  /**
   * @brief Mesh model skins, up to one per face.
   *
   * Only meaningful when `has_skins` is `true` (see below). In that case, a
   * `NULL` entry means the face has no skin and should not be drawn at all,
   * rather than falling back to the mesh's baked-in default material.
   */
  RenderMaterial *skins[MAX_MESH_FACES];

  /**
   * @brief Whether `skins` is populated and authoritative for this entity.
   *
   * `false` means this entity does not use per-face skins; every face falls
   * back to its mesh's baked-in default material (`face->material`). `true`
   * means `skins` is authoritative for each face: a `NULL` entry explicitly
   * means "do not draw this face" (see `Cg_LoadClientSkins`).
   */
  bool hasSkins;

  /**
   * @brief The entity effects (`EF_NO_DRAW`, `EF_WEAPON`, ..).
   */
  int32_t effects;

  /**
   * @brief The entity shade color.
   */
  Vec4 color;

  /**
   * @brief The entity shell color for flag carriers, etc.
   */
  Vec4 shell;

  /**
   * @brief Tint maps allow users to customize their player skins.
   */
  Vec4 tints[TINT_TOTAL];

} RenderEntity;

/**
 * @brief Light sources per scene.
 */
#define MAX_LIGHTS (MAX_BSP_LIGHTS + MAX_DYNAMIC_LIGHTS)

/**
 * @brief The maximum number of shadow-caster entity references per frame.
 */
#define MAX_SHADOW_CASTERS (MAX_ENTITIES * 4)

/**
 * @brief Hardware light source flags.
 */
#define R_LIGHT_NO_SHADOW (1 << 0)

/**
 * @brief Hardware light sources.
 */
typedef struct {
  /**
   * @brief The light flags.
   */
  int32_t flags;

  /**
   * @brief The light origin.
   */
  Vec3 origin;

  /**
   * @brief The light color.
   */
  Vec3 color;

  /**
   * @brief The light radius.
   */
  float radius;

  /**
   * @brief The light intensity.
   */
  float intensity;

  /**
   * @brief The light bounds, or the volume visible to the light.
   */
  Box3 bounds;

  /**
   * @brief The backing BSP light, for static light sources.
   */
  const RenderBspLight *bspLight;

  /**
   * @brief The optional light source entity identifier.
   */
  const void *source;

  /**
   * @brief True if the light is occluded for the current frame.
   */
  bool occluded;

  /**
   * @brief The shadow-casting entities intersecting this light.
   */
  const RenderEntity *entities[MAX_ENTITIES];

  /**
   * @brief The count of intersecting entities.
   */
  int32_t numEntities;

  /**
   * @brief The shadow atlas tile origin in pixels.
   */
  Vec2 tile;

  /**
   * @brief The hash of this light's shadow map inputs, or `0` if it casts none this frame.
   */
  uint64_t hash;
} RenderLight;

/**
 * @brief View types.
 */
typedef enum {
  VIEW_UNKNOWN,
  VIEW_MAIN,
  VIEW_PLAYER_MODEL,
  VIEW_PORTAL,
} RenderViewType;

/**
 * @brief View flags.
 */
typedef enum {
  VIEW_FLAG_NONE = 0x0,
  VIEW_FLAG_NO_DELTA = 0x1
} RenderViewFlags;

/**
 * @brief Draw statistics, accumulated by the renderer for each view it draws.
 */
typedef struct {

  /**
   * @brief The count of visible lights.
   */
  int32_t lightsVisible;

  /**
   * @brief The count of occluded lights.
   */
  int32_t lightsOccluded;

  /**
   * @brief The count of lights with cached shadowmaps.
   */
  int32_t lightsCached;

  /**
   * @brief The count of visible entities.
   */
  int32_t entitiesVisible;

  /**
   * @brief The count of occluded entities.
   */
  int32_t entitiesOccluded;

  /**
   * @brief The count of visible (non-occluded) BSP blocks.
   */
  int32_t blocksVisible;

  /**
   * @brief The count of occluded BSP blocks.
   */
  int32_t blocksOccluded;

  /**
   * @brief The count of currently allocated occlusion queries.
   */
  int32_t queriesAllocated;

  /**
   * @brief The count of visible occlusion queries this frame.
   */
  int32_t queriesVisible;

  /**
   * @brief The count of occluded occlusion queries this frame.
   */
  int32_t queriesOccluded;

  /**
   * @brief The counts of portals the client game offered, and of those actually drawn.
   */
  int32_t portalsOffered, portalsDrawn;

  /**
   * @brief The count of triangles drawn into portal views this frame.
   */
  int32_t portalsTriangles;

  /**
   * @brief The count of rendered inline BSP models.
   */
  int32_t bspInlineModels;

  /**
   * @brief The count of rendered BSP draw element batches.
   */
  int32_t bspDrawElements;

  /**
   * @brief The count of rendered BSP triangles.
   */
  int32_t bspTriangles;

  /**
   * @brief The count of rendered mesh models.
   */
  int32_t meshModels;

  /**
   * @brief The count of rendered mesh draw element batches.
   */
  int32_t meshDrawElements;

  /**
   * @brief The count of rendered mesh triangles.
   */
  int32_t meshTriangles;

  /**
   * @brief The count of rendered sprite draw element batches.
   */
  int32_t spriteDrawElements;

  /**
   * @brief The count of rendered decal draw element batches.
   */
  int32_t decalDrawElements;
} RenderViewStats;

/**
 * @brief Each client frame populates a view, and submits it to the renderer.
 */
typedef struct RenderView {

  /**
   * @brief The view type.
   */
  RenderViewType type;

  /**
   * @brief The view flags.
   */
  RenderViewFlags flags;

  /**
   * @brief The target scene framebuffer.
   */
  Framebuffer *framebuffer;

  /**
   * @brief The viewport, in device pixels.
   */
  Vec4i viewport;

  /**
   * @brief The horizontal and vertical field of view.
   */
  Vec2 fov;

  /**
   * @brief The depth range; near and far clipping plane distances.
   */
  Vec2 depthRange;

  /**
   * @brief The view origin.
   */
  Vec3 origin;

  /**
   * @brief The view angles.
   */
  Vec3 angles;

  /**
   * @brief The forward vector, derived from angles.
   */
  Vec3 forward;

  /**
   * @brief The right vector, derived from angles.
   */
  Vec3 right;

  /**
   * @brief The up vector, derived from angles.
   */
  Vec3 up;

  /**
   * @brief The contents mask at the view origin.
   */
  int32_t contents;

  /**
   * @brief The unclamped simulation time, in millis.
   */
  uint32_t ticks;

  /**
   * @brief The ambient modulation, per channel.
   */
  Vec3 ambient;

  /**
   * @brief The entities to render for the current frame.
   */
  RenderEntity entities[MAX_ENTITIES];

  /**
   * @brief The count of entities.
   */
  int32_t numEntities;

  /**
   * @brief The sprites to render for the current frame.
   */
  RenderSprite sprites[MAX_SPRITES];

  /**
   * @brief The count of sprites.
   */
  int32_t numSprites;

  /**
   * @brief The beams to render for the current frame.
   */
  RenderBeam beams[MAX_BEAMS];

  /**
   * @brief The count of beams.
   */
  int32_t numBeams;

  /**
   * @brief The portals whose views are drawn for this view to sample.
   */
  RenderBspPortal *portals[MAX_PORTALS];

  /**
   * @brief The count of portals.
   */
  int32_t numPortals;

  /**
   * @brief The batching state for the current frame's sprite instances.
   */
  RenderSpriteBatch spriteBatches[MAX_SPRITE_INSTANCES];

  /**
   * @brief The count of sprite instances.
   */
  int32_t numSpriteInstances;

  /**
   * @brief The lights to render for the current frame.
   */
  RenderLight lights[MAX_LIGHTS];

  /**
   * @brief The count of lights.
   */
  int32_t numLights;

  /**
   * @brief New decals added this frame, to be processed during `R_UpdateDecals`.
   */
  RenderDecal decals[MAX_DECALS];

  /**
   * @brief The count of decals.
   */
  int32_t numDecals;

  /**
   * @brief The view frustum, for box and sphere culling.
   */
  CmBspPlane frustum[4];

  /**
   * @brief Draw statistics for the most recent render of this view.
   */
  RenderViewStats stats;
} RenderView;

/**
 * @brief Window and GPU device information.
 */
typedef struct {

  /**
   * @brief The display associated with the application window.
   */
  SDL_DisplayID display;

  /**
   * @brief The display mode.
   */
  const SDL_DisplayMode *displayMode;

  /**
   * @brief The display usable bounds, which may be smaller than the display mode resolution.
   */
  SDL_Rect displayUsableBounds;

  /**
   * @brief The application window.
   */
  SDL_Window *window;

  /**
   * @brief The window flags.
   */
  SDL_WindowFlags windowFlags;

  /**
   * @brief The window position and size in logical pixels.
   */
  SDL_Rect windowBounds;

  /**
   * @brief The GPU render device.
   */
  RenderDevice *device;

  /**
   * @brief A 1x1 opaque white @c Texture, useful for binding to unused @c Samplers.
   */
  Texture *nullTexture;
} RenderContext;

#if defined(__R_LOCAL_H__)

/**
 * @brief Cached material-stage pipeline state.
 */
typedef struct {
  /**
   * @brief The blend operators.
   */
  CmBlend src, dest;

  /**
   * @brief The depth write flag.
   */
  bool depthWrite;

  /**
   * @brief The cached pipeline.
   */
  GraphicsPipeline *pipeline;
} RenderStagePipeline;

#endif
