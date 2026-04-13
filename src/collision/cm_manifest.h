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
 * @brief A single entry in a map manifest.
 */
typedef struct {
	char path[MAX_QPATH];
	char hash[33]; // 32 hex chars + NUL (MD5)
} cm_manifest_entry_t;

/**
 * @brief Appends an entry to a manifest list, computing the MD5 checksum of the given data.
 * @param manifest Pointer to the manifest GList (may point to NULL for empty list).
 * @param path The asset path.
 * @param data The file data to checksum.
 * @param len The length of the data in bytes.
 */
void Cm_AddManifestEntry(GList **manifest, const char *path, const void *data, size_t len);

/**
 * @brief Verifies a manifest entry against the local file on disk.
 * @param entry The manifest entry to check.
 * @return true if the local file exists and its MD5 matches the entry's hash.
 */
bool Cm_CheckManifestEntry(const cm_manifest_entry_t *entry);

/**
 * @brief Writes a manifest list to a file.
 * @param path The output file path (e.g. "maps/edge.mf").
 * @param manifest The manifest list to write.
 * @return The number of entries written, or -1 on failure.
 */
int32_t Cm_WriteManifest(const char *path, GList *manifest);

/**
 * @brief Reads a manifest file and returns a GList of cm_manifest_entry_t.
 * Each entry is allocated with Mem_Malloc and should be freed with
 * Cm_FreeManifest.
 * @param path The manifest file path (e.g. "maps/edge.mf").
 * @return A GList of cm_manifest_entry_t, or NULL on failure.
 */
GList *Cm_ReadManifest(const char *path);

/**
 * @brief Frees a manifest list.
 */
void Cm_FreeManifest(GList *manifest);
