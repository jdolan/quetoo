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
   * @brief The allocated lights.
   */
  List *allocated;

  /**
   * @brief The free lights.
   */
  List *free;
} cg_lights;

static cg_light_t *Cg_PopLight(List *lights) {

  if (lights == NULL || lights->head == NULL) {
    return NULL;
  }

  ListNode *node = lights->head;
  cg_light_t *light = node->element;

  $(lights, removeNode, node);

  return light;
}

/**
 * @brief Allocates a dynamic light source.
 */
static cg_light_t *Cg_AllocLight(const cg_light_t *in) {

  cg_light_t *light = Cg_PopLight(cg_lights.free);
  if (light == NULL) {
    light = cgi.Malloc(sizeof(cg_light_t), MEM_TAG_CGAME_LEVEL);
  }

  *light = *in;

  light->intensity = light->intensity ?: 1.f;

  light->time = cgi.client->unclamped_time;

  $(cg_lights.allocated, prependElement, light);
  return light;
}

/**
 * @brief Frees the specified light source.
 */
static void Cg_FreeLight(cg_light_t *light) {

  ListNode *node = $(cg_lights.allocated, nodeForElement, light);
  assert(node);

  $(cg_lights.allocated, removeNode, node);
  $(cg_lights.free, prependElement, light);
}

/**
 * @brief Adds a dynamic light source to the current view if dynamic lights are enabled.
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
 * @param drift A phase offset (0-1 fraction of the style cycle) to desynchronize instances.
 * @return The animated intensity.
 */
float Cg_AnimateLight(float intensity, const char *style, float drift) {

  if (style && *style) {
    const size_t len = strlen(style);
    const uint32_t phase_offset = (uint32_t)(drift * len * 100);
    const uint32_t time = cgi.client->unclamped_time + phase_offset;
    const uint32_t style_index = (time / 100) % len;
    const uint32_t style_time = (time / 100) * 100;

    const float lerp = (time - style_time) / 100.f;

    const float s = (style[(style_index + 0) % len] - 'a') / (float) ('z' - 'a');
    const float u = (style[(style_index + 1) % len] - 'a') / (float) ('z' - 'a');

    intensity *= Clampf(Mixf(s, u, lerp), FLT_EPSILON, 1.f);
  }

  return intensity;
}

/**
 * @brief Resolves the model1 index for a BSP inline model string (e.g. "*3").
 * @return The model1 index, or -1 if not found.
 */
static int32_t Cg_ResolveModel1(const char *model) {

  for (int32_t i = 1; i < MAX_MODELS; i++) {
    if (!strcmp(cgi.client->config_strings[CS_MODELS + i], model)) {
      return i;
    }
  }

  return -1;
}

/**
 * @brief Adds all BSP light sources to the view.
 * @details For lights with a `target_entity`, resolves the current world position of the
 * attached inline model entity each frame and adds the light as a dynamic (unshadowed) light.
 */
static void Cg_AddBspLights(void) {

  r_bsp_light_t *l = cgi.WorldModel()->bsp->lights;
  for (int32_t i = 0; i < cgi.WorldModel()->bsp->num_lights; i++, l++) {

    const float intensity = Cg_AnimateLight(l->intensity ?: 1.f, l->style, l->drift);

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
          origin = Vec3_Add(l->origin, ent->origin);
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
 * @brief Adds all BSP and allocated dynamic lights to the current view.
 */
void Cg_AddLights(void) {

  Cg_AddBspLights();

  for (ListNode *node = cg_lights.allocated->head; node; ) {
    ListNode *next = node->next;

    cg_light_t *light = node->element;

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
        .flags = light->flags,
      });
    }

    if (age >= light->decay) {
      Cg_FreeLight(light);
    }

    node = next;
  }
}

/**
 * @brief Initializes the allocated and free dynamic light queues.
 */
void Cg_InitLights(void) {

  memset(&cg_lights, 0, sizeof(cg_lights));

  cg_lights.allocated = $(alloc(List), init);
  cg_lights.free = $(alloc(List), init);
}

/**
 * @brief Frees all allocated and free dynamic light queue memory.
 */
void Cg_FreeLights(void) {

  if (cg_lights.allocated) {
    cg_lights.allocated->destroy = (ListDestroyFunc) cgi.Free;
    $(cg_lights.allocated, removeAll);
    release(cg_lights.allocated);
  }

  cg_lights.allocated = NULL;

  if (cg_lights.free) {
    cg_lights.free->destroy = (ListDestroyFunc) cgi.Free;
    $(cg_lights.free, removeAll);
    release(cg_lights.free);
  }

  cg_lights.free = NULL;
}
