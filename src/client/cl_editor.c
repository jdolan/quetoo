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

#define LIGHT_RADIUS 300.f
#define LIGHT_COLOR Vec3(1.f, 1.f, 1.f)
#define LIGHT_INTENSITY 1.f

/**
 * @brief Persistent storage for editor light shadow cache state.
 * @details Indexed by entity number to track shadowmap caching per light.
 */
static bool cl_editor_shadow_cached[MAX_ENTITIES];

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

  cl_editor_shadow_cached[number] = false;

  SDL_PushEvent(&(SDL_Event) {
    .user.type = MVC_NOTIFICATION_EVENT,
    .user.code = NOTIFICATION_ENTITY_PARSED,
    .user.data1 = (void *) (intptr_t) number
  });
}

/**
 * @brief Finds the `team_master` light entity for the given team.
 */
int32_t Cl_FindTeamMaster(const char *team) {

  if (!team) {
    return -1;
  }

  for (int32_t i = 0; i < MAX_ENTITIES; i++) {
    const cm_entity_t *e = cl.entity_definitions[i];
    if (!e) {
      continue;
    }

    if (!g_strcmp0(Cm_EntityValue(e, "team")->string, team)) {
      if (Cm_EntityValue(e, "team_master")->parsed) {
        return i;
      }
    }
  }

  return -1;
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

  R_AddEntity(&cl_view, &(const r_entity_t) {
    .model = R_WorldModel()->bsp->worldspawn,
    .scale = 1.f
  });

  for (int32_t i = 0; i < MAX_ENTITIES; i++) {

    const cl_entity_t *ent = &cl.entities[i];
    const cm_entity_t *def = cl.entity_definitions[i];

    if (!def) {
      continue;
    }

    bool in_frame = false;
    for (int32_t j = 0; j < frame->num_entities; j++) {
      const uint32_t snum = (frame->entity_state + j) & ENTITY_STATE_MASK;
      const entity_state_t *s = &cl.entity_states[snum];
      if (s->number == i) {
        in_frame = true;
        break;
      }
    }

    if (!in_frame) {
      continue;
    }

    vec4_t color = Color32_Vec4(ent->current.color);

    const char *classname = Cm_EntityValue(def, "classname")->string;
    if (!g_strcmp0(classname, "light")) {

      r_light_t light = { 0 };

      light.origin = Cm_EntityValue(def, "origin")->vec3;
      light.radius = Cm_EntityValue(def, "radius")->value;
      light.color = Cm_EntityValue(def, "color")->vec3;
      light.intensity = Cm_EntityValue(def, "intensity")->value;

      const char *style = Cm_EntityValue(def, "style")->nullable_string;

      const int32_t team_master = Cl_FindTeamMaster(Cm_EntityValue(def, "team")->nullable_string);
      if (team_master > -1) {
        const cm_entity_t *master = cl.entity_definitions[team_master];
        light.radius = light.radius ?: Cm_EntityValue(master, "radius")->value;

        if (Vec3_Equal(Vec3_Zero(), light.color)) {
          light.color = Cm_EntityValue(master, "color")->vec3;
        }

        light.intensity = light.intensity ?: Cm_EntityValue(master, "intensity")->value;

        if (!style) {
          style = Cm_EntityValue(master, "style")->nullable_string;
        }
      }

      light.radius = light.radius ?: LIGHT_RADIUS;

      if (Vec3_Equal(Vec3_Zero(), light.color)) {
        light.color = LIGHT_COLOR;
      }

      light.intensity = light.intensity ?: LIGHT_INTENSITY;

      light.bounds = Box3_FromCenterRadius(light.origin, light.radius);

      if (style && *style) {
        const size_t len = strlen(style);
        const uint32_t style_index = (cl.unclamped_time / 100) % len;
        const uint32_t style_time = (cl.unclamped_time / 100) * 100;

        const float lerp = (cl.unclamped_time - style_time) / 100.f;

        const float s = (style[(style_index + 0) % len] - 'a') / (float) ('z' - 'a');
        const float t = (style[(style_index + 1) % len] - 'a') / (float) ('z' - 'a');

        const float style_value = Clampf(Mixf(s, t, lerp), FLT_EPSILON, 1.f);
        light.intensity *= style_value;
      }

      light.shadow_cached = &cl_editor_shadow_cached[i];

      R_AddLight(&cl_view, &light);

      color = Vec3_ToVec4(light.color, 1.f);
    }

    R_AddEntity(&cl_view, &(const r_entity_t) {
      .id = ent,
      .origin = ent->origin,
      .angles = ent->angles,
      .scale = 1.f,
      .bounds = ent->bounds,
      .abs_bounds = ent->abs_bounds,
      .color = color,
    });
  }
}
