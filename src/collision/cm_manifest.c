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
#include "shared/md5.h"

/**
 * @brief Computes the `MD5` hex digest of the given data.
 */
static void Cm_Md5Hex(const void *data, size_t len, char *hex, size_t hex_size) {

	md5_ctx ctx;
	uint8_t digest[16];

	md5_init(&ctx);
	md5_update(&ctx, data, len);
	md5_finalize(&ctx, digest);

	for (int i = 0; i < 16 && (size_t)(i * 2 + 3) <= hex_size; i++) {
		q_snprintf(hex + i * 2, 3, "%02x", digest[i]);
	}
}

/**
 * @brief Allocates an empty manifest table.
 */
HashTable *Cm_AllocManifest(void) {
	HashTable *manifest = $(alloc(HashTable), init, HashTableHashStr, HashTableEqualStr);
	manifest->destroyValue = Mem_Free;
	return manifest;
}

/**
 * @brief Inserts an entry into a manifest table, computing the `MD5` checksum of the given data.
 */
void Cm_AddManifestEntry(HashTable *manifest, const char *path, const void *data, size_t len) {

	assert(manifest);
	assert(path);
	assert(data);
	assert(len > 0);

	cm_manifest_entry_t *entry = Mem_Malloc(sizeof(*entry));
	q_strlcpy(entry->path, path, sizeof(entry->path));
	entry->size = (int64_t) len;
	Cm_Md5Hex(data, len, entry->hash, sizeof(entry->hash));

	$(manifest, set, entry->path, entry);
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

	return !q_strcmp(entry->hash, hash);
}

/**
 * @brief Comparator for sorting manifest keys alphabetically.
 */
static int Cm_ManifestKeyCmp(const void *a, const void *b) {
	return q_strcmp(*(const char **) a, *(const char **) b);
}

typedef struct {
	const char **keys;
	size_t count;
} cm_manifest_keys_t;

/**
 * @brief HashTableEnumerator callback that collects keys.
 */
static void Cm_CollectKey(const HashTable *table, ident key, ident value, ident data) {
	cm_manifest_keys_t *collector = data;
	collector->keys[collector->count++] = key;
}

/**
 * @brief Writes a manifest table to a file, sorted by path.
 */
int32_t Cm_WriteManifest(const char *path, HashTable *manifest) {

	file_t *file = Fs_OpenWrite(path);
	if (!file) {
		Com_Warn("Failed to open %s for writing\n", path);
		return -1;
	}

	const size_t count = manifest->count;
	const char **keys = Mem_Malloc(count * sizeof(char *));
	cm_manifest_keys_t collector = { .keys = keys };

	$(manifest, enumerate, Cm_CollectKey, &collector);
	qsort(keys, count, sizeof(char *), Cm_ManifestKeyCmp);

	for (size_t k = 0; k < count; k++) {
		const cm_manifest_entry_t *entry = $(manifest, get, (void *) keys[k]);
		Fs_Print(file, "%s %" PRId64 " %s\n", entry->hash, entry->size, entry->path);
	}

	Mem_Free(keys);
	Fs_Close(file);

	return (int32_t) count;
}

/**
 * @brief Parses a manifest from an in-memory buffer.
 */
HashTable *Cm_ParseManifest(const char *data, size_t len) {

	HashTable *manifest = Cm_AllocManifest();

	if (!data || len == 0) {
		return manifest;
	}

	// Work on a mutable copy so we can tokenize in-place (`data` is not
	// guaranteed to be null-terminated; buffers from Net_HttpGet are exactly
	// `len` bytes).
	char *buf = Mem_Malloc(len + 1);
	memcpy(buf, data, len);
	buf[len] = '\0';

	char *saveptr = NULL;
	char *line = q_strtok_r(buf, "\n", &saveptr);
	while (line) {

		// Trim trailing whitespace
		char *end = line + q_strlen(line);
		while (end > line && (*end == '\0' || *end == '\r' || *end == ' ' || *end == '\t')) {
			*end-- = '\0';
		}

		if (*line == '\0') {
			line = q_strtok_r(NULL, "\n", &saveptr);
			continue;
		}

		char *space1 = q_strchr(line, ' ');
		if (!space1) {
			Com_Warn("Malformed manifest line: %s\n", line);
			line = q_strtok_r(NULL, "\n", &saveptr);
			continue;
		}
		*space1 = '\0';

		char *space2 = q_strchr(space1 + 1, ' ');
		if (!space2) {
			Com_Warn("Malformed manifest line: %s\n", line);
			line = q_strtok_r(NULL, "\n", &saveptr);
			continue;
		}
		*space2 = '\0';

		cm_manifest_entry_t *entry = Mem_Malloc(sizeof(*entry));
		q_strlcpy(entry->hash, line, sizeof(entry->hash));
		entry->size = (int64_t) strtoll(space1 + 1, NULL, 10);
		q_strlcpy(entry->path, space2 + 1, sizeof(entry->path));

		$(manifest, set, entry->path, entry);

		line = q_strtok_r(NULL, "\n", &saveptr);
	}

	Mem_Free(buf);
	return manifest;
}

/**
 * @brief Reads a manifest file into a table.
 */
HashTable *Cm_ReadManifest(const char *path) {

	void *data = NULL;
	const int64_t len = Fs_Load(path, &data);
	if (len <= 0 || !data) {
		return NULL;
	}

	HashTable *manifest = Cm_ParseManifest((const char *) data, (size_t) len);
	Fs_Free(data);

	return manifest;
}

/**
 * @brief Frees a manifest table and all its entries.
 */
void Cm_FreeManifest(HashTable *manifest) {
	if (manifest) {
		release(manifest);
	}
}
