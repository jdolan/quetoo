/*
 * glib.h — compatibility shim for the Quetoo CMake (Android NDK / iOS) build.
 *
 * WHY THIS FILE EXISTS
 * --------------------
 * The engine includes glib unconditionally via src/quetoo.h:
 *
 *     #include <glib.h>            // src/quetoo.h, line ~28
 *
 * On platforms where glib2 is unavailable (notably the Android NDK; see
 * android/PORTING.md and jdolan/quetoo#856) we substitute the in-tree
 * compatibility shim at android/qglib/ (qglib.h). We are NOT permitted to edit
 * src/quetoo.h to change that angle-bracket include, so instead we put THIS
 * directory on the compiler's include search path *ahead of* any real glib.
 * A `#include <glib.h>` then resolves here, and we forward to qglib.h.
 *
 * This indirection keeps the autotools build (which uses real glib via
 * pkg-config) completely untouched: that build never adds this directory to its
 * include path, so it continues to pick up the system <glib.h>.
 *
 * The qglib.h umbrella header pulls in every container/primitive sub-header
 * (qglib_util.h, qglib_ptrarray.h, qglib_slist.h, qglib_queue.h, qglib_string.h,
 * qglib_hash.h). The CMake target adds android/qglib/ to the include path so the
 * relative include below (and qglib.h's own internal includes) resolve.
 */
#ifndef QUETOO_COMPAT_GLIB_H
#define QUETOO_COMPAT_GLIB_H

#include "qglib.h"

#endif /* QUETOO_COMPAT_GLIB_H */
