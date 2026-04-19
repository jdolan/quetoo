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
#include "net/net_http.h"
#include "thread.h"

#define QUETOO_REVISION_URL      "https://quetoo.s3.amazonaws.com/revisions/" BUILD
#define QUETOO_RELEASES_URL      "https://github.com/jdolan/quetoo/releases/latest"

#define QUETOO_DATA_REVISION_URL "https://quetoo-data.s3.amazonaws.com/revision"
#define QUETOO_DATA_BASE_URL     "https://quetoo-data.s3.amazonaws.com/"
#define QUETOO_DATA_LIST_URL     "https://quetoo-data.s3.amazonaws.com/?list-type=2&max-keys=1000"

static installer_status_t status;

/**
 * @brief Compares a local revision string against a remote revision URL.
 * @return 0 if they match, non-zero otherwise.
 */
static int32_t Installer_CompareRevision(const char *rev, const char *rev_url) {
	void *data;
	size_t length;

	int32_t status = Net_HttpGet(rev_url, &data, &length);
	if (status == 200) {
		char *remote_rev = g_strchomp(g_strndup(data, length));
		Com_Debug(DEBUG_COMMON, "%s == %s\n", rev, remote_rev);
		status = g_strcmp0(rev, remote_rev);
		g_free(remote_rev);
	} else {
		Com_Warn("%s: HTTP %d\n", rev_url, status);
		if (length) {
			Com_Debug(DEBUG_COMMON, "%s\n", (gchar *) data);
		}
	}

	Mem_Free(data);
	return status;
}

/**
 * @brief Checks whether a newer engine build is available.
 * @return 0 if the engine is up to date, 1 if an update is available.
 */
int32_t Installer_CheckForUpdates(void) {

	if (revision->integer == -1) {
		Com_Debug(DEBUG_COMMON, "Skipping revision check\n");
		return 0;
	}

	if (Installer_CompareRevision(revision->string, QUETOO_REVISION_URL) == 0) {
		Com_Debug(DEBUG_COMMON, "Build revision %s is latest.\n", revision->string);
		return 0;
	}

	Com_Debug(DEBUG_COMMON, "Build revision %s is out of date.\n", revision->string);
	return 1;
}

/**
 * @brief Opens the GitHub releases page in the user's web browser.
 */
void Installer_OpenReleasesPage(void) {

	Com_Print("Opening %s\n", QUETOO_RELEASES_URL);

	if (!SDL_OpenURL(QUETOO_RELEASES_URL)) {
		Com_Warn("Failed to open browser: %s\n", SDL_GetError());
	}
}

/**
 * @brief An object entry from the S3 bucket listing.
 */
typedef struct {
	char key[MAX_OS_PATH];
	char etag[64]; // MD5 hex, 32 chars; may be empty for multipart uploads
	int64_t size;
} s3_object_t;

/**
 * @brief GMarkupParser context for S3 ListObjectsV2 XML responses.
 */
typedef struct {
	GList *objects;         // accumulated list of s3_object_t*
	s3_object_t *current;   // object being built inside <Contents>
	char next_token[512];   // NextContinuationToken for pagination
	bool is_truncated;
	GString *text;          // accumulated character data for current element
} s3_parse_ctx_t;

static void s3_start_element(GMarkupParseContext *ctx, const gchar *element_name,
	const gchar **attr_names, const gchar **attr_values,
	gpointer user_data, GError **error) {

	s3_parse_ctx_t *c = user_data;

	g_string_truncate(c->text, 0);

	if (!g_strcmp0(element_name, "Contents")) {
		c->current = Mem_Malloc(sizeof(s3_object_t));
	}
}

static void s3_text(GMarkupParseContext *ctx, const gchar *text, gsize text_len,
	gpointer user_data, GError **error) {

	s3_parse_ctx_t *c = user_data;
	g_string_append_len(c->text, text, (gssize) text_len);
}

static void s3_end_element(GMarkupParseContext *ctx, const gchar *element_name,
	gpointer user_data, GError **error) {

	s3_parse_ctx_t *c = user_data;
	const char *value = c->text->str;

	if (!g_strcmp0(element_name, "Contents")) {
		if (c->current) {
			c->objects = g_list_prepend(c->objects, c->current);
			c->current = NULL;
		}
	} else if (c->current) {
		if (!g_strcmp0(element_name, "Key")) {
			g_strlcpy(c->current->key, value, sizeof(c->current->key));
		} else if (!g_strcmp0(element_name, "ETag")) {
			// ETags arrive as "&quot;md5hex&quot;" decoded to "md5hex" by GMarkup.
			// Strip the surrounding quotes.
			const char *etag = value;
			if (*etag == '"') {
				etag++;
			}
			g_strlcpy(c->current->etag, etag, sizeof(c->current->etag));
			const size_t len = strlen(c->current->etag);
			if (len > 0 && c->current->etag[len - 1] == '"') {
				c->current->etag[len - 1] = '\0';
			}
		} else if (!g_strcmp0(element_name, "Size")) {
			c->current->size = (int64_t) g_ascii_strtoull(value, NULL, 10);
		}
	} else {
		if (!g_strcmp0(element_name, "NextContinuationToken")) {
			g_strlcpy(c->next_token, value, sizeof(c->next_token));
		} else if (!g_strcmp0(element_name, "IsTruncated")) {
			c->is_truncated = !g_strcmp0(value, "true");
		}
	}

	g_string_truncate(c->text, 0);
}

static const GMarkupParser s3_parser = {
	.start_element = s3_start_element,
	.text          = s3_text,
	.end_element   = s3_end_element,
};

/**
 * @brief Fetches and parses all pages of the S3 bucket listing.
 * @return A GList of s3_object_t*, caller must free with g_list_free + Mem_Free per node.
 */
static GList *Installer_ListObjects(void) {

	s3_parse_ctx_t ctx = {
		.text = g_string_new(NULL),
	};

	char url[MAX_OS_PATH * 2];
	g_strlcpy(url, QUETOO_DATA_LIST_URL, sizeof(url));

	do {
		void *data = NULL;
		size_t length = 0;

		const int32_t http_status = Net_HttpGet(url, &data, &length);
		if (http_status != 200) {
			Com_Warn("S3 listing failed: HTTP %d\n", http_status);
			Mem_Free(data);
			g_list_free_full(ctx.objects, Mem_Free);
			g_string_free(ctx.text, TRUE);
			return NULL;
		}

		ctx.is_truncated = false;
		ctx.next_token[0] = '\0';

		GError *parse_error = NULL;
		GMarkupParseContext *mctx = g_markup_parse_context_new(&s3_parser, 0, &ctx, NULL);

		const bool ok =
			g_markup_parse_context_parse(mctx, data, (gssize) length, &parse_error) &&
			g_markup_parse_context_end_parse(mctx, &parse_error);

		g_markup_parse_context_free(mctx);
		Mem_Free(data);

		if (!ok) {
			Com_Warn("S3 XML parse error: %s\n", parse_error ? parse_error->message : "unknown");
			if (parse_error) {
				g_error_free(parse_error);
			}
			Mem_Free(ctx.current);
			g_list_free_full(ctx.objects, Mem_Free);
			g_string_free(ctx.text, TRUE);
			return NULL;
		}

		if (ctx.is_truncated && ctx.next_token[0]) {
			gchar *encoded = g_uri_escape_string(ctx.next_token, NULL, FALSE);
			g_snprintf(url, sizeof(url), QUETOO_DATA_LIST_URL "&continuation-token=%s", encoded);
			g_free(encoded);
		}

	} while (ctx.is_truncated);

	g_string_free(ctx.text, TRUE);

	return g_list_reverse(ctx.objects);
}

/**
 * @brief Computes the MD5 hex digest of a local file under the data directory.
 * @return True and fills hex[33] on success, false if the file does not exist.
 */
static bool Installer_LocalMD5(const char *key, char hex[33]) {

  char path[MAX_OS_PATH];
  g_snprintf(path, sizeof(path), "%s%c%s", Fs_DataDir(), G_DIR_SEPARATOR, key);

  FILE *f = g_fopen(path, "rb");
  if (!f) {
    return false;
  }

  GChecksum *md5 = g_checksum_new(G_CHECKSUM_MD5);
  uint8_t buf[65536];
  size_t n;
  while ((n = fread(buf, 1, sizeof(buf), f)) > 0) {
    g_checksum_update(md5, buf, (gssize) n);
  }
  fclose(f);

  g_strlcpy(hex, g_checksum_get_string(md5), 33);
  g_checksum_free(md5);
  return true;
}

/**
 * @brief Returns the size in bytes of a local file under the data directory, or -1 if absent.
 */
static int64_t Installer_LocalSize(const char *key) {

  char path[MAX_OS_PATH];
  g_snprintf(path, sizeof(path), "%s%c%s", Fs_DataDir(), G_DIR_SEPARATOR, key);

  FILE *f = g_fopen(path, "rb");
  if (!f) {
    return -1;
  }

  fseek(f, 0, SEEK_END);
  const int64_t size = (int64_t) ftell(f);
  fclose(f);
  return size;
}

/**
 * @brief The number of concurrent download threads.
 */
#define INSTALLER_DOWNLOAD_THREADS 4

/**
 * @brief Shared state for parallel download workers.
 */
typedef struct {
	installer_status_t *status;
	SDL_Mutex *queue_lock;
	GList **queue;
	bool *ok;
} installer_worker_t;

/**
 * @brief Downloads a single S3 object and writes it to the data directory.
 * @return True on success.
 */
static bool Installer_Download(const s3_object_t *obj) {

  char url[MAX_OS_PATH * 2];
  g_snprintf(url, sizeof(url), "%s%s", QUETOO_DATA_BASE_URL, obj->key);

  void *data = NULL;
  size_t length = 0;

  const int32_t http_status = Net_HttpGet(url, &data, &length);
  if (http_status != 200) {
    Com_Warn("Download failed %s: HTTP %d\n", obj->key, http_status);
    Mem_Free(data);
    return false;
  }

  char path[MAX_OS_PATH];
  g_snprintf(path, sizeof(path), "%s%c%s", Fs_DataDir(), G_DIR_SEPARATOR, obj->key);

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
    Com_Warn("Short write for %s: %zu of %zu bytes\n", obj->key, written, length);
    return false;
  }

  return true;
}

/**
 * @brief Worker thread for parallel downloads. Pops items from the shared
 * queue until it is empty or a download failure is recorded.
 */
static int Installer_DownloadWorker(void *data) {

  installer_worker_t *worker = (installer_worker_t *) data;
  installer_status_t *status = worker->status;

  while (true) {

    SDL_LockMutex(worker->queue_lock);
    if (!*worker->queue || !*worker->ok) {
      SDL_UnlockMutex(worker->queue_lock);
      break;
    }
    GList *item = *worker->queue;
    *worker->queue = item->next;
    SDL_UnlockMutex(worker->queue_lock);

    const s3_object_t *obj = item->data;

    SDL_LockMutex(status->lock);
    g_strlcpy(status->current_file, obj->key, sizeof(status->current_file));
    SDL_UnlockMutex(status->lock);

    Com_Debug(DEBUG_COMMON, "Downloading: %s\n", obj->key);

    if (!Installer_Download(obj)) {
      SDL_LockMutex(status->lock);
      g_snprintf(status->error, sizeof(status->error), "Download failed: %s", obj->key);
      SDL_UnlockMutex(status->lock);

      SDL_LockMutex(worker->queue_lock);
      *worker->ok = false;
      SDL_UnlockMutex(worker->queue_lock);
    } else {
      SDL_LockMutex(status->lock);
      status->files_done++;
      status->kbytes_done += (int32_t) ((obj->size + 1023) / 1024);
      SDL_UnlockMutex(status->lock);
    }
  }

  return 0;
}

/**
 * @brief Prune local files in the write dir that are absent from the S3 index.
 * Only files that PhysicsFS resolves to the write directory are removed.
 */
typedef struct {
	GHashTable *s3_keys;
	int32_t count;
} prune_ctx_t;

/**
 * @brief Prune local files under base/rel that are absent from the S3 index.
 */
static void Installer_PruneDir(const char *base, const char *rel, prune_ctx_t *ctx) {

  char abs[MAX_OS_PATH];
  g_snprintf(abs, sizeof(abs), "%s%c%s", base, G_DIR_SEPARATOR, rel);

  GDir *dir = g_dir_open(abs, 0, NULL);
  if (!dir) {
    return;
  }

  const char *name;
  while ((name = g_dir_read_name(dir)) != NULL) {
    char rel_child[MAX_OS_PATH];
    g_snprintf(rel_child, sizeof(rel_child), "%s/%s", rel, name);

    char abs_child[MAX_OS_PATH];
    g_snprintf(abs_child, sizeof(abs_child), "%s%c%s", base, G_DIR_SEPARATOR, rel_child);

    if (g_file_test(abs_child, G_FILE_TEST_IS_DIR)) {
      Installer_PruneDir(base, rel_child, ctx);
    } else if (!g_hash_table_contains(ctx->s3_keys, rel_child)) {
      Com_Debug(DEBUG_COMMON, "Pruning: %s\n", rel_child);
      g_remove(abs_child);
      ctx->count++;
    }
  }

  g_dir_close(dir);
}

/**
 * @brief Background thread that performs the full S3 data sync.
 */
static void Installer_UpdateThread(void *data) {

	installer_status_t *status = (installer_status_t *) data;

	// Phase 1: check whether the data revision has changed.
	SDL_LockMutex(status->lock);
	status->state = INSTALLER_CHECKING;
	SDL_UnlockMutex(status->lock);

	char local_rev[64] = {};
	{
		void *rev_data = NULL;
		if (Fs_Load("revision", &rev_data) > 0) {
			g_strlcpy(local_rev, g_strchomp((char *) rev_data), sizeof(local_rev));
		}
		Fs_Free(rev_data);
	}

	void *remote_rev_data = NULL;
	size_t remote_rev_length = 0;
	const int32_t rev_status = Net_HttpGet(QUETOO_DATA_REVISION_URL, &remote_rev_data, &remote_rev_length);

	if (rev_status != 200) {
		Com_Warn("Failed to fetch data revision: HTTP %d\n", rev_status);
		Mem_Free(remote_rev_data);
		SDL_LockMutex(status->lock);
		g_strlcpy(status->error, "Failed to fetch data revision", sizeof(status->error));
		status->state = INSTALLER_ERROR;
		SDL_UnlockMutex(status->lock);
		return;
	}

	char remote_rev[64] = {};
	gchar *remote_rev_tmp = g_strndup(remote_rev_data, remote_rev_length);
	g_strlcpy(remote_rev, g_strchomp(remote_rev_tmp), sizeof(remote_rev));
	g_free(remote_rev_tmp);
	Mem_Free(remote_rev_data);

	if (!g_strcmp0(local_rev, remote_rev)) {
		Com_Print("Data is current at revision %s.\n", local_rev);
		SDL_LockMutex(status->lock);
		status->state = INSTALLER_DONE;
		SDL_UnlockMutex(status->lock);
		return;
	}

	Com_Print("Data revision %s → %s, updating...\n", local_rev[0] ? local_rev : "(none)", remote_rev);

	// Phase 2: list all S3 objects.
	SDL_LockMutex(status->lock);
	status->state = INSTALLER_LISTING;
	SDL_UnlockMutex(status->lock);

	GList *objects = Installer_ListObjects();
	if (!objects) {
		SDL_LockMutex(status->lock);
		g_strlcpy(status->error, "Failed to list S3 objects", sizeof(status->error));
		status->state = INSTALLER_ERROR;
		SDL_UnlockMutex(status->lock);
		return;
	}

	// Phase 3: compute delta — which files need downloading.
	SDL_LockMutex(status->lock);
	status->state = INSTALLER_LISTING;
	SDL_UnlockMutex(status->lock);

	GList *delta = NULL;
	int32_t kbytes_total = 0;

	for (GList *l = objects; l; l = l->next) {
		const s3_object_t *obj = l->data;

		if (!g_strcmp0(obj->key, "revision")) {
			continue;
		}

		bool needs_update = true;

		char local_md5[33] = {};
		if (Installer_LocalMD5(obj->key, local_md5)) {
			if (strchr(obj->etag, '-')) {
				// Multipart upload: ETag is not a plain MD5; fall back to size.
				needs_update = (Installer_LocalSize(obj->key) != obj->size);
			} else {
				needs_update = !!g_strcmp0(local_md5, obj->etag);
			}
		}

		if (needs_update) {
			delta = g_list_prepend(delta, (gpointer) obj);
			kbytes_total += (int32_t) ((obj->size + 1023) / 1024);
		}
	}

	delta = g_list_reverse(delta);

	SDL_LockMutex(status->lock);
	status->files_total = (int32_t) g_list_length(delta);
	status->kbytes_total = kbytes_total;
	status->files_done = 0;
	status->kbytes_done = 0;
	SDL_UnlockMutex(status->lock);

	// Phase 3 (continued): download the delta in parallel.
	SDL_LockMutex(status->lock);
	status->state = INSTALLER_DOWNLOADING;
	SDL_UnlockMutex(status->lock);

	bool download_ok = true;

	SDL_Mutex *queue_lock = SDL_CreateMutex();
	GList *queue = delta;

	installer_worker_t workers[INSTALLER_DOWNLOAD_THREADS];
	SDL_Thread *threads[INSTALLER_DOWNLOAD_THREADS];

	for (int32_t i = 0; i < INSTALLER_DOWNLOAD_THREADS; i++) {
		workers[i] = (installer_worker_t) {
			.status     = status,
			.queue_lock = queue_lock,
			.queue      = &queue,
			.ok         = &download_ok,
		};
		threads[i] = SDL_CreateThread(Installer_DownloadWorker, "installer", &workers[i]);
	}

	for (int32_t i = 0; i < INSTALLER_DOWNLOAD_THREADS; i++) {
		SDL_WaitThread(threads[i], NULL);
	}

	SDL_DestroyMutex(queue_lock);
	g_list_free(delta);

	if (!download_ok) {
		g_list_free_full(objects, Mem_Free);
		SDL_LockMutex(status->lock);
		status->state = INSTALLER_ERROR;
		SDL_UnlockMutex(status->lock);
		return;
	}

	// Phase 4: prune local files absent from the S3 index.
	SDL_LockMutex(status->lock);
	status->state = INSTALLER_PRUNING;
	SDL_UnlockMutex(status->lock);

	GHashTable *s3_keys = g_hash_table_new(g_str_hash, g_str_equal);
	for (GList *l = objects; l; l = l->next) {
		s3_object_t *obj = l->data;
		g_hash_table_add(s3_keys, obj->key);
	}

	prune_ctx_t prune_ctx = { .s3_keys = s3_keys };
	Installer_PruneDir(Fs_DataDir(), DEFAULT_GAME, &prune_ctx);
	if (prune_ctx.count) {
		Com_Print("Pruned %d stale file(s).\n", prune_ctx.count);
	}

	g_hash_table_destroy(s3_keys);
	g_list_free_full(objects, Mem_Free);

	char rev_path[MAX_OS_PATH];
	g_snprintf(rev_path, sizeof(rev_path), "%s%crevision", Fs_DataDir(), G_DIR_SEPARATOR);
	FILE *rev_file = g_fopen(rev_path, "wb");
	if (rev_file) {
		fputs(remote_rev, rev_file);
		fclose(rev_file);
	} else {
		Com_Warn("Failed to write revision file: %s\n", rev_path);
	}

	Com_Print("Data sync complete (revision %s).\n", remote_rev);
	SDL_LockMutex(status->lock);
	status->state = INSTALLER_DONE;
	SDL_UnlockMutex(status->lock);
}

/**
 * @brief Begins an asynchronous S3 data update if one is not already running.
 */
void Installer_Update(void) {

	if (!status.lock) {
		status.lock = SDL_CreateMutex();
	}

	if (revision->integer == -1) {
		Com_Debug(DEBUG_COMMON, "Skipping data update\n");
		SDL_LockMutex(status.lock);
		status.state = INSTALLER_DONE;
		SDL_UnlockMutex(status.lock);
		return;
	}

	SDL_LockMutex(status.lock);

  const bool already_running = (status.state != INSTALLER_IDLE &&
                                status.state != INSTALLER_DONE &&
	                              status.state != INSTALLER_ERROR);
	if (!already_running) {
		status.state           = INSTALLER_IDLE;
		status.files_done      = 0;
		status.files_total     = 0;
		status.kbytes_done     = 0;
		status.kbytes_total    = 0;
		status.current_file[0] = '\0';
		status.error[0]        = '\0';

    Thread_Create(Installer_UpdateThread, &status, THREAD_NO_WAIT);
  }

	SDL_UnlockMutex(status.lock);
}

/**
 * @brief Populates @a out with a snapshot of the current sync status.
 * Safe to call from any thread.
 */
void Installer_Status(installer_status_t *out) {

	if (!status.lock) {
		*out = (installer_status_t) {};
		return;
	}

	SDL_LockMutex(status.lock);
	*out = status;
	SDL_UnlockMutex(status.lock);

	out->lock = NULL; // callers never need the lock pointer
}
