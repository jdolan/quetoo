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

#include <SDL3/SDL_misc.h>
#include <SDL3/SDL_mutex.h>

#include <glib/gstdio.h>

#include "collision/cm_manifest.h"
#include "console.h"
#include "filesystem.h"
#include "installer.h"
#include "net/net_http.h"

/**
 * @brief The installer type.
 * @details The installer runs a dedicated thread that steps through the `installer_state_t`
 * lifecycle. `Installer_Main` is called on the main thread via `Init`, which pumps an
 * `Installer_FrameFunction` in loop to show progress. When the `Installer_Main` returns, the
 * standard `Frame` loop begins.
 */
static struct {
  /**
   * @brief Enforces mutex across the main thread, installer thread, and download threads.
   */
  SDL_Mutex *mutex;

  /**
   * @brief The installer thread that advances the state machine.
   */
  SDL_Thread *thread;

  /**
   * @brief The remote data manifest.
   */
  GHashTable *remote_manifest;

  /**
   * @brief The local data manifest.
   */
  GHashTable *local_manifest;

  /**
   * @brief The installer status, used to expose progress via `Installer_FrameFunction`.
   */
  installer_status_t status;
} installer;

/**
 * @brief Performs a blocking HTTP GET and parses an integer from the response body.
 */
static int32_t Installer_GetVersion(const char *version_url) {
  void *body;
  size_t length;
  int32_t version;

  const int32_t status = Net_HttpGet(version_url, &body, &length);
  if (status == 200) {
    version = (int32_t) strtol((char *) body, NULL, 10);
    Com_Debug(DEBUG_COMMON, "%s == %d\n", version_url, version);
  } else {
    Com_Warn("%s: HTTP %d\n", version_url, status);
    if (length) {
      Com_Debug(DEBUG_COMMON, "%s\n", (char *) body);
    }
    version = -1;
  }

  Mem_Free(body);
  return version;
}

/**
 * @brief Prunes stale files and writes the updated manifest on a successful update.
 * @details Must be called from the installer thread without holding the mutex.
 */
static void Installer_Commit(void) {

  if (installer.local_manifest) {
    GHashTableIter iter;
    gpointer key, val;
    g_hash_table_iter_init(&iter, installer.local_manifest);
    while (g_hash_table_iter_next(&iter, &key, &val)) {
      const cm_manifest_entry_t *entry = val;
      if (entry->status == ENTRY_STALE) {
        char full_path[MAX_OS_PATH];
        g_snprintf(full_path, sizeof(full_path), "%s%c%s%c%s", Fs_DataDir(), G_DIR_SEPARATOR, game->string, G_DIR_SEPARATOR, entry->path);
        if (g_remove(full_path) != 0) {
          Com_Warn("Failed to remove stale file: %s\n", full_path);
        } else {
          Com_Debug(DEBUG_COMMON, "Pruned stale file: %s\n", entry->path);
        }
      }
    }
    Cm_FreeManifest(installer.local_manifest);
    installer.local_manifest = NULL;
  }

  if (installer.remote_manifest) {
    char mf_path[MAX_OS_PATH];
    g_snprintf(mf_path, sizeof(mf_path), "%s%c%s%cmanifest.mf", Fs_DataDir(), G_DIR_SEPARATOR, game->string, G_DIR_SEPARATOR);

    FILE *f = g_fopen(mf_path, "wb");
    if (f) {
      GList *keys = g_hash_table_get_keys(installer.remote_manifest);
      keys = g_list_sort(keys, (GCompareFunc) g_strcmp0);
      for (GList *k = keys; k; k = k->next) {
        const cm_manifest_entry_t *entry = g_hash_table_lookup(installer.remote_manifest, k->data);
        fprintf(f, "%s %" G_GINT64_FORMAT " %s\n", entry->hash, entry->size, entry->path);
      }
      g_list_free(keys);
      fclose(f);
    } else {
      Com_Warn("Failed to write manifest: %s\n", mf_path);
    }

    Cm_FreeManifest(installer.remote_manifest);
    installer.remote_manifest = NULL;
  }
}

/**
 * @brief Downloads a single data file to the data directory.
 * @return True on success, false on failure.
 */
static bool Installer_DownloadFile(const cm_manifest_entry_t *entry) {

  gchar *encoded = g_uri_escape_string(entry->path, "/", FALSE);
  char url[MAX_OS_PATH * 2];
  g_snprintf(url, sizeof(url), QUETOO_DATA_BASE_URL "/%s/%s", game->string, encoded);
  g_free(encoded);

  void *data = NULL;
  size_t length = 0;

  const int32_t status = Net_HttpGet(url, &data, &length);
  if (status != 200) {
    Com_Warn("Downloading %s failed: HTTP %d\n", url, status);
    Mem_Free(data);
    return false;
  }

  char path[MAX_OS_PATH];
  g_snprintf(path, sizeof(path), "%s%c%s%c%s", Fs_DataDir(), G_DIR_SEPARATOR, game->string, G_DIR_SEPARATOR, entry->path);

  gchar *dir = g_path_get_dirname(path);
  const int mkdir_result = g_mkdir_with_parents(dir, 0755);
  g_free(dir);
  if (mkdir_result != 0) {
    Com_Warn("Failed to create directory for: %s\n", path);
    Mem_Free(data);
    return false;
  }

  FILE *f = g_fopen(path, "wb");
  if (!f) {
    Com_Warn("Failed to open for writing: %s\n", path);
    Mem_Free(data);
    return false;
  }

  const size_t written = fwrite(data, 1, length, f);
  fclose(f);
  Mem_Free(data);

  if (written != length) {
    Com_Warn("Failed to write %s: %zu of %zu bytes\n", path, written, length);
    return false;
  }

  return true;
}

/**
 * @brief Worker thread for parallel downloads. Iterates the remote manifest for
 * PENDING entries, claims each by marking it CURRENT, then downloads it.
 */
static int Installer_DownloadThread(void *unused) {

  installer_status_t *in = &installer.status;

  while (true) {

    SDL_LockMutex(installer.mutex);

    if (in->state != INSTALLER_DOWNLOADING) {
      SDL_UnlockMutex(installer.mutex);
      break;
    }

    const cm_manifest_entry_t *entry = NULL;
    GHashTableIter iter;
    gpointer key, val;
    g_hash_table_iter_init(&iter, installer.remote_manifest);
    while (g_hash_table_iter_next(&iter, &key, &val)) {
      cm_manifest_entry_t *e = val;
      if (e->status == ENTRY_PENDING) {
        e->status = ENTRY_DOWNLOADING;
        g_strlcpy(in->current_file, e->path, sizeof(in->current_file));
        entry = e;
        break;
      }
    }

    SDL_UnlockMutex(installer.mutex);

    if (!entry) {
      break;
    }

    const bool ok = Installer_DownloadFile(entry);

    SDL_LockMutex(installer.mutex);

    if (ok) {
      in->files_done++;
      in->kbytes_done += (int32_t) ((entry->size + 1023) / 1024);
      ((cm_manifest_entry_t *) entry)->status = ENTRY_CURRENT;
    } else if (in->state == INSTALLER_DOWNLOADING) {
      in->state = INSTALLER_ERROR;
      g_snprintf(in->error, sizeof(in->error), "Download failed: %s", entry->path);
    }

    SDL_UnlockMutex(installer.mutex);
  }

  return 0;
}

/**
 * @brief `ThreadFunc` for the installer.
 */
static int Installer_Thread(void *unused) {

  installer_status_t *in = &installer.status;

  bool run = true;
  while (run) {

    switch (in->state) {

      case INSTALLER_CHECKING: {
        const int32_t v = Installer_GetVersion(QUETOO_VERSION_URL);
        SDL_LockMutex(installer.mutex);
        if (v < 0) {
          in->state = INSTALLER_ERROR;
          g_snprintf(in->error, sizeof(in->error), "Failed to fetch %s", QUETOO_VERSION_URL);
        } else {
          in->bin_version = v;
          in->state = in->bin_version > version->integer ? INSTALLER_UPDATE_AVAILABLE : INSTALLER_COMPARING;
        }
        SDL_UnlockMutex(installer.mutex);
      }
        break;

      case INSTALLER_COMPARING: {
        void *data = NULL;
        size_t length = 0;
        char manifest_url[MAX_OS_PATH];
        g_snprintf(manifest_url, sizeof(manifest_url), QUETOO_DATA_BASE_URL "/%s/manifest.mf", game->string);
        const int32_t http_status = Net_HttpGet(manifest_url, &data, &length);
        if (http_status != 200 || !data) {
          SDL_LockMutex(installer.mutex);
          in->state = INSTALLER_ERROR;
          g_snprintf(in->error, sizeof(in->error), "Failed to fetch manifest: HTTP %d", http_status);
          SDL_UnlockMutex(installer.mutex);
          Mem_Free(data);
          break;
        }

        GHashTable *remote = Cm_ParseManifest((const char *) data, length);
        Mem_Free(data);

        {
          GHashTableIter iter;
          gpointer key, val;
          g_hash_table_iter_init(&iter, remote);
          while (g_hash_table_iter_next(&iter, &key, &val)) {
            ((cm_manifest_entry_t *) val)->status = ENTRY_PENDING;
          }
        }

        GHashTable *local = Cm_ReadManifest("manifest.mf");

        if (local) {
          GHashTableIter iter;
          gpointer key, val;
          g_hash_table_iter_init(&iter, local);
          while (g_hash_table_iter_next(&iter, &key, &val)) {
            ((cm_manifest_entry_t *) val)->status = ENTRY_STALE;
          }
        }

        int32_t files_total = 0;
        int32_t kbytes_total = 0;
        {
          GHashTableIter iter;
          gpointer key, val;
          g_hash_table_iter_init(&iter, remote);
          while (g_hash_table_iter_next(&iter, &key, &val)) {
            cm_manifest_entry_t *re = val;
            const cm_manifest_entry_t *le = local ? g_hash_table_lookup(local, re->path) : NULL;
            if (le) {
              ((cm_manifest_entry_t *) le)->status = ENTRY_CURRENT;
              if (g_strcmp0(le->hash, re->hash) == 0) {
                re->status = ENTRY_CURRENT;
              }
            }
            if (re->status == ENTRY_PENDING) {
              files_total++;
              kbytes_total += (int32_t) ((re->size + 1023) / 1024);
            }
          }
        }

        installer.remote_manifest = remote;
        installer.local_manifest = local;

        SDL_LockMutex(installer.mutex);
        if (files_total == 0) {
          in->state = INSTALLER_COMMITTING;
        } else {
          in->state = INSTALLER_DOWNLOADING;
          in->files_total = files_total;
          in->kbytes_total = kbytes_total;
          in->files_done = 0;
          in->kbytes_done = 0;
        }
        SDL_UnlockMutex(installer.mutex);
      }
        break;

      case INSTALLER_DOWNLOADING: {
        SDL_Thread *threads[8];
        for (size_t i = 0; i < lengthof(threads); i++) {
          threads[i] = SDL_CreateThread(Installer_DownloadThread, "Installer_DownloadThread", NULL);
        }
        for (size_t i = 0; i < lengthof(threads); i++) {
          SDL_WaitThread(threads[i], NULL);
        }
        SDL_LockMutex(installer.mutex);
        if (in->state == INSTALLER_DOWNLOADING) {
          in->state = INSTALLER_COMMITTING;
        }
        SDL_UnlockMutex(installer.mutex);
      }
        break;

      case INSTALLER_COMMITTING:
        Installer_Commit();
        SDL_LockMutex(installer.mutex);
        in->state = INSTALLER_DONE;
        SDL_UnlockMutex(installer.mutex);
        break;

      case INSTALLER_UPDATE_AVAILABLE:
      case INSTALLER_CANCELLED:
      case INSTALLER_DONE:
      case INSTALLER_ERROR:
        run = false;
        break;
    }
  }

  return 0;
}

/**
 * @brief Starts an asynchronous data update and blocks until it completes,
 * calling @c frame each iteration while the installer is in progress.
 */
void Installer_Init(Installer_FrameFunction frame) {

  if (version->integer == -1) {
    return;
  }

  memset(&installer, 0, sizeof(installer));
  installer.status.state = INSTALLER_CHECKING;

  installer.mutex = SDL_CreateMutex();
  assert(installer.mutex);

  installer.thread = SDL_CreateThread(Installer_Thread, "installer", &installer);
  assert(installer.thread);

  installer_status_t *in = &installer.status;

  while (true) {
    installer_status_t s;

    SDL_LockMutex(installer.mutex);
    s = *in;
    SDL_UnlockMutex(installer.mutex);

    if (frame(&s)) {
      break;
    }

    SDL_Delay(QUETOO_TICK_MILLIS); // 40Hz should be plenty for progress bar updates etc
  }

  SDL_LockMutex(installer.mutex);
  if (in->state < INSTALLER_DONE) {
    in->state = INSTALLER_CANCELLED;
  }
  SDL_UnlockMutex(installer.mutex);

  SDL_WaitThread(installer.thread, NULL);
  installer.thread = NULL;

  SDL_DestroyMutex(installer.mutex);
  installer.mutex = NULL;
}

/**
 * @brief Cancels any in-progress data update and waits for it to finish.
 */
void Installer_Shutdown(void) {

  if (installer.thread) {
    SDL_LockMutex(installer.mutex);
    if (installer.status.state < INSTALLER_DONE) {
      installer.status.state = INSTALLER_CANCELLED;
    }
    SDL_UnlockMutex(installer.mutex);

    SDL_WaitThread(installer.thread, NULL);
    installer.thread = NULL;

    SDL_DestroyMutex(installer.mutex);
    installer.mutex = NULL;
  }

  Cm_FreeManifest(installer.remote_manifest);
  installer.remote_manifest = NULL;

  Cm_FreeManifest(installer.local_manifest);
  installer.local_manifest = NULL;
}

