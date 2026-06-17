/*
 * Standalone unit tests for qglib's GRand (Mersenne Twister, MT19937).
 * Dual-backend: the same tests build against qglib or system glib, which must
 * produce identical sequences.
 *   cc ...                      -> qglib (this layer)
 *   cc -DQG_USE_REAL_GLIB ...   -> system glib-2.0 (the oracle)
 *
 * The load-bearing assertions are the exact-value ones: for a fixed seed the
 * first N g_rand_int outputs must equal a hardcoded sequence, which is also
 * what the real-glib build emits. That proves qglib reproduces glib's PRNG
 * bit-for-bit. (To regenerate the expected table, build either backend with
 * -DQG_PRINT_VALUES and paste the printed literals below.)
 */
#include <stdio.h>
#include <stdlib.h>

#ifdef QG_USE_REAL_GLIB
#include <glib.h>
#else
#include "qglib_rand.h"
#endif

static int qg_failures = 0;
static int qg_checks = 0;

#define CHECK(cond, msg) do { \
	qg_checks++; \
	if (!(cond)) { printf("FAIL: %s (%s:%d)\n", (msg), __FILE__, __LINE__); qg_failures++; } \
} while (0)

#define FIXED_SEED 42u
#define N_INTS     10

/*
 * Expected g_rand_int sequence for seed 42, captured from real glib-2.0 and
 * reproduced exactly by qglib. Both backends are asserted against this table,
 * so any divergence in the MT seeding/tempering fails the test on qglib while
 * still passing on the oracle.
 */
static const guint32 expected_seed42[N_INTS] = {
	1608637542u,
	3421126067u,
	4083286876u,
	 787846414u,
	3143890026u,
	3348747335u,
	2571218620u,
	2563451924u,
	 670094950u,
	1914837113u,
};

static void test_rand_int_exact(void) {
	GRand *r = g_rand_new_with_seed(FIXED_SEED);
	CHECK(r != NULL, "g_rand_new_with_seed non-NULL");

	int all_match = 1;
	for (int i = 0; i < N_INTS; i++) {
		guint32 v = g_rand_int(r);
#ifdef QG_PRINT_VALUES
		printf("\t%10luu,\n", (unsigned long) v);
#endif
		if (v != expected_seed42[i]) {
			all_match = 0;
		}
	}
	CHECK(all_match, "g_rand_int seed 42 matches glib reference sequence");
	g_rand_free(r);
}

static void test_rand_same_seed_identical(void) {
	GRand *a = g_rand_new_with_seed(12345u);
	GRand *b = g_rand_new_with_seed(12345u);
	int identical = 1;
	for (int i = 0; i < 64; i++) {
		if (g_rand_int(a) != g_rand_int(b)) {
			identical = 0;
		}
	}
	CHECK(identical, "two generators, same seed -> identical sequences");
	g_rand_free(a);
	g_rand_free(b);
}

static void test_rand_diff_seed_differ(void) {
	GRand *a = g_rand_new_with_seed(1u);
	GRand *b = g_rand_new_with_seed(2u);
	int differ = 0;
	for (int i = 0; i < 64; i++) {
		if (g_rand_int(a) != g_rand_int(b)) {
			differ = 1;
		}
	}
	CHECK(differ, "different seeds -> differing sequences");
	g_rand_free(a);
	g_rand_free(b);
}

static void test_rand_int_range_bounds(void) {
	GRand *r = g_rand_new_with_seed(99u);
	int in_bounds = 1;
	int saw_low = 0, saw_high_minus_1 = 0;
	const gint32 lo = -5, hi = 7; /* range [-5, 7) */
	for (int i = 0; i < 100000; i++) {
		gint32 v = g_rand_int_range(r, lo, hi);
		if (v < lo || v >= hi) {
			in_bounds = 0;
		}
		if (v == lo) { saw_low = 1; }
		if (v == hi - 1) { saw_high_minus_1 = 1; }
	}
	CHECK(in_bounds, "g_rand_int_range stays within [begin, end)");
	CHECK(saw_low && saw_high_minus_1, "g_rand_int_range reaches both endpoints of [begin,end)");
	g_rand_free(r);
}

static void test_rand_int_range_exact(void) {
	/* Exact values pin the range derivation against the oracle too. */
	GRand *r = g_rand_new_with_seed(FIXED_SEED);
	int all_match = 1;
	static const gint32 expected_range[8] = { 2, 7, 6, 4, 6, 5, 0, 4 };
	for (int i = 0; i < 8; i++) {
		gint32 v = g_rand_int_range(r, 0, 10);
#ifdef QG_PRINT_VALUES
		printf("\trange[%d] = %ld\n", i, (long) v);
#endif
		if (v != expected_range[i]) {
			all_match = 0;
		}
	}
	CHECK(all_match, "g_rand_int_range(0,10) seed 42 matches glib reference");
	g_rand_free(r);
}

static void test_rand_double_bounds(void) {
	GRand *r = g_rand_new_with_seed(7u);
	int in_bounds = 1;
	for (int i = 0; i < 100000; i++) {
		gdouble d = g_rand_double(r);
		if (!(d >= 0.0 && d < 1.0)) {
			in_bounds = 0;
		}
	}
	CHECK(in_bounds, "g_rand_double stays within [0, 1)");
	g_rand_free(r);
}

static void test_rand_double_range_bounds(void) {
	GRand *r = g_rand_new_with_seed(7u);
	int in_bounds = 1;
	const gdouble lo = -2.5, hi = 3.5;
	for (int i = 0; i < 100000; i++) {
		gdouble d = g_rand_double_range(r, lo, hi);
		if (!(d >= lo && d < hi)) {
			in_bounds = 0;
		}
	}
	CHECK(in_bounds, "g_rand_double_range stays within [begin, end)");
	g_rand_free(r);
}

int main(void) {
	test_rand_int_exact();
	test_rand_same_seed_identical();
	test_rand_diff_seed_differ();
	test_rand_int_range_bounds();
	test_rand_int_range_exact();
	test_rand_double_bounds();
	test_rand_double_range_bounds();

	if (qg_failures == 0) {
		printf("ALL PASS (%d checks)\n", qg_checks);
		return 0;
	}
	printf("%d FAILURE(S) of %d checks\n", qg_failures, qg_checks);
	return 1;
}
