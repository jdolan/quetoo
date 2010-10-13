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

#include <ctype.h>

#ifndef _WIN32
#include <dlfcn.h>
#endif

#include "client.h"

renderer_view_t r_view;

renderer_locals_t r_locals;

renderer_config_t r_config;

renderer_state_t r_state;

model_t *r_worldmodel;
model_t *r_loadmodel;

byte color_white[4] = {255, 255, 255, 255};
byte color_black[4] = {0, 0, 0, 128};

cvar_t *r_clear;
cvar_t *r_cull;
cvar_t *r_deluxemap;
cvar_t *r_lightmap;
cvar_t *r_lockvis;
cvar_t *r_novis;
cvar_t *r_shownormals;
cvar_t *r_showpolys;
cvar_t *r_speeds;

cvar_t *r_anisotropy;
cvar_t *r_brightness;
cvar_t *r_bumpmap;
cvar_t *r_contrast;
cvar_t *r_coronas;
cvar_t *r_drawbuffer;
cvar_t *r_flares;
cvar_t *r_fog;
cvar_t *r_fullscreen;
cvar_t *r_gamma;
cvar_t *r_hardness;
cvar_t *r_height;
cvar_t *r_hunkmegs;
cvar_t *r_invert;
cvar_t *r_lightmapsize;
cvar_t *r_lighting;
cvar_t *r_lights;
cvar_t *r_lines;
cvar_t *r_linewidth;
cvar_t *r_materials;
cvar_t *r_modulate;
cvar_t *r_monochrome;
cvar_t *r_multisample;
cvar_t *r_optimize;
cvar_t *r_parallax;
cvar_t *r_programs;
cvar_t *r_rendermode;
cvar_t *r_saturation;
cvar_t *r_screenshot_type;
cvar_t *r_screenshot_quality;
cvar_t *r_shadows;
cvar_t *r_soften;
cvar_t *r_specular;
cvar_t *r_swapinterval;
cvar_t *r_texturemode;
cvar_t *r_threads;
cvar_t *r_vertexbuffers;
cvar_t *r_warp;
cvar_t *r_width;
cvar_t *r_windowedheight;
cvar_t *r_windowedwidth;

void (APIENTRY *qglActiveTexture)(GLenum texture);
void (APIENTRY *qglClientActiveTexture)(GLenum texture);

void (APIENTRY *qglGenBuffers)(GLuint count, GLuint *id);
void (APIENTRY *qglDeleteBuffers)(GLuint count, GLuint *id);
void (APIENTRY *qglBindBuffer)(GLenum target, GLuint id);
void (APIENTRY *qglBufferData)(GLenum target, GLsizei size, const GLvoid *data, GLenum usage);

void (APIENTRY *qglEnableVertexAttribArray)(GLuint index);
void (APIENTRY *qglDisableVertexAttribArray)(GLuint index);
void (APIENTRY *qglVertexAttribPointer)(GLuint index, GLint size, GLenum type,
		GLboolean normalized, GLsizei stride, const GLvoid *pointer);

GLuint (APIENTRY *qglCreateShader)(GLenum type);
void (APIENTRY *qglDeleteShader)(GLuint id);
void (APIENTRY *qglShaderSource)(GLuint id, GLuint count, GLchar **sources, GLuint *len);
void (APIENTRY *qglCompileShader)(GLuint id);
void (APIENTRY *qglGetShaderiv)(GLuint id, GLenum field, GLuint *dest);
void (APIENTRY *qglGetShaderInfoLog)(GLuint id, GLuint maxlen, GLuint *len, GLchar *dest);
GLuint (APIENTRY *qglCreateProgram)(void);
void (APIENTRY *qglDeleteProgram)(GLuint id);
void (APIENTRY *qglAttachShader)(GLuint prog, GLuint shader);
void (APIENTRY *qglDetachShader)(GLuint prog, GLuint shader);
void (APIENTRY *qglLinkProgram)(GLuint id);
void (APIENTRY *qglUseProgram)(GLuint id);
void (APIENTRY *qglGetProgramiv)(GLuint id, GLenum field, GLuint *dest);
void (APIENTRY *qglGetProgramInfoLog)(GLuint id, GLuint maxlen, GLuint *len, GLchar *dest);
GLint (APIENTRY *qglGetUniformLocation)(GLuint id, const GLchar *name);
void (APIENTRY *qglUniform1i)(GLint location, GLint i);
void (APIENTRY *qglUniform1f)(GLint location, GLfloat f);
void (APIENTRY *qglUniform3fv)(GLint location, int count, GLfloat *f);
void (APIENTRY *qglUniform4fv)(GLint location, int count, GLfloat *f);
GLint (APIENTRY *qglGetAttribLocation)(GLuint id, const GLchar *name);

void (*R_DrawOpaqueSurfaces)(msurfaces_t *surfs);
void (*R_DrawOpaqueWarpSurfaces)(msurfaces_t *surfs);
void (*R_DrawAlphaTestSurfaces)(msurfaces_t *surfs);
void (*R_DrawBlendSurfaces)(msurfaces_t *surfs);
void (*R_DrawBlendWarpSurfaces)(msurfaces_t *surfs);
void (*R_DrawBackSurfaces)(msurfaces_t *surfs);
void (*R_DrawMeshModel)(entity_t *e);

/*
 * R_Trace
 *
 * Traces to world and BSP models.  If a BSP entity is hit, it is saved as
 * r_view.trace_ent.
 */
void R_Trace(const vec3_t start, const vec3_t end, float size, int mask){
	vec3_t mins, maxs;
	trace_t tr;
	float frac;
	int i;

	r_locals.tracenum++;

	if(r_locals.tracenum > 0xffff)  // avoid overflows
		r_locals.tracenum = 0;

	VectorSet(mins, -size, -size, -size);
	VectorSet(maxs, size, size, size);

	// check world
	r_view.trace = Cm_BoxTrace(start, end, mins, maxs, r_worldmodel->firstnode, mask);
	r_view.trace_ent = NULL;

	frac = r_view.trace.fraction;

	// check bsp models
	for(i = 0; i < r_view.num_entities; i++){

		entity_t *ent = &r_view.entities[i];
		const model_t *m = ent->model;

		if(!m || m->type != mod_bsp_submodel)
			continue;

		tr = Cm_TransformedBoxTrace(start, end, mins, maxs, m->firstnode,
				mask, ent->origin, ent->angles);

		if(tr.fraction < frac){
			r_view.trace = tr;
			r_view.trace_ent = ent;

			frac = tr.fraction;
		}
	}
}


/*
 * R_SignbisForPlane
 */
static inline int R_SignbitsForPlane(const cplane_t *plane){
	int bits, j;

	// for fast box on planeside test
	bits = 0;
	for(j = 0; j < 3; j++){
		if(plane->normal[j] < 0)
			bits |= 1 << j;
	}
	return bits;
}


/*
 * R_UpdateFrustum
 */
void R_UpdateFrustum(void){
	int i;

	if(!r_cull->value)
		return;

	// rotate r_view.forward right by fov_x / 2 degrees
	RotatePointAroundVector(r_locals.frustum[0].normal, r_view.up, r_view.forward, -(90 - r_view.fov_x / 2));
	// rotate r_view.forward left by fov_x / 2 degrees
	RotatePointAroundVector(r_locals.frustum[1].normal, r_view.up, r_view.forward, 90 - r_view.fov_x / 2);
	// rotate r_view.forward up by fov_x / 2 degrees
	RotatePointAroundVector(r_locals.frustum[2].normal, r_view.right, r_view.forward, 90 - r_view.fov_y / 2);
	// rotate r_view.forward down by fov_x / 2 degrees
	RotatePointAroundVector(r_locals.frustum[3].normal, r_view.right, r_view.forward, -(90 - r_view.fov_y / 2));

	for(i = 0; i < 4; i++){
		r_locals.frustum[i].type = PLANE_ANYZ;
		r_locals.frustum[i].dist = DotProduct(r_view.origin, r_locals.frustum[i].normal);
		r_locals.frustum[i].signbits = R_SignbitsForPlane(&r_locals.frustum[i]);
	}
}


/*
 * R_GetError
 */
static void R_GetError(void){
	GLenum err;
	char *s;

	while(true){

		if((err = glGetError()) == GL_NO_ERROR)
			return;

		switch(err){
			case GL_INVALID_ENUM:
				s = "GL_INVALID_ENUM";
				break;
			case GL_INVALID_VALUE:
				s = "GL_INVALID_VALUE";
				break;
			case GL_INVALID_OPERATION:
				s = "GL_INVALID_OPERATION";
				break;
			case GL_STACK_OVERFLOW:
				s = "GL_STACK_OVERFLOW";
				break;
			case GL_OUT_OF_MEMORY:
				s = "GL_OUT_OF_MEMORY";
				break;
			default:
				s = "Unkown error";
				break;
		}

		Com_Warn("R_GetError: %s.\n", s);
	}
}


#include <unistd.h>

/*
 * R_DrawFrame
 */
void R_DrawFrame(void){

	R_GetError();

	if(r_threads->value){
		while(r_threadstate.state != THREAD_RENDERER)
			usleep(0);
	}
	else {
		R_UpdateFrustum();

		R_MarkLeafs();

		R_MarkSurfaces();
	}

	R_MarkLights();

	R_EnableFog(true);

	R_DrawSkyBox();

	R_DrawOpaqueSurfaces(r_worldmodel->opaque_surfaces);

	R_DrawOpaqueWarpSurfaces(r_worldmodel->opaque_warp_surfaces);

	R_DrawAlphaTestSurfaces(r_worldmodel->alpha_test_surfaces);

	R_EnableBlend(true);

	R_DrawBackSurfaces(r_worldmodel->back_surfaces);

	R_DrawMaterialSurfaces(r_worldmodel->material_surfaces);

	R_DrawFlareSurfaces(r_worldmodel->flare_surfaces);

	R_EnableBlend(false);

	R_DrawBspNormals();

	R_DrawEntities();

	R_EnableBlend(true);

	R_DrawBlendSurfaces(r_worldmodel->blend_surfaces);

	R_DrawBlendWarpSurfaces(r_worldmodel->blend_warp_surfaces);

	R_DrawMeshShadows();

	R_DrawParticles();

	R_EnableFog(false);

	R_DrawCoronas();

	R_EnableBlend(false);

	R_ResetArrayState();

	r_threadstate.state = THREAD_CLIENT;
}


/*
 * R_RenderMode
 */
static void R_RenderMode(const char *mode){

	r_state.rendermode = rendermode_default;

	R_DrawOpaqueSurfaces = R_DrawOpaqueSurfaces_default;
	R_DrawOpaqueWarpSurfaces = R_DrawOpaqueWarpSurfaces_default;
	R_DrawAlphaTestSurfaces = R_DrawAlphaTestSurfaces_default;
	R_DrawBlendSurfaces = R_DrawBlendSurfaces_default;
	R_DrawBlendWarpSurfaces = R_DrawBlendWarpSurfaces_default;
	R_DrawBackSurfaces = R_DrawBackSurfaces_default;

	R_DrawMeshModel = R_DrawMeshModel_default;

	if(!mode || !*mode)
		return;

	if(!strcmp(mode, "pro")){

		r_state.rendermode = rendermode_pro;

		R_DrawOpaqueSurfaces = R_DrawOpaqueSurfaces_pro;
		R_DrawAlphaTestSurfaces = R_DrawAlphaTestSurfaces_pro;
		R_DrawBlendSurfaces = R_DrawBlendSurfaces_pro;
		R_DrawBackSurfaces = R_DrawBackSurfaces_pro;
	}
}


/*
 * R_Clear
 */
static inline void R_Clear(void){

	// clear screen if desired
	if(r_clear->value || r_showpolys->value || r_view.x || r_view.y || !r_view.ready)
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	else
		glClear(GL_DEPTH_BUFFER_BIT);
}

/*
 * R_BeginFrame
 */
void R_BeginFrame(void){

	// gamma control
	if(r_gamma->modified){
		float g = r_gamma->value;

		if(g < 0.1)
			g = 0.1;
		else if(g > 3.0)
			g = 3.0;

		SDL_SetGamma(g, g, g);

		Cvar_SetValue("r_gamma", g);
		r_gamma->modified = false;
	}

	// draw buffer stuff
	if(r_drawbuffer->modified){
		if(!strcasecmp(r_drawbuffer->string, "GL_FRONT"))
			glDrawBuffer(GL_FRONT);
		else
			glDrawBuffer(GL_BACK);
		r_drawbuffer->modified = false;
	}

	// texturemode stuff
	if(r_texturemode->modified || r_anisotropy->modified){
		R_TextureMode(r_texturemode->string);
		r_texturemode->modified = r_anisotropy->modified = false;
	}

	// rendermode stuff
	if(r_rendermode->modified){
		R_RenderMode(r_rendermode->string);
		r_rendermode->modified = false;
	}

	// threads
	if(r_threads->modified){
		if(r_threads->value)
			R_InitThreads();
		else
			R_ShutdownThreads();
		r_threads->modified = false;
	}

	R_Clear();
}


/*
 * R_EndFrame
 */
void R_EndFrame(void){

	SDL_GL_SwapBuffers();  // swap buffers
}


/*
 * R_ResolveWeather
 *
 * Parses the weather configstring for weather and fog definitions,
 * e.g. "rain fog 0.8 0.75 0.65".
 */
static void R_ResolveWeather(void){
	char *weather, *c;
	int err;

	r_view.weather = WEATHER_NONE;
	r_view.fog_color[3] = 1.0;

	VectorSet(r_view.fog_color, 0.75, 0.75, 0.75);

	weather = cl.configstrings[CS_WEATHER];

	if(!weather || !strlen(weather))
		return;

	if(strstr(weather, "rain"))
		r_view.weather |= WEATHER_RAIN;

	if(strstr(weather, "snow"))
		r_view.weather |= WEATHER_SNOW;

	if((c = strstr(weather, "fog"))){

		r_view.weather |= WEATHER_FOG;
		err = -1;

		if(strlen(c) > 3)  // try to parse fog color
			err = sscanf(c + 4, "%f %f %f", &r_view.fog_color[0],
					&r_view.fog_color[1], &r_view.fog_color[2]);

		if(err != 3)  // default to gray
			VectorSet(r_view.fog_color, 0.75, 0.75, 0.75);
	}
}


/*
 * R_LoadMedia
 */
void R_LoadMedia(void){
	char name[MAX_QPATH];
	int i, j;

	if(!cl.configstrings[CS_MODELS + 1][0])
		return;  // no map loaded

	Cl_LoadProgress(0);

	memset(&r_view, 0, sizeof(r_view));
	memset(&r_locals, 0, sizeof(r_locals));

	R_FreeImages();

	strncpy(name, cl.configstrings[CS_MODELS + 1] + 5, sizeof(name) - 1);  // skip "maps/"
	name[strlen(name) - 4] = 0;  // cut off ".bsp"

	R_BeginLoading(cl.configstrings[CS_MODELS + 1], atoi(cl.configstrings[CS_MAPSIZE]));
	Cl_LoadProgress(50);

	num_cl_weaponmodels = j = 0;

	// models, including bsp submodels and client weapon models
	for(i = 1; i < MAX_MODELS && cl.configstrings[CS_MODELS + i][0]; i++){
		memset(name, 0, sizeof(name));
		strncpy(name, cl.configstrings[CS_MODELS + i], sizeof(name) - 1);

		if(name[0] == '#'){  // hack to retrieve client weapon models from server
			if(num_cl_weaponmodels < MAX_CLIENTWEAPONMODELS){
				strncpy(cl_weaponmodels[num_cl_weaponmodels], cl.configstrings[CS_MODELS + i] + 1,
						sizeof(cl_weaponmodels[num_cl_weaponmodels]) - 1);
				num_cl_weaponmodels++;
			}
			continue;
		}

		cl.model_draw[i] = R_LoadModel(cl.configstrings[CS_MODELS + i]);
		if(name[0] == '*')
			cl.model_clip[i] = Cm_InlineModel(cl.configstrings[CS_MODELS + i]);
		else
			cl.model_clip[i] = NULL;

		if(++j <= 20)  // nudge loading progress
			Cl_LoadProgress(50 + j);
	}
	Cl_LoadProgress(70);

	// images for the heads up display
	R_FreePics();

	for(i = 1; i < MAX_IMAGES && cl.configstrings[CS_IMAGES + i][0]; i++)
		cl.image_precache[i] = R_LoadPic(cl.configstrings[CS_IMAGES + i]);

	Cl_LoadProgress(75);

	Cl_LoadClientinfo(&cl.baseclientinfo, "newbie\\ichabod/ichabod");

	j = 0;

	// client models and skins
	for(i = 0; i < MAX_CLIENTS; i++){

		if(!cl.configstrings[CS_PLAYERSKINS + i][0])
			continue;

		Cl_ParseClientinfo(i);

		if(++j < 10)
			Cl_LoadProgress(75 + j);
	}
	Cl_LoadProgress(85);

	R_SetSky(cl.configstrings[CS_SKY]);
	Cl_LoadProgress(90);

	// weather and fog effects
	R_ResolveWeather();

	Cl_ClearNotify();

	r_view.ready = r_view.update = true;
}


/*
 * R_Sky_f
 */
static void R_Sky_f(void){

	if(Cmd_Argc() != 2){
		Com_Print("Usage: %s <basename>\n", Cmd_Argv(0));
		return;
	}

	R_SetSky(Cmd_Argv(1));
}


/*
 * R_Reload_f
 */
static void R_Reload_f(void){

	r_view.ready = false;

	cls.loading = 1;

	R_DrawFill(0, 0, r_view.width, r_view.height, 0, 1.0);

	R_ShutdownImages();

	R_InitImages();

	R_InitDraw();

	R_LoadMedia();

	Cvar_ClearVars(CVAR_R_IMAGES);

	cls.loading = 0;
}


/*
 * R_Restart_f
 */
void R_Restart_f(void){
#ifdef _WIN32
	// on win32, we must always recreate the entire context
	R_Shutdown();

	R_Init();

	R_Reload_f();
#else
	// while on linux, we can avoid it for the general case
	if(Cvar_PendingVars(CVAR_R_CONTEXT)){
		R_Shutdown();

		R_Init();

		R_Reload_f();
	}
	else {
		// reset the video mode
		if(Cvar_PendingVars(CVAR_R_MODE))
			R_SetMode();

		// reload the shaders
		if(Cvar_PendingVars(CVAR_R_PROGRAMS)){
			R_ShutdownPrograms();

			R_InitPrograms();
		}

		// reload all renderer media
		if(Cvar_PendingVars(CVAR_R_IMAGES))
			R_Reload_f();
	}
#endif

	Cvar_ClearVars(CVAR_R_MASK);

	r_rendermode->modified = true;

	r_view.update = true;
}


/*
 * R_InitLocal
 */
static void R_InitLocal(void){

	// development tools
	r_clear = Cvar_Get("r_clear", "0", 0, NULL);
	r_cull = Cvar_Get("r_cull", "1", 0, NULL);
	r_deluxemap = Cvar_Get("r_deluxemap", "0", CVAR_R_PROGRAMS, "Activate or deactivate the rendering of deluxemap textures (developer tool)");
	r_lightmap = Cvar_Get("r_lightmap", "0", CVAR_R_PROGRAMS, "Activate or deactivate the rendering of lightmap textures (developer tool)");
	r_lockvis = Cvar_Get("r_lockvis", "0", 0, NULL);
	r_novis = Cvar_Get("r_novis", "0", 0, NULL);
	r_shownormals = Cvar_Get("r_shownormals", "0", 0, "Activate or deactivate the rendering of surface normals (developer tool)");
	r_showpolys = Cvar_Get("r_showpolys", "0", 0, NULL);
	r_speeds = Cvar_Get("r_speeds", "0", 0, NULL);

	// settings and preferences
	r_anisotropy = Cvar_Get("r_anisotropy", "1", CVAR_ARCHIVE, NULL);
	r_brightness = Cvar_Get("r_brightness", "1.5", CVAR_ARCHIVE | CVAR_R_IMAGES, NULL);
	r_bumpmap = Cvar_Get("r_bumpmap", "1.0", CVAR_ARCHIVE | CVAR_R_PROGRAMS, NULL);
	r_contrast = Cvar_Get("r_contrast", "1.0", CVAR_ARCHIVE | CVAR_R_IMAGES, NULL);
	r_coronas = Cvar_Get("r_coronas", "1", CVAR_ARCHIVE, "Activate or deactivate coronas");
	r_drawbuffer = Cvar_Get("r_drawbuffer", "GL_BACK", CVAR_ARCHIVE, NULL);
	r_flares = Cvar_Get("r_flares", "1", CVAR_ARCHIVE, "Activate or deactivate flares");
	r_fog = Cvar_Get("r_fog", "1", CVAR_ARCHIVE | CVAR_R_PROGRAMS, "Activate or deactivate fog");
	r_fullscreen = Cvar_Get("r_fullscreen", "1", CVAR_ARCHIVE | CVAR_R_MODE, "Activate or deactivate fullscreen mode");
	r_gamma = Cvar_Get("r_gamma", "1.0", CVAR_ARCHIVE, NULL);
	r_hardness = Cvar_Get("r_hardness", "1.0", CVAR_ARCHIVE, NULL);
	r_height = Cvar_Get("r_height", "0", CVAR_ARCHIVE | CVAR_R_MODE, NULL);
	r_hunkmegs = Cvar_Get("r_hunkmegs", "128", CVAR_R_CONTEXT, "Memory size for the renderer hunk in megabytes");
	r_invert = Cvar_Get("r_invert", "0", CVAR_ARCHIVE | CVAR_R_IMAGES, NULL);
	r_lightmapsize = Cvar_Get("r_lightmapsize", "1024", CVAR_ARCHIVE | CVAR_R_IMAGES, NULL);
	r_lighting = Cvar_Get("r_lighting", "1", CVAR_ARCHIVE, "Activate or deactivate lighting effects");
	r_lights = Cvar_Get("r_lights", "1", CVAR_ARCHIVE | CVAR_R_PROGRAMS, NULL);
	r_lines = Cvar_Get("r_lines", "0.5", CVAR_ARCHIVE, NULL);
	r_linewidth = Cvar_Get("r_linewidth", "1.0", CVAR_ARCHIVE, NULL);
	r_materials = Cvar_Get("r_materials", "1", CVAR_ARCHIVE, NULL);
	r_modulate = Cvar_Get("r_modulate", "3.0", CVAR_ARCHIVE | CVAR_R_IMAGES, "Controls the brightness of the lightmap");
	r_monochrome = Cvar_Get("r_monochrome", "0", CVAR_ARCHIVE | CVAR_R_IMAGES, NULL);
	r_multisample = Cvar_Get("r_multisample", "0", CVAR_ARCHIVE | CVAR_R_CONTEXT, NULL);
	r_optimize = Cvar_Get("r_optimize", "1", CVAR_ARCHIVE, NULL);
	r_parallax = Cvar_Get("r_parallax", "1.0", CVAR_ARCHIVE, NULL);
	r_programs = Cvar_Get("r_programs", "1", CVAR_ARCHIVE | CVAR_R_PROGRAMS, "Activate or deactivate GLSL shaders");
	r_rendermode = Cvar_Get("r_rendermode", "default", CVAR_ARCHIVE, NULL);
	r_saturation = Cvar_Get("r_saturation", "1.0", CVAR_ARCHIVE | CVAR_R_IMAGES, NULL);
	r_screenshot_type = Cvar_Get("r_screenshot_type", "jpeg", CVAR_ARCHIVE, "Screenshot image format (jpeg or tga)");
	r_screenshot_quality = Cvar_Get("r_screenshot_quality", "0.9", CVAR_ARCHIVE, "Screenshot image quality (jpeg only)");
	r_shadows = Cvar_Get("r_shadows", "1", CVAR_ARCHIVE, NULL);
	r_soften = Cvar_Get("r_soften", "4", CVAR_ARCHIVE | CVAR_R_IMAGES, NULL);
	r_specular = Cvar_Get("r_specular", "1.0", CVAR_ARCHIVE, NULL);
	r_swapinterval = Cvar_Get("r_swapinterval", "0", CVAR_ARCHIVE | CVAR_R_CONTEXT, NULL);
	r_texturemode = Cvar_Get("r_texturemode", "GL_LINEAR_MIPMAP_LINEAR", CVAR_ARCHIVE, NULL);
	r_threads = Cvar_Get("r_threads", "0", CVAR_ARCHIVE, "Activate or deactivate the threaded renderer");
	r_vertexbuffers = Cvar_Get("r_vertexbuffers", "1", CVAR_ARCHIVE, NULL);
	r_warp = Cvar_Get("r_warp", "1", CVAR_ARCHIVE, "Activate or deactivate warping surfaces");
	r_width = Cvar_Get("r_width", "0", CVAR_ARCHIVE | CVAR_R_MODE, NULL);
	r_windowedheight = Cvar_Get("r_windowedheight", "0", CVAR_ARCHIVE | CVAR_R_MODE, NULL);
	r_windowedwidth = Cvar_Get("r_windowedwidth", "0", CVAR_ARCHIVE | CVAR_R_MODE, NULL);

	// prevent unecessary reloading for initial values
	Cvar_ClearVars(CVAR_R_MASK);

	Cmd_AddCommand("r_listmodels", R_ListModels_f, "Print information about all the loaded models to the game console");
	Cmd_AddCommand("r_hunkstats", R_HunkStats_f, "Renderer memory usage information");

	Cmd_AddCommand("r_listimages", R_ListImages_f, "Print information about all the loaded images to the game console");
	Cmd_AddCommand("r_screenshot", R_Screenshot_f, "Take a screenshot");

	Cmd_AddCommand("r_sky", R_Sky_f, NULL);
	Cmd_AddCommand("r_reload", R_Reload_f, NULL);

	Cmd_AddCommand("r_restart", R_Restart_f, "Restart the rendering subsystem");
}


/*
 * R_SetMode
 */
qboolean R_SetMode(void){
	int w, h;

	if(r_fullscreen->value){
		w = r_width->value;
		h = r_height->value;
	}
	else {
		w = r_windowedwidth->value;
		h = r_windowedheight->value;
	}

	return R_InitContext(w, h, r_fullscreen->value);
}


/*
 * R_InitExtensions
 */
static qboolean R_InitExtensions(void){

	// multitexture
	if(strstr(r_config.extensions_string, "GL_ARB_multitexture")){
		qglActiveTexture = SDL_GL_GetProcAddress("glActiveTexture");
		qglClientActiveTexture = SDL_GL_GetProcAddress("glClientActiveTexture");
	}
	else
		Com_Warn("R_InitExtensions: GL_ARB_multitexture not found.\n");

	// vertex buffer objects
	if(strstr(r_config.extensions_string, "GL_ARB_vertex_buffer_object")){
		qglGenBuffers = SDL_GL_GetProcAddress("glGenBuffers");
		qglDeleteBuffers = SDL_GL_GetProcAddress("glDeleteBuffers");
		qglBindBuffer = SDL_GL_GetProcAddress("glBindBuffer");
		qglBufferData = SDL_GL_GetProcAddress("glBufferData");
	}
	else
		Com_Warn("R_InitExtensions: GL_ARB_vertex_buffer_object not found.\n");

	// glsl vertex and fragment shaders and programs
	if(strstr(r_config.extensions_string, "GL_ARB_fragment_shader")){
		qglCreateShader = SDL_GL_GetProcAddress("glCreateShader");
		qglDeleteShader = SDL_GL_GetProcAddress("glDeleteShader");
		qglShaderSource = SDL_GL_GetProcAddress("glShaderSource");
		qglCompileShader = SDL_GL_GetProcAddress("glCompileShader");
		qglGetShaderiv = SDL_GL_GetProcAddress("glGetShaderiv");
		qglGetShaderInfoLog = SDL_GL_GetProcAddress("glGetShaderInfoLog");
		qglCreateProgram = SDL_GL_GetProcAddress("glCreateProgram");
		qglDeleteProgram = SDL_GL_GetProcAddress("glDeleteProgram");
		qglAttachShader = SDL_GL_GetProcAddress("glAttachShader");
		qglDetachShader = SDL_GL_GetProcAddress("glDetachShader");
		qglLinkProgram = SDL_GL_GetProcAddress("glLinkProgram");
		qglUseProgram = SDL_GL_GetProcAddress("glUseProgram");
		qglGetProgramiv = SDL_GL_GetProcAddress("glGetProgramiv");
		qglGetProgramInfoLog = SDL_GL_GetProcAddress("glGetProgramInfoLog");
		qglGetUniformLocation = SDL_GL_GetProcAddress("glGetUniformLocation");
		qglUniform1i = SDL_GL_GetProcAddress("glUniform1i");
		qglUniform1f = SDL_GL_GetProcAddress("glUniform1f");
		qglUniform3fv = SDL_GL_GetProcAddress("glUniform3fv");
		qglUniform4fv = SDL_GL_GetProcAddress("glUniform4fv");
		qglGetAttribLocation = SDL_GL_GetProcAddress("glGetAttribLocation");

		// vertex attribute arrays
		qglEnableVertexAttribArray = SDL_GL_GetProcAddress("glEnableVertexAttribArray");
		qglDisableVertexAttribArray = SDL_GL_GetProcAddress("glDisableVertexAttribArray");
		qglVertexAttribPointer = SDL_GL_GetProcAddress("glVertexAttribPointer");
	}
	else
		Com_Warn("R_InitExtensions: GL_ARB_fragment_shader not found.\n");

	// multitexture is the only one we absolutely need
	if(!qglActiveTexture || !qglClientActiveTexture)
		return false;

	return true;
}


/*
 * R_EnforceVersion
 */
static void R_EnforceVersion(void){
	int maj, min, rel;

	sscanf(r_config.version_string, "%d.%d.%d ", &maj, &min, &rel);

	if(maj > 1)
		return;

	if(maj < 1)
		Com_Error(ERR_FATAL, "OpenGL version %s is less than 1.2.1",
				r_config.version_string);

	if(min > 2)
		return;

	if(min < 2)
		Com_Error(ERR_FATAL, "OpenGL Version %s is less than 1.2.1",
				r_config.version_string);

	if(rel > 1)
		return;

	if(rel < 1)
		Com_Error(ERR_FATAL, "OpenGL version %s is less than 1.2.1",
				r_config.version_string);
}


/*
 * R_Init
 */
void R_Init(void){

	Com_Print("Video initialization..\n");

	memset(&r_state, 0, sizeof(renderer_state_t));

	R_InitLocal();

	// create the window and set up the context
	if(!R_SetMode())
		Com_Error(ERR_FATAL, "Failed to set video mode.");

	r_config.renderer_string = (char *)glGetString(GL_RENDERER);
	r_config.vendor_string = (char *)glGetString(GL_VENDOR);
	r_config.version_string = (char *)glGetString(GL_VERSION);
	r_config.extensions_string = (char *)glGetString(GL_EXTENSIONS);
	glGetIntegerv(GL_MAX_TEXTURE_UNITS, &r_config.max_texunits);

	// initialize any extensions
	if(!R_InitExtensions())
		Com_Error(ERR_FATAL, "Failed to resolve required extensions.");

	Com_Print("  Renderer: ^%d%s\n  Vendor:   ^%d%s\n  Version:  ^%d%s\n",
			CON_COLOR_ALT, r_config.renderer_string, CON_COLOR_ALT,
			r_config.vendor_string, CON_COLOR_ALT, r_config.version_string);

	R_EnforceVersion();

	R_SetDefaultState();

	R_InitPrograms();

	R_InitImages();

	R_InitDraw();

	R_InitModels();

	Com_Print("Video initialized %dx%dx%dbpp %s.\n", r_state.width, r_state.height,
			(r_state.redbits + r_state.greenbits + r_state.bluebits + r_state.alphabits),
			(r_state.fullscreen ? "fullscreen" : "windowed"));
}


/*
 * R_Shutdown
 */
void R_Shutdown(void){

	Cmd_RemoveCommand("r_listmodels");
	Cmd_RemoveCommand("r_listimages");
	Cmd_RemoveCommand("r_screenshot");

	Cmd_RemoveCommand("r_reload");

	Cmd_RemoveCommand("r_restart");

	R_ShutdownModels();

	R_ShutdownImages();

	R_ShutdownPrograms();

	R_ShutdownContext();
}
