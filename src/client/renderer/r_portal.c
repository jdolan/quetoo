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

#define MAX_PORTAL_TEXTURES 4096

/**
 * @brief The texture each trigger_portal's face samples, indexed by inline model, as handed over
 * by the client game after it renders the view through. NULL leaves a face on its plain material.
 */
static struct {
  const r_bsp_inline_model_t *model;
  SDL_GPUTexture *texture;
} r_portals[MAX_PORTAL_TEXTURES];

/**
 * @return The index of `in` within the world's inline models, or -1.
 */
static int32_t R_PortalIndex(const r_bsp_inline_model_t *in) {

  if (!r_models.world || !in) {
    return -1;
  }

  const ptrdiff_t index = in - r_models.world->bsp->inline_models;
  if (index < 0 || index >= r_models.world->bsp->num_inline_models || index >= MAX_PORTAL_TEXTURES) {
    return -1;
  }

  return (int32_t) index;
}

/**
 * @brief Sets the texture the portal face of `in` samples, or clears it with NULL.
 */
void R_SetPortalTexture(const r_bsp_inline_model_t *in, Texture *texture) {

  const int32_t index = R_PortalIndex(in);
  if (index < 0) {
    return;
  }

  r_portals[index].model = in;
  r_portals[index].texture = texture ? texture->texture : NULL;
}

/**
 * @return The texture the portal face of `in` samples this frame, or NULL for none.
 */
SDL_GPUTexture *R_PortalTexture(const r_bsp_inline_model_t *in) {

  const int32_t index = R_PortalIndex(in);
  if (index < 0 || r_portals[index].model != in) {
    return NULL;
  }

  return r_portals[index].texture;
}

/**
 * @brief Forgets all portal textures, e.g. when a new world is loaded.
 */
void R_ClearPortalTextures(void) {
  memset(r_portals, 0, sizeof(r_portals));
}

/**
 * @brief Draws a portal view: the scene as seen from the paired portal, into the view's own
 * framebuffer. The caller populates the view and later hands its resolved color texture back
 * via `R_SetPortalTexture`.
 * @details Portal views draw no shadows of their own (they sample the atlas the main view last
 * rendered - the light lists are copied from it, so the tiles line up), and never draw portal
 * views themselves: a portal face seen through a portal shows its plain material.
 * @param outer The view being drawn around this one. `R_DrawMainView` relies on the uniforms
 * `R_DrawViewDepth` wrote, so they are restored for it here before returning.
 */
void R_DrawPortalView(r_view_t *view, const r_view_t *outer) {

  assert(view);
  assert(outer);

  CommandBuffer *commands = r_context.device->commands;
  if (!commands) {
    return;
  }

  r_view_stats_t *stats = r_stats;
  r_stats = &view->stats;

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

  Framebuffer *framebuffer = view->framebuffer;

  const SDL_GPUColorTargetInfo color[] = {
    $(framebuffer, colorTargetInfo, 0, SDL_GPU_LOADOP_CLEAR, SDL_GPU_STOREOP_STORE),
    $(framebuffer, colorTargetInfo, 1, SDL_GPU_LOADOP_CLEAR, SDL_GPU_STOREOP_STORE),
  };
  const SDL_GPUDepthStencilTargetInfo depth =
    $(framebuffer, depthTargetInfo, SDL_GPU_LOADOP_CLEAR, SDL_GPU_STOREOP_STORE);

  {
    RenderPass *pass = $(commands, beginRenderPass, color, 2, &depth);

    R_DrawEntities(view, pass);

    R_DrawSprites(view, pass);

    pass = release(pass);
  }

  $(framebuffer, swap);

  r_stats = stats;

  R_UpdateUniforms(outer);
}
