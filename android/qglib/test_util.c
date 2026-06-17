/*
 * Standalone unit tests for qglib's string/format/memory/filesystem/time
 * primitives. Dual-backend: builds against qglib or system glib.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifdef QG_USE_REAL_GLIB
#include <glib.h>
#include <glib/gstdio.h>
#else
#include "qglib_util.h"
#endif

static int qg_failures = 0;
static int qg_checks = 0;

#define CHECK(cond, msg) do { \
	qg_checks++; \
	if (!(cond)) { printf("FAIL: %s (%s:%d)\n", (msg), __FILE__, __LINE__); qg_failures++; } \
} while (0)

static void test_util_strlcpy(void) {
	char buf[8];
	gsize r = g_strlcpy(buf, "abc", sizeof buf);
	CHECK(r == 3 && strcmp(buf, "abc") == 0, "strlcpy basic copy + return length");
	char small[4];
	r = g_strlcpy(small, "abcdef", sizeof small);
	CHECK(r == 6 && strcmp(small, "abc") == 0, "strlcpy truncates but returns full src length");
}

static void test_util_strlcat(void) {
	char buf[8] = "ab";
	gsize r = g_strlcat(buf, "cd", sizeof buf);
	CHECK(r == 4 && strcmp(buf, "abcd") == 0, "strlcat basic append");
	char small[5] = "ab";
	r = g_strlcat(small, "xyz", sizeof small);
	CHECK(r == 5 && strcmp(small, "abxy") == 0, "strlcat truncates, returns intended length");
}

static void test_util_snprintf(void) {
	char buf[16];
	gint r = g_snprintf(buf, sizeof buf, "%d-%s", 42, "x");
	CHECK(r == 4 && strcmp(buf, "42-x") == 0, "snprintf formats and returns length");
}

static void test_util_ascii_casecmp(void) {
	CHECK(g_ascii_strcasecmp("ABC", "abc") == 0, "ascii_strcasecmp equal ignoring case");
	CHECK(g_ascii_strcasecmp("abc", "abd") < 0, "ascii_strcasecmp less");
	CHECK(g_ascii_strcasecmp("abd", "abc") > 0, "ascii_strcasecmp greater");
	CHECK(g_ascii_strncasecmp("ABCxx", "abcyy", 3) == 0, "ascii_strncasecmp equal prefix");
	CHECK(g_ascii_strncasecmp("abc", "abd", 3) < 0, "ascii_strncasecmp differs within n");
}

static void test_util_prefix_suffix(void) {
	CHECK(g_str_has_prefix("foobar", "foo") == TRUE, "has_prefix true");
	CHECK(g_str_has_prefix("foobar", "bar") == FALSE, "has_prefix false");
	CHECK(g_str_has_suffix("foobar", "bar") == TRUE, "has_suffix true");
	CHECK(g_str_has_suffix("foobar", "foo") == FALSE, "has_suffix false");
	CHECK(g_strcmp0(NULL, NULL) == 0, "strcmp0 both NULL equal");
	CHECK(g_strcmp0(NULL, "a") < 0, "strcmp0 NULL sorts first");
	CHECK(g_strcmp0("a", NULL) > 0, "strcmp0 non-NULL after NULL");
	CHECK(g_strcmp0("a", "a") == 0, "strcmp0 equal strings");
	CHECK(g_strcmp0("a", "b") < 0, "strcmp0 ordering");
}

static void test_util_strdup(void) {
	gchar *d = g_strdup("hi");
	CHECK(d != NULL && strcmp(d, "hi") == 0, "strdup copies");
	g_free(d);
	CHECK(g_strdup(NULL) == NULL, "strdup(NULL) is NULL");
	gchar *p = g_strdup_printf("%d.%d", 1, 2);
	CHECK(strcmp(p, "1.2") == 0, "strdup_printf");
	g_free(p);
}

static void test_util_strsplit(void) {
	gchar **v = g_strsplit("a:b:c", ":", 0);
	gint n = 0;
	while (v[n]) { n++; }
	CHECK(n == 3 && strcmp(v[0], "a") == 0 && strcmp(v[2], "c") == 0, "strsplit into 3 tokens");
	CHECK(g_strv_length(v) == 3, "strv_length counts 3 tokens");
	g_strfreev(v);
	v = g_strsplit("a:b:c", ":", 2);
	CHECK(v[0] && v[1] && !v[2] && strcmp(v[0], "a") == 0 && strcmp(v[1], "b:c") == 0,
	      "strsplit honors max_tokens (remainder in last)");
	CHECK(g_strv_length(v) == 2, "strv_length counts 2 tokens (max_tokens split)");
	g_strfreev(v);
}

static void test_util_malloc(void) {
	int *p = g_malloc0(4 * sizeof(int));
	CHECK(p[0] == 0 && p[3] == 0, "malloc0 zero-initializes");
	g_free(p);
	CHECK(g_malloc(0) == NULL, "malloc(0) is NULL");
}

static void test_util_build_filename(void) {
	gchar *p = g_build_filename("a", "b", "c", (gchar *) NULL);
	CHECK(strcmp(p, "a" G_DIR_SEPARATOR_S "b" G_DIR_SEPARATOR_S "c") == 0, "build_filename joins");
	g_free(p);
	p = g_build_filename("a" G_DIR_SEPARATOR_S, G_DIR_SEPARATOR_S "b", (gchar *) NULL);
	CHECK(strcmp(p, "a" G_DIR_SEPARATOR_S "b") == 0, "build_filename collapses separators at join");
	g_free(p);
}

static void test_util_build_path(void) {
	gchar *p = g_build_path("/", "a", "b", "c", (gchar *) NULL);
	CHECK(strcmp(p, "a/b/c") == 0, "build_path joins with separator");
	g_free(p);
	p = g_build_path("/", "a/", "/b", (gchar *) NULL);
	CHECK(strcmp(p, "a/b") == 0, "build_path collapses separators at join");
	g_free(p);
}

static void test_util_file_test(void) {
	CHECK(g_file_test("test_util.c", G_FILE_TEST_EXISTS) == TRUE, "file_test EXISTS on a real file");
	CHECK(g_file_test("no_such_file_xyz_123", G_FILE_TEST_EXISTS) == FALSE, "file_test miss");
	CHECK(g_file_test(".", G_FILE_TEST_IS_DIR) == TRUE, "file_test IS_DIR on cwd");
}

static void test_util_mkdir_with_parents(void) {
	const char *dir = "qg_tmp_test" G_DIR_SEPARATOR_S "a" G_DIR_SEPARATOR_S "b";
	gint r = g_mkdir_with_parents(dir, 0755);
	CHECK(r == 0, "mkdir_with_parents returns 0");
	CHECK(g_file_test(dir, G_FILE_TEST_IS_DIR) == TRUE, "mkdir_with_parents created nested dir");
	rmdir("qg_tmp_test" G_DIR_SEPARATOR_S "a" G_DIR_SEPARATOR_S "b");
	rmdir("qg_tmp_test" G_DIR_SEPARATOR_S "a");
	rmdir("qg_tmp_test");
}

static void test_util_monotonic_time(void) {
	gint64 t1 = g_get_monotonic_time();
	gint64 t2 = g_get_monotonic_time();
	CHECK(t1 > 0, "monotonic time is positive");
	CHECK(t2 >= t1, "monotonic time is non-decreasing");
}

static void test_util_strip_and_paths(void) {
	char buf[] = "  hi  ";
	CHECK(strcmp(g_strstrip(buf), "hi") == 0, "strstrip trims both ends");
	const gchar *r = g_strrstr("a/b/c", "/");
	CHECK(r != NULL && strcmp(r, "/c") == 0, "strrstr finds last occurrence");
	gchar *d = g_ascii_strdown("AbC", -1);
	CHECK(strcmp(d, "abc") == 0, "ascii_strdown lowercases");
	g_free(d);
	gchar *b = g_path_get_basename("/foo/bar");
	CHECK(strcmp(b, "bar") == 0, "path_get_basename");
	g_free(b);
	b = g_path_get_basename("foo");
	CHECK(strcmp(b, "foo") == 0, "path_get_basename no-separator");
	g_free(b);
	gchar *dir = g_path_get_dirname("/foo/bar");
	CHECK(strcmp(dir, "/foo") == 0, "path_get_dirname");
	g_free(dir);
	dir = g_path_get_dirname("foo");
	CHECK(strcmp(dir, ".") == 0, "path_get_dirname no-separator is dot");
	g_free(dir);
	CHECK(g_get_home_dir() != NULL, "get_home_dir non-NULL");
	CHECK(g_get_user_data_dir() != NULL, "get_user_data_dir non-NULL");
}

static void test_util_gstdio(void) {
	const char *path = "qg_gstdio_test.tmp";
	FILE *f = g_fopen(path, "w");
	CHECK(f != NULL, "g_fopen opens for write");
	if (f) {
		fputs("hi", f);
		fclose(f);
	}
	CHECK(g_file_test(path, G_FILE_TEST_EXISTS), "file exists after g_fopen");
	CHECK(g_remove(path) == 0, "g_remove deletes the file");
	CHECK(!g_file_test(path, G_FILE_TEST_EXISTS), "file gone after g_remove");
	f = g_fopen(path, "w");
	if (f) {
		fclose(f);
	}
	CHECK(g_unlink(path) == 0, "g_unlink deletes the file");
}

static void test_util_ascii_num(void) {
	gchar *end;
	gint64 v = g_ascii_strtoll("123abc", &end, 10);
	CHECK(v == 123 && *end == 'a', "ascii_strtoll parses value and sets endptr");
	CHECK(g_ascii_strtoll("ff", NULL, 16) == 255, "ascii_strtoll base 16");
	CHECK(g_ascii_isalnum('a') && g_ascii_isalnum('Z') && g_ascii_isalnum('5'), "ascii_isalnum true cases");
	CHECK(!g_ascii_isalnum('-') && !g_ascii_isalnum(' '), "ascii_isalnum false cases");
}

int main(void) {
	test_util_strlcpy();
	test_util_strlcat();
	test_util_snprintf();
	test_util_ascii_casecmp();
	test_util_prefix_suffix();
	test_util_strdup();
	test_util_strsplit();
	test_util_malloc();
	test_util_build_filename();
	test_util_build_path();
	test_util_file_test();
	test_util_mkdir_with_parents();
	test_util_monotonic_time();
	test_util_strip_and_paths();
	test_util_ascii_num();
	test_util_gstdio();

	if (qg_failures == 0) {
		printf("ALL PASS (%d checks)\n", qg_checks);
		return 0;
	}
	printf("%d FAILURE(S) of %d checks\n", qg_failures, qg_checks);
	return 1;
}
