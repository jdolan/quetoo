/*
 * Standalone unit tests for qglib's GSList (singly-linked list).
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
#include "qglib_slist.h"
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

/* ---- GSList ---- */

static gint cmp_ptr_int(gconstpointer a, gconstpointer b) {
	const int x = GPOINTER_TO_INT(a), y = GPOINTER_TO_INT(b);
	return (x > y) - (x < y);
}

static gint cmp_ptr_int_data(gconstpointer a, gconstpointer b, gpointer user_data) {
	(void) user_data;
	const int x = GPOINTER_TO_INT(a), y = GPOINTER_TO_INT(b);
	return (x > y) - (x < y);
}

static void count_and_free(gpointer p) {
	qg_destroy_count++;
	TEST_FREE(p);
}

/* A keyed payload used to prove the sort is stable for equal keys. */
typedef struct { int key; int seq; } qg_pair;

static gint cmp_pair_by_key(gconstpointer a, gconstpointer b, gpointer user_data) {
	(void) user_data;
	const qg_pair *x = a, *y = b;
	return (x->key > y->key) - (x->key < y->key);
}

static guint slist_nth_int(GSList *list, guint n) {
	guint i = 0;
	for (; list; list = list->next, i++) {
		if (i == n) {
			return (guint) GPOINTER_TO_INT(list->data);
		}
	}
	return 0;
}

static void test_slist_alloc(void) {
	GSList *n = g_slist_alloc();
	CHECK(n != NULL, "alloc returns a node");
	CHECK(n->data == NULL, "alloc node data is NULL");
	CHECK(n->next == NULL, "alloc node next is NULL");
	g_slist_free(n);
}

static void test_slist_prepend_length_order(void) {
	GSList *l = NULL;
	l = g_slist_prepend(l, GINT_TO_POINTER(1));
	l = g_slist_prepend(l, GINT_TO_POINTER(2));
	l = g_slist_prepend(l, GINT_TO_POINTER(3));
	CHECK(g_slist_length(l) == 3, "length after prepends");
	CHECK(GPOINTER_TO_INT(l->data) == 3, "prepend puts newest at head");
	CHECK(slist_nth_int(l, 1) == 2, "prepend keeps prior element");
	CHECK(slist_nth_int(l, 2) == 1, "prepend keeps oldest at tail");
	g_slist_free(l);
}

static void test_slist_length_empty(void) {
	CHECK(g_slist_length(NULL) == 0, "length of empty list is zero");
}

static void test_slist_insert_sorted(void) {
	GSList *l = NULL;
	int vals[5] = { 4, 1, 3, 5, 2 };
	for (int i = 0; i < 5; i++) {
		l = g_slist_insert_sorted(l, GINT_TO_POINTER(vals[i]), cmp_ptr_int);
	}
	int ok = 1;
	for (int i = 0; i < 5; i++) {
		if (slist_nth_int(l, (guint) i) != (guint) (i + 1)) { ok = 0; }
	}
	CHECK(ok, "insert_sorted maintains ascending order");
	CHECK(g_slist_length(l) == 5, "insert_sorted length");
	g_slist_free(l);
}

static void test_slist_insert_sorted_into_empty(void) {
	GSList *l = NULL;
	l = g_slist_insert_sorted(l, GINT_TO_POINTER(42), cmp_ptr_int);
	CHECK(l != NULL && GPOINTER_TO_INT(l->data) == 42, "insert_sorted into empty list");
	CHECK(g_slist_length(l) == 1, "insert_sorted empty length");
	g_slist_free(l);
}

static void test_slist_insert_sorted_after_equal(void) {
	/* glib inserts a new equal element after existing equals; with pointer-tagged
	 * ints the data values are identical, so we just check placement is stable. */
	GSList *l = NULL;
	l = g_slist_insert_sorted(l, GINT_TO_POINTER(1), cmp_ptr_int);
	l = g_slist_insert_sorted(l, GINT_TO_POINTER(2), cmp_ptr_int);
	l = g_slist_insert_sorted(l, GINT_TO_POINTER(2), cmp_ptr_int);
	l = g_slist_insert_sorted(l, GINT_TO_POINTER(3), cmp_ptr_int);
	CHECK(slist_nth_int(l, 0) == 1 && slist_nth_int(l, 1) == 2 &&
	      slist_nth_int(l, 2) == 2 && slist_nth_int(l, 3) == 3,
	      "insert_sorted places equal elements adjacently in order");
	g_slist_free(l);
}

static void test_slist_remove_head(void) {
	GSList *l = NULL;
	for (int i = 0; i < 4; i++) { l = g_slist_prepend(l, GINT_TO_POINTER(i)); } /* 3 2 1 0 */
	l = g_slist_remove(l, GINT_TO_POINTER(3)); /* removing head returns new head */
	CHECK(GPOINTER_TO_INT(l->data) == 2, "remove head updates head");
	CHECK(g_slist_length(l) == 3, "remove head decrements length");
	g_slist_free(l);
}

static void test_slist_remove_middle_and_order(void) {
	GSList *l = NULL;
	for (int i = 3; i >= 0; i--) { l = g_slist_prepend(l, GINT_TO_POINTER(i)); } /* 0 1 2 3 */
	l = g_slist_remove(l, GINT_TO_POINTER(2));
	CHECK(g_slist_length(l) == 3, "remove decrements length");
	CHECK(slist_nth_int(l, 0) == 0 && slist_nth_int(l, 1) == 1 &&
	      slist_nth_int(l, 2) == 3, "remove preserves remaining order");
	g_slist_free(l);
}

static void test_slist_remove_first_match_only(void) {
	GSList *l = NULL;
	l = g_slist_prepend(l, GINT_TO_POINTER(5));
	l = g_slist_prepend(l, GINT_TO_POINTER(7));
	l = g_slist_prepend(l, GINT_TO_POINTER(5)); /* 5 7 5 */
	l = g_slist_remove(l, GINT_TO_POINTER(5));
	CHECK(g_slist_length(l) == 2, "remove deletes only first match");
	CHECK(slist_nth_int(l, 0) == 7 && slist_nth_int(l, 1) == 5,
	      "remove leaves later matches intact");
	g_slist_free(l);
}

static void test_slist_remove_missing_is_noop(void) {
	GSList *l = NULL;
	for (int i = 0; i < 3; i++) { l = g_slist_prepend(l, GINT_TO_POINTER(i)); }
	l = g_slist_remove(l, GINT_TO_POINTER(99));
	CHECK(g_slist_length(l) == 3, "remove of absent element is a no-op");
	g_slist_free(l);
}

static void test_slist_remove_to_empty(void) {
	GSList *l = NULL;
	l = g_slist_prepend(l, GINT_TO_POINTER(1));
	l = g_slist_remove(l, GINT_TO_POINTER(1));
	CHECK(l == NULL, "removing the only element yields empty list");
	CHECK(g_slist_length(l) == 0, "emptied list has zero length");
	g_slist_free(l);
}

static void test_slist_sort(void) {
	GSList *l = NULL;
	int vals[6] = { 5, 2, 8, 1, 9, 3 };
	int expected[6] = { 1, 2, 3, 5, 8, 9 };
	for (int i = 5; i >= 0; i--) { l = g_slist_prepend(l, GINT_TO_POINTER(vals[i])); }
	l = g_slist_sort_with_data(l, cmp_ptr_int_data, NULL);
	int ok = 1;
	for (int i = 0; i < 6; i++) {
		if (slist_nth_int(l, (guint) i) != (guint) expected[i]) { ok = 0; }
	}
	CHECK(ok, "sort_with_data orders ascending");
	CHECK(g_slist_length(l) == 6, "sort preserves length");
	g_slist_free(l);
}

static void test_slist_sort_empty_and_single(void) {
	CHECK(g_slist_sort_with_data(NULL, cmp_ptr_int_data, NULL) == NULL,
	      "sort of empty list is NULL");
	GSList *l = g_slist_prepend(NULL, GINT_TO_POINTER(7));
	l = g_slist_sort_with_data(l, cmp_ptr_int_data, NULL);
	CHECK(l != NULL && GPOINTER_TO_INT(l->data) == 7, "sort of single element is unchanged");
	g_slist_free(l);
}

static void test_slist_sort_stable(void) {
	/* Stability: sort by key only and confirm the original relative order of
	 * equal keys is preserved (seq stays ascending within each key group). */
	static qg_pair items[6] = {
		{ 1, 0 }, { 2, 1 }, { 1, 2 }, { 2, 3 }, { 1, 4 }, { 2, 5 }
	};
	GSList *l = NULL;
	for (int i = 5; i >= 0; i--) { l = g_slist_prepend(l, &items[i]); }
	l = g_slist_sort_with_data(l, cmp_pair_by_key, NULL);
	/* Expect: key1 seq{0,2,4} then key2 seq{1,3,5}. */
	int seqs[6], i = 0, ok = 1;
	for (GSList *n = l; n; n = n->next, i++) { seqs[i] = ((const qg_pair *) n->data)->seq; }
	int expect[6] = { 0, 2, 4, 1, 3, 5 };
	for (i = 0; i < 6; i++) { if (seqs[i] != expect[i]) { ok = 0; } }
	CHECK(ok, "sort is stable for equal keys");
	g_slist_free(l);
}

static void test_slist_free_full_calls_destroy(void) {
	GSList *l = NULL;
	for (int i = 0; i < 3; i++) { l = g_slist_prepend(l, TEST_MALLOC(8)); }
	qg_destroy_count = 0;
	g_slist_free_full(l, count_and_free);
	CHECK(qg_destroy_count == 3, "free_full invokes destroy per element");
}

int main(void) {
	test_slist_alloc();
	test_slist_prepend_length_order();
	test_slist_length_empty();
	test_slist_insert_sorted();
	test_slist_insert_sorted_into_empty();
	test_slist_insert_sorted_after_equal();
	test_slist_remove_head();
	test_slist_remove_middle_and_order();
	test_slist_remove_first_match_only();
	test_slist_remove_missing_is_noop();
	test_slist_remove_to_empty();
	test_slist_sort();
	test_slist_sort_empty_and_single();
	test_slist_sort_stable();
	test_slist_free_full_calls_destroy();

	if (qg_failures == 0) {
		printf("ALL PASS (%d checks)\n", qg_checks);
		return 0;
	}
	printf("%d FAILURE(S) of %d checks\n", qg_failures, qg_checks);
	return 1;
}
