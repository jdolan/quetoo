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

cvar_t *r_alpha_test;
cvar_t *r_cull;
cvar_t *r_depth_pass;
cvar_t *r_draw_occlusion_queries;
cvar_t *r_draw_bsp_blocks;
cvar_t *r_draw_bsp_normals;
cvar_t *r_draw_bsp_voxels;
cvar_t *r_draw_entity_bounds;
cvar_t *r_draw_light_bounds;
cvar_t *r_draw_wireframe;
cvar_t *r_get_error;
cvar_t *r_error_level;
cvar_t *r_max_errors;
cvar_t *r_occlude;

int32_t r_error_count;

cvar_t *r_allow_high_dpi;
cvar_t *r_ambient;
cvar_t *r_anisotropy;
cvar_t *r_caustics;
cvar_t *r_display;
cvar_t *r_finish;
cvar_t *r_fog_density;
cvar_t *r_fog_samples;
cvar_t *r_fullscreen;
cvar_t *r_hardness;
cvar_t *r_height;
cvar_t *r_materials;
cvar_t *r_modulate;
cvar_t *r_parallax;
cvar_t *r_parallax_shadow;
cvar_t *r_roughness;
cvar_t *r_screenshot_format;
cvar_t *r_shadows;
cvar_t *r_shadow_cubemap_array_size;
cvar_t *r_shadow_distance;
cvar_t *r_specularity;
cvar_t *r_supersample;
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
  
  // reset error count, so as-it-happens errors can happen again.
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
 * @brief
 */
static void R_UpdateUniforms(const r_view_t *view) {

  struct r_uniform_block_t *out = &r_uniforms.block;
  memset(out, 0, sizeof(*out));

  out->projection2D = Mat4_FromOrtho(0.f, r_context.w, r_context.h, 0.f, -1.f, 1.f);

  if (view) {
    out->viewport = view->viewport;

    const float aspect = view->viewport.z / (float) view->viewport.w;

    const float ymax = tanf(Radians(view->fov.y));
    const float ymin = -ymax;

    const float xmin = ymin * aspect;
    const float xmax = ymax * aspect;

    out->projection3D = Mat4_FromFrustum(xmin, xmax, ymin, ymax, NEAR_DIST, MAX_WORLD_DIST);
    out->view = Mat4_LookAt(view->origin, Vec3_Add(view->origin, view->forward), view->up);

    out->sky_projection = Mat4_FromScale3(Vec3(-1.f, 1.f, 1.f)); // put Z going up
    out->sky_projection = Mat4_ConcatTranslation(out->sky_projection, Vec3_Negate(view->origin));

    out->light_projection = Mat4_FromFrustum(-1.f, 1.f, -1.f, 1.f, NEAR_DIST, MAX_WORLD_DIST);

    out->depth_range.x = NEAR_DIST;
    out->depth_range.y = MAX_WORLD_DIST;
    out->view_type = view->type;
    out->ticks = view->ticks;
    out->ambient = r_ambient->value * view->ambient;
    out->modulate = r_modulate->value;
    out->caustics = r_caustics->value;
    out->fog_density = r_fog_density->value;
    out->fog_samples = r_fog_samples->value;
    out->editor = editor->integer;
    out->developer = developer->integer;
    out->wireframe = r_draw_wireframe->integer;

    if (r_models.world) {
      const r_bsp_voxels_t *voxels = &r_models.world->bsp->voxels;

      out->voxels.mins = Vec3_ToVec4(voxels->bounds.mins, 0.f);
      out->voxels.maxs = Vec3_ToVec4(voxels->bounds.maxs, 0.f);

      const vec3_t pos = Vec3_Subtract(view->origin, voxels->bounds.mins);
      const vec3_t extents = Box3_Size(voxels->bounds);

      out->voxels.view_coordinate = Vec3_ToVec4(Vec3_Divide(pos, extents), 0.f);
      out->voxels.size = Vec3_ToVec4(Vec3i_CastVec3(voxels->size), 0.f);
    }
  }

  glBindBuffer(GL_UNIFORM_BUFFER, r_uniforms.buffer);
  glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(r_uniforms.block), &r_uniforms.block);
  glBindBuffer(GL_UNIFORM_BUFFER, 0);
}

/**
 * @brief Called at the beginning of each render frame.
 */
void R_BeginFrame(void) {

  memset(&r_stats, 0, sizeof(r_stats));

  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

/**
 * @brief Initializes the view, preparing it for a new frame.
 */
void R_InitView(r_view_t *view) {

  view->ticks = (uint32_t) SDL_GetTicks();
  view->num_beams = 0;
  view->num_entities = 0;
  view->num_lights = 0;
  view->num_sprites = 0;
  view->num_sprite_instances = 0;
  view->num_decals = 0;
}

/**
 * @brief
 */
void R_DrawViewDepth(r_view_t *view) {

  assert(view);
  assert(view->framebuffer);

  R_UpdateFrustum(view);

  R_UpdateUniforms(view);

  R_UpdateOcclusionQueries(view);

  R_ClearFramebuffer(view->framebuffer);

  glBindFramebuffer(GL_FRAMEBUFFER, view->framebuffer->name);

  glViewport(0, 0, view->framebuffer->width, view->framebuffer->height);

  R_DrawDepthPass(view);

  R_DrawOcclusionQueries(view);

  glViewport(0, 0, r_context.pw, r_context.ph);

  glBindFramebuffer(GL_FRAMEBUFFER, 0);

  R_GetError(NULL);
}

/**
 * @brief Entry point for drawing the main view.
 */
void R_DrawMainView(r_view_t *view) {

  assert(view);
  assert(view->framebuffer);

  R_UpdateEntities(view);

  thread_t *sprites = Thread_Create((ThreadRunFunc) R_UpdateSprites, view, THREAD_NONE);
  //thread_t *decals = Thread_Create((ThreadRunFunc) R_UpdateDecals, view, THREAD_NONE);

  R_UpdateLights(view);

  R_UpdateDecals(view);

  R_DrawShadows(view);

  glBindFramebuffer(GL_FRAMEBUFFER, view->framebuffer->name);
  glDrawBuffers(2, (const GLenum []) { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 });

  glViewport(0, 0, view->framebuffer->width, view->framebuffer->height);

  if (r_draw_wireframe->value) {
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
  }

  R_DrawEntities(view);

  Thread_Wait(sprites);
  
  R_DrawSprites(view);

  //Thread_Wait(decals);
  R_UpdateDecals(view);
  R_DrawDecals(view);

  if (r_draw_wireframe->value) {
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
  }

  R_Draw3D();

  glViewport(0, 0, r_context.pw, r_context.ph);

  glDrawBuffers(1, (const GLenum []) { GL_COLOR_ATTACHMENT0 });
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

/**
 * @brief Entry point for drawing the player model view.
 */
void R_DrawPlayerModelView(r_view_t *view) {

  assert(view);
  assert(view->framebuffer);

  R_UpdateUniforms(view);

  glBindBuffer(GL_UNIFORM_BUFFER, r_uniforms.buffer);
  glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(r_uniforms.block), &r_uniforms.block);
  glBindBuffer(GL_UNIFORM_BUFFER, 0);

  R_UpdateEntities(view);

  glBindFramebuffer(GL_FRAMEBUFFER, view->framebuffer->name);
  glDrawBuffers(2, (const GLenum []) { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 });

  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  glViewport(0, 0, view->framebuffer->width, view->framebuffer->height);

  R_DrawMeshEntities(view);

  glViewport(0, 0, r_context.pw, r_context.ph);

  glDrawBuffers(1, (const GLenum []) { GL_COLOR_ATTACHMENT0 });
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
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
  r_alpha_test = Cvar_Add("r_alpha_test", "1", CVAR_DEVELOPER, "Controls alpha test (developer tool)");
  r_cull = Cvar_Add("r_cull", "1", CVAR_DEVELOPER, "Controls bounded box culling routines (developer tool)");
  r_draw_occlusion_queries = Cvar_Add("r_draw_occlusion_queries", "0", CVAR_DEVELOPER , "Controls the rendering of occlusion queries (developer tool)");
  r_draw_bsp_blocks = Cvar_Add("r_draw_bsp_blocks", "0", CVAR_DEVELOPER, "Controls the rendering of BSP block boundaries (developer tool)");
  r_draw_bsp_normals = Cvar_Add("r_draw_bsp_normals", "0", CVAR_DEVELOPER, "Controls the rendering of BSP vertex normals (developer tool)");
  r_draw_bsp_voxels = Cvar_Add("r_draw_bsp_voxels", "0", CVAR_DEVELOPER | CVAR_R_MEDIA, "Controls the rendering of BSP voxel textures (developer tool)");
  r_draw_entity_bounds = Cvar_Add("r_draw_entity_bounds", "0", CVAR_DEVELOPER, "Controls the rendering of entity bounding boxes (developer tool)");
  r_draw_light_bounds = Cvar_Add("r_draw_light_bounds", "0", CVAR_DEVELOPER, "Controls the rendering of light source bounding boxes (developer tool)");
  r_draw_wireframe = Cvar_Add("r_draw_wireframe", "0", CVAR_DEVELOPER, "Controls the rendering of polygons as wireframe (developer tool)");
  r_depth_pass = Cvar_Add("r_depth_pass", "1", CVAR_DEVELOPER, "Controls the rendering of the depth pass (developer tool");
  r_get_error = Cvar_Add("r_get_error", "0", CVAR_DEVELOPER | CVAR_R_CONTEXT, "Log OpenGL information to the console. 2 will also cause a breakpoint for errors. (developer tool)");
  r_error_level = Cvar_Add("r_error_level", "2", CVAR_DEVELOPER, "Error level for more fine-tuned control over KHR_debug reporting. 0 will report all, up to 3 which will only report errors. (developer tool)");
  r_max_errors = Cvar_Add("r_max_errors", "8", CVAR_DEVELOPER, "The max number of errors before skipping error handlers (developer tool)");
  r_occlude = Cvar_Add("r_occlude", "1", CVAR_DEVELOPER, "Controls the rendering of occlusion queries (developer tool)");

  // settings and preferences
  r_allow_high_dpi = Cvar_Add("r_allow_high_dpi", "1", CVAR_ARCHIVE | CVAR_R_CONTEXT, "Enables or disables support for High-DPI (Retina, 4K) display modes");
  r_ambient = Cvar_Add("r_ambient", "1.0", CVAR_ARCHIVE, "Controls the intensity of ambient lighting");
  r_anisotropy = Cvar_Add("r_anisotropy", "16", CVAR_ARCHIVE | CVAR_R_MEDIA, "Controls anisotropic texture filtering");
  r_caustics = Cvar_Add("r_caustics", "1", CVAR_ARCHIVE, "Controls the intensity of liquid caustic effects");
  r_display = Cvar_Add("r_display", "0", CVAR_ARCHIVE, "Specifies the default display to use");
  r_finish = Cvar_Add("r_finish", "0", CVAR_ARCHIVE, "Controls whether to finish before moving to the next renderer frame.");
  r_fog_density = Cvar_Add("r_fog_density", "1", CVAR_ARCHIVE, "Controls the density of fog effects");
  r_fog_samples = Cvar_Add("r_fog_samples", "8", CVAR_ARCHIVE, "Controls the quality of fog effects");
  r_fullscreen = Cvar_Add("r_fullscreen", "1", CVAR_ARCHIVE | CVAR_R_CONTEXT, "Controls fullscreen mode. 1 = exclusive, 2 = borderless");
  r_hardness = Cvar_Add("r_hardness", "1", CVAR_ARCHIVE, "Controls the hardness of bump-mapping effects");
  r_height = Cvar_Add("r_height", "0", CVAR_ARCHIVE | CVAR_R_CONTEXT, NULL);
  r_materials = Cvar_Add("r_materials", "1", CVAR_DEVELOPER, "Controls the rendering of material stage effects.");
  r_modulate = Cvar_Add("r_modulate", "1", CVAR_ARCHIVE, "Controls the brightness of static lighting");
  r_parallax = Cvar_Add("r_parallax", "1", CVAR_ARCHIVE, "Controls the intensity of parallax effects.");
  r_parallax_shadow = Cvar_Add("r_parallax_shadow", "1", CVAR_ARCHIVE, "Controls the intensity of parallax self-shadow effects.");
  r_roughness = Cvar_Add("r_roughness", "1", CVAR_ARCHIVE, "Controls the roughness of bump-mapping effects.");
  r_screenshot_format = Cvar_Add("r_screenshot_format", "jpg", CVAR_ARCHIVE, "Set your preferred screenshot format. Supports \"jpg\", \"png\", or \"tga\".");
  r_shadows = Cvar_Add("r_shadows", "1", CVAR_ARCHIVE, "Controls shadowmap rendering.");
  r_shadow_cubemap_array_size = Cvar_Add("r_shadow_cubemap_array_size", "128", CVAR_ARCHIVE | CVAR_R_CONTEXT, "Controls shadowmap resolution.");
  r_shadow_distance = Cvar_Add("r_shadow_distance", "1024", CVAR_ARCHIVE, "Controls the distance at which mesh shadows are culled.");
  r_specularity = Cvar_Add("r_specularity", "1", CVAR_ARCHIVE, "Controls the specularity of bump-mapping effects.");
  r_supersample = Cvar_Add("r_supersample", "1", CVAR_ARCHIVE, "Controls supersampling (anti-aliasing).");
  r_swap_interval = Cvar_Add("r_swap_interval", "1", CVAR_ARCHIVE | CVAR_R_CONTEXT, "Controls vertical refresh synchronization. 0 disables, 1 enables, -1 enables adaptive VSync.");
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

  GLint num_extensions;
  glGetIntegerv(GL_NUM_EXTENSIONS, &num_extensions);

  glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &r_config.max_texunits);
  glGetIntegerv(GL_MAX_TEXTURE_SIZE, &r_config.max_texture_size);
  glGetIntegerv(GL_MAX_3D_TEXTURE_SIZE, &r_config.max_3d_texture_size);
  glGetIntegerv(GL_MAX_UNIFORM_BLOCK_SIZE, &r_config.max_uniform_block_size);

  Com_Print(  "  Renderer:           ^2%s^7\n", r_config.renderer);
  Com_Print(  "  Vendor:             ^2%s^7\n", r_config.vendor);
  Com_Print(  "  Version:            ^2%s^7\n", r_config.version);
  Com_Verbose("  Texture Units:      ^2%i^7\n", r_config.max_texunits);
  Com_Verbose("  Texture Size:       ^2%i^7\n", r_config.max_texture_size);
  Com_Verbose("  3D Texture Size     ^2%i^7\n", r_config.max_3d_texture_size);
  Com_Verbose("  Uniform block Size: ^2%i^7\n", r_config.max_uniform_block_size);

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

  glBindBufferBase(GL_UNIFORM_BUFFER, 0, r_uniforms.buffer);

  R_UpdateUniforms(NULL);

  glBindBuffer(GL_UNIFORM_BUFFER, r_uniforms.buffer);
  glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(r_uniforms.block), &r_uniforms.block);
  glBindBuffer(GL_UNIFORM_BUFFER, 0);
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

  R_InitOcclusionQueries();

  R_InitMedia();

  R_InitImages();

  R_InitDepthPass();

  R_InitShadows();

  R_InitDraw2D();

  R_InitDraw3D();

  R_InitModels();

  R_InitSprites();

  R_InitLights();

  R_InitDecals();

  R_InitSky();

  R_GetError("Video initialization");

  Com_Print("Video initialized %dx%d (%dx%d) %s\n",
        r_context.w, r_context.h,
        r_context.pw, r_context.ph,
        r_context.fullscreen ? "fullscreen" : "windowed");
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

  R_ShutdownDecals();

  R_ShutdownSprites();

  R_ShutdownSky();

  R_ShutdownShadows();

  R_ShutdownDepthPass();

  R_ShutdownOcclusionQueries();

  R_ShutdownUniforms();

  R_ShutdownContext();

  Mem_FreeTag(MEM_TAG_RENDERER);
}
