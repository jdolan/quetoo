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

#ifndef __R_TYPES_H__
#define __R_TYPES_H__

#include "r_glad_core.h"

#include <SDL2/SDL_video.h>

#include "files.h"
#include "image.h"
#include "matrix.h"
#include "thread.h"

// media handles
typedef struct r_media_s {
	char name[MAX_QPATH];
	GList *dependencies;
	void (*Register)(struct r_media_s *self);
	_Bool (*Retain)(struct r_media_s *self);
	void (*Free)(struct r_media_s *self);
	int32_t seed;
} r_media_t;

typedef int16_t r_pixel_t;

// 32 bit RGBA colors
typedef union {
	struct {
		byte r, g, b, a;
	} bytes;
	uint32_t c;
} r_color_t;

// high bits OR'ed with image types
#define IT_MASK_MIPMAP	128
#define IT_MASK_FILTER	256

#define IT_MASK_TYPE	0x7F
#define IT_MASK_FLAGS	(-1 & ~IT_MASK_TYPE)

// image types
typedef enum {
	IT_NULL = 0,
	IT_PROGRAM = 1,
	IT_FONT = 2 + (IT_MASK_FILTER),
	IT_UI = 3 + (IT_MASK_FILTER),
	IT_EFFECT = 4 + (IT_MASK_MIPMAP | IT_MASK_FILTER),
	IT_DIFFUSE = 5 + (IT_MASK_MIPMAP | IT_MASK_FILTER),
	IT_LIGHTMAP = 6 + (IT_MASK_FILTER),
	IT_DELUXEMAP = 7,
	IT_NORMALMAP = 8 + (IT_MASK_MIPMAP),
	IT_SPECULARMAP = 9 + (IT_MASK_MIPMAP),
	IT_ENVMAP = 10 + (IT_MASK_MIPMAP | IT_MASK_FILTER),
	IT_FLARE = 11 + (IT_MASK_MIPMAP | IT_MASK_FILTER),
	IT_SKY = 12 + (IT_MASK_MIPMAP | IT_MASK_FILTER),
	IT_PIC = 13 + (IT_MASK_MIPMAP | IT_MASK_FILTER),
	IT_ATLAS_MAP = 14 + (IT_MASK_MIPMAP), // image is an r_atlas_t*
	IT_ATLAS_IMAGE = 15 // image is an r_atlas_image_t*
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
	R_ARRAY_POSITION,
	R_ARRAY_COLOR,
	R_ARRAY_NORMAL,
	R_ARRAY_TANGENT,
	R_ARRAY_DIFFUSE,
	R_ARRAY_LIGHTMAP,

	/**
	 * @brief These three are only used for shader-based lerp.
	 * They are only enabled if the ones that match up to it are enabled as well.
	 */
	R_ARRAY_NEXT_POSITION,
	R_ARRAY_NEXT_NORMAL,
	R_ARRAY_NEXT_TANGENT,

	R_ARRAY_MAX_ATTRIBS,
	R_ARRAY_ALL = R_ARRAY_MAX_ATTRIBS,

	/**
	 * @brief This is a special entry so that R_BindAttributeBuffer can be
	 * used for binding element buffers as well.
	 */
	R_ARRAY_ELEMENTS = -1
} r_attribute_id_t;

/**
 * @brief These are the masks used to tell which data
 * should be actually bound. They should match
 * up with the ones above to make things simple.
 */
typedef enum {
	R_ARRAY_MASK_POSITION		= (1 << R_ARRAY_POSITION),
	R_ARRAY_MASK_COLOR			= (1 << R_ARRAY_COLOR),
	R_ARRAY_MASK_NORMAL			= (1 << R_ARRAY_NORMAL),
	R_ARRAY_MASK_TANGENT		= (1 << R_ARRAY_TANGENT),
	R_ARRAY_MASK_DIFFUSE		= (1 << R_ARRAY_DIFFUSE),
	R_ARRAY_MASK_LIGHTMAP		= (1 << R_ARRAY_LIGHTMAP),

	R_ARRAY_MASK_NEXT_POSITION	= (1 << R_ARRAY_NEXT_POSITION),
	R_ARRAY_MASK_NEXT_NORMAL	= (1 << R_ARRAY_NEXT_NORMAL),
	R_ARRAY_MASK_NEXT_TANGENT	= (1 << R_ARRAY_NEXT_TANGENT),

	R_ARRAY_MASK_ALL			= (1 << R_ARRAY_MAX_ATTRIBS) - 1
} r_attribute_mask_t;

/**
 * @brief Types that can be used in buffers for attributes.
 */
typedef enum {
	R_ATTRIB_FLOAT,
	R_ATTRIB_BYTE,
	R_ATTRIB_UNSIGNED_BYTE,
	R_ATTRIB_SHORT,
	R_ATTRIB_UNSIGNED_SHORT,
	R_ATTRIB_INT,
	R_ATTRIB_UNSIGNED_INT,

	R_ATTRIB_TOTAL_TYPES
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
		uint32_t type : 3;
		uint32_t count : 3;
		uint32_t offset : 6;
		uint32_t normalized : 2;
		uint32_t stride : 6;
	};
} r_attrib_type_state_t;

/**
 * @brief Represents a single attribute layout for a buffer.
 * An interleaved buffer will supply a chain of these.
 * The last entry should have an 'attribute' of -1.
 */
typedef struct {
	r_attribute_id_t attribute;

	r_attrib_type_t type;
	uint8_t count;
	uint8_t offset;
	uint8_t size;
	_Bool normalized;

	// internal, no touch
	r_attrib_type_state_t _type_state;
} r_buffer_layout_t;

/**
 * @brief Buffers are used to hold data for the renderer.
 */
typedef struct r_buffer_s {
	r_buffer_type_t type; // R_BUFFER_DATA or R_BUFFER_ELEMENT
	GLenum hint; // GL_x_y, where x is STATIC or DYNAMIC, and where y is DRAW, READ or COPY
	GLenum target; // GL_ARRAY_BUFFER or GL_ELEMENT_ARRAY_BUFFER; mapped from above var
	GLuint bufnum; // e.g. 123
	size_t size; // last size of buffer, for resize operations
	const char *func; // location of allocation

	// attribute crap
	r_attrib_type_state_t element_type;
	_Bool interleave; // whether this buffer is an interleave buffer. Only valid for R_BUFFER_DATA.
	r_attribute_mask_t attrib_mask;
	const r_buffer_layout_t *interleave_attribs[R_ARRAY_MAX_ATTRIBS];
} r_buffer_t;

/**
 * @brief Images are referenced by materials, models, entities, particles, etc.
 */
typedef struct {
	r_media_t media;
	r_image_type_t type;
	r_pixel_t width, height; // image dimensions
	GLuint texnum; // OpenGL texture binding
	vec3_t color; // average color
} r_image_t;

/**
 * @brief This is a proxy structure that allows an atlased piece of texture to be
 * used in places that expect r_image_t's.
 */
typedef struct {
	r_image_t image; // this allows the individual atlas piece to be used as an image

	const r_image_t *input_image; // image ptr
	u16vec2_t position; // position in pixels
	vec4_t texcoords; // position in texcoords
	r_color_t *scratch; // scratch space for image
} r_atlas_image_t;

/**
 * @brief An atlas is composed of multiple images stitched together to make
 * a single image. It is a sub-type of r_image_t.
 */
typedef struct {
	r_image_t image;
	GArray *images; // image list
	GHashTable *hash; // hash of image -> image list ptr
} r_atlas_t;

typedef enum {
	PARTICLE_NORMAL,
	PARTICLE_SPARK,
	PARTICLE_ROLL,
	PARTICLE_DECAL,
	PARTICLE_BUBBLE,
	PARTICLE_BEAM,
	PARTICLE_WEATHER,
	PARTICLE_SPLASH,
	PARTICLE_CORONA,
	PARTICLE_FLARE
} r_particle_type_t;

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
} r_particle_t;

#define MAX_PARTICLES		16384

typedef struct {
	GLenum src, dest;
} r_stage_blend_t;

typedef struct {
	vec_t hz, dhz;
} r_stage_pulse_t;

typedef struct {
	vec_t hz, dhz;
	vec_t amp, damp;
} r_stage_stretch_t;

typedef struct {
	vec_t hz, deg;
} r_stage_rotate_t;

typedef struct {
	vec_t s, t;
	vec_t ds, dt;
} r_stage_scroll_t;

typedef struct {
	vec_t s, t;
} r_stage_scale_t;

typedef struct {
	vec_t floor, ceil;
	vec_t height;
} r_stage_terrain_t;

typedef struct {
	vec_t intensity;
} r_stage_dirt_t;

// frame based material animation, lerp between consecutive images
typedef struct {
	uint16_t num_frames;
	r_image_t **frames;
	vec_t fps;
	uint32_t dtime;
	uint16_t dframe;
} r_stage_anim_t;

typedef struct r_stage_s {
	uint32_t flags;
	r_image_t *image;
	struct r_material_s *material;
	r_stage_blend_t blend;
	vec3_t color;
	r_stage_pulse_t pulse;
	r_stage_stretch_t stretch;
	r_stage_rotate_t rotate;
	r_stage_scroll_t scroll;
	r_stage_scale_t scale;
	r_stage_terrain_t terrain;
	r_stage_dirt_t dirt;
	r_stage_anim_t anim;
	struct r_stage_s *next;
} r_stage_t;

#define DEFAULT_BUMP 1.0
#define DEFAULT_PARALLAX 1.0
#define DEFAULT_HARDNESS 1.0
#define DEFAULT_SPECULAR 1.0

typedef struct r_material_s {
	r_media_t media;
	r_image_t *diffuse;
	r_image_t *normalmap;
	r_image_t *specularmap;
	uint32_t flags;
	uint32_t time;
	vec_t bump;
	vec_t parallax;
	vec_t hardness;
	vec_t specular;
	r_stage_t *stages;
	uint16_t num_stages;
} r_material_t;

// bsp model memory representation
typedef struct {
	vec3_t mins, maxs;
	vec3_t origin; // for sounds or lights
	vec_t radius;
	int32_t head_node;
	uint16_t first_surface, num_surfaces;
} r_bsp_inline_model_t;

/**
 * @brief Resolves a unique(ish) stencil reference value for the given plane.
 */
#define R_STENCIL_REF(p) (((p)->num % 0xff) + 1)

typedef cm_bsp_plane_t r_bsp_plane_t;

typedef struct {
	uint16_t v[2];
} r_bsp_edge_t;

typedef struct {
	char name[32];
	vec_t vecs[2][4];
	uint32_t flags;
	int32_t value;
	r_material_t *material;
	vec3_t emissive;
	vec_t light;
} r_bsp_texinfo_t;

typedef struct {
	vec_t radius;
	uint32_t time;
	vec_t alpha;

	r_particle_t particle;
} r_bsp_flare_t;

// r_bsp_surface_t flags
#define R_SURF_PLANE_BACK	1
#define R_SURF_LIGHTMAP		2

typedef struct {
	int16_t vis_frame; // PVS frame
	int16_t frame; // renderer frame
	int16_t back_frame; // back-facing renderer frame
	int16_t light_frame; // dynamic lighting frame
	uint64_t light_mask; // bit mask of dynamic light sources

	r_bsp_plane_t *plane;
	uint16_t flags; // R_SURF flags

	int32_t first_edge; // look up in model->surf_edges, negative numbers
	uint16_t num_edges; // are backwards edges

	vec3_t mins;
	vec3_t maxs;
	vec3_t center;
	vec3_t normal;
	vec_t area;

	vec2_t st_mins;
	vec2_t st_maxs;
	vec2_t st_center;
	vec2_t st_extents;

	GLuint index; // index into element buffer
	GLuint *elements; // elements unique to this surf

	r_bsp_texinfo_t *texinfo; // SURF_ flags

	r_bsp_flare_t *flare;

	r_pixel_t lightmap_s, lightmap_t; // lightmap texture coords
	r_image_t *lightmap;
	r_image_t *deluxemap;

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
	r_bsp_surfaces_t back;
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
	r_bsp_plane_t *plane;
	struct r_bsp_node_s *children[2];

	uint16_t first_surface;
	uint16_t num_surfaces;
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
	int16_t cluster;
	int16_t area;

	r_bsp_surface_t **first_leaf_surface;
	uint16_t num_leaf_surfaces;
} r_bsp_leaf_t;

/**
 * @brief
 */
typedef struct {
	int16_t vis_frame; // PVS eligibility
} r_bsp_cluster_t;

/**
 * @brief BSP lightmap parameters.
 */
typedef struct {
	uint16_t scale;
	uint32_t size;
	byte *data;
} r_bsp_lightmaps_t;

/**
 * @brief BSP light sources.
 */
typedef struct {
	const r_bsp_leaf_t *leaf;
	uint16_t count;

	/**
	 * @see r_light_t
	 */
	struct {
		vec3_t origin;
		vec3_t color;
		vec_t radius;
	} light;

	r_particle_t debug;

} r_bsp_light_t;

// md3 model memory representation
typedef struct {
	vec3_t point;
	vec3_t normal;
	vec4_t tangent;
} r_md3_vertex_t;

typedef struct {
	char name[MD3_MAX_PATH];
	matrix4x4_t matrix;
} r_md3_tag_t;

typedef struct {
	char id[4];

	char name[MD3_MAX_PATH];

	int32_t flags;

	uint16_t num_skins;

	uint16_t num_verts;
	r_md3_vertex_t *verts;
	d_md3_texcoord_t *coords;

	uint16_t num_tris;
	uint32_t *tris;

	uint32_t num_elements;
} r_md3_mesh_t;

typedef struct {
	uint16_t first_frame;
	uint16_t num_frames;
	uint16_t looped_frames;
	uint16_t hz;
} r_md3_animation_t;

/**
 * @brief Quake3 (MD3) model in-memory representation.
 */
typedef struct {
	int32_t id;
	int32_t version;

	char file_name[MD3_MAX_PATH];

	int32_t flags;

	uint16_t num_frames;
	d_md3_frame_t *frames;

	uint16_t num_tags;
	r_md3_tag_t *tags;

	uint16_t num_meshes;
	r_md3_mesh_t *meshes;

	uint16_t num_animations;
	r_md3_animation_t *animations;
} r_md3_t;

typedef struct {
	uint32_t position;
	uint16_t indices[3];
	vec_t *point;
	vec_t *texcoords;
	vec_t *normal;
	vec_t *tangent;
} r_obj_vertex_t;

typedef struct {
	r_obj_vertex_t *verts[3];
} r_obj_triangle_t;

/*
 * brief Object (OBJ) model in-memory representation.
 */
typedef struct {
	GList *points;
	GList *texcoords;
	GList *normals;

	GList *verts;
	GList *tris;

	GList *tangents;
} r_obj_t;

// shared structure for all model types
typedef enum {
	MOD_BAD,
	MOD_BSP,
	MOD_BSP_INLINE,
	MOD_MD3,
	MOD_OBJ
} r_model_type_t;

typedef struct {
	int32_t version;

	uint16_t num_inline_models;
	r_bsp_inline_model_t *inline_models;

	uint16_t num_planes;
	r_bsp_plane_t *planes;

	uint16_t num_leafs;
	r_bsp_leaf_t *leafs;

	uint32_t num_edges;
	r_bsp_edge_t *edges;

	uint16_t num_nodes;
	r_bsp_node_t *nodes;

	uint16_t num_texinfo;
	r_bsp_texinfo_t *texinfo;

	uint16_t num_surfaces;
	r_bsp_surface_t *surfaces;

	uint32_t num_surface_edges;
	int32_t *surface_edges;

	uint16_t num_leaf_surfaces;
	r_bsp_surface_t **leaf_surfaces;

	uint16_t num_clusters;
	r_bsp_cluster_t *clusters;

	r_bsp_lightmaps_t *lightmaps;

	uint16_t num_bsp_lights;
	r_bsp_light_t *bsp_lights;

	// sorted surfaces arrays
	r_sorted_bsp_surfaces_t *sorted_surfaces;

	// vertex arrays, for materials
	vec3_t *verts;
	vec2_t *texcoords;
	vec2_t *lightmap_texcoords;
	vec3_t *normals;
	vec4_t *tangents;

	// buffers
	r_buffer_t vertex_buffer;
	r_buffer_t element_buffer;

	// active shadow counts, indexed by plane number
	uint16_t *plane_shadows;
} r_bsp_model_t;

/**
 * @brief Provides load-time normalization of mesh models.
 */
typedef struct {
	vec3_t translate;
	vec_t scale;
	uint32_t flags; // EF_ALPHA_TEST, etc..
} r_mesh_config_t;

typedef struct {
	uint16_t num_frames;
	uint32_t flags;

	r_material_t *material;

	r_mesh_config_t *world_config;
	r_mesh_config_t *view_config;
	r_mesh_config_t *link_config;

	// buffer data
	r_buffer_t vertex_buffer;
	r_buffer_t element_buffer;

	// animated models use a separate texcoord buffer
	r_buffer_t texcoord_buffer;
	
	r_buffer_t shell_vertex_buffer;
	r_buffer_t shell_element_buffer;

	void *data; // raw model data (r_md3_t, r_obj_t, ..)
} r_mesh_model_t;

/**
 * @brief Models represent a subset of the BSP or an OBJ / MD3 mesh.
 */
typedef struct r_model_s {
	r_media_t media;
	r_model_type_t type;

	r_bsp_model_t *bsp;
	r_bsp_inline_model_t *bsp_inline;
	r_mesh_model_t *mesh;

	vec3_t mins, maxs;
	vec_t radius;

	GLsizei num_verts; // raw vertex primitive count, used to build arrays
	GLsizei num_elements; // number of vertex elements, if element_buffer is to be used
	GLsizei num_tris; // cached num_tris amount
} r_model_t;

#define IS_BSP_MODEL(m) (m && m->bsp)
#define IS_MESH_MODEL(m) (m && m->mesh)
#define IS_BSP_INLINE_MODEL(m) (m && m->bsp_inline)

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
	r_bsp_plane_t plane;
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
#define MAX_ILLUMINATIONS_PER_SHADOW (MAX_ILLUMINATIONS / 2)
#define MAX_PLANES_PER_SHADOW 3
#define MAX_SHADOWS (MAX_ILLUMINATIONS_PER_SHADOW * MAX_PLANES_PER_SHADOW)

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

	const struct r_entity_s *parent; // for linked models
	const char *tag_name;

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

// matrix indexes
typedef enum {
	R_MATRIX_PROJECTION,
	R_MATRIX_MODELVIEW,
	R_MATRIX_TEXTURE,
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

	matrix4x4_t matrix; // the base modelview matrix
	matrix4x4_t inverse_matrix;

	matrix4x4_t active_matrices[R_MATRIX_TOTAL];

	uint32_t contents; // view origin contents mask
	vec_t bob;

	uint32_t time;

	byte *area_bits; // if not NULL, only areas with set bits will be drawn

	r_plugin_t plugin; // active renderer plugin

	byte weather; // weather effects
	vec3_t fog_color;

	uint16_t num_entities;
	r_entity_t entities[MAX_ENTITIES];

	uint16_t num_particles;
	r_particle_t particles[MAX_PARTICLES];

	uint16_t num_lights;
	r_light_t lights[MAX_LIGHTS];

	r_sustained_light_t sustained_lights[MAX_LIGHTS];

	thread_t *thread; // client thread which populates view

	const r_entity_t *current_entity; // entity being rendered
	const r_shadow_t *current_shadow; // shadow being rendered
	vec4_t current_shadow_light, current_shadow_plane;

	// counters, reset each frame

	uint32_t num_bind_texture;
	uint32_t num_bind_lightmap;
	uint32_t num_bind_deluxemap;
	uint32_t num_bind_normalmap;
	uint32_t num_bind_specularmap;

	uint32_t num_bsp_clusters;
	uint32_t num_bsp_leafs;
	uint32_t num_bsp_surfaces;

	uint32_t num_mesh_models;
	uint32_t num_mesh_tris;

	uint32_t cull_passes;
	uint32_t cull_fails;

	uint32_t num_state_changes[R_STATE_TOTAL];
	uint32_t num_buffer_full_uploads, num_buffer_partial_uploads, size_buffer_uploads;

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
	r_pixel_t render_width, render_height;

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

#endif /* __R_TYPES_H__ */
