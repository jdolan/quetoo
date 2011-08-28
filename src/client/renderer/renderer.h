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

// lights are dynamic lighting sources
typedef struct r_light_s {
	vec3_t origin;
	float radius;
	vec3_t color;
} r_light_t;

// sustains are light flashes which slowly decay
typedef struct r_sustained_light_s {
	r_light_t light;
	float time;
	float sustain;
} r_sustained_light_t;

#define MAX_LIGHTS			32
#define MAX_ACTIVE_LIGHTS	8

// a reference to a static light source plus a light level
typedef struct r_bsp_light_ref_s {
	r_bsp_light_t *bsp_light;
	vec3_t dir;
	float intensity;
} r_bsp_light_ref_t;

typedef enum {
	LIGHTING_INIT,
	LIGHTING_DIRTY,
	LIGHTING_READY
} r_lighting_state_t;

// lighting structures contain static lighting info for entities
typedef struct r_lighting_s {
	vec3_t origin;  // entity origin
	float radius;  // entity radius
	vec3_t mins, maxs;  // entity bounding box in world space
	vec3_t shadow_origin;
	vec3_t shadow_normal;
	vec3_t color;
	r_bsp_light_ref_t bsp_light_refs[MAX_ACTIVE_LIGHTS];  // light sources
	r_lighting_state_t state;
} r_lighting_t;

#define LIGHTING_MAX_SHADOW_DISTANCE 128.0

#define MAX_ENTITIES		512
typedef struct r_entity_s {
	vec3_t origin;
	vec3_t angles;

	matrix4x4_t matrix;

	struct r_model_s *model;

	int frame, old_frame;  // frame-based animations
	float lerp, back_lerp;

	vec3_t scale;  // for mesh models

	struct r_image_s *skin;  // NULL for default skin

	int effects;  // e.g. EF_ROCKET, EF_WEAPON, ..
	vec3_t shell;  // shell color

	r_lighting_t *lighting;  // static lighting information

	struct r_entity_s *next;  // for draw lists
} r_entity_t;

typedef struct r_particle_s {
	struct r_particle_s *next;
	float time;
	vec3_t org;
	vec3_t end;
	vec3_t vel;
	vec3_t accel;
	vec3_t current_org;
	vec3_t current_end;
	vec3_t dir;
	float roll;
	struct r_image_s *image;
	int type;
	int color;
	float alpha;
	float alpha_vel;
	float current_alpha;
	float scale;
	float scale_vel;
	float current_scale;
	float scroll_s;
	float scroll_t;
	GLenum blend;
} r_particle_t;

#define MAX_PARTICLES		4096

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
	vec3_t org;
	float radius;
	float flicker;
	vec3_t color;
} r_corona_t;

#define MAX_CORONAS 		1024

#define VID_NORM_WIDTH 		1024
#define VID_NORM_HEIGHT 	768

// read-write variables for renderer and client
typedef struct r_view_s {
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

	byte *area_bits;  // if not NULL, only areas with set bits will be drawn

	int weather;  // weather effects
	vec4_t fog_color;

	int num_entities;
	r_entity_t entities[MAX_ENTITIES];

	int num_particles;
	r_particle_t particles[MAX_PARTICLES];

	int num_coronas;
	r_corona_t coronas[MAX_CORONAS];

	int num_lights;
	r_light_t lights[MAX_LIGHTS];

	r_sustained_light_t sustained_lights[MAX_LIGHTS];

	trace_t trace;  // occlusion testing
	r_entity_t *trace_ent;

	int bsp_polys;  // counters
	int mesh_polys;

	qboolean update;  // eligible for update from client
	qboolean ready;  // eligible for rendering to the client
} r_view_t;

extern r_view_t r_view;

// hardware configuration information
typedef struct r_config_s {
	const char *renderer_string;
	const char *vendor_string;
	const char *version_string;
	const char *extensions_string;

	qboolean vbo;
	qboolean shaders;

	int max_texunits;
} r_config_t;

extern r_config_t r_config;

// private renderer variables
typedef struct r_locals_s {

	vec3_t ambient_light;  // from worldspawn entity

	float sun_light;
	vec3_t sun_color;

	float brightness;
	float saturation;
	float contrast;

	short cluster;  // PVS at origin
	short old_cluster;

	int vis_frame;  // PVS frame

	int frame;  // renderer frame
	int back_frame;  // back-facing renderer frame

	int light_frame;  // dynamic lighting frame

	int active_light_mask;  // a bit mask into r_view.lights
	int active_light_count;  // a count of active lights

	int trace_num;  // lighting traces

	c_plane_t frustum[4];  // for box culling
} r_locals_t;

extern r_locals_t r_locals;

extern byte color_white[4];
extern byte color_black[4];

// development tools
extern cvar_t *r_clear;
extern cvar_t *r_cull;
extern cvar_t *r_lock_vis;
extern cvar_t *r_no_vis;
extern cvar_t *r_draw_bsp_lights;
extern cvar_t *r_draw_bsp_normals;
extern cvar_t *r_draw_deluxemaps;
extern cvar_t *r_draw_lightmaps;
extern cvar_t *r_draw_poly_counts;
extern cvar_t *r_draw_wireframe;

// settings and preferences
extern cvar_t *r_anisotropy;
extern cvar_t *r_brightness;
extern cvar_t *r_bumpmap;
extern cvar_t *r_capture;
extern cvar_t *r_capture_fps;
extern cvar_t *r_capture_quality;
extern cvar_t *r_contrast;
extern cvar_t *r_coronas;
extern cvar_t *r_draw_buffer;
extern cvar_t *r_flares;
extern cvar_t *r_fog;
extern cvar_t *r_fullscreen;
extern cvar_t *r_gamma;
extern cvar_t *r_hardness;
extern cvar_t *r_height;
extern cvar_t *r_hunk_mb;
extern cvar_t *r_invert;
extern cvar_t *r_lightmap_block_size;
extern cvar_t *r_lighting;
extern cvar_t *r_line_alpha;
extern cvar_t *r_line_width;
extern cvar_t *r_materials;
extern cvar_t *r_modulate;
extern cvar_t *r_monochrome;
extern cvar_t *r_multisample;
extern cvar_t *r_optimize;
extern cvar_t *r_parallax;
extern cvar_t *r_programs;
extern cvar_t *r_render_mode;
extern cvar_t *r_saturation;
extern cvar_t *r_screenshot_type;
extern cvar_t *r_screenshot_quality;
extern cvar_t *r_shadows;
extern cvar_t *r_soften;
extern cvar_t *r_specular;
extern cvar_t *r_swap_interval;
extern cvar_t *r_texture_mode;
extern cvar_t *r_threads;
extern cvar_t *r_vertex_buffers;
extern cvar_t *r_warp;
extern cvar_t *r_width;
extern cvar_t *r_windowed_height;
extern cvar_t *r_windowed_width;

// rendermode function pointers
extern void (*R_DrawOpaqueSurfaces)(r_bsp_surfaces_t *surfs);
extern void (*R_DrawOpaqueWarpSurfaces)(r_bsp_surfaces_t *surfs);
extern void (*R_DrawAlphaTestSurfaces)(r_bsp_surfaces_t *surfs);
extern void (*R_DrawBlendSurfaces)(r_bsp_surfaces_t *surfs);
extern void (*R_DrawBlendWarpSurfaces)(r_bsp_surfaces_t *surfs);
extern void (*R_DrawBackSurfaces)(r_bsp_surfaces_t *surfs);
extern void (*R_DrawMeshModel)(r_entity_t *e);

// r_array.c
void R_SetArrayState(const r_model_t *mod);
void R_ResetArrayState(void);

// r_bsp_light.c
void R_LoadBspLights(void);

// r_bsp.c
qboolean R_CullBox(const vec3_t mins, const vec3_t maxs);
qboolean R_CullBspModel(const r_entity_t *e);
void R_DrawBspModel(const r_entity_t *e);
void R_DrawBspLights(void);
void R_DrawBspNormals(void);
void R_MarkSurfaces(void);
const r_bsp_leaf_t *R_LeafForPoint(const vec3_t p, const r_model_t *model);
void R_MarkLeafs(void);

// r_capture.c
void R_UpdateCapture(void);
void R_FlushCapture(void);
void R_InitCapture(void);
void R_ShutdownCapture(void);

// r_context.c
qboolean R_InitContext(int width, int height, qboolean fullscreen);
void R_ShutdownContext(void);

// r_corona.c
void R_AddCorona(const vec3_t org, float radius, float flicker, const vec3_t color);
void R_DrawCoronas(void);

// r_draw.c
void R_InitDraw(void);
void R_FreePics(void);
r_image_t *R_LoadPic(const char *name);
void R_DrawScaledPic(int x, int y, float scale, const char *name);
void R_DrawPic(int x, int y, const char *name);
void R_DrawCursor(int x, int y);
int R_StringWidth(const char *s);
int R_DrawString(int x, int y, const char *s, int color);
int R_DrawBytes(int x, int y, const char *s, size_t size, int color);
int R_DrawSizedString(int x, int y, const char *s, size_t len, size_t size, int color);
void R_DrawChar(int x, int y, char c, int color);
void R_DrawChars(void);
void R_DrawFill(int x, int y, int w, int h, int c, float a);
void R_DrawFills(void);
void R_DrawLine(int x1, int y1, int x2, int y2, int c, float a);
void R_DrawLines(void);

// r_entity.c
r_entity_t *R_AddEntity(const r_entity_t *e);
void R_RotateForEntity(const r_entity_t *e);
void R_TransformForEntity(const r_entity_t *e, const vec3_t in, vec3_t out);
void R_DrawEntities(void);

// r_flare.c
void R_CreateSurfaceFlare(r_bsp_surface_t *surf);
void R_DrawFlareSurfaces(r_bsp_surfaces_t *surfs);

// r_light.c
void R_AddLight(const vec3_t origin, float radius, const vec3_t color);
void R_AddSustainedLight(const vec3_t origin, float radius, const vec3_t color, float sustain);
void R_ResetLights(void);
void R_MarkLights(void);
void R_ShiftLights(const vec3_t offset);
void R_EnableLights(int mask);
void R_EnableLightsByRadius(const vec3_t p);

// r_lighting.c
void R_UpdateLighting(r_lighting_t *lighting);
void R_ApplyLighting(const r_lighting_t *lighting);

// r_lightmap.c
typedef struct r_lightmaps_s {
	GLuint lightmap_texnum;
	GLuint deluxemap_texnum;

	size_t size;  // lightmap block size (NxN)
	unsigned *allocated;  // block availability

	byte *sample_buffer;  // RGB buffers for uploading
	byte *direction_buffer;

	float fbuffer[MAX_BSP_LIGHTMAP * 3];  // RGB buffer for bsp loading
} r_lightmaps_t;

extern r_lightmaps_t r_lightmaps;

void R_BeginBuildingLightmaps(void);
void R_CreateSurfaceLightmap(r_bsp_surface_t *surf);
void R_EndBuildingLightmaps(void);

// r_main.c
const char *R_WorldspawnValue(const char *key);
void R_Trace(const vec3_t start, const vec3_t end, float size, int mask);
void R_Init(void);
void R_Shutdown(void);
void R_BeginFrame(void);
void R_UpdateFrustum(void);
void R_DrawFrame(void);
void R_EndFrame(void);
void R_InitView(void);
void R_LoadMedia(void);
void R_Restart_f(void);
qboolean R_SetMode(void);

// r_material.c
void R_DrawMaterialSurfaces(r_bsp_surfaces_t *surfs);
void R_LoadMaterials(const char *map);

// r_mesh.c
extern vec3_t r_mesh_verts[MD3_MAX_TRIANGLES * 3];
extern vec3_t r_mesh_norms[MD3_MAX_TRIANGLES * 3];
void R_ApplyMeshModelTag(r_entity_t *parent, r_entity_t *e, const char *name);
void R_ApplyMeshModelConfig(r_entity_t *e);
qboolean R_CullMeshModel(const r_entity_t *e);
void R_DrawMeshModel_default(r_entity_t *e);

// r_particle.c
void R_AddParticle(r_particle_t *p);
void R_DrawParticles(void);

// r_sky.c
void R_ClearSkyBox(void);
void R_DrawSkyBox(void);
void R_SetSky(char *name);

// r_surface.c
void R_DrawOpaqueSurfaces_default(r_bsp_surfaces_t *surfs);
void R_DrawOpaqueWarpSurfaces_default(r_bsp_surfaces_t *surfs);
void R_DrawAlphaTestSurfaces_default(r_bsp_surfaces_t *surfs);
void R_DrawBlendSurfaces_default(r_bsp_surfaces_t *surfs);
void R_DrawBlendWarpSurfaces_default(r_bsp_surfaces_t *surfs);
void R_DrawBackSurfaces_default(r_bsp_surfaces_t *surfs);

// r_surface_pro.c
void R_DrawOpaqueSurfaces_pro(r_bsp_surfaces_t *surfs);
void R_DrawAlphaTestSurfaces_pro(r_bsp_surfaces_t *surfs);
void R_DrawBlendSurfaces_pro(r_bsp_surfaces_t *surfs);
void R_DrawBackSurfaces_pro(r_bsp_surfaces_t *surfs);

// r_thread.c
typedef enum {
	THREAD_DEAD,
	THREAD_IDLE,
	THREAD_WAIT,
	THREAD_RUN,
} r_thread_state_t;

typedef struct r_thread_s {
	char name[32];
	SDL_Thread *thread;
	r_thread_state_t state;
	int wait_count;
} r_thread_t;

extern r_thread_t r_thread_pool[2];

#define r_bsp_thread r_thread_pool[0]
#define r_capture_thread r_thread_pool[1]

void R_WaitForThread(r_thread_t *t);
void R_UpdateThreads(int mask);
void R_ShutdownThreads(void);
void R_InitThreads(void);

#endif /*__RENDERER_H__*/
