/*
 * Standalone unit tests for qglib's GQueue (double-ended queue).
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
#include "qglib_queue.h"
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

static void count_and_free(gpointer p) {
	qg_destroy_count++;
	TEST_FREE(p);
}

static void accumulate_int(gpointer data, gpointer user) {
	*(int *) user += GPOINTER_TO_INT(data);
}

/* ---- GQueue ---- */

static void test_queue_new_is_empty(void) {
	GQueue *q = g_queue_new();
	CHECK(q != NULL, "new queue is non-NULL");
	CHECK(q->head == NULL, "new queue head is NULL");
	CHECK(q->tail == NULL, "new queue tail is NULL");
	CHECK(q->length == 0, "new queue length is 0");
	g_queue_clear(q); TEST_FREE(q);
}

static void test_queue_push_tail_order(void) {
	GQueue *q = g_queue_new();
	for (int i = 0; i < 4; i++) { g_queue_push_tail(q, GINT_TO_POINTER(i)); }
	CHECK(q->length == 4, "push_tail length");
	CHECK(GPOINTER_TO_INT(q->head->data) == 0, "push_tail keeps first at head");
	CHECK(GPOINTER_TO_INT(q->tail->data) == 3, "push_tail puts newest at tail");
	CHECK(GPOINTER_TO_INT(g_queue_peek_nth(q, 0)) == 0, "push_tail order index 0");
	CHECK(GPOINTER_TO_INT(g_queue_peek_nth(q, 3)) == 3, "push_tail order index 3");
	g_queue_clear(q); TEST_FREE(q);
}

static void test_queue_push_head_order(void) {
	GQueue *q = g_queue_new();
	for (int i = 0; i < 4; i++) { g_queue_push_head(q, GINT_TO_POINTER(i)); }
	CHECK(q->length == 4, "push_head length");
	CHECK(GPOINTER_TO_INT(q->head->data) == 3, "push_head puts newest at head");
	CHECK(GPOINTER_TO_INT(q->tail->data) == 0, "push_head keeps first at tail");
	g_queue_clear(q); TEST_FREE(q);
}

static void test_queue_head_tail_links(void) {
	GQueue *q = g_queue_new();
	for (int i = 0; i < 3; i++) { g_queue_push_tail(q, GINT_TO_POINTER(i)); }
	CHECK(q->head->prev == NULL, "head->prev is NULL");
	CHECK(q->tail->next == NULL, "tail->next is NULL");
	CHECK(q->head->next != NULL && GPOINTER_TO_INT(q->head->next->data) == 1, "head->next links forward");
	CHECK(q->tail->prev != NULL && GPOINTER_TO_INT(q->tail->prev->data) == 1, "tail->prev links backward");
	g_queue_clear(q); TEST_FREE(q);
}

static void test_queue_pop_head(void) {
	GQueue *q = g_queue_new();
	for (int i = 0; i < 3; i++) { g_queue_push_tail(q, GINT_TO_POINTER(i)); } /* 0 1 2 */
	CHECK(GPOINTER_TO_INT(g_queue_pop_head(q)) == 0, "pop_head returns head data");
	CHECK(q->length == 2, "pop_head decrements length");
	CHECK(GPOINTER_TO_INT(q->head->data) == 1, "pop_head advances head");
	CHECK(q->head->prev == NULL, "pop_head clears new head prev");
	g_queue_clear(q); TEST_FREE(q);
}

static void test_queue_pop_head_to_empty(void) {
	GQueue *q = g_queue_new();
	g_queue_push_tail(q, GINT_TO_POINTER(7));
	CHECK(GPOINTER_TO_INT(g_queue_pop_head(q)) == 7, "pop_head of single element");
	CHECK(q->length == 0, "queue empty after popping last");
	CHECK(q->head == NULL, "head NULL after popping last");
	CHECK(q->tail == NULL, "tail NULL after popping last");
	CHECK(g_queue_pop_head(q) == NULL, "pop_head on empty returns NULL");
	g_queue_clear(q); TEST_FREE(q);
}

static void test_queue_peek_nth_out_of_range(void) {
	GQueue *q = g_queue_new();
	for (int i = 0; i < 3; i++) { g_queue_push_tail(q, GINT_TO_POINTER(i)); }
	CHECK(GPOINTER_TO_INT(g_queue_peek_nth(q, 1)) == 1, "peek_nth middle");
	CHECK(g_queue_peek_nth(q, 3) == NULL, "peek_nth out of range is NULL");
	CHECK(g_queue_peek_nth(q, 99) == NULL, "peek_nth far out of range is NULL");
	g_queue_clear(q); TEST_FREE(q);
}

static void test_queue_remove_middle(void) {
	GQueue *q = g_queue_new();
	for (int i = 0; i < 4; i++) { g_queue_push_tail(q, GINT_TO_POINTER(i)); } /* 0 1 2 3 */
	CHECK(g_queue_remove(q, GINT_TO_POINTER(1)) == TRUE, "remove existing returns TRUE");
	CHECK(q->length == 3, "remove decrements length");
	CHECK(GPOINTER_TO_INT(g_queue_peek_nth(q, 0)) == 0 &&
	      GPOINTER_TO_INT(g_queue_peek_nth(q, 1)) == 2 &&
	      GPOINTER_TO_INT(g_queue_peek_nth(q, 2)) == 3, "remove preserves remaining order");
	CHECK(g_queue_remove(q, GINT_TO_POINTER(99)) == FALSE, "remove missing returns FALSE");
	g_queue_clear(q); TEST_FREE(q);
}

static void test_queue_remove_head(void) {
	GQueue *q = g_queue_new();
	for (int i = 0; i < 3; i++) { g_queue_push_tail(q, GINT_TO_POINTER(i)); } /* 0 1 2 */
	CHECK(g_queue_remove(q, GINT_TO_POINTER(0)) == TRUE, "remove head returns TRUE");
	CHECK(GPOINTER_TO_INT(q->head->data) == 1, "remove head updates head");
	CHECK(q->head->prev == NULL, "new head prev is NULL after removing head");
	CHECK(q->length == 2, "remove head length");
	g_queue_clear(q); TEST_FREE(q);
}

static void test_queue_remove_tail(void) {
	GQueue *q = g_queue_new();
	for (int i = 0; i < 3; i++) { g_queue_push_tail(q, GINT_TO_POINTER(i)); } /* 0 1 2 */
	CHECK(g_queue_remove(q, GINT_TO_POINTER(2)) == TRUE, "remove tail returns TRUE");
	CHECK(GPOINTER_TO_INT(q->tail->data) == 1, "remove tail updates tail");
	CHECK(q->tail->next == NULL, "new tail next is NULL after removing tail");
	CHECK(q->length == 2, "remove tail length");
	g_queue_clear(q); TEST_FREE(q);
}

static void test_queue_remove_only_element(void) {
	GQueue *q = g_queue_new();
	g_queue_push_tail(q, GINT_TO_POINTER(5));
	CHECK(g_queue_remove(q, GINT_TO_POINTER(5)) == TRUE, "remove only element returns TRUE");
	CHECK(q->head == NULL && q->tail == NULL, "remove only element clears head/tail");
	CHECK(q->length == 0, "remove only element zeroes length");
	g_queue_clear(q); TEST_FREE(q);
}

static void test_queue_clear(void) {
	GQueue *q = g_queue_new();
	for (int i = 0; i < 5; i++) { g_queue_push_tail(q, GINT_TO_POINTER(i)); }
	g_queue_clear(q);
	CHECK(q->head == NULL, "clear resets head");
	CHECK(q->tail == NULL, "clear resets tail");
	CHECK(q->length == 0, "clear resets length");
	g_queue_push_tail(q, GINT_TO_POINTER(42)); /* usable after clear */
	CHECK(q->length == 1 && GPOINTER_TO_INT(q->head->data) == 42, "queue usable after clear");
	g_queue_clear(q); TEST_FREE(q);
}

static void test_queue_foreach(void) {
	GQueue *q = g_queue_new();
	for (int i = 1; i <= 4; i++) { g_queue_push_tail(q, GINT_TO_POINTER(i)); }
	int sum = 0;
	g_queue_foreach(q, accumulate_int, &sum);
	CHECK(sum == 10, "foreach visits every element");
	g_queue_clear(q); TEST_FREE(q);
}

static void test_queue_free_full_calls_destroy(void) {
	GQueue *q = g_queue_new();
	for (int i = 0; i < 3; i++) { g_queue_push_tail(q, TEST_MALLOC(8)); }
	qg_destroy_count = 0;
	g_queue_free_full(q, count_and_free);
	CHECK(qg_destroy_count == 3, "free_full invokes destroy per element");
}

static void test_queue_push_head_tail_mixed(void) {
	GQueue *q = g_queue_new();
	g_queue_push_tail(q, GINT_TO_POINTER(1)); /* 1 */
	g_queue_push_head(q, GINT_TO_POINTER(0)); /* 0 1 */
	g_queue_push_tail(q, GINT_TO_POINTER(2)); /* 0 1 2 */
	CHECK(q->length == 3, "mixed push length");
	CHECK(GPOINTER_TO_INT(g_queue_peek_nth(q, 0)) == 0 &&
	      GPOINTER_TO_INT(g_queue_peek_nth(q, 1)) == 1 &&
	      GPOINTER_TO_INT(g_queue_peek_nth(q, 2)) == 2, "mixed push order");
	CHECK(q->tail->next == NULL && q->head->prev == NULL, "mixed push terminal links");
	g_queue_clear(q); TEST_FREE(q);
}

static void test_queue_unlink(void) {
	GQueue *q = g_queue_new();
	for (int i = 0; i < 4; i++) { g_queue_push_tail(q, GINT_TO_POINTER(i)); } /* 0 1 2 3 */
	GList *mid = q->head->next; /* node holding 1 */
	g_queue_unlink(q, mid);
	CHECK(q->length == 3, "unlink decrements length");
	CHECK(mid->next == NULL && mid->prev == NULL, "unlinked node has NULL links");
	CHECK(GPOINTER_TO_INT(mid->data) == 1, "unlink preserves node data");
	CHECK(GPOINTER_TO_INT(g_queue_peek_nth(q, 0)) == 0 &&
	      GPOINTER_TO_INT(g_queue_peek_nth(q, 1)) == 2 &&
	      GPOINTER_TO_INT(g_queue_peek_nth(q, 2)) == 3, "unlink rejoins neighbours");
	TEST_FREE(mid); /* caller owns the unlinked node */
	g_queue_clear(q); TEST_FREE(q);
}

static void test_queue_unlink_head(void) {
	GQueue *q = g_queue_new();
	for (int i = 0; i < 3; i++) { g_queue_push_tail(q, GINT_TO_POINTER(i)); } /* 0 1 2 */
	GList *h = q->head;
	g_queue_unlink(q, h);
	CHECK(GPOINTER_TO_INT(q->head->data) == 1, "unlink head advances head");
	CHECK(q->head->prev == NULL, "new head prev NULL after unlink head");
	CHECK(q->length == 2, "unlink head length");
	TEST_FREE(h);
	g_queue_clear(q); TEST_FREE(q);
}

static void test_queue_unlink_tail(void) {
	GQueue *q = g_queue_new();
	for (int i = 0; i < 3; i++) { g_queue_push_tail(q, GINT_TO_POINTER(i)); } /* 0 1 2 */
	GList *t = q->tail;
	g_queue_unlink(q, t);
	CHECK(GPOINTER_TO_INT(q->tail->data) == 1, "unlink tail retreats tail");
	CHECK(q->tail->next == NULL, "new tail next NULL after unlink tail");
	CHECK(q->length == 2, "unlink tail length");
	TEST_FREE(t);
	g_queue_clear(q); TEST_FREE(q);
}

static void test_queue_unlink_only(void) {
	GQueue *q = g_queue_new();
	g_queue_push_tail(q, GINT_TO_POINTER(9));
	GList *only = q->head;
	g_queue_unlink(q, only);
	CHECK(q->head == NULL && q->tail == NULL, "unlink only element clears head/tail");
	CHECK(q->length == 0, "unlink only element zeroes length");
	CHECK(only->next == NULL && only->prev == NULL, "unlinked only node has NULL links");
	TEST_FREE(only);
	g_queue_clear(q); TEST_FREE(q);
}

int main(void) {
	test_queue_new_is_empty();
	test_queue_push_tail_order();
	test_queue_push_head_order();
	test_queue_head_tail_links();
	test_queue_pop_head();
	test_queue_pop_head_to_empty();
	test_queue_peek_nth_out_of_range();
	test_queue_remove_middle();
	test_queue_remove_head();
	test_queue_remove_tail();
	test_queue_remove_only_element();
	test_queue_clear();
	test_queue_foreach();
	test_queue_free_full_calls_destroy();
	test_queue_push_head_tail_mixed();
	test_queue_unlink();
	test_queue_unlink_head();
	test_queue_unlink_tail();
	test_queue_unlink_only();

	if (qg_failures == 0) {
		printf("ALL PASS (%d checks)\n", qg_checks);
		return 0;
	}
	printf("%d FAILURE(S) of %d checks\n", qg_failures, qg_checks);
	return 1;
}
