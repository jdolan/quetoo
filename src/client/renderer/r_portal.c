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
   * @brief The framebuffer all portals render into, its color attachment holding one layer
   * per portal of the loaded world. Its depth and depth copy are scratch, reused by each
   * portal in turn, since portals are drawn one after another and nothing reads them after.
   */
  Framebuffer *framebuffer;

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
 * @brief Adds a portal for @p view to sample. The client game populates the portal's own view
 * first, placing its camera and adding whatever it should see.
 */
void R_AddPortal(r_view_t *view, r_bsp_portal_t *portal) {

  assert(view);
  assert(portal);

  if (!r_portals->integer) {
    return;
  }

  if (view->num_portals == MAX_PORTALS) {
    return;
  }

  view->portals[view->num_portals++] = portal;

  R_UpdateFrustum(portal->view);
}

/**
 * @brief Creates or resizes the portal framebuffer for the loaded world.
 * @remarks The color attachment is layered, one layer per portal, so that the BSP fragment
 * stage samples every portal from a single binding.
 */
static void R_UpdatePortalFramebuffer(void) {

  const int32_t num_portals = r_models.world->bsp->num_portals;

  const SDL_Size size = MakeSize(r_context.window_bounds.w, r_context.window_bounds.h);

  if (r_portal.framebuffer) {
    if (r_portal.framebuffer->colorAttachments[0].layerCount == (Uint32) num_portals) {
      $(r_portal.framebuffer, resize, &size);
      return;
    }
    R_DestroyFramebuffer(r_portal.framebuffer);
  }

  r_portal.framebuffer = R_CreateFramebuffer(&(GPU_FramebufferCreateInfo) {
    .size = size,
    .colorAttachments = {
      {
        .format = SDL_GPU_TEXTUREFORMAT_R11G11B10_UFLOAT,
        .clearColor = { 0.f, 0.f, 0.f, 1.f },
        .layerCount = num_portals,
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
 * @return The layer of the portal texture @p portal renders into, which is its index within
 * the world's portals.
 */
static Uint32 R_PortalLayer(const r_bsp_portal_t *portal) {
  return (Uint32) (portal - r_models.world->bsp->portals);
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
    $(r_portal.framebuffer, colorTargetInfoForLayer, 0, R_PortalLayer(portal), SDL_GPU_LOADOP_CLEAR, SDL_GPU_STOREOP_STORE),
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
 * @param view The view being drawn around these, whose uniforms are restored before
 * returning, since `R_DrawMainView` relies on the ones `R_DrawViewDepth` wrote for it.
 */
void R_DrawPortals(const r_view_t *view) {

  if (!view->num_portals || !r_context.device->commands) {
    return;
  }

  R_UpdatePortalFramebuffer();

  r_view_stats_t *stats = r_stats;

  for (int32_t i = 0; i < view->num_portals; i++) {
    r_stats = &view->portals[i]->view->stats;
    R_DrawPortal(view->portals[i]);
  }

  $(r_portal.framebuffer, swap);

  r_stats = stats;

  R_UpdateUniforms(view);
}
