/*
 * Standalone unit tests for qglib's GDateTime / UUID / URI primitives.
 * Dual-backend: builds against qglib or system glib.
 *
 * The g_uri_escape_string cases are differential (exact output is compared, so
 * they are validated against real glib). The UUID and GDateTime cases are
 * structure-only: their values are random / wall-clock dependent, so we assert
 * format/shape (length, separator positions, version/variant nibbles, a 4-digit
 * year) rather than comparing to glib.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef QG_USE_REAL_GLIB
#include <glib.h>
#define TFREE(p) g_free(p)
#else
#include "qglib_misc.h"
/* The qglib backend defaults its allocator to the C stdlib (see qglib.h), and
 * we link only qglib_misc.c here — not qglib_util.c, where g_free lives — so
 * release results with the matching stdlib free() to avoid an extra link dep. */
#define TFREE(p) free(p)
#endif

static int qg_failures = 0;
static int qg_checks = 0;

#define CHECK(cond, msg) do { \
	qg_checks++; \
	if (!(cond)) { printf("FAIL: %s (%s:%d)\n", (msg), __FILE__, __LINE__); qg_failures++; } \
} while (0)

/* ---- g_uri_escape_string: differential (oracle-validated vs real glib) ---- */

static void test_misc_uri_escape(void) {
	gchar *r = g_uri_escape_string("a b/c?", NULL, FALSE);
	CHECK(r != NULL && strcmp(r, "a%20b%2Fc%3F") == 0, "uri_escape space/slash/question -> %XX uppercase");
	TFREE(r);

	r = g_uri_escape_string("a b/c?", "/", FALSE);
	CHECK(r != NULL && strcmp(r, "a%20b/c%3F") == 0, "uri_escape keeps '/' when reserved-allowed");
	TFREE(r);

	r = g_uri_escape_string("ABCabc019-_.~", NULL, FALSE);
	CHECK(r != NULL && strcmp(r, "ABCabc019-_.~") == 0, "uri_escape unreserved set passes through");
	TFREE(r);

	r = g_uri_escape_string("a&b=c+d", NULL, FALSE);
	CHECK(r != NULL && strcmp(r, "a%26b%3Dc%2Bd") == 0, "uri_escape encodes &=+");
	TFREE(r);

	r = g_uri_escape_string("", NULL, FALSE);
	CHECK(r != NULL && strcmp(r, "") == 0, "uri_escape empty string -> empty");
	TFREE(r);
}

/* ---- g_uuid_string_random: structure-only (random value) ---- */

static void test_misc_uuid(void) {
	gchar *u = g_uuid_string_random();
	CHECK(u != NULL, "uuid non-NULL");
	if (u) {
		CHECK(strlen(u) == 36, "uuid is 36 chars");
		CHECK(u[8] == '-' && u[13] == '-' && u[18] == '-' && u[23] == '-',
		      "uuid hyphens at 8/13/18/23");
		CHECK(u[14] == '4', "uuid version nibble is '4'");
		CHECK(strchr("89ab", u[19]) != NULL, "uuid variant nibble in [89ab]");
		int ok = 1;
		for (int i = 0; i < 36; i++) {
			if (i == 8 || i == 13 || i == 18 || i == 23) {
				continue; /* hyphen positions */
			}
			const char c = u[i];
			if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) {
				ok = 0;
			}
		}
		CHECK(ok, "uuid body is lowercase hex");
	}
	TFREE(u);

	/* Two draws should differ (random source actually varies). */
	gchar *a = g_uuid_string_random();
	gchar *b = g_uuid_string_random();
	CHECK(a && b && strcmp(a, b) != 0, "two uuids differ");
	TFREE(a);
	TFREE(b);
}

/* ---- GDateTime: structure-only (wall-clock dependent, timezone-robust) ---- */

static void test_misc_datetime(void) {
	GDateTime *dt = g_date_time_new_now_local();
	CHECK(dt != NULL, "date_time_new_now_local non-NULL");

	gchar *year = g_date_time_format(dt, "%Y");
	CHECK(year != NULL && strlen(year) == 4, "format %Y is 4 digits");
	if (year && strlen(year) == 4) {
		int alldigits = 1;
		for (int i = 0; i < 4; i++) {
			if (year[i] < '0' || year[i] > '9') {
				alldigits = 0;
			}
		}
		CHECK(alldigits, "format %Y is all digits");
		CHECK(strcmp(year, "2024") >= 0, "format %Y >= 2024");
	}
	TFREE(year);

	/* Shape of the engine's actual format string, without asserting H:M:S. */
	gchar *ts = g_date_time_format(dt, "%Y-%m-%d %H:%M:%S");
	CHECK(ts != NULL && strlen(ts) == 19, "format full timestamp is 19 chars");
	if (ts && strlen(ts) == 19) {
		CHECK(ts[4] == '-' && ts[7] == '-' && ts[10] == ' ' && ts[13] == ':' && ts[16] == ':',
		      "full timestamp punctuation at expected offsets");
	}
	TFREE(ts);

	g_date_time_unref(dt);
}

int main(void) {
	test_misc_uri_escape();
	test_misc_uuid();
	test_misc_datetime();

	if (qg_failures == 0) {
		printf("ALL PASS (%d checks)\n", qg_checks);
		return 0;
	}
	printf("%d FAILURE(S) of %d checks\n", qg_failures, qg_checks);
	return 1;
}
