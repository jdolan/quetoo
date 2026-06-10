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
 * @brief Returns the next map from the configured list, or `NULL` if unavailable.
 */
const cm_entity_t *Sv_NextMap(void) {

  if (g_strcmp0(svs.maps.filename, sv_map_list->string)) {
    Sv_InitMapList();
  }

  if (svs.maps.list == NULL) {
    return NULL;
  }

  if (sv_map_list_shuffle->value && svs.maps.length > 1) {
    const int32_t index = svs.maps.index;
    do {
      svs.maps.index = (int32_t) RandomRangeu(0, (uint32_t) svs.maps.length);
    } while (svs.maps.index == index);
  } else {
    svs.maps.index = (svs.maps.index + 1) % svs.maps.length;
  }

  return g_list_nth_data(svs.maps.list, svs.maps.index);
}

/**
 * @brief Initializes the map list, parsing the configured file from disk.
 */
void Sv_InitMapList(void) {

  Sv_ShutdownMapList();

  if (*sv_map_list->string == '\0') {
    return;
  }

  char *buffer;
  if (Fs_Load(sv_map_list->string, (void **) &buffer) <= 0) {
    Com_Warn("Couldn't load %s\n", sv_map_list->string);
    return;
  }

  g_strlcpy(svs.maps.filename, sv_map_list->string, sizeof(svs.maps.filename));

  svs.maps.list = Cm_LoadEntities(buffer);

  int32_t i = 0;
  for (GList *next, *link = svs.maps.list; link; link = next, i++) {
    next = link->next;

    cm_entity_t *props = link->data;

    const cm_entity_t *name = Cm_EntityValue(props, "name");
    if (strlen(name->string) == 0) {
      Com_Warn("Map list element %d in %s is missing \"name\"\n", i, sv_map_list->string);
      Cm_FreeEntity(props);
      svs.maps.list = g_list_delete_link(svs.maps.list, link);
    }
  }

  svs.maps.length = g_list_length(svs.maps.list);
  svs.maps.index = -1;

  Mem_Free(buffer);

  Com_Print("Parsed %d maps from %s\n", svs.maps.length, svs.maps.filename);
}

/**
 * @brief Frees all map list entries and clears the active map list.
 */
void Sv_ShutdownMapList(void) {

  if (svs.maps.list) {
    g_list_free_full(svs.maps.list, (GDestroyNotify) Cm_FreeEntity);
  }

  svs.maps.list = NULL;
  svs.maps.length = 0;
  svs.maps.index = -1;
  svs.maps.filename[0] = '\0';
}
