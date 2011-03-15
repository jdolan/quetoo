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

#ifndef __R_MODEL_H__
#define __R_MODEL_H__

#include "cmodel.h"

// bsp model memory representation
typedef struct {
	vec3_t position;
	vec3_t normal;
	vec4_t color;
	int surfaces;
} r_bsp_vertex_t;

typedef struct {
	vec3_t mins, maxs;
	vec3_t origin;  // for sounds or lights
	float radius;
	int head_node;
	int first_face, num_faces;
} r_bsp_submodel_t;

typedef struct {
	unsigned short v[2];
} r_bsp_edge_t;

typedef struct {
	float vecs[2][4];
	int flags;
	int value;
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

#define SIDE_FRONT	0
#define SIDE_BACK	1
#define SIDE_ON		2

#define MSURF_PLANEBACK	1
#define MSURF_LIGHTMAP	2

typedef struct {
	int vis_frame;  // PVS frame
	int frame;  // renderer frame
	int back_frame;  // back-facing renderer frame
	int light_frame;  // dynamic lighting frame
	int trace_num;  // lightmap trace lookups

	c_plane_t *plane;
	int flags;  // MSURF_ flags

	int first_edge;  // look up in model->surfedges[], negative numbers
	int num_edges;  // are backwards edges

	vec3_t mins;
	vec3_t maxs;
	vec3_t center;
	vec3_t normal;

	vec2_t st_mins;
	vec2_t st_maxs;
	vec2_t st_center;
	vec2_t st_extents;

	GLuint index;  // index into r_worldmodel vertex buffers

	r_bsp_texinfo_t *texinfo;  // SURF_ flags

	r_bsp_flare_t *flare;

	int light_s, light_t;  // lightmap texcoords
	GLuint lightmap_texnum;
	GLuint deluxemap_texnum;

	byte *samples;  // raw lighting information, only used at loading time
	byte *lightmap;  // finalized lightmap samples, cached for lookups

	vec4_t color;

	int lights;  // bit mask of enabled light sources
} r_bsp_surface_t;

// surfaces are assigned to arrays based on their primary rendering type
// and then sorted by world texnum to reduce glBindTexture calls
typedef struct {
	r_bsp_surface_t **surfaces;
	int count;
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
	int contents;  // -1, to differentiate from leafs
	int vis_frame;  // node needs to be traversed if current

	vec3_t mins;  // for bounded box culling
	vec3_t maxs;

	struct r_bsp_node_s *parent;
	struct r_model_s *model;

	// node specific
	c_plane_t *plane;
	struct r_bsp_node_s *children[2];

	unsigned short first_surface;
	unsigned short num_surfaces;
} r_bsp_node_t;

typedef struct {
	// common with node
	int contents;  // will be a negative contents number
	int vis_frame;  // node needs to be traversed if current

	vec3_t mins;  // for bounding box culling
	vec3_t maxs;

	struct r_bsp_node_s *parent;
	struct model_s *model;

	// leaf specific
	int cluster;
	int area;

	r_bsp_surface_t **first_leaf_surface;
	int num_leaf_surfaces;
} r_bsp_leaf_t;

// static light sources
typedef struct {
	vec3_t origin;
	float radius;
	vec3_t color;
	int count;
	const r_bsp_leaf_t *leaf;
} r_bsp_light_t;

// md3 model memory representation
typedef struct {
	vec3_t point;
	vec3_t normal;
} r_md3_vertex_t;

typedef struct {
	char id[4];

	char name[MD3_MAX_PATH];

	int flags;

	int num_skins;

	int num_verts;
	r_md3_vertex_t *verts;
	d_md3_texcoord_t *coords;

	int num_tris;
	unsigned *tris;
} r_md3_mesh_t;

typedef struct {
	int id;
	int version;

	char file_name[MD3_MAX_PATH];

	int flags;

	int num_frames;
	d_md3_frame_t *frames;

	int num_tags;
	d_md3_tag_t *tags;

	int num_meshes;
	r_md3_mesh_t *meshes;
} r_md3_t;


// object model memory representation
typedef struct {
	int vert;
	int normal;
	int texcoord;
} r_obj_vert_t;

typedef struct {
	r_obj_vert_t verts[3];
} r_obj_tri_t;

typedef struct {
	int num_verts;
	int num_verts_parsed;
	float *verts;

	int num_normals;
	int num_normals_parsed;
	float *normals;

	int num_texcoords;
	int num_texcoords_parsed;
	float *texcoords;

	int num_tris;
	int num_tris_parsed;
	r_obj_tri_t *tris;
} r_obj_t;


// shared structure for all model types
typedef enum {
	mod_bad, mod_bsp, mod_bsp_submodel, mod_md2, mod_md3, mod_obj
} r_model_type_t;

// shared mesh configuration
typedef struct {
	vec3_t translate;
	vec3_t scale;
	int flags;  // EF_ALPHATEST, etc..
} r_mesh_config_t;

typedef struct r_model_s {
	char name[MAX_QPATH];

	r_model_type_t type;
	int version;

	int num_frames;

	int flags;

	vec3_t mins, maxs;
	float radius;

	// for bsp models
	int num_submodels;
	r_bsp_submodel_t *submodels;

	int num_planes;
	c_plane_t *planes;

	int num_leafs;
	r_bsp_leaf_t *leafs;

	int num_vertexes;
	r_bsp_vertex_t *vertexes;

	int num_edges;
	r_bsp_edge_t *edges;

	int num_nodes;
	int firstnode;
	r_bsp_node_t *nodes;

	int num_texinfo;
	r_bsp_texinfo_t *texinfo;

	int num_surfaces;
	r_bsp_surface_t *surfaces;

	int num_surf_edges;
	int *surfedges;

	int num_leaf_surfaces;
	r_bsp_surface_t **leafsurfaces;

	int vis_size;
	d_bsp_vis_t *vis;

	int lightmap_scale;
	int lightmap_data_size;
	byte *lightmap_data;

	int num_bsp_lights;
	r_bsp_light_t *bsp_lights;

	// sorted surfaces arrays
	r_bsp_surfaces_t *sorted_surfaces[NUM_SURFACES_ARRAYS];

	int first_model_surface, num_model_surfaces;  // for submodels
	int lights;  // bit mask of enabled light sources for submodels

	// for mesh models
	r_image_t *skin;

	int num_verts;  // raw vertex primitive count, used to build arrays

	GLfloat *verts;  // vertex arrays
	GLfloat *texcoords;
	GLfloat *lmtexcoords;
	GLfloat *normals;
	GLfloat *tangents;
	GLfloat *colors;

	GLuint vertex_buffer;  // vertex buffer objects
	GLuint texcoord_buffer;
	GLuint lmtexcoord_buffer;
	GLuint normal_buffer;
	GLuint tangent_buffer;
	GLuint color_buffer;

	r_mesh_config_t *world_config;
	r_mesh_config_t *view_config;

	int extra_data_size;
	void *extra_data;  // raw model data
} r_model_t;

#define MAX_MOD_KNOWN 512
extern r_model_t r_models[MAX_MOD_KNOWN];
extern r_model_t r_inline_models[MAX_MOD_KNOWN];

void *R_HunkAlloc(size_t size);
void R_AllocVertexArrays(r_model_t *mod);
r_model_t *R_LoadModel(const char *name);
void R_InitModels(void);
void R_ShutdownModels(void);
void R_BeginLoading(const char *map, int mapsize);
void R_ListModels_f(void);
void R_HunkStats_f(void);

// r_bsp_model.c
void R_LoadBspModel(r_model_t *mod, void *buffer);

// r_mesh_model.c
void R_LoadMd2Model(r_model_t *mod, void *buffer);
void R_LoadMd3Model(r_model_t *mod, void *buffer);
void R_LoadObjModel(r_model_t *mod, void *buffer);

#endif /* __R_MODEL_H__ */
