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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include "r_local.h"

/**
 * @brief The factor by which subview framebuffers are smaller than the window, per axis.
 */
#define SUBVIEW_FRAMEBUFFER_DIVISOR 2

static struct {
  /**
   * @brief The views the subviews of a frame are drawn with, one per layer of the framebuffer.
   * @details A map may hold far more subviews than can be drawn, and each of these carries the
   * whole scene, so they are pooled and handed out to the subviews a view offers rather than
   * allocated per subview of the world.
   */
  RenderView views[MAX_SUBVIEWS];

  /**
   * @brief The framebuffer all subviews render into, its color attachment holding one layer
   * per subview of the loaded world. Its depth and depth copy are scratch, reused by each
   * subview in turn, since subviews are drawn one after another and nothing reads them after.
   */
  Framebuffer *framebuffer;

  /**
   * @brief The window size the framebuffer was created for, so that it follows the window.
   */
  SDL_Size size;

  /**
   * @brief A single-layer placeholder, bound when there is no subview framebuffer, since the
   * BSP fragment stage declares the sampler whether or not any face reads it.
   */
  Texture *nullTexture;
} module;

/**
 * @brief Allocates the placeholder subview texture.
 */
void R_InitSubviews(void) {
  module.nullTexture = $(rContext.device, createSolidColorTexture, SDL_GPU_TEXTURETYPE_2D_ARRAY, 1, 0xff000000);
}

/**
 * @brief Releases the subview framebuffer and placeholder texture.
 */
void R_ShutdownSubviews(void) {

  if (module.framebuffer) {
    R_DestroyFramebuffer(module.framebuffer);
    module.framebuffer = NULL;
    module.size = MakeSize(0, 0);
  }

  module.nullTexture = release(module.nullTexture);
}

/**
 * @return The array texture to bind to @p view's BSP subview sampler, never `NULL`.
 * @remarks A subview is drawn into that very texture, and binding a color attachment as a
 * sampler in the pass writing it is undefined, whether or not any fragment reads it. Subviews
 * sample nothing, so they are given the placeholder.
 */
SDL_GPUTexture *R_SubviewTexture(const RenderView *view) {

  if (module.framebuffer && view->type != VIEW_SUBVIEW) {
    return $(module.framebuffer, resolveColorTexture, 0)->texture;
  }

  return module.nullTexture->texture;
}

/**
 * @brief Resolves @p subview's face into world space for the frame, through the model matrix of
 * the entity drawing it.
 * @details A face's frame is baked in the space of the model that draws it, since the compiler
 * offsets a brush entity's geometry by its origin brush.
 */
static void R_UpdateSubview(RenderSubview *subview, const Mat4 matrix) {

  subview->absOrigin = Mat4_Transform(matrix, subview->origin);
  subview->absBounds = Mat4_TransformBounds(matrix, subview->bounds);
  const Vec3 normal = Mat4_RotateVector(matrix, subview->normal);

  subview->absPlane = (CmBspPlane) {
    .normal = normal,
    .dist = Vec3_Dot(subview->absOrigin, normal),
    .type = Cm_PlaneTypeForNormal(normal),
    .signBits = Cm_SignBitsForNormal(normal),
  };
}

/**
 * @brief Resolves @p portal into world space, and composes the transform that carries a camera
 * from its face to the point it views the world from.
 */
static void R_UpdatePortal(RenderSubview *portal, const Mat4 matrix) {

  R_UpdateSubview(portal, matrix);

  portal->matrix = Mat4_Concat(portal->exit, Mat4_Inverse(Mat4_Concat(matrix, portal->entry)));
}

/**
 * @return The squared distance from @p view's camera to the nearest point of @p subview's face.
 * @remarks The nearest point rather than the center, so that a lake the camera stands in does not
 * lose the pool to a portal across the map on the strength of where its middle happens to be.
 */
static float R_SubviewDistance(const RenderView *view, const RenderSubview *subview) {
  return Vec3_DistanceSquared(Box3_ClampPoint(subview->absBounds, view->origin), view->origin);
}

/**
 * @brief Takes @p subview into @p view's pool, and places the camera it is drawn with.
 * @details A view holds far fewer subviews than a map may contain, and the scene is populated
 * before any of it is culled, so there is no knowing here which are actually visible. The nearest
 * are kept instead: subviews are held in order of distance, and offering one farther than a full
 * view's last evicts nothing, while offering a nearer one drops that last subview and takes its
 * pooled view. Ordering is the renderer's business rather than the caller's, so that one too far
 * to matter cannot crowd out one in front of the player.
 *
 * Portals and reflections share this pool. Both are a whole scene rendered into a layer, so there
 * is no reason to reserve layers for either, and the distance sort arbitrates between them.
 * @param origin The point in world space whose image the camera shows, carried by @p matrix. A
 * portal passes its face, flattened onto its own plane; a reflection passes the eye itself.
 * @param matrix The transform that carries a point or direction from the world into the frame the
 * camera is placed in.
 * @param clipPlane The plane, in world space, whose front the camera keeps, or zero for none.
 * @param mirrored Whether @p matrix reverses handedness. Passed rather than derived, since `Mat4`
 * has no determinant.
 * @return `true` if @p subview was taken into the pool.
 */
static bool R_AddSubview(RenderView *view, RenderSubview *subview, const Vec3 origin,
                         const Mat4 matrix, const Vec4 clipPlane, bool mirrored) {

  view->stats.subviewsOffered++;

  // a subview face is single sided, and the BSP pipeline culls back faces, so from behind its
  // plane there is nothing of it to draw -- and a whole scene would be rendered into a layer
  // that no fragment goes on to sample. Tested against the camera's position rather than where
  // it happens to be looking: a face off to the side is still plainly visible, so the view's
  // forward vector says nothing about whether this one can be seen
  // the epsilon matters once the near plane is skewed onto the surface: at the waterline that
  // divides by a vanishing distance, and an eye exactly there is a routine gameplay state
  if (Cm_DistanceToPlane(view->origin, &subview->absPlane) <= ON_EPSILON) {
    return false;
  }

  const float dist = R_SubviewDistance(view, subview);

  int32_t i = view->numSubviews;
  while (i > 0 && R_SubviewDistance(view, view->subviews[i - 1]) > dist) {
    i--;
  }

  if (i == MAX_SUBVIEWS) {
    return false;
  }

  RenderView *pooled;

  if (view->numSubviews == MAX_SUBVIEWS) {
    RenderSubview *evicted = view->subviews[--view->numSubviews];
    pooled = evicted->view;
    evicted->view = NULL;
  } else {
    // an eviction is always followed by the insertion that caused it, so a view that is not full
    // has never evicted, and holds exactly the first `numSubviews` views of the pool
    pooled = &module.views[view->numSubviews];
  }

  for (int32_t j = view->numSubviews; j > i; j--) {
    view->subviews[j] = view->subviews[j - 1];
  }

  view->subviews[i] = subview;
  view->numSubviews++;

  // emptied here rather than left to the caller, since `R_UpdateSubviewScene` fills these
  // arrays by copy and relies on there being room for the whole scene
  R_InitView(pooled);

  // the projection must be the outer view's to the last bit: the face samples its subview at its
  // own screen coordinates, and the two images only register because both were drawn with it
  pooled->type = VIEW_SUBVIEW;
  pooled->mirrored = mirrored;
  pooled->clipPlane = clipPlane;
  pooled->viewport = view->viewport;
  pooled->fov = view->fov;
  pooled->depthRange = view->depthRange;
  pooled->ticks = view->ticks;
  pooled->ambient = view->ambient;

  pooled->origin = Mat4_Transform(matrix, origin);
  pooled->forward = Mat4_RotateVector(matrix, view->forward);
  pooled->right = Mat4_RotateVector(matrix, view->right);
  pooled->up = Mat4_RotateVector(matrix, view->up);

  // `R_UpdateFrustum` reads `right` symmetrically, so a mirrored basis still bounds the volume
  // the image is drawn from. The Euler angles cannot describe a mirrored frame at all, and
  // nothing reads a subview's angles
  pooled->angles = Vec3_Euler(pooled->forward);

  Vec3 right, up;
  Vec3_Vectors(pooled->angles, NULL, &right, &up);
  pooled->angles.z = Degrees(atan2f(Vec3_Dot(pooled->up, right), Vec3_Dot(pooled->up, up)));

  subview->view = pooled;

  return true;
}

/**
 * @return The matrix that mirrors a point or a direction about @p plane.
 * @remarks A Householder reflection: the linear part is `I - 2nn'`, and the translation is `2dn`.
 * `Mat4` takes a row-vector convention, so the translation is the last literal row, as it is in
 * `Mat4_FromFrustum`.
 */
static Mat4 R_ReflectionMatrix(const CmBspPlane *plane) {

  const Vec3 n = plane->normal;
  const float d = plane->dist;

  return MakeMat4((const float[]) {
    1.f - 2.f * n.x * n.x,      -2.f * n.y * n.x,      -2.f * n.z * n.x, 0.f,
         -2.f * n.x * n.y, 1.f - 2.f * n.y * n.y,      -2.f * n.z * n.y, 0.f,
         -2.f * n.x * n.z,      -2.f * n.y * n.z, 1.f - 2.f * n.z * n.z, 0.f,
          2.f * d * n.x,         2.f * d * n.y,         2.f * d * n.z,   1.f,
  });
}

/**
 * @brief Offers a reflection for @p view to sample.
 * @details Where a portal carries the camera through a pair of frames a mapper set up, a
 * reflection mirrors the camera about the face's own plane, which the face supplies. There is
 * nothing to pair and nothing to place, so unlike `R_AddPortal` this is not offered by the client
 * game: the renderer walks the scene's inline models for itself.
 * @param matrix The model matrix of the entity drawing @p reflection's faces.
 */
static void R_AddReflection(RenderView *view, RenderSubview *reflection, const Mat4 matrix) {

  assert(view);
  assert(reflection);

  if (!r_reflections->integer) {
    return;
  }

  R_UpdateSubview(reflection, matrix);

  // the camera sits under the surface, where that surface is the nearest thing in front of it.
  // The plane is raised slightly clear of it: coincident, the surface's own fragments sit on the
  // boundary and shimmer, and the skewed depth range has no margin at the waterline
  const Vec4 clipPlane = Vec3_ToVec4(reflection->absPlane.normal, reflection->absPlane.dist + 1.f);

  R_AddSubview(view, reflection, view->origin, R_ReflectionMatrix(&reflection->absPlane), clipPlane, true);

  view->stats.reflectionsOffered++;
}

/**
 * @brief Offers the reflections of every BSP inline model drawn in @p view.
 * @details Done here rather than by the client game because everything it needs is already in the
 * scene: the model matrix of each inline model entity is the one the client game would otherwise
 * have to find by scanning the frame's entity states for it.
 */
static void R_AddReflections(RenderView *view) {

  if (!rModels.world || !rModels.world->bsp->numReflections) {
    return;
  }

  const RenderEntity *e = view->entities;
  for (int32_t i = 0; i < view->numEntities; i++, e++) {

    if (!IS_BSP_INLINE_MODEL(e->model)) {
      continue;
    }

    if (e->effects & EF_NO_DRAW) {
      continue;
    }

    RenderSubview *r = rModels.world->bsp->reflections;
    for (int32_t j = 0; j < rModels.world->bsp->numReflections; j++, r++) {

      if (r->model == e->model) {
        R_AddReflection(view, r, e->matrix);
      }
    }
  }
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
void R_AddPortal(RenderView *view, RenderSubview *portal, const Mat4 matrix) {

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

  R_UpdatePortal(portal, matrix);

  // project the camera onto the portal's plane, clamped to the portal's bounds. The carry is a
  // rigid motion, so it preserves handedness and a portal's view is never mirrored
  Vec3 origin = Box3_ClampPoint(portal->absBounds, view->origin);

  origin = Vec3_Subtract(origin, Vec3_Scale(portal->absPlane.normal,
                                            Cm_DistanceToPlane(origin, &portal->absPlane)));

  R_AddSubview(view, portal, origin, portal->matrix, Vec4_Zero(), false);

  view->stats.portalsOffered++;
}

/**
 * @brief Creates or resizes the subview framebuffer for the loaded world.
 * @remarks The color attachment is layered, one layer per subview, so that the BSP fragment
 * stage samples every subview from a single binding.
 * @details Subviews render at half the window's resolution. A subview is sampled through a
 * warping material with further stages over it, so a full size layer per subview buys nothing
 * that can be seen, and costs a whole scene's fill rate each. The projection is unaffected: it
 * comes from the view's own viewport rather than from this size, so the image still registers
 * with the face, and only the rasterization is coarser.
 */
static void R_UpdateSubviewFramebuffer(void) {

  const SDL_Size window = MakeSize(rContext.windowBounds.w, rContext.windowBounds.h);

  if (module.framebuffer) {
    if (module.size.w == window.w && module.size.h == window.h) {
      return;
    }

    R_DestroyFramebuffer(module.framebuffer);
  }

  module.size = window;

  module.framebuffer = R_CreateFramebuffer(&(GPU_FramebufferCreateInfo) {
    .size = MakeSize(window.w / SUBVIEW_FRAMEBUFFER_DIVISOR, window.h / SUBVIEW_FRAMEBUFFER_DIVISOR),
    .colorAttachments = {
      {
        .format = SDL_GPU_TEXTUREFORMAT_R11G11B10_UFLOAT,
        .clearColor = { 0.f, 0.f, 0.f, 1.f },
        .layerCount = MAX_SUBVIEWS,
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
 * @brief Projects @p subview's face onto the screen of the view being drawn around it.
 * @param vp The view-projection of that view, whose screen coordinates the face will be sampled
 * at. Taken as an argument rather than read from the uniform block, which each subview drawn
 * before this one has already replaced with its own.
 * @param mins Receives the lower left corner of the face's rect, in NDC.
 * @param maxs Receives the upper right corner of the face's rect, in NDC.
 * @return `false` for a face straddling the camera plane, which has no finite rect to project onto.
 */
static bool R_SubviewScreen(const Mat4 vp, const RenderSubview *subview, Vec2 *mins, Vec2 *maxs) {

  Vec3 points[8];
  Box3_ToPoints(subview->absBounds, points);

  *mins = MakeVec2(FLT_MAX, FLT_MAX);
  *maxs = MakeVec2(-FLT_MAX, -FLT_MAX);

  for (int32_t i = 0; i < 8; i++) {

    const Vec3 p = points[i];

    const float w = p.x * vp.m[0][3] + p.y * vp.m[1][3] + p.z * vp.m[2][3] + vp.m[3][3];
    if (w <= FLT_EPSILON) {
      return false;
    }

    const Vec3 clip = Mat4_Transform(vp, p);

    *mins = Vec2_Minf(*mins, MakeVec2(clip.x / w, clip.y / w));
    *maxs = Vec2_Maxf(*maxs, MakeVec2(clip.x / w, clip.y / w));
  }

  return true;
}

/**
 * @brief Narrows @p view's frustum to the rect of the face it is drawn for, so that nothing the
 * face cannot show is culled in, drawn, or uploaded.
 * @details A subview shares the outer view's projection, and a face samples it at the outer view's
 * screen coordinates, so the rect the face covers there is the rect of the subview's own screen
 * that is ever read. Geometrically that holds for a mirror as much as for a portal: a point on the
 * mirror's plane is its own reflection, so the reflected camera sees it at the coordinates the
 * outer camera does. The planes are built from the view's basis vectors, as `R_UpdateFrustum`'s
 * are, rather than from anything a rasterized layer's own flip would bear on.
 * @param mins The lower left corner of the face's rect on the outer view's screen, in NDC.
 * @param maxs The upper right corner of the face's rect on the outer view's screen, in NDC.
 */
static void R_UpdateSubviewFrustum(RenderView *view, Vec2 mins, Vec2 maxs) {

  R_UpdateFrustum(view);

  if (!r_cull->value) {
    return;
  }

  mins = Vec2_Maxf(mins, MakeVec2(-1.f, -1.f));
  maxs = Vec2_Minf(maxs, MakeVec2(1.f, 1.f));

  const float tx = tanf(Radians(view->fov.x));
  const float ty = tanf(Radians(view->fov.y));

  // a point at direction `d` is on the inside of `n` when `dot(n, d) >= 0`. The planes are the
  // same four of `R_UpdateFrustum`, in the same order, drawn in to the face
  CmBspPlane *p = view->frustum;

  p[0].normal = Vec3_Fmaf(Vec3_Scale(view->right, -1.f), maxs.x * tx, view->forward);
  p[1].normal = Vec3_Fmaf(view->right, -mins.x * tx, view->forward);
  p[2].normal = Vec3_Fmaf(Vec3_Scale(view->up, -1.f), maxs.y * ty, view->forward);
  p[3].normal = Vec3_Fmaf(view->up, -mins.y * ty, view->forward);

  for (size_t i = 0; i < lengthof(view->frustum); i++) {
    p[i].normal = Vec3_Normalize(p[i].normal);
    p[i].dist = Vec3_Dot(view->origin, p[i].normal);
    p[i].type = Cm_PlaneTypeForNormal(p[i].normal);
    p[i].signBits = Cm_SignBitsForNormal(p[i].normal);
  }
}

/**
 * @return The rect of @p subview's face on screen, in the subview framebuffer's pixels.
 * @details A face samples its subview at its own screen coordinates, so the only texels ever
 * read are the ones beneath the face. Scissoring the subview's pass to them discards nothing
 * that could be sampled, and the further off a subview is the less of its layer it needs -- the
 * saving scales with distance without any of the resolution stepping, and its pop, that
 * choosing a size per subview would bring.
 * @remarks The whole framebuffer is returned for a face straddling the camera plane, which has
 * no finite rect to project onto.
 * @remarks A mirrored subview stores its layer flipped in x, so the texels beneath the face are
 * the mirror of the rect the face projects to.
 * @param mins The lower left corner of the face on screen, from `R_SubviewScreen`, in NDC.
 * @param maxs The upper right corner of the face on screen, from `R_SubviewScreen`, in NDC.
 */
static SDL_Rect R_SubviewScissor(const RenderSubview *subview, const bool projected, const Vec2 mins, const Vec2 maxs) {

  const SDL_Size size = module.framebuffer->size;
  const SDL_Rect framebuffer = { 0, 0, size.w, size.h };

  if (!projected) {
    return framebuffer;
  }

  // NDC to pixels, rounded outward, so that a face is never scissored short of its own edge
  const int32_t x0 = (int32_t) floorf((mins.x * .5f + .5f) * size.w);
  const int32_t x1 = (int32_t) ceilf((maxs.x * .5f + .5f) * size.w);
  const int32_t y0 = (int32_t) floorf((.5f - maxs.y * .5f) * size.h);
  const int32_t y1 = (int32_t) ceilf((.5f - mins.y * .5f) * size.h);

  const int32_t x = Maxi(x0, 0);
  const int32_t y = Maxi(y0, 0);

  const int32_t w = Maxi(Mini(x1, size.w) - x, 0);
  const int32_t h = Maxi(Mini(y1, size.h) - y, 0);

  return (SDL_Rect) {
    .x = subview->view->mirrored ? size.w - (x + w) : x,
    .y = y,
    .w = w,
    .h = h,
  };
}

/**
 * @brief Draws one subview into its own layer of the subview framebuffer.
 */
static void R_DrawSubview(const RenderSubview *subview, const SDL_Rect *scissor, const bool projected,
                          const Vec2 mins, const Vec2 maxs) {

  CommandBuffer *commands = rContext.device->commands;

  RenderView *view = subview->view;

  view->framebuffer = module.framebuffer;

  if (projected) {
    R_UpdateSubviewFrustum(view, mins, maxs);
  } else {
    R_UpdateFrustum(view);
  }

  R_UpdateUniforms(view);

  {
    CopyPass *pass = $(commands, beginCopyPass);

    R_UpdateLights(view, pass);

    R_UpdateEntities(view, pass);

    R_UpdateSprites(view, pass);

    pass = release(pass);
  }

  const SDL_GPUColorTargetInfo color[] = {
    $(module.framebuffer, colorTargetInfoForLayer, 0, (Uint32) subview->layer, SDL_GPU_LOADOP_CLEAR, SDL_GPU_STOREOP_STORE),
    $(module.framebuffer, colorTargetInfo, 1, SDL_GPU_LOADOP_CLEAR, SDL_GPU_STOREOP_STORE),
  };

  const SDL_GPUDepthStencilTargetInfo depth =
    $(module.framebuffer, depthTargetInfo, SDL_GPU_LOADOP_CLEAR, SDL_GPU_STOREOP_STORE);

  RenderPass *pass = $(commands, beginRenderPass, color, 2, &depth);

  // the viewport first: RenderPass::setScissor clamps against it for want of the target's own
  // dimensions, so a scissor set before one is set collapses to nothing. The draws below set the
  // same viewport again for themselves
  $(pass, setViewport, &(SDL_GPUViewport) {
    .x = 0.f, .y = 0.f,
    .w = (float) module.framebuffer->size.w, .h = (float) module.framebuffer->size.h,
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
 * @brief Repeats @p view's scene into @p out, the view of one of its subviews, which culls it
 * for itself.
 * @details Done once the scene is complete rather than as each addition is made, so that nothing
 * depends on subviews having been offered before the rest of the scene was populated, and only
 * for a subview that survived culling, so that one drawn nowhere is copied nowhere. This is the
 * same shape as `R_UpdateLights`, which likewise resolves per-light state only once every entity
 * that could cast a shadow is known.
 *
 * The player's own model is drawn here even in first person. It is hidden only because the camera
 * it was added for sits inside it, and a subview's camera does not.
 *
 * Decals are deliberately not repeated. `R_UpdateDecals` clips them into the shared, persistent
 * geometry of the blocks they land on, rather than into anything the view owns, so repeating
 * them would clip each decal once per subview and draw it that many times over.
 */
static void R_UpdateSubviewScene(const RenderView *view, RenderView *out) {

  assert(out->numEntities == 0);

  const RenderEntity *e = view->entities;
  for (int32_t j = 0; j < view->numEntities; j++, e++) {

    // the view weapon is placed relative to the camera it was added for, so it would appear
    // adrift in the world of any other view
    if (e->effects & EF_WEAPON) {
      continue;
    }

    RenderEntity *copy = &out->entities[out->numEntities++];
    *copy = *e;

    if (copy->effects & EF_SELF) {
      copy->effects &= ~EF_NO_DRAW;
    }
  }

  memcpy(out->lights, view->lights, view->numLights * sizeof(out->lights[0]));
  out->numLights = view->numLights;

  memcpy(out->sprites, view->sprites, view->numSprites * sizeof(out->sprites[0]));
  out->numSprites = view->numSprites;

  memcpy(out->beams, view->beams, view->numBeams * sizeof(out->beams[0]));
  out->numBeams = view->numBeams;
}

/**
 * @brief Draws every subview added this frame, for @p view to sample.
 * @details Subviews draw no shadows of their own: they copy the light list of the view they are
 * drawn for, so the atlas it rendered lines up. They draw subview faces on their plain material
 * rather than sampled, which is what keeps this from recursing; see `R_PushBspSubviewLayer`.
 *
 * Their particles are not softened. Softening blends against a double buffered copy of the
 * view's own depth, and one copy cannot serve several subviews in a frame -- nor can each have
 * its own, since a render pass has four color targets -- so `sprite_fs` draws them hard rather
 * than against whichever subview was drawn last.
 * @param view The view being drawn around these, whose uniforms are restored before
 * returning, since `R_DrawMainView` relies on the ones `R_DrawViewDepth` wrote for it.
 */
void R_DrawSubviews(RenderView *view) {

  // cleared before anything is offered, so that a subview that was not offered, or was offered
  // and culled, leaves its face on its own material rather than sampling a stale layer
  if (rModels.world) {

    RenderSubview *p = rModels.world->bsp->portals;
    for (int32_t i = 0; i < rModels.world->bsp->numPortals; i++, p++) {
      p->layer = -1;
    }

    RenderSubview *r = rModels.world->bsp->reflections;
    for (int32_t i = 0; i < rModels.world->bsp->numReflections; i++, r++) {
      r->layer = -1;
    }
  }

  // the client game offers the portals during scene population, since only it can resolve the
  // entity drawing each portal's face. Reflections need no such help, and the scene is complete
  // by now, so they are offered here and sort against the portals already held
  R_AddReflections(view);

  if (!view->numSubviews || !rContext.device->commands) {
    return;
  }

  R_UpdateSubviewFramebuffer();

  // captured before any subview is drawn, since drawing one replaces the uniform block with
  // its own view
  const Mat4 vp = Mat4_Concat(rUniforms.block.projection3D, rUniforms.block.view);

  RenderViewStats *stats = rStats;

  int32_t layer = 0;
  for (int32_t i = 0; i < view->numSubviews; i++) {

    RenderSubview *subview = view->subviews[i];

    // the scene was populated before any of it was culled, so a subview may well have been
    // offered a view it turns out not to need
    if (R_CulludeBox(view, subview->absBounds)) {
      continue;
    }

    Vec2 mins, maxs;
    const bool projected = R_SubviewScreen(vp, subview, &mins, &maxs);

    const SDL_Rect scissor = R_SubviewScissor(subview, projected, mins, maxs);
    if (scissor.w == 0 || scissor.h == 0) {
      continue;
    }

    // assigned only once this subview is certain to be drawn, so that a non-negative layer always
    // names one rendered this frame
    subview->layer = layer++;

    stats->subviewsDrawn++;

    if (subview->type == SUBVIEW_PORTAL) {
      stats->portalsDrawn++;
    } else if (subview->type == SUBVIEW_REFLECTION) {
      stats->reflectionsDrawn++;
    }

    R_UpdateSubviewScene(view, subview->view);

    rStats = &subview->view->stats;
    R_DrawSubview(subview, &scissor, projected, mins, maxs);

    stats->subviewsTriangles += subview->view->stats.bspTriangles + subview->view->stats.meshTriangles;
  }

  $(module.framebuffer, swap);

  rStats = stats;

  R_UpdateUniforms(view);
}
