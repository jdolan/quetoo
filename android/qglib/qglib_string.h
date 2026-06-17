/*
 * qglib_string.h — GString (growable text buffer) for the qglib layer.
 *
 * Public layout matches glib: callers read `->str`, `->len` and
 * `->allocated_len` directly. Invariants held by every operation below:
 *   - `str` is always NUL-terminated.
 *   - `len` is the string length in bytes, excluding the trailing NUL.
 *   - `allocated_len` is the buffer capacity in bytes, including that NUL.
 * Buffer growth is geometric.
 */
#ifndef QGLIB_STRING_H
#define QGLIB_STRING_H

#include "qglib.h"

typedef struct _GString {
	gchar *str;
	gsize  len;
	gsize  allocated_len;
} GString;

GString *g_string_new           (const gchar *init);
GString *g_string_sized_new     (gsize dfl_size);
GString *g_string_append        (GString *string, const gchar *val);
GString *g_string_append_c      (GString *string, gchar c);
GString *g_string_append_printf (GString *string, const gchar *format, ...);
GString *g_string_truncate      (GString *string, gsize len);
gchar   *g_string_free          (GString *string, gboolean free_segment);

#endif /* QGLIB_STRING_H */
