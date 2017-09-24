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
cvar_t *r_caustics;
cvar_t *r_contrast;
cvar_t *r_deluxemap;
cvar_t *r_draw_buffer;
cvar_t *r_flares;
cvar_t *r_fog;
cvar_t *r_fullscreen;
cvar_t *r_gamma;
cvar_t *r_hardness;
cvar_t *r_height;
cvar_t *r_invert;
cvar_t *r_lighting;
cvar_t *r_line_alpha;
cvar_t *r_line_width;
cvar_t *r_materials;
cvar_t *r_max_lights;
cvar_t *r_modulate;
cvar_t *r_monochrome;
cvar_t *r_multisample;
cvar_t *r_parallax;
cvar_t *r_render_plugin;
cvar_t *r_saturation;
cvar_t *r_screenshot_format;
cvar_t *r_shadows;
cvar_t *r_shell;
cvar_t *r_specular;
cvar_t *r_stainmaps;
cvar_t *r_supersample;
cvar_t *r_texture_mode;
cvar_t *r_swap_interval;
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

	if (!r_cull->value) {
		return;
	}

	cm_bsp_plane_t *p = r_locals.frustum;

	vec_t ang = Radians(r_view.fov[0]);
	vec_t xs = sin(ang);
	vec_t xc = cos(ang);

	VectorScale(r_view.forward, xs, p[0].normal);
	VectorMA(p[0].normal, xc, r_view.right, p[0].normal);

	VectorScale(r_view.forward, xs, p[1].normal);
	VectorMA(p[1].normal, -xc, r_view.right, p[1].normal);

	ang = Radians(r_view.fov[1]);
	xs = sin(ang);
	xc = cos(ang);

	VectorScale(r_view.forward, xs, p[2].normal);
	VectorMA(p[2].normal, xc, r_view.up, p[2].normal);

	VectorScale(r_view.forward, xs, p[3].normal);
	VectorMA(p[3].normal, -xc, r_view.up, p[3].normal);

	for (int32_t i = 0; i < 4; i++) {
		p[i].type = PLANE_ANY_Z;
		p[i].dist = DotProduct (r_view.origin, p[i].normal);
		p[i].sign_bits = Cm_SignBitsForPlane(&p[i]);
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

	// add stains first, since world uses the stainmap
	R_AddStains();

	R_UpdateFrustum();

	R_UpdateVis();

	R_MarkBspSurfaces();

	R_DrawSkyBox();

	R_AddSustainedLights();

	R_AddFlares();

	R_CullEntities();

	R_MarkLights();

	thread_t *sort_elements = Thread_Create(R_SortElements, NULL);

	const r_sorted_bsp_surfaces_t *surfs = r_model_state.world->bsp->sorted_surfaces;

	R_DrawOpaqueBspSurfaces(&surfs->opaque);

	R_DrawOpaqueWarpBspSurfaces(&surfs->opaque_warp);

	R_DrawAlphaTestBspSurfaces(&surfs->alpha_test);

	R_EnableBlend(true);

	R_EnableDepthMask(false);

	R_DrawBackBspSurfaces(&surfs->back);

	R_DrawMaterialBspSurfaces(&surfs->material);

	R_EnableBlend(false);

	R_EnableDepthMask(true);

	R_DrawEntities();

	R_EnableBlend(true);

	R_EnableDepthMask(false);

	// wait for element sorting to complete
	Thread_Wait(sort_elements);

	R_DrawElements();

	R_DrawDeveloperTools();

	R_EnableBlend(false);

	R_EnableDepthMask(true);

	R_ResetArrayState();

#if 0
	vec3_t tmp;
	VectorMA(r_view.origin, MAX_WORLD_DIST, r_view.forward, tmp);

	cm_trace_t tr = Cl_Trace(r_view.origin, tmp, NULL, NULL, 0, MASK_SOLID);
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

	if (!plugin || !*plugin) {
		return;
	}

	// assign function pointers to different renderer paths here
}

/**
 * @brief Clears the frame buffer.
 */
static void R_Clear(void) {

	GLbitfield bits = GL_DEPTH_BUFFER_BIT;

	// clear the stencil bit if shadows are enabled
	if (r_shadows->value) {
		bits |= GL_STENCIL_BUFFER_BIT;
	}

	// clear the color buffer if requested
	if (r_clear->value || r_draw_wireframe->value) {
		bits |= GL_COLOR_BUFFER_BIT;
	}

	// or if the viewport does not occupy the entire context
	if (r_view.viewport.x || r_view.viewport.y) {
		bits |= GL_COLOR_BUFFER_BIT;
	}

	// or if the client is not fully loaded
	if (cls.state != CL_ACTIVE) {
		bits |= GL_COLOR_BUFFER_BIT;
	}

	// or if the client is no-clipping around the world
	if (r_view.contents & CONTENTS_SOLID) {
		bits |= GL_COLOR_BUFFER_BIT;
	}

	glClear(bits);
}

/**
 * @brief
 */
void R_BeginFrame(void) {

	// draw buffer stuff
	if (r_draw_buffer->modified) {
		if (!g_ascii_strcasecmp(r_draw_buffer->string, "GL_FRONT")) {
			glDrawBuffer(GL_FRONT);
		} else {
			glDrawBuffer(GL_BACK);
		}
		r_draw_buffer->modified = false;
	}

	// render plugin stuff
	if (r_render_plugin->modified) {
		R_RenderPlugin(r_render_plugin->string);
		r_render_plugin->modified = false;
	}

	if (r_state.supersample_fb) {
		R_Clear();
		R_BindFramebuffer(r_state.supersample_fb);
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

	Matrix4x4_FromOrtho(&r_view.matrix_base_2d, 0.0, r_context.width, r_context.height, 0.0, -1.0, 1.0);

	Matrix4x4_FromOrtho(&r_view.matrix_base_ui, 0.0, r_context.window_width, r_context.window_height, 0.0, -1.0, 1.0);
}

/**
 * @brief Loads all media for the renderer subsystem.
 */
void R_LoadMedia(void) {

	if (!cl.config_strings[CS_MODELS][0]) {
		return; // no map specified
	}

	R_InitView();

	Cl_LoadingProgress(0, cl.config_strings[CS_MODELS]);

	R_BeginLoading();

	Cl_LoadingProgress(5, "world");

	R_LoadModel(cl.config_strings[CS_MODELS]); // load the world

	Cl_LoadingProgress(55, "mopping up blood");

	R_ResetStainmap(); // clear the stainmap if we have to

	Cl_LoadingProgress(60, "models");

	// load all other models
	for (uint32_t i = 1; i < MAX_MODELS && cl.config_strings[CS_MODELS + i][0]; i++) {

		if (i <= 30) { // bump loading progress
			Cl_LoadingProgress(60 + (i / 3), cl.config_strings[CS_MODELS + i]);
		}

		cl.model_precache[i] = R_LoadModel(cl.config_strings[CS_MODELS + i]);
	}

	Cl_LoadingProgress(75, "images");

	// load all known images
	for (uint32_t i = 0; i < MAX_IMAGES && cl.config_strings[CS_IMAGES + i][0]; i++) {
		cl.image_precache[i] = R_LoadImage(cl.config_strings[CS_IMAGES + i], IT_PIC);
	}

	Cl_LoadingProgress(77, "sky");

	// sky environment map
	R_SetSky(cl.config_strings[CS_SKY]);

	r_render_plugin->modified = true;

	r_view.update = true;
}

/**
 * @brief Restarts the renderer subsystem. The OpenGL context is discarded and
 * recreated. All media is reloaded. Other subsystems can elect to refresh
 * their media references by inspecting r_view.update.
 */
static void R_Restart_f(void) {

	if (cls.state == CL_LOADING) {
		return;
	}

	R_Shutdown();

	R_Init();

	const cl_state_t state = cls.state;

	if (cls.state == CL_ACTIVE) {
		cls.state = CL_LOADING;
	}

	cls.loading.percent = 0;
	cls.cgame->UpdateLoading(cls.loading);

	R_LoadMedia();

	cls.loading.percent = 100;
	cls.cgame->UpdateLoading(cls.loading);

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
 * @brief Resets the stainmap.
 */
static void R_ResetStainmap_f(void) {
	R_ResetStainmap();
}

/**
 * @brief Initializes console variables and commands for the renderer.
 */
static void R_InitLocal(void) {

	// development tools
	r_clear = Cvar_Add("r_clear", "0", 0, "Controls buffer clearing (developer tool)");
	r_cull = Cvar_Add("r_cull", "1", CVAR_DEVELOPER, "Controls bounded box culling routines (developer tool)");
	r_lock_vis = Cvar_Add("r_lock_vis", "0", CVAR_DEVELOPER, "Temporarily locks the PVS lookup for world surfaces (developer tool)");
	r_no_vis = Cvar_Add("r_no_vis", "0", CVAR_DEVELOPER, "Disables PVS refresh and lookup for world surfaces (developer tool)");
	r_draw_bsp_leafs = Cvar_Add("r_draw_bsp_leafs", "0", CVAR_DEVELOPER, "Controls the rendering of BSP leafs (developer tool)");
	r_draw_bsp_lights = Cvar_Add("r_draw_bsp_lights", "0", CVAR_DEVELOPER, "Controls the rendering of static BSP light sources (developer tool)");
	r_draw_bsp_lightmaps = Cvar_Add("r_draw_bsp_lightmaps", "0", CVAR_DEVELOPER, "Controls the rendering of BSP lightmap textures (developer tool)");
	r_draw_bsp_normals = Cvar_Add("r_draw_bsp_normals", "0", CVAR_DEVELOPER, "Controls the rendering of BSP surface normals (developer tool)");
	r_draw_entity_bounds = Cvar_Add("r_draw_entity_bounds", "0", CVAR_DEVELOPER, "Controls the rendering of entity bounding boxes (developer tool)");
	r_draw_wireframe = Cvar_Add("r_draw_wireframe", "0", CVAR_DEVELOPER, "Controls the rendering of polygons as wireframe (developer tool)");

	// settings and preferences
	r_allow_high_dpi = Cvar_Add("r_allow_high_dpi", "0", CVAR_ARCHIVE | CVAR_R_CONTEXT, "Enables or disables support for High-DPI (Retina, 4K) display modes");
	r_anisotropy = Cvar_Add("r_anisotropy", "4", CVAR_ARCHIVE | CVAR_R_MEDIA, "Controls anisotropic texture filtering");
	r_brightness = Cvar_Add("r_brightness", "1.0", CVAR_ARCHIVE | CVAR_R_MEDIA, "Controls texture brightness");
	r_bumpmap = Cvar_Add("r_bumpmap", "1.0", CVAR_ARCHIVE | CVAR_R_MEDIA, "Controls the intensity of bump-mapping effects");
	r_caustics = Cvar_Add("r_caustics", "1.0", CVAR_ARCHIVE | CVAR_R_MEDIA, "Enable or disable liquid caustic effects");
	r_contrast = Cvar_Add("r_contrast", "1.0", CVAR_ARCHIVE | CVAR_R_MEDIA, "Controls texture contrast");
	r_deluxemap = Cvar_Add("r_deluxemap", "1", CVAR_ARCHIVE, "Controls deluxemap rendering");
	r_draw_buffer = Cvar_Add("r_draw_buffer", "GL_BACK", CVAR_ARCHIVE, NULL);
	r_flares = Cvar_Add("r_flares", "1.0", CVAR_ARCHIVE, "Controls the rendering of light source flares");
	r_fog = Cvar_Add("r_fog", "1", CVAR_ARCHIVE, "Controls the rendering of fog effects");
	r_fullscreen = Cvar_Add("r_fullscreen", "2", CVAR_ARCHIVE | CVAR_R_CONTEXT, "Controls fullscreen mode. 1 = exclusive, 2 = borderless");
	r_gamma = Cvar_Add("r_gamma", "1.0", CVAR_ARCHIVE | CVAR_R_CONTEXT, "Controls video gamma (brightness)");
	r_hardness = Cvar_Add("r_hardness", "1.0", CVAR_ARCHIVE, "Controls the hardness of bump-mapping effects");
	r_height = Cvar_Add("r_height", "0", CVAR_ARCHIVE | CVAR_R_CONTEXT, NULL);
	r_invert = Cvar_Add("r_invert", "0", CVAR_ARCHIVE | CVAR_R_MEDIA, "Inverts the RGB values of all world textures");
	r_lighting = Cvar_Add("r_lighting", "1.0", CVAR_ARCHIVE, "Controls intensity of lighting effects");
	r_line_alpha = Cvar_Add("r_line_alpha", "0.5", CVAR_ARCHIVE, NULL);
	r_line_width = Cvar_Add("r_line_width", "1.0", CVAR_ARCHIVE, NULL);
	r_materials = Cvar_Add("r_materials", "1", CVAR_ARCHIVE, "Enables or disables the materials (progressive texture effects) system");
	r_max_lights = Cvar_Add("r_max_lights", "16", CVAR_ARCHIVE | CVAR_R_CONTEXT, "Controls the maximum number of lights affecting a rendered object");
	r_modulate = Cvar_Add("r_modulate", "3.0", CVAR_ARCHIVE | CVAR_R_MEDIA, "Controls the brightness of world surface lightmaps");
	Cvar_Add("r_lightscale", "1.0", CVAR_ARCHIVE | CVAR_R_CONTEXT, "Controls the scale of lightmaps during fragment generation");
	r_monochrome = Cvar_Add("r_monochrome", "0", CVAR_ARCHIVE | CVAR_R_MEDIA, "Loads all world textures as monochrome");
	r_multisample = Cvar_Add("r_multisample", "0", CVAR_ARCHIVE | CVAR_R_CONTEXT, "Controls multisampling (anti-aliasing)");
	r_parallax = Cvar_Add("r_parallax", "1.0", CVAR_ARCHIVE, "Controls the intensity of parallax mapping effects");
	r_render_plugin = Cvar_Add("r_render_plugin", "default", CVAR_ARCHIVE, "Specifies the active renderer plugin (default or pro)");
	r_saturation = Cvar_Add("r_saturation", "1.0", CVAR_ARCHIVE | CVAR_R_MEDIA, "Controls texture saturation");
	r_screenshot_format = Cvar_Add("r_screenshot_format", "png", CVAR_ARCHIVE, "Set your preferred screenshot format. Supports \"png\" or \"tga\".");
	r_shadows = Cvar_Add("r_shadows", "2", CVAR_ARCHIVE | CVAR_R_MEDIA, "Controls the rendering of mesh model shadows");
	r_shell = Cvar_Add("r_shell", "2", CVAR_ARCHIVE | CVAR_R_MEDIA, "Controls mesh shell effect (e.g. Quad Damage shell)");
	r_specular = Cvar_Add("r_specular", "1.0", CVAR_ARCHIVE, "Controls the specularity of bump-mapping effects");
	r_stainmaps = Cvar_Add("r_stainmaps", "1.0", CVAR_ARCHIVE, "Controls persistent stain effects.");
	r_swap_interval = Cvar_Add("r_swap_interval", "1", CVAR_ARCHIVE | CVAR_R_CONTEXT, "Controls vertical refresh synchronization. 0 disables, 1 enables, -1 enables adaptive VSync.");
	r_texture_mode = Cvar_Add("r_texture_mode", "GL_LINEAR_MIPMAP_LINEAR", CVAR_ARCHIVE | CVAR_R_MEDIA, "Specifies the active texture filtering mode");
	r_warp = Cvar_Add("r_warp", "1", CVAR_ARCHIVE, "Controls warping surface effects (e.g. water)");
	r_width = Cvar_Add("r_width", "0", CVAR_ARCHIVE | CVAR_R_CONTEXT, NULL);
	r_windowed_height = Cvar_Add("r_windowed_height", "1024", CVAR_ARCHIVE | CVAR_R_CONTEXT, NULL);
	r_windowed_width = Cvar_Add("r_windowed_width", "768", CVAR_ARCHIVE | CVAR_R_CONTEXT, NULL);
	r_supersample = Cvar_Add("r_supersample", "0", CVAR_ARCHIVE | CVAR_R_CONTEXT, "Controls the level of super-sampling. Requires framebuffer extension.");

	Cvar_ClearAll(CVAR_R_MASK);

	Cmd_Add("r_list_media", R_ListMedia_f, CMD_RENDERER, "List all currently loaded media");
	Cmd_Add("r_dump_images", R_DumpImages_f, CMD_RENDERER, "Dump all loaded images. Careful!");
	Cmd_Add("r_reset_stainmap", R_ResetStainmap_f, CMD_RENDERER, "Reset the stainmap");
	Cmd_Add("r_screenshot", R_Screenshot_f, CMD_SYSTEM | CMD_RENDERER, "Take a screenshot");
	Cmd_Add("r_sky", R_Sky_f, CMD_RENDERER, NULL);
	Cmd_Add("r_toggle_fullscreen", R_ToggleFullscreen_f, CMD_SYSTEM | CMD_RENDERER, "Toggle fullscreen");
	Cmd_Add("r_restart", R_Restart_f, CMD_RENDERER, "Restart the rendering subsystem");
}

/**
 * @brief Populates the GL config structure by querying the implementation.
 */
static void R_InitConfig(void) {

	memset(&r_config, 0, sizeof(r_config));

	// initialize GL pointers
	gladLoadGL();

	r_config.renderer = (const char *) glGetString(GL_RENDERER);
	r_config.vendor = (const char *) glGetString(GL_VENDOR);
	r_config.version = (const char *) glGetString(GL_VERSION);

	int32_t num_extensions;
	glGetIntegerv(GL_NUM_EXTENSIONS, &num_extensions);

	glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &r_config.max_texunits);
	glGetIntegerv(GL_MAX_TEXTURE_SIZE, &r_config.max_texture_size);

	Com_Print("  Renderer:   ^2%s^7\n", r_config.renderer);
	Com_Print("  Vendor:     ^2%s^7\n", r_config.vendor);
	Com_Print("  Version:    ^2%s^7\n", r_config.version);
	Com_Verbose("  Tex Units:  ^2%i^7\n", r_config.max_texunits);
	Com_Verbose("  Tex Size:   ^2%i^7\n", r_config.max_texture_size);

	for (int32_t i = 0; i < num_extensions; i++) {
		const char *c = (const char *) glGetStringi(GL_EXTENSIONS, i);

		if (i == 0) {
			Com_Verbose("  Extensions: ^2%s^7\n", c);
		} else {
			Com_Verbose("              ^2%s^7\n", c);
		}
	}
}

/**
 * @brief Creates the OpenGL context and initializes all GL state.
 */
void R_Init(void) {

	Com_Print("Video initialization...\n");

	R_InitLocal();

	R_InitContext();

	R_InitConfig();

	R_InitMedia();

	R_InitState();

	R_GetError("Video initialization");

	R_InitPrograms();

	R_InitImages();

	R_InitSupersample();

	R_InitDraw();

	R_InitModels();

	R_InitView();

	R_InitParticles();

	R_InitSky();

	R_InitMaterials();

	R_InitStainmaps();

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

	R_ShutdownDraw();

	R_ShutdownModels();

	R_ShutdownPrograms();

	R_ShutdownParticles();

	R_ShutdownSky();

	R_ShutdownStainmaps();

	R_ShutdownMaterials();

	R_ShutdownState();

	R_ShutdownContext();

	Mem_FreeTag(MEM_TAG_RENDERER);
}
