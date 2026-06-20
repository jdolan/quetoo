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

#include <Objectively/HashTable.h>
#include <Objectively/Vector.h>

#include "s_local.h"

typedef struct {
  HashTable *media;
  List *keys;
  int32_t seed; // for freeing stale assets
} s_media_state_t;

static s_media_state_t s_media_state;

/**
 * @brief Precaches all of the sexed sounds for a given player model.
 */
void S_LoadClientModelSamples(const char *model) {

  Vector *sounds = $(alloc(Vector), initWithSize, sizeof(s_media_t *));

  for (const ListNode *node = s_media_state.keys ? s_media_state.keys->head : NULL; node; node = node->next) {
    s_media_t *media = $(s_media_state.media, get, node->element);

    if (media && media->name[0] == '*') {
      $(sounds, addElement, &media);
    }
  }

  for (size_t i = 0; i < sounds->count; i++) {
    const s_media_t *media = *VectorElement(sounds, s_media_t *, i);
    S_LoadClientModelSample(model, media->name);
  }

  release(sounds);
}

/**
 * @brief Prints information about all currently loaded media to the console.
 */
void S_ListMedia_f(void) {

  Com_Print("Loaded media:\n");

  for (const ListNode *node = s_media_state.keys ? s_media_state.keys->head : NULL; node; node = node->next) {
    s_media_t *media = $(s_media_state.media, get, node->element);

    if (media) {
      Com_Print("%s\n", media->name);
    }
  }
}

/**
 * @brief Establishes a dependency from the specified dependent to the given
 * dependency. Dependencies in use by registered media are never freed.
 */
void S_RegisterDependency(s_media_t *dependent, s_media_t *dependency) {

  if (dependent) {
    if (dependency) {
      bool found = false;
      if (dependent->dependencies) {
        for (const ListNode *n = dependent->dependencies->head; n; n = n->next) {
          if (n->element == dependency) { found = true; break; }
        }
      }
      if (!found) {
        Com_Debug(DEBUG_SOUND, "%s -> %s\n", dependent->name, dependency->name);
        if (!dependent->dependencies) {
          dependent->dependencies = $(alloc(List), init);
        }
        $(dependent->dependencies, appendElement, dependency);

        S_RegisterMedia(dependency);
      }
    } else {
      Com_Debug(DEBUG_SOUND, "Invalid dependency for %s\n", dependent->name);
    }
  } else {
    Com_Warn("Invalid dependent\n");
  }
}


static bool S_FreeMedia_(const char *key, s_media_t *media, bool force);

/**
 * @brief Inserts @p name into @p keys in sorted order.
 */
static void S_RegisterMedia_InsertSortedKey(const char *name) {

  if (!s_media_state.keys) {
    s_media_state.keys = $(alloc(List), init);
  }

  // find insertion point (insert before first node where name <= existing)
  for (ListNode *n = s_media_state.keys->head; n; n = n->next) {
    if (q_strcmp(name, (const char *) n->element) <= 0) {
      $(s_media_state.keys, insertElementAfter, n->prev, (void *) name);
      return;
    }
  }
  $(s_media_state.keys, appendElement, (void *) name);
}

/**
 * @brief Inserts the specified media into the shared table.
 */
void S_RegisterMedia(s_media_t *media) {

  // check to see if we're already seeded
  if (media->seed != s_media_state.seed) {
    s_media_t *m;

    if ((m = $(s_media_state.media, get, media->name))) {
      if (m != media) {
        Com_Debug(DEBUG_SOUND, "Replacing %s\n", media->name);
        S_FreeMedia_(NULL, m, true);
        $(s_media_state.media, set, media->name, media);
      } else {
        Com_Debug(DEBUG_SOUND, "Retaining %s\n", media->name);
      }
    } else {
      Com_Debug(DEBUG_SOUND, "Inserting %s\n", media->name);
      $(s_media_state.media, set, media->name, media);

      S_RegisterMedia_InsertSortedKey(media->name);
    }

    // re-seed the media to retain it
    media->seed = s_media_state.seed;
  }

  // finally re-register all dependencies
  for (const ListNode *d = media->dependencies ? media->dependencies->head : NULL; d; d = d->next) {
    S_RegisterMedia((s_media_t *) d->element);
  }
}

/**
 * @brief Resolves the specified media if it is already known. The returned
 * media is re-registered for convenience.
 * @return `s_media_t` The media, or `NULL`.
 */
s_media_t *S_FindMedia(const char *name, s_media_type_t type) {

  s_media_t lookup = {
    .type = type
  };

  q_strlcpy(lookup.name, name, sizeof(lookup.name));

  s_media_t *media = $(s_media_state.media, get, &lookup);
  if (media) {
    S_RegisterMedia(media);
  }

  return media;
}

/**
 * @brief Returns a newly allocated `s_media_t` with the specified name.
 * @param size The number of bytes to allocate for the media.
 * @param type The media type.
 * @return The newly initialized media.
 */
s_media_t *S_AllocMedia(const char *name, size_t size, s_media_type_t type) {

  if (!name || !*name) {
    Com_Error(ERROR_DROP, "NULL name\n");
  }

  s_media_t *media = Mem_TagMalloc(size, MEM_TAG_SOUND);

  q_strlcpy(media->name, name, sizeof(media->name));
  media->type = type;

  return media;
}

/**
 * @brief Frees the specified media entry. If @p force is true, always freed;
 * otherwise only media with stale seed values and no explicit retainment are freed.
 * Returns true if the media was freed.
 */
static bool S_FreeMedia_(const char *key, s_media_t *media, bool force) {
  (void) key;

  if (!force) { // see if the media should be freed
    if (media->seed == s_media_state.seed || (media->Retain && media->Retain(media))) {
      return false;
    }
  }

  Com_Debug(DEBUG_SOUND, "Freeing %s\n", media->name);

  // ask the implementation to clean up
  if (media->Free) {
    media->Free(media);
  }

  if (media->dependencies) {
    release(media->dependencies);
    media->dependencies = NULL;
  }

  // remove key from sorted keys list
  if (s_media_state.keys) {
    for (ListNode *n = s_media_state.keys->head; n; n = n->next) {
      if (n->element == media->name) {
        $(s_media_state.keys, removeNode, n);
        break;
      }
    }
  }

  return true;
}

/**
 * @brief Collects stale media entries into a Vector for deferred removal.
 */
static void S_EndLoading_Collect(const HashTable *table, ident k, ident v, ident data) {
  (void) table; (void) k;
  s_media_t *media = (s_media_t *) v;
  Vector *vec = (Vector *) data;
  if (!(media->seed == s_media_state.seed || (media->Retain && media->Retain(media)))) {
    $(vec, addElement, &media);
  }
}

/**
 * @brief Frees any media that has a stale seed and is not explicitly retained.
 */
void S_EndLoading(void) {

  // Collect keys to remove (can't modify table during enumeration)
  Vector *to_free = $(alloc(Vector), initWithSize, sizeof(s_media_t *));

  $(s_media_state.media, enumerate, S_EndLoading_Collect, to_free);

  for (size_t i = 0; i < to_free->count; i++) {
    s_media_t *media = *VectorElement(to_free, s_media_t *, i);
    $(s_media_state.media, remove, media->name);
    S_FreeMedia_(NULL, media, true);
    Mem_Free(media);
  }

  release(to_free);
}

/**
 * @brief Prepares the media subsystem for loading.
 */
void S_BeginLoading(void) {
  int32_t s;

  do {
    s = Randomi();
  } while (s == s_media_state.seed);

  s_media_state.seed = s;
}

/**
 * @brief Hash function for sound media entries keyed by name and type.
 */
static size_t S_MediaHash(const void * key) {
  const s_media_t *media = key;

  uint32_t hash = 5381;
  for (const char *p = media->name; *p; p++) {
    hash = hash * 33 ^ (unsigned char) *p;
  }
  return hash + media->type;
}

/**
 * @brief Equality function for sound media entries keyed by name and type.
 */
static bool S_MediaEqual(const void * a, const void * b) {
  const s_media_t *_a = a, *_b = b;

  if (_a->type == _b->type) {
    return !q_strcmp(_a->name, _b->name);
  }

  return false;
}

/**
 * @brief Initializes the media pool.
 */
void S_InitMedia(void) {

  memset(&s_media_state, 0, sizeof(s_media_state));

  s_media_state.media = $(alloc(HashTable), init,
                          (HashTableHashFunc) S_MediaHash,
                          (HashTableEqualFunc) S_MediaEqual);

  S_BeginLoading();
}

/**
 * @brief Collects all media entries into a Vector for forced removal.
 */
static void S_ShutdownMedia_Collect(const HashTable *table, ident k, ident v, ident data) {
  (void) table; (void) k;
  Vector *vec = (Vector *) data;
  s_media_t *media = (s_media_t *) v;
  $(vec, addElement, &media);
}

/**
 * @brief Shuts down the media pool.
 */
void S_ShutdownMedia(void) {

  Vector *to_free = $(alloc(Vector), initWithSize, sizeof(s_media_t *));
  $(s_media_state.media, enumerate, S_ShutdownMedia_Collect, to_free);

  for (size_t i = 0; i < to_free->count; i++) {
    s_media_t *media = *VectorElement(to_free, s_media_t *, i);
    $(s_media_state.media, remove, media->name);
    S_FreeMedia_(NULL, media, true);
    Mem_Free(media);
  }

  release(to_free);
  release(s_media_state.media);

  if (s_media_state.keys) {
    release(s_media_state.keys);
  }
}

