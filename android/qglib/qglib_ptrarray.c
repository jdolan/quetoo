/*
 * qglib_ptrarray.c — GPtrArray implementation for the qglib compatibility layer.
 *
 * Mirrors glib's GPtrArray semantics: the public GPtrArray { pdata, len } is the
 * head of the private GRealPtrArray below, so callers may read array->pdata /
 * array->len directly. Growth is geometric. An optional element_free_func is run
 * on elements that are removed (remove_index) or discarded when the buffer is
 * freed (free with free_seg, or the final unref).
 */
#include <string.h>
#include <stdlib.h>

#include "qglib_ptrarray.h"

typedef struct {
	gpointer      *pdata;            /* aliases GPtrArray.pdata */
	guint          len;              /* aliases GPtrArray.len   */
	guint          alloc;            /* capacity in elements    */
	gint           ref_count;
	GDestroyNotify element_free_func;
} GRealPtrArray;

static void qg_ptr_array_maybe_expand(GRealPtrArray *a, guint extra) {
	const guint want = a->len + extra;
	if (want <= a->alloc) {
		return;
	}
	guint na = a->alloc ? a->alloc : 1u;
	while (na < want) {
		na <<= 1;
	}
	a->pdata = QG_REALLOC(a->pdata, (size_t) na * sizeof(gpointer));
	a->alloc = na;
}

GPtrArray *g_ptr_array_new(void) {
	GRealPtrArray *a = QG_MALLOC(sizeof(GRealPtrArray));
	a->pdata = NULL;
	a->len = 0;
	a->alloc = 0;
	a->ref_count = 1;
	a->element_free_func = NULL;
	return (GPtrArray *) a;
}

GPtrArray *g_ptr_array_new_with_free_func(GDestroyNotify element_free_func) {
	GRealPtrArray *a = (GRealPtrArray *) g_ptr_array_new();
	a->element_free_func = element_free_func;
	return (GPtrArray *) a;
}

void g_ptr_array_add(GPtrArray *array, gpointer data) {
	GRealPtrArray *a = (GRealPtrArray *) array;
	qg_ptr_array_maybe_expand(a, 1);
	a->pdata[a->len] = data;
	a->len += 1;
}

gpointer g_ptr_array_remove_index(GPtrArray *array, guint index) {
	GRealPtrArray *a = (GRealPtrArray *) array;
	gpointer result = a->pdata[index];
	if (a->element_free_func) {
		a->element_free_func(result);
	}
	if (index != a->len - 1) {
		memmove(a->pdata + index, a->pdata + index + 1,
		        (size_t) (a->len - index - 1) * sizeof(gpointer));
	}
	a->len -= 1;
	return result;
}

gboolean g_ptr_array_find(GPtrArray *haystack, gconstpointer needle, guint *index_) {
	GRealPtrArray *a = (GRealPtrArray *) haystack;
	for (guint i = 0; i < a->len; i++) {
		if (a->pdata[i] == needle) {
			if (index_) {
				*index_ = i;
			}
			return TRUE;
		}
	}
	return FALSE;
}

void g_ptr_array_foreach(GPtrArray *array, GFunc func, gpointer user_data) {
	GRealPtrArray *a = (GRealPtrArray *) array;
	for (guint i = 0; i < a->len; i++) {
		func(a->pdata[i], user_data);
	}
}

/* glib 2.76+ hands the comparator the ELEMENT pointers directly (unlike the
 * older g_ptr_array_sort, which passes pointers-to-pointers). Both entry points
 * share one stable insertion sort over the data channel; sort_values routes its
 * GCompareFunc through user_data, mirroring how g_list_sort delegates. */
static gint qg_ptr_cmp_adapter(gconstpointer a, gconstpointer b, gpointer user_data) {
	GCompareFunc f = *(GCompareFunc *) user_data;
	return f(a, b);
}

void g_ptr_array_sort_values_with_data(GPtrArray *array, GCompareDataFunc compare_func,
                                       gpointer user_data) {
	GRealPtrArray *a = (GRealPtrArray *) array;
	if (a->len <= 1) {
		return;
	}
	/* Stable insertion sort: avoids qsort's lack of a user-data channel without
	 * relying on platform extensions (qsort_r/qsort_s). */
	for (guint i = 1; i < a->len; i++) {
		gpointer v = a->pdata[i];
		guint j = i;
		while (j > 0 && compare_func(a->pdata[j - 1], v, user_data) > 0) {
			a->pdata[j] = a->pdata[j - 1];
			j--;
		}
		a->pdata[j] = v;
	}
}

void g_ptr_array_sort_values(GPtrArray *array, GCompareFunc compare_func) {
	g_ptr_array_sort_values_with_data(array, qg_ptr_cmp_adapter, &compare_func);
}

static void qg_ptr_array_free_elements(GRealPtrArray *a) {
	if (a->element_free_func) {
		for (guint i = 0; i < a->len; i++) {
			a->element_free_func(a->pdata[i]);
		}
	}
}

gpointer *g_ptr_array_free(GPtrArray *array, gboolean free_seg) {
	GRealPtrArray *a = (GRealPtrArray *) array;
	gpointer *segment;
	if (free_seg) {
		qg_ptr_array_free_elements(a);
		QG_FREE(a->pdata);
		segment = NULL;
	} else {
		segment = a->pdata;
	}
	QG_FREE(a);
	return segment;
}

GPtrArray *g_ptr_array_ref(GPtrArray *array) {
	GRealPtrArray *a = (GRealPtrArray *) array;
	a->ref_count += 1;
	return array;
}

void g_ptr_array_unref(GPtrArray *array) {
	GRealPtrArray *a = (GRealPtrArray *) array;
	if (--a->ref_count == 0) {
		qg_ptr_array_free_elements(a);
		QG_FREE(a->pdata);
		QG_FREE(a);
	}
}
