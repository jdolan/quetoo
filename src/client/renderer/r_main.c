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

#include "r_local.h"
#include "client.h"

r_view_t r_view;

r_locals_t r_locals;

r_config_t r_config;

cvar_t *r_clear;
cvar_t *r_cull;
cvar_t *r_lock_vis;
cvar_t *r_no_vis;
cvar_t *r_draw_bsp_leafs;
cvar_t *r_draw_bsp_lights;
cvar_t *r_draw_bsp_normals;
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
cvar_t *r_vertex_buffers;
cvar_t *r_warp;
cvar_t *r_width;
cvar_t *r_windowed_height;
cvar_t *r_windowed_width;

void (*R_DrawOpaqueBspSurfaces)(const r_bsp_surfaces_t *surfs);
void (*R_DrawOpaqueWarpBspSurfaces)(const r_bsp_surfaces_t *surfs);
void (*R_DrawAlphaTestBspSurfaces)(const r_bsp_surfaces_t *surfs);
void (*R_DrawBlendBspSurfaces)(const r_bsp_surfaces_t *surfs);
void (*R_DrawBlendWarpBspSurfaces)(const r_bsp_surfaces_t *surfs);
void (*R_DrawBackBspSurfaces)(const r_bsp_surfaces_t *surfs);
void (*R_DrawMeshModel)(const r_entity_t *e);

extern cl_client_t cl;
extern cl_static_t cls;

/*
 * @brief Qsort comparator for render elements.
 */
static int R_SortElements_Compare(const void *e1, const void *e2) {
	return ((r_element_t *) e1)->dist > ((r_element_t *) e2)->dist;
}

/*
 * @brief Sorts the specified elements array by their distance from the origin.
 * Elements are sorted farthest-first so that they are rendered back-to-front.
 */
void R_SortElements(r_element_t *elements, size_t len) {
	r_element_t *e = elements;
	size_t i;

	for (i = 0; i < len; i++, e++) {
		vec3_t delta;

		switch (e->type) {
			case ELEMENT_PARTICLE:
				VectorSubtract(((r_particle_t *)e->element)->org, r_view.origin, delta);
				break;
			default:
				VectorSet(delta, 0.0, 0.0, 0.0);
				break;
		}

		e->dist = VectorLength(delta);
	}

	qsort(elements, len, sizeof(r_element_t), R_SortElements_Compare);
}

/*
 * @brief Updates the clipping planes for the view frustum based on the origin
 * and angles for this frame.
 */
void R_UpdateFrustum(void) {
	int32_t i;

	if (!r_cull->value)
		return;

	// rotate r_view.forward right by fov_x / 2 degrees
	RotatePointAroundVector(r_locals.frustum[0].normal, r_view.up, r_view.forward,
			-(90 - r_view.fov[0] / 2));
	// rotate r_view.forward left by fov_x / 2 degrees
	RotatePointAroundVector(r_locals.frustum[1].normal, r_view.up, r_view.forward,
			90 - r_view.fov[0] / 2);
	// rotate r_view.forward up by fov_x / 2 degrees
	RotatePointAroundVector(r_locals.frustum[2].normal, r_view.right, r_view.forward,
			90 - r_view.fov[1] / 2);
	// rotate r_view.forward down by fov_x / 2 degrees
	RotatePointAroundVector(r_locals.frustum[3].normal, r_view.right, r_view.forward,
			-(90 - r_view.fov[1] / 2));

	for (i = 0; i < 4; i++) {
		r_locals.frustum[i].type = PLANE_ANYZ;
		r_locals.frustum[i].dist = DotProduct(r_view.origin, r_locals.frustum[i].normal);
		r_locals.frustum[i].sign_bits = SignBitsForPlane(&r_locals.frustum[i]);
	}
}

/*
 * @brief Main entry point for drawing the scene (world and entities).
 */
void R_DrawView(void) {

	R_UpdateFrustum();

	R_UpdateVis();

	R_MarkBspSurfaces();

	R_EnableFog(true);

	R_DrawSkyBox();

	// wait for the client to populate our lights array
	Thread_Wait(&r_view.thread);

	// now dispatch another thread to cull entities while we draw the world
	r_view.thread = Thread_Create(R_CullEntities, NULL);

	R_MarkLights();

	const r_sorted_bsp_surfaces_t *surfs = R_WorldModel()->bsp->sorted_surfaces;

	R_DrawOpaqueBspSurfaces(&surfs->opaque);

	R_DrawOpaqueWarpBspSurfaces(&surfs->opaque_warp);

	R_DrawAlphaTestBspSurfaces(&surfs->alpha_test);

	R_EnableBlend(true);

	R_DrawBackBspSurfaces(&surfs->back);

	R_DrawMaterialBspSurfaces(&surfs->material);

	R_DrawFlareBspSurfaces(&surfs->flare);

	R_EnableBlend(false);

	R_DrawBspNormals();

	// ensure the thread has finished culling entities
	Thread_Wait(&r_view.thread);

	// now dispatch another thread to update particles while we draw entities
	r_view.thread = Thread_Create(R_UpdateParticles, NULL);

	R_DrawEntities();

	R_EnableBlend(true);

	R_DrawBspLights();

	R_DrawBlendBspSurfaces(&surfs->blend);

	R_DrawBlendWarpBspSurfaces(&surfs->blend_warp);

	// ensure the thread has finished updating particles
	Thread_Wait(&r_view.thread);

	R_DrawParticles();

	R_EnableFog(false);

	R_DrawCoronas();

	R_DrawBspLeafs();

	R_EnableBlend(false);

	R_ResetArrayState();
}

/*
 * @brief Assigns surface and entity rendering function pointers for the
 * r_render_mode plugin framework.
 */
static void R_RenderMode(const char *mode) {

	r_view.render_mode = RENDER_MODEL_DEFAULT;

	R_DrawOpaqueBspSurfaces = R_DrawOpaqueBspSurfaces_default;
	R_DrawOpaqueWarpBspSurfaces = R_DrawOpaqueWarpBspSurfaces_default;
	R_DrawAlphaTestBspSurfaces = R_DrawAlphaTestBspSurfaces_default;
	R_DrawBlendBspSurfaces = R_DrawBlendBspSurfaces_default;
	R_DrawBlendWarpBspSurfaces = R_DrawBlendWarpBspSurfaces_default;
	R_DrawBackBspSurfaces = R_DrawBackBspSurfaces_default;

	R_DrawMeshModel = R_DrawMeshModel_default;

	if (!mode || !*mode)
		return;

	// assign function pointers to different plugins here
}

/*
 * @brief Clears the frame buffer.
 */
static void R_Clear(void) {
	int32_t bits;

	bits = GL_DEPTH_BUFFER_BIT;

	// clear the stencil bit if shadows are enabled
	if (r_shadows->value)
		bits |= GL_STENCIL_BUFFER_BIT;

	// clear the color buffer if desired or necessary
	if (r_clear->value || r_draw_wireframe->value || r_view.x || r_view.y)
		bits |= GL_COLOR_BUFFER_BIT;

	// or if the client is not fully loaded
	if (cls.state != CL_ACTIVE || cls.loading)
		bits |= GL_COLOR_BUFFER_BIT;

	glClear(bits);
}

/*
 * @brief
 */
void R_BeginFrame(void) {

	// draw buffer stuff
	if (r_draw_buffer->modified) {
		if (!strcasecmp(r_draw_buffer->string, "GL_FRONT"))
			glDrawBuffer(GL_FRONT);
		else
			glDrawBuffer(GL_BACK);
		r_draw_buffer->modified = false;
	}

	// texture mode stuff
	if (r_texture_mode->modified || r_anisotropy->modified) {
		R_TextureMode(r_texture_mode->string);
		r_texture_mode->modified = r_anisotropy->modified = false;
	}

	// render mode stuff
	if (r_render_mode->modified) {
		R_RenderMode(r_render_mode->string);
		r_render_mode->modified = false;
	}

	R_Clear();
}

/*
 * @brief
 */
void R_EndFrame(void) {

	R_UpdateCapture();

	if (cls.state == CL_ACTIVE && !cls.loading) {
		r_view.update = false;

		R_FreeMaterials();

		R_FreeModels();

		R_FreeImages();
	}

	SDL_GL_SwapBuffers(); // swap buffers
}

/*
 * @brief Wipes the view structures after waiting for the view thread.
 */
static void R_InitView(void) {

	memset(&r_view, 0, sizeof(r_view));

	R_RenderMode(r_render_mode->string);

	memset(&r_locals, 0, sizeof(r_locals));
}

/*
 * @brief Parses the weather config_string for weather and fog definitions,
 * e.g. "rain fog 0.8 0.75 0.65".
 */
static void R_ResolveWeather(void) {
	char *weather, *c;
	int32_t err;

	r_view.weather = WEATHER_NONE;
	r_view.fog_color[3] = 1.0;

	VectorSet(r_view.fog_color, 0.75, 0.75, 0.75);

	weather = cl.config_strings[CS_WEATHER];

	if (!weather || *weather == '\0')
		return;

	if (strstr(weather, "rain"))
		r_view.weather |= WEATHER_RAIN;

	if (strstr(weather, "snow"))
		r_view.weather |= WEATHER_SNOW;

	if ((c = strstr(weather, "fog"))) {

		r_view.weather |= WEATHER_FOG;
		err = -1;

		if (strlen(c) > 3) // try to parse fog color
			err = sscanf(c + 4, "%f %f %f", &r_view.fog_color[0], &r_view.fog_color[1],
					&r_view.fog_color[2]);

		if (err != 3) // default to gray
			VectorSet(r_view.fog_color, 0.75, 0.75, 0.75);
	}
}

/*
 * @brief Iterate the config_strings, loading all renderer-specific media.
 */
void R_LoadMedia(void) {
	int32_t i, j;

	if (!cl.config_strings[CS_MODELS + 1][0])
		return; // no map specified

	R_InitView();

	Cl_LoadProgress(1);

	R_BeginLoading(cl.config_strings[CS_MODELS + 1], atoi(cl.config_strings[CS_BSP_SIZE]));
	Cl_LoadProgress(50);

	j = 0;

	// models, including bsp submodels and client weapon models
	for (i = 1; i < MAX_MODELS && cl.config_strings[CS_MODELS + i][0]; i++) {

		cl.model_draw[i] = R_LoadModel(cl.config_strings[CS_MODELS + i]);

		const char *s = cl.config_strings[CS_MODELS + i];
		if (*s == '*')
			cl.model_clip[i] = Cm_Model(cl.config_strings[CS_MODELS + i]);
		else
			cl.model_clip[i] = NULL;

		if (++j <= 20) // bump loading progress
			Cl_LoadProgress(50 + j);
	}
	Cl_LoadProgress(70);

	// images for the heads up display
	R_FreePics();

	for (i = 1; i < MAX_IMAGES && cl.config_strings[CS_IMAGES + i][0]; i++)
		cl.image_precache[i] = R_LoadPic(cl.config_strings[CS_IMAGES + i]);
	Cl_LoadProgress(75);

	// sky environment map
	R_SetSky(cl.config_strings[CS_SKY]);
	Cl_LoadProgress(77);

	// weather and fog effects
	R_ResolveWeather();
	Cl_LoadProgress(79);

	R_FreeModels();

	R_FreeMaterials();

	R_FreeImages();

	r_view.update = true;
}

/*
 * @brief
 */
static void R_Sky_f(void) {

	if (Cmd_Argc() != 2) {
		Com_Print("Usage: %s <basename>\n", Cmd_Argv(0));
		return;
	}

	R_SetSky(Cmd_Argv(1));
}


/*
 * @brief Restarts the renderer subsystem. The OpenGL context is discarded and
 * recreated. All media is reloaded.
 */
void R_Restart_f(void) {

	R_Shutdown();

	R_Init();

	R_LoadMedia();

	Cvar_ClearVars(CVAR_R_MASK);

	r_render_mode->modified = true;

	r_view.update = true;
}

/*
 * @brief
 */
static void R_ToggleFullscreen_f(void) {

	Cvar_Toggle("r_fullscreen");

	R_Restart_f();
}

/*
 * @brief
 */
static void R_InitLocal(void) {

	// development tools
	r_clear
			= Cvar_Get("r_clear", "0", 0, "Controls screen clearing at each frame (developer tool)");
	r_cull = Cvar_Get("r_cull", "1", CVAR_LO_ONLY,
			"Controls bounded box culling routines (developer tool)");
	r_lock_vis = Cvar_Get("r_lock_vis", "0", CVAR_LO_ONLY,
			"Temporarily locks the PVS lookup for world surfaces (developer tool)");
	r_no_vis = Cvar_Get("r_no_vis", "0", CVAR_LO_ONLY,
			"Disables PVS refresh and lookup for world surfaces (developer tool)");
	r_draw_bsp_leafs = Cvar_Get("r_draw_bsp_leafs", "0", CVAR_LO_ONLY,
			"Controls the rendering of BSP leafs (developer tool)");
	r_draw_bsp_lights = Cvar_Get("r_draw_bsp_lights", "0", CVAR_LO_ONLY,
			"Controls the rendering of static BSP light sources (developer tool)");
	r_draw_bsp_normals = Cvar_Get("r_draw_bsp_normals", "0", CVAR_LO_ONLY,
			"Controls the rendering of BSP surface normals (developer tool)");
	r_draw_wireframe = Cvar_Get("r_draw_wireframe", "0", CVAR_LO_ONLY,
			"Controls the rendering of polygons as wireframe (developer tool)");

	// settings and preferences
	r_anisotropy = Cvar_Get("r_anisotropy", "1", CVAR_ARCHIVE,
			"Controls anisotropic texture filtering");
	r_brightness = Cvar_Get("r_brightness", "1.0", CVAR_ARCHIVE | CVAR_R_IMAGES,
			"Controls texture brightness");
	r_bumpmap = Cvar_Get("r_bumpmap", "1.0", CVAR_ARCHIVE | CVAR_R_IMAGES,
			"Controls the intensity of bump-mapping effects");
	r_capture = Cvar_Get("r_capture", "0", 0, "Toggle screen capturing to jpeg files");
	r_capture_fps
			= Cvar_Get("r_capture_fps", "25", 0, "The desired framerate for screen capturing");
	r_capture_quality = Cvar_Get("r_capture_quality", "0.7", CVAR_ARCHIVE,
			"Screen capturing image quality");
	r_contrast = Cvar_Get("r_contrast", "1.0", CVAR_ARCHIVE | CVAR_R_IMAGES,
			"Controls texture contrast");
	r_coronas = Cvar_Get("r_coronas", "1", CVAR_ARCHIVE, "Controls the rendering of coronas");
	r_draw_buffer = Cvar_Get("r_draw_buffer", "GL_BACK", CVAR_ARCHIVE, NULL);
	r_flares = Cvar_Get("r_flares", "1.0", CVAR_ARCHIVE,
			"Controls the rendering of light source flares");
	r_fog = Cvar_Get("r_fog", "1", CVAR_ARCHIVE, "Controls the rendering of fog effects");
	r_fullscreen = Cvar_Get("r_fullscreen", "1", CVAR_ARCHIVE | CVAR_R_CONTEXT,
			"Controls fullscreen mode");
	r_gamma = Cvar_Get("r_gamma", "1.0", CVAR_ARCHIVE | CVAR_R_CONTEXT,
			"Controls video gamma (brightness)");
	r_hardness = Cvar_Get("r_hardness", "1.0", CVAR_ARCHIVE,
			"Controls the hardness of bump-mapping effects");
	r_height = Cvar_Get("r_height", "0", CVAR_ARCHIVE | CVAR_R_CONTEXT, NULL);
	r_hunk_mb = Cvar_Get("r_hunk_mb", "512", CVAR_R_CONTEXT,
			"Memory size for the renderer hunk in megabytes");
	r_invert = Cvar_Get("r_invert", "0", CVAR_ARCHIVE | CVAR_R_IMAGES,
			"Inverts the RGB values of all world textures");
	r_lightmap_block_size = Cvar_Get("r_lightmap_block_size", "4096", CVAR_ARCHIVE | CVAR_R_IMAGES,
			NULL);
	r_lighting = Cvar_Get("r_lighting", "1.0", CVAR_ARCHIVE,
			"Controls intensity of hardware lighting effects");
	r_line_alpha = Cvar_Get("r_line_alpha", "0.5", CVAR_ARCHIVE, NULL);
	r_line_width = Cvar_Get("r_line_width", "1.0", CVAR_ARCHIVE, NULL);
	r_materials = Cvar_Get("r_materials", "1", CVAR_ARCHIVE,
			"Enables or disables the materials (progressive texture effects) system");
	r_modulate = Cvar_Get("r_modulate", "3.0", CVAR_ARCHIVE | CVAR_R_IMAGES,
			"Controls the brightness of world surface lightmaps");
	r_monochrome = Cvar_Get("r_monochrome", "0", CVAR_ARCHIVE | CVAR_R_IMAGES,
			"Loads all world textures as monochrome");
	r_multisample = Cvar_Get("r_multisample", "0", CVAR_ARCHIVE | CVAR_R_CONTEXT,
			"Controls multisampling (anti-aliasing)");
	r_optimize = Cvar_Get("r_optimize", "1", CVAR_ARCHIVE,
			"Controls BSP recursion optimization strategy");
	r_parallax = Cvar_Get("r_parallax", "1.0", CVAR_ARCHIVE,
			"Controls the intensity of parallax mapping effects");
	r_programs = Cvar_Get("r_programs", "1", CVAR_ARCHIVE, "Controls GLSL shaders");
	r_render_mode = Cvar_Get("r_render_mode", "default", CVAR_ARCHIVE,
			"Specifies the active renderer plugin (default or pro)");
	r_saturation = Cvar_Get("r_saturation", "1.0", CVAR_ARCHIVE | CVAR_R_IMAGES,
			"Controls texture saturation");
	r_screenshot_type = Cvar_Get("r_screenshot_type", "jpeg", CVAR_ARCHIVE,
			"Screenshot image format (jpeg or tga)");
	r_screenshot_quality = Cvar_Get("r_screenshot_quality", "0.9", CVAR_ARCHIVE,
			"Screenshot image quality (jpeg only)");
	r_shadows = Cvar_Get("r_shadows", "1", CVAR_ARCHIVE,
			"Controls the rendering of mesh model shadows");
	r_soften = Cvar_Get("r_soften", "4", CVAR_ARCHIVE | CVAR_R_IMAGES,
			"Controls lightmap softening");
	r_specular = Cvar_Get("r_specular", "1.0", CVAR_ARCHIVE,
			"Controls the specularity of bump-mapping effects");
	r_swap_interval = Cvar_Get("r_swap_interval", "0", CVAR_ARCHIVE | CVAR_R_CONTEXT,
			"Controls vertical refresh synchronization (v-sync)");
	r_texture_mode = Cvar_Get("r_texture_mode", "GL_LINEAR_MIPMAP_LINEAR", CVAR_ARCHIVE,
			"Specifies the active texture filtering mode");
	r_vertex_buffers = Cvar_Get("r_vertex_buffers", "1", CVAR_ARCHIVE,
			"Controls the use of vertex buffer objects (VBO)");
	r_warp = Cvar_Get("r_warp", "1", CVAR_ARCHIVE, "Controls warping surface effects (e.g. water)");
	r_width = Cvar_Get("r_width", "0", CVAR_ARCHIVE | CVAR_R_CONTEXT, NULL);
	r_windowed_height = Cvar_Get("r_windowed_height", "0", CVAR_ARCHIVE | CVAR_R_CONTEXT, NULL);
	r_windowed_width = Cvar_Get("r_windowed_width", "0", CVAR_ARCHIVE | CVAR_R_CONTEXT, NULL);

	// prevent unnecessary reloading for initial values
	Cvar_ClearVars(CVAR_R_MASK);

	Cmd_AddCommand("r_list_models", R_ListModels_f, 0,
			"Print information about all the loaded models to the game console");

	Cmd_AddCommand("r_list_images", R_ListImages_f, 0,
			"Print information about all the loaded images to the game console");
	Cmd_AddCommand("r_screenshot", R_Screenshot_f, CMD_SYSTEM, "Take a screenshot");

	Cmd_AddCommand("r_sky", R_Sky_f, 0, NULL);

	Cmd_AddCommand("r_toggle_fullscreen", R_ToggleFullscreen_f, CMD_SYSTEM, "Toggle fullscreen");

	Cmd_AddCommand("r_restart", R_Restart_f, 0, "Restart the rendering subsystem");
}

/*
 * @brief
 */
static void R_InitConfig(void) {

	memset(&r_config, 0, sizeof(r_config));

	r_config.renderer_string = (char *) glGetString(GL_RENDERER);
	r_config.vendor_string = (char *) glGetString(GL_VENDOR);
	r_config.version_string = (char *) glGetString(GL_VERSION);
	r_config.extensions_string = (char *) glGetString(GL_EXTENSIONS);

	glGetIntegerv(GL_MAX_TEXTURE_UNITS, &r_config.max_texunits);
	glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &r_config.max_teximage_units);

	Com_Print("  Renderer: ^2%s^7\n", r_config.renderer_string);
	Com_Print("  Vendor:   ^2%s^7\n", r_config.vendor_string);
	Com_Print("  Version:  ^2%s^7\n", r_config.version_string);
}

/*
 * @brief Creates the OpenGL context and initializes all GL state.
 */
void R_Init(void) {

	Com_Print("Video initialization...\n");

	R_InitLocal();

	R_InitContext();

	R_InitConfig();

	R_EnforceGlVersion();

	R_InitGlExtensions();

	R_InitState();

	R_InitPrograms();

	R_InitImages();

	R_InitDraw();

	R_InitMaterials();

	R_InitModels();

	R_InitCapture();

	R_InitView();

	Com_Print(
			"Video initialized %dx%dx%dbpp %s.\n",
			r_context.width,
			r_context.height,
			(r_context.red_bits + r_context.green_bits + r_context.blue_bits + r_context.alpha_bits),
			(r_context.fullscreen ? "fullscreen" : "windowed"));
}

/*
 * @brief
 */
void R_Shutdown(void) {

	Cmd_RemoveCommand("r_list_models");

	Cmd_RemoveCommand("r_list_images");
	Cmd_RemoveCommand("r_screenshot");

	Cmd_RemoveCommand("r_sky");

	Cmd_RemoveCommand("r_toggle_fullscreen");

	Cmd_RemoveCommand("r_restart");

	R_ShutdownCapture();

	R_ShutdownPrograms();

	R_ShutdownModels();

	R_ShutdownMaterials();

	R_ShutdownImages();

	R_ShutdownContext();

	R_ShutdownState();

	Z_FreeTag(Z_TAG_RENDERER);
}
