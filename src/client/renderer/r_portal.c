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

static struct {
  /**
   * @brief The views the portals of a frame are drawn with, one per layer of the framebuffer.
   * @details A map may hold far more portals than can be drawn, and each of these carries the
   * whole scene, so they are pooled and handed out to the portals a view offers rather than
   * allocated per portal of the world.
   */
  r_view_t views[MAX_PORTALS];

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
  Texture *null_texture;
} r_portal;

/**
 * @brief Allocates the placeholder portal texture.
 */
void R_InitPortal(void) {
  r_portal.null_texture = $(r_context.device, createSolidColorTexture, SDL_GPU_TEXTURETYPE_2D_ARRAY, 1, 0xff000000);
}

/**
 * @brief Releases the portal framebuffer and placeholder texture.
 */
void R_ShutdownPortal(void) {

  if (r_portal.framebuffer) {
    R_DestroyFramebuffer(r_portal.framebuffer);
    r_portal.framebuffer = NULL;
    r_portal.size = MakeSize(0, 0);
  }

  r_portal.null_texture = release(r_portal.null_texture);
}

/**
 * @return The array texture to bind to @p view's BSP portal sampler, never `NULL`.
 * @remarks A portal view is drawn into that very texture, and binding a color attachment as a
 * sampler in the pass writing it is undefined, whether or not any fragment reads it. Portal
 * views sample nothing, so they are given the placeholder.
 */
SDL_GPUTexture *R_PortalTexture(const r_view_t *view) {

  if (r_portal.framebuffer && view->type != VIEW_PORTAL) {
    return $(r_portal.framebuffer, resolveColorTexture, 0)->texture;
  }

  return r_portal.null_texture->texture;
}

/**
 * @brief Offers a portal for @p view to sample.
 * @details The returned view is the client game's to place: its camera, and anything added to
 * @p view afterwards, which the renderer repeats into it. A portal is offered a view before it
 * is known to be visible, since the scene is populated before anything is culled, so the client
 * game should offer them nearest first.
 * @return The view to populate, or `NULL` if this portal will not be drawn.
 */
r_view_t *R_AddPortal(r_view_t *view, r_bsp_portal_t *portal) {

  assert(view);
  assert(portal);

  if (!r_portals->integer) {
    return NULL;
  }

  if (view->num_portals == MAX_PORTALS) {
    return NULL;
  }

  portal->view = &r_portal.views[view->num_portals];

  view->portals[view->num_portals++] = portal;

  return portal->view;
}

/**
 * @brief Creates or resizes the portal framebuffer for the loaded world.
 * @remarks The color attachment is layered, one layer per portal, so that the BSP fragment
 * stage samples every portal from a single binding.
 */
static void R_UpdatePortalFramebuffer(void) {

  const SDL_Size size = MakeSize(r_context.window_bounds.w, r_context.window_bounds.h);

  if (r_portal.framebuffer) {
    if (r_portal.size.w == size.w && r_portal.size.h == size.h) {
      return;
    }

    // recreated rather than resized, since R_CreateFramebuffer is what applies the renderer's
    // scale and sample count, and Framebuffer::resize takes the size it is given
    R_DestroyFramebuffer(r_portal.framebuffer);
  }

  r_portal.size = size;

  r_portal.framebuffer = R_CreateFramebuffer(&(GPU_FramebufferCreateInfo) {
    .size = size,
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
 * @brief Draws one portal's view into its own layer of the portal framebuffer.
 */
static void R_DrawPortal(const r_bsp_portal_t *portal) {

  CommandBuffer *commands = r_context.device->commands;

  r_view_t *view = portal->view;

  view->framebuffer = r_portal.framebuffer;

  R_UpdateFrustum(view);

  R_UpdateUniforms(view);

  {
    CopyPass *pass = $(commands, beginCopyPass);

    R_UpdateLights(view, pass);

    R_UpdateEntities(view, pass);

    R_UpdateSprites(view, pass);

    R_UpdateDecals(view, pass);

    pass = release(pass);
  }

  const SDL_GPUColorTargetInfo color[] = {
    $(r_portal.framebuffer, colorTargetInfoForLayer, 0, (Uint32) portal->layer, SDL_GPU_LOADOP_CLEAR, SDL_GPU_STOREOP_STORE),
    $(r_portal.framebuffer, colorTargetInfo, 1, SDL_GPU_LOADOP_CLEAR, SDL_GPU_STOREOP_STORE),
  };

  const SDL_GPUDepthStencilTargetInfo depth =
    $(r_portal.framebuffer, depthTargetInfo, SDL_GPU_LOADOP_CLEAR, SDL_GPU_STOREOP_STORE);

  RenderPass *pass = $(commands, beginRenderPass, color, 2, &depth);

  R_DrawEntities(view, pass);

  R_DrawSprites(view, pass);

  pass = release(pass);
}

/**
 * @brief Draws the views of all portals added this frame, for @p view to sample.
 * @details Portal views draw no shadows of their own: they copy the light list of the view
 * they are drawn for, so the atlas it rendered lines up. They draw no portal faces either,
 * which is what keeps this from recursing -- a portal seen through a portal shows its plain
 * material.
 *
 * Their particles are not softened. Softening blends against a double buffered copy of the
 * view's own depth, and one copy cannot serve several portals in a frame -- nor can each have
 * its own, since a render pass has four color targets -- so `sprite_fs` draws them hard rather
 * than against whichever portal was drawn last.
 * @param view The view being drawn around these, whose uniforms are restored before
 * returning, since `R_DrawMainView` relies on the ones `R_DrawViewDepth` wrote for it.
 */
void R_DrawPortals(const r_view_t *view) {

  if (r_models.world) {
    r_bsp_portal_t *p = r_models.world->bsp->portals;
    for (int32_t i = 0; i < r_models.world->bsp->num_portals; i++, p++) {
      p->layer = -1;
    }
  }

  r_stats->portals_offered = view->num_portals;

  if (!view->num_portals || !r_context.device->commands) {
    return;
  }

  R_UpdatePortalFramebuffer();

  r_view_stats_t *stats = r_stats;

  int32_t layer = 0;
  for (int32_t i = 0; i < view->num_portals; i++) {

    r_bsp_portal_t *portal = view->portals[i];

    // the scene was populated before any of it was culled, so a portal may well have been
    // offered a view it turns out not to need
    if (R_CulludeBox(view, portal->bounds)) {
      continue;
    }

    portal->layer = layer++;

    stats->portals_drawn++;

    r_stats = &portal->view->stats;
    R_DrawPortal(portal);

    stats->portals_triangles += portal->view->stats.bsp_triangles + portal->view->stats.mesh_triangles;
  }

  $(r_portal.framebuffer, swap);

  r_stats = stats;

  R_UpdateUniforms(view);
}
