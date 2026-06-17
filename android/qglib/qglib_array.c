/*
 * qglib_array.c — GArray implementation for the qglib compatibility layer.
 *
 * Mirrors glib's GArray semantics: the public GArray { data, len } is the head
 * of the private GRealArray below, so callers may read array->data / array->len
 * directly. Growth is geometric; `clear` zero-fills elements exposed by growth;
 * `zero_terminated` keeps a trailing zeroed element past array->len.
 */
#include <string.h>
#include <stdlib.h>

#include "qglib.h"

typedef struct {
	gchar *data;                 /* aliases GArray.data */
	guint  len;                  /* aliases GArray.len  */
	guint  elt_size;
	guint  alloc;                /* capacity in elements, incl. terminator slot */
	guint  zero_terminated : 1;
	guint  clear : 1;
	gint   ref_count;
} GRealArray;

static void qg_array_maybe_expand(GRealArray *a, guint extra) {
	const guint want = a->len + extra + (a->zero_terminated ? 1u : 0u);
	if (want <= a->alloc) {
		return;
	}
	guint na = a->alloc ? a->alloc : 1u;
	while (na < want) {
		na <<= 1;
	}
	a->data = QG_REALLOC(a->data, (size_t) na * a->elt_size);
	a->alloc = na;
}

static void qg_array_zero_terminate(GRealArray *a) {
	if (a->zero_terminated) {
		memset(a->data + (size_t) a->len * a->elt_size, 0, a->elt_size);
	}
}

GArray *g_array_sized_new(gboolean zero_terminated, gboolean clear,
                          guint element_size, guint reserved_size) {
	GRealArray *a = QG_MALLOC(sizeof(GRealArray));
	a->data = NULL;
	a->len = 0;
	a->elt_size = element_size;
	a->alloc = 0;
	a->zero_terminated = zero_terminated ? 1u : 0u;
	a->clear = clear ? 1u : 0u;
	a->ref_count = 1;

	if (reserved_size || a->zero_terminated) {
		qg_array_maybe_expand(a, reserved_size);
		qg_array_zero_terminate(a);
	}
	return (GArray *) a;
}

GArray *g_array_new(gboolean zero_terminated, gboolean clear, guint element_size) {
	return g_array_sized_new(zero_terminated, clear, element_size, 0);
}

GArray *g_array_append_vals(GArray *array, gconstpointer data, guint len) {
	GRealArray *a = (GRealArray *) array;
	if (len == 0) {
		return array;
	}
	qg_array_maybe_expand(a, len);
	memcpy(a->data + (size_t) a->len * a->elt_size, data, (size_t) len * a->elt_size);
	a->len += len;
	qg_array_zero_terminate(a);
	return array;
}

GArray *g_array_prepend_vals(GArray *array, gconstpointer data, guint len) {
	GRealArray *a = (GRealArray *) array;
	if (len == 0) {
		return array;
	}
	qg_array_maybe_expand(a, len);
	memmove(a->data + (size_t) len * a->elt_size, a->data, (size_t) a->len * a->elt_size);
	memcpy(a->data, data, (size_t) len * a->elt_size);
	a->len += len;
	qg_array_zero_terminate(a);
	return array;
}

GArray *g_array_insert_vals(GArray *array, guint index_, gconstpointer data, guint len) {
	GRealArray *a = (GRealArray *) array;
	if (len == 0) {
		return array;
	}
	if (index_ >= a->len) {
		/* glib grows the array (zero-filling the gap when clear) when inserting past the end */
		g_array_set_size(array, index_);
		return g_array_append_vals(array, data, len);
	}
	qg_array_maybe_expand(a, len);
	memmove(a->data + (size_t) (index_ + len) * a->elt_size,
	        a->data + (size_t) index_ * a->elt_size,
	        (size_t) (a->len - index_) * a->elt_size);
	memcpy(a->data + (size_t) index_ * a->elt_size, data, (size_t) len * a->elt_size);
	a->len += len;
	qg_array_zero_terminate(a);
	return array;
}

GArray *g_array_set_size(GArray *array, guint length) {
	GRealArray *a = (GRealArray *) array;
	if (length > a->len) {
		qg_array_maybe_expand(a, length - a->len);
		if (a->clear) {
			memset(a->data + (size_t) a->len * a->elt_size, 0,
			       (size_t) (length - a->len) * a->elt_size);
		}
	}
	a->len = length;
	qg_array_zero_terminate(a);
	return array;
}

GArray *g_array_remove_index(GArray *array, guint index) {
	GRealArray *a = (GRealArray *) array;
	if (index < a->len - 1) {
		memmove(a->data + (size_t) index * a->elt_size,
		        a->data + (size_t) (index + 1) * a->elt_size,
		        (size_t) (a->len - index - 1) * a->elt_size);
	}
	a->len -= 1;
	qg_array_zero_terminate(a);
	return array;
}

GArray *g_array_remove_index_fast(GArray *array, guint index) {
	GRealArray *a = (GRealArray *) array;
	if (index != a->len - 1) {
		memcpy(a->data + (size_t) index * a->elt_size,
		       a->data + (size_t) (a->len - 1) * a->elt_size,
		       a->elt_size);
	}
	a->len -= 1;
	qg_array_zero_terminate(a);
	return array;
}

GArray *g_array_remove_range(GArray *array, guint index, guint length) {
	GRealArray *a = (GRealArray *) array;
	if (index + length < a->len) {
		memmove(a->data + (size_t) index * a->elt_size,
		        a->data + (size_t) (index + length) * a->elt_size,
		        (size_t) (a->len - (index + length)) * a->elt_size);
	}
	a->len -= length;
	qg_array_zero_terminate(a);
	return array;
}

void g_array_sort(GArray *array, GCompareFunc compare_func) {
	GRealArray *a = (GRealArray *) array;
	if (a->len > 1) {
		qsort(a->data, a->len, a->elt_size,
		      (int (*)(const void *, const void *)) compare_func);
	}
}

gchar *g_array_free(GArray *array, gboolean free_segment) {
	GRealArray *a = (GRealArray *) array;
	gchar *segment;
	if (free_segment) {
		QG_FREE(a->data);
		segment = NULL;
	} else {
		segment = a->data;
	}
	QG_FREE(a);
	return segment;
}

GArray *g_array_ref(GArray *array) {
	GRealArray *a = (GRealArray *) array;
	a->ref_count += 1;
	return array;
}

void g_array_unref(GArray *array) {
	GRealArray *a = (GRealArray *) array;
	if (--a->ref_count == 0) {
		QG_FREE(a->data);
		QG_FREE(a);
	}
}
