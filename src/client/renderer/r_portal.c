/*
 * Copyright(c) 1997-2001 id Software, Inc.
 * Copyright(c) 2002 The Quakeforge Project.
 * Copyright(c) 2006 Quetoo.
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

/**
 * @brief The factor by which portal framebuffers are smaller than the window, per axis.
 */
#define PORTAL_FRAMEBUFFER_DIVISOR 2

static struct {
  /**
   * @brief The views the portals of a frame are drawn with, one per layer of the framebuffer.
   * @details A map may hold far more portals than can be drawn, and each of these carries the
   * whole scene, so they are pooled and handed out to the portals a view offers rather than
   * allocated per portal of the world.
   */
  RenderView views[MAX_PORTALS];

  /**
   * @brief The framebuffer all portals render into, its color attachment holding one layer
   * per portal of the loaded world. Its depth and depth copy are scratch, reused by each
   * portal in turn, since portals are drawn one after another and nothing reads them after.
   */
  Framebuffer *framebuffer;

  /**
   * @brief The window size the framebuffer was created for, so that it follows the window.
   */
  SDL_Size size;

  /**
   * @brief A single-layer placeholder, bound when there is no portal framebuffer, since the
   * BSP fragment stage declares the sampler whether or not any face reads it.
   */
  Texture *nullTexture;
} r_portal;

/**
 * @brief Allocates the placeholder portal texture.
 */
void R_InitPortals(void) {
  r_portal.nullTexture = $(r_context.device, createSolidColorTexture, SDL_GPU_TEXTURETYPE_2D_ARRAY, 1, 0xff000000);
}

/**
 * @brief Releases the portal framebuffer and placeholder texture.
 */
void R_ShutdownPortals(void) {

  if (r_portal.framebuffer) {
    R_DestroyFramebuffer(r_portal.framebuffer);
    r_portal.framebuffer = NULL;
    r_portal.size = MakeSize(0, 0);
  }

  r_portal.nullTexture = release(r_portal.nullTexture);
}

/**
 * @return The array texture to bind to @p view's BSP portal sampler, never `NULL`.
 * @remarks A portal view is drawn into that very texture, and binding a color attachment as a
 * sampler in the pass writing it is undefined, whether or not any fragment reads it. Portal
 * views sample nothing, so they are given the placeholder.
 */
SDL_GPUTexture *R_PortalTexture(const RenderView *view) {

  if (r_portal.framebuffer && view->type != VIEW_PORTAL) {
    return $(r_portal.framebuffer, resolveColorTexture, 0)->texture;
  }

  return r_portal.nullTexture->texture;
}

/**
 * @brief Resolves @p portal into world space for the frame, through the model matrix of the
 * entity drawing its face.
 * @details A portal face's frame is baked in the space of the model that draws it, since the
 * compiler offsets a brush entity's geometry by its origin brush.
 */
static void R_UpdatePortal(RenderBspPortal *portal, const Mat4 matrix) {

  portal->absOrigin = Mat4_Transform(matrix, portal->origin);
  portal->absBounds = Mat4_TransformBounds(matrix, portal->bounds);
  const Vec3 normal = Mat4_RotateVector(matrix, portal->normal);

  portal->absPlane = (CmBspPlane) {
    .normal = normal,
    .dist = Vec3_Dot(portal->absOrigin, normal),
    .type = Cm_PlaneTypeForNormal(normal),
    .signBits = Cm_SignBitsForNormal(normal),
  };

  portal->matrix = Mat4_Concat(portal->exit, Mat4_Inverse(Mat4_Concat(matrix, portal->entry)));
}

/**
 * @brief Offers a portal for @p view to sample.
 * @details The portal's own view is placed here rather than by the client game. Every part of it
 * is the outer view carried through the portal, and its projection must match the outer view's
 * exactly for the two images to register, so there is nothing in it for a caller to decide.
 *
 * A view holds far fewer portals than a map may contain, and the scene is populated before any
 * of it is culled, so there is no knowing here which portals are actually visible. The nearest
 * are kept instead: portals are held in order of distance, and offering one farther than a full
 * view's last evicts nothing, while offering a nearer one drops that last portal and takes its
 * pooled view. Ordering is the renderer's business rather than the client game's, so that a
 * portal too far to matter cannot crowd out one in front of the player.
 * @param matrix The model matrix of the entity drawing @p portal's face, or the identity for a
 * portal on worldspawn or on anything else that does not move. A portal face's frame is baked in
 * the space of the model that draws it, so this is what carries it into the world.
 */
void R_AddPortal(RenderView *view, RenderBspPortal *portal, const Mat4 matrix) {

  assert(view);
  assert(portal);

  if (!r_portals->integer) {
    return;
  }

  // a portal the world dropped for want of valid draw elements has no frames to carry a camera
  // through, and no face to show one on
  if (!portal->model) {
    return;
  }

  view->stats.portalsOffered++;

  R_UpdatePortal(portal, matrix);

  // a portal face is single sided, and the BSP pipeline culls back faces, so from behind its
  // plane there is nothing of it to draw -- and a whole scene would be rendered into a layer
  // that no fragment goes on to sample. Tested against the camera's position rather than where
  // it happens to be looking: a portal off to the side is still plainly visible, so the view's
  // forward vector says nothing about whether this one can be seen
  if (Cm_DistanceToPlane(view->origin, &portal->absPlane) <= 0.f) {
    return;
  }

  const float dist = Vec3_DistanceSquared(portal->absOrigin, view->origin);

  int32_t i = view->numPortals;
  while (i > 0 && Vec3_DistanceSquared(view->portals[i - 1]->absOrigin, view->origin) > dist) {
    i--;
  }

  if (i == MAX_PORTALS) {
    return;
  }

  RenderView *pooled;

  if (view->numPortals == MAX_PORTALS) {
    RenderBspPortal *evicted = view->portals[--view->numPortals];
    pooled = evicted->view;
    evicted->view = NULL;
  } else {
    // an eviction is always followed by the insertion that caused it, so a view that is not full
    // has never evicted, and holds exactly the first `num_portals` views of the pool
    pooled = &r_portal.views[view->numPortals];
  }

  for (int32_t j = view->numPortals; j > i; j--) {
    view->portals[j] = view->portals[j - 1];
  }

  view->portals[i] = portal;
  view->numPortals++;

  // emptied here rather than left to the caller, since `R_UpdatePortalView` fills these arrays by
  // copy and relies on there being room for the whole scene
  R_InitView(pooled);

  // the projection must be the outer view's to the last bit: the face samples the portal at its
  // own screen coordinates, and the two images only register because both were drawn with it
  pooled->type = VIEW_PORTAL;
  pooled->viewport = view->viewport;
  pooled->fov = view->fov;
  pooled->depthRange = view->depthRange;
  pooled->ticks = view->ticks;
  pooled->ambient = view->ambient;

  // project the camera onto the portal's plane, clamped to the portal's bounds
  Vec3 origin = Box3_ClampPoint(portal->absBounds, view->origin);

  origin = Vec3_Subtract(origin, Vec3_Scale(portal->absPlane.normal,
                                            Cm_DistanceToPlane(origin, &portal->absPlane)));

  pooled->origin = Mat4_Transform(portal->matrix, origin);
  pooled->forward = Mat4_RotateVector(portal->matrix, view->forward);
  pooled->right = Mat4_RotateVector(portal->matrix, view->right);
  pooled->up = Mat4_RotateVector(portal->matrix, view->up);
  pooled->angles = Vec3_Euler(pooled->forward);

  Vec3 right, up;
  Vec3_Vectors(pooled->angles, NULL, &right, &up);
  pooled->angles.z = Degrees(atan2f(Vec3_Dot(pooled->up, right), Vec3_Dot(pooled->up, up)));

  portal->view = pooled;
}

/**
 * @brief Creates or resizes the portal framebuffer for the loaded world.
 * @remarks The color attachment is layered, one layer per portal, so that the BSP fragment
 * stage samples every portal from a single binding.
 * @details Portals render at half the window's resolution. A portal is sampled through a warping
 * material with further stages over it, so a full size layer per portal buys nothing that can be
 * seen, and costs a whole scene's fill rate each. The projection is unaffected: it comes from the
 * view's own viewport rather than from this size, so the image still registers with the face, and
 * only the rasterization is coarser.
 */
static void R_UpdatePortalFramebuffer(void) {

  const SDL_Size window = MakeSize(r_context.windowBounds.w, r_context.windowBounds.h);

  if (r_portal.framebuffer) {
    if (r_portal.size.w == window.w && r_portal.size.h == window.h) {
      return;
    }

    R_DestroyFramebuffer(r_portal.framebuffer);
  }

  r_portal.size = window;

  r_portal.framebuffer = R_CreateFramebuffer(&(GPU_FramebufferCreateInfo) {
    .size = MakeSize(window.w / PORTAL_FRAMEBUFFER_DIVISOR, window.h / PORTAL_FRAMEBUFFER_DIVISOR),
    .colorAttachments = {
      {
        .format = SDL_GPU_TEXTUREFORMAT_R11G11B10_UFLOAT,
        .clearColor = { 0.f, 0.f, 0.f, 1.f },
        .layerCount = MAX_PORTALS,
      },
      {
        .format = SDL_GPU_TEXTUREFORMAT_R32_FLOAT,
        .clearColor = { 1.f, 1.f, 1.f, 1.f },
        .doubleBuffered = true,
      },
    },
    .numColorTargets = 2,
    .depthAttachment = { .format = SDL_GPU_TEXTUREFORMAT_D32_FLOAT, .clearDepth = 1.f },
  });
}

/**
 * @return The rect of @p portal's face on screen, in the portal framebuffer's pixels.
 * @details A portal face samples the portal texture at its own screen coordinates, so the only
 * texels ever read are the ones beneath the face. Scissoring the portal's pass to them discards
 * nothing that could be sampled, and the further off a portal is the less of its layer it needs
 * -- the saving scales with distance without any of the resolution stepping, and its pop, that
 * choosing a size per portal would bring.
 * @remarks The whole framebuffer is returned for a face straddling the camera plane, which has
 * no finite rect to project onto.
 * @param vp The view-projection of the view being drawn around these, whose screen coordinates
 * the face will be sampled at. Taken as an argument rather than read from the uniform block,
 * which each portal drawn before this one has already replaced with its own.
 */
static SDL_Rect R_PortalScissor(const Mat4 vp, const RenderBspPortal *portal) {

  const SDL_Size size = r_portal.framebuffer->size;
  const SDL_Rect framebuffer = { 0, 0, size.w, size.h };

  Vec3 points[8];
  Box3_ToPoints(portal->absBounds, points);

  Vec2 mins = MakeVec2(FLT_MAX, FLT_MAX);
  Vec2 maxs = MakeVec2(-FLT_MAX, -FLT_MAX);

  for (int32_t i = 0; i < 8; i++) {

    const Vec3 p = points[i];

    const float w = p.x * vp.m[0][3] + p.y * vp.m[1][3] + p.z * vp.m[2][3] + vp.m[3][3];
    if (w <= FLT_EPSILON) {
      return framebuffer;
    }

    const Vec3 clip = Mat4_Transform(vp, p);

    mins = Vec2_Minf(mins, MakeVec2(clip.x / w, clip.y / w));
    maxs = Vec2_Maxf(maxs, MakeVec2(clip.x / w, clip.y / w));
  }

  // NDC to pixels, rounded outward, so that a face is never scissored short of its own edge
  const int32_t x0 = (int32_t) floorf((mins.x * .5f + .5f) * size.w);
  const int32_t x1 = (int32_t) ceilf((maxs.x * .5f + .5f) * size.w);
  const int32_t y0 = (int32_t) floorf((.5f - maxs.y * .5f) * size.h);
  const int32_t y1 = (int32_t) ceilf((.5f - mins.y * .5f) * size.h);

  const int32_t x = Maxi(x0, 0);
  const int32_t y = Maxi(y0, 0);

  return (SDL_Rect) {
    .x = x,
    .y = y,
    .w = Maxi(Mini(x1, size.w) - x, 0),
    .h = Maxi(Mini(y1, size.h) - y, 0),
  };
}

/**
 * @brief Draws one portal's view into its own layer of the portal framebuffer.
 */
static void R_DrawPortal(const RenderBspPortal *portal, const SDL_Rect *scissor) {

  CommandBuffer *commands = r_context.device->commands;

  RenderView *view = portal->view;

  view->framebuffer = r_portal.framebuffer;

  R_UpdateFrustum(view);

  R_UpdateUniforms(view);

  {
    CopyPass *pass = $(commands, beginCopyPass);

    R_UpdateLights(view, pass);

    R_UpdateEntities(view, pass);

    R_UpdateSprites(view, pass);

    pass = release(pass);
  }

  const SDL_GPUColorTargetInfo color[] = {
    $(r_portal.framebuffer, colorTargetInfoForLayer, 0, (Uint32) portal->layer, SDL_GPU_LOADOP_CLEAR, SDL_GPU_STOREOP_STORE),
    $(r_portal.framebuffer, colorTargetInfo, 1, SDL_GPU_LOADOP_CLEAR, SDL_GPU_STOREOP_STORE),
  };

  const SDL_GPUDepthStencilTargetInfo depth =
    $(r_portal.framebuffer, depthTargetInfo, SDL_GPU_LOADOP_CLEAR, SDL_GPU_STOREOP_STORE);

  RenderPass *pass = $(commands, beginRenderPass, color, 2, &depth);

  // the viewport first: RenderPass::setScissor clamps against it for want of the target's own
  // dimensions, so a scissor set before one is set collapses to nothing. The draws below set the
  // same viewport again for themselves
  $(pass, setViewport, &(SDL_GPUViewport) {
    .x = 0.f, .y = 0.f,
    .w = (float) r_portal.framebuffer->size.w, .h = (float) r_portal.framebuffer->size.h,
    .min_depth = 0.f, .max_depth = 1.f,
  });

  // set once for the pass: nothing below binds a scissor of its own, and the viewports they set
  // are separate state that leaves this alone
  $(pass, setScissor, scissor);

  R_DrawEntities(view, pass);

  R_DrawSprites(view, pass);

  pass = release(pass);
}

/**
 * @brief Repeats @p view's scene into @p out, the view of one of its portals, which culls it for
 * itself.
 * @details Done once the scene is complete rather than as each addition is made, so that nothing
 * depends on portals having been offered before the rest of the scene was populated, and only
 * for a portal that survived culling, so that one drawn nowhere is copied nowhere. This is the
 * same shape as `R_UpdateLights`, which likewise resolves per-light state only once every entity
 * that could cast a shadow is known.
 *
 * Decals are deliberately not repeated. `R_UpdateDecals` clips them into the shared, persistent
 * geometry of the blocks they land on, rather than into anything the view owns, so repeating
 * them would clip each decal once per portal and draw it that many times over.
 */
static void R_UpdatePortalView(const RenderView *view, RenderView *out) {

  assert(out->numEntities == 0);

  const RenderEntity *e = view->entities;
  for (int32_t j = 0; j < view->numEntities; j++, e++) {

    // the view weapon is placed relative to the camera it was added for, so it would appear
    // adrift in the world of any other view
    if (e->effects & EF_WEAPON) {
      continue;
    }

    out->entities[out->numEntities++] = *e;
  }

  memcpy(out->lights, view->lights, view->numLights * sizeof(out->lights[0]));
  out->numLights = view->numLights;

  memcpy(out->sprites, view->sprites, view->numSprites * sizeof(out->sprites[0]));
  out->numSprites = view->numSprites;

  memcpy(out->beams, view->beams, view->numBeams * sizeof(out->beams[0]));
  out->numBeams = view->numBeams;
}

/**
 * @brief Draws the views of all portals added this frame, for @p view to sample.
 * @details Portal views draw no shadows of their own: they copy the light list of the view
 * they are drawn for, so the atlas it rendered lines up. They draw portal faces on their plain
 * material rather than portalled, which is what keeps this from recursing; see
 * `R_PushBspPortalLayer`.
 *
 * Their particles are not softened. Softening blends against a double buffered copy of the
 * view's own depth, and one copy cannot serve several portals in a frame -- nor can each have
 * its own, since a render pass has four color targets -- so `sprite_fs` draws them hard rather
 * than against whichever portal was drawn last.
 * @param view The view being drawn around these, whose uniforms are restored before
 * returning, since `R_DrawMainView` relies on the ones `R_DrawViewDepth` wrote for it.
 */
void R_DrawPortals(const RenderView *view) {

  if (r_models.world) {
    RenderBspPortal *p = r_models.world->bsp->portals;
    for (int32_t i = 0; i < r_models.world->bsp->numPortals; i++, p++) {
      p->layer = -1;
    }
  }

  if (!view->numPortals || !r_context.device->commands) {
    return;
  }

  R_UpdatePortalFramebuffer();

  // captured before any portal is drawn, since drawing one replaces the uniform block with its
  // own view
  const Mat4 vp = Mat4_Concat(r_uniforms.block.projection3D, r_uniforms.block.view);

  RenderViewStats *stats = r_stats;

  int32_t layer = 0;
  for (int32_t i = 0; i < view->numPortals; i++) {

    RenderBspPortal *portal = view->portals[i];

    // the scene was populated before any of it was culled, so a portal may well have been
    // offered a view it turns out not to need
    if (R_CulludeBox(view, portal->absBounds)) {
      continue;
    }

    portal->layer = layer++;

    const SDL_Rect scissor = R_PortalScissor(vp, portal);
    if (scissor.w == 0 || scissor.h == 0) {
      continue;
    }

    stats->portalsDrawn++;

    R_UpdatePortalView(view, portal->view);

    r_stats = &portal->view->stats;
    R_DrawPortal(portal, &scissor);

    stats->portalsTriangles += portal->view->stats.bspTriangles + portal->view->stats.meshTriangles;
  }

  $(r_portal.framebuffer, swap);

  r_stats = stats;

  R_UpdateUniforms(view);
}
