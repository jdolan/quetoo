/*
 * Copyright(c) 1997-2001 id Software, Inc.
 * Copyright(c) 2002 The Quakeforge Project.
 * Copyright(c) 2006 Quetoo.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#include <check.h>
#include <stdio.h>

#include "quemath.h"

static void assert_flt_eq(float a, float b) {
	ck_assert_msg(fabsf(a - b) <= __FLT_EPSILON__, "%g != %g", a, b);
}

static void assert_vec3_eq(const vec3 a, const vec3 b) {
	ck_assert_msg(vec3_equal(a, b, __FLT_EPSILON__), "(%g %g %g) != (%g %g %g)", a.x, a.y, a.z, b.x, b.y, b.z);
}

START_TEST(_clampf) {
	assert_flt_eq(0, clampf(-1, 0, 1));
	assert_flt_eq(1, clampf( 1, 0, 1));
	assert_flt_eq(0, clampf( 0, 0, 1));
} END_TEST

START_TEST(_vec3f) {
	assert_vec3_eq(vec3f(1, 2, 3), vec3f(1, 2, 3));
} END_TEST

START_TEST(_vec3_add) {
	assert_vec3_eq(vec3f(2, 2, 2), vec3_add(vec3f(1, 1, 1), vec3f(1, 1, 1)));
} END_TEST

START_TEST(_vec3_cross) {
	assert_vec3_eq(vec3f(-3, 6, -3), vec3_cross(vec3f(1, 2, 3), vec3f(4, 5, 6)));
} END_TEST

START_TEST(_vec3_distance) {
	assert_flt_eq(5, vec3_distance(vec3f(0, 0, 0), vec3f(3, 4, 0)));
} END_TEST

START_TEST(_vec3_dot) {
	assert_flt_eq( 1, vec3_dot(vec3f(1, 0, 0), vec3f( 1, 0, 0)));
	assert_flt_eq(-1, vec3_dot(vec3f(1, 0, 0), vec3f(-1, 0, 0)));
	assert_flt_eq( 0, vec3_dot(vec3f(1, 0, 0), vec3f( 0, 1, 0)));
} END_TEST

START_TEST(_vec3_down) {
	assert_vec3_eq(vec3f(0, 0, -1), vec3_down());
} END_TEST

START_TEST(_vec3_equal) {
	ck_assert(vec3_equal(vec3_zero(), vec3_zero(), __FLT_EPSILON__));
	ck_assert(vec3_equal(vec3_one(), vec3_one(), __FLT_EPSILON__));
	ck_assert(!vec3_equal(vec3_zero(), vec3_one(), __FLT_EPSILON__));
} END_TEST

START_TEST(_vec3_euler) {
	{
		const vec3 euler = vec3_degrees(vec3_euler(vec3f(1, 0, 0)));
		assert_flt_eq(0, euler.x);
		assert_flt_eq(0, euler.y);
		assert_flt_eq(0, euler.z);
	}
	{
		const vec3 euler = vec3_degrees(vec3_euler(vec3f(-1, 0, 0)));
		assert_flt_eq(0, euler.x);
		assert_flt_eq(180, euler.y);
		assert_flt_eq(0, euler.z);
	}
	{
		const vec3 euler = vec3_degrees(vec3_euler(vec3f(0, 1, 0)));
		assert_flt_eq(0, euler.x);
		assert_flt_eq(90, euler.y);
		assert_flt_eq(0, euler.z);
	}
	{
		const vec3 euler = vec3_degrees(vec3_euler(vec3f(0, -1, 0)));
		assert_flt_eq(0, euler.x);
		assert_flt_eq(-90, euler.y);
		assert_flt_eq(0, euler.z);
	}
} END_TEST

START_TEST(_vec3_length) {
	assert_flt_eq(M_SQRT2, vec3_length(vec3f(1, 1, 0)));
} END_TEST

START_TEST(_vec3_negate) {
	assert_vec3_eq(vec3f(-1, -2, -3), vec3_negate(vec3f(1, 2, 3)));
} END_TEST

START_TEST(_vec3_normalize) {
	assert_flt_eq(1, vec3_length(vec3_normalize(vec3f(1, 1, 1))));
} END_TEST

START_TEST(_vec3_one) {
	assert_vec3_eq(vec3f(1, 1, 1), vec3_one());
} END_TEST

START_TEST(_vec3_radians) {
	assert_vec3_eq(vec3f(0, M_PI_2, M_PI), vec3_radians(vec3f(0, 90, 180)));
} END_TEST

START_TEST(_vec3_subtract) {
	assert_vec3_eq(vec3_zero(), vec3_subtract(vec3_one(), vec3_one()));
} END_TEST

START_TEST(_vec3_scale) {
	assert_vec3_eq(vec3f(2, 2, 2), vec3_scale(vec3_one(), 2));
} END_TEST

START_TEST(_vec3_up) {
	assert_vec3_eq(vec3f(0, 0, 1), vec3_up());
} END_TEST

int32_t main(int32_t argc, char **argv) {

	Suite *suite = suite_create("quemath");
	TCase *tcase;

	tcase = tcase_create("float");
	tcase_add_test(tcase, _clampf);

	suite_add_tcase(suite, tcase);

	tcase = tcase_create("vec3");
	tcase_add_test(tcase, _vec3f);
	tcase_add_test(tcase, _vec3_add);
	tcase_add_test(tcase, _vec3_cross);
	tcase_add_test(tcase, _vec3_distance);
	tcase_add_test(tcase, _vec3_dot);
	tcase_add_test(tcase, _vec3_down);
	tcase_add_test(tcase, _vec3_equal);
	tcase_add_test(tcase, _vec3_euler);
	tcase_add_test(tcase, _vec3_length);
	tcase_add_test(tcase, _vec3_negate);
	tcase_add_test(tcase, _vec3_normalize);
	tcase_add_test(tcase, _vec3_one);
	tcase_add_test(tcase, _vec3_radians);
	tcase_add_test(tcase, _vec3_subtract);
	tcase_add_test(tcase, _vec3_scale);
	tcase_add_test(tcase, _vec3_up);

	suite_add_tcase(suite, tcase);

	SRunner *runner = srunner_create(suite);

	srunner_run_all(runner, CK_VERBOSE);
	const int32_t failed = srunner_ntests_failed(runner);

	srunner_free(runner);
	return failed;
}

