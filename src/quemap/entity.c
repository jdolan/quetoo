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

#include "entity.h"

/**
 * @brief Sets or creates the key-value pair with the given key on the entity.
 */
void SetValueForKey(Entity *ent, const char *key, const char *value) {

  for (EntityKeyValue *e = ent->values; e; e = e->next) {
    if (!q_strcmp(e->key, key)) {
      q_strlcpy(e->value, value, sizeof(e->value));
      return;
    }
  }

  EntityKeyValue *e = Mem_TagMalloc(sizeof(*e), (MemTag) MEM_TAG_EPAIR);
  e->next = ent->values;
  ent->values = e;

  q_strlcpy(e->key, key, sizeof(e->key));
  q_strlcpy(e->value, value, sizeof(e->value));
}

/**
 * @brief Returns the value for the given key on the entity, or def if not found.
 */
const char *ValueForKey(const Entity *ent, const char *key, const char *def) {

  for (const EntityKeyValue *e = ent->values; e; e = e->next) {
    if (!q_strcmp(e->key, key)) {
      return e->value;
    }
  }

  return def;
}

/**
 * @brief Returns the `Vec3` value for the given key on the entity, or def if not found or not parseable.
 */
Vec3 VectorForKey(const Entity *ent, const char *key, const Vec3 def) {

  const char *value = ValueForKey(ent, key, NULL);
  if (value) {
    Vec3 out;
    if (Parse_QuickPrimitive(value, PARSER_NO_COMMENTS, PARSE_DEFAULT, PARSE_FLOAT, &out, 3) == 3) {
      return out;
    }
  }

  return def;
}
