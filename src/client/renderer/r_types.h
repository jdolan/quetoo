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
	MEDIA_FRAMEBUFFER, // r_framebuffer_t

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
	IT_NULL = (1 << 0),
	IT_PROGRAM = (1 << 1),
	IT_FONT = (1 << 2) + (IT_MASK_FILTER),
	IT_UI = (1 << 3) + (IT_MASK_FILTER),
	IT_EFFECT = (1 << 4) + (IT_MASK_MIPMAP | IT_MASK_FILTER),
	IT_DIFFUSE = (1 << 5) + (IT_MASK_MIPMAP | IT_MASK_FILTER),
	IT_LIGHTMAP = (1 << 6) + (IT_MASK_FILTER),
	IT_NORMALMAP = (1 << 7) + (IT_MASK_MIPMAP),
	IT_SPECULARMAP = (1 << 8) + (IT_MASK_MIPMAP),
	IT_ENVMAP = (1 << 9) + (IT_MASK_MIPMAP | IT_MASK_FILTER),
	IT_FLARE = (1 << 10) + (IT_MASK_MIPMAP | IT_MASK_FILTER | IT_MASK_MULTIPLY),
	IT_SKY = (1 << 11) + (IT_MASK_MIPMAP | IT_MASK_FILTER),
	IT_PIC = (1 << 12) + (IT_MASK_MIPMAP | IT_MASK_FILTER),
	IT_ATLAS = (1 << 13) + (IT_MASK_MIPMAP),
	IT_STAINMAP = (1 << 14),
	IT_TINTMAP = (1 << 15) + (IT_MASK_MIPMAP | IT_MASK_FILTER)
} r_image_type_t;

/**
 * @brief Buffer types
 */
typedef enum {
	R_BUFFER_DATA,
	R_BUFFER_ELEMENT,

	R_NUM_BUFFERS,

	// buffer flags, not stored in type
	R_BUFFER_INTERLEAVE = 4,
	R_BUFFER_TYPE_MASK = 0x03
} r_buffer_type_t;

/**
 * @brief Attribute indices - these should be assigned to
 * every program that uses them, and are also used for buffer storage.
 */
typedef enum {
	R_ATTRIB_POSITION,
	R_ATTRIB_COLOR,
	R_ATTRIB_NORMAL,
	R_ATTRIB_TANGENT,
	R_ATTRIB_BITANGENT,
	R_ATTRIB_DIFFUSE,
	R_ATTRIB_LIGHTMAP,

	/**
	 * @brief These are only used for shader-based lerp. They are only enabled if
	 * the ones that match up to it are enabled as well.
	 */
	R_ATTRIB_NEXT_POSITION,
	R_ATTRIB_NEXT_NORMAL,
	R_ATTRIB_NEXT_TANGENT,
	R_ATTRIB_NEXT_BITANGENT,

	/**
	 * @brief Geometry shader parameters
	 */
	R_ATTRIB_SCALE,
	R_ATTRIB_ROLL,
	R_ATTRIB_END,
	R_ATTRIB_TYPE,

	R_ATTRIB_ALL,

	/**
	 * @brief This is a special entry so that R_BindAttributeBuffer can be
	 * used for binding element buffers as well.
	 */
	R_ATTRIB_ELEMENTS = -1
} r_attribute_id_t;

/**
 * @brief These are the masks used to tell which data
 * should be actually bound. They should match
 * up with the ones above to make things simple.
 */
typedef enum {
	R_ATTRIB_MASK_POSITION       = (1 << R_ATTRIB_POSITION),
	R_ATTRIB_MASK_COLOR          = (1 << R_ATTRIB_COLOR),
	R_ATTRIB_MASK_NORMAL         = (1 << R_ATTRIB_NORMAL),
	R_ATTRIB_MASK_TANGENT        = (1 << R_ATTRIB_TANGENT),
	R_ATTRIB_MASK_BITANGENT      = (1 << R_ATTRIB_BITANGENT),
	R_ATTRIB_MASK_DIFFUSE        = (1 << R_ATTRIB_DIFFUSE),
	R_ATTRIB_MASK_LIGHTMAP       = (1 << R_ATTRIB_LIGHTMAP),

	R_ATTRIB_MASK_NEXT_POSITION  = (1 << R_ATTRIB_NEXT_POSITION),
	R_ATTRIB_MASK_NEXT_NORMAL    = (1 << R_ATTRIB_NEXT_NORMAL),
	R_ATTRIB_MASK_NEXT_TANGENT   = (1 << R_ATTRIB_NEXT_TANGENT),
	R_ATTRIB_MASK_NEXT_BITANGENT = (1 << R_ATTRIB_NEXT_BITANGENT),

	R_ATTRIB_MASK_SCALE          = (1 << R_ATTRIB_SCALE),
	R_ATTRIB_MASK_ROLL           = (1 << R_ATTRIB_ROLL),
	R_ATTRIB_MASK_END            = (1 << R_ATTRIB_END),
	R_ATTRIB_MASK_TYPE           = (1 << R_ATTRIB_TYPE),

	R_ATTRIB_MASK_ALL            = (1 << R_ATTRIB_ALL) - 1,
	R_ATTRIB_GEOMETRY_MASK       = R_ATTRIB_MASK_SCALE | R_ATTRIB_MASK_ROLL | R_ATTRIB_MASK_END | R_ATTRIB_MASK_TYPE
} r_attribute_mask_t;

/**
 * @brief Types that can be used in buffers for attributes.
 */
typedef enum {
	R_TYPE_FLOAT,
	R_TYPE_BYTE,
	R_TYPE_UNSIGNED_BYTE,
	R_TYPE_SHORT,
	R_TYPE_UNSIGNED_SHORT,
	R_TYPE_INT,
	R_TYPE_UNSIGNED_INT,

	R_TYPE_INTERLEAVE // special value to denote interleave buffers
} r_attrib_type_t;

/**
 * @brief A structure packing a bunch of attribute state together
 * for quick comparison of a single variable.
 *
 * @note if any of the enums or data types used here change,
 * be sure to modify the bit field too.
 */
typedef union {
	uint32_t packed;

	struct {
		uint32_t normalized : 1; // bool
		uint32_t integer : 1; // bool
		uint32_t type : 3; // r_attrib_type_t, max 7
		uint32_t count : 3; // # of elements, max 7
		uint32_t offset : 12; // offset in bytes, max 4095
		uint32_t stride : 12; // offset in bytes, max 4095
	};
} r_attrib_type_state_t;

/**
 * @brief Represents a single attribute layout for a buffer.
 * An interleaved buffer will supply a chain of these.
 * The last entry should have an 'attribute' of -1.
 */
typedef struct {
	r_attrib_type_t type;
	uint8_t count;
	_Bool normalized;
	_Bool integer;

	// only required for interleave buffers
	r_attribute_id_t attribute;

	// internal, no touch
	GLenum gl_type;
	r_attrib_type_state_t _type_state;
} r_buffer_layout_t;

/**
 * @brief Structure that holds construction arguments for buffers
 */
typedef struct {
	r_buffer_type_t type;
	GLenum hint;
	r_buffer_layout_t element;
	size_t size;
	const void *data;
} r_create_buffer_t;

/**
 * @brief Structure that holds construction arguments for element buffers
 */
typedef struct {
	r_attrib_type_t type;
	GLenum hint;
	size_t size;
	const void *data;
} r_create_element_t;

/**
 * @brief Structure that holds construction arguments for interleave buffers
 */
typedef struct {
	GLubyte struct_size;
	r_buffer_layout_t *layout;
	GLenum hint;
	size_t size;
	const void *data;
} r_create_interleave_t;

/**
 * @brief Buffers are used to hold data for the renderer.
 */
typedef struct r_buffer_s {
	r_buffer_type_t type; // R_BUFFER_DATA or R_BUFFER_ELEMENT
	GLenum hint; // GL_x_y, where x is STATIC or DYNAMIC, and where y is DRAW, READ or COPY
	GLenum target; // GL_ARRAY_BUFFER or GL_ELEMENT_ARRAY_BUFFER; mapped from above var
	GLuint bufnum; // e.g. 123
	size_t size; // last size of buffer, for resize operations

	// attribute crap
	r_attrib_type_state_t element_type;
	GLenum element_gl_type;
	r_attribute_mask_t attrib_mask;
	const r_buffer_layout_t *interleave_attribs[R_ATTRIB_ALL];
	_Bool interleave; // whether this buffer is an interleave buffer. Only valid for R_BUFFER_DATA.
} r_buffer_t;

/**
 * @brief Images are referenced by materials, models, entities, particles, etc.
 */
typedef struct {
	r_media_t media;
	r_image_type_t type;
	r_pixel_t width, height, layers; // image dimensions
	GLuint texnum; // OpenGL texture binding
	vec3_t color; // average color
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
 * @brief This is a proxy structure that allows an atlased image to be used in places
*  that expect r_image_t's.
 */
typedef struct {
	r_image_t image;
	atlas_node_t *node;
	vec4_t texcoords;
} r_atlas_image_t;

/**
 * @brief A framebuffer is a screen buffer that can be drawn to.
 */
typedef struct {
	r_media_t media;
	GLuint framebuffer;

	r_image_t *color; // the texture color attachment
	r_image_type_t color_type; // since color might be freed
	GLuint depth_stencil;

	uint32_t width, height; // matches color attachment
} r_framebuffer_t;

typedef enum {
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
	const r_image_t *image;
	GLenum blend;
	vec4_t color;
	vec_t scale;
	vec_t scroll_s;
	vec_t scroll_t;
	vec_t roll;
	vec3_t org;
	vec3_t end;
	vec3_t dir;
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
	r_image_t *specularmap;
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
	int32_t first_surface, num_surfaces;
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

// r_bsp_surface_t flags
#define R_SURF_BACK_SIDE	0x1
#define R_SURF_IN_LIQUID	0x2

/**
 * @brief BSP lightmaps are atlas images of indivudual surface lightmaps.
 */
typedef struct {

	/**
	 * @brief The atlased lightmaps and deluxemaps, in a layered texture.
	 */
	r_image_t *lightmaps;

	/**
	 * @brief The atlased stainmaps, which are backed by the framebuffer.
	 */
	r_image_t *stainmaps;

	/**
	 * @brief The framebuffer backing the stainmaps.
	 */
	r_framebuffer_t *framebuffer;

	/**
	 * @brief The projection matrix into the stainmap framebuffer.
	 */
	matrix4x4_t projection;
} r_bsp_lightmap_t;

/**
 * @brief Each indivudual surface lightmap has a projection matrix.
 */
typedef struct {
	r_bsp_lightmap_t *atlas; // the lightmap atlas containing this lightmap

	r_pixel_t s, t; // the texture coordinates into the atlas image
	r_pixel_t w, h;

	vec2_t st_mins, st_maxs;

} r_bsp_surface_lightmap_t;

typedef struct {
	cm_bsp_plane_t *plane;
	r_bsp_texinfo_t *texinfo;
	r_bsp_flare_t *flare;

	r_bsp_surface_lightmap_t lightmap;

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
} r_bsp_surface_t;

/**
 * @brief Surfaces are assigned to arrays based on their render path and then
 * ordered by material to reduce glBindTexture calls.
 */
typedef struct {
	r_bsp_surface_t **surfaces;
	size_t count;
} r_bsp_surfaces_t;

/**
 * @brief The world model maintains master lists of all surfaces, partitioned
 * by render path and ordered by material.
 */
typedef struct {
	r_bsp_surfaces_t sky;
	r_bsp_surfaces_t opaque;
	r_bsp_surfaces_t opaque_warp;
	r_bsp_surfaces_t alpha_test;
	r_bsp_surfaces_t blend;
	r_bsp_surfaces_t blend_warp;
	r_bsp_surfaces_t material;
	r_bsp_surfaces_t flare;
} r_sorted_bsp_surfaces_t;

/**
 * @brief Appends a reference to the specified face to the given surfaces list.
 */
#define R_SURFACE_TO_SURFACES(surfs, surf) (surfs)->surfaces[(surfs)->count++] = surf

/**
 * @brief Function prototype for BSP surfaces draw lists.
 */
typedef void (*BspSurfacesDrawFunc)(const r_bsp_surfaces_t *surfs);

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

	int32_t first_surface;
	int32_t num_surfaces;
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

	r_bsp_surface_t **first_leaf_surface;
	int32_t num_leaf_surfaces;
} r_bsp_leaf_t;

/**
 * @brief BSP vertexes include position, normal, tangent and bitangent. Because
 * lightmap texture coordinates are unique to each face, they are not calculated
 * until the renderer loads them.
 */
typedef struct {
	vec3_t position;
	vec3_t normal;
	vec3_t tangent;
	vec3_t bitangent;
	vec2_t diffuse;
	vec2_t lightmap;
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
	vec3_t point;
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
	bsp_file_t *file;

	int32_t num_texinfo;
	r_bsp_texinfo_t *texinfo;

	int32_t num_nodes;
	r_bsp_node_t *nodes;

	int32_t num_leafs;
	r_bsp_leaf_t *leafs;

	int32_t num_leaf_surfaces;
	r_bsp_surface_t **leaf_surfaces;

	int32_t num_vertexes;
	r_bsp_vertex_t *vertexes;

	int32_t num_elements;
	int32_t *elements;

	int32_t num_lightmaps;
	r_bsp_lightmap_t *lightmaps;

	int32_t num_surfaces;
	r_bsp_surface_t *surfaces;

	int32_t num_inline_models;
	r_bsp_inline_model_t *inline_models;

	int32_t num_clusters;
	r_bsp_cluster_t *clusters;

	int16_t luxel_size;

	int32_t num_lights;
	r_bsp_light_t *lights;

	// sorted surfaces arrays
	r_sorted_bsp_surfaces_t *sorted_surfaces;

	// vertex buffer
	r_buffer_t vertex_buffer;

	// element buffer
	r_buffer_t element_buffer;

	// an array of shadow counts, indexed by plane number
	int32_t *plane_shadows;
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
	r_buffer_t vertex_buffer;
	r_buffer_t element_buffer;

	// animated models use a separate texcoord buffer
	r_buffer_t texcoord_buffer;

	r_buffer_t shell_vertex_buffer;
	r_buffer_t shell_element_buffer;
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

	GLsizei num_verts; // raw vertex primitive count, used to build arrays
	GLsizei num_elements; // number of vertex elements, if element_buffer is to be used
	GLsizei num_tris; // cached num_tris amount
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
	const r_image_t *image;
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

/**
 * @brief Renderer element types.
 */
typedef enum {
	ELEMENT_NONE,
	ELEMENT_BSP_SURFACE_BLEND,
	ELEMENT_BSP_SURFACE_BLEND_WARP,
	ELEMENT_ENTITY,
	ELEMENT_PARTICLE
} r_element_type_t;

/**
 * @brief Element abstraction to allow sorting of mixed draw lists,
 * asynchronous rendering via commands, etc.
 */
typedef struct {
	const void *element;
	const vec_t *origin;
	void *data;
	vec_t depth; // resolved for all elements
	r_element_type_t type;
} r_element_t;

/**
 * @brief Allows alternate renderer plugins to be dropped in.
 */
typedef enum {
	R_PLUGIN_DEFAULT
} r_plugin_t;

#define WEATHER_NONE		0
#define WEATHER_RAIN 		1
#define WEATHER_SNOW 		2
#define WEATHER_FOG 		4
#define WEATHER_PRECIP_MASK	(WEATHER_RAIN | WEATHER_SNOW)

#define FOG_START			128.0
#define FOG_END				2048.0

// program index
typedef enum {
	R_PROGRAM_DEFAULT,
	R_PROGRAM_NULL,
	R_PROGRAM_PARTICLE,
	R_PROGRAM_SHADOW,
	R_PROGRAM_SHELL,
	R_PROGRAM_STAIN,
	R_PROGRAM_WARP,

	R_PROGRAM_TOTAL
} r_program_id_t;

// matrix index
typedef enum {
	R_MATRIX_PROJECTION,
	R_MATRIX_MODELVIEW,
	R_MATRIX_SHADOW,

	R_MATRIX_TOTAL
} r_matrix_id_t;

// texunit IDs
typedef enum {
	R_TEXUNIT_DIFFUSE,
	R_TEXUNIT_LIGHTMAP,
	R_TEXUNIT_DELUXEMAP,
	R_TEXUNIT_NORMALMAP,
	R_TEXUNIT_SPECULARMAP,
	R_TEXUNIT_WARP,
	R_TEXUNIT_TINTMAP,
	R_TEXUNIT_STAINMAP,

	R_TEXUNIT_TOTAL
} r_texunit_id_t;

typedef enum {
	R_STATE_PROGRAM,
	R_STATE_ACTIVE_TEXTURE,
	R_STATE_BIND_TEXTURE,
	R_STATE_BIND_BUFFER,
	R_STATE_BLEND_FUNC,
	R_STATE_ENABLE_BLEND,
	R_STATE_DEPTHMASK,
	R_STATE_ENABLE_STENCIL,
	R_STATE_STENCIL_OP,
	R_STATE_STENCIL_FUNC,
	R_STATE_POLYGON_OFFSET,
	R_STATE_ENABLE_POLYGON_OFFSET,
	R_STATE_VIEWPORT,
	R_STATE_ENABLE_DEPTH_TEST,
	R_STATE_DEPTH_RANGE,
	R_STATE_ENABLE_SCISSOR,
	R_STATE_SCISSOR,
	R_STATE_PROGRAM_PARAMETER,
	R_STATE_PROGRAM_ATTRIB_POINTER,
	R_STATE_PROGRAM_ATTRIB_CONSTANT,
	R_STATE_PROGRAM_ATTRIB_TOGGLE,

	R_STATE_TOTAL
} r_state_id_t;

/**
 * @brief
 */
typedef struct {
	uint32_t bound;
	uint32_t num_full_uploads;
	uint32_t num_partial_uploads;
	size_t size_uploaded;
} r_buffer_stats_t;

/**
 * @brief Provides read-write visibility and scene management to the client.
 */
typedef struct {
	SDL_Rect viewport, viewport_3d;
	vec2_t fov;

	vec3_t origin; // client's view origin, angles, and vectors
	vec3_t angles;
	vec3_t forward;
	vec3_t right;
	vec3_t up;

	matrix4x4_t matrix_base_3d; // base projection matrices
	matrix4x4_t matrix_base_2d;
	matrix4x4_t matrix_base_ui;

	matrix4x4_t matrix; // the base modelview matrix
	matrix4x4_t inverse_matrix;

	int32_t contents; // view origin contents mask
	vec_t bob;

	uint32_t ticks; // unclamped simulation time

	byte *area_bits; // if not NULL, only areas with set bits will be drawn

	r_plugin_t plugin; // active renderer plugin

	byte weather; // weather effects
	vec4_t fog_color;

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
	uint32_t num_bsp_surfaces;

	uint32_t num_mesh_models;
	uint32_t num_mesh_tris;

	uint32_t cull_passes;
	uint32_t cull_fails;

	uint32_t num_state_changes[R_STATE_TOTAL];
	uint32_t num_binds[R_TEXUNIT_TOTAL];
	r_buffer_stats_t buffer_stats[R_NUM_BUFFERS];

	uint32_t num_draw_elements, num_draw_element_count;
	uint32_t num_draw_arrays, num_draw_array_count;

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
	 * @brief Actual OpenGL size being rendered after supersampling
	 */
	uint32_t render_width, render_height;

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
