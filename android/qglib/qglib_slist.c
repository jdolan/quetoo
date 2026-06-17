/*
 * qglib_slist.c — GSList (singly-linked list) for the qglib compatibility layer.
 *
 * Faithful to glib: head-returning mutators, NULL == empty list, and a stable
 * sort. Sorting is merge sort over the ->next chain; being singly-linked there
 * are no ->prev links to fix up afterwards.
 */
#include "qglib_slist.h"

static GSList *qg_slist_alloc(gpointer data) {
	GSList *n = QG_MALLOC(sizeof(GSList));
	n->data = data;
	n->next = NULL;
	return n;
}

GSList *g_slist_alloc(void) {
	return qg_slist_alloc(NULL);
}

GSList *g_slist_prepend(GSList *list, gpointer data) {
	GSList *n = qg_slist_alloc(data);
	n->next = list;
	return n;
}

guint g_slist_length(GSList *list) {
	guint len = 0;
	for (; list; list = list->next) {
		len++;
	}
	return len;
}

GSList *g_slist_insert_sorted(GSList *list, gpointer data, GCompareFunc func) {
	GSList *n = qg_slist_alloc(data);
	if (list == NULL) {
		return n;
	}
	GSList *cur = list, *prev = NULL;
	while (cur && func(cur->data, data) <= 0) { /* insert after equal elements */
		prev = cur;
		cur = cur->next;
	}
	n->next = cur;
	if (prev) {
		prev->next = n;
		return list;
	}
	return n; /* new head */
}

GSList *g_slist_remove(GSList *list, gconstpointer data) {
	GSList *prev = NULL;
	for (GSList *n = list; n; prev = n, n = n->next) {
		if (n->data == data) {
			if (prev) {
				prev->next = n->next;
			} else {
				list = n->next;
			}
			QG_FREE(n);
			break;
		}
	}
	return list;
}

void g_slist_free(GSList *list) {
	while (list) {
		GSList *next = list->next;
		QG_FREE(list);
		list = next;
	}
}

void g_slist_free_full(GSList *list, GDestroyNotify free_func) {
	while (list) {
		GSList *next = list->next;
		if (free_func) {
			free_func(list->data);
		}
		QG_FREE(list);
		list = next;
	}
}

/* ---- merge sort (stable) over the ->next chain ---- */

static GSList *qg_slist_merge(GSList *a, GSList *b, GCompareDataFunc cmp, gpointer ud) {
	GSList head;
	GSList *tail = &head;
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

static GSList *qg_slist_sort_real(GSList *list, GCompareDataFunc cmp, gpointer ud) {
	if (list == NULL || list->next == NULL) {
		return list;
	}
	GSList *slow = list, *fast = list->next;
	while (fast && fast->next) {
		slow = slow->next;
		fast = fast->next->next;
	}
	GSList *mid = slow->next;
	slow->next = NULL;
	GSList *left = qg_slist_sort_real(list, cmp, ud);
	GSList *right = qg_slist_sort_real(mid, cmp, ud);
	return qg_slist_merge(left, right, cmp, ud);
}

GSList *g_slist_sort_with_data(GSList *list, GCompareDataFunc compare_func, gpointer user_data) {
	return qg_slist_sort_real(list, compare_func, user_data);
}
