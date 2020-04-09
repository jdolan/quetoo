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
	int32_t seed; // for tracking stale assets
} r_media_state_t;

static r_media_state_t r_media_state;

/**
 * @brief Enumerates all media in key-alphabetical order.
 */
void R_EnumerateMedia(R_MediaEnumerator enumerator, void *data) {

	GList *keys = g_hash_table_get_keys(r_media_state.media);
	keys = g_list_sort(keys, (GCompareFunc) g_strcmp0);

	const GList *key = keys;
	while (key) {
		const r_media_t *media = g_hash_table_lookup(r_media_state.media, key->data);
		if (enumerator) {
			enumerator(media, data);
		}
		key = key->next;
	}

	g_list_free(keys);
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

	assert(dependent);
	assert(dependency);

	if (!g_list_find(dependent->dependencies, dependency)) {
		dependent->dependencies = g_list_prepend(dependent->dependencies, dependency);
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

		if (g_hash_table_contains(r_media_state.media, media->name)) {
			r_media_t *other = g_hash_table_lookup(r_media_state.media, media->name);
			if (other != media) {
				R_FreeMedia(other);
				g_hash_table_insert(r_media_state.media, media->name, media);
			}
		} else {
			g_hash_table_insert(r_media_state.media, media->name, media);
		}

		media->seed = r_media_state.seed;
	}

	if (media->Register) {
		media->Register(media);
	}

	GList *d = media->dependencies;
	while (d) {
		R_RegisterMedia((r_media_t *) d->data);
		d = d->next;
	}

	return media;
}

/**
 * @return The named media if it is already known, re-registering it for convenience.
 */
r_media_t *R_FindMedia(const char *name) {

	r_media_t *media = g_hash_table_lookup(r_media_state.media, name);

	if (media) {
		R_RegisterMedia(media);
	}

	return media;
}

/**
 * @brief Returns a newly allocated r_media_t with the specified name.
 * @param size The number of bytes to allocate for the media.
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
 * retainment are freed.
 */
static gboolean R_FreeMedia_(gpointer key, gpointer value, gpointer data) {
	r_media_t *media = (r_media_t *) value;

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

	g_list_free(media->dependencies);

	return true;
}

/**
 * @breif Frees the specified media immediately.
 */
void R_FreeMedia(r_media_t *media) {

	R_FreeMedia_(NULL, media, (void *) 1);

	g_hash_table_remove(r_media_state.media, media->name);
}

/**
 * @brief Frees any media that has a stale seed and is not explicitly retained.
 */
void R_FreeUnseededMedia(void) {
	g_hash_table_foreach_remove(r_media_state.media, R_FreeMedia_, (void *) 0);
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

	g_hash_table_foreach_remove(r_media_state.media, R_FreeMedia_, (void *) 1);
	g_hash_table_destroy(r_media_state.media);
}

