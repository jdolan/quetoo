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

#include "cm_local.h"

static const Entity nullEntity = { .key = "", .string = "" };

/**
 * @brief Allocates and returns a new zeroed entity key-value pair.
 */
Entity *Entity_Alloc(void) {

  Entity *pair = Mem_TagMalloc(sizeof(Entity), MEM_TAG_COLLISION);

  pair->key = Mem_Link(Mem_TagCopyString("", MEM_TAG_COLLISION), pair);
  pair->string = Mem_Link(Mem_TagCopyString("", MEM_TAG_COLLISION), pair);

  return pair;
}

/**
 * @brief Replaces one of the pair's strings with a copy of `value`, freed with the pair.
 */
static void Entity_SetPair_string(Entity *pair, char **field, const char *value) {

  char *copy = Mem_Link(Mem_TagCopyString(value ?: "", MEM_TAG_COLLISION), pair);

  if (*field) {
    Mem_Free(*field);
  }

  *field = copy;
}

/**
 * @brief Sets the key and the value of a single entity pair, and parses the value.
 */
Entity *Entity_SetPair(Entity *pair, const char *key, const char *string) {

  assert(pair);
  assert(pair != &nullEntity);

  if (key != pair->key) {
    Entity_SetPair_string(pair, &pair->key, key);
  }

  if (string != pair->string) {
    Entity_SetPair_string(pair, &pair->string, string);
  }

  Entity_Parse(pair);

  return pair;
}

/**
 * @brief Frees the entity and all subsequent pairs in its linked list.
 */
void Entity_Free(Entity *entity) {

  Entity *e = entity, *next;

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
Entity *Entity_Copy(const Entity *entity) {

  Entity *copy = NULL;
  for (const Entity *in = entity; in; in = in->next) {

    Entity *out = Entity_SetPair(Entity_Alloc(), in->key, in->string);

    if (in->brushes) {
      out->brushes = Mem_TagCopyString(in->brushes, MEM_TAG_COLLISION);
    }

    out->next = copy;
    copy = out;
  }

  return Entity_Sort(copy);
}

/**
 * @brief Returns a new entity list with keys from src assigned into a copy of dst.
 * @details Keys already present in dst take priority; keys only in src are appended.
 *   Analogous to JavaScript's `Object.assign(dst, src)`.
 * @return A newly allocated entity list; the caller must free with `Entity_Free`.
 */
Entity *Entity_Assign(const Entity *dst, const Entity *src) {

  Entity *out = Entity_Copy(dst);

  for (const Entity *s = src; s; s = s->next) {

    if (!s->parsed) {
      continue;
    }

    if (Entity_Value(out, s->key)->parsed) {
      continue;
    }

    Entity *pair = Entity_SetPair(Entity_Alloc(), s->key, s->string);

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
void Entity_Parse(Entity *pair) {

  assert(pair);
  assert(pair->string);

  pair->parsed = 0;
  pair->nullableString = NULL;

  if (Str_Length(pair->string)) {
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
static Order Entity_Sort_cmp(const ident a, const ident b) {

  const Entity *m = *(const Entity *const *) a;
  const Entity *n = *(const Entity *const *) b;

  if (!Str_Compare(m->key, "classname")) {
    return OrderAscending;
  }

  if (!Str_Compare(n->key, "classname")) {
    return OrderDescending;
  }

  const int32_t cmp = Str_Compare(m->key, n->key);
  return cmp < 0 ? OrderAscending : cmp > 0 ? OrderDescending : OrderSame;
}

/**
 * @brief Sorts the entity key-value pairs, placing classname first.
 */
Entity *Entity_Sort(Entity *entity) {

  assert(entity);
  assert(entity != &nullEntity);

  Vector *pairs = $(alloc(Vector), initWithSize, sizeof(Entity *));

  for (Entity *e = entity; e; e = e->next) {
    $(pairs, add, &e);
  }

  $(pairs, sort, Entity_Sort_cmp);

  Entity *classname = NULL;

  // now rebuild the linked list

  for (size_t i = 0; i < pairs->count; i++) {

    Entity *e = VectorValue(pairs, Entity *, i);

    if (i == 0) {
      classname = e;
      classname->prev = NULL;
    } else {
      e->prev = VectorValue(pairs, Entity *, i - 1);
    }

    if (i < pairs->count - 1) {
      e->next = VectorValue(pairs, Entity *, i + 1);
    } else {
      e->next = NULL;
    }
  }

  release(pairs);

  return classname;
}

/**
 * @brief The longest key or value that `Entity_LoadAll` reads. The whole entity lump of a BSP is
 * smaller than this, so in practice no key or value is truncated.
 */
#define ENTITY_TOKEN_SIZE MAX_BSP_ENTITIES_SIZE

/**
 * @brief Parses the next token into `token`, reading to the token's end even when it is longer
 * than `ENTITY_TOKEN_SIZE`, so that parsing resumes after it. A token that does not fit is
 * truncated, with a warning that names the entity and the key.
 * @return False at the end of the input.
 */
static bool Entity_LoadAll_token(Parser *parser, char *token, int32_t number, const char *key) {

  if (!Parse_Token(parser, PARSE_DEFAULT | PARSE_ALLOW_OVERRUN, token, ENTITY_TOKEN_SIZE)) {
    return false;
  }

  const size_t length = strlen(token);
  if (length == ENTITY_TOKEN_SIZE - 1) {
    if (key) {
      Com_Warn("Entity %d: value of \"%s\" is %zu or more characters, truncated\n",
               number, key, length);
    } else {
      Com_Warn("Entity %d: a key is %zu or more characters, truncated\n", number, length);
    }
  }

  return true;
}

/**
 * @brief Loads the BSP entity string lump.
 */
List *Entity_LoadAll(const char *entityString) {

  List *entities = $(alloc(List), init);

  char *key = Mem_TagMalloc(ENTITY_TOKEN_SIZE, MEM_TAG_COLLISION);
  char *value = Mem_TagMalloc(ENTITY_TOKEN_SIZE, MEM_TAG_COLLISION);

  Parser parser = Parse_Init(entityString, PARSER_ALL_COMMENTS);

  while (true) {

    char token[MAX_BSP_ENTITY_VALUE];

    if (!Parse_Token(&parser, PARSE_DEFAULT | PARSE_ALLOW_OVERRUN, token, sizeof(token))) {
      break;
    }

    if (!Str_Compare("{", token)) {

      Entity *entity = NULL;

      while (true) {

        Entity *pair = Entity_Alloc();

        const int32_t number = (int32_t) entities->count;

        if (!Entity_LoadAll_token(&parser, key, number, NULL)) {
          Entity_Free(pair);
          break;
        }

        if (!Entity_LoadAll_token(&parser, value, number, key)) {
          *value = '\0';
        }

        Entity_SetPair(pair, key, value);

        pair->next = entity;
        if (entity) {
          entity->prev = pair;
        }
        entity = pair;

        Parse_PeekToken(&parser, PARSE_DEFAULT | PARSE_ALLOW_OVERRUN, token, sizeof(token));

        if (!Str_Compare("}", token)) {
          break;
        }
      }

      entity = Entity_Sort(entity);

      assert(entity);

      $(entities, append, entity);
    }
  }

  Mem_Free(key);
  Mem_Free(value);

  return entities;
}

/**
 * @brief Returns the index of the entity in the loaded BSP entities array, or -1 if not found.
 */
int32_t Entity_Number(const Entity *entity) {

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
const Entity *Entity_Value(const Entity *entity, const char *key) {

  for (const Entity *e = entity; e; e = e->next) {
    if (!Str_Compare(e->key, key)) {
      return e;
    }
  }

  return &nullEntity;
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
Entity *Entity_SetKeyValue(Entity *entity, const char *key, EntityParsed field, const void *value) {

  assert(entity != &nullEntity);

  Entity *e;
  Entity *target = NULL;
  for (e = entity; e; e = e->next) {
    if (!Str_Compare(e->key, key)) {
      target = e;
      break;
    }
  }

  if (target == NULL) {
    target = Entity_Alloc();
    if (entity) {
      for (e = entity; e->next; e = e->next) ;
      e->next = target;
      target->prev = e;
    }
  }

  const char *string = "";
  char number[MAX_STRING_CHARS];

  switch (field) {
    case ENTITY_STRING:
      string = (const char *) value;
      break;
    case ENTITY_INTEGER:
      Str_Format(number, sizeof(number), "%d", *(int32_t *) value);
      string = number;
      break;
    case ENTITY_FLOAT:
      Str_Format(number, sizeof(number), "%g", *(float *) value);
      string = number;
      break;
    case ENTITY_VEC2: {
      const Vec2 v = *(Vec2 *) value;
      Str_Format(number, sizeof(number), "%g %g", v.x, v.y);
      string = number;
      break;
    }
    case ENTITY_VEC3: {
      const Vec3 v = *(Vec3 *) value;
      Str_Format(number, sizeof(number), "%g %g %g", v.x, v.y, v.z);
      string = number;
      break;
    }
    case ENTITY_VEC4: {
      const Vec4 v = *(Vec4 *) value;
      Str_Format(number, sizeof(number), "%g %g %g %g", v.x, v.y, v.z, v.w);
      string = number;
      break;
    }
  }

  return Entity_SetPair(target, key, string);
}

/**
 * @brief Returns a Vector of brushes belonging to the given entity.
 */
Vector *Entity_Brushes(const Entity *entity) {

  Vector *brushes = $(alloc(Vector), initWithSize, sizeof(CollisionBrush *));

  CollisionBrush *brush = Cm_Bsp()->brushes;
  for (int32_t i = 0; i < Cm_Bsp()->numBrushes; i++, brush++) {

    if (brush->entity == entity) {
      $(brushes, add, &brush);
    }
  }

  return brushes;
}

/**
 * @brief Serializes a `Entity` to an info string.
 */
char *Entity_ToInfoString(const Entity *entity) {
  char *str = Mem_TagMalloc(MAX_INFO_STRING_STRING, MEM_TAG_COLLISION);

  for (const Entity *e = entity; e; e = e->next) {
    if (!InfoString_Set(str, e->key, e->string)) {
      Com_Warn("\"%s\" is too long for an info string, and is left out\n", e->key);
    }
  }

  return str;
}

/**
 * @brief Deserializes an info string to a `Entity`.
 */
Entity *Entity_FromInfoString(const char *str) {

  if (InfoString_Validate(str)) {

    Entity *entity = NULL;
    const char *s = str;

    do {
      char key[MAX_INFO_STRING_KEY], value[MAX_INFO_STRING_VALUE];

      s = InfoString_Next(s, key, value);

      Entity *pair = Entity_SetPair(Entity_Alloc(), key, value);

      pair->next = entity;
      if (entity) {
        entity->prev = pair;
      }
      entity = pair;

    } while (s);

    return Entity_Sort(entity);
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
void Entity_ParseBrushes(const char *mapText, Entity **entities, int32_t numEntities) {

  Parser parser = Parse_Init(mapText, PARSER_DEFAULT);

  for (int32_t i = 0; i < numEntities; i++) {
    Entity *e = entities[i];

    const char *brushes = NULL;
    bool inEntity = false;
    int32_t brushDepth = 0;
    char token[MAX_TOKEN_CHARS] = "";

    while (Parse_Token(&parser, PARSE_DEFAULT | PARSE_ALLOW_OVERRUN, token, sizeof(token))) {

      if (!Str_Compare(token, "{")) {
        if (!inEntity) {
          inEntity = true;
        } else {
          brushDepth++;
          if (brushDepth == 1 && !brushes) {
            brushes = parser.position.ptr - 1;
          }
        }
      }

      if (!Str_Compare(token, "}")) {
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
      e->brushes = Mem_TagMalloc(len + Str_Length("// brush 0\n") + 1, MEM_TAG_COLLISION);
      strcpy(e->brushes, "// brush 0\n");
      memcpy(e->brushes + Str_Length(e->brushes), brushes, len);
    }
  }
}
