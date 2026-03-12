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
 * @brief Resolves the model1 index for a BSP inline model string (e.g. "*3").
 * @return The model1 index, or -1 if not found.
 */
static int32_t Cg_ResolveModel1(const char *model) {

  for (int32_t i = 1; i < MAX_MODELS; i++) {
    if (!g_strcmp0(cgi.client->config_strings[CS_MODELS + i], model)) {
      return i;
    }
  }

  return -1;
}

/**
 * @brief Adds all BSP light sources to the view.
 * @details For lights with a target_entity, resolves the current world position of the
 * attached inline model entity each frame and adds the light as a dynamic (unshadowed) light.
 */
static void Cg_AddBspLights(void) {

  r_bsp_light_t *l = cgi.WorldModel()->bsp->lights;
  for (int32_t i = 0; i < cgi.WorldModel()->bsp->num_lights; i++, l++) {

    const float intensity = Cg_AnimateLight(l->intensity ?: 1.f, l->style);

    if (l->target_entity) {
      // Resolve the inline model string and find the matching cl_entity_t each frame.
      const char *model = cgi.EntityValue(l->target_entity, "model")->nullable_string;
      if (!model) {
        continue;
      }

      const int32_t model1 = Cg_ResolveModel1(model);
      if (model1 == -1) {
        continue;
      }

      vec3_t origin = l->origin;
      for (int32_t j = 0; j < MAX_ENTITIES; j++) {
        const cl_entity_t *ent = &cgi.client->entities[j];
        if (ent->current.model1 == model1) {
          origin = Vec3_Add(l->origin, ent->current.origin);
          break;
        }
      }

      cgi.AddLight(cgi.view, &(const r_light_t) {
        .origin = origin,
        .color = l->color,
        .radius = l->radius,
        .intensity = intensity,
        .bounds = Box3_FromCenterRadius(origin, l->radius),
      });
    } else {
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
}

/**
 * @brief
 */
void Cg_AddLights(void) {

  Cg_AddBspLights();

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
 * @brief
 */
void Cg_InitLights(void) {

  memset(&cg_lights, 0, sizeof(cg_lights));

  cg_lights.allocated = g_queue_new();
  cg_lights.free = g_queue_new();
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
}
