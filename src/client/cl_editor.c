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
 * @brief The editor entities.
 */
static cl_editor_entity_t cl_editor_entities[MAX_ENTITIES];

/**
 * @brief
 */
void Cl_ParseEditorEntity(int16_t number, const char *info) {

  Cm_FreeEntity(cl.entity_definitions[number]);

  if (strlen(info)) {
    cl.entity_definitions[number] = Cm_EntityFromInfoString(info);
  } else {
    cl.entity_definitions[number] = NULL;
  }

  cl_editor_entity_t *edit = &cl_editor_entities[number];
  memset(edit, 0, sizeof(*edit));
  edit->number = number;
  edit->ent = &cl.entities[number];
  edit->def = cl.entity_definitions[number];

  for (int32_t i = 0; i < MAX_ENTITIES; i++) {
    cl_editor_entities[i].shadow_cached = false;
  }

  SDL_PushEvent(&(SDL_Event) {
    .user.type = MVC_NOTIFICATION_EVENT,
    .user.code = NOTIFICATION_ENTITY_PARSED,
    .user.data1 = (void *) (intptr_t) edit->number
  });
}

/**
 * @brief Finds the `team_master` entity for the given classname and team.
 */
int32_t Cl_FindTeamMaster(const char *classname, const char *team) {

  if (!team) {
    return -1;
  }

  for (int32_t i = 0; i < MAX_ENTITIES; i++) {
    const cm_entity_t *e = cl.entity_definitions[i];
    if (!e) {
      continue;
    }

    if (!g_strcmp0(Cm_EntityValue(e, "classname")->string, classname)) {
      if (!g_strcmp0(Cm_EntityValue(e, "team")->string, team)) {
        if (Cm_EntityValue(e, "team_master")->parsed) {
          return i;
        }
      }
    }
  }

  return -1;
}

/**
 * @brief
 */
static void Cl_PopulateEditorLight(cl_editor_entity_t *edit, r_light_t *light) {

  light->origin = Cm_EntityValue(edit->def, "origin")->vec3;
  light->radius = Cm_EntityValue(edit->def, "radius")->value;
  light->color = Cm_EntityValue(edit->def, "color")->vec3;
  light->intensity = Cm_EntityValue(edit->def, "intensity")->value;

  const char *style = Cm_EntityValue(edit->def, "style")->nullable_string;
  const char *team = Cm_EntityValue(edit->def, "team")->nullable_string;

  const int32_t team_master = Cl_FindTeamMaster("light", team);
  if (team_master > -1) {
    const cm_entity_t *master = cl.entity_definitions[team_master];
    light->radius = light->radius ?: Cm_EntityValue(master, "radius")->value;
    light->color = Vec3_Equal(Vec3_Zero(), light->color) ? Cm_EntityValue(master, "color")->vec3 : light->color;
    light->intensity = light->intensity ?: Cm_EntityValue(master, "intensity")->value;

    if (!style) {
      style = Cm_EntityValue(master, "style")->nullable_string;
    }
  }

  light->radius = light->radius ?: LIGHT_RADIUS;
  light->color = Vec3_Equal(Vec3_Zero(), light->color) ? LIGHT_COLOR : light->color;
  light->intensity = light->intensity ?: LIGHT_INTENSITY;
  light->bounds = Box3_FromCenterRadius(light->origin, light->radius);

  if (style && *style) {
    const size_t len = strlen(style);
    const uint32_t style_index = (cl.unclamped_time / 100) % len;
    const uint32_t style_time = (cl.unclamped_time / 100) * 100;

    const float lerp = (cl.unclamped_time - style_time) / 100.f;

    const float s = (style[(style_index + 0) % len] - 'a') / (float) ('z' - 'a');
    const float t = (style[(style_index + 1) % len] - 'a') / (float) ('z' - 'a');

    const float atten = Clampf(Mixf(s, t, lerp), FLT_EPSILON, 1.f);
    light->intensity *= atten;
  }

  light->shadow_cached = &edit->shadow_cached;
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

  cl_editor_entity_t *edit = cl_editor_entities;
  for (int32_t i = 0; i < MAX_ENTITIES; i++, edit++) {

    if (!edit->def) {
      continue;
    }

    bool in_frame = false;
    for (int32_t j = 0; j < frame->num_entities; j++) {
      const uint32_t snum = (frame->entity_state + j) & ENTITY_STATE_MASK;
      const entity_state_t *s = &cl.entity_states[snum];
      if (s->number == edit->number) {
        in_frame = true;
        break;
      }
    }

    if (!in_frame) {
      continue;
    }

    vec4_t color = Color32_Vec4(edit->ent->current.color);

    const char *classname = Cm_EntityValue(edit->def, "classname")->string;
    if (!g_strcmp0(classname, "light")) {

      r_light_t light = { 0 };
      Cl_PopulateEditorLight(edit, &light);
      R_AddLight(&cl_view, &light);

      color = Vec3_ToVec4(light.color, 1.f);
    }

    const char *mod = Cm_EntityValue(edit->def, "model")->string;
    r_model_t *model = strlen(mod) ? R_LoadModel(mod) : NULL;

    R_AddEntity(&cl_view, &(const r_entity_t) {
      .id = edit,
      .origin = edit->ent->origin,
      .angles = edit->ent->angles,
      .scale = 1.f,
      .bounds = edit->ent->bounds,
      .abs_bounds = edit->ent->abs_bounds,
      .color = color,
      .effects = edit->ent->current.effects,
      .model = model
    });
  }
}
