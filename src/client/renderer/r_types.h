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

	MEDIA_MD3, //
	MEDIA_OBJ, // r_model_t
	MEDIA_BSP, //

	MEDIA_MATERIAL, // r_material_t

	MEDIA_TOTAL
} r_media_type_t;

// media handles
typedef struct r_media_s {
	char name[MAX_QPATH];
	r_media_type_t type;
	GList *dependencies;
	void (*Register)(struct r_media_s *self);
	_Bool (*Retain)(struct r_media_s *self);
	void (*Free)(struct r_media_s *self);
	int32_t seed;
} r_media_t;

typedef int16_t r_pixel_t;

typedef enum {
	MOD_BAD,
	MOD_BSP,
	MOD_BSP_INLINE,
	MOD_MESH
} r_model_type_t;

// high bits OR'ed with image categories, flags are bits 24..31
#define IT_MASK_MIPMAP		1 << 24
#define IT_MASK_FILTER		1 << 25
#define IT_MASK_MULTIPLY	1 << 26
#define IT_MASK_FLAGS		(IT_MASK_MIPMAP | IT_MASK_FILTER | IT_MASK_MULTIPLY)

// image categories (bits 0..23) + flags are making image types
typedef enum {
	IT_NULL =        (1 <<  0),
	IT_PROGRAM =     (1 <<  1),
	IT_FONT =        (1 <<  2) + (IT_MASK_FILTER),
	IT_UI =          (1 <<  3) + (IT_MASK_FILTER),
	IT_EFFECT =      (1 <<  4) + (IT_MASK_MIPMAP | IT_MASK_FILTER),
	IT_DIFFUSE =     (1 <<  5) + (IT_MASK_MIPMAP | IT_MASK_FILTER),
	IT_LIGHTMAP =    (1 <<  6) + (IT_MASK_FILTER),
	IT_LIGHTGRID =   (1 <<  7) + (IT_MASK_FILTER),
	IT_NORMALMAP =   (1 <<  8) + (IT_MASK_MIPMAP | IT_MASK_FILTER),
	IT_GLOSSMAP = (1 <<  9) + (IT_MASK_MIPMAP | IT_MASK_FILTER),
	IT_ENVMAP =      (1 << 10) + (IT_MASK_MIPMAP | IT_MASK_FILTER),
	IT_FLARE =       (1 << 11) + (IT_MASK_MIPMAP | IT_MASK_FILTER | IT_MASK_MULTIPLY),
	IT_SKY =         (1 << 12) + (IT_MASK_MIPMAP | IT_MASK_FILTER),
	IT_PIC =         (1 << 13) + (IT_MASK_MIPMAP | IT_MASK_FILTER),
	IT_ATLAS =       (1 << 14) + (IT_MASK_MIPMAP),
	IT_STAINMAP =    (1 << 15) + (IT_MASK_FILTER),
	IT_TINTMAP =     (1 << 16) + (IT_MASK_MIPMAP | IT_MASK_FILTER)
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

	/**
	 * @brief The average color of the image.
	 */
	vec3_t color;
} r_image_t;

/**
 * @brief An image atlas.
 */
typedef struct {
	r_media_t media;
	r_image_t *image;
	atlas_t *atlas;
} r_atlas_t;

/**
 * @brief An atlas image, castable to r_image_t and r_media_t.
 */
typedef struct {
	r_image_t image;
	atlas_node_t *node;
	vec4_t texcoords;
} r_atlas_image_t;

typedef enum {
	PARTICLE_INVALID = -1,
	PARTICLE_DEFAULT,
	PARTICLE_SPARK,
	PARTICLE_ROLL,
	PARTICLE_EXPLOSION,
	PARTICLE_BUBBLE,
	PARTICLE_BEAM,
	PARTICLE_WEATHER,
	PARTICLE_SPLASH,
	PARTICLE_CORONA,
	PARTICLE_FLARE,
	PARTICLE_WIRE
} r_particle_type_t;

typedef enum {
	PARTICLE_FLAG_NONE,

	/**
	 * @brief Only for PARTICLE_BEAM - causes the beam to
	 * repeat instead of stretch
	 */
	PARTICLE_FLAG_REPEAT = 1 << 0,

	/**
	 * @brief Always sort closest to camera
	 */
	PARTICLE_FLAG_NO_DEPTH = 1 << 1,
} r_particle_flags_t;

/**
 * @brief Particles are alpha-blended quads.
 */
typedef struct r_particle_s {
	r_particle_type_t type;
	const r_media_t *media;
	GLenum blend;
	vec4_t color;
	vec_t scale;
	vec3_t org;
//	vec3_t end;
	r_particle_flags_t flags;
	vec_t repeat_scale;
} r_particle_t;

#define MAX_PARTICLES		0x4000

// renderer-specific material stuff
typedef struct {
	vec_t dhz;
} r_stage_pulse_t;

typedef struct {
	vec_t dhz;
	vec_t damp;
} r_stage_stretch_t;

typedef struct {
	vec_t deg;
} r_stage_rotate_t;

typedef struct {
	vec_t ds, dt;
} r_stage_scroll_t;

// frame based material animation, lerp between consecutive images
typedef struct {
	r_image_t **frames;
	uint32_t dtime;
	uint16_t dframe;
} r_stage_anim_t;

typedef struct r_stage_s {
	const struct cm_stage_s *cm; // link to cm stage

	// renderer-local stuff parsed from cm
	struct r_material_s *material;
	r_image_t *image;
	r_stage_pulse_t pulse;
	r_stage_stretch_t stretch;
	r_stage_rotate_t rotate;
	r_stage_scroll_t scroll;
	r_stage_anim_t anim;

	// next stage
	struct r_stage_s *next;
} r_stage_t;

typedef struct r_material_s {
	// from media
	r_media_t media;

	struct cm_material_s *cm; // the parsed material

	// renderer-local stuff parsed from cm
	r_image_t *diffuse;
	r_image_t *normalmap;
	r_image_t *glossmap;
	r_image_t *tintmap;

	uint32_t time;

	r_stage_t *stages;
} r_material_t;

// bsp model memory representation
typedef struct {
	vec3_t mins, maxs;
	vec3_t origin; // for sounds or lights
	vec_t radius;
	int32_t head_node;
	int32_t first_surface, num_faces;
} r_bsp_inline_model_t;

/**
 * @brief Resolves a unique(ish) stencil reference value for the given plane number.
 */
#define R_STENCIL_REF(pnum) (((pnum) % 0xff) + 1)

typedef struct {
	vec_t vecs[2][4];
	int32_t flags;
	int32_t value;
	char texture[32];
	r_material_t *material;
} r_bsp_texinfo_t;

typedef struct {
	vec_t radius;
	uint32_t time;
	vec_t alpha;

	r_particle_t particle;
} r_bsp_flare_t;

// r_bsp_face_t flags
#define R_SURF_BACK_SIDE	0x1
#define R_SURF_IN_LIQUID	0x2

typedef struct {
	r_image_t *atlas;
} r_bsp_lightmap_t;

/**
 * @brief Each indivudual surface lightmap has a projection matrix.
 */
typedef struct {
	r_bsp_lightmap_t *atlas; // the lightmap atlas containing this lightmap

	r_pixel_t s, t; // the texture coordinates into the atlas image
	r_pixel_t w, h;

	vec2_t st_mins, st_maxs;

} r_bsp_face_lightmap_t;

typedef struct {
	cm_bsp_plane_t *plane;
	r_bsp_texinfo_t *texinfo;
	r_bsp_flare_t *flare;

	r_bsp_face_lightmap_t lightmap;

	vec3_t mins, maxs;
	vec2_t st_mins, st_maxs;

	int32_t flags; // R_SURF flags

	int32_t first_vertex;
	int32_t num_vertexes;

	int32_t first_element;
	int32_t num_elements;

	int16_t vis_frame; // PVS frame
	int16_t frame; // renderer frame
	int16_t light_frame; // dynamic lighting frame
	uint64_t light_mask; // bit mask of dynamic light sources
} r_bsp_face_t;

typedef struct {
	r_bsp_texinfo_t *texinfo;
	r_bsp_lightmap_t *lightmap;

	int32_t first_element;
	int32_t num_elements;
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
typedef struct r_bsp_node_s {
	// common with leaf
	int32_t contents; // -1, to differentiate from leafs
	int16_t vis_frame; // node needs to be traversed if current

	vec3_t mins; // for bounded box culling
	vec3_t maxs;

	struct r_bsp_node_s *parent;
	struct r_model_s *model;

	// node specific
	cm_bsp_plane_t *plane;
	struct r_bsp_node_s *children[2];

	r_bsp_face_t *faces;
	int32_t num_faces;
} r_bsp_node_t;

/**
 * @brief BSP leafs terminate the branches of the BSP tree and provide grouping
 * for surfaces. If a leaf is found to be in the potentially visible set (PVS)
 * for a given frame, then all surfaces associated to that leaf are flagged for
 * drawing.
 */
typedef struct {
	// common with node
	int32_t contents; // will be a negative contents number
	int16_t vis_frame; // node needs to be traversed if current

	vec3_t mins; // for bounding box culling
	vec3_t maxs;

	struct r_bsp_node_s *parent;
	struct r_model_s *model;

	// leaf specific
	int32_t cluster;
	int32_t area;

	r_bsp_face_t **leaf_faces;
	int32_t num_leaf_faces;

	r_bsp_draw_elements_t *draw_elements;
	int32_t num_draw_elements;
} r_bsp_leaf_t;

/**
 * @brief BSP vertex structure.
 */
typedef struct {
	vec3_t position;
	vec3_t normal;
	vec3_t tangent;
	vec3_t bitangent;
	vec2_t diffuse;
	vec2_t lightmap;
	vec4_t color;
} r_bsp_vertex_t;

/**
 * @brief
 */
typedef struct {
	int16_t vis_frame; // PVS eligibility
} r_bsp_cluster_t;

/**
 * @brief BSP light sources.
 */
typedef struct {
	bsp_light_type_t type;
	bsp_light_atten_t atten;
	vec3_t origin;
	vec3_t color;
	vec3_t normal;
	vec_t radius;
	vec_t theta;

	const r_bsp_leaf_t *leaf;

	r_particle_t debug;

} r_bsp_light_t;

// mesh model, used for objects
typedef struct {
	vec3_t position;
	vec3_t normal;
	vec3_t tangent;
	vec3_t bitangent;
} r_model_vertex_t;

typedef struct {
	char name[MD3_MAX_PATH];
	matrix4x4_t matrix;
} r_model_tag_t;

typedef struct {
	char name[MD3_MAX_PATH];

	uint16_t num_verts;
	uint16_t num_tris;
	uint32_t num_elements;

	r_material_t *material;
} r_model_mesh_t;

typedef struct {
	uint16_t first_frame;
	uint16_t num_frames;
	uint16_t looped_frames;
	uint16_t hz;
} r_model_animation_t;

// BSP model, used for maps
typedef struct {

	/**
	 * @brief Reference to the cm_bsp_t that is currently loaded. We use this
	 * to populate some stuff in r_bsp.
	 */
	cm_bsp_t *cm;

	int32_t num_texinfo;
	r_bsp_texinfo_t *texinfo;

	int32_t num_vertexes;
	r_bsp_vertex_t *vertexes;

	int32_t num_elements;
	GLuint *elements;

	int32_t num_draw_elements;
	r_bsp_draw_elements_t *draw_elements;

	int32_t num_lightmaps;
	r_bsp_lightmap_t *lightmaps;

	int32_t num_faces;
	r_bsp_face_t *faces;

	int32_t num_leaf_faces;
	r_bsp_face_t **leaf_faces;

	int32_t num_nodes;
	r_bsp_node_t *nodes;

	int32_t num_leafs;
	r_bsp_leaf_t *leafs;

	int32_t num_inline_models;
	r_bsp_inline_model_t *inline_models;

	int32_t num_clusters;
	r_bsp_cluster_t *clusters;

	int16_t luxel_size;

	int32_t num_lights;
	r_bsp_light_t *lights;

	// vertex buffer
	GLuint vertex_buffer;

	// elements buffer
	GLuint elements_buffer;

	// vertex array
	GLuint vertex_array;

} r_bsp_model_t;

/**
 * @brief Provides load-time normalization of mesh models.
 */
typedef struct {
	vec3_t translate;
	vec3_t rotate;
	vec_t scale;
	uint32_t flags; // EF_ALPHA_TEST, etc..
} r_mesh_config_t;

typedef struct {
	uint32_t flags;

	uint32_t num_verts;
	uint16_t num_frames;
	uint16_t num_tags;
	uint16_t num_meshes;
	uint16_t num_animations;

	r_model_tag_t *tags;
	r_model_mesh_t *meshes;
	r_model_animation_t *animations;

	r_mesh_config_t world_config;
	r_mesh_config_t view_config;
	r_mesh_config_t link_config;

	// buffer data
	GLuint vertex_buffer;
	GLuint elements_buffer;
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
	vec_t radius;

} r_model_t;

#define IS_BSP_MODEL(m) (m && m->type == MOD_BSP)
#define IS_BSP_INLINE_MODEL(m) (m && m->type == MOD_BSP_INLINE)
#define IS_MESH_MODEL(m) (m && m->type == MOD_MESH)

/**
 * @brief Stains are low-resolution color effects added to the map's lightmap
 * data. They are persistent for the duration of the map.
 */
typedef struct {
	vec3_t origin;
	vec_t radius;
	const r_media_t *media;
	vec4_t color;
} r_stain_t;

#define MAX_STAINS			64

/**
 * @brief Dynamic light sources expire immediately and must be re-added
 * for each frame they appear.
 */
typedef struct {
	vec3_t origin;
	vec3_t color;
	vec_t radius;
} r_light_t;

#define MAX_LIGHTS			64

/**
 * @brief Sustains are light flashes which slowly decay over time. These
 * persist over multiple frames.
 */
typedef struct {
	r_light_t light;
	uint32_t time;
	uint32_t sustain;
} r_sustained_light_t;

typedef enum {
	ILLUM_AMBIENT = 0x1,
	ILLUM_SUN     = 0x2,
	ILLUM_STATIC  = 0x4,
	ILLUM_DYNAMIC = 0x8
} r_illumination_type_t;

/**
 * @brief Describes a light source contributions to point lighting.
 */
typedef struct {
	r_illumination_type_t type;
	r_light_t light;
	vec_t diffuse;
} r_illumination_t;

/**
 * @brief Describes the projection of a mesh model onto a BSP plane.
 */
typedef struct {
	const r_illumination_t *illumination;
	cm_bsp_plane_t plane;
	vec_t shadow;
} r_shadow_t;

/**
 * @brief Static lighting information is cached on the client entity structure.
 */
typedef enum {
	LIGHTING_DIRTY,
	LIGHTING_READY
} r_lighting_state_t;

/**
 * @brief Up to MAX_ILLUMINATIONS illuminations are calculated for each mesh entity. The first
 * illumination in the structure is reserved for world lighting (ambient and
 * sunlight). The remaining (MAX_ILLUMINATIONS - 1) are populated by both BSP and dynamic light
 * sources, by order of their contribution.
 */
#define MAX_ILLUMINATIONS 8

/**
 * @brief Up to MAX_PLANES_PER_SHADOW shadows are cast for up to MAX_ILLUMINATIONS_PER_SHADOW illuminations.
 * These are populated by tracing from the illumination position through the lighting origin and
 * bounds. A shadow is cast for each unique plane hit.
 */
#define MAX_ILLUMINATIONS_PER_SHADOW	(MAX_ILLUMINATIONS / 2)
#define MAX_PLANES_PER_SHADOW			3
#define MAX_SHADOWS						(MAX_ILLUMINATIONS_PER_SHADOW * MAX_PLANES_PER_SHADOW)

/**
 * @brief Provides lighting information for mesh entities. Illuminations and
 * shadows are maintained in separate arrays because they must be sorted by
 * different criteria: for illuminations, the light that reaches the entity
 * defines priority; for shadows, the negative light that reaches the plane on
 * which the shadow is cast does.
 */
typedef struct r_lighting_s {
	r_lighting_state_t state;
	uint16_t number; // entity number
	vec3_t origin; // entity origin
	vec_t radius; // entity radius
	vec3_t mins, maxs; // entity bounding box in world space
	uint64_t light_mask; // dynamic light sources mask
	r_illumination_t illuminations[MAX_ILLUMINATIONS]; // light sources, ordered by diffuse
	r_shadow_t shadows[MAX_SHADOWS]; // shadows, ordered by intensity
} r_lighting_t;

/**
 * @brief Entities provide a means to add model instances to the view. Entity
 * lighting is cached on the client entity so that it is only recalculated
 * when an entity moves.
 */
typedef struct r_entity_s {
	vec3_t origin;
	vec3_t termination;
	vec3_t angles;
	vec3_t mins, maxs;

	matrix4x4_t matrix;
	matrix4x4_t inverse_matrix;

	const r_model_t *model;

	uint16_t frame, old_frame; // frame-based animations
	vec_t lerp, back_lerp;

	vec_t scale; // for mesh models

	r_material_t *skins[MD3_MAX_MESHES]; // NULL for default skin
	uint16_t num_skins;

	uint32_t effects; // e.g. EF_NO_DRAW, EF_WEAPON, ..

	vec4_t color; // shaded color, e.g. EF_PULSE

	vec3_t shell; // shell color

	r_lighting_t *lighting; // static lighting information

	vec4_t tints[TINT_TOTAL]; // tint colors, non-zero alpha enables the tint
} r_entity_t;

/**
 * @brief Entity draw lists.
 */
typedef struct {
	const r_entity_t *entities[MAX_ENTITIES];
	size_t count;
} r_entities_t;

/**
 * @brief The view maintains lists of entities, sorted by render path.
 */
typedef struct {
	r_entities_t bsp_inline_entities;
	r_entities_t mesh_entities;
	r_entities_t null_entities;
} r_sorted_entities_t;

/**
 * @brief Appends a reference to the specified entity to the given entities list.
 */
#define R_ENTITY_TO_ENTITIES(ents, ent) (ents)->entities[(ents)->count++] = ent

/**
 * @brief Function prototype for mesh entity draw lists.
 */
typedef void (*MeshModelsDrawFunc)(const r_entities_t *ents);

#define WEATHER_NONE		0
#define WEATHER_RAIN 		1
#define WEATHER_SNOW 		2
#define WEATHER_FOG 		4
#define WEATHER_PRECIP_MASK	(WEATHER_RAIN | WEATHER_SNOW)

#define FOG_START			128.0
#define FOG_END				2048.0

/**
 * @brief Provides read-write visibility and scene management to the client.
 */
typedef struct {

	/**
	 * @brief The Viewport.
	 */
	SDL_Rect viewport;

	/**
	 * @brief The horizontal and vertical field of view.
	 */
	vec2_t fov;

	/**
	 * @brief The 3D projection matrix.
	 */
	matrix4x4_t projection3D;

	/**
	 * @brief The 2D projection matrix.
	 */
	matrix4x4_t projection2D;

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
	 * @brief The model view matrix.
	 */
	matrix4x4_t model_view;

	/**
	 * @brief The inverse model view matrix.
	 */
	matrix4x4_t inverse_model_view;

	/**
	 * @brief The inverse transpose model view matrix (the normal matrix).
	 */
	matrix4x4_t normal;

	/**
	 * @brief The contents mask at the view position.
	 */
	int32_t contents;

	/**
	 * @brief The unclamped simulation time, in millis.
	 */
	uint32_t ticks;

	/**
	 * @brief If not NULL, only BSP areas matching the corresponding bits will be drawn.
	 */
	byte *area_bits;

	/**
	 * @brief A bitmask of weather effects.
	 */
	byte weather;

	/**
	 * @brief The fog color.
	 */
	vec4_t fog;

	uint16_t num_entities;
	r_entity_t entities[MAX_ENTITIES];

	uint16_t num_particles;
	r_particle_t particles[MAX_PARTICLES];

	uint16_t num_lights;
	r_light_t lights[MAX_LIGHTS];

	uint16_t num_stains;
	r_stain_t stains[MAX_STAINS];

	r_sustained_light_t sustained_lights[MAX_LIGHTS];

	const r_entity_t *current_entity; // entity being rendered
	const r_shadow_t *current_shadow; // shadow being rendered

	// counters, reset each frame

	uint32_t num_bsp_clusters;
	uint32_t num_bsp_leafs;
	uint32_t num_bsp_faces;

	uint32_t cull_passes;
	uint32_t cull_fails;

	uint32_t num_draw_elements;
	uint32_t num_draw_arrays;

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
	r_pixel_t width, height;

	/**
	 * @brief Window size as reported by SDL_GetWindowSize (High-DPI compatibility).
	 */
	r_pixel_t window_width, window_height;

	/**
	 * @brief True if the application window uses High-DPI (Retina, 4K).
	 */
	_Bool high_dpi;

	/**
	 * @brief True if fullscreen, false if windowed.
	 */
	_Bool fullscreen;
} r_context_t;

#ifdef __R_LOCAL_H__

/**
 * @brief Quake3 (MD3) model in-memory representation.
 */
typedef struct {
	uint32_t *tris;
	r_model_vertex_t *verts;
	d_md3_texcoord_t *coords;
} r_md3_mesh_t;

typedef struct {
	uint16_t num_verts;
	uint16_t num_tris;

	d_md3_frame_t *frames;
	r_md3_mesh_t *meshes;
} r_md3_t;

/*
 * @brief Object (OBJ) model in-memory representation.
 */
typedef struct {
	uint32_t position;
	uint16_t indices[3];

	vec_t *point;
	vec_t *texcoords;
	vec_t *normal;
	vec3_t tangent;
	vec3_t bitangent;
} r_obj_vertex_t;

typedef uint32_t r_obj_triangle_t[3];

typedef struct {
	char name[MAX_QPATH];
	char material[MAX_QPATH];

	uint32_t num_tris;
} r_obj_group_t;

typedef struct {
	uint32_t num_points;
	uint32_t num_texcoords;
	uint32_t num_normals;
	uint32_t num_tris;
	uint32_t num_groups;

	vec3_t *points;
	uint32_t cur_point;
	vec2_t *texcoords;
	uint32_t cur_texcoord;
	vec3_t *normals;
	uint32_t cur_normal;
	r_obj_group_t *groups;
	uint32_t cur_group;
	r_obj_triangle_t *tris;
	uint32_t cur_tris;
	
	GArray *verts;
} r_obj_t;

#endif /* __R_LOCAL_H__ */
