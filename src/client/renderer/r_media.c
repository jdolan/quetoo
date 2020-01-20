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
	GHashTable *media;
	GList *keys;
	int32_t seed; // for tracking stale assets
} r_media_state_t;

static r_media_state_t r_media_state;

/**
 * @brief Enumerates all media in key-alphabetical order.
 */
void R_EnumerateMedia(R_MediaEnumerator enumerator, void *data) {

	const GList *key = r_media_state.keys;
	while (key) {
		const r_media_t *media = g_hash_table_lookup(r_media_state.media, key->data);
		if (enumerator) {
			enumerator(media, data);
		}
		key = key->next;
	}
}

/**
 * @brief R_MediaEnumerator for R_ListMedia_f.
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

	if (dependent) {
		if (dependency) {
			if (!g_list_find(dependent->dependencies, dependency)) {
				Com_Debug(DEBUG_RENDERER, "%s -> %s\n", dependent->name, dependency->name);
				dependent->dependencies = g_list_prepend(dependent->dependencies, dependency);

				R_RegisterMedia(dependency);
			}
		} else {
			// Com_Debug("Invalid dependency for %s\n", dependent->name);
		}
	} else {
		Com_Warn("Invalid dependent\n");
	}

	return dependency;
}

/**
 * @brief GCompareFunc for R_RegisterMedia. Sorts media by name.
 */
static int32_t R_RegisterMedia_Compare(gconstpointer name1, gconstpointer name2) {
	return g_strcmp0((const char *) name1, (const char *) name2);
}

static gboolean R_FreeMedia_(gpointer key, gpointer value, gpointer data);

/**
 * @brief Inserts the specified media into the shared table, re-registering all
 * of its dependencies as well.
 */
r_media_t *R_RegisterMedia(r_media_t *media) {

	// check to see if we're already seeded
	if (media->seed != r_media_state.seed) {
		r_media_t *m;

		if ((m = g_hash_table_lookup(r_media_state.media, media->name))) {
			if (m != media) {
				Com_Debug(DEBUG_RENDERER, "Replacing %s\n", media->name);
				R_FreeMedia_(NULL, m, m);
				g_hash_table_replace(r_media_state.media, media->name, media);
			} else {
				Com_Debug(DEBUG_RENDERER, "Retaining %s\n", media->name);
			}
		} else {
			Com_Debug(DEBUG_RENDERER, "Inserting %s\n", media->name);
			g_hash_table_insert(r_media_state.media, media->name, media);

			r_media_state.keys = g_list_insert_sorted(r_media_state.keys, media->name,
			                     R_RegisterMedia_Compare);
		}

		// re-seed the media to retain it
		media->seed = r_media_state.seed;
	}

	// call the implementation-specific register function
	if (media->Register) {
		media->Register(media);
	}

	// and finally re-register all dependencies
	GList *d = media->dependencies;
	while (d) {
		R_RegisterMedia((r_media_t *) d->data);
		d = d->next;
	}

	return media;
}

/**
 * @brief Resolves the specified media if it is already known. The returned
 * media is re-registered for convenience.
 *
 * @return r_media_t The media, or `NULL`.
 */
r_media_t *R_FindMedia(const char *name) {
	r_media_t *media;

	if ((media = g_hash_table_lookup(r_media_state.media, name))) {
		R_RegisterMedia(media);
	}

	return media;
}

/**
 * @brief Returns a newly allocated r_media_t with the specified name.
 *
 * @param size_t size The number of bytes to allocate for the media.
 *
 * @return The newly initialized media.
 */
r_media_t *R_AllocMedia(const char *name, size_t size, r_media_type_t type) {

	if (!name || !*name) {
		Com_Error(ERROR_DROP, "NULL name\n");
	}

	r_media_t *media = Mem_TagMalloc(size, MEM_TAG_RENDERER);

	g_strlcpy(media->name, name, sizeof(media->name));
	media->type = type;

	return media;
}

/**
 * @brief GHRFunc for freeing media. If data is non-NULL, then the media is
 * always freed. Otherwise, only media with stale seed values and no explicit
 * retainment are released.
 */
static gboolean R_FreeMedia_(gpointer key, gpointer value, gpointer data) {
	r_media_t *media = (r_media_t *) value;

	if (!data) { // see if the media should be freed
		if (media->seed == r_media_state.seed || (media->Retain && media->Retain(media))) {
			return false;
		}
	}

	Com_Debug(DEBUG_RENDERER, "Freeing %s\n", media->name);

	// ask the implementation to clean up
	if (media->Free) {
		media->Free(media);
	}

	g_list_free(media->dependencies);
	r_media_state.keys = g_list_remove(r_media_state.keys, key);

	return true;
}

/**
 * @brief Frees any media that has a stale seed and is not explicitly retained.
 */
void R_FreeMedia(void) {
	g_hash_table_foreach_remove(r_media_state.media, R_FreeMedia_, NULL);
}

/**
 * @brief Prepares the media subsystem for loading.
 */
void R_BeginLoading(void) {
	int32_t s;

	do {
		s = Random();
	} while (s == r_media_state.seed);

	r_media_state.seed = s;
}

/**
 * @brief Initializes the media pool.
 */
void R_InitMedia(void) {

	memset(&r_media_state, 0, sizeof(r_media_state));

	r_media_state.media = g_hash_table_new_full(g_str_hash, g_str_equal, NULL, Mem_Free);

	R_BeginLoading();
}

/**
 * @brief Shuts down the media pool.
 */
void R_ShutdownMedia(void) {

	g_hash_table_foreach_remove(r_media_state.media, R_FreeMedia_, (void *) true);

	g_hash_table_destroy(r_media_state.media);
	g_list_free(r_media_state.keys);
}

