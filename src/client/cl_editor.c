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
 * @brief
 */
void Cl_ParseEditorEntity(int16_t number, const char *info) {

  if (cl.entity_definitions[number]) {
    Cm_FreeEntity(cl.entity_definitions[number]);
  }

  cl.entity_definitions[number] = Cm_EntityFromInfoString(info);
}

/**
 * @brief
 */
void Cl_PopulateEditorScene(const cl_frame_t *frame) {

  for (int32_t i = 0; i < MAX_ENTITIES; i++) {

    const cm_entity_t *e = cl.entity_definitions[i];
    const char *classname = Cm_EntityValue(e, "classname")->string;

    if (!g_strcmp0(classname, "light")) {

      r_light_t light = {
        .origin = Cm_EntityValue(e, "origin")->vec3,
        .radius = Cm_EntityValue(e, "radius")->value ?: 300.f,
        .intensity = Cm_EntityValue(e, "intensity")->value ?: 1.f,
        .color = Cm_EntityValue(e, "color")->vec3
      };

      if (Vec3_Equal(light.color, Vec3_Zero())) {
        light.color = Vec3(1.f, 1.f, 1.f);
      }

      light.bounds = Box3_FromCenterRadius(light.origin, light.radius);

      R_AddLight(&cl_view, &light);
    }
  }
}
