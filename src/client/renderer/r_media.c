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

#include "r_local.h"

typedef struct {
  HashTable *media;
  int32_t seed; // for tracking stale assets
} r_media_state_t;

static r_media_state_t r_media_state;

static Order R_EnumerateMedia_comparator(const ident a, const ident b) {
  const int32_t cmp = q_strcmp((*(const r_media_t *const *) a)->name, (*(const r_media_t *const *) b)->name);
  return cmp < 0 ? OrderAscending : cmp > 0 ? OrderDescending : OrderSame;
}

typedef struct {
  Vector *media;
} R_EnumerateMedia_ctx_t;

static void R_EnumerateMedia_collect(const HashTable *table, ident key, ident value, ident data) {
  R_EnumerateMedia_ctx_t *ctx = data;
  r_media_t *media = value;
  $(ctx->media, addElement, &media);
}

/**
 * @brief Enumerates all media in key-alphabetical order.
 */
void R_EnumerateMedia(R_MediaEnumerator enumerator, void *data) {
  R_EnumerateMedia_ctx_t ctx = {
    .media = $(alloc(Vector), initWithSize, sizeof(r_media_t *)),
  };

  $(r_media_state.media, enumerate, R_EnumerateMedia_collect, &ctx);
  $(ctx.media, sort, R_EnumerateMedia_comparator);

  for (size_t i = 0; i < ctx.media->count; i++) {
    const r_media_t *media = VectorValue(ctx.media, r_media_t *, i);
    if (enumerator) {
      enumerator(media, data);
    }
  }

  release(ctx.media);
}

/**
 * @brief `R_MediaEnumerator` for `R_ListMedia_f`.
 */
static void R_ListMedia_enumerator(const r_media_t *media, void *data) {
  Com_Print("%s\n", media->name);
}

/**
 * @brief Prints information about all currently loaded media to the console.
 */
void R_ListMedia_f(void) {
  R_EnumerateMedia(R_ListMedia_enumerator, NULL);
}

/**
 * @brief Establishes a dependency from the specified dependent to the given
 * dependency. Dependencies in use by registered media are never freed.
 */
r_media_t *R_RegisterDependency(r_media_t *dependent, r_media_t *dependency) {

  assert(dependent);
  assert(dependency);

  if (dependent->dependencies == NULL) {
    dependent->dependencies = $(alloc(List), init);
  }

  if ($(dependent->dependencies, nodeForElement, dependency) == NULL) {
    $(dependent->dependencies, prependElement, dependency);
  }

  return R_RegisterMedia(dependency);
}

/**
 * @brief Inserts the specified media into the shared table, re-registering all
 * of its dependencies as well.
 */
r_media_t *R_RegisterMedia(r_media_t *media) {

  assert(media);

  if (media->seed != r_media_state.seed) {
    r_media_t *other = $(r_media_state.media, get, media);

    if (other) {
      if (other != media) {
        R_FreeMedia(other);
        $(r_media_state.media, set, media, media);
      }
    } else {
      $(r_media_state.media, set, media, media);
    }

    media->seed = r_media_state.seed;
  }

  if (media->Register) {
    media->Register(media);
  }

  for (const ListNode *node = media->dependencies ? media->dependencies->head : NULL; node; node = node->next) {
    R_RegisterMedia((r_media_t *) node->element);
  }

  return media;
}

/**
 * @return The named media if it is already known, re-registering it for convenience.
 */
r_media_t *R_FindMedia(const char *name, r_media_type_t type) {

  r_media_t lookup = {
    .type = type
  };
  
  q_strlcpy(lookup.name, name, sizeof(lookup.name));

  r_media_t *media = $(r_media_state.media, get, &lookup);
  if (media) {
    R_RegisterMedia(media);
  }

  return media;
}

/**
 * @brief Returns a newly allocated `r_media_t` with the specified name.
 * @param size The number of bytes to allocate for the media.
 * @return The newly initialized media.
 */
r_media_t *R_AllocMedia(const char *name, size_t size, r_media_type_t type) {

  if (!name || !*name) {
    Com_Error(ERROR_DROP, "NULL name\n");
  }

  r_media_t *media = Mem_TagMalloc(size, MEM_TAG_RENDERER);

  q_strlcpy(media->name, name, sizeof(media->name));
  media->type = type;

  return media;
}

/**
 * @brief Frees media resources. If data is non-`NULL`, then the media is
 * always freed. Otherwise, only media with stale seed values and no explicit
 * retainment are freed.
 */
static bool R_FreeMedia_(r_media_t *media, void *data) {

  if (!data) {
    if (media->seed == r_media_state.seed) {
      return false;
    }

    if (media->Retain && media->Retain(media)) {
      return false;
    }
  }

  if (media->Free) {
    media->Free(media);
  }

  media->dependencies = release(media->dependencies);

  return true;
}

typedef struct {
  Vector *media;
  void *data;
} R_FreeMedia_ctx_t;

static void R_FreeMedia_collect(const HashTable *table, ident key, ident value, ident data) {
  R_FreeMedia_ctx_t *ctx = data;
  r_media_t *media = value;

  if (R_FreeMedia_(media, ctx->data)) {
    $(ctx->media, addElement, &media);
  }
}

static void R_FreeMediaEntries(void *data) {
  R_FreeMedia_ctx_t ctx = {
    .media = $(alloc(Vector), initWithSize, sizeof(r_media_t *)),
    .data = data,
  };

  $(r_media_state.media, enumerate, R_FreeMedia_collect, &ctx);

  for (size_t i = 0; i < ctx.media->count; i++) {
    r_media_t *media = VectorValue(ctx.media, r_media_t *, i);
    $(r_media_state.media, remove, media);
  }

  release(ctx.media);
}

/**
 * @brief Frees the specified media immediately.
 */
void R_FreeMedia(r_media_t *media) {

  R_FreeMedia_(media, (void *) 1);

  $(r_media_state.media, remove, media);
}

/**
 * @brief Prepares the media subsystem for loading.
 */
void R_BeginLoading(void) {
  int32_t s;

  do {
    s = Randomi();
  } while (s == r_media_state.seed);

  r_media_state.seed = s;
}

/**
 * @brief Frees any media that has a stale seed and is not explicitly retained.
 */
void R_EndLoading(void) {
  R_FreeMediaEntries(NULL);
}

/**
 * @brief Computes the hash value for a media entry by name and type.
 */
static size_t R_MediaHash(const void * key) {
  const r_media_t *media = key;
  uint32_t hash = 5381;

  for (const char *c = media->name; *c; c++) {
    hash = (hash * 33u) ^ (uint8_t) *c;
  }

  return hash + media->type;
}

/**
 * @brief Tests whether two media entries are equal by type and name.
 */
static bool R_MediaEqual(const void * a, const void * b) {
  const r_media_t *_a = a, *_b = b;

  if (_a->type == _b->type) {
    return !q_strcmp(_a->name, _b->name);
  }

  return false;
}

/**
 * @brief Initializes the media pool.
 */
void R_InitMedia(void) {

  memset(&r_media_state, 0, sizeof(r_media_state));

  r_media_state.media = $(alloc(HashTable), init, (HashTableHashFunc) R_MediaHash, (HashTableEqualFunc) R_MediaEqual);
  r_media_state.media->destroyValue = Mem_Free;

  R_BeginLoading();
}

/**
 * @brief Shuts down the media pool.
 */
void R_ShutdownMedia(void) {

  R_FreeMediaEntries((void *) 1);

  r_media_state.media = release(r_media_state.media);
}
