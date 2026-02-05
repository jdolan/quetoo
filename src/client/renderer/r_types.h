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

#include "r_gl_types.h"

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
  /**
   * @brief The media name.
   */
  char name[MAX_QPATH];

  /**
   * @brief The media type.
   */
  r_media_type_t type;

  /**
   * @brief The media on which this media depends.
   */
  GList *dependencies;

  /**
   * @brief The media registration callback.
   */
  void (*Register)(struct r_media_s *self);

  /**
   * @brief The media retain callback, to avoid being freed.
   */
  bool (*Retain)(struct r_media_s *self);

  /**
   * @brief The free callback, to release any system resources.
   */
  void (*Free)(struct r_media_s *self);

  /**
   * @brief The media seed, to determine if this media is current.
   */
  int32_t seed;
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
 * @brieef Image types.
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
  /**
   * @brief The media.
   */
  r_media_t media;

  /**
   * @brief The image type.
   */
  r_image_type_t type;

  /**
   * @brief The image width, height and depth (or layers).
   */
  GLint width, height, depth;

  /**
   * @brief The target to bind this texture.
   */
  GLenum target;

  /**
   * @brief The number of mipmap levels to allocate, typically `log2(Maxi(w, h)) + 1`.
   */
  GLsizei levels;

  /**
   * @brief The minification and magnifaction filters, typically `GL_LINEAR`.
   */
  GLenum minify, magnify;

  /**
   * @brief The internal pixel format, typically `GL_RGB` or `GL_RGBA`, but may be a sized value.
   */
  GLenum internal_format;

  /**
   * @brief The pixel format, typically `GL_RGB` or `GL_RGBA`.
   */
  GLenum format;

  /**
   * @brief The pixel data type, typically `GL_UNSIGNED_BYTE`.
   */
  GLenum pixel_type;

  /**
   * @brief The buffer object name, for `GL_TEXTURE_BUFFER` (TBO).
   */
  GLuint buffer;

  /**
   * @brief The texture name.
   */
  GLuint texnum;
} r_image_t;

/**
 * @brief An image atlas.
 */
typedef struct {
  /**
   * @brief The media.
   */
  r_media_t media;

  /**
   * @brief The atlas.
   */
  atlas_t *atlas;

  /**
   * @brief The compiled image atlas containing all nodes.
   */
  r_image_t *image;

  /**
   * @brief True if this at atlas should be recompiled.
   */
  bool dirty;
} r_atlas_t;

/**
 * @brief An atlas image, castable to r_image_t and r_media_t.
 */
typedef struct {
  /**
   * @brief The image.
   */
  r_image_t image;

  /**
   * @brief The atlas node that created this atlas image.
   */
  atlas_node_t *node;

  /**
   * @brief The texture coordinates of this atlas image within the atlas.
   */
  vec4_t texcoords;
} r_atlas_image_t;

/**
 * @brief An animation, castable to r_media_t.
 */
typedef struct {
  /**
   * @brief The media.
   */
  r_media_t media;

  /**
   * @brief The number of frames in this animation.
   */
  int32_t num_frames;

  /**
   * @brief The frames in this animation.
   */
  const r_image_t **frames;
} r_animation_t;

/**
 * @brief Material stages.
 */
typedef struct r_stage_s {
  /**
   * @brief The backing collision material stage.
   */
  const cm_stage_t *cm;

  /**
   * @brief Stages with a render pass will reference an image, atlas image, material, animation, etc.
   */
  r_media_t *media;

  /**
   * @brief The next stage in the material.
   */
  struct r_stage_s *next;
} r_stage_t;

/**
 * @brief Materials define texture, animation and lighting properties for BSP and mesh models.
 */
typedef struct r_material_s {
  /**
   * @brief Materials are media.
   */
  r_media_t media;

  /**
   * @brief The collision material definition.
   */
  cm_material_t *cm;

  /**
   * @brief The layered texture containing the diffusemap, normalmap and specularmap.
   */
  r_image_t *texture;

  /**
   * @brief Animated stage definitions.
   */
  r_stage_t *stages;

  /**
   * @brief The time when this material was last animated.
   */
  uint32_t ticks;

  /**
   * @brief The diffusemap color.
   */
  color_t color;
} r_material_t;

/**
 * @brief OpenGL occlusion queries.
 */
typedef struct r_occlusion_query_s {
  /**
   * @brief The query name.
   */
  GLuint name;

  /**
   * @brief The query bounds.
   */
  box3_t bounds;

  /**
   * @brief The base vertex in the shared vertex buffer.
   */
  GLint base_vertex;

  /**
   * @brief Non-zero if the query is available.
   */
  GLint available;

  /**
   * @brief Non-zero of the query produced visible fragments.
   */
  GLint result;
} r_occlusion_query_t;

/**
 * @brief BSP plane structure.
 */
typedef struct {
  /**
   * @brief The collision plane.
   */
  const cm_bsp_plane_t *cm;
} r_bsp_plane_t;

/**
 * @brief BSP brush side structure.
 */
typedef struct {
  /**
   * @brief The plane.
   */
  const r_bsp_plane_t *plane;

  /**
   * @brief The material.
   */
  const r_material_t *material;

  /**
   * @brief The texture axis for S and T, in xyz + offset notation.
   */
  vec4_t axis[2];

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
} r_bsp_brush_side_t;

/**
 * @brief BSP vertex structure.
 */
typedef struct {
  /**
   * @brief The position.
   */
  vec3_t position;

  /**
   * @brief The normal, for Phong shading.
   */
  vec3_t normal;

  /**
   * @brief The tangent, for per-pixel lighting.
   */
  vec3_t tangent;

  /**
   * @brief The bitangent, for per-pixel lighting.
   *
   */
  vec3_t bitangent;

  /**
   * @brief The diffusemap texture coordinate.
   */
  vec2_t diffusemap;

  /**
   * @brief The color, for alpha blending and vertex lighting effects.
   */
  color32_t color;
} r_bsp_vertex_t;

/**
 * @brief BSP faces, which may reside on the front or back of their node.
 */
typedef struct {
  /**
   * @brief The node containing this face.
   */
  struct r_bsp_node_s *node;

  /**
   * @brief The brush side which generated this face.
   */
  r_bsp_brush_side_t *brush_side;

  /**
   * @brief The plane on which this face resides (to disambiguiate `node`).
   */
  r_bsp_plane_t *plane;

  /**
   * @brief The AABB of this face.
   */
  box3_t bounds;

  /**
   * @brief The vertexes.
   * @details This is simply a pointer into the BSP's vertex array.
   */
  r_bsp_vertex_t *vertexes;

  /**
   * @brief The count of vertexes.
   */
  int32_t num_vertexes;

  /**
   * @brief The elements.
   * @details This is simply a pointer into the BSP's elements array.
   */
  GLvoid *elements;

  /**
   * @brief The count of elements.
   */
  int32_t num_elements;
} r_bsp_face_t;

/**
 * @brief BSP draw elements, which include all opaque faces of a given material
 * within a particular inline model.
 */
typedef struct {
  /**
   * @brief The plane, for blended draw elements.
   * @details Alpha blended draw elements are sorted by plane so that they may be depth sorted.
   */
  r_bsp_plane_t *plane;

  /**
   * @brief The material.
   */
  r_material_t *material;

  /**
   * @brief The surface flags.
   */
  int32_t surface;

  /**
   * @brief The AABB of the elements.
   */
  box3_t bounds;

  /**
   * @brief An offset pointer (in bytes) into the BSP elements array.
   */
  GLvoid *elements;

  /**
   * @brief The count of elements.
   */
  int32_t num_elements;

  /**
   * @brief The texture coordinate center.
   * @details This is only used for draw elements that require scaling or rotating material
   * stages (`STAGE_SCALE`, `STAGE_STRETCH`, `STAGE_ROTATE`).
   */
  vec2_t st_origin;
} r_bsp_draw_elements_t;

/**
 * @brief BSP nodes comprise the tree representation of the world.
 */
typedef struct r_bsp_node_s {
  /**
   * @brief The contents mask; one of `CONTENTS_NODE` or `CONTENTS_BLOCK` for nodes.
   */
  int32_t contents;

  /**
   * @brief The AABB.
   */
  box3_t bounds;

  /**
   * @brief The parent node.
   */
  struct r_bsp_node_s *parent;

  /**
   * @brief The inline model. Each inline model contains its own sub-tree.
   */
  struct r_bsp_inline_model_s *model;

  /**
   * @brief The plane that created this node.
   */
  r_bsp_plane_t *plane;

  /**
   * @brief The child nodes, which may be leaves.
   */
  struct r_bsp_node_s *children[2];

  /**
   * @brief The AABB of visible faces within this node.
   * @remarks Often smaller than bounds, and useful for frustum culling.
   */
  box3_t visible_bounds;

  /**
   * @brief The faces within this node.
   * @details Faces will reside only in a single node, but they may straddle leaves.
   */
  r_bsp_face_t *faces;

  /**
   * @brief The count of faces.
   */
  int32_t num_faces;
} r_bsp_node_t;

/**
 * @brief BSP leafs terminate the branches of the BSP tree.
 * @remarks Leafs are truncated node structures so that they may be cast to `r_bsp_node_t`.
 */
typedef struct {
  /**
   * @brief The contents mask. Valid for leafs, always `CONTENTS_NODE` for nodes.
   */
  int32_t contents;

  /**
   * @brief The AABB.
   */
  box3_t bounds;

  /**
   * @brief The parent node.
   */
  struct r_bsp_node_s *parent;

  /**
   * @brief The inline model. Each inline model contains its own sub-tree.
   */
  struct r_bsp_inline_model_s *model;
} r_bsp_leaf_t;

/**
 * @brief BSP blocks are large, axial-aligned, gridded nodes used to aggregate rendering operations.
 */
typedef struct {
  /**
   * @brief The `CONTENTS_BLOCK` node defining this block.
   */
  r_bsp_node_t *node;

  /**
   * @brief The draw elements within this block.
   */
  r_bsp_draw_elements_t *draw_elements;

  /**
   * @brief The count of draw elements.
   */
  int32_t num_draw_elements;

  /**
   * @brief The visible bounds of this block, used for occlusion query and culling.
   * @remarks This is different from the node's bounds, and the node's visible bounds.
   */
  box3_t visible_bounds;

  /**
   * @brief The occlusion query for this block.
   */
  r_occlusion_query_t *query;
} r_bsp_block_t;

/**
 * @brief Decals are projected textures that conform to BSP geometry.
 */
typedef struct r_decal_s {
  /**
   * @brief The decal origin.
   */
  vec3_t origin;

  /**
   * @brief The decal surface normal.
   */
  vec3_t normal;

  /**
   * @brief The decal radius.
   */
  float radius;

  /**
   * @brief The decal color.
   */
  color_t color;

  /**
   * @brief The decal media (image or atlas image).
   */
  r_media_t *media;

  /**
   * @brief The decal creation time in ticks.
   */
  uint32_t time;

  /**
   * @brief The decal lifetime in ticks (0 = permanent).
   */
  uint32_t lifetime;

  /**
   * @brief The `r_decal_face_t`s for this decal (renderer-private).
   */
  void *faces;

  /**
   * @brief The count of `r_decal_face_t`, for batched rendering (renderer-private).
   */
  int32_t num_faces;

  /**
   * @brief The previous decal on the model (set by renderer).
   */
  struct r_decal_s *prev;

  /**
   * @brief The next decal on the model (set by renderer).
   */
  struct r_decal_s *next;
} r_decal_t;

#define MAX_DECALS 0x400

/**
 * @brief The BSP is organized into one or more models (trees). The first model is
 * the worldspawn model, and typically is the largest. An additional model exists
 * for each entity that contains brushes. Non-worldspawn models can move and rotate.
 */
typedef struct r_bsp_inline_model_s {

  /**
   * @brief The backing entity definition for this inline model.
   */
  cm_entity_t *entity;

  /**
   * @brief The head node of this inline model.
   * @brief This is a pointer into the BSP model's nodes array.
   */
  r_bsp_node_t *head_node;

  /**
   * @brief For frustum culling.
   */
  box3_t visible_bounds;

  /**
   * @brief The faces of this inline model.
   * @details This is a pointer into the BSP model's faces array.
   */
  r_bsp_face_t *faces;
  int32_t num_faces;

  /**
   * @brief The depth pass elements of this inline model.
   * @brief This is an offset pointer, in bytes, into the BSP model's elements array.
   */
  GLvoid *depth_pass_elements;
  int32_t num_depth_pass_elements;

  /**
   * @brief The draw elements of this inline model.
   * @details This is a pointer into the BSP model's draw elements array.
   */
  r_bsp_draw_elements_t *draw_elements;
  int32_t num_draw_elements;

  /**
   * @brief The blocks of this inline model.
   * @details This is a pointer into the BSP model's blocks array.
   */
  r_bsp_block_t *blocks;
  int32_t num_blocks;

  /**
   * @brief Decals attached to this inline model (linked list).
   */
  r_decal_t *decals;

  /**
   * @brief An offset pointer (in bytes) into the decal elements array for decal geometry.
   */
  GLvoid *decal_elements;
} r_bsp_inline_model_t;

/**
 * @brief
 */
typedef struct {
  /**
   * @brief The entity that defines this light.
   */
  cm_entity_t *entity;

  /**
   * @brief The light origin.
   */
  vec3_t origin;

  /**
   * @brief The light color.
   */
  vec3_t color;

  /**
   * @brief The light normal and plane distance for directional lights.
   */
  vec4_t normal;

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
  box3_t bounds;

  /**
   * @brief The light occlusion query.
   */
  r_occlusion_query_t *query;

  /**
   * @brief An offset pointer (in bytes) into the BSP elements array for shadow geometry.
   */
  GLvoid *depth_pass_elements;

  /**
   * @brief The count of elements.
   */
  int32_t num_depth_pass_elements;

  /**
   * @brief The style string, a-z (26 levels), animated at 10Hz.
   */
  char style[MAX_BSP_ENTITY_VALUE];

  /**
   * @brief True if this light's shadowmap can be reused from the previous frame.
   */
  bool shadow_cached;
} r_bsp_light_t;

/**
 * @brief Individual voxel data for CPU-side access.
 */
typedef struct {
  /**
   * @brief The voxel's world-space bounds.
   */
  box3_t bounds;

  /**
   * @brief The voxel's combined contents mask.
   */
  int32_t contents;

  /**
   * @brief The lights affecting this voxel.
   */
  const r_bsp_light_t **lights;

  /**
   * @brief The number of lights affecting this voxel.
   */
  int32_t num_lights;
} r_bsp_voxel_t;

/**
 * @brief The BSP voxel grid, including light index data for clustered forward lighting.
 */
typedef struct {
  /**
   * @brief The grid dimensions in voxels.
   */
  vec3i_t size;

  /**
   * The total number of voxels.
   */
  int32_t num_voxels;

  /**
   * @brief The voxel bounds in world space.
   */
  box3_t bounds;

  /**
   * @brief Array of individual voxel data (for CPU-side access and debugging).
   */
  r_bsp_voxel_t *voxels;

  /**
   * @brief The voxel data 3D texture (RG8): caustics and exposure.
   */
  r_image_t *data;

  /**
   * @brief The fog 3D texture (RGBA8).
   */
  r_image_t *fog;

  /**
   * @brief The light data 3D texture (RG32I) for offset and count pairs per voxel.
   */
  r_image_t *light_data;

  /*/
   * @brief Voxel light index texture to sample the index buffer (R32I).
   */
  r_image_t *light_indices;

  /**
   * @brief The buffer object backing the index vector.
   */
  GLuint light_indices_buffer;

  /**
   * @brief The length of `light_indices_buffer`.
   */
  int32_t num_light_indices;

} r_bsp_voxels_t;

/**
 * @brief The renderer representation of the BSP model.
 */
typedef struct {

  const cm_bsp_t *cm;

  int32_t num_planes;
  r_bsp_plane_t *planes;

  int32_t num_materials;
  r_material_t **materials;

  int32_t num_brush_sides;
  r_bsp_brush_side_t *brush_sides;

  int32_t num_vertexes;
  r_bsp_vertex_t *vertexes;

  int32_t num_elements;
  GLuint *elements;

  int32_t num_faces;
  r_bsp_face_t *faces;

  int32_t num_draw_elements;
  r_bsp_draw_elements_t *draw_elements;

  int32_t num_nodes;
  r_bsp_node_t *nodes;

  int32_t num_leafs;
  r_bsp_leaf_t *leafs;

  r_bsp_block_t *blocks;
  int32_t num_blocks;

  int32_t num_inline_models;
  r_bsp_inline_model_t *inline_models;

  int32_t num_lights;
  r_bsp_light_t *lights;

  r_bsp_voxels_t *voxels;

  /**
   * @brief The vertex array (VAO) name.
   */
  GLuint vertex_array;

  /**
   * @brief The vertex array buffer (VBO) name.
   */
  GLuint vertex_buffer;

  /**
   * @brief The elements array buffer (VBO) name.
   */
  GLuint elements_buffer;

  /**
   * @brief The depth pass vertex array.
   */
  struct {
    /**
     * @brief The depth pass vertex array (VAO) name.
     */
    GLuint vertex_array;
  } depth_pass;

  /**
   * @brief The first inline BSP model, aka worldspawn.
   */
  struct r_model_s *worldspawn;

} r_bsp_model_t;

/**
 * @brief The mesh vertex type.
 */
typedef struct {
  vec3_t position;
  vec3_t normal;
  vec3_t smooth_normal;
  vec3_t tangent;
  vec3_t bitangent;
  vec2_t diffusemap;
} r_mesh_vertex_t;

/**
 * @brief The mesh frame type.
 */
typedef struct {
  box3_t bounds;
  vec3_t translate;
} r_mesh_frame_t;

/**
 * @brief The mesh tag type.
 * @details Tags are used to align submodels (e.g. weapons, CTF flags).
 */
typedef struct {
  char name[MAX_QPATH];
  mat4_t matrix;
} r_mesh_tag_t;

/**
 * @brief The mesh face type.
 * @details A mesh model is comprised of one or more faces, which may have distinct materials.
 */
typedef struct {
  /**
   * @brief The face name. This is used to resolve the material.
   */
  char name[MAX_QPATH];

  /**
   * @brief The material.
   */
  r_material_t *material;

  /**
   * @brief The vertexes.
   * @details This is a pointer into the model's vertex array.
   */
  r_mesh_vertex_t *vertexes;

  /**
   * @brief The count of vertexes.
   */
  int32_t num_vertexes;

  /**
   * @brief The elements.
   * @details This is a poitner into the model's elements array.
   */
  GLuint *elements;

  /**
   * @brief The count of elements.
   */
  int32_t num_elements;

  /**
   * @brief The base vertex in the shared mesh VAO.
   */
  GLint base_vertex;

  /**
   * @brief The elements pointer in the shared mesh VAO.
   */
  GLvoid *indices;
} r_mesh_face_t;

/**
 * @brief Max mesh model faces.
 */
#define MAX_MESH_FACES 0x20

/**
 * @brief The mesh animation type.
 */
typedef struct {
  int32_t first_frame;
  int32_t num_frames;
  int32_t looped_frames;
  int32_t hz;
} r_mesh_animation_t;

/**
 * @brief Provides load-time normalization of mesh models.
 */
typedef struct {
  mat4_t transform;
} r_mesh_config_t;

/**
 * @brief The mesh model type.
 */
typedef struct {
  uint32_t flags;

  r_mesh_vertex_t *vertexes;
  int32_t num_vertexes;

  GLuint *elements;
  int32_t num_elements;

  r_mesh_frame_t *frames;
  int32_t num_frames;

  r_mesh_tag_t *tags;
  int32_t num_tags;

  r_mesh_face_t *faces;
  int32_t num_faces;

  r_mesh_animation_t *animations;
  int32_t num_animations;

  /**
   * @brief The base vertex in the shared mesh VAO.
   */
  GLint base_vertex;

  /**
   * @brief The indices pointer in the shared mesh VAO.
   */
  GLvoid *indices;

  struct {
    r_mesh_config_t world;
    r_mesh_config_t view;
    r_mesh_config_t link;
  } config;
} r_mesh_model_t;

/**
 * @brief Models represent a subset of the BSP or a mesh.
 */
typedef struct r_model_s {
  r_media_t media;
  r_model_type_t type;

  union {
    r_bsp_model_t *bsp;
    r_bsp_inline_model_t *bsp_inline;
    r_mesh_model_t *mesh;
  };

  box3_t bounds;
  float radius;
} r_model_t;

#define IS_BSP_MODEL(m) (m && m->type == MODEL_BSP)
#define IS_BSP_INLINE_MODEL(m) (m && m->type == MODEL_BSP_INLINE)
#define IS_MESH_MODEL(m) (m && m->type == MODEL_MESH)

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
  r_model_type_t type;

  /**
   * @brief The load function.
   */
  void (*Load)(r_model_t *mod, void *buffer);

  /**
   * @brief The media registration callback.
   */
  void (*Register)(r_media_t *self);

  /**
   * @brief The media free callback.
   */
  void (*Free)(r_media_t *self);
} r_model_format_t;

/**
 * @brief The models type.
 */
typedef struct {

  /**
   * @brief The currently loaded world model, if any.
   */
  r_model_t *world;

  /**
   * @brief The shared vertex array for mesh models.
   */
  struct {
    /**
     * @brief The vertex array (VAO) name.
     */
    GLuint vertex_array;

    /**
     * @brief THe vertex buffer (VBO) name.
     */
    GLuint vertex_buffer;

    /**
     * @brief The elements buffer (VBO) name.
     */
    GLuint elements_buffer;

    /**
     * @brief The depth pass vertex array.
     */
    struct {
      /**
       * @brief The depth pass vertex array (VAO) name.
       */
      GLuint vertex_array;
    } depth_pass;

  } mesh;

} r_models_t;

/**
 * @brief The models instance.
 */
extern r_models_t r_models;

/**
 * @brief
 */
enum {
  /**
   * @brief If set, the sprite will tile its bounds, rather than stretch to them.
   */
  SPRITE_BEAM_REPEAT      = 1 << 0,

  /**
   * @brief Beginning of flags reserved for cgame
   */
  SPRITE_CGAME      = 1 << 16
};

typedef uint32_t r_sprite_flags_t;

/**
 * @brief
 */
typedef enum {
  /**
   * @brief
   */
  SPRITE_AXIS_ALL = 0,

  /**
   * @brief
   */
  SPRITE_AXIS_X = 1,

  /**
   * @brief
   */
  SPRITE_AXIS_Y = 2,

  /**
   * @brief
   */
  SPRITE_AXIS_Z = 4
} r_sprite_billboard_axis_t;

/**
 * @brief Sprites are billboarded alpha blended quads, optionally animated.
 */
typedef struct {
  /**
   * @brief The sprite origin.
   */
  vec3_t origin;

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
   * @brief The sprite media (an r_amimation_t, r_image_t, etc).
   */
  r_media_t *media;

  /**
   * @brief The sprite's rotation, for non-beam sprites.
   */
  float rotation;

  /**
   * @brief The sprite color.
   */
  vec3_t color;

  /**
   * @brief The sprite's life from 0 to 1.
   */
  float life;

  /**
   * @brief Direction of the sprite. { 0, 0, 0 } is billboard.
   */
  vec3_t dir;

  /**
   * @brief Axis modifier for billboard sprites.
   */
  r_sprite_billboard_axis_t axis;

  /**
   * @brief Sprite flags
   */
  r_sprite_flags_t flags;

  /**
   * @brief Sprite softness scalar. Negative values apply an invert to the result.
   */
  float softness;

  /**
   * @brief Sprite lighting mix factor. 0 is fullbright, 1 is fully affected by light.
   */
  float lighting;
} r_sprite_t;

#define MAX_SPRITES    0x8000

/**
 * @brief Beams are segmented sprites.
 */
typedef struct {
  /**
   * @brief The beam start.
   */
  vec3_t start;

  /**
   * @brief The beam end.
   */
  vec3_t end;

  /**
   * @brief The beam size.
   */
  float size;

  /**
   * @brief The beam texture.
   */
  r_image_t *image;

  /**
   * @brief The beam color.
   */
  vec3_t color;

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
  r_sprite_flags_t flags;

  /**
   * @brief Beam softness scalar. Negative values apply an invert to the result.
   */
  float softness;

  /**
   * @brief Beam lighting mix factor. 0 is fullbright, 1 is fully affected by light.
   */
  float lighting;
} r_beam_t;

#define MAX_BEAMS 0x200

/**
 * @brief The sprite instance vertex structure.
 */
typedef struct {
  vec3_t position;
  vec2_t diffusemap;
  vec2_t next_diffusemap;
  color24_t color;
  uint8_t lerp;
  uint8_t softness;
  uint8_t lighting;
} r_sprite_vertex_t;

/**
 * @brief An instance of a renderable sprite.
 */
typedef struct {
  /**
   * @brief The sprite flags.
   */
  r_sprite_flags_t flags;

  /**
   * @brief The diffusemap texture.
   */
  const r_image_t *diffusemap;

  /**
   * @brief The next diffusemap texture, for animation interpolation.
   */
  const r_image_t *next_diffusemap;

  /**
   * @brief The sprite vertexes in the shared array.
   */
  r_sprite_vertex_t *vertexes;

  /**
   * @brief An offset pointer (in bytes) in the shared array.
   */
  GLvoid *elements;

  /**
   * @brief The sprite bounds.
   */
  box3_t bounds;
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
  /**
   * @brief The entity identifier.
   */
  const void *id;

  /**
   * @brief The parent entity, if any, for linked mesh models.
   */
  const struct r_entity_s *parent;

  /**
   * @brief The tag name, if any, for linked mesh models.
   */
  const char *tag;

  /**
   * @brief The entity origin.
   */
  vec3_t origin;

  /**
   * @brief The entity termination for beams.
   */
  vec3_t termination;

  /**
   * @brief The entity angles.
   */
  vec3_t angles;

  /**
   * @brief The entity scale, for mesh models.
   */
  float scale;

  /**
   * @brief The relative entity bounds, as known by the client.
   */
  box3_t bounds;

  /**
   * @brief The absolute entity bounds, as known by the client.
   */
  box3_t abs_bounds;

  /**
   * @brief The visual model bounds, in world space, for frustum culling.
   */
  box3_t abs_model_bounds;

  /**
   * @brief The model matrix.
   */
  mat4_t matrix;

  /**
   * @brief The inverse model matrix.
   */
  mat4_t inverse_matrix;

  /**
   * @brief True if this entity is occluded, false otherwise.
   */
  bool occluded;

  /**
   * @brief The model, if any.
   */
  const r_model_t *model;

  /**
   * @brief Frame animations.
   */
  int32_t frame, old_frame;

  /**
   * @brief Frame interpolation.
   */
  float lerp, back_lerp;

  /**
   * @brief Mesh model skins, up to one per face. NULL implies the default skin.
   */
  r_material_t *skins[MAX_MESH_FACES];

  /**
   * @brief The number of mesh model skins.
   */
  int32_t num_skins;

  /**
   * @brief The entity effects (`EF_NO_DRAW`, `EF_WEAPON`, ..).
   */
  int32_t effects;

  /**
   * @brief The entity shade color.
   */
  vec4_t color;

  /**
   * @brief The entity shell color for flag carriers, etc.
   */
  vec3_t shell;

  /**
   * @brief Tint maps allow users to customize their player skins.
   */
  vec4_t tints[TINT_TOTAL];

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
  /**
   * @brief The light flags.
   */
  int32_t flags;

  /**
   * @brief The light origin.
   */
  vec3_t origin;

  /**
   * @brief The light color.
   */
  vec3_t color;

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
  box3_t bounds;

  /**
   * @brief The occlusion query, for lights that persist multiple frames.
   */
  r_occlusion_query_t *query;

  /**
   * @brief True if the light is occluded for the current frame.
   */
  bool occluded;

  /**
   * @brief The backing BSP light, for static light sources.
   */
  const r_bsp_light_t *bsp_light;

  /**
   * @brief Pointer to the shadow cache flag for this light.
   */
  bool *shadow_cached;

  /**
   * @brief The optional light source, which will not cast shadow.
   */
  const r_entity_t *source;
} r_light_t;

/**
 * @brief Framebuffer attachments bitmask.
 */
typedef enum {
  ATTACHMENT_COLOR      = 0x1,
  ATTACHMENT_DEPTH      = 0x2,
  ATTACHMENT_DEPTH_COPY = 0x4,
  ATTACHMENT_ALL        = 0xFF
} r_attachment_t;

/**
 * @brief The framebuffer type.
 */
typedef struct r_framebuffer_s {
  /**
   * @brief The framebuffer name.
   */
  GLuint name;

  /**
   * The attachments enabled for this framebuffer.
   */
  int32_t attachments;

  /**
   * @brief The color attachment texture name.
   */
  GLuint color_attachment;

  /**
   * @brief The depth attachment texture name.
   */
  GLuint depth_attachment;

  /**
   * @brief The depth attachment copy texture name.
   */
  GLuint depth_attachment_copy;

  /**
   * @brief The framebuffer width.
   */
  GLint width;

  /**
   * @brief The framebuffer height.
   */
  GLint height;

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
  /**
   * @brief The view type.
   */
  r_view_type_t type;

  /**
   * @brief The view flags.
   */
  r_view_flags_t flags;

  /**
   * @brief The target framebuffer (required).
   */
  r_framebuffer_t *framebuffer;

  /**
   * @brief The viewport, in device pixels.
   */
  vec4i_t viewport;

  /**
   * @brief The horizontal and vertical field of view.
   */
  vec2_t fov;

  /**
   * @brief The depth range; near and far clipping plane distances.
   */
  vec2_t depth_range;

  /**
   * @brief The view origin.
   */
  vec3_t origin;

  /**
   * @brief The view angles.
   */
  vec3_t angles;

  /**
   * @brief The forward vector, derived from angles.
   */
  vec3_t forward;

  /**
   * @brief The right vector, derived from angles.
   */
  vec3_t right;

  /**
   * @brief The up vector, derived from angles.
   */
  vec3_t up;

  /**
   * @brief The contents mask at the view origin.
   */
  int32_t contents;

  /**
   * @brief The unclamped simulation time, in millis.
   */
  uint32_t ticks;

  /**
   * @brief The ambient scalar.
   */
  float ambient;

  /**
   * @brief The entities to render for the current frame.
   */
  r_entity_t entities[MAX_ENTITIES];
  int32_t num_entities;

  /**
   * @brief The sprites to render for the current frame.
   */
  r_sprite_t sprites[MAX_SPRITES];
  int32_t num_sprites;

  /**
   * @brief The beams to render for the current frame.
   */
  r_beam_t beams[MAX_BEAMS];
  int32_t num_beams;

  /**
   * @brief The sprite instances (sprites and beams) for the current frame.
   * @remarks This array is populated by the renderer from sprites and beams.
   */
  r_sprite_instance_t sprite_instances[MAX_SPRITE_INSTANCES];
  int32_t num_sprite_instances;

  /**
   * @brief The lights to render for the current frame.
   */
  r_light_t lights[MAX_LIGHTS];
  int32_t num_lights;

  /**
   * @brief New decals added this frame, to be processed during R_UpdateDecals.
   */
  r_decal_t decals[MAX_DECALS];
  int32_t num_decals;

  /**
   * @brief The view frustum, for box and sphere culling.
   * @remarks This is populated by the renderer.
   */
  cm_bsp_plane_t frustum[4];
} r_view_t;

/**
 * @brief Window and OpenGL context information.
 */
typedef struct {

  /**
   * @brief The application window.
   */
  SDL_Window *window;

  /**
   * @brief The OpenGL 3.3 context.
   */
  SDL_GLContext context;

  /**
   * @brief The window size, which may be smaller than the drawable size in pixels
   * on high pixel density (4K+) displays.
   */
  GLint w, h;

  /**
   * @brief OpenGL context size in drawable pixels, as reported by `SDL_GetWindowSizeInPixels`.
   */
  GLint pw, ph;

  /**
   * @brief True if fullscreen, false if windowed.
   */
  bool fullscreen;

  /**
   * @brief Greater than 1.0 if High DPI mode is enabled.
   */
  float window_scale;

  /**
   * @brief The display refresh rate in Hz.
   */
  float refresh_rate;
} r_context_t;

/**
 * @brief Renderer statistics.
 */
typedef struct {

  int32_t queries_allocated;
  int32_t queries_visible;
  int32_t queries_occluded;

  int32_t blocks_visible;
  int32_t blocks_occluded;

  int32_t lights_visible;
  int32_t lights_occluded;

  int32_t entities_visible;
  int32_t entities_occluded;

  int32_t bsp_inline_models;
  int32_t bsp_draw_elements;
  int32_t bsp_triangles;

  int32_t mesh_models;
  int32_t mesh_draw_elements;
  int32_t mesh_triangles;

  int32_t sprite_draw_elements;

  int32_t decals;
  int32_t decal_draw_elements;

  int32_t draw_chars;
  int32_t draw_fills;
  int32_t draw_images;
  int32_t draw_lines;
  int32_t draw_arrays;

} r_stats_t;

#ifdef __R_LOCAL_H__

/**
 * @brief OpenGL texture unit reservations.
 */
typedef enum {
  /**
   * @brief The base texture.
   */
  TEXTURE_DIFFUSEMAP = 0,

  /**
   * @brief Material specific textures.
   */
  TEXTURE_MATERIAL = TEXTURE_DIFFUSEMAP,
  TEXTURE_STAGE,
  TEXTURE_WARP,

  /**
   * @brief The voxel textures, used by the BSP, mesh, sprite and sky programs.
   */
  TEXTURE_VOXEL,
  TEXTURE_VOXEL_DATA,
  TEXTURE_VOXEL_FOG,
  TEXTURE_VOXEL_LIGHT_DATA,
  TEXTURE_VOXEL_LIGHT_INDICES,

  /**
   * @brief The sky cubemap texture.
   */
  TEXTURE_SKY,

  /**
   * @brief The shadowmap cubemap array texture.
   */
  TEXTURE_SHADOW_CUBEMAP_ARRAY,

  /**
   * @brief Sprite specific textures.
   */
  TEXTURE_NEXT_DIFFUSEMAP,

  /**
   * @brief Framebuffer specific textures.
   */
  TEXTURE_COLOR_ATTACHMENT,
  TEXTURE_DEPTH_ATTACHMENT,
  TEXTURE_DEPTH_ATTACHMENT_COPY,
} r_texture_t;

#endif
