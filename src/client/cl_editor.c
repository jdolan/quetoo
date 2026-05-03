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

/**
 * @brief Parses an editor entity update from the server and notifies the UI.
 */
void Cl_ParseEditorEntity(int16_t number, const char *info) {

  Cm_FreeEntity(cl.entity_definitions[number]);

  if (strlen(info)) {
    cl.entity_definitions[number] = Cm_EntityFromInfoString(info);
  } else {
    cl.entity_definitions[number] = NULL;
  }

  cl_editor_entity_t *edit = &cl.editor_entities[number];
  memset(edit, 0, sizeof(*edit));
  edit->number = number;
  edit->ent = &cl.entities[number];
  edit->def = cl.entity_definitions[number];

  for (int32_t i = 0; i < MAX_ENTITIES; i++) {
    cl.editor_entities[i].shadow_cached = false;
  }

  SDL_PushEvent(&(SDL_Event) {
    .user.type = MVC_NOTIFICATION_EVENT,
    .user.code = NOTIFICATION_ENTITY_PARSED,
    .user.data1 = (void *) (intptr_t) edit->number
  });
}
