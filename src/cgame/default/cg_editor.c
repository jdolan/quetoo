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

#include "ui/editor/EditorViewController.h"

static ViewController *cg_editorViewController;

/**
 * @brief Finds the `team_master` entity for the given classname and team.
 */
int32_t Cg_FindTeamMaster(const char *classname, const char *team) {

  if (!team) {
    return -1;
  }

  for (int32_t i = 0; i < MAX_ENTITIES; i++) {
    const cm_entity_t *e = cgi.client->entity_definitions[i];
    if (!e) {
      continue;
    }

    if (!g_strcmp0(cgi.EntityValue(e, "classname")->string, classname)) {
      if (!g_strcmp0(cgi.EntityValue(e, "team")->string, team)) {
        if (cgi.EntityValue(e, "team_master")->parsed) {
          return i;
        }
      }
    }
  }

  return -1;
}

/**
 * @brief Populates the scene with editor entities for the interpolated frame.
 */
void Cg_PopulateEditorScene(const cl_frame_t *frame) {
  static bool did_print_help = false;

  if (!did_print_help) {
    cgi.Print("^5In-game editor enabled\n");
    cgi.Print("^5To select an entity, place your crosshair over it and press ESC\n");
    cgi.Print("^5To move a selected entity, use W, A, S, D, E, C\n");
    cgi.Print("^5Use your normal hotkeys to cut, copy and paste entities\n");
    cgi.Print("^6To select a material, place your crosshair over it and press ESC\n");

    did_print_help = true;
  }

  cl_editor_entity_t *edit = cgi.client->editor_entities;
  for (int32_t i = 0; i < MAX_ENTITIES; i++, edit++) {

    if (!edit->def) {
      continue;
    }

    bool in_frame = false;
    for (int32_t j = 0; j < frame->num_entities; j++) {
      const uint32_t snum = (frame->entity_state + j) & ENTITY_STATE_MASK;
      const entity_state_t *s = &cgi.client->entity_states[snum];
      if (s->number == edit->number) {
        in_frame = true;
        break;
      }
    }

    if (!in_frame) {
      continue;
    }

    vec4_t color = Color32_Vec4(edit->ent->current.color);

    const char *classname = cgi.EntityValue(edit->def, "classname")->string;
    if (!g_strcmp0(classname, "light")) {

      r_light_t light = { 0 };

      light.origin = cgi.EntityValue(edit->def, "origin")->vec3;
      light.radius = cgi.EntityValue(edit->def, "radius")->value;
      light.color = cgi.EntityValue(edit->def, "color")->vec3;
      light.intensity = cgi.EntityValue(edit->def, "intensity")->value;

      const char *style = cgi.EntityValue(edit->def, "style")->nullable_string;
      const char *team = cgi.EntityValue(edit->def, "team")->nullable_string;

      if (team) {
        const int32_t master = Cg_FindTeamMaster(classname, team);
        if (master != -1) {
          const cm_entity_t *e = cgi.client->entity_definitions[master];
          light.radius = light.radius ?: cgi.EntityValue(e, "radius")->value;
          light.color = Vec3_Equal(Vec3_Zero(), light.color) ? cgi.EntityValue(e, "color")->vec3 : light.color;
          light.intensity = light.intensity ?: cgi.EntityValue(e, "intensity")->value;
          if (!style) {
            style = cgi.EntityValue(e, "style")->nullable_string;
          }
        }
      }

      light.radius = light.radius ?: 300.f;
      light.color = Vec3_Equal(Vec3_Zero(), light.color) ? Vec3(1.f, 1.f, 1.f) : light.color;
      light.intensity = light.intensity ?: 1.f;
      light.bounds = Box3_FromCenterRadius(light.origin, light.radius);

      if (style && *style) {
        const size_t len = strlen(style);
        const uint32_t style_index = (cgi.client->unclamped_time / 100) % len;
        const uint32_t style_time = (cgi.client->unclamped_time / 100) * 100;
        const float lerp = (cgi.client->unclamped_time - style_time) / 100.f;
        const float s = (style[(style_index + 0) % len] - 'a') / (float) ('z' - 'a');
        const float t = (style[(style_index + 1) % len] - 'a') / (float) ('z' - 'a');
        light.intensity *= Clampf(Mixf(s, t, lerp), FLT_EPSILON, 1.f);
      }

      light.shadow_cached = &edit->shadow_cached;

      cgi.AddLight(cgi.view, &light);

      color = Vec3_ToVec4(light.color, 1.f);
    }

    const char *mod = cgi.EntityValue(edit->def, "model")->string;
    r_model_t *model = strlen(mod) ? cgi.LoadModel(mod) : NULL;

    cgi.AddEntity(cgi.view, &(const r_entity_t) {
      .id = edit,
      .origin = edit->ent->origin,
      .angles = edit->ent->angles,
      .scale = cgi.EntityValue(edit->def, "scale")->value ?: 1.f,
      .bounds = edit->ent->bounds,
      .abs_bounds = edit->ent->abs_bounds,
      .color = color,
      .effects = edit->ent->current.effects,
      .model = model
    });
  }

  // think client-side entities so misc_sound, misc_flame, etc. are live in editor mode
  cg_entity_t *e = (cg_entity_t *) cg_entities->data;
  for (guint i = 0; i < cg_entities->len; i++, e++) {

    if (e->next_think > cgi.client->unclamped_time) {
      continue;
    }

    e->clazz->Think(e);

    if (e->hz) {
      e->next_think += 1000.f / e->hz + 1000.f * e->drift * Randomf();
    }
  }
}

/**
 * @brief Pushes or pops the editor view controller based on the current editor cvar state.
 */
void Cg_CheckEditor(void) {

  if (*cgi.state != CL_ACTIVE) {
    return;
  }

  if (editor->value) {
    if (cgi.TopViewController() != cg_editorViewController) {
      cgi.PushViewController(cg_editorViewController);
    }
  } else {
    if (cgi.TopViewController() == cg_editorViewController) {
      cgi.PopViewController();
    }
  }
}

/**
 * @brief Initializes the in-game editor.
 */
void Cg_InitEditor(void) {
  cg_editorViewController = (ViewController *) alloc(EditorViewController);
  cg_editorViewController = (ViewController *) $(cg_editorViewController, init);
}

/**
 * @brief Shuts down the in-game editor.
 */
void Cg_ShutdownEditor(void) {
  release(cg_editorViewController);
  cg_editorViewController = NULL;
}
