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

#include "sv_local.h"

/**
 * @brief Spawn an editor entity; a non-functional bounding box.
 */
void Sv_SpawnEditorEntity(int32_t number, cm_entity_t *def) {

  g_entity_t *ent = svs.game->entities[number];

  ent->def = Cm_CopyEntity(def);
  ent->in_use = true;
  ent->classname = Cm_EntityValue(ent->def, "classname")->string;
  ent->bounds = Box3_FromCenterRadius(Vec3_Zero(), 8.f);

  ent->s.number = number;
  ent->s.origin = Cm_EntityValue(ent->def, "origin")->vec3;
  ent->s.angles = Cm_EntityValue(ent->def, "angles")->vec3;
  ent->s.color = Color32i(0xffffffff);

  if (g_str_has_prefix(ent->classname, "info_player")) {
    ent->bounds = Box3(Vec3(-16.f, -16.f, -24.f), Vec3(16.f, 16.f, 36.f));
    ent->s.color = Color32i(0xffff00ff);
  } else if (g_str_has_prefix(ent->classname, "light")) {
    ent->bounds = Box3_FromCenterRadius(Vec3_Zero(), 4.f);
  } else if (g_str_has_prefix(ent->classname, "trigger_")) {
    ent->s.color = Color32i(0xff0088ff);
  } else if (g_str_has_prefix(ent->classname, "func_")) {
    ent->s.color = Color32i(0xff00ff00);
  } else if (g_str_has_prefix(ent->classname, "misc_")) {
    ent->s.color = Color32i(0xff00ffff);
  } else if (g_str_has_prefix(ent->classname, "item_")) {
    ent->s.color = Color32i(0xffffff00);
  }

  // use the BSP inline model to set bounds
  const char *model = Cm_EntityValue(ent->def, "model")->nullable_string;
  if (model && *model == '*') {
    const cm_bsp_model_t *mod = Cm_Model(model);
    ent->bounds = mod->bounds;
  }

  if (number > 0) {
    ent->solid = SOLID_EDITOR;
    Sv_LinkEntity(ent);
  }

  char *info = Cm_EntityToInfoString(ent->def);

  Sv_SetConfigString(CS_ENTITIES + number, info);

  Mem_Free(info);
}

/**
 * @brief
 */
void Sv_EditEditorEntity(int32_t number, const char *info) {

  cm_entity_t *def = Cm_EntityFromInfoString(info);

  if (number > -1) {
    g_entity_t *entity = svs.game->entities[number];
    cm_entity_t *ent = (cm_entity_t *) entity->def;
    def->brushes = ent->brushes;
    ent->brushes = NULL;
    Cm_FreeEntity(ent);
  } else {
    for (int32_t i = Cm_Bsp()->num_entities; i < sv_max_entities->integer; i++) {
      if (svs.game->entities[i]->in_use == false) {
        number = i;
        break;
      }
    }

    if (number == -1) {
      Com_Warn("No free entity slots available\n");
      Cm_FreeEntity(def);
      return;
    }
  }

  Sv_SpawnEditorEntity(number, def);
}

/**
 * @brief
 */
void Sv_FreeEditorEntity(int32_t number) {

  g_entity_t *entity = svs.game->entities[number];

  Sv_UnlinkEntity(entity);

  memset(entity, 0, sizeof(*entity));

  Sv_SetConfigString(CS_ENTITIES + number, "");
}

/**
 * @brief Resolves the map brushes for each BSP entity.
 */
void Sv_LoadEditorMap(void) {
  char path[MAX_QPATH];
  StripExtension(Cm_Bsp()->name, path);
  g_strlcat(path, ".map", sizeof(path));

  void *buffer;
  if (Fs_Load(path, &buffer) == -1) {
    Com_Warn("Could not load %s\n", path);
    Com_Warn("You will not be able to edit inline model entities\n");
    return;
  }

  parser_t parser = Parse_Init(buffer, PARSER_DEFAULT);

  for (int32_t i = 0; i < Cm_Bsp()->num_entities; i++) {
    cm_entity_t *e = Cm_Bsp()->entities[i];

    const char *brushes = NULL;
    bool in_entity = false;
    bool in_brush = false;
    char token[MAX_TOKEN_CHARS] = "";

    while (Parse_Token(&parser, PARSE_DEFAULT, token, sizeof(token))) {

      if (!g_strcmp0(token, "{")) {
        if (!in_entity) {
          in_entity = true;
        } else if (!in_brush) {
          in_brush = true;
          if (!brushes) {
            brushes = parser.position.ptr - 1;
          }
        }
      }

      if (!g_strcmp0(token, "}")) {
        if (in_brush) {
          in_brush = false;
        } else if (in_entity) {
          in_entity = false;
          break;
        }
      }
    }

    if (brushes) {
      const size_t len = parser.position.ptr - brushes - 1;
      e->brushes = Mem_TagMalloc(len + strlen("// brush 0\n") + 1, MEM_TAG_COLLISION);
      strcpy(e->brushes, "// brush 0\n");
      memcpy(e->brushes + strlen(e->brushes), brushes, len);
    }
  }

  Fs_Free(buffer);
}

/**
 * @brief
 */
void Sv_SaveEditorMap_f(void) {

  if (svs.state != SV_ACTIVE_GAME) {
    Com_Warn("Server is not initialized\n");
    return;
  }

  if (!editor->value) {
    Com_Warn("Editor is not active.\n");
    return;
  }

  char path[MAX_QPATH];
  StripExtension(Cm_Bsp()->name, path);
  g_strlcat(path, ".map", sizeof(path));

  file_t *file = Fs_OpenWrite(path);
  if (!file) {
    Com_Warn("Failed to save %s\n", path);
    return;
  }

  int32_t entity_num = 0;
  for (int32_t i = 0; i < MAX_ENTITIES; i++) {

    const g_entity_t *ent = svs.game->entities[i];
    if (!ent->in_use) {
      continue;
    }

    if (!ent->def) {
      continue;
    }

    Fs_Print(file, "// entity %d\n", entity_num++);
    Fs_Print(file, "{\n");

    for (const cm_entity_t *e = ent->def; e; e = e->next) {
      Fs_Print(file, "\"%s\" \"%s\"\n", e->key, e->string);
    }

    const char *brushes = ent->def->brushes ?: "";
    Fs_Write(file, brushes, sizeof(char), strlen(brushes));

    Fs_Print(file, "}\n");
  }

  Fs_Close(file);

  Com_Print("Wrote %d entities to %s\n", entity_num, path);
}
