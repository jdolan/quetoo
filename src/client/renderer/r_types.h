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

#include <SDL_video.h>

#include "collision/collision.h"

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
	_Bool (*Retain)(struct r_media_s *self);

	/**
	 * @brief The free callback, to release any system resources.
	 */
	void (*Free)(struct r_media_s *self);

	/**
	 * @brief The media seed, to determine if this media is current.
	 */
	int32_t seed;
} r_media_t;

typedef int16_t r_pixel_t;

typedef enum {
	MOD_INVALID,
	MOD_BSP,
	MOD_BSP_INLINE,
	MOD_MESH
} r_model_type_t;

// high bits OR'ed with image categories, flags are bits 24..31
#define IT_MASK_MIPMAP		(1 << 24)
#define IT_MASK_CLAMP_EDGE  (1 << 25)
#define IT_MASK_QUALITY		(1 << 26)
#define IT_MASK_FLAGS		(IT_MASK_MIPMAP | IT_MASK_CLAMP_EDGE)

// image categories (bits 0..23) + flags are making image types
typedef enum {
	IT_PROGRAM =     (1 <<  1),
	IT_FONT =        (1 <<  2),
	IT_UI =          (1 <<  3),
	IT_SPRITE =      (1 <<  4) + (IT_MASK_MIPMAP),
	IT_MATERIAL =    (1 <<  5) + (IT_MASK_MIPMAP),
	IT_CUBEMAP =     (1 <<  6) + (IT_MASK_CLAMP_EDGE),
	IT_PIC =         (1 <<  7) + (IT_MASK_MIPMAP),
	IT_ATLAS =       (1 <<  8) + (IT_MASK_MIPMAP | IT_MASK_CLAMP_EDGE),
	IT_LIGHTMAP =    (1 <<  9) + (IT_MASK_CLAMP_EDGE),
	IT_LIGHTGRID =   (1 << 10) + (IT_MASK_CLAMP_EDGE),
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
	r_pixel_t width, height, depth;

	/**
	 * @brief The target to bind this texture.
	 */
	GLenum target;

	/**
	 * @brief The pixel format, typically GL_RGB or GL_RGBA.
	 */
	GLenum format;

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
	_Bool dirty;
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
	 * @brief The layered texture containing the diffusemap, normalmap and glossmap.
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
} r_material_t;

/**
 * @brief BSP texture information.
 */
typedef struct {
	/**
	 * @brief The XYZ + W texture vectors in world space.
	 */
	vec4_t vecs[2];

	/**
	 * @brief The surface flags.
	 */
	int32_t flags;

	/**
	 * @brief The surface value, for lights or Phong grouping.
	 */
	int32_t value;

	/**
	 * @brief The diffusemap texture name.
	 */
	char texture[32];

	/**
	 * @brief The material.
	 */
	r_material_t *material;
} r_bsp_texinfo_t;

/**
 * @brief BSP plane structure.
 */
typedef struct {
	/**
	 * @brief The collision plane.
	 */
	const cm_bsp_plane_t *cm;

	/**
	 * @brief The alpha blended draw elements referencing this plane.
	 */
	GPtrArray *blend_elements;
} r_bsp_plane_t;

/**
 * @brief BSP brush side structure.
 */
typedef struct {
	/**
	 * @brief The texinfo.
	 */
	const r_bsp_texinfo_t *texinfo;

	/**
	 * @brief The plane.
	 */
	const r_bsp_plane_t *plane;

	/**
	 * @brief The contents mask.
	 */
	int32_t contents;
} r_bsp_brush_side_t;

/**
 * @brief BSP vertex structure.
 */
typedef struct {
	vec3_t position;
	vec3_t normal;
	vec3_t tangent;
	vec3_t bitangent;
	vec2_t diffusemap;
	vec2_t lightmap;
	color32_t color;
} r_bsp_vertex_t;

/**
 * @brief Indivudual face lightmap information.
 */
typedef struct {
	/**
	 * @brief The texture coordinates of this lightmap in the lightmap atlas.
	 */
	r_pixel_t s, t;

	/**
	 * @brief The width and height of this lightmap.
	 */
	r_pixel_t w, h;

	/**
	 * @brief The world-to-lightmap projection matrix.
	 */
	mat4_t matrix;

	/**
	 * @brief The texture coordinate bounds.
	 */
	vec2_t st_mins, st_maxs;

	/**
	 * @brief The stainmap for this lightmap.
	 */
	byte *stainmap;
} r_bsp_face_lightmap_t;

/**
 * @brief BSP faces, which may reside on the front or back of their node.
 */
typedef struct {
	struct r_bsp_node_s *node;

	r_bsp_brush_side_t *brush_side;

	box3_t bounds;

	r_bsp_face_lightmap_t lightmap;

	r_bsp_vertex_t *vertexes;
	int32_t num_vertexes;

	GLvoid *elements;
	int32_t num_elements;

	int32_t stain_frame;
} r_bsp_face_t;

/**
 * @brief BSP draw elements, which include all opaque faces of a given material
 * within a particular inline model.
 */
typedef struct {
	r_bsp_plane_t *plane;
	byte plane_side;

	r_bsp_texinfo_t *texinfo;
	int32_t contents;

	box3_t bounds;

	GLvoid *elements;
	int32_t num_elements;

	vec2_t st_origin;

	int32_t blend_depth_types;
} r_bsp_draw_elements_t;

/**
 * @brief BSP occlusion queries are defined by brushes with CONTENTS_OCCLUSION_QUERY.
 * @remarks Occlusion queries are processed once per frame. Objects residing completely
 * within such a brush may be culled if the occlusion query produces no visible samples.
 */
typedef struct {
	GLuint name;

	box3_t bounds;

	vec3_t vertexes[8];

	_Bool pending;
	
	GLint result;
} r_bsp_occlusion_query_t;

/**
 * @brief BSP nodes comprise the tree representation of the world.
 */
struct r_bsp_node_s {
	// common with leaf
	int32_t contents; // -1, to differentiate from leafs

	box3_t bounds;

	struct r_bsp_node_s *parent;
	struct r_bsp_inline_model_s *model;

	// node specific
	r_bsp_plane_t *plane;
	struct r_bsp_node_s *children[2];

	r_bsp_face_t *faces;
	int32_t num_faces;
};

typedef struct r_bsp_node_s r_bsp_node_t;

/**
 * @brief BSP leafs terminate the branches of the BSP tree.
 */
struct r_bsp_leaf_s {
	// common with node
	int32_t contents;

	box3_t bounds;

	struct r_bsp_node_s *parent;
	struct r_bsp_inline_model_s *model;
};

typedef struct r_bsp_leaf_s r_bsp_leaf_t;

/**
 * @brief The BSP is organized into one or more models (trees). The first model is
 * the worldspawn model, and typically is the largest. An additional model exists
 * for each entity that contains brushes. Non-worldspawn models can move and rotate.
 */
typedef struct r_bsp_inline_model_s {
	/**
	 * @brief The head node of this inline model.
	 * @brief This is a pointer into the BSP model's nodes array.
	 */
	r_bsp_node_t *head_node;

	/**
	 * @brief For frustum culling.
	 */
	box3_t bounds;

	/**
	 * @brief The faces of this inline model.
	 * @remarks This is a pointer into the BSP model's faces array.
	 */
	r_bsp_face_t *faces;
	int32_t num_faces;

	/**
	 * @brief The alpha blended draw elements of this inline model, sorted by depth each frame.
	 */
	GPtrArray *blend_elements;

	/**
	 * @brief The draw elements of this inline model.
	 * @brief This is a pointer into the BSP model's draw elements array.
	 */
	r_bsp_draw_elements_t *draw_elements;
	int32_t num_draw_elements;
} r_bsp_inline_model_t;

/**
 * @brief
 */
typedef struct {
	/**
	 * @brief The atlas width.
	 */
	int32_t width;

	/**
	 * @brief The lightmap atlas.
	 */
	r_image_t *atlas;
} r_bsp_lightmap_t;

/**
 * @brief
 */
typedef struct {
	/**
	 * @brief The lightgrid size in luxels.
	 */
	vec3i_t size;

	/**
	 * @brief The lightgrid bounds in world space.
	 */
	box3_t bounds;

	/**
	 * @brief The lightgrid textures (ambient, diffuse, etc..).
	 */
	r_image_t *textures[BSP_LIGHTGRID_TEXTURES + BSP_FOG_TEXTURES];
} r_bsp_lightgrid_t;

/**
 * @brief The renderer representation of the BSP model.
 */
typedef struct {

	cm_bsp_t *cm;

	int32_t num_planes;
	r_bsp_plane_t *planes;

	int32_t num_texinfo;
	r_bsp_texinfo_t *texinfo;

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

	int32_t num_occlusion_queries;
	r_bsp_occlusion_query_t *occlusion_queries;

	int32_t num_nodes;
	r_bsp_node_t *nodes;

	int32_t num_leafs;
	r_bsp_leaf_t *leafs;

	int32_t num_inline_models;
	r_bsp_inline_model_t *inline_models;

	int32_t luxel_size;

	r_bsp_lightmap_t *lightmap;
	r_bsp_lightgrid_t *lightgrid;

	r_bsp_draw_elements_t *sky;

	GLuint vertex_array;
	GLuint vertex_buffer;
	GLuint elements_buffer;

	GLuint depth_pass_elements_buffer;
	GLuint num_depth_pass_elements;

} r_bsp_model_t;

// mesh model, used for objects
typedef struct {
	vec3_t position;
	vec3_t normal;
	vec3_t tangent;
	vec3_t bitangent;
	vec2_t diffusemap;
} r_mesh_vertex_t;

typedef struct {
	box3_t bounds;
	vec3_t translate;
} r_mesh_frame_t;

typedef struct {
	char name[MAX_QPATH];
	mat4_t matrix;
} r_mesh_tag_t;

typedef struct {
	char name[MAX_QPATH];

	r_material_t *material;

	r_mesh_vertex_t *vertexes;
	int32_t num_vertexes;

	vec3_t *shell_normals;

	GLvoid *elements;
	int32_t num_elements;
} r_mesh_face_t;

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
	uint32_t flags; // EF_ALPHA_TEST, etc..
} r_mesh_config_t;

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

	struct {
		r_mesh_config_t world;
		r_mesh_config_t view;
		r_mesh_config_t link;
	} config;

	vec3_t *shell_normals;

	// buffer data
	GLuint vertex_buffer;
	GLuint elements_buffer;
	GLuint vertex_array;
	GLuint shell_normals_buffer;
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

	r_material_t **materials;
	size_t num_materials;

	box3_t bounds;
	float radius;
} r_model_t;

#define IS_BSP_MODEL(m) (m && m->type == MOD_BSP)
#define IS_BSP_INLINE_MODEL(m) (m && m->type == MOD_BSP_INLINE)
#define IS_MESH_MODEL(m) (m && m->type == MOD_MESH)

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
 * @brief
 */
enum {
	/**
	 * @brief If set, sprite does not attempt to check blend depth.
	 */
	SPRITE_NO_BLEND_DEPTH	= 1 << 0,

	/**
	 * @brief If set, animations don't interpolate
	 */
	SPRITE_NO_LERP			= 1 << 1,

	/**
	 * @brief If set, the sprite will ignore the depth buffer entirely.
	 */
	SPRITE_NO_DEPTH			= 1 << 2,

	/**
	 * @brief Beginning of flags reserved for cgame
	 */
	SPRITE_CGAME			= 1 << 16
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
	color32_t color;
	
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

#define MAX_SPRITES		0x8000

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
	color32_t color;
	
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
	color32_t color;
	float lerp;
	float softness;
	float lighting;
} r_sprite_vertex_t;

/**
 * @brief An instance of a renderable sprite.
 */
typedef struct r_sprite_instance_s {
	/**
	 * @brief The sprite flags.
	 */
	r_sprite_flags_t flags;

	/**
	 * @brief The diffusemap texture.
	 */
	const r_image_t *diffusemap;

	/**
	 * @brief The next diffusemap texture for frame interpolation.
	 */
	const r_image_t *next_diffusemap;

	/**
	 * @brief The sprite vertexes in the shared array.
	 */
	r_sprite_vertex_t *vertexes;

	/**
	 * @brief The element offset in the shared array.
	 */
	ptrdiff_t offset;

	/**
	 * @brief The blend depth at which this sprite should be rendered.
	 */
	int32_t blend_depth;

	/**
	 * @brief The next sprite instance to be rendered at the same blend depth.
	 */
	struct r_sprite_instance_s *tail, *head, *prev, *next;
} r_sprite_instance_t;

#define MAX_SPRITE_INSTANCES (MAX_SPRITES + MAX_BEAMS)

/**
 * @brief Stains are low-resolution color effects added to the map's lightmap
 * data. They are persistent for the duration of the map.
 */
typedef struct {
	/**
	 * @brief The stain origin.
	 */
	vec3_t origin;

	/**
	 * @brief The stain radius.
	 */
	float radius;

	/**
	 * @brief The stain color.
	 */
	color_t color;
} r_stain_t;

#define MAX_STAINS			0x400

/**
 * @brief Dynamic light sources.
 * @remarks This struct is vec4 aligned.
 */
typedef struct {
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
} r_light_t;

/**
 * @brief 32 dynamic light sources per scene.
 */
#define MAX_LIGHTS			0x20

/**
 * @brief Entity sub-mesh skins.
 */
#define MAX_ENTITY_SKINS 	0x8

/**
 * @brief Entities provide a means to add model instances to the view. Entity
 * lighting is cached on the client entity so that it is only recalculated
 * when an entity moves.
 */
typedef struct r_entity_s {
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
	r_material_t *skins[MAX_ENTITY_SKINS];

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

	/**
	 * @brief The alpha blended element behind which this entity should be rendered.
	 */
	int32_t blend_depth;
} r_entity_t;

/**
 * @brief The framebuffer type.
 */
typedef struct {
	/**
	 * @brief The framebuffer name.
	 */
	GLuint name;

	/**
	 * @brief The color attachment texture name.
	 */
	GLuint color_attachment;

	/**
	 * @brief The depth attachment texture name.
	 */
	GLuint depth_attachment;

	/**
	 * @brief The framebuffer width.
	 */
	r_pixel_t width;

	/**
	 * @brief The framebuffer height.
	 */
	r_pixel_t height;

	/**
	 * @brief True if this framebuffer supports multisampling (MSAA).
	 */
	_Bool multisample;
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
 * @brief Each client frame populates a view, and submits it to the renderer.
 */
typedef struct {
	/**
	 * @brief The view type.
	 */
	r_view_type_t type;

	/**
	 * @brief The target framebuffer (required).
	 */
	r_framebuffer_t *framebuffer;

	/**
	 * @brief The viewport, in device pixels.
	 */
	vec4_t viewport;

	/**
	 * @brief The horizontal and vertical field of view.
	 */
	vec2_t fov;

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
	 * @brief The stains to render for the current frame.
	 */
	r_stain_t stains[MAX_STAINS];
	int32_t num_stains;

	/**
	 * @brief The view frustum, for box and sphere culling.
	 * @remarks This is populated by the renderer.
	 */
	cm_bsp_plane_t frustum[4];

} r_view_t;

/**
 * @brief Convenience inline function to clear a view.
 * Use this instead of memset.
 */
static inline void R_ClearView(r_view_t *view) {
	view->num_beams = view->num_entities = view->num_lights = view->num_sprites =
		view->num_sprite_instances = view->num_stains = 0;

	memset(view->frustum, 0, sizeof(view->frustum));
	view->angles = Vec3_Zero();
	view->contents = 0;
	view->forward = Vec3_Zero();
	view->fov = Vec2_Zero();
	view->origin = Vec3_Zero();
	view->right = Vec3_Zero();
	view->up = Vec3_Zero();
	view->viewport = Vec4_Zero();
}

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
	SDL_GLContext *context;

	/**
	 * @brief OpenGL context size in drawable pixels, as reported by SDL_GL_GetDrawableSize.
	 */
	r_pixel_t drawable_width, drawable_height;

	/**
	 * @brief Window size as reported by SDL_GetWindowSize (High-DPI compatibility).
	 */
	r_pixel_t width, height;

	/**
	 * @brief The display vertical refresh rate, in Hz.
	 */
	float refresh_rate;

	/**
	 * @brief Greater than 1.0 if High DPI mode is enabled.
	 */
	float window_scale;

	/**
	 * @brief True if fullscreen, false if windowed.
	 */
	_Bool fullscreen;

	/**
	 * @brief Number of samples for multisampled buffers
	 */
	int32_t multisample_samples;
} r_context_t;

/**
 * @brief Renderer statistics.
 */
typedef struct {

	int32_t count_bsp_inline_models;
	int32_t count_bsp_draw_elements;
	int32_t count_bsp_draw_elements_blend;
	int32_t count_bsp_triangles;
	int32_t count_bsp_occlusion_queries;
	int32_t count_bsp_occlusion_queries_passed;

	int32_t count_mesh_models;
	int32_t count_mesh_triangles;

	int32_t count_sprite_draw_elements;

	int32_t count_draw_chars;
	int32_t count_draw_fills;
	int32_t count_draw_images;
	int32_t count_draw_lines;
	int32_t count_draw_arrays;

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
	 * @brief The lightmap texture, used by the BSP program.
	 */
	TEXTURE_LIGHTMAP,

	/**
	 * @brief The lightgrid textures, used for mesh lighting, and universally for fog.
	 */
	TEXTURE_LIGHTGRID,
	TEXTURE_LIGHTGRID_AMBIENT = TEXTURE_LIGHTGRID,
	TEXTURE_LIGHTGRID_DIFFUSE,
	TEXTURE_LIGHTGRID_DIRECTION,
	TEXTURE_LIGHTGRID_FOG,

	/**
	 * @brief The sky cubemap texture.
	 */
	TEXTURE_SKY,

	/**
	 * @brief Sprite specific textures.
	 */
	TEXTURE_NEXT_DIFFUSEMAP,
	TEXTURE_DEPTH_STENCIL_ATTACHMENT,
} r_texture_t;

#endif /* __R_LOCAL_H__ */
