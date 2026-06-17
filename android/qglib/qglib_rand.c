/*
 * qglib_rand.c — glib GRand compatibility (Mersenne Twister, MT19937).
 * See qglib_rand.h. The algorithm and every constant here mirror glib's
 * grand.c so the emitted sequence matches real glib bit-for-bit for any seed;
 * the differential test (test_rand.c) pins this against system glib.
 *
 * MT19937 reference: Matsumoto & Nishimura, "Mersenne Twister", ACM TOMACS
 * vol. 8 no. 1 (1998). The single-integer seeding uses Knuth's 2002 init
 * (multiplier 1812433253), matching glib >= 2.x.
 */
#include <time.h>

#include "qglib_rand.h"

/* MT19937 parameters. */
#define QG_N          624
#define QG_M          397
#define QG_MATRIX_A   0x9908b0dfUL  /* constant vector a */
#define QG_UPPER_MASK 0x80000000UL  /* most significant w-r bits */
#define QG_LOWER_MASK 0x7fffffffUL  /* least significant r bits */

struct _GRand {
	guint32 mt[QG_N];
	guint   mti;
};

/*
 * Seed the generator from a single 32-bit value, exactly as glib's
 * g_rand_set_seed does for the default (Knuth 2002) initialisation: glib maps
 * a zero seed to 4357 (an all-zero MT state never escapes zero), then fills the
 * state with the 1812433253-multiplier recurrence.
 */
static void qg_rand_set_seed(GRand *rand_, guint32 seed) {
	if (seed == 0) { /* glib forbids an all-zero initial state */
		seed = 4357;
	}

	rand_->mt[0] = seed;
	for (rand_->mti = 1; rand_->mti < QG_N; rand_->mti++) {
		/* See Knuth TAOCP Vol. 2, 3rd Ed., p.106 for the multiplier. */
		rand_->mt[rand_->mti] = 1812433253UL *
			(rand_->mt[rand_->mti - 1] ^ (rand_->mt[rand_->mti - 1] >> 30)) +
			rand_->mti;
	}
}

GRand *g_rand_new_with_seed(guint32 seed) {
	GRand *rand_ = QG_MALLOC(sizeof(GRand));
	qg_rand_set_seed(rand_, seed);
	return rand_;
}

/*
 * g_rand_new() without an entropy source. glib seeds from /dev/urandom or the
 * clock; qglib has no such requirement here (the engine always seeds
 * explicitly), so we derive a non-zero default seed from the clock. Sequences
 * are only required to match glib when a fixed seed is supplied.
 */
GRand *g_rand_new(void) {
	guint32 seed = (guint32) time(NULL);
	return g_rand_new_with_seed(seed);
}

void g_rand_free(GRand *rand_) {
	QG_FREE(rand_);
}

guint32 g_rand_int(GRand *rand_) {
	guint32 y;
	static const guint32 mag01[2] = { 0x0UL, QG_MATRIX_A };
	/* mag01[x] = x * MATRIX_A  for x = 0, 1 */

	if (rand_->mti >= QG_N) { /* generate N words at one time */
		guint kk;

		for (kk = 0; kk < QG_N - QG_M; kk++) {
			y = (rand_->mt[kk] & QG_UPPER_MASK) | (rand_->mt[kk + 1] & QG_LOWER_MASK);
			rand_->mt[kk] = rand_->mt[kk + QG_M] ^ (y >> 1) ^ mag01[y & 0x1];
		}
		for (; kk < QG_N - 1; kk++) {
			y = (rand_->mt[kk] & QG_UPPER_MASK) | (rand_->mt[kk + 1] & QG_LOWER_MASK);
			rand_->mt[kk] = rand_->mt[kk + (QG_M - QG_N)] ^ (y >> 1) ^ mag01[y & 0x1];
		}
		y = (rand_->mt[QG_N - 1] & QG_UPPER_MASK) | (rand_->mt[0] & QG_LOWER_MASK);
		rand_->mt[QG_N - 1] = rand_->mt[QG_M - 1] ^ (y >> 1) ^ mag01[y & 0x1];

		rand_->mti = 0;
	}

	y = rand_->mt[rand_->mti++];
	y ^= (y >> 11);
	y ^= (y << 7) & 0x9d2c5680UL;
	y ^= (y << 15) & 0xefc60000UL;
	y ^= (y >> 18);

	return y;
}

/*
 * Uniform integer in [begin, end), matching glib exactly. glib computes the
 * unsigned distance, then for distances that do not divide 2^32 evenly it
 * rejection-samples: it discards any draw at or above the largest multiple of
 * `dist` representable, so the kept range is an exact multiple of `dist`. The
 * 64-bit path mirrors glib's handling of distances that overflow guint32.
 */
gint32 g_rand_int_range(GRand *rand_, gint32 begin, gint32 end) {
	const guint32 dist = (guint32) (end - begin);
	guint32 random = 0;

	if (dist == 0) {
		random = 0;
	} else {
		/* maxvalue is the largest value < 2^32 that is congruent to 0 mod
		 * dist when we later take `% dist`; equivalently the count of kept
		 * draws is the greatest multiple of dist that fits in 2^32. We
		 * reject anything at or above that multiple so every residue is
		 * equally likely. Computed in 64 bits so dist > 2^31 is handled too,
		 * which matches glib's behaviour across the whole gint32 range. */
		const guint32 maxvalue = (guint32) (((guint64) 1 << 32) / dist) * dist;

		do {
			random = g_rand_int(rand_);
		} while (maxvalue != 0 && random >= maxvalue);

		random %= dist;
	}

	return begin + (gint32) random;
}

/*
 * Uniform double in [0, 1), matching glib. glib builds a 52-bit mantissa from
 * two 32-bit draws: the low draw contributes the high 26 bits of the result's
 * fraction (scaled by 2^-26) and the high draw the next 26 bits (scaled by
 * 2^-52), with each draw masked to its width so the value stays in [0,1).
 */
gdouble g_rand_double(GRand *rand_) {
	/* This is the exact construction used by glib's g_rand_double. */
	gdouble retval = g_rand_int(rand_) * 2.3283064365386963e-10; /* 2^-32 */
	retval = (retval + g_rand_int(rand_)) * 2.3283064365386963e-10;

	/* Defend against rounding nudging the value to exactly 1.0, as glib does. */
	if (retval >= 1.0) {
		return g_rand_double(rand_);
	}

	return retval;
}

gdouble g_rand_double_range(GRand *rand_, gdouble begin, gdouble end) {
	gdouble r = g_rand_double(rand_);
	return r * (end - begin) + begin;
}
