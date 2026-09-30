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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <check.h>
#include <stdio.h>

#include "shared/vector.h"

static void Test_AssertFloatEq(float a, float b) {
  ck_assert_msg(fabsf(a - b) <= __FLT_EPSILON__, "%g != %g", a, b);
}

static void Test_AssertVec3Eq(const Vec3 a, const Vec3 b) {
  ck_assert_msg(Vec3_Equal(a, b), "(%g %g %g) != (%g %g %g)", a.x, a.y, a.z, b.x, b.y, b.z);
}

static void Test_AssertVec4Eq(const Vec4 a, const Vec4 b) {
  ck_assert_msg(Vec4_EqualEpsilon(a, b, .01), "(%g %g %g %g) != (%g %g %g %g)", a.x, a.y, a.z, a.w, b.x, b.y, b.z, b.w);
}

START_TEST(_SignedZero) {
  float f = -0.f;
  printf("%g %g\n", f, f + 0.f);
} END_TEST

START_TEST(_Clampf) {
  Test_AssertFloatEq(0, Clampf(-1, 0, 1));
  Test_AssertFloatEq(1, Clampf( 1, 0, 1));
  Test_AssertFloatEq(0, Clampf( 0, 0, 1));
} END_TEST

START_TEST(_Smoothf ) {
  ck_assert_float_eq(Smoothf(-0.2f, -0.2f, 0.5f), 0.f);
  ck_assert_float_eq(Smoothf( 0.5f, -0.2f, 0.5f), 1.f);

  ck_assert_float_gt(Smoothf( 0.f, -0.2f, 0.5f),  0.f);
  ck_assert_float_lt(Smoothf( 0.f, -0.2f, 0.5f),  1.f);

} END_TEST

START_TEST(_Vec3f) {
  Test_AssertVec3Eq(MakeVec3(1, 2, 3), MakeVec3(1, 2, 3));
} END_TEST

START_TEST(_Vec3_Add) {
  Test_AssertVec3Eq(MakeVec3(2, 2, 2), Vec3_Add(MakeVec3(1, 1, 1), MakeVec3(1, 1, 1)));
} END_TEST

START_TEST(_Vec3_Cross) {
  Test_AssertVec3Eq(MakeVec3(-3, 6, -3), Vec3_Cross(MakeVec3(1, 2, 3), MakeVec3(4, 5, 6)));
} END_TEST

START_TEST(_Vec3_Distance) {
  Test_AssertFloatEq(5, Vec3_Distance(MakeVec3(0, 0, 0), MakeVec3(3, 4, 0)));
  ck_assert_float_lt(0.f, Vec3_Distance(Vec3_Maxs(), Vec3_Mins()));
} END_TEST

START_TEST(_Vec3_Dot) {
  Test_AssertFloatEq( 1, Vec3_Dot(MakeVec3(1, 0, 0), MakeVec3( 1, 0, 0)));
  Test_AssertFloatEq(-1, Vec3_Dot(MakeVec3(1, 0, 0), MakeVec3(-1, 0, 0)));
  Test_AssertFloatEq( 0, Vec3_Dot(MakeVec3(1, 0, 0), MakeVec3( 0, 1, 0)));
} END_TEST

START_TEST(_Vec3_Down) {
  Test_AssertVec3Eq(MakeVec3(0, 0, -1), Vec3_Down());
} END_TEST

START_TEST(_Vec3_Equal) {
  ck_assert(Vec3_Equal(Vec3_Zero(), Vec3_Zero()));
  ck_assert(Vec3_Equal(Vec3_One(), Vec3_One()));
  ck_assert(!Vec3_Equal(Vec3_Zero(), Vec3_One()));
  ck_assert(Vec3_Equal(Vec3_Mins(), Vec3_Mins()));
  ck_assert(Vec3_Equal(Vec3_Maxs(), Vec3_Maxs()));
  ck_assert(!Vec3_Equal(Vec3_Mins(), Vec3_One()));

} END_TEST

START_TEST(_Vec3_Euler) {
  Test_AssertVec3Eq(MakeVec3(0, 0, 0), Vec3_Euler(MakeVec3(1, 0, 0)));
  Test_AssertVec3Eq(MakeVec3(0, 90, 0), Vec3_Euler(MakeVec3(0, 1, 0)));
  Test_AssertVec3Eq(MakeVec3(0, 180, 0), Vec3_Euler(MakeVec3(-1, 0, 0)));
  Test_AssertVec3Eq(MakeVec3(0, 270, 0), Vec3_Euler(MakeVec3(0, -1, 0)));
} END_TEST

START_TEST(_Vec3_Length) {
  Test_AssertFloatEq(M_SQRT2, Vec3_Length(MakeVec3(1, 1, 0)));
  Test_AssertFloatEq(sqrtf(3), Vec3_Length(MakeVec3(1, 1, 1)));
} END_TEST

START_TEST(_Vec3_Negate) {
  Test_AssertVec3Eq(MakeVec3(-1, -2, -3), Vec3_Negate(MakeVec3(1, 2, 3)));
} END_TEST

START_TEST(_Vec3_Normalize) {
  Test_AssertFloatEq(1, Vec3_Length(Vec3_Normalize(MakeVec3(1, 1, 1))));
} END_TEST

START_TEST(_Vec3_One) {
  Test_AssertVec3Eq(MakeVec3(1, 1, 1), Vec3_One());
} END_TEST

START_TEST(_Vec3_Radians) {
  Test_AssertVec3Eq(MakeVec3(0, M_PI_2, M_PI), Vec3_Radians(MakeVec3(0, 90, 180)));
} END_TEST

START_TEST(_Vec3_Subtract) {
  Test_AssertVec3Eq(Vec3_Zero(), Vec3_Subtract(Vec3_One(), Vec3_One()));
} END_TEST

START_TEST(_Vec3_Scale) {
  Test_AssertVec3Eq(MakeVec3(2, 2, 2), Vec3_Scale(Vec3_One(), 2));
} END_TEST

START_TEST(_Vec3_Up) {
  Test_AssertVec3Eq(MakeVec3(0, 0, 1), Vec3_Up());
} END_TEST

START_TEST(_Vec4_Bytes) {

  const Vec4 a = MakeVec4(1, 0, 0, 1);
  const Vec4 b = Vec4bv(Vec4_Bytes(a));

  Test_AssertVec4Eq(a, b);

  const Vec4 c = MakeVec4(-1, 0, 0, 1);
  const Vec4 d = Vec4bv(Vec4_Bytes(c));

  Test_AssertVec4Eq(c, d);

  const Vec4 e = MakeVec4(0, .7, .7, 1);
  const Vec4 f = Vec4bv(Vec4_Bytes(e));

  Test_AssertVec4Eq(e, f);

  const Vec4 g = MakeVec4(0, -.7, -.7, 1);
  const Vec4 h = Vec4bv(Vec4_Bytes(g));

  Test_AssertVec4Eq(g, h);
} END_TEST

START_TEST(_Vec3_Vec2s) {

  {
    const Vec3 in = MakeVec3(0.f, 0.f, 1.f);
    const Vec2s out = Vec3_Vec2s(in);
    ck_assert_int_eq(0, out.x);
    ck_assert_int_eq(0, out.y);
  }

  {
    const Vec3 in = MakeVec3(1.f, 0.f, 0.f);
    const Vec2s out = Vec3_Vec2s(in);
    ck_assert_int_eq(INT16_MAX, out.x);
    ck_assert_int_eq(0, out.y);
  }

} END_TEST

int32_t main(int32_t argc, char **argv) {

  Suite *suite = suite_create("check_vector");
  TCase *tcase;

  tcase = tcase_create("float");
  tcase_add_test(tcase, _SignedZero);
  tcase_add_test(tcase, _Clampf);
  tcase_add_test(tcase, _Smoothf);

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
  tcase_add_test(tcase, _Vec3_Vec2s);

  suite_add_tcase(suite, tcase);

  tcase = tcase_create("vec4");
  tcase_add_test(tcase, _Vec4_Bytes);

  suite_add_tcase(suite, tcase);

  SRunner *runner = srunner_create(suite);

  srunner_run_all(runner, CK_VERBOSE);
  const int32_t failed = srunner_ntests_failed(runner);

  srunner_free(runner);
  return failed;
}

