/*
 * qglib_util.h — glib string/format/memory/filesystem/time primitives.
 *
 * The non-container half of the qglib subset Quetoo uses. POSIX-targeted
 * (Android's kernel is Linux); the Windows engine build still uses real glib,
 * so the POSIX calls here are fine for the Android/Linux targets. Allocation
 * routes through QG_MALLOC/QG_FREE (see qglib.h).
 */
#ifndef QGLIB_UTIL_H
#define QGLIB_UTIL_H

#include <stdio.h> /* FILE, for g_fopen */

#include "qglib.h"

#ifdef _WIN32
#define G_DIR_SEPARATOR   '\\'
#define G_DIR_SEPARATOR_S "\\"
#else
#define G_DIR_SEPARATOR   '/'
#define G_DIR_SEPARATOR_S "/"
#endif

typedef enum {
	G_FILE_TEST_IS_REGULAR    = 1 << 0,
	G_FILE_TEST_IS_SYMLINK    = 1 << 1,
	G_FILE_TEST_IS_DIR        = 1 << 2,
	G_FILE_TEST_IS_EXECUTABLE = 1 << 3,
	G_FILE_TEST_EXISTS        = 1 << 4
} GFileTest;

/* memory */
gpointer g_malloc  (gsize n_bytes);
gpointer g_malloc0 (gsize n_bytes);
gpointer g_realloc (gpointer mem, gsize n_bytes);
void     g_free    (gpointer mem);
#define  g_new(struct_type, n)  ((struct_type *) g_malloc(sizeof(struct_type) * (gsize) (n)))
#define  g_new0(struct_type, n) ((struct_type *) g_malloc0(sizeof(struct_type) * (gsize) (n)))

/* strings */
gsize    g_strlcpy          (gchar *dest, const gchar *src, gsize dest_size);
gsize    g_strlcat          (gchar *dest, const gchar *src, gsize dest_size);
gint     g_snprintf         (gchar *string, gulong n, const gchar *format, ...);
gint     g_ascii_strcasecmp (const gchar *s1, const gchar *s2);
gint     g_ascii_strncasecmp(const gchar *s1, const gchar *s2, gsize n);
gint64   g_ascii_strtoll    (const gchar *nptr, gchar **endptr, guint base);
gboolean g_ascii_isalnum    (gchar c);
gboolean g_str_has_prefix   (const gchar *str, const gchar *prefix);
gboolean g_str_has_suffix   (const gchar *str, const gchar *suffix);
gint     g_strcmp0          (const gchar *str1, const gchar *str2);  /* NULL-safe strcmp */
gchar   *g_strdup           (const gchar *str);
gchar   *g_strdup_printf    (const gchar *format, ...);
gchar  **g_strsplit         (const gchar *string, const gchar *delimiter, gint max_tokens);
void     g_strfreev         (gchar **str_array);
guint    g_strv_length      (gchar **str_array);            /* count of the NULL-terminated array */
gchar   *g_strchug          (gchar *string);                 /* trim leading whitespace, in place */
gchar   *g_strchomp         (gchar *string);                 /* trim trailing whitespace, in place */
#define  g_strstrip(string) g_strchomp(g_strchug(string))
gchar   *g_strrstr          (const gchar *haystack, const gchar *needle);
gchar   *g_ascii_strdown    (const gchar *str, gssize len);  /* len -1 = whole string */
gchar   *g_path_get_basename(const gchar *file_name);
gchar   *g_path_get_dirname (const gchar *file_name);
const gchar *g_get_home_dir     (void);                      /* cached; not freed by caller */
const gchar *g_get_user_data_dir(void);

/* filesystem + time */
gchar   *g_build_filename     (const gchar *first_element, ...);
gchar   *g_build_path         (const gchar *separator, const gchar *first_element, ...);
FILE    *g_fopen              (const gchar *filename, const gchar *mode);  /* <glib/gstdio.h> */
gint     g_remove             (const gchar *filename);
gint     g_unlink             (const gchar *filename);
gboolean g_file_test          (const gchar *filename, GFileTest test);
gint     g_mkdir_with_parents (const gchar *pathname, gint mode);
gint64   g_get_monotonic_time (void);

#endif /* QGLIB_UTIL_H */
