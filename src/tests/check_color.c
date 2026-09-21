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

#include "shared/color.h"

START_TEST(check_Color_Parse_default) {
  Color color = Color3f(0.25f, 0.5f, 0.75f);

  ck_assert(!Color_Parse("default", &color));
  ck_assert_float_eq(color.r, 0.25f);
  ck_assert_float_eq(color.g, 0.5f);
  ck_assert_float_eq(color.b, 0.75f);
  ck_assert_float_eq(color.a, 1.f);
} END_TEST

START_TEST(check_Color_Parse_rgb) {
  Color color;

  ck_assert(Color_Parse("ff8000", &color));
  ck_assert_float_eq_tol(color.r, 1.f, 1.f / 255.f);
  ck_assert_float_eq_tol(color.g, 128.f / 255.f, 1.f / 255.f);
  ck_assert_float_eq(color.b, 0.f);
  ck_assert_float_eq(color.a, 1.f);
} END_TEST

START_TEST(check_Color_Parse_rgba) {
  Color color;

  ck_assert(Color_Parse("ff800000", &color));
  ck_assert_float_eq(color.a, 0.f);
} END_TEST

START_TEST(check_Color_Unparse) {
  ck_assert_str_eq(Color_Unparse(color_white), "ffffffff");
  ck_assert_str_eq(Color_Unparse(Color3f(1.f, 0.f, 0.f)), "ff0000ff");
} END_TEST

int32_t main(int32_t argc, char **argv) {

  Suite *suite = suite_create("color");
  TCase *tcase;

  tcase = tcase_create("color");
  tcase_add_test(tcase, check_Color_Parse_default);
  tcase_add_test(tcase, check_Color_Parse_rgb);
  tcase_add_test(tcase, check_Color_Parse_rgba);
  tcase_add_test(tcase, check_Color_Unparse);
  suite_add_tcase(suite, tcase);

  tcase = tcase_create("color32");
  suite_add_tcase(suite, tcase);

  SRunner *runner = srunner_create(suite);

  srunner_run_all(runner, CK_VERBOSE);
  const int32_t failed = srunner_ntests_failed(runner);

  srunner_free(runner);
  return failed;
}

