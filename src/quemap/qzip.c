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

#include <Objectively/List.h>

#include "deps/minizip/miniz.h"

#include "bsp.h"
#include "qzip.h"

bool include_shared = false;
bool update_zip = false;

static bool HasSuffix(const char *str, const char *suffix) {
  const size_t len = q_strlen(str);
  const size_t suffix_len = q_strlen(suffix);
  return len >= suffix_len && !q_strcmp(str + len - suffix_len, suffix);
}

static void CollectManifestAsset(const HashTable *table, ident key, ident value, ident data) {
  List *assets = data;
  const cm_manifest_entry_t *entry = value;
  $(assets, appendElement, q_strdup(entry->path));
}

/**
 * @brief Reads the manifest file and generates a pk3 archive containing
 * all referenced assets.
 */
int32_t ZIP_Main(void) {
  char path[MAX_OS_PATH];

  Com_Print("\n------------------------------------------\n");
  Com_Print("\nCreating archive for %s\n\n", bsp_name);

  const uint32_t start = (uint32_t) SDL_GetTicks();

  // read the manifest
  char mf_path[MAX_OS_PATH];
  q_snprintf(mf_path, sizeof(mf_path), "maps/%s.mf", map_base);

  HashTable *manifest = Cm_ReadManifest(mf_path);
  if (!manifest) {
    Com_Error(ERROR_FATAL, "Failed to load %s. Run -bsp first to generate the manifest.\n", mf_path);
  }

  // the manifest includes the bsp and all referenced assets
  List *assets = $(alloc(List), init);
  assets->destroy = free;

  // include the manifest itself so the pk3 is self-contained
  $(assets, appendElement, q_strdup(mf_path));
  $(manifest, enumerate, CollectManifestAsset, assets);

  Cm_FreeManifest(manifest);

  mz_zip_archive zip;
  memset(&zip, 0, sizeof(zip));

  // write to a "temporary" archive name
  q_snprintf(path, sizeof(path), "%s/map-%s-%d.pk3", Fs_WriteDir(), map_base, getpid());

  if (mz_zip_writer_init_file(&zip, path, 0)) {
    Com_Print("Compressing %zu resources to %s...\n", assets->count, path);

    for (const ListNode *node = assets->head; node; node = node->next) {
      const char *filename = node->element;

      if (!Fs_Exists(filename)) {
        Com_Warn("Missing %s\n", filename);
        continue;
      }

      if (include_shared == false) {
        const char *dir = Fs_RealDir(filename);

        if (GlobMatch("sky-*.pk3", dir, GLOB_CASE_INSENSITIVE) ||
          GlobMatch("sounds-*.pk3", dir, GLOB_CASE_INSENSITIVE) ||
          GlobMatch("textures-*.pk3", dir, GLOB_CASE_INSENSITIVE) ||
          GlobMatch("*/common/*", filename, GLOB_CASE_INSENSITIVE) ||

          // If the file comes from the official game data, and is not in a pk3,
          // skip it. This allows us to rebuild our official maps easily, but without
          // pulling in flares, envmaps, etc.

          (!q_strncmp(dir, PKGDATADIR, q_strlen(PKGDATADIR)) && !HasSuffix(dir, ".pk3"))) {

            Com_Print("[S] %s (%s)\n", filename, dir);
            continue;
        }
      }

      void *buffer;
      const int64_t len = Fs_Load(filename, &buffer);
      if (len == -1) {
        Com_Warn("Failed to read %s\n", filename);
        continue;
      }

      if (!mz_zip_writer_add_mem(&zip, filename, buffer, len, MZ_DEFAULT_COMPRESSION)) {
        Com_Error(ERROR_FATAL, "Failed to add %s to %s: %s\n",
              filename,
              path,
              mz_zip_get_error_string(mz_zip_get_last_error(&zip)));
      }

      Com_Print("[A] %s\n", filename);

      Fs_Free(buffer);
    }

    if (!mz_zip_writer_finalize_archive(&zip)) {
      Com_Error(ERROR_FATAL, "Failed to finalize %s: %s\n",
            path,
            mz_zip_get_error_string(mz_zip_get_last_error(&zip)));
    }
  } else {
    Com_Error(ERROR_FATAL, "Failed to open %s: %s\n",
          path,
          mz_zip_get_error_string(mz_zip_get_last_error(&zip)));
  }

  release(assets);

  const uint32_t end = (uint32_t) SDL_GetTicks();
  Com_Print("\nWrote %s in %d ms\n", path, end - start);

  if (update_zip) {
    const char *existing = va("map-%s.pk3", map_base);

    if (Fs_Exists(existing)) {
      const char *dir = Fs_RealDir(existing);

      if (dir) {
        char to_update[MAX_OS_PATH];
        q_snprintf(to_update, sizeof(to_update), "%s/%s", dir, existing);

        rename(path, to_update);
        Com_Print("Renamed %s to %s\n", path, to_update);
      } else {
        Com_Warn("Can't update %s: Failed to resolve real path\n", existing);
      }
    } else {
      Com_Warn("Can't update %s: file not found\n", existing);
    }
  }

  return 0;
}
