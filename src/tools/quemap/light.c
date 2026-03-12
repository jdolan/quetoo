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
  return Mem_TagMalloc(sizeof(light_t), (mem_tag_t) MEM_TAG_LIGHT);
}

/**
 * @brief
 */
static void FreeLight(light_t *light) {
  Mem_Free(light);
}

/**
 * @brief Finds the team_master light entity for the given team.
 */
static const cm_entity_t *FindTeamMaster(const char *team) {

  if (!team) {
    return NULL;
  }

  cm_entity_t **e = Cm_Bsp()->entities;
  for (int32_t i = 0; i < Cm_Bsp()->num_entities; i++, e++) {
    const char *classname = Cm_EntityValue(*e, "classname")->string;
    if (!g_strcmp0(classname, "light")) {
      const char *ent_team = Cm_EntityValue(*e, "team")->nullable_string;
      if (ent_team && !g_strcmp0(ent_team, team)) {
        if (Cm_EntityValue(*e, "team_master")->parsed) {
          return *e;
        }
      }
    }
  }

  return NULL;
}

/**
 * @brief
 */
static light_t *LightForEntity(const cm_entity_t *entity) {

  const char *classname = Cm_EntityValue(entity, "classname")->string;
  if (!g_strcmp0(classname, "light")) {

    // Entity-attached lights are handled as dynamic lights at runtime; skip static baking.
    // Resolve team_master attributes now and stamp them back onto the cm_entity_t so that
    // the runtime can read them without needing to know about teams.
    if (Cm_EntityValue(entity, "target")->nullable_string) {

      float radius = Cm_EntityValue(entity, "radius")->value;
      vec3_t color = Cm_EntityValue(entity, "color")->vec3;
      float intensity = Cm_EntityValue(entity, "intensity")->value;
      char style[MAX_BSP_ENTITY_VALUE];
      g_strlcpy(style, Cm_EntityValue(entity, "style")->string, sizeof(style));

      const cm_entity_t *master = FindTeamMaster(Cm_EntityValue(entity, "team")->nullable_string);
      if (master) {
        radius = radius ?: Cm_EntityValue(master, "radius")->value;
        if (Vec3_Equal(Vec3_Zero(), color)) {
          color = Cm_EntityValue(master, "color")->vec3;
        }
        intensity = intensity ?: Cm_EntityValue(master, "intensity")->value;
        if (!*style) {
          g_strlcpy(style, Cm_EntityValue(master, "style")->string, sizeof(style));
        }
      }

      radius = radius ?: LIGHT_RADIUS;
      if (Vec3_Equal(Vec3_Zero(), color)) {
        color = LIGHT_COLOR;
      }
      intensity = intensity ?: LIGHT_INTENSITY;

      Cm_EntitySetKeyValue((cm_entity_t *) entity, "radius", ENTITY_FLOAT, &radius);
      Cm_EntitySetKeyValue((cm_entity_t *) entity, "color", ENTITY_VEC3, &color);
      Cm_EntitySetKeyValue((cm_entity_t *) entity, "intensity", ENTITY_FLOAT, &intensity);
      Cm_EntitySetKeyValue((cm_entity_t *) entity, "style", ENTITY_STRING, style);

      return NULL;
    }

    light_t *light = AllocLight();

    light->entity = Cm_EntityNumber(entity);
    light->origin = Cm_EntityValue(entity, "origin")->vec3;
    light->radius = Cm_EntityValue(entity, "radius")->value;
    light->color = Cm_EntityValue(entity, "color")->vec3;
    light->intensity = Cm_EntityValue(entity, "intensity")->value;
    g_strlcpy(light->style, Cm_EntityValue(entity, "style")->string, sizeof(light->style));

    const cm_entity_t *master = FindTeamMaster(Cm_EntityValue(entity, "team")->nullable_string);
    if (master) {
      light->radius = light->radius ?: Cm_EntityValue(master, "radius")->value;

      if (Vec3_Equal(Vec3_Zero(), light->color)) {
        light->color = Cm_EntityValue(master, "color")->vec3;
      }

      light->intensity = light->intensity ?: Cm_EntityValue(master, "intensity")->value;

      if (!*light->style) {
        g_strlcpy(light->style, Cm_EntityValue(master, "style")->string, sizeof(light->style));
      }
    }

    light->radius = light->radius ?: LIGHT_RADIUS;

    if (Vec3_Equal(Vec3_Zero(), light->color)) {
      light->color = LIGHT_COLOR;
    }

    light->intensity = light->intensity ?: LIGHT_INTENSITY;

    light->bounds = Box3_FromCenterRadius(light->origin, light->radius);
    light->visible_bounds = Box3_Null();

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
 * @brief Re-serializes the BSP entity string from the parsed cm_entity_t definitions.
 * @details Called after BuildLights so that any key-values stamped onto cm_entity_t
 * during the light pass (e.g. team_master resolution on entity-attached lights) are
 * flushed back to the on-disk entity lump.
 */
void EmitEntityString(void) {

  char *out = bsp_file.entity_string;
  *out = '\0';

  const cm_bsp_t *bsp = Cm_Bsp();
  for (int32_t i = 0; i < bsp->num_entities; i++) {
    g_strlcat(out, "{\n", MAX_BSP_ENTITIES_SIZE);
    for (const cm_entity_t *e = bsp->entities[i]; e; e = e->next) {
      g_strlcat(out, va(" \"%s\" \"%s\"\n", e->key, e->string), MAX_BSP_ENTITIES_SIZE);
    }
    g_strlcat(out, "}\n", MAX_BSP_ENTITIES_SIZE);
  }

  const size_t len = strlen(out);
  if (len == MAX_BSP_ENTITIES_SIZE - 1) {
    Com_Error(ERROR_FATAL, "MAX_BSP_ENTITIES_SIZE\n");
  }

  bsp_file.entity_string_size = (int32_t) len + 1;
}

/**
 * @brief
 */
void BuildLights(void) {

  const uint32_t start = (uint32_t) SDL_GetTicks();

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

  Com_Print("\r%-24s [100%%] %d ms\n", "Building lights", (uint32_t) SDL_GetTicks() - start);

  Com_Verbose("Lighting for %d lights\n", lights->len);
}

/**
 * @brief
 */
void EmitLights(void) {

  if (!lights) {
    return;
  }

  const uint32_t start = (uint32_t) SDL_GetTicks();

  bsp_file.num_lights = lights->len;

  if (bsp_file.num_lights >= MAX_BSP_LIGHTS) {
    Com_Error(ERROR_FATAL, "MAX_BSP_LIGHTS\n");
  }

  Bsp_AllocLump(&bsp_file, BSP_LUMP_ELEMENTS, MAX_BSP_ELEMENTS);
  Bsp_AllocLump(&bsp_file, BSP_LUMP_LIGHTS, bsp_file.num_lights);

  bsp_light_t *out = bsp_file.lights;
  for (guint i = 0; i < lights->len; i++) {

    light_t *light = g_ptr_array_index(lights, i);

    if (Box3_IsNull(light->visible_bounds)) {
      continue;
    }

    light->out = out;

    out->entity = light->entity;
    out->origin = light->origin;
    out->radius = light->radius;
    out->color = light->color;
    out->intensity = light->intensity;
    out->bounds = light->visible_bounds;
    g_strlcpy(out->style, light->style, sizeof(out->style));

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

      if (side->surface & SURF_LIQUID) {
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

  Com_Print("\r%-24s [100%%] %d ms\n", "Emitting lights", (uint32_t) SDL_GetTicks() - start);
}

