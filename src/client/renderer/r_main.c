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

#include "r_local.h"

r_config_t r_config;
r_uniforms_t r_uniforms;
r_stats_t r_stats;

cvar_t *r_alpha_test_threshold;
cvar_t *r_blend_depth_sorting;
cvar_t *r_cull;
cvar_t *r_depth_pass;
cvar_t *r_draw_bsp_lightgrid;
cvar_t *r_draw_bsp_normals;
cvar_t *r_draw_bsp_occlusion_queries;
cvar_t *r_draw_entity_bounds;
cvar_t *r_draw_material_stages;
cvar_t *r_draw_wireframe;
cvar_t *r_get_error;
cvar_t *r_max_errors;
cvar_t *r_occlude;

int32_t r_error_count;

cvar_t *r_allow_high_dpi;
cvar_t *r_anisotropy;
cvar_t *r_brightness;
cvar_t *r_bicubic;
cvar_t *r_caustics;
cvar_t *r_contrast;
cvar_t *r_display;
cvar_t *r_finish;
cvar_t *r_fog_density;
cvar_t *r_fog_samples;
cvar_t *r_fullscreen;
cvar_t *r_gamma;
cvar_t *r_hardness;
cvar_t *r_height;
cvar_t *r_modulate;
cvar_t *r_multisample;
cvar_t *r_parallax;
cvar_t *r_parallax_samples;
cvar_t *r_roughness;
cvar_t *r_saturation;
cvar_t *r_screenshot_format;
cvar_t *r_shell;
cvar_t *r_specularity;
cvar_t *r_sprite_downsample;
cvar_t *r_texture_downsample;
cvar_t *r_stains;
cvar_t *r_texture_mode;
cvar_t *r_texture_storage;
cvar_t *r_swap_interval;
cvar_t *r_width;

/**
 * @brief Queries OpenGL for any errors and prints them as warnings.
 */
void R_GetError_(const char *function, const char *msg) {

	if (!r_get_error->integer) {
		return;
	}

	if (GLAD_GL_KHR_debug) {
		return;
	}
	
	// reset error count, so as-it-happens errors
	// can happen again.
	r_error_count = 0;

	// reinstall debug handler.
	gladInstallGLDebug();

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

		if (r_get_error->integer == 2) {
			SDL_TriggerBreakpoint();
		}
	}
}

/**
 * @return True if the specified bounding box is culled by the view frustum, false otherwise.
 * @see http://www.lighthouse3d.com/tutorials/view-frustum-culling/geometric-approach-testing-boxes/
 */
_Bool R_CullBox(const r_view_t *view, const box3_t bounds) {

	if (!r_cull->value) {
		return false;
	}

	if (view->type == VIEW_PLAYER_MODEL) {
		return false;
	}

	vec3_t points[8];
	
	Box3_ToPoints(bounds, points);

	const cm_bsp_plane_t *plane = view->frustum;
	for (size_t i = 0; i < lengthof(view->frustum); i++, plane++) {

		size_t j;
		for (j = 0; j < lengthof(points); j++) {
			const float dist = Cm_DistanceToPlane(points[j], plane);
			if (dist >= 0.f) {
				break;
			}
		}

		if (j == lengthof(points)) {
			return true;
		}
    }

	return false;
}

/**
 * @return True if the specified sphere is culled by the view frustum, false otherwise.
 */
_Bool R_CullSphere(const r_view_t *view, const vec3_t point, const float radius) {

	if (!r_cull->value) {
		return false;
	}

	if (view->type == VIEW_PLAYER_MODEL) {
		return false;
	}

	const cm_bsp_plane_t *plane = view->frustum;
	for (size_t i = 0 ; i < lengthof(view->frustum) ; i++, plane++)  {
		const float dist = Cm_DistanceToPlane(point, plane);
		if (dist < -radius) {
			return true;
		}
	}

	return false;
}

/**
 * @return True if the specified box is occluded *or* culled by the view frustum,
 * false otherwise.
 */
_Bool R_CulludeBox(const r_view_t *view, const box3_t bounds) {

	return R_OccludeBox(view, bounds) || R_CullBox(view, bounds);
}

/**
 * @return True if the specified sphere is occluded *or* culled by the view frustum,
 * false otherwise.
 */
_Bool R_CulludeSphere(const r_view_t *view, const vec3_t point, const float radius) {

	return R_OccludeSphere(view, point, radius) || R_CullSphere(view, point, radius);
}

/**
 * @brief
 */
static void R_UpdateUniforms(const r_view_t *view) {

	memset(&r_uniforms.block, 0, sizeof(r_uniforms.block));

	r_uniforms.block.projection2D = Mat4_FromOrtho(0.f, r_context.width, r_context.height, 0.f, -1.f, 1.f);

	r_uniforms.block.brightness = r_brightness->value;
	r_uniforms.block.contrast = r_contrast->value;
	r_uniforms.block.saturation = r_saturation->value;
	r_uniforms.block.gamma = r_gamma->value;

	if (view) {
		r_uniforms.block.viewport = view->viewport;

		const float aspect = view->viewport.z / view->viewport.w;

		const float ymax = tanf(Radians(view->fov.y));
		const float ymin = -ymax;

		const float xmin = ymin * aspect;
		const float xmax = ymax * aspect;

		r_uniforms.block.projection3D = Mat4_FromFrustum(xmin, xmax, ymin, ymax, NEAR_DIST, MAX_WORLD_DIST);

		r_uniforms.block.view = Mat4_FromRotation(-90.f, Vec3(1.f, 0.f, 0.f)); // put Z going up
		r_uniforms.block.view = Mat4_ConcatRotation(r_uniforms.block.view, 90.f, Vec3(0.f, 0.f, 1.f)); // put Z going up
		r_uniforms.block.view = Mat4_ConcatRotation3(r_uniforms.block.view, Vec3(-view->angles.z, -view->angles.x, -view->angles.y));
		r_uniforms.block.view = Mat4_ConcatTranslation(r_uniforms.block.view, Vec3_Negate(view->origin));

		r_uniforms.block.depth_range.x = NEAR_DIST;
		r_uniforms.block.depth_range.y = MAX_WORLD_DIST;

		r_uniforms.block.view_type = view->type;
		r_uniforms.block.ticks = view->ticks;

		r_uniforms.block.modulate = r_modulate->value;
		
		r_uniforms.block.fog_density = r_fog_density->value;
		r_uniforms.block.fog_samples = r_fog_samples->integer;

		if (r_world_model) {
			r_uniforms.block.lightgrid.mins = Vec3_ToVec4(r_world_model->bsp->lightgrid->bounds.mins, 0.f);
			r_uniforms.block.lightgrid.maxs = Vec3_ToVec4(r_world_model->bsp->lightgrid->bounds.maxs, 0.f);

			const vec3_t pos = Vec3_Subtract(view->origin, r_world_model->bsp->lightgrid->bounds.mins);
			const vec3_t size = Box3_Size(r_world_model->bsp->lightgrid->bounds);

			r_uniforms.block.lightgrid.view_coordinate = Vec3_ToVec4(Vec3_Divide(pos, size), 0.f);
			r_uniforms.block.lightgrid.size = Vec3_ToVec4(Vec3i_CastVec3(r_world_model->bsp->lightgrid->size), 0.f);
		}
	}

	glBindBuffer(GL_UNIFORM_BUFFER, r_uniforms.buffer);
	// FIXME: we can adjust the written size based on data that's being updated
	glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(r_uniforms.block), &r_uniforms.block);
	glBindBuffer(GL_UNIFORM_BUFFER, 0);
}

/**
 * @brief Updates the clipping planes for the view frustum for the current frame.
 * @details The frustum planes are outward facing. Thus, any object that appears
 * partially behind any of the frustum planes should be visible.
 */
static void R_UpdateFrustum(r_view_t *view) {

	if (!r_cull->value) {
		return;
	}

	cm_bsp_plane_t *p = view->frustum;

	float ang = Radians(view->fov.x);
	float xs = sinf(ang);
	float xc = cosf(ang);

	p[0].normal = Vec3_Scale(view->forward, xs);
	p[0].normal = Vec3_Fmaf(p[0].normal, xc, view->right);

	p[1].normal = Vec3_Scale(view->forward, xs);
	p[1].normal = Vec3_Fmaf(p[1].normal, -xc, view->right);

	ang = Radians(view->fov.y);
	xs = sinf(ang);
	xc = cosf(ang);

	p[2].normal = Vec3_Scale(view->forward, xs);
	p[2].normal = Vec3_Fmaf(p[2].normal, xc, view->up);

	p[3].normal = Vec3_Scale(view->forward, xs);
	p[3].normal = Vec3_Fmaf(p[3].normal, -xc, view->up);

	for (size_t i = 0; i < lengthof(view->frustum); i++) {
		p[i].normal = Vec3_Normalize(p[i].normal);
		p[i].dist = Vec3_Dot(view->origin, p[i].normal);
		p[i].type = Cm_PlaneTypeForNormal(p[i].normal);
		p[i].sign_bits = Cm_SignBitsForNormal(p[i].normal);
	}
}

/**
 * @brief Called at the beginning of each render frame.
 */
void R_BeginFrame(void) {

	memset(&r_stats, 0, sizeof(r_stats));

	glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);
}

/**
 * @brief
 */
void R_DrawViewDepth(r_view_t *view) {

	assert(view);
	assert(view->framebuffer);

	glBindFramebuffer(GL_FRAMEBUFFER, view->framebuffer->name);

	glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);

	R_UpdateFrustum(view);

	R_UpdateUniforms(view);

	R_DrawDepthPass(view);

	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	R_GetError(NULL);
}

/**
 * @brief Entry point for drawing the main view.
 */
void R_DrawMainView(r_view_t *view) {

	assert(view);
	assert(view->framebuffer);

	R_DrawBspLightgrid(view);

	R_UpdateBlendDepth(view);

	R_UpdateEntities(view);

	R_UpdateLights(view);

	R_UpdateSprites(view);

	R_UpdateStains(view);

	glBindFramebuffer(GL_FRAMEBUFFER, view->framebuffer->name);

	if (r_draw_wireframe->value) {
		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	}

	R_DrawWorld(view);

	R_DrawEntities(view, -1);

	if (r_draw_wireframe->value) {
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	}

	R_DrawSprites(view, -1);

	R_Draw3D();

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

/**
 * @brief Entry point for drawing the player model view.
 */
void R_DrawPlayerModelView(r_view_t *view) {

	assert(view);
	assert(view->framebuffer);

	R_UpdateUniforms(view);

	R_UpdateEntities(view);

	glBindFramebuffer(GL_FRAMEBUFFER, view->framebuffer->name);

	glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);

	glViewport(0, 0, view->viewport.z, view->viewport.w);

	R_DrawEntities(view, -1);

	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	glViewport(0, 0, r_context.drawable_width, r_context.drawable_height);
}

/**
 * @brief Called at the end of each render frame.
 */
void R_EndFrame(void) {

	R_Draw2D();

	if (r_finish->value) {
		glFinish();
	}

	SDL_GL_SwapWindow(r_context.window);
}

/**
 * @brief Initializes console variables and commands for the renderer.
 */
static void R_InitLocal(void) {

	// development tools
	r_alpha_test_threshold = Cvar_Add("r_alpha_test_threshold", ".8", CVAR_DEVELOPER, "Controls alpha test threshold (developer tool)");
	r_blend_depth_sorting = Cvar_Add("r_blend_depth_sorting", "1", CVAR_DEVELOPER, "Controls alpha blending sorting (developer tool)");
	r_cull = Cvar_Add("r_cull", "1", CVAR_DEVELOPER, "Controls bounded box culling routines (developer tool)");
	r_draw_bsp_lightgrid = Cvar_Add("r_draw_bsp_lightgrid", "0", CVAR_DEVELOPER | CVAR_R_MEDIA, "Controls the rendering of BSP lightgrid textures (developer tool)");
	r_draw_bsp_normals = Cvar_Add("r_draw_bsp_normals", "0", CVAR_DEVELOPER, "Controls the rendering of BSP vertex normals (developer tool)");
	r_draw_bsp_occlusion_queries = Cvar_Add("r_draw_bsp_occlusion_queries", "0", CVAR_DEVELOPER, "Controls the rendering of BSP occlusion queries (developer tool)");
	r_draw_entity_bounds = Cvar_Add("r_draw_entity_bounds", "0", CVAR_DEVELOPER, "Controls the rendering of entity bounding boxes (developer tool)");
	r_draw_material_stages = Cvar_Add("r_draw_material_stages", "1", CVAR_DEVELOPER, "Controls the rendering of material stage effects (developer tool)");
	r_draw_wireframe = Cvar_Add("r_draw_wireframe", "0", CVAR_DEVELOPER, "Controls the rendering of polygons as wireframe (developer tool)");
	r_depth_pass = Cvar_Add("r_depth_pass", "1", CVAR_DEVELOPER, "Controls the rendering of the depth pass (developer tool");
	r_get_error = Cvar_Add("r_get_error", "0", CVAR_DEVELOPER | CVAR_R_CONTEXT, "Log OpenGL errors to the console (developer tool)");
	r_max_errors = Cvar_Add("r_max_errors", "8", CVAR_DEVELOPER, "The max number of errors before skipping error handlers (developer tool)");
	r_occlude = Cvar_Add("r_occlude", "1", CVAR_DEVELOPER, "Controls the rendering of occlusion queries (developer tool)");

	// settings and preferences
	r_allow_high_dpi = Cvar_Add("r_allow_high_dpi", "1", CVAR_ARCHIVE | CVAR_R_CONTEXT, "Enables or disables support for High-DPI (Retina, 4K) display modes");
	r_anisotropy = Cvar_Add("r_anisotropy", "4", CVAR_ARCHIVE | CVAR_R_MEDIA, "Controls anisotropic texture filtering");
	r_brightness = Cvar_Add("r_brightness", "1", CVAR_ARCHIVE, "Controls texture brightness");
	r_bicubic = Cvar_Add("r_bicubic", "1", CVAR_ARCHIVE, "Enable or disable high quality texture filtering on lightmaps and stainmaps.");
	r_caustics = Cvar_Add("r_caustics", "1", CVAR_ARCHIVE | CVAR_R_MEDIA, "Enable or disable liquid caustic effects");
	r_contrast = Cvar_Add("r_contrast", "1", CVAR_ARCHIVE, "Controls texture contrast");
	r_display = Cvar_Add("r_display", "0", CVAR_ARCHIVE, "Specifies the default display to use");
	r_finish = Cvar_Add("r_finish", "0", CVAR_ARCHIVE, "Flush or finish the GL command buffer before swapping frame buffers. 1 = flush, 2 = finish.");
	r_fog_density = Cvar_Add("r_fog_density", "1", CVAR_ARCHIVE, "Controls the density of fog effects");
	r_fog_samples = Cvar_Add("r_fog_samples", "8", CVAR_ARCHIVE, "Controls the quality of fog effects");
	r_fullscreen = Cvar_Add("r_fullscreen", "1", CVAR_ARCHIVE | CVAR_R_CONTEXT, "Controls fullscreen mode. 1 = exclusive, 2 = borderless");
	r_gamma = Cvar_Add("r_gamma", "1", CVAR_ARCHIVE, "Controls video gamma (brightness)");
	r_hardness = Cvar_Add("r_hardness", "1", CVAR_ARCHIVE, "Controls the hardness of bump-mapping effects");
	r_height = Cvar_Add("r_height", "0", CVAR_ARCHIVE | CVAR_R_CONTEXT, NULL);
	r_modulate = Cvar_Add("r_modulate", "1", CVAR_ARCHIVE, "Controls the brightness of static lighting");
	r_multisample = Cvar_Add("r_multisample", "0", CVAR_ARCHIVE | CVAR_R_CONTEXT, "Controls multisampling (anti-aliasing).");
	r_parallax = Cvar_Add("r_parallax", "1", CVAR_ARCHIVE, "Controls the intensity of parallax mapping effects.");
	r_parallax_samples = Cvar_Add("r_parallax_samples", "32", CVAR_ARCHIVE, "Controls the number of steps for parallax mapping.");
	r_roughness = Cvar_Add("r_roughness", "1", CVAR_ARCHIVE, "Controls the roughness of bump-mapping effects");
	r_saturation = Cvar_Add("r_saturation", "1", CVAR_ARCHIVE, "Controls texture saturation.");
	r_screenshot_format = Cvar_Add("r_screenshot_format", "png", CVAR_ARCHIVE, "Set your preferred screenshot format. Supports \"png\", \"tga\" or \"pbm\".");
	r_shell = Cvar_Add("r_shell", "2", CVAR_ARCHIVE, "Controls mesh shell effect (e.g. Quad Damage shell)");
	r_specularity = Cvar_Add("r_specularity", "1", CVAR_ARCHIVE, "Controls the specularity of bump-mapping effects.");
	r_sprite_downsample = Cvar_Add("r_sprite_quality", "1", CVAR_ARCHIVE | CVAR_R_MEDIA, "Controls downsampling of sprite effects to boost performance on low-end systems.");
	r_texture_downsample = Cvar_Add("r_texture_downsample", "1", CVAR_ARCHIVE | CVAR_R_MEDIA, "Controls downsampling of textures to boost performance on low-end systems.");
	r_stains = Cvar_Add("r_stains", "1", CVAR_ARCHIVE | CVAR_R_MEDIA, "Controls persistent stain effects.");
	r_swap_interval = Cvar_Add("r_swap_interval", "1", CVAR_ARCHIVE | CVAR_R_CONTEXT, "Controls vertical refresh synchronization. 0 disables, 1 enables, -1 enables adaptive VSync.");
	r_texture_mode = Cvar_Add("r_texture_mode", "GL_LINEAR_MIPMAP_LINEAR", CVAR_ARCHIVE | CVAR_R_MEDIA, "Specifies the active texture filtering mode");
	r_texture_storage = Cvar_Add("r_texture_storage", "1", CVAR_ARCHIVE | CVAR_R_MEDIA, "Specifies whether to use newer texture storage routines; keep on unless you have errors stemming from glTexStorage.");
	r_width = Cvar_Add("r_width", "0", CVAR_ARCHIVE | CVAR_R_CONTEXT, NULL);

	Cvar_ClearAll(CVAR_R_MASK);

	Cmd_Add("r_dump_images", R_DumpImages_f, CMD_RENDERER, "Dump all loaded images to disk (developer tool)");
	Cmd_Add("r_list_media", R_ListMedia_f, CMD_RENDERER, "List all currently loaded media (developer tool)");
	Cmd_Add("r_save_materials", R_SaveMaterials_f, CMD_RENDERER, "Write all of the loaded map materials to disk (developer tool).");
	Cmd_Add("r_screenshot", R_Screenshot_f, CMD_SYSTEM | CMD_RENDERER, "Take a screenshot");
	Cmd_Add("r_sky", R_Sky_f, CMD_RENDERER, "Sets the sky environment map");
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
 * @brief
 */
static void R_InitUniforms(void) {

	glGenBuffers(1, &r_uniforms.buffer);

	glBindBuffer(GL_UNIFORM_BUFFER, r_uniforms.buffer);
	glBufferData(GL_UNIFORM_BUFFER, sizeof(r_uniforms.block), NULL, GL_DYNAMIC_DRAW);

	R_UpdateUniforms(NULL);
}

/**
 * @brief
 */
static void R_ShutdownUniforms(void) {

	glDeleteBuffers(1, &r_uniforms.buffer);
}

/**
 * @brief Creates the OpenGL context and initializes all GL state.
 */
void R_Init(void) {

	Com_Print("Video initialization...\n");

	R_InitLocal();

	R_InitContext();

	R_InitConfig();

	R_InitUniforms();

	R_InitMedia();

	R_InitImages();

	R_InitDepthPass();

	R_InitDraw2D();

	R_InitDraw3D();

	R_InitModels();

	R_InitSprites();

	R_InitLights();

	R_InitSky();

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

	R_ShutdownLights();

	R_ShutdownSprites();

	R_ShutdownSky();

	R_ShutdownDepthPass();

	R_ShutdownUniforms();

	R_ShutdownContext();

	Mem_FreeTag(MEM_TAG_RENDERER);
}
