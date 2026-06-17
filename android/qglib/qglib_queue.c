/*
 * qglib_queue.c — GQueue (double-ended queue) for the qglib compatibility layer.
 *
 * Faithful to glib: head/tail/length stay consistent after every mutation, and
 * an empty queue has head == tail == NULL with length 0. Nodes are GList nodes,
 * but allocation and (un)linking are done here directly (no g_list_* calls) so
 * this container links without qglib_list.c.
 */
#include "qglib_queue.h"

static GList *qg_queue_alloc(gpointer data) {
	GList *n = QG_MALLOC(sizeof(GList));
	n->data = data;
	n->next = NULL;
	n->prev = NULL;
	return n;
}

GQueue *g_queue_new(void) {
	GQueue *queue = QG_MALLOC(sizeof(GQueue));
	queue->head = NULL;
	queue->tail = NULL;
	queue->length = 0;
	return queue;
}

void g_queue_clear(GQueue *queue) {
	GList *n = queue->head;
	while (n) {
		GList *next = n->next;
		QG_FREE(n);
		n = next;
	}
	queue->head = NULL;
	queue->tail = NULL;
	queue->length = 0;
}

void g_queue_free_full(GQueue *queue, GDestroyNotify free_func) {
	GList *n = queue->head;
	while (n) {
		GList *next = n->next;
		if (free_func) {
			free_func(n->data);
		}
		QG_FREE(n);
		n = next;
	}
	QG_FREE(queue);
}

void g_queue_push_head(GQueue *queue, gpointer data) {
	GList *n = qg_queue_alloc(data);
	n->next = queue->head;
	if (queue->head) {
		queue->head->prev = n;
	} else {
		queue->tail = n;
	}
	queue->head = n;
	queue->length++;
}

void g_queue_push_tail(GQueue *queue, gpointer data) {
	GList *n = qg_queue_alloc(data);
	n->prev = queue->tail;
	if (queue->tail) {
		queue->tail->next = n;
	} else {
		queue->head = n;
	}
	queue->tail = n;
	queue->length++;
}

gpointer g_queue_pop_head(GQueue *queue) {
	if (queue->head == NULL) {
		return NULL;
	}
	GList *n = queue->head;
	gpointer data = n->data;
	queue->head = n->next;
	if (queue->head) {
		queue->head->prev = NULL;
	} else {
		queue->tail = NULL;
	}
	QG_FREE(n);
	queue->length--;
	return data;
}

gpointer g_queue_peek_nth(GQueue *queue, guint n) {
	guint i = 0;
	for (GList *l = queue->head; l; l = l->next, i++) {
		if (i == n) {
			return l->data;
		}
	}
	return NULL;
}

/* Unlink an existing node from the queue without freeing it; the node's own
 * next/prev are cleared and head/tail/length are kept consistent. */
void g_queue_unlink(GQueue *queue, GList *link_) {
	if (link_ == NULL) {
		return;
	}
	if (link_->prev) {
		link_->prev->next = link_->next;
	} else {
		queue->head = link_->next;
	}
	if (link_->next) {
		link_->next->prev = link_->prev;
	} else {
		queue->tail = link_->prev;
	}
	link_->next = NULL;
	link_->prev = NULL;
	queue->length--;
}

gboolean g_queue_remove(GQueue *queue, gconstpointer data) {
	for (GList *n = queue->head; n; n = n->next) {
		if (n->data == data) {
			g_queue_unlink(queue, n);
			QG_FREE(n);
			return TRUE;
		}
	}
	return FALSE;
}

void g_queue_foreach(GQueue *queue, GFunc func, gpointer user_data) {
	GList *n = queue->head;
	while (n) {
		GList *next = n->next; /* glib permits the callback to free the node */
		func(n->data, user_data);
		n = next;
	}
}
