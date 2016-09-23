/*
 * Copyright(c) 1997-2001 id Software, Inc.
 * Copyright(c) 2002 The Quakeforge Project.
 * Copyright(c) 2006-2011 Quetoo.
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
cvar_t *r_draw_bsp_lightmaps;
cvar_t *r_draw_bsp_lights;
cvar_t *r_draw_bsp_normals;
cvar_t *r_draw_entity_bounds;
cvar_t *r_draw_wireframe;

cvar_t *r_allow_high_dpi;
cvar_t *r_anisotropy;
cvar_t *r_brightness;
cvar_t *r_bumpmap;
cvar_t *r_contrast;
cvar_t *r_coronas;
cvar_t *r_draw_buffer;
cvar_t *r_flares;
cvar_t *r_fog;
cvar_t *r_fullscreen;
cvar_t *r_gamma;
cvar_t *r_hardness;
cvar_t *r_height;
cvar_t *r_invert;
cvar_t *r_lightmap_block_size;
cvar_t *r_lighting;
cvar_t *r_line_alpha;
cvar_t *r_line_width;
cvar_t *r_materials;
cvar_t *r_modulate;
cvar_t *r_monochrome;
cvar_t *r_multisample;
cvar_t *r_parallax;
cvar_t *r_programs;
cvar_t *r_render_plugin;
cvar_t *r_saturation;
cvar_t *r_screenshot_quality;
cvar_t *r_shadows;
cvar_t *r_shell;
cvar_t *r_specular;
cvar_t *r_swap_interval;
cvar_t *r_texture_mode;
cvar_t *r_vertex_buffers;
cvar_t *r_warp;
cvar_t *r_width;
cvar_t *r_windowed_height;
cvar_t *r_windowed_width;

// render mode function pointers
BspSurfacesDrawFunc R_DrawOpaqueBspSurfaces;
BspSurfacesDrawFunc R_DrawOpaqueWarpBspSurfaces;
BspSurfacesDrawFunc R_DrawAlphaTestBspSurfaces;
BspSurfacesDrawFunc R_DrawBlendBspSurfaces;
BspSurfacesDrawFunc R_DrawBlendWarpBspSurfaces;
BspSurfacesDrawFunc R_DrawBackBspSurfaces;

MeshModelsDrawFunc R_DrawMeshModels;

extern cl_client_t cl;
extern cl_static_t cls;

/**
 * @brief Updates the clipping planes for the view frustum based on the origin
 * and angles for this frame.
 */
void R_UpdateFrustum(void) {
	int32_t i;

	if (!r_cull->value)
		return;

	cm_bsp_plane_t *p = r_locals.frustum;

	const vec_t fov_x = r_view.fov[0];
	const vec_t fov_y = r_view.fov[1];

	// rotate r_view.forward right by fov_x degrees
	RotatePointAroundVector(r_view.forward, r_view.right, -(90.0 - fov_x), (p++)->normal);
	// rotate r_view.forward left by fov_x degrees
	RotatePointAroundVector(r_view.forward, r_view.right, 90.0 - fov_x, (p++)->normal);
	// rotate r_view.forward up by fov_y degrees
	RotatePointAroundVector(r_view.forward, r_view.up, 90.0 - fov_y, (p++)->normal);
	// rotate r_view.forward down by fov_y degrees
	RotatePointAroundVector(r_view.forward, r_view.up, -(90.0 - fov_y), p->normal);

	for (i = 0; i < 4; i++) {
		r_locals.frustum[i].type = PLANE_ANY_Z;
		r_locals.frustum[i].dist = DotProduct(r_view.origin, r_locals.frustum[i].normal);
		r_locals.frustum[i].sign_bits = Cm_SignBitsForPlane(&r_locals.frustum[i]);
	}
}

/**
 * @brief Draws all developer tools towards the end of the frame.
 */
static void R_DrawDeveloperTools(void) {

	R_DrawBspNormals();

	R_DrawBspLights();

	R_DrawBspLeafs();
}

/**
 * @brief Main entry point for drawing the scene (world and entities).
 */
void R_DrawView(void) {

	R_UpdateFrustum();

	R_UpdateVis();

	R_MarkBspSurfaces();

	R_EnableFog(true);

	R_DrawSkyBox();

	// wait for the client to fully populate the scene
	Thread_Wait(r_view.thread);

	// dispatch threads to cull entities and sort elements while we draw the world
	thread_t *cull_entities = Thread_Create(R_CullEntities, NULL);
	thread_t *sort_elements = Thread_Create(R_SortElements, NULL);

	R_MarkLights();

	const r_sorted_bsp_surfaces_t *surfs = r_model_state.world->bsp->sorted_surfaces;

	R_DrawOpaqueBspSurfaces(&surfs->opaque);

	R_DrawOpaqueWarpBspSurfaces(&surfs->opaque_warp);

	R_DrawAlphaTestBspSurfaces(&surfs->alpha_test);

	R_EnableBlend(true);

	R_DrawBackBspSurfaces(&surfs->back);

	R_DrawMaterialBspSurfaces(&surfs->material);

	R_DrawFlareBspSurfaces(&surfs->flare);

	R_EnableBlend(false);

	// wait for entity culling to complete
	Thread_Wait(cull_entities);

	R_DrawEntities();

	R_EnableBlend(true);

	// wait for element sorting to complete
	Thread_Wait(sort_elements);

	R_DrawElements();

	R_EnableFog(false);

	R_DrawDeveloperTools();

	R_DrawCoronas();

	R_EnableBlend(false);

	R_ResetArrayState();

#if 0
	vec3_t tmp;
	VectorMA(r_view.origin, MAX_WORLD_DIST, r_view.forward, tmp);

	cm_trace_t tr = Cl_Trace(r_view.origin, tmp, NULL, NULL, cl.client_num + 1, MASK_SOLID);
	if (tr.fraction > 0.0 && tr.fraction < 1.0) {
		Com_Print("%s: %d: %s\n", tr.surface->name, tr.plane.num, vtos(tr.plane.normal));
	}

#endif
}

/**
 * @brief Assigns surface and entity rendering function pointers for the
 * r_render_plugin plugin framework.
 */
static void R_RenderPlugin(const char *plugin) {

	r_view.plugin = R_PLUGIN_DEFAULT;

	R_DrawOpaqueBspSurfaces = R_DrawOpaqueBspSurfaces_default;
	R_DrawOpaqueWarpBspSurfaces = R_DrawOpaqueWarpBspSurfaces_default;
	R_DrawAlphaTestBspSurfaces = R_DrawAlphaTestBspSurfaces_default;
	R_DrawBlendBspSurfaces = R_DrawBlendBspSurfaces_default;
	R_DrawBlendWarpBspSurfaces = R_DrawBlendWarpBspSurfaces_default;
	R_DrawBackBspSurfaces = R_DrawBackBspSurfaces_default;

	R_DrawMeshModels = R_DrawMeshModels_default;

	if (!plugin || !*plugin)
		return;

	// assign function pointers to different renderer paths here
}

/**
 * @brief Clears the frame buffer.
 */
static void R_Clear(void) {

	GLbitfield bits = GL_DEPTH_BUFFER_BIT;

	// clear the stencil bit if shadows are enabled
	if (r_shadows->value)
		bits |= GL_STENCIL_BUFFER_BIT;

	// clear the color buffer if requested
	if (r_clear->value || r_draw_wireframe->value)
		bits |= GL_COLOR_BUFFER_BIT;

	// or if the viewport does not occupy the entire context
	if (r_view.viewport.x || r_view.viewport.y)
		bits |= GL_COLOR_BUFFER_BIT;

	// or if the client is not fully loaded
	if (cls.state != CL_ACTIVE)
		bits |= GL_COLOR_BUFFER_BIT;

	// of if the client is no-clipping around the world
	if (cl.frame.ps.pm_state.type == PM_SPECTATOR)
		bits |= GL_COLOR_BUFFER_BIT;

	glClear(bits);
}

/**
 * @brief
 */
void R_BeginFrame(void) {

	// draw buffer stuff
	if (r_draw_buffer->modified) {
		if (!g_ascii_strcasecmp(r_draw_buffer->string, "GL_FRONT"))
			glDrawBuffer(GL_FRONT);
		else
			glDrawBuffer(GL_BACK);
		r_draw_buffer->modified = false;
	}

	// render plugin stuff
	if (r_render_plugin->modified) {
		R_RenderPlugin(r_render_plugin->string);
		r_render_plugin->modified = false;
	}

	R_Clear();
}

/**
 * @brief Called at the end of each video frame to swap buffers. Also, if the
 * loading cycle has completed, media is freed here.
 */
void R_EndFrame(void) {

	if (cls.state == CL_ACTIVE) {

		if (r_view.update) {
			r_view.update = false;
			R_FreeMedia();
		}
	}

	SDL_GL_SwapWindow(r_context.window);
}

/**
 * @brief Initializes the view and locals structures so that loading may begin.
 */
void R_InitView(void) {

	memset(&r_view, 0, sizeof(r_view));

	R_RenderPlugin(r_render_plugin->string);

	memset(&r_locals, 0, sizeof(r_locals));

	r_locals.clusters[0] = r_locals.clusters[1] = -1;
}

/**
 * @brief Loads all media for the renderer subsystem.
 */
void R_LoadMedia(void) {

	if (!cl.config_strings[CS_MODELS][0]) {
		return; // no map specified
	}

	R_InitView();

	R_BeginLoading();

	Cl_LoadingProgress(0, cl.config_strings[CS_MODELS]);

	R_LoadModel(cl.config_strings[CS_MODELS]); // load the world

	Cl_LoadingProgress(60, "world");

	// load all other models
	for (uint32_t i = 1; i < MAX_MODELS && cl.config_strings[CS_MODELS + i][0]; i++) {

		cl.model_precache[i] = R_LoadModel(cl.config_strings[CS_MODELS + i]);

		if (i <= 30) // bump loading progress
			Cl_LoadingProgress(60 + (i / 3), cl.config_strings[CS_MODELS + i]);
	}

	Cl_LoadingProgress(70, "models");

	// load all known images
	for (uint32_t i = 0; i < MAX_IMAGES && cl.config_strings[CS_IMAGES + i][0]; i++) {
		cl.image_precache[i] = R_LoadImage(cl.config_strings[CS_IMAGES + i], IT_PIC);
	}

	Cl_LoadingProgress(75, "images");

	// sky environment map
	R_SetSky(cl.config_strings[CS_SKY]);

	Cl_LoadingProgress(77, "sky");

	r_render_plugin->modified = true;

	r_view.update = true;
}

/**
 * @brief Restarts the renderer subsystem. The OpenGL context is discarded and
 * recreated. All media is reloaded. Other subsystems can elect to refresh
 * their media references by inspecting r_view.update.
 */
void R_Restart_f(void) {

	if (cls.state == CL_LOADING) {
		return;
	}

	R_Shutdown();

	R_Init();

	const cl_state_t state = cls.state;

	if (cls.state == CL_ACTIVE) {
		cls.state = CL_LOADING;
	}

	R_LoadMedia();

	cls.state = state;
}

/**
 * @brief Toggles fullscreen vs windowed mode.
 */
static void R_ToggleFullscreen_f(void) {

	Cvar_Toggle("r_fullscreen");

	R_Restart_f();
}

/**
 * @brief Initializes console variables and commands for the renderer.
 */
static void R_InitLocal(void) {

	// development tools
	r_clear = Cvar_Get("r_clear", "0", 0, "Controls buffer clearing (developer tool)");
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
	r_draw_bsp_lightmaps = Cvar_Get("r_draw_bsp_lightmaps", "0", CVAR_LO_ONLY,
			"Controls the rendering of BSP lightmap textures (developer tool)");
	r_draw_bsp_normals = Cvar_Get("r_draw_bsp_normals", "0", CVAR_LO_ONLY,
			"Controls the rendering of BSP surface normals (developer tool)");
	r_draw_entity_bounds = Cvar_Get("r_draw_entitiy_bounds", "0", CVAR_LO_ONLY,
			"Controls the rendering of entity bounding boxes (developer tool)");
	r_draw_wireframe = Cvar_Get("r_draw_wireframe", "0", CVAR_LO_ONLY,
			"Controls the rendering of polygons as wireframe (developer tool)");

	// settings and preferences
	r_allow_high_dpi = Cvar_Get("r_allow_high_dpi", "0", CVAR_ARCHIVE | CVAR_R_CONTEXT,
			"Enables or disables support for High-DPI (Retina, 4K) display modes");
	r_anisotropy = Cvar_Get("r_anisotropy", "1", CVAR_ARCHIVE | CVAR_R_MEDIA,
			"Controls anisotropic texture filtering");
	r_brightness = Cvar_Get("r_brightness", "1.0", CVAR_ARCHIVE | CVAR_R_MEDIA,
			"Controls texture brightness");
	r_bumpmap = Cvar_Get("r_bumpmap", "1.0", CVAR_ARCHIVE | CVAR_R_MEDIA,
			"Controls the intensity of bump-mapping effects");
	r_contrast = Cvar_Get("r_contrast", "1.0", CVAR_ARCHIVE | CVAR_R_MEDIA,
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
	r_invert = Cvar_Get("r_invert", "0", CVAR_ARCHIVE | CVAR_R_MEDIA,
			"Inverts the RGB values of all world textures");
	r_lightmap_block_size = Cvar_Get("r_lightmap_block_size", "4096", CVAR_ARCHIVE | CVAR_R_MEDIA,
			NULL);
	r_lighting = Cvar_Get("r_lighting", "1.0", CVAR_ARCHIVE | CVAR_R_MEDIA,
			"Controls intensity of lighting effects");
	r_line_alpha = Cvar_Get("r_line_alpha", "0.5", CVAR_ARCHIVE, NULL);
	r_line_width = Cvar_Get("r_line_width", "1.0", CVAR_ARCHIVE, NULL);
	r_materials = Cvar_Get("r_materials", "1", CVAR_ARCHIVE,
			"Enables or disables the materials (progressive texture effects) system");
	r_modulate = Cvar_Get("r_modulate", "3.0", CVAR_ARCHIVE | CVAR_R_MEDIA,
			"Controls the brightness of world surface lightmaps");
	r_monochrome = Cvar_Get("r_monochrome", "0", CVAR_ARCHIVE | CVAR_R_MEDIA,
			"Loads all world textures as monochrome");
	r_multisample = Cvar_Get("r_multisample", "0", CVAR_ARCHIVE | CVAR_R_CONTEXT,
			"Controls multisampling (anti-aliasing)");
	r_parallax = Cvar_Get("r_parallax", "1.0", CVAR_ARCHIVE,
			"Controls the intensity of parallax mapping effects");
	r_programs = Cvar_Get("r_programs", "1", CVAR_ARCHIVE, "Controls GLSL shaders");
	r_render_plugin = Cvar_Get("r_render_plugin", "default", CVAR_ARCHIVE,
			"Specifies the active renderer plugin (default or pro)");
	r_saturation = Cvar_Get("r_saturation", "1.0", CVAR_ARCHIVE | CVAR_R_MEDIA,
			"Controls texture saturation");
	r_screenshot_quality = Cvar_Get("r_screenshot_quality", "95", CVAR_ARCHIVE,
			"Screenshot image quality (JPEG compression 0 - 100)");
	r_shadows = Cvar_Get("r_shadows", "7", CVAR_ARCHIVE | CVAR_R_MEDIA,
			"Controls the rendering of mesh model shadows");
	r_shell = Cvar_Get("r_shell", "1", CVAR_ARCHIVE,
			"Controls mesh shell effect (e.g. Quad Damage shell)");
	r_specular = Cvar_Get("r_specular", "1.0", CVAR_ARCHIVE,
			"Controls the specularity of bump-mapping effects");
	r_swap_interval = Cvar_Get("r_swap_interval", "0", CVAR_ARCHIVE | CVAR_R_CONTEXT,
			"Controls vertical refresh synchronization (v-sync)");
	r_texture_mode = Cvar_Get("r_texture_mode", "GL_LINEAR_MIPMAP_LINEAR",
			CVAR_ARCHIVE | CVAR_R_MEDIA, "Specifies the active texture filtering mode");
	r_vertex_buffers = Cvar_Get("r_vertex_buffers", "1", CVAR_ARCHIVE,
			"Controls the use of vertex buffer objects (VBO)");
	r_warp = Cvar_Get("r_warp", "1", CVAR_ARCHIVE, "Controls warping surface effects (e.g. water)");
	r_width = Cvar_Get("r_width", "0", CVAR_ARCHIVE | CVAR_R_CONTEXT, NULL);
	r_windowed_height = Cvar_Get("r_windowed_height", "1024", CVAR_ARCHIVE | CVAR_R_CONTEXT, NULL);
	r_windowed_width = Cvar_Get("r_windowed_width", "768", CVAR_ARCHIVE | CVAR_R_CONTEXT, NULL);

	Cvar_ClearAll(CVAR_R_MASK);

	Cmd_Add("r_list_media", R_ListMedia_f, CMD_RENDERER, "List all currently loaded media");
	Cmd_Add("r_save_materials", R_SaveMaterials_f, CMD_RENDERER,
			"Save the current materials properties to disk");
	Cmd_Add("r_screenshot", R_Screenshot_f, CMD_SYSTEM | CMD_RENDERER, "Take a screenshot");
	Cmd_Add("r_sky", R_Sky_f, CMD_RENDERER, NULL);
	Cmd_Add("r_toggle_fullscreen", R_ToggleFullscreen_f, CMD_SYSTEM | CMD_RENDERER,
			"Toggle fullscreen");
	Cmd_Add("r_restart", R_Restart_f, CMD_RENDERER, "Restart the rendering subsystem");
}

/**
 * @brief Populates the GL config structure by querying the implementation.
 */
static void R_InitConfig(void) {

	memset(&r_config, 0, sizeof(r_config));

	r_config.renderer_string = (const char *) glGetString(GL_RENDERER);
	r_config.vendor_string = (const char *) glGetString(GL_VENDOR);
	r_config.version_string = (const char *) glGetString(GL_VERSION);
	r_config.extensions_string = (const char *) glGetString(GL_EXTENSIONS);

	glGetIntegerv(GL_MAX_TEXTURE_UNITS, &r_config.max_texunits);
	glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &r_config.max_teximage_units);

	Com_Print("  Renderer: ^2%s^7\n", r_config.renderer_string);
	Com_Print("  Vendor:   ^2%s^7\n", r_config.vendor_string);
	Com_Print("  Version:  ^2%s^7\n", r_config.version_string);
}

/**
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

	R_InitMedia();

	R_InitImages();

	R_InitDraw();

	R_InitModels();

	R_InitView();

	Com_Print("Video initialized %dx%d %s\n", r_context.width, r_context.height,
			(r_context.fullscreen ? "fullscreen" : "windowed"));
}

/**
 * @brief Shuts down the renderer, freeing all resources belonging to it,
 * including the GL context.
 */
void R_Shutdown(void) {

	Cmd_RemoveAll(CMD_RENDERER);

	R_ShutdownMedia();

	R_ShutdownPrograms();

	R_ShutdownContext();

	R_ShutdownState();

	Mem_FreeTag(MEM_TAG_RENDERER);
}
