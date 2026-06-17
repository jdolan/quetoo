/*
 * Standalone unit tests for qglib's GHashTable. Dual-backend: builds against
 * qglib (with qglib_list.c for the get_keys/get_values GList) or system glib.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef QG_USE_REAL_GLIB
#include <glib.h>
#else
#include "qglib_hash.h"
#endif

static int qg_failures = 0;
static int qg_checks = 0;
static int qg_destroys = 0;

#define CHECK(cond, msg) do { \
	qg_checks++; \
	if (!(cond)) { printf("FAIL: %s (%s:%d)\n", (msg), __FILE__, __LINE__); qg_failures++; } \
} while (0)

static char *tdup(const char *s) {
#ifdef QG_USE_REAL_GLIB
	return g_strdup(s);
#else
	size_t n = strlen(s) + 1;
	char *p = (char *) malloc(n);
	memcpy(p, s, n);
	return p;
#endif
}

static void counting_free(gpointer p) {
	qg_destroys++;
#ifdef QG_USE_REAL_GLIB
	g_free(p);
#else
	free(p);
#endif
}

static void test_hash_direct_insert_lookup(void) {
	GHashTable *h = g_hash_table_new(g_direct_hash, g_direct_equal);
	gboolean a = g_hash_table_insert(h, GINT_TO_POINTER(1), GINT_TO_POINTER(100));
	CHECK(a == TRUE, "insert new key returns TRUE");
	g_hash_table_insert(h, GINT_TO_POINTER(2), GINT_TO_POINTER(200));
	CHECK(g_hash_table_size(h) == 2, "size after two inserts");
	CHECK(GPOINTER_TO_INT(g_hash_table_lookup(h, GINT_TO_POINTER(1))) == 100, "lookup key 1");
	CHECK(GPOINTER_TO_INT(g_hash_table_lookup(h, GINT_TO_POINTER(2))) == 200, "lookup key 2");
	CHECK(g_hash_table_lookup(h, GINT_TO_POINTER(99)) == NULL, "lookup miss is NULL");
	gboolean b = g_hash_table_insert(h, GINT_TO_POINTER(1), GINT_TO_POINTER(111));
	CHECK(b == FALSE, "insert existing key returns FALSE");
	CHECK(GPOINTER_TO_INT(g_hash_table_lookup(h, GINT_TO_POINTER(1))) == 111, "insert existing updates value");
	CHECK(g_hash_table_size(h) == 2, "size unchanged after updating existing");
	g_hash_table_destroy(h);
}

static void test_hash_contains_remove(void) {
	GHashTable *h = g_hash_table_new(g_direct_hash, g_direct_equal);
	g_hash_table_insert(h, GINT_TO_POINTER(5), GINT_TO_POINTER(50));
	CHECK(g_hash_table_contains(h, GINT_TO_POINTER(5)) == TRUE, "contains present key");
	CHECK(g_hash_table_contains(h, GINT_TO_POINTER(6)) == FALSE, "contains absent key");
	CHECK(g_hash_table_remove(h, GINT_TO_POINTER(5)) == TRUE, "remove present returns TRUE");
	CHECK(g_hash_table_remove(h, GINT_TO_POINTER(5)) == FALSE, "remove absent returns FALSE");
	CHECK(g_hash_table_size(h) == 0, "size 0 after remove");
	g_hash_table_destroy(h);
}

static void test_hash_add(void) {
	GHashTable *h = g_hash_table_new(g_direct_hash, g_direct_equal);
	g_hash_table_add(h, GINT_TO_POINTER(7));
	CHECK(g_hash_table_contains(h, GINT_TO_POINTER(7)) == TRUE, "add then contains");
	CHECK(g_hash_table_size(h) == 1, "add increments size");
	g_hash_table_destroy(h);
}

static void test_hash_string_keys(void) {
	GHashTable *h = g_hash_table_new(g_str_hash, g_str_equal);
	g_hash_table_insert(h, (gpointer) "alpha", GINT_TO_POINTER(1));
	g_hash_table_insert(h, (gpointer) "beta", GINT_TO_POINTER(2));
	CHECK(GPOINTER_TO_INT(g_hash_table_lookup(h, "alpha")) == 1, "string lookup alpha");
	CHECK(GPOINTER_TO_INT(g_hash_table_lookup(h, "beta")) == 2, "string lookup beta");
	char key[8];
	strcpy(key, "alpha");
	CHECK(GPOINTER_TO_INT(g_hash_table_lookup(h, key)) == 1, "lookup by a distinct value-equal key");
	CHECK(g_str_equal("x", "x") == TRUE && g_str_equal("x", "y") == FALSE, "g_str_equal");
	g_hash_table_destroy(h);
}

static void test_hash_destroy_notify_on_remove(void) {
	GHashTable *h = g_hash_table_new_full(g_str_hash, g_str_equal, counting_free, counting_free);
	g_hash_table_insert(h, tdup("k1"), tdup("v1"));
	g_hash_table_insert(h, tdup("k2"), tdup("v2"));
	qg_destroys = 0;
	g_hash_table_remove(h, "k1");
	CHECK(qg_destroys == 2, "remove frees both key and value");
	qg_destroys = 0;
	g_hash_table_destroy(h);
	CHECK(qg_destroys == 2, "destroy frees remaining key+value");
}

static void test_hash_insert_vs_replace_key(void) {
	GHashTable *h = g_hash_table_new_full(g_str_hash, g_str_equal, counting_free, NULL);
	g_hash_table_insert(h, tdup("dup"), GINT_TO_POINTER(1));
	qg_destroys = 0;
	g_hash_table_insert(h, tdup("dup"), GINT_TO_POINTER(2));   /* existing key: frees the NEW key */
	CHECK(qg_destroys == 1, "insert on existing frees the NEW key");
	qg_destroys = 0;
	g_hash_table_replace(h, tdup("dup"), GINT_TO_POINTER(3));  /* replace: frees the OLD key */
	CHECK(qg_destroys == 1, "replace on existing frees the OLD key");
	CHECK(GPOINTER_TO_INT(g_hash_table_lookup(h, "dup")) == 3, "value updated after replace");
	g_hash_table_destroy(h);
}

static void test_hash_remove_all(void) {
	GHashTable *h = g_hash_table_new_full(g_str_hash, g_str_equal, counting_free, NULL);
	g_hash_table_insert(h, tdup("a"), NULL);
	g_hash_table_insert(h, tdup("b"), NULL);
	g_hash_table_insert(h, tdup("c"), NULL);
	qg_destroys = 0;
	g_hash_table_remove_all(h);
	CHECK(qg_destroys == 3, "remove_all frees all keys");
	CHECK(g_hash_table_size(h) == 0, "size 0 after remove_all");
	g_hash_table_destroy(h);
}

static void sum_values(gpointer key, gpointer value, gpointer user) {
	(void) key;
	*(int *) user += GPOINTER_TO_INT(value);
}

static void test_hash_foreach(void) {
	GHashTable *h = g_hash_table_new(g_direct_hash, g_direct_equal);
	for (int i = 1; i <= 5; i++) { g_hash_table_insert(h, GINT_TO_POINTER(i), GINT_TO_POINTER(i * 10)); }
	int sum = 0;
	g_hash_table_foreach(h, sum_values, &sum);
	CHECK(sum == 150, "foreach visits all (sum of values)");
	g_hash_table_destroy(h);
}

static gboolean remove_even_keys(gpointer key, gpointer value, gpointer user) {
	(void) value;
	(void) user;
	return (GPOINTER_TO_INT(key) % 2) == 0;
}

static void test_hash_foreach_remove(void) {
	GHashTable *h = g_hash_table_new(g_direct_hash, g_direct_equal);
	for (int i = 1; i <= 6; i++) { g_hash_table_insert(h, GINT_TO_POINTER(i), GINT_TO_POINTER(i)); }
	guint removed = g_hash_table_foreach_remove(h, remove_even_keys, NULL);
	CHECK(removed == 3, "foreach_remove returns removed count");
	CHECK(g_hash_table_size(h) == 3, "size reflects removals");
	CHECK(g_hash_table_contains(h, GINT_TO_POINTER(2)) == FALSE, "even key removed");
	CHECK(g_hash_table_contains(h, GINT_TO_POINTER(3)) == TRUE, "odd key kept");
	g_hash_table_destroy(h);
}

static void test_hash_get_keys_values(void) {
	GHashTable *h = g_hash_table_new(g_direct_hash, g_direct_equal);
	for (int i = 0; i < 4; i++) { g_hash_table_insert(h, GINT_TO_POINTER(i + 1), GINT_TO_POINTER((i + 1) * 10)); }
	GList *keys = g_hash_table_get_keys(h);
	GList *vals = g_hash_table_get_values(h);
	guint kc = 0, vc = 0;
	int ksum = 0, vsum = 0;
	for (GList *n = keys; n; n = n->next) { kc++; ksum += GPOINTER_TO_INT(n->data); }
	for (GList *n = vals; n; n = n->next) { vc++; vsum += GPOINTER_TO_INT(n->data); }
	CHECK(kc == 4 && vc == 4, "get_keys/get_values length");
	CHECK(ksum == 10, "keys sum (1+2+3+4)");
	CHECK(vsum == 100, "values sum (10+20+30+40)");
	g_list_free(keys);
	g_list_free(vals);
	g_hash_table_destroy(h);
}

static void test_hash_iter(void) {
	GHashTable *h = g_hash_table_new(g_direct_hash, g_direct_equal);
	for (int i = 1; i <= 5; i++) { g_hash_table_insert(h, GINT_TO_POINTER(i), GINT_TO_POINTER(i * 2)); }
	GHashTableIter it;
	gpointer k, v;
	g_hash_table_iter_init(&it, h);
	guint count = 0;
	int ksum = 0, vsum = 0;
	while (g_hash_table_iter_next(&it, &k, &v)) {
		count++;
		ksum += GPOINTER_TO_INT(k);
		vsum += GPOINTER_TO_INT(v);
	}
	CHECK(count == 5, "iter visits every entry once");
	CHECK(ksum == 15 && vsum == 30, "iter key/value sums");
	g_hash_table_destroy(h);
}

static void test_hash_iter_remove(void) {
	GHashTable *h = g_hash_table_new(g_direct_hash, g_direct_equal);
	for (int i = 1; i <= 6; i++) { g_hash_table_insert(h, GINT_TO_POINTER(i), GINT_TO_POINTER(i)); }
	GHashTableIter it;
	gpointer k, v;
	g_hash_table_iter_init(&it, h);
	while (g_hash_table_iter_next(&it, &k, &v)) {
		if (GPOINTER_TO_INT(k) % 2 == 0) { g_hash_table_iter_remove(&it); }
	}
	CHECK(g_hash_table_size(h) == 3, "iter_remove removed evens");
	CHECK(g_hash_table_contains(h, GINT_TO_POINTER(4)) == FALSE, "iter_remove removed key 4");
	CHECK(g_hash_table_contains(h, GINT_TO_POINTER(5)) == TRUE, "iter_remove kept key 5");
	g_hash_table_destroy(h);
}

static void test_hash_resize(void) {
	GHashTable *h = g_hash_table_new(g_direct_hash, g_direct_equal);
	for (int i = 0; i < 1000; i++) { g_hash_table_insert(h, GINT_TO_POINTER(i + 1), GINT_TO_POINTER(i + 1)); }
	CHECK(g_hash_table_size(h) == 1000, "size after 1000 inserts (forces resize)");
	int ok = 1;
	for (int i = 0; i < 1000; i++) {
		if (GPOINTER_TO_INT(g_hash_table_lookup(h, GINT_TO_POINTER(i + 1))) != i + 1) { ok = 0; }
	}
	CHECK(ok, "all 1000 keys retrievable after resize");
	g_hash_table_destroy(h);
}

int main(void) {
	test_hash_direct_insert_lookup();
	test_hash_contains_remove();
	test_hash_add();
	test_hash_string_keys();
	test_hash_destroy_notify_on_remove();
	test_hash_insert_vs_replace_key();
	test_hash_remove_all();
	test_hash_foreach();
	test_hash_foreach_remove();
	test_hash_get_keys_values();
	test_hash_iter();
	test_hash_iter_remove();
	test_hash_resize();

	if (qg_failures == 0) {
		printf("ALL PASS (%d checks)\n", qg_checks);
		return 0;
	}
	printf("%d FAILURE(S) of %d checks\n", qg_failures, qg_checks);
	return 1;
}
