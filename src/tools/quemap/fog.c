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

#include "fog.h"
#include "light.h"
#include "material.h"

GArray *fogs;

/**
 * @brief
 */
bool PointInsideFog(const vec3_t point, const fog_t *fog) {

  if (Box3_ContainsPoint(fog->bounds, point)) {

    for (guint i = 0; i < fog->brushes->len; i++) {
      const cm_bsp_brush_t *brush = g_ptr_array_index(fog->brushes, i);
      if (Cm_PointInsideBrush(point, brush)) {
        return true;
      }
    }
  }

  return false;
}

/**
 * @brief
 */
void FreeFog(void) {

  if (!fogs) {
    return;
  }

  g_array_free(fogs, true);
  fogs = NULL;
}

/**
 * @brief
 */
static void FogForEntity(const cm_entity_t *entity) {

  const char *classname = Cm_EntityValue(entity, "classname")->string;
  if (!g_strcmp0(classname, "worldspawn")) {

    if (Cm_EntityValue(entity, "fog_absorption")->parsed ||
      Cm_EntityValue(entity, "fog_color")->parsed ||
      Cm_EntityValue(entity, "fog_density")->parsed) {

      fog_t fog = {};
      fog.type = FOG_GLOBAL;
      fog.entity = entity;

      fog.density = Cm_EntityValue(entity, "fog_density")->value ?: FOG_DENSITY;

      fogs = g_array_append_val(fogs, fog);
    }
  } else if (!g_strcmp0(classname, "misc_fog")) {

    fog_t fog = {};
    fog.type = FOG_VOLUME;
    fog.entity = entity;

    fog.brushes = Cm_EntityBrushes(entity);
    fog.bounds = Box3_Null();

    for (guint i = 0; i < fog.brushes->len; i++) {
      const cm_bsp_brush_t *brush = g_ptr_array_index(fog.brushes, i);
      fog.bounds = Box3_Union(fog.bounds, brush->bounds);
    }

    fog.density = Cm_EntityValue(entity, "density")->value ?: FOG_DENSITY;

    fogs = g_array_append_val(fogs, fog);
  }
}

/**
 * @brief
 */
void BuildFog(void) {

  FreeFog();

  fogs = g_array_new(false, false, sizeof(fog_t));

  cm_entity_t **entity = Cm_Bsp()->entities;
  for (int32_t i = 0; i < Cm_Bsp()->num_entities; i++, entity++) {
    FogForEntity(*entity);
  }

  Com_Verbose("Fog lighting for %d sources\n", fogs->len);
}
