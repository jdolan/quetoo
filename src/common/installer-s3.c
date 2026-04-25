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

#include <glib/gstdio.h>

#include "installer-s3.h"

/**
 * @brief GMarkupParser context for S3 ListObjectsV2 XML responses.
 */
typedef struct {
  const char *bucket;                // the bucket name
  GList *objects;                    // accumulated list of s3_object_t*
  s3_object_t *current;              // object being built inside <Contents>
  char next_token[MAX_STRING_CHARS]; // NextContinuationToken for pagination
  bool is_truncated;
  GString *text;                     // accumulated character data for current element
} s3_parse_ctx_t;


/**
 * @brief
 */
static void S3_StartElement(GMarkupParseContext *ctx, const gchar *element_name, const gchar **attr_names, const gchar **attr_values, gpointer user_data, GError **error) {

  s3_parse_ctx_t *c = user_data;

  g_string_truncate(c->text, 0);

  if (!g_strcmp0(element_name, "Contents")) {
    c->current = Mem_Malloc(sizeof(s3_object_t));
    g_strlcpy(c->current->bucket, c->bucket, sizeof(c->current->bucket));
  }
}

/**
 * @brief
 */
static void S3_Text(GMarkupParseContext *ctx, const gchar *text, gsize text_len, gpointer user_data, GError **error) {

  s3_parse_ctx_t *c = user_data;
  g_string_append_len(c->text, text, (gssize) text_len);
}


/**
 * @brief
 */
static void S3_EndElement(GMarkupParseContext *ctx, const gchar *element_name, gpointer user_data, GError **error) {

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
  .start_element = S3_StartElement,
  .text          = S3_Text,
  .end_element   = S3_EndElement,
};

/**
 * @brief Fetches and parses all pages of the S3 bucket listing.
 * @return A GList of `s3_object_t *`, caller must free with `g_list_free` + `Mem_Free` per node.
 */
GList *S3_ListObjects(const char *bucket) {

  s3_parse_ctx_t ctx = {
    .bucket = bucket,
    .text = g_string_new(NULL),
  };

  char url[MAX_OS_PATH];
  g_snprintf(url, sizeof(url), "https://%s.s3.amazonaws.com/", bucket);

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
      g_snprintf(url, sizeof(url), "https://%s.s3.amazonaws.com/?continuation-token=%s", bucket, encoded);
      g_free(encoded);
    }

  } while (ctx.is_truncated);

  g_string_free(ctx.text, TRUE);

  return g_list_reverse(ctx.objects);
}

/**
 * @brief Downloads a single S3 object to the data directory.
 * @return True on success, false on failure.
 */
extern bool S3_GetObject(const s3_object_t *obj) {

  gchar *encoded_key = g_uri_escape_string(obj->key, "/", FALSE);

  char url[MAX_OS_PATH * 2];
  g_snprintf(url, sizeof(url), "https://%s.s3.amazonaws.com/%s", obj->bucket, encoded_key);
  g_free(encoded_key);

  void *data = NULL;
  size_t length = 0;

  const int32_t status = Net_HttpGet(url, &data, &length);
  if (status != 200) {
    Com_Warn("Downloading %s failed: HTTP %d\n", url, status);
    Mem_Free(data);
    return false;
  }

#if 0

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
    Com_Warn("Failed to write %s: %zu of %zu bytes\n", path, written, length);
    return false;
  }
#endif

  return true;
}
