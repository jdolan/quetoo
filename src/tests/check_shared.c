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
#include <unistd.h>

#include "shared.h"

START_TEST(check_TriangleArea) {
	vec3_t a, b, c;

	VectorSet(a, 0, 0, 0);
	VectorSet(b, 0, 1, 0);
	VectorSet(c, 1, 1, 0);

	const vec_t area = TriangleArea(a, b, c);
	ck_assert(area == 0.5);

} END_TEST

START_TEST(check_BarycentricCoordinates) {
	vec3_t a, b, c, p, out;

	VectorSet(a, 0, 0, 0);
	VectorSet(b, 0, 1, 0);
	VectorSet(c, 1, 1, 0);

	VectorSet(p, 0, 0, 0);

	BarycentricCoordinates(a, b, c, p, out);
//	puts(vtos(out));
	ck_assert(out[0] == 1);
	ck_assert(out[1] == 0);
	ck_assert(out[2] == 0);

	VectorSet(p, 0, 1, 0);

	BarycentricCoordinates(a, b, c, p, out);
//	puts(vtos(out));
	ck_assert(out[0] == 0);
	ck_assert(out[1] == 1);
	ck_assert(out[2] == 0);

	VectorSet(p, 1, 1, 0);

	BarycentricCoordinates(a, b, c, p, out);
//	puts(vtos(out));
	ck_assert(out[0] == 0);
	ck_assert(out[1] == 0);
	ck_assert(out[2] == 1);

	VectorSet(p, 0.5, 0.5, 0);

	BarycentricCoordinates(a, b, c, p, out);
//	puts(vtos(out));
	ck_assert(out[0] == 0.5);
	ck_assert(out[1] == 0);
	ck_assert(out[2] == 0.5);

} END_TEST

/**
 * @brief Test entry point.
 */
int32_t main(int32_t argc, char **argv) {

	TCase *tcase = tcase_create("check_shared");

	tcase_add_test(tcase, check_TriangleArea);
	tcase_add_test(tcase, check_BarycentricCoordinates);

	Suite *suite = suite_create("check_shared");
	suite_add_tcase(suite, tcase);

	SRunner *runner = srunner_create(suite);

	srunner_run_all(runner, CK_NORMAL);
	int32_t failed = srunner_ntests_failed(runner);

	srunner_free(runner);
	return failed;
}
