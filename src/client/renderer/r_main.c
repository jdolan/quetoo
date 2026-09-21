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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include "r_local.h"

RenderConfig rConfig;
RenderUniforms rUniforms;
RenderViewStats *rStats;

Cvar *r_alphaTest;
Cvar *r_cull;
Cvar *r_depthPass;
Cvar *r_drawBspBlocks;
Cvar *r_drawOcclusionQueries;
Cvar *r_drawBspNormals;
Cvar *r_drawBspVoxels;
Cvar *r_drawEntityBounds;
Cvar *r_drawLightBounds;
Cvar *r_drawMaterialStages;
Cvar *r_drawWireframe;
Cvar *r_occlude;
Cvar *r_portals;
Cvar *r_reflections;

Cvar *r_ambient;
Cvar *r_ambientOcclusion;
Cvar *r_anisotropy;
Cvar *r_antialias;
Cvar *r_bloom;
Cvar *r_bloomIterations;
Cvar *r_bloomThreshold;
Cvar *r_caustics;
Cvar *r_framebufferScale;
Cvar *r_fullscreen;
Cvar *r_fullscreenWidth;
Cvar *r_fullscreenHeight;
Cvar *r_gpuDriver;
Cvar *r_hardness;
Cvar *r_lightingDistance;
Cvar *r_modulate;
Cvar *r_modulateMesh;
Cvar *r_saturation;
Cvar *r_parallax;
Cvar *r_parallaxShadow;
Cvar *r_roughness;
Cvar *r_screenshotFormat;
Cvar *r_shadows;
Cvar *r_shadowTileSize;
Cvar *r_specularity;
Cvar *r_swapInterval;
Cvar *r_windowHeight;
Cvar *r_windowWidth;

/**
 * @brief MSAA sample count for the 3D scene.
 */
SDL_GPUSampleCount rSceneSamples = SDL_GPU_SAMPLECOUNT_1;

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
void R_UpdateUniforms(const RenderView *view) {

  struct RenderUniformBlock *out = &rUniforms.block;
  memset(out, 0, sizeof(*out));

  if (view) {
    out->viewport = view->viewport;

    const float aspect = view->viewport.z / (float) view->viewport.w;

    const float ymax = tanf(Radians(view->fov.y));
    const float ymin = -ymax;

    const float xmin = ymin * aspect;
    const float xmax = ymax * aspect;

    const Mat4 clip = MakeMat4((const float[]) {
      1.f, 0.f, 0.f, 0.f,
      0.f, 1.f, 0.f, 0.f,
      0.f, 0.f, .5f, 0.f,
      0.f, 0.f, .5f, 1.f
    });

    // a mirrored view swaps its horizontal frustum bounds, which negates the projection's x
    // column. `Mat4_LookAt` below never reads `view->right`: it derives its own x axis from
    // `cross(up, forward)`, and for a reflected basis that cross product comes back negated,
    // so the image it would otherwise draw is the mirror flipped left to right. Flipping clip
    // space in x undoes that -- and reverses the winding, which is why a mirrored view is drawn
    // with front faces culled
    const float left = view->mirrored ? xmax : xmin;
    const float right = view->mirrored ? xmin : xmax;

    out->projection3D = Mat4_Concat(clip, Mat4_FromFrustum(left, right, ymin, ymax, NEAR_DIST, MAX_WORLD_DIST));
    out->view = Mat4_LookAt(view->origin, Vec3_Add(view->origin, view->forward), view->up);

    out->skyProjection = Mat4_FromScale3(MakeVec3(-1.f, 1.f, 1.f));
    out->skyProjection = Mat4_ConcatTranslation(out->skyProjection, Vec3_Negate(view->origin));

    out->lightProjection = Mat4_Concat(clip, Mat4_FromFrustum(-1.f, 1.f, -1.f, 1.f, NEAR_DIST, MAX_WORLD_DIST));

    out->depthRange.x = NEAR_DIST;
    out->depthRange.y = MAX_WORLD_DIST;
    out->viewType = view->type;
    out->ticks = view->ticks;
    out->ambient = Vec3_Scale(view->ambient, r_ambient->value);
    out->modulate = r_modulate->value;
    out->saturation = r_saturation->value;
    out->caustics = r_caustics->value;
    out->ambientOcclusion = r_ambientOcclusion->value;
    out->lightingDistance = r_lightingDistance->value;
    out->editor = editor->integer;
    out->developer = developer->integer;

    // the player model preview must land all of its lookups on the one voxel of the fallback
    // buffers: clamping to a zero-sized grid would not, since clamp() with a low bound above
    // its high bound is undefined, and a zero-sized box would not either, since voxel_uvw
    // divides by it
    if (view->type == VIEW_PLAYER_MODEL) {
      out->voxels.mins = MakeVec4(0.f, 0.f, 0.f, 0.f);
      out->voxels.maxs = MakeVec4(1.f, 1.f, 1.f, 0.f);
      out->voxels.size = MakeVec4(1.f, 1.f, 1.f, 0.f);
    } else {
      const RenderBspVoxels *voxels = &rModels.world->bsp->voxels;

      out->voxels.mins = Vec3_ToVec4(voxels->bounds.mins, 0.f);
      out->voxels.maxs = Vec3_ToVec4(voxels->bounds.maxs, 0.f);

      const Vec3 pos = Vec3_Subtract(view->origin, voxels->bounds.mins);
      const Vec3 extents = Box3_Size(voxels->bounds);

      out->voxels.viewCoordinate = Vec3_ToVec4(Vec3_Divide(pos, extents), 0.f);
      out->voxels.size = Vec3_ToVec4(Vec3i_CastVec3(voxels->size), 0.f);
    }
  }
}

/**
 * @brief Applies @c r_swapInterval to the device's swapchain present mode.
 */
static void R_UpdateSwapInterval(void) {

  SDL_GPUPresentMode mode;
  switch (r_swapInterval->integer) {
    case -1: mode = SDL_GPU_PRESENTMODE_MAILBOX;   break;
    case  0: mode = SDL_GPU_PRESENTMODE_IMMEDIATE; break;
    default: mode = SDL_GPU_PRESENTMODE_VSYNC;     break;
  }

  if (mode != SDL_GPU_PRESENTMODE_VSYNC && !$(rContext.device, supportsPresentMode, mode)) {
    Com_Warn("Present mode %d unsupported by this device, falling back to VSYNC\n", mode);
    $(rContext.device, setSwapchainParameters, SDL_GPU_SWAPCHAINCOMPOSITION_SDR, SDL_GPU_PRESENTMODE_VSYNC);
    return;
  }

  if (!$(rContext.device, setSwapchainParameters, SDL_GPU_SWAPCHAINCOMPOSITION_SDR, mode)) {
    Com_Warn("Failed to set present mode %d: %s\n", mode, SDL_GetError());
  }
}

/**
 * @return The fill mode the world is rasterized with.
 * @details Fill mode is pipeline state under SDL_GPU, so this is read where a pipeline is built
 * rather than where one is bound, and `r_drawWireframe` rebuilds them all when it changes.
 * @remarks Vulkan gates line fill behind a device feature. SDL falls back to filled where it is
 * missing, so the cvar is quietly ignored on such a device rather than failing to build.
 */
SDL_GPUFillMode R_FillMode(void) {
  return r_drawWireframe->integer ? SDL_GPU_FILLMODE_LINE : SDL_GPU_FILLMODE_FILL;
}

/**
 * @brief Rebuilds every pipeline whose creation info is derived from a
 * pipeline-bound cvar (@c r_antialias, @c r_anisotropy, ...).
 */
static void R_UpdatePipelines(void) {

  R_UpdateBspPipeline();

  R_UpdateMeshPipeline();

  R_UpdateDepthPass();

  R_UpdateDraw3DPipeline();

  R_UpdateSky();

  R_UpdateSpritePipeline();

  R_UpdateDecalPipeline();

  R_UpdatePostPipeline();
}

/**
 * @brief Called at the beginning of each render frame.
 */
void R_BeginFrame(void) {

  if (r_framebufferScale->modified) {
    SDL_PushEvent(&(SDL_Event) {
      .type = SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED,
    });
    r_framebufferScale->modified = false;
  }

  if (r_antialias->modified) {
    const SDL_GPUSampleCount samples = R_SampleCount();
    if (samples != rSceneSamples) {
      rSceneSamples = samples;
      R_UpdatePipelines();
      SDL_PushEvent(&(SDL_Event) {
        .type = SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED,
      });
    }
    r_antialias->modified = false;
  }

  if (r_drawWireframe->modified) {
    R_UpdatePipelines();
    r_drawWireframe->modified = false;
  }

  if (r_anisotropy->modified) {
    r_anisotropy->value = Clampf(r_anisotropy->value, 0.f, 16.f);
    rContext.device->maxAnisotropy = r_anisotropy->value;
    R_UpdatePipelines();
    r_anisotropy->modified = false;
  }

  if (r_swapInterval->modified) {
    r_swapInterval->value = Clampf(r_swapInterval->value, -1.f, 1.f);
    R_UpdateSwapInterval();
    r_swapInterval->modified = false;
  }

  CommandBuffer *commands = $(rContext.device, beginFrame);
  if (commands) {
    const Framebuffer *fb = rContext.device->framebuffer;
    RenderPass *pass = $(commands, beginRenderPassWithFramebuffer, fb, SDL_GPU_LOADOP_CLEAR, SDL_GPU_STOREOP_STORE);
    pass = release(pass);
  }
}

/**
 * @brief Initializes the view, preparing it for a new frame.
 */
void R_InitView(RenderView *view) {

  view->ticks = (uint32_t) SDL_GetTicks();
  view->numBeams = 0;
  view->numSubviews = 0;
  view->numEntities = 0;
  view->numLights = 0;
  view->numSprites = 0;
  view->numSpriteInstances = 0;
  view->numDecals = 0;

  memset(&view->stats, 0, sizeof(view->stats));
}

/**
 * @brief Renders the depth pre-pass and occlusion queries for the view.
 */
void R_DrawViewDepth(RenderView *view) {

  rStats = &view->stats;

  R_UpdateFrustum(view);

  R_UpdateUniforms(view);

  CommandBuffer *commands = $(rContext.device, acquireCommandBuffer);

  R_DrawDepthPass(view, commands);

  R_DrawOcclusionQueries(view, commands);

  if (rDepthPipeline.fence) {
    $(commands, submit);
  } else {
    rDepthPipeline.fence = $(commands, submitAndFence);
  }

  release(commands);
}

/**
 * @brief Draws the main view.
 */
void R_DrawMainView(RenderView *view) {

  assert(view);

  rStats = &view->stats;

  CommandBuffer *commands = rContext.device->commands;
  if (!commands) {
    return;
  }

  {
    CopyPass *pass = $(commands, beginCopyPass);

    R_UpdateLights(view, pass);

    R_UpdateEntities(view, pass);

    R_UpdateSprites(view, pass);

    R_UpdateDecals(view, pass);

    R_UpdateDraw3D(view, pass);

    pass = release(pass);
  }

  R_DrawShadows(view);

  Framebuffer *framebuffer = view->framebuffer;

  const SDL_GPUColorTargetInfo color[] = {
    $(framebuffer, colorTargetInfo, 0, SDL_GPU_LOADOP_CLEAR, SDL_GPU_STOREOP_STORE),
    $(framebuffer, colorTargetInfo, 1, SDL_GPU_LOADOP_CLEAR, SDL_GPU_STOREOP_STORE),
  };

  const SDL_GPULoadOp depthLoadop = r_depthPass->integer ? SDL_GPU_LOADOP_LOAD : SDL_GPU_LOADOP_CLEAR;
  const SDL_GPUDepthStencilTargetInfo depth = $(framebuffer, depthTargetInfo, depthLoadop, SDL_GPU_STOREOP_STORE);

  {
    RenderPass *pass = $(commands, beginRenderPass, color, 2, &depth);

    R_DrawEntities(view, pass);

    R_DrawSprites(view, pass);

    R_Draw3D(view, pass);

    pass = release(pass);
  }

  $(framebuffer, swap);
}

/**
 * @brief Draws the player-model preview view.
 */
void R_DrawPlayerModelView(RenderView *view) {

  assert(view);

  rStats = &view->stats;

  CommandBuffer *commands = rContext.device->commands;
  if (!commands) {
    return;
  }

  R_UpdateUniforms(view);

  {
    CopyPass *pass = $(commands, beginCopyPass);

    R_UpdateLights(view, pass);

    R_UpdateEntities(view, pass);

    pass = release(pass);
  }

  R_DrawShadows(view);

  Framebuffer *framebuffer = view->framebuffer;

  const SDL_GPUColorTargetInfo color[] = {
    $(framebuffer, colorTargetInfo, 0, SDL_GPU_LOADOP_CLEAR, SDL_GPU_STOREOP_STORE),
    $(framebuffer, colorTargetInfo, 1, SDL_GPU_LOADOP_CLEAR, SDL_GPU_STOREOP_STORE),
  };
  const SDL_GPUDepthStencilTargetInfo depth =
    $(framebuffer, depthTargetInfo, SDL_GPU_LOADOP_CLEAR, SDL_GPU_STOREOP_STORE);

  RenderPass *pass = $(commands, beginRenderPass, color, 2, &depth);

  R_DrawMeshEntities(view, pass);

  pass = release(pass);

  $(framebuffer, swap);
}

/**
 * @brief Called at the end of each render frame.
 */
void R_EndFrame(void) {

  if (rContext.device->commands) {
    $(rContext.device, endFrame);
  }
}

/**
 * @brief Initializes console variables and commands for the renderer.
 */
static void R_InitLocal(void) {

  r_alphaTest = Cvar_Add("r_alphaTest", "1", CVAR_DEVELOPER, "Controls alpha test (developer tool).");
  r_cull = Cvar_Add("r_cull", "1", CVAR_DEVELOPER, "Controls bounded box culling routines (developer tool).");
  r_drawBspBlocks = Cvar_Add("r_drawBspBlocks", "0", CVAR_DEVELOPER, "Controls the rendering of BSP block boundaries (developer tool).");
  r_drawOcclusionQueries = Cvar_Add("r_drawOcclusionQueries", "0", CVAR_DEVELOPER, "Controls the rendering of occlusion query bounding boxes (developer tool).");
  r_drawBspNormals = Cvar_Add("r_drawBspNormals", "0", CVAR_DEVELOPER, "Controls the rendering of BSP vertex normals (developer tool).");
  r_drawBspVoxels = Cvar_Add("r_drawBspVoxels", "0", CVAR_DEVELOPER | CVAR_R_MEDIA, "Controls the rendering of BSP voxel textures (developer tool).");
  r_drawEntityBounds = Cvar_Add("r_drawEntityBounds", "0", CVAR_DEVELOPER, "Controls the rendering of entity bounding boxes (developer tool).");
  r_drawLightBounds = Cvar_Add("r_drawLightBounds", "0", CVAR_DEVELOPER, "Controls the rendering of light source bounding boxes (developer tool).");
  r_drawMaterialStages = Cvar_Add("r_drawMaterialStages", "1", CVAR_DEVELOPER, "Controls the rendering of material stage effects (developer tool).");
  r_depthPass = Cvar_Add("r_depthPass", "1", CVAR_DEVELOPER, "Controls the rendering of the depth pass (developer tool).");
  r_occlude = Cvar_Add("r_occlude", "1", CVAR_DEVELOPER, "Controls the rendering of occlusion queries (developer tool).");
  r_drawWireframe = Cvar_Add("r_drawWireframe", "0", CVAR_DEVELOPER, "Draws world geometry as wireframe (developer tool).");
  r_portals = Cvar_Add("r_portals", "1", CVAR_ARCHIVE, "Controls rendering the view through portal surfaces.");
  r_reflections = Cvar_Add("r_reflections", "1", CVAR_ARCHIVE, "Controls rendering reflections in reflective surfaces.");

  r_ambient = Cvar_Add("r_ambient", "1", CVAR_ARCHIVE, "Controls the intensity of ambient lighting.");
  r_ambientOcclusion = Cvar_Add("r_ambientOcclusion", "1", CVAR_ARCHIVE, "Controls the intensity of ambient occlusion. 0 = disabled, 1 = full.");
  r_anisotropy = Cvar_Add("r_anisotropy", "16", CVAR_ARCHIVE | CVAR_R_MEDIA, "Controls anisotropic texture filtering.");
  r_antialias = Cvar_Add("r_antialias", "0", CVAR_ARCHIVE, "MSAA sample count (0 = disabled, 2, 4, 8).");
  r_bloom = Cvar_Add("r_bloom", "4", CVAR_ARCHIVE, "Controls the intensity of bloom. 0 disables bloom.");
  r_bloomIterations = Cvar_Add("r_bloomIterations", "8", CVAR_ARCHIVE, "Controls the number of bloom blur iterations. Higher values produce softer, wider bloom.");
  r_bloomThreshold = Cvar_Add("r_bloomThreshold", "1.0", CVAR_ARCHIVE, "Controls the luminance threshold above which bloom is applied.");
  r_caustics = Cvar_Add("r_caustics", "1", CVAR_ARCHIVE, "Controls the intensity of liquid caustic effects");
  r_framebufferScale = Cvar_Add("r_framebufferScale", "1", CVAR_ARCHIVE, "Controls the render scale of 3D elements.");
  r_fullscreen = Cvar_Add("r_fullscreen", "1", CVAR_ARCHIVE | CVAR_R_CONTEXT, "Controls fullscreen mode. 1 = borderless, 2 = exclusive.");
  r_fullscreenWidth = Cvar_Add("r_fullscreenWidth", "0", CVAR_ARCHIVE | CVAR_R_CONTEXT, "Fullscreen resolution width. 0 uses the desktop resolution.");
  r_fullscreenHeight = Cvar_Add("r_fullscreenHeight", "0", CVAR_ARCHIVE | CVAR_R_CONTEXT, "Fullscreen resolution height. 0 uses the desktop resolution.");
  r_gpuDriver = Cvar_Add("r_gpuDriver", "", CVAR_NO_SET, "Forces the SDL_gpu backend driver: \"vulkan\", \"direct3d12\" or \"metal\". Empty lets SDL choose. Set via +set at the command line.");
  r_hardness = Cvar_Add("r_hardness", "1", CVAR_ARCHIVE, "Controls the hardness of bump-mapping effects.");
  r_lightingDistance = Cvar_Add("r_lightingDistance", "2048", CVAR_ARCHIVE, "Distance threshold for vertex lighting.");
  r_modulate = Cvar_Add("r_modulate", "1", CVAR_ARCHIVE, "Controls the brightness of static lighting.");
  r_modulateMesh = Cvar_Add("r_modulateMesh", "1", CVAR_ARCHIVE, "Controls the brightness of players and items, to increase their visibility.");
  r_saturation = Cvar_Add("r_saturation", "1", CVAR_ARCHIVE, "Controls the color saturation of the rendered scene. 0 = grayscale, 1 = normal, 2 = vivid.");
  r_parallax = Cvar_Add("r_parallax", "1", CVAR_ARCHIVE, "Controls the intensity of parallax effects.");
  r_parallaxShadow = Cvar_Add("r_parallaxShadow", "1", CVAR_ARCHIVE, "Controls the intensity of parallax self-shadow effects.");
  r_roughness = Cvar_Add("r_roughness", "1", CVAR_ARCHIVE, "Controls the roughness of bump-mapping effects.");
  r_screenshotFormat = Cvar_Add("r_screenshotFormat", "jpg", CVAR_ARCHIVE, "Set your preferred screenshot format. Supports \"jpg\", \"png\", or \"tga\".");
  r_shadows = Cvar_Add("r_shadows", "1", CVAR_ARCHIVE, "Controls shadowmap rendering.");
  r_shadowTileSize = Cvar_Add("r_shadowTileSize", "256", CVAR_ARCHIVE | CVAR_R_CONTEXT, "Controls shadow atlas tile resolution (128-512).");
  r_specularity = Cvar_Add("r_specularity", "1", CVAR_ARCHIVE, "Controls the specularity of bump-mapping effects.");
  r_swapInterval = Cvar_Add("r_swapInterval", "1", CVAR_ARCHIVE, "Controls vertical refresh synchronization. 0 disables, 1 enables, -1 enables mailbox (low latency, no tearing).");
  r_windowHeight = Cvar_Add("r_windowHeight", "1080", CVAR_ARCHIVE | CVAR_R_CONTEXT, "Controls the window height for windowed mode.");
  r_windowWidth = Cvar_Add("r_windowWidth", "1920", CVAR_ARCHIVE | CVAR_R_CONTEXT, "Controls the window width for windowed mode.");

  Cvar_ClearAll(CVAR_R_MASK);

  Cmd_Add("r_dumpImages", R_DumpImages_f, CMD_RENDERER, "Dump all loaded images to disk (developer tool).");
  Cmd_Add("r_listMedia", R_ListMedia_f, CMD_RENDERER, "List all currently loaded media (developer tool).");
  Cmd_Add("r_saveMaterials", R_SaveMaterials_f, CMD_RENDERER, "Write all of the loaded map materials to disk (developer tool).");
  Cmd_Add("r_saveMeshConfigs", R_SaveMeshConfigs_f, CMD_RENDERER, "Write the mesh configs for the named model to disk (developer tool).");
  Cmd_Add("r_screenshot", R_Screenshot_f, CMD_SYSTEM | CMD_RENDERER, "Take a screenshot.");
}

/**
 * @brief Populates the GL config structure by querying the implementation.
 */
static void R_InitConfig(void) {

  memset(&rConfig, 0, sizeof(rConfig));

  rConfig.renderer = SDL_GetGPUDeviceDriver(rContext.device->device);

  const SDL_PropertiesID properties = SDL_GetGPUDeviceProperties(rContext.device->device);
  if (properties == 0) {
    Com_Warn("Failed to query GPU device properties: %s\n", SDL_GetError());
  }

  rConfig.device = SDL_GetStringProperty(properties, SDL_PROP_GPU_DEVICE_NAME_STRING, "unknown");
  rConfig.vendor = SDL_GetStringProperty(properties, SDL_PROP_GPU_DEVICE_DRIVER_NAME_STRING, "unknown");
  rConfig.version = SDL_GetStringProperty(properties, SDL_PROP_GPU_DEVICE_DRIVER_VERSION_STRING, "unknown");

  rConfig.maxTexunits = 16;
  rConfig.maxTextureSize = 16384;
  rConfig.max3dTextureSize = 2048;
  rConfig.maxUniformBlockSize = 65536;

  Com_Print(  "  Renderer:   ^2%s^7\n", rConfig.renderer);
  Com_Print(  "  Device:     ^2%s^7\n", rConfig.device);
  Com_Print(  "  Vendor:     ^2%s^7\n", rConfig.vendor);
  Com_Print(  "  Version:    ^2%s^7\n", rConfig.version);
}

/**
 * @brief Creates the ObjectivelyGPU render device and initializes the renderer.
 */
void R_Init(void) {

  Com_Print("Video initialization...\n");

  R_InitLocal();

  R_InitContext();

  R_UpdateSwapInterval();
  r_swapInterval->modified = false;

  rSceneSamples = R_SampleCount();

  R_InitConfig();
  
  R_InitImages();
  
  R_InitMedia();
  
  R_InitLights();
  
  R_InitShadows();
  
  R_InitBspPipeline();
  
  R_InitModels();
  
  R_InitDepthPass();
  
  R_InitOcclusionQueries();
  
  R_InitDraw3D();
  
  R_InitSprites();
  
  R_InitDecals();
  
  R_InitSky();

  R_InitSubviews();

  R_InitPost();

  const SDL_Rect bounds = rContext.windowBounds;
  const float density = rContext.displayMode->pixel_density;

  Com_Print("Video initialized %dx%d (%dx%d)\n", bounds.w, bounds.h,
            (int32_t) (bounds.w * density), (int32_t) (bounds.h * density));
}

/**
 * @brief Shuts down the renderer and frees its resources.
 */
void R_Shutdown(void) {

  Cmd_RemoveAll(CMD_RENDERER);

  R_ShutdownDraw3D();
  R_ShutdownPost();
  R_ShutdownDepthPass();

  R_ShutdownModels();

  R_ShutdownShadows();

  R_ShutdownSky();

  R_ShutdownSubviews();

  R_ShutdownSprites();

  R_ShutdownDecals();

  R_ShutdownBspPipeline();

  R_ShutdownLights();

  R_ShutdownMedia();

  R_ShutdownOcclusionQueries();

  R_ShutdownContext();

  Mem_FreeTag(MEM_TAG_RENDERER);
}
