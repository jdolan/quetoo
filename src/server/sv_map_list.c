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

  if (q_strcmp(svs.maps.filename, sv_map_list->string) ||
      (*sv_map_list->string && Fs_LastModTime(sv_map_list->string) != svs.maps.modtime)) {
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

  const ListNode *node = svs.maps.list->head;
  for (int32_t i = 0; i < svs.maps.index && node; i++) {
    node = node->next;
  }
  return node ? (const cm_entity_t *) node->element : NULL;
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

  q_strlcpy(svs.maps.filename, sv_map_list->string, sizeof(svs.maps.filename));

  svs.maps.modtime = Fs_LastModTime(sv_map_list->string);

  svs.maps.list = Cm_LoadEntities(buffer);

  List *valid = $(alloc(List), init);
  valid->destroy = (Consumer) Cm_FreeEntity;

  int32_t i = 0;
  for (const ListNode *node = svs.maps.list->head; node; node = node->next, i++) {
    cm_entity_t *props = (cm_entity_t *) node->element;

    const cm_entity_t *name = Cm_EntityValue(props, "name");
    if (q_strlen(name->string) == 0) {
      Com_Warn("Map list element %d in %s is missing \"name\"\n", i, sv_map_list->string);
      Cm_FreeEntity(props);
    } else {
      $(valid, append, props);
    }
  }

  release(svs.maps.list);
  svs.maps.list = valid;

  svs.maps.length = (int32_t) svs.maps.list->count;
  svs.maps.index = -1;

  Fs_Free(buffer);

  Com_Print("Parsed %d maps from %s\n", svs.maps.length, svs.maps.filename);
}

/**
 * @brief Frees all map list entries and clears the active map list.
 */
void Sv_ShutdownMapList(void) {

  svs.maps.list = release(svs.maps.list);
  svs.maps.length = 0;
  svs.maps.index = -1;
  svs.maps.modtime = 0;
  svs.maps.filename[0] = '\0';
}
