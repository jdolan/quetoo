/*
 * qglib_ptrarray.h — GPtrArray for the qglib compatibility layer.
 *
 * Public layout matches glib: callers read `->pdata` and `->len` directly. The
 * private bookkeeping (capacity, refcount, element_free_func) lives in the
 * implementation's GRealPtrArray, whose head aliases this struct.
 *
 * Split from qglib.h so each container lands behind its own unit tests; this
 * header pulls in qglib.h for the fundamental types and callback signatures.
 */
#ifndef QGLIB_PTRARRAY_H
#define QGLIB_PTRARRAY_H

#include "qglib.h"

/* ---- GPtrArray (growable array of pointers) ----
 * Public layout matches glib: callers read `->pdata` and `->len` directly.
 */
typedef struct _GPtrArray GPtrArray;
struct _GPtrArray {
	gpointer *pdata;
	guint     len;
};

GPtrArray *g_ptr_array_new                (void);
GPtrArray *g_ptr_array_new_with_free_func (GDestroyNotify element_free_func);
void       g_ptr_array_add                (GPtrArray *array, gpointer data);
gpointer   g_ptr_array_remove_index       (GPtrArray *array, guint index);
gboolean   g_ptr_array_find               (GPtrArray *haystack, gconstpointer needle, guint *index_);
void       g_ptr_array_foreach            (GPtrArray *array, GFunc func, gpointer user_data);
void       g_ptr_array_sort_values        (GPtrArray *array, GCompareFunc compare_func);
void       g_ptr_array_sort_values_with_data (GPtrArray *array, GCompareDataFunc compare_func, gpointer user_data);
gpointer  *g_ptr_array_free               (GPtrArray *array, gboolean free_seg);
GPtrArray *g_ptr_array_ref                (GPtrArray *array);
void       g_ptr_array_unref              (GPtrArray *array);

#define g_ptr_array_index(a, i) ((a)->pdata[(i)])

#endif /* QGLIB_PTRARRAY_H */
