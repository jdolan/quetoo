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

/**
 * @brief Asserts that the given color encodes and decodes to approximately the same color.
 */
static void check_color_rgbe(const vec3_t in) {
	#define PRINT(in, out, index) "(%g %g %g) !~= (%g %g %g) (%d epsilon %g)", \
		in.x, in.y, in.z, out.r, out.g, out.b, index, epsilon

	const color_t out = Color32_RGBE(Color_RGBE(in));

	const float range = Maxf(1.f, Vec3_Hmaxf(in) - Vec3_Hminf(in));
	const float epsilon = Minf(1.f, .33f * range);

	ck_assert_msg(EqualEpsilonf(in.x, out.r, epsilon), PRINT(in, out, 0));
	ck_assert_msg(EqualEpsilonf(in.y, out.g, epsilon), PRINT(in, out, 1));
	ck_assert_msg(EqualEpsilonf(in.z, out.b, epsilon), PRINT(in, out, 2));
}

START_TEST(_Color_RGBE) {

	check_color_rgbe(Vec3(  1.f,   0.f,   0.f));
	check_color_rgbe(Vec3(  2.f,   0.f,   0.f));
	check_color_rgbe(Vec3(  2.f,   2.f,   2.f));
	check_color_rgbe(Vec3(100.f, 200.f, 300.f));
	check_color_rgbe(Vec3(   .1f,   .2f,   .3f));

	for (int32_t i = 0; i < 1000; i++) {
		check_color_rgbe(Vec3_RandomRange(0.f, 100.f));
	}

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

