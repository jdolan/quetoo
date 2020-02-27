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

cvar_t *r_blend;
cvar_t *r_clear;
cvar_t *r_cull;
cvar_t *r_lock_vis;
cvar_t *r_no_vis;
cvar_t *r_draw_bsp_lightgrid;
cvar_t *r_draw_bsp_lightmaps;
cvar_t *r_draw_bsp_normals;
cvar_t *r_draw_entity_bounds;
cvar_t *r_draw_wireframe;
static cvar_t *r_draw_depth;

cvar_t *r_allow_high_dpi;
cvar_t *r_anisotropy;
cvar_t *r_brightness;
cvar_t *r_bumpmap;
cvar_t *r_caustics;
cvar_t *r_contrast;
cvar_t *r_display;
cvar_t *r_draw_buffer;
cvar_t *r_flares;
cvar_t *r_fog;
cvar_t *r_fullscreen;
cvar_t *r_gamma;
cvar_t *r_get_error;
cvar_t *r_hardness;
cvar_t *r_height;
cvar_t *r_lightmap;
cvar_t *r_lights;
cvar_t *r_materials;
cvar_t *r_max_lights;
cvar_t *r_modulate;
cvar_t *r_multisample;
cvar_t *r_parallax;
cvar_t *r_saturation;
cvar_t *r_screenshot_format;
cvar_t *r_shadows;
cvar_t *r_shell;
cvar_t *r_soft_particles;
cvar_t *r_specular;
cvar_t *r_stainmaps;
cvar_t *r_supersample;
cvar_t *r_texture_mode;
cvar_t *r_swap_interval;
cvar_t *r_warp;
cvar_t *r_width;

extern cl_client_t cl;
extern cl_static_t cls;

/**
 * @brief Queries OpenGL for any errors and prints them as warnings.
 */
void R_GetError_(const char *function, const char *msg) {

	if (!r_get_error->integer) {
		return;
	}

	while (true) {

		const GLenum err = glGetError();
		const char *s;

		switch (err) {
			case GL_NO_ERROR:
				return;
			case GL_INVALID_ENUM:
				s = "GL_INVALID_ENUM";
				break;
			case GL_INVALID_VALUE:
				s = "GL_INVALID_VALUE";
				break;
			case GL_INVALID_OPERATION:
				s = "GL_INVALID_OPERATION";
				break;
			case GL_OUT_OF_MEMORY:
				s = "GL_OUT_OF_MEMORY";
				break;
			default:
				s = va("%" PRIx32, err);
				break;
		}

		Com_Warn("%s threw %s: %s.\n", function, s, msg);

		if (r_get_error->integer >= 2) {
			SDL_TriggerBreakpoint();
		}
	}
}

/**
 * @brief Returns true if the specified bounding box is completely culled by the
 * view frustum, false otherwise.
 */
_Bool R_CullBox(const vec3_t mins, const vec3_t maxs) {

	if (!r_cull->value) {
		return false;
	}

	for (size_t i = 0; i < lengthof(r_locals.frustum); i++) {
		if (Cm_BoxOnPlaneSide(mins, maxs, &r_locals.frustum[i]) != SIDE_BACK) {
			return false;
		}
	}

	return true;
}

/**
 * @brief Returns true if the specified sphere (point and radius) is completely culled by the
 * view frustum, false otherwise.
 */
_Bool R_CullSphere(const vec3_t point, const float radius) {

	if (!r_cull->value) {
		return false;
	}

	for (size_t i = 0 ; i < lengthof(r_locals.frustum) ; i++)  {
		const cm_bsp_plane_t *p = &r_locals.frustum[i];
		const float dist = Vec3_Dot(point, p->normal) - p->dist;

		if (dist < -radius) {
			return true;
		}
	}

	return false;
}

/**
 * @brief
 */
static void R_UpdateProjection(void) {

	const float aspect = (float) r_context.width / (float) r_context.height;

	const float ymax = tanf(Radians(r_view.fov.y));
	const float ymin = -ymax;

	const float xmin = ymin * aspect;
	const float xmax = ymax * aspect;

	Matrix4x4_FromFrustum(&r_locals.projection3D, xmin, xmax, ymin, ymax, 1.0, MAX_WORLD_DIST);

	Matrix4x4_CreateIdentity(&r_locals.view);

	Matrix4x4_ConcatRotate(&r_locals.view, -90.0, 1.0, 0.0, 0.0); // put Z going up
	Matrix4x4_ConcatRotate(&r_locals.view,  90.0, 0.0, 0.0, 1.0); // put Z going up

	Matrix4x4_ConcatRotate(&r_locals.view, -r_view.angles.z, 1.0, 0.0, 0.0);
	Matrix4x4_ConcatRotate(&r_locals.view, -r_view.angles.x, 0.0, 1.0, 0.0);
	Matrix4x4_ConcatRotate(&r_locals.view, -r_view.angles.y, 0.0, 0.0, 1.0);

	Matrix4x4_ConcatTranslate(&r_locals.view, -r_view.origin.x, -r_view.origin.y, -r_view.origin.z);
}

/**
 * @brief Updates the clipping planes for the view frustum based on the origin
 * and angles for this frame.
 */
static void R_UpdateFrustum(void) {

	if (!r_cull->value) {
		return;
	}

	cm_bsp_plane_t *p = r_locals.frustum;

	float ang = Radians(r_view.fov.x);
	float xs = sinf(ang);
	float xc = cosf(ang);

	p[0].normal = Vec3_Scale(r_view.forward, xs);
	p[0].normal = Vec3_Add(p[0].normal, Vec3_Scale(r_view.right, xc));

	p[1].normal = Vec3_Scale(r_view.forward, xs);
	p[1].normal = Vec3_Add(p[1].normal, Vec3_Scale(r_view.right, -xc));

	ang = Radians(r_view.fov.y);
	xs = sinf(ang);
	xc = cosf(ang);

	p[2].normal = Vec3_Scale(r_view.forward, xs);
	p[2].normal = Vec3_Add(p[2].normal, Vec3_Scale(r_view.up, xc));

	p[3].normal = Vec3_Scale(r_view.forward, xs);
	p[3].normal = Vec3_Add(p[3].normal, Vec3_Scale(r_view.up, -xc));

	for (int32_t i = 0; i < 4; i++) {
		p[i].type = PLANE_ANY_Z;
		p[i].dist = Vec3_Dot (r_view.origin, p[i].normal);
		p[i].sign_bits = Cm_SignBitsForPlane(&p[i]);
	}
}

/**
 * @brief
 */
static void R_UpdateFog(void) {

	r_locals.fog_parameters = Vec3_Multiply(r_view.fog_parameters, Vec3(1.f, 1.f, r_fog->value));
}

/**
 * @brief
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

	// or if the client is not fully loaded
	if (cls.state != CL_ACTIVE) {
		bits |= GL_COLOR_BUFFER_BIT;
	}

	// or if the client is no-clipping around the world
	if (r_view.contents & CONTENTS_SOLID) {
		bits |= GL_COLOR_BUFFER_BIT;
	}

	glClear(bits);
	R_GetError(NULL);
}

/**
 * @brief Main entry point for drawing the 3D view.
 */
void R_DrawView(r_view_t *view) {

	glBindFramebuffer(GL_FRAMEBUFFER, r_context.framebuffer);

	R_Clear();

	R_UpdateProjection();

	R_UpdateFrustum();

	R_UpdateVis();
	
	R_UpdateLights();

	R_UpdateFog();

	R_DrawWorld();

	R_DrawSkyBox();

//	R_AddFlares();

	R_DrawEntities();

	R_DrawParticles();

	R_Draw3D();

	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	const r_image_t frame_buffer = {
		.texnum = r_context.color_attachment,
		.width = r_context.width,
		.height = -r_context.height
	};

	R_Draw2DImage(0, r_context.height, frame_buffer.width, frame_buffer.height, &frame_buffer, color_white);

	R_GetError(NULL);
}

/**
 * @brief Called at the beginning of each render frame.
 */
void R_BeginFrame(void) {

	R_Clear();

	r_view.count_bsp_nodes = 0;
	r_view.count_bsp_draw_elements = 0;

	r_view.count_mesh_models = 0;
	r_view.count_mesh_triangles = 0;

	r_view.count_draw_chars = 0;
	r_view.count_draw_fills = 0;
	r_view.count_draw_images = 0;
	r_view.count_draw_lines = 0;
}

/**
 * @brief Called at the end of each render frame.
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
static void R_InitView(void) {

	memset(&r_view, 0, sizeof(r_view));

	memset(&r_locals, 0, sizeof(r_locals));
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

	Cl_LoadingProgress(1, "world");

	R_LoadModel(cl.config_strings[CS_MODELS]); // load the world

	//Cl_LoadingProgress(55, "mopping up blood");

//	R_ResetStainmaps(); // clear the stainmap if we have to

	Cl_LoadingProgress(66, "models");

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
		cl.image_precache[i] = R_LoadImage(cl.config_strings[CS_IMAGES + i], IT_PIC); // FIXME: Atlas?
	}

	Cl_LoadingProgress(77, "sky");

	// sky environment map
	R_SetSky(cl.config_strings[CS_SKY]);

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

	extern void Ui_HandleEvent(const SDL_Event *event);

	Ui_HandleEvent(&(const SDL_Event) {
		.window.type = SDL_WINDOWEVENT,
		.window.event = SDL_WINDOWEVENT_CLOSE
	});

	R_Shutdown();

	R_Init();

	extern void Cl_HandleEvents(void);

	Cl_HandleEvents();

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
 * @brief Initializes console variables and commands for the renderer.
 */
static void R_InitLocal(void) {

	// development tools
	r_blend = Cvar_Add("r_blend", "1", CVAR_DEVELOPER, "Controls alpha blending operations (developer tool)");
	r_clear = Cvar_Add("r_clear", "0", CVAR_DEVELOPER, "Controls buffer clearing (developer tool)");
	r_cull = Cvar_Add("r_cull", "1", CVAR_DEVELOPER, "Controls bounded box culling routines (developer tool)");
	r_lock_vis = Cvar_Add("r_lock_vis", "0", CVAR_DEVELOPER, "Temporarily locks the PVS lookup for world surfaces (developer tool)");
	r_no_vis = Cvar_Add("r_no_vis", "0", CVAR_DEVELOPER, "Disables PVS refresh and lookup for world surfaces (developer tool)");
	r_draw_bsp_lightgrid = Cvar_Add("r_draw_bsp_lightgrid", "0", CVAR_DEVELOPER | CVAR_R_MEDIA, "Controls the rendering of BSP lightgrid textures (developer tool)");
	r_draw_bsp_lightmaps = Cvar_Add("r_draw_bsp_lightmaps", "0", CVAR_DEVELOPER, "Controls the rendering of BSP lightmap textures (developer tool)");
	r_draw_bsp_normals = Cvar_Add("r_draw_bsp_normals", "0", CVAR_DEVELOPER, "Controls the rendering of BSP vertex normals (developer tool)");
	r_draw_entity_bounds = Cvar_Add("r_draw_entity_bounds", "0", CVAR_DEVELOPER, "Controls the rendering of entity bounding boxes (developer tool)");
	r_draw_wireframe = Cvar_Add("r_draw_wireframe", "0", CVAR_DEVELOPER, "Controls the rendering of polygons as wireframe (developer tool)");
	r_draw_depth = Cvar_Add("r_draw_depth", "0", CVAR_DEVELOPER, "Controls rendering the depth buffer attachment");

	// settings and preferences
	r_allow_high_dpi = Cvar_Add("r_allow_high_dpi", "1", CVAR_ARCHIVE | CVAR_R_CONTEXT, "Enables or disables support for High-DPI (Retina, 4K) display modes");
	r_anisotropy = Cvar_Add("r_anisotropy", "4", CVAR_ARCHIVE | CVAR_R_MEDIA, "Controls anisotropic texture filtering");
	r_brightness = Cvar_Add("r_brightness", "1", CVAR_ARCHIVE, "Controls texture brightness");
	r_bumpmap = Cvar_Add("r_bumpmap", "1", CVAR_ARCHIVE, "Controls the intensity of bump-mapping effects");
	r_caustics = Cvar_Add("r_caustics", "1", CVAR_ARCHIVE | CVAR_R_MEDIA, "Enable or disable liquid caustic effects");
	r_contrast = Cvar_Add("r_contrast", "1", CVAR_ARCHIVE, "Controls texture contrast");
	r_display = Cvar_Add("r_display", "0", CVAR_ARCHIVE, "Specifies the default display to use");
	r_draw_buffer = Cvar_Add("r_draw_buffer", "GL_BACK", CVAR_ARCHIVE, NULL);
	r_flares = Cvar_Add("r_flares", "1", CVAR_ARCHIVE, "Controls the rendering of light source flares");
	r_fog = Cvar_Add("r_fog", "1", CVAR_ARCHIVE, "Controls the rendering of fog effects");
	r_fullscreen = Cvar_Add("r_fullscreen", "1", CVAR_ARCHIVE | CVAR_R_CONTEXT, "Controls fullscreen mode. 1 = exclusive, 2 = borderless");
	r_gamma = Cvar_Add("r_gamma", "1", CVAR_ARCHIVE, "Controls video gamma (brightness)");
	r_get_error = Cvar_Add("r_get_error", "0", CVAR_DEVELOPER, "Log OpenGL errors to the console (developer tool)");
	r_hardness = Cvar_Add("r_hardness", "1", CVAR_ARCHIVE, "Controls the hardness of bump-mapping effects");
	r_height = Cvar_Add("r_height", "0", CVAR_ARCHIVE | CVAR_R_CONTEXT, NULL);
	r_lightmap = Cvar_Add("r_lightmap", "1", CVAR_ARCHIVE, "Controls lightmap rendering");
	r_lights = Cvar_Add("r_lights", "1", CVAR_ARCHIVE, "Enables or disables dyanmic lighting");
	r_materials = Cvar_Add("r_materials", "1", CVAR_ARCHIVE, "Enables or disables the materials (progressive texture effects) system");
	r_max_lights = Cvar_Add("r_max_lights", "16", CVAR_ARCHIVE | CVAR_R_CONTEXT, "Controls the maximum number of lights affecting a rendered object");
	r_modulate = Cvar_Add("r_modulate", "1", CVAR_ARCHIVE, "Controls the brightness of static lighting");
	r_multisample = Cvar_Add("r_multisample", "0", CVAR_ARCHIVE | CVAR_R_CONTEXT, "Controls multisampling (anti-aliasing).");
	r_parallax = Cvar_Add("r_parallax", "1", CVAR_ARCHIVE, "Controls the intensity of parallax mapping effects.");
	r_saturation = Cvar_Add("r_saturation", "1", CVAR_ARCHIVE, "Controls texture saturation.");
	r_screenshot_format = Cvar_Add("r_screenshot_format", "png", CVAR_ARCHIVE, "Set your preferred screenshot format. Supports \"png\", \"tga\" or \"pbm\".");
	r_shadows = Cvar_Add("r_shadows", "2", CVAR_ARCHIVE, "Controls the rendering of mesh model shadows");
	r_shell = Cvar_Add("r_shell", "2", CVAR_ARCHIVE, "Controls mesh shell effect (e.g. Quad Damage shell)");
	r_soft_particles = Cvar_Add("r_soft_particles", "1", CVAR_ARCHIVE, "Controls soft particles, which are more expensive.");
	r_specular = Cvar_Add("r_specular", "1", CVAR_ARCHIVE, "Controls the specularity of bump-mapping effects.");
	r_stainmaps = Cvar_Add("r_stainmaps", "1", CVAR_ARCHIVE, "Controls persistent stain effects.");
	r_supersample = Cvar_Add("r_supersample", "0", CVAR_ARCHIVE | CVAR_R_CONTEXT, "Controls the level of super-sampling. Requires framebuffer extension.");
	r_swap_interval = Cvar_Add("r_swap_interval", "1", CVAR_ARCHIVE | CVAR_R_CONTEXT, "Controls vertical refresh synchronization. 0 disables, 1 enables, -1 enables adaptive VSync.");
	r_texture_mode = Cvar_Add("r_texture_mode", "GL_LINEAR_MIPMAP_LINEAR", CVAR_ARCHIVE | CVAR_R_MEDIA, "Specifies the active texture filtering mode");
	r_warp = Cvar_Add("r_warp", "1", CVAR_ARCHIVE, "Controls warping surface effects (e.g. water)");
	r_width = Cvar_Add("r_width", "0", CVAR_ARCHIVE | CVAR_R_CONTEXT, NULL);

	Cvar_ClearAll(CVAR_R_MASK);

	Cmd_Add("r_dump_images", R_DumpImages_f, CMD_RENDERER, "Dump all loaded images to disk (developer tool)");
	Cmd_Add("r_list_media", R_ListMedia_f, CMD_RENDERER, "List all currently loaded media (developer tool)");
	Cmd_Add("r_restart", R_Restart_f, CMD_RENDERER, "Restart the rendering subsystem");
	Cmd_Add("r_screenshot", R_Screenshot_f, CMD_SYSTEM | CMD_RENDERER, "Take a screenshot");
	Cmd_Add("r_sky", R_Sky_f, CMD_RENDERER, "Sets the sky environment map");
	Cmd_Add("r_toggle_fullscreen", R_ToggleFullscreen_f, CMD_SYSTEM | CMD_RENDERER, "Toggle fullscreen");
}

/**
 * @brief Populates the GL config structure by querying the implementation.
 */
static void R_InitConfig(void) {

	memset(&r_config, 0, sizeof(r_config));

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

	R_GetError(NULL);
}

/**
 * @brief Creates the default 3d framebuffer.
 */
static void R_InitFramebuffer(void) {

	glGenFramebuffers(1, &r_context.framebuffer);

	glBindFramebuffer(GL_FRAMEBUFFER, r_context.framebuffer);

	R_GetError("Make framebuffer");

	glGenTextures(1, &r_context.color_attachment);
	glBindTexture(GL_TEXTURE_2D, r_context.color_attachment);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, r_context.drawable_width, r_context.drawable_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, r_context.color_attachment, 0);

	R_GetError("Color attachment");
	
	glGenTextures(1, &r_context.depth_attachment);
	glBindTexture(GL_TEXTURE_2D, r_context.depth_attachment);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH24_STENCIL8, r_context.drawable_width, r_context.drawable_height, 0, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, r_context.depth_attachment, 0);

	R_GetError("Depth attachment");

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glBindTexture(GL_TEXTURE_2D, 0);
}

/**
 * @brief Destroys the default 3d framebuffer.
 */
static void R_ShutdownFramebuffer(void) {
	
	glDeleteFramebuffers(1, &r_context.framebuffer);
	glDeleteTextures(1, &r_context.color_attachment);
	glDeleteTextures(1, &r_context.depth_attachment);

	R_GetError(NULL);
}

/**
 * @brief Creates the OpenGL context and initializes all GL state.
 */
void R_Init(void) {

	Com_Print("Video initialization...\n");

	R_InitLocal();

	R_InitContext();

	R_InitConfig();

	R_InitFramebuffer();

	R_InitMedia();

	R_InitImages();

	R_InitDraw2D();

	R_InitDraw3D();

	R_InitModels();

	R_InitView();

	R_InitParticles();

	R_InitSky();

	R_InitMaterials();

	R_GetError("Video initialization");

	Com_Print("Video initialized %dx%d (%dx%d) %s\n",
			  r_context.width, r_context.height,
			  r_context.drawable_width, r_context.drawable_height,
	          (r_context.fullscreen ? "fullscreen" : "windowed"));
}

/**
 * @brief Shuts down the renderer, freeing all resources belonging to it,
 * including the GL context.
 */
void R_Shutdown(void) {

	Cmd_RemoveAll(CMD_RENDERER);

	R_ShutdownMedia();

	R_ShutdownDraw2D();

	R_ShutdownDraw3D();

	R_ShutdownModels();

	R_ShutdownParticles();

	R_ShutdownSky();

	R_ShutdownMaterials();

	R_ShutdownFramebuffer();

	R_ShutdownContext();

	Mem_FreeTag(MEM_TAG_RENDERER);
}
