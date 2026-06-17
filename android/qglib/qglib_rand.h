/*
 * qglib_rand.h — glib GRand compatibility (Mersenne Twister, MT19937).
 *
 * The Quetoo engine pulls in GRand via src/shared/vector.h (included nearly
 * everywhere): g_rand_new_with_seed, g_rand_int, g_rand_int_range,
 * g_rand_double, g_rand_double_range. This header mirrors glib's public GRand
 * surface for that path.
 *
 * The implementation (qglib_rand.c) reproduces glib's GRand bit-for-bit: the
 * same MT19937 state, the same single-seed initialisation, the same tempering,
 * and the same range/double derivations, so for any given seed qglib emits the
 * exact sequence real glib does. The differential test (test_rand.c) pins this
 * against system glib.
 */
#ifndef QGLIB_RAND_H
#define QGLIB_RAND_H

#include "qglib.h"

typedef struct _GRand GRand;

GRand  *g_rand_new          (void);
GRand  *g_rand_new_with_seed(guint32 seed);
void    g_rand_free         (GRand *rand_);
guint32 g_rand_int          (GRand *rand_);
gint32  g_rand_int_range    (GRand *rand_, gint32 begin, gint32 end);
gdouble g_rand_double       (GRand *rand_);
gdouble g_rand_double_range (GRand *rand_, gdouble begin, gdouble end);

#endif /* QGLIB_RAND_H */
