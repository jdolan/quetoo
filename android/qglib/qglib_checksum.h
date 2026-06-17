/*
 * qglib_checksum.h — glib GChecksum compatibility (MD5).
 *
 * The Quetoo engine (src/collision/cm_manifest.c) uses GChecksum only to
 * compute MD5 digests, via g_checksum_new(G_CHECKSUM_MD5), g_checksum_update,
 * g_checksum_get_string, g_checksum_free. This header mirrors glib's public
 * GChecksum surface for that path. See qglib_checksum.c for what is actually
 * implemented (MD5 only).
 */
#ifndef QGLIB_CHECKSUM_H
#define QGLIB_CHECKSUM_H

#include "qglib.h"

/* Enum ordering/values match glib exactly so call sites compile unchanged. */
typedef enum {
	G_CHECKSUM_MD5 = 0,
	G_CHECKSUM_SHA1,
	G_CHECKSUM_SHA256,
	G_CHECKSUM_SHA512,
	G_CHECKSUM_SHA384
} GChecksumType;

typedef struct _GChecksum GChecksum;

GChecksum   *g_checksum_new        (GChecksumType checksum_type);
void         g_checksum_update     (GChecksum *checksum, const guchar *data, gssize length);
const gchar *g_checksum_get_string (GChecksum *checksum);
void         g_checksum_free       (GChecksum *checksum);

#endif /* QGLIB_CHECKSUM_H */
