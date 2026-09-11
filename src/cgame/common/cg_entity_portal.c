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

#include "cg_local.h"
#include "game/common/bg_portal.h"

/**
 * @brief A `trigger_portal`, client side: the pair's geometry, and the view rendered from the
 * paired portal each frame for this portal's own face to show.
 */
typedef struct {

  /**
   * @brief This portal's brush, whose `common/portal` face shows the view.
   */
  const r_bsp_inline_model_t *model;

  /**
   * @brief The paired portal's brush, or NULL for a destination-only portal.
   */
  const r_bsp_inline_model_t *exit;

  /**
   * @brief The trigger volumes of the two portals: what transit is measured against, as
   * opposed to the inline models' visible bounds, which are only their faces.
   */
  box3_t bounds, exit_bounds;

  /**
   * @brief True for a strict_displace portal, which slides a toucher rather than carrying it.
   */
  bool strict;

  /**
   * @brief The baked centroids of the two portal faces.
   */
  vec3_t origin, exit_origin;

  /**
   * @brief The departure basis at this face, and the arrival basis at the paired face.
   */
  vec3_t right, up, forward;
  vec3_t exit_right, exit_up, exit_forward;

  /**
   * @brief The bounding sphere radius of this brush, for culling.
   */
  float radius;

  /**
   * @brief The view through this portal, populated on the scene thread and drawn before the
   * main view. NULL for a destination-only portal.
   */
  r_view_t *view;

  /**
   * @brief True when `view` holds this frame's scene.
   */
  bool populated;

} cg_portal_t;

#define MAX_PORTAL_FRAMEBUFFERS 4096

/**
 * @brief Portal framebuffers, indexed by inline model. These outlive the entities, which are
 * tag-allocated per level, and are destroyed with the level's media.
 */
static struct {
  Framebuffer *framebuffer;
  SDL_Size size;
} cg_portal_framebuffers[MAX_PORTAL_FRAMEBUFFERS];

/**
 * @return The index of `in` within the world's inline models, or -1.
 */
static int32_t Cg_PortalIndex(const r_bsp_inline_model_t *in) {

  const r_bsp_model_t *bsp = cgi.WorldModel()->bsp;

  const ptrdiff_t index = in - bsp->inline_models;
  if (index < 0 || index >= bsp->num_inline_models || index >= MAX_PORTAL_FRAMEBUFFERS) {
    return -1;
  }

  return (int32_t) index;
}

/**
 * @return The baked facing of a portal definition: the direction of travel into its face.
 */
static vec3_t Cg_PortalAngles(const cm_entity_t *def) {
  return cgi.EntityValue(def, "portal_angles")->vec3;
}

/**
 * @return The inline model of a portal definition, or NULL.
 */
static const r_model_t *Cg_PortalModel(const cm_entity_t *def) {

  const cm_entity_t *model = cgi.EntityValue(def, "model");
  if (model->parsed & ENTITY_STRING) {
    const r_model_t *mod = cgi.LoadModel(model->string);
    if (mod && mod->bsp_inline) {
      return mod;
    }
  }

  return NULL;
}

/**
 * @brief Carries a direction through the portal.
 */
static vec3_t Cg_PortalCarry(const cg_portal_t *portal, const vec3_t v) {
  return Bg_PortalCarry(v, portal->right, portal->up, portal->forward,
                        portal->exit_right, portal->exit_up, portal->exit_forward);
}

/**
 * @brief Carries a point through the portal.
 */
static vec3_t Cg_PortalCarryPoint(const cg_portal_t *portal, const vec3_t p) {
  return Vec3_Add(portal->exit_origin, Cg_PortalCarry(portal, Vec3_Subtract(p, portal->origin)));
}

/**
 * @brief Resolves the portal's brush and, if it has a target, the paired portal and the view.
 */
static void Cg_trigger_portal_Init(cg_entity_t *self) {

  cg_portal_t *portal = self->data;

  const r_model_t *model = Cg_PortalModel(self->def);
  if (!model) {
    Cg_Warn("%s has no brush model\n", self->clazz->classname);
    return;
  }

  // the inline model's bounds are the whole trigger volume; its visible bounds, and the
  // model's own, are only the portal face, which is all of it that draws
  portal->model = model->bsp_inline;
  portal->bounds = model->bsp_inline->bounds;
  portal->strict = cgi.EntityValue(self->def, "spawnflags")->integer & 1;

  portal->origin = cgi.EntityValue(self->def, "portal_origin")->vec3;
  Bg_PortalBasis(Cg_PortalAngles(self->def), false, &portal->right, &portal->up, &portal->forward);

  portal->radius = Vec3_Length(Box3_Size(portal->model->visible_bounds)) * 0.5f;

  // visibility mirrors transit: a destination-only portal shows its plain face
  if (!self->target) {
    return;
  }

  const r_model_t *exit = Cg_PortalModel(self->target);
  if (!exit) {
    Cg_Warn("%s targets an entity with no brush model\n", self->clazz->classname);
    return;
  }

  portal->exit = exit->bsp_inline;
  portal->exit_bounds = exit->bsp_inline->bounds;

  portal->exit_origin = cgi.EntityValue(self->target, "portal_origin")->vec3;
  Bg_PortalBasis(Cg_PortalAngles(self->target), true, &portal->exit_right, &portal->exit_up, &portal->exit_forward);

  // the view holds every entity and sprite slot, so it is far too large for the stack
  portal->view = cgi.Malloc(sizeof(r_view_t), MEM_TAG_CGAME_LEVEL);
}

/**
 * @brief Sets up the view through the portal for this frame: the main camera carried through
 * the portal exactly as a toucher would be, so the view lines up with what stepping through
 * will actually show. The scene itself is added later, by `Cg_AddPortalEntities`.
 */
static void Cg_trigger_portal_Think(cg_entity_t *self) {

  cg_portal_t *portal = self->data;

  portal->populated = false;

  if (!portal->view || !cg_portals->value) {
    return;
  }

  if (cgi.CulludeSphere(cgi.view, portal->origin, portal->radius)) {
    return;
  }

  r_view_t *view = portal->view;

  cgi.InitView(view);

  view->type = VIEW_PORTAL;
  view->flags = VIEW_FLAG_NONE;
  view->fov = cgi.view->fov;
  view->depth_range = cgi.view->depth_range;

  view->origin = Cg_PortalCarryPoint(portal, cgi.view->origin);
  view->forward = Cg_PortalCarry(portal, cgi.view->forward);
  view->right = Cg_PortalCarry(portal, cgi.view->right);
  view->up = Cg_PortalCarry(portal, cgi.view->up);
  view->angles = Vec3_Euler(view->forward);

  view->contents = cgi.PointContents(view->origin);
  view->ticks = cgi.view->ticks;
  view->ambient = cgi.view->ambient;

  // the camera sits inside the wall behind the exit face: cut that wall away, and skip the exit
  // face itself, which would otherwise cover the whole view with its plain material
  view->portal_exit = portal->exit;
  view->clip_plane = Vec3_ToVec4(portal->exit_forward, Vec3_Dot(portal->exit_origin, portal->exit_forward) + 1.f);

  portal->populated = true;
}

/**
 * @brief The client-side entity class descriptor for `trigger_portal`.
 */
const cg_entity_class_t cg_trigger_portal = {
  .classname = "trigger_portal",
  .Init = Cg_trigger_portal_Init,
  .Think = Cg_trigger_portal_Think,
  .data_size = sizeof(cg_portal_t)
};

/**
 * @return True if `bounds` overlap any portal's volume, the client's side of G_OccupiesPortal.
 */
bool Cg_OccupiesPortal(const box3_t bounds) {

  if (!cg_entities) {
    return false;
  }

  const cg_entity_t *e = cg_entities->elements;
  for (uint32_t i = 0; i < cg_entities->count; i++, e++) {

    if (e->clazz != &cg_trigger_portal) {
      continue;
    }

    const cg_portal_t *portal = e->data;

    if (Box3_Intersects(Box3_Expand(bounds, PORTAL_PLAYER_CLEARANCE), portal->bounds)) {
      return true;
    }
  }

  return false;
}

/**
 * @brief Carries the predicted player state through any portal its body has reached, exactly as
 * the server will, so that prediction never runs on through the face and shows what lies behind
 * it while the server's transit is still in flight.
 */
void Cg_PredictPortalTransit(pm_move_t *pm) {

  if (!cg_entities) {
    return;
  }

  // the server touches nothing for a spectator (noclip included), the dead, or the frozen
  switch (pm->s.type) {
    case PM_SPECTATOR:
    case PM_DEAD:
    case PM_FREEZE:
      return;
    default:
      break;
  }

  const cg_entity_t *e = cg_entities->elements;
  for (uint32_t i = 0; i < cg_entities->count; i++, e++) {

    if (e->clazz != &cg_trigger_portal) {
      continue;
    }

    const cg_portal_t *portal = e->data;
    if (!portal->exit) {
      continue;
    }

    // the portal transits us once the leading edge of our body reaches its face, unless we are
    // still moving out of it (a floor lets us settle onto it too), whenever we are touching its
    // volume - exactly as the server does (see G_trigger_portal_Touch)
    const float after = Vec3_Dot(Vec3_Subtract(pm->s.origin, portal->origin), portal->forward);
    const float leading = after + Bg_PortalExtent(pm->bounds, portal->forward);
    const float into = Vec3_Dot(pm->s.velocity, portal->forward);

    if (leading < -PORTAL_TRANSIT_LEAD || (Bg_PortalIsHorizontal(portal->forward) ? into < 0.f : into <= 0.f)) {
      continue;
    }

    if (!Box3_Intersects(Box3_Translate(pm->bounds, pm->s.origin), portal->bounds)) {
      continue;
    }

    // the same carry and the same nudge as G_trigger_portal_Transit: as far into a wall as we
    // were into this one, as deep as its recess holds us, and just inside a floor or ceiling
    const vec3_t offset = Vec3_Subtract(pm->s.origin, portal->origin);

    float distance = PORTAL_TRANSIT_OFFSET - Minf(after, 0.f);

    if (!Bg_PortalIsHorizontal(portal->exit_forward)) {
      const float recess = Bg_PortalRecess(portal->exit_bounds, portal->exit_origin, portal->exit_forward, pm->bounds);
      distance = Maxf(after, -recess) - after;
    }

    const vec3_t nudge = Vec3_Scale(portal->exit_forward, distance);

    // a strict_displace portal slides us by the offset between the faces, and leaves the
    // rest alone (see G_trigger_portal_Transit)
    if (portal->strict) {
      pm->s.origin = Vec3_Add(Vec3_Add(pm->s.origin, Vec3_Subtract(portal->exit_origin, portal->origin)), nudge);
      return;
    }

    pm->s.origin = Vec3_Add(Vec3_Add(portal->exit_origin, Cg_PortalCarry(portal, offset)), nudge);
    pm->s.velocity = Cg_PortalCarry(portal, pm->s.velocity);

    // a pair that turns the view is snapped exactly as the server's own SV_CMD_SNAP_ANGLES
    // will; a pair that does not is left alone, so input is never needlessly reset
    vec3_t view_forward;
    Vec3_Vectors(pm->s.view_angles, &view_forward, NULL, NULL);

    const vec3_t carried_forward = Cg_PortalCarry(portal, view_forward);

    if (!Vec3_EqualEpsilon(carried_forward, view_forward, 0.001f)) {
      const vec3_t angles = Vec3_Euler(carried_forward);

      pm->s.view_angles = angles;
      pm->s.delta_angles = Vec3_Zero();

      if (!Vec3_EqualEpsilon(cgi.client->angles, angles, 0.01f)) {
        cg_state.snap_view_angles = angles;
        cg_state.snap_angles = true;
      }
    }

    return;
  }
}

/**
 * @brief Populates the view through each visible portal with this frame's entities, lights,
 * sprites and beams.
 * @remarks Runs on the scene thread, after the main view is populated. The per-frame side
 * effects of adding entities (trails, breath, the view weapon) are suppressed throughout.
 */
void Cg_AddPortalEntities(const cl_frame_t *frame) {

  if (!cg_entities || !cg_portals->value) {
    return;
  }

  r_view_t *view = cgi.view;

  cg_state.portal_view = true;

  const cg_entity_t *e = cg_entities->elements;
  for (uint32_t i = 0; i < cg_entities->count; i++, e++) {

    if (e->clazz != &cg_trigger_portal) {
      continue;
    }

    const cg_portal_t *portal = e->data;
    if (!portal->populated) {
      continue;
    }

    cgi.view = portal->view;

    Cg_AddFrameEntities(frame);

    // the same lights in the same order, so the shadow tiles the main view assigns line up
    memcpy(portal->view->lights, view->lights, view->num_lights * sizeof(r_light_t));
    portal->view->num_lights = view->num_lights;

    // particles and beams are world-space, so the main view's serve every view
    memcpy(portal->view->sprites, view->sprites, view->num_sprites * sizeof(r_sprite_t));
    portal->view->num_sprites = view->num_sprites;

    memcpy(portal->view->beams, view->beams, view->num_beams * sizeof(r_beam_t));
    portal->view->num_beams = view->num_beams;
  }

  cgi.view = view;

  cg_state.portal_view = false;
}

/**
 * @return The framebuffer for the portal at `index`, created or resized as needed.
 */
static Framebuffer *Cg_PortalFramebuffer(int32_t index) {

  cg_portal_scale->value = Clampf(cg_portal_scale->value, 0.1f, 1.f);

  const SDL_Rect rect = cgi.context->window_bounds;
  const SDL_Size size = MakeSize((int32_t) (rect.w * cg_portal_scale->value) ?: 1,
                                 (int32_t) (rect.h * cg_portal_scale->value) ?: 1);

  Framebuffer **framebuffer = &cg_portal_framebuffers[index].framebuffer;
  SDL_Size *current = &cg_portal_framebuffers[index].size;

  if (*framebuffer && (current->w != size.w || current->h != size.h)) {
    cgi.DestroyFramebuffer(*framebuffer);
    *framebuffer = NULL;
  }

  if (*framebuffer == NULL) {
    // the same layout as the main framebuffer, see Cg_CreateFramebuffer
    *framebuffer = cgi.CreateFramebuffer(&(GPU_FramebufferCreateInfo) {
      .size = size,
      .colorAttachments = {
        { .format = SDL_GPU_TEXTUREFORMAT_R11G11B10_UFLOAT, .clearColor = { 0.f, 0.f, 0.f, 1.f } },
        { .format = SDL_GPU_TEXTUREFORMAT_R32_FLOAT, .clearColor = { 1.f, 1.f, 1.f, 1.f }, .doubleBuffered = true },
      },
      .numColorTargets = 2,
      .depthAttachment = { .format = SDL_GPU_TEXTUREFORMAT_D32_FLOAT, .clearDepth = 1.f },
    });
    *current = size;
  }

  return *framebuffer;
}

/**
 * @brief Draws the view through each populated portal, and hands the result to the renderer
 * for the portal's face to show when the main view is drawn.
 */
void Cg_DrawPortals(const cl_frame_t *frame) {

  if (!cg_entities) {
    return;
  }

  cg_entity_t *e = cg_entities->elements;
  for (uint32_t i = 0; i < cg_entities->count; i++, e++) {

    if (e->clazz != &cg_trigger_portal) {
      continue;
    }

    cg_portal_t *portal = e->data;
    if (!portal->model) {
      continue;
    }

    if (!portal->populated) {
      cgi.SetPortalTexture(portal->model, NULL);
      continue;
    }

    portal->populated = false;

    // the frustum was still last frame's when the scene was populated; check again now
    if (cgi.CulludeSphere(cgi.view, portal->origin, portal->radius)) {
      cgi.SetPortalTexture(portal->model, NULL);
      continue;
    }

    const int32_t index = Cg_PortalIndex(portal->model);

    Framebuffer *framebuffer = index < 0 ? NULL : Cg_PortalFramebuffer(index);
    if (!framebuffer) {
      cgi.SetPortalTexture(portal->model, NULL);
      continue;
    }

    r_view_t *view = portal->view;

    view->framebuffer = framebuffer;
    view->viewport = Vec4i(0, 0, framebuffer->size.w, framebuffer->size.h);

    cgi.DrawPortalView(view, cgi.view);

    cgi.SetPortalTexture(portal->model, $(framebuffer, resolveColorTexture, 0));
  }
}

/**
 * @brief Destroys the portal framebuffers, e.g. when the level's media is freed.
 */
void Cg_FreePortals(void) {

  for (int32_t i = 0; i < MAX_PORTAL_FRAMEBUFFERS; i++) {
    if (cg_portal_framebuffers[i].framebuffer) {
      cgi.DestroyFramebuffer(cg_portal_framebuffers[i].framebuffer);
    }
  }

  memset(cg_portal_framebuffers, 0, sizeof(cg_portal_framebuffers));
}
