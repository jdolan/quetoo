/*
 * qglib_queue.h — GQueue (double-ended queue) for the qglib compatibility layer.
 *
 * Public layout matches glib: callers read `->head`, `->tail` and `->length`
 * directly, and may walk the GList nodes themselves. Nodes are plain GList nodes
 * (declared in qglib.h); this container allocates and links them itself so it
 * builds without qglib_list.c.
 */
#ifndef QGLIB_QUEUE_H
#define QGLIB_QUEUE_H

#include "qglib.h"

typedef struct {
	GList *head;
	GList *tail;
	guint  length;
} GQueue;

GQueue  *g_queue_new       (void);
void     g_queue_clear     (GQueue *queue);
void     g_queue_free_full (GQueue *queue, GDestroyNotify free_func);
void     g_queue_push_head (GQueue *queue, gpointer data);
void     g_queue_push_tail (GQueue *queue, gpointer data);
gpointer g_queue_pop_head  (GQueue *queue);
gpointer g_queue_peek_nth  (GQueue *queue, guint n);
gboolean g_queue_remove    (GQueue *queue, gconstpointer data);
void     g_queue_foreach   (GQueue *queue, GFunc func, gpointer user_data);
void     g_queue_unlink    (GQueue *queue, GList *link_);

#endif /* QGLIB_QUEUE_H */
