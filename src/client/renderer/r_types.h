/*
 * Copyright(c) 1997-2001 Id Software, Inc.
 * Copyright(c) 2002 The Quakeforge Project.
 * Copyright(c) 2006 Quake2World.
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

#include <SDL/SDL_opengl.h>

#include "files.h"
#include "threads.h"
#include "r_matrix.h"

typedef int16_t r_pixel_t;

typedef struct r_stage_blend_s {
	GLenum src, dest;
} r_stage_blend_t;

typedef struct r_stage_pulse_s {
	float hz, dhz;
} r_stage_pulse_t;

typedef struct r_stage_stretch_s {
	float hz, dhz;
	float amp, damp;
} r_stage_stretch_t;

typedef struct r_stage_rotate_s {
	float hz, deg;
} r_stage_rotate_t;

typedef struct r_stage_scroll_s {
	float s, t;
	float ds, dt;
} r_stage_scroll_t;

typedef struct r_stage_scale_s {
	float s, t;
} r_stage_scale_t;

typedef struct r_stage_terrain_s {
	float floor, ceil;
	float height;
} r_stage_terrain_t;

typedef struct r_stage_dirt_s {
	float intensity;
} r_stage_dirt_t;

// frame based material animation, lerp between consecutive images
#define MAX_ANIM_FRAMES 8

typedef struct r_stage_anim_s {
	uint16_t num_frames;
	struct r_image_s *images[MAX_ANIM_FRAMES];
	float fps;
	float dtime;
	uint16_t dframe;
} r_stage_anim_t;

typedef struct r_stage_s {
	uint32_t flags;
	struct r_image_s *image;
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
	uint32_t flags;
	float time;
	float bump;
	float parallax;
	float hardness;
	float specular;
	r_stage_t *stages;
	uint16_t num_stages;
} r_material_t;

// image types
typedef enum {
	it_null,
	it_font,
	it_effect,
	it_diffuse,
	it_lightmap,
	it_deluxemap,
	it_normalmap,
	it_glossmap,
	it_material,
	it_sky,
	it_pic
} r_image_type_t;

// images
typedef struct r_image_s {
	char name[MAX_QPATH]; // game path, excluding extension
	r_image_type_t type;
	r_pixel_t width, height; // image dimensions
	GLuint texnum; // gl texture binding
	r_material_t material; // material definition
	vec3_t color; // average color
	struct r_image_s *normalmap;
	struct r_image_s *glossmap;
} r_image_t;

// bsp model memory representation
typedef struct {
	vec3_t position;
	vec3_t normal;
	vec4_t color;
	uint16_t surfaces;
} r_bsp_vertex_t;

typedef struct {
	vec3_t mins, maxs;
	vec3_t origin; // for sounds or lights
	float radius;
	int32_t head_node;
	uint16_t first_face, num_faces;
} r_bsp_submodel_t;

typedef struct {
	uint16_t v[2];
} r_bsp_edge_t;

typedef struct {
	float vecs[2][4];
	uint32_t flags;
	int32_t value;
	char name[32];
	r_image_t *image;
} r_bsp_texinfo_t;

typedef struct {
	vec3_t origin;
	float radius;
	const r_image_t *image;
	vec3_t color;
	float time;
	float alpha;
} r_bsp_flare_t;

// r_bsp_surface_t flags
#define R_SURF_SIDE_BACK	1
#define R_SURF_LIGHTMAP		2

typedef struct {
	int16_t vis_frame; // PVS frame
	int16_t frame; // renderer frame
	int16_t back_frame; // back-facing renderer frame
	int16_t light_frame; // dynamic lighting frame

	c_bsp_plane_t *plane;
	uint16_t flags; // R_SURF flags

	int32_t first_edge; // look up in model->surf_edges, negative numbers
	uint16_t num_edges; // are backwards edges

	vec3_t mins;
	vec3_t maxs;
	vec3_t center;
	vec3_t normal;

	vec2_t st_mins;
	vec2_t st_maxs;
	vec2_t st_center;
	vec2_t st_extents;

	GLuint index; // index into r_world_model vertex buffers

	r_bsp_texinfo_t *texinfo; // SURF_ flags

	r_bsp_flare_t *flare;

	r_pixel_t light_s, light_t; // lightmap texcoords
	GLuint lightmap_texnum;
	GLuint deluxemap_texnum;

	byte *samples; // raw lighting information, only used at loading time
	byte *lightmap; // finalized lightmap samples, cached for lookups

	vec4_t color;

	uint32_t lights; // bit mask of enabled light sources
} r_bsp_surface_t;

// surfaces are assigned to arrays based on their primary rendering type
// and then sorted by world texnum to reduce glBindTexture calls
typedef struct {
	r_bsp_surface_t **surfaces;
	uint32_t count;
} r_bsp_surfaces_t;

#define sky_surfaces			sorted_surfaces[0]
#define opaque_surfaces			sorted_surfaces[1]
#define opaque_warp_surfaces	sorted_surfaces[2]
#define alpha_test_surfaces		sorted_surfaces[3]
#define blend_surfaces			sorted_surfaces[4]
#define blend_warp_surfaces		sorted_surfaces[5]
#define material_surfaces		sorted_surfaces[6]
#define flare_surfaces			sorted_surfaces[7]
#define back_surfaces			sorted_surfaces[8]

#define NUM_SURFACES_ARRAYS		9

#define R_SurfaceToSurfaces(surfs, surf)\
	(surfs)->surfaces[(surfs)->count++] = surf

typedef struct r_bsp_node_s {
	// common with leaf
	int32_t contents; // -1, to differentiate from leafs
	int16_t vis_frame; // node needs to be traversed if current

	vec3_t mins; // for bounded box culling
	vec3_t maxs;

	struct r_bsp_node_s *parent;
	struct r_model_s *model;

	// node specific
	c_bsp_plane_t *plane;
	struct r_bsp_node_s *children[2];

	uint16_t first_surface;
	uint16_t num_surfaces;
} r_bsp_node_t;

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
	int16_t num_leaf_surfaces;
} r_bsp_leaf_t;

typedef struct {
	int16_t vis_frame;
} r_bsp_cluster_t;

// static light sources
typedef struct {
	vec3_t origin;
	float radius;
	vec3_t color;
	int16_t count;
	const r_bsp_leaf_t *leaf;
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
} r_md3_mesh_t;

typedef struct {
	uint16_t first_frame;
	uint16_t num_frames;
	uint16_t looped_frames;
	uint16_t hz;
} r_md3_animation_t;

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

// object model memory representation
typedef struct {
	uint16_t vert;
	uint16_t normal;
	uint16_t texcoord;
} r_obj_vert_t;

typedef struct {
	r_obj_vert_t verts[3];
} r_obj_tri_t;

typedef struct {
	uint16_t num_verts;
	uint16_t num_verts_parsed;
	float *verts;

	uint16_t num_normals;
	uint16_t num_normals_parsed;
	float *normals;

	uint16_t num_tangents;
	float *tangents;

	uint16_t num_texcoords;
	uint16_t num_texcoords_parsed;
	float *texcoords;

	uint16_t num_tris;
	uint16_t num_tris_parsed;
	r_obj_tri_t *tris;
} r_obj_t;

// shared structure for all model types
typedef enum {
	mod_bad, mod_bsp, mod_bsp_submodel, mod_md3, mod_obj
} r_model_type_t;

// shared mesh configuration
typedef struct {
	vec3_t translate;
	float scale;
	uint32_t flags; // EF_ALPHA_TEST, etc..
} r_mesh_config_t;

typedef struct r_model_s {
	char name[MAX_QPATH];

	r_model_type_t type;
	int32_t version;

	uint16_t num_frames;

	uint32_t flags;

	vec3_t mins, maxs;
	float radius;

	// for bsp models
	uint16_t num_submodels;
	r_bsp_submodel_t *submodels;

	uint16_t num_planes;
	c_bsp_plane_t *planes;

	uint16_t num_leafs;
	r_bsp_leaf_t *leafs;

	uint16_t num_vertexes;
	r_bsp_vertex_t *vertexes;

	uint32_t num_edges;
	r_bsp_edge_t *edges;

	uint16_t num_nodes;
	r_bsp_node_t *nodes;
	uint16_t first_node;

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

	uint16_t lightmap_scale;
	uint32_t lightmap_data_size;
	byte *lightmap_data;

	uint16_t num_bsp_lights;
	r_bsp_light_t *bsp_lights;

	// sorted surfaces arrays
	r_bsp_surfaces_t *sorted_surfaces[NUM_SURFACES_ARRAYS];

	uint16_t first_model_surface, num_model_surfaces; // for submodels
	uint32_t lights; // bit mask of enabled light sources for submodels

	// for mesh models
	r_image_t *skin;

	GLuint num_verts; // raw vertex primitive count, used to build arrays

	GLfloat *verts; // vertex arrays
	GLfloat *texcoords;
	GLfloat *lmtexcoords;
	GLfloat *normals;
	GLfloat *tangents;
	GLfloat *colors;

	GLuint vertex_buffer; // vertex buffer objects
	GLuint texcoord_buffer;
	GLuint lmtexcoord_buffer;
	GLuint normal_buffer;
	GLuint tangent_buffer;
	GLuint color_buffer;

	r_mesh_config_t *world_config;
	r_mesh_config_t *view_config;
	r_mesh_config_t *link_config;

	uint32_t extra_data_size;
	void *extra_data; // raw model data
} r_model_t;

// is a given model a mesh model?
#define IS_MESH_MODEL(m) (m && (m->type == mod_md3 || m->type == mod_obj))

// lights are dynamic lighting sources
typedef struct r_light_s {
	vec3_t origin;
	float radius;
	vec3_t color;
} r_light_t;

#define MAX_LIGHTS			32
#define MAX_ACTIVE_LIGHTS	8

// sustains are light flashes which slowly decay
typedef struct r_sustained_light_s {
	r_light_t light;
	float time;
	float sustain;
} r_sustained_light_t;

// a reference to a static light source plus a light level
typedef struct r_bsp_light_ref_s {
	r_bsp_light_t *bsp_light;
	vec3_t dir;
	float intensity;
} r_bsp_light_ref_t;

typedef enum {
	LIGHTING_INIT, LIGHTING_DIRTY, LIGHTING_READY
} r_lighting_state_t;

// lighting structures contain static lighting info for entities
typedef struct r_lighting_s {
	vec3_t origin; // entity origin
	float radius; // entity radius
	vec3_t mins, maxs; // entity bounding box in world space
	vec3_t shadow_origin;
	vec3_t shadow_normal;
	vec3_t color;
	r_bsp_light_ref_t bsp_light_refs[MAX_ACTIVE_LIGHTS]; // light sources
	r_lighting_state_t state;
} r_lighting_t;

#define LIGHTING_MAX_SHADOW_DISTANCE 128.0

typedef struct r_entity_s {
	struct r_entity_s *next; // for draw lists

	const struct r_entity_s *parent; // for linked models
	const char *tag_name;

	vec3_t origin;
	vec3_t angles;

	matrix4x4_t matrix;
	bool culled;

	struct r_model_s *model;

	uint16_t frame, old_frame; // frame-based animations
	float lerp, back_lerp;

	float scale; // for mesh models

	r_image_t *skins[MD3_MAX_MESHES]; // NULL for default skin
	uint16_t num_skins;

	uint32_t effects; // e.g. EF_ROCKET, EF_WEAPON, ..
	vec3_t shell; // shell color

	r_lighting_t *lighting; // static lighting information
} r_entity_t;

#define MAX_ENTITIES		(MAX_EDICTS * 2)

typedef struct r_particle_s {
	uint16_t type;
	const r_image_t *image;
	GLenum blend;
	uint32_t color;
	float alpha;
	float scale;
	float scroll_s;
	float scroll_t;
	float roll;
	vec3_t org;
	vec3_t end;
	vec3_t dir;
} r_particle_t;

#define MAX_PARTICLES		16384

#define PARTICLE_GRAVITY	150

#define PARTICLE_NORMAL		0x1
#define PARTICLE_ROLL		0x2
#define PARTICLE_DECAL		0x4
#define PARTICLE_BUBBLE		0x8
#define PARTICLE_BEAM		0x10
#define PARTICLE_WEATHER	0x20
#define PARTICLE_SPLASH		0x40

// coronas are soft, alpha-blended, rounded polys
typedef struct r_corona_s {
	vec3_t origin;
	float radius;
	float flicker;
	vec3_t color;
} r_corona_t;

// render modes, set via r_render_mode
typedef enum render_mode_s {
	render_mode_default, render_mode_pro
} r_render_mode_t;

#define MAX_CORONAS 		1024

#define WEATHER_NONE		0
#define WEATHER_RAIN 		1
#define WEATHER_SNOW 		2
#define WEATHER_FOG 		4

#define FOG_START			300.0
#define FOG_END				2500.0

// read-write variables for renderer and client
typedef struct r_view_s {
	r_pixel_t x, y, width, height; // in virtual screen coordinates
	vec2_t fov;

	vec3_t origin; // client's view origin, angles, and vectors
	vec3_t angles;
	vec3_t forward;
	vec3_t right;
	vec3_t up;

	uint32_t contents; // view origin contents mask
	float bob;

	float time;

	byte *area_bits; // if not NULL, only areas with set bits will be drawn

	r_render_mode_t render_mode; // active renderer plugin

	byte weather; // weather effects
	vec4_t fog_color;

	uint16_t num_entities;
	r_entity_t entities[MAX_ENTITIES];

	uint16_t num_particles;
	r_particle_t particles[MAX_PARTICLES];

	uint16_t num_coronas;
	r_corona_t coronas[MAX_CORONAS];

	uint16_t num_lights;
	r_light_t lights[MAX_LIGHTS];

	r_sustained_light_t sustained_lights[MAX_LIGHTS];

	thread_t *thread; // client thread which populates view

	c_trace_t trace; // occlusion testing
	r_entity_t *trace_ent;

	// counters, reset each frame

	uint32_t num_bind_texture;
	uint32_t num_bind_lightmap;
	uint32_t num_bind_deluxemap;
	uint32_t num_bind_normalmap;
	uint32_t num_bind_glossmap;

	uint32_t num_bsp_clusters;
	uint32_t num_bsp_leafs;
	uint32_t num_bsp_surfaces;

	uint32_t num_mesh_models;
	uint32_t num_mesh_tris;

	bool update; // inform the client of state changes
} r_view_t;

// gl context information
typedef struct r_context_s {
	r_pixel_t width, height;

	bool fullscreen;

	int32_t red_bits, green_bits, blue_bits, alpha_bits;
	int32_t stencil_bits, depth_bits, double_buffer;
} r_context_t;

#endif /* __R_TYPES_H__ */
