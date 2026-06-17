/*
 * qglib_io.h — file I/O + user-name primitives for the qglib compatibility layer.
 *
 * The last slice of the qglib subset Quetoo uses: src/common/sys.c reads/writes
 * whole files (engine config, crash dumps) and appends to a crash log through a
 * minimal GIOChannel. POSIX-targeted (Android/Linux); built over libc stdio +
 * getenv.
 *
 * Source-compatible with the glib subset the engine touches: g_get_user_name
 * returns a cached string the caller must not free; the get/set_contents pair
 * and the GIOChannel calls match glib signatures and report failures via GError
 * (callers read ->message, then g_error_free). Allocation routes through
 * g_malloc/g_free (see qglib.h).
 */
#ifndef QGLIB_IO_H
#define QGLIB_IO_H

#include <stdio.h> /* FILE — GIOChannel wraps one */

#include "qglib.h" /* base types + GError */

/* Current login name; cached, NOT freed by the caller (glib semantics). */
const gchar *g_get_user_name(void);

/* Read the whole file into a g_malloc'd, NUL-terminated buffer (the trailing NUL
 * is not counted in *length). On failure returns FALSE and, if `error` is
 * non-NULL, sets *error (free with g_error_free). `length` may be NULL. */
gboolean g_file_get_contents(const gchar *filename, gchar **contents, gsize *length, GError **error);

/* Write `length` bytes of `contents` to `filename` (length -1 = strlen). On
 * failure returns FALSE and, if `error` is non-NULL, sets *error. */
gboolean g_file_set_contents(const gchar *filename, const gchar *contents, gssize length, GError **error);

/* GIOChannel: just the sliver sys.c uses to append to a crash log. */
typedef enum {
	G_IO_STATUS_ERROR,
	G_IO_STATUS_NORMAL,
	G_IO_STATUS_EOF,
	G_IO_STATUS_AGAIN
} GIOStatus;

typedef struct _GIOChannel GIOChannel;

/* Open `filename` with the libc `mode` string (e.g. "w", "a"). On failure
 * returns NULL and, if `error` is non-NULL, sets *error. */
GIOChannel *g_io_channel_new_file(const gchar *filename, const gchar *mode, GError **error);

/* Write `count` bytes of `buf` (count -1 = strlen), reporting the number written
 * via `bytes_written` (may be NULL). Returns G_IO_STATUS_NORMAL on success. */
GIOStatus g_io_channel_write_chars(GIOChannel *channel, const gchar *buf, gssize count, gsize *bytes_written, GError **error);

/* Flush and close the channel (glib flushes regardless of `flush` when closing). */
GIOStatus g_io_channel_shutdown(GIOChannel *channel, gboolean flush, GError **error);

/* Drop a reference; closes the underlying file if still open and frees `channel`. */
void g_io_channel_unref(GIOChannel *channel);

#endif /* QGLIB_IO_H */
