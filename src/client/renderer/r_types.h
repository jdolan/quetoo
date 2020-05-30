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

#include "atlas.h"
#include "collision/collision.h"
#include "files.h"
#include "image.h"
#include "matrix.h"
#include "thread.h"

#include "r_gl_types.h"

/**
 * @brief Media identifier type
 */
typedef enum {
	MEDIA_GENERIC, // unknown/generic type
	MEDIA_IMAGE, // r_image_t
	MEDIA_ATLAS, // r_atlas_t
	MEDIA_ATLAS_IMAGE, // r_atlas_image_t
	MEDIA_ANIMATION, // r_animation_t

	MEDIA_MD3, //
	MEDIA_OBJ, // r_model_t
	MEDIA_BSP, //

	MEDIA_MATERIAL, // r_material_t

	MEDIA_TOTAL
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
#define IT_MASK_FLAGS		(IT_MASK_MIPMAP | IT_MASK_CLAMP_EDGE)

// image categories (bits 0..23) + flags are making image types
typedef enum {
	IT_PROGRAM =     (1 <<  1),
	IT_FONT =        (1 <<  2),
	IT_UI =          (1 <<  3),
	IT_EFFECT =      (1 <<  4) + (IT_MASK_MIPMAP),
	IT_MATERIAL =    (1 <<  5) + (IT_MASK_MIPMAP),
	IT_PIC =         (1 <<  6) + (IT_MASK_MIPMAP),
	IT_ATLAS =       (1 <<  7) + (IT_MASK_MIPMAP | IT_MASK_CLAMP_EDGE),
	IT_LIGHTMAP =    (1 <<  8) + (IT_MASK_CLAMP_EDGE),
	IT_LIGHTGRID =   (1 <<  9) + (IT_MASK_CLAMP_EDGE)
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

	cm_bsp_plane_t *plane;
	byte plane_side;

	r_bsp_texinfo_t *texinfo;
	int32_t contents;

	struct r_sprite_s *flare;

	r_bsp_face_lightmap_t lightmap;

	r_bsp_vertex_t *vertexes;
	int32_t num_vertexes;

	GLvoid *elements;
	int32_t num_elements;

	int32_t stain_frame;
} r_bsp_face_t;

/**
 * @brief BSP draw elements, which include one or more faces on a given node.
 * @remarks Draw elements may span both sides of the node. We rely on back face culling to deal
 * with that, and avoid the work of testing and marking plane sidedness.
 */
typedef struct {
	struct r_bsp_node_s *node;

	r_bsp_texinfo_t *texinfo;
	int32_t contents;

	r_bsp_face_t *faces;
	int32_t num_faces;

	GLvoid *elements;
	int32_t num_elements;

	vec2_t st_origin;
} r_bsp_draw_elements_t;

/**
 * @brief BSP nodes comprise the tree representation of the world. At compile
 * time, the map is divided into convex volumes that fall along brushes
 * (walls). These volumes become nodes. The planes these divisions create
 * provide a basis for testing all other nodes in the world for sidedness
 * using the dot-product check: DOT(point, plane.normal) - plane.dist.
 * Starting from the origin, this information is gathered into a tree structure
 * with which a simple recursion can quickly determine:
 *
 *  a. Which nodes are in front of my view vector from my current origin?
 *  b. Of those, which nodes are facing me?
 *
 * This is the basis for all collision detection and rendering in Quake.
 */
struct r_bsp_node_s {
	// common with leaf
	int32_t contents; // -1, to differentiate from leafs

	vec3_t mins; // for frustum culling
	vec3_t maxs;

	struct r_bsp_node_s *parent;
	struct r_bsp_inline_model_s *model;

	int32_t vis_frame;

	// node specific
	cm_bsp_plane_t *plane;
	struct r_bsp_node_s *children[2];

	r_bsp_face_t *faces;
	int32_t num_faces;

	r_bsp_draw_elements_t *draw_elements;
	int32_t num_draw_elements;

	int32_t lights_mask;
	int32_t blend_depth;
};

typedef struct r_bsp_node_s r_bsp_node_t;

/**
 * @brief BSP leafs terminate the branches of the BSP tree and are grouped into
 * clusters that are the unit of granularity for the PVS. If a leaf's cluster is
 * visible, all ancestors of that leaf should be marked as visible.
 */
struct r_bsp_leaf_s {
	// common with node
	int32_t contents;

	vec3_t mins; // for frustum culling
	vec3_t maxs;

	struct r_bsp_node_s *parent;
	struct r_bsp_inline_model_s *model;

	int32_t vis_frame;

	// leaf specific
	int32_t cluster;
	int32_t area;
};

typedef struct r_bsp_leaf_s r_bsp_leaf_t;

/**
 * @brief The BSP is organized into one or more models (trees). The first model is
 * the world, and typically is the largest. An additional model exist for each entity
 * that contains brushes. Non-world models can move and rotate.
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
	vec3_t mins;
	vec3_t maxs;

	/**
	 * @brief The faces of this inline model.
	 * @remarks This is a pointer into the BSP model's faces array.
	 */
	r_bsp_face_t *faces;
	int32_t num_faces;

	/**
	 * @brief The faces of this inline model that include flares, sorted by material at level load.
	 */
	GPtrArray *flare_faces;

	/**
	 * @brief The draw elements of this inline model.
	 * @brief This is a pointer into the BSP model's draw elements array.
	 */
	r_bsp_draw_elements_t *draw_elements;
	int32_t num_draw_elements;

	/**
	 * @brief The opaque draw elements of this inline model, sorted by material at level load.
	 */
	GPtrArray *opaque_draw_elements;

	/**
	 * @brief The alpha blended draw elements of this inline model, sorted by depth each frame.
	 */
	GPtrArray *alpha_blend_draw_elements;

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
	vec3_t mins, maxs;

	/**
	 * @brief The lightgrid textures (ambient, diffuse, etc..).
	 */
	r_image_t *textures[BSP_LIGHTGRID_TEXTURES];
} r_bsp_lightgrid_t;

/**
 * @brief The renderer representation of the BSP model.
 */
typedef struct {

	cm_bsp_t *cm;

	int32_t num_texinfo;
	r_bsp_texinfo_t *texinfo;

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

	int32_t num_inline_models;
	r_bsp_inline_model_t *inline_models;

	int32_t luxel_size;

	r_bsp_lightmap_t *lightmap;
	r_bsp_lightgrid_t *lightgrid;

	GLuint vertex_buffer;
	GLuint elements_buffer;
	GLuint vertex_array;

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
	vec3_t mins;
	vec3_t maxs;
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

	// buffer data
	GLuint vertex_buffer;
	GLuint elements_buffer;
	GLuint vertex_array;
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

	vec3_t mins, maxs;
	float radius;

} r_model_t;

#define IS_BSP_MODEL(m) (m && m->type == MOD_BSP)
#define IS_BSP_INLINE_MODEL(m) (m && m->type == MOD_BSP_INLINE)
#define IS_MESH_MODEL(m) (m && m->type == MOD_MESH)

/**
 * @brief Particles are alpha blended points.
 */
typedef struct {

	/**
	 * @brief The particle origin.
	 */
	vec3_t origin;

	/**
	 * @brief The particle size.
	 */
	float size;

	/**
	 * @brief The particle color.
	 */
	color_t color;

} r_particle_t;

#define MAX_PARTICLES		0x7ffe

/**
 * @brief Sprites are billboarded alpha blended textures.
 */
typedef struct r_sprite_s {

	/**
	 * @brief The sprite origin.
	 */
	vec3_t origin;

	/**
	 * @brief The sprite size.
	 */
	float size;
	
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
	color_t color;
	
	/**
	 * @brief The sprite's life from 0 to 1.
	 */
	float life;

	/**
	 * @brief Whether animations should use texture lerping
	 */
	_Bool lerp;

	/**
	 * @brief Direction of the sprite. { 0, 0, 0 } is billboard.
	 */
	vec3_t dir;
} r_sprite_t;

typedef struct {
	/**
	 * @brief Sprite base data
	 */
	r_sprite_t sprite;

	/**
	 * @brief Normal of the sprite
	 */
	vec3_t dir;
} r_directional_sprite_t;

/**
 * @brief Beams are billboarded alpha blended textures.
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
	color_t color;
	
	/**
	 * @brief The beam texture translation.
	 */
	float translate;
			
	/**
	 * @brief The beam texture stretch.
	 */
	float stretch;
} r_beam_t;

#define MAX_SPRITES		0x8000

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

#define MAX_LIGHTS			0x20

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
	 * @brief The entity bounds, for frustum culling.
	 */
	vec3_t mins, maxs;

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
	 * @brief The entity shade color for e.g. `EF_PULSE`.
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
	 * @brief The entity light mask for dynamic light sources.
	 */
	int32_t lights;

	/**
	 * @brief The alpha blended depth in which this entity should be rendered.
	 */
	int32_t blend_depth;
} r_entity_t;

#define WEATHER_NONE        0x0
#define WEATHER_RAIN        0x1
#define WEATHER_SNOW        0x2
#define WEATHER_FOG         0x4

#define FOG_START			0.f
#define FOG_END				MAX_WORLD_AXIAL
#define FOG_DENSITY			1.f

/**
 * @brief Structure used for sprite images, to contain buffered sprites
 */
typedef struct {
	/**
	 * @brief Image
	 */
	const r_image_t *diffusemap;

	/**
	 * @brief Animation interpolation next image
	 */
	const r_image_t *next_diffusemap;

	/**
	 * @brief The instance count.
	 */
	int32_t count;
} r_sprite_instance_t;

/**
 * @brief Each client frame populates a view, and submits it to the renderer.
 */
typedef struct {

	/**
	 * @brief The unclamped simulation time, in millis.
	 */
	uint32_t ticks;

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
	 * @brief If not NULL, only BSP areas matching the corresponding bits will be drawn.
	 */
	byte *area_bits;

	/**
	 * @brief A bitmask of weather effects.
	 */
	int32_t weather;

	/**
	 * @brief The fog color.
	 */
	vec3_t fog_color;

	/**
	 * @brief The fog start, end and density.
	 */
	vec3_t fog_parameters;

	/**
	 * @brief The entities to render for the current frame.
	 */
	r_entity_t entities[MAX_ENTITIES];
	int32_t num_entities;

	/**
	 * @brief The sprites to render for the current frame.
	 */
	int32_t num_sprites;

	/**
	 * @brief The animated sprite instances to render for the current frame.
	 */
	r_sprite_instance_t sprite_instances[MAX_SPRITES];
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

	// counters, reset each frame

	int32_t count_bsp_inline_models;
	int32_t count_bsp_leafs;
	int32_t count_bsp_nodes;
	int32_t count_bsp_triangles;

	int32_t count_mesh_models;
	int32_t count_mesh_triangles;

	int32_t count_draw_chars;
	int32_t count_draw_fills;
	int32_t count_draw_images;
	int32_t count_draw_lines;

	_Bool update; // inform the client of state changes

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
	 * @brief The OpenGL 2.1 context.
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
	 * @brief Greater than 1.0 if High DPI mode is enabled.
	 */
	float window_scale;

	/**
	 * @brief True if fullscreen, false if windowed.
	 */
	_Bool fullscreen;
	
	/**
	 * @brief Framebuffer things.
	 */
	GLuint framebuffer, color_attachment, depth_stencil_attachment;
} r_context_t;

#ifdef __R_LOCAL_H__
#endif /* __R_LOCAL_H__ */
