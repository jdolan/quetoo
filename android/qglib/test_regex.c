/*
 * Standalone unit tests for qglib's GRegex + GError.
 *
 * Differential harness: the same tests build against either backend.
 *   make test       -> qglib (this layer, POSIX regex.h)
 *   make test-glib  -> system glib-2.0 (PCRE; must produce identical results)
 * The replace_eval output is ORACLE-VALIDATED: real glib must produce the
 * identical string for Quetoo's simple `\$([a-z0-9_]+)` pattern.
 *
 * Mirrors src/common/cvar.c: same pattern, same compile flags, same callback
 * shape (fetch group 1, append). Returns non-zero if any check fails.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef QG_USE_REAL_GLIB
#include <glib.h>
#else
#include "qglib_regex.h"
#endif

static int qg_failures = 0;
static int qg_checks = 0;

#define CHECK(cond, msg) do { \
	qg_checks++; \
	if (!(cond)) { printf("FAIL: %s (%s:%d)\n", (msg), __FILE__, __LINE__); qg_failures++; } \
} while (0)

/* The cvar.c substitution callback, but wrapping each name in <...> so the
 * test asserts on an unambiguous, easily-checked transform. */
static gboolean wrap_name_cb(const GMatchInfo *mi, GString *result, gpointer user_data) {
	(void) user_data;
	gchar *n = g_match_info_fetch(mi, 1);
	g_string_append(result, "<");
	g_string_append(result, n);
	g_string_append(result, ">");
	g_free(n);
	return FALSE;
}

/* A callback that records the fetched group-1 name, to assert capture works. */
static gboolean capture_cb(const GMatchInfo *mi, GString *result, gpointer user_data) {
	gchar *n = g_match_info_fetch(mi, 1);
	g_strlcpy((gchar *) user_data, n, 64);
	g_string_append(result, n); /* identity substitution */
	g_free(n);
	return FALSE;
}

static GRegex *make_cvar_regex(void) {
	GError *error = NULL;
	GRegex *re = g_regex_new("\\$([a-z0-9_]+)",
	                         G_REGEX_CASELESS | G_REGEX_MULTILINE | G_REGEX_DOTALL, 0, &error);
	CHECK(re != NULL, "g_regex_new compiles the cvar pattern");
	CHECK(error == NULL, "g_regex_new sets no error on success");
	if (error) {
		g_error_free(error);
	}
	return re;
}

static void test_regex_replace_basic(void) {
	GRegex *re = make_cvar_regex();
	if (!re) {
		return;
	}
	GError *error = NULL;
	gchar *out = g_regex_replace_eval(re, "a $foo b $bar_2 c", -1, 0, 0, wrap_name_cb, NULL, &error);
	CHECK(error == NULL, "replace_eval sets no error");
	CHECK(out != NULL && strcmp(out, "a <foo> b <bar_2> c") == 0,
	      "replace_eval substitutes each $name (oracle-validated)");
	g_free(out);
	if (error) {
		g_error_free(error);
	}
}

static void test_regex_no_match(void) {
	GRegex *re = make_cvar_regex();
	if (!re) {
		return;
	}
	gchar *out = g_regex_replace_eval(re, "nothing to expand here", -1, 0, 0, wrap_name_cb, NULL, NULL);
	CHECK(out != NULL && strcmp(out, "nothing to expand here") == 0,
	      "no-match input passes through unchanged");
	g_free(out);
}

static void test_regex_match_at_start(void) {
	GRegex *re = make_cvar_regex();
	if (!re) {
		return;
	}
	gchar *out = g_regex_replace_eval(re, "$start rest", -1, 0, 0, wrap_name_cb, NULL, NULL);
	CHECK(out != NULL && strcmp(out, "<start> rest") == 0, "match at start of string");
	g_free(out);
}

static void test_regex_fetch_group(void) {
	GRegex *re = make_cvar_regex();
	if (!re) {
		return;
	}
	char captured[64] = "";
	gchar *out = g_regex_replace_eval(re, "x $name_1 y", -1, 0, 0, capture_cb, captured, NULL);
	CHECK(strcmp(captured, "name_1") == 0, "g_match_info_fetch(.,1) returns the captured name");
	CHECK(out != NULL && strcmp(out, "x name_1 y") == 0, "identity substitution round-trips");
	g_free(out);
}

static void test_regex_empty_input(void) {
	GRegex *re = make_cvar_regex();
	if (!re) {
		return;
	}
	gchar *out = g_regex_replace_eval(re, "", -1, 0, 0, wrap_name_cb, NULL, NULL);
	CHECK(out != NULL && strcmp(out, "") == 0, "empty input yields empty output");
	g_free(out);
}

int main(void) {
	test_regex_replace_basic();
	test_regex_no_match();
	test_regex_match_at_start();
	test_regex_fetch_group();
	test_regex_empty_input();

	if (qg_failures == 0) {
		printf("ALL PASS (%d checks)\n", qg_checks);
		return 0;
	}
	printf("%d FAILURE(S) of %d checks\n", qg_failures, qg_checks);
	return 1;
}
