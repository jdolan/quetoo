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

RenderConfig renderConfig;
RenderUniforms renderUniforms;
RenderDiagnostics *renderDiagnostics;

Cvar *r_alphaTest;
Cvar *r_cull;
Cvar *r_debugGroups;
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
Cvar *r_subviewQuality;

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
Cvar *r_gpuDriver;
Cvar *r_hardness;
Cvar *r_lightingDistance;
Cvar *r_modulate;
Cvar *r_modulateMesh;
Cvar *r_saturation;
Cvar *r_parallax;
Cvar *r_parallaxQuality;
Cvar *r_parallaxShadow;
Cvar *r_parallaxShadowQuality;
Cvar *r_roughness;
Cvar *r_screenshotFormat;
Cvar *r_shadows;
Cvar *r_shadowQuality;
Cvar *r_shadowTileSize;
Cvar *r_specularity;
Cvar *r_swapInterval;
Cvar *r_windowHeight;
Cvar *r_windowWidth;

/**
 * @brief MSAA sample count for the 3D scene.
 */
SDL_GPUSampleCount renderSceneSamples = SDL_GPU_SAMPLECOUNT_1;

int32_t R_QualityLevel(const Cvar *quality) {
  return clamp(quality->integer, 1, 3);
}

float R_QualityScale(const Cvar *quality) {
  return (float) (1 << (R_QualityLevel(quality) - 1)) * .25f;
}

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
 * @return @p view's clip plane in the space of @p viewMatrix, as a plane whose front is kept.
 * @remarks Derived through the view matrix rather than from the view's own basis vectors, which
 * do not all agree with the axes `Mat4_LookAt` derives for it. The matrix is rigid, so the normal
 * rotates by it and a point on the plane transforms by it.
 */
static Vec4 R_ViewClipPlane(const RenderView *view, const Mat4 viewMatrix) {

  const Vec3 normal = view->clipPlane.xyz;

  const Vec3 n = Vec3_Normalize(Mat4_RotateVector(viewMatrix, normal));
  const Vec3 p = Mat4_Transform(viewMatrix, Vec3_Scale(normal, view->clipPlane.w));

  return Vec3_ToVec4(n, -Vec3_Dot(p, n));
}

/**
 * @brief Skews @p projection so that its near plane becomes @p plane, given in view space.
 * @details Clipping geometrically rather than discarding fragments, which would cost the depth
 * pre-pass its early-out in every view sharing these shaders. Depth precision is skewed in
 * exchange, which no subview reads: it runs no depth pre-pass, and nothing samples the depth copy
 * it writes.
 * @remarks See Lengyel, "Oblique View Frustum Depth Projection and Clipping".
 */
static Mat4 R_ObliqueProjection(Mat4 projection, const Vec4 plane) {

  const Vec4 q = MakeVec4((SignOf(plane.x) + projection.m[2][0]) / projection.m[0][0],
                          (SignOf(plane.y) + projection.m[2][1]) / projection.m[1][1],
                          -1.f,
                          (1.f + projection.m[2][2]) / projection.m[3][2]);

  const float d = plane.x * q.x + plane.y * q.y + plane.z * q.z + plane.w * q.w;
  const Vec4 c = Vec4_Scale(plane, 2.f / d);

  projection.m[0][2] = c.x;
  projection.m[1][2] = c.y;
  projection.m[2][2] = c.z + 1.f;
  projection.m[3][2] = c.w;

  return projection;
}

/**
 * @brief Updates the global uniform buffer object with view and projection matrices for the current frame.
 */
void R_UpdateUniforms(const RenderView *view) {

  struct RenderUniformBlock *out = &renderUniforms.block;
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

    out->view = Mat4_LookAt(view->origin, Vec3_Add(view->origin, view->forward), view->up);

    Mat4 projection = Mat4_FromFrustum(xmin, xmax, ymin, ymax, NEAR_DIST, MAX_WORLD_DIST);

    // skewed before the clip matrix, whose depth remap the formula's frustum terms do not survive
    if (!Vec4_Equal(view->clipPlane, Vec4_Zero())) {
      projection = R_ObliqueProjection(projection, R_ViewClipPlane(view, out->view));
    }

    out->projection3D = Mat4_Concat(clip, projection);

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
    out->wireframe = r_drawWireframe->integer;
    out->parallaxShadow = r_parallaxShadow->integer;
    out->shadowSamples = (int32_t) (8.f * R_QualityScale(r_shadowQuality));
    out->parallaxSamples = R_QualityScale(r_parallaxQuality);
    out->parallaxShadowSamples = R_QualityScale(r_parallaxShadowQuality);
    out->parallaxRefineSteps = 1 << (R_QualityLevel(r_parallaxQuality) - 1);

    // the player model preview must land all of its lookups on the one voxel of the fallback
    // buffers: clamping to a zero-sized grid would not, since clamp() with a low bound above
    // its high bound is undefined, and a zero-sized box would not either, since voxel_uvw
    // divides by it
    if (view->type == VIEW_PLAYER_MODEL) {
      out->voxels.mins = MakeVec4(0.f, 0.f, 0.f, 0.f);
      out->voxels.maxs = MakeVec4(1.f, 1.f, 1.f, 0.f);
      out->voxels.size = MakeVec4(1.f, 1.f, 1.f, 0.f);
    } else {
      const RenderBspVoxels *voxels = &renderModels.world->bsp->voxels;

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

  if (mode != SDL_GPU_PRESENTMODE_VSYNC && !$(renderContext.device, supportsPresentMode, mode)) {
    Com_Warn("Present mode %d unsupported by this device, falling back to VSYNC\n", mode);
    $(renderContext.device, setSwapchainParameters, SDL_GPU_SWAPCHAINCOMPOSITION_SDR, SDL_GPU_PRESENTMODE_VSYNC);
    return;
  }

  if (!$(renderContext.device, setSwapchainParameters, SDL_GPU_SWAPCHAINCOMPOSITION_SDR, mode)) {
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
    if (samples != renderSceneSamples) {
      renderSceneSamples = samples;
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
    renderContext.device->maxAnisotropy = r_anisotropy->value;
    R_UpdatePipelines();
    r_anisotropy->modified = false;
  }

  if (r_swapInterval->modified) {
    r_swapInterval->value = Clampf(r_swapInterval->value, -1.f, 1.f);
    R_UpdateSwapInterval();
    r_swapInterval->modified = false;
  }

  CommandBuffer *commands = $(renderContext.device, beginFrame);
  if (commands) {
    const Framebuffer *fb = renderContext.device->framebuffer;
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
  view->clipPlane = Vec4_Zero();
  view->numEntities = 0;
  view->numLights = 0;
  view->numSprites = 0;
  view->numSpriteInstances = 0;
  view->numDecals = 0;

  memset(&view->diagnostics, 0, sizeof(view->diagnostics));
}

/**
 * @brief Opens a labelled GPU debug group on @p commands, if `r_debugGroups` is set.
 */
void R_PushDebugGroup(const CommandBuffer *commands, const char *name) {

  if (r_debugGroups->integer) {
    $(commands, pushDebugGroup, name);
  }
}

/**
 * @brief Closes the GPU debug group opened by `R_PushDebugGroup`.
 */
void R_PopDebugGroup(const CommandBuffer *commands) {

  if (r_debugGroups->integer) {
    $(commands, popDebugGroup);
  }
}

/**
 * @brief Renders the depth pre-pass and occlusion queries for the view.
 */
void R_DrawViewDepth(RenderView *view) {

  renderDiagnostics = &view->diagnostics;

  R_UpdateFrustum(view);

  R_UpdateUniforms(view);

  CommandBuffer *commands = $(renderContext.device, acquireCommandBuffer);

  R_PushDebugGroup(commands, "Depth pass");
  R_DrawDepthPass(view, commands);
  R_PopDebugGroup(commands);

  R_PushDebugGroup(commands, "Occlusion queries");
  R_DrawOcclusionQueries(view, commands);
  R_PopDebugGroup(commands);

  if (renderDepthPipeline.fence) {
    $(commands, submit);
  } else {
    renderDepthPipeline.fence = $(commands, submitAndFence);
  }

  release(commands);
}

/**
 * @brief Draws the main view.
 */
void R_DrawMainView(RenderView *view) {

  assert(view);

  renderDiagnostics = &view->diagnostics;

  CommandBuffer *commands = renderContext.device->commands;
  if (!commands) {
    return;
  }

  R_PushDebugGroup(commands, "Scene upload");
  {
    CopyPass *pass = $(commands, beginCopyPass);

    R_UpdateLights(view, pass);

    R_UpdateEntities(view, pass);

    R_UpdateSprites(view, pass);

    R_UpdateDecals(view, pass);

    R_UpdateDraw3D(view, pass);

    pass = release(pass);
  }
  R_PopDebugGroup(commands);

  R_PushDebugGroup(commands, "Shadows");
  R_DrawShadows(view);
  R_PopDebugGroup(commands);

  Framebuffer *framebuffer = view->framebuffer;

  const SDL_GPUColorTargetInfo color[] = {
    $(framebuffer, colorTargetInfo, 0, SDL_GPU_LOADOP_CLEAR, SDL_GPU_STOREOP_STORE),
    $(framebuffer, colorTargetInfo, 1, SDL_GPU_LOADOP_CLEAR, SDL_GPU_STOREOP_STORE),
  };

  const SDL_GPULoadOp depthLoadop = r_depthPass->integer ? SDL_GPU_LOADOP_LOAD : SDL_GPU_LOADOP_CLEAR;
  const SDL_GPUDepthStencilTargetInfo depth = $(framebuffer, depthTargetInfo, depthLoadop, SDL_GPU_STOREOP_STORE);

  R_PushDebugGroup(commands, "Main view");
  {
    RenderPass *pass = $(commands, beginRenderPass, color, 2, &depth);

    R_DrawEntities(view, pass);

    R_PushDebugGroup(commands, "Sprites");
    R_DrawSprites(view, pass);
    R_PopDebugGroup(commands);

    R_Draw3D(view, pass);

    pass = release(pass);
  }
  R_PopDebugGroup(commands);

  $(framebuffer, swap);
}

/**
 * @brief Draws the player-model preview view.
 */
void R_DrawPlayerModelView(RenderView *view) {

  assert(view);

  renderDiagnostics = &view->diagnostics;

  CommandBuffer *commands = renderContext.device->commands;
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

  if (renderContext.device->commands) {
    $(renderContext.device, endFrame);
  }
}

/**
 * @brief Initializes console variables and commands for the renderer.
 */
static void R_InitLocal(void) {

  r_alphaTest = Cvar_Add("r_alphaTest", "1", CVAR_DEVELOPER, "Controls alpha test (developer tool).");
  r_cull = Cvar_Add("r_cull", "1", CVAR_DEVELOPER, "Controls bounded box culling routines (developer tool).");
  r_debugGroups = Cvar_Add("r_debugGroups", "0", CVAR_DEVELOPER, "Labels GPU passes for frame capture and Metal System Trace (developer tool).");
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
  r_subviewQuality = Cvar_Add("r_subviewQuality", "3", CVAR_ARCHIVE, "Portal and reflection resolution per axis: 1 = quarter, 2 = third, 3 = half.");

  r_ambient = Cvar_Add("r_ambient", "1", CVAR_ARCHIVE, "Controls the intensity of ambient lighting.");
  r_ambientOcclusion = Cvar_Add("r_ambientOcclusion", "1", CVAR_ARCHIVE, "Controls the intensity of ambient occlusion. 0 = disabled, 1 = full.");
  r_anisotropy = Cvar_Add("r_anisotropy", "16", CVAR_ARCHIVE | CVAR_R_MEDIA, "Controls anisotropic texture filtering.");
  r_antialias = Cvar_Add("r_antialias", "0", CVAR_ARCHIVE, "MSAA sample count (0 = disabled, 2, 4, 8).");
  r_bloom = Cvar_Add("r_bloom", "4", CVAR_ARCHIVE, "Controls the intensity of bloom. 0 disables bloom.");
  r_bloomIterations = Cvar_Add("r_bloomIterations", "8", CVAR_ARCHIVE, "Controls the number of bloom blur iterations. Higher values produce softer, wider bloom.");
  r_bloomThreshold = Cvar_Add("r_bloomThreshold", "1.0", CVAR_ARCHIVE, "Controls the luminance threshold above which bloom is applied.");
  r_caustics = Cvar_Add("r_caustics", "1", CVAR_ARCHIVE, "Controls the intensity of liquid caustic effects.");
  r_framebufferScale = Cvar_Add("r_framebufferScale", "1", CVAR_ARCHIVE, "Controls the render scale of 3D elements.");
  r_fullscreen = Cvar_Add("r_fullscreen", "1", CVAR_ARCHIVE | CVAR_R_CONTEXT, "Controls fullscreen mode. 1 = borderless, 2 = exclusive.");
  r_gpuDriver = Cvar_Add("r_gpuDriver", "", CVAR_NO_SET, "Forces the SDL_gpu backend driver: \"vulkan\", \"direct3d12\" or \"metal\". Empty lets SDL choose. Set via +set at the command line.");
  r_hardness = Cvar_Add("r_hardness", "1", CVAR_ARCHIVE, "Controls the hardness of bump-mapping effects.");
  r_lightingDistance = Cvar_Add("r_lightingDistance", "2048", CVAR_ARCHIVE, "Distance threshold for vertex lighting.");
  r_modulate = Cvar_Add("r_modulate", "1", CVAR_ARCHIVE, "Controls the brightness of static lighting.");
  r_modulateMesh = Cvar_Add("r_modulateMesh", "1", CVAR_ARCHIVE, "Controls the brightness of players and items, to increase their visibility.");
  r_saturation = Cvar_Add("r_saturation", "1", CVAR_ARCHIVE, "Controls the color saturation of the rendered scene. 0 = grayscale, 1 = normal, 2 = vivid.");
  r_parallax = Cvar_Add("r_parallax", "1", CVAR_ARCHIVE, "Controls the intensity of parallax effects.");
  r_parallaxQuality = Cvar_Add("r_parallaxQuality", "3", CVAR_ARCHIVE, "Parallax quality: 1 = 16 samples / half relief, 2 = 32 samples / three-quarter relief, 3 = 64 samples / full relief.");
  r_parallaxShadow = Cvar_Add("r_parallaxShadow", "1", CVAR_ARCHIVE, "Enables parallax self-shadows.");
  r_parallaxShadowQuality = Cvar_Add("r_parallaxShadowQuality", "3", CVAR_ARCHIVE, "Parallax self-shadow quality: 1 = 4, 2 = 8, 3 = 16 samples maximum.");
  r_roughness = Cvar_Add("r_roughness", "1", CVAR_ARCHIVE, "Controls the roughness of bump-mapping effects.");
  r_screenshotFormat = Cvar_Add("r_screenshotFormat", "jpg", CVAR_ARCHIVE, "Set your preferred screenshot format. Supports \"jpg\", \"png\", or \"tga\".");
  r_shadows = Cvar_Add("r_shadows", "1", CVAR_ARCHIVE, "Controls shadowmap rendering.");
  r_shadowQuality = Cvar_Add("r_shadowQuality", "3", CVAR_ARCHIVE, "Shadow PCF quality: 1 = 2, 2 = 4, 3 = 8 taps maximum.");
  r_shadowTileSize = Cvar_Add("r_shadowTileSize", "256", CVAR_ARCHIVE | CVAR_R_CONTEXT, "Controls shadow atlas tile resolution (128-512).");
  r_specularity = Cvar_Add("r_specularity", "1", CVAR_ARCHIVE, "Controls the specularity of bump-mapping effects.");
  r_swapInterval = Cvar_Add("r_swapInterval", "1", CVAR_ARCHIVE, "Controls vertical refresh synchronization. 0 disables, 1 enables, -1 enables mailbox (low latency, no tearing).");
  r_windowHeight = Cvar_Add("r_windowHeight", "1080", CVAR_ARCHIVE | CVAR_R_CONTEXT, "Controls the window height for windowed mode.");
  r_windowWidth = Cvar_Add("r_windowWidth", "1920", CVAR_ARCHIVE | CVAR_R_CONTEXT, "Controls the window width for windowed mode.");

  Cvar_ClearAll(CVAR_R_MASK);

  Cmd_Add("r_dumpImages", R_DumpImages_f, CMD_RENDERER, "Dump all loaded images to disk (developer tool).");
  Cmd_Add("r_listMedia", R_ListMedia_f, CMD_RENDERER, "List all currently loaded media (developer tool).");
  Cmd_Add("r_saveMaterials", R_SaveMaterials_f, CMD_RENDERER, "Write all of the loaded map materials to disk (developer tool).");
  Cmd_Add("r_saveMeshConfigs", R_SaveMeshConfigs_f, CMD_RENDERER, "Write the world configs of all edited mesh models to disk (developer tool).");
  Cmd_Add("r_screenshot", R_Screenshot_f, CMD_SYSTEM | CMD_RENDERER, "Take a screenshot.");
}

/**
 * @brief Populates the GL config structure by querying the implementation.
 */
static void R_InitConfig(void) {

  memset(&renderConfig, 0, sizeof(renderConfig));

  renderConfig.renderer = SDL_GetGPUDeviceDriver(renderContext.device->device);

  const SDL_PropertiesID properties = SDL_GetGPUDeviceProperties(renderContext.device->device);
  if (properties == 0) {
    Com_Warn("Failed to query GPU device properties: %s\n", SDL_GetError());
  }

  renderConfig.device = SDL_GetStringProperty(properties, SDL_PROP_GPU_DEVICE_NAME_STRING, "unknown");
  renderConfig.vendor = SDL_GetStringProperty(properties, SDL_PROP_GPU_DEVICE_DRIVER_NAME_STRING, "unknown");
  renderConfig.version = SDL_GetStringProperty(properties, SDL_PROP_GPU_DEVICE_DRIVER_VERSION_STRING, "unknown");

  renderConfig.maxTexunits = 16;
  renderConfig.maxTextureSize = 16384;
  renderConfig.max3dTextureSize = 2048;
  renderConfig.maxUniformBlockSize = 65536;

  Com_Print(  "  Renderer:   ^2%s^7\n", renderConfig.renderer);
  Com_Print(  "  Device:     ^2%s^7\n", renderConfig.device);
  Com_Print(  "  Vendor:     ^2%s^7\n", renderConfig.vendor);
  Com_Print(  "  Version:    ^2%s^7\n", renderConfig.version);
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

  renderSceneSamples = R_SampleCount();

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

  const SDL_Rect bounds = renderContext.windowBounds;
  const float density = renderContext.displayMode->pixel_density;

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
