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

  if (strlen(info)) {
    cl.entity_definitions[number] = Cm_EntityFromInfoString(info);
  } else {
    cl.entity_definitions[number] = NULL;
  }

  SDL_PushEvent(&(SDL_Event) {
    .user.type = MVC_NOTIFICATION_EVENT,
    .user.code = NOTIFICATION_ENTITY_PARSED,
    .user.data1 = (void *) (intptr_t) number
  });
}

/**
 * @brief
 */
void Cl_PopulateEditorScene(const cl_frame_t *frame) {
  static bool did_print_help = false;

  if (!did_print_help) {
    Com_Print("^5In-game editor enabled\n");
    Com_Print("^5To select an entity, place your crosshair over it and press ESC\n");
    Com_Print("^5To move a selected entity, use W, A, S, D, E, C\n");
    Com_Print("^5Use your normal hotkeys to cut, copy and paste entities\n");
    Com_Print("^6To select a material, place your crosshair over it and press ESC\n");

    did_print_help = true;
  }

  const cm_entity_t *worldspawn = cl.entity_definitions[0];

  R_AddEntity(&cl_view, &(const r_entity_t) {
    .model = R_WorldModel()->bsp->worldspawn,
    .scale = 1.f
  });

  cl_view.ambient = Cm_EntityValue(worldspawn, "ambient")->value;

  for (int32_t i = 0; i < frame->num_entities; i++) {

    const uint32_t snum = (frame->entity_state + i) & ENTITY_STATE_MASK;
    const entity_state_t *s = &cl.entity_states[snum];
    const cl_entity_t *ent = &cl.entities[s->number];
    const cm_entity_t *def = cl.entity_definitions[s->number];

    const char *classname = Cm_EntityValue(def, "classname")->string;
    if (!g_strcmp0(classname, "light")) {

      r_light_t light = { 0 };

      light.origin = Cm_EntityValue(def, "origin")->vec3;
      light.radius = Cm_EntityValue(def, "radius")->value;
      light.color = Cm_EntityValue(def, "color")->vec3;
      light.intensity = Cm_EntityValue(def, "intensity")->value ?: 1.f;
      light.bounds = Box3_FromCenterRadius(light.origin, light.radius);

      const char *style = Cm_EntityValue(def, "style")->string;
      if (*style) {
        const size_t len = strlen(style);
        const uint32_t style_index = (cl.unclamped_time / 100) % len;
        const uint32_t style_time = (cl.unclamped_time / 100) * 100;

        const float lerp = (cl.unclamped_time - style_time) / 100.f;

        const float s = (style[(style_index + 0) % len] - 'a') / (float) ('z' - 'a');
        const float t = (style[(style_index + 1) % len] - 'a') / (float) ('z' - 'a');

        const float style_value = Clampf(Mixf(s, t, lerp), FLT_EPSILON, 1.f);
        light.intensity *= style_value;
      }

      R_AddLight(&cl_view, &light);
    }

    R_AddEntity(&cl_view, &(const r_entity_t) {
      .id = ent,
      .origin = ent->origin,
      .angles = ent->angles,
      .scale = 1.f,
      .bounds = ent->bounds,
      .abs_bounds = ent->abs_bounds,
      .color = Color32_Vec4(ent->current.color),
    });
  }
}
