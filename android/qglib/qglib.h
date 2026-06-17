/*
 * qglib.h — minimal glib-compatible compatibility layer for Quetoo.
 *
 * Provides drop-in replacements for the subset of glib-2.0 that Quetoo actually
 * uses, so the engine can build on toolchains where glib2 is unavailable
 * (notably the Android NDK; see android/PORTING.md, jdolan/quetoo#856).
 *
 * Design goals:
 *   - Source-compatible signatures and public struct layouts, so call sites and
 *     `array->len` / `array->data` access compile unchanged (header swap, not a
 *     rewrite).
 *   - No external dependencies. Allocation routes through QG_MALLOC/REALLOC/FREE,
 *     which default to the C stdlib but can be redefined (e.g. to SDL_malloc) by
 *     the embedder before including this header.
 *
 * This file is built incrementally, one container at a time, each landed behind
 * its own unit tests. Implemented so far: fundamental types, GArray.
 */
#ifndef QGLIB_H
#define QGLIB_H

#include <stddef.h>
#include <stdint.h>
/* glib.h transitively exposes these; engine code (e.g. shared/vector.h relies on
 * FLT_MAX/time(), shared/parse.c on signal()/SIGSEGV), so the umbrella header
 * must pull them in too. */
#include <float.h>
#include <inttypes.h>
#include <limits.h>
#include <signal.h>
#include <time.h>

/* printf length/format modifiers (used as "%" G_GINT64_FORMAT) */
#define G_GINT64_FORMAT   PRId64
#define G_GUINT64_FORMAT  PRIu64
#define G_GINT32_FORMAT   PRId32
#define G_GUINT32_FORMAT  PRIu32
#define G_GSIZE_FORMAT    "zu"
#define G_GSSIZE_FORMAT   "zd"

/* glib convenience macros (gmacros.h); engine code (e.g. game/) uses these.
 * Guarded so they coexist with any prior definition. */
#ifndef MIN
#define MIN(a, b)  (((a) < (b)) ? (a) : (b))
#endif
#ifndef MAX
#define MAX(a, b)  (((a) > (b)) ? (a) : (b))
#endif
#ifndef CLAMP
#define CLAMP(x, low, high)  (((x) > (high)) ? (high) : (((x) < (low)) ? (low) : (x)))
#endif
#ifndef ABS
#define ABS(a)  (((a) < 0) ? -(a) : (a))
#endif

/* Token stringification (gmacros.h). cg_discord.c uses G_STRINGIFY(major). */
#ifndef G_STRINGIFY
#define G_STRINGIFY(macro_or_string)  G_STRINGIFY_ARG(macro_or_string)
#define G_STRINGIFY_ARG(contents)     #contents
#endif

#ifndef QG_MALLOC
#include <stdlib.h>
#define QG_MALLOC(n)      malloc(n)
#define QG_REALLOC(p, n)  realloc((p), (n))
#define QG_CALLOC(n, s)   calloc((n), (s))
#define QG_FREE(p)        free(p)
#endif

/* ---- fundamental types ---- */
typedef int             gboolean;
typedef char            gchar;
typedef unsigned char   guchar;
typedef int             gint;
typedef unsigned int    guint;
typedef short           gshort;
typedef unsigned short  gushort;
typedef long            glong;
typedef unsigned long   gulong;
typedef float           gfloat;
typedef double          gdouble;
typedef void           *gpointer;
typedef const void     *gconstpointer;
typedef size_t          gsize;
typedef ptrdiff_t       gssize;
typedef int8_t          gint8;
typedef uint8_t         guint8;
typedef int16_t         gint16;
typedef uint16_t        guint16;
typedef int32_t         gint32;
typedef uint32_t        guint32;
typedef int64_t         gint64;
typedef uint64_t        guint64;

#ifndef FALSE
#define FALSE 0
#endif
#ifndef TRUE
#define TRUE (!FALSE)
#endif

/* int<->pointer stuffing (intptr_t casts: correct on LLP64/Windows too) */
#define GINT_TO_POINTER(i)   ((gpointer) (intptr_t) (i))
#define GPOINTER_TO_INT(p)   ((gint) (intptr_t) (p))
#define GUINT_TO_POINTER(u)  ((gpointer) (uintptr_t) (u))
#define GPOINTER_TO_UINT(p)  ((guint) (uintptr_t) (p))
#define GSIZE_TO_POINTER(s)  ((gpointer) (uintptr_t) (s))
#define GPOINTER_TO_SIZE(p)  ((gsize) (uintptr_t) (p))

/* ---- callback signatures ---- */
typedef void     (*GFunc)            (gpointer data, gpointer user_data);
typedef gint     (*GCompareFunc)     (gconstpointer a, gconstpointer b);
typedef gint     (*GCompareDataFunc) (gconstpointer a, gconstpointer b, gpointer user_data);
typedef void     (*GDestroyNotify)   (gpointer data);
typedef guint    (*GHashFunc)        (gconstpointer key);
typedef gboolean (*GEqualFunc)       (gconstpointer a, gconstpointer b);
typedef gboolean (*GHRFunc)          (gpointer key, gpointer value, gpointer user_data);
typedef void     (*GHFunc)           (gpointer key, gpointer value, gpointer user_data);

/* GError lives in core (used by qglib_regex and qglib_io); callers typically
 * only read ->message. domain/code are kept for source/layout compatibility. */
typedef struct {
	guint32  domain;
	gint     code;
	gchar   *message;
} GError;
void g_error_free(GError *error); /* implemented in qglib_util.c */

/* ---- GArray ----
 * Public layout matches glib: callers read `->data` and `->len` directly. The
 * private bookkeeping (element size, capacity, flags, refcount) lives in the
 * implementation's GRealArray, whose head aliases this struct.
 */
typedef struct _GArray GArray;
struct _GArray {
	gchar *data;
	guint  len;
};

GArray *g_array_new          (gboolean zero_terminated, gboolean clear, guint element_size);
GArray *g_array_sized_new    (gboolean zero_terminated, gboolean clear, guint element_size, guint reserved_size);
GArray *g_array_append_vals  (GArray *array, gconstpointer data, guint len);
GArray *g_array_prepend_vals (GArray *array, gconstpointer data, guint len);
GArray *g_array_insert_vals  (GArray *array, guint index_, gconstpointer data, guint len);
GArray *g_array_set_size     (GArray *array, guint length);
GArray *g_array_remove_index      (GArray *array, guint index);
GArray *g_array_remove_index_fast (GArray *array, guint index);
GArray *g_array_remove_range      (GArray *array, guint index, guint length);
void    g_array_sort         (GArray *array, GCompareFunc compare_func);
gchar  *g_array_free         (GArray *array, gboolean free_segment);
GArray *g_array_ref          (GArray *array);
void    g_array_unref        (GArray *array);

#define g_array_append_val(a, v)  g_array_append_vals((a), &(v), 1)
#define g_array_prepend_val(a, v) g_array_prepend_vals((a), &(v), 1)
#define g_array_index(a, t, i)    (((t *) (void *) (a)->data)[(i)])

/* ---- GList (doubly-linked list) ----
 * Functions that may change the head return the (possibly new) head, which
 * callers must reassign: list = g_list_append(list, data);
 */
typedef struct _GList GList;
struct _GList {
	gpointer data;
	GList   *next;
	GList   *prev;
};

GList   *g_list_append         (GList *list, gpointer data);
GList   *g_list_prepend        (GList *list, gpointer data);
GList   *g_list_insert_sorted  (GList *list, gpointer data, GCompareFunc func);
GList   *g_list_remove         (GList *list, gconstpointer data);
GList   *g_list_remove_link    (GList *list, GList *llink);
GList   *g_list_delete_link    (GList *list, GList *link_);
void     g_list_free           (GList *list);
void     g_list_free_full      (GList *list, GDestroyNotify free_func);
guint    g_list_length         (GList *list);
GList   *g_list_last           (GList *list);
gpointer g_list_nth_data       (GList *list, guint n);
GList   *g_list_nth            (GList *list, guint n);
GList   *g_list_find           (GList *list, gconstpointer data);
GList   *g_list_find_custom    (GList *list, gconstpointer data, GCompareFunc func);
void     g_list_foreach        (GList *list, GFunc func, gpointer user_data);
GList   *g_list_reverse        (GList *list);
GList   *g_list_copy           (GList *list);
GList   *g_list_concat         (GList *list1, GList *list2);
GList   *g_list_sort           (GList *list, GCompareFunc compare_func);
GList   *g_list_sort_with_data (GList *list, GCompareDataFunc compare_func, gpointer user_data);

/* ---- remaining containers ----
 * Each header re-includes qglib.h (harmlessly, via the include guard) and only
 * depends on the base types / GList declared above, so the include order here
 * is safe. This makes qglib.h the single umbrella header — the drop-in for
 * #include <glib.h>.
 */
#include "qglib_util.h"
#include "qglib_ptrarray.h"
#include "qglib_slist.h"
#include "qglib_queue.h"
#include "qglib_string.h"
#include "qglib_hash.h"
#include "qglib_checksum.h"
#include "qglib_misc.h"
#include "qglib_rand.h"
#include "qglib_regex.h"
#include "qglib_io.h"

#endif /* QGLIB_H */
