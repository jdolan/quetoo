/*
 * qglib_hash.c — GHashTable for the qglib compatibility layer.
 *
 * Separate chaining; bucket count is a power of two (mask indexing); resize
 * doubles at load factor 0.75. Cached per-node hashes avoid rehashing on
 * resize/lookup. Honors key/value GDestroyNotify on remove/replace/destroy,
 * matching glib's insert (keep old key) vs replace (keep new key) ownership.
 */
#include <string.h>

#include "qglib_hash.h"

typedef struct qg_hnode {
	gpointer key;
	gpointer value;
	guint hash;
	struct qg_hnode *next;
} qg_hnode;

struct _GHashTable {
	qg_hnode **buckets;
	guint n_buckets;        /* always a power of two */
	guint n_items;
	GHashFunc hash_func;
	GEqualFunc key_equal;
	GDestroyNotify key_destroy;
	GDestroyNotify value_destroy;
};

#define QG_HASH_INITIAL_BUCKETS 8u

/* ---- standard helpers ---- */

guint g_direct_hash(gconstpointer v) {
	return GPOINTER_TO_UINT(v);
}

gboolean g_direct_equal(gconstpointer a, gconstpointer b) {
	return a == b;
}

guint g_str_hash(gconstpointer v) {
	const signed char *p = (const signed char *) v;
	guint32 h = 5381;
	for (; *p != '\0'; p++) { /* glib's djb2 variant */
		h = (h << 5) + h + (guint32) *p;
	}
	return h;
}

gboolean g_str_equal(gconstpointer a, gconstpointer b) {
	return strcmp((const char *) a, (const char *) b) == 0;
}

/* ---- internals ---- */

static guint qg_hash_of(GHashTable *h, gconstpointer key) {
	return h->hash_func ? h->hash_func(key) : GPOINTER_TO_UINT(key);
}

static gboolean qg_keys_equal(GHashTable *h, gconstpointer a, gconstpointer b) {
	return h->key_equal ? h->key_equal(a, b) : (a == b);
}

static qg_hnode *qg_find(GHashTable *h, gconstpointer key, guint hash) {
	for (qg_hnode *n = h->buckets[hash & (h->n_buckets - 1)]; n; n = n->next) {
		if (n->hash == hash && qg_keys_equal(h, n->key, key)) {
			return n;
		}
	}
	return NULL;
}

static void qg_maybe_resize(GHashTable *h) {
	if (h->n_items < h->n_buckets * 3 / 4) {
		return;
	}
	const guint new_n = h->n_buckets << 1;
	qg_hnode **nb = QG_CALLOC(new_n, sizeof(qg_hnode *));
	for (guint b = 0; b < h->n_buckets; b++) {
		qg_hnode *n = h->buckets[b];
		while (n) {
			qg_hnode *next = n->next;
			const guint i = n->hash & (new_n - 1);
			n->next = nb[i];
			nb[i] = n;
			n = next;
		}
	}
	QG_FREE(h->buckets);
	h->buckets = nb;
	h->n_buckets = new_n;
}

GHashTable *g_hash_table_new_full(GHashFunc hash_func, GEqualFunc key_equal_func,
                                  GDestroyNotify key_destroy_func, GDestroyNotify value_destroy_func) {
	GHashTable *h = QG_MALLOC(sizeof(*h));
	h->n_buckets = QG_HASH_INITIAL_BUCKETS;
	h->n_items = 0;
	h->buckets = QG_CALLOC(h->n_buckets, sizeof(qg_hnode *));
	h->hash_func = hash_func;
	h->key_equal = key_equal_func;
	h->key_destroy = key_destroy_func;
	h->value_destroy = value_destroy_func;
	return h;
}

GHashTable *g_hash_table_new(GHashFunc hash_func, GEqualFunc key_equal_func) {
	return g_hash_table_new_full(hash_func, key_equal_func, NULL, NULL);
}

static gboolean qg_insert(GHashTable *h, gpointer key, gpointer value, gboolean keep_new_key) {
	const guint hash = qg_hash_of(h, key);
	qg_hnode *n = qg_find(h, key, hash);
	if (n) {
		if (keep_new_key) {                       /* replace: drop old key, adopt new */
			if (h->key_destroy && n->key != key) {
				h->key_destroy(n->key);
			}
			n->key = key;
		} else if (h->key_destroy && key != n->key) { /* insert: keep old key, drop new */
			h->key_destroy(key);
		}
		if (h->value_destroy && n->value != value) {
			h->value_destroy(n->value);
		}
		n->value = value;
		return FALSE;
	}
	n = QG_MALLOC(sizeof(qg_hnode));
	n->key = key;
	n->value = value;
	n->hash = hash;
	const guint b = hash & (h->n_buckets - 1);
	n->next = h->buckets[b];
	h->buckets[b] = n;
	h->n_items++;
	qg_maybe_resize(h);
	return TRUE;
}

gboolean g_hash_table_insert(GHashTable *h, gpointer key, gpointer value) {
	return qg_insert(h, key, value, FALSE);
}

gboolean g_hash_table_replace(GHashTable *h, gpointer key, gpointer value) {
	return qg_insert(h, key, value, TRUE);
}

gboolean g_hash_table_add(GHashTable *h, gpointer key) {
	return qg_insert(h, key, key, TRUE); /* glib: equivalent to replace(key, key) */
}

gpointer g_hash_table_lookup(GHashTable *h, gconstpointer key) {
	qg_hnode *n = qg_find(h, key, qg_hash_of(h, key));
	return n ? n->value : NULL;
}

gboolean g_hash_table_contains(GHashTable *h, gconstpointer key) {
	return qg_find(h, key, qg_hash_of(h, key)) != NULL;
}

gboolean g_hash_table_remove(GHashTable *h, gconstpointer key) {
	const guint hash = qg_hash_of(h, key);
	const guint b = hash & (h->n_buckets - 1);
	qg_hnode *prev = NULL, *n = h->buckets[b];
	while (n) {
		if (n->hash == hash && qg_keys_equal(h, n->key, key)) {
			if (prev) {
				prev->next = n->next;
			} else {
				h->buckets[b] = n->next;
			}
			if (h->key_destroy) { h->key_destroy(n->key); }
			if (h->value_destroy) { h->value_destroy(n->value); }
			QG_FREE(n);
			h->n_items--;
			return TRUE;
		}
		prev = n;
		n = n->next;
	}
	return FALSE;
}

void g_hash_table_remove_all(GHashTable *h) {
	for (guint b = 0; b < h->n_buckets; b++) {
		qg_hnode *n = h->buckets[b];
		while (n) {
			qg_hnode *next = n->next;
			if (h->key_destroy) { h->key_destroy(n->key); }
			if (h->value_destroy) { h->value_destroy(n->value); }
			QG_FREE(n);
			n = next;
		}
		h->buckets[b] = NULL;
	}
	h->n_items = 0;
}

void g_hash_table_destroy(GHashTable *h) {
	g_hash_table_remove_all(h);
	QG_FREE(h->buckets);
	QG_FREE(h);
}

guint g_hash_table_size(GHashTable *h) {
	return h->n_items;
}

void g_hash_table_foreach(GHashTable *h, GHFunc func, gpointer user_data) {
	for (guint b = 0; b < h->n_buckets; b++) {
		for (qg_hnode *n = h->buckets[b]; n; n = n->next) {
			func(n->key, n->value, user_data);
		}
	}
}

guint g_hash_table_foreach_remove(GHashTable *h, GHRFunc func, gpointer user_data) {
	guint removed = 0;
	for (guint b = 0; b < h->n_buckets; b++) {
		qg_hnode *prev = NULL, *n = h->buckets[b];
		while (n) {
			qg_hnode *next = n->next;
			if (func(n->key, n->value, user_data)) {
				if (prev) {
					prev->next = next;
				} else {
					h->buckets[b] = next;
				}
				if (h->key_destroy) { h->key_destroy(n->key); }
				if (h->value_destroy) { h->value_destroy(n->value); }
				QG_FREE(n);
				h->n_items--;
				removed++;
			} else {
				prev = n;
			}
			n = next;
		}
	}
	return removed;
}

GList *g_hash_table_get_keys(GHashTable *h) {
	GList *list = NULL;
	for (guint b = 0; b < h->n_buckets; b++) {
		for (qg_hnode *n = h->buckets[b]; n; n = n->next) {
			list = g_list_prepend(list, n->key);
		}
	}
	return list;
}

GList *g_hash_table_get_values(GHashTable *h) {
	GList *list = NULL;
	for (guint b = 0; b < h->n_buckets; b++) {
		for (qg_hnode *n = h->buckets[b]; n; n = n->next) {
			list = g_list_prepend(list, n->value);
		}
	}
	return list;
}

/* ---- iteration ---- (lookahead so iter_remove of the current node is safe) */

static qg_hnode *qg_scan_from(GHashTable *h, guint *bucket) {
	for (guint b = *bucket; b < h->n_buckets; b++) {
		if (h->buckets[b]) {
			*bucket = b;
			return h->buckets[b];
		}
	}
	return NULL;
}

void g_hash_table_iter_init(GHashTableIter *iter, GHashTable *h) {
	guint b = 0;
	qg_hnode *first = qg_scan_from(h, &b);
	iter->priv_table = h;
	iter->priv_next = first;
	iter->priv_next_bucket = first ? b : 0;
	iter->priv_cur = NULL;
	iter->priv_cur_bucket = 0;
}

gboolean g_hash_table_iter_next(GHashTableIter *iter, gpointer *key, gpointer *value) {
	qg_hnode *cur = (qg_hnode *) iter->priv_next;
	if (!cur) {
		return FALSE;
	}
	GHashTable *h = (GHashTable *) iter->priv_table;
	iter->priv_cur = cur;
	iter->priv_cur_bucket = iter->priv_next_bucket;
	if (cur->next) {
		iter->priv_next = cur->next; /* same bucket */
	} else {
		guint b = iter->priv_cur_bucket + 1;
		qg_hnode *nx = qg_scan_from(h, &b);
		iter->priv_next = nx;
		iter->priv_next_bucket = nx ? b : 0;
	}
	if (key) { *key = cur->key; }
	if (value) { *value = cur->value; }
	return TRUE;
}

void g_hash_table_iter_remove(GHashTableIter *iter) {
	GHashTable *h = (GHashTable *) iter->priv_table;
	qg_hnode *cur = (qg_hnode *) iter->priv_cur;
	if (!cur) {
		return;
	}
	const guint b = iter->priv_cur_bucket;
	qg_hnode *prev = NULL, *n = h->buckets[b];
	while (n && n != cur) {
		prev = n;
		n = n->next;
	}
	if (n == cur) {
		if (prev) {
			prev->next = cur->next;
		} else {
			h->buckets[b] = cur->next;
		}
		if (h->key_destroy) { h->key_destroy(cur->key); }
		if (h->value_destroy) { h->value_destroy(cur->value); }
		QG_FREE(cur);
		h->n_items--;
	}
	iter->priv_cur = NULL;
}
