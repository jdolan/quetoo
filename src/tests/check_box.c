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

#include "shared/box.h"

START_TEST(_Box3_Equal) {
	ck_assert(Box3_Equal(Box3_Zero(), Box3_Zero()));
	ck_assert(!Box3_Equal(Box3_Zero(), Box3_Null()));
	ck_assert(!Box3_Equal(Box3_Zero(), Box3fv(Vec3_One())));
	ck_assert(Box3_Equal(Box3_Null(), Box3_Null()));
	ck_assert(Box3_Equal(Box3_Null(), Box3(Vec3_Mins(), Vec3_Maxs())));

} END_TEST

START_TEST(_Box3_IsNull) {
	ck_assert(Box3_IsNull(Box3_Null()));
	ck_assert(Box3_IsNull(Box3(Vec3_Mins(), Vec3_Maxs())));
	ck_assert(!Box3_IsNull(Box3_Zero()));
	ck_assert(!Box3_IsNull(Box3fv(Vec3_One())));
	ck_assert(Box3_IsNull(Box3_Union(Box3_Null(), Box3_Null())));
	ck_assert(!Box3_IsNull(Box3_Union(Box3_Zero(), Box3_Null())));
} END_TEST

START_TEST(_Box3_Union) {
	ck_assert(Box3_IsNull(Box3_Union(Box3_Null(), Box3_Null())));
	ck_assert(!Box3_IsNull(Box3_Union(Box3_Zero(), Box3_Null())));

	box3_t a = Box3(Vec3(-1.f, -1.f, -1.f), Vec3(0.f, 0.f, 0.f));
	box3_t b = Box3(Vec3( 0.f,  0.f,  0.f), Vec3(1.f, 1.f, 1.f));
	box3_t c = Box3(Vec3(-1.f, -1.f, -1.f), Vec3(1.f, 1.f, 1.f));

	ck_assert(Box3_Equal(Box3_Union(a, b), c));
} END_TEST

START_TEST(_Box3_Intersection) {
	ck_assert(Box3_IsNull(Box3_Intersection(Box3_Null(), Box3_Null())));
	ck_assert(Box3_IsNull(Box3_Intersection(Box3_Zero(), Box3_Null())));

	box3_t a = Box3(Vec3(-1.f, -1.f, -1.f), Vec3(0.f, 0.f, 0.f));
	box3_t b = Box3(Vec3( 0.f,  0.f,  0.f), Vec3(1.f, 1.f, 1.f));
	box3_t c = Box3(Vec3(-1.f, -1.f, -1.f), Vec3(1.f, 1.f, 1.f));

	ck_assert(Box3_Equal(Box3_Zero(), Box3_Intersection(a, b)));
} END_TEST

int32_t main(int32_t argc, char **argv) {

	Suite *suite = suite_create("check_box"); // Get it??
	TCase *tcase;

	tcase = tcase_create("box3");
	tcase_add_test(tcase, _Box3_Equal);
	tcase_add_test(tcase, _Box3_IsNull);
	tcase_add_test(tcase, _Box3_Union);
	tcase_add_test(tcase, _Box3_Intersection);

	suite_add_tcase(suite, tcase);

	SRunner *runner = srunner_create(suite);

	srunner_run_all(runner, CK_VERBOSE);
	const int32_t failed = srunner_ntests_failed(runner);

	srunner_free(runner);
	return failed;
}

