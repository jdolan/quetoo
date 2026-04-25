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

static struct {

  SDL_Mutex *mutex;
  SDL_Thread *thread;

  installer_status_t status;  ///< Shared state; always access under mutex.

  struct {
    GList *pending;      ///< cm_manifest_entry_t * remaining to download
    GList *prune;        ///< g_strdup'd paths to remove on commit
    char *manifest;      ///< Raw remote manifest bytes to write on commit
    size_t manifest_len;
  } locals;              ///< Installer thread-private; never touch from outside.

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
 * @brief Prunes stale files and writes the cached manifest on a successful update.
 * @details Must be called from the installer thread without holding the mutex.
 */
static void Installer_Commit(void) {

  for (GList *e = installer.locals.prune; e; e = e->next) {
    const char *path = e->data;
    char full_path[MAX_OS_PATH];
    g_snprintf(full_path, sizeof(full_path), "%s%c%s", Fs_DataDir(), G_DIR_SEPARATOR, path);
    if (g_remove(full_path) != 0) {
      Com_Warn("Failed to remove stale file: %s\n", full_path);
    } else {
      Com_Debug(DEBUG_COMMON, "Pruned stale file: %s\n", path);
    }
  }
  g_list_free_full(installer.locals.prune, g_free);
  installer.locals.prune = NULL;

  if (installer.locals.manifest) {
    char mf_path[MAX_OS_PATH];
    g_snprintf(mf_path, sizeof(mf_path), "%s%cmanifest.mf", Fs_DataDir(), G_DIR_SEPARATOR);
    FILE *f = g_fopen(mf_path, "wb");
    if (f) {
      fwrite(installer.locals.manifest, 1, installer.locals.manifest_len, f);
      fclose(f);
    } else {
      Com_Warn("Failed to write manifest: %s\n", mf_path);
    }
    Mem_Free(installer.locals.manifest);
    installer.locals.manifest = NULL;
  }
}

/**
 * @brief Downloads a single data file to the data directory.
 * @return True on success, false on failure.
 */
static bool Installer_DownloadFile(const cm_manifest_entry_t *entry) {

  gchar *encoded = g_uri_escape_string(entry->path, "/", FALSE);
  char url[MAX_OS_PATH * 2];
  g_snprintf(url, sizeof(url), QUETOO_DATA_BASE_URL "/%s", encoded);
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
  g_snprintf(path, sizeof(path), "%s%c%s", Fs_DataDir(), G_DIR_SEPARATOR, entry->path);

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
 * @brief Worker thread for parallel downloads. Pops items from the shared
 * queue until it is empty, cancelled, or a download failure is recorded.
 */
static int Installer_DownloadThread(void *data) {

  installer_status_t *in = &installer.status;

  while (true) {

    SDL_LockMutex(installer.mutex);

    if (installer.locals.pending == NULL || in->state == INSTALLER_CANCELLED) {
      SDL_UnlockMutex(installer.mutex);
      break;
    }

    GList *item = installer.locals.pending;
    installer.locals.pending = item->next;

    const cm_manifest_entry_t *entry = item->data;
    g_strlcpy(in->current_file, entry->path, sizeof(in->current_file));

    SDL_UnlockMutex(installer.mutex);

    const bool ok = Installer_DownloadFile(entry);

    SDL_LockMutex(installer.mutex);

    if (ok) {
      in->files_done++;
      in->kbytes_done += (int32_t) ((entry->size + 1023) / 1024);
    } else if (in->state == INSTALLER_DOWNLOADING) {
      in->state = INSTALLER_ERROR;
      g_snprintf(in->error, sizeof(in->error), "Download failed: %s", entry->path);

      if (installer.locals.pending) {
        g_list_free_full(installer.locals.pending, Mem_Free);
        installer.locals.pending = NULL;
      }
    }

    SDL_UnlockMutex(installer.mutex);

    g_list_free_full(item, Mem_Free);
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
        if (version->integer == -1) {
          SDL_LockMutex(installer.mutex);
          in->state = INSTALLER_DONE;
          SDL_UnlockMutex(installer.mutex);
          break;
        }
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
        if (data_version->integer == -1) {
          SDL_LockMutex(installer.mutex);
          in->state = INSTALLER_DONE;
          SDL_UnlockMutex(installer.mutex);
          break;
        }

        void *data = NULL;
        size_t length = 0;
        const int32_t http_status = Net_HttpGet(QUETOO_DATA_MANIFEST_URL, &data, &length);
        if (http_status != 200 || !data) {
          SDL_LockMutex(installer.mutex);
          in->state = INSTALLER_ERROR;
          g_snprintf(in->error, sizeof(in->error), "Failed to fetch manifest: HTTP %d", http_status);
          SDL_UnlockMutex(installer.mutex);
          Mem_Free(data);
          break;
        }

        GList *remote = Cm_ParseManifest((const char *) data, length);

        GHashTable *remote_table = g_hash_table_new(g_str_hash, g_str_equal);
        for (GList *e = remote; e; e = e->next) {
          const cm_manifest_entry_t *entry = e->data;
          g_hash_table_insert(remote_table, (gpointer) entry->path, (gpointer) entry);
        }

        GHashTable *local_table = g_hash_table_new(g_str_hash, g_str_equal);
        GList *local = Cm_ReadManifest("manifest.mf");
        for (GList *e = local; e; e = e->next) {
          const cm_manifest_entry_t *entry = e->data;
          g_hash_table_insert(local_table, (gpointer) entry->path, (gpointer) entry);
        }

        GList *pending = NULL;
        int32_t kbytes_total = 0;
        for (GList *e = remote; e; e = e->next) {
          cm_manifest_entry_t *entry = e->data;
          const cm_manifest_entry_t *local_entry = g_hash_table_lookup(local_table, entry->path);
          if (!local_entry || g_strcmp0(local_entry->hash, entry->hash) != 0) {
            pending = g_list_append(pending, entry);
            kbytes_total += (int32_t) ((entry->size + 1023) / 1024);
          } else {
            Mem_Free(entry);
          }
        }
        g_list_free(remote);

        GList *prune = NULL;
        for (GList *e = local; e; e = e->next) {
          const cm_manifest_entry_t *entry = e->data;
          if (!g_hash_table_contains(remote_table, entry->path)) {
            prune = g_list_append(prune, g_strdup(entry->path));
          }
        }

        g_hash_table_destroy(remote_table);
        g_hash_table_destroy(local_table);
        Cm_FreeManifest(local);

        installer.locals.pending = pending;
        installer.locals.prune = prune;
        installer.locals.manifest = data;
        installer.locals.manifest_len = length;

        SDL_LockMutex(installer.mutex);
        if (installer.locals.pending == NULL) {
          in->state = INSTALLER_COMMITTING;
        } else {
          in->state = INSTALLER_DOWNLOADING;
          in->files_total = (int32_t) g_list_length(installer.locals.pending);
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
        } else {
          g_list_free_full(installer.locals.prune, g_free);
          installer.locals.prune = NULL;
          Mem_Free(installer.locals.manifest);
          installer.locals.manifest = NULL;
        }
        if (installer.locals.pending) {
          g_list_free_full(installer.locals.pending, Mem_Free);
          installer.locals.pending = NULL;
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

  g_list_free_full(installer.locals.pending, Mem_Free);
  installer.locals.pending = NULL;

  g_list_free_full(installer.locals.prune, g_free);
  installer.locals.prune = NULL;

  Mem_Free(installer.locals.manifest);
  installer.locals.manifest = NULL;
}

