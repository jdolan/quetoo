/*
 * Copyright(c) 1997-2001 Id Software, Inc.
 * Copyright(c) 2002 The Quakeforge Project.
 * Copyright(c) 2006-2011 Quake2World.
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

r_view_t r_view;

r_locals_t r_locals;

r_config_t r_config;

byte color_white[4] = {255, 255, 255, 255};
byte color_black[4] = {0, 0, 0, 128};

cvar_t *r_clear;
cvar_t *r_cull;
cvar_t *r_draw_deluxemaps;
cvar_t *r_draw_lightmaps;
cvar_t *r_lock_vis;
cvar_t *r_no_vis;
cvar_t *r_draw_bsp_lights;
cvar_t *r_draw_bsp_normals;
cvar_t *r_draw_poly_counts;
cvar_t *r_draw_wireframe;

cvar_t *r_anisotropy;
cvar_t *r_brightness;
cvar_t *r_bumpmap;
cvar_t *r_capture;
cvar_t *r_capture_fps;
cvar_t *r_capture_quality;
cvar_t *r_contrast;
cvar_t *r_coronas;
cvar_t *r_draw_buffer;
cvar_t *r_flares;
cvar_t *r_fog;
cvar_t *r_fullscreen;
cvar_t *r_gamma;
cvar_t *r_hardness;
cvar_t *r_height;
cvar_t *r_hunk_mb;
cvar_t *r_invert;
cvar_t *r_lightmap_block_size;
cvar_t *r_lighting;
cvar_t *r_line_alpha;
cvar_t *r_line_width;
cvar_t *r_materials;
cvar_t *r_modulate;
cvar_t *r_monochrome;
cvar_t *r_multisample;
cvar_t *r_optimize;
cvar_t *r_parallax;
cvar_t *r_programs;
cvar_t *r_render_mode;
cvar_t *r_saturation;
cvar_t *r_screenshot_type;
cvar_t *r_screenshot_quality;
cvar_t *r_shadows;
cvar_t *r_soften;
cvar_t *r_specular;
cvar_t *r_swap_interval;
cvar_t *r_texture_mode;
cvar_t *r_threads;
cvar_t *r_vertex_buffers;
cvar_t *r_warp;
cvar_t *r_width;
cvar_t *r_windowed_height;
cvar_t *r_windowed_width;

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

void (*R_DrawOpaqueSurfaces)(r_bsp_surfaces_t *surfs);
void (*R_DrawOpaqueWarpSurfaces)(r_bsp_surfaces_t *surfs);
void (*R_DrawAlphaTestSurfaces)(r_bsp_surfaces_t *surfs);
void (*R_DrawBlendSurfaces)(r_bsp_surfaces_t *surfs);
void (*R_DrawBlendWarpSurfaces)(r_bsp_surfaces_t *surfs);
void (*R_DrawBackSurfaces)(r_bsp_surfaces_t *surfs);
void (*R_DrawMeshModel)(r_entity_t *e);


/*
 * R_WordspawnValue
 *
 * Parses values from the worldspawn entity definition.
 */
const char *R_WorldspawnValue(const char *key){
	const char *c, *v;

	c = strstr(Cm_EntityString(), va("\"%s\"", key));

	if(c){
		v = Com_Parse(&c);  // parse the key itself
		v = Com_Parse(&c);  // parse the value
	}
	else {
		v = NULL;
	}

	return v;
}


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

	r_locals.trace_num++;

	if(r_locals.trace_num < 0)  // avoid overflows
		r_locals.trace_num = 0;

	VectorSet(mins, -size, -size, -size);
	VectorSet(maxs, size, size, size);

	// check world
	r_view.trace = Cm_BoxTrace(start, end, mins, maxs, r_world_model->first_node, mask);
	r_view.trace_ent = NULL;

	frac = r_view.trace.fraction;

	// check bsp models
	for(i = 0; i < r_view.num_entities; i++){

		r_entity_t *ent = &r_view.entities[i];
		const r_model_t *m = ent->model;

		if(!m || m->type != mod_bsp_submodel)
			continue;

		tr = Cm_TransformedBoxTrace(start, end, mins, maxs, m->first_node,
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
static inline int R_SignbitsForPlane(const c_plane_t *plane){
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
		r_locals.frustum[i].sign_bits = R_SignbitsForPlane(&r_locals.frustum[i]);
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


/*
 * R_DrawFrame
 */
void R_DrawFrame(void){

	R_GetError();

	if(r_bsp_thread.state > THREAD_DEAD){
		R_WaitForThread(&r_bsp_thread);
	}
	else {
		R_UpdateFrustum();

		R_MarkLeafs();

		R_MarkSurfaces();
	}

	R_MarkLights();

	R_EnableFog(true);

	R_DrawSkyBox();

	R_DrawOpaqueSurfaces(r_world_model->opaque_surfaces);

	R_DrawOpaqueWarpSurfaces(r_world_model->opaque_warp_surfaces);

	R_DrawAlphaTestSurfaces(r_world_model->alpha_test_surfaces);

	R_EnableBlend(true);

	R_DrawBackSurfaces(r_world_model->back_surfaces);

	R_DrawMaterialSurfaces(r_world_model->material_surfaces);

	R_DrawFlareSurfaces(r_world_model->flare_surfaces);

	R_EnableBlend(false);

	R_DrawBspNormals();

	R_DrawEntities();

	R_EnableBlend(true);

	R_DrawBspLights();

	R_DrawBlendSurfaces(r_world_model->blend_surfaces);

	R_DrawBlendWarpSurfaces(r_world_model->blend_warp_surfaces);

	R_DrawParticles();

	R_EnableFog(false);

	R_DrawCoronas();

	R_EnableBlend(false);

	R_ResetArrayState();
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
static void R_Clear(void){
	int bits;

	bits = GL_DEPTH_BUFFER_BIT;

	// clear the stencil bit if shadows are enabled
	if(r_shadows->value)
		bits |= GL_STENCIL_BUFFER_BIT;

	// clear the color buffer if desired or necessary
	if(r_clear->value || r_draw_wireframe->value || r_view.x || r_view.y || !r_view.ready)
		bits |= GL_COLOR_BUFFER_BIT;

	glClear(bits);
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
	if(r_draw_buffer->modified){
		if(!strcasecmp(r_draw_buffer->string, "GL_FRONT"))
			glDrawBuffer(GL_FRONT);
		else
			glDrawBuffer(GL_BACK);
		r_draw_buffer->modified = false;
	}

	// texture mode stuff
	if(r_texture_mode->modified || r_anisotropy->modified){
		R_TextureMode(r_texture_mode->string);
		r_texture_mode->modified = r_anisotropy->modified = false;
	}

	// render mode stuff
	if(r_render_mode->modified){
		R_RenderMode(r_render_mode->string);
		r_render_mode->modified = false;
	}

	// threads
	if(r_threads->modified){
		R_UpdateThreads(r_threads->integer);
		r_threads->modified = false;
	}

	R_Clear();
}


/*
 * R_EndFrame
 */
void R_EndFrame(void){

	if(r_capture_thread.state > THREAD_DEAD){
		R_WaitForThread(&r_capture_thread);

		R_UpdateCapture();

		r_capture_thread.state = THREAD_RUN;
	}
	else {
		R_UpdateCapture();

		R_FlushCapture();
	}

	SDL_GL_SwapBuffers();  // swap buffers
}


/*
 * R_InitView
 */
void R_InitView(void) {

	memset(&r_view, 0, sizeof(r_view));
	memset(&r_locals, 0, sizeof(r_locals));
}


/*
 * R_ResolveWeather
 *
 * Parses the weather config_string for weather and fog definitions,
 * e.g. "rain fog 0.8 0.75 0.65".
 */
static void R_ResolveWeather(void){
	char *weather, *c;
	int err;

	r_view.weather = WEATHER_NONE;
	r_view.fog_color[3] = 1.0;

	VectorSet(r_view.fog_color, 0.75, 0.75, 0.75);

	weather = cl.config_strings[CS_WEATHER];

	if(!weather || *weather == '\0')
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
 *
 * Iterate the config_strings, loading all renderer-specific media.
 */
void R_LoadMedia(void){
	char name[MAX_QPATH];
	int i, j;

	if(!cl.config_strings[CS_MODELS + 1][0])
		return;  // no map loaded

	Cl_LoadProgress(0);

	R_InitView();

	R_FreeImages();

	strncpy(name, cl.config_strings[CS_MODELS + 1] + 5, sizeof(name) - 1);  // skip "maps/"
	name[strlen(name) - 4] = 0;  // cut off ".bsp"

	R_BeginLoading(cl.config_strings[CS_MODELS + 1], atoi(cl.config_strings[CS_BSP_SIZE]));
	Cl_LoadProgress(50);

	j = 0;

	// models, including bsp submodels and client weapon models
	for(i = 1; i < MAX_MODELS && cl.config_strings[CS_MODELS + i][0]; i++){

		memset(name, 0, sizeof(name));

		strncpy(name, cl.config_strings[CS_MODELS + i], sizeof(name) - 1);

		cl.model_draw[i] = R_LoadModel(cl.config_strings[CS_MODELS + i]);

		if(name[0] == '*')
			cl.model_clip[i] = Cm_Model(cl.config_strings[CS_MODELS + i]);
		else
			cl.model_clip[i] = NULL;

		if(++j <= 20)  // bump loading progress
			Cl_LoadProgress(50 + j);
	}
	Cl_LoadProgress(70);

	// images for the heads up display
	R_FreePics();

	for(i = 1; i < MAX_IMAGES && cl.config_strings[CS_IMAGES + i][0]; i++)
		cl.image_precache[i] = R_LoadPic(cl.config_strings[CS_IMAGES + i]);

	Cl_LoadProgress(75);

	j = 0;

	// client models and skins
	for(i = 0; i < MAX_CLIENTS; i++){

		if(!cl.config_strings[CS_PLAYER_SKINS + i][0])
			continue;

		Cl_ParseClientInfo(i);

		if(++j < 10)
			Cl_LoadProgress(75 + j);
	}
	Cl_LoadProgress(85);

	R_SetSky(cl.config_strings[CS_SKY]);
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
 *
 * Restarts any "dirty" portions of the renderer subsystem.  On OSX and
 * Windows, this implies recreating the entire GL context, while Linux allows
 * us to only refresh the parts we need to.
 */
void R_Restart_f(void){
#if defined(__APPLE__) || defined(_WIN32)
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

	r_render_mode->modified = true;

	r_view.update = true;
}


/*
 * R_InitLocal
 */
static void R_InitLocal(void){

	// development tools
	r_clear = Cvar_Get("r_clear", "0", 0, "Controls screen clearing at each frame (developer tool)");
	r_cull = Cvar_Get("r_cull", "1", 0, "Controls bounded box culling routines (developer tool)");
	r_lock_vis = Cvar_Get("r_lock_vis", "0", 0, "Temporarily locks the PVS lookup for world surfaces (developer tool)");
	r_no_vis = Cvar_Get("r_no_vis", "0", 0, "Disables PVS refresh and lookup for world surfaces (developer tool)");
	r_draw_bsp_lights = Cvar_Get("r_draw_bsp_lights", "0", 0, "Controls the rendering of static BSP light sources (developer tool)");
	r_draw_bsp_normals = Cvar_Get("r_draw_bsp_normals", "0", 0, "Controls the rendering of surface normals (developer tool)");
	r_draw_deluxemaps = Cvar_Get("r_draw_bsp_deluxemaps", "0", CVAR_R_PROGRAMS, "Controls the rendering of deluxemap textures (developer tool)");
	r_draw_lightmaps = Cvar_Get("r_draw_bsp_lightmaps", "0", CVAR_R_PROGRAMS, "Controls the rendering of lightmap textures (developer tool)");
	r_draw_poly_counts = Cvar_Get("r_draw_poly_counts", "0", 0, "Controls the rendering of polygon counts per frame (developer tool)");
	r_draw_wireframe = Cvar_Get("r_draw_wireframe", "0", 0, "Controls the rendering of polygons as wireframe (developer tool)");

	// settings and preferences
	r_anisotropy = Cvar_Get("r_anisotropy", "1", CVAR_ARCHIVE, "Controls anisotropic texture filtering");
	r_brightness = Cvar_Get("r_brightness", "1.0", CVAR_ARCHIVE | CVAR_R_IMAGES, "Controls texture brightness");
	r_bumpmap = Cvar_Get("r_bumpmap", "1.0", CVAR_ARCHIVE | CVAR_R_PROGRAMS, "Controls the intensity of bump-mapping effects");
	r_capture = Cvar_Get("r_capture", "0", 0, "Toggle screen capturing to jpeg files");
	r_capture_fps = Cvar_Get("r_capture_fps", "25", 0, "The desired framerate for screen capturing");
	r_capture_quality = Cvar_Get("r_capture_quality", "0.7", CVAR_ARCHIVE, "Screen capturing image quality");
	r_contrast = Cvar_Get("r_contrast", "1.0", CVAR_ARCHIVE | CVAR_R_IMAGES, "Controls texture contrast");
	r_coronas = Cvar_Get("r_coronas", "1", CVAR_ARCHIVE, "Controls the rendering of coronas");
	r_draw_buffer = Cvar_Get("r_draw_buffer", "GL_BACK", CVAR_ARCHIVE, NULL);
	r_flares = Cvar_Get("r_flares", "1.0", CVAR_ARCHIVE, "Controls the rendering of light source flares");
	r_fog = Cvar_Get("r_fog", "1", CVAR_ARCHIVE | CVAR_R_PROGRAMS, "Controls the rendering of fog effects");
	r_fullscreen = Cvar_Get("r_fullscreen", "1", CVAR_ARCHIVE | CVAR_R_MODE, "Controls fullscreen mode");
	r_gamma = Cvar_Get("r_gamma", "1.0", CVAR_ARCHIVE, "Controls video gamma (brightness)");
	r_hardness = Cvar_Get("r_hardness", "1.0", CVAR_ARCHIVE, "Controls the hardness of bump-mapping effects");
	r_height = Cvar_Get("r_height", "0", CVAR_ARCHIVE | CVAR_R_MODE, NULL);
	r_hunk_mb = Cvar_Get("r_hunk_mb", "512", CVAR_R_CONTEXT, "Memory size for the renderer hunk in megabytes");
	r_invert = Cvar_Get("r_invert", "0", CVAR_ARCHIVE | CVAR_R_IMAGES, "Inverts the RGB values of all world textures");
	r_lightmap_block_size = Cvar_Get("r_lightmap_block_size", "4096", CVAR_ARCHIVE | CVAR_R_IMAGES, NULL);
	r_lighting = Cvar_Get("r_lighting", "1.0", CVAR_ARCHIVE, "Controls intensity of hardware lighting effects");
	r_line_alpha = Cvar_Get("r_line_alpha", "0.5", CVAR_ARCHIVE, NULL);
	r_line_width = Cvar_Get("r_line_width", "1.0", CVAR_ARCHIVE, NULL);
	r_materials = Cvar_Get("r_materials", "1", CVAR_ARCHIVE, "Enables or disables the materials (progressive texture effects) system");
	r_modulate = Cvar_Get("r_modulate", "3.0", CVAR_ARCHIVE | CVAR_R_IMAGES, "Controls the brightness of world surface lightmaps");
	r_monochrome = Cvar_Get("r_monochrome", "0", CVAR_ARCHIVE | CVAR_R_IMAGES, "Loads all world textures as monochrome");
	r_multisample = Cvar_Get("r_multisample", "0", CVAR_ARCHIVE | CVAR_R_CONTEXT, "Controls multisampling (anti-aliasing)");
	r_optimize = Cvar_Get("r_optimize", "1", CVAR_ARCHIVE, "Controls BSP recursion optimization strategy");
	r_parallax = Cvar_Get("r_parallax", "1.0", CVAR_ARCHIVE, "Controls the intensity of parallax mapping effects");
	r_programs = Cvar_Get("r_programs", "1", CVAR_ARCHIVE | CVAR_R_PROGRAMS, "Controls GLSL shaders");
	r_render_mode = Cvar_Get("r_render_mode", "default", CVAR_ARCHIVE, "Specifies the active renderer plugin (default or pro)");
	r_saturation = Cvar_Get("r_saturation", "1.0", CVAR_ARCHIVE | CVAR_R_IMAGES, "Controls texture saturation");
	r_screenshot_type = Cvar_Get("r_screenshot_type", "jpeg", CVAR_ARCHIVE, "Screenshot image format (jpeg or tga)");
	r_screenshot_quality = Cvar_Get("r_screenshot_quality", "0.9", CVAR_ARCHIVE, "Screenshot image quality (jpeg only)");
	r_shadows = Cvar_Get("r_shadows", "1", CVAR_ARCHIVE, "Controls the rendering of mesh model shadows");
	r_soften = Cvar_Get("r_soften", "4", CVAR_ARCHIVE | CVAR_R_IMAGES, "Controls lightmap softening");
	r_specular = Cvar_Get("r_specular", "1.0", CVAR_ARCHIVE, "Controls the specularity of bump-mapping effects");
	r_swap_interval = Cvar_Get("r_swap_interval", "0", CVAR_ARCHIVE | CVAR_R_CONTEXT, "Controls vertical refresh synchronization (v-sync)");
	r_texture_mode = Cvar_Get("r_texture_mode", "GL_LINEAR_MIPMAP_LINEAR", CVAR_ARCHIVE, "Specifies the active texture filtering mode");
	r_threads = Cvar_Get("r_threads", "0", CVAR_ARCHIVE, "Controls the threaded renderer");
	r_vertex_buffers = Cvar_Get("r_vertex_buffers", "1", CVAR_ARCHIVE, "Controls the use of vertex buffer objects (VBO)");
	r_warp = Cvar_Get("r_warp", "1", CVAR_ARCHIVE, "Controls warping surface effects (e.g. water)");
	r_width = Cvar_Get("r_width", "0", CVAR_ARCHIVE | CVAR_R_MODE, NULL);
	r_windowed_height = Cvar_Get("r_windowed_height", "0", CVAR_ARCHIVE | CVAR_R_MODE, NULL);
	r_windowed_width = Cvar_Get("r_windowed_width", "0", CVAR_ARCHIVE | CVAR_R_MODE, NULL);

	// prevent unnecessary reloading for initial values
	Cvar_ClearVars(CVAR_R_MASK);

	Cmd_AddCommand("r_list_models", R_ListModels_f, "Print information about all the loaded models to the game console");
	Cmd_AddCommand("r_hunk_stats", R_HunkStats_f, "Renderer memory usage information");

	Cmd_AddCommand("r_list_images", R_ListImages_f, "Print information about all the loaded images to the game console");
	Cmd_AddCommand("r_screenshot", R_Screenshot_f, "Take a screenshot");

	Cmd_AddCommand("r_sky", R_Sky_f, NULL);

	Cmd_AddCommand("r_reload", R_Reload_f, "Reloads all rendering media");
	Cmd_AddCommand("r_restart", R_Restart_f, "Restart the rendering subsystem");
}


/*
 * R_SetMode
 */
qboolean R_SetMode(void){
	int w, h;

	if(r_fullscreen->value){
		w = r_width->integer;
		h = r_height->integer;
	}
	else {
		w = r_windowed_width->integer;
		h = r_windowed_height->integer;
	}

	return R_InitContext(w, h, r_fullscreen->value);
}


/*
 * R_InitConfig
 */
static void R_InitConfig(void){

	memset(&r_config, 0, sizeof(r_config));

	r_config.renderer_string = (char *)glGetString(GL_RENDERER);
	r_config.vendor_string = (char *)glGetString(GL_VENDOR);
	r_config.version_string = (char *)glGetString(GL_VERSION);
	r_config.extensions_string = (char *)glGetString(GL_EXTENSIONS);
	glGetIntegerv(GL_MAX_TEXTURE_UNITS, &r_config.max_texunits);

	Com_Print("  Renderer: ^2%s^7\n", r_config.renderer_string);
	Com_Print("  Vendor:   ^2%s^7\n", r_config.vendor_string);
	Com_Print("  Version:  ^2%s^7\n", r_config.version_string);
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
 * R_Init
 */
void R_Init(void){

	Com_Print("Video initialization...\n");

	R_InitLocal();

	R_InitState();

	// create the window and set up the context
	if(!R_SetMode())
		Com_Error(ERR_FATAL, "Failed to set video mode.");

	R_InitConfig();

	R_EnforceVersion();

	// initialize any extensions
	if(!R_InitExtensions())
		Com_Error(ERR_FATAL, "Failed to resolve required extensions.");

	R_InitThreads();

	R_SetDefaultState();

	R_InitPrograms();

	R_InitImages();

	R_InitDraw();

	R_InitModels();

	R_InitCapture();

	Com_Print("Video initialized %dx%dx%dbpp %s.\n", r_state.width, r_state.height,
			(r_state.red_bits + r_state.green_bits + r_state.blue_bits + r_state.alpha_bits),
			(r_state.fullscreen ? "fullscreen" : "windowed"));
}


/*
 * R_Shutdown
 */
void R_Shutdown(void){

	Cmd_RemoveCommand("r_list_models");
	Cmd_RemoveCommand("r_hunk_stats");

	Cmd_RemoveCommand("r_list_images");
	Cmd_RemoveCommand("r_screenshot");

	Cmd_RemoveCommand("r_sky");

	Cmd_RemoveCommand("r_reload");
	Cmd_RemoveCommand("r_restart");

	R_ShutdownCapture();

	R_ShutdownModels();

	R_ShutdownImages();

	R_ShutdownPrograms();

	R_ShutdownThreads();

	R_ShutdownContext();
}
