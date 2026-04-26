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

#include <SDL3/SDL_video.h>

#include "common/atlas.h"
#include "collision/cm_bsp.h"

#include "r_gl.h"

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
} r_media_type_t;

/**
 * @brief Images, atlases, models, materials, etc. are all managed as media.
 */
typedef struct r_media_s {
  char name[MAX_QPATH]; ///< The media name.
  r_media_type_t type; ///< The media type.
  GList *dependencies; ///< The media on which this media depends.
  void (*Register)(struct r_media_s *self); ///< The media registration callback.
  bool (*Retain)(struct r_media_s *self); ///< The media retain callback, to avoid being freed.
  void (*Free)(struct r_media_s *self); ///< The free callback, to release any system resources.
  int32_t seed; ///< The media seed, to determine if this media is current.
} r_media_t;

/**
 * @brief Model types.
 */
typedef enum {
  MOD_INVALID,
  MODEL_BSP,
  MODEL_BSP_INLINE,
  MODEL_MESH
} r_model_type_t;

/**
 * @brief Image types.
 */
typedef enum {
  IMG_PROGRAM = 1,
  IMG_FONT,
  IMG_UI,
  IMG_PIC,
  IMG_SPRITE,
  IMG_ATLAS,
  IMG_MATERIAL,
  IMG_CUBEMAP,
  IMG_VOXELS,
} r_image_type_t;

/**
 * @brief Images are referenced by materials, models, entities, particles, etc.
 */
typedef struct {
  r_media_t media; ///< The media.
  r_image_type_t type; ///< The image type.
  GLint width, height, depth; ///< The image width, height and depth (or layers).
  GLenum target; ///< The target to bind this texture.
  GLsizei levels; ///< The number of mipmap levels to allocate, typically `log2(Maxi(w, h)) + 1`.
  GLenum minify, magnify; ///< The minification and magnification filters, typically `GL_LINEAR`.
  GLenum internal_format; ///< The internal pixel format, typically `GL_RGB` or `GL_RGBA`, but may be a sized value.
  GLenum format; ///< The pixel format, typically `GL_RGB` or `GL_RGBA`.
  GLenum pixel_type; ///< The pixel data type, typically `GL_UNSIGNED_BYTE`.
  GLuint buffer; ///< The buffer object name, for `GL_TEXTURE_BUFFER` (TBO).
  GLuint texnum; ///< The texture name.
} r_image_t;

/**
 * @brief An image atlas.
 */
typedef struct {
  r_media_t media; ///< The media.
  atlas_t *atlas; ///< The atlas.
  r_image_t *image; ///< The compiled image atlas containing all nodes.
  bool dirty; ///< True if this at atlas should be recompiled.
} r_atlas_t;

/**
 * @brief An atlas image, castable to `r_image_t` and `r_media_t`.
 */
typedef struct {
  r_image_t image; ///< The image.
  atlas_node_t *node; ///< The atlas node that created this atlas image.
  vec4_t texcoords; ///< The texture coordinates of this atlas image within the atlas.
} r_atlas_image_t;

/**
 * @brief An animation, castable to r_media_t.
 */
typedef struct {
  r_media_t media; ///< The media.
  int32_t num_frames; ///< The number of frames in this animation.
  const r_image_t **frames; ///< The frames in this animation.
} r_animation_t;

/**
 * @brief Material stages.
 */
typedef struct r_stage_s {
  const cm_stage_t *cm; ///< The backing collision material stage.
  r_media_t *media; ///< Stages with a render pass will reference an image, atlas image, material, animation, etc.
  struct r_stage_s *next; ///< The next stage in the material.
} r_stage_t;

/**
 * @brief Materials define texture, animation and lighting properties for BSP and mesh models.
 */
typedef struct r_material_s {
  r_media_t media; ///< Materials are media.
  cm_material_t *cm; ///< The collision material definition.
  r_image_t *texture; ///< The layered texture containing the diffusemap, normalmap and specularmap.
  r_stage_t *stages; ///< Animated stage definitions.
  uint32_t ticks; ///< The time when this material was last animated.
  color_t color; ///< The diffusemap color.
} r_material_t;

/**
 * @brief Decals are projected textures that conform to BSP geometry.
 */
typedef struct r_decal_s {
  vec3_t origin; ///< The decal origin.
  float radius; ///< The decal radius.
  color_t color; ///< The decal color.
  r_atlas_image_t *image; ///< The decal atlas image.
  uint32_t time; ///< The decal creation time in ticks.
  uint32_t lifetime; ///< The decal lifetime in ticks (0 = permanent).
  float rotation; ///< The decal rotation angle in radians.
} r_decal_t;

#define MAX_DECALS 0x400

/**
 * @brief OpenGL occlusion queries.
 */
typedef struct r_occlusion_query_s {
  GLuint name; ///< The query name.
  box3_t bounds; ///< The query bounds.
  GLint base_vertex; ///< The base vertex in the shared vertex buffer.
  GLint available; ///< Non-zero if the query is available.
  GLint result; ///< Non-zero of the query produced visible fragments.
} r_occlusion_query_t;

/**
 * @brief BSP plane structure.
 */
typedef struct {
  const cm_bsp_plane_t *cm; ///< The collision plane.
} r_bsp_plane_t;

/**
 * @brief BSP brush side structure.
 */
typedef struct {
  const r_bsp_plane_t *plane; ///< The plane.
  const r_material_t *material; ///< The material.
  vec4_t axis[2]; ///< The texture axis for S and T, in xyz + offset notation.
  int32_t contents; ///< The brush contents.
  int32_t surface; ///< The surface flags.
  int32_t value; ///< The surface value, for lights or Phong grouping.
} r_bsp_brush_side_t;

/**
 * @brief BSP patch structure, resolved from bsp_patch_t.
 */
typedef struct {
  const r_material_t *material; ///< The material.
  int32_t contents; ///< The brush contents.
  int32_t surface; ///< The surface flags.
} r_bsp_patch_t;

/**
 * @brief BSP vertex structure.
 */
typedef struct {
  vec3_t position; ///< The position.
  vec3_t normal; ///< The normal, for Phong shading.
  vec3_t tangent; ///< The tangent, for per-pixel lighting.
  vec3_t bitangent; ///< The bitangent, for per-pixel lighting.
  vec2_t diffusemap; ///< The diffusemap texture coordinate.
  color32_t color; ///< The color, for alpha blending and vertex lighting effects.
} r_bsp_vertex_t;

/**
 * @brief BSP faces, which may reside on the front or back of their node.
 */
typedef struct r_bsp_face_s {
  r_bsp_brush_side_t *brush_side; ///< The brush side which generated this face, or NULL for patch faces.
  r_bsp_plane_t *plane; ///< The plane on which this face resides (to disambiguate `node`).
  r_bsp_patch_t *patch; ///< The patch which generated this face, or NULL for brush faces.
  struct r_bsp_node_s *node; ///< The node containing this face.
  struct r_bsp_block_s *block; ///< The block containing this face.
  box3_t bounds; ///< The AABB of this face.
  r_bsp_vertex_t *vertexes; ///< The vertexes.
  int32_t num_vertexes; ///< The count of vertexes.
  GLvoid *elements; ///< The elements.
  int32_t num_elements; ///< The count of elements.
} r_bsp_face_t;

/**
 * @brief BSP draw elements, which include all opaque faces of a given material
 * within a particular inline model.
 */
typedef struct {
  r_material_t *material; ///< The material.
  int32_t surface; ///< The surface flags.
  box3_t bounds; ///< The AABB of the elements.
  GLvoid *elements; ///< An offset pointer (in bytes) into the BSP elements array.
  int32_t num_elements; ///< The count of elements.
  vec2_t st_origin; ///< Texture coordinate origin for stage transforms (scale, stretch, rotate).
} r_bsp_draw_elements_t;

/**
 * @brief BSP nodes comprise the tree representation of the world.
 */
typedef struct r_bsp_node_s {
  int32_t contents; ///< The contents mask; one of `CONTENTS_NODE` or `CONTENTS_BLOCK` for nodes.
  box3_t bounds; ///< The AABB.
  struct r_bsp_node_s *parent; ///< The parent node.
  struct r_bsp_inline_model_s *model; ///< The inline model. Each inline model contains its own sub-tree.
  r_bsp_plane_t *plane; ///< The plane that created this node.
  struct r_bsp_node_s *children[2]; ///< The child nodes, which may be leaves.
  box3_t visible_bounds; ///< The AABB of visible faces within this node.
  r_bsp_face_t *faces; ///< The faces within this node.
  int32_t num_faces; ///< The count of faces.
} r_bsp_node_t;

/**
 * @brief BSP leafs terminate the branches of the BSP tree.
 * @remarks Leafs are truncated node structures so that they may be cast to `r_bsp_node_t`.
 */
typedef struct {
  int32_t contents; ///< The contents mask. Valid for leafs, always `CONTENTS_NODE` for nodes.
  box3_t bounds; ///< The AABB.
  struct r_bsp_node_s *parent; ///< The parent node.
  struct r_bsp_inline_model_s *model; ///< The inline model. Each inline model contains its own sub-tree.
} r_bsp_leaf_t;

/**
 * @brief The maximum number of decals that can be attached to a single BSP block.
 */
#define MAX_BSP_BLOCK_DECALS 0x1000

/**
 * @brief Decals are aggregated at the BSP block level.
 */
typedef struct {
  r_image_t *image; ///< The decal atlas, cast to `r_image_t` for texture binding.
  GArray *triangles; ///< The triangles of the decals attached to the containing block.
  GLuint vertex_buffer; ///< The decal vertex buffer object.
  GLuint elements_buffer; ///< The decal elements buffer object.
  GLuint vertex_array; ///< The decal vertex array object.
  bool dirty; ///< True if the containing block's decals require uploading.

} r_bsp_block_decals_t;

/**
 * @brief BSP blocks are large, axial-aligned, gridded nodes used to aggregate rendering operations.
 */
typedef struct r_bsp_block_s {
  r_bsp_node_t *node; ///< The `CONTENTS_BLOCK` node defining this block.
  r_bsp_draw_elements_t *draw_elements; ///< The draw elements within this block.
  int32_t num_draw_elements; ///< The count of draw elements.
  box3_t visible_bounds; ///< The visible bounds of this block, used for occlusion query and culling.
  int32_t surface; ///< The bitwise OR of all draw element surface flags for this block.
  r_occlusion_query_t *query; ///< The occlusion query for this block.
  r_bsp_block_decals_t decals; ///< The decals for this block.

} r_bsp_block_t;

/**
 * @brief The BSP is organized into one or more models (trees). The first model is
 * the worldspawn model, and typically is the largest. An additional model exists
 * for each entity that contains brushes. Non-worldspawn models can move and rotate.
 */
typedef struct r_bsp_inline_model_s {

  cm_entity_t *entity; ///< The backing entity definition for this inline model.
  r_bsp_node_t *head_node; ///< The head node of this inline model.
  box3_t visible_bounds; ///< For frustum culling.
  r_bsp_face_t *faces; ///< The faces of this inline model.
  int32_t num_faces; ///< The count of faces.
  GLvoid *depth_pass_elements; ///< The depth pass elements of this inline model.
  int32_t num_depth_pass_elements; ///< The count of depth pass elements.
  r_bsp_draw_elements_t *draw_elements; ///< The draw elements of this inline model.
  int32_t num_draw_elements; ///< The count of draw elements.
  r_bsp_block_t *blocks; ///< The blocks of this inline model.
  int32_t num_blocks; ///< The count of blocks.

} r_bsp_inline_model_t;

/**
 * @brief A BSP light source, including shadow, style, and entity data.
 */
typedef struct {
  cm_entity_t *entity; ///< The entity that defines this light.
  vec3_t origin; ///< The light origin.
  vec3_t color; ///< The light color.
  vec4_t normal; ///< The light normal and plane distance for directional lights.
  float radius; ///< The light radius.
  float intensity; ///< The light intensity.
  box3_t bounds; ///< The light bounds (sphere).
  r_occlusion_query_t *query; ///< The light occlusion query.
  GLvoid *depth_pass_elements; ///< An offset pointer (in bytes) into the BSP elements array for shadow geometry.
  int32_t num_depth_pass_elements; ///< The count of elements.
  char style[MAX_BSP_ENTITY_VALUE]; ///< The style string, a-z (26 levels), animated at 10Hz.
  bool shadow_cached; ///< True if this light's shadowmap can be reused from the previous frame.
  cm_entity_t *target_entity; ///< The target entity for dynamic lights attached to inline model entities, or NULL.
} r_bsp_light_t;

/**
 * @brief Individual voxel data for CPU-side access.
 */
typedef struct {
  box3_t bounds; ///< The voxel's world-space bounds.
  int32_t contents; ///< The voxel's combined contents mask.
  const r_bsp_light_t **lights; ///< The lights affecting this voxel.
  int32_t num_lights; ///< The number of lights affecting this voxel.
} r_bsp_voxel_t;

/**
 * @brief The BSP voxel grid, including light index data for clustered forward lighting.
 */
typedef struct {
  vec3i_t size; ///< The grid dimensions in voxels.
  int32_t num_voxels; ///< The total number of voxels.
  box3_t bounds; ///< The voxel bounds in world space.
  r_bsp_voxel_t *voxels; ///< Array of individual voxel data (for CPU-side access and debugging).
  r_image_t *data; ///< The voxel data 3D texture (RG8): caustics and exposure.
  r_image_t *light_data; ///< The light data 3D texture (RG32I) for offset and count pairs per voxel.
  r_image_t *light_indices; ///< Voxel light index texture to sample the index buffer (R32I).
  GLuint light_indices_buffer; ///< The buffer object backing the index vector.
  int32_t num_light_indices; ///< The length of `light_indices_buffer`.

} r_bsp_voxels_t;

/**
 * @brief The renderer representation of the BSP model.
 */
typedef struct {
  const cm_bsp_t *cm; ///< The backing collision BSP model.
  int32_t num_planes; ///< The count of planes.
  r_bsp_plane_t *planes; ///< The planes array.
  int32_t num_materials; ///< The count of materials.
  r_material_t **materials; ///< The materials array.
  int32_t num_brush_sides; ///< The count of brush sides.
  r_bsp_brush_side_t *brush_sides; ///< The brush sides array.
  int32_t num_patches; ///< The count of patches.
  r_bsp_patch_t *patches; ///< The patches array.
  int32_t num_vertexes; ///< The count of vertexes.
  r_bsp_vertex_t *vertexes; ///< The vertexes array.
  int32_t num_elements; ///< The count of elements.
  GLuint *elements; ///< The elements array.
  int32_t num_faces; ///< The count of faces.
  r_bsp_face_t *faces; ///< The faces array.
  int32_t num_draw_elements; ///< The count of draw elements.
  r_bsp_draw_elements_t *draw_elements; ///< The draw elements array.
  int32_t num_nodes; ///< The count of nodes.
  r_bsp_node_t *nodes; ///< The nodes array.
  int32_t num_leafs; ///< The count of leafs.
  r_bsp_leaf_t *leafs; ///< The leafs array.
  int32_t num_blocks; ///< The count of blocks.
  r_bsp_block_t *blocks; ///< The blocks array.
  int32_t num_inline_models; ///< The count of inline models.
  r_bsp_inline_model_t *inline_models; ///< The inline models array.
  int32_t num_lights; ///< The count of lights.
  r_bsp_light_t *lights; ///< The lights array.
  r_bsp_voxels_t voxels; ///< The voxel data.
  GLuint vertex_array; ///< The vertex array (VAO) name.
  GLuint vertex_buffer; ///< The vertex array buffer (VBO) name.
  GLuint elements_buffer; ///< The elements array buffer (VBO) name.
  struct {
    GLuint vertex_array; ///< The depth pass vertex array (VAO) name.
  } depth_pass; ///< The depth pass vertex array.
  struct r_model_s *worldspawn; ///< The first inline BSP model, aka worldspawn.

} r_bsp_model_t;

/**
 * @brief The mesh vertex type.
 */
typedef struct {
  vec3_t position; ///< The vertex position.
  vec3_t normal; ///< The vertex normal.
  vec3_t smooth_normal; ///< The smoothed vertex normal, for Phong shading.
  vec3_t tangent; ///< The vertex tangent, for per-pixel lighting.
  vec3_t bitangent; ///< The vertex bitangent, for per-pixel lighting.
  vec2_t diffusemap; ///< The diffusemap texture coordinate.
} r_mesh_vertex_t;

/**
 * @brief The mesh frame type.
 */
typedef struct {
  box3_t bounds; ///< The frame bounds.
  vec3_t translate; ///< The frame translation offset.
} r_mesh_frame_t;

/**
 * @brief The mesh tag type.
 * @details Tags are used to align submodels (e.g. weapons, CTF flags).
 */
typedef struct {
  char name[MAX_QPATH]; ///< The tag name.
  mat4_t matrix; ///< The tag matrix.
} r_mesh_tag_t;

/**
 * @brief The mesh face type.
 * @details A mesh model is comprised of one or more faces, which may have distinct materials.
 */
typedef struct {
  char name[MAX_QPATH]; ///< The face name. This is used to resolve the material.
  r_material_t *material; ///< The material.
  r_mesh_vertex_t *vertexes; ///< The vertexes.
  int32_t num_vertexes; ///< The count of vertexes.
  GLuint *elements; ///< The elements.
  int32_t num_elements; ///< The count of elements.
  GLint base_vertex; ///< The base vertex in the shared mesh VAO.
  GLvoid *indices; ///< The elements pointer in the shared mesh VAO.
} r_mesh_face_t;

/**
 * @brief Max mesh model faces.
 */
#define MAX_MESH_FACES 0x20

/**
 * @brief The mesh animation type.
 */
typedef struct {
  int32_t first_frame; ///< The index of the first frame.
  int32_t num_frames; ///< The total number of frames.
  int32_t looped_frames; ///< The number of frames that loop.
  int32_t hz; ///< The animation playback rate in frames per second.
} r_mesh_animation_t;

/**
 * @brief Provides load-time normalization of mesh models.
 */
typedef struct {
  mat4_t transform; ///< The normalization transform matrix.
} r_mesh_config_t;

/**
 * @brief The mesh model type.
 */
typedef struct {
  uint32_t flags; ///< The mesh model flags.
  r_mesh_vertex_t *vertexes; ///< The vertex array.
  int32_t num_vertexes; ///< The count of vertexes.
  GLuint *elements; ///< The elements array.
  int32_t num_elements; ///< The count of elements.
  r_mesh_frame_t *frames; ///< The animation frames.
  int32_t num_frames; ///< The count of frames.
  r_mesh_tag_t *tags; ///< The model tags.
  int32_t num_tags; ///< The count of tags.
  r_mesh_face_t *faces; ///< The model faces.
  int32_t num_faces; ///< The count of faces.
  r_mesh_animation_t *animations; ///< The animations.
  int32_t num_animations; ///< The count of animations.
  GLint base_vertex; ///< The base vertex in the shared mesh VAO.
  GLvoid *indices; ///< The indices pointer in the shared mesh VAO.
  struct {
    r_mesh_config_t world; ///< The world-space normalization configuration.
    r_mesh_config_t view; ///< The view-space normalization configuration.
    r_mesh_config_t link; ///< The link-space normalization configuration.
  } config; ///< The mesh normalization configurations.
} r_mesh_model_t;

/**
 * @brief Models represent a subset of the BSP or a mesh.
 */
typedef struct r_model_s {
  r_media_t media; ///< The media.
  r_model_type_t type; ///< The model type.
  union {
    r_bsp_model_t *bsp; ///< The BSP model data.
    r_bsp_inline_model_t *bsp_inline; ///< The inline BSP model data.
    r_mesh_model_t *mesh; ///< The mesh model data.
  };
  box3_t bounds; ///< The model bounds.
  float radius; ///< The model bounding radius.
} r_model_t;

#define IS_BSP_MODEL(m) (m && m->type == MODEL_BSP)
#define IS_BSP_INLINE_MODEL(m) (m && m->type == MODEL_BSP_INLINE)
#define IS_WORLDSPAWN(m) (m && m == r_models.world->bsp->worldspawn)
#define IS_MESH_MODEL(m) (m && m->type == MODEL_MESH)

/**
 * @brief The model format type.
 */
typedef struct {
  const char *extension; ///< The file extension.
  r_model_type_t type; ///< The model type.
  void (*Load)(r_model_t *mod, void *buffer); ///< The load function.
  void (*Register)(r_media_t *self); ///< The media registration callback.
  void (*Free)(r_media_t *self); ///< The media free callback.
} r_model_format_t;

/**
 * @brief The models type.
 */
typedef struct {

  r_model_t *world; ///< The currently loaded world model, if any.
  struct {
    GLuint vertex_array; ///< The vertex array (VAO) name.
    GLuint vertex_buffer; ///< The vertex buffer (VBO) name.
    GLuint elements_buffer; ///< The elements buffer (VBO) name.
    struct {
      GLuint vertex_array; ///< The depth pass vertex array (VAO) name.
    } depth_pass; ///< The depth pass vertex array.
  } mesh; ///< The shared vertex array for mesh models.

} r_models_t;

/**
 * @brief The models instance.
 */
extern r_models_t r_models;

/**
 * @brief Sprite rendering flags.
 */
enum {
  SPRITE_BEAM_REPEAT = 1 << 0, ///< If set, the sprite will tile its bounds rather than stretch to them.
  SPRITE_AXIAL       = 1 << 1, ///< If set, the sprite renders as two perpendicular axial quads in world space.
  SPRITE_CGAME       = 1 << 16 ///< Beginning of flags reserved for cgame.
};

typedef uint32_t r_sprite_flags_t;

/**
 * @brief Sprite billboard axis constraints.
 */
typedef enum {
  SPRITE_AXIS_ALL = 0, ///< Billboard sprite faces the camera on all axes.
  SPRITE_AXIS_X   = 1, ///< Billboard sprite constrained to the X axis.
  SPRITE_AXIS_Y   = 2, ///< Billboard sprite constrained to the Y axis.
  SPRITE_AXIS_Z   = 4  ///< Billboard sprite constrained to the Z axis.
} r_sprite_billboard_axis_t;

/**
 * @brief Sprites are billboarded alpha blended quads, optionally animated.
 */
typedef struct {
  vec3_t origin; ///< The sprite origin.
  float size; ///< The sprite size; if set, this is used for both width & height, otherwise width/height are used.
  float width; ///< The sprite width.
  float height; ///< The sprite width.
  r_media_t *media; ///< The sprite media (an r_animation_t, r_image_t, etc).
  float rotation; ///< The sprite's rotation, for non-beam sprites.
  vec3_t color; ///< The sprite color.
  float life; ///< The sprite's life from 0 to 1.
  vec3_t dir; ///< Direction of the sprite. { 0, 0, 0 } is billboard.
  r_sprite_billboard_axis_t axis; ///< Axis modifier for billboard sprites.
  r_sprite_flags_t flags; ///< Sprite flags
  float lighting; ///< Sprite lighting mix factor. 0 is fullbright, 1 is fully affected by light.
} r_sprite_t;

#define MAX_SPRITES    0x8000

/**
 * @brief Beams are segmented sprites.
 */
typedef struct {
  vec3_t start; ///< The beam start.
  vec3_t end; ///< The beam end.
  float size; ///< The beam size.
  r_image_t *image; ///< The beam texture.
  vec3_t color; ///< The beam color.
  float translate; ///< The beam texture translation.
  float stretch; ///< The beam texture stretch.
  r_sprite_flags_t flags; ///< The beam flags.
  float lighting; ///< Beam lighting mix factor. 0 is fullbright, 1 is fully affected by light.
} r_beam_t;

#define MAX_BEAMS 0x200

/**
 * @brief The sprite instance vertex structure.
 */
typedef struct {
  vec3_t position; ///< The vertex position.
  vec2_t diffusemap; ///< The diffusemap texture coordinate.
  vec2_t next_diffusemap; ///< The next diffusemap texture coordinate, for animation.
  color24_t color; ///< The vertex color.
  uint8_t lerp; ///< The animation interpolation factor.
  uint8_t lighting; ///< The lighting intensity.
} r_sprite_vertex_t;

/**
 * @brief An instance of a renderable sprite.
 */
typedef struct {
  r_sprite_flags_t flags; ///< The sprite flags.
  const r_image_t *diffusemap; ///< The diffusemap texture.
  const r_image_t *next_diffusemap; ///< The next diffusemap texture, for animation interpolation.
  r_sprite_vertex_t *vertexes; ///< The sprite vertexes in the shared array.
  GLvoid *elements; ///< An offset pointer (in bytes) in the shared array.
  box3_t bounds; ///< The sprite bounds.
} r_sprite_instance_t;

#define MAX_SPRITE_INSTANCES (MAX_SPRITES + MAX_BEAMS)

/**
 * @brief Renderer-specific entity effect bits. The lower 16 bits are reserved for the game and
 * client game module, and are sent over the wire as part of entity state. The higher bits are applied
 * locally by the client, client game or renderer.
 */
#define EF_SELF      (1 << 17) // local client's entity model
#define EF_WEAPON    (1 << 18) // view weapon
#define EF_SHELL     (1 << 19) // colored shell
#define EF_BLEND     (1 << 20) // preset alpha blend
#define EF_NO_SHADOW (1 << 21) // no shadow
#define EF_NO_DRAW   (1 << 22) // no draw (but perhaps shadow)

/**
 * @brief Entities provide a means to add model instances to the view. Entity
 * lighting is cached on the client entity so that it is only recalculated
 * when an entity moves.
 */
typedef struct r_entity_s {
  const void *id; ///< The entity identifier.
  const struct r_entity_s *parent; ///< The parent entity, if any, for linked mesh models.
  const char *tag; ///< The tag name, if any, for linked mesh models.
  vec3_t origin; ///< The entity origin.
  vec3_t termination; ///< The entity termination for beams.
  vec3_t angles; ///< The entity angles.
  float scale; ///< The entity scale, for mesh models.
  box3_t bounds; ///< The relative entity bounds, as known by the client.
  box3_t abs_bounds; ///< The absolute entity bounds, as known by the client.
  box3_t abs_model_bounds; ///< The visual model bounds, in world space, for frustum culling.
  mat4_t matrix; ///< The model matrix.
  mat4_t inverse_matrix; ///< The inverse model matrix.
  bool occluded; ///< True if this entity is occluded, false otherwise.
  const r_model_t *model; ///< The model, if any.
  int32_t frame, old_frame; ///< Frame animations.
  float lerp, back_lerp; ///< Frame interpolation.
  r_material_t *skins[MAX_MESH_FACES]; ///< Mesh model skins, up to one per face. NULL implies the default skin.
  int32_t num_skins; ///< The number of mesh model skins.
  int32_t effects; ///< The entity effects (`EF_NO_DRAW`, `EF_WEAPON`, ..).
  vec4_t color; ///< The entity shade color.
  vec3_t shell; ///< The entity shell color for flag carriers, etc.
  vec4_t tints[TINT_TOTAL]; ///< Tint maps allow users to customize their player skins.

} r_entity_t;

/**
 * @brief Light sources per scene.
 */
#define MAX_DYNAMIC_LIGHTS 64
#define MAX_LIGHTS (MAX_BSP_LIGHTS + MAX_DYNAMIC_LIGHTS)

/**
 * @brief Hardware light sources.
 */
typedef struct {
  int32_t flags; ///< The light flags.
  vec3_t origin; ///< The light origin.
  vec3_t color; ///< The light color.
  float radius; ///< The light radius.
  float intensity; ///< The light intensity.
  box3_t bounds; ///< The light bounds, or the volume visible to the light.
  r_occlusion_query_t *query; ///< The occlusion query, for lights that persist multiple frames.
  bool occluded; ///< True if the light is occluded for the current frame.
  const r_bsp_light_t *bsp_light; ///< The backing BSP light, for static light sources.
  bool *shadow_cached; ///< Pointer to the shadow cache flag for this light.
  const r_entity_t *source; ///< The optional light source, which will not cast shadow.
} r_light_t;

/**
 * @brief Framebuffer attachments bitmask.
 */
typedef enum {
  ATTACHMENT_COLOR      = 0x1,
  ATTACHMENT_DEPTH      = 0x2,
  ATTACHMENT_DEPTH_COPY = 0x4,
  ATTACHMENT_POST       = 0x8,
  ATTACHMENT_ALL        = 0xFF
} r_attachment_t;

/**
 * @brief The framebuffer type.
 */
typedef struct r_framebuffer_s {
  GLuint name; ///< The framebuffer name.
  int32_t attachments; ///< The attachments enabled for this framebuffer.
  GLuint color_attachment; ///< The color attachment texture name.
  GLuint depth_attachment; ///< The depth attachment texture name.
  GLuint depth_attachment_copy; ///< The depth attachment copy texture name.
  GLuint post_attachment; ///< The post-processing composite attachment texture name.
  GLint width; ///< The framebuffer width.
  GLint height; ///< The framebuffer height.

} r_framebuffer_t;

/**
 * @brief View types.
 */
typedef enum {
  VIEW_UNKNOWN,
  VIEW_MAIN,
  VIEW_PLAYER_MODEL,
} r_view_type_t;

/**
 * @brief View flags.
 */
typedef enum {
  VIEW_FLAG_NONE = 0x0,
  VIEW_FLAG_NO_DELTA = 0x1
} r_view_flags_t;

/**
 * @brief Each client frame populates a view, and submits it to the renderer.
 */
typedef struct {
  r_view_type_t type; ///< The view type.
  r_view_flags_t flags; ///< The view flags.
  r_framebuffer_t *framebuffer; ///< The target framebuffer (required).
  vec4i_t viewport; ///< The viewport, in device pixels.
  vec2_t fov; ///< The horizontal and vertical field of view.
  vec2_t depth_range; ///< The depth range; near and far clipping plane distances.
  vec3_t origin; ///< The view origin.
  vec3_t angles; ///< The view angles.
  vec3_t forward; ///< The forward vector, derived from angles.
  vec3_t right; ///< The right vector, derived from angles.
  vec3_t up; ///< The up vector, derived from angles.
  int32_t contents; ///< The contents mask at the view origin.
  uint32_t ticks; ///< The unclamped simulation time, in millis.
  float ambient; ///< The ambient scalar.
  r_entity_t entities[MAX_ENTITIES]; ///< The entities to render for the current frame.
  int32_t num_entities; ///< The count of entities.
  r_sprite_t sprites[MAX_SPRITES]; ///< The sprites to render for the current frame.
  int32_t num_sprites; ///< The count of sprites.
  r_beam_t beams[MAX_BEAMS]; ///< The beams to render for the current frame.
  int32_t num_beams; ///< The count of beams.
  r_sprite_instance_t sprite_instances[MAX_SPRITE_INSTANCES]; ///< The sprite instances (sprites and beams) for the current frame.
  int32_t num_sprite_instances; ///< The count of sprite instances.
  r_light_t lights[MAX_LIGHTS]; ///< The lights to render for the current frame.
  int32_t num_lights; ///< The count of lights.
  r_decal_t decals[MAX_DECALS]; ///< New decals added this frame, to be processed during R_UpdateDecals.
  int32_t num_decals; ///< The count of decals.
  cm_bsp_plane_t frustum[4]; ///< The view frustum, for box and sphere culling.
} r_view_t;

/**
 * @brief Window and OpenGL context information.
 */
typedef struct {

  SDL_DisplayID display; ///< The display associated with the application window.
  const SDL_DisplayMode *display_mode; ///< The display mode.
  SDL_Rect display_usable_bounds; ///< The display usable bounds, which may be smaller than the display mode resolution.
  SDL_Window *window; ///< The application window.
  SDL_WindowFlags window_flags; ///< The window flags.
  SDL_Rect window_bounds; ///< The window position and size in logical pixels.
  GLint w, h; ///< The window size, in logical pixels.
  SDL_Rect viewport; ///< The OpenGL viewport suitable for the current window.
  SDL_GLContext context; ///< The OpenGL context.
} r_context_t;

/**
 * @brief Renderer statistics.
 */
typedef struct {
  int32_t queries_allocated; ///< The count of allocated occlusion queries.
  int32_t queries_visible; ///< The count of visible occlusion queries.
  int32_t queries_occluded; ///< The count of occluded occlusion queries.
  int32_t blocks_visible; ///< The count of visible BSP blocks.
  int32_t blocks_occluded; ///< The count of occluded BSP blocks.
  int32_t lights_visible; ///< The count of visible lights.
  int32_t lights_occluded; ///< The count of occluded lights.
  int32_t entities_visible; ///< The count of visible entities.
  int32_t entities_occluded; ///< The count of occluded entities.
  int32_t bsp_inline_models; ///< The count of rendered inline BSP models.
  int32_t bsp_draw_elements; ///< The count of rendered BSP draw element batches.
  int32_t bsp_triangles; ///< The count of rendered BSP triangles.
  int32_t mesh_models; ///< The count of rendered mesh models.
  int32_t mesh_draw_elements; ///< The count of rendered mesh draw element batches.
  int32_t mesh_triangles; ///< The count of rendered mesh triangles.
  int32_t sprite_draw_elements; ///< The count of rendered sprite draw element batches.
  int32_t decals; ///< The count of rendered decals.
  int32_t decal_draw_elements; ///< The count of rendered decal draw element batches.
  int32_t draw_chars; ///< The count of rendered characters.
  int32_t draw_fills; ///< The count of rendered fill rectangles.
  int32_t draw_images; ///< The count of rendered images.
  int32_t draw_lines; ///< The count of rendered lines.
  int32_t draw_arrays; ///< The count of rendered arrays.
} r_stats_t;

#if defined(__R_LOCAL_H__)

/**
 * @brief OpenGL texture unit reservations.
 */
typedef enum {
  TEXTURE_DIFFUSEMAP         = 0, ///< The base diffuse map texture unit.

  TEXTURE_MATERIAL           = TEXTURE_DIFFUSEMAP, ///< First material-specific texture unit (alias for TEXTURE_DIFFUSEMAP).
  TEXTURE_STAGE,                                   ///< Stage map texture unit.
  TEXTURE_WARP,                                    ///< Warp map texture unit.

  TEXTURE_VOXEL,             ///< Voxel grid texture unit, used by BSP, mesh, sprite, and sky programs.
  TEXTURE_VOXEL_DATA,        ///< Voxel data texture unit.
  TEXTURE_VOXEL_LIGHT_DATA,  ///< Voxel light data texture unit.
  TEXTURE_VOXEL_LIGHT_INDICES, ///< Voxel light index texture unit.

  TEXTURE_SKY,               ///< Sky cubemap texture unit.

  TEXTURE_SHADOW_ATLAS,      ///< Shadow atlas texture unit.

  TEXTURE_NEXT_DIFFUSEMAP,   ///< Next diffuse map texture unit (sprite animation).

  TEXTURE_COLOR_ATTACHMENT,  ///< Framebuffer color attachment texture unit.
  TEXTURE_BLOOM_ATTACHMENT,  ///< Framebuffer bloom attachment texture unit.
  TEXTURE_DEPTH_ATTACHMENT,  ///< Framebuffer depth attachment texture unit.
  TEXTURE_DEPTH_ATTACHMENT_COPY, ///< Framebuffer depth attachment copy texture unit.
} r_texture_t;

#endif
