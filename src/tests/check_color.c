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

#include "shared/color.h"

static void assert_flt_eq(float a, float b) {
	ck_assert_msg(fabsf(a - b) <= .01, "%g != %g", a, b);
}

START_TEST(_Color_RGBE) {

	color32_t in;
	color_t out;

	in = Color_RGBE(Vec3(1, 0, 0));
	out = Color32_RGBE(in);

	assert_flt_eq(out.r, 1);
	assert_flt_eq(out.g, 0);
	assert_flt_eq(out.b, 0);

	in = Color_RGBE(Vec3(2, 0, 0));
	out = Color32_RGBE(in);

	assert_flt_eq(out.r, 2);
	assert_flt_eq(out.g, 0);
	assert_flt_eq(out.b, 0);

	in = Color_RGBE(Vec3(2, 2, 3));
	out = Color32_RGBE(in);

	assert_flt_eq(out.r, 2);
	assert_flt_eq(out.g, 2);
	assert_flt_eq(out.b, 3);

	in = Color_RGBE(Vec3(.1, .2, .3));
	out = Color32_RGBE(in);

	assert_flt_eq(out.r, .1);
	assert_flt_eq(out.g, .2);
	assert_flt_eq(out.b, .3);

} END_TEST

int32_t main(int32_t argc, char **argv) {

	Suite *suite = suite_create("color");
	TCase *tcase;

	tcase = tcase_create("color");
	tcase_add_test(tcase, _Color_RGBE);

	suite_add_tcase(suite, tcase);

	tcase = tcase_create("color32");

	suite_add_tcase(suite, tcase);

	SRunner *runner = srunner_create(suite);

	srunner_run_all(runner, CK_VERBOSE);
	const int32_t failed = srunner_ntests_failed(runner);

	srunner_free(runner);
	return failed;
}

