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

#include "r_local.h"

RenderModels rModels;

/**
 * @brief Loads the model by the specified name.
 */
RenderModel *R_LoadModel(const char *name) {
  char key[MAX_QPATH];

  if (!name || !name[0]) {
    Com_Error(ERROR_DROP, "R_LoadModel: NULL name\n");
  }

  if (*name == '*') {
    q_snprintf(key, sizeof(key), "%s#%s", rModels.world->media.name, name + 1);
  } else {
    StripExtension(name, key);
  }

  RenderModel *mod = (RenderModel *) R_FindMedia(key, R_MEDIA_MODEL);
  if (mod == NULL) {

    const RenderModelFormat formats[] = {
      rObjModelFormat,
      rMd3ModelFormat,
      rBspModelFormat
    };

    const RenderModelFormat *format = formats;
    char path[MAX_QPATH];

    size_t i;
    for (i = 0; i < lengthof(formats); i++, format++) {

      q_snprintf(path, sizeof(path), "%s.%s", key, format->extension);

      if (Fs_Exists(path)) {
        break;
      }
    }

    if (i == lengthof(formats)) {
      static HashTable *warned;
      if (!warned) {
        warned = $(alloc(HashTable), init, HashTableHashStr, HashTableEqualStr);
        warned->destroyKey = free;
      }
      if ($(warned, get, (void *) key) == NULL) {
        char *warnedKey = q_strdup(key);
        $(warned, set, warnedKey, warnedKey);
        if (q_strstr(name, "players/")) {
          Com_Debug(DEBUG_RENDERER, "Failed to load player %s\n", name);
        } else {
          Com_Warn("Failed to load %s\n", name);
        }
      }
      return NULL;
    }

    mod = (RenderModel *) R_AllocMedia(key, sizeof(RenderModel), R_MEDIA_MODEL);

    mod->media.Register = format->Register;
    mod->media.Free = format->Free;

    mod->type = format->type;

    mod->bounds = Box3_Null();

    void *buf = NULL;

    Fs_Load(path, &buf);

    format->Load(mod, buf);

    Fs_Free(buf);

    mod->radius = Box3_Radius(mod->bounds);

    R_RegisterMedia((RenderMedia *) mod);
  }

  return mod;
}

/**
 * @brief Returns the currently loaded world model (BSP).
 */
RenderModel *R_WorldModel(void) {
  return rModels.world;
}

/**
 * @brief Initializes the model facilities.
 */
void R_InitModels(void) {

  memset(&rModels, 0, sizeof(rModels));

  R_InitMd3Normals();

  R_InitMeshPipeline();
}

/**
 * @brief Shuts down the model facilities.
 */
void R_ShutdownModels(void) {

  memset(&rModels, 0, sizeof(rModels));

  R_ShutdownMeshPipeline();
}
