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

#include "light.h"
#include "points.h"
#include "qlight.h"

GPtrArray *lights = NULL;

/**
 * @brief
 */
static light_t *AllocLight(void) {
  return Mem_TagMalloc(sizeof(light_t), MEM_TAG_LIGHT);
}

/**
 * @brief
 */
static void FreeLight(light_t *light) {
  Mem_Free(light);
}

/**
 * @brief
 */
const cm_entity_t *EntityTarget(const cm_entity_t *entity) {

  const char *targetname = Cm_EntityValue(entity, "target")->nullable_string;
  if (targetname) {

    cm_entity_t **e = Cm_Bsp()->entities;
    for (int32_t i = 0; i < Cm_Bsp()->num_entities; i++, e++) {
      if (!g_strcmp0(targetname, Cm_EntityValue(*e, "targetname")->nullable_string)) {
        return *e;
      }
    }
  }

  return NULL;
}

/**
 * @brief
 */
static light_t *LightForEntity(const cm_entity_t *entity) {

  const char *class_name = Cm_EntityValue(entity, "classname")->string;
  if (!g_strcmp0(class_name, "light")) {

    light_t *light = AllocLight();

    light->entity = Cm_EntityNumber(entity);
    light->flags = Cm_EntityValue(entity, "spawnflags")->integer;
    light->origin = Cm_EntityValue(entity, "origin")->vec3;
    light->radius = Cm_EntityValue(entity, "radius")->value ?: LIGHT_RADIUS;
    light->intensity = Cm_EntityValue(entity, "intensity")->value ?: LIGHT_INTENSITY;
    light->color = Cm_EntityValue(entity, "color")->vec3;

    if (Vec3_Equal(Vec3_Zero(), light->color)) {
      light->color = LIGHT_COLOR;
    }

    light->bounds = Box3_FromCenter(light->origin);

    return light;
  } else {
    return NULL;
  }
}

/**
 * @brief
 */
void FreeLights(void) {

  if (!lights) {
    return;
  }

  g_ptr_array_free(lights, true);
  lights = NULL;
}

/**
 * @brief
 */
void BuildLights(void) {

  const uint32_t start = SDL_GetTicks();

  Progress("Building lights", 0);

  lights = lights ?: g_ptr_array_new_with_free_func((GDestroyNotify) FreeLight);

  const int32_t count = Cm_Bsp()->num_entities + Cm_Bsp()->num_brush_sides;

  cm_entity_t **entity = Cm_Bsp()->entities;
  for (int32_t i = 0; i < Cm_Bsp()->num_entities; i++, entity++) {
    light_t *light = LightForEntity(*entity);
    if (light) {
      g_ptr_array_add(lights, light);
    }
    Progress("Building lights", i * 100.f / count);
  }

  Com_Print("\r%-24s [100%%] %d ms\n", "Building lights", SDL_GetTicks() - start);

  Com_Verbose("Lighting for %d lights\n", lights->len);
}

/**
 * @brief
 */
void EmitLights(void) {

  if (!lights) {
    return;
  }

  const uint32_t start = SDL_GetTicks();

  bsp_file.num_lights = lights->len;

  if (bsp_file.num_lights >= MAX_BSP_LIGHTS) {
    Com_Error(ERROR_FATAL, "MAX_BSP_LIGHTS\n");
  }

  Bsp_AllocLump(&bsp_file, BSP_LUMP_ELEMENTS, MAX_BSP_ELEMENTS);
  Bsp_AllocLump(&bsp_file, BSP_LUMP_LIGHTS, bsp_file.num_lights);

  bsp_light_t *out = bsp_file.lights;
  for (guint i = 0; i < lights->len; i++) {

    light_t *light = g_ptr_array_index(lights, i);
    light->out = out;

    out->entity = light->entity;
    out->flags = light->flags;
    out->origin = light->origin;
    out->radius = light->radius;
    out->color = light->color;
    out->intensity = light->intensity;
    out->bounds = light->bounds;

    out->bounds = Box3_Expand(out->bounds, BSP_VOXEL_SIZE * .5f);

    out->first_depth_pass_element = bsp_file.num_elements;

    const bsp_model_t *worldspawn = bsp_file.models;
    const bsp_face_t *face = &bsp_file.faces[worldspawn->first_face];
    for (int32_t j = 0; j < worldspawn->num_faces; j++, face++) {

      if (!Box3_Intersects(face->bounds, out->bounds)) {
        continue;
      }

      const bsp_brush_side_t *side = &bsp_file.brush_sides[face->brush_side];
      if (side->contents & CONTENTS_MIST) {
        continue;
      }

      if (side->surface & SURF_MASK_NO_DRAW_ELEMENTS) {
        continue;
      }

      if (side->surface & SURF_MASK_TRANSLUCENT) {
        continue;
      }

      if (bsp_file.num_elements + face->num_elements >= MAX_BSP_ELEMENTS) {
        Com_Error(ERROR_FATAL, "MAX_BSP_ELEMENTS\n");
      }

      memcpy(bsp_file.elements + bsp_file.num_elements,
           bsp_file.elements + face->first_element,
           sizeof(int32_t) * face->num_elements);

      bsp_file.num_elements += face->num_elements;
      out->num_depth_pass_elements += face->num_elements;
    }

    out++;

    Progress("Emitting lights", 100.f * i / lights->len);
  }

  Com_Print("\r%-24s [100%%] %d ms\n\n", "Emitting lights", SDL_GetTicks() - start);
}
