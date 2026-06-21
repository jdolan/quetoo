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

#include "game/default/bg_item.h"
#include "ui/editor/EditorViewController.h"

/**
 * @brief Global editor state.
 */
cg_editor_t cg_editor = {
  .show_func_groups = true,
  .selected = -1
};

/**
 * @brief Finds the `team_master` entity for the given classname and team.
 */
int32_t Cg_FindTeamMaster(const char *classname, const char *team) {

  if (!team) {
    return -1;
  }

  for (int32_t i = 0; i < MAX_ENTITIES; i++) {
    const cm_entity_t *e = cg_editor.entities[i].def;
    if (!e) {
      continue;
    }

    if (!q_strcmp(cgi.EntityValue(e, "classname")->string, classname)) {
      if (!q_strcmp(cgi.EntityValue(e, "team")->string, team)) {
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
  float drift = cgi.EntityValue(edit->def, "drift")->value;

  const char *style = cgi.EntityValue(edit->def, "style")->nullable_string;
  const char *team = cgi.EntityValue(edit->def, "team")->nullable_string;

  if (team) {
    const int32_t master = Cg_FindTeamMaster("light", team);
    if (master != -1) {
      const cm_entity_t *e = cg_editor.entities[master].def;
      light.radius = light.radius ?: cgi.EntityValue(e, "radius")->value;
      light.color = Vec3_Equal(Vec3_Zero(), light.color) ? cgi.EntityValue(e, "color")->vec3 : light.color;
      light.intensity = light.intensity ?: cgi.EntityValue(e, "intensity")->value;
      drift = drift ?: cgi.EntityValue(e, "drift")->value;
      style = style ?: cgi.EntityValue(e, "style")->nullable_string;
    }
  }

  light.radius = light.radius ?: 300.f;
  light.color = Vec3_Equal(Vec3_Zero(), light.color) ? Vec3(1.f, 1.f, 1.f) : light.color;
  light.intensity = light.intensity ?: 1.f;
  light.bounds = Box3_FromCenterRadius(light.origin, light.radius);
  light.intensity = Cg_AnimateLight(light.intensity, style, drift);
  light.shadow_cached = &edit->shadow_cached;

  cgi.AddLight(cgi.view, &light);

  return Vec3_ToVec4(light.color, 1.f);
}

/**
 * @brief Populates the view and sound stage for the given editor frame.
 */
void Cg_PopulateEditorScene(const cl_frame_t *frame) {
  static bool did_print_help = false;

  if (!did_print_help) {
    cgi.Print("^5In-game editor enabled\n");
    cgi.Print("^5To select an entity, place your crosshair over it and press ESC\n");
    cgi.Print("^5To move a selected entity, use W, A, S, D, E, C\n");
    cgi.Print("^5Use your normal hotkeys to cut, copy and paste entities\n");
    cgi.Print("^5To toggle func_group entities, use G\n");
    cgi.Print("^6To select a material, place your crosshair over it and press ESC\n");

    did_print_help = true;
  }

  cg_editor_entity_t *edit = cg_editor.entities;
  for (int32_t i = 0; i < MAX_ENTITIES; i++, edit++) {

    if (!edit->def) {
      continue;
    }

    const char *classname = cgi.EntityValue(edit->def, "classname")->string;
    if (!q_strcmp(classname, "func_group") && !cg_editor.show_func_groups) {
      continue;
    }

    const cl_entity_t *ent = edit->ent;

    vec4_t debug_color = ent->current.color.rgba ? Color32_Vec4(ent->current.color) : color_white.vec4;
    vec4_t model_color = color_white.vec4;

    if (!q_strcmp(classname, "light")) {
      model_color = Cg_AddEditorEntity_Light(edit);
      debug_color = model_color;
    } else {

      // check for a client-side entity like misc_flame

      cg_entity_t *misc = &cg_editor.entities[i].misc;
      if (misc->clazz) {
        if (misc->next_think <= cgi.client->unclamped_time) {
          misc->clazz->Think(misc);
          if (misc->hz) {
            misc->next_think += 1000.f / misc->hz + 1000.f * misc->drift * Randomf();
          }
        }
      }
    }

    const bool is_selected = cg_editor.selected == edit->number;

    if (edit->brushes) {
      cgi.AddEntity(cgi.view, &(const r_entity_t) {
        .id = edit,
        .origin = ent->origin,
        .angles = ent->angles,
        .scale = cgi.EntityValue(edit->def, "scale")->value ?: 1.f,
        .bounds = Box3_Null(),
        .abs_bounds = Box3_Null(),
        .color = model_color,
        .effects = ent->current.effects,
        .model = edit->model
      });

      if (is_selected) {
        for (uint32_t j = 0; j < edit->brushes->count; j++) {
          const cm_bsp_brush_t *brush = VectorValue(edit->brushes, cm_bsp_brush_t *, j);
          cgi.Draw3DBox(brush->bounds, color_red, true);
        }
      } else if (q_strcmp(classname, "worldspawn")) {
        for (uint32_t j = 0; j < edit->brushes->count; j++) {
          const cm_bsp_brush_t *brush = VectorValue(edit->brushes, cm_bsp_brush_t *, j);
          cgi.Draw3DBox(brush->bounds, Color4fv(debug_color), true);
        }
      }

    } else {
      const r_entity_t *e = cgi.AddEntity(cgi.view, &(const r_entity_t) {
        .id = edit,
        .origin = ent->origin,
        .angles = ent->angles,
        .scale = cgi.EntityValue(edit->def, "scale")->value ?: 1.f,
        .bounds = ent->bounds,
        .abs_bounds = ent->abs_bounds,
        .color = model_color,
        .effects = ent->current.effects,
        .model = edit->model
      });

      if (is_selected) {
        cgi.Draw3DBox(Box3_Expand(ent->abs_bounds, 2.f), color_red, true);

        if (edit->model && IS_MESH_MODEL(edit->model)) {
          const r_mesh_config_t *view = &edit->model->mesh->config.view;
          if (!Vec3_Equal(Vec3_Zero(), view->muzzle)) {
            const vec3_t muzzle = Mat4_Transform(e->matrix, view->muzzle);
            Cg_AddSprite(&(cg_sprite_t) {
              .animation = cg_sprite_impact_spark_01,
              .origin = muzzle,
              .size = 30.f,
              .color = Vec3(1.f, .9f, .7f),
            });
          }
        }
      } else {
        cgi.Draw3DBox(Box3_Expand(ent->abs_bounds, 2.f), Color4fv(debug_color), true);
      }
    }

    if (is_selected && q_strcmp(classname, "worldspawn")) {
      vec3_t points[2] = { ent->origin };

      points[1] = Vec3_Fmaf(ent->origin, 64.f, Vec3(1.f, 0.f, 0.f));
      cgi.Draw3DLines(GL_LINES, points, 2, color_red, true);

      points[1] = Vec3_Fmaf(ent->origin, 64.f, Vec3(0.f, 1.f, 0.f));
      cgi.Draw3DLines(GL_LINES, points, 2, color_green, true);

      points[1] = Vec3_Fmaf(ent->origin, 64.f, Vec3(0.f, 0.f, 1.f));
      cgi.Draw3DLines(GL_LINES, points, 2, color_blue, true);
    }
  }

  Cg_AddFlares();

  Cg_AddSprites();
}

/**
 * @brief Initializes the `cg_editor_entity_t` for the given entity number.
 * @details The slot must be zeroed before calling this function.
 */
static void Cg_InitEditorEntity(int16_t number) {

  cg_editor_entity_t *edit = &cg_editor.entities[number];

  edit->number = number;
  edit->ent = &cgi.client->entities[number];

  const char *info = cgi.client->config_strings[CS_ENTITIES + number];
  edit->def = cgi.EntityFromInfoString(info);

  const char *mod = cgi.EntityValue(edit->def, "model")->string;
  if (q_strlen(mod)) {
    edit->model = cgi.LoadModel(mod);
  } else {
    const char *classname = cgi.EntityValue(edit->def, "classname")->string;
    for (size_t i = 0; i < bg_num_items; i++) {
      if (!q_strcmp(bg_item_defs[i].classname, classname)) {
        edit->model = cgi.LoadModel(bg_item_defs[i].model);
        break;
      }
    }
  }

  if (number < cgi.Bsp()->num_entities) {
    edit->brushes = cgi.EntityBrushes(cgi.Bsp()->entities[number]);
    if (!edit->brushes->count) {
      release(edit->brushes);
      edit->brushes = NULL;
    }
  }

  const char *classname = cgi.EntityValue(edit->def, "classname")->string;

  const cg_entity_class_t *clazz = NULL;
  for (size_t j = 0; j < cg_num_entity_classes; j++) {
    if (!q_strcmp(classname, cg_entity_classes[j]->classname)) {
      clazz = cg_entity_classes[j];
      break;
    }
  }

  if (!clazz) {
    return;
  }

  cg_entity_t *misc = &edit->misc;
  misc->id = number;
  misc->clazz = clazz;
  misc->def = edit->def;
  misc->origin = cgi.EntityValue(edit->def, "origin")->vec3;
  misc->bounds = Box3_FromCenter(misc->origin);
  if (clazz->data_size) {
    misc->data = cgi.Malloc(clazz->data_size, MEM_TAG_CGAME_LEVEL);
  }
  misc->clazz->Init(misc);
  misc->next_think = cgi.client->unclamped_time;
}

/**
 * @brief Frees the numbered `cg_editor.entities` slot.
 */
static void Cg_FreeEditorEntity(int16_t number) {

  if (cg_editor.selected == number) {
    cg_editor.selected = -1;
  }

  cg_editor_entity_t *edit = &cg_editor.entities[number];

  cgi.FreeEntity(edit->def);

  if (edit->brushes) {
    release(edit->brushes);
  }

  if (edit->misc.clazz && edit->misc.data) {
    if (edit->misc.clazz->Free) {
      edit->misc.clazz->Free(&edit->misc);
    }
    cgi.Free(edit->misc.data);
  }

  memset(edit, 0, sizeof(*edit));
}

/**
 * @brief Called by the client when an entity configstring is received.
 */
void Cg_ParseEditorEntity(int16_t number, const char *info) {

  Cg_FreeEditorEntity(number);

  for (int32_t i = 0; i < MAX_ENTITIES; i++) {
    cg_editor.entities[i].shadow_cached = false;
  }

  if (*cgi.state == CL_ACTIVE && q_strlen(info)) {
    Cg_InitEditorEntity(number);
  }

  SDL_PushEvent(&(SDL_Event) {
    .user.type = MVC_NOTIFICATION_EVENT,
    .user.code = NOTIFICATION_ENTITY_PARSED,
    .user.data1 = (void *) (ptrdiff_t) number
  });
}

/**
 * @brief Initializes all client-side editor entity slots from the current config strings.
 */
void Cg_LoadEditorEntities(void) {

  if (!editor->integer) {
    return;
  }

  Cg_FreeEditorEntities();

  for (int32_t i = 0; i < MAX_ENTITIES; i++) {
    const char *info = cgi.client->config_strings[CS_ENTITIES + i];
    if (*info) {
      Cg_InitEditorEntity(i);
    }
  }
}

/**
 * @brief Frees all client-side editor entity slots and resets the sparse arrays.
 */
void Cg_FreeEditorEntities(void) {

  for (int32_t i = 0; i < MAX_ENTITIES; i++) {
    Cg_FreeEditorEntity(i);
  }

  memset(cg_editor.entities, 0, sizeof(cg_editor.entities));

  cg_editor.selected = -1;
}

/**
 * @brief Traces the view ray against each editor entity's brush list, falling back to a
 *   ray-AABB slab test for point entities that have no brushes.
 */
/**
 * @brief Traces the view ray for entity selection.
 * @details Skips worldspawn (it is the default fallback and has many brushes) and includes
 *   point entities via a ray-AABB slab test.
 */
cg_editor_trace_t Cg_EntitySelectionTrace(const vec3_t start, const vec3_t end) {

  cg_editor_trace_t out = {
    .ent = NULL,
    .trace = { .fraction = 1.f }
  };

  cg_editor_entity_t *edit = cg_editor.entities;
  for (int32_t i = 1; i < MAX_ENTITIES; i++, edit++) {

    if (edit->def == NULL) {
      continue;
    }

    if (!cg_editor.show_func_groups) {
      if (!q_strcmp(cgi.EntityValue(edit->def, "classname")->string, "func_group")) {
        continue;
      }
    }

    if (edit->brushes) {
      for (uint32_t j = 0; j < edit->brushes->count; j++) {
        const cm_bsp_brush_t *brush = VectorValue(edit->brushes, cm_bsp_brush_t *, j);

        const cm_trace_t tr = cgi.TraceToBrush(start, end, brush);

        if (tr.start_solid || tr.fraction >= out.trace.fraction) {
          continue;
        }

        out.ent = edit;
        out.trace = tr;
      }
    } else {

      const entity_state_t *s = &edit->ent->current;
      const box3_t bounds = Box3_Translate(s->bounds, s->origin);

      const float frac = Box3_RayFraction(start, end, bounds);
      if (frac >= out.trace.fraction) {
        continue;
      }

      out.ent = edit;
      out.trace = (cm_trace_t) {
        .fraction = frac,
        .end = Vec3_Mix(start, end, frac)
      };
    }
  }

  return out;
}

/**
 * @brief Traces the view ray for material selection.
 */
cg_editor_trace_t Cg_MaterialSelectionTrace(const vec3_t start, const vec3_t end) {

  cg_editor_trace_t out = {
    .ent = NULL,
    .trace = {
      .fraction = 1.f
    }
  };

  cg_editor_entity_t *edit = cg_editor.entities;
  for (int32_t i = 0; i < MAX_ENTITIES; i++, edit++) {

    if (edit->def == NULL) {
      continue;
    }

    if (edit->brushes) {
      for (uint32_t j = 0; j < edit->brushes->count; j++) {
        const cm_bsp_brush_t *brush = VectorValue(edit->brushes, cm_bsp_brush_t *, j);

        const cm_trace_t tr = cgi.TraceToBrush(start, end, brush);

        if (tr.start_solid || tr.fraction >= out.trace.fraction) {
          continue;
        }

        out.ent = edit;
        out.trace = tr;
      }
    } else if (IS_MESH_MODEL(edit->model)) {

      const entity_state_t *s = &edit->ent->current;
      const box3_t bounds = Box3_Translate(s->bounds, s->origin);

      const float frac = Box3_RayFraction(start, end, bounds);
      if (frac >= out.trace.fraction) {
        continue;
      }

      const r_mesh_model_t *mesh = edit->model->mesh;
      for (int32_t j = 0; j < mesh->num_faces; j++) {
        if (mesh->faces[j].material) {
          out.ent = edit;
          out.trace = (cm_trace_t) {
            .fraction = frac,
            .end = Vec3_Mix(start, end, frac),
            .material = mesh->faces[j].material->cm,
          };
          break;
        }
      }
    }
  }

  return out;
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
