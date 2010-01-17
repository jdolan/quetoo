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

#ifndef __RENDERER_H__
#define __RENDERER_H__

#include <stdio.h>
#include <math.h>

#include "r_gl.h"
#include "r_image.h"
#include "r_matrix.h"
#include "r_model.h"
#include "r_state.h"

#define PITCH 0
#define YAW 1
#define ROLL 2

#define WEATHER_NONE	0
#define WEATHER_RAIN 	1
#define WEATHER_SNOW 	2
#define WEATHER_FOG 	4

#define FOG_START	300.0
#define FOG_END		2500.0

typedef struct static_lighting_s {
	vec3_t origin;  // starting point, entity origin
	vec3_t point;  // impact point, shadow origin
	vec3_t normal;  // shadow direction
	vec3_t color;  // light color
	vec3_t position;  // and position
	float time;  // lerping interval
	vec3_t colors[2];  // lerping color
	vec3_t positions[2];  // and positions
	qboolean dirty;  // cache invalidation
} static_lighting_t;

#define MAX_ENTITIES		256
typedef struct entity_s {
	vec3_t origin;
	vec3_t angles;

	struct model_s *model;

	int frame, oldframe;  // frame-based animations
	float lerp, backlerp;

	vec3_t scale;  // for mesh models

	int skinnum;  // for masking players and vweaps
	struct image_s *skin;  // NULL for inline skin

	int flags;  // e.g. EF_ROCKET, EF_WEAPON, ..
	vec3_t shell;  // shell color

	static_lighting_t *lighting;

	struct entity_s *next;  // for chaining
} entity_t;

typedef struct particle_s {
	struct particle_s *next;
	float time;
	vec3_t org;
	vec3_t end;
	vec3_t vel;
	vec3_t accel;
	vec3_t curorg;
	vec3_t curend;
	vec3_t dir;
	float roll;
	struct image_s *image;
	int type;
	int color;
	float alpha;
	float alphavel;
	float curalpha;
	float scale;
	float scalevel;
	float curscale;
	float scroll_s;
	float scroll_t;
	GLenum blend;
} particle_t;

#define MAX_PARTICLES		4096

#define PARTICLE_GRAVITY	120

#define PARTICLE_NORMAL		0x1
#define PARTICLE_ROLL		0x2
#define PARTICLE_DECAL		0x4
#define PARTICLE_BUBBLE		0x8
#define PARTICLE_BEAM		0x10
#define PARTICLE_WEATHER	0x20
#define PARTICLE_SPLASH		0x40

// coronas are soft, alpha-blended, rounded polys
typedef struct corona_s {
	vec3_t org;
	float radius;
	vec3_t color;
} corona_t;

#define MAX_CORONAS 		128

// lights are dynamic lighting sources
typedef struct light_s {
	vec3_t org;
	float radius;
	vec3_t color;
} light_t;

// sustains are light flashes which slowly decay
typedef struct sustain_s {
	light_t light;
	float time;
	float sustain;
} sustain_t;

#define MAX_LIGHTS			32
#define MAX_ACTIVE_LIGHTS	8

// read-write variables for renderer and client
typedef struct renderer_view_s {
	int x, y, width, height;  // in virtual screen coordinates
	float fov_x, fov_y;

	vec3_t origin;  // client's view origin, angles, and velocity
	vec3_t angles;
	vec3_t velocity;
	vec3_t forward;
	vec3_t right;
	vec3_t up;

	qboolean ground;  // client is on ground
	float bob;

	float time;

	byte *areabits;  // if not NULL, only areas with set bits will be drawn

	int weather;  // weather effects
	vec4_t fog_color;

	vec3_t ambient_light;  // from static lighting

	int num_entities;
	entity_t entities[MAX_ENTITIES];

	int num_particles;
	particle_t particles[MAX_PARTICLES];

	int num_coronas;
	corona_t coronas[MAX_CORONAS];

	int num_lights;
	light_t lights[MAX_LIGHTS];

	sustain_t sustains[MAX_LIGHTS];

	trace_t trace;  // occlusion testing
	entity_t *trace_ent;

	int bsp_polys;  // counters
	int mesh_polys;

	qboolean update;  // eligible for update from client
	qboolean ready;  // eligible for rendering to the client
} renderer_view_t;

extern renderer_view_t r_view;

// hardware configuration information
typedef struct renderer_config_s {
	const char *renderer_string;
	const char *vendor_string;
	const char *version_string;
	const char *extensions_string;

	qboolean vbo;
	qboolean shaders;

	int max_texunits;
} renderer_config_t;

extern renderer_config_t r_config;

// threading state
typedef enum {
	THREAD_DEAD,
	THREAD_IDLE,
	THREAD_CLIENT,
	THREAD_BSP,
	THREAD_RENDERER
} threadstate_t;

typedef struct renderer_threadstate_s {
	SDL_Thread *thread;
	threadstate_t state;
} renderer_threadstate_t;

extern renderer_threadstate_t r_threadstate;

// private renderer variables
typedef struct renderer_locals_s {
	int cluster;  // PVS at origin
	int oldcluster;

	int visframe;  // PVS frame

	int frame;  // renderer frame
	int backframe;  // back-facing renderer frame

	int lightframe;  // dynamic lighting frame

	int tracenum;  // lighting traces

	GLfloat modelview[16];  // model-view matrix

	cplane_t frustum[4];  // for box culling
} renderer_locals_t;

extern renderer_locals_t r_locals;

extern byte color_white[4];
extern byte color_black[4];

// development tools
extern cvar_t *r_clear;
extern cvar_t *r_cull;
extern cvar_t *r_deluxemap;
extern cvar_t *r_lightmap;
extern cvar_t *r_lockvis;
extern cvar_t *r_novis;
extern cvar_t *r_shownormals;
extern cvar_t *r_showpolys;
extern cvar_t *r_speeds;

// settings and preferences
extern cvar_t *r_anisotropy;
extern cvar_t *r_brightness;
extern cvar_t *r_bumpmap;
extern cvar_t *r_contrast;
extern cvar_t *r_coronas;
extern cvar_t *r_drawbuffer;
extern cvar_t *r_flares;
extern cvar_t *r_fog;
extern cvar_t *r_fullscreen;
extern cvar_t *r_gamma;
extern cvar_t *r_hardness;
extern cvar_t *r_height;
extern cvar_t *r_hunkmegs;
extern cvar_t *r_invert;
extern cvar_t *r_lightmapsize;
extern cvar_t *r_lighting;
extern cvar_t *r_lights;
extern cvar_t *r_lines;
extern cvar_t *r_linewidth;
extern cvar_t *r_materials;
extern cvar_t *r_modulate;
extern cvar_t *r_monochrome;
extern cvar_t *r_multisample;
extern cvar_t *r_optimize;
extern cvar_t *r_parallax;
extern cvar_t *r_programs;
extern cvar_t *r_rendermode;
extern cvar_t *r_saturation;
extern cvar_t *r_screenshot_type;
extern cvar_t *r_screenshot_quality;
extern cvar_t *r_shadows;
extern cvar_t *r_soften;
extern cvar_t *r_specular;
extern cvar_t *r_swapinterval;
extern cvar_t *r_texturemode;
extern cvar_t *r_threads;
extern cvar_t *r_vertexbuffers;
extern cvar_t *r_warp;
extern cvar_t *r_width;
extern cvar_t *r_windowedheight;
extern cvar_t *r_windowedwidth;

extern model_t *r_worldmodel;
extern model_t *r_loadmodel;

// rendermode function pointers
void (*R_DrawOpaqueSurfaces)(msurfaces_t *surfs);
void (*R_DrawOpaqueWarpSurfaces)(msurfaces_t *surfs);
void (*R_DrawAlphaTestSurfaces)(msurfaces_t *surfs);
void (*R_DrawBlendSurfaces)(msurfaces_t *surfs);
void (*R_DrawBlendWarpSurfaces)(msurfaces_t *surfs);
void (*R_DrawBackSurfaces)(msurfaces_t *surfs);
void (*R_DrawMeshModel)(entity_t *e);

// r_array.c
void R_SetArrayState(const model_t *mod);
void R_ResetArrayState(void);

// r_bsp.c
qboolean R_CullBox(const vec3_t mins, const vec3_t maxs);
qboolean R_CullBspModel(const entity_t *e);
void R_DrawBspModel(const entity_t *e);
void R_DrawBspNormals(void);
void R_MarkSurfaces(void);
const mleaf_t *R_LeafForPoint(const vec3_t p, const model_t *model);
void R_MarkLeafs(void);

// r_context.c
qboolean R_InitContext(int width, int height, qboolean fullscreen);
void R_ShutdownContext(void);

// r_corona.c
void R_AddCorona(const vec3_t org, float intensity, const vec3_t color);
void R_DrawCoronas(void);

// r_draw.c
void R_InitDraw(void);
void R_FreePics(void);
image_t *R_LoadPic(const char *name);
void R_DrawScaledPic(int x, int y, float scale, const char *name);
void R_DrawPic(int x, int y, const char *name);
void R_DrawChar(int x, int y, char c, int color);
int R_DrawString(int x, int y, const char *s, int color);
int R_DrawBytes(int x, int y, const char *s, int size, int color);
int R_DrawSizedString(int x, int y, const char *s, int len, int size, int color);
void R_DrawChars(void);
void R_DrawFill(int x, int y, int w, int h, int c);
void R_DrawFillAlpha(int x, int y, int w, int h, int c, float a);
void R_DrawFillAlphas(void);

// r_entity.c
void R_AddEntity(const entity_t *e);
void R_RotateForEntity(const entity_t *e);
void R_TransformForEntity(const entity_t *e, const vec3_t in, vec3_t out);
void R_DrawEntities(void);

// r_flare.c
void R_CreateSurfaceFlare(msurface_t *surf);
void R_DrawFlareSurfaces(msurfaces_t *surfs);

// r_light.c
void R_AddLight(const vec3_t org, float radius, const vec3_t color);
void R_AddSustainedLight(const vec3_t org, float radius, const vec3_t color, float sustain);
void R_MarkLights(void);
void R_ShiftLights(const vec3_t offset);
void R_EnableLights(int mask);
void R_EnableLightsByRadius(const vec3_t p);

// r_lightmap.c
typedef struct lightmaps_s {
	GLuint lightmap_texnum;
	GLuint deluxemap_texnum;

	int size;  // lightmap block size (NxN)
	unsigned *allocated;  // block availability

	byte *sample_buffer;  // RGBA buffers for uploading
	byte *direction_buffer;

	float fbuffer[MAX_BSP_LIGHTMAP * 3];  // RGB buffer for bsp loading
} lightmaps_t;

extern lightmaps_t r_lightmaps;

void R_BeginBuildingLightmaps(void);
void R_CreateSurfaceLightmap(msurface_t *surf);
void R_EndBuildingLightmaps(void);
void R_LightPoint(const vec3_t point, static_lighting_t *lighting);

// r_main.c
void R_Trace(const vec3_t start, const vec3_t end, float size, int mask);
void R_Init(void);
void R_Shutdown(void);
void R_BeginFrame(void);
void R_UpdateFrustum(void);
void R_DrawFrame(void);
void R_EndFrame(void);
void R_LoadMedia(void);
void R_Restart_f(void);
qboolean R_SetMode(void);

// r_material.c
void R_DrawMaterialSurfaces(msurfaces_t *surfs);
void R_LoadMaterials(const char *map);

// r_mesh.c
extern vec3_t r_mesh_verts[MD3_MAX_VERTS];
extern vec3_t r_mesh_norms[MD3_MAX_VERTS];
void R_ApplyMeshModelConfig(entity_t *e);
qboolean R_CullMeshModel(const entity_t *e);
void R_DrawMeshModel_default(entity_t *e);
void R_DrawMeshShadows(void);

// r_particle.c
void R_AddParticle(particle_t *p);
void R_DrawParticles(void);

// r_sky.c
void R_ClearSkyBox(void);
void R_DrawSkyBox(void);
void R_SetSky(char *name);

// r_surface.c
void R_DrawOpaqueSurfaces_default(msurfaces_t *surfs);
void R_DrawOpaqueWarpSurfaces_default(msurfaces_t *surfs);
void R_DrawAlphaTestSurfaces_default(msurfaces_t *surfs);
void R_DrawBlendSurfaces_default(msurfaces_t *surfs);
void R_DrawBlendWarpSurfaces_default(msurfaces_t *surfs);
void R_DrawBackSurfaces_default(msurfaces_t *surfs);

// r_surface_pro.c
void R_DrawOpaqueSurfaces_pro(msurfaces_t *surfs);
void R_DrawAlphaTestSurfaces_pro(msurfaces_t *surfs);
void R_DrawBlendSurfaces_pro(msurfaces_t *surfs);
void R_DrawBackSurfaces_pro(msurfaces_t *surfs);

// r_thread.c
int R_RunThread(void *p);
void R_ShutdownThreads(void);
void R_InitThreads(void);

#endif /*__RENDERER_H__*/
