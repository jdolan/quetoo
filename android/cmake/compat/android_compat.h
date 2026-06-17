/*
 * android_compat.h — small libc gap-fillers for cross-compiling third-party C
 * (notably Objectively) with the Android NDK (Bionic). Force-included via
 * `-include` for the affected translation units; the definitions live in
 * android_compat.c. See android/DEPENDENCIES.md.
 */
#ifndef QUETOO_ANDROID_COMPAT_H
#define QUETOO_ANDROID_COMPAT_H

#include <stddef.h>

#if defined(__ANDROID__) && __ANDROID_API__ < 36
/*
 * Bionic gained qsort_r only at API 36; on older targets it is absent.
 * Objectively (MutableArray.c, Vector.c) uses the GNU signature: the comparator
 * takes (a, b, arg) with arg last. Implemented in android_compat.c.
 */
void qsort_r(void *base, size_t nmemb, size_t size,
             int (*compar)(const void *, const void *, void *), void *arg);
#endif

#endif /* QUETOO_ANDROID_COMPAT_H */
