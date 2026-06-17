/*
 * glib/gstdio.h — compatibility shim for the Quetoo CMake (NDK/iOS) build.
 *
 * Some engine TUs (common/installer.c, common/sys.c) include <glib/gstdio.h>
 * directly for g_fopen/g_remove/g_unlink. Like compat/glib.h, this directory is
 * placed on the include path ahead of any real glib so the angle-bracket
 * include resolves here and forwards to qglib (which declares those in
 * qglib_util.h via the qglib.h umbrella).
 */
#ifndef QUETOO_COMPAT_GLIB_GSTDIO_H
#define QUETOO_COMPAT_GLIB_GSTDIO_H

#include "qglib.h"

#endif /* QUETOO_COMPAT_GLIB_GSTDIO_H */
