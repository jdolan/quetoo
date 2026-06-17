/*
 * android_compat.c — see android_compat.h.
 */
#include <stdlib.h>

#include "android_compat.h"

#if defined(__ANDROID__) && __ANDROID_API__ < 36

/*
 * Portable GNU-style qsort_r over the standard qsort, via a thread-local
 * comparator/arg trampoline. Objectively's sorts are non-recursive, so the
 * thread-local indirection is safe here.
 */
static _Thread_local int (*qg_qsort_compar)(const void *, const void *, void *);
static _Thread_local void *qg_qsort_arg;

static int qg_qsort_trampoline(const void *a, const void *b) {
	return qg_qsort_compar(a, b, qg_qsort_arg);
}

void qsort_r(void *base, size_t nmemb, size_t size,
             int (*compar)(const void *, const void *, void *), void *arg) {
	qg_qsort_compar = compar;
	qg_qsort_arg = arg;
	qsort(base, nmemb, size, qg_qsort_trampoline);
}

#endif
