/*
 * qglib_list.c — GList (doubly-linked list) for the qglib compatibility layer.
 *
 * Faithful to glib: head-returning mutators, NULL == empty list, and a stable
 * sort. Sorting is done on the ->next chain (merge sort) with a final pass to
 * rebuild ->prev links.
 */
#include "qglib.h"

static GList *qg_list_alloc(gpointer data) {
	GList *n = QG_MALLOC(sizeof(GList));
	n->data = data;
	n->next = NULL;
	n->prev = NULL;
	return n;
}

GList *g_list_last(GList *list) {
	if (list) {
		while (list->next) {
			list = list->next;
		}
	}
	return list;
}

GList *g_list_append(GList *list, gpointer data) {
	GList *n = qg_list_alloc(data);
	if (list == NULL) {
		return n;
	}
	GList *last = g_list_last(list);
	last->next = n;
	n->prev = last;
	return list;
}

GList *g_list_prepend(GList *list, gpointer data) {
	GList *n = qg_list_alloc(data);
	n->next = list;
	if (list) {
		list->prev = n;
	}
	return n;
}

guint g_list_length(GList *list) {
	guint len = 0;
	for (; list; list = list->next) {
		len++;
	}
	return len;
}

GList *g_list_nth(GList *list, guint n) {
	while (n-- > 0 && list) {
		list = list->next;
	}
	return list;
}

gpointer g_list_nth_data(GList *list, guint n) {
	guint i = 0;
	for (; list; list = list->next, i++) {
		if (i == n) {
			return list->data;
		}
	}
	return NULL;
}

GList *g_list_find(GList *list, gconstpointer data) {
	for (; list; list = list->next) {
		if (list->data == data) {
			return list;
		}
	}
	return NULL;
}

GList *g_list_find_custom(GList *list, gconstpointer data, GCompareFunc func) {
	for (; list; list = list->next) {
		if (func(list->data, data) == 0) {
			return list;
		}
	}
	return NULL;
}

void g_list_foreach(GList *list, GFunc func, gpointer user_data) {
	while (list) {
		GList *next = list->next; /* glib permits the callback to free the node */
		func(list->data, user_data);
		list = next;
	}
}

GList *g_list_remove_link(GList *list, GList *llink) {
	if (llink == NULL) {
		return list;
	}
	if (llink->prev) {
		llink->prev->next = llink->next;
	}
	if (llink->next) {
		llink->next->prev = llink->prev;
	}
	if (llink == list) {
		list = llink->next;
	}
	llink->next = NULL;
	llink->prev = NULL;
	return list;
}

GList *g_list_delete_link(GList *list, GList *link_) {
	list = g_list_remove_link(list, link_);
	QG_FREE(link_);
	return list;
}

GList *g_list_remove(GList *list, gconstpointer data) {
	for (GList *n = list; n; n = n->next) {
		if (n->data == data) {
			list = g_list_delete_link(list, n);
			break;
		}
	}
	return list;
}

void g_list_free(GList *list) {
	while (list) {
		GList *next = list->next;
		QG_FREE(list);
		list = next;
	}
}

void g_list_free_full(GList *list, GDestroyNotify free_func) {
	while (list) {
		GList *next = list->next;
		if (free_func) {
			free_func(list->data);
		}
		QG_FREE(list);
		list = next;
	}
}

GList *g_list_reverse(GList *list) {
	GList *last = NULL;
	while (list) {
		last = list;
		list = last->next;
		last->next = last->prev;
		last->prev = list;
	}
	return last;
}

GList *g_list_copy(GList *list) {
	GList *head = NULL, *tail = NULL;
	for (; list; list = list->next) {
		GList *n = qg_list_alloc(list->data);
		if (tail) {
			tail->next = n;
			n->prev = tail;
		} else {
			head = n;
		}
		tail = n;
	}
	return head;
}

GList *g_list_concat(GList *list1, GList *list2) {
	if (list2 == NULL) {
		return list1;
	}
	if (list1 == NULL) {
		return list2;
	}
	GList *last = g_list_last(list1);
	last->next = list2;
	list2->prev = last;
	return list1;
}

GList *g_list_insert_sorted(GList *list, gpointer data, GCompareFunc func) {
	GList *n = qg_list_alloc(data);
	if (list == NULL) {
		return n;
	}
	GList *cur = list, *prev = NULL;
	while (cur && func(cur->data, data) <= 0) { /* insert after equal elements */
		prev = cur;
		cur = cur->next;
	}
	n->prev = prev;
	n->next = cur;
	if (cur) {
		cur->prev = n;
	}
	if (prev) {
		prev->next = n;
		return list;
	}
	return n; /* new head */
}

/* ---- merge sort (stable) over the ->next chain ---- */

static GList *qg_list_merge(GList *a, GList *b, GCompareDataFunc cmp, gpointer ud) {
	GList head;
	GList *tail = &head;
	head.next = NULL;
	while (a && b) {
		if (cmp(a->data, b->data, ud) <= 0) { /* <= keeps the sort stable */
			tail->next = a;
			a = a->next;
		} else {
			tail->next = b;
			b = b->next;
		}
		tail = tail->next;
	}
	tail->next = a ? a : b;
	return head.next;
}

static GList *qg_list_sort_real(GList *list, GCompareDataFunc cmp, gpointer ud) {
	if (list == NULL || list->next == NULL) {
		return list;
	}
	GList *slow = list, *fast = list->next;
	while (fast && fast->next) {
		slow = slow->next;
		fast = fast->next->next;
	}
	GList *mid = slow->next;
	slow->next = NULL;
	GList *left = qg_list_sort_real(list, cmp, ud);
	GList *right = qg_list_sort_real(mid, cmp, ud);
	return qg_list_merge(left, right, cmp, ud);
}

static void qg_list_fix_prev(GList *list) {
	GList *prev = NULL;
	for (GList *n = list; n; n = n->next) {
		n->prev = prev;
		prev = n;
	}
}

static gint qg_cmp_adapter(gconstpointer a, gconstpointer b, gpointer user_data) {
	GCompareFunc f = *(GCompareFunc *) user_data;
	return f(a, b);
}

GList *g_list_sort_with_data(GList *list, GCompareDataFunc compare_func, gpointer user_data) {
	list = qg_list_sort_real(list, compare_func, user_data);
	qg_list_fix_prev(list);
	return list;
}

GList *g_list_sort(GList *list, GCompareFunc compare_func) {
	return g_list_sort_with_data(list, qg_cmp_adapter, &compare_func);
}
