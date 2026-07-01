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

/**
 * @brief A binding from a `SURF_TOGGLE` surface (material stage or flare) to the
 * `target_light` or styled `light` that controls its brightness.
 */
typedef struct {

  /**
   * @brief True if a controlling light was resolved. Unbound surfaces render normally.
   */
  _Bool found;

  /**
   * @brief True if the controller is a `target_light` (switched on/off at runtime via
   * `EF_LIGHT`); false for a static `light` (always on, brightness from its style).
   */
  _Bool switchable;

  /**
   * @brief The controlling light's origin, used to resolve its live entity for the
   * `EF_LIGHT` switch state (switchable lights only).
   */
  vec3_t origin;

  /**
   * @brief The controlling light's `a`-`z` style string (may be empty) and phase offset,
   * driving the animated brightness via `Cg_AnimateLight`.
   */
  char style[MAX_BSP_ENTITY_VALUE];
  float drift;
} cg_light_binding_t;

/**
 * @brief The flare type.
 */
typedef struct {

  /**
   * @brief The face this flare is anchored to.
   */
  const r_bsp_face_t *face;

  /**
   * @brief The material stage defining this flare.
   */
  const r_stage_t *stage;

  /**
   * @brief The bounds of all faces represented by this flare.
   */
  box3_t bounds;

  /**
   * @brief The sprite input and output instances.
   */
  r_sprite_t in, out;

  /**
   * @brief The entity referencing the model containin this flare, if any.
   */
  const cl_entity_t *entity;

  /**
   * @brief Binding to a controlling light for `SURF_TOGGLE` flares. The flare's alpha is
   * scaled by the light's brightness (on/off and/or style). Unbound if `light.found` is false.
   */
  cg_light_binding_t light;
} cg_flare_t;

static Vector *cg_flares;

#define FLARE_ALPHA_RAMP 0.01

/**
 * @brief Returns true if a live `target_light` at the given origin is currently lit.
 */
static _Bool Cg_ToggleLightLit(const vec3_t origin) {

  const cl_entity_t *e = cgi.client->entities;
  for (int32_t i = 0; i < MAX_ENTITIES; i++, e++) {

    // a target_light that has switched off is culled from the snapshot, but its
    // last-seen state lingers in the persistent entities array. Only trust an
    // entity that was actually transmitted in the current frame, matching how the
    // renderer adds the dynamic light (Cg_AddEntities iterates the frame only).
    if (e->frame_num != cgi.client->frame.frame_num) {
      continue;
    }

    if (!(e->current.effects & EF_LIGHT)) {
      continue;
    }

    if (Vec3_Distance(e->current.origin, origin) < 1.f) {
      return true;
    }
  }

  return false;
}

/**
 * @brief Binds a `SURF_TOGGLE` surface to the nearest light controlling the given material.
 * @details Both switched `target_light` entities (matched in the entity list, animated by their
 * live `EF_LIGHT` state) and static styled `light` entities (matched in the baked BSP lights,
 * which carry the team-resolved, phase-correct style) are candidates; the nearest match wins.
 * @param material The material name to match against light `material` keys.
 * @param pos The surface position to measure proximity from.
 * @param b Filled with the resolved binding; `b->found` is false if no controlling light exists.
 */
static void Cg_BindToggleLight(const char *material, const vec3_t pos, cg_light_binding_t *b) {

  memset(b, 0, sizeof(*b));

  const r_bsp_model_t *bsp = cgi.WorldModel()->bsp;
  const cm_bsp_t *cm = bsp->cm;

  float best = FLT_MAX;

  // switched lights: target_light entities, animated at runtime by EF_LIGHT
  for (int32_t i = 0; i < cm->num_entities; i++) {
    const cm_entity_t *e = cm->entities[i];

    const char *classname = cgi.EntityValue(e, "classname")->nullable_string;
    if (!classname || q_strcmp(classname, "target_light")) {
      continue;
    }

    const char *m = cgi.EntityValue(e, "material")->nullable_string;
    if (!m || q_strcmp(m, material)) {
      continue;
    }

    const vec3_t o = cgi.EntityValue(e, "origin")->vec3;
    const float d = Vec3_Distance(o, pos);
    if (d < best) {
      best = d;
      b->found = true;
      b->switchable = true;
      b->origin = o;
      const char *style = cgi.EntityValue(e, "style")->nullable_string;
      q_strlcpy(b->style, style ?: "", sizeof(b->style));
      b->drift = cgi.EntityValue(e, "drift")->value;
    }
  }

  // static styled lights: baked BSP lights carry the resolved style/drift (target_lights
  // are not baked, so there is no overlap with the loop above)
  const r_bsp_light_t *l = bsp->lights;
  for (int32_t i = 0; i < bsp->num_lights; i++, l++) {

    if (!l->entity) {
      continue;
    }

    const char *m = cgi.EntityValue(l->entity, "material")->nullable_string;
    if (!m || q_strcmp(m, material)) {
      continue;
    }

    const float d = Vec3_Distance(l->origin, pos);
    if (d < best) {
      best = d;
      b->found = true;
      b->switchable = false;
      b->origin = l->origin;
      q_strlcpy(b->style, l->style, sizeof(b->style));
      b->drift = l->drift;
    }
  }
}

/**
 * @brief Returns the current brightness `[0, 1]` of a toggle light binding: a switched
 * `target_light` contributes its on/off state, and any light style modulates on top, so a
 * plain switched light is binary and a styled light flickers. Unbound bindings return 1.
 */
static float Cg_ToggleLightAlpha(const cg_light_binding_t *b) {

  if (!b->found) {
    return 1.f;
  }

  const float base = b->switchable ? (Cg_ToggleLightLit(b->origin) ? 1.f : 0.f) : 1.f;

  return base * Cg_AnimateLight(1.f, b->style, b->drift);
}

/**
 * @brief Adds all loaded flare sprites to the view, attenuated by their surface angle to the camera.
 */
void Cg_AddFlares(void) {

  if (!cg_add_flares->value) {
    return;
  }

  for (size_t i = 0; i < cg_flares->count; i++) {
    cg_flare_t *flare = VectorValue(cg_flares, cg_flare_t *, i);

    // a toggle flare's brightness tracks its controlling light (on/off and/or style);
    // unbound flares return 1 and are unaffected
    const float toggle_alpha = Cg_ToggleLightAlpha(&flare->light);
    if (toggle_alpha <= 0.f) {
      continue;
    }

    mat4_t matrix = Mat4_Identity();
    flare->entity = NULL;

    const r_bsp_inline_model_t *in = flare->face->node->model;

    if (in != cgi.WorldModel()->bsp->inline_models && !editor->value) {

      const cl_entity_t *e = cgi.client->entities;
      for (int32_t j = 0; j < MAX_ENTITIES; j++, e++) {

        if (!e->current.model1) {
          continue;
        }

        if (e->current.model1 == MODEL_CLIENT) {
          continue;
        }

        const r_model_t *mod = cgi.client->models[e->current.model1];

        if (mod && mod->type == MODEL_BSP_INLINE) {
          if (in == mod->bsp_inline) {
            matrix = Mat4_FromRotationTranslationScale(e->angles, e->origin, 1.f);
            flare->entity = e;
            break;
          }
        }
      }

      assert(flare->entity);
    }

    cm_bsp_plane_t plane = *(flare->face->plane->cm);

    if (flare->entity) {
      flare->out.origin = Mat4_Transform(matrix, flare->in.origin);

      const vec4_t out = Mat4_TransformPlane(matrix, plane.normal, plane.dist);

      plane.normal = out.xyz;
      plane.dist = out.w;
    }

    // Dot product gives us facing: positive=front, negative=back
    const float dot = Vec3_Dot(Vec3_Direction(cgi.view->origin, flare->out.origin), plane.normal);
    // Use absolute value to allow sprites from behind, but abs(dot) reduces visibility for grazing angles
    const float alpha = Clampf01(Maxf(fabsf(dot), 0.25f) * cg_add_flares->value) * toggle_alpha;

    if (alpha == 0.f) {
      continue;
    }

    flare->out.color = Vec3_Scale(flare->in.color, alpha);

    cgi.AddSprite(cgi.view, &flare->out);
  }
}

/**
 * @brief Creates a flare from the specified face and stage.
 */
cg_flare_t *Cg_LoadFlare(const r_bsp_face_t *face, const r_stage_t *stage) {

  cg_flare_t *flare = cgi.Malloc(sizeof(*flare), MEM_TAG_CGAME_LEVEL);

  flare->face = face;
  flare->stage = stage;

  flare->bounds = Box3_Null();
  for (int32_t i = 0; i < face->num_vertexes; i++) {
    flare->bounds = Box3_Append(flare->bounds, face->vertexes[i].position);
  }

  flare->bounds = Box3_Expand(flare->bounds, Box3_Distance(flare->bounds) * .1f);

  if (stage->cm->flags & STAGE_COLOR) {
    flare->in.color = stage->cm->color.vec3;
  } else {
    flare->in.color = color_white.vec3;
  }

  flare->in.media = stage->media;
  flare->in.lighting = 1.f;
  // Use frustum-aligned billboard without the two perpendicular axial quads

  return flare;
}

/**
 * @brief Returns true if two faces share a vertex position, indicating geometric adjacency.
 */
static _Bool Cg_FacesShareVertex(const r_bsp_face_t *a, const r_bsp_face_t *b) {

  const float epsilon = 1.f;

  for (int32_t i = 0; i < a->num_vertexes; i++) {
    for (int32_t j = 0; j < b->num_vertexes; j++) {
      if (Vec3_Distance(a->vertexes[i].position, b->vertexes[j].position) < epsilon) {
        return true;
      }
    }
  }

  return false;
}

/**
 * @brief Merges adjacent flares on the same brush side into unified flares by union of bounds.
 */
static void Cg_MergeFlares(void) {

  for (size_t i = 0; i < cg_flares->count; i++) {
    cg_flare_t *a = VectorValue(cg_flares, cg_flare_t *, i);

    for (size_t j = i + 1; j < cg_flares->count; j++) {
      cg_flare_t *b = VectorValue(cg_flares, cg_flare_t *, j);

      if (a->face->brush_side == b->face->brush_side &&
          Cg_FacesShareVertex(a->face, b->face)) {
        a->bounds = Box3_Union(a->bounds, b->bounds);

        $(cg_flares, removeAt, j);
        cgi.Free(b);

        j--;
      }
    }

    a->in.origin = Box3_Center(a->bounds);
    a->in.size = Box3_Distance(a->bounds);

    if (a->stage->cm->flags & (STAGE_SCALE_S | STAGE_SCALE_T)) {
      a->in.size *= (a->stage->cm->scale.s ? a->stage->cm->scale.s : a->stage->cm->scale.t);
    }

    a->out = a->in;
  }
}

/**
 * @brief Loads all flare stages from BSP faces and merges adjacent flares on the same brush side.
 */
void Cg_LoadFlares(void) {

  cg_flares = $(alloc(Vector), initWithSize, sizeof(cg_flare_t *));

  const r_bsp_model_t *bsp = cgi.WorldModel()->bsp;

  const r_bsp_face_t *face = bsp->faces;
  for (int32_t i = 0; i < bsp->num_faces; i++, face++) {

    if (!face->brush_side) {
      continue;
    }

    const r_material_t *material = face->brush_side->material;
    if (material->cm->stage_flags & STAGE_FLARE) {

      const r_stage_t *stage = material->stages;
      while (stage) {
        if (stage->cm->flags & STAGE_FLARE) {
          break;
        }
        stage = stage->next;
      }
      assert(stage);

      if (stage->media == NULL) {
        continue;
      }

      cg_flare_t *flare = Cg_LoadFlare(face, stage);
      $(cg_flares, add, &flare);
    }
  }

  Cg_MergeFlares();

  // bind flares on SURF_TOGGLE faces to their nearest controlling light; the flare then
  // tracks that light's brightness. Scoping is by the face flag alone (not the `toggle`
  // stage keyword), so unrelated faces using the same material keep their always-on flares.
  for (size_t i = 0; i < cg_flares->count; i++) {
    cg_flare_t *flare = VectorValue(cg_flares, cg_flare_t *, i);

    if (flare->face->brush_side->surface & SURF_TOGGLE) {
      const char *material = flare->face->brush_side->material->cm->name;
      Cg_BindToggleLight(material, flare->in.origin, &flare->light);
    }
  }

  Cg_Debug("Loaded %zu flares\n", cg_flares->count);
}

/**
 * @brief Frees the flare array and sets the pointer to `NULL`.
 */
void Cg_FreeFlares(void) {

  if (cg_flares) {
    release(cg_flares);
    cg_flares = NULL;
  }
}

/**
 * @brief A `SURF_TOGGLE` draw element driven by a controlling light.
 */
typedef struct {

  /**
   * @brief The controlled draw element. The renderer reads `draw->toggle_alpha`.
   */
  r_bsp_draw_elements_t *draw;

  /**
   * @brief The binding to the controlling light, resolved at load.
   */
  cg_light_binding_t light;
} cg_material_toggle_t;

static Vector *cg_material_toggles;

/**
 * @brief Binds each `SURF_TOGGLE` draw element to the nearest light whose `material` key
 * matches the element's material. Each element is driven independently, so panels sharing
 * one material can chase or flicker individually. Elements default to fully on (`toggle_alpha`
 * 1); only bound elements are dimmed.
 */
void Cg_LoadMaterialToggles(void) {

  cg_material_toggles = $(alloc(Vector), initWithSize, sizeof(cg_material_toggle_t));

  r_bsp_model_t *bsp = cgi.WorldModel()->bsp;

  for (int32_t i = 0; i < bsp->num_draw_elements; i++) {
    r_bsp_draw_elements_t *draw = &bsp->draw_elements[i];

    draw->toggle_alpha = 1.f;

    if (!(draw->surface & SURF_TOGGLE)) {
      continue;
    }

    cg_material_toggle_t toggle = { .draw = draw };
    Cg_BindToggleLight(draw->material->cm->name, Box3_Center(draw->bounds), &toggle.light);
    if (!toggle.light.found) {
      continue;
    }

    $(cg_material_toggles, add, &toggle);
  }

  Cg_Debug("Loaded %zu material toggles\n", cg_material_toggles->count);
}

/**
 * @brief Frees the material toggle controller list.
 */
void Cg_FreeMaterialToggles(void) {

  if (cg_material_toggles) {
    release(cg_material_toggles);
    cg_material_toggles = NULL;
  }
}

/**
 * @brief Refreshes each controlled draw element's `toggle_alpha` from its controlling
 * light's current brightness (on/off state and/or light style).
 */
void Cg_UpdateMaterialToggles(void) {

  if (!cg_material_toggles) {
    return;
  }

  // nothing reads toggle_alpha while material stages aren't being drawn, so don't
  // bother resolving controlling lights (the flare path is gated by cg_add_flares)
  if (!cgi.GetCvarValue("r_draw_material_stages")) {
    return;
  }

  for (size_t i = 0; i < cg_material_toggles->count; i++) {
    cg_material_toggle_t *toggle = VectorElement(cg_material_toggles, cg_material_toggle_t, i);
    toggle->draw->toggle_alpha = Cg_ToggleLightAlpha(&toggle->light);
  }
}
