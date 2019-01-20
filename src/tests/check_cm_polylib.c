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

#include "tests.h"
#include "collision/cm_polylib.h"

quetoo_t quetoo;

/**
 * @brief Setup fixture.
 */
void setup(void) {
	Mem_Init();
}

/**
 * @brief Teardown fixture.
 */
void teardown(void) {
	Mem_Shutdown();
}

static void assert_point_equal(const vec_t *point, vec_t x, vec_t y, vec_t z) {

	if (fabsf(point[0] - x) < ON_EPSILON &&
		fabsf(point[1] - y) < ON_EPSILON &&
		fabsf(point[2] - z) < ON_EPSILON) {
		return;
	}

	ck_assert_msg("%s !== to %g %g %g", vtos(point), x, y, z);
}

START_TEST(Cm_TrianglesForWinding_triangle) {

	cm_winding_t *w = Cm_AllocWinding(3);
	w->num_points = 3;

	VectorSet(w->points[0], 0, 0, 0);
	VectorSet(w->points[1], 1, 0, 0);
	VectorSet(w->points[2], 0, 1, 0);

	vec_t *tris;
	const int32_t num_tris = Cm_TrianglesForWinding(w, &tris);

	ck_assert_int_eq(1, num_tris);

	assert_point_equal(tris + 0, 0, 0, 0);
	assert_point_equal(tris + 3, 1, 0, 0);
	assert_point_equal(tris + 6, 0, 1, 0);

	Mem_Free(tris);

} END_TEST

START_TEST(Cm_TrianglesForWinding_quad) {

	cm_winding_t *w = Cm_AllocWinding(4);
	w->num_points = 4;

	VectorSet(w->points[0], 0, 0, 0);
	VectorSet(w->points[1], 1, 0, 0);
	VectorSet(w->points[2], 1, 1, 0);
	VectorSet(w->points[3], 0, 1, 0);

	vec_t *tris;
	const int32_t num_tris = Cm_TrianglesForWinding(w, &tris);

	ck_assert_int_eq(2, num_tris);

	assert_point_equal(tris + 0, 0, 0, 0);
	assert_point_equal(tris + 3, 1, 0, 0);
	assert_point_equal(tris + 6, 1, 1, 0);

	assert_point_equal(tris +  9, 0, 0, 0);
	assert_point_equal(tris + 12, 1, 1, 0);
	assert_point_equal(tris + 15, 0, 1, 0);

	Mem_Free(tris);

} END_TEST

START_TEST(Cm_TrianglesForWinding_colinear) {

	cm_winding_t *w = Cm_AllocWinding(6);
	w->num_points = 6;

	VectorSet(w->points[0], 0, 0, 0);
	VectorSet(w->points[1], 1, 0, 0);
	VectorSet(w->points[2], 2, 0, 0);

	VectorSet(w->points[3], 2, 2, 0);
	VectorSet(w->points[4], 1, 2, 0);
	VectorSet(w->points[5], 0, 2, 0);

	vec_t *tris;
	const int32_t num_tris = Cm_TrianglesForWinding(w, &tris);

	ck_assert_int_eq(4, num_tris);

	assert_point_equal(tris + 0, 1, 0, 0);
	assert_point_equal(tris + 3, 2, 0, 0);
	assert_point_equal(tris + 6, 2, 2, 0);

	assert_point_equal(tris +  9, 0, 0, 0);
	assert_point_equal(tris + 12, 1, 0, 0);
	assert_point_equal(tris + 15, 2, 2, 0);

	assert_point_equal(tris + 18, 0, 0, 0);
	assert_point_equal(tris + 21, 2, 2, 0);
	assert_point_equal(tris + 24, 1, 2, 0);

	assert_point_equal(tris + 27, 0, 0, 0);
	assert_point_equal(tris + 30, 1, 2, 0);
	assert_point_equal(tris + 33, 0, 2, 0);

//	const vec_t *v = tris;
//	for (int32_t i = 0; i < num_tris; i++, v += 9) {
//		printf("%s %s %s\n", vtos(v), vtos(v + 3), vtos(v + 6));
//	}

	Mem_Free(tris);

} END_TEST

/**
 * @brief Test entry point.
 */
int32_t main(int32_t argc, char **argv) {

	Test_Init(argc, argv);

	TCase *tcase = tcase_create("check_cm_polylib");
	tcase_add_checked_fixture(tcase, setup, teardown);

	tcase_add_test(tcase, Cm_TrianglesForWinding_triangle);
	tcase_add_test(tcase, Cm_TrianglesForWinding_quad);
	tcase_add_test(tcase, Cm_TrianglesForWinding_colinear);

	Suite *suite = suite_create("check_cm_polylib");
	suite_add_tcase(suite, tcase);

	int32_t failed = Test_Run(suite);

	Test_Shutdown();
	return failed;
}
