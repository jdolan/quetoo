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

#include "cm_local.h"

static const cm_entity_t null_entity;

/**
 * @brief
 */
cm_entity_t *Cm_AllocEntity(void) {
  return Mem_TagMalloc(sizeof(cm_entity_t), MEM_TAG_COLLISION);
}

/**
 * @brief
 */
void Cm_FreeEntity(cm_entity_t *entity) {

  cm_entity_t *e = entity, *next;

  while (e) {
    next = e->next;
    Mem_Free(e);
    e = next;
  }
}

/**
 * @brief
 */
cm_entity_t *Cm_CopyEntity(const cm_entity_t *entity) {

  cm_entity_t *copy = NULL;
  for (const cm_entity_t *in = entity; in; in = in->next) {

    cm_entity_t *out = Cm_AllocEntity();

    g_strlcpy(out->key, in->key, sizeof(out->key));
    g_strlcpy(out->string, in->string, sizeof(out->string));

    Cm_ParseEntity(out);

    if (in->brushes) {
      out->brushes = Mem_TagCopyString(in->brushes, MEM_TAG_COLLISION);
    }

    out->next = copy;
    copy = out;
  }

  return Cm_SortEntity(copy);
}

/**
 * @brief
 */
void Cm_ParseEntity(cm_entity_t *pair) {

  assert(pair);
  assert(pair->string);

  if (strlen(pair->string)) {
    pair->parsed |= ENTITY_STRING;
    pair->nullable_string = pair->string;
  }

  if (Parse_QuickPrimitive(pair->string,
                           PARSER_NO_COMMENTS,
                           PARSE_DEFAULT,
                           PARSE_INT32,
                           &pair->integer, 1) == 1) {
    pair->parsed |= ENTITY_INTEGER;
  }

  const size_t count = Parse_QuickPrimitive(pair->string,
                                            PARSER_NO_COMMENTS,
                                            PARSE_DEFAULT,
                                            PARSE_FLOAT,
                                            &pair->vec4, 4);

  switch (count) {
    case 1:
      pair->parsed |= ENTITY_FLOAT;
      break;
    case 2:
      pair->parsed |= ENTITY_VEC2;
      break;
    case 3:
      pair->parsed |= ENTITY_VEC3;
      break;
    case 4:
      pair->parsed |= ENTITY_VEC4;
      break;
  }

  if ((pair->parsed & ENTITY_VEC3) && !(pair->parsed & ENTITY_VEC4)) {
    pair->vec4.w = 1.f;
  }
}

/**
 * @brief GCompareFunc for entity sorting.
 * @details Classname comes first, followed by the rest in lexigraphical order.
 */
static gint Cm_SortEntity_cmp(gconstpointer a, gconstpointer b) {

  const cm_entity_t *m = a;
  const cm_entity_t *n = b;

  if (!g_strcmp0(m->key, "classname")) {
    return INT_MIN;
  }

  if (!g_strcmp0(n->key, "classname")) {
    return INT_MAX;
  }

  return g_strcmp0(m->key, n->key);
}

/**
 * @brief
 */
cm_entity_t *Cm_SortEntity(cm_entity_t *entity) {

  assert(entity);
  assert(entity != &null_entity);

  GPtrArray *pairs = g_ptr_array_new();

  for (cm_entity_t *e = entity; e; e = e->next) {
    g_ptr_array_add(pairs, e);
  }

  g_ptr_array_sort_values(pairs, Cm_SortEntity_cmp);

  cm_entity_t *classname = NULL;

  // now rebuild the linked list

  for (guint i = 0; i < pairs->len; i++) {

    cm_entity_t *e = g_ptr_array_index(pairs, i);

    if (i == 0) {
      classname = e;
      classname->prev = NULL;
    } else {
      e->prev = g_ptr_array_index(pairs, i - 1);
    }

    if (i < pairs->len - 1) {
      e->next = g_ptr_array_index(pairs, i + 1);
    } else {
      e->next = NULL;
    }
  }

  g_ptr_array_free(pairs, false);

  return classname;
}

/**
 * @brief Loads the BSP entity string lump.
 */
GList *Cm_LoadEntities(const char *entity_string) {

  GList *entities = NULL;

  parser_t parser = Parse_Init(entity_string, PARSER_NO_COMMENTS);

  while (true) {

    char token[MAX_BSP_ENTITY_VALUE];

    if (!Parse_Token(&parser, PARSE_DEFAULT, token, sizeof(token))) {
      break;
    }

    if (!g_strcmp0("{", token)) {

      cm_entity_t *entity = NULL;

      while (true) {

        cm_entity_t *pair = Cm_AllocEntity();

        Parse_Token(&parser, PARSE_DEFAULT, pair->key, sizeof(pair->key));
        Parse_Token(&parser, PARSE_DEFAULT, pair->string, sizeof(pair->string));

        Cm_ParseEntity(pair);

        pair->next = entity;
        if (entity) {
          entity->prev = pair;
        }
        entity = pair;

        Parse_PeekToken(&parser, PARSE_DEFAULT, token, sizeof(token));

        if (!g_strcmp0("}", token)) {
          break;
        }
      }

      entity = Cm_SortEntity(entity);

      assert(entity);

      entities = g_list_prepend(entities, entity);
    }
  }

  return g_list_reverse(entities);
}

/**
 * @brief
 */
int32_t Cm_EntityNumber(const cm_entity_t *entity) {

  for (int32_t i = 0; i < Cm_Bsp()->num_entities; i++) {
    if (Cm_Bsp()->entities[i] == entity) {
      return i;
    }
  }

  return -1;
}

/**
 * @brief
 */
const cm_entity_t *Cm_EntityValue(const cm_entity_t *entity, const char *key) {

  for (const cm_entity_t *e = entity; e; e = e->next) {
    if (!g_strcmp0(e->key, key)) {
      return e;
    }
  }

  return &null_entity;
}

/**
 * @brief Sets the specified key-value pair in the given entity.
 * @details If the key exists, it is modified. If it does not exist, it is added.
 * @param entity The head of the entity linked list.
 * @param key The key to set.
 * @param field How the value should be parsed.
 * @param value The value string.
 * @return The modified key-value pair.
 */
cm_entity_t *Cm_EntitySetKeyValue(cm_entity_t *entity, const char *key, cm_entity_parsed_t field, const void *value) {

  assert(entity != &null_entity);

  cm_entity_t *e;
  cm_entity_t *target = NULL;
  for (e = entity; e; e = e->next) {
    if (!g_strcmp0(e->key, key)) {
      target = e;
      break;
    }
  }

  if (target == NULL) {
    target = Cm_AllocEntity();
    if (entity) {
      for (e = entity; e->next; e = e->next) ;
      e->next = target;
      target->prev = e;
    }
  }

  g_strlcpy(target->key, key, sizeof(target->key));

  switch (field) {
    case ENTITY_STRING:
      g_strlcpy(target->string, (const char *) value, sizeof(entity->string));
      break;
    case ENTITY_INTEGER:
      g_snprintf(target->string, sizeof(entity->string), "%d", *(int32_t *) value);
      break;
    case ENTITY_FLOAT:
      g_snprintf(target->string, sizeof(entity->string), "%g", *(float *) value);
      break;
    case ENTITY_VEC2: {
      const vec2_t v = *(vec2_t *) value;
      g_snprintf(target->string, sizeof(entity->string), "%g %g", v.x, v.y);
      break;
    }
    case ENTITY_VEC3: {
      const vec3_t v = *(vec3_t *) value;
      g_snprintf(target->string, sizeof(entity->string), "%g %g %g", v.x, v.y, v.z);
      break;
    }
    case ENTITY_VEC4: {
      const vec4_t v = *(vec4_t *) value;
      g_snprintf(target->string, sizeof(entity->string), "%g %g %g %g", v.x, v.y, v.z, v.w);
      break;
    }
  }

  Cm_ParseEntity(target);
  return target;
}

/**
 * @brief
 */
GPtrArray *Cm_EntityBrushes(const cm_entity_t *entity) {

  GPtrArray *brushes = g_ptr_array_new();

  cm_bsp_brush_t *brush = Cm_Bsp()->brushes;
  for (int32_t i = 0; i < Cm_Bsp()->num_brushes; i++, brush++) {

    if (brush->entity == entity) {
      g_ptr_array_add(brushes, brush);
    }
  }

  return brushes;
}

/**
 * @brief Serializes a `cm_entity_t` to an info string.
 */
char *Cm_EntityToInfoString(const cm_entity_t *entity) {
  char *str = Mem_TagMalloc(MAX_INFO_STRING_STRING, MEM_TAG_COLLISION);

  for (const cm_entity_t *e = entity; e; e = e->next) {
    InfoString_Set(str, e->key, e->string);
  }

  return str;
}

/**
 * @brief Deserializes an info string to a `cm_entity_t`.
 */
cm_entity_t *Cm_EntityFromInfoString(const char *str) {

  if (InfoString_Validate(str)) {

    cm_entity_t *entity = NULL;
    const char *s = str;

    do {
      cm_entity_t *pair = Cm_AllocEntity();

      s = InfoString_Next(s, pair->key, pair->string);

      Cm_ParseEntity(pair);

      pair->next = entity;
      if (entity) {
        entity->prev = pair;
      }
      entity = pair;

    } while (s);

    return Cm_SortEntity(entity);
  }

  Com_Debug(DEBUG_COLLISION, "Invalid entity info string: %s\n", str);
  return NULL;
}
