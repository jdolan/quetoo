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

#include "vector.h"

static void assert_flt_eq(float a, float b) {
	ck_assert_msg(fabsf(a - b) <= __FLT_EPSILON__, "%g != %g", a, b);
}

static void assert_vec3_eq(const vec3_t a, const vec3_t b) {
	ck_assert_msg(Vec3_Equal(a, b), "(%g %g %g) != (%g %g %g)", a.x, a.y, a.z, b.x, b.y, b.z);
}

START_TEST(_Clampf) {
	assert_flt_eq(0, Clampf(-1, 0, 1));
	assert_flt_eq(1, Clampf( 1, 0, 1));
	assert_flt_eq(0, Clampf( 0, 0, 1));
} END_TEST

START_TEST(_Vec3f) {
	assert_vec3_eq(Vec3(1, 2, 3), Vec3(1, 2, 3));
} END_TEST

START_TEST(_Vec3_Add) {
	assert_vec3_eq(Vec3(2, 2, 2), Vec3_Add(Vec3(1, 1, 1), Vec3(1, 1, 1)));
} END_TEST

START_TEST(_Vec3_Cross) {
	assert_vec3_eq(Vec3(-3, 6, -3), Vec3_Cross(Vec3(1, 2, 3), Vec3(4, 5, 6)));
} END_TEST

START_TEST(_Vec3_Distance) {
	assert_flt_eq(5, Vec3_Distance(Vec3(0, 0, 0), Vec3(3, 4, 0)));
} END_TEST

START_TEST(_Vec3_Dot) {
	assert_flt_eq( 1, Vec3_Dot(Vec3(1, 0, 0), Vec3( 1, 0, 0)));
	assert_flt_eq(-1, Vec3_Dot(Vec3(1, 0, 0), Vec3(-1, 0, 0)));
	assert_flt_eq( 0, Vec3_Dot(Vec3(1, 0, 0), Vec3( 0, 1, 0)));
} END_TEST

START_TEST(_Vec3_Down) {
	assert_vec3_eq(Vec3(0, 0, -1), Vec3_Down());
} END_TEST

START_TEST(_Vec3_Equal) {
	ck_assert(Vec3_Equal(Vec3_Zero(), Vec3_Zero()));
	ck_assert(Vec3_Equal(Vec3_One(), Vec3_One()));
	ck_assert(!Vec3_Equal(Vec3_Zero(), Vec3_One()));
} END_TEST

START_TEST(_Vec3_Euler) {
	assert_vec3_eq(Vec3(0, 0, 0), Vec3_Euler(Vec3(1, 0, 0)));
	assert_vec3_eq(Vec3(0, 180, 0), Vec3_Euler(Vec3(-1, 0, 0)));
	assert_vec3_eq(Vec3(0, 90, 0), Vec3_Euler(Vec3(0, 1, 0)));
	assert_vec3_eq(Vec3(0, -90, 0), Vec3_Euler(Vec3(0, -1, 0)));
} END_TEST

START_TEST(_Vec3_Length) {
	assert_flt_eq(M_SQRT2, Vec3_Length(Vec3(1, 1, 0)));
	assert_flt_eq(sqrtf(3), Vec3_Length(Vec3(1, 1, 1)));
} END_TEST

START_TEST(_Vec3_Negate) {
	assert_vec3_eq(Vec3(-1, -2, -3), Vec3_Negate(Vec3(1, 2, 3)));
} END_TEST

START_TEST(_Vec3_Normalize) {
	assert_flt_eq(1, Vec3_Length(vec3_normalize(Vec3(1, 1, 1))));
} END_TEST

START_TEST(_Vec3_One) {
	assert_vec3_eq(Vec3(1, 1, 1), Vec3_One());
} END_TEST

START_TEST(_Vec3_Radians) {
	assert_vec3_eq(Vec3(0, M_PI_2, M_PI), Vec3_Radians(Vec3(0, 90, 180)));
} END_TEST

START_TEST(_Vec3_Subtract) {
	assert_vec3_eq(Vec3_Zero(), Vec3_Subtract(Vec3_One(), Vec3_One()));
} END_TEST

START_TEST(_Vec3_Scale) {
	assert_vec3_eq(Vec3(2, 2, 2), Vec3_Scale(Vec3_One(), 2));
} END_TEST

START_TEST(_Vec3_Up) {
	assert_vec3_eq(Vec3(0, 0, 1), Vec3_Up());
} END_TEST

int32_t main(int32_t argc, char **argv) {

	Suite *suite = suite_create("vector");
	TCase *tcase;

	tcase = tcase_create("float");
	tcase_add_test(tcase, _Clampf);

	suite_add_tcase(suite, tcase);

	tcase = tcase_create("vec3");
	tcase_add_test(tcase, _Vec3f);
	tcase_add_test(tcase, _Vec3_Add);
	tcase_add_test(tcase, _Vec3_Cross);
	tcase_add_test(tcase, _Vec3_Distance);
	tcase_add_test(tcase, _Vec3_Dot);
	tcase_add_test(tcase, _Vec3_Down);
	tcase_add_test(tcase, _Vec3_Equal);
	tcase_add_test(tcase, _Vec3_Euler);
	tcase_add_test(tcase, _Vec3_Length);
	tcase_add_test(tcase, _Vec3_Negate);
	tcase_add_test(tcase, _Vec3_Normalize);
	tcase_add_test(tcase, _Vec3_One);
	tcase_add_test(tcase, _Vec3_Radians);
	tcase_add_test(tcase, _Vec3_Subtract);
	tcase_add_test(tcase, _Vec3_Scale);
	tcase_add_test(tcase, _Vec3_Up);

	suite_add_tcase(suite, tcase);

	SRunner *runner = srunner_create(suite);

	srunner_run_all(runner, CK_VERBOSE);
	const int32_t failed = srunner_ntests_failed(runner);

	srunner_free(runner);
	return failed;
}

