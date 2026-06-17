/*
 * Standalone unit tests for the qglib GString container.
 *
 * Built without SDL or the Quetoo toolchain (uses the stdlib QG_MALLOC default)
 * so the container semantics can be locked in independently of the engine build.
 * Returns non-zero if any check fails.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Differential harness: the same tests build against either backend.
 *   make test       -> qglib (this layer)
 *   make test-glib  -> system glib-2.0 (must produce identical results)
 * This proves qglib is behaviourally faithful to the library it replaces.
 */
#ifdef QG_USE_REAL_GLIB
#include <glib.h>
#define TEST_MALLOC(n) g_malloc(n)
#define TEST_FREE(p)   g_free(p)
#else
#include "qglib_string.h"
#define TEST_MALLOC(n) malloc(n)
#define TEST_FREE(p)   free(p)
#endif

static int qg_failures = 0;
static int qg_checks = 0;

#define CHECK(cond, msg) do { \
	qg_checks++; \
	if (!(cond)) { printf("FAIL: %s (%s:%d)\n", (msg), __FILE__, __LINE__); qg_failures++; } \
} while (0)

/* ---- GString ---- */

static void test_string_new_from_init(void) {
	GString *s = g_string_new("hello");
	CHECK(s->len == 5, "new: len matches init");
	CHECK(strcmp(s->str, "hello") == 0, "new: str matches init");
	CHECK(s->str[s->len] == '\0', "new: str is NUL-terminated at len");
	CHECK(s->allocated_len >= s->len + 1, "new: allocated_len covers data plus NUL");
	g_string_free(s, TRUE);
}

static void test_string_new_null_is_empty(void) {
	GString *s = g_string_new(NULL);
	CHECK(s->len == 0, "new(NULL): empty len");
	CHECK(s->str != NULL && s->str[0] == '\0', "new(NULL): str is empty string, not NULL");
	g_string_free(s, TRUE);
}

static void test_string_new_empty(void) {
	GString *s = g_string_new("");
	CHECK(s->len == 0, "new(\"\"): empty len");
	CHECK(s->str[0] == '\0', "new(\"\"): NUL-terminated");
	g_string_free(s, TRUE);
}

static void test_string_sized_new(void) {
	GString *s = g_string_sized_new(64);
	CHECK(s->len == 0, "sized_new: starts empty");
	CHECK(s->str != NULL && s->str[0] == '\0', "sized_new: NUL-terminated empty string");
	CHECK(s->allocated_len >= 64, "sized_new: reserves at least requested capacity");
	g_string_free(s, TRUE);
}

static void test_string_append(void) {
	GString *s = g_string_new("foo");
	GString *r = g_string_append(s, "bar");
	CHECK(r == s, "append: returns same string");
	CHECK(s->len == 6, "append: len after append");
	CHECK(strcmp(s->str, "foobar") == 0, "append: content concatenated");
	CHECK(s->str[s->len] == '\0', "append: NUL-terminated");
	g_string_free(s, TRUE);
}

static void test_string_append_empty(void) {
	GString *s = g_string_new("keep");
	g_string_append(s, "");
	CHECK(s->len == 4, "append empty: len unchanged");
	CHECK(strcmp(s->str, "keep") == 0, "append empty: content unchanged");
	g_string_free(s, TRUE);
}

static void test_string_append_grows(void) {
	GString *s = g_string_new(NULL);
	for (int i = 0; i < 100; i++) {
		g_string_append(s, "ab");
	}
	CHECK(s->len == 200, "append grows: len after many appends");
	CHECK(s->str[0] == 'a' && s->str[199] == 'b', "append grows: endpoints intact");
	CHECK(s->str[200] == '\0', "append grows: NUL-terminated after growth");
	CHECK(s->allocated_len >= 201, "append grows: capacity covers data plus NUL");
	g_string_free(s, TRUE);
}

static void test_string_append_c(void) {
	GString *s = g_string_new("ab");
	GString *r = g_string_append_c(s, 'c');
	CHECK(r == s, "append_c: returns same string");
	CHECK(s->len == 3, "append_c: len incremented by one");
	CHECK(strcmp(s->str, "abc") == 0, "append_c: char appended");
	CHECK(s->str[s->len] == '\0', "append_c: NUL-terminated");
	g_string_free(s, TRUE);
}

static void test_string_append_c_many(void) {
	GString *s = g_string_new(NULL);
	for (int i = 0; i < 50; i++) {
		g_string_append_c(s, 'x');
	}
	CHECK(s->len == 50, "append_c many: len after 50 chars");
	CHECK(s->str[49] == 'x' && s->str[50] == '\0', "append_c many: terminated");
	g_string_free(s, TRUE);
}

static void test_string_append_printf(void) {
	GString *s = g_string_new("n=");
	g_string_append_printf(s, "%d/%s", 42, "ok");
	CHECK(strcmp(s->str, "n=42/ok") == 0, "append_printf: formatted text appended");
	CHECK(s->len == 7, "append_printf: len matches result");
	CHECK(s->str[s->len] == '\0', "append_printf: NUL-terminated");
	g_string_free(s, TRUE);
}

static void test_string_append_printf_long(void) {
	GString *s = g_string_new(NULL);
	g_string_append_printf(s, "%080d", 1); /* 80 chars, forces growth from empty */
	CHECK(s->len == 80, "append_printf long: len matches wide field");
	CHECK(s->str[0] == '0' && s->str[79] == '1', "append_printf long: content correct");
	CHECK(s->str[80] == '\0', "append_printf long: NUL-terminated");
	g_string_free(s, TRUE);
}

static void test_string_append_printf_accumulates(void) {
	GString *s = g_string_new(NULL);
	g_string_append_printf(s, "[%d]", 1);
	g_string_append_printf(s, "[%d]", 2);
	CHECK(strcmp(s->str, "[1][2]") == 0, "append_printf: accumulates across calls");
	g_string_free(s, TRUE);
}

static void test_string_truncate(void) {
	GString *s = g_string_new("abcdef");
	GString *r = g_string_truncate(s, 3);
	CHECK(r == s, "truncate: returns same string");
	CHECK(s->len == 3, "truncate: len reduced");
	CHECK(strcmp(s->str, "abc") == 0, "truncate: content cut");
	CHECK(s->str[3] == '\0', "truncate: NUL at new length");
	g_string_free(s, TRUE);
}

static void test_string_truncate_noop_when_longer(void) {
	GString *s = g_string_new("abc");
	g_string_truncate(s, 10); /* glib clamps: len >= current is a no-op */
	CHECK(s->len == 3, "truncate beyond len: clamped, len unchanged");
	CHECK(strcmp(s->str, "abc") == 0, "truncate beyond len: content unchanged");
	g_string_free(s, TRUE);
}

static void test_string_truncate_zero(void) {
	GString *s = g_string_new("abc");
	g_string_truncate(s, 0);
	CHECK(s->len == 0, "truncate 0: empty len");
	CHECK(s->str[0] == '\0', "truncate 0: empty string");
	g_string_free(s, TRUE);
}

static void test_string_append_after_truncate(void) {
	GString *s = g_string_new("abcdef");
	g_string_truncate(s, 2);
	g_string_append(s, "XY");
	CHECK(strcmp(s->str, "abXY") == 0, "append after truncate: rebuilds from new tail");
	CHECK(s->len == 4, "append after truncate: len correct");
	g_string_free(s, TRUE);
}

static void test_string_free_true_returns_null(void) {
	GString *s = g_string_new("gone");
	gchar *r = g_string_free(s, TRUE);
	CHECK(r == NULL, "free(TRUE): returns NULL");
}

static void test_string_free_false_returns_buffer(void) {
	GString *s = g_string_new("keepme");
	gchar *buf = g_string_free(s, FALSE);
	CHECK(buf != NULL, "free(FALSE): returns the buffer");
	CHECK(strcmp(buf, "keepme") == 0, "free(FALSE): buffer holds the data");
	TEST_FREE(buf);
}

int main(void) {
	test_string_new_from_init();
	test_string_new_null_is_empty();
	test_string_new_empty();
	test_string_sized_new();
	test_string_append();
	test_string_append_empty();
	test_string_append_grows();
	test_string_append_c();
	test_string_append_c_many();
	test_string_append_printf();
	test_string_append_printf_long();
	test_string_append_printf_accumulates();
	test_string_truncate();
	test_string_truncate_noop_when_longer();
	test_string_truncate_zero();
	test_string_append_after_truncate();
	test_string_free_true_returns_null();
	test_string_free_false_returns_buffer();

	if (qg_failures == 0) {
		printf("ALL PASS (%d checks)\n", qg_checks);
		return 0;
	}
	printf("%d FAILURE(S) of %d checks\n", qg_failures, qg_checks);
	return 1;
}
