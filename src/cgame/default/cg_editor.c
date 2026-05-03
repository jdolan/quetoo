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

/**
 * @brief Editor entity array, indexed by entity number.
 * @details Each slot owns its definition, vtable state, and shadow cache flag.
 */
cg_editor_entity_t cg_edit[MAX_ENTITIES];

/**
 * @brief Finds the `team_master` entity for the given classname and team.
 */
int32_t Cg_FindTeamMaster(const char *classname, const char *team) {

  if (!team) {
    return -1;
  }

  for (int32_t i = 0; i < MAX_ENTITIES; i++) {
    const cm_entity_t *e = cg_edit[i].def;
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
 * @brief Adds a dynamic light for the given editor light entity.
 * @return The resolved light color, for use in the selection overlay.
 */
static vec4_t Cg_AddEditorEntity_Light(cg_editor_entity_t *edit) {

  r_light_t light = { 0 };

  light.origin = cgi.EntityValue(edit->def, "origin")->vec3;
  light.radius = cgi.EntityValue(edit->def, "radius")->value;
  light.color = cgi.EntityValue(edit->def, "color")->vec3;
  light.intensity = cgi.EntityValue(edit->def, "intensity")->value;

  const char *style = cgi.EntityValue(edit->def, "style")->nullable_string;
  const char *team = cgi.EntityValue(edit->def, "team")->nullable_string;

  if (team) {
    const int32_t master = Cg_FindTeamMaster("light", team);
    if (master != -1) {
      const cm_entity_t *e = cg_edit[master].def;
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

  return Vec3_ToVec4(light.color, 1.f);
}


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

  cg_editor_entity_t *edit = cg_edit;
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

    const char *mod = cgi.EntityValue(edit->def, "model")->string;
    r_model_t *model = strlen(mod) ? cgi.LoadModel(mod) : NULL;

    const char *classname = cgi.EntityValue(edit->def, "classname")->string;
    if (!g_strcmp0(classname, "light")) {
      color = Cg_AddEditorEntity_Light(edit);
    } else {

      // check for a client-side entity like misc_flame

      cg_entity_t *e = &cg_edit[i].entity;
      if (e->clazz) {
        if (e->next_think <= cgi.client->unclamped_time) {
          e->clazz->Think(e);
          if (e->hz) {
            e->next_think += 1000.f / e->hz + 1000.f * e->drift * Randomf();
          }
        }
      }
    }

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

  Cg_AddSprites();
}

/**
 * @brief Initializes or re-initializes the vtable entity slot for the given number.
 */
static void Cg_InitEditorEntity(int16_t number) {

  cg_editor_entity_t *edit = &cg_edit[number];
  cg_entity_t *e = &edit->entity;

  if (!edit->def) {
    if (e->clazz) {
      cgi.Free(e->data);
    }
    memset(e, 0, sizeof(*e));
    return;
  }

  const cm_entity_t *def = edit->def;
  const char *classname = cgi.EntityValue(def, "classname")->string;

  const cg_entity_class_t *clazz = NULL;
  for (size_t j = 0; j < cg_num_entity_classes; j++) {
    if (!g_strcmp0(classname, cg_entity_classes[j]->classname)) {
      clazz = cg_entity_classes[j];
      break;
    }
  }

  if (!clazz) {
    if (e->clazz) {
      cgi.Free(e->data);
    }
    memset(e, 0, sizeof(*e));
    return;
  }

  if (e->clazz != clazz) {
    cgi.Free(e->data);
    e->data = cgi.Malloc(clazz->data_size, MEM_TAG_CGAME_LEVEL);
  } else if (clazz->data_size) {
    memset(e->data, 0, clazz->data_size);
  }

  e->id = number;
  e->clazz = clazz;
  e->def = def;
  e->origin = cgi.EntityValue(def, "origin")->vec3;
  e->bounds = Box3_FromCenter(e->origin);
  e->target = NULL;
  e->team = NULL;
  e->next_think = 0;

  e->clazz->Init(e);
}

/**
 * @brief Called by the client when an entity configstring is received.
 * @details Parses the entity definition, populates the cg_edit slot, and (if active)
 *   initializes the vtable entity. Also fires NOTIFICATION_ENTITY_PARSED for the editor UI.
 */
void Cg_ParseEditorEntity(int16_t number, const char *info) {

  cg_editor_entity_t *edit = &cg_edit[number];

  if (edit->entity.clazz && edit->entity.data) {
    cgi.Free(edit->entity.data);
  }

  cgi.FreeEntity(edit->def);

  memset(edit, 0, sizeof(*edit));
  edit->number = number;
  edit->ent = &cgi.client->entities[number];
  edit->def = strlen(info) ? cgi.EntityFromInfoString(info) : NULL;

  for (int32_t i = 0; i < MAX_ENTITIES; i++) {
    cg_edit[i].shadow_cached = false;
  }

  if (*cgi.state == CL_ACTIVE) {
    Cg_InitEditorEntity(number);
  }

  SDL_PushEvent(&(SDL_Event) {
    .user = {
      .type = NOTIFICATION_ENTITY_PARSED,
      .data1 = (void *) (ptrdiff_t) number
    }
  });
}

/**
 * @brief Initializes all client-side editor entity slots from the current config strings.
 */
void Cg_LoadEditorEntities(void) {

  if (!editor->integer) {
    return;
  }

  for (int32_t i = 0; i < MAX_ENTITIES; i++) {
    const char *info = cgi.client->config_strings[CS_ENTITIES + i];
    if (!*info) {
      continue;
    }

    cg_edit[i].number = i;
    cg_edit[i].ent = &cgi.client->entities[i];
    cg_edit[i].def = cgi.EntityFromInfoString(info);
    Cg_InitEditorEntity(i);
  }
}

/**
 * @brief Frees all client-side editor entity slots and resets the sparse arrays.
 */
void Cg_FreeEditorEntities(void) {

  for (int32_t i = 0; i < MAX_ENTITIES; i++) {
    cg_editor_entity_t *edit = &cg_edit[i];
    if (edit->entity.clazz && edit->entity.data) {
      cgi.Free(edit->entity.data);
    }
    cgi.FreeEntity(edit->def);
  }

  memset(cg_edit, 0, sizeof(cg_edit));
}

/**
 * @brief Pushes or pops the editor view controller based on the current editor cvar state.
 */
void Cg_CheckEditor(void) {

  if (*cgi.state != CL_ACTIVE) {
    return;
  }

  if (editor->value) {
    if (!instanceof(EditorViewController, cgi.TopViewController())) {
      Cg_LoadEditorEntities();
      ViewController *vc = (ViewController *) alloc(EditorViewController);
      cgi.PushViewController($(vc, init));
      release(vc);
    }
  } else {
    if (instanceof(EditorViewController, cgi.TopViewController())) {
      cgi.PopViewController();
    }
  }
}
