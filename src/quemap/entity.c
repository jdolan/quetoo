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

#include "entity.h"

/**
 * @brief Allocates a key-value pair, with copies of the key and the value that are freed with it.
 */
MapEntityKeyValue *AllocKeyValue(const char *key, const char *value) {

  MapEntityKeyValue *e = Mem_TagMalloc(sizeof(*e), (MemTag) MEM_TAG_EPAIR);

  e->key = Mem_Link(Mem_TagCopyString(key, (MemTag) MEM_TAG_EPAIR), e);
  e->value = Mem_Link(Mem_TagCopyString(value, (MemTag) MEM_TAG_EPAIR), e);

  return e;
}

/**
 * @brief Sets or creates the key-value pair with the given key on the entity.
 */
void SetValueForKey(MapEntity *ent, const char *key, const char *value) {

  for (MapEntityKeyValue *e = ent->values; e; e = e->next) {
    if (!Str_Compare(e->key, key)) {
      char *copy = Mem_Link(Mem_TagCopyString(value, (MemTag) MEM_TAG_EPAIR), e);
      Mem_Free(e->value);
      e->value = copy;
      return;
    }
  }

  MapEntityKeyValue *e = AllocKeyValue(key, value);
  e->next = ent->values;
  ent->values = e;
}

/**
 * @brief Returns the value for the given key on the entity, or def if not found.
 */
const char *ValueForKey(const MapEntity *ent, const char *key, const char *def) {

  for (const MapEntityKeyValue *e = ent->values; e; e = e->next) {
    if (!Str_Compare(e->key, key)) {
      return e->value;
    }
  }

  return def;
}

/**
 * @brief Returns the `Vec3` value for the given key on the entity, or def if not found or not parseable.
 */
Vec3 VectorForKey(const MapEntity *ent, const char *key, const Vec3 def) {

  const char *value = ValueForKey(ent, key, NULL);
  if (value) {
    Vec3 out;
    if (Parse_QuickPrimitive(value, PARSER_NO_COMMENTS, PARSE_DEFAULT, PARSE_FLOAT, &out, 3) == 3) {
      return out;
    }
  }

  return def;
}
