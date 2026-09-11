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

#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_mutex.h>


#include "collision/cm_manifest.h"
#include "console.h"
#include "filesystem.h"
#include "installer.h"

#include <Objectively/Array.h>
#include <Objectively/Dictionary.h>
#include <Objectively/JSONContext.h>
#include <Objectively/Number.h>
#include <Objectively/RESTClient.h>
#include <Objectively/String.h>
#include <Objectively/URL.h>
#include <Objectively/URLResponse.h>
#include <Objectively/URLSession.h>
#include <Objectively/URLSessionDownloadTask.h>

#include "archive.h"

/**
 * @brief The release asset carrying this platform's engine build.
 * @details Not derived from `BUILD`, which is an autoconf host triplet
 * (`x86_64-pc-linux-gnu`) and does not match how the assets are named.
 */
#if defined(_WIN32)
  #define INSTALLER_ASSET "quetoo-x86_64-pc-windows.zip"
#elif defined(__APPLE__) && defined(__aarch64__)
  #define INSTALLER_ASSET "quetoo-arm64-apple-darwin.dmg"
#elif defined(__aarch64__)
  #define INSTALLER_ASSET "quetoo-aarch64-pc-linux.tar.gz"
#elif defined(__linux__)
  #define INSTALLER_ASSET "quetoo-x86_64-pc-linux.tar.gz"
#else
  #define INSTALLER_ASSET NULL
#endif

/**
 * @brief The latest release, as reported by the GitHub Releases API.
 */
typedef struct {
  char tag[64];
  char asset[MAX_QPATH];
  char url[MAX_OS_PATH * 2];
  int64_t size;
} installer_release_t;

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
  HashTable *remote_manifest;

  /**
   * @brief The local data manifest.
   */
  HashTable *local_manifest;

  /**
   * @brief The latest release, populated by `INSTALLER_CHECKING`.
   */
  installer_release_t release;

  /**
   * @brief Whether a cold install of the game data has been attempted, so that
   * a failure falls through to the file by file sync instead of retrying.
   */
  bool installed_data;

  /**
   * @brief Whether the player has agreed to install an available update.
   * @details Zero until answered, then 1 to accept or -1 to decline.
   */
  int32_t consent;

  /**
   * @brief The installer status, used to expose progress via `Installer_FrameFunction`.
   */
  installer_status_t status;
} installer;

/**
 * @brief Compares dotted release versions, e.g. `1.0.91` against `1.0.9`.
 * @details Compared component-wise rather than lexically, where `1.0.9` would
 * sort after `1.0.91`. A leading `v` is optional on either side: release tags
 * carry one and `--with-version` may or may not.
 * @return Negative, zero or positive as `a` orders before, with, or after `b`.
 */
static int32_t Installer_CompareVersions(const char *a, const char *b) {

  if (*a == 'v') { a++; }
  if (*b == 'v') { b++; }

  while (*a || *b) {

    char *ea = NULL, *eb = NULL;
    const long x = strtol(a, &ea, 10);
    const long y = strtol(b, &eb, 10);

    if (x != y) {
      return x < y ? -1 : 1;
    }

    if (ea == a && eb == b) {
      break;
    }

    a = *ea == '.' ? ea + 1 : ea;
    b = *eb == '.' ? eb + 1 : eb;
  }

  return 0;
}

/**
 * @brief Returns `object` if it is of `clazz`, else `NULL`.
 * @details The response is remote input and its shape is not guaranteed; a
 * root Array where a Dictionary is expected would otherwise be dispatched
 * through the wrong interface.
 */
static ident Installer_Cast(ident object, Class *clazz) {
  return object && $((Object *) object, isKindOfClass, clazz) ? object : NULL;
}

/**
 * @brief Queries a GitHub Releases API endpoint for its latest release and the
 * named asset.
 * @details `/releases/latest` excludes drafts and prereleases, and carries the
 * asset list in the same response, so one request yields everything needed.
 * The asset size is taken from the API rather than a `HEAD`, because
 * `RESTClient` discards response headers.
 */
static bool Installer_FetchRelease(const char *api, const char *want, installer_release_t *out) {

  const char *headers[] = {
    "Accept", "application/vnd.github+json",
    "X-GitHub-Api-Version", "2022-11-28",
    "User-Agent", "quetoo/" VERSION,
    NULL
  };

  Data *data = NULL;
  const int32_t status = $($$(RESTClient, sharedInstance), get, api, headers, &data);
  if (status != 200 || !data) {
    Com_Warn("%s: HTTP %d\n", api, status);
    release(data);
    return false;
  }

  JSONContext *context = $(alloc(JSONContext), init);
  ident object = $(context, objectFromData, data, 0);

  release(data);

  bool success = false;

  Dictionary *root = Installer_Cast(object, _Dictionary());

  if (root) {

    const String *tag = Installer_Cast($(root, objectForKeyPath, "tag_name"), _String());
    const Array *assets = Installer_Cast($(root, objectForKeyPath, "assets"), _Array());

    if (tag && assets) {

      q_strlcpy(out->tag, tag->chars, sizeof(out->tag));

      for (size_t i = 0; i < assets->count; i++) {

        const Dictionary *asset = Installer_Cast($(assets, objectAtIndex, i), _Dictionary());
        if (asset == NULL) {
          continue;
        }

        const String *name = Installer_Cast($(asset, objectForKeyPath, "name"), _String());
        if (name == NULL || q_strcmp(name->chars, want)) {
          continue;
        }

        const String *url = Installer_Cast($(asset, objectForKeyPath, "browser_download_url"), _String());
        const Number *size = Installer_Cast($(asset, objectForKeyPath, "size"), _Number());

        if (url && size) {
          q_strlcpy(out->asset, name->chars, sizeof(out->asset));
          q_strlcpy(out->url, url->chars, sizeof(out->url));
          out->size = (int64_t) size->value;
          success = true;
        }

        break;
      }
    }
  }

  release(object);
  release(context);

  if (success) {
    Com_Debug(DEBUG_COMMON, "Latest release %s, asset %s (%" PRId64 " bytes)\n",
              out->tag, out->asset, out->size);
  } else {
    Com_Warn("No %s in %s\n", want, api);
  }

  return success;
}

/**
 * @brief Recursively deletes `path`.
 * @details `SDL_RemovePath` only unlinks files and empty directories, but a
 * displaced application bundle is neither.
 */
static SDL_EnumerationResult Installer_RemoveEntry(void *data, const char *dir, const char *name) {

  char path[MAX_OS_PATH];
  q_snprintf(path, sizeof(path), "%s/%s", dir, name);

  SDL_PathInfo info;
  if (SDL_GetPathInfo(path, &info) && info.type == SDL_PATHTYPE_DIRECTORY) {
    SDL_EnumerateDirectory(path, Installer_RemoveEntry, data);
  }

  SDL_RemovePath(path);
  return SDL_ENUM_CONTINUE;
}

static void Installer_RemoveTree(const char *path) {

  SDL_PathInfo info;
  if (SDL_GetPathInfo(path, &info) && info.type == SDL_PATHTYPE_DIRECTORY) {
    SDL_EnumerateDirectory(path, Installer_RemoveEntry, NULL);
  }

  SDL_RemovePath(path);
}

/**
 * @brief The directory the staging area lives beside.
 * @remarks Empty when the executable is not laid out like an installation, as
 * in a source tree. Only engine updates depend on it; game content is written
 * to `Fs_DataDir()` and syncs as usual.
 * @details Everywhere but macOS this is the installation itself, and an update
 * replaces files within it. On macOS the installation *is* `Quetoo.app` and an
 * update replaces the whole bundle, so staging inside it would carry the staged
 * payload along when the bundle is renamed aside.
 */
static void Installer_StagingParent(char *out, size_t len) {

#if defined(__APPLE__)
  q_strlcpy(out, Fs_BaseDir(), len);

  char *slash = q_strrchr(out, '/');
  if (slash) {
    *slash = '\0';
  }
#else
  q_strlcpy(out, Fs_BaseDir(), len);
#endif
}

/**
 * @brief The directory holding a downloaded update until it is applied.
 */
static void Installer_PendingDir(char *out, size_t len) {

  char parent[MAX_OS_PATH];
  Installer_StagingParent(parent, sizeof(parent));

  q_snprintf(out, (int32_t) len, "%s/.quetoo-pending", parent);
}

/**
 * @brief Returns true if something other than us maintains this installation.
 * @details A storefront that installs and patches the game expects to be the
 * only thing doing so; updating underneath it leaves its copy out of step with
 * what it believes it installed. Distributions that manage their own updates
 * ship this marker beside the installation to say so.
 */
static bool Installer_IsManaged(const char *dir) {

  char path[MAX_OS_PATH];
  q_snprintf(path, sizeof(path), "%s/%s", dir, INSTALLER_MANAGED);

  return SDL_GetPathInfo(path, NULL);
}

/**
 * @brief Returns true if the installation directory can be written to.
 * @details Checked before downloading rather than after, so a user whose
 * install lives somewhere privileged is told immediately instead of after
 * pulling down an archive that can never be applied.
 */
static bool Installer_IsWritable(const char *dir) {

  char probe[MAX_OS_PATH];
  q_snprintf(probe, sizeof(probe), "%s/.writable", dir);

  FILE *file = fopen(probe, "wb");
  if (!file) {
    return false;
  }

  fclose(file);
  SDL_RemovePath(probe);
  return true;
}

/**
 * @brief Streams `url` to `path`, publishing progress and honoring cancellation.
 * @details The task is resumed rather than executed because
 * `URLSessionTask::execute` never inspects the task state, so a synchronous
 * request cannot be interrupted. These payloads are large enough that Cancel
 * would otherwise appear to hang.
 */
static bool Installer_DownloadToFile(const char *address, const char *path, int64_t expected) {

  FILE *file = fopen(path, "wb");
  if (!file) {
    Com_Warn("Failed to open %s for writing\n", path);
    return false;
  }

  URL *url = $(alloc(URL), initWithCharacters, address);
  if (!url) {
    Com_Warn("Failed to parse %s\n", address);
    fclose(file);
    SDL_RemovePath(path);
    return false;
  }

  URLSessionDownloadTask *download = $($$(URLSession, sharedInstance), downloadTaskWithURL, url, NULL);
  release(url);

  if (!download) {
    Com_Warn("Failed to request %s\n", address);
    fclose(file);
    SDL_RemovePath(path);
    return false;
  }

  download->file = file;

  URLSessionTask *task = (URLSessionTask *) download;
  $(task, resume);

  installer_status_t *in = &installer.status;

  while (task->state != URLSESSIONTASK_COMPLETED && task->state != URLSESSIONTASK_CANCELED) {

    SDL_LockMutex(installer.mutex);
    const bool cancelled = in->state == INSTALLER_CANCELLED;
    if (!cancelled) {
      const int64_t total = expected > 0 ? expected : (int64_t) task->bytesExpectedToReceive;
      in->kbytes_done = (int32_t) (task->bytesReceived / 1024);
      in->kbytes_total = (int32_t) (total / 1024);
    }
    SDL_UnlockMutex(installer.mutex);

    if (cancelled) {
      $(task, cancel);
      break;
    }

    SDL_Delay(QUETOO_TICK_MILLIS);
  }

  while (task->state != URLSESSIONTASK_COMPLETED && task->state != URLSESSIONTASK_CANCELED) {
    SDL_Delay(QUETOO_TICK_MILLIS);
  }

  fclose(file);

  const int32_t http = task->response ? task->response->httpStatusCode : 0;
  bool success = task->state == URLSESSIONTASK_COMPLETED && http == 200;

  if (success && expected > 0 && (int64_t) task->bytesReceived != expected) {
    Com_Warn("Truncated download of %s: %" PRId64 " of %" PRId64 " bytes\n",
             address, (int64_t) task->bytesReceived, expected);
    success = false;
  }

  release(task);

  if (!success) {
    Com_Warn("Failed to download %s: HTTP %d\n", address, http);
    SDL_RemovePath(path);
  }

  return success;
}

#if !defined(__APPLE__)

/**
 * @brief Recursively records every staged file.
 */
static SDL_EnumerationResult Installer_EnumeratePending(void *data, const char *dir, const char *name) {

  FILE *file = data;

  char path[MAX_OS_PATH];
  q_snprintf(path, sizeof(path), "%s/%s", dir, name);

  SDL_PathInfo info;
  if (!SDL_GetPathInfo(path, &info)) {
    return SDL_ENUM_CONTINUE;
  }

  if (info.type == SDL_PATHTYPE_DIRECTORY) {
    SDL_EnumerateDirectory(path, Installer_EnumeratePending, file);
  } else if (info.type == SDL_PATHTYPE_FILE &&
             q_strcmp(name, "pending.mf") && q_strcmp(name, "pending.tmp")) {
    fprintf(file, "%s\n", path);
  }

  return SDL_ENUM_CONTINUE;
}

#endif

/**
 * @brief Records what was staged, so the apply on exit needs no directory walk
 * and can tell a complete stage from a half-extracted one.
 * @details `root` is the directory within the staging area that mirrors the
 * installation: the archive's own top-level directory for a tarball, the
 * staging directory itself for a zip, and the bundle for a disk image.
 */
static void Installer_WritePending(const char *pending) {

  char root[MAX_OS_PATH];
#if defined(__APPLE__)
  q_snprintf(root, sizeof(root), "%s/Quetoo.app", pending);
#elif defined(_WIN32)
  q_strlcpy(root, pending, sizeof(root));
#else
  q_snprintf(root, sizeof(root), "%s/quetoo", pending);
#endif

  char path[MAX_OS_PATH], temp[MAX_OS_PATH];
  q_snprintf(path, sizeof(path), "%s/pending.mf", pending);
  q_snprintf(temp, sizeof(temp), "%s/pending.tmp", pending);

  FILE *file = fopen(temp, "wb");
  if (!file) {
    Com_Warn("Failed to write %s\n", temp);
    return;
  }

  fprintf(file, "%s\n%s\n", installer.release.tag, root);

#if !defined(__APPLE__)
  SDL_EnumerateDirectory(root, Installer_EnumeratePending, file);
#endif

  fclose(file);

  if (!SDL_RenamePath(temp, path)) {
    Com_Warn("Failed to write %s: %s\n", path, SDL_GetError());
    SDL_RemovePath(temp);
  }
}

/**
 * @brief Prunes stale files and writes the updated manifest on a successful update.
 * @details Must be called from the installer thread without holding the mutex.
 */
static void Installer_FindPending(const HashTable *table, ident key, ident value, ident data) {
  cm_manifest_entry_t **out = data;
  if (*out) { return; } // already found
  cm_manifest_entry_t *e = value;
  if (e->status == ENTRY_PENDING) {
    e->status = ENTRY_DOWNLOADING;
    *out = e;
  }
}

static void Installer_PruneStaleEntry(const HashTable *table, ident key, ident value, ident data) {
  const cm_manifest_entry_t *entry = value;
  if (entry->status == ENTRY_STALE) {
    char full_path[MAX_OS_PATH];
    q_snprintf(full_path, sizeof(full_path), "%s/%s/%s", Fs_DataDir(), Com_Game(), entry->path);
    if (SDL_RemovePath(full_path)) {
      Com_Debug(DEBUG_COMMON, "Pruned stale file: %s\n", entry->path);
    } else {
      Com_Warn("Failed to remove stale file: %s\n", full_path);
    }
  }
}

static void Installer_MarkPending(const HashTable *table, ident key, ident value, ident data) {
  ((cm_manifest_entry_t *) value)->status = ENTRY_PENDING;
}

static void Installer_MarkStale(const HashTable *table, ident key, ident value, ident data) {
  ((cm_manifest_entry_t *) value)->status = ENTRY_STALE;
}

static void Installer_WriteManifestEntry(const HashTable *table, ident key, ident value, ident data) {
  const cm_manifest_entry_t *entry = value;
  fprintf((FILE *) data, "%s %" PRId64 " %s\n", entry->hash, entry->size, entry->path);
}

/**
 * @brief Writes `manifest` directly to `path` on the real filesystem.
 * @details `Cm_WriteManifest` writes through PhysFS, which resolves relative
 * paths against `Fs_WriteDir()` -- the user's writable game directory -- not
 * `Fs_DataDir()`, where `path` actually lives. Using it here would silently
 * write the manifest to a bogus nested path under the write dir instead of
 * updating the real one, leaving the local manifest permanently stale and
 * causing already-downloaded files to be re-downloaded on every launch.
 */
static bool Installer_WriteManifest(const char *path, HashTable *manifest) {
  FILE *f = fopen(path, "wb");
  if (!f) {
    Com_Warn("Failed to open %s for writing\n", path);
    return false;
  }
  $(manifest, enumerate, Installer_WriteManifestEntry, f);
  fclose(f);
  return true;
}

typedef struct {
  HashTable *local;
  int32_t files_total;
  int32_t kbytes_total;
} installer_compare_t;

static void Installer_CompareEntry(const HashTable *table, ident key, ident value, ident data) {
  installer_compare_t *ctx = data;
  cm_manifest_entry_t *re = value;
  const cm_manifest_entry_t *le = ctx->local ? $(ctx->local, get, re->path) : NULL;
  if (le) {
    ((cm_manifest_entry_t *) le)->status = ENTRY_CURRENT;
    if (q_strcmp(le->hash, re->hash) == 0) {
      re->status = ENTRY_CURRENT;
    }
  }
  if (re->status == ENTRY_PENDING) {
    ctx->files_total++;
    ctx->kbytes_total += (int32_t) ((re->size + 1023) / 1024);
  }
}

static void Installer_Commit(void) {

  if (installer.local_manifest) {
    $(installer.local_manifest, enumerate, Installer_PruneStaleEntry, NULL);
    Cm_FreeManifest(installer.local_manifest);
    installer.local_manifest = NULL;
  }

  if (installer.remote_manifest) {
    char mf_path[MAX_OS_PATH];
    q_snprintf(mf_path, sizeof(mf_path), "%s/%s/manifest.mf", Fs_DataDir(), Com_Game());
    Installer_WriteManifest(mf_path, installer.remote_manifest);
    Cm_FreeManifest(installer.remote_manifest);
    installer.remote_manifest = NULL;
  }
}

/**
 * @brief Downloads a single data file to the data directory.
 * @return True on success, false on failure.
 */
static bool Installer_DownloadFile(const cm_manifest_entry_t *entry) {

  // URL-encode the path (pass-through '/' as safe)
  const char *src = entry->path;
  char encoded[MAX_OS_PATH * 3];
  char *dst = encoded;
  while (*src && dst < encoded + sizeof(encoded) - 4) {
    if (isalnum((unsigned char)*src) || *src == '-' || *src == '_' || *src == '.' || *src == '~' || *src == '/') {
      *dst++ = *src;
    } else {
      dst += q_snprintf(dst, 4, "%%%02X", (unsigned char)*src);
    }
    src++;
  }
  *dst = '\0';

  char url[MAX_OS_PATH * 2];
  q_snprintf(url, sizeof(url), QUETOO_DATA_BASE_URL "/%s/%s", Com_Game(), encoded);

  Data *data = NULL;

  const int32_t status = $($$(RESTClient, sharedInstance), get, url, NULL, &data);
  if (status != 200) {
    Com_Warn("Downloading %s failed: HTTP %d\n", url, status);
    release(data);
    return false;
  }

  char path[MAX_OS_PATH];
  q_snprintf(path, sizeof(path), "%s/%s/%s", Fs_DataDir(), Com_Game(), entry->path);

  {
    char dir[MAX_OS_PATH];
    q_strlcpy(dir, path, sizeof(dir));
    char *slash = q_strrchr(dir, '/');
    if (slash) {
      *slash = '\0';
    }
    if (!SDL_CreateDirectory(dir)) {
      Com_Warn("Failed to create directory for: %s\n", path);
      release(data);
      return false;
    }
  }

  FILE *f = fopen(path, "wb");
  if (!f) {
    Com_Warn("Failed to open for writing: %s\n", path);
    release(data);
    return false;
  }

  const size_t written = fwrite(data->bytes, 1, data->length, f);
  fclose(f);

  if (written != data->length) {
    Com_Warn("Failed to write %s: %zu of %zu bytes\n", path, written, data->length);
    release(data);
    return false;
  }

  release(data);
  return true;
}

/**
 * @brief Returns true if the game data manifest is present on disk.
 */
static bool Installer_HasManifest(void) {

  char path[MAX_OS_PATH];
  q_snprintf(path, sizeof(path), "%s/%s/manifest.mf", Fs_DataDir(), Com_Game());

  return SDL_GetPathInfo(path, NULL);
}

/**
 * @brief Reads the installed data manifest directly from the filesystem.
 * @details Not `Cm_ReadManifest`, which resolves through PhysFS: the data
 * directory is only mounted if it existed when `Fs_Init` ran, so a tree this
 * installer just created is invisible to it. That would leave every entry
 * pending and re-download the whole data set a file at a time -- exactly what
 * the archive install exists to avoid. `Installer_WriteManifest` bypasses
 * PhysFS for the same reason.
 */
static HashTable *Installer_ReadManifest(void) {

  char path[MAX_OS_PATH];
  q_snprintf(path, sizeof(path), "%s/%s/manifest.mf", Fs_DataDir(), Com_Game());

  FILE *file = fopen(path, "rb");
  if (!file) {
    return NULL;
  }

  fseek(file, 0, SEEK_END);
  const long length = ftell(file);
  fseek(file, 0, SEEK_SET);

  HashTable *manifest = NULL;

  if (length > 0) {
    char *data = Mem_Malloc((size_t) length);
    if (fread(data, 1, (size_t) length, file) == (size_t) length) {
      manifest = Cm_ParseManifest(data, (size_t) length);
    }
    Mem_Free(data);
  }

  fclose(file);
  return manifest;
}

/**
 * @brief Installs the whole game data set from its release archive.
 * @details A fresh install would otherwise pull eleven thousand files one at a
 * time from S3. The archive is published on GitHub Releases, whose egress is
 * free, so the cold start costs nothing to serve; the per-file sync is left to
 * carry the small differences between releases.
 * @remarks A partial extraction is left in place rather than unwound, but its
 * manifest is discarded: the per-file sync compares against that manifest and
 * would otherwise treat files it never wrote as already current.
 */
static bool Installer_InstallData(void) {

  installer_status_t *in = &installer.status;

  installer_release_t data;
  if (!Installer_FetchRelease(QUETOO_DATA_API_URL, QUETOO_DATA_ARCHIVE, &data)) {
    return false;
  }

  char archive[MAX_OS_PATH];
  q_snprintf(archive, sizeof(archive), "%s/%s", Fs_DataDir(), QUETOO_DATA_ARCHIVE);

  if (!SDL_CreateDirectory(Fs_DataDir())) {
    Com_Warn("Failed to create %s: %s\n", Fs_DataDir(), SDL_GetError());
    return false;
  }

  SDL_LockMutex(installer.mutex);
  in->state = INSTALLER_INSTALLING_DATA;
  in->kbytes_done = 0;
  in->kbytes_total = (int32_t) (data.size / 1024);
  q_strlcpy(in->current_file, data.asset, sizeof(in->current_file));
  SDL_UnlockMutex(installer.mutex);

  bool success = Installer_DownloadToFile(data.url, archive, data.size);

  if (success) {
    success = Archive_Extract(archive, Fs_DataDir());
  }

  SDL_RemovePath(archive);

  if (!success) {
    char manifest[MAX_OS_PATH];
    q_snprintf(manifest, sizeof(manifest), "%s/%s/manifest.mf", Fs_DataDir(), Com_Game());
    SDL_RemovePath(manifest);
  }

  SDL_LockMutex(installer.mutex);
  if (in->state == INSTALLER_INSTALLING_DATA) {
    in->state = INSTALLER_COMPARING;
  }
  SDL_UnlockMutex(installer.mutex);

  return success;
}

/**
 * @brief Worker thread for parallel downloads. Iterates the remote manifest for
 * `PENDING` entries, claims each by marking it `CURRENT`, then downloads it.
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
    {
      cm_manifest_entry_t *found = NULL;
      $(installer.remote_manifest, enumerate, Installer_FindPending, &found);
      if (found) {
        q_strlcpy(in->current_file, found->path, sizeof(in->current_file));
        entry = found;
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
      q_snprintf(in->error, sizeof(in->error), "Download failed: %s", entry->path);
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

    SDL_LockMutex(installer.mutex);
    const installer_state_t state = in->state;
    SDL_UnlockMutex(installer.mutex);

    switch (state) {

      case INSTALLER_CHECKING: {

        if (INSTALLER_ASSET == NULL) {
          SDL_LockMutex(installer.mutex);
          in->state = INSTALLER_COMPARING;
          SDL_UnlockMutex(installer.mutex);
          break;
        }

        const bool ok = Installer_FetchRelease(QUETOO_RELEASES_API_URL, INSTALLER_ASSET,
                                               &installer.release);

        char parent[MAX_OS_PATH];
        Installer_StagingParent(parent, sizeof(parent));

        const bool managed = *parent && Installer_IsManaged(parent);
        const bool writable = *parent && !managed && Installer_IsWritable(parent);

        if (ok && managed) {
          Com_Print("Externally managed installation; engine updates disabled.\n");
        }

        SDL_LockMutex(installer.mutex);
        if (!ok) {
          in->state = INSTALLER_ERROR;
          q_snprintf(in->error, sizeof(in->error), "Failed to check for updates");
        } else if (Installer_CompareVersions(installer.release.tag, version->string) > 0) {
          if (writable) {
            in->state = INSTALLER_UPDATE_AVAILABLE;
            q_strlcpy(in->current_file, installer.release.asset, sizeof(in->current_file));
          } else {
            in->state = INSTALLER_COMPARING;
            if (!managed) {
              Com_Warn("Quetoo %s is available, but %s is not writable.\n"
                       "Download it from %s\n", installer.release.tag,
                       *parent ? parent : "this installation", QUETOO_RELEASES_PAGE);
            }
          }
        } else {
          in->state = INSTALLER_COMPARING;
        }
        SDL_UnlockMutex(installer.mutex);
      }
        break;

      case INSTALLER_COMPARING: {

        if (!installer.installed_data && !Installer_HasManifest()) {
          installer.installed_data = true;
          if (!Installer_InstallData()) {
            Com_Warn("Falling back to a file by file sync\n");
          }
          SDL_LockMutex(installer.mutex);
          const bool cancelled = in->state == INSTALLER_CANCELLED;
          SDL_UnlockMutex(installer.mutex);
          if (cancelled) {
            break;
          }
        }

        Data *data = NULL;
        char manifest_url[MAX_OS_PATH];
        q_snprintf(manifest_url, sizeof(manifest_url), QUETOO_DATA_BASE_URL "/%s/manifest.mf", Com_Game());
        const int32_t http_status = $($$(RESTClient, sharedInstance), get, manifest_url, NULL, &data);
        if (http_status != 200 || !data) {
          SDL_LockMutex(installer.mutex);
          in->state = INSTALLER_ERROR;
          q_snprintf(in->error, sizeof(in->error), "Failed to fetch manifest: HTTP %d", http_status);
          SDL_UnlockMutex(installer.mutex);
          release(data);
          break;
        }

        HashTable *remote = Cm_ParseManifest((const char *) data->bytes, data->length);
        release(data);

        $(remote, enumerate, Installer_MarkPending, NULL);

        HashTable *local = Installer_ReadManifest();
        if (local) {
          $(local, enumerate, Installer_MarkStale, NULL);
        }

        installer_compare_t ctx = { .local = local };
        $(remote, enumerate, Installer_CompareEntry, &ctx);
        const int32_t files_total = ctx.files_total;
        const int32_t kbytes_total = ctx.kbytes_total;

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

      case INSTALLER_UPDATE_AVAILABLE: {

        SDL_LockMutex(installer.mutex);
        const int32_t consent = installer.consent;
        SDL_UnlockMutex(installer.mutex);

        if (consent == 0) {
          SDL_Delay(QUETOO_TICK_MILLIS);
          break;
        }

        SDL_LockMutex(installer.mutex);
        if (in->state != INSTALLER_CANCELLED) {
          if (consent > 0) {
            in->state = INSTALLER_DOWNLOADING_UPDATE;
            in->kbytes_done = 0;
            in->kbytes_total = (int32_t) (installer.release.size / 1024);
          } else {
            Com_Print("Skipping the update to Quetoo %s.\n", installer.release.tag);
            in->state = INSTALLER_COMPARING;
          }
        }
        SDL_UnlockMutex(installer.mutex);
      }
        break;

      case INSTALLER_DOWNLOADING_UPDATE: {
        char pending[MAX_OS_PATH], archive[MAX_OS_PATH];
        Installer_PendingDir(pending, sizeof(pending));

        Installer_RemoveTree(pending);

        if (!SDL_CreateDirectory(pending)) {
          SDL_LockMutex(installer.mutex);
          in->state = INSTALLER_COMPARING;
          SDL_UnlockMutex(installer.mutex);
          Com_Warn("Failed to create %s: %s\n", pending, SDL_GetError());
          break;
        }

        q_snprintf(archive, sizeof(archive), "%s/%s", pending, installer.release.asset);

        const bool ok = Installer_DownloadToFile(installer.release.url, archive,
                                                 installer.release.size);
        SDL_LockMutex(installer.mutex);
        if (in->state == INSTALLER_CANCELLED) {
          SDL_UnlockMutex(installer.mutex);
          break;
        }
        in->state = ok ? INSTALLER_STAGING_UPDATE : INSTALLER_COMPARING;
        SDL_UnlockMutex(installer.mutex);

        if (!ok) {
          Installer_RemoveTree(pending);
        }
      }
        break;

      case INSTALLER_STAGING_UPDATE: {
        char pending[MAX_OS_PATH], archive[MAX_OS_PATH];
        Installer_PendingDir(pending, sizeof(pending));
        q_snprintf(archive, sizeof(archive), "%s/%s", pending, installer.release.asset);

        const bool ok = Archive_Extract(archive, pending);
        SDL_RemovePath(archive);

        if (ok) {
          Installer_WritePending(pending);
          Com_Print("Quetoo %s staged; it will be applied when you quit.\n", installer.release.tag);
        } else {
          Installer_RemoveTree(pending);
        }

        SDL_LockMutex(installer.mutex);
        if (in->state != INSTALLER_CANCELLED) {
          in->state = ok ? INSTALLER_UPDATE_STAGED : INSTALLER_COMPARING;
        }
        SDL_UnlockMutex(installer.mutex);
      }
        break;

      case INSTALLER_UPDATE_STAGED:
        SDL_LockMutex(installer.mutex);
        in->state = INSTALLER_COMPARING;
        SDL_UnlockMutex(installer.mutex);
        break;

      case INSTALLER_INSTALLING_DATA:
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
 * @brief Strips a trailing newline in place.
 */
static void Installer_Chomp(char *line) {

  char *end = line + q_strlen(line);
  while (end > line && (end[-1] == '\n' || end[-1] == '\r')) {
    *--end = '\0';
  }
}

#if defined(_WIN32)

/**
 * @brief Deletes the displaced files recorded by a previous apply.
 * @details Windows cannot delete a file while it is mapped, so the copies
 * displaced by the last update survive until the process that held them has
 * exited. Failures are expected and ignored: a slow-exiting predecessor, or a
 * second copy of the game still running, simply leaves the entry for the next
 * launch to retry.
 */
static void Installer_SweepDisplaced(void) {

  char path[MAX_OS_PATH];
  q_snprintf(path, sizeof(path), "%s/.cleanup", Fs_BaseDir());

  FILE *file = fopen(path, "rb");
  if (!file) {
    return;
  }

  bool swept = true;
  char line[MAX_OS_PATH];
  char survivors[MAX_OS_PATH * 8];
  size_t length = 0;

  while (fgets(line, sizeof(line), file)) {

    Installer_Chomp(line);
    if (*line == '\0') {
      continue;
    }

    Installer_RemoveTree(line);

    if (SDL_GetPathInfo(line, NULL)) {
      swept = false;
      const int32_t n = q_snprintf(survivors + length, sizeof(survivors) - length, "%s\n", line);
      if (n > 0) {
        length += (size_t) n;
      }
    }
  }

  fclose(file);

  if (swept) {
    SDL_RemovePath(path);
  } else if ((file = fopen(path, "wb"))) {
    fwrite(survivors, 1, length, file);
    fclose(file);
  }
}

#endif

/**
 * @brief Applied to each staged file by `Installer_EachPending`.
 */
typedef bool (*Installer_PendingFunc)(const char *staged, const char *target, FILE *cleanup);

/**
 * @brief Returns the path a displaced file is parked at while an apply runs.
 */
static void Installer_Displaced(const char *target, char *out, size_t len) {
  q_snprintf(out, (int32_t) len, "%s.old", target);
}

/**
 * @brief Moves a staged file into place, parking whatever was there.
 * @details The displaced copy is kept until every file has been installed, so
 * that a failure part way can be undone. Leaving a half-updated tree would
 * pair an executable with game modules of another version, which the
 * `CGAME_API_VERSION` check rejects outright -- an install that cannot start
 * is far worse than one that is merely out of date.
 *
 * A running executable or loaded library cannot be overwritten or deleted, but
 * it can be renamed, which is what makes this possible at all.
 */
static bool Installer_Install(const char *staged, const char *target, FILE *cleanup) {

  char dir[MAX_OS_PATH];
  q_strlcpy(dir, target, sizeof(dir));

  char *slash = q_strrchr(dir, '/');
  if (slash) {
    *slash = '\0';
    SDL_CreateDirectory(dir);
  }

  if (SDL_GetPathInfo(target, NULL)) {

    char displaced[MAX_OS_PATH];
    Installer_Displaced(target, displaced, sizeof(displaced));

    Installer_RemoveTree(displaced);

    if (!SDL_RenamePath(target, displaced)) {
      Com_Warn("Failed to displace %s: %s\n", target, SDL_GetError());
      return false;
    }
  }

  if (!SDL_RenamePath(staged, target)) {
    Com_Warn("Failed to install %s: %s\n", target, SDL_GetError());
    return false;
  }

  return true;
}

/**
 * @brief Discards the file displaced by a successful install.
 * @details POSIX can drop it immediately, because a process running from it
 * holds the inode regardless of the name. Windows cannot delete a mapped
 * image, so the path is recorded and swept at the next launch.
 */
static bool Installer_Commit_(const char *staged, const char *target, FILE *cleanup) {

  char displaced[MAX_OS_PATH];
  Installer_Displaced(target, displaced, sizeof(displaced));

  if (SDL_GetPathInfo(displaced, NULL)) {
    if (cleanup) {
      fprintf(cleanup, "%s\n", displaced);
    } else {
      Installer_RemoveTree(displaced);
    }
  }

  return true;
}

/**
 * @brief Restores a displaced file, undoing a failed apply.
 * @details The staged copy is moved back out of the way first, so that the
 * staging directory survives intact and the whole update can simply be retried
 * at the next exit.
 */
static bool Installer_Rollback(const char *staged, const char *target, FILE *cleanup) {

  char displaced[MAX_OS_PATH];
  Installer_Displaced(target, displaced, sizeof(displaced));

  if (!SDL_GetPathInfo(displaced, NULL)) {
    return true;
  }

  if (!SDL_GetPathInfo(staged, NULL)) {
    SDL_RenamePath(target, staged);
  }

  if (!SDL_RenamePath(displaced, target)) {
    Com_Warn("Failed to restore %s: %s\n", target, SDL_GetError());
  }

  return true;
}

/**
 * @brief Iterates the staged files, invoking `func` for each.
 * @return The number of entries visited, or -1 if the manifest is malformed.
 */
static int32_t Installer_EachPending(FILE *file, const char *root, Installer_PendingFunc func,
                                     FILE *cleanup) {

  fseek(file, 0, SEEK_SET);

  char line[MAX_OS_PATH];
  if (!fgets(line, sizeof(line), file) || !fgets(line, sizeof(line), file)) {
    return -1;
  }

  const size_t root_len = q_strlen(root);

  int32_t count = 0;

  while (fgets(line, sizeof(line), file)) {

    const bool complete = q_strchr(line, '\n') || feof(file);
    Installer_Chomp(line);

    if (!complete) {
      Com_Warn("Staged path too long: %s\n", line);
      return -1;
    }

    if (q_strncmp(line, root, root_len) || line[root_len] != '/') {
      continue;
    }

    char target[MAX_OS_PATH];
    q_snprintf(target, sizeof(target), "%s%s", Fs_BaseDir(), line + root_len);

    if (!func(line, target, cleanup)) {
      return -1;
    }

    count++;
  }

  return count;
}

void Installer_Consent(bool accept) {

  if (installer.mutex) {
    SDL_LockMutex(installer.mutex);
    installer.consent = accept ? 1 : -1;
    SDL_UnlockMutex(installer.mutex);
  }
}

void Installer_ApplyPending(void) {

  if (*Fs_BaseDir() == '\0') {
    return;
  }

  char pending[MAX_OS_PATH], path[MAX_OS_PATH];
  Installer_PendingDir(pending, sizeof(pending));
  q_snprintf(path, sizeof(path), "%s/pending.mf", pending);

  FILE *file = fopen(path, "rb");
  if (!file) {
    return;
  }

  char version[64] = { '\0' }, root[MAX_OS_PATH] = { '\0' };

  if (!fgets(version, sizeof(version), file) || !fgets(root, sizeof(root), file)) {
    Com_Warn("Discarding an incomplete staged update\n");
    fclose(file);
    Installer_RemoveTree(pending);
    return;
  }

  Installer_Chomp(version);
  Installer_Chomp(root);

  FILE *cleanup = NULL;
#if defined(_WIN32)
  char cleanup_path[MAX_OS_PATH];
  q_snprintf(cleanup_path, sizeof(cleanup_path), "%s/.cleanup", Fs_BaseDir());
  cleanup = fopen(cleanup_path, "ab");
#endif

  const int32_t installed = Installer_EachPending(file, root, Installer_Install, cleanup);

  bool success = installed >= 0;

  const bool whole = success && installed == 0;
  if (whole) {
    success = Installer_Install(root, Fs_BaseDir(), cleanup);
  }

  const Installer_PendingFunc finish = success ? Installer_Commit_ : Installer_Rollback;

  if (whole) {
    finish(root, Fs_BaseDir(), success ? cleanup : NULL);
  } else {
    Installer_EachPending(file, root, finish, success ? cleanup : NULL);
  }

  if (cleanup) {
    fclose(cleanup);
  }

  fclose(file);

  if (success) {
    Installer_RemoveTree(pending);
    Com_Print("Updated to Quetoo %s.\n", version);
  } else {
    Com_Warn("Could not apply the staged update; the previous version is intact "
             "and it will be retried on the next exit.\n");
  }
}

/**
 * @brief Starts an asynchronous data update and blocks until it completes,
 * calling @c frame each iteration while the installer is in progress.
 */
void Installer_Init(Installer_FrameFunction frame) {

#if defined(_WIN32)
  Installer_SweepDisplaced();
#endif

  if (build_number->integer == -1 || version->integer == -1) {
    return;
  }

#if defined(__linux__)
  if (q_strcmp(Fs_BinDir(), "/usr/lib/quetoo/bin") == 0) {
    Com_Print("Package-managed installation; auto-updates disabled.\n");
    return;
  }
#endif

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

