/*
 * Standalone unit tests for qglib's GPtrArray.
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
#include "qglib_ptrarray.h"
#define TEST_MALLOC(n) malloc(n)
#define TEST_FREE(p)   free(p)
#endif

static int qg_failures = 0;
static int qg_checks = 0;
static int qg_destroy_count = 0;

#define CHECK(cond, msg) do { \
	qg_checks++; \
	if (!(cond)) { printf("FAIL: %s (%s:%d)\n", (msg), __FILE__, __LINE__); qg_failures++; } \
} while (0)

/* ---- helpers ---- */

static void count_and_free(gpointer p) {
	qg_destroy_count++;
	TEST_FREE(p);
}

static void accumulate_int(gpointer data, gpointer user) {
	*(int *) user += GPOINTER_TO_INT(data);
}

static int cmp_str(gconstpointer a, gconstpointer b) {
	return strcmp((const char *) a, (const char *) b);
}

static int cmp_str_data(gconstpointer a, gconstpointer b, gpointer user_data) {
	*(int *) user_data += 1; /* prove user_data threads through */
	return strcmp((const char *) a, (const char *) b);
}

/* ---- GPtrArray ---- */

static void test_ptr_array_new_is_empty(void) {
	GPtrArray *a = g_ptr_array_new();
	CHECK(a->len == 0, "new array len is 0");
	CHECK(a->pdata == NULL, "new array pdata is NULL");
	g_ptr_array_free(a, TRUE);
}

static void test_ptr_array_add_and_index(void) {
	GPtrArray *a = g_ptr_array_new();
	for (int i = 0; i < 5; i++) {
		g_ptr_array_add(a, GINT_TO_POINTER(i * 10));
	}
	CHECK(a->len == 5, "len after 5 adds");
	CHECK(GPOINTER_TO_INT(g_ptr_array_index(a, 0)) == 0, "index 0");
	CHECK(GPOINTER_TO_INT(g_ptr_array_index(a, 4)) == 40, "index 4");
	g_ptr_array_free(a, TRUE);
}

static void test_ptr_array_index_is_lvalue(void) {
	GPtrArray *a = g_ptr_array_new();
	g_ptr_array_add(a, GINT_TO_POINTER(7));
	g_ptr_array_index(a, 0) = GINT_TO_POINTER(99);
	CHECK(GPOINTER_TO_INT(g_ptr_array_index(a, 0)) == 99, "index usable as lvalue");
	g_ptr_array_free(a, TRUE);
}

static void test_ptr_array_remove_index_preserves_order(void) {
	GPtrArray *a = g_ptr_array_new();
	for (int i = 0; i < 5; i++) { g_ptr_array_add(a, GINT_TO_POINTER(i)); } /* 0 1 2 3 4 */
	gpointer removed = g_ptr_array_remove_index(a, 1);                      /* 0 2 3 4   */
	CHECK(GPOINTER_TO_INT(removed) == 1, "remove_index returns the removed element");
	CHECK(a->len == 4, "remove_index len");
	CHECK(GPOINTER_TO_INT(g_ptr_array_index(a, 0)) == 0 &&
	      GPOINTER_TO_INT(g_ptr_array_index(a, 1)) == 2 &&
	      GPOINTER_TO_INT(g_ptr_array_index(a, 2)) == 3 &&
	      GPOINTER_TO_INT(g_ptr_array_index(a, 3)) == 4,
	      "remove_index shifts remaining down, preserving order");
	g_ptr_array_free(a, TRUE);
}

static void test_ptr_array_remove_index_last(void) {
	GPtrArray *a = g_ptr_array_new();
	for (int i = 0; i < 3; i++) { g_ptr_array_add(a, GINT_TO_POINTER(i)); } /* 0 1 2 */
	gpointer removed = g_ptr_array_remove_index(a, 2);                      /* 0 1   */
	CHECK(GPOINTER_TO_INT(removed) == 2, "remove last returns last element");
	CHECK(a->len == 2 && GPOINTER_TO_INT(g_ptr_array_index(a, 1)) == 1,
	      "remove last leaves prefix intact");
	g_ptr_array_free(a, TRUE);
}

/* glib invokes the element_free_func on the element removed by remove_index,
 * yet still returns its (now-freed) pointer value. We only compare the pointer
 * identity here -- never dereference it -- to stay defined under both backends. */
static void test_ptr_array_remove_index_calls_free_func(void) {
	GPtrArray *a = g_ptr_array_new_with_free_func(count_and_free);
	void *p0 = TEST_MALLOC(8);
	void *p1 = TEST_MALLOC(8);
	void *p2 = TEST_MALLOC(8);
	g_ptr_array_add(a, p0);
	g_ptr_array_add(a, p1);
	g_ptr_array_add(a, p2);
	qg_destroy_count = 0;
	gpointer removed = g_ptr_array_remove_index(a, 1);
	CHECK(qg_destroy_count == 1, "remove_index invokes element_free_func once");
	CHECK(removed == p1, "remove_index still returns the removed pointer value");
	CHECK(a->len == 2 && g_ptr_array_index(a, 0) == p0 && g_ptr_array_index(a, 1) == p2,
	      "remove_index with free_func preserves remaining order");
	g_ptr_array_free(a, TRUE); /* frees p0 and p2 */
	CHECK(qg_destroy_count == 3, "free(TRUE) runs free_func on remaining elements");
}

static void test_ptr_array_find(void) {
	GPtrArray *a = g_ptr_array_new();
	for (int i = 0; i < 4; i++) { g_ptr_array_add(a, GINT_TO_POINTER(i * 5)); }
	guint idx = 999;
	gboolean found = g_ptr_array_find(a, GINT_TO_POINTER(10), &idx);
	CHECK(found && idx == 2, "find locates element by pointer equality and sets index");
	CHECK(g_ptr_array_find(a, GINT_TO_POINTER(10), NULL),
	      "find with NULL index_ still reports presence");
	CHECK(!g_ptr_array_find(a, GINT_TO_POINTER(99), &idx),
	      "find miss returns FALSE");
	g_ptr_array_free(a, TRUE);
}

static void test_ptr_array_foreach(void) {
	GPtrArray *a = g_ptr_array_new();
	for (int i = 1; i <= 4; i++) { g_ptr_array_add(a, GINT_TO_POINTER(i)); }
	int sum = 0;
	g_ptr_array_foreach(a, accumulate_int, &sum);
	CHECK(sum == 10, "foreach visits every element");
	g_ptr_array_free(a, TRUE);
}

/* sort_values (2.76+) hands the comparator the ELEMENT pointers directly. */
static void test_ptr_array_sort_values(void) {
	GPtrArray *a = g_ptr_array_new();
	g_ptr_array_add(a, (gpointer) "cherry");
	g_ptr_array_add(a, (gpointer) "apple");
	g_ptr_array_add(a, (gpointer) "banana");
	g_ptr_array_sort_values(a, cmp_str);
	CHECK(strcmp((char *) g_ptr_array_index(a, 0), "apple") == 0 &&
	      strcmp((char *) g_ptr_array_index(a, 1), "banana") == 0 &&
	      strcmp((char *) g_ptr_array_index(a, 2), "cherry") == 0,
	      "sort_values orders elements (comparator gets element pointers)");
	g_ptr_array_free(a, TRUE);
}

static void test_ptr_array_sort_values_with_data(void) {
	GPtrArray *a = g_ptr_array_new();
	g_ptr_array_add(a, (gpointer) "delta");
	g_ptr_array_add(a, (gpointer) "alpha");
	g_ptr_array_add(a, (gpointer) "charlie");
	g_ptr_array_add(a, (gpointer) "bravo");
	int calls = 0;
	g_ptr_array_sort_values_with_data(a, cmp_str_data, &calls);
	CHECK(strcmp((char *) g_ptr_array_index(a, 0), "alpha") == 0 &&
	      strcmp((char *) g_ptr_array_index(a, 3), "delta") == 0,
	      "sort_values_with_data orders elements");
	CHECK(calls > 0, "sort_values_with_data threads user_data to the comparator");
	g_ptr_array_free(a, TRUE);
}

static void test_ptr_array_free_false_returns_pdata(void) {
	GPtrArray *a = g_ptr_array_new();
	g_ptr_array_add(a, GINT_TO_POINTER(42));
	g_ptr_array_add(a, GINT_TO_POINTER(43));
	gpointer *pd = g_ptr_array_free(a, FALSE);
	CHECK(pd != NULL && GPOINTER_TO_INT(pd[0]) == 42 && GPOINTER_TO_INT(pd[1]) == 43,
	      "free(free_seg=FALSE) returns the pdata buffer");
	TEST_FREE(pd);
}

static void test_ptr_array_free_true_runs_free_func(void) {
	GPtrArray *a = g_ptr_array_new_with_free_func(count_and_free);
	for (int i = 0; i < 3; i++) { g_ptr_array_add(a, TEST_MALLOC(8)); }
	qg_destroy_count = 0;
	g_ptr_array_free(a, TRUE);
	CHECK(qg_destroy_count == 3, "free(TRUE) invokes element_free_func per element");
}

static void test_ptr_array_ref_unref(void) {
	GPtrArray *a = g_ptr_array_new();
	g_ptr_array_add(a, GINT_TO_POINTER(1));
	GPtrArray *b = g_ptr_array_ref(a);
	CHECK(b == a, "ref returns same array");
	g_ptr_array_unref(a);                 /* 2 -> 1, still alive */
	CHECK(a->len == 1, "array alive after one of two unrefs");
	g_ptr_array_unref(a);                 /* 1 -> 0, freed */
}

static void test_ptr_array_unref_last_runs_free_func(void) {
	GPtrArray *a = g_ptr_array_new_with_free_func(count_and_free);
	g_ptr_array_add(a, TEST_MALLOC(8));
	g_ptr_array_add(a, TEST_MALLOC(8));
	GPtrArray *b = g_ptr_array_ref(a);
	qg_destroy_count = 0;
	g_ptr_array_unref(a);
	CHECK(qg_destroy_count == 0, "non-final unref does not free elements");
	g_ptr_array_unref(b);
	CHECK(qg_destroy_count == 2, "final unref runs free_func on remaining elements");
}

int main(void) {
	test_ptr_array_new_is_empty();
	test_ptr_array_add_and_index();
	test_ptr_array_index_is_lvalue();
	test_ptr_array_remove_index_preserves_order();
	test_ptr_array_remove_index_last();
	test_ptr_array_remove_index_calls_free_func();
	test_ptr_array_find();
	test_ptr_array_foreach();
	test_ptr_array_sort_values();
	test_ptr_array_sort_values_with_data();
	test_ptr_array_free_false_returns_pdata();
	test_ptr_array_free_true_runs_free_func();
	test_ptr_array_ref_unref();
	test_ptr_array_unref_last_runs_free_func();

	if (qg_failures == 0) {
		printf("ALL PASS (%d checks)\n", qg_checks);
		return 0;
	}
	printf("%d FAILURE(S) of %d checks\n", qg_failures, qg_checks);
	return 1;
}
