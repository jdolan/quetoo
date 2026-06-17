/*
 * qglib_slist.h — GSList (singly-linked list) for the qglib compatibility layer.
 *
 * A standalone container header in the same spirit as qglib.h's GList: the
 * public struct layout matches glib (callers read `->data` / `->next`
 * directly) and the head-returning mutators must have their result reassigned:
 *   list = g_slist_prepend(list, data);
 */
#ifndef QGLIB_SLIST_H
#define QGLIB_SLIST_H

#include "qglib.h"

/* ---- GSList (singly-linked list) ----
 * Functions that may change the head return the (possibly new) head, which
 * callers must reassign: list = g_slist_prepend(list, data);
 */
typedef struct _GSList GSList;
struct _GSList {
	gpointer data;
	GSList  *next;
};

GSList *g_slist_alloc          (void);
GSList *g_slist_prepend        (GSList *list, gpointer data);
GSList *g_slist_insert_sorted  (GSList *list, gpointer data, GCompareFunc func);
GSList *g_slist_remove         (GSList *list, gconstpointer data);
guint   g_slist_length         (GSList *list);
void    g_slist_free           (GSList *list);
void    g_slist_free_full      (GSList *list, GDestroyNotify free_func);
GSList *g_slist_sort_with_data (GSList *list, GCompareDataFunc compare_func, gpointer user_data);

#endif /* QGLIB_SLIST_H */
