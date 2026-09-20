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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include "cg_local.h"

#include "bg_item.h"
#include "ui/editor/EditorViewController.h"

/**
 * @brief Global editor state.
 */
CGameEditor cgEditor = {
  .showFuncGroups = true,
  .selected = -1
};

/**
 * @brief Finds the `teamMaster` entity for the given classname and team.
 */
int32_t Cg_FindTeamMaster(const char *classname, const char *team) {

  if (!team) {
    return -1;
  }

  for (int32_t i = 0; i < MAX_ENTITIES; i++) {
    const CmEntity *e = cgEditor.entities[i].def;
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
static Vec4 Cg_AddEditorEntity_Light(CGameEditorEntity *edit) {

  RenderLight light = { 0 };

  light.origin = cgi.EntityValue(edit->def, "origin")->vec3;
  light.radius = cgi.EntityValue(edit->def, "radius")->value;
  light.color = cgi.EntityValue(edit->def, "color")->vec3;
  light.intensity = cgi.EntityValue(edit->def, "intensity")->value;
  float drift = cgi.EntityValue(edit->def, "drift")->value;

  const char *style = cgi.EntityValue(edit->def, "style")->nullableString;
  const char *team = cgi.EntityValue(edit->def, "team")->nullableString;

  if (team) {
    const int32_t master = Cg_FindTeamMaster("light", team);
    if (master != -1) {
      const CmEntity *e = cgEditor.entities[master].def;
      light.radius = light.radius ?: cgi.EntityValue(e, "radius")->value;
      light.color = Vec3_Equal(Vec3_Zero(), light.color) ? cgi.EntityValue(e, "color")->vec3 : light.color;
      light.intensity = light.intensity ?: cgi.EntityValue(e, "intensity")->value;
      drift = drift ?: cgi.EntityValue(e, "drift")->value;
      style = style ?: cgi.EntityValue(e, "style")->nullableString;
    }
  }

  light.radius = light.radius ?: 300.f;
  light.color = Vec3_Equal(Vec3_Zero(), light.color) ? MakeVec3(1.f, 1.f, 1.f) : light.color;
  light.intensity = light.intensity ?: 1.f;
  light.bounds = Box3_FromCenterRadius(light.origin, light.radius);
  light.intensity = Cg_AnimateLight(light.intensity, style, drift);

  cgi.AddLight(cgi.view, &light);

  return Vec3_ToVec4(light.color, 1.f);
}

/**
 * @brief Resolves the transform from world space to the entity's model space, in which
 * the BSP brushes of an entity with an origin are stored.
 */
static Mat4 Cg_EditorEntityInverseMatrix(const CGameEditorEntity *edit) {

  const ClientEntity *ent = edit->ent;
  const float scale = cgi.EntityValue(edit->def, "scale")->value ?: 1.f;

  return Mat4_Inverse(Mat4_FromRotationTranslationScale(ent->angles, ent->origin, scale));
}

/**
 * @brief Draws a brush wireframe in the entity's frame of reference.
 * @details BSP brushes of an entity with an origin are stored in model space, so they
 * must be transformed by the entity's matrix to be drawn where the entity is.
 */
static void Cg_DrawEditorBrush(const Box3 bounds, const Mat4 matrix, const Color color) {
  static const int32_t edges[] = {
    0, 1, 1, 3, 3, 2, 2, 0,
    4, 5, 5, 7, 7, 6, 6, 4,
    0, 4, 1, 5, 2, 6, 3, 7
  };

  Vec3 points[8];
  Box3_ToPoints(bounds, points);

  Vec3 lines[lengthof(edges)];
  for (size_t i = 0; i < lengthof(edges); i++) {
    lines[i] = Mat4_Transform(matrix, points[edges[i]]);
  }

  cgi.Draw3DLines(SDL_GPU_PRIMITIVETYPE_LINELIST, lines, lengthof(lines), color, true);
}

/**
 * @brief Populates the view and sound stage for the given editor frame.
 */
void Cg_PopulateEditorScene(const ClientFrame *frame) {
  static bool didPrintHelp = false;

  if (!didPrintHelp) {
    cgi.Print("^5In-game editor enabled\n");
    cgi.Print("^5To select an entity, place your crosshair over it and press ESC\n");
    cgi.Print("^5To cycle through the entities behind your crosshair, use the mouse wheel\n");
    cgi.Print("^5To move a selected entity, use W, A, S, D, E, C\n");
    cgi.Print("^5Use your normal hotkeys to cut, copy and paste entities\n");
    cgi.Print("^5To toggle func_group entities, use G\n");
    cgi.Print("^5To activate a selected mover, use U\n");
    cgi.Print("^5Movers are previewed with their real behavior; a func_train will\n");
    cgi.Print("^5relocate to its first path_corner, as it does in game\n");
    cgi.Print("^6To select a material, place your crosshair over it and press ESC\n");

    didPrintHelp = true;
  }

  CGameEditorEntity *edit = cgEditor.entities;
  for (int32_t i = 0; i < MAX_ENTITIES; i++, edit++) {

    if (!edit->def) {
      continue;
    }

    const char *classname = cgi.EntityValue(edit->def, "classname")->string;
    if (!q_strcmp(classname, "func_group") && !cgEditor.showFuncGroups) {
      continue;
    }

    const ClientEntity *ent = edit->ent;

    Vec4 debugColor = ent->current.color.rgba ? Color32_Vec4(ent->current.color) : color_white.vec4;
    Vec4 modelColor = color_white.vec4;

    if (!q_strcmp(classname, "light")) {
      modelColor = Cg_AddEditorEntity_Light(edit);
      debugColor = modelColor;
    } else {

      // check for a client-side entity like misc_flame

      CGameEntity *misc = &cgEditor.entities[i].misc;
      if (misc->clazz) {
        if (misc->nextThink <= cgi.client->unclampedTime) {
          misc->clazz->Think(misc);
          if (misc->hz) {
            misc->nextThink += 1000.f / misc->hz + 1000.f * misc->drift * Randomf();
          }
        }
      }
    }

    const bool isSelected = cgEditor.selected == edit->number;

    if (edit->brushes) {
      const RenderEntity *e = cgi.AddEntity(cgi.view, &(const RenderEntity) {
        .id = edit,
        .origin = ent->origin,
        .angles = ent->angles,
        .scale = cgi.EntityValue(edit->def, "scale")->value ?: 1.f,
        .bounds = Box3_Null(),
        .absBounds = Box3_Null(),
        .color = modelColor,
        .effects = ent->current.effects,
        .model = edit->model
      });

      if (isSelected || q_strcmp(classname, "worldspawn")) {
        const Color color = isSelected ? color_red : Color4fv(debugColor);
        for (uint32_t j = 0; j < edit->brushes->count; j++) {
          const CmBspBrush *brush = VectorValue(edit->brushes, CmBspBrush *, j);
          Cg_DrawEditorBrush(brush->bounds, e->matrix, color);
        }
      }

    } else {
      const RenderEntity *e = cgi.AddEntity(cgi.view, &(const RenderEntity) {
        .id = edit,
        .origin = ent->origin,
        .angles = ent->angles,
        .scale = cgi.EntityValue(edit->def, "scale")->value ?: 1.f,
        .bounds = ent->bounds,
        .absBounds = ent->absBounds,
        .color = modelColor,
        .effects = ent->current.effects,
        .model = edit->model
      });

      if (isSelected) {
        cgi.Draw3DBox(Box3_Expand(ent->absBounds, 2.f), color_red, true);

        if (edit->model && IS_MESH_MODEL(edit->model)) {
          const RenderMeshConfig *view = &edit->model->mesh->config.view;
          if (!Vec3_Equal(Vec3_Zero(), view->muzzle)) {
            const Vec3 muzzle = Mat4_Transform(e->matrix, view->muzzle);
            Cg_AddSprite(&(CGameSprite) {
              .animation = cgSpriteImpactSpark01,
              .origin = muzzle,
              .size = 30.f,
              .color = MakeVec3(1.f, .9f, .7f),
            });
          }
        }
      } else {
        cgi.Draw3DBox(Box3_Expand(ent->absBounds, 2.f), Color4fv(debugColor), true);
      }
    }

    if (isSelected && q_strcmp(classname, "worldspawn")) {
      Vec3 points[2] = { ent->origin };

      points[1] = Vec3_Fmaf(ent->origin, 64.f, MakeVec3(1.f, 0.f, 0.f));
      cgi.Draw3DLines(SDL_GPU_PRIMITIVETYPE_LINELIST, points, 2, color_red, true);

      points[1] = Vec3_Fmaf(ent->origin, 64.f, MakeVec3(0.f, 1.f, 0.f));
      cgi.Draw3DLines(SDL_GPU_PRIMITIVETYPE_LINELIST, points, 2, color_green, true);

      points[1] = Vec3_Fmaf(ent->origin, 64.f, MakeVec3(0.f, 0.f, 1.f));
      cgi.Draw3DLines(SDL_GPU_PRIMITIVETYPE_LINELIST, points, 2, color_blue, true);
    }
  }

  Cg_AddFlares();

  Cg_AddSprites();

  Cg_AddDynamicLights();
}

/**
 * @brief Initializes the `CGameEditorEntity` for the given entity number.
 * @details The slot must be zeroed before calling this function.
 */
static void Cg_InitEditorEntity(int16_t number) {

  CGameEditorEntity *edit = &cgEditor.entities[number];

  edit->number = number;
  edit->ent = &cgi.client->entities[number];

  const char *info = cgi.client->configStrings[CS_ENTITIES + number];
  edit->def = cgi.EntityFromInfoString(info);

  const char *mod = cgi.EntityValue(edit->def, "model")->string;
  if (q_strlen(mod)) {
    edit->model = cgi.LoadModel(mod);
  } else {
    const char *classname = cgi.EntityValue(edit->def, "classname")->string;
    for (size_t i = 0; i < bgNumItems; i++) {
      if (!q_strcmp(bgItemDefs[i].classname, classname)) {
        edit->model = cgi.LoadModel(bgItemDefs[i].model);
        break;
      }
    }
  }

  if (number < cgi.Bsp()->numEntities) {
    edit->brushes = cgi.EntityBrushes(cgi.Bsp()->entities[number]);
    if (!edit->brushes->count) {
      edit->brushes = release(edit->brushes);
    }
  }

  const char *classname = cgi.EntityValue(edit->def, "classname")->string;

  const CGameEntityClass *clazz = NULL;
  for (size_t j = 0; j < cgNumEntityClasses; j++) {
    if (!q_strcmp(classname, cgEntityClasses[j]->classname)) {
      clazz = cgEntityClasses[j];
      break;
    }
  }

  if (!clazz) {
    return;
  }

  CGameEntity *misc = &edit->misc;
  misc->id = number;
  misc->clazz = clazz;
  misc->def = edit->def;
  misc->origin = cgi.EntityValue(edit->def, "origin")->vec3;
  misc->bounds = Box3_FromCenter(misc->origin);
  if (clazz->dataSize) {
    misc->data = cgi.Malloc(clazz->dataSize, MEM_TAG_CGAME_LEVEL);
  }
  misc->clazz->Init(misc);
  misc->nextThink = cgi.client->unclampedTime;
}

/**
 * @brief Frees the numbered `cgEditor.entities` slot.
 */
static void Cg_FreeEditorEntity(int16_t number) {

  if (cgEditor.selected == number) {
    cgEditor.selected = -1;
  }

  CGameEditorEntity *edit = &cgEditor.entities[number];

  cgi.FreeEntity(edit->def);

  release(edit->brushes);

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
    const char *info = cgi.client->configStrings[CS_ENTITIES + i];
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

  memset(cgEditor.entities, 0, sizeof(cgEditor.entities));

  cgEditor.selected = -1;
}

/**
 * @brief Collects the editor entities intersected by the given ray for entity selection.
 * @details Each entity is traced against its brush list, falling back to a ray-AABB slab test
 *   for point entities that have no brushes. Worldspawn is skipped, as its brushes would occlude
 *   every other candidate.
 * @param out Receives the entity numbers of the intersected entities, nearest first.
 * @return The number of entity numbers written to `out`.
 */
size_t Cg_EntitySelectionCandidates(const Vec3 start, const Vec3 end, int16_t out[CG_EDITOR_MAX_CANDIDATES]) {

  float fractions[CG_EDITOR_MAX_CANDIDATES];
  size_t count = 0;

  CGameEditorEntity *edit = cgEditor.entities + 1;
  for (int32_t i = 1; i < MAX_ENTITIES; i++, edit++) {

    if (edit->def == NULL) {
      continue;
    }

    if (!cgEditor.showFuncGroups) {
      if (!q_strcmp(cgi.EntityValue(edit->def, "classname")->string, "func_group")) {
        continue;
      }
    }

    float fraction = 1.f;

    if (edit->brushes) {
      const Mat4 inverse = Cg_EditorEntityInverseMatrix(edit);

      const Vec3 modelStart = Mat4_Transform(inverse, start);
      const Vec3 modelEnd = Mat4_Transform(inverse, end);

      for (uint32_t j = 0; j < edit->brushes->count; j++) {
        const CmBspBrush *brush = VectorValue(edit->brushes, CmBspBrush *, j);

        const CmTrace tr = cgi.TraceToBrush(modelStart, modelEnd, brush);

        if (tr.startSolid || tr.fraction >= fraction) {
          continue;
        }

        fraction = tr.fraction;
      }
    } else {

      const EntityState *s = &edit->ent->current;
      const Box3 bounds = Box3_Translate(s->bounds, s->origin);

      fraction = Box3_RayFraction(start, end, bounds);
    }

    if (fraction >= 1.f) {
      continue;
    }

    if (count == CG_EDITOR_MAX_CANDIDATES && fraction >= fractions[count - 1]) {
      continue;
    }

    size_t j = count < CG_EDITOR_MAX_CANDIDATES ? count++ : CG_EDITOR_MAX_CANDIDATES - 1;
    while (j > 0 && fractions[j - 1] > fraction) {
      fractions[j] = fractions[j - 1];
      out[j] = out[j - 1];
      j--;
    }

    fractions[j] = fraction;
    out[j] = edit->number;
  }

  return count;
}

/**
 * @brief Traces the view ray for material selection.
 */
CGameEditorTrace Cg_MaterialSelectionTrace(const Vec3 start, const Vec3 end) {

  CGameEditorTrace out = {
    .ent = NULL,
    .trace = {
      .fraction = 1.f
    }
  };

  CGameEditorEntity *edit = cgEditor.entities;
  for (int32_t i = 0; i < MAX_ENTITIES; i++, edit++) {

    if (edit->def == NULL) {
      continue;
    }

    if (edit->brushes) {
      const Mat4 inverse = Cg_EditorEntityInverseMatrix(edit);

      const Vec3 modelStart = Mat4_Transform(inverse, start);
      const Vec3 modelEnd = Mat4_Transform(inverse, end);

      for (uint32_t j = 0; j < edit->brushes->count; j++) {
        const CmBspBrush *brush = VectorValue(edit->brushes, CmBspBrush *, j);

        const CmTrace tr = cgi.TraceToBrush(modelStart, modelEnd, brush);

        if (tr.startSolid || tr.fraction >= out.trace.fraction) {
          continue;
        }

        out.ent = edit;
        out.trace = tr;
        out.trace.end = Vec3_Mix(start, end, tr.fraction);
      }
    } else if (IS_MESH_MODEL(edit->model)) {

      const EntityState *s = &edit->ent->current;
      const Box3 bounds = Box3_Translate(s->bounds, s->origin);

      const float frac = Box3_RayFraction(start, end, bounds);
      if (frac >= out.trace.fraction) {
        continue;
      }

      const RenderMeshModel *mesh = edit->model->mesh;
      for (int32_t j = 0; j < mesh->numFaces; j++) {
        if (mesh->faces[j].material) {
          out.ent = edit;
          out.trace = (CmTrace) {
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
 * @brief Broadcasts a request to cycle the entity selection along the selection ray.
 * @details ObjectivelyMVC delivers mouse wheel events to the View beneath the cursor, so the
 *   EntityViewController never sees them; they reach it as a notification instead.
 * @param dir A positive value to select a nearer entity, negative for a farther one.
 */
void Cg_CycleEditorSelection(int32_t dir) {

  if (!editor->integer || cgi.GetKeyDest() != KEY_UI) {
    return;
  }

  SDL_PushEvent(&(SDL_Event) {
    .user.type = MVC_NOTIFICATION_EVENT,
    .user.code = NOTIFICATION_EDITOR_SELECTION_CYCLE,
    .user.data1 = (void *) (intptr_t) dir
  });
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
