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

#include "cl_local.h"

/**
 * @brief EntityViewDelegate for lights.
 */
static void Cl_UpdateEditedEntity_light(int16_t number, cm_entity_t *entity) {
  const r_bsp_model_t *bsp = r_models.world->bsp;

  r_bsp_light_t *light = NULL;

  r_bsp_light_t *l = bsp->lights;
  for (int32_t i = 0; i < bsp->num_lights; i++, l++) {
    if (l->entity == bsp->cm->entities[number]) {
      light = l;
      break;
    }
  }

  assert(light);

  const cm_entity_t *color = Cm_EntityValue(entity, "color");

  light->origin = Cm_EntityValue(entity, "origin")->vec3;
  light->radius = Cm_EntityValue(entity, "radius")->value ?: 300.f;
  light->color = (color->parsed & ENTITY_VEC3) ? color->vec3 : Vec3(1.f, 1.f, 1.f);
  light->intensity = Cm_EntityValue(entity, "intensity")->value ?: 1.f;
  light->bounds = Box3_FromCenterRadius(light->origin, light->radius);
  light->depth_pass_elements = NULL;
  light->num_depth_pass_elements = bsp->num_elements;
}

/**
 * @brief
 */
void Cl_UpdateEditedEntity(int16_t number) {
  const cm_bsp_t *bsp = Cm_Bsp();

  const char *info = cl.config_strings[CS_ENTITIES + number];
  cm_entity_t *entity = Cm_EntityFromInfoString(info);

  if (number < bsp->num_entities) {
    Mem_Free(bsp->entities[number]);
    bsp->entities[number] = entity;
  } else {
    
  }

  const cm_entity_t *classname = Cm_EntityValue(entity, "classname");
  Com_Debug(DEBUG_CLIENT, "%d %s\n", number, classname->string);

  if (!g_strcmp0(classname->string, "light")) {
    Cl_UpdateEditedEntity_light(number, entity);
  }

  Mem_Free(entity);
}
