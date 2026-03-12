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

#define ENTITY_LIGHT_RADIUS    300.f
#define ENTITY_LIGHT_INTENSITY 1.f
#define ENTITY_LIGHT_COLOR     Vec3(1.f, 1.f, 1.f)

/**
 * @brief Dynamic light source accounting structure.
 */
static struct {
  /**
   * @brief The queue of allocated lights.
   */
  GQueue *allocated;

  /**
   * @brief The queue of free lights.
   */
  GQueue *free;
} cg_lights;

/**
 * @brief Cached data for a light entity attached to a BSP inline model entity.
 */
typedef struct {
  /**
   * @brief The light origin as specified in the map, relative to the entity's initial origin.
   */
  vec3_t origin;

  /**
   * @brief The light color.
   */
  vec3_t color;

  /**
   * @brief The light radius.
   */
  float radius;

  /**
   * @brief The light intensity.
   */
  float intensity;

  /**
   * @brief The light style string (animated at 10Hz).
   */
  char style[MAX_BSP_ENTITY_VALUE];

  /**
   * @brief The model1 index of the target entity, used to find the cl_entity_t each frame.
   */
  int32_t model1;
} cg_entity_light_t;

static GArray *cg_entity_lights;

/**
 * @brief Allocates a dynamic light source.
 */
static cg_light_t *Cg_AllocLight(const cg_light_t *in) {

  cg_light_t *light = g_queue_pop_head(cg_lights.free);
  if (light == NULL) {
    light = cgi.Malloc(sizeof(cg_light_t), MEM_TAG_CGAME_LEVEL);
  }

  *light = *in;

  light->intensity = light->intensity ?: 1.f;

  light->time = cgi.client->unclamped_time;

  g_queue_push_head(cg_lights.allocated, light);
  return light;
}

/**
 * @brief Frees the specified light source.
 */
static void Cg_FreeLight(cg_light_t *light) {

  const bool removed = g_queue_remove(cg_lights.allocated, light);
  assert(removed);

  g_queue_push_head(cg_lights.free, light);
}

/**
 * @brief
 */
void Cg_AddLight(const cg_light_t *in) {

  if (!cg_add_lights->value) {
    return;
  }

  Cg_AllocLight(in);
}

/**
 * @brief Applies light style animation to an intensity value.
 * @param intensity The base intensity.
 * @param style The style string (a-z, animated at 10Hz). May be empty.
 * @return The animated intensity.
 */
static float Cg_AnimateLight(float intensity, const char *style) {

  if (*style) {
    const size_t len = strlen(style);
    const uint32_t style_index = (cgi.client->unclamped_time / 100) % len;
    const uint32_t style_time = (cgi.client->unclamped_time / 100) * 100;

    const float lerp = (cgi.client->unclamped_time - style_time) / 100.f;

    const float s = (style[(style_index + 0) % len] - 'a') / (float) ('z' - 'a');
    const float t = (style[(style_index + 1) % len] - 'a') / (float) ('z' - 'a');

    intensity *= Clampf(Mixf(s, t, lerp), FLT_EPSILON, 1.f);
  }

  return intensity;
}

/**
 * @brief Adds all BSP light sources to the view.
 */
static void Cg_AddBspLights(void) {

  r_bsp_light_t *l = cgi.WorldModel()->bsp->lights;
  for (int32_t i = 0; i < cgi.WorldModel()->bsp->num_lights; i++, l++) {

    const float intensity = Cg_AnimateLight(l->intensity ?: 1.f, l->style);

    cgi.AddLight(cgi.view, &(const r_light_t) {
      .origin = l->origin,
      .color = l->color,
      .radius = l->radius,
      .intensity = intensity,
      .bounds = l->bounds,
      .query = l->query,
      .bsp_light = l,
      .shadow_cached = &l->shadow_cached,
    });
  }
}

/**
 * @brief Adds all entity-attached light sources to the view.
 * @details Resolves the current world position of each entity light by finding
 * the `cl_entity_t` for its target inline model, then adds it as a dynamic light.
 */
static void Cg_AddBspEntityLights(void) {

  for (guint i = 0; i < cg_entity_lights->len; i++) {
    const cg_entity_light_t *el = &g_array_index(cg_entity_lights, cg_entity_light_t, i);

    // Find the cl_entity_t for this light's target model
    vec3_t origin = el->origin;
    for (int32_t j = 0; j < MAX_ENTITIES; j++) {
      const cl_entity_t *ent = &cgi.client->entities[j];
      if (ent->current.model1 == el->model1) {
        origin = Vec3_Add(el->origin, ent->current.origin);
        break;
      }
    }

    float intensity = Cg_AnimateLight(el->intensity ?: 1.f, el->style);

    cgi.AddLight(cgi.view, &(const r_light_t) {
      .origin = origin,
      .color = el->color,
      .radius = el->radius,
      .intensity = intensity,
      .bounds = Box3_FromCenterRadius(origin, el->radius),
    });
  }
}

/**
 * @brief
 */
void Cg_AddLights(void) {

  Cg_AddBspLights();
  Cg_AddBspEntityLights();

  GList *list = cg_lights.allocated->head;
  while (list != NULL) {
    GList *next = list->next; // for potential removal

    cg_light_t *light = list->data;

    const uint32_t age = cgi.client->unclamped_time - light->time;
    float intensity = light->intensity;
    if (light->decay) {
      intensity = Mixf(intensity, 0.f, age / (float) light->decay);
    }

    if (cg_add_lights->value) {
      cgi.AddLight(cgi.view, &(const r_light_t) {
        .origin = light->origin,
        .color = light->color,
        .radius = light->radius,
        .intensity = intensity,
        .bounds = Box3_FromCenterRadius(light->origin, light->radius),
        .source = light->source,
      });
    }

    if (age >= light->decay) {
      Cg_FreeLight(light);
    }

    list = next;
  }
}

/**
 * @brief Loads light entities that target inline BSP models and should move with them.
 */
static void Cg_InitBspEntityLights(void) {

  cg_entity_lights = g_array_new(false, false, sizeof(cg_entity_light_t));

  const cm_bsp_t *bsp = cgi.WorldModel()->bsp->cm;
  for (int32_t i = 0; i < bsp->num_entities; i++) {
    const cm_entity_t *def = bsp->entities[i];

    if (g_strcmp0(cgi.EntityValue(def, "classname")->string, "light")) {
      continue;
    }

    const char *target = cgi.EntityValue(def, "target")->nullable_string;
    if (!target) {
      continue;
    }

    // Find the entity with the matching targetname
    const cm_entity_t *target_def = NULL;
    for (int32_t j = 0; j < bsp->num_entities; j++) {
      const char *targetname = cgi.EntityValue(bsp->entities[j], "targetname")->nullable_string;
      if (!g_strcmp0(targetname, target)) {
        target_def = bsp->entities[j];
        break;
      }
    }

    if (!target_def) {
      Cg_Warn("Entity light @ %s: target \"%s\" not found\n",
              vtos(cgi.EntityValue(def, "origin")->vec3), target);
      continue;
    }

    // Resolve the target entity's model1 index from config strings
    const char *model = cgi.EntityValue(target_def, "model")->nullable_string;
    if (!model || model[0] != '*') {
      Cg_Warn("Entity light @ %s: target \"%s\" has no inline model\n",
              vtos(cgi.EntityValue(def, "origin")->vec3), target);
      continue;
    }

    int32_t model1 = -1;
    for (int32_t j = 1; j < MAX_MODELS; j++) {
      if (!g_strcmp0(cgi.client->config_strings[CS_MODELS + j], model)) {
        model1 = j;
        break;
      }
    }

    if (model1 == -1) {
      Cg_Warn("Entity light @ %s: target model \"%s\" not in config strings\n",
              vtos(cgi.EntityValue(def, "origin")->vec3), model);
      continue;
    }

    const vec3_t light_origin = cgi.EntityValue(def, "origin")->vec3;
    const vec3_t entity_origin = cgi.EntityValue(target_def, "origin")->vec3;

    cg_entity_light_t el = {
      .origin = Vec3_Subtract(light_origin, entity_origin),
      .color = cgi.EntityValue(def, "color")->vec3,
      .radius = cgi.EntityValue(def, "radius")->value ?: ENTITY_LIGHT_RADIUS,
      .intensity = cgi.EntityValue(def, "intensity")->value ?: ENTITY_LIGHT_INTENSITY,
      .model1 = model1,
    };

    if (Vec3_Equal(el.color, Vec3_Zero())) {
      el.color = ENTITY_LIGHT_COLOR;
    }

    g_strlcpy(el.style, cgi.EntityValue(def, "style")->string, sizeof(el.style));

    g_array_append_val(cg_entity_lights, el);

    Cg_Debug("Entity light: %s -> %s model1=%d offset=%s\n",
             vtos(light_origin), model, model1, vtos(el.origin));
  }
}

/**
 * @brief
 */
void Cg_InitLights(void) {

  memset(&cg_lights, 0, sizeof(cg_lights));

  cg_lights.allocated = g_queue_new();
  cg_lights.free = g_queue_new();

  Cg_InitBspEntityLights();
}

/**
 * @brief
 */
void Cg_FreeLights(void) {

  if (cg_lights.allocated) {
    g_queue_free_full(cg_lights.allocated, cgi.Free);
  }

  cg_lights.allocated = NULL;

  if (cg_lights.free) {
    g_queue_free_full(cg_lights.free, cgi.Free);
  }

  cg_lights.free = NULL;

  if (cg_entity_lights) {
    g_array_free(cg_entity_lights, true);
    cg_entity_lights = NULL;
  }
}
