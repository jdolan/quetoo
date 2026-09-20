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

#include "r_local.h"

typedef struct {
  HashTable *media;
  int32_t seed;
} RenderMediaState;

static RenderMediaState rMediaState;

static Order R_EnumerateMedia_comparator(const ident a, const ident b) {
  const int32_t cmp = q_strcmp((*(const RenderMedia *const *) a)->name, (*(const RenderMedia *const *) b)->name);
  return cmp < 0 ? OrderAscending : cmp > 0 ? OrderDescending : OrderSame;
}

typedef struct {
  Vector *media;
} REnumerateMediaCtx;

static void R_EnumerateMedia_collect(const HashTable *table, ident key, ident value, ident data) {
  REnumerateMediaCtx *ctx = data;
  RenderMedia *media = value;
  $(ctx->media, add, &media);
}

/**
 * @brief Enumerates media in key order.
 */
void R_EnumerateMedia(R_MediaEnumerator enumerator, void *data) {
  REnumerateMediaCtx ctx = {
    .media = $(alloc(Vector), initWithSize, sizeof(RenderMedia *)),
  };

  $(rMediaState.media, enumerate, R_EnumerateMedia_collect, &ctx);
  $(ctx.media, sort, R_EnumerateMedia_comparator);

  for (size_t i = 0; i < ctx.media->count; i++) {
    const RenderMedia *media = VectorValue(ctx.media, RenderMedia *, i);
    if (enumerator) {
      enumerator(media, data);
    }
  }

  release(ctx.media);
}

/**
 * @brief Prints one media name for `R_ListMedia_f`.
 */
static void R_ListMedia_enumerator(const RenderMedia *media, void *data) {
  Com_Print("%s\n", media->name);
}

/**
 * @brief Prints all loaded media names.
 */
void R_ListMedia_f(void) {
  R_EnumerateMedia(R_ListMedia_enumerator, NULL);
}

/**
 * @brief Registers a dependency for a media object.
 */
RenderMedia *R_RegisterDependency(RenderMedia *dependent, RenderMedia *dependency) {

  assert(dependent);
  assert(dependency);

  if (dependent->dependencies == NULL) {
    dependent->dependencies = $(alloc(List), init);
  }

  if ($(dependent->dependencies, nodeForElement, dependency) == NULL) {
    $(dependent->dependencies, prepend, dependency);
  }

  return R_RegisterMedia(dependency);
}

/**
 * @brief Registers a media object and its dependencies.
 */
RenderMedia *R_RegisterMedia(RenderMedia *media) {

  assert(media);

  if (media->seed != rMediaState.seed) {
    RenderMedia *other = $(rMediaState.media, get, media);

    if (other) {
      if (other != media) {
        R_FreeMedia(other);
        $(rMediaState.media, set, media, media);
      }
    } else {
      $(rMediaState.media, set, media, media);
    }

    media->seed = rMediaState.seed;
  }

  if (media->Register) {
    media->Register(media);
  }

  for (const ListNode *node = media->dependencies ? media->dependencies->head : NULL; node; node = node->next) {
    R_RegisterMedia((RenderMedia *) node->element);
  }

  return media;
}

/**
 * @brief Finds and re-registers a named media object.
 */
RenderMedia *R_FindMedia(const char *name, RenderMediaType type) {

  RenderMedia lookup = {
    .type = type
  };
  
  q_strlcpy(lookup.name, name, sizeof(lookup.name));

  RenderMedia *media = $(rMediaState.media, get, &lookup);
  if (media) {
    R_RegisterMedia(media);
  }

  return media;
}

/**
 * @brief Allocates a media object with the specified name.
 */
RenderMedia *R_AllocMedia(const char *name, size_t size, RenderMediaType type) {

  if (!name || !*name) {
    Com_Error(ERROR_DROP, "NULL name\n");
  }

  RenderMedia *media = Mem_TagMalloc(size, MEM_TAG_RENDERER);

  q_strlcpy(media->name, name, sizeof(media->name));
  media->type = type;

  return media;
}

/**
 * @brief Frees media when forced or when it is stale and unretained.
 */
static bool R_FreeMedia_(RenderMedia *media, void *data) {

  if (!data) {
    if (media->seed == rMediaState.seed) {
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
} RFreeMediaCtx;

static void R_FreeMedia_collect(const HashTable *table, ident key, ident value, ident data) {
  RFreeMediaCtx *ctx = data;
  RenderMedia *media = value;

  if (R_FreeMedia_(media, ctx->data)) {
    $(ctx->media, add, &media);
  }
}

static void R_FreeMediaEntries(void *data) {
  RFreeMediaCtx ctx = {
    .media = $(alloc(Vector), initWithSize, sizeof(RenderMedia *)),
    .data = data,
  };

  $(rMediaState.media, enumerate, R_FreeMedia_collect, &ctx);

  for (size_t i = 0; i < ctx.media->count; i++) {
    RenderMedia *media = VectorValue(ctx.media, RenderMedia *, i);
    $(rMediaState.media, remove, media);
  }

  release(ctx.media);
}

/**
 * @brief Frees the specified media immediately.
 */
void R_FreeMedia(RenderMedia *media) {

  R_FreeMedia_(media, (void *) 1);

  $(rMediaState.media, remove, media);
}

/**
 * @brief Begins a media loading pass.
 */
void R_BeginLoading(void) {
  int32_t s;

  do {
    s = Randomi();
  } while (s == rMediaState.seed);

  rMediaState.seed = s;
}

/**
 * @brief Ends a media loading pass and frees stale media.
 * @details Releasing a GPU resource only queues its destruction, which the backend performs
 * once no submitted work refers to it. The GPU is drained after the release, so that the
 * destruction of the media freed here is carried out rather than left pending.
 */
void R_EndLoading(void) {

  R_FreeMediaEntries(NULL);

  $(rContext.device, waitForIdle);

  R_LoadOcclusionQueries();

  R_ClearShadows();
}

/**
 * @brief Computes the hash value for a media entry by name and type.
 */
static size_t R_MediaHash(const void * key) {
  const RenderMedia *media = key;
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
  const RenderMedia *_a = a, *_b = b;

  if (_a->type == _b->type) {
    return !q_strcmp(_a->name, _b->name);
  }

  return false;
}

/**
 * @brief Initializes the media cache.
 */
void R_InitMedia(void) {

  memset(&rMediaState, 0, sizeof(rMediaState));

  rMediaState.media = $(alloc(HashTable), init, (HashTableHashFunc) R_MediaHash, (HashTableEqualFunc) R_MediaEqual);
  rMediaState.media->destroyValue = Mem_Free;

  R_BeginLoading();
}

/**
 * @brief Shuts down the media cache.
 */
void R_ShutdownMedia(void) {

  R_FreeMediaEntries((void *) 1);

  rMediaState.media = release(rMediaState.media);
}
