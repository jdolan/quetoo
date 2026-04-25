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

#include "console.h"
#include "filesystem.h"
#include "installer.h"
#include "installer-s3.h"
#include "net/net_http.h"

static struct {

  SDL_Mutex *mutex;
  SDL_Thread *thread;

  installer_status_t status;

  GList *pending;
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
 * @return True if the object should be [re]downloaded, false if it is up to date.
 */
static bool Installer_IsPending(const s3_object_t *obj) {

  char path[MAX_OS_PATH];
  g_snprintf(path, sizeof(path), "%s%c%s", Fs_DataDir(), G_DIR_SEPARATOR, obj->key);

  bool result = true;

  FILE *f = g_fopen(path, "rb");
  if (f) {
    if (strchr(obj->etag, '-') == NULL) {
      GChecksum *checksum = g_checksum_new(G_CHECKSUM_MD5);
      uint8_t buf[65536];
      size_t n;
      while ((n = fread(buf, 1, sizeof(buf), f)) > 0) {
        g_checksum_update(checksum, buf, (gssize) n);
      }
      char md5[MAX_QPATH];
      g_strlcpy(md5, g_checksum_get_string(checksum), sizeof(md5));
      g_checksum_free(checksum);
      result = g_strcmp0(md5, obj->etag);
    } else {
      fseek(f, 0, SEEK_END);
      result = ftell(f) != obj->size;
    }
  }

  fclose(f);
  return result;
}

/**
 * @brief Worker thread for parallel downloads. Pops items from the shared
 * queue until it is empty or a download failure is recorded.
 */
static int Installer_DownloadThread(void *data) {

  installer_status_t *in = &installer.status;

  while (true) {

    SDL_LockMutex(installer.mutex);

    if (installer.pending == NULL) {
      SDL_UnlockMutex(installer.mutex);
      break;
    }

    GList *item = installer.pending;
    installer.pending = item->next;

    const s3_object_t *obj = item->data;
    g_strlcpy(in->current_file, obj->key, sizeof(in->current_file));

    SDL_UnlockMutex(installer.mutex);

    const bool ok = S3_GetObject(obj);

    SDL_LockMutex(installer.mutex);

    if (ok) {
      in->files_done++;
      in->kbytes_done += (int32_t) ((obj->size + 1023) / 1024);
    } else if (in->state == INSTALLER_DOWNLOADING_DATA) {
      in->state = INSTALLER_ERROR;
      g_snprintf(in->error, sizeof(in->error), "Download failed: %s", obj->key);

      g_list_free_full(installer.pending, Mem_Free);
      installer.pending = NULL;
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

      case INSTALLER_INIT_BIN:
        SDL_LockMutex(installer.mutex);
        if (version->integer == -1) {
          in->state = INSTALLER_DONE;
        } else {
          in->state = INSTALLER_CHECKING_BIN;
        }
        SDL_UnlockMutex(installer.mutex);
        break;

      case INSTALLER_CHECKING_BIN: {
        const int32_t v = Installer_GetVersion(QUETOO_VERSION_URL);
        SDL_LockMutex(installer.mutex);
        if (v < 0) {
          in->state = INSTALLER_ERROR;
          g_snprintf(in->error, sizeof(in->error), "Failed to fetch %s", QUETOO_VERSION_URL);
        } else {
          in->versions.bin = v;
          if (in->versions.bin < version->integer) {
            in->state = INSTALLER_UPDATE_AVAILABLE_BIN;
          } else {
            in->state = INSTALLER_INIT_DATA;
          }
        }
        SDL_UnlockMutex(installer.mutex);
      }
        break;

      case INSTALLER_INIT_DATA:
        SDL_LockMutex(installer.mutex);
        if (data_version->integer == -1) {
          in->state = INSTALLER_DONE;
        } else {
          in->state = INSTALLER_CHECKING_DATA;
        }
        SDL_UnlockMutex(installer.mutex);
        break;

      case INSTALLER_CHECKING_DATA: {
        const int32_t v = Installer_GetVersion(QUETOO_DATA_VERSION_URL);
        SDL_LockMutex(installer.mutex);
        if (v < 0) {
          in->state = INSTALLER_ERROR;
          g_snprintf(in->error, sizeof(in->error), "Failed to fetch %s", QUETOO_DATA_VERSION_URL);
        } else {
          in->versions.data = v;
          if (in->versions.data < data_version->integer) {
            in->state = INSTALLER_COMPARING_DATA;
          } else {
            in->state = INSTALLER_DONE;
          }
        }
        SDL_UnlockMutex(installer.mutex);
      }
        break;

      case INSTALLER_COMPARING_DATA: {
        GList *list = S3_ListObjects("quetoo-data");
        int32_t kbytes_total = 0;

        GList *it = list;
        while (it) {
          GList *next = it->next;
          const s3_object_t *obj = it->data;
          if (Installer_IsPending(obj)) {
            kbytes_total += (int32_t) ((obj->size + 1023) / 1024);
          } else {
            list = g_list_remove_link(list, it);
            g_list_free_full(it, Mem_Free);
          }
          it = next;
        }

        SDL_LockMutex(installer.mutex);
        installer.pending = list;
        if (installer.pending == NULL) {
          in->state = INSTALLER_DONE;
        } else {
          in->state = INSTALLER_DOWNLOADING_DATA;
          in->files_total = (int32_t) g_list_length(installer.pending);
          in->kbytes_total = kbytes_total;
          in->files_done = 0;
          in->kbytes_done = 0;
        }
        SDL_UnlockMutex(installer.mutex);
      }
        break;

      case INSTALLER_DOWNLOADING_DATA: {
        SDL_Thread *threads[8];
        for (size_t i = 0; i < lengthof(threads); i++) {
          threads[i] = SDL_CreateThread(Installer_DownloadThread, "Installer_DownloadThread", NULL);
        }
        for (size_t i = 0; i < lengthof(threads); i++) {
          SDL_WaitThread(threads[i], NULL);
        }
        SDL_LockMutex(installer.mutex);
        if (in->state == INSTALLER_DOWNLOADING_DATA) {
          in->state = INSTALLER_DONE;
        } else {
          // We already have our error
        }
        g_list_free_full(installer.pending, Mem_Free);
        SDL_UnlockMutex(installer.mutex);
      }
        break;

      case INSTALLER_UPDATE_AVAILABLE_BIN:
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

  SDL_DestroyMutex(installer.mutex);
}

/**
 * @brief Cancels any in-progress data update and waits for it to finish.
 */
void Installer_Shutdown(void) {


  // TODO: Lock mutex, cancel installer so that thread terminates, wait?

}

