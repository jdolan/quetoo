/*
 * Standalone unit tests for the qglib compatibility layer.
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
#include "qglib.h"
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

/* ---- GArray ---- */

static void test_array_append_and_index(void) {
	GArray *a = g_array_new(FALSE, FALSE, sizeof(int));
	for (int i = 0; i < 5; i++) {
		int v = i * 10;
		g_array_append_val(a, v);
	}
	CHECK(a->len == 5, "array len after 5 appends");
	CHECK(g_array_index(a, int, 0) == 0, "array index 0");
	CHECK(g_array_index(a, int, 4) == 40, "array index 4");
	g_array_free(a, TRUE);
}

static void test_array_index_is_lvalue(void) {
	GArray *a = g_array_new(FALSE, FALSE, sizeof(int));
	int v = 7;
	g_array_append_val(a, v);
	g_array_index(a, int, 0) = 99;
	CHECK(g_array_index(a, int, 0) == 99, "array index usable as lvalue");
	g_array_free(a, TRUE);
}

static void test_array_append_vals(void) {
	GArray *a = g_array_new(FALSE, FALSE, sizeof(int));
	int src[3] = { 1, 2, 3 };
	g_array_append_vals(a, src, 3);
	CHECK(a->len == 3, "append_vals len");
	CHECK(g_array_index(a, int, 1) == 2, "append_vals content");
	g_array_free(a, TRUE);
}

static void test_array_clear_zero_inits(void) {
	GArray *a = g_array_sized_new(FALSE, TRUE, sizeof(int), 4);
	g_array_set_size(a, 4);
	CHECK(a->len == 4, "set_size len");
	int zeroed = 1;
	for (int i = 0; i < 4; i++) {
		if (g_array_index(a, int, i) != 0) { zeroed = 0; }
	}
	CHECK(zeroed, "clear=TRUE zero-initializes new elements");
	g_array_free(a, TRUE);
}

static void test_array_remove_index_preserves_order(void) {
	GArray *a = g_array_new(FALSE, FALSE, sizeof(int));
	for (int i = 0; i < 5; i++) { int v = i; g_array_append_val(a, v); } /* 0 1 2 3 4 */
	g_array_remove_index(a, 1);                                          /* 0 2 3 4   */
	CHECK(a->len == 4, "remove_index len");
	CHECK(g_array_index(a, int, 0) == 0 && g_array_index(a, int, 1) == 2 &&
	      g_array_index(a, int, 2) == 3 && g_array_index(a, int, 3) == 4,
	      "remove_index preserves order");
	g_array_free(a, TRUE);
}

static void test_array_remove_index_fast(void) {
	GArray *a = g_array_new(FALSE, FALSE, sizeof(int));
	for (int i = 0; i < 5; i++) { int v = i; g_array_append_val(a, v); } /* 0 1 2 3 4 */
	g_array_remove_index_fast(a, 1);                                     /* 0 4 2 3   */
	CHECK(a->len == 4, "remove_index_fast len");
	CHECK(g_array_index(a, int, 1) == 4, "remove_index_fast moves last into hole");
	g_array_free(a, TRUE);
}

static void test_array_remove_range(void) {
	GArray *a = g_array_new(FALSE, FALSE, sizeof(int));
	for (int i = 0; i < 6; i++) { int v = i; g_array_append_val(a, v); } /* 0..5 */
	g_array_remove_range(a, 1, 3);                                       /* 0 4 5 */
	CHECK(a->len == 3, "remove_range len");
	CHECK(g_array_index(a, int, 0) == 0 && g_array_index(a, int, 1) == 4 &&
	      g_array_index(a, int, 2) == 5, "remove_range content");
	g_array_free(a, TRUE);
}

static void test_array_insert_vals(void) {
	GArray *a = g_array_new(FALSE, FALSE, sizeof(int));
	int init[3] = { 0, 3, 4 };
	g_array_append_vals(a, init, 3);   /* 0 3 4 */
	int ins[2] = { 1, 2 };
	g_array_insert_vals(a, 1, ins, 2); /* 0 1 2 3 4 */
	CHECK(a->len == 5, "insert_vals len");
	int ok = 1;
	for (int i = 0; i < 5; i++) {
		if (g_array_index(a, int, i) != i) { ok = 0; }
	}
	CHECK(ok, "insert_vals places elements at index");
	g_array_free(a, TRUE);
}

static void test_array_zero_terminated(void) {
	GArray *a = g_array_new(TRUE, FALSE, sizeof(int));
	int v = 5;
	g_array_append_val(a, v);
	CHECK(a->len == 1, "zero-terminated len excludes terminator");
	CHECK(g_array_index(a, int, 1) == 0, "zero terminator present past last element");
	g_array_free(a, TRUE);
}

static int cmp_int(gconstpointer a, gconstpointer b) {
	const int x = *(const int *) a, y = *(const int *) b;
	return (x > y) - (x < y);
}

static void test_array_sort(void) {
	GArray *a = g_array_new(FALSE, FALSE, sizeof(int));
	int vals[5] = { 4, 1, 3, 5, 2 };
	g_array_append_vals(a, vals, 5);
	g_array_sort(a, cmp_int);
	int ok = 1;
	for (int i = 0; i < 5; i++) {
		if (g_array_index(a, int, i) != i + 1) { ok = 0; }
	}
	CHECK(ok, "sort orders ascending");
	g_array_free(a, TRUE);
}

static void test_array_free_false_returns_buffer(void) {
	GArray *a = g_array_new(FALSE, FALSE, sizeof(int));
	int v = 42;
	g_array_append_val(a, v);
	int *buf = (int *) g_array_free(a, FALSE);
	CHECK(buf != NULL && buf[0] == 42, "free(free_segment=FALSE) returns data buffer");
	TEST_FREE(buf);
}

static void test_array_ref_unref(void) {
	GArray *a = g_array_new(FALSE, FALSE, sizeof(int));
	int v = 1;
	g_array_append_val(a, v);
	GArray *b = g_array_ref(a);
	CHECK(b == a, "ref returns same array");
	g_array_unref(a);                 /* 2 -> 1, still alive */
	CHECK(a->len == 1, "array alive after one of two unrefs");
	g_array_unref(a);                 /* 1 -> 0, freed */
}

/* ---- GList ---- */

static gint cmp_ptr_int(gconstpointer a, gconstpointer b) {
	const int x = GPOINTER_TO_INT(a), y = GPOINTER_TO_INT(b);
	return (x > y) - (x < y);
}

static void count_and_free(gpointer p) {
	qg_destroy_count++;
	TEST_FREE(p);
}

static void accumulate_int(gpointer data, gpointer user) {
	*(int *) user += GPOINTER_TO_INT(data);
}

static void test_list_append_length_order(void) {
	GList *l = NULL;
	for (int i = 0; i < 4; i++) { l = g_list_append(l, GINT_TO_POINTER(i)); }
	CHECK(g_list_length(l) == 4, "list length after appends");
	CHECK(GPOINTER_TO_INT(g_list_nth_data(l, 0)) == 0, "append order head");
	CHECK(GPOINTER_TO_INT(g_list_nth_data(l, 3)) == 3, "append order tail");
	CHECK(g_list_nth_data(l, 4) == NULL, "nth_data out of range is NULL");
	CHECK(g_list_nth(l, 2) != NULL && GPOINTER_TO_INT(g_list_nth(l, 2)->data) == 2, "nth returns the nth node");
	CHECK(g_list_nth(l, 9) == NULL, "nth out of range is NULL");
	g_list_free(l);
}

static void test_list_prepend(void) {
	GList *l = NULL;
	l = g_list_prepend(l, GINT_TO_POINTER(1));
	l = g_list_prepend(l, GINT_TO_POINTER(2));
	CHECK(GPOINTER_TO_INT(l->data) == 2, "prepend puts newest at head");
	CHECK(GPOINTER_TO_INT(g_list_nth_data(l, 1)) == 1, "prepend keeps prior element");
	g_list_free(l);
}

static void test_list_last_and_prev_links(void) {
	GList *l = NULL;
	for (int i = 0; i < 3; i++) { l = g_list_append(l, GINT_TO_POINTER(i)); }
	GList *last = g_list_last(l);
	CHECK(GPOINTER_TO_INT(last->data) == 2, "last node data");
	CHECK(last->next == NULL, "last->next is NULL");
	CHECK(last->prev != NULL && GPOINTER_TO_INT(last->prev->data) == 1, "prev links maintained");
	CHECK(l->prev == NULL, "head->prev is NULL");
	g_list_free(l);
}

static void test_list_find_and_custom(void) {
	GList *l = NULL;
	for (int i = 0; i < 4; i++) { l = g_list_append(l, GINT_TO_POINTER(i * 5)); }
	GList *f = g_list_find(l, GINT_TO_POINTER(10));
	CHECK(f != NULL && GPOINTER_TO_INT(f->data) == 10, "find by pointer value");
	GList *fc = g_list_find_custom(l, GINT_TO_POINTER(15), cmp_ptr_int);
	CHECK(fc != NULL && GPOINTER_TO_INT(fc->data) == 15, "find_custom by comparator");
	CHECK(g_list_find(l, GINT_TO_POINTER(99)) == NULL, "find miss is NULL");
	g_list_free(l);
}

static void test_list_remove(void) {
	GList *l = NULL;
	for (int i = 0; i < 4; i++) { l = g_list_append(l, GINT_TO_POINTER(i)); } /* 0 1 2 3 */
	l = g_list_remove(l, GINT_TO_POINTER(0)); /* removing head returns new head */
	CHECK(GPOINTER_TO_INT(l->data) == 1, "remove head updates head");
	l = g_list_remove(l, GINT_TO_POINTER(2));
	CHECK(g_list_length(l) == 2, "remove decrements length");
	CHECK(GPOINTER_TO_INT(g_list_nth_data(l, 0)) == 1 &&
	      GPOINTER_TO_INT(g_list_nth_data(l, 1)) == 3, "remove preserves remaining order");
	g_list_free(l);
}

static void test_list_delete_link(void) {
	GList *l = NULL;
	for (int i = 0; i < 3; i++) { l = g_list_append(l, GINT_TO_POINTER(i)); } /* 0 1 2 */
	GList *mid = g_list_find(l, GINT_TO_POINTER(1));
	l = g_list_delete_link(l, mid);
	CHECK(g_list_length(l) == 2, "delete_link removes the node");
	CHECK(GPOINTER_TO_INT(g_list_nth_data(l, 0)) == 0 &&
	      GPOINTER_TO_INT(g_list_nth_data(l, 1)) == 2, "delete_link preserves order");
	g_list_free(l);
}

static void test_list_reverse(void) {
	GList *l = NULL;
	for (int i = 0; i < 4; i++) { l = g_list_append(l, GINT_TO_POINTER(i)); }
	l = g_list_reverse(l);
	CHECK(GPOINTER_TO_INT(g_list_nth_data(l, 0)) == 3 &&
	      GPOINTER_TO_INT(g_list_nth_data(l, 3)) == 0, "reverse flips order");
	CHECK(l->prev == NULL, "reversed head has no prev");
	g_list_free(l);
}

static void test_list_insert_sorted(void) {
	GList *l = NULL;
	int vals[5] = { 4, 1, 3, 5, 2 };
	for (int i = 0; i < 5; i++) { l = g_list_insert_sorted(l, GINT_TO_POINTER(vals[i]), cmp_ptr_int); }
	int ok = 1;
	for (int i = 0; i < 5; i++) {
		if (GPOINTER_TO_INT(g_list_nth_data(l, i)) != i + 1) { ok = 0; }
	}
	CHECK(ok, "insert_sorted maintains ascending order");
	g_list_free(l);
}

static void test_list_sort(void) {
	GList *l = NULL;
	int vals[6] = { 5, 2, 8, 1, 9, 3 };
	int expected[6] = { 1, 2, 3, 5, 8, 9 };
	for (int i = 0; i < 6; i++) { l = g_list_append(l, GINT_TO_POINTER(vals[i])); }
	l = g_list_sort(l, cmp_ptr_int);
	int fwd = 1;
	for (int i = 0; i < 6; i++) {
		if (GPOINTER_TO_INT(g_list_nth_data(l, i)) != expected[i]) { fwd = 0; }
	}
	CHECK(fwd, "sort orders ascending");
	int back = 1, c = 5;
	for (GList *n = g_list_last(l); n; n = n->prev, c--) {
		if (GPOINTER_TO_INT(n->data) != expected[c]) { back = 0; }
	}
	CHECK(back, "sort keeps prev links consistent");
	g_list_free(l);
}

static void test_list_concat(void) {
	GList *a = NULL, *b = NULL;
	a = g_list_append(a, GINT_TO_POINTER(1));
	a = g_list_append(a, GINT_TO_POINTER(2));
	b = g_list_append(b, GINT_TO_POINTER(3));
	b = g_list_append(b, GINT_TO_POINTER(4));
	a = g_list_concat(a, b);
	CHECK(g_list_length(a) == 4, "concat total length");
	CHECK(GPOINTER_TO_INT(g_list_nth_data(a, 2)) == 3, "concat joins second list");
	GList *last = g_list_last(a);
	CHECK(last->prev != NULL && GPOINTER_TO_INT(last->prev->data) == 3,
	      "concat keeps prev links across the join");
	g_list_free(a);
}

static void test_list_copy_is_independent(void) {
	GList *a = NULL;
	for (int i = 0; i < 3; i++) { a = g_list_append(a, GINT_TO_POINTER(i)); }
	GList *b = g_list_copy(a);
	CHECK(b != a, "copy is a new list");
	CHECK(g_list_length(b) == 3 && GPOINTER_TO_INT(g_list_nth_data(b, 1)) == 1, "copy preserves data");
	b = g_list_remove(b, GINT_TO_POINTER(1));
	CHECK(g_list_length(a) == 3, "mutating copy does not affect original");
	g_list_free(a);
	g_list_free(b);
}

static void test_list_foreach(void) {
	GList *l = NULL;
	for (int i = 1; i <= 4; i++) { l = g_list_append(l, GINT_TO_POINTER(i)); }
	int sum = 0;
	g_list_foreach(l, accumulate_int, &sum);
	CHECK(sum == 10, "foreach visits every element");
	g_list_free(l);
}

static void test_list_free_full_calls_destroy(void) {
	GList *l = NULL;
	for (int i = 0; i < 3; i++) { l = g_list_append(l, TEST_MALLOC(8)); }
	qg_destroy_count = 0;
	g_list_free_full(l, count_and_free);
	CHECK(qg_destroy_count == 3, "free_full invokes destroy per element");
}

int main(void) {
	test_array_append_and_index();
	test_array_index_is_lvalue();
	test_array_append_vals();
	test_array_clear_zero_inits();
	test_array_remove_index_preserves_order();
	test_array_remove_index_fast();
	test_array_remove_range();
	test_array_insert_vals();
	test_array_zero_terminated();
	test_array_sort();
	test_array_free_false_returns_buffer();
	test_array_ref_unref();

	test_list_append_length_order();
	test_list_prepend();
	test_list_last_and_prev_links();
	test_list_find_and_custom();
	test_list_remove();
	test_list_delete_link();
	test_list_reverse();
	test_list_insert_sorted();
	test_list_sort();
	test_list_concat();
	test_list_copy_is_independent();
	test_list_foreach();
	test_list_free_full_calls_destroy();

	if (qg_failures == 0) {
		printf("ALL PASS (%d checks)\n", qg_checks);
		return 0;
	}
	printf("%d FAILURE(S) of %d checks\n", qg_failures, qg_checks);
	return 1;
}
