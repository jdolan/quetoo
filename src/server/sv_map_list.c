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
 * @brief Re-parses the map list if the configured file has changed since it was loaded.
 * @remarks The filename is compared even when `sv_map_list` is empty, so that clearing
 * it drops the rotation rather than leaving the last one loaded.
 */
static void Sv_RefreshMapList(void) {

  if (q_strcmp(svs.maps.filename, sv_map_list->string) ||
      (*sv_map_list->string && Fs_LastModTime(sv_map_list->string) != svs.maps.modtime)) {
    Sv_InitMapList();
  }
}

/**
 * @brief The rotation entry at `index`, or `NULL`.
 */
static const cm_entity_t *Sv_MapAt(int32_t index) {

  if (svs.maps.list == NULL || index < 0 || index >= svs.maps.length) {
    return NULL;
  }

  const ListNode *node = svs.maps.list->head;
  for (int32_t i = 0; i < index && node; i++) {
    node = node->next;
  }

  return node ? (const cm_entity_t *) node->element : NULL;
}

/**
 * @brief Returns a copy of the configured map list, or `NULL` if there is none.
 * @return A list of `cm_entity_t *`, each to be freed with `Cm_FreeEntity`.
 * @remarks The copy is the caller's, so that a `sv_map_list` edit which re-parses the
 * list underneath them does not free entries they still hold.
 */
List *Sv_MapList(void) {

  Sv_RefreshMapList();

  if (svs.maps.list == NULL) {
    return NULL;
  }

  List *copy = $(alloc(List), init);

  for (const ListNode *node = svs.maps.list->head; node; node = node->next) {
    $(copy, append, Cm_CopyEntity((const cm_entity_t *) node->element));
  }

  return copy;
}

/**
 * @brief Returns the rotation index the running level was served from, or `-1` if it
 * was not served from the rotation.
 * @remarks This is what identifies the level when a list names the same map twice,
 * which its name can not.
 */
int32_t Sv_MapIndex(void) {

  Sv_RefreshMapList();

  return svs.maps.current;
}

/**
 * @brief Chooses the rotation index the next `next_map` serves, in place of the
 * rotation's own pick.
 * @remarks An index rather than a name, so that a list naming the same map twice
 * serves, and resumes from, the occurrence that was actually chosen. The override is
 * consumed by that one map change, so that it can not survive to decide a later one.
 */
void Sv_SetNextMap(int32_t index) {

  if (index >= 0 && index < svs.maps.length) {
    svs.maps.next = index;
  } else {
    Com_Warn("Ignoring next map %d\n", index);
  }
}

/**
 * @brief Returns the next map from the configured list, or `NULL` if unavailable.
 */
const cm_entity_t *Sv_NextMap(void) {

  Sv_RefreshMapList();

  // consumed whether or not it can be served, so that it can not survive to decide
  // a later map change
  const int32_t next = svs.maps.next;
  svs.maps.next = -1;

  if (svs.maps.list == NULL) {
    return NULL;
  }

  if (next >= 0 && next < svs.maps.length) {
    svs.maps.index = next;
  } else if (sv_map_list_shuffle->value && svs.maps.length > 1) {
    const int32_t index = svs.maps.index;
    do {
      svs.maps.index = (int32_t) RandomRangeu(0, (uint32_t) svs.maps.length);
    } while (svs.maps.index == index);
  } else {
    svs.maps.index = (svs.maps.index + 1) % svs.maps.length;
  }

  svs.maps.current = svs.maps.index;

  return Sv_MapAt(svs.maps.index);
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
  svs.maps.current = -1;
  svs.maps.next = -1;

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
  svs.maps.current = -1;
  svs.maps.modtime = 0;
  svs.maps.filename[0] = '\0';
  svs.maps.next = -1;
}
