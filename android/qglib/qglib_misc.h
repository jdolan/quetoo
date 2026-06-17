/*
 * qglib_misc.h — glib GDateTime / UUID / URI compatibility.
 *
 * The non-container, non-string odds-and-ends of the qglib subset Quetoo uses:
 *   - GDateTime: src/common/sys.c formats a crash timestamp via
 *     g_date_time_new_now_local() + g_date_time_format(now, "%Y-%m-%d %H:%M:%S")
 *     + g_date_time_unref(now).
 *   - g_uuid_string_random(): src/client/cl_main.c mints the player "guid".
 *   - g_uri_escape_string(): src/collision installer percent-encodes a path with
 *     "/" left unescaped.
 *
 * POSIX-targeted (Android's kernel is Linux); the Windows engine build still
 * uses real glib. GDateTime wraps libc time()/localtime_r()/strftime(); the UUID
 * matches glib's FORMAT (RFC 4122 v4) but not its value (it is random). All
 * newly-allocated results are QG_MALLOC'd, so the engine's g_free (== QG_FREE,
 * see qglib_util.c) releases them. The differential test (test_misc.c) pins the
 * URI output against real glib.
 */
#ifndef QGLIB_MISC_H
#define QGLIB_MISC_H

#include "qglib.h"

/* Opaque, like glib: callers only hold the pointer (sys.c never dereferences). */
typedef struct _GDateTime GDateTime;

/* GDateTime (ref-counted; _new_ returns a single ref, _unref frees at zero). */
GDateTime *g_date_time_new_now_local (void);
gchar     *g_date_time_format        (GDateTime *datetime, const gchar *format); /* newly-allocated */
void       g_date_time_unref         (GDateTime *datetime);

/* Newly-allocated lowercase RFC 4122 v4 UUID: "xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx". */
gchar     *g_uuid_string_random      (void);

/* Percent-encode: unreserved A-Za-z0-9-_.~ pass through, as do any chars in
 * reserved_chars_allowed; everything else becomes %XX (uppercase hex). The
 * allow_utf8 flag lets bytes >= 0x80 pass through unescaped when TRUE. */
gchar     *g_uri_escape_string       (const gchar *unescaped,
                                      const gchar *reserved_chars_allowed,
                                      gboolean allow_utf8);

#endif /* QGLIB_MISC_H */
