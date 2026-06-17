/*
 * Standalone unit tests for qglib's file I/O + user-name primitives.
 * Dual-backend: builds against qglib or system glib.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifdef QG_USE_REAL_GLIB
#include <glib.h>
#else
#include "qglib_io.h"
#endif

static int qg_failures = 0;
static int qg_checks = 0;

#define CHECK(cond, msg) do { \
	qg_checks++; \
	if (!(cond)) { printf("FAIL: %s (%s:%d)\n", (msg), __FILE__, __LINE__); qg_failures++; } \
} while (0)

static void test_io_set_get_roundtrip(void) {
	const char *path = "qg_io_roundtrip.tmp";
	/* Include an embedded NUL so we exercise byte-exact (not strlen) handling. */
	const char data[] = "line one\nline two\0binary\x01\x02tail";
	const gsize data_len = sizeof(data) - 1; /* drop the array's implicit terminator */

	GError *error = NULL;
	gboolean ok = g_file_set_contents(path, data, (gssize) data_len, &error);
	CHECK(ok && error == NULL, "set_contents succeeds");
	if (error) { g_error_free(error); error = NULL; }

	gchar *contents = NULL;
	gsize length = 0;
	ok = g_file_get_contents(path, &contents, &length, &error);
	CHECK(ok && error == NULL, "get_contents succeeds");
	CHECK(length == data_len, "get_contents reports exact byte length");
	CHECK(contents != NULL && memcmp(contents, data, data_len) == 0, "round-trip bytes identical");
	CHECK(contents != NULL && contents[length] == '\0', "get_contents NUL-terminates");
	if (error) { g_error_free(error); error = NULL; }
	g_free(contents);

	/* length -1 => strlen path. */
	const char *text = "hello world";
	ok = g_file_set_contents(path, text, -1, &error);
	CHECK(ok && error == NULL, "set_contents with length -1 (strlen)");
	if (error) { g_error_free(error); error = NULL; }

	contents = NULL;
	length = 0;
	ok = g_file_get_contents(path, &contents, &length, &error);
	CHECK(ok && length == strlen(text) && contents != NULL && strcmp(contents, text) == 0,
	      "round-trip strlen-sized content");
	if (error) { g_error_free(error); error = NULL; }
	g_free(contents);

	remove(path);
}

static void test_io_get_missing(void) {
	gchar *contents = NULL;
	gsize length = 123;
	GError *error = NULL;
	gboolean ok = g_file_get_contents("qg_io_no_such_file_xyz_123", &contents, &length, &error);
	CHECK(ok == FALSE, "get_contents on missing file returns FALSE");
	CHECK(error != NULL, "get_contents sets error on failure");
	CHECK(error == NULL || error->message != NULL, "error carries a message");
	if (error) { g_error_free(error); }
}

static void test_io_channel(void) {
	const char *path = "qg_io_channel.tmp";
	const char *text = "crash log line\n";

	GError *error = NULL;
	GIOChannel *ch = g_io_channel_new_file(path, "w", &error);
	CHECK(ch != NULL && error == NULL, "io_channel_new_file (w) opens");
	if (error) { g_error_free(error); error = NULL; }

	gsize written = 0;
	GIOStatus st = g_io_channel_write_chars(ch, text, -1, &written, NULL);
	CHECK(st == G_IO_STATUS_NORMAL, "write_chars returns NORMAL");
	CHECK(written == strlen(text), "write_chars reports bytes written (count -1 => strlen)");

	st = g_io_channel_shutdown(ch, TRUE, &error);
	CHECK(st == G_IO_STATUS_NORMAL && error == NULL, "io_channel_shutdown returns NORMAL");
	if (error) { g_error_free(error); error = NULL; }
	g_io_channel_unref(ch);

	/* Read it back and verify the channel actually persisted the bytes. */
	gchar *contents = NULL;
	gsize length = 0;
	gboolean ok = g_file_get_contents(path, &contents, &length, &error);
	CHECK(ok && length == strlen(text) && contents != NULL && strcmp(contents, text) == 0,
	      "io_channel output reads back identical");
	if (error) { g_error_free(error); error = NULL; }
	g_free(contents);

	remove(path);
}

static void test_io_user_name(void) {
	const gchar *user = g_get_user_name();
	CHECK(user != NULL, "get_user_name non-NULL");
	/* Cached: stable pointer across calls. */
	CHECK(g_get_user_name() == user, "get_user_name is cached (stable pointer)");
}

int main(void) {
	test_io_set_get_roundtrip();
	test_io_get_missing();
	test_io_channel();
	test_io_user_name();

	if (qg_failures == 0) {
		printf("ALL PASS (%d checks)\n", qg_checks);
		return 0;
	}
	printf("%d FAILURE(S) of %d checks\n", qg_failures, qg_checks);
	return 1;
}
