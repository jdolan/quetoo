/*
* Copyright(c) 1997-2001 Id Software, Inc.
* Copyright(c) 2002 The Quakeforge Project.
* Copyright(c) 2006 Quake2World.
*
* This program is free software; you can redistribute it and/or
* modify it under the terms of the GNU General Public License
* as published by the Free Software Foundation; either version 2
* of the License, or(at your option) any later version.
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

// bsp model memory representation
typedef struct {
	vec3_t position;
	vec3_t normal;
	vec4_t color;
	int surfaces;
} mvertex_t;

typedef struct {
	vec3_t mins, maxs;
	vec3_t origin;  // for sounds or lights
	float radius;
	int headnode;
	int firstface, numfaces;
} msubmodel_t;

typedef struct {
	unsigned short v[2];
} medge_t;

typedef struct mtexinfo_s {
	float vecs[2][4];
	int flags;
	int value;
	char name[32];
	image_t *image;
} mtexinfo_t;

typedef struct mflare_s {
	vec3_t origin;
	float radius;
	const image_t *image;
	vec3_t color;
	float time;
	float alpha;
} mflare_t;

#define SIDE_FRONT	0
#define SIDE_BACK	1
#define SIDE_ON		2

#define MSURF_PLANEBACK	1
#define MSURF_LIGHTMAP	2

typedef struct msurface_s {
	int visframe;  // PVS frame
	int frame;  // renderer frame
	int backframe;  // back-facing renderer frame
	int lightframe;  // dynamic lighting frame
	int tracenum;  // lightmap trace lookups

	cplane_t *plane;
	int flags;  // MSURF_ flags

	int firstedge;  // look up in model->surfedges[], negative numbers
	int numedges;  // are backwards edges

	vec3_t mins;
	vec3_t maxs;
	vec3_t center;
	vec3_t normal;

	vec2_t stmins;
	vec2_t stmaxs;
	vec2_t stcenter;
	vec2_t stextents;

	GLuint index;  // index into r_worldmodel vertex buffers

	mtexinfo_t *texinfo;  // SURF_ flags

	mflare_t *flare;

	int light_s, light_t;  // lightmap texcoords
	GLuint lightmap_texnum;
	GLuint deluxemap_texnum;

	byte *samples;  // raw lighting information, only used at loading time
	byte *lightmap;  // finalized lightmap samples, cached for lookups

	vec4_t color;

	int lights;  // bit mask of enabled light sources
} msurface_t;

// surfaces are assigned to arrays based on their primary rendering type
// and then sorted by world texnum to reduce binds
typedef struct msurfaces_s {
	msurface_t **surfaces;
	int count;
} msurfaces_t;

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

typedef struct mnode_s {
	// common with leaf
	int contents;  // -1, to differentiate from leafs
	int visframe;  // node needs to be traversed if current

	float minmaxs[6];  // for bounding box culling

	struct mnode_s *parent;
	struct model_s *model;

	// node specific
	cplane_t *plane;
	struct mnode_s *children[2];

	unsigned short firstsurface;
	unsigned short numsurfaces;
} mnode_t;

typedef struct mleaf_s {
	// common with node
	int contents;  // will be a negative contents number
	int visframe;  // node needs to be traversed if current

	float minmaxs[6];  // for bounding box culling

	struct mnode_s *parent;
	struct model_s *model;

	// leaf specific
	int cluster;
	int area;

	msurface_t **firstleafsurface;
	int numleafsurfaces;
} mleaf_t;

// md3 model memory representation
typedef struct mmd3vertex_s {
	vec3_t point;
	vec3_t normal;
} mmd3vertex_t;

typedef struct {
	char id[4];

	char name[MD3_MAX_PATH];

	int flags;

	int num_skins;

	int num_verts;
	mmd3vertex_t *verts;
	dmd3coord_t *coords;

	int num_tris;
	unsigned *tris;
} mmd3mesh_t;

typedef struct mmd3_s {
	int id;
	int version;

	char filename[MD3_MAX_PATH];

	int flags;

	int num_frames;
	dmd3frame_t *frames;

	int num_tags;
	dmd3tag_t *tags;

	int num_meshes;
	mmd3mesh_t *meshes;
} mmd3_t;


// object model memory representation
typedef struct mobjvert_s {
	int vert;
	int normal;
	int texcoord;
} mobjvert_t;

typedef struct mobjtri_s {
	mobjvert_t verts[3];
} mobjtri_t;

typedef struct mobj_s {
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
	mobjtri_t *tris;
} mobj_t;


// shared struct for all model types
typedef enum {
	mod_bad, mod_bsp, mod_bsp_submodel, mod_md2, mod_md3, mod_obj
} modtype_t;

// shared mesh configuration
typedef struct mesh_config_s {
	vec3_t translate;
	vec3_t scale;
	int flags;  // EF_ALPHATEST, etc..
} mesh_config_t;

typedef struct model_s {
	char name[MAX_QPATH];

	modtype_t type;
	int version;

	int num_frames;

	int flags;

	vec3_t mins, maxs;
	float radius;

	// for bsp models
	int numsubmodels;
	msubmodel_t *submodels;

	int numplanes;
	cplane_t *planes;

	int numleafs;
	mleaf_t *leafs;

	int numvertexes;
	mvertex_t *vertexes;

	int numedges;
	medge_t *edges;

	int numnodes;
	int firstnode;
	mnode_t *nodes;

	int numtexinfo;
	mtexinfo_t *texinfo;

	int numsurfaces;
	msurface_t *surfaces;

	int numsurfedges;
	int *surfedges;

	int numleafsurfaces;
	msurface_t **leafsurfaces;

	dvis_t *vis;

	int lightmap_scale;
	byte *lightdata;

	// sorted surfaces arrays
	msurfaces_t *sorted_surfaces[NUM_SURFACES_ARRAYS];

	int firstmodelsurface, nummodelsurfaces;  // for submodels
	int lights;  // bit mask of enabled light sources for submodels

	// for md2/md3 models
	image_t *skin;

	int vertexcount;

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

	mesh_config_t *world_config;
	mesh_config_t *view_config;

	int extradatasize;
	void *extradata;  // raw model data
} model_t;

#define MAX_MOD_KNOWN 512
extern model_t r_models[MAX_MOD_KNOWN];
extern model_t r_inlinemodels[MAX_MOD_KNOWN];

void *R_HunkAlloc(size_t size);
void R_AllocVertexArrays(model_t *mod);
model_t *R_LoadModel(const char *name);
void R_InitModels(void);
void R_ShutdownModels(void);
void R_BeginLoading(const char *map, int mapsize);
void R_ListModels_f(void);
void R_HunkStats_f(void);

// r_model_bsp.c
void R_LoadBspModel(model_t *mod, void *buffer);

// r_model_mesh.c
void R_LoadMd2Model(model_t *mod, void *buffer);
void R_LoadMd3Model(model_t *mod, void *buffer);
void R_LoadObjModel(model_t *mod, void *buffer);

#endif /* __R_MODEL_H__ */
