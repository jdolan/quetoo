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
cvar_t *r_draw_bsp_blocks;
cvar_t *r_draw_bsp_normals;
cvar_t *r_draw_bsp_voxels;
cvar_t *r_draw_entity_bounds;
cvar_t *r_draw_light_bounds;
cvar_t *r_draw_material_stages;
cvar_t *r_draw_wireframe;

cvar_t *r_ambient;
cvar_t *r_ambient_occlusion;
cvar_t *r_anisotropy;
cvar_t *r_antialias;
cvar_t *r_bloom;
cvar_t *r_bloom_iterations;
cvar_t *r_bloom_threshold;
cvar_t *r_caustics;
cvar_t *r_draw_scale;
cvar_t *r_finish;
cvar_t *r_framebuffer_scale;
cvar_t *r_fullscreen;
cvar_t *r_fullscreen_width;
cvar_t *r_fullscreen_height;
cvar_t *r_hardness;
cvar_t *r_lighting_distance;
cvar_t *r_modulate;
cvar_t *r_modulate_mesh;
cvar_t *r_saturation;
cvar_t *r_parallax;
cvar_t *r_parallax_shadow;
cvar_t *r_roughness;
cvar_t *r_screenshot_format;
cvar_t *r_shadows;
cvar_t *r_shadow_tile_size;
cvar_t *r_shadow_distance;
cvar_t *r_specularity;
cvar_t *r_swap_interval;
cvar_t *r_window_height;
cvar_t *r_window_width;
cvar_t *r_draw_stats;

/**
 * @brief The MSAA sample count for the 3D scene, snapshotted from r_antialias at
 * renderer init. The scene framebuffer and every 3D pipeline are built with this
 * count; changing r_antialias takes effect on the next renderer restart.
 */
SDL_GPUSampleCount r_scene_samples = SDL_GPU_SAMPLECOUNT_1;

/**
 * @brief Maps the r_antialias cvar to an SDL_gpu sample count.
 */
SDL_GPUSampleCount R_SampleCount(void) {
  switch (r_antialias->integer) {
    case 2:  return SDL_GPU_SAMPLECOUNT_2;
    case 4:  return SDL_GPU_SAMPLECOUNT_4;
    case 8:  return SDL_GPU_SAMPLECOUNT_8;
    default: return SDL_GPU_SAMPLECOUNT_1;
  }
}

/**
 * @brief Updates the global uniform buffer object with view and projection matrices for the current frame.
 */
void R_UpdateUniforms(const r_view_t *view) {

  struct r_uniform_block_t *out = &r_uniforms.block;
  memset(out, 0, sizeof(*out));

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
    out->saturation = r_saturation->value;
    out->caustics = r_caustics->value;
    out->ambient_occlusion = r_ambient_occlusion->value;
    out->lighting_distance = r_lighting_distance->value;
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

  // The block is pushed per-pass via CommandBuffer::pushVertexUniformData/pushFragmentUniformData.
}

/**
 * @brief Called at the beginning of each render frame.
 */
void R_BeginFrame(void) {

  if (r_draw_scale->modified) {
    R_UpdateContext();
    r_draw_scale->modified = false;
  }

  if (r_framebuffer_scale->modified) {
    // Recreate the scene framebuffer at the new scale (the cgame rebuilds it on a
    // pixel-size change); R_CreateFramebuffer reads r_framebuffer_scale. The 3D
    // scene renders at the scaled resolution and R_DrawPost up/downscales it into
    // the present framebuffer, leaving the UI at native resolution.
    SDL_PushEvent(&(SDL_Event) {
      .type = SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED,
    });
    r_framebuffer_scale->modified = false;
  }

  if (r_antialias->modified) {
    if (R_SampleCount() != r_scene_samples) {
      Com_Print("r_antialias will take effect on the next renderer restart\n");
    }
    r_antialias->modified = false;
  }

  if (r_swap_interval->modified) {
    r_swap_interval->modified = false; // TODO(#864): present mode via RenderDevice::setSwapchainParameters
  }

  memset(&r_stats, 0, sizeof(r_stats));

  // Acquire the frame and clear the present-target framebuffer. The UI (and,
  // later, the resolved 3D scene) composite into it; R_EndFrame presents.
  CommandBuffer *commands = $(r_context.device, beginFrame);
  if (commands) {

    const SDL_FColor clear_color = { 0.f, 0.f, 0.f, 1.f };
    const SDL_GPUColorTargetInfo color =
        $(r_context.device->framebuffer, colorTargetInfo, 0, SDL_GPU_LOADOP_CLEAR, SDL_GPU_STOREOP_STORE, &clear_color);

    RenderPass *pass = $(commands, beginRenderPass, &color, 1, NULL);
    pass = release(pass);
  }
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
 * @brief Renders the view depth pre-pass, updating frustum and uploading uniforms.
 */
void R_DrawViewDepth(r_view_t *view) {

  R_UpdateFrustum(view);

  R_UpdateUniforms(view);

  // The opaque BSP depth pre-pass. TODO(#864): move this into a dedicated scene
  // framebuffer shared with the main pass for early-Z, and reuse the linear-depth
  // target for soft particles.
  R_DrawDepthPass(view);
}

/**
 * @brief Entry point for drawing the main view.
 */
void R_DrawMainView(r_view_t *view) {

  assert(view);

  // TODO(#864): bring-up vertical slice — render the world BSP into the present
  // framebuffer with the depth_pass pipeline (flat color). Entities, sprites,
  // shadows, materials/lighting, and the dedicated scene framebuffer + post
  // pipeline are ported in later Phase 5 steps.

  // R_UpdateFrustum and R_UpdateUniforms ran in R_DrawViewDepth, before the
  // depth pre-pass; the frustum and globals are already current here.
  R_UpdateLights(view);

  R_UpdateSprites(view);

  R_UpdateDecals(view);

  R_DrawShadows(view);

  R_DrawBspEntities(view);

  if (r_models.world) {
    R_DrawSky(view, r_models.world->bsp);
  }

  R_DrawMeshEntities(view);

  // Decals project onto the opaque surfaces beneath them.
  R_DrawDecals(view);

  // Translucent world surfaces composite over all opaque geometry.
  R_DrawBlendBspEntities(view);

  R_DrawSprites(view);
}

/**
 * @brief Entry point for drawing the player model view.
 */
void R_DrawPlayerModelView(r_view_t *view) {

  // TODO(#864): player-model mesh pass not yet ported to SDL_gpu.
}

/**
 * @brief Called at the end of each render frame.
 */
void R_EndFrame(void) {

  R_Draw2D();

  if (r_context.device->commands) {
    $(r_context.device, endFrame);
  }
}

/**
 * @brief Initializes console variables and commands for the renderer.
 */
static void R_InitLocal(void) {

  // development tools
  r_alpha_test = Cvar_Add("r_alpha_test", "1", CVAR_DEVELOPER, "Controls alpha test (developer tool).");
  r_cull = Cvar_Add("r_cull", "1", CVAR_DEVELOPER, "Controls bounded box culling routines (developer tool).");
  r_draw_bsp_blocks = Cvar_Add("r_draw_bsp_blocks", "0", CVAR_DEVELOPER, "Controls the rendering of BSP block boundaries (developer tool).");
  r_draw_bsp_normals = Cvar_Add("r_draw_bsp_normals", "0", CVAR_DEVELOPER, "Controls the rendering of BSP vertex normals (developer tool).");
  r_draw_bsp_voxels = Cvar_Add("r_draw_bsp_voxels", "0", CVAR_DEVELOPER | CVAR_R_MEDIA, "Controls the rendering of BSP voxel textures (developer tool).");
  r_draw_entity_bounds = Cvar_Add("r_draw_entity_bounds", "0", CVAR_DEVELOPER, "Controls the rendering of entity bounding boxes (developer tool).");
  r_draw_light_bounds = Cvar_Add("r_draw_light_bounds", "0", CVAR_DEVELOPER, "Controls the rendering of light source bounding boxes (developer tool).");
  r_draw_material_stages = Cvar_Add("r_draw_material_stages", "1", CVAR_DEVELOPER, "Controls the rendering of material stage effects (developer tool).");
  r_draw_wireframe = Cvar_Add("r_draw_wireframe", "0", CVAR_DEVELOPER, "Controls the rendering of polygons as wireframe (developer tool).");
  r_depth_pass = Cvar_Add("r_depth_pass", "1", CVAR_DEVELOPER, "Controls the rendering of the depth pass (developer tool).");
  r_draw_stats = Cvar_Add("r_draw_stats", "0", CVAR_DEVELOPER, "Draw renderer performance statistics (developer tool).");

  // settings and preferences
  r_ambient = Cvar_Add("r_ambient", "1", CVAR_ARCHIVE, "Controls the intensity of ambient lighting.");
  r_ambient_occlusion = Cvar_Add("r_ambient_occlusion", "1", CVAR_ARCHIVE, "Controls the intensity of ambient occlusion. 0 = disabled, 1 = full.");
  r_anisotropy = Cvar_Add("r_anisotropy", "16", CVAR_ARCHIVE | CVAR_R_MEDIA, "Controls anisotropic texture filtering.");
  r_antialias = Cvar_Add("r_antialias", "0", CVAR_ARCHIVE, "MSAA sample count (0 = disabled, 2, 4, 8).");
  r_bloom = Cvar_Add("r_bloom", "4", CVAR_ARCHIVE, "Controls the intensity of bloom. 0 disables bloom.");
  r_bloom_iterations = Cvar_Add("r_bloom_iterations", "8", CVAR_ARCHIVE, "Controls the number of bloom blur iterations. Higher values produce softer, wider bloom.");
  r_bloom_threshold = Cvar_Add("r_bloom_threshold", "1.0", CVAR_ARCHIVE, "Controls the luminance threshold above which bloom is applied.");
  r_caustics = Cvar_Add("r_caustics", "1", CVAR_ARCHIVE, "Controls the intensity of liquid caustic effects");
  r_draw_scale = Cvar_Add("r_draw_scale", "1", CVAR_ARCHIVE, "Controls the render scale of 2D elements.");
  r_finish = Cvar_Add("r_finish", "0", CVAR_ARCHIVE, "Controls whether to finish before moving to the next renderer frame.");
  r_framebuffer_scale = Cvar_Add("r_framebuffer_scale", "1", CVAR_ARCHIVE, "Controls the render scale of 3D elements.");
  r_fullscreen = Cvar_Add("r_fullscreen", "1", CVAR_ARCHIVE | CVAR_R_CONTEXT, "Controls fullscreen mode. 1 = borderless, 2 = exclusive.");
  r_fullscreen_width = Cvar_Add("r_fullscreen_width", "0", CVAR_ARCHIVE | CVAR_R_CONTEXT, "Fullscreen resolution width. 0 uses the desktop resolution.");
  r_fullscreen_height = Cvar_Add("r_fullscreen_height", "0", CVAR_ARCHIVE | CVAR_R_CONTEXT, "Fullscreen resolution height. 0 uses the desktop resolution.");
  r_hardness = Cvar_Add("r_hardness", "1", CVAR_ARCHIVE, "Controls the hardness of bump-mapping effects.");
  r_lighting_distance = Cvar_Add("r_lighting_distance", "2048", CVAR_ARCHIVE, "Distance threshold for vertex lighting.");
  r_modulate = Cvar_Add("r_modulate", "1", CVAR_ARCHIVE, "Controls the brightness of static lighting.");
  r_modulate_mesh = Cvar_Add("r_modulate_mesh", "1", CVAR_ARCHIVE, "Controls the brightness of mesh model static lighting.");
  r_saturation = Cvar_Add("r_saturation", "1", CVAR_ARCHIVE, "Controls the color saturation of the rendered scene. 0 = grayscale, 1 = normal, 2 = vivid.");
  r_parallax = Cvar_Add("r_parallax", "1", CVAR_ARCHIVE, "Controls the intensity of parallax effects.");
  r_parallax_shadow = Cvar_Add("r_parallax_shadow", "1", CVAR_ARCHIVE, "Controls the intensity of parallax self-shadow effects.");
  r_roughness = Cvar_Add("r_roughness", "1", CVAR_ARCHIVE, "Controls the roughness of bump-mapping effects.");
  r_screenshot_format = Cvar_Add("r_screenshot_format", "jpg", CVAR_ARCHIVE, "Set your preferred screenshot format. Supports \"jpg\", \"png\", or \"tga\".");
  r_shadows = Cvar_Add("r_shadows", "1", CVAR_ARCHIVE, "Controls shadowmap rendering.");
  r_shadow_tile_size = Cvar_Add("r_shadow_tile_size", "256", CVAR_ARCHIVE | CVAR_R_CONTEXT, "Controls shadow atlas tile resolution (128-512).");
  r_shadow_distance = Cvar_Add("r_shadow_distance", "1024", CVAR_ARCHIVE, "Controls the distance at which mesh shadows are culled.");
  r_specularity = Cvar_Add("r_specularity", "1", CVAR_ARCHIVE, "Controls the specularity of bump-mapping effects.");
  r_swap_interval = Cvar_Add("r_swap_interval", "1", CVAR_ARCHIVE, "Controls vertical refresh synchronization. 0 disables, 1 enables, -1 enables adaptive VSync.");
  r_window_height = Cvar_Add("r_window_height", "1080", CVAR_ARCHIVE | CVAR_R_CONTEXT, "Controls the window height for windowed mode.");
  r_window_width = Cvar_Add("r_window_width", "1920", CVAR_ARCHIVE | CVAR_R_CONTEXT, "Controls the window width for windowed mode.");

  Cvar_ClearAll(CVAR_R_MASK);

  Cmd_Add("r_dump_images", R_DumpImages_f, CMD_RENDERER, "Dump all loaded images to disk (developer tool).");
  Cmd_Add("r_list_media", R_ListMedia_f, CMD_RENDERER, "List all currently loaded media (developer tool).");
  Cmd_Add("r_save_materials", R_SaveMaterials_f, CMD_RENDERER, "Write all of the loaded map materials to disk (developer tool).");
  Cmd_Add("r_save_mesh_configs", R_SaveMeshConfigs_f, CMD_RENDERER, "Write the mesh configs for the named model to disk (developer tool).");
  Cmd_Add("r_screenshot", R_Screenshot_f, CMD_SYSTEM | CMD_RENDERER, "Take a screenshot.");
}

/**
 * @brief Populates the GL config structure by querying the implementation.
 */
static void R_InitConfig(void) {

  memset(&r_config, 0, sizeof(r_config));

  r_config.renderer = SDL_GetGPUDeviceDriver(r_context.device->device);
  r_config.vendor = "SDL_gpu";
  r_config.version = SDL_GetGPUDeviceDriver(r_context.device->device);

  // TODO(#864): SDL_gpu does not expose device limits directly; use conservative
  // defaults that hold across the Metal/Vulkan/D3D12 backends we target. Revisit
  // if a backend reports smaller limits (e.g. via SDL properties when available).
  r_config.max_texunits = 16;
  r_config.max_texture_size = 16384;
  r_config.max_3d_texture_size = 2048;
  r_config.max_uniform_block_size = 65536;

  Com_Print(  "  Renderer:   ^2%s^7\n", r_config.renderer);
  Com_Print(  "  Vendor:     ^2%s^7\n", r_config.vendor);
  Com_Print(  "  Version:    ^2%s^7\n", r_config.version);
}

/**
 * @brief Creates the OpenGL context and initializes all GL state.
 */
void R_Init(void) {

  Com_Print("Video initialization...\n");

  R_InitLocal();

  R_InitContext();

  // Snapshot the MSAA sample count for the scene framebuffer and all 3D pipelines.
  r_scene_samples = R_SampleCount();

  // TODO(#864): the GL subsystems below are bypassed during the SDL_gpu port.
  // The UI renders through ObjectivelyGPU on r_context.device; the 3D scene, 2D
  // console, media/images and shaders are ported back in Phase 4/5.
  //
  R_InitConfig();
  R_InitMedia();
  R_InitLights();
  R_InitShadows();
  R_InitBspPipeline();
  R_InitModels();
  R_InitDraw2D();
  // R_InitImages();
  R_InitDepthPass();
  // R_InitDraw3D();
  R_InitSprites();
  R_InitDecals();
  R_InitSky();
  R_InitPost();

  const SDL_Rect bounds = r_context.window_bounds;
  const SDL_Rect viewport = r_context.viewport;
  
  Com_Print("Video initialized %dx%d (%dx%d)\n", bounds.w, bounds.h, viewport.w, viewport.h);
}

/**
 * @brief Shuts down the renderer, freeing all resources belonging to it,
 * including the GL context.
 */
void R_Shutdown(void) {

  Cmd_RemoveAll(CMD_RENDERER);

  // TODO(#864): GL subsystem teardown bypassed during the SDL_gpu port (see R_Init).
  // R_ShutdownMedia();
  // R_ShutdownDraw2D();
  // R_ShutdownDraw3D();
  // R_ShutdownDecals();
  // R_ShutdownSprites();
  // R_ShutdownSky();
  R_ShutdownPost();
  R_ShutdownDepthPass();

  R_ShutdownDraw2D();

  R_ShutdownModels();

  R_ShutdownShadows();

  R_ShutdownSky();

  R_ShutdownSprites();

  R_ShutdownDecals();

  R_ShutdownBspPipeline();

  R_ShutdownLights();

  R_ShutdownMedia();

  R_ShutdownContext();

  Mem_FreeTag(MEM_TAG_RENDERER);
}
