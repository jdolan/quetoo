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

#pragma once

#include "common/common.h"

/**
 * @brief Status of a manifest entry, used by the installer to track update state.
 */
typedef enum {
	ENTRY_CURRENT,
	ENTRY_PENDING,
	ENTRY_DOWNLOADING,
	ENTRY_STALE,
} cm_manifest_entry_status_t;

/**
 * @brief A single entry in a manifest.
 * @details Fields are ordered to match the on-disk format: <hash> <size> <path>
 */
typedef struct {

 /**
  * @brief MD5 hex digest of the file contents.
  */
	char hash[MAX_QPATH];

 /**
  * @brief File size in bytes.
  */
	int64_t size;

 /**
  * @brief Asset path used as the table key (e.g. "maps/edge.bsp").
  */
	char path[MAX_QPATH];

 /**
  * @brief Current update status of this entry.
  */
	cm_manifest_entry_status_t status;
} cm_manifest_entry_t;

/**
 * @brief Allocates an empty manifest table.
 * @return A new GHashTable mapping path strings to cm_manifest_entry_t values.
 */
GHashTable *Cm_AllocManifest(void);

/**
 * @brief Inserts an entry into a manifest table, computing the MD5 checksum of the given data.
 * @param manifest The manifest table returned by Cm_AllocManifest or Cm_ParseManifest.
 * @param path The asset path (used as the table key).
 * @param data The file data to checksum.
 * @param len The length of the data in bytes.
 */
void Cm_AddManifestEntry(GHashTable *manifest, const char *path, const void *data, size_t len);

/**
 * @brief Verifies a manifest entry against the local file on disk.
 * @param entry The manifest entry to check.
 * @return true if the local file exists and its MD5 matches the entry's hash.
 */
bool Cm_CheckManifestEntry(const cm_manifest_entry_t *entry);

/**
 * @brief Writes a manifest table to a file, sorted by path.
 * @param path The output file path (e.g. "maps/edge.mf").
 * @param manifest The manifest table to write.
 * @return The number of entries written, or -1 on failure.
 */
int32_t Cm_WriteManifest(const char *path, GHashTable *manifest);

/**
 * @brief Parses a manifest from an in-memory buffer.
 * @details The buffer must be NUL-terminated (or zero-padded past @c len).
 * Returns an empty table (never NULL) when @c data is NULL or @c len is zero.
 * @param data The manifest text data, or NULL for an empty manifest.
 * @param len The length of @c data in bytes.
 * @return A GHashTable mapping path strings to cm_manifest_entry_t values.
 */
GHashTable *Cm_ParseManifest(const char *data, size_t len);

/**
 * @brief Reads a manifest file into a table.
 * @param path The manifest file path (e.g. "maps/edge.mf").
 * @return A GHashTable of cm_manifest_entry_t, or NULL if the file does not exist.
 */
GHashTable *Cm_ReadManifest(const char *path);

/**
 * @brief Frees a manifest table and all its entries.
 */
void Cm_FreeManifest(GHashTable *manifest);
