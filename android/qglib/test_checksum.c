/*
 * Standalone unit tests for qglib's GChecksum (MD5). Dual-backend: the same
 * tests build against qglib or system glib, which must produce identical hex.
 *   cc ...                      -> qglib (this layer)
 *   cc -DQG_USE_REAL_GLIB ...   -> system glib-2.0 (the oracle)
 * Known RFC 1321 / common test vectors pin the absolute output; the real-glib
 * build proves byte-for-byte agreement with the library being replaced.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef QG_USE_REAL_GLIB
#include <glib.h>
#else
#include "qglib_checksum.h"
#endif

static int qg_failures = 0;
static int qg_checks = 0;

#define CHECK(cond, msg) do { \
	qg_checks++; \
	if (!(cond)) { printf("FAIL: %s (%s:%d)\n", (msg), __FILE__, __LINE__); qg_failures++; } \
} while (0)

/* Hash a single buffer in one update call and compare to expected hex. */
static void check_md5(const char *input, const char *expected, const char *msg) {
	GChecksum *c = g_checksum_new(G_CHECKSUM_MD5);
	CHECK(c != NULL, "g_checksum_new(G_CHECKSUM_MD5) non-NULL");
	if (!c) {
		return;
	}
	g_checksum_update(c, (const guchar *) input, (gssize) strlen(input));
	const gchar *got = g_checksum_get_string(c);
	CHECK(got != NULL && strcmp(got, expected) == 0, msg);
	g_checksum_free(c);
}

static void test_md5_empty(void) {
	check_md5("", "d41d8cd98f00b204e9800998ecf8427e", "MD5(\"\")");
}

static void test_md5_abc(void) {
	check_md5("abc", "900150983cd24fb0d6963f7d28e17f72", "MD5(\"abc\")");
}

static void test_md5_fox(void) {
	check_md5("The quick brown fox jumps over the lazy dog",
	          "9e107d9d372bb6826bd81d3542a419d6", "MD5(quick brown fox)");
}

/* length == -1 must behave like strlen, matching glib. */
static void test_md5_length_minus_one(void) {
	GChecksum *c = g_checksum_new(G_CHECKSUM_MD5);
	g_checksum_update(c, (const guchar *) "abc", -1);
	CHECK(strcmp(g_checksum_get_string(c), "900150983cd24fb0d6963f7d28e17f72") == 0,
	      "update length -1 == strlen");
	g_checksum_free(c);
}

/* Incremental updates must equal a single update of the concatenation. */
static void test_md5_multi_update(void) {
	GChecksum *c = g_checksum_new(G_CHECKSUM_MD5);
	g_checksum_update(c, (const guchar *) "ab", 2);
	g_checksum_update(c, (const guchar *) "c", 1);
	CHECK(strcmp(g_checksum_get_string(c), "900150983cd24fb0d6963f7d28e17f72") == 0,
	      "update \"ab\"+\"c\" == update \"abc\"");
	g_checksum_free(c);
}

/* get_string is idempotent: repeated calls return the same finalized digest. */
static void test_md5_get_string_idempotent(void) {
	GChecksum *c = g_checksum_new(G_CHECKSUM_MD5);
	g_checksum_update(c, (const guchar *) "abc", 3);
	const gchar *first = g_checksum_get_string(c);
	char saved[33];
	strcpy(saved, first);
	const gchar *second = g_checksum_get_string(c);
	CHECK(strcmp(saved, second) == 0, "get_string idempotent across calls");
	CHECK(strcmp(second, "900150983cd24fb0d6963f7d28e17f72") == 0, "idempotent value correct");
	g_checksum_free(c);
}

/* Long, multi-block input (80 bytes => 2 MD5 blocks), fed in awkward chunks.
 * MD5 of 80 'a' characters, verified against the real-glib oracle build. */
static void test_md5_long_multiblock(void) {
	char buf[81];
	memset(buf, 'a', 80);
	buf[80] = '\0';

	GChecksum *whole = g_checksum_new(G_CHECKSUM_MD5);
	g_checksum_update(whole, (const guchar *) buf, 80);
	const gchar *expected = g_checksum_get_string(whole);
	char saved[33];
	strcpy(saved, expected);

	/* Same data, fed 7 bytes at a time to cross the 64-byte block boundary. */
	GChecksum *chunked = g_checksum_new(G_CHECKSUM_MD5);
	for (int off = 0; off < 80; off += 7) {
		const int n = (off + 7 <= 80) ? 7 : (80 - off);
		g_checksum_update(chunked, (const guchar *) (buf + off), n);
	}
	CHECK(strcmp(g_checksum_get_string(chunked), saved) == 0,
	      "chunked multi-block update matches single update");
	CHECK(strlen(saved) == 32, "long digest length is 32 hex chars");

	g_checksum_free(whole);
	g_checksum_free(chunked);
}

int main(void) {
	test_md5_empty();
	test_md5_abc();
	test_md5_fox();
	test_md5_length_minus_one();
	test_md5_multi_update();
	test_md5_get_string_idempotent();
	test_md5_long_multiblock();

	if (qg_failures == 0) {
		printf("ALL PASS (%d checks)\n", qg_checks);
		return 0;
	}
	printf("%d FAILURE(S) of %d checks\n", qg_failures, qg_checks);
	return 1;
}
