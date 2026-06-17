/*
 * qglib_string.c — GString implementation for the qglib compatibility layer.
 *
 * Mirrors glib's GString semantics. The struct is fully public (str/len/
 * allocated_len), so unlike GArray there is no private head-aliasing type;
 * bookkeeping lives in those three fields directly. `str` stays NUL-terminated,
 * `len` excludes the NUL, `allocated_len` includes it, and growth is geometric.
 */
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "qglib_string.h"

/* Ensure room for `len` bytes of text plus the trailing NUL, growing the
 * buffer geometrically. glib starts a fresh buffer at a small power of two. */
static void qg_string_maybe_expand(GString *string, gsize len) {
	const gsize want = string->len + len + 1; /* +1 for the NUL */
	if (want <= string->allocated_len) {
		return;
	}
	gsize na = string->allocated_len ? string->allocated_len : 2u;
	while (na < want) {
		na <<= 1;
	}
	string->str = QG_REALLOC(string->str, na);
	string->allocated_len = na;
}

GString *g_string_sized_new(gsize dfl_size) {
	GString *string = QG_MALLOC(sizeof(GString));
	string->str = NULL;
	string->len = 0;
	string->allocated_len = 0;
	qg_string_maybe_expand(string, dfl_size);
	string->str[0] = '\0';
	return string;
}

GString *g_string_new(const gchar *init) {
	const gsize len = (init && *init) ? strlen(init) : 0;
	/* Reserve a little headroom for the common append-after-new pattern, but
	 * always enough to hold `init` and its NUL. */
	GString *string = g_string_sized_new(len < 2u ? 2u : len);
	if (len) {
		memcpy(string->str, init, len);
		string->len = len;
		string->str[len] = '\0';
	}
	return string;
}

GString *g_string_append(GString *string, const gchar *val) {
	if (val == NULL || *val == '\0') {
		return string;
	}
	const gsize len = strlen(val);
	qg_string_maybe_expand(string, len);
	memcpy(string->str + string->len, val, len);
	string->len += len;
	string->str[string->len] = '\0';
	return string;
}

GString *g_string_append_c(GString *string, gchar c) {
	qg_string_maybe_expand(string, 1);
	string->str[string->len] = c;
	string->len += 1;
	string->str[string->len] = '\0';
	return string;
}

GString *g_string_append_printf(GString *string, const gchar *format, ...) {
	va_list args;

	/* First pass: measure the formatted length without writing. */
	va_start(args, format);
	const int needed = vsnprintf(NULL, 0, format, args);
	va_end(args);

	if (needed <= 0) { /* nothing to add (or an encoding error) */
		return string;
	}

	const gsize len = (gsize) needed;
	qg_string_maybe_expand(string, len);

	/* Second pass: write into the now-guaranteed space (len + 1 for the NUL). */
	va_start(args, format);
	vsnprintf(string->str + string->len, len + 1, format, args);
	va_end(args);

	string->len += len;
	return string;
}

GString *g_string_truncate(GString *string, gsize len) {
	if (len < string->len) { /* glib clamps: never grows the logical length */
		string->len = len;
		string->str[string->len] = '\0';
	}
	return string;
}

gchar *g_string_free(GString *string, gboolean free_segment) {
	gchar *segment;
	if (free_segment) {
		QG_FREE(string->str);
		segment = NULL;
	} else {
		segment = string->str;
	}
	QG_FREE(string);
	return segment;
}
