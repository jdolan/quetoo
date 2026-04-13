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

#include "cm_manifest.h"

/**
 * @brief Computes the MD5 hex digest of the given data.
 */
static void Cm_Md5Hex(const void *data, size_t len, char *hex, size_t hex_size) {

	GChecksum *md5 = g_checksum_new(G_CHECKSUM_MD5);
	g_checksum_update(md5, (const guchar *) data, len);
	g_strlcpy(hex, g_checksum_get_string(md5), hex_size);
	g_checksum_free(md5);
}

/**
 * @brief Appends an entry to a manifest list, computing the MD5 checksum of the given data.
 */
void Cm_AddManifestEntry(GList **manifest, const char *path, const void *data, size_t len) {

	assert(manifest);
	assert(path);
	assert(data);
	assert(len > 0);

	cm_manifest_entry_t *entry = Mem_Malloc(sizeof(*entry));
	g_strlcpy(entry->path, path, sizeof(entry->path));
	Cm_Md5Hex(data, len, entry->hash, sizeof(entry->hash));

	*manifest = g_list_append(*manifest, entry);
}

/**
 * @brief Verifies a manifest entry against the local file on disk.
 */
bool Cm_CheckManifestEntry(const cm_manifest_entry_t *entry) {

	assert(entry);

	void *data = NULL;
	const int64_t len = Fs_Load(entry->path, &data);
	if (len <= 0 || !data) {
		if (data) {
			Fs_Free(data);
		}
		return false;
	}

	char hash[sizeof(entry->hash)];
	Cm_Md5Hex(data, len, hash, sizeof(hash));
	Fs_Free(data);

	return !g_strcmp0(entry->hash, hash);
}

/**
 * @brief Writes a manifest list to a file.
 */
int32_t Cm_WriteManifest(const char *path, GList *manifest) {

	file_t *file = Fs_OpenWrite(path);
	if (!file) {
		Com_Warn("Failed to open %s for writing\n", path);
		return -1;
	}

	int32_t count = 0;
	for (GList *e = manifest; e; e = e->next) {
		const cm_manifest_entry_t *entry = (const cm_manifest_entry_t *) e->data;
		Fs_Print(file, "%s %s\n", entry->hash, entry->path);
		count++;
	}

	Fs_Close(file);

	return count;
}

/**
 * @brief Reads a manifest file and returns a GList of cm_manifest_entry_t.
 */
GList *Cm_ReadManifest(const char *path) {

	void *data = NULL;
	const int64_t len = Fs_Load(path, &data);
	if (len <= 0 || !data) {
		return NULL;
	}

	GList *manifest = NULL;

	gchar **lines = g_strsplit((const char *) data, "\n", -1);
	Fs_Free(data);

	for (gchar **line = lines; *line; line++) {
		if (**line == '\0') {
			continue;
		}

		char *space = strchr(*line, ' ');
		if (!space) {
			Com_Warn("Malformed manifest line: %s\n", *line);
			continue;
		}

		*space = '\0';

		cm_manifest_entry_t *entry = Mem_Malloc(sizeof(*entry));
		g_strlcpy(entry->path, space + 1, sizeof(entry->path));
		g_strlcpy(entry->hash, *line, sizeof(entry->hash));

		manifest = g_list_append(manifest, entry);
	}

	g_strfreev(lines);

	return manifest;
}

/**
 * @brief Frees a manifest list.
 */
void Cm_FreeManifest(GList *manifest) {
	g_list_free_full(manifest, Mem_Free);
}
