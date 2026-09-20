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

#include <Objectively/HashTable.h>
#include <Objectively/Vector.h>

#include "s_local.h"

typedef struct {
  HashTable *media;
  List *keys;
  int32_t seed; // for freeing stale assets
} SoundMediaState;

static SoundMediaState sMediaState;

/**
 * @brief Precaches all of the sexed sounds for a given player model.
 */
void S_LoadClientModelSamples(const char *model, const char *soundSet) {

  Vector *sounds = $(alloc(Vector), initWithSize, sizeof(SoundMedia *));

  for (const ListNode *node = sMediaState.keys ? sMediaState.keys->head : NULL; node; node = node->next) {
    SoundMedia *media = $(sMediaState.media, get, node->element);

    if (media && media->name[0] == '*') {
      $(sounds, add, &media);
    }
  }

  for (size_t i = 0; i < sounds->count; i++) {
    const SoundMedia *media = VectorValue(sounds, SoundMedia *, i);
    S_LoadClientModelSample(model, soundSet, media->name);
  }

  release(sounds);
}

/**
 * @brief Prints information about all currently loaded media to the console.
 */
void S_ListMedia_f(void) {

  Com_Print("Loaded media:\n");

  for (const ListNode *node = sMediaState.keys ? sMediaState.keys->head : NULL; node; node = node->next) {
    SoundMedia *media = $(sMediaState.media, get, node->element);

    if (media) {
      Com_Print("%s\n", media->name);
    }
  }
}

/**
 * @brief Establishes a dependency from the specified dependent to the given
 * dependency. Dependencies in use by registered media are never freed.
 */
void S_RegisterDependency(SoundMedia *dependent, SoundMedia *dependency) {

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
        $(dependent->dependencies, append, dependency);

        S_RegisterMedia(dependency);
      }
    } else {
      Com_Debug(DEBUG_SOUND, "Invalid dependency for %s\n", dependent->name);
    }
  } else {
    Com_Warn("Invalid dependent\n");
  }
}


static bool S_FreeMedia_(const char *key, SoundMedia *media, bool force);

/**
 * @brief Inserts @p name into @p keys in sorted order.
 */
static void S_RegisterMedia_InsertSortedKey(const char *name) {

  if (!sMediaState.keys) {
    sMediaState.keys = $(alloc(List), init);
  }

  // find insertion point (insert before first node where name <= existing)
  for (ListNode *n = sMediaState.keys->head; n; n = n->next) {
    if (q_strcmp(name, (const char *) n->element) <= 0) {
      $(sMediaState.keys, insertAfter, n->prev, (void *) name);
      return;
    }
  }
  $(sMediaState.keys, append, (void *) name);
}

/**
 * @brief Inserts the specified media into the shared table.
 */
void S_RegisterMedia(SoundMedia *media) {

  // check to see if we're already seeded
  if (media->seed != sMediaState.seed) {
    SoundMedia *m;

    if ((m = $(sMediaState.media, get, media->name))) {
      if (m != media) {
        Com_Debug(DEBUG_SOUND, "Replacing %s\n", media->name);
        S_FreeMedia_(NULL, m, true);
        $(sMediaState.media, set, media->name, media);
      } else {
        Com_Debug(DEBUG_SOUND, "Retaining %s\n", media->name);
      }
    } else {
      Com_Debug(DEBUG_SOUND, "Inserting %s\n", media->name);
      $(sMediaState.media, set, media->name, media);

      S_RegisterMedia_InsertSortedKey(media->name);
    }

    // re-seed the media to retain it
    media->seed = sMediaState.seed;
  }

  // finally re-register all dependencies
  for (const ListNode *d = media->dependencies ? media->dependencies->head : NULL; d; d = d->next) {
    S_RegisterMedia((SoundMedia *) d->element);
  }
}

/**
 * @brief Resolves the specified media if it is already known. The returned
 * media is re-registered for convenience.
 * @return `SoundMedia` The media, or `NULL`.
 */
SoundMedia *S_FindMedia(const char *name, SoundMediaType type) {

  SoundMedia lookup = {
    .type = type
  };

  q_strlcpy(lookup.name, name, sizeof(lookup.name));

  SoundMedia *media = $(sMediaState.media, get, &lookup);
  if (media) {
    S_RegisterMedia(media);
  }

  return media;
}

/**
 * @brief Returns a newly allocated `SoundMedia` with the specified name.
 * @param size The number of bytes to allocate for the media.
 * @param type The media type.
 * @return The newly initialized media.
 */
SoundMedia *S_AllocMedia(const char *name, size_t size, SoundMediaType type) {

  if (!name || !*name) {
    Com_Error(ERROR_DROP, "NULL name\n");
  }

  SoundMedia *media = Mem_TagMalloc(size, MEM_TAG_SOUND);

  q_strlcpy(media->name, name, sizeof(media->name));
  media->type = type;

  return media;
}

/**
 * @brief Frees the specified media entry. If @p force is true, always freed;
 * otherwise only media with stale seed values and no explicit retainment are freed.
 * Returns true if the media was freed.
 */
static bool S_FreeMedia_(const char *key, SoundMedia *media, bool force) {
  (void) key;

  if (!force) { // see if the media should be freed
    if (media->seed == sMediaState.seed || (media->Retain && media->Retain(media))) {
      return false;
    }
  }

  Com_Debug(DEBUG_SOUND, "Freeing %s\n", media->name);

  // ask the implementation to clean up
  if (media->Free) {
    media->Free(media);
  }

  media->dependencies = release(media->dependencies);

  // remove key from sorted keys list
  if (sMediaState.keys) {
    for (ListNode *n = sMediaState.keys->head; n; n = n->next) {
      if (n->element == media->name) {
        $(sMediaState.keys, removeNode, n);
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
  SoundMedia *media = (SoundMedia *) v;
  Vector *vec = (Vector *) data;
  if (!(media->seed == sMediaState.seed || (media->Retain && media->Retain(media)))) {
    $(vec, add, &media);
  }
}

/**
 * @brief Frees any media that has a stale seed and is not explicitly retained.
 */
void S_EndLoading(void) {

  // Collect keys to remove (can't modify table during enumeration)
  Vector *toFree = $(alloc(Vector), initWithSize, sizeof(SoundMedia *));

  $(sMediaState.media, enumerate, S_EndLoading_Collect, toFree);

  for (size_t i = 0; i < toFree->count; i++) {
    SoundMedia *media = VectorValue(toFree, SoundMedia *, i);
    $(sMediaState.media, remove, media->name);
    S_FreeMedia_(NULL, media, true);
    Mem_Free(media);
  }

  release(toFree);
}

/**
 * @brief Prepares the media subsystem for loading.
 */
void S_BeginLoading(void) {
  int32_t s;

  do {
    s = Randomi();
  } while (s == sMediaState.seed);

  sMediaState.seed = s;
}

/**
 * @brief Hash function for sound media entries keyed by name and type.
 */
static size_t S_MediaHash(const void * key) {
  const SoundMedia *media = key;

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
  const SoundMedia *_a = a, *_b = b;

  if (_a->type == _b->type) {
    return !q_strcmp(_a->name, _b->name);
  }

  return false;
}

/**
 * @brief Initializes the media pool.
 */
void S_InitMedia(void) {

  memset(&sMediaState, 0, sizeof(sMediaState));

  sMediaState.media = $(alloc(HashTable), init,
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
  SoundMedia *media = (SoundMedia *) v;
  $(vec, add, &media);
}

/**
 * @brief Shuts down the media pool.
 */
void S_ShutdownMedia(void) {

  Vector *toFree = $(alloc(Vector), initWithSize, sizeof(SoundMedia *));
  $(sMediaState.media, enumerate, S_ShutdownMedia_Collect, toFree);

  for (size_t i = 0; i < toFree->count; i++) {
    SoundMedia *media = VectorValue(toFree, SoundMedia *, i);
    $(sMediaState.media, remove, media->name);
    S_FreeMedia_(NULL, media, true);
    Mem_Free(media);
  }

  release(toFree);
  release(sMediaState.media);

  if (sMediaState.keys) {
    release(sMediaState.keys);
  }
}

