/*
 * qglib_hash.h — GHashTable for the qglib compatibility layer.
 *
 * Chained hash table with cached hashes, geometric resize, optional key/value
 * destroy-notify, and a stack-allocated iterator. Includes glib's standard
 * hash/equal helpers. get_keys/get_values build a GList (struct from qglib.h).
 */
#ifndef QGLIB_HASH_H
#define QGLIB_HASH_H

#include "qglib.h"

typedef struct _GHashTable GHashTable;

/* Stack-allocated iterator: opaque to callers, only ever passed to the iter
 * functions. Fields are private; layout need not match glib's since callers
 * never inspect them. */
typedef struct {
	void  *priv_table;
	void  *priv_cur;
	void  *priv_next;
	guint  priv_cur_bucket;
	guint  priv_next_bucket;
} GHashTableIter;

/* standard hash/equality helpers */
guint    g_str_hash     (gconstpointer v);
gboolean g_str_equal    (gconstpointer a, gconstpointer b);
guint    g_direct_hash  (gconstpointer v);
gboolean g_direct_equal (gconstpointer a, gconstpointer b);

GHashTable *g_hash_table_new      (GHashFunc hash_func, GEqualFunc key_equal_func);
GHashTable *g_hash_table_new_full (GHashFunc hash_func, GEqualFunc key_equal_func,
                                   GDestroyNotify key_destroy_func,
                                   GDestroyNotify value_destroy_func);
gboolean g_hash_table_insert       (GHashTable *hash_table, gpointer key, gpointer value);
gboolean g_hash_table_replace      (GHashTable *hash_table, gpointer key, gpointer value);
gboolean g_hash_table_add          (GHashTable *hash_table, gpointer key);
gboolean g_hash_table_contains     (GHashTable *hash_table, gconstpointer key);
gpointer g_hash_table_lookup       (GHashTable *hash_table, gconstpointer key);
gboolean g_hash_table_remove       (GHashTable *hash_table, gconstpointer key);
void     g_hash_table_remove_all   (GHashTable *hash_table);
void     g_hash_table_destroy      (GHashTable *hash_table);
guint    g_hash_table_size         (GHashTable *hash_table);
void     g_hash_table_foreach      (GHashTable *hash_table, GHFunc func, gpointer user_data);
guint    g_hash_table_foreach_remove(GHashTable *hash_table, GHRFunc func, gpointer user_data);
GList   *g_hash_table_get_keys     (GHashTable *hash_table);
GList   *g_hash_table_get_values   (GHashTable *hash_table);
void     g_hash_table_iter_init    (GHashTableIter *iter, GHashTable *hash_table);
gboolean g_hash_table_iter_next    (GHashTableIter *iter, gpointer *key, gpointer *value);
void     g_hash_table_iter_remove  (GHashTableIter *iter);

#endif /* QGLIB_HASH_H */
