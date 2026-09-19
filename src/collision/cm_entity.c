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

static const CmEntity null_entity = { 0 };

/**
 * @brief Allocates and returns a new zeroed entity key-value pair.
 */
CmEntity *Cm_AllocEntity(void) {
  return Mem_TagMalloc(sizeof(CmEntity), MEM_TAG_COLLISION);
}

/**
 * @brief Frees the entity and all subsequent pairs in its linked list.
 */
void Cm_FreeEntity(CmEntity *entity) {

  CmEntity *e = entity, *next;

  while (e) {
    next = e->next;
    if (e->brushes) {
      Mem_Free(e->brushes);
    }
    Mem_Free(e);
    e = next;
  }
}

/**
 * @brief Returns a deep copy of the entity linked list.
 */
CmEntity *Cm_CopyEntity(const CmEntity *entity) {

  CmEntity *copy = NULL;
  for (const CmEntity *in = entity; in; in = in->next) {

    CmEntity *out = Cm_AllocEntity();

    q_strlcpy(out->key, in->key, sizeof(out->key));
    q_strlcpy(out->string, in->string, sizeof(out->string));

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
 * @brief Returns a new entity list with keys from src assigned into a copy of dst.
 * @details Keys already present in dst take priority; keys only in src are appended.
 *   Analogous to JavaScript's `Object.assign(dst, src)`.
 * @return A newly allocated entity list; the caller must free with `Cm_FreeEntity`.
 */
CmEntity *Cm_EntityAssign(const CmEntity *dst, const CmEntity *src) {

  CmEntity *out = Cm_CopyEntity(dst);

  for (const CmEntity *s = src; s; s = s->next) {

    if (!s->parsed) {
      continue;
    }

    if (Cm_EntityValue(out, s->key)->parsed) {
      continue;
    }

    CmEntity *pair = Cm_AllocEntity();

    q_strlcpy(pair->key, s->key, sizeof(pair->key));
    q_strlcpy(pair->string, s->string, sizeof(pair->string));

    Cm_ParseEntity(pair);

    pair->next = out;
    if (out) {
      out->prev = pair;
    }
    out = pair;
  }

  return out;
}

/**
 * @brief Parses the string field of an entity pair into its typed fields.
 */
void Cm_ParseEntity(CmEntity *pair) {

  assert(pair);
  assert(pair->string);

  if (q_strlen(pair->string)) {
    pair->parsed |= ENTITY_STRING;
    pair->nullableString = pair->string;
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
static Order Cm_SortEntity_cmp(const ident a, const ident b) {

  const CmEntity *m = *(const CmEntity *const *) a;
  const CmEntity *n = *(const CmEntity *const *) b;

  if (!q_strcmp(m->key, "classname")) {
    return OrderAscending;
  }

  if (!q_strcmp(n->key, "classname")) {
    return OrderDescending;
  }

  const int32_t cmp = q_strcmp(m->key, n->key);
  return cmp < 0 ? OrderAscending : cmp > 0 ? OrderDescending : OrderSame;
}

/**
 * @brief Sorts the entity key-value pairs, placing classname first.
 */
CmEntity *Cm_SortEntity(CmEntity *entity) {

  assert(entity);
  assert(entity != &null_entity);

  Vector *pairs = $(alloc(Vector), initWithSize, sizeof(CmEntity *));

  for (CmEntity *e = entity; e; e = e->next) {
    $(pairs, add, &e);
  }

  $(pairs, sort, Cm_SortEntity_cmp);

  CmEntity *classname = NULL;

  // now rebuild the linked list

  for (size_t i = 0; i < pairs->count; i++) {

    CmEntity *e = VectorValue(pairs, CmEntity *, i);

    if (i == 0) {
      classname = e;
      classname->prev = NULL;
    } else {
      e->prev = VectorValue(pairs, CmEntity *, i - 1);
    }

    if (i < pairs->count - 1) {
      e->next = VectorValue(pairs, CmEntity *, i + 1);
    } else {
      e->next = NULL;
    }
  }

  release(pairs);

  return classname;
}

/**
 * @brief Loads the BSP entity string lump.
 */
List *Cm_LoadEntities(const char *entityString) {

  List *entities = $(alloc(List), init);

  Parser parser = Parse_Init(entityString, PARSER_ALL_COMMENTS);

  while (true) {

    char token[MAX_BSP_ENTITY_VALUE];

    if (!Parse_Token(&parser, PARSE_DEFAULT, token, sizeof(token))) {
      break;
    }

    if (!q_strcmp("{", token)) {

      CmEntity *entity = NULL;

      while (true) {

        CmEntity *pair = Cm_AllocEntity();

        if (!Parse_Token(&parser, PARSE_DEFAULT, pair->key, sizeof(pair->key))) {
          Cm_FreeEntity(pair);
          break;
        }
        Parse_Token(&parser, PARSE_DEFAULT, pair->string, sizeof(pair->string));

        Cm_ParseEntity(pair);

        pair->next = entity;
        if (entity) {
          entity->prev = pair;
        }
        entity = pair;

        Parse_PeekToken(&parser, PARSE_DEFAULT, token, sizeof(token));

        if (!q_strcmp("}", token)) {
          break;
        }
      }

      entity = Cm_SortEntity(entity);

      assert(entity);

      $(entities, append, entity);
    }
  }

  return entities;
}

/**
 * @brief Returns the index of the entity in the loaded BSP entities array, or -1 if not found.
 */
int32_t Cm_EntityNumber(const CmEntity *entity) {

  for (int32_t i = 0; i < Cm_Bsp()->numEntities; i++) {
    if (Cm_Bsp()->entities[i] == entity) {
      return i;
    }
  }

  return -1;
}

/**
 * @brief Returns the entity pair matching key, or a null entity if not found.
 */
const CmEntity *Cm_EntityValue(const CmEntity *entity, const char *key) {

  for (const CmEntity *e = entity; e; e = e->next) {
    if (!q_strcmp(e->key, key)) {
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
CmEntity *Cm_EntitySetKeyValue(CmEntity *entity, const char *key, CmEntityParsed field, const void *value) {

  assert(entity != &null_entity);

  CmEntity *e;
  CmEntity *target = NULL;
  for (e = entity; e; e = e->next) {
    if (!q_strcmp(e->key, key)) {
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

  q_strlcpy(target->key, key, sizeof(target->key));

  switch (field) {
    case ENTITY_STRING:
      q_strlcpy(target->string, (const char *) value, sizeof(entity->string));
      break;
    case ENTITY_INTEGER:
      q_snprintf(target->string, sizeof(entity->string), "%d", *(int32_t *) value);
      break;
    case ENTITY_FLOAT:
      q_snprintf(target->string, sizeof(entity->string), "%g", *(float *) value);
      break;
    case ENTITY_VEC2: {
      const Vec2 v = *(Vec2 *) value;
      q_snprintf(target->string, sizeof(entity->string), "%g %g", v.x, v.y);
      break;
    }
    case ENTITY_VEC3: {
      const Vec3 v = *(Vec3 *) value;
      q_snprintf(target->string, sizeof(entity->string), "%g %g %g", v.x, v.y, v.z);
      break;
    }
    case ENTITY_VEC4: {
      const Vec4 v = *(Vec4 *) value;
      q_snprintf(target->string, sizeof(entity->string), "%g %g %g %g", v.x, v.y, v.z, v.w);
      break;
    }
  }

  Cm_ParseEntity(target);
  return target;
}

/**
 * @brief Returns a Vector of brushes belonging to the given entity.
 */
Vector *Cm_EntityBrushes(const CmEntity *entity) {

  Vector *brushes = $(alloc(Vector), initWithSize, sizeof(CmBspBrush *));

  CmBspBrush *brush = Cm_Bsp()->brushes;
  for (int32_t i = 0; i < Cm_Bsp()->numBrushes; i++, brush++) {

    if (brush->entity == entity) {
      $(brushes, add, &brush);
    }
  }

  return brushes;
}

/**
 * @brief Serializes a `CmEntity` to an info string.
 */
char *Cm_EntityToInfoString(const CmEntity *entity) {
  char *str = Mem_TagMalloc(MAX_INFO_STRING_STRING, MEM_TAG_COLLISION);

  for (const CmEntity *e = entity; e; e = e->next) {
    InfoString_Set(str, e->key, e->string);
  }

  return str;
}

/**
 * @brief Deserializes an info string to a `CmEntity`.
 */
CmEntity *Cm_EntityFromInfoString(const char *str) {

  if (InfoString_Validate(str)) {

    CmEntity *entity = NULL;
    const char *s = str;

    do {
      CmEntity *pair = Cm_AllocEntity();

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

/**
 * @brief Parses .map file text and assigns brush/patch text to entities.
 * @details Iterates sequentially through the .map text, capturing the raw brush
 * definitions (including patchDef2 blocks) for each entity. Entity ordering in
 * the map text must match the entities array.
 */
void Cm_ParseMapBrushes(const char *mapText, CmEntity **entities, int32_t numEntities) {

  Parser parser = Parse_Init(mapText, PARSER_DEFAULT);

  for (int32_t i = 0; i < numEntities; i++) {
    CmEntity *e = entities[i];

    const char *brushes = NULL;
    bool inEntity = false;
    int32_t brushDepth = 0;
    char token[MAX_TOKEN_CHARS] = "";

    while (Parse_Token(&parser, PARSE_DEFAULT | PARSE_ALLOW_OVERRUN, token, sizeof(token))) {

      if (!q_strcmp(token, "{")) {
        if (!inEntity) {
          inEntity = true;
        } else {
          brushDepth++;
          if (brushDepth == 1 && !brushes) {
            brushes = parser.position.ptr - 1;
          }
        }
      }

      if (!q_strcmp(token, "}")) {
        if (brushDepth > 0) {
          brushDepth--;
        } else if (inEntity) {
          inEntity = false;
          break;
        }
      }
    }

    if (brushes) {
      const size_t len = parser.position.ptr - brushes - 1;
      e->brushes = Mem_TagMalloc(len + q_strlen("// brush 0\n") + 1, MEM_TAG_COLLISION);
      strcpy(e->brushes, "// brush 0\n");
      memcpy(e->brushes + q_strlen(e->brushes), brushes, len);
    }
  }
}
