/*
 * qglib_io.c — file I/O + user-name primitives. See qglib_io.h.
 * POSIX-targeted (Android/Linux), over libc stdio + getenv. The differential
 * test (test_io.c) pins behaviour against real glib.
 */
#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "qglib_io.h"

/* Allocate a GError carrying strerror(err) for `code` (matches qglib_regex.c's
 * layout: domain unused, message g_malloc'd, freed by g_error_free). */
static void qg_io_set_error(GError **error, gint code, const gchar *what, const gchar *filename) {
	if (!error) {
		return;
	}
	GError *e = g_malloc(sizeof(GError));
	e->domain = 0;
	e->code = code;
	e->message = g_strdup_printf("%s '%s': %s", what, filename, strerror(code));
	*error = e;
}

const gchar *g_get_user_name(void) {
	static gchar *name = NULL;
	if (!name) {
		const char *e = getenv("USER");
		if (!e || !*e) {
			e = getenv("LOGNAME");
		}
		name = g_strdup(e && *e ? e : "");
	}
	return name;
}

gboolean g_file_get_contents(const gchar *filename, gchar **contents, gsize *length, GError **error) {
	FILE *f = fopen(filename, "rb");
	if (!f) {
		qg_io_set_error(error, errno, "Failed to open file", filename);
		return FALSE;
	}

	gsize cap = 0, len = 0;
	gchar *buf = NULL;
	for (;;) {
		if (len + 1 >= cap) {
			const gsize ncap = cap ? cap * 2 : 4096;
			gchar *nbuf = g_realloc(buf, ncap);
			if (!nbuf) {
				g_free(buf);
				fclose(f);
				qg_io_set_error(error, ENOMEM, "Failed to read file", filename);
				return FALSE;
			}
			buf = nbuf;
			cap = ncap;
		}
		const gsize got = fread(buf + len, 1, cap - len - 1, f);
		len += got;
		if (got == 0) {
			break;
		}
	}

	if (ferror(f)) {
		const gint code = errno;
		g_free(buf);
		fclose(f);
		qg_io_set_error(error, code, "Failed to read file", filename);
		return FALSE;
	}
	fclose(f);

	if (!buf) {
		buf = g_malloc(1); /* empty file: still hand back a NUL-terminated buffer */
	}
	buf[len] = '\0';

	*contents = buf;
	if (length) {
		*length = len;
	}
	return TRUE;
}

gboolean g_file_set_contents(const gchar *filename, const gchar *contents, gssize length, GError **error) {
	const gsize n = (length < 0) ? strlen(contents) : (gsize) length;

	FILE *f = fopen(filename, "wb");
	if (!f) {
		qg_io_set_error(error, errno, "Failed to open file", filename);
		return FALSE;
	}

	const gboolean ok = (n == 0) || (fwrite(contents, 1, n, f) == n);
	if (!ok) {
		const gint code = errno;
		fclose(f);
		qg_io_set_error(error, code, "Failed to write file", filename);
		return FALSE;
	}

	if (fclose(f) != 0) {
		qg_io_set_error(error, errno, "Failed to write file", filename);
		return FALSE;
	}
	return TRUE;
}

/* ---- GIOChannel (libc FILE* wrapper) ---- */

struct _GIOChannel {
	FILE *fp;
	gchar *name; /* retained for error messages */
};

GIOChannel *g_io_channel_new_file(const gchar *filename, const gchar *mode, GError **error) {
	FILE *fp = fopen(filename, mode);
	if (!fp) {
		qg_io_set_error(error, errno, "Failed to open file", filename);
		return NULL;
	}
	GIOChannel *ch = g_malloc(sizeof(GIOChannel));
	ch->fp = fp;
	ch->name = g_strdup(filename);
	return ch;
}

GIOStatus g_io_channel_write_chars(GIOChannel *channel, const gchar *buf, gssize count, gsize *bytes_written, GError **error) {
	const gsize n = (count < 0) ? strlen(buf) : (gsize) count;
	const gsize wrote = (n == 0) ? 0 : fwrite(buf, 1, n, channel->fp);
	if (bytes_written) {
		*bytes_written = wrote;
	}
	if (wrote != n) {
		qg_io_set_error(error, errno, "Failed to write file", channel->name);
		return G_IO_STATUS_ERROR;
	}
	return G_IO_STATUS_NORMAL;
}

GIOStatus g_io_channel_shutdown(GIOChannel *channel, gboolean flush, GError **error) {
	(void) flush; /* closing flushes regardless; glib flushes on shutdown too */
	GIOStatus status = G_IO_STATUS_NORMAL;
	if (channel->fp) {
		if (fflush(channel->fp) != 0) {
			qg_io_set_error(error, errno, "Failed to flush file", channel->name);
			status = G_IO_STATUS_ERROR;
		}
		if (fclose(channel->fp) != 0 && status == G_IO_STATUS_NORMAL) {
			qg_io_set_error(error, errno, "Failed to close file", channel->name);
			status = G_IO_STATUS_ERROR;
		}
		channel->fp = NULL;
	}
	return status;
}

void g_io_channel_unref(GIOChannel *channel) {
	if (channel) {
		if (channel->fp) {
			fclose(channel->fp);
		}
		g_free(channel->name);
		g_free(channel);
	}
}
