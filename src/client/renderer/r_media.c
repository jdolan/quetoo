/*
 * Copyright(c) 1997-2001 Id Software, Inc.
 * Copyright(c) 2002 The Quakeforge Project.
 * Copyright(c) 2006 Quake2World.
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
	uint32_t seed; // for tracking stale assets
} r_media_state_t;

static r_media_state_t r_media_state;

/*
 * @brief GHRFunc for R_ListMedia_f.
 */
static void R_ListMedia_f_(gpointer key __attribute__((unused)), gpointer value, gpointer data __attribute__((unused))) {
	Com_Print("%s\n", ((r_media_t *) value)->name);
}

/*
 * @brief Prints information about all currently loaded media to the console.
 */
void R_ListMedia_f(void) {

	Com_Print("Loaded media:\n");

	g_hash_table_foreach(r_media_state.media, R_ListMedia_f_, NULL);
}

/*
 * @brief Establishes a dependency from the specified dependent to the given
 * dependency. Dependencies in use by registered media are never freed.
 */
void R_RegisterDependency(r_media_t *dependent, r_media_t *dependency) {

	if (dependent) {
		if (dependency) {
			if (!g_list_find(dependent->dependencies, dependency)) {
				Com_Debug("R_RegisterDependency: %s -> %s.\n", dependent->name, dependency->name);
				dependent->dependencies = g_list_prepend(dependent->dependencies, dependency);

				R_RegisterMedia(dependency);
			}
		} else {
			Com_Debug("R_RegisterDependency: Invalid dependency for %s.\n", dependent->name);
		}
	} else {
		Com_Warn("R_RegisterDependency: Invalid dependent.\n");
	}
}

static gboolean R_FreeMedia_(gpointer key, gpointer value, gpointer data);

/*
 * @brief Inserts the specified media into the shared table, re-registering all
 * of its dependencies as well.
 */
void R_RegisterMedia(r_media_t *media) {

	// check to see if we're already seeded
	if (media->seed != r_media_state.seed) {
		r_media_t *m;

		if ((m = g_hash_table_lookup(r_media_state.media, media->name))) {
			if (m != media) {
				if (R_FreeMedia_(NULL, m, NULL)) {
					Com_Debug("R_RegisterMedia: Replacing %s.\n", media->name);
					g_hash_table_insert(r_media_state.media, media->name, media);
				} else {
					Com_Error(ERR_DROP, "R_RegisterMedia: Failed to replace %s.\n", media->name);
				}
			} else {
				Com_Debug("R_RegisterMedia: Retaining %s.\n", media->name);
			}
		} else {
			Com_Debug("R_RegisterMedia: Inserting %s.\n", media->name);
			g_hash_table_insert(r_media_state.media, media->name, media);
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
}

/*
 * @brief Resolves the specified media if it is already known. The returned
 * media is re-registered for convenience.
 *
 * @return r_media_t The media, or NULL.
 */
r_media_t *R_FindMedia(const char *name) {
	r_media_t *media;

	if ((media = g_hash_table_lookup(r_media_state.media, name))) {
		R_RegisterMedia(media);
	}

	return media;
}

/*
 * @brief Returns a newly allocated r_media_t with the specified name.
 *
 * @param size_t size The number of bytes to allocate for the media.
 *
 * @return The newly initialized media.
 */
r_media_t *R_MallocMedia(const char *name, size_t size) {

	if (!name || !*name) {
		Com_Error(ERR_DROP, "R_MallocMedia: NULL name\n");
	}

	r_media_t *media = Z_TagMalloc(size, Z_TAG_RENDERER);

	g_strlcpy(media->name, name, sizeof(media->name));

	return media;
}

/*
 * @brief GHRFunc for freeing media. If data is non-NULL, then the media is
 * always freed. Otherwise, only media with stale seed values and no explicit
 * retainment are released.
 */
static gboolean R_FreeMedia_(gpointer key __attribute__((unused)), gpointer value, gpointer data) {
	r_media_t *media = (r_media_t *) value;

	if (!data) { // see if the media should be freed
		if (media->seed == r_media_state.seed || (media->Retain && media->Retain(media))) {
			return false;
		}
	}

	Com_Debug("R_FreeMedia: Freeing %s\n", media->name);

	// ask the implementation to clean up
	if (media->Free) {
		media->Free(media);
	}

	g_list_free(media->dependencies);

	return true;
}

/*
 * @brief Frees any media that has a stale seed and is not explicitly retained.
 */
void R_FreeMedia(void) {
	g_hash_table_foreach_remove(r_media_state.media, R_FreeMedia_, NULL);
}

/*
 * @brief Prepares the media subsystem for loading.
 */
void R_BeginLoading(void) {
	r_media_state.seed = Sys_Milliseconds();
}

/*
 * @brief Initializes the media pool.
 */
void R_InitMedia(void) {

	memset(&r_media_state, 0, sizeof(r_media_state));

	r_media_state.media = g_hash_table_new_full(g_str_hash, g_str_equal, NULL, Z_Free);
}

/*
 * @brief Shuts down the media pool.
 */
void R_ShutdownMedia(void) {

	g_hash_table_foreach_remove(r_media_state.media, R_FreeMedia_, (void *) true);

	g_hash_table_destroy(r_media_state.media);
}

