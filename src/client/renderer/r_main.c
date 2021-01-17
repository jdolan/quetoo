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

r_view_t *r_view;

r_config_t r_config;
r_uniforms_t r_uniforms;
r_stats_t r_stats;

cvar_t *r_blend_depth_sorting;
cvar_t *r_clear;
cvar_t *r_cull;
cvar_t *r_depth_pass;
cvar_t *r_draw_bsp_lightgrid;
cvar_t *r_draw_bsp_normals;
cvar_t *r_draw_entity_bounds;
cvar_t *r_draw_material_stages;
cvar_t *r_draw_wireframe;
cvar_t *r_get_error;
cvar_t *r_occlude;

cvar_t *r_allow_high_dpi;
cvar_t *r_anisotropy;
cvar_t *r_brightness;
cvar_t *r_bicubic;
cvar_t *r_caustics;
cvar_t *r_contrast;
cvar_t *r_display;
cvar_t *r_flares;
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
cvar_t *r_stains;
cvar_t *r_texture_mode;
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
 * @return True if the specified point is culled by the view frustum, false otherwise.
 */
_Bool R_CullPoint(const r_view_t *view, const vec3_t point) {

	if (!r_cull->value) {
		return false;
	}

	const cm_bsp_plane_t *plane = view->frustum;
	for (size_t i = 0; i< lengthof(view->frustum); i++, plane++) {
		const float dist = Cm_DistanceToPlane(point, plane);
		if (dist > 0.f) {
			return true;
		}
	}

	return false;
}

/**
 * @return True if the specified bounding box is culled by the view frustum, false otherwise.
 * @see http://www.lighthouse3d.com/tutorials/view-frustum-culling/geometric-approach-testing-boxes/
 */
_Bool R_CullBox(const r_view_t *view, const vec3_t mins, const vec3_t maxs) {

	if (!r_cull->value) {
		return false;
	}

	const vec3_t points[] = {
		Vec3(mins.x, mins.y, mins.z),
		Vec3(maxs.x, mins.y, mins.z),
		Vec3(maxs.x, maxs.y, mins.z),
		Vec3(mins.x, maxs.y, mins.z),
		Vec3(mins.x, mins.y, maxs.z),
		Vec3(maxs.x, mins.y, maxs.z),
		Vec3(maxs.x, maxs.y, maxs.z),
		Vec3(mins.x, maxs.y, maxs.z),
	};

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
 * @brief
 */
static void R_UpdateUniforms(const r_view_t *view) {

	memset(&r_uniforms.block, 0, sizeof(r_uniforms.block));

	Matrix4x4_FromOrtho(&r_uniforms.block.projection2D, 0.0, r_context.width, r_context.height, 0.0, -1.0, 1.0);

	r_uniforms.block.brightness = r_brightness->value;
	r_uniforms.block.contrast = r_contrast->value;
	r_uniforms.block.saturation = r_saturation->value;
	r_uniforms.block.gamma = r_gamma->value;

	if (view) {
		r_uniforms.block.viewport = view->viewport;

		const float aspect = (view->viewport.z - view->viewport.x) / (view->viewport.w - view->viewport.y);

		const float ymax = tanf(Radians(view->fov.y));
		const float ymin = -ymax;

		const float xmin = ymin * aspect;
		const float xmax = ymax * aspect;

		Matrix4x4_FromFrustum(&r_uniforms.block.projection3D, xmin, xmax, ymin, ymax, 1.0, MAX_WORLD_DIST);

		Matrix4x4_CreateIdentity(&r_uniforms.block.view);

		Matrix4x4_ConcatRotate(&r_uniforms.block.view, -90.0, 1.0, 0.0, 0.0); // put Z going up
		Matrix4x4_ConcatRotate(&r_uniforms.block.view,  90.0, 0.0, 0.0, 1.0); // put Z going up

		Matrix4x4_ConcatRotate(&r_uniforms.block.view, -view->angles.z, 1.0, 0.0, 0.0);
		Matrix4x4_ConcatRotate(&r_uniforms.block.view, -view->angles.x, 0.0, 1.0, 0.0);
		Matrix4x4_ConcatRotate(&r_uniforms.block.view, -view->angles.y, 0.0, 0.0, 1.0);

		Matrix4x4_ConcatTranslate(&r_uniforms.block.view, -view->origin.x, -view->origin.y, -view->origin.z);

		r_uniforms.block.depth_range.x = 1.f;
		r_uniforms.block.depth_range.y = MAX_WORLD_DIST;

		r_uniforms.block.ticks = view->ticks;

		r_uniforms.block.modulate = r_modulate->value;
		
		r_uniforms.block.fog_density = r_fog_density->value;
		r_uniforms.block.fog_samples = r_fog_samples->integer;

		r_uniforms.block.resolution.x = r_context.width;
		r_uniforms.block.resolution.y = r_context.height;

		if (r_world_model) {
			r_uniforms.block.lightgrid.mins = Vec3_ToVec4(r_world_model->bsp->lightgrid->mins, 0.f);
			r_uniforms.block.lightgrid.maxs = Vec3_ToVec4(r_world_model->bsp->lightgrid->maxs, 0.f);

			const vec3_t pos = Vec3_Subtract(view->origin, r_world_model->bsp->lightgrid->mins);
			const vec3_t size = Vec3_Subtract(r_world_model->bsp->lightgrid->maxs, r_world_model->bsp->lightgrid->mins);

			r_uniforms.block.lightgrid.view_coordinate = Vec3_ToVec4(Vec3_Divide(pos, size), 0.f);
			r_uniforms.block.lightgrid.size = Vec3_ToVec4(Vec3i_CastVec3(r_world_model->bsp->lightgrid->size), 0.f);
		}
	}

	glBindBuffer(GL_UNIFORM_BUFFER, r_uniforms.buffer);
	glBufferData(GL_UNIFORM_BUFFER, sizeof(r_uniforms.block), &r_uniforms.block, GL_DYNAMIC_DRAW);
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
 * @brief
 */
static void R_Clear(const r_view_t *view) {

	GLbitfield bits = GL_DEPTH_BUFFER_BIT;

	if (r_clear->value || r_draw_wireframe->value) {
		bits |= GL_COLOR_BUFFER_BIT;
	}

	if (view) {
		if (view->contents & CONTENTS_SOLID) {
			bits |= GL_COLOR_BUFFER_BIT;
		}
	} else {
		bits |= GL_COLOR_BUFFER_BIT;
	}

	glClear(bits);

	R_GetError(NULL);
}

/**
 * @brief Called at the beginning of each render frame.
 */
void R_BeginFrame(void) {

	memset(&r_stats, 0, sizeof(r_stats));

	R_Clear(NULL);
}

/**
 * @brief
 */
void R_DrawViewDepth(r_view_t *view) {

	glBindFramebuffer(GL_FRAMEBUFFER, r_context.framebuffer);

	R_Clear(view);

	R_UpdateFrustum(view);

	R_UpdateUniforms(view);

	R_DrawDepthPass(view);

	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	R_GetError(NULL);
}

/**
 * @brief Blits the color attachment to the framebuffer.
 */
static void R_DrawColorAttachment(void) {

	const r_image_t img = {
		.texnum = r_context.color_attachment,
		.width = r_context.width,
		.height = -r_context.height
	};

	R_Draw2DImage(0, r_context.height, img.width, img.height, &img, color_white);

	R_GetError(NULL);
}

/**
 * @brief Main entry point for drawing the 3D view.
 */
void R_DrawView(r_view_t *view) {

	assert(view);

	R_DrawBspLightgrid(view);

	R_UpdateBlendDepth(view);

	R_UpdateEntities(view);

	R_UpdateLights(view);

	R_UpdateFlares(view);

	R_UpdateSprites(view);

	R_UpdateStains(view);

	glBindFramebuffer(GL_FRAMEBUFFER, r_context.framebuffer);

	if (r_draw_wireframe->value) {
		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	} else {
		R_DrawSky();
	}

	R_DrawWorld(view);

	R_DrawEntities(view, -1);

	if (r_draw_wireframe->value) {
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	}

	R_DrawSprites(view, -1);

	R_Draw3D();

	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	R_DrawColorAttachment();
}

/**
 * @brief Called at the end of each render frame.
 */
void R_EndFrame(void) {

	R_Draw2D();

	SDL_GL_SwapWindow(r_context.window);
}

/**
 * @brief Initializes console variables and commands for the renderer.
 */
static void R_InitLocal(void) {

	// development tools
	r_blend_depth_sorting = Cvar_Add("r_blend_depth_sorting", "1", CVAR_DEVELOPER, "Controls alpha blending sorting (developer tool)");
	r_clear = Cvar_Add("r_clear", "0", CVAR_DEVELOPER, "Controls buffer clearing (developer tool)");
	r_cull = Cvar_Add("r_cull", "1", CVAR_DEVELOPER, "Controls bounded box culling routines (developer tool)");
	r_draw_bsp_lightgrid = Cvar_Add("r_draw_bsp_lightgrid", "0", CVAR_DEVELOPER | CVAR_R_MEDIA, "Controls the rendering of BSP lightgrid textures (developer tool)");
	r_draw_bsp_normals = Cvar_Add("r_draw_bsp_normals", "0", CVAR_DEVELOPER, "Controls the rendering of BSP vertex normals (developer tool)");
	r_draw_entity_bounds = Cvar_Add("r_draw_entity_bounds", "0", CVAR_DEVELOPER, "Controls the rendering of entity bounding boxes (developer tool)");
	r_draw_material_stages = Cvar_Add("r_draw_material_stages", "1", CVAR_DEVELOPER, "Controls the rendering of material stage effects (developer tool)");
	r_draw_wireframe = Cvar_Add("r_draw_wireframe", "0", CVAR_DEVELOPER, "Controls the rendering of polygons as wireframe (developer tool)");
	r_depth_pass = Cvar_Add("r_depth_pass", "1", CVAR_DEVELOPER, "Controls the rendering of the depth pass (developer tool");
	r_get_error = Cvar_Add("r_get_error", "0", CVAR_DEVELOPER, "Log OpenGL errors to the console (developer tool)");
	r_occlude = Cvar_Add("r_occlude", "1", CVAR_DEVELOPER, "Controls the rendering of occlusion queries (developer tool)");

	// settings and preferences
	r_allow_high_dpi = Cvar_Add("r_allow_high_dpi", "1", CVAR_ARCHIVE | CVAR_R_CONTEXT, "Enables or disables support for High-DPI (Retina, 4K) display modes");
	r_anisotropy = Cvar_Add("r_anisotropy", "4", CVAR_ARCHIVE | CVAR_R_MEDIA, "Controls anisotropic texture filtering");
	r_brightness = Cvar_Add("r_brightness", "1", CVAR_ARCHIVE, "Controls texture brightness");
	r_bicubic = Cvar_Add("r_bicubic", "1", CVAR_ARCHIVE, "Enable or disable high quality texture filtering on lightmaps and stainmaps.");
	r_caustics = Cvar_Add("r_caustics", "1", CVAR_ARCHIVE | CVAR_R_MEDIA, "Enable or disable liquid caustic effects");
	r_contrast = Cvar_Add("r_contrast", "1", CVAR_ARCHIVE, "Controls texture contrast");
	r_display = Cvar_Add("r_display", "0", CVAR_ARCHIVE, "Specifies the default display to use");
	r_flares = Cvar_Add("r_flares", "1", CVAR_ARCHIVE, "Controls the rendering of light source flares");
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
	r_stains = Cvar_Add("r_stains", "1", CVAR_ARCHIVE, "Controls persistent stain effects.");
	r_swap_interval = Cvar_Add("r_swap_interval", "1", CVAR_ARCHIVE | CVAR_R_CONTEXT, "Controls vertical refresh synchronization. 0 disables, 1 enables, -1 enables adaptive VSync.");
	r_texture_mode = Cvar_Add("r_texture_mode", "GL_LINEAR_MIPMAP_LINEAR", CVAR_ARCHIVE | CVAR_R_MEDIA, "Specifies the active texture filtering mode");
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
 * @brief Creates the default 3d framebuffer.
 */
static void R_InitFramebuffer(void) {

	glGenFramebuffers(1, &r_context.framebuffer);
	glBindFramebuffer(GL_FRAMEBUFFER, r_context.framebuffer);

	R_GetError("Make framebuffer");

	glGenTextures(1, &r_context.color_attachment);
	glBindTexture(GL_TEXTURE_2D, r_context.color_attachment);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, r_context.drawable_width, r_context.drawable_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, r_context.color_attachment, 0);

	{
		const GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
		if (status != GL_FRAMEBUFFER_COMPLETE) {
			Com_Error(ERROR_FATAL, "Color attachment incomplete: %d\n", status);
		}
		R_GetError("Color attachment");
	}

	glGenTextures(1, &r_context.depth_stencil_attachment);
	glBindTexture(GL_TEXTURE_2D, r_context.depth_stencil_attachment);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH24_STENCIL8, r_context.drawable_width, r_context.drawable_height, 0, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, r_context.depth_stencil_attachment, 0);

	{
		const GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
		if (status != GL_FRAMEBUFFER_COMPLETE) {
			Com_Error(ERROR_FATAL, "Depth stencil attachment incomplete: %d\n", status);
		}
		R_GetError("Depth stencil attachment");
	}

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glBindTexture(GL_TEXTURE_2D, 0);
}


/**
 * @brief
 */
static void R_InitUniforms(void) {

	glGenBuffers(1, &r_uniforms.buffer);

	R_UpdateUniforms(NULL);
}

/**
 * @brief
 */
static void R_ShutdownUniforms(void) {

	glDeleteBuffers(1, &r_uniforms.buffer);
}

/**
 * @brief Destroys the default 3d framebuffer.
 */
static void R_ShutdownFramebuffer(void) {
	
	glDeleteFramebuffers(1, &r_context.framebuffer);
	glDeleteTextures(1, &r_context.color_attachment);
	glDeleteTextures(1, &r_context.depth_stencil_attachment);

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

	R_ShutdownFramebuffer();

	R_ShutdownContext();

	Mem_FreeTag(MEM_TAG_RENDERER);
}
