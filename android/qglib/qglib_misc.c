/*
 * qglib_misc.c — GDateTime / UUID / URI implementation. See qglib_misc.h.
 * POSIX-targeted (Android/Linux). The differential test (test_misc.c) pins the
 * URI output against real glib; the UUID/date paths are checked for shape only
 * (they are random / wall-clock dependent).
 */
#define _POSIX_C_SOURCE 200809L /* expose localtime_r, getpid under -std=c11 */

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "qglib_misc.h"

/* ---- GDateTime ----
 * glib's GDateTime is ref-counted and immutable. The engine only needs a local
 * "now" snapshot it can strftime, so we store the broken-down local time plus a
 * refcount and free at zero (pairing the implicit ref from _new_now_local).
 */
struct _GDateTime {
	struct tm tm;   /* broken-down local time */
	gint      ref;  /* reference count */
};

GDateTime *g_date_time_new_now_local(void) {
	GDateTime *dt = QG_MALLOC(sizeof(*dt));
	if (!dt) {
		return NULL;
	}
	const time_t now = time(NULL);
	localtime_r(&now, &dt->tm);
	dt->ref = 1;
	return dt;
}

gchar *g_date_time_format(GDateTime *datetime, const gchar *format) {
	if (!datetime || !format) {
		return NULL;
	}
	/* strftime can't distinguish "buffer too small" from "produced 0 bytes", so
	 * grow until it fits. A leading-space guard byte disambiguates the 0 case. */
	gsize cap = 64;
	for (;;) {
		gchar *buf = QG_MALLOC(cap);
		if (!buf) {
			return NULL;
		}
		buf[0] = '\1';
		const gsize n = strftime(buf, cap, format, &datetime->tm);
		if (n != 0 || buf[0] == '\0') {
			return buf; /* fits (n bytes) or format legitimately yielded "" */
		}
		QG_FREE(buf);
		cap *= 2;
	}
}

void g_date_time_unref(GDateTime *datetime) {
	if (datetime && --datetime->ref <= 0) {
		QG_FREE(datetime);
	}
}

/* ---- random source (seeded once from time + pid) ----
 * A tiny SplitMix64; we only need decent spread for UUID/format purposes, not
 * cryptographic quality (glib's value isn't reproduced — only the format is).
 */
static guint64 qg_rng_state;
static gboolean qg_rng_seeded;

static guint64 qg_rng_next(void) {
	if (!qg_rng_seeded) {
		qg_rng_state = (guint64) time(NULL);
		qg_rng_state ^= (guint64) getpid() << 32;
		qg_rng_state ^= (guint64) (uintptr_t) &qg_rng_state;
		qg_rng_seeded = TRUE;
	}
	guint64 z = (qg_rng_state += 0x9E3779B97F4A7C15ULL);
	z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
	z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
	return z ^ (z >> 31);
}

gchar *g_uuid_string_random(void) {
	static const gchar hex[] = "0123456789abcdef";
	gchar *s = QG_MALLOC(37); /* 36 chars + NUL */
	if (!s) {
		return NULL;
	}
	for (gint i = 0; i < 36; i++) {
		switch (i) {
			case 8:
			case 13:
			case 18:
			case 23:
				s[i] = '-';
				break;
			case 14:
				s[i] = '4'; /* version 4 */
				break;
			case 19:
				s[i] = hex[8 + (guint) (qg_rng_next() & 0x3)]; /* variant: 8,9,a,b */
				break;
			default:
				s[i] = hex[(guint) (qg_rng_next() & 0xf)];
				break;
		}
	}
	s[36] = '\0';
	return s;
}

/* ---- g_uri_escape_string ----
 * RFC 3986 unreserved set passes through; so does anything in
 * reserved_chars_allowed; everything else becomes %XX with uppercase hex. With
 * allow_utf8 set, bytes >= 0x80 pass through unescaped (matching glib).
 */
static gboolean qg_uri_unreserved(guchar c) {
	return (c >= 'A' && c <= 'Z') ||
	       (c >= 'a' && c <= 'z') ||
	       (c >= '0' && c <= '9') ||
	       c == '-' || c == '_' || c == '.' || c == '~';
}

gchar *g_uri_escape_string(const gchar *unescaped,
                           const gchar *reserved_chars_allowed,
                           gboolean allow_utf8) {
	if (!unescaped) {
		return NULL;
	}
	static const gchar hex[] = "0123456789ABCDEF";
	const gsize in_len = strlen(unescaped);
	gchar *out = QG_MALLOC(in_len * 3 + 1); /* worst case: every byte -> %XX */
	if (!out) {
		return NULL;
	}
	gchar *w = out;
	for (gsize i = 0; i < in_len; i++) {
		const guchar c = (guchar) unescaped[i];
		const gboolean passthrough =
		    qg_uri_unreserved(c) ||
		    (reserved_chars_allowed && strchr(reserved_chars_allowed, c) != NULL) ||
		    (allow_utf8 && c >= 0x80);
		if (passthrough) {
			*w++ = (gchar) c;
		} else {
			*w++ = '%';
			*w++ = hex[c >> 4];
			*w++ = hex[c & 0xf];
		}
	}
	*w = '\0';
	return out;
}
